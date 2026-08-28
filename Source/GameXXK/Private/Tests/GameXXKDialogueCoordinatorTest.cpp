#include "Misc/AutomationTest.h"

#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueCoordinator.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKDialogueCoordinatorTestPrivate
{
	FGameXXKDialogueStartContext Context()
	{
		FGameXXKDialogueStartContext Result;
		Result.StoryId = TEXT("Story.Test");
		Result.StoryVersion = 1;
		Result.TaskId = TEXT("Task.Test");
		Result.StepId = TEXT("Step.Test");
		Result.SequenceId = TEXT("Sequence.Test");
		Result.StageContractId = TEXT("Stage.Test");
		return Result;
	}

	UGameXXKDialogueAsset* BranchingAsset()
	{
		UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
		Asset->DialogueId = TEXT("Dialogue.Test.Coordinator");
		Asset->DialogueVersion = 1;
		Asset->EntryNodeId = TEXT("line.first");
		FGameXXKDialogueNodeDefinition First;
		First.NodeId = TEXT("line.first");
		First.Type = EGameXXKDialogueNodeType::Line;
		First.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		First.SpeakerId = TEXT("Npc.YueBai");
		First.Text = FText::FromString(TEXT("你是谁？"));
		First.NextNodeId = TEXT("choice");
		Asset->Nodes.Add(First);
		FGameXXKDialogueNodeDefinition Choice;
		Choice.NodeId = TEXT("choice");
		Choice.Type = EGameXXKDialogueNodeType::Choice;
		Choice.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		FGameXXKDialogueOptionDefinition Left;
		Left.OptionId = TEXT("left");
		Left.Text = FText::FromString(TEXT("我是你恩公"));
		Left.OutcomeId = TEXT("Outcome.Option.Left");
		Left.NextNodeId = TEXT("line.last");
		Choice.Options.Add(Left);
		FGameXXKDialogueOptionDefinition Right;
		Right.OptionId = TEXT("right");
		Right.Text = FText::FromString(TEXT("我不知道"));
		Right.OutcomeId = TEXT("Outcome.Option.Right");
		Right.NextNodeId = TEXT("line.last");
		Choice.Options.Add(Right);
		Asset->Nodes.Add(Choice);
		FGameXXKDialogueNodeDefinition Last;
		Last.NodeId = TEXT("line.last");
		Last.Type = EGameXXKDialogueNodeType::Line;
		Last.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		Last.SpeakerId = TEXT("Character.Hero");
		Last.Text = FText::FromString(TEXT("前方路远。"));
		Last.NextNodeId = TEXT("end");
		Asset->Nodes.Add(Last);
		FGameXXKDialogueNodeDefinition End;
		End.NodeId = TEXT("end");
		End.Type = EGameXXKDialogueNodeType::End;
		End.EndOutcomeId = TEXT("Outcome.Dialogue.Done");
		Asset->Nodes.Add(End);
		return Asset;
	}

	void Bind(
		UGameXXKDialogueCoordinator* Coordinator,
		FGameXXKDialogueSessionState& Session,
		UGameXXKDialoguePanelWidget*& OutPanel,
		UGameXXKDialogueHistoryWidget*& OutHistory)
	{
		OutPanel = NewObject<UGameXXKDialoguePanelWidget>();
		OutPanel->TakeWidget();
		OutHistory = NewObject<UGameXXKDialogueHistoryWidget>();
		OutHistory->TakeWidget();
		Coordinator->Bind(Session, OutPanel, nullptr, OutHistory);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueCoordinatorManualAndOutcomeTest,
	"GameXXK.Dialogue.Coordinator.ManualChoiceAndOutcomeOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueCoordinatorManualAndOutcomeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDialogueCoordinatorTestPrivate;
	UGameXXKDialogueCoordinator* Coordinator = NewObject<UGameXXKDialogueCoordinator>();
	FGameXXKDialogueSessionState Session;
	UGameXXKDialoguePanelWidget* Panel = nullptr;
	UGameXXKDialogueHistoryWidget* History = nullptr;
	Bind(Coordinator, Session, Panel, History);
	UGameXXKDialogueAsset* Asset = BranchingAsset();
	int32 FinishCount = 0;
	FName FinishedDialogue;
	FName FinishedOutcome;
	FGameXXKDialogueFinished Finished = FGameXXKDialogueFinished::CreateLambda(
		[&](const FName DialogueId, const FName OutcomeId)
		{
			++FinishCount;
			FinishedDialogue = DialogueId;
			FinishedOutcome = OutcomeId;
		});
	FString Error;
	TestTrue(FString::Printf(TEXT("dialogue starts: %s"), *Error),
		Coordinator->StartDialogue(*Asset, Context(), Finished, &Error));
	TestTrue(TEXT("dialogue blocks while visible"), Coordinator->IsBlockingPresentation());
	TestEqual(TEXT("entry line presented"), Coordinator->GetCurrentNodeIdForTest(), FName(TEXT("line.first")));
	TestFalse(TEXT("second blocking session rejects"),
		Coordinator->StartDialogue(*Asset, Context(), Finished, &Error));

	TestTrue(TEXT("manual advance reaches choice"), Coordinator->Advance(&Error));
	TestEqual(TEXT("choice is current"), Coordinator->GetCurrentNodeIdForTest(), FName(TEXT("choice")));
	TestEqual(TEXT("first line added to history"), History->GetHistoryCountForTest(), 1);
	Coordinator->SetAutoEnabled(true);
	TestFalse(TEXT("choice pauses auto mode"), Coordinator->TickAuto(20.0f, &Error));
	TestEqual(TEXT("choice remains current"), Coordinator->GetCurrentNodeIdForTest(), FName(TEXT("choice")));

	TestTrue(TEXT("choice dispatches"), Coordinator->ChooseOption(TEXT("left"), &Error));
	TestEqual(TEXT("option outcome does not finish dialogue"), FinishCount, 0);
	TestEqual(TEXT("branch line presented"), Coordinator->GetCurrentNodeIdForTest(), FName(TEXT("line.last")));
	TestTrue(TEXT("final line advances to End"), Coordinator->Advance(&Error));
	TestEqual(TEXT("finish callback fires once"), FinishCount, 1);
	TestEqual(TEXT("finished dialogue ID"), FinishedDialogue, Asset->DialogueId);
	TestEqual(TEXT("only End outcome returns"), FinishedOutcome, FName(TEXT("Outcome.Dialogue.Done")));
	TestFalse(TEXT("finished session is inactive"), Session.bActive);
	TestFalse(TEXT("extra advance rejects"), Coordinator->Advance(&Error));
	TestEqual(TEXT("finish callback remains exactly once"), FinishCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueCoordinatorAutoSkipPauseTest,
	"GameXXK.Dialogue.Coordinator.AutoSkipPauseResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueCoordinatorAutoSkipPauseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDialogueCoordinatorTestPrivate;
	TestEqual(TEXT("ten visible characters use minimum delay"),
		UGameXXKDialogueCoordinator::ComputeAutoDelayForTest(10, 0.0f, 0.0f), 1.2f);
	TestEqual(TEXT("hundred visible characters clamp text delay to six"),
		UGameXXKDialogueCoordinator::ComputeAutoDelayForTest(100, 0.0f, 0.0f), 6.0f);
	TestEqual(TEXT("voice duration wins"),
		UGameXXKDialogueCoordinator::ComputeAutoDelayForTest(10, 2.5f, 1.8f), 2.5f);
	TestEqual(TEXT("animation duration may exceed text clamp"),
		UGameXXKDialogueCoordinator::ComputeAutoDelayForTest(100, 2.0f, 7.0f), 7.0f);

	UGameXXKDialogueCoordinator* Coordinator = NewObject<UGameXXKDialogueCoordinator>();
	FGameXXKDialogueSessionState Session;
	UGameXXKDialoguePanelWidget* Panel = nullptr;
	UGameXXKDialogueHistoryWidget* History = nullptr;
	Bind(Coordinator, Session, Panel, History);
	UGameXXKDialogueAsset* Asset = BranchingAsset();
	int32 FinishCount = 0;
	FString Error;
	TestTrue(TEXT("auto fixture starts"), Coordinator->StartDialogue(
		*Asset,
		Context(),
		FGameXXKDialogueFinished::CreateLambda([&](FName, FName) { ++FinishCount; }),
		&Error));
	Coordinator->SetAutoEnabled(true);
	Coordinator->SetPresentationDurations(2.5f, 1.8f);
	TestFalse(TEXT("auto waits before max duration"), Coordinator->TickAuto(2.49f, &Error));
	TestTrue(TEXT("auto advances at max duration"), Coordinator->TickAuto(0.01f, &Error));
	TestEqual(TEXT("auto reaches choice"), Coordinator->GetCurrentNodeIdForTest(), FName(TEXT("choice")));
	TestTrue(TEXT("choose branch for skip fixture"), Coordinator->ChooseOption(TEXT("left"), &Error));
	TestFalse(TEXT("unseen current node cannot Ctrl-skip"), Coordinator->SkipSeenCurrentNode(&Error));
	Session.SeenNodeIds.Add(TEXT("line.last"));
	TestTrue(TEXT("seen current node can Ctrl-skip"), Coordinator->SkipSeenCurrentNode(&Error));
	TestEqual(TEXT("skip completes once"), FinishCount, 1);

	FGameXXKDialogueSessionState PausedSession;
	UGameXXKDialogueCoordinator* Paused = NewObject<UGameXXKDialogueCoordinator>();
	Bind(Paused, PausedSession, Panel, History);
	TestTrue(TEXT("pause fixture starts"), Paused->StartDialogue(
		*Asset, Context(), FGameXXKDialogueFinished(), &Error));
	const FName ReplayableNode = PausedSession.CurrentNodeId;
	Paused->PauseAndExit();
	TestTrue(TEXT("pause keeps session active"), PausedSession.bActive);
	TestEqual(TEXT("pause stays at replayable node boundary"), PausedSession.CurrentNodeId, ReplayableNode);
	TestFalse(TEXT("paused presenter releases blocking state"), Paused->IsBlockingPresentation());
	TestFalse(TEXT("paused panel hidden"), Panel->GetVisibility() != ESlateVisibility::Collapsed);
	TestTrue(TEXT("resume replays same node"), Paused->ResumeDialogue(*Asset, &Error));
	TestEqual(TEXT("resume keeps current node"), Paused->GetCurrentNodeIdForTest(), ReplayableNode);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueHistoryTrimTest,
	"GameXXK.Dialogue.Coordinator.HistoryTrim100",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueHistoryTrimTest::RunTest(const FString& Parameters)
{
	UGameXXKDialogueHistoryWidget* History = NewObject<UGameXXKDialogueHistoryWidget>();
	History->TakeWidget();
	TArray<FGameXXKDialogueHistoryEntry> Entries;
	for (int32 Index = 0; Index < 125; ++Index)
	{
		FGameXXKDialogueHistoryEntry Entry;
		Entry.TextId = FName(*FString::Printf(TEXT("history.%03d"), Index));
		Entry.Text = FText::FromString(FString::Printf(TEXT("第%d条"), Index));
		Entries.Add(Entry);
	}
	History->PresentHistory(Entries);
	TestEqual(TEXT("history presenter trims to one hundred"), History->GetHistoryCountForTest(), 100);
	TestEqual(TEXT("history retains newest first entry"),
		History->GetHistoryEntryForTest(0).TextId, FName(TEXT("history.025")));
	TestEqual(TEXT("history retains newest final entry"),
		History->GetHistoryEntryForTest(99).TextId, FName(TEXT("history.124")));
	TestTrue(TEXT("history is read-only"), History->IsReadOnlyForTest());
	return true;
}

#endif
