// Configurator state. The C++ rules engine is the single source of truth: every
// change is sent as {variant, selections, changed} and the engine answers with
// the resolved, priced build. The UI never re-implements pricing or rules.
import { api } from "./api.js";
import { visualFromResolved } from "./three/materials.js";

let toastSeq = 0;

export class Configurator {
  vehicle = $state(null);
  variant = $state("");
  selections = $state([]); // what the user explicitly asked for, oldest -> newest
  result = $state(null); // last engine response
  error = $state(null);
  loading = $state(false);
  evaluating = $state(false);
  toasts = $state([]);
  canUndo = $state(false);
  canRedo = $state(false);
  lastEngineUs = $state(0);

  resolvedList = $derived(this.result?.resolved ?? []);
  resolved = $derived(new Map(this.resolvedList.map((r) => [r.code, r])));
  resolvedCodes = $derived(new Set(this.resolvedList.map((r) => r.code)));
  visual = $derived(visualFromResolved(this.vehicle, this.resolvedCodes));
  variantInfo = $derived(this.vehicle?.variants?.find((v) => v.code === (this.result?.variant ?? this.variant)) ?? this.vehicle?.variants?.[0] ?? null);
  price = $derived(this.result?.price ?? { base: 0, options: 0, total: 0, sections: {} });
  specs = $derived(this.result?.specs ?? this.variantInfo?.specs ?? {});

  #history = [];
  #future = [];
  #controller = null;
  #seq = 0;

  async load(slug, { variant = "", selections = [] } = {}) {
    this.loading = true;
    this.error = null;
    this.#history = [];
    this.#future = [];
    this.syncHistoryFlags();
    try {
      this.vehicle = await api.getVehicle(slug);
      const def = this.vehicle.variants?.find((v) => v.is_default) ?? this.vehicle.variants?.[0];
      this.variant = this.vehicle.variants?.some((v) => v.code === variant) ? variant : def?.code ?? "";
      this.selections = selections;
      await this.evaluate({ record: false });
    } catch (err) {
      this.error = err.message;
    } finally {
      this.loading = false;
    }
  }

  async evaluate({ changed = "", deselect = false, record = true, variant = this.variant, selections = this.selections } = {}) {
    if (!this.vehicle) return;
    this.#controller?.abort();
    const controller = new AbortController();
    this.#controller = controller;
    const seq = ++this.#seq;
    const before = { variant: this.variant, selections: [...this.selections] };
    this.evaluating = true;
    try {
      const res = await api.evaluate(this.vehicle.slug, { variant, selections: [...selections], changed, deselect }, controller.signal);
      if (seq !== this.#seq) return;
      if (record) {
        this.#history.push(before);
        if (this.#history.length > 100) this.#history.shift();
        this.#future = [];
      }
      this.result = res;
      this.variant = res.variant;
      this.selections = res.selections;
      this.lastEngineUs = res.engine_us ?? 0;
      this.error = null;
      if (changed) for (const c of res.changes ?? []) this.toast(c.type === "removed" ? "removed" : "added", c.name, c.reason);
    } catch (err) {
      if (err.name !== "AbortError") this.error = err.message;
    } finally {
      if (seq === this.#seq) this.evaluating = false;
      this.syncHistoryFlags();
    }
  }

  isSelected(code) {
    return this.resolvedCodes.has(code);
  }

  toggle(choice, category) {
    const on = this.resolvedCodes.has(choice.code);
    if (category.selection_type !== "multi") {
      if (on) return;
      return this.evaluate({ changed: choice.code });
    }
    return this.evaluate({ changed: choice.code, deselect: on });
  }

  setVariant(code) {
    if (code === this.variant) return;
    return this.evaluate({ variant: code });
  }

  reset() {
    return this.evaluate({ selections: [] });
  }

  undo() {
    const prev = this.#history.pop();
    if (!prev) return;
    this.#future.push({ variant: this.variant, selections: [...this.selections] });
    return this.evaluate({ ...prev, record: false });
  }

  redo() {
    const next = this.#future.pop();
    if (!next) return;
    this.#history.push({ variant: this.variant, selections: [...this.selections] });
    return this.evaluate({ ...next, record: false });
  }

  syncHistoryFlags() {
    this.canUndo = this.#history.length > 0;
    this.canRedo = this.#future.length > 0;
  }

  toast(type, title, text) {
    const id = ++toastSeq;
    this.toasts = [...this.toasts.slice(-3), { id, type, title, text }];
    setTimeout(() => this.dismiss(id), 6000);
  }

  dismiss(id) {
    this.toasts = this.toasts.filter((t) => t.id !== id);
  }

  // The currently resolved choice of a single-choice category.
  choiceIn(category) {
    return category.choices.find((c) => this.resolvedCodes.has(c.code)) ?? null;
  }
}
