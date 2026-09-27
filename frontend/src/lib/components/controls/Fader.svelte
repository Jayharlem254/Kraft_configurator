<script>
  import { createEventDispatcher } from "svelte";

  export let value = 0.5;
  export let min = 0;
  export let max = 1;
  export let step = 0.01;
  export let label = "";
  export let unit = "";
  export let width = 140;

  const dispatch = createEventDispatcher();

  function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
  }

  $: norm = clamp((value - min) / (max - min || 1), 0, 1);

  let trackEl;

  function setFromClientX(clientX) {
    if (!trackEl) return;
    const rect = trackEl.getBoundingClientRect();
    const ratio = clamp((clientX - rect.left) / rect.width, 0, 1);
    let next = min + ratio * (max - min);
    if (step > 0) {
      next = Math.round(next / step) * step;
    }
    value = next;
    dispatch("input", value);
    dispatch("change", value);
  }

  function pointerDown(e) {
    setFromClientX(e.clientX);
    window.addEventListener("pointermove", pointerMove);
    window.addEventListener("pointerup", pointerUp);
  }

  function pointerMove(e) {
    setFromClientX(e.clientX);
  }

  function pointerUp() {
    window.removeEventListener("pointermove", pointerMove);
    window.removeEventListener("pointerup", pointerUp);
    dispatch("commit", value);
  }
</script>

<div class="fader-root" style="width: {width}px;">
  {#if label}
    <div class="header row">
      <span class="label mono">{label}</span>
      <span class="value mono">{value.toFixed(2)}{unit}</span>
    </div>
  {/if}

  <!-- Debossed milled track -->
  <div
    class="track-slot milled"
    bind:this={trackEl}
    on:pointerdown={pointerDown}
    role="slider"
    tabindex="0"
    aria-valuenow={value}
    aria-valuemin={min}
    aria-valuemax={max}
  >
    <!-- Crystal blue active fill line -->
    <div class="fill" style="width: {norm * 100}%"></div>

    <!-- Extruded tactile slider thumb -->
    <div class="thumb raised" style="left: {norm * 100}%">
      <div class="thumb-led"></div>
    </div>
  </div>
</div>

<style>
  .fader-root {
    display: flex;
    flex-direction: column;
    gap: 4px;
    user-select: none;
  }

  .header {
    justify-content: space-between;
    font-size: 10px;
  }

  .label {
    color: var(--text-dim);
    text-transform: uppercase;
  }

  .value {
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .track-slot {
    position: relative;
    height: 14px;
    border-radius: 3px;
    cursor: pointer;
    overflow: visible;
  }

  .fill {
    position: absolute;
    top: 4px;
    bottom: 4px;
    left: 4px;
    background: linear-gradient(90deg, var(--crystal-deep), var(--crystal-blue));
    border-radius: 2px;
    box-shadow: 0 0 6px var(--crystal-glow);
    pointer-events: none;
  }

  .thumb {
    position: absolute;
    top: -2px;
    width: 10px;
    height: 18px;
    margin-left: -5px;
    border-radius: 2px;
    cursor: ew-resize;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: transform 0.05s ease;
  }
  .thumb:active {
    transform: scale(0.95);
  }

  .thumb-led {
    width: 2px;
    height: 10px;
    background: var(--crystal-bright);
    box-shadow: 0 0 4px var(--crystal-blue);
    border-radius: 1px;
  }
</style>
