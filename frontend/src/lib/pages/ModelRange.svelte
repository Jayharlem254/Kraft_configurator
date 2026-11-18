<script>
  import { api } from "../api.js";
  import { classLabel, formatSpec, headlineSpecs, isElectric, money, specLabel, TYPE_LABELS } from "../format.js";
  import { href, navigate } from "../router.svelte.js";
  import Icon from "../components/Icon.svelte";
  import VehicleThumb from "../components/VehicleThumb.svelte";

  let { type = "" } = $props();

  let vehicles = $state([]);
  let loading = $state(true);
  let error = $state(null);
  let query = $state("");
  let classes = $state(new Set());
  let power = $state("all"); // all | combustion | electric
  let maxPrice = $state(0);
  let sort = $state("featured");
  let filtersOpen = $state(false);
  let drawer = $state(null);
  let compare = $state(loadCompare());

  async function load() {
    loading = true;
    try {
      vehicles = await api.listVehicles();
      error = null;
    } catch (e) {
      error = e.message;
    }
    loading = false;
  }
  load();

  // Reset class filters when switching tabs.
  $effect(() => {
    type;
    classes = new Set();
  });

  const inType = $derived(type ? vehicles.filter((v) => v.vehicle_type === type) : vehicles);
  const priceCeiling = $derived(Math.max(0, ...inType.map((v) => v.from_price ?? 0)));
  const classCounts = $derived.by(() => {
    const m = new Map();
    for (const v of inType) m.set(v.body_class || "other", (m.get(v.body_class || "other") ?? 0) + 1);
    return [...m.entries()].sort((a, b) => classLabel(a[0]).localeCompare(classLabel(b[0])));
  });
  const typeCounts = $derived.by(() => {
    const m = { car: 0, bike: 0, boat: 0 };
    for (const v of vehicles) m[v.vehicle_type] = (m[v.vehicle_type] ?? 0) + 1;
    return m;
  });

  const filtered = $derived.by(() => {
    const q = query.trim().toLowerCase();
    let list = inType.filter((v) => {
      if (classes.size && !classes.has(v.body_class || "other")) return false;
      if (power === "electric" && !isElectric(v)) return false;
      if (power === "combustion" && isElectric(v)) return false;
      if (maxPrice && v.from_price > maxPrice) return false;
      if (q && !`${v.name} ${v.family} ${v.body_class} ${v.tagline}`.toLowerCase().includes(q)) return false;
      return true;
    });
    const defSpecs = (v) => (v.variants?.find((x) => x.is_default) ?? v.variants?.[0])?.specs ?? {};
    if (sort === "price-asc") list = [...list].sort((a, b) => a.from_price - b.from_price);
    else if (sort === "price-desc") list = [...list].sort((a, b) => b.from_price - a.from_price);
    else if (sort === "power") list = [...list].sort((a, b) => (defSpecs(b).power_hp ?? 0) - (defSpecs(a).power_hp ?? 0));
    else if (sort === "name") list = [...list].sort((a, b) => a.name.localeCompare(b.name));
    return list;
  });

  // Group by vehicle type when showing "All", like a model overview page.
  const groups = $derived.by(() => {
    if (type || sort !== "featured") return [{ key: type || "all", label: "", items: filtered }];
    return ["car", "bike", "boat"]
      .map((t) => ({ key: t, label: TYPE_LABELS[t], items: filtered.filter((v) => v.vehicle_type === t) }))
      .filter((g) => g.items.length);
  });

  function toggleClass(c) {
    const next = new Set(classes);
    next.has(c) ? next.delete(c) : next.add(c);
    classes = next;
  }

  function clearFilters() {
    classes = new Set();
    power = "all";
    maxPrice = 0;
    query = "";
  }

  function loadCompare() {
    try {
      return JSON.parse(sessionStorage.getItem("kraft-compare") ?? "[]");
    } catch {
      return [];
    }
  }
  function saveCompare(list) {
    compare = list;
    try {
      sessionStorage.setItem("kraft-compare", JSON.stringify(list));
    } catch {
      /* ignore */
    }
  }
  function toggleCompare(v, variant) {
    const key = `${v.slug}:${variant.code}`;
    if (compare.some((c) => c.key === key)) saveCompare(compare.filter((c) => c.key !== key));
    else if (compare.length < 4) saveCompare([...compare, { key, name: variant.name, slug: v.slug }]);
  }

  const defaultVariant = (v) => v.variants?.find((x) => x.is_default) ?? v.variants?.[0];
  const activeFilters = $derived(classes.size + (power !== "all" ? 1 : 0) + (maxPrice ? 1 : 0));

  function onKey(e) {
    if (e.key === "Escape") drawer = null;
  }
</script>

<svelte:window onkeydown={onKey} />

<section class="hero">
  <div class="container">
    <p class="eyebrow">Kraft Configurator</p>
    <h1 class="display">Choose your vehicle.</h1>
    <p class="lead muted">
      Cars, motorcycles and boats in every class — configured in real-time 3D, priced by the Kraft engine.
    </p>
  </div>
</section>

<div class="container">
  <div class="tabs" role="tablist">
    <a class="tab" class:active={!type} href="#/" role="tab" aria-selected={!type}>
      All models <span class="count">{vehicles.length}</span>
    </a>
    {#each ["car", "bike", "boat"] as t}
      <a class="tab" class:active={type === t} href={`#/models/${t}`} role="tab" aria-selected={type === t}>
        {TYPE_LABELS[t]} <span class="count">{typeCounts[t] ?? 0}</span>
      </a>
    {/each}
  </div>

  <div class="layout">
    <aside class="filters" class:open={filtersOpen} aria-label="Filters">
      <div class="filters-head">
        <h2 class="h3">Filter</h2>
        {#if activeFilters}
          <button class="link small" onclick={clearFilters}>Reset all</button>
        {/if}
        <button class="icon-btn close-filters" aria-label="Close filters" onclick={() => (filtersOpen = false)}>
          <Icon name="close" />
        </button>
      </div>

      <div class="search">
        <Icon name="search" size={18} />
        <input class="input" placeholder="Search models" bind:value={query} aria-label="Search models" />
      </div>

      <fieldset>
        <legend>Body type</legend>
        {#each classCounts as [c, n]}
          <label class="check">
            <input type="checkbox" checked={classes.has(c)} onchange={() => toggleClass(c)} />
            <span class="box"><Icon name="check" size={14} stroke={2.4} /></span>
            <span class="lbl">{classLabel(c)}</span>
            <span class="subtle small">{n}</span>
          </label>
        {/each}
      </fieldset>

      <fieldset>
        <legend>Powertrain</legend>
        <div class="seg">
          {#each [["all", "All"], ["combustion", "Combustion"], ["electric", "Electric"]] as [val, lbl]}
            <button class:active={power === val} onclick={() => (power = val)}>{lbl}</button>
          {/each}
        </div>
      </fieldset>

      {#if priceCeiling > 0}
        <fieldset>
          <legend>Max. starting price</legend>
          <input
            type="range"
            min="0"
            max={priceCeiling}
            step={Math.max(100, Math.round(priceCeiling / 100))}
            value={maxPrice || priceCeiling}
            oninput={(e) => (maxPrice = Number(e.currentTarget.value) >= priceCeiling ? 0 : Number(e.currentTarget.value))}
            aria-label="Maximum starting price"
          />
          <div class="row small"><span class="muted">Up to</span><span class="spacer"></span><strong class="num">{money(maxPrice || priceCeiling)}</strong></div>
        </fieldset>
      {/if}
    </aside>

    <section class="results">
      <div class="toolbar">
        <button class="btn btn-secondary btn-sm filter-btn" onclick={() => (filtersOpen = true)}>
          <Icon name="filter" size={18} /> Filter {activeFilters ? `(${activeFilters})` : ""}
        </button>
        <p class="small muted">{filtered.length} {filtered.length === 1 ? "model" : "models"}</p>
        <span class="spacer"></span>
        <label class="sort small">
          <span class="muted">Sort</span>
          <select class="select" bind:value={sort}>
            <option value="featured">Featured</option>
            <option value="price-asc">Price: low to high</option>
            <option value="price-desc">Price: high to low</option>
            <option value="power">Power</option>
            <option value="name">Name</option>
          </select>
        </label>
      </div>

      {#if loading}
        <div class="grid">
          {#each Array(6) as _}
            <div class="card"><div class="skeleton" style="aspect-ratio:16/10"></div><div class="body"><div class="skeleton" style="height:20px;width:60%"></div></div></div>
          {/each}
        </div>
      {:else if error}
        <div class="empty">
          <h3 class="h3">Couldn't load the catalog</h3>
          <p class="muted">{error}</p>
          <button class="btn btn-primary" onclick={load}>Try again</button>
        </div>
      {:else if !filtered.length}
        <div class="empty">
          <h3 class="h3">No models match these filters</h3>
          <button class="btn btn-secondary" onclick={clearFilters}>Reset filters</button>
        </div>
      {:else}
        {#each groups as group (group.key)}
          {#if group.label}
            <h2 class="h2 group-title">{group.label}</h2>
          {/if}
          <div class="grid">
            {#each group.items as v (v.slug)}
              {@const dv = defaultVariant(v)}
              <article class="card">
                <button class="card-hit" onclick={() => (drawer = v)} aria-label={`${v.name} — show variants`}>
                  <VehicleThumb vehicle={v} alt={v.name} />
                  <div class="badges">
                    <span class="tag">{classLabel(v.body_class)}</span>
                    {#if isElectric(v)}<span class="tag tag-success">Electric</span>{/if}
                    {#if v.model_file}<span class="tag tag-info">3D model</span>{/if}
                  </div>
                </button>
                <div class="body">
                  <div class="title-row">
                    <h3 class="h3">{v.name}</h3>
                    <span class="small muted">{v.variants?.length ?? 1} {v.variants?.length === 1 ? "variant" : "variants"}</span>
                  </div>
                  <p class="small muted tagline">{v.tagline}</p>
                  <p class="price">From <strong class="num">{money(v.from_price)}</strong></p>
                  {#if headlineSpecs(v.vehicle_type, dv?.specs).length}
                  <dl class="specs">
                    {#each headlineSpecs(v.vehicle_type, dv?.specs) as key}
                      <div>
                        <dt class="xsmall muted">{specLabel(key)}</dt>
                        <dd class="num">{formatSpec(key, dv.specs[key])}</dd>
                      </div>
                    {/each}
                  </dl>
                  {/if}
                  <div class="actions">
                    <a class="btn btn-primary btn-sm" href={href(`/configure/${v.slug}`, { variant: dv?.code })}>Configure</a>
                    <button class="btn btn-secondary btn-sm" onclick={() => (drawer = v)}>All variants</button>
                  </div>
                </div>
              </article>
            {/each}
          </div>
        {/each}
      {/if}
    </section>
  </div>
</div>

{#if drawer}
  <div class="scrim" onclick={() => (drawer = null)} role="presentation"></div>
  <div class="drawer" role="dialog" aria-modal="true" aria-label={`${drawer.name} variants`}>
    <div class="drawer-head">
      <div>
        <p class="eyebrow">{classLabel(drawer.body_class)}</p>
        <h2 class="h2">{drawer.name}</h2>
      </div>
      <button class="icon-btn" aria-label="Close" onclick={() => (drawer = null)}><Icon name="close" /></button>
    </div>
    <VehicleThumb vehicle={drawer} alt={drawer.name} />
    <p class="muted drawer-desc">{drawer.description}</p>
    <ul class="variants">
      {#each drawer.variants ?? [] as variant (variant.code)}
        {@const inCompare = compare.some((c) => c.key === `${drawer.slug}:${variant.code}`)}
        <li>
          <div class="v-head">
            <h3 class="h3">{variant.name}</h3>
            <span class="num">{money(variant.price)}</span>
          </div>
          <dl class="specs">
            {#each headlineSpecs(drawer.vehicle_type, variant.specs) as key}
              <div>
                <dt class="xsmall muted">{specLabel(key)}</dt>
                <dd class="num">{formatSpec(key, variant.specs[key])}</dd>
              </div>
            {/each}
          </dl>
          <div class="actions">
            <a class="btn btn-primary btn-sm" href={href(`/configure/${drawer.slug}`, { variant: variant.code })}>Configure</a>
            <button class="btn btn-ghost btn-sm" class:on={inCompare} onclick={() => toggleCompare(drawer, variant)} disabled={!inCompare && compare.length >= 4}>
              <Icon name={inCompare ? "check" : "plus"} size={16} /> Compare
            </button>
          </div>
        </li>
      {/each}
    </ul>
  </div>
{/if}

{#if compare.length}
  <div class="compare-tray" role="region" aria-label="Compare">
    <div class="container row">
      <div class="chips">
        {#each compare as c (c.key)}
          <span class="chip active">
            {c.name}
            <button aria-label={`Remove ${c.name}`} onclick={() => saveCompare(compare.filter((x) => x.key !== c.key))}><Icon name="close" size={14} /></button>
          </span>
        {/each}
      </div>
      <span class="spacer"></span>
      <button class="btn btn-ghost btn-sm" onclick={() => saveCompare([])}>Clear</button>
      <button class="btn btn-primary btn-sm" disabled={compare.length < 2} onclick={() => navigate("/compare", { v: compare.map((c) => c.key).join(",") })}>
        Compare {compare.length}
      </button>
    </div>
  </div>
{/if}

<style>
  .hero {
    padding: clamp(32px, 6vw, 72px) 0 clamp(20px, 3vw, 36px);
  }
  .hero .display {
    margin: 8px 0 12px;
  }
  .lead {
    font-size: clamp(16px, 1.4vw, 19px);
    max-width: 640px;
  }
  .count {
    font-size: 12px;
    color: var(--ink-3);
    margin-left: 4px;
    font-weight: 400;
  }
  .layout {
    display: grid;
    grid-template-columns: 260px 1fr;
    gap: clamp(24px, 3vw, 48px);
    padding: 28px 0 120px;
  }
  .filters {
    position: sticky;
    top: calc(var(--header-h) + 24px);
    align-self: start;
    display: flex;
    flex-direction: column;
    gap: 24px;
    max-height: calc(100vh - var(--header-h) - 48px);
    overflow: auto;
    padding-right: 4px;
  }
  .filters-head {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .filters-head .h3 {
    flex: 1;
  }
  .close-filters {
    display: none;
  }
  .search {
    position: relative;
    color: var(--ink-3);
  }
  .search :global(svg) {
    position: absolute;
    left: 14px;
    top: 15px;
  }
  .search .input {
    padding-left: 42px;
  }
  fieldset {
    border: 0;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  legend {
    font-weight: 600;
    font-size: 15px;
    margin-bottom: 8px;
  }
  .check {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 6px 0;
    cursor: pointer;
  }
  .check input {
    position: absolute;
    opacity: 0;
  }
  .check .box {
    width: 20px;
    height: 20px;
    border: 1.5px solid var(--line-strong);
    border-radius: 3px;
    display: grid;
    place-items: center;
    color: transparent;
    transition: all 0.15s var(--ease);
  }
  .check input:checked + .box {
    background: var(--ink);
    border-color: var(--ink);
    color: #fff;
  }
  .check input:focus-visible + .box {
    outline: 2px solid var(--focus);
    outline-offset: 2px;
  }
  .check .lbl {
    flex: 1;
  }
  .seg {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    border: 1.5px solid var(--line-strong);
    border-radius: var(--radius-sm);
    overflow: hidden;
  }
  .seg button {
    height: 40px;
    font-size: 13px;
    border-right: 1px solid var(--line);
  }
  .seg button:last-child {
    border-right: 0;
  }
  .seg button.active {
    background: var(--ink);
    color: #fff;
  }
  input[type="range"] {
    width: 100%;
    accent-color: var(--ink);
  }
  .toolbar {
    display: flex;
    align-items: center;
    gap: 16px;
    margin-bottom: 20px;
  }
  .filter-btn {
    display: none;
  }
  .sort {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .sort .select {
    height: 40px;
    width: auto;
  }
  .group-title {
    margin: 12px 0 20px;
  }
  .group-title:not(:first-of-type) {
    margin-top: 48px;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(min(100%, 340px), 1fr));
    gap: 24px 20px;
  }
  .card {
    display: flex;
    flex-direction: column;
    background: var(--bg);
    animation: fade-in 0.3s var(--ease);
  }
  .card-hit {
    position: relative;
    display: block;
    border-radius: var(--radius);
    overflow: hidden;
    text-align: left;
  }
  .card-hit :global(img) {
    transition: transform 0.6s var(--ease);
  }
  .card-hit:hover :global(img) {
    transform: scale(1.035);
  }
  .badges {
    position: absolute;
    top: 12px;
    left: 12px;
    display: flex;
    gap: 6px;
  }
  .badges .tag {
    background: rgba(255, 255, 255, 0.9);
  }
  .body {
    padding: 16px 2px 0;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .title-row {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 12px;
  }
  .tagline {
    min-height: 42px;
  }
  .price {
    font-size: 15px;
    margin-top: 2px;
  }
  .specs {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 8px;
    margin: 10px 0 4px;
    padding: 12px 0;
    border-top: 1px solid var(--line);
    border-bottom: 1px solid var(--line);
  }
  .specs div {
    display: flex;
    flex-direction: column;
  }
  .specs dt {
    order: 2;
  }
  .specs dd {
    margin: 0;
    font-weight: 600;
    font-size: 17px;
  }
  .actions {
    display: flex;
    gap: 8px;
    margin-top: 10px;
    flex-wrap: wrap;
  }
  .empty {
    padding: 80px 0;
    display: flex;
    flex-direction: column;
    gap: 12px;
    align-items: flex-start;
  }

  .scrim {
    position: fixed;
    inset: 0;
    background: rgba(14, 14, 18, 0.4);
    z-index: 80;
    animation: fade-in 0.2s var(--ease);
  }
  .drawer {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: min(560px, 100vw);
    background: var(--bg);
    z-index: 90;
    overflow-y: auto;
    padding: 28px;
    display: flex;
    flex-direction: column;
    gap: 18px;
    box-shadow: var(--shadow-2);
    animation: slide-in 0.28s var(--ease);
  }
  @keyframes slide-in {
    from {
      transform: translateX(40px);
      opacity: 0;
    }
  }
  .drawer-head {
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
  }
  .drawer :global(.thumb) {
    border-radius: var(--radius);
  }
  .drawer-desc {
    font-size: 15px;
  }
  .variants {
    list-style: none;
    padding: 0;
    margin: 0;
    display: flex;
    flex-direction: column;
  }
  .variants li {
    padding: 20px 0;
    border-top: 1px solid var(--line);
  }
  .v-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
  }
  .btn.on {
    background: var(--surface);
  }

  .compare-tray {
    position: fixed;
    left: 0;
    right: 0;
    bottom: 0;
    z-index: 60;
    background: var(--bg);
    border-top: 1px solid var(--line);
    box-shadow: 0 -8px 24px rgba(0, 0, 0, 0.06);
    padding: 12px 0;
  }
  .compare-tray .row {
    gap: 12px;
  }
  .chips {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .chips button {
    display: inline-grid;
    color: inherit;
  }

  @media (max-width: 960px) {
    .layout {
      grid-template-columns: 1fr;
    }
    .filters {
      display: none;
    }
    .filters.open {
      display: flex;
      position: fixed;
      inset: 0;
      z-index: 95;
      background: var(--bg);
      padding: 24px;
      max-height: none;
    }
    .close-filters {
      display: inline-grid;
    }
    .filter-btn {
      display: inline-flex;
    }
  }
</style>
