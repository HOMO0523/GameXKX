#include "GameXXKDesktopInventoryRules.h"

#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKTalentRules.h"

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

	int32 LogicalCapacityFor(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? FGameXXKTalentRules::GetUnlockedWarehouseCapacity(State)
			: FGameXXKTalentRules::GetUnlockedBackpackCapacity(State);
	}

	bool IsValidContainer(const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Backpack
			|| Container == EGameXXKDesktopItemContainer::Warehouse;
	}

	bool IsOppositeContainerPair(
		const EGameXXKDesktopItemContainer From,
		const EGameXXKDesktopItemContainer To)
	{
		return IsValidContainer(From)
			&& IsValidContainer(To)
			&& From != To;
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

	bool HasEquipmentInstanceExactlyOnce(
		const FGameXXKRuntimeState& State,
		const FName InstanceId)
	{
		if (InstanceId.IsNone())
		{
			return false;
		}
		int32 MatchCount = 0;
		for (const FGameXXKEquipmentInstance& Instance : State.EquipmentCollection.EquipmentInstances)
		{
			if (Instance.InstanceId == InstanceId && ++MatchCount > 1)
			{
				return false;
			}
		}
		return MatchCount == 1;
	}

	bool HasPositiveItemStack(const FGameXXKRuntimeState& State, const FName ItemId)
	{
		return !ItemId.IsNone()
			&& !FGameXXKEquipmentCatalog::FindDefinition(ItemId)
			&& (State.Inventory.FindRef(ItemId) > 0
				|| State.DesktopInventory.WarehouseItems.FindRef(ItemId) > 0);
	}

	bool IsLockTargetValid(
		const FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry)
	{
		return Entry.IsValid()
			&& (Entry.bEquipmentInstance
				? HasEquipmentInstanceExactlyOnce(State, Entry.EntryId)
				: HasPositiveItemStack(State, Entry.EntryId));
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
				if (!Pair.Key.IsNone()
					&& Pair.Value > 0
					&& !FGameXXKEquipmentCatalog::FindDefinition(Pair.Key))
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
				if (!Pair.Key.IsNone()
					&& Pair.Value > 0
					&& !FGameXXKEquipmentCatalog::FindDefinition(Pair.Key))
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
		const int32 PhysicalCapacity,
		const int32 LogicalCapacity,
		const bool bAllowLegacyOverflow,
		FString* OutError)
	{
		if (Expected.Num() > LogicalCapacity && !bAllowLegacyOverflow)
		{
			SetError(OutError, TEXT("Desktop inventory container is over capacity."));
			return false;
		}
		Slots.SetNum(PhysicalCapacity);
		TSet<FGameXXKDesktopInventoryEntryKey> Seen;
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			FGameXXKDesktopInventoryEntryKey& Slot = Slots[SlotIndex];
			if (SlotIndex >= LogicalCapacity)
			{
				if (Slot.IsValid() && Expected.Contains(Slot))
				{
					SetError(OutError, TEXT("Desktop inventory contains an authoritative entry in a locked talent slot."));
					return false;
				}
				Slot = FGameXXKDesktopInventoryEntryKey();
				continue;
			}
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
			int32 EmptyIndex = INDEX_NONE;
			for (int32 SlotIndex = 0; SlotIndex < LogicalCapacity; ++SlotIndex)
			{
				if (!Slots[SlotIndex].IsValid())
				{
					EmptyIndex = SlotIndex;
					break;
				}
			}
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

	bool IsAuthoritativeSourceEntry(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container,
		const FGameXXKDesktopInventoryEntryKey& Entry,
		FString* OutError)
	{
		if (!Entry.IsValid())
		{
			SetError(OutError, TEXT("Desktop inventory source slot is empty."));
			return false;
		}
		if (Entry.bEquipmentInstance)
		{
			const bool bInWarehousePartition =
				State.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(Entry.EntryId);
			if (!HasEquipmentInstanceExactlyOnce(State, Entry.EntryId)
				|| !State.EquipmentCollection.WarehouseInstanceIds.Contains(Entry.EntryId)
				|| bInWarehousePartition != (Container == EGameXXKDesktopItemContainer::Warehouse))
			{
				SetError(OutError, TEXT("Equipment instance is no longer an unequipped entry in its source container."));
				return false;
			}
			return true;
		}

		const TMap<FName, int32>& SourceItems =
			Container == EGameXXKDesktopItemContainer::Warehouse
				? State.DesktopInventory.WarehouseItems
				: State.Inventory;
		if (SourceItems.FindRef(Entry.EntryId) <= 0)
		{
			SetError(OutError, TEXT("Item stack is no longer available in its source container."));
			return false;
		}
		return true;
	}

	bool TransferEntryPartition(
		FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry,
		const EGameXXKDesktopItemContainer FromContainer,
		const EGameXXKDesktopItemContainer ToContainer,
		FString* OutError)
	{
		if (FromContainer == ToContainer)
		{
			return true;
		}
		if (Entry.bEquipmentInstance)
		{
			if (ToContainer == EGameXXKDesktopItemContainer::Warehouse)
			{
				State.DesktopInventory.WarehouseEquipmentInstanceIds.AddUnique(Entry.EntryId);
			}
			else
			{
				State.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveSingle(Entry.EntryId);
			}
			return true;
		}

		TMap<FName, int32>& SourceItems = FromContainer == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems
			: State.Inventory;
		TMap<FName, int32>& DestinationItems = ToContainer == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems
			: State.Inventory;
		const int32 Quantity = SourceItems.FindRef(Entry.EntryId);
		if (Quantity <= 0)
		{
			SetError(OutError, TEXT("Item stack is no longer available in its source container."));
			return false;
		}
		SourceItems.Remove(Entry.EntryId);
		DestinationItems.Remove(Entry.EntryId);
		DestinationItems.Add(Entry.EntryId, Quantity);
		return true;
	}

	bool ResolveWarehousePageRange(
		const FGameXXKRuntimeState& State,
		const int32 PageIndex,
		const int32 PageSize,
		int32& OutFirst,
		int32& OutLastExclusive)
	{
		OutFirst = INDEX_NONE;
		OutLastExclusive = INDEX_NONE;
		if (PageIndex < 0 || PageSize <= 0)
		{
			return false;
		}
		const int32 Capacity = LogicalCapacityFor(
			State, EGameXXKDesktopItemContainer::Warehouse);
		const int64 First64 = static_cast<int64>(PageIndex) * PageSize;
		if (First64 < 0 || First64 > MAX_int32)
		{
			return false;
		}
		OutFirst = static_cast<int32>(First64);
		OutLastExclusive = static_cast<int32>(FMath::Min<int64>(
			First64 + static_cast<int64>(PageSize),
			Capacity));
		return OutFirst < OutLastExclusive;
	}

	bool ResolveContainerRange(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container,
		const int32 WarehouseFirst,
		const int32 WarehouseLastExclusive,
		int32& OutFirst,
		int32& OutLastExclusive)
	{
		if (!IsValidContainer(Container))
		{
			return false;
		}
		if (Container == EGameXXKDesktopItemContainer::Warehouse)
		{
			OutFirst = WarehouseFirst;
			OutLastExclusive = WarehouseLastExclusive;
		}
		else
		{
			OutFirst = 0;
			OutLastExclusive = LogicalCapacityFor(State, Container);
		}
		return OutFirst >= 0 && OutFirst < OutLastExclusive;
	}

	int32 FindFirstEmptySlotInRange(
		const FGameXXKRuntimeState& State,
		const EGameXXKDesktopItemContainer Container,
		const int32 First,
		const int32 LastExclusive)
	{
		const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
		for (int32 SlotIndex = First; SlotIndex < LastExclusive; ++SlotIndex)
		{
			if (!Slots.IsValidIndex(SlotIndex) || !Slots[SlotIndex].IsValid())
			{
				return SlotIndex;
			}
		}
		return INDEX_NONE;
	}

	bool MergeItemStackAcrossContainers(
		FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry,
		const EGameXXKDesktopItemContainer From,
		const EGameXXKDesktopItemContainer To,
		FString* OutError)
	{
		if (!Entry.IsValid() || Entry.bEquipmentInstance || From == To)
		{
			SetError(OutError, TEXT("Desktop inventory stack merge request is invalid."));
			return false;
		}
		auto ItemStacksFor = [&State](const EGameXXKDesktopItemContainer Container) -> TMap<FName, int32>&
		{
			return Container == EGameXXKDesktopItemContainer::Warehouse
				? State.DesktopInventory.WarehouseItems
				: State.Inventory;
		};
		TMap<FName, int32>& Source = ItemStacksFor(From);
		TMap<FName, int32>& Destination = ItemStacksFor(To);
		const int32 SourceQuantity = Source.FindRef(Entry.EntryId);
		const int32 DestinationQuantity = Destination.FindRef(Entry.EntryId);
		if (SourceQuantity <= 0
			|| DestinationQuantity <= 0
			|| DestinationQuantity > MAX_int32 - SourceQuantity)
		{
			SetError(OutError, TEXT("Desktop inventory stack merge is invalid or overflows."));
			return false;
		}
		Source.Remove(Entry.EntryId);
		Destination.Add(Entry.EntryId, DestinationQuantity + SourceQuantity);
		return true;
	}

	bool ValidateBatchRequest(
		const FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryBatchTransferRequest& Request,
		int32& OutWarehouseFirst,
		int32& OutWarehouseLastExclusive,
		FString* OutError)
	{
		if (!IsOppositeContainerPair(Request.FromContainer, Request.ToContainer))
		{
			SetError(OutError, TEXT("Desktop inventory batch containers must be opposite."));
			return false;
		}
		if (!ResolveWarehousePageRange(
			State,
			Request.WarehousePageIndex,
			Request.WarehousePageSize,
			OutWarehouseFirst,
			OutWarehouseLastExclusive))
		{
			SetError(OutError, TEXT("Desktop inventory Warehouse page is invalid or locked."));
			return false;
		}
		return true;
	}

	bool MergeDuplicateSourceStacksBeforeNormalize(
		FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryBatchTransferRequest& Request,
		const int32 SourceFirst,
		const int32 SourceLastExclusive,
		int32& InOutMovedEntryCount,
		FString* OutError)
	{
		TArray<FGameXXKDesktopInventoryEntryKey>& SourceSlots = SlotsFor(
			State, Request.FromContainer);
		TMap<FName, int32>& DestinationItems =
			Request.ToContainer == EGameXXKDesktopItemContainer::Warehouse
				? State.DesktopInventory.WarehouseItems
				: State.Inventory;
		TSet<FGameXXKDesktopInventoryEntryKey> Seen;
		for (int32 SlotIndex = SourceFirst; SlotIndex < SourceLastExclusive; ++SlotIndex)
		{
			if (!SourceSlots.IsValidIndex(SlotIndex))
			{
				continue;
			}
			const FGameXXKDesktopInventoryEntryKey Entry = SourceSlots[SlotIndex];
			if (!Entry.IsValid()
				|| Entry.bEquipmentInstance
				|| Request.ExcludedEntries.Contains(Entry)
				|| Seen.Contains(Entry)
				|| DestinationItems.FindRef(Entry.EntryId) <= 0)
			{
				continue;
			}
			Seen.Add(Entry);
			if (!MergeItemStackAcrossContainers(
				State, Entry, Request.FromContainer, Request.ToContainer, OutError))
			{
				return false;
			}
			SourceSlots[SlotIndex] = FGameXXKDesktopInventoryEntryKey();
			++InOutMovedEntryCount;
		}
		return true;
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

bool FGameXXKDesktopInventoryRules::IsEntryLocked(
	const FGameXXKRuntimeState& State,
	const FGameXXKDesktopInventoryEntryKey& Entry)
{
	if (!Entry.IsValid())
	{
		return false;
	}
	return Entry.bEquipmentInstance
		? State.DesktopInventory.LockedEquipmentInstanceIds.Contains(Entry.EntryId)
		: State.DesktopInventory.LockedItemIds.Contains(Entry.EntryId);
}

bool FGameXXKDesktopInventoryRules::SetEntryLocked(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKDesktopInventoryEntryKey& Entry,
	const bool bLocked,
	FString* OutError)
{
	SetError(OutError, FString());
	if (!IsLockTargetValid(InOutState, Entry))
	{
		SetError(OutError, TEXT("Desktop inventory lock target is empty or stale."));
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	TSet<FName>& LockedIds = Entry.bEquipmentInstance
		? Candidate.DesktopInventory.LockedEquipmentInstanceIds
		: Candidate.DesktopInventory.LockedItemIds;
	if (bLocked)
	{
		LockedIds.Add(Entry.EntryId);
	}
	else
	{
		LockedIds.Remove(Entry.EntryId);
	}
	if (!Validate(Candidate, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
	return true;
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

	for (auto Iterator = InOutState.DesktopInventory.LockedEquipmentInstanceIds.CreateIterator(); Iterator; ++Iterator)
	{
		if (!HasEquipmentInstanceExactlyOnce(InOutState, *Iterator))
		{
			Iterator.RemoveCurrent();
		}
	}
	for (auto Iterator = InOutState.DesktopInventory.LockedItemIds.CreateIterator(); Iterator; ++Iterator)
	{
		if (!HasPositiveItemStack(InOutState, *Iterator))
		{
			Iterator.RemoveCurrent();
		}
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
		LogicalCapacityFor(InOutState, EGameXXKDesktopItemContainer::Backpack),
		InOutState.EquipmentCollection.bLegacyWarehouseOverflow,
		OutError)
		|| !NormalizeSlots(
			InOutState.DesktopInventory.WarehouseSlots,
			WarehouseExpected,
			WarehouseCapacity,
			LogicalCapacityFor(InOutState, EGameXXKDesktopItemContainer::Warehouse),
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
	for (const FName InstanceId : State.DesktopInventory.LockedEquipmentInstanceIds)
	{
		if (!HasEquipmentInstanceExactlyOnce(State, InstanceId))
		{
			SetError(OutError, TEXT("Desktop inventory contains a stale equipment lock."));
			return false;
		}
	}
	for (const FName ItemId : State.DesktopInventory.LockedItemIds)
	{
		if (!HasPositiveItemStack(State, ItemId))
		{
			SetError(OutError, TEXT("Desktop inventory contains a stale item lock."));
			return false;
		}
	}

	TSet<FGameXXKDesktopInventoryEntryKey> AcrossContainers;
	for (const EGameXXKDesktopItemContainer Container : {
		EGameXXKDesktopItemContainer::Backpack,
		EGameXXKDesktopItemContainer::Warehouse})
	{
		const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
		const int32 LogicalCapacity = LogicalCapacityFor(State, Container);
		if (Slots.Num() > CapacityFor(Container))
		{
			SetError(OutError, TEXT("Desktop inventory slot array exceeds capacity."));
			return false;
		}
		const TSet<FGameXXKDesktopInventoryEntryKey> Expected = BuildExpectedEntries(State, Container);
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			const FGameXXKDesktopInventoryEntryKey& Entry = Slots[SlotIndex];
			if (!Entry.IsValid())
			{
				continue;
			}
			if (SlotIndex >= LogicalCapacity)
			{
				SetError(OutError, TEXT("Desktop inventory contains an entry beyond unlocked talent capacity."));
				return false;
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

bool FGameXXKDesktopInventoryRules::MoveOrSwap(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKDesktopInventoryMoveRequest& Request,
	FString* OutError)
{
	SetError(OutError, FString());
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Normalize(Candidate, OutError))
	{
		return false;
	}
	if (!IsValidContainer(Request.FromContainer)
		|| !IsValidContainer(Request.ToContainer))
	{
		SetError(OutError, TEXT("Desktop inventory container is invalid."));
		return false;
	}
	if (Request.FromSlotIndex < 0
		|| Request.FromSlotIndex >= LogicalCapacityFor(Candidate, Request.FromContainer)
		|| Request.ToSlotIndex < 0
		|| Request.ToSlotIndex >= LogicalCapacityFor(Candidate, Request.ToContainer))
	{
		SetError(OutError, TEXT("Desktop inventory slot index is invalid."));
		return false;
	}
	TArray<FGameXXKDesktopInventoryEntryKey>& FromSlots = SlotsFor(Candidate, Request.FromContainer);
	TArray<FGameXXKDesktopInventoryEntryKey>& ToSlots = SlotsFor(Candidate, Request.ToContainer);
	const FGameXXKDesktopInventoryEntryKey SourceEntry = FromSlots[Request.FromSlotIndex];
	if (Request.ExpectedEntry.IsValid() && SourceEntry != Request.ExpectedEntry)
	{
		SetError(OutError, TEXT("Desktop inventory source no longer matches the expected carried entry."));
		return false;
	}
	if (!IsAuthoritativeSourceEntry(Candidate, Request.FromContainer, SourceEntry, OutError))
	{
		return false;
	}
	if (Request.FromContainer == Request.ToContainer
		&& Request.FromSlotIndex == Request.ToSlotIndex)
	{
		return true;
	}

	const FGameXXKDesktopInventoryEntryKey TargetEntry = ToSlots[Request.ToSlotIndex];
	if (TargetEntry.IsValid() && !Request.bAllowSwap)
	{
		SetError(OutError, TEXT("Desktop inventory destination slot is occupied."));
		return false;
	}
	if (TargetEntry.IsValid()
		&& !IsAuthoritativeSourceEntry(Candidate, Request.ToContainer, TargetEntry, OutError))
	{
		return false;
	}

	if (!TransferEntryPartition(
		Candidate,
		SourceEntry,
		Request.FromContainer,
		Request.ToContainer,
		OutError)
		|| (TargetEntry.IsValid()
			&& !TransferEntryPartition(
				Candidate,
				TargetEntry,
				Request.ToContainer,
				Request.FromContainer,
				OutError)))
	{
		return false;
	}

	FromSlots[Request.FromSlotIndex] = TargetEntry;
	ToSlots[Request.ToSlotIndex] = SourceEntry;
	SynchronizeLegacyMaterialMirrors(Candidate);
	if (!Normalize(Candidate, OutError))
	{
		return false;
	}
	if (GetEntryAt(Candidate, Request.ToContainer, Request.ToSlotIndex) != SourceEntry)
	{
		SetError(OutError, TEXT("Desktop inventory normalization changed the requested target entry."));
		return false;
	}
	if (TargetEntry.IsValid()
		&& GetEntryAt(Candidate, Request.FromContainer, Request.FromSlotIndex) != TargetEntry)
	{
		SetError(OutError, TEXT("Desktop inventory normalization changed the displaced source entry."));
		return false;
	}
	if (!Validate(Candidate, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
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
	FGameXXKDesktopInventoryMoveRequest Request;
	Request.FromContainer = FromContainer;
	Request.FromSlotIndex = FromSlotIndex;
	Request.ToContainer = ToContainer;
	Request.ToSlotIndex = ToSlotIndex;
	Request.bAllowSwap = false;
	return MoveOrSwap(InOutState, Request, OutError);
}

bool FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(
	const FGameXXKRuntimeState& State,
	const FGameXXKDesktopInventoryBatchTransferRequest& Request)
{
	int32 WarehouseFirst = INDEX_NONE;
	int32 WarehouseLastExclusive = INDEX_NONE;
	if (!ValidateBatchRequest(
		State, Request, WarehouseFirst, WarehouseLastExclusive, nullptr))
	{
		return false;
	}
	int32 RawSourceFirst = INDEX_NONE;
	int32 RawSourceLastExclusive = INDEX_NONE;
	if (!ResolveContainerRange(
		State,
		Request.FromContainer,
		WarehouseFirst,
		WarehouseLastExclusive,
		RawSourceFirst,
		RawSourceLastExclusive))
	{
		return false;
	}
	const TArray<FGameXXKDesktopInventoryEntryKey>& RawSourceSlots = SlotsFor(
		State, Request.FromContainer);
	const TMap<FName, int32>& RawDestinationItems =
		Request.ToContainer == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems
			: State.Inventory;
	for (int32 SlotIndex = RawSourceFirst; SlotIndex < RawSourceLastExclusive; ++SlotIndex)
	{
		if (!RawSourceSlots.IsValidIndex(SlotIndex))
		{
			continue;
		}
		const FGameXXKDesktopInventoryEntryKey& Entry = RawSourceSlots[SlotIndex];
		if (Entry.IsValid()
			&& !Entry.bEquipmentInstance
			&& !Request.ExcludedEntries.Contains(Entry)
			&& RawDestinationItems.FindRef(Entry.EntryId) > 0)
		{
			return true;
		}
	}

	FGameXXKRuntimeState Candidate = State;
	if (!Normalize(Candidate))
	{
		return false;
	}
	int32 SourceFirst = INDEX_NONE;
	int32 SourceLastExclusive = INDEX_NONE;
	int32 DestinationFirst = INDEX_NONE;
	int32 DestinationLastExclusive = INDEX_NONE;
	if (!ResolveContainerRange(
		Candidate,
		Request.FromContainer,
		WarehouseFirst,
		WarehouseLastExclusive,
		SourceFirst,
		SourceLastExclusive)
		|| !ResolveContainerRange(
			Candidate,
			Request.ToContainer,
			WarehouseFirst,
			WarehouseLastExclusive,
			DestinationFirst,
			DestinationLastExclusive)
		|| FindFirstEmptySlotInRange(
			Candidate,
			Request.ToContainer,
			DestinationFirst,
			DestinationLastExclusive) == INDEX_NONE)
	{
		return false;
	}
	const TArray<FGameXXKDesktopInventoryEntryKey>& SourceSlots = SlotsFor(
		Candidate, Request.FromContainer);
	for (int32 SlotIndex = SourceFirst; SlotIndex < SourceLastExclusive; ++SlotIndex)
	{
		if (SourceSlots.IsValidIndex(SlotIndex)
			&& SourceSlots[SlotIndex].IsValid()
			&& !Request.ExcludedEntries.Contains(SourceSlots[SlotIndex]))
		{
			return true;
		}
	}
	return false;
}

bool FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKDesktopInventoryBatchTransferRequest& Request,
	FGameXXKDesktopInventoryBatchTransferResult& OutResult)
{
	OutResult = FGameXXKDesktopInventoryBatchTransferResult();
	int32 WarehouseFirst = INDEX_NONE;
	int32 WarehouseLastExclusive = INDEX_NONE;
	if (!ValidateBatchRequest(
		InOutState,
		Request,
		WarehouseFirst,
		WarehouseLastExclusive,
		&OutResult.Error))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	int32 SourceFirst = INDEX_NONE;
	int32 SourceLastExclusive = INDEX_NONE;
	int32 DestinationFirst = INDEX_NONE;
	int32 DestinationLastExclusive = INDEX_NONE;
	if (!ResolveContainerRange(
		Candidate,
		Request.FromContainer,
		WarehouseFirst,
		WarehouseLastExclusive,
		SourceFirst,
		SourceLastExclusive)
		|| !ResolveContainerRange(
			Candidate,
			Request.ToContainer,
			WarehouseFirst,
			WarehouseLastExclusive,
			DestinationFirst,
			DestinationLastExclusive))
	{
		OutResult.Error = TEXT("Desktop inventory batch source or destination range is invalid.");
		return false;
	}

	// A newly granted Backpack stack may duplicate an older Warehouse stack.
	// Merge those current-source candidates before general normalization so the
	// operation can restore the one-authoritative-stack invariant atomically.
	if (!MergeDuplicateSourceStacksBeforeNormalize(
		Candidate,
		Request,
		SourceFirst,
		SourceLastExclusive,
		OutResult.MovedEntryCount,
		&OutResult.Error)
		|| !Normalize(Candidate, &OutResult.Error))
	{
		return false;
	}

	TArray<FGameXXKDesktopInventoryEntryKey> Candidates;
	const TArray<FGameXXKDesktopInventoryEntryKey>& SourceSlots = SlotsFor(
		Candidate, Request.FromContainer);
	for (int32 SlotIndex = SourceFirst; SlotIndex < SourceLastExclusive; ++SlotIndex)
	{
		if (SourceSlots.IsValidIndex(SlotIndex)
			&& SourceSlots[SlotIndex].IsValid()
			&& !Request.ExcludedEntries.Contains(SourceSlots[SlotIndex]))
		{
			Candidates.Add(SourceSlots[SlotIndex]);
		}
	}

	for (const FGameXXKDesktopInventoryEntryKey& Entry : Candidates)
	{
		const int32 CurrentSourceSlot = FindEntrySlot(
			Candidate, Request.FromContainer, Entry);
		if (CurrentSourceSlot == INDEX_NONE)
		{
			continue;
		}

		if (!Entry.bEquipmentInstance)
		{
			const TMap<FName, int32>& DestinationItems =
				Request.ToContainer == EGameXXKDesktopItemContainer::Warehouse
					? Candidate.DesktopInventory.WarehouseItems
					: Candidate.Inventory;
			if (DestinationItems.FindRef(Entry.EntryId) > 0)
			{
				if (!MergeItemStackAcrossContainers(
					Candidate,
					Entry,
					Request.FromContainer,
					Request.ToContainer,
					&OutResult.Error))
				{
					return false;
				}
				SlotsFor(Candidate, Request.FromContainer)[CurrentSourceSlot] =
					FGameXXKDesktopInventoryEntryKey();
				++OutResult.MovedEntryCount;
				continue;
			}
		}

		const int32 TargetSlot = FindFirstEmptySlotInRange(
			Candidate,
			Request.ToContainer,
			DestinationFirst,
			DestinationLastExclusive);
		if (TargetSlot == INDEX_NONE)
		{
			OutResult.bDestinationFull = true;
			break;
		}
		FGameXXKDesktopInventoryMoveRequest MoveRequest;
		MoveRequest.FromContainer = Request.FromContainer;
		MoveRequest.FromSlotIndex = CurrentSourceSlot;
		MoveRequest.ToContainer = Request.ToContainer;
		MoveRequest.ToSlotIndex = TargetSlot;
		MoveRequest.bAllowSwap = false;
		MoveRequest.ExpectedEntry = Entry;
		if (!MoveOrSwap(Candidate, MoveRequest, &OutResult.Error))
		{
			return false;
		}
		++OutResult.MovedEntryCount;
	}

	if (!Normalize(Candidate, &OutResult.Error)
		|| !Validate(Candidate, &OutResult.Error))
	{
		return false;
	}
	SynchronizeLegacyMaterialMirrors(Candidate);
	InOutState = MoveTemp(Candidate);
	OutResult.bSucceeded = true;
	return true;
}

FGameXXKDesktopInventoryEntryKey FGameXXKDesktopInventoryRules::GetEntryAt(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container,
	const int32 SlotIndex)
{
	const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
	return SlotIndex >= 0
		&& SlotIndex < LogicalCapacityFor(State, Container)
		&& Slots.IsValidIndex(SlotIndex)
		? Slots[SlotIndex]
		: FGameXXKDesktopInventoryEntryKey();
}

int32 FGameXXKDesktopInventoryRules::FindEntrySlot(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container,
	const FGameXXKDesktopInventoryEntryKey& Entry)
{
	if (!Entry.IsValid())
	{
		return INDEX_NONE;
	}
	const int32 SlotIndex = SlotsFor(State, Container).IndexOfByKey(Entry);
	return SlotIndex >= 0 && SlotIndex < LogicalCapacityFor(State, Container) ? SlotIndex : INDEX_NONE;
}

int32 FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
	const FGameXXKRuntimeState& State,
	const EGameXXKDesktopItemContainer Container)
{
	const TArray<FGameXXKDesktopInventoryEntryKey>& Slots = SlotsFor(State, Container);
	for (int32 Index = 0; Index < LogicalCapacityFor(State, Container); ++Index)
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
