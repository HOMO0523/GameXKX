#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentRules.h"

struct FGameXXKRuntimeState;

/** Atomic equipment transactions that also touch permanent runtime resources or mirrors. */
class GAMEXXK_API FGameXXKEquipmentEconomyRules final
{
public:
	/** Canonical derived mirror/stat projection used by transactions, migration, and legacy facades. */
	static bool SynchronizeRuntimeMirrors(FGameXXKRuntimeState& InOutState);
	static int32 CountLegacyEquipmentInstances(const FGameXXKRuntimeState& State, FName BaseEquipmentId);
	/** Stable warehouse order first; optionally falls back to the hero's matching equipped slot. */
	static FName FindLegacyInstanceForCompatibility(
		const FGameXXKRuntimeState& State,
		FName BaseEquipmentId,
		bool bAllowHeroEquippedFallback);
	static bool GrantLegacyEquipmentForCompatibility(
		FGameXXKRuntimeState& InOutState,
		FName BaseEquipmentId,
		int32 Quantity,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool SellLegacyEquipmentForCompatibility(
		FGameXXKRuntimeState& InOutState,
		FName BaseEquipmentId,
		int32 Quantity,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool Equip(
		FGameXXKRuntimeState& InOutState,
		FName CharacterId,
		EGameXXKEquipmentSlot Slot,
		FName InstanceId,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool Unequip(
		FGameXXKRuntimeState& InOutState,
		FName CharacterId,
		EGameXXKEquipmentSlot Slot,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool EnhanceInstance(
		FGameXXKRuntimeState& InOutState,
		FName InstanceId,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool BeginReforge(
		FGameXXKRuntimeState& InOutState,
		FName InstanceId,
		int32 AffixIndex,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool ResolvePendingReforge(
		FGameXXKRuntimeState& InOutState,
		bool bAccept,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool DismantleBatch(
		FGameXXKRuntimeState& InOutState,
		const TArray<FName>& InstanceIds,
		bool bConfirmedProtected,
		FGameXXKEquipmentTransactionResult& OutResult);
	static bool PurchaseLegacyEquipmentForCompatibility(
		FGameXXKRuntimeState& InOutState,
		FName BaseEquipmentId,
		FGameXXKEquipmentTransactionResult& OutResult);
};
