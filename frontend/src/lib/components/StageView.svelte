<script>
  import { onMount } from "svelte";
  import { Stage } from "../three/Stage.js";
  import { proceduralShape, storeSnapshot } from "../three/thumbnails.js";

  let {
    vehicle,
    visual,
    view = "front34",
    environment = "studio",
    autoRotate = false,
    interactive = true,
    onstage = () => {},
    onbuilt = () => {},
    oninteract = () => {},
  } = $props();

  let host;
  let stage = $state(null);
  let progress = $state(0);
  let loading = $state(false);
  let failed = $state(null);
  let loadedSlug = null;

  onMount(() => {
    stage = new Stage(host, { interactive, autoRotate });
    stage.onInteract = () => oninteract();
    onstage(stage);
    return () => {
      stage.dispose();
      stage = null;
    };
  });

  // (Re)load the model when the vehicle changes.
  $effect(() => {
    if (!stage || !vehicle) return;
    if (loadedSlug === vehicle.slug) return;
    loadedSlug = vehicle.slug;
    const v = vehicle;
    const spec = {
      kind: v.vehicle_type,
      shape: proceduralShape(v),
      modelUrl: v.model_file || null,
      materialMap: v.material_map,
    };
    loading = true;
    failed = null;
    progress = 0;
    stage.view = view;
    stage
      .setVehicle(spec, $state.snapshot(visual), (p) => (progress = p))
      .then((built) => {
        if (!built) return;
        loading = false;
        onbuilt(built);
        // Capture a real gallery thumbnail for uploaded models.
        if (v.model_file) {
          requestAnimationFrame(() => {
            try {
              storeSnapshot(v, stage.snapshot({ width: 960, height: 600, type: "image/webp", quality: 0.85, view: built.analysis?.hero ?? "front34" }));
            } catch {
              /* ignore */
            }
          });
        }
      })
      .catch((err) => {
        console.warn("3D model failed to load, falling back to procedural model", err);
        failed = "The uploaded 3D model could not be loaded — showing a procedural preview instead.";
        stage.setVehicle({ ...spec, modelUrl: null }, $state.snapshot(visual)).finally(() => (loading = false));
      });
  });

  $effect(() => {
    const snap = $state.snapshot(visual);
    if (stage && !loading) stage.applyVisual(snap);
  });
  $effect(() => {
    // Before the first model is framed there is nothing to animate toward.
    if (!stage) return;
    if (!stage.current) {
      stage.view = view;
      return;
    }
    if (stage.view !== view) stage.setView(view);
  });
  $effect(() => {
    stage?.setEnvironment(environment);
  });
  $effect(() => {
    stage?.setAutoRotate(autoRotate);
  });
</script>

<div class="stage" bind:this={host}>
  {#if loading}
    <div class="loading" role="status">
      <div class="bar"><span style={`transform: scaleX(${progress || 0.08})`}></span></div>
      <p class="small muted">{vehicle?.model_file ? `Loading 3D model… ${Math.round(progress * 100)}%` : "Preparing 3D view…"}</p>
    </div>
  {/if}
  {#if failed}
    <p class="failed xsmall">{failed}</p>
  {/if}
</div>

<style>
  .stage {
    position: absolute;
    inset: 0;
    overflow: hidden;
    transition: background 0.6s var(--ease);
    cursor: grab;
  }
  .stage:active {
    cursor: grabbing;
  }
  .loading {
    position: absolute;
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%);
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 12px;
    pointer-events: none;
  }
  .bar {
    width: 220px;
    height: 2px;
    background: rgba(14, 14, 18, 0.12);
    overflow: hidden;
  }
  .bar span {
    display: block;
    height: 100%;
    background: var(--ink);
    transform-origin: left;
    transition: transform 0.2s linear;
  }
  .failed {
    position: absolute;
    left: 16px;
    top: 16px;
    background: #fff4e5;
    color: #6b3d00;
    padding: 6px 10px;
    border-radius: 4px;
  }
</style>
