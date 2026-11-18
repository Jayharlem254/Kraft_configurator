// Kraft Configurator — native C++ configuration engine & HTTP server.

#include <algorithm>
#include <cctype>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "db.hpp"
#include "engine.hpp"
#include "httplib.h"
#include "json.hpp"
#include "models.hpp"
#include "seed.hpp"

namespace fs = std::filesystem;
using kraft::json;

static std::unique_ptr<httplib::Server> g_server;
static const char* ENGINE_VERSION = "3.0.0";

static void signal_handler(int signum) {
    std::cout << "\n[Server] Received signal " << signum << ", shutting down...\n";
    if (g_server) g_server->stop();
}

// ---------------------------------------------------------------------------
// Path discovery — works whether launched from the repo root, backend-cpp/ or bin/.
// ---------------------------------------------------------------------------

static std::string first_existing(const std::vector<fs::path>& candidates, const std::string& must_contain = "") {
    for (const auto& p : candidates) {
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        if (!must_contain.empty() && !fs::exists(p / must_contain, ec)) continue;
        return fs::canonical(p, ec).string();
    }
    return "";
}

static std::string find_media_directory(const std::string& user_path, const fs::path& exe_dir) {
    if (!user_path.empty()) {
        fs::create_directories(fs::path(user_path) / "models");
        return fs::canonical(user_path).string();
    }
    std::string found = first_existing({
        exe_dir / "../../backend/media", exe_dir / "../backend/media", "backend/media", "../backend/media",
        "Kraft_configurator/backend/media", exe_dir / "media", "media",
    });
    if (found.empty()) {
        fs::create_directories(exe_dir / "media/models");
        found = fs::canonical(exe_dir / "media").string();
    }
    fs::create_directories(fs::path(found) / "models");
    return found;
}

static std::string find_ui_directory(const std::string& user_path, const fs::path& exe_dir) {
    if (!user_path.empty()) return first_existing({user_path}, "index.html");
    return first_existing({
        exe_dir / "../../frontend/dist", exe_dir / "../frontend/dist", "frontend/dist", "../frontend/dist",
        "Kraft_configurator/frontend/dist", "dist",
    }, "index.html");
}

static std::string find_seed_file(const std::string& user_path, const fs::path& exe_dir) {
    if (!user_path.empty()) return user_path;
    for (const auto& p : std::vector<fs::path>{exe_dir / "../data/catalog_seed.json", exe_dir / "data/catalog_seed.json",
                                               "data/catalog_seed.json", "backend-cpp/data/catalog_seed.json",
                                               "Kraft_configurator/backend-cpp/data/catalog_seed.json"}) {
        std::error_code ec;
        if (fs::exists(p, ec)) return fs::canonical(p, ec).string();
    }
    return "";
}

// ---------------------------------------------------------------------------
// HTTP helpers
// ---------------------------------------------------------------------------

static void send_json(httplib::Response& res, const json& body, int status = 200) {
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

static void send_error(httplib::Response& res, int status, const std::string& message, const std::string& detail = "") {
    json body = {{"error", message}, {"detail", detail.empty() ? message : detail}};
    send_json(res, body, status);
}

// Conditional GET: catalog responses carry the snapshot version as ETag.
static bool not_modified(const httplib::Request& req, httplib::Response& res, const std::string& tag) {
    const std::string etag = "\"" + tag + "\"";
    res.set_header("ETag", etag);
    res.set_header("Cache-Control", "no-cache");
    if (req.get_header_value("If-None-Match") == etag) {
        res.status = 304;
        return true;
    }
    return false;
}

static double json_number(const json& body, const std::string& key, double fallback) {
    if (!body.contains(key)) return fallback;
    const auto& v = body[key];
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) {
        try {
            return std::stod(v.get<std::string>());
        } catch (...) {
        }
    }
    return fallback;
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string unique_slug(kraft::Database& db, const std::string& name) {
    std::string base = kraft::slugify(name);
    if (base.empty()) base = "vehicle";
    std::string slug = base;
    for (int i = 2; db.get_vehicle_by_slug(slug); ++i) slug = base + "-" + std::to_string(i);
    return slug;
}

static const std::vector<std::string> ALLOWED_MODEL_EXT = {".glb", ".gltf", ".obj", ".fbx"};

int main(int argc, char* argv[]) {
    const auto boot = std::chrono::steady_clock::now();
    fs::path exe_dir = fs::absolute(fs::path(argv[0])).parent_path();

    int port = 8000;
    std::string host = "0.0.0.0";
    std::string db_path = (exe_dir / "kraft_configurator.db").string();
    std::string manifest_path, media_path, ui_path, seed_path;
    bool publish_manifest = false, no_seed = false, force_seed = false;
    size_t threads = std::max<size_t>(4, std::thread::hardware_concurrency() * 2);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--port") port = std::stoi(next());
        else if (arg == "--host") host = next();
        else if (arg == "--db") db_path = next();
        else if (arg == "--import-manifest") manifest_path = next();
        else if (arg == "--publish") publish_manifest = true;
        else if (arg == "--media") media_path = next();
        else if (arg == "--ui") ui_path = next();
        else if (arg == "--seed") seed_path = next();
        else if (arg == "--no-seed") no_seed = true;
        else if (arg == "--reseed" || arg == "--seed-defaults") force_seed = true;
        else if (arg == "--threads") threads = static_cast<size_t>(std::max(1, std::stoi(next())));
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Kraft Configurator Engine " << ENGINE_VERSION << "\n"
                      << "Usage: kraft_backend [options]\n"
                      << "  --port <port>             Port to listen on (default 8000)\n"
                      << "  --host <host>             Bind address (default 0.0.0.0)\n"
                      << "  --db <path>               SQLite database (default: next to the executable)\n"
                      << "  --media <dir>             Directory served at /media/\n"
                      << "  --ui <dir>                Built UI directory (frontend/dist)\n"
                      << "  --seed <file>             Catalog seed JSON (default: data/catalog_seed.json)\n"
                      << "  --reseed                  Re-apply the seed catalog even if up to date\n"
                      << "  --no-seed                 Do not import the seed catalog\n"
                      << "  --import-manifest <file>  Import asset-pipeline manifest.json\n"
                      << "  --publish                 Publish vehicles imported from the manifest\n"
                      << "  --threads <n>             Worker threads (default 2x cores)\n";
            return 0;
        }
    }

    std::cout << "========================================================\n"
              << "   KRAFT CONFIGURATOR ENGINE " << ENGINE_VERSION << "  (C++17 / SQLite)\n"
              << "========================================================\n";

    kraft::Database db(db_path);
    if (!db.open()) {
        std::cerr << "[Fatal] Could not open database at " << db_path << "\n";
        return 1;
    }
    std::cout << "[Database] " << db_path << "\n";

    kraft::SeedLibrary seeds;
    std::string seed_file = find_seed_file(seed_path, exe_dir);
    if (!seed_file.empty() && seeds.load(seed_file)) {
        std::cout << "[Seed] Library v" << seeds.version() << " loaded from " << seed_file << "\n";
        if (!no_seed) {
            int n = seeds.apply(db, force_seed);
            if (n > 0) std::cout << "[Seed] Imported " << n << " vehicles into the catalog\n";
        }
    } else {
        std::cout << "[Seed] No catalog_seed.json found — uploads will get a minimal option set\n";
    }
    if (int up = seeds.upgrade_legacy(db); up > 0) {
        std::cout << "[Migrate] Upgraded " << up << " legacy vehicle(s) to variants + full option sets\n";
    }

    if (!manifest_path.empty()) {
        std::ifstream mf(manifest_path);
        try {
            json root;
            mf >> root;
            int imported = 0;
            for (const auto& entry : root.value("assets", json::array())) {
                std::string name = entry.value("name", "Imported Vehicle");
                std::string type = entry.value("vehicleType", "car");
                double price = entry.value("basePrice", 75000.0);
                kraft::VehicleBlueprint bp = seeds.upload_blueprint(type, price);
                bp.vehicle.name = name;
                bp.vehicle.slug = entry.value("slug", kraft::slugify(name));
                bp.vehicle.vehicle_type = type;
                bp.vehicle.body_class = entry.value("bodyClass", "");
                bp.vehicle.model_file = entry.value("glbPath", "");
                bp.vehicle.is_published = publish_manifest;
                bp.vehicle.source = "upload";
                if (db.upsert_blueprint(bp)) ++imported;
            }
            std::cout << "[Manifest] Imported " << imported << " vehicles\n";
        } catch (const std::exception& e) {
            std::cerr << "[Manifest] " << e.what() << "\n";
        }
    }

    kraft::CatalogCache cache(db);
    kraft::EvalStats eval_stats;
    cache.get();  // warm the snapshot before the first request

    std::string media_dir = find_media_directory(media_path, exe_dir);
    std::string ui_dir = find_ui_directory(ui_path, exe_dir);
    std::cout << "[Media] /media -> " << media_dir << "\n";

    g_server = std::make_unique<httplib::Server>();
    auto& svr = *g_server;
    svr.new_task_queue = [threads] { return new httplib::ThreadPool(threads); };
    svr.set_payload_max_length(512ull * 1024 * 1024);
    svr.set_keep_alive_max_count(1000);
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Range, If-None-Match"},
        {"Access-Control-Expose-Headers", "Content-Range, Content-Length, Accept-Ranges, ETag, X-Eval-Time-Us"},
    });
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    svr.set_mount_point("/media", media_dir);
    if (!ui_dir.empty()) {
        std::cout << "[UI] Serving SPA from " << ui_dir << "\n";
        svr.set_mount_point("/", ui_dir);
    }
    // Hashed build assets never change; media is revalidated hourly; the shell always.
    svr.set_file_request_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.path.rfind("/assets/", 0) == 0) res.set_header("Cache-Control", "public, max-age=31536000, immutable");
        else if (req.path.rfind("/media/", 0) == 0) res.set_header("Cache-Control", "public, max-age=3600");
        else res.set_header("Cache-Control", "no-cache");
    });

    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        if (req.path.rfind("/assets/", 0) == 0) return;  // keep the log readable
        std::cout << "[" << kraft::current_iso_time() << "] " << req.method << " " << req.path << " -> " << res.status << "\n";
    });
    svr.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
        std::string what = "unknown error";
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            what = e.what();
        } catch (...) {
        }
        std::cerr << "[Exception] " << req.path << ": " << what << "\n";
        send_error(res, 500, "Internal server error", what);
    });

    // ---- Health & meta ---------------------------------------------------
    svr.Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        auto cat = cache.get();
        double uptime = std::chrono::duration<double>(std::chrono::steady_clock::now() - boot).count();
        send_json(res, {
            {"service", "Kraft Configurator Engine"},
            {"status", "online"},
            {"version", ENGINE_VERSION},
            {"engine", "C++17 / SQLite WAL / cpp-httplib"},
            {"vehicles_count", cat->vehicles.size()},
            {"catalog_version", cat->version},
            {"catalog_build_ms", cache.last_build_ms()},
            {"seed_version", seeds.version()},
            {"rules", eval_stats.to_json()},
            {"threads", threads},
            {"uptime_s", uptime},
            {"timestamp", kraft::current_iso_time()},
        });
    });

    svr.Get("/api/catalog/meta", [&](const httplib::Request& req, httplib::Response& res) {
        auto cat = cache.get();
        if (not_modified(req, res, cat->etag + "-meta")) return;
        std::map<std::string, std::map<std::string, int>> counts;
        std::map<std::string, double> from;
        for (const auto& d : cat->vehicles) {
            if (!d->vehicle.is_published) continue;
            const std::string cls = d->vehicle.body_class.empty() ? "other" : d->vehicle.body_class;
            counts[d->vehicle.vehicle_type][cls]++;
        }
        json types = json::array();
        for (const auto& [type, classes] : counts) {
            json cl = json::array();
            int total = 0;
            for (const auto& [name, n] : classes) {
                cl.push_back({{"class", name}, {"count", n}});
                total += n;
            }
            types.push_back({{"type", type}, {"count", total}, {"classes", cl}});
        }
        send_json(res, {{"types", types}, {"version", cat->version}});
    });

    // ---- Vehicles ----------------------------------------------------------
    auto list_vehicles = [&](const httplib::Request& req, httplib::Response& res) {
        auto cat = cache.get();
        const std::string type = req.get_param_value("vehicle_type");
        const std::string cls = req.get_param_value("body_class");
        const std::string q = lower(req.get_param_value("q"));
        const bool all = req.get_param_value("all") == "1";
        std::string etag = cat->etag + "-" + type + "-" + cls + "-" + q + (all ? "-a" : "");
        if (not_modified(req, res, etag)) return;
        json arr = json::array();
        for (const auto& d : cat->vehicles) {
            const auto& v = d->vehicle;
            if (!all && !v.is_published) continue;
            if (!type.empty() && v.vehicle_type != type) continue;
            if (!cls.empty() && v.body_class != cls) continue;
            if (!q.empty() && lower(v.name + " " + v.family + " " + v.body_class + " " + v.tagline).find(q) == std::string::npos) continue;
            arr.push_back(d->list_json());
        }
        send_json(res, arr);
    };
    svr.Get("/api/vehicles", list_vehicles);
    svr.Get("/api/vehicles/", list_vehicles);

    svr.Get(R"(/api/vehicles/([a-zA-Z0-9_-]+)/?)", [&](const httplib::Request& req, httplib::Response& res) {
        auto cat = cache.get();
        auto def = cat->find(req.matches[1]);
        if (!def) return send_error(res, 404, "Not found.");
        if (not_modified(req, res, cat->etag + "-" + def->vehicle.slug)) return;
        send_json(res, def->detail_json());
    });

    // The rules engine: resolve a requested build into a valid, priced one.
    svr.Post(R"(/api/vehicles/([a-zA-Z0-9_-]+)/evaluate/?)", [&](const httplib::Request& req, httplib::Response& res) {
        auto t0 = std::chrono::steady_clock::now();
        auto cat = cache.get();
        auto def = cat->find(req.matches[1]);
        if (!def) return send_error(res, 404, "Vehicle not found.");
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded()) return send_error(res, 400, "Invalid JSON body");
        json result = kraft::RulesEngine::evaluate(*def, kraft::RulesEngine::parse_request(body));
        auto us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count());
        eval_stats.record(us);
        result["engine_us"] = us;
        res.set_header("X-Eval-Time-Us", std::to_string(us));
        send_json(res, result);
    });

    // Upload a 3D model; creates a new vehicle with the full option set for its type,
    // or attaches the model to an existing vehicle (vehicle_id / vehicle_slug).
    auto handle_upload = [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.is_multipart_form_data()) return send_error(res, 400, "Request must be multipart/form-data");
        httplib::FormData file;
        if (req.form.has_file("file")) file = req.form.get_file("file");
        else if (req.form.has_file("model")) file = req.form.get_file("model");
        else if (!req.form.files.empty()) file = req.form.files.begin()->second;
        if (file.content.empty()) return send_error(res, 400, "No 3D model file provided or file is empty");

        fs::path original(file.filename.empty() ? "model.glb" : file.filename);
        std::string ext = lower(original.extension().string());
        if (ext.empty()) ext = ".glb";
        if (std::find(ALLOWED_MODEL_EXT.begin(), ALLOWED_MODEL_EXT.end(), ext) == ALLOWED_MODEL_EXT.end())
            return send_error(res, 400, "Unsupported file type " + ext + " (use .glb, .gltf, .obj or .fbx)");

        std::string stem;
        for (unsigned char c : original.stem().string()) {
            if (std::isalnum(c)) stem += static_cast<char>(std::tolower(c));
            else if (c == ' ' || c == '-' || c == '_') stem += '_';
        }
        if (stem.empty()) stem = "vehicle_model";
        const std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        const std::string filename = stem + "_" + ts + ext;
        fs::path target = fs::path(media_dir) / "models" / filename;
        {
            std::ofstream out(target, std::ios::binary);
            if (!out) return send_error(res, 500, "Failed to write model to disk", target.string());
            out.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        }
        const std::string uri = "/media/models/" + filename;
        std::cout << "[Upload] " << file.content.size() << " bytes -> " << target.string() << "\n";

        auto field = [&](const char* k, const std::string& fb = "") { return req.form.has_field(k) ? req.form.get_field(k) : fb; };
        std::optional<kraft::Vehicle> vehicle;
        int vehicle_id = 0;
        try { vehicle_id = std::stoi(field("vehicle_id", "0")); } catch (...) {}
        if (vehicle_id > 0 && db.get_vehicle_by_id(vehicle_id)) {
            db.update_vehicle_model(vehicle_id, uri);
            vehicle = db.get_vehicle_by_id(vehicle_id);
        } else if (!field("vehicle_slug").empty() && db.get_vehicle_by_slug(field("vehicle_slug"))) {
            db.update_vehicle_model_by_slug(field("vehicle_slug"), uri);
            vehicle = db.get_vehicle_by_slug(field("vehicle_slug"));
        } else {
            double price = 75000.0;
            try { price = std::stod(field("base_price", "75000")); } catch (...) {}
            std::string type = field("vehicle_type", "car");
            kraft::VehicleBlueprint bp = seeds.upload_blueprint(type, price);
            bp.vehicle.name = field("name", stem);
            bp.vehicle.slug = unique_slug(db, bp.vehicle.name);
            bp.vehicle.vehicle_type = type;
            bp.vehicle.body_class = field("body_class", "");
            bp.vehicle.family = field("family", "");
            bp.vehicle.tagline = field("tagline", "Custom imported 3D vehicle");
            bp.vehicle.description = field("description", "Imported into Kraft Configurator.");
            bp.vehicle.model_file = uri;
            bp.vehicle.is_published = true;
            bp.vehicle.source = "upload";
            bp.vehicle.year = 2027;
            if (int id = db.upsert_blueprint(bp)) vehicle = db.get_vehicle_by_id(id);
        }
        cache.invalidate();

        json resp = {{"status", "success"}, {"message", "3D model uploaded."}, {"file_path", uri},
                     {"filename", filename}, {"size_bytes", file.content.size()}};
        if (vehicle) {
            if (auto def = cache.get()->find(vehicle->slug)) resp["vehicle"] = def->detail_json();
        }
        send_json(res, resp, 201);
    };
    svr.Post("/api/vehicles/upload", handle_upload);
    svr.Post("/api/upload", handle_upload);

    // Create a vehicle entry (e.g. for an already uploaded, unassigned model).
    svr.Post(R"(/api/vehicles/?)", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded()) return send_error(res, 400, "Invalid JSON");
        std::string type = body.value("vehicle_type", "car");
        kraft::VehicleBlueprint bp = seeds.upload_blueprint(type, json_number(body, "base_price", 75000.0));
        bp.vehicle.name = body.value("name", "New Vehicle");
        bp.vehicle.slug = body.contains("slug") ? body["slug"].get<std::string>() : unique_slug(db, bp.vehicle.name);
        bp.vehicle.vehicle_type = type;
        bp.vehicle.body_class = body.value("body_class", "");
        bp.vehicle.family = body.value("family", "");
        bp.vehicle.tagline = body.value("tagline", "Custom vehicle platform");
        bp.vehicle.description = body.value("description", "Imported into Kraft Configurator.");
        std::string mf = body.value("model_file", "");
        if (!mf.empty()) bp.vehicle.model_file = mf;
        bp.vehicle.is_published = true;
        bp.vehicle.source = "upload";
        bp.vehicle.year = 2027;
        int id = db.upsert_blueprint(bp);
        if (!id) return send_error(res, 500, "Failed to create vehicle");
        cache.invalidate();
        auto def = cache.get()->find(std::to_string(id));
        send_json(res, def ? def->detail_json() : json::object(), 201);
    });

    // Store the 3D model analyser's report (computed in the browser, where the
    // model is decoded). When it detected the model's own paint, the engine adds
    // that as an "Original finish" option and makes it the default.
    svr.Put(R"(/api/vehicles/([0-9]+)/analysis/?)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto v = db.get_vehicle_by_id(id);
        if (!v) return send_error(res, 404, "Vehicle not found");
        if (req.body.size() > 512 * 1024) return send_error(res, 413, "Analysis report too large");
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded() || !body.is_object()) return send_error(res, 400, "Analysis must be a JSON object");
        if (!db.set_vehicle_analysis(id, body)) return send_error(res, 500, "Failed to store analysis");

        bool finish_added = false;
        auto paint = body.find("original_paint");
        if (paint != body.end() && paint->is_object()) {
            auto str = [&](const char* k, const std::string& fb) {
                auto it = paint->find(k);
                return it != paint->end() && it->is_string() ? it->get<std::string>() : fb;
            };
            static const std::vector<std::string> finishes = {"solid", "gloss", "metallic", "pearl", "matte", "satin",
                                                              "chrome", "titanium", "carbon"};
            std::string color = lower(str("color", ""));
            std::string finish = str("finish", "gloss");
            std::string role = str("role", v->vehicle_type == "boat" ? "hull" : "paint");
            bool hex = color.size() == 7 && color[0] == '#' &&
                       std::all_of(color.begin() + 1, color.end(), [](unsigned char c) { return std::isxdigit(c); });
            if (std::find(finishes.begin(), finishes.end(), finish) == finishes.end()) finish = "gloss";
            if (hex && (role == "paint" || role == "hull")) {
                kraft::OptionChoice c;
                c.code = "ORIG";
                c.name = "Original finish";
                c.description = "The finish this model was created in, detected from the 3D file.";
                c.swatch_hex = color;
                c.finish = finish;
                c.visual = json::array({{{"role", role}, {"color", color}, {"finish", finish}, {"original", true}}});
                c.tag = "Detected";
                c.is_default = true;
                c.display_order = 0;
                finish_added = db.apply_original_finish(id, role, c);
            }
        }
        cache.invalidate();
        auto def = cache.get()->find(std::to_string(id));
        json out = def ? def->detail_json() : json::object();
        out["original_finish_added"] = finish_added;
        send_json(res, out);
    });

    auto update_vehicle = [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto v = db.get_vehicle_by_id(id);
        if (!v) return send_error(res, 404, "Vehicle not found");
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded()) return send_error(res, 400, "Invalid JSON");
        v->name = body.value("name", v->name);
        v->vehicle_type = body.value("vehicle_type", v->vehicle_type);
        v->body_class = body.value("body_class", v->body_class);
        v->family = body.value("family", v->family);
        v->tagline = body.value("tagline", v->tagline);
        v->description = body.value("description", v->description);
        v->base_price = json_number(body, "base_price", v->base_price);
        v->is_published = body.value("is_published", v->is_published);
        if (body.contains("material_map") && body["material_map"].is_object()) v->material_map = body["material_map"];
        if (body.contains("shape") && body["shape"].is_object()) v->shape = body["shape"];
        if (body.contains("model_file")) {
            if (body["model_file"].is_null() || body["model_file"] == "") v->model_file.reset();
            else v->model_file = body["model_file"].get<std::string>();
        }
        db.update_vehicle_core(*v);
        cache.invalidate();
        auto def = cache.get()->find(std::to_string(id));
        send_json(res, def ? def->list_json() : json::object());
    };
    svr.Patch(R"(/api/vehicles/([0-9]+)/?)", update_vehicle);
    svr.Put(R"(/api/vehicles/([0-9]+)/?)", update_vehicle);

    svr.Delete(R"(/api/vehicles/([0-9]+)/?)", [&](const httplib::Request& req, httplib::Response& res) {
        int id = std::stoi(req.matches[1]);
        auto v = db.get_vehicle_by_id(id);
        if (!v) return send_error(res, 404, "Vehicle not found");
        std::string del = req.get_param_value("delete_file");
        if ((del == "1" || del == "true") && v->model_file) {
            fs::path f = fs::path(media_dir) / "models" / fs::path(*v->model_file).filename();
            std::error_code ec;
            fs::remove(f, ec);
        }
        if (!db.delete_vehicle(id)) return send_error(res, 500, "Failed to delete vehicle");
        cache.invalidate();
        send_json(res, {{"status", "success"}, {"message", "Vehicle deleted"}});
    });

    // ---- 3D model files ----------------------------------------------------
    auto list_models = [&](const httplib::Request&, httplib::Response& res) {
        auto cat = cache.get();
        json arr = json::array();
        fs::path dir = fs::path(media_dir) / "models";
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = lower(entry.path().extension().string());
            if (std::find(ALLOWED_MODEL_EXT.begin(), ALLOWED_MODEL_EXT.end(), ext) == ALLOWED_MODEL_EXT.end()) continue;
            std::string fname = entry.path().filename().string();
            std::string uri = "/media/models/" + fname;
            json assigned = nullptr;
            for (const auto& d : cat->vehicles) {
                if (d->vehicle.model_file && *d->vehicle.model_file == uri) {
                    assigned = {{"id", d->vehicle.id}, {"name", d->vehicle.name}, {"slug", d->vehicle.slug}, {"vehicle_type", d->vehicle.vehicle_type}};
                    break;
                }
            }
            std::error_code fec;
            arr.push_back({{"filename", fname}, {"file_path", uri}, {"size_bytes", entry.file_size(fec)}, {"extension", ext}, {"vehicle", assigned}});
        }
        send_json(res, arr);
    };
    svr.Get("/api/models", list_models);
    svr.Get("/api/models/", list_models);

    svr.Delete(R"(/api/models/([a-zA-Z0-9_][a-zA-Z0-9_.-]*)/?)", [&](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.matches[1];
        if (filename.find("..") != std::string::npos) return send_error(res, 400, "Invalid filename");
        fs::path target = fs::path(media_dir) / "models" / filename;
        std::error_code ec;
        bool removed = fs::is_regular_file(target, ec) && fs::remove(target, ec);
        db.clear_vehicle_model_by_file(filename);
        cache.invalidate();
        send_json(res, {{"status", "success"}, {"filename", filename}, {"disk_file_removed", removed}});
    });

    // ---- Configurations ------------------------------------------------------
    // Saving always re-runs the rules engine so stored prices are authoritative.
    auto evaluate_for_save = [&](const json& body, std::shared_ptr<const kraft::VehicleDef>& def, json& result,
                                 std::string& err) -> bool {
        auto cat = cache.get();
        std::string key = body.contains("vehicle") ? (body["vehicle"].is_number() ? std::to_string(body["vehicle"].get<int>())
                                                                                  : body["vehicle"].get<std::string>())
                                                   : body.value("vehicle_slug", "");
        def = cat->find(key);
        if (!def) {
            err = "Unknown vehicle '" + key + "'";
            return false;
        }
        kraft::EvalRequest er = kraft::RulesEngine::parse_request(body);
        if (er.selections.empty() && body.contains("option_ids") && body["option_ids"].is_array()) {
            for (const auto& oid : body["option_ids"])
                if (oid.is_number_integer())
                    if (auto* c = def->choice_by_id(oid.get<int>())) er.selections.push_back(c->code);
        }
        result = kraft::RulesEngine::evaluate(*def, er);
        return true;
    };
    auto option_ids_of = [](const json& result) {
        std::vector<int> ids;
        for (const auto& r : result["resolved"]) ids.push_back(r["id"].get<int>());
        return ids;
    };
    auto variant_id_of = [](const kraft::VehicleDef& def, const json& result) {
        const kraft::Variant* v = def.variant(result.value("variant", ""));
        return v ? v->id : 0;
    };

    auto list_configs = [&](const httplib::Request& req, httplib::Response& res) {
        json arr = json::array();
        for (const auto& c : db.get_configurations(req.get_param_value("session_key"))) arr.push_back(c.to_json());
        send_json(res, arr);
    };
    svr.Get("/api/configurations", list_configs);
    svr.Get("/api/configurations/", list_configs);

    auto create_config = [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded()) return send_error(res, 400, "Invalid JSON body");
        std::shared_ptr<const kraft::VehicleDef> def;
        json result;
        std::string err;
        if (!evaluate_for_save(body, def, result, err)) return send_error(res, 400, err);
        auto cfg = db.create_configuration(def->vehicle.id, variant_id_of(*def, result), option_ids_of(result),
                                           result["price"]["total"].get<double>(), result,
                                           body.value("session_key", ""), body.value("name", def->vehicle.name));
        if (!cfg) return send_error(res, 500, "Failed to save configuration");
        send_json(res, cfg->to_json(), 201);
    };
    svr.Post("/api/configurations", create_config);
    svr.Post("/api/configurations/", create_config);

    svr.Get(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", [&](const httplib::Request& req, httplib::Response& res) {
        auto cfg = db.get_configuration(req.matches[1]);
        if (!cfg) return send_error(res, 404, "Configuration not found.");
        send_json(res, cfg->to_json());
    });

    auto update_config = [&](const httplib::Request& req, httplib::Response& res) {
        auto existing = db.get_configuration(req.matches[1]);
        if (!existing) return send_error(res, 404, "Configuration not found.");
        json body = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
        if (body.is_discarded()) return send_error(res, 400, "Invalid JSON body");
        if (!body.contains("vehicle")) body["vehicle"] = existing->vehicle_id;
        std::shared_ptr<const kraft::VehicleDef> def;
        json result;
        std::string err;
        if (!evaluate_for_save(body, def, result, err)) return send_error(res, 400, err);
        std::optional<std::string> name;
        if (body.contains("name") && body["name"].is_string()) name = body["name"].get<std::string>();
        auto cfg = db.update_configuration(existing->id, variant_id_of(*def, result), option_ids_of(result),
                                           result["price"]["total"].get<double>(), result, name);
        if (!cfg) return send_error(res, 500, "Failed to update configuration");
        send_json(res, cfg->to_json());
    };
    svr.Patch(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", update_config);
    svr.Put(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", update_config);

    svr.Delete(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", [&](const httplib::Request& req, httplib::Response& res) {
        if (db.delete_configuration(req.matches[1])) res.status = 204;
        else send_error(res, 404, "Not found.");
    });

    if (ui_dir.empty()) {
        std::cout << "[UI] Built UI not found (run `npm run build` in frontend/). API-only mode.\n";
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            send_json(res, {{"service", "Kraft Configurator Engine"}, {"status", "online"},
                            {"hint", "Build the UI with `npm run build` inside frontend/"}});
        });
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "[Server] " << threads << " worker threads\n"
              << "[Server] Listening on http://" << (host == "0.0.0.0" ? "localhost" : host) << ":" << port << "\n";
    if (!svr.listen(host, port)) {
        std::cerr << "[Error] Failed to bind " << host << ":" << port << "\n";
        return 1;
    }
    std::cout << "[Server] Stopped.\n";
    return 0;
}
