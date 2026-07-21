package com.kraftconfigurator.assetpipeline.model;

import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * One processed asset, ready to be imported into the Django catalog via
 * `python manage.py import_manifest manifest.json`.
 *
 * Field names match what catalog/management/commands/import_manifest.py expects.
 */
@JsonInclude(JsonInclude.Include.NON_NULL)
public class AssetManifestEntry {

    @JsonProperty("slug")
    public String slug;

    @JsonProperty("name")
    public String name;

    @JsonProperty("vehicleType")
    public String vehicleType;

    /** Path to the .glb, relative to the Django MEDIA_ROOT (e.g. "models/velora-gt.glb"). */
    @JsonProperty("glbPath")
    public String glbPath;

    /** Original raw download this was generated from, for traceability. */
    @JsonProperty("sourceFile")
    public String sourceFile;

    /** Optional — only set if you fill in real prices before import; otherwise defaults to 0. */
    @JsonProperty("basePrice")
    public Double basePrice;

    /** Vertex/face counts when the conversion step reports them; null if unknown. */
    @JsonProperty("vertexCount")
    public Integer vertexCount;

    @JsonProperty("faceCount")
    public Integer faceCount;

    @JsonProperty("processedAt")
    public String processedAt;

    @JsonProperty("status")
    public String status; // "ok" | "skipped" | "failed"

    @JsonProperty("notes")
    public String notes;
}
