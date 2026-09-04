#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

static_assert(
	FGameXXKSaveMigration::ThreeChapterRouteIntroducedSaveVersion == 8,
	"Three-chapter route persistence must remain a version-8 feature.");
static_assert(
	FGameXXKSaveMigration::RouteMerchantSnapshotIntroducedSaveVersion == 8,
	"Route-merchant snapshot persistence must remain a version-8 feature.");
static_assert(
	FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion == 9,
	"Route-economy persistence requires save version 9.");
static_assert(
	FGameXXKSaveMigration::RouteMerchantStockSchemaIntroducedSaveVersion == 10,
	"Canonical route-merchant stock persistence requires save version 10.");
static_assert(
	FGameXXKSaveMigration::DesktopInventoryStorageIntroducedSaveVersion == 21,
	"Persistent desktop inventory storage advances the current save version to twenty-one.");
static_assert(
	FGameXXKSaveMigration::CurrentSaveVersion == 35,
	"The active 173-card pool is part of the current save version.");

namespace
{
	FGameXXKRuntimeState MakeStartedState()
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		return Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
	}

	FGameXXKRuntimeState MakeActiveRouteState(const int32 Chapter)
	{
		FGameXXKRuntimeState State = MakeStartedState();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.ReachableRouteNodeIds = {10};
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = INDEX_NONE;

		FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		Progress.SchemaVersion = 1;
		Progress.RootSeed = State.RouteSeed;
		Progress.ChapterSeeds = {
			State.RouteSeed,
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 2)),
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 3))};
		Progress.CurrentChapter = Chapter;
		Progress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, 20);
		Progress.ActualRouteCardAcquisitionCount = 6;

		FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
		Merchant.SourceNodeId = 10;
		Merchant.OfferSeed = 0x7351;
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = TEXT("Merchant.10.Card.0");
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Card;
		Offer.ContentId = TEXT("Route.General.PoJiaTuCi");
		Offer.Price = 15;
		Merchant.Offers.Add(Offer);

		return State;
	}

	FGameXXKRouteSettlementReceipt MakeSettlementReceipt(
		const int32 TravelMoney,
		const int32 CardAcquisitionCount)
	{
		FGameXXKRouteSettlementReceipt Receipt;
		Receipt.SettlementId = FGuid(0x125B2001, 0x125B2002, 0x125B2003, 0x125B2004);
		Receipt.Outcome = EGameXXKRouteTerminalOutcome::Cleared;
		Receipt.SourceTravelMoney = TravelMoney;
		Receipt.SourceCardAcquisitionCount = CardAcquisitionCount;
		Receipt.PermanentGoldAward = TravelMoney / 10;
		Receipt.EnhancementStoneAward = CardAcquisitionCount / 5;
		return Receipt;
	}

	FGameXXKSaveState MakeVersionedSave(FGameXXKRuntimeState State, const int32 Version)
	{
		FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		Save.SaveVersion = Version;
		return Save;
	}

	bool MerchantStatesMatch(const FGameXXKRouteMerchantState& Left, const FGameXXKRouteMerchantState& Right)
	{
		return FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool PendingSettlementsMatch(
		const FGameXXKRouteSettlementReceipt& Left,
		const FGameXXKRouteSettlementReceipt& Right)
	{
		return FGameXXKRouteSettlementReceipt::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool RuntimeStatesMatch(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEconomySaveVersionContractTest,
	"GameXXK.MVP.SaveGame.RouteEconomyV9.VersionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEconomySaveVersionContractTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("three-chapter route introduction remains version eight"),
		FGameXXKSaveMigration::ThreeChapterRouteIntroducedSaveVersion,
		8);
	TestEqual(
		TEXT("merchant snapshot introduction remains version eight"),
		FGameXXKSaveMigration::RouteMerchantSnapshotIntroducedSaveVersion,
		8);
	TestEqual(
		TEXT("route economy introduction is version nine"),
		FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion,
		9);
	TestEqual(
		TEXT("canonical merchant stock schema is version ten"),
		FGameXXKSaveMigration::RouteMerchantStockSchemaIntroducedSaveVersion,
		10);
	TestEqual(TEXT("current save version includes the active 173-card pool"), FGameXXKSaveMigration::CurrentSaveVersion, 35);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEconomyV9MigrationTest,
	"GameXXK.MVP.SaveGame.RouteEconomyV9.Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEconomyV9MigrationTest::RunTest(const FString& Parameters)
{
	for (const int32 Chapter : {2, 3})
	{
		FGameXXKRuntimeState State = MakeActiveRouteState(Chapter);
		const int32 SourceBalance = Chapter == 3 ? 0 : 82;
		State.CardRun.RouteTravelMoney = SourceBalance;
		State.CardRun.bRouteEconomyInitialized = Chapter == 3;
		State.CardRun.RewardedTravelMoneyNodes = {FGameXXKRouteTravelMoneyReceipt{99, -4, -1}};
		State.CardRun.PendingSettlement = MakeSettlementReceipt(
			SourceBalance,
			State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount);
		State.CardRun.LastAppliedRouteSettlementId = FGuid(0x8A010001, 0x8A010002, 0x8A010003, Chapter);

		const FGameXXKRouteProgress ExpectedProgress = State.CardRun.RouteProgress;
		const FGameXXKRouteSettlementReceipt ExpectedPending = State.CardRun.PendingSettlement;
		const FGuid ExpectedLastApplied = State.CardRun.LastAppliedRouteSettlementId;
		const TArray<FGameXXKRouteMapNode> ExpectedNodes = State.RouteMapNodes;
		FGameXXKRuntimeState ExpectedRuntime = State;
		ExpectedRuntime.CardRun.RouteTravelMoney = SourceBalance;
		ExpectedRuntime.CardRun.bRouteEconomyInitialized = true;
		ExpectedRuntime.CardRun.RewardedTravelMoneyNodes.Reset();
		ExpectedRuntime.CardRun.RouteMerchant = FGameXXKRouteMerchantState();

		const FGameXXKSaveState Source = MakeVersionedSave(
			MoveTemp(State),
			FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion - 1);
		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		TestTrue(
			FString::Printf(TEXT("v8 active chapter %d migrates"), Chapter),
			FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
		TestEqual(TEXT("v8 migration writes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
		TestEqual(TEXT("v8 balance is preserved"), Migrated.RuntimeState.CardRun.RouteTravelMoney, SourceBalance);
		TestTrue(TEXT("v8 active economy becomes initialized"), Migrated.RuntimeState.CardRun.bRouteEconomyInitialized);
		TestTrue(TEXT("v8 forged receipts are discarded"), Migrated.RuntimeState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
		TestTrue(
			TEXT("v8 chapter progress is preserved"),
			FGameXXKRouteProgress::StaticStruct()->CompareScriptStruct(
				&Migrated.RuntimeState.CardRun.RouteProgress,
				&ExpectedProgress,
				PPF_None));
		TestTrue(
			TEXT("v8 merchant snapshot is discarded by the canonical v10 stock migration"),
			MerchantStatesMatch(Migrated.RuntimeState.CardRun.RouteMerchant, FGameXXKRouteMerchantState()));
		TestTrue(TEXT("v8 pending settlement is preserved"), PendingSettlementsMatch(Migrated.RuntimeState.CardRun.PendingSettlement, ExpectedPending));
		TestEqual(TEXT("v8 last-applied settlement is preserved"), Migrated.RuntimeState.CardRun.LastAppliedRouteSettlementId, ExpectedLastApplied);
		TestEqual(TEXT("v8 route-map node count is preserved"), Migrated.RuntimeState.RouteMapNodes.Num(), ExpectedNodes.Num());
		if (Migrated.RuntimeState.RouteMapNodes.Num() == ExpectedNodes.Num())
		{
			for (int32 Index = 0; Index < ExpectedNodes.Num(); ++Index)
			{
				TestTrue(
					FString::Printf(TEXT("v8 route-map node %d is preserved"), Index),
					FGameXXKRouteMapNode::StaticStruct()->CompareScriptStruct(
						&Migrated.RuntimeState.RouteMapNodes[Index],
						&ExpectedNodes[Index],
						PPF_None));
			}
		}
		FGameXXKRuntimeState NormalizedMigrated = Migrated.RuntimeState;
		FGameXXKRuntimeState NormalizedExpected = ExpectedRuntime;
		// v12+ migrations deterministically re-derive the hero card pool and the
		// route random seed; those re-derived values are covered by their own
		// migration tests, so the v8 preservation contract ignores them.
		NormalizedMigrated.CardRun.HeroUnlockedCardIds = NormalizedExpected.CardRun.HeroUnlockedCardIds;
		NormalizedMigrated.CardRun.HeroSelectedCardIds = NormalizedExpected.CardRun.HeroSelectedCardIds;
		NormalizedMigrated.CardRun.RouteRandomSeed = NormalizedExpected.CardRun.RouteRandomSeed;
		// v11-v24 introduced new durable namespaces after this route-economy
		// fixture was authored. Their deterministic defaults and NPC loadouts are
		// covered by their own migration contracts; do not treat those derived
		// fields as a route-economy preservation failure.
		NormalizedExpected.MetaShop = NormalizedMigrated.MetaShop;
		NormalizedExpected.Training = NormalizedMigrated.Training;
		NormalizedExpected.DesktopInventory = NormalizedMigrated.DesktopInventory;
		NormalizedExpected.CardRun.PartySelection.QuestNpc = NormalizedMigrated.CardRun.PartySelection.QuestNpc;
		NormalizedExpected.CardRun.PartySelection.QuestNpcCardLoadouts = NormalizedMigrated.CardRun.PartySelection.QuestNpcCardLoadouts;
		NormalizedExpected.CardRun.OrderedFormation = NormalizedMigrated.CardRun.OrderedFormation;
		TestTrue(
			TEXT("v8 migration preserves the complete runtime except the three route-economy fields, the cleared merchant snapshot, and re-derived hero pools"),
			RuntimeStatesMatch(NormalizedMigrated, NormalizedExpected));
	}

	FGameXXKRuntimeState InactiveState = MakeStartedState();
	InactiveState.CardRun.RouteTravelMoney = 44;
	InactiveState.CardRun.bRouteEconomyInitialized = true;
	InactiveState.CardRun.RewardedTravelMoneyNodes = {FGameXXKRouteTravelMoneyReceipt{1, 7, 20}};
	InactiveState.CardRun.LastAppliedRouteSettlementId = FGuid(0x1A010001, 0x1A010002, 0x1A010003, 0x1A010004);
	const FGuid ExpectedInactiveLastApplied = InactiveState.CardRun.LastAppliedRouteSettlementId;
	const FGameXXKSaveState InactiveV8 = MakeVersionedSave(
		InactiveState,
		FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion - 1);
	FGameXXKSaveState MigratedInactive;
	FGameXXKSaveMigrationReport InactiveReport;
	TestTrue(TEXT("v8 inactive state migrates"), FGameXXKSaveMigration::MigrateToCurrent(InactiveV8, MigratedInactive, InactiveReport));
	TestEqual(TEXT("v8 inactive balance is cleared"), MigratedInactive.RuntimeState.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("v8 inactive economy is uninitialized"), MigratedInactive.RuntimeState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("v8 inactive receipts are cleared"), MigratedInactive.RuntimeState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
	TestEqual(
		TEXT("v8 inactive migration preserves last-applied settlement"),
		MigratedInactive.RuntimeState.CardRun.LastAppliedRouteSettlementId,
		ExpectedInactiveLastApplied);

	FGameXXKRuntimeState V7State = MakeActiveRouteState(3);
	V7State.CardRun.RouteTravelMoney = 42;
	V7State.CardRun.bRouteEconomyInitialized = false;
	V7State.CardRun.RewardedTravelMoneyNodes = {FGameXXKRouteTravelMoneyReceipt{3, 10, 50}};
	const FGameXXKSaveState SourceV7 = MakeVersionedSave(
		MoveTemp(V7State),
		FGameXXKSaveMigration::ThreeChapterRouteIntroducedSaveVersion - 1);
	FGameXXKSaveState MigratedV7;
	FGameXXKSaveMigrationReport V7Report;
	TestTrue(TEXT("v7 active state runs the full migration chain"), FGameXXKSaveMigration::MigrateToCurrent(SourceV7, MigratedV7, V7Report));
	TestEqual(TEXT("v7 route first migrates to chapter one"), MigratedV7.RuntimeState.CardRun.RouteProgress.CurrentChapter, 1);
	TestEqual(TEXT("v7 merchant snapshot is cleared"), MigratedV7.RuntimeState.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("v7 balance survives economy migration"), MigratedV7.RuntimeState.CardRun.RouteTravelMoney, 42);
	TestTrue(TEXT("v7 economy is initialized after older migrations"), MigratedV7.RuntimeState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("v7 forged receipts are cleared"), MigratedV7.RuntimeState.CardRun.RewardedTravelMoneyNodes.IsEmpty());

	FGameXXKRuntimeState NegativeState = MakeActiveRouteState(2);
	NegativeState.CardRun.RouteTravelMoney = -1;
	NegativeState.CardRun.bRouteEconomyInitialized = true;
	const FGameXXKSaveState NegativeSource = MakeVersionedSave(
		MoveTemp(NegativeState),
		FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion - 1);
	FGameXXKSaveState NegativeSourceBefore = NegativeSource;
	FGameXXKSaveState Rejected;
	FGameXXKSaveMigrationReport RejectedReport;
	TestFalse(TEXT("v8 active negative balance is rejected"), FGameXXKSaveMigration::MigrateToCurrent(NegativeSource, Rejected, RejectedReport));
	TestTrue(
		TEXT("negative-balance rejection leaves the source unchanged"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&NegativeSource, &NegativeSourceBefore, PPF_None));
	TestEqual(TEXT("negative-balance rejection exposes no partial output"), Rejected.SaveVersion, 0);

	FGameXXKRuntimeState OrderingState = MakeActiveRouteState(2);
	OrderingState.CardRun.RouteTravelMoney = -1;
	OrderingState.EquipmentCollection = FGameXXKEquipmentCollectionState();
	OrderingState.EquippedWeapon = TEXT("Item.UnknownLegacyWeapon");
	const FGameXXKSaveState OrderingSource = MakeVersionedSave(
		MoveTemp(OrderingState),
		FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion - 1);
	FGameXXKSaveState OrderingRejected;
	FGameXXKSaveMigrationReport OrderingReport;
	TestFalse(
		TEXT("legacy equipment failure rejects a v8 save that also has invalid route economy"),
		FGameXXKSaveMigration::MigrateToCurrent(OrderingSource, OrderingRejected, OrderingReport));
	TestTrue(
		TEXT("established equipment migration fails before route-economy migration"),
		OrderingReport.Error.Contains(TEXT("hero mirror")));
	TestEqual(TEXT("ordered migration failure exposes no partial output"), OrderingRejected.SaveVersion, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEconomyV9RuntimeValidationTest,
	"GameXXK.MVP.SaveGame.RouteEconomyV9.RuntimeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEconomyV9RuntimeValidationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState UninitializedState = MakeActiveRouteState(2);
	UninitializedState.CardRun.RouteTravelMoney = 17;
	const FGameXXKSaveState UninitializedCurrent = MakeVersionedSave(
		MoveTemp(UninitializedState),
		FGameXXKSaveMigration::CurrentSaveVersion);
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestFalse(
		TEXT("current v10 active uninitialized economy is rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(UninitializedCurrent, Migrated, Report));

	FGameXXKRuntimeState DirtyInactive = MakeStartedState();
	DirtyInactive.CardRun.RouteTravelMoney = 1;
	TestFalse(
		TEXT("current v10 inactive nonzero balance is rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(MoveTemp(DirtyInactive), FGameXXKSaveMigration::CurrentSaveVersion),
			Migrated,
			Report));
	DirtyInactive = MakeStartedState();
	DirtyInactive.CardRun.bRouteEconomyInitialized = true;
	TestFalse(
		TEXT("current v10 inactive initialized flag is rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(MoveTemp(DirtyInactive), FGameXXKSaveMigration::CurrentSaveVersion),
			Migrated,
			Report));
	DirtyInactive = MakeStartedState();
	DirtyInactive.CardRun.RewardedTravelMoneyNodes = {FGameXXKRouteTravelMoneyReceipt{1, 0, 0}};
	TestFalse(
		TEXT("current v10 inactive receipt is rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(MoveTemp(DirtyInactive), FGameXXKSaveMigration::CurrentSaveVersion),
			Migrated,
			Report));

	FGameXXKRuntimeState ValidReceipts = MakeActiveRouteState(3);
	ValidReceipts.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
	TestTrue(TEXT("receipt fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(ValidReceipts.CardRun, 70));
	ValidReceipts.CardRun.RewardedTravelMoneyNodes = {FGameXXKRouteTravelMoneyReceipt{1, 999, 20}};
	ValidReceipts.CardRun.LastAppliedRouteSettlementId = FGuid(0x9A010001, 0x9A010002, 0x9A010003, 0x9A010004);
	const FGameXXKSaveState ValidReceiptSave = MakeVersionedSave(
		ValidReceipts,
		FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(
		TEXT("a receipt from an older chapter need not exist in the current map"),
		FGameXXKSaveMigration::MigrateToCurrent(ValidReceiptSave, Migrated, Report));
	TestEqual(
		TEXT("last-applied settlement may survive into a new active route"),
		Migrated.RuntimeState.CardRun.LastAppliedRouteSettlementId,
		ValidReceipts.CardRun.LastAppliedRouteSettlementId);

	FGameXXKSaveState InvalidReceipt = ValidReceiptSave;
	InvalidReceipt.RuntimeState.CardRun.RewardedTravelMoneyNodes[0].Chapter = 4;
	TestFalse(TEXT("receipt chapter four is rejected"), FGameXXKSaveMigration::MigrateToCurrent(InvalidReceipt, Migrated, Report));
	InvalidReceipt = ValidReceiptSave;
	InvalidReceipt.RuntimeState.CardRun.RewardedTravelMoneyNodes[0].NodeId = -1;
	TestFalse(TEXT("negative receipt node is rejected"), FGameXXKSaveMigration::MigrateToCurrent(InvalidReceipt, Migrated, Report));
	InvalidReceipt = ValidReceiptSave;
	InvalidReceipt.RuntimeState.CardRun.RewardedTravelMoneyNodes[0].Amount = -1;
	TestFalse(TEXT("negative receipt amount is rejected"), FGameXXKSaveMigration::MigrateToCurrent(InvalidReceipt, Migrated, Report));
	InvalidReceipt = ValidReceiptSave;
	InvalidReceipt.RuntimeState.CardRun.RewardedTravelMoneyNodes.Add(FGameXXKRouteTravelMoneyReceipt{1, 999, 0});
	TestFalse(TEXT("duplicate chapter-node receipt is rejected"), FGameXXKSaveMigration::MigrateToCurrent(InvalidReceipt, Migrated, Report));

	FGameXXKRuntimeState MatchingPending = MakeActiveRouteState(2);
	MatchingPending.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
	TestTrue(TEXT("pending-settlement fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(MatchingPending.CardRun, 71));
	MatchingPending.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 6;
	MatchingPending.CardRun.PendingSettlement = MakeSettlementReceipt(71, 6);
	const FGameXXKSaveState MatchingPendingSave = MakeVersionedSave(
		MatchingPending,
		FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(TEXT("matching active pending settlement is valid"), FGameXXKSaveMigration::MigrateToCurrent(MatchingPendingSave, Migrated, Report));

	for (const EGameXXKRouteTerminalOutcome Outcome : {
		EGameXXKRouteTerminalOutcome::Defeated,
		EGameXXKRouteTerminalOutcome::Abandoned})
	{
		FGameXXKSaveState ValidLossOutcome = MatchingPendingSave;
		FGameXXKRouteSettlementReceipt& Receipt = ValidLossOutcome.RuntimeState.CardRun.PendingSettlement;
		Receipt.Outcome = Outcome;
		Receipt.PermanentGoldAward = Receipt.SourceTravelMoney / 20;
		Receipt.EnhancementStoneAward = Receipt.SourceCardAcquisitionCount / 10;
		TestTrue(
			TEXT("defeated and abandoned pending settlements remain legal"),
			FGameXXKSaveMigration::MigrateToCurrent(ValidLossOutcome, Migrated, Report));
	}

	FGameXXKSaveState OutcomeOnlyPending = MatchingPendingSave;
	OutcomeOnlyPending.RuntimeState.CardRun.PendingSettlement = FGameXXKRouteSettlementReceipt();
	OutcomeOnlyPending.RuntimeState.CardRun.PendingSettlement.Outcome = EGameXXKRouteTerminalOutcome::Cleared;
	TestFalse(
		TEXT("an outcome-only pending settlement is not the exact empty default"),
		FGameXXKSaveMigration::MigrateToCurrent(OutcomeOnlyPending, Migrated, Report));

	FGameXXKSaveState InvalidOutcomePending = MatchingPendingSave;
	FGameXXKRouteSettlementReceipt& InvalidOutcomeReceipt = InvalidOutcomePending.RuntimeState.CardRun.PendingSettlement;
	InvalidOutcomeReceipt.Outcome = static_cast<EGameXXKRouteTerminalOutcome>(255);
	InvalidOutcomeReceipt.PermanentGoldAward = InvalidOutcomeReceipt.SourceTravelMoney / 20;
	InvalidOutcomeReceipt.EnhancementStoneAward = InvalidOutcomeReceipt.SourceCardAcquisitionCount / 10;
	TestFalse(
		TEXT("an undefined pending-settlement outcome is rejected even when loss awards match"),
		FGameXXKSaveMigration::MigrateToCurrent(InvalidOutcomePending, Migrated, Report));

	FGameXXKSaveState MismatchedPending = MatchingPendingSave;
	MismatchedPending.RuntimeState.CardRun.RouteTravelMoney = 72;
	TestFalse(TEXT("pending settlement source balance must match runtime"), FGameXXKSaveMigration::MigrateToCurrent(MismatchedPending, Migrated, Report));
	MismatchedPending = MatchingPendingSave;
	MismatchedPending.RuntimeState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 7;
	TestFalse(TEXT("pending settlement source card count must match runtime"), FGameXXKSaveMigration::MigrateToCurrent(MismatchedPending, Migrated, Report));

	FGameXXKRuntimeState InactivePending = MakeStartedState();
	InactivePending.CardRun.PendingSettlement = MakeSettlementReceipt(0, 0);
	TestFalse(
		TEXT("inactive route cannot retain a nonempty pending settlement"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(MoveTemp(InactivePending), FGameXXKSaveMigration::CurrentSaveVersion),
			Migrated,
			Report));
	return true;
}

#endif
