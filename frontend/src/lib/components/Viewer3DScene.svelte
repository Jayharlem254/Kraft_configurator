<script>
  import { onMount } from "svelte";
  import {
    Box3,
    Vector3,
    Quaternion,
    Color,
    FrontSide,
    DoubleSide,
    LineSegments,
    WireframeGeometry,
    LineBasicMaterial,
    MeshBasicMaterial,
    MeshMatcapMaterial,
    MeshPhysicalMaterial,
    CanvasTexture,
    SRGBColorSpace,
    PMREMGenerator,
  } from "three";
  import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
  import { T, useThrelte } from "@threlte/core";
  import { OrbitControls, GLTF, HTML, Billboard, interactivity } from "@threlte/extras";
  import { effectiveSelections } from "../selections.js";

  /**
   * Lives inside <Canvas> (unlike the parent Viewer3D shell) specifically so
   * interactivity() has a Threlte/Canvas context to attach to — it can't be
   * called from a component outside the Canvas tree.
   */

  export let glbUrl = null;
  export let accentColor = "#00d2ff";
  export let modelScale = 1;
  export let activeTool = "select";
  export let environment = "studio";
  export let decals = [];
  export let measurePoints = [];
  export let measurementDistance = null;
  export let vehicle = null;
  export let selections = {};
  export let onModelClick = () => {};
  export let onLoadingChange = () => {};

  // --- Model Inspector props (all no-ops when Viewer3D's `inspector` flag
  // is off, since Viewer3D only ever passes non-default values in that mode) ---
  export let wireframe = false;
  export let wireframeColor = "#ffffff";
  export let singleSided = false;
  export let materialChannel = "combined"; // "combined" | "base" | "normal" | "roughness" | "metalness" | "emission"
  export let matcap = false;
  export let useEnvironmentMap = true;
  export let onSceneStats = () => {};
  export let bindCameraApi = () => {};

  interactivity();

  // Real environment lighting for PBR materials — this is the fix for
  // metallic/glossy paint rendering near-black under only 3 point/directional
  // lights (a metal MeshStandardMaterial has almost no diffuse response; it
  // needs *something* to reflect). Built procedurally with PMREMGenerator
  // from three's bundled RoomEnvironment so there's no external .hdr file to
  // host or for this to 404 against.
  const { renderer, scene } = useThrelte();
  let envTexture = null;
  onMount(() => {
    const pmremGenerator = new PMREMGenerator(renderer);
    envTexture = pmremGenerator.fromScene(new RoomEnvironment(renderer), 0.04).texture;
    pmremGenerator.dispose();
    return () => envTexture?.dispose();
  });
  $: scene.environment = useEnvironmentMap ? envTexture : null;

  const TARGET_SIZE = 3.4; // world units the model's longest axis is normalized to

  // Lighting presets — "studio" keeps the crystal-blue rim look tied to
  // accentColor; the others are fixed moods independent of vehicle branding.
  const ENV_PRESETS = {
    studio: { ambient: 0.85, keyIntensity: 3.6, keyColor: "#ffffff", rimIntensity: 26, rimColor: accentColor, groundColor: "#06090e" },
    sunset: { ambient: 0.7, keyIntensity: 4, keyColor: "#ffb37a", rimIntensity: 20, rimColor: "#ff6a3d", groundColor: "#140c0a" },
    overcast: { ambient: 1.1, keyIntensity: 2.2, keyColor: "#dfe6ee", rimIntensity: 8, rimColor: "#8fa2b8", groundColor: "#0e1013" },
    night: { ambient: 0.25, keyIntensity: 1.2, keyColor: "#7ea8ff", rimIntensity: 30, rimColor: "#00d2ff", groundColor: "#04080e" },
  };
  $: preset = ENV_PRESETS[environment] ?? ENV_PRESETS.studio;

  let loadFailed = false;
  let transform = { scale: 1, position: [0, 0, 0] };
  let previousUrl;

  // The loaded GLTF's scene graph, kept around (outside frameModel's local
  // scope) so material updates can re-traverse it whenever selections change,
  // without needing to reload the model. materialCache holds one cloned
  // material per original material — GLTF exports commonly share a single
  // material instance across several meshes, so mutating .color in place
  // would silently recolor every mesh using that material, not just the
  // targeted one.
  let loadedScene = null;
  let materialCache = new Map();

  // Inspector-only caches — kept separate from materialCache above because
  // that one holds *color-selection* clones (persist for the model's
  // lifetime); these hold whatever material was on a mesh right before a
  // debug view (channel isolation / matcap) temporarily replaced it, so it
  // can be restored exactly when the debug view is turned back off.
  let wireframeOverlays = new Map(); // mesh -> LineSegments overlay
  let channelDebugCache = new Map(); // mesh -> material to restore
  let matcapTexture = null;

  $: if (glbUrl !== previousUrl) {
    previousUrl = glbUrl;
    loadFailed = false;
    transform = { scale: 1, position: [0, 0, 0] };
    loadedScene = null;
    materialCache = new Map();
    wireframeOverlays = new Map();
    channelDebugCache = new Map();
    onLoadingChange(!!glbUrl);
  }

  // Procedural matcap so the "Matcap" toggle doesn't depend on shipping a
  // texture asset — a soft studio-sphere gradient is enough to check
  // silhouette/topology independent of the model's real materials.
  function getMatcapTexture() {
    if (matcapTexture) return matcapTexture;
    const size = 256;
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = size;
    const ctx = canvas.getContext("2d");
    const gradient = ctx.createRadialGradient(size * 0.35, size * 0.32, size * 0.02, size * 0.5, size * 0.5, size * 0.62);
    gradient.addColorStop(0, "#ffffff");
    gradient.addColorStop(0.4, "#b9b9c4");
    gradient.addColorStop(0.75, "#4a4a58");
    gradient.addColorStop(1, "#111018");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, size, size);
    matcapTexture = new CanvasTexture(canvas);
    matcapTexture.colorSpace = SRGBColorSpace;
    return matcapTexture;
  }

  function computeSceneStats(scene) {
    let tris = 0;
    let verts = 0;
    let meshes = 0;
    const mats = new Set();
    const texs = new Set();
    const graph = [];

    scene.traverse((node) => {
      if (node.isMesh) {
        meshes++;
        const geo = node.geometry;
        if (geo?.attributes?.position) {
          verts += geo.attributes.position.count;
          tris += geo.index ? geo.index.count / 3 : geo.attributes.position.count / 3;
        }
        const nodeMats = Array.isArray(node.material) ? node.material : [node.material];
        nodeMats.forEach((m) => {
          if (!m) return;
          mats.add(m);
          // Not exhaustive (glTF can pack occlusion/roughness/metalness into
          // one ORM texture, which this counts once under roughnessMap) —
          // good enough for a "how heavy is this asset" readout.
          ["map", "normalMap", "roughnessMap", "metalnessMap", "emissiveMap", "aoMap"].forEach((key) => {
            if (m[key]) texs.add(m[key]);
          });
        });
      }
      if (node.name) graph.push({ name: node.name, isMesh: !!node.isMesh });
    });

    return { tris: Math.round(tris), verts, meshes, mats: mats.size, tex: texs.size, graph };
  }

  function frameModel(scene) {
    const box = new Box3().setFromObject(scene);
    const size = new Vector3();
    box.getSize(size);
    const maxDim = Math.max(size.x, size.y, size.z) || 1;
    const scale = (TARGET_SIZE / maxDim) * modelScale;

    const center = new Vector3();
    box.getCenter(center);

    transform = {
      scale,
      position: [-center.x * scale, -box.min.y * scale, -center.z * scale],
    };

    scene.traverse((node) => {
      if (node.isMesh) {
        node.castShadow = true;
        node.receiveShadow = true;
      }
    });

    onLoadingChange(false);
    onSceneStats(computeSceneStats(scene));
    loadedScene = scene;
  }

  // Upgrades a GLTF-imported material (usually MeshStandardMaterial) to
  // MeshPhysicalMaterial so painted panels get a real clear-coat layer
  // instead of a flat color swap. Built explicitly field-by-field rather
  // than material.copy() so an original that already lacks clearcoat/sheen
  // properties can't leave them undefined on the new material.
  function toCarPaintMaterial(original) {
    if (original.isMeshPhysicalMaterial) return original.clone();
    return new MeshPhysicalMaterial({
      name: original.name,
      map: original.map ?? null,
      normalMap: original.normalMap ?? null,
      normalScale: original.normalScale,
      roughnessMap: original.roughnessMap ?? null,
      metalnessMap: original.metalnessMap ?? null,
      aoMap: original.aoMap ?? null,
      aoMapIntensity: original.aoMapIntensity ?? 1,
      emissive: original.emissive,
      emissiveMap: original.emissiveMap ?? null,
      emissiveIntensity: original.emissiveIntensity ?? 1,
      envMapIntensity: original.envMapIntensity ?? 1,
      side: original.side,
    });
  }

  function applyMaterialSelections() {
    if (!loadedScene || !vehicle?.option_categories) return;
    const effective = effectiveSelections(vehicle, selections);

    for (const category of vehicle.option_categories) {
      const choice = category.choices.find((c) => c.id === effective[category.id]);
      if (!choice?.mesh_target || !choice?.swatch_hex) continue;

      let matched = false;
      loadedScene.traverse((node) => {
        if (!node.isMesh) return;
        const materialName = Array.isArray(node.material) ? node.material[0]?.name : node.material?.name;
        if (node.name !== choice.mesh_target && materialName !== choice.mesh_target) return;
        matched = true;

        const originals = Array.isArray(node.material) ? node.material : [node.material];
        const cloned = originals.map((mat) => {
          if (!materialCache.has(mat)) materialCache.set(mat, toCarPaintMaterial(mat));
          return materialCache.get(mat);
        });
        node.material = Array.isArray(node.material) ? cloned : cloned[0];
        cloned.forEach((mat) => {
          mat.color?.set(choice.swatch_hex);
          // Flat, even clearcoat gloss over the base color — the layer that
          // reads as "real automotive paint" instead of "flat plastic".
          // Once choices carry a finish (metallic/matte/pearl), branch these
          // per finish instead of the fixed values below.
          mat.clearcoat = 1.0;
          mat.clearcoatRoughness = 0.03;
          mat.metalness = 0.55;
          mat.roughness = 0.35;
        });
      });

      if (!matched) {
        console.warn(
          `Option "${choice.name}" has mesh_target "${choice.mesh_target}", but no mesh or material with that name was found in the loaded model.`
        );
      }
    }
  }

  // Re-run whenever a new model finishes loading (loadedScene flips from
  // null) or the user changes a selection — both are cheap: at most a few
  // hundred nodes to traverse, and materials are cloned once then cached.
  $: if (loadedScene && selections) {
    applyMaterialSelections();
  }

  // Wireframe overlay: a LineSegments twin per mesh, toggled visible rather
  // than replacing the mesh's real material — so wireframe can be viewed
  // together with the actual shaded surface, not instead of it.
  $: if (loadedScene) {
    loadedScene.traverse((node) => {
      if (!node.isMesh || wireframeOverlays.has(node)) return;
      const overlay = new LineSegments(
        new WireframeGeometry(node.geometry),
        new LineBasicMaterial({ color: wireframeColor, transparent: true, opacity: 0.85, depthTest: true })
      );
      overlay.visible = false;
      overlay.raycast = () => {}; // never intercept clicks meant for the mesh
      node.add(overlay);
      wireframeOverlays.set(node, overlay);
    });
  }
  $: for (const overlay of wireframeOverlays.values()) {
    overlay.visible = wireframe;
    overlay.material.color.set(wireframeColor);
  }

  // Single Sided: applies straight to whatever material is currently on the
  // mesh (selection-colored or debug), so it composes with both of those.
  $: if (loadedScene) {
    loadedScene.traverse((node) => {
      if (!node.isMesh) return;
      const mats = Array.isArray(node.material) ? node.material : [node.material];
      mats.forEach((m) => {
        if (m) m.side = singleSided ? FrontSide : DoubleSide;
      });
    });
  }

  // Material channel isolation + Matcap. Both temporarily replace the real
  // material with a debug one; channelDebugCache remembers the material to
  // restore per-mesh. NOTE: if a color selection changes while a debug view
  // is active, applyMaterialSelections above may not find a name match on
  // the swapped-in debug material — known limitation, worth unifying into
  // one traversal if this starts causing visible glitches in practice.
  $: debugViewActive = matcap || materialChannel !== "combined";

  function buildChannelMaterial(baseMat, channel) {
    if (!baseMat) return new MeshBasicMaterial({ color: "#333" });
    if (channel === "base") {
      return new MeshBasicMaterial({ map: baseMat.map ?? null, color: baseMat.map ? "#ffffff" : baseMat.color ?? "#999" });
    }
    if (channel === "normal") {
      return baseMat.normalMap
        ? new MeshBasicMaterial({ map: baseMat.normalMap })
        : new MeshBasicMaterial({ color: "#8080ff" }); // flat "no normal map authored" tint
    }
    if (channel === "roughness") {
      const r = baseMat.roughness ?? 1;
      return baseMat.roughnessMap
        ? new MeshBasicMaterial({ map: baseMat.roughnessMap })
        : new MeshBasicMaterial({ color: new Color(r, r, r) });
    }
    if (channel === "metalness") {
      const mval = baseMat.metalness ?? 0;
      return baseMat.metalnessMap
        ? new MeshBasicMaterial({ map: baseMat.metalnessMap })
        : new MeshBasicMaterial({ color: new Color(mval, mval, mval) });
    }
    if (channel === "emission") {
      return baseMat.emissiveMap
        ? new MeshBasicMaterial({ map: baseMat.emissiveMap })
        : new MeshBasicMaterial({ color: baseMat.emissive ?? "#000000" });
    }
    return new MeshBasicMaterial({ color: "#333" });
  }

  $: if (loadedScene) {
    loadedScene.traverse((node) => {
      if (!node.isMesh) return;

      if (!debugViewActive) {
        if (channelDebugCache.has(node)) {
          node.material = channelDebugCache.get(node);
          channelDebugCache.delete(node);
        }
        return;
      }

      if (!channelDebugCache.has(node)) channelDebugCache.set(node, node.material);
      const original = channelDebugCache.get(node);
      const baseMat = Array.isArray(original) ? original[0] : original;

      node.material = matcap
        ? new MeshMatcapMaterial({ matcap: getMatcapTexture(), map: baseMat?.map ?? null })
        : buildChannelMaterial(baseMat, materialChannel);
    });
  }

  // --- Camera presets ---
  let cameraRef;
  let controlsRef;
  const CAMERA_PRESETS = {
    front: [0, 1.2, 6],
    back: [0, 1.2, -6],
    left: [-6, 1.2, 0],
    right: [6, 1.2, 0],
    top: [0, 7, 0.01],
    "3/4": [4.5, 2, 4.5],
  };
  const ORBIT_TARGET = [0, 0.4, 0];

  function goToPreset(preset) {
    const pos = CAMERA_PRESETS[preset];
    if (!pos || !cameraRef || !controlsRef) return;
    cameraRef.position.set(...pos);
    controlsRef.target.set(...ORBIT_TARGET);
    controlsRef.update();
  }

  $: if (cameraRef && controlsRef) {
    bindCameraApi({ goTo: goToPreset, reset: () => goToPreset("3/4") });
  }

  function handleClick(e) {
    if (activeTool === "select") return;
    e.stopPropagation();
    onModelClick({ point: e.point.clone() });
  }

  function midpoint(a, b) {
    return [(a.x + b.x) / 2, (a.y + b.y) / 2 + 0.16, (a.z + b.z) / 2];
  }

  // Orient+scale a thin cylinder to act as a line between two world points —
  // more robust across Threlte versions than hand-building a BufferGeometry.
  function segmentTransform(a, b) {
    const start = new Vector3(a.x, a.y, a.z);
    const end = new Vector3(b.x, b.y, b.z);
    const mid = start.clone().add(end).multiplyScalar(0.5);
    const dir = end.clone().sub(start);
    const length = dir.length() || 0.0001;
    const quat = new Quaternion().setFromUnitVectors(new Vector3(0, 1, 0), dir.clone().normalize());
    return {
      position: [mid.x, mid.y, mid.z],
      quaternion: [quat.x, quat.y, quat.z, quat.w],
      length,
    };
  }
</script>

<T.PerspectiveCamera makeDefault position={[4.5, 2, 4.5]} bind:ref={cameraRef} fov={35}>
  <OrbitControls enableDamping target={[0, 0.4, 0]} maxPolarAngle={1.45} bind:ref={controlsRef} />
</T.PerspectiveCamera>

<T.AmbientLight intensity={preset.ambient} />
<T.DirectionalLight
  position={[5, 6, 3]}
  intensity={preset.keyIntensity}
  color={preset.keyColor}
  castShadow
  shadow.mapSize={[1024, 1024]}
/>
<!-- Rim light — purple by default (studio), tinted per environment preset -->
<T.PointLight position={[-4, 2, -3]} intensity={preset.rimIntensity} color={preset.rimColor} />

<!-- Ground plane, subtle, so the model doesn't float in pure void -->
<T.Mesh rotation.x={-Math.PI / 2} position.y={-0.01} receiveShadow>
  <T.CircleGeometry args={[6, 64]} />
  <T.MeshStandardMaterial color={preset.groundColor} roughness={0.9} />
</T.Mesh>

<!-- #key remounts on URL change, so a fresh GLTF load starts each time -->
{#key glbUrl}
  {#if glbUrl && !loadFailed}
    <T.Group scale={transform.scale} position={transform.position} onclick={handleClick}>
      <GLTF
        url={glbUrl}
        onload={(gltf) => frameModel(gltf.scene)}
        onerror={(err) => {
          console.warn(`Couldn't load 3D model at "${glbUrl}".`, err);
          loadFailed = true;
          onLoadingChange(false);
        }}
      />
    </T.Group>
  {/if}
{/key}

<!-- Decal markers: small camera-facing discs at each recorded click point -->
{#each decals as decal (decal.id)}
  <Billboard position={[decal.position.x, decal.position.y, decal.position.z]}>
    <T.Mesh>
      <T.CircleGeometry args={[0.045, 24]} />
      <T.MeshBasicMaterial color={decal.color} />
    </T.Mesh>
    <T.Mesh position.z={-0.001}>
      <T.RingGeometry args={[0.045, 0.065, 24]} />
      <T.MeshBasicMaterial color={decal.color} transparent opacity={0.5} />
    </T.Mesh>
  </Billboard>
{/each}

<!-- Measurement: point markers + connecting segment + distance label -->
{#each measurePoints as point, i (i)}
  <Billboard position={[point.x, point.y, point.z]}>
    <T.Mesh>
      <T.CircleGeometry args={[0.04, 20]} />
      <T.MeshBasicMaterial color="#22d3ee" />
    </T.Mesh>
  </Billboard>
{/each}

{#if measurePoints.length === 2}
  {@const segment = segmentTransform(measurePoints[0], measurePoints[1])}
  <T.Mesh position={segment.position} quaternion={segment.quaternion}>
    <T.CylinderGeometry args={[0.007, 0.007, segment.length, 8]} />
    <T.MeshBasicMaterial color="#22d3ee" />
  </T.Mesh>

  {#if measurementDistance !== null}
    <HTML position={midpoint(measurePoints[0], measurePoints[1])} center>
      <div class="measure-label">{measurementDistance.toFixed(2)} units</div>
    </HTML>
  {/if}
{/if}

<style>
  .measure-label {
    font-family: var(--font-mono, monospace);
    font-size: 11px;
    letter-spacing: 0.05em;
    color: #d9f7ff;
    background: rgba(8, 14, 18, 0.85);
    border: 1px solid rgba(34, 211, 238, 0.5);
    padding: 3px 8px;
    border-radius: 4px;
    white-space: nowrap;
  }
</style>