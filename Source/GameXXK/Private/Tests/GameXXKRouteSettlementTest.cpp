#include "GameXXKRouteSettlementRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool StartMaterializedFormationGame(FGameXXKRuntimeState& OutState)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeState();
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementConversionTest,
	"GameXXK.Route.Settlement.Conversions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementConversionTest::RunTest(const FString& Parameters)
{
	const TArray<int32> TravelMoneyValues = {0, 9, 10, 19, 20, 101};
	const TArray<int32> RouteCardCounts = {0, 4, 5, 9, 10, 51};
	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Cleared,
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Abandoned})
	{
		const int32 MoneyDivisor = Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 10 : 20;
		const int32 CardDivisor = Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 5 : 10;
		for (const int32 TravelMoney : TravelMoneyValues)
		{
			for (const int32 RouteCardCount : RouteCardCounts)
			{
				FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
				State.bDungeonActive = true;
				TestTrue(TEXT("settlement fixture initializes its route economy"),
					FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, TravelMoney));
				State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = RouteCardCount;

				FGameXXKRouteSettlementReceipt Receipt;
				FString Error;
				TestTrue(TEXT("settlement preview accepts a non-negative active route"),
					FGameXXKRouteSettlementRules::Preview(State, Outcome, Receipt, &Error));
				TestTrue(TEXT("a valid zero-value settlement still has a receipt ID"), Receipt.SettlementId.IsValid());
				TestEqual(TEXT("receipt preserves the route travel money snapshot"), Receipt.SourceTravelMoney, TravelMoney);
				TestEqual(TEXT("receipt preserves the acquired route-card snapshot"), Receipt.SourceCardAcquisitionCount, RouteCardCount);
				TestEqual(TEXT("permanent gold uses the approved floor conversion"), Receipt.PermanentGoldAward, TravelMoney / MoneyDivisor);
				TestEqual(TEXT("enhancement stones use the approved floor conversion"), Receipt.EnhancementStoneAward, RouteCardCount / CardDivisor);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementReplayTest,
	"GameXXK.Route.Settlement.ReplayAndAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementReplayTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("replay fixture starts with a materialized ordered formation"), StartMaterializedFormationGame(State)))
	{
		return false;
	}
	State.bDungeonActive = true;
	TestTrue(TEXT("replay fixture initializes its route economy"),
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 101));
	State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 51;
	const int32 GoldBefore = State.PlayerGold;
	const int32 StonesBefore = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemEnhancementStone());

	FGameXXKRouteSettlementReceipt Receipt;
	FString Error;
	TestTrue(TEXT("a terminal receipt can be created"),
		FGameXXKRouteSettlementRules::Preview(State, EGameXXKRouteTerminalOutcome::Cleared, Receipt, &Error));
	State.CardRun.PendingSettlement = Receipt;
	TestTrue(TEXT("the staged terminal receipt awards once and clears route-local values"),
		FGameXXKRouteSettlementRules::Apply(State, Receipt, &Error));
	TestEqual(TEXT("clear settlement awards permanent gold exactly once"), State.PlayerGold, GoldBefore + 10);
	TestEqual(TEXT("clear settlement awards enhancement stones exactly once"),
		UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemEnhancementStone()), StonesBefore + 10);
	TestEqual(TEXT("settlement clears the source travel money"), State.CardRun.RouteTravelMoney, 0);
	TestEqual(TEXT("settlement clears the source card-acquisition count"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 0);
	TestEqual(TEXT("settlement records its idempotency key"), State.CardRun.LastAppliedRouteSettlementId, Receipt.SettlementId);

	TestTrue(TEXT("replaying an already applied receipt succeeds without a second award"),
		FGameXXKRouteSettlementRules::Apply(State, Receipt, &Error));
	TestEqual(TEXT("replaying an already applied receipt never duplicates permanent gold"), State.PlayerGold, GoldBefore + 10);
	TestEqual(TEXT("replaying an already applied receipt never duplicates enhancement stones"),
		UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemEnhancementStone()), StonesBefore + 10);

	FGameXXKRuntimeState NextRouteState;
	if (!TestTrue(TEXT("later-route fixture starts with a materialized ordered formation"), StartMaterializedFormationGame(NextRouteState)))
	{
		return false;
	}
	TestTrue(TEXT("the later route opens the world map"), UGameXXKMVPRules::OpenWorldMap(NextRouteState));
	TestTrue(TEXT("the later route enters Qingshan through the canonical town path"),
		UGameXXKMVPRules::EnterWorldRegion(NextRouteState, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("the later route accepts the quest through the canonical follower path"),
		UGameXXKMVPRules::AcceptTownQuest(NextRouteState));
	NextRouteState.CardRun.LastAppliedRouteSettlementId = Receipt.SettlementId;
	TestTrue(TEXT("a later route enters while retaining the old settlement idempotency key"),
		UGameXXKMVPRules::EnterDungeon(NextRouteState));
	FGameXXKRouteMapNode* LaterMerchantNode = NextRouteState.RouteMapNodes.FindByPredicate(
		[](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeKind != EGameXXKNodeKind::Start
				&& Node.NodeKind != EGameXXKNodeKind::Boss;
		});
	if (!TestNotNull(TEXT("the later route has a deterministic non-terminal node for the merchant fixture"), LaterMerchantNode))
	{
		return false;
	}
	LaterMerchantNode->NodeKind = EGameXXKNodeKind::Merchant;
	NextRouteState.Screen = EGameXXKScreen::RouteMerchant;
	NextRouteState.CurrentMapId = TEXT("RouteMerchant");
	NextRouteState.PendingRouteNodeId = LaterMerchantNode->NodeId;
	TestTrue(TEXT("the later route persists fresh merchant stock"),
		FGameXXKRouteMerchantRules::EnsureStock(NextRouteState, &Error));
	TestTrue(TEXT("the later route merchant snapshot is nonempty"),
		!NextRouteState.CardRun.RouteMerchant.Offers.IsEmpty());
	const FGameXXKRuntimeState NextRouteBeforeOldReplay = NextRouteState;
	TestTrue(TEXT("replaying the old receipt into a later route succeeds as an idempotent no-op"),
		FGameXXKRouteSettlementRules::Apply(NextRouteState, Receipt, &Error));
	TestTrue(TEXT("an old receipt replay preserves every property of the newly entered route"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&NextRouteState,
			&NextRouteBeforeOldReplay,
			PPF_None));

	FGameXXKRuntimeState RecoveryState;
	if (!TestTrue(TEXT("recovery fixture starts with a materialized ordered formation"), StartMaterializedFormationGame(RecoveryState)))
	{
		return false;
	}
	RecoveryState.bDungeonActive = true;
	TestTrue(TEXT("crash-recovery fixture initializes its original route economy"),
		FGameXXKRouteEconomyRules::InitializeRoute(RecoveryState.CardRun, Receipt.SourceTravelMoney));
	RecoveryState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = Receipt.SourceCardAcquisitionCount;
	RecoveryState.CardRun.PendingSettlement = Receipt;
	RecoveryState.CardRun.LastAppliedRouteSettlementId = Receipt.SettlementId;
	const int32 RecoveryGoldBefore = RecoveryState.PlayerGold;
	const int32 RecoveryStonesBefore = UGameXXKMVPRules::GetItemCount(
		RecoveryState,
		UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("a matching partially cleaned applied receipt completes crash recovery"),
		FGameXXKRouteSettlementRules::Apply(RecoveryState, Receipt, &Error));
	TestEqual(TEXT("crash recovery never duplicates permanent gold"), RecoveryState.PlayerGold, RecoveryGoldBefore);
	TestEqual(TEXT("crash recovery never duplicates enhancement stones"),
		UGameXXKMVPRules::GetItemCount(RecoveryState, UGameXXKMVPRules::ItemEnhancementStone()),
		RecoveryStonesBefore);
	TestEqual(TEXT("crash recovery clears the matching source travel money"), RecoveryState.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("crash recovery clears route economy initialization"), RecoveryState.CardRun.bRouteEconomyInitialized);
	TestEqual(TEXT("crash recovery retains the applied receipt id"),
		RecoveryState.CardRun.LastAppliedRouteSettlementId,
		Receipt.SettlementId);

	FGameXXKRuntimeState RejectedState = UGameXXKMVPRules::CreateNewGame();
	RejectedState.bDungeonActive = true;
	TestTrue(TEXT("rejected fixture initializes its route economy"),
		FGameXXKRouteEconomyRules::InitializeRoute(RejectedState.CardRun, 20));
	RejectedState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
	FGameXXKRouteSettlementReceipt RejectedReceipt;
	TestTrue(TEXT("a second terminal receipt can be created"),
		FGameXXKRouteSettlementRules::Preview(RejectedState, EGameXXKRouteTerminalOutcome::Defeated, RejectedReceipt, &Error));
	RejectedState.CardRun.PendingSettlement = RejectedReceipt;
	const FGameXXKRuntimeState BeforeRejectedApply = RejectedState;
	++RejectedReceipt.PermanentGoldAward;
	TestFalse(TEXT("a tampered receipt is rejected"), FGameXXKRouteSettlementRules::Apply(RejectedState, RejectedReceipt, &Error));
	TestTrue(TEXT("a rejected receipt is fully atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&RejectedState, &BeforeRejectedApply, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementTerminalPathsTest,
	"GameXXK.Route.Settlement.TerminalPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	bool StartAcceptedThreeChapterRoute(FGameXXKRuntimeState& OutState)
	{
		return StartMaterializedFormationGame(OutState)
			&& UGameXXKMVPRules::OpenWorldMap(OutState)
			&& UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}

	void SeedTerminalSettlementSources(FGameXXKRuntimeState& InOutState)
	{
		InOutState.CardRun.RouteTravelMoney = 101;
		InOutState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 51;
	}
}

bool FGameXXKRouteSettlementTerminalPathsTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState ClearState;
	TestTrue(TEXT("the terminal-clear fixture enters an accepted three-chapter route"), StartAcceptedThreeChapterRoute(ClearState));
	ClearState.CardRun.RouteProgress.CurrentChapter = 3;
	SeedTerminalSettlementSources(ClearState);
	const int32 ClearGoldBefore = ClearState.PlayerGold;
	const int32 ClearStonesBefore = UGameXXKMVPRules::GetItemCount(ClearState, UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("the third chapter Boss clears through the terminal settlement path"), UGameXXKMVPRules::ResolveBossClear(ClearState));
	TestEqual(TEXT("a clear converts travel money at ten to one"), ClearState.PlayerGold, ClearGoldBefore + 10);
	TestEqual(TEXT("a clear converts route cards to stones at five to one"),
		UGameXXKMVPRules::GetItemCount(ClearState, UGameXXKMVPRules::ItemEnhancementStone()), ClearStonesBefore + 10);
	TestFalse(TEXT("a clear ends the active route"), ClearState.bDungeonActive);
	TestTrue(TEXT("a clear retains its settlement idempotency key"), ClearState.CardRun.LastAppliedRouteSettlementId.IsValid());

	FGameXXKRuntimeState FailureState;
	TestTrue(TEXT("the failure fixture enters an accepted three-chapter route"), StartAcceptedThreeChapterRoute(FailureState));
	SeedTerminalSettlementSources(FailureState);
	const int32 FailureGoldBefore = FailureState.PlayerGold;
	const int32 FailureStonesBefore = UGameXXKMVPRules::GetItemCount(FailureState, UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("failing the route settles through the defeated path"), UGameXXKMVPRules::FailDungeonToTown(FailureState));
	TestEqual(TEXT("a failure converts travel money at twenty to one"), FailureState.PlayerGold, FailureGoldBefore + 5);
	TestEqual(TEXT("a failure converts route cards to stones at ten to one"),
		UGameXXKMVPRules::GetItemCount(FailureState, UGameXXKMVPRules::ItemEnhancementStone()), FailureStonesBefore + 5);
	TestEqual(TEXT("a failure returns the player to town"), FailureState.Screen, EGameXXKScreen::Town);

	FGameXXKRuntimeState AbandonState;
	TestTrue(TEXT("the abandon fixture enters an accepted three-chapter route"), StartAcceptedThreeChapterRoute(AbandonState));
	SeedTerminalSettlementSources(AbandonState);
	const int32 AbandonGoldBefore = AbandonState.PlayerGold;
	const int32 AbandonStonesBefore = UGameXXKMVPRules::GetItemCount(AbandonState, UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("voluntarily abandoning the route settles through the abandoned path"), UGameXXKMVPRules::AbandonDungeonToTown(AbandonState));
	TestEqual(TEXT("abandoning converts travel money at twenty to one"), AbandonState.PlayerGold, AbandonGoldBefore + 5);
	TestEqual(TEXT("abandoning converts route cards to stones at ten to one"),
		UGameXXKMVPRules::GetItemCount(AbandonState, UGameXXKMVPRules::ItemEnhancementStone()), AbandonStonesBefore + 5);
	TestEqual(TEXT("abandoning returns the player to town"), AbandonState.Screen, EGameXXKScreen::Town);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementAbandonSubsystemFacadeTest,
	"GameXXK.Route.Settlement.AbandonSubsystemFacade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementAbandonSubsystemFacadeTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState ActiveRoute;
	if (!TestTrue(
		TEXT("subsystem abandon fixture enters an accepted route"),
		StartAcceptedThreeChapterRoute(ActiveRoute)))
	{
		return false;
	}
	ActiveRoute.CardRun.RouteTravelMoney = 99;
	ActiveRoute.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	Subsystem->GetMutableRuntimeState() = ActiveRoute;

	const FGameXXKRuntimeState BeforePreview = Subsystem->GetRuntimeState();
	FGameXXKRouteSettlementReceipt Preview;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("subsystem previews abandoned settlement: %s"), *Error),
		Subsystem->PreviewAbandonedRouteSettlement(Preview, &Error));
	TestEqual(TEXT("preview uses abandoned outcome"), Preview.Outcome, EGameXXKRouteTerminalOutcome::Abandoned);
	TestEqual(TEXT("99 route money previews four permanent gold"), Preview.PermanentGoldAward, 4);
	TestEqual(TEXT("29 acquisitions preview two enhancement stones"), Preview.EnhancementStoneAward, 2);
	TestTrue(TEXT("preview creates a stable receipt id"), Preview.SettlementId.IsValid());
	TestTrue(
		TEXT("settlement preview has no runtime side effects"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(),
			&BeforePreview,
			PPF_None));

	const int32 GoldBefore = BeforePreview.PlayerGold;
	const int32 StonesBefore = UGameXXKMVPRules::GetItemCount(
		BeforePreview,
		UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("subsystem applies abandoned settlement"), Subsystem->AbandonDungeonToTown());
	const FGameXXKRuntimeState& Settled = Subsystem->GetRuntimeState();
	TestEqual(TEXT("abandon facade awards previewed permanent gold"), Settled.PlayerGold, GoldBefore + 4);
	TestEqual(
		TEXT("abandon facade awards previewed enhancement stones"),
		UGameXXKMVPRules::GetItemCount(Settled, UGameXXKMVPRules::ItemEnhancementStone()),
		StonesBefore + 2);
	TestEqual(TEXT("abandon facade returns to town"), Settled.Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("abandon facade ends the route"), Settled.bDungeonActive);
	TestTrue(TEXT("abandon facade records an idempotency receipt id"), Settled.CardRun.LastAppliedRouteSettlementId.IsValid());

	const int32 GoldAfterFirstApply = Settled.PlayerGold;
	const int32 StonesAfterFirstApply = UGameXXKMVPRules::GetItemCount(
		Settled,
		UGameXXKMVPRules::ItemEnhancementStone());
	TestFalse(TEXT("a second UI confirmation cannot settle an inactive route"), Subsystem->AbandonDungeonToTown());
	TestEqual(TEXT("a second confirmation cannot duplicate gold"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFirstApply);
	TestEqual(
		TEXT("a second confirmation cannot duplicate stones"),
		UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemEnhancementStone()),
		StonesAfterFirstApply);

	FGameXXKRouteSettlementReceipt InvalidPreview;
	Error.Reset();
	TestFalse(
		TEXT("inactive route has no abandoned settlement preview"),
		Subsystem->PreviewAbandonedRouteSettlement(InvalidPreview, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementExitToDesktopTransactionTest,
	"GameXXK.MVP.RouteSettlement.SettleAndExitActiveRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementExitToDesktopTransactionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState ActiveRoute;
	if (!TestTrue(TEXT("settlement-exit fixture enters an active generated route"), StartAcceptedThreeChapterRoute(ActiveRoute)))
	{
		return false;
	}

	ActiveRoute.CardRun.RouteTravelMoney = 99;
	ActiveRoute.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	ActiveRoute.Training.bTravelActive = true;
	ActiveRoute.Training.CurrentTravelStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	ActiveRoute.Training.ActiveTravelEncounterIndex = 3;
	ActiveRoute.Training.TravelNormalChestCooldownRemainingSeconds = 71;
	ActiveRoute.Training.TravelAdvancedChestCooldownRemainingSeconds = 119;
	ActiveRoute.Training.PendingTravelGold = 321;
	ActiveRoute.Training.PendingTravelExperience = 654;
	ActiveRoute.Training.TravelLastUpdatedUnixSeconds = 123456789;
	ActiveRoute.Training.bChallengeActive = true;
	ActiveRoute.Training.ActiveChallengeStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	ActiveRoute.Training.ActiveChallengeEncounterIndex = 4;
	ActiveRoute.Training.ActiveChallengeRouteNodeId = 1234;
	ActiveRoute.Training.ChallengeRouteNodeEncounterIndices.Add(1234, 4);
	ActiveRoute.Training.bChallengeAutoBattle = true;
	const int32 PendingNodeId = ActiveRoute.ReachableRouteNodeIds.IsEmpty()
		? ActiveRoute.RouteMapNodes.Last().NodeId
		: ActiveRoute.ReachableRouteNodeIds[0];
	ActiveRoute.PendingRouteNodeId = PendingNodeId;
	ActiveRoute.CardRun.PendingEvent.SourceNodeId = PendingNodeId;
	ActiveRoute.CardRun.PendingEvent.ChoiceSeed = 77;
	ActiveRoute.CardRun.PendingRelicOffer.SourceNodeId = PendingNodeId;
	ActiveRoute.CardRun.PendingRelicOffer.ChoiceSeed = 88;
	ActiveRoute.CardRun.RouteMerchant.SourceNodeId = PendingNodeId;
	ActiveRoute.CardRun.RouteMerchant.OfferSeed = 99;
	ActiveRoute.CardRun.PendingReward.SourceNodeId = PendingNodeId;
	ActiveRoute.CardRun.PendingReward.ChoiceSeed = 111;
	ActiveRoute.bHasActiveBattle = true;
	ActiveRoute.ActiveBattleNodeId = PendingNodeId;
	ActiveRoute.ActiveBattleEnemies.Add(FGameXXKBattleRuntimeUnit());
	ActiveRoute.ActiveBattleParty.Add(FGameXXKBattleRuntimeUnit());
	ActiveRoute.BattleEntryCheckpoint.bValid = true;
	ActiveRoute.BattleEntryCheckpoint.SourceNodeId = PendingNodeId;
	FGameXXKTrainingProgress ExpectedTrainingAfter = ActiveRoute.Training;
	ExpectedTrainingAfter.bChallengeActive = false;
	ExpectedTrainingAfter.ActiveChallengeStageId = NAME_None;
	ExpectedTrainingAfter.ActiveChallengeEncounterIndex = INDEX_NONE;
	ExpectedTrainingAfter.ActiveChallengeRouteNodeId = INDEX_NONE;
	ExpectedTrainingAfter.ChallengeRouteNodeEncounterIndices.Reset();
	ExpectedTrainingAfter.bChallengeAutoBattle = false;
	const int32 GoldBefore = ActiveRoute.PlayerGold;
	const int32 StonesBefore = UGameXXKMVPRules::GetItemCount(ActiveRoute, UGameXXKMVPRules::ItemEnhancementStone());
	Subsystem->GetMutableRuntimeState() = ActiveRoute;

	FGameXXKRouteSettlementReceipt Receipt;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("authoritative route settlement-exit transaction succeeds: %s"), *Error),
		Subsystem->SettleAndExitActiveRoute(Receipt, Error));
	const FGameXXKRuntimeState& Settled = Subsystem->GetRuntimeState();
	TestTrue(TEXT("successful transaction returns a valid receipt"), Receipt.SettlementId.IsValid());
	TestEqual(TEXT("receipt settles only earned route money"), Receipt.SourceTravelMoney, 99);
	TestEqual(TEXT("receipt settles only earned route-card progress"), Receipt.SourceCardAcquisitionCount, 29);
	TestEqual(TEXT("receipt converts earned route money at abandoned rate"), Receipt.PermanentGoldAward, 4);
	TestEqual(TEXT("receipt converts earned route cards at abandoned rate"), Receipt.EnhancementStoneAward, 2);
	TestEqual(TEXT("transaction grants ordinary gold exactly once"), Settled.PlayerGold, GoldBefore + 4);
	TestEqual(
		TEXT("transaction grants earned item reward exactly once"),
		UGameXXKMVPRules::GetItemCount(Settled, UGameXXKMVPRules::ItemEnhancementStone()),
		StonesBefore + 2);
	TestEqual(TEXT("settlement returns to Town workbench screen"), Settled.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("settlement stays on the canonical pure-2D map"), Settled.CurrentMapId, FName(TEXT("DesktopTrainingHUD")));
	TestFalse(TEXT("settlement clears active route"), Settled.bDungeonActive);
	TestFalse(TEXT("settlement clears generated route flag"), Settled.bHasGeneratedRouteMap);
	TestTrue(TEXT("settlement clears route nodes"), Settled.RouteMapNodes.IsEmpty());
	TestTrue(TEXT("settlement clears route edges"), Settled.RouteMapEdges.IsEmpty());
	TestTrue(TEXT("settlement clears visited route progress"), Settled.VisitedRouteNodeIds.IsEmpty());
	TestTrue(TEXT("settlement clears reachable route progress"), Settled.ReachableRouteNodeIds.IsEmpty());
	TestEqual(TEXT("settlement clears current route node"), Settled.CurrentRouteNodeId, INDEX_NONE);
	TestEqual(TEXT("settlement clears pending route node"), Settled.PendingRouteNodeId, INDEX_NONE);
	TestEqual(TEXT("settlement clears pending event"), Settled.CardRun.PendingEvent.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("settlement clears pending relic offer"), Settled.CardRun.PendingRelicOffer.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("settlement clears pending merchant"), Settled.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("settlement clears pending battle reward"), Settled.CardRun.PendingReward.SourceNodeId, INDEX_NONE);
	TestFalse(TEXT("settlement clears legacy battle projection"), Settled.bHasActiveBattle);
	TestFalse(TEXT("settlement clears card battle runtime"), Settled.CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("settlement clears battle rollback checkpoint"), Settled.BattleEntryCheckpoint.bValid);
	TestTrue(
		TEXT("settlement preserves the complete Training Travel runtime bit-identically"),
		FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(&Settled.Training, &ExpectedTrainingAfter, PPF_None));
	TestEqual(TEXT("settlement records the returned idempotency key"), Settled.CardRun.LastAppliedRouteSettlementId, Receipt.SettlementId);

	const int32 GoldAfterFirstApply = Settled.PlayerGold;
	const int32 StonesAfterFirstApply = UGameXXKMVPRules::GetItemCount(Settled, UGameXXKMVPRules::ItemEnhancementStone());
	FGameXXKRouteSettlementReceipt DuplicateReceipt;
	Error.Reset();
	TestFalse(TEXT("repeating settlement on the cleared route is rejected"),
		Subsystem->SettleAndExitActiveRoute(DuplicateReceipt, Error));
	TestFalse(TEXT("failed duplicate call returns no receipt"), DuplicateReceipt.SettlementId.IsValid());
	TestFalse(TEXT("failed duplicate call reports a concrete error"), Error.IsEmpty());
	TestEqual(TEXT("duplicate call cannot grant gold twice"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFirstApply);
	TestEqual(
		TEXT("duplicate call cannot grant item rewards twice"),
		UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemEnhancementStone()),
		StonesAfterFirstApply);

	UGameXXKMVPSubsystem* OverflowSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState OverflowRoute;
	TestTrue(TEXT("overflow rollback fixture enters an active route"), StartAcceptedThreeChapterRoute(OverflowRoute));
	OverflowRoute.PlayerGold = MAX_int32;
	OverflowRoute.CardRun.RouteTravelMoney = 20;
	OverflowRoute.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
	OverflowSubsystem->GetMutableRuntimeState() = OverflowRoute;
	const FGameXXKRuntimeState OverflowBefore = OverflowSubsystem->GetRuntimeState();
	FGameXXKRouteSettlementReceipt OverflowReceipt;
	OverflowReceipt.SettlementId = FGuid::NewGuid();
	Error.Reset();
	TestFalse(TEXT("ungrantable settlement is rejected"),
		OverflowSubsystem->SettleAndExitActiveRoute(OverflowReceipt, Error));
	TestFalse(TEXT("failed transaction returns no partial receipt"), OverflowReceipt.SettlementId.IsValid());
	TestFalse(TEXT("failed transaction reports a concrete error"), Error.IsEmpty());
	TestTrue(
		TEXT("failed transaction rolls back every runtime property"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&OverflowSubsystem->GetRuntimeState(),
			&OverflowBefore,
			PPF_None));

	UGameXXKMVPSubsystem* InvalidSaveSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState InvalidSaveRoute;
	TestTrue(TEXT("post-clean validation rollback fixture enters an active route"),
		StartAcceptedThreeChapterRoute(InvalidSaveRoute));
	InvalidSaveRoute.PlayerXP = -1;
	InvalidSaveRoute.CardRun.RouteTravelMoney = 20;
	InvalidSaveRoute.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
	InvalidSaveSubsystem->GetMutableRuntimeState() = InvalidSaveRoute;
	const FGameXXKRuntimeState InvalidSaveBefore = InvalidSaveSubsystem->GetRuntimeState();
	FGameXXKRouteSettlementReceipt InvalidSaveReceipt;
	InvalidSaveReceipt.SettlementId = FGuid::NewGuid();
	Error.Reset();
	TestFalse(TEXT("post-clean invalid save state rejects the entire transaction"),
		InvalidSaveSubsystem->SettleAndExitActiveRoute(InvalidSaveReceipt, Error));
	TestFalse(TEXT("post-clean validation failure returns no receipt"), InvalidSaveReceipt.SettlementId.IsValid());
	TestTrue(TEXT("post-clean validation failure returns a concrete save error"),
		Error.Contains(TEXT("无法安全保存")));
	TestTrue(TEXT("post-clean validation failure rolls back every runtime property"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&InvalidSaveSubsystem->GetRuntimeState(),
			&InvalidSaveBefore,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteSettlementWorkbenchValidityTest,
	"GameXXK.MVP.RouteSettlement.WorkbenchHealthRegionAndSaveValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteSettlementWorkbenchValidityTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* RouteSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState HuangshanRoute;
	if (!TestTrue(TEXT("health-validity fixture enters a generated route"), StartAcceptedThreeChapterRoute(HuangshanRoute)))
	{
		return false;
	}
	HuangshanRoute.CurrentRegion = UGameXXKMVPRules::RegionHuangshan();
	FGameXXKEquipmentCollectionState LegalEquipment;
	LegalEquipment.CollectionSeed = 0xB412;
	LegalEquipment.RefinementSand = HuangshanRoute.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand());
	FGameXXKEquipmentInstance ManaAccessory;
	ManaAccessory.InstanceId = TEXT("EquipmentInstance.RouteSettlement.ManaAccessory");
	ManaAccessory.BaseEquipmentId = TEXT("Equipment.PoJun.Accessory");
	ManaAccessory.ItemLevel = 7;
	ManaAccessory.Quality = EGameXXKEquipmentQuality::Common;
	ManaAccessory.ScalingRule = EGameXXKEquipmentScalingRule::ModernPercentBase;
	ManaAccessory.OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	ManaAccessory.OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKEquipmentAffixRoll AttackAffix;
	AttackAffix.AffixId = TEXT("Affix.Universal.Attack");
	AttackAffix.Tier = EGameXXKAffixTier::Common;
	AttackAffix.Magnitude = 300;
	AttackAffix.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
	ManaAccessory.RolledAffixes.Add(AttackAffix);
	LegalEquipment.EquipmentInstances.Add(ManaAccessory);
	LegalEquipment.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).AccessoryInstanceId =
		ManaAccessory.InstanceId;
	HuangshanRoute.EquipmentCollection = MoveTemp(LegalEquipment);
	TestTrue(TEXT("legal fixture synchronizes its authoritative equipment mirrors"),
		FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(HuangshanRoute));
	TestTrue(TEXT("legal fixture normalizes its physical desktop inventory"),
		FGameXXKDesktopInventoryRules::Normalize(HuangshanRoute));
	TestEqual(TEXT("legal fixture has the requested base max health"), HuangshanRoute.PlayerMaxHP, 100);
	TestEqual(TEXT("legal fixture has the requested equipped max mana"), HuangshanRoute.PlayerMaxMP, 40);
	HuangshanRoute.CardRun.RouteAttributeBonuses.MaxHealth = 10;
	HuangshanRoute.CardRun.RouteAttributeBonuses.MaxMana = 5;
	HuangshanRoute.PlayerHP = 110;
	HuangshanRoute.PlayerMP = 45;
	HuangshanRoute.CardRun.RouteTravelMoney = 20;
	HuangshanRoute.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 10;
	FString ValidationError;
	TestTrue(
		FString::Printf(TEXT("route bonus fixture is legal before settlement: %s"), *ValidationError),
		FGameXXKSaveMigration::ValidateRuntimeState(HuangshanRoute, ValidationError));
	RouteSubsystem->GetMutableRuntimeState() = HuangshanRoute;

	FGameXXKRouteSettlementReceipt RouteReceipt;
	FString Error;
	TestTrue(TEXT("generated Huangshan route settles to workbench"),
		RouteSubsystem->SettleAndExitActiveRoute(RouteReceipt, Error));
	const FGameXXKRuntimeState& SettledRoute = RouteSubsystem->GetRuntimeState();
	TestEqual(TEXT("route-local max-health removal restores full base health"), SettledRoute.PlayerHP, 100);
	TestEqual(TEXT("route-local max-mana removal restores full base mana"), SettledRoute.PlayerMP, 40);
	TestEqual(TEXT("generated Huangshan route returns to Qingshan workbench region"),
		SettledRoute.CurrentRegion,
		UGameXXKMVPRules::RegionQingshan());
	ValidationError.Reset();
	TestTrue(
		FString::Printf(TEXT("settled generated route is immediately save-valid: %s"), *ValidationError),
		FGameXXKSaveMigration::ValidateRuntimeState(SettledRoute, ValidationError));

	UGameXXKMVPSubsystem* TrainingSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("training-region fixture starts the desktop workbench"), TrainingSubsystem->StartGame());
	const FName TravelStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("training-region fixture starts the independent Travel loop"),
		TrainingSubsystem->StartTrainingTravel(TravelStageId));
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("training-region fixture starts a real generated challenge route"),
		TrainingSubsystem->StartTrainingChallenge(StageId));
	const FGameXXKTrainingTravelRuntime TravelRuntimeBefore = TrainingSubsystem->GetTrainingTravelRuntimeCopy();
	TrainingSubsystem->GetMutableRuntimeState().CurrentRegion = UGameXXKMVPRules::RegionHuangshan();
	FGameXXKRouteSettlementReceipt TrainingReceipt;
	Error.Reset();
	TestTrue(TEXT("training challenge route settles to workbench"),
		TrainingSubsystem->SettleAndExitActiveRoute(TrainingReceipt, Error));
	const FGameXXKRuntimeState& SettledTraining = TrainingSubsystem->GetRuntimeState();
	TestEqual(TEXT("training challenge returns to Qingshan workbench region"),
		SettledTraining.CurrentRegion,
		UGameXXKMVPRules::RegionQingshan());
	TestEqual(TEXT("training challenge returns to Town"), SettledTraining.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("training challenge stays on DesktopTrainingHUD"),
		SettledTraining.CurrentMapId,
		FName(TEXT("DesktopTrainingHUD")));
	const FGameXXKTrainingTravelRuntime TravelRuntimeAfter = TrainingSubsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("challenge settlement preserves the independent Travel party HP/runtime bit-identically"),
		FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
			&TravelRuntimeAfter,
			&TravelRuntimeBefore,
			PPF_None));
	ValidationError.Reset();
	TestTrue(
		FString::Printf(TEXT("settled training challenge is immediately save-valid: %s"), *ValidationError),
		FGameXXKSaveMigration::ValidateRuntimeState(SettledTraining, ValidationError));
	return true;
}

#endif
