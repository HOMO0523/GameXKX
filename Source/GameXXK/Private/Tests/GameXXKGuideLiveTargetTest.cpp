#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKTrainingRules.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "UI/GameXXKRouteMerchantWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideLiveWidgetTargetRegistrationTest,
	"GameXXK.Guide.LiveTargets.WidgetRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideLiveWidgetTargetRegistrationTest::RunTest(const FString& Parameters)
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	Registry.Reset();
	UGameXXKMVPSubsystem* BattleSubsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKMVPSubsystem* RouteSubsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKOneGameRouteMapWidget* RouteMap = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* BattleBoard = NewObject<UGameXXKBattleBoardWidget>();
	UGameXXKRouteMerchantWidget* Merchant = NewObject<UGameXXKRouteMerchantWidget>();
	UGameXXKRouteEncounterPanelWidget* Encounter = NewObject<UGameXXKRouteEncounterPanelWidget>();
	FString BattleError;
	if (!TestTrue(TEXT("live-target fixtures exist"),
		BattleSubsystem
		&& BattleSubsystem->StartGame()
		&& BattleSubsystem->StartNarrativeEncounter(TEXT("Encounter.Main.XuXiake.0-1"), &BattleError)
		&& RouteSubsystem
		&& RouteSubsystem->StartGame()
		&& RouteSubsystem->StartTrainingChallenge(
			FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1))
		&& RouteMap && BattleBoard && Merchant && Encounter))
	{
		return false;
	}
	Registry.SetActionGate(BattleBoard, [](const FName ActionId) { return true; });
	RouteMap->SetMVPSubsystem(RouteSubsystem);
	const TSharedRef<SWidget> RouteSlate = RouteMap->TakeWidget();
	RouteMap->RefreshFromState();
	BattleBoard->SetMVPSubsystem(BattleSubsystem);
	const TSharedRef<SWidget> BattleSlate = BattleBoard->TakeWidget();
	BattleBoard->RefreshFromState();
	FName TargetedCardInstanceId;
	for (const FGameXXKCardInstance& Card : BattleSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand)
	{
		FGameXXKCardPlayPreview Preview;
		if (FGameXXKCardBattleAdapter::BuildCardPlayPreview(
			BattleSubsystem->GetRuntimeState(), Card.InstanceId, Preview, &BattleError)
			&& Preview.bCanPlay
			&& Preview.TargetRequest.bRequiresManualSelection)
		{
			TargetedCardInstanceId = Card.InstanceId;
			break;
		}
	}
	TestFalse(TEXT("tutorial opening hand contains a playable targeted card"), TargetedCardInstanceId.IsNone());
	int32 TargetedCardSelectedEvents = 0;
	const FDelegateHandle BattleEventHandle = Registry.OnGuideEvent().AddLambda(
		[&TargetedCardSelectedEvents](const FName EventId)
		{
			if (EventId == TEXT("Event.Battle.TargetedCardSelected"))
			{
				++TargetedCardSelectedEvents;
			}
		});
	TestFalse(TEXT("invalid card click cannot emit authoritative selection event"),
		BattleBoard->ClickCardInHand(TEXT("CardInstance.Missing")));
	TestEqual(TEXT("failed card click emits no guide completion"), TargetedCardSelectedEvents, 0);
	if (!TargetedCardInstanceId.IsNone())
	{
		TestTrue(TEXT("targeted card enters legal-target selection"),
			BattleBoard->ClickCardInHand(TargetedCardInstanceId));
		TestEqual(TEXT("successful targeted selection emits once"), TargetedCardSelectedEvents, 1);
	}
	const TSharedRef<SWidget> MerchantSlate = Merchant->TakeWidget();
	const TSharedRef<SWidget> EncounterSlate = Encounter->TakeWidget();

	const TArray<FName> ExpectedTargetIds = {
		TEXT("Route.Tutorial.NextNode"),
		TEXT("Route.Settlement.Confirm"),
		TEXT("Battle.Hud.PartyQi"),
		TEXT("Battle.Hand.FirstPlayableTargetedCard"),
		TEXT("Battle.EndTurn"),
		TEXT("Route.Merchant.CardRow"),
		TEXT("Route.Merchant.RelicRow"),
		TEXT("Route.Merchant.Leave"),
		TEXT("Route.Event.ValidChoiceGroup"),
		TEXT("Route.Camp.Heal"),
		TEXT("Route.Camp.Gold"),
		TEXT("Route.Chest.Open")};
	for (const FName TargetId : ExpectedTargetIds)
	{
		TestTrue(
			FString::Printf(TEXT("owning widget registers %s"), *TargetId.ToString()),
			Registry.IsTargetRegistered(TargetId));
	}
	TestTrue(TEXT("dynamic legal-target ID remains frozen even when this headless fixture has no unit visual proxy"),
		FGameXXKGuideTargetRegistry::IsKnownTargetId(TEXT("Battle.Enemy.FirstLegalTarget")));
	Registry.OnGuideEvent().Remove(BattleEventHandle);

	int32 RouteNodeSelectedEvents = 0;
	const FDelegateHandle RouteEventHandle = Registry.OnGuideEvent().AddLambda(
		[&RouteNodeSelectedEvents](const FName EventId)
		{
			if (EventId == TEXT("Event.Route.NextNodeSelected"))
			{
				++RouteNodeSelectedEvents;
			}
		});
	TestFalse(TEXT("invalid route node cannot emit completion"), RouteMap->ExecuteRouteNodeById(-999));
	TestEqual(TEXT("failed route selection emits no event"), RouteNodeSelectedEvents, 0);
	const TArray<int32>& ReachableNodeIds = RouteSubsystem->GetRuntimeState().ReachableRouteNodeIds;
	if (!TestFalse(TEXT("challenge exposes a reachable node"), ReachableNodeIds.IsEmpty()))
	{
		Registry.OnGuideEvent().Remove(RouteEventHandle);
		return false;
	}
	TestTrue(TEXT("reachable route node executes"), RouteMap->ExecuteRouteNodeById(ReachableNodeIds[0]));
	TestEqual(TEXT("successful route mutation emits once"), RouteNodeSelectedEvents, 1);
	Registry.OnGuideEvent().Remove(RouteEventHandle);
	return true;
}

#endif
