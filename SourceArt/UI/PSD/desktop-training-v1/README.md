# Desktop Training / Travel Strip — v1 art handoff

## Visual rule

The existing in-game screenshots and approved `Content/GameXXK/UI` textures are the visual source of truth. The GPT image workflow is restricted to a background plate; it must not redraw the project's buttons, tabs, frames, icons, text, characters, enemies, or map nodes.

The runtime travel strip is intentionally a short horizontal band. It must keep a locked aspect ratio and be cropped/scaled uniformly. Never resize the strip independently on X/Y, because that makes the pixel sprites and round nodes look squeezed.

## Current drafts

### v003 — full-canvas transparent seamless plate (runtime MVP candidate)

- `generated/TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png`
- 1983 × 793 px, RGBA, full canvas retained (no crop and no sprite/UI cut-up)
- The baked checkerboard/black negative space has been removed to actual alpha (`1,027,203` fully transparent pixels); the mountain, path, grass and ink edge remain opaque.
- A deterministic 128 px edge blend makes the horizontal wrap exact (`max |first column - last column| = 0`), so the strip can translate indefinitely in either direction. The two-copy preview is `Saved/HarnessReports/travel-strip-tile-preview-v003.png`.
- Imported for the opt-in runtime MVP as
  `/Game/GameXXK/UI/Training/Generated/walkloop_pilot_v1/T_TrainingIdleStrip_Background`.
  This is only a runtime candidate: the PSD handoff still needs the layers below rebuilt separately and the source remains outside the default 3D-town entry.

### v001 — prior cropped draft

- `generated/TrainingIdleStrip_Background_GPT_v001.png`
- 1983 × 200 px, RGB, 9.915:1
- Derived from the GPT-generated wide reference plate, then cropped deterministically to the runtime strip envelope.
- Preview-only draft: not yet imported as a `.uasset` and not wired into runtime.

The crop leaves a continuous warm parchment path for the six existing player sprites and three existing enemy sprites. The charcoal/ink upper band is deliberate so the strip can sit over the desktop game's dark background without adding a second frame.

## PSD layer contract

The final PSD must be rebuilt as separate layers, not delivered as a single noisy flattened UI image:

1. `BG_CharcoalInk` — solid dark surround / negative space.
2. `BG_MountainSilhouette` — low-frequency slate mountain line, no text-like marks.
3. `BG_Path` — warm parchment travel path with crisp pixel edge.
4. `BG_Decor` — sparse tree, fence, stone and grass accents; keep these below gameplay actors.
5. `FX_GroundShadow` — optional low-contrast contact shadows only.
6. `Actors_ExistingSprites` — assembled from the existing PaperZD/flipbook sprites; never baked into the background.
7. `HUD_RuntimeOnly` — cooldown, reward, retry and status text are Slate/UMG, never rasterized into the PSD.

## Pixel and export rules

- Work at native pixel dimensions; use nearest-neighbor for every preview/export.
- Keep transparent/opaque intent explicit per layer. v003 is an RGBA plate with real transparent surround; actor and UI layers remain separate.
- Use 1x pixel outlines and the existing ink-brown contour language. Do not add anti-aliased vector strokes, glow, bloom, or photographic noise.
- Preserve the current backpack/warehouse proportions and approved MasterV2/Town PSD assets. This strip is a backdrop only and must not change those panels.
- Export a source PSD plus a flattened review PNG; the review PNG is not a substitute for the PSD layer handoff.

## Runtime envelope

The current workbench places the idle strip at approximately `1200 × 108` logical pixels. The runtime MVP composes the existing runtime text plus the approved 2K hero walk atlas over two clipped copies of this plate. The final implementation still requires the target desktop-resolution review before this source can graduate from runtime candidate to PSD production art.
