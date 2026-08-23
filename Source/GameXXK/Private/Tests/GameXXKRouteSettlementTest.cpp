#include "GameXXKRouteSettlementRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteMerchantRules.h"
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

#endif
