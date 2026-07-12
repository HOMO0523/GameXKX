# Player Occlusion Reveal Design

## Goal

Keep the full-color Paper2D player readable when opaque town buildings stand between the active camera and the player. The reveal appears only while occluded, is constrained to a soft circular region around the player, and does not alter Asian Village building materials.

## Rendering Design

`AGameXXKHeroCharacter` keeps its existing `Visual` flipbook component and gains a non-colliding `OccludedVisual` flipbook component. The reveal component copies the active flipbook, playback position, facing, transform, tint, and scale from `Visual`.

`OccludedVisual` uses a dedicated unlit Paper2D material that can render through opaque scene depth. Screen-position math clips the duplicate to a resolution-independent circle centered on the player's visual center. A soft radial falloff removes the edge without creating a science-fiction glow. The center preserves the original sprite color; an optional low-strength dark-teal ink edge may be exposed as a parameter but defaults to subtle.

The normal component continues to render visible pixels. The duplicate supplies the player image only while runtime occlusion detection is active. It is hidden otherwise, preventing double-bright rendering.

## Occlusion Detection

A small runtime component owned by the player samples the active gameplay camera to the player visual center with a sphere trace at 15–20 Hz. The trace:

- ignores the player and owned components;
- tests world-static occluders;
- can require an opt-in component tag for fine control after the first validation pass;
- uses a radius of roughly 30–45 Unreal units to avoid single-ray edge flicker;
- activates after 0.06–0.10 seconds of continuous obstruction;
- fades out over 0.18–0.25 seconds after visibility returns.

The feature runs only in the town gameplay screen with a valid local player camera. It is disabled in menus, route maps, and battle presentation.

## Parameters

- Radius: viewport-relative, initially 6–8% of the shorter viewport dimension.
- Feather: initially 20% of the radius.
- Circle center: player visual center rather than capsule origin or feet.
- Reveal opacity: 1.0 at the center, smoothly approaching 0.0 at the edge.
- Color: unchanged full-color sprite.

## Performance

The design adds one duplicate sprite draw only while the player is occluded and one throttled sphere trace. It adds no building dynamic material instances, no per-building fade updates, and no Scene Capture. It should not change Lumen, Virtual Shadow Map, or building draw-call behavior.

## Validation

- No reveal while the player is unobstructed.
- Full-color reveal appears through an opaque roof, wall, and pillar.
- Normal and reveal flipbooks remain on the same animation frame and facing.
- Partial occlusion does not brighten already visible sprite pixels noticeably.
- Reveal transitions do not flicker while passing thin modular pillars.
- Circle position and radius remain correct at 720p, 1080p, and 1440p.
- Opening town panels, entering the route map, or entering battle disables the reveal.
- Existing camera arm length, camera pitch, character collision, and Paper2D tuning remain unchanged.

## Non-goals

- Cutting a real hole in building geometry.
- Fading whole buildings or roof modules.
- Revealing NPCs in the first version.
- Editing vendor materials or meshes.
