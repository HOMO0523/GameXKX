#include "GameXXKDesktopInventoryRules.h"

#include "GameXXKEquipmentRules.h"

namespace
{
	void SetError(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
	}

	int32 CapacityFor(const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? FGameXXKDesktopInventoryRules::WarehouseCapacity
			: FGameXXKDesktopInventoryRules::BackpackCapacity;
	}

	TArray<FGameXXKDesktopInventoryEntryKey>& SlotsFor(
		FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseSlots
			: State.DesktopInventory.BackpackSlots;
	}

	const TArray<FGameXXKDesktopInventoryEntryKey>& SlotsFor(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseSlots
			: State.DesktopInventory.BackpackSlots;
	}

	TSet<FGameXXKDesktopInventoryEntryKey> BuildExpectedEntries(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container)
	{
		TSet<FGameXXKDesktopInventoryEntryKey> Result;
		const TSet<FName> StoredEquipment(State.DesktopInventory.WarehouseEquipmentInstanceIds);
		if (Container == EGameXXKDesktopItemContainer::Backpack)
		{
			for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
			{
				if (!InstanceId.IsNone() && !StoredEquipment.Contains(InstanceId))
				{
					Result.Add(FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
				}
			}
			for (const TPair<FName, int32>& Pair : State.Inventory)
			{
				if (!Pair.Key.IsNone() && Pair.Value > 0)
				{
					Result.Add(FGameXXKDesktopInventoryRules::MakeItemEntry(Pair.Key));
				}
			}
		}
		else
		{
			for (const FName InstanceId : State.DesktopInventory.WarehouseEquipmentInstanceIds)
			{
				if (!InstanceId.IsNone())
				{
					Result.Add(FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
				}
			}
			for (const TPair<FName, int32>& Pair : State.DesktopInventory.WarehouseItems)
			{
				if (!Pair.Key.IsNone() && Pair.Value > 0)
				{
					Result.Add(FGameXXKDesktopInventoryRules::MakeItemEntry(Pair.Key));
				}
			}
		}
		return Result;
	}

	bool NormalizeSlots(
		TArray<FGameXXKDesktopInventoryEntryKey>& Slots,
		const TSet<FGameXXKDesktopInventoryEntryKey>& Expected,
		const int32 Capacity,
		const bool bAllowLegacyOverflow,
		FString* OutError)
	{
		if (Expected.Num() > Capacity && !bAllowLegacyOverflow)
		{
			SetError(OutError, TEXT("Desktop inventory container is over capacity."));
			return false;
		}
		Slots.SetNum(Capacity);
		TSet<FGameXXKDesktopInventoryEntryKey> Seen;
		for (FGameXXKDesktopInventoryEntryKey& Slot : Slots)
		{
			if (!Slot.IsValid() || !Expected.Contains(Slot) || Seen.Contains(Slot))
			{
				Slot = FGameXXKDesktopInventoryEntryKey();
				continue;
			}
			Seen.Add(Slot);
		}

		TArray<FGameXXKDesktopInventoryEntryKey> Missing = Expected.Difference(Seen).Array();
		Missing.Sort([](const FGameXXKDesktopInventoryEntryKey& Left, const FGameXXKDesktopInventoryEntryKey& Right)
		{
			if (Left.bEquipmentInstance != Right.bEquipmentInstance)
			{
				return Left.bEquipmentInstance;
			}
			return Left.EntryId.LexicalLess(Right.EntryId);
		});
		for (const FGameXXKDesktopInventoryEntryKey& Entry : Missing)
		{
			const int32 EmptyIndex = Slots.IndexOfByPredicate([](const FGameXXKDesktopInventoryEntryKey& Candidate)
			{
				return !Candidate.IsValid();
			});
			if (EmptyIndex == INDEX_NONE)
			{
				if (bAllowLegacyOverflow)
				{
					// Pre-v7 saves may contain more than the modern warehouse cap.
					// Preserve every authoritative equipment instance while exposing
					// only the deterministic first page-capacity subset. The legacy
					// overflow flag already blocks new acquisition until space exists.
					break;
				}
				SetError(OutError, TEXT("Desktop inventory has no empty slot for a new entry."));
				return false;
			}
			Slots[EmptyIndex] = Entry;
		}
		return true;
	}

	void SynchronizeLegacyMaterialMirrors(FGameXXKRuntimeState& State)
	{
		State.EnhancementMaterial = FMath::Max(
			0,
			State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()));
		State.EquipmentCollection.RefinementSand = FMath::Max(
			0,
			State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()));
	}
}

FGameXXKDesktopInventoryEntryKey FGameXXKDesktopInventoryRules::MakeItemEntry(const FName ItemId)
{
	FGameXXKDesktopInventoryEntryKey Result;
	Result.EntryId = ItemId;
	return Result;
}

FGameXXKDesktopInventoryEntryKey FGameXXKDesktopInventoryRules::MakeEquipmentEntry(const FName InstanceId)
{
	FGameXXKDesktopInventoryEntryKey Result;
	Result.bEquipmentInstance = true;
	Result.EntryId = InstanceId;
	return Result;
}

bool FGameXXKDesktopInventoryRules::Normalize(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	SetError(OutError, FString());
	TSet<FName> SeenStorageEquipment;
	InOutState.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveAll(
		[&InOutState, &SeenStorageEquipment](const FName InstanceId)
		{
			const bool bInvalid = InstanceId.IsNone()
				|| SeenStorageEquipment.Contains(InstanceId)
				|| !InOutState.EquipmentCollection.WarehouseInstanceIds.Contains(InstanceId);
			if (!bInvalid)
			{
				SeenStorageEquipment.Add(InstanceId);
			}
			return bInvalid;
		});

	TArray<FName> InvalidWarehouseItems;
	for (const TPair<FName, int32>& Pair : InOutState.DesktopInventory.WarehouseItems)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0)
		{
			InvalidWarehouseItems.Add(Pair.Key);
		}
	}
	for (const FName ItemId : InvalidWarehouseItems)
	{
		InOutState.DesktopInventory.WarehouseItems.Remove(ItemId);
	}

	const TSet<FGameXXKDesktopInventoryEntryKey> BackpackExpected = BuildExpectedEntries(
		InOutState,
		EGameXXKDesktopItemContainer::Backpack);
	const TSet<FGameXXKDesktopInventoryEntryKey> WarehouseExpected = BuildExpectedEntries(
		InOutState,
		EGameXXKDesktopItemContainer::Warehouse);
	if (!NormalizeSlots(
		InOutState.DesktopInventory.BackpackSlots,
		BackpackExpected,
		BackpackCapacity,
		InOutState.EquipmentCollection.bLegacyWarehouseOverflow,
		OutError)
		|| !NormalizeSlots(
			InOutState.DesktopInventory.WarehouseSlots,
			WarehouseExpected,
			WarehouseCapacity,
			false,
			OutError))
	{
		return false;
	}
	SynchronizeLegacyMaterialMirrors(InOutState);
	return Validate(InOutState, OutError);
}

bool FGameXXKDesktopInventoryRules::Validate(const FGameXXKRuntimeState& State, FString* OutError)
{
	SetError(OutError, FString());
	TSet<FName> SeenStorageEquipment;
	for (const FName InstanceId : State.DesktopInventory.WarehouseEquipmentInstanceIds)
	{
		if (InstanceId.IsNone()
			|| SeenStorageEquipment.Contains(InstanceId)
			|| !State.EquipmentCollection.WarehouseInstanceIds.Contains(InstanceId))
		{
			SetError(OutError, TEXT("Desktop warehouse equipment partition is invalid."));
			return false;
		}
		SeenStorageEquipment.Add(InstanceId);
	}
	for (const TPair<FName, int32>& Pair : State.DesktopInventory.WarehouseItems)
	{
		if (Pair.Key.IsNone() || Pair.Value <= 0 || State.Inventory.FindRef(Pair.Key) > 0)
		{
			SetError(OutError, TEXT("Desktop warehouse item partition is invalid."));
			return false;
		}
	}

	TSet<FGameXXKDesktopInventoryEntryKey> AcrossContainers;
	for (const EGameXXKDesktopItemContainer Container : {
		EGameXXKDesktopItemContainer::Backpack,
		EGameXXKDesktopItemContainer::Warehouse})
	{
		const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
		if (Slots.Num() > CapacityFor(Container))
		{
			SetError(OutError, TEXT("Desktop inventory slot array exceeds capacity."));
			return false;
		}
		const TSet<FGameXXKDesktopInventoryEntryKey> Expected = BuildExpectedEntries(State, Container);
		for (const FGameXXKDesktopInventoryEntryKey& Entry : Slots)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			if (!Expected.Contains(Entry) || AcrossContainers.Contains(Entry))
			{
				SetError(OutError, TEXT("Desktop inventory contains a stale or duplicated physical entry."));
				return false;
			}
			AcrossContainers.Add(Entry);
		}
	}
	return true;
}

bool FGameXXKDesktopInventoryRules::MoveEntry(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKDesktopItemContainer FromContainer,
	const int32 FromSlotIndex,
	const EGameXXKDesktopItemContainer ToContainer,
	const int32 ToSlotIndex,
	FString* OutError)
{
	SetError(OutError, FString());
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Normalize(Candidate, OutError)
		|| FromSlotIndex < 0 || FromSlotIndex >= CapacityFor(FromContainer)
		|| ToSlotIndex < 0 || ToSlotIndex >= CapacityFor(ToContainer))
	{
		SetError(OutError, TEXT("Desktop inventory slot index is invalid."));
		return false;
	}
	TArray<FGameXXKDesktopInventoryEntryKey>& FromSlots = SlotsFor(Candidate, FromContainer);
	TArray<FGameXXKDesktopInventoryEntryKey>& ToSlots = SlotsFor(Candidate, ToContainer);
	const FGameXXKDesktopInventoryEntryKey Entry = FromSlots[FromSlotIndex];
	if (!Entry.IsValid())
	{
		SetError(OutError, TEXT("Desktop inventory source slot is empty."));
		return false;
	}
	if (FromContainer == ToContainer && FromSlotIndex == ToSlotIndex)
	{
		return true;
	}
	if (ToSlots[ToSlotIndex].IsValid())
	{
		SetError(OutError, TEXT("Desktop inventory destination slot is occupied."));
		return false;
	}

	if (FromContainer != ToContainer)
	{
		if (Entry.bEquipmentInstance)
		{
			if (!Candidate.EquipmentCollection.WarehouseInstanceIds.Contains(Entry.EntryId))
			{
				SetError(OutError, TEXT("Equipment instance is no longer unequipped."));
				return false;
			}
			if (ToContainer == EGameXXKDesktopItemContainer::Warehouse)
			{
				Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.AddUnique(Entry.EntryId);
			}
			else
			{
				Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveSingle(Entry.EntryId);
			}
		}
		else
		{
			TMap<FName, int32>& SourceItems = FromContainer == EGameXXKDesktopItemContainer::Warehouse
				? Candidate.DesktopInventory.WarehouseItems
				: Candidate.Inventory;
			TMap<FName, int32>& DestinationItems = ToContainer == EGameXXKDesktopItemContainer::Warehouse
				? Candidate.DesktopInventory.WarehouseItems
				: Candidate.Inventory;
			const int32 Quantity = SourceItems.FindRef(Entry.EntryId);
			if (Quantity <= 0)
			{
				SetError(OutError, TEXT("Item stack is no longer available in its source container."));
				return false;
			}
			SourceItems.Remove(Entry.EntryId);
			DestinationItems.FindOrAdd(Entry.EntryId) += Quantity;
		}
	}

	FromSlots[FromSlotIndex] = FGameXXKDesktopInventoryEntryKey();
	ToSlots[ToSlotIndex] = Entry;
	SynchronizeLegacyMaterialMirrors(Candidate);
	if (!Validate(Candidate, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
	return true;
}

FGameXXKDesktopInventoryEntryKey FGameXXKDesktopInventoryRules::GetEntryAt(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container,
	const int32 SlotIndex)
{
	const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : FGameXXKDesktopInventoryEntryKey();
}

int32 FGameXXKDesktopInventoryRules::FindEntrySlot(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container,
	const FGameXXKDesktopInventoryEntryKey& Entry)
{
	return Entry.IsValid() ? SlotsFor(State, Container).IndexOfByKey(Entry) : INDEX_NONE;
}

int32 FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container)
{
	const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
	for (int32 Index = 0; Index < CapacityFor(Container); ++Index)
	{
		if (!Slots.IsValidIndex(Index) || !Slots[Index].IsValid())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container)
{
	int32 Count = 0;
	for (const FGameXXKDesktopInventoryEntryKey& Entry : SlotsFor(State, Container))
	{
		Count += Entry.IsValid() ? 1 : 0;
	}
	return Count;
}

int32 FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container)
{
	const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
	for (int32 Index = Slots.Num() - 1; Index >= 0; --Index)
	{
		if (Slots[Index].IsValid())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
