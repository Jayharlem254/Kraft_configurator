#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "models.hpp"
#include "sqlite3.h"

namespace kraft {

class Database {
public:
    explicit Database(const std::string& db_path = "kraft_configurator.db");
    ~Database();

    bool open();
    void close();

    // Vehicles
    std::vector<Vehicle> get_vehicles(const std::string& vehicle_type = "", bool published_only = true);
    std::optional<std::pair<Vehicle, std::vector<OptionCategory>>> get_vehicle_by_slug(const std::string& slug);
    std::optional<Vehicle> get_vehicle_by_id(int id);
    bool upsert_vehicle(const Vehicle& vehicle);
    bool update_vehicle_model(int id, const std::string& model_file);
    bool update_vehicle_model_by_slug(const std::string& slug, const std::string& model_file);
    std::optional<Vehicle> create_vehicle(const std::string& name, const std::string& slug, const std::string& vehicle_type, const std::string& tagline, const std::string& description, double base_price, const std::string& model_file, bool is_published = true);
    bool delete_vehicle(int id);
    bool clear_vehicle_model_by_file(const std::string& model_file);
    bool update_vehicle_details(int id, const std::string& name, const std::string& vehicle_type, const std::string& tagline, const std::string& description, double base_price);
    int add_option_category(int vehicle_id, const std::string& name, const std::string& slug, const std::string& sel_type = "single", int order = 1);
    int add_option_choice(int category_id, const std::string& name, double delta = 0.0, const std::string& hex = "", const std::string& mesh = "", bool is_default = true, int order = 1);

    // Configurations
    std::vector<Configuration> get_configurations(const std::string& session_key = "");
    std::optional<Configuration> get_configuration(const std::string& id);
    std::optional<Configuration> create_configuration(int vehicle_id, const std::vector<int>& option_ids, const std::string& session_key = "", const std::string& name = "");
    std::optional<Configuration> update_configuration(const std::string& id, const std::vector<int>& option_ids);
    bool delete_configuration(const std::string& id);

    // Seed & Manifest
    void seed_default_catalog_if_empty();
    std::pair<int, int> import_manifest_file(const std::string& filepath, bool publish = false);

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;

    bool init_schema();
    bool execute(const std::string& sql);
    std::vector<OptionCategory> load_categories_for_vehicle(int vehicle_id);
    std::vector<OptionChoice> load_selected_options(const std::string& config_id);
};

std::string current_iso_time();
std::string generate_uuid();

} // namespace kraft
