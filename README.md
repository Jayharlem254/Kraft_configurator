# Kraft Configurator

A real-time, hardware-grade 3D vehicle configurator for high-performance automobiles, offshore powerboats, and super-naked motorbikes. Kraft Configurator allows users to inspect 3D models in an interactive studio, customize exterior colors, materials, wheels, and aerodynamic trims, upload custom 3D vehicle assets, and view live pricing and hardware telemetry updates in real time.

Re-engineered following the architecture, mechanisms, and tactile hardware design language of **Quni4m (HQC)**:
- **High-Performance C++ Engine**: Native C++17 server (`cpp-httplib` + embedded SQLite 3) delivering sub-millisecond latency, zero-copy static file streaming, high-throughput multipart 3D model uploads, and static Single Page Application (SPA) hosting.
- **Frontend 3D Studio**: Built with Svelte, Vite, and Three.js / Threlte featuring a **Crystal Blue** tactile industrial aesthetic, machined aluminum chassis, rotary encoders, slide faders, and live hardware telemetry.
- **Integrated 3D Model Uploader**: Direct in-browser dropzone for streaming `.glb`, `.gltf`, and `.obj` vehicle models into the C++ backend repository with automatic catalog registration and SQLite synchronization.
- **Zero-Friction Launch**: Single-command startup via `run.bat`.

---

## Table of Contents

- [System Architecture](#system-architecture)
- [Project Directory Structure](#project-directory-structure)
- [Quick Start Guide](#quick-start-guide)
- [3D Model Upload Engine](#3d-model-upload-engine)
- [REST API Reference](#rest-api-reference)
- [Design System & Hardware Controls](#design-system--hardware-controls)
- [Asset Pipeline](#asset-pipeline)
- [Building & Compiling](#building--compiling)
- [Technology Stack](#technology-stack)

---

## System Architecture

```
                    ┌────────────────────────────────────────────────────────┐
                    │                    WEB CLIENT / SPA                    │
                    │         Svelte + Vite + Three.js / Threlte             │
                    │   Tactile UI • 3D Viewport • Uploader • Telemetry      │
                    └───────────────────────────┬────────────────────────────┘
                                                │
                 HTTP REST / Multipart Uploads  │  Static Assets & Models
                                                │
                    ┌───────────────────────────▼────────────────────────────┐
                    │               KRAFT C++ ENGINE (Native)                │
                    │               cpp-httplib Server (Port 8000)           │
                    │                                                        │
                    │  ┌────────────────────┐     ┌───────────────────────┐  │
                    │  │   REST Handlers    │     │ Static Media Handler  │  │
                    │  │  - Health Telemetry│     │ Zero-copy stream      │  │
                    │  │  - Fleet Catalog   │     │ Range / ETag caching  │  │
                    │  │  - Model Uploader  │     │ /media/models/*.glb   │  │
                    │  │  - Configurations  │     └───────────┬───────────┘  │
                    │  └─────────┬──────────┘                 │              │
                    │            │ Write / Read               │ Read File    │
                    │            ▼                            ▼              │
                    │  ┌────────────────────┐     ┌───────────────────────┐  │
                    │  │ SQLite 3 (WAL Mode)│     │  Filesystem Storage   │  │
                    │  │ kraft_configurator │     │  /media/models/       │  │
                    │  │ .db                │     │  /media/thumbnails/   │  │
                    │  └────────────────────┘     └───────────────────────┘  │
                    └────────────────────────────────────────────────────────┘
```

---

## Project Directory Structure

```
Kraft_configurator/
├── run.bat                          # Single-command launcher (builds & starts C++ engine + browser)
├── package.json                     # Monorepo scripts orchestrator
├── README.md                        # Master project documentation
│
└── Kraft_configurator/
    ├── backend-cpp/                 # High-performance C and C++ Engine
    │   ├── src/
    │   │   ├── server.cpp           # Multi-threaded HTTP server, routing, upload handler
    │   │   ├── db.cpp & db.hpp      # SQLite3 data access layer, migrations & schema
    │   │   ├── models.hpp           # Data structures (Vehicle, OptionCategory, Configuration)
    │   │   └── sqlite3.c            # SQLite 3.45 C amalgamation engine
    │   ├── include/
    │   │   ├── httplib.h            # Single-header C++ HTTP/HTTPS server (cpp-httplib)
    │   │   ├── json.hpp             # Modern C++ JSON library (nlohmann/json)
    │   │   └── sqlite3.h            # SQLite C API header
    │   ├── bin/
    │   │   └── kraft_backend.exe    # Compiled standalone C++ binary
    │   ├── build.bat                # Windows automated compiler script (Clang / MSVC)
    │   ├── CMakeLists.txt           # Cross-platform CMake build configuration
    │   └── README.md                # C++ backend specific documentation
    │
    ├── frontend/                    # Svelte + Vite + Three.js Client UI
    │   ├── src/
    │   │   ├── App.svelte           # Root application shell (Cockpit, TopBar, StatusBar)
    │   │   ├── app.css              # Quni4m Crystal Blue variables & tactile light physics
    │   │   ├── main.js              # Application mount point
    │   │   └── lib/
    │   │       ├── api.js           # REST API client & upload helper
    │   │       ├── selections.js    # Configuration pricing & selection engine
    │   │       ├── mockData.js      # Embedded offline vehicle catalog preview
    │   │       └── components/
    │   │           ├── TopBar.svelte             # Machined header with LED telemetry & mode switcher
    │   │           ├── StatusBar.svelte          # Hardware bottom bar with price counter & status
    │   │           ├── VehicleGallery.svelte     # Fleet catalog with interactive cards & upload card
    │   │           ├── VehicleOverview.svelte    # Technical vehicle spec deck & blueprint
    │   │           ├── Viewer3D.svelte           # 3D Studio canvas shell, HUD chrome & tools
    │   │           ├── Viewer3DScene.svelte      # Three.js / Threlte WebGL scene & material binder
    │   │           ├── ConfiguratorPanel.svelte  # Option category racks & finishes
    │   │           ├── ModelUploader.svelte      # Dedicated 3D vehicle model upload studio
    │   │           ├── ModelInspector.svelte     # Dev/QA mesh & material channel inspector
    │   │           └── controls/                 # Hardware control components
    │   │               ├── Knob.svelte           # Tactile rotary encoder with crystal light ring
    │   │               ├── Fader.svelte          # Recessed linear slider
    │   │               └── ModeSwitch.svelte     # Machined segmented selector
    │   ├── dist/                    # Compiled static production bundle
    │   ├── package.json             # Frontend NPM scripts & dependencies
    │   └── vite.config.js           # Vite bundler configuration & /api proxy
    │
    ├── backend/                     # Reference Python/Django service (preserved for parity/media)
    │   ├── media/                   # Media repository
    │   │   ├── models/              # Stored 3D vehicle models (.glb)
    │   │   └── thumbnails/          # Vehicle thumbnails and previews
    │   └── manage.py
    │
    └── asset-pipeline/              # Java glTF asset optimization & validation toolchain
        ├── pom.xml                  # Maven build descriptor
        ├── src/                     # Asset inspection, draco compression & validation
        └── scripts/                 # Ingestion & conversion scripts
```

---

## Quick Start Guide

### Option 1: One-Click Launch (Recommended)
Double-click **`run.bat`** in the project root, or execute from PowerShell / Terminal:
```cmd
.\run.bat
```

**What this does automatically:**
1. Detects `kraft_backend.exe`. If missing, compiles it automatically with Clang or MSVC.
2. Checks for `frontend/dist`. If missing, installs npm packages and builds the Svelte UI.
3. Launches the native C++ engine on port `8000`.
4. Automatically opens `http://127.0.0.1:8000` in your default web browser.

---

### Option 2: Running Development Mode
Run the C++ backend and the Vite development server with hot-module replacement (HMR):

1. **Start the C++ Backend Engine:**
   ```cmd
   cd Kraft_configurator\backend-cpp
   .\bin\kraft_backend.exe --port 8000
   ```

2. **In a second terminal, start the Vite Dev Server:**
   ```bash
   cd Kraft_configurator\frontend
   npm install
   npm run dev
   ```
   *Vite proxies `/api` and `/media` requests directly to `http://127.0.0.1:8000`.*

---

## 3D Model Upload Engine

Kraft Configurator features an integrated 3D model ingestion workflow directly connected to the native C++ backend.

### Upload Features
- **File Format Support**: Accepts `.glb` (glTF 2.0 Binary, recommended), `.gltf`, and `.obj` files up to **200 MB**.
- **Two Ingestion Modes**:
  1. **New Vehicle Entry**: Uploads the 3D file and creates a new catalog vehicle with name, category (Car, Boat, Bike), tagline, description, base price, and automatically generated option categories.
  2. **Attach to Existing Vehicle**: Select any registered vehicle from the fleet catalog and replace/assign its 3D model.
- **Backend File Management**:
  - Sanitizes model filenames and adds unique timestamps to prevent collisions.
  - Saves models directly to `/media/models/<filename>`.
  - Instantly updates SQLite records and serves the model with byte-range support.
- **One-Click Viewing**: Immediately upon successful upload, click **LAUNCH IN 3D STUDIO** to jump straight into configuring the vehicle in real-time WebGL.

### Accessing the Uploader in the UI
- **From TopBar**: Click the **`UPLOAD 3D`** segment in the main navigation switch.
- **From Fleet Catalog**: Click the **`IMPORT 3D MODEL`** card at the end of the vehicle catalog grid.

---

## REST API Reference

The C++ backend provides 100% DRF-compatible REST endpoints:

### 1. 3D Model Upload
- **`POST /api/vehicles/upload`** (Alias: `/api/upload`)
  - **Content-Type**: `multipart/form-data`
  - **Form Fields**:
    - `file` *(Required)*: Binary 3D model file (`.glb`, `.gltf`, `.obj`).
    - `name` *(Optional)*: Vehicle name (e.g. `"Apex GT Prototype"`).
    - `vehicle_id` *(Optional)*: ID of an existing vehicle to update.
    - `vehicle_slug` *(Optional)*: Slug of an existing vehicle to update.
    - `vehicle_type` *(Optional)*: `"car"` | `"boat"` | `"bike"` (default: `"car"`).
    - `base_price` *(Optional)*: Base price in USD (e.g. `125000`).
    - `tagline` *(Optional)*: Short tagline description.
    - `description` *(Optional)*: Detailed technical description.
  - **Sample Response (`201 Created`)**:
    ```json
    {
      "status": "success",
      "message": "3D model uploaded and saved successfully.",
      "file_path": "/media/models/apex_gt_1790501813.glb",
      "filename": "apex_gt_1790501813.glb",
      "size_bytes": 18390888,
      "vehicle": {
        "id": 5,
        "name": "Apex GT Prototype",
        "slug": "apex-gt-prototype-1813",
        "vehicle_type": "car",
        "base_price": "125000.00",
        "model_file": "/media/models/apex_gt_1790501813.glb",
        "option_categories": [...]
      }
    }
    ```

### 2. Vehicle Catalog
- **`GET /api/vehicles/`**
  - Query parameters: `?vehicle_type=car|boat|bike`
  - Returns a list of published vehicles with base prices, thumbnails, and model file paths.
- **`GET /api/vehicles/:slug/`**
  - Returns complete vehicle detail including nested `option_categories` and `choices` (swatches, price deltas, mesh targets).

### 3. Build Configurations
- **`GET /api/configurations/`**
  - Query parameter: `?session_key=<key>`
  - Retrieves saved configuration builds.
- **`POST /api/configurations/`**
  - Body: `{"vehicle": 1, "option_ids": [2, 7, 14], "session_key": "..."}`
  - Calculates total price and stores configuration selections.
- **`GET /api/configurations/:id/`**
  - Retrieves an existing configuration by UUID.
- **`PATCH /api/configurations/:id/`**
  - Updates selected options for a configuration.
- **`DELETE /api/configurations/:id/`**
  - Removes a saved configuration (`204 No Content`).

### 4. Telemetry & Media
- **`GET /api/health`**
  - Returns C++ engine status, vehicle count, uptime, and engine version.
- **`GET /media/*`**
  - Static file streaming endpoint for 3D model geometry (`.glb`), thumbnails, and textures with `Accept-Ranges`, `ETag`, and `CORS` headers enabled.

---

## Design System & Hardware Controls

The UI was crafted from scratch based on the **Quni4m (HQC)** tactile design language:
- **Chassis & Surfaces**: Deep machined bead-blasted aluminum shades (`#0c0f13`, `#13171f`, `#1e2634`).
- **Tactile Light Physics**:
  - Convex / raised surfaces (`.raised`) with crisp specular highlights.
  - Recessed / milled sockets (`.milled`) with internal occlusion shadows.
- **Crystal Blue Signature**:
  - Primary accent `#00d2ff`, bright neon `#70e1ff`, deep sapphire `#0077b6`.
  - Ambient luminous bloom on active controls and status LEDs.
- **Hardware Controls**:
  - **`Knob.svelte`**: Smooth rotary encoder with radial light arc and value readout.
  - **`Fader.svelte`**: Recessed linear slide fader for continuous adjustment.
  - **`ModeSwitch.svelte`**: Machined segment rocker for cockpit view transitions.
  - **`StatusBar.svelte`**: Continuous live price accumulator and C++ engine heartbeat indicator.

---

## Asset Pipeline

The `asset-pipeline/` directory contains a dedicated Java utility designed for 3D vehicle asset processing:
- **Mesh Validation**: Inspects glTF/GLB vertex counts, submesh hierarchy, and material channels.
- **Draco Compression**: Optimizes 3D asset file size for sub-second web delivery.
- **Manifest Generator**: Generates `manifest.json` catalog index, which can be directly imported into the C++ engine via `--import-manifest <path> --publish`.

---

## Building & Compiling

### Compiling C++ Backend Manually
Requirements: Clang++ (via LLVM / MinGW-w64) or MSVC (`cl.exe`).

Using the build script:
```cmd
cd Kraft_configurator\backend-cpp
build.bat
```

Or using Clang manually:
```cmd
clang++ --target=x86_64-w64-mingw32 -std=c++17 -O2 -static -I include src\server.cpp src\db.cpp bin\sqlite3.o -o bin\kraft_backend.exe -lws2_32
```

### Compiling Frontend UI Manually
Requirements: Node.js 18+ and npm.
```bash
cd Kraft_configurator\frontend
npm install
npm run build
```
This generates the optimized production bundle in `Kraft_configurator/frontend/dist/`.

---

## Technology Stack

| Layer | Technologies |
|---|---|
| **C++ Engine** | C++17, `cpp-httplib`, `nlohmann/json`, SQLite 3 (WAL mode), WinSock2 |
| **Frontend** | Svelte 4, Vite 5, Three.js 0.160+, `@threlte/core`, `@threlte/extras` |
| **3D Rendering** | WebGL 2, PBR Materials, RoomEnvironment IBL, ACES Filmic Tone Mapping |
| **Asset Pipeline**| Java 17+, Maven, glTF-Transform |
| **Styling** | Vanilla CSS with custom hardware design tokens & CSS variables |

---

## License

Crafted for Kraft Configurator. All rights reserved.
