#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteSettlementRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool StartDefaultOrderedFormationRoute(FGameXXKRuntimeState& OutState)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeState();
		return UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}

	int32 FindQuestNpcSlot(const FGameXXKOrderedPartyFormation& Formation)
	{
		return Formation.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
		});
	}

	FName FindStableUndeployedCompanion(
		const FGameXXKRuntimeState& State,
		const FGameXXKOrderedPartyFormation& Formation)
	{
		TArray<FName> OwnedIds;
		for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (!Companion.InstanceId.IsNone()
				&& !Formation.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
				{
					return Ref.MemberId == Companion.InstanceId;
				}))
			{
				OwnedIds.AddUnique(Companion.InstanceId);
			}
		}
		OwnedIds.Sort(FNameLexicalLess());
		return OwnedIds.IsEmpty() ? NAME_None : OwnedIds[0];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementFormationTerminalOutcomesTest,
	"GameXXK.Route.Settlement.FormationRepair.TerminalOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementFormationTerminalOutcomesTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Abandoned,
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Cleared})
	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("default StartGame state enters a legal route"), StartDefaultOrderedFormationRoute(State)))
		{
			return false;
		}
		const FGameXXKOrderedPartyFormation FormationBefore = State.CardRun.OrderedFormation;
		const int32 QuestNpcSlot = FindQuestNpcSlot(FormationBefore);
		if (!TestTrue(TEXT("default route deploys one task NPC"), QuestNpcSlot != INDEX_NONE))
		{
			return false;
		}
		const FName RemovedNpcId = FormationBefore.Members[QuestNpcSlot].MemberId;
		const FName ExpectedReplacementId = FindStableUndeployedCompanion(State, FormationBefore);
		if (!TestFalse(TEXT("default six-companion roster has a stable unused replacement"), ExpectedReplacementId.IsNone()))
		{
			return false;
		}
		const FGameXXKTrainingProgress TrainingBefore = State.Training;
		const FGameXXKCompanionRosterState RosterBefore = State.CardRun.CompanionRoster;

		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
		FGameXXKRouteSettlementReceipt Receipt;
		FString Error;
		if (!TestTrue(
			FString::Printf(TEXT("terminal receipt previews for outcome %d: %s"), static_cast<int32>(Outcome), *Error),
			FGameXXKRouteSettlementRules::Preview(State, Outcome, Receipt, &Error)))
		{
			return false;
		}
		State.CardRun.PendingSettlement = Receipt;
		if (!TestTrue(
			FString::Printf(TEXT("terminal receipt applies for outcome %d: %s"), static_cast<int32>(Outcome), *Error),
			FGameXXKRouteSettlementRules::Apply(State, Receipt, &Error)))
		{
			return false;
		}

		TestEqual(TEXT("settlement preserves exact party size"), State.CardRun.OrderedFormation.Members.Num(), 3);
		TestEqual(
			TEXT("settlement replaces the retired NPC in its exact slot"),
			State.CardRun.OrderedFormation.Members[QuestNpcSlot].MemberId,
			ExpectedReplacementId);
		TestEqual(
			TEXT("replacement slot becomes a permanent companion"),
			State.CardRun.OrderedFormation.Members[QuestNpcSlot].Kind,
			EGameXXKPartyMemberKind::PermanentCompanion);
		for (int32 SlotIndex = 0; SlotIndex < FormationBefore.Members.Num(); ++SlotIndex)
		{
			if (SlotIndex != QuestNpcSlot)
			{
				TestEqual(
					FString::Printf(TEXT("slot %d kind is preserved"), SlotIndex),
					State.CardRun.OrderedFormation.Members[SlotIndex].Kind,
					FormationBefore.Members[SlotIndex].Kind);
				TestEqual(
					FString::Printf(TEXT("slot %d identity is preserved"), SlotIndex),
					State.CardRun.OrderedFormation.Members[SlotIndex].MemberId,
					FormationBefore.Members[SlotIndex].MemberId);
			}
		}
		TestFalse(
			TEXT("settled formation contains no stale task NPC reference"),
			State.CardRun.OrderedFormation.Members.ContainsByPredicate([RemovedNpcId](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc || Ref.MemberId == RemovedNpcId;
			}));
		Error.Reset();
		TestTrue(
			FString::Printf(TEXT("settled formation validates for outcome %d: %s"), static_cast<int32>(Outcome), *Error),
			FGameXXKPartyFormationRules::Validate(State, State.CardRun.OrderedFormation, &Error));
		Error.Reset();
		TestTrue(
			FString::Printf(TEXT("settled compatibility projection validates for outcome %d: %s"), static_cast<int32>(Outcome), *Error),
			FGameXXKPartyFormationRules::ValidateCompatibilityProjection(State, &Error));
		TestTrue(
			TEXT("settlement preserves Training bit-identically"),
			FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(&State.Training, &TrainingBefore, PPF_None));
		TestTrue(
			TEXT("settlement preserves the owned permanent roster bit-identically"),
			FGameXXKCompanionRosterState::StaticStruct()->CompareScriptStruct(
				&State.CardRun.CompanionRoster,
				&RosterBefore,
				PPF_None));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKUnavailableQuestNpcFormationRepairRulesTest,
	"GameXXK.PartyFormation.UnavailableQuestNpcRepair.Atomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKUnavailableQuestNpcFormationRepairRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("pure repair fixture enters a legal route"), StartDefaultOrderedFormationRoute(State)))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation FormationBeforeClear = State.CardRun.OrderedFormation;
	const int32 QuestNpcSlot = FindQuestNpcSlot(FormationBeforeClear);
	if (!TestTrue(TEXT("pure repair fixture has a task-NPC slot"), QuestNpcSlot != INDEX_NONE))
	{
		return false;
	}
	const FName ExpectedReplacementId = FindStableUndeployedCompanion(State, FormationBeforeClear);
	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(State);
	const FGameXXKRuntimeState StateBeforePureCall = State;

	FGameXXKOrderedPartyFormation Repaired;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("pure repair replaces an unavailable known task NPC: %s"), *Error),
		FGameXXKPartyFormationRules::RepairUnavailableQuestNpcSlotsPreservingOrder(State, Repaired, &Error));
	TestTrue(
		TEXT("pure repair does not mutate its source runtime state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &StateBeforePureCall, PPF_None));
	TestEqual(TEXT("pure repair uses the lexical undeployed companion"), Repaired.Members[QuestNpcSlot].MemberId, ExpectedReplacementId);
	for (int32 SlotIndex = 0; SlotIndex < FormationBeforeClear.Members.Num(); ++SlotIndex)
	{
		if (SlotIndex != QuestNpcSlot)
		{
			TestEqual(TEXT("pure repair preserves every non-NPC kind"), Repaired.Members[SlotIndex].Kind, FormationBeforeClear.Members[SlotIndex].Kind);
			TestEqual(TEXT("pure repair preserves every non-NPC identity"), Repaired.Members[SlotIndex].MemberId, FormationBeforeClear.Members[SlotIndex].MemberId);
		}
	}

	const auto TestRejectedState = [this](const TCHAR* Label, const FGameXXKRuntimeState& RejectedState)
	{
		FGameXXKOrderedPartyFormation Output = RejectedState.CardRun.OrderedFormation;
		const FGameXXKOrderedPartyFormation OutputBefore = Output;
		FString RejectedError;
		TestFalse(
			FString::Printf(TEXT("%s is not silently repaired"), Label),
			FGameXXKPartyFormationRules::RepairUnavailableQuestNpcSlotsPreservingOrder(
				RejectedState,
				Output,
				&RejectedError));
		TestFalse(FString::Printf(TEXT("%s returns a concrete error"), Label), RejectedError.IsEmpty());
		TestTrue(
			FString::Printf(TEXT("%s leaves output formation atomic"), Label),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(&Output, &OutputBefore, PPF_None));
	};

	FGameXXKRuntimeState UnknownNpcState = State;
	UnknownNpcState.CardRun.OrderedFormation.Members[QuestNpcSlot].MemberId = TEXT("Npc.Unknown.Corrupt");
	TestRejectedState(TEXT("unknown current-version task NPC"), UnknownNpcState);

	FGameXXKRuntimeState UnknownHeroState = State;
	const int32 HeroSlot = UnknownHeroState.CardRun.OrderedFormation.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::Hero;
	});
	UnknownHeroState.CardRun.OrderedFormation.Members[HeroSlot].MemberId = TEXT("Hero.Unknown.Corrupt");
	TestRejectedState(TEXT("unknown current-version hero"), UnknownHeroState);

	FGameXXKRuntimeState UnknownCompanionState = State;
	const int32 CompanionSlot = UnknownCompanionState.CardRun.OrderedFormation.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
	});
	UnknownCompanionState.CardRun.OrderedFormation.Members[CompanionSlot].MemberId = TEXT("Companion.Unknown.Corrupt");
	TestRejectedState(TEXT("unknown current-version companion"), UnknownCompanionState);

	FGameXXKRuntimeState MismatchedAvailabilityState = State;
	MismatchedAvailabilityState.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.YueBai");
	TestRejectedState(TEXT("mismatched nonempty task-NPC availability"), MismatchedAvailabilityState);

	FGameXXKRuntimeState MismatchedCompanionProjectionState = State;
	MismatchedCompanionProjectionState.CardRun.PartySelection.ActivePermanentCompanionInstanceId = ExpectedReplacementId;
	TestRejectedState(TEXT("unrelated active-companion compatibility corruption"), MismatchedCompanionProjectionState);

	FGameXXKRuntimeState NoReplacementState = State;
	NoReplacementState.CardRun.CompanionRoster.PermanentCompanions.RemoveAll(
		[&FormationBeforeClear](const FGameXXKPermanentCompanion& Companion)
		{
			return !FormationBeforeClear.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.MemberId == Companion.InstanceId;
			});
		});
	TestRejectedState(TEXT("unavailable task NPC without an undeployed owned companion"), NoReplacementState);

	FGameXXKRuntimeState AlreadyLegalState = State;
	AlreadyLegalState.CardRun.OrderedFormation = Repaired;
	FGameXXKPartyFormationRules::ProjectCompatibility(AlreadyLegalState);
	FGameXXKOrderedPartyFormation AlreadyLegalOutput;
	Error.Reset();
	TestTrue(
		FString::Printf(TEXT("an already legal companion-only formation remains valid: %s"), *Error),
		FGameXXKPartyFormationRules::RepairUnavailableQuestNpcSlotsPreservingOrder(
			AlreadyLegalState,
			AlreadyLegalOutput,
			&Error));
	TestTrue(
		TEXT("an already legal formation is returned byte-identically"),
		FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&AlreadyLegalOutput,
			&AlreadyLegalState.CardRun.OrderedFormation,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementFormationFailureAtomicityTest,
	"GameXXK.Route.Settlement.FormationRepair.NoReplacementRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementFormationFailureAtomicityTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("failure fixture enters a legal route"), StartDefaultOrderedFormationRoute(State)))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation Formation = State.CardRun.OrderedFormation;
	State.CardRun.CompanionRoster.PermanentCompanions.RemoveAll([&Formation](const FGameXXKPermanentCompanion& Companion)
	{
		return !Formation.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.MemberId == Companion.InstanceId;
		});
	});
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
	FGameXXKRouteSettlementReceipt Receipt;
	FString Error;
	if (!TestTrue(
		TEXT("failure fixture previews a valid settlement receipt"),
		FGameXXKRouteSettlementRules::Preview(State, EGameXXKRouteTerminalOutcome::Abandoned, Receipt, &Error)))
	{
		return false;
	}
	State.CardRun.PendingSettlement = Receipt;
	const FGameXXKRuntimeState BeforeApply = State;
	TestFalse(
		TEXT("settlement rejects when no legal exact-slot replacement exists"),
		FGameXXKRouteSettlementRules::Apply(State, Receipt, &Error));
	TestFalse(TEXT("failed settlement reports a concrete replacement error"), Error.IsEmpty());
	TestTrue(TEXT("one-companion current route is rejected by the pre-settlement roster invariant"),
		Error.Contains(TEXT("two"), ESearchCase::IgnoreCase));
	TestTrue(
		TEXT("failed settlement grants nothing and leaves the entire runtime bit-identical"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforeApply, PPF_None));

	const auto TestCorruptApplyRollback = [this](
		const TCHAR* Label,
		FGameXXKRuntimeState CorruptState,
		const bool bIdempotentRecovery)
	{
		CorruptState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
		FGameXXKRouteSettlementReceipt CorruptReceipt;
		FString CorruptError;
		if (!TestTrue(
			FString::Printf(TEXT("%s previews a structurally valid receipt"), Label),
			FGameXXKRouteSettlementRules::Preview(
				CorruptState,
				EGameXXKRouteTerminalOutcome::Abandoned,
				CorruptReceipt,
				&CorruptError)))
		{
			return;
		}
		CorruptState.CardRun.PendingSettlement = CorruptReceipt;
		if (bIdempotentRecovery)
		{
			CorruptState.CardRun.LastAppliedRouteSettlementId = CorruptReceipt.SettlementId;
		}
		const FGameXXKRuntimeState CorruptBeforeApply = CorruptState;
		CorruptError.Reset();
		TestFalse(
			FString::Printf(TEXT("%s is rejected before cleanup or award"), Label),
			FGameXXKRouteSettlementRules::Apply(CorruptState, CorruptReceipt, &CorruptError));
		TestFalse(FString::Printf(TEXT("%s returns a concrete validation error"), Label), CorruptError.IsEmpty());
		TestTrue(
			FString::Printf(TEXT("%s preserves the complete state atomically"), Label),
			FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
				&CorruptState,
				&CorruptBeforeApply,
				PPF_None));
	};

	FGameXXKRuntimeState PreClearedCorruptState;
	if (!TestTrue(TEXT("pre-cleared corruption fixture enters a legal route"), StartDefaultOrderedFormationRoute(PreClearedCorruptState)))
	{
		return false;
	}
	PreClearedCorruptState.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
	PreClearedCorruptState.CardRun.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
	TestCorruptApplyRollback(TEXT("pre-existing stale task-NPC formation"), PreClearedCorruptState, false);
	TestCorruptApplyRollback(TEXT("pre-existing stale task-NPC recovery snapshot"), PreClearedCorruptState, true);

	FGameXXKRuntimeState MismatchedNpcCorruptState;
	if (!TestTrue(TEXT("mismatched NPC fixture enters a legal route"), StartDefaultOrderedFormationRoute(MismatchedNpcCorruptState)))
	{
		return false;
	}
	MismatchedNpcCorruptState.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.YueBai");
	TestCorruptApplyRollback(TEXT("mismatched task-NPC mirrors"), MismatchedNpcCorruptState, false);

	FGameXXKRuntimeState RecoveryNoReplacementState = BeforeApply;
	RecoveryNoReplacementState.CardRun.LastAppliedRouteSettlementId = Receipt.SettlementId;
	const FGameXXKRuntimeState RecoveryNoReplacementBefore = RecoveryNoReplacementState;
	Error.Reset();
	TestFalse(
		TEXT("idempotent recovery also rejects when no replacement exists"),
		FGameXXKRouteSettlementRules::Apply(RecoveryNoReplacementState, Receipt, &Error));
	TestTrue(
		TEXT("failed idempotent recovery remains bit-identical"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&RecoveryNoReplacementState,
			&RecoveryNoReplacementBefore,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementFormationSaveRoundTripTest,
	"GameXXK.Route.Settlement.FormationRepair.TerminalFacadeSaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementFormationSaveRoundTripTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Abandoned,
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Cleared})
	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("terminal facade fixture enters a legal route"), StartDefaultOrderedFormationRoute(State)))
		{
			return false;
		}
		if (Outcome == EGameXXKRouteTerminalOutcome::Cleared)
		{
			State.CardRun.RouteProgress.CurrentChapter = 3;
		}
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
		const FGameXXKTrainingProgress TrainingBefore = State.Training;
		const FGameXXKCompanionRosterState RosterBefore = State.CardRun.CompanionRoster;
		const bool bSettled = Outcome == EGameXXKRouteTerminalOutcome::Cleared
			? UGameXXKMVPRules::ResolveBossClear(State)
			: (Outcome == EGameXXKRouteTerminalOutcome::Defeated
				? UGameXXKMVPRules::FailDungeonToTown(State)
				: UGameXXKMVPRules::AbandonDungeonToTown(State));
		if (!TestTrue(TEXT("terminal facade completes settlement"), bSettled))
		{
			return false;
		}

		FString Error;
		TestTrue(
			FString::Printf(TEXT("terminal facade result is immediately save-valid: %s"), *Error),
			FGameXXKSaveMigration::ValidateRuntimeState(State, Error));
		TestTrue(
			TEXT("terminal facade preserves Training bit-identically"),
			FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(&State.Training, &TrainingBefore, PPF_None));
		TestTrue(
			TEXT("terminal facade preserves permanent roster bit-identically"),
			FGameXXKCompanionRosterState::StaticStruct()->CompareScriptStruct(
				&State.CardRun.CompanionRoster,
				&RosterBefore,
				PPF_None));

		const FGameXXKSaveState Saved = UGameXXKMVPRules::MakeSaveState(State);
		FGameXXKSaveState Reloaded;
		FGameXXKSaveMigrationReport Report;
		TestTrue(
			FString::Printf(TEXT("v24 settlement save roundtrips: %s"), *Report.Error),
			FGameXXKSaveMigration::MigrateToCurrent(Saved, Reloaded, Report));
		TestTrue(
			TEXT("v24 roundtrip preserves exact ordered formation"),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&Reloaded.RuntimeState.CardRun.OrderedFormation,
				&State.CardRun.OrderedFormation,
				PPF_None));
	}

	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("facade fixture creates subsystem"), Subsystem)
		|| !TestTrue(TEXT("facade fixture starts with materialized formation"), Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& FacadeState = Subsystem->GetMutableRuntimeState();
	if (!TestTrue(TEXT("facade fixture accepts quest"), UGameXXKMVPRules::AcceptTownQuest(FacadeState)))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation FormationBeforeEnter = FacadeState.CardRun.OrderedFormation;
	if (!TestTrue(TEXT("facade fixture enters route"), UGameXXKMVPRules::EnterDungeon(FacadeState)))
	{
		return false;
	}
	TestTrue(
		TEXT("EnterDungeon clear-and-readd keeps the selected NPC and full formation byte-identical"),
		FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&FacadeState.CardRun.OrderedFormation,
			&FormationBeforeEnter,
			PPF_None));
	TestTrue(TEXT("subsystem abandon facade settles the route"), Subsystem->AbandonDungeonToTown());
	FString Error;
	TestTrue(
		FString::Printf(TEXT("subsystem facade result is immediately save-valid: %s"), *Error),
		FGameXXKSaveMigration::ValidateRuntimeState(Subsystem->GetRuntimeState(), Error));
	return true;
}

#endif
