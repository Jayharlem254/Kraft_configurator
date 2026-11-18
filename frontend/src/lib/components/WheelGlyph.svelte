<script>
  // Small SVG rendering of a wheel design from its option's visual spec.
  let { spec = {}, color = "#c9ccd1", size = 72 } = $props();

  const n = $derived(Math.min(spec.spokes ?? 5, 40));
  const style = $derived(spec.style ?? "classic");
  const spokes = $derived.by(() => {
    const out = [];
    const add = (a, w, r0 = 9, r1 = 30) => out.push({ a, w, r0, r1 });
    for (let i = 0; i < n; i++) {
      const a = (i / n) * 360;
      if (style === "twin") {
        add(a - 4, 2.4);
        add(a + 4, 2.4);
      } else if (style === "split" || style === "y") {
        const s = style === "y" ? 12 : 7;
        add(a, 4, 9, 18);
        add(a - s, 2.4, 17, 30);
        add(a + s, 2.4, 17, 30);
      } else if (style === "wire") {
        add(a, 0.6);
      } else if (style === "thin") {
        add(a, 1.8);
      } else if (style === "turbine") {
        add(a, 3.2);
      } else {
        add(a, style === "carbon" ? 6 : 3.6);
      }
    }
    return out;
  });
</script>

<svg width={size} height={size} viewBox="-40 -40 80 80" aria-hidden="true">
  <circle r="38" fill="#1b1c1f" />
  <circle r="31.5" fill="none" stroke={color} stroke-width="3" />
  <circle r="30" fill="#2a2b2f" />
  {#each spokes as s}
    <rect
      x={s.r0}
      y={-s.w / 2}
      width={s.r1 - s.r0}
      height={s.w}
      rx={Math.min(1, s.w / 2)}
      fill={color}
      transform={`rotate(${s.a}) ${style === "turbine" ? "skewY(18)" : ""}`}
    />
  {/each}
  <circle r="9" fill={color} />
  <circle r="3.5" fill="#1b1c1f" opacity="0.6" />
  {#if spec.centerlock}
    <polygon points="0,-5 4.3,-2.5 4.3,2.5 0,5 -4.3,2.5 -4.3,-2.5" fill="#c0161f" />
  {/if}
</svg>
