<script>
  import Viewer3D from "./Viewer3D.svelte";

  export let vehicle;
  export let onProceed = () => {};
  export let onPrev = () => {};
  export let onNext = () => {};
  export let hasPrev = false;
  export let hasNext = false;

  let activeCategoryId = null;

  // Default to the first option category whenever the vehicle changes.
  $: if (vehicle?.option_categories?.length && activeCategoryId === null) {
    activeCategoryId = vehicle.option_categories[0].id;
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
    <Viewer3D glbUrl={vehicle.model_file} />

    <button class="nav-arrow prev" on:click={onPrev} disabled={!hasPrev} aria-label="Previous model">‹</button>
    <button class="nav-arrow next" on:click={onNext} disabled={!hasNext} aria-label="Next model">›</button>

    {#if highlightText}
      <div class="highlight-card">
        <span class="mono-label">OVERVIEW</span>
        <p>{highlightText}</p>
      </div>
    {/if}
  </div>

  <aside class="sidebar">
    <div>
      <span class="mono-label step">STEP 2 // OVERVIEW</span>
      <h1>{vehicle.name}</h1>
      <p class="tagline">{vehicle.tagline}</p>
    </div>

    {#if vehicle.option_categories?.length}
      <nav class="pills">
        {#each vehicle.option_categories as cat}
          <button
            class="pill mono-label"
            class:active={cat.id === activeCategoryId}
            on:click={() => selectCategory(cat.id)}
          >
            {cat.name}
          </button>
        {/each}
      </nav>

      {#if activeCategory}
        <div class="category-preview">
          <span class="mono-label count">
            {activeCategory.choices.length} OPTION{activeCategory.choices.length === 1 ? "" : "S"}
          </span>
          <ul>
            {#each activeCategory.choices as choice}
              <li>
                <span>{choice.name}</span>
                <span class="mono-label delta">
                  {choice.price_delta > 0 ? `+${formatPrice(choice.price_delta)}` : "Included"}
                </span>
              </li>
            {/each}
          </ul>
        </div>
      {/if}
    {/if}

    <button class="cta" on:click={onProceed}>ENTER CONFIGURATOR →</button>
  </aside>

  <div class="highlights">
    <div class="stat">
      <span class="mono-label">BASE PRICE</span>
      <span class="value">{formatPrice(vehicle.base_price)}</span>
    </div>
    <div class="stat">
      <span class="mono-label">TYPE</span>
      <span class="value type">{vehicle.vehicle_type}</span>
    </div>
    <div class="stat">
      <span class="mono-label">OPTION GROUPS</span>
      <span class="value">{vehicle.option_categories?.length ?? 0}</span>
    </div>
    {#if colorCategory}
      <div class="stat colors">
        <span class="mono-label">COLORS</span>
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
    grid-template-columns: 1fr 340px;
    grid-template-rows: 1fr auto;
    gap: 20px;
    height: 100%;
    min-height: 0;
  }

  .stage {
    position: relative;
    grid-column: 1;
    grid-row: 1;
    min-height: 0;
  }

  .nav-arrow {
    position: absolute;
    bottom: 20px;
    width: 40px;
    height: 40px;
    border-radius: 50%;
    background: rgba(17, 16, 24, 0.75);
    border: 1px solid var(--line-strong);
    color: var(--text-primary);
    font-size: 18px;
    line-height: 1;
    backdrop-filter: blur(6px);
    transition: border-color 0.15s, color 0.15s, opacity 0.15s;
  }
  .nav-arrow:hover:not(:disabled) {
    border-color: var(--purple-500);
    color: var(--purple-300);
  }
  .nav-arrow:disabled {
    opacity: 0.3;
    cursor: default;
  }
  .nav-arrow.prev {
    left: 20px;
  }
  .nav-arrow.next {
    right: 20px;
  }

  .highlight-card {
    position: absolute;
    top: 20px;
    right: 20px;
    max-width: 260px;
    background: rgba(17, 16, 24, 0.85);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius-md);
    padding: 14px 16px;
    backdrop-filter: blur(6px);
  }
  .highlight-card .mono-label {
    color: var(--purple-300);
  }
  .highlight-card p {
    margin: 8px 0 0;
    font-size: 13px;
    line-height: 1.5;
    color: var(--text-secondary);
  }

  .sidebar {
    grid-column: 2;
    grid-row: 1 / span 2;
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-lg);
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 20px;
    overflow-y: auto;
    min-height: 0;
  }

  .step {
    color: var(--text-tertiary);
  }
  .sidebar h1 {
    margin-top: 8px;
    font-size: 24px;
  }
  .tagline {
    margin: 8px 0 0;
    font-size: 13px;
    color: var(--text-secondary);
    line-height: 1.5;
  }

  .pills {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
  }
  .pill {
    background: var(--panel-raised);
    border: 1px solid var(--line);
    color: var(--text-secondary);
    padding: 8px 14px;
    border-radius: 999px;
    transition: border-color 0.15s, color 0.15s;
  }
  .pill:hover {
    color: var(--text-primary);
  }
  .pill.active {
    border-color: var(--purple-500);
    color: var(--purple-300);
    background: var(--panel);
  }

  .category-preview {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }
  .count {
    color: var(--text-tertiary);
  }
  .category-preview ul {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .category-preview li {
    display: flex;
    justify-content: space-between;
    font-size: 13px;
    padding: 8px 10px;
    background: var(--panel-raised);
    border-radius: var(--radius-sm);
    border: 1px solid var(--line);
  }
  .delta {
    color: var(--text-tertiary);
  }

  .cta {
    margin-top: auto;
    background: var(--gradient-metal);
    color: var(--text-primary);
    border: none;
    border-radius: var(--radius-md);
    padding: 14px;
    font-size: 14px;
    font-weight: 500;
    letter-spacing: 0.02em;
  }
  .cta:hover {
    filter: brightness(1.1);
  }

  .highlights {
    grid-column: 1;
    grid-row: 2;
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
    gap: 14px;
  }
  .stat {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-md);
    padding: 14px 16px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .value {
    font-family: var(--font-mono);
    font-size: 18px;
    color: var(--text-primary);
  }
  .value.type {
    text-transform: uppercase;
  }
  .colors {
    grid-column: span 2;
  }
  .swatch-row {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .swatch {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: var(--swatch-color);
    border: 2px solid var(--line);
  }

  @media (max-width: 900px) {
    .overview {
      grid-template-columns: 1fr;
      grid-template-rows: 50vh auto auto;
      height: auto;
      min-height: 100%;
    }
    .sidebar {
      grid-column: 1;
      grid-row: 2;
    }
    .highlights {
      grid-row: 3;
    }
  }
</style>