#pragma once

#include <string>
#include <vector>
#include <optional>
#include <iomanip>
#include <sstream>
#include "json.hpp"

namespace kraft {

inline std::string format_price(double price) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << price;
    return ss.str();
}

struct OptionChoice {
    int id = 0;
    int category_id = 0;
    std::string name;
    double price_delta = 0.0;
    std::string swatch_hex;
    std::optional<std::string> thumbnail;
    std::string mesh_target;
    bool is_default = false;
    int display_order = 0;

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"id", id},
            {"name", name},
            {"price_delta", format_price(price_delta)},
            {"swatch_hex", swatch_hex},
            {"thumbnail", thumbnail.has_value() ? nlohmann::json(thumbnail.value()) : nlohmann::json(nullptr)},
            {"mesh_target", mesh_target},
            {"is_default", is_default}
        };
        return j;
    }
};

struct OptionCategory {
    int id = 0;
    int vehicle_id = 0;
    std::string name;
    std::string slug;
    std::string selection_type = "single";
    int display_order = 0;
    std::vector<OptionChoice> choices;

    nlohmann::json to_json() const {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : choices) {
            arr.push_back(c.to_json());
        }
        return {
            {"id", id},
            {"name", name},
            {"slug", slug},
            {"selection_type", selection_type},
            {"choices", arr}
        };
    }
};

struct Vehicle {
    int id = 0;
    std::string name;
    std::string slug;
    std::string vehicle_type;
    std::string tagline;
    std::string description;
    double base_price = 0.0;
    std::optional<std::string> model_file;
    std::optional<std::string> thumbnail;
    bool is_published = false;
    std::string created_at;
    std::string updated_at;

    // List view serialization (matches DRF VehicleListSerializer)
    nlohmann::json to_list_json() const {
        return {
            {"id", id},
            {"name", name},
            {"slug", slug},
            {"vehicle_type", vehicle_type},
            {"tagline", tagline},
            {"base_price", format_price(base_price)},
            {"thumbnail", thumbnail.has_value() ? nlohmann::json(thumbnail.value()) : nlohmann::json(nullptr)}
        };
    }

    // Detail view serialization (matches DRF VehicleDetailSerializer)
    nlohmann::json to_detail_json(const std::vector<OptionCategory>& categories) const {
        nlohmann::json cat_arr = nlohmann::json::array();
        for (const auto& cat : categories) {
            cat_arr.push_back(cat.to_json());
        }
        return {
            {"id", id},
            {"name", name},
            {"slug", slug},
            {"vehicle_type", vehicle_type},
            {"tagline", tagline},
            {"description", description},
            {"base_price", format_price(base_price)},
            {"model_file", model_file.has_value() ? nlohmann::json(model_file.value()) : nlohmann::json(nullptr)},
            {"thumbnail", thumbnail.has_value() ? nlohmann::json(thumbnail.value()) : nlohmann::json(nullptr)},
            {"option_categories", cat_arr}
        };
    }
};

struct Configuration {
    std::string id;
    int vehicle_id = 0;
    std::string name;
    std::string session_key;
    std::vector<OptionChoice> selected_options;
    double total_price = 0.0;
    std::string created_at;
    std::string updated_at;

    nlohmann::json to_json() const {
        nlohmann::json selections_arr = nlohmann::json::array();
        for (const auto& opt : selected_options) {
            selections_arr.push_back({
                {"option", opt.to_json()}
            });
        }
        return {
            {"id", id},
            {"vehicle", vehicle_id},
            {"name", name},
            {"session_key", session_key},
            {"selections", selections_arr},
            {"total_price", format_price(total_price)},
            {"created_at", created_at},
            {"updated_at", updated_at}
        };
    }
};

} // namespace kraft
