// Shared geometry helpers for the procedural vehicle builders.
import {
  BoxGeometry,
  CatmullRomCurve3,
  CylinderGeometry,
  ExtrudeGeometry,
  Group,
  LatheGeometry,
  Mesh,
  Shape,
  SplineCurve,
  TorusGeometry,
  TubeGeometry,
  Vector2,
  Vector3,
} from "three";
import { RoundedBoxGeometry } from "three/examples/jsm/geometries/RoundedBoxGeometry.js";

export const v2 = (x, y) => new Vector2(x, y);
export const v3 = (x, y, z) => new Vector3(x, y, z);

export function mesh(geometry, material, { cast = true, receive = false, name } = {}) {
  const m = new Mesh(geometry, material);
  m.castShadow = cast;
  m.receiveShadow = receive;
  if (name) m.name = name;
  return m;
}

export function spline(points, divisions = 64) {
  return new SplineCurve(points.map(([x, y]) => v2(x, y))).getPoints(divisions);
}

// Extrude a closed 2D outline (x = length, y = height) across the vehicle's
// width, centred on z = 0, with rounded edges.
export function extrude(points, width, { bevel = 0.04, segments = 4 } = {}) {
  const shape = new Shape(points);
  const depth = Math.max(0.001, width - bevel * 2);
  const geo = new ExtrudeGeometry(shape, {
    depth,
    bevelEnabled: bevel > 0,
    bevelThickness: bevel,
    bevelSize: bevel * 0.85,
    bevelSegments: segments,
    curveSegments: 6,
  });
  geo.translate(0, 0, -depth / 2);
  geo.computeVertexNormals();
  return geo;
}

export function rbox(w, h, d, r = 0.02, seg = 3) {
  return new RoundedBoxGeometry(w, h, d, seg, Math.min(r, w / 2 - 1e-4, h / 2 - 1e-4, d / 2 - 1e-4));
}

export function box(w, h, d) {
  return new BoxGeometry(w, h, d);
}

export function cyl(rTop, rBottom, h, seg = 24, open = false) {
  return new CylinderGeometry(rTop, rBottom, h, seg, 1, open);
}

export function tube(points, radius, segments = 48, radial = 10) {
  const curve = new CatmullRomCurve3(points.map(([x, y, z]) => v3(x, y, z)));
  return new TubeGeometry(curve, segments, radius, radial, false);
}

// Clip the top of a closed outline at height y (used for roof panels): returns
// the part of the outline above y, closed with a straight line.
export function clipAbove(points, y) {
  const out = [];
  for (let i = 0; i < points.length - 1; i++) {
    const a = points[i];
    const b = points[i + 1];
    if (a.y >= y) out.push(a.clone());
    if ((a.y < y) !== (b.y < y)) {
      const t = (y - a.y) / (b.y - a.y);
      out.push(v2(a.x + (b.x - a.x) * t, y));
    }
  }
  if (points[points.length - 1].y >= y) out.push(points[points.length - 1].clone());
  return out;
}

// A thin band following a polyline (stripes, accent lines), returned as an outline.
export function band(points, thickness) {
  const top = points.map((p) => v2(p.x, p.y + thickness));
  return [...points.map((p) => p.clone()), ...top.reverse()];
}

// ---------------------------------------------------------------------------
// Car wheel: lathed tyre, rim barrel, styled spokes, brake disc and caliper.
// Built facing +z (outer face); mirror for the other side.
// ---------------------------------------------------------------------------
export function carWheel({ R = 0.34, width = 0.26, spokes = 5, style = "twin", size = 19, centerlock = false }, mats) {
  const g = new Group();
  const rr = R * Math.min(0.8, 0.6 + (size - 18) * 0.035); // rim radius grows with wheel size
  const w = width;

  const tyreProfile = [
    v2(rr - 0.005, -w / 2 + 0.01),
    v2(rr + 0.02, -w / 2),
    v2(R - 0.035, -w / 2),
    v2(R - 0.008, -w / 2 + 0.03),
    v2(R, -w / 4),
    v2(R, w / 4),
    v2(R - 0.008, w / 2 - 0.03),
    v2(R - 0.035, w / 2),
    v2(rr + 0.02, w / 2),
    v2(rr - 0.005, w / 2 - 0.01),
  ];
  const tyre = new LatheGeometry(tyreProfile, 56);
  tyre.rotateX(Math.PI / 2);
  g.add(mesh(tyre, mats.get("tyre")));

  const barrel = cyl(rr, rr, w * 0.92, 48, true);
  barrel.rotateX(Math.PI / 2);
  g.add(mesh(barrel, mats.get("rims"), { cast: false }));

  const lip = new TorusGeometry(rr - 0.004, 0.011, 8, 56);
  lip.translate(0, 0, w * 0.44);
  g.add(mesh(lip, mats.get("rims"), { cast: false }));

  // Brake disc and caliper sit behind the spokes.
  const disc = cyl(rr * 0.8, rr * 0.8, 0.028, 48);
  disc.rotateX(Math.PI / 2);
  disc.translate(0, 0, -0.02);
  g.add(mesh(disc, mats.get("disc"), { cast: false }));
  const caliper = rbox(0.11, rr * 0.55, 0.08, 0.02);
  caliper.translate(0, rr * 0.62, 0.035);
  caliper.rotateZ(Math.PI * 0.22);
  g.add(mesh(caliper, mats.get("calipers"), { cast: false }));

  const face = w * 0.36;
  const spokeLen = rr * 0.88;
  const rims = mats.get("rims");
  const addSpoke = (angle, thickness, len = spokeLen, startR = 0.05, twist = 0) => {
    const geo = box(len, thickness, 0.028);
    geo.translate(startR + len / 2, 0, 0);
    if (twist) geo.rotateX(twist);
    geo.rotateZ(angle);
    geo.translate(0, 0, face);
    g.add(mesh(geo, rims, { cast: false }));
  };

  const n = Math.max(3, spokes);
  for (let i = 0; i < n; i++) {
    const a = (i / n) * Math.PI * 2;
    switch (style) {
      case "twin":
        addSpoke(a - 0.07, 0.03);
        addSpoke(a + 0.07, 0.03);
        break;
      case "split":
      case "y": {
        const spread = style === "y" ? 0.22 : 0.12;
        addSpoke(a, 0.05, spokeLen * 0.45);
        addSpoke(a - spread, 0.03, spokeLen * 0.55, spokeLen * 0.45);
        addSpoke(a + spread, 0.03, spokeLen * 0.55, spokeLen * 0.45);
        break;
      }
      case "turbine":
        addSpoke(a, 0.045, spokeLen, 0.05, 0.5);
        break;
      case "thin":
        addSpoke(a, 0.022);
        break;
      default:
        addSpoke(a, 0.045);
    }
  }

  const hub = cyl(0.075, 0.085, 0.05, 24);
  hub.rotateX(Math.PI / 2);
  hub.translate(0, 0, face + 0.005);
  g.add(mesh(hub, rims, { cast: false }));
  if (centerlock) {
    const nut = cyl(0.05, 0.05, 0.05, 6);
    nut.rotateX(Math.PI / 2);
    nut.translate(0, 0, face + 0.04);
    g.add(mesh(nut, mats.get("calipers"), { cast: false }));
  }
  return g;
}

// ---------------------------------------------------------------------------
// Motorcycle wheel: torus tyre, rim ring, spokes, disc.
// ---------------------------------------------------------------------------
export function bikeWheel({ R = 0.31, tyreW = 0.06, spokes = 5, style = "cast", discs = 1 }, mats) {
  const g = new Group();
  const tyre = new TorusGeometry(R - tyreW, tyreW, 16, 56);
  g.add(mesh(tyre, mats.get("tyre")));
  const rimR = R - tyreW * 1.9;
  const rim = new TorusGeometry(rimR, 0.014, 8, 56);
  g.add(mesh(rim, mats.get("rims"), { cast: false }));

  const rims = mats.get("rims");
  if (style === "wire") {
    for (let i = 0; i < spokes; i++) {
      const a = (i / spokes) * Math.PI * 2;
      const geo = cyl(0.0025, 0.0025, rimR - 0.04, 4);
      geo.translate(0, (rimR - 0.04) / 2 + 0.04, 0);
      geo.rotateX((i % 2 ? 1 : -1) * 0.08);
      geo.rotateZ(a);
      g.add(mesh(geo, rims, { cast: false }));
    }
  } else {
    const thick = style === "carbon" ? 0.05 : style === "forged" ? 0.022 : 0.032;
    for (let i = 0; i < spokes; i++) {
      const a = (i / spokes) * Math.PI * 2;
      const geo = box(thick, rimR - 0.04, 0.02);
      geo.translate(0, (rimR - 0.04) / 2 + 0.04, 0);
      geo.rotateZ(a);
      g.add(mesh(geo, rims, { cast: false }));
    }
  }
  const hub = cyl(0.045, 0.045, 0.1, 16);
  hub.rotateX(Math.PI / 2);
  g.add(mesh(hub, rims, { cast: false }));

  for (let d = 0; d < discs; d++) {
    const disc = cyl(rimR * 0.62, rimR * 0.62, 0.006, 32);
    disc.rotateX(Math.PI / 2);
    disc.translate(0, 0, (d === 0 ? 1 : -1) * 0.055);
    g.add(mesh(disc, mats.get("disc"), { cast: false }));
  }
  return g;
}

export function disposeObject(root) {
  root?.traverse?.((o) => {
    if (o.geometry) o.geometry.dispose();
  });
}
