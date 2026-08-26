#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingThreeNodeMapPanelTest,
	"GameXXK.DesktopTraining.Workbench.ThreeNodeChapterMapPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingThreeNodeMapPanelTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("training-map fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	if (!TestTrue(TEXT("training-map fixture opens the workbench"), Widget->OpenWorkbench())
		|| !TestTrue(TEXT("training-map fixture opens the Backpack parent"), Widget->OpenBackpack()))
	{
		return false;
	}
	Widget->HandleActionClicked(4);
	const FName NormalOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	const FGameXXKTrainingTravelRuntime RuntimeBeforeDifficultyView =
		Subsystem->GetTrainingTravelRuntimeCopy();

	TestNotNull(TEXT("training map uses one dropdown difficulty control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingDifficultyDropdownButton")) : nullptr);
	TestNull(TEXT("the old three-button difficulty row is removed"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingDifficultyTab_0")) : nullptr);
	Widget->HandleActionClicked(620);
	UButton* NormalOption = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingDifficultyOption_0")))
		: nullptr;
	UButton* HardOption = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingDifficultyOption_1")))
		: nullptr;
	TestNotNull(TEXT("opening the dropdown reveals Normal"), NormalOption);
	TestNotNull(TEXT("opening the dropdown keeps locked difficulties visible"), HardOption);
	TestTrue(TEXT("Normal is selectable in a new game"), NormalOption && NormalOption->GetIsEnabled());
	TestTrue(TEXT("Hard remains selectable for view before Normal 3-3 is cleared"),
		HardOption && HardOption->GetIsEnabled());
	const UTextBlock* HardOptionText = HardOption ? Cast<UTextBlock>(HardOption->GetContent()) : nullptr;
	TestTrue(TEXT("Hard is explicitly labelled as locked progression"),
		HardOptionText && HardOptionText->GetText().ToString().Contains(TEXT("未解锁")));
	Widget->HandleActionClicked(622);
	TestEqual(TEXT("viewing locked Hard selects Hard 1-1"),
		Widget->GetSelectedStageIdForTest(),
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hard, 1));
	TestEqual(TEXT("viewing locked Hard keeps Normal 1-1 as the Travel target"),
		Widget->GetCurrentTravelStageIdForTest(),
		NormalOne);
	const FGameXXKTrainingTravelRuntime RuntimeAfterDifficultyView =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("viewing locked Hard preserves the Travel runtime stage"),
		RuntimeAfterDifficultyView.StageId,
		RuntimeBeforeDifficultyView.StageId);
	TestEqual(TEXT("viewing locked Hard does not reset the walk cursor"),
		RuntimeAfterDifficultyView.WalkStep,
		RuntimeBeforeDifficultyView.WalkStep);
	UButton* LockedChallenge = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingChallengeButton")))
		: nullptr;
	UButton* LockedTravel = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingTravelButton")))
		: nullptr;
	TestFalse(TEXT("locked Hard cannot be challenged"), LockedChallenge && LockedChallenge->GetIsEnabled());
	TestFalse(TEXT("locked Hard cannot be travelled"), LockedTravel && LockedTravel->GetIsEnabled());
	Widget->HandleActionClicked(620);
	Widget->HandleActionClicked(621);
	for (int32 ChapterIndex = 0; ChapterIndex < 3; ++ChapterIndex)
	{
		TestNotNull(
			*FString::Printf(TEXT("chapter tab %d exists"), ChapterIndex + 1),
			Widget->WidgetTree
				? Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("TrainingChapterTab_%d"), ChapterIndex))
				: nullptr);
	}
	TestEqual(TEXT("one chapter materializes exactly three route nodes"),
		Widget->GetTrainingStageButtonCountForTest(), 3);

	TArray<UCanvasPanelSlot*> NodeSlots;
	for (int32 StageNumber = 1; StageNumber <= 3; ++StageNumber)
	{
		UWidget* Node = Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("TrainingNode_%d"), StageNumber))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("chapter-one node %d exists"), StageNumber), Node);
		NodeSlots.Add(Node ? Cast<UCanvasPanelSlot>(Node->Slot) : nullptr);
	}
	TestNull(TEXT("the next chapter is not stacked into the active chapter map"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingNode_4")) : nullptr);
	if (NodeSlots.Num() == 3 && NodeSlots[0] && NodeSlots[1] && NodeSlots[2])
	{
		TestTrue(TEXT("the three chapter nodes share one vertical route axis"),
			FMath::IsNearlyEqual(NodeSlots[0]->GetPosition().X, NodeSlots[1]->GetPosition().X)
				&& FMath::IsNearlyEqual(NodeSlots[1]->GetPosition().X, NodeSlots[2]->GetPosition().X));
		TestTrue(TEXT("the route advances from top to bottom"),
			NodeSlots[0]->GetPosition().Y < NodeSlots[1]->GetPosition().Y
				&& NodeSlots[1]->GetPosition().Y < NodeSlots[2]->GetPosition().Y);
	}

	Widget->HandleActionClicked(625);
	TestEqual(TEXT("switching chapters still materializes only three nodes"),
		Widget->GetTrainingStageButtonCountForTest(), 3);
	TestNull(TEXT("chapter one nodes leave the tree when chapter two is selected"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingNode_1")) : nullptr);
	TestNotNull(TEXT("chapter two begins at node 2-1"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TrainingNode_4")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingChapterSelectionDoesNotRestartTravelTest,
	"GameXXK.DesktopTraining.Workbench.ChapterSelectionDoesNotRestartTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingChapterSelectionDoesNotRestartTravelTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("chapter-selection fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	Subsystem->GetMutableRuntimeState().Training.ClearedStageIds.Add(StageTwo);

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("fixture advances into the current Travel walk delay"),
		Subsystem->AdvanceTrainingTravelStep(
			bEncounterCompleted,
			bStageCompleted,
			bDefeated,
			Reward,
			1));
	const FGameXXKTrainingTravelRuntime BeforeChapterSwitch =
		Subsystem->GetTrainingTravelRuntimeCopy();

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("chapter-selection workbench opens"), Widget->OpenWorkbench());
	TestTrue(TEXT("chapter-selection Backpack parent opens"), Widget->OpenBackpack());
	Widget->HandleActionClicked(4);
	Widget->HandleActionClicked(625);

	TestEqual(TEXT("viewing chapter two keeps the current Travel target"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId,
		StageOne);
	const FGameXXKTrainingTravelRuntime AfterChapterSwitch =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("viewing another chapter keeps the Travel runtime stage"),
		AfterChapterSwitch.StageId,
		BeforeChapterSwitch.StageId);
	TestEqual(TEXT("viewing another chapter does not restart the walk delay"),
		AfterChapterSwitch.WalkStep,
		BeforeChapterSwitch.WalkStep);
	TestEqual(TEXT("viewing another chapter preserves the Travel phase"),
		AfterChapterSwitch.Phase,
		BeforeChapterSwitch.Phase);

	TestTrue(TEXT("selecting another cleared node succeeds without travelling"),
		Widget->SelectStageForTest(StageTwo));
	TestEqual(TEXT("node selection alone still keeps the old Travel target"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId,
		StageOne);
	TestTrue(TEXT("pressing Travel starts the selected cleared node"), Widget->ClickTravelForTest());
	TestEqual(TEXT("Travel button changes the target to the selected node"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId,
		StageTwo);
	const FGameXXKTrainingTravelRuntime Restarted = Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("Travel button rebuilds the runtime for the selected node"), Restarted.StageId, StageTwo);
	TestEqual(TEXT("Travel button restarts at walking"), Restarted.Phase, EGameXXKTrainingTravelPhase::Walking);
	TestEqual(TEXT("Travel button restarts the five-second walk cursor"), Restarted.WalkStep, 0);
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Restarted.PartyUnits)
	{
		TestEqual(TEXT("Travel button restores every party member to full health"), Unit.HP, Unit.MaxHP);
	}
	return true;
}

#endif
