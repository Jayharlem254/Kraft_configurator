#include "db.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>

namespace kraft {

#define KRAFT_LOCK std::lock_guard<std::recursive_mutex> _kraft_lock(mutex_)

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

std::string current_iso_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &tt);
#else
    gmtime_r(&tt, &gm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gm);
    return std::string(buf);
}

static std::mt19937_64& rng() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    return gen;
}

std::string generate_uuid() {
    std::uniform_int_distribution<uint64_t> dis;
    uint64_t p1 = dis(rng());
    uint64_t p2 = dis(rng());
    p1 = (p1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    p2 = (p2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<uint32_t>(p1 >> 32), static_cast<uint16_t>((p1 >> 16) & 0xFFFF),
                  static_cast<uint16_t>(p1 & 0xFFFF), static_cast<uint16_t>(p2 >> 48),
                  static_cast<unsigned long long>(p2 & 0xFFFFFFFFFFFFULL));
    return std::string(buf);
}

std::string slugify(const std::string& s) {
    std::string out;
    bool dash = false;
    for (unsigned char c : s) {
        if (std::isalnum(c)) {
            out += static_cast<char>(std::tolower(c));
            dash = false;
        } else if (!out.empty() && !dash) {
            out += '-';
            dash = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

std::string join_csv(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) out += ',';
        out += v[i];
    }
    return out;
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (c != ' ') {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static json parse_json_or(const std::string& text, json fallback) {
    if (text.empty()) return fallback;
    try {
        return json::parse(text);
    } catch (...) {
        return fallback;
    }
}

// ---------------------------------------------------------------------------
// Stmt
// ---------------------------------------------------------------------------

Stmt::Stmt(sqlite3* db, const char* sql) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
        std::cerr << "[SQLite] prepare failed: " << sqlite3_errmsg(db) << "\n  SQL: " << sql << "\n";
        stmt_ = nullptr;
    }
}
Stmt::~Stmt() {
    if (stmt_) sqlite3_finalize(stmt_);
}
Stmt& Stmt::bind(int idx, const std::string& v) {
    if (stmt_) sqlite3_bind_text(stmt_, idx, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT);
    return *this;
}
Stmt& Stmt::bind(int idx, int v) {
    if (stmt_) sqlite3_bind_int(stmt_, idx, v);
    return *this;
}
Stmt& Stmt::bind(int idx, double v) {
    if (stmt_) sqlite3_bind_double(stmt_, idx, v);
    return *this;
}
Stmt& Stmt::bind_null(int idx) {
    if (stmt_) sqlite3_bind_null(stmt_, idx);
    return *this;
}
Stmt& Stmt::bind_opt(int idx, const std::optional<std::string>& v) {
    return (v && !v->empty()) ? bind(idx, *v) : bind_null(idx);
}
bool Stmt::row() { return stmt_ && sqlite3_step(stmt_) == SQLITE_ROW; }
bool Stmt::exec() {
    if (!stmt_) return false;
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_DONE || rc == SQLITE_ROW;
}
int Stmt::col_int(int i) const { return sqlite3_column_int(stmt_, i); }
double Stmt::col_double(int i) const { return sqlite3_column_double(stmt_, i); }
std::string Stmt::col_text(int i) const {
    const unsigned char* t = sqlite3_column_text(stmt_, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}
std::optional<std::string> Stmt::col_opt(int i) const {
    if (is_null(i)) return std::nullopt;
    return col_text(i);
}
bool Stmt::is_null(int i) const { return sqlite3_column_type(stmt_, i) == SQLITE_NULL; }

// ---------------------------------------------------------------------------
// Database lifecycle & schema
// ---------------------------------------------------------------------------

Database::Database(std::string db_path) : db_path_(std::move(db_path)) {}
Database::~Database() { close(); }

bool Database::open() {
    KRAFT_LOCK;
    if (db_) return true;
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Failed to open SQLite database: " << sqlite3_errmsg(db_) << "\n";
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");
    execute("PRAGMA synchronous = NORMAL;");
    execute("PRAGMA temp_store = MEMORY;");
    execute("PRAGMA cache_size = -16000;");  // 16 MB page cache
    sqlite3_busy_timeout(db_, 3000);
    return init_schema() && migrate();
}

void Database::close() {
    KRAFT_LOCK;
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[SQLite] " << (err ? err : "unknown error") << "\n";
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::transaction(const std::function<bool()>& body) {
    KRAFT_LOCK;
    if (!execute("BEGIN IMMEDIATE;")) return false;
    bool ok = false;
    try {
        ok = body();
    } catch (const std::exception& e) {
        std::cerr << "[SQLite] transaction aborted: " << e.what() << "\n";
        ok = false;
    }
    execute(ok ? "COMMIT;" : "ROLLBACK;");
    return ok;
}

bool Database::init_schema() {
    return execute(R"(
        CREATE TABLE IF NOT EXISTS vehicles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            slug TEXT NOT NULL UNIQUE,
            vehicle_type TEXT NOT NULL,
            tagline TEXT DEFAULT '',
            description TEXT DEFAULT '',
            base_price REAL NOT NULL DEFAULT 0.0,
            model_file TEXT DEFAULT NULL,
            thumbnail TEXT DEFAULT NULL,
            is_published INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS variants (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
            code TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT DEFAULT '',
            price REAL NOT NULL DEFAULT 0.0,
            specs TEXT DEFAULT '{}',
            is_default INTEGER NOT NULL DEFAULT 0,
            display_order INTEGER NOT NULL DEFAULT 0,
            UNIQUE(vehicle_id, code)
        );
        CREATE TABLE IF NOT EXISTS option_categories (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            slug TEXT NOT NULL,
            selection_type TEXT NOT NULL DEFAULT 'single',
            display_order INTEGER NOT NULL DEFAULT 0,
            UNIQUE(vehicle_id, slug)
        );
        CREATE TABLE IF NOT EXISTS option_choices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            category_id INTEGER NOT NULL REFERENCES option_categories(id) ON DELETE CASCADE,
            name TEXT NOT NULL,
            price_delta REAL NOT NULL DEFAULT 0.0,
            swatch_hex TEXT DEFAULT '',
            thumbnail TEXT DEFAULT NULL,
            mesh_target TEXT DEFAULT '',
            is_default INTEGER NOT NULL DEFAULT 0,
            display_order INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS configurations (
            id TEXT PRIMARY KEY,
            vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
            name TEXT DEFAULT '',
            session_key TEXT DEFAULT '',
            created_at TEXT NOT NULL,
            updated_at TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS configuration_selections (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            configuration_id TEXT NOT NULL REFERENCES configurations(id) ON DELETE CASCADE,
            option_id INTEGER NOT NULL REFERENCES option_choices(id) ON DELETE CASCADE,
            UNIQUE(configuration_id, option_id)
        );
        CREATE TABLE IF NOT EXISTS meta (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
}

bool Database::has_column(const std::string& table, const std::string& column) {
    Stmt s(db_, ("PRAGMA table_info(" + table + ")").c_str());
    while (s.row()) {
        if (s.col_text(1) == column) return true;
    }
    return false;
}

void Database::ensure_column(const std::string& table, const std::string& column, const std::string& decl) {
    if (!has_column(table, column)) {
        execute("ALTER TABLE " + table + " ADD COLUMN " + column + " " + decl + ";");
    }
}

// Additive, idempotent migrations — existing databases keep all their rows.
bool Database::migrate() {
    ensure_column("vehicles", "body_class", "TEXT DEFAULT ''");
    ensure_column("vehicles", "family", "TEXT DEFAULT ''");
    ensure_column("vehicles", "year", "INTEGER DEFAULT 0");
    ensure_column("vehicles", "shape", "TEXT DEFAULT '{}'");
    ensure_column("vehicles", "material_map", "TEXT DEFAULT '{}'");
    ensure_column("vehicles", "source", "TEXT DEFAULT 'upload'");
    ensure_column("vehicles", "display_order", "INTEGER DEFAULT 0");

    ensure_column("option_categories", "section", "TEXT DEFAULT 'exterior'");
    ensure_column("option_categories", "display", "TEXT DEFAULT 'list'");
    ensure_column("option_categories", "description", "TEXT DEFAULT ''");

    ensure_column("option_choices", "code", "TEXT DEFAULT ''");
    ensure_column("option_choices", "description", "TEXT DEFAULT ''");
    ensure_column("option_choices", "finish", "TEXT DEFAULT ''");
    ensure_column("option_choices", "visual", "TEXT DEFAULT '[]'");
    ensure_column("option_choices", "mods", "TEXT DEFAULT '{}'");
    ensure_column("option_choices", "requires_codes", "TEXT DEFAULT ''");
    ensure_column("option_choices", "excludes_codes", "TEXT DEFAULT ''");
    ensure_column("option_choices", "includes_codes", "TEXT DEFAULT ''");
    ensure_column("option_choices", "variants", "TEXT DEFAULT ''");
    ensure_column("option_choices", "tag", "TEXT DEFAULT ''");

    ensure_column("configurations", "variant_id", "INTEGER DEFAULT 0");
    ensure_column("configurations", "code", "TEXT DEFAULT ''");
    ensure_column("configurations", "total_price", "REAL DEFAULT 0");
    ensure_column("configurations", "snapshot", "TEXT DEFAULT '{}'");

    // Legacy choices had no codes; give them stable ones so rules can address them.
    execute("UPDATE option_choices SET code = 'OPT' || id WHERE code IS NULL OR code = '';");
    // Legacy option categories: infer a section from their slug.
    execute(R"(UPDATE option_categories SET display = 'swatch'
               WHERE (slug LIKE '%color%' OR slug LIKE '%finish%' OR slug LIKE '%paint%' OR slug LIKE '%hull%')
                 AND display = 'list';)");

    return execute(R"(
        CREATE INDEX IF NOT EXISTS idx_vehicles_slug ON vehicles(slug);
        CREATE INDEX IF NOT EXISTS idx_vehicles_type ON vehicles(vehicle_type, body_class);
        CREATE INDEX IF NOT EXISTS idx_variants_vehicle ON variants(vehicle_id);
        CREATE INDEX IF NOT EXISTS idx_categories_vehicle ON option_categories(vehicle_id);
        CREATE INDEX IF NOT EXISTS idx_choices_category ON option_choices(category_id);
        CREATE INDEX IF NOT EXISTS idx_config_session ON configurations(session_key);
        CREATE INDEX IF NOT EXISTS idx_config_code ON configurations(code);
        CREATE INDEX IF NOT EXISTS idx_selections_config ON configuration_selections(configuration_id);
    )");
}

// ---------------------------------------------------------------------------
// Meta
// ---------------------------------------------------------------------------

std::string Database::get_meta(const std::string& key, const std::string& fallback) {
    KRAFT_LOCK;
    Stmt s(db_, "SELECT value FROM meta WHERE key = ?");
    s.bind(1, key);
    return s.row() ? s.col_text(0) : fallback;
}

void Database::set_meta(const std::string& key, const std::string& value) {
    KRAFT_LOCK;
    Stmt s(db_, "INSERT INTO meta(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    s.bind(1, key).bind(2, value).exec();
}

// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

static const char* VEHICLE_COLS =
    "id, name, slug, vehicle_type, tagline, description, base_price, model_file, thumbnail, is_published, "
    "created_at, updated_at, body_class, family, year, shape, material_map, source, display_order";

static Vehicle read_vehicle(const Stmt& s) {
    Vehicle v;
    v.id = s.col_int(0);
    v.name = s.col_text(1);
    v.slug = s.col_text(2);
    v.vehicle_type = s.col_text(3);
    v.tagline = s.col_text(4);
    v.description = s.col_text(5);
    v.base_price = s.col_double(6);
    v.model_file = s.col_opt(7);
    if (v.model_file && v.model_file->empty()) v.model_file.reset();
    v.thumbnail = s.col_opt(8);
    v.is_published = s.col_int(9) != 0;
    v.created_at = s.col_text(10);
    v.updated_at = s.col_text(11);
    v.body_class = s.col_text(12);
    v.family = s.col_text(13);
    v.year = s.col_int(14);
    v.shape = parse_json_or(s.col_text(15), json::object());
    v.material_map = parse_json_or(s.col_text(16), json::object());
    v.source = s.col_text(17);
    v.display_order = s.col_int(18);
    return v;
}

std::vector<Vehicle> Database::get_vehicles(const std::string& vehicle_type, bool published_only) {
    KRAFT_LOCK;
    std::string sql = std::string("SELECT ") + VEHICLE_COLS + " FROM vehicles WHERE 1=1";
    if (published_only) sql += " AND is_published = 1";
    if (!vehicle_type.empty()) sql += " AND vehicle_type = ?";
    sql += " ORDER BY vehicle_type ASC, display_order ASC, name ASC";
    Stmt s(db_, sql.c_str());
    if (!vehicle_type.empty()) s.bind(1, vehicle_type);
    std::vector<Vehicle> out;
    while (s.row()) out.push_back(read_vehicle(s));
    return out;
}

std::optional<Vehicle> Database::get_vehicle_by_id(int id) {
    KRAFT_LOCK;
    Stmt s(db_, (std::string("SELECT ") + VEHICLE_COLS + " FROM vehicles WHERE id = ?").c_str());
    s.bind(1, id);
    if (!s.row()) return std::nullopt;
    return read_vehicle(s);
}

std::optional<Vehicle> Database::get_vehicle_by_slug(const std::string& slug) {
    KRAFT_LOCK;
    Stmt s(db_, (std::string("SELECT ") + VEHICLE_COLS + " FROM vehicles WHERE slug = ?").c_str());
    s.bind(1, slug);
    if (!s.row()) return std::nullopt;
    return read_vehicle(s);
}

int Database::count_vehicles() {
    KRAFT_LOCK;
    Stmt s(db_, "SELECT COUNT(*) FROM vehicles");
    return s.row() ? s.col_int(0) : 0;
}

std::vector<Variant> Database::get_variants(int vehicle_id) {
    KRAFT_LOCK;
    Stmt s(db_,
           "SELECT id, vehicle_id, code, name, description, price, specs, is_default, display_order "
           "FROM variants WHERE vehicle_id = ? ORDER BY display_order ASC, price ASC");
    s.bind(1, vehicle_id);
    std::vector<Variant> out;
    while (s.row()) {
        Variant v;
        v.id = s.col_int(0);
        v.vehicle_id = s.col_int(1);
        v.code = s.col_text(2);
        v.name = s.col_text(3);
        v.description = s.col_text(4);
        v.price = s.col_double(5);
        v.specs = parse_json_or(s.col_text(6), json::object());
        v.is_default = s.col_int(7) != 0;
        v.display_order = s.col_int(8);
        out.push_back(std::move(v));
    }
    return out;
}

static OptionChoice read_choice(const Stmt& s) {
    OptionChoice c;
    c.id = s.col_int(0);
    c.category_id = s.col_int(1);
    c.name = s.col_text(2);
    c.price_delta = s.col_double(3);
    c.swatch_hex = s.col_text(4);
    c.thumbnail = s.col_opt(5);
    c.mesh_target = s.col_text(6);
    c.is_default = s.col_int(7) != 0;
    c.display_order = s.col_int(8);
    c.code = s.col_text(9);
    c.description = s.col_text(10);
    c.finish = s.col_text(11);
    c.visual = parse_json_or(s.col_text(12), json::array());
    if (!c.visual.is_array()) c.visual = json::array({c.visual});
    c.mods = parse_json_or(s.col_text(13), json::object());
    c.requires_codes = split_csv(s.col_text(14));
    c.excludes_codes = split_csv(s.col_text(15));
    c.includes_codes = split_csv(s.col_text(16));
    c.variants = split_csv(s.col_text(17));
    c.tag = s.col_text(18);

    // Legacy rows: derive a visual binding from swatch + mesh_target.
    if (c.visual.empty() && !c.swatch_hex.empty()) {
        std::string role = "paint";
        std::string mt = c.mesh_target;
        std::transform(mt.begin(), mt.end(), mt.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (mt.find("wheel") != std::string::npos || mt.find("rim") != std::string::npos) role = "rims";
        else if (mt.find("interior") != std::string::npos || mt.find("seat") != std::string::npos) role = "interior";
        else if (mt.find("aero") != std::string::npos || mt.find("trim") != std::string::npos) role = "trim";
        else if (mt.find("hull") != std::string::npos) role = "hull";
        else if (mt.find("deck") != std::string::npos) role = "deck";
        json v = {{"role", role}, {"color", c.swatch_hex}};
        if (!c.finish.empty()) v["finish"] = c.finish;
        if (!c.mesh_target.empty()) v["target"] = c.mesh_target;
        c.visual.push_back(v);
    }
    return c;
}

static const char* CHOICE_COLS =
    "id, category_id, name, price_delta, swatch_hex, thumbnail, mesh_target, is_default, display_order, "
    "code, description, finish, visual, mods, requires_codes, excludes_codes, includes_codes, variants, tag";

std::vector<OptionCategory> Database::get_categories(int vehicle_id) {
    KRAFT_LOCK;
    std::vector<OptionCategory> cats;
    {
        Stmt s(db_,
               "SELECT id, vehicle_id, name, slug, selection_type, display_order, section, display, description "
               "FROM option_categories WHERE vehicle_id = ? ORDER BY display_order ASC, id ASC");
        s.bind(1, vehicle_id);
        while (s.row()) {
            OptionCategory c;
            c.id = s.col_int(0);
            c.vehicle_id = s.col_int(1);
            c.name = s.col_text(2);
            c.slug = s.col_text(3);
            c.selection_type = s.col_text(4);
            c.display_order = s.col_int(5);
            c.section = s.col_text(6);
            c.display = s.col_text(7);
            c.description = s.col_text(8);
            if (c.section.empty()) c.section = "exterior";
            if (c.display.empty()) c.display = "list";
            cats.push_back(std::move(c));
        }
    }
    if (cats.empty()) return cats;

    // One query for every choice of the vehicle instead of one per category.
    Stmt s(db_, (std::string("SELECT ") + CHOICE_COLS +
                 " FROM option_choices WHERE category_id IN (SELECT id FROM option_categories WHERE vehicle_id = ?)"
                 " ORDER BY display_order ASC, id ASC").c_str());
    s.bind(1, vehicle_id);
    while (s.row()) {
        OptionChoice c = read_choice(s);
        for (auto& cat : cats) {
            if (cat.id == c.category_id) {
                cat.choices.push_back(std::move(c));
                break;
            }
        }
    }
    return cats;
}

std::optional<Vehicle> Database::insert_vehicle(const Vehicle& v) {
    KRAFT_LOCK;
    std::string now = current_iso_time();
    Stmt s(db_,
           "INSERT INTO vehicles (name, slug, vehicle_type, tagline, description, base_price, model_file, thumbnail, "
           "is_published, created_at, updated_at, body_class, family, year, shape, material_map, source, display_order) "
           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    s.bind(1, v.name).bind(2, v.slug).bind(3, v.vehicle_type).bind(4, v.tagline).bind(5, v.description)
        .bind(6, v.base_price).bind_opt(7, v.model_file).bind_opt(8, v.thumbnail).bind(9, v.is_published ? 1 : 0)
        .bind(10, now).bind(11, now).bind(12, v.body_class).bind(13, v.family).bind(14, v.year)
        .bind(15, v.shape.dump()).bind(16, v.material_map.dump()).bind(17, v.source).bind(18, v.display_order);
    if (!s.exec()) return std::nullopt;
    return get_vehicle_by_id(static_cast<int>(sqlite3_last_insert_rowid(db_)));
}

bool Database::update_vehicle_core(const Vehicle& v) {
    KRAFT_LOCK;
    Stmt s(db_,
           "UPDATE vehicles SET name = ?, vehicle_type = ?, tagline = ?, description = ?, base_price = ?, "
           "model_file = ?, thumbnail = ?, is_published = ?, updated_at = ?, body_class = ?, family = ?, year = ?, "
           "shape = ?, material_map = ?, source = ?, display_order = ? WHERE id = ?");
    s.bind(1, v.name).bind(2, v.vehicle_type).bind(3, v.tagline).bind(4, v.description).bind(5, v.base_price)
        .bind_opt(6, v.model_file).bind_opt(7, v.thumbnail).bind(8, v.is_published ? 1 : 0).bind(9, current_iso_time())
        .bind(10, v.body_class).bind(11, v.family).bind(12, v.year).bind(13, v.shape.dump())
        .bind(14, v.material_map.dump()).bind(15, v.source).bind(16, v.display_order).bind(17, v.id);
    return s.exec();
}

bool Database::update_vehicle_model(int id, const std::string& model_file) {
    KRAFT_LOCK;
    Stmt s(db_, "UPDATE vehicles SET model_file = ?, updated_at = ? WHERE id = ?");
    if (model_file.empty()) s.bind_null(1); else s.bind(1, model_file);
    s.bind(2, current_iso_time()).bind(3, id);
    return s.exec();
}

bool Database::update_vehicle_model_by_slug(const std::string& slug, const std::string& model_file) {
    KRAFT_LOCK;
    Stmt s(db_, "UPDATE vehicles SET model_file = ?, updated_at = ? WHERE slug = ?");
    s.bind(1, model_file).bind(2, current_iso_time()).bind(3, slug);
    return s.exec();
}

bool Database::clear_vehicle_model_by_file(const std::string& filename) {
    KRAFT_LOCK;
    Stmt s(db_, "UPDATE vehicles SET model_file = NULL, updated_at = ? WHERE model_file = ? OR model_file LIKE ?");
    s.bind(1, current_iso_time()).bind(2, filename).bind(3, "%/" + filename);
    return s.exec();
}

bool Database::delete_vehicle(int id) {
    KRAFT_LOCK;
    Stmt s(db_, "DELETE FROM vehicles WHERE id = ?");
    s.bind(1, id);
    return s.exec() && sqlite3_changes(db_) > 0;
}

int Database::insert_variant(int vehicle_id, const Variant& v) {
    Stmt s(db_,
           "INSERT INTO variants (vehicle_id, code, name, description, price, specs, is_default, display_order) "
           "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    s.bind(1, vehicle_id).bind(2, v.code).bind(3, v.name).bind(4, v.description).bind(5, v.price)
        .bind(6, v.specs.dump()).bind(7, v.is_default ? 1 : 0).bind(8, v.display_order);
    return s.exec() ? static_cast<int>(sqlite3_last_insert_rowid(db_)) : 0;
}

bool Database::add_variant(int vehicle_id, const Variant& v) {
    KRAFT_LOCK;
    return insert_variant(vehicle_id, v) > 0;
}

int Database::insert_category(int vehicle_id, const OptionCategory& c) {
    Stmt s(db_,
           "INSERT INTO option_categories (vehicle_id, name, slug, selection_type, display_order, section, display, description) "
           "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    s.bind(1, vehicle_id).bind(2, c.name).bind(3, c.slug).bind(4, c.selection_type).bind(5, c.display_order)
        .bind(6, c.section).bind(7, c.display).bind(8, c.description);
    return s.exec() ? static_cast<int>(sqlite3_last_insert_rowid(db_)) : 0;
}

int Database::insert_choice(int category_id, const OptionChoice& c) {
    Stmt s(db_,
           "INSERT INTO option_choices (category_id, name, price_delta, swatch_hex, thumbnail, mesh_target, is_default, "
           "display_order, code, description, finish, visual, mods, requires_codes, excludes_codes, includes_codes, variants, tag) "
           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    s.bind(1, category_id).bind(2, c.name).bind(3, c.price_delta).bind(4, c.swatch_hex).bind_opt(5, c.thumbnail)
        .bind(6, c.mesh_target).bind(7, c.is_default ? 1 : 0).bind(8, c.display_order).bind(9, c.code)
        .bind(10, c.description).bind(11, c.finish).bind(12, c.visual.dump()).bind(13, c.mods.dump())
        .bind(14, join_csv(c.requires_codes)).bind(15, join_csv(c.excludes_codes)).bind(16, join_csv(c.includes_codes))
        .bind(17, join_csv(c.variants)).bind(18, c.tag);
    return s.exec() ? static_cast<int>(sqlite3_last_insert_rowid(db_)) : 0;
}

bool Database::replace_vehicle_content(int vehicle_id, const std::vector<Variant>& variants,
                                       const std::vector<OptionCategory>& categories) {
    return transaction([&]() {
        {
            Stmt d1(db_, "DELETE FROM variants WHERE vehicle_id = ?");
            d1.bind(1, vehicle_id).exec();
            Stmt d2(db_, "DELETE FROM option_categories WHERE vehicle_id = ?");
            d2.bind(1, vehicle_id).exec();
        }
        int order = 0;
        for (const auto& v : variants) {
            Variant copy = v;
            if (copy.display_order == 0) copy.display_order = ++order;
            if (!insert_variant(vehicle_id, copy)) return false;
        }
        int cat_order = 0;
        for (const auto& c : categories) {
            OptionCategory copy = c;
            if (copy.display_order == 0) copy.display_order = ++cat_order;
            int cat_id = insert_category(vehicle_id, copy);
            if (!cat_id) return false;
            int choice_order = 0;
            for (const auto& ch : c.choices) {
                OptionChoice cc = ch;
                if (cc.display_order == 0) cc.display_order = ++choice_order;
                if (!insert_choice(cat_id, cc)) return false;
            }
        }
        return true;
    });
}

int Database::upsert_blueprint(const VehicleBlueprint& bp) {
    KRAFT_LOCK;
    auto existing = get_vehicle_by_slug(bp.vehicle.slug);
    int id = 0;
    if (existing) {
        Vehicle v = bp.vehicle;
        v.id = existing->id;
        // Never clobber a model the user attached to a seeded vehicle.
        if (existing->model_file && !v.model_file) v.model_file = existing->model_file;
        if (!update_vehicle_core(v)) return 0;
        id = v.id;
    } else {
        auto created = insert_vehicle(bp.vehicle);
        if (!created) return 0;
        id = created->id;
    }
    return replace_vehicle_content(id, bp.variants, bp.categories) ? id : 0;
}

// ---------------------------------------------------------------------------
// Configurations
// ---------------------------------------------------------------------------

std::string Database::unique_config_code() {
    // Crockford-style alphabet: no I, L, O, U to stay readable when typed.
    static const char* alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    std::uniform_int_distribution<int> dis(0, 31);
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::string code = "K";
        for (int i = 0; i < 7; ++i) code += alphabet[dis(rng())];
        Stmt s(db_, "SELECT 1 FROM configurations WHERE code = ?");
        s.bind(1, code);
        if (!s.row()) return code;
    }
    return "K" + generate_uuid().substr(0, 8);
}

void Database::write_selections(const std::string& config_id, const std::vector<int>& option_ids) {
    Stmt d(db_, "DELETE FROM configuration_selections WHERE configuration_id = ?");
    d.bind(1, config_id).exec();
    for (int opt_id : option_ids) {
        Stmt s(db_, "INSERT OR IGNORE INTO configuration_selections (configuration_id, option_id) VALUES (?, ?)");
        s.bind(1, config_id).bind(2, opt_id).exec();
    }
}

std::vector<OptionChoice> Database::load_selected_options(const std::string& config_id) {
    std::string sql = "SELECT ";
    // Prefix choice columns with the table alias.
    std::string cols = CHOICE_COLS;
    std::stringstream ss(cols);
    std::string col;
    bool first = true;
    while (std::getline(ss, col, ',')) {
        col.erase(0, col.find_first_not_of(' '));
        if (!first) sql += ", ";
        sql += "c." + col;
        first = false;
    }
    sql += " FROM option_choices c JOIN configuration_selections s ON s.option_id = c.id "
           "WHERE s.configuration_id = ? ORDER BY c.category_id ASC, c.display_order ASC";
    Stmt s(db_, sql.c_str());
    s.bind(1, config_id);
    std::vector<OptionChoice> out;
    while (s.row()) out.push_back(read_choice(s));
    return out;
}

std::optional<Configuration> Database::get_configuration(const std::string& id_or_code) {
    KRAFT_LOCK;
    Stmt s(db_,
           "SELECT c.id, c.vehicle_id, c.name, c.session_key, c.created_at, c.updated_at, c.variant_id, c.code, "
           "c.total_price, c.snapshot, v.slug, v.name, v.base_price, "
           "COALESCE(var.code, ''), COALESCE(var.name, ''), COALESCE(var.price, -1) "
           "FROM configurations c JOIN vehicles v ON v.id = c.vehicle_id "
           "LEFT JOIN variants var ON var.id = c.variant_id "
           "WHERE c.id = ? OR c.code = ? COLLATE NOCASE");
    s.bind(1, id_or_code).bind(2, id_or_code);
    if (!s.row()) return std::nullopt;

    Configuration cfg;
    cfg.id = s.col_text(0);
    cfg.vehicle_id = s.col_int(1);
    cfg.name = s.col_text(2);
    cfg.session_key = s.col_text(3);
    cfg.created_at = s.col_text(4);
    cfg.updated_at = s.col_text(5);
    cfg.variant_id = s.col_int(6);
    cfg.code = s.col_text(7);
    cfg.total_price = s.col_double(8);
    cfg.snapshot = parse_json_or(s.col_text(9), json::object());
    cfg.vehicle_slug = s.col_text(10);
    cfg.vehicle_name = s.col_text(11);
    double vehicle_base = s.col_double(12);
    cfg.variant_code = s.col_text(13);
    cfg.variant_name = s.col_text(14);
    double variant_price = s.col_double(15);

    cfg.selected_options = load_selected_options(cfg.id);
    if (cfg.total_price <= 0) {
        // Legacy rows were never priced at save time.
        cfg.total_price = variant_price >= 0 ? variant_price : vehicle_base;
        for (const auto& o : cfg.selected_options) cfg.total_price += o.price_delta;
    }
    return cfg;
}

std::vector<Configuration> Database::get_configurations(const std::string& session_key) {
    KRAFT_LOCK;
    std::vector<std::string> ids;
    {
        std::string sql = "SELECT id FROM configurations";
        if (!session_key.empty()) sql += " WHERE session_key = ?";
        sql += " ORDER BY updated_at DESC LIMIT 200";
        Stmt s(db_, sql.c_str());
        if (!session_key.empty()) s.bind(1, session_key);
        while (s.row()) ids.push_back(s.col_text(0));
    }
    std::vector<Configuration> out;
    for (const auto& id : ids) {
        if (auto c = get_configuration(id)) out.push_back(std::move(*c));
    }
    return out;
}

std::optional<Configuration> Database::create_configuration(int vehicle_id, int variant_id, const std::vector<int>& option_ids,
                                                            double total_price, const json& snapshot,
                                                            const std::string& session_key, const std::string& name) {
    KRAFT_LOCK;
    std::string id = generate_uuid();
    bool ok = transaction([&]() {
        std::string now = current_iso_time();
        Stmt s(db_,
               "INSERT INTO configurations (id, vehicle_id, name, session_key, created_at, updated_at, variant_id, code, "
               "total_price, snapshot) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        s.bind(1, id).bind(2, vehicle_id).bind(3, name).bind(4, session_key).bind(5, now).bind(6, now)
            .bind(7, variant_id).bind(8, unique_config_code()).bind(9, total_price).bind(10, snapshot.dump());
        if (!s.exec()) return false;
        write_selections(id, option_ids);
        return true;
    });
    if (!ok) return std::nullopt;
    return get_configuration(id);
}

std::optional<Configuration> Database::update_configuration(const std::string& id, int variant_id, const std::vector<int>& option_ids,
                                                            double total_price, const json& snapshot,
                                                            const std::optional<std::string>& name) {
    KRAFT_LOCK;
    auto existing = get_configuration(id);
    if (!existing) return std::nullopt;
    bool ok = transaction([&]() {
        Stmt s(db_,
               "UPDATE configurations SET updated_at = ?, variant_id = ?, total_price = ?, snapshot = ?, "
               "name = COALESCE(?, name) WHERE id = ?");
        s.bind(1, current_iso_time()).bind(2, variant_id).bind(3, total_price).bind(4, snapshot.dump());
        if (name) s.bind(5, *name); else s.bind_null(5);
        s.bind(6, existing->id);
        if (!s.exec()) return false;
        write_selections(existing->id, option_ids);
        return true;
    });
    if (!ok) return std::nullopt;
    return get_configuration(existing->id);
}

bool Database::delete_configuration(const std::string& id) {
    KRAFT_LOCK;
    Stmt s(db_, "DELETE FROM configurations WHERE id = ? OR code = ? COLLATE NOCASE");
    s.bind(1, id).bind(2, id);
    return s.exec() && sqlite3_changes(db_) > 0;
}

}  // namespace kraft
