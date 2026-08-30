# Qingshan Carriage Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing desktop `剧情` button repeatedly launch a safe, world-space HD2D carriage arrival/stop/hero-reveal/departure preview in the existing Qingshan 3D town, then restore normal player control without mutating story, guide, idle, party, or reward state.

**Architecture:** The Workbench emits a semantic request; PlayerController owns guarded map travel with the transient `GameXXKIntro=CarriagePreview` option. A single map-placed `AGameXXKPrologueCarriageRig` owns the fixed camera, marker components, world-space atlas widget, pure timeline rules, hero presentation snapshot, scoped input lock, pause overlay, cleanup, and one runtime-only completion event. The default 2D map, normal town toggle, save schema, idle strip, Narrative, and Guide remain unchanged.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, `UWidgetComponent`, `UCameraComponent`, existing Texture2D atlases, focused Unreal Python through UE MCP, Automation Framework, cold UBT, Luna Max visual review.

---

## Scope and repository guard

- Work directly in the root repository on `main`; do not create a worktree.
- Preserve all unrelated dirty work, especially the existing story-button layout, desktop dragging, travel-animation, inventory, battle-node, maps, cameras, PaperZD/Flipbook, and HD2D tuning.
- These three user deletions must remain staged and must never enter a task commit:

```powershell
$ProtectedDeletions = @(
  'Content/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.uasset',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_BackpackScrollbarRight.png'
)
```

- Before each task commit: `git restore --staged -- $ProtectedDeletions`, stage only the named task files/hunks, inspect `git diff --cached --name-status`, commit, then restore the deletions with `git add -u -- $ProtectedDeletions` and require the cached list to contain exactly those three paths.
- Do not use Live Coding, Hot Reload, synthetic mouse input, `SetWindowsHookEx`, `SendInput`, `HideWindow/ShowWindow`, viewport-per-frame hiding, or automatic window minimization.
- Do not run a full `StartsWith:GameXXK` suite until focused gates are green; known unrelated baseline failures and the CompanionRoster tooltip crash are recorded in `docs/production/current-goal-acceptance.md`.

## File map

### New runtime units

- `Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageTypes.h`
  - Timeline phases, immutable configuration, step output, and presentation snapshot data.
- `Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageRules.h`
- `Source/GameXXK/Private/Prologue/GameXXKPrologueCarriageRules.cpp`
  - Pure, deterministic timeline, atlas-frame, pause, and URL-option rules.
- `Source/GameXXK/Public/UI/GameXXKPrologueCarriageWidget.h`
- `Source/GameXXK/Private/UI/GameXXKPrologueCarriageWidget.cpp`
  - One world-space `UImage` that crops the existing 8×8 atlas by normalized UV.
- `Source/GameXXK/Public/UI/GameXXKProloguePauseWidget.h`
- `Source/GameXXK/Private/UI/GameXXKProloguePauseWidget.cpp`
  - Minimal Continue / Return to Desktop safety overlay; no story or Guide ownership.
- `Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h`
- `Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp`
  - World Actor, camera/markers, texture fallback, hero snapshot, motion, pause, handoff, and fail-open cleanup.

### Existing runtime integration

- `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
  - Stable Qingshan target and transient carriage option helpers.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
  - Replace only action `654`'s inert return with a semantic carriage request.
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
  - Guarded story travel, active-Rig registration, scoped input snapshot, Escape routing, pause overlay, cleanup, and desktop return.
- `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap`
  - Add exactly one managed dormant Rig at the existing PlayerStart; do not move existing actors.

### Tests, focused editor scripts, and evidence

- `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- `Content/Python/gamexxk_place_prologue_carriage_rig.py`
- `Content/Python/gamexxk_validate_prologue_carriage_preview.py`
- `Content/Python/gamexxk_probe_prologue_carriage_preview.py`
- `scripts/test_prologue_carriage_policy.py`
- `docs/production/current-goal-acceptance.md`

---

### Task 1: Route the story button into an isolated transient town-travel request

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h:495-610`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:7979-8010,8763-8782`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h:220-235,340-370,515-530`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp:540-550,2940-2990`
- Modify: `Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:1875-2000`

- [ ] **Step 1: Replace the placeholder expectations with failing carriage-request tests**

Add the following expectations before production declarations exist:

```cpp
TestEqual(TEXT("carriage preview target is the playable Qingshan map"),
    GameXXKLevelFlow::QingshanTownGameplayMap(),
    FName(TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")));
TestEqual(TEXT("carriage preview option is stable"),
    GameXXKLevelFlow::CarriagePreviewTravelOptions(),
    FString(TEXT("GameXXKIntro=CarriagePreview")));
TestTrue(TEXT("carriage option parses from a travelled URL"),
    GameXXKLevelFlow::HasCarriagePreviewTravelOption(
        TEXT("?GameXXKIntro=CarriagePreview")));
TestFalse(TEXT("ordinary town travel is not a carriage preview"),
    GameXXKLevelFlow::HasCarriagePreviewTravelOption(TEXT("")));
```

Rename the Workbench test to `GameXXK.DesktopTraining.Workbench.StoryQuestCarriageRequest`, bind a test delegate, click the real action button, and require exactly one request while the runtime-state snapshot remains identical:

```cpp
int32 RequestCount = 0;
Widget->SetStoryCarriageRequestedForTest(
    FGameXXKStoryCarriageRequested::CreateLambda([&RequestCount]()
    {
        ++RequestCount;
        return true;
    }));
Button->HandleClicked();
TestEqual(TEXT("one click emits one carriage request"), RequestCount, 1);
TestEqual(TEXT("story request preserves story records"),
    Subsystem->GetRuntimeState().NarrativeProgress.StoryProgressById.Num(),
    RuntimeBefore.NarrativeProgress.StoryProgressById.Num());
TestEqual(TEXT("story request preserves task records"),
    Subsystem->GetRuntimeState().NarrativeProgress.TaskProgressById.Num(),
    RuntimeBefore.NarrativeProgress.TaskProgressById.Num());
TestEqual(TEXT("story request preserves tracked task"),
    Subsystem->GetRuntimeState().NarrativeProgress.TrackedTaskId,
    RuntimeBefore.NarrativeProgress.TrackedTaskId);
TestEqual(TEXT("story request preserves ordered formation"),
    Subsystem->GetRuntimeState().CardRun.OrderedFormation,
    RuntimeBefore.CardRun.OrderedFormation);
```

- [ ] **Step 2: Run a cold compile to prove the red boundary**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120 --filter "[TDD]"
```

Expected: UBT fails because `QingshanTownGameplayMap`, `CarriagePreviewTravelOptions`, `HasCarriagePreviewTravelOption`, `FGameXXKStoryCarriageRequested`, and the widget test setter do not yet exist. Do not treat the expected compile failure as a green build.

- [ ] **Step 3: Add the stable LevelFlow helpers**

Add these declarations and implementations:

```cpp
// GameXXKLevelFlow.h
GAMEXXK_API FName QingshanTownGameplayMap();
GAMEXXK_API FString CarriagePreviewTravelOptions();
GAMEXXK_API bool HasCarriagePreviewTravelOption(const FString& Options);

// GameXXKLevelFlow.cpp
FName GameXXKLevelFlow::QingshanTownGameplayMap()
{
    return QingshanTownMap;
}

FString GameXXKLevelFlow::CarriagePreviewTravelOptions()
{
    return TEXT("GameXXKIntro=CarriagePreview");
}

bool GameXXKLevelFlow::HasCarriagePreviewTravelOption(const FString& Options)
{
    return UGameplayStatics::ParseOption(Options, TEXT("GameXXKIntro"))
        == TEXT("CarriagePreview");
}
```

- [ ] **Step 4: Convert action 654 into a semantic request**

Add a delegate and request seam without moving or rebuilding the button:

```cpp
DECLARE_DELEGATE_RetVal(bool, FGameXXKStoryCarriageRequested);

void SetStoryCarriageRequestedForTest(FGameXXKStoryCarriageRequested InRequest)
{
    StoryCarriageRequested = MoveTemp(InRequest);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RequestStoryCarriage()
{
    if (bTownMapTravelPending)
    {
        return false;
    }
    if (StoryCarriageRequested.IsBound())
    {
        return StoryCarriageRequested.Execute();
    }
    AGameXXKMVPPlayerController* Controller = ResolveMVPPlayerController();
    return Controller && Controller->RequestDesktopStoryCarriageFromWorkbench();
}
```

Change only the existing inert branch:

```cpp
if (ActionId == ActionStoryQuest)
{
    RequestStoryCarriage();
    return;
}
```

- [ ] **Step 5: Share the existing guarded town-travel transaction**

Refactor the body of `RequestDesktopTownToggleFromWorkbench()` into a private helper accepting `FName TargetMap` and `FString Options`. Keep the normal toggle's empty options, and add:

```cpp
bool AGameXXKMVPPlayerController::RequestDesktopStoryCarriageFromWorkbench()
{
    return BeginDesktopTownMapTravelFromWorkbench(
        GameXXKLevelFlow::QingshanTownGameplayMap(),
        GameXXKLevelFlow::CarriagePreviewTravelOptions());
}

// The shared helper keeps the existing session snapshot, pending guard,
// CloseWorkbench, and overlay hiding, then performs absolute travel.
UGameplayStatics::OpenLevel(World, TargetMap, true, Options);
```

Normal town toggle must still call the helper with `FString()` and must not gain the carriage option.

- [ ] **Step 6: Cold-build and run focused green tests**

Run the cold pipeline, then use the running editor's `AutomationTestToolset.RunTests` for:

```text
GameXXK.MVP.LevelFlow
GameXXK.DesktopTraining.Workbench.StoryQuestCarriageRequest
GameXXK.DesktopTraining.Workbench.TownTogglePresentation
GameXXK.DesktopTraining.Workbench.TownSessionOneShot
```

Expected: UBT succeeds and every named test reports `Success`, with zero failed and zero skipped.

- [ ] **Step 7: Commit only Task 1 hunks and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -p -- Source/GameXXK/Public/MVP/GameXXKLevelFlow.h Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git diff --cached --name-status
git diff --cached --check
git commit -m "feat: route story button to carriage preview"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

Expected: the commit contains only the new semantic request/travel hunks; the cached list afterward contains exactly the three protected deletions.

---

### Task 2: Build the deterministic carriage timeline and atlas-frame rules

**Files:**
- Create: `Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageTypes.h`
- Create: `Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageRules.h`
- Create: `Source/GameXXK/Private/Prologue/GameXXKPrologueCarriageRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRulesTest.cpp`

- [ ] **Step 1: Write the compile-red timeline test**

Cover URL activation, every phase, pause freezing, atlas ranges, exact hold duration, cancellation, and one-shot finish:

```cpp
FGameXXKPrologueCarriageState State;
FGameXXKPrologueCarriageConfig Config;
FGameXXKPrologueCarriageStepOutput Output;

TestTrue(TEXT("preview starts"),
    FGameXXKPrologueCarriageRules::Start(State));
TestEqual(TEXT("starts arriving"), State.Phase,
    EGameXXKPrologueCarriagePhase::Arriving);

FGameXXKPrologueCarriageRules::Advance(Config.ArrivalSeconds, Config, State, Output);
TestEqual(TEXT("arrival reaches parked"), State.Phase,
    EGameXXKPrologueCarriagePhase::Parked);
FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output);
TestTrue(TEXT("parking reveals hero"), Output.bRevealHero);
TestEqual(TEXT("hero reveal is explicit"), State.Phase,
    EGameXXKPrologueCarriagePhase::HeroRevealed);
FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output);
TestEqual(TEXT("hold begins"), State.Phase,
    EGameXXKPrologueCarriagePhase::HoldTwoSeconds);

FGameXXKPrologueCarriageRules::SetPaused(State, true);
const FGameXXKPrologueCarriageState PausedBefore = State;
FGameXXKPrologueCarriageRules::Advance(10.0f, Config, State, Output);
TestEqual(TEXT("pause freezes state"), State, PausedBefore);

FGameXXKPrologueCarriageRules::SetPaused(State, false);
FGameXXKPrologueCarriageRules::Advance(2.0f, Config, State, Output);
TestEqual(TEXT("two seconds begins departure"), State.Phase,
    EGameXXKPrologueCarriagePhase::Departing);
FGameXXKPrologueCarriageRules::Advance(0.01f, Config, State, Output);
TestTrue(TEXT("departure uses only audited running frames"),
    Output.AtlasFrameIndex >= 0 && Output.AtlasFrameIndex <= 35);
```

Also require `0–59` for arrival, `0–59` looping for idle, and `0–35` looping for departure. The frame audit source is `Saved/HarnessReports/prologue-carriage-frame-audit-luna.md`; the contact-sheet evidence identifies `36–46` as braking and `47–59` as parked.

- [ ] **Step 2: Run the cold compile and require the expected missing-type failure**

Run the standard cold pipeline. Expected: compilation fails on the new Prologue types/rules, proving the test is active.

- [ ] **Step 3: Implement the pure data types**

Define exact defaults:

```cpp
UENUM()
enum class EGameXXKPrologueCarriagePhase : uint8
{
    Dormant,
    Arriving,
    Parked,
    HeroRevealed,
    HoldTwoSeconds,
    Departing,
    Handoff,
    Finished,
    Cancelled,
};

struct FGameXXKPrologueCarriageConfig
{
    float ArrivalSeconds = 4.04f;
    float HoldSeconds = 2.0f;
    float DepartureSeconds = 4.04f;
    float FramesPerSecond = 14.851485f;
    int32 FullFrameCount = 60;
    int32 DepartureFirstFrame = 0;
    int32 DepartureLastFrame = 35;
};
```

`FGameXXKPrologueCarriageState` contains only phase, phase elapsed seconds, paused flag, and finish-broadcast flag. Give it an explicit value comparison used by tests:

```cpp
bool operator==(const FGameXXKPrologueCarriageState& Other) const
{
    return Phase == Other.Phase
        && FMath::IsNearlyEqual(PhaseElapsedSeconds, Other.PhaseElapsedSeconds)
        && bPaused == Other.bPaused
        && bFinishBroadcastConsumed == Other.bFinishBroadcastConsumed;
}
```

`FGameXXKPrologueCarriageStepOutput` contains phase progress, atlas kind/frame, and transition flags; it contains no gameplay/save state.

- [ ] **Step 4: Implement one-way rules with bounded time**

`Advance` must clamp negative/NaN delta to zero, leave a paused/terminal state unchanged, use immediate explicit `Parked` and `HeroRevealed` boundaries, and move to `Finished` only after `Handoff`. `Cancel` must be idempotent. `ConsumeFinishBroadcast` must return true exactly once.

- [ ] **Step 5: Build and run the exact rules test green**

Expected: cold UBT succeeds; `GameXXK.Prologue.Carriage.Rules` reports `Success` with no warnings.

- [ ] **Step 6: Commit Task 2 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageTypes.h Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageRules.h Source/GameXXK/Private/Prologue/GameXXKPrologueCarriageRules.cpp Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRulesTest.cpp
git diff --cached --check
git commit -m "feat: add carriage preview timeline rules"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 3: Render existing carriage atlases in one world-space widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKPrologueCarriageWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKPrologueCarriageWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageWidgetTest.cpp`
- Create: `scripts/test_prologue_carriage_policy.py`

- [ ] **Step 1: Write the widget and policy tests first**

The C++ test requires exact normalized UVs for an 8×8 atlas:

```cpp
TestEqual(TEXT("frame zero UV"),
    UGameXXKPrologueCarriageWidget::FrameUvForTest(0),
    FBox2f(FVector2f(0.0f, 0.0f), FVector2f(0.125f, 0.125f)));
TestEqual(TEXT("frame 59 UV"),
    UGameXXKPrologueCarriageWidget::FrameUvForTest(59),
    FBox2f(FVector2f(0.375f, 0.875f), FVector2f(0.5f, 1.0f)));
TestTrue(TEXT("real image receives a texture frame"),
    Widget->SetAtlasFrame(Texture, 35));
TestFalse(TEXT("empty texture fails safely"),
    Widget->SetAtlasFrame(nullptr, 0));
```

The Python policy test scans only new Prologue runtime files and rejects `SetWindowsHookEx`, `SendInput`, `mouse_event`, `HideWindow`, `ShowWindow`, `bIdleStripFolded`, `OrderedFormation`, `NarrativeProgress`, and `GuideProgress`.

- [ ] **Step 2: Prove red**

Run cold UBT and `python -m unittest scripts.test_prologue_carriage_policy -v`. Expected: C++ compile fails because the widget is absent; policy fails until its exact file list exists.

- [ ] **Step 3: Implement the programmatic atlas widget**

Create one `UImage` filling a transparent root. Do not create or reimport Sprite/Flipbook assets. `SetAtlasFrame` applies the existing Texture2D and normalized UV box:

```cpp
const int32 SafeFrame = FMath::Clamp(FrameIndex, 0, 59);
const int32 Column = SafeFrame % 8;
const int32 Row = SafeFrame / 8;
const FVector2f Min(Column / 8.0f, Row / 8.0f);
FSlateBrush Brush;
Brush.SetResourceObject(Texture);
Brush.ImageSize = FVector2D(512.0f, 512.0f);
Brush.SetUVRegion(FBox2f(Min, Min + FVector2f(0.125f, 0.125f)));
CarriageImage->SetBrush(Brush);
```

Use a 512×512 logical draw surface and transparent hit-test-invisible visibility. The Rig will set the WidgetComponent pivot to `(0.5, 1.0)` for bottom-center grounding.

- [ ] **Step 4: Run widget and policy gates green**

Expected: `GameXXK.Prologue.Carriage.Widget` succeeds and Python policy reports all tests `OK`.

- [ ] **Step 5: Commit Task 3 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Source/GameXXK/Public/UI/GameXXKPrologueCarriageWidget.h Source/GameXXK/Private/UI/GameXXKPrologueCarriageWidget.cpp Source/GameXXK/Private/Tests/GameXXKPrologueCarriageWidgetTest.cpp scripts/test_prologue_carriage_policy.py
git diff --cached --check
git commit -m "feat: render carriage preview atlases"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 4: Implement the independent Qingshan carriage Rig and fail-open handoff

**Files:**
- Create: `Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h`
- Create: `Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Write compile-red CDO and presentation-ownership tests**

Require exact defaults and no save-facing dependencies:

```cpp
const AGameXXKPrologueCarriageRig* Rig =
    GetDefault<AGameXXKPrologueCarriageRig>();
TestEqual(TEXT("start is camera-left by 400"),
    Rig->GetStartOffsetForTest(), FVector(0.0f, -400.0f, 0.0f));
TestEqual(TEXT("stop is the anchor"),
    Rig->GetStopOffsetForTest(), FVector::ZeroVector);
TestEqual(TEXT("exit continues in the same direction"),
    Rig->GetExitOffsetForTest(), FVector(0.0f, 800.0f, 0.0f));
TestEqual(TEXT("hero is closer to the camera than carriage"),
    Rig->GetHeroRevealOffsetForTest(), FVector(-80.0f, 0.0f, 0.0f));
TestEqual(TEXT("carriage sorts behind hero"),
    Rig->GetCarriageSortPriorityForTest(), 9);
TestFalse(TEXT("ordinary URL stays dormant"),
    Rig->ShouldActivateForOptionsForTest(TEXT("")));
TestTrue(TEXT("preview URL activates"),
    Rig->ShouldActivateForOptionsForTest(
        TEXT("?GameXXKIntro=CarriagePreview")));
```

Add controller snapshot tests modeled after `GameXXKBattleOverlayCoordinatorTest`: acquire only locks the flags it owns, release restores an already-ignored input state exactly, and a second release is harmless.

- [ ] **Step 2: Prove red with cold UBT**

Expected: missing Rig class and controller presentation methods fail compilation.

- [ ] **Step 3: Create the dormant component hierarchy**

Constructor defaults:

```cpp
Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
SetRootComponent(Root);
CarriageStart = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageStart"));
CarriageStart->SetupAttachment(Root);
CarriageStart->SetRelativeLocation(FVector(0.0f, -400.0f, 0.0f));
CarriageStop = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageStop"));
CarriageStop->SetupAttachment(Root);
CarriageExit = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageExit"));
CarriageExit->SetupAttachment(Root);
CarriageExit->SetRelativeLocation(FVector(0.0f, 800.0f, 0.0f));
HeroReveal = CreateDefaultSubobject<USceneComponent>(TEXT("HeroReveal"));
HeroReveal->SetupAttachment(Root);
HeroReveal->SetRelativeLocation(FVector(-80.0f, 0.0f, 0.0f));
```

Add a fixed `USpringArmComponent` using the town hero's established top-down envelope (`TargetArmLength=900`, absolute rotation `(-60,0,0)`) and an attached `UCameraComponent`. Add a `UWidgetComponent` with draw size 512×512, transparent world space, bottom-center pivot, yaw 90, relative ground offset Z=-72, scale 0.75, collision disabled, and translucent sort priority 9. Default Actor tick is disabled.

- [ ] **Step 4: Load existing atlas textures with an isolated fallback**

Use soft paths only to the four existing carriage textures:

```text
/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_run_stop_2k_atlas.T_cinematic_carriage_run_stop_2k_atlas
/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_post_stop_idle_2k_atlas.T_cinematic_carriage_post_stop_idle_2k_atlas
/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_run_stop_1k_atlas.T_cinematic_carriage_run_stop_1k_atlas
/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_post_stop_idle_1k_atlas.T_cinematic_carriage_post_stop_idle_1k_atlas
```

Prefer 2K; if either 2K texture fails, load the matching 1K resource. Never run the old bulk importer and never resave those textures.

- [ ] **Step 5: Implement activation, movement, reveal, and cleanup**

In `BeginPlay`, parse `GetWorld()->URL.Options`; without the option remain hidden and tickless. With the option, retry player/controller discovery on bounded next ticks, then:

1. snapshot the unique possessed `AGameXXKHeroCharacter`;
2. ask PlayerController to acquire the scoped presentation token;
3. hide hero, disable capsule collision, reset movement input, and disable movement;
4. switch ViewTarget to the Rig camera;
5. show the carriage at `CarriageStart` and start pure rules.

Tick applies `FMath::InterpEaseOut` arrival position from Start to Stop, uses rules output to set frames, moves/reveals the existing hero at `HeroReveal`, holds, then linearly moves Stop to Exit while looping frames 0–35. Handoff hides the carriage and calls one idempotent cleanup path.

The cleanup path must restore the hero's original hidden/collision/movement state, release the controller token, restore the hero ViewTarget, disable tick, and broadcast the runtime completion delegate at most once. Failure/cancel uses the same cleanup but never broadcasts success.

- [ ] **Step 6: Run Rig, rules, widget, LevelFlow, and player-flow tests green**

Required filters:

```text
GameXXK.Prologue.Carriage
GameXXK.MVP.LevelFlow
GameXXK.MVP.PlayableShell
GameXXK.MVP.Town.ShellInputInteractionFollower
```

Expected: every discovered named test succeeds; no formation, idle, narrative, or guide state changes appear in logs.

- [ ] **Step 7: Commit Task 4 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp
git add -p -- Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp
git diff --cached --check
git commit -m "feat: add Qingshan carriage preview rig"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 5: Add a minimal pause overlay and guaranteed Escape/return behavior

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKProloguePauseWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKProloguePauseWidget.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp:349-487`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp`

- [ ] **Step 1: Write failing pause, resume, and return tests**

Require:

```cpp
Rig->SetSequencePausedForTest(true);
const FGameXXKPrologueCarriageState Before = Rig->GetTimelineStateForTest();
Rig->AdvanceForTest(10.0f);
TestEqual(TEXT("paused rig does not advance"),
    Rig->GetTimelineStateForTest(), Before);
Rig->SetSequencePausedForTest(false);
Rig->AdvanceForTest(0.5f);
TestTrue(TEXT("resumed rig advances"),
    Rig->GetTimelineStateForTest().PhaseElapsedSeconds
        > Before.PhaseElapsedSeconds);
```

Create a pause widget, bind both delegates, invoke its test seams, and require Continue and Return each fire once. Add controller tests that Escape is always handled while a Rig is active, gameplay keys are consumed while running, and paused UI mouse input still reaches UMG.

- [ ] **Step 2: Prove red**

Cold-build; expected missing pause widget/Rig APIs cause compilation failure.

- [ ] **Step 3: Build the minimal safety overlay**

Programmatically create a centered translucent overlay with title `剧情已暂停` and two buttons `继续` and `返回桌面`. It owns no save, narrative, guide, task, or reward state. Expose delegates:

```cpp
DECLARE_DELEGATE(FGameXXKPrologueResumeRequested);
DECLARE_DELEGATE(FGameXXKPrologueReturnDesktopRequested);
```

No new art asset is required for this safety-only UI.

- [ ] **Step 4: Route Escape before ordinary town shortcuts**

At the beginning of `InputKey`, after existing Narrative handling but before Q/I/C/Tab/F routing:

```cpp
if (AGameXXKPrologueCarriageRig* Rig = ActivePrologueCarriageRig.Get())
{
    if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
    {
        Rig->TogglePauseFromController();
        return true;
    }
    if (!Rig->IsSequencePaused())
    {
        return true;
    }
}
```

When paused, Rig shows the overlay, freezes only its timeline/frame/motion, switches to tracked `GameAndUI`, and shows the cursor. Continue removes the overlay and restores the cinematic input mode. Return first cancels/cleans the Rig, then calls a dedicated controller method that reuses the existing guarded town-to-desktop travel transaction.

`EndPlay`, `HandlePreLoadMapWithContext`, repeated cancel, missing widget, and missing controller all call idempotent cleanup.

- [ ] **Step 5: Run focused pause and input regressions green**

Required filters:

```text
GameXXK.Prologue.Carriage
GameXXK.MVP.LevelFlow
GameXXK.MVP.Town
GameXXK.DesktopTraining.Workbench.StoryQuestCarriageRequest
```

Also rerun `python -m unittest scripts.test_prologue_carriage_policy -v`.

- [ ] **Step 6: Commit Task 5 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Source/GameXXK/Public/UI/GameXXKProloguePauseWidget.h Source/GameXXK/Private/UI/GameXXKProloguePauseWidget.cpp Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp
git add -p -- Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp
git diff --cached --check
git commit -m "feat: make carriage preview safely pausable"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 6: Place and validate one managed Rig in the existing Qingshan map

**Files:**
- Create: `Content/Python/gamexxk_place_prologue_carriage_rig.py`
- Create: `Content/Python/gamexxk_validate_prologue_carriage_preview.py`
- Modify: `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap`
- Test: `scripts/test_prologue_carriage_policy.py`

This task is map/visual assembly and follows the project art exception: do not use TDD for the binary placement itself; use deterministic pre/post validation and visual review.

- [ ] **Step 1: Write the guarded placement script**

The script must:

1. require the exact target map path;
2. query dirty packages and refuse to load the map if any unsaved package remains; the caller saves through MCP first;
3. find exactly one existing `PlayerStart` and never change its transform;
4. find zero or one actor labeled `GameXXK_PrologueCarriageRig` with tag `GameXXKManaged.PrologueCarriageRig`;
5. refuse to update any actor lacking that tag;
6. spawn the native Rig class only when absent;
7. set Rig transform equal to the PlayerStart transform;
8. save only the target map package;
9. emit JSON containing pre/post PlayerStart transform, actor count, Rig transform, and changed package list.

Core guard:

```python
if existing and MANAGED_TAG not in {str(tag) for tag in existing.tags}:
    raise RuntimeError("refusing to update unowned prologue carriage actor")
if len(player_starts) != 1:
    raise RuntimeError(f"expected exactly one PlayerStart, got {len(player_starts)}")
```

- [ ] **Step 2: Write the deterministic validator**

Validate without moving actors:

- exact map and one managed Rig;
- Rig transform equals PlayerStart transform;
- start-to-stop distance is 400 ± 0.1 Unreal units;
- stop-to-exit direction has positive dot product with the start-to-stop direction;
- hero marker is closer to the Rig camera than the carriage path;
- 2K and 1K run/idle Texture2D assets exist with dimensions 2048/1024;
- the Rig class has no save-game property and stays dormant without URL option.

- [ ] **Step 3: Save through MCP, then run placement through UE MCP**

Call `UnrealMCPClient.save_dirty_packages()` and require `dirty_after=[]`, then use `UnrealMCPClient.run_project_python_file` for the placement script. Do not use UnrealBridge. Expected JSON: one managed Rig, unchanged PlayerStart transform, and exactly one dirty/saved map package.

- [ ] **Step 4: Run validator and inspect the map diff boundary**

Run the validator through MCP and require `ok: true`. Then verify:

```powershell
git status --short -- Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap Content/Python/gamexxk_place_prologue_carriage_rig.py Content/Python/gamexxk_validate_prologue_carriage_preview.py
```

Expected: only the target map and two focused scripts appear for this task.

- [ ] **Step 5: Commit Task 6 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Content/Python/gamexxk_place_prologue_carriage_rig.py Content/Python/gamexxk_validate_prologue_carriage_preview.py Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap
git diff --cached --check
git commit -m "feat: place Qingshan carriage preview rig"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 7: Prove runtime invariants and expose a read-only acceptance probe

**Files:**
- Create: `Content/Python/gamexxk_probe_prologue_carriage_preview.py`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp`
- Modify: `scripts/test_prologue_carriage_policy.py`

- [ ] **Step 1: Add failure-path and state-invariance tests**

Snapshot and compare all relevant state before/after success, cancel, pause/return, missing texture, missing hero, and timeout:

```cpp
const FGameXXKCarriageInvariantSnapshot RuntimeBefore =
    CaptureCarriageInvariantSnapshot(Subsystem->GetRuntimeState());
Rig->ForceFailureForTest(EGameXXKPrologueCarriageFailure::MissingArrivalTexture);
TestEqual(TEXT("failure does not mutate gameplay runtime"),
    CaptureCarriageInvariantSnapshot(Subsystem->GetRuntimeState()),
    RuntimeBefore);
TestFalse(TEXT("failure releases move lock"), Controller->IsMoveInputIgnored());
TestFalse(TEXT("failure releases look lock"), Controller->IsLookInputIgnored());
TestFalse(TEXT("failure leaves no active Rig"),
    Controller->HasActivePrologueCarriageForTest());
```

Define `FGameXXKCarriageInvariantSnapshot` in the test file with explicit value fields for Training, `OrderedFormation`, inventory, equipment, cards, experience, task/narrative/guide progress, and rewards, plus an explicit `operator==`. Do not rely on an undeclared whole-`FGameXXKRuntimeState` equality operator.

- [ ] **Step 2: Implement a read-only probe**

`gamexxk_probe_prologue_carriage_preview.py observe` must only report:

- current map;
- URL option;
- Rig actor count and current phase;
- elapsed phase time and frame index;
- carriage/start/stop/exit/hero transforms;
- hero hidden, collision, movement and input-lock state;
- current ViewTarget;
- whether pause overlay is visible.

The probe must contain no click, key, mouse, window, map-load, save, or state-mutation command. Add policy assertions rejecting `click`, `SendInput`, `key_down`, `OpenLevel`, and `save_dirty_packages` in this file.

- [ ] **Step 3: Cold-build and run the full task-focused gate**

After cold UBT, run exactly:

```text
GameXXK.Prologue.Carriage
GameXXK.MVP.LevelFlow
GameXXK.DesktopTraining.Workbench.StoryQuestCarriageRequest
GameXXK.DesktopTraining.Workbench.TownTogglePresentation
GameXXK.DesktopTraining.Workbench.TownSessionOneShot
GameXXK.MVP.Town.ShellInputInteractionFollower
GameXXK.MVP.PlayableShell.GameModeDefaults
```

Run:

```powershell
python -m unittest scripts.test_prologue_carriage_policy -v
python -m py_compile Content/Python/gamexxk_place_prologue_carriage_rig.py Content/Python/gamexxk_validate_prologue_carriage_preview.py Content/Python/gamexxk_probe_prologue_carriage_preview.py
python scripts/harness_state_validator.py
git diff --check
```

Expected: focused Automation zero failures/skips; Python checks exit 0; harness validator `OK` apart from already-recorded old metadata warnings; diff check exits 0.

- [ ] **Step 4: Commit Task 7 and restore protected staging**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- Content/Python/gamexxk_probe_prologue_carriage_preview.py Source/GameXXK/Private/Tests/GameXXKPrologueCarriageRigTest.cpp scripts/test_prologue_carriage_policy.py
git diff --cached --check
git commit -m "test: cover carriage preview recovery"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
```

---

### Task 8: Player visual acceptance, Luna review, and rolling evidence

**Files:**
- Modify: `docs/production/current-goal-acceptance.md`
- Evidence only: `Saved/Codex/*`, `Saved/HarnessReports/*`

- [ ] **Step 1: Prepare the canonical manual surface without synthetic input**

Save dirty packages through UE MCP, cold-build once more, relaunch on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, and ensure PIE is stopped. Do not run an automation input driver.

- [ ] **Step 2: Ask the user to click the real story button**

The user performs the click. While the sequence runs, use only the read-only probe and screenshot capture to gather:

1. fixed camera + arrival;
2. parked carriage + hero in front;
3. departure behind hero;
4. restored hero camera/control.

- [ ] **Step 3: Review captures with Luna Max**

Invoke `C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1 -Effort max` and require an evidence-backed verdict for:

- carriage apparent size;
- bottom-center ground contact;
- no atlas clipping or alpha box;
- exact stop/reveal composition;
- carriage visually behind hero during departure;
- no camera jump at handoff.

Only tune properties inside the new Rig. Do not move existing PlayerStart, NPCs, camera, PaperZD/Flipbooks, scene planes, or protected art.

- [ ] **Step 4: Manually verify replay, pause, return, and restart**

Require the user-visible chain:

```text
click → full carriage preview → return desktop → click → full replay
click → Escape → pause → Continue → resumes same phase
click → Escape → Return Desktop → no lock
close/restart → desktop only, no automatic carriage
```

If the user has not performed a chain, record it as unverified; do not infer it from Automation.

- [ ] **Step 5: Update rolling acceptance honestly**

Record commit IDs, cold UBT command/result, exact Automation counts, Python checks, map-validator JSON, read-only phase evidence, Luna verdict, user manual steps actually performed, and any unrelated baseline failures. State explicitly that Guide implementation has not started.

- [ ] **Step 6: Commit evidence documentation, restore protected staging, and push**

```powershell
git restore --staged -- $ProtectedDeletions
git add -- docs/production/current-goal-acceptance.md
git diff --cached --check
git commit -m "docs: record carriage preview acceptance"
git add -u -- $ProtectedDeletions
git diff --cached --name-status
git push origin main
```

Verify `git ls-remote origin refs/heads/main` equals local `HEAD`. The three protected deletions must remain the only cached paths after the push.

---

## Completion checklist

- [ ] Story action `654` repeatedly launches Qingshan with only the transient carriage option.
- [ ] Normal town entry never activates the Rig.
- [ ] The existing atlas already contains horse, driver, and carriage; no separate horse is layered.
- [ ] Arrival uses frames 0–59, parked uses the idle atlas, departure loops only audited frames 0–35.
- [ ] Start-to-stop distance is 400 Unreal units; hero appears at the car-front marker; departure continues in the same direction behind hero.
- [ ] Hold time is exactly 2 seconds of unpaused game time.
- [ ] Success, pause, cancel, failure, return, map travel, and shutdown cannot leave camera/input/hero state locked.
- [ ] No story, guide, task, reward, idle, party, inventory, equipment, cards, currency, or experience state changes.
- [ ] No save version or new persistent progress field.
- [ ] Cold UBT, focused Automation, policy checks, map validator, read-only probe, Luna review, and player-visible acceptance are recorded.
- [ ] Existing user changes and three protected staged deletions remain intact.
- [ ] Guide work remains a separate next design/implementation cycle after carriage acceptance.
