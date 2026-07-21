<script>
  import { onMount } from "svelte";
  import { api } from "./lib/api.js";
  import { mockVehicle, mockVehicleList } from "./lib/mockData.js";
  import Header from "./lib/components/Header.svelte";
  import VehicleGallery from "./lib/components/VehicleGallery.svelte";
  import VehicleOverview from "./lib/components/VehicleOverview.svelte";
  import Viewer3D from "./lib/components/Viewer3D.svelte";
  import ConfiguratorPanel from "./lib/components/ConfiguratorPanel.svelte";

  let view = "gallery"; // "gallery" | "overview" | "configurator"
  let vehicleType = "all";
  let vehicles = [];
  let galleryLoading = true;
  let vehicle = null;
  let selections = {};
  let loadError = false;

  async function loadGallery(type) {
    galleryLoading = true;
    try {
      const list = await api.listVehicles(type === "all" ? "" : type);
      vehicles = list.results ?? list ?? [];
      loadError = false;
    } catch (err) {
      console.warn("Falling back to mock catalog — is the Django API running and seeded?", err);
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
    } catch (err) {
      console.warn("Falling back to mock vehicle detail — is the Django API running?", err);
      vehicle = mockVehicle;
      loadError = true;
    }
    view = "overview";
  }

  function proceedToConfigurator() {
    view = "configurator";
  }

  function goBack() {
    if (view === "configurator") {
      view = "overview";
    } else {
      view = "gallery";
      vehicle = null;
    }
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

  onMount(() => loadGallery(vehicleType));
</script>

<div class="layout">
  <Header
    mode={view}
    {vehicleType}
    onSelectType={selectType}
    activeVehicleName={vehicle?.name ?? ""}
    onBack={goBack}
  />

  <main class:overview-layout={view === "overview"} class:configurator-layout={view === "configurator"}>
    {#if view === "gallery"}
      <VehicleGallery {vehicles} loading={galleryLoading} onSelect={openOverview} />
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
        <Viewer3D glbUrl={vehicle.model_file} {vehicle} {selections} />
      </div>
      <ConfiguratorPanel {vehicle} bind:selections />
    {:else}
      <p class="mono-label loading">LOADING VEHICLE…</p>
    {/if}
  </main>

  {#if loadError}
    <div class="dev-banner mono-label">
      Showing demo data — connect the Django API (and seed vehicles) to replace this.
    </div>
  {/if}
</div>

<style>
  .layout {
    display: flex;
    flex-direction: column;
    height: 100vh;
  }

  main {
    flex: 1;
    padding: 20px;
    min-height: 0;
    overflow-y: auto;
  }

  main.overview-layout {
    overflow: hidden;
  }

  main.configurator-layout {
    display: grid;
    grid-template-columns: 1fr 380px;
    gap: 20px;
    overflow: hidden;
  }

  .viewer-pane {
    min-height: 0;
  }

  .loading {
    margin: auto;
  }

  .dev-banner {
    text-align: center;
    padding: 8px;
    background: var(--panel);
    border-top: 1px solid var(--line);
    color: var(--text-tertiary);
  }

  @media (max-width: 900px) {
    main.overview-layout {
      overflow-y: auto;
    }

    main.configurator-layout {
      grid-template-columns: 1fr;
      grid-template-rows: 50vh auto;
    }
  }
</style>