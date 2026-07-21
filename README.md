# Kraft Configurator

A 3D configurator for cars, speed boats, and bikes — pick a vehicle, customize colors,
wheels/finishes, and trim options, and see a live price update next to a real-time 3D
preview. Inspired by the Porsche configurator (https://models.porsche.com), with a
custom dark/purple-metallic, SpaceX-telemetry-inspired visual identity.

## Monorepo layout

```
kraft-configurator/
├── backend/          Django + DRF — vehicle catalog, options, pricing, saved configs
├── frontend/          Svelte + Vite + Threlte (Three.js) — the configurator UI & 3D viewer
└── asset-pipeline/    Java — turns raw Sketchfab downloads into optimized, catalog-ready glTF
```

These three pieces are independent and talk to each other over HTTP + the filesystem:

1. You drop a raw Sketchfab download (`.zip`/`.fbx`/`.obj` + textures) into
   `asset-pipeline/raw-assets/`.
2. The Java pipeline inspects it, writes an optimized `.glb` + a `manifest.json` entry
   (name, poly count, materials, suggested category) into a shared `media/models/` folder.
3. You import that manifest into Django (a management command, see backend README) to
   create/update a `Vehicle` row pointing at the `.glb` file.
4. The Svelte frontend calls the Django API for the catalog and renders the chosen
   model with Threlte, letting the user swap colors/options live.

## Quick start

Open three terminals:

```bash
# 1. Backend
cd backend
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
python manage.py migrate
python manage.py createsuperuser
python manage.py runserver        # http://localhost:8000

# 2. Frontend
cd frontend
npm install
npm run dev                        # http://localhost:5173

# 3. Asset pipeline (only when you have new models to process)
cd asset-pipeline
mvn package
java -jar target/asset-pipeline-0.1.0.jar
```

See `backend/README.md`, `frontend/README.md`, and `asset-pipeline/README.md` for
details on each piece.

## Where things stand

This is initial scaffolding, meant as a foundation to build on, not a finished product:

- **Backend**: a working `catalog` app with models for vehicles, option categories/choices,
  and saved configurations, exposed over a DRF API. No auth wiring yet beyond Django's
  defaults — add proper auth/permissions before this is public-facing.
- **Frontend**: a working dev server with the purple-metallic/SpaceX theme tokens in
  place, a placeholder 3D viewer (renders a stand-in mesh — swap in real `.glb` files),
  and a configurator panel wired to the (currently mocked) API client.
- **Asset pipeline**: a Java skeleton that defines the manifest format and folder
  scanning logic. The actual mesh conversion/optimization step is stubbed — see the
  TODO in `AssetProcessor.java` — since that depends on which conversion tool you
  install locally (Blender CLI, gltf-pipeline, etc.) and this sandbox can't reach
  Maven Central to resolve a 3D library for you to test against.

## Licensing reminder

Check each Sketchfab model's license before shipping it in a real product — many
Creative Commons licenses require attribution, and some forbid commercial use.
# Kraft_configurator
