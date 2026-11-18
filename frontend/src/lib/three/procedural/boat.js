// Procedural boat builder. Hulls are lofted from V-shaped cross-sections
// (keel -> chine -> sheer) along the length; x runs transom (0) -> bow (L).
import { BufferGeometry, DoubleSide, Float32BufferAttribute, Group, Shape, ShapeGeometry, Vector3 } from "three";
import { cyl, extrude, mesh, rbox, tube, v2 } from "./common.js";

const BOATS = {
  speedboat: { L: 12.8, beam: 0.28, depth: 0.13, deadrise: 0.42, kind: "console" },
  fishing: { L: 8.6, beam: 0.33, depth: 0.15, deadrise: 0.4, kind: "console", rails: true },
  yacht: { L: 24.4, beam: 0.25, depth: 0.15, deadrise: 0.3, kind: "yacht" },
  sailboat: { L: 11.5, beam: 0.34, depth: 0.1, deadrise: 0.2, kind: "sail" },
  catamaran: { L: 13.7, beam: 0.12, depth: 0.15, deadrise: 0.35, kind: "cat" },
  pontoon: { L: 7.4, beam: 0.35, depth: 0.1, deadrise: 0, kind: "pontoon" },
  jetski: { L: 3.4, beam: 0.36, depth: 0.2, deadrise: 0.38, kind: "pwc" },
};

// Half cross-section at station t (0 transom .. 1 bow).
function section(P, t) {
  const halfBeam = (P.L * P.beam) / 2;
  const taper = t < 0.55 ? 1 : Math.pow(Math.max(0, Math.cos(((t - 0.55) / 0.45) * (Math.PI / 2))), 0.75);
  const hb = Math.max(0.002, halfBeam * taper);
  const D = P.L * P.depth;
  const keel = t > 0.7 ? Math.pow((t - 0.7) / 0.3, 2) * D * 0.75 : 0;
  const chineY = keel + hb * 0.85 * Math.tan(P.deadrise) * (0.6 + 0.4 * (1 - t));
  const sheer = D * (1 + 0.18 * t * t);
  return [
    [0, keel],
    [hb * 0.88, Math.min(chineY, sheer - 0.05)],
    [hb, Math.min(chineY + 0.03, sheer - 0.03)],
    [hb * (1 + 0.04 * (1 - t)), sheer],
  ];
}

// Loft a strip of the hull surface between section point indices i0..i1.
function loft(P, i0, i1, stations = 48, offset = 0) {
  const pos = [];
  const idx = [];
  const cols = i1 - i0 + 1;
  for (let s = 0; s <= stations; s++) {
    const t = s / stations;
    const sec = section(P, t);
    for (const side of [1, -1]) {
      for (let i = i0; i <= i1; i++) {
        const [z, y] = sec[i];
        pos.push(t * P.L, y, side * (z + offset));
      }
    }
  }
  const rowLen = cols * 2;
  for (let s = 0; s < stations; s++) {
    for (let sideIdx = 0; sideIdx < 2; sideIdx++) {
      for (let i = 0; i < cols - 1; i++) {
        const a = s * rowLen + sideIdx * cols + i;
        const b = a + 1;
        const c = a + rowLen;
        const d = c + 1;
        if (sideIdx === 0) idx.push(a, c, b, b, c, d);
        else idx.push(a, b, c, b, d, c);
      }
    }
  }
  const g = new BufferGeometry();
  g.setAttribute("position", new Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return g;
}

// Accent band on the topsides between fractions f0..f1 of the chine->sheer height.
function accentBand(P, f0, f1, stations = 48) {
  const pos = [];
  const idx = [];
  for (let s = 0; s <= stations; s++) {
    const t = s / stations;
    const sec = section(P, t);
    const [zc, yc] = sec[2];
    const [zs, ys] = sec[3];
    for (const side of [1, -1]) {
      for (const f of [f0, f1]) {
        const z = zc + (zs - zc) * f;
        const y = yc + (ys - yc) * f;
        pos.push(t * P.L, y, side * (z + 0.006));
      }
    }
  }
  for (let s = 0; s < stations; s++) {
    const a = s * 4;
    const n = a + 4;
    idx.push(a, n, a + 1, a + 1, n, n + 1);
    idx.push(a + 2, a + 3, n + 2, a + 3, n + 3, n + 2);
  }
  const g = new BufferGeometry();
  g.setAttribute("position", new Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return g;
}

// Flat deck following the sheer line, slightly inset below the gunwale.
function deckGeometry(P, inset = 0.06, drop = 0.08, stations = 48) {
  const pts = [];
  for (let s = 0; s <= stations; s++) {
    const t = s / stations;
    const [z, y] = section(P, t)[3];
    pts.push([t * P.L, y - drop, Math.max(0.001, z - inset)]);
  }
  const pos = [];
  const idx = [];
  for (const [x, y, z] of pts) pos.push(x, y, z, x, y, -z);
  for (let s = 0; s < stations; s++) {
    const a = s * 2;
    idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2);
  }
  const g = new BufferGeometry();
  g.setAttribute("position", new Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  g.computeVertexNormals();
  return g;
}

function transom(P) {
  const sec = section(P, 0);
  const shape = new Shape([...sec.map(([z, y]) => v2(z, y)), ...[...sec].reverse().map(([z, y]) => v2(-z, y))]);
  const g = new ShapeGeometry(shape);
  g.rotateY(Math.PI / 2);
  return g;
}

function buildHull(P, mats, group) {
  // Lofted surfaces are open shells; render both faces so winding never shows.
  for (const role of ["hull", "hull_bottom", "hull_accent", "deck"]) mats.get(role).side = DoubleSide;
  group.add(mesh(loft(P, 0, 1), mats.get("hull_bottom"), { name: "hull-bottom" }));
  group.add(mesh(loft(P, 1, 3), mats.get("hull"), { name: "hull" }));
  group.add(mesh(accentBand(P, 0.55, 0.68), mats.get("hull_accent"), { cast: false }));
  group.add(mesh(transom(P), mats.get("hull")));
  group.add(mesh(deckGeometry(P), mats.get("deck"), { cast: false, receive: true, name: "deck" }));
}

export function buildBoat(shape = {}, parts = {}, mats) {
  const base = BOATS[shape.body] ?? BOATS.speedboat;
  const L = shape.length ?? base.L;
  const P = { ...base, L };
  const root = new Group();
  const hullGroup = new Group();
  root.add(hullGroup);
  const D = L * base.depth;
  const halfBeam = (L * base.beam) / 2;
  const cutaway = [];
  const up = mats.get("upholstery");
  const metal = mats.get("metal");

  if (base.kind === "cat") {
    const offset = L * 0.19;
    for (const z of [-offset, offset]) {
      const g = new Group();
      buildHull(P, mats, g);
      g.position.z = z;
      hullGroup.add(g);
    }
    const bridge = mesh(rbox(L * 0.8, D * 0.3, offset * 2 + halfBeam, 0.1), mats.get("hull"));
    bridge.position.set(L * 0.42, D * 0.95, 0);
    hullGroup.add(bridge);
    const deck = mesh(rbox(L * 0.8, 0.04, offset * 2 + halfBeam - 0.2, 0.02), mats.get("deck"), { cast: false });
    deck.position.set(L * 0.42, D * 1.12, 0);
    hullGroup.add(deck);
    addSuperstructure(hullGroup, mats, { x0: L * 0.2, x1: L * 0.72, y: D * 1.12, w: offset * 2, h: D * 0.75, levels: 2 }, cutaway);
  } else if (base.kind === "pontoon") {
    const tubes = parts.engines >= 2 ? [-1, 0, 1] : [-1, 1];
    for (const k of tubes) {
      const t = cyl(L * 0.032, L * 0.032, L * 0.86, 24);
      t.rotateZ(Math.PI / 2);
      const m = mesh(t, metal);
      m.position.set(L * 0.45, L * 0.032, k * halfBeam * 0.72);
      hullGroup.add(m);
      const nose = cyl(0.001, L * 0.032, L * 0.12, 24);
      nose.rotateZ(-Math.PI / 2);
      const n = mesh(nose, metal);
      n.position.set(L * 0.94, L * 0.032 + 0.02, k * halfBeam * 0.72);
      hullGroup.add(n);
    }
    const platform = mesh(rbox(L * 0.98, 0.1, halfBeam * 2, 0.03), mats.get("deck"));
    platform.position.set(L * 0.48, L * 0.075, 0);
    hullGroup.add(platform);
    // Fence panels carry the "hull" colour on pontoons.
    const fy = L * 0.075 + 0.4;
    for (const s of [-1, 1]) {
      const side = mesh(rbox(L * 0.78, 0.62, 0.04, 0.02), mats.get("hull"));
      side.position.set(L * 0.48, fy, s * (halfBeam - 0.03));
      hullGroup.add(side);
      const accent = mesh(rbox(L * 0.78, 0.06, 0.045, 0.01), mats.get("hull_accent"), { cast: false });
      accent.position.set(L * 0.48, fy - 0.2, s * (halfBeam - 0.03));
      hullGroup.add(accent);
    }
    const bow = mesh(rbox(0.04, 0.62, halfBeam * 1.2, 0.02), mats.get("hull"));
    bow.position.set(L * 0.9, fy, 0);
    hullGroup.add(bow);
    for (const s of [-1, 1]) {
      const lounge = mesh(rbox(L * 0.32, 0.42, 0.55, 0.08), up);
      lounge.position.set(L * 0.66, L * 0.075 + 0.26, s * (halfBeam - 0.38));
      hullGroup.add(lounge);
      const back = mesh(rbox(L * 0.32, 0.4, 0.12, 0.05), up);
      back.position.set(L * 0.66, L * 0.075 + 0.6, s * (halfBeam - 0.14));
      hullGroup.add(back);
    }
    const aft = mesh(rbox(0.6, 0.42, halfBeam * 1.4, 0.08), up);
    aft.position.set(L * 0.18, L * 0.075 + 0.26, 0);
    hullGroup.add(aft);
    const helm = mesh(rbox(0.6, 0.9, 0.6, 0.06), mats.get("hull"));
    helm.position.set(L * 0.42, L * 0.075 + 0.5, halfBeam * 0.45);
    hullGroup.add(helm);
  } else {
    buildHull(P, mats, hullGroup);
  }

  const deckY = (t) => section(P, t)[3][1] - 0.08;

  if (base.kind === "console") {
    const cx = L * 0.47;
    const consoleH = Math.min(1.25, D * 0.75);
    const cons = mesh(rbox(L * 0.09, consoleH, halfBeam * 0.7, 0.08), mats.get("hull"));
    cons.position.set(cx, deckY(0.47) + consoleH / 2, 0);
    hullGroup.add(cons);
    const ws = mesh(extrude([v2(cx + L * 0.045, deckY(0.47) + consoleH), v2(cx + L * 0.005, deckY(0.47) + consoleH + 0.42), v2(cx - L * 0.01, deckY(0.47) + consoleH + 0.42), v2(cx + L * 0.02, deckY(0.47) + consoleH)], halfBeam * 0.78, { bevel: 0.01 }), mats.get("glass"), { cast: false });
    hullGroup.add(ws);
    const lean = mesh(rbox(L * 0.06, 0.95, halfBeam * 0.8, 0.08), up);
    lean.position.set(cx - L * 0.1, deckY(0.37) + 0.48, 0);
    hullGroup.add(lean);
    // Bow U-lounge and aft bench.
    for (const s of [-1, 1]) {
      const bl = mesh(rbox(L * 0.16, 0.38, 0.5, 0.08), up);
      bl.position.set(L * 0.72, deckY(0.72) + 0.2, s * halfBeam * 0.55);
      bl.rotation.y = s * 0.18;
      hullGroup.add(bl);
    }
    const aft = mesh(rbox(L * 0.07, 0.42, halfBeam * 1.5, 0.08), up);
    aft.position.set(L * 0.08, deckY(0.08) + 0.22, 0);
    hullGroup.add(aft);
    addTop(hullGroup, mats, parts.top ?? (shape.body === "fishing" ? "ttop" : "none"), { x: cx - L * 0.02, y: deckY(0.47), w: halfBeam * 1.3, len: L * 0.18, h: consoleH + 0.95 });
    if (parts.tower) addTower(hullGroup, metal, { x: L * 0.3, y: deckY(0.3), w: halfBeam * 1.6, h: 2.2 });
    addRails(hullGroup, P, metal);
  } else if (base.kind === "yacht") {
    addSuperstructure(hullGroup, mats, { x0: L * 0.18, x1: L * 0.7, y: deckY(0.4), w: halfBeam * 1.6, h: D * 0.62, levels: 2 }, cutaway);
    // Hull windows.
    for (const s of [-1, 1]) {
      const win = mesh(rbox(L * 0.3, D * 0.1, 0.02, 0.03), mats.get("glass"), { cast: false });
      win.position.set(L * 0.42, D * 0.6, s * (halfBeam * 1.01 + 0.005));
      hullGroup.add(win);
    }
    addRails(hullGroup, P, metal);
  } else if (base.kind === "sail") {
    const trunk = mesh(rbox(L * 0.35, D * 0.35, halfBeam * 1.1, 0.12), mats.get("superstructure"));
    trunk.position.set(L * 0.5, deckY(0.5) + D * 0.17, 0);
    hullGroup.add(trunk);
    for (const s of [-1, 1]) {
      const port = mesh(rbox(L * 0.2, D * 0.08, 0.02, 0.02), mats.get("glass"), { cast: false });
      port.position.set(L * 0.5, deckY(0.5) + D * 0.22, s * halfBeam * 0.56);
      hullGroup.add(port);
    }
    const mastX = L * 0.58;
    const mastH = L * 1.35;
    const mast = mesh(cyl(0.07, 0.09, mastH, 12), metal);
    mast.position.set(mastX, deckY(0.58) + mastH / 2, 0);
    hullGroup.add(mast);
    const boomY = deckY(0.58) + 1.4;
    const boom = mesh(cyl(0.05, 0.05, L * 0.45, 10), metal);
    boom.rotation.z = Math.PI / 2;
    boom.position.set(mastX - L * 0.22, boomY, 0);
    hullGroup.add(boom);
    const sail = mats.get("sail");
    const main = new ShapeGeometry(new Shape([v2(mastX - 0.05, boomY + 0.08), v2(mastX - 0.05, deckY(0.58) + mastH - 0.3), v2(mastX - L * 0.44, boomY + 0.08)]));
    hullGroup.add(mesh(main, sail));
    const jib = new ShapeGeometry(new Shape([v2(L * 0.97, deckY(0.97) + 0.3), v2(mastX + 0.08, deckY(0.58) + mastH * 0.82), v2(mastX + 0.25, deckY(0.6) + 0.6)]));
    hullGroup.add(mesh(jib, sail));
    const fin = mesh(rbox(L * 0.08, D * 1.6, 0.12, 0.04), mats.get("hull_bottom"));
    fin.position.set(L * 0.5, -D * 0.8 + 0.1, 0);
    hullGroup.add(fin);
    const bulb = mesh(rbox(L * 0.16, D * 0.22, 0.32, 0.1), mats.get("hull_bottom"));
    bulb.position.set(L * 0.48, -D * 1.55, 0);
    hullGroup.add(bulb);
    for (const s of [-1, 1]) {
      const rudder = mesh(rbox(0.25, D * 0.9, 0.05, 0.02), mats.get("hull_bottom"));
      rudder.position.set(L * 0.06, -D * 0.3, s * halfBeam * 0.5);
      hullGroup.add(rudder);
    }
    addRails(hullGroup, P, metal);
  } else if (base.kind === "pwc") {
    const top = mesh(extrude([v2(L * 0.05, D), v2(L * 0.55, D * 1.05), v2(L * 0.85, D * 1.35), v2(L * 0.98, D * 1.0), v2(L * 0.3, D * 0.9)], halfBeam * 1.7, { bevel: 0.06 }), mats.get("hull"));
    hullGroup.add(top);
    const seat = mesh(rbox(L * 0.4, 0.14, halfBeam * 0.9, 0.06), up);
    seat.position.set(L * 0.32, D * 1.12, 0);
    hullGroup.add(seat);
    hullGroup.add(mesh(tube([[L * 0.72, D * 1.45, -0.32], [L * 0.68, D * 1.5, 0], [L * 0.72, D * 1.45, 0.32]], 0.016, 16, 6), metal));
    const post = mesh(cyl(0.04, 0.05, 0.3, 12), mats.get("chassis"));
    post.position.set(L * 0.7, D * 1.3, 0);
    hullGroup.add(post);
  }

  // Outboards / drives at the transom.
  const engines = typeof parts.engines === "number" ? parts.engines : base.kind === "console" ? 2 : base.kind === "pontoon" ? 1 : 0;
  if (engines > 0 && !shape.electric) {
    const spacing = Math.min(0.75, (halfBeam * 1.4) / engines);
    for (let i = 0; i < engines; i++) {
      const z = (i - (engines - 1) / 2) * spacing;
      const cowl = mesh(rbox(0.6, 0.75, 0.42, 0.12), mats.get("engine"));
      cowl.position.set(-0.32, (base.kind === "pontoon" ? L * 0.075 : D) + 0.35, z);
      hullGroup.add(cowl);
      const leg = mesh(rbox(0.22, 1.0, 0.12, 0.04), mats.get("engine"));
      leg.position.set(-0.28, (base.kind === "pontoon" ? L * 0.075 : D) - 0.45, z);
      hullGroup.add(leg);
    }
  } else if (shape.electric && base.kind === "console") {
    const pod = mesh(rbox(0.5, 0.5, 0.36, 0.1), mats.get("engine"));
    pod.position.set(-0.25, D * 0.7, 0);
    hullGroup.add(pod);
  }

  // Underwater lights (emissive only when the option is fitted).
  const glow = mesh(rbox(0.04, 0.05, halfBeam * 1.2, 0.02), mats.get("underlight"), { cast: false });
  glow.position.set(-0.01, D * 0.15, 0);
  hullGroup.add(glow);

  // Display cradle: the hull sits on two padded stands.
  const cradleH = base.kind === "sail" ? D * 1.7 : base.kind === "pontoon" ? 0 : 0.18;
  if (cradleH > 0) {
    for (const x of [L * 0.3, L * 0.65]) {
      const stand = mesh(rbox(0.25, cradleH, Math.max(halfBeam * 1.4, 0.6), 0.04), mats.get("chassis"));
      stand.position.set(x, cradleH / 2, 0);
      root.add(stand);
    }
    hullGroup.position.y = cradleH;
  }

  root.position.x = -L / 2;
  const anchors = {
    wheel: new Vector3(-L / 2 + 0.2, D * 0.6 + cradleH, 0),
    cabin: new Vector3(L * 0.47 - L / 2, D + cradleH + 0.8, 0),
  };
  return { group: root, cutaway, anchors, length: L };
}

function addTop(group, mats, kind, { x, y, w, len, h }) {
  if (!kind || kind === "none") return;
  const metal = mats.get("metal");
  const poles = [
    [x - len / 2, -w / 2],
    [x - len / 2, w / 2],
    [x + len / 2, -w / 2],
    [x + len / 2, w / 2],
  ];
  for (const [px, pz] of poles) {
    const p = mesh(cyl(0.025, 0.025, h, 8), metal);
    p.position.set(px, y + h / 2, pz);
    group.add(p);
  }
  if (kind === "bimini") {
    const canvas = mesh(rbox(len * 1.3, 0.05, w * 1.1, 0.025), mats.get("canopy"));
    canvas.position.set(x, y + h + 0.03, 0);
    group.add(canvas);
  } else if (kind === "ttop") {
    const slab = mesh(rbox(len * 1.25, 0.12, w * 1.05, 0.05), mats.get("canopy"));
    slab.position.set(x, y + h + 0.06, 0);
    group.add(slab);
  } else if (kind === "hardtop") {
    const roof = mesh(rbox(len * 1.6, 0.14, w * 1.15, 0.06), mats.get("canopy"));
    roof.position.set(x - len * 0.15, y + h + 0.07, 0);
    group.add(roof);
    const glass = mesh(rbox(0.04, h * 0.45, w * 1.1, 0.02), mats.get("glass"), { cast: false });
    glass.position.set(x + len * 0.62, y + h * 0.78, 0);
    group.add(glass);
  }
}

function addTower(group, metal, { x, y, w, h }) {
  group.add(mesh(tube([[x - 0.4, y, -w / 2], [x, y + h, -w / 2.6], [x, y + h + 0.1, 0], [x, y + h, w / 2.6], [x - 0.4, y, w / 2]], 0.05, 40, 10), metal));
}

function addRails(group, P, metal) {
  for (const side of [1, -1]) {
    const pts = [];
    for (let s = 0; s <= 10; s++) {
      const t = 0.62 + (s / 10) * 0.36;
      const [z, y] = section(P, t)[3];
      pts.push([t * P.L, y + 0.32, side * Math.max(0.02, z - 0.05)]);
    }
    group.add(mesh(tube(pts, 0.018, 32, 6), metal, { cast: false }));
  }
}

function addSuperstructure(root, mats, { x0, x1, y, w, h, levels }, cutaway) {
  const sup = mats.get("superstructure");
  const glass = mats.get("glass");
  let lx0 = x0;
  let lx1 = x1;
  let ly = y;
  let lw = w;
  for (let level = 0; level < levels; level++) {
    const len = lx1 - lx0;
    const outline = [v2(lx0, ly), v2(lx1, ly), v2(lx1 - len * 0.12, ly + h), v2(lx0 + len * 0.05, ly + h)];
    const deckhouse = mesh(extrude(outline, lw, { bevel: 0.08 }), sup);
    root.add(deckhouse);
    cutaway.push(deckhouse);
    const band = mesh(extrude([v2(lx0 + len * 0.03, ly + h * 0.35), v2(lx1 - len * 0.03, ly + h * 0.35), v2(lx1 - len * 0.1, ly + h * 0.8), v2(lx0 + len * 0.06, ly + h * 0.8)], lw + 0.03, { bevel: 0.02 }), glass, { cast: false });
    root.add(band);
    cutaway.push(band);
    lx0 += len * 0.12;
    lx1 -= len * 0.22;
    ly += h + 0.05;
    lw *= 0.88;
  }
  // Radar arch.
  root.add(mesh(tube([[lx0, ly, -lw / 2], [lx0 + 0.6, ly + 1.1, -lw / 3], [lx0 + 0.6, ly + 1.2, lw / 3], [lx0, ly, lw / 2]], 0.08, 32, 8), sup));
}
