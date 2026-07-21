// Used only as a fallback when the Django API has no data yet (e.g. fresh
// clone, empty DB). Once you seed real vehicles via the admin or the
// asset-pipeline import command, the real API response takes over.
export const mockVehicleList = [
  {
    id: 0,
    name: "Velora GT",
    slug: "velora-gt",
    vehicle_type: "car",
    tagline: "Demo vehicle — replace with a real Sketchfab model",
    base_price: 84000,
    thumbnail: null,
  },
  {
    id: -1,
    name: "Solace Roadster",
    slug: "solace-roadster",
    vehicle_type: "car",
    tagline: "Open-top, low-slung, built for coastal roads",
    base_price: 96500,
    thumbnail: null,
  },
  {
    id: -2,
    name: "Marea 42",
    slug: "marea-42",
    vehicle_type: "boat",
    tagline: "42ft center console, offshore-ready",
    base_price: 412000,
    thumbnail: null,
  },
  {
    id: -3,
    name: "Nyx Streetfighter",
    slug: "nyx-streetfighter",
    vehicle_type: "bike",
    tagline: "Naked street bike, carbon bodywork",
    base_price: 21800,
    thumbnail: null,
  },
];

export const mockVehicle = {
  id: 0,
  name: "Velora GT",
  slug: "velora-gt",
  vehicle_type: "car",
  tagline: "Demo vehicle — replace with a real Sketchfab model",
  base_price: 84000,
  model_file: null,
  option_categories: [
    {
      id: 1,
      name: "Exterior color",
      slug: "exterior-color",
      selection_type: "single",
      choices: [
        { id: 11, name: "Nebula Purple", price_delta: 0, swatch_hex: "#6d28d9", is_default: true },
        { id: 12, name: "Lunar Silver", price_delta: 800, swatch_hex: "#9a9aa6", is_default: false },
        { id: 13, name: "Void Black", price_delta: 800, swatch_hex: "#0c0b12", is_default: false },
        { id: 14, name: "Ion White", price_delta: 600, swatch_hex: "#f4f3f8", is_default: false },
      ],
    },
    {
      id: 2,
      name: "Wheels",
      slug: "wheels",
      selection_type: "single",
      choices: [
        { id: 21, name: "19in Aero", price_delta: 0, swatch_hex: "", is_default: true },
        { id: 22, name: "20in Forged", price_delta: 2200, swatch_hex: "", is_default: false },
        { id: 23, name: "21in Carbon", price_delta: 4100, swatch_hex: "", is_default: false },
      ],
    },
    {
      id: 3,
      name: "Interior",
      slug: "interior",
      selection_type: "single",
      choices: [
        { id: 31, name: "Graphite weave", price_delta: 0, swatch_hex: "#3a3a42", is_default: true },
        { id: 32, name: "Violet leather", price_delta: 1600, swatch_hex: "#5b21b6", is_default: false },
      ],
    },
  ],
};