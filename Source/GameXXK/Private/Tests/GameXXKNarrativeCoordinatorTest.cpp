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
	if (Completion.IsBound())
	{
		Completion.Execute(TEXT("Outcome.Done"));
	}
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorExplicitDialogueHostTest,
	"GameXXK.Narrative.Coordinator.ExplicitDesktopAndLegacyDialogueHosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorExplicitDialogueHostTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	int32 DesktopStartCount = 0;
	int32 LegacyStartCount = 0;
	FGameXXKNarrativeDialogueCompleted DesktopCompletion;
	FGameXXKNarrativeDialogueCompleted LegacyCompletion;
	Coordinator->SetDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost::Desktop2D,
		FGameXXKNarrativeDialogueStartRequest::CreateLambda(
			[&](FName, FGameXXKNarrativeDialogueCompleted Completion)
			{
				++DesktopStartCount;
				DesktopCompletion = MoveTemp(Completion);
			}));
	Coordinator->SetDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost::LegacyNpc3D,
		FGameXXKNarrativeDialogueStartRequest::CreateLambda(
			[&](FName, FGameXXKNarrativeDialogueCompleted Completion)
			{
				++LegacyStartCount;
				LegacyCompletion = MoveTemp(Completion);
			}));

	FString Error;
	TestTrue(TEXT("desktop host selection succeeds while idle"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestEqual(TEXT("coordinator records desktop host explicitly"),
		Coordinator->GetDialogueHost(), EGameXXKNarrativeDialogueHost::Desktop2D);
	TestTrue(TEXT("desktop dialogue sequence starts"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("desktop host receives the request"), DesktopStartCount, 1);
	TestEqual(TEXT("legacy host is never an accidental fallback"), LegacyStartCount, 0);
	TestTrue(TEXT("same-host re-entry is idempotent while active"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestFalse(TEXT("an active desktop sequence cannot switch to legacy"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, &Error));
	TestTrue(TEXT("desktop completion is bound"), DesktopCompletion.IsBound());
	if (DesktopCompletion.IsBound())
	{
		DesktopCompletion.Execute(TEXT("Outcome.Done"));
	}
	TestFalse(TEXT("desktop dialogue completion ends the sequence"), Session.bActive);

	TestTrue(TEXT("legacy host selection succeeds after desktop completion"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, &Error));
	TestTrue(TEXT("legacy dialogue sequence starts"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("legacy host receives its explicit request"), LegacyStartCount, 1);
	TestEqual(TEXT("desktop host does not receive legacy dialogue"), DesktopStartCount, 1);
	TestTrue(TEXT("legacy completion is bound"), LegacyCompletion.IsBound());
	if (LegacyCompletion.IsBound())
	{
		LegacyCompletion.Execute(TEXT("Outcome.Done"));
	}
	TestFalse(TEXT("legacy dialogue completion ends the sequence"), Session.bActive);

	Coordinator->ClearDialogueStartDelegate(EGameXXKNarrativeDialogueHost::Desktop2D);
	TestTrue(TEXT("clearing an inactive host is teardown-safe"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestFalse(TEXT("desktop selection never falls back after its delegate is cleared"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestTrue(TEXT("missing host preserves a replayable session"), Session.bActive);
	TestFalse(TEXT("missing host releases input"), Coordinator->IsInputTokenHeld());
	Coordinator->PauseAndRelease();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorDialogueHostReentryTest,
	"GameXXK.Narrative.Coordinator.DialogueHostNullTeardownAndSynchronousReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorDialogueHostReentryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	FString Error;
	TestTrue(TEXT("an explicitly null desktop host may be selected"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestFalse(TEXT("null desktop presenter fails without crashing"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestTrue(TEXT("null desktop presenter records a reason"), !Session.PauseReason.IsEmpty());
	TestFalse(TEXT("null desktop presenter releases input"), Coordinator->IsInputTokenHeld());

	Session = FGameXXKNarrativeSequenceSessionState();
	int32 SynchronousCompletionCount = 0;
	Coordinator->SetDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost::Desktop2D,
		FGameXXKNarrativeDialogueStartRequest::CreateLambda(
			[&](FName, FGameXXKNarrativeDialogueCompleted Completion)
			{
				++SynchronousCompletionCount;
				if (Completion.IsBound())
				{
					Completion.Execute(TEXT("Outcome.Done"));
				}
			}));
	TestTrue(TEXT("synchronous presenter completion is re-entry-safe"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("synchronous presenter completes exactly once"), SynchronousCompletionCount, 1);
	TestFalse(TEXT("synchronous completion leaves no active sequence"), Session.bActive);
	TestFalse(TEXT("synchronous completion releases input"), Coordinator->IsInputTokenHeld());
	Coordinator->ClearDialogueStartDelegates();
	TestEqual(TEXT("teardown clears host selection"),
		Coordinator->GetDialogueHost(), EGameXXKNarrativeDialogueHost::None);
	Coordinator->PauseAndRelease();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeCoordinatorActiveHostAffinityTest,
	"GameXXK.Narrative.Coordinator.ActiveHostAffinitySurvivesPresenterRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeCoordinatorActiveHostAffinityTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeCoordinatorTestPrivate;
	FGameXXKRuntimeState RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKNarrativeSequenceSessionState Session;
	UGameXXKNarrativeCoordinator* Coordinator = NewObject<UGameXXKNarrativeCoordinator>();
	Coordinator->BindState(RuntimeState, Session);
	FGameXXKNarrativeDialogueCompleted DesktopCompletion;
	FGameXXKNarrativeDialogueCompleted LegacyCompletion;
	auto BindDesktop = [&]()
	{
		Coordinator->SetDialogueStartDelegate(
			EGameXXKNarrativeDialogueHost::Desktop2D,
			FGameXXKNarrativeDialogueStartRequest::CreateLambda(
				[&](FName, FGameXXKNarrativeDialogueCompleted Completion)
				{
					DesktopCompletion = MoveTemp(Completion);
				}));
	};
	auto BindLegacy = [&]()
	{
		Coordinator->SetDialogueStartDelegate(
			EGameXXKNarrativeDialogueHost::LegacyNpc3D,
			FGameXXKNarrativeDialogueStartRequest::CreateLambda(
				[&](FName, FGameXXKNarrativeDialogueCompleted Completion)
				{
					LegacyCompletion = MoveTemp(Completion);
				}));
	};

	FString Error;
	BindDesktop();
	TestTrue(TEXT("desktop affinity fixture selects desktop"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestTrue(TEXT("desktop affinity fixture starts"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("active desktop sequence owns desktop affinity"),
		Coordinator->GetActiveDialogueHostAffinity(),
		EGameXXKNarrativeDialogueHost::Desktop2D);
	Coordinator->PauseAndRelease();
	Coordinator->ClearDialogueStartDelegates();
	TestEqual(TEXT("desktop presenter release clears only current binding"),
		Coordinator->GetDialogueHost(), EGameXXKNarrativeDialogueHost::None);
	TestEqual(TEXT("desktop presenter release retains active affinity"),
		Coordinator->GetActiveDialogueHostAffinity(),
		EGameXXKNarrativeDialogueHost::Desktop2D);
	TestFalse(TEXT("released desktop session rejects legacy host"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, &Error));
	BindDesktop();
	TestTrue(TEXT("released desktop session accepts same-host rebind"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	TestTrue(TEXT("released desktop session resumes on desktop"), Coordinator->Resume(&Error));
	TestTrue(TEXT("desktop resume captures completion"), DesktopCompletion.IsBound());
	if (DesktopCompletion.IsBound())
	{
		DesktopCompletion.Execute(TEXT("Outcome.Done"));
	}
	TestFalse(TEXT("desktop resume completes sequence"), Session.bActive);
	TestEqual(TEXT("desktop completion clears active affinity"),
		Coordinator->GetActiveDialogueHostAffinity(),
		EGameXXKNarrativeDialogueHost::None);

	Coordinator->ClearDialogueStartDelegates();
	BindLegacy();
	TestTrue(TEXT("legacy affinity fixture selects legacy"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, &Error));
	TestTrue(TEXT("legacy affinity fixture starts"),
		Coordinator->StartSequence(*MakeDialogueSequence(), MakeContext(), &Error));
	TestEqual(TEXT("active legacy sequence owns legacy affinity"),
		Coordinator->GetActiveDialogueHostAffinity(),
		EGameXXKNarrativeDialogueHost::LegacyNpc3D);
	Coordinator->PauseAndRelease();
	Coordinator->ClearDialogueStartDelegates();
	TestFalse(TEXT("released legacy session rejects desktop host"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::Desktop2D, &Error));
	BindLegacy();
	TestTrue(TEXT("released legacy session accepts same-host rebind"),
		Coordinator->SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, &Error));
	TestTrue(TEXT("released legacy session resumes on legacy"), Coordinator->Resume(&Error));
	TestTrue(TEXT("legacy resume captures completion"), LegacyCompletion.IsBound());
	if (LegacyCompletion.IsBound())
	{
		LegacyCompletion.Execute(TEXT("Outcome.Done"));
	}
	TestFalse(TEXT("legacy resume completes sequence"), Session.bActive);
	TestEqual(TEXT("legacy completion clears active affinity"),
		Coordinator->GetActiveDialogueHostAffinity(),
		EGameXXKNarrativeDialogueHost::None);
	return true;
}

#endif
