#include "engine.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <deque>
#include <iostream>
#include <set>
#include <unordered_set>

namespace kraft {

// ---------------------------------------------------------------------------
// VehicleDef
// ---------------------------------------------------------------------------

const OptionChoice* VehicleDef::choice(const std::string& code) const {
    auto it = by_code.find(code);
    if (it == by_code.end()) return nullptr;
    return &categories[it->second.cat].choices[it->second.choice];
}

const OptionChoice* VehicleDef::choice_by_id(int id) const {
    auto it = by_id.find(id);
    if (it == by_id.end()) return nullptr;
    return &categories[it->second.cat].choices[it->second.choice];
}

const OptionCategory& VehicleDef::category_of(const std::string& code) const {
    return categories[by_code.at(code).cat];
}

const Variant* VehicleDef::default_variant() const {
    for (const auto& v : variants)
        if (v.is_default) return &v;
    return variants.empty() ? nullptr : &variants.front();
}

const Variant* VehicleDef::variant(const std::string& code) const {
    for (const auto& v : variants)
        if (v.code == code) return &v;
    return default_variant();
}

static double from_price(const VehicleDef& d) {
    if (d.variants.empty()) return d.vehicle.base_price;
    double p = d.variants.front().price;
    for (const auto& v : d.variants) p = std::min(p, v.price);
    return p;
}

json VehicleDef::list_json() const {
    json j = vehicle.base_json();
    j.erase("material_map");
    j.erase("analysis");
    json vars = json::array();
    for (const auto& v : variants) vars.push_back(v.to_json());
    j["variants"] = vars;
    j["from_price"] = from_price(*this);
    j["base_price"] = format_price(from_price(*this));
    size_t options = 0;
    std::set<std::string> sections;
    for (const auto& c : categories) {
        options += c.choices.size();
        sections.insert(c.section);
    }
    j["option_count"] = options;
    j["sections"] = sections;
    return j;
}

json VehicleDef::detail_json() const {
    json j = vehicle.base_json();
    json vars = json::array();
    for (const auto& v : variants) vars.push_back(v.to_json());
    json cats = json::array();
    for (const auto& c : categories) cats.push_back(c.to_json());
    j["variants"] = vars;
    j["option_categories"] = cats;
    j["from_price"] = from_price(*this);
    j["base_price"] = format_price(from_price(*this));
    return j;
}

std::shared_ptr<const VehicleDef> Catalog::find(const std::string& key) const {
    auto it = by_slug.find(key);
    if (it != by_slug.end()) return it->second;
    bool numeric = !key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isdigit(c); });
    if (numeric) {
        auto jt = by_id.find(std::stoi(key));
        if (jt != by_id.end()) return jt->second;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// CatalogCache
// ---------------------------------------------------------------------------

std::shared_ptr<const Catalog> CatalogCache::get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_) return snapshot_;

    auto t0 = std::chrono::steady_clock::now();
    auto cat = std::make_shared<Catalog>();
    cat->version = version_.load();
    cat->etag = "kraft-cat-" + std::to_string(cat->version);

    {
        // Hold the DB lock for the whole rebuild so the snapshot is consistent.
        std::lock_guard<std::recursive_mutex> db_lock(db_.mutex());
        for (auto& v : db_.get_vehicles("", false)) {
            auto def = std::make_shared<VehicleDef>();
            def->vehicle = std::move(v);
            def->variants = db_.get_variants(def->vehicle.id);
            def->categories = db_.get_categories(def->vehicle.id);
            if (def->variants.empty()) {
                Variant fallback;
                fallback.code = "STD";
                fallback.name = "Standard";
                fallback.price = def->vehicle.base_price;
                fallback.is_default = true;
                def->variants.push_back(fallback);
            }
            for (size_t ci = 0; ci < def->categories.size(); ++ci) {
                for (size_t oi = 0; oi < def->categories[ci].choices.size(); ++oi) {
                    const auto& ch = def->categories[ci].choices[oi];
                    def->by_code[ch.code] = {ci, oi};
                    def->by_id[ch.id] = {ci, oi};
                }
            }
            std::shared_ptr<const VehicleDef> frozen = def;
            cat->vehicles.push_back(frozen);
            cat->by_slug[frozen->vehicle.slug] = frozen;
            cat->by_id[frozen->vehicle.id] = frozen;
        }
    }

    snapshot_ = cat;
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    last_build_ms_.store(ms);
    std::cout << "[Engine] Catalog snapshot v" << cat->version << " built: " << cat->vehicles.size()
              << " vehicles in " << ms << " ms\n";
    return snapshot_;
}

void CatalogCache::invalidate() {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.reset();
    version_.fetch_add(1);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

void EvalStats::record(uint64_t us) {
    count.fetch_add(1);
    total_us.fetch_add(us);
    uint64_t prev = max_us.load();
    while (us > prev && !max_us.compare_exchange_weak(prev, us)) {
    }
}

json EvalStats::to_json() const {
    uint64_t n = count.load();
    return {
        {"evaluations", n},
        {"avg_us", n ? static_cast<double>(total_us.load()) / static_cast<double>(n) : 0.0},
        {"max_us", max_us.load()},
    };
}

// ---------------------------------------------------------------------------
// RulesEngine
// ---------------------------------------------------------------------------

EvalRequest RulesEngine::parse_request(const json& body) {
    EvalRequest r;
    r.variant = body.value("variant", "");
    r.changed = body.value("changed", "");
    r.deselect = body.value("deselect", false);
    if (body.contains("selections") && body["selections"].is_array()) {
        for (const auto& s : body["selections"]) {
            if (s.is_string()) r.selections.push_back(s.get<std::string>());
        }
    }
    return r;
}

namespace {

struct Origin {
    std::string type;  // user | included | required | default
    std::string by;    // code of the option that pulled this one in
    std::string root;  // user/default code at the top of the chain
};

double round2(double v) { return std::round(v * 100.0) / 100.0; }

}  // namespace

json RulesEngine::evaluate(const VehicleDef& def, const EvalRequest& req) {
    const Variant* var = def.variant(req.variant);
    const std::string var_code = var ? var->code : "";
    const std::string var_name = var ? var->name : "";

    auto available = [&](const OptionChoice* c) {
        return c && (c->variants.empty() ||
                     std::find(c->variants.begin(), c->variants.end(), var_code) != c->variants.end());
    };

    json changes = json::array();
    auto note = [&](const std::string& type, const OptionChoice* c, const std::string& reason) {
        changes.push_back({{"type", type}, {"code", c->code}, {"name", c->name}, {"reason", reason}});
    };

    // 1. Normalise the request: drop unknown codes, enforce variant availability,
    //    and assign priorities (later in the list = newer = wins conflicts).
    std::unordered_map<std::string, int> prio;
    std::vector<std::string> user;
    int p = 1;
    for (const auto& code : req.selections) {
        if (req.deselect && code == req.changed) continue;
        const OptionChoice* c = def.choice(code);
        if (!c) continue;
        if (!available(c)) {
            if (code == req.changed) note("removed", c, "Not available for the " + var_name);
            continue;
        }
        if (!prio.count(code)) user.push_back(code);
        prio[code] = p++;
    }
    if (!req.changed.empty() && !req.deselect) {
        const OptionChoice* c = def.choice(req.changed);
        if (c && available(c)) {
            if (!prio.count(req.changed)) user.push_back(req.changed);
            prio[req.changed] = INT_MAX / 2;
        } else if (c) {
            note("removed", c, "Not available for the " + var_name);
        }
    }

    // Within a single-choice category only the newest user pick survives.
    {
        std::unordered_map<int, std::string> best;
        for (const auto& code : user) {
            const auto& cat = def.category_of(code);
            if (!cat.is_single()) continue;
            auto it = best.find(cat.id);
            if (it == best.end() || prio[code] > prio[it->second]) best[cat.id] = code;
        }
        user.erase(std::remove_if(user.begin(), user.end(),
                                  [&](const std::string& code) {
                                      const auto& cat = def.category_of(code);
                                      return cat.is_single() && best[cat.id] != code;
                                  }),
                   user.end());
    }

    // Deselecting an option also removes every pick that cannot exist without
    // it — packages that include it and options whose requirement chain ends
    // at it with no other available alternative.
    if (req.deselect && !req.changed.empty() && def.choice(req.changed)) {
        const std::string& gone = req.changed;
        auto depends_on_gone = [&](const std::string& root) {
            std::unordered_set<std::string> seen;
            std::deque<std::string> q{root};
            while (!q.empty()) {
                std::string code = q.front();
                q.pop_front();
                if (code == gone) return true;
                if (!seen.insert(code).second) continue;
                const OptionChoice* c = def.choice(code);
                if (!c) continue;
                for (const auto& inc : c->includes_codes) q.push_back(inc);
                for (const auto& requirement : c->requires_codes) {
                    std::string pick;
                    size_t start = 0;
                    while (start <= requirement.size()) {
                        size_t bar = requirement.find('|', start);
                        std::string alt = requirement.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
                        if (alt != gone && available(def.choice(alt))) { pick = alt; break; }
                        if (bar == std::string::npos) break;
                        start = bar + 1;
                    }
                    q.push_back(pick.empty() ? gone : pick);
                }
            }
            return false;
        };
        const OptionChoice* gc = def.choice(gone);
        for (auto it = user.begin(); it != user.end();) {
            if (depends_on_gone(*it)) {
                const OptionChoice* rc = def.choice(*it);
                bool contains = std::find(rc->includes_codes.begin(), rc->includes_codes.end(), gone) != rc->includes_codes.end();
                note("removed", rc, (contains ? "Contains " : "Requires ") + gc->name);
                it = user.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::vector<std::string> defaults;           // auto-filled single-category picks (lowest priority)
    std::unordered_set<std::string> banned_defaults;
    std::unordered_map<std::string, Origin> sel;
    std::vector<std::string> order;
    bool valid = true;

    auto prio_of = [&](const std::string& root) {
        auto it = prio.find(root);
        return it == prio.end() ? 0 : it->second;  // defaults rank lowest
    };

    // 2. Fixed-point resolution: build the closure, find one conflict, drop the
    //    lower-priority root, repeat. Bounded so bad data can't spin forever.
    for (int iter = 0; iter < 128; ++iter) {
        sel.clear();
        order.clear();
        std::deque<std::string> work;
        std::vector<std::pair<std::string, std::string>> unsatisfied;  // (root, missing requirement)

        auto add = [&](const std::string& code, const std::string& type, const std::string& by, const std::string& root) {
            if (sel.count(code)) return;
            sel[code] = {type, by, root};
            order.push_back(code);
            work.push_back(code);
        };

        std::vector<std::string> roots = user;
        std::stable_sort(roots.begin(), roots.end(), [&](const std::string& a, const std::string& b) { return prio_of(a) > prio_of(b); });
        for (const auto& u : roots) add(u, "user", "", u);
        for (const auto& d : defaults) add(d, "default", "", d);

        while (!work.empty()) {
            std::string code = work.front();
            work.pop_front();
            const OptionChoice* c = def.choice(code);
            const std::string root = sel[code].root;
            for (const auto& inc : c->includes_codes) {
                const OptionChoice* ic = def.choice(inc);
                if (available(ic)) add(inc, "included", code, root);
            }
            for (const auto& requirement : c->requires_codes) {
                std::vector<std::string> alts;
                size_t start = 0;
                while (start <= requirement.size()) {
                    size_t bar = requirement.find('|', start);
                    alts.push_back(requirement.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
                    if (bar == std::string::npos) break;
                    start = bar + 1;
                }
                bool satisfied = std::any_of(alts.begin(), alts.end(), [&](const std::string& a) { return sel.count(a) > 0; });
                if (satisfied) continue;
                // Prefer an alternative the user already asked for, then the first available one.
                std::string pick;
                for (const auto& a : alts)
                    if (prio.count(a) && available(def.choice(a))) { pick = a; break; }
                if (pick.empty())
                    for (const auto& a : alts)
                        if (available(def.choice(a))) { pick = a; break; }
                if (pick.empty()) unsatisfied.emplace_back(root, requirement);
                else add(pick, "required", code, root);
            }
        }

        // Find the first conflict.
        std::string a, b, reason_code;
        if (!unsatisfied.empty()) {
            a = b = unsatisfied.front().first;
            reason_code = unsatisfied.front().second;
        }
        if (a.empty()) {
            for (const auto& cat : def.categories) {
                if (!cat.is_single()) continue;
                std::vector<std::string> members;
                for (const auto& ch : cat.choices)
                    if (sel.count(ch.code)) members.push_back(ch.code);
                if (members.size() > 1) {
                    a = members[0];
                    b = members[1];
                    break;
                }
            }
        }
        if (a.empty()) {
            for (const auto& code : order) {
                for (const auto& ex : def.choice(code)->excludes_codes) {
                    if (sel.count(ex)) {
                        a = code;
                        b = ex;
                        break;
                    }
                }
                if (!a.empty()) break;
            }
        }

        if (a.empty()) {
            // No conflicts: fill any empty single-choice category with its default.
            bool added = false;
            for (const auto& cat : def.categories) {
                if (!cat.is_single()) continue;
                bool has = std::any_of(cat.choices.begin(), cat.choices.end(), [&](const OptionChoice& ch) { return sel.count(ch.code) > 0; });
                if (has) continue;
                auto compatible = [&](const OptionChoice& ch) {
                    if (!available(&ch) || banned_defaults.count(ch.code)) return false;
                    for (const auto& ex : ch.excludes_codes)
                        if (sel.count(ex)) return false;
                    for (const auto& code : order)
                        for (const auto& ex : def.choice(code)->excludes_codes)
                            if (ex == ch.code) return false;
                    return true;
                };
                const OptionChoice* pick = nullptr;
                for (const auto& ch : cat.choices)
                    if (ch.is_default && compatible(ch)) { pick = &ch; break; }
                if (!pick)
                    for (const auto& ch : cat.choices)
                        if (compatible(ch)) { pick = &ch; break; }
                if (pick) {
                    defaults.push_back(pick->code);
                    added = true;
                } else {
                    valid = false;
                }
            }
            if (!added) break;
            continue;
        }

        // Resolve: drop whichever root has the lower priority.
        const std::string ra = sel.count(a) ? sel[a].root : a;
        const std::string rb = sel.count(b) ? sel[b].root : b;
        std::string loser, winner;
        if (ra == rb) {
            loser = ra;
            winner = reason_code.empty() ? b : reason_code;
        } else if (prio_of(ra) < prio_of(rb)) {
            loser = ra;
            winner = (sel.count(b) ? b : rb);
        } else {
            loser = rb;
            winner = (sel.count(a) ? a : ra);
        }

        auto dit = std::find(defaults.begin(), defaults.end(), loser);
        if (dit != defaults.end()) {
            defaults.erase(dit);
            banned_defaults.insert(loser);
            continue;
        }
        auto uit = std::find(user.begin(), user.end(), loser);
        if (uit == user.end()) {
            valid = false;
            break;
        }
        user.erase(uit);
        const OptionChoice* lc = def.choice(loser);
        const OptionChoice* wc = def.choice(winner);
        std::string reason;
        if (!reason_code.empty() && ra == rb) reason = "Its requirement is not available for the " + var_name;
        else if (wc && def.category_of(winner).id == def.category_of(loser).id) reason = "Replaced by " + wc->name;
        else if (wc) reason = "Not compatible with " + wc->name;
        else reason = "Not compatible with the current build";
        note("removed", lc, reason);
    }

    // 3. Package contents are free even when also picked manually.
    std::unordered_map<std::string, std::string> included_by;
    for (const auto& code : order)
        for (const auto& inc : def.choice(code)->includes_codes)
            if (sel.count(inc) && !included_by.count(inc)) included_by[inc] = code;

    std::unordered_set<std::string> requested(req.selections.begin(), req.selections.end());
    if (!req.deselect && !req.changed.empty()) requested.insert(req.changed);

    // 4. Emit in catalog order, price and compute specs.
    json resolved = json::array();
    double options_total = 0.0;
    std::unordered_map<std::string, double> section_totals;
    json specs = var ? var->specs : json::object();
    if (!specs.is_object()) specs = json::object();

    for (const auto& cat : def.categories) {
        for (const auto& ch : cat.choices) {
            auto it = sel.find(ch.code);
            if (it == sel.end()) continue;
            const Origin& o = it->second;
            auto inc = included_by.find(ch.code);
            bool is_included = inc != included_by.end();
            double price = is_included ? 0.0 : ch.price_delta;
            options_total += price;
            section_totals[cat.section] += price;

            json entry = {
                {"code", ch.code},
                {"id", ch.id},
                {"name", ch.name},
                {"category_id", cat.id},
                {"category", cat.name},
                {"section", cat.section},
                {"price", price},
                {"list_price", ch.price_delta},
                {"origin", is_included ? "included" : o.type},
                {"by", is_included ? inc->second : o.by},
            };
            const std::string by_code = entry["by"].get<std::string>();
            if (!by_code.empty()) {
                if (const OptionChoice* bc = def.choice(by_code)) entry["by_name"] = bc->name;
            }
            resolved.push_back(entry);

            if ((o.type == "required" || is_included) && !requested.count(ch.code)) {
                std::string why = is_included ? "Included with " : "Required by ";
                const OptionChoice* bc = def.choice(by_code);
                note("added", &ch, why + (bc ? bc->name : by_code));
            }

            if (ch.mods.is_object()) {
                for (auto m = ch.mods.begin(); m != ch.mods.end(); ++m) {
                    // Modifiers only adjust figures the variant actually publishes;
                    // a "-8 kg" on a vehicle with no weight figure means nothing.
                    if (!m.value().is_number() || !specs.contains(m.key()) || !specs[m.key()].is_number()) continue;
                    specs[m.key()] = round2(specs[m.key()].get<double>() + m.value().get<double>());
                }
            }
        }
    }

    json final_selection = json::array();
    {
        std::vector<std::string> sorted = user;
        std::stable_sort(sorted.begin(), sorted.end(), [&](const std::string& a, const std::string& b) { return prio_of(a) < prio_of(b); });
        for (const auto& u : sorted) final_selection.push_back(u);
    }

    double base = var ? var->price : def.vehicle.base_price;
    json sections = json::object();
    for (const auto& [k, v] : section_totals) sections[k] = round2(v);

    return {
        {"vehicle", def.vehicle.slug},
        {"variant", var_code},
        {"variant_name", var_name},
        {"valid", valid},
        {"selections", final_selection},
        {"resolved", resolved},
        {"changes", changes},
        {"specs", specs},
        {"price", {
            {"base", round2(base)},
            {"options", round2(options_total)},
            {"total", round2(base + options_total)},
            {"sections", sections},
            {"currency", "USD"},
        }},
    };
}

}  // namespace kraft
