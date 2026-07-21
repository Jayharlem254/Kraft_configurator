# Kraft Configurator — backend

Django + Django REST Framework. SQLite for local dev (swap `DATABASES` in
`config/settings.py` for Postgres when you're ready to deploy).

## Setup

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
python manage.py migrate
python manage.py createsuperuser
python manage.py runserver
```

This has been run end-to-end in development (`migrate` + `runserver` +
`GET /api/vehicles/` all verified working) — it's a solid base to build on, not
just an untested sketch.

## API

| Endpoint | Method | What |
|---|---|---|
| `/api/vehicles/` | GET | List published vehicles. Filter with `?vehicle_type=car\|boat\|bike`. |
| `/api/vehicles/<slug>/` | GET | Full detail for one vehicle, including option categories/choices. |
| `/api/configurations/` | POST | Save a configuration: `{ vehicle, option_ids: [...], session_key }`. |
| `/api/configurations/<id>/` | GET / PATCH | Fetch or update a saved configuration. |
| `/api/configurations/?session_key=...` | GET | List configs for a given anonymous session. |

Browse interactively at `http://localhost:8000/api/vehicles/` (DRF's browsable API)
once the server is running.

## Data model

- **Vehicle** — name, type (car/boat/bike), base price, `.glb` file, thumbnail.
- **OptionCategory** — a group of choices for one vehicle (e.g. "Exterior color").
- **OptionChoice** — one selectable choice (e.g. "Nebula Purple"), with a price delta
  and a `mesh_target` field identifying which mesh/material node in the `.glb` it
  should affect — that's the hook the frontend will use to actually swap materials
  on the 3D model rather than just showing a swatch.
- **Configuration** — a saved build: a vehicle + a set of chosen options, with
  `total_price` computed from the base price plus each selection's delta.

## Getting vehicles into the catalog

Either:
- Use the Django admin (`/admin/`) to create a `Vehicle` and its option categories/choices
  by hand — good for your first one or two while you're iterating on the data model, or
- Run a model through `../asset-pipeline` and import the resulting manifest:
  `python manage.py import_manifest <path-to-manifest.json>`

## Known rough edges / next steps

- **Auth**: there's none yet beyond Django's defaults. Configurations are tied to a
  `session_key` string the frontend should generate and persist (e.g. in a cookie) —
  add real user accounts before "save my build and come back later" needs to survive
  a cleared browser.
- **Pricing**: `total_price` is a simple sum of deltas. If you want
  category-dependent rules (e.g. a wheel choice that's only available with certain
  paint, or bundle discounts), that logic has nowhere to live yet — this is exactly
  the kind of thing a separate "rules engine" service could own.
- **Images**: thumbnails are plain `ImageField`s with local storage. Fine for dev;
  move to S3/Cloud Storage + `django-storages` before deploying.
