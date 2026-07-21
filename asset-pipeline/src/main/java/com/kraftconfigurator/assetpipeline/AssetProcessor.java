package com.kraftconfigurator.assetpipeline;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.kraftconfigurator.assetpipeline.model.AssetManifest;
import com.kraftconfigurator.assetpipeline.model.AssetManifestEntry;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.stream.Stream;

/**
 * Entry point for the asset pipeline.
 *
 * Expects raw Sketchfab downloads to live in subfolders of raw-assets/, named
 * "<vehicleType>__<slug>", e.g.:
 *
 *   raw-assets/
 *     car__velora-gt/
 *       velora-gt.fbx
 *       textures/...
 *     boat__riptide-x/
 *       riptide-x.glb
 *
 * Run from the asset-pipeline/ directory:
 *
 *   java -jar target/asset-pipeline-0.1.0.jar
 *
 * Output:
 *   - Optimized .glb files copied/written into ../backend/media/models/
 *   - output/manifest.json describing what was processed, ready for
 *     `python manage.py import_manifest output/manifest.json` on the Django side.
 */
public class AssetProcessor {

    private static final Set<String> VALID_VEHICLE_TYPES = Set.of("car", "boat", "bike");
    private static final List<String> SOURCE_EXTENSION_PRIORITY = List.of(".glb", ".gltf", ".fbx", ".obj");

    private final Path rawAssetsDir;
    private final Path mediaModelsDir;
    private final Path manifestOutputPath;
    private final Path blenderScriptPath;

    public AssetProcessor(Path rawAssetsDir, Path mediaModelsDir, Path manifestOutputPath, Path blenderScriptPath) {
        this.rawAssetsDir = rawAssetsDir;
        this.mediaModelsDir = mediaModelsDir;
        this.manifestOutputPath = manifestOutputPath;
        this.blenderScriptPath = blenderScriptPath;
    }

    public static void main(String[] args) throws IOException {
        Path baseDir = Paths.get(".");
        AssetProcessor processor = new AssetProcessor(
                baseDir.resolve("raw-assets"),
                baseDir.resolve("..").resolve("backend").resolve("media").resolve("models"),
                baseDir.resolve("output").resolve("manifest.json"),
                baseDir.resolve("scripts").resolve("convert_to_glb.py")
        );
        processor.run();
    }

    public void run() throws IOException {
        if (!Files.isDirectory(rawAssetsDir)) {
            System.err.println("No raw-assets directory found at " + rawAssetsDir.toAbsolutePath());
            System.err.println("Create it and drop Sketchfab downloads into subfolders named <type>__<slug>.");
            return;
        }
        Files.createDirectories(mediaModelsDir);
        Files.createDirectories(manifestOutputPath.getParent());

        AssetManifest manifest = new AssetManifest();
        manifest.generatedAt = Instant.now().toString();

        try (Stream<Path> entries = Files.list(rawAssetsDir)) {
            List<Path> assetDirs = entries.filter(Files::isDirectory).sorted().toList();
            if (assetDirs.isEmpty()) {
                System.out.println("raw-assets/ has no subfolders yet — nothing to process.");
            }
            for (Path assetDir : assetDirs) {
                manifest.assets.add(processAssetFolder(assetDir));
            }
        }

        writeManifest(manifest);
        printSummary(manifest);
    }

    private AssetManifestEntry processAssetFolder(Path assetDir) {
        String folderName = assetDir.getFileName().toString();
        AssetManifestEntry entry = new AssetManifestEntry();
        entry.processedAt = Instant.now().toString();

        String[] parts = folderName.split("__", 2);
        if (parts.length != 2 || !VALID_VEHICLE_TYPES.contains(parts[0])) {
            entry.slug = folderName;
            entry.status = "skipped";
            entry.notes = "Folder name must be <type>__<slug> where type is one of "
                    + VALID_VEHICLE_TYPES + ". Got: \"" + folderName + "\".";
            System.out.println("[skip] " + folderName + " — " + entry.notes);
            return entry;
        }

        entry.vehicleType = parts[0];
        entry.slug = parts[1];
        entry.name = humanize(parts[1]);

        Path sourceFile = findSourceFile(assetDir);
        if (sourceFile == null) {
            entry.status = "skipped";
            entry.notes = "No recognized mesh file (.glb/.gltf/.fbx/.obj) found in " + assetDir;
            System.out.println("[skip] " + folderName + " — " + entry.notes);
            return entry;
        }
        entry.sourceFile = assetDir.relativize(sourceFile).toString();

        Path outputGlb = mediaModelsDir.resolve(entry.slug + ".glb");
        String extension = extensionOf(sourceFile);

        try {
            if (extension.equals(".glb") || extension.equals(".gltf")) {
                // Already in a web-ready format — copy through as-is. (For .gltf with
                // separate .bin/textures you'd want to copy the whole folder instead;
                // left as a TODO since Sketchfab "glTF" downloads vary in layout.)
                Files.copy(sourceFile, outputGlb, java.nio.file.StandardCopyOption.REPLACE_EXISTING);
                entry.status = "ok";
                entry.notes = "Copied through without conversion (already glTF/glb).";
            } else {
                ConversionResult result = convertWithBlender(sourceFile, outputGlb);
                if (result.success()) {
                    entry.status = "ok";
                    entry.vertexCount = result.vertexCount();
                    entry.faceCount = result.faceCount();
                } else {
                    entry.status = "failed";
                    entry.notes = result.errorMessage();
                }
            }
        } catch (IOException e) {
            entry.status = "failed";
            entry.notes = "I/O error: " + e.getMessage();
        }

        entry.glbPath = entry.status.equals("ok") ? "models/" + entry.slug + ".glb" : null;
        System.out.println("[" + entry.status + "] " + folderName);
        return entry;
    }

    private Path findSourceFile(Path assetDir) {
        try (Stream<Path> files = Files.walk(assetDir, 2)) {
            List<Path> candidates = files.filter(Files::isRegularFile).toList();
            for (String ext : SOURCE_EXTENSION_PRIORITY) {
                var match = candidates.stream()
                        .filter(p -> extensionOf(p).equals(ext))
                        .min(Comparator.comparing(Path::toString));
                if (match.isPresent()) {
                    return match.get();
                }
            }
        } catch (IOException e) {
            System.err.println("Could not scan " + assetDir + ": " + e.getMessage());
        }
        return null;
    }

    private ConversionResult convertWithBlender(Path inputFile, Path outputGlb) {
        if (!Files.isReadable(blenderScriptPath)) {
            return ConversionResult.failure("Blender script not found at " + blenderScriptPath);
        }

        ProcessBuilder pb = new ProcessBuilder(
                "blender",
                "--background",
                "--factory-startup",
                "--python", blenderScriptPath.toString(),
                "--",
                inputFile.toAbsolutePath().toString(),
                outputGlb.toAbsolutePath().toString()
        );
        pb.redirectErrorStream(true);

        try {
            Process process = pb.start();
            Integer vertexCount = null;
            Integer faceCount = null;
            StringBuilder fullLog = new StringBuilder();

            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null) {
                    fullLog.append(line).append('\n');
                    if (line.startsWith("STATS_JSON:")) {
                        var mapper = new ObjectMapper();
                        var stats = mapper.readTree(line.substring("STATS_JSON:".length()));
                        vertexCount = stats.get("vertexCount").asInt();
                        faceCount = stats.get("faceCount").asInt();
                    }
                }
            }

            int exitCode = process.waitFor();
            if (exitCode != 0 || !Files.exists(outputGlb)) {
                return ConversionResult.failure(
                        "Blender exited with code " + exitCode + ". Last output:\n"
                                + lastLines(fullLog.toString(), 15));
            }
            return ConversionResult.success(vertexCount, faceCount);

        } catch (IOException e) {
            return ConversionResult.failure(
                    "Could not run `blender` (is it installed and on PATH?): " + e.getMessage());
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return ConversionResult.failure("Conversion was interrupted.");
        }
    }

    private void writeManifest(AssetManifest manifest) throws IOException {
        ObjectMapper mapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);
        mapper.writeValue(manifestOutputPath.toFile(), manifest);
    }

    private void printSummary(AssetManifest manifest) {
        long ok = manifest.assets.stream().filter(a -> "ok".equals(a.status)).count();
        long skipped = manifest.assets.stream().filter(a -> "skipped".equals(a.status)).count();
        long failed = manifest.assets.stream().filter(a -> "failed".equals(a.status)).count();
        System.out.printf(
                "%nDone. %d ok, %d skipped, %d failed. Manifest written to %s%n",
                ok, skipped, failed, manifestOutputPath.toAbsolutePath()
        );
    }

    private static String extensionOf(Path path) {
        String name = path.getFileName().toString().toLowerCase(Locale.ROOT);
        int dot = name.lastIndexOf('.');
        return dot == -1 ? "" : name.substring(dot);
    }

    private static String humanize(String slug) {
        String[] words = slug.split("-");
        StringBuilder sb = new StringBuilder();
        for (String word : words) {
            if (word.isEmpty()) continue;
            if (sb.length() > 0) sb.append(' ');
            sb.append(Character.toUpperCase(word.charAt(0))).append(word.substring(1));
        }
        return sb.toString();
    }

    private static String lastLines(String text, int n) {
        String[] lines = text.split("\n");
        int start = Math.max(0, lines.length - n);
        return String.join("\n", List.of(lines).subList(start, lines.length));
    }

    private record ConversionResult(boolean success, Integer vertexCount, Integer faceCount, String errorMessage) {
        static ConversionResult success(Integer vertexCount, Integer faceCount) {
            return new ConversionResult(true, vertexCount, faceCount, null);
        }
        static ConversionResult failure(String message) {
            return new ConversionResult(false, null, null, message);
        }
    }
}
