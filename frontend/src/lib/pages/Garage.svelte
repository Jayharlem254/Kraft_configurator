<script>
  import { api, getSessionKey } from "../api.js";
  import { money } from "../format.js";
  import { href, navigate } from "../router.svelte.js";
  import Icon from "../components/Icon.svelte";
  import VehicleThumb from "../components/VehicleThumb.svelte";

  let configs = $state([]);
  let vehicles = $state(new Map());
  let loading = $state(true);
  let error = $state(null);
  let codeInput = $state("");

  async function load() {
    loading = true;
    try {
      const [list, all] = await Promise.all([api.listConfigurations(getSessionKey()), api.listVehicles({ all: true })]);
      configs = list;
      vehicles = new Map(all.map((v) => [v.slug, v]));
      error = null;
    } catch (e) {
      error = e.message;
    }
    loading = false;
  }
  load();

  async function remove(c) {
    if (!confirm(`Delete saved configuration ${c.code}?`)) return;
    await api.deleteConfiguration(c.id);
    configs = configs.filter((x) => x.id !== c.id);
  }

  function openCode(e) {
    e.preventDefault();
    const c = codeInput.trim().toUpperCase();
    if (c) navigate(`/code/${c}`);
  }

  const queryOf = (c) => ({ variant: c.variant || c.snapshot?.variant, o: (c.snapshot?.selections ?? c.option_codes ?? []).join(".") || undefined });
  const fmtDate = (s) => (s ? new Date(s).toLocaleDateString("en-US", { year: "numeric", month: "short", day: "numeric" }) : "");
</script>

<div class="container page">
  <header class="head">
    <div>
      <p class="eyebrow">My Garage</p>
      <h1 class="display">Saved configurations</h1>
    </div>
    <form class="code-form" onsubmit={openCode}>
      <label class="label" for="code">Open a Kraft code</label>
      <div class="row" style="gap:8px">
        <input id="code" class="input" placeholder="e.g. KE4YNW5X" bind:value={codeInput} autocomplete="off" />
        <button class="btn btn-primary" type="submit">Open</button>
      </div>
    </form>
  </header>

  {#if loading}
    <p class="muted">Loading…</p>
  {:else if error}
    <p class="muted">{error}</p>
  {:else if !configs.length}
    <div class="empty">
      <h2 class="h3">No saved configurations yet</h2>
      <p class="muted">Build a vehicle and press “Save” to keep it here with its Kraft code.</p>
      <a class="btn btn-primary" href="#/">Start configuring</a>
    </div>
  {:else}
    <div class="grid">
      {#each configs as c (c.id)}
        {@const v = vehicles.get(c.vehicle_slug)}
        <article class="card">
          {#if v}<VehicleThumb vehicle={v} alt={c.vehicle_name} />{/if}
          <div class="body">
            <div class="row">
              <h3 class="h3">{c.variant_name || c.vehicle_name}</h3>
              <span class="spacer"></span>
              <span class="tag tag-dark num">{c.code}</span>
            </div>
            <p class="small muted">{c.vehicle_name} · {c.option_codes.length} options · saved {fmtDate(c.updated_at)}</p>
            <p class="price num">{money(c.total_price)}</p>
            <div class="actions">
              <a class="btn btn-primary btn-sm" href={href(`/configure/${c.vehicle_slug}`, queryOf(c))}>Open</a>
              <a class="btn btn-secondary btn-sm" href={href(`/summary/${c.vehicle_slug}`, { ...queryOf(c), code: c.code })}>Summary</a>
              <span class="spacer"></span>
              <button class="icon-btn" aria-label={`Delete ${c.code}`} onclick={() => remove(c)}><Icon name="trash" size={18} /></button>
            </div>
          </div>
        </article>
      {/each}
    </div>
  {/if}
</div>

<style>
  .page {
    padding-top: clamp(32px, 5vw, 64px);
    padding-bottom: 96px;
  }
  .head {
    display: flex;
    justify-content: space-between;
    align-items: flex-end;
    gap: 24px;
    flex-wrap: wrap;
    margin-bottom: 40px;
  }
  .display {
    margin-top: 8px;
  }
  .code-form {
    width: min(420px, 100%);
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(min(100%, 340px), 1fr));
    gap: 24px;
  }
  .card :global(.thumb) {
    border-radius: var(--radius);
  }
  .body {
    padding-top: 14px;
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .price {
    font-weight: 600;
    font-size: 18px;
  }
  .actions {
    display: flex;
    gap: 8px;
    align-items: center;
    margin-top: 6px;
  }
  .empty {
    display: flex;
    flex-direction: column;
    gap: 12px;
    align-items: flex-start;
    padding: 40px 0;
  }
</style>
