// Procedural car builder. Each body style is a side silhouette (lower body +
// greenhouse) that gets extruded across the width, with wheel arches cut into
// the sill line. Every surface is assigned a material *role* so catalog options
// (paint, rims, calipers, interior, roof, ...) restyle it live.
import { Group, TorusGeometry, Vector3 } from "three";
import { band, box, carWheel, clipAbove, cyl, extrude, mesh, rbox, spline, v2 } from "./common.js";

// x runs rear (0) -> front (L); y is height above ground.
const BODIES = {
  coupe: {
    L: 4.52, W: 1.85, R: 0.345, axles: [1.13, 3.58], clear: 0.12, cabinW: 0.8,
    top: [[4.46, 0.2], [4.52, 0.4], [4.42, 0.6], [4.05, 0.71], [3.55, 0.77], [3.12, 0.82], [2.4, 0.87], [1.55, 0.91], [1.0, 0.95], [0.45, 0.88], [0.08, 0.74], [0, 0.5], [0.05, 0.22]],
    cabin: [[3.2, 0.74], [2.45, 1.27], [1.95, 1.3], [1.45, 1.18], [0.9, 0.95], [0.5, 0.82]],
    exhaust: "dual",
  },
  cabriolet: {
    L: 4.52, W: 1.85, R: 0.345, axles: [1.13, 3.58], clear: 0.12, cabinW: 0.8, convertible: true,
    top: [[4.46, 0.2], [4.52, 0.4], [4.42, 0.6], [4.05, 0.71], [3.55, 0.77], [3.12, 0.82], [2.4, 0.86], [1.55, 0.89], [1.0, 0.93], [0.45, 0.88], [0.08, 0.74], [0, 0.5], [0.05, 0.22]],
    cabin: [[3.2, 0.74], [2.5, 1.24], [1.95, 1.26], [1.2, 1.08], [0.72, 0.86]],
    exhaust: "dual",
  },
  supercar: {
    L: 4.7, W: 2.0, R: 0.35, axles: [1.12, 3.82], clear: 0.1, cabinW: 0.74,
    top: [[4.66, 0.16], [4.7, 0.32], [4.55, 0.48], [4.1, 0.6], [3.5, 0.68], [3.0, 0.76], [2.2, 0.84], [1.4, 0.92], [0.7, 0.95], [0.2, 0.86], [0, 0.6], [0.03, 0.2]],
    cabin: [[3.4, 0.64], [2.7, 1.08], [2.2, 1.13], [1.6, 1.05], [0.9, 0.93], [0.45, 0.88]],
    exhaust: "center", spoiler: "ducktail",
  },
  sedan: {
    L: 4.98, W: 1.95, R: 0.36, axles: [1.1, 4.05], clear: 0.13, cabinW: 0.82,
    top: [[4.93, 0.22], [4.98, 0.44], [4.86, 0.66], [4.4, 0.76], [3.75, 0.82], [3.38, 0.86], [2.5, 0.92], [1.5, 0.96], [0.8, 0.98], [0.25, 0.92], [0.03, 0.76], [0, 0.5], [0.05, 0.24]],
    cabin: [[3.45, 0.82], [2.75, 1.38], [1.8, 1.42], [1.15, 1.3], [0.55, 0.98], [0.38, 0.92]],
    exhaust: "quad",
  },
  wagon: {
    L: 4.98, W: 1.95, R: 0.36, axles: [1.1, 4.05], clear: 0.13, cabinW: 0.82,
    top: [[4.93, 0.22], [4.98, 0.44], [4.86, 0.66], [4.4, 0.76], [3.75, 0.82], [3.38, 0.86], [2.5, 0.92], [1.5, 0.96], [0.8, 0.98], [0.2, 0.97], [0.04, 0.86], [0, 0.5], [0.05, 0.24]],
    cabin: [[3.45, 0.82], [2.75, 1.38], [1.4, 1.43], [0.45, 1.37], [0.12, 1.1], [0.08, 0.92]],
    exhaust: "dual",
  },
  hatchback: {
    L: 4.25, W: 1.8, R: 0.33, axles: [0.8, 3.38], clear: 0.14, cabinW: 0.82,
    top: [[4.2, 0.24], [4.25, 0.46], [4.12, 0.7], [3.75, 0.8], [3.3, 0.88], [2.6, 0.95], [1.5, 0.98], [0.6, 1.0], [0.1, 0.98], [0, 0.7], [0.04, 0.26]],
    cabin: [[3.35, 0.84], [2.65, 1.4], [1.2, 1.46], [0.3, 1.42], [0.07, 1.1], [0.04, 0.95]],
    exhaust: "dual",
  },
  suv: {
    L: 4.93, W: 2.0, R: 0.4, axles: [1.0, 3.9], clear: 0.21, cabinW: 0.84,
    top: [[4.86, 0.32], [4.93, 0.58], [4.82, 0.92], [4.3, 1.04], [3.7, 1.1], [3.35, 1.14], [2.2, 1.18], [1.0, 1.2], [0.25, 1.16], [0.04, 1.0], [0, 0.62], [0.06, 0.34]],
    cabin: [[3.42, 1.08], [2.75, 1.64], [1.4, 1.7], [0.5, 1.66], [0.12, 1.3], [0.08, 1.1]],
    exhaust: "dual",
  },
  offroad: {
    L: 4.75, W: 2.0, R: 0.42, axles: [0.95, 3.85], clear: 0.25, cabinW: 0.88,
    top: [[4.7, 0.36], [4.75, 0.7], [4.73, 1.06], [4.55, 1.12], [3.55, 1.16], [2.5, 1.2], [1.0, 1.22], [0.12, 1.21], [0.01, 1.1], [0, 0.42], [0.05, 0.36]],
    cabin: [[3.55, 1.12], [3.28, 1.84], [3.1, 1.9], [0.25, 1.9], [0.06, 1.84], [0.04, 1.14]],
    exhaust: "dual", spare: true, boxy: true,
  },
  van: {
    L: 5.15, W: 2.0, R: 0.37, axles: [1.0, 4.15], clear: 0.15, cabinW: 0.88,
    top: [[5.1, 0.26], [5.15, 0.5], [5.05, 0.8], [4.7, 0.92], [4.4, 1.0], [3.6, 1.05], [2.0, 1.08], [0.5, 1.08], [0.06, 1.04], [0, 0.6], [0.05, 0.28]],
    cabin: [[4.48, 0.96], [3.6, 1.78], [3.0, 1.86], [0.4, 1.86], [0.08, 1.75], [0.05, 1.06]],
    exhaust: "single",
  },
  pickup: {
    L: 5.6, W: 2.05, R: 0.42, axles: [1.25, 4.75], clear: 0.26, cabinW: 0.9,
    top: [[5.55, 0.38], [5.6, 0.7], [5.56, 1.06], [5.0, 1.16], [4.2, 1.22], [3.6, 1.24], [2.6, 1.26], [1.0, 1.26], [0.1, 1.25], [0, 1.0], [0, 0.45], [0.06, 0.38]],
    cabin: [[3.78, 1.18], [3.2, 1.92], [2.9, 1.96], [2.25, 1.96], [2.15, 1.86], [2.12, 1.2]],
    exhaust: "single", bed: [0.08, 2.05],
  },
};

export function carBodies() {
  return Object.keys(BODIES);
}

export function buildCar(shape = {}, parts = {}, mats) {
  const base = BODIES[shape.body] ?? BODIES.coupe;
  const scale = shape.length ? shape.length / base.L : 1;
  const P = { ...base, L: base.L * scale, W: shape.width ?? base.W };
  const sx = (pts) => pts.map(([x, y]) => [x * scale, y]);
  const root = new Group();
  const body = new Group(); // everything that rides on the suspension
  root.add(body);
  const cutaway = [];
  const electric = !!shape.electric;

  const ride = parts.ride === "lowered" ? -0.02 : parts.ride === "lifted" ? 0.05 : 0;
  body.position.y = ride;
  const axles = base.axles.map((x) => x * scale);
  const bevel = 0.045;

  // ---- Lower body with wheel arches ---------------------------------------
  const topPts = spline(sx(base.top), 90);
  const archR = P.R + 0.045;
  const bottom = [];
  const [xr, xf] = axles;
  const archPts = (cx) => {
    const pts = [v2(cx - archR, P.clear), v2(cx - archR, P.R)];
    for (let i = 1; i < 16; i++) {
      const a = Math.PI - (i / 16) * Math.PI;
      pts.push(v2(cx + Math.cos(a) * archR, P.R + Math.sin(a) * archR));
    }
    pts.push(v2(cx + archR, P.R), v2(cx + archR, P.clear));
    return pts;
  };
  bottom.push(v2(topPts[topPts.length - 1].x + 0.06, P.clear));
  bottom.push(...archPts(xr));
  bottom.push(...archPts(xf));
  bottom.push(v2(topPts[0].x - 0.06, P.clear));
  const outline = [...topPts, ...bottom];
  body.add(mesh(extrude(outline, P.W, { bevel, segments: 5 }), mats.get("paint"), { name: "body" }));

  // ---- Greenhouse ----------------------------------------------------------
  const cabinPts = spline(sx(base.cabin), 60);
  const cabinW = P.W * base.cabinW;
  const topPart = parts.top === "open" ? "open" : "closed";
  const roofTopY = Math.max(...cabinPts.map((p) => p.y));

  if (base.convertible) {
    if (topPart === "closed") {
      const soft = mesh(extrude([...cabinPts], cabinW, { bevel: 0.05 }), mats.get("softtop"), { name: "softtop" });
      body.add(soft);
      cutaway.push(soft);
      const side = mesh(extrude(insetOutline(cabinPts, 0.78), cabinW + 0.012, { bevel: 0.01 }), mats.get("glass"), { cast: false });
      body.add(side);
      cutaway.push(side);
    } else {
      // Open: raked windscreen frame only.
      const ws = [cabinPts[0], v2(cabinPts[0].x - 0.52, cabinPts[0].y + 0.42), v2(cabinPts[0].x - 0.6, cabinPts[0].y + 0.4), v2(cabinPts[0].x - 0.12, cabinPts[0].y - 0.02)];
      body.add(mesh(extrude(ws, cabinW, { bevel: 0.015 }), mats.get("glass"), { cast: false }));
    }
  } else {
    const glass = mesh(extrude(cabinPts, cabinW, { bevel: 0.05 }), mats.get("glass"), { cast: false, name: "glass" });
    body.add(glass);
    cutaway.push(glass);

    // Roof panel: the top of the greenhouse in paint / contrast / carbon / glass.
    const clipY = roofTopY - (base.boxy ? 0.05 : 0.075);
    const roofOutline = clipAbove(cabinPts, clipY);
    if (roofOutline.length > 2) {
      const roofMat =
        parts.roof === "glass" ? mats.get("glass")
        : parts.roof === "contrast" ? mats.get("roof_contrast")
        : parts.roof === "carbon" ? mats.get("roof_carbon")
        : mats.get("roof");
      const roof = mesh(extrude(roofOutline, cabinW + 0.014, { bevel: 0.03 }), roofMat, { name: "roof" });
      roof.position.y = 0.006;
      body.add(roof);
      cutaway.push(roof);
    }
    // Pillars: thin painted strips at windscreen base and C-pillar.
    const pillar = (from, to) => {
      const len = Math.hypot(to.x - from.x, to.y - from.y);
      const g = rbox(len, 0.06, cabinW + 0.02, 0.02);
      const m = mesh(g, mats.get("roof"));
      m.position.set((from.x + to.x) / 2, (from.y + to.y) / 2, 0);
      m.rotation.z = Math.atan2(to.y - from.y, to.x - from.x);
      body.add(m);
      cutaway.push(m);
    };
    const roofFront = roofOutline[0] ?? cabinPts[0];
    pillar(cabinPts[0], roofFront);
    const roofRear = roofOutline[roofOutline.length - 1] ?? cabinPts[cabinPts.length - 1];
    if (!base.boxy) pillar(roofRear, cabinPts[cabinPts.length - 1]);
  }

  // ---- Interior (visible through the glass, and in the cut-away view) -----
  const cabinFront = cabinPts[0].x;
  const cabinRear = cabinPts[cabinPts.length - 1].x;
  const beltY = Math.min(cabinPts[0].y, cabinPts[cabinPts.length - 1].y);
  const seatX = cabinFront - Math.min(1.05, (cabinFront - cabinRear) * 0.45);
  const floorY = beltY - 0.38;
  const seatZ = cabinW * 0.24;
  const interior = mats.get("interior");
  const accent = mats.get("interior_accent");
  for (const z of [-seatZ, seatZ]) {
    const cushion = mesh(rbox(0.5, 0.12, 0.48, 0.05), interior, { cast: false });
    cushion.position.set(seatX, floorY + 0.06, z);
    body.add(cushion);
    const back = mesh(rbox(0.12, 0.62, 0.48, 0.05), interior, { cast: false });
    back.position.set(seatX - 0.28, floorY + 0.36, z);
    back.rotation.z = -0.22;
    body.add(back);
    const bolster = mesh(rbox(0.13, 0.5, 0.06, 0.03), accent, { cast: false });
    for (const dz of [-0.22, 0.22]) {
      const b = bolster.clone();
      b.position.set(seatX - 0.27, floorY + 0.34, z + dz);
      b.rotation.z = -0.22;
      body.add(b);
    }
    const head = mesh(rbox(0.1, 0.18, 0.28, 0.04), interior, { cast: false });
    head.position.set(seatX - 0.38, floorY + 0.74, z);
    head.rotation.z = -0.22;
    body.add(head);
  }
  if (cabinFront - cabinRear > 2.0) {
    // Rear bench for four/five-seaters.
    const bench = mesh(rbox(0.5, 0.14, cabinW * 0.86, 0.05), interior, { cast: false });
    bench.position.set(seatX - 0.95, floorY + 0.08, 0);
    body.add(bench);
    const bback = mesh(rbox(0.12, 0.55, cabinW * 0.86, 0.05), interior, { cast: false });
    bback.position.set(seatX - 1.22, floorY + 0.36, 0);
    bback.rotation.z = -0.2;
    body.add(bback);
  }
  const dash = mesh(rbox(0.45, 0.16, cabinW * 0.94, 0.05), interior, { cast: false });
  dash.position.set(cabinFront - 0.22, beltY - 0.05, 0);
  body.add(dash);
  const dashTrim = mesh(box(0.04, 0.035, cabinW * 0.9), mats.get("interior_trim"), { cast: false });
  dashTrim.position.set(cabinFront - 0.45, beltY - 0.06, 0);
  body.add(dashTrim);
  const console = mesh(rbox(0.9, 0.16, 0.2, 0.04), mats.get("interior_trim"), { cast: false });
  console.position.set(seatX + 0.15, floorY + 0.1, 0);
  body.add(console);
  const wheelRing = mesh(new TorusGeometry(0.17, 0.02, 8, 32), interior, { cast: false });
  wheelRing.rotation.y = Math.PI / 2;
  wheelRing.rotation.x = 0.35;
  wheelRing.position.set(cabinFront - 0.6, beltY + 0.02, seatZ);
  body.add(wheelRing);

  // ---- Lights, grille, trims ----------------------------------------------
  const front = topPts[0].x;
  const edge = bevel * 0.85;
  const noseY = sampleY(topPts, front - 0.18) - 0.07;
  const lightX = edgeX(topPts, noseY, 1) + edge;
  for (const s of [-1, 1]) {
    const housing = mesh(rbox(0.2, 0.08, 0.36, 0.035), mats.get("lights"), { cast: false });
    housing.position.set(lightX - 0.07, noseY, s * (P.W / 2 - 0.28));
    housing.rotation.z = -0.35;
    body.add(housing);
    const lamp = mesh(rbox(0.04, 0.025, 0.3, 0.01), mats.get("lamp"), { cast: false });
    lamp.position.set(lightX + 0.005, noseY - 0.005, s * (P.W / 2 - 0.28));
    body.add(lamp);
  }
  const grilleY = P.clear + 0.16;
  const grille = mesh(rbox(0.1, 0.17, P.W * 0.5, 0.03), mats.get("chassis"), { cast: false });
  grille.position.set(edgeX(topPts, grilleY, 1) + edge - 0.035, grilleY, 0);
  body.add(grille);

  const rearX = topPts[topPts.length - 1].x;
  const tailY = sampleY(topPts, 0.2 * scale) - 0.12;
  const rearEdge = (y) => edgeX(topPts, y, -1) - edge;
  const tail = mesh(rbox(0.05, 0.045, P.W - 0.2, 0.02), mats.get("taillight"), { cast: false });
  tail.position.set(rearEdge(tailY) + 0.012, tailY, 0);
  body.add(tail);
  const diffY = P.clear + 0.07;
  const diffuser = mesh(rbox(0.12, 0.12, P.W * 0.72, 0.03), mats.get("trim"), { cast: false });
  diffuser.position.set(rearEdge(diffY) + 0.04, diffY, 0);
  body.add(diffuser);
  for (const s of [-1, 1]) {
    const skirt = mesh(rbox(xf - xr - archR * 2 - 0.1, 0.07, 0.05, 0.02), mats.get("trim"), { cast: false });
    skirt.position.set((xr + xf) / 2, P.clear + 0.05, s * (P.W / 2 + 0.005));
    body.add(skirt);
    const mirror = mesh(rbox(0.14, 0.09, 0.2, 0.04), mats.get("trim"));
    mirror.position.set(cabinFront - 0.12, beltY + 0.08, s * (cabinW / 2 + 0.14));
    body.add(mirror);
  }

  // ---- Exhaust -------------------------------------------------------------
  if (!electric) {
    const style = typeof parts.exhaust === "string" ? parts.exhaust : base.exhaust;
    const zs = style === "quad" ? [-0.55, -0.42, 0.42, 0.55] : style === "center" ? [-0.08, 0.08] : style === "single" ? [0.55] : [-0.5, 0.5];
    for (const z of zs) {
      const pipe = cyl(0.045, 0.045, 0.14, 20, true);
      pipe.rotateZ(Math.PI / 2);
      const m = mesh(pipe, mats.get("exhaust"), { cast: false });
      const y = style === "center" ? tailY - 0.14 : P.clear + 0.1;
      m.position.set(rearEdge(y) + 0.02, y, z * (P.W / 1.85));
      body.add(m);
    }
  }

  // ---- Optional parts ------------------------------------------------------
  const spoiler = parts.spoiler ?? base.spoiler;
  if (spoiler === "wing") {
    const deckY = sampleY(topPts, 0.35 * scale);
    for (const z of [-0.45, 0.45]) {
      const post = mesh(box(0.05, 0.2, 0.03), mats.get("trim"));
      post.position.set(0.32 * scale, deckY + 0.1, z);
      body.add(post);
    }
    const blade = mesh(rbox(0.32, 0.035, P.W * 0.92, 0.015), mats.get("trim"));
    blade.position.set(0.28 * scale, deckY + 0.22, 0);
    blade.rotation.z = 0.08;
    body.add(blade);
  } else if (spoiler === "ducktail") {
    const deckY = sampleY(topPts, 0.25 * scale);
    const lip = mesh(rbox(0.22, 0.05, P.W * 0.8, 0.02), mats.get("paint"));
    lip.position.set(0.22 * scale, deckY + 0.02, 0);
    lip.rotation.z = 0.25;
    body.add(lip);
  }

  if (parts.stripes) {
    const roofOutline = base.convertible ? [] : clipAbove(cabinPts, roofTopY - 0.075);
    const segments = [
      topPts.filter((p) => p.x > cabinFront + 0.05 && p.x < front - 0.12),
      roofOutline.filter((p) => p.y > roofTopY - 0.03),
      topPts.filter((p) => p.x > rearX + 0.15 && p.x < cabinRear - 0.05),
    ];
    for (const seg of segments) {
      if (seg.length < 2) continue;
      const lift = seg === segments[1] ? 0.042 : bevel * 0.85;
      const pts = seg.map((p) => v2(p.x, p.y + lift));
      for (const z of [-0.09, 0.09]) {
        const m = mesh(extrude(band(pts, 0.006), 0.1, { bevel: 0 }), mats.get("stripes"), { cast: false });
        m.position.z = z;
        body.add(m);
      }
    }
  }

  if (parts.roofrack && !base.convertible) {
    const rackY = roofTopY + 0.07;
    const r0 = cabinPts.find((p) => p.y > roofTopY - 0.02)?.x ?? cabinFront - 0.6;
    const r1 = [...cabinPts].reverse().find((p) => p.y > roofTopY - 0.02)?.x ?? cabinRear + 0.6;
    for (const z of [-cabinW / 2 + 0.06, cabinW / 2 - 0.06]) {
      const rail = mesh(rbox(Math.abs(r0 - r1), 0.03, 0.035, 0.012), mats.get("metal"));
      rail.position.set((r0 + r1) / 2, rackY, z);
      body.add(rail);
    }
    for (const x of [r1 + (r0 - r1) * 0.25, r1 + (r0 - r1) * 0.75]) {
      const bar = mesh(rbox(0.05, 0.025, cabinW, 0.01), mats.get("chassis"));
      bar.position.set(x, rackY + 0.02, 0);
      body.add(bar);
    }
  }

  if (parts.lightbar && !base.convertible) {
    const bar = mesh(rbox(0.08, 0.06, cabinW * 0.8, 0.02), mats.get("chassis"));
    const x = cabinPts.find((p) => p.y > roofTopY - 0.03)?.x ?? cabinFront - 0.5;
    bar.position.set(x - 0.05, roofTopY + 0.08, 0);
    body.add(bar);
    const lamp = mesh(box(0.01, 0.035, cabinW * 0.76), mats.get("lamp"), { cast: false });
    lamp.position.set(x - 0.005, roofTopY + 0.08, 0);
    body.add(lamp);
  }

  if (parts.steps) {
    for (const s of [-1, 1]) {
      const step = mesh(rbox(xf - xr - archR * 2 - 0.2, 0.04, 0.16, 0.015), mats.get("trim"));
      step.position.set((xr + xf) / 2, P.clear + 0.02, s * (P.W / 2 + 0.06));
      body.add(step);
    }
  }

  if (base.bed) {
    const [b0, b1] = base.bed.map((x) => x * scale);
    const bedTop = sampleY(topPts, (b0 + b1) / 2);
    const inset = mesh(box(b1 - b0, 0.004, P.W - 0.2), mats.get("chassis"), { cast: false, receive: true });
    inset.position.set((b0 + b1) / 2, bedTop + bevel * 0.85 + 0.002, 0);
    body.add(inset);
    if (parts.bedcover) {
      const cover = mesh(rbox(b1 - b0, 0.04, P.W - 0.08, 0.015), mats.get("trim"));
      cover.position.set((b0 + b1) / 2, bedTop + bevel + 0.02, 0);
      body.add(cover);
    }
  }

  if (base.spare) {
    const spare = carWheel({ R: P.R * 0.92, width: 0.24, spokes: 6, style: "classic", size: 17 }, mats);
    spare.rotation.y = -Math.PI / 2;
    spare.position.set(rearEdge(0.95) - 0.13, 0.95, 0);
    body.add(spare);
  }

  // ---- Wheels ----------------------------------------------------------------
  const wheelSpec = typeof parts.wheel === "object" && parts.wheel ? parts.wheel : {};
  const tyreW = Math.min(0.3, 0.2 + P.W * 0.03);
  const template = carWheel({ R: P.R, width: tyreW, size: 19, ...wheelSpec }, mats);
  const wheelZ = P.W / 2 - tyreW / 2 - 0.01;
  const anchors = { wheel: new Vector3() };
  for (const x of axles) {
    for (const s of [1, -1]) {
      const w = template.clone();
      w.position.set(x, P.R, s * wheelZ);
      if (s < 0) w.rotation.y = Math.PI;
      root.add(w);
    }
  }
  anchors.wheel.set(xf, P.R, wheelZ);
  anchors.cabin = new Vector3(seatX, floorY + 0.4, 0);

  // Centre the model on the origin along its length.
  root.position.x = -P.L / 2;
  anchors.wheel.x -= P.L / 2;
  anchors.cabin.x -= P.L / 2;
  return { group: root, cutaway, anchors, length: P.L };
}

// Outermost x of the outline near height y: dir 1 = front face, -1 = rear face.
function edgeX(points, y, dir) {
  let best = null;
  for (const p of points) {
    if (Math.abs(p.y - y) > 0.12) continue;
    if (best === null || (dir > 0 ? p.x > best : p.x < best)) best = p.x;
  }
  if (best !== null) return best;
  return dir > 0 ? Math.max(...points.map((p) => p.x)) : Math.min(...points.map((p) => p.x));
}

// Shrink an outline toward its centroid (side windows inside a soft top).
function insetOutline(points, k) {
  const cx = points.reduce((a, p) => a + p.x, 0) / points.length;
  const cy = points.reduce((a, p) => a + p.y, 0) / points.length;
  return points.map((p) => v2(cx + (p.x - cx) * k, cy + (p.y - cy) * k));
}

function sampleY(points, x) {
  let best = points[0];
  for (const p of points) if (Math.abs(p.x - x) < Math.abs(best.x - x)) best = p;
  return best.y;
}
