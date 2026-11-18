// Model-range thumbnails rendered from the same procedural engine, through a
// single hidden Stage (one WebGL context for the whole gallery), queued so
// cards fill in progressively, and cached in memory + localStorage.
import { hashString } from "../format.js";
import { Stage } from "./Stage.js";

const VERSION = "v3";
const memory = new Map();
let stage = null;
let host = null;
let queue = Promise.resolve();

const PALETTES = {
  car: [
    ["#b3121b", "solid"], ["#eeeeea", "solid"], ["#1b2c4f", "metallic"], ["#4a4d52", "metallic"],
    ["#f0c20c", "solid"], ["#1f4a3a", "metallic"], ["#0b0c0e", "solid"], ["#b9bdc2", "metallic"], ["#7a4a2c", "metallic"],
  ],
  bike: [["#c1121f", "solid"], ["#0d0e10", "solid"], ["#7ac70c", "solid"], ["#1f3f9e", "metallic"], ["#ecebe6", "pearl"]],
  boat: [["#f1f2f0", "gloss"], ["#16294d", "metallic"], ["#5b6168", "metallic"], ["#0b0c0e", "gloss"]],
};

export function thumbKey(v) {
  return `kraft-thumb-${VERSION}:${v.slug}:${hashString(JSON.stringify(v.shape ?? {}) + v.vehicle_type)}`;
}

export function storeSnapshot(v, dataUrl) {
  const key = v.model_file ? `kraft-thumb-glb:${v.slug}:${v.model_file}` : thumbKey(v);
  memory.set(key, dataUrl);
  try {
    localStorage.setItem(key, dataUrl);
  } catch {
    /* quota exceeded: memory cache only */
  }
}

export function cachedThumbnail(v) {
  const keys = v.model_file ? [`kraft-thumb-glb:${v.slug}:${v.model_file}`] : [thumbKey(v)];
  for (const key of keys) {
    if (memory.has(key)) return memory.get(key);
    try {
      const hit = localStorage.getItem(key);
      if (hit) {
        memory.set(key, hit);
        return hit;
      }
    } catch {
      /* storage unavailable */
    }
  }
  return null;
}

function defaultVisual(v) {
  const type = v.vehicle_type ?? "car";
  const palette = PALETTES[type] ?? PALETTES.car;
  const [color, finish] = palette[hashString(v.slug) % palette.length];
  const role = type === "boat" ? "hull" : "paint";
  return { roles: { [role]: { color, finish } }, parts: {} };
}

function ensureStage() {
  if (stage) return stage;
  host = document.createElement("div");
  host.setAttribute("aria-hidden", "true");
  Object.assign(host.style, { position: "fixed", left: "-10000px", top: "0", width: "960px", height: "600px", pointerEvents: "none" });
  document.body.appendChild(host);
  stage = new Stage(host, { interactive: false });
  stage.view = "front34";
  return stage;
}

export function renderThumbnail(v) {
  const cached = cachedThumbnail(v);
  if (cached) return Promise.resolve(cached);
  const job = queue.then(async () => {
    const again = cachedThumbnail(v);
    if (again) return again;
    const s = ensureStage();
    const kind = v.vehicle_type ?? v.shape?.kind ?? "car";
    // Uploaded GLBs are too heavy to load for a card; they get a procedural
    // stand-in until the configurator captures a real snapshot.
    const shape = proceduralShape(v);
    await s.setVehicle({ kind, shape }, defaultVisual(v));
    await new Promise((r) => requestAnimationFrame(r));
    const url = s.snapshot({ width: 960, height: 600, type: "image/webp", quality: 0.86, view: "front34" });
    if (!v.model_file) storeSnapshot(v, url);
    else memory.set(`kraft-thumb-glb:${v.slug}:${v.model_file}`, url);
    return url;
  });
  queue = job.catch(() => null);
  return job;
}

// Procedural shape for a vehicle: its own `shape`, or a stand-in derived from
// its type and body class (uploaded vehicles have no shape).
export function proceduralShape(v) {
  if (v.shape && Object.keys(v.shape).length) return v.shape;
  return { kind: v.vehicle_type ?? "car", body: fallbackBody(v) };
}

function fallbackBody(v) {
  if (v.vehicle_type === "bike") return v.body_class || "naked";
  if (v.vehicle_type === "boat") return v.body_class || "speedboat";
  const map = { convertible: "cabriolet", electric: "sedan" };
  return map[v.body_class] ?? (v.body_class || "supercar");
}
