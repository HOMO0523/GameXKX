# Gem Icon Progression Design

**Status:** Approved in conversation on 2026-08-24; pending written-spec review.

## 1. Scope

Produce and integrate thirty final gem item icons: three gem types across ten append-only qualities. These are project assets, not preview-only concepts.

| Gem type | Stable main color | Core silhouette |
|---|---|---|
| Attack | vermilion red | sharp diamond / arrowhead |
| Defense | indigo blue | heavy hexagon / shield face |
| Max Health | jade green | rounded seed / jade droplet |

Color supports type recognition but does not carry quality by itself. Each gem type keeps its core silhouette and main color through all ten qualities.

## 2. Art Direction

Match the established equipment-item language visible in `SourceArt/Generated/Draft/V1/DRAFT-B02-EQUIPMENT_checkpoint31_contact_sheet.png`: Q-version ink-cartoon illustration, confident dark ink outline, hand-painted material shading, compact centered composition, and readable shape at inventory-cell size.

Every icon must:

- be a separate 512×512 RGBA PNG with genuine transparency;
- contain no checkerboard, text, number, logo, watermark, frame, or UI background;
- keep the subject centered and use roughly 75% of the canvas;
- use a consistent three-quarter product-icon camera and lighting direction;
- preserve clear negative space around the silhouette;
- remain recognizable when reduced to the actual item-cell size.

## 3. Ten-Quality Visual Grammar

The ten ranks must evolve in shape and complexity, not merely change hue.

| Rank | Quality | Required visual step |
|---:|---|---|
| 1 | Common | rough raw stone; simplest outline |
| 2 | Rare | polished surface and first clean facets |
| 3 | Epic | precise multi-facet cut; no external ornament yet |
| 4 | Legendary | small metal bezel and first engraved rune |
| 5 | Immortal | complete gold setting with stronger rune work |
| 6 | Treasure | one restrained set of floating shards or satellites |
| 7 | Transcendent | ink-energy halo added around the established form |
| 8 | Celestial | celestial ring and cloud-pattern ornament |
| 9 | Ascendant | double halo, richer crown structure, and energy trail |
| 10 | Cosmic | star-cloud core, constellation details, and final crown; most elaborate silhouette |

Each step inherits the previous step's core shape. New detail is additive and controlled so the icon does not become an unreadable particle cloud.

## 4. Production Method

Use the built-in image-generation workflow.

1. Generate one Common master for each gem type using the equipment contact sheet as the style reference.
2. For each type, derive ranks 2–10 sequentially from the immediately preceding accepted icon while also retaining the Common master as the silhouette anchor.
3. Change only the quality-step features defined above. Preserve type, camera, scale, main palette, outline weight, and transparent background.
4. Inspect every output directly. Reject baked checkerboards, opaque borders, type drift, unrelated objects, illegible effects, or a rank that is not visibly stronger than its predecessor.
5. Copy accepted project-bound outputs into the workspace; no referenced asset may remain only in the generator's default output directory.

## 5. File and Asset Contract

Source PNGs live under:

```text
SourceArt/UI/Items/Gems/final/
```

Names use:

```text
T_Item_Gem_<Attack|Defense|MaxHealth>_<Common|Rare|Epic|Legendary|Immortal|Treasure|Transcendent|Celestial|Ascendant|Cosmic>.png
```

Imported textures live under:

```text
/Game/GameXXK/UI/Items/Gems/
```

The imported asset name exactly matches the PNG stem. Import settings use UI texture grouping, sRGB color, preserved alpha, and no lossy alpha removal.

`FGameXXKGemRules` owns the stable mapping from `(GemType, GemQuality)` and `Item.Gem.<Type>.<Quality>` to the exact soft texture path. UI consumers use this mapping rather than constructing paths independently. Missing or invalid mappings display an empty icon; they never fall back to a different type or quality.

## 6. Review Artifacts

Create:

```text
SourceArt/UI/Items/Gems/gem_icon_manifest.json
SourceArt/UI/Items/Gems/review/gem-quality-progression-contact-sheet.png
```

The manifest records all thirty item IDs, source PNGs, imported texture paths, dimensions, alpha statistics, and SHA-256 hashes.

The contact sheet is a 3×10 matrix ordered Attack, Defense, Max Health by rows and Common through Cosmic by columns. It includes both full-size inspection cells and an actual inventory-cell-size inset so type recognition and quality progression can be judged at runtime scale.

## 7. Acceptance Checks

Deterministic asset validation must prove:

- exactly thirty final PNGs exist;
- every PNG is 512×512 RGBA;
- every border contains transparent pixels and no baked checkerboard;
- every icon has non-empty visible content and safe transparent padding;
- all thirty SHA-256 hashes are unique;
- each type keeps its approved primary-color family and core silhouette;
- visual complexity increases monotonically within each ten-icon row;
- all thirty UE texture assets load from their declared paths;
- all thirty stable item definitions resolve to the matching texture and never cross type or quality.

Pure art generation is not subject to TDD. Runtime mapping, item registration, save behavior, socket projection, and UI consumption remain subject to the normal RED/GREEN workflow.

## 8. Out of Scope

- animated gem icons;
- numeric rank labels baked into art;
- rarity frames shared with equipment cards;
- procedural recoloring as a substitute for the thirty final bitmaps;
- chest icons, which remain a separate Training-chest work package.
