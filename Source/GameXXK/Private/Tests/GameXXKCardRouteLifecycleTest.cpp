#include "GameXXKMVPRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRouteEconomyRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteLifecycleTest,
	"GameXXK.Integration.CardRoute.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteLifecycleTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the accepted Qingshan quest enters a card-ready route"),
		UGameXXKMVPRules::OpenWorldMap(State)
		&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(State)
		&& UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("entering the route initializes and locks the permanent card loadout"),
		State.CardRun.bLoadoutLockedForRoute);
	TestTrue(TEXT("a named task NPC can still join after the route lock but before combat"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.YueBai"), {}));
	State.CardRun.RouteCardIds = { TEXT("Hero.FlyingCloud"), TEXT("Npc.YueBai.MoonWard") };
	State.CardRun.PendingEvent.SourceNodeId = 71;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	State.CardRun.RouteMerchant.SourceNodeId = 71;
	State.CardRun.RouteMerchant.OfferSeed = 0x7135;
	State.CardRun.RouteMerchant.RefreshCount = 2;
	FGameXXKRouteMerchantOffer MerchantOffer;
	MerchantOffer.OfferId = TEXT("Merchant.Terminal.Sentinel");
	MerchantOffer.Kind = EGameXXKRouteMerchantOfferKind::Card;
	MerchantOffer.ContentId = TEXT("Route.General.PoJiaTuCi");
	MerchantOffer.Quality = EGameXXKCardQuality::Common;
	MerchantOffer.Price = 15;
	State.CardRun.RouteMerchant.Offers.Add(MerchantOffer);
	TestTrue(TEXT("failing a route removes temporary NPC, transient route cards, and pending events"),
		UGameXXKMVPRules::FailDungeonToTown(State));
	TestFalse(TEXT("the returned town state no longer has a route loadout lock"), State.CardRun.bLoadoutLockedForRoute);
	TestTrue(TEXT("the returned town state removes the temporary task NPC"), State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestTrue(TEXT("the returned town state removes route-only cards"), State.CardRun.RouteCardIds.IsEmpty());
	TestTrue(TEXT("the returned town state removes pending events"), State.CardRun.PendingEvent.EventNpcId.IsNone());
	const FGameXXKRouteMerchantState EmptyMerchant;
	TestTrue(TEXT("the returned town state removes the terminal route's merchant snapshot"),
		FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(
			&State.CardRun.RouteMerchant,
			&EmptyMerchant,
			PPF_None));

	FGameXXKRuntimeState EventState = UGameXXKMVPRules::CreateNewGame();
	EventState.bDungeonActive = true;
	EventState.bHasGeneratedRouteMap = true;
	EventState.Screen = EGameXXKScreen::RouteEvent;
	EventState.RouteMapNodes = { FGameXXKRouteMapNode(79, 2, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.4f), {}) };
	EventState.PendingRouteNodeId = 79;
	EventState.CardRun.RouteProgress.CurrentChapter = 1;
	TestTrue(TEXT("event fixture initializes its route economy"), FGameXXKRouteEconomyRules::InitializeRoute(EventState.CardRun));
	EventState.CardRun.PendingEvent.SourceNodeId = 79;
	EventState.CardRun.PendingEvent.EventNpcId = TEXT("Npc.ZhouGuangZu");
	TestTrue(TEXT("taking a normal event reward resolves its route node"), UGameXXKMVPRules::ResolveEventReward(EventState, true));
	TestTrue(TEXT("taking a normal event reward clears its stale NPC invitation"), EventState.CardRun.PendingEvent.EventNpcId.IsNone());

	return true;
}

#endif
