#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> SerializeBatchState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	FGameXXKRuntimeState MakeTwoPageState(FAutomationTestBase& Test)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
		FString Error;
		Test.TestTrue(TEXT("batch fixture unlocks and normalizes two Warehouse pages"),
			FGameXXKDesktopInventoryRules::Normalize(State, &Error));
		return State;
	}

	bool AddBackpackItem(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName ItemId,
		const int32 Quantity)
	{
		State.Inventory.Add(ItemId, Quantity);
		FString Error;
		return Test.TestTrue(TEXT("added batch item normalizes"),
			FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	}

	bool MoveToWarehouse(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry,
		const int32 WarehouseSlot)
	{
		const int32 SourceSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
			State, EGameXXKDesktopItemContainer::Backpack, Entry);
		FString Error;
		return Test.TestTrue(TEXT("batch fixture moves entry to exact Warehouse slot"),
			SourceSlot != INDEX_NONE
			&& FGameXXKDesktopInventoryRules::MoveEntry(
				State,
				EGameXXKDesktopItemContainer::Backpack,
				SourceSlot,
				EGameXXKDesktopItemContainer::Warehouse,
				WarehouseSlot,
				&Error));
	}

	FGameXXKDesktopInventoryBatchTransferRequest MakeBatchRequest(
		const EGameXXKDesktopItemContainer From,
		const int32 PageIndex = 0)
	{
		FGameXXKDesktopInventoryBatchTransferRequest Request;
		Request.FromContainer = From;
		Request.ToContainer = From == EGameXXKDesktopItemContainer::Backpack
			? EGameXXKDesktopItemContainer::Warehouse
			: EGameXXKDesktopItemContainer::Backpack;
		Request.WarehousePageIndex = PageIndex;
		Request.WarehousePageSize = 36;
		return Request;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopInventoryBatchTransferTest,
	"GameXXK.DesktopInventory.BatchTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopInventoryBatchTransferTest::RunTest(const FString& Parameters)
{
	{
		FGameXXKRuntimeState State = MakeTwoPageState(*this);
		const FName LockedItemId(TEXT("Item.Test.Batch.LockedStack"));
		const FName ExcludedItemId(TEXT("Item.Test.Batch.ToolReserved"));
		if (!AddBackpackItem(*this, State, LockedItemId, 19)
			|| !AddBackpackItem(*this, State, ExcludedItemId, 7)
			|| !TestTrue(TEXT("batch fixture has unequipped equipment"),
				!State.EquipmentCollection.WarehouseInstanceIds.IsEmpty()))
		{
			return false;
		}

		const FGameXXKDesktopInventoryEntryKey LockedItem =
			FGameXXKDesktopInventoryRules::MakeItemEntry(LockedItemId);
		const FGameXXKDesktopInventoryEntryKey LockedEquipment =
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(
				State.EquipmentCollection.WarehouseInstanceIds[0]);
		const FGameXXKDesktopInventoryEntryKey Excluded =
			FGameXXKDesktopInventoryRules::MakeItemEntry(ExcludedItemId);
		FString Error;
		TestTrue(TEXT("locked item fixture records its lock"),
			FGameXXKDesktopInventoryRules::SetEntryLocked(State, LockedItem, true, &Error));
		TestTrue(TEXT("locked equipment fixture records its lock"),
			FGameXXKDesktopInventoryRules::SetEntryLocked(State, LockedEquipment, true, &Error));

		TArray<FGameXXKDesktopInventoryEntryKey> ExpectedOrder;
		for (const FGameXXKDesktopInventoryEntryKey& Entry : State.DesktopInventory.BackpackSlots)
		{
			if (Entry.IsValid() && Entry != Excluded)
			{
				ExpectedOrder.Add(Entry);
			}
		}
		FGameXXKDesktopInventoryBatchTransferRequest Request = MakeBatchRequest(
			EGameXXKDesktopItemContainer::Backpack, 1);
		Request.ExcludedEntries.Add(Excluded);
		TestTrue(TEXT("backpack to current Warehouse page is enabled"),
			FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(State, Request));
		FGameXXKDesktopInventoryBatchTransferResult Result;
		TestTrue(TEXT("backpack to current Warehouse page succeeds"),
			FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Request, Result));
		TestTrue(TEXT("successful batch result is marked succeeded"), Result.bSucceeded);
		TestEqual(TEXT("whole stacks count as one moved entry"),
			Result.MovedEntryCount, ExpectedOrder.Num());
		TestFalse(TEXT("page-two fixture has enough destination capacity"), Result.bDestinationFull);
		for (int32 Index = 0; Index < ExpectedOrder.Num(); ++Index)
		{
			TestEqual(TEXT("source physical order is preserved in newly allocated page slots"),
				FGameXXKDesktopInventoryRules::GetEntryAt(
					State, EGameXXKDesktopItemContainer::Warehouse, 36 + Index),
				ExpectedOrder[Index]);
		}
		TestTrue(TEXT("locked equipment remains locked after transfer"),
			FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedEquipment));
		TestTrue(TEXT("locked item remains locked after transfer"),
			FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedItem));
		TestTrue(TEXT("excluded tool reservation stays in source"),
			FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Backpack, Excluded) != INDEX_NONE);
	}

	{
		FGameXXKRuntimeState State = MakeTwoPageState(*this);
		const FGameXXKDesktopInventoryEntryKey PageOneEntry =
			FGameXXKDesktopInventoryRules::MakeItemEntry(TEXT("Item.Test.Batch.PageOne"));
		const FGameXXKDesktopInventoryEntryKey PageTwoEntry =
			FGameXXKDesktopInventoryRules::MakeItemEntry(TEXT("Item.Test.Batch.PageTwo"));
		AddBackpackItem(*this, State, PageOneEntry.EntryId, 3);
		AddBackpackItem(*this, State, PageTwoEntry.EntryId, 4);
		MoveToWarehouse(*this, State, PageOneEntry, 4);
		MoveToWarehouse(*this, State, PageTwoEntry, 40);

		FGameXXKDesktopInventoryBatchTransferRequest Request = MakeBatchRequest(
			EGameXXKDesktopItemContainer::Warehouse, 0);
		FGameXXKDesktopInventoryBatchTransferResult Result;
		TestTrue(TEXT("Warehouse current page transfers back to Backpack"),
			FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Request, Result));
		TestTrue(TEXT("selected page entry reaches Backpack"),
			FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Backpack, PageOneEntry) != INDEX_NONE);
		TestTrue(TEXT("only selected Warehouse page supplies candidates"),
			FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Warehouse, PageTwoEntry) != INDEX_NONE);
	}

	{
		FGameXXKRuntimeState State = MakeTwoPageState(*this);
		const FName MergeItemId(TEXT("Item.Test.Batch.Merge"));
		const FGameXXKDesktopInventoryEntryKey MergeEntry =
			FGameXXKDesktopInventoryRules::MakeItemEntry(MergeItemId);
		AddBackpackItem(*this, State, MergeItemId, 5);
		MoveToWarehouse(*this, State, MergeEntry, 40);
		State.Inventory.Add(MergeItemId, 7);
		const int32 BackpackMergeSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
			State, EGameXXKDesktopItemContainer::Backpack);
		State.DesktopInventory.BackpackSlots[BackpackMergeSlot] = MergeEntry;

		FGameXXKDesktopInventoryBatchTransferRequest Request = MakeBatchRequest(
			EGameXXKDesktopItemContainer::Backpack, 0);
		FGameXXKDesktopInventoryBatchTransferResult Result;
		TestTrue(TEXT("existing destination stack can merge across Warehouse pages"),
			FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Request, Result));
		TestEqual(TEXT("existing destination stack receives the full source quantity"),
			State.DesktopInventory.WarehouseItems.FindRef(MergeItemId), 12);
		TestFalse(TEXT("merged source stack leaves Backpack authoritative items"),
			State.Inventory.Contains(MergeItemId));
		TestEqual(TEXT("merge keeps the existing destination physical slot"),
			FGameXXKDesktopInventoryRules::FindEntrySlot(
				State, EGameXXKDesktopItemContainer::Warehouse, MergeEntry), 40);
	}

	{
		FGameXXKRuntimeState State = MakeTwoPageState(*this);
		for (int32 Index = 0; Index < 35; ++Index)
		{
			const FName ItemId(*FString::Printf(TEXT("Item.Test.Batch.Fill.%02d"), Index));
			const FGameXXKDesktopInventoryEntryKey Entry =
				FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId);
			if (!AddBackpackItem(*this, State, ItemId, 1)
				|| !MoveToWarehouse(*this, State, Entry, Index))
			{
				return false;
			}
		}
		FGameXXKDesktopInventoryBatchTransferRequest Request = MakeBatchRequest(
			EGameXXKDesktopItemContainer::Backpack, 0);
		FGameXXKDesktopInventoryBatchTransferResult Result;
		TestTrue(TEXT("full destination reports partial success"),
			FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Request, Result));
		TestTrue(TEXT("partial success reports target full"), Result.bDestinationFull);
		TestEqual(TEXT("one remaining page slot moves one source entry"), Result.MovedEntryCount, 1);
	}

	{
		FGameXXKRuntimeState State = MakeTwoPageState(*this);
		const TArray<uint8> Before = SerializeBatchState(State);
		FGameXXKDesktopInventoryBatchTransferRequest Invalid = MakeBatchRequest(
			EGameXXKDesktopItemContainer::Backpack, 0);
		Invalid.ToContainer = EGameXXKDesktopItemContainer::Backpack;
		FGameXXKDesktopInventoryBatchTransferResult Result;
		TestFalse(TEXT("same-container batch request rejects"),
			FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Invalid, Result));
		TestFalse(TEXT("invalid request never reports success"), Result.bSucceeded);
		TestEqual(TEXT("invalid request preserves every serialized runtime byte"),
			SerializeBatchState(State), Before);
	}
	return true;
}

#endif
