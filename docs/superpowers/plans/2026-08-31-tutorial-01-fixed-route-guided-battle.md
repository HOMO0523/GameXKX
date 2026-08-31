# Tutorial 0-1 Fixed Route and Guided Battle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the incorrect direct-BattleBoard tutorial boot with an isolated three-node 0-1 route, a temporary Hero + existing partner + YueBai formation, and the approved nine-step new-player battle guide.

**Architecture:** `UGameXXKTutorial01SessionSubsystem` owns only transient route/formation/guide state and projects a fixed route into the existing `UGameXXKOneGameRouteMapWidget`; `AGameXXKMVPPlayerController` remains the route/battle overlay host and intercepts tutorial terminal states before ordinary rewards. The generic Guide stack gains local-coordinate multi-target output, while a tutorial-only host binds the approved JSON guide to BattleBoard semantic targets and releases every gate on failure.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, existing card-battle adapter and BattleOverlayCoordinator, JSON-authored Guide DataAssets, UE Automation Framework, focused Unreal Python through UE MCP, cold UBT only.

---

## Scope and repository guard

- Work directly in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- Do not use UnrealBridge, Live Coding, Hot Reload, synthetic mouse/keyboard input, window hiding, window showing, automatic minimization, or startup-triggered tests.
- Do not move or retune the town, statue, camera, PaperZD, HD2D, BattleBoard art, or authored route assets.
- Preserve all unrelated dirty files. Before editing an overlapping file, inspect its current diff and patch only the required hunk.
- Before closing/restarting UE, stop PIE and save dirty packages through `scripts/ue_mcp_client.py`/UE MCP. Never force-close an editor that may contain unsaved changes.
- Keep these three user-staged deletions staged before and after every task commit, but never include them in a task commit:

```powershell
$TutorialPlanProtectedDeletions = @(
  'Content/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.uasset',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_BackpackScrollbarRight.png'
)
```

- Commit protocol: `git restore --staged -- $TutorialPlanProtectedDeletions`; stage only named task paths; inspect `git diff --cached --name-status`; run `git diff --cached --check`; commit; then `git add -u -- $TutorialPlanProtectedDeletions` and verify the cached diff contains exactly those three deletions.
- Runtime/gameplay changes use red-green TDD. Map/asset import uses deterministic validation plus visual review.
- Player acceptance is manual. Automation may call semantic C++ seams only when explicitly launched by the developer; it must never move the player's pointer or click UI.

## Current-state correction

The following uncommitted implementation is a known wrong turn and must be replaced, not built upon as a direct battle host:

- `TutorialBattleOnly` boot profile;
- `BeginTutorial01Battle()` from PlayerController `BeginPlay`;
- `PrepareTutorial01BattleForTest()` immediately calling `StartNarrativeEncounter`;
- the tutorial map showing BattleBoard without `UGameXXKBattleOverlayCoordinator`;
- ordinary `ResolveBattleVictory`/`FailDungeonToTown` handling tutorial terminal states.

Keep `AGameXXKTutorial01GameMode` and the actor-free map, but make the map boot a route surface. Do not delete working prologue, map-item, YueBai-follow, statue, or guide-choice changes.

## File map

### New focused units

- `Source/GameXXK/Public/MVP/GameXXKTutorial01RouteRules.h` — pure fixed-node route state and legal transitions.
- `Source/GameXXK/Private/MVP/GameXXKTutorial01RouteRules.cpp` — fixed projection implementation.
- `Source/GameXXK/Public/Guide/GameXXKTutorial01GuideHost.h` — tutorial-local Guide lifecycle, gate, suspension, watchdog, and failure delegate.
- `Source/GameXXK/Private/Guide/GameXXKTutorial01GuideHost.cpp` — host implementation.
- `Source/GameXXK/Public/UI/GameXXKBattleGuideBubbleWidget.h` — BattleBoard-local YueBai bubble presentation.
- `Source/GameXXK/Private/UI/GameXXKBattleGuideBubbleWidget.cpp` — safe-stage bubble layout.
- `Source/GameXXK/Public/UI/GameXXKTutorial01ResultWidget.h` — paper-backed Retry/Return failure surface.
- `Source/GameXXK/Private/UI/GameXXKTutorial01ResultWidget.cpp` — result surface implementation.
- `SourceAssets/Narrative/Guides/Guide.Battle.Tutorial01.NewPlayer.guide.json` — approved nine visible steps.
- `Content/GameXXK/Narrative/Guides/DA_Guide_Battle_Tutorial01_NewPlayer.uasset` — imported Guide DataAsset.
- `Source/GameXXK/Private/Tests/GameXXKTutorial01RouteRulesTest.cpp`.
- `Source/GameXXK/Private/Tests/GameXXKTutorial01GuideHostTest.cpp`.
- `Source/GameXXK/Private/Tests/GameXXKTutorial01BattleFlowTest.cpp`.
- `scripts/test_tutorial01_guide_policy.py` — static isolation/no-input-automation/local-coordinate guard.

### Existing integration files

- `Source/GameXXK/Public/MVP/GameXXKTutorial01SessionSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKTutorial01SessionSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- `Source/GameXXK/Public/Guide/GameXXKGuideAsset.h`
- `Source/GameXXK/Private/Guide/GameXXKGuideAsset.cpp`
- `Source/GameXXK/Public/Guide/GameXXKGuideRules.h`
- `Source/GameXXK/Private/Guide/GameXXKGuideRules.cpp`
- `Source/GameXXK/Public/Guide/GameXXKGuideCoordinator.h`
- `Source/GameXXK/Private/Guide/GameXXKGuideCoordinator.cpp`
- `Source/GameXXK/Public/Guide/GameXXKGuideTargetRegistry.h`
- `Source/GameXXK/Private/Guide/GameXXKGuideTargetRegistry.cpp`
- `Source/GameXXK/Public/UI/GameXXKGuideOverlayWidget.h`
- `Source/GameXXK/Private/UI/GameXXKGuideOverlayWidget.cpp`
- `Source/GameXXK/Public/Town/GameXXKPrologueAftermathController.h`
- `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- `scripts/validate_guide_json.py`
- `scripts/test_guide_json_validation.py`
- `Content/Python/gamexxk_import_guide_json.py`
- `Content/Python/gamexxk_validate_tutorial01_map.py`

### Existing tests to extend

- `Source/GameXXK/Private/Tests/GameXXKTutorial01SessionTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTutorial01PlayerFlowTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKGuideRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKGuideWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKGuideLiveTargetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`

---

### Task 1: Add fixed route rules and transient tutorial battle preparation

**Files:**
- Create: `Source/GameXXK/Public/MVP/GameXXKTutorial01RouteRules.h`
- Create: `Source/GameXXK/Private/MVP/GameXXKTutorial01RouteRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorial01RouteRulesTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKTutorial01SessionSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKTutorial01SessionSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorial01SessionTest.cpp`

- [ ] **Step 1: Write the red fixed-route and session tests**

Create `GameXXK.Tutorial01.RouteRules` with these exact route expectations:

```cpp
FGameXXKTutorial01RouteState Route;
FGameXXKTutorial01RouteRules::Initialize(Route);
TestTrue(TEXT("start is already complete"),
    Route.VisitedNodeIds.Num() == 1 && Route.VisitedNodeIds.Contains(100));
TestTrue(TEXT("battle is the only reachable node"),
    Route.ReachableNodeIds.Num() == 1 && Route.ReachableNodeIds.Contains(101));
EGameXXKTutorial01RouteAction Action = EGameXXKTutorial01RouteAction::None;
TestFalse(TEXT("start cannot execute"),
    FGameXXKTutorial01RouteRules::RequestNode(Route, 100, Action));
TestTrue(TEXT("battle executes once"),
    FGameXXKTutorial01RouteRules::RequestNode(Route, 101, Action));
TestEqual(TEXT("battle action"), Action, EGameXXKTutorial01RouteAction::StartBattle);
TestTrue(TEXT("victory advances route"), FGameXXKTutorial01RouteRules::MarkVictory(Route));
TestTrue(TEXT("battle is complete"),
    Route.VisitedNodeIds.Num() == 2
    && Route.VisitedNodeIds.Contains(100)
    && Route.VisitedNodeIds.Contains(101));
TestTrue(TEXT("return is now reachable"),
    Route.ReachableNodeIds.Num() == 1 && Route.ReachableNodeIds.Contains(102));
TestTrue(TEXT("return executes"),
    FGameXXKTutorial01RouteRules::RequestNode(Route, 102, Action));
TestEqual(TEXT("return action"), Action, EGameXXKTutorial01RouteAction::ReturnTown);
```

Extend `GameXXK.Tutorial01.Session` to assert:

```cpp
FGameXXKRuntimeState RouteRuntime;
TestTrue(TEXT("session projects route runtime"), Session->BuildRouteRuntime(RouteRuntime));
TestEqual(TEXT("route boot is not battle"), RouteRuntime.Screen, EGameXXKScreen::DungeonMap);
TestFalse(TEXT("route boot has no active battle"), RouteRuntime.CardRun.bHasActiveCardBattle);

FGameXXKRuntimeState BattleSeed;
TestTrue(TEXT("session builds battle seed"), Session->BuildBattleSeedRuntime(BattleSeed, &Error));
FName TutorialNpc;
TestTrue(TEXT("tutorial formation resolves NPC"),
    FGameXXKPartyFormationRules::ResolveQuestNpcId(BattleSeed, TutorialNpc, &Error));
TestEqual(TEXT("YueBai replaces Tusi only in seed"), TutorialNpc, FName(TEXT("Npc.YueBai")));
TestTrue(TEXT("snapshot still has original formation"),
    Session->GetContextForTest().RuntimeBeforeTutorial.CardRun.OrderedFormation.Members
        == Before.CardRun.OrderedFormation.Members);
```

After starting `Encounter.Main.XuXiake.0-1`, call `ArrangeDeterministicOpeningHand` and assert the first three `CardId` values are exactly HengJian, SuiYan, FengShen and `ActiveInstanceIds` is unchanged.

- [ ] **Step 2: Run red**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 180 --filter "[TDD]"
```

Expected: compile failure for missing route types and new session APIs.

- [ ] **Step 3: Implement the pure fixed route model**

Define the public contract exactly:

```cpp
UENUM()
enum class EGameXXKTutorial01RouteAction : uint8
{
    None,
    StartBattle,
    ReturnTown,
};

USTRUCT()
struct GAMEXXK_API FGameXXKTutorial01RouteState
{
    GENERATED_BODY()
    UPROPERTY(Transient) TSet<int32> VisitedNodeIds;
    UPROPERTY(Transient) TSet<int32> ReachableNodeIds;
    UPROPERTY(Transient) bool bBattleInProgress = false;
    UPROPERTY(Transient) bool bBattleWon = false;
};

class GAMEXXK_API FGameXXKTutorial01RouteRules final
{
public:
    static constexpr int32 StartNodeId = 100;
    static constexpr int32 BattleNodeId = 101;
    static constexpr int32 ReturnTownNodeId = 102;
    static void Initialize(FGameXXKTutorial01RouteState& OutState);
    static TArray<FGameXXKRouteMapNode> BuildNodes(const FGameXXKTutorial01RouteState& State);
    static TArray<FGameXXKRouteMapEdge> BuildEdges();
    static TMap<int32, FText> BuildLabels();
    static FText BuildCompletionNotice(const FGameXXKTutorial01RouteState& State);
    static bool RequestNode(FGameXXKTutorial01RouteState& InOutState, int32 NodeId,
        EGameXXKTutorial01RouteAction& OutAction);
    static bool MarkVictory(FGameXXKTutorial01RouteState& InOutState);
    static void MarkBattleAborted(FGameXXKTutorial01RouteState& InOutState);
};
```

`BuildNodes` returns only 100 Start at `(0.50, 0.12)`, 101 Battle at `(0.50, 0.50)`, and 102 ReturnTown at `(0.50, 0.86)`; `BuildEdges` returns `{100→101, 101→102}`. `BuildLabels` returns `起点`, `0-1 战斗`, and `返回青山镇`. `BuildCompletionNotice` is empty before victory and exactly `0-1 完成` afterward. Node 101 is Battle; node 102 is Camp so it reuses a non-combat return icon without inventing a route type, while its label override prevents it from reading as a rest node.

- [ ] **Step 4: Extend the transient session without mutating the save snapshot**

Add to `FGameXXKTutorial01ReturnContext`:

```cpp
UPROPERTY(Transient) FGameXXKTutorial01RouteState RouteState;
UPROPERTY(Transient) FGameXXKGuideProgress TutorialGuideProgress;
```

Add these APIs:

```cpp
bool BuildRouteRuntime(FGameXXKRuntimeState& OutRuntimeState) const;
bool BuildBattleSeedRuntime(FGameXXKRuntimeState& OutRuntimeState, FString* OutError = nullptr);
bool ArrangeDeterministicOpeningHand(FGameXXKRuntimeState& InOutRuntimeState, FString* OutError = nullptr) const;
bool MarkBattleVictory(FGameXXKRuntimeState& OutRouteRuntime);
bool MarkBattleDefeat();
bool RequestRouteNode(int32 NodeId, EGameXXKTutorial01RouteAction& OutAction);
TArray<FGameXXKRouteMapNode> BuildRouteNodes() const;
TArray<FGameXXKRouteMapEdge> BuildRouteEdges() const;
TMap<int32, FText> BuildRouteLabels() const;
FText BuildRouteCompletionNotice() const;
const FGameXXKTutorial01RouteState& GetRouteState() const;
FGameXXKGuideProgress& GetMutableGuideProgress();
void ResetGuideForRetry();
```

`BuildBattleSeedRuntime` starts from `RuntimeBeforeTutorial`, calls `FGameXXKPartyFormationRules::SetQuestNpc(Candidate, TEXT("Npc.YueBai"))`, and never writes `RuntimeBeforeTutorial`.

Implement opening-hand arrangement by moving existing instances, never manufacturing cards:

```cpp
const TArray<FName> Required = {
    TEXT("Hero.Generic.HengJianShouShi"),
    TEXT("Hero.Generic.SuiYanJi"),
    TEXT("Hero.Generic.FengShenBu")};
TArray<FGameXXKCardInstance> RequiredInstances;
for (const FName CardId : Required)
{
    FGameXXKCardInstance Found;
    if (!RemoveFirstCardById(Deck.Hand, CardId, Found)
        && !RemoveFirstCardById(Deck.DrawPile, CardId, Found))
    {
        return SetError(OutError, FString::Printf(TEXT("Tutorial opening card missing: %s"), *CardId.ToString()));
    }
    RequiredInstances.Add(MoveTemp(Found));
}
while (Deck.Hand.Num() > Deck.HandLimit - RequiredInstances.Num())
{
    Deck.DrawPile.Insert(Deck.Hand.Pop(EAllowShrinking::No), 0);
}
Deck.Hand.Insert(RequiredInstances[2], 0);
Deck.Hand.Insert(RequiredInstances[1], 0);
Deck.Hand.Insert(RequiredInstances[0], 0);
```

Validate that the instance ledger and total zone count are unchanged and that all three instances belong to `Character.Hero`.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Tutorial01.RouteRules"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Tutorial01.Session"
```

Expected: both tests pass, cold UBT succeeds, ordinary formation snapshot/gold/route data remains byte-for-byte equal.

Commit named files only:

```text
feat: add transient tutorial 0-1 route state
```

---

### Task 2: Let the existing route widget render a transient fixed projection

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`

- [ ] **Step 1: Add red adapter tests**

Construct a widget with ordinary MVP runtime left untouched, then install the tutorial projection and assert:

```cpp
const FGameXXKRuntimeState RuntimeBefore = Subsystem->GetRuntimeStateCopy();
int32 ExecutedNodeId = INDEX_NONE;
Widget->SetTransientRouteProjection(
    FGameXXKTutorial01RouteRules::BuildNodes(Route),
    FGameXXKTutorial01RouteRules::BuildEdges(),
    FGameXXKTutorial01RouteRules::BuildLabels(),
    FGameXXKTutorial01RouteRules::BuildCompletionNotice(Route), Route.VisitedNodeIds,
    Route.ReachableNodeIds,
    FGameXXKTransientRouteNodeExecuted::CreateLambda(
        [&ExecutedNodeId](int32 NodeId){ ExecutedNodeId = NodeId; return true; }));
Widget->RefreshFromState();
TestEqual(TEXT("exactly three tutorial visuals"), Widget->GetCreatedNodeVisualWidgetCount(), 3);
TestFalse(TEXT("start is disabled"), Widget->ExecuteRouteNodeById(100));
TestTrue(TEXT("battle is clickable"), Widget->ExecuteRouteNodeById(101));
TestEqual(TEXT("delegate receives stable id"), ExecutedNodeId, 101);
TestFalse(TEXT("return is locked before victory"), Widget->ExecuteRouteNodeById(102));
TestEqual(TEXT("ordinary runtime route remains untouched"),
    RuntimeBefore.RouteMapNodes, Subsystem->GetRuntimeState().RouteMapNodes);
```

After installing the victory projection, assert 101 is visited, 102 is enabled, exactly two edges render, and `GetTransientCompletionNoticeForTest()` equals `0-1 完成`.

- [ ] **Step 2: Run red**

Run cold UBT and `GameXXK.MVP.RouteMap.OneGameAdapter`; expect missing transient projection methods.

- [ ] **Step 3: Implement a copied projection plus delegate**

Add:

```cpp
DECLARE_DELEGATE_RetVal_OneParam(bool, FGameXXKTransientRouteNodeExecuted, int32);

void SetTransientRouteProjection(
    const TArray<FGameXXKRouteMapNode>& Nodes,
    const TArray<FGameXXKRouteMapEdge>& Edges,
    const TMap<int32, FText>& Labels,
    const FText& CompletionNotice,
    const TSet<int32>& VisitedNodeIds,
    const TSet<int32>& ReachableNodeIds,
    FGameXXKTransientRouteNodeExecuted OnExecuted);
void ClearTransientRouteProjection();
bool IsUsingTransientRouteProjectionForTest() const;
```

Store copied arrays/sets and the delegate. In transient mode:

- `BuildAdapterNodes()` maps only the copied nodes, applies copied label overrides, and sets `bVisited`/`bEnabled` from copied sets;
- `TryGetRenderedRouteEdge()` reads copied edges;
- `CalculateRouteContentSize()` derives layers from copied nodes;
- `ExecuteRouteNodeById()` rejects disabled/visited nodes and invokes only the transient delegate;
- ordinary route summary, capacity, abandon, and close-challenge controls collapse;
- a paper/ink transient notice appears only when `CompletionNotice` is nonempty and reads exactly `0-1 完成` after victory;
- `ClearTransientRouteProjection()` restores the unchanged ordinary path.

Do not write `RuntimeState.RouteMapNodes`, `VisitedRouteNodeIds`, `ReachableRouteNodeIds`, `RouteSeed`, or challenge state.

- [ ] **Step 4: Run ordinary and transient regression green**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.MVP.RouteMap.OneGameAdapter"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Guide.LiveTargets.WidgetRegistration"
```

Expected: existing generated routes still render/click; transient route has exactly three nodes and two edges.

- [ ] **Step 5: Commit**

```text
feat: project fixed tutorial route through shared map widget
```

---

### Task 3: Boot the route first, enter battle through the overlay, and intercept terminal results

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKTutorial01ResultWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTutorial01ResultWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorial01BattleFlowTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKPrologueAftermathController.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorial01PlayerFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`

- [ ] **Step 1: Replace direct-boot expectations with red route-first tests**

Change the boot enum/test to `TutorialRouteOnly`. The test must prove:

```cpp
TestEqual(TEXT("tutorial map boots route only"),
    ResolvePlayerFlowBootProfileForMapForTest(TutorialMap),
    EGameXXKPlayerFlowBootProfile::TutorialRouteOnly);
TestTrue(TEXT("route boot succeeds"),
    Controller->PrepareTutorial01RouteForTest(Runtime, Session, TEXT("?GameXXKTutorial=0-1")));
TestEqual(TEXT("route appears before battle"), Runtime->GetRuntimeState().Screen,
    EGameXXKScreen::DungeonMap);
TestFalse(TEXT("route boot has no battle"), Runtime->GetRuntimeState().CardRun.bHasActiveCardBattle);
```

Add terminal-interceptor tests that set a Victory runtime with gold/XP sentinels, call `ResolveCardBattleTerminalStateForTest`, and assert no ordinary reward/pending reward is created, BattleOverlay exits, the route marks node 101 complete, and node 102 unlocks.

- [ ] **Step 2: Run red**

Expected: failures from the old `TutorialBattleOnly` and direct encounter start.

- [ ] **Step 3: Replace the PlayerController direct host**

Rename/remove the old APIs and add:

```cpp
bool BeginTutorial01Route();
bool PrepareTutorial01RouteForTest(UGameXXKMVPSubsystem*, UGameXXKTutorial01SessionSubsystem*, const FString& Options);
bool HandleTutorial01RouteNode(int32 NodeId);
bool StartTutorial01Battle();
bool HandleTutorial01BattleTerminal(EGameXXKCardBattlePhase Phase);
void ShowTutorial01Failure(const FText& Reason);
bool HandleTutorial01BattleExitRequested();
void RetryTutorial01Battle();
void ReturnTutorial01ToTown(EGameXXKTutorial01ReturnReason Reason);
void RefreshTutorial01RouteProjection();
```

`BeginPlay` for `TutorialRouteOnly` must create/configure RouteMap, leave BattleBoard collapsed, and never call `StartNarrativeEncounter` until node 101 is clicked.

`StartTutorial01Battle` performs this exact transaction:

```cpp
FGameXXKRuntimeState BattleSeed;
if (!Session->BuildBattleSeedRuntime(BattleSeed, &Error))
{
    ShowTutorial01Failure(FText::FromString(Error));
    return false;
}
Runtime->GetMutableRuntimeState() = MoveTemp(BattleSeed);
if (!Runtime->StartNarrativeEncounter(TEXT("Encounter.Main.XuXiake.0-1"), &Error))
{
    ShowTutorial01Failure(FText::FromString(Error));
    return false;
}
if (!Session->ArrangeDeterministicOpeningHand(Runtime->GetMutableRuntimeState(), &Error))
{
    ShowTutorial01Failure(FText::FromString(Error));
    return false;
}
BattleBoard->SetBattleTerminalInterceptor(
    FGameXXKBattleTerminalInterceptor::CreateUObject(
        this, &AGameXXKMVPPlayerController::HandleTutorial01BattleTerminal));
BattleBoard->SetBattleExitInterceptor(
    FGameXXKBattleExitInterceptor::CreateUObject(
        this, &AGameXXKMVPPlayerController::HandleTutorial01BattleExitRequested));
EnterBattleOverlay();
```

The overlay coordinator must issue a nonzero session token; the direct `SetVisibility + RefreshFromState` path is deleted.

- [ ] **Step 4: Add a terminal interceptor before ordinary rewards**

Add to BattleBoard:

```cpp
DECLARE_DELEGATE_RetVal_OneParam(bool, FGameXXKBattleTerminalInterceptor,
    EGameXXKCardBattlePhase);
DECLARE_DELEGATE_RetVal(bool, FGameXXKBattleExitInterceptor);
void SetBattleTerminalInterceptor(FGameXXKBattleTerminalInterceptor InDelegate);
void ClearBattleTerminalInterceptor();
void SetBattleExitInterceptor(FGameXXKBattleExitInterceptor InDelegate);
void ClearBattleExitInterceptor();
```

At the top of the Victory/Defeat branch in `ResolveCardBattleTerminalState`, invoke it once. If it returns true, do not call `ResolveBattleVictory`, `FailDungeonToTown`, reward generation, XP, item grants, or route settlement.

`HandleBattleCloseClicked` invokes the optional exit interceptor before opening the ordinary retreat panel. The tutorial interceptor opens the same safe `重新挑战 / 返回城镇` paper surface without mutating ordinary route/retreat state. Escape may still cancel targeting or dismiss an already open confirmation; it must never be swallowed by the spotlight.

Victory: session marks route won, rebuilds the snapshot-derived DungeonMap runtime, Board clears tutorial delegates, overlay exits, route projection refreshes. Defeat: keep the terminal board visible and open the tutorial result widget.

- [ ] **Step 5: Implement the paper failure surface**

`UGameXXKTutorial01ResultWidget` contains a MasterV2 paper panel, reason text, and exactly two buttons: `重新挑战` and `返回城镇`. Expose delegates and headless test seams:

```cpp
DECLARE_DELEGATE(FGameXXKTutorial01RetryRequested);
DECLARE_DELEGATE(FGameXXKTutorial01ReturnTownRequested);
void PresentFailure(const FText& Reason);
void Dismiss();
bool IsVisibleForTest() const;
void ChooseRetryForTest();
void ChooseReturnTownForTest();
```

Retry exits the old overlay, resets session guide/route battle flags, rebuilds a new battle seed and hand, then enters with a fresh nonzero overlay token. Return restores the exact pre-tutorial runtime and opens Qingshan with `?GameXXKTutorialReturn=Defeat`.

- [ ] **Step 6: Restore YueBai/follower state on town return**

Teach `AGameXXKPrologueAftermathController` to consume a pending tutorial return only when the explicit return option exists. Apply `StatueReturnTransform` to the hero, reactivate YueBai's 220–300 UU follower, and:

- Victory: no statue prompt, no automatic dialogue/battle;
- Defeat: restore `StatuePrompt`, passive bubble, and F interaction;
- missing/duplicate YueBai: release input and leave the ordinary town playable.

Do not persist partial route/Guide state.

- [ ] **Step 7: Run green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Tutorial01.PlayerFlow"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Tutorial01.BattleFlow"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.MVP.Battle.OverlayCoordinator"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Prologue.Aftermath.Controller"
```

Expected: route-first boot, nonzero overlay token contract, no tutorial rewards, retry/return, and exact formation restoration pass.

- [ ] **Step 8: Commit**

```text
feat: run tutorial battle from fixed route overlay
```

### Manual checkpoint 1

Stop here and ask the user to verify manually:

```text
雕像 F → 选择任一按钮 → 只看到三节点路线
→ 起点已完成、仅战斗可点
→ 点击战斗后四个角色完整显示
→ 胜利无奖励并返回同一张路线
→ 战斗节点完成、返回城镇节点解锁
```

Do not continue to Task 4 until the user accepts this checkpoint.

---

### Task 4: Extend the generic Guide model for local multi-target steps

**Files:**
- Modify: `Source/GameXXK/Public/Guide/GameXXKGuideAsset.h`
- Modify: `Source/GameXXK/Private/Guide/GameXXKGuideAsset.cpp`
- Modify: `Source/GameXXK/Public/Guide/GameXXKGuideRules.h`
- Modify: `Source/GameXXK/Private/Guide/GameXXKGuideRules.cpp`
- Modify: `Source/GameXXK/Public/Guide/GameXXKGuideCoordinator.h`
- Modify: `Source/GameXXK/Private/Guide/GameXXKGuideCoordinator.cpp`
- Modify: `Source/GameXXK/Public/Guide/GameXXKGuideTargetRegistry.h`
- Modify: `Source/GameXXK/Private/Guide/GameXXKGuideTargetRegistry.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuideRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuideWidgetTest.cpp`

- [ ] **Step 1: Write red multi-target/local-coordinate/missing-target tests**

Add tests proving one step emits two focus target IDs plus one bubble anchor, all rectangles resolve in overlay-local coordinates, and an `AbortGuide` missing target returns false without advancing to the next step. Existing single-target guides must still emit one-element arrays and keep `SkipStep` compatibility.

- [ ] **Step 2: Run red**

Expected: missing enum/arrays/local resolver APIs.

- [ ] **Step 3: Extend the data contract compatibly**

Add:

```cpp
UENUM(BlueprintType)
enum class EGameXXKGuideMissingTargetPolicy : uint8
{
    SkipStep,
    AbortGuide,
};
```

Keep legacy `TargetId`, then add to `FGameXXKGuideStepDefinition`:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guide")
TArray<FName> AdditionalTargetIds;
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guide")
FName BubbleAnchorTargetId;
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guide")
EGameXXKGuideMissingTargetPolicy MissingTargetPolicy =
    EGameXXKGuideMissingTargetPolicy::SkipStep;
```

Replace `FGameXXKGuideOutput::TargetId` with:

```cpp
UPROPERTY(BlueprintReadOnly, Category="Guide") TArray<FName> TargetIds;
UPROPERTY(BlueprintReadOnly, Category="Guide") FName BubbleAnchorTargetId;
```

`BuildOutput` adds primary first, unique additional IDs afterward. Data validation rejects duplicate targets, unknown additional targets, unknown bubble anchors, or forced steps with no focus target.

- [ ] **Step 4: Make target resolution host-local**

Change the registry contract to resolve relative to the overlay host:

```cpp
using FGameXXKGuideTargetRectResolver =
    TFunction<bool(const UWidget& HostWidget, FSlateRect& OutLocalRect)>;
bool ResolveTargetRect(FName TargetId, const UWidget& HostWidget, FSlateRect& OutLocalRect);
bool HasActionGate() const;
```

`RegisterWidgetTarget` computes the relative layout transform without `LocalToAbsolute`/`AbsoluteToLocal`:

```cpp
// Include Layout/TransformCalculus2D.h. Concatenate applies target→desktop
// followed by desktop→host, so the floating-window origin cancels out.
const FSlateRenderTransform TargetToHost = Concatenate(
    TargetGeometry.GetAccumulatedRenderTransform(),
    Inverse(HostGeometry.GetAccumulatedRenderTransform()));
const FVector2D Min = TransformPoint(TargetToHost, FVector2D::ZeroVector);
const FVector2D Max = TransformPoint(TargetToHost, TargetGeometry.GetLocalSize());
OutLocalRect = FSlateRect(Min.X, Min.Y, Max.X, Max.Y);
```

Coordinator resolves every focus target and optional bubble anchor against its own full-screen overlay. `AbortGuide` missing targets return an error; the caller owns cancellation/failure UI. Do not silently complete the tutorial step.

Add the following optional coordinator fault seam:

```cpp
DECLARE_DELEGATE_OneParam(FGameXXKGuideCoordinatorFault, const FString&);
void SetFaultDelegate(FGameXXKGuideCoordinatorFault InDelegate);
```

Missing `AbortGuide` targets and unexpected overlay destruction release the coordinator token first, then notify the tutorial host; old Workbench callers that do not bind this delegate retain their current behavior.

- [ ] **Step 5: Run green and commit**

Run `GameXXK.Guide.Rules`, `GameXXK.Guide.Widget`, `GameXXK.Guide.LiveTargets`, and cold UBT. Expected: old guides stay green; new local/multi-target tests pass.

```text
feat: support local multi-target guide output
```

---

### Task 5: Draw multi-hole dimming and a BattleBoard-local YueBai bubble

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleGuideBubbleWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleGuideBubbleWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKGuideOverlayWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKGuideOverlayWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuideWidgetTest.cpp`

- [ ] **Step 1: Write red paint/layout tests**

For a 1920×1080 host and two cutouts, assert `BuildDimRegionsForTest` returns non-overlapping dim rectangles whose total area equals host area minus the union of cutouts. Assert no dim region intersects either cutout. Test a YueBai anchor near the right edge chooses the left side and remains inside the safe stage.

- [ ] **Step 2: Run red**

Expected: missing array presentation, dim-region builder, and battle bubble.

- [ ] **Step 3: Implement complement-region dimming**

Change overlay presentation to:

```cpp
void PresentGuide(
    const FGameXXKGuideOutput& Output,
    const TArray<FSlateRect>& LocalTargetRects,
    const TOptional<FSlateRect>& LocalBubbleAnchorRect);
```

Remove the full-screen `DimMask` child. In `NativePaint`, clamp and pad all cutouts, collect `{0, H, Top, Bottom}` Y boundaries, iterate each horizontal band, merge intersecting X intervals, and paint only the complementary spans with black alpha `0.56`. Paint one ink outline around each cutout. Call `Super::NativePaint` after dim regions so bubble/outline children remain above the dim.

The overlay remains `HitTestInvisible`; no child may accept pointer input. Do not reparent, clone, snapshot, resize, or disable target controls.

- [ ] **Step 4: Implement the YueBai battle bubble**

`UGameXXKBattleGuideBubbleWidget` accepts text, an optional “空格继续” hint, anchor rect, and host size. It uses approved paper styling, chooses right/left/above in that order, clamps to a 16-pixel safe margin, and exposes its final local rect for tests. It never performs world projection.

When `BubbleAnchorTargetId` is absent, preserve generic Guide behavior by placing the panel below the first focus target.

- [ ] **Step 5: Run green and static coordinate guard**

Run Guide Widget tests. Also assert these files contain neither `LocalToAbsolute` nor `AbsoluteToLocal`:

```text
Source/GameXXK/Private/UI/GameXXKGuideOverlayWidget.cpp
Source/GameXXK/Private/Guide/GameXXKGuideTargetRegistry.cpp
```

- [ ] **Step 6: Commit**

```text
feat: add multi-hole battle guide spotlight
```

---

### Task 6: Author and import the isolated nine-step Guide asset

**Files:**
- Create: `SourceAssets/Narrative/Guides/Guide.Battle.Tutorial01.NewPlayer.guide.json`
- Create: `Content/GameXXK/Narrative/Guides/DA_Guide_Battle_Tutorial01_NewPlayer.uasset`
- Modify: `scripts/validate_guide_json.py`
- Modify: `scripts/test_guide_json_validation.py`
- Modify: `Content/Python/gamexxk_import_guide_json.py`

- [ ] **Step 1: Add red JSON-schema/source tests**

Extend the validator with optional `additionalTargets`, `bubbleAnchor`, and `missingTargetPolicy` (`skip`/`abort`). Update the exact source-file set to include the new guide. Assert its visible chain is exactly nine nodes and `Guide.Battle.Basic.guide.json` is byte-for-byte unchanged by this task.

- [ ] **Step 2: Run red**

```powershell
python -m unittest scripts.test_guide_json_validation -v
```

Expected: missing new source/schema vocabulary.

- [ ] **Step 3: Add the exact JSON**

Create schema-version 1 JSON with this exact chain:

```json
{
  "schemaVersion": 1,
  "guideId": "Guide.Battle.Tutorial01.NewPlayer",
  "guideVersion": 1,
  "entryStep": "Guide.Battle.Tutorial01.Qi",
  "steps": {
    "Guide.Battle.Tutorial01.Qi": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Hud.PartyQi",
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "气力值：每回合出牌时消耗的点数。",
      "allowedActions": ["Action.Guide.Continue"],
      "completionEvent": "Event.Tutorial01.Continue", "next": "Guide.Battle.Tutorial01.HeroVitals"
    },
    "Guide.Battle.Tutorial01.HeroVitals": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Unit.Hero.Health",
      "additionalTargets": ["Battle.Unit.Hero.Mana"],
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "气血：主角生命值，归零时死亡。内力值：出牌时消耗的角色能量条，每回合恢复2点内力。",
      "allowedActions": ["Action.Guide.Continue"],
      "completionEvent": "Event.Tutorial01.Continue", "next": "Guide.Battle.Tutorial01.EnemyIntent"
    },
    "Guide.Battle.Tutorial01.EnemyIntent": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Enemy.Intent",
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "怪物意图卡：玩家悬停可以看到下回合怪物的动向和攻击数值，提前布局。",
      "allowedActions": ["Action.Guide.Continue"],
      "completionEvent": "Event.Tutorial01.Continue", "next": "Guide.Battle.Tutorial01.HengJian"
    },
    "Guide.Battle.Tutorial01.HengJian": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Hand.HengJianShouShi",
      "additionalTargets": ["Battle.Unit.Hero.Target"],
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "先点击横剑守势，再点击主角，增加属性与护甲。",
      "allowedActions": ["Action.Battle.SelectCard.HengJianShouShi", "Action.Battle.SelectTarget.Hero", "Action.Battle.CommitCard"],
      "completionEvent": "Event.Tutorial01.HengJianResolved", "next": "Guide.Battle.Tutorial01.SuiYan"
    },
    "Guide.Battle.Tutorial01.SuiYan": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Hand.SuiYanJi",
      "additionalTargets": ["Battle.Unit.Enemy.Target"],
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "先点击碎岩击，再点击敌方角色。",
      "allowedActions": ["Action.Battle.SelectCard.SuiYanJi", "Action.Battle.SelectTarget.Enemy", "Action.Battle.CommitCard"],
      "completionEvent": "Event.Tutorial01.SuiYanResolved", "next": "Guide.Battle.Tutorial01.FengShen"
    },
    "Guide.Battle.Tutorial01.FengShen": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Hand.FengShenBu",
      "additionalTargets": ["Battle.Unit.Hero.Target"],
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "先点击风身步，再点击主角。",
      "allowedActions": ["Action.Battle.SelectCard.FengShenBu", "Action.Battle.SelectTarget.Hero", "Action.Battle.CommitCard"],
      "completionEvent": "Event.Tutorial01.FengShenForcedDiscardOpened", "next": "Guide.Battle.Tutorial01.ForcedDiscard"
    },
    "Guide.Battle.Tutorial01.ForcedDiscard": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.Pending.ForcedDiscard",
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "在弃牌框中选择一张手牌丢弃。",
      "allowedActions": ["Action.Battle.SubmitForcedDiscard"],
      "completionEvent": "Event.Tutorial01.ForcedDiscardResolved", "next": "Guide.Battle.Tutorial01.EndTurn"
    },
    "Guide.Battle.Tutorial01.EndTurn": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.EndTurn",
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "结束回合：本回合气力消耗和出牌结束后，点击这个按钮结束回合。",
      "allowedActions": ["Action.Battle.EndTurn"],
      "completionEvent": "Event.Tutorial01.PlayerTurnReady", "next": "Guide.Battle.Tutorial01.AutoBattle"
    },
    "Guide.Battle.Tutorial01.AutoBattle": {
      "triggerEvent": "Event.Battle.Opened", "target": "Battle.AutoBattle",
      "bubbleAnchor": "Battle.Unit.YueBai.Visual", "missingTargetPolicy": "abort",
      "inputPolicy": "forced", "text": "点击自动战斗，接下来会自动出牌完成战斗。",
      "allowedActions": ["Action.Battle.EnableAuto"],
      "completionEvent": "Event.Tutorial01.AutoBattleEnabled"
    }
  }
}
```

- [ ] **Step 4: Update importer and semantic catalogs**

Importer maps `additionalTargets`, `bubbleAnchor`, and policy into the new USTRUCT fields. Add every new target/action/event/guide ID to both Python default catalogs and C++ registry known sets. Keep Guide source validation on the Guide catalog/default semantic snapshot; do not incorrectly read Guide vocabulary from the Dialogue-only `runtime-catalog.json`.

- [ ] **Step 5: Validate and import through UE MCP**

Run Python tests, then import only the new JSON with `Content/Python/gamexxk_import_guide_json.py`. Expected report:

```text
ok=true
importedCount=1
guideId=Guide.Battle.Tutorial01.NewPlayer
stepCount=9
assetPath=/Game/GameXXK/Narrative/Guides/DA_Guide_Battle_Tutorial01_NewPlayer.DA_Guide_Battle_Tutorial01_NewPlayer
```

- [ ] **Step 6: Commit**

```text
feat: author tutorial 0-1 battle guide
```

---

### Task 7: Register real BattleBoard targets and drive the nine-step guide

**Files:**
- Create: `Source/GameXXK/Public/Guide/GameXXKTutorial01GuideHost.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKTutorial01GuideHost.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorial01GuideHostTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuideLiveTargetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Write the red host/action/target chain tests**

Test that NewPlayer starts at Qi, Space advances only the first three information steps, wrong actions stay blocked, and the exact event chain reaches Auto. Assert HengJian raises the selected hero's armor/status, SuiYan reduces enemy HP, FengShen opens `ForcedDiscard`, the chosen discard leaves the hand, and the next player-round boundary restores exactly `min(2, MaxMana-Mana)` to each living party unit. Test that ExperiencedPlayer creates no host/gate/overlay. Destroy/missing target/explicit failure must call the failure delegate once and leave `Registry.HasActionGate()==false`.

- [ ] **Step 2: Expose the two real hero resource rows**

Add read-only production getters on `UGameXXKBattleUnitResourceWidget`:

```cpp
UWidget* GetHealthRowForGuide() const { return HealthRow; }
UWidget* GetManaRowForGuide() const { return ManaRow; }
```

Do not create duplicate bars.

- [ ] **Step 3: Register all tutorial semantic targets in BattleBoard-local space**

Extend `RefreshGuideTargets()` to register/unregister dynamically:

```text
Battle.Hud.PartyQi                     PartyQiWidget
Battle.Unit.Hero.Health                hero ResourceWidget health row
Battle.Unit.Hero.Mana                  hero ResourceWidget mana row
Battle.Enemy.Intent                    first visible EnemyIntentCardButton
Battle.Hand.HengJianShouShi            current hand button found by CardId
Battle.Hand.SuiYanJi                    current hand button found by CardId
Battle.Hand.FengShenBu                  current hand button found by CardId
Battle.Unit.Hero.Target                hero UnitTargetProxy
Battle.Unit.Enemy.Target               first living enemy UnitTargetProxy
Battle.Unit.YueBai.Visual               YueBai UnitVisual
Battle.Pending.ForcedDiscard            PendingChoicePanel only while ForcedDiscard
Battle.EndTurn                          EndTurnButton
Battle.AutoBattle                       AutoBattleButton
```

Registration uses the Guide overlay as local host through Task 4's resolver; no screenshot coordinates.

- [ ] **Step 4: Gate payload-specific actions and emit authoritative events**

Before generic card action checks, map CardId to the specific tutorial action. Allow a card only when either its specific action or legacy `Action.Battle.SelectTargetedCard` is permitted. Do the equivalent for hero/enemy targets while still requiring `Action.Battle.CommitCard`.

Add gate checks/events:

```text
Space + active Action.Guide.Continue       → Event.Tutorial01.Continue
HengJian resolves on Character.Hero        → Event.Tutorial01.HengJianResolved
SuiYan resolves on a living enemy           → Event.Tutorial01.SuiYanResolved
FengShen resolves and opens ForcedDiscard   → Event.Tutorial01.FengShenForcedDiscardOpened
forced discard transaction succeeds         → Event.Tutorial01.ForcedDiscardResolved
enemy phase fully finishes/player phase     → Event.Tutorial01.PlayerTurnReady
SetAutoBattleEnabled(true) succeeds          → Event.Tutorial01.AutoBattleEnabled
```

Only emit after the authoritative mutation succeeds. Hovering intent remains ungated. Pause, battle-close/return, and failure controls do not consult the Guide action gate.

- [ ] **Step 5: Implement tutorial-local Guide ownership**

Public host contract:

```cpp
DECLARE_DELEGATE_OneParam(FGameXXKTutorial01GuideFailed, const FString&);

void Bind(FGameXXKGuideProgress& Progress,
    FGameXXKGuideTargetRegistry& Registry,
    UGameXXKGuideOverlayWidget& Overlay,
    UGameXXKGuideAsset& Asset,
    FGameXXKTutorial01GuideFailed OnFailed);
bool Start();
void HandleGuideEvent(FName EventId);
void Tick(float DeltaSeconds, bool bPaused, bool bBattleBusy,
    EGameXXKCardBattlePhase Phase);
void Cancel(const FString& Diagnostic = FString());
bool IsSuspendedForEnemyTurnForTest() const;
```

The host owns the registry event handle and action-gate owner. On successful EndTurn it dismisses the overlay and clears the gate without completing the step; when `Event.Tutorial01.PlayerTurnReady` arrives it restores the gate, completes EndTurn, and shows AutoBattle. On Auto enabled it cancels/destroys Guide presentation but leaves the existing auto-battle system on.

The 15-second watchdog arms when `Event.Battle.EndTurnResolved` arrives while the active step still awaits `Event.Tutorial01.PlayerTurnReady`; all other approved actions emit their completion event synchronously and fail immediately if event handling returns a nonempty error. The watchdog pauses while paused, while BattleBoard reports presentation busy, and during the normal Enemy phase. Expiry calls `Cancel`, releases the gate, and invokes failure.

- [ ] **Step 6: Bind host only for the new-player tutorial session**

PlayerController loads the exact asset, passes `Session->GetMutableGuideProgress()`, and asks BattleBoard to attach Overlay + Host inside its safe-stage root. ExperiencedPlayer skips all of this. Board teardown, overlay destruction, retry, route return, map travel, and terminal state call `CancelTutorial01Guide` idempotently.

Guide failure calls the same paper `重新挑战 / 返回城镇` path as defeat; it never skips to the next Guide step.

- [ ] **Step 7: Run green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Tutorial01.GuideHost"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Guide.LiveTargets"
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Battle.Board"
```

Expected: exact chain, target registration, payload-specific blocking, Space handling, suspension, auto teardown, and failure release pass.

- [ ] **Step 8: Commit**

```text
feat: drive tutorial 0-1 guided battle
```

### Manual checkpoint 2

Stop and ask the user to play the new-player branch through AutoBattle. Do not proceed to final polish until they confirm:

- three information bubbles and Space behavior;
- exact pair highlights/click order for all three cards;
- forced-discard frame;
- EndTurn wait with no overlay during enemy action;
- AutoBattle highlight on the next player turn;
- pause/exit always works;
- wrong clicks do not advance.

---

### Task 8: Add isolation guards, map validation, and full recovery regression

**Files:**
- Create: `scripts/test_tutorial01_guide_policy.py`
- Modify: `Content/Python/gamexxk_validate_tutorial01_map.py`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorial01BattleFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorial01PlayerFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`

- [ ] **Step 1: Add static policy tests**

Scan only tutorial/Guide integration files and reject:

```text
SendInput
SetWindowsHookEx
mouse_event
HideWindow
ShowWindow
Minimize
LocalToAbsolute (in GuideOverlay/GuideTargetRegistry)
AbsoluteToLocal (in GuideOverlay/GuideTargetRegistry)
BeginTutorial01Battle (retired direct boot name)
TutorialBattleOnly (retired direct boot profile)
ResolveBattleVictory (inside tutorial terminal handler)
FailDungeonToTown (inside tutorial terminal handler)
```

Also assert `Content/Python/init_unreal.py` does not import/run any tutorial test, PIE probe, mouse automation, or window automation.

- [ ] **Step 2: Extend map validation**

Keep `/Game/GameXXK/Maps/Tutorial/L_Tutorial_0_1` actor-free with `AGameXXKTutorial01GameMode`. Add class-default checks through UE MCP proving the controller boot profile is RouteOnly and the map has no 3D pawn/default actor dependency.

- [ ] **Step 3: Add full recovery matrix**

Automation covers:

```text
NewPlayer: route → guide → victory → route → town
Experienced: route → no guide → victory → route → town
Defeat → Retry: fresh battle, guide restarts at Qi
Defeat → ReturnTown: old formation restored, statue prompt restored
Missing Guide asset/target/YueBai visual: gate released, Retry/Return visible
Exit/map travel/overlay destruction: no action gate or input token remains
Gold/XP/items/rewards/challenge/ordinary route data unchanged
Ordinary BattleBoard terminal rewards still behave exactly as before
```

- [ ] **Step 4: Run the complete cold gate**

Save dirty packages and close UE through MCP, then run:

```powershell
python -m unittest scripts.test_guide_json_validation scripts.test_tutorial01_guide_policy -v
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK"
```

Expected: zero failures; UBT is cold and uses `-NoHotReload`.

- [ ] **Step 5: Commit**

```text
test: guard tutorial 0-1 guided route flow
```

---

### Task 9: Real PIE acceptance and handoff

**Files:**
- Modify only if evidence requires a fix: files already named in Tasks 1–8.
- Evidence: `Saved/Codex/` screenshots and `Saved/HarnessReports/` reports (do not commit transient captures unless the user asks).

- [ ] **Step 1: Save, cold build, and start the correct map**

Use UE MCP, not UnrealBridge. Ensure default project startup remains `/Game/GameXXK/Maps/L_DesktopTrainingHUD`; enter Qingshan only through the explicit story flow. Do not auto-click.

- [ ] **Step 2: Developer read-only evidence pass**

Capture route-before-battle, new-player Qi step, one card+target pair, forced discard, next-turn Auto, victory-return route, and return-town frames. Verify the spotlight rectangles remain aligned at the actual floating PIE size and after one manual resize.

- [ ] **Step 3: User manual acceptance**

Ask the user to personally execute the full flow. Record untested branches as unverified; do not infer manual acceptance from semantic Automation.

- [ ] **Step 4: Final regression and status**

Run `git diff --check`, confirm the three protected deletions are still the only staged paths, list task commits, and report:

- accepted manual steps;
- automated pass counts;
- cold UBT result;
- any branch not manually tested;
- confirmation that no synthetic input/window automation was introduced.

Do not push or create a PR unless the user explicitly asks at execution time.
