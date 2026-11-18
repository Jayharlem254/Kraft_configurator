// Kraft Stage — the real-time 3D renderer behind the configurator.
//
// Performance model:
//  * Render on demand: frames are only drawn while something changes (camera
//    damping, tweens, option updates). An idle configurator costs ~0% GPU.
//  * Adaptive resolution: while the user drags, the pixel ratio drops to 1;
//    when the camera settles one full-resolution frame is drawn.
//  * Static shadow map: shadows are re-rendered only when the model changes.
//  * Role materials are shared instances, so an option change is a uniform
//    update, not a scene traversal; procedural geometry is rebuilt only when a
//    structural part (wheel design, spoiler, roof, ...) actually changes.
import {
  ACESFilmicToneMapping,
  AmbientLight,
  Box3,
  CanvasTexture,
  CircleGeometry,
  Color,
  DirectionalLight,
  Group,
  HemisphereLight,
  Mesh,
  MeshBasicMaterial,
  PCFSoftShadowMap,
  PerspectiveCamera,
  PlaneGeometry,
  PMREMGenerator,
  Scene,
  ShadowMaterial,
  SRGBColorSpace,
  Vector3,
  WebGLRenderer,
} from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
import { loadGLTF, prepareGLTF } from "./glb.js";
import { MaterialLibrary } from "./materials.js";
import { buildBike } from "./procedural/bike.js";
import { buildBoat } from "./procedural/boat.js";
import { buildCar } from "./procedural/car.js";
import { disposeObject } from "./procedural/common.js";

export const ENVIRONMENTS = {
  studio: { label: "Studio", top: "#f7f7f8", bottom: "#d4d5d8", exposure: 1.0, env: 1.0, key: 2.6, shadow: 0.28 },
  gallery: { label: "Gallery", top: "#ffffff", bottom: "#eceae6", exposure: 1.08, env: 1.1, key: 2.2, shadow: 0.2 },
  dusk: { label: "Dusk", top: "#2b3446", bottom: "#c98d6a", exposure: 0.95, env: 0.8, key: 3.0, keyColor: "#ffd2a8", shadow: 0.4 },
  night: { label: "Night", top: "#05070b", bottom: "#1b2230", exposure: 0.9, env: 0.55, key: 1.4, keyColor: "#a9c4ff", shadow: 0.5 },
};

// Camera presets as direction vectors (scaled by model size) + target offsets.
const VIEWS = {
  front34: { dir: [1, 0.32, 1.05], fit: 1.0 },
  side: { dir: [0, 0.18, 1], fit: 1.05 },
  rear34: { dir: [-1, 0.36, 1.05], fit: 1.0 },
  front: { dir: [1, 0.2, 0.001], fit: 1.25 },
  rear: { dir: [-1, 0.25, 0.001], fit: 1.25 },
  top: { dir: [0.001, 1, 0.35], fit: 1.0 },
  interior: { dir: [-0.35, 1, 0.45], fit: 0.55, target: "cabin", cutaway: true },
  wheel: { dir: [0.55, 0.12, 1], fit: 0.32, target: "wheel" },
};

const easeInOut = (t) => (t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2);

export class Stage {
  constructor(container, { interactive = true, autoRotate = false } = {}) {
    this.container = container;
    this.renderer = new WebGLRenderer({ antialias: true, alpha: true, powerPreference: "high-performance", preserveDrawingBuffer: false });
    this.renderer.outputColorSpace = SRGBColorSpace;
    this.renderer.toneMapping = ACESFilmicToneMapping;
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = PCFSoftShadowMap;
    this.renderer.shadowMap.autoUpdate = false;
    this.maxDpr = Math.min(window.devicePixelRatio || 1, 2);
    this.renderer.setPixelRatio(this.maxDpr);
    this.renderer.domElement.style.display = "block";
    this.renderer.domElement.style.width = "100%";
    this.renderer.domElement.style.height = "100%";
    this.renderer.domElement.style.touchAction = "none";
    container.appendChild(this.renderer.domElement);

    this.scene = new Scene();
    this.camera = new PerspectiveCamera(30, 1, 0.05, 400);
    this.camera.position.set(6, 2.2, 6);

    const pmrem = new PMREMGenerator(this.renderer);
    this.envMap = pmrem.fromScene(new RoomEnvironment(this.renderer), 0.04).texture;
    pmrem.dispose();
    this.scene.environment = this.envMap;

    this.hemi = new HemisphereLight("#ffffff", "#8a8d93", 0.5);
    this.scene.add(this.hemi);
    this.ambient = new AmbientLight("#ffffff", 0.15);
    this.scene.add(this.ambient);
    this.key = new DirectionalLight("#ffffff", 2.6);
    this.key.castShadow = true;
    this.key.shadow.mapSize.set(2048, 2048);
    this.key.shadow.bias = -0.0004;
    this.key.shadow.normalBias = 0.02;
    this.key.shadow.radius = 6;
    this.scene.add(this.key);
    this.scene.add(this.key.target);

    // Ground: a soft radial contact shadow plus a shadow-catcher plane.
    this.ground = new Group();
    this.shadowCatcher = new Mesh(new PlaneGeometry(1, 1), new ShadowMaterial({ opacity: 0.28 }));
    this.shadowCatcher.rotation.x = -Math.PI / 2;
    this.shadowCatcher.receiveShadow = true;
    this.ground.add(this.shadowCatcher);
    this.contact = new Mesh(new CircleGeometry(0.5, 48), new MeshBasicMaterial({ map: contactTexture(), transparent: true, depthWrite: false, opacity: 0.55 }));
    this.contact.rotation.x = -Math.PI / 2;
    this.contact.position.y = 0.002;
    this.ground.add(this.contact);
    this.scene.add(this.ground);

    this.mats = new MaterialLibrary();
    this.model = new Group();
    this.scene.add(this.model);
    this.current = null; // { spec, parts, build }
    this.visual = { roles: {}, parts: {} };
    this.cutawayActive = false;
    this.anchors = {};
    this.size = 4.5;
    this.loadToken = 0;

    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = true;
    this.controls.dampingFactor = 0.08;
    this.controls.enablePan = false;
    this.controls.maxPolarAngle = Math.PI * 0.495;
    this.controls.autoRotateSpeed = 0.6;
    this.controls.autoRotate = autoRotate;
    this.controls.enabled = interactive;
    this.controls.addEventListener("change", () => this.invalidate());
    this.controls.addEventListener("start", () => {
      this.interacting = true;
      this.tween = null;
      this.onInteract?.();
    });
    this.controls.addEventListener("end", () => {
      this.interacting = false;
      this.settleFrames = 12;
      this.invalidate();
    });

    this.environment = "studio";
    this.setEnvironment("studio");

    this.resizeObserver = new ResizeObserver(() => this.resize());
    this.resizeObserver.observe(container);
    this.resize();

    this.frameTimes = [];
    this.loop = this.loop.bind(this);
    this.raf = requestAnimationFrame(this.loop);
  }

  // ---- Render loop ------------------------------------------------------------
  invalidate(frames = 2) {
    this.dirty = Math.max(this.dirty ?? 0, frames);
  }

  loop(now) {
    this.raf = requestAnimationFrame(this.loop);
    let active = false;
    if (this.tween) {
      const t = Math.min(1, (now - this.tween.start) / this.tween.duration);
      const k = easeInOut(t);
      this.camera.position.lerpVectors(this.tween.fromPos, this.tween.toPos, k);
      this.controls.target.lerpVectors(this.tween.fromTarget, this.tween.toTarget, k);
      if (t >= 1) {
        this.tween = null;
        this.settleFrames = 2;
      }
      active = true;
    }
    if (this.controls.autoRotate || this.controls.enableDamping) {
      // update() returns true while damping is still moving the camera.
      if (this.controls.update()) active = true;
    }
    if (this.controls.autoRotate) active = true;

    // Adaptive resolution: draw at DPR 1 while moving, full DPR once settled.
    const moving = active || this.interacting;
    const wantDpr = moving && this.lowDprWhileMoving ? 1 : this.maxDpr;
    if (this.renderer.getPixelRatio() !== wantDpr) {
      this.renderer.setPixelRatio(wantDpr);
      this.resize(false);
    }

    if (active || this.dirty > 0 || this.settleFrames > 0) {
      const t0 = performance.now();
      this.renderer.render(this.scene, this.camera);
      const dt = performance.now() - t0;
      this.frameTimes.push(dt);
      if (this.frameTimes.length > 30) this.frameTimes.shift();
      // Switch on low-DPR interaction only on GPUs that actually struggle.
      if (this.frameTimes.length === 30) {
        const avg = this.frameTimes.reduce((a, b) => a + b, 0) / 30;
        this.lowDprWhileMoving = avg > 9;
      }
      if (this.dirty > 0) this.dirty--;
      if (this.settleFrames > 0) this.settleFrames--;
      this.frames = (this.frames ?? 0) + 1;
    }
  }

  resize(render = true) {
    const w = Math.max(1, this.container.clientWidth);
    const h = Math.max(1, this.container.clientHeight);
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    if (render) this.invalidate();
  }

  // ---- Environment ----------------------------------------------------------
  setEnvironment(name) {
    const e = ENVIRONMENTS[name] ?? ENVIRONMENTS.studio;
    this.environment = name;
    this.renderer.toneMappingExposure = e.exposure;
    this.key.intensity = e.key;
    this.key.color = new Color(e.keyColor ?? "#ffffff");
    this.scene.environmentIntensity = e.env;
    this.shadowCatcher.material.opacity = e.shadow;
    this.contact.material.opacity = e.shadow * 2;
    this.container.style.background = `linear-gradient(180deg, ${e.top} 0%, ${e.top} 35%, ${e.bottom} 100%)`;
    this.renderer.shadowMap.needsUpdate = true;
    this.invalidate();
  }

  setAutoRotate(on) {
    this.controls.autoRotate = on;
    this.invalidate();
  }

  // ---- Model -----------------------------------------------------------------
  // spec: { kind, shape, modelUrl, materialMap }
  async setVehicle(spec, visual = this.visual, onProgress) {
    const token = ++this.loadToken;
    this.visual = visual;
    // Each vehicle gets a fresh material library so nothing leaks between
    // models; the old one is disposed only after the swap.
    const mats = new MaterialLibrary();
    let built;
    if (spec.modelUrl) {
      const gltf = await loadGLTF(spec.modelUrl, onProgress);
      if (token !== this.loadToken) return false;
      const prepared = prepareGLTF(gltf, { kind: spec.kind, length: spec.shape?.length, materialMap: spec.materialMap }, mats);
      built = {
        group: prepared.group,
        cutaway: [],
        anchors: prepared.anchors,
        length: prepared.length,
        glb: true,
        roles: prepared.roles,
        boundRoles: prepared.boundRoles,
        analysis: prepared.analysis,
      };
    } else {
      built = this.buildProcedural(spec, visual.parts ?? {}, mats);
    }
    if (token !== this.loadToken) {
      mats.dispose();
      return false;
    }
    mats.applyAll(visual.roles ?? {});
    const oldMats = this.mats;
    this.mats = mats;
    this.swapModel(built);
    oldMats.dispose();
    this.current = { spec, partsKey: JSON.stringify(visual.parts ?? {}), built };
    this.frameModel();
    return built;
  }

  buildProcedural(spec, parts, mats = this.mats) {
    const kind = spec.kind ?? spec.shape?.kind ?? "car";
    const shape = { ...(spec.shape ?? {}) };
    if (kind === "bike") return buildBike(shape, parts, mats);
    if (kind === "boat") return buildBoat(shape, parts, mats);
    return buildCar(shape, parts, mats);
  }

  swapModel(built) {
    for (const child of [...this.model.children]) {
      this.model.remove(child);
      // Cached GLB geometry is shared with other clones; only dispose procedural meshes.
      if (!this.current?.built?.glb) disposeObject(child);
    }
    this.model.add(built.group);
    this.model.updateMatrixWorld(true);

    const box = new Box3().setFromObject(this.model);
    const size = box.getSize(new Vector3());
    this.bounds = box;
    this.size = Math.max(size.x, size.z, size.y);
    this.anchors = {
      wheel: built.anchors?.wheel ?? new Vector3(box.max.x * 0.62, size.y * 0.22, box.max.z * 0.9),
      cabin: built.anchors?.cabin ?? new Vector3(box.min.x * 0.05, size.y * 0.65, 0),
      center: box.getCenter(new Vector3()),
    };
    this.cutaway = built.cutaway ?? [];
    this.highlights = built.analysis?.highlights ?? [];
    this.applyCutaway(this.cutawayActive);

    // Fit lights and ground to the model.
    const r = this.size;
    this.key.position.set(r * 0.6, r * 1.4, r * 0.9);
    this.key.target.position.set(0, 0, 0);
    const cam = this.key.shadow.camera;
    cam.left = cam.bottom = -r * 0.75;
    cam.right = cam.top = r * 0.75;
    cam.near = 0.1;
    cam.far = r * 4;
    cam.updateProjectionMatrix();
    this.shadowCatcher.scale.set(r * 3, r * 3, 1);
    this.contact.scale.set(size.x * 1.25, size.z * 2.2, 1);
    this.controls.minDistance = r * 0.25;
    this.controls.maxDistance = r * 3.2;
    this.renderer.shadowMap.needsUpdate = true;
    this.invalidate(4);
  }

  // Apply a visual state; rebuilds geometry only if structural parts changed.
  applyVisual(visual) {
    this.visual = visual;
    this.mats.applyAll(visual.roles ?? {});
    if (this.current && !this.current.spec.modelUrl) {
      const key = JSON.stringify(visual.parts ?? {});
      if (key !== this.current.partsKey) {
        const built = this.buildProcedural(this.current.spec, visual.parts ?? {});
        this.swapModel(built);
        this.current.partsKey = key;
        this.current.built = built;
      }
    }
    this.renderer.shadowMap.needsUpdate = true;
    this.invalidate(3);
  }

  // ---- Camera ------------------------------------------------------------------
  // Named presets, or "hl:<id>" for a highlight found by the model analyser.
  // Uploaded models have no cut-away, so "interior" uses the analysed cabin shot.
  resolveView(name) {
    const hl = (id) => this.highlights?.find((h) => h.id === id);
    if (name?.startsWith("hl:")) {
      const h = hl(name.slice(3));
      if (h) return { dir: h.dir, fit: h.fit, point: h.target };
    }
    if (name === "interior" && this.current?.built?.glb && hl("interior")) {
      const h = hl("interior");
      return { dir: h.dir, fit: h.fit, point: h.target };
    }
    return VIEWS[name] ?? VIEWS.front34;
  }

  viewPose(name) {
    const v = this.resolveView(name);
    const dir = new Vector3(...v.dir).normalize();
    const target = v.point
      ? v.point.clone()
      : v.target
        ? this.anchors[v.target].clone()
        : new Vector3(0, (this.bounds?.max.y ?? 1.2) * 0.38, 0);
    const fov = (this.camera.fov * Math.PI) / 180;
    const aspectFit = Math.max(1, 1.5 / Math.max(this.camera.aspect, 0.5));
    const dist = ((this.size * 0.62) / Math.tan(fov / 2)) * v.fit * aspectFit;
    return { pos: target.clone().add(dir.multiplyScalar(dist)), target, cutaway: !!v.cutaway };
  }

  setView(name, { animate = true, duration = 900 } = {}) {
    const pose = this.viewPose(name);
    this.view = name;
    this.applyCutaway(pose.cutaway);
    if (!animate) {
      this.tween = null; // a pending tween would otherwise override the new pose
      this.camera.position.copy(pose.pos);
      this.controls.target.copy(pose.target);
      this.controls.update();
      this.invalidate();
      return;
    }
    this.tween = {
      start: performance.now(),
      duration,
      fromPos: this.camera.position.clone(),
      toPos: pose.pos,
      fromTarget: this.controls.target.clone(),
      toTarget: pose.target,
    };
    this.invalidate();
  }

  frameModel() {
    this.setView(this.view ?? "front34", { animate: false });
  }

  applyCutaway(on) {
    this.cutawayActive = on;
    for (const o of this.cutaway ?? []) o.visible = !on;
    this.renderer.shadowMap.needsUpdate = true;
    this.invalidate();
  }

  // ---- Capture -------------------------------------------------------------------
  snapshot({ width = 1200, height = 675, type = "image/jpeg", quality = 0.9, view } = {}) {
    const prev = { w: this.renderer.domElement.width, h: this.renderer.domElement.height, dpr: this.renderer.getPixelRatio(), aspect: this.camera.aspect, pos: this.camera.position.clone(), target: this.controls.target.clone() };
    if (view) {
      const pose = this.viewPose(view);
      this.camera.position.copy(pose.pos);
      this.camera.lookAt(pose.target);
    }
    this.renderer.setPixelRatio(1);
    this.renderer.setSize(width, height, false);
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    const bg = ENVIRONMENTS[this.environment] ?? ENVIRONMENTS.studio;
    this.scene.background = gradientTexture(bg.top, bg.bottom);
    this.renderer.render(this.scene, this.camera);
    const url = this.renderer.domElement.toDataURL(type, quality);
    this.scene.background.dispose();
    this.scene.background = null;
    this.renderer.setPixelRatio(prev.dpr);
    this.camera.position.copy(prev.pos);
    this.controls.target.copy(prev.target);
    this.resize();
    return url;
  }

  stats() {
    const info = this.renderer.info;
    return { calls: info.render.calls, triangles: info.render.triangles, geometries: info.memory.geometries, textures: info.memory.textures, frames: this.frames ?? 0 };
  }

  dispose() {
    cancelAnimationFrame(this.raf);
    this.resizeObserver.disconnect();
    this.controls.dispose();
    if (!this.current?.built?.glb) disposeObject(this.model);
    this.mats.dispose();
    this.envMap.dispose();
    this.renderer.dispose();
    this.renderer.forceContextLoss?.();
    this.renderer.domElement.remove();
  }
}

function contactTexture() {
  const s = 128;
  const c = document.createElement("canvas");
  c.width = c.height = s;
  const g = c.getContext("2d");
  const grad = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
  grad.addColorStop(0, "rgba(0,0,0,0.55)");
  grad.addColorStop(0.5, "rgba(0,0,0,0.22)");
  grad.addColorStop(1, "rgba(0,0,0,0)");
  g.fillStyle = grad;
  g.fillRect(0, 0, s, s);
  const t = new CanvasTexture(c);
  t.colorSpace = SRGBColorSpace;
  return t;
}

function gradientTexture(top, bottom) {
  const c = document.createElement("canvas");
  c.width = 2;
  c.height = 256;
  const g = c.getContext("2d");
  const grad = g.createLinearGradient(0, 0, 0, 256);
  grad.addColorStop(0, top);
  grad.addColorStop(0.35, top);
  grad.addColorStop(1, bottom);
  g.fillStyle = grad;
  g.fillRect(0, 0, 2, 256);
  const t = new CanvasTexture(c);
  t.colorSpace = SRGBColorSpace;
  return t;
}
