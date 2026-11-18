<script>
  import { untrack } from "svelte";
  import { api, getSessionKey } from "../api.js";
  import { Configurator } from "../configurator.svelte.js";
  import { classLabel, formatSpec, money, SECTION_ORDER, sectionLabel, specDelta, specLabel, SPECS } from "../format.js";
  import { href, route } from "../router.svelte.js";
  import Icon from "../components/Icon.svelte";
  import StageView from "../components/StageView.svelte";

  let { slug } = $props();

  const cfg = new Configurator();
  untrack(() => cfg.load(slug, { variant: route.query.variant ?? "", selections: (route.query.o ?? "").split(".").filter(Boolean) }));

  let code = $state(route.query.code ?? null);
  let saving = $state(false);
  let copied = $state(false);
  let view = $state("front34");

  const v = $derived(cfg.vehicle);
  const query = $derived({ variant: cfg.result?.variant, o: cfg.result?.selections.join(".") || undefined });
  const bySection = $derived.by(() => {
    const groups = new Map();
    for (const r of cfg.resolvedList) {
      if (!groups.has(r.section)) groups.set(r.section, []);
      groups.get(r.section).push(r);
    }
    const order = [...SECTION_ORDER, ...[...groups.keys()].filter((s) => !SECTION_ORDER.includes(s))];
    return order.filter((s) => groups.has(s)).map((s) => ({ section: s, items: groups.get(s) }));
  });
  const baseSpecs = $derived(cfg.variantInfo?.specs ?? {});
  const specKeys = $derived(Object.keys(SPECS).filter((k) => cfg.specs[k] !== undefined && cfg.specs[k] !== 0));

  async function save() {
    saving = true;
    try {
      const res = await api.saveConfiguration({
        vehicle: v.slug,
        variant: cfg.result.variant,
        selections: cfg.result.selections,
        name: cfg.variantInfo?.name ?? v.name,
        session_key: getSessionKey(),
      });
      code = res.code;
    } catch (e) {
      alert(`Couldn't save: ${e.message}`);
    }
    saving = false;
  }

  async function copy(text) {
    try {
      await navigator.clipboard.writeText(text);
      copied = true;
      setTimeout(() => (copied = false), 1800);
    } catch {
      prompt("Copy", text);
    }
  }

  const shareUrl = $derived(code ? `${location.origin}${location.pathname}#/code/${code}` : location.href);
  const today = new Date().toLocaleDateString("en-US", { year: "numeric", month: "long", day: "numeric" });
</script>

{#if cfg.error && !v}
  <div class="container empty">
    <h2 class="h2">Summary unavailable</h2>
    <p class="muted">{cfg.error}</p>
    <a class="btn btn-primary" href="#/">All models</a>
  </div>
{:else}
  <div class="summary">
    <section class="hero">
      <div class="stage-wrap">
        <StageView vehicle={v} visual={cfg.visual} {view} autoRotate={view === "front34"} />
        <div class="views no-print">
          {#each [["front34", "Exterior"], ["rear34", "Rear"], ["side", "Side"], ["interior", "Interior"]] as [key, label]}
            <button class:active={view === key} onclick={() => (view = key)}>{label}</button>
          {/each}
        </div>
      </div>

      <div class="intro container">
        <div>
          <p class="eyebrow">Your {classLabel(v?.body_class)} · {today}</p>
          <h1 class="display">{cfg.variantInfo?.name ?? v?.name ?? ""}</h1>
          <p class="muted lead">{v?.tagline}</p>
        </div>
        <div class="price-card">
          <p class="small muted">Total price</p>
          <p class="total num">{money(cfg.price.total)}</p>
          {#if code}
            <div class="code-row">
              <span class="small muted">Kraft code</span>
              <strong class="code num">{code}</strong>
            </div>
          {/if}
          <div class="actions no-print">
            {#if !code}
              <button class="btn btn-primary" onclick={save} disabled={saving || !cfg.result}><Icon name="save" size={18} /> {saving ? "Saving…" : "Save & get code"}</button>
            {/if}
            <button class="btn btn-secondary" onclick={() => copy(shareUrl)}><Icon name="share" size={18} /> {copied ? "Copied" : "Share link"}</button>
            <button class="btn btn-secondary" onclick={() => window.print()}><Icon name="print" size={18} /> Print / PDF</button>
            <a class="btn btn-ghost" href={href(`/configure/${slug}`, query)}><Icon name="edit" size={18} /> Edit configuration</a>
          </div>
        </div>
      </div>
    </section>

    <div class="container grid">
      <section>
        <h2 class="h2">Your configuration</h2>
        {#each bySection as group (group.section)}
          <div class="block">
            <h3 class="h3">{sectionLabel(group.section, v?.vehicle_type)}</h3>
            <table class="lines">
              <tbody>
                {#each group.items as r (r.code)}
                  <tr>
                    <td>
                      <span class="opt">{r.name}</span>
                      <span class="xsmall subtle">{r.category} · {r.code}{r.by_name ? ` · ${r.origin === "included" ? "included with" : "required by"} ${r.by_name}` : ""}</span>
                    </td>
                    <td class="num right">
                      {#if r.origin === "included"}<span class="tag tag-success">Included</span>
                      {:else if !r.price}<span class="muted small">Standard</span>
                      {:else}{money(r.price)}{/if}
                    </td>
                  </tr>
                {/each}
              </tbody>
            </table>
          </div>
        {/each}
      </section>

      <aside>
        <div class="block">
          <h2 class="h3">Price breakdown</h2>
          <table class="lines">
            <tbody>
              <tr><td>{cfg.variantInfo?.name} base price</td><td class="num right">{money(cfg.price.base)}</td></tr>
              {#each SECTION_ORDER.filter((s) => cfg.price.sections?.[s]) as s}
                <tr><td>{sectionLabel(s, v?.vehicle_type)} options</td><td class="num right">{money(cfg.price.sections[s])}</td></tr>
              {/each}
              <tr class="total-row"><td>Total</td><td class="num right">{money(cfg.price.total)}</td></tr>
            </tbody>
          </table>
          <p class="xsmall subtle">Indicative prices in USD, excluding taxes, delivery and registration.</p>
        </div>

        <div class="block">
          <h2 class="h3">Technical data</h2>
          <table class="lines">
            <tbody>
              {#each specKeys as key}
                {@const d = specDelta(key, cfg.specs[key], baseSpecs[key])}
                <tr>
                  <td>{specLabel(key)}</td>
                  <td class="num right">
                    {formatSpec(key, cfg.specs[key])}
                    {#if d}<span class="delta" class:good={d.good === true} class:bad={d.good === false}>{d.text}</span>{/if}
                  </td>
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
      </aside>
    </div>
  </div>
{/if}

<style>
  .summary {
    padding-bottom: 96px;
  }
  .stage-wrap {
    position: relative;
    height: min(62vh, 620px);
  }
  .views {
    position: absolute;
    left: 50%;
    bottom: 20px;
    transform: translateX(-50%);
    display: flex;
    background: rgba(255, 255, 255, 0.85);
    backdrop-filter: blur(10px);
    border-radius: 22px;
    padding: 4px;
    box-shadow: var(--shadow-1);
  }
  .views button {
    height: 32px;
    padding: 0 14px;
    border-radius: 16px;
    font-size: 13px;
    color: var(--ink-2);
  }
  .views button.active {
    background: var(--ink);
    color: #fff;
  }
  .intro {
    display: grid;
    grid-template-columns: 1fr minmax(320px, 420px);
    gap: 40px;
    align-items: start;
    padding-top: 40px;
    padding-bottom: 24px;
  }
  .display {
    margin: 8px 0 12px;
  }
  .lead {
    font-size: 18px;
  }
  .price-card {
    background: var(--surface);
    border-radius: var(--radius-lg);
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .total {
    font-size: 36px;
    font-weight: 600;
    letter-spacing: -0.02em;
  }
  .code-row {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    padding: 10px 0;
    border-top: 1px solid var(--line);
  }
  .code {
    font-size: 20px;
    letter-spacing: 0.08em;
  }
  .actions {
    display: flex;
    flex-direction: column;
    gap: 8px;
    margin-top: 10px;
  }
  .grid {
    display: grid;
    grid-template-columns: 1fr minmax(320px, 420px);
    gap: 40px;
    padding-top: 24px;
  }
  .block {
    margin-top: 28px;
  }
  .block .h3 {
    margin-bottom: 8px;
  }
  .lines {
    width: 100%;
    border-collapse: collapse;
  }
  .lines td {
    padding: 12px 0;
    border-bottom: 1px solid var(--line);
    vertical-align: top;
  }
  .lines td:first-child {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }
  .right {
    text-align: right;
    white-space: nowrap;
  }
  .opt {
    font-weight: 500;
  }
  .total-row td {
    font-weight: 600;
    font-size: 18px;
    border-bottom: 0;
  }
  .delta {
    font-size: 12px;
    margin-left: 6px;
    color: var(--ink-3);
  }
  .delta.good {
    color: var(--success);
  }
  .delta.bad {
    color: var(--warning);
  }
  .empty {
    padding: 80px 0;
    display: flex;
    flex-direction: column;
    gap: 12px;
    align-items: flex-start;
  }

  @media (max-width: 900px) {
    .intro,
    .grid {
      grid-template-columns: 1fr;
    }
    .stage-wrap {
      height: 44vh;
    }
  }
  @media print {
    .stage-wrap {
      height: 360px;
    }
    .price-card {
      background: none;
      border: 1px solid #ccc;
    }
  }
</style>
