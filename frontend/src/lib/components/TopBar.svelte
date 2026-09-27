<script>
  import { createEventDispatcher } from "svelte";
  import ModeSwitch from "./controls/ModeSwitch.svelte";

  export let view = "gallery"; // "gallery" | "overview" | "configurator"
  export let vehicleType = "all";
  export let activeVehicle = null;
  export let engineOnline = false;
  export let enginePing = 1;

  const dispatch = createEventDispatcher();

  const viewOptions = [
    { id: "gallery", label: "CATALOG" },
    { id: "overview", label: "SPECS" },
    { id: "configurator", label: "3D STUDIO" },
    { id: "upload", label: "UPLOAD 3D" },
  ];

  const types = [
    { id: "all", label: "ALL" },
    { id: "car", label: "CARS" },
    { id: "boat", label: "BOATS" },
    { id: "bike", label: "BIKES" },
  ];
</script>

<header class="topbar">
  <!-- Brand Badge (Quni4m tactile badge with crystal blue LED) -->
  <button
    type="button"
    class="brand-cluster row"
    on:click={() => dispatch("nav", "gallery")}
    title="Return to Catalog"
  >
    <div class="crystal-gem"></div>
    <div class="brand-text">
      <span class="brand-name">KRAFT</span>
      <span class="brand-sub mono">SYS.V2</span>
    </div>
  </button>

  <div class="sep"></div>

  <!-- Primary View Mode Switch (Quni4m style) -->
  <ModeSwitch
    options={viewOptions}
    value={view}
    on:change={(e) => dispatch("nav", e.detail)}
  />

  {#if view === "gallery"}
    <div class="sep"></div>
    <div class="type-chips row">
      {#each types as t}
        <button
          class="chip"
          class:on={vehicleType === t.id}
          on:click={() => dispatch("type", t.id)}
        >
          {t.label}
        </button>
      {/each}
    </div>
  {/if}

  {#if activeVehicle}
    <div class="sep"></div>
    <div class="vehicle-tag row">
      <span class="dim mono">ACTIVE:</span>
      <span class="vehicle-title mono">{activeVehicle.name}</span>
      <span class="vehicle-slug mono dim">[{activeVehicle.slug}]</span>
    </div>
  {/if}

  <div class="grow"></div>

  <!-- C++ Backend Engine Telemetry Badge (Emulating Quni4m engine status) -->
  <div class="engine-badge row" title={engineOnline ? "C++ Engine Live (cpp-httplib/SQLite)" : "Connecting to C++ Backend..."}>
    <span class="led-dot" class:online={engineOnline}></span>
    <span class="mono engine-text">{engineOnline ? "C++ ENGINE LIVE" : "DISCONNECTED"}</span>
    {#if engineOnline}
      <span class="mono ping-text dim">{enginePing}ms</span>
    {/if}
  </div>

  <div class="sep"></div>

  <!-- Action chips -->
  <button
    class="tb"
    title="Toggle Fullscreen"
    on:click={() => {
      if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen();
      } else {
        document.exitFullscreen();
      }
    }}
  >
    <svg width="12" height="12" viewBox="0 0 14 14" fill="currentColor">
      <path d="M1 1h4v1.5H2.5v2.5H1V1zm8 0h4v4h-1.5V2.5H9V1zM1 9h1.5v2.5H5V13H1V9zm11.5 2.5V9H13v4H9v-1.5h2.5z" />
    </svg>
  </button>
</header>

<style>
  .topbar {
    display: flex;
    align-items: center;
    gap: 8px;
    height: 38px;
    padding: 0 10px;
    background: var(--panel);
    border-bottom: 1px solid var(--line);
    flex-shrink: 0;
    z-index: 20;
  }

  .brand-cluster {
    gap: 7px;
    cursor: pointer;
    background: transparent;
    border: none;
    padding: 0;
  }

  .crystal-gem {
    width: 14px;
    height: 14px;
    background: linear-gradient(135deg, var(--crystal-bright), var(--crystal-deep));
    border-radius: 3px;
    box-shadow: 0 0 10px var(--crystal-glow);
    transform: rotate(45deg);
    flex-shrink: 0;
  }

  .brand-text {
    display: flex;
    align-items: baseline;
    gap: 4px;
  }

  .brand-name {
    font-family: var(--font-display);
    font-weight: 800;
    font-size: 14px;
    letter-spacing: 0.05em;
    color: var(--text-hi);
  }

  .brand-sub {
    font-size: 9px;
    color: var(--crystal-blue);
    font-weight: 600;
  }

  .type-chips {
    gap: 4px;
  }

  .vehicle-tag {
    gap: 6px;
    font-size: 11px;
    background: var(--surface);
    padding: 2px 7px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .vehicle-title {
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .engine-badge {
    gap: 6px;
    padding: 3px 8px;
    background: var(--field);
    border: 1px solid var(--line-soft);
    border-radius: var(--radius);
    font-size: 10px;
  }

  .led-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--led-red);
    box-shadow: 0 0 4px var(--led-red);
  }

  .led-dot.online {
    background: var(--crystal-blue);
    box-shadow: 0 0 6px var(--crystal-blue);
  }

  .engine-text {
    font-size: 10px;
    letter-spacing: 0.05em;
    color: var(--text);
  }

  .ping-text {
    font-size: 9px;
  }
</style>
