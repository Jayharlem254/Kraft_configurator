#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "models.hpp"
#include "sqlite3.h"

namespace kraft {

// Thin RAII wrapper over a prepared statement. Text is always bound with
// SQLITE_TRANSIENT so callers can pass temporaries safely.
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql);
    ~Stmt();
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    bool ok() const { return stmt_ != nullptr; }
    Stmt& bind(int idx, const std::string& v);
    Stmt& bind(int idx, int v);
    Stmt& bind(int idx, double v);
    Stmt& bind_null(int idx);
    Stmt& bind_opt(int idx, const std::optional<std::string>& v);

    bool row();    // steps; true while a row is available
    bool exec();   // steps once; true on SQLITE_DONE / SQLITE_ROW

    int col_int(int i) const;
    double col_double(int i) const;
    std::string col_text(int i) const;
    std::optional<std::string> col_opt(int i) const;
    bool is_null(int i) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// Bundle used by seed import and upload templates.
struct VehicleBlueprint {
    Vehicle vehicle;
    std::vector<Variant> variants;
    std::vector<OptionCategory> categories;
};

class Database {
public:
    explicit Database(std::string db_path);
    ~Database();

    bool open();
    void close();

    // Every public method locks; a recursive mutex lets them call each other.
    std::recursive_mutex& mutex() { return mutex_; }
    bool transaction(const std::function<bool()>& body);

    // ---- Catalog reads -------------------------------------------------
    std::vector<Vehicle> get_vehicles(const std::string& vehicle_type = "", bool published_only = true);
    std::optional<Vehicle> get_vehicle_by_id(int id);
    std::optional<Vehicle> get_vehicle_by_slug(const std::string& slug);
    std::vector<Variant> get_variants(int vehicle_id);
    std::vector<OptionCategory> get_categories(int vehicle_id);
    int count_vehicles();

    // ---- Catalog writes ------------------------------------------------
    std::optional<Vehicle> insert_vehicle(const Vehicle& v);
    bool update_vehicle_core(const Vehicle& v);  // all scalar fields incl. shape/material_map
    bool update_vehicle_model(int id, const std::string& model_file);
    bool set_vehicle_analysis(int id, const json& analysis);
    // Insert or refresh a detected "original finish" choice in the vehicle's
    // paint (or hull) colour category and make it that category's default.
    // Existing choices keep their ids, so saved configurations stay valid.
    bool apply_original_finish(int vehicle_id, const std::string& role, const OptionChoice& choice);
    bool update_vehicle_model_by_slug(const std::string& slug, const std::string& model_file);
    bool clear_vehicle_model_by_file(const std::string& filename);
    bool delete_vehicle(int id);
    // Replace a vehicle's variants and option tree atomically.
    bool replace_vehicle_content(int vehicle_id, const std::vector<Variant>& variants,
                                 const std::vector<OptionCategory>& categories);
    bool add_variant(int vehicle_id, const Variant& v);
    // Insert or update (by slug) a full blueprint; returns vehicle id or 0.
    int upsert_blueprint(const VehicleBlueprint& bp);

    // ---- Configurations ------------------------------------------------
    std::vector<Configuration> get_configurations(const std::string& session_key = "");
    std::optional<Configuration> get_configuration(const std::string& id_or_code);
    std::optional<Configuration> create_configuration(int vehicle_id, int variant_id, const std::vector<int>& option_ids,
                                                      double total_price, const json& snapshot,
                                                      const std::string& session_key, const std::string& name);
    std::optional<Configuration> update_configuration(const std::string& id, int variant_id, const std::vector<int>& option_ids,
                                                      double total_price, const json& snapshot,
                                                      const std::optional<std::string>& name);
    bool delete_configuration(const std::string& id);

    // ---- Meta ----------------------------------------------------------
    std::string get_meta(const std::string& key, const std::string& fallback = "");
    void set_meta(const std::string& key, const std::string& value);

    std::string path() const { return db_path_; }

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;
    std::recursive_mutex mutex_;

    bool execute(const std::string& sql);
    bool init_schema();
    bool migrate();
    bool has_column(const std::string& table, const std::string& column);
    void ensure_column(const std::string& table, const std::string& column, const std::string& decl);

    int insert_variant(int vehicle_id, const Variant& v);
    int insert_category(int vehicle_id, const OptionCategory& c);
    int insert_choice(int category_id, const OptionChoice& c);
    void write_selections(const std::string& config_id, const std::vector<int>& option_ids);
    std::vector<OptionChoice> load_selected_options(const std::string& config_id);
    std::string unique_config_code();
};

std::string current_iso_time();
std::string generate_uuid();
std::string slugify(const std::string& s);
std::string join_csv(const std::vector<std::string>& v);
std::vector<std::string> split_csv(const std::string& s);

}  // namespace kraft
