#include "MVP/GameXXKTutorial01GameMode.h"

#include "Engine/GameInstance.h"
#include "GameXXKPartyFormationRules.h"
#include "GameFramework/HUD.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01SessionSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01PlayerFlowTest,
	"GameXXK.Tutorial01.PlayerFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01PlayerFlowTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("tutorial map resolves the isolated boot profile"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapForTest(
			TEXT("/Game/GameXXK/Maps/Tutorial/UEDPIE_0_L_Tutorial_0_1")),
		EGameXXKPlayerFlowBootProfile::TutorialRouteOnly);
	TestEqual(TEXT("desktop map keeps its existing profile"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapForTest(
			TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_DesktopTrainingHUD")),
		EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly);
	TestEqual(TEXT("Qingshan keeps the full player flow"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapForTest(
			TEXT("/Game/GameXXK/Maps/Prototype/UEDPIE_0_L_Qingshan_AsianVillage_Demo")),
		EGameXXKPlayerFlowBootProfile::FullPlayerFlow);
	TestEqual(TEXT("tutorial victory return suppresses the legacy town HUD flow"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapAndOptionsForTest(
			TEXT("/Game/GameXXK/Maps/Prototype/UEDPIE_0_L_Qingshan_AsianVillage_Demo"),
			TEXT("?GameXXKTutorialReturn=Victory")),
		EGameXXKPlayerFlowBootProfile::TutorialTownReturnOnly);
	TestEqual(TEXT("tutorial defeat return also suppresses the legacy town HUD flow"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapAndOptionsForTest(
			TEXT("/Game/GameXXK/Maps/Prototype/UEDPIE_0_L_Qingshan_AsianVillage_Demo"),
			TEXT("?GameXXKTutorialReturn=Defeat")),
		EGameXXKPlayerFlowBootProfile::TutorialTownReturnOnly);
	TestEqual(TEXT("unrelated Qingshan options retain the full player flow"),
		AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfileForMapAndOptionsForTest(
			TEXT("/Game/GameXXK/Maps/Prototype/UEDPIE_0_L_Qingshan_AsianVillage_Demo"),
			TEXT("?GameXXKIntro=CarriagePreview")),
		EGameXXKPlayerFlowBootProfile::FullPlayerFlow);

	const AGameXXKTutorial01GameMode* Defaults =
		GetDefault<AGameXXKTutorial01GameMode>();
	if (!TestNotNull(TEXT("tutorial game mode CDO exists"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("tutorial game mode uses the MVP controller"),
		Defaults->PlayerControllerClass.Get(),
		AGameXXKMVPPlayerController::StaticClass());
	TestNull(TEXT("tutorial map never spawns a pawn"), Defaults->DefaultPawnClass);
	TestEqual(TEXT("tutorial map needs only the base HUD"),
		Defaults->HUDClass.Get(),
		AHUD::StaticClass());

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Runtime =
		NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKTutorial01SessionSubsystem* Session =
		NewObject<UGameXXKTutorial01SessionSubsystem>(TestGameInstance);
	if (!TestTrue(TEXT("tutorial fixture starts the real Qingshan runtime"),
		Runtime && Runtime->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}
	FGameXXKRuntimeState Before = Runtime->GetRuntimeStateCopy();
	Before.Screen = EGameXXKScreen::Town;
	Runtime->GetMutableRuntimeState() = Before;
	TestTrue(TEXT("tutorial fixture owns a transient new-player session"),
		Session->BeginFromTown(
			Before,
			FTransform::Identity,
			EGameXXKGuidePreference::NewPlayer));

	AGameXXKMVPPlayerController* Controller =
		NewObject<AGameXXKMVPPlayerController>();
	TestFalse(TEXT("tutorial boot rejects a missing travel option"),
		Controller->PrepareTutorial01RouteForTest(
			Runtime,
			Session,
			TEXT("")));
	TestTrue(TEXT("tutorial boot prepares only the fixed route"),
		Controller->PrepareTutorial01RouteForTest(
			Runtime,
			Session,
			TEXT("?GameXXKTutorial=0-1")));
	TestEqual(TEXT("tutorial first opens the route screen"),
		Runtime->GetRuntimeState().Screen,
		EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("route boot does not start a card battle"),
		Runtime->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("route boot does not start a legacy battle"),
		Runtime->GetRuntimeState().bHasActiveBattle);

	EGameXXKTutorial01RouteAction Action = EGameXXKTutorial01RouteAction::None;
	TestTrue(TEXT("battle node becomes selected"),
		Session->RequestRouteNode(
			FGameXXKTutorial01RouteRules::BattleNodeId,
			Action));
	TestEqual(TEXT("battle node reports start action"),
		Action,
		EGameXXKTutorial01RouteAction::StartBattle);
	TestTrue(TEXT("selected battle starts the isolated encounter"),
		Controller->StartTutorial01BattleRuntimeForTest(Runtime, Session));
	const FGameXXKRuntimeState& BattleState = Runtime->GetRuntimeState();
	TestEqual(TEXT("battle node opens the existing battle screen"),
		BattleState.Screen,
		EGameXXKScreen::Battle);
	TestTrue(TEXT("tutorial starts exactly one card battle"),
		BattleState.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("tutorial projects one enemy"),
		BattleState.ActiveBattleEnemies.Num(),
		1);
	if (BattleState.ActiveBattleEnemies.Num() == 1)
	{
		TestEqual(TEXT("tutorial enemy is the 0-1 rooster"),
			BattleState.ActiveBattleEnemies[0].EnemyDefinitionId,
			FName(TEXT("Enemy.Ch1.Rooster")));
	}
	FName TutorialNpc;
	TestTrue(TEXT("tutorial battle formation resolves an NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			BattleState,
			TutorialNpc));
	TestEqual(TEXT("tutorial battle replaces Tusi with YueBai"),
		TutorialNpc,
		FName(TEXT("Npc.YueBai")));
	const FGameXXKBattleDeckState& Deck = BattleState.CardRun.ActiveBattle.Deck;
	TestTrue(TEXT("tutorial battle has the fixed three-card prefix"),
		Deck.Hand.Num() >= 3);
	if (Deck.Hand.Num() >= 3)
	{
		TestEqual(TEXT("leftmost tutorial card"),
			Deck.Hand[0].CardId,
			FName(TEXT("Hero.Generic.HengJianShouShi")));
		TestEqual(TEXT("second tutorial card"),
			Deck.Hand[1].CardId,
			FName(TEXT("Hero.Generic.SuiYanJi")));
		TestEqual(TEXT("third tutorial card"),
			Deck.Hand[2].CardId,
			FName(TEXT("Hero.Generic.FengShenBu")));
	}

	return true;
}

#endif
