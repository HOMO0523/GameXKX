#include "GameXXKEquipmentCatalog.h"

#include "Misc/PackageName.h"

namespace
{
	constexpr int32 MaxEquipmentLevel = 20;

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	FGameXXKEquipmentStatCurve Curve(const int32 LevelOne, const int32 Numerator = 0, const int32 Divisor = 0)
	{
		FGameXXKEquipmentStatCurve Result;
		Result.LevelOne = LevelOne;
		Result.GrowthNumerator = Numerator;
		Result.GrowthDivisor = Divisor;
		return Result;
	}

	FGameXXKEquipmentBaseStatCoefficients MakeSlotCoefficients(const EGameXXKEquipmentSlot Slot)
	{
		FGameXXKEquipmentBaseStatCoefficients Coefficients;
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon:
			Coefficients.Attack = Curve(2, 1, 1);
			break;
		case EGameXXKEquipmentSlot::Head:
			Coefficients.MaxHealth = Curve(8, 2, 1);
			break;
		case EGameXXKEquipmentSlot::Armor:
			Coefficients.MaxHealth = Curve(4, 1, 1);
			Coefficients.Defense = Curve(1, 1, 3);
			break;
		case EGameXXKEquipmentSlot::Belt:
			Coefficients.MaxHealth = Curve(6, 1, 1);
			Coefficients.MaxMana = Curve(2, 1, 1);
			break;
		case EGameXXKEquipmentSlot::Shoes:
			Coefficients.Speed = Curve(1, 1, 5);
			break;
		case EGameXXKEquipmentSlot::Accessory:
			Coefficients.MaxMana = Curve(4, 1, 1);
			Coefficients.Attack = Curve(1, 1, 4);
			break;
		default:
			break;
		}
		return Coefficients;
	}

	struct FSetPresentation
	{
		EGameXXKEquipmentSet Set;
		const TCHAR* IdSegment;
		FText DisplayName;
	};

	struct FSlotPresentation
	{
		EGameXXKEquipmentSlot Slot;
		const TCHAR* IdSegment;
		FText DisplayName;
	};

	TArray<FGameXXKEquipmentDefinition> BuildPackageDefinitions()
	{
		const FSetPresentation Sets[] = {
			{EGameXXKEquipmentSet::PoJun, TEXT("PoJun"), NSLOCTEXT("GameXXKEquipment", "SetPoJun", "破军")},
			{EGameXXKEquipmentSet::XuanJia, TEXT("XuanJia"), NSLOCTEXT("GameXXKEquipment", "SetXuanJia", "玄甲")},
			{EGameXXKEquipmentSet::QingNang, TEXT("QingNang"), NSLOCTEXT("GameXXKEquipment", "SetQingNang", "青囊")},
			{EGameXXKEquipmentSet::ZhuiFeng, TEXT("ZhuiFeng"), NSLOCTEXT("GameXXKEquipment", "SetZhuiFeng", "追风")},
			{EGameXXKEquipmentSet::ShiGu, TEXT("ShiGu"), NSLOCTEXT("GameXXKEquipment", "SetShiGu", "蚀骨")},
			{EGameXXKEquipmentSet::ShanHe, TEXT("ShanHe"), NSLOCTEXT("GameXXKEquipment", "SetShanHe", "山河")},
		};
		const FSlotPresentation Slots[] = {
			{EGameXXKEquipmentSlot::Weapon, TEXT("Weapon"), NSLOCTEXT("GameXXKEquipment", "SlotWeapon", "武器")},
			{EGameXXKEquipmentSlot::Head, TEXT("Head"), NSLOCTEXT("GameXXKEquipment", "SlotHead", "头冠")},
			{EGameXXKEquipmentSlot::Armor, TEXT("Armor"), NSLOCTEXT("GameXXKEquipment", "SlotArmor", "护甲")},
			{EGameXXKEquipmentSlot::Belt, TEXT("Belt"), NSLOCTEXT("GameXXKEquipment", "SlotBelt", "腰带")},
			{EGameXXKEquipmentSlot::Shoes, TEXT("Shoes"), NSLOCTEXT("GameXXKEquipment", "SlotShoes", "鞋履")},
			{EGameXXKEquipmentSlot::Accessory, TEXT("Accessory"), NSLOCTEXT("GameXXKEquipment", "SlotAccessory", "饰品")},
		};

		TArray<FGameXXKEquipmentDefinition> Definitions;
		Definitions.Reserve(UE_ARRAY_COUNT(Sets) * UE_ARRAY_COUNT(Slots));
		for (const FSetPresentation& Set : Sets)
		{
			for (const FSlotPresentation& Slot : Slots)
			{
				FGameXXKEquipmentDefinition Definition;
				Definition.Id = FName(*FString::Printf(TEXT("Equipment.%s.%s"), Set.IdSegment, Slot.IdSegment));
				Definition.DisplayName = FText::Format(
					NSLOCTEXT("GameXXKEquipment", "ModernEquipmentNameFormat", "{0}{1}"),
					Set.DisplayName,
					Slot.DisplayName);
				Definition.Set = Set.Set;
				Definition.Slot = Slot.Slot;
				Definition.ScalingRule = EGameXXKEquipmentScalingRule::ModernPercentBase;
				Definition.BaseStatCoefficients = MakeSlotCoefficients(Slot.Slot);
				// UI V2 approved set icons: /Game/GameXXK/UI/Equipment/<set>_<slot>.
				const FString SetSegment = FString(Set.IdSegment).ToLower();
				const FString SlotSegment = FString(Slot.IdSegment).ToLower();
				Definition.IconSoftPath = FSoftObjectPath(*FString::Printf(
					TEXT("/Game/GameXXK/UI/Equipment/%s_%s.%s_%s"),
					*SetSegment, *SlotSegment, *SetSegment, *SlotSegment));
				Definitions.Add(MoveTemp(Definition));
			}
		}

		// Starter set: the UI V2 approved six-slot ordinary starter equipment.
		// No set bonuses; plain modern curves so they grow like any modern gear.
		for (const FSlotPresentation& Slot : Slots)
		{
			FGameXXKEquipmentDefinition Definition;
			Definition.Id = FName(*FString::Printf(TEXT("Equipment.Starter.%s"), Slot.IdSegment));
			Definition.DisplayName = FText::Format(
				NSLOCTEXT("GameXXKEquipment", "StarterEquipmentNameFormat", "基础{0}"),
				Slot.DisplayName);
			Definition.Set = EGameXXKEquipmentSet::Starter;
			Definition.Slot = Slot.Slot;
			Definition.ScalingRule = EGameXXKEquipmentScalingRule::ModernPercentBase;
			Definition.BaseStatCoefficients = MakeSlotCoefficients(Slot.Slot);
			const FString SlotSegment = FString(Slot.IdSegment).ToLower();
			Definition.IconSoftPath = FSoftObjectPath(*FString::Printf(
				TEXT("/Game/GameXXK/UI/StarterEquipment/starter_%s.starter_%s"),
				*SlotSegment, *SlotSegment));
			Definitions.Add(MoveTemp(Definition));
		}
		return Definitions;
	}

	FGameXXKEquipmentDefinition MakeLegacyDefinition(
		const TCHAR* Id,
		const FText& DisplayName,
		const EGameXXKEquipmentSlot Slot,
		const FGameXXKCharacterStats& Snapshot,
		const TCHAR* IconPath)
	{
		FGameXXKEquipmentDefinition Definition;
		Definition.Id = FName(Id);
		Definition.DisplayName = DisplayName;
		Definition.Set = EGameXXKEquipmentSet::Legacy;
		Definition.Slot = Slot;
		Definition.ScalingRule = EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement;
		Definition.LegacyBaseStatSnapshot = Snapshot;
		Definition.IconSoftPath = FSoftObjectPath(IconPath);
		return Definition;
	}

	FGameXXKCharacterStats LegacyStats(const int32 Health, const int32 Mana, const int32 Attack, const int32 Defense)
	{
		FGameXXKCharacterStats Stats;
		Stats.MaxHealth = Health;
		Stats.MaxMana = Mana;
		Stats.Attack = Attack;
		Stats.Defense = Defense;
		return Stats;
	}

	TArray<FGameXXKEquipmentDefinition> BuildLegacyDefinitions()
	{
		constexpr const TCHAR* Root = TEXT("/Game/GameXXK/UI/Inventory/Textures/");
		return {
			MakeLegacyDefinition(TEXT("Item.IronSword"), NSLOCTEXT("GameXXKEquipment", "LegacyIronSword", "青锋短剑"), EGameXXKEquipmentSlot::Weapon, LegacyStats(0, 0, 8, 0), *(FString(Root) + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword"))),
			MakeLegacyDefinition(TEXT("Item.ClothArmor"), NSLOCTEXT("GameXXKEquipment", "LegacyClothArmor", "竹编轻甲"), EGameXXKEquipmentSlot::Armor, LegacyStats(0, 0, 0, 6), *(FString(Root) + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor"))),
			MakeLegacyDefinition(TEXT("Item.CranePatternTalisman"), NSLOCTEXT("GameXXKEquipment", "LegacyCranePatternTalisman", "鹤纹护符"), EGameXXKEquipmentSlot::Accessory, LegacyStats(30, 0, 0, 0), *(FString(Root) + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman"))),
			MakeLegacyDefinition(TEXT("Item.InkstonePendant"), NSLOCTEXT("GameXXKEquipment", "LegacyInkstonePendant", "墨砚坠饰"), EGameXXKEquipmentSlot::Accessory, LegacyStats(0, 20, 0, 0), *(FString(Root) + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant"))),
			MakeLegacyDefinition(TEXT("Item.WoodenSword"), NSLOCTEXT("GameXXKEquipment", "LegacyWoodenSword", "木剑"), EGameXXKEquipmentSlot::Weapon, LegacyStats(0, 0, 3, 0), *(FString(Root) + TEXT("T_ItemWoodenSword.T_ItemWoodenSword"))),
			MakeLegacyDefinition(TEXT("Item.StarterClothArmor"), NSLOCTEXT("GameXXKEquipment", "LegacyStarterClothArmor", "布甲"), EGameXXKEquipmentSlot::Armor, LegacyStats(0, 0, 0, 3), *(FString(Root) + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor"))),
			MakeLegacyDefinition(TEXT("Item.ClothTalisman"), NSLOCTEXT("GameXXKEquipment", "LegacyClothTalisman", "布护符"), EGameXXKEquipmentSlot::Accessory, LegacyStats(10, 0, 0, 0), *(FString(Root) + TEXT("T_ItemClothTalisman.T_ItemClothTalisman"))),
		};
	}

	const TArray<FGameXXKEquipmentDefinition>& LegacyDefinitions()
	{
		static const TArray<FGameXXKEquipmentDefinition> Definitions = BuildLegacyDefinitions();
		return Definitions;
	}
}

int32 FGameXXKEquipmentStatCurve::Resolve(const int32 Level) const
{
	if (GrowthNumerator == 0 || GrowthDivisor <= 0)
	{
		return LevelOne;
	}
	const int64 LevelOffset = static_cast<int64>(FMath::Clamp(Level, 1, MaxEquipmentLevel) - 1);
	return LevelOne + static_cast<int32>((LevelOffset * GrowthNumerator) / GrowthDivisor);
}

bool FGameXXKEquipmentStatCurve::IsValid() const
{
	return LevelOne >= 0
		&& GrowthNumerator >= 0
		&& (GrowthNumerator == 0 || GrowthDivisor > 0);
}

FGameXXKCharacterStats FGameXXKEquipmentBaseStatCoefficients::Resolve(const int32 Level) const
{
	FGameXXKCharacterStats Stats;
	Stats.MaxHealth = MaxHealth.Resolve(Level);
	Stats.MaxMana = MaxMana.Resolve(Level);
	Stats.Attack = Attack.Resolve(Level);
	Stats.Defense = Defense.Resolve(Level);
	Stats.Speed = Speed.Resolve(Level);
	return Stats;
}

bool FGameXXKEquipmentBaseStatCoefficients::IsValid() const
{
	return MaxHealth.IsValid()
		&& MaxMana.IsValid()
		&& Attack.IsValid()
		&& Defense.IsValid()
		&& Speed.IsValid();
}

const TArray<FGameXXKEquipmentDefinition>& FGameXXKEquipmentCatalog::GetPackageDefinitions()
{
	static const TArray<FGameXXKEquipmentDefinition> Definitions = BuildPackageDefinitions();
	return Definitions;
}

const FGameXXKEquipmentDefinition* FGameXXKEquipmentCatalog::FindDefinition(const FName EquipmentId)
{
	if (EquipmentId.IsNone())
	{
		return nullptr;
	}
	if (const FGameXXKEquipmentDefinition* Modern = GetPackageDefinitions().FindByPredicate(
		[EquipmentId](const FGameXXKEquipmentDefinition& Definition) { return Definition.Id == EquipmentId; }))
	{
		return Modern;
	}
	return LegacyDefinitions().FindByPredicate(
		[EquipmentId](const FGameXXKEquipmentDefinition& Definition) { return Definition.Id == EquipmentId; });
}

bool FGameXXKEquipmentCatalog::ValidateDefinition(const FGameXXKEquipmentDefinition& Definition, FString* OutError)
{
	if (Definition.Id.IsNone()
		|| Definition.DisplayName.IsEmpty()
		|| Definition.Slot < EGameXXKEquipmentSlot::Weapon
		|| Definition.Slot > EGameXXKEquipmentSlot::Accessory)
	{
		SetError(OutError, TEXT("Equipment definition requires an ID, display name, and valid slot."));
		return false;
	}

	const bool bModern = Definition.Set == EGameXXKEquipmentSet::Starter
		|| (Definition.Set >= EGameXXKEquipmentSet::PoJun
			&& Definition.Set <= EGameXXKEquipmentSet::ShanHe);
	if (bModern)
	{
		if (Definition.ScalingRule != EGameXXKEquipmentScalingRule::ModernPercentBase
			|| !Definition.BaseStatCoefficients.IsValid())
		{
			SetError(OutError, TEXT("Modern equipment requires valid stat curves and modern scaling."));
			return false;
		}
	}
	else if (Definition.Set != EGameXXKEquipmentSet::Legacy
		|| Definition.ScalingRule != EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement)
	{
		SetError(OutError, TEXT("Equipment set and scaling rule are inconsistent."));
		return false;
	}

	if (!Definition.IconSoftPath.IsNull()
		&& !FPackageName::IsValidObjectPath(Definition.IconSoftPath.ToString()))
	{
		SetError(OutError, TEXT("Equipment icon must be empty or a well-formed soft object path."));
		return false;
	}
	return true;
}

int32 FGameXXKEquipmentCatalog::GetEnhancementStoneCost(const int32 CurrentEnhancementLevel)
{
	return CurrentEnhancementLevel >= 0 && CurrentEnhancementLevel < 10 ? 1 : 0;
}

int32 FGameXXKEquipmentCatalog::GetReforgeSandCost(const EGameXXKEquipmentQuality Quality)
{
	// One refinement sand per wash, regardless of quality.
	return 1;
}

int32 FGameXXKEquipmentCatalog::GetDismantleSandYield(const EGameXXKEquipmentQuality Quality)
{
	return Quality >= EGameXXKEquipmentQuality::Common && Quality <= EGameXXKEquipmentQuality::Epic ? 1 : 0;
}
