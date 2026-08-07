#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentRules.h"

class UGameXXKMVPSubsystem;

struct GAMEXXK_API FGameXXKCharacterBackpackSlotView
{
	EGameXXKEquipmentSlot Slot = EGameXXKEquipmentSlot::Invalid;
	FName EquippedInstanceId = NAME_None;
};

/**
 * Shared interaction model for the hero and permanent-companion backpack screens.
 *
 * It deliberately delegates every mutation to UGameXXKMVPSubsystem so replacement,
 * warehouse-capacity, owner validation and route-lock behavior stay atomic.
 */
class GAMEXXK_API FGameXXKCharacterBackpackModel
{
public:
	void Bind(UGameXXKMVPSubsystem* InSubsystem, FName InCharacterId);

	FName GetCharacterId() const;
	TArray<FGameXXKCharacterBackpackSlotView> GetSixSlotSnapshot() const;
	bool QuickEquip(FName WarehouseInstanceId, FGameXXKEquipmentTransactionResult& OutResult);
	bool QuickUnequip(EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);

private:
	TWeakObjectPtr<UGameXXKMVPSubsystem> Subsystem;
	FName CharacterId = NAME_None;
};
