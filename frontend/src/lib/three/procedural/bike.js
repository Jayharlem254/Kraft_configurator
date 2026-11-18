// Procedural motorcycle builder. x runs from the rear axle (0) to the front
// axle (wheelbase); the model is centred afterwards.
import { Group, SphereGeometry, Vector3 } from "three";
import { bikeWheel, cyl, extrude, mesh, rbox, tube, v2 } from "./common.js";

const BIKES = {
  sport: { WB: 1.42, Rf: 0.31, Rr: 0.31, tf: 0.055, tr: 0.085, head: [1.2, 0.9], seat: 0.84, rake: 0.42, fairing: "full", tank: [0.82, 0.86], exhaust: "under" },
  naked: { WB: 1.43, Rf: 0.31, Rr: 0.31, tf: 0.055, tr: 0.085, head: [1.2, 0.93], seat: 0.84, rake: 0.42, fairing: "none", tank: [0.82, 0.88], exhaust: "side", bars: "wide" },
  cruiser: { WB: 1.7, Rf: 0.34, Rr: 0.33, tf: 0.06, tr: 0.1, head: [1.4, 0.92], seat: 0.68, rake: 0.58, fairing: "none", tank: [1.0, 0.86], exhaust: "long", bars: "pullback", fenders: true, low: true },
  touring: { WB: 1.6, Rf: 0.32, Rr: 0.32, tf: 0.06, tr: 0.09, head: [1.32, 0.98], seat: 0.76, rake: 0.46, fairing: "touring", tank: [0.95, 0.9], exhaust: "side", luggage: true, screen: "tall" },
  adventure: { WB: 1.56, Rf: 0.36, Rr: 0.33, tf: 0.05, tr: 0.075, head: [1.28, 1.05], seat: 0.86, rake: 0.46, fairing: "beak", tank: [0.88, 0.97], exhaust: "high", crashbars: true },
  motocross: { WB: 1.48, Rf: 0.36, Rr: 0.33, tf: 0.045, tr: 0.06, head: [1.25, 1.02], seat: 0.96, rake: 0.46, fairing: "mx", tank: [0.88, 0.95], exhaust: "high", fenders: "mx", nolights: true },
  scooter: { WB: 1.35, Rf: 0.23, Rr: 0.23, tf: 0.055, tr: 0.06, head: [1.12, 0.92], seat: 0.79, rake: 0.42, fairing: "scooter", tank: null, exhaust: "scooter" },
  cafe: { WB: 1.42, Rf: 0.32, Rr: 0.32, tf: 0.055, tr: 0.075, head: [1.18, 0.9], seat: 0.8, rake: 0.42, fairing: "none", tank: [0.82, 0.86], exhaust: "twin", bars: "clipon", cowl: true },
};

export function buildBike(shape = {}, parts = {}, mats) {
  const B = BIKES[shape.body] ?? BIKES.naked;
  const electric = !!shape.electric;
  const root = new Group();
  const wheelSpec = typeof parts.wheel === "object" && parts.wheel ? parts.wheel : {};
  const style = wheelSpec.style ?? (shape.body === "motocross" || shape.body === "adventure" ? "wire" : "cast");
  const spokes = wheelSpec.spokes ?? (style === "wire" ? 36 : 5);

  // Wheels.
  const front = bikeWheel({ R: B.Rf, tyreW: B.tf, spokes, style, discs: 2 }, mats);
  front.position.set(B.WB, B.Rf, 0);
  root.add(front);
  const rear = bikeWheel({ R: B.Rr, tyreW: B.tr, spokes, style, discs: 1 }, mats);
  rear.position.set(0, B.Rr, 0);
  root.add(rear);

  const [hx, hy] = B.head;
  const pivot = [0.55, B.low ? 0.38 : 0.45];
  const frameMat = mats.get("frame");
  const paint = mats.get("paint");

  // Fork: two stanchions from the steering head to the front axle.
  for (const z of [-0.09, 0.09]) {
    root.add(mesh(tube([[hx, hy + 0.06, z], [B.WB, B.Rf, z]], 0.024, 8, 8), mats.get("metal")));
  }
  root.add(mesh(rbox(0.12, 0.05, 0.26, 0.02), frameMat).translateX(hx).translateY(hy + 0.06));

  // Frame: twin spars (or a trellis/cradle) from head to swingarm pivot.
  for (const z of [-0.11, 0.11]) {
    if (B.fairing === "scooter") {
      root.add(mesh(tube([[hx, hy - 0.05, 0], [hx - 0.25, 0.35, 0], [0.55, 0.3, 0]], 0.035, 24, 10), frameMat));
      break;
    }
    root.add(mesh(tube([[hx - 0.02, hy, z * 0.6], [hx - 0.35, hy - 0.08, z], [pivot[0] + 0.1, pivot[1] + 0.25, z], [pivot[0], pivot[1], z * 0.9]], 0.03, 32, 10), frameMat));
    if (B.low || B.fairing === "none") {
      root.add(mesh(tube([[hx - 0.02, hy - 0.05, z * 0.4], [hx - 0.2, 0.32, z * 0.7], [pivot[0], pivot[1] - 0.05, z * 0.9]], 0.022, 24, 8), frameMat));
    }
    // Swingarm.
    root.add(mesh(tube([[pivot[0], pivot[1], z], [0.25, (pivot[1] + B.Rr) / 2, z], [0, B.Rr, z * 0.9]], 0.028, 16, 8), frameMat));
    // Subframe to the seat.
    root.add(mesh(tube([[pivot[0] + 0.05, pivot[1] + 0.25, z * 0.8], [0.25, B.seat - 0.05, z * 0.6], [0.05, B.seat + 0.02, z * 0.4]], 0.016, 16, 6), frameMat));
  }

  // Engine or battery.
  if (B.fairing !== "scooter") {
    if (electric) {
      const pack = mesh(rbox(0.52, 0.34, 0.3, 0.04), mats.get("engine"));
      pack.position.set(0.8, 0.46, 0);
      root.add(pack);
      const motor = mesh(cyl(0.11, 0.11, 0.2, 24), mats.get("metal"));
      motor.rotation.x = Math.PI / 2;
      motor.position.set(0.55, 0.33, 0);
      root.add(motor);
    } else {
      const block = mesh(rbox(0.46, 0.34, 0.34, 0.05), mats.get("engine"));
      block.position.set(0.78, B.low ? 0.4 : 0.46, 0);
      root.add(block);
      const heads = mesh(rbox(0.22, 0.22, 0.38, 0.04), mats.get("engine"));
      heads.position.set(0.95, B.low ? 0.6 : 0.66, 0);
      heads.rotation.z = -0.35;
      root.add(heads);
      const cover = mesh(cyl(0.11, 0.11, 0.04, 24), mats.get("metal"));
      cover.rotation.x = Math.PI / 2;
      cover.position.set(0.72, B.low ? 0.38 : 0.44, 0.19);
      root.add(cover);
    }
  }

  // Tank with an accent band.
  if (B.tank) {
    const tank = new SphereGeometry(1, 32, 20);
    tank.scale(B.low ? 0.32 : 0.28, B.cowl ? 0.11 : 0.13, 0.17);
    const t = mesh(tank, paint, { name: "tank" });
    t.position.set(B.tank[0], B.tank[1], 0);
    root.add(t);
    const stripe = new SphereGeometry(1, 32, 10, 0, Math.PI * 2, Math.PI * 0.32, Math.PI * 0.12);
    stripe.scale(B.low ? 0.325 : 0.285, B.cowl ? 0.115 : 0.135, 0.175);
    const s = mesh(stripe, mats.get("accent"), { cast: false });
    s.position.copy(t.position);
    s.rotation.x = Math.PI / 2;
    root.add(s);
  }

  // Seat + tail.
  const seatLen = B.cowl ? 0.34 : B.fairing === "scooter" ? 0.62 : B.low ? 0.4 : 0.5;
  const seat = mesh(rbox(seatLen, 0.08, B.fairing === "touring" ? 0.34 : 0.26, 0.04), mats.get("seat"));
  seat.position.set(B.fairing === "scooter" ? 0.38 : B.tank ? B.tank[0] - 0.33 - seatLen / 2 + 0.12 : 0.4, B.seat, 0);
  root.add(seat);
  if (B.fairing === "touring") {
    const pillion = mesh(rbox(0.3, 0.1, 0.34, 0.04), mats.get("seat"));
    pillion.position.set(seat.position.x - 0.32, B.seat + 0.06, 0);
    root.add(pillion);
  }
  if (B.fairing !== "scooter") {
    const tail = mesh(extrude([v2(0.45, B.seat - 0.06), v2(0.3, B.seat - 0.02), v2(-0.05, B.seat + 0.06), v2(-0.12, B.seat + 0.02), v2(0.05, B.seat - 0.12), v2(0.35, B.seat - 0.14)], 0.2, { bevel: 0.03 }), paint);
    if (B.cowl) tail.scale.set(0.8, 1.3, 1);
    root.add(tail);
    if (!B.nolights) {
      const tl = mesh(rbox(0.03, 0.03, 0.14, 0.01), mats.get("taillight"), { cast: false });
      tl.position.set(-0.13, B.seat + 0.01, 0);
      root.add(tl);
    }
  }

  // Bodywork per style.
  if (B.fairing === "full" || B.fairing === "touring") {
    const tall = B.fairing === "touring";
    // Pointed nose fairing, narrow enough to read as bodywork rather than a box.
    const outline = tall
      ? [v2(hx + 0.3, 0.62), v2(hx + 0.32, 0.8), v2(hx + 0.18, hy + 0.12), v2(hx - 0.1, hy + 0.16), v2(hx - 0.3, hy - 0.04), v2(hx - 0.42, 0.48), v2(hx - 0.2, 0.4), v2(hx + 0.12, 0.46)]
      : [v2(hx + 0.34, 0.66), v2(hx + 0.3, 0.78), v2(hx + 0.12, hy + 0.06), v2(hx - 0.12, hy + 0.08), v2(hx - 0.26, hy - 0.08), v2(hx - 0.36, 0.5), v2(hx - 0.22, 0.38), v2(hx + 0.08, 0.5)];
    root.add(mesh(extrude(outline, tall ? 0.52 : 0.34, { bevel: 0.08, segments: 6 }), paint, { name: "fairing" }));
    for (const z of [-1, 1]) {
      const vent = mesh(rbox(0.22, 0.04, 0.02, 0.01), mats.get("chassis"), { cast: false });
      vent.position.set(hx - 0.12, 0.62, z * (tall ? 0.27 : 0.18));
      root.add(vent);
    }
    const screenTall = parts.screen === "tall" || B.screen === "tall";
    const screen = mesh(extrude([v2(hx + 0.24, hy + 0.12), v2(hx + 0.02, hy + (screenTall ? 0.48 : 0.3)), v2(hx - 0.1, hy + (screenTall ? 0.44 : 0.26)), v2(hx - 0.02, hy + 0.14)], tall ? 0.5 : 0.36, { bevel: 0.01 }), mats.get("glass"), { cast: false });
    root.add(screen);
    if (!B.nolights) addHeadlight(root, mats, hx + 0.36, 0.74, tall ? 0.4 : 0.26);
  } else if (B.fairing === "beak") {
    const beak = mesh(extrude([v2(B.WB + 0.12, B.Rf + 0.28), v2(hx + 0.12, hy - 0.02), v2(hx + 0.02, hy - 0.08), v2(B.WB - 0.08, B.Rf + 0.22)], 0.16, { bevel: 0.02 }), paint);
    root.add(beak);
    const shroud = mesh(extrude([v2(hx + 0.22, hy - 0.2), v2(hx + 0.12, hy + 0.12), v2(hx - 0.1, hy + 0.1), v2(hx - 0.3, hy - 0.2), v2(hx - 0.1, hy - 0.35)], 0.5, { bevel: 0.04 }), paint);
    root.add(shroud);
    const screen = mesh(extrude([v2(hx + 0.12, hy + 0.12), v2(hx - 0.02, hy + (parts.screen === "tall" ? 0.5 : 0.36)), v2(hx - 0.12, hy + (parts.screen === "tall" ? 0.46 : 0.32)), v2(hx - 0.06, hy + 0.1)], 0.34, { bevel: 0.01 }), mats.get("glass"), { cast: false });
    root.add(screen);
    addHeadlight(root, mats, hx + 0.22, hy - 0.08, 0.2);
  } else if (B.fairing === "mx") {
    const fender = mesh(extrude([v2(B.WB + 0.28, B.Rf + 0.2), v2(B.WB - 0.25, B.Rf + 0.22), v2(B.WB - 0.2, B.Rf + 0.18), v2(B.WB + 0.25, B.Rf + 0.16)], 0.14, { bevel: 0.02 }), paint);
    root.add(fender);
    const plate = mesh(rbox(0.06, 0.22, 0.24, 0.03), paint);
    plate.position.set(hx + 0.08, hy + 0.02, 0);
    plate.rotation.z = -0.4;
    root.add(plate);
    for (const z of [-0.14, 0.14]) {
      const side = mesh(rbox(0.32, 0.18, 0.02, 0.03), paint);
      side.position.set(0.3, B.seat - 0.12, z);
      root.add(side);
    }
    const rf = mesh(extrude([v2(0.25, B.seat - 0.06), v2(-0.35, B.seat + 0.02), v2(-0.36, B.seat - 0.02), v2(0.2, B.seat - 0.12)], 0.14, { bevel: 0.02 }), paint);
    root.add(rf);
  } else if (B.fairing === "scooter") {
    const shield = mesh(extrude([v2(hx + 0.12, 0.35), v2(hx + 0.16, hy + 0.05), v2(hx - 0.02, hy + 0.12), v2(hx - 0.12, 0.42), v2(hx - 0.22, 0.32)], 0.5, { bevel: 0.06 }), paint, { name: "shield" });
    root.add(shield);
    const floor = mesh(rbox(0.6, 0.06, 0.38, 0.03), mats.get("chassis"));
    floor.position.set(0.72, 0.32, 0);
    root.add(floor);
    const rearBody = mesh(extrude([v2(0.68, 0.34), v2(0.6, B.seat - 0.06), v2(0.0, B.seat - 0.02), v2(-0.2, B.seat - 0.12), v2(-0.1, 0.42), v2(0.25, 0.3)], 0.42, { bevel: 0.07 }), paint);
    root.add(rearBody);
    addHeadlight(root, mats, hx + 0.16, hy - 0.05, 0.2);
    const tl = mesh(rbox(0.03, 0.04, 0.22, 0.01), mats.get("taillight"), { cast: false });
    tl.position.set(-0.22, B.seat - 0.1, 0);
    root.add(tl);
  } else if (!B.nolights) {
    // Round headlight for naked / cruiser / café.
    const lamp = mesh(new SphereGeometry(0.1, 24, 16, 0, Math.PI), mats.get("lamp"), { cast: false });
    lamp.rotation.y = Math.PI / 2;
    lamp.position.set(hx + 0.14, hy - 0.04, 0);
    root.add(lamp);
    const bucket = mesh(new SphereGeometry(0.11, 24, 16, Math.PI, Math.PI), shape.body === "naked" ? paint : mats.get("metal"));
    bucket.rotation.y = Math.PI / 2;
    bucket.position.copy(lamp.position);
    root.add(bucket);
  }

  if (B.fenders === true) {
    root.add(mesh(extrude(arcBand(B.WB, B.Rf, B.Rf + 0.04, B.Rf + 0.08, 0.15, 1.75), 0.17, { bevel: 0.02 }), paint));
    root.add(mesh(extrude(arcBand(0, B.Rr, B.Rr + 0.04, B.Rr + 0.09, 0.9, 2.95), 0.25, { bevel: 0.02 }), paint));
  }

  // Handlebars.
  const barY = hy + (B.bars === "pullback" ? 0.24 : B.bars === "clipon" ? 0.02 : 0.14);
  const barX = hx - (B.bars === "pullback" ? 0.2 : 0.06);
  const width = B.bars === "wide" || B.bars === "pullback" ? 0.4 : B.bars === "clipon" ? 0.3 : 0.36;
  root.add(mesh(tube([[barX, barY, -width], [hx - 0.02, hy + 0.08, -0.08], [hx - 0.02, hy + 0.08, 0.08], [barX, barY, width]], 0.014, 24, 8), mats.get("metal")));
  for (const z of [-width, width]) {
    const grip = mesh(cyl(0.02, 0.02, 0.11, 12), mats.get("chassis"));
    grip.rotation.x = Math.PI / 2;
    grip.position.set(barX, barY, z + Math.sign(z) * 0.04);
    root.add(grip);
  }

  // Exhaust.
  if (!electric) {
    const ex = mats.get("exhaust");
    if (B.exhaust === "under") {
      root.add(mesh(tube([[0.95, 0.55, 0.12], [0.85, 0.24, 0.12], [0.55, 0.2, 0.08]], 0.03, 16, 8), ex));
      const can = mesh(rbox(0.3, 0.12, 0.16, 0.04), ex);
      can.position.set(0.42, 0.24, 0.08);
      root.add(can);
    } else if (B.exhaust === "long") {
      for (const [y, z] of [[0.32, 0.2], [0.42, 0.22]]) {
        root.add(mesh(tube([[0.95, 0.55, 0.16], [0.85, y, z], [0.2, y - 0.04, z], [-0.25, y - 0.02, z]], 0.035, 24, 10), ex));
      }
    } else if (B.exhaust === "high") {
      root.add(mesh(tube([[0.98, 0.6, 0.1], [0.8, 0.45, 0.17], [0.4, 0.62, 0.17], [0.05, B.seat - 0.06, 0.16]], 0.03, 24, 8), ex));
      const can = mesh(cyl(0.055, 0.05, 0.36, 20), ex);
      can.rotation.z = Math.PI / 2 - 0.25;
      can.position.set(0.12, B.seat - 0.08, 0.17);
      root.add(can);
    } else if (B.exhaust === "twin") {
      for (const z of [-0.18, 0.18]) {
        root.add(mesh(tube([[0.95, 0.55, z * 0.5], [0.85, 0.3, z], [0.3, 0.33, z]], 0.028, 16, 8), ex));
        const meg = mesh(cyl(0.05, 0.035, 0.4, 20), ex);
        meg.rotation.z = Math.PI / 2 - 0.12;
        meg.position.set(0.12, 0.36, z);
        root.add(meg);
      }
    } else if (B.exhaust === "scooter") {
      const can = mesh(cyl(0.05, 0.05, 0.36, 16), ex);
      can.rotation.z = Math.PI / 2 - 0.1;
      can.position.set(0.15, 0.28, 0.17);
      root.add(can);
    } else {
      root.add(mesh(tube([[0.95, 0.55, 0.12], [0.85, 0.3, 0.16], [0.45, 0.34, 0.17]], 0.03, 16, 8), ex));
      const can = mesh(cyl(0.055, 0.05, 0.34, 20), ex);
      can.rotation.z = Math.PI / 2 - 0.25;
      can.position.set(0.22, 0.44, 0.17);
      root.add(can);
    }
  }

  if (B.crashbars) {
    for (const z of [-0.2, 0.2]) {
      root.add(mesh(tube([[hx - 0.25, hy - 0.15, z * 0.6], [hx - 0.25, 0.42, z], [0.75, 0.3, z], [0.62, 0.45, z * 0.6]], 0.014, 16, 6), mats.get("metal")));
    }
  }

  if (parts.luggage || B.luggage) {
    for (const z of [-0.27, 0.27]) {
      const p = mesh(rbox(0.5, 0.36, 0.2, 0.05), mats.get("trim"));
      p.position.set(0.08, B.seat - 0.18, z);
      root.add(p);
    }
    const top = mesh(rbox(0.38, 0.3, 0.42, 0.06), mats.get("trim"));
    top.position.set(-0.08, B.seat + 0.22, 0);
    root.add(top);
  }

  const L = B.WB + B.Rf + B.Rr;
  root.position.x = -B.WB / 2;
  return {
    group: root,
    cutaway: [],
    anchors: { wheel: new Vector3(B.WB / 2, B.Rf, 0.12), cabin: new Vector3(-B.WB / 2 + 0.4, B.seat, 0) },
    length: L,
  };
}

// Curved band around (cx, cy) between radii r1..r2 and angles a0..a1 (radians).
function arcBand(cx, cy, r1, r2, a0, a1, n = 18) {
  const outer = [];
  const inner = [];
  for (let i = 0; i <= n; i++) {
    const a = a0 + ((a1 - a0) * i) / n;
    outer.push(v2(cx + Math.cos(a) * r2, cy + Math.sin(a) * r2));
    inner.push(v2(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1));
  }
  return [...outer, ...inner.reverse()];
}

function addHeadlight(root, mats, x, y, w) {
  const lamp = mesh(rbox(0.04, 0.05, w, 0.015), mats.get("lamp"), { cast: false });
  lamp.position.set(x, y, 0);
  root.add(lamp);
}
