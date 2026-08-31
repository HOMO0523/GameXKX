#include "MVP/GameXXKTutorial01SessionSubsystem.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "Engine/GameInstance.h"
#include "Guide/GameXXKGuideAsset.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01SessionTest,
	"GameXXK.Tutorial01.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01SessionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* BattleRuntime =
		NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	if (!TestTrue(TEXT("tutorial fixture establishes the real Qingshan runtime"),
		BattleRuntime
		&& BattleRuntime->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}
	FGameXXKRuntimeState Before = BattleRuntime->GetRuntimeStateCopy();
	TestEqual(TEXT("tutorial fixture owns all six permanent partners"),
		Before.CardRun.CompanionRoster.PermanentCompanions.Num(),
		6);
	TestEqual(TEXT("tutorial fixture owns a valid three-member formation"),
		Before.CardRun.OrderedFormation.Members.Num(),
		FGameXXKPartyFormationRules::PartySize);
	Before.Screen = EGameXXKScreen::Town;
	Before.PlayerGold = 4321;
	Before.RouteSeed = 24680;
	Before.CurrentRouteNodeId = 17;
	Before.Training.SelectedStageId = TEXT("Training.Normal.1-1");
	Before.Inventory.Add(UGameXXKMVPRules::ItemTutorialRiverMap(), 1);
	const FTransform StatueReturn(
		FRotator(0.0f, 90.0f, 0.0f),
		FVector(19930.0f, 5240.0f, 1610.0f),
		FVector::OneVector);

	UGameXXKTutorial01SessionSubsystem* Session =
		NewObject<UGameXXKTutorial01SessionSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("tutorial session creates"), Session))
	{
		return false;
	}
	TestFalse(TEXT("new tutorial session is inactive"), Session->HasActiveSession());
	TestFalse(TEXT("unset preference cannot begin tutorial"),
		Session->BeginFromTown(
			Before,
			StatueReturn,
			EGameXXKGuidePreference::Unset));
	TestTrue(TEXT("experienced player begins the same tutorial session"),
		Session->BeginFromTown(
			Before,
			StatueReturn,
			EGameXXKGuidePreference::ExperiencedPlayer));
	TestTrue(TEXT("tutorial session is active"), Session->HasActiveSession());
	TestEqual(TEXT("experienced preference remains transient"),
		Session->GetGuidePreference(),
		EGameXXKGuidePreference::ExperiencedPlayer);
	TestFalse(TEXT("active tutorial rejects duplicate begin"),
		Session->BeginFromTown(
			Before,
			StatueReturn,
			EGameXXKGuidePreference::NewPlayer));

	FGameXXKRuntimeState RouteRuntime;
	TestTrue(TEXT("session projects route runtime"),
		Session->BuildRouteRuntime(RouteRuntime));
	TestEqual(TEXT("route boot is not battle"),
		RouteRuntime.Screen,
		EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("route boot has no active card battle"),
		RouteRuntime.CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("route boot has no active legacy battle"),
		RouteRuntime.bHasActiveBattle);
	TestEqual(TEXT("route projection preserves gold"), RouteRuntime.PlayerGold, 4321);
	TestEqual(TEXT("fixed route exposes three nodes"), Session->BuildRouteNodes().Num(), 3);
	TestEqual(TEXT("fixed route exposes two edges"), Session->BuildRouteEdges().Num(), 2);
	TestEqual(TEXT("fixed route labels all three nodes"), Session->BuildRouteLabels().Num(), 3);
	TestTrue(TEXT("route completion notice starts empty"),
		Session->BuildRouteCompletionNotice().IsEmpty());

	EGameXXKTutorial01RouteAction RouteAction = EGameXXKTutorial01RouteAction::None;
	TestTrue(TEXT("session accepts the battle node"),
		Session->RequestRouteNode(
			FGameXXKTutorial01RouteRules::BattleNodeId,
			RouteAction));
	TestEqual(TEXT("session reports battle action"),
		RouteAction,
		EGameXXKTutorial01RouteAction::StartBattle);

	FGameXXKRuntimeState BattleSeed;
	TestTrue(FString::Printf(TEXT("session builds battle seed: %s"), *Error),
		Session->BuildBattleSeedRuntime(BattleSeed, &Error));
	FName TutorialNpc;
	TestTrue(FString::Printf(TEXT("tutorial formation resolves NPC: %s"), *Error),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			BattleSeed,
			TutorialNpc,
			&Error));
	TestEqual(TEXT("YueBai replaces Tusi only in seed"),
		TutorialNpc,
		FName(TEXT("Npc.YueBai")));
	const TArray<FName> RequiredTutorialCards = {
		TEXT("Hero.Generic.HengJianShouShi"),
		TEXT("Hero.Generic.SuiYanJi"),
		TEXT("Hero.Generic.FengShenBu"),
	};
	for (const FName RequiredCardId : RequiredTutorialCards)
	{
		TestTrue(
			FString::Printf(TEXT("battle seed contains %s"), *RequiredCardId.ToString()),
			BattleSeed.CardRun.HeroSelectedCardIds.Contains(RequiredCardId));
	}
	TestTrue(TEXT("snapshot keeps the player's original hero loadout"),
		Session->GetContextForTest().RuntimeBeforeTutorial.CardRun.HeroSelectedCardIds
			== Before.CardRun.HeroSelectedCardIds);
	TestTrue(TEXT("snapshot still has original formation"),
		Session->GetContextForTest().RuntimeBeforeTutorial.CardRun.OrderedFormation.Members
			== Before.CardRun.OrderedFormation.Members);
	FName SnapshotNpc;
	TestTrue(TEXT("snapshot NPC still resolves"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Session->GetContextForTest().RuntimeBeforeTutorial,
			SnapshotNpc,
			&Error));
	TestEqual(TEXT("snapshot retains Tusi"), SnapshotNpc, FName(TEXT("Npc.TusiChief")));

	BattleRuntime->GetMutableRuntimeState() = BattleSeed;
	TestTrue(FString::Printf(TEXT("0-1 encounter starts from tutorial seed: %s"), *Error),
		BattleRuntime->StartNarrativeEncounter(
			TEXT("Encounter.Main.XuXiake.0-1"),
			&Error));
	const TArray<FName> ActiveInstanceIdsBefore =
		BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.ActiveInstanceIds;
	const int32 ZoneCountBefore =
		BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.Num()
		+ BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.DrawPile.Num()
		+ BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.DiscardPile.Num()
		+ BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.ExhaustPile.Num()
		+ BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck.PendingAutomaticHandCards.Num();
	TestTrue(FString::Printf(TEXT("tutorial opening hand is arranged: %s"), *Error),
		Session->ArrangeDeterministicOpeningHand(
			BattleRuntime->GetMutableRuntimeState(),
			&Error));
	const FGameXXKBattleDeckState& Deck =
		BattleRuntime->GetRuntimeState().CardRun.ActiveBattle.Deck;
	TestTrue(TEXT("tutorial opening hand contains at least three cards"), Deck.Hand.Num() >= 3);
	if (Deck.Hand.Num() >= 3)
	{
		TestEqual(TEXT("HengJian is the leftmost card"),
			Deck.Hand[0].CardId,
			FName(TEXT("Hero.Generic.HengJianShouShi")));
		TestEqual(TEXT("SuiYan is the second card"),
			Deck.Hand[1].CardId,
			FName(TEXT("Hero.Generic.SuiYanJi")));
		TestEqual(TEXT("FengShen is the third card"),
			Deck.Hand[2].CardId,
			FName(TEXT("Hero.Generic.FengShenBu")));
		for (int32 CardIndex = 0; CardIndex < 3; ++CardIndex)
		{
			TestEqual(
				FString::Printf(TEXT("tutorial card %d belongs to hero"), CardIndex),
				Deck.Hand[CardIndex].OwnerUnitId,
				FGameXXKEquipmentRules::HeroCharacterId());
		}
	}
	TestTrue(TEXT("opening arrangement preserves the instance ledger"),
		Deck.ActiveInstanceIds == ActiveInstanceIdsBefore);
	const int32 ZoneCountAfter = Deck.Hand.Num()
		+ Deck.DrawPile.Num()
		+ Deck.DiscardPile.Num()
		+ Deck.ExhaustPile.Num()
		+ Deck.PendingAutomaticHandCards.Num();
	TestEqual(TEXT("opening arrangement preserves zone count"), ZoneCountAfter, ZoneCountBefore);

	FGameXXKRuntimeState RetryState;
	TestTrue(TEXT("retry restores the pre-tutorial runtime"),
		Session->PrepareRetry(RetryState));
	TestEqual(TEXT("retry restores gold"), RetryState.PlayerGold, 4321);
	TestEqual(TEXT("retry restores route seed"), RetryState.RouteSeed, 24680);
	TestEqual(TEXT("retry restores selected training stage"),
		RetryState.Training.SelectedStageId,
		FName(TEXT("Training.Normal.1-1")));
	TestEqual(TEXT("retry preserves the unique tutorial map item"),
		UGameXXKMVPRules::GetItemCount(
			RetryState,
			UGameXXKMVPRules::ItemTutorialRiverMap()),
		1);

	FGameXXKRuntimeState ReturnState;
	TestTrue(TEXT("victory prepares the exact town snapshot"),
		Session->RestoreForTownReturn(
			EGameXXKTutorial01ReturnReason::Victory,
			ReturnState));
	TestEqual(TEXT("town return remains Town"), ReturnState.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("town return restores route node"), ReturnState.CurrentRouteNodeId, 17);

	FGameXXKTutorial01ReturnContext ReturnContext;
	TestTrue(TEXT("town consumes one tutorial return"),
		Session->ConsumeTownReturn(ReturnContext));
	TestEqual(TEXT("return reason is victory"),
		ReturnContext.ReturnReason,
		EGameXXKTutorial01ReturnReason::Victory);
	TestEqual(TEXT("return transform survives travel"),
		ReturnContext.StatueReturnTransform,
		StatueReturn);
	TestEqual(TEXT("return preference survives travel"),
		ReturnContext.GuidePreference,
		EGameXXKGuidePreference::ExperiencedPlayer);
	TestFalse(TEXT("return context is one-shot"),
		Session->ConsumeTownReturn(ReturnContext));
	TestFalse(TEXT("consumed tutorial session is inactive"),
		Session->HasActiveSession());

	return true;
}

#endif
