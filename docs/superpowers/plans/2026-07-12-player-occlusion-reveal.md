# Player Occlusion Material Cutout Pilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove a native-resolution, main-camera circular cutout on the Asian Village thatched-roof blocker without modifying vendor assets, then leave a verified mapping path for additional materials.

**Architecture:** The existing camera-to-hero trace controller identifies foreground primitive components. Eligible material slots are temporarily replaced with project-owned Masked copies whose opacity mask uses player viewport coordinates from a Material Parameter Collection. The SceneCapture, render target, post-process composite, and duplicate sprite prototype are disabled and removed after the pilot passes.

**Tech Stack:** UE 5.8 C++, Paper2D, Material Parameter Collection, Masked materials, MaterialEditingLibrary through project UE MCP, Automation Tests, UBT, PIE screenshot probes.

---

### Task 1: Replace the SceneCapture contract with a material-cutout contract

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKHeroCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKHeroCharacter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write the failing component contract test**

Replace SceneCapture assertions with assertions that the reveal starts with no modified components, uses nine samples, peels at least four layers, and has a restoration path:

```cpp
TestNotNull(TEXT("hero owns occlusion cutout controller"), RevealController);
TestEqual(TEXT("cutout starts with no modified components"), RevealController->GetModifiedComponentCount(), 0);
TestEqual(TEXT("cutout samples center and perimeter"), RevealController->GetOcclusionSampleCount(), 9);
TestTrue(TEXT("cutout peels layered blockers"), RevealController->GetMaxOcclusionLayers() >= 4);
TestTrue(TEXT("cutout restores original material slots"), RevealController->RestoresOriginalMaterials());
TestFalse(TEXT("duplicate reveal sprite remains disabled"), RevealVisual->IsVisible());
```

- [ ] **Step 2: Run UBT and verify RED**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development 'D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload
```

Expected: compile failure because `GetModifiedComponentCount` and `RestoresOriginalMaterials` do not exist.

- [ ] **Step 3: Define focused cutout state types**

Add the following contract and remove SceneCapture/render-target/post-process members:

```cpp
USTRUCT()
struct FGameXXKOcclusionOriginalMaterials
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> Component;

    UPROPERTY()
    TArray<TObjectPtr<UMaterialInterface>> Slots;
};

int32 GetModifiedComponentCount() const { return ModifiedComponents.Num(); }
bool RestoresOriginalMaterials() const { return true; }

UPROPERTY(Transient)
TArray<FGameXXKOcclusionOriginalMaterials> ModifiedComponents;
```

Remove `USceneCaptureComponent2D`, `UTextureRenderTarget2D`, `UMaterialInstanceDynamic` post-process state, `SynchronizeCaptureAndMaterial`, and hero construction of `OcclusionRevealCapture`. Keep the duplicate Paper2D component hidden until Task 4 visual acceptance.

- [ ] **Step 4: Implement unconditional restoration cleanup**

Add:

```cpp
void RestoreAllModifiedComponents();
virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
```

`RestoreAllModifiedComponents` iterates cached slots, restores valid components with `SetMaterial`, then clears the cache. Call it when reveal becomes inactive, leaves Town, and from `EndPlay`.

- [ ] **Step 5: Run UBT and automation test to verify GREEN**

Run UBT, then run `GameXXK.MVP.Town.ShellInputInteractionFollower` through `AutomationTestToolset`. Expected: build succeeds and the test reports `Success` with no warnings.

- [ ] **Step 6: Commit Task 1**

```powershell
git add Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp Source/GameXXK/Public/Town/GameXXKHeroCharacter.h Source/GameXXK/Private/Town/GameXXKHeroCharacter.cpp Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp
git commit -m "refactor: replace scene capture occlusion contract"
```

### Task 2: Author the project-owned thatched-roof cutout assets

**Files:**
- Create: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`
- Create: `scripts/test_occlusion_cutout_pilot_assets.py`
- Create through UE: `Content/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion.uasset`
- Create through UE: `Content/GameXXK/Materials/Occlusion/AsianVillage/M_thatched_roof_Cutout.uasset`
- Create through UE: `Content/GameXXK/Materials/Occlusion/AsianVillage/MI_thatched_roof_Cutout.uasset` when the inspected mesh uses an instance

- [ ] **Step 1: Write the failing source contract test**

Require the authoring script to contain the project-owned destination, `MD_SURFACE`, `BLEND_MASKED`, `MP_OPACITY_MASK`, `MaterialExpressionScreenPosition`, aspect-corrected radius parameters, and explicit save/recompile calls. Reject writes under `/Game/Asian_Village`.

```python
for token in (
    "/Game/GameXXK/Materials/Occlusion/AsianVillage",
    "MPC_PlayerOcclusion",
    "BLEND_MASKED",
    "MP_OPACITY_MASK",
    "MaterialExpressionScreenPosition",
    "OcclusionCenter",
    "OcclusionAspect",
    "OcclusionRadius",
    "recompile_material",
    "save_asset",
):
    self.assertIn(token, source)
self.assertNotIn('save_asset("/Game/Asian_Village', source)
```

- [ ] **Step 2: Run the source test and verify RED**

Run `python scripts/test_occlusion_cutout_pilot_assets.py`. Expected: FAIL because the authoring script is absent.

- [ ] **Step 3: Inspect the actual roof material before authoring**

In the UE Python script, load the current demo map, find `SM_thatched_roof_10` and `SM_thatched_roof_12`, and record each material slot path. Abort if neither uses the expected thatched-roof family; do not guess a replacement.

- [ ] **Step 4: Duplicate and patch only the required roof material family**

Duplicate the required master/instance chain into `/Game/GameXXK/Materials/Occlusion/AsianVillage`. Preserve existing material expressions and output connections. Set only the copied root to Masked. Build a screen-UV circular mask using MPC values and multiply it with any pre-existing opacity mask:

```text
FinalOpacityMask = ExistingOpacityMask * (1 - DitheredCircleMask)
```

The circle is aspect-corrected; radius defaults to `0.18`, and the feather occupies at most 15% of the radius. Recompile and fail the script on any material compiler error.

- [ ] **Step 5: Run the authoring script through UE MCP and verify assets**

Run `gamexxk_author_occlusion_cutout_pilot.py` with `run_project_python_file`. Expected report: all created paths are under `/Game/GameXXK`, the root reports Masked blend mode, and compiler errors are empty.

- [ ] **Step 6: Commit Task 2 source and generated assets**

```powershell
git add Content/Python/gamexxk_author_occlusion_cutout_pilot.py scripts/test_occlusion_cutout_pilot_assets.py Content/GameXXK/Materials/Occlusion
git commit -m "feat: author thatched roof occlusion cutout assets"
```

### Task 3: Apply cutout mappings only to active foreground blockers

**Files:**
- Create: `Source/GameXXK/Public/Town/GameXXKOcclusionMaterialMap.h`
- Create: `Source/GameXXK/Private/Town/GameXXKOcclusionMaterialMap.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write failing mapping and restoration tests**

Create a transient primitive with two original materials. Assert that only a mapped roof slot is replaced, an unmapped slot remains untouched, and `RestoreAllModifiedComponentsForTest` restores pointer-identical originals.

- [ ] **Step 2: Run automation test and verify RED**

Expected: compile failure because the material mapper and test hooks are absent.

- [ ] **Step 3: Implement the minimal mapping service**

`FGameXXKOcclusionMaterialMap` loads a fixed pilot mapping from the inspected original roof material path to the project-owned cutout path. It returns `nullptr` for all unmapped materials and logs each missing path at most once.

- [ ] **Step 4: Apply and restore component materials on blocker-set changes**

For each newly eligible blocker, cache all original slots, replace only mapped slots, and retain the component only if at least one slot changed. Restore components no longer in the blocker set after the release hold. Never allocate or swap materials every tick.

- [ ] **Step 5: Update the MPC from the real main view**

Use `APlayerController::ProjectWorldLocationToScreen` on the Paper2D visual bounds center and `GetViewportSize`. Set `OcclusionCenter`, `OcclusionAspect`, and `OcclusionRadius` through `UKismetMaterialLibrary`. No camera, render target, or projection matrix is created.

- [ ] **Step 6: Run UBT and automation tests to verify GREEN**

Expected: mapping/restoration tests and `GameXXK.MVP.Town.ShellInputInteractionFollower` pass.

- [ ] **Step 7: Commit Task 3**

```powershell
git add Source/GameXXK/Public/Town/GameXXKOcclusionMaterialMap.h Source/GameXXK/Private/Town/GameXXKOcclusionMaterialMap.cpp Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp
git commit -m "feat: cut out active town occluder materials"
```

### Task 4: PIE visual acceptance and prototype removal

**Files:**
- Modify: `Content/Python/gamexxk_probe_player_occlusion_reveal.py`
- Modify: `Source/GameXXK/Public/Town/GameXXKHeroCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKHeroCharacter.cpp`
- Delete after acceptance: `Content/GameXXK/Materials/Player/M_PlayerOcclusionReveal.uasset`
- Delete after acceptance: `Content/GameXXK/Materials/Player/M_PP_PlayerOcclusionReveal.uasset`

- [ ] **Step 1: Update the PIE probe for the material route**

Report hero viewport position, current blocker actor/component, each blocker material path, whether it belongs to `/Game/GameXXK/Materials/Occlusion`, and the absence of SceneCapture components.

- [ ] **Step 2: Verify the blocked thatched-roof view**

Use collision-respecting movement rather than teleporting inside the hollow building. Capture native-resolution screenshots before obstruction, during obstruction, and after release. Acceptance: the hero sprite has the same pixel height in clear and blocked screenshots; the opening contains the normal scene; no black fill or blur is visible.

- [ ] **Step 3: Verify restoration and screen transitions**

Move clear of the roof, open a town panel, and stop PIE. Confirm every inspected component has its original material paths and the MPC is inactive.

- [ ] **Step 4: Measure pilot cost**

Capture `stat unit`, `stat gpu`, and trace/update timing in clear and blocked views. Acceptance: no secondary scene capture pass; added GPU time below 1ms and average CPU update below 0.25ms.

- [ ] **Step 5: Remove obsolete capture and duplicate-sprite assets/code**

Only after Steps 2–4 pass, remove the duplicate Paper2D reveal component and obsolete material assets. Keep no fallback that can render a black circle.

- [ ] **Step 6: Run final verification**

Run the source asset test, UBT, `GameXXK.MVP.Town.ShellInputInteractionFollower`, and one final PIE screenshot probe. Expected: all automated checks pass, screenshot meets acceptance, and `git diff --check` reports no errors.

- [ ] **Step 7: Commit Task 4**

```powershell
git add Content/Python/gamexxk_probe_player_occlusion_reveal.py Source/GameXXK/Public/Town/GameXXKHeroCharacter.h Source/GameXXK/Private/Town/GameXXKHeroCharacter.cpp Content/GameXXK/Materials/Player
git commit -m "test: verify native town occlusion cutout"
```
