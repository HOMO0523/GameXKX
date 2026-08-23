#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryPersistentLocksTest,
	"GameXXK.Data.DesktopInventory.PersistentLocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryPersistentLocksTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("new-game Tool Auto Fill includes Warehouse by default"),
		State.DesktopInventory.bToolAutoFillIncludesWarehouse);
	TestEqual(TEXT("new-game equipment lock set starts empty"),
		State.DesktopInventory.LockedEquipmentInstanceIds.Num(), 0);
	TestEqual(TEXT("new-game item lock set starts empty"),
		State.DesktopInventory.LockedItemIds.Num(), 0);
	TestTrue(TEXT("lock fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State));

	if (!TestTrue(TEXT("lock fixture has unequipped equipment"),
		!State.EquipmentCollection.WarehouseInstanceIds.IsEmpty()))
	{
		return false;
	}
	const FName EquipmentInstanceId = State.EquipmentCollection.WarehouseInstanceIds[0];
	const FName ItemId = UGameXXKMVPRules::ItemEnhancementStone();
	const FGameXXKDesktopInventoryEntryKey EquipmentEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentInstanceId);
	const FGameXXKDesktopInventoryEntryKey ItemEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId);

	FString Error;
	TestTrue(TEXT("equipment locks by stable instance ID"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, EquipmentEntry, true, &Error));
	TestTrue(TEXT("item locks cover the whole item-type stack"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, ItemEntry, true, &Error));
	TestTrue(TEXT("equipment lock query reads the stable instance lock"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, EquipmentEntry));
	TestTrue(TEXT("item lock query reads the whole-stack lock"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, ItemEntry));

	const int32 ItemBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State,
		EGameXXKDesktopItemContainer::Backpack,
		ItemEntry);
	const int32 EquipmentBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State,
		EGameXXKDesktopItemContainer::Backpack,
		EquipmentEntry);
	if (!TestTrue(TEXT("locked item starts in a physical backpack slot"), ItemBackpackSlot != INDEX_NONE)
		|| !TestTrue(TEXT("locked equipment starts in a physical backpack slot"), EquipmentBackpackSlot != INDEX_NONE))
	{
		return false;
	}
	TestTrue(TEXT("locked item stack moves to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			ItemBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			31,
			&Error));
	TestTrue(TEXT("locked equipment instance moves to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			EquipmentBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			32,
			&Error));
	TestTrue(TEXT("item lock survives Backpack to Warehouse movement"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, ItemEntry));
	TestTrue(TEXT("equipment lock survives Backpack to Warehouse movement"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, EquipmentEntry));
	TestEqual(TEXT("locked whole stack keeps its exact quantity"),
		State.DesktopInventory.WarehouseItems.FindRef(ItemId), 10);

	TestTrue(TEXT("item stack unlock succeeds"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, ItemEntry, false, &Error));
	TestTrue(TEXT("equipment instance unlock succeeds"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, EquipmentEntry, false, &Error));
	TestFalse(TEXT("unlocked item stack no longer reports locked"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, ItemEntry));
	TestFalse(TEXT("unlocked equipment instance no longer reports locked"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, EquipmentEntry));
	TestTrue(TEXT("item can be relocked for stale-normalization coverage"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, ItemEntry, true, &Error));
	TestTrue(TEXT("equipment can be relocked for stale-normalization coverage"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(State, EquipmentEntry, true, &Error));

	const FName StaleEquipmentId(TEXT("Equipment.Instance.StaleLock"));
	const FName StaleItemId(TEXT("Item.StaleLock"));
	auto SerializeRuntimeState = [](const FGameXXKRuntimeState& Source)
	{
		FGameXXKRuntimeState Copy = Source;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		return Bytes;
	};
	FGameXXKRuntimeState CandidateValidationFailure = State;
	CandidateValidationFailure.DesktopInventory.LockedEquipmentInstanceIds.Remove(EquipmentInstanceId);
	CandidateValidationFailure.DesktopInventory.LockedItemIds.Add(StaleItemId);
	const TArray<uint8> BeforeCandidateFailureBytes = SerializeRuntimeState(CandidateValidationFailure);
	const int32 BeforeCandidateEquipmentLockCount =
		CandidateValidationFailure.DesktopInventory.LockedEquipmentInstanceIds.Num();
	const int32 BeforeCandidateItemLockCount =
		CandidateValidationFailure.DesktopInventory.LockedItemIds.Num();
	TestFalse(TEXT("valid lock preflight rolls back when candidate validation finds a separate stale lock"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			CandidateValidationFailure,
			EquipmentEntry,
			true,
			&Error));
	TestTrue(TEXT("candidate rollback failure comes from post-mutation stale-lock validation"),
		Error.Contains(TEXT("stale item lock")));
	TestEqual(TEXT("candidate validation rollback preserves every serialized runtime byte"),
		SerializeRuntimeState(CandidateValidationFailure),
		BeforeCandidateFailureBytes);
	TestEqual(TEXT("candidate validation rollback preserves the equipment lock set"),
		CandidateValidationFailure.DesktopInventory.LockedEquipmentInstanceIds.Num(),
		BeforeCandidateEquipmentLockCount);
	TestEqual(TEXT("candidate validation rollback preserves the item lock set"),
		CandidateValidationFailure.DesktopInventory.LockedItemIds.Num(),
		BeforeCandidateItemLockCount);
	TestFalse(TEXT("failed candidate never commits the requested valid equipment lock"),
		CandidateValidationFailure.DesktopInventory.LockedEquipmentInstanceIds.Contains(EquipmentInstanceId));
	TestTrue(TEXT("failed candidate keeps the pre-existing stale item lock untouched"),
		CandidateValidationFailure.DesktopInventory.LockedItemIds.Contains(StaleItemId));

	const FGameXXKRuntimeState BeforeInvalidSet = State;
	TestFalse(TEXT("empty entry lock request is rejected"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State, FGameXXKDesktopInventoryEntryKey(), true, &Error));
	TestTrue(TEXT("empty lock rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&State, &BeforeInvalidSet, PPF_None));
	TestFalse(TEXT("stale equipment instance lock request is rejected"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(StaleEquipmentId),
			true,
			&Error));
	TestTrue(TEXT("stale equipment lock rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&State, &BeforeInvalidSet, PPF_None));
	TestFalse(TEXT("stale item stack lock request is rejected"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State,
			FGameXXKDesktopInventoryRules::MakeItemEntry(StaleItemId),
			true,
			&Error));
	TestTrue(TEXT("stale item lock rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&State, &BeforeInvalidSet, PPF_None));

	FGameXXKRuntimeState InvalidEquipmentLock = State;
	InvalidEquipmentLock.DesktopInventory.LockedEquipmentInstanceIds.Add(StaleEquipmentId);
	TestFalse(TEXT("validation rejects a stale equipment lock"),
		FGameXXKDesktopInventoryRules::Validate(InvalidEquipmentLock, &Error));
	FGameXXKRuntimeState InvalidItemLock = State;
	InvalidItemLock.DesktopInventory.LockedItemIds.Add(StaleItemId);
	TestFalse(TEXT("validation rejects a stale item lock"),
		FGameXXKDesktopInventoryRules::Validate(InvalidItemLock, &Error));

	const TArray<FGameXXKDesktopInventoryEntryKey> BackpackSlotsBeforeNormalize =
		State.DesktopInventory.BackpackSlots;
	const TArray<FGameXXKDesktopInventoryEntryKey> WarehouseSlotsBeforeNormalize =
		State.DesktopInventory.WarehouseSlots;
	const TArray<FName> WarehouseEquipmentBeforeNormalize =
		State.DesktopInventory.WarehouseEquipmentInstanceIds;
	const TMap<FName, int32> WarehouseItemsBeforeNormalize =
		State.DesktopInventory.WarehouseItems;
	State.DesktopInventory.LockedEquipmentInstanceIds.Add(StaleEquipmentId);
	State.DesktopInventory.LockedItemIds.Add(StaleItemId);
	TestTrue(TEXT("normalization removes stale locks"),
		FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	TestTrue(TEXT("normalization retains the valid moved equipment lock"),
		State.DesktopInventory.LockedEquipmentInstanceIds.Contains(EquipmentInstanceId));
	TestTrue(TEXT("normalization retains the valid moved item lock"),
		State.DesktopInventory.LockedItemIds.Contains(ItemId));
	TestFalse(TEXT("normalization removes only the stale equipment lock"),
		State.DesktopInventory.LockedEquipmentInstanceIds.Contains(StaleEquipmentId));
	TestFalse(TEXT("normalization removes only the stale item lock"),
		State.DesktopInventory.LockedItemIds.Contains(StaleItemId));
	TestEqual(TEXT("lock normalization preserves Backpack physical cells"),
		State.DesktopInventory.BackpackSlots, BackpackSlotsBeforeNormalize);
	TestEqual(TEXT("lock normalization preserves Warehouse physical cells"),
		State.DesktopInventory.WarehouseSlots, WarehouseSlotsBeforeNormalize);
	TestEqual(TEXT("lock normalization preserves the equipment partition"),
		State.DesktopInventory.WarehouseEquipmentInstanceIds, WarehouseEquipmentBeforeNormalize);
	bool bWarehouseItemsPreserved =
		State.DesktopInventory.WarehouseItems.Num() == WarehouseItemsBeforeNormalize.Num();
	for (const TPair<FName, int32>& Pair : WarehouseItemsBeforeNormalize)
	{
		const int32* ActualQuantity = State.DesktopInventory.WarehouseItems.Find(Pair.Key);
		bWarehouseItemsPreserved = bWarehouseItemsPreserved
			&& ActualQuantity
			&& *ActualQuantity == Pair.Value;
	}
	TestTrue(TEXT("lock normalization preserves the item partition"), bWarehouseItemsPreserved);
	return true;
}

#endif
