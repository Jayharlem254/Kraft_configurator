// GLB/GLTF ingestion: decode (Draco + Meshopt), normalise orientation/scale,
// and bind materials to configurator roles by name heuristics (overridable per
// vehicle through `material_map`).
import { Box3, MeshPhysicalMaterial, Vector3 } from "three";
import { DRACOLoader } from "three/examples/jsm/loaders/DRACOLoader.js";
import { GLTFLoader } from "three/examples/jsm/loaders/GLTFLoader.js";
import { MeshoptDecoder } from "three/examples/jsm/libs/meshopt_decoder.module.js";

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

// Prepare a loaded GLTF for the stage. Returns { group, roles, length }.
export function prepareGLTF(gltf, { kind = "car", length, materialMap = {} } = {}, mats) {
  const scene = gltf.scene.clone(true);

  // Orientation: lay the longest horizontal axis along x.
  let box = new Box3().setFromObject(scene);
  let size = box.getSize(new Vector3());
  if (size.z > size.x * 1.15) {
    scene.rotation.y = Math.PI / 2;
    scene.updateMatrixWorld(true);
    box = new Box3().setFromObject(scene);
    size = box.getSize(new Vector3());
  }

  // Scale to a plausible real-world length for the vehicle type.
  const target = length ?? (kind === "bike" ? 2.1 : kind === "boat" ? 9 : 4.6);
  const s = target / Math.max(size.x, 0.001);
  scene.scale.multiplyScalar(s);
  scene.updateMatrixWorld(true);
  box = new Box3().setFromObject(scene);
  const center = box.getCenter(new Vector3());
  scene.position.x -= center.x;
  scene.position.z -= center.z;
  scene.position.y -= box.min.y;

  // Explicit overrides first: material_map = { paint: ["MAT_Body", ...], ... }.
  const overrides = new Map();
  for (const [role, names] of Object.entries(materialMap ?? {})) {
    for (const n of [].concat(names)) overrides.set(String(n).toLowerCase(), role);
  }

  const converted = new Map(); // original material -> physical clone
  const roleOf = new Map(); // physical clone -> role
  const areaByMat = new Map();
  scene.traverse((node) => {
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
        if (role) roleOf.set(phys, role);
      }
      const phys = converted.get(orig);
      const tris = (node.geometry.index?.count ?? node.geometry.attributes.position?.count ?? 0) / 3;
      areaByMat.set(phys, (areaByMat.get(phys) ?? 0) + tris);
      return phys;
    });
    node.material = Array.isArray(node.material) ? next : next[0];
  });

  // No paint found by name: the busiest opaque, non-dark material is almost
  // always the body shell.
  const paintRole = kind === "boat" ? "hull" : "paint";
  if (![...roleOf.values()].includes(paintRole)) {
    let best = null;
    let bestScore = 0;
    for (const [m, tris] of areaByMat) {
      if (roleOf.has(m) || m.transparent || m.opacity < 1) continue;
      const lum = m.color ? m.color.r * 0.3 + m.color.g * 0.59 + m.color.b * 0.11 : 0;
      const score = tris * (lum > 0.08 ? 1 : 0.25);
      if (score > bestScore) {
        best = m;
        bestScore = score;
      }
    }
    if (best) roleOf.set(best, paintRole);
  }

  // Roles we restyle. Glass, tyres and discs keep the artist's materials.
  const restyle = new Set(["paint", "hull", "rims", "calipers", "interior", "seat", "frame", "deck", "exhaust"]);
  const roles = {};
  for (const [m, role] of roleOf) {
    roles[role] = (roles[role] ?? 0) + 1;
    if (!restyle.has(role)) continue;
    // Painted surfaces drop a baked colour map so the chosen colour reads true.
    if (role === "paint" || role === "hull") m.map = null;
    m.needsUpdate = true;
    mats.bind(role, m);
  }

  return { group: scene, roles, length: target };
}
