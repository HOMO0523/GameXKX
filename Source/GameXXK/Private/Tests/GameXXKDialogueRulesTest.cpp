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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueConditionsAndOutcomesTest,
	"GameXXK.Dialogue.Core.ConditionsAndOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueConditionsAndOutcomesTest::RunTest(const FString& Parameters)
{
	FGameXXKDialogueConditionContext Context;
	Context.Flags.Add(TEXT("Flag.IntroSeen"));
	Context.ItemCounts.Add(TEXT("Item.Test.Map"), 1);
	Context.Gold = 5000;
	Context.UnlockedCompanionIds.Add(TEXT("Companion.Blade"));
	Context.SelectedOptionIds.Add(TEXT("prior_choice"));
	Context.SeenNodeIds.Add(TEXT("prior_node"));
	Context.TutorialState = EGameXXKTutorialQuestState::Active;
	Context.TaskStateValues.Add(TEXT("Task.Test"), 2);

	TMap<FName, FString> AllConditions;
	AllConditions.Add(TEXT("flag"), TEXT("Flag.IntroSeen"));
	AllConditions.Add(TEXT("tutorialState"), TEXT("Active"));
	AllConditions.Add(TEXT("taskState"), TEXT("Task.Test:2"));
	AllConditions.Add(TEXT("itemAtLeast"), TEXT("Item.Test.Map:1"));
	AllConditions.Add(TEXT("goldAtLeast"), TEXT("5000"));
	AllConditions.Add(TEXT("companionUnlocked"), TEXT("Companion.Blade"));
	AllConditions.Add(TEXT("optionSelected"), TEXT("prior_choice"));
	AllConditions.Add(TEXT("nodeSeen"), TEXT("prior_node"));

	FString Error;
	TestTrue(FString::Printf(TEXT("all registered conditions pass: %s"), *Error),
		FGameXXKDialogueRules::EvaluateConditions(AllConditions, Context, &Error));
	TestTrue(TEXT("successful evaluation clears error"), Error.IsEmpty());

	Context.Gold = 4999;
	TestFalse(TEXT("5000-gold condition is unmet"),
		FGameXXKDialogueRules::EvaluateConditions(AllConditions, Context, &Error));
	TestTrue(TEXT("unmet valid condition is not a schema error"), Error.IsEmpty());

	TMap<FName, FString> UnknownCondition;
	UnknownCondition.Add(TEXT("unknownCondition"), TEXT("anything"));
	TestFalse(TEXT("unknown condition fails closed"),
		FGameXXKDialogueRules::EvaluateConditions(UnknownCondition, Context, &Error));
	TestTrue(TEXT("unknown condition reports an error"), Error.Contains(TEXT("unknownCondition")));

	UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
	Asset->DialogueId = TEXT("Dialogue.Test.Conditions");
	Asset->DialogueVersion = 1;
	Asset->EntryNodeId = TEXT("start");

	FGameXXKDialogueNodeDefinition Start;
	Start.NodeId = TEXT("start");
	Start.Type = EGameXXKDialogueNodeType::Line;
	Start.NextNodeId = TEXT("choice");
	Asset->Nodes.Add(Start);

	FGameXXKDialogueNodeDefinition Choice;
	Choice.NodeId = TEXT("choice");
	Choice.Type = EGameXXKDialogueNodeType::Choice;

	FGameXXKDialogueOptionDefinition Available;
	Available.OptionId = TEXT("available");
	Available.Text = FText::FromString(TEXT("可选"));
	Available.OutcomeId = TEXT("Outcome.Test.Available");
	Available.NextNodeId = TEXT("available.line");
	Choice.Options.Add(Available);

	FGameXXKDialogueOptionDefinition Hidden;
	Hidden.OptionId = TEXT("hidden");
	Hidden.Text = FText::FromString(TEXT("隐藏"));
	Hidden.OutcomeId = TEXT("Outcome.Test.Hidden");
	Hidden.NextNodeId = TEXT("hidden.line");
	Hidden.Conditions.Add(TEXT("goldAtLeast"), TEXT("5000"));
	Choice.Options.Add(Hidden);

	FGameXXKDialogueOptionDefinition Disabled = Hidden;
	Disabled.OptionId = TEXT("disabled");
	Disabled.Text = FText::FromString(TEXT("置灰"));
	Disabled.OutcomeId = TEXT("Outcome.Test.Disabled");
	Disabled.NextNodeId = TEXT("disabled.line");
	Disabled.DisabledReason = FText::FromString(TEXT("金币不足"));
	Choice.Options.Add(Disabled);
	Asset->Nodes.Add(Choice);

	for (const FName LineId : {FName(TEXT("available.line")), FName(TEXT("hidden.line")), FName(TEXT("disabled.line"))})
	{
		FGameXXKDialogueNodeDefinition Line;
		Line.NodeId = LineId;
		Line.Type = EGameXXKDialogueNodeType::Line;
		Line.NextNodeId = FName(*(LineId.ToString() + TEXT(".end")));
		Asset->Nodes.Add(Line);

		FGameXXKDialogueNodeDefinition End;
		End.NodeId = Line.NextNodeId;
		End.Type = EGameXXKDialogueNodeType::End;
		End.EndOutcomeId = FName(*(FString(TEXT("Outcome.Test.Done.")) + LineId.ToString()));
		Asset->Nodes.Add(End);
	}

	FDataValidationContext ValidContext;
	TestEqual(TEXT("unique outcomes validate"), Asset->IsDataValid(ValidContext), EDataValidationResult::Valid);

	FGameXXKDialogueStartContext StartContext;
	StartContext.StoryId = TEXT("Story.Test");
	StartContext.TaskId = TEXT("Task.Test");
	StartContext.StepId = TEXT("Step.Test");
	StartContext.SequenceId = TEXT("Sequence.Test");
	StartContext.StageContractId = TEXT("Stage.Test");

	FGameXXKDialogueSessionState Session;
	FGameXXKDialogueOutput Output;
	Context.Gold = 4999;
	TestTrue(TEXT("conditional dialogue starts"),
		FGameXXKDialogueRules::Start(*Asset, StartContext, Session, Output, &Error, &Context));
	TestTrue(TEXT("conditional choice reached"),
		FGameXXKDialogueRules::CompletePresentedNode(*Asset, Session, Output, &Error, &Context));
	TestEqual(TEXT("hidden option omitted and disabled retained"), Output.Options.Num(), 2);
	const FGameXXKDialogueVisibleOption* DisabledView = Output.Options.FindByPredicate(
		[](const FGameXXKDialogueVisibleOption& Option)
		{
			return Option.OptionId == TEXT("disabled");
		});
	TestTrue(TEXT("disabled option is visible but disabled"),
		DisabledView && !DisabledView->bEnabled && DisabledView->DisabledReason.ToString() == TEXT("金币不足"));

	TestFalse(TEXT("disabled option cannot be chosen"),
		FGameXXKDialogueRules::Choose(*Asset, TEXT("disabled"), Session, Output, &Error, &Context));
	TestEqual(TEXT("failed choice leaves node"), Session.CurrentNodeId, FName(TEXT("choice")));

	TestTrue(TEXT("available choice succeeds"),
		FGameXXKDialogueRules::Choose(*Asset, TEXT("available"), Session, Output, &Error, &Context));
	TestEqual(TEXT("choice outcome returned"), Output.OutcomeId, FName(TEXT("Outcome.Test.Available")));

	Asset->Nodes[1].Options[1].OutcomeId = Asset->Nodes[1].Options[0].OutcomeId;
	FDataValidationContext DuplicateOutcomeContext;
	TestEqual(TEXT("duplicate outcomes reject"),
		Asset->IsDataValid(DuplicateOutcomeContext), EDataValidationResult::Invalid);
	return true;
}

#endif
