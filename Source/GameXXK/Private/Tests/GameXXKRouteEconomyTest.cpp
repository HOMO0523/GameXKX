#include "GameXXKCardRunTypes.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool AreCardRunsEqual(const FGameXXKCardRunState& Left, const FGameXXKCardRunState& Right)
	{
		return FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	void TestSaveGameProperty(
		FAutomationTestBase& Test,
		const UStruct* Struct,
		const FName PropertyName,
		const TCHAR* Context)
	{
		const FProperty* Property = Struct ? Struct->FindPropertyByName(PropertyName) : nullptr;
		Test.TestNotNull(FString::Printf(TEXT("%s is reflected"), Context), Property);
		if (Property)
		{
			Test.TestTrue(
				FString::Printf(TEXT("%s is marked SaveGame"), Context),
				Property->HasAnyPropertyFlags(CPF_SaveGame));
		}
	}

	void TestRejectedAwardIsAtomic(
		FAutomationTestBase& Test,
		FGameXXKCardRunState& CardRun,
		const int32 Chapter,
		const int32 NodeId,
		const int32 Amount,
		const TCHAR* Context)
	{
		const FGameXXKCardRunState Before = CardRun;
		bool bAwarded = true;
		FString Error;
		Test.TestFalse(
			FString::Printf(TEXT("%s is rejected"), Context),
			FGameXXKRouteEconomyRules::AwardNodeOnce(
				CardRun,
				Chapter,
				NodeId,
				Amount,
				bAwarded,
				&Error));
		Test.TestFalse(FString::Printf(TEXT("%s reports no award"), Context), bAwarded);
		Test.TestTrue(
			FString::Printf(TEXT("%s leaves every card-run field unchanged"), Context),
			AreCardRunsEqual(CardRun, Before));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEconomyRulesTest,
	"GameXXK.Route.Economy.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEconomyRulesTest::RunTest(const FString& Parameters)
{
	{
		const FGameXXKRouteTravelMoneyReceipt Receipt;
		TestEqual(TEXT("a receipt defaults to no chapter"), Receipt.Chapter, INDEX_NONE);
		TestEqual(TEXT("a receipt defaults to no node"), Receipt.NodeId, INDEX_NONE);
		TestEqual(TEXT("a receipt defaults to zero amount"), Receipt.Amount, 0);

		const FGameXXKCardRunState CardRun;
		TestEqual(TEXT("route travel money defaults to zero"), CardRun.RouteTravelMoney, 0);
		TestFalse(TEXT("route economy defaults to uninitialized"), CardRun.bRouteEconomyInitialized);
		TestTrue(TEXT("rewarded-node receipts default empty"), CardRun.RewardedTravelMoneyNodes.IsEmpty());

		const UScriptStruct* ReceiptStruct = FGameXXKRouteTravelMoneyReceipt::StaticStruct();
		TestNotNull(TEXT("the route travel-money receipt is reflected"), ReceiptStruct);
		if (ReceiptStruct)
		{
			TestTrue(
				TEXT("the route travel-money receipt is BlueprintType"),
				ReceiptStruct->GetBoolMetaData(TEXT("BlueprintType")));
			TestSaveGameProperty(
				*this,
				ReceiptStruct,
				GET_MEMBER_NAME_CHECKED(FGameXXKRouteTravelMoneyReceipt, Chapter),
				TEXT("receipt Chapter"));
			TestSaveGameProperty(
				*this,
				ReceiptStruct,
				GET_MEMBER_NAME_CHECKED(FGameXXKRouteTravelMoneyReceipt, NodeId),
				TEXT("receipt NodeId"));
			TestSaveGameProperty(
				*this,
				ReceiptStruct,
				GET_MEMBER_NAME_CHECKED(FGameXXKRouteTravelMoneyReceipt, Amount),
				TEXT("receipt Amount"));
		}

		const UScriptStruct* CardRunStruct = FGameXXKCardRunState::StaticStruct();
		TestSaveGameProperty(
			*this,
			CardRunStruct,
			GET_MEMBER_NAME_CHECKED(FGameXXKCardRunState, RouteTravelMoney),
			TEXT("card-run RouteTravelMoney"));
		TestSaveGameProperty(
			*this,
			CardRunStruct,
			GET_MEMBER_NAME_CHECKED(FGameXXKCardRunState, bRouteEconomyInitialized),
			TEXT("card-run bRouteEconomyInitialized"));
		TestSaveGameProperty(
			*this,
			CardRunStruct,
			GET_MEMBER_NAME_CHECKED(FGameXXKCardRunState, RewardedTravelMoneyNodes),
			TEXT("card-run RewardedTravelMoneyNodes"));
	}

	{
		FGameXXKCardRunState CardRun;
		CardRun.RouteTravelMoney = 999;
		FGameXXKRouteTravelMoneyReceipt StaleReceipt;
		StaleReceipt.Chapter = 3;
		StaleReceipt.NodeId = 41;
		StaleReceipt.Amount = 50;
		CardRun.RewardedTravelMoneyNodes.Add(StaleReceipt);

		TestTrue(
			TEXT("first initialization accepts the default starting balance"),
			FGameXXKRouteEconomyRules::InitializeRoute(CardRun));
		TestTrue(TEXT("first initialization records the initialized state"), CardRun.bRouteEconomyInitialized);
		TestEqual(TEXT("first initialization sets exactly sixty travel money"), CardRun.RouteTravelMoney, 60);
		TestTrue(TEXT("first initialization clears stale rewarded-node receipts"), CardRun.RewardedTravelMoneyNodes.IsEmpty());

		bool bAwarded = false;
		FString Error;
		TestTrue(
			TEXT("an initialized route can record a receipt before idempotent reinitialization"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 1, 7, 20, bAwarded, &Error));
		TestTrue(TEXT("the setup node was awarded"), bAwarded);
		const FGameXXKCardRunState BeforeSecondInitialization = CardRun;
		TestTrue(
			TEXT("reinitializing a valid route succeeds as a no-op"),
			FGameXXKRouteEconomyRules::InitializeRoute(CardRun, 999, &Error));
		TestTrue(
			TEXT("reinitializing never replaces the live balance or receipts"),
			AreCardRunsEqual(CardRun, BeforeSecondInitialization));

		const FGameXXKCardRunState BeforeInitializedNegativeStart = CardRun;
		TestFalse(
			TEXT("an initialized route still rejects a negative starting-balance argument"),
			FGameXXKRouteEconomyRules::InitializeRoute(CardRun, -1, &Error));
		TestTrue(
			TEXT("a rejected initialized negative argument leaves every field unchanged"),
			AreCardRunsEqual(CardRun, BeforeInitializedNegativeStart));

		FGameXXKCardRunState NegativeStart;
		const FGameXXKCardRunState BeforeNegativeStart = NegativeStart;
		TestFalse(
			TEXT("a negative initial balance is rejected"),
			FGameXXKRouteEconomyRules::InitializeRoute(NegativeStart, -1, &Error));
		TestTrue(
			TEXT("a rejected initial balance leaves every field unchanged"),
			AreCardRunsEqual(NegativeStart, BeforeNegativeStart));

		FGameXXKCardRunState InvalidInitialized = CardRun;
		InvalidInitialized.RouteTravelMoney = -1;
		const FGameXXKCardRunState BeforeInvalidReinitialization = InvalidInitialized;
		TestFalse(
			TEXT("reinitializing an invalid initialized economy is rejected"),
			FGameXXKRouteEconomyRules::InitializeRoute(InvalidInitialized, 60, &Error));
		TestTrue(
			TEXT("invalid reinitialization is atomic"),
			AreCardRunsEqual(InvalidInitialized, BeforeInvalidReinitialization));

		FGameXXKCardRunState MalformedReceiptState;
		MalformedReceiptState.bRouteEconomyInitialized = true;
		MalformedReceiptState.RouteTravelMoney = 80;
		FGameXXKRouteTravelMoneyReceipt MalformedReceipt;
		MalformedReceipt.Chapter = 0;
		MalformedReceipt.NodeId = 3;
		MalformedReceipt.Amount = 20;
		MalformedReceiptState.RewardedTravelMoneyNodes.Add(MalformedReceipt);
		const FGameXXKCardRunState BeforeMalformedReceiptReinitialization = MalformedReceiptState;
		TestFalse(
			TEXT("reinitializing a state with a malformed receipt is rejected"),
			FGameXXKRouteEconomyRules::InitializeRoute(MalformedReceiptState, 60, &Error));
		TestTrue(
			TEXT("malformed-receipt reinitialization is atomic"),
			AreCardRunsEqual(MalformedReceiptState, BeforeMalformedReceiptReinitialization));

		FGameXXKCardRunState DuplicateReceiptState;
		DuplicateReceiptState.bRouteEconomyInitialized = true;
		DuplicateReceiptState.RouteTravelMoney = 100;
		FGameXXKRouteTravelMoneyReceipt DuplicateReceipt;
		DuplicateReceipt.Chapter = 2;
		DuplicateReceipt.NodeId = 9;
		DuplicateReceipt.Amount = 35;
		DuplicateReceiptState.RewardedTravelMoneyNodes.Add(DuplicateReceipt);
		DuplicateReceiptState.RewardedTravelMoneyNodes.Add(DuplicateReceipt);
		const FGameXXKCardRunState BeforeDuplicateReceiptReinitialization = DuplicateReceiptState;
		TestFalse(
			TEXT("reinitializing a state with duplicate chapter-node receipts is rejected"),
			FGameXXKRouteEconomyRules::InitializeRoute(DuplicateReceiptState, 60, &Error));
		TestTrue(
			TEXT("duplicate-receipt reinitialization is atomic"),
			AreCardRunsEqual(DuplicateReceiptState, BeforeDuplicateReceiptReinitialization));
	}

	{
		FGameXXKCardRunState CardRun;
		FString Error;
		TestTrue(TEXT("award fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(CardRun, 60, &Error));

		bool bAwarded = false;
		TestTrue(
			TEXT("chapter one node zero awards once"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 1, 0, 20, bAwarded, &Error));
		TestTrue(TEXT("a new chapter-one key reports an award"), bAwarded);
		TestEqual(TEXT("chapter one award increases the balance"), CardRun.RouteTravelMoney, 80);
		TestEqual(TEXT("chapter one award records one receipt"), CardRun.RewardedTravelMoneyNodes.Num(), 1);
		if (CardRun.RewardedTravelMoneyNodes.Num() == 1)
		{
			const FGameXXKRouteTravelMoneyReceipt& Receipt = CardRun.RewardedTravelMoneyNodes[0];
			TestEqual(TEXT("chapter one receipt stores its chapter"), Receipt.Chapter, 1);
			TestEqual(TEXT("chapter one receipt stores its node"), Receipt.NodeId, 0);
			TestEqual(TEXT("chapter one receipt stores the original amount"), Receipt.Amount, 20);
		}

		bAwarded = true;
		const FGameXXKCardRunState BeforeDuplicate = CardRun;
		TestTrue(
			TEXT("the same chapter and node key succeeds as a no-op"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 1, 0, 999, bAwarded, &Error));
		TestFalse(TEXT("a repeated key reports no award"), bAwarded);
		TestTrue(TEXT("a different repeated amount cannot change any field"), AreCardRunsEqual(CardRun, BeforeDuplicate));

		TestTrue(
			TEXT("chapter two may reuse node zero"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 2, 0, 35, bAwarded, &Error));
		TestTrue(TEXT("chapter two reused node key is newly awarded"), bAwarded);
		TestTrue(
			TEXT("chapter three may reuse node zero"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 3, 0, 50, bAwarded, &Error));
		TestTrue(TEXT("chapter three reused node key is newly awarded"), bAwarded);
		TestEqual(TEXT("the three chapter-scoped awards all accumulate"), CardRun.RouteTravelMoney, 165);
		TestEqual(TEXT("the same node ID in three chapters produces three receipts"), CardRun.RewardedTravelMoneyNodes.Num(), 3);
		if (CardRun.RewardedTravelMoneyNodes.Num() == 3)
		{
			TestEqual(TEXT("the second receipt belongs to chapter two"), CardRun.RewardedTravelMoneyNodes[1].Chapter, 2);
			TestEqual(TEXT("the third receipt belongs to chapter three"), CardRun.RewardedTravelMoneyNodes[2].Chapter, 3);
		}

		TestTrue(
			TEXT("a zero-value new key succeeds"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(CardRun, 3, 1, 0, bAwarded, &Error));
		TestTrue(TEXT("a zero-value new key still reports an award"), bAwarded);
		TestEqual(TEXT("a zero-value key does not change the balance"), CardRun.RouteTravelMoney, 165);
		TestEqual(TEXT("a zero-value key still records a receipt"), CardRun.RewardedTravelMoneyNodes.Num(), 4);
		if (CardRun.RewardedTravelMoneyNodes.Num() == 4)
		{
			TestEqual(TEXT("the zero-value receipt stores zero"), CardRun.RewardedTravelMoneyNodes[3].Amount, 0);
		}
	}

	{
		FGameXXKCardRunState Uninitialized;
		TestRejectedAwardIsAtomic(*this, Uninitialized, 1, 0, 20, TEXT("an uninitialized award"));

		FGameXXKCardRunState Initialized;
		FString Error;
		TestTrue(TEXT("invalid-input fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(Initialized, 60, &Error));
		TestRejectedAwardIsAtomic(*this, Initialized, 0, 0, 20, TEXT("chapter zero"));
		TestRejectedAwardIsAtomic(*this, Initialized, 4, 0, 20, TEXT("chapter four"));
		TestRejectedAwardIsAtomic(*this, Initialized, 1, INDEX_NONE, 20, TEXT("a negative node"));
		TestRejectedAwardIsAtomic(*this, Initialized, 1, 0, -1, TEXT("a negative award"));

		FGameXXKCardRunState NegativeBalance = Initialized;
		NegativeBalance.RouteTravelMoney = -1;
		TestRejectedAwardIsAtomic(*this, NegativeBalance, 1, 0, 1, TEXT("an invalid negative balance"));

		FGameXXKCardRunState Overflow;
		TestTrue(
			TEXT("overflow fixture accepts the maximum legal initial balance"),
			FGameXXKRouteEconomyRules::InitializeRoute(Overflow, MAX_int32, &Error));
		TestRejectedAwardIsAtomic(*this, Overflow, 1, 0, 1, TEXT("an overflowing award"));
		TestEqual(TEXT("the rejected overflow never wraps the balance"), Overflow.RouteTravelMoney, MAX_int32);

		bool bAwarded = false;
		TestTrue(
			TEXT("a zero award at maximum balance records safely"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(Overflow, 1, 0, 0, bAwarded, &Error));
		TestTrue(TEXT("the maximum-balance zero receipt is newly awarded"), bAwarded);
		const FGameXXKCardRunState BeforeMaximumDuplicate = Overflow;
		TestTrue(
			TEXT("a duplicate key does not attempt overflowing arithmetic"),
			FGameXXKRouteEconomyRules::AwardNodeOnce(Overflow, 1, 0, MAX_int32, bAwarded, &Error));
		TestFalse(TEXT("the maximum duplicate reports no award"), bAwarded);
		TestTrue(
			TEXT("the maximum duplicate remains a full no-op"),
			AreCardRunsEqual(Overflow, BeforeMaximumDuplicate));
	}

	{
		FGameXXKCardRunState CardRun;
		FString Error;
		TestTrue(TEXT("spend fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(CardRun, 10, &Error));
		TestFalse(TEXT("a negative amount is never affordable"), FGameXXKRouteEconomyRules::CanAfford(CardRun, -1));
		TestTrue(TEXT("zero is affordable with a legal balance"), FGameXXKRouteEconomyRules::CanAfford(CardRun, 0));
		TestTrue(TEXT("the exact balance is affordable"), FGameXXKRouteEconomyRules::CanAfford(CardRun, 10));
		TestFalse(TEXT("more than the balance is not affordable"), FGameXXKRouteEconomyRules::CanAfford(CardRun, 11));

		const FGameXXKCardRunState BeforeZeroSpend = CardRun;
		TestTrue(TEXT("spending zero succeeds"), FGameXXKRouteEconomyRules::Spend(CardRun, 0, &Error));
		TestTrue(TEXT("spending zero changes no fields"), AreCardRunsEqual(CardRun, BeforeZeroSpend));

		const FGameXXKCardRunState BeforeNegativeSpend = CardRun;
		TestFalse(TEXT("spending a negative amount is rejected"), FGameXXKRouteEconomyRules::Spend(CardRun, -1, &Error));
		TestTrue(TEXT("a negative spend is atomic"), AreCardRunsEqual(CardRun, BeforeNegativeSpend));

		const FGameXXKCardRunState BeforeInsufficientSpend = CardRun;
		TestFalse(TEXT("spending beyond the balance is rejected"), FGameXXKRouteEconomyRules::Spend(CardRun, 11, &Error));
		TestTrue(TEXT("an insufficient spend is atomic"), AreCardRunsEqual(CardRun, BeforeInsufficientSpend));

		TestTrue(TEXT("spending the exact balance succeeds"), FGameXXKRouteEconomyRules::Spend(CardRun, 10, &Error));
		TestEqual(TEXT("spending the exact balance reaches zero"), CardRun.RouteTravelMoney, 0);
		TestTrue(TEXT("zero remains affordable at zero balance"), FGameXXKRouteEconomyRules::CanAfford(CardRun, 0));

		FGameXXKCardRunState InvalidBalance = CardRun;
		InvalidBalance.RouteTravelMoney = -1;
		TestFalse(TEXT("zero is not affordable against an invalid negative balance"), FGameXXKRouteEconomyRules::CanAfford(InvalidBalance, 0));
		const FGameXXKCardRunState BeforeInvalidBalanceSpend = InvalidBalance;
		TestFalse(TEXT("spending against an invalid balance is rejected"), FGameXXKRouteEconomyRules::Spend(InvalidBalance, 0, &Error));
		TestTrue(TEXT("an invalid-balance spend is atomic"), AreCardRunsEqual(InvalidBalance, BeforeInvalidBalanceSpend));

		FGameXXKCardRunState MaximumBalance;
		TestTrue(TEXT("maximum spend fixture initializes"), FGameXXKRouteEconomyRules::InitializeRoute(MaximumBalance, MAX_int32, &Error));
		TestTrue(TEXT("the maximum int32 amount is affordable exactly"), FGameXXKRouteEconomyRules::CanAfford(MaximumBalance, MAX_int32));
		TestTrue(TEXT("spending the maximum int32 amount succeeds"), FGameXXKRouteEconomyRules::Spend(MaximumBalance, MAX_int32, &Error));
		TestEqual(TEXT("the maximum int32 subtraction cannot underflow"), MaximumBalance.RouteTravelMoney, 0);

		FGameXXKCardRunState UninitializedCompatibleBalance;
		UninitializedCompatibleBalance.RouteTravelMoney = 5;
		TestFalse(
			TEXT("the legacy-compatible balance fixture remains uninitialized"),
			UninitializedCompatibleBalance.bRouteEconomyInitialized);
		TestTrue(
			TEXT("CanAfford reads a legal balance before migration initialization"),
			FGameXXKRouteEconomyRules::CanAfford(UninitializedCompatibleBalance, 5));
		TestTrue(
			TEXT("Spend can deduct a legal balance before migration initialization"),
			FGameXXKRouteEconomyRules::Spend(UninitializedCompatibleBalance, 3, &Error));
		TestEqual(
			TEXT("the pre-initialization spend deducts exactly the requested amount"),
			UninitializedCompatibleBalance.RouteTravelMoney,
			2);
		TestFalse(
			TEXT("a pre-initialization spend does not toggle the initialization flag"),
			UninitializedCompatibleBalance.bRouteEconomyInitialized);
		const FGameXXKCardRunState BeforeRejectedUninitializedSpend = UninitializedCompatibleBalance;
		TestFalse(
			TEXT("an unaffordable pre-initialization spend is rejected"),
			FGameXXKRouteEconomyRules::Spend(UninitializedCompatibleBalance, 3, &Error));
		TestTrue(
			TEXT("a rejected pre-initialization spend is fully atomic"),
			AreCardRunsEqual(UninitializedCompatibleBalance, BeforeRejectedUninitializedSpend));
	}

	{
		FGameXXKRuntimeState State;
		State.PlayerGold = 777;
		State.CardRun.RouteTravelMoney = 123;
		State.CardRun.bRouteEconomyInitialized = true;
		FGameXXKRouteTravelMoneyReceipt NodeReceipt;
		NodeReceipt.Chapter = 2;
		NodeReceipt.NodeId = 9;
		NodeReceipt.Amount = 35;
		State.CardRun.RewardedTravelMoneyNodes.Add(NodeReceipt);
		State.CardRun.RouteProgress.SchemaVersion = 7;
		State.CardRun.RouteProgress.RootSeed = 12345;
		State.CardRun.RouteProgress.ChapterSeeds = {11, 22, 33};
		State.CardRun.RouteProgress.CurrentChapter = 2;
		State.CardRun.RouteProgress.RouteCombatLevel = 8;
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 6;
		State.CardRun.PendingSettlement.SettlementId = FGuid::NewGuid();
		State.CardRun.PendingSettlement.Outcome = EGameXXKRouteTerminalOutcome::Cleared;
		State.CardRun.PendingSettlement.SourceTravelMoney = 123;
		State.CardRun.PendingSettlement.SourceCardAcquisitionCount = 6;
		State.CardRun.PendingSettlement.PermanentGoldAward = 12;
		State.CardRun.PendingSettlement.EnhancementStoneAward = 1;
		State.CardRun.LastAppliedRouteSettlementId = FGuid::NewGuid();
		State.CardRun.RouteRandomSeed = 7654;
		State.CardRun.NextRewardOrdinal = 99;
		State.CardRun.RouteMerchant.SourceNodeId = 17;
		State.CardRun.RouteMerchant.OfferSeed = 818;
		FGameXXKRouteMerchantOffer MerchantOffer;
		MerchantOffer.OfferId = TEXT("Route.Test.Offer");
		MerchantOffer.Kind = EGameXXKRouteMerchantOfferKind::Card;
		MerchantOffer.ContentId = TEXT("Route.Test.Card");
		MerchantOffer.Price = 25;
		State.CardRun.RouteMerchant.Offers.Add(MerchantOffer);
		State.CardRun.RouteCardIds = {TEXT("Route.Test.LegacyCard")};
		FGameXXKRouteCardEntry CardEntry;
		CardEntry.EntryId = TEXT("Route.Test.Entry");
		CardEntry.CardId = TEXT("Route.Test.Card");
		CardEntry.CurrentQuality = EGameXXKCardQuality::Rare;
		CardEntry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
		CardEntry.bTemporaryRouteCard = true;
		CardEntry.AcquisitionOrdinal = 5;
		State.CardRun.RouteCardEntries.Add(CardEntry);
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = 23;
		State.CardRun.ActiveBattle.RoundNumber = 4;
		State.CardRun.ActiveBattle.NextModifierOrdinal = 9;
		State.CardRun.PendingReward.SourceNodeId = 29;
		State.CardRun.PendingReward.ChoiceSeed = 303;
		State.CardRun.PendingEvent.SourceNodeId = 31;
		State.CardRun.PendingEvent.ChoiceSeed = 404;
		FGameXXKRelicInstance Relic;
		Relic.RelicId = TEXT("Relic.Test.ClearSentinel");
		Relic.Stacks = 2;
		Relic.AcquisitionOrdinal = 7;
		State.CardRun.Relics.Add(Relic);
		State.CardRun.PendingRelicOffer.SourceNodeId = 37;
		State.CardRun.PendingRelicOffer.ChoiceSeed = 505;
		State.CardRun.RouteAttributeBonuses.Attack = 6;

		const int32 PlayerGoldBefore = State.PlayerGold;
		const FGameXXKRouteProgress RouteProgressBefore = State.CardRun.RouteProgress;
		const FGameXXKRouteSettlementReceipt PendingSettlementBefore = State.CardRun.PendingSettlement;
		const FGuid LastAppliedBefore = State.CardRun.LastAppliedRouteSettlementId;
		const int32 RouteRandomSeedBefore = State.CardRun.RouteRandomSeed;
		const int32 NextRewardOrdinalBefore = State.CardRun.NextRewardOrdinal;
		FGameXXKCardRunState ExpectedAfterClear = State.CardRun;
		ExpectedAfterClear.RouteTravelMoney = 0;
		ExpectedAfterClear.bRouteEconomyInitialized = false;
		ExpectedAfterClear.RewardedTravelMoneyNodes.Reset();

		FGameXXKRouteEconomyRules::ClearRouteEconomy(State.CardRun);

		TestEqual(TEXT("clear resets only the travel-money balance"), State.CardRun.RouteTravelMoney, 0);
		TestFalse(TEXT("clear resets the route-economy initialization flag"), State.CardRun.bRouteEconomyInitialized);
		TestTrue(TEXT("clear removes rewarded-node receipts"), State.CardRun.RewardedTravelMoneyNodes.IsEmpty());
		TestEqual(TEXT("clear preserves permanent player gold"), State.PlayerGold, PlayerGoldBefore);
		TestTrue(
			TEXT("clear preserves the terminal settlement receipt"),
			FGameXXKRouteSettlementReceipt::StaticStruct()->CompareScriptStruct(
				&State.CardRun.PendingSettlement,
				&PendingSettlementBefore,
				PPF_None));
		TestEqual(
			TEXT("clear preserves the terminal settlement idempotency key"),
			State.CardRun.LastAppliedRouteSettlementId,
			LastAppliedBefore);
		TestTrue(
			TEXT("clear preserves route progress"),
			FGameXXKRouteProgress::StaticStruct()->CompareScriptStruct(
				&State.CardRun.RouteProgress,
				&RouteProgressBefore,
				PPF_None));
		TestEqual(TEXT("clear preserves unrelated route seed state"), State.CardRun.RouteRandomSeed, RouteRandomSeedBefore);
		TestEqual(TEXT("clear preserves unrelated reward ordinal state"), State.CardRun.NextRewardOrdinal, NextRewardOrdinalBefore);
		TestTrue(
			TEXT("clear changes no card-run field beyond the three route-economy fields"),
			AreCardRunsEqual(State.CardRun, ExpectedAfterClear));
	}

	return true;
}

#endif
