<script>
  import { onMount } from "svelte";
  import { api } from "./lib/api.js";
  import { route } from "./lib/router.svelte.js";
  import Header from "./lib/components/Header.svelte";
  import ModelRange from "./lib/pages/ModelRange.svelte";
  import Configurator from "./lib/pages/Configurator.svelte";
  import Summary from "./lib/pages/Summary.svelte";
  import Garage from "./lib/pages/Garage.svelte";
  import Compare from "./lib/pages/Compare.svelte";
  import Studio from "./lib/pages/Studio.svelte";
  import CodeLoader from "./lib/pages/CodeLoader.svelte";

  let engine = $state(null);

  async function ping() {
    try {
      engine = await api.health();
    } catch {
      engine = null;
    }
  }

  onMount(() => {
    ping();
    const id = setInterval(ping, 15000);
    return () => clearInterval(id);
  });

  const page = $derived(route.segments[0] ?? "");
  const fullBleed = $derived(page === "configure");
</script>

<div class="app" class:full={fullBleed}>
  <Header {engine} />
  <main>
    {#if engine === null && page !== "studio"}
      <div class="offline no-print" role="status">
        The configuration engine is not reachable. Start it with <code>run.bat</code> (or
        <code>kraft_backend.exe --port 8000</code>) — the page reconnects automatically.
      </div>
    {/if}

    {#if page === "configure" && route.segments[1]}
      {#key route.segments[1]}
        <Configurator slug={route.segments[1]} />
      {/key}
    {:else if page === "summary" && route.segments[1]}
      {#key route.segments[1]}
        <Summary slug={route.segments[1]} />
      {/key}
    {:else if page === "code" && route.segments[1]}
      <CodeLoader code={route.segments[1]} />
    {:else if page === "garage"}
      <Garage />
    {:else if page === "compare"}
      <Compare />
    {:else if page === "studio"}
      <Studio />
    {:else}
      <ModelRange type={page === "models" ? route.segments[1] ?? "" : ""} />
    {/if}
  </main>
</div>

<style>
  .app {
    min-height: 100%;
    display: flex;
    flex-direction: column;
  }
  .app.full {
    height: 100%;
    overflow: hidden;
  }
  @media (max-width: 820px) {
    .app.full {
      height: auto;
      overflow: visible;
    }
  }
  main {
    flex: 1;
    min-height: 0;
    display: flex;
    flex-direction: column;
  }
  .offline {
    padding: 10px clamp(16px, 4vw, 56px);
    background: #fff4e5;
    color: #6b3d00;
    font-size: 14px;
    border-bottom: 1px solid #f3d9b4;
  }
  code {
    font-size: 13px;
    background: rgba(0, 0, 0, 0.06);
    padding: 1px 6px;
    border-radius: 3px;
  }
</style>
