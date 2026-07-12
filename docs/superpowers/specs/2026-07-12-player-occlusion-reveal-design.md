# Player Occlusion Material Cutout Design

## Goal

When a town object lies between the active player camera and the Paper2D hero, cut a soft circular opening only in the blocking components. The hero and the scene behind the blocker remain the normal main-camera render at native resolution, scale, perspective, and color. The effect must never introduce a black backing, blurred capture, enlarged character, duplicate sprite, or second-camera color mismatch.

## Decision and Superseded Prototype

Use main-camera material cutout rather than `SceneCaptureComponent2D` compositing. The SceneCapture prototype demonstrated three unacceptable properties with the modular Asian Village assets: hollow buildings exposed black interiors, render-target scaling softened the image, and independent projection could change perceived character scale. The prototype capture, render target, post-process material, and duplicate Paper2D visual remain disabled during transition and are removed after the material route passes acceptance.

## Asset Strategy

The imported `/Game/Asian_Village` library contains 127 material assets: 95 material instances and 32 materials. These vendor assets remain untouched.

Project-owned cutout copies live under `/Game/GameXXK/Materials/Occlusion/AsianVillage`. Only material families used by potential foreground blockers are copied: roofs, building walls and trim, trees, foliage, and props. Terrain, roads, water, sky, characters, and UI are excluded.

Each copied root material preserves its original textures, shading, WPO, normal, emissive, and stylization. Opaque roots become Masked only in the project-owned copy. Existing opacity masks are multiplied by the new reveal mask. Material-instance ancestry is reproduced against the copied roots so appearance remains equivalent.

## Cutout Material Contract

A shared material function and Material Parameter Collection provide:

- projected hero visual center in viewport UV;
- viewport aspect correction;
- normalized reveal radius, initially `0.18`;
- short feather band implemented with temporal or stable screen-space dither;
- per-component enable state supplied through a dynamic material parameter or custom primitive data.

Inside the circle the selected foreground component is fully clipped. Outside it the component renders normally. The transition band softens the edge without making the entire component translucent. Because the main camera performs the only scene render, player scale and sharpness are identical inside and outside the opening.

## Occluder Detection and State

`UGameXXKPlayerOcclusionRevealComponent` runs only on the Town screen. At approximately 20Hz it samples from the actual `PlayerCameraManager` toward the projected hero region using one center trace and eight perimeter samples.

Each sample may peel several blocking layers, but accepts only hit components whose depth is strictly before the hero plane. The hero, owned components, terrain near the hero, water, and components behind the hero are excluded. Eligible components are resolved through an original-material-to-cutout-material mapping.

When a component first becomes an eligible blocker, the controller caches every original material slot and applies the matching cutout material instances. A roughly `0.08s` activation delay rejects momentary hits. A roughly `0.22s` release hold prevents flicker at roof and foliage edges. On release, level transition, component destruction, or feature shutdown, every cached original material is restored.

If any slot lacks a valid cutout mapping, that slot keeps its original material and one diagnostic warning is logged. Missing mappings never produce a black, default, or checkerboard material.

## Performance

There is no secondary scene render or render target. Traces run at 20Hz rather than every frame. Material swaps occur only when the blocker set changes, not during every tick. Dynamic instances are cached per component and reused. Masked overdraw is restricted to currently blocking components.

Initial acceptance targets on the local RTX 4060 Laptop GPU are no measurable character/UI sharpness loss and less than 1ms added GPU cost in a representative blocked view. CPU trace and material update cost must remain below 0.25ms average in the town test area.

## Validation

- Clear view: no materials are swapped and the feature has no visible output.
- Roof, wall, tree/foliage, and prop obstruction: the hero and real main-camera scene are visible through the opening.
- Character scale, sprite pixels, camera perspective, and scene color are unchanged.
- No black backing, RenderTarget blur, duplicate sprite, or circular color patch appears.
- Multiple simultaneous blockers cut correctly; terrain and objects behind the hero remain rendered.
- Original material slots restore after clearing the obstruction, leaving the town, route map, battle, or main menu, and after PIE ends.
- Behavior remains stable at 720p, 1080p, 1440p, and non-16:9 editor viewports.
- Asian Village source materials and user-tuned camera, Paper2D, placed level, and character assets are unchanged.

## Non-goals

- Authoring interiors or backfaces for hollow imported buildings.
- Making an entire building globally translucent.
- Changing the camera transform, hero scale, or Paper2D render style.
- Applying the feature outside town gameplay.
