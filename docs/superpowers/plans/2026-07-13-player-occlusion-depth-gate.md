# Player Occlusion Foreground Depth Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reveal the player only through foreground pixels between camera and hero, while retaining each original material's Mask and blend behavior.

**Architecture:** `UGameXXKPlayerOcclusionRevealComponent` keeps its existing 20Hz blocker detection and temporary material-slot swapping. It additionally publishes camera position, normalized forward vector, hero view depth, and a small bias to `MPC_PlayerOcclusion`; project-owned Cutout materials calculate a screen circle and an independent per-pixel foreground gate, then multiply the resulting keep value into the original opacity chain.

**Tech Stack:** UE 5.8 C++, Material Parameter Collection, editor Python material authoring, Python `unittest`, Unreal Automation, UBT, UE MCP.

---

### Task 1: Lock the material recipe with failing contract tests

**Files:**
- Modify: `scripts/test_occlusion_cutout_pilot_assets.py`
- Modify: `scripts/test_asian_village_occlusion_material_pipeline.py`

- [ ] **Step 1: Add the pilot-depth test**

Append to `OcclusionCutoutPilotAssetContractTest`:

```python
def test_authoring_script_combines_circle_with_camera_to_hero_depth_gate(self):
    source = AUTHOR.read_text(encoding="utf-8")
    for token in (
        "OcclusionCameraLocation", "OcclusionCameraForward",
        "OcclusionHeroViewDepth", "OcclusionDepthBias",
        "MaterialExpressionWorldPosition", "MaterialExpressionDotProduct",
        "MaterialExpressionIf", "MaterialExpressionOneMinus",
    ):
        self.assertIn(token, source)
```

- [ ] **Step 2: Prove the test is red**

Run `python -m unittest scripts.test_occlusion_cutout_pilot_assets.OcclusionCutoutPilotAssetContractTest.test_authoring_script_combines_circle_with_camera_to_hero_depth_gate -v`.

Expected: `FAIL`, because the pilot script currently only contains a screen-space circle.

- [ ] **Step 3: Add the same recipe assertions to the full-generation script test**

Point the assertion at `Content/Python/gamexxk_author_asian_village_occlusion_materials.py`. It must require the same four MPC names and four material-expression types, preserving the existing source blend/naming assertions.

- [ ] **Step 4: Prove the full-generation test is red**

Run `python -m unittest scripts.test_asian_village_occlusion_material_pipeline -v`.

Expected: only the new depth-gate contract fails.

- [ ] **Step 5: Commit red tests**

Run `git add scripts/test_occlusion_cutout_pilot_assets.py scripts/test_asian_village_occlusion_material_pipeline.py` then `git commit -m "test: define player occlusion depth gate contract"`.

### Task 2: Publish camera-to-hero depth through the MPC

**Files:**
- Modify: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`
- Modify: `Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Add a failing C++ test API and assertion**

Expose `GetOcclusionDepthBias()`, `GetLastHeroViewDepthForTest()`, and `UpdateMaterialParametersForTest(FVector CameraLocation, FVector CameraForward, FVector HeroCenter)`. In `GameXXKTownShellTest.cpp`, add:

```cpp
RevealController->UpdateMaterialParametersForTest(
    FVector::ZeroVector, FVector::ForwardVector, FVector(500.0f, 0.0f, 0.0f));
TestEqual(TEXT("occlusion stores hero camera-forward depth"),
    RevealController->GetLastHeroViewDepthForTest(), 500.0f);
TestTrue(TEXT("occlusion has positive front-depth bias"),
    RevealController->GetOcclusionDepthBias() > 0.0f);
```

- [ ] **Step 2: Prove the C++ test is red**

Run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -NullRHI -Unattended -NoSplash '-ExecCmds=Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower; Quit' -TestExit='Automation Test Queue Empty' -stdout -FullStdOutLogOutput
```

Expected: compile failure because the new API does not yet exist.

- [ ] **Step 3: Add MPC parameters in `_ensure_collection()`**

Create `CollectionVectorParameter`s named `OcclusionCameraLocation` and `OcclusionCameraForward`, plus `CollectionScalarParameter`s named `OcclusionHeroViewDepth` and `OcclusionDepthBias`. Include them in the existing vectors/scalars arrays; set defaults `(0,0,0)`, `(1,0,0)`, `0.0`, and `5.0` respectively.

- [ ] **Step 4: Update C++ through one helper**

Refactor `UpdateMaterialParameters()` to call the testable helper. The helper normalizes `CameraForward`, computes `FVector::DotProduct(HeroCenter - CameraLocation, CameraForward)`, records the depth for test, and writes all four values with `UKismetMaterialLibrary`. Add an edit-default `OcclusionDepthBias = 5.0f` with a positive clamp.

- [ ] **Step 5: Verify and commit**

Re-run the commandlet from Step 2; expected `Test Completed. Result={Success} Name={ShellInputInteractionFollower}`. Then stage these four files and commit `feat: publish player occlusion view depth`.

### Task 3: Build the depth-gated Cutout graph without overwriting source opacity

**Files:**
- Modify: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`
- Modify: `Content/Python/gamexxk_author_asian_village_occlusion_materials.py`
- Modify: `scripts/test_occlusion_cutout_pilot_assets.py`
- Modify: `scripts/test_asian_village_occlusion_material_pipeline.py`

- [ ] **Step 1: Implement the shared graph recipe in both scripts**

After the circle expression, construct this material graph:

```text
PixelViewDepth = dot(AbsoluteWorldPosition - OcclusionCameraLocation,
                     OcclusionCameraForward)
IsForeground = if(PixelViewDepth < OcclusionHeroViewDepth - OcclusionDepthBias, 1, 0)
Reveal = CircleMask * IsForeground
KeepMask = 1 - Reveal
```

Use `MaterialExpressionWorldPosition`, `Subtract`, `DotProduct`, `If`, `Multiply`, and `OneMinus`. Keep the existing circle feather at `0.02`; depth is a hard gate.

- [ ] **Step 2: Preserve source blend paths exactly**

Maintain:

```text
opaque copy:       OpacityMask = KeepMask
masked copy:       OpacityMask = OriginalOpacityMask * KeepMask
translucent copy:  Opacity = OriginalOpacity * KeepMask
```

Never overwrite an original Mask/Opacity link. Existing generated-material validation must continue requiring `Multiply` for masked/translucent roots.

- [ ] **Step 3: Verify tests and commit**

Run `python -m unittest scripts.test_occlusion_cutout_pilot_assets scripts.test_asian_village_occlusion_material_pipeline -v`; expected all pass. Stage these four files and commit `feat: gate player cutout by camera depth`.

### Task 4: Regenerate a pilot roof material and inspect it

**Files:**
- Generated only: `Content/GameXXK/Materials/Occlusion/AsianVillage/*_Cutout.uasset`
- Use: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`
- Use: `Content/Python/gamexxk_probe_cutout_material_outputs.py`

- [ ] **Step 1: Save and stop PIE safely**

Use `scripts/ue_mcp_client.py` to save dirty packages and stop PIE. If MCP is unavailable and editor packages are dirty, ask the user instead of closing the editor.

- [ ] **Step 2: Regenerate only project-owned pilot assets**

Run `gamexxk_author_occlusion_cutout_pilot.py` through UE MCP Python. Confirm JSON has `all_cutout_blends_masked: true` and only `/Game/GameXXK/Materials/Occlusion/AsianVillage` destinations.

- [ ] **Step 3: Inspect the result**

Run `gamexxk_probe_cutout_material_outputs.py` through UE MCP Python. Confirm the root graph contains the original opacity source and the new `KeepMask` multiplication; do not alter `/Game/Asian_Village`.

### Task 5: Validate the blocked view and final build

**Files:**
- Use: `Content/Python/gamexxk_probe_player_occlusion_reveal.py`
- Use: `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`

- [ ] **Step 1: Run a blocked-view PIE check**

Use UE MCP to start PIE in the target map and move the pawn to the known roof-blocked point. Accept only when front roof pixels in the circle clear, the hero remains unmodified, and pixels behind the hero do not clear.

- [ ] **Step 2: Check restoration**

Move to a clear view, wait `0.22s`, then end PIE. Confirm `IsRevealActive()` is false and cached material slots are pointer-identical to their originals.

- [ ] **Step 3: Cold build and automation**

After UE MCP save and editor close, run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject'
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -NullRHI -Unattended -NoSplash '-ExecCmds=Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower; Quit' -TestExit='Automation Test Queue Empty' -stdout -FullStdOutLogOutput
```

Expected: UBT succeeds and the Automation result is success.

- [ ] **Step 4: Commit project-owned work only**

Run `git add Source/GameXXK Content/Python scripts Content/GameXXK/Materials/Occlusion`, commit `feat: reveal player through foreground occluders only`, then run `git status --short`. Never stage `Content/Asian_Village`, Paper2D assets, camera transforms, or placed levels.
