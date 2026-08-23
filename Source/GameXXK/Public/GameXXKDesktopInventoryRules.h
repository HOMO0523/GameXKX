#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

enum class EGameXXKDesktopItemContainer : uint8
{
	Backpack,
	Warehouse
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

	/** Whole-stack move. Occupied destinations reject atomically; no implicit swap occurs. */
	static bool MoveEntry(
		FGameXXKRuntimeState& InOutState,
		EGameXXKDesktopItemContainer FromContainer,
		int32 FromSlotIndex,
		EGameXXKDesktopItemContainer ToContainer,
		int32 ToSlotIndex,
		FString* OutError = nullptr);

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
