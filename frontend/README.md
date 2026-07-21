# Kraft Configurator — frontend

Svelte 5 + Vite + Threlte (a Svelte renderer for Three.js). Dark, purple-metallic,
SpaceX-telemetry-styled UI: see `src/app.css` for the full token list (colors, type,
radii) and `src/lib/components/Viewer3D.svelte` for the HUD overlay (corner brackets +
scanline) around the 3D viewport.

## Setup

```bash
npm install
npm run dev      # http://localhost:5173, proxies /api and /media to Django on :8000
```

If the Django backend isn't running yet, or has no published vehicles, the app falls
back to a small mock vehicle (`src/lib/mockData.js`) so the UI is still reviewable.
A banner at the bottom of the screen tells you when you're looking at mock data.

## Where things live

- `src/lib/api.js` — fetch wrapper for the Django REST endpoints.
- `src/lib/components/Viewer3D.svelte` — the 3D canvas + HUD chrome. Currently renders
  `PlaceholderVehicle.svelte` (simple boxes) since there's no real `.glb` yet. Once a
  model has gone through the asset pipeline, load it with `useGltf()` from
  `@threlte/extras` — there's a comment in the file showing exactly where.
- `src/lib/components/ConfiguratorPanel.svelte` — option categories, swatches, and the
  live price readout.
- `src/app.css` — theme tokens. Change these to retune the whole look.

## Adding a real 3D model

1. Run a model through the Java asset pipeline (`../asset-pipeline`) to get an
   optimized `.glb` and an entry in `manifest.json`.
2. Import it into Django with `python manage.py import_manifest manifest.json`.
3. The vehicle now has a real `model_file` URL — `Viewer3D` will need the small
   `useGltf` swap mentioned above to actually render it instead of the placeholder.

## Known rough edges

- No router yet — there's one configurator screen. Add `svelte-routing` or similar
  once you have a vehicle list/landing page to link from.
- No loading/error states beyond the basic mock-data fallback.
- The production bundle is ~790kB (mostly Three.js) — fine for now, but worth
  code-splitting (dynamic `import()` for the viewer) before shipping.
