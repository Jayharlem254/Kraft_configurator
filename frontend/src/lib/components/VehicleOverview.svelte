<script>
  import Viewer3D from "./Viewer3D.svelte";

  export let vehicle;
  export let onProceed = () => {};
  export let onPrev = () => {};
  export let onNext = () => {};
  export let hasPrev = false;
  export let hasNext = false;

  let activeCategoryId = null;
  let previousVehicleSlug = null;

  $: if (vehicle?.slug !== previousVehicleSlug) {
    previousVehicleSlug = vehicle?.slug ?? null;
    activeCategoryId = vehicle?.option_categories?.[0]?.id ?? null;
  }
  $: activeCategory = vehicle?.option_categories?.find((c) => c.id === activeCategoryId);
  $: colorCategory = vehicle?.option_categories?.find((c) => c.choices?.some((ch) => ch.swatch_hex));
  $: highlightText = vehicle?.description || vehicle?.tagline;

  function selectCategory(id) {
    activeCategoryId = id;
  }

  function formatPrice(value) {
    return new Intl.NumberFormat("en-US", {
      style: "currency",
      currency: "USD",
      maximumFractionDigits: 0,
    }).format(Number(value));
  }
</script>

<div class="overview">
  <div class="stage">
    <Viewer3D glbUrl={vehicle.model_file} accentColor="#00d2ff" />

    <!-- Quni4m Tactile Navigation Arrows -->
    <button class="nav-arrow prev raised" on:click={onPrev} disabled={!hasPrev} aria-label="Previous model">‹</button>
    <button class="nav-arrow next raised" on:click={onNext} disabled={!hasNext} aria-label="Next model">›</button>

    {#if highlightText}
      <div class="highlight-card milled">
        <div class="row card-header">
          <span class="dot-led"></span>
          <span class="mono-label">PLATFORM TELEMETRY</span>
        </div>
        <p>{highlightText}</p>
      </div>
    {/if}
  </div>

  <aside class="sidebar">
    <div class="header-unit">
      <div class="row step-row">
        <span class="dot-led"></span>
        <span class="mono-label step">STAGE 01 // OVERVIEW SPEC</span>
      </div>
      <h1 class="vehicle-title">{vehicle.name}</h1>
      <p class="tagline">{vehicle.tagline}</p>
    </div>

    {#if vehicle.option_categories?.length}
      <nav class="pills milled">
        {#each vehicle.option_categories as cat}
          <button
            class="pill mono"
            class:active={cat.id === activeCategoryId}
            on:click={() => selectCategory(cat.id)}
          >
            {cat.name}
          </button>
        {/each}
      </nav>

      {#if activeCategory}
        <div class="category-preview milled">
          <span class="mono-label count">
            {activeCategory.choices.length} AVAILABLE OPTION{activeCategory.choices.length === 1 ? "" : "S"}
          </span>
          <ul>
            {#each activeCategory.choices as choice}
              <li>
                <span class="choice-title">{choice.name}</span>
                <span class="mono-label delta">
                  {choice.price_delta > 0 ? `+${formatPrice(choice.price_delta)}` : "INCLUDED"}
                </span>
              </li>
            {/each}
          </ul>
        </div>
      {/if}
    {/if}

    <div class="grow"></div>

    <button class="cta raised" on:click={onProceed}>
      <span class="cta-text mono">ENTER 3D CONFIGURATOR →</span>
    </button>
  </aside>

  <!-- Machined Bottom Highlights Deck -->
  <div class="highlights milled">
    <div class="stat">
      <span class="mono-label">BASE PRICE</span>
      <span class="value price mono">{formatPrice(vehicle.base_price)}</span>
    </div>
    <div class="stat">
      <span class="mono-label">PLATFORM TYPE</span>
      <span class="value type mono">{vehicle.vehicle_type?.toUpperCase()}</span>
    </div>
    <div class="stat">
      <span class="mono-label">MODULAR GROUPS</span>
      <span class="value mono">{vehicle.option_categories?.length ?? 0}</span>
    </div>
    {#if colorCategory}
      <div class="stat colors">
        <span class="mono-label">CORE FINISHES</span>
        <div class="swatch-row">
          {#each colorCategory.choices as choice}
            <span class="swatch" style="--swatch-color: {choice.swatch_hex}" title={choice.name}></span>
          {/each}
        </div>
      </div>
    {/if}
  </div>
</div>

<style>
  .overview {
    display: grid;
    grid-template-columns: 1fr 360px;
    grid-template-rows: 1fr auto;
    gap: 16px;
    height: 100%;
    min-height: 0;
  }

  .stage {
    position: relative;
    grid-column: 1;
    grid-row: 1;
    min-height: 0;
    border: 1px solid var(--line);
    border-radius: var(--radius-md);
    overflow: hidden;
    background: var(--canvas);
  }

  .dot-led {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--crystal-blue);
    box-shadow: 0 0 6px var(--crystal-blue);
  }

  .nav-arrow {
    position: absolute;
    bottom: 20px;
    width: 38px;
    height: 38px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
    color: var(--text-hi);
    font-size: 20px;
    display: grid;
    place-items: center;
    line-height: 1;
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .nav-arrow:hover:not(:disabled) {
    border-color: var(--crystal-blue);
    color: var(--crystal-bright);
    box-shadow: 0 0 10px var(--crystal-glow);
  }
  .nav-arrow:disabled {
    opacity: 0.25;
    cursor: default;
  }
  .nav-arrow.prev { left: 20px; }
  .nav-arrow.next { right: 20px; }

  .highlight-card {
    position: absolute;
    top: 20px;
    right: 20px;
    max-width: 280px;
    border-radius: var(--radius);
    padding: 12px 14px;
    border: 1px solid var(--line-soft);
    backdrop-filter: blur(8px);
  }
  .card-header {
    gap: 6px;
    margin-bottom: 6px;
  }
  .highlight-card .mono-label {
    color: var(--crystal-bright);
    font-size: 9px;
  }
  .highlight-card p {
    margin: 0;
    font-size: 11px;
    line-height: 1.45;
    color: var(--text-dim);
  }

  .sidebar {
    grid-column: 2;
    grid-row: 1 / span 2;
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-md);
    padding: 20px;
    display: flex;
    flex-direction: column;
    gap: 16px;
    overflow-y: auto;
    min-height: 0;
  }

  .header-unit {
    display: flex;
    flex-direction: column;
    gap: 4px;
  }

  .step-row {
    gap: 6px;
  }

  .step {
    color: var(--crystal-bright);
    font-size: 9px;
  }

  .vehicle-title {
    font-size: 22px;
    color: var(--text-hi);
  }

  .tagline {
    font-size: 11px;
    color: var(--text-dim);
    margin: 0;
  }

  .pills {
    display: flex;
    flex-wrap: wrap;
    gap: 4px;
    padding: 4px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .pill {
    padding: 4px 8px;
    border-radius: var(--radius-sm);
    background: transparent;
    border: none;
    color: var(--text-dim);
    font-size: 10px;
    cursor: pointer;
    transition: all 0.12s ease;
  }
  .pill:hover {
    color: var(--text-hi);
  }
  .pill.active {
    background: var(--surface-hi);
    color: var(--crystal-bright);
    box-shadow: 0 0 6px var(--crystal-dim);
    font-weight: 600;
  }

  .category-preview {
    border-radius: var(--radius);
    padding: 12px;
    border: 1px solid var(--line-soft);
  }

  .count {
    color: var(--crystal-bright);
    font-size: 9px;
  }

  ul {
    list-style: none;
    padding: 0;
    margin: 8px 0 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  li {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 11px;
    padding: 4px 0;
    border-bottom: 1px solid var(--line);
  }
  li:last-child {
    border-bottom: none;
  }

  .choice-title {
    color: var(--text-hi);
  }

  .delta {
    font-size: 10px;
    color: var(--text-dim);
  }

  .cta {
    width: 100%;
    padding: 11px 16px;
    border-radius: var(--radius);
    border: 1px solid var(--crystal-deep);
    background: linear-gradient(180deg, #012b48 0%, #011320 100%);
    color: var(--crystal-bright);
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .cta:hover {
    border-color: var(--crystal-blue);
    box-shadow: 0 0 14px var(--crystal-glow);
    color: #ffffff;
  }
  .cta:active {
    transform: var(--press-shift);
  }

  .cta-text {
    font-weight: 700;
    letter-spacing: 0.08em;
  }

  .highlights {
    grid-column: 1;
    grid-row: 2;
    display: flex;
    gap: 24px;
    align-items: center;
    padding: 12px 20px;
    border-radius: var(--radius-md);
    border: 1px solid var(--line);
  }

  .stat {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .stat .mono-label {
    font-size: 9px;
    color: var(--text-dim);
  }

  .value {
    font-size: 13px;
    font-weight: 600;
    color: var(--text-hi);
  }

  .value.price {
    color: var(--crystal-bright);
    font-size: 15px;
    font-weight: 700;
  }

  .value.type {
    color: var(--crystal-blue);
  }

  .swatch-row {
    display: flex;
    gap: 5px;
    margin-top: 2px;
  }

  .swatch {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    background: var(--swatch-color);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.5);
    border: 1px solid rgba(255, 255, 255, 0.2);
  }
</style>