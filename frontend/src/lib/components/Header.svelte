<script>
  import { route } from "../router.svelte.js";
  import Icon from "./Icon.svelte";

  let { engine = null } = $props();

  const NAV = [
    { href: "#/", label: "Models", match: (s) => !s[0] || s[0] === "models" || s[0] === "compare" },
    { href: "#/garage", label: "My Garage", match: (s) => s[0] === "garage" },
    { href: "#/studio", label: "Studio", match: (s) => s[0] === "studio" },
  ];

  let menuOpen = $state(false);
  $effect(() => {
    route.path;
    menuOpen = false;
  });
</script>

<header class="header no-print">
  <div class="inner">
    <a class="brand" href="#/" aria-label="Kraft — all models">
      <span class="mark">KRAFT</span>
      <span class="sub">Configurator</span>
    </a>

    <nav class="nav" class:open={menuOpen} aria-label="Primary">
      {#each NAV as item}
        <a href={item.href} class:active={item.match(route.segments)}>{item.label}</a>
      {/each}
    </nav>

    <div class="right">
      <span
        class="engine"
        class:online={engine?.status === "online"}
        title={engine
          ? `C++ engine ${engine.version} · ${engine.vehicles_count} vehicles · rules avg ${Math.round(engine.rules?.avg_us ?? 0)} µs`
          : "Engine offline"}
      >
        <span class="dot"></span>
        <span class="xsmall">{engine?.status === "online" ? "Engine online" : "Engine offline"}</span>
      </span>
      <button class="icon-btn menu" aria-label="Menu" aria-expanded={menuOpen} onclick={() => (menuOpen = !menuOpen)}>
        <Icon name={menuOpen ? "close" : "list"} />
      </button>
    </div>
  </div>
</header>

<style>
  .header {
    position: sticky;
    top: 0;
    z-index: 50;
    height: var(--header-h);
    background: rgba(255, 255, 255, 0.92);
    backdrop-filter: saturate(180%) blur(12px);
    border-bottom: 1px solid var(--line);
  }
  .inner {
    height: 100%;
    display: grid;
    grid-template-columns: 1fr auto 1fr;
    align-items: center;
    padding: 0 clamp(16px, 3vw, 40px);
  }
  .brand {
    display: inline-flex;
    align-items: baseline;
    gap: 10px;
    justify-self: start;
  }
  .mark {
    font-size: 20px;
    font-weight: 700;
    letter-spacing: 0.32em;
  }
  .sub {
    font-size: 13px;
    color: var(--ink-3);
    letter-spacing: 0.02em;
  }
  .nav {
    display: flex;
    gap: 32px;
  }
  .nav a {
    position: relative;
    font-size: 15px;
    color: var(--ink-2);
    padding: 20px 0;
    transition: color 0.15s var(--ease);
  }
  .nav a:hover,
  .nav a.active {
    color: var(--ink);
  }
  .nav a.active::after {
    content: "";
    position: absolute;
    left: 0;
    right: 0;
    bottom: 12px;
    height: 2px;
    background: var(--ink);
  }
  .right {
    justify-self: end;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .engine {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: var(--ink-3);
  }
  .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--danger);
  }
  .engine.online .dot {
    background: var(--success);
    box-shadow: 0 0 0 3px rgba(25, 126, 16, 0.15);
  }
  .menu {
    display: none;
  }

  @media (max-width: 760px) {
    .inner {
      grid-template-columns: 1fr auto;
    }
    .engine .xsmall,
    .sub {
      display: none;
    }
    .menu {
      display: inline-grid;
    }
    .nav {
      display: none;
      position: absolute;
      top: var(--header-h);
      left: 0;
      right: 0;
      flex-direction: column;
      gap: 0;
      background: var(--bg);
      border-bottom: 1px solid var(--line);
      padding: 8px 16px 16px;
    }
    .nav.open {
      display: flex;
    }
    .nav a {
      padding: 12px 0;
    }
    .nav a.active::after {
      display: none;
    }
  }
</style>
