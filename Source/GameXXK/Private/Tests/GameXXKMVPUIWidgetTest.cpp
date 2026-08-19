#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
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
#include "UI/GameXXKTownHudWidget.h"
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
	UGameXXKTownHudWidget* TownHud = NewObject<UGameXXKTownHudWidget>();
	UGameXXKTownOverlayWidget* TownOverlay = NewObject<UGameXXKTownOverlayWidget>();
	UGameXXKDungeonMapWidget* DungeonMap = NewObject<UGameXXKDungeonMapWidget>();
	UGameXXKBattleWidget* Battle = NewObject<UGameXXKBattleWidget>();
	UGameXXKCharacterPanelWidget* CharacterPanel = NewObject<UGameXXKCharacterPanelWidget>();

	MainMenu->SetMVPSubsystem(Subsystem);
	WorldMap->SetMVPSubsystem(Subsystem);
	QuestDialog->SetMVPSubsystem(Subsystem);
	QuestDialog->TakeWidget();
	TestNotNull(TEXT("quest dialog builds a full-screen ink backdrop"), QuestDialog->WidgetTree ? QuestDialog->WidgetTree->FindWidget(TEXT("QuestDialogBackdrop")) : nullptr);
	UBorder* QuestDialogFrame = QuestDialog->WidgetTree ? Cast<UBorder>(QuestDialog->WidgetTree->FindWidget(TEXT("QuestDialogFrame"))) : nullptr;
	UButton* QuestDialogAcceptButton = QuestDialog->WidgetTree ? Cast<UButton>(QuestDialog->WidgetTree->FindWidget(TEXT("QuestDialogAcceptButton"))) : nullptr;
	UButton* QuestDialogLeaveButton = QuestDialog->WidgetTree ? Cast<UButton>(QuestDialog->WidgetTree->FindWidget(TEXT("QuestDialogLeaveButton"))) : nullptr;
	TestNotNull(TEXT("quest dialog builds a centered paper frame"), QuestDialogFrame);
	TestNotNull(TEXT("quest dialog exposes the accept button"), QuestDialogAcceptButton);
	TestNotNull(TEXT("quest dialog exposes the leave button"), QuestDialogLeaveButton);
	TestNotNull(TEXT("quest dialog frame resolves the generated parchment texture"), QuestDialogFrame ? QuestDialogFrame->Background.GetResourceObject() : nullptr);
	TestNotNull(TEXT("quest dialog accept button resolves the generated jade texture"), QuestDialogAcceptButton ? QuestDialogAcceptButton->GetStyle().Normal.GetResourceObject() : nullptr);
	TestNotNull(TEXT("quest dialog leave button resolves the generated ochre texture"), QuestDialogLeaveButton ? QuestDialogLeaveButton->GetStyle().Normal.GetResourceObject() : nullptr);
	TestTrue(TEXT("quest dialog frame is sourced from the generated parchment texture"), QuestDialog->GetDialogFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogFrame")));
	TestTrue(TEXT("quest dialog accept action is sourced from the generated jade texture"), QuestDialog->GetAcceptButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogAccept")));
	TestTrue(TEXT("quest dialog leave action is sourced from the generated ochre texture"), QuestDialog->GetLeaveButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogLeave")));
	TestTrue(TEXT("quest dialog can open after its visual layout is built"), !QuestDialog->IsDialogOpen());
	QuestDialog->OpenDialog();
	TestTrue(TEXT("quest dialog opens the ink paper modal"), QuestDialog->IsDialogOpen());
	TestTrue(TEXT("quest dialog can close after the preview"), QuestDialog->CloseDialog());
	Trade->SetMVPSubsystem(Subsystem);
	Inventory->SetMVPSubsystem(Subsystem);
	InventoryWindow->SetMVPSubsystem(Subsystem);
	TownHud->SetMVPSubsystem(Subsystem);
	TownHud->TakeWidget();
	TownOverlay->SetMVPSubsystem(Subsystem);
	DungeonMap->SetMVPSubsystem(Subsystem);
	Battle->SetMVPSubsystem(Subsystem);
	CharacterPanel->SetMVPSubsystem(Subsystem);

	TestFalse(TEXT("main menu continue rejects missing slot"), MainMenu->ContinueGameFromSlot(UiTestSlot, UserIndex));
	TestTrue(TEXT("main menu start creates a new game"), MainMenu->StartGame());
	TestEqual(TEXT("main menu start lands directly in Qingshan town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	const FGameXXKCompanionRosterState& StarterRoster = Subsystem->GetRuntimeState().CardRun.CompanionRoster;
	TestEqual(TEXT("main menu StartNewGame grants all six profession companions"), StarterRoster.PermanentCompanions.Num(), 6);
	FName StarterCompanionId = NAME_None;
	if (StarterRoster.PermanentCompanions.Num() == 6)
	{
		const FGameXXKPermanentCompanion& StarterCompanion = StarterRoster.PermanentCompanions[0];
		StarterCompanionId = StarterCompanion.InstanceId;
		TestFalse(TEXT("main menu starter companion has a stable instance id"), StarterCompanionId.IsNone());
		TestTrue(TEXT("main menu StartNewGame activates the starter companion"), StarterCompanion.bIsActive);
		TestEqual(TEXT("main menu StartNewGame synchronizes the active companion selection"),
			Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
			StarterCompanionId);
	}
	if (StarterCompanionId.IsNone())
	{
		return false;
	}
	TestFalse(TEXT("main menu start does not autosave"), MainMenu->DoesSaveGameExist(UiTestSlot, UserIndex));
	TestTrue(TEXT("town explicitly opens the world map for map-widget coverage"), Subsystem->OpenWorldMap());
	WorldMap->RefreshFromState();
	WorldMap->TakeWidget();
	TestTrue(TEXT("Qingshan is initially selectable"), WorldMap->IsRegionEnabledForTest(UGameXXKMVPRules::RegionQingshan()));
	TestFalse(TEXT("Tanjiang is not a playable town target yet"), WorldMap->IsRegionEnabledForTest(UGameXXKMVPRules::RegionTanjiang()));
	TestNotNull(TEXT("world map builds terrain image"), WorldMap->GetWidgetFromName(TEXT("WorldMapTerrain")));
	TestNotNull(TEXT("world map builds Qingshan button"), WorldMap->GetWidgetFromName(TEXT("WorldMapRegionQingshan")));
	TestNotNull(TEXT("world map builds selection notice"), WorldMap->GetWidgetFromName(TEXT("WorldMapSelectionNotice")));
	TestTrue(TEXT("world map terrain is GameXXK owned"), WorldMap->GetTerrainResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_Terrain")));
	TestFalse(TEXT("world map never resolves 1Game background"), WorldMap->GetTerrainResourcePathForTest().Contains(TEXT("/Game/1Game/")));
	TestFalse(TEXT("locked or unavailable town selection is rejected"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionTanjiang()));
	TestFalse(TEXT("last selection records locked state"), WorldMap->WasLastSelectionUnlocked());
	TestEqual(TEXT("locked selection is remembered"), WorldMap->GetLastSelectedRegion(), UGameXXKMVPRules::RegionTanjiang());
	TestFalse(TEXT("unavailable town selection explains why"), WorldMap->GetSelectionNoticeForTest().IsEmpty());
	Subsystem->GetMutableRuntimeState().UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	WorldMap->RefreshFromState();
	TestFalse(TEXT("unimplemented Tanjiang stays disabled after its progression unlock"), WorldMap->IsRegionEnabledForTest(UGameXXKMVPRules::RegionTanjiang()));
	TestFalse(TEXT("unimplemented Tanjiang selection remains rejected after its progression unlock"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionTanjiang()));
	TestFalse(TEXT("unimplemented Tanjiang selection remains non-playable after its progression unlock"), WorldMap->WasLastSelectionUnlocked());
	TestEqual(TEXT("unimplemented Tanjiang selection keeps the map on world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("Qingshan click enters town"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("last selection records unlocked state"), WorldMap->WasLastSelectionUnlocked());
	TestEqual(TEXT("town screen after selecting Qingshan"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	UButton* TownMapButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudMap"))) : nullptr;
	TestNotNull(TEXT("town HUD exposes the world-map navigation button"), TownMapButton);
	if (TownMapButton)
	{
		TownMapButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("town HUD map button returns to world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("Qingshan can be re-entered from the returned world map"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
	TestEqual(TEXT("re-entering Qingshan restores town screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
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
	TestFalse(TEXT("town overlay no longer exposes the retired trade command"), TownOverlay->HasTownActionForTest(FName(TEXT("OpenTrade")), true));

	TestTrue(TEXT("independent free inventory window opens"), InventoryWindow->OpenFreeInventoryForTest());
	TestEqual(TEXT("free inventory window records free mode"), InventoryWindow->GetWindowModeForTest(), EGameXXKInventoryWindowMode::FreeInventory);
	TestTrue(TEXT("free inventory window has one coherent frame"), InventoryWindow->HasWindowFrameForTest());
	TestTrue(TEXT("free inventory window frame uses the approved MasterV2 large panel"), InventoryWindow->GetWindowFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge")));
	TestTrue(TEXT("free inventory window has its own top-right close button"), InventoryWindow->HasCloseButtonForTest());
	TestTrue(TEXT("free inventory close button uses the approved shared ink X"), InventoryWindow->GetCloseButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk")));
	TestEqual(TEXT("free inventory window exposes a twenty-cell backpack viewport"), InventoryWindow->GetBackpackSlotCountForTest(), 20);
	TestTrue(TEXT("free inventory backpack slots use the approved MasterV2 item slot"), InventoryWindow->GetBackpackSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot")));
	TestEqual(TEXT("free inventory window owns all six authoritative equipment slots"), InventoryWindow->GetEquipmentSlotCountForTest(), 6);
	TestTrue(TEXT("free inventory equipment slots use the approved MasterV2 equipment slot"), InventoryWindow->GetEquipmentSlotResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_EquipmentSlot")));
	// A new game grants the six-piece Starter gear set (no legacy healing powder).
	const TArray<FName> StarterWarehouseEntries = InventoryWindow->GetVisibleBackpackEquipmentInstanceIdsForTest();
	TestTrue(TEXT("starter gear remains visible after instance equipment is merged into the backpack"), StarterWarehouseEntries.Num() > 0);
	if (StarterWarehouseEntries.Num() > 0)
	{
		const int32 StarterGearSlot = InventoryWindow->FindBackpackEquipmentInstanceSlotForTest(StarterWarehouseEntries[0]);
		TestTrue(TEXT("starter gear backpack cell loads its equipment icon"), StarterGearSlot >= 0 && !InventoryWindow->GetBackpackSlotIconResourcePathForTest(StarterGearSlot).IsEmpty());
	}
	TestFalse(TEXT("free inventory window does not lock movement input"), InventoryWindow->IsModalInputLockActiveForTest());
	TestTrue(TEXT("inventory window remains available only in free mode"), InventoryWindow->OpenFreeInventoryForTest());
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

	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	Subsystem->GetMutableRuntimeState().PlayerHP = 25;
	Subsystem->GetMutableRuntimeState().PlayerMaxHP = 100;
	Subsystem->GetMutableRuntimeState().PlayerXP = 75;
	Subsystem->GetMutableRuntimeState().PlayerLevel = 1;
	TownHud->RefreshFromState();
	UTextBlock* HeroLevel = Cast<UTextBlock>(TownHud->GetWidgetFromName(TEXT("TownHudHeroLevel")));
	UTextBlock* HeroName = Cast<UTextBlock>(TownHud->GetWidgetFromName(TEXT("TownHudHeroName")));
	UTextBlock* IngotValue = Cast<UTextBlock>(TownHud->GetWidgetFromName(TEXT("TownHudIngotValue")));
	TestNotNull(TEXT("town HUD builds a named hero level text"), HeroLevel);
	TestNotNull(TEXT("town HUD builds a named hero name text"), HeroName);
	TestNotNull(TEXT("town HUD builds a named ingot value text"), IngotValue);
	if (HeroLevel)
	{
		TestEqual(TEXT("town HUD level text follows PlayerLevel"), HeroLevel->GetText().ToString(), FString(TEXT("Lv.1")));
	}
	if (IngotValue)
	{
		TestEqual(TEXT("town HUD ingot value follows PlayerGold"), IngotValue->GetText().ToString(), FString::FromInt(Subsystem->GetRuntimeState().PlayerGold));
	}

	TestTrue(TEXT("quest dialog accepts quest"), QuestDialog->AcceptQuest());
	TestEqual(TEXT("quest accepted in subsystem state"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	// New semantics: accepting through the dialog keeps the guide NPC in town; the follower
	// only joins when the player later clicks the dialog's 入队 action.
	TestFalse(TEXT("accepting the quest keeps the guide NPC in town until 入队"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestTrue(TEXT("town party selector explicitly adds Tusi Chief to this route"),
		Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.TusiChief")));

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
	const FGameXXKRuntimeState& BattlePresentationState = Subsystem->GetRuntimeState();
	const FGameXXKCardBattleRuntime& BattleRuntime = BattlePresentationState.CardRun.ActiveBattle;
	TestEqual(TEXT("the Qingshan UI battle projects the fixed three-member party"), BattlePresentationState.ActiveBattleParty.Num(), 3);
	const FGameXXKCardCombatUnit* StarterCompanionUnit = BattleRuntime.Units.FindByPredicate([StarterCompanionId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == StarterCompanionId && Unit.Side == EGameXXKCardTargetSide::Party;
	});
	const FGameXXKCardCombatUnit* HeroUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::Hero;
	});
	const FGameXXKCardCombatUnit* TaskNpcUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::QuestNpc;
	});
	TestTrue(TEXT("the UI battle includes the selected permanent companion"), StarterCompanionUnit != nullptr);
	TestTrue(TEXT("the UI battle includes the fixed hero"), HeroUnit != nullptr);
	TestTrue(TEXT("the UI battle includes the Qingshan task NPC"), TaskNpcUnit != nullptr);
	if (!StarterCompanionUnit || !HeroUnit || !TaskNpcUnit)
	{
		return false;
	}
	TestEqual(TEXT("the UI battle presents the permanent companion at 我 1P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, StarterCompanionUnit->UnitId), 1);
	TestEqual(TEXT("the UI battle presents the hero at 我 2P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, HeroUnit->UnitId), 2);
	TestEqual(TEXT("the UI battle presents the task NPC at 我 3P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, TaskNpcUnit->UnitId), 3);

	const TArray<FGameXXKBattlePresentationSlot> PresentationSlots = FGameXXKBattlePresentation::BuildSlots(BattleRuntime);
	TestTrue(TEXT("the UI presentation slot list retains companion 1P"), PresentationSlots.ContainsByPredicate([StarterCompanionId](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == StarterCompanionId && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 1;
	}));
	TestTrue(TEXT("the UI presentation slot list retains hero 2P"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Player") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 2;
	}));
	TestTrue(TEXT("the UI presentation slot list retains task NPC 3P"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Npc.TusiChief") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 3;
	}));
	TestTrue(TEXT("the UI battle exposes saved enemy intents"), !BattlePresentationState.CardRun.EnemyIntents.IsEmpty());
	if (!BattlePresentationState.CardRun.EnemyIntents.IsEmpty())
	{
		const FGameXXKCardEnemyIntent& FirstIntent = BattlePresentationState.CardRun.EnemyIntents[0];
		TestEqual(TEXT("the UI enemy intent source follows the enemy presentation slot"),
			FirstIntent.SourceSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SourceUnitId));
		TestEqual(TEXT("the UI enemy intent target follows the party presentation slot"),
			FirstIntent.TargetSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SuggestedTargetUnitId));
	}
	TestTrue(TEXT("battle widget can fail back to town"), Battle->FailToTown());
	TestEqual(TEXT("failure through battle widget returns to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("retry dungeon after battle widget failure"), DungeonMap->OpenFromTownExit());
	TestTrue(TEXT("retry start node after failure"), DungeonMap->SelectNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("retry battle node after failure"), DungeonMap->SelectNode(EGameXXKNodeKind::Battle));

	FGameXXKRuntimeState& BattleState = Subsystem->GetMutableRuntimeState();
	TestTrue(TEXT("battle widget opens a real card battle session"), BattleState.CardRun.bHasActiveCardBattle);
	bool bDefeatedEnemyCard = false;
	for (FGameXXKCardCombatUnit& Unit : BattleState.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
			bDefeatedEnemyCard = true;
		}
	}
	TestTrue(TEXT("battle fixture defeats the active enemy card before resolving victory"), bDefeatedEnemyCard);
	BattleState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	const int32 GoldBeforeBattle = Subsystem->GetRuntimeState().PlayerGold;
	const int32 TravelMoneyBeforeBattle = BattleState.CardRun.RouteTravelMoney;
	TestTrue(TEXT("battle widget opens the tiered three-choice route reward after a card battle victory"), Battle->ResolveVictory(false));
	TestEqual(TEXT("pending route reward keeps the battle screen active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("battle victory exposes exactly three pending route reward options"), BattleState.CardRun.PendingReward.Options.Num(), 3);
	TestEqual(TEXT("opening the route reward does not grant permanent gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBattle);
	TestEqual(TEXT("opening the route reward does not settle route travel money"), BattleState.CardRun.RouteTravelMoney, TravelMoneyBeforeBattle);
	FString RewardError;
	const bool bSkippedRouteReward = FGameXXKCardBattleAdapter::SkipPendingRouteReward(BattleState, &RewardError);
	TestTrue(FString::Printf(TEXT("skipping the pending route reward succeeds: %s"), *RewardError), bSkippedRouteReward);
	TestTrue(TEXT("battle widget finalizes victory after the route reward is resolved"), Battle->ResolveVictory(false));
	TestEqual(TEXT("finalized battle victory returns to the dungeon route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("finalized normal battle does not grant permanent gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeBattle);
	TestEqual(TEXT("finalized normal battle awards twenty route travel money"), BattleState.CardRun.RouteTravelMoney, TravelMoneyBeforeBattle + 20);

	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("inventory widget uses healing item"), Inventory->UseHealingItem());
	TestTrue(TEXT("healing item raises HP"), Subsystem->GetRuntimeState().PlayerHP > 1);

	UGameplayStatics::DeleteGameInSlot(UiTestSlot, UserIndex);
	return true;
}

#endif
