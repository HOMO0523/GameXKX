#include "GameXXKMVPRules.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static int32 InventoryCategoryRank(EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Weapon:
		case EGameXXKItemKind::Armor:
		case EGameXXKItemKind::Accessory:
			return 0;
		case EGameXXKItemKind::Consumable:
			return 1;
		case EGameXXKItemKind::Material:
			return 2;
		case EGameXXKItemKind::Task:
			return 3;
		default:
			return 4;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKInventoryEnhancementTest,
	"GameXXK.MVP.Inventory.EnhancementAndStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInventoryEnhancementTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	const FName Weapon = UGameXXKMVPRules::ItemWoodenSword();
	const FName Armor = UGameXXKMVPRules::ItemStarterClothArmor();
	const FName Accessory = UGameXXKMVPRules::ItemClothTalisman();
	const FName EnhancementStone = UGameXXKMVPRules::ItemEnhancementStone();
	const FName QingshanRouteSeal = UGameXXKMVPRules::ItemQingshanRouteSeal();

	TestEqual(TEXT("new game grants ten enhancement materials"), State.EnhancementMaterial, 10);
	TestEqual(TEXT("new game exposes enhancement material in the backpack"), UGameXXKMVPRules::GetItemCount(State, EnhancementStone), 10);
	TestTrue(TEXT("owned equipment can be enhanced"), UGameXXKMVPRules::CanEnhanceItem(State, Weapon));
	TestFalse(TEXT("consumables cannot be enhanced"), UGameXXKMVPRules::CanEnhanceItem(State, UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("equipment can be decomposed into material"), UGameXXKMVPRules::CanDecomposeItem(State, Weapon));
	TestFalse(TEXT("consumables cannot be decomposed"), UGameXXKMVPRules::CanDecomposeItem(State, UGameXXKMVPRules::ItemHealingPowder()));
	TestFalse(TEXT("task items cannot be bought directly"), UGameXXKMVPRules::BuyItem(State, QingshanRouteSeal, 1));
	TestEqual(TEXT("new game equipment schema is one"), State.EquipmentCollection.EquipmentSchemaVersion, 1);
	TestTrue(TEXT("new game collection seed is nonzero"), State.EquipmentCollection.CollectionSeed != 0);
	TestEqual(TEXT("new game starter ordinal advances exactly three times"), State.EquipmentCollection.NextInstanceOrdinal, 3);
	TestEqual(TEXT("new game has exactly three starter warehouse entries"), State.EquipmentCollection.WarehouseInstanceIds.Num(), 3);
	const TArray<FName> ExpectedStarterOrder{Weapon, Armor, Accessory};
	for (int32 Index = 0; Index < ExpectedStarterOrder.Num() && State.EquipmentCollection.WarehouseInstanceIds.IsValidIndex(Index); ++Index)
	{
		const FGameXXKEquipmentInstance* Starter = FGameXXKEquipmentRules::FindInstance(
			State.EquipmentCollection,
			State.EquipmentCollection.WarehouseInstanceIds[Index]);
		TestEqual(TEXT("starter warehouse order is exact"), Starter ? Starter->BaseEquipmentId : NAME_None, ExpectedStarterOrder[Index]);
		TestEqual(TEXT("starter equipment remains unequipped"), Starter ? Starter->OwnerKind : EGameXXKEquipmentOwnerKind::Invalid, EGameXXKEquipmentOwnerKind::Warehouse);
	}

	const int32 BaseAttack = State.PlayerAttack;
	const int32 BaseDefense = State.PlayerDefense;
	const int32 BaseSpeed = State.PlayerSpeed;
	TestTrue(TEXT("weapon enhancement spends one material"), UGameXXKMVPRules::EnhanceItem(State, Weapon));
	TestEqual(TEXT("weapon enhancement level starts at one"), UGameXXKMVPRules::GetItemEnhancementLevel(State, Weapon), 1);
	TestEqual(TEXT("weapon enhancement consumes one material"), State.EnhancementMaterial, 9);
	TestEqual(TEXT("weapon enhancement consumes one backpack material"), UGameXXKMVPRules::GetItemCount(State, EnhancementStone), 9);
	TestEqual(TEXT("un-equipped weapon enhancement does not change attack"), State.PlayerAttack, BaseAttack);
	TestTrue(TEXT("enhanced weapon equips"), UGameXXKMVPRules::EquipItem(State, Weapon));
	TestEqual(TEXT("enhanced weapon adds base plus one attack"), State.PlayerAttack, BaseAttack + 4);

	TestTrue(TEXT("armor enhancement succeeds"), UGameXXKMVPRules::EnhanceItem(State, Armor));
	TestTrue(TEXT("enhanced armor equips"), UGameXXKMVPRules::EquipItem(State, Armor));
	TestEqual(TEXT("enhanced armor adds base plus one defense"), State.PlayerDefense, BaseDefense + 4);

	TestTrue(TEXT("accessory enhancement succeeds"), UGameXXKMVPRules::EnhanceItem(State, Accessory));
	TestTrue(TEXT("enhanced accessory equips"), UGameXXKMVPRules::EquipItem(State, Accessory));
	TestEqual(TEXT("enhanced accessory adds one speed"), State.PlayerSpeed, BaseSpeed + 1);

	State.EnhancementMaterial = 20;
	State.Inventory.Add(EnhancementStone, 20);
	while (UGameXXKMVPRules::CanEnhanceItem(State, Weapon))
	{
		TestTrue(TEXT("weapon can advance toward the cap"), UGameXXKMVPRules::EnhanceItem(State, Weapon));
	}
	TestEqual(TEXT("enhancement caps at plus ten"), UGameXXKMVPRules::GetItemEnhancementLevel(State, Weapon), 10);
	TestFalse(TEXT("plus ten weapon cannot consume another material"), UGameXXKMVPRules::EnhanceItem(State, Weapon));
	TestEqual(TEXT("equipped plus ten weapon contributes ten extra attack"), State.PlayerAttack, BaseAttack + 13);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(State);
	TestEqual(TEXT("enhancement save uses the current version seven format"), SaveState.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport RestoreReport;
	TestTrue(TEXT("current enhancement save restores through typed dispatcher"), FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, RestoreReport));
	TestEqual(TEXT("save restores enhancement material"), Restored.EnhancementMaterial, State.EnhancementMaterial);
	TestEqual(TEXT("save restores enhancement material backpack item"), UGameXXKMVPRules::GetItemCount(Restored, EnhancementStone), State.EnhancementMaterial);
	TestEqual(TEXT("save restores enhancement levels"), UGameXXKMVPRules::GetItemEnhancementLevel(Restored, Weapon), 10);

	FGameXXKSaveState VersionTwoSave;
	VersionTwoSave.SaveVersion = 2;
	VersionTwoSave.RuntimeState = FGameXXKRuntimeState();
	VersionTwoSave.RuntimeState.Inventory.Reset();
	VersionTwoSave.RuntimeState.Inventory.Add(Weapon, 1);
	VersionTwoSave.RuntimeState.EnhancementMaterial = 0;
	VersionTwoSave.RuntimeState.ItemEnhancementLevels.Add(Weapon, 4);
	FGameXXKRuntimeState MigratedVersionTwoState;
	FGameXXKSaveMigrationReport VersionTwoReport;
	TestTrue(TEXT("version two migration succeeds through typed dispatcher"), FGameXXKSaveMigration::TryRestoreRuntimeState(VersionTwoSave, MigratedVersionTwoState, VersionTwoReport));
	TestEqual(TEXT("version two migration grants default enhancement materials"), MigratedVersionTwoState.EnhancementMaterial, 10);
	TestEqual(TEXT("version two migration adds enhancement material backpack item"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, EnhancementStone), 10);
	TestEqual(TEXT("version two migration clears unsupported enhancement levels"), UGameXXKMVPRules::GetItemEnhancementLevel(MigratedVersionTwoState, Weapon), 0);
	TestEqual(TEXT("version two migration keeps only explicit legacy weapon"), MigratedVersionTwoState.EquipmentCollection.EquipmentInstances.Num(), 1);
	TestEqual(TEXT("version two migration inherits no starter armor"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, Armor), 0);
	TestEqual(TEXT("version two migration inherits no starter accessory"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, Accessory), 0);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestNotNull(TEXT("inventory test subsystem exists"), Subsystem);
	Subsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("inventory task test starts a new game"), Subsystem->StartGame());
	TestTrue(TEXT("inventory task test enters Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("inventory task test accepts the Qingshan route quest"), Subsystem->AcceptQuest());
	TestEqual(TEXT("accepting the route quest adds its task item"), Subsystem->GetItemCount(QingshanRouteSeal), 1);
	TestFalse(TEXT("active task item cannot be sold"), Subsystem->CanSellItem(QingshanRouteSeal));
	TestTrue(TEXT("test state can buy a second equipment item"), Subsystem->BuyItem(UGameXXKMVPRules::ItemIronSword(), 1));

	UGameXXKInventoryWindowWidget* InventoryWindow = NewObject<UGameXXKInventoryWindowWidget>();
	InventoryWindow->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("inventory window opens for filter and action tests"), InventoryWindow->OpenFreeInventoryForTest());
	TestEqual(TEXT("inventory starts on all filter"), InventoryWindow->GetActiveInventoryFilterForTest(), EGameXXKInventoryFilter::All);
	TestTrue(TEXT("backpack selection starts on wooden sword"), InventoryWindow->SelectPlayerBackpackItemForTest(Weapon));
	TestTrue(TEXT("equipment filter preserves selected wooden sword"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Equipment));
	const int32 EquipmentFilterWoodenSwordIndex = InventoryWindow->GetVisibleBackpackItemIdsForTest().IndexOfByKey(Weapon);
	TestTrue(TEXT("equipment filter keeps wooden sword visible"), EquipmentFilterWoodenSwordIndex != INDEX_NONE);
	TestEqual(TEXT("equipment filter rebinds the selected highlight to wooden sword"), InventoryWindow->GetSelectedBackpackSlotIndexForTest(), EquipmentFilterWoodenSwordIndex);
	TestTrue(TEXT("sort preserves selected wooden sword"), InventoryWindow->SortInventoryForTest());
	const int32 SortedWoodenSwordIndex = InventoryWindow->GetVisibleBackpackItemIdsForTest().IndexOfByKey(Weapon);
	TestTrue(TEXT("sorted backpack keeps wooden sword visible"), SortedWoodenSwordIndex != INDEX_NONE);
	TestEqual(TEXT("sort rebinds the selected highlight to wooden sword"), InventoryWindow->GetSelectedBackpackSlotIndexForTest(), SortedWoodenSwordIndex);
	TestTrue(TEXT("selected wooden sword equips before equipped-slot decomposition"), InventoryWindow->ExecuteSelectedPrimaryActionForTest());
	TestEqual(TEXT("wooden sword is equipped before equipped-slot decomposition"), InventoryWindow->GetEquippedItemForSlotForTest(FName(TEXT("Weapon"))), Weapon);
	InventoryWindow->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::Equipment, 0, FName(TEXT("Weapon")));
	const int32 MaterialBeforeEquippedDecompose = Subsystem->GetItemCount(EnhancementStone);
	const int32 SandBeforeEquippedDecompose = Subsystem->GetRuntimeState().EquipmentCollection.RefinementSand;
	const int32 GoldBeforeEquippedDecompose = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("equipped item can open global decomposition confirmation"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("equipped item decomposition confirmation executes"), InventoryWindow->ConfirmDialogForTest());
	TestEqual(TEXT("equipped item decomposition removes the last owned copy"), Subsystem->GetItemCount(Weapon), 0);
	TestEqual(TEXT("equipped item decomposition clears the equipment slot"), InventoryWindow->GetEquippedItemForSlotForTest(FName(TEXT("Weapon"))), NAME_None);
	TestEqual(TEXT("zero-enhancement decomposition does not invent enhancement stones"), Subsystem->GetItemCount(EnhancementStone), MaterialBeforeEquippedDecompose);
	TestEqual(TEXT("common equipment decomposition grants refinement sand"), Subsystem->GetRuntimeState().EquipmentCollection.RefinementSand, SandBeforeEquippedDecompose + 5);
	TestEqual(TEXT("equipped item decomposition does not grant merchant gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeEquippedDecompose);
	TestTrue(TEXT("all filter is restored after equipped-slot decomposition"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::All));
	TestTrue(TEXT("equipment filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Equipment));
	for (const FName ItemId : InventoryWindow->GetVisibleBackpackItemIdsForTest())
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		TestTrue(TEXT("equipment filter contains only equipment"), bFound && InventoryCategoryRank(Def.Kind) == 0);
	}
	TestTrue(TEXT("material filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Materials));
	TestTrue(TEXT("material filter shows the enhancement material"), InventoryWindow->GetVisibleBackpackItemIdsForTest().Contains(EnhancementStone));
	for (const FName ItemId : InventoryWindow->GetVisibleBackpackItemIdsForTest())
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		TestTrue(TEXT("material filter contains only material"), bFound && Def.Kind == EGameXXKItemKind::Material);
	}
	TestTrue(TEXT("material filter can select enhancement material"), InventoryWindow->SelectPlayerBackpackItemForTest(EnhancementStone));
	TestTrue(TEXT("material selection does not offer an invalid primary action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().IsEmpty());
	TestFalse(TEXT("material selection cannot open decomposition"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("task filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Tasks));
	TestTrue(TEXT("task filter shows active Qingshan task item"), InventoryWindow->GetVisibleBackpackItemIdsForTest().Contains(QingshanRouteSeal));
	TestTrue(TEXT("task filter can select active Qingshan task item"), InventoryWindow->SelectPlayerBackpackItemForTest(QingshanRouteSeal));
	TestTrue(TEXT("task selection does not offer an invalid primary action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().IsEmpty());
	TestFalse(TEXT("task selection cannot open decomposition"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("all filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::All));
	TestTrue(TEXT("sort control succeeds"), InventoryWindow->SortInventoryForTest());
	const TArray<FName> SortedItems = InventoryWindow->GetVisibleBackpackItemIdsForTest();
	int32 PreviousRank = INDEX_NONE;
	FString PreviousName;
	for (const FName ItemId : SortedItems)
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		const int32 Rank = InventoryCategoryRank(Def.Kind);
		TestTrue(TEXT("sort keeps category order"), PreviousRank == INDEX_NONE || PreviousRank <= Rank);
		TestTrue(TEXT("sort keeps display name order inside a category"), PreviousRank != Rank || PreviousName <= Def.DisplayName.ToString());
		PreviousRank = Rank;
		PreviousName = Def.DisplayName.ToString();
	}

	const FName Powder = UGameXXKMVPRules::ItemHealingPowder();
	const int32 GoldBeforeDecompose = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeDecompose = Subsystem->GetItemCount(Powder);
	TestTrue(TEXT("decompose selects a player backpack item"), InventoryWindow->SelectPlayerBackpackItemForTest(Powder));
	TestFalse(TEXT("decompose rejects consumables instead of selling them"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestFalse(TEXT("consumable decompose does not open confirmation"), InventoryWindow->IsConfirmationDialogVisibleForTest());
	TestEqual(TEXT("consumable decompose keeps the item"), Subsystem->GetItemCount(Powder), PowderBeforeDecompose);
	TestEqual(TEXT("consumable decompose keeps gold unchanged"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeDecompose);

	const FName IronSword = UGameXXKMVPRules::ItemIronSword();
	TestTrue(TEXT("inventory can select equipment for enhancement"), InventoryWindow->SelectPlayerBackpackItemForTest(IronSword));
	TestTrue(TEXT("inventory enhancement opens confirmation"), InventoryWindow->RequestSelectedEnhanceForTest());
	TestTrue(TEXT("inventory enhancement confirmation executes"), InventoryWindow->ConfirmDialogForTest());
	TestTrue(TEXT("inventory detail reports enhancement level"), InventoryWindow->GetSelectedDetailTextForTest().ToString().Contains(TEXT("+1 / +10")));

	const FString EnhancementSaveSlot = TEXT("GameXXK_MVP_Automation_InventoryEnhancement");
	const FString EnhancementBackupSlot = EnhancementSaveSlot + TEXT(".PreV7Backup");
	const int32 EnhancementSaveUserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(EnhancementSaveSlot, EnhancementSaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(EnhancementBackupSlot, EnhancementSaveUserIndex);
	UGameInstance* EnhancementSaveSourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EnhancementSaveSourceSubsystem = NewObject<UGameXXKMVPSubsystem>(EnhancementSaveSourceGameInstance);
	EnhancementSaveSourceSubsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("save path enhancement succeeds"), EnhancementSaveSourceSubsystem->EnhanceItem(Weapon));
	TestTrue(TEXT("save path writes enhancement state"), EnhancementSaveSourceSubsystem->SaveCurrentGame(EnhancementSaveSlot, EnhancementSaveUserIndex));
	TestFalse(TEXT("current v7 save creates no migration backup"), UGameplayStatics::DoesSaveGameExist(EnhancementBackupSlot, EnhancementSaveUserIndex));
	UGameInstance* EnhancementSaveLoadGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EnhancementSaveLoadSubsystem = NewObject<UGameXXKMVPSubsystem>(EnhancementSaveLoadGameInstance);
	TestTrue(TEXT("save path reloads enhancement state"), EnhancementSaveLoadSubsystem->LoadGameFromSlot(EnhancementSaveSlot, EnhancementSaveUserIndex));
	TestEqual(TEXT("real SaveGame reload preserves enhancement material"), EnhancementSaveLoadSubsystem->GetRuntimeState().EnhancementMaterial, 9);
	TestEqual(TEXT("real SaveGame reload preserves enhancement material backpack item"), EnhancementSaveLoadSubsystem->GetItemCount(EnhancementStone), 9);
	TestEqual(TEXT("real SaveGame reload preserves enhancement level"), EnhancementSaveLoadSubsystem->GetItemEnhancementLevel(Weapon), 1);
	UGameplayStatics::DeleteGameInSlot(EnhancementSaveSlot, EnhancementSaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(EnhancementBackupSlot, EnhancementSaveUserIndex);
	TestTrue(TEXT("selling the final enhanced copy succeeds"), Subsystem->SellItem(IronSword, 1));
	TestEqual(TEXT("selling the final enhanced copy clears its enhancement state"), Subsystem->GetItemEnhancementLevel(IronSword), 0);

	return true;
}

#endif
