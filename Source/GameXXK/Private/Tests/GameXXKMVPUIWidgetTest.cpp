#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleWidget.h"
#include "UI/GameXXKCharacterPanelWidget.h"
#include "UI/GameXXKDungeonMapWidget.h"
#include "UI/GameXXKInventoryWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKQuestDialogWidget.h"
#include "UI/GameXXKTradeWidget.h"
#include "UI/GameXXKWorldMapWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPUIWidgetTest,
	"GameXXK.MVP.UI.WidgetBasesDriveRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPUIWidgetTest::RunTest(const FString& Parameters)
{
	const FString UiTestSlot = TEXT("GameXXK_MVP_Automation_UIWidget_Start");
	const int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(UiTestSlot, UserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMainMenuWidget* MainMenu = NewObject<UGameXXKMainMenuWidget>();
	UGameXXKWorldMapWidget* WorldMap = NewObject<UGameXXKWorldMapWidget>();
	UGameXXKQuestDialogWidget* QuestDialog = NewObject<UGameXXKQuestDialogWidget>();
	UGameXXKTradeWidget* Trade = NewObject<UGameXXKTradeWidget>();
	UGameXXKInventoryWidget* Inventory = NewObject<UGameXXKInventoryWidget>();
	UGameXXKDungeonMapWidget* DungeonMap = NewObject<UGameXXKDungeonMapWidget>();
	UGameXXKBattleWidget* Battle = NewObject<UGameXXKBattleWidget>();
	UGameXXKCharacterPanelWidget* CharacterPanel = NewObject<UGameXXKCharacterPanelWidget>();

	MainMenu->SetMVPSubsystem(Subsystem);
	WorldMap->SetMVPSubsystem(Subsystem);
	QuestDialog->SetMVPSubsystem(Subsystem);
	Trade->SetMVPSubsystem(Subsystem);
	Inventory->SetMVPSubsystem(Subsystem);
	DungeonMap->SetMVPSubsystem(Subsystem);
	Battle->SetMVPSubsystem(Subsystem);
	CharacterPanel->SetMVPSubsystem(Subsystem);

	TestFalse(TEXT("main menu continue rejects missing slot"), MainMenu->ContinueGameFromSlot(UiTestSlot, UserIndex));
	TestTrue(TEXT("main menu start creates new game and opens Qingshan town"), MainMenu->StartGame());
	TestEqual(TEXT("town screen after player-facing main menu start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("Qingshan selected after player-facing main menu start"), Subsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());
	TestFalse(TEXT("main menu start does not autosave"), MainMenu->DoesSaveGameExist(UiTestSlot, UserIndex));
	TestFalse(TEXT("locked Tanjiang click is rejected"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionTanjiang()));
	TestFalse(TEXT("last selection records locked state"), WorldMap->WasLastSelectionUnlocked());
	TestEqual(TEXT("locked selection is remembered"), WorldMap->GetLastSelectedRegion(), UGameXXKMVPRules::RegionTanjiang());
	TestTrue(TEXT("Qingshan click enters town"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("last selection records unlocked state"), WorldMap->WasLastSelectionUnlocked());
	TestEqual(TEXT("town screen after selecting Qingshan"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	TestTrue(TEXT("quest dialog accepts quest"), QuestDialog->AcceptQuest());
	TestEqual(TEXT("quest accepted in subsystem state"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("follower joins after quest dialog"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldBeforeBuy = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("trade widget buys healing powder"), Trade->BuyItemById(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("buy spends gold through subsystem"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBuy - 10);
	TestEqual(TEXT("inventory widget reads bought stack"), Inventory->GetItemCount(UGameXXKMVPRules::ItemHealingPowder()), 2);
	TestTrue(TEXT("trade widget sells healing powder"), Trade->SellItemById(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("sell awards gold through subsystem"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBuy - 5);

	TestTrue(TEXT("trade widget buys weapon for equipment UI"), Trade->BuyItemById(UGameXXKMVPRules::ItemIronSword(), 1));
	const int32 AttackBeforeEquip = Subsystem->GetRuntimeState().PlayerAttack;
	TestTrue(TEXT("inventory widget equips weapon"), Inventory->EquipItemById(UGameXXKMVPRules::ItemIronSword()));
	TestEqual(TEXT("equipment changes subsystem attack"), Subsystem->GetRuntimeState().PlayerAttack, AttackBeforeEquip + 6);
	TestTrue(TEXT("character panel refreshes from subsystem"), CharacterPanel->RefreshPlayerSummary());
	TestEqual(TEXT("character panel reads equipped attack"), CharacterPanel->GetCharacterSummary().Attack, Subsystem->GetRuntimeState().PlayerAttack);

	TestTrue(TEXT("dungeon map opens from accepted quest"), DungeonMap->OpenFromTownExit());
	TestEqual(TEXT("dungeon map screen after town exit"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("dungeon widget advances start node"), DungeonMap->SelectNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("dungeon widget opens battle node"), DungeonMap->SelectNode(EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle screen after battle node"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	const TArray<FName> TurnOrder = Battle->BuildTurnOrder(false);
	TestEqual(TEXT("battle widget exposes turn order"), TurnOrder[0], FName(TEXT("Wolf")));
	TestTrue(TEXT("battle widget can fail back to town"), Battle->FailToTown());
	TestEqual(TEXT("failure through battle widget returns to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("retry dungeon after battle widget failure"), DungeonMap->OpenFromTownExit());
	TestTrue(TEXT("retry start node after failure"), DungeonMap->SelectNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("retry battle node after failure"), DungeonMap->SelectNode(EGameXXKNodeKind::Battle));

	const int32 GoldBeforeBattle = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("battle widget resolves victory"), Battle->ResolveVictory(false));
	TestTrue(TEXT("battle victory grants gold"), Subsystem->GetRuntimeState().PlayerGold > GoldBeforeBattle);

	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("inventory widget uses healing item"), Inventory->UseHealingItem());
	TestTrue(TEXT("healing item raises HP"), Subsystem->GetRuntimeState().PlayerHP > 1);

	UGameplayStatics::DeleteGameInSlot(UiTestSlot, UserIndex);
	return true;
}

#endif
