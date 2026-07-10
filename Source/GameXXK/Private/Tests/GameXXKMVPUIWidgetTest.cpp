#include "GameXXKMVPRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Widget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleWidget.h"
#include "UI/GameXXKCharacterPanelWidget.h"
#include "UI/GameXXKDungeonMapWidget.h"
#include "UI/GameXXKInventoryWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKQuestDialogWidget.h"
#include "UI/GameXXKTradeWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"
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
	UGameXXKInventoryWindowWidget* InventoryWindow = NewObject<UGameXXKInventoryWindowWidget>();
	UGameXXKTownOverlayWidget* TownOverlay = NewObject<UGameXXKTownOverlayWidget>();
	UGameXXKDungeonMapWidget* DungeonMap = NewObject<UGameXXKDungeonMapWidget>();
	UGameXXKBattleWidget* Battle = NewObject<UGameXXKBattleWidget>();
	UGameXXKCharacterPanelWidget* CharacterPanel = NewObject<UGameXXKCharacterPanelWidget>();

	MainMenu->SetMVPSubsystem(Subsystem);
	WorldMap->SetMVPSubsystem(Subsystem);
	QuestDialog->SetMVPSubsystem(Subsystem);
	Trade->SetMVPSubsystem(Subsystem);
	Inventory->SetMVPSubsystem(Subsystem);
	InventoryWindow->SetMVPSubsystem(Subsystem);
	TownOverlay->SetMVPSubsystem(Subsystem);
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
	Subsystem->GetMutableRuntimeState().PlayerGold = 500;
	TownOverlay->RefreshFromState();
	TestTrue(TEXT("town overlay exposes inventory panel command"), TownOverlay->HasTownActionForTest(FName(TEXT("OpenInventory")), true));
	TestTrue(TEXT("town overlay exposes character equipment panel command"), TownOverlay->HasTownActionForTest(FName(TEXT("OpenCharacterPanel")), true));
	TestTrue(TEXT("town overlay opens inventory panel"), TownOverlay->ExecuteTownCommandForTest(FName(TEXT("OpenInventory"))));
	TestEqual(TEXT("town overlay records inventory panel"), TownOverlay->GetActiveTownPanelForTest(), EGameXXKTownPanelMode::Inventory);
	TestEqual(TEXT("inventory panel exposes 20 backpack slots"), TownOverlay->GetInventorySlotCountForTest(), 20);
	TestTrue(TEXT("inventory slots use generated ink slot texture"), TownOverlay->GetInventorySlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot")));
	TestNotNull(TEXT("inventory panel has one shared backpack backplate"), TownOverlay->WidgetTree ? Cast<UBorder>(TownOverlay->WidgetTree->FindWidget(TEXT("TownInventoryBackplate"))) : nullptr);
	USizeBox* FirstInventorySlot = TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownInventorySlotSize_00"))) : nullptr;
	USizeBox* LastInventorySlot = TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownInventorySlotSize_19"))) : nullptr;
	TestNotNull(TEXT("inventory panel wraps the first slot in a fixed size box"), FirstInventorySlot);
	TestNotNull(TEXT("inventory panel wraps the last slot in a fixed size box"), LastInventorySlot);
	if (FirstInventorySlot && LastInventorySlot)
	{
		TestEqual(TEXT("inventory slots share fixed width"), FirstInventorySlot->GetWidthOverride(), LastInventorySlot->GetWidthOverride());
		TestEqual(TEXT("inventory slots share fixed height"), FirstInventorySlot->GetHeightOverride(), LastInventorySlot->GetHeightOverride());
		TestEqual(TEXT("inventory slot width matches MVP layout"), FirstInventorySlot->GetWidthOverride(), 72.0f);
		TestEqual(TEXT("inventory slot height matches MVP layout"), FirstInventorySlot->GetHeightOverride(), 72.0f);
	}
	TestNotNull(TEXT("character equipment panel exposes weapon slot"), TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownEquipmentSlot_Weapon"))) : nullptr);
	TestNotNull(TEXT("character equipment panel exposes armor slot"), TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownEquipmentSlot_Armor"))) : nullptr);
	TestNotNull(TEXT("character equipment panel exposes accessory slot"), TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownEquipmentSlot_Accessory"))) : nullptr);
	TestTrue(TEXT("town overlay opens trade panel"), TownOverlay->ExecuteTownCommandForTest(FName(TEXT("OpenTrade"))));
	TestEqual(TEXT("town overlay records trade panel"), TownOverlay->GetActiveTownPanelForTest(), EGameXXKTownPanelMode::Trade);
	UWidget* SharedBackpackGrid = TownOverlay->WidgetTree ? TownOverlay->WidgetTree->FindWidget(TEXT("TownSharedInventoryGrid")) : nullptr;
	TestNotNull(TEXT("trade panel reuses the shared player backpack grid"), SharedBackpackGrid);
	TestTrue(TEXT("shared backpack grid stays visible while trading"), SharedBackpackGrid && SharedBackpackGrid->GetVisibility() != ESlateVisibility::Collapsed);
	TestNotNull(TEXT("trade panel has a shop stock panel beside the same backpack"), TownOverlay->WidgetTree ? TownOverlay->WidgetTree->FindWidget(TEXT("TownShopStockPanel")) : nullptr);
	TestEqual(TEXT("trade panel renders every merchant stock item as a slot"), TownOverlay->GetShopStockSlotCountForTest(), UGameXXKMVPRules::GetShopItemIds().Num());
	TestTrue(TEXT("shop stock slots use the shared ink slot texture"), TownOverlay->GetShopStockSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot")));
	USizeBox* FirstShopSlot = TownOverlay->WidgetTree ? Cast<USizeBox>(TownOverlay->WidgetTree->FindWidget(TEXT("TownShopStockSlotSize_00"))) : nullptr;
	TestNotNull(TEXT("trade panel wraps the first shop stock item in a fixed size slot"), FirstShopSlot);
	if (FirstShopSlot)
	{
		TestEqual(TEXT("shop stock slot width matches backpack slot width"), FirstShopSlot->GetWidthOverride(), 72.0f);
		TestEqual(TEXT("shop stock slot height matches backpack slot height"), FirstShopSlot->GetHeightOverride(), 72.0f);
	}
	TestEqual(TEXT("first shop slot is driven by the shop stock view model"), TownOverlay->GetShopStockSlotItemIdForTest(0), UGameXXKMVPRules::GetShopItemIds()[0]);
	TestEqual(TEXT("first backpack slot is driven by the player inventory view model"), TownOverlay->GetPlayerBackpackSlotItemIdForTest(0), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("healing powder has a loaded item icon path"), TownOverlay->GetItemIconResourcePathForTest(UGameXXKMVPRules::ItemHealingPowder()).Contains(TEXT("T_ItemHealingPowder")));
	TestTrue(TEXT("Qingxin tea has a loaded item icon path"), TownOverlay->GetItemIconResourcePathForTest(FName(TEXT("Item.QingxinTea"))).Contains(TEXT("T_ItemQingxinTea")));
	TestTrue(TEXT("starter wooden sword has a loaded item icon path"), TownOverlay->GetItemIconResourcePathForTest(UGameXXKMVPRules::ItemWoodenSword()).Contains(TEXT("T_ItemWoodenSword")));
	const int32 GoldBeforeSlotPurchase = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeSlotPurchase = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("clicking shop stock slot selects it for purchase"), TownOverlay->SelectShopStockSlotForTest(0));
	TestTrue(TEXT("shop slot selection opens purchase confirmation"), TownOverlay->IsPurchaseConfirmationVisibleForTest());
	TestEqual(TEXT("purchase confirmation records pending shop item"), TownOverlay->GetPendingPurchaseItemIdForTest(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("confirming pending shop purchase buys one item"), TownOverlay->ConfirmPendingPurchaseForTest());
	TestEqual(TEXT("slot purchase spends item buy price"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeSlotPurchase - 10);
	TestEqual(TEXT("slot purchase refreshes player backpack inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeSlotPurchase + 1);

	TestTrue(TEXT("independent free inventory window opens"), InventoryWindow->OpenFreeInventoryForTest());
	TestEqual(TEXT("free inventory window records free mode"), InventoryWindow->GetWindowModeForTest(), EGameXXKInventoryWindowMode::FreeInventory);
	TestTrue(TEXT("free inventory window has one coherent frame"), InventoryWindow->HasWindowFrameForTest());
	TestTrue(TEXT("free inventory window frame uses generated ink frame texture"), InventoryWindow->GetWindowFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InventoryWindowFrame")));
	TestTrue(TEXT("free inventory window has its own top-right close button"), InventoryWindow->HasCloseButtonForTest());
	TestTrue(TEXT("free inventory close button uses generated close texture"), InventoryWindow->GetCloseButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InventoryCloseButton")));
	TestEqual(TEXT("free inventory window owns twenty fixed backpack slots"), InventoryWindow->GetBackpackSlotCountForTest(), 20);
	TestTrue(TEXT("free inventory backpack slots use generated ink slot texture"), InventoryWindow->GetBackpackSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot")));
	TestEqual(TEXT("free inventory window owns weapon armor accessory slots"), InventoryWindow->GetEquipmentSlotCountForTest(), 3);
	TestTrue(TEXT("free inventory equipment slots use generated slot texture"), InventoryWindow->GetEquipmentSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InventoryEquipmentSlots")));
	TestTrue(TEXT("first backpack slot loads its item icon"), InventoryWindow->GetBackpackSlotIconResourcePathForTest(0).Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemHealingPowder")));
	TestFalse(TEXT("free inventory window does not lock movement input"), InventoryWindow->IsModalInputLockActiveForTest());
	TestTrue(TEXT("independent merchant trade window opens"), InventoryWindow->OpenMerchantTradeForTest());
	TestEqual(TEXT("merchant inventory window records trade mode"), InventoryWindow->GetWindowModeForTest(), EGameXXKInventoryWindowMode::MerchantTrade);
	TestTrue(TEXT("merchant trade window has one coherent frame"), InventoryWindow->HasWindowFrameForTest());
	TestTrue(TEXT("merchant trade window has its own top-right close button"), InventoryWindow->HasCloseButtonForTest());
	TestEqual(TEXT("merchant trade window shows every shop stock item as a slot"), InventoryWindow->GetMerchantStockSlotCountForTest(), UGameXXKMVPRules::GetShopItemIds().Num());
	TestTrue(TEXT("merchant stock slots share the generated ink slot texture"), InventoryWindow->GetMerchantStockSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot")));
	TestTrue(TEXT("merchant trade window locks movement input"), InventoryWindow->IsModalInputLockActiveForTest());
	TestTrue(TEXT("merchant stock selection picks first shop item"), InventoryWindow->SelectMerchantStockSlotForTest(0));
	TestEqual(TEXT("merchant stock selection exposes buy action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().ToString(), FString(TEXT("购买")));
	const int32 GoldBeforeDialogCancel = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeDialogCancel = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("buy action opens independent confirmation dialog"), InventoryWindow->RequestSelectedBuyForTest());
	TestTrue(TEXT("confirmation dialog is visible"), InventoryWindow->IsConfirmationDialogVisibleForTest());
	TestTrue(TEXT("confirmation dialog has confirm button"), InventoryWindow->HasConfirmationConfirmButtonForTest());
	TestTrue(TEXT("confirmation dialog has cancel button"), InventoryWindow->HasConfirmationCancelButtonForTest());
	TestTrue(TEXT("canceling confirmation dialog succeeds"), InventoryWindow->CancelDialogForTest());
	TestEqual(TEXT("canceling buy dialog leaves gold unchanged"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeDialogCancel);
	TestEqual(TEXT("canceling buy dialog leaves quantity unchanged"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeDialogCancel);
	TestTrue(TEXT("buy action can reopen independent confirmation dialog"), InventoryWindow->RequestSelectedBuyForTest());
	TestTrue(TEXT("confirming buy dialog succeeds"), InventoryWindow->ConfirmDialogForTest());
	TestEqual(TEXT("confirming buy dialog spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeDialogCancel - 10);
	TestEqual(TEXT("confirming buy dialog adds backpack item"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeDialogCancel + 1);
	TestTrue(TEXT("inventory window opens free mode after buying equipment"), InventoryWindow->OpenFreeInventoryForTest());
	TestTrue(TEXT("trade widget buys armor for independent inventory equipment UI"), Trade->BuyItemById(UGameXXKMVPRules::ItemClothArmor(), 1));
	const int32 DefenseBeforeIndependentEquip = Subsystem->GetRuntimeState().PlayerDefense;
	TestTrue(TEXT("independent inventory selects bought armor"), InventoryWindow->SelectPlayerBackpackItemForTest(UGameXXKMVPRules::ItemClothArmor()));
	TestEqual(TEXT("selected bought equipment exposes equip action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().ToString(), FString(TEXT("装备")));
	TestTrue(TEXT("independent inventory equips selected armor through detail action"), InventoryWindow->ExecuteSelectedPrimaryActionForTest());
	TestEqual(TEXT("independent inventory places armor in armor slot"), InventoryWindow->GetEquippedItemForSlotForTest(FName(TEXT("Armor"))), UGameXXKMVPRules::ItemClothArmor());
	TestEqual(TEXT("independent inventory equipment action updates defense"), Subsystem->GetRuntimeState().PlayerDefense, DefenseBeforeIndependentEquip + 6);
	InventoryWindow->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::Equipment, INDEX_NONE, FName(TEXT("Armor")));
	TestEqual(TEXT("equipped armor slot exposes unequip action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().ToString(), FString(TEXT("卸下")));
	TestTrue(TEXT("independent inventory unequips selected armor through detail action"), InventoryWindow->ExecuteSelectedPrimaryActionForTest());
	TestTrue(TEXT("independent inventory clears armor slot after unequip"), InventoryWindow->GetEquippedItemForSlotForTest(FName(TEXT("Armor"))).IsNone());
	TestEqual(TEXT("unequipping selected armor restores defense"), Subsystem->GetRuntimeState().PlayerDefense, DefenseBeforeIndependentEquip);

	TestTrue(TEXT("quest dialog accepts quest"), QuestDialog->AcceptQuest());
	TestEqual(TEXT("quest accepted in subsystem state"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("follower joins after quest dialog"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldBeforeBuy = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeTradeWidgetBuy = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("trade widget buys healing powder"), Trade->BuyItemById(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("buy spends gold through subsystem"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBuy - 10);
	TestEqual(TEXT("inventory widget reads bought stack"), Inventory->GetItemCount(UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeTradeWidgetBuy + 1);
	TestTrue(TEXT("inventory widget lists bought healing powder"), Inventory->GetInventoryItemIds().Contains(UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("trade widget sells healing powder"), Trade->SellItemById(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("sell awards gold through subsystem"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBuy - 5);
	TestTrue(TEXT("trade widget exposes PPT shop stock"), Trade->GetShopItemIds().Contains(FName(TEXT("Item.QingxinTea"))));
	TestTrue(TEXT("trade widget sells PPT Qingfeng sword"), Trade->GetShopItemIds().Contains(UGameXXKMVPRules::ItemIronSword()));

	TestTrue(TEXT("trade widget buys weapon for equipment UI"), Trade->BuyItemById(UGameXXKMVPRules::ItemIronSword(), 1));
	const int32 AttackBeforeEquip = Subsystem->GetRuntimeState().PlayerAttack;
	TestTrue(TEXT("inventory widget equips weapon"), Inventory->EquipItemById(UGameXXKMVPRules::ItemIronSword()));
	TestEqual(TEXT("equipment changes subsystem attack"), Subsystem->GetRuntimeState().PlayerAttack, AttackBeforeEquip + 8);
	TestTrue(TEXT("inventory widget buys and equips PPT HP accessory"), Trade->BuyItemById(FName(TEXT("Item.CranePatternTalisman")), 1));
	const int32 MaxHPBeforeAccessory = Subsystem->GetRuntimeState().PlayerMaxHP;
	TestTrue(TEXT("inventory widget equips accessory"), Inventory->EquipItemById(FName(TEXT("Item.CranePatternTalisman"))));
	TestEqual(TEXT("accessory changes subsystem max HP"), Subsystem->GetRuntimeState().PlayerMaxHP, MaxHPBeforeAccessory + 30);
	TestTrue(TEXT("inventory widget uses arbitrary consumable item"), Trade->BuyItemById(FName(TEXT("Item.QingxinTea")), 1));
	Subsystem->GetMutableRuntimeState().PlayerMP = 0;
	TestTrue(TEXT("inventory widget uses Qingxin tea"), Inventory->UseItemById(FName(TEXT("Item.QingxinTea"))));
	TestEqual(TEXT("Qingxin tea raises MP through inventory widget"), Subsystem->GetRuntimeState().PlayerMP, 20);
	TestTrue(TEXT("character panel refreshes from subsystem"), CharacterPanel->RefreshPlayerSummary());
	TestEqual(TEXT("character panel reads equipped attack"), CharacterPanel->GetCharacterSummary().Attack, Subsystem->GetRuntimeState().PlayerAttack);
	TestEqual(TEXT("character panel reads equipped weapon"), CharacterPanel->GetCharacterSummary().EquippedWeapon, UGameXXKMVPRules::ItemIronSword());
	TestEqual(TEXT("character panel reads equipped accessory"), CharacterPanel->GetCharacterSummary().EquippedAccessory, FName(TEXT("Item.CranePatternTalisman")));

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
