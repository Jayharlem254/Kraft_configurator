<script>
  import { money } from "../format.js";
  import Icon from "./Icon.svelte";
  import WheelGlyph from "./WheelGlyph.svelte";

  let { category, cfg, filter = "" } = $props();

  const FINISH_GROUPS = [
    ["Solid", ["solid", "gloss", ""]],
    ["Metallic", ["metallic", "satin", "titanium"]],
    ["Special", ["pearl", "chrome", "carbon"]],
    ["Matte", ["matte", "suede", "fabric"]],
  ];

  const variantCode = $derived(cfg.result?.variant ?? cfg.variant);
  const available = (c) => !c.variants?.length || c.variants.includes(variantCode);
  const choices = $derived(
    filter ? category.choices.filter((c) => `${c.name} ${c.code} ${c.description}`.toLowerCase().includes(filter)) : category.choices,
  );
  const selected = $derived(cfg.choiceIn(category));
  const nameOf = (code) => {
    for (const cat of cfg.vehicle?.option_categories ?? []) {
      const hit = cat.choices.find((c) => c.code === code);
      if (hit) return hit.name;
    }
    return code;
  };

  const swatchGroups = $derived.by(() => {
    if (category.display !== "swatch") return [];
    const groups = FINISH_GROUPS.map(([label, finishes]) => ({
      label,
      items: choices.filter((c) => finishes.includes(c.finish ?? "")),
    })).filter((g) => g.items.length);
    // Only show finish headings when there's more than one group.
    return groups.length > 1 ? groups : [{ label: "", items: choices }];
  });

  function priceLabel(c) {
    const r = cfg.resolved.get(c.code);
    if (r?.origin === "included") return "Included";
    if (!c.price) return "No extra cost";
    return money(c.price);
  }

  function wheelSpec(c) {
    const part = c.visual?.find((v) => v.part === "wheel");
    const rim = c.visual?.find((v) => v.role === "rims");
    return { spec: part?.value ?? {}, color: rim?.color ?? "#c9ccd1" };
  }

  function swatchStyle(c) {
    const color = c.swatch_hex || c.visual?.find((v) => v.color)?.color || "#888";
    const f = c.finish;
    let overlay = "linear-gradient(145deg, rgba(255,255,255,0.28) 0%, rgba(255,255,255,0) 45%, rgba(0,0,0,0.18) 100%)";
    if (f === "metallic" || f === "satin" || f === "titanium") overlay = "linear-gradient(135deg, rgba(255,255,255,0.55) 0%, rgba(255,255,255,0) 40%, rgba(0,0,0,0.35) 100%)";
    if (f === "pearl") overlay = "linear-gradient(135deg, rgba(255,240,255,0.6) 0%, rgba(160,220,255,0.15) 45%, rgba(0,0,0,0.3) 100%)";
    if (f === "chrome") overlay = "linear-gradient(160deg, #fff 0%, rgba(255,255,255,0) 30%, rgba(0,0,0,0.45) 55%, rgba(255,255,255,0.7) 80%)";
    if (f === "matte" || f === "suede" || f === "fabric") overlay = "linear-gradient(145deg, rgba(255,255,255,0.08), rgba(0,0,0,0.08))";
    if (f === "carbon") overlay = "repeating-linear-gradient(45deg, rgba(255,255,255,0.12) 0 3px, rgba(0,0,0,0.2) 3px 6px)";
    return `background: ${overlay}, ${color}`;
  }

  function ruleHint(c) {
    const hints = [];
    if (c.requires?.length) hints.push(`Requires ${c.requires.map((r) => r.split("|").map(nameOf).join(" or ")).join(", ")}`);
    if (c.includes?.length) hints.push(`Includes ${c.includes.map(nameOf).join(", ")}`);
    if (c.excludes?.length) hints.push(`Not with ${c.excludes.map(nameOf).join(", ")}`);
    return hints;
  }
</script>

{#if choices.length}
  <section class="category" aria-labelledby={`cat-${category.id}`}>
    <header>
      <h3 id={`cat-${category.id}`} class="title">{category.name}</h3>
      {#if category.description}<p class="small muted">{category.description}</p>{/if}
    </header>

    {#if category.display === "swatch"}
      {#each swatchGroups as group}
        {#if group.label}<p class="group xsmall">{group.label}</p>{/if}
        <div class="swatches" role="radiogroup" aria-label={category.name}>
          {#each group.items as c (c.code)}
            {@const on = cfg.isSelected(c.code)}
            <button
              class="swatch"
              class:on
              disabled={!available(c)}
              role="radio"
              aria-checked={on}
              title={`${c.name} — ${priceLabel(c)}`}
              onclick={() => cfg.toggle(c, category)}
            >
              <span class="chip-color" style={swatchStyle(c)}></span>
              {#if on}<span class="tick"><Icon name="check" size={14} stroke={2.6} /></span>{/if}
            </button>
          {/each}
        </div>
      {/each}
      {#if selected}
        <div class="selected">
          <div>
            <p class="sel-name">{selected.name}</p>
            {#if selected.tag}<span class="tag">{selected.tag}</span>{/if}
          </div>
          <span class="num small">{priceLabel(selected)}</span>
        </div>
      {/if}
    {:else if category.display === "tile"}
      <div class="tiles" role="radiogroup" aria-label={category.name}>
        {#each choices as c (c.code)}
          {@const on = cfg.isSelected(c.code)}
          {@const w = wheelSpec(c)}
          <button class="tile" class:on disabled={!available(c)} role="radio" aria-checked={on} onclick={() => cfg.toggle(c, category)}>
            <span class="glyph"><WheelGlyph spec={w.spec} color={w.color} size={76} /></span>
            <span class="tile-name small">{c.name}</span>
            <span class="xsmall muted num">{priceLabel(c)}</span>
            {#if c.tag}<span class="tag tile-tag">{c.tag}</span>{/if}
            {#if c.requires?.length}<span class="xsmall subtle">{ruleHint(c)[0]}</span>{/if}
          </button>
        {/each}
      </div>
    {:else}
      <ul class="list" role={category.selection_type === "multi" ? "group" : "radiogroup"} aria-label={category.name}>
        {#each choices as c (c.code)}
          {@const on = cfg.isSelected(c.code)}
          {@const r = cfg.resolved.get(c.code)}
          {@const ok = available(c)}
          <li>
            <button
              class="row-opt"
              class:on
              disabled={!ok}
              role={category.selection_type === "multi" ? "checkbox" : "radio"}
              aria-checked={on}
              onclick={() => cfg.toggle(c, category)}
            >
              <span class="ctl" class:radio={category.selection_type !== "multi"}>
                {#if on}<Icon name="check" size={14} stroke={2.6} />{/if}
              </span>
              <span class="opt-main">
                <span class="opt-name">
                  {c.name}
                  {#if c.tag}<span class="tag">{c.tag}</span>{/if}
                </span>
                {#if c.description}<span class="small muted">{c.description}</span>{/if}
                {#if r && r.origin !== "user" && r.origin !== "default" && r.by_name}
                  <span class="xsmall origin">{r.origin === "included" ? "Included with" : "Required by"} {r.by_name}</span>
                {:else if !ok}
                  <span class="xsmall subtle">Not available for this variant</span>
                {:else}
                  {#each ruleHint(c) as hint}<span class="xsmall subtle">{hint}</span>{/each}
                {/if}
              </span>
              <span class="opt-price num small">{priceLabel(c)}</span>
            </button>
          </li>
        {/each}
      </ul>
    {/if}
  </section>
{/if}

<style>
  .category {
    padding: 24px 0;
    border-bottom: 1px solid var(--line);
  }
  .category:last-child {
    border-bottom: 0;
  }
  header {
    margin-bottom: 14px;
  }
  .title {
    font-size: 18px;
    font-weight: 600;
  }
  .group {
    margin: 12px 0 8px;
    color: var(--ink-3);
    font-weight: 600;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }
  .swatches {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
  }
  .swatch {
    position: relative;
    width: 48px;
    height: 48px;
    border-radius: 50%;
    padding: 3px;
    border: 2px solid transparent;
    transition: border-color 0.15s var(--ease), transform 0.15s var(--ease);
  }
  .swatch:hover {
    border-color: var(--line-strong);
  }
  .swatch.on {
    border-color: var(--ink);
  }
  .swatch:disabled {
    opacity: 0.3;
    cursor: not-allowed;
  }
  .chip-color {
    display: block;
    width: 100%;
    height: 100%;
    border-radius: 50%;
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.12);
  }
  .tick {
    position: absolute;
    right: -2px;
    bottom: -2px;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: var(--ink);
    color: #fff;
    display: grid;
    place-items: center;
    border: 2px solid #fff;
  }
  .selected {
    margin-top: 14px;
    display: flex;
    justify-content: space-between;
    align-items: flex-start;
    gap: 12px;
  }
  .selected > div {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }
  .sel-name {
    font-weight: 600;
  }
  .tiles {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(118px, 1fr));
    gap: 10px;
  }
  .tile {
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 6px;
    padding: 14px 10px 12px;
    border: 1.5px solid var(--line);
    border-radius: var(--radius);
    text-align: center;
    transition: border-color 0.15s var(--ease), background 0.15s var(--ease);
  }
  .tile:hover {
    border-color: var(--ink-2);
  }
  .tile.on {
    border-color: var(--ink);
    box-shadow: inset 0 0 0 1px var(--ink);
  }
  .tile:disabled {
    opacity: 0.35;
    cursor: not-allowed;
  }
  .glyph {
    display: grid;
    place-items: center;
  }
  .tile-name {
    font-weight: 500;
    line-height: 1.3;
  }
  .tile-tag {
    position: absolute;
    top: 6px;
    right: 6px;
    font-size: 10px;
    height: 18px;
    padding: 0 6px;
  }
  .list {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .row-opt {
    width: 100%;
    display: grid;
    grid-template-columns: 22px 1fr auto;
    gap: 14px;
    align-items: start;
    padding: 14px 14px;
    border: 1.5px solid var(--line);
    border-radius: var(--radius);
    text-align: left;
    transition: border-color 0.15s var(--ease), background 0.15s var(--ease);
  }
  .row-opt:hover {
    border-color: var(--ink-2);
  }
  .row-opt.on {
    border-color: var(--ink);
    background: #fafafa;
  }
  .row-opt:disabled {
    opacity: 0.45;
    cursor: not-allowed;
  }
  .ctl {
    width: 22px;
    height: 22px;
    border: 1.5px solid var(--line-strong);
    border-radius: 3px;
    display: grid;
    place-items: center;
    margin-top: 1px;
  }
  .ctl.radio {
    border-radius: 50%;
  }
  .on .ctl {
    background: var(--ink);
    border-color: var(--ink);
    color: #fff;
  }
  .opt-main {
    display: flex;
    flex-direction: column;
    gap: 3px;
    min-width: 0;
  }
  .opt-name {
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
  }
  .origin {
    color: var(--success);
    font-weight: 500;
  }
  .opt-price {
    white-space: nowrap;
    color: var(--ink-2);
  }
</style>
