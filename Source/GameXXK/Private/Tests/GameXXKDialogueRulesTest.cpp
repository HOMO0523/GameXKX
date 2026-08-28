#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"

#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueRules.h"
#include "Dialogue/GameXXKDialogueTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueAssetContractTest,
	"GameXXK.Dialogue.Core.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueAssetContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
	Asset->DialogueId = TEXT("Dialogue.Test.Branching");
	Asset->DialogueVersion = 1;
	Asset->EntryNodeId = TEXT("start");

	FGameXXKDialogueNodeDefinition Start;
	Start.NodeId = TEXT("start");
	Start.Type = EGameXXKDialogueNodeType::Line;
	Start.NextNodeId = TEXT("end");
	Asset->Nodes.Add(Start);

	FGameXXKDialogueNodeDefinition End;
	End.NodeId = TEXT("end");
	End.Type = EGameXXKDialogueNodeType::End;
	End.EndOutcomeId = TEXT("Outcome.Test.Done");
	Asset->Nodes.Add(End);

	TestNotNull(TEXT("entry resolves"), Asset->FindNode(TEXT("start")));
	TestNull(TEXT("missing node rejects"), Asset->FindNode(TEXT("missing")));

	FDataValidationContext ValidContext;
	TestEqual(TEXT("unique node ids validate"), Asset->IsDataValid(ValidContext), EDataValidationResult::Valid);

	Asset->Nodes.Add(Start);
	FDataValidationContext DuplicateContext;
	TestEqual(TEXT("duplicate node ids reject"), Asset->IsDataValid(DuplicateContext), EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueRunnerBranchingTest,
	"GameXXK.Dialogue.Core.RunnerBranching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueRunnerBranchingTest::RunTest(const FString& Parameters)
{
	UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
	Asset->DialogueId = TEXT("Dialogue.Test.Branching");
	Asset->DialogueVersion = 3;
	Asset->EntryNodeId = TEXT("start");

	FGameXXKDialogueNodeDefinition Start;
	Start.NodeId = TEXT("start");
	Start.Type = EGameXXKDialogueNodeType::Line;
	Start.Presentation = EGameXXKDialoguePresentation::Bubble;
	Start.SpeakerId = TEXT("Hero");
	Start.TextId = TEXT("test.start");
	Start.Text = FText::FromString(TEXT("开始"));
	Start.NextNodeId = TEXT("choice");
	Asset->Nodes.Add(Start);

	FGameXXKDialogueNodeDefinition Choice;
	Choice.NodeId = TEXT("choice");
	Choice.Type = EGameXXKDialogueNodeType::Choice;
	Choice.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
	FGameXXKDialogueOptionDefinition Left;
	Left.OptionId = TEXT("left");
	Left.Text = FText::FromString(TEXT("向左"));
	Left.OutcomeId = TEXT("Outcome.Test.Left");
	Left.NextNodeId = TEXT("left.line");
	Choice.Options.Add(Left);
	FGameXXKDialogueOptionDefinition Right;
	Right.OptionId = TEXT("right");
	Right.Text = FText::FromString(TEXT("向右"));
	Right.OutcomeId = TEXT("Outcome.Test.Right");
	Right.NextNodeId = TEXT("right.line");
	Choice.Options.Add(Right);
	Asset->Nodes.Add(Choice);

	FGameXXKDialogueNodeDefinition LeftLine;
	LeftLine.NodeId = TEXT("left.line");
	LeftLine.Type = EGameXXKDialogueNodeType::Line;
	LeftLine.Text = FText::FromString(TEXT("左边"));
	LeftLine.NextNodeId = TEXT("left.end");
	Asset->Nodes.Add(LeftLine);

	FGameXXKDialogueNodeDefinition RightLine;
	RightLine.NodeId = TEXT("right.line");
	RightLine.Type = EGameXXKDialogueNodeType::Line;
	RightLine.Text = FText::FromString(TEXT("右边"));
	RightLine.NextNodeId = TEXT("right.end");
	Asset->Nodes.Add(RightLine);

	FGameXXKDialogueNodeDefinition LeftEnd;
	LeftEnd.NodeId = TEXT("left.end");
	LeftEnd.Type = EGameXXKDialogueNodeType::End;
	LeftEnd.EndOutcomeId = TEXT("Outcome.Test.LeftDone");
	Asset->Nodes.Add(LeftEnd);

	FGameXXKDialogueNodeDefinition RightEnd;
	RightEnd.NodeId = TEXT("right.end");
	RightEnd.Type = EGameXXKDialogueNodeType::End;
	RightEnd.EndOutcomeId = TEXT("Outcome.Test.RightDone");
	Asset->Nodes.Add(RightEnd);

	FGameXXKDialogueStartContext StartContext;
	StartContext.StoryId = TEXT("Story.Test");
	StartContext.StoryVersion = 2;
	StartContext.TaskId = TEXT("Task.Test");
	StartContext.StepId = TEXT("Step.Test");
	StartContext.SequenceId = TEXT("Sequence.Test");
	StartContext.StageContractId = TEXT("Stage.Test");

	FGameXXKDialogueSessionState Session;
	FGameXXKDialogueOutput Output;
	FString Error;
	TestTrue(FString::Printf(TEXT("start succeeds: %s"), *Error),
		FGameXXKDialogueRules::Start(*Asset, StartContext, Session, Output, &Error));
	TestTrue(TEXT("session active"), Session.bActive);
	TestEqual(TEXT("story context copied"), Session.StoryId, StartContext.StoryId);
	TestEqual(TEXT("start node shown"), Output.NodeId, FName(TEXT("start")));
	TestEqual(TEXT("start uses bubble"), Output.Presentation, EGameXXKDialoguePresentation::Bubble);

	FGameXXKDialogueOutput SecondStartOutput;
	TestFalse(TEXT("second active start rejects"),
		FGameXXKDialogueRules::Start(*Asset, StartContext, Session, SecondStartOutput, &Error));
	TestEqual(TEXT("second start leaves current node"), Session.CurrentNodeId, FName(TEXT("start")));

	TestTrue(TEXT("line completes"),
		FGameXXKDialogueRules::CompletePresentedNode(*Asset, Session, Output, &Error));
	TestEqual(TEXT("choice reached"), Session.CurrentNodeId, FName(TEXT("choice")));
	TestEqual(TEXT("two options visible"), Output.Options.Num(), 2);
	TestTrue(TEXT("start node marked seen"), Session.SeenNodeIds.Contains(TEXT("start")));

	const int32 SelectionCountBeforeInvalid = Session.SelectedOptionIds.Num();
	TestFalse(TEXT("invalid option rejects"),
		FGameXXKDialogueRules::Choose(*Asset, TEXT("missing"), Session, Output, &Error));
	TestEqual(TEXT("invalid option leaves node"), Session.CurrentNodeId, FName(TEXT("choice")));
	TestEqual(TEXT("invalid option leaves selections"), Session.SelectedOptionIds.Num(), SelectionCountBeforeInvalid);

	TestTrue(TEXT("right choice accepted"),
		FGameXXKDialogueRules::Choose(*Asset, TEXT("right"), Session, Output, &Error));
	TestEqual(TEXT("choice outcome returned"), Output.OutcomeId, FName(TEXT("Outcome.Test.Right")));
	TestEqual(TEXT("right branch reached"), Session.CurrentNodeId, FName(TEXT("right.line")));
	TestTrue(TEXT("selection recorded"), Session.SelectedOptionIds.Contains(TEXT("right")));

	FGameXXKDialogueOutput Resumed;
	TestTrue(TEXT("resume succeeds"), FGameXXKDialogueRules::Resume(*Asset, Session, Resumed, &Error));
	TestEqual(TEXT("resume stays on branch line"), Resumed.NodeId, FName(TEXT("right.line")));
	TestEqual(TEXT("resume does not duplicate history"), Session.History.Num(), 2);

	TestTrue(TEXT("branch line completes"),
		FGameXXKDialogueRules::CompletePresentedNode(*Asset, Session, Output, &Error));
	TestTrue(TEXT("end emitted"), Output.bEnded);
	TestEqual(TEXT("terminal outcome emitted"), Output.OutcomeId, FName(TEXT("Outcome.Test.RightDone")));
	TestFalse(TEXT("session ended"), Session.bActive);
	return true;
}

#endif
