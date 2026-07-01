#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPGameMode.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static bool HasCommand(const TArray<FGameXXKMVPCommandDescriptor>& Commands, FName CommandName, bool* bOutEnabled = nullptr)
	{
		for (const FGameXXKMVPCommandDescriptor& Command : Commands)
		{
			if (Command.CommandName == CommandName)
			{
				if (bOutEnabled)
				{
					*bOutEnabled = Command.bEnabled;
				}
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPPlayableHUDTest,
	"GameXXK.MVP.PlayableShell.HUDCommandsDriveFullLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPPlayableHUDTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	const FString TestSlotName = TEXT("GameXXK_MVP_Automation_PlayableHUD");
	HUD->SetStartGameSlotForTest(TestSlotName, 0);
	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);

	TestTrue(TEXT("main menu exposes start command"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("StartGame"))));
	TestTrue(TEXT("start command opens world map"), HUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("screen after start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);

	bool bTanjiangEnabled = true;
	TestTrue(TEXT("world map shows locked Tanjiang button"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SelectTanjiang")), &bTanjiangEnabled));
	TestFalse(TEXT("Tanjiang is disabled before Boss"), bTanjiangEnabled);
	TestFalse(TEXT("locked Tanjiang command is rejected"), HUD->HandleDemoCommand(FName(TEXT("SelectTanjiang"))));
	TestTrue(TEXT("Qingshan command enters town"), HUD->HandleDemoCommand(FName(TEXT("SelectQingshan"))));
	TestEqual(TEXT("screen after Qingshan"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	bool bEnterDungeonEnabled = true;
	TestTrue(TEXT("town shows dungeon gate before quest"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("EnterDungeon")), &bEnterDungeonEnabled));
	TestFalse(TEXT("dungeon gate disabled before quest"), bEnterDungeonEnabled);
	TestFalse(TEXT("dungeon command rejected before quest"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("quest command accepts route quest"), HUD->HandleDemoCommand(FName(TEXT("AcceptQuest"))));
	TestTrue(TEXT("follower joins after quest command"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldBeforeTrade = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("town HUD buys healing powder"), HUD->HandleDemoCommand(FName(TEXT("BuyHealingPowder"))));
	TestEqual(TEXT("buy spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("town HUD sells healing powder"), HUD->HandleDemoCommand(FName(TEXT("SellHealingPowder"))));
	TestEqual(TEXT("sell refunds gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 5);
	bool bUseHealingEnabled = true;
	const int32 HealingBeforeDisabledUse = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("town HUD shows healing command at full HP"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("UseHealingPowder")), &bUseHealingEnabled));
	TestFalse(TEXT("healing command disabled at full HP"), bUseHealingEnabled);
	TestFalse(TEXT("disabled healing command cannot execute"), HUD->HandleDemoCommand(FName(TEXT("UseHealingPowder"))));
	TestEqual(TEXT("disabled healing command keeps inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingBeforeDisabledUse);

	TestTrue(TEXT("accepted quest enters dungeon map"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestEqual(TEXT("screen after dungeon gate"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("dungeon HUD advances start node"), HUD->HandleDemoCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("dungeon HUD opens battle node"), HUD->HandleDemoCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("battle screen visible"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle HUD failure returns to town"), HUD->HandleDemoCommand(FName(TEXT("FailBattle"))));
	TestEqual(TEXT("failure command returns town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("follower remains after HUD failure"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestTrue(TEXT("retry enters dungeon after HUD failure"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("retry advances start node"), HUD->HandleDemoCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("retry opens battle node"), HUD->HandleDemoCommand(FName(TEXT("SelectBattle"))));
	TestTrue(TEXT("battle HUD resolves normal victory"), HUD->HandleDemoCommand(FName(TEXT("ResolveBattleVictory"))));
	TestEqual(TEXT("returns to dungeon after battle"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	TestTrue(TEXT("event HUD grants gold"), HUD->HandleDemoCommand(FName(TEXT("ResolveEventGold"))));
	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("camp HUD heals"), HUD->HandleDemoCommand(FName(TEXT("ResolveCampHeal"))));
	TestEqual(TEXT("camp restored HP"), Subsystem->GetRuntimeState().PlayerHP, Subsystem->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("dungeon HUD opens boss node"), HUD->HandleDemoCommand(FName(TEXT("SelectBoss"))));
	TestEqual(TEXT("boss battle visible"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle HUD resolves boss victory"), HUD->HandleDemoCommand(FName(TEXT("ResolveBattleVictory"))));
	TestEqual(TEXT("boss returns to world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);

	bTanjiangEnabled = false;
	TestTrue(TEXT("world map still shows Tanjiang button"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SelectTanjiang")), &bTanjiangEnabled));
	TestTrue(TEXT("Tanjiang enabled after Boss"), bTanjiangEnabled);
	TestTrue(TEXT("Tanjiang command enters next town"), HUD->HandleDemoCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("Tanjiang opens town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPPlayableGameModeTest,
	"GameXXK.MVP.PlayableShell.GameModeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPPlayableGameModeTest::RunTest(const FString& Parameters)
{
	AGameXXKMVPGameMode* GameMode = NewObject<AGameXXKMVPGameMode>();

	TestTrue(TEXT("default player controller is MVP controller"), GameMode->PlayerControllerClass.Get() == AGameXXKMVPPlayerController::StaticClass());
	TestTrue(TEXT("default HUD is MVP HUD"), GameMode->HUDClass.Get() == AGameXXKMVPHUD::StaticClass());
	TestTrue(TEXT("default pawn is Paper2D town pawn"), GameMode->DefaultPawnClass.Get() == AGameXXKTownPlayerPawn::StaticClass());
	TestEqual(TEXT("project global default GameMode resolves MVP class"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(TEXT("/Script/GameXXK.GameXXKMVPGameMode")));
	TestEqual(TEXT("playable shell defines town NPC spawn roles"), GameMode->GetConfiguredTownNpcCount(), 3);

	return true;
}

#endif
