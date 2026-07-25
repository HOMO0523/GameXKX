# Battle Animation Texture Memory Design

## Objective

Keep the approved 4096x4096, 60-frame battle animation atlases visually suitable for 200% HUD presentation while making battles stable on PCs with 4 GB of GPU memory. The solution must reduce the current approximately 63 MB per-atlas GPU footprint and must not hide excessive residency by only increasing the streaming pool.

## Current State

- Each animation contains 60 RGBA frames packed into an 8x8 atlas.
- Each atlas is 4096x4096 and each frame cell is 512x512.
- Textures currently use uncompressed editor-icon compression with no mipmaps, producing an approximately 63 MB GPU estimate per atlas.
- The project currently configures `r.Streaming.PoolSize=1000` MB.
- The battle scene loads persistent idle flipbooks and the HUD layer loads attack, hit, death, status, and impact atlases.
- A real PIE battle did not report a texture-streaming-pool-over-budget warning, but the per-texture cost is too high for the intended 4 GB GPU target.

## Selected Design

### BC7 Atlas Compression

All production battle animation atlases will use BC7 compression with sRGB enabled and mip generation disabled.

- Preserve the 4096x4096 atlas and 512x512 frame cells.
- Preserve alpha for the already-matted character and effect silhouettes.
- Use BC7 rather than reducing source resolution because battle animations are enlarged to approximately 200% during HUD presentation.
- Keep mipmaps disabled because the atlas does not currently include per-cell gutters and mip generation could blend neighboring animation cells.
- Keep bilinear filtering unless the pilot comparison reveals visible neighboring-cell contamination.

Expected GPU size is approximately 16 MB per atlas, around 75% below the current uncompressed estimate.

### Battle-Scoped Residency

The runtime must not create hard references that load all production atlases at startup.

- Persistent scene units load only the idle flipbooks required by the current battle formation.
- Attack, hit, and death atlases are resolved from soft asset paths only when an animation sequence requires them.
- The generic impact, generic buff, and generic debuff atlases may remain resident during a battle because only three shared textures are involved.
- The animation layer clears all Slate brushes and strong texture references when a sequence finishes.
- Battle teardown clears presentation queues, streamable handles, cached textures, and persistent idle actors so UE garbage collection can reclaim their textures.
- Asset lookup tables contain names or soft paths, never a startup array of loaded `UTexture2D` objects.

The first implementation may retain synchronous on-demand loads because BC7 reduces each transfer to approximately 16 MB. If PIE timing evidence shows a visible hitch, a later isolated change may add asynchronous preloading for only the pending attacker, target, and impact textures.

## Pilot and Rollout

The conversion is gated by a small visual pilot before touching every imported asset.

1. Convert the hero idle/attack/hit and rooster idle/attack/hit atlases to BC7.
2. Verify imported pixel dimensions, alpha support, sRGB, no mipmaps, and BC7 settings through UE MCP.
3. Run a real card attack in PIE using the existing hero-versus-rooster path.
4. Capture idle, attack, impact, and hit frames at the current 200% HUD scale.
5. Compare water-ink edges, thin weapon strokes, facial features, transparent edges, and magenta contamination against the current uncompressed sample.
6. Measure the texture resource estimate and inspect the log for streaming-pool-over-budget warnings.
7. Only after the pilot passes, apply the same settings idempotently to all production atlases.

The source PNG files and processed animation frames remain unchanged. The importer changes only UE texture build settings, so the operation can be repeated or revised without spending generation credits.

## Error Handling

- Abort the pilot if UE does not expose BC7 or if an imported texture loses usable alpha.
- Do not fall back silently to a lower-resolution atlas.
- Do not increase the texture pool as a substitute for failed compression or residency cleanup.
- If BC7 introduces unacceptable edge artifacts, compare BC3 and uncompressed output on the same six pilot atlases before selecting a fallback.
- Continue treating `enemy_07_graywolf_attack` as the single known missing production animation; memory optimization must not regenerate it.

## Verification

### Automated Contracts

- Importer tests require BC7 compression and reject the old editor-icon setting.
- Importer tests continue to require 4096x4096 atlases, 60 frames, an 8x8 grid, and idle-only PaperFlipbooks.
- Presentation tests verify brushes and strong texture references are cleared after playback and on battle teardown.
- Mapping tests verify atlas lookup remains soft-path based and does not load the complete catalog.

### UE and PIE Acceptance

- Pilot atlas GPU estimate is approximately 16 MB rather than approximately 63 MB.
- Hero and rooster remain visually acceptable at 200% HUD scale.
- Transparent boundaries contain no magenta fringe visible against the dimmed battle background.
- Real card damage still triggers attacker, victim, impact, and shake in the correct timing and orientation.
- No texture-streaming-pool-over-budget warning appears during repeated attacks or battle transitions.
- Re-entering battle does not monotonically increase resident battle-animation memory.

## Non-Goals

- No video regeneration or generation-credit use.
- No reduction to 2048 atlases in the default quality tier.
- No changes to character cards or story portraits.
- No increase beyond the existing 1000 MB texture pool unless later profiling provides separate evidence that the general project needs it.
