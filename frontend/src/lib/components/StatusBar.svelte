<script>
  export let vehicle = null;
  export let totalPrice = 0;
  export let engineOnline = false;
  export let selectedCount = 0;

  function formatPrice(val) {
    return new Intl.NumberFormat("en-US", {
      style: "currency",
      currency: "USD",
      maximumFractionDigits: 0,
    }).format(val);
  }
</script>

<footer class="statusbar">
  <!-- Status Text / Info Readout -->
  <div class="row gap-md">
    <span class="dot" class:online={engineOnline}></span>
    <span class="mono info-text">
      {engineOnline ? "C++ BACKEND: OK" : "C++ BACKEND: OFFLINE (FALLBACK)"}
    </span>
    <span class="dim mono sep-text">|</span>
    <span class="dim mono protocol-text">REST / MEDIA / SQLITE3</span>
  </div>

  <div class="grow"></div>

  {#if vehicle}
    <div class="specs-readout row">
      {#if vehicle.specs?.power}
        <span class="spec-item mono">
          <span class="dim">PWR:</span> <span class="val">{vehicle.specs.power}</span>
        </span>
      {/if}
      {#if vehicle.specs?.acceleration}
        <span class="spec-item mono">
          <span class="dim">0-60:</span> <span class="val">{vehicle.specs.acceleration}</span>
        </span>
      {/if}
      {#if vehicle.specs?.top_speed}
        <span class="spec-item mono">
          <span class="dim">TOP:</span> <span class="val">{vehicle.specs.top_speed}</span>
        </span>
      {/if}
      {#if vehicle.specs?.weight}
        <span class="spec-item mono">
          <span class="dim">WT:</span> <span class="val">{vehicle.specs.weight}</span>
        </span>
      {/if}
      <span class="dim mono sep-text">|</span>
      <span class="spec-item mono">
        <span class="dim">CUSTOM OPTS:</span> <span class="val opt">{selectedCount}</span>
      </span>
      <span class="dim mono sep-text">|</span>
      <span class="spec-item mono price-badge">
        <span class="dim">TOTAL:</span> <span class="val price">{formatPrice(totalPrice)}</span>
      </span>
    </div>
  {:else}
    <span class="dim mono">SELECT A VEHICLE FROM CATALOG TO CONFIGURE</span>
  {/if}

  <!-- Quni4m-style mini telemetry visualizer -->
  <div class="mini-meter" title="Telemetry activity">
    <div class="meter-bar" style="width: {engineOnline ? '85%' : '15%'}"></div>
  </div>
</footer>

<style>
  .statusbar {
    display: flex;
    align-items: center;
    gap: 12px;
    height: 24px;
    padding: 0 10px;
    background: var(--panel);
    border-top: 1px solid var(--line);
    font-size: 10px;
    flex-shrink: 0;
    z-index: 20;
  }

  .gap-md {
    gap: 8px;
  }

  .dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--led-red);
    box-shadow: 0 0 3px var(--led-red);
  }

  .dot.online {
    background: var(--crystal-blue);
    box-shadow: 0 0 5px var(--crystal-blue);
  }

  .info-text {
    color: var(--text-hi);
    font-size: 10px;
  }

  .protocol-text {
    font-size: 9px;
  }

  .sep-text {
    opacity: 0.4;
  }

  .specs-readout {
    gap: 10px;
  }

  .spec-item {
    font-size: 10px;
  }

  .val {
    color: var(--text-hi);
  }

  .val.opt {
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .price-badge {
    background: var(--surface);
    padding: 1px 6px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .val.price {
    color: var(--crystal-bright);
    font-weight: 700;
  }

  .mini-meter {
    width: 50px;
    height: 6px;
    background: var(--field);
    border: 1px solid var(--line-soft);
    border-radius: 2px;
    overflow: hidden;
  }

  .meter-bar {
    height: 100%;
    background: linear-gradient(90deg, var(--crystal-deep), var(--crystal-blue));
    box-shadow: 0 0 4px var(--crystal-glow);
    transition: width 0.3s ease;
  }
</style>
