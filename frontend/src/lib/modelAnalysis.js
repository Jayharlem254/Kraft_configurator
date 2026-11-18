// Glue between the 3D model analyser and the engine: analyse an uploaded
// vehicle's model and store the report, so the engine can offer the model's own
// finish and the Studio can show what was found.
import { api } from "./api.js";
import { ANALYSIS_VERSION, summarize } from "./three/analyze.js";
import { loadGLTF, prepareGLTF } from "./three/glb.js";
import { MaterialLibrary } from "./three/materials.js";

/** True when the vehicle's stored report is missing or out of date. */
export function needsAnalysis(vehicle) {
  if (!vehicle?.model_file || !vehicle.id) return false;
  const a = vehicle.analysis;
  return !a || a.version !== ANALYSIS_VERSION || a.model_file !== vehicle.model_file;
}

/** Send an analysis (from the stage, or freshly computed) to the engine. */
export async function storeAnalysis(vehicle, analysis) {
  const summary = summarize(analysis, { model_file: vehicle.model_file });
  const updated = await api.saveAnalysis(vehicle.id, summary);
  api.invalidateCatalog();
  return { summary, vehicle: updated };
}

/** Load, analyse and store a vehicle's model without showing it. */
export async function analyzeVehicle(vehicle, onProgress) {
  const gltf = await loadGLTF(vehicle.model_file, onProgress);
  const mats = new MaterialLibrary();
  const prepared = prepareGLTF(gltf, { kind: vehicle.vehicle_type, materialMap: vehicle.material_map }, mats);
  // Only the report is needed; free the cloned materials.
  prepared.group.traverse((n) => {
    if (!n.isMesh) return;
    for (const m of [].concat(n.material)) m?.dispose?.();
  });
  mats.dispose();
  return storeAnalysis(vehicle, prepared.analysis);
}
