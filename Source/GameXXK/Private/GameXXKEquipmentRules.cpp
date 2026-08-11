#include "GameXXKEquipmentRules.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "Misc/Crc.h"
#include "Math/RandomStream.h"

namespace
{
	constexpr EGameXXKEquipmentSlot AllSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory,
	};

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	bool IsValidSlot(const EGameXXKEquipmentSlot Slot)
	{
		return Slot >= EGameXXKEquipmentSlot::Weapon && Slot <= EGameXXKEquipmentSlot::Accessory;
	}

	bool IsModernSet(const EGameXXKEquipmentSet Set)
	{
		return Set == EGameXXKEquipmentSet::Starter
			|| (Set >= EGameXXKEquipmentSet::PoJun && Set <= EGameXXKEquipmentSet::ShanHe);
	}

	bool IsValidQuality(const EGameXXKEquipmentQuality Quality)
	{
		return Quality >= EGameXXKEquipmentQuality::Common && Quality <= EGameXXKEquipmentQuality::Epic;
	}

	bool IsValidTier(const EGameXXKAffixTier Tier)
	{
		return Tier >= EGameXXKAffixTier::Common && Tier <= EGameXXKAffixTier::Epic;
	}

	int32 ExpectedAffixCount(const EGameXXKEquipmentQuality Quality)
	{
		return IsValidQuality(Quality) ? static_cast<int32>(static_cast<uint8>(Quality)) : INDEX_NONE;
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

	bool IsLoadoutEmpty(const FGameXXKEquipmentLoadout& Loadout)
	{
		for (const EGameXXKEquipmentSlot Slot : AllSlots)
		{
			if (!GetSlotPtr(Loadout, Slot)->IsNone())
			{
				return false;
			}
		}
		return true;
	}

	const FGameXXKEquipmentDefinition* FindPackageDefinition(
		const EGameXXKEquipmentSet Set,
		const EGameXXKEquipmentSlot Slot)
	{
		return FGameXXKEquipmentCatalog::GetPackageDefinitions().FindByPredicate(
			[Set, Slot](const FGameXXKEquipmentDefinition& Definition)
			{
				return Definition.Set == Set && Definition.Slot == Slot;
			});
	}

	FString BuildRollSeedText(
		const FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentCreateRequest& Request,
		const EGameXXKEquipmentSlot Slot)
	{
		return FString::Printf(
			TEXT("%d|%d|%d|%d|%d|%d"),
			Collection.CollectionSeed,
			Collection.NextInstanceOrdinal,
			static_cast<int32>(Request.Set),
			static_cast<int32>(Slot),
			static_cast<int32>(Request.Quality),
			Request.ItemLevel);
	}

	uint32 BuildRollSeed(
		const FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentCreateRequest& Request,
		const EGameXXKEquipmentSlot Slot)
	{
		const FString StableAscii = BuildRollSeedText(Collection, Request, Slot);
		return FCrc::StrCrc32(*StableAscii);
	}

	EGameXXKAffixTier RollTier(FRandomStream& Stream, const EGameXXKEquipmentQuality Quality)
	{
		const FGameXXKAffixTierWeights Weights = FGameXXKAffixCatalog::GetTierWeights(Quality);
		const int32 TotalWeight = Weights.Common + Weights.Rare + Weights.Epic;
		const int32 Pick = Stream.RandRange(1, TotalWeight);
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

	bool AffixRollsEqual(const FGameXXKEquipmentAffixRoll& A, const FGameXXKEquipmentAffixRoll& B)
	{
		return A.AffixId == B.AffixId
			&& A.Tier == B.Tier
			&& A.Magnitude == B.Magnitude
			&& A.Unit == B.Unit;
	}

	bool StatsEqual(const FGameXXKCharacterStats& A, const FGameXXKCharacterStats& B)
	{
		return A.MaxHealth == B.MaxHealth
			&& A.MaxMana == B.MaxMana
			&& A.Attack == B.Attack
			&& A.Defense == B.Defense
			&& A.Speed == B.Speed;
	}

	bool ValidateAffixRoll(
		const FGameXXKEquipmentAffixRoll& Roll,
		const EGameXXKEquipmentSet EquipmentSet,
		const EGameXXKEquipmentQuality EquipmentQuality,
		TSet<EGameXXKEquipmentModifierKind>& InOutModifierKinds,
		FString* OutError)
	{
		const FGameXXKAffixDefinition* Affix = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
		if (!Affix)
		{
			SetError(OutError, FString::Printf(TEXT("Unknown equipment affix %s."), *Roll.AffixId.ToString()));
			return false;
		}
		if (Affix->Set != EGameXXKEquipmentSet::Invalid && Affix->Set != EquipmentSet)
		{
			SetError(OutError, TEXT("Equipment contains an affix from another set."));
			return false;
		}
		if (!IsValidTier(Roll.Tier)
			|| !IsValidQuality(EquipmentQuality)
			|| static_cast<uint8>(Roll.Tier) > static_cast<uint8>(EquipmentQuality)
			|| Roll.Unit != Affix->Unit)
		{
			SetError(OutError, TEXT("Equipment affix tier exceeds its quality or its unit is invalid."));
			return false;
		}
		if (InOutModifierKinds.Contains(Affix->ModifierKind))
		{
			SetError(OutError, TEXT("Equipment affix modifier kinds must be unique."));
			return false;
		}
		const FGameXXKAffixMagnitudeRange Range = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.Unit, Roll.Tier);
		if (Range.Minimum <= 0 || Range.Maximum < Range.Minimum
			|| Roll.Magnitude < Range.Minimum || Roll.Magnitude > Range.Maximum)
		{
			SetError(OutError, TEXT("Equipment affix magnitude is outside its catalog tier range."));
			return false;
		}
		InOutModifierKinds.Add(Affix->ModifierKind);
		return true;
	}

	bool IsRosterOwner(const FGameXXKCompanionRosterState& Roster, const FName CharacterId)
	{
		if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
		{
			return true;
		}
		return Roster.PermanentCompanions.ContainsByPredicate(
			[CharacterId](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == CharacterId;
			});
	}

	EGameXXKEquipmentOwnerKind OwnerKindForCharacter(const FName CharacterId)
	{
		return CharacterId == FGameXXKEquipmentRules::HeroCharacterId()
			? EGameXXKEquipmentOwnerKind::Hero
			: EGameXXKEquipmentOwnerKind::PermanentCompanion;
	}

	FGameXXKEquipmentTransactionResult MakeFailure(const EGameXXKEquipmentTransactionError Error)
	{
		FGameXXKEquipmentTransactionResult Result;
		Result.Error = Error;
		Result.Message = FGameXXKEquipmentRules::GetTransactionErrorMessage(Error);
		Result.bConfirmationRequired = Error == EGameXXKEquipmentTransactionError::ConfirmationRequired;
		return Result;
	}

	FGameXXKEquipmentTransactionResult MakeSuccess(TArray<FName> AffectedInstanceIds = {})
	{
		FGameXXKEquipmentTransactionResult Result;
		Result.bSucceeded = true;
		Result.Error = EGameXXKEquipmentTransactionError::None;
		Result.AffectedInstanceIds = MoveTemp(AffectedInstanceIds);
		return Result;
	}

	int32 AddClamped(const int32 A, const int32 B)
	{
		return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(A) + B, MIN_int32, MAX_int32));
	}

	int32 ScaleFloor(const int32 Value, const int32 BasisPoints)
	{
		return static_cast<int32>(FMath::Clamp<int64>(
			(static_cast<int64>(Value) * BasisPoints) / 10000,
			MIN_int32,
			MAX_int32));
	}

	void AddStats(FGameXXKCharacterStats& InOutStats, const FGameXXKCharacterStats& Addend)
	{
		InOutStats.MaxHealth = AddClamped(InOutStats.MaxHealth, Addend.MaxHealth);
		InOutStats.MaxMana = AddClamped(InOutStats.MaxMana, Addend.MaxMana);
		InOutStats.Attack = AddClamped(InOutStats.Attack, Addend.Attack);
		InOutStats.Defense = AddClamped(InOutStats.Defense, Addend.Defense);
		InOutStats.Speed = AddClamped(InOutStats.Speed, Addend.Speed);
	}

	FGameXXKCharacterStats SubtractStats(
		const FGameXXKCharacterStats& A,
		const FGameXXKCharacterStats& B)
	{
		FGameXXKCharacterStats Result;
		Result.MaxHealth = AddClamped(A.MaxHealth, -B.MaxHealth);
		Result.MaxMana = AddClamped(A.MaxMana, -B.MaxMana);
		Result.Attack = AddClamped(A.Attack, -B.Attack);
		Result.Defense = AddClamped(A.Defense, -B.Defense);
		Result.Speed = AddClamped(A.Speed, -B.Speed);
		return Result;
	}

	bool IsBaseStatKind(const EGameXXKEquipmentModifierKind Kind)
	{
		return Kind >= EGameXXKEquipmentModifierKind::MaxHealth
			&& Kind <= EGameXXKEquipmentModifierKind::Speed;
	}

	void AccumulateBaseStatModifier(
		TMap<EGameXXKEquipmentModifierKind, int32>& InOutBasisPoints,
		TMap<EGameXXKEquipmentModifierKind, int32>& InOutFlatCounts,
		const EGameXXKEquipmentModifierKind Kind,
		const int32 Magnitude,
		const EGameXXKEquipmentMagnitudeUnit Unit)
	{
		if (!IsBaseStatKind(Kind))
		{
			return;
		}
		TMap<EGameXXKEquipmentModifierKind, int32>& Target =
			Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints
			? InOutBasisPoints
			: InOutFlatCounts;
		Target.FindOrAdd(Kind) = AddClamped(Target.FindRef(Kind), Magnitude);
	}

	void ApplyOneStatModifier(
		FGameXXKCharacterStats& InOutStats,
		const EGameXXKEquipmentModifierKind Kind,
		const int32 Magnitude,
		const EGameXXKEquipmentMagnitudeUnit Unit)
	{
		int32* Value = nullptr;
		switch (Kind)
		{
		case EGameXXKEquipmentModifierKind::MaxHealth: Value = &InOutStats.MaxHealth; break;
		case EGameXXKEquipmentModifierKind::MaxMana: Value = &InOutStats.MaxMana; break;
		case EGameXXKEquipmentModifierKind::Attack: Value = &InOutStats.Attack; break;
		case EGameXXKEquipmentModifierKind::Defense: Value = &InOutStats.Defense; break;
		case EGameXXKEquipmentModifierKind::Speed: Value = &InOutStats.Speed; break;
		default: return;
		}
		const int32 Delta = Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints
			? ScaleFloor(*Value, Magnitude)
			: Magnitude;
		*Value = AddClamped(*Value, Delta);
	}

	FGameXXKCharacterStats ResolveItemBaseStats(
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Definition)
	{
		return Instance.ScalingRule == EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement
			? Instance.LegacyBaseStatSnapshot
			: Definition.BaseStatCoefficients.Resolve(Instance.ItemLevel);
	}

	FGameXXKCharacterStats ResolveItemCurrentStats(
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Definition)
	{
		FGameXXKCharacterStats Result = ResolveItemBaseStats(Instance, Definition);
		if (Instance.ScalingRule == EGameXXKEquipmentScalingRule::ModernPercentBase)
		{
			const int32 Factor = 10000 + 1000 * Instance.EnhancementLevel;
			Result.MaxHealth = ScaleFloor(Result.MaxHealth, Factor);
			Result.MaxMana = ScaleFloor(Result.MaxMana, Factor);
			Result.Attack = ScaleFloor(Result.Attack, Factor);
			Result.Defense = ScaleFloor(Result.Defense, Factor);
			Result.Speed = ScaleFloor(Result.Speed, Factor);
			// Flat enhancement growth so low-level gear still visibly gains
			// stats each enhancement (percent-only growth floors away on
			// single-digit base values).
			Result.MaxHealth = AddClamped(Result.MaxHealth, 2 * Instance.EnhancementLevel);
			Result.MaxMana = AddClamped(Result.MaxMana, 1 * Instance.EnhancementLevel);
			Result.Attack = AddClamped(Result.Attack, 1 * Instance.EnhancementLevel);
			Result.Defense = AddClamped(Result.Defense, 1 * Instance.EnhancementLevel);
			Result.Speed = AddClamped(Result.Speed, 1 * Instance.EnhancementLevel);
		}
		else
		{
			switch (Definition.Slot)
			{
			case EGameXXKEquipmentSlot::Weapon:
				Result.Attack = AddClamped(Result.Attack, Instance.EnhancementLevel);
				break;
			case EGameXXKEquipmentSlot::Armor:
				Result.Defense = AddClamped(Result.Defense, Instance.EnhancementLevel);
				break;
			case EGameXXKEquipmentSlot::Accessory:
				Result.Speed = AddClamped(Result.Speed, Instance.EnhancementLevel);
				break;
			default:
				break;
			}
		}
		return Result;
	}

	EGameXXKEquipmentModifierKind ModifierKindForBonus(const EGameXXKEquipmentSetBonusKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKEquipmentSetBonusKind::PoJunDirectDamage: return EGameXXKEquipmentModifierKind::DirectDamage;
		case EGameXXKEquipmentSetBonusKind::PoJunMultiHitArmorBreak: return EGameXXKEquipmentModifierKind::ArmorBreakStacks;
		case EGameXXKEquipmentSetBonusKind::PoJunFirstAttackFollowUp: return EGameXXKEquipmentModifierKind::FirstAttackDamage;
		case EGameXXKEquipmentSetBonusKind::XuanJiaArmorGain: return EGameXXKEquipmentModifierKind::ArmorGain;
		case EGameXXKEquipmentSetBonusKind::XuanJiaArmorRetentionCounter: return EGameXXKEquipmentModifierKind::CounterDamage;
		case EGameXXKEquipmentSetBonusKind::XuanJiaTeamGuard: return EGameXXKEquipmentModifierKind::GuardReduction;
		case EGameXXKEquipmentSetBonusKind::QingNangHealingCleanse: return EGameXXKEquipmentModifierKind::Healing;
		case EGameXXKEquipmentSetBonusKind::QingNangCleanseOverheal: return EGameXXKEquipmentModifierKind::OverhealConversion;
		case EGameXXKEquipmentSetBonusKind::QingNangTeamHealEnergy: return EGameXXKEquipmentModifierKind::SharedEnergy;
		case EGameXXKEquipmentSetBonusKind::ZhuiFengSpeedOpeningDraw: return EGameXXKEquipmentModifierKind::Draw;
		case EGameXXKEquipmentSetBonusKind::ZhuiFengLowCostEnergy: return EGameXXKEquipmentModifierKind::SharedEnergy;
		case EGameXXKEquipmentSetBonusKind::ZhuiFengComboFreeCard: return EGameXXKEquipmentModifierKind::TemporaryCostReduction;
		case EGameXXKEquipmentSetBonusKind::ShiGuDamageOverTimeStacks: return EGameXXKEquipmentModifierKind::DamageOverTime;
		case EGameXXKEquipmentSetBonusKind::ShiGuMixedDamageOverTime: return EGameXXKEquipmentModifierKind::DamageOverTime;
		case EGameXXKEquipmentSetBonusKind::ShiGuExtraDamageOverTimeTick: return EGameXXKEquipmentModifierKind::DamageOverTime;
		case EGameXXKEquipmentSetBonusKind::ShanHeTerrainPower: return EGameXXKEquipmentModifierKind::TerrainPower;
		case EGameXXKEquipmentSetBonusKind::ShanHeTerrainCardFormation: return EGameXXKEquipmentModifierKind::TerrainCostReduction;
		case EGameXXKEquipmentSetBonusKind::ShanHeTeamFormationCore: return EGameXXKEquipmentModifierKind::TeamTerrainPower;
		case EGameXXKEquipmentSetBonusKind::PoJunChargeDraw: return EGameXXKEquipmentModifierKind::BladeChargeDraw;
		case EGameXXKEquipmentSetBonusKind::PoJunFinishStoresCharge: return EGameXXKEquipmentModifierKind::BladeStoredCharge;
		case EGameXXKEquipmentSetBonusKind::PoJunOpeningFinishReplay: return EGameXXKEquipmentModifierKind::BladeOpeningReplay;
		case EGameXXKEquipmentSetBonusKind::QingNangHighCostDraw:
		case EGameXXKEquipmentSetBonusKind::QingNangHighCostBloodCycle:
		case EGameXXKEquipmentSetBonusKind::QingNangHighCostEnergyCycle:
			return EGameXXKEquipmentModifierKind::QingNangCycle;
		case EGameXXKEquipmentSetBonusKind::ShiGuCardTargetRot:
		case EGameXXKEquipmentSetBonusKind::ShiGuFirstDualDotExplosion:
		case EGameXXKEquipmentSetBonusKind::ShiGuFirstExplosionPreservesDots:
			return EGameXXKEquipmentModifierKind::ShiGuCycle;
		case EGameXXKEquipmentSetBonusKind::ZhuiFengPairDraw:
		case EGameXXKEquipmentSetBonusKind::ZhuiFengSecondCardEnergy:
		case EGameXXKEquipmentSetBonusKind::ZhuiFengFourthCardCycle:
			return EGameXXKEquipmentModifierKind::ZhuiFengCycle;
		default: return EGameXXKEquipmentModifierKind::Invalid;
		}
	}

	FGameXXKEquipmentActiveEffect MakeSetEffect(
		const FGameXXKEquipmentSetBonusDefinition& Definition,
		const FName SourceCharacterId)
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = Definition.Id;
		Effect.SourceCharacterId = SourceCharacterId;
		Effect.Set = Definition.Set;
		Effect.RequiredPieces = Definition.RequiredPieces;
		Effect.Scope = Definition.Scope;
		Effect.Hook = Definition.Hook;
		Effect.ModifierKind = ModifierKindForBonus(Definition.BonusKind);
		// The two-piece ZhuiFeng row has one projected passive stat and one
		// consumable battle-start event. Keep the already-projected Speed bonus
		// out of the event descriptor so downstream consumers cannot apply it twice.
		const bool bZhuiFengOpeningDraw =
			Definition.BonusKind == EGameXXKEquipmentSetBonusKind::ZhuiFengSpeedOpeningDraw;
		Effect.Magnitude = bZhuiFengOpeningDraw ? 1 : Definition.Value;
		Effect.Unit = bZhuiFengOpeningDraw
			? EGameXXKEquipmentMagnitudeUnit::FlatCount
			: Definition.Unit;
		Effect.MaxTriggersPerRound = Definition.TriggersPerRound;
		return Effect;
	}

	FGameXXKEquipmentTransactionResult BuildEquippedCandidate(
		const FGameXXKEquipmentCollectionState& Collection,
		const FName CharacterId,
		const EGameXXKEquipmentSlot Slot,
		const FName InstanceId,
		FGameXXKEquipmentCollectionState& OutCandidate)
	{
		if (!FGameXXKEquipmentRules::ValidateCollectionState(Collection))
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		}
		if (CharacterId.IsNone())
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::InvalidOwner);
		}
		if (!IsValidSlot(Slot) || InstanceId.IsNone())
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
		}
		const FGameXXKEquipmentInstance* Existing = FGameXXKEquipmentRules::FindInstance(Collection, InstanceId);
		if (!Existing)
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::InstanceMissing);
		}
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Existing->BaseEquipmentId);
		if (!Definition)
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::DefinitionMissing);
		}
		if (Definition->Slot != Slot)
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::SlotMismatch);
		}
		const int32 WarehouseIndex = Collection.WarehouseInstanceIds.IndexOfByKey(InstanceId);
		if (WarehouseIndex == INDEX_NONE)
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
		}

		OutCandidate = Collection;
		FGameXXKEquipmentLoadout& Loadout = OutCandidate.CharacterLoadouts.FindOrAdd(CharacterId);
		FName* EquippedSlot = GetSlotPtr(Loadout, Slot);
		const FName DisplacedInstanceId = *EquippedSlot;
		OutCandidate.WarehouseInstanceIds.RemoveAt(WarehouseIndex);
		if (!DisplacedInstanceId.IsNone())
		{
			OutCandidate.WarehouseInstanceIds.Insert(DisplacedInstanceId, WarehouseIndex);
			FGameXXKEquipmentInstance* Displaced = OutCandidate.EquipmentInstances.FindByPredicate(
				[DisplacedInstanceId](const FGameXXKEquipmentInstance& Instance)
				{
					return Instance.InstanceId == DisplacedInstanceId;
				});
			if (!Displaced)
			{
				return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
			}
			Displaced->OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
			Displaced->OwnerCharacterId = NAME_None;
		}
		*EquippedSlot = InstanceId;
		FGameXXKEquipmentInstance* Incoming = OutCandidate.EquipmentInstances.FindByPredicate(
			[InstanceId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
		if (!Incoming)
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		}
		Incoming->OwnerKind = OwnerKindForCharacter(CharacterId);
		Incoming->OwnerCharacterId = CharacterId;
		if (OutCandidate.bLegacyWarehouseOverflow && OutCandidate.WarehouseInstanceIds.Num() <= FGameXXKEquipmentRules::WarehouseCapacity)
		{
			OutCandidate.bLegacyWarehouseOverflow = false;
		}
		if (!FGameXXKEquipmentRules::ValidateCollectionState(OutCandidate))
		{
			return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
		}
		TArray<FName> Affected = {InstanceId};
		if (!DisplacedInstanceId.IsNone())
		{
			Affected.Add(DisplacedInstanceId);
		}
		return MakeSuccess(MoveTemp(Affected));
	}
}

FName FGameXXKEquipmentRules::HeroCharacterId()
{
	static const FName PlayerId(TEXT("Player"));
	return PlayerId;
}

const FGameXXKEquipmentInstance* FGameXXKEquipmentRules::FindInstance(
	const FGameXXKEquipmentCollectionState& Collection,
	const FName InstanceId)
{
	if (InstanceId.IsNone())
	{
		return nullptr;
	}
	return Collection.EquipmentInstances.FindByPredicate(
		[InstanceId](const FGameXXKEquipmentInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
}

int32 FGameXXKEquipmentRules::CountWarehouseItems(const FGameXXKEquipmentCollectionState& Collection)
{
	return Collection.WarehouseInstanceIds.Num();
}

bool FGameXXKEquipmentRules::HasWarehouseCapacity(
	const FGameXXKEquipmentCollectionState& Collection,
	const int32 RequiredSlots)
{
	return RequiredSlots >= 0
		&& !Collection.bLegacyWarehouseOverflow
		&& Collection.WarehouseInstanceIds.Num() <= WarehouseCapacity
		&& RequiredSlots <= WarehouseCapacity - Collection.WarehouseInstanceIds.Num();
}

bool FGameXXKEquipmentRules::ValidateCollectionState(
	const FGameXXKEquipmentCollectionState& Collection,
	FString* OutError)
{
	SetError(OutError, FString());
	if (Collection.EquipmentSchemaVersion != 1 || Collection.CollectionSeed == 0
		|| Collection.NextInstanceOrdinal < 0 || Collection.NextReforgeOrdinal < 0
		|| Collection.RefinementSand < 0)
	{
		SetError(OutError, TEXT("Equipment collection metadata is invalid."));
		return false;
	}

	TMap<FName, const FGameXXKEquipmentInstance*> InstancesById;
	for (const FGameXXKEquipmentInstance& Instance : Collection.EquipmentInstances)
	{
		if (Instance.InstanceId.IsNone() || InstancesById.Contains(Instance.InstanceId))
		{
			SetError(OutError, TEXT("Equipment instance IDs must be populated and unique."));
			return false;
		}
		InstancesById.Add(Instance.InstanceId, &Instance);

		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
		if (!Definition)
		{
			SetError(OutError, FString::Printf(TEXT("Equipment instance %s has no catalog definition."), *Instance.InstanceId.ToString()));
			return false;
		}
		if (Instance.ItemLevel < 1 || Instance.ItemLevel > MaxItemLevel
			|| Instance.EnhancementLevel < 0 || Instance.EnhancementLevel > MaxEnhancementLevel
			|| !IsValidQuality(Instance.Quality)
			|| Instance.ScalingRule != Definition->ScalingRule)
		{
			SetError(OutError, TEXT("Equipment instance level, quality, enhancement, or scaling is invalid."));
			return false;
		}

		if (Definition->Set == EGameXXKEquipmentSet::Legacy)
		{
			if (Instance.Quality != EGameXXKEquipmentQuality::Common
				|| !Instance.RolledAffixes.IsEmpty()
				|| !StatsEqual(Instance.LegacyBaseStatSnapshot, Definition->LegacyBaseStatSnapshot))
			{
				SetError(OutError, TEXT("Legacy equipment must preserve its Common quality, empty affixes, and base snapshot."));
				return false;
			}
		}
		else
		{
			if (!IsModernSet(Definition->Set)
				|| Instance.RolledAffixes.Num() != ExpectedAffixCount(Instance.Quality))
			{
				SetError(OutError, TEXT("Modern equipment must have exactly one, two, or three affixes for its quality."));
				return false;
			}
			TSet<EGameXXKEquipmentModifierKind> ModifierKinds;
			for (const FGameXXKEquipmentAffixRoll& Roll : Instance.RolledAffixes)
			{
				if (!ValidateAffixRoll(Roll, Definition->Set, Instance.Quality, ModifierKinds, OutError))
				{
					return false;
				}
			}
		}
	}

	TSet<FName> IndexedIds;
	bool bAllOverflowItemsAreLegacy = true;
	for (const FName InstanceId : Collection.WarehouseInstanceIds)
	{
		const FGameXXKEquipmentInstance* const* InstancePtr = InstancesById.Find(InstanceId);
		if (InstanceId.IsNone() || !InstancePtr)
		{
			SetError(OutError, TEXT("Warehouse references a missing equipment instance."));
			return false;
		}
		if (IndexedIds.Contains(InstanceId))
		{
			SetError(OutError, TEXT("An equipment instance appears in more than one authoritative index."));
			return false;
		}
		IndexedIds.Add(InstanceId);
		const FGameXXKEquipmentInstance& Instance = **InstancePtr;
		if (Instance.OwnerKind != EGameXXKEquipmentOwnerKind::Warehouse || !Instance.OwnerCharacterId.IsNone())
		{
			SetError(OutError, TEXT("Warehouse owner redundancy does not match its authoritative index."));
			return false;
		}
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
		bAllOverflowItemsAreLegacy = bAllOverflowItemsAreLegacy && Definition && Definition->Set == EGameXXKEquipmentSet::Legacy;
	}

	for (const TPair<FName, FGameXXKEquipmentLoadout>& Pair : Collection.CharacterLoadouts)
	{
		if (Pair.Key.IsNone())
		{
			SetError(OutError, TEXT("Equipment loadout owner ID cannot be empty."));
			return false;
		}
		for (const EGameXXKEquipmentSlot Slot : AllSlots)
		{
			const FName InstanceId = *GetSlotPtr(Pair.Value, Slot);
			if (InstanceId.IsNone())
			{
				continue;
			}
			const FGameXXKEquipmentInstance* const* InstancePtr = InstancesById.Find(InstanceId);
			if (!InstancePtr)
			{
				SetError(OutError, TEXT("Equipment loadout references a missing instance."));
				return false;
			}
			if (IndexedIds.Contains(InstanceId))
			{
				SetError(OutError, TEXT("An equipment instance appears in more than one authoritative index."));
				return false;
			}
			IndexedIds.Add(InstanceId);
			const FGameXXKEquipmentInstance& Instance = **InstancePtr;
			const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
			if (!Definition || Definition->Slot != Slot)
			{
				SetError(OutError, TEXT("Equipment catalog slot does not match its loadout slot."));
				return false;
			}
			if (Instance.OwnerKind != OwnerKindForCharacter(Pair.Key) || Instance.OwnerCharacterId != Pair.Key)
			{
				SetError(OutError, TEXT("Equipped owner redundancy does not match its authoritative loadout."));
				return false;
			}
		}
	}

	if (IndexedIds.Num() != Collection.EquipmentInstances.Num())
	{
		SetError(OutError, TEXT("Every equipment instance must have exactly one authoritative location."));
		return false;
	}

	const int32 WarehouseCount = Collection.WarehouseInstanceIds.Num();
	if (WarehouseCount > WarehouseCapacity)
	{
		if (!Collection.bLegacyWarehouseOverflow || !bAllOverflowItemsAreLegacy)
		{
			SetError(OutError, TEXT("Only flagged all-legacy migration data may exceed warehouse capacity."));
			return false;
		}
	}
	else if (Collection.bLegacyWarehouseOverflow)
	{
		SetError(OutError, TEXT("Legacy warehouse overflow flag must clear at normal capacity."));
		return false;
	}

	const FGameXXKPendingEquipmentReforge& Pending = Collection.PendingReforge;
	if (!Pending.bActive)
	{
		if (!Pending.InstanceId.IsNone() || Pending.AffixIndex != INDEX_NONE
			|| !Pending.OriginalAffix.AffixId.IsNone() || Pending.OriginalAffix.Tier != EGameXXKAffixTier::Invalid
			|| Pending.OriginalAffix.Magnitude != 0 || Pending.OriginalAffix.Unit != EGameXXKEquipmentMagnitudeUnit::Invalid
			|| !Pending.CandidateAffix.AffixId.IsNone() || Pending.CandidateAffix.Tier != EGameXXKAffixTier::Invalid
			|| Pending.CandidateAffix.Magnitude != 0 || Pending.CandidateAffix.Unit != EGameXXKEquipmentMagnitudeUnit::Invalid
			|| Pending.PaidRefinementSand != 0 || Pending.ConsumedReforgeOrdinal != INDEX_NONE)
		{
			SetError(OutError, TEXT("Inactive pending reforge data must be empty."));
			return false;
		}
	}
	else
	{
		const FGameXXKEquipmentInstance* Instance = FindInstance(Collection, Pending.InstanceId);
		if (!Instance || !Instance->RolledAffixes.IsValidIndex(Pending.AffixIndex)
			|| Pending.PaidRefinementSand <= 0 || Pending.ConsumedReforgeOrdinal < 0
			|| Pending.ConsumedReforgeOrdinal >= Collection.NextReforgeOrdinal
			|| !AffixRollsEqual(Instance->RolledAffixes[Pending.AffixIndex], Pending.OriginalAffix))
		{
			SetError(OutError, TEXT("Pending equipment reforge references stale or invalid data."));
			return false;
		}
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
		if (!Definition || Definition->Set == EGameXXKEquipmentSet::Legacy)
		{
			SetError(OutError, TEXT("Legacy or missing equipment cannot hold a pending reforge."));
			return false;
		}
		TSet<EGameXXKEquipmentModifierKind> ModifierKinds;
		for (int32 AffixIndex = 0; AffixIndex < Instance->RolledAffixes.Num(); ++AffixIndex)
		{
			const FGameXXKEquipmentAffixRoll& Roll = AffixIndex == Pending.AffixIndex
				? Pending.CandidateAffix
				: Instance->RolledAffixes[AffixIndex];
			if (!ValidateAffixRoll(Roll, Definition->Set, Instance->Quality, ModifierKinds, OutError))
			{
				return false;
			}
		}
	}
	return true;
}

bool FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
	const FGameXXKEquipmentCollectionState& Collection,
	const FGameXXKCompanionRosterState& Roster,
	FString* OutError)
{
	if (!ValidateCollectionState(Collection, OutError))
	{
		return false;
	}
	if (Roster.PermanentCompanions.Num() > FGameXXKCompanionRules::MaxPermanentCompanions)
	{
		SetError(OutError, TEXT("Permanent companion roster exceeds the supported maximum."));
		return false;
	}
	TSet<FName> CompanionIds;
	for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
	{
		if (Companion.InstanceId.IsNone() || CompanionIds.Contains(Companion.InstanceId)
			|| Companion.InstanceId == HeroCharacterId())
		{
			SetError(OutError, TEXT("Permanent companion equipment owner IDs must be populated and unique."));
			return false;
		}
		CompanionIds.Add(Companion.InstanceId);
	}
	for (const TPair<FName, FGameXXKEquipmentLoadout>& Pair : Collection.CharacterLoadouts)
	{
		if (Pair.Key != HeroCharacterId() && !CompanionIds.Contains(Pair.Key))
		{
			SetError(OutError, TEXT("Equipment owner is not Player or a current permanent companion."));
			return false;
		}
	}
	return true;
}

bool FGameXXKEquipmentRules::CreateRolledInstance(
	FGameXXKEquipmentCollectionState& InOutCollection,
	const FGameXXKEquipmentCreateRequest& Request,
	FName& OutInstanceId,
	FString* OutError)
{
	OutInstanceId = NAME_None;
	SetError(OutError, FString());
	FString ValidationError;
	if (!ValidateCollectionState(InOutCollection, &ValidationError))
	{
		SetError(OutError, FString::Printf(TEXT("Collection is invalid before creation: %s"), *ValidationError));
		return false;
	}
	if (!IsModernSet(Request.Set) || !IsValidQuality(Request.Quality)
		|| Request.ItemLevel < 1 || Request.ItemLevel > MaxItemLevel
		|| (Request.bForceSlot && !IsValidSlot(Request.ForcedSlot)))
	{
		SetError(OutError, TEXT("Equipment creation request is invalid."));
		return false;
	}
	if (InOutCollection.NextInstanceOrdinal == MAX_int32)
	{
		SetError(OutError, TEXT("Equipment instance ordinal is exhausted."));
		return false;
	}
	if (!HasWarehouseCapacity(InOutCollection))
	{
		SetError(OutError, TEXT("Equipment warehouse is full."));
		return false;
	}

	EGameXXKEquipmentSlot Slot = Request.ForcedSlot;
	if (!Request.bForceSlot)
	{
		FRandomStream SlotStream(static_cast<int32>(BuildRollSeed(InOutCollection, Request, EGameXXKEquipmentSlot::Invalid)));
		Slot = static_cast<EGameXXKEquipmentSlot>(SlotStream.RandRange(1, 6));
	}
	const FGameXXKEquipmentDefinition* EquipmentDefinition = FindPackageDefinition(Request.Set, Slot);
	if (!EquipmentDefinition)
	{
		SetError(OutError, TEXT("Equipment package definition is missing."));
		return false;
	}

	const FName InstanceId(*FString::Printf(
		TEXT("EquipmentInstance.%08X.%d"),
		static_cast<uint32>(InOutCollection.CollectionSeed),
		InOutCollection.NextInstanceOrdinal));
	if (FindInstance(InOutCollection, InstanceId))
	{
		SetError(OutError, TEXT("Stable equipment instance ID already exists."));
		return false;
	}

	const uint32 StreamSeed = BuildRollSeed(InOutCollection, Request, Slot);
	FRandomStream Stream(static_cast<int32>(StreamSeed));
	TArray<const FGameXXKAffixDefinition*> Candidates;
	for (const FGameXXKAffixDefinition& Definition : FGameXXKAffixCatalog::GetUniversalDefinitions())
	{
		Candidates.Add(&Definition);
	}
	for (const FGameXXKAffixDefinition& Definition : FGameXXKAffixCatalog::GetSetDefinitions(Request.Set))
	{
		Candidates.Add(&Definition);
	}

	FGameXXKEquipmentInstance Instance;
	Instance.InstanceId = InstanceId;
	Instance.BaseEquipmentId = EquipmentDefinition->Id;
	Instance.ItemLevel = Request.ItemLevel;
	Instance.Quality = Request.Quality;
	Instance.AcquisitionSeed = static_cast<int32>(StreamSeed);
	Instance.ScalingRule = EquipmentDefinition->ScalingRule;
	Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
	for (int32 AffixIndex = 0; AffixIndex < ExpectedAffixCount(Request.Quality); ++AffixIndex)
	{
		const int32 CandidateIndex = Stream.RandRange(0, Candidates.Num() - 1);
		const FGameXXKAffixDefinition* Definition = Candidates[CandidateIndex];
		Candidates.RemoveAt(CandidateIndex);

		FGameXXKEquipmentAffixRoll Roll;
		Roll.AffixId = Definition->Id;
		Roll.Tier = RollTier(Stream, Request.Quality);
		Roll.Unit = Definition->Unit;
		const FGameXXKAffixMagnitudeRange Range = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.Unit, Roll.Tier);
		Roll.Magnitude = Stream.RandRange(Range.Minimum, Range.Maximum);
		Instance.RolledAffixes.Add(Roll);
	}

	FGameXXKEquipmentCollectionState Candidate = InOutCollection;
	Candidate.EquipmentInstances.Add(MoveTemp(Instance));
	Candidate.WarehouseInstanceIds.Add(InstanceId);
	if (!ValidateCollectionState(Candidate, &ValidationError))
	{
		SetError(OutError, FString::Printf(TEXT("Created equipment failed validation: %s"), *ValidationError));
		return false;
	}
	++Candidate.NextInstanceOrdinal;
	InOutCollection = MoveTemp(Candidate);
	OutInstanceId = InstanceId;
	return true;
}

FGameXXKEquipmentTransactionResult FGameXXKEquipmentRules::EquipInstance(
	FGameXXKEquipmentCollectionState& InOutCollection,
	const FGameXXKCompanionRosterState& Roster,
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	const FName InstanceId)
{
	if (!ValidateCollectionAgainstRoster(InOutCollection, Roster))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	if (!IsRosterOwner(Roster, CharacterId))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::InvalidOwner);
	}
	FGameXXKEquipmentCollectionState Candidate;
	const FGameXXKEquipmentTransactionResult CoreResult = BuildEquippedCandidate(
		InOutCollection,
		CharacterId,
		Slot,
		InstanceId,
		Candidate);
	if (!CoreResult.bSucceeded)
	{
		return CoreResult;
	}
	if (!ValidateCollectionAgainstRoster(Candidate, Roster))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	InOutCollection = MoveTemp(Candidate);
	return CoreResult;
}

FGameXXKEquipmentTransactionResult FGameXXKEquipmentRules::UnequipInstance(
	FGameXXKEquipmentCollectionState& InOutCollection,
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot)
{
	if (!ValidateCollectionState(InOutCollection))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	if (CharacterId.IsNone())
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::InvalidOwner);
	}
	if (!IsValidSlot(Slot))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::InvalidRequest);
	}
	const FGameXXKEquipmentLoadout* ExistingLoadout = InOutCollection.CharacterLoadouts.Find(CharacterId);
	if (!ExistingLoadout || GetSlotPtr(*ExistingLoadout, Slot)->IsNone())
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::InstanceMissing);
	}
	if (!HasWarehouseCapacity(InOutCollection))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::WarehouseFull);
	}

	FGameXXKEquipmentCollectionState Candidate = InOutCollection;
	FGameXXKEquipmentLoadout* Loadout = Candidate.CharacterLoadouts.Find(CharacterId);
	FName* EquippedSlot = GetSlotPtr(*Loadout, Slot);
	const FName InstanceId = *EquippedSlot;
	*EquippedSlot = NAME_None;
	Candidate.WarehouseInstanceIds.Add(InstanceId);
	FGameXXKEquipmentInstance* Instance = Candidate.EquipmentInstances.FindByPredicate(
		[InstanceId](const FGameXXKEquipmentInstance& CandidateInstance)
		{
			return CandidateInstance.InstanceId == InstanceId;
		});
	Instance->OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
	Instance->OwnerCharacterId = NAME_None;
	if (IsLoadoutEmpty(*Loadout))
	{
		Candidate.CharacterLoadouts.Remove(CharacterId);
	}
	if (!ValidateCollectionState(Candidate))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	InOutCollection = MoveTemp(Candidate);
	return MakeSuccess({InstanceId});
}

FGameXXKEquipmentTransactionResult FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(
	FGameXXKEquipmentCollectionState& InOutCollection,
	const FName CharacterId)
{
	if (!ValidateCollectionState(InOutCollection))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	if (CharacterId.IsNone())
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::InvalidOwner);
	}
	const FGameXXKEquipmentLoadout* ExistingLoadout = InOutCollection.CharacterLoadouts.Find(CharacterId);
	if (!ExistingLoadout)
	{
		return MakeSuccess();
	}
	TArray<FName> ReturningIds;
	for (const EGameXXKEquipmentSlot Slot : AllSlots)
	{
		const FName InstanceId = *GetSlotPtr(*ExistingLoadout, Slot);
		if (!InstanceId.IsNone())
		{
			ReturningIds.Add(InstanceId);
		}
	}
	if (ReturningIds.IsEmpty())
	{
		FGameXXKEquipmentCollectionState Candidate = InOutCollection;
		Candidate.CharacterLoadouts.Remove(CharacterId);
		InOutCollection = MoveTemp(Candidate);
		return MakeSuccess();
	}
	if (!HasWarehouseCapacity(InOutCollection, ReturningIds.Num()))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::WarehouseFull);
	}

	FGameXXKEquipmentCollectionState Candidate = InOutCollection;
	for (const FName InstanceId : ReturningIds)
	{
		Candidate.WarehouseInstanceIds.Add(InstanceId);
		FGameXXKEquipmentInstance* Instance = Candidate.EquipmentInstances.FindByPredicate(
			[InstanceId](const FGameXXKEquipmentInstance& CandidateInstance)
			{
				return CandidateInstance.InstanceId == InstanceId;
			});
		Instance->OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
		Instance->OwnerCharacterId = NAME_None;
	}
	Candidate.CharacterLoadouts.Remove(CharacterId);
	if (!ValidateCollectionState(Candidate))
	{
		return MakeFailure(EGameXXKEquipmentTransactionError::CollectionInvalid);
	}
	InOutCollection = MoveTemp(Candidate);
	return MakeSuccess(MoveTemp(ReturningIds));
}

bool FGameXXKEquipmentRules::BuildLoadoutSnapshot(
	const FGameXXKEquipmentCollectionState& Collection,
	const FName CharacterId,
	const FGameXXKCharacterStats& BareStats,
	FGameXXKEquipmentLoadoutSnapshot& OutSnapshot,
	FString* OutError)
{
	OutSnapshot = FGameXXKEquipmentLoadoutSnapshot();
	SetError(OutError, FString());
	if (CharacterId.IsNone() || !BareStats.IsValidProjectionInput())
	{
		SetError(OutError, TEXT("Equipment projection requires a character ID and valid naked stats."));
		return false;
	}
	if (!ValidateCollectionState(Collection, OutError))
	{
		return false;
	}

	OutSnapshot.CharacterId = CharacterId;
	OutSnapshot.BareStats = BareStats;
	OutSnapshot.AttributesBeforeRoute = BareStats;
	const FGameXXKEquipmentLoadout* Loadout = Collection.CharacterLoadouts.Find(CharacterId);
	if (!Loadout)
	{
		return true;
	}

	TMap<EGameXXKEquipmentModifierKind, EGameXXKEquipmentMagnitudeUnit> UniversalUnits;
	TMap<EGameXXKEquipmentModifierKind, EGameXXKEquipmentMagnitudeUnit> SetUnits;
	TMap<EGameXXKEquipmentSet, int32> ScoresBySet;
	struct FAffixEffectAggregate
	{
		EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;
		EGameXXKEquipmentModifierKind Kind = EGameXXKEquipmentModifierKind::Invalid;
		EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;
		int32 Magnitude = 0;
	};
	TMap<int32, FAffixEffectAggregate> SetAffixEffects;

	for (const EGameXXKEquipmentSlot Slot : AllSlots)
	{
		const FName InstanceId = *GetSlotPtr(*Loadout, Slot);
		if (InstanceId.IsNone())
		{
			continue;
		}
		const FGameXXKEquipmentInstance* Instance = FindInstance(Collection, InstanceId);
		const FGameXXKEquipmentDefinition* Definition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;
		if (!Instance || !Definition || Definition->Slot != Slot)
		{
			SetError(OutError, TEXT("Equipment loadout contains an unresolved slot."));
			OutSnapshot = FGameXXKEquipmentLoadoutSnapshot();
			return false;
		}

		AddStats(OutSnapshot.EnhancedEquipmentBaseStats, ResolveItemCurrentStats(*Instance, *Definition));
		if (IsModernSet(Definition->Set))
		{
			OutSnapshot.SetPieceCounts.FindOrAdd(Definition->Set) += 1;
			ScoresBySet.FindOrAdd(Definition->Set) +=
				static_cast<int32>(static_cast<uint8>(Instance->Quality)) * 10 + Instance->EnhancementLevel;
		}

		for (const FGameXXKEquipmentAffixRoll& Roll : Instance->RolledAffixes)
		{
			const FGameXXKAffixDefinition* AffixDefinition = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
			if (!AffixDefinition)
			{
				SetError(OutError, TEXT("Equipment projection cannot resolve an affix."));
				OutSnapshot = FGameXXKEquipmentLoadoutSnapshot();
				return false;
			}
			if (AffixDefinition->Set == EGameXXKEquipmentSet::Invalid)
			{
				OutSnapshot.UniversalModifiers.FindOrAdd(AffixDefinition->ModifierKind) = AddClamped(
					OutSnapshot.UniversalModifiers.FindRef(AffixDefinition->ModifierKind),
					Roll.Magnitude);
				UniversalUnits.Add(AffixDefinition->ModifierKind, Roll.Unit);
			}
			else
			{
				OutSnapshot.SetModifiers.FindOrAdd(AffixDefinition->ModifierKind) = AddClamped(
					OutSnapshot.SetModifiers.FindRef(AffixDefinition->ModifierKind),
					Roll.Magnitude);
				SetUnits.Add(AffixDefinition->ModifierKind, Roll.Unit);
				if (!IsBaseStatKind(AffixDefinition->ModifierKind))
				{
					const int32 Key = (static_cast<int32>(AffixDefinition->Set) << 8)
						| static_cast<int32>(AffixDefinition->ModifierKind);
					FAffixEffectAggregate& Aggregate = SetAffixEffects.FindOrAdd(Key);
					Aggregate.Set = AffixDefinition->Set;
					Aggregate.Kind = AffixDefinition->ModifierKind;
					Aggregate.Unit = Roll.Unit;
					Aggregate.Magnitude = AddClamped(Aggregate.Magnitude, Roll.Magnitude);
				}
			}
		}
	}

	AddStats(OutSnapshot.AttributesBeforeRoute, OutSnapshot.EnhancedEquipmentBaseStats);
	TMap<EGameXXKEquipmentModifierKind, int32> EquipmentStatBasisPoints;
	TMap<EGameXXKEquipmentModifierKind, int32> EquipmentStatFlatCounts;
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : OutSnapshot.UniversalModifiers)
	{
		AccumulateBaseStatModifier(
			EquipmentStatBasisPoints,
			EquipmentStatFlatCounts,
			Pair.Key,
			Pair.Value,
			UniversalUnits.FindRef(Pair.Key));
	}
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : OutSnapshot.SetModifiers)
	{
		AccumulateBaseStatModifier(
			EquipmentStatBasisPoints,
			EquipmentStatFlatCounts,
			Pair.Key,
			Pair.Value,
			SetUnits.FindRef(Pair.Key));
	}

	TArray<int32> EffectKeys;
	SetAffixEffects.GetKeys(EffectKeys);
	EffectKeys.Sort();
	for (const int32 Key : EffectKeys)
	{
		const FAffixEffectAggregate& Aggregate = SetAffixEffects.FindChecked(Key);
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = FName(*FString::Printf(
			TEXT("EquipmentAffixAggregate.%d.%d"),
			static_cast<int32>(Aggregate.Set),
			static_cast<int32>(Aggregate.Kind)));
		Effect.SourceCharacterId = CharacterId;
		Effect.Set = Aggregate.Set;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Owner;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::Passive;
		Effect.ModifierKind = Aggregate.Kind;
		Effect.Magnitude = Aggregate.Magnitude;
		Effect.Unit = Aggregate.Unit;
		OutSnapshot.ActivePersonalEffects.Add(MoveTemp(Effect));
	}

	for (const FGameXXKEquipmentSetBonusDefinition& Definition : FGameXXKEquipmentSetCatalog::GetDefinitions())
	{
		if (OutSnapshot.SetPieceCounts.FindRef(Definition.Set) < Definition.RequiredPieces)
		{
			continue;
		}
		FGameXXKEquipmentActiveEffect Effect = MakeSetEffect(Definition, CharacterId);
		if (Definition.Scope == EGameXXKEquipmentSetBonusScope::Team)
		{
			OutSnapshot.CandidateTeamEffects.Add(MoveTemp(Effect));
			OutSnapshot.TeamEffectSourceScore = FMath::Max(
				OutSnapshot.TeamEffectSourceScore,
				ScoresBySet.FindRef(Definition.Set));
		}
		else
		{
			// ZhuiFeng's two-piece passive Speed is part of the single additive
			// equipment-stat percentage pass. Its separate active descriptor is Draw.
			if (Definition.BonusKind == EGameXXKEquipmentSetBonusKind::ZhuiFengSpeedOpeningDraw)
			{
				AccumulateBaseStatModifier(
					EquipmentStatBasisPoints,
					EquipmentStatFlatCounts,
					EGameXXKEquipmentModifierKind::Speed,
					Definition.Value,
					Definition.Unit);
			}
			else
			{
				AccumulateBaseStatModifier(
					EquipmentStatBasisPoints,
					EquipmentStatFlatCounts,
					Effect.ModifierKind,
					Effect.Magnitude,
					Effect.Unit);
			}
			OutSnapshot.ActivePersonalEffects.Add(MoveTemp(Effect));
		}
	}
	// All equipment percentages use the same naked + enhanced-base subtotal.
	// Applying one aggregate per stat prevents cross-layer compounding.
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : EquipmentStatBasisPoints)
	{
		ApplyOneStatModifier(
			OutSnapshot.AttributesBeforeRoute,
			Pair.Key,
			Pair.Value,
			EGameXXKEquipmentMagnitudeUnit::BasisPoints);
	}
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : EquipmentStatFlatCounts)
	{
		ApplyOneStatModifier(
			OutSnapshot.AttributesBeforeRoute,
			Pair.Key,
			Pair.Value,
			EGameXXKEquipmentMagnitudeUnit::FlatCount);
	}
	return true;
}

bool FGameXXKEquipmentRules::BuildTooltipSnapshot(
	const FGameXXKEquipmentCollectionState& Collection,
	const FName InstanceId,
	const FName CompareCharacterId,
	const FGameXXKCharacterStats& CompareBareStats,
	FGameXXKEquipmentTooltipSnapshot& OutSnapshot,
	FString* OutError)
{
	OutSnapshot = FGameXXKEquipmentTooltipSnapshot();
	SetError(OutError, FString());
	if (InstanceId.IsNone() || CompareCharacterId.IsNone() || !CompareBareStats.IsValidProjectionInput())
	{
		SetError(OutError, TEXT("Equipment tooltip requires an instance, character, and valid naked stats."));
		return false;
	}
	if (!ValidateCollectionState(Collection, OutError))
	{
		return false;
	}
	const FGameXXKEquipmentInstance* Instance = FindInstance(Collection, InstanceId);
	if (!Instance)
	{
		SetError(OutError, TEXT("Equipment tooltip instance is missing."));
		return false;
	}
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
	if (!Definition)
	{
		SetError(OutError, TEXT("Equipment tooltip definition is missing."));
		return false;
	}

	OutSnapshot.InstanceId = Instance->InstanceId;
	OutSnapshot.BaseEquipmentId = Instance->BaseEquipmentId;
	OutSnapshot.Set = Definition->Set;
	OutSnapshot.Slot = Definition->Slot;
	OutSnapshot.Quality = Instance->Quality;
	OutSnapshot.ItemLevel = Instance->ItemLevel;
	OutSnapshot.EnhancementLevel = Instance->EnhancementLevel;
	OutSnapshot.ItemBaseStats = ResolveItemBaseStats(*Instance, *Definition);
	OutSnapshot.ItemCurrentStats = ResolveItemCurrentStats(*Instance, *Definition);
	OutSnapshot.Affixes = Instance->RolledAffixes;

	FGameXXKEquipmentLoadoutSnapshot Current;
	if (!BuildLoadoutSnapshot(Collection, CompareCharacterId, CompareBareStats, Current, OutError))
	{
		OutSnapshot = FGameXXKEquipmentTooltipSnapshot();
		return false;
	}
	OutSnapshot.CurrentSetPieceCounts = Current.SetPieceCounts;
	OutSnapshot.CurrentCharacterStats = Current.AttributesBeforeRoute;

	FGameXXKEquipmentLoadoutSnapshot Candidate = Current;
	FGameXXKEquipmentCollectionState CandidateCollection;
	const FGameXXKEquipmentTransactionResult EquipResult = BuildEquippedCandidate(
		Collection,
		CompareCharacterId,
		Definition->Slot,
		InstanceId,
		CandidateCollection);
	OutSnapshot.EquipError = EquipResult.Error;
	if (EquipResult.bSucceeded
		&& !BuildLoadoutSnapshot(CandidateCollection, CompareCharacterId, CompareBareStats, Candidate, OutError))
	{
		OutSnapshot = FGameXXKEquipmentTooltipSnapshot();
		return false;
	}
	OutSnapshot.CandidateSetPieceCounts = Candidate.SetPieceCounts;
	OutSnapshot.CandidateCharacterStats = Candidate.AttributesBeforeRoute;
	OutSnapshot.CharacterStatDeltas = SubtractStats(
		OutSnapshot.CandidateCharacterStats,
		OutSnapshot.CurrentCharacterStats);
	return true;
}

TArray<FGameXXKEquipmentActiveEffect> FGameXXKEquipmentRules::ResolveTeamEffects(
	const TArray<FGameXXKEquipmentLoadoutSnapshot>& Snapshots)
{
	struct FChosenEffect
	{
		FGameXXKEquipmentActiveEffect Effect;
		int32 Score = 0;
	};
	TMap<EGameXXKEquipmentSet, FChosenEffect> ChosenBySet;
	for (const FGameXXKEquipmentLoadoutSnapshot& Snapshot : Snapshots)
	{
		for (const FGameXXKEquipmentActiveEffect& Effect : Snapshot.CandidateTeamEffects)
		{
			FChosenEffect* Existing = ChosenBySet.Find(Effect.Set);
			const bool bHigherTier = Existing
				&& Effect.RequiredPieces > Existing->Effect.RequiredPieces;
			const bool bHigherScoreAtSameTier = Existing
				&& Effect.RequiredPieces == Existing->Effect.RequiredPieces
				&& Snapshot.TeamEffectSourceScore > Existing->Score;
			const bool bWinsTie = Existing
				&& Effect.RequiredPieces == Existing->Effect.RequiredPieces
				&& Snapshot.TeamEffectSourceScore == Existing->Score
				&& Effect.SourceCharacterId.ToString() < Existing->Effect.SourceCharacterId.ToString();
			if (!Existing || bHigherTier || bHigherScoreAtSameTier || bWinsTie)
			{
				FChosenEffect Choice;
				Choice.Effect = Effect;
				Choice.Score = Snapshot.TeamEffectSourceScore;
				ChosenBySet.Add(Effect.Set, MoveTemp(Choice));
			}
		}
	}

	TArray<EGameXXKEquipmentSet> Sets;
	ChosenBySet.GetKeys(Sets);
	Sets.Sort([](const EGameXXKEquipmentSet A, const EGameXXKEquipmentSet B)
	{
		return static_cast<uint8>(A) < static_cast<uint8>(B);
	});
	TArray<FGameXXKEquipmentActiveEffect> Result;
	Result.Reserve(Sets.Num());
	for (const EGameXXKEquipmentSet Set : Sets)
	{
		Result.Add(ChosenBySet.FindChecked(Set).Effect);
	}
	return Result;
}

bool FGameXXKEquipmentRules::IsKnownActiveEffect(const FGameXXKEquipmentActiveEffect& Effect)
{
	if (const FGameXXKEquipmentSetBonusDefinition* Definition = FGameXXKEquipmentSetCatalog::FindDefinition(Effect.EffectId))
	{
		const bool bZhuiFengOpeningDraw =
			Definition->BonusKind == EGameXXKEquipmentSetBonusKind::ZhuiFengSpeedOpeningDraw;
		return Effect.Set == Definition->Set
			&& Effect.RequiredPieces == Definition->RequiredPieces
			&& Effect.Scope == Definition->Scope
			&& Effect.Hook == Definition->Hook
			&& Effect.ModifierKind == ModifierKindForBonus(Definition->BonusKind)
			&& Effect.Magnitude == (bZhuiFengOpeningDraw ? 1 : Definition->Value)
			&& Effect.Unit == (bZhuiFengOpeningDraw ? EGameXXKEquipmentMagnitudeUnit::FlatCount : Definition->Unit)
			&& Effect.MaxTriggersPerRound == Definition->TriggersPerRound;
	}

	if (Effect.Set < EGameXXKEquipmentSet::PoJun || Effect.Set > EGameXXKEquipmentSet::ShanHe
		|| IsBaseStatKind(Effect.ModifierKind)
		|| Effect.RequiredPieces != 0
		|| Effect.Scope != EGameXXKEquipmentSetBonusScope::Owner
		|| Effect.Hook != EGameXXKEquipmentSetBonusHook::Passive
		|| Effect.MaxTriggersPerRound != 0)
	{
		return false;
	}

	const FString ExpectedId = FString::Printf(
		TEXT("EquipmentAffixAggregate.%d.%d"),
		static_cast<int32>(Effect.Set),
		static_cast<int32>(Effect.ModifierKind));
	if (Effect.EffectId != FName(*ExpectedId))
	{
		return false;
	}

	return FGameXXKAffixCatalog::GetSetDefinitions(Effect.Set).ContainsByPredicate(
		[&Effect](const FGameXXKAffixDefinition& Definition)
		{
			return Definition.ModifierKind == Effect.ModifierKind && Definition.Unit == Effect.Unit;
		});
}

FGameXXKCharacterStats FGameXXKEquipmentRules::ApplyPostEquipmentModifiers(
	const FGameXXKCharacterStats& AttributesBeforeRoute,
	const TMap<EGameXXKEquipmentModifierKind, int32>& BasisPointModifiers,
	const TMap<EGameXXKEquipmentModifierKind, int32>& FlatCountModifiers)
{
	FGameXXKCharacterStats Result = AttributesBeforeRoute;
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : BasisPointModifiers)
	{
		ApplyOneStatModifier(Result, Pair.Key, Pair.Value, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
	}
	for (const TPair<EGameXXKEquipmentModifierKind, int32>& Pair : FlatCountModifiers)
	{
		ApplyOneStatModifier(Result, Pair.Key, Pair.Value, EGameXXKEquipmentMagnitudeUnit::FlatCount);
	}
	return Result;
}

FText FGameXXKEquipmentRules::GetTransactionErrorMessage(const EGameXXKEquipmentTransactionError Error)
{
	switch (Error)
	{
	case EGameXXKEquipmentTransactionError::None: return FText::GetEmpty();
	case EGameXXKEquipmentTransactionError::InvalidRequest: return NSLOCTEXT("GameXXKEquipment", "InvalidRequest", "装备操作请求无效");
	case EGameXXKEquipmentTransactionError::InstanceMissing: return NSLOCTEXT("GameXXKEquipment", "InstanceMissing", "当前装备实例已不存在");
	case EGameXXKEquipmentTransactionError::DefinitionMissing: return NSLOCTEXT("GameXXKEquipment", "DefinitionMissing", "装备定义不存在");
	case EGameXXKEquipmentTransactionError::CollectionInvalid: return NSLOCTEXT("GameXXKEquipment", "CollectionInvalid", "装备数据校验失败");
	case EGameXXKEquipmentTransactionError::WarehouseFull: return NSLOCTEXT("GameXXKEquipment", "WarehouseFull", "装备背包已满");
	case EGameXXKEquipmentTransactionError::InvalidOwner: return NSLOCTEXT("GameXXKEquipment", "InvalidOwner", "该角色不能持有装备");
	case EGameXXKEquipmentTransactionError::SlotMismatch: return NSLOCTEXT("GameXXKEquipment", "SlotMismatch", "装备部位不匹配");
	case EGameXXKEquipmentTransactionError::ItemNotInWarehouse: return NSLOCTEXT("GameXXKEquipment", "ItemNotInWarehouse", "装备不在背包中");
	case EGameXXKEquipmentTransactionError::ConfirmationRequired: return NSLOCTEXT("GameXXKEquipment", "ConfirmationRequired", "该操作需要确认");
	case EGameXXKEquipmentTransactionError::InsufficientEnhancementStones: return NSLOCTEXT("GameXXKEquipment", "InsufficientEnhancementStones", "强化石不足");
	case EGameXXKEquipmentTransactionError::MaxEnhancementReached: return NSLOCTEXT("GameXXKEquipment", "MaxEnhancementReached", "装备已达到最高强化等级");
	case EGameXXKEquipmentTransactionError::InsufficientRefinementSand: return NSLOCTEXT("GameXXKEquipment", "InsufficientRefinementSand", "洗炼砂不足");
	case EGameXXKEquipmentTransactionError::PendingReforgeExists: return NSLOCTEXT("GameXXKEquipment", "PendingReforgeExists", "已有待处理的洗炼结果");
	case EGameXXKEquipmentTransactionError::NoPendingReforge: return NSLOCTEXT("GameXXKEquipment", "NoPendingReforge", "没有待处理的洗炼结果");
	case EGameXXKEquipmentTransactionError::PendingReforgeStale: return NSLOCTEXT("GameXXKEquipment", "PendingReforgeStale", "洗炼结果已失效");
	case EGameXXKEquipmentTransactionError::RouteLocked: return NSLOCTEXT("GameXXKEquipment", "RouteLocked", "路线进行中无法更换伙伴");
	case EGameXXKEquipmentTransactionError::SaveMigrationFailed: return NSLOCTEXT("GameXXKEquipment", "SaveMigrationFailed", "存档迁移失败，已保留原存档。");
	default: return NSLOCTEXT("GameXXKEquipment", "UnknownError", "装备操作失败");
	}
}
