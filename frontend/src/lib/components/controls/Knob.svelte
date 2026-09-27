<script>
  import { createEventDispatcher } from "svelte";

  export let value = 0.5;
  export let min = 0;
  export let max = 1;
  export let step = 0.01;
  export let size = 38;
  export let label = "";
  export let unit = "";
  export let bipolar = false;
  export let def = 0.5;

  const dispatch = createEventDispatcher();

  const A0 = -135;
  const A1 = 135;
  const R = 15;

  function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
  }

  $: norm = clamp((value - min) / (max - min || 1), 0, 1);
  $: ang = A0 + norm * (A1 - A0);

  function polarR(deg, r) {
    const a = ((deg - 90) * Math.PI) / 180;
    return [20 + r * Math.cos(a), 20 + r * Math.sin(a)];
  }

  function polar(deg) {
    return polarR(deg, R);
  }

  function arc(a0, a1) {
    const p0 = polar(a0);
    const p1 = polar(a1);
    const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
    const sweep = a1 > a0 ? 1 : 0;
    return `M ${p0[0].toFixed(2)} ${p0[1].toFixed(2)} A ${R} ${R} 0 ${large} ${sweep} ${p1[0].toFixed(2)} ${p1[1].toFixed(2)}`;
  }

  $: bgPath = arc(A0, A1);
  $: valPath = bipolar
    ? norm >= 0.5
      ? arc(A0 + 0.5 * (A1 - A0), ang)
      : arc(ang, A0 + 0.5 * (A1 - A0))
    : arc(A0, ang);

  $: ptr = polarR(ang, 9);

  let startY = 0;
  let startVal = 0;

  function down(e) {
    if (e.button !== 0) return;
    if (e.ctrlKey || e.metaKey) {
      reset();
      return;
    }
    startY = e.clientY;
    startVal = value;
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
  }

  function move(e) {
    const delta = ((startY - e.clientY) / 130) * (max - min);
    let next = clamp(startVal + delta, min, max);
    if (step > 0) {
      next = Math.round(next / step) * step;
    }
    value = next;
    dispatch("input", value);
    dispatch("change", value);
  }

  function up() {
    window.removeEventListener("pointermove", move);
    window.removeEventListener("pointerup", up);
    dispatch("commit", value);
  }

  function reset() {
    value = def ?? (bipolar ? (min + max) / 2 : min);
    dispatch("input", value);
    dispatch("change", value);
    dispatch("commit", value);
  }

  function wheel(e) {
    e.preventDefault();
    const dir = e.deltaY < 0 ? 1 : -1;
    const inc = (step || 0.05) * dir;
    value = clamp(value + inc, min, max);
    dispatch("input", value);
    dispatch("change", value);
    dispatch("commit", value);
  }
</script>

<div class="knob-container" style="width: {size}px;" title={label ? `${label}: ${value.toFixed(2)}${unit}` : ""}>
  <svg
    class="enc"
    width={size}
    height={size}
    viewBox="0 0 40 40"
    role="slider"
    tabindex="0"
    aria-valuenow={value}
    aria-valuemin={min}
    aria-valuemax={max}
    aria-label={label || "Knob"}
    on:pointerdown={down}
    on:dblclick={reset}
    on:wheel={wheel}
  >
    <defs>
      <!-- Tactile machined metal cap gradient -->
      <radialGradient id="capface" cx="35%" cy="30%" r="75%">
        <stop offset="0%" stop-color="#2a3648" />
        <stop offset="60%" stop-color="#16202c" />
        <stop offset="100%" stop-color="#0b1118" />
      </radialGradient>
      <!-- Crystal blue glow filter -->
      <filter id="c-glow" x="-20%" y="-20%" width="140%" height="140%">
        <feGaussianBlur stdDeviation="1.2" result="blur" />
        <feComposite in="SourceGraphic" in2="blur" operator="over" />
      </filter>
    </defs>

    <!-- Recessed debossed track groove -->
    <path d={bgPath} fill="none" stroke="#070a0e" stroke-width="3.4" stroke-linecap="round" />
    <path d={bgPath} fill="none" stroke="#1c2634" stroke-width="2.6" stroke-linecap="round" />

    <!-- Crystal blue active value arc -->
    <path
      d={valPath}
      fill="none"
      stroke="var(--crystal-blue)"
      stroke-width="2.6"
      stroke-linecap="round"
      filter="url(#c-glow)"
    />

    <!-- Extruded machined rotary dial cap -->
    <circle cx="20" cy="20" r="11" fill="url(#capface)" />
    <circle cx="20" cy="20" r="11" fill="none" stroke="rgba(255,255,255,0.12)" stroke-width="0.8" />
    <circle cx="20" cy="20" r="10.2" fill="none" stroke="rgba(0,0,0,0.6)" stroke-width="0.8" />

    <!-- Crystal blue indicator tick -->
    <circle cx={ptr[0]} cy={ptr[1]} r="1.6" fill="var(--crystal-bright)" filter="url(#c-glow)" />
  </svg>

  {#if label}
    <span class="label mono">{label}</span>
  {/if}
</div>

<style>
  .knob-container {
    display: inline-flex;
    flex-direction: column;
    align-items: center;
    gap: 3px;
    user-select: none;
  }

  .enc {
    cursor: ns-resize;
    display: block;
    filter: drop-shadow(0 2px 4px rgba(0, 0, 0, 0.6));
    transition: transform 0.08s ease;
  }
  .enc:active {
    transform: scale(0.97);
  }

  .label {
    font-size: 9px;
    letter-spacing: 0.05em;
    color: var(--text-dim);
    text-transform: uppercase;
    text-align: center;
    white-space: nowrap;
  }
</style>
