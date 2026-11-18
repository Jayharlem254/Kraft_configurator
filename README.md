# Kraft Configurator

A real-time 3D configurator for **cars, motorcycles and boats in every class**, in the spirit of premium online car configurators. You choose a model and variant, then configure paint, wheels, interior, performance, technology, comfort and packages. A native C++ rules engine validates and prices every change, and the vehicle restyles live in 3D.

```
┌──────────────────────────────── Browser (Svelte 5 + three.js) ────────────────────────────────┐
│ Model range · Configurator · Summary · Garage · Compare · Studio                              │
│                                                                                               │
│ Kraft Stage (3D engine)                    Configurator state                                 │
│  • procedural models for 30+ body classes   • sends {variant, selections, changed}            │
│  • GLB/GLTF (Draco + Meshopt) with automatic • renders the engine's resolved build            │
│    material-role binding                    • undo / redo · shareable URL · Kraft codes       │
│  • render-on-demand, adaptive resolution                                                      │
└────────────────────────────────────────────┬──────────────────────────────────────────────────┘
                                             │ REST / JSON (ETag-cached catalog)
┌────────────────────────────────────────────▼──────────────────────────────────────────────────┐
│ Kraft Configuration Engine (C++17, cpp-httplib, SQLite WAL)                                    │
│  CatalogCache — immutable versioned in-memory snapshot, rebuilt only after writes             │
│  RulesEngine  — availability · single-choice · requires (A|B) · excludes · package includes   │
│                 · conflict resolution by recency · pricing · live technical specs             │
│  SeedLibrary  — data/catalog_seed.json: option templates + full vehicle line-up               │
└───────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Quick start

```cmd
run.bat
```

This builds the engine and the UI if they are missing, starts the engine on port 8000 and opens `http://127.0.0.1:8000`.

Development with hot reload:

```cmd
backend-cpp\bin\kraft_backend.exe --port 8000
cd frontend && npm install && npm run dev      :: http://localhost:3000 (proxies /api and /media)
```

## What you can configure

| Type | Classes (procedural 3D) |
|---|---|
| Cars | coupé, convertible (roof open/closed), supercar, sedan, electric, SUV, hatchback, touring/wagon, pickup, van/MPV, off-road |
| Motorcycles | sport, naked, cruiser, touring, adventure, motocross, scooter, electric, café racer |
| Boats | speedboat/centre console, motor yacht, sailboat, catamaran, pontoon, personal watercraft, fishing, electric |

Each vehicle has **variants** (trims with their own price and specs) and option **sections**:

- **Exterior:** solid, metallic, pearl, matte and chrome paints, plus trim packages, glass, roof (body, glass, contrast, carbon), stripes and aero.
- **Wheels:** wheel designs (spoke count and style) and caliper colours.
- **Interior:** leather and microfibre colours, seats and trims.
- **Performance:** power kits, brakes, suspension, batteries and propulsion.
- **Technology, comfort and packages.**

Options carry **rules**. For example, the Performance Kit *requires* Sport Chrono and the Sport Exhaust. Ceramic brakes *include* yellow calipers. The Lightweight Package *excludes* the glass roof and *includes* bucket seats, which in turn *exclude* seat ventilation. Some options are only available on certain variants. The engine resolves every change, tells the UI what it added or removed and why, and recomputes price and specs (power, 0–100, top speed, weight, range and more).

Configurations can be saved (`My Garage`), reopened by an 8-character **Kraft code** (`#/code/KXXXXXXX`), shared as a URL, printed or saved as a PDF from the summary page, and compared side by side.

## 3D engine (`frontend/src/lib/three`)

| File | Role |
|---|---|
| `Stage.js` | Renderer: render-on-demand loop, adaptive pixel ratio while orbiting, static shadow map, camera tweens, cut-away interior view, environments (studio, gallery, dusk, night), snapshots |
| `materials.js` | Role-based PBR material library (paint, rims, calipers, interior, hull, deck, sail, …) and the finish presets |
| `procedural/car.js`, `bike.js`, `boat.js` | Parametric vehicle builders; structural options (wheel design, spoiler, roof, engines, T-top, luggage, …) rebuild geometry |
| `glb.js` | Uploaded models: Draco/Meshopt decoding, orientation and scale normalisation, automatic role detection by material name |
| `thumbnails.js` | Gallery thumbnails rendered from the same engine through one shared hidden WebGL context, cached in localStorage |

Uploaded GLBs bind roles by material name (`carpaint`, `rim`/`jante`, `caliper`, `seat`, `hull`, …). When nothing matches, the largest light-coloured material becomes the paint. You can override this per vehicle with `material_map` (`PATCH /api/vehicles/:id {"material_map": {"paint": ["MyBodyMat"]}}`).

## Engine (`backend-cpp`)

```
src/models.hpp   domain model (Vehicle, Variant, OptionCategory, OptionChoice, Configuration)
src/db.cpp       SQLite access layer: statement helper, thread-safe, additive migrations
src/engine.cpp   CatalogCache + RulesEngine
src/seed.cpp     catalog_seed.json loader / template expansion / legacy upgrade
src/server.cpp   HTTP API, static UI + media serving, cache headers
data/catalog_seed.json
```

Build: `backend-cpp\build.bat` (Clang or MSVC) or CMake. Options:

```
--port 8000  --db <file>  --media <dir>  --ui <dir>  --seed <file>
--reseed     re-apply the seed catalog     --no-seed   skip it
--threads N  worker threads                 --import-manifest <file> [--publish]
```

The database defaults to `backend-cpp/bin/kraft_configurator.db`. Migrations are additive. Vehicles uploaded before v3 automatically get variants and the full option set for their type.

### REST API

| Method & path | Purpose |
|---|---|
| `GET /api/health` | status, catalog version, rules-engine timings |
| `GET /api/catalog/meta` | types and classes with counts |
| `GET /api/vehicles/?vehicle_type=&body_class=&q=` | model range (ETag) |
| `GET /api/vehicles/:slug/` | full definition: variants + option tree (ETag) |
| `POST /api/vehicles/:slug/evaluate` | `{variant, selections[], changed, deselect}` → `{resolved[], changes[], price{}, specs{}, selections[]}` |
| `POST /api/configurations/` | save (re-evaluated server-side) → returns `code` |
| `GET/PATCH/DELETE /api/configurations/:id-or-code/` | load, update or delete a saved build |
| `POST /api/vehicles/upload` | multipart 3D upload (`file`, `name`, `vehicle_type`, `body_class`, `base_price` or `vehicle_id`) |
| `POST /api/vehicles/`, `PATCH/DELETE /api/vehicles/:id/` | catalog management |
| `GET /api/models`, `DELETE /api/models/:filename` | model files |

## Extending the catalog

Edit `backend-cpp/data/catalog_seed.json` and bump `"version"`; the engine re-imports on its next start, or immediately with `--reseed`. Templates define reusable option categories. Vehicles list the templates they use, plus `drop` (remove options), `availability` (restrict an option to variants), `price_scale`, vehicle-specific `categories` and their `variants`. An option looks like this:

```json
{"code": "PF03", "name": "Performance Kit +45 hp", "price": 12900,
 "requires": ["PF01", "PF02"], "excludes": [], "includes": [],
 "mods": {"power_hp": 45, "accel_0_100_s": -0.2},
 "visual": [{"role": "exhaust", "color": "#121314", "finish": "satin"}, {"part": "exhaust", "value": "quad"}]}
```

`requires` entries may list alternatives as `"A|B"`. Visual `role`s recolour materials, and `part`s change procedural geometry.
