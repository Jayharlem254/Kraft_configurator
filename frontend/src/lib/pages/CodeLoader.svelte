<script>
  import { api } from "../api.js";
  import { navigate } from "../router.svelte.js";

  let { code } = $props();
  let error = $state(null);

  // Resolve a Kraft code into the configurator URL it represents. Saved builds
  // store the requested option codes, so re-evaluation reproduces them exactly
  // even if the catalog was re-seeded since.
  $effect(() => {
    const c = code;
    api
      .getConfiguration(c)
      .then((cfg) => {
        const selections = cfg.snapshot?.selections ?? cfg.option_codes ?? [];
        navigate(`/configure/${cfg.vehicle_slug}`, { variant: cfg.variant || cfg.snapshot?.variant, o: selections.join(".") || undefined });
      })
      .catch((e) => (error = e.status === 404 ? `No configuration found for code “${c}”.` : e.message));
  });
</script>

<div class="container wrap">
  {#if error}
    <h1 class="h2">Code not found</h1>
    <p class="muted">{error}</p>
    <div class="row" style="gap:8px">
      <a class="btn btn-primary" href="#/garage">Enter another code</a>
      <a class="btn btn-secondary" href="#/">All models</a>
    </div>
  {:else}
    <p class="muted">Opening configuration {code}…</p>
  {/if}
</div>

<style>
  .wrap {
    padding: 80px 0;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
</style>
