#pragma once

// Kraft Configurator — core domain model.
//
//   Vehicle (model line, e.g. "Kraft 9R")
//     ├── Variant[]          trims with their own base price + technical specs
//     └── OptionCategory[]   grouped into UI sections (exterior, wheels, …)
//           └── OptionChoice[]  priced options carrying rules (requires /
//                               excludes / includes), per-variant availability,
//                               visual bindings for the 3D engine and spec
//                               modifiers for the performance calculator.

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "json.hpp"

namespace kraft {

using json = nlohmann::json;

inline std::string format_price(double price) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << price;
    return ss.str();
}

inline json opt_str(const std::optional<std::string>& s) {
    return s ? json(*s) : json(nullptr);
}

struct OptionChoice {
    int id = 0;
    int category_id = 0;
    std::string code;          // unique within a vehicle, used by the rules engine
    std::string name;
    std::string description;
    double price_delta = 0.0;
    std::string swatch_hex;
    std::string finish;        // solid | metallic | pearl | matte | satin | chrome | ...
    std::optional<std::string> thumbnail;
    std::string mesh_target;   // legacy explicit binding (mesh or material name)
    json visual = json::array();   // [{role,color,finish} | {part,value}]
    json mods = json::object();    // additive spec modifiers {power_hp: 45, ...}
    std::vector<std::string> requires_codes;  // entries may be "A|B" alternatives
    std::vector<std::string> excludes_codes;
    std::vector<std::string> includes_codes;
    std::vector<std::string> variants;        // empty = available on all variants
    std::string tag;           // e.g. "Popular", "New"
    bool is_default = false;
    int display_order = 0;

    json to_json() const {
        return {
            {"id", id},
            {"code", code},
            {"name", name},
            {"description", description},
            {"price_delta", format_price(price_delta)},
            {"price", price_delta},
            {"swatch_hex", swatch_hex},
            {"finish", finish},
            {"thumbnail", opt_str(thumbnail)},
            {"mesh_target", mesh_target},
            {"visual", visual},
            {"mods", mods},
            {"requires", requires_codes},
            {"excludes", excludes_codes},
            {"includes", includes_codes},
            {"variants", variants},
            {"tag", tag},
            {"is_default", is_default},
        };
    }
};

struct OptionCategory {
    int id = 0;
    int vehicle_id = 0;
    std::string name;
    std::string slug;
    std::string section = "exterior";   // exterior | wheels | interior | performance | technology | comfort | packages
    std::string display = "list";       // swatch | tile | list
    std::string selection_type = "single";
    std::string description;
    int display_order = 0;
    std::vector<OptionChoice> choices;

    bool is_single() const { return selection_type != "multi"; }

    json to_json() const {
        json arr = json::array();
        for (const auto& c : choices) arr.push_back(c.to_json());
        return {
            {"id", id},
            {"name", name},
            {"slug", slug},
            {"section", section},
            {"display", display},
            {"selection_type", selection_type},
            {"description", description},
            {"choices", arr},
        };
    }
};

struct Variant {
    int id = 0;
    int vehicle_id = 0;
    std::string code;
    std::string name;
    std::string description;
    double price = 0.0;
    json specs = json::object();
    bool is_default = false;
    int display_order = 0;

    json to_json() const {
        return {
            {"id", id},
            {"code", code},
            {"name", name},
            {"description", description},
            {"price", price},
            {"specs", specs},
            {"is_default", is_default},
        };
    }
};

struct Vehicle {
    int id = 0;
    std::string name;
    std::string slug;
    std::string vehicle_type;   // car | bike | boat
    std::string body_class;     // coupe, suv, naked, yacht, ...
    std::string family;
    std::string tagline;
    std::string description;
    std::string source = "upload";  // seed | upload
    int year = 0;
    double base_price = 0.0;
    std::optional<std::string> model_file;
    std::optional<std::string> thumbnail;
    json shape = json::object();         // procedural 3D parameters
    json material_map = json::object();  // role -> [material/mesh names] overrides for GLBs
    bool is_published = false;
    int display_order = 0;
    std::string created_at;
    std::string updated_at;

    json base_json() const {
        return {
            {"id", id},
            {"name", name},
            {"slug", slug},
            {"vehicle_type", vehicle_type},
            {"body_class", body_class},
            {"family", family},
            {"tagline", tagline},
            {"description", description},
            {"year", year},
            {"base_price", format_price(base_price)},
            {"model_file", opt_str(model_file)},
            {"thumbnail", opt_str(thumbnail)},
            {"shape", shape},
            {"material_map", material_map},
            {"source", source},
            {"is_published", is_published},
        };
    }
};

struct Configuration {
    std::string id;
    std::string code;
    int vehicle_id = 0;
    std::string vehicle_slug;
    std::string vehicle_name;
    int variant_id = 0;
    std::string variant_code;
    std::string variant_name;
    std::string name;
    std::string session_key;
    std::vector<OptionChoice> selected_options;
    double total_price = 0.0;
    json snapshot = json::object();   // evaluated result at save time (price, specs, resolved list)
    std::string created_at;
    std::string updated_at;

    json to_json() const {
        json selections_arr = json::array();
        json codes = json::array();
        for (const auto& opt : selected_options) {
            selections_arr.push_back({{"option", opt.to_json()}});
            codes.push_back(opt.code);
        }
        return {
            {"id", id},
            {"code", code},
            {"vehicle", vehicle_id},
            {"vehicle_slug", vehicle_slug},
            {"vehicle_name", vehicle_name},
            {"variant", variant_code},
            {"variant_name", variant_name},
            {"name", name},
            {"session_key", session_key},
            {"selections", selections_arr},
            {"option_codes", codes},
            {"total_price", format_price(total_price)},
            {"snapshot", snapshot},
            {"created_at", created_at},
            {"updated_at", updated_at},
        };
    }
};

}  // namespace kraft
