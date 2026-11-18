<script>
  import { untrack } from "svelte";
  import { api, getSessionKey } from "../api.js";
  import { Configurator } from "../configurator.svelte.js";
  import { formatSpec, headlineSpecs, money, SECTION_ORDER, sectionLabel, specDelta, specLabel, SPECS } from "../format.js";
  import { href, navigate, replaceQuery, route } from "../router.svelte.js";
  import { ENVIRONMENTS } from "../three/Stage.js";
  import Icon from "../components/Icon.svelte";
  import OptionCategory from "../components/OptionCategory.svelte";
  import StageView from "../components/StageView.svelte";
  import { needsAnalysis, storeAnalysis } from "../modelAnalysis.js";

  let { slug } = $props();

  const cfg = new Configurator();
  // The page is keyed on slug by the router, so loading once is enough.
  untrack(() =>
    cfg.load(slug, {
      variant: route.query.variant ?? "",
      selections: (route.query.o ?? "").split(".").filter(Boolean),
    }),
  );

  let section = $state("exterior");
  let view = $state("front34");
  let environment = $state("studio");
  let autoRotate = $state(false);
  let autoCamera = $state(true);
  let search = $state("");
  let specsOpen = $state(false);
  let saveState = $state(null); // null | "saving" | { code } | { error }
  let stageEl = $state(null);
  let stage = null;

  // ---- Model analysis: highlights, hero angle, preview coverage ----------------
  let analysis = $state(null);
  let previewRoles = $state(null); // roles an uploaded model can show (null = procedural, all)
  let touring = $state(false);
  let cameraTouched = false;
  let backfilled = false;
  let tourTimer = null;
  const highlights = $derived(analysis?.highlights ?? []);
  const activeHighlight = $derived(view.startsWith("hl:") ? highlights.find((h) => `hl:${h.id}` === view) : null);

  function onBuilt(built) {
    analysis = built.analysis ?? null;
    previewRoles = built.glb ? new Set(built.boundRoles ?? []) : null;
    // Open on the model's best side unless the user already moved the camera.
    if (analysis?.hero && !cameraTouched && view === "front34") view = analysis.hero;
    // Older uploads (or a replaced model) get analysed once and stored, so the
    // engine can offer the model's original finish.
    if (analysis && v && needsAnalysis(v) && !backfilled) {
      backfilled = true;
      storeAnalysis(v, analysis)
        .then(() => cfg.load(v.slug, { variant: cfg.result?.variant ?? cfg.variant, selections: $state.snapshot(cfg.selections) }))
        .catch((e) => console.warn("Could not store model analysis", e));
    }
  }

  function showHighlight(h) {
    stopTour();
    cameraTouched = true;
    view = `hl:${h.id}`;
  }

  function startTour() {
    if (!highlights.length) return;
    touring = true;
    autoRotate = false;
    let i = Math.max(0, highlights.findIndex((h) => `hl:${h.id}` === view) + 1) % highlights.length;
    const step = () => {
      view = `hl:${highlights[i].id}`;
      i = (i + 1) % highlights.length;
    };
    step();
    tourTimer = setInterval(step, 3800);
  }

  function stopTour() {
    touring = false;
    clearInterval(tourTimer);
    tourTimer = null;
  }

  $effect(() => () => clearInterval(tourTimer));

  const SECTION_VIEWS = { exterior: "front34", wheels: "wheel", interior: "interior", performance: "rear34", technology: "front34", comfort: "interior", packages: "side" };
  const VIEW_BUTTONS = [
    ["front34", "Front"],
    ["side", "Side"],
    ["rear34", "Rear"],
    ["top", "Top"],
    ["interior", "Interior"],
    ["wheel", "Detail"],
  ];

  const v = $derived(cfg.vehicle);
  const sections = $derived(SECTION_ORDER.filter((s) => v?.option_categories?.some((c) => c.section === s)).concat(
    [...new Set((v?.option_categories ?? []).map((c) => c.section))].filter((s) => !SECTION_ORDER.includes(s)),
  ));
  const filter = $derived(search.trim().toLowerCase());
  const visibleCategories = $derived(
    (v?.option_categories ?? []).filter((c) => (filter ? true : c.section === section)),
  );
  const optionsCount = $derived(cfg.resolvedList.filter((r) => r.origin === "user" || r.origin === "required").length);
  const baseSpecs = $derived(cfg.variantInfo?.specs ?? {});
  const liveSpecs = $derived(headlineSpecs(v?.vehicle_type, cfg.specs).concat(cfg.specs.weight_kg ? ["weight_kg"] : []).slice(0, 4));

  // Keep the URL shareable: every change rewrites ?variant=&o=.
  $effect(() => {
    if (!cfg.result || !v) return;
    replaceQuery(`/configure/${v.slug}`, { variant: cfg.result.variant, o: cfg.result.selections.join(".") || undefined });
  });

  $effect(() => {
    if (sections.length && !sections.includes(section)) section = sections[0];
  });

  function pickSection(s) {
    stopTour();
    section = s;
    search = "";
    if (autoCamera) view = SECTION_VIEWS[s] ?? "front34";
    stageEl?.closest(".cfg")?.querySelector(".panel-scroll")?.scrollTo({ top: 0, behavior: "smooth" });
  }

  function onKey(e) {
    const mod = e.ctrlKey || e.metaKey;
    if (mod && e.key.toLowerCase() === "z" && !e.shiftKey) {
      e.preventDefault();
      cfg.undo();
    } else if (mod && (e.key.toLowerCase() === "y" || (e.key.toLowerCase() === "z" && e.shiftKey))) {
      e.preventDefault();
      cfg.redo();
    } else if (e.key === "Escape") {
      specsOpen = false;
      saveState = null;
    }
  }

  async function save() {
    saveState = "saving";
    try {
      const res = await api.saveConfiguration({
        vehicle: v.slug,
        variant: cfg.result.variant,
        selections: cfg.result.selections,
        name: cfg.variantInfo?.name ?? v.name,
        session_key: getSessionKey(),
      });
      saveState = { code: res.code };
    } catch (e) {
      saveState = { error: e.message };
    }
  }

  let copied = $state(false);
  async function copyLink(text = location.href) {
    try {
      await navigator.clipboard.writeText(text);
      copied = true;
      setTimeout(() => (copied = false), 1800);
    } catch {
      prompt("Copy this link", text);
    }
  }

  function toggleFullscreen() {
    const el = stageEl?.parentElement;
    if (!el) return;
    if (document.fullscreenElement) document.exitFullscreen();
    else el.requestFullscreen?.();
  }

  function goSummary() {
    navigate(`/summary/${v.slug}`, { variant: cfg.result?.variant, o: cfg.result?.selections.join(".") || undefined });
  }

  const paintCategory = $derived(v?.option_categories?.find((c) => c.display === "swatch"));
  const paintChoice = $derived(paintCategory ? cfg.choiceIn(paintCategory) : null);
</script>

<svelte:window onkeydown={onKey} />

<div class="cfg">
  {#if cfg.error && !v}
    <div class="center">
      <h2 class="h2">This vehicle couldn't be loaded</h2>
      <p class="muted">{cfg.error}</p>
      <a class="btn btn-primary" href="#/">Back to all models</a>
    </div>
  {:else}
    <div class="topbar">
      <a class="icon-btn" href="#/" aria-label="All models"><Icon name="arrow-left" /></a>
      <div class="identity">
        <p class="xsmall muted">{[v?.family || v?.name || "Kraft", v?.year].filter(Boolean).join(" · ")}</p>
        {#if v && v.variants?.length > 1}
          <label class="variant">
            <span class="sr-only">Variant</span>
            <select value={cfg.result?.variant ?? cfg.variant} onchange={(e) => cfg.setVariant(e.currentTarget.value)}>
              {#each v.variants as variant (variant.code)}
                <option value={variant.code}>{variant.name} — {money(variant.price)}</option>
              {/each}
            </select>
            <Icon name="chevron-down" size={18} />
          </label>
        {:else}
          <h1 class="name">{cfg.variantInfo?.name ?? v?.name ?? "Loading…"}</h1>
        {/if}
      </div>
      <span class="spacer"></span>
      <div class="tools">
        <button class="icon-btn" title="Undo (Ctrl+Z)" aria-label="Undo" disabled={!cfg.canUndo} onclick={() => cfg.undo()}><Icon name="undo" /></button>
        <button class="icon-btn" title="Redo (Ctrl+Y)" aria-label="Redo" disabled={!cfg.canRedo} onclick={() => cfg.redo()}><Icon name="redo" /></button>
        <button class="icon-btn" title="Reset to standard" aria-label="Reset" onclick={() => cfg.reset()}><Icon name="reset" /></button>
        <span class="sep"></span>
        <button class="btn btn-ghost btn-sm" onclick={() => copyLink()}><Icon name="share" size={18} /> <span class="lbl">{copied ? "Link copied" : "Share"}</span></button>
        <button class="btn btn-secondary btn-sm" onclick={save} disabled={!cfg.result || saveState === "saving"}><Icon name="save" size={18} /> <span class="lbl">Save</span></button>
      </div>
    </div>

    <div class="main">
      <div class="stage-col">
        <div class="stage-host" bind:this={stageEl}>
          <StageView
            vehicle={v}
            visual={cfg.visual}
            {view}
            {environment}
            {autoRotate}
            onstage={(s) => (stage = s)}
            onbuilt={onBuilt}
            oninteract={() => {
              autoRotate = false;
              cameraTouched = true;
              stopTour();
            }}
          />
        </div>

        <div class="toasts" aria-live="polite">
          {#each cfg.toasts as t (t.id)}
            <div class="toast" class:removed={t.type === "removed"}>
              <span class="badge">{t.type === "removed" ? "Removed" : "Added"}</span>
              <div>
                <p class="small"><strong>{t.title}</strong></p>
                <p class="xsmall muted">{t.text}</p>
              </div>
              <button class="icon-btn" aria-label="Dismiss" onclick={() => cfg.dismiss(t.id)}><Icon name="close" size={16} /></button>
            </div>
          {/each}
        </div>

        <div class="stage-tools">
          <div class="env" role="group" aria-label="Environment">
            {#each Object.entries(ENVIRONMENTS) as [key, e]}
              <button class:active={environment === key} onclick={() => (environment = key)}>{e.label}</button>
            {/each}
          </div>
          <button class="icon-btn glass" title="Auto-rotate" aria-pressed={autoRotate} onclick={() => (autoRotate = !autoRotate)}>
            <Icon name={autoRotate ? "pause" : "rotate"} />
          </button>
          <button class="icon-btn glass" title="Fullscreen" aria-label="Fullscreen" onclick={toggleFullscreen}><Icon name="fullscreen" /></button>
        </div>

        {#if highlights.length > 1}
          <div class="highlights" aria-label="Model highlights">
            <div class="hl-head">
              <p class="xsmall muted">Highlights</p>
              <button class="hl-tour" onclick={() => (touring ? stopTour() : startTour())} aria-pressed={touring}>
                <Icon name={touring ? "pause" : "rotate"} size={14} />
                {touring ? "Stop" : "Tour"}
              </button>
            </div>
            <ul>
              {#each highlights as h (h.id)}
                <li>
                  <button class:active={activeHighlight?.id === h.id} onclick={() => showHighlight(h)}>
                    <span class="hl-label">{h.label}</span>
                    <span class="hl-detail xsmall">{h.detail}</span>
                  </button>
                </li>
              {/each}
            </ul>
          </div>
        {/if}

        {#if paintChoice}
          <div class="paint-label">
            <p class="xsmall muted">{paintCategory.name}</p>
            <p class="paint-name">{paintChoice.name}</p>
          </div>
        {/if}

        <div class="views" role="group" aria-label="Camera views">
          {#each VIEW_BUTTONS as [key, label]}
            <button
              class:active={view === key}
              onclick={() => {
                stopTour();
                cameraTouched = true;
                view = key;
              }}>{label}</button
            >
          {/each}
          <label class="auto" title="Move the camera when switching sections">
            <input type="checkbox" bind:checked={autoCamera} /> Auto
          </label>
        </div>
      </div>

      <aside class="panel">
        <nav class="sections tabs" aria-label="Configuration sections">
          {#each sections as s}
            <button class="tab" class:active={!filter && section === s} onclick={() => pickSection(s)}>{sectionLabel(s, v?.vehicle_type)}</button>
          {/each}
        </nav>
        <div class="panel-search">
          <Icon name="search" size={18} />
          <input class="input" placeholder="Search all options" bind:value={search} aria-label="Search options" />
          {#if search}<button class="icon-btn" aria-label="Clear search" onclick={() => (search = "")}><Icon name="close" size={16} /></button>{/if}
        </div>
        <div class="panel-scroll">
          {#if cfg.loading && !v}
            {#each Array(4) as _}<div class="skeleton" style="height:120px;margin:20px 0;border-radius:8px"></div>{/each}
          {:else}
            {#each visibleCategories as category (category.id)}
              <OptionCategory {category} {cfg} {filter} {previewRoles} />
            {/each}
            {#if filter && !visibleCategories.some((c) => c.choices.some((x) => `${x.name} ${x.code} ${x.description}`.toLowerCase().includes(filter)))}
              <p class="muted small empty">No options match “{search}”.</p>
            {/if}
          {/if}
        </div>
      </aside>
    </div>

    <footer class="bar">
      <div class="price">
        <p class="xsmall muted">Total price</p>
        <p class="total num">{money(cfg.price.total)}</p>
        <p class="xsmall muted num">Base {money(cfg.price.base)} · Options {money(cfg.price.options)} · {optionsCount} selected</p>
      </div>
      <dl class="live">
        {#each liveSpecs as key}
          {@const d = specDelta(key, cfg.specs[key], baseSpecs[key])}
          <div>
            <dt class="xsmall muted">{specLabel(key)}</dt>
            <dd class="num">
              {formatSpec(key, cfg.specs[key])}
              {#if d}<span class="delta" class:good={d.good === true} class:bad={d.good === false}>{d.text}</span>{/if}
            </dd>
          </div>
        {/each}
      </dl>
      <div class="cta">
        <button class="btn btn-ghost" onclick={() => (specsOpen = true)}><Icon name="gauge" size={18} /> Technical data</button>
        <button class="btn btn-primary" onclick={goSummary} disabled={!cfg.result}>Summary <Icon name="arrow-right" size={18} /></button>
      </div>
    </footer>
  {/if}
</div>

{#if specsOpen}
  <div class="scrim" role="presentation" onclick={() => (specsOpen = false)}></div>
  <div class="sheet" role="dialog" aria-modal="true" aria-label="Technical data">
    <div class="sheet-head">
      <h2 class="h2">Technical data</h2>
      <button class="icon-btn" aria-label="Close" onclick={() => (specsOpen = false)}><Icon name="close" /></button>
    </div>
    <p class="small muted">{cfg.variantInfo?.name} with your selected options. Figures update live from the configuration engine.</p>
    <table class="spec-table">
      <thead><tr><th>Specification</th><th>Standard</th><th>Your build</th></tr></thead>
      <tbody>
        {#each Object.keys(SPECS).filter((k) => cfg.specs[k] !== undefined && cfg.specs[k] !== 0) as key}
          {@const d = specDelta(key, cfg.specs[key], baseSpecs[key])}
          <tr>
            <td>{specLabel(key)}</td>
            <td class="num muted">{formatSpec(key, baseSpecs[key])}</td>
            <td class="num"><strong>{formatSpec(key, cfg.specs[key])}</strong>{#if d}<span class="delta" class:good={d.good === true} class:bad={d.good === false}>{d.text}</span>{/if}</td>
          </tr>
        {/each}
      </tbody>
    </table>
    <p class="xsmall subtle">Rules engine evaluated this build in {cfg.lastEngineUs} µs.</p>
  </div>
{/if}

{#if saveState && saveState !== "saving"}
  <div class="scrim" role="presentation" onclick={() => (saveState = null)}></div>
  <div class="modal" role="dialog" aria-modal="true" aria-label="Configuration saved">
    {#if saveState.code}
      <p class="eyebrow">Saved to My Garage</p>
      <h2 class="h2">Your Kraft code</h2>
      <p class="code num">{saveState.code}</p>
      <p class="small muted">Use this code to reopen, share or hand over this exact configuration.</p>
      <div class="modal-actions">
        <button class="btn btn-secondary" onclick={() => copyLink(`${location.origin}${location.pathname}#/code/${saveState.code}`)}>
          <Icon name="copy" size={18} /> {copied ? "Copied" : "Copy link"}
        </button>
        <a class="btn btn-primary" href={href(`/summary/${v.slug}`, { variant: cfg.result?.variant, o: cfg.result?.selections.join(".") || undefined, code: saveState.code })}>View summary</a>
      </div>
    {:else}
      <h2 class="h2">Couldn't save</h2>
      <p class="muted">{saveState.error}</p>
      <div class="modal-actions"><button class="btn btn-primary" onclick={() => (saveState = null)}>Close</button></div>
    {/if}
  </div>
{/if}

<style>
  .cfg {
    flex: 1;
    min-height: 0;
    min-width: 0;
    width: 100%;
    display: grid;
    grid-template-rows: auto 1fr auto;
    grid-template-columns: minmax(0, 1fr);
    height: calc(100vh - var(--header-h));
  }
  .center {
    grid-row: 1 / -1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 12px;
  }
  .topbar {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px clamp(12px, 2vw, 28px);
    border-bottom: 1px solid var(--line);
    min-height: 64px;
  }
  .identity {
    display: flex;
    flex-direction: column;
    min-width: 0;
    flex: 1;
  }
  .name {
    font-size: 20px;
  }
  .variant {
    position: relative;
    display: inline-flex;
    align-items: center;
  }
  .variant select {
    appearance: none;
    border: 0;
    background: transparent;
    font-size: 20px;
    font-weight: 600;
    padding-right: 26px;
    cursor: pointer;
    max-width: 52vw;
    text-overflow: ellipsis;
  }
  .variant :global(svg) {
    position: absolute;
    right: 0;
    pointer-events: none;
  }
  .tools {
    display: flex;
    align-items: center;
    gap: 4px;
  }
  .sep {
    width: 1px;
    height: 24px;
    background: var(--line);
    margin: 0 8px;
  }
  .main {
    display: grid;
    grid-template-columns: minmax(0, 1fr) var(--panel-w);
    min-height: 0;
    min-width: 0;
  }
  .stage-col {
    position: relative;
    min-height: 0;
    overflow: hidden;
  }
  .stage-host {
    position: absolute;
    inset: 0;
  }
  .stage-tools {
    position: absolute;
    top: 16px;
    right: 16px;
    display: flex;
    gap: 8px;
    align-items: center;
  }
  .env,
  .views {
    display: flex;
    background: rgba(255, 255, 255, 0.82);
    backdrop-filter: blur(10px);
    border-radius: 22px;
    padding: 4px;
    box-shadow: var(--shadow-1);
  }
  .env button,
  .views button {
    height: 32px;
    padding: 0 14px;
    border-radius: 16px;
    font-size: 13px;
    color: var(--ink-2);
    transition: all 0.15s var(--ease);
  }
  .env button:hover,
  .views button:hover {
    color: var(--ink);
  }
  .env button.active,
  .views button.active {
    background: var(--ink);
    color: #fff;
  }
  .glass {
    background: rgba(255, 255, 255, 0.82);
    backdrop-filter: blur(10px);
    border-radius: 50%;
    box-shadow: var(--shadow-1);
  }
  .views {
    position: absolute;
    left: 50%;
    bottom: 20px;
    transform: translateX(-50%);
    align-items: center;
  }
  .auto {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    color: var(--ink-2);
    padding: 0 10px 0 8px;
    border-left: 1px solid var(--line);
    margin-left: 4px;
    cursor: pointer;
  }
  .auto input {
    accent-color: var(--ink);
  }
  .highlights {
    position: absolute;
    left: clamp(16px, 2vw, 32px);
    top: 50%;
    transform: translateY(-50%);
    width: 210px;
    max-height: calc(100% - 200px);
    overflow-y: auto;
    background: rgba(255, 255, 255, 0.82);
    backdrop-filter: blur(10px);
    border-radius: var(--radius);
    box-shadow: var(--shadow-1);
    padding: 10px 6px 6px;
    z-index: 2;
  }
  .hl-head {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 8px 6px;
  }
  .hl-tour {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    font-size: 12px;
    color: var(--ink-2);
    padding: 2px 8px;
    border-radius: 12px;
  }
  .hl-tour:hover,
  .hl-tour[aria-pressed="true"] {
    background: var(--ink);
    color: #fff;
  }
  .highlights ul {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .highlights li button {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    width: 100%;
    text-align: left;
    padding: 7px 8px;
    border-radius: 6px;
    transition: background 0.15s var(--ease);
  }
  .highlights li button:hover {
    background: rgba(14, 14, 18, 0.06);
  }
  .highlights li button.active {
    background: var(--ink);
    color: #fff;
  }
  .highlights li button.active .hl-detail {
    color: rgba(255, 255, 255, 0.75);
  }
  .hl-label {
    font-size: 14px;
    font-weight: 600;
  }
  .hl-detail {
    color: var(--ink-3);
  }
  .paint-label {
    position: absolute;
    left: clamp(16px, 2vw, 32px);
    bottom: 76px;
    pointer-events: none;
  }
  .paint-name {
    font-size: clamp(18px, 1.8vw, 26px);
    font-weight: 600;
    letter-spacing: -0.01em;
  }
  .toasts {
    position: absolute;
    left: 16px;
    top: 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    max-width: min(420px, calc(100% - 32px));
    z-index: 5;
  }
  .toast {
    display: grid;
    grid-template-columns: auto 1fr auto;
    gap: 12px;
    align-items: start;
    background: var(--bg);
    border-radius: var(--radius);
    box-shadow: var(--shadow-2);
    padding: 12px 8px 12px 14px;
    animation: fade-in 0.25s var(--ease);
    border-left: 3px solid var(--success);
  }
  .toast.removed {
    border-left-color: var(--warning);
  }
  .toast .badge {
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--success);
    padding-top: 2px;
  }
  .toast.removed .badge {
    color: var(--warning);
  }
  .toast .icon-btn {
    width: 28px;
    height: 28px;
  }
  .panel {
    display: flex;
    flex-direction: column;
    min-height: 0;
    border-left: 1px solid var(--line);
    background: var(--bg);
  }
  .sections {
    padding: 0 24px;
    gap: 22px;
    flex-shrink: 0;
  }
  .sections .tab {
    font-size: 14px;
  }
  .panel-search {
    position: relative;
    padding: 16px 24px 4px;
    color: var(--ink-3);
    flex-shrink: 0;
  }
  .panel-search > :global(svg) {
    position: absolute;
    left: 38px;
    top: 31px;
  }
  .panel-search .input {
    padding-left: 40px;
    height: 44px;
  }
  .panel-search .icon-btn {
    position: absolute;
    right: 28px;
    top: 18px;
  }
  .panel-scroll {
    flex: 1;
    overflow-y: auto;
    padding: 0 24px 40px;
    overscroll-behavior: contain;
  }
  .empty {
    padding: 24px 0;
  }
  .bar {
    display: grid;
    grid-template-columns: auto 1fr auto;
    align-items: center;
    gap: clamp(16px, 3vw, 48px);
    padding: 12px clamp(16px, 2vw, 28px);
    border-top: 1px solid var(--line);
    background: var(--bg);
    min-height: var(--bar-h);
  }
  .price {
    min-width: 0;
  }
  .total {
    font-size: 24px;
    font-weight: 600;
    letter-spacing: -0.01em;
    line-height: 1.2;
  }
  .live {
    display: flex;
    gap: clamp(16px, 3vw, 40px);
    margin: 0;
    justify-content: center;
    flex-wrap: wrap;
  }
  .live div {
    display: flex;
    flex-direction: column;
  }
  .live dd {
    margin: 0;
    font-weight: 600;
    font-size: 16px;
    display: flex;
    align-items: baseline;
    gap: 6px;
  }
  .delta {
    font-size: 12px;
    font-weight: 500;
    color: var(--ink-3);
    margin-left: 6px;
  }
  .delta.good {
    color: var(--success);
  }
  .delta.bad {
    color: var(--warning);
  }
  .cta {
    display: flex;
    gap: 8px;
  }

  .scrim {
    position: fixed;
    inset: 0;
    background: rgba(14, 14, 18, 0.4);
    z-index: 80;
    animation: fade-in 0.2s var(--ease);
  }
  .sheet {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: min(560px, 100vw);
    background: var(--bg);
    z-index: 90;
    padding: 28px;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 16px;
    box-shadow: var(--shadow-2);
  }
  .sheet-head {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .spec-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 15px;
  }
  .spec-table th {
    text-align: left;
    font-size: 12px;
    color: var(--ink-3);
    font-weight: 500;
    padding: 8px 0;
    border-bottom: 1px solid var(--line);
  }
  .spec-table td {
    padding: 12px 0;
    border-bottom: 1px solid var(--line);
  }
  .modal {
    position: fixed;
    left: 50%;
    top: 50%;
    transform: translate(-50%, -50%);
    width: min(480px, calc(100vw - 32px));
    background: var(--bg);
    border-radius: var(--radius-lg);
    padding: 32px;
    z-index: 90;
    display: flex;
    flex-direction: column;
    gap: 10px;
    box-shadow: var(--shadow-2);
    animation: fade-in 0.2s var(--ease);
  }
  .code {
    font-size: 40px;
    font-weight: 600;
    letter-spacing: 0.08em;
    margin: 4px 0;
  }
  .modal-actions {
    display: flex;
    gap: 8px;
    margin-top: 12px;
    flex-wrap: wrap;
  }

  @media (max-width: 1100px) {
    .main {
      grid-template-columns: 1fr 380px;
    }
    .live {
      display: none;
    }
    .bar {
      grid-template-columns: 1fr auto;
    }
  }
  @media (max-width: 820px) {
    .cfg {
      height: auto;
      min-height: calc(100vh - var(--header-h));
    }
    .main {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: 52vh auto;
    }
    .topbar {
      gap: 4px;
      padding: 8px 8px 8px 4px;
    }
    .topbar > .spacer,
    .tools .btn-ghost,
    .tools [aria-label="Redo"] {
      display: none;
    }
    .variant select {
      font-size: 16px;
      max-width: 100%;
    }
    .sections {
      padding: 0 16px;
    }
    .panel-search {
      padding: 12px 16px 4px;
    }
    .panel-search > :global(svg) {
      left: 30px;
      top: 27px;
    }
    .panel-scroll {
      padding: 0 16px 24px;
    }
    .panel {
      border-left: 0;
      border-top: 1px solid var(--line);
    }
    .panel-scroll {
      overflow: visible;
    }
    .tools .btn .lbl,
    .sep {
      display: none;
    }
    .env,
    .highlights {
      display: none;
    }
    .views {
      bottom: 12px;
      max-width: calc(100% - 24px);
      overflow-x: auto;
    }
    .auto {
      display: none;
    }
    .paint-label {
      bottom: 64px;
    }
    .bar {
      position: sticky;
      bottom: 0;
    }
    .cta .btn-ghost {
      display: none;
    }
  }
</style>
