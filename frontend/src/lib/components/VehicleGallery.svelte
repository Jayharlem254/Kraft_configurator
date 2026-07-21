<script>
  export let vehicles = [];
  export let loading = false;
  export let onSelect = () => {};

  function formatPrice(value) {
    const num = Number(value);
    if (Number.isNaN(num)) return "—";
    return num.toLocaleString("en-US", { maximumFractionDigits: 0 });
  }

  // Cursor-tracked spotlight on each card — position fed in as CSS vars and
  // painted with a radial-gradient in the stylesheet, so this stays a plain
  // style update rather than anything that re-triggers Svelte reactivity per
  // mouse-move.
  function trackPointer(e) {
    const rect = e.currentTarget.getBoundingClientRect();
    e.currentTarget.style.setProperty("--mx", `${e.clientX - rect.left}px`);
    e.currentTarget.style.setProperty("--my", `${e.clientY - rect.top}px`);
  }
</script>

<div class="gallery">
  <div class="intro">
    <h1>Build your own</h1>
    <p class="mono-label subtitle">SELECT A MODEL TO ENTER THE CONFIGURATOR</p>
  </div>

  {#if loading}
    <div class="grid" aria-hidden="true">
      {#each { length: 8 } as _}
        <div class="card skeleton">
          <div class="thumb"></div>
          <div class="meta">
            <div class="row">
              <span class="skel-line title"></span>
              <span class="skel-line price"></span>
            </div>
            <span class="skel-line tagline"></span>
          </div>
        </div>
      {/each}
    </div>
  {:else if vehicles.length === 0}
    <p class="mono-label state">NO VEHICLES PUBLISHED YET — CHECK THE DJANGO ADMIN</p>
  {:else}
    <div class="grid">
      {#each vehicles as v (v.slug)}
        <button class="card" on:click={() => onSelect(v)} on:pointermove={trackPointer}>
          <div class="glow" aria-hidden="true"></div>
          <div class="thumb">
            {#if v.thumbnail}
              <img src={v.thumbnail} alt={v.name} loading="lazy" />
            {:else}
              <div class="placeholder">
                <span class="corner tl"></span>
                <span class="corner tr"></span>
                <span class="corner bl"></span>
                <span class="corner br"></span>
                <span class="mono-label glyph">{v.vehicle_type}</span>
              </div>
            {/if}
            <div class="scrim"></div>
            <span class="mono-label cta">CONFIGURE →</span>
          </div>
          <div class="meta">
            <div class="row">
              <h3>{v.name}</h3>
              <span class="mono-label price">${formatPrice(v.base_price)}</span>
            </div>
            {#if v.tagline}
              <p class="tagline">{v.tagline}</p>
            {/if}
          </div>
        </button>
      {/each}
    </div>
  {/if}
</div>

<style>
  .gallery {
    max-width: 1240px;
    margin: 0 auto;
    padding: 12px 4px 40px;
  }

  .intro {
    padding: 28px 0 32px;
  }
  .intro h1 {
    font-size: 34px;
    margin-bottom: 8px;
  }
  .subtitle {
    color: var(--text-tertiary);
  }

  .state {
    margin: 80px auto;
    text-align: center;
    display: block;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
    gap: 22px;
  }

  .card {
    position: relative;
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-md);
    padding: 0;
    text-align: left;
    color: var(--text-primary);
    overflow: hidden;
    transition: border-color 0.18s, transform 0.18s, box-shadow 0.18s;
  }
  .card:hover {
    border-color: var(--purple-500);
    transform: translateY(-3px);
    box-shadow: 0 12px 30px -12px rgba(139, 92, 246, 0.45);
  }

  /* Cursor-tracked spotlight — --mx/--my set from pointermove in the script */
  .glow {
    position: absolute;
    inset: 0;
    z-index: 1;
    pointer-events: none;
    opacity: 0;
    transition: opacity 0.25s;
    background: radial-gradient(
      280px circle at var(--mx, 50%) var(--my, 50%),
      rgba(139, 92, 246, 0.16),
      transparent 70%
    );
  }
  .card:hover .glow {
    opacity: 1;
  }

  .skeleton {
    cursor: default;
  }
  .skeleton .thumb {
    background: linear-gradient(90deg, #100f17 0%, #1c1a29 50%, #100f17 100%);
    background-size: 200% 100%;
    animation: shimmer 1.6s ease-in-out infinite;
  }
  .skel-line {
    display: block;
    height: 12px;
    border-radius: 4px;
    background: linear-gradient(90deg, #14121d 0%, #221f30 50%, #14121d 100%);
    background-size: 200% 100%;
    animation: shimmer 1.6s ease-in-out infinite;
  }
  .skel-line.title {
    width: 55%;
    height: 15px;
  }
  .skel-line.price {
    width: 20%;
  }
  .skel-line.tagline {
    width: 80%;
    margin-top: 10px;
  }
  @keyframes shimmer {
    0% {
      background-position: 200% 0;
    }
    100% {
      background-position: -200% 0;
    }
  }
  @media (prefers-reduced-motion: reduce) {
    .skeleton .thumb,
    .skel-line {
      animation: none;
    }
  }

  .thumb {
    position: relative;
    aspect-ratio: 4 / 3;
    background: radial-gradient(circle at 50% 30%, #14121d 0%, #06060a 75%);
    overflow: hidden;
  }
  .thumb img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
  }

  .placeholder {
    position: absolute;
    inset: 14px;
  }
  .corner {
    position: absolute;
    width: 18px;
    height: 18px;
    border: 1.5px solid var(--purple-300);
    opacity: 0.55;
  }
  .corner.tl { top: 0; left: 0; border-right: none; border-bottom: none; }
  .corner.tr { top: 0; right: 0; border-left: none; border-bottom: none; }
  .corner.bl { bottom: 0; left: 0; border-right: none; border-top: none; }
  .corner.br { bottom: 0; right: 0; border-left: none; border-top: none; }
  .glyph {
    position: absolute;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    letter-spacing: 0.2em;
    color: var(--text-tertiary);
  }

  .scrim {
    position: absolute;
    inset: 0;
    background: linear-gradient(180deg, transparent 55%, rgba(6, 6, 10, 0.85) 100%);
    opacity: 0;
    transition: opacity 0.18s;
  }
  .card:hover .scrim {
    opacity: 1;
  }

  .cta {
    position: absolute;
    bottom: 14px;
    left: 16px;
    color: var(--purple-300);
    opacity: 0;
    transform: translateY(4px);
    transition: opacity 0.18s, transform 0.18s;
  }
  .card:hover .cta {
    opacity: 1;
    transform: translateY(0);
  }

  .meta {
    padding: 16px 18px 18px;
  }
  .row {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: 12px;
  }
  .row h3 {
    font-size: 17px;
  }
  .price {
    color: var(--metal-silver);
    white-space: nowrap;
  }
  .tagline {
    margin: 8px 0 0;
    font-size: 13px;
    color: var(--text-secondary);
    line-height: 1.4;
  }
</style>