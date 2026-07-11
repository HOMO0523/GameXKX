# Qingshan PCG Dress B1 Design

## Status

Approved by the user on 2026-07-11. This document defines the first enriched, style-aware town scene pass after the deterministic B0R whitebox.

## Goal

Create an isolated `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1` map that is compositionally rich enough to judge the town as a place before all final assets exist. Preserve B0R as the deterministic layout source, use QuickRoad for real Landscape/road geometry and terrain fitting, and keep bridges, river, buildings, props, vegetation, mountains, cameras, and gameplay anchors owned by project systems.

## Hard Safety Boundaries

- Work only in the root project on `main`; do not create a worktree.
- Do not use UnrealBridge. Run editor automation through the project UE 5.8 MCP scripts.
- Do not save `/Game/GameXXK/Maps/L_QingshanInn` or mutate B0R in place.
- Duplicate B0R into B1. On the first B1 setup only, the assembler may remove cloned actors carrying `QingshanB0RManaged` after proving the current package is B1 and the actor label is in the fixed B0R generated-label allowlist. Normal reruns may mutate only actors tagged `QingshanB1Managed` or assets under `/Game/GameXXK/Environment/TownPCG/B1/`.
- Preserve player, quest, PaperZD, camera, HD2D plane, and manually tuned source-map actors.
- Save dirty packages through MCP before any editor restart or cold compile.
- Do not claim QuickRoad integration from tags alone. Require an actual `MainRoadNetwork`/QuickRoad component or baked QuickRoad asset in B1 evidence.
- Record raw SHA-256 baselines for `L_QingshanInn.umap`, `L_Qingshan_PCG_Whitebox_B0R.umap`, and `QingshanWhiteboxB0R.json`. Every mutating phase compares them before and after; clean-but-saved source mutations are failures.

## Architecture and Ownership

### Deterministic layout source

B0R JSON and seed remain the canonical topology. B1 is a validated overlay that expands scene dressing without replacing the B0R anchors, road/river semantics, exclusions, or camera intent.

All B0R and B1 layout coordinates are `QingshanInn_TownExit` anchor-local. A single assembler transform converts local records to world space. Landscape centre, roads, buildings, river, props, vegetation, mountains, and cameras may not mix local and world coordinates.

### QuickRoad

QuickRoad owns:

- the B1 Landscape creation contract;
- one real road network per road-width class;
- consolidated road intersections;
- road-to-Landscape and road-to-layout-mesh influence;
- road-edge extraction for PCG exclusion/dressing;
- final road StaticMesh bake and split.

QuickRoad does not own the bridge, river/water, buildings, props, Paper2D vegetation, mountain ring, gameplay anchors, cameras, or deterministic seed.

### Project dressing system

The project B1 config and assembler own stable IDs and placement for:

- 26 building plots: 1 large, 7 medium, and 18 small;
- 6 building archetype IDs and 4 roof palettes;
- bridge, south dock, and river actors;
- reusable sign, lantern, banner, fence, crate, stall, well, rock, cart, and dock-post props;
- 100 vegetation instances split into near animated, mid staggered, and far static populations;
- 24 perimeter mountain/backdrop instances;
- four fixed evaluation cameras.

Final asset replacement is by stable archetype/actor ID, so layout work does not need to be repeated when new Blender or Tripo assets arrive.

The deliverable scene uses real UE PCG graphs/components, not Python-spawned category substitutes. Six building proxy meshes, repeated prop meshes, far vegetation cards, and perimeter mountain meshes are spawned through B1 PCG graphs from deterministic points. Python authors/validates inputs and triggers editor generation. The independent bridge, river, cameras, and 30 near Paper2D flipbook plants remain explicit project actors because they have unique semantics or animation state.

## Landscape Contract

- Resolution: 505 x 505 vertices.
- Topology: 63 quads per section, 2 x 2 sections per component, 4 x 4 components.
- XY scale: 100 cm per quad; coverage approximately 504 x 504 metres.
- Centre: derived from B0R world bounds, approximately `(-15500, 0)` cm.
- Base elevation: broad, low-frequency terrain only. Do not use high-frequency erosion because the approved visual direction rejects fragmented/noisy terrain.
- Generate a deterministic 16-bit 505 x 505 base heightmap from the four B0R terrain zones, a broad river trench/bank profile, and low-amplitude low-frequency variation. Import it as the base Landscape content before applying the road edit layer.
- Landscape Edit Layers must be enabled explicitly. QuickRoad does not create or enable them itself.
- Use a dedicated unlocked `QR_B1_Roads` edit layer and make it active before conform/road-influence operations.
- Clear the `QR_B1_Roads` height contribution before every infrastructure rebuild, then apply road influence once. Repeated builds must produce the same edit-layer height digest.
- Perimeter mountains remain separate 3D/2.5D dressing; do not attempt to generate the mountain ring from terrain noise.

## Road and Crossing Contract

- Preserve the semantic widths 800, 450, and 400 cm.
- Generate each width group with a different QuickRoad tag so the plugin does not overwrite per-spline widths during first generation.
- Rebuild intersections with a combined tag expression.
- Split the main road at the bridge approaches. Do not generate a QuickRoad deck across the river and do not apply road influence to the riverbed beneath the bridge.
- The bridge and dock remain independent project actors aligned to their fixed anchors.
- The river remains an independent spline/ribbon and stylized-water material system.
- After visual acceptance, bake QuickRoad roads into 5,000 cm StaticMesh chunks with simple collision. Disable Nanite on chunks below 50,000 triangles; enable it only on larger chunks, record the decision per chunk, and preserve one fallback LOD for every non-Nanite chunk.
- Extract QuickRoad road-edge splines after the network is rebuilt. B1 PCG generation records these tagged edges as exclusion inputs and validation proves all generated category points remain outside the required road corridor.

## Building Composition Contract

- Retain all 16 accepted B0R plots and add 10 deterministic plots without overlapping road, river, bridge, or other building exclusion zones.
- Use 6 distinct silhouette/archetype IDs. No camera should show more than three consecutive buildings using the same archetype.
- Use at least four roof palettes: warm orange, muted teal, indigo blue, and ochre.
- Entrances generally face the nearest road, but setbacks, yaw, height, and lateral offset must vary. Avoid rows and mirrored street symmetry.
- One large landmark anchors the core. Smaller structures form irregular approach, core, bridge, and south/dock clusters.
- Windows use chunky dark timber frames with warm ivory/ochre rice-paper infill; black void windows are forbidden.
- Reusable lanterns, signs, banners, awnings, fences, and similar attachments remain separate Actor assets.

## Prop and Vegetation Contract

- Repeated props use instancing or shared actor blueprints and shared materials.
- Buildings, repeated props, far plant cards, and mountains are PCG StaticMeshSpawner outputs, grouped by archetype/mesh so repeated instances use ISM/HISM components.
- Prop density is clustered around storefronts, bridge approaches, town-core pauses, and dock work areas; do not scatter uniformly.
- Paper2D vegetation uses a 4-6 frame ping-pong sway cycle.
- Near vegetation may animate; mid vegetation uses staggered start frames; far vegetation uses static extracted frames.
- Initial cap remains 100 vegetation instances. Prefer roughly 30 animated near instances and 70 static/staggered mid/far instances.
- Vegetation, props, and buildings must respect road/river/footprint exclusions.

## Visual Direction

- Chinese ink-cartoon town with chunky, readable silhouettes and mild humorous exaggeration.
- Broad shapes and controlled asymmetry take priority over small tiled or dotted detail.
- Daylight base: warm paper windows, warm plaster, dark timber, cool teal/blue shadows, restrained fog.
- The B1 proxy pass may use reusable stylized proxy actors, but every proxy must read as its intended category; generic cubes/cones alone are not acceptable final B1 evidence.

## Performance Budget

- `runtime_generation` stays false.
- QuickRoad is an editor-time source and must be baked for the deliverable pass.
- Keep building count at or below 26, vegetation at or below 100, mountains at 24, crossings at 2.
- Use shared Toon BSDF materials and material instances.
- Landmark/high-detail buildings may use Nanite; small buildings and props require simpler meshes and simple collision.
- Only near Paper2D vegetation animates. Far cards are static and aggressively culled.
- Evidence records actor/component counts, road triangle/chunk counts, editor memory, and a short PIE smoke result.
- Evidence also records generated PCG graph paths, seed, point/output counts, road-edge spline count, Actor/Component/Tick counts, Paper2D cull distances, and frame-time/stat sampling.

## Acceptance

B1 passes when all of the following are true:

1. The source map and B0R remain clean and unchanged by the B1 assembly run.
2. B1 is isolated, idempotent, and contains exactly one managed actor per required stable label.
3. A real 505 x 505 Landscape exists with Edit Layers enabled and an unlocked active QuickRoad layer.
4. Three road width groups exist as real QuickRoad road networks or their verified baked output.
5. Main-road geometry stops at the bridge approaches; bridge and river remain distinct and readable.
6. There are 26 buildings, at least 6 archetype IDs, and at least 4 roof palettes with no invalid overlaps.
7. Props, vegetation, and mountain/backdrop populations meet their caps and exclusion rules.
8. Gate, town-core, main-bridge, and south-dock captures have no large unintentional empty foreground/midground zones and clearly distinguish road, river, bridge, buildings, vegetation, and backdrop.
9. Repeating the assembly produces the same live scene-manifest digest, including managed transforms, PCG graphs/outputs, road mesh summary, road-edge count, cameras, and Landscape edit-layer height digest.
10. Raw source/B0R hashes match their pre-build baselines after every mutating phase.
11. Fresh host tests, UBT compile when C++ changes are present, UE MCP validation, saved-package audit, harness validation, existing real-play flow, and PIE smoke all succeed before completion is claimed.
12. The first B1 delivery stops at a user visual gate after presenting the four camera captures. QuickRoad bake and final production recording begin only after that gate is accepted.

## Deferred Work

- Final Tripo/Blender replacement for all six building archetypes.
- Final Paper2D painted plant atlas and production animation polish.
- Final bridge and river art.
- Final lighting/post-process tuning after formal assets are present.
- Gameplay/NPC/navmesh changes outside the B1 scene-dressing scope.
