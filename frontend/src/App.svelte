<script>
  import { onMount } from "svelte";
  import { api } from "./lib/api.js";
  import { mockVehicle, mockVehicleList } from "./lib/mockData.js";
  import { effectiveSelections } from "./lib/selections.js";
  import TopBar from "./lib/components/TopBar.svelte";
  import StatusBar from "./lib/components/StatusBar.svelte";
  import VehicleGallery from "./lib/components/VehicleGallery.svelte";
  import VehicleOverview from "./lib/components/VehicleOverview.svelte";
  import Viewer3D from "./lib/components/Viewer3D.svelte";
  import ConfiguratorPanel from "./lib/components/ConfiguratorPanel.svelte";
  import ModelUploader from "./lib/components/ModelUploader.svelte";

  let view = "gallery"; // "gallery" | "overview" | "configurator" | "upload"
  let vehicleType = "all";
  let vehicles = [];
  let galleryLoading = true;
  let vehicle = null;
  let selections = {};
  let loadError = false;

  let engineOnline = false;
  let enginePing = 1;
  let healthInterval = null;

  // Dev/QA-only Model Inspector — opt in with ?inspector=1
  const inspectorMode = typeof window !== "undefined" && new URLSearchParams(window.location.search).has("inspector");

  async function checkEngine() {
    const res = await api.checkHealth();
    engineOnline = res.online;
    enginePing = res.ping;
  }

  async function loadGallery(type) {
    galleryLoading = true;
    try {
      const list = await api.listVehicles(type === "all" ? "" : type);
      vehicles = list.results ?? list ?? [];
      loadError = false;
      engineOnline = true;
    } catch (err) {
      console.warn("Falling back to embedded vehicle catalog — connect C++ backend:", err);
      vehicles =
        type === "all" ? mockVehicleList : mockVehicleList.filter((v) => v.vehicle_type === type);
      loadError = true;
    }
    galleryLoading = false;
  }

  function selectType(type) {
    vehicleType = type;
    loadGallery(type);
  }

  async function openOverview(summary) {
    selections = {};
    try {
      vehicle = await api.getVehicle(summary.slug);
      loadError = false;
      engineOnline = true;
    } catch (err) {
      console.warn("Falling back to embedded vehicle detail — connect C++ backend:", err);
      vehicle = mockVehicle;
      loadError = true;
    }
    view = "overview";
  }

  function proceedToConfigurator() {
    view = "configurator";
  }

  async function handleModelUploaded(uploadedVehicle) {
    await loadGallery(vehicleType);
    if (uploadedVehicle) {
      vehicle = uploadedVehicle;
    }
  }

  function handleOpenConfiguratorFromUpload(v) {
    vehicle = v;
    selections = {};
    view = "configurator";
  }

  function handleOpenSpecsFromUpload(v) {
    vehicle = v;
    selections = {};
    view = "overview";
  }

  function handleNav(targetView) {
    if ((targetView === "configurator" || targetView === "overview") && !vehicle) {
      if (vehicles.length > 0) {
        openOverview(vehicles[0]);
      } else {
        view = "upload";
        return;
      }
    }
    view = targetView;
  }

  $: currentIndex = vehicle ? vehicles.findIndex((v) => v.slug === vehicle.slug) : -1;
  $: hasPrev = currentIndex > 0;
  $: hasNext = currentIndex >= 0 && currentIndex < vehicles.length - 1;

  function goPrev() {
    if (hasPrev) openOverview(vehicles[currentIndex - 1]);
  }

  function goNext() {
    if (hasNext) openOverview(vehicles[currentIndex + 1]);
  }

  $: effective = vehicle ? effectiveSelections(vehicle, selections) : {};
  $: totalPrice = vehicle
    ? Number(vehicle.base_price || 0) +
      Object.entries(effective).reduce((sum, [catId, choiceId]) => {
        const cat = vehicle.option_categories?.find((c) => c.id === Number(catId));
        const choice = cat?.choices?.find((c) => c.id === choiceId);
        return sum + Number(choice?.price_delta || 0);
      }, 0)
    : 0;

  onMount(() => {
    checkEngine();
    loadGallery(vehicleType);
    healthInterval = setInterval(checkEngine, 8000);
    return () => clearInterval(healthInterval);
  });
</script>

<div class="layout">
  <!-- Quni4m Tactile TopBar in Crystal Blue -->
  <TopBar
    {view}
    {vehicleType}
    activeVehicle={vehicle}
    {engineOnline}
    {enginePing}
    on:nav={(e) => handleNav(e.detail)}
    on:type={(e) => selectType(e.detail)}
  />

  <main class:overview-layout={view === "overview"} class:configurator-layout={view === "configurator"}>
    {#if view === "gallery"}
      <VehicleGallery
        {vehicles}
        loading={galleryLoading}
        onSelect={openOverview}
        onUpload={() => (view = "upload")}
      />
    {:else if view === "upload"}
      <ModelUploader
        {vehicles}
        activeVehicle={vehicle}
        on:uploaded={(e) => handleModelUploaded(e.detail)}
        on:openConfigurator={(e) => handleOpenConfiguratorFromUpload(e.detail)}
        on:openSpecs={(e) => handleOpenSpecsFromUpload(e.detail)}
        on:close={() => (view = "gallery")}
      />
    {:else if view === "overview" && vehicle}
      <VehicleOverview
        {vehicle}
        onProceed={proceedToConfigurator}
        onPrev={goPrev}
        onNext={goNext}
        {hasPrev}
        {hasNext}
      />
    {:else if view === "configurator" && vehicle}
      <div class="viewer-pane">
        <Viewer3D
          glbUrl={vehicle.model_file}
          {vehicle}
          {selections}
          accentColor="#00d2ff"
          inspector={inspectorMode}
        />
      </div>
      <ConfiguratorPanel {vehicle} bind:selections />
    {:else}
      <div class="loading-box milled">
        <span class="mono loading">INITIALIZING C++ CONFIGURATION ENGINE…</span>
      </div>
    {/if}
  </main>

  {#if !engineOnline && loadError}
    <div class="dev-banner mono-label">
      <span class="dot-warn"></span>
      EMBEDDED PREVIEW MODE — RUN "run.bat" OR "kraft_backend.exe" FOR HIGH-SPEED C++ REST & SQLITE
    </div>
  {/if}

  <!-- Quni4m Tactile StatusBar in Crystal Blue -->
  <StatusBar
    {vehicle}
    {totalPrice}
    {engineOnline}
    selectedCount={Object.keys(selections).length}
  />
</div>

<style>
  .layout {
    display: flex;
    flex-direction: column;
    height: 100vh;
    width: 100vw;
    background: var(--bg);
    overflow: hidden;
  }

  main {
    flex: 1;
    min-height: 0;
    overflow-y: auto;
    position: relative;
  }

  main.overview-layout {
    overflow: hidden;
    padding: 12px;
  }

  main.configurator-layout {
    display: grid;
    grid-template-columns: 1fr 390px;
    height: 100%;
    overflow: hidden;
  }

  .viewer-pane {
    position: relative;
    height: 100%;
    min-height: 0;
    background: var(--canvas);
  }

  .loading-box {
    margin: 120px auto;
    max-width: 400px;
    padding: 24px;
    text-align: center;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .loading {
    color: var(--crystal-bright);
    letter-spacing: 0.1em;
    font-size: 11px;
  }

  .dev-banner {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    padding: 4px 10px;
    background: var(--surface);
    border-top: 1px solid var(--line-soft);
    color: var(--crystal-bright);
    font-size: 10px;
    flex-shrink: 0;
  }

  .dot-warn {
    width: 5px;
    height: 5px;
    border-radius: 50%;
    background: var(--led-amber);
    box-shadow: 0 0 4px var(--led-amber);
  }

  @media (max-width: 900px) {
    main.configurator-layout {
      grid-template-columns: 1fr;
      grid-template-rows: 55vh 45vh;
    }
  }
</style>