<script>
  /**
   * Dev/QA sidebar — mounted by Viewer3D only when `inspector` is true.
   * Pure presentation + event emission; all the actual three.js work
   * (wireframe overlays, material-channel swaps, camera moves, stats
   * collection) lives in Viewer3DScene, which this binds to via props.
   */
  export let wireframe = false;
  export let wireframeColor = "#ffffff";
  export let singleSided = false;
  export let renderMode = "final"; // "final" | "raw"
  export let materialChannel = "combined";
  export let matcap = false;
  export let stats = null; // { tris, verts, meshes, mats, tex, graph } | null
  export let onCameraPreset = () => {};
  export let onResetView = () => {};

  const WIREFRAME_COLORS = ["off", "#000000", "#9a9aa6", "#ef4444", "#3b82f6", "#22c55e", "#eab308"];

  const CHANNELS = [
    { id: "base", label: "Base Color", wired: true },
    { id: "metalness", label: "Metalness", wired: true },
    { id: "roughness", label: "Roughness", wired: true },
    { id: "normal", label: "Normal Map", wired: true },
    { id: "emission", label: "Emission", wired: true },
    { id: "opacity", label: "Opacity", wired: false },
    { id: "alpha", label: "Alpha Masking", wired: false },
    { id: "specular", label: "Specular F0", wired: false },
    { id: "clearcoat", label: "Clear Coat", wired: false },
    { id: "clearcoat-rough", label: "Clear Coat Roughness", wired: false },
  ];

  const CAMERA_ROWS = [
    ["front", "back", "left"],
    ["right", "top", "3/4"],
  ];

  let collapsed = false;

  function pickWireframe(color) {
    if (color === "off") {
      wireframe = false;
      return;
    }
    wireframe = true;
    wireframeColor = color;
  }

  function pickChannel(channel) {
    if (!channel.wired) return; // greyed-out entries aren't implemented yet
    matcap = false;
    materialChannel = materialChannel === channel.id ? "combined" : channel.id;
  }

  function toggleMatcap() {
    materialChannel = "combined";
    matcap = !matcap;
  }

  function formatNumber(n) {
    if (n == null) return "—";
    return n >= 1000 ? `${(n / 1000).toFixed(1)}k` : `${n}`;
  }
</script>

<div class="inspector">
  <button class="header" on:click={() => (collapsed = !collapsed)}>
    <span class="chevron" class:collapsed>▾</span>
    <span>Model Inspector</span>
  </button>

  {#if !collapsed}
    <div class="body">
      <!-- WIREFRAME -->
      <section>
        <span class="mono-label section-label">Wireframe</span>
        <div class="swatch-row">
          {#each WIREFRAME_COLORS as color}
            <button
              class="w-swatch"
              class:checker={color === "off"}
              class:active={color === "off" ? !wireframe : wireframe && wireframeColor === color}
              style={color !== "off" ? `--swatch: ${color}` : ""}
              title={color === "off" ? "Off" : color}
              on:click={() => pickWireframe(color)}
              aria-label={color === "off" ? "Wireframe off" : `Wireframe ${color}`}
            ></button>
          {/each}
        </div>
      </section>

      <!-- SINGLE SIDED -->
      <section class="row-toggle">
        <button
          class="switch"
          class:on={singleSided}
          role="switch"
          aria-checked={singleSided}
          aria-label="Single Sided"
          on:click={() => (singleSided = !singleSided)}
        >
          <span class="knob"></span>
        </button>
        <span>Single Sided</span>
      </section>

      <!-- VIEWPORT (3D wired; 3D+2D / 2D are placeholders — no multi-view renderer yet) -->
      <section>
        <span class="mono-label section-label">Viewport</span>
        <div class="segmented">
          <button class="active">3D</button>
          <button disabled title="Not implemented yet">3D + 2D</button>
          <button disabled title="Not implemented yet">2D</button>
        </div>
      </section>

      <!-- RENDER -->
      <section>
        <span class="mono-label section-label">Render (1)</span>
        <button class="option-row" class:active={renderMode === "final"} on:click={() => (renderMode = "final")}>
          <span class="radio" class:checked={renderMode === "final"}></span>
          Final Render
        </button>
        <button class="option-row" class:active={renderMode === "raw"} on:click={() => (renderMode = "raw")}>
          <span class="radio" class:checked={renderMode === "raw"}></span>
          No Post-Processing
        </button>
      </section>

      <!-- MATERIAL CHANNELS -->
      <section>
        <span class="mono-label section-label">Material Channels (2)</span>
        {#each CHANNELS as channel}
          <button
            class="option-row"
            class:active={!matcap && materialChannel === channel.id}
            class:disabled={!channel.wired}
            title={channel.wired ? "" : "Not wired up yet"}
            on:click={() => pickChannel(channel)}
          >
            <span class="radio" class:checked={!matcap && materialChannel === channel.id}></span>
            {channel.label}
          </button>
        {/each}
      </section>

      <!-- GEOMETRY -->
      <section>
        <span class="mono-label section-label">Geometry (3)</span>
        <button class="option-row" class:active={matcap} on:click={toggleMatcap}>
          <span class="radio" class:checked={matcap}></span>
          Matcap
        </button>
      </section>

      <!-- SCENE (stats + camera — not in the reference screenshot, but the
           data was already being collected for the panel so it's included) -->
      <section>
        <span class="mono-label section-label">Scene</span>
        {#if stats}
          <div class="stat-grid">
            <div><span class="stat-value">{formatNumber(stats.tris)}</span><span class="mono-label">Tris</span></div>
            <div><span class="stat-value">{formatNumber(stats.verts)}</span><span class="mono-label">Verts</span></div>
            <div><span class="stat-value">{stats.meshes}</span><span class="mono-label">Meshes</span></div>
            <div><span class="stat-value">{stats.mats}</span><span class="mono-label">Mats</span></div>
            <div><span class="stat-value">{stats.tex}</span><span class="mono-label">Tex</span></div>
          </div>
        {:else}
          <span class="mono-label muted">No model loaded</span>
        {/if}
      </section>

      <section>
        <span class="mono-label section-label">Camera</span>
        {#each CAMERA_ROWS as row}
          <div class="cam-row">
            {#each row as preset}
              <button class="cam-btn mono-label" on:click={() => onCameraPreset(preset)}>{preset.toUpperCase()}</button>
            {/each}
          </div>
        {/each}
        <button class="reset-btn mono-label" on:click={onResetView}>Reset View</button>
      </section>

      {#if stats?.graph?.length}
        <section>
          <span class="mono-label section-label">Scene Graph ({stats.graph.length})</span>
          <div class="graph-list">
            {#each stats.graph as node}
              <div class="graph-row mono-label" class:mesh={node.isMesh}>{node.name}</div>
            {/each}
          </div>
        </section>
      {/if}
    </div>
  {/if}
</div>

<style>
  .inspector {
    position: absolute;
    top: 16px;
    left: 16px;
    bottom: 16px;
    width: 230px;
    display: flex;
    flex-direction: column;
    background: rgba(15, 14, 22, 0.88);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius-md);
    backdrop-filter: blur(10px);
    overflow: hidden;
    color: var(--text-primary);
    font-size: 13px;
  }

  .header {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 12px 14px;
    background: transparent;
    border: none;
    border-bottom: 1px solid var(--line);
    color: var(--text-primary);
    font-family: var(--font-display);
    font-weight: 600;
    font-size: 13px;
    text-align: left;
  }
  .chevron {
    display: inline-block;
    color: var(--text-tertiary);
    transition: transform 0.15s;
  }
  .chevron.collapsed {
    transform: rotate(-90deg);
  }

  .body {
    overflow-y: auto;
    padding: 12px 14px 16px;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  section {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .section-label {
    margin-bottom: 2px;
  }

  .muted {
    color: var(--text-tertiary);
  }

  /* Wireframe swatches */
  .swatch-row {
    display: flex;
    gap: 6px;
  }
  .w-swatch {
    width: 20px;
    height: 20px;
    border-radius: 4px;
    border: 1px solid var(--line-strong);
    background: var(--swatch);
    padding: 0;
  }
  .w-swatch.checker {
    background-image: linear-gradient(45deg, #333 25%, transparent 25%), linear-gradient(-45deg, #333 25%, transparent 25%),
      linear-gradient(45deg, transparent 75%, #333 75%), linear-gradient(-45deg, transparent 75%, #333 75%);
    background-size: 8px 8px;
    background-position: 0 0, 0 4px, 4px -4px, -4px 0;
    background-color: #1a1a22;
  }
  .w-swatch.active {
    outline: 2px solid var(--purple-300);
    outline-offset: 1px;
  }

  /* Single Sided toggle */
  .row-toggle {
    flex-direction: row;
    align-items: center;
    gap: 10px;
  }
  .switch {
    width: 32px;
    height: 18px;
    border-radius: 999px;
    background: var(--panel-raised);
    border: 1px solid var(--line-strong);
    padding: 2px;
    display: flex;
    align-items: center;
  }
  .switch .knob {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: var(--text-tertiary);
    transition: transform 0.15s, background 0.15s;
  }
  .switch.on {
    background: var(--purple-700);
    border-color: var(--purple-500);
  }
  .switch.on .knob {
    transform: translateX(14px);
    background: var(--purple-300);
  }

  /* Viewport segmented control */
  .segmented {
    display: flex;
    border: 1px solid var(--line-strong);
    border-radius: var(--radius-sm);
    overflow: hidden;
  }
  .segmented button {
    flex: 1;
    background: transparent;
    border: none;
    color: var(--text-tertiary);
    padding: 6px 4px;
    font-size: 11px;
    border-right: 1px solid var(--line);
  }
  .segmented button:last-child {
    border-right: none;
  }
  .segmented button.active {
    background: var(--purple-700);
    color: var(--text-primary);
  }
  .segmented button:disabled {
    opacity: 0.4;
    cursor: default;
  }

  /* Radio-style option rows (render mode, material channels, geometry) */
  .option-row {
    display: flex;
    align-items: center;
    gap: 8px;
    background: transparent;
    border: none;
    color: var(--text-secondary);
    padding: 4px 2px;
    font-size: 12px;
    text-align: left;
  }
  .option-row.active {
    color: var(--text-primary);
  }
  .option-row.disabled {
    opacity: 0.35;
    cursor: default;
  }
  .radio {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    border: 1px solid var(--line-strong);
    flex-shrink: 0;
  }
  .radio.checked {
    border-color: var(--purple-300);
    background: radial-gradient(circle, var(--purple-300) 0 40%, transparent 44%);
  }

  /* Scene stats */
  .stat-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 8px;
  }
  .stat-grid > div {
    background: var(--panel-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    padding: 6px 8px;
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .stat-value {
    font-family: var(--font-mono);
    font-size: 15px;
    color: var(--purple-300);
  }

  /* Camera presets */
  .cam-row {
    display: flex;
    gap: 6px;
  }
  .cam-btn {
    flex: 1;
    background: var(--panel-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    padding: 6px 0;
    color: var(--text-secondary);
    font-size: 10px;
  }
  .cam-btn:hover {
    color: var(--text-primary);
    border-color: var(--line-strong);
  }
  .reset-btn {
    margin-top: 4px;
    background: transparent;
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    padding: 6px 0;
    color: var(--text-tertiary);
    font-size: 10px;
  }
  .reset-btn:hover {
    color: var(--text-primary);
    border-color: var(--line-strong);
  }

  /* Scene graph list */
  .graph-list {
    max-height: 160px;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 1px;
  }
  .graph-row {
    padding: 2px 4px;
    color: var(--text-tertiary);
    font-size: 10px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .graph-row.mesh {
    color: var(--text-secondary);
  }
</style>