// REST client for the Kraft C++ engine.
const BASE = "/api";

// Conditional-GET cache: the engine tags catalog responses with an ETag tied to
// the catalog snapshot version, so repeat visits cost a 304 and no parsing.
const etagCache = new Map(); // url -> { etag, data }

async function request(path, options = {}) {
  const url = `${BASE}${path}`;
  const method = (options.method ?? "GET").toUpperCase();
  const headers = { ...(options.body ? { "Content-Type": "application/json" } : {}), ...options.headers };
  const cached = method === "GET" ? etagCache.get(url) : null;
  if (cached) headers["If-None-Match"] = cached.etag;

  const res = await fetch(url, { ...options, method, headers, signal: options.signal });
  if (res.status === 304 && cached) return cached.data;
  if (!res.ok) {
    let message = `Request failed (${res.status})`;
    try {
      const body = await res.json();
      message = body.error || body.detail || message;
    } catch {
      /* non-JSON error body */
    }
    const err = new Error(message);
    err.status = res.status;
    throw err;
  }
  if (res.status === 204) return null;
  const data = await res.json();
  const etag = res.headers.get("ETag");
  if (method === "GET" && etag) etagCache.set(url, { etag, data });
  return data;
}

const json = (body) => JSON.stringify(body);

export const api = {
  health: () => request("/health"),
  meta: () => request("/catalog/meta"),

  listVehicles: ({ type = "", bodyClass = "", q = "", all = false } = {}) => {
    const params = new URLSearchParams();
    if (type) params.set("vehicle_type", type);
    if (bodyClass) params.set("body_class", bodyClass);
    if (q) params.set("q", q);
    if (all) params.set("all", "1");
    const qs = params.toString();
    return request(`/vehicles/${qs ? `?${qs}` : ""}`);
  },
  getVehicle: (slug) => request(`/vehicles/${encodeURIComponent(slug)}/`),

  evaluate: (slug, payload, signal) =>
    request(`/vehicles/${encodeURIComponent(slug)}/evaluate`, { method: "POST", body: json(payload), signal }),

  listConfigurations: (sessionKey) => request(`/configurations/?session_key=${encodeURIComponent(sessionKey)}`),
  getConfiguration: (idOrCode) => request(`/configurations/${encodeURIComponent(idOrCode)}/`),
  saveConfiguration: (payload) => request(`/configurations/`, { method: "POST", body: json(payload) }),
  updateConfiguration: (id, payload) =>
    request(`/configurations/${encodeURIComponent(id)}/`, { method: "PATCH", body: json(payload) }),
  deleteConfiguration: (id) => request(`/configurations/${encodeURIComponent(id)}/`, { method: "DELETE" }),

  createVehicle: (payload) => request(`/vehicles/`, { method: "POST", body: json(payload) }),
  updateVehicle: (id, payload) => request(`/vehicles/${id}/`, { method: "PATCH", body: json(payload) }),
  deleteVehicle: (id, deleteFile = false) => request(`/vehicles/${id}/?delete_file=${deleteFile ? 1 : 0}`, { method: "DELETE" }),
  listModels: () => request("/models"),
  deleteModel: (filename) => request(`/models/${encodeURIComponent(filename)}`, { method: "DELETE" }),

  // XHR rather than fetch so large (100 MB+) uploads can report progress.
  uploadModel(formData, onProgress) {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open("POST", `${BASE}/vehicles/upload`);
      xhr.upload.onprogress = (e) => e.lengthComputable && onProgress?.(e.loaded / e.total);
      xhr.onload = () => {
        let body = null;
        try {
          body = JSON.parse(xhr.responseText);
        } catch {
          /* ignore */
        }
        if (xhr.status >= 200 && xhr.status < 300) resolve(body);
        else reject(new Error(body?.error || `Upload failed (${xhr.status})`));
      };
      xhr.onerror = () => reject(new Error("Network error during upload"));
      xhr.send(formData);
    });
  },

  invalidateCatalog: () => etagCache.clear(),
};

const SESSION_KEY = "kraft-configurator-session";
export function getSessionKey() {
  try {
    let key = localStorage.getItem(SESSION_KEY);
    if (!key) {
      key = crypto.randomUUID?.() ?? `s-${Date.now()}-${Math.random().toString(36).slice(2)}`;
      localStorage.setItem(SESSION_KEY, key);
    }
    return key;
  } catch {
    return "anonymous";
  }
}
