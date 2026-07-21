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
};
