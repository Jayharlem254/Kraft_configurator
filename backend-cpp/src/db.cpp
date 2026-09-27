#include "db.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <random>
#include <sstream>
#include <algorithm>

namespace kraft {

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

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t p1 = dis(gen);
    uint64_t p2 = dis(gen);

    // Set UUID version 4
    p1 = (p1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Set variant (RFC 4122)
    p2 = (p2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    char buf[40];
    std::snprintf(buf, sizeof(buf),
                  "%08x-%04x-%04x-%04x-%012llx",
                  static_cast<uint32_t>(p1 >> 32),
                  static_cast<uint16_t>((p1 >> 16) & 0xFFFF),
                  static_cast<uint16_t>(p1 & 0xFFFF),
                  static_cast<uint16_t>(p2 >> 48),
                  static_cast<unsigned long long>(p2 & 0xFFFFFFFFFFFFULL));
    return std::string(buf);
}

Database::Database(const std::string& db_path) : db_path_(db_path) {}

Database::~Database() {
    close();
}

bool Database::open() {
    if (db_) return true;

    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open SQLite database: " << sqlite3_errmsg(db_) << "\n";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    // Enable foreign keys and WAL mode for high performance
    execute("PRAGMA foreign_keys = ON;");
    execute("PRAGMA journal_mode = WAL;");

    return init_schema();
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error executing SQL: " << (err_msg ? err_msg : "unknown") << "\nSQL: " << sql << "\n";
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool Database::init_schema() {
    const std::string schema = R"(
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

        CREATE INDEX IF NOT EXISTS idx_vehicles_slug ON vehicles(slug);
        CREATE INDEX IF NOT EXISTS idx_vehicles_type ON vehicles(vehicle_type);
        CREATE INDEX IF NOT EXISTS idx_config_session ON configurations(session_key);
    )";

    return execute(schema);
}

std::vector<Vehicle> Database::get_vehicles(const std::string& vehicle_type, bool published_only) {
    std::vector<Vehicle> list;
    std::string sql = "SELECT id, name, slug, vehicle_type, tagline, description, base_price, model_file, thumbnail, is_published, created_at, updated_at FROM vehicles WHERE 1=1";
    
    if (published_only) {
        sql += " AND is_published = 1";
    }
    if (!vehicle_type.empty()) {
        sql += " AND vehicle_type = ?";
    }
    sql += " ORDER BY vehicle_type ASC, name ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    if (!vehicle_type.empty()) {
        sqlite3_bind_text(stmt, 1, vehicle_type.c_str(), -1, SQLITE_STATIC);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Vehicle v;
        v.id = sqlite3_column_int(stmt, 0);
        v.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        v.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        v.vehicle_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        v.tagline = tag ? tag : "";
        const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        v.description = desc ? desc : "";
        v.base_price = sqlite3_column_double(stmt, 6);
        
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            v.model_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        }
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
            v.thumbnail = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        }
        v.is_published = sqlite3_column_int(stmt, 9) != 0;
        v.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        v.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));

        list.push_back(v);
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<OptionCategory> Database::load_categories_for_vehicle(int vehicle_id) {
    std::vector<OptionCategory> categories;

    std::string sql = "SELECT id, vehicle_id, name, slug, selection_type, display_order FROM option_categories WHERE vehicle_id = ? ORDER BY display_order ASC, name ASC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return categories;
    }
    sqlite3_bind_int(stmt, 1, vehicle_id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OptionCategory cat;
        cat.id = sqlite3_column_int(stmt, 0);
        cat.vehicle_id = sqlite3_column_int(stmt, 1);
        cat.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        cat.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        cat.selection_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        cat.display_order = sqlite3_column_int(stmt, 5);
        categories.push_back(cat);
    }
    sqlite3_finalize(stmt);

    // Now load choices for each category
    std::string choice_sql = "SELECT id, category_id, name, price_delta, swatch_hex, thumbnail, mesh_target, is_default, display_order FROM option_choices WHERE category_id = ? ORDER BY display_order ASC, name ASC";
    for (auto& cat : categories) {
        sqlite3_stmt* c_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, choice_sql.c_str(), -1, &c_stmt, nullptr) != SQLITE_OK) {
            continue;
        }
        sqlite3_bind_int(c_stmt, 1, cat.id);

        while (sqlite3_step(c_stmt) == SQLITE_ROW) {
            OptionChoice choice;
            choice.id = sqlite3_column_int(c_stmt, 0);
            choice.category_id = sqlite3_column_int(c_stmt, 1);
            choice.name = reinterpret_cast<const char*>(sqlite3_column_text(c_stmt, 2));
            choice.price_delta = sqlite3_column_double(c_stmt, 3);
            const char* hex = reinterpret_cast<const char*>(sqlite3_column_text(c_stmt, 4));
            choice.swatch_hex = hex ? hex : "";
            if (sqlite3_column_type(c_stmt, 5) != SQLITE_NULL) {
                choice.thumbnail = reinterpret_cast<const char*>(sqlite3_column_text(c_stmt, 5));
            }
            const char* mesh = reinterpret_cast<const char*>(sqlite3_column_text(c_stmt, 6));
            choice.mesh_target = mesh ? mesh : "";
            choice.is_default = sqlite3_column_int(c_stmt, 7) != 0;
            choice.display_order = sqlite3_column_int(c_stmt, 8);

            cat.choices.push_back(choice);
        }
        sqlite3_finalize(c_stmt);
    }

    return categories;
}

std::optional<std::pair<Vehicle, std::vector<OptionCategory>>> Database::get_vehicle_by_slug(const std::string& slug) {
    std::string sql = "SELECT id, name, slug, vehicle_type, tagline, description, base_price, model_file, thumbnail, is_published, created_at, updated_at FROM vehicles WHERE slug = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    Vehicle v;
    v.id = sqlite3_column_int(stmt, 0);
    v.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    v.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    v.vehicle_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    const char* tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    v.tagline = tag ? tag : "";
    const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    v.description = desc ? desc : "";
    v.base_price = sqlite3_column_double(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
        v.model_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    }
    if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
        v.thumbnail = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    }
    v.is_published = sqlite3_column_int(stmt, 9) != 0;
    v.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    v.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    sqlite3_finalize(stmt);

    auto categories = load_categories_for_vehicle(v.id);
    return std::make_pair(v, categories);
}

std::optional<Vehicle> Database::get_vehicle_by_id(int id) {
    std::string sql = "SELECT id, name, slug, vehicle_type, tagline, description, base_price, model_file, thumbnail, is_published, created_at, updated_at FROM vehicles WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_int(stmt, 1, id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    Vehicle v;
    v.id = sqlite3_column_int(stmt, 0);
    v.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    v.slug = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    v.vehicle_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    const char* tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    v.tagline = tag ? tag : "";
    const char* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    v.description = desc ? desc : "";
    v.base_price = sqlite3_column_double(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
        v.model_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
    }
    if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
        v.thumbnail = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    }
    v.is_published = sqlite3_column_int(stmt, 9) != 0;
    v.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
    v.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    sqlite3_finalize(stmt);

    return v;
}

bool Database::update_vehicle_model(int id, const std::string& model_file) {
    std::string sql = "UPDATE vehicles SET model_file = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, model_file.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::update_vehicle_model_by_slug(const std::string& slug, const std::string& model_file) {
    std::string sql = "UPDATE vehicles SET model_file = ?, updated_at = ? WHERE slug = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, model_file.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, slug.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::optional<Vehicle> Database::create_vehicle(const std::string& name, const std::string& slug, const std::string& vehicle_type, const std::string& tagline, const std::string& description, double base_price, const std::string& model_file, bool is_published) {
    std::string sql = "INSERT INTO vehicles (name, slug, vehicle_type, tagline, description, base_price, model_file, is_published, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, slug.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, vehicle_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, tagline.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, base_price);
    if (!model_file.empty()) {
        sqlite3_bind_text(stmt, 7, model_file.c_str(), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    sqlite3_bind_int(stmt, 8, is_published ? 1 : 0);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, now.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    int new_id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    sqlite3_finalize(stmt);

    return get_vehicle_by_id(new_id);
}

bool Database::delete_vehicle(int id) {
    std::string sql = "DELETE FROM vehicles WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::clear_vehicle_model_by_file(const std::string& model_file) {
    std::string sql = "UPDATE vehicles SET model_file = NULL, updated_at = ? WHERE model_file = ? OR model_file LIKE ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    std::string now = current_iso_time();
    std::string pattern = "%" + model_file;
    sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, model_file.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, pattern.c_str(), -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::update_vehicle_details(int id, const std::string& name, const std::string& vehicle_type, const std::string& tagline, const std::string& description, double base_price) {
    std::string sql = "UPDATE vehicles SET name = ?, vehicle_type = ?, tagline = ?, description = ?, base_price = ?, updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    std::string now = current_iso_time();
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, vehicle_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, tagline.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, description.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, base_price);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int Database::add_option_category(int vehicle_id, const std::string& name, const std::string& slug, const std::string& sel_type, int order) {
    std::string sql = "INSERT INTO option_categories (vehicle_id, name, slug, selection_type, display_order) VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt* s = nullptr;
    int id = 0;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(s, 1, vehicle_id);
        sqlite3_bind_text(s, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(s, 3, slug.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(s, 4, sel_type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(s, 5, order);
        if (sqlite3_step(s) == SQLITE_DONE) {
            id = static_cast<int>(sqlite3_last_insert_rowid(db_));
        }
        sqlite3_finalize(s);
    }
    return id;
}

int Database::add_option_choice(int category_id, const std::string& name, double delta, const std::string& hex, const std::string& mesh, bool is_default, int order) {
    std::string sql = "INSERT INTO option_choices (category_id, name, price_delta, swatch_hex, mesh_target, is_default, display_order) VALUES (?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* s = nullptr;
    int id = 0;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(s, 1, category_id);
        sqlite3_bind_text(s, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(s, 3, delta);
        sqlite3_bind_text(s, 4, hex.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(s, 5, mesh.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(s, 6, is_default ? 1 : 0);
        sqlite3_bind_int(s, 7, order);
        if (sqlite3_step(s) == SQLITE_DONE) {
            id = static_cast<int>(sqlite3_last_insert_rowid(db_));
        }
        sqlite3_finalize(s);
    }
    return id;
}

bool Database::upsert_vehicle(const Vehicle& vehicle) {
    if (vehicle.id > 0) {
        std::string sql = "UPDATE vehicles SET name = ?, vehicle_type = ?, tagline = ?, description = ?, base_price = ?, model_file = ?, thumbnail = ?, is_published = ?, updated_at = ? WHERE id = ?";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return false;
        std::string now = current_iso_time();
        sqlite3_bind_text(stmt, 1, vehicle.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, vehicle.vehicle_type.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, vehicle.tagline.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, vehicle.description.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 5, vehicle.base_price);
        if (vehicle.model_file.has_value()) {
            sqlite3_bind_text(stmt, 6, vehicle.model_file->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 6);
        }
        if (vehicle.thumbnail.has_value()) {
            sqlite3_bind_text(stmt, 7, vehicle.thumbnail->c_str(), -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt, 7);
        }
        sqlite3_bind_int(stmt, 8, vehicle.is_published ? 1 : 0);
        sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 10, vehicle.id);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return ok;
    }
    return false;
}

std::vector<OptionChoice> Database::load_selected_options(const std::string& config_id) {
    std::vector<OptionChoice> choices;
    std::string sql = R"(
        SELECT c.id, c.category_id, c.name, c.price_delta, c.swatch_hex, c.thumbnail, c.mesh_target, c.is_default, c.display_order
        FROM option_choices c
        JOIN configuration_selections s ON s.option_id = c.id
        WHERE s.configuration_id = ?
        ORDER BY c.category_id ASC, c.display_order ASC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return choices;
    }
    sqlite3_bind_text(stmt, 1, config_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OptionChoice choice;
        choice.id = sqlite3_column_int(stmt, 0);
        choice.category_id = sqlite3_column_int(stmt, 1);
        choice.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        choice.price_delta = sqlite3_column_double(stmt, 3);
        const char* hex = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        choice.swatch_hex = hex ? hex : "";
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            choice.thumbnail = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        }
        const char* mesh = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        choice.mesh_target = mesh ? mesh : "";
        choice.is_default = sqlite3_column_int(stmt, 7) != 0;
        choice.display_order = sqlite3_column_int(stmt, 8);
        choices.push_back(choice);
    }
    sqlite3_finalize(stmt);

    return choices;
}

std::optional<Configuration> Database::get_configuration(const std::string& id) {
    std::string sql = "SELECT id, vehicle_id, name, session_key, created_at, updated_at FROM configurations WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    Configuration cfg;
    cfg.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    cfg.vehicle_id = sqlite3_column_int(stmt, 1);
    const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    cfg.name = name ? name : "";
    const char* sess = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    cfg.session_key = sess ? sess : "";
    cfg.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    cfg.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    sqlite3_finalize(stmt);

    // Calculate total price: vehicle base price + option price deltas
    auto v_opt = get_vehicle_by_id(cfg.vehicle_id);
    double base = v_opt ? v_opt->base_price : 0.0;
    cfg.selected_options = load_selected_options(cfg.id);
    cfg.total_price = base;
    for (const auto& opt : cfg.selected_options) {
        cfg.total_price += opt.price_delta;
    }

    return cfg;
}

std::vector<Configuration> Database::get_configurations(const std::string& session_key) {
    std::vector<Configuration> list;
    std::string sql = "SELECT id FROM configurations";
    if (!session_key.empty()) {
        sql += " WHERE session_key = ?";
    }
    sql += " ORDER BY created_at DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }
    if (!session_key.empty()) {
        sqlite3_bind_text(stmt, 1, session_key.c_str(), -1, SQLITE_STATIC);
    }

    std::vector<std::string> ids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ids.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);

    for (const auto& id : ids) {
        auto cfg = get_configuration(id);
        if (cfg) {
            list.push_back(*cfg);
        }
    }
    return list;
}

std::optional<Configuration> Database::create_configuration(int vehicle_id, const std::vector<int>& option_ids, const std::string& session_key, const std::string& name) {
    std::string id = generate_uuid();
    std::string now = current_iso_time();

    std::string sql = "INSERT INTO configurations (id, vehicle_id, name, session_key, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, vehicle_id);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, session_key.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return std::nullopt;
    }

    // Insert selections
    std::string sel_sql = "INSERT OR IGNORE INTO configuration_selections (configuration_id, option_id) VALUES (?, ?)";
    for (int opt_id : option_ids) {
        sqlite3_stmt* s_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sel_sql.c_str(), -1, &s_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s_stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(s_stmt, 2, opt_id);
            sqlite3_step(s_stmt);
            sqlite3_finalize(s_stmt);
        }
    }

    return get_configuration(id);
}

std::optional<Configuration> Database::update_configuration(const std::string& id, const std::vector<int>& option_ids) {
    std::string now = current_iso_time();
    std::string sql = "UPDATE configurations SET updated_at = ? WHERE id = ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Replace selections
    std::string del_sql = "DELETE FROM configuration_selections WHERE configuration_id = ?";
    sqlite3_stmt* d_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, del_sql.c_str(), -1, &d_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(d_stmt, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(d_stmt);
        sqlite3_finalize(d_stmt);
    }

    std::string sel_sql = "INSERT OR IGNORE INTO configuration_selections (configuration_id, option_id) VALUES (?, ?)";
    for (int opt_id : option_ids) {
        sqlite3_stmt* s_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sel_sql.c_str(), -1, &s_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s_stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(s_stmt, 2, opt_id);
            sqlite3_step(s_stmt);
            sqlite3_finalize(s_stmt);
        }
    }

    return get_configuration(id);
}

bool Database::delete_configuration(const std::string& id) {
    std::string del_selections = "DELETE FROM configuration_selections WHERE configuration_id = ?";
    sqlite3_stmt* s1 = nullptr;
    if (sqlite3_prepare_v2(db_, del_selections.c_str(), -1, &s1, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s1, 1, id.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(s1);
        sqlite3_finalize(s1);
    }

    std::string del_cfg = "DELETE FROM configurations WHERE id = ?";
    sqlite3_stmt* s2 = nullptr;
    if (sqlite3_prepare_v2(db_, del_cfg.c_str(), -1, &s2, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(s2, 1, id.c_str(), -1, SQLITE_STATIC);
        int rc = sqlite3_step(s2);
        sqlite3_finalize(s2);
        return rc == SQLITE_DONE;
    }
    return false;
}

std::pair<int, int> Database::import_manifest_file(const std::string& filepath, bool publish) {
    int created = 0;
    int updated = 0;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open manifest file: " << filepath << "\n";
        return {created, updated};
    }

    try {
        nlohmann::json root;
        file >> root;
        if (!root.contains("assets") || !root["assets"].is_array()) {
            std::cerr << "Manifest missing 'assets' array.\n";
            return {created, updated};
        }

        std::string now = current_iso_time();

        for (const auto& entry : root["assets"]) {
            std::string name = entry.value("name", "");
            std::string slug = entry.value("slug", "");
            if (slug.empty() && !name.empty()) {
                // simple slugify
                for (char c : name) {
                    if (std::isalnum(static_cast<unsigned char>(c))) slug += static_cast<char>(std::tolower(c));
                    else if (c == ' ' || c == '-' || c == '_') slug += '-';
                }
            }
            std::string vehicle_type = entry.value("vehicleType", "car");
            std::string glb_path = entry.value("glbPath", "");
            double base_price = entry.value("basePrice", 0.0);

            // Check if vehicle exists
            std::string check_sql = "SELECT id FROM vehicles WHERE slug = ?";
            sqlite3_stmt* chk = nullptr;
            bool exists = false;
            int existing_id = 0;
            if (sqlite3_prepare_v2(db_, check_sql.c_str(), -1, &chk, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(chk, 1, slug.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(chk) == SQLITE_ROW) {
                    exists = true;
                    existing_id = sqlite3_column_int(chk, 0);
                }
                sqlite3_finalize(chk);
            }

            if (exists) {
                std::string upd_sql = "UPDATE vehicles SET name = ?, vehicle_type = ?, model_file = ?, base_price = ?, is_published = ?, updated_at = ? WHERE id = ?";
                sqlite3_stmt* u_stmt = nullptr;
                if (sqlite3_prepare_v2(db_, upd_sql.c_str(), -1, &u_stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(u_stmt, 1, name.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(u_stmt, 2, vehicle_type.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(u_stmt, 3, glb_path.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_double(u_stmt, 4, base_price);
                    sqlite3_bind_int(u_stmt, 5, publish ? 1 : 0);
                    sqlite3_bind_text(u_stmt, 6, now.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_int(u_stmt, 7, existing_id);
                    sqlite3_step(u_stmt);
                    sqlite3_finalize(u_stmt);
                    updated++;
                }
            } else {
                std::string ins_sql = "INSERT INTO vehicles (name, slug, vehicle_type, base_price, model_file, is_published, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
                sqlite3_stmt* i_stmt = nullptr;
                if (sqlite3_prepare_v2(db_, ins_sql.c_str(), -1, &i_stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(i_stmt, 1, name.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(i_stmt, 2, slug.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(i_stmt, 3, vehicle_type.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_double(i_stmt, 4, base_price);
                    sqlite3_bind_text(i_stmt, 5, glb_path.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_int(i_stmt, 6, publish ? 1 : 0);
                    sqlite3_bind_text(i_stmt, 7, now.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(i_stmt, 8, now.c_str(), -1, SQLITE_STATIC);
                    sqlite3_step(i_stmt);
                    sqlite3_finalize(i_stmt);
                    created++;
                }
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "JSON error parsing manifest: " << ex.what() << "\n";
    }

    return {created, updated};
}

void Database::seed_default_catalog_if_empty() {
    std::string count_sql = "SELECT COUNT(*) FROM vehicles";
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(db_, count_sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (count > 0) {
        return; // already has data
    }

    std::cout << "[Database] Catalog is empty. Seeding default vehicles and options...\n";
    std::string now = current_iso_time();

    auto insert_vehicle = [&](const std::string& name, const std::string& slug, const std::string& type,
                              const std::string& tagline, const std::string& desc, double base_price,
                              const std::string& model_file = "") -> int {
        std::string sql = "INSERT INTO vehicles (name, slug, vehicle_type, tagline, description, base_price, model_file, is_published, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?)";
        sqlite3_stmt* s = nullptr;
        int id = 0;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 2, slug.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 3, type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 4, tagline.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 5, desc.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(s, 6, base_price);
            if (!model_file.empty()) {
                sqlite3_bind_text(s, 7, model_file.c_str(), -1, SQLITE_STATIC);
            } else {
                sqlite3_bind_null(s, 7);
            }
            sqlite3_bind_text(s, 8, now.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 9, now.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(s) == SQLITE_DONE) {
                id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            }
            sqlite3_finalize(s);
        }
        return id;
    };

    auto insert_category = [&](int vehicle_id, const std::string& name, const std::string& slug, const std::string& sel_type, int order) -> int {
        std::string sql = "INSERT INTO option_categories (vehicle_id, name, slug, selection_type, display_order) VALUES (?, ?, ?, ?, ?)";
        sqlite3_stmt* s = nullptr;
        int id = 0;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(s, 1, vehicle_id);
            sqlite3_bind_text(s, 2, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 3, slug.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 4, sel_type.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(s, 5, order);
            if (sqlite3_step(s) == SQLITE_DONE) {
                id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            }
            sqlite3_finalize(s);
        }
        return id;
    };

    auto insert_choice = [&](int category_id, const std::string& name, double delta, const std::string& hex, const std::string& mesh, bool is_default, int order) -> int {
        std::string sql = "INSERT INTO option_choices (category_id, name, price_delta, swatch_hex, mesh_target, is_default, display_order) VALUES (?, ?, ?, ?, ?, ?, ?)";
        sqlite3_stmt* s = nullptr;
        int id = 0;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(s, 1, category_id);
            sqlite3_bind_text(s, 2, name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(s, 3, delta);
            sqlite3_bind_text(s, 4, hex.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(s, 5, mesh.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(s, 6, is_default ? 1 : 0);
            sqlite3_bind_int(s, 7, order);
            if (sqlite3_step(s) == SQLITE_DONE) {
                id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            }
            sqlite3_finalize(s);
        }
        return id;
    };

    // 1. Velora GT (Car)
    int v1 = insert_vehicle("Velora GT", "velora-gt", "car",
                            "Flagship electric grand tourer with active aero and cybernetic cockpit",
                            "The Velora GT blends dual-motor instant torque with an ultra-rigid carbon-composite monocoque. Configurable aerodynamic wings and custom metallic finishes.",
                            84000.0, "/media/models/velora-gt.glb");
    if (v1 > 0) {
        int c_col = insert_category(v1, "Exterior color", "exterior-color", "single", 1);
        insert_choice(c_col, "Nebula Purple", 0.0, "#6d28d9", "Body_Paint", true, 1);
        insert_choice(c_col, "Lunar Silver", 800.0, "#9a9aa6", "Body_Paint", false, 2);
        insert_choice(c_col, "Void Black", 800.0, "#0c0b12", "Body_Paint", false, 3);
        insert_choice(c_col, "Ion White", 600.0, "#f4f3f8", "Body_Paint", false, 4);

        int c_whl = insert_category(v1, "Wheels", "wheels", "single", 2);
        insert_choice(c_whl, "19in Aero", 0.0, "", "Wheel_Rim", true, 1);
        insert_choice(c_whl, "20in Forged", 2200.0, "", "Wheel_Rim", false, 2);
        insert_choice(c_whl, "21in Carbon", 4100.0, "", "Wheel_Rim", false, 3);

        int c_int = insert_category(v1, "Interior", "interior", "single", 3);
        insert_choice(c_int, "Graphite weave", 0.0, "#3a3a42", "Interior_Trim", true, 1);
        insert_choice(c_int, "Violet leather", 1600.0, "#5b21b6", "Interior_Trim", false, 2);
    }

    // 2. Solace Roadster (Car)
    int v2 = insert_vehicle("Solace Roadster", "solace-roadster", "car",
                            "Open-top, low-slung, built for coastal canyon carving",
                            "Pure analog driving feel combined with next-gen electronic stabilization and lightweight magnesium alloys.",
                            96500.0);
    if (v2 > 0) {
        int c_col = insert_category(v2, "Exterior color", "exterior-color", "single", 1);
        insert_choice(c_col, "Solar Flare Gold", 0.0, "#eab308", "Body_Paint", true, 1);
        insert_choice(c_col, "Onyx Obsidian", 1200.0, "#111827", "Body_Paint", false, 2);
        insert_choice(c_col, "Alpine Frost", 950.0, "#f8fafc", "Body_Paint", false, 3);

        int c_whl = insert_category(v2, "Wheels", "wheels", "single", 2);
        insert_choice(c_whl, "20in Sport", 0.0, "", "Wheel_Rim", true, 1);
        insert_choice(c_whl, "21in Performance", 2800.0, "", "Wheel_Rim", false, 2);

        int c_int = insert_category(v2, "Interior", "interior", "single", 3);
        insert_choice(c_int, "Black Alcantara", 0.0, "#1e293b", "Interior_Trim", true, 1);
        insert_choice(c_int, "Cognac Nappa", 2400.0, "#9a3412", "Interior_Trim", false, 2);
    }

    // 3. Marea 42 (Speed Boat)
    int v3 = insert_vehicle("Marea 42", "marea-42", "boat",
                            "42ft luxury offshore center console powerboat",
                            "Engineered for heavy seas and triple-digit speed with dual step hull architecture and aerospace grade telemetry.",
                            412000.0);
    if (v3 > 0) {
        int c_hull = insert_category(v3, "Hull finish", "hull-finish", "single", 1);
        insert_choice(c_hull, "Arctic Pearl", 0.0, "#e2e8f0", "Hull_Gelcoat", true, 1);
        insert_choice(c_hull, "Abyss Blue", 4500.0, "#1e3a8a", "Hull_Gelcoat", false, 2);
        insert_choice(c_hull, "Titanium Gray", 3200.0, "#475569", "Hull_Gelcoat", false, 3);

        int c_deck = insert_category(v3, "Decking", "decking", "single", 2);
        insert_choice(c_deck, "Natural Teak", 0.0, "#b45309", "Deck_Surface", true, 1);
        insert_choice(c_deck, "Synthetic CoolDeck", 6500.0, "#64748b", "Deck_Surface", false, 2);

        int c_pwr = insert_category(v3, "Power", "power", "single", 3);
        insert_choice(c_pwr, "Twin 450R", 0.0, "", "Engine_Bay", true, 1);
        insert_choice(c_pwr, "Triple 450R Racing", 55000.0, "", "Engine_Bay", false, 2);
    }

    // 4. Nyx Streetfighter (Bike)
    int v4 = insert_vehicle("Nyx Streetfighter", "nyx-streetfighter", "bike",
                            "Naked hyper-naked street motorcycle with carbon chassis",
                            "Raw aggressive stance, 208 horsepower V4 engine, integrated aerodynamic winglets, and race telemetry dashboard.",
                            21800.0);
    if (v4 > 0) {
        int c_fair = insert_category(v4, "Fairing & Tank", "fairing-tank", "single", 1);
        insert_choice(c_fair, "Stealth Matte Black", 0.0, "#18181b", "Body_Fairing", true, 1);
        insert_choice(c_fair, "Neon Acid Green", 650.0, "#84cc16", "Body_Fairing", false, 2);
        insert_choice(c_fair, "Cyber Violet", 750.0, "#8b5cf6", "Body_Fairing", false, 3);

        int c_exh = insert_category(v4, "Exhaust System", "exhaust", "single", 2);
        insert_choice(c_exh, "Stock Stainless", 0.0, "", "Exhaust_Pipe", true, 1);
        insert_choice(c_exh, "Titanium Slip-on", 1450.0, "", "Exhaust_Pipe", false, 2);
        insert_choice(c_exh, "Full Race Titanium System", 3200.0, "", "Exhaust_Pipe", false, 3);
    }

    std::cout << "[Database] Seeded 4 default vehicles with custom option sets.\n";
}

} // namespace kraft
