package com.kraftconfigurator.assetpipeline.model;

import com.fasterxml.jackson.annotation.JsonProperty;

import java.util.ArrayList;
import java.util.List;

public class AssetManifest {

    @JsonProperty("generatedAt")
    public String generatedAt;

    @JsonProperty("assets")
    public List<AssetManifestEntry> assets = new ArrayList<>();
}
