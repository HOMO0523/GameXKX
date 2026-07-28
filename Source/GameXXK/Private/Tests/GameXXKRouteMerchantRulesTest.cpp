#include "GameXXKMVPRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "GameXXKRunDeckRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	TArray<FName> FindCardIdsByOwner(const EGameXXKCardOwner Owner, const EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid)
	{
		TArray<FName> Result;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == Owner && (Role == EGameXXKCharacterRole::Invalid || Definition.Role == Role))
			{
				Result.Add(Definition.Id);
			}
		}
		Result.Sort(NameLess);
		return Result;
	}

	FGameXXKRuntimeState MakeMerchantState(const bool bWithActiveCompanion)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::RouteMerchant;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = 10;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {State.RouteSeed};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = 1;
		State.CardRun.bLoadoutLockedForRoute = true;
		State.CardRun.bRouteEconomyInitialized = true;
		State.CardRun.RouteTravelMoney = 500;
		State.PlayerGold = 777;

		const TArray<FName> HeroCards = FindCardIdsByOwner(EGameXXKCardOwner::Hero);
		State.CardRun.HeroUnlockedCardIds.Append(HeroCards.GetData(), FMath::Min(8, HeroCards.Num()));
		State.CardRun.HeroSelectedCardIds = State.CardRun.HeroUnlockedCardIds;

		if (bWithActiveCompanion)
		{
			const TArray<FName> BladeCards = FindCardIdsByOwner(EGameXXKCardOwner::Profession, EGameXXKCharacterRole::Blade);
			FGameXXKPermanentCompanion Companion;
			Companion.InstanceId = TEXT("Companion.Instance.Merchant.Blade");
			Companion.RecruitTemplateId = TEXT("Companion.Blade.01");
			Companion.Role = EGameXXKCharacterRole::Blade;
			Companion.bIsActive = true;
			Companion.PersonalCardIds.Append(BladeCards.GetData(), FMath::Min(12, BladeCards.Num()));
			Companion.UnlockedPersonalCardIds = Companion.PersonalCardIds;
			Companion.SelectedCardIds.Append(Companion.PersonalCardIds.GetData(), FMath::Min(5, Companion.PersonalCardIds.Num()));
			State.CardRun.CompanionRoster.PermanentCompanions.Add(Companion);
			State.CardRun.PartySelection.ActivePermanentCompanionInstanceId = Companion.InstanceId;
		}
		return State;
	}

	bool MerchantStatesMatch(const FGameXXKRouteMerchantState& Left, const FGameXXKRouteMerchantState& Right)
	{
		return FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool RuntimeStatesMatch(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	int32 CountOffersOfKind(const FGameXXKRouteMerchantState& Merchant, const EGameXXKRouteMerchantOfferKind Kind)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
		{
			if (Offer.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	void AddEpicOwnedEntry(FGameXXKRuntimeState& State, const FName CardId, const int32 Ordinal)
	{
		FGameXXKRouteCardEntry Entry;
		Entry.EntryId = FName(*FString::Printf(TEXT("Test.EpicOwned.%d"), Ordinal));
		Entry.CardId = CardId;
		Entry.CurrentQuality = EGameXXKCardQuality::Epic;
		Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
		Entry.OwnerUnitId = TEXT("Player");
		Entry.bTemporaryRouteCard = true;
		Entry.bConsumesRouteCapacity = true;
		Entry.AcquisitionOrdinal = Ordinal;
		State.CardRun.RouteCardEntries.Add(Entry);
	}

	bool RouteNodesMatch(const TArray<FGameXXKRouteMapNode>& Left, const TArray<FGameXXKRouteMapNode>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteMapNode::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	bool RouteEdgesMatch(const TArray<FGameXXKRouteMapEdge>& Left, const TArray<FGameXXKRouteMapEdge>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteMapEdge::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	FGameXXKSaveState MakeLegacyMerchantSnapshot()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.bDungeonActive = true;
		State.RouteSeed = 0x6137;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9, 10};
		State.CurrentRouteNodeId = 10;
		State.PendingRouteNodeId = 10;
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {
			State.RouteSeed,
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 2)),
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 3))};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, 20);

		FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
		Merchant.SourceNodeId = 10;
		Merchant.OfferSeed = 0x7351;
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = TEXT("Merchant.10.Card.0");
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Card;
		Offer.ContentId = TEXT("Route.General.PoJiaTuCi");
		Offer.Price = 15;
		Merchant.Offers.Add(Offer);

		FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(State);
		LegacySave.SaveVersion = FGameXXKSaveMigration::RouteMerchantSnapshotIntroducedSaveVersion - 1;
		return LegacySave;
	}

	bool StartAcceptedGeneratedRoute(FGameXXKRuntimeState& OutState)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.RouteSeed = 0x6137;
		return UGameXXKMVPRules::OpenWorldMap(OutState)
			&& UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}

	const FGameXXKRouteMapNode* FindNodeOfKind(
		const FGameXXKRuntimeState& State,
		const EGameXXKNodeKind Kind)
	{
		return State.RouteMapNodes.FindByPredicate([Kind](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeKind == Kind;
		});
	}

	bool OpenGeneratedMerchant(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		const FGameXXKRouteMapNode* MerchantNode = FindNodeOfKind(InOutState, EGameXXKNodeKind::Merchant);
		if (!MerchantNode)
		{
			return false;
		}
		InOutState.Screen = EGameXXKScreen::RouteMerchant;
		InOutState.CurrentMapId = TEXT("RouteMerchant");
		InOutState.PendingRouteNodeId = MerchantNode->NodeId;
		return FGameXXKRouteMerchantRules::EnsureStock(InOutState, OutError);
	}

	void ForceCardBattleVictory(FGameXXKRuntimeState& InOutState)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		InOutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockCompanionTest,
	"GameXXK.Route.Merchant.Rules.Stock.CompanionAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockCompanionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState(true);
	const FName EpicHeroCard = State.CardRun.HeroUnlockedCardIds[0];
	const TArray<FName> RouteCards = FindCardIdsByOwner(EGameXXKCardOwner::Route);
	const FName EpicRouteCard = RouteCards[0];
	AddEpicOwnedEntry(State, EpicHeroCard, 0);
	AddEpicOwnedEntry(State, EpicRouteCard, 1);

	FString Error;
	TestTrue(TEXT("locked active merchant generates stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("stock generation reports no error"), Error.IsEmpty());
	const FGameXXKRouteMerchantState FirstStock = State.CardRun.RouteMerchant;
	TestEqual(TEXT("merchant snapshot records pending node"), FirstStock.SourceNodeId, 10);
	TestTrue(TEXT("merchant snapshot has a stable non-zero seed"), FirstStock.OfferSeed != 0);
	TestEqual(TEXT("first snapshot starts before refreshes"), FirstStock.RefreshCount, 0);
	TestEqual(TEXT("snapshot always has six offers"), FirstStock.Offers.Num(), 6);
	TestEqual(TEXT("snapshot has exactly three card slots"), CountOffersOfKind(FirstStock, EGameXXKRouteMerchantOfferKind::Card), 3);
	TestEqual(TEXT("snapshot has exactly three relic slots"), CountOffersOfKind(FirstStock, EGameXXKRouteMerchantOfferKind::Relic), 3);

	const FGameXXKPermanentCompanion& Companion = State.CardRun.CompanionRoster.PermanentCompanions[0];
	TSet<FName> SeenCardIds;
	TSet<FName> SeenRelicIds;
	int32 HeroOfferCount = 0;
	int32 CompanionOfferCount = 0;
	int32 RouteOfferCount = 0;
	for (int32 Index = 0; Index < FirstStock.Offers.Num(); ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = FirstStock.Offers[Index];
		TestFalse(*FString::Printf(TEXT("offer %d has a stable id"), Index), Offer.OfferId.IsNone());
		TestFalse(*FString::Printf(TEXT("offer %d is available in the full-catalog fixture"), Index), Offer.bUnavailable);
		TestFalse(*FString::Printf(TEXT("offer %d starts unsold"), Index), Offer.bSold);
		if (Index < 3)
		{
			TestEqual(*FString::Printf(TEXT("slot %d is a card"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Card);
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId);
			TestNotNull(*FString::Printf(TEXT("card slot %d resolves catalog content"), Index), Definition);
			if (!Definition)
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("card slot %d persists catalog base quality"), Index), Offer.Quality, Definition->BaseQuality);
			TestEqual(*FString::Printf(TEXT("card slot %d uses quality price"), Index), Offer.Price, FGameXXKCardQualityRules::GetCardPrice(Offer.Quality));
			TestFalse(*FString::Printf(TEXT("card slot %d is unique in its batch"), Index), SeenCardIds.Contains(Offer.ContentId));
			SeenCardIds.Add(Offer.ContentId);
			TestTrue(*FString::Printf(TEXT("card slot %d never contains a task NPC"), Index), Definition->Owner != EGameXXKCardOwner::QuestNpc);
			TestTrue(*FString::Printf(TEXT("card slot %d excludes an already-Epic CardId"), Index), Offer.ContentId != EpicHeroCard && Offer.ContentId != EpicRouteCard);
			if (Definition->Owner == EGameXXKCardOwner::Hero)
			{
				++HeroOfferCount;
				TestTrue(TEXT("hero offer is unlocked in the current save"), State.CardRun.HeroUnlockedCardIds.Contains(Offer.ContentId));
			}
			else if (Definition->Owner == EGameXXKCardOwner::Profession)
			{
				++CompanionOfferCount;
				TestTrue(TEXT("companion offer comes from the active companion's exact twelve-card personal pool"), Companion.PersonalCardIds.Contains(Offer.ContentId));
			}
			else if (Definition->Owner == EGameXXKCardOwner::Route)
			{
				++RouteOfferCount;
			}
		}
		else
		{
			TestEqual(*FString::Printf(TEXT("slot %d is a relic"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Relic);
			const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer.ContentId);
			TestNotNull(*FString::Printf(TEXT("relic slot %d resolves catalog content"), Index), Definition);
			if (!Definition)
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("relic slot %d persists catalog base quality"), Index), Offer.Quality, Definition->BaseQuality);
			TestEqual(*FString::Printf(TEXT("relic slot %d uses quality price"), Index), Offer.Price, FGameXXKCardQualityRules::GetRelicPrice(Offer.Quality));
			TestFalse(*FString::Printf(TEXT("relic slot %d is unique in its batch"), Index), SeenRelicIds.Contains(Offer.ContentId));
			SeenRelicIds.Add(Offer.ContentId);
		}
	}
	TestEqual(TEXT("active companion stock has exactly one hero card"), HeroOfferCount, 1);
	TestEqual(TEXT("active companion stock has exactly one companion card"), CompanionOfferCount, 1);
	TestEqual(TEXT("active companion stock has exactly one route card"), RouteOfferCount, 1);
	FGameXXKRuntimeState IndependentTwin = MakeMerchantState(true);
	AddEpicOwnedEntry(IndependentTwin, EpicHeroCard, 0);
	AddEpicOwnedEntry(IndependentTwin, EpicRouteCard, 1);
	TestTrue(TEXT("independent same-root fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(IndependentTwin, &Error));
	TestTrue(TEXT("root seed, source node, and refresh count deterministically reproduce identical ordered stock"), MerchantStatesMatch(IndependentTwin.CardRun.RouteMerchant, FirstStock));

	TestTrue(TEXT("reopening the same merchant succeeds"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("reopening is byte-stable"), MerchantStatesMatch(State.CardRun.RouteMerchant, FirstStock));
	FGameXXKRuntimeState SaveLikeCopy = State;
	TestTrue(TEXT("save-like struct copy reopens"), FGameXXKRouteMerchantRules::EnsureStock(SaveLikeCopy, &Error));
	TestTrue(TEXT("save-like struct copy preserves array order and every offer byte"), MerchantStatesMatch(SaveLikeCopy.CardRun.RouteMerchant, FirstStock));

	const FGameXXKRuntimeState BeforeView = State;
	FGameXXKRouteMerchantView View;
	TestTrue(TEXT("view can be read from saved stock"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestTrue(TEXT("view read is pure"), RuntimeStatesMatch(State, BeforeView));
	TestEqual(TEXT("view exposes three card slots"), View.CardOffers.Num(), 3);
	TestEqual(TEXT("view exposes three relic slots"), View.RelicOffers.Num(), 3);
	TestEqual(TEXT("view exposes route-only balance"), View.RouteTravelMoney, State.CardRun.RouteTravelMoney);
	TestEqual(TEXT("view exposes first refresh cost"), View.RefreshCost, 20);
	TestTrue(TEXT("view enables affordable refresh"), View.bRefreshEnabled);

	State.RouteMapNodes.Add(FGameXXKRouteMapNode{11, 3, 0, EGameXXKNodeKind::Merchant, FVector2D(0.75f, 0.5f), TArray<int32>{}});
	State.PendingRouteNodeId = 11;
	State.CardRun.RouteMerchant.PendingPurchase.bActive = true;
	State.CardRun.RouteMerchant.PendingPurchase.OfferId = FirstStock.Offers[0].OfferId;
	State.CardRun.RouteMerchant.PendingPurchase.CardId = FirstStock.Offers[0].ContentId;
	State.CardRun.RouteMerchant.PendingPurchase.Price = FirstStock.Offers[0].Price;
	const FGameXXKRuntimeState BeforePendingNodeChange = State;
	TestFalse(TEXT("a different merchant cannot replace stock while a purchase is pending"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("pending different-node rejection preserves complete runtime"), RuntimeStatesMatch(State, BeforePendingNodeChange));
	State.CardRun.RouteMerchant.PendingPurchase = FGameXXKPendingRouteMerchantPurchase();
	TestTrue(TEXT("a different merchant generates an independent snapshot after pending is cleared"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestEqual(TEXT("new snapshot records the second merchant"), State.CardRun.RouteMerchant.SourceNodeId, 11);
	TestTrue(TEXT("new merchant gets an independent seed"), State.CardRun.RouteMerchant.OfferSeed != FirstStock.OfferSeed);

	FGameXXKRuntimeState InvalidCompanion = MakeMerchantState(true);
	InvalidCompanion.CardRun.CompanionRoster.PermanentCompanions[0].PersonalCardIds.Pop();
	const FGameXXKRuntimeState InvalidCompanionBefore = InvalidCompanion;
	TestFalse(TEXT("invalid eleven-card active companion cannot generate merchant stock"), FGameXXKRouteMerchantRules::EnsureStock(InvalidCompanion, &Error));
	TestTrue(TEXT("invalid companion stock rejection preserves complete runtime"), RuntimeStatesMatch(InvalidCompanion, InvalidCompanionBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockFallbackTest,
	"GameXXK.Route.Merchant.Rules.Stock.NoCompanionFallbackAndUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockFallbackTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState(false);
	FString Error;
	TestTrue(TEXT("no-companion stock generates"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	int32 HeroCount = 0;
	int32 RouteCount = 0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(State.CardRun.RouteMerchant.Offers[Index].ContentId);
		if (Definition && Definition->Owner == EGameXXKCardOwner::Hero) ++HeroCount;
		if (Definition && Definition->Owner == EGameXXKCardOwner::Route) ++RouteCount;
	}
	TestEqual(TEXT("no-companion stock has one hero card"), HeroCount, 1);
	TestEqual(TEXT("no-companion stock has two route cards"), RouteCount, 2);

	FGameXXKRuntimeState HeroFallback = MakeMerchantState(false);
	HeroFallback.CardRun.HeroUnlockedCardIds.Reset();
	HeroFallback.CardRun.HeroSelectedCardIds.Reset();
	TestTrue(TEXT("empty hero source falls back to route stock"), FGameXXKRouteMerchantRules::EnsureStock(HeroFallback, &Error));
	TSet<FName> FallbackCardIds;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = HeroFallback.CardRun.RouteMerchant.Offers[Index];
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId);
		TestNotNull(TEXT("fallback route card resolves"), Definition);
		if (Definition) TestEqual(TEXT("fallback slot uses route owner"), Definition->Owner, EGameXXKCardOwner::Route);
		TestFalse(TEXT("fallback cards remain unique"), FallbackCardIds.Contains(Offer.ContentId));
		FallbackCardIds.Add(Offer.ContentId);
	}

	FGameXXKRuntimeState ExhaustedState = MakeMerchantState(false);
	int32 EpicOrdinal = 0;
	for (const FName HeroId : ExhaustedState.CardRun.HeroUnlockedCardIds)
	{
		AddEpicOwnedEntry(ExhaustedState, HeroId, EpicOrdinal++);
	}
	for (const FName RouteId : FindCardIdsByOwner(EGameXXKCardOwner::Route))
	{
		AddEpicOwnedEntry(ExhaustedState, RouteId, EpicOrdinal++);
	}
	const TArray<FGameXXKRelicDefinition>& Relics = FGameXXKRelicCatalog::GetAllDefinitions();
	for (int32 Index = 0; Index < Relics.Num() - 1; ++Index)
	{
		FGameXXKRelicInstance Owned;
		Owned.RelicId = Relics[Index].Id;
		Owned.Stacks = 1;
		Owned.AcquisitionOrdinal = Index + 1;
		ExhaustedState.CardRun.Relics.Add(Owned);
	}
	TestTrue(TEXT("exhausted legal pools still generate all six persistent slots"), FGameXXKRouteMerchantRules::EnsureStock(ExhaustedState, &Error));
	TestEqual(TEXT("exhausted snapshot still has six slots"), ExhaustedState.CardRun.RouteMerchant.Offers.Num(), 6);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = ExhaustedState.CardRun.RouteMerchant.Offers[Index];
		TestTrue(*FString::Printf(TEXT("exhausted card slot %d is unavailable"), Index), Offer.bUnavailable);
		TestFalse(*FString::Printf(TEXT("exhausted card slot %d keeps a stable nonempty offer id"), Index), Offer.OfferId.IsNone());
		TestEqual(*FString::Printf(TEXT("exhausted card slot %d keeps its kind"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Card);
		TestTrue(*FString::Printf(TEXT("exhausted card slot %d has no content"), Index), Offer.ContentId.IsNone());
		TestEqual(*FString::Printf(TEXT("exhausted card slot %d has invalid quality"), Index), Offer.Quality, EGameXXKCardQuality::Invalid);
		TestEqual(*FString::Printf(TEXT("exhausted card slot %d costs zero"), Index), Offer.Price, 0);
	}
	int32 AvailableRelics = 0;
	int32 UnavailableRelics = 0;
	for (int32 Index = 3; Index < 6; ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = ExhaustedState.CardRun.RouteMerchant.Offers[Index];
		if (Offer.bUnavailable)
		{
			++UnavailableRelics;
			TestFalse(TEXT("unavailable relic keeps a stable offer id"), Offer.OfferId.IsNone());
			TestTrue(TEXT("unavailable relic has no content"), Offer.ContentId.IsNone());
			TestEqual(TEXT("unavailable relic has invalid quality"), Offer.Quality, EGameXXKCardQuality::Invalid);
			TestEqual(TEXT("unavailable relic costs zero"), Offer.Price, 0);
		}
		else
		{
			++AvailableRelics;
			TestEqual(TEXT("last legal relic is preserved"), Offer.ContentId, Relics.Last().Id);
		}
	}
	TestEqual(TEXT("one unowned relic remains available"), AvailableRelics, 1);
	TestEqual(TEXT("relic shortage produces two explicit unavailable slots"), UnavailableRelics, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantRefreshTest,
	"GameXXK.Route.Merchant.Rules.Stock.RefreshAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantRefreshTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState(true);
	FString Error;
	TestTrue(TEXT("initial stock generates before refresh"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	const FGameXXKRouteMerchantState BeforeRefresh = State.CardRun.RouteMerchant;
	const int32 PlayerGoldBefore = State.PlayerGold;
	const int32 MoneyBefore = State.CardRun.RouteTravelMoney;
	TestEqual(TEXT("refresh zero costs twenty"), FGameXXKRouteMerchantRules::GetRefreshCost(0), 20);
	TestEqual(TEXT("refresh one costs thirty"), FGameXXKRouteMerchantRules::GetRefreshCost(1), 30);
	TestEqual(TEXT("refresh two costs forty"), FGameXXKRouteMerchantRules::GetRefreshCost(2), 40);
	TestEqual(TEXT("refresh three costs fifty"), FGameXXKRouteMerchantRules::GetRefreshCost(3), 50);
	TestEqual(TEXT("later refresh remains fifty"), FGameXXKRouteMerchantRules::GetRefreshCost(999), 50);
	TestEqual(TEXT("invalid refresh count has no valid cost"), FGameXXKRouteMerchantRules::GetRefreshCost(-1), 0);
	TestTrue(TEXT("refresh succeeds"), FGameXXKRouteMerchantRules::Refresh(State, &Error));
	TestEqual(TEXT("refresh debits route travel money once"), State.CardRun.RouteTravelMoney, MoneyBefore - 20);
	TestEqual(TEXT("refresh never touches permanent gold"), State.PlayerGold, PlayerGoldBefore);
	TestEqual(TEXT("refresh count advances once"), State.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh replaces the complete six-slot stock"), State.CardRun.RouteMerchant.Offers.Num(), 6);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("refresh replaces slot %d identity"), Index), State.CardRun.RouteMerchant.Offers[Index].OfferId != BeforeRefresh.Offers[Index].OfferId);
	}
	FGameXXKRouteMerchantView View;
	TestTrue(TEXT("post-refresh view succeeds"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestEqual(TEXT("post-refresh view exposes next cost"), View.RefreshCost, 30);

	FGameXXKRuntimeState Insufficient = State;
	Insufficient.CardRun.RouteTravelMoney = View.RefreshCost - 1;
	const FGameXXKRuntimeState InsufficientBefore = Insufficient;
	TestFalse(TEXT("refresh rejects insufficient route money"), FGameXXKRouteMerchantRules::Refresh(Insufficient, &Error));
	TestTrue(TEXT("insufficient refresh preserves the complete runtime"), RuntimeStatesMatch(Insufficient, InsufficientBefore));
	FGameXXKRouteMerchantView InsufficientView;
	TestTrue(TEXT("insufficient view still reads"), FGameXXKRouteMerchantRules::GetView(Insufficient, InsufficientView, &Error));
	TestFalse(TEXT("insufficient view disables refresh"), InsufficientView.bRefreshEnabled);
	TestFalse(TEXT("insufficient view exposes a stable reason"), InsufficientView.RefreshDisabledReason.IsEmpty());

	FGameXXKRuntimeState Overflow = State;
	Overflow.CardRun.RouteMerchant.RefreshCount = MAX_int32;
	const FGameXXKRuntimeState OverflowBefore = Overflow;
	TestFalse(TEXT("refresh count overflow is rejected"), FGameXXKRouteMerchantRules::Refresh(Overflow, &Error));
	TestTrue(TEXT("refresh MAX reaches the explicit overflow guard"), Error.Contains(TEXT("safely incremented")));
	TestTrue(TEXT("overflow rejection preserves the complete runtime"), RuntimeStatesMatch(Overflow, OverflowBefore));

	FGameXXKRuntimeState InvalidContext = State;
	InvalidContext.Screen = EGameXXKScreen::DungeonMap;
	const FGameXXKRuntimeState InvalidContextBefore = InvalidContext;
	TestFalse(TEXT("refresh rejects a non-merchant screen"), FGameXXKRouteMerchantRules::Refresh(InvalidContext, &Error));
	TestTrue(TEXT("invalid-context refresh preserves the complete runtime"), RuntimeStatesMatch(InvalidContext, InvalidContextBefore));
	return true;
}

namespace
{
	FGameXXKRouteMerchantOffer* FindAvailableOffer(
		FGameXXKRuntimeState& State,
		const EGameXXKRouteMerchantOfferKind Kind)
	{
		return State.CardRun.RouteMerchant.Offers.FindByPredicate([Kind](const FGameXXKRouteMerchantOffer& Offer)
		{
			return Offer.Kind == Kind && !Offer.bUnavailable && !Offer.bSold;
		});
	}

	FGameXXKRouteMerchantOffer* ForceCommonHeroOffer(FGameXXKRuntimeState& State)
	{
		FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Card);
		if (!Offer)
		{
			return nullptr;
		}
		TSet<FName> OtherOfferedIds;
		for (const FGameXXKRouteMerchantOffer& Existing : State.CardRun.RouteMerchant.Offers)
		{
			if (&Existing != Offer && Existing.Kind == EGameXXKRouteMerchantOfferKind::Card && !Existing.bUnavailable)
			{
				OtherOfferedIds.Add(Existing.ContentId);
			}
		}
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero
				&& Definition.BaseQuality == EGameXXKCardQuality::Common
				&& !OtherOfferedIds.Contains(Definition.Id))
			{
				Offer->ContentId = Definition.Id;
				Offer->Quality = Definition.BaseQuality;
				Offer->Price = FGameXXKCardQualityRules::GetCardPrice(Definition.BaseQuality);
				if (!State.CardRun.HeroUnlockedCardIds.Contains(Definition.Id))
				{
					State.CardRun.HeroUnlockedCardIds.Add(Definition.Id);
				}
				return Offer;
			}
		}
		return nullptr;
	}

	FGameXXKRouteMerchantOffer* ForceStackableRelicOffer(FGameXXKRuntimeState& State)
	{
		FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Relic);
		if (!Offer)
		{
			return nullptr;
		}
		TSet<FName> OtherOfferedIds;
		for (const FGameXXKRouteMerchantOffer& Existing : State.CardRun.RouteMerchant.Offers)
		{
			if (&Existing != Offer && Existing.Kind == EGameXXKRouteMerchantOfferKind::Relic && !Existing.bUnavailable)
			{
				OtherOfferedIds.Add(Existing.ContentId);
			}
		}
		for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
		{
			if (Definition.bStackable && !OtherOfferedIds.Contains(Definition.Id))
			{
				Offer->ContentId = Definition.Id;
				Offer->Quality = Definition.BaseQuality;
				Offer->Price = FGameXXKCardQualityRules::GetRelicPrice(Definition.BaseQuality);
				return Offer;
			}
		}
		return nullptr;
	}

	void FillRouteCapacity(
		FGameXXKRuntimeState& State,
		const int32 Count,
		const FName MatchingCardId = NAME_None,
		const EGameXXKCardQuality MatchingQuality = EGameXXKCardQuality::Common)
	{
		State.CardRun.RouteCardEntries.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKRouteCardEntry Entry;
			Entry.EntryId = FName(*FString::Printf(TEXT("Capacity.Entry.%02d"), Index));
			Entry.CardId = Index == 0 && !MatchingCardId.IsNone()
				? MatchingCardId
				: FName(*FString::Printf(TEXT("Capacity.Card.%02d"), Index));
			Entry.CurrentQuality = Index == 0 && !MatchingCardId.IsNone()
				? MatchingQuality
				: EGameXXKCardQuality::Common;
			Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
			Entry.OwnerUnitId = TEXT("Player");
			Entry.bTemporaryRouteCard = true;
			Entry.bConsumesRouteCapacity = true;
			Entry.AcquisitionOrdinal = Index;
			State.CardRun.RouteCardEntries.Add(Entry);
		}
		State.CardRun.NextRouteCardEntryOrdinal = Count;
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 0;
	}

	void ExpectPurchaseFailureAndRollback(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName OfferId,
		const FName ReplacementEntryId,
		const EGameXXKRouteMerchantPurchaseFailure ExpectedFailure,
		const TCHAR* Label)
	{
		const FGameXXKRuntimeState Before = State;
		FGameXXKRouteMerchantPurchaseResult Result;
		Test.TestFalse(Label, FGameXXKRouteMerchantRules::Purchase(State, OfferId, ReplacementEntryId, Result));
		Test.TestEqual(*FString::Printf(TEXT("%s reports the stable failure enum"), Label), Result.Failure, ExpectedFailure);
		Test.TestFalse(*FString::Printf(TEXT("%s reports a stable reason string"), Label), Result.FailureReason.IsEmpty());
		Test.TestTrue(*FString::Printf(TEXT("%s preserves the complete runtime"), Label), RuntimeStatesMatch(State, Before));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseValidationTest,
	"GameXXK.Route.Merchant.Rules.Purchase.ValidationAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState Base = MakeMerchantState(false);
	TestTrue(TEXT("purchase validation fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(Base, &Error));
	FGameXXKRouteMerchantOffer* BaseCard = ForceCommonHeroOffer(Base);
	TestNotNull(TEXT("purchase fixture has a common hero card"), BaseCard);
	if (!BaseCard)
	{
		return false;
	}
	const FName CardOfferId = BaseCard->OfferId;
	const int32 CardPrice = BaseCard->Price;

	FGameXXKRuntimeState Insufficient = Base;
	Insufficient.CardRun.RouteTravelMoney = CardPrice - 1;
	const FGameXXKRuntimeState BeforePreview = Insufficient;
	FGameXXKRouteMerchantPurchasePreview Preview;
	TestFalse(TEXT("insufficient preview rejects"), FGameXXKRouteMerchantRules::PreviewPurchase(Insufficient, CardOfferId, NAME_None, Preview, &Error));
	TestEqual(TEXT("insufficient preview reports typed reason"), Preview.Failure, EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney);
	TestEqual(TEXT("insufficient preview includes the saved offer"), Preview.Offer.OfferId, CardOfferId);
	TestEqual(TEXT("insufficient preview includes balance before"), Preview.BalanceBefore, CardPrice - 1);
	TestEqual(TEXT("insufficient preview includes price"), Preview.Price, CardPrice);
	TestTrue(TEXT("preview is pure on failure"), RuntimeStatesMatch(Insufficient, BeforePreview));
	ExpectPurchaseFailureAndRollback(
		*this,
		Insufficient,
		CardOfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney,
		TEXT("insufficient purchase"));

	FGameXXKRuntimeState Sold = Base;
	Sold.CardRun.RouteMerchant.Offers[0].bSold = true;
	ExpectPurchaseFailureAndRollback(
		*this,
		Sold,
		Sold.CardRun.RouteMerchant.Offers[0].OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold,
		TEXT("sold purchase"));

	FGameXXKRuntimeState Stale = Base;
	ExpectPurchaseFailureAndRollback(
		*this,
		Stale,
		TEXT("Merchant.Stale.Offer"),
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::StaleOfferId,
		TEXT("stale-id purchase"));

	FGameXXKRuntimeState Unavailable = Base;
	FGameXXKRouteMerchantOffer& UnavailableOffer = Unavailable.CardRun.RouteMerchant.Offers[0];
	UnavailableOffer.ContentId = NAME_None;
	UnavailableOffer.Quality = EGameXXKCardQuality::Invalid;
	UnavailableOffer.Price = 0;
	UnavailableOffer.bUnavailable = true;
	ExpectPurchaseFailureAndRollback(
		*this,
		Unavailable,
		UnavailableOffer.OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable,
		TEXT("unavailable-slot purchase"));

	FGameXXKRuntimeState DuplicateRelic = Base;
	FGameXXKRouteMerchantOffer* StackableOffer = ForceStackableRelicOffer(DuplicateRelic);
	TestNotNull(TEXT("duplicate fixture resolves a legacy-stackable relic"), StackableOffer);
	if (StackableOffer)
	{
		FGameXXKRelicInstance Owned;
		Owned.RelicId = StackableOffer->ContentId;
		Owned.Stacks = 7;
		Owned.AcquisitionOrdinal = 1;
		DuplicateRelic.CardRun.Relics.Add(Owned);
		const FGameXXKRuntimeState OwnedRelicBeforeReopen = DuplicateRelic;
		TestTrue(TEXT("same-node reopen preserves an offer that became owned after stock generation"), FGameXXKRouteMerchantRules::EnsureStock(DuplicateRelic, &Error));
		TestTrue(TEXT("owned-relic reopen is byte-stable"), RuntimeStatesMatch(DuplicateRelic, OwnedRelicBeforeReopen));
		ExpectPurchaseFailureAndRollback(
			*this,
			DuplicateRelic,
			StackableOffer->OfferId,
			NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic,
			TEXT("legacy-stackable duplicate relic"));
	}

	FGameXXKRuntimeState MaxOrdinal = Base;
	MaxOrdinal.CardRun.NextRouteCardEntryOrdinal = MAX_int32;
	ExpectPurchaseFailureAndRollback(
		*this,
		MaxOrdinal,
		CardOfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::InvalidRouteCardOrdinal,
		TEXT("MAX route-card ordinal purchase"));

	FGameXXKRuntimeState AcquisitionOverflow = Base;
	AcquisitionOverflow.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = MAX_int32;
	ExpectPurchaseFailureAndRollback(
		*this,
		AcquisitionOverflow,
		CardOfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::DeckAcquisitionRejected,
		TEXT("route-card acquisition-count overflow"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseDirectAndMergeTest,
	"GameXXK.Route.Merchant.Rules.Purchase.DirectAndMergeAtCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseDirectAndMergeTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState Direct = MakeMerchantState(false);
	TestTrue(TEXT("direct fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(Direct, &Error));
	FGameXXKRouteMerchantOffer* DirectOffer = ForceCommonHeroOffer(Direct);
	TestNotNull(TEXT("direct fixture has a common hero offer"), DirectOffer);
	if (!DirectOffer)
	{
		return false;
	}
	const FGameXXKRouteMerchantOffer SavedDirectOffer = *DirectOffer;
	const FGameXXKRuntimeState BeforeDirectPreview = Direct;
	FGameXXKRouteMerchantPurchasePreview DirectPreview;
	TestTrue(TEXT("direct-add preview succeeds"), FGameXXKRouteMerchantRules::PreviewPurchase(Direct, DirectOffer->OfferId, NAME_None, DirectPreview, &Error));
	TestTrue(TEXT("direct-add preview can purchase"), DirectPreview.bCanPurchase);
	TestFalse(TEXT("direct-add preview needs no replacement"), DirectPreview.bRequiresReplacement);
	TestEqual(TEXT("direct-add preview includes offer"), DirectPreview.Offer.OfferId, DirectOffer->OfferId);
	TestEqual(TEXT("direct-add preview includes before balance"), DirectPreview.BalanceBefore, Direct.CardRun.RouteTravelMoney);
	TestEqual(TEXT("direct-add preview includes after balance"), DirectPreview.BalanceAfter, Direct.CardRun.RouteTravelMoney - DirectOffer->Price);
	TestEqual(TEXT("direct-add preview reports capacity delta"), DirectPreview.CapacityDelta, 1);
	TestTrue(TEXT("direct-add preview is pure"), RuntimeStatesMatch(Direct, BeforeDirectPreview));

	const int32 DirectMoneyBefore = Direct.CardRun.RouteTravelMoney;
	const int32 DirectGoldBefore = Direct.PlayerGold;
	const int32 DirectAcquisitionsBefore = Direct.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	FGameXXKRouteMerchantPurchaseResult DirectResult;
	TestTrue(TEXT("direct card purchase commits"), FGameXXKRouteMerchantRules::Purchase(Direct, DirectOffer->OfferId, NAME_None, DirectResult));
	TestTrue(TEXT("direct result marks purchased"), DirectResult.bPurchased);
	TestEqual(TEXT("direct result repeats exact offer"), DirectResult.Offer.OfferId, SavedDirectOffer.OfferId);
	TestEqual(TEXT("direct result repeats debit"), DirectResult.Price, SavedDirectOffer.Price);
	TestEqual(TEXT("direct purchase debits once"), Direct.CardRun.RouteTravelMoney, DirectMoneyBefore - SavedDirectOffer.Price);
	TestEqual(TEXT("direct purchase preserves permanent gold"), Direct.PlayerGold, DirectGoldBefore);
	TestEqual(TEXT("direct purchase adds one stable entry"), Direct.CardRun.RouteCardEntries.Num(), 1);
	TestEqual(TEXT("direct purchase advances dedicated ordinal once"), Direct.CardRun.NextRouteCardEntryOrdinal, 1);
	TestEqual(TEXT("RunDeck commit advances acquisition count once"), Direct.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, DirectAcquisitionsBefore + 1);
	TestEqual(TEXT("merchant card source is stable"), Direct.CardRun.RouteCardEntries[0].SourceKind, EGameXXKRouteCardSourceKind::Merchant);
	TestEqual(TEXT("hero card owner is Player"), Direct.CardRun.RouteCardEntries[0].OwnerUnitId, FName(TEXT("Player")));
	TestTrue(TEXT("direct purchase marks only selected offer sold"), Direct.CardRun.RouteMerchant.Offers[0].bSold);
	int32 SoldCount = 0;
	for (const FGameXXKRouteMerchantOffer& Offer : Direct.CardRun.RouteMerchant.Offers)
	{
		if (Offer.bSold) ++SoldCount;
	}
	TestEqual(TEXT("exactly one offer is sold"), SoldCount, 1);
	TestEqual(TEXT("successful purchase remains on merchant screen"), Direct.Screen, EGameXXKScreen::RouteMerchant);
	const FGameXXKRuntimeState SoldCardBeforeReopen = Direct;
	TestTrue(TEXT("same-node reopen accepts sold card stock"), FGameXXKRouteMerchantRules::EnsureStock(Direct, &Error));
	TestTrue(TEXT("sold card reopen is byte-stable"), RuntimeStatesMatch(Direct, SoldCardBeforeReopen));
	FName ExpectedEntryId;
	TestTrue(TEXT("stable entry id helper succeeds"), FGameXXKRouteCardRecipe::MakeStableEntryId(Direct.CardRun.RouteProgress.RootSeed, 0, ExpectedEntryId, &Error));
	TestEqual(TEXT("merchant entry id derives from root seed and prior ordinal"), Direct.CardRun.RouteCardEntries[0].EntryId, ExpectedEntryId);
	const FGameXXKRuntimeState AfterDirect = Direct;
	FGameXXKRouteMerchantPurchaseResult RetryResult;
	TestFalse(TEXT("sold offer cannot debit twice"), FGameXXKRouteMerchantRules::Purchase(Direct, SavedDirectOffer.OfferId, NAME_None, RetryResult));
	TestEqual(TEXT("retry reports sold"), RetryResult.Failure, EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold);
	TestTrue(TEXT("retry preserves complete post-purchase runtime"), RuntimeStatesMatch(Direct, AfterDirect));

	FGameXXKRuntimeState CompanionState = MakeMerchantState(true);
	TestTrue(TEXT("companion fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(CompanionState, &Error));
	FGameXXKRouteMerchantOffer* CompanionOffer = CompanionState.CardRun.RouteMerchant.Offers.FindByPredicate([](const FGameXXKRouteMerchantOffer& Offer)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId);
		return Offer.Kind == EGameXXKRouteMerchantOfferKind::Card && Definition && Definition->Owner == EGameXXKCardOwner::Profession;
	});
	TestNotNull(TEXT("active-companion stock includes companion offer"), CompanionOffer);
	if (CompanionOffer)
	{
		FGameXXKRouteMerchantPurchaseResult CompanionResult;
		TestTrue(TEXT("companion card purchase commits"), FGameXXKRouteMerchantRules::Purchase(CompanionState, CompanionOffer->OfferId, NAME_None, CompanionResult));
		TestEqual(
			TEXT("companion card owner is the active stable instance"),
			CompanionState.CardRun.RouteCardEntries.Last().OwnerUnitId,
			CompanionState.CardRun.PartySelection.ActivePermanentCompanionInstanceId);
	}

	FGameXXKRuntimeState Merge = MakeMerchantState(false);
	TestTrue(TEXT("merge fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(Merge, &Error));
	FGameXXKRouteMerchantOffer* MergeOffer = ForceCommonHeroOffer(Merge);
	TestNotNull(TEXT("merge fixture has common offer"), MergeOffer);
	if (!MergeOffer)
	{
		return false;
	}
	const FName MergeOfferId = MergeOffer->OfferId;
	const FName MergeCardId = MergeOffer->ContentId;
	const int32 MergePrice = MergeOffer->Price;
	FillRouteCapacity(Merge, FGameXXKRunDeckRules::MaxRouteCardCapacity, MergeCardId, EGameXXKCardQuality::Common);
	const FName ExpectedMergeSurvivor = Merge.CardRun.RouteCardEntries[0].EntryId;
	FGameXXKRouteMerchantPurchasePreview MergePreview;
	TestTrue(TEXT("merge-at-capacity preview succeeds"), FGameXXKRouteMerchantRules::PreviewPurchase(Merge, MergeOfferId, NAME_None, MergePreview, &Error));
	TestFalse(TEXT("merge-at-capacity requires no replacement"), MergePreview.bRequiresReplacement);
	TestTrue(TEXT("merge-at-capacity can purchase directly"), MergePreview.bCanPurchase);
	TestEqual(TEXT("merge preview reports stable survivor"), MergePreview.MergeSurvivorEntryId, ExpectedMergeSurvivor);
	TestEqual(TEXT("merge preview reports final rare quality"), MergePreview.FinalQuality, EGameXXKCardQuality::Rare);
	TestEqual(TEXT("merge preview reports zero capacity delta"), MergePreview.CapacityDelta, 0);
	TestEqual(TEXT("merge preview reports one consumed entry"), MergePreview.ConsumedEntryIds.Num(), 1);
	const int32 MergeMoneyBefore = Merge.CardRun.RouteTravelMoney;
	FGameXXKRouteMerchantPurchaseResult MergeResult;
	TestTrue(TEXT("merge-at-capacity purchase commits"), FGameXXKRouteMerchantRules::Purchase(Merge, MergeOfferId, NAME_None, MergeResult));
	int32 CapacityAfterMerge = 0;
	TestTrue(TEXT("merged deck remains valid"), FGameXXKRunDeckRules::GetCapacityUsed(Merge.CardRun.RouteCardEntries, CapacityAfterMerge, &Error));
	TestEqual(TEXT("merge leaves capacity at twelve"), CapacityAfterMerge, FGameXXKRunDeckRules::MaxRouteCardCapacity);
	const FGameXXKRouteCardEntry* Survivor = Merge.CardRun.RouteCardEntries.FindByPredicate([ExpectedMergeSurvivor](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.EntryId == ExpectedMergeSurvivor;
	});
	TestNotNull(TEXT("merge survivor remains"), Survivor);
	if (Survivor) TestEqual(TEXT("merge survivor upgrades to Rare"), Survivor->CurrentQuality, EGameXXKCardQuality::Rare);
	TestEqual(TEXT("merge purchase debits exactly once"), Merge.CardRun.RouteTravelMoney, MergeMoneyBefore - MergePrice);
	TestEqual(TEXT("merge purchase still advances ordinal"), Merge.CardRun.NextRouteCardEntryOrdinal, 13);
	TestEqual(TEXT("merge purchase still records one acquisition"), Merge.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseReplacementTest,
	"GameXXK.Route.Merchant.Rules.Purchase.ReplacementEntryIdentityAndCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseReplacementTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState(false);
	TestTrue(TEXT("replacement fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantOffer* Offer = ForceCommonHeroOffer(State);
	TestNotNull(TEXT("replacement fixture has a card offer"), Offer);
	if (!Offer)
	{
		return false;
	}
	const FName OfferId = Offer->OfferId;
	const int32 Price = Offer->Price;
	FillRouteCapacity(State, FGameXXKRunDeckRules::MaxRouteCardCapacity);
	const TArray<FGameXXKRouteCardEntry> EntriesBefore = State.CardRun.RouteCardEntries;
	const FName ExactReplacementEntryId = EntriesBefore[4].EntryId;
	const FName CardIdMasqueradingAsEntryId = EntriesBefore[4].CardId;
	const FGameXXKRuntimeState BeforePreview = State;
	FGameXXKRouteMerchantPurchasePreview Preview;
	TestTrue(TEXT("full-deck preview succeeds as replacement-required"), FGameXXKRouteMerchantRules::PreviewPurchase(State, OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("full-deck preview requires replacement"), Preview.bRequiresReplacement);
	TestFalse(TEXT("full-deck preview cannot commit without replacement"), Preview.bCanPurchase);
	TestEqual(TEXT("preview exposes capacity delta before replacement"), Preview.CapacityDelta, 1);
	TestEqual(TEXT("preview exposes all twelve stable eligible EntryIds"), Preview.EligibleReplacementEntryIds.Num(), FGameXXKRunDeckRules::MaxRouteCardCapacity);
	for (const FGameXXKRouteCardEntry& Entry : EntriesBefore)
	{
		TestTrue(TEXT("each capacity entry id is eligible"), Preview.EligibleReplacementEntryIds.Contains(Entry.EntryId));
	}
	TestTrue(TEXT("replacement preview is pure"), RuntimeStatesMatch(State, BeforePreview));

	const int32 MoneyBeforePending = State.CardRun.RouteTravelMoney;
	const FGameXXKRouteMerchantState MerchantBeforePending = State.CardRun.RouteMerchant;
	FGameXXKRouteMerchantPurchaseResult PendingResult;
	TestFalse(TEXT("purchase without required replacement does not commit"), FGameXXKRouteMerchantRules::Purchase(State, OfferId, NAME_None, PendingResult));
	TestTrue(TEXT("result explicitly requires replacement"), PendingResult.bRequiresReplacement);
	TestEqual(TEXT("replacement-required is not a failure"), PendingResult.Failure, EGameXXKRouteMerchantPurchaseFailure::None);
	TestTrue(TEXT("pending replacement is persisted"), State.CardRun.RouteMerchant.PendingPurchase.bActive);
	TestEqual(TEXT("pending replacement records offer"), State.CardRun.RouteMerchant.PendingPurchase.OfferId, OfferId);
	TestEqual(TEXT("pending replacement records price"), State.CardRun.RouteMerchant.PendingPurchase.Price, Price);
	TestEqual(TEXT("pending replacement never debits"), State.CardRun.RouteTravelMoney, MoneyBeforePending);
	TestEqual(TEXT("pending replacement never changes deck"), State.CardRun.RouteCardEntries.Num(), EntriesBefore.Num());
	for (int32 Index = 0; Index < EntriesBefore.Num(); ++Index)
	{
		TestTrue(TEXT("pending replacement preserves every deck entry"), FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(&State.CardRun.RouteCardEntries[Index], &EntriesBefore[Index], PPF_None));
	}
	for (int32 Index = 0; Index < MerchantBeforePending.Offers.Num(); ++Index)
	{
		TestTrue(TEXT("pending replacement leaves every offer unchanged"), FGameXXKRouteMerchantOffer::StaticStruct()->CompareScriptStruct(&State.CardRun.RouteMerchant.Offers[Index], &MerchantBeforePending.Offers[Index], PPF_None));
	}
	const FGameXXKRuntimeState PendingBeforeReopen = State;
	TestTrue(TEXT("same-offer pending replacement survives same-node reopen"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("pending same-node reopen is byte-stable"), RuntimeStatesMatch(State, PendingBeforeReopen));

	const FGameXXKRuntimeState BeforeCancel = State;
	TestTrue(TEXT("cancel pending replacement succeeds"), FGameXXKRouteMerchantRules::CancelPendingPurchase(State, &Error));
	TestFalse(TEXT("cancel clears pending only"), State.CardRun.RouteMerchant.PendingPurchase.bActive);
	TestEqual(TEXT("cancel never debits"), State.CardRun.RouteTravelMoney, BeforeCancel.CardRun.RouteTravelMoney);
	TestEqual(TEXT("cancel preserves deck size"), State.CardRun.RouteCardEntries.Num(), BeforeCancel.CardRun.RouteCardEntries.Num());
	TestFalse(TEXT("cancel does not sell offer"), State.CardRun.RouteMerchant.Offers[0].bSold);

	FGameXXKRouteMerchantPurchaseResult PendingAgain;
	TestFalse(TEXT("replacement can be requested again"), FGameXXKRouteMerchantRules::Purchase(State, OfferId, NAME_None, PendingAgain));
	ExpectPurchaseFailureAndRollback(
		*this,
		State,
		OfferId,
		CardIdMasqueradingAsEntryId,
		EGameXXKRouteMerchantPurchaseFailure::InvalidReplacementEntryId,
		TEXT("CardId masquerading as EntryId"));

	const int32 MoneyBeforeCommit = State.CardRun.RouteTravelMoney;
	const int32 GoldBeforeCommit = State.PlayerGold;
	FGameXXKRouteMerchantPurchaseResult CommitResult;
	TestTrue(TEXT("exact stable replacement EntryId commits"), FGameXXKRouteMerchantRules::Purchase(State, OfferId, ExactReplacementEntryId, CommitResult));
	TestTrue(TEXT("replacement result marks purchased"), CommitResult.bPurchased);
	TestEqual(TEXT("replacement result records exact EntryId"), CommitResult.ReplacementEntryId, ExactReplacementEntryId);
	TestFalse(TEXT("successful replacement clears pending"), State.CardRun.RouteMerchant.PendingPurchase.bActive);
	TestEqual(TEXT("successful replacement debits once"), State.CardRun.RouteTravelMoney, MoneyBeforeCommit - Price);
	TestEqual(TEXT("successful replacement preserves permanent gold"), State.PlayerGold, GoldBeforeCommit);
	TestFalse(TEXT("replaced stable entry is removed"), State.CardRun.RouteCardEntries.ContainsByPredicate([ExactReplacementEntryId](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.EntryId == ExactReplacementEntryId;
	}));
	int32 CapacityAfter = 0;
	TestTrue(TEXT("replacement result deck validates"), FGameXXKRunDeckRules::GetCapacityUsed(State.CardRun.RouteCardEntries, CapacityAfter, &Error));
	TestEqual(TEXT("replacement leaves capacity at twelve"), CapacityAfter, FGameXXKRunDeckRules::MaxRouteCardCapacity);
	TestEqual(TEXT("replacement advances ordinal once"), State.CardRun.NextRouteCardEntryOrdinal, 13);
	TestEqual(TEXT("replacement records acquisition once"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 1);
	TestEqual(TEXT("successful replacement remains in merchant"), State.Screen, EGameXXKScreen::RouteMerchant);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseRelicTest,
	"GameXXK.Route.Merchant.Rules.Purchase.RelicUniqueAtomicCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseRelicTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState(false);
	TestTrue(TEXT("relic fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Relic);
	TestNotNull(TEXT("relic fixture has an available relic"), Offer);
	if (!Offer)
	{
		return false;
	}
	const FGameXXKRouteMerchantOffer SavedOffer = *Offer;
	const int32 MoneyBefore = State.CardRun.RouteTravelMoney;
	const int32 GoldBefore = State.PlayerGold;
	const int32 CardOrdinalBefore = State.CardRun.NextRouteCardEntryOrdinal;
	FGameXXKRouteMerchantPurchasePreview Preview;
	const FGameXXKRuntimeState BeforePreview = State;
	TestTrue(TEXT("relic preview succeeds"), FGameXXKRouteMerchantRules::PreviewPurchase(State, Offer->OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("relic preview can purchase"), Preview.bCanPurchase);
	TestEqual(TEXT("relic preview final quality is offer quality"), Preview.FinalQuality, Offer->Quality);
	TestEqual(TEXT("relic preview capacity delta is zero"), Preview.CapacityDelta, 0);
	TestTrue(TEXT("relic preview is pure"), RuntimeStatesMatch(State, BeforePreview));
	FGameXXKRouteMerchantPurchaseResult Result;
	TestTrue(TEXT("relic purchase commits"), FGameXXKRouteMerchantRules::Purchase(State, Offer->OfferId, NAME_None, Result));
	TestEqual(TEXT("relic purchase adds exactly one relic"), State.CardRun.Relics.Num(), 1);
	TestEqual(TEXT("relic purchase adds selected id"), State.CardRun.Relics[0].RelicId, SavedOffer.ContentId);
	TestEqual(TEXT("relic purchase debits once"), State.CardRun.RouteTravelMoney, MoneyBefore - SavedOffer.Price);
	TestEqual(TEXT("relic purchase preserves permanent gold"), State.PlayerGold, GoldBefore);
	TestEqual(TEXT("relic purchase leaves card ordinal untouched"), State.CardRun.NextRouteCardEntryOrdinal, CardOrdinalBefore);
	TestTrue(TEXT("relic purchase marks offer sold"), State.CardRun.RouteMerchant.Offers[3].bSold);
	TestEqual(TEXT("relic purchase remains in merchant"), State.Screen, EGameXXKScreen::RouteMerchant);
	const FGameXXKRuntimeState SoldRelicBeforeReopen = State;
	TestTrue(TEXT("same-node reopen accepts sold relic stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("sold relic reopen is byte-stable"), RuntimeStatesMatch(State, SoldRelicBeforeReopen));

	FGameXXKRuntimeState RelicOverflow = MakeMerchantState(false);
	TestTrue(TEXT("relic overflow fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(RelicOverflow, &Error));
	FGameXXKRouteMerchantOffer* OverflowOffer = FindAvailableOffer(RelicOverflow, EGameXXKRouteMerchantOfferKind::Relic);
	TestNotNull(TEXT("relic overflow fixture has offer"), OverflowOffer);
	if (OverflowOffer)
	{
		RelicOverflow.CardRun.NextRelicAcquisitionOrdinal = MAX_int32;
		ExpectPurchaseFailureAndRollback(
			*this,
			RelicOverflow,
			OverflowOffer->OfferId,
			NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow,
			TEXT("MAX relic ordinal purchase"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSavedStockValidationTest,
	"GameXXK.Route.Merchant.SaveState.CanonicalValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSavedStockValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState(true);
	if (!TestTrue(TEXT("saved-stock fixture generates canonical companion stock"),
		FGameXXKRouteMerchantRules::EnsureStock(State, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("canonical generated stock passes the context-free save validator"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(State, &Error));

	FGameXXKRuntimeState AwayFromMerchant = State;
	AwayFromMerchant.Screen = EGameXXKScreen::DungeonMap;
	AwayFromMerchant.PendingRouteNodeId = INDEX_NONE;
	TestTrue(TEXT("saved stock remains valid after leaving its active merchant screen"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(AwayFromMerchant, &Error));

	FGameXXKRuntimeState UnavailableSlot = State;
	const FName RemovedContentId = UnavailableSlot.CardRun.RouteMerchant.Offers[0].ContentId;
	FGameXXKRouteMerchantOffer& EmptyOffer = UnavailableSlot.CardRun.RouteMerchant.Offers[0];
	EmptyOffer.ContentId = NAME_None;
	EmptyOffer.Quality = EGameXXKCardQuality::Invalid;
	EmptyOffer.Price = 0;
	EmptyOffer.bUnavailable = true;
	EmptyOffer.bSold = false;
	TestTrue(TEXT("a canonical unavailable slot is represented by an empty persisted payload"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(UnavailableSlot, &Error));
	UnavailableSlot.CardRun.RouteMerchant.Offers[0].ContentId = RemovedContentId;
	TestFalse(TEXT("an unavailable slot cannot retain catalog content"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(UnavailableSlot, &Error));

	FGameXXKRuntimeState WrongCount = State;
	WrongCount.CardRun.RouteMerchant.Offers.Pop();
	TestFalse(TEXT("saved stock requires exactly three card and three relic slots"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongCount, &Error));

	FGameXXKRuntimeState WrongPrice = State;
	++WrongPrice.CardRun.RouteMerchant.Offers[0].Price;
	TestFalse(TEXT("saved card prices are derived from catalog quality"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongPrice, &Error));

	FGameXXKRuntimeState WrongQuality = State;
	WrongQuality.CardRun.RouteMerchant.Offers[3].Quality =
		WrongQuality.CardRun.RouteMerchant.Offers[3].Quality == EGameXXKCardQuality::Common
			? EGameXXKCardQuality::Rare
			: EGameXXKCardQuality::Common;
	TestFalse(TEXT("saved relic quality must match the catalog"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongQuality, &Error));

	FGameXXKRuntimeState WrongSeed = State;
	WrongSeed.CardRun.RouteMerchant.OfferSeed ^= 1;
	TestFalse(TEXT("saved stock seed is derived from route, node, and refresh count"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongSeed, &Error));

	FGameXXKRuntimeState WrongRefreshIdentity = State;
	++WrongRefreshIdentity.CardRun.RouteMerchant.RefreshCount;
	TestFalse(TEXT("refresh count and every persisted offer identity must stay in sync"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongRefreshIdentity, &Error));

	FGameXXKRuntimeState WrongOfferIdentity = State;
	WrongOfferIdentity.CardRun.RouteMerchant.Offers[0].OfferId = TEXT("Merchant.Tampered.C.0");
	TestFalse(TEXT("saved offer identities are deterministic and slot-stable"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongOfferIdentity, &Error));

	const FName HeroOfferId = State.CardRun.RouteMerchant.Offers[0].ContentId;
	FGameXXKRuntimeState LockedHero = State;
	LockedHero.CardRun.HeroUnlockedCardIds.Remove(HeroOfferId);
	TestFalse(TEXT("slot zero cannot persist a hero card that is no longer unlocked in the save"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(LockedHero, &Error));

	const TArray<FName> GuardCards = FindCardIdsByOwner(
		EGameXXKCardOwner::Profession,
		EGameXXKCharacterRole::Guard);
	if (!TestTrue(TEXT("ownership fixture has a non-Blade profession card"), !GuardCards.IsEmpty()))
	{
		return false;
	}
	const FGameXXKCardDefinition* GuardDefinition = FGameXXKCardCatalog::FindCardDefinition(GuardCards[0]);
	if (!TestNotNull(TEXT("ownership fixture resolves its non-Blade card"), GuardDefinition))
	{
		return false;
	}
	FGameXXKRuntimeState WrongCompanionRole = State;
	FGameXXKRouteMerchantOffer& CompanionOffer = WrongCompanionRole.CardRun.RouteMerchant.Offers[1];
	CompanionOffer.ContentId = GuardDefinition->Id;
	CompanionOffer.Quality = GuardDefinition->BaseQuality;
	CompanionOffer.Price = FGameXXKCardQualityRules::GetCardPrice(GuardDefinition->BaseQuality);
	TestFalse(TEXT("slot one accepts only the active companion's role and exact personal pool"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongCompanionRole, &Error));

	FGameXXKRuntimeState WrongRouteSlot = State;
	FGameXXKRouteMerchantOffer& HeroSlot = WrongRouteSlot.CardRun.RouteMerchant.Offers[0];
	FGameXXKRouteMerchantOffer& RouteSlot = WrongRouteSlot.CardRun.RouteMerchant.Offers[2];
	Swap(HeroSlot.ContentId, RouteSlot.ContentId);
	Swap(HeroSlot.Quality, RouteSlot.Quality);
	Swap(HeroSlot.Price, RouteSlot.Price);
	TestFalse(TEXT("slot two remains route-owned even when slot-zero fallback can be route-owned"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongRouteSlot, &Error));

	FGameXXKRuntimeState SoldSnapshot = State;
	SoldSnapshot.CardRun.RouteMerchant.Offers[0].bSold = true;
	AddEpicOwnedEntry(SoldSnapshot, SoldSnapshot.CardRun.RouteMerchant.Offers[0].ContentId, 77);
	SoldSnapshot.CardRun.RouteMerchant.Offers[3].bSold = true;
	FGameXXKRelicInstance PurchasedRelic;
	PurchasedRelic.RelicId = SoldSnapshot.CardRun.RouteMerchant.Offers[3].ContentId;
	PurchasedRelic.Stacks = 1;
	PurchasedRelic.AcquisitionOrdinal = 1;
	SoldSnapshot.CardRun.Relics.Add(PurchasedRelic);
	SoldSnapshot.CardRun.NextRelicAcquisitionOrdinal = 1;
	TestTrue(TEXT("sold snapshot content remains valid after Epic-card and relic ownership changes"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(SoldSnapshot, &Error));

	FGameXXKRuntimeState MatchingPending = State;
	const FGameXXKRouteMerchantOffer& PendingOffer = MatchingPending.CardRun.RouteMerchant.Offers[0];
	MatchingPending.CardRun.RouteMerchant.PendingPurchase.bActive = true;
	MatchingPending.CardRun.RouteMerchant.PendingPurchase.OfferId = PendingOffer.OfferId;
	MatchingPending.CardRun.RouteMerchant.PendingPurchase.CardId = PendingOffer.ContentId;
	MatchingPending.CardRun.RouteMerchant.PendingPurchase.Price = PendingOffer.Price;
	TestTrue(TEXT("a pending replacement that exactly matches an unsold card offer is valid"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(MatchingPending, &Error));
	++MatchingPending.CardRun.RouteMerchant.PendingPurchase.Price;
	TestFalse(TEXT("pending replacement metadata must match its offer exactly"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(MatchingPending, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantV10SaveMigrationTest,
	"GameXXK.Route.Merchant.SaveState.V10Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantV10SaveMigrationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState CurrentState;
	if (!TestTrue(TEXT("current-save fixture enters a generated route"), StartAcceptedGeneratedRoute(CurrentState))
		|| !TestTrue(TEXT("current-save fixture opens a generated merchant"), OpenGeneratedMerchant(CurrentState, &Error)))
	{
		return false;
	}
	const FGameXXKRouteMerchantState CurrentMerchant = CurrentState.CardRun.RouteMerchant;
	FGameXXKSaveState CurrentSave = UGameXXKMVPRules::MakeSaveState(CurrentState);
	FGameXXKSaveState CurrentRoundTrip;
	FGameXXKSaveMigrationReport CurrentReport;
	TestTrue(TEXT("a current-v10 save accepts canonical generated merchant stock"),
		FGameXXKSaveMigration::MigrateToCurrent(CurrentSave, CurrentRoundTrip, CurrentReport));
	TestTrue(TEXT("current-v10 validation preserves merchant stock byte-for-byte"),
		MerchantStatesMatch(CurrentRoundTrip.RuntimeState.CardRun.RouteMerchant, CurrentMerchant));
	FGameXXKRuntimeState OffscreenCurrentState = CurrentState;
	OffscreenCurrentState.Screen = EGameXXKScreen::DungeonMap;
	OffscreenCurrentState.CurrentMapId = TEXT("HuangshanRoute");
	OffscreenCurrentState.PendingRouteNodeId = INDEX_NONE;
	FGameXXKSaveState OffscreenCurrentSave = UGameXXKMVPRules::MakeSaveState(OffscreenCurrentState);
	FGameXXKSaveState OffscreenCurrentRoundTrip;
	FGameXXKSaveMigrationReport OffscreenCurrentReport;
	TestTrue(TEXT("a current-v10 save accepts retained stock after the merchant node is no longer pending"),
		FGameXXKSaveMigration::MigrateToCurrent(
			OffscreenCurrentSave,
			OffscreenCurrentRoundTrip,
			OffscreenCurrentReport));
	TestTrue(TEXT("offscreen current-v10 validation preserves merchant stock byte-for-byte"),
		MerchantStatesMatch(
			OffscreenCurrentRoundTrip.RuntimeState.CardRun.RouteMerchant,
			CurrentMerchant));

	FGameXXKRuntimeState LegacyState;
	if (!TestTrue(TEXT("v9 fixture enters a generated route"), StartAcceptedGeneratedRoute(LegacyState))
		|| !TestTrue(TEXT("v9 fixture generates stock before a later reward"), OpenGeneratedMerchant(LegacyState, &Error)))
	{
		return false;
	}
	const FGameXXKRouteMapNode* BattleNode = FindNodeOfKind(LegacyState, EGameXXKNodeKind::Battle);
	if (!TestNotNull(TEXT("v9 fixture has a generated battle node"), BattleNode))
	{
		return false;
	}
	LegacyState.Screen = EGameXXKScreen::DungeonMap;
	LegacyState.CurrentMapId = TEXT("HuangshanRoute");
	LegacyState.PendingRouteNodeId = INDEX_NONE;
	LegacyState.ReachableRouteNodeIds = {BattleNode->NodeId};
	if (!TestTrue(TEXT("v9 fixture starts a generated battle after retaining merchant stock"),
		UGameXXKMVPRules::SelectRouteNodeById(LegacyState, BattleNode->NodeId)))
	{
		return false;
	}
	ForceCardBattleVictory(LegacyState);
	if (!TestTrue(TEXT("v9 fixture creates a real pending route reward"),
		UGameXXKMVPRules::ResolveBattleVictory(LegacyState, false)))
	{
		return false;
	}
	TestEqual(TEXT("v9 fixture has three pending reward cards"), LegacyState.CardRun.PendingReward.CardIds.Num(), 3);

	const TArray<FGameXXKRouteMapNode> ExpectedNodes = LegacyState.RouteMapNodes;
	const TArray<FGameXXKRouteMapEdge> ExpectedEdges = LegacyState.RouteMapEdges;
	const int32 ExpectedCurrentNodeId = LegacyState.CurrentRouteNodeId;
	const int32 ExpectedPendingNodeId = LegacyState.PendingRouteNodeId;
	const FGameXXKPendingRouteCardReward ExpectedReward = LegacyState.CardRun.PendingReward;
	const TArray<FGameXXKRouteCardEntry> ExpectedEntries = LegacyState.CardRun.RouteCardEntries;
	const int32 ExpectedNextEntryOrdinal = LegacyState.CardRun.NextRouteCardEntryOrdinal;
	const bool bExpectedEconomyInitialized = LegacyState.CardRun.bRouteEconomyInitialized;
	const int32 ExpectedTravelMoney = LegacyState.CardRun.RouteTravelMoney;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyState);
	LegacySave.SaveVersion = FGameXXKSaveMigration::RouteMerchantStockSchemaIntroducedSaveVersion - 1;
	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v9 migrates by discarding only its obsolete merchant stock schema"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, MigratedSave, Report));
	TestTrue(TEXT("v9 migration leaves a canonical empty merchant snapshot"),
		MerchantStatesMatch(MigratedSave.RuntimeState.CardRun.RouteMerchant, FGameXXKRouteMerchantState()));
	TestTrue(TEXT("v9 merchant migration preserves route nodes byte-for-byte"),
		RouteNodesMatch(MigratedSave.RuntimeState.RouteMapNodes, ExpectedNodes));
	TestTrue(TEXT("v9 merchant migration preserves route edges byte-for-byte"),
		RouteEdgesMatch(MigratedSave.RuntimeState.RouteMapEdges, ExpectedEdges));
	TestEqual(TEXT("v9 merchant migration preserves the current node"),
		MigratedSave.RuntimeState.CurrentRouteNodeId, ExpectedCurrentNodeId);
	TestEqual(TEXT("v9 merchant migration preserves the pending node"),
		MigratedSave.RuntimeState.PendingRouteNodeId, ExpectedPendingNodeId);
	TestTrue(TEXT("v9 merchant migration preserves the pending reward byte-for-byte"),
		FGameXXKPendingRouteCardReward::StaticStruct()->CompareScriptStruct(
			&MigratedSave.RuntimeState.CardRun.PendingReward,
			&ExpectedReward,
			PPF_None));
	TestEqual(TEXT("v9 merchant migration preserves the stable deck size"),
		MigratedSave.RuntimeState.CardRun.RouteCardEntries.Num(), ExpectedEntries.Num());
	if (MigratedSave.RuntimeState.CardRun.RouteCardEntries.Num() == ExpectedEntries.Num())
	{
		for (int32 Index = 0; Index < ExpectedEntries.Num(); ++Index)
		{
			TestTrue(*FString::Printf(TEXT("v9 merchant migration preserves deck entry %d"), Index),
				FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(
					&MigratedSave.RuntimeState.CardRun.RouteCardEntries[Index],
					&ExpectedEntries[Index],
					PPF_None));
		}
	}
	TestEqual(TEXT("v9 merchant migration preserves the next stable deck ordinal"),
		MigratedSave.RuntimeState.CardRun.NextRouteCardEntryOrdinal, ExpectedNextEntryOrdinal);
	TestEqual(TEXT("v9 merchant migration preserves route-economy initialization"),
		MigratedSave.RuntimeState.CardRun.bRouteEconomyInitialized, bExpectedEconomyInitialized);
	TestEqual(TEXT("v9 merchant migration preserves route travel money"),
		MigratedSave.RuntimeState.CardRun.RouteTravelMoney, ExpectedTravelMoney);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSaveStateTest,
	"GameXXK.Route.Merchant.SaveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSaveStateTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState NewRunState = UGameXXKMVPRules::CreateNewGame();
	const FGameXXKPendingRouteMerchantPurchase EmptyPendingPurchase;
	TestFalse(TEXT("required pending-purchase API defaults inactive"), EmptyPendingPurchase.bActive);
	TestEqual(TEXT("new run has no merchant source node"), NewRunState.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("new run has no merchant seed"), NewRunState.CardRun.RouteMerchant.OfferSeed, 0);
	TestTrue(TEXT("new run has no merchant offers"), NewRunState.CardRun.RouteMerchant.Offers.IsEmpty());
	TestFalse(TEXT("new run has no pending merchant purchase"), NewRunState.CardRun.RouteMerchant.PendingPurchase.bActive);

	const FGameXXKSaveState LegacySave = MakeLegacyMerchantSnapshot();
	const TArray<FGameXXKRouteMapNode> ExpectedRouteNodes = LegacySave.RuntimeState.RouteMapNodes;
	const TArray<FGameXXKRouteMapEdge> ExpectedRouteEdges = LegacySave.RuntimeState.RouteMapEdges;
	const int32 ExpectedCurrentNodeId = LegacySave.RuntimeState.CurrentRouteNodeId;
	const int32 ExpectedPendingNodeId = LegacySave.RuntimeState.PendingRouteNodeId;
	const FGameXXKRouteProgress ExpectedRouteProgress = LegacySave.RuntimeState.CardRun.RouteProgress;
	TestTrue(TEXT("legacy fixture contains a nonempty merchant snapshot"), !LegacySave.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty());
	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("older save clears an obsolete merchant snapshot"), FGameXXKSaveMigration::MigrateToCurrent(LegacySave, MigratedSave, Report));
	TestTrue(TEXT("migration report succeeds"), Report.bSucceeded);
	TestEqual(TEXT("migrated legacy run has no merchant source node"), MigratedSave.RuntimeState.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("migrated legacy run has no merchant seed"), MigratedSave.RuntimeState.CardRun.RouteMerchant.OfferSeed, 0);
	TestTrue(TEXT("migrated legacy run has no merchant offers"), MigratedSave.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty());
	TestFalse(TEXT("migrated legacy run has no pending merchant purchase"), MigratedSave.RuntimeState.CardRun.RouteMerchant.PendingPurchase.bActive);
	TestTrue(TEXT("legacy merchant reset leaves route nodes byte-identical"), RouteNodesMatch(MigratedSave.RuntimeState.RouteMapNodes, ExpectedRouteNodes));
	TestTrue(TEXT("legacy merchant reset leaves route edges byte-identical"), RouteEdgesMatch(MigratedSave.RuntimeState.RouteMapEdges, ExpectedRouteEdges));
	TestEqual(TEXT("legacy merchant reset leaves current node unchanged"), MigratedSave.RuntimeState.CurrentRouteNodeId, ExpectedCurrentNodeId);
	TestEqual(TEXT("legacy merchant reset leaves pending node unchanged"), MigratedSave.RuntimeState.PendingRouteNodeId, ExpectedPendingNodeId);
	TestTrue(
		TEXT("legacy merchant reset leaves three-chapter route progress byte-identical"),
		FGameXXKRouteProgress::StaticStruct()->CompareScriptStruct(
			&MigratedSave.RuntimeState.CardRun.RouteProgress,
			&ExpectedRouteProgress,
			PPF_None));
	return true;
}

#endif
