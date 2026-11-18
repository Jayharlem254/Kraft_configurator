<script>
  import { api } from "../api.js";
  import { classLabel, CLASSES_BY_TYPE, money, TYPE_SINGULAR } from "../format.js";
  import { href } from "../router.svelte.js";
  import Icon from "../components/Icon.svelte";
  import { analyzeVehicle } from "../modelAnalysis.js";

  let models = $state([]);
  let vehicles = $state([]);
  let health = $state(null);

  let file = $state(null);
  let dragging = $state(false);
  let form = $state({ name: "", vehicle_type: "car", body_class: "supercar", base_price: 120000, tagline: "", target: "new" });
  let progress = $state(0);
  let uploading = $state(false);
  let result = $state(null);
  let error = $state(null);

  // Model analysis report: { name, slug, summary } | { name, error }
  let report = $state(null);
  let analyzing = $state(null); // vehicle id being analysed
  let analyzeProgress = $state(0);

  const ROLE_LABELS = {
    paint: "Body paint",
    hull: "Hull",
    rims: "Rims",
    tyre: "Tyres",
    calipers: "Brake calipers",
    disc: "Brake discs",
    glass: "Glass",
    interior: "Interior",
    seat: "Seat",
    frame: "Frame",
    deck: "Deck",
    exhaust: "Exhaust",
    lamp: "Lights",
    taillight: "Tail lights",
  };

  async function analyze(v) {
    if (!v?.model_file) return;
    analyzing = v.id;
    analyzeProgress = 0;
    try {
      // List rows omit material_map; analyse with the full definition.
      const full = v.material_map ? v : await api.getVehicle(v.slug);
      const { summary } = await analyzeVehicle(full, (p) => (analyzeProgress = p));
      report = { name: v.name, slug: v.slug, summary };
      await refresh();
    } catch (e) {
      report = { name: v.name, error: e.message || String(e) };
    }
    analyzing = null;
  }

  async function refresh() {
    try {
      [models, vehicles, health] = await Promise.all([api.listModels(), api.listVehicles({ all: true }), api.health()]);
    } catch (e) {
      error = e.message;
    }
  }
  refresh();

  $effect(() => {
    if (!CLASSES_BY_TYPE[form.vehicle_type].includes(form.body_class)) form.body_class = CLASSES_BY_TYPE[form.vehicle_type][0];
  });

  function pick(f) {
    if (!f) return;
    if (!/\.(glb|gltf|obj|fbx)$/i.test(f.name)) {
      error = "Please choose a .glb, .gltf, .obj or .fbx file.";
      return;
    }
    error = null;
    file = f;
    if (!form.name) form.name = f.name.replace(/\.[^.]+$/, "").replace(/[_-]+/g, " ").replace(/\b\w/g, (m) => m.toUpperCase());
  }

  async function upload(e) {
    e.preventDefault();
    if (!file) return;
    uploading = true;
    error = null;
    result = null;
    const fd = new FormData();
    fd.append("file", file);
    if (form.target !== "new") fd.append("vehicle_id", form.target);
    else {
      fd.append("name", form.name);
      fd.append("vehicle_type", form.vehicle_type);
      fd.append("body_class", form.body_class);
      fd.append("base_price", String(form.base_price));
      if (form.tagline) fd.append("tagline", form.tagline);
    }
    try {
      result = await api.uploadModel(fd, (p) => (progress = p));
      api.invalidateCatalog();
      file = null;
      progress = 0;
      await refresh();
      // Analyse straight away so the configurator opens arranged, with highlights
      // and the model's original finish as an option.
      if (result?.vehicle?.model_file) analyze(result.vehicle);
    } catch (err) {
      error = err.message;
    }
    uploading = false;
  }

  async function togglePublish(v) {
    await api.updateVehicle(v.id, { is_published: !v.is_published });
    api.invalidateCatalog();
    refresh();
  }

  async function removeVehicle(v) {
    if (!confirm(`Delete “${v.name}” from the catalog?${v.model_file ? " Its 3D model file is kept." : ""}`)) return;
    await api.deleteVehicle(v.id, false);
    api.invalidateCatalog();
    refresh();
  }

  async function removeModel(m) {
    if (!confirm(`Delete ${m.filename} from disk?${m.vehicle ? ` “${m.vehicle.name}” will fall back to its procedural model.` : ""}`)) return;
    await api.deleteModel(m.filename);
    api.invalidateCatalog();
    refresh();
  }

  const mb = (b) => `${(b / 1024 / 1024).toFixed(1)} MB`;
  const uploads = $derived(vehicles.filter((v) => v.source !== "seed"));
</script>

<div class="container page">
  <p class="eyebrow">Studio</p>
  <h1 class="display">Bring your own 3D models.</h1>
  <p class="muted lead">
    Upload a .glb (recommended, Draco/Meshopt compression supported), .gltf, .obj or .fbx. The engine registers it as a vehicle with
    the full option set for its type, then analyses the model: it removes ground planes, turns it the right way round, finds the
    wheels, glass, lights, cabin and paint from the geometry, and picks out its best features as camera highlights.
  </p>

  {#if health}
    <dl class="stats">
      <div><dt class="xsmall muted">Engine</dt><dd>{health.version}</dd></div>
      <div><dt class="xsmall muted">Vehicles</dt><dd class="num">{health.vehicles_count}</dd></div>
      <div><dt class="xsmall muted">Rule evaluations</dt><dd class="num">{health.rules?.evaluations ?? 0}</dd></div>
      <div><dt class="xsmall muted">Avg. evaluation</dt><dd class="num">{Math.round(health.rules?.avg_us ?? 0)} µs</dd></div>
      <div><dt class="xsmall muted">Catalog snapshot</dt><dd class="num">v{health.catalog_version} · {health.catalog_build_ms?.toFixed(1)} ms</dd></div>
    </dl>
  {/if}

  <div class="cols">
    <form class="upload" onsubmit={upload}>
      <h2 class="h3">Upload a model</h2>
      <label
        class="drop"
        class:dragging
        ondragover={(e) => {
          e.preventDefault();
          dragging = true;
        }}
        ondragleave={() => (dragging = false)}
        ondrop={(e) => {
          e.preventDefault();
          dragging = false;
          pick(e.dataTransfer.files[0]);
        }}
      >
        <input type="file" accept=".glb,.gltf,.obj,.fbx" onchange={(e) => pick(e.currentTarget.files[0])} />
        <Icon name="upload" size={28} />
        {#if file}
          <strong>{file.name}</strong>
          <span class="small muted">{mb(file.size)}</span>
        {:else}
          <strong>Drop a 3D file here</strong>
          <span class="small muted">or click to browse · up to 512 MB</span>
        {/if}
      </label>

      <div>
        <label class="label" for="target">Use for</label>
        <select id="target" class="select" bind:value={form.target}>
          <option value="new">A new vehicle</option>
          {#each vehicles as v (v.id)}<option value={String(v.id)}>Replace model of {v.name}</option>{/each}
        </select>
      </div>

      {#if form.target === "new"}
        <div>
          <label class="label" for="name">Name</label>
          <input id="name" class="input" bind:value={form.name} required />
        </div>
        <div class="two">
          <div>
            <label class="label" for="type">Type</label>
            <select id="type" class="select" bind:value={form.vehicle_type}>
              {#each Object.keys(CLASSES_BY_TYPE) as t}<option value={t}>{TYPE_SINGULAR[t]}</option>{/each}
            </select>
          </div>
          <div>
            <label class="label" for="class">Body type</label>
            <select id="class" class="select" bind:value={form.body_class}>
              {#each CLASSES_BY_TYPE[form.vehicle_type] as c}<option value={c}>{classLabel(c)}</option>{/each}
            </select>
          </div>
        </div>
        <div class="two">
          <div>
            <label class="label" for="price">Base price (USD)</label>
            <input id="price" class="input" type="number" min="0" step="100" bind:value={form.base_price} />
          </div>
          <div>
            <label class="label" for="tag">Tagline</label>
            <input id="tag" class="input" bind:value={form.tagline} placeholder="Optional" />
          </div>
        </div>
      {/if}

      {#if uploading}
        <div class="progress"><span style={`transform:scaleX(${progress})`}></span></div>
        <p class="small muted">Uploading… {Math.round(progress * 100)}%</p>
      {/if}
      {#if error}<p class="error small">{error}</p>{/if}
      {#if result?.vehicle}
        <div class="success">
          <p><strong>{result.vehicle.name}</strong> is ready with {result.vehicle.option_categories?.length ?? 0} option categories.</p>
          <a class="btn btn-primary btn-sm" href={href(`/configure/${result.vehicle.slug}`)}>Open in configurator</a>
        </div>
      {/if}
      <button class="btn btn-primary" type="submit" disabled={!file || uploading}>{uploading ? "Uploading…" : "Upload & register"}</button>

      {#if analyzing}
        <div class="progress"><span style={`transform:scaleX(${Math.max(analyzeProgress, 0.05)})`}></span></div>
        <p class="small muted">Analysing the 3D model… finding wheels, glass, lights, cabin and paint.</p>
      {/if}
      {#if report}
        <section class="report" aria-label="Model analysis">
          {#if report.error}
            <p><strong>{report.name}</strong>: the model couldn't be analysed.</p>
            <p class="small error">{report.error}</p>
          {:else}
            {@const a = report.summary}
            <p class="eyebrow">Model analysis</p>
            <h3 class="h3">{report.name}</h3>
            <dl class="facts">
              <div><dt class="xsmall muted">Size</dt><dd class="num">{a.dims.length} × {a.dims.width} × {a.dims.height} m</dd></div>
              <div><dt class="xsmall muted">Wheels</dt><dd class="num">{a.wheels.length || "—"}{a.wheel_inches ? ` · ≈${a.wheel_inches}″` : ""}</dd></div>
              <div><dt class="xsmall muted">Geometry</dt><dd class="num">{a.stats.meshes} meshes · {Math.round(a.stats.triangles / 1000)}k tris</dd></div>
              <div><dt class="xsmall muted">Hero angle</dt><dd>{a.hero === "rear34" ? "Rear ¾" : "Front ¾"}</dd></div>
            </dl>
            {#if a.notes.length}
              <p class="small"><strong>Arranged</strong></p>
              <ul class="notes small">{#each a.notes as n}<li>{n}</li>{/each}</ul>
            {/if}
            <p class="small"><strong>Parts the configurator can restyle</strong></p>
            <ul class="parts">
              {#each Object.entries(a.roles) as [role, names]}
                <li class="small"><span>{ROLE_LABELS[role] ?? role}</span><span class="xsmall muted">{names.slice(0, 3).join(", ")}{names.length > 3 ? ` +${names.length - 3}` : ""}</span></li>
              {/each}
            </ul>
            {#if a.original_paint}
              <p class="small orig">
                <span class="dot" style={`background:${a.original_paint.color}`}></span>
                Original finish added as a paint option ({a.original_paint.finish}{a.original_paint.textured ? ", textured" : ""}).
              </p>
            {/if}
            <p class="small"><strong>Highlights</strong></p>
            <ul class="notes small">{#each a.highlights as h}<li>{h.label} <span class="muted">· {h.detail}</span></li>{/each}</ul>
            {#if report.slug}<a class="btn btn-primary btn-sm" href={href(`/configure/${report.slug}`)}>Open in configurator</a>{/if}
          {/if}
        </section>
      {/if}
    </form>

    <div class="lists">
      <section>
        <h2 class="h3">Your vehicles</h2>
        {#if !uploads.length}<p class="small muted">No uploaded vehicles yet.</p>{/if}
        <ul class="items">
          {#each uploads as v (v.id)}
            <li>
              <div class="grow">
                <p><strong>{v.name}</strong> {#if !v.is_published}<span class="tag">Hidden</span>{/if}</p>
                <p class="xsmall muted">{TYPE_SINGULAR[v.vehicle_type]} · {classLabel(v.body_class)} · from {money(v.from_price)} · {v.option_count} options</p>
              </div>
              <a class="btn btn-ghost btn-sm" href={href(`/configure/${v.slug}`)}>Open</a>
              {#if v.model_file}
                <button class="btn btn-ghost btn-sm" disabled={analyzing !== null} onclick={() => analyze(v)}>{analyzing === v.id ? "Analysing…" : "Analyse"}</button>
              {/if}
              <button class="btn btn-ghost btn-sm" onclick={() => togglePublish(v)}>{v.is_published ? "Hide" : "Publish"}</button>
              <button class="icon-btn" aria-label={`Delete ${v.name}`} onclick={() => removeVehicle(v)}><Icon name="trash" size={18} /></button>
            </li>
          {/each}
        </ul>
      </section>

      <section>
        <h2 class="h3">Model files</h2>
        {#if !models.length}<p class="small muted">No model files on the server.</p>{/if}
        <ul class="items">
          {#each models as m (m.filename)}
            <li>
              <Icon name="cube" size={20} />
              <div class="grow">
                <p class="small"><strong>{m.filename}</strong></p>
                <p class="xsmall muted">{mb(m.size_bytes)} · {m.vehicle ? `used by ${m.vehicle.name}` : "unassigned"}</p>
              </div>
              <button class="icon-btn" aria-label={`Delete ${m.filename}`} onclick={() => removeModel(m)}><Icon name="trash" size={18} /></button>
            </li>
          {/each}
        </ul>
      </section>
    </div>
  </div>
</div>

<style>
  .page {
    padding-top: clamp(32px, 5vw, 64px);
    padding-bottom: 96px;
  }
  .display {
    margin: 8px 0 12px;
  }
  .lead {
    max-width: 760px;
    font-size: 17px;
  }
  .stats {
    display: flex;
    flex-wrap: wrap;
    gap: 32px;
    margin: 32px 0 0;
    padding: 20px 0;
    border-top: 1px solid var(--line);
    border-bottom: 1px solid var(--line);
  }
  .stats div {
    display: flex;
    flex-direction: column;
  }
  .stats dt {
    order: 2;
  }
  .stats dd {
    margin: 0;
    font-weight: 600;
    font-size: 18px;
  }
  .cols {
    display: grid;
    grid-template-columns: minmax(320px, 520px) 1fr;
    gap: 48px;
    margin-top: 40px;
  }
  .upload {
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .drop {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 6px;
    padding: 36px 16px;
    border: 1.5px dashed var(--line-strong);
    border-radius: var(--radius);
    background: var(--surface);
    cursor: pointer;
    text-align: center;
    transition: all 0.15s var(--ease);
  }
  .drop:hover,
  .drop.dragging {
    border-color: var(--ink);
    background: var(--surface-2);
  }
  .drop input {
    display: none;
  }
  .two {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }
  .progress {
    height: 3px;
    background: var(--surface-2);
    overflow: hidden;
  }
  .progress span {
    display: block;
    height: 100%;
    background: var(--ink);
    transform-origin: left;
  }
  .error {
    color: var(--danger);
  }
  .success {
    background: #e3f1e1;
    padding: 14px;
    border-radius: var(--radius);
    display: flex;
    flex-direction: column;
    gap: 10px;
    align-items: flex-start;
  }
  .report {
    border: 1px solid var(--line);
    border-radius: var(--radius);
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .report .h3 {
    margin: 0;
  }
  .facts {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px 16px;
    margin: 4px 0;
  }
  .facts div {
    display: flex;
    flex-direction: column;
  }
  .facts dd {
    margin: 0;
    font-weight: 600;
  }
  .notes {
    margin: 0;
    padding-left: 18px;
  }
  .parts {
    list-style: none;
    margin: 0;
    padding: 0;
  }
  .parts li {
    display: flex;
    justify-content: space-between;
    gap: 12px;
    padding: 4px 0;
    border-bottom: 1px solid var(--line);
  }
  .parts li span:last-child {
    text-align: right;
    word-break: break-word;
  }
  .orig {
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .dot {
    width: 16px;
    height: 16px;
    border-radius: 50%;
    border: 1px solid var(--line-strong);
    flex-shrink: 0;
  }
  .lists {
    display: flex;
    flex-direction: column;
    gap: 36px;
  }
  .items {
    list-style: none;
    padding: 0;
    margin: 12px 0 0;
  }
  .items li {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 0;
    border-bottom: 1px solid var(--line);
  }
  .grow {
    flex: 1;
    min-width: 0;
    word-break: break-word;
  }
  @media (max-width: 900px) {
    .cols {
      grid-template-columns: 1fr;
    }
  }
</style>
