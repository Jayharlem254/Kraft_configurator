#pragma once

#include <string>
#include <vector>

#include "db.hpp"
#include "models.hpp"

namespace kraft {

// Loads backend-cpp/data/catalog_seed.json: reusable option templates plus a
// vehicle line-up that references them. Expands everything into blueprints,
// and also provides the option set given to freshly uploaded 3D models.
class SeedLibrary {
public:
    bool load(const std::string& path);
    bool loaded() const { return loaded_; }
    int version() const { return version_; }
    const std::string& path() const { return path_; }

    std::vector<VehicleBlueprint> vehicles() const;
    VehicleBlueprint upload_blueprint(const std::string& vehicle_type, double base_price) const;

    // Applies the seed when its version is newer than the database's. Returns
    // the number of vehicles written (0 when already up to date).
    int apply(Database& db, bool force) const;

    // Vehicles created before variants existed (or by bare uploads) get a
    // default variant, and the old two-category auto option set is replaced by
    // the full template set for their type.
    int upgrade_legacy(Database& db) const;

private:
    bool loaded_ = false;
    int version_ = 0;
    std::string path_;
    json root_;

    OptionCategory build_category(const json& cat, double price_scale) const;
    void finalize(VehicleBlueprint& bp) const;
};

}  // namespace kraft
