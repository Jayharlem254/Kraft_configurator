<script>
  import { T } from "@threlte/core";

  export let accentColor = "#8b5cf6";
</script>

<!--
  Simple stand-in geometry shaped a little like a low car silhouette, just so
  the viewer/lighting/HUD have something to render and orbit around. Replace
  by rendering the loaded glTF scene (see comment in Viewer3D.svelte) once a
  real Sketchfab-derived model has gone through the asset pipeline.
-->
<T.Group position.y={0.32}>
  <T.Mesh castShadow>
    <T.BoxGeometry args={[2.2, 0.45, 1]} />
    <T.MeshStandardMaterial color={accentColor} metalness={0.6} roughness={0.3} />
  </T.Mesh>
  <T.Mesh position.y={0.4} position.x={-0.1} castShadow>
    <T.BoxGeometry args={[1.3, 0.4, 0.85]} />
    <T.MeshStandardMaterial color="#1a1822" metalness={0.4} roughness={0.4} />
  </T.Mesh>
  {#each [-0.85, 0.85] as x}
    {#each [-0.45, 0.45] as z}
      <T.Mesh position.x={x} position.z={z} position.y={-0.18} rotation.z={Math.PI / 2} castShadow>
        <T.CylinderGeometry args={[0.22, 0.22, 0.22, 24]} />
        <T.MeshStandardMaterial color="#0c0b12" metalness={0.2} roughness={0.7} />
      </T.Mesh>
    {/each}
  {/each}
</T.Group>
