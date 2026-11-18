#include "seed.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>

namespace kraft {

static std::vector<std::string> str_list(const json& j) {
    std::vector<std::string> out;
    if (j.is_string()) {
        out = split_csv(j.get<std::string>());
    } else if (j.is_array()) {
        for (const auto& s : j)
            if (s.is_string()) out.push_back(s.get<std::string>());
    }
    return out;
}

static double rounded_price(double v) {
    // Scaled template prices land on clean $10 steps.
    return std::round(v / 10.0) * 10.0;
}

bool SeedLibrary::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    try {
        in >> root_;
    } catch (const std::exception& e) {
        std::cerr << "[Seed] Failed to parse " << path << ": " << e.what() << "\n";
        return false;
    }
    version_ = root_.value("version", 1);
    path_ = path;
    loaded_ = true;
    return true;
}

OptionCategory SeedLibrary::build_category(const json& cat, double price_scale) const {
    OptionCategory c;
    c.name = cat.value("name", "Options");
    c.slug = cat.value("slug", slugify(c.name));
    c.section = cat.value("section", "exterior");
    c.display = cat.value("display", "list");
    c.selection_type = cat.value("selection", "single");
    c.description = cat.value("description", "");
    c.display_order = cat.value("order", 0);
    const std::string role = cat.value("role", "");

    if (cat.contains("options")) {
        for (const auto& o : cat["options"]) {
            OptionChoice ch;
            ch.code = o.value("code", "");
            ch.name = o.value("name", ch.code);
            ch.description = o.value("desc", o.value("description", ""));
            ch.price_delta = rounded_price(o.value("price", 0.0) * price_scale);
            ch.swatch_hex = o.value("hex", "");
            ch.finish = o.value("finish", "");
            ch.tag = o.value("tag", "");
            ch.is_default = o.value("default", false);
            ch.mesh_target = o.value("mesh_target", "");
            if (o.contains("visual")) ch.visual = o["visual"].is_array() ? o["visual"] : json::array({o["visual"]});
            else if (!role.empty() && !ch.swatch_hex.empty()) {
                json v = {{"role", role}, {"color", ch.swatch_hex}};
                if (!ch.finish.empty()) v["finish"] = ch.finish;
                ch.visual = json::array({v});
            }
            if (o.contains("mods") && o["mods"].is_object()) ch.mods = o["mods"];
            ch.requires_codes = str_list(o.value("requires", json::array()));
            ch.excludes_codes = str_list(o.value("excludes", json::array()));
            ch.includes_codes = str_list(o.value("includes", json::array()));
            ch.variants = str_list(o.value("variants", json::array()));
            if (!ch.code.empty()) c.choices.push_back(std::move(ch));
        }
    }
    return c;
}

// Drop rule references to codes that don't exist on this vehicle (a template
// may mention an option that a particular vehicle dropped) and make sure
// single-choice categories have exactly one default.
void SeedLibrary::finalize(VehicleBlueprint& bp) const {
    std::unordered_set<std::string> codes;
    for (const auto& c : bp.categories)
        for (const auto& ch : c.choices) codes.insert(ch.code);

    auto keep_known = [&](std::vector<std::string>& list) {
        list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& s) { return !codes.count(s); }), list.end());
    };

    for (auto& c : bp.categories) {
        bool has_default = false;
        for (auto& ch : c.choices) {
            keep_known(ch.excludes_codes);
            keep_known(ch.includes_codes);
            std::vector<std::string> reqs;
            for (const auto& r : ch.requires_codes) {
                std::string kept;
                size_t start = 0;
                while (start <= r.size()) {
                    size_t bar = r.find('|', start);
                    std::string alt = r.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
                    if (codes.count(alt)) kept += (kept.empty() ? "" : "|") + alt;
                    if (bar == std::string::npos) break;
                    start = bar + 1;
                }
                if (!kept.empty()) reqs.push_back(kept);
            }
            ch.requires_codes = reqs;
            if (c.is_single() && ch.is_default) {
                if (has_default) ch.is_default = false;
                has_default = true;
            }
        }
        if (c.is_single() && !has_default && !c.choices.empty()) c.choices.front().is_default = true;
    }

    // Order categories by the conventional section flow, keeping template order within a section.
    static const std::vector<std::string> flow = {"exterior", "wheels", "interior", "performance", "technology", "comfort", "packages"};
    auto rank = [&](const std::string& s) {
        auto it = std::find(flow.begin(), flow.end(), s);
        return it == flow.end() ? static_cast<int>(flow.size()) : static_cast<int>(it - flow.begin());
    };
    std::stable_sort(bp.categories.begin(), bp.categories.end(), [&](const OptionCategory& a, const OptionCategory& b) {
        int ra = rank(a.section), rb = rank(b.section);
        if (ra != rb) return ra < rb;
        return a.display_order < b.display_order;
    });
    for (size_t i = 0; i < bp.categories.size(); ++i) bp.categories[i].display_order = static_cast<int>(i + 1);

    bp.vehicle.base_price = bp.variants.empty() ? bp.vehicle.base_price : bp.variants.front().price;
    for (const auto& v : bp.variants) bp.vehicle.base_price = std::min(bp.vehicle.base_price, v.price);
}

std::vector<VehicleBlueprint> SeedLibrary::vehicles() const {
    std::vector<VehicleBlueprint> out;
    if (!loaded_ || !root_.contains("vehicles")) return out;
    const json& templates = root_.value("templates", json::object());

    int order = 0;
    for (const auto& vj : root_["vehicles"]) {
        VehicleBlueprint bp;
        Vehicle& v = bp.vehicle;
        v.name = vj.value("name", "Vehicle");
        v.slug = vj.value("slug", slugify(v.name));
        v.vehicle_type = vj.value("type", "car");
        v.body_class = vj.value("class", "");
        v.family = vj.value("family", "");
        v.year = vj.value("year", 0);
        v.tagline = vj.value("tagline", "");
        v.description = vj.value("description", "");
        v.shape = vj.value("shape", json::object());
        v.material_map = vj.value("material_map", json::object());
        v.source = "seed";
        v.is_published = true;
        v.display_order = ++order;
        if (vj.contains("model_file") && vj["model_file"].is_string()) v.model_file = vj["model_file"].get<std::string>();

        const double scale = vj.value("price_scale", 1.0);
        const auto drop = str_list(vj.value("drop", json::array()));
        const std::set<std::string> dropped(drop.begin(), drop.end());

        int tpl_order = 0;
        for (const auto& name : str_list(vj.value("templates", json::array()))) {
            if (!templates.contains(name)) {
                std::cerr << "[Seed] " << v.slug << ": unknown template '" << name << "'\n";
                continue;
            }
            OptionCategory c = build_category(templates[name], scale);
            c.display_order = (++tpl_order) * 10;
            bp.categories.push_back(std::move(c));
        }
        if (vj.contains("categories")) {
            for (const auto& cj : vj["categories"]) {
                OptionCategory c = build_category(cj, 1.0);  // vehicle-specific prices are absolute
                c.display_order = cj.value("order", 50) * 10 - 5;
                bp.categories.push_back(std::move(c));
            }
        }
        for (auto& c : bp.categories) {
            c.choices.erase(std::remove_if(c.choices.begin(), c.choices.end(),
                                           [&](const OptionChoice& ch) { return dropped.count(ch.code) > 0; }),
                            c.choices.end());
        }
        bp.categories.erase(std::remove_if(bp.categories.begin(), bp.categories.end(),
                                           [](const OptionCategory& c) { return c.choices.empty(); }),
                            bp.categories.end());

        if (vj.contains("availability") && vj["availability"].is_object()) {
            for (auto it = vj["availability"].begin(); it != vj["availability"].end(); ++it) {
                for (auto& c : bp.categories)
                    for (auto& ch : c.choices)
                        if (ch.code == it.key()) ch.variants = str_list(it.value());
            }
        }

        int var_order = 0;
        for (const auto& vv : vj.value("variants", json::array())) {
            Variant var;
            var.code = vv.value("code", "STD");
            var.name = vv.value("name", v.name);
            var.description = vv.value("desc", "");
            var.price = vv.value("price", 0.0);
            var.specs = vv.value("specs", json::object());
            var.is_default = vv.value("default", false);
            var.display_order = ++var_order;
            bp.variants.push_back(std::move(var));
        }
        if (bp.variants.empty()) {
            Variant var;
            var.code = "STD";
            var.name = v.name;
            var.price = vj.value("base_price", 0.0);
            var.is_default = true;
            bp.variants.push_back(var);
        }

        finalize(bp);
        out.push_back(std::move(bp));
    }
    return out;
}

VehicleBlueprint SeedLibrary::upload_blueprint(const std::string& vehicle_type, double base_price) const {
    VehicleBlueprint bp;
    const json& templates = root_.value("templates", json::object());
    const json& uploads = root_.value("upload_templates", json::object());
    std::string type = uploads.contains(vehicle_type) ? vehicle_type : "car";
    const json& spec = uploads.value(type, json::object());

    // Scale option prices loosely with the vehicle price so a $15k bike
    // doesn't get $18k paint.
    double scale = 1.0;
    if (type == "car") scale = std::clamp(base_price / 120000.0, 0.4, 2.0);
    else if (type == "boat") scale = std::clamp(base_price / 250000.0, 0.1, 4.0);
    else scale = std::clamp(base_price / 20000.0, 0.4, 2.0);

    int order = 0;
    for (const auto& name : str_list(spec.value("templates", json::array()))) {
        if (!templates.contains(name)) continue;
        OptionCategory c = build_category(templates[name], scale);
        c.display_order = (++order) * 10;
        bp.categories.push_back(std::move(c));
    }
    int var_order = 0;
    for (const auto& vv : spec.value("variants", json::array())) {
        Variant var;
        var.code = vv.value("code", "STD");
        var.name = vv.value("name", "Standard");
        var.price = rounded_price(base_price * vv.value("factor", 1.0));
        var.specs = vv.value("specs", json::object());
        var.is_default = var_order == 0;
        var.display_order = ++var_order;
        bp.variants.push_back(std::move(var));
    }
    if (bp.variants.empty()) {
        Variant var;
        var.code = "STD";
        var.name = "Standard";
        var.price = base_price;
        var.is_default = true;
        bp.variants.push_back(var);
    }
    bp.vehicle.base_price = base_price;
    finalize(bp);
    return bp;
}

int SeedLibrary::apply(Database& db, bool force) const {
    if (!loaded_) return 0;
    int current = std::atoi(db.get_meta("seed_version", "0").c_str());
    if (!force && current >= version_) return 0;
    int written = 0;
    for (const auto& bp : vehicles()) {
        if (db.upsert_blueprint(bp)) ++written;
        else std::cerr << "[Seed] Failed to write " << bp.vehicle.slug << "\n";
    }
    db.set_meta("seed_version", std::to_string(version_));
    return written;
}

int SeedLibrary::upgrade_legacy(Database& db) const {
    int upgraded = 0;
    for (const auto& v : db.get_vehicles("", false)) {
        if (!db.get_variants(v.id).empty()) continue;
        auto cats = db.get_categories(v.id);
        std::set<std::string> slugs;
        for (const auto& c : cats) slugs.insert(c.slug);
        bool legacy_auto = cats.empty() || slugs == std::set<std::string>{"exterior-finish", "aerodynamics-trim"};

        if (legacy_auto && loaded_) {
            VehicleBlueprint bp = upload_blueprint(v.vehicle_type, v.base_price > 0 ? v.base_price : 75000.0);
            if (db.replace_vehicle_content(v.id, bp.variants, bp.categories)) ++upgraded;
        } else {
            // Hand-built option tree: keep it, just add a default variant.
            Variant var;
            var.code = "STD";
            var.name = "Standard";
            var.price = v.base_price;
            var.is_default = true;
            if (db.add_variant(v.id, var)) ++upgraded;
        }
    }
    return upgraded;
}

}  // namespace kraft
