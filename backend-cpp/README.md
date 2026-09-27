# Kraft Configurator — High-Performance C++ Backend

A drop-in, high-speed C++17 replacement for the Django + DRF backend. It provides full API parity with the Django service, built-in SQLite persistence, static media serving for `.glb` 3D models and thumbnails, asset pipeline manifest importing, and auto-seeding.

## Architecture

- **HTTP Engine**: `cpp-httplib` (lightweight multi-threaded server with full CORS & static file streaming)
- **JSON Engine**: `nlohmann/json` (modern, standard C++ JSON serialization)
- **Database**: Embedded SQLite 3 amalgamation in WAL mode with foreign key integrity
- **Port**: Default `8000` (matching the Vite dev proxy)

## API Endpoints (100% DRF Parity)

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/vehicles/` | List published vehicles (optional filter: `?vehicle_type=car\|boat\|bike`) |
| `GET` | `/api/vehicles/:slug/` | Vehicle detail with nested option categories and choices |
| `GET` | `/api/configurations/` | List saved configurations (optional filter: `?session_key=...`) |
| `POST` | `/api/configurations/` | Create a new configuration build (`{"vehicle": id, "option_ids": [...]}`) |
| `GET` | `/api/configurations/:id/` | Retrieve a specific configuration build |
| `PATCH`/`PUT` | `/api/configurations/:id/` | Update selected option choices (`{"option_ids": [...]}`) |
| `DELETE` | `/api/configurations/:id/` | Remove a configuration (returns `204 No Content`) |
| `GET` | `/media/*` | Serves `.glb` models, thumbnails, and textures |

## Quick Start

### 1. Build

Run the included automated build script on Windows:

```cmd
backend-cpp\build.bat
```

Or using CMake:

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 2. Run

```cmd
backend-cpp\bin\kraft_backend.exe --port 8000
```

### 3. Command-Line Options

```
Kraft Configurator C++ Backend
Usage: kraft_backend [options]
Options:
  --port <port>              Port to listen on (default: 8000)
  --host <host>              Host address to bind to (default: 0.0.0.0)
  --db <path>                SQLite database file (default: kraft_configurator.db)
  --media <dir>              Directory to serve /media/ files from
  --import-manifest <path>   Import models from asset-pipeline manifest.json
  --publish                  Publish imported vehicles immediately
  --help, -h                 Show help message
```

### 4. Importing Asset Pipeline Manifests

Matches Django's `manage.py import_manifest`:

```cmd
backend-cpp\bin\kraft_backend.exe --import-manifest path\to\manifest.json --publish
```

### 5. Frontend Compatibility

Because this backend exposes the exact routes, schema, and CORS headers expected by the Svelte frontend (`frontend/src/lib/api.js`), no changes to the frontend are required. Vite's proxy will forward `/api` and `/media` requests directly to this server on port 8000.
