<script>
  import { createEventDispatcher } from "svelte";
  import { api } from "../api.js";
  import { effectiveSelections, getSessionKey } from "../selections.js";
  import Knob from "./controls/Knob.svelte";
  import Fader from "./controls/Fader.svelte";

  export let vehicle;
  /** @type {Record<number, number>} categoryId -> selected choiceId */
  export let selections = {};

  const dispatch = createEventDispatcher();

  let saveState = "idle"; // "idle" | "saving" | "saved" | "error"
  let priceFlash = false;

  // Realtime tactile paint & finish parameters (Quni4m hardware feel)
  let paintGloss = 0.85;
  let paintMetallic = 0.90;
  let paintClearcoat = 0.80;

  function selectChoice(categoryId, choiceId) {
    selections = { ...selections, [categoryId]: choiceId };
    dispatch("change", { selections });
  }

  function priceFor(catId, choiceId) {
    const category = vehicle.option_categories?.find((c) => c.id === catId);
    return category?.choices.find((c) => c.id === choiceId)?.price_delta ?? 0;
  }

  $: effective = effectiveSelections(vehicle, selections);
  $: totalPrice =
    Number(vehicle.base_price || 0) +
    Object.entries(effective).reduce(
      (sum, [catId, choiceId]) => sum + Number(priceFor(Number(catId), choiceId)),
      0
    );

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
      console.warn("Couldn't save configuration to C++ engine:", err);
      saveState = "error";
    }
    setTimeout(() => (saveState = "idle"), 2400);
  }
</script>

<aside class="panel-rack">
  <!-- Header Deck -->
  <div class="deck-header">
    <div class="model-lineage">
      <span class="mono-label lineage-badge">SPEC DECK // {vehicle.vehicle_type?.toUpperCase() || 'CHASSIS'}</span>
      <h2 class="vehicle-heading">{vehicle.name}</h2>
      <p class="tagline">{vehicle.tagline || 'Precision engineered vehicle'}</p>
    </div>

    <!-- Crystal Blue Machined Price Display (Quni4m segment readout) -->
    <div class="price-readout milled">
      <div class="price-top row">
        <span class="mono-label">ESTIMATED TOTAL</span>
        <span class="mono base-tag dim">BASE: {formatPrice(vehicle.base_price)}</span>
      </div>
      <div class="price-amount" class:flash={priceFlash}>
        {formatPrice(totalPrice)}
      </div>
    </div>
  </div>

  <div class="rack-content">
    <!-- Tactile Finish Customizer (Quni4m Rotary Encoders) -->
    <section class="rack-section">
      <div class="section-banner row">
        <span class="section-led"></span>
        <h3 class="mono-label section-title">SURFACE PHYSICS / FINISH TUNER</h3>
      </div>
      <div class="knob-row row">
        <Knob
          bind:value={paintGloss}
          min={0}
          max={1}
          step={0.01}
          size={38}
          label="GLOSS"
          on:change={(e) => dispatch("finishChange", { gloss: e.detail, metallic: paintMetallic, clearcoat: paintClearcoat })}
        />
        <Knob
          bind:value={paintMetallic}
          min={0}
          max={1}
          step={0.01}
          size={38}
          label="METALLIC"
          on:change={(e) => dispatch("finishChange", { gloss: paintGloss, metallic: e.detail, clearcoat: paintClearcoat })}
        />
        <Knob
          bind:value={paintClearcoat}
          min={0}
          max={1}
          step={0.01}
          size={38}
          label="CLEARCOAT"
          on:change={(e) => dispatch("finishChange", { gloss: paintGloss, metallic: paintMetallic, clearcoat: e.detail })}
        />
      </div>
    </section>

    <!-- Option Categories & Choices -->
    {#if vehicle.option_categories}
      {#each vehicle.option_categories as category}
        <section class="rack-section">
          <div class="section-banner row">
            <span class="section-led"></span>
            <h3 class="mono-label section-title">{category.name}</h3>
          </div>

          <div class="choices" class:swatches={category.choices.some((c) => c.swatch_hex)}>
            {#each category.choices as choice}
              {@const selected =
                (selections[category.id] ?? category.choices.find((c) => c.is_default)?.id) === choice.id}

              {#if choice.swatch_hex}
                <button
                  class="swatch-btn raised"
                  class:selected
                  style="--swatch-color: {choice.swatch_hex}"
                  title="{choice.name} ({choice.price_delta > 0 ? `+${formatPrice(choice.price_delta)}` : 'Included'})"
                  aria-label={choice.name}
                  on:click={() => selectChoice(category.id, choice.id)}
                >
                  <span class="swatch-disc" style="background-color: {choice.swatch_hex}"></span>
                  {#if selected}
                    <span class="swatch-ring"></span>
                  {/if}
                </button>
              {:else}
                <button
                  class="choice-card"
                  class:selected
                  class:raised={!selected}
                  class:milled={selected}
                  on:click={() => selectChoice(category.id, choice.id)}
                >
                  <div class="choice-info">
                    <span class="choice-name">{choice.name}</span>
                    {#if choice.description}
                      <span class="choice-desc">{choice.description}</span>
                    {/if}
                  </div>
                  <span class="choice-delta mono" class:nonzero={choice.price_delta > 0}>
                    {choice.price_delta > 0 ? `+${formatPrice(choice.price_delta)}` : "INCL"}
                  </span>
                </button>
              {/if}
            {/each}
          </div>
        </section>
      {/each}
    {/if}
  </div>

  <!-- Rack Footer Actions -->
  <div class="deck-footer">
    <button
      class="save-btn raised"
      class:saving={saveState === "saving"}
      class:saved={saveState === "saved"}
      class:error={saveState === "error"}
      disabled={saveState === "saving"}
      on:click={saveConfiguration}
    >
      <div class="btn-glow"></div>
      <span class="mono btn-label">
        {#if saveState === "idle"}
          SAVE CONFIGURATION [C++]
        {:else if saveState === "saving"}
          SAVING TO C++ ENGINE…
        {:else if saveState === "saved"}
          ✓ SAVED TO SQLITE
        {:else}
          ⚠ SAVE FAILED
        {/if}
      </span>
    </button>
  </div>
</aside>

<style>
  .panel-rack {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: var(--panel);
    border-left: 1px solid var(--line);
    overflow: hidden;
  }

  .deck-header {
    padding: 14px 16px 10px 16px;
    background: var(--panel);
    border-bottom: 1px solid var(--line);
    flex-shrink: 0;
  }

  .lineage-badge {
    color: var(--crystal-bright);
    font-size: 9px;
    letter-spacing: 0.14em;
  }

  .vehicle-heading {
    font-size: 19px;
    color: var(--text-hi);
    margin-top: 2px;
  }

  .tagline {
    font-size: 11px;
    color: var(--text-dim);
    margin: 2px 0 10px 0;
  }

  .price-readout {
    padding: 8px 12px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .price-top {
    justify-content: space-between;
    font-size: 9px;
    margin-bottom: 2px;
  }

  .base-tag {
    font-size: 9px;
  }

  .price-amount {
    font-family: var(--font-mono);
    font-size: 20px;
    font-weight: 700;
    color: var(--crystal-bright);
    text-shadow: 0 0 10px var(--crystal-glow);
    transition: transform 0.15s ease, color 0.15s ease;
  }

  .price-amount.flash {
    color: #ffffff;
    text-shadow: 0 0 16px var(--crystal-bright);
    transform: scale(1.02);
  }

  .rack-content {
    flex: 1;
    overflow-y: auto;
    padding: 12px 16px;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }

  .rack-section {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .section-banner {
    gap: 6px;
  }

  .section-led {
    width: 4px;
    height: 4px;
    border-radius: 50%;
    background: var(--crystal-blue);
    box-shadow: 0 0 4px var(--crystal-blue);
  }

  .section-title {
    font-size: 10px;
    letter-spacing: 0.08em;
    color: var(--text-dim);
  }

  .knob-row {
    justify-content: space-around;
    padding: 10px;
    background: var(--surface);
    border: 1px solid var(--line-soft);
    border-radius: var(--radius);
  }

  .choices {
    display: flex;
    flex-direction: column;
    gap: 5px;
  }

  .choices.swatches {
    flex-direction: row;
    flex-wrap: wrap;
    gap: 8px;
  }

  .swatch-btn {
    position: relative;
    width: 32px;
    height: 32px;
    border-radius: 50%;
    border: 1px solid var(--line-soft);
    display: grid;
    place-items: center;
    padding: 0;
    cursor: pointer;
  }

  .swatch-disc {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    box-shadow: inset 0 1px 2px rgba(255, 255, 255, 0.25), 0 2px 4px rgba(0, 0, 0, 0.4);
  }

  .swatch-btn.selected {
    border-color: var(--crystal-bright);
    box-shadow: 0 0 10px var(--crystal-glow);
  }

  .swatch-ring {
    position: absolute;
    inset: -3px;
    border: 1.5px solid var(--crystal-blue);
    border-radius: 50%;
    box-shadow: 0 0 6px var(--crystal-blue);
  }

  .choice-card {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 12px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
    color: var(--text);
    cursor: pointer;
    text-align: left;
    transition: all 0.12s ease;
  }

  .choice-card:hover {
    border-color: var(--crystal-blue);
  }

  .choice-card.selected {
    border-color: var(--crystal-blue);
    background: rgba(0, 210, 255, 0.08);
    box-shadow: inset 0 0 8px rgba(0, 210, 255, 0.15);
  }

  .choice-info {
    display: flex;
    flex-direction: column;
    gap: 2px;
  }

  .choice-name {
    font-size: 11px;
    font-weight: 600;
    color: var(--text-hi);
  }

  .choice-desc {
    font-size: 10px;
    color: var(--text-dim);
  }

  .choice-delta {
    font-size: 10px;
    color: var(--text-dim);
  }

  .choice-delta.nonzero {
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .deck-footer {
    padding: 12px 16px;
    border-top: 1px solid var(--line);
    background: var(--panel);
    flex-shrink: 0;
  }

  .save-btn {
    position: relative;
    width: 100%;
    padding: 9px 12px;
    border-radius: var(--radius);
    border: 1px solid var(--crystal-deep);
    background: linear-gradient(180deg, #022742 0%, #011422 100%);
    color: var(--crystal-bright);
    cursor: pointer;
    overflow: hidden;
    transition: all 0.15s ease;
  }

  .save-btn:hover:not(:disabled) {
    border-color: var(--crystal-blue);
    box-shadow: 0 0 12px var(--crystal-glow);
    color: #ffffff;
  }

  .save-btn:active:not(:disabled) {
    transform: var(--press-shift);
  }

  .btn-label {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.06em;
  }

  .save-btn.saved {
    border-color: var(--led-emerald);
    color: var(--led-emerald);
    background: rgba(70, 209, 138, 0.1);
  }

  .save-btn.error {
    border-color: var(--led-red);
    color: var(--led-red);
    background: rgba(255, 84, 104, 0.1);
  }
</style>
