#pragma once

// Kraft configuration engine.
//
// CatalogCache  — immutable, versioned in-memory snapshot of the whole catalog.
//                 Reads never touch SQLite; any write invalidates the snapshot
//                 and the next reader rebuilds it once.
// RulesEngine   — resolves a requested set of option codes into a valid build:
//                 variant availability, single-choice categories, package
//                 includes, requirements (with alternatives) and exclusions,
//                 then prices it and computes the resulting technical specs.

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "db.hpp"
#include "models.hpp"

namespace kraft {

struct ChoiceRef {
    size_t cat = 0;
    size_t choice = 0;
};

struct VehicleDef {
    Vehicle vehicle;
    std::vector<Variant> variants;
    std::vector<OptionCategory> categories;
    std::unordered_map<std::string, ChoiceRef> by_code;
    std::unordered_map<int, ChoiceRef> by_id;

    const OptionChoice* choice(const std::string& code) const;
    const OptionChoice* choice_by_id(int id) const;
    const OptionCategory& category_of(const std::string& code) const;
    const Variant* variant(const std::string& code) const;  // falls back to default
    const Variant* default_variant() const;

    json list_json() const;    // compact card for the model range
    json detail_json() const;  // full definition incl. option tree
};

struct Catalog {
    uint64_t version = 0;
    std::vector<std::shared_ptr<const VehicleDef>> vehicles;
    std::unordered_map<std::string, std::shared_ptr<const VehicleDef>> by_slug;
    std::unordered_map<int, std::shared_ptr<const VehicleDef>> by_id;
    std::string etag;

    std::shared_ptr<const VehicleDef> find(const std::string& slug_or_id) const;
};

class CatalogCache {
public:
    explicit CatalogCache(Database& db) : db_(db) {}

    std::shared_ptr<const Catalog> get();
    void invalidate();
    uint64_t version() const { return version_.load(); }
    double last_build_ms() const { return last_build_ms_.load(); }

private:
    Database& db_;
    std::mutex mutex_;
    std::shared_ptr<const Catalog> snapshot_;
    std::atomic<uint64_t> version_{1};
    std::atomic<double> last_build_ms_{0.0};
};

struct EvalRequest {
    std::string variant;
    std::vector<std::string> selections;  // ordered oldest -> newest (later wins conflicts)
    std::string changed;                  // the option the user just toggled (highest priority)
    bool deselect = false;                // `changed` was switched off rather than on
};

struct EvalStats {
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> total_us{0};
    std::atomic<uint64_t> max_us{0};
    void record(uint64_t us);
    json to_json() const;
};

class RulesEngine {
public:
    // Returns {variant, resolved[], changes[], price{}, specs{}, selections[], valid}
    static json evaluate(const VehicleDef& def, const EvalRequest& req);
    static EvalRequest parse_request(const json& body);
};

}  // namespace kraft
