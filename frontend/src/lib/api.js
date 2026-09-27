const BASE_URL = "/api";

async function request(path, options = {}) {
  const res = await fetch(`${BASE_URL}${path}`, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  if (!res.ok) {
    throw new Error(`API error ${res.status}: ${await res.text()}`);
  }
  return res.status === 204 ? null : res.json();
}

export const api = {
  checkHealth: async () => {
    try {
      const start = performance.now();
      const res = await fetch(`${BASE_URL}/health`);
      if (!res.ok) throw new Error("Health check failed");
      const data = await res.json();
      const ping = Math.max(1, Math.round(performance.now() - start));
      return { online: true, ping, data };
    } catch (err) {
      return { online: false, ping: 0, error: err };
    }
  },

  listVehicles: (vehicleType) =>
    request(`/vehicles/${vehicleType ? `?vehicle_type=${vehicleType}` : ""}`),

  getVehicle: (slug) => request(`/vehicles/${slug}/`),

  createConfiguration: (vehicleId, optionIds, sessionKey) =>
    request(`/configurations/`, {
      method: "POST",
      body: JSON.stringify({
        vehicle: vehicleId,
        option_ids: optionIds,
        session_key: sessionKey,
      }),
    }),

  updateConfiguration: (id, optionIds) =>
    request(`/configurations/${id}/`, {
      method: "PATCH",
      body: JSON.stringify({ option_ids: optionIds }),
    }),

  uploadVehicleModel: async (formData) => {
    const res = await fetch(`${BASE_URL}/vehicles/upload`, {
      method: "POST",
      body: formData,
    });
    if (!res.ok) {
      let errMsg = `Upload failed (${res.status})`;
      try {
        const errJson = await res.json();
        errMsg = errJson.error || errJson.message || errMsg;
      } catch (_) {}
      throw new Error(errMsg);
    }
    return res.json();
  },

  listModels: () => request("/models"),

  deleteModel: (filename) =>
    request(`/models/${encodeURIComponent(filename)}`, { method: "DELETE" }),

  createVehicle: (data) =>
    request(`/vehicles/`, {
      method: "POST",
      body: JSON.stringify(data),
    }),

  updateVehicle: (id, data) =>
    request(`/vehicles/${id}/`, {
      method: "PATCH",
      body: JSON.stringify(data),
    }),

  deleteVehicle: (id, deleteFile = false) =>
    request(`/vehicles/${id}/?delete_file=${deleteFile ? 1 : 0}`, {
      method: "DELETE",
    }),
};

