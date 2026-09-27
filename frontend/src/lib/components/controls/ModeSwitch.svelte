<script>
  import { createEventDispatcher } from "svelte";

  export let options = []; // [{ id, label, icon }]
  export let value = "";

  const dispatch = createEventDispatcher();

  function select(id) {
    if (value !== id) {
      value = id;
      dispatch("change", id);
    }
  }
</script>

<div class="mode-switch milled">
  {#each options as opt}
    {@const active = value === opt.id}
    <button
      type="button"
      class="mode-btn"
      class:active
      class:raised={active}
      on:click={() => select(opt.id)}
      title={opt.label}
    >
      {#if opt.icon}
        <span class="icon">{@html opt.icon}</span>
      {/if}
      <span class="label mono">{opt.label}</span>
      {#if active}
        <span class="indicator"></span>
      {/if}
    </button>
  {/each}
</div>

<style>
  .mode-switch {
    display: inline-flex;
    align-items: center;
    padding: 2px;
    border-radius: var(--radius);
    gap: 2px;
    background: var(--face-lo);
    border: 1px solid var(--line);
  }

  .mode-btn {
    position: relative;
    display: inline-flex;
    align-items: center;
    gap: 5px;
    padding: 3px 9px;
    border: none;
    background: transparent;
    color: var(--text-dim);
    font-size: 11px;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.12s ease;
  }

  .mode-btn:hover {
    color: var(--text-hi);
  }

  .mode-btn.active {
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .indicator {
    position: absolute;
    bottom: -1px;
    left: 20%;
    right: 20%;
    height: 2px;
    background: var(--crystal-blue);
    box-shadow: 0 0 5px var(--crystal-blue);
    border-radius: 1px;
  }

  .icon {
    display: inline-flex;
    align-items: center;
    font-size: 12px;
  }

  .label {
    letter-spacing: 0.05em;
  }
</style>
