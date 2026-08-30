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
	bool StartSelectedNpcRoute(FGameXXKRuntimeState& OutState, const FName NpcId)
	{
		UGameXXKMVPSubsystem* Subsystem =
			NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeStateCopy();
		return FGameXXKPartyFormationRules::SetQuestNpc(OutState, NpcId)
			&& UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementFormationTerminalOutcomesTest,
	"GameXXK.Route.Settlement.FormationPreservation.TerminalOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementFormationTerminalOutcomesTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Abandoned,
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Cleared})
	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("selected Yue Bai party enters a legal route"),
			StartSelectedNpcRoute(State, TEXT("Npc.YueBai"))))
		{
			return false;
		}
		const FGameXXKOrderedPartyFormation FormationBefore = State.CardRun.OrderedFormation;
		const FGameXXKCompanionPartySelection PartySelectionBefore =
			State.CardRun.PartySelection;
		const FGameXXKTrainingProgress TrainingBefore = State.Training;

		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
		FGameXXKRouteSettlementReceipt Receipt;
		FString Error;
		if (!TestTrue(
			FString::Printf(TEXT("terminal receipt previews for outcome %d: %s"),
				static_cast<int32>(Outcome), *Error),
			FGameXXKRouteSettlementRules::Preview(State, Outcome, Receipt, &Error)))
		{
			return false;
		}
		State.CardRun.PendingSettlement = Receipt;
		if (!TestTrue(
			FString::Printf(TEXT("terminal receipt applies for outcome %d: %s"),
				static_cast<int32>(Outcome), *Error),
			FGameXXKRouteSettlementRules::Apply(State, Receipt, &Error)))
		{
			return false;
		}

		TestTrue(TEXT("settlement preserves exact ordered formation"),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&State.CardRun.OrderedFormation,
				&FormationBefore,
				PPF_None));
		TestTrue(TEXT("settlement preserves active selection, all loadouts, and all progressions"),
			FGameXXKCompanionPartySelection::StaticStruct()->CompareScriptStruct(
				&State.CardRun.PartySelection,
				&PartySelectionBefore,
				PPF_None));
		TestTrue(TEXT("temporary provenance stays retired"),
			State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
		TestTrue(TEXT("settled formation validates"),
			FGameXXKPartyFormationRules::Validate(
				State,
				State.CardRun.OrderedFormation,
				&Error));
		TestTrue(TEXT("settled compatibility projection validates"),
			FGameXXKPartyFormationRules::ValidateCompatibilityProjection(State, &Error));
		TestTrue(TEXT("settlement preserves Training bit-identically"),
			FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(
				&State.Training,
				&TrainingBefore,
				PPF_None));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteLocalCleanupPreservesFormationTest,
	"GameXXK.Route.Settlement.FormationPreservation.RouteCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteLocalCleanupPreservesFormationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("cleanup fixture enters a legal route"),
		StartSelectedNpcRoute(State, TEXT("Npc.JinGui"))))
	{
		return false;
	}
	State.CardRun.PendingEvent.SourceNodeId = 71;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
	State.CardRun.RouteMerchant.SourceNodeId = 72;
	State.CardRun.RouteMerchant.OfferSeed = 7201;
	const FGameXXKOrderedPartyFormation FormationBefore = State.CardRun.OrderedFormation;
	const FGameXXKCompanionPartySelection PartyBefore = State.CardRun.PartySelection;

	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(State);

	TestTrue(TEXT("route cleanup preserves ordered formation"),
		FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&State.CardRun.OrderedFormation,
			&FormationBefore,
			PPF_None));
	TestTrue(TEXT("route cleanup preserves all party selection data"),
		FGameXXKCompanionPartySelection::StaticStruct()->CompareScriptStruct(
			&State.CardRun.PartySelection,
			&PartyBefore,
			PPF_None));
	TestFalse(TEXT("route cleanup releases the loadout lock"),
		State.CardRun.bLoadoutLockedForRoute);
	TestTrue(TEXT("route cleanup clears pending event"),
		State.CardRun.PendingEvent.EventNpcId.IsNone());
	TestEqual(TEXT("route cleanup clears merchant source"),
		State.CardRun.RouteMerchant.SourceNodeId,
		INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementFormationSaveRoundTripTest,
	"GameXXK.Route.Settlement.FormationPreservation.TerminalFacade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementFormationSaveRoundTripTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Abandoned,
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Cleared})
	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("terminal facade fixture enters a legal route"),
			StartSelectedNpcRoute(State, TEXT("Npc.QiongMeiEr"))))
		{
			return false;
		}
		const FGameXXKOrderedPartyFormation FormationBefore = State.CardRun.OrderedFormation;
		if (Outcome == EGameXXKRouteTerminalOutcome::Cleared)
		{
			State.CardRun.RouteProgress.CurrentChapter = 3;
		}
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
		const bool bSettled = Outcome == EGameXXKRouteTerminalOutcome::Cleared
			? UGameXXKMVPRules::ResolveBossClear(State)
			: (Outcome == EGameXXKRouteTerminalOutcome::Defeated
				? UGameXXKMVPRules::FailDungeonToTown(State)
				: UGameXXKMVPRules::AbandonDungeonToTown(State));
		if (!TestTrue(TEXT("terminal facade completes settlement"), bSettled))
		{
			return false;
		}
		TestTrue(TEXT("terminal facade preserves formation"),
			FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&State.CardRun.OrderedFormation,
				&FormationBefore,
				PPF_None));

	}
	return true;
}

#endif
