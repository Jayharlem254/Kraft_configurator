<script>
  import { createEventDispatcher } from "svelte";
  import { api } from "../api.js";
  import { effectiveSelections, getSessionKey } from "../selections.js";

  export let vehicle;
  /** @type {Record<number, number>} categoryId -> selected choiceId */
  export let selections = {};

  const dispatch = createEventDispatcher();

  let saveState = "idle"; // "idle" | "saving" | "saved" | "error"
  let priceFlash = false;

  function selectChoice(categoryId, choiceId) {
    selections = { ...selections, [categoryId]: choiceId };
    dispatch("change", { selections });
  }

  function priceFor(catId, choiceId) {
    const category = vehicle.option_categories.find((c) => c.id === catId);
    return category?.choices.find((c) => c.id === choiceId)?.price_delta ?? 0;
  }

  // Price for every category, not just the ones the user has explicitly
  // clicked — an untouched category is still showing (and charging for) its
  // is_default choice, so that has to be counted too.
  $: effective = effectiveSelections(vehicle, selections);
  $: totalPrice =
    Number(vehicle.base_price) +
    Object.entries(effective).reduce((sum, [catId, choiceId]) => sum + Number(priceFor(Number(catId), choiceId)), 0);

  // Brief pulse on the price readout whenever it changes, so a choice swap
  // reads as an immediate, tangible effect rather than a silent number swap.
  let previousPrice = totalPrice;
  $: if (totalPrice !== previousPrice) {
    previousPrice = totalPrice;
    priceFlash = false;
    requestAnimationFrame(() => (priceFlash = true));
  }

  function formatPrice(value) {
    return new Intl.NumberFormat("en-US", {
      style: "currency",
      currency: "USD",
      maximumFractionDigits: 0,
    }).format(value);
  }

  async function saveConfiguration() {
    saveState = "saving";
    try {
      await api.createConfiguration(vehicle.id, Object.values(effective), getSessionKey());
      saveState = "saved";
    } catch (err) {
      console.warn("Couldn't save configuration — is the Django API running?", err);
      saveState = "error";
    }
    setTimeout(() => (saveState = "idle"), 2400);
  }
</script>

<aside>
  <div class="header-block">
    <h2>{vehicle.name}</h2>
    <p class="tagline">{vehicle.tagline}</p>
  </div>

  <div class="price-block">
    <span class="mono-label">EST. PRICE</span>
    <span class="price" class:flash={priceFlash}>{formatPrice(totalPrice)}</span>
  </div>

  {#each vehicle.option_categories as category}
    <section class="category">
      <h3 class="mono-label category-title">{category.name}</h3>

      <div class="choices" class:swatches={category.choices.some((c) => c.swatch_hex)}>
        {#each category.choices as choice}
          {@const selected =
            (selections[category.id] ?? category.choices.find((c) => c.is_default)?.id) === choice.id}

          {#if choice.swatch_hex}
            <button
              class="swatch"
              class:selected
              style="--swatch-color: {choice.swatch_hex}"
              title={choice.name}
              aria-label={choice.name}
              on:click={() => selectChoice(category.id, choice.id)}
            ></button>
          {:else}
            <button class="choice-row" class:selected on:click={() => selectChoice(category.id, choice.id)}>
              <span>{choice.name}</span>
              <span class="mono-label delta">
                {choice.price_delta > 0 ? `+${formatPrice(choice.price_delta)}` : "Included"}
              </span>
            </button>
          {/if}
        {/each}
      </div>
    </section>
  {/each}

  <button class="cta" class:success={saveState === "saved"} class:error={saveState === "error"} disabled={saveState === "saving"} on:click={saveConfiguration}>
    {#if saveState === "saving"}
      Saving…
    {:else if saveState === "saved"}
      Saved ✓
    {:else if saveState === "error"}
      Couldn't save — try again
    {:else}
      Save configuration
    {/if}
  </button>
</aside>

<style>
  aside {
    background: var(--panel);
    border-radius: var(--radius-lg);
    border: 1px solid var(--line);
    padding: 24px;
    display: flex;
    flex-direction: column;
    gap: 22px;
    overflow-y: auto;
  }

  .header-block h2 {
    font-size: 22px;
  }
  .tagline {
    margin: 6px 0 0;
    color: var(--text-secondary);
    font-size: 13px;
  }

  .price-block {
    display: flex;
    flex-direction: column;
    gap: 4px;
    padding: 14px 16px;
    border-radius: var(--radius-md);
    background: var(--panel-raised);
    border: 1px solid var(--line);
  }
  .price {
    display: inline-block;
    font-family: var(--font-mono);
    font-size: 24px;
    font-weight: 500;
    color: var(--purple-300);
  }
  .price.flash {
    animation: price-pulse 0.5s ease;
  }
  @keyframes price-pulse {
    0% {
      color: var(--text-primary);
      transform: scale(1.06);
    }
    100% {
      color: var(--purple-300);
      transform: scale(1);
    }
  }

  .category-title {
    margin-bottom: 10px;
  }

  .choices {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .choices.swatches {
    flex-direction: row;
    flex-wrap: wrap;
    gap: 10px;
  }

  .choice-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    background: var(--panel-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius-sm);
    padding: 10px 14px;
    color: var(--text-primary);
    font-size: 13px;
    text-align: left;
  }
  .choice-row.selected {
    border-color: var(--purple-500);
    box-shadow: 0 0 0 1px var(--purple-500) inset;
  }
  .delta {
    color: var(--text-tertiary);
  }
  .choice-row.selected .delta {
    color: var(--purple-300);
  }

  .swatch {
    width: 34px;
    height: 34px;
    border-radius: 50%;
    background: var(--swatch-color);
    border: 2px solid var(--line);
    padding: 0;
    transition: transform 0.1s, border-color 0.15s;
  }
  .swatch:hover {
    transform: scale(1.08);
  }
  .swatch.selected {
    border-color: var(--purple-300);
    box-shadow: 0 0 0 3px rgba(196, 181, 253, 0.25);
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
  .cta:hover:not(:disabled) {
    filter: brightness(1.1);
  }
  .cta:disabled {
    opacity: 0.7;
    cursor: default;
  }
  .cta.success {
    background: var(--success);
  }
  .cta.error {
    background: var(--danger);
  }
</style>
