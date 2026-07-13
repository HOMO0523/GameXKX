# Player Occlusion Foot Seal

## Goal

Keep the wall, rail, and stair material visible at and below the hero's projected feet while retaining the existing depth-gated visibility opening around the hero's upper body.

## Decision

Use a screen-space foot seal in the existing project-owned cutout materials. The hero component publishes the exact projected foot Y coordinate; the material only applies its circle/depth reveal above that coordinate. The cutout terminates behind the boots with no below-feet padding or feather.

## Constraints

- Keep the narrow `OcclusionDetectionRadiusNormalized` separate from the `RevealRadiusNormalized` visual halo.
- Do not change vendor `Asian_Village` assets.
- Preserve source Masked and Translucent material connections.
- Do not change character sprites, camera transforms, or placed level geometry.

## Acceptance

- A rail or wall below the hero's feet is rendered normally, with no black opening below the boots.
- A foreground roof or wall above/alongside the hero can still open to reveal the full-color hero.
- The existing material-depth gate and source opacity mask remain in the material graph.
