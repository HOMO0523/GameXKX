#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryPersistentContainerTest,
	"GameXXK.DesktopInventory.PersistentContainers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryPersistentContainerTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("new-game desktop inventory normalizes"), FGameXXKDesktopInventoryRules::Normalize(State));

	const FGameXXKDesktopInventoryEntryKey StoneKey = FGameXXKDesktopInventoryRules::MakeItemEntry(
		UGameXXKMVPRules::ItemEnhancementStone());
	const int32 StoneBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State,
		EGameXXKDesktopItemContainer::Backpack,
		StoneKey);
	TestTrue(TEXT("starter enhancement stones occupy one physical backpack slot"), StoneBackpackSlot != INDEX_NONE);
	TestEqual(TEXT("starter storage begins empty"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(State, EGameXXKDesktopItemContainer::Warehouse),
		0);

	FString Error;
	TestTrue(TEXT("whole stack moves from backpack into an exact warehouse slot"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			StoneBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			5,
			&Error));
	TestEqual(TEXT("moved stack leaves the gameplay backpack"), State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()), 0);
	TestEqual(TEXT("moved stack is save-authoritative in warehouse storage"),
		State.DesktopInventory.WarehouseItems.FindRef(UGameXXKMVPRules::ItemEnhancementStone()),
		10);
	TestEqual(TEXT("legacy enhancement mirror follows the backpack balance"), State.EnhancementMaterial, 0);
	TestEqual(TEXT("warehouse preserves the requested physical slot"),
		FGameXXKDesktopInventoryRules::GetEntryAt(State, EGameXXKDesktopItemContainer::Warehouse, 5),
		StoneKey);

	const FName EquipmentInstanceId = State.EquipmentCollection.WarehouseInstanceIds[0];
	const FGameXXKDesktopInventoryEntryKey EquipmentKey = FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentInstanceId);
	const int32 EquipmentBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State,
		EGameXXKDesktopItemContainer::Backpack,
		EquipmentKey);
	TestTrue(TEXT("starter equipment occupies a physical backpack slot"), EquipmentBackpackSlot != INDEX_NONE);
	TestTrue(TEXT("equipment moves into the warehouse without violating equipment collection ownership"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			EquipmentBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			6,
			&Error));
	TestTrue(TEXT("warehouse partition records the equipment instance"),
		State.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(EquipmentInstanceId));
	TestTrue(TEXT("equipment remains part of the validated unequipped collection"),
		State.EquipmentCollection.WarehouseInstanceIds.Contains(EquipmentInstanceId));

	const FGameXXKRuntimeState BeforeOccupiedDrop = State;
	TestFalse(TEXT("dropping on an occupied destination is rejected without swapping"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Warehouse,
			5,
			EGameXXKDesktopItemContainer::Warehouse,
			6,
			&Error));
	TestEqual(TEXT("rejected occupied drop keeps the source slot"),
		FGameXXKDesktopInventoryRules::GetEntryAt(State, EGameXXKDesktopItemContainer::Warehouse, 5),
		FGameXXKDesktopInventoryRules::GetEntryAt(BeforeOccupiedDrop, EGameXXKDesktopItemContainer::Warehouse, 5));

	FString ValidationError;
	TestTrue(TEXT("desktop storage remains runtime-valid"), FGameXXKDesktopInventoryRules::Validate(State, &ValidationError));
	FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(State);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("current save restores desktop storage"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, Report));
	TestEqual(TEXT("warehouse stack slot survives save/restore"),
		FGameXXKDesktopInventoryRules::GetEntryAt(Restored, EGameXXKDesktopItemContainer::Warehouse, 5),
		StoneKey);
	TestEqual(TEXT("warehouse equipment slot survives save/restore"),
		FGameXXKDesktopInventoryRules::GetEntryAt(Restored, EGameXXKDesktopItemContainer::Warehouse, 6),
		EquipmentKey);
	return true;
}

#endif
