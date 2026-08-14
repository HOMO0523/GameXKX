---
status: record
owner: codex
updated_at: 2026-07-28
source_commit: 4d4c0147ae9cb2b35606c328219e3cc7eb0efb4e
---
# GameXXK Machine Transfer Checkpoint — 2026-07-28

## Purpose

This checkpoint preserves the project state before the current development machine is sent for repair. Continue work on `main` from the GitHub remote `origin` (`https://github.com/HOMO0523/GameXKX.git`).

## Repository and editor state

- Canonical branch: `main`.
- The previously accumulated 57 local commits, ending at `68577c3` (`docs: design fullscreen HUD battle visuals`), were pushed to `origin/main` on 2026-07-28.
- Source, scripts, tests, prompts, ledgers, final source selections, 1600 safe frames, and this handoff were preserved in `e3ebf6a` (`chore: checkpoint project progress for machine transfer`).
- The runnable UE Content checkpoint, including the 138 production animation atlases, was preserved in `630eabe` (`assets: checkpoint UE content for machine transfer`).
- Unreal Editor was not running when this checkpoint was prepared. There were therefore no editor-memory-only dirty packages left to save; the current `.umap` and `.uasset` changes already exist on disk.
- This is a preservation checkpoint, not a release claim. The large working tree contains ongoing gameplay, UI, route, equipment, party-deck, battle-animation, test, and content work that has not received one fresh end-to-end compile/PIE certification as a single combined state.

## Active Goal record

- Codex Goal/thread ID: `019f938d-8e8b-7e80-82b3-d7ecb6fdd1ac`.
- Recorded status at transfer time: `blocked`.
- The status came from the interrupted task/session, not from a newly diagnosed implementation impasse.
- Goal scope: finish the full character/enemy battle-animation production and UE integration while preserving approved source portraits, enforcing 1600×1600 magenta safe frames and fixed facing, using the approved Seedance model split and one paid submission per unit/action, then validate all animation presentation in PIE.

## Confirmed battle presentation direction

The approved design is recorded in:

- `docs/superpowers/specs/2026-07-26-fullscreen-hud-battle-visual-system-design.md`

Key confirmed decisions:

- Enter battle as a full-screen HUD in the current map; do not load a separate route-to-battle map and do not move or lock the camera.
- Hide world rendering and pause the world while the full-screen 16:9 battle HUD is active; restore both on exit.
- Use the same atlas-driven widget path for formation idle and enlarged central attack/hit presentation.
- Production atlas contract: 4096×4096, 8×8 grid, 512×512 cells, 60 frames, 12 fps.
- Player units are on the right and face left; enemy units are on the left and face right.
- Central attack/hit presentation uses both units at about 200%, a 50% black HUD overlay, a fixed central hit point, synchronized impact at real time 1.1 seconds, and a 4× generic ink impact animation plus screen shake.
- The implementation plan for this approved spec was not completed before the machine-transfer checkpoint; resume by writing/executing that plan against the current code rather than redesigning the feature.

## Animation production state

- Approved final source portraits: 13 player characters and 21 enemies.
- 1600×1600 safe-frame sources exist for all 34 units.
- Production ledger: `SourceAssets/AnimationProduction/production_v1/ledger.json`.
- Prompt/generation manifest: `SourceAssets/AnimationProduction/prompts_v1/generation_manifest.json`.
- Ledger snapshot: starting credits `6870`, spent `6540`, projected remaining `330`, submitted `138`, successful `138`, failed `1`, reused `1`, pending `0`.
- Current production covers per-unit `idle`, `attack`, `hit`, and `death`, plus generic Buff and Debuff assets. Per-unit Buff actions and a final generic ink-impact import remain outside the current 138-atlas set.
- Processed source atlas count: 138.
- UE atlas asset count under `Content/GameXXK/BattleAnimations/Atlases`: 138.
- Character/enemy cards and story portraits remain separate from battle animation and must not be overwritten.

## Large-file transfer boundary

The runnable UE project assets, source code, scripts, manifests, prompts, tests, final selected portraits, 1600 safe frames, and documentation are present in the Git checkpoint. The following local production evidence is much larger and is intentionally not part of the normal Git checkpoint; copy it separately or move it to Git LFS/object storage if exact raw regeneration evidence is required:

- `SourceAssets/AnimationProcessing/Production` — approximately 2.13 GB of extracted/processed PNG frames.
- `SourceAssets/AnimationProduction/production_v1/raw` — approximately 1.06 GB of original generated MP4 files.
- Other candidate/source-art folders — approximately 0.6 GB combined.
- `outputs/UI_PSD/GameXXK_Town_4K.psd` — approximately 112.65 MB and exceeds GitHub's normal per-file limit.

Do not pay for or resubmit any missing animation automatically. The one-submission rule and credit ledger remain authoritative.

## Windows hardware evidence captured before repair

- Current Windows inventory enumerated one 32 GB Crucial module (`CT32G56C46S5.M16B2`) configured at 5200 MT/s.
- No `Microsoft-Windows-MemoryDiagnostics-Results` event was present in the last 60 days, so Windows Memory Diagnostic has not left a pass/fail result in the System log.
- 2026-07-20 23:38:51: BugCheck event 1001, stop code `0x0000007A`, with a recorded dump path `C:\Windows\Minidump\072026-8843-01.dmp`. That dump file was no longer present when queried.
- 2026-07-25 20:16:23: WHEA-Logger event 3 recorded a generic hardware event. Its public message does not identify a DIMM or memory address.
- Critical Kernel-Power event 41 / unexpected shutdown event 6008 pairs were recorded on 2026-07-20, 2026-07-21, and 2026-07-25.

Interpretation: the machine has objective records of a bugcheck, hardware-event telemetry, and repeated abnormal shutdowns. This is consistent with the reported instability, but `0x7A` can also involve the storage/pagefile path and the WHEA event does not identify a specific RAM stick. The logs support sending the machine for hardware diagnosis; they do not by themselves prove which DIMM failed.

## Resume checklist on the replacement machine

1. Clone `origin/main` and verify the checkpoint commit reported in the transfer handoff.
2. Install the same UE 5.8 project dependencies; the licensed `Content/Asian_Village/**` dependency remains intentionally excluded from Git and must be installed locally.
3. Generate project files and perform a cold UBT build; do not use Live Coding or Hot Reload for verification.
4. Open the editor, enable the project UE MCP tooling, and run focused tests before changing existing user-tuned assets.
5. Read the approved full-screen HUD design above, then complete the implementation plan and resume from the atlas-driven battle HUD work.
6. If raw animation reproduction is needed, restore the separately copied `SourceAssets/AnimationProcessing/Production` and `SourceAssets/AnimationProduction/production_v1/raw` folders before running processing scripts.
