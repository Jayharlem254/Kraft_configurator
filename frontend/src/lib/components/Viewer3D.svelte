<script>
  import { WebGLRenderer, ACESFilmicToneMapping, NoToneMapping, SRGBColorSpace } from "three";
  import { Canvas } from "@threlte/core";
  import Viewer3DScene from "./Viewer3DScene.svelte";
  import ModelInspector from "./ModelInspector.svelte";

  /**
   * `glbUrl` should point at the optimized model produced by the asset
   * pipeline (e.g. "/media/models/velora-gt.glb", served by Django's
   * FileField on Vehicle.model_file). See Viewer3DScene for loading/framing
   * details. This component is the "shell": Canvas + HUD chrome + the tool
   * panel (select / measure / paint marker / screenshot / environment).
   *
   * `inspector` turns on the dev/QA "Model Inspector" sidebar (scene stats,
   * scene graph, wireframe/material-channel debugging, camera presets).
   * It's off by default — it's a build-verification tool, not something a
   * customer configuring a car should see. App.svelte flips it on via a
   * `?inspector=1` query param.
   */
  export let glbUrl = null;
  export let accentColor = "#00d2ff";
  export let modelScale = 1;
  export let vehicle = null;
  export let selections = {};
  export let inspector = false;

  const TOOLS = [
    { id: "select", label: "Select", hint: "DRAG TO ORBIT" },
    { id: "measure", label: "Measure", hint: "CLICK TWO POINTS ON THE MODEL" },
    { id: "decal", label: "Paint marker", hint: "CLICK THE MODEL TO DROP A MARKER" },
  ];
  const ENVIRONMENTS = ["studio", "sunset", "overcast", "night"];
  const DECAL_COLORS = ["#00d2ff", "#70e1ff", "#22c55e", "#ff5468", "#f8fafc"];

  let activeTool = "select";
  let environment = "studio";
  let decalColor = DECAL_COLORS[0];
  let decals = [];
  let measurePoints = [];
  let measurementDistance = null;
  let loading = false;
  let canvasEl;

  // --- Model Inspector state (only read when `inspector` is true) ---
  let wireframe = false;
  let wireframeColor = "#ffffff";
  let singleSided = false;
  let renderMode = "final"; // "final" | "raw" — final = tone-mapped + env lighting
  let materialChannel = "combined"; // "combined" | "base" | "normal" | "roughness" | "metalness" | "emission"
  let matcap = false;
  let sceneStats = null; // { tris, verts, meshes, mats, tex, graph }
  let cameraApi = null; // { goTo(preset), reset() } — bound up from Viewer3DScene

  $: activeHint = TOOLS.find((t) => t.id === activeTool)?.hint ?? "DRAG TO ORBIT";

  function selectTool(id) {
    activeTool = id;
  }

  function handleModelClick({ point }) {
    if (activeTool === "measure") {
      if (measurePoints.length >= 2) {
        measurePoints = [point];
        measurementDistance = null;
      } else {
        measurePoints = [...measurePoints, point];
        if (measurePoints.length === 2) {
          measurementDistance = measurePoints[0].distanceTo(measurePoints[1]);
        }
      }
    } else if (activeTool === "decal") {
      decals = [...decals, { position: point, color: decalColor, id: `${Date.now()}-${decals.length}` }];
    }
  }

  function clearMeasurement() {
    measurePoints = [];
    measurementDistance = null;
  }

  function clearDecals() {
    decals = [];
  }

  // preserveDrawingBuffer is required for canvas.toDataURL() to reliably
  // capture the current frame instead of a blank/black one.
  //
  // toneMapping + outputColorSpace are what actually turn a "correct but
  // flat" render into the punchy, glossy look of a proper preview render —
  // without them three.js renders in linear space and clips highlights
  // instead of rolling them off. shadowMap.enabled is required for the
  // castShadow/receiveShadow flags set in Viewer3DScene to do anything.
  let rendererRef = null;

  function createRenderer(canvas) {
    canvasEl = canvas;
    const renderer = new WebGLRenderer({ canvas, antialias: true, alpha: true, preserveDrawingBuffer: true });
    renderer.toneMapping = ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.1;
    renderer.outputColorSpace = SRGBColorSpace;
    renderer.shadowMap.enabled = true;
    rendererRef = renderer;
    return renderer;
  }

  // "No Post-Processing" (renderMode === "raw") drops tone mapping so you can
  // sanity-check raw material/texture values without the ACES roll-off —
  // useful for spotting a blown-out or mis-authored texture that the tone
  // mapper would otherwise quietly mask.
  $: if (rendererRef) {
    rendererRef.toneMapping = renderMode === "final" ? ACESFilmicToneMapping : NoToneMapping;
  }

  function takeScreenshot() {
    if (!canvasEl) return;
    const link = document.createElement("a");
    link.href = canvasEl.toDataURL("image/png");
    link.download = "kraft-configurator-render.png";
    link.click();
  }
</script>

<div class="viewer">
  <Canvas {createRenderer}>
    <Viewer3DScene
      {glbUrl}
      {accentColor}
      {modelScale}
      {activeTool}
      {environment}
      {decals}
      {measurePoints}
      {measurementDistance}
      {vehicle}
      {selections}
      {wireframe}
      {wireframeColor}
      {singleSided}
      {materialChannel}
      {matcap}
      useEnvironmentMap={renderMode === "final"}
      onModelClick={handleModelClick}
      onLoadingChange={(v) => (loading = v)}
      onSceneStats={(s) => (sceneStats = s)}
      bindCameraApi={(api) => (cameraApi = api)}
    />
  </Canvas>

  <!-- HUD chrome: corner brackets + scanline, pure overlay, doesn't touch the 3D scene -->
  <div class="hud-frame" aria-hidden="true">
    <span class="corner tl"></span>
    <span class="corner tr"></span>
    <span class="corner bl"></span>
    <span class="corner br"></span>
    <div class="scanline" class:active={loading}></div>
  </div>
  <div class="hud-label top-left mono-label">{loading ? "LOADING MODEL…" : "VIEWPORT // LIVE"}</div>
  <div class="hud-label bottom-right mono-label">{activeHint}</div>

  <!-- Tool panel, Illustrator-style vertical strip -->
  <div class="toolbar" class:shifted={inspector}>
    {#each TOOLS as tool (tool.id)}
      <button
        class="tool-btn"
        class:active={activeTool === tool.id}
        title={tool.label}
        on:click={() => selectTool(tool.id)}
      >
        <svg viewBox="0 0 24 24" width="18" height="18">
          {#if tool.id === "select"}
            <path d="M4 3l7 17 2.5-7L20 10.5 4 3z" fill="currentColor" />
          {:else if tool.id === "measure"}
            <path
              d="M3 17L17 3M3 17l3-1M3 17l1-3M8 12l2 2M12 8l2 2M16 4l2 2"
              stroke="currentColor"
              stroke-width="1.6"
              fill="none"
              stroke-linecap="round"
            />
          {:else if tool.id === "decal"}
            <circle cx="12" cy="9" r="3.4" fill="currentColor" />
            <path
              d="M12 21c3.5-4.2 5-6.9 5-9.5A5 5 0 0 0 7 11.5c0 2.6 1.5 5.3 5 9.5z"
              stroke="currentColor"
              stroke-width="1.4"
              fill="none"
            />
          {/if}
        </svg>
      </button>
    {/each}

    <div class="tool-divider"></div>

    <button class="tool-btn" title="Screenshot" on:click={takeScreenshot}>
      <svg viewBox="0 0 24 24" width="18" height="18">
        <path
          d="M4 8h3l1.5-2h7L17 8h3a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V9a1 1 0 0 1 1-1z"
          stroke="currentColor"
          stroke-width="1.4"
          fill="none"
        />
        <circle cx="12" cy="13" r="3.2" stroke="currentColor" stroke-width="1.4" fill="none" />
      </svg>
    </button>

    <div class="env-picker" title="Environment">
      <svg viewBox="0 0 24 24" width="15" height="15" class="env-icon">
        <circle cx="12" cy="12" r="4" stroke="currentColor" stroke-width="1.4" fill="none" />
        <path
          d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"
          stroke="currentColor"
          stroke-width="1.3"
          stroke-linecap="round"
        />
      </svg>
      <select bind:value={environment}>
        {#each ENVIRONMENTS as env}
          <option value={env}>{env[0].toUpperCase() + env.slice(1)}</option>
        {/each}
      </select>
    </div>
  </div>

  <!-- Contextual sub-panel for the active tool -->
  {#if activeTool === "decal"}
    <div class="tool-context">
      <span class="mono-label">MARKER COLOR</span>
      <div class="swatch-row">
        {#each DECAL_COLORS as color}
          <button
            class="color-swatch"
            class:active={decalColor === color}
            style="--swatch-color: {color}"
            on:click={() => (decalColor = color)}
            aria-label={`Use ${color}`}
          ></button>
        {/each}
      </div>
      {#if decals.length > 0}
        <button class="ghost-btn mono-label" on:click={clearDecals}>CLEAR ({decals.length})</button>
      {/if}
    </div>
  {:else if activeTool === "measure"}
    <div class="tool-context">
      <span class="mono-label">
        {#if measurementDistance !== null}
          DISTANCE: {measurementDistance.toFixed(2)} UNITS
        {:else if measurePoints.length === 1}
          CLICK A SECOND POINT
        {:else}
          CLICK TWO POINTS ON THE MODEL
        {/if}
      </span>
      {#if measurePoints.length > 0}
        <button class="ghost-btn mono-label" on:click={clearMeasurement}>CLEAR</button>
      {/if}
    </div>
  {/if}

  {#if inspector}
    <ModelInspector
      bind:wireframe
      bind:wireframeColor
      bind:singleSided
      bind:renderMode
      bind:materialChannel
      bind:matcap
      stats={sceneStats}
      onCameraPreset={(preset) => cameraApi?.goTo(preset)}
      onResetView={() => cameraApi?.reset()}
    />
  {/if}

  {#if !glbUrl}
    <div class="empty-model-overlay">
      <div class="empty-badge mono">NO 3D MODEL ATTACHED</div>
      <p class="empty-sub mono dim">Select or upload a .glb model in the "UPLOAD 3D" studio</p>
    </div>
  {/if}
</div>

<style>
  .viewer {
    position: relative;
    width: 100%;
    height: 100%;
    background: radial-gradient(circle at 50% 35%, #0f1622 0%, #05070a 75%);
    border-radius: var(--radius-lg);
    overflow: hidden;
  }

  .empty-model-overlay {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    text-align: center;
    pointer-events: none;
    background: rgba(10, 14, 20, 0.85);
    border: 1px solid var(--line);
    padding: 20px 32px;
    border-radius: var(--radius);
    backdrop-filter: blur(8px);
    z-index: 5;
  }

  .empty-badge {
    color: var(--crystal-bright);
    font-size: 13px;
    letter-spacing: 0.1em;
    font-weight: 700;
    margin-bottom: 6px;
  }

  .empty-sub {
    font-size: 11px;
    margin: 0;
  }

  .hud-frame {
    position: absolute;
    inset: 16px;
    pointer-events: none;
  }

  .corner {
    position: absolute;
    width: 22px;
    height: 22px;
    border: 1.5px solid var(--crystal-blue);
    opacity: 0.75;
    box-shadow: 0 0 6px var(--crystal-glow);
  }
  .corner.tl {
    top: 0;
    left: 0;
    border-right: none;
    border-bottom: none;
  }
  .corner.tr {
    top: 0;
    right: 0;
    border-left: none;
    border-bottom: none;
  }
  .corner.bl {
    bottom: 0;
    left: 0;
    border-right: none;
    border-top: none;
  }
  .corner.br {
    bottom: 0;
    right: 0;
    border-left: none;
    border-top: none;
  }

  .scanline {
    position: absolute;
    left: 0;
    right: 0;
    top: 0;
    height: 1px;
    background: linear-gradient(90deg, transparent, var(--crystal-bright), transparent);
    opacity: 0.15;
    animation: sweep 1.4s linear infinite;
    animation-play-state: paused;
    transition: opacity 0.2s;
  }
  .scanline.active {
    opacity: 0.55;
    animation-play-state: running;
  }

  @keyframes sweep {
    0% {
      top: 0%;
    }
    100% {
      top: 100%;
    }
  }

  .hud-label {
    position: absolute;
    pointer-events: none;
  }
  .top-left {
    top: 28px;
    left: 28px;
  }
  .bottom-right {
    bottom: 28px;
    right: 28px;
    max-width: 45%;
    text-align: right;
  }

  .toolbar {
    position: absolute;
    top: 50%;
    left: 16px;
    transform: translateY(-50%);
    display: flex;
    flex-direction: column;
    gap: 6px;
    background: rgba(14, 19, 26, 0.85);
    border: 1px solid var(--line-soft);
    border-radius: var(--radius-md);
    padding: 8px;
    backdrop-filter: blur(8px);
    transition: left 0.15s;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
  }
  .toolbar.shifted {
    left: 262px;
  }

  .tool-btn {
    width: 34px;
    height: 34px;
    display: flex;
    align-items: center;
    justify-content: center;
    background: transparent;
    border: 1px solid transparent;
    border-radius: var(--radius-sm);
    color: var(--text-secondary);
    transition: color 0.15s, border-color 0.15s, background 0.15s;
  }
  .tool-btn:hover {
    color: var(--text-primary);
    background: var(--surface);
  }
  .tool-btn.active {
    color: var(--crystal-bright);
    border-color: var(--crystal-blue);
    background: rgba(0, 210, 255, 0.12);
    box-shadow: 0 0 10px var(--crystal-glow);
  }

  .tool-divider {
    height: 1px;
    background: var(--line-soft);
    margin: 2px 4px;
  }

  .env-picker {
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 6px 4px 2px;
    color: var(--text-tertiary);
  }
  .env-picker select {
    flex: 1;
    width: 66px;
    background: transparent;
    border: none;
    color: var(--text-secondary);
    font-size: 11px;
    font-family: var(--font-mono);
  }
  .env-picker select option {
    background: var(--panel);
    color: var(--text-primary);
  }

  .tool-context {
    position: absolute;
    bottom: 20px;
    left: 50%;
    transform: translateX(-50%);
    display: flex;
    align-items: center;
    gap: 12px;
    background: rgba(15, 14, 22, 0.85);
    border: 1px solid var(--line-strong);
    border-radius: 999px;
    padding: 8px 16px;
    backdrop-filter: blur(8px);
    max-width: calc(100% - 40px);
  }

  .swatch-row {
    display: flex;
    gap: 6px;
  }
  .color-swatch {
    width: 18px;
    height: 18px;
    border-radius: 50%;
    background: var(--swatch-color);
    border: 2px solid transparent;
  }
  .color-swatch.active {
    border-color: var(--text-primary);
  }

  .ghost-btn {
    background: transparent;
    border: 1px solid var(--line);
    color: var(--text-tertiary);
    padding: 4px 10px;
    border-radius: 999px;
    white-space: nowrap;
  }
  .ghost-btn:hover {
    color: var(--text-primary);
    border-color: var(--line-strong);
  }
</style>