"""
Run inside Blender, headless, by AssetProcessor.java — not meant to be run directly
with plain `python` (it depends on the `bpy` module that only exists inside Blender).

Usage (this is exactly what AssetProcessor.java shells out to):

    blender --background --factory-startup --python convert_to_glb.py -- <input_path> <output_path>

What it does:
  1. Clears the default scene.
  2. Imports the source mesh (.fbx or .obj).
  3. Exports it as a single, draco-compressed .glb.
  4. Prints one line to stdout: STATS_JSON:{"vertexCount": N, "faceCount": N}
     — AssetProcessor.java scans stdout for that prefix to fill in the manifest.

Why Blender instead of a Java library: there is no actively-maintained, license-clean
Java library that reads Sketchfab's typical FBX/OBJ exports and writes optimized,
draco-compressed glTF. Blender's Python API does this reliably and is free, so the
Java process here is an orchestrator, not the converter.
"""
import json
import sys
import os

import bpy


def parse_args():
    # Blender puts everything after "--" into sys.argv as-is.
    argv = sys.argv
    if "--" not in argv:
        raise SystemExit("Expected -- <input_path> <output_path>")
    args = argv[argv.index("--") + 1:]
    if len(args) != 2:
        raise SystemExit("Usage: convert_to_glb.py -- <input_path> <output_path>")
    return args[0], args[1]


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def import_source(input_path):
    ext = os.path.splitext(input_path)[1].lower()
    if ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=input_path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=input_path)
    else:
        raise SystemExit(f"Unsupported source extension for Blender import: {ext}")


def mesh_stats():
    vertex_count = 0
    face_count = 0
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            vertex_count += len(obj.data.vertices)
            face_count += len(obj.data.polygons)
    return vertex_count, face_count


def export_glb(output_path):
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=output_path,
        export_format="GLB",
        export_draco_mesh_compression_enable=True,
        export_apply=True,
    )


def main():
    input_path, output_path = parse_args()
    clear_scene()
    import_source(input_path)
    vertex_count, face_count = mesh_stats()
    export_glb(output_path)
    print("STATS_JSON:" + json.dumps({"vertexCount": vertex_count, "faceCount": face_count}))


if __name__ == "__main__":
    main()
