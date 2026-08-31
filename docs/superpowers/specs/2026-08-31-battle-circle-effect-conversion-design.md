---
status: approved-design
owner: codex
updated_at: 2026-08-31
---

# Battle Circle Effect MOV Conversion Design

## Goal

Convert `画圈-1.mov` into a review-only project animation candidate by following the repository's existing black-background MOV workflow. The task stops at source-art outputs and must not import, replace, or modify Unreal assets.

## Input

- Source: `C:/Users/shxuw/xwechat_files/wxid_g90er9r4o8p312_cd3c/msg/file/2026-08/画圈-1/画圈-1.mov`
- Source SHA-256: `dc9f684ec0815c4ec562474b3d379fac8907c6e055677ed990fca7a0e00c3b40`
- Decoded contract: HEVC, 1440×1440, 30 fps, 1.72 seconds, 51 decoded frames, uniform black background.
- Playback policy: one-shot. Preserve the authored complete-circle hold and the final disappearance; do not force a loop seam.

## Canonical Workflow

Use the same staging and processing model as `SourceAssets/AnimationProduction/upgrade_20260827_corrected` and `scripts/build_animation_upgrade_candidate_atlas.py`:

1. Stage an immutable copy of the source MOV and record its source path, hash, metadata, and black-background classification in `source_manifest.json`.
2. Decode and normalize to the project's 60-frame candidate contract while preserving the source duration. Record the derived playback FPS in the output manifest.
3. Convert black background to transparent RGBA with the existing color-signal, outline-dilation, feathering, and codec-island cleanup rules.
4. Use one union crop for the complete sequence so frame-local bounds cannot recenter or jitter the ink circle.
5. Normalize every output frame to a 512×512 logical cell and pack frames row-major into an 8×8 atlas.
6. Produce the existing 2K and 1K runtime-sized atlas variants, a checkerboard contact sheet, per-frame PNG files, hashes, placement data, alpha-edge measurements, and a candidate manifest.

The circle is a centered battle effect rather than a grounded actor. Its placement record therefore uses a centered anchor while retaining the project's existing atlas, matting, manifest, and review-only conventions. Fully black source frames at the authored ending become transparent empty frames. Any compatibility change needed for such frames must be narrowly scoped and must preserve current behavior for existing candidates.

## Output Boundary

- Candidate ID: `battle_circle_effect_01`.
- Source and processed candidate files remain under a dedicated review-only directory in `SourceAssets/AnimationProduction`.
- Review evidence remains under `Saved/HarnessReports`.
- Status: `candidate_review_only`.
- Runtime replacement status remains forbidden pending separate user approval.
- No writes are allowed under `Content/`, `SourceAssets/AnimationProcessing/Production`, existing character/PaperZD assets, maps, or camera/HD2D tuning.

## Verification

Deterministic checks must confirm:

- source and staged-copy hashes match;
- exactly 60 RGBA frames exist and each is 512×512;
- atlases are RGBA, 8×8, 2048×2048 and 1024×1024 with 256 px and 128 px cells;
- transparent pixels have zero RGB values;
- all atlas hashes match the manifest;
- every outer edge has zero alpha;
- shared placement is stable across frames;
- the final authored black frames are transparent, not rejected or converted into opaque black cells;
- a contact sheet shows the draw, completed-circle hold, and clean disappearance in the original order.

Visual review is limited to generated source-art evidence. Unreal import and PIE verification are out of scope for this task.
