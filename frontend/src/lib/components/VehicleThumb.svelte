<script>
  import { onMount, untrack } from "svelte";
  import { cachedThumbnail, renderThumbnail } from "../three/thumbnails.js";

  let { vehicle, alt = "" } = $props();
  let el;
  let src = $state(untrack(() => cachedThumbnail(vehicle)));

  // Render only when the card scrolls into view.
  onMount(() => {
    if (src) return;
    const target = vehicle;
    const io = new IntersectionObserver(
      (entries) => {
        if (!entries.some((e) => e.isIntersecting)) return;
        io.disconnect();
        renderThumbnail(target).then((url) => (src = url)).catch(() => {});
      },
      { rootMargin: "200px" },
    );
    io.observe(el);
    return () => io.disconnect();
  });
</script>

<div class="thumb" bind:this={el}>
  {#if src}
    <img {src} {alt} loading="lazy" decoding="async" />
  {:else}
    <div class="ph skeleton"></div>
  {/if}
</div>

<style>
  .thumb {
    position: relative;
    aspect-ratio: 16 / 10;
    background: linear-gradient(180deg, #f7f7f8 0%, #f7f7f8 35%, #d4d5d8 100%);
    overflow: hidden;
  }
  img {
    width: 100%;
    height: 100%;
    object-fit: cover;
    display: block;
    animation: fade-in 0.35s var(--ease);
  }
  .ph {
    position: absolute;
    inset: 0;
    opacity: 0.5;
  }
</style>
