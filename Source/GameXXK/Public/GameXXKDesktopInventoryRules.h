#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

enum class EGameXXKDesktopItemContainer : uint8
{
	Backpack,
	Warehouse
};

/** Plain C++ request for one exact physical-cell move or occupied swap. */
struct GAMEXXK_API FGameXXKDesktopInventoryMoveRequest
{
	EGameXXKDesktopItemContainer FromContainer = EGameXXKDesktopItemContainer::Backpack;
	int32 FromSlotIndex = INDEX_NONE;
	EGameXXKDesktopItemContainer ToContainer = EGameXXKDesktopItemContainer::Backpack;
	int32 ToSlotIndex = INDEX_NONE;
	bool bAllowSwap = true;
	FGameXXKDesktopInventoryEntryKey ExpectedEntry;
};

/** One deterministic all-entry transfer between Backpack and one Warehouse page. */
struct GAMEXXK_API FGameXXKDesktopInventoryBatchTransferRequest
{
	EGameXXKDesktopItemContainer FromContainer = EGameXXKDesktopItemContainer::Backpack;
	EGameXXKDesktopItemContainer ToContainer = EGameXXKDesktopItemContainer::Warehouse;
	int32 WarehousePageIndex = 0;
	int32 WarehousePageSize = 36;
	TSet<FGameXXKDesktopInventoryEntryKey> ExcludedEntries;
};

/** Result counts physical source entries, not quantities inside whole item stacks. */
struct GAMEXXK_API FGameXXKDesktopInventoryBatchTransferResult
{
	bool bSucceeded = false;
	bool bDestinationFull = false;
	int32 MovedEntryCount = 0;
	FString Error;
};

/** Pure, transactional rules for the desktop backpack/warehouse cell model. */
class GAMEXXK_API FGameXXKDesktopInventoryRules final
{
public:
	static constexpr int32 BackpackCapacity = 200;
	static constexpr int32 WarehouseCapacity = 200;

	static FGameXXKDesktopInventoryEntryKey MakeItemEntry(FName ItemId);
	static FGameXXKDesktopInventoryEntryKey MakeEquipmentEntry(FName InstanceId);
	static bool IsEntryLocked(
		const FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry);
	static bool SetEntryLocked(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKDesktopInventoryEntryKey& Entry,
		bool bLocked,
		FString* OutError = nullptr);

	/** Preserves valid occupied indices, clears stale entries, and appends newly acquired entries. */
	static bool Normalize(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
	static bool Validate(const FGameXXKRuntimeState& State, FString* OutError = nullptr);
	static bool GrantUniqueTaskItem(
		FGameXXKRuntimeState& InOutState,
		FName ItemId,
		FString* OutError = nullptr);

	/** Whole-stack/instance move or occupied swap between exact physical cells. */
	static bool MoveOrSwap(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKDesktopInventoryMoveRequest& Request,
		FString* OutError = nullptr);

	/** Compatibility move facade. Occupied destinations continue to reject atomically. */
	static bool MoveEntry(
		FGameXXKRuntimeState& InOutState,
		EGameXXKDesktopItemContainer FromContainer,
		int32 FromSlotIndex,
		EGameXXKDesktopItemContainer ToContainer,
		int32 ToSlotIndex,
		FString* OutError = nullptr);

	static bool CanBatchTransferCurrentWarehousePage(
		const FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryBatchTransferRequest& Request);
	static bool BatchTransferCurrentWarehousePage(
		FGameXXKRuntimeState& InOutState,
		const FGameXXKDesktopInventoryBatchTransferRequest& Request,
		FGameXXKDesktopInventoryBatchTransferResult& OutResult);

	static FGameXXKDesktopInventoryEntryKey GetEntryAt(
		const FGameXXKRuntimeState& State,
		EGameXXKDesktopItemContainer Container,
		int32 SlotIndex);
	static int32 FindEntrySlot(
		const FGameXXKRuntimeState& State,
		EGameXXKDesktopItemContainer Container,
		const FGameXXKDesktopInventoryEntryKey& Entry);
	static int32 FindFirstEmptySlot(
		const FGameXXKRuntimeState& State,
		EGameXXKDesktopItemContainer Container);
	static int32 GetOccupiedSlotCount(
		const FGameXXKRuntimeState& State,
		EGameXXKDesktopItemContainer Container);
	static int32 GetLastOccupiedSlotIndex(
		const FGameXXKRuntimeState& State,
		EGameXXKDesktopItemContainer Container);
};
