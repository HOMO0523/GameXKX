# GameXXK Route-Owned Auto Battle Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the accepted portal-to-route-map-to-existing-battle player flow and add exactly one player-facing feature: optional legal automatic card play inside the existing full-screen battle board.

**Architecture:** The desktop workbench challenge action becomes a thin adapter over `UGameXXKMVPSubsystem::OpenDungeonFromTownExit()` plus the same `GameXXKLevelFlow::OpenMapForRuntimeState()` travel used by the town exit; it never creates or owns a battle canvas. A transient session flag lives on `UGameXXKMVPSubsystem`, while `UGameXXKBattleBoardWidget` owns the timer and performs one legal action at a time through its existing card/target/choice/end-turn presentation APIs. Route nodes, events, shops, rewards, retries, and quest/follower prerequisites remain player-owned.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, CardBattle adapter/rules, UE Automation, project UE MCP, PowerShell, cold UBT, Luna max visual review.

---

## File map

- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: locks successful route delegation, failed-prerequisite behavior, and absence of the rejected embedded challenge controls.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: removes the rejected `ChallengeViewport` mode, embedded-board fields, challenge geometry seams, and workbench-owned auto controls.
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: delegates Challenge to the existing dungeon entrance, closes the workbench, opens the runtime-owned map, and deletes the embedded battle/3+3/read-only-shell implementation.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h` and `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`: delete challenge-only rectangles that no longer have a consumer.
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`: removes the exception that kept the workbench visible outside Town and replaces the obsolete challenge performance boot with an explicit existing-battle fixture.
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` and `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: own the transient auto-play session flag; the flag is not part of `FGameXXKRuntimeState` and does not change SaveVersion.
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` and `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`: add the existing-style auto-play button, safe cadence gate, stable legal action selection, and Board-owned forced-discard batch submission.
- `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`: proves the toggle, manual-target card play, pending choices, end-turn fallback, presentation locking, terminal stop, and no action outside `Battle`.
- `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`: proves the explicit performance profile renders the existing BattleBoard and does not recreate the workbench challenge viewport.
- `scripts/test_desktop_training_route_ownership.py`: deterministic source contract forbidding the retired embedded battle and any player-facing route/party auto-selection.
- `scripts/measure_desktop_training_hud_memory.ps1`, `scripts/test_measure_desktop_training_hud_memory.py`, and `scripts/README.md`: keep the `challenge` measurement name for report compatibility but redefine it as the existing full-screen card battle.
- `docs/production/current-goal-acceptance.md` and `docs/production/2026-08-19-goal-progress-evidence.md`: record corrected semantics, verification evidence, and protected-asset hashes.

### Task 1: Make Challenge delegate to the existing route entrance

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [x] **Step 1: Replace the embedded-viewport assertions with failing route-ownership tests**

Add one success and one prerequisite-failure contract. The success fixture explicitly accepts the quest; the failure fixture snapshots quest and party state so the workbench cannot silently change either:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChallengeDelegatesToRouteTest,
	"GameXXK.DesktopTraining.Workbench.ChallengeDelegatesToExistingRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChallengeDelegatesToRouteTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("route-delegation fixture starts in town"), Subsystem->StartGame());
	TestTrue(TEXT("route-delegation fixture accepts the existing quest prerequisite"), Subsystem->AcceptQuest());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("route-delegation fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("Challenge delegates to the existing route entrance"), Widget->ClickChallengeForTest());
	TestEqual(TEXT("Challenge stops on the player-owned route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("the workbench closes before route ownership begins"), Widget->IsWorkbenchVisibleForTest());
	TestNull(TEXT("the workbench never constructs an embedded BattleBoard"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeBattleBoard")) : nullptr);
	TestNull(TEXT("the rejected auto button is absent"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeAutoButton")) : nullptr);
	TestNull(TEXT("the rejected debug-advance button is absent"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ChallengeAdvanceButton")) : nullptr);
	return true;
}
```

```cpp
const EGameXXKQuestState QuestBefore = Subsystem->GetRuntimeState().QuestState;
const FGameXXKPartySelectionState PartyBefore = Subsystem->GetRuntimeState().CardRun.PartySelection;
TestFalse(TEXT("Challenge rejects missing route prerequisites"), Widget->ClickChallengeForTest());
TestEqual(TEXT("failed Challenge remains in Town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
TestEqual(TEXT("failed Challenge does not accept the quest"), Subsystem->GetRuntimeState().QuestState, QuestBefore);
TestTrue(TEXT("failed Challenge does not alter follower selection"),
	FGameXXKPartySelectionState::StaticStruct()->CompareScriptStruct(
		&Subsystem->GetRuntimeState().CardRun.PartySelection, &PartyBefore, PPF_None));
TestTrue(TEXT("failed Challenge keeps the workbench visible"), Widget->IsWorkbenchVisibleForTest());
```

Remove the former assertions for `ChallengeViewport`, read-only side shells, 960x968 geometry, 3+3 slots, and the two challenge buttons from the existing reference-geometry and approved-control tests.

- [x] **Step 2: Cold-build the test and verify RED**

Run with every Unreal editor process closed:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.DesktopTraining.Workbench.ChallengeDelegatesToExistingRoute') -TimeoutSeconds 240
```

Expected: UBT succeeds; Automation fails because `ClickChallengeForTest()` still starts `StartTrainingChallenge`, keeps the workbench visible, and leaves `Screen == Battle`.

- [x] **Step 3: Implement the minimal existing-route handoff**

Include `MVP/GameXXKLevelFlow.h`. Change Action 6 and the test seam to use the authoritative route entrance:

```cpp
case 6:
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	if (!Subsystem->OpenDungeonFromTownExit())
	{
		SetNotice(FText::FromString(TEXT("请先完成当前路线入口条件")));
		break;
	}
	bSettingsPanelOpen = false;
	bExitConfirmationOpen = false;
	CloseWorkbench();
	GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
	NotifyPlayerFlowStateChanged();
	break;
```

```cpp
bool UGameXXKDesktopTrainingWorkbenchWidget::ClickChallengeForTest()
{
	ApplyAction(6);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::DungeonMap;
}
```

The action must not call `AcceptQuest`, `SelectDungeonNode`, `SelectRouteNodeById`, `StartTrainingChallenge`, or any follower mutation.

- [x] **Step 4: Cold-build and verify GREEN**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.DesktopTraining.Workbench.ChallengeDelegatesToExistingRoute',
  'GameXXK.DesktopTraining.Workbench'
) -TimeoutSeconds 360
```

Expected: all selected tests pass; success stops at `DungeonMap`, failure stays in `Town`, and no challenge widget is found.

- [x] **Step 5: Commit the route handoff**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp
git commit -m "fix: restore route-owned challenge entry"
```

### Task 2: Delete the rejected workbench battle presentation

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add a failing source-retirement contract**

Create the focused source contract with all paths and assertions defined:

```python
from pathlib import Path
import unittest

PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_HEADER = PROJECT_ROOT / "Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h"
WORKBENCH_CPP = PROJECT_ROOT / "Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp"


class DesktopTrainingRouteOwnershipTest(unittest.TestCase):
    def combined_source(self) -> str:
        return WORKBENCH_HEADER.read_text(encoding="utf-8") + WORKBENCH_CPP.read_text(encoding="utf-8")

    def test_rejected_embedded_challenge_surface_is_retired(self) -> None:
        text = self.combined_source()
        for rejected in (
            "ChallengeViewport", "BuildChallengeViewport", "BuildChallengeCombatStrip",
            "ChallengeBattleBoard", "ChallengeAutoButton", "ChallengeAdvanceButton",
            "AutoBattleAccumulator", "bChallengeSidePanelsReadOnly",
        ):
            self.assertNotIn(rejected, text)

    def test_challenge_action_delegates_without_choosing_route_or_party(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        action = source[source.index("case 6:"):source.index("case 7:")]
        self.assertIn("OpenDungeonFromTownExit", action)
        self.assertIn("OpenMapForRuntimeState", action)
        for forbidden in (
            "AcceptQuest", "StartTrainingChallenge", "SelectDungeonNode",
            "SelectRouteNodeById", "SelectTownQuestNpcForParty",
        ):
            self.assertNotIn(forbidden, action)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the source test and verify RED**

Run:

```powershell
python scripts/test_desktop_training_route_ownership.py
```

Expected: FAIL listing the still-present rejected names.

- [ ] **Step 3: Remove all zero-reference challenge presentation code**

Delete:

- `EGameXXKDesktopTrainingViewMode` and its `ViewMode` field;
- challenge geometry constants and public layout getters;
- `IsChallengeViewportActiveForTest`, `AreChallengeSidePanelsReadOnlyForTest`, `IsAutoBattleVisibleForTest`, challenge geometry/count seams, `ToggleAutoBattleForTest`, and `AdvanceChallengeForTest`;
- `BuildChallengeViewport`, `BuildChallengeCombatStrip`, `ChallengeBattleBoard`, its visual-session token, and workbench `AutoBattleAccumulator`;
- every navigation/carry/collapse branch conditional on `ChallengeViewport`;
- Action 8's workbench auto toggle and Action 9's debug encounter advance;
- the PlayerController `bKeepTrainingChallenge` exception.

The resulting controller rule is unconditional outside Town:

```cpp
else if (Workbench && ActiveScreen != EGameXXKScreen::Town)
{
	Workbench->CloseWorkbench();
}
```

`NativeTick` keeps only collapsed-resource and Travel work. `BuildWorkbenchShell` builds normal warehouse/backpack/tools/training views only.

- [ ] **Step 4: Verify no rejected symbol remains and run regression tests**

Run:

```powershell
python scripts/test_desktop_training_route_ownership.py
rg -n "ChallengeViewport|BuildChallengeViewport|BuildChallengeCombatStrip|ChallengeBattleBoard|ChallengeAutoButton|ChallengeAdvanceButton|bKeepTrainingChallenge" Source/GameXXK
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.DesktopTraining.Workbench','GameXXK.MVP.UI') -TimeoutSeconds 480
```

Expected: source test passes; `rg` prints nothing; UBT succeeds; selected Automation suites pass.

- [ ] **Step 5: Commit the retirement**

```powershell
git add scripts/test_desktop_training_route_ownership.py Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "refactor: retire embedded workbench battle"
```

### Task 3: Add a transient battle auto-play session switch and existing-style control

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write the failing toggle/session/UI test**

Add `GameXXK.Integration.CardBattle.BoardAutoPlayToggle`:

```cpp
UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
TestFalse(TEXT("a fresh application session defaults auto battle off"), Subsystem->IsBattleAutoPlayEnabled());
UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
Board->SetMVPSubsystem(Subsystem);
TestTrue(TEXT("auto-play board initializes"), Board->Initialize());
Board->NativeConstruct();
UButton* AutoButton = Board->WidgetTree
	? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleAutoPlayButton")))
	: nullptr;
TestNotNull(TEXT("the existing BattleBoard owns one auto-play button"), AutoButton);
TestTrue(TEXT("player can enable auto battle"), Board->SetAutoBattleEnabled(true));
TestTrue(TEXT("the subsystem retains the setting for later monster battles"), Subsystem->IsBattleAutoPlayEnabled());
UGameXXKMVPSubsystem* FreshSession = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
TestFalse(TEXT("a new application session resets auto battle"), FreshSession->IsBattleAutoPlayEnabled());
```

Assert `BattleAutoPlayLabel` reads `自动战斗：关` before the toggle and `自动战斗：开` after the Board refresh.

- [ ] **Step 2: Cold-build and verify RED**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.Integration.CardBattle.BoardAutoPlayToggle') -TimeoutSeconds 240
```

Expected: compile fails because the new subsystem and Board APIs do not exist.

- [ ] **Step 3: Implement the transient owner and Board control**

Add the subsystem API and a non-save field:

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Auto")
bool IsBattleAutoPlayEnabled() const { return bBattleAutoPlayEnabled; }

UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Auto")
bool SetBattleAutoPlayEnabled(bool bEnabled);

UPROPERTY(Transient)
bool bBattleAutoPlayEnabled = false;
```

```cpp
bool UGameXXKMVPSubsystem::SetBattleAutoPlayEnabled(const bool bEnabled)
{
	bBattleAutoPlayEnabled = bEnabled;
	return true;
}
```

Do not add the flag to `FGameXXKRuntimeState`, SaveGame, migration, or SaveVersion.

On the Board, add `SetAutoBattleEnabled(bool)`, `IsAutoBattleEnabled()`, and `HandleAutoBattleClicked()`. Build one `BattleAutoPlayButton` with `StyleBattleActionButton`, anchored immediately left/above the existing end-turn rail without changing the battle safe-stage aspect ratio:

```cpp
AutoBattleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattleAutoPlayButton"));
StyleBattleActionButton(AutoBattleButton, FName(TEXT("BattleAutoPlay")));
AutoBattleLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleAutoPlayLabel"));
AutoBattleLabel->SetText(FText::FromString(IsAutoBattleEnabled() ? TEXT("自动战斗：开") : TEXT("自动战斗：关")));
AutoBattleLabel->SetJustification(ETextJustify::Center);
AutoBattleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
AutoBattleButton->AddChild(AutoBattleLabel);
AutoBattleButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleAutoBattleClicked);
```

- [ ] **Step 4: Cold-build and verify GREEN**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Integration.CardBattle.BoardAutoPlayToggle',
  'GameXXK.Integration.CardBattle.BoardPartyQiResponsive',
  'GameXXK.Integration.CardBattle.BoardPresentationInputGate'
) -TimeoutSeconds 420
```

Expected: the toggle, labels, and existing end-turn/Party Qi/presentation-lock tests all pass.

- [ ] **Step 5: Commit the switch and control**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp
git commit -m "feat: add battle auto-play session toggle"
```

### Task 4: Auto-play only legal Board actions, one presentation-safe step at a time

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing tests for each owned action boundary**

Add focused tests:

- `BoardAutoPlayManualTarget`: use `BuildManualTargetCardFixture`, enable auto, call `AdvanceAutoBattleForTest(0.75f)`, and assert the known card leaves hand, the stable first legal target receives the mutation, and Board presentation is queued.
- `BoardAutoPlayPendingChoices`: install live Insight, HeroTaskSearch, and forced-discard choices one at a time; assert the stable first candidate is submitted through the Board and the blocking choice clears.
- `BoardAutoPlayMultiDiscard`: open a real forced-discard choice with `RequiredCount == 2`; assert exactly the first two stable hand candidates are submitted together and the choice clears.
- `BoardAutoPlayEndsTurn`: make every hand card unplayable with cost overrides, then assert one auto step invokes the normal `EndCardPlayerPhase()` path and enters Enemy/presentation state.
- `BoardAutoPlayWaitsForPresentation`: queue a card presentation, call another auto step, and assert hand/energy/phase do not mutate until the presentation drains.
- `BoardAutoPlayStopsOutsideBattle`: snapshot `VisitedRouteNodeIds`, `ReachableRouteNodeIds`, `PendingRouteNodeId`, and `Screen == DungeonMap`; call the auto step and assert exact equality.
- `BoardAutoPlayStopsAtTerminal`: set `Phase` to Victory and Defeat in separate fixtures and assert no state mutation.

The manual-target assertion uses the desired public seam:

```cpp
TestTrue(TEXT("auto-play advances one legal Board-owned action"), Board->AdvanceAutoBattleForTest(0.75f));
TestFalse(TEXT("the played stable instance leaves hand"),
	Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate(
		[CardInstanceId](const FGameXXKCardInstance& Card) { return Card.InstanceId == CardInstanceId; }));
TestTrue(TEXT("the action owns a normal presentation boundary"),
	Board->GetBattlePresentationQueueCountForTest() > 0 || Board->IsPlayedCardCommitActiveForTest());
```

- [ ] **Step 2: Cold-build and verify RED**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: compile fails because `AdvanceAutoBattleForTest` and the batch forced-discard path do not exist.

- [ ] **Step 3: Implement the cadence and stable decision order**

Add `TickAutoBattle`, `AdvanceAutoBattleStep`, and a 0.75-second accumulator to the Board. Call it at the end of `NativeTick`, after visual and enemy-intent advancement.

The gate must reject and reset the accumulator when any of these is true:

```cpp
!Subsystem->IsBattleAutoPlayEnabled()
|| State.Screen != EGameXXKScreen::Battle
|| !State.CardRun.bHasActiveCardBattle
|| Runtime.Phase != EGameXXKCardBattlePhase::Player
|| IsBattlePresentationPending()
|| IsEnemyIntentPresentationActive()
|| bEnemyIntentCompletionRecoveryPending
|| IsCardTargetingActive()
```

For one eligible step, use these complete stable-selection helpers:

```cpp
TArray<FName> UGameXXKBattleBoardWidget::BuildStableForcedDiscardSelection(
	const FGameXXKPendingCardChoice& Pending,
	const FGameXXKBattleDeckState& Deck) const
{
	TArray<FName> Selection;
	for (const FGameXXKCardInstance& Candidate : Pending.Candidates)
	{
		if (Selection.Num() >= Pending.RequiredCount)
		{
			break;
		}
		if (Deck.Hand.ContainsByPredicate([&Candidate](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == Candidate.InstanceId;
		}))
		{
			Selection.Add(Candidate.InstanceId);
		}
	}
	return Selection;
}

TArray<FGameXXKCardInstance> UGameXXKBattleBoardWidget::BuildStablePendingCandidates(
	const FGameXXKPendingCardChoice& Pending) const
{
	TArray<FGameXXKCardInstance> Candidates = Pending.Candidates;
	Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
	{
		return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
			? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
			: Left.InstanceId.LexicalLess(Right.InstanceId);
	});
	return Candidates;
}
```

Then implement one action without direct route or reward calls:

```cpp
bool UGameXXKBattleBoardWidget::AdvanceAutoBattleStep()
{
	UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (State.Screen != EGameXXKScreen::Battle || !State.CardRun.bHasActiveCardBattle)
	{
		return false;
	}
	const FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
	const FGameXXKPendingCardChoice Pending = Runtime.Deck.PendingChoice;
	switch (Pending.Kind)
	{
	case EGameXXKCardPendingChoiceKind::ForcedDiscard:
		return SubmitPendingForcedDiscards(BuildStableForcedDiscardSelection(Pending, Runtime.Deck));
	case EGameXXKCardPendingChoiceKind::InsightChooseToHand:
		return !Pending.InsightTopOrder.IsEmpty() && SubmitPendingInsightChoice(Pending.InsightTopOrder[0]);
	case EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand:
	{
		const TArray<FGameXXKCardInstance> Candidates = BuildStablePendingCandidates(Pending);
		return !Candidates.IsEmpty() && SubmitPendingHeroTaskSearchChoice(Candidates[0].InstanceId);
	}
	default:
		break;
	}

	const TArray<FGameXXKCardInstance> HandSnapshot = Runtime.Deck.Hand;
	for (const FGameXXKCardInstance& Card : HandSnapshot)
	{
		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, Card.InstanceId, Preview, &Error)
			|| !Preview.bCanPlay)
		{
			continue;
		}
		FName StableTarget = NAME_None;
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
			{
				if (Candidate.bCanSelect && !Candidate.UnitId.IsNone())
				{
					StableTarget = Candidate.UnitId;
					break;
				}
			}
			if (StableTarget.IsNone())
			{
				continue;
			}
		}
		if (!ClickCardInHand(Card.InstanceId))
		{
			return false;
		}
		return !Preview.TargetRequest.bRequiresManualSelection || ConfirmTargetingUnit(StableTarget);
	}
	return EndCardPlayerPhase();
}
```

Implement a Board-owned batch helper so multi-discard remains presentation-safe:

```cpp
bool UGameXXKBattleBoardWidget::SubmitPendingForcedDiscards(const TArray<FName>& DiscardedInstanceIds)
{
	if (RejectBattleHudFixtureMutation() || RejectBattlePresentationMutation())
	{
		return false;
	}
	UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle
		|| !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有需要弃置的手牌。");
		return false;
	}
	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPendingCardChoice& Pending = MutableState.CardRun.ActiveBattle.Deck.PendingChoice;
	TSet<FName> UniqueIds;
	for (const FName InstanceId : DiscardedInstanceIds)
	{
		UniqueIds.Add(InstanceId);
	}
	if (Pending.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard
		|| DiscardedInstanceIds.Num() != Pending.RequiredCount
		|| UniqueIds.Num() != DiscardedInstanceIds.Num())
	{
		LastCardInteractionError = TEXT("弃牌数量与当前要求不一致。");
		return false;
	}
	for (const FName InstanceId : DiscardedInstanceIds)
	{
		const bool bCandidate = Pending.Candidates.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
		if (InstanceId.IsNone() || !bCandidate)
		{
			LastCardInteractionError = TEXT("所选卡牌不在当前弃牌列表中。");
			return false;
		}
	}
	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitForcedDiscard(
		MutableState, DiscardedInstanceIds, &Error, &ResumedResults))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		FlattenResumedCardDamageResults(ResumedResults),
		EBattlePresentationContinuation::FinalizeCardMutation);
}

bool UGameXXKBattleBoardWidget::SubmitPendingForcedDiscard(const FName DiscardedInstanceId)
{
	return SubmitPendingForcedDiscards({DiscardedInstanceId});
}
```

No auto code may call `SelectRouteNodeById`, `SelectDungeonNode`, event/shop/reward APIs, retry APIs, or mutate `FGameXXKRuntimeState` directly outside these Board submission implementations.

- [ ] **Step 4: Run focused GREEN tests and full card-battle regression**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Integration.CardBattle.BoardAutoPlay',
  'GameXXK.Integration.CardBattle',
  'GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets'
) -TimeoutSeconds 720
```

Expected: all selected tests pass, including presentation ordering and pending-choice regressions.

- [ ] **Step 5: Commit auto-play behavior**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp
git commit -m "feat: auto-play legal cards in existing battles"
```

### Task 5: Correct the performance profile and prove the real player flow

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `scripts/measure_desktop_training_hud_memory.ps1`
- Modify: `scripts/test_measure_desktop_training_hud_memory.py`
- Modify: `scripts/README.md`
- Modify: `docs/production/current-goal-acceptance.md`
- Modify: `docs/production/2026-08-19-goal-progress-evidence.md`
- Evidence: `Saved/HarnessReports/*`, `Saved/VisualReview/20260819-route-owned-auto-battle/*`

- [ ] **Step 1: Write a failing corrected-profile test**

Retain the command-line report key `challenge` for schema compatibility, but require it to build the existing route battle, not a workbench view:

```cpp
TestTrue(TEXT("battle profile applies"), Controller->ApplyDesktopTrainingPerfProfileForTest(TEXT("challenge")));
TestEqual(TEXT("battle profile enters the existing Battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
TestTrue(TEXT("battle profile owns an active CardBattle"), Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);
TestFalse(TEXT("battle profile closes the desktop workbench"),
	Controller->GetDesktopTrainingWorkbenchWidgetForTest()->IsWorkbenchVisibleForTest());
TestNotNull(TEXT("battle profile uses the existing full BattleBoard"), Controller->GetBattleBoardWidgetForTest());
```

Add sampler/source assertions that `challenge` no longer references `ChallengeViewport` and is described as `existing-fullscreen-battle`.

- [ ] **Step 2: Verify RED, then implement the explicit development fixture**

Run the newly added `GameXXK.MVP.UI.DesktopTrainingBattlePerfProfile` test first. Expected: FAIL because the current `challenge` branch still calls the workbench Challenge action and does not create the existing full BattleBoard.

Inside the explicit `-GameXXKPerfProfile=challenge` branch only, use the complete development fixture below. It may select Start/Battle nodes only because this is a named measurement fixture; no player-facing action or auto-play path may call those APIs:

```cpp
if (NormalizedProfile == TEXT("challenge"))
{
	UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem
		|| !Subsystem->AcceptQuest()
		|| !Subsystem->OpenDungeonFromTownExit()
		|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start)
		|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle))
	{
		return false;
	}
	CloseDesktopTrainingWorkbench();
	if (!EnsurePlayerFlowWidgets())
	{
		return false;
	}
	RefreshPlayerFlowWidgets();
	return BattleBoardWidget
		&& BattleBoardWidget->IsBattleBoardVisible()
		&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle;
}
```

- [ ] **Step 3: Cold-build and run the full automated gate**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.SaveGame',
  'GameXXK.Training',
  'GameXXK.DesktopTraining.Workbench',
  'GameXXK.Integration.CardBattle',
  'GameXXK.MVP.UI'
) -TimeoutSeconds 1200
python scripts/harness_state_validator.py --json
```

Expected: UBT `Result: Succeeded`, all selected Automation tests pass, script tests pass, and harness state validates.

- [ ] **Step 4: Run real PIE/MCP and visual evidence**

Use only project UE MCP scripts. Save dirty packages through MCP before any editor restart. Run the existing real play flow through quest acceptance, town exit, visible route map, a manually selected battle node, then enable auto play and observe at least one legal card plus one enemy turn:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
python scripts/gamexxk_real_play_flow_mcp.py --timeout 600 --report Saved/HarnessReports/route-owned-auto-battle-real-flow.json
```

Capture 1672x941 and 1920x1080 screenshots for route map and battle. Run Luna max through:

```powershell
& 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' -Effort max
```

Visual acceptance:

- route map is the existing full player-clickable map;
- battle is the existing full-screen BattleBoard at the established aspect ratio;
- no workbench paper, repeated route text, 3+3 strip, embedded board, or actor/HUD overflow appears;
- only the existing-style `自动战斗：开/关` control is new;
- card, target, hit, death, and enemy-intent presentation finishes before the next auto action.

- [ ] **Step 5: Rerun the corrected four-profile sample and record evidence**

Run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/measure_desktop_training_hud_memory.ps1 -Profile all
```

Record that the old 20260819-184206 `challenge` number is a rejected pre-correction baseline and that the new `challenge` profile is the existing full-screen battle. Do not compare them as equivalent surfaces.

- [ ] **Step 6: Verify protected files and commit only target changes**

Run:

```powershell
git diff --check
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
git status --short
```

Expected protected map hash:

```text
EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B
```

Do not stage `Content/GameXXK/Maps/L_Main.umap`, `scripts/test_battle_camera_framing.py`, `SourceAssets/`, `SourceArt/` review trees, root `Private/Public`, or historical probes. Commit the corrected profile, docs, and scripts with:

```powershell
git commit -m "test: verify route-owned auto battle flow"
```

## Completion boundary

This correction is complete only when all of the following are simultaneously true:

- Challenge enters the existing route map and leaves node choice to the player.
- Monster battles use the existing full-screen BattleBoard.
- The one new control can enable/disable session-level auto play.
- Auto play uses legal stable cards, targets, pending choices, and end turn through Board presentation APIs.
- Auto play never chooses routes, events, shops, rewards, retries, quests, or followers.
- The rejected workbench battle UI has no source or runtime path.
- Cold UBT, focused/full Automation, real PIE/MCP, dual-resolution screenshots, and Luna max all pass.
- Protected map and user-tuned art remain untouched.
