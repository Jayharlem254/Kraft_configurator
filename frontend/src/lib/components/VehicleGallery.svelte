<script>
  export let vehicles = [];
  export let loading = false;
  export let onSelect = () => {};
  export let onUpload = () => {};

  function formatPrice(value) {
    const num = Number(value);
    if (Number.isNaN(num)) return "—";
    return num.toLocaleString("en-US", { maximumFractionDigits: 0 });
  }

  function trackPointer(e) {
    const rect = e.currentTarget.getBoundingClientRect();
    e.currentTarget.style.setProperty("--mx", `${e.clientX - rect.left}px`);
    e.currentTarget.style.setProperty("--my", `${e.clientY - rect.top}px`);
  }
</script>

<div class="gallery">
  <div class="intro">
    <div class="intro-badge row">
      <span class="dot-led"></span>
      <span class="mono-label">FLEET CATALOG // C++ HIGH PERFORMANCE ENGINE</span>
    </div>
    <h1>PRECISION VEHICLES</h1>
    <p class="mono-label subtitle">SELECT A VEHICLE PLATFORM TO INITIALIZE REAL-TIME 3D CONFIGURATION</p>
  </div>

  {#if loading}
    <div class="grid" aria-hidden="true">
      {#each { length: 6 } as _}
        <div class="card skeleton milled">
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
    <div class="empty-gallery milled">
      <div class="empty-icon">
        <svg width="42" height="42" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
          <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z" />
          <polyline points="3.27 6.96 12 12.01 20.73 6.96" />
          <line x1="12" y1="22.08" x2="12" y2="12" />
        </svg>
      </div>
      <h3 class="mono empty-gallery-title">NO VEHICLES REGISTERED</h3>
      <p class="mono-label empty-gallery-sub">PLACEHOLDERS REMOVED — READY FOR YOUR ACTUAL 3D MODELS</p>
      <button class="btn-upload-first raised mono" on:click={onUpload}>
        + UPLOAD 3D MODEL (.GLB / .GLTF)
      </button>
    </div>
  {:else}
    <div class="grid">
      {#each vehicles as v (v.slug)}
        <button class="card raised" on:click={() => onSelect(v)} on:pointermove={trackPointer}>
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
                <span class="mono-label glyph">{v.vehicle_type?.toUpperCase() || 'PLATFORM'}</span>
              </div>
            {/if}
            <div class="scrim"></div>
            <div class="type-tag mono">{v.vehicle_type?.toUpperCase()}</div>
            <span class="mono-label cta">CONFIGURE STUDIO →</span>
          </div>
          <div class="meta">
            <div class="title-row row">
              <h3 class="name">{v.name}</h3>
              <span class="price mono">${formatPrice(v.base_price)}</span>
            </div>
            {#if v.tagline}
              <p class="tagline">{v.tagline}</p>
            {/if}

            <!-- Quick Specs Row -->
            {#if v.specs}
              <div class="specs-preview row">
                {#if v.specs.power}
                  <span class="spec-tag mono">{v.specs.power}</span>
                {/if}
                {#if v.specs.acceleration}
                  <span class="spec-tag mono">{v.specs.acceleration}</span>
                {/if}
                {#if v.specs.top_speed}
                  <span class="spec-tag mono">{v.specs.top_speed}</span>
                {/if}
              </div>
            {/if}
          </div>
        </button>
      {/each}

      <!-- Quick Upload Card -->
      <button class="card raised upload-card" on:click={onUpload} on:pointermove={trackPointer}>
        <div class="glow" aria-hidden="true"></div>
        <div class="thumb upload-thumb">
          <div class="upload-icon">
            <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
              <polyline points="17 8 12 3 7 8" />
              <line x1="12" y1="3" x2="12" y2="15" />
            </svg>
          </div>
          <span class="mono-label upload-label">IMPORT 3D MODEL</span>
          <span class="upload-sub dim mono">.GLB / .GLTF / .OBJ</span>
        </div>
        <div class="meta">
          <div class="row spread">
            <span class="name upload-name">+ ADD NEW VEHICLE</span>
            <span class="tag-upload mono">C++ INGEST</span>
          </div>
          <p class="tagline">Stream high-poly 3D models directly to native backend storage</p>
        </div>
      </button>
    </div>
  {/if}
</div>

<style>
  .gallery {
    max-width: 1260px;
    margin: 0 auto;
    padding: 16px 12px 40px;
  }

  .intro {
    padding: 20px 0 24px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .intro-badge {
    gap: 6px;
    font-size: 10px;
    color: var(--crystal-bright);
  }

  .dot-led {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--crystal-blue);
    box-shadow: 0 0 6px var(--crystal-blue);
  }

  .intro h1 {
    font-size: 28px;
    letter-spacing: -0.02em;
    color: var(--text-hi);
  }

  .subtitle {
    color: var(--text-dim);
    font-size: 10px;
  }

  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(310px, 1fr));
    gap: 20px;
  }

  .card {
    position: relative;
    background: var(--surface);
    border: 1px solid var(--line-soft);
    border-radius: var(--radius-md);
    padding: 0;
    text-align: left;
    color: var(--text);
    overflow: hidden;
    cursor: pointer;
    transition: border-color 0.18s ease, transform 0.18s ease, box-shadow 0.18s ease;
  }

  .card:hover {
    border-color: var(--crystal-blue);
    transform: translateY(-2px);
    box-shadow: 0 10px 24px -10px var(--crystal-glow);
  }

  /* Cursor-tracked spotlight in Crystal Blue */
  .glow {
    position: absolute;
    inset: 0;
    z-index: 1;
    pointer-events: none;
    opacity: 0;
    transition: opacity 0.25s;
    background: radial-gradient(
      320px circle at var(--mx, 50%) var(--my, 50%),
      rgba(0, 210, 255, 0.14),
      transparent 70%
    );
  }
  .card:hover .glow {
    opacity: 1;
  }

  .thumb {
    position: relative;
    aspect-ratio: 16 / 10;
    background: radial-gradient(circle at 50% 30%, #151d28 0%, #070a0e 85%);
    overflow: hidden;
    border-bottom: 1px solid var(--line);
  }

  .thumb img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
    transition: transform 0.3s ease;
  }
  .card:hover .thumb img {
    transform: scale(1.03);
  }

  .placeholder {
    position: absolute;
    inset: 0;
    display: grid;
    place-items: center;
    background: radial-gradient(circle at 50% 40%, #172230 0%, #080c10 80%);
  }

  .corner {
    position: absolute;
    width: 10px;
    height: 10px;
    border: 1px solid var(--crystal-blue);
    opacity: 0.6;
  }
  .corner.tl { top: 12px; left: 12px; border-right: none; border-bottom: none; }
  .corner.tr { top: 12px; right: 12px; border-left: none; border-bottom: none; }
  .corner.bl { bottom: 12px; left: 12px; border-right: none; border-top: none; }
  .corner.br { bottom: 12px; right: 12px; border-left: none; border-top: none; }

  .glyph {
    color: var(--crystal-bright);
    letter-spacing: 0.2em;
    font-size: 11px;
  }

  .type-tag {
    position: absolute;
    top: 10px;
    left: 10px;
    background: rgba(12, 15, 19, 0.75);
    backdrop-filter: blur(4px);
    border: 1px solid var(--line-soft);
    padding: 2px 6px;
    font-size: 9px;
    letter-spacing: 0.08em;
    color: var(--text-dim);
    border-radius: var(--radius-sm);
  }

  .scrim {
    position: absolute;
    inset: 0;
    background: linear-gradient(180deg, transparent 40%, rgba(8, 11, 15, 0.8) 100%);
  }

  .cta {
    position: absolute;
    bottom: 10px;
    right: 12px;
    color: var(--crystal-bright);
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 0.08em;
    opacity: 0;
    transform: translateX(-4px);
    transition: all 0.18s ease;
  }
  .card:hover .cta {
    opacity: 1;
    transform: translateX(0);
  }

  .meta {
    padding: 12px 14px 14px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .title-row {
    justify-content: space-between;
    align-items: baseline;
  }

  .name {
    font-size: 15px;
    color: var(--text-hi);
  }

  .price {
    font-size: 13px;
    font-weight: 700;
    color: var(--crystal-bright);
  }

  .tagline {
    font-size: 11px;
    color: var(--text-dim);
    margin: 0;
    line-height: 1.35;
  }

  .specs-preview {
    gap: 6px;
    margin-top: 4px;
  }

  .spec-tag {
    font-size: 9px;
    background: var(--panel);
    border: 1px solid var(--line-soft);
    padding: 2px 5px;
    border-radius: 2px;
    color: var(--text-secondary);
  }

  /* Skeleton loading state */
  .skeleton .thumb {
    background: linear-gradient(90deg, #0e131a 0%, #19222f 50%, #0e131a 100%);
    background-size: 200% 100%;
    animation: shimmer 1.6s ease-in-out infinite;
  }
  .skel-line {
    display: block;
    height: 12px;
    border-radius: 3px;
    background: linear-gradient(90deg, #10161f 0%, #1e2837 50%, #10161f 100%);
    background-size: 200% 100%;
    animation: shimmer 1.6s ease-in-out infinite;
  }
  .skel-line.title { width: 50%; height: 14px; }
  .skel-line.price { width: 25%; }
  .skel-line.tagline { width: 80%; margin-top: 8px; }

  @keyframes shimmer {
    0% { background-position: 200% 0; }
    100% { background-position: -200% 0; }
  }

  .upload-card {
    border: 1px dashed var(--line-bright);
    background: rgba(19, 23, 31, 0.6);
  }

  .upload-card:hover {
    border-color: var(--crystal-blue);
    border-style: solid;
    box-shadow: 0 0 16px rgba(0, 210, 255, 0.15);
  }

  .upload-thumb {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    background: var(--field);
    gap: 8px;
  }

  .upload-icon {
    color: var(--crystal-bright);
    opacity: 0.85;
    transition: transform 0.2s ease;
  }

  .upload-card:hover .upload-icon {
    transform: translateY(-2px) scale(1.05);
    color: var(--crystal-blue);
  }

  .upload-label {
    font-size: 11px;
    letter-spacing: 0.08em;
    color: var(--text-hi);
  }

  .upload-sub {
    font-size: 9px;
  }

  .upload-name {
    color: var(--crystal-bright);
  }

  .tag-upload {
    font-size: 9px;
    background: rgba(0, 210, 255, 0.15);
    border: 1px solid var(--crystal-blue);
    color: var(--crystal-bright);
    padding: 1px 6px;
    border-radius: 3px;
  }

  .empty-gallery {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    text-align: center;
    padding: 48px 24px;
    margin: 40px auto;
    max-width: 540px;
    border-radius: var(--radius-lg);
    background: var(--surface);
    border: 1px solid var(--line);
  }

  .empty-icon {
    color: var(--crystal-blue);
    opacity: 0.8;
    margin-bottom: 16px;
  }

  .empty-gallery-title {
    font-size: 15px;
    letter-spacing: 0.12em;
    color: var(--text-hi);
    margin: 0 0 8px;
  }

  .empty-gallery-sub {
    font-size: 10px;
    letter-spacing: 0.08em;
    color: var(--text-dim);
    margin: 0 0 24px;
  }

  .btn-upload-first {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    padding: 10px 22px;
    background: linear-gradient(135deg, #00d2ff, #0099cc);
    border: 1px solid var(--crystal-bright);
    color: #05070a;
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.08em;
    border-radius: var(--radius);
    cursor: pointer;
    box-shadow: 0 0 16px var(--crystal-glow);
    transition: all 0.15s ease;
  }

  .btn-upload-first:hover {
    transform: translateY(-1px);
    box-shadow: 0 0 24px var(--crystal-glow);
    background: linear-gradient(135deg, #70e1ff, #00d2ff);
  }
</style>