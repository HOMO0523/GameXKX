# Battle Animation Texture Memory Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce production battle-animation atlas GPU cost from approximately 63 MB to approximately 16 MB per texture and ensure only the current battle presentation retains animation textures.

**Architecture:** Keep the approved 4096x4096, 8x8 atlas format and switch UE texture build settings to BC7 with no mipmaps. Continue resolving animation assets through soft paths, but explicitly clear UMG brushes, texture pointers, queued clips, and active presentation state after playback and whenever the BattleBoard leaves battle.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Paper2D, UE Editor Python, UE MCP, Python `unittest`, Unreal Automation Tests, UBT.

---

## File Structure

- Modify `Content/Python/gamexxk_import_battle_animation_production.py`: define and apply the canonical BC7 texture build policy idempotently.
- Modify `scripts/test_battle_animation_production_import.py`: pure-Python contracts for the build policy and fixed pilot set.
- Create `Content/Python/gamexxk_validate_battle_animation_texture_memory.py`: UE-side validation of imported dimensions, compression, mip, filtering, alpha-capable format, and resource-size evidence.
- Create `scripts/test_battle_animation_texture_memory_validator.py`: pure tests for validator result aggregation and pilot membership.
- Modify `Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h`: expose production reset behavior and resource-state inspection used by automation.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp`: clear Slate brushes, strong texture references, queued clips, and active state.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`: prove completed playback and battle teardown release presentation resources.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`: reset the animation layer whenever the board exits battle.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`: prove non-battle refresh tears down an active animation presentation.

### Task 1: Lock the BC7 importer policy with pure tests

**Files:**
- Modify: `scripts/test_battle_animation_production_import.py`
- Modify: `Content/Python/gamexxk_import_battle_animation_production.py`

- [ ] **Step 1: Write the failing policy tests**

Add these assertions to `BattleAnimationProductionImportTests`:

```python
def test_uses_bc7_no_mipmap_policy_for_every_production_atlas(self) -> None:
    importer = load_importer()
    self.assertEqual(importer.TEXTURE_COMPRESSION_SETTING, "TC_BC7")
    self.assertEqual(importer.TEXTURE_MIP_SETTING, "TMGS_NO_MIPMAPS")
    self.assertEqual(importer.TEXTURE_FILTER_SETTING, "TF_BILINEAR")
    self.assertTrue(importer.TEXTURE_SRGB)

def test_pilot_set_is_limited_to_hero_and_rooster_combat_clips(self) -> None:
    importer = load_importer()
    self.assertEqual(
        importer.PILOT_ASSET_IDS,
        {
            "character_00_hero_idle",
            "character_00_hero_attack",
            "character_00_hero_hit",
            "enemy_01_rooster_idle",
            "enemy_01_rooster_attack",
            "enemy_01_rooster_hit",
        },
    )
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
python -m unittest scripts.test_battle_animation_production_import -v
```

Expected: the two new tests fail because the policy constants do not exist and the importer still uses `TC_EDITOR_ICON`.

- [ ] **Step 3: Add the canonical policy and apply it in one function**

Add near the importer constants:

```python
TEXTURE_COMPRESSION_SETTING = "TC_BC7"
TEXTURE_MIP_SETTING = "TMGS_NO_MIPMAPS"
TEXTURE_FILTER_SETTING = "TF_BILINEAR"
TEXTURE_SRGB = True
PILOT_ASSET_IDS = {
    "character_00_hero_idle",
    "character_00_hero_attack",
    "character_00_hero_hit",
    "enemy_01_rooster_idle",
    "enemy_01_rooster_attack",
    "enemy_01_rooster_hit",
}
```

Replace the inline texture settings with:

```python
def _configure_texture(texture: object) -> None:
    texture.set_editor_property(
        "mip_gen_settings",
        getattr(unreal.TextureMipGenSettings, TEXTURE_MIP_SETTING),
    )
    texture.set_editor_property(
        "filter",
        getattr(unreal.TextureFilter, TEXTURE_FILTER_SETTING),
    )
    texture.set_editor_property(
        "compression_settings",
        getattr(unreal.TextureCompressionSettings, TEXTURE_COMPRESSION_SETTING),
    )
    texture.set_editor_property("srgb", TEXTURE_SRGB)
```

Call `_configure_texture(texture)` before `_save(texture)` for both existing and newly imported assets. Do not set `never_stream`, change dimensions, or enable mipmaps.

- [ ] **Step 4: Run the test and verify GREEN**

Run:

```powershell
python -m unittest scripts.test_battle_animation_production_import -v
```

Expected: all importer contract tests pass.

- [ ] **Step 5: Commit only the importer policy files**

```powershell
git add Content/Python/gamexxk_import_battle_animation_production.py scripts/test_battle_animation_production_import.py
git commit -m "perf: configure battle animation atlases as BC7"
```

### Task 2: Add an editor-side texture-memory validator

**Files:**
- Create: `Content/Python/gamexxk_validate_battle_animation_texture_memory.py`
- Create: `scripts/test_battle_animation_texture_memory_validator.py`

- [ ] **Step 1: Write the failing aggregation tests**

The pure test loads the validator module without Unreal and verifies that one invalid record fails the report:

```python
def test_report_rejects_any_texture_that_drifted_from_bc7_policy(self):
    validator = load_validator()
    report = validator.build_report([
        {"asset_id": "hero", "ok": True, "resource_size_bytes": 16 * 1024 * 1024},
        {"asset_id": "rooster", "ok": False, "errors": ["compression=TC_EDITOR_ICON"]},
    ])
    self.assertFalse(report["ok"])
    self.assertEqual(report["failed_asset_ids"], ["rooster"])

def test_pilot_validation_contains_exactly_six_assets(self):
    validator = load_validator()
    self.assertEqual(len(validator.PILOT_ASSET_IDS), 6)
    self.assertIn("character_00_hero_attack", validator.PILOT_ASSET_IDS)
    self.assertIn("enemy_01_rooster_hit", validator.PILOT_ASSET_IDS)
```

- [ ] **Step 2: Run the validator tests and verify RED**

Run:

```powershell
python -m unittest scripts.test_battle_animation_texture_memory_validator -v
```

Expected: FAIL because the validator module does not exist.

- [ ] **Step 3: Implement the minimal validator**

The validator must expose `PILOT_ASSET_IDS`, `build_report(records)`, and a UE entrypoint. For each `/Game/GameXXK/BattleAnimations/Atlases/T_<asset>_atlas` texture it records:

```python
record = {
    "asset_id": asset_id,
    "path": asset_path,
    "size": [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())],
    "compression": str(texture.get_editor_property("compression_settings")),
    "mip_gen": str(texture.get_editor_property("mip_gen_settings")),
    "filter": str(texture.get_editor_property("filter")),
    "srgb": bool(texture.get_editor_property("srgb")),
    "resource_size_bytes": int(
        texture.get_resource_size_bytes(unreal.ResourceSizeMode.ESTIMATED_TOTAL)
    ),
}
```

`record["ok"]` is true only when dimensions are `[4096, 4096]`, compression contains `BC7`, mip setting contains `NO_MIPMAPS`, filter contains `BILINEAR`, sRGB is true, and `resource_size_bytes <= 20 * 1024 * 1024`. The script accepts repeated `--asset-id`; without IDs it validates the fixed pilot set and prints one JSON object.

- [ ] **Step 4: Run the validator tests and verify GREEN**

Run:

```powershell
python -m unittest scripts.test_battle_animation_texture_memory_validator -v
```

Expected: all validator aggregation tests pass outside UE.

- [ ] **Step 5: Commit the validator**

```powershell
git add Content/Python/gamexxk_validate_battle_animation_texture_memory.py scripts/test_battle_animation_texture_memory_validator.py
git commit -m "test: validate battle atlas texture memory policy"
```

### Task 3: Convert and inspect the six-asset pilot

**Files:**
- Modify generated UE assets only under: `Content/GameXXK/BattleAnimations/Atlases/`

- [ ] **Step 1: Save dirty packages and end PIE through UE MCP**

Use the project MCP client; do not force-close the editor:

```powershell
@'
import sys
sys.path.insert(0, "scripts")
from ue_mcp_client import UnrealMCPClient
c = UnrealMCPClient(timeout=180)
assert c.connect()
c.save_dirty_packages()
c.stop_pie()
'@ | python -
```

- [ ] **Step 2: Reconfigure only the six pilot assets**

Run `Content/Python/gamexxk_import_battle_animation_production.py` through `UnrealMCPClient.run_project_python_file` with twelve alternating arguments:

```text
--asset-id character_00_hero_idle
--asset-id character_00_hero_attack
--asset-id character_00_hero_hit
--asset-id enemy_01_rooster_idle
--asset-id enemy_01_rooster_attack
--asset-id enemy_01_rooster_hit
```

Expected JSON: `ok=true`, `requested_count=6`, and all six texture paths are returned. No source PNG or generation ledger changes.

- [ ] **Step 3: Run UE-side pilot validation**

Run:

```python
client.run_project_python_file(
    "Content/Python/gamexxk_validate_battle_animation_texture_memory.py",
    [],
)
```

Expected: `ok=true`, six records, every texture 4096x4096, BC7, no mipmaps, bilinear, sRGB, and no record above 20 MB.

- [ ] **Step 4: Record pilot evidence without committing unrelated assets**

Save the JSON under `Saved/HarnessReports/battle-animation-bc7-pilot.json`. Do not stage any other dirty project files at this checkpoint.

### Task 4: Release HUD texture references after playback

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`

- [ ] **Step 1: Add failing resource-release assertions**

After the existing lethal sequence finishes, assert:

```cpp
TestFalse(TEXT("completed cinematic releases loaded textures"),
    Layer->HasLoadedPresentationTexturesForTest());
TestFalse(TEXT("completed cinematic clears all Slate brush resources"),
    Layer->HasPresentationBrushResourcesForTest());
TestEqual(TEXT("completed cinematic clears queued descriptors"),
    Layer->GetQueuedSequenceCountForTest(), 0);
```

Add a second case that queues a sequence, calls `Layer->ResetPresentation()`, and asserts inactive state, no loaded textures, no brush resources, and an empty queue.

- [ ] **Step 2: Compile/run the targeted test and verify RED**

Run a cold build/test through:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0.2 --filter "GameXXK.MVP.Battle.AnimationLayerWidget"
```

Expected: compile failure because `ResetPresentation`, `HasLoadedPresentationTexturesForTest`, and `HasPresentationBrushResourcesForTest` do not exist.

- [ ] **Step 3: Implement one resource-clear path**

Declare public `ResetPresentation()` and the two const inspection methods. Override `NativeDestruct()`. Add a private helper:

```cpp
void UGameXXKBattleAnimationLayerWidget::ClearPresentationResources(const bool bClearQueue)
{
    const FSlateBrush EmptyBrush;
    if (AttackerImage) AttackerImage->SetBrush(EmptyBrush);
    if (TargetImage) TargetImage->SetBrush(EmptyBrush);
    if (ImpactImage) ImpactImage->SetBrush(EmptyBrush);
    LeftTexture = nullptr;
    RightTexture = nullptr;
    ImpactTexture = nullptr;
    ActiveSequence = FQueuedSequence();
    ImpactClip = FGameXXKBattleAnimationClipDescriptor();
    if (bClearQueue) QueuedSequences.Reset();
}
```

`FinishPresentation()` calls `ClearPresentationResources(false)` after hiding images. `ResetPresentation()` clears playback flags, hides/collapses the layer, and calls `ClearPresentationResources(true)`. `NativeDestruct()` calls `ResetPresentation()` before `Super::NativeDestruct()`.

- [ ] **Step 4: Re-run the targeted automation test and verify GREEN**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0.2 --filter "GameXXK.MVP.Battle.AnimationLayerWidget"
```

Expected: build succeeds and the animation-layer test passes with zero failures.

- [ ] **Step 5: Commit the presentation cleanup**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp
git commit -m "perf: release battle animation presentation textures"
```

### Task 5: Reset presentation when BattleBoard exits battle

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write the failing non-battle teardown test**

Construct a board with a battle runtime, queue one combat animation, transition the fixture/runtime screen away from battle, call `RefreshFromState()`, and assert:

```cpp
TestFalse(TEXT("leaving battle stops the cinematic"),
    Board->GetBattleAnimationLayerForTest()->IsPresentationActiveForTest());
TestFalse(TEXT("leaving battle releases cinematic textures"),
    Board->GetBattleAnimationLayerForTest()->HasLoadedPresentationTexturesForTest());
TestEqual(TEXT("leaving battle drops queued sequences"),
    Board->GetBattleAnimationLayerForTest()->GetQueuedSequenceCountForTest(), 0);
```

- [ ] **Step 2: Run targeted automation and verify RED**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0.2 --filter "GameXXK.MVP.Battle.BoardWidget"
```

Expected: the new teardown assertions fail because `RefreshFromState()` only collapses the board.

- [ ] **Step 3: Add the minimal board teardown call**

Inside the existing `if (!bInBattle)` branch of `UGameXXKBattleBoardWidget::RefreshFromState()` add:

```cpp
if (BattleAnimationLayer)
{
    BattleAnimationLayer->ResetPresentation();
}
```

Do not clear card-runtime state or persistent save data.

- [ ] **Step 4: Run the targeted board and animation tests**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0.2 --filter "GameXXK.MVP.Battle"
```

Expected: all Battle animation and Board tests pass.

- [ ] **Step 5: Commit the battle teardown bridge**

```powershell
git add Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp
git commit -m "perf: clear battle animation cache on battle exit"
```

### Task 6: PIE visual pilot and hitch check

**Files:**
- Evidence: `Saved/Codex/battle_animation_bc7_*.png`
- Evidence: `Saved/HarnessReports/battle-animation-bc7-pie.json`

- [ ] **Step 1: Run the real playable flow and keep PIE open**

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --keep-pie --timeout 240
```

Expected: main menu, town, quest, route map, and battle all pass.

- [ ] **Step 2: Trigger the real hero attack card**

Run `Content/Python/gamexxk_trigger_battle_animation_sample.py` through UE MCP against `Enemy.Rooster.P3`. If the randomized encounter does not contain the rooster or the preferred attack card, restart the deterministic sample flow rather than mutating the catalog.

- [ ] **Step 3: Capture and inspect three frames**

Capture early attack, synchronized impact, and late hit frames. Compare them with:

```text
Saved/Codex/battle_animation_hero_attack_early.png
Saved/Codex/battle_animation_hero_attack_impact.png
Saved/Codex/battle_animation_hero_attack_late.png
```

Acceptance: no visible magenta fringe, no broken transparent edges, facial and weapon detail remain readable, and the 200% images do not show an unacceptable compression change.

- [ ] **Step 4: Inspect logs and timing**

Search the latest `Saved/Logs/GameXXK.log` for `TEXTURE STREAMING POOL OVER`, asset load errors, and animation-layer warnings. Record card-click-to-overlay time; reject the pilot if a new visible hitch is reproducible.

- [ ] **Step 5: Stop PIE cleanly after review**

Save dirty packages through MCP and call `stop_pie`. Preserve the screenshots and JSON report as uncommitted evidence.

### Task 7: Roll BC7 policy across all atlases and finish missing idle assets

**Files:**
- Modify generated UE assets under: `Content/GameXXK/BattleAnimations/Atlases/`
- Create remaining generated assets under: `Content/GameXXK/BattleAnimations/IdleSprites/` and `IdleFlipbooks/`

- [ ] **Step 1: Reapply the importer in small texture-only batches**

Discover all 138 manifest-backed assets and invoke the importer in batches of six with `--textures-only`. Run UE garbage collection between batches. Existing textures must be reconfigured and saved, not reimported from video or regenerated.

- [ ] **Step 2: Validate all 138 atlas assets**

Run the validator with all discovered asset IDs. Expected: 138 records and no failed IDs. Preserve `enemy_07_graywolf_attack` as absent from the manifest-backed set.

- [ ] **Step 3: Finish the four interrupted idle imports**

Run the importer only for:

```text
enemy_18_deer_idle
enemy_19_moneyrat_boss_idle
enemy_20_blackbear_boss_idle
enemy_21_tiger_boss_idle
```

Expected final disk counts: 138 atlas `.uasset` files, 34 idle flipbooks, and 2,040 idle sprite frames.

- [ ] **Step 4: Save and collect garbage through UE MCP**

Call `save_dirty_packages()` and `collect_garbage(full_purge=True)`. Do not force-close the editor if MCP reports unsaved packages.

### Task 8: Final verification on the 4 GB target policy

**Files:**
- Evidence: `Saved/HarnessReports/battle-animation-memory-final.json`

- [ ] **Step 1: Run all pure Python contracts**

```powershell
python -m unittest scripts.test_battle_animation_production_import scripts.test_battle_animation_texture_memory_validator scripts.test_trigger_battle_animation_sample -v
```

Expected: all tests pass.

- [ ] **Step 2: Run a fresh no-hot-reload C++ build and Battle automation**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0.2 --filter "GameXXK.MVP.Battle"
```

Expected: UBT succeeds and Battle tests report zero failures.

- [ ] **Step 3: Repeat battle entry, attack, battle exit, and battle re-entry**

Use the real MCP playable-flow harness. Trigger at least three attacks, leave battle normally, collect garbage, and enter a second battle. Confirm the cinematic still uses correct sides, 50% dimming, 1.1-second impact timing, and current-unit idles.

- [ ] **Step 4: Verify memory and logs**

The final report must include:

```json
{
  "atlas_count": 138,
  "idle_flipbook_count": 34,
  "idle_sprite_count": 2040,
  "compression": "BC7",
  "max_atlas_resource_mb": 20,
  "streaming_pool_mb": 1000,
  "streaming_pool_over_budget_warnings": 0
}
```

Do not claim completion if any count, resource limit, visual acceptance item, build, automation test, or real PIE flow fails.
