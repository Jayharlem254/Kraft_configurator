// GLB/GLTF ingestion: decode (Draco + Meshopt), arrange and analyse the model
// (analyze.js), and bind materials to configurator roles by name heuristics plus
// geometry (overridable per vehicle through `material_map`).
import { MeshPhysicalMaterial } from "three";
import { DRACOLoader } from "three/examples/jsm/loaders/DRACOLoader.js";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";
import { MeshoptDecoder } from "three/examples/jsm/libs/meshopt_decoder.module.js";
import { analyzeModel, describeFinish } from "./analyze.js";

let loader = null;
function getLoader() {
  if (loader) return loader;
  const draco = new DRACOLoader();
  draco.setDecoderPath("/draco/");
  loader = new GLTFLoader();
  loader.setDRACOLoader(draco);
  loader.setMeshoptDecoder(MeshoptDecoder);
  return loader;
}

// Parsed GLTFs are cached by URL so returning to a model is instant; each use
// gets a cloned scene graph (geometry is shared, materials are re-bound).
const cache = new Map();

export function loadGLTF(url, onProgress) {
  if (!cache.has(url)) {
    const p = new Promise((resolve, reject) => {
      getLoader().load(
        url,
        resolve,
        (e) => e.total && onProgress?.(e.loaded / e.total),
        (err) => {
          cache.delete(url);
          reject(err);
        },
      );
    });
    cache.set(url, p);
  }
  return cache.get(url);
}

const ROLE_PATTERNS = [
  ["glass", /(glass|window|windshield|windscreen|vitre|verre|scheibe)/],
  ["tyre", /(tyre|tire|pneu|rubber|reifen)/],
  ["calipers", /(caliper|calliper|etrier|bremssattel|brake_?cal)/],
  ["disc", /(brake_?disc|rotor|disque|frein|plaquette)/],
  ["rims", /(rim|jante|felge|wheel(?!.*(arch|well)))/],
  ["interior", /(seat|leather|interior|cuir|siege|sitz|upholster)/],
  ["paint", /(car_?paint|carpaint|body_?paint|paint|carros|carross|body(?!.*(glass|chrome|trim)))/],
  ["hull", /(hull|coque|rumpf)/],
  ["deck", /(deck|teak|pont)/],
  ["frame", /(frame|chassis_?tube|cadre)/],
  ["seat", /(saddle|selle)/],
  ["exhaust", /(exhaust|muffler|silencer|echappement)/],
];

const NOT_PAINT = /(black|dark|trim|chrome|plastic|rubber|carbon|crbn|grill|grille|light|lamp|led|mirror|logo|badge|emblem|metal|steel|alu)/;

function toPhysical(m) {
  if (m.isMeshPhysicalMaterial) return m.clone();
  const p = new MeshPhysicalMaterial();
  for (const key of ["name", "map", "normalMap", "normalScale", "roughnessMap", "metalnessMap", "aoMap", "aoMapIntensity", "emissive", "emissiveMap", "emissiveIntensity", "alphaMap", "transparent", "opacity", "side", "envMapIntensity"]) {
    if (m[key] !== undefined && m[key] !== null) p[key] = m[key]?.clone && !m[key].isTexture ? m[key].clone() : m[key];
  }
  if (m.color) p.color.copy(m.color);
  p.metalness = m.metalness ?? 0;
  p.roughness = m.roughness ?? 0.5;
  return p;
}

// Prepare a loaded GLTF for the stage: arrange + analyse it (analyze.js), then
// bind materials to roles. Name matches (and explicit `material_map` overrides)
// win; the geometric analysis fills every role the names leave open.
// Returns { group, roles, boundRoles, length, analysis, anchors }.
export function prepareGLTF(gltf, { kind = "car", length, materialMap = {} } = {}, mats) {
  const scene = gltf.scene.clone(true);
  const { group, analysis, materialRoles } = analyzeModel(scene, { kind, length });

  // Explicit overrides first: material_map = { paint: ["MAT_Body", ...], ... }.
  const overrides = new Map();
  for (const [role, names] of Object.entries(materialMap ?? {})) {
    for (const n of [].concat(names)) overrides.set(String(n).toLowerCase(), role);
  }

  const paintRole = kind === "boat" ? "hull" : "paint";
  const converted = new Map(); // original material -> physical clone
  const named = new Map(); // physical clone -> role from names
  const geometric = new Map(); // physical clone -> role from geometry
  group.traverse((node) => {
    if (!node.isMesh) return;
    node.castShadow = true;
    node.receiveShadow = true;
    const list = Array.isArray(node.material) ? node.material : [node.material];
    const next = list.map((orig) => {
      if (!converted.has(orig)) {
        const phys = toPhysical(orig);
        converted.set(orig, phys);
        const names = [orig.name, node.name, node.parent?.name].filter(Boolean).map((x) => x.toLowerCase());
        let role = null;
        for (const n of names) if (overrides.has(n)) role = overrides.get(n);
        if (!role) {
          // The material's own name is the strongest signal; mesh/parent names
          // only decide when it says nothing, and never turn an obviously
          // non-paint material (black trim, chrome, rubber, lights) into paint.
          const matName = (orig.name ?? "").toLowerCase();
          role = ROLE_PATTERNS.find(([, re]) => re.test(matName))?.[0] ?? null;
          if (!role && !NOT_PAINT.test(matName)) {
            const hay = names.slice(1).join(" ");
            role = ROLE_PATTERNS.find(([, re]) => re.test(hay))?.[0] ?? null;
          }
        }
        if (role) named.set(phys, role);
        const geo = materialRoles.get(orig);
        if (geo) geometric.set(phys, geo);
      }
      return converted.get(orig);
    });
    node.material = Array.isArray(node.material) ? next : next[0];
  });

  // Merge: names first; geometry for unnamed materials, and for single-owner
  // roles (paint/hull, interior) only when names found nothing for that role.
  const roleOf = new Map(named);
  const namedRoles = new Set(named.values());
  for (const [m, role] of geometric) {
    if (roleOf.has(m)) continue;
    if ((role === paintRole || role === "interior") && namedRoles.has(role)) continue;
    roleOf.set(m, role);
  }

  // Roles we restyle. Glass, tyres, discs and lamps keep the artist's materials.
  const restyle = new Set(["paint", "hull", "rims", "calipers", "interior", "seat", "frame", "deck", "exhaust"]);
  const roles = {};
  const boundRoles = new Set();
  for (const [m, role] of roleOf) {
    roles[role] = (roles[role] ?? 0) + 1;
    if (!restyle.has(role)) continue;
    // Remember the artist's look so "Original finish" can bring it back.
    m.userData.original = snapshotMaterial(m);
    // Painted surfaces drop a baked colour map so a chosen colour reads true.
    if (role === "paint" || role === "hull") m.userData.dropMapOnRestyle = true;
    m.needsUpdate = true;
    mats.bind(role, m);
    boundRoles.add(role);
  }

  // Report what was actually bound (names + geometry), not only the guesses.
  analysis.roles = {};
  for (const [m, role] of roleOf) (analysis.roles[role] ??= []).push(m.name || "(unnamed)");
  const paintMat = [...roleOf].find(([, r]) => r === paintRole)?.[0];
  if (paintMat && (!analysis.originalPaint || named.get(paintMat) === paintRole)) analysis.originalPaint = describeFinish(paintMat, paintRole);
  analysis.boundRoles = [...boundRoles];

  return { group, roles, boundRoles, length: analysis.dims.length, analysis, anchors: analysis.anchors };
}

function snapshotMaterial(m) {
  return {
    color: m.color?.clone(),
    map: m.map ?? null,
    metalness: m.metalness,
    roughness: m.roughness,
    clearcoat: m.clearcoat ?? 0,
    clearcoatRoughness: m.clearcoatRoughness ?? 0,
    iridescence: m.iridescence ?? 0,
    sheen: m.sheen ?? 0,
  };
}
