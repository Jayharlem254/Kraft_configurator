<script>
  import { createEventDispatcher, onMount } from "svelte";
  import { api } from "../api.js";

  export let vehicles = [];
  export let activeVehicle = null;

  const dispatch = createEventDispatcher();

  // Top-level tab: "upload" | "manage"
  let activeTab = "manage"; // default to manage if models exist, or let user switch

  // --- UPLOAD STATE ---
  let mode = "new"; // "new" | "existing"
  let selectedVehicleId = activeVehicle?.id || (vehicles.length > 0 ? vehicles[0].id : "");

  let vehicleName = "";
  let vehicleType = "car";
  let tagline = "";
  let description = "";
  let basePrice = 75000;

  let file = null;
  let isDragging = false;
  let fileInputEl;

  let isUploading = false;
  let uploadError = "";
  let uploadResult = null;

  // --- REPOSITORY / MANAGE STATE ---
  let models = [];
  let modelsLoading = false;
  let modelsError = "";

  // Edit / Write Modal state
  let editingVehicle = null; // { id, name, vehicle_type, tagline, description, base_price, model_file }
  let isSavingVehicle = false;
  let saveVehicleError = "";

  // Delete Confirmation Modal state
  let deletingModel = null; // { filename, vehicle }
  let isDeleting = false;
  let deleteError = "";

  const vehicleTypes = [
    { id: "car", label: "AUTOMOBILE / CAR" },
    { id: "boat", label: "SPEED BOAT / WATERCRAFT" },
    { id: "bike", label: "MOTORBIKE / CYCLERY" },
  ];

  async function loadModels() {
    modelsLoading = true;
    modelsError = "";
    try {
      models = await api.listModels();
    } catch (err) {
      modelsError = err.message || "Failed to load 3D models repository from C++ engine.";
      console.warn("Error fetching models:", err);
    } finally {
      modelsLoading = false;
    }
  }

  onMount(() => {
    loadModels();
  });

  function handleDragOver(e) {
    e.preventDefault();
    isDragging = true;
  }

  function handleDragLeave(e) {
    e.preventDefault();
    isDragging = false;
  }

  function handleDrop(e) {
    e.preventDefault();
    isDragging = false;
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      processSelectedFile(e.dataTransfer.files[0]);
    }
  }

  function handleFileChange(e) {
    if (e.target.files && e.target.files.length > 0) {
      processSelectedFile(e.target.files[0]);
    }
  }

  function processSelectedFile(selectedFile) {
    const ext = selectedFile.name.split(".").pop().toLowerCase();
    if (!["glb", "gltf", "obj"].includes(ext)) {
      uploadError = `Selected file type .${ext} is not supported. Please upload a .glb, .gltf, or .obj 3D model.`;
      file = null;
      return;
    }
    file = selectedFile;
    uploadError = "";
    uploadResult = null;

    if (!vehicleName && mode === "new") {
      const stem = selectedFile.name.replace(/\.[^/.]+$/, "").replace(/[_-]+/g, " ");
      vehicleName = stem.charAt(0).toUpperCase() + stem.slice(1);
    }
  }

  function formatBytes(bytes) {
    if (!bytes || bytes === 0) return "0 Bytes";
    const k = 1024;
    const sizes = ["Bytes", "KB", "MB", "GB"];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(2) + " " + sizes[i];
  }

  function resetUploadForm() {
    file = null;
    uploadResult = null;
    uploadError = "";
    vehicleName = "";
    tagline = "";
    description = "";
    if (fileInputEl) fileInputEl.value = "";
  }

  async function submitUpload() {
    if (!file) {
      uploadError = "Please select or drop a 3D model file (.glb / .gltf / .obj).";
      return;
    }

    if (mode === "new" && !vehicleName.trim()) {
      uploadError = "Please specify a vehicle name.";
      return;
    }

    isUploading = true;
    uploadError = "";
    uploadResult = null;

    try {
      const formData = new FormData();
      formData.append("file", file);

      if (mode === "existing") {
        formData.append("vehicle_id", selectedVehicleId);
      } else {
        formData.append("name", vehicleName.trim());
        formData.append("vehicle_type", vehicleType);
        formData.append("base_price", basePrice.toString());
        if (tagline.trim()) formData.append("tagline", tagline.trim());
        if (description.trim()) formData.append("description", description.trim());
      }

      const res = await api.uploadVehicleModel(formData);
      uploadResult = res;
      dispatch("uploaded", res.vehicle);
      await loadModels();
    } catch (err) {
      uploadError = err.message || "Failed to upload 3D model to backend.";
    } finally {
      isUploading = false;
    }
  }

  // --- EDIT / WRITE VEHICLE ---
  function startEditVehicle(modelItem) {
    if (modelItem.vehicle) {
      const matched = vehicles.find((v) => v.id === modelItem.vehicle.id);
      editingVehicle = {
        id: modelItem.vehicle.id,
        name: matched?.name || modelItem.vehicle.name,
        vehicle_type: matched?.vehicle_type || modelItem.vehicle.vehicle_type,
        tagline: matched?.tagline || "",
        description: matched?.description || "",
        base_price: Number(matched?.base_price || modelItem.vehicle.base_price || 75000),
        model_file: modelItem.file_path,
      };
    } else {
      // Create new vehicle record linked to this unassigned model
      const stem = modelItem.filename.replace(/\.[^/.]+$/, "").replace(/[_-]+/g, " ");
      editingVehicle = {
        id: 0,
        name: stem.charAt(0).toUpperCase() + stem.slice(1),
        vehicle_type: "car",
        tagline: "Custom 3D model imported into repository",
        description: "",
        base_price: 80000,
        model_file: modelItem.file_path,
      };
    }
    saveVehicleError = "";
  }

  async function saveVehicleChanges() {
    if (!editingVehicle || !editingVehicle.name.trim()) {
      saveVehicleError = "Vehicle name is required.";
      return;
    }

    isSavingVehicle = true;
    saveVehicleError = "";

    try {
      if (editingVehicle.id > 0) {
        // Update existing vehicle
        await api.updateVehicle(editingVehicle.id, {
          name: editingVehicle.name.trim(),
          vehicle_type: editingVehicle.vehicle_type,
          tagline: editingVehicle.tagline.trim(),
          description: editingVehicle.description.trim(),
          base_price: editingVehicle.base_price,
          model_file: editingVehicle.model_file,
        });
      } else {
        // Create new vehicle platform linked to this 3D model
        await api.createVehicle({
          name: editingVehicle.name.trim(),
          vehicle_type: editingVehicle.vehicle_type,
          tagline: editingVehicle.tagline.trim(),
          description: editingVehicle.description.trim(),
          base_price: editingVehicle.base_price,
          model_file: editingVehicle.model_file,
        });
      }
      editingVehicle = null;
      dispatch("uploaded");
      await loadModels();
    } catch (err) {
      saveVehicleError = err.message || "Failed to update vehicle record.";
    } finally {
      isSavingVehicle = false;
    }
  }

  let isDeletingVehicle = false;
  async function handleDeleteVehicle() {
    if (!editingVehicle || editingVehicle.id <= 0) return;
    isDeletingVehicle = true;
    try {
      await api.deleteVehicle(editingVehicle.id, false);
      editingVehicle = null;
      dispatch("uploaded");
      await loadModels();
    } catch (err) {
      saveVehicleError = err.message || "Failed to delete vehicle record.";
    } finally {
      isDeletingVehicle = false;
    }
  }

  // --- DELETE MODEL ---
  function confirmDeleteModel(modelItem) {
    deletingModel = modelItem;
    deleteError = "";
  }

  async function executeDelete() {
    if (!deletingModel) return;
    isDeleting = true;
    deleteError = "";

    try {
      await api.deleteModel(deletingModel.filename);
      deletingModel = null;
      dispatch("uploaded");
      await loadModels();
    } catch (err) {
      deleteError = err.message || "Failed to delete 3D model file.";
    } finally {
      isDeleting = false;
    }
  }

  function launchConfigurator(targetVehicle) {
    if (targetVehicle) {
      dispatch("openConfigurator", targetVehicle);
    }
  }

  function launchSpecs(targetVehicle) {
    if (targetVehicle) {
      dispatch("openSpecs", targetVehicle);
    }
  }
</script>

<div class="uploader-container">
  <div class="uploader-panel raised">
    <!-- Header cluster with Quni4m crystal LED & Tab switch -->
    <header class="panel-header row">
      <div class="row header-left">
        <span class="led-dot online"></span>
        <h2 class="title mono">3D ASSET & VEHICLE REPOSITORY STUDIO</h2>
      </div>

      <!-- Action Tabs: UPLOAD vs REPOSITORY MANAGER -->
      <div class="tab-switch row">
        <button
          type="button"
          class="tab-btn mono"
          class:active={activeTab === "manage"}
          on:click={() => { activeTab = "manage"; loadModels(); }}
        >
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="3" y="3" width="7" height="7" />
            <rect x="14" y="3" width="7" height="7" />
            <rect x="14" y="14" width="7" height="7" />
            <rect x="3" y="14" width="7" height="7" />
          </svg>
          MANAGE 3D MODELS ({models.length})
        </button>
        <button
          type="button"
          class="tab-btn mono"
          class:active={activeTab === "upload"}
          on:click={() => { activeTab = "upload"; uploadError = ""; }}
        >
          <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
            <polyline points="17 8 12 3 7 8" />
            <line x1="12" y1="3" x2="12" y2="15" />
          </svg>
          UPLOAD NEW ASSET
        </button>
      </div>

      <button class="tb-close" on:click={() => dispatch("close")} title="Return to catalog">
        <svg width="14" height="14" viewBox="0 0 14 14" fill="currentColor">
          <path d="M2.2 1.2L1.2 2.2 6 7l-4.8 4.8 1 1L7 8l4.8 4.8 1-1L8 7l4.8-4.8-1-1L7 6 2.2 1.2z"/>
        </svg>
      </button>
    </header>

    <div class="panel-body">
      <!-- TAB 1: REPOSITORY MANAGER (READ, WRITE, DELETE) -->
      {#if activeTab === "manage"}
        <div class="manage-view">
          <div class="manage-toolbar row">
            <div class="row info-cluster">
              <span class="dim mono-label">BACKEND REPOSITORY:</span>
              <span class="mono repo-path">/media/models/</span>
              <span class="count-badge mono">{models.length} ASSETS REGISTERED</span>
            </div>

            <div class="row toolbar-actions">
              <button class="btn-refresh mono" on:click={loadModels} disabled={modelsLoading}>
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class:spin={modelsLoading}>
                  <polyline points="23 4 23 10 17 10" />
                  <path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10" />
                </svg>
                REFRESH
              </button>
              <button class="btn-new-upload mono" on:click={() => (activeTab = "upload")}>
                + UPLOAD NEW 3D MODEL
              </button>
            </div>
          </div>

          {#if modelsLoading}
            <div class="loading-state milled">
              <span class="spinner"></span>
              <span class="mono loading-text">SCANNING C++ BACKEND STORAGE REPOSITORY…</span>
            </div>
          {:else if modelsError}
            <div class="error-banner row">
              <span class="led-dot error"></span>
              <span class="error-text mono">{modelsError}</span>
            </div>
          {:else if models.length === 0}
            <div class="empty-state milled">
              <div class="empty-icon">
                <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                  <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z" />
                  <polyline points="3.27 6.96 12 12.01 20.73 6.96" />
                  <line x1="12" y1="22.08" x2="12" y2="12" />
                </svg>
              </div>
              <h3 class="mono empty-title">NO 3D VEHICLE MODELS FOUND IN REPOSITORY</h3>
              <p class="empty-desc">
                The storage directory <code>/media/models/</code> currently contains no 3D files. Upload your first model (.glb, .gltf, .obj) to initialize the repository.
              </p>
              <button class="btn-primary raised" on:click={() => (activeTab = "upload")}>
                UPLOAD FIRST 3D MODEL
              </button>
            </div>
          {:else}
            <!-- 3D Assets Grid / Table -->
            <div class="models-grid">
              {#each models as item (item.filename)}
                <div class="model-card milled">
                  <div class="card-top row">
                    <div class="row model-title-cluster">
                      <span class="format-badge mono">{item.extension.toUpperCase()}</span>
                      <span class="filename mono" title={item.filename}>{item.filename}</span>
                    </div>
                    <span class="filesize mono dim">{formatBytes(item.size_bytes)}</span>
                  </div>

                  <div class="card-mid">
                    {#if item.vehicle}
                      <div class="vehicle-assigned row">
                        <span class="led-dot online"></span>
                        <div class="assigned-details">
                          <span class="dim mono-label">ASSIGNED PLATFORM:</span>
                          <span class="assigned-name mono">{item.vehicle.name}</span>
                          <span class="assigned-type mono dim">[{item.vehicle.vehicle_type?.toUpperCase()}] • ${Number(item.vehicle.base_price || 0).toLocaleString()}</span>
                        </div>
                      </div>
                    {:else}
                      <div class="vehicle-unassigned row">
                        <span class="led-dot warn"></span>
                        <div class="assigned-details">
                          <span class="dim mono-label">STATUS:</span>
                          <span class="unassigned-text mono">UNASSIGNED TO VEHICLE</span>
                        </div>
                      </div>
                    {/if}
                  </div>

                  <div class="card-actions row">
                    {#if item.vehicle}
                      <button
                        type="button"
                        class="btn-action mono launch"
                        on:click={() => launchConfigurator(item.vehicle)}
                        title="Open in 3D Studio"
                      >
                        <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                          <polygon points="5 3 19 12 5 21 5 3" />
                        </svg>
                        3D STUDIO
                      </button>
                      <button
                        type="button"
                        class="btn-action mono edit"
                        on:click={() => startEditVehicle(item)}
                        title="Edit vehicle details"
                      >
                        <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                          <path d="M12 20h9" />
                          <path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z" />
                        </svg>
                        EDIT
                      </button>
                    {:else}
                      <button
                        type="button"
                        class="btn-action mono assign"
                        on:click={() => startEditVehicle(item)}
                        title="Create or assign vehicle for this model"
                      >
                        + ASSIGN VEHICLE
                      </button>
                    {/if}

                    <button
                      type="button"
                      class="btn-action mono delete"
                      on:click={() => confirmDeleteModel(item)}
                      title="Delete model from server"
                    >
                      <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <polyline points="3 6 5 6 21 6" />
                        <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" />
                      </svg>
                      DELETE
                    </button>
                  </div>
                </div>
              {/each}
            </div>
          {/if}
        </div>
      {/if}

      <!-- TAB 2: UPLOAD NEW ASSET -->
      {#if activeTab === "upload"}
        <div class="upload-view">
          <div class="mode-section row">
            <span class="section-label mono">TARGET DESTINATION:</span>
            <div class="mode-chips row">
              <button
                type="button"
                class="chip"
                class:on={mode === "new"}
                on:click={() => { mode = "new"; uploadError = ""; }}
              >
                NEW VEHICLE ENTRY
              </button>
              <button
                type="button"
                class="chip"
                class:on={mode === "existing"}
                on:click={() => { mode = "existing"; uploadError = ""; }}
              >
                ATTACH TO EXISTING VEHICLE
              </button>
            </div>
          </div>

          {#if uploadResult}
            <!-- Success Confirmation Display -->
            <div class="success-box milled">
              <div class="success-header row">
                <span class="led-dot online pulse"></span>
                <span class="success-title mono">3D MODEL UPLOADED & COMPILED TO BACKEND</span>
              </div>

              <p class="success-desc">
                The 3D asset was successfully transferred to the C++ server engine and saved to
                <code class="mono-code">{uploadResult.file_path}</code>.
                Catalog database records have been updated and synchronized with SQLite.
              </p>

              <div class="result-details milled">
                <div class="detail-row">
                  <span class="dim mono">VEHICLE:</span>
                  <span class="val mono">{uploadResult.vehicle?.name || vehicleName}</span>
                </div>
                <div class="detail-row">
                  <span class="dim mono">SERVER FILE:</span>
                  <span class="val mono">{uploadResult.filename}</span>
                </div>
                <div class="detail-row">
                  <span class="dim mono">PAYLOAD SIZE:</span>
                  <span class="val mono">{formatBytes(uploadResult.size_bytes || (file ? file.size : 0))}</span>
                </div>
              </div>

              <div class="action-row row">
                <button class="btn-primary raised" on:click={() => launchConfigurator(uploadResult.vehicle)}>
                  <span class="btn-gem"></span>
                  LAUNCH IN 3D STUDIO
                </button>
                <button class="btn-secondary" on:click={() => launchSpecs(uploadResult.vehicle)}>
                  VIEW SPECIFICATIONS
                </button>
                <button class="btn-ghost mono" on:click={resetUploadForm}>
                  UPLOAD ANOTHER MODEL
                </button>
                <button class="btn-ghost mono" on:click={() => { activeTab = "manage"; loadModels(); }}>
                  GO TO REPOSITORY MANAGER
                </button>
              </div>
            </div>
          {:else}
            <!-- Upload & Configuration Form -->
            <div class="form-grid">
              <!-- Left Column: Dropzone & File Summary -->
              <div class="drop-column">
                <div
                  class="dropzone milled"
                  class:dragging={isDragging}
                  class:has-file={!!file}
                  on:dragover={handleDragOver}
                  on:dragleave={handleDragLeave}
                  on:drop={handleDrop}
                  on:click={() => fileInputEl && fileInputEl.click()}
                  role="button"
                  tabindex="0"
                  on:keydown={(e) => (e.key === "Enter" || e.key === " ") && fileInputEl?.click()}
                >
                  <input
                    type="file"
                    bind:this={fileInputEl}
                    on:change={handleFileChange}
                    accept=".glb,.gltf,.obj"
                    style="display: none;"
                  />

                  <div class="drop-content">
                    <div class="icon-wrap">
                      <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6">
                        <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
                        <polyline points="17 8 12 3 7 8" />
                        <line x1="12" y1="3" x2="12" y2="15" />
                      </svg>
                    </div>

                    {#if file}
                      <div class="file-info">
                        <span class="file-name mono">{file.name}</span>
                        <span class="file-meta mono dim">{formatBytes(file.size)} • READY FOR C++ INGESTION</span>
                      </div>
                      <span class="btn-replace mono">CLICK OR DROP TO CHANGE FILE</span>
                    {:else}
                      <div class="drop-text">
                        <span class="primary-text mono">DRAG & DROP 3D MODEL HERE</span>
                        <span class="secondary-text dim mono">or click to browse local files</span>
                      </div>
                      <div class="format-badges row">
                        <span class="format-badge mono">.GLB</span>
                        <span class="format-badge mono">.GLTF</span>
                        <span class="format-badge mono">.OBJ</span>
                      </div>
                    {/if}
                  </div>
                </div>

                <div class="hardware-note milled">
                  <span class="dim mono-label">ENGINE SPECIFICATION:</span>
                  <p class="note-text">
                    Files are streamed directly into the native C++ engine via multipart upload, saved to <code>/media/models/</code>, and immediately served via zero-copy static file handlers with WebGL / Three.js viewport readiness.
                  </p>
                </div>
              </div>

              <!-- Right Column: Metadata Fields -->
              <div class="fields-column">
                {#if mode === "existing"}
                  <div class="field-group">
                    <label for="vehicle-select" class="mono label">TARGET VEHICLE IN CATALOG:</label>
                    <select id="vehicle-select" bind:value={selectedVehicleId} class="input milled">
                      {#each vehicles as v}
                        <option value={v.id}>
                          {v.name} [{v.vehicle_type.toUpperCase()}] — {v.slug}
                        </option>
                      {/each}
                    </select>
                    <span class="hint mono dim">Replaces or assigns the 3D model for the chosen vehicle.</span>
                  </div>
                {:else}
                  <div class="field-group">
                    <label for="v-name" class="mono label">VEHICLE NAME *</label>
                    <input
                      id="v-name"
                      type="text"
                      bind:value={vehicleName}
                      placeholder="e.g. Audi R8 LMS GT3, Phantom Speedboat..."
                      class="input milled"
                    />
                  </div>

                  <div class="row field-split">
                    <div class="field-group flex-1">
                      <label for="v-type" class="mono label">VEHICLE CATEGORY</label>
                      <select id="v-type" bind:value={vehicleType} class="input milled">
                        {#each vehicleTypes as vt}
                          <option value={vt.id}>{vt.label}</option>
                        {/each}
                      </select>
                    </div>

                    <div class="field-group flex-1">
                      <label for="v-price" class="mono label">BASE PRICE (USD)</label>
                      <input
                        id="v-price"
                        type="number"
                        bind:value={basePrice}
                        step="1000"
                        min="0"
                        class="input milled"
                      />
                    </div>
                  </div>

                  <div class="field-group">
                    <label for="v-tagline" class="mono label">TAGLINE</label>
                    <input
                      id="v-tagline"
                      type="text"
                      bind:value={tagline}
                      placeholder="e.g. High-downforce track monster engineered for endurance."
                      class="input milled"
                    />
                  </div>

                  <div class="field-group">
                    <label for="v-desc" class="mono label">DESCRIPTION</label>
                    <textarea
                      id="v-desc"
                      bind:value={description}
                      rows="3"
                      placeholder="Technical specs, powertrain, aerodynamics..."
                      class="input milled"
                    ></textarea>
                  </div>
                {/if}

                {#if uploadError}
                  <div class="error-banner row">
                    <span class="led-dot error"></span>
                    <span class="error-text mono">{uploadError}</span>
                  </div>
                {/if}

                <div class="submit-wrap">
                  <button
                    type="button"
                    class="btn-primary raised w-full"
                    class:disabled={!file || isUploading}
                    disabled={!file || isUploading}
                    on:click={submitUpload}
                  >
                    {#if isUploading}
                      <span class="spinner"></span>
                      <span class="mono">STREAMING 3D ASSET TO C++ BACKEND…</span>
                    {:else}
                      <span class="btn-gem"></span>
                      <span class="mono">UPLOAD 3D MODEL & REGISTER VEHICLE</span>
                    {/if}
                  </button>
                </div>
              </div>
            </div>
          {/if}
        </div>
      {/if}
    </div>
  </div>
</div>

<!-- MODAL: EDIT / WRITE VEHICLE DETAILS -->
{#if editingVehicle}
  <div class="modal-backdrop" on:click|self={() => (editingVehicle = null)}>
    <div class="modal-card raised">
      <header class="modal-header row">
        <div class="row header-left">
          <span class="led-dot online"></span>
          <h3 class="modal-title mono">EDIT VEHICLE SPECIFICATIONS</h3>
        </div>
        <button class="tb-close" on:click={() => (editingVehicle = null)}>✕</button>
      </header>

      <div class="modal-body">
        <div class="field-group">
          <label for="edit-name" class="mono label">VEHICLE NAME</label>
          <input id="edit-name" type="text" bind:value={editingVehicle.name} class="input milled" />
        </div>

        <div class="row field-split">
          <div class="field-group flex-1">
            <label for="edit-type" class="mono label">CATEGORY</label>
            <select id="edit-type" bind:value={editingVehicle.vehicle_type} class="input milled">
              {#each vehicleTypes as vt}
                <option value={vt.id}>{vt.label}</option>
              {/each}
            </select>
          </div>

          <div class="field-group flex-1">
            <label for="edit-price" class="mono label">BASE PRICE (USD)</label>
            <input id="edit-price" type="number" bind:value={editingVehicle.base_price} step="500" class="input milled" />
          </div>
        </div>

        <div class="field-group">
          <label for="edit-tag" class="mono label">TAGLINE</label>
          <input id="edit-tag" type="text" bind:value={editingVehicle.tagline} class="input milled" />
        </div>

        <div class="field-group">
          <label for="edit-desc" class="mono label">DESCRIPTION</label>
          <textarea id="edit-desc" bind:value={editingVehicle.description} rows="2" class="input milled"></textarea>
        </div>

        <div class="field-group">
          <label for="edit-model" class="mono label">3D MODEL ASSET PATH</label>
          <input id="edit-model" type="text" bind:value={editingVehicle.model_file} class="input milled mono-code-input" />
        </div>

        {#if saveVehicleError}
          <div class="error-banner row">
            <span class="led-dot error"></span>
            <span class="error-text mono">{saveVehicleError}</span>
          </div>
        {/if}
      </div>

      <footer class="modal-footer row">
        {#if editingVehicle.id > 0}
          <button
            type="button"
            class="btn-danger-text mono"
            on:click={handleDeleteVehicle}
            disabled={isDeletingVehicle}
            title="Delete this vehicle entry from database"
          >
            {isDeletingVehicle ? "DELETING…" : "DELETE VEHICLE"}
          </button>
        {/if}
        <div class="grow"></div>
        <button class="btn-ghost mono" on:click={() => (editingVehicle = null)}>CANCEL</button>
        <button class="btn-primary raised" on:click={saveVehicleChanges} disabled={isSavingVehicle}>
          {#if isSavingVehicle}
            <span class="spinner"></span>
            <span class="mono">WRITING TO SQLITE…</span>
          {:else}
            <span class="btn-gem"></span>
            <span class="mono">SAVE SPECIFICATIONS</span>
          {/if}
        </button>
      </footer>
    </div>
  </div>
{/if}

<!-- MODAL: DELETE CONFIRMATION -->
{#if deletingModel}
  <div class="modal-backdrop" on:click|self={() => (deletingModel = null)}>
    <div class="modal-card raised danger-card">
      <header class="modal-header row">
        <div class="row header-left">
          <span class="led-dot error"></span>
          <h3 class="modal-title mono danger-title">CONFIRM 3D ASSET DELETION</h3>
        </div>
        <button class="tb-close" on:click={() => (deletingModel = null)}>✕</button>
      </header>

      <div class="modal-body">
        <p class="danger-msg">
          Are you sure you want to permanently delete this 3D model asset from the C++ backend repository?
        </p>

        <div class="delete-details milled">
          <div class="detail-row">
            <span class="dim mono">FILENAME:</span>
            <span class="val mono danger-val">{deletingModel.filename}</span>
          </div>
          <div class="detail-row">
            <span class="dim mono">FILE SIZE:</span>
            <span class="val mono">{formatBytes(deletingModel.size_bytes)}</span>
          </div>
          {#if deletingModel.vehicle}
            <div class="detail-row">
              <span class="dim mono">ASSIGNED VEHICLE:</span>
              <span class="val mono">{deletingModel.vehicle.name} (model reference will be detached)</span>
            </div>
          {/if}
        </div>

        <p class="warn-note dim">
          ⚠️ This action removes the physical <code>.glb</code> file from <code>/media/models/</code> and updates SQLite records immediately.
        </p>

        {#if deleteError}
          <div class="error-banner row">
            <span class="led-dot error"></span>
            <span class="error-text mono">{deleteError}</span>
          </div>
        {/if}
      </div>

      <footer class="modal-footer row">
        <button class="btn-ghost mono" on:click={() => (deletingModel = null)}>CANCEL</button>
        <button class="btn-danger" on:click={executeDelete} disabled={isDeleting}>
          {#if isDeleting}
            <span class="spinner"></span>
            <span class="mono">DELETING FILE FROM DISK…</span>
          {:else}
            <span class="mono">PERMANENTLY DELETE ASSET</span>
          {/if}
        </button>
      </footer>
    </div>
  </div>
{/if}

<style>
  .uploader-container {
    width: 100%;
    max-width: 1080px;
    margin: 28px auto;
    padding: 0 16px;
  }

  .uploader-panel {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    overflow: hidden;
  }

  .panel-header {
    justify-content: space-between;
    padding: 10px 16px;
    border-bottom: 1px solid var(--line);
    background: var(--surface);
    gap: 12px;
  }

  .header-left {
    gap: 10px;
  }

  .title {
    font-size: 12px;
    letter-spacing: 0.1em;
    color: var(--text-hi);
    margin: 0;
  }

  .tab-switch {
    gap: 4px;
    background: var(--field);
    padding: 3px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .tab-btn {
    display: flex;
    align-items: center;
    gap: 6px;
    background: transparent;
    border: none;
    color: var(--text-dim);
    font-size: 10px;
    letter-spacing: 0.06em;
    padding: 5px 12px;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .tab-btn:hover {
    color: var(--text);
  }

  .tab-btn.active {
    background: var(--surface);
    color: var(--crystal-bright);
    border: 1px solid var(--crystal-blue);
    box-shadow: 0 0 8px rgba(0, 210, 255, 0.2);
  }

  .tb-close {
    background: transparent;
    border: none;
    color: var(--text-dim);
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 4px;
    border-radius: 4px;
  }

  .tb-close:hover {
    color: var(--crystal-bright);
    background: var(--field);
  }

  .panel-body {
    padding: 20px;
  }

  /* REPOSITORY MANAGER STYLES */
  .manage-toolbar {
    justify-content: space-between;
    margin-bottom: 18px;
    padding-bottom: 12px;
    border-bottom: 1px solid var(--line-soft);
    flex-wrap: wrap;
    gap: 12px;
  }

  .info-cluster {
    gap: 8px;
    align-items: center;
  }

  .repo-path {
    background: var(--surface);
    padding: 2px 6px;
    border-radius: 3px;
    color: var(--crystal-bright);
    font-size: 11px;
    border: 1px solid var(--line-soft);
  }

  .count-badge {
    font-size: 10px;
    color: var(--text-dim);
  }

  .toolbar-actions {
    gap: 8px;
  }

  .btn-refresh {
    display: flex;
    align-items: center;
    gap: 6px;
    background: var(--surface);
    border: 1px solid var(--line-soft);
    color: var(--text-dim);
    font-size: 10px;
    padding: 5px 10px;
    border-radius: var(--radius);
    cursor: pointer;
    transition: all 0.15s;
  }

  .btn-refresh:hover:not(:disabled) {
    color: var(--crystal-bright);
    border-color: var(--crystal-blue);
  }

  .btn-new-upload {
    background: linear-gradient(135deg, rgba(0, 119, 182, 0.3), rgba(0, 210, 255, 0.3));
    border: 1px solid var(--crystal-blue);
    color: var(--crystal-bright);
    font-size: 10px;
    padding: 5px 12px;
    border-radius: var(--radius);
    cursor: pointer;
    transition: all 0.15s;
  }

  .btn-new-upload:hover {
    background: rgba(0, 210, 255, 0.4);
    box-shadow: 0 0 10px rgba(0, 210, 255, 0.3);
  }

  .models-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
    gap: 14px;
  }

  .model-card {
    padding: 14px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
    background: var(--surface);
    display: flex;
    flex-direction: column;
    gap: 12px;
    transition: border-color 0.15s;
  }

  .model-card:hover {
    border-color: var(--line-bright);
  }

  .card-top {
    justify-content: space-between;
    align-items: flex-start;
    gap: 8px;
  }

  .model-title-cluster {
    gap: 8px;
    min-width: 0;
    flex: 1;
  }

  .filename {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-hi);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .filesize {
    font-size: 10px;
    flex-shrink: 0;
  }

  .card-mid {
    background: var(--field);
    padding: 8px 10px;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .vehicle-assigned,
  .vehicle-unassigned {
    gap: 8px;
    align-items: center;
  }

  .assigned-details {
    display: flex;
    flex-direction: column;
    gap: 2px;
    min-width: 0;
  }

  .assigned-name {
    font-size: 11px;
    color: var(--crystal-bright);
    font-weight: 600;
  }

  .assigned-type {
    font-size: 9px;
  }

  .unassigned-text {
    font-size: 10px;
    color: var(--led-amber);
  }

  .card-actions {
    gap: 6px;
    margin-top: auto;
  }

  .btn-action {
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 5px 8px;
    border-radius: 3px;
    font-size: 9px;
    letter-spacing: 0.06em;
    cursor: pointer;
    background: var(--field);
    border: 1px solid var(--line-soft);
    color: var(--text-dim);
    transition: all 0.15s;
  }

  .btn-action:hover {
    color: var(--text-hi);
    border-color: var(--line-bright);
  }

  .btn-action.launch {
    color: var(--crystal-bright);
    border-color: rgba(0, 210, 255, 0.4);
    background: rgba(0, 210, 255, 0.08);
  }

  .btn-action.launch:hover {
    background: rgba(0, 210, 255, 0.2);
    border-color: var(--crystal-blue);
    box-shadow: 0 0 8px rgba(0, 210, 255, 0.3);
  }

  .btn-action.assign {
    color: var(--crystal-bright);
    border-color: var(--crystal-blue);
  }

  .btn-action.delete:hover {
    color: #f87171;
    border-color: #ef4444;
    background: rgba(239, 68, 68, 0.1);
  }

  .loading-state,
  .empty-state {
    padding: 40px 20px;
    text-align: center;
    border-radius: var(--radius);
    border: 1px solid var(--line-soft);
  }

  .empty-icon {
    color: var(--text-dim);
    opacity: 0.5;
    margin-bottom: 12px;
  }

  .empty-title {
    font-size: 12px;
    letter-spacing: 0.08em;
    color: var(--text-hi);
  }

  .empty-desc {
    font-size: 11px;
    color: var(--text-dim);
    max-width: 440px;
    margin: 8px auto 20px;
    line-height: 1.5;
  }

  .empty-desc code {
    color: var(--crystal-bright);
  }

  /* UPLOAD SECTION STYLES */
  .mode-section {
    gap: 12px;
    margin-bottom: 20px;
    align-items: center;
  }

  .section-label {
    font-size: 10px;
    letter-spacing: 0.08em;
    color: var(--text-dim);
  }

  .mode-chips {
    gap: 6px;
  }

  .chip {
    background: var(--surface);
    border: 1px solid var(--line-soft);
    color: var(--text-dim);
    font-family: var(--font-mono);
    font-size: 10px;
    letter-spacing: 0.06em;
    padding: 5px 12px;
    border-radius: var(--radius);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .chip:hover {
    border-color: var(--line-bright);
    color: var(--text);
  }

  .chip.on {
    background: var(--field);
    border-color: var(--crystal-blue);
    color: var(--crystal-bright);
    box-shadow: 0 0 8px rgba(0, 210, 255, 0.2);
  }

  .form-grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 24px;
  }

  @media (max-width: 768px) {
    .form-grid {
      grid-template-columns: 1fr;
    }
  }

  .drop-column {
    display: flex;
    flex-direction: column;
    gap: 14px;
  }

  .dropzone {
    border: 2px dashed var(--line-bright);
    border-radius: var(--radius);
    padding: 36px 20px;
    display: flex;
    align-items: center;
    justify-content: center;
    text-align: center;
    cursor: pointer;
    background: var(--field);
    transition: all 0.2s ease;
    min-height: 240px;
  }

  .dropzone:hover,
  .dropzone.dragging {
    border-color: var(--crystal-bright);
    background: rgba(0, 210, 255, 0.04);
    box-shadow: inset 0 0 16px rgba(0, 210, 255, 0.1);
  }

  .dropzone.has-file {
    border-style: solid;
    border-color: var(--crystal-blue);
    background: rgba(0, 210, 255, 0.06);
  }

  .drop-content {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 12px;
  }

  .icon-wrap {
    color: var(--crystal-bright);
    opacity: 0.85;
  }

  .primary-text {
    font-size: 12px;
    letter-spacing: 0.08em;
    color: var(--text-hi);
  }

  .secondary-text {
    font-size: 10px;
    margin-top: 3px;
  }

  .format-badges {
    gap: 6px;
    margin-top: 4px;
  }

  .format-badge {
    background: var(--surface);
    border: 1px solid var(--line-soft);
    padding: 2px 6px;
    border-radius: 3px;
    font-size: 9px;
    color: var(--crystal-bright);
  }

  .file-name {
    font-size: 13px;
    font-weight: 600;
    color: var(--crystal-bright);
    word-break: break-all;
  }

  .file-meta {
    font-size: 10px;
    margin-top: 4px;
  }

  .btn-replace {
    font-size: 9px;
    color: var(--text-dim);
    text-decoration: underline;
    margin-top: 8px;
  }

  .hardware-note {
    padding: 12px;
    border-radius: var(--radius);
    background: var(--surface);
    border: 1px solid var(--line-soft);
  }

  .note-text {
    font-size: 11px;
    color: var(--text-dim);
    line-height: 1.5;
    margin-top: 6px;
    margin-bottom: 0;
  }

  .note-text code {
    color: var(--crystal-bright);
    background: var(--field);
    padding: 1px 4px;
    border-radius: 3px;
  }

  .fields-column {
    display: flex;
    flex-direction: column;
    gap: 14px;
  }

  .field-group {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }

  .field-split {
    gap: 12px;
  }

  .flex-1 {
    flex: 1;
  }

  .label {
    font-size: 10px;
    letter-spacing: 0.08em;
    color: var(--text-dim);
  }

  .hint {
    font-size: 10px;
  }

  .input {
    width: 100%;
    padding: 8px 12px;
    background: var(--field);
    border: 1px solid var(--line-soft);
    border-radius: var(--radius);
    color: var(--text-hi);
    font-family: inherit;
    font-size: 12px;
    outline: none;
    box-sizing: border-box;
    transition: border-color 0.15s ease;
  }

  .input:focus {
    border-color: var(--crystal-blue);
    box-shadow: 0 0 6px rgba(0, 210, 255, 0.2);
  }

  .mono-code-input {
    font-family: var(--font-mono);
    color: var(--crystal-bright);
  }

  select.input {
    cursor: pointer;
  }

  textarea.input {
    resize: vertical;
  }

  .submit-wrap {
    margin-top: 8px;
  }

  .w-full {
    width: 100%;
  }

  .btn-primary {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    padding: 10px 18px;
    background: linear-gradient(135deg, #0077b6, #00d2ff);
    color: #0c0f13;
    font-weight: 700;
    font-size: 11px;
    letter-spacing: 0.08em;
    border: 1px solid #70e1ff;
    border-radius: var(--radius);
    cursor: pointer;
    box-shadow: 0 0 12px rgba(0, 210, 255, 0.3);
    transition: all 0.2s ease;
  }

  .btn-primary:hover:not(:disabled) {
    box-shadow: 0 0 20px rgba(0, 210, 255, 0.5);
    filter: brightness(1.1);
  }

  .btn-primary:disabled {
    opacity: 0.45;
    cursor: not-allowed;
    filter: grayscale(0.5);
  }

  .btn-secondary {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 10px 18px;
    background: var(--surface);
    border: 1px solid var(--line-bright);
    color: var(--text-hi);
    font-family: var(--font-mono);
    font-size: 11px;
    letter-spacing: 0.08em;
    border-radius: var(--radius);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .btn-secondary:hover {
    border-color: var(--crystal-bright);
    color: var(--crystal-bright);
  }

  .btn-danger {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    padding: 9px 18px;
    background: #dc2626;
    border: 1px solid #ef4444;
    color: #ffffff;
    font-weight: 700;
    font-size: 11px;
    letter-spacing: 0.08em;
    border-radius: var(--radius);
    cursor: pointer;
    box-shadow: 0 0 12px rgba(220, 38, 38, 0.3);
    transition: all 0.15s;
  }

  .btn-danger-text {
    background: transparent;
    border: 1px solid rgba(239, 68, 68, 0.4);
    color: #f87171;
    padding: 6px 12px;
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 0.08em;
    border-radius: var(--radius);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .btn-danger-text:hover:not(:disabled) {
    background: rgba(220, 38, 38, 0.15);
    border-color: #ef4444;
    color: #ffffff;
  }

  .btn-danger:hover:not(:disabled) {
    background: #b91c1c;
    box-shadow: 0 0 16px rgba(239, 68, 68, 0.5);
  }

  .btn-ghost {
    background: transparent;
    border: none;
    color: var(--text-dim);
    font-size: 10px;
    letter-spacing: 0.06em;
    cursor: pointer;
    text-decoration: underline;
    padding: 6px 12px;
  }

  .btn-ghost:hover {
    color: var(--crystal-bright);
  }

  .btn-gem {
    width: 8px;
    height: 8px;
    background: #ffffff;
    border-radius: 2px;
    transform: rotate(45deg);
    box-shadow: 0 0 6px #ffffff;
  }

  .error-banner {
    gap: 8px;
    padding: 8px 12px;
    background: rgba(239, 68, 68, 0.12);
    border: 1px solid rgba(239, 68, 68, 0.4);
    border-radius: var(--radius);
    color: #fca5a5;
    font-size: 11px;
  }

  .led-dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--led-red);
    flex-shrink: 0;
  }

  .led-dot.online {
    background: var(--crystal-blue);
    box-shadow: 0 0 6px var(--crystal-blue);
  }

  .led-dot.online.pulse {
    animation: ledPulse 1.8s infinite;
  }

  .led-dot.warn {
    background: var(--led-amber);
    box-shadow: 0 0 6px var(--led-amber);
  }

  .led-dot.error {
    background: #ef4444;
    box-shadow: 0 0 6px #ef4444;
  }

  @keyframes ledPulse {
    0%, 100% { opacity: 1; transform: scale(1); }
    50% { opacity: 0.5; transform: scale(0.85); }
  }

  .success-box {
    padding: 24px;
    border-radius: var(--radius);
    border: 1px solid var(--line-bright);
    background: var(--field);
  }

  .success-header {
    gap: 8px;
    margin-bottom: 12px;
  }

  .success-title {
    font-size: 12px;
    letter-spacing: 0.08em;
    color: var(--crystal-bright);
    font-weight: 700;
  }

  .success-desc {
    font-size: 12px;
    color: var(--text);
    line-height: 1.6;
    margin-bottom: 16px;
  }

  .mono-code {
    background: var(--surface);
    padding: 2px 6px;
    border-radius: 3px;
    color: var(--crystal-bright);
    font-family: var(--font-mono);
  }

  .result-details,
  .delete-details {
    padding: 14px;
    border-radius: var(--radius);
    margin-bottom: 20px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    background: var(--surface);
  }

  .detail-row {
    display: flex;
    justify-content: space-between;
    font-size: 11px;
    gap: 8px;
  }

  .danger-val {
    color: #f87171;
  }

  .action-row {
    gap: 12px;
    flex-wrap: wrap;
    align-items: center;
  }

  .spinner {
    width: 12px;
    height: 12px;
    border: 2px solid rgba(0, 0, 0, 0.3);
    border-top-color: #000;
    border-radius: 50%;
    animation: spin 0.8s linear infinite;
  }

  .spin {
    animation: spin 0.8s linear infinite;
  }

  @keyframes spin {
    to { transform: rotate(360deg); }
  }

  /* MODAL STYLES */
  .modal-backdrop {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: rgba(12, 15, 19, 0.8);
    backdrop-filter: blur(4px);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 100;
    padding: 20px;
  }

  .modal-card {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    width: 100%;
    max-width: 520px;
    overflow: hidden;
    box-shadow: 0 12px 32px rgba(0, 0, 0, 0.6);
  }

  .danger-card {
    border-color: rgba(239, 68, 68, 0.4);
  }

  .modal-header {
    justify-content: space-between;
    padding: 12px 16px;
    background: var(--surface);
    border-bottom: 1px solid var(--line);
  }

  .modal-title {
    font-size: 11px;
    letter-spacing: 0.08em;
    color: var(--text-hi);
    margin: 0;
  }

  .danger-title {
    color: #f87171;
  }

  .danger-msg {
    font-size: 13px;
    color: var(--text-hi);
    line-height: 1.5;
    margin-bottom: 14px;
  }

  .warn-note {
    font-size: 10px;
    line-height: 1.5;
    margin-bottom: 14px;
  }

  .modal-body {
    padding: 18px;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  .modal-footer {
    justify-content: flex-end;
    gap: 10px;
    padding: 12px 18px;
    background: var(--surface);
    border-top: 1px solid var(--line);
  }
</style>
