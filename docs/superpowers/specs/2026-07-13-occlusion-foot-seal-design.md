# Player Occlusion Circle with Hero-Silhouette Gate

## Goal

Keep the existing circular player-occlusion range, but only remove foreground pixels that overlap the hero's actual on-screen silhouette. A rail, wall, or stair that merely lies inside the circle must remain visible.

## Decision

Use the hero Paper2D visual's Custom Stencil value as a per-pixel silhouette gate in the existing project-owned cutout materials. The cutout remains `circle × foreground-depth × hero-stencil`; the circular scope stays unchanged, but pixels outside the hero silhouette—including the wall below the feet—retain their original material.

## Constraints

- Keep the narrow `OcclusionDetectionRadiusNormalized` separate from the `RevealRadiusNormalized` visual halo.
- Do not change vendor `Asian_Village` assets.
- Preserve source Masked and Translucent material connections.
- Do not change character sprites, camera transforms, or placed level geometry.

## Acceptance

- A rail or wall below the hero's feet is rendered normally even when it falls inside the circular occlusion range.
- A foreground roof or wall that overlaps the hero's screen-space silhouette can still open to reveal the full-color hero.
- The existing material-depth gate and source opacity mask remain in the material graph.
