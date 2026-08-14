---
status: record
owner: codex
updated_at: 2026-07-28
source_commit: e3ebf6a0c7d12ade0ef73c2fe24ee1a8f939b996
---
# Battle Animation Pilot — Hero Idle

## Scope

- Battle screen only for this pilot.
- Seedance model: `seedance2.0_vip`.
- Runtime target: non-enemy `Player` battle unit, facing left.
- Static character art may later also replace character-card and story-portrait assets, but that replacement is outside this pilot.

## Source and matting

- Recipe: `SourceAssets/AnimationProcessing/Pilot/Hero/Idle/recipe.json`
- Manifest: `SourceAssets/AnimationProcessing/Pilot/Hero/Idle/manifest.json`
- Atlas: `SourceAssets/AnimationProcessing/Pilot/Hero/Idle/atlas/hero_idle_atlas.png`
- Chroma tolerance: `50`
- Output: 60 RGBA frames, 12 fps, 5 seconds, 512x512 bottom-center canvas.
- Atlas: 4096x4096, 8x8 cells.

## UE assets

- Root: `/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle`
- Flipbook: `/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle/Flipbooks/FB_Pilot_Hero_Idle`
- Sprite count: 60
- Sprite cell: 512x512, bottom-center pivot
- PPU: 2.5. This makes the 512-pixel pilot cell approximately 205 UE units high, matching the production Hero battle cell (`171x205`, PPU 1).

## PIE evidence

- Start: `Saved/Codex/battle_animation_pilot_scaled_start.png`
- Mid-cycle: `Saved/Codex/battle_animation_pilot_scaled_mid.png`
- After loop boundary: `Saved/Codex/battle_animation_pilot_scaled_loop.png`
- Real-flow report: `Saved/Automation/battle_animation_pilot_real_flow_scaled.json`

Runtime probes:

- Mid-cycle playback position: approximately 3.20 seconds.
- After waiting beyond five seconds: approximately 0.91 seconds.
- Both probes reported the Pilot flipbook still assigned, looping and playing.

Visual review:

- Transparent background rendered correctly; no opaque square.
- Hero scale matches the existing battle formation after PPU correction.
- Bottom anchor is stable across sampled frames.
- Hero remains left-facing.
- No obvious magenta fringe at battle-screen display scale.
- PIE was intentionally left running for direct review.

## Known implementation detail

During PIE, `EditorAssetLibrary` did not expose the newly imported pilot in its registry view, while `unreal.load_asset` loaded the same package successfully. Runtime application therefore uses `unreal.load_asset`; editor-only import continues using `EditorAssetLibrary` while PIE is stopped.
