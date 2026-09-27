#include "httplib.h"
#include "json.hpp"
#include "models.hpp"
#include "db.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <csignal>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

static std::unique_ptr<httplib::Server> g_server;

void signal_handler(int signum) {
    std::cout << "\n[Server] Received signal " << signum << ", shutting down gracefully...\n";
    if (g_server) {
        g_server->stop();
    }
}

std::string find_media_directory(const std::string& user_path) {
    if (!user_path.empty() && fs::exists(user_path)) {
        std::string can = fs::canonical(user_path).string();
        fs::create_directories(can + "/models");
        return can;
    }
    const std::vector<std::string> candidates = {
        "media",
        "../media",
        "backend/media",
        "../backend/media",
        "../../backend/media",
        "Kraft_configurator/backend/media",
        "Kraft_configurator/media"
    };
    for (const auto& path : candidates) {
        if (fs::exists(path)) {
            std::string can = fs::canonical(path).string();
            fs::create_directories(can + "/models");
            return can;
        }
    }
    // Fallback: create ./media/models
    fs::create_directories("media/models");
    return fs::canonical("media").string();
}

std::string find_ui_directory(const std::string& user_path) {
    if (!user_path.empty() && fs::exists(user_path) && fs::exists(user_path + "/index.html")) {
        return fs::canonical(user_path).string();
    }
    const std::vector<std::string> candidates = {
        "dist",
        "../frontend/dist",
        "frontend/dist",
        "../../frontend/dist",
        "Kraft_configurator/frontend/dist"
    };
    for (const auto& path : candidates) {
        if (fs::exists(path) && fs::exists(path + "/index.html")) {
            return fs::canonical(path).string();
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    int port = 8000;
    std::string host = "0.0.0.0";
    std::string db_path = "kraft_configurator.db";
    std::string manifest_path = "";
    std::string media_path = "";
    std::string ui_path = "";
    bool publish_manifest = false;
    bool seed_defaults = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--db" && i + 1 < argc) {
            db_path = argv[++i];
        } else if (arg == "--import-manifest" && i + 1 < argc) {
            manifest_path = argv[++i];
        } else if (arg == "--publish") {
            publish_manifest = true;
        } else if (arg == "--seed-defaults") {
            seed_defaults = true;
        } else if (arg == "--media" && i + 1 < argc) {
            media_path = argv[++i];
        } else if (arg == "--ui" && i + 1 < argc) {
            ui_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Kraft Configurator C++ Backend\n"
                      << "Usage: kraft_backend [options]\n"
                      << "Options:\n"
                      << "  --port <port>              Port to listen on (default: 8000)\n"
                      << "  --host <host>              Host address to bind to (default: 0.0.0.0)\n"
                      << "  --db <path>                SQLite database file (default: kraft_configurator.db)\n"
                      << "  --media <dir>              Directory to serve /media/ files from\n"
                      << "  --ui <dir>                 Directory containing built UI (dist)\n"
                      << "  --import-manifest <path>   Import models from asset-pipeline manifest.json\n"
                      << "  --publish                  Publish imported vehicles immediately\n"
                      << "  --seed-defaults            Seed default placeholder catalog if empty\n"
                      << "  --help, -h                 Show this help message\n";
            return 0;
        }
    }

    std::cout << "========================================================\n";
    std::cout << "        KRAFT CONFIGURATOR - HIGH-SPEED C++ BACKEND      \n";
    std::cout << "========================================================\n";

    kraft::Database db(db_path);
    if (!db.open()) {
        std::cerr << "[Fatal] Could not open database at " << db_path << "\n";
        return 1;
    }
    std::cout << "[Database] Initialized SQLite database: " << db_path << "\n";

    if (!manifest_path.empty()) {
        std::cout << "[Manifest] Importing manifest from " << manifest_path << "...\n";
        auto [created, updated] = db.import_manifest_file(manifest_path, publish_manifest);
        std::cout << "[Manifest] Import completed: " << created << " created, " << updated << " updated.\n";
    }

    // Seed default catalog only if explicitly requested
    if (seed_defaults) {
        db.seed_default_catalog_if_empty();
    }

    std::string media_dir = find_media_directory(media_path);
    std::cout << "[Media] Serving /media/ from: " << media_dir << "\n";

    g_server = std::make_unique<httplib::Server>();
    auto& svr = *g_server;

    // Support large 3D models (up to 200MB)
    svr.set_payload_max_length(200 * 1024 * 1024);

    // CORS & Common Headers
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Range"},
        {"Access-Control-Expose-Headers", "Content-Range, Content-Length, Accept-Ranges"}
    });

    // Handle preflight OPTIONS requests for any path
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Mount static media directory for .glb 3D models, thumbnails, etc.
    svr.set_mount_point("/media", media_dir);

    // Logging & error handling middleware
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << "[" << kraft::current_iso_time() << "] "
                  << req.method << " " << req.path;
        if (!req.params.empty()) {
            std::cout << "?";
            bool first = true;
            for (const auto& [k, v] : req.params) {
                if (!first) std::cout << "&";
                std::cout << k << "=" << v;
                first = false;
            }
        }
        std::cout << " -> " << res.status << "\n";
    });

    svr.set_exception_handler([](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            std::cerr << "[Exception] Error processing " << req.path << ": " << e.what() << "\n";
            res.status = 500;
            res.set_content(nlohmann::json({{"error", "Internal server error"}, {"detail", e.what()}}).dump(), "application/json");
        }
    });

    // 1. GET /api/vehicles/ - List published vehicles (optional filter ?vehicle_type=car)
    auto handle_list_vehicles = [&db](const httplib::Request& req, httplib::Response& res) {
        std::string vehicle_type = req.get_param_value("vehicle_type");
        auto vehicles = db.get_vehicles(vehicle_type, true);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& v : vehicles) {
            arr.push_back(v.to_list_json());
        }
        res.set_content(arr.dump(2), "application/json");
    };
    svr.Get("/api/vehicles/", handle_list_vehicles);
    svr.Get("/api/vehicles", handle_list_vehicles);

    // 2. GET /api/vehicles/:slug/ - Retrieve vehicle detail with option categories and choices
    svr.Get(R"(/api/vehicles/([a-zA-Z0-9_-]+)/?)", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string slug = req.matches[1];
        auto result = db.get_vehicle_by_slug(slug);
        if (!result) {
            res.status = 404;
            res.set_content(nlohmann::json({{"detail", "Not found."}}).dump(), "application/json");
            return;
        }

        const auto& [vehicle, categories] = *result;
        res.set_content(vehicle.to_detail_json(categories).dump(2), "application/json");
    });

    // 2b. POST /api/vehicles/upload - Upload and save 3D model (.glb, .gltf, .obj) to backend
    auto handle_upload_model = [&db, &media_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            if (!req.is_multipart_form_data()) {
                res.status = 400;
                res.set_content(nlohmann::json({
                    {"error", "Request must be multipart/form-data"}
                }).dump(2), "application/json");
                return;
            }

            httplib::FormData file_data;
            bool found_file = false;

            if (req.form.has_file("file")) {
                file_data = req.form.get_file("file");
                found_file = true;
            } else if (req.form.has_file("model")) {
                file_data = req.form.get_file("model");
                found_file = true;
            } else if (!req.form.files.empty()) {
                file_data = req.form.files.begin()->second;
                found_file = true;
            }

            if (!found_file || file_data.content.empty()) {
                res.status = 400;
                res.set_content(nlohmann::json({
                    {"error", "No 3D model file provided or file is empty"}
                }).dump(2), "application/json");
                return;
            }

            std::string raw_name = file_data.filename;
            if (raw_name.empty()) raw_name = "model.glb";

            fs::path p(raw_name);
            std::string ext = p.extension().string();
            std::string stem = p.stem().string();
            if (ext.empty()) ext = ".glb";

            std::string clean_stem;
            for (char c : stem) {
                if (std::isalnum(static_cast<unsigned char>(c))) clean_stem += static_cast<char>(std::tolower(c));
                else if (c == ' ' || c == '-' || c == '_') clean_stem += '_';
            }
            if (clean_stem.empty()) clean_stem = "vehicle_model";

            auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            std::string final_filename = clean_stem + "_" + timestamp + ext;

            fs::path models_dir = fs::path(media_dir) / "models";
            fs::create_directories(models_dir);

            fs::path target_file_path = models_dir / final_filename;

            std::ofstream out(target_file_path, std::ios::binary);
            if (!out.is_open()) {
                res.status = 500;
                res.set_content(nlohmann::json({
                    {"error", "Failed to write 3D model file to disk on server"},
                    {"path", target_file_path.string()}
                }).dump(2), "application/json");
                return;
            }
            out.write(file_data.content.data(), static_cast<std::streamsize>(file_data.content.size()));
            out.close();

            std::string relative_media_path = "/media/models/" + final_filename;
            std::cout << "[Upload] Saved 3D model (" << file_data.content.size() << " bytes) to: "
                      << target_file_path.string() << " (URI: " << relative_media_path << ")\n";

            int vehicle_id = 0;
            if (req.form.has_field("vehicle_id")) {
                try {
                    vehicle_id = std::stoi(req.form.get_field("vehicle_id"));
                } catch (...) {
                    vehicle_id = 0;
                }
            }

            std::string vehicle_slug = req.form.has_field("vehicle_slug") ? req.form.get_field("vehicle_slug") : "";

            std::optional<kraft::Vehicle> vehicle_out;

            if (vehicle_id > 0) {
                db.update_vehicle_model(vehicle_id, relative_media_path);
                vehicle_out = db.get_vehicle_by_id(vehicle_id);
            } else if (!vehicle_slug.empty()) {
                db.update_vehicle_model_by_slug(vehicle_slug, relative_media_path);
                auto res_v = db.get_vehicle_by_slug(vehicle_slug);
                if (res_v) vehicle_out = res_v->first;
            }

            if (!vehicle_out) {
                std::string name = req.form.has_field("name") ? req.form.get_field("name") : clean_stem;
                std::string type = req.form.has_field("vehicle_type") ? req.form.get_field("vehicle_type") : "car";
                std::string tagline = req.form.has_field("tagline") ? req.form.get_field("tagline") : "Custom imported 3D vehicle model";
                std::string description = req.form.has_field("description") ? req.form.get_field("description") : "Imported into Kraft Configurator.";
                double base_price = 75000.0;
                if (req.form.has_field("base_price")) {
                    try {
                        base_price = std::stod(req.form.get_field("base_price"));
                    } catch (...) {
                        base_price = 75000.0;
                    }
                }

                std::string slug = req.form.has_field("slug") ? req.form.get_field("slug") : "";
                if (slug.empty()) {
                    for (char c : name) {
                        if (std::isalnum(static_cast<unsigned char>(c))) slug += static_cast<char>(std::tolower(c));
                        else if (c == ' ' || c == '-' || c == '_') slug += '-';
                    }
                    if (slug.empty()) slug = "vehicle-" + timestamp;
                    else slug += "-" + (timestamp.size() > 4 ? timestamp.substr(timestamp.size() - 4) : timestamp);
                }

                vehicle_out = db.create_vehicle(name, slug, type, tagline, description, base_price, relative_media_path, true);
                if (vehicle_out) {
                    int cat_paint = db.add_option_category(vehicle_out->id, "Exterior Finish", "exterior-finish", "single", 1);
                    if (cat_paint > 0) {
                        db.add_option_choice(cat_paint, "Studio Gloss", 0.0, "#00d2ff", "Body_Paint", true, 1);
                        db.add_option_choice(cat_paint, "Stealth Matte", 1200.0, "#13171f", "Body_Paint", false, 2);
                        db.add_option_choice(cat_paint, "Silver Chrome", 1800.0, "#e2e8f0", "Body_Paint", false, 3);
                    }
                    int cat_trim = db.add_option_category(vehicle_out->id, "Aerodynamics & Trim", "aerodynamics-trim", "single", 2);
                    if (cat_trim > 0) {
                        db.add_option_choice(cat_trim, "Aero Stock", 0.0, "", "Aero_Stock", true, 1);
                        db.add_option_choice(cat_trim, "Carbon Package", 3400.0, "#1e2634", "Aero_Carbon", false, 2);
                    }
                }
            }

            nlohmann::json resp = {
                {"status", "success"},
                {"message", "3D model uploaded and saved successfully."},
                {"file_path", relative_media_path},
                {"filename", final_filename},
                {"size_bytes", file_data.content.size()}
            };

            if (vehicle_out) {
                auto v_detail = db.get_vehicle_by_slug(vehicle_out->slug);
                if (v_detail) {
                    resp["vehicle"] = v_detail->first.to_detail_json(v_detail->second);
                } else {
                    resp["vehicle"] = vehicle_out->to_list_json();
                }
            }

            res.status = 201;
            res.set_content(resp.dump(2), "application/json");
        } catch (const std::exception& ex) {
            std::cerr << "[Exception] Upload handler error: " << ex.what() << "\n";
            res.status = 500;
            res.set_content(nlohmann::json({
                {"error", "Internal server error during upload"},
                {"detail", ex.what()}
            }).dump(2), "application/json");
        }
    };

    svr.Post("/api/vehicles/upload", handle_upload_model);
    svr.Post("/api/upload", handle_upload_model);

    // 2c. GET /api/models - Read/list all uploaded 3D models with metadata and linked vehicle
    auto handle_list_models = [&db, &media_dir](const httplib::Request&, httplib::Response& res) {
        fs::path models_dir = fs::path(media_dir) / "models";
        nlohmann::json arr = nlohmann::json::array();

        auto all_vehicles = db.get_vehicles("", false);

        if (fs::exists(models_dir) && fs::is_directory(models_dir)) {
            for (const auto& entry : fs::directory_iterator(models_dir)) {
                if (entry.is_regular_file()) {
                    std::string fname = entry.path().filename().string();
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx") {
                        uintmax_t size_bytes = 0;
                        try { size_bytes = entry.file_size(); } catch (...) {}

                        std::string rel_path = "/media/models/" + fname;

                        // Check if assigned to any vehicle
                        nlohmann::json assigned_vehicle = nullptr;
                        for (const auto& v : all_vehicles) {
                            if (v.model_file.has_value() && 
                               (v.model_file.value() == rel_path || v.model_file.value().find(fname) != std::string::npos)) {
                                assigned_vehicle = {
                                    {"id", v.id},
                                    {"name", v.name},
                                    {"slug", v.slug},
                                    {"vehicle_type", v.vehicle_type},
                                    {"base_price", kraft::format_price(v.base_price)}
                                };
                                break;
                            }
                        }

                        arr.push_back({
                            {"filename", fname},
                            {"file_path", rel_path},
                            {"size_bytes", size_bytes},
                            {"extension", ext},
                            {"vehicle", assigned_vehicle}
                        });
                    }
                }
            }
        }
        res.set_content(arr.dump(2), "application/json");
    };
    svr.Get("/api/models", handle_list_models);
    svr.Get("/api/models/", handle_list_models);

    // 2d. DELETE /api/models/:filename - Delete 3D model from disk & detach from vehicles
    svr.Delete(R"(/api/models/([a-zA-Z0-9_.-]+)/?)", [&db, &media_dir](const httplib::Request& req, httplib::Response& res) {
        std::string filename = req.matches[1];
        fs::path target = fs::path(media_dir) / "models" / filename;

        bool removed = false;
        if (fs::exists(target)) {
            try {
                removed = fs::remove(target);
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(nlohmann::json({
                    {"error", "Failed to delete file from disk"},
                    {"detail", e.what()}
                }).dump(2), "application/json");
                return;
            }
        }

        db.clear_vehicle_model_by_file(filename);

        res.set_content(nlohmann::json({
            {"status", "success"},
            {"message", "3D model deleted successfully"},
            {"filename", filename},
            {"disk_file_removed", removed}
        }).dump(2), "application/json");
    });

    // 2e. PATCH/PUT /api/vehicles/:id - Update vehicle details or model
    auto handle_update_vehicle = [&db](const httplib::Request& req, httplib::Response& res) {
        int id = 0;
        try { id = std::stoi(req.matches[1]); } catch (...) { id = 0; }
        if (id <= 0) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid vehicle id"}}).dump(), "application/json");
            return;
        }

        auto v_opt = db.get_vehicle_by_id(id);
        if (!v_opt) {
            res.status = 404;
            res.set_content(nlohmann::json({{"error", "Vehicle not found"}}).dump(), "application/json");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            std::string name = body.value("name", v_opt->name);
            std::string type = body.value("vehicle_type", v_opt->vehicle_type);
            std::string tagline = body.value("tagline", v_opt->tagline);
            std::string desc = body.value("description", v_opt->description);
            double base_price = v_opt->base_price;
            if (body.contains("base_price")) {
                if (body["base_price"].is_number()) {
                    base_price = body["base_price"].get<double>();
                } else if (body["base_price"].is_string()) {
                    base_price = std::stod(body["base_price"].get<std::string>());
                }
            }

            db.update_vehicle_details(id, name, type, tagline, desc, base_price);

            if (body.contains("model_file")) {
                std::string mf = body["model_file"].is_null() ? "" : body["model_file"].get<std::string>();
                db.update_vehicle_model(id, mf);
            }

            auto updated = db.get_vehicle_by_id(id);
            res.set_content(updated ? updated->to_list_json().dump(2) : "{}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid JSON"}, {"detail", e.what()}}).dump(), "application/json");
        }
    };
    svr.Patch(R"(/api/vehicles/([0-9]+)/?)", handle_update_vehicle);
    svr.Put(R"(/api/vehicles/([0-9]+)/?)", handle_update_vehicle);

    // 2e2. POST /api/vehicles - Create new vehicle entry (for assigning unassigned 3D models)
    auto handle_create_vehicle = [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string name = body.value("name", "New Vehicle");
            std::string type = body.value("vehicle_type", "car");
            std::string tagline = body.value("tagline", "Custom vehicle platform");
            std::string desc = body.value("description", "Imported into Kraft Configurator.");
            double base_price = 75000.0;
            if (body.contains("base_price")) {
                if (body["base_price"].is_number()) {
                    base_price = body["base_price"].get<double>();
                } else if (body["base_price"].is_string()) {
                    base_price = std::stod(body["base_price"].get<std::string>());
                }
            }
            std::string model_file = body.value("model_file", "");

            std::string slug = body.value("slug", "");
            if (slug.empty()) {
                for (char c : name) {
                    if (std::isalnum(static_cast<unsigned char>(c))) slug += static_cast<char>(std::tolower(c));
                    else if (c == ' ' || c == '-' || c == '_') slug += '-';
                }
                auto timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                if (slug.empty()) slug = "vehicle-" + timestamp;
                else slug += "-" + (timestamp.size() > 4 ? timestamp.substr(timestamp.size() - 4) : timestamp);
            }

            auto created = db.create_vehicle(name, slug, type, tagline, desc, base_price, model_file, true);
            if (created) {
                int cat_paint = db.add_option_category(created->id, "Exterior Finish", "exterior-finish", "single", 1);
                if (cat_paint > 0) {
                    db.add_option_choice(cat_paint, "Studio Gloss", 0.0, "#00d2ff", "Body_Paint", true, 1);
                    db.add_option_choice(cat_paint, "Stealth Matte", 1200.0, "#13171f", "Body_Paint", false, 2);
                    db.add_option_choice(cat_paint, "Silver Chrome", 1800.0, "#e2e8f0", "Body_Paint", false, 3);
                }
                int cat_trim = db.add_option_category(created->id, "Aerodynamics & Trim", "aerodynamics-trim", "single", 2);
                if (cat_trim > 0) {
                    db.add_option_choice(cat_trim, "Aero Stock", 0.0, "", "Aero_Stock", true, 1);
                    db.add_option_choice(cat_trim, "Carbon Package", 3400.0, "#1e2634", "Aero_Carbon", false, 2);
                }

                res.status = 201;
                res.set_content(created->to_list_json().dump(2), "application/json");
            } else {
                res.status = 500;
                res.set_content(nlohmann::json({{"error", "Failed to create vehicle in SQLite"}}).dump(2), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid JSON"}, {"detail", e.what()}}).dump(), "application/json");
        }
    };
    svr.Post("/api/vehicles/?", handle_create_vehicle);

    // 2f. DELETE /api/vehicles/:id - Delete vehicle and optionally delete model file
    svr.Delete(R"(/api/vehicles/([0-9]+)/?)", [&db, &media_dir](const httplib::Request& req, httplib::Response& res) {
        int id = 0;
        try { id = std::stoi(req.matches[1]); } catch (...) { id = 0; }
        if (id <= 0) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid vehicle id"}}).dump(), "application/json");
            return;
        }

        auto v_opt = db.get_vehicle_by_id(id);
        if (!v_opt) {
            res.status = 404;
            res.set_content(nlohmann::json({{"error", "Vehicle not found"}}).dump(), "application/json");
            return;
        }

        std::string del_file = req.get_param_value("delete_file");
        if ((del_file == "1" || del_file == "true") && v_opt->model_file.has_value()) {
            fs::path p(v_opt->model_file.value());
            std::string fname = p.filename().string();
            fs::path target = fs::path(media_dir) / "models" / fname;
            if (fs::exists(target)) {
                try { fs::remove(target); } catch (...) {}
            }
        }

        bool ok = db.delete_vehicle(id);
        if (ok) {
            res.set_content(nlohmann::json({
                {"status", "success"},
                {"message", "Vehicle deleted successfully"}
            }).dump(2), "application/json");
        } else {
            res.status = 500;
            res.set_content(nlohmann::json({{"error", "Failed to delete vehicle"}}).dump(), "application/json");
        }
    });

    // 3. GET /api/configurations/ - List configurations (optional ?session_key=...)
    auto handle_list_configurations = [&db](const httplib::Request& req, httplib::Response& res) {
        std::string session_key = req.get_param_value("session_key");
        auto list = db.get_configurations(session_key);

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : list) {
            arr.push_back(c.to_json());
        }
        res.set_content(arr.dump(2), "application/json");
    };
    svr.Get("/api/configurations/", handle_list_configurations);
    svr.Get("/api/configurations", handle_list_configurations);

    // 4. POST /api/configurations/ - Create new configuration
    auto handle_create_configuration = [&db](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("vehicle")) {
                res.status = 400;
                res.set_content(nlohmann::json({{"error", "Field 'vehicle' is required"}}).dump(), "application/json");
                return;
            }

            int vehicle_id = body["vehicle"].get<int>();
            std::string session_key = body.value("session_key", "");
            std::string name = body.value("name", "");

            std::vector<int> option_ids;
            if (body.contains("option_ids") && body["option_ids"].is_array()) {
                for (const auto& id_val : body["option_ids"]) {
                    option_ids.push_back(id_val.get<int>());
                }
            }

            auto cfg = db.create_configuration(vehicle_id, option_ids, session_key, name);
            if (!cfg) {
                res.status = 500;
                res.set_content(nlohmann::json({{"error", "Failed to create configuration"}}).dump(), "application/json");
                return;
            }

            res.status = 201;
            res.set_content(cfg->to_json().dump(2), "application/json");
        } catch (const std::exception& ex) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid JSON body"}, {"detail", ex.what()}}).dump(), "application/json");
        }
    };
    svr.Post("/api/configurations/", handle_create_configuration);
    svr.Post("/api/configurations", handle_create_configuration);

    // 5. GET /api/configurations/:id/ - Retrieve single configuration
    svr.Get(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        auto cfg = db.get_configuration(id);
        if (!cfg) {
            res.status = 404;
            res.set_content(nlohmann::json({{"detail", "Not found."}}).dump(), "application/json");
            return;
        }
        res.set_content(cfg->to_json().dump(2), "application/json");
    });

    // 6. PATCH /api/configurations/:id/ - Update configuration options
    auto handle_update_config = [&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        try {
            auto body = nlohmann::json::parse(req.body);
            std::vector<int> option_ids;
            if (body.contains("option_ids") && body["option_ids"].is_array()) {
                for (const auto& id_val : body["option_ids"]) {
                    option_ids.push_back(id_val.get<int>());
                }
            }

            auto cfg = db.update_configuration(id, option_ids);
            if (!cfg) {
                res.status = 404;
                res.set_content(nlohmann::json({{"detail", "Configuration not found."}}).dump(), "application/json");
                return;
            }

            res.set_content(cfg->to_json().dump(2), "application/json");
        } catch (const std::exception& ex) {
            res.status = 400;
            res.set_content(nlohmann::json({{"error", "Invalid JSON body"}, {"detail", ex.what()}}).dump(), "application/json");
        }
    };
    svr.Patch(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", handle_update_config);
    svr.Put(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", handle_update_config);

    // 7. DELETE /api/configurations/:id/ - Delete configuration
    svr.Delete(R"(/api/configurations/([a-zA-Z0-9_-]+)/?)", [&db](const httplib::Request& req, httplib::Response& res) {
        std::string id = req.matches[1];
        bool deleted = db.delete_configuration(id);
        if (deleted) {
            res.status = 204;
        } else {
            res.status = 404;
            res.set_content(nlohmann::json({{"detail", "Not found."}}).dump(), "application/json");
        }
    });

    // 8. GET /api/health - Telemetry and engine status
    svr.Get("/api/health", [&db](const httplib::Request&, httplib::Response& res) {
        auto vehicles = db.get_vehicles("", false);
        res.set_content(nlohmann::json({
            {"service", "Kraft Configurator C++ Engine"},
            {"status", "online"},
            {"version", "2.0.0"},
            {"engine", "C++17 / SQLite / cpp-httplib"},
            {"vehicles_count", vehicles.size()},
            {"timestamp", kraft::current_iso_time()}
        }).dump(2), "application/json");
    });

    // Mount UI distribution directory if present, else provide fallback root probe
    std::string ui_dir = find_ui_directory(ui_path);
    if (!ui_dir.empty()) {
        std::cout << "[UI] Serving frontend SPA from: " << ui_dir << "\n";
        svr.set_mount_point("/", ui_dir);
    } else {
        std::cout << "[UI] Built UI not found. Fallback API root probe active.\n";
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(nlohmann::json({
                {"service", "Kraft Configurator C++ Engine"},
                {"status", "online"},
                {"endpoints", {
                    "/api/health",
                    "/api/vehicles/",
                    "/api/vehicles/:slug/",
                    "/api/configurations/",
                    "/media/"
                }}
            }).dump(2), "application/json");
        });
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "[Server] Listening on http://" << (host == "0.0.0.0" ? "localhost" : host) << ":" << port << "\n";
    std::cout << "[Server] Press Ctrl+C to stop.\n";

    if (!svr.listen(host, port)) {
        std::cerr << "[Error] Failed to start HTTP server on " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "[Server] Stopped.\n";
    return 0;
}
