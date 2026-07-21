<script>
  export let mode = "gallery"; // "gallery" | "configurator"
  export let vehicleType = "all";
  export let onSelectType = () => {};
  export let activeVehicleName = "";
  export let onBack = () => {};

  const types = [
    { id: "all", label: "All" },
    { id: "car", label: "Cars" },
    { id: "boat", label: "Boats" },
    { id: "bike", label: "Bikes" },
  ];
</script>

<header>
  <div class="wordmark">
    KRAFT<span class="accent">.</span>
    <span class="mono-label subtitle">CONFIGURATOR</span>
  </div>

  {#if mode !== "gallery"}
    <button class="back mono-label" on:click={onBack}>← BACK</button>
    {#if activeVehicleName}
      <span class="mono-label current">{activeVehicleName}</span>
    {/if}
  {:else}
    <nav>
      {#each types as t}
        <button
          class="tab mono-label"
          class:active={vehicleType === t.id}
          on:click={() => onSelectType(t.id)}
        >
          {t.label}
        </button>
      {/each}
    </nav>
  {/if}

  <div class="mono-label status">
    <span class="dot"></span>
    SYS // ONLINE
  </div>
</header>

<style>
  header {
    display: flex;
    align-items: center;
    gap: 32px;
    padding: 18px 28px;
    border-bottom: 1px solid var(--line);
    background: var(--void);
  }

  .wordmark {
    font-family: var(--font-display);
    font-size: 20px;
    font-weight: 700;
    letter-spacing: 0.02em;
    display: flex;
    align-items: baseline;
    gap: 10px;
  }
  .accent {
    color: var(--purple-500);
  }
  .subtitle {
    font-size: 10px;
    color: var(--text-tertiary);
  }

  nav {
    display: flex;
    gap: 6px;
    margin-left: auto;
  }

  .back {
    background: transparent;
    border: 1px solid var(--line);
    color: var(--text-secondary);
    padding: 8px 16px;
    border-radius: var(--radius-sm);
    margin-left: auto;
    transition: border-color 0.15s, color 0.15s;
  }
  .back:hover {
    border-color: var(--line-strong);
    color: var(--text-primary);
  }

  .current {
    color: var(--purple-300);
  }

  .tab {
    background: transparent;
    border: 1px solid var(--line);
    color: var(--text-secondary);
    padding: 8px 16px;
    border-radius: var(--radius-sm);
    transition: border-color 0.15s, color 0.15s;
  }
  .tab:hover {
    border-color: var(--line-strong);
    color: var(--text-primary);
  }
  .tab.active {
    border-color: var(--purple-500);
    color: var(--purple-300);
  }

  .status {
    display: flex;
    align-items: center;
    gap: 8px;
    color: var(--text-secondary);
  }
  .dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--success);
    box-shadow: 0 0 6px var(--success);
  }
</style>