# Kraft Configurator — asset pipeline (Java)

Turns raw Sketchfab downloads into optimized `.glb` files in `../backend/media/models/`,
plus a `manifest.json` you import into Django.

## Prerequisites

- JDK 17+
- Maven
- [Blender](https://www.blender.org/) installed, with `blender` runnable from your
  terminal (needed only for `.fbx`/`.obj` sources — `.glb`/`.gltf` downloads are
  copied through with no conversion step).

## How it works

```
AssetProcessor.java  →  shells out to  →  scripts/convert_to_glb.py (runs inside Blender)
```

Java does the orchestration (folder scanning, naming conventions, manifest output);
Blender does the actual mesh import/export, because there isn't a good license-clean
Java library for reading Sketchfab's typical FBX/OBJ exports and writing optimized,
draco-compressed glTF. If you'd rather not depend on Blender being installed, you can
swap `convertWithBlender()` in `AssetProcessor.java` for a call to a different CLI tool
(e.g. `gltf-pipeline` for already-glTF inputs, or FBX2glTF) — the rest of the pipeline
(folder scanning, manifest writing) doesn't care how the conversion happens.

## Usage

1. For each model you've downloaded from Sketchfab, make a folder under `raw-assets/`
   named `<type>__<slug>`, where `<type>` is `car`, `boat`, or `bike`:

   ```
   raw-assets/
     car__velora-gt/
       velora-gt.fbx
       textures/
   ```

2. Build and run:

   ```bash
   mvn package
   java -jar target/asset-pipeline-0.1.0.jar
   ```

3. Check `output/manifest.json` — each entry shows `status: "ok" | "skipped" | "failed"`
   with a `notes` field explaining any skip/failure (wrong folder naming, no mesh file
   found, Blender not on PATH, etc).

4. From the backend, import the manifest:

   ```bash
   cd ../backend
   python manage.py import_manifest ../asset-pipeline/output/manifest.json
   ```

   This creates/updates a `Vehicle` row per entry. Prices default to 0 and vehicles are
   unpublished by default — fill in real pricing and flip `is_published` in the Django
   admin (or pass `--publish` to the import command) once you're happy with them.

## Honest status of this code

This sandbox has no network access to Maven Central and no JDK compiler (`javac`)
installed, so **`AssetProcessor.java` has been carefully reviewed but not actually
compiled or run here.** Before relying on it:

```bash
mvn package          # will fail loudly with any real syntax/dependency issue
java -jar target/asset-pipeline-0.1.0.jar
```

If `mvn package` surfaces an error, it's almost certainly something small (an import,
a typo) — the overall structure (scan → convert-or-copy → write manifest) is solid,
but treat this module as "needs a first real build" rather than "tested."
