#include "GameXXKMVPRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteLifecycleTest,
	"GameXXK.Integration.CardRoute.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteLifecycleTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("route lifecycle fixture starts a complete game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
	TestTrue(TEXT("Yue Bai becomes the persistent NPC before route entry"),
		FGameXXKPartyFormationRules::SetQuestNpc(State, TEXT("Npc.YueBai")));
	TestTrue(TEXT("the accepted Qingshan quest enters a card-ready route"),
		UGameXXKMVPRules::AcceptTownQuest(State)
		&& UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("entering the route initializes and locks the permanent card loadout"),
		State.CardRun.bLoadoutLockedForRoute);
	FName NpcDuringRoute;
	TestTrue(TEXT("route resolves the frozen NPC from ordered formation"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcDuringRoute));
	TestEqual(TEXT("route keeps Yue Bai"), NpcDuringRoute, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("route keeps Yue Bai's card projection"),
		State.CardRun.PartySelection.QuestNpc.NpcId,
		FName(TEXT("Npc.YueBai")));
	const FGameXXKRuntimeState LockedBefore = State;
	TestFalse(TEXT("route lock rejects NPC replacement"),
		FGameXXKPartyFormationRules::SetQuestNpc(State, TEXT("Npc.JinGui")));
	TestTrue(TEXT("rejected route replacement is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&State,
			&LockedBefore,
			PPF_None));
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
	TestTrue(TEXT("failing a route preserves the permanent NPC and clears pending events"),
		UGameXXKMVPRules::FailDungeonToTown(State));
	TestFalse(TEXT("the returned town state no longer has a route loadout lock"), State.CardRun.bLoadoutLockedForRoute);
	FName NpcAfterDefeat;
	TestTrue(TEXT("NPC still resolves after defeat"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcAfterDefeat));
	TestEqual(TEXT("defeat preserves Yue Bai"), NpcAfterDefeat, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("defeat preserves Yue Bai's card projection"),
		State.CardRun.PartySelection.QuestNpc.NpcId,
		FName(TEXT("Npc.YueBai")));
	TestTrue(TEXT("temporary route provenance remains retired"),
		State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
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
