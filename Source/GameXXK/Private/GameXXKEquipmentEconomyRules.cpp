#include "GameXXKEquipmentEconomyRules.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKMVPRules.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

namespace
{
	constexpr EGameXXKEquipmentSlot CompatibilitySlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Accessory,
	};

	FGameXXKEquipmentTransactionResult MakeFailure(
		const EGameXXKEquipmentTransactionError Error,
		const TArray<FName>& AffectedInstanceIds = {},
		const int32 EnhancementStoneDelta = 0,
		const int32 RefinementSandDelta = 0)
	{
		FGameXXKEquipmentTransactionResult Result;
		Result.Error = Error;
		Result.Message = FGameXXKEquipmentRules::GetTransactionErrorMessage(Error);
		Result.AffectedInstanceIds = AffectedInstanceIds;
		Result.bConfirmationRequired = Error == EGameXXKEquipmentTransactionError::ConfirmationRequired;
		Result.EnhancementStoneDelta = EnhancementStoneDelta;
		Result.RefinementSandDelta = RefinementSandDelta;
		return Result;
	}

	FGameXXKEquipmentTransactionResult MakeSuccess(
		const TArray<FName>& AffectedInstanceIds = {},
		const int32 EnhancementStoneDelta = 0,
		const int32 RefinementSandDelta = 0)
	{
		FGameXXKEquipmentTransactionResult Result;
		Result.bSucceeded = true;
		Result.AffectedInstanceIds = AffectedInstanceIds;
		Result.EnhancementStoneDelta = EnhancementStoneDelta;
		Result.RefinementSandDelta = RefinementSandDelta;
		return Result;
	}

	FName* GetSlotPtr(FGameXXKEquipmentLoadout& Loadout, const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return &Loadout.WeaponInstanceId;
		case EGameXXKEquipmentSlot::Head: return &Loadout.HeadInstanceId;
		case EGameXXKEquipmentSlot::Armor: return &Loadout.ArmorInstanceId;
		case EGameXXKEquipmentSlot::Belt: return &Loadout.BeltInstanceId;
		case EGameXXKEquipmentSlot::Shoes: return &Loadout.ShoesInstanceId;
		case EGameXXKEquipmentSlot::Accessory: return &Loadout.AccessoryInstanceId;
		default: return nullptr;
		}
	}

	const FName* GetSlotPtr(const FGameXXKEquipmentLoadout& Loadout, const EGameXXKEquipmentSlot Slot)
	{
		return GetSlotPtr(const_cast<FGameXXKEquipmentLoadout&>(Loadout), Slot);
	}

	FGameXXKEquipmentInstance* FindMutableInstance(
		FGameXXKEquipmentCollectionState& Collection,
		const FName InstanceId)
	{
		return Collection.EquipmentInstances.FindByPredicate(
			[InstanceId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
	}

	bool IsLoadoutEmpty(const FGameXXKEquipmentLoadout& Loadout)
	{
		return Loadout.WeaponInstanceId.IsNone()
			&& Loadout.HeadInstanceId.IsNone()
			&& Loadout.ArmorInstanceId.IsNone()
			&& Loadout.BeltInstanceId.IsNone()
			&& Loadout.ShoesInstanceId.IsNone()
			&& Loadout.AccessoryInstanceId.IsNone();
	}

	bool AffixRollsEqual(const FGameXXKEquipmentAffixRoll& A, const FGameXXKEquipmentAffixRoll& B)
	{
		return A.AffixId == B.AffixId
			&& A.Tier == B.Tier
			&& A.Magnitude == B.Magnitude
			&& A.Unit == B.Unit;
	}

	void SynchronizeEnhancementMaterial(FGameXXKRuntimeState& State)
	{
		State.EnhancementMaterial = FMath::Max(
			0,
			State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()));
	}

	void SynchronizeLegacyEquipmentMirrors(FGameXXKRuntimeState& State)
	{
		TArray<FName> InventoryKeys;
		State.Inventory.GetKeys(InventoryKeys);
		for (const FName ItemId : InventoryKeys)
		{
			if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
			{
				State.Inventory.Remove(ItemId);
			}
		}

		TMap<FName, int32> LegacyCounts;
		for (const FGameXXKEquipmentInstance& Instance : State.EquipmentCollection.EquipmentInstances)
		{
			const FGameXXKEquipmentDefinition* Definition =
				FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
			if (Definition && Definition->Set == EGameXXKEquipmentSet::Legacy)
			{
				LegacyCounts.FindOrAdd(Instance.BaseEquipmentId) += 1;
			}
		}
		for (const TPair<FName, int32>& Pair : LegacyCounts)
		{
			State.Inventory.Add(Pair.Key, Pair.Value);
		}

		TArray<FName> EnhancementKeys;
		State.ItemEnhancementLevels.GetKeys(EnhancementKeys);
		for (const FName ItemId : EnhancementKeys)
		{
			if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
			{
				State.ItemEnhancementLevels.Remove(ItemId);
			}
		}

		State.EquippedWeapon = NAME_None;
		State.EquippedArmor = NAME_None;
		State.EquippedAccessory = NAME_None;
		const FGameXXKEquipmentLoadout* HeroLoadout =
			State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
		if (!HeroLoadout)
		{
			return;
		}

		for (const EGameXXKEquipmentSlot Slot : CompatibilitySlots)
		{
			const FName* InstanceId = GetSlotPtr(*HeroLoadout, Slot);
			const FGameXXKEquipmentInstance* Instance = InstanceId
				? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, *InstanceId)
				: nullptr;
			if (!Instance)
			{
				continue;
			}
			switch (Slot)
			{
			case EGameXXKEquipmentSlot::Weapon: State.EquippedWeapon = Instance->BaseEquipmentId; break;
			case EGameXXKEquipmentSlot::Armor: State.EquippedArmor = Instance->BaseEquipmentId; break;
			case EGameXXKEquipmentSlot::Accessory: State.EquippedAccessory = Instance->BaseEquipmentId; break;
			default: break;
			}
			State.ItemEnhancementLevels.Add(Instance->BaseEquipmentId, Instance->EnhancementLevel);
		}
	}

	bool RecalculateHeroMirrors(FGameXXKRuntimeState& State)
	{
		const int32 RouteMaxHealth = FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxHealth);
		const int32 RouteMaxMana = FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxMana);
		const int32 OldMaxHealth = FMath::Max(1, State.PlayerMaxHP + RouteMaxHealth);
		const int32 OldMaxMana = FMath::Max(1, State.PlayerMaxMP + RouteMaxMana);
		const int32 MissingHealth = FMath::Max(0, OldMaxHealth - State.PlayerHP);
		const int32 MissingMana = FMath::Max(0, OldMaxMana - State.PlayerMP);
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			State.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel),
			Snapshot))
		{
			return false;
		}
		State.PlayerMaxHP = Snapshot.AttributesBeforeRoute.MaxHealth;
		State.PlayerMaxMP = Snapshot.AttributesBeforeRoute.MaxMana;
		State.PlayerAttack = Snapshot.AttributesBeforeRoute.Attack;
		State.PlayerDefense = Snapshot.AttributesBeforeRoute.Defense;
		State.PlayerSpeed = Snapshot.AttributesBeforeRoute.Speed;
		const int32 NewEffectiveMaxHealth = FMath::Max(1, State.PlayerMaxHP + RouteMaxHealth);
		const int32 NewEffectiveMaxMana = FMath::Max(1, State.PlayerMaxMP + RouteMaxMana);
		State.PlayerHP = FMath::Clamp(NewEffectiveMaxHealth - MissingHealth, 0, NewEffectiveMaxHealth);
		State.PlayerMP = FMath::Clamp(NewEffectiveMaxMana - MissingMana, 0, NewEffectiveMaxMana);
		return true;
	}

	bool SynchronizeAndValidate(FGameXXKRuntimeState& Candidate)
	{
		if (Candidate.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()) < 0
			|| Candidate.PlayerGold < 0)
		{
			return false;
		}
		SynchronizeEnhancementMaterial(Candidate);
		SynchronizeLegacyEquipmentMirrors(Candidate);
		return RecalculateHeroMirrors(Candidate)
			&& FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				Candidate.EquipmentCollection,
				Candidate.CardRun.CompanionRoster);
	}

	bool ValidateInputCollection(const FGameXXKRuntimeState& State)
	{
		return State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()) >= 0
			&& State.PlayerGold >= 0
			&& FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				State.EquipmentCollection,
				State.CardRun.CompanionRoster);
	}

	EGameXXKAffixTier RollTier(
		FRandomStream& Stream,
		const EGameXXKEquipmentQuality Quality)
	{
		const FGameXXKAffixTierWeights Weights = FGameXXKAffixCatalog::GetTierWeights(Quality);
		const int32 Total = Weights.Common + Weights.Rare + Weights.Epic;
		const int32 Pick = Stream.RandRange(1, Total);
		if (Pick <= Weights.Common)
		{
			return EGameXXKAffixTier::Common;
		}
		if (Pick <= Weights.Common + Weights.Rare)
		{
			return EGameXXKAffixTier::Rare;
		}
		return EGameXXKAffixTier::Epic;
	}

	bool BuildReforgeCandidate(
		const FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& EquipmentDefinition,
		const int32 AffixIndex,
		FGameXXKEquipmentAffixRoll& OutCandidate)
	{
		TSet<EGameXXKEquipmentModifierKind> ExcludedKinds;
		for (int32 Index = 0; Index < Instance.RolledAffixes.Num(); ++Index)
		{
			const FGameXXKAffixDefinition* Affix =
				FGameXXKAffixCatalog::FindDefinition(Instance.RolledAffixes[Index].AffixId);
			if (Affix)
			{
				ExcludedKinds.Add(Affix->ModifierKind);
			}
		}

		TArray<const FGameXXKAffixDefinition*> Candidates;
		auto AppendCandidates = [&Candidates, &ExcludedKinds](const TArray<FGameXXKAffixDefinition>& Definitions)
		{
			for (const FGameXXKAffixDefinition& Definition : Definitions)
			{
				if (!ExcludedKinds.Contains(Definition.ModifierKind))
				{
					Candidates.Add(&Definition);
				}
			}
		};
		AppendCandidates(FGameXXKAffixCatalog::GetUniversalDefinitions());
		AppendCandidates(FGameXXKAffixCatalog::GetSetDefinitions(EquipmentDefinition.Set));
		Candidates.Sort([](const FGameXXKAffixDefinition& A, const FGameXXKAffixDefinition& B)
		{
			return A.Id.LexicalLess(B.Id);
		});
		if (Candidates.IsEmpty())
		{
			return false;
		}

		const FString SeedText = FString::Printf(
			TEXT("%d|%d|%s|%d"),
			Collection.CollectionSeed,
			Collection.NextReforgeOrdinal,
			*Instance.InstanceId.ToString(),
			AffixIndex);
		FRandomStream Stream(FCrc::StrCrc32(*SeedText));
		const FGameXXKAffixDefinition* Chosen = Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
		const EGameXXKAffixTier Tier = RollTier(Stream, Instance.Quality);
		const FGameXXKAffixMagnitudeRange Range = FGameXXKAffixCatalog::GetMagnitudeRange(Chosen->Unit, Tier);
		if (Range.Minimum <= 0 || Range.Maximum < Range.Minimum)
		{
			return false;
		}
		OutCandidate.AffixId = Chosen->Id;
		OutCandidate.Tier = Tier;
		OutCandidate.Magnitude = Stream.RandRange(Range.Minimum, Range.Maximum);
		OutCandidate.Unit = Chosen->Unit;
		return true;
	}

	int32 CalculateSpentEnhancementStones(const FGameXXKEquipmentInstance& Instance)
	{
		int32 Spent = 0;
		for (int32 Level = 0; Level < Instance.EnhancementLevel; ++Level)
		{
			Spent += FGameXXKEquipmentCatalog::GetEnhancementStoneCost(Level);
		}
		return Spent;
	}

	FString BuildLegacyInstanceId(const FGameXXKEquipmentCollectionState& Collection)
	{
		return FString::Printf(
			TEXT("EquipmentInstance.%08X.%d"),
			static_cast<uint32>(Collection.CollectionSeed),
			Collection.NextInstanceOrdinal);
	}

	bool AppendLegacyWarehouseInstance(
		FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentDefinition& Definition,
		FName& OutInstanceId)
	{
		OutInstanceId = NAME_None;
		if (Collection.NextInstanceOrdinal == MAX_int32)
		{
			return false;
		}
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = FName(*BuildLegacyInstanceId(Collection));
		if (FGameXXKEquipmentRules::FindInstance(Collection, Instance.InstanceId))
		{
			return false;
		}
		Instance.BaseEquipmentId = Definition.Id;
		Instance.ItemLevel = 1;
		Instance.Quality = EGameXXKEquipmentQuality::Common;
		Instance.AcquisitionSeed = static_cast<int32>(FCrc::StrCrc32(*Instance.InstanceId.ToString()));
		Instance.ScalingRule = Definition.ScalingRule;
		Instance.LegacyBaseStatSnapshot = Definition.LegacyBaseStatSnapshot;
		Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
		OutInstanceId = Instance.InstanceId;
		Collection.EquipmentInstances.Add(MoveTemp(Instance));
		Collection.WarehouseInstanceIds.Add(OutInstanceId);
		Collection.NextInstanceOrdinal += 1;
		return true;
	}
}

bool FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(FGameXXKRuntimeState& InOutState)
{
	FGameXXKRuntimeState Candidate = InOutState;
	if (!SynchronizeAndValidate(Candidate))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
	return true;
}

int32 FGameXXKEquipmentEconomyRules::CountLegacyEquipmentInstances(
	const FGameXXKRuntimeState& State,
	const FName BaseEquipmentId)
{
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
	if (!Definition || Definition->Set != EGameXXKEquipmentSet::Legacy)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FGameXXKEquipmentInstance& Instance : State.EquipmentCollection.EquipmentInstances)
	{
		if (Instance.BaseEquipmentId == BaseEquipmentId)
		{
			++Count;
		}
	}
	return Count;
}

FName FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(
	const FGameXXKRuntimeState& State,
	const FName BaseEquipmentId,
	const bool bAllowHeroEquippedFallback)
{
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
	if (!Definition || Definition->Set != EGameXXKEquipmentSet::Legacy)
	{
		return NAME_None;
	}
	for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
	{
		const FGameXXKEquipmentInstance* Instance =
			FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, InstanceId);
		if (Instance && Instance->BaseEquipmentId == BaseEquipmentId)
		{
			return InstanceId;
		}
	}
	if (!bAllowHeroEquippedFallback)
	{
		return NAME_None;
	}
	const FGameXXKEquipmentLoadout* HeroLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	if (!HeroLoadout)
	{
		return NAME_None;
	}
	const FName* SlotId = GetSlotPtr(*HeroLoadout, Definition->Slot);
	const FGameXXKEquipmentInstance* Instance = SlotId
		? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, *SlotId)
		: nullptr;
	return Instance && Instance->BaseEquipmentId == BaseEquipmentId ? *SlotId : NAME_None;
}

bool FGameXXKEquipmentEconomyRules::GrantLegacyEquipmentForCompatibility(
	FGameXXKRuntimeState& InOutState,
	const FName BaseEquipmentId,
	const int32 Quantity,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
	if (Quantity <= 0 || !Definition || Definition->Set != EGameXXKEquipmentSet::Legacy)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	if (!FGameXXKEquipmentRules::HasWarehouseCapacity(InOutState.EquipmentCollection, Quantity)
		|| static_cast<int64>(InOutState.EquipmentCollection.NextInstanceOrdinal) + Quantity > MAX_int32)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::WarehouseFull);
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	TArray<FName> AddedIds;
	AddedIds.Reserve(Quantity);
	for (int32 CopyIndex = 0; CopyIndex < Quantity; ++CopyIndex)
	{
		FName InstanceId;
		if (!AppendLegacyWarehouseInstance(Candidate.EquipmentCollection, *Definition, InstanceId))
		{
			OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
			return false;
		}
		AddedIds.Add(InstanceId);
	}
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess(AddedIds);
	return true;
}

bool FGameXXKEquipmentEconomyRules::SellLegacyEquipmentForCompatibility(
	FGameXXKRuntimeState& InOutState,
	const FName BaseEquipmentId,
	const int32 Quantity,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
	bool bFoundItemDefinition = false;
	const FGameXXKItemDef ItemDefinition = UGameXXKMVPRules::GetItemDef(BaseEquipmentId, bFoundItemDefinition);
	if (Quantity <= 0 || !Definition || Definition->Set != EGameXXKEquipmentSet::Legacy
		|| !bFoundItemDefinition || ItemDefinition.SellPrice < 0 || !ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	TArray<FName> SelectedIds;
	for (const FName InstanceId : InOutState.EquipmentCollection.WarehouseInstanceIds)
	{
		const FGameXXKEquipmentInstance* Instance =
			FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, InstanceId);
		if (Instance && Instance->BaseEquipmentId == BaseEquipmentId)
		{
			SelectedIds.Add(InstanceId);
			if (SelectedIds.Num() == Quantity)
			{
				break;
			}
		}
	}
	const int64 GoldDelta = static_cast<int64>(ItemDefinition.SellPrice) * Quantity;
	if (SelectedIds.Num() != Quantity || static_cast<int64>(InOutState.PlayerGold) + GoldDelta > MAX_int32)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	for (const FName InstanceId : SelectedIds)
	{
		Candidate.EquipmentCollection.WarehouseInstanceIds.RemoveSingle(InstanceId);
		Candidate.EquipmentCollection.EquipmentInstances.RemoveAll(
			[InstanceId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
	}
	Candidate.PlayerGold += static_cast<int32>(GoldDelta);
	if (Candidate.EquipmentCollection.bLegacyWarehouseOverflow
		&& Candidate.EquipmentCollection.WarehouseInstanceIds.Num() <= FGameXXKEquipmentRules::WarehouseCapacity)
	{
		Candidate.EquipmentCollection.bLegacyWarehouseOverflow = false;
	}
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess(SelectedIds);
	return true;
}

bool FGameXXKEquipmentEconomyRules::Equip(
	FGameXXKRuntimeState& InOutState,
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	const FName InstanceId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	const FGameXXKEquipmentTransactionResult CoreResult = FGameXXKEquipmentRules::EquipInstance(
		Candidate.EquipmentCollection,
		Candidate.CardRun.CompanionRoster,
		CharacterId,
		Slot,
		InstanceId);
	if (!CoreResult.bSucceeded)
	{
		OutResult = CoreResult;
		return false;
	}
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = CoreResult;
	return true;
}

bool FGameXXKEquipmentEconomyRules::Unequip(
	FGameXXKRuntimeState& InOutState,
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	const FGameXXKEquipmentTransactionResult CoreResult = FGameXXKEquipmentRules::UnequipInstance(
		Candidate.EquipmentCollection,
		CharacterId,
		Slot);
	if (!CoreResult.bSucceeded)
	{
		OutResult = CoreResult;
		return false;
	}
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = CoreResult;
	return true;
}

bool FGameXXKEquipmentEconomyRules::EnhanceInstance(
	FGameXXKRuntimeState& InOutState,
	const FName InstanceId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	const FGameXXKEquipmentInstance* Existing =
		FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, InstanceId);
	if (!Existing)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InstanceMissing);
		return false;
	}
	if (!FGameXXKEquipmentCatalog::FindDefinition(Existing->BaseEquipmentId))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::DefinitionMissing);
		return false;
	}
	if (Existing->EnhancementLevel >= FGameXXKEquipmentRules::MaxEnhancementLevel)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::MaxEnhancementReached);
		return false;
	}
	const int32 Cost = FGameXXKEquipmentCatalog::GetEnhancementStoneCost(Existing->EnhancementLevel);
	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	if (Cost <= 0 || InOutState.Inventory.FindRef(StoneId) < Cost)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InsufficientEnhancementStones);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKEquipmentInstance* Enhanced = FindMutableInstance(Candidate.EquipmentCollection, InstanceId);
	Candidate.Inventory.FindOrAdd(StoneId) -= Cost;
	Enhanced->EnhancementLevel += 1;
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess({InstanceId}, -Cost, 0);
	return true;
}

bool FGameXXKEquipmentEconomyRules::BeginReforge(
	FGameXXKRuntimeState& InOutState,
	const FName InstanceId,
	const int32 AffixIndex,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	if (InOutState.EquipmentCollection.PendingReforge.bActive)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::PendingReforgeExists);
		return false;
	}
	if (InOutState.EquipmentCollection.NextReforgeOrdinal == MAX_int32)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	const FGameXXKEquipmentInstance* Existing =
		FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, InstanceId);
	if (!Existing)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InstanceMissing);
		return false;
	}
	const FGameXXKEquipmentDefinition* EquipmentDefinition =
		FGameXXKEquipmentCatalog::FindDefinition(Existing->BaseEquipmentId);
	if (!EquipmentDefinition)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::DefinitionMissing);
		return false;
	}
	if (EquipmentDefinition->Set == EGameXXKEquipmentSet::Legacy
		|| !Existing->RolledAffixes.IsValidIndex(AffixIndex))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	const int32 Cost = FGameXXKEquipmentCatalog::GetReforgeSandCost(Existing->Quality);
	const FName SandItemId = UGameXXKMVPRules::ItemRefinementSand();
	if (Cost <= 0 || InOutState.Inventory.FindRef(SandItemId) < Cost)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InsufficientRefinementSand);
		return false;
	}
	FGameXXKEquipmentAffixRoll CandidateAffix;
	if (!BuildReforgeCandidate(
		InOutState.EquipmentCollection,
		*Existing,
		*EquipmentDefinition,
		AffixIndex,
		CandidateAffix))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKPendingEquipmentReforge& Pending = Candidate.EquipmentCollection.PendingReforge;
	Pending.bActive = true;
	Pending.InstanceId = InstanceId;
	Pending.AffixIndex = AffixIndex;
	Pending.OriginalAffix = Existing->RolledAffixes[AffixIndex];
	Pending.CandidateAffix = CandidateAffix;
	Pending.PaidRefinementSand = Cost;
	Pending.ConsumedReforgeOrdinal = Candidate.EquipmentCollection.NextReforgeOrdinal;
	// Consume refinement sand from the backpack material item and keep the
	// legacy resource field in sync.
	Candidate.Inventory.FindOrAdd(SandItemId) = FMath::Max(0, Candidate.Inventory.FindOrAdd(SandItemId) - Cost);
	Candidate.EquipmentCollection.RefinementSand = FMath::Max(0, Candidate.EquipmentCollection.RefinementSand - Cost);
	Candidate.EquipmentCollection.NextReforgeOrdinal += 1;
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess({InstanceId}, 0, -Cost);
	return true;
}

bool FGameXXKEquipmentEconomyRules::ResolvePendingReforge(
	FGameXXKRuntimeState& InOutState,
	const bool bAccept,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	const FGameXXKPendingEquipmentReforge& Pending = InOutState.EquipmentCollection.PendingReforge;
	if (!Pending.bActive)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::NoPendingReforge);
		return false;
	}
	const FGameXXKEquipmentInstance* Existing =
		FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Pending.InstanceId);
	if (bAccept && (!Existing
		|| !Existing->RolledAffixes.IsValidIndex(Pending.AffixIndex)
		|| !AffixRollsEqual(Existing->RolledAffixes[Pending.AffixIndex], Pending.OriginalAffix)))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::PendingReforgeStale);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	const FName AffectedId = Pending.InstanceId;
	if (bAccept)
	{
		FGameXXKEquipmentInstance* Mutable = FindMutableInstance(Candidate.EquipmentCollection, Pending.InstanceId);
		Mutable->RolledAffixes[Pending.AffixIndex] = Pending.CandidateAffix;
	}
	Candidate.EquipmentCollection.PendingReforge = FGameXXKPendingEquipmentReforge();
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(
			bAccept
			? EGameXXKEquipmentTransactionError::PendingReforgeStale
			: EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess({AffectedId});
	return true;
}

bool FGameXXKEquipmentEconomyRules::DismantleBatch(
	FGameXXKRuntimeState& InOutState,
	const TArray<FName>& InstanceIds,
	const bool bConfirmedProtected,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState) || InstanceIds.IsEmpty())
	{
		OutResult = MakeFailure(
			InstanceIds.IsEmpty()
			? EGameXXKEquipmentTransactionError::InvalidRequest
			: EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	TSet<FName> UniqueIds;
	int64 SandYieldWide = 0;
	int64 StoneRefundWide = 0;
	int64 GoldRewardWide = 0;
	bool bProtected = false;
	for (const FName InstanceId : InstanceIds)
	{
		if (InstanceId.IsNone() || UniqueIds.Contains(InstanceId))
		{
			OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
			return false;
		}
		UniqueIds.Add(InstanceId);
		const FGameXXKEquipmentInstance* Instance =
			FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, InstanceId);
		if (!Instance)
		{
			OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InstanceMissing);
			return false;
		}
		if (InOutState.EquipmentCollection.PendingReforge.bActive
			&& InOutState.EquipmentCollection.PendingReforge.InstanceId == InstanceId)
		{
			OutResult = MakeFailure(EGameXXKEquipmentTransactionError::PendingReforgeExists);
			return false;
		}
		// Fixed dismantle reward per piece: 10 gold, 1 enhancement stone, 1 refinement sand.
		SandYieldWide += 1;
		StoneRefundWide += 1;
		GoldRewardWide += 10;
		bProtected = bProtected
			|| Instance->Quality != EGameXXKEquipmentQuality::Common
			|| Instance->EnhancementLevel > 0
			|| Instance->OwnerKind != EGameXXKEquipmentOwnerKind::Warehouse;
	}
	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const int64 SandAfterDismantle = static_cast<int64>(InOutState.EquipmentCollection.RefinementSand) + SandYieldWide;
	const int64 StonesAfterDismantle = static_cast<int64>(InOutState.Inventory.FindRef(StoneId)) + StoneRefundWide;
	const int64 GoldAfterDismantle = static_cast<int64>(InOutState.PlayerGold) + GoldRewardWide;
	if (SandYieldWide > MAX_int32 || StoneRefundWide > MAX_int32 || GoldRewardWide > MAX_int32
		|| SandAfterDismantle > MAX_int32 || StonesAfterDismantle > MAX_int32 || GoldAfterDismantle > MAX_int32)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	const int32 SandYield = static_cast<int32>(SandYieldWide);
	const int32 StoneRefund = static_cast<int32>(StoneRefundWide);
	const int32 GoldReward = static_cast<int32>(GoldRewardWide);
	if (bProtected && !bConfirmedProtected)
	{
		OutResult = MakeFailure(
			EGameXXKEquipmentTransactionError::ConfirmationRequired,
			InstanceIds,
			StoneRefund,
			SandYield);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	for (const FName InstanceId : InstanceIds)
	{
		Candidate.EquipmentCollection.WarehouseInstanceIds.RemoveSingle(InstanceId);
		TArray<FName> EmptyLoadoutOwners;
		for (TPair<FName, FGameXXKEquipmentLoadout>& Pair : Candidate.EquipmentCollection.CharacterLoadouts)
		{
			for (uint8 SlotValue = static_cast<uint8>(EGameXXKEquipmentSlot::Weapon);
				SlotValue <= static_cast<uint8>(EGameXXKEquipmentSlot::Accessory);
				++SlotValue)
			{
				FName* SlotId = GetSlotPtr(Pair.Value, static_cast<EGameXXKEquipmentSlot>(SlotValue));
				if (SlotId && *SlotId == InstanceId)
				{
					*SlotId = NAME_None;
				}
			}
			if (IsLoadoutEmpty(Pair.Value))
			{
				EmptyLoadoutOwners.Add(Pair.Key);
			}
		}
		for (const FName OwnerId : EmptyLoadoutOwners)
		{
			Candidate.EquipmentCollection.CharacterLoadouts.Remove(OwnerId);
		}
		Candidate.EquipmentCollection.EquipmentInstances.RemoveAll(
			[InstanceId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
	}
	if (Candidate.EquipmentCollection.bLegacyWarehouseOverflow
		&& Candidate.EquipmentCollection.WarehouseInstanceIds.Num() <= FGameXXKEquipmentRules::WarehouseCapacity)
	{
		Candidate.EquipmentCollection.bLegacyWarehouseOverflow = false;
	}
	Candidate.EquipmentCollection.RefinementSand += SandYield;
	Candidate.Inventory.FindOrAdd(StoneId) += StoneRefund;
	// Refinement sand also lands in the backpack as a material item.
	Candidate.Inventory.FindOrAdd(UGameXXKMVPRules::ItemRefinementSand()) += SandYield;
	Candidate.PlayerGold += GoldReward;
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess(InstanceIds, StoneRefund, SandYield);
	return true;
}

bool FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(
	FGameXXKRuntimeState& InOutState,
	const FName BaseEquipmentId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateInputCollection(InOutState))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	if (InOutState.EquipmentCollection.NextInstanceOrdinal == MAX_int32)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	const FGameXXKEquipmentDefinition* Definition =
		FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
	if (!Definition)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::DefinitionMissing);
		return false;
	}
	if (Definition->Set != EGameXXKEquipmentSet::Legacy)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	if (!FGameXXKEquipmentRules::HasWarehouseCapacity(InOutState.EquipmentCollection))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::WarehouseFull);
		return false;
	}
	bool bFoundItemDefinition = false;
	const FGameXXKItemDef ItemDefinition = UGameXXKMVPRules::GetItemDef(BaseEquipmentId, bFoundItemDefinition);
	if (!bFoundItemDefinition || ItemDefinition.BuyPrice < 0 || InOutState.PlayerGold < ItemDefinition.BuyPrice)
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKEquipmentInstance Instance;
	Instance.InstanceId = FName(*BuildLegacyInstanceId(Candidate.EquipmentCollection));
	if (FGameXXKEquipmentRules::FindInstance(Candidate.EquipmentCollection, Instance.InstanceId))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	Instance.BaseEquipmentId = BaseEquipmentId;
	Instance.ItemLevel = 1;
	Instance.Quality = EGameXXKEquipmentQuality::Common;
	Instance.AcquisitionSeed = static_cast<int32>(FCrc::StrCrc32(*Instance.InstanceId.ToString()));
	Instance.ScalingRule = Definition->ScalingRule;
	Instance.LegacyBaseStatSnapshot = Definition->LegacyBaseStatSnapshot;
	Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
	const FName InstanceId = Instance.InstanceId;
	Candidate.PlayerGold -= ItemDefinition.BuyPrice;
	Candidate.EquipmentCollection.EquipmentInstances.Add(MoveTemp(Instance));
	Candidate.EquipmentCollection.WarehouseInstanceIds.Add(InstanceId);
	Candidate.EquipmentCollection.NextInstanceOrdinal += 1;
	if (!SynchronizeAndValidate(Candidate))
	{
		OutResult = MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	InOutState = MoveTemp(Candidate);
	OutResult = MakeSuccess({InstanceId});
	return true;
}
