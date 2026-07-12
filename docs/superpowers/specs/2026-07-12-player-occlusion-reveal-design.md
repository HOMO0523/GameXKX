# Player Occlusion Reveal Design

## Goal

When any foreground object blocks the player, show the full-color player and the real scene around and behind the player through a soft circular window. Buildings, roofs, trees, foliage, props, and any other blocking primitive may be removed inside the reveal. The normal view outside the circle remains unchanged.

## Rendering Architecture

The main camera renders normally. A hidden `SceneCaptureComponent2D` mirrors the active gameplay camera into a dedicated render target. While the reveal is active, the capture hides every primitive component identified as foreground occlusion and renders the unobstructed scene. A post-process material composites the capture into the main view only inside a soft screen-space circle centered on the projected player visual center.

This is a true scene reveal: the circle contains the player, ground, NPCs, props, water, and background geometry that should be visible after the foreground blockers are removed. It is not a player silhouette, sprite-only overlay, tinted rectangle, or modification of vendor materials.

## Occluder Detection

`UGameXXKPlayerOcclusionRevealComponent` samples the active camera toward the player and the intended reveal region. A center sphere trace plus eight perimeter rays approximates the circular footprint. Every blocking primitive before the player region is collected and hidden only from the secondary capture.

- All blocking object categories are eligible, including buildings, trees, foliage, and props.
- The player, owned components, UI, ground behind the player, and actors not between the camera and reveal region remain visible.
- Components rather than entire modular buildings are hidden where the capture API permits it.
- Activation waits roughly 0.08 seconds; release holds and fades for roughly 0.22 seconds to prevent edge flicker.

## Composite

The post-process material samples the unobstructed render target and the current scene color. A viewport-relative circular mask blends from capture color at the center to normal scene color at the edge.

- Radius: initially 7% of the shorter viewport dimension.
- Feather: initially 20% of the circle radius.
- Center: projected Paper2D visual center, not capsule feet.
- Color: unchanged captured scene color; no glow, desaturation, or grey backing.
- The material is active only in the town gameplay screen while an occluder set is non-empty.

## Performance

The secondary capture is disabled when unobstructed. While active it uses a 512×512 render target initially and updates at 30Hz; fast camera/player movement may temporarily update every frame. The first acceptance target is an added GPU cost below 4ms on the local RTX 4060 Laptop GPU. No building dynamic material instances or vendor asset edits are allowed.

## Transition from the Prototype

The existing trace timing and hysteresis may be retained. The sprite-only reveal material, grey circular backing, and always-on-top Paper2D duplicate are not the final renderer. The duplicate may remain hidden as a diagnostic fallback until the SceneCapture composite passes visual acceptance, then should be removed or disabled by default.

## Validation

- Unobstructed player: no secondary capture and no composite.
- Roof, wall, tree, foliage, and prop obstruction: circle shows the player and genuine scene behind the blocker.
- Outside the circle: the blocker remains fully visible.
- Multiple simultaneous blockers are removed from the capture.
- Circle center and radius remain stable at 720p, 1080p, and 1440p.
- Town panels, route map, battle, and main menu disable the capture and composite.
- No edits to Asian Village, foliage, bridge, or prop materials.
- Existing camera, Paper2D, collision, NPC, save, and map-flow tests remain valid.

## Non-goals

- Making the whole building transparent.
- Permanently hiding foreground objects.
- Applying the effect outside the town gameplay screen.
- Replacing the main camera or changing its user-tuned transform.
