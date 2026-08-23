#include "UI/GameXXKCharacterBackpackModel.h"

#include "GameXXKEquipmentCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const TArray<EGameXXKEquipmentSlot>& GetStableEquipmentSlotOrder()
	{
		static const TArray<EGameXXKEquipmentSlot> Slots = {
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKEquipmentSlot::Head,
			EGameXXKEquipmentSlot::Armor,
			EGameXXKEquipmentSlot::Belt,
			EGameXXKEquipmentSlot::Shoes,
			EGameXXKEquipmentSlot::Accessory};
		return Slots;
	}

	FGameXXKEquipmentTransactionResult MakeLocalFailure(
		const EGameXXKEquipmentTransactionError Error,
		const FText& Message)
	{
		FGameXXKEquipmentTransactionResult Result;
		Result.bSucceeded = false;
		Result.Error = Error;
		Result.Message = Message;
		return Result;
	}
}

void FGameXXKCharacterBackpackModel::Bind(
	UGameXXKMVPSubsystem* InSubsystem,
	const FName InCharacterId)
{
	Subsystem = InSubsystem;
	CharacterId = InCharacterId;
}

FName FGameXXKCharacterBackpackModel::GetCharacterId() const
{
	return CharacterId;
}

TArray<FGameXXKCharacterBackpackSlotView> FGameXXKCharacterBackpackModel::GetSixSlotSnapshot() const
{
	TArray<FGameXXKCharacterBackpackSlotView> Views;
	Views.Reserve(GetStableEquipmentSlotOrder().Num());

	const UGameXXKMVPSubsystem* ResolvedSubsystem = Subsystem.Get();
	const FGameXXKEquipmentLoadout* Loadout = nullptr;
	if (ResolvedSubsystem && !CharacterId.IsNone())
	{
		FGameXXKEquipmentLoadoutSnapshot ValidatedSnapshot;
		if (ResolvedSubsystem->GetEquipmentLoadoutSnapshot(CharacterId, ValidatedSnapshot))
		{
			Loadout = ResolvedSubsystem->GetRuntimeState().EquipmentCollection.CharacterLoadouts.Find(CharacterId);
		}
	}

	for (const EGameXXKEquipmentSlot Slot : GetStableEquipmentSlotOrder())
	{
		FGameXXKCharacterBackpackSlotView& View = Views.AddDefaulted_GetRef();
		View.Slot = Slot;
		View.EquippedInstanceId = Loadout
			? FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*Loadout, Slot)
			: NAME_None;
	}
	return Views;
}

bool FGameXXKCharacterBackpackModel::QuickEquip(
	const FName WarehouseInstanceId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	UGameXXKMVPSubsystem* ResolvedSubsystem = Subsystem.Get();
	if (!ResolvedSubsystem || CharacterId.IsNone() || WarehouseInstanceId.IsNone())
	{
		OutResult = MakeLocalFailure(
			EGameXXKEquipmentTransactionError::InvalidRequest,
			NSLOCTEXT("GameXXKCharacterBackpackModel", "InvalidQuickEquip", "无法装备：角色或装备无效"));
		return false;
	}

	const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
		ResolvedSubsystem->GetRuntimeState().EquipmentCollection,
		WarehouseInstanceId);
	if (!Instance)
	{
		OutResult = MakeLocalFailure(
			EGameXXKEquipmentTransactionError::InstanceMissing,
			NSLOCTEXT("GameXXKCharacterBackpackModel", "MissingQuickEquipInstance", "无法装备：装备实例不存在"));
		return false;
	}

	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
	if (!Definition)
	{
		OutResult = MakeLocalFailure(
			EGameXXKEquipmentTransactionError::DefinitionMissing,
			NSLOCTEXT("GameXXKCharacterBackpackModel", "MissingQuickEquipDefinition", "无法装备：装备定义不存在"));
		return false;
	}

	return ResolvedSubsystem->EquipEquipmentInstance(
		CharacterId,
		Definition->Slot,
		WarehouseInstanceId,
		OutResult);
}

bool FGameXXKCharacterBackpackModel::QuickUnequip(
	const EGameXXKEquipmentSlot Slot,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	UGameXXKMVPSubsystem* ResolvedSubsystem = Subsystem.Get();
	if (!ResolvedSubsystem || CharacterId.IsNone() || Slot == EGameXXKEquipmentSlot::Invalid)
	{
		OutResult = MakeLocalFailure(
			EGameXXKEquipmentTransactionError::InvalidRequest,
			NSLOCTEXT("GameXXKCharacterBackpackModel", "InvalidQuickUnequip", "无法卸下：角色或装备槽无效"));
		return false;
	}

	return ResolvedSubsystem->UnequipEquipmentSlot(CharacterId, Slot, OutResult);
}
