// Minimal hash router. Hash routing keeps the C++ static file server simple
// (no SPA fallback rewrite needed) and makes every configuration a shareable URL.
//
//   #/                         model range
//   #/models/:type             model range filtered by car | bike | boat
//   #/configure/:slug?variant=&o=CODE.CODE
//   #/summary/:slug?variant=&o=
//   #/code/:code               load a saved configuration by its code
//   #/garage  #/compare?v=slug:variant,...  #/studio

function parse() {
  const raw = location.hash.replace(/^#/, "") || "/";
  const [pathPart, queryPart = ""] = raw.split("?");
  const segments = pathPart.split("/").filter(Boolean).map(decodeURIComponent);
  return { path: pathPart, segments, query: Object.fromEntries(new URLSearchParams(queryPart)) };
}

export const route = $state(parse());

if (typeof window !== "undefined") {
  window.addEventListener("hashchange", () => {
    const next = parse();
    const pageChanged = next.segments[0] !== route.segments[0] || next.segments[1] !== route.segments[1];
    Object.assign(route, next);
    if (pageChanged) window.scrollTo({ top: 0 });
  });
}

export function href(path, query) {
  const qs = query ? new URLSearchParams(Object.entries(query).filter(([, v]) => v !== undefined && v !== null && v !== "")).toString() : "";
  return `#${path}${qs ? `?${qs}` : ""}`;
}

export function navigate(path, query) {
  location.hash = href(path, query).slice(1);
}

// Update the URL without adding a history entry or re-triggering navigation
// (used to keep the configurator URL in sync with every option change).
export function replaceQuery(path, query) {
  const url = href(path, query);
  history.replaceState(history.state, "", url);
  Object.assign(route, parse());
}
