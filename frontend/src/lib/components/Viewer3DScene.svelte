<script>
  import { Box3, Vector3, Quaternion } from "three";
  import { T } from "@threlte/core";
  import { OrbitControls, GLTF, HTML, Billboard, interactivity } from "@threlte/extras";
  import PlaceholderVehicle from "./PlaceholderVehicle.svelte";
  import { effectiveSelections } from "../selections.js";

  /**
   * Lives inside <Canvas> (unlike the parent Viewer3D shell) specifically so
   * interactivity() has a Threlte/Canvas context to attach to — it can't be
   * called from a component outside the Canvas tree.
   */

  export let glbUrl = null;
  export let accentColor = "#8b5cf6";
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

  interactivity();

  const TARGET_SIZE = 3.4; // world units the model's longest axis is normalized to

  // Lighting presets — "studio" keeps the original purple-rim look tied to
  // accentColor; the others are fixed moods independent of vehicle branding.
  const ENV_PRESETS = {
    studio: { ambient: 0.35, keyIntensity: 1.1, keyColor: "#ffffff", rimIntensity: 6, rimColor: accentColor, groundColor: "#0c0b12" },
    sunset: { ambient: 0.3, keyIntensity: 1.3, keyColor: "#ffb37a", rimIntensity: 5, rimColor: "#ff6a3d", groundColor: "#140c0a" },
    overcast: { ambient: 0.55, keyIntensity: 0.7, keyColor: "#dfe6ee", rimIntensity: 2, rimColor: "#8fa2b8", groundColor: "#0e1013" },
    night: { ambient: 0.12, keyIntensity: 0.4, keyColor: "#7ea8ff", rimIntensity: 8, rimColor: "#6d5bff", groundColor: "#040308" },
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

  $: if (glbUrl !== previousUrl) {
    previousUrl = glbUrl;
    loadFailed = false;
    transform = { scale: 1, position: [0, 0, 0] };
    loadedScene = null;
    materialCache = new Map();
    onLoadingChange(!!glbUrl);
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
    onLoadingChange(false);
    loadedScene = scene;
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
          if (!materialCache.has(mat)) materialCache.set(mat, mat.clone());
          return materialCache.get(mat);
        });
        node.material = Array.isArray(node.material) ? cloned : cloned[0];
        cloned.forEach((mat) => mat.color?.set(choice.swatch_hex));
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

<T.PerspectiveCamera makeDefault position={[4.5, 2, 4.5]} fov={35}>
  <OrbitControls enableDamping target={[0, 0.4, 0]} maxPolarAngle={1.45} />
</T.PerspectiveCamera>

<T.AmbientLight intensity={preset.ambient} />
<T.DirectionalLight position={[5, 6, 3]} intensity={preset.keyIntensity} color={preset.keyColor} />
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
          console.warn(`Couldn't load model at "${glbUrl}", falling back to placeholder.`, err);
          loadFailed = true;
          onLoadingChange(false);
        }}
      />
    </T.Group>
  {:else}
    <T.Group onclick={handleClick}>
      <PlaceholderVehicle {accentColor} />
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