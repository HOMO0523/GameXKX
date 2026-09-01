#include "Misc/AutomationTest.h"

#include "GameXXKMVPRules.h"
#include "Narrative/GameXXKNarrativeCommandExecutor.h"
#include "Narrative/GameXXKNarrativeCoordinator.h"
#include "Narrative/GameXXKNarrativeSequenceAsset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKNarrativeCoordinatorTestPrivate
{
	class FTestExecutor final : public IGameXXKNarrativeCommandExecutor
	{
	public:
		EGameXXKNarrativeCommandStatus NextStatus = EGameXXKNarrativeCommandStatus::Completed;
		int32 GoldDelta = 0;
		int32 ExecuteCount = 0;
		int32 CancelCount = 0;

		virtual bool Supports(const FName CommandType) const override
		{
			return CommandType == TEXT("testCommand");
		}

		virtual FGameXXKNarrativeCommandResult Execute(
			const FGameXXKNarrativeCommandDefinition& Command,
			FGameXXKRuntimeState& InOutCandidateState) override
		{
			++ExecuteCount;
			InOutCandidateState.PlayerGold += GoldDelta;
			FGameXXKNarrativeCommandResult Result;
			Result.Status = NextStatus;
			Result.Error = NextStatus == EGameXXKNarrativeCommandStatus::Failed
				? TEXT("test failure")
				: FString();
			return Result;
		}

		virtual void CancelPending() override
		{
			++CancelCount;
		}
	};

	FGameXXKNarrativeStartContext MakeContext()
	{
		FGameXXKNarrativeStartContext Context;
		Context.StoryId = TEXT("Story.Test");
		Context.TaskId = TEXT("Task.Test");
		Context.StepId = TEXT("Step.Test");
		Context.StageContractId = TEXT("Stage.Test");
		return Context;
	}

	UGameXXKNarrativeSequenceAsset* MakeCommandSequence(bool bOptional = false)
	{
		UGameXXKNarrativeSequenceAsset* Asset = NewObject<UGameXXKNarrativeSequenceAsset>();
		Asset->SequenceId = bOptional ? TEXT("Sequence.Test.Optional") : TEXT("Sequence.Test.Command");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Test");
		Asset->EntryStepId = TEXT("command");
		FGameXXKNarrativeSequenceStepDefinition Command;
		Command.StepId = TEXT("command");
		Command.Type = EGameXXKNarrativeStepType::Command;
		Command.Command.CommandId = TEXT("command_once");
		Command.Command.CommandType = bOptional ? TEXT("missingOptional") : TEXT("testCommand");
		Command.Command.bOptional = bOptional;
		Command.NextStepId = TEXT("end");
		Asset->Steps.Add(Command);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeDialogueSequence()
	{
		UGameXXKNarrativeSequenceAsset* Asset = NewObject<UGameXXKNarrativeSequenceAsset>();
		Asset->SequenceId = TEXT("Sequence.Test.Dialogue");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Test");
		Asset->EntryStepId = TEXT("dialogue");
		FGameXXKNarrativeSequenceStepDefinition Dialogue;
		Dialogue.StepId = TEXT("dialogue");
		Dialogue.Type = EGameXXKNarrativeStepType::Dialogue;
		Dialogue.DialogueId = TEXT("Dialogue.Test");
		Dialogue.NextStepId = TEXT("branch");
		Asset->Steps.Add(Dialogue);
		FGameXXKNarrativeSequenceStepDefinition Branch;
		Branch.StepId = TEXT("branch");
		Branch.Type = EGameXXKNarrativeStepType::BranchOnOutcome;
		Branch.OutcomeToStepId.Add(TEXT("Outcome.Done"), TEXT("end"));
		Asset->Steps.Add(Branch);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeWaitSequence()
	{
		UGameXXKNarrativeSequenceAsset* Asset = NewObject<UGameXXKNarrativeSequenceAsset>();
		Asset->SequenceId = TEXT("Sequence.Test.WaitCoordinator");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Test");
		Asset->EntryStepId = TEXT("wait");
		FGameXXKNarrativeSequenceStepDefinition Wait;
		Wait.StepId = TEXT("wait");
		Wait.Type = EGameXXKNarrativeStepType::Wait;
		Wait.WaitType = TEXT("seconds");
		Wait.NextStepId = TEXT("end");
		Asset->Steps.Add(Wait);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorAtomicDispatchTest,
	"GameXXK.Narrative.Coordinator.AtomicDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorAtomicDispatchTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	const int32 InitialGold = RuntimeState.PlayerGold;
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	TSharedRef<FTestExecutor> Executor = MakeShared<FTestExecutor>();
	Executor->GoldDelta = 5;
	TestTrue(TEXT("executor registers"), Coordinator->RegisterExecutor(TEXT("testCommand"), Executor));
	TestFalse(TEXT("duplicate command type rejects"),
		Coordinator->RegisterExecutor(TEXT("testCommand"), MakeShared<FTestExecutor>()));

	FString Error;
	TestTrue(FString::Printf(TEXT("synchronous command sequence starts: %s"), *Error),
		Coordinator->StartSequence(*MakeCommandSequence(), MakeContext(), &Error));
	TestEqual(TEXT("runtime mutation committed"), RuntimeState.PlayerGold, InitialGold + 5);
	TestEqual(TEXT("executor ran once"), Executor->ExecuteCount, 1);
	TestFalse(TEXT("ended sequence releases input token"), Coordinator->IsInputTokenHeld());
	TestFalse(TEXT("ended session inactive"), Session.bActive);
	TestTrue(TEXT("command key committed before end"),
		Session.ExecutedCommandKeys.Contains(TEXT("Story.Test/Task.Test/Step.Test/command_once")));

	FGameXXKRuntimeState PendingRuntime = UGameXXKMVPRules::CreateNewGame();
	const int32 PendingInitialGold = PendingRuntime.PlayerGold;
	FGameXXKNarrativeSequenceSessionState PendingSession;
	UGameXXKNarrativeCoordinator* PendingCoordinator = NewObject<UGameXXKNarrativeCoordinator>();
	PendingCoordinator->BindState(PendingRuntime, PendingSession);
	TSharedRef<FTestExecutor> PendingExecutor = MakeShared<FTestExecutor>();
	PendingExecutor->GoldDelta = 999;
	PendingExecutor->NextStatus = EGameXXKNarrativeCommandStatus::Pending;
	PendingCoordinator->RegisterExecutor(TEXT("testCommand"), PendingExecutor);
	TestTrue(TEXT("pending sequence starts"),
		PendingCoordinator->StartSequence(*MakeCommandSequence(), MakeContext(), &Error));
	TestEqual(TEXT("pending candidate mutation is discarded"), PendingRuntime.PlayerGold, PendingInitialGold);
	TestTrue(TEXT("pending sequence keeps input"), PendingCoordinator->IsInputTokenHeld());
	TestTrue(TEXT("pending completion advances with a fresh candidate"),
		PendingCoordinator->CompletePendingCommand(EGameXXKNarrativeCommandStatus::Completed, &Error));
	TestEqual(TEXT("pending world completion does not replay stale mutation"),
		PendingRuntime.PlayerGold, PendingInitialGold);
	TestFalse(TEXT("pending completion releases input at end"), PendingCoordinator->IsInputTokenHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorDialogueAndPauseTest,
	"GameXXK.Narrative.Coordinator.DialoguePauseAndSingleSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorDialogueAndPauseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	FName RequestedDialogue;
	FGameXXKNarrativeDialogueCompleted Completion;
	Coordinator->SetDialogueStartDelegate(FGameXXKNarrativeDialogueStartRequest::CreateLambda(
		[&](const FName DialogueId, FGameXXKNarrativeDialogueCompleted Callback)
		{
			RequestedDialogue = DialogueId;
			Completion = MoveTemp(Callback);
		}));

	FString Error;
	TestTrue(TEXT("dialogue sequence starts"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("dialogue host receives id"), RequestedDialogue, FName(TEXT("Dialogue.Test")));
	TestTrue(TEXT("dialogue keeps input token"), Coordinator->IsInputTokenHeld());
	TestFalse(TEXT("second sequence rejects while active"),
		Coordinator->StartSequence(*MakeWaitSequence(), MakeContext(), &Error));
	TestTrue(TEXT("dialogue completion callback bound"), Completion.IsBound());
	Completion.Execute(TEXT("Outcome.Done"));
	TestFalse(TEXT("dialogue completion ends sequence"), Session.bActive);
	TestFalse(TEXT("dialogue completion releases input"), Coordinator->IsInputTokenHeld());

	TestTrue(TEXT("wait sequence starts"),
		Coordinator->StartSequence(*MakeWaitSequence(), MakeContext(), &Error));
	Coordinator->PauseAndRelease();
	TestTrue(TEXT("pause preserves active session"), Session.bActive);
	TestFalse(TEXT("pause releases input"), Coordinator->IsInputTokenHeld());
	TestTrue(TEXT("resume reacquires input"), Coordinator->Resume(&Error));
	TestTrue(TEXT("resumed wait owns input"), Coordinator->IsInputTokenHeld());
	TestTrue(TEXT("wait completion ends"), Coordinator->CompletePendingWait(&Error));
	TestFalse(TEXT("wait completion releases input"), Coordinator->IsInputTokenHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorFailureTest,
	"GameXXK.Narrative.Coordinator.RequiredAndOptionalFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorFailureTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	FString Error;
	TestFalse(TEXT("unknown required command fails start"),
		Coordinator->StartSequence(*MakeCommandSequence(), MakeContext(), &Error));
	TestTrue(TEXT("required failure keeps replayable session"), Session.bActive);
	TestFalse(TEXT("required failure records reason"), Session.PauseReason.IsEmpty());
	TestFalse(TEXT("required failure releases input"), Coordinator->IsInputTokenHeld());

	Coordinator->CancelForMapTravel();
	Session = FGameXXKNarrativeSequenceSessionState();
	TestTrue(TEXT("unknown optional command is skipped"),
		Coordinator->StartSequence(*MakeCommandSequence(true), MakeContext(), &Error));
	TestFalse(TEXT("optional skip reaches end"), Session.bActive);
	TestFalse(TEXT("optional skip releases input"), Coordinator->IsInputTokenHeld());
	return true;
}

#endif
