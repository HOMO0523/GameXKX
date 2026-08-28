#include "Misc/AutomationTest.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "Narrative/GameXXKCharacterCatalog.h"
#include "Narrative/GameXXKNarrativeSequenceAsset.h"
#include "Narrative/GameXXKNarrativeSequenceRules.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKNarrativeSequenceRulesTestPrivate
{
	UGameXXKNarrativeSequenceAsset* MakeSequence()
	{
		UGameXXKNarrativeSequenceAsset* Asset = NewObject<UGameXXKNarrativeSequenceAsset>();
		Asset->SequenceId = TEXT("Sequence.Test.AcceptOrReject");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Test");
		Asset->EntryStepId = TEXT("dialogue");
		Asset->DefaultCharacterIdByRole.Add(TEXT("Hero"), TEXT("Character.Hero"));

		FGameXXKNarrativeSequenceStepDefinition Dialogue;
		Dialogue.StepId = TEXT("dialogue");
		Dialogue.Type = EGameXXKNarrativeStepType::Dialogue;
		Dialogue.DialogueId = TEXT("Dialogue.Test.Choice");
		Dialogue.NextStepId = TEXT("branch");
		Asset->Steps.Add(Dialogue);

		FGameXXKNarrativeSequenceStepDefinition Branch;
		Branch.StepId = TEXT("branch");
		Branch.Type = EGameXXKNarrativeStepType::BranchOnOutcome;
		Branch.OutcomeToStepId.Add(TEXT("Outcome.Accept"), TEXT("grant"));
		Branch.OutcomeToStepId.Add(TEXT("Outcome.Reject"), TEXT("end"));
		Asset->Steps.Add(Branch);

		FGameXXKNarrativeSequenceStepDefinition Grant;
		Grant.StepId = TEXT("grant");
		Grant.Type = EGameXXKNarrativeStepType::Command;
		Grant.Command.CommandId = TEXT("grant_once");
		Grant.Command.CommandType = TEXT("grantItem");
		Grant.Command.Arguments.Add(TEXT("itemId"), TEXT("Item.Test"));
		Grant.Command.Arguments.Add(TEXT("count"), TEXT("1"));
		Grant.NextStepId = TEXT("end");
		Asset->Steps.Add(Grant);

		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	FGameXXKNarrativeStartContext MakeStartContext()
	{
		FGameXXKNarrativeStartContext Context;
		Context.StoryId = TEXT("Story.Test");
		Context.StoryVersion = 2;
		Context.TaskId = TEXT("Task.Test");
		Context.StepId = TEXT("Step.Test");
		Context.StageContractId = TEXT("Stage.Test");
		Context.CharacterIdByRole.Add(TEXT("YueBai"), TEXT("Character.YueBai"));
		return Context;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterCatalogContractTest,
	"GameXXK.Narrative.CharacterCatalog.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterCatalogContractTest::RunTest(const FString& Parameters)
{
	UGameXXKCharacterCatalog* Catalog = NewObject<UGameXXKCharacterCatalog>();
	FGameXXKCharacterDefinition Hero;
	Hero.CharacterId = TEXT("Character.Hero");
	Hero.DisplayName = FText::FromString(TEXT("主角"));
	Hero.SupportedActionIds = {TEXT("Town.Idle"), TEXT("Town.Walk")};
	Hero.DefaultInteractionSequenceId = TEXT("Sequence.Hero.Default");
	Catalog->Characters.Add(Hero);

	FGameXXKCharacterDefinition YueBai;
	YueBai.CharacterId = TEXT("Character.YueBai");
	YueBai.DisplayName = FText::FromString(TEXT("月白"));
	YueBai.SupportedActionIds = {TEXT("Narrative.Appear"), TEXT("Narrative.Idle")};
	Catalog->Characters.Add(YueBai);

	TestNotNull(TEXT("hero resolves"), Catalog->FindCharacter(TEXT("Character.Hero")));
	TestNull(TEXT("missing character rejects"), Catalog->FindCharacter(TEXT("Character.Missing")));
	const UScriptStruct* DefinitionStruct = FGameXXKCharacterDefinition::StaticStruct();
	for (const FName ForbiddenProperty : {
		FName(TEXT("Transform")),
		FName(TEXT("Location")),
		FName(TEXT("MapPath")),
		FName(TEXT("BattlePosition"))})
	{
		TestNull(
			FString::Printf(TEXT("character catalog has no placement property %s"), *ForbiddenProperty.ToString()),
			DefinitionStruct->FindPropertyByName(ForbiddenProperty));
	}
#if WITH_EDITOR
	FDataValidationContext ValidationContext;
	TestEqual(TEXT("unique characters validate"),
		Catalog->IsDataValid(ValidationContext), EDataValidationResult::Valid);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeSequenceBranchAndIdempotencyTest,
	"GameXXK.Narrative.Sequence.BranchAndIdempotency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeSequenceBranchAndIdempotencyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeSequenceRulesTestPrivate;
	UGameXXKNarrativeSequenceAsset* Asset = MakeSequence();
#if WITH_EDITOR
	FDataValidationContext ValidationContext;
	TestEqual(TEXT("sequence validates"), Asset->IsDataValid(ValidationContext), EDataValidationResult::Valid);
#endif

	FGameXXKNarrativeSequenceSessionState Session;
	FGameXXKNarrativeRequest Request;
	FString Error;
	const FGameXXKNarrativeStartContext Context = MakeStartContext();
	TestTrue(FString::Printf(TEXT("sequence starts: %s"), *Error),
		FGameXXKNarrativeSequenceRules::Start(*Asset, Context, Session, Request, &Error));
	TestEqual(TEXT("dialogue requested"), Request.Type, EGameXXKNarrativeRequestType::Dialogue);
	TestEqual(TEXT("dialogue id emitted"), Request.DialogueId, FName(TEXT("Dialogue.Test.Choice")));
	TestEqual(TEXT("default hero role binds"),
		Session.CharacterIdByRole.FindRef(TEXT("Hero")), FName(TEXT("Character.Hero")));
	TestEqual(TEXT("context role override binds"),
		Session.CharacterIdByRole.FindRef(TEXT("YueBai")), FName(TEXT("Character.YueBai")));

	FGameXXKNarrativeRequest SecondStartRequest;
	TestFalse(TEXT("second active sequence rejects"),
		FGameXXKNarrativeSequenceRules::Start(*Asset, Context, Session, SecondStartRequest, &Error));
	TestEqual(TEXT("second start leaves dialogue boundary"),
		Session.CurrentSequenceStepId, FName(TEXT("dialogue")));

	TestTrue(TEXT("dialogue outcome resumes"),
		FGameXXKNarrativeSequenceRules::CompleteDialogue(
			*Asset, TEXT("Outcome.Accept"), Session, Request, &Error));
	TestEqual(TEXT("accept branch requests command"), Request.Type, EGameXXKNarrativeRequestType::Command);
	TestEqual(TEXT("grant command emitted"), Request.Command.CommandId, FName(TEXT("grant_once")));

	const FName CommandKey = FGameXXKNarrativeSequenceRules::MakeCommandKey(
		TEXT("Story.Test"), TEXT("Task.Test"), TEXT("Step.Test"), TEXT("grant_once"));
	TestEqual(TEXT("command key is fully namespaced"),
		CommandKey, FName(TEXT("Story.Test/Task.Test/Step.Test/grant_once")));
	TestTrue(TEXT("pending command reissues"),
		FGameXXKNarrativeSequenceRules::CompleteCommand(
			*Asset, EGameXXKNarrativeCommandStatus::Pending, Session, Request, &Error));
	TestEqual(TEXT("pending command stays at grant"),
		Session.CurrentSequenceStepId, FName(TEXT("grant")));
	TestFalse(TEXT("pending command is not committed"), Session.ExecutedCommandKeys.Contains(CommandKey));

	TestTrue(TEXT("completed command advances"),
		FGameXXKNarrativeSequenceRules::CompleteCommand(
			*Asset, EGameXXKNarrativeCommandStatus::Completed, Session, Request, &Error));
	TestEqual(TEXT("sequence ended"), Request.Type, EGameXXKNarrativeRequestType::Ended);
	TestTrue(TEXT("ended request marked"), Request.bEnded);
	TestFalse(TEXT("ended session inactive"), Session.bActive);
	TestTrue(TEXT("completed command key retained"), Session.ExecutedCommandKeys.Contains(CommandKey));

	TestTrue(TEXT("same sequence may start again"),
		FGameXXKNarrativeSequenceRules::Start(*Asset, Context, Session, Request, &Error));
	TestTrue(TEXT("same accepted outcome resumes"),
		FGameXXKNarrativeSequenceRules::CompleteDialogue(
			*Asset, TEXT("Outcome.Accept"), Session, Request, &Error));
	TestEqual(TEXT("already committed grant is skipped to end"),
		Request.Type, EGameXXKNarrativeRequestType::Ended);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeSequenceWaitAndFailureTest,
	"GameXXK.Narrative.Sequence.WaitAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeSequenceWaitAndFailureTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeSequenceRulesTestPrivate;
	UGameXXKNarrativeSequenceAsset* Asset = NewObject<UGameXXKNarrativeSequenceAsset>();
	Asset->SequenceId = TEXT("Sequence.Test.Wait");
	Asset->SequenceVersion = 1;
	Asset->StageContractId = TEXT("Stage.Test");
	Asset->EntryStepId = TEXT("wait");
	FGameXXKNarrativeSequenceStepDefinition Wait;
	Wait.StepId = TEXT("wait");
	Wait.Type = EGameXXKNarrativeStepType::Wait;
	Wait.WaitType = TEXT("seconds");
	Wait.WaitArguments.Add(TEXT("duration"), TEXT("0.5"));
	Wait.NextStepId = TEXT("required");
	Asset->Steps.Add(Wait);
	FGameXXKNarrativeSequenceStepDefinition Required;
	Required.StepId = TEXT("required");
	Required.Type = EGameXXKNarrativeStepType::Command;
	Required.Command.CommandId = TEXT("required_command");
	Required.Command.CommandType = TEXT("requiredType");
	Required.NextStepId = TEXT("end");
	Asset->Steps.Add(Required);
	FGameXXKNarrativeSequenceStepDefinition End;
	End.StepId = TEXT("end");
	End.Type = EGameXXKNarrativeStepType::End;
	Asset->Steps.Add(End);

	FGameXXKNarrativeSequenceSessionState Session;
	FGameXXKNarrativeRequest Request;
	FString Error;
	TestTrue(TEXT("wait sequence starts"),
		FGameXXKNarrativeSequenceRules::Start(*Asset, MakeStartContext(), Session, Request, &Error));
	TestEqual(TEXT("wait requested"), Request.Type, EGameXXKNarrativeRequestType::Wait);
	TestTrue(TEXT("wait completion advances"),
		FGameXXKNarrativeSequenceRules::CompleteWait(*Asset, Session, Request, &Error));
	TestEqual(TEXT("required command follows"), Request.Type, EGameXXKNarrativeRequestType::Command);
	TestFalse(TEXT("required failure pauses"),
		FGameXXKNarrativeSequenceRules::CompleteCommand(
			*Asset, EGameXXKNarrativeCommandStatus::Failed, Session, Request, &Error));
	TestTrue(TEXT("required failure leaves session active"), Session.bActive);
	TestEqual(TEXT("required failure stays at command"),
		Session.CurrentSequenceStepId, FName(TEXT("required")));
	TestFalse(TEXT("required failure stores a reason"), Session.PauseReason.IsEmpty());
	return true;
}

#endif
