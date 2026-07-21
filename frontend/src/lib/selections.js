// A category the user hasn't touched yet still has an active choice — its
// is_default option. Anywhere we need the *actual* full build (pricing, the
// 3D preview, saving a configuration) needs to fold explicit `selections`
// over each category's default, not just look at what the user has clicked.
export function effectiveSelections(vehicle, selections = {}) {
  const categories = vehicle?.option_categories ?? [];
  const effective = {};
  for (const category of categories) {
    const choiceId = selections[category.id] ?? category.choices.find((c) => c.is_default)?.id;
    if (choiceId != null) effective[category.id] = choiceId;
  }
  return effective;
}

// Stable anonymous identity for saved configurations (Configuration.session_key).
// One key per browser, kept in localStorage.
const SESSION_STORAGE_KEY = "kraft-configurator-session";

export function getSessionKey() {
  let key = localStorage.getItem(SESSION_STORAGE_KEY);
  if (!key) {
    key = crypto.randomUUID();
    localStorage.setItem(SESSION_STORAGE_KEY, key);
  }
  return key;
}
