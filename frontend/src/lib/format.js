const currency = new Intl.NumberFormat("en-US", { style: "currency", currency: "USD", maximumFractionDigits: 0 });
const number = new Intl.NumberFormat("en-US", { maximumFractionDigits: 1 });

export const money = (v) => currency.format(Number(v) || 0);
export const signedMoney = (v) => (Number(v) > 0 ? `+ ${money(v)}` : Number(v) < 0 ? `− ${money(-v)}` : money(0));
export const num = (v) => number.format(Number(v) || 0);

export const TYPE_LABELS = { car: "Cars", bike: "Motorcycles", boat: "Boats" };
export const TYPE_SINGULAR = { car: "Car", bike: "Motorcycle", boat: "Boat" };

export const CLASS_LABELS = {
  coupe: "Coupé",
  convertible: "Convertible",
  supercar: "Supercar",
  sedan: "Sedan",
  electric: "Electric",
  suv: "SUV",
  hatchback: "Hatchback",
  wagon: "Touring / Wagon",
  pickup: "Pickup",
  van: "Van / MPV",
  offroad: "Off-road",
  sport: "Sport",
  naked: "Naked",
  cruiser: "Cruiser",
  touring: "Touring",
  adventure: "Adventure",
  motocross: "Motocross",
  scooter: "Scooter",
  cafe: "Café Racer",
  speedboat: "Speedboat",
  yacht: "Motor Yacht",
  sailboat: "Sailboat",
  catamaran: "Catamaran",
  pontoon: "Pontoon",
  jetski: "Personal Watercraft",
  fishing: "Fishing",
  other: "Custom",
  "": "Custom",
};
export const classLabel = (c) => CLASS_LABELS[c ?? ""] ?? c;

// Body classes available per vehicle type (used by the Studio form).
export const CLASSES_BY_TYPE = {
  car: ["coupe", "convertible", "supercar", "sedan", "electric", "suv", "hatchback", "wagon", "pickup", "van", "offroad"],
  bike: ["sport", "naked", "cruiser", "touring", "adventure", "motocross", "scooter", "electric", "cafe"],
  boat: ["speedboat", "yacht", "sailboat", "catamaran", "pontoon", "jetski", "fishing", "electric"],
};

export const SECTION_ORDER = ["exterior", "wheels", "interior", "performance", "technology", "comfort", "packages"];
export function sectionLabel(section, type) {
  if (section === "interior" && type === "bike") return "Rider";
  if (section === "wheels" && type === "boat") return "Running gear";
  return (
    {
      exterior: "Exterior",
      wheels: "Wheels",
      interior: "Interior",
      performance: type === "boat" ? "Propulsion" : "Performance",
      technology: "Technology",
      comfort: "Comfort",
      packages: "Packages",
    }[section] ?? section.charAt(0).toUpperCase() + section.slice(1)
  );
}

// Technical data dictionary. `better` drives the green/amber delta colouring.
export const SPECS = {
  power_hp: { label: "Power", unit: "hp", better: "up" },
  torque_nm: { label: "Max. torque", unit: "Nm", better: "up" },
  accel_0_100_s: { label: "0–100 km/h", unit: "s", better: "down", digits: 1 },
  top_speed_kmh: { label: "Top speed", unit: "km/h", better: "up" },
  range_km: { label: "Electric range", unit: "km", better: "up" },
  consumption_l_100km: { label: "Fuel consumption", unit: "l/100 km", better: "down", digits: 1 },
  weight_kg: { label: "Weight", unit: "kg", better: "down" },
  drive: { label: "Drive", unit: "" },
  seats: { label: "Seats", unit: "" },
  towing_kg: { label: "Towing capacity", unit: "kg", better: "up" },
  displacement_cc: { label: "Displacement", unit: "cc" },
  seat_height_mm: { label: "Seat height", unit: "mm" },
  length_m: { label: "Length overall", unit: "m", digits: 1 },
  beam_m: { label: "Beam", unit: "m", digits: 1 },
  draft_m: { label: "Draft", unit: "m", digits: 1 },
  capacity: { label: "Passengers", unit: "" },
  berths: { label: "Berths", unit: "" },
  fuel_l: { label: "Fuel capacity", unit: "l" },
};

// The three figures shown on cards and in the live bar, per vehicle type.
export function headlineSpecs(type, specs = {}) {
  const keys =
    type === "boat"
      ? ["power_hp", "top_speed_kmh", "length_m"]
      : specs.range_km && !specs.consumption_l_100km
        ? ["power_hp", "accel_0_100_s", "range_km"]
        : ["power_hp", "accel_0_100_s", "top_speed_kmh"];
  return keys.filter((k) => specs[k] !== undefined && specs[k] !== 0);
}

export function formatSpec(key, value) {
  const meta = SPECS[key];
  if (value === undefined || value === null || value === "") return "—";
  if (typeof value !== "number") return String(value);
  const digits = meta?.digits ?? (Number.isInteger(value) ? 0 : 1);
  const text = value.toLocaleString("en-US", { minimumFractionDigits: digits, maximumFractionDigits: digits });
  return meta?.unit ? `${text} ${meta.unit}` : text;
}

export function specLabel(key) {
  return SPECS[key]?.label ?? key.replace(/_/g, " ");
}

export function specDelta(key, value, base) {
  if (typeof value !== "number" || typeof base !== "number") return null;
  const d = Math.round((value - base) * 100) / 100;
  if (Math.abs(d) < 0.001) return null;
  const better = SPECS[key]?.better;
  const good = better === "up" ? d > 0 : better === "down" ? d < 0 : null;
  return { value: d, good, text: `${d > 0 ? "+" : "−"}${formatSpec(key, Math.abs(d))}` };
}

export function isElectric(v) {
  if (v?.shape?.electric || v?.body_class === "electric") return true;
  const specs = v?.variants?.[0]?.specs ?? {};
  return !!specs.range_km && !specs.consumption_l_100km && !specs.fuel_l;
}

export function hashString(s) {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return h >>> 0;
}
