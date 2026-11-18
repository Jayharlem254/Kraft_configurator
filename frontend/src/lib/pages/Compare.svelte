<script>
  import { api } from "../api.js";
  import { classLabel, formatSpec, money, specLabel, SPECS } from "../format.js";
  import { href, route } from "../router.svelte.js";
  import VehicleThumb from "../components/VehicleThumb.svelte";

  let columns = $state([]);
  let error = $state(null);

  $effect(() => {
    const keys = (route.query.v ?? "").split(",").filter(Boolean).slice(0, 4);
    Promise.all(
      keys.map(async (key) => {
        const [slug, code] = key.split(":");
        const vehicle = await api.getVehicle(slug);
        const variant = vehicle.variants.find((x) => x.code === code) ?? vehicle.variants[0];
        return { vehicle, variant };
      }),
    )
      .then((cols) => (columns = cols))
      .catch((e) => (error = e.message));
  });

  const specKeys = $derived(Object.keys(SPECS).filter((k) => columns.some((c) => c.variant.specs?.[k] !== undefined && c.variant.specs?.[k] !== 0)));

  function best(key) {
    const meta = SPECS[key];
    if (!meta?.better) return null;
    const vals = columns.map((c) => c.variant.specs?.[key]).filter((x) => typeof x === "number" && x !== 0);
    if (vals.length < 2) return null;
    return meta.better === "up" ? Math.max(...vals) : Math.min(...vals);
  }
  const cheapest = $derived(Math.min(...columns.map((c) => c.variant.price)));
</script>

<div class="container page">
  <p class="eyebrow">Compare</p>
  <h1 class="display">Side by side.</h1>

  {#if error}
    <p class="muted">{error}</p>
  {:else if columns.length < 2}
    <p class="muted">Pick at least two variants from the model range to compare them.</p>
    <a class="btn btn-primary" href="#/">Choose models</a>
  {:else}
    <div class="scroll">
      <table style={`--cols:${columns.length}`}>
        <thead>
          <tr>
            <th></th>
            {#each columns as c}
              <th>
                <VehicleThumb vehicle={c.vehicle} alt={c.variant.name} />
                <p class="xsmall muted">{classLabel(c.vehicle.body_class)}</p>
                <p class="h3">{c.variant.name}</p>
                <a class="btn btn-primary btn-sm" href={href(`/configure/${c.vehicle.slug}`, { variant: c.variant.code })}>Configure</a>
              </th>
            {/each}
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>Base price</td>
            {#each columns as c}<td class="num" class:best={c.variant.price === cheapest}>{money(c.variant.price)}</td>{/each}
          </tr>
          {#each specKeys as key}
            {@const b = best(key)}
            <tr>
              <td>{specLabel(key)}</td>
              {#each columns as c}
                <td class="num" class:best={b !== null && c.variant.specs?.[key] === b}>{formatSpec(key, c.variant.specs?.[key])}</td>
              {/each}
            </tr>
          {/each}
          <tr>
            <td>Options available</td>
            {#each columns as c}<td class="num">{c.vehicle.option_categories.reduce((n, cat) => n + cat.choices.filter((x) => !x.variants?.length || x.variants.includes(c.variant.code)).length, 0)}</td>{/each}
          </tr>
        </tbody>
      </table>
    </div>
  {/if}
</div>

<style>
  .page {
    padding-top: clamp(32px, 5vw, 64px);
    padding-bottom: 96px;
  }
  .display {
    margin: 8px 0 32px;
  }
  .scroll {
    overflow-x: auto;
  }
  table {
    width: 100%;
    min-width: calc(200px + var(--cols) * 240px);
    border-collapse: collapse;
  }
  th {
    text-align: left;
    vertical-align: top;
    padding: 0 12px 20px;
    font-weight: 400;
  }
  th :global(.thumb) {
    border-radius: var(--radius);
    margin-bottom: 12px;
  }
  th .h3 {
    margin: 2px 0 12px;
  }
  td {
    padding: 14px 12px;
    border-top: 1px solid var(--line);
  }
  td:first-child {
    color: var(--ink-2);
    width: 200px;
  }
  td.best {
    font-weight: 600;
    color: var(--success);
  }
</style>
