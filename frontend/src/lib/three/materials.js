// Role-based PBR material library.
//
// Every visible surface belongs to a *role* (paint, rims, calipers, interior,
// hull, ...). Options in the catalog address roles, never mesh names, so the
// same option data drives procedural models and uploaded GLBs alike. Each role
// owns one material instance shared by all meshes using it, so recolouring is
// a uniform update — no scene traversal, no shader recompiles.
import {
  CanvasTexture,
  Color,
  DoubleSide,
  MeshPhysicalMaterial,
  MeshStandardMaterial,
  RepeatWrapping,
  SRGBColorSpace,
} from "three";

// Finish presets. Values tuned for ACES tone mapping + RoomEnvironment IBL.
export const FINISHES = {
  solid: { metalness: 0.0, roughness: 0.3, clearcoat: 1, clearcoatRoughness: 0.04 },
  gloss: { metalness: 0.0, roughness: 0.25, clearcoat: 1, clearcoatRoughness: 0.03 },
  metallic: { metalness: 0.62, roughness: 0.32, clearcoat: 1, clearcoatRoughness: 0.03 },
  pearl: { metalness: 0.35, roughness: 0.28, clearcoat: 1, clearcoatRoughness: 0.03, iridescence: 0.6, iridescenceIOR: 1.5 },
  matte: { metalness: 0.22, roughness: 0.62, clearcoat: 0, clearcoatRoughness: 0.5 },
  satin: { metalness: 0.45, roughness: 0.45, clearcoat: 0.25, clearcoatRoughness: 0.35 },
  chrome: { metalness: 1.0, roughness: 0.05, clearcoat: 0.6, clearcoatRoughness: 0.02 },
  titanium: { metalness: 1.0, roughness: 0.3, clearcoat: 0.3, clearcoatRoughness: 0.2 },
  carbon: { metalness: 0.15, roughness: 0.35, clearcoat: 1, clearcoatRoughness: 0.05, texture: "carbon" },
  suede: { metalness: 0.0, roughness: 0.95, clearcoat: 0, sheen: 1, sheenRoughness: 0.8 },
  leather: { metalness: 0.0, roughness: 0.55, clearcoat: 0.1, clearcoatRoughness: 0.4, sheen: 0.4, sheenRoughness: 0.5 },
  fabric: { metalness: 0.0, roughness: 0.9, clearcoat: 0, sheen: 0.7, sheenRoughness: 0.9 },
  rubber: { metalness: 0.0, roughness: 0.82, clearcoat: 0 },
  plastic: { metalness: 0.0, roughness: 0.55, clearcoat: 0 },
};

// Default look per role, before any option touches it.
export const ROLE_DEFAULTS = {
  paint: { color: "#eeeeea", finish: "solid" },
  roof: { color: "#eeeeea", finish: "solid", match: "paint" },
  roof_contrast: { color: "#0b0b0c", finish: "gloss" },
  roof_carbon: { color: "#1b1c1f", finish: "carbon" },
  trim: { color: "#121315", finish: "gloss" },
  stripes: { color: "#f2f2ef", finish: "gloss" },
  rims: { color: "#c9ccd1", finish: "metallic" },
  calipers: { color: "#141517", finish: "gloss" },
  disc: { color: "#8b8e93", finish: "titanium" },
  tyre: { color: "#18191b", finish: "rubber" },
  chassis: { color: "#16171a", finish: "plastic" },
  glass: { color: "#0a0d11", opacity: 0.62 },
  lights: { color: "#d9dde3", finish: "chrome" },
  lamp: { color: "#ffffff", emissive: "#f2f6ff", emissiveIntensity: 2.2 },
  taillight: { color: "#7a0008", emissive: "#ff1a22", emissiveIntensity: 1.6 },
  exhaust: { color: "#c8cbd0", finish: "chrome" },
  interior: { color: "#141416", finish: "leather" },
  interior_accent: { color: "#141416", finish: "leather", match: "interior" },
  interior_trim: { color: "#9aa0a6", finish: "metallic" },
  softtop: { color: "#141516", finish: "fabric" },
  frame: { color: "#141516", finish: "satin" },
  engine: { color: "#2a2b2e", finish: "satin" },
  metal: { color: "#c8cbd0", finish: "chrome" },
  seat: { color: "#151517", finish: "leather" },
  accent: { color: "#121315", finish: "gloss", match: "paint" },
  hull: { color: "#f1f2f0", finish: "gloss" },
  hull_bottom: { color: "#202328", finish: "satin" },
  hull_accent: { color: "#c4c8cc", finish: "metallic" },
  deck: { color: "#a8743f", finish: "satin" },
  upholstery: { color: "#e9e4d8", finish: "leather" },
  canopy: { color: "#e9e9e6", finish: "gloss" },
  superstructure: { color: "#f3f4f2", finish: "gloss" },
  sail: { color: "#f4f2ea", finish: "fabric", side: "double" },
  underlight: { color: "#000000", emissive: "#000000", emissiveIntensity: 0 },
};

let carbonTexture = null;
function getCarbonTexture() {
  if (carbonTexture) return carbonTexture;
  const s = 64;
  const c = document.createElement("canvas");
  c.width = c.height = s;
  const g = c.getContext("2d");
  const cell = s / 4;
  for (let y = 0; y < 4; y++) {
    for (let x = 0; x < 4; x++) {
      const horiz = (x + y) % 2 === 0;
      const grad = horiz ? g.createLinearGradient(x * cell, 0, x * cell + cell, 0) : g.createLinearGradient(0, y * cell, 0, y * cell + cell);
      grad.addColorStop(0, "#3a3a3e");
      grad.addColorStop(0.5, "#8c8c92");
      grad.addColorStop(1, "#3a3a3e");
      g.fillStyle = grad;
      g.fillRect(x * cell, y * cell, cell, cell);
    }
  }
  carbonTexture = new CanvasTexture(c);
  carbonTexture.wrapS = carbonTexture.wrapT = RepeatWrapping;
  carbonTexture.repeat.set(24, 24);
  carbonTexture.colorSpace = SRGBColorSpace;
  return carbonTexture;
}

function makeMaterial(role) {
  const d = ROLE_DEFAULTS[role] ?? { color: "#888888", finish: "plastic" };
  if (role === "glass") {
    return new MeshPhysicalMaterial({
      name: "role:glass",
      color: d.color,
      metalness: 0,
      roughness: 0.04,
      transparent: true,
      opacity: d.opacity,
      envMapIntensity: 0.9,
      clearcoat: 1,
      clearcoatRoughness: 0,
      depthWrite: false,
    });
  }
  if (d.emissive !== undefined) {
    return new MeshStandardMaterial({
      name: `role:${role}`,
      color: d.color,
      emissive: d.emissive,
      emissiveIntensity: d.emissiveIntensity,
      roughness: 0.3,
    });
  }
  const m = new MeshPhysicalMaterial({ name: `role:${role}`, color: d.color });
  if (d.side === "double") m.side = DoubleSide;
  return m;
}

export class MaterialLibrary {
  constructor() {
    this.materials = new Map(); // role -> material
    this.extra = new Map(); // role -> Set(materials) (GLB-imported materials bound to a role)
    this.state = {}; // role -> last applied spec
  }

  get(role) {
    if (!this.materials.has(role)) {
      const m = makeMaterial(role);
      this.materials.set(role, m);
      this.apply(role, ROLE_DEFAULTS[role] ?? {}, m);
    }
    return this.materials.get(role);
  }

  // Bind an imported (GLB) material to a role so role updates reach it too.
  bind(role, material) {
    if (!this.extra.has(role)) this.extra.set(role, new Set());
    this.extra.get(role).add(material);
    if (this.state[role]) this.applyTo(material, this.state[role]);
  }

  // Apply a full role map ({paint: {color, finish}, ...}), filling unspecified
  // roles with defaults so deselecting an option restores the base look.
  applyAll(roles) {
    const all = new Set([...this.materials.keys(), ...this.extra.keys(), ...Object.keys(roles)]);
    const resolved = {};
    for (const role of all) resolved[role] = { ...(ROLE_DEFAULTS[role] ?? {}), ...(roles[role] ?? {}) };
    // "match" copies another role (interior trim in body colour, contrast roof
    // following paint unless overridden, ...).
    for (const role of all) {
      const spec = resolved[role];
      const explicit = roles[role];
      const match = explicit?.match ?? (explicit?.color ? null : spec.match);
      if (match && resolved[match]) resolved[role] = { ...resolved[match], ...(explicit?.opacity ? { opacity: explicit.opacity } : {}) };
    }
    for (const role of all) this.apply(role, resolved[role]);
  }

  apply(role, spec, only) {
    this.state[role] = spec;
    const targets = only ? [only] : [this.materials.get(role), ...(this.extra.get(role) ?? [])].filter(Boolean);
    for (const m of targets) this.applyTo(m, spec);
  }

  applyTo(m, spec) {
    // "Original finish": imported materials go back to exactly what the artist
    // made (textures included); procedural ones use the detected colour/finish.
    const orig = m.userData?.original;
    if (spec.original && orig) {
      if (orig.color) m.color.copy(orig.color);
      if (m.map !== orig.map) {
        m.map = orig.map;
        m.needsUpdate = true;
      }
      for (const k of ["metalness", "roughness", "clearcoat", "clearcoatRoughness", "iridescence", "sheen"]) if (k in m && orig[k] !== undefined) m[k] = orig[k];
      return;
    }
    if (orig && m.userData.dropMapOnRestyle && m.map) {
      m.map = null;
      m.needsUpdate = true;
    }
    if (spec.color && m.color) m.color.set(spec.color);
    if (m.name === "role:glass" || spec.opacity !== undefined) {
      if (spec.opacity !== undefined) {
        m.opacity = spec.opacity;
        m.transparent = spec.opacity < 1;
      }
      return;
    }
    if (spec.emissive !== undefined && m.emissive) {
      m.emissive.set(spec.emissive);
      m.emissiveIntensity = spec.emissiveIntensity ?? 1;
    }
    const f = FINISHES[spec.finish];
    if (!f) return;
    m.metalness = f.metalness;
    m.roughness = f.roughness;
    if ("clearcoat" in m) {
      m.clearcoat = f.clearcoat ?? 0;
      m.clearcoatRoughness = f.clearcoatRoughness ?? 0.1;
      m.iridescence = f.iridescence ?? 0;
      if (f.iridescenceIOR) m.iridescenceIOR = f.iridescenceIOR;
      m.sheen = f.sheen ?? 0;
      if (f.sheen) {
        m.sheenRoughness = f.sheenRoughness ?? 0.5;
        m.sheenColor = new Color(spec.color ?? "#ffffff").lerp(new Color("#ffffff"), 0.3);
      }
    }
    // Only touch maps on materials we created; imported ones keep their textures.
    if (m.name?.startsWith("role:") || (m.userData?.dropMapOnRestyle && (f.texture === "carbon" || m.map === carbonTexture))) {
      const wantCarbon = f.texture === "carbon";
      const hasCarbon = m.map === carbonTexture && !!carbonTexture;
      if (wantCarbon !== hasCarbon) {
        m.map = wantCarbon ? getCarbonTexture() : null;
        m.needsUpdate = true;
      }
    }
  }

  dispose() {
    for (const m of this.materials.values()) m.dispose();
    this.materials.clear();
    this.extra.clear();
  }
}

// Turn a resolved option list into a visual state:
//   roles: { paint: {color, finish}, ... }  parts: { spoiler: "wing", wheel: {...}, ... }
// Later categories win, mirroring catalog order.
export function visualFromResolved(vehicle, resolvedCodes) {
  const roles = {};
  const parts = {};
  if (!vehicle?.option_categories) return { roles, parts };
  for (const cat of vehicle.option_categories) {
    for (const choice of cat.choices) {
      if (!resolvedCodes.has(choice.code)) continue;
      for (const v of choice.visual ?? []) {
        if (v.role) roles[v.role] = { ...(roles[v.role] ?? {}), ...stripRole(v) };
        if (v.part) parts[v.part] = v.value;
      }
    }
  }
  return { roles, parts };
}

function stripRole(v) {
  const { role, target, ...rest } = v;
  return rest;
}
