#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> SerializeDesktopInventoryRuntimeState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	FGameXXKDesktopInventoryMoveRequest MakeMoveRequest(
		const EGameXXKDesktopItemContainer FromContainer,
		const int32 FromSlotIndex,
		const EGameXXKDesktopItemContainer ToContainer,
		const int32 ToSlotIndex,
		const bool bAllowSwap = true,
		const FGameXXKDesktopInventoryEntryKey ExpectedEntry = FGameXXKDesktopInventoryEntryKey())
	{
		FGameXXKDesktopInventoryMoveRequest Request;
		Request.FromContainer = FromContainer;
		Request.FromSlotIndex = FromSlotIndex;
		Request.ToContainer = ToContainer;
		Request.ToSlotIndex = ToSlotIndex;
		Request.bAllowSwap = bAllowSwap;
		Request.ExpectedEntry = ExpectedEntry;
		return Request;
	}

	bool BuildLegacyOverflowState(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& OutState)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.Inventory.Reset();
		OutState.EnhancementMaterial = 0;
		OutState.ItemEnhancementLevels.Reset();
		OutState.EquippedWeapon = NAME_None;
		OutState.EquippedArmor = NAME_None;
		OutState.EquippedAccessory = NAME_None;
		OutState.EquipmentCollection = FGameXXKEquipmentCollectionState();
		OutState.EquipmentCollection.CollectionSeed = 0x7315;
		OutState.DesktopInventory = FGameXXKDesktopInventoryState();
		const FGameXXKEquipmentDefinition* Definition =
			FGameXXKEquipmentCatalog::FindDefinition(TEXT("Item.WoodenSword"));
		if (!Test.TestNotNull(TEXT("legacy-overflow fixture finds Wooden Sword"), Definition))
		{
			return false;
		}
		for (int32 Index = 0; Index <= FGameXXKEquipmentRules::WarehouseCapacity; ++Index)
		{
			FGameXXKEquipmentInstance Instance;
			Instance.InstanceId = FName(*FString::Printf(
				TEXT("EquipmentInstance.DesktopOverflow.%03d"), Index));
			Instance.BaseEquipmentId = Definition->Id;
			Instance.ItemLevel = 1;
			Instance.Quality = EGameXXKEquipmentQuality::Common;
			Instance.ScalingRule = Definition->ScalingRule;
			Instance.LegacyBaseStatSnapshot = Definition->LegacyBaseStatSnapshot;
			Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
			OutState.EquipmentCollection.WarehouseInstanceIds.Add(Instance.InstanceId);
			OutState.EquipmentCollection.EquipmentInstances.Add(MoveTemp(Instance));
		}
		OutState.EquipmentCollection.NextInstanceOrdinal =
			OutState.EquipmentCollection.EquipmentInstances.Num();
		OutState.EquipmentCollection.bLegacyWarehouseOverflow = true;
		FString Error;
		return Test.TestTrue(
			TEXT("legacy-overflow fixture collection validates"),
			FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				OutState.EquipmentCollection,
				OutState.CardRun.CompanionRoster,
				&Error))
			&& Test.TestTrue(
				TEXT("legacy-overflow fixture projects its visible first page"),
				FGameXXKDesktopInventoryRules::Normalize(OutState, &Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryPersistentContainerTest,
	"GameXXK.DesktopInventory.PersistentContainers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryPersistentContainerTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* StartedSubsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("current inventory fixture starts with a legal formation"),
		StartedSubsystem && StartedSubsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = StartedSubsystem->GetRuntimeStateCopy();
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
	TestTrue(FString::Printf(TEXT("current save restores desktop storage: %s"), *Report.Error),
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryMoveOrSwapTest,
	"GameXXK.Data.DesktopInventory.MoveOrSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryMoveOrSwapTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("move/swap fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error)))
	{
		AddError(Error);
		return false;
	}

	TArray<int32> OccupiedBackpackSlots;
	for (int32 SlotIndex = 0; SlotIndex < State.DesktopInventory.BackpackSlots.Num(); ++SlotIndex)
	{
		if (State.DesktopInventory.BackpackSlots[SlotIndex].IsValid())
		{
			OccupiedBackpackSlots.Add(SlotIndex);
			if (OccupiedBackpackSlots.Num() == 2)
			{
				break;
			}
		}
	}
	if (!TestEqual(TEXT("same-container swap fixture has two occupied cells"), OccupiedBackpackSlots.Num(), 2))
	{
		return false;
	}

	const int32 FirstSlot = OccupiedBackpackSlots[0];
	const int32 SecondSlot = OccupiedBackpackSlots[1];
	const FGameXXKDesktopInventoryEntryKey FirstEntry = State.DesktopInventory.BackpackSlots[FirstSlot];
	const FGameXXKDesktopInventoryEntryKey SecondEntry = State.DesktopInventory.BackpackSlots[SecondSlot];
	const TArray<uint8> BeforeSameCell = SerializeDesktopInventoryRuntimeState(State);
	TestTrue(TEXT("dropping an entry on its own cell succeeds"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			State,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, FirstSlot,
				EGameXXKDesktopItemContainer::Backpack, FirstSlot),
			&Error));
	TestEqual(TEXT("same-cell success does not normalize or mutate authoritative state"),
		SerializeDesktopInventoryRuntimeState(State), BeforeSameCell);

	const TArray<uint8> BeforeRejectedSwap = SerializeDesktopInventoryRuntimeState(State);
	TestFalse(TEXT("an occupied target rejects when swapping is disabled"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			State,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, FirstSlot,
				EGameXXKDesktopItemContainer::Backpack, SecondSlot, false),
			&Error));
	TestTrue(TEXT("disabled-swap rejection reports the occupied destination"), Error.Contains(TEXT("occupied")));
	TestEqual(TEXT("disabled-swap rejection preserves every serialized runtime byte"),
		SerializeDesktopInventoryRuntimeState(State), BeforeRejectedSwap);
	TestFalse(TEXT("legacy MoveEntry keeps occupied-destination rejection compatibility"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			FirstSlot,
			EGameXXKDesktopItemContainer::Backpack,
			SecondSlot,
			&Error));
	TestEqual(TEXT("legacy occupied rejection is atomic"),
		SerializeDesktopInventoryRuntimeState(State), BeforeRejectedSwap);

	FGameXXKRuntimeState ExpectedSameContainerSwap = State;
	Swap(
		ExpectedSameContainerSwap.DesktopInventory.BackpackSlots[FirstSlot],
		ExpectedSameContainerSwap.DesktopInventory.BackpackSlots[SecondSlot]);
	TestTrue(TEXT("occupied cells swap within one authoritative container"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			State,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, FirstSlot,
				EGameXXKDesktopItemContainer::Backpack, SecondSlot),
			&Error));
	TestEqual(TEXT("same-container swap reverses the first physical key"),
		State.DesktopInventory.BackpackSlots[FirstSlot], SecondEntry);
	TestEqual(TEXT("same-container swap reverses the second physical key"),
		State.DesktopInventory.BackpackSlots[SecondSlot], FirstEntry);
	TestTrue(TEXT("same-container swap changes no authoritative item or equipment partition"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&State, &ExpectedSameContainerSwap, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryCrossContainerSwapTest,
	"GameXXK.Data.DesktopInventory.CrossContainerSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryCrossContainerSwapTest::RunTest(const FString& Parameters)
{
	FString Error;
	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const FName SandId = UGameXXKMVPRules::ItemRefinementSand();
	FGameXXKRuntimeState ItemState = UGameXXKMVPRules::CreateNewGame();
	ItemState.Inventory.Add(StoneId, 11);
	ItemState.Inventory.Add(SandId, 7);
	ItemState.EnhancementMaterial = 11;
	ItemState.EquipmentCollection.RefinementSand = 7;
	if (!TestTrue(TEXT("item cross-swap fixture normalizes"),
		FGameXXKDesktopInventoryRules::Normalize(ItemState, &Error)))
	{
		AddError(Error);
		return false;
	}

	const FGameXXKDesktopInventoryEntryKey StoneEntry = FGameXXKDesktopInventoryRules::MakeItemEntry(StoneId);
	const FGameXXKDesktopInventoryEntryKey SandEntry = FGameXXKDesktopInventoryRules::MakeItemEntry(SandId);
	const int32 InitialStoneSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		ItemState, EGameXXKDesktopItemContainer::Backpack, StoneEntry);
	TestTrue(TEXT("whole stack moves Backpack to an exact empty Warehouse cell"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			ItemState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, InitialStoneSlot,
				EGameXXKDesktopItemContainer::Warehouse, 17),
			&Error));
	TestEqual(TEXT("empty cross-container move keeps the whole stack quantity"),
		ItemState.DesktopInventory.WarehouseItems.FindRef(StoneId), 11);
	TestEqual(TEXT("moving stones out synchronizes the legacy enhancement mirror"),
		ItemState.EnhancementMaterial, 0);
	TestTrue(TEXT("whole stack moves Warehouse back to an exact empty Backpack cell"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			ItemState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Warehouse, 17,
				EGameXXKDesktopItemContainer::Backpack, InitialStoneSlot),
			&Error));
	TestEqual(TEXT("return move restores the exact Backpack quantity"), ItemState.Inventory.FindRef(StoneId), 11);
	TestEqual(TEXT("return move restores the legacy enhancement mirror"), ItemState.EnhancementMaterial, 11);

	const int32 SandBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		ItemState, EGameXXKDesktopItemContainer::Backpack, SandEntry);
	TestTrue(TEXT("item/item fixture moves sand to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			ItemState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, SandBackpackSlot,
				EGameXXKDesktopItemContainer::Warehouse, 21),
			&Error));
	TestTrue(TEXT("different whole item stacks swap across Backpack and Warehouse"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			ItemState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, InitialStoneSlot,
				EGameXXKDesktopItemContainer::Warehouse, 21),
			&Error));
	TestEqual(TEXT("item/item swap puts the target whole stack in Backpack"), ItemState.Inventory.FindRef(SandId), 7);
	TestEqual(TEXT("item/item swap puts the source whole stack in Warehouse"),
		ItemState.DesktopInventory.WarehouseItems.FindRef(StoneId), 11);
	TestEqual(TEXT("item/item swap updates enhancement mirror from Backpack only"), ItemState.EnhancementMaterial, 0);
	TestEqual(TEXT("item/item swap updates refinement mirror from Backpack only"),
		ItemState.EquipmentCollection.RefinementSand, 7);
	TestEqual(TEXT("item/item swap preserves the exact Backpack physical key"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			ItemState, EGameXXKDesktopItemContainer::Backpack, InitialStoneSlot), SandEntry);
	TestEqual(TEXT("item/item swap preserves the exact Warehouse physical key"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			ItemState, EGameXXKDesktopItemContainer::Warehouse, 21), StoneEntry);

	FGameXXKRuntimeState EquipmentState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("equipment cross-swap fixture normalizes"),
		FGameXXKDesktopInventoryRules::Normalize(EquipmentState, &Error));
	TArray<FName> EquipmentIds;
	for (const FName InstanceId : EquipmentState.EquipmentCollection.WarehouseInstanceIds)
	{
		if (!InstanceId.IsNone())
		{
			EquipmentIds.Add(InstanceId);
			if (EquipmentIds.Num() == 2)
			{
				break;
			}
		}
	}
	if (!TestEqual(TEXT("equipment swap fixture has two unequipped instances"), EquipmentIds.Num(), 2))
	{
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey FirstEquipmentEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentIds[0]);
	const FGameXXKDesktopInventoryEntryKey SecondEquipmentEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentIds[1]);
	const int32 FirstEquipmentBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		EquipmentState, EGameXXKDesktopItemContainer::Backpack, FirstEquipmentEntry);
	const int32 SecondEquipmentBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		EquipmentState, EGameXXKDesktopItemContainer::Backpack, SecondEquipmentEntry);
	TestTrue(TEXT("equipment fixture moves one instance to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			EquipmentState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, FirstEquipmentBackpackSlot,
				EGameXXKDesktopItemContainer::Warehouse, 31),
			&Error));
	TestTrue(TEXT("equipment/equipment swaps across authoritative partitions"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			EquipmentState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, SecondEquipmentBackpackSlot,
				EGameXXKDesktopItemContainer::Warehouse, 31),
			&Error));
	TestTrue(TEXT("equipment/equipment swap partitions the incoming instance into Warehouse"),
		EquipmentState.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(EquipmentIds[1]));
	TestFalse(TEXT("equipment/equipment swap partitions the displaced instance into Backpack"),
		EquipmentState.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(EquipmentIds[0]));
	TestEqual(TEXT("equipment/equipment swap returns displaced key to the exact Backpack cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			EquipmentState, EGameXXKDesktopItemContainer::Backpack, SecondEquipmentBackpackSlot),
		FirstEquipmentEntry);
	TestEqual(TEXT("equipment/equipment swap places incoming key in the exact Warehouse cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			EquipmentState, EGameXXKDesktopItemContainer::Warehouse, 31),
		SecondEquipmentEntry);

	FGameXXKRuntimeState MixedState = UGameXXKMVPRules::CreateNewGame();
	const FName CrossItemId(TEXT("Item.Test.CrossStack"));
	MixedState.Inventory.Add(CrossItemId, 37);
	TestTrue(TEXT("equipment/item fixture normalizes"),
		FGameXXKDesktopInventoryRules::Normalize(MixedState, &Error));
	const FName MixedEquipmentId = MixedState.EquipmentCollection.WarehouseInstanceIds[0];
	const FGameXXKDesktopInventoryEntryKey MixedEquipmentEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(MixedEquipmentId);
	const FGameXXKDesktopInventoryEntryKey MixedItemEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(CrossItemId);
	const int32 MixedEquipmentSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		MixedState, EGameXXKDesktopItemContainer::Backpack, MixedEquipmentEntry);
	const int32 MixedItemSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		MixedState, EGameXXKDesktopItemContainer::Backpack, MixedItemEntry);
	TestTrue(TEXT("mixed fixture moves equipment to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			MixedState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, MixedEquipmentSlot,
				EGameXXKDesktopItemContainer::Warehouse, 40),
			&Error));
	TestTrue(TEXT("equipment and whole item stack swap across containers"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			MixedState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, MixedItemSlot,
				EGameXXKDesktopItemContainer::Warehouse, 40),
			&Error));
	TestEqual(TEXT("mixed swap preserves the whole item quantity in Warehouse"),
		MixedState.DesktopInventory.WarehouseItems.FindRef(CrossItemId), 37);
	TestEqual(TEXT("mixed swap returns equipment to the exact Backpack cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			MixedState, EGameXXKDesktopItemContainer::Backpack, MixedItemSlot),
		MixedEquipmentEntry);
	TestFalse(TEXT("mixed swap removes returned equipment from the Warehouse partition"),
		MixedState.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(MixedEquipmentId));
	TestTrue(TEXT("the reverse equipment/item swap is also atomic"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			MixedState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, MixedItemSlot,
				EGameXXKDesktopItemContainer::Warehouse, 40),
			&Error));
	TestEqual(TEXT("reverse mixed swap returns the item to its exact Backpack cell"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			MixedState, EGameXXKDesktopItemContainer::Backpack, MixedItemSlot),
		MixedItemEntry);
	TestTrue(TEXT("reverse mixed swap restores the equipment Warehouse partition"),
		MixedState.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(MixedEquipmentId));
	TestEqual(TEXT("reverse mixed swap preserves the whole Backpack item quantity"),
		MixedState.Inventory.FindRef(CrossItemId), 37);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryLegacyOverflowBackfillTest,
	"GameXXK.Data.DesktopInventory.LegacyOverflowBackfill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryLegacyOverflowBackfillTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!BuildLegacyOverflowState(*this, State))
	{
		return false;
	}
	const int32 SourceSlot = 0;
	const int32 WarehouseTargetSlot = 37;
	const FGameXXKDesktopInventoryEntryKey SourceEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot);
	if (!TestTrue(TEXT("overflow move source is a visible equipment instance"),
		SourceEntry.IsValid() && SourceEntry.bEquipmentInstance))
	{
		return false;
	}
	FName HiddenInstanceId = NAME_None;
	for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
	{
		if (FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId)) == INDEX_NONE)
		{
			HiddenInstanceId = InstanceId;
			break;
		}
	}
	if (!TestFalse(TEXT("overflow fixture has one hidden equipment instance"), HiddenInstanceId.IsNone()))
	{
		return false;
	}

	FString Error;
	TestTrue(TEXT("cross-container move succeeds from a visible legacy-overflow cell"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			State,
			MakeMoveRequest(
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlot,
				EGameXXKDesktopItemContainer::Warehouse,
				WarehouseTargetSlot,
				true,
				SourceEntry),
			&Error));
	TestEqual(TEXT("moved overflow entry stays in the exact Warehouse target"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, WarehouseTargetSlot),
		SourceEntry);
	TestEqual(TEXT("freed overflow Backpack source is immediately backfilled"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot),
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(HiddenInstanceId));
	TestEqual(TEXT("all 200 now-projectable Backpack entries are visible"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			State, EGameXXKDesktopItemContainer::Backpack),
		FGameXXKDesktopInventoryRules::BackpackCapacity);
	for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
	{
		const FGameXXKDesktopInventoryEntryKey Entry =
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId);
		const int32 ProjectionCount =
			(FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Backpack, Entry) != INDEX_NONE ? 1 : 0)
			+ (FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Warehouse, Entry) != INDEX_NONE ? 1 : 0);
		TestEqual(TEXT("every overflow equipment instance projects exactly once after movement"),
			ProjectionCount, 1);
	}
	TestTrue(TEXT("backfilled overflow move remains desktop-valid"),
		FGameXXKDesktopInventoryRules::Validate(State, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryLegacyCompatibilityMirrorTest,
	"GameXXK.Data.DesktopInventory.LegacyCompatibilityMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryLegacyCompatibilityMirrorTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("legacy mirror fixture starts normalized"),
		FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	const int32 BackpackOccupiedBefore = FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
		State, EGameXXKDesktopItemContainer::Backpack);
	const int32 WarehouseOccupiedBefore = FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
		State, EGameXXKDesktopItemContainer::Warehouse);
	const FName BackpackMirrorId(TEXT("Item.WoodenSword"));
	const FName WarehouseMirrorId(TEXT("Item.ClothArmor"));
	State.Inventory.Add(BackpackMirrorId, 3);
	State.DesktopInventory.WarehouseItems.Add(WarehouseMirrorId, 2);
	TestTrue(TEXT("legacy catalog compatibility counts normalize without becoming item stacks"),
		FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	TestEqual(TEXT("Backpack legacy compatibility count remains API-visible"),
		State.Inventory.FindRef(BackpackMirrorId), 3);
	TestEqual(TEXT("Warehouse legacy compatibility count remains API-visible"),
		State.DesktopInventory.WarehouseItems.FindRef(WarehouseMirrorId), 2);
	TestEqual(TEXT("Backpack legacy compatibility mirror consumes no physical capacity"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			State, EGameXXKDesktopItemContainer::Backpack),
		BackpackOccupiedBefore);
	TestEqual(TEXT("Warehouse legacy compatibility mirror consumes no physical capacity"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			State, EGameXXKDesktopItemContainer::Warehouse),
		WarehouseOccupiedBefore);
	TestEqual(TEXT("Backpack legacy equipment mirror has no item-stack cell"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			FGameXXKDesktopInventoryRules::MakeItemEntry(BackpackMirrorId)),
		INDEX_NONE);
	TestEqual(TEXT("Warehouse legacy equipment mirror has no item-stack cell"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Warehouse,
			FGameXXKDesktopInventoryRules::MakeItemEntry(WarehouseMirrorId)),
		INDEX_NONE);
	TestTrue(TEXT("non-physical compatibility mirrors remain desktop-valid"),
		FGameXXKDesktopInventoryRules::Validate(State, &Error));

	const TArray<uint8> BeforeLockRequest = SerializeDesktopInventoryRuntimeState(State);
	TestFalse(TEXT("non-physical legacy compatibility mirror cannot be item-locked"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State,
			FGameXXKDesktopInventoryRules::MakeItemEntry(BackpackMirrorId),
			true,
			&Error));
	TestEqual(TEXT("legacy compatibility lock rejection is byte-identical"),
		SerializeDesktopInventoryRuntimeState(State), BeforeLockRequest);
	State.DesktopInventory.LockedItemIds.Add(BackpackMirrorId);
	TestFalse(TEXT("validation rejects a persisted item lock on a compatibility mirror"),
		FGameXXKDesktopInventoryRules::Validate(State, &Error));
	TestTrue(TEXT("normalization removes a persisted compatibility-mirror item lock"),
		FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	TestFalse(TEXT("compatibility-mirror item lock stays removed"),
		State.DesktopInventory.LockedItemIds.Contains(BackpackMirrorId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryMoveOrSwapRollbackTest,
	"GameXXK.Data.DesktopInventory.MoveOrSwapRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryMoveOrSwapRollbackTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("rollback fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	const int32 SourceSlot = FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
		State, EGameXXKDesktopItemContainer::Backpack);
	const int32 EmptySlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		State, EGameXXKDesktopItemContainer::Backpack);
	const TArray<uint8> BeforeInvalidIndex = SerializeDesktopInventoryRuntimeState(State);
	TestFalse(TEXT("invalid source index rejects"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			State,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, INDEX_NONE,
				EGameXXKDesktopItemContainer::Backpack, EmptySlot),
			&Error));
	TestEqual(TEXT("invalid index rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(State), BeforeInvalidIndex);

	FGameXXKDesktopInventoryMoveRequest InvalidFromContainer = MakeMoveRequest(
		EGameXXKDesktopItemContainer::Backpack,
		SourceSlot,
		EGameXXKDesktopItemContainer::Backpack,
		EmptySlot,
		true,
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot));
	InvalidFromContainer.FromContainer = static_cast<EGameXXKDesktopItemContainer>(255);
	TestFalse(TEXT("invalid source-container enum rejects explicitly"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(State, InvalidFromContainer, &Error));
	TestTrue(TEXT("invalid source-container enum reports a container error"),
		Error.Contains(TEXT("container")));
	TestEqual(TEXT("invalid source-container rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(State), BeforeInvalidIndex);

	FGameXXKDesktopInventoryMoveRequest InvalidTargetContainer = MakeMoveRequest(
		EGameXXKDesktopItemContainer::Backpack,
		SourceSlot,
		EGameXXKDesktopItemContainer::Backpack,
		EmptySlot,
		true,
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot));
	InvalidTargetContainer.ToContainer = static_cast<EGameXXKDesktopItemContainer>(255);
	TestFalse(TEXT("invalid target-container enum rejects explicitly"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(State, InvalidTargetContainer, &Error));
	TestTrue(TEXT("invalid target-container enum reports a container error"),
		Error.Contains(TEXT("container")));
	TestEqual(TEXT("invalid target-container rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(State), BeforeInvalidIndex);

	FGameXXKRuntimeState AbaState = State;
	TArray<int32> AbaOccupiedSlots;
	for (int32 SlotIndex = 0; SlotIndex < AbaState.DesktopInventory.BackpackSlots.Num(); ++SlotIndex)
	{
		if (AbaState.DesktopInventory.BackpackSlots[SlotIndex].IsValid())
		{
			AbaOccupiedSlots.Add(SlotIndex);
			if (AbaOccupiedSlots.Num() == 2)
			{
				break;
			}
		}
	}
	if (!TestEqual(TEXT("ABA fixture has two swappable physical entries"), AbaOccupiedSlots.Num(), 2))
	{
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey ExpectedAbaEntry =
		AbaState.DesktopInventory.BackpackSlots[AbaOccupiedSlots[0]];
	Swap(
		AbaState.DesktopInventory.BackpackSlots[AbaOccupiedSlots[0]],
		AbaState.DesktopInventory.BackpackSlots[AbaOccupiedSlots[1]]);
	const TArray<uint8> BeforeAbaRejection = SerializeDesktopInventoryRuntimeState(AbaState);
	TestFalse(TEXT("move rejects when the physical source was replaced after carry began"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			AbaState,
			MakeMoveRequest(
				EGameXXKDesktopItemContainer::Backpack,
				AbaOccupiedSlots[0],
				EGameXXKDesktopItemContainer::Backpack,
				EmptySlot,
				true,
				ExpectedAbaEntry),
			&Error));
	TestTrue(TEXT("move ABA rejection reports the changed expected source"),
		Error.Contains(TEXT("expected")));
	TestEqual(TEXT("move ABA rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(AbaState), BeforeAbaRejection);

	FGameXXKRuntimeState StaleState = State;
	const FName StaleInstanceId(TEXT("Equipment.Instance.StalePhysicalSource"));
	StaleState.EquipmentCollection.WarehouseInstanceIds.Add(StaleInstanceId);
	TestTrue(TEXT("desktop-only normalization can expose the stale logical source for rejection"),
		FGameXXKDesktopInventoryRules::Normalize(StaleState, &Error));
	const int32 StaleSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		StaleState,
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(StaleInstanceId));
	const TArray<uint8> BeforeStaleSource = SerializeDesktopInventoryRuntimeState(StaleState);
	TestFalse(TEXT("stale physical equipment source rejects"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			StaleState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, StaleSlot,
				EGameXXKDesktopItemContainer::Backpack,
				FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
					StaleState, EGameXXKDesktopItemContainer::Backpack)),
			&Error));
	TestEqual(TEXT("stale source rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(StaleState), BeforeStaleSource);

	FGameXXKRuntimeState FullState = UGameXXKMVPRules::CreateNewGame();
	for (int32 Index = 0; Index <= FGameXXKDesktopInventoryRules::BackpackCapacity; ++Index)
	{
		FullState.Inventory.Add(
			FName(*FString::Printf(TEXT("Item.Test.Full.%03d"), Index)),
			1);
	}
	const TArray<uint8> BeforeFullFailure = SerializeDesktopInventoryRuntimeState(FullState);
	TestFalse(TEXT("over-capacity candidate rejects before movement"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			FullState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, 0,
				EGameXXKDesktopItemContainer::Warehouse, 0),
			&Error));
	TestTrue(TEXT("over-capacity failure is causal"), Error.Contains(TEXT("over capacity")));
	TestEqual(TEXT("over-capacity rejection preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(FullState), BeforeFullFailure);

	FGameXXKRuntimeState ValidationFailureState = UGameXXKMVPRules::CreateNewGame();
	const FName DuplicatePartitionItem = UGameXXKMVPRules::ItemEnhancementStone();
	ValidationFailureState.DesktopInventory.WarehouseItems.Add(DuplicatePartitionItem, 3);
	ValidationFailureState.DesktopInventory.BackpackSlots.Reset();
	ValidationFailureState.DesktopInventory.WarehouseSlots.Reset();
	const TArray<uint8> BeforeValidationFailure = SerializeDesktopInventoryRuntimeState(ValidationFailureState);
	TestFalse(TEXT("candidate post-normalization Validate failure rejects the request"),
		FGameXXKDesktopInventoryRules::MoveOrSwap(
			ValidationFailureState,
			MakeMoveRequest(EGameXXKDesktopItemContainer::Backpack, SourceSlot,
				EGameXXKDesktopItemContainer::Warehouse, 0),
			&Error));
	TestTrue(TEXT("post-normalization failure comes from invalid item partition validation"),
		Error.Contains(TEXT("item partition")));
	TestEqual(TEXT("post-normalization Validate rollback preserves every serialized byte"),
		SerializeDesktopInventoryRuntimeState(ValidationFailureState), BeforeValidationFailure);
	return true;
}

#endif
