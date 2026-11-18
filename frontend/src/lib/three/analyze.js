// Kraft model analyser — understands an uploaded GLB before it reaches the stage.
//
//   1. Clean-up   removes ground planes, backdrops and sky domes baked into the file.
//   2. Arrange    fixes Z-up exports, upside-down models and models facing backwards,
//                 lays the vehicle along x with its nose at +x, scales it to a
//                 plausible length and sits it on its tyres.
//   3. Analyse    finds the wheels from geometry (round, near the ground, at the
//                 corners), glass from transparency, lights from emission, the cabin
//                 from the glasshouse, and from those the paint, rims, tyres,
//                 calipers and interior — even when material names say nothing.
//   4. Highlight  ranks the model's best features (wheels, lights, interior, wing,
//                 exhaust, engine, glass roof, profile) and gives each a camera
//                 shot, plus a hero angle for the first impression.
//
// Everything works in the arranged model space: metres, y up, nose towards +x,
// centred on x/z with the tyres on y = 0 — the same space the procedural
// builders and the Stage camera presets use.
import { Color, Group, Vector3 } from "three";

export const ANALYSIS_VERSION = 1;

const DEFAULT_LENGTH = { car: 4.6, bike: 2.1, boat: 9 };

const RX = {
  junk: /(^|[^a-z])(ground|floor|backdrop|shadow_?(plane|catcher)?|studio|turntable|podium|platform|env_?sphere|sky_?dome|skybox|sky|cyclorama|plane\d*)([^a-z]|$)/,
  wheel: /(wheel|rim|tyre|tire|jante|felge|reifen|pneu|caliper|brake|disc|rotor|hub)/,
  front: /(front|head_?(light|lamp)|headl|hood|bonnet|grill|grille|splitter|windshield|windscreen|fork|handle_?bar|bow)(?!.*rear)/,
  rear: /(rear|back|tail|trunk|boot|exhaust|muffler|tail_?pipe|diffuser|spoiler|licen[cs]e|stern|transom)/,
  light: /(light|lamp|led|drl|indicator|blinker|signal)/,
  headlight: /(head_?(light|lamp)|headl|drl|front_?(light|lamp))/,
  taillight: /(tail_?(light|lamp)|taill|brake_?(light|lamp)|rear_?(light|lamp)|stop_?(light|lamp))/,
  wing: /(spoiler|rear_?wing|wing(?!.*(mirror|door))|aileron|ducktail)/,
  exhaust: /(exhaust|muffler|silencer|tail_?pipe|echappement|auspuff)/,
  engine: /(engine|motor(?!cycle)|outboard|v8|v10|v12|cylinder|intake)/,
  interior: /(seat|interior|dash|steering|console|cockpit|cabin|leather|upholster)/,
};

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

/**
 * Arrange and analyse a (cloned) GLTF scene.
 * @returns {{ group: Group, analysis: object, materialRoles: Map<object,string> }}
 *   group          wrapper to add to the stage (scene inside, already arranged)
 *   analysis       serialisable report + live Vector3 anchors/highlights
 *   materialRoles  geometric role guess per ORIGINAL material object
 */
export function analyzeModel(scene, { kind = "car", length } = {}) {
  const notes = [];
  const orient = new Group();
  orient.name = "kraft:orient";
  orient.add(scene);
  const wrapper = new Group();
  wrapper.name = "kraft:arranged";
  wrapper.add(orient);

  // 1. Clean-up -----------------------------------------------------------
  const removed = removeJunk(wrapper, notes);

  // 2. Arrange --------------------------------------------------------------
  let meshes = collect(wrapper);
  let box = unionBox(meshes);
  let size = box.size;

  // Up axis. Wheel axles are horizontal on an upright vehicle, so round parts
  // with a vertical axle mean it lies on its side, and wheels stacked
  // vertically mean it stands on its nose or tail (typical Z-up exports).
  if (kind !== "boat") {
    const probe = probeWheels(meshes);
    if (probe?.axle === "y") {
      const sx = spread(probe.centers.map((c) => c.x));
      const sz = spread(probe.centers.map((c) => c.z));
      rotateWorld(orient, sx >= sz ? AXIS.x : AXIS.z, Math.PI / 2);
      notes.push("Model was lying on its side — stood it on its wheels.");
      meshes = collect(wrapper);
    } else if (probe) {
      const other = probe.axle === "x" ? "z" : "x";
      const sy = spread(probe.centers.map((c) => c.y));
      const sh = spread(probe.centers.map((c) => c[other]));
      if (sy > 1.2 * sh) {
        rotateWorld(orient, AXIS[probe.axle], Math.PI / 2);
        notes.push("Model was exported Z-up (standing on end) — stood it upright.");
        meshes = collect(wrapper);
      }
    } else if (size.y > 1.25 * Math.max(size.x, size.z)) {
      rotateWorld(orient, AXIS.x, -Math.PI / 2);
      notes.push("Model was exported Z-up (standing on end) — stood it upright.");
      meshes = collect(wrapper);
    }
  }

  // Wheels tell us which way is down and which axis is the length.
  let wheels = kind === "boat" ? [] : findWheels(meshes, kind);
  if (wheels.length >= 2) {
    const b = unionBox(meshes);
    const mid = (b.min.y + b.max.y) / 2;
    const above = wheels.filter((w) => w.center.y > mid).length;
    if (above > wheels.length / 2) {
      rotateWorld(orient, AXIS.x, Math.PI);
      notes.push("Model was upside-down — flipped it onto its wheels.");
      meshes = collect(wrapper);
      wheels = findWheels(meshes, kind);
    }
  }

  // Length axis: wheel spread when we have wheels, footprint otherwise.
  box = unionBox(meshes);
  size = box.size;
  let lengthAlongZ = size.z > size.x * 1.15;
  if (wheels.length >= 2) {
    const sx = spread(wheels.map((w) => w.center.x));
    const sz = spread(wheels.map((w) => w.center.z));
    if (Math.abs(sx - sz) > 0.05 * Math.max(sx, sz)) lengthAlongZ = sz > sx;
  }
  if (lengthAlongZ) {
    // +90° about y maps +z to +x: a model facing +z ends up facing +x.
    rotateWorldY(orient, Math.PI / 2);
    meshes = collect(wrapper);
    wheels = wheels.length ? findWheels(meshes, kind) : wheels;
  }

  // Which end is the front?
  box = unionBox(meshes);
  const front = frontScore(meshes, box, kind);
  if (front.score < 0) {
    rotateWorldY(orient, Math.PI);
    notes.push(`Model faced backwards — turned it around (${front.reason}).`);
    meshes = collect(wrapper);
    wheels = wheels.length ? findWheels(meshes, kind) : wheels;
  }

  // Scale to a plausible real-world length, centre, and sit it on its tyres.
  box = unionBox(meshes);
  const target = length ?? DEFAULT_LENGTH[kind] ?? DEFAULT_LENGTH.car;
  const scale = target / Math.max(box.size.x, 1e-6);
  wrapper.scale.setScalar(scale);
  wrapper.updateMatrixWorld(true);
  meshes = collect(wrapper);
  box = unionBox(meshes);
  wheels = kind === "boat" ? [] : findWheels(meshes, kind);
  let groundY = box.min.y;
  if (wheels.length >= 2) {
    const tyreBottom = Math.min(...wheels.map((w) => w.center.y - w.radius));
    // Something hangs below the tyres (a stray plane, a stand): trust the tyres.
    if (tyreBottom - box.min.y > 0.03 * box.size.y) {
      groundY = tyreBottom;
      notes.push("Geometry below the tyres ignored when placing the model on the ground.");
    }
  }
  wrapper.position.set(-box.center.x * 1, -groundY, -box.center.z);
  // Position is applied after scale, so the offsets above are already in metres.
  wrapper.updateMatrixWorld(true);

  // 3. Analyse (final space) ------------------------------------------------
  meshes = collect(wrapper, { area: true });
  box = unionBox(meshes);
  wheels = kind === "boat" ? [] : findWheels(meshes, kind);
  const mats = materialStats(meshes, wheels);
  const { roles, cabin: cabinRegion } = classify(mats, meshes, wheels, box, kind);

  // 4. Highlights -------------------------------------------------------------
  const { highlights, anchors, hero, cabin } = buildHighlights(meshes, wheels, roles, mats, box, kind, cabinRegion);

  const materialRoles = new Map();
  for (const [mat, role] of roles) materialRoles.set(mat, role);

  const dims = { length: round(box.size.x), width: round(box.size.z), height: round(box.size.y) };
  const paintRole = kind === "boat" ? "hull" : "paint";
  const paintMat = [...roles].filter(([, r]) => r === paintRole).map(([m]) => m).sort((a, b) => (mats.get(b)?.area ?? 0) - (mats.get(a)?.area ?? 0))[0];

  const analysis = {
    version: ANALYSIS_VERSION,
    kind,
    dims,
    scale: round(scale, 5),
    notes,
    removed,
    wheels: wheels.map((w) => ({ center: vec(w.center), radius: round(w.radius) })),
    wheelInches: wheelInches(wheels, roles, mats),
    roles: rolesByName(roles),
    originalPaint: paintMat ? describeFinish(paintMat, paintRole) : null,
    highlights,
    hero,
    anchors,
    cabin,
    stats: {
      meshes: meshes.length,
      materials: mats.size,
      triangles: meshes.reduce((s, m) => s + m.tris, 0),
    },
  };
  return { group: wrapper, analysis, materialRoles };
}

/** JSON-safe summary for the engine (Vector3 -> arrays, no object refs). */
export function summarize(analysis, extra = {}) {
  const toArr = (v) => (v?.isVector3 ? vec(v) : v);
  return {
    version: analysis.version,
    kind: analysis.kind,
    dims: analysis.dims,
    notes: analysis.notes,
    removed: analysis.removed,
    wheels: analysis.wheels,
    wheel_inches: analysis.wheelInches,
    roles: analysis.roles,
    original_paint: analysis.originalPaint,
    hero: analysis.hero,
    highlights: analysis.highlights.map((h) => ({ id: h.id, label: h.label, detail: h.detail, score: h.score, target: toArr(h.target), dir: h.dir, fit: h.fit })),
    stats: analysis.stats,
    analyzed_at: new Date().toISOString(),
    ...extra,
  };
}

// ---------------------------------------------------------------------------
// Mesh collection
// ---------------------------------------------------------------------------

function collect(root, { area = false } = {}) {
  root.updateMatrixWorld(true);
  const out = [];
  root.traverse((node) => {
    if (!node.isMesh || node.visible === false) return;
    const g = node.geometry;
    if (!g?.attributes?.position) return;
    if (!g.boundingBox) g.computeBoundingBox();
    const b = g.boundingBox.clone().applyMatrix4(node.matrixWorld);
    if (!isFinite(b.min.x) || !isFinite(b.max.x)) return;
    const size = b.getSize(new Vector3());
    const mats = Array.isArray(node.material) ? node.material : [node.material];
    out.push({
      node,
      box: b,
      size,
      center: b.getCenter(new Vector3()),
      tris: triCount(g),
      area: area ? worldArea(node) : 0,
      mats,
      names: nameChain(node),
    });
  });
  return out;
}

function nameChain(node) {
  const names = [];
  let n = node;
  for (let i = 0; i < 3 && n; i++, n = n.parent) if (n.name) names.push(n.name);
  const mats = Array.isArray(node.material) ? node.material : [node.material];
  for (const m of mats) if (m?.name) names.push(m.name);
  return names.join(" ").toLowerCase();
}

function triCount(g) {
  return Math.floor((g.index?.count ?? g.attributes.position.count) / 3);
}

// Surface area in world units, sampling large meshes (≤ ~20k triangles each).
const _a = new Vector3();
const _b = new Vector3();
const _c = new Vector3();
function worldArea(node) {
  const g = node.geometry;
  const pos = g.attributes.position;
  const idx = g.index;
  const tris = triCount(g);
  if (!tris) return 0;
  const stride = Math.max(1, Math.ceil(tris / 20000));
  let sum = 0;
  let sampled = 0;
  for (let t = 0; t < tris; t += stride) {
    const i0 = idx ? idx.getX(t * 3) : t * 3;
    const i1 = idx ? idx.getX(t * 3 + 1) : t * 3 + 1;
    const i2 = idx ? idx.getX(t * 3 + 2) : t * 3 + 2;
    _a.fromBufferAttribute(pos, i0).applyMatrix4(node.matrixWorld);
    _b.fromBufferAttribute(pos, i1).applyMatrix4(node.matrixWorld);
    _c.fromBufferAttribute(pos, i2).applyMatrix4(node.matrixWorld);
    _b.sub(_a);
    _c.sub(_a);
    sum += _b.cross(_c).length() / 2;
    sampled++;
  }
  return sampled ? (sum * tris) / sampled : 0;
}

function unionBox(meshes) {
  const min = new Vector3(Infinity, Infinity, Infinity);
  const max = new Vector3(-Infinity, -Infinity, -Infinity);
  for (const m of meshes) {
    min.min(m.box.min);
    max.max(m.box.max);
  }
  if (!meshes.length) {
    min.set(0, 0, 0);
    max.set(1, 1, 1);
  }
  return { min, max, size: max.clone().sub(min), center: min.clone().add(max).multiplyScalar(0.5) };
}

const AXIS = { x: new Vector3(1, 0, 0), y: new Vector3(0, 1, 0), z: new Vector3(0, 0, 1) };

// Rotate about a world axis regardless of earlier rotations.
function rotateWorld(obj, axis, angle) {
  obj.rotateOnWorldAxis(axis, angle);
  obj.updateMatrixWorld(true);
}

function rotateWorldY(obj, angle) {
  rotateWorld(obj, AXIS.y, angle);
}

// ---------------------------------------------------------------------------
// 1. Clean-up: ground planes, backdrops, sky domes
// ---------------------------------------------------------------------------

function removeJunk(root, notes) {
  const removed = [];
  for (let pass = 0; pass < 4; pass++) {
    const meshes = collect(root);
    if (meshes.length < 2) break;
    // Biggest cross-sections first; a backdrop is always among them. The model
    // may not be upright yet, so a "ground" is flat along whichever axis is thin.
    const cross = (m) => {
      const d = [m.size.x, m.size.y, m.size.z].sort((a, b) => b - a);
      return d[0] * d[1];
    };
    const byFoot = [...meshes].sort((a, b) => cross(b) - cross(a)).slice(0, 4);
    let victim = null;
    for (const cand of byFoot) {
      const rest = unionBox(meshes.filter((m) => m !== cand));
      const t = ["x", "y", "z"].reduce((a, b) => (cand.size[b] < cand.size[a] ? b : a));
      const [p, q] = ["x", "y", "z"].filter((k) => k !== t);
      const restFoot = Math.max(rest.size[p] * rest.size[q], 1e-9);
      const foot = cand.size[p] * cand.size[q];
      const restMax = Math.max(rest.size.x, rest.size.y, rest.size.z);
      const flat = cand.size[t] < 0.02 * Math.max(cand.size[p], cand.size[q]);
      const edge = 0.05 * rest.size[t];
      const atBottom = cand.center[t] <= rest.min[t] + edge || cand.center[t] >= rest.max[t] - edge;
      const named = RX.junk.test(cand.names);
      const encloses =
        cand.box.min.x < rest.min.x - 0.25 * rest.size.x &&
        cand.box.max.x > rest.max.x + 0.25 * rest.size.x &&
        cand.box.min.z < rest.min.z - 0.25 * rest.size.z &&
        cand.box.max.z > rest.max.z + 0.25 * rest.size.z &&
        cand.box.max.y > rest.max.y + 0.25 * rest.size.y;
      const groundPlane = flat && atBottom && foot > 1.6 * restFoot;
      const dome = encloses && Math.max(cand.size.x, cand.size.y, cand.size.z) > 1.4 * restMax;
      const namedFlat = named && flat && foot > 0.9 * restFoot;
      if (groundPlane || dome || namedFlat) {
        victim = cand;
        break;
      }
    }
    if (!victim) break;
    victim.node.parent?.remove(victim.node);
    removed.push(victim.node.name || "(unnamed)");
  }
  if (removed.length) notes.push(`Removed ${removed.length} ground/backdrop mesh${removed.length > 1 ? "es" : ""} (${removed.join(", ")}).`);
  return removed;
}

// ---------------------------------------------------------------------------
// 2a. Wheel detection
// ---------------------------------------------------------------------------

// A wheel part is round across its axle and thinner along it.
function roundness(m, axles = ["x", "z"]) {
  for (const a of axles) {
    const [p, q] = ["x", "y", "z"].filter((k) => k !== a);
    const d = Math.max(m.size[p], m.size[q]);
    if (d <= 0) continue;
    if (Math.min(m.size[p], m.size[q]) / d > 0.82 && m.size[a] < 0.8 * d) return { axle: a, d };
  }
  return null;
}

// Group concentric round parts (tyre, rim, disc) into wheel clusters.
function clusterRound(cands) {
  cands.sort((a, b) => b.d - a.d);
  const clusters = [];
  for (const c of cands) {
    const hit = clusters.find((k) => k.axle === c.axle && k.center.distanceTo(c.m.center) < 0.4 * k.d);
    if (hit) {
      hit.parts.push(c.m);
      hit.box.min.min(c.m.box.min);
      hit.box.max.max(c.m.box.max);
    } else {
      clusters.push({ axle: c.axle, d: c.d, center: c.m.center.clone(), parts: [c.m], box: c.m.box.clone() });
    }
  }
  return clusters;
}

// Orientation probe: which way do the wheel axles point, before we know up?
function probeWheels(meshes) {
  const all = unionBox(meshes);
  const M = Math.max(all.size.x, all.size.y, all.size.z, 1e-6);
  const cands = [];
  for (const m of meshes) {
    const r = roundness(m, ["x", "y", "z"]);
    if (r && r.d > 0.06 * M && r.d < 0.45 * M) cands.push({ m, d: r.d, axle: r.axle });
  }
  const group = dominantGroup(clusterRound(cands));
  if (group.length < 2) return null;
  return { axle: group[0].axle, centers: group.map((k) => k.center) };
}

// Real wheels come in a set: same axle direction, similar diameter. Pick the
// largest such set (ties: the bigger wheels). A lone round canopy, steering
// wheel or gauge never forms a set, so it cannot crowd out the wheels.
function dominantGroup(clusters) {
  let best = [];
  for (const k of clusters) {
    const set = clusters.filter((o) => o.axle === k.axle && o.d >= 0.75 * k.d && o.d <= 1.33 * k.d);
    const d = (g) => Math.max(...g.map((o) => o.d));
    if (set.length > best.length || (set.length === best.length && set.length && d(set) > d(best))) best = set;
  }
  return best;
}

function findWheels(meshes, kind) {
  if (!meshes.length) return [];
  const all = unionBox(meshes);
  const H = Math.max(all.size.y, 1e-6);
  const L = Math.max(all.size.x, all.size.z, 1e-6);

  const plausible = (d) => d >= 0.06 * L && d <= 0.75 * H + 0.05 * L;
  const cands = [];
  for (const m of meshes) {
    const round = roundness(m);
    if (round && plausible(m.size.y)) cands.push({ m, d: m.size.y, axle: round.axle });
  }
  // Many exports merge all four tyres (or rims) into one mesh. Split
  // wheel-named meshes that span the vehicle into corner groups by vertex
  // and treat each group as a part of its own.
  const pieces = [];
  for (const m of meshes) {
    if (roundness(m) || !RX.wheel.test(m.names)) continue;
    if (Math.max(m.size.x, m.size.z) < 0.3 * L) continue;
    for (const piece of splitCorners(m, kind)) {
      pieces.push(piece);
      const round = roundness(piece);
      if (round && plausible(piece.size.y)) cands.push({ m: piece, d: piece.size.y, axle: round.axle });
    }
  }
  if (!cands.length) return [];
  // Real wheels share an axle direction and roughly one diameter; keep the
  // dominant set (drops steering wheels, gauges, headlamp bowls, canopies).
  let wheels = dominantGroup(clusterRound(cands));
  // They sit at the extremes of the vehicle's lower half... or upper half if upside-down.
  const lowY = all.min.y + 0.5 * H;
  const lower = wheels.filter((k) => k.center.y < lowY);
  const upper = wheels.filter((k) => k.center.y >= lowY);
  wheels = lower.length >= upper.length ? lower : upper;
  // ...and all touch the ground (a round part higher up is a steering wheel,
  // a halo or a spare, not a road wheel).
  if (wheels.length) {
    const tol = 0.12 * H;
    if (lower.length >= upper.length) {
      const ground = Math.min(...wheels.map((k) => k.box.min.y));
      wheels = wheels.filter((k) => k.box.min.y <= ground + tol);
    } else {
      const top = Math.max(...wheels.map((k) => k.box.max.y));
      wheels = wheels.filter((k) => k.box.max.y >= top - tol);
    }
  }

  const want = kind === "bike" ? 2 : 4;
  wheels = wheels.sort((a, b) => b.parts.length - a.parts.length || b.d - a.d).slice(0, kind === "bike" ? 3 : 8);
  if (wheels.length < 2) return [];
  if (kind !== "bike" && wheels.length < 3) {
    // Two round things on a car are usually a pair of lamps, unless both
    // touch the ground (a model with only one side's wheels modelled).
    const grounded = wheels.every((w) => w.box.min.y - all.min.y < 0.08 * H || w.box.max.y - all.max.y > -0.08 * H);
    if (!grounded || wheels.length < want / 2) return [];
  }
  const out = wheels.map((k) => {
    const c = k.box.min.clone().add(k.box.max).multiplyScalar(0.5);
    const sz = k.box.max.clone().sub(k.box.min);
    // A split piece stands for its whole (merged) mesh.
    const parts = [...new Set(k.parts.map((p) => p.whole ?? p))];
    return { center: c, radius: sz.y / 2, axle: k.axle, parts, box: k.box };
  });
  // Non-round parts inside a wheel's volume (calipers, bolts, valve caps)
  // belong to that wheel too — whole meshes, or merged ones piece by piece.
  const inWheel = (c, w) => {
    const pad = 0.08 * w.radius;
    const b = w.box;
    return c.x >= b.min.x - pad && c.x <= b.max.x + pad && c.y >= b.min.y - pad && c.y <= b.max.y + pad && c.z >= b.min.z - pad && c.z <= b.max.z + pad;
  };
  const taken = new Set(out.flatMap((w) => w.parts));
  for (const m of meshes) {
    if (taken.has(m)) continue;
    const big = Math.max(m.size.x, m.size.y, m.size.z);
    const w = out.find((w) => big <= 2.2 * w.radius && inWheel(m.center, w));
    if (w) {
      w.parts.push(m);
      taken.add(m);
    }
  }
  const byWhole = new Map();
  for (const p of pieces) (byWhole.get(p.whole) ?? byWhole.set(p.whole, []).get(p.whole)).push(p);
  for (const [whole, ps] of byWhole) {
    if (taken.has(whole)) continue;
    const homes = ps.map((p) => out.find((w) => inWheel(p.center, w))).filter(Boolean);
    if (homes.length >= Math.ceil(ps.length / 2)) {
      for (const w of new Set(homes)) w.parts.push(whole);
      taken.add(whole);
    }
  }
  return out;
}

// Split a merged wheel mesh into its corners (cars) or ends (bikes) by vertex
// position, returning pseudo-parts with their own bounds.
function splitCorners(m, kind) {
  const pos = m.node.geometry.attributes.position;
  const stride = Math.max(1, Math.ceil(pos.count / 20000));
  const cx = m.center.x;
  const cz = m.center.z;
  const alongX = m.size.x >= m.size.z;
  const groups = new Map();
  const v = new Vector3();
  for (let i = 0; i < pos.count; i += stride) {
    v.fromBufferAttribute(pos, i).applyMatrix4(m.node.matrixWorld);
    const key = kind === "bike" ? ((alongX ? v.x > cx : v.z > cz) ? "a" : "b") : `${v.x > cx ? 1 : 0}${v.z > cz ? 1 : 0}`;
    let g = groups.get(key);
    if (!g) groups.set(key, (g = { min: v.clone(), max: v.clone(), n: 0 }));
    g.min.min(v);
    g.max.max(v);
    g.n++;
  }
  const out = [];
  for (const g of groups.values()) {
    if (g.n < 12) continue;
    const size = g.max.clone().sub(g.min);
    out.push({
      whole: m,
      node: m.node,
      box: { min: g.min, max: g.max, clone() { return { min: this.min.clone(), max: this.max.clone() }; } },
      size,
      center: g.min.clone().add(g.max).multiplyScalar(0.5),
      area: m.area / groups.size,
      tris: 0,
      mats: m.mats,
      names: m.names,
    });
  }
  return out;
}

// ---------------------------------------------------------------------------
// 2b. Front detection
// ---------------------------------------------------------------------------

function frontScore(meshes, box, kind) {
  const cx = box.center.x;
  const half = Math.max(box.size.x / 2, 1e-6);
  let score = 0;
  const reasons = [];
  let nameVotes = 0;
  let lightVotes = 0;

  for (const m of meshes) {
    const side = (m.center.x - cx) / half; // -1 rear .. +1 front (if already right)
    if (Math.abs(side) < 0.15) continue;
    const s = Math.sign(side);
    if (RX.front.test(m.names) && !RX.rear.test(m.names)) nameVotes += s;
    else if (RX.rear.test(m.names) && !RX.front.test(m.names)) nameVotes -= s;

    // Lights: emissive or named lamps. Red at the back, white/amber at the front.
    for (const mat of m.mats) {
      if (!mat) continue;
      const em = emissiveStrength(mat);
      const lamp = em > 0.05 || RX.light.test(m.names);
      if (!lamp) continue;
      const col = em > 0.05 ? mat.emissive : mat.color;
      if (!col) continue;
      const red = col.r > 0.35 && col.r > 2.2 * col.g && col.r > 2.2 * col.b;
      const white = Math.min(col.r, col.g, col.b) > 0.45;
      if (red) lightVotes -= 2 * s;
      else if (white && em > 0.05) lightVotes += 1 * s;
    }
  }
  if (nameVotes) {
    score += nameVotes;
    reasons.push("part names");
  }
  if (lightVotes) {
    score += lightVotes;
    reasons.push("light colours");
  }

  if (kind === "boat") {
    // The bow is the narrow end of the hull.
    const prof = widthProfile(meshes, box);
    if (prof) {
      const diff = prof.rear - prof.front; // positive: +x end narrower = bow at +x
      if (Math.abs(diff) > 0.08 * box.size.z) {
        score += Math.sign(diff) * 4;
        reasons.push("hull shape");
      }
    }
  }
  return { score, reason: reasons.join(" + ") || "default" };
}

// Max half-width of the model within the outer 12% at each end.
function widthProfile(meshes, box) {
  const L = box.size.x;
  const v = new Vector3();
  let front = 0;
  let rear = 0;
  let any = false;
  for (const m of meshes) {
    const pos = m.node.geometry.attributes.position;
    const stride = Math.max(1, Math.ceil(pos.count / 4000));
    for (let i = 0; i < pos.count; i += stride) {
      v.fromBufferAttribute(pos, i).applyMatrix4(m.node.matrixWorld);
      const t = (v.x - box.min.x) / L;
      const w = Math.abs(v.z - box.center.z);
      if (t > 0.88) front = Math.max(front, w);
      else if (t < 0.12) rear = Math.max(rear, w);
      any = true;
    }
  }
  return any ? { front, rear } : null;
}

// ---------------------------------------------------------------------------
// 3. Material statistics + classification
// ---------------------------------------------------------------------------

function emissiveStrength(mat) {
  if (!mat?.emissive) return 0;
  const e = mat.emissive;
  return Math.max(e.r, e.g, e.b) * (mat.emissiveIntensity ?? 1) * (mat.emissiveMap ? 1 : 1);
}

// Base colour times the average of its colour texture (a white base colour
// with a dark texture is still a dark tyre).
const texAverage = new WeakMap();
function effectiveColor(mat) {
  const c = mat.color ? mat.color.clone() : new Color(1, 1, 1);
  if (mat.map) {
    if (!texAverage.has(mat.map)) texAverage.set(mat.map, averageTextureColor(mat.map));
    const t = texAverage.get(mat.map);
    if (t) c.multiply(t);
  }
  return c;
}

function lum(c) {
  return c ? 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b : 0;
}

function sat(c) {
  if (!c) return 0;
  const mx = Math.max(c.r, c.g, c.b);
  const mn = Math.min(c.r, c.g, c.b);
  return mx <= 0 ? 0 : (mx - mn) / mx;
}

const DECAL = /(decal|logo|sticker|livery|badge|emblem|number|text|sponsor|label|kers)/;
function isGlass(mat) {
  const name = (mat.name ?? "").toLowerCase();
  if (/glass|window|windshield|windscreen|vitre|verre|scheibe/.test(name)) return true;
  if (DECAL.test(name)) return false;
  if ((mat.transmission ?? 0) > 0.1) return true;
  // See-through and not bright: tinted glazing. Bright see-through surfaces
  // are decals, light lenses' haze or fabric.
  return !!mat.transparent && (mat.opacity ?? 1) < 0.95 && lum(mat.color) < 0.55;
}

function materialStats(meshes, wheels) {
  const wheelOf = new Map();
  wheels.forEach((w, i) => w.parts.forEach((p) => wheelOf.set(p, i)));
  const stats = new Map();
  for (const m of meshes) {
    const share = 1 / m.mats.length;
    for (const mat of m.mats) {
      if (!mat) continue;
      let s = stats.get(mat);
      if (!s) {
        s = { mat, area: 0, wheelArea: 0, meshes: [], min: new Vector3(Infinity, Infinity, Infinity), max: new Vector3(-Infinity, -Infinity, -Infinity), wheelDiamRatio: 0 };
        stats.set(mat, s);
      }
      const a = m.area * share;
      s.area += a;
      s.meshes.push(m);
      s.min.min(m.box.min);
      s.max.max(m.box.max);
      if (wheelOf.has(m)) {
        s.wheelArea += a;
        const w = wheels[wheelOf.get(m)];
        s.wheelDiamRatio = Math.max(s.wheelDiamRatio, m.size.y / (2 * w.radius));
      }
    }
  }
  for (const s of stats.values()) {
    s.size = s.max.clone().sub(s.min);
    s.center = s.min.clone().add(s.max).multiplyScalar(0.5);
    s.wheelFrac = s.area > 0 ? s.wheelArea / s.area : s.wheelArea > 0 ? 1 : 0;
    s.color = effectiveColor(s.mat);
    s.lum = lum(s.color);
    s.sat = sat(s.color);
    s.metal = s.mat.metalness ?? 0;
    s.rough = s.mat.roughness ?? 0.5;
    s.emissive = emissiveStrength(s.mat);
    s.glass = isGlass(s.mat);
  }
  return stats;
}

function classify(stats, meshes, wheels, box, kind) {
  const roles = new Map();
  const L = box.size.x;
  const W = box.size.z;
  const H = box.size.y;

  // Glass and lights first: they are unambiguous and define the cabin.
  for (const s of stats.values()) {
    if (s.glass) roles.set(s.mat, "glass");
    else if (s.emissive > 0.05) roles.set(s.mat, s.center.x < box.center.x && s.mat.emissive.r > 2 * s.mat.emissive.g ? "taillight" : "lamp");
  }

  // Wheels: tyre = full-diameter dark part, rims = the big bright/metal part
  // inside it, calipers = small coloured part inside it.
  const wheelMats = [...stats.values()].filter((s) => !roles.has(s.mat) && s.wheelFrac > 0.6);
  if (wheelMats.length) {
    const tyre = wheelMats
      .filter((s) => s.wheelDiamRatio > 0.9 && s.lum < 0.2 && s.metal < 0.5)
      .sort((a, b) => b.area - a.area)[0];
    if (tyre) roles.set(tyre.mat, "tyre");
    const rims = wheelMats
      .filter((s) => !roles.has(s.mat) && s.wheelDiamRatio > 0.45 && (s.metal > 0.3 || s.lum > 0.2))
      .sort((a, b) => b.area - a.area)[0];
    if (rims) roles.set(rims.mat, "rims");
    const caliper = wheelMats
      .filter((s) => !roles.has(s.mat) && s.wheelDiamRatio < 0.6 && s.sat > 0.35 && s.lum > 0.05)
      .sort((a, b) => b.sat - a.sat)[0];
    if (caliper) roles.set(caliper.mat, "calipers");
    // Remaining dark wheel parts with no metal are probably also tyre rubber.
    if (!tyre) {
      const dark = wheelMats.filter((s) => !roles.has(s.mat) && s.lum < 0.12).sort((a, b) => b.area - a.area)[0];
      if (dark) roles.set(dark.mat, "tyre");
    }
  }

  // Cabin: the glasshouse if there is one, else the central upper volume.
  const cabin = cabinBox(stats, roles, box, kind);

  // Interior: materials whose surface sits mostly inside the cabin.
  if (cabin && kind !== "bike") {
    let best = null;
    for (const s of stats.values()) {
      if (roles.has(s.mat) || s.wheelFrac > 0.3) continue;
      // Seats, dash and trim are confined; a livery or body colour that also
      // shows through the cabin spans most of the vehicle.
      if (s.size.x > 0.5 * L || s.size.z > 0.95 * W) continue;
      let inside = 0;
      for (const m of s.meshes) {
        if (m.size.x > 0.6 * L) continue; // body shell passes through the cabin
        if (insideBox(m.center, cabin)) inside += m.area / m.mats.length;
      }
      const frac = s.area > 0 ? inside / s.area : 0;
      if (frac < 0.6) continue;
      const score = inside * (RX.interior.test(s.mat.name?.toLowerCase() ?? "") ? 3 : 1);
      if (!best || score > best.score) best = { s, score };
    }
    if (best) roles.set(best.s.mat, "interior");
  }

  // Paint: the material that wraps the most of the outside of the vehicle,
  // weighted towards what paint looks like (glossy) and is called (livery,
  // body) and away from exposed carbon, trim and floor panels.
  const paintRole = kind === "boat" ? "hull" : "paint";
  const PAINTY = /(paint|livery|body|shell|exterior|carros|coque|lack|hull)/;
  const NOT_PAINTY = /(carbon|crbn|black|trim|chrome|plastic|rubber|grill|grille|mesh|generic|detail|interior|floor|under|metal|alu)/;
  const look = (s) => {
    const name = (s.mat.name ?? "").toLowerCase();
    let k = 1;
    if (PAINTY.test(name)) k *= 2.5;
    else if (NOT_PAINTY.test(name)) k *= 0.35;
    if (s.rough > 0.8 && !(s.mat.clearcoat > 0.2)) k *= 0.5; // near-matte: rarely the main finish
    return k;
  };
  const pick = (allowDark) => {
    let best = null;
    for (const s of stats.values()) {
      if (roles.has(s.mat) || s.wheelFrac > 0.3 || s.glass || s.emissive > 0.05) continue;
      const chrome = s.metal > 0.85 && s.rough < 0.2 && s.lum > 0.45;
      if (chrome) continue;
      if (!allowDark && s.lum < 0.035) continue;
      const spanX = Math.min(1, s.size.x / Math.max(L, 1e-6));
      const spanZ = Math.min(1, s.size.z / Math.max(W, 1e-6));
      const spanY = Math.min(1, s.size.y / Math.max(H, 1e-6));
      const score = s.area * (0.2 + spanX) * (0.3 + spanZ) * (0.5 + spanY) * (s.lum < 0.035 ? 0.4 : 1) * look(s);
      if (!best || score > best.score) best = { s, score };
    }
    return best?.s;
  };
  const paint = pick(false) ?? pick(true);
  if (paint) roles.set(paint.mat, paintRole);

  return { roles, cabin };
}

function cabinBox(stats, roles, box, kind) {
  if (kind === "bike") return null;
  const glass = [...stats.values()].filter((s) => roles.get(s.mat) === "glass" && s.size.x < 0.8 * box.size.x);
  if (glass.length) {
    const min = new Vector3(Infinity, Infinity, Infinity);
    const max = new Vector3(-Infinity, -Infinity, -Infinity);
    for (const g of glass) {
      for (const m of g.meshes) {
        // Ignore lamp lenses: glass low on the body or at the very ends.
        if (m.center.y < box.min.y + 0.45 * box.size.y) continue;
        min.min(m.box.min);
        max.max(m.box.max);
      }
    }
    if (isFinite(min.x) && max.x - min.x > 0.15 * box.size.x) {
      return {
        min: new Vector3(min.x, box.min.y + 0.18 * box.size.y, box.center.z - 0.42 * box.size.z),
        max: new Vector3(max.x, max.y, box.center.z + 0.42 * box.size.z),
      };
    }
  }
  if (kind === "boat") return null;
  return {
    min: new Vector3(box.center.x - 0.25 * box.size.x, box.min.y + 0.3 * box.size.y, box.center.z - 0.35 * box.size.z),
    max: new Vector3(box.center.x + 0.2 * box.size.x, box.min.y + 0.85 * box.size.y, box.center.z + 0.35 * box.size.z),
  };
}

function insideBox(p, b) {
  return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
}

// ---------------------------------------------------------------------------
// 4. Highlights
// ---------------------------------------------------------------------------

function buildHighlights(meshes, wheels, roles, stats, box, kind, cabin) {
  const L = box.size.x;
  const H = box.size.y;
  const list = [];
  const add = (h) => list.push({ fit: 0.4, ...h, target: h.target.clone() });
  const centroid = (ms) => {
    const c = new Vector3();
    let w = 0;
    for (const m of ms) {
      const a = Math.max(m.area, 1e-6);
      c.addScaledVector(m.center, a);
      w += a;
    }
    return w ? c.multiplyScalar(1 / w) : null;
  };
  const named = (re) => meshes.filter((m) => re.test(m.names));

  // Wheels: the front wheel on the camera (+z) side.
  let wheelAnchor = null;
  if (wheels.length) {
    const near = wheels.filter((w) => w.center.z >= box.center.z);
    const pool = near.length ? near : wheels;
    const fw = pool.reduce((a, b) => (b.center.x > a.center.x ? b : a));
    wheelAnchor = fw.center.clone();
    const inches = wheelInches(wheels, roles, stats);
    const caliper = [...roles].find(([, r]) => r === "calipers");
    const bits = [`${wheels.length} wheels`];
    if (inches) bits.push(`≈${inches}″ rims`);
    if (caliper) bits.push("coloured calipers");
    add({ id: "wheels", label: "Wheels", detail: bits.join(" · "), score: 3, target: wheelAnchor, dir: [0.55, 0.12, 1], fit: kind === "bike" ? 0.45 : 0.32 });
  }

  // Lights.
  const lampMeshes = meshes.filter((m) => m.mats.some((mat) => mat && (roles.get(mat) === "lamp" || roles.get(mat) === "taillight")) || RX.light.test(m.names));
  const fronts = lampMeshes.filter((m) => (m.center.x > box.center.x + 0.25 * L && !RX.taillight.test(m.names)) || RX.headlight.test(m.names));
  const rears = lampMeshes.filter((m) => (m.center.x < box.center.x - 0.25 * L && !RX.headlight.test(m.names)) || RX.taillight.test(m.names));
  const fc = centroid(fronts);
  if (fc) add({ id: "headlights", label: "Headlights", detail: lightDetail(fronts, roles), score: 2, target: fc, dir: [1, 0.18, 0.75], fit: 0.42 });
  const rc = centroid(rears);
  if (rc) add({ id: "taillights", label: "Tail lights", detail: lightDetail(rears, roles), score: 2, target: rc, dir: [-1, 0.22, 0.75], fit: 0.42 });

  // Interior / cockpit.
  const interiorMat = [...roles].find(([, r]) => r === "interior")?.[0];
  let cabinCenter = null;
  if (cabin) {
    cabinCenter = cabin.min.clone().add(cabin.max).multiplyScalar(0.5);
    if (interiorMat) {
      const c = stats.get(interiorMat)?.center;
      if (c) cabinCenter = c.clone();
    }
    if (interiorMat || kind === "car") {
      const seats = named(/seat/).length;
      add({
        id: "interior",
        label: "Interior",
        detail: interiorMat ? `${seats ? `${seats} seat part${seats > 1 ? "s" : ""} · ` : ""}${finishWord(interiorMat)} trim` : "Cabin view",
        score: interiorMat ? 2 : 1,
        target: cabinCenter,
        dir: [0.15, 0.42, 1],
        fit: 0.5,
      });
    }
  }

  // Named performance features.
  const wing = named(RX.wing).filter((m) => m.center.x < box.center.x && m.center.y > box.min.y + 0.4 * H);
  const wc = centroid(wing);
  if (wc) add({ id: "wing", label: "Rear wing", detail: "Aerodynamic spoiler", score: 2, target: wc, dir: [-1, 0.6, 0.7], fit: 0.45 });

  const ex = named(RX.exhaust);
  const ec = centroid(ex);
  if (ec) add({ id: "exhaust", label: "Exhaust", detail: `${countClusters(ex)} outlet${countClusters(ex) > 1 ? "s" : ""}`, score: 1.5, target: ec, dir: [-1, 0.12, 0.45], fit: 0.35 });

  const en = named(RX.engine);
  const nc = centroid(en);
  if (nc) add({ id: "engine", label: kind === "boat" ? "Propulsion" : "Engine", detail: "Visible powertrain", score: 1.5, target: nc, dir: [0.25, 0.45, 1], fit: 0.45 });

  // Panoramic glass roof.
  const roofGlass = meshes.filter((m) => m.mats.some((mat) => mat && roles.get(mat) === "glass") && m.center.y > box.min.y + 0.8 * H && m.size.y < 0.35 * m.size.x);
  const rg = centroid(roofGlass);
  if (rg && kind === "car" && roofGlass.some((m) => m.size.x > 0.18 * L)) add({ id: "roof", label: "Glass roof", detail: "Panoramic glazing", score: 1, target: rg, dir: [0.2, 1, 0.5], fit: 0.6 });

  // Profile always exists: the stance.
  add({
    id: "profile",
    label: "Profile",
    detail: `${round(L, 2)} m long · ${round(H, 2)} m tall`,
    score: 0.5,
    target: new Vector3(box.center.x, H * 0.38, box.center.z),
    dir: [0, 0.16, 1],
    fit: 1.05,
  });

  list.sort((a, b) => b.score - a.score);
  const frontPts = (fc ? 2 : 0) + 1;
  const rearPts = (rc ? 2 : 0) + (wc ? 2 : 0) + (ec ? 1 : 0);
  const hero = rearPts > frontPts + 1 ? "rear34" : "front34";

  const anchors = {};
  if (wheelAnchor) anchors.wheel = wheelAnchor;
  if (cabinCenter) anchors.cabin = cabinCenter;
  return { highlights: list, anchors, hero, cabin: cabin ? { min: vec(cabin.min), max: vec(cabin.max) } : null };
}

function lightDetail(ms, roles) {
  const led = ms.some((m) => /led|drl/.test(m.names));
  const lit = ms.some((m) => m.mats.some((mat) => mat && (roles.get(mat) === "lamp" || roles.get(mat) === "taillight")));
  return [led ? "LED signature" : null, lit ? "illuminated" : "lamp units"].filter(Boolean).join(" · ");
}

function countClusters(ms) {
  const pts = [];
  for (const m of ms) if (!pts.some((p) => p.distanceTo(m.center) < 0.12)) pts.push(m.center);
  return Math.max(1, pts.length);
}

function wheelInches(wheels, roles, stats) {
  if (!wheels.length) return null;
  const tyreD = (2 * wheels.reduce((s, w) => s + w.radius, 0)) / wheels.length;
  const rimMat = [...roles].find(([, r]) => r === "rims")?.[0];
  const ratio = rimMat ? stats.get(rimMat)?.wheelDiamRatio : 0;
  // Without an identified rim, assume a typical 70% rim-to-tyre diameter.
  const rimD = tyreD * (ratio && ratio < 0.95 ? ratio : 0.7);
  const inches = Math.round(rimD / 0.0254);
  return inches >= 10 && inches <= 30 ? inches : null;
}

function finishWord(mat) {
  const r = mat.roughness ?? 0.5;
  if (lum(effectiveColor(mat)) < 0.06) return "dark";
  return r > 0.6 ? "soft-touch" : "leather";
}

// ---------------------------------------------------------------------------
// Original paint: the artist's own finish becomes a configurator option.
// ---------------------------------------------------------------------------

export function describeFinish(mat, role) {
  const metal = mat.metalness ?? 0;
  const rough = mat.roughness ?? 0.5;
  let finish = "gloss";
  if (metal > 0.85 && rough < 0.18) finish = "chrome";
  else if (metal > 0.4) finish = rough > 0.55 ? "satin" : "metallic";
  else if (rough > 0.6) finish = "matte";
  else if ((mat.iridescence ?? 0) > 0.3) finish = "pearl";
  const base = effectiveColor(mat);
  return {
    role,
    color: `#${base.getHexString()}`,
    finish,
    textured: !!mat.map,
    material: mat.name || null,
  };
}

function averageTextureColor(tex) {
  try {
    const img = tex.image;
    if (!img || typeof document === "undefined") return null;
    const c = document.createElement("canvas");
    c.width = c.height = 8;
    const g = c.getContext("2d", { willReadFrequently: true });
    g.drawImage(img, 0, 0, 8, 8);
    const d = g.getImageData(0, 0, 8, 8).data;
    let r = 0;
    let gg = 0;
    let b = 0;
    for (let i = 0; i < d.length; i += 4) {
      r += d[i];
      gg += d[i + 1];
      b += d[i + 2];
    }
    const n = d.length / 4;
    // Canvas pixels are sRGB; convert into the working (linear) space.
    return new Color().setRGB(r / n / 255, gg / n / 255, b / n / 255, "srgb");
  } catch {
    return null;
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function rolesByName(roles) {
  const out = {};
  for (const [mat, role] of roles) {
    (out[role] ??= []).push(mat.name || "(unnamed)");
  }
  return out;
}

function spread(xs) {
  return xs.length ? Math.max(...xs) - Math.min(...xs) : 0;
}

function round(x, d = 3) {
  const k = 10 ** d;
  return Math.round(x * k) / k;
}

function vec(v) {
  return [round(v.x), round(v.y), round(v.z)];
}
