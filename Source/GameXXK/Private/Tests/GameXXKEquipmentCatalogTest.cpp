#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentCatalogTest,
	"GameXXK.Equipment.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	const TCHAR* SetSegment(const EGameXXKEquipmentSet Set)
	{
		switch (Set)
		{
		case EGameXXKEquipmentSet::PoJun: return TEXT("PoJun");
		case EGameXXKEquipmentSet::XuanJia: return TEXT("XuanJia");
		case EGameXXKEquipmentSet::QingNang: return TEXT("QingNang");
		case EGameXXKEquipmentSet::ZhuiFeng: return TEXT("ZhuiFeng");
		case EGameXXKEquipmentSet::ShiGu: return TEXT("ShiGu");
		case EGameXXKEquipmentSet::ShanHe: return TEXT("ShanHe");
		default: return TEXT("Invalid");
		}
	}

	const TCHAR* SlotSegment(const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return TEXT("Weapon");
		case EGameXXKEquipmentSlot::Head: return TEXT("Head");
		case EGameXXKEquipmentSlot::Armor: return TEXT("Armor");
		case EGameXXKEquipmentSlot::Belt: return TEXT("Belt");
		case EGameXXKEquipmentSlot::Shoes: return TEXT("Shoes");
		case EGameXXKEquipmentSlot::Accessory: return TEXT("Accessory");
		default: return TEXT("Invalid");
		}
	}

	FGameXXKCharacterStats ExpectedSlotStats(const EGameXXKEquipmentSlot Slot, const int32 Level)
	{
		const int32 Offset = FMath::Clamp(Level, 1, 20) - 1;
		FGameXXKCharacterStats Stats;
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon:
			Stats.Attack = 2 + Offset;
			break;
		case EGameXXKEquipmentSlot::Head:
			Stats.MaxHealth = 8 + Offset * 2;
			break;
		case EGameXXKEquipmentSlot::Armor:
			Stats.MaxHealth = 4 + Offset;
			Stats.Defense = 1 + Offset / 3;
			break;
		case EGameXXKEquipmentSlot::Belt:
			Stats.MaxHealth = 6 + Offset;
			Stats.MaxMana = 0;
			break;
		case EGameXXKEquipmentSlot::Shoes:
			Stats.Speed = 1 + Offset / 5;
			break;
		case EGameXXKEquipmentSlot::Accessory:
			Stats.MaxMana = 0;
			Stats.Attack = 1 + Offset / 4;
			break;
		default:
			break;
		}
		return Stats;
	}

	void TestStats(
		FAutomationTestBase& Test,
		const FString& Prefix,
		const FGameXXKCharacterStats& Actual,
		const FGameXXKCharacterStats& Expected)
	{
		Test.TestEqual(Prefix + TEXT(" max health"), Actual.MaxHealth, Expected.MaxHealth);
		Test.TestEqual(Prefix + TEXT(" max mana"), Actual.MaxMana, Expected.MaxMana);
		Test.TestEqual(Prefix + TEXT(" attack"), Actual.Attack, Expected.Attack);
		Test.TestEqual(Prefix + TEXT(" defense"), Actual.Defense, Expected.Defense);
		Test.TestEqual(Prefix + TEXT(" speed"), Actual.Speed, Expected.Speed);
	}
}

bool FGameXXKEquipmentCatalogTest::RunTest(const FString& Parameters)
{
	struct FQualityExpectation
	{
		EGameXXKEquipmentQuality Quality;
		EGameXXKAffixTier Tier;
		int32 Rank;
		const TCHAR* DisplayName;
	};
	const FQualityExpectation QualityExpectations[] = {
		{EGameXXKEquipmentQuality::Common, EGameXXKAffixTier::Common, 1, TEXT("普通")},
		{EGameXXKEquipmentQuality::Rare, EGameXXKAffixTier::Rare, 2, TEXT("稀有")},
		{EGameXXKEquipmentQuality::Epic, EGameXXKAffixTier::Epic, 3, TEXT("珍稀")},
		{EGameXXKEquipmentQuality::Legendary, EGameXXKAffixTier::Legendary, 4, TEXT("传奇")},
		{EGameXXKEquipmentQuality::Immortal, EGameXXKAffixTier::Immortal, 5, TEXT("不朽")},
		{EGameXXKEquipmentQuality::Treasure, EGameXXKAffixTier::Treasure, 6, TEXT("至宝")},
		{EGameXXKEquipmentQuality::Transcendent, EGameXXKAffixTier::Transcendent, 7, TEXT("超凡")},
		{EGameXXKEquipmentQuality::Celestial, EGameXXKAffixTier::Celestial, 8, TEXT("天界")},
		{EGameXXKEquipmentQuality::Ascendant, EGameXXKAffixTier::Ascendant, 9, TEXT("登神")},
		{EGameXXKEquipmentQuality::Cosmic, EGameXXKAffixTier::Cosmic, 10, TEXT("宇宙")},
	};
	const UEnum* EquipmentQualityEnum = StaticEnum<EGameXXKEquipmentQuality>();
	const UEnum* AffixTierEnum = StaticEnum<EGameXXKAffixTier>();
	TestNotNull(TEXT("equipment quality is reflected"), EquipmentQualityEnum);
	TestNotNull(TEXT("affix tier is reflected"), AffixTierEnum);
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(QualityExpectations)); ++Index)
	{
		const FQualityExpectation& Expected = QualityExpectations[Index];
		const FString Label = FString::Printf(TEXT("quality rank %d"), Expected.Rank);
		TestEqual(Label + TEXT(" equipment ordinal"), static_cast<int32>(static_cast<uint8>(Expected.Quality)), Expected.Rank);
		TestEqual(Label + TEXT(" affix ordinal"), static_cast<int32>(static_cast<uint8>(Expected.Tier)), Expected.Rank);
		TestEqual(Label + TEXT(" equipment helper rank"), FGameXXKEquipmentQualityRules::GetRank(Expected.Quality), Expected.Rank);
		TestEqual(Label + TEXT(" affix helper rank"), FGameXXKEquipmentQualityRules::GetRank(Expected.Tier), Expected.Rank);
		TestEqual(Label + TEXT(" equipment helper display"), FGameXXKEquipmentQualityRules::GetDisplayName(Expected.Quality).ToString(), FString(Expected.DisplayName));
		TestEqual(Label + TEXT(" affix helper display"), FGameXXKEquipmentQualityRules::GetDisplayName(Expected.Tier).ToString(), FString(Expected.DisplayName));
		TestEqual(Label + TEXT(" equipment from rank"), FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(Expected.Rank), Expected.Quality);
		TestEqual(Label + TEXT(" affix from rank"), FGameXXKEquipmentQualityRules::AffixTierFromRank(Expected.Rank), Expected.Tier);
		if (EquipmentQualityEnum && AffixTierEnum)
		{
			TestEqual(Label + TEXT(" reflected equipment display"), EquipmentQualityEnum->GetDisplayNameTextByValue(Expected.Rank).ToString(), FString(Expected.DisplayName));
			TestEqual(Label + TEXT(" reflected affix display"), AffixTierEnum->GetDisplayNameTextByValue(Expected.Rank).ToString(), FString(Expected.DisplayName));
		}
		const EGameXXKEquipmentQuality ExpectedNextQuality = Index + 1 < static_cast<int32>(UE_ARRAY_COUNT(QualityExpectations))
			? QualityExpectations[Index + 1].Quality
			: EGameXXKEquipmentQuality::Invalid;
		const EGameXXKAffixTier ExpectedNextTier = Index + 1 < static_cast<int32>(UE_ARRAY_COUNT(QualityExpectations))
			? QualityExpectations[Index + 1].Tier
			: EGameXXKAffixTier::Invalid;
		TestEqual(Label + TEXT(" next equipment quality"), FGameXXKEquipmentQualityRules::GetNext(Expected.Quality), ExpectedNextQuality);
		TestEqual(Label + TEXT(" next affix tier"), FGameXXKEquipmentQualityRules::GetNext(Expected.Tier), ExpectedNextTier);
	}
	TestEqual(TEXT("Cosmic equipment has no next quality"), FGameXXKEquipmentQualityRules::GetNext(EGameXXKEquipmentQuality::Cosmic), EGameXXKEquipmentQuality::Invalid);
	TestEqual(TEXT("Cosmic affix has no next tier"), FGameXXKEquipmentQualityRules::GetNext(EGameXXKAffixTier::Cosmic), EGameXXKAffixTier::Invalid);
	TestFalse(TEXT("equipment rank eleven is not a known quality"), FGameXXKEquipmentQualityRules::IsValid(static_cast<EGameXXKEquipmentQuality>(11)));
	TestFalse(TEXT("affix rank eleven is not a known tier"), FGameXXKEquipmentQualityRules::IsValid(static_cast<EGameXXKAffixTier>(11)));
	TestEqual(TEXT("equipment rank eleven converts to Invalid"), FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(11), EGameXXKEquipmentQuality::Invalid);
	TestEqual(TEXT("affix rank eleven converts to Invalid"), FGameXXKEquipmentQualityRules::AffixTierFromRank(11), EGameXXKAffixTier::Invalid);

	const TArray<FGameXXKEquipmentDefinition>& Packages = FGameXXKEquipmentCatalog::GetPackageDefinitions();
	TestEqual(TEXT("six combat sets plus Starter across six slots produce exactly 42 package definitions"), Packages.Num(), 42);
	TSet<FName> AllPackageIds;
	int32 StarterPackageCount = 0;
	for (const FGameXXKEquipmentDefinition& Definition : Packages)
	{
		AllPackageIds.Add(Definition.Id);
		StarterPackageCount += Definition.Set == EGameXXKEquipmentSet::Starter ? 1 : 0;
	}
	TestEqual(TEXT("Starter contributes exactly one package per slot"), StarterPackageCount, 6);
	TestEqual(TEXT("all combat-set and Starter package IDs are unique"), AllPackageIds.Num(), 42);

	TSet<FName> StableIds;
	TSet<int32> SetSlotPairs;
	const int32 LockedLevels[] = {1, 5, 10, 15, 20};
	for (uint8 SetValue = static_cast<uint8>(EGameXXKEquipmentSet::PoJun); SetValue <= static_cast<uint8>(EGameXXKEquipmentSet::ShanHe); ++SetValue)
	{
		const EGameXXKEquipmentSet Set = static_cast<EGameXXKEquipmentSet>(SetValue);
		for (uint8 SlotValue = static_cast<uint8>(EGameXXKEquipmentSlot::Weapon); SlotValue <= static_cast<uint8>(EGameXXKEquipmentSlot::Accessory); ++SlotValue)
		{
			const EGameXXKEquipmentSlot Slot = static_cast<EGameXXKEquipmentSlot>(SlotValue);
			const FName ExpectedId(*FString::Printf(TEXT("Equipment.%s.%s"), SetSegment(Set), SlotSegment(Slot)));
			const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(ExpectedId);
			TestNotNull(FString::Printf(TEXT("stable id %s resolves independently of its localized name"), *ExpectedId.ToString()), Definition);
			if (!Definition)
			{
				continue;
			}
			TestEqual(TEXT("modern id is exact"), Definition->Id, ExpectedId);
			TestEqual(TEXT("modern set is exact"), static_cast<uint8>(Definition->Set), SetValue);
			TestEqual(TEXT("modern slot is exact"), static_cast<uint8>(Definition->Slot), SlotValue);
			TestEqual(TEXT("modern definitions use percent-base scaling"), Definition->ScalingRule, EGameXXKEquipmentScalingRule::ModernPercentBase);
			TestFalse(TEXT("localized display name is presentational only but must be non-empty"), Definition->DisplayName.IsEmpty());
			TestTrue(TEXT("modern stat curves are valid"), Definition->BaseStatCoefficients.IsValid());
			TestTrue(TEXT("modern empty or well-formed icon paths pass validation"), FGameXXKEquipmentCatalog::ValidateDefinition(*Definition));
			StableIds.Add(Definition->Id);
			SetSlotPairs.Add(static_cast<int32>(SetValue) * 16 + SlotValue);
			for (const int32 Level : LockedLevels)
			{
				TestStats(*this, FString::Printf(TEXT("%s level %d"), *ExpectedId.ToString(), Level), Definition->BaseStatCoefficients.Resolve(Level), ExpectedSlotStats(Slot, Level));
			}
			TestStats(*this, TEXT("level zero clamps to one"), Definition->BaseStatCoefficients.Resolve(0), ExpectedSlotStats(Slot, 1));
			TestStats(*this, TEXT("level twenty-one clamps to twenty"), Definition->BaseStatCoefficients.Resolve(21), ExpectedSlotStats(Slot, 20));
		}
	}
	TestEqual(TEXT("all six combat-set package IDs are unique"), StableIds.Num(), 36);
	TestEqual(TEXT("all six combat-set slot combinations are unique"), SetSlotPairs.Num(), 36);
	for (uint8 SlotValue = static_cast<uint8>(EGameXXKEquipmentSlot::Weapon); SlotValue <= static_cast<uint8>(EGameXXKEquipmentSlot::Accessory); ++SlotValue)
	{
		const EGameXXKEquipmentSlot Slot = static_cast<EGameXXKEquipmentSlot>(SlotValue);
		const FName StarterId(*FString::Printf(TEXT("Equipment.Starter.%s"), SlotSegment(Slot)));
		const FGameXXKEquipmentDefinition* Starter = FGameXXKEquipmentCatalog::FindDefinition(StarterId);
		TestNotNull(FString::Printf(TEXT("Starter package %s resolves"), *StarterId.ToString()), Starter);
		if (Starter)
		{
			TestEqual(TEXT("Starter package keeps the Starter set identity"), Starter->Set, EGameXXKEquipmentSet::Starter);
			TestEqual(TEXT("Starter package keeps its requested slot"), Starter->Slot, Slot);
		}
	}

	struct FLegacyExpectation
	{
		const TCHAR* Id;
		EGameXXKEquipmentSlot Slot;
		FGameXXKCharacterStats Stats;
		const TCHAR* IconPath;
	};
	TArray<FLegacyExpectation> Legacy;
	auto AddLegacy = [&Legacy](
		const TCHAR* Id,
		const EGameXXKEquipmentSlot Slot,
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const TCHAR* IconPath)
	{
		FGameXXKCharacterStats Stats;
		Stats.MaxHealth = Health;
		Stats.MaxMana = Mana;
		Stats.Attack = Attack;
		Stats.Defense = Defense;
		Legacy.Add({Id, Slot, Stats, IconPath});
	};
	AddLegacy(TEXT("Item.IronSword"), EGameXXKEquipmentSlot::Weapon, 0, 0, 8, 0, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemQingfengShortSword.T_ItemQingfengShortSword"));
	AddLegacy(TEXT("Item.ClothArmor"), EGameXXKEquipmentSlot::Armor, 0, 0, 0, 6, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemBambooLightArmor.T_ItemBambooLightArmor"));
	AddLegacy(TEXT("Item.CranePatternTalisman"), EGameXXKEquipmentSlot::Accessory, 30, 0, 0, 0, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemCranePatternTalisman.T_ItemCranePatternTalisman"));
	AddLegacy(TEXT("Item.InkstonePendant"), EGameXXKEquipmentSlot::Accessory, 0, 20, 0, 0, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemInkstonePendant.T_ItemInkstonePendant"));
	AddLegacy(TEXT("Item.WoodenSword"), EGameXXKEquipmentSlot::Weapon, 0, 0, 3, 0, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemWoodenSword.T_ItemWoodenSword"));
	AddLegacy(TEXT("Item.StarterClothArmor"), EGameXXKEquipmentSlot::Armor, 0, 0, 0, 3, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemStarterClothArmor.T_ItemStarterClothArmor"));
	AddLegacy(TEXT("Item.ClothTalisman"), EGameXXKEquipmentSlot::Accessory, 10, 0, 0, 0, TEXT("/Game/GameXXK/UI/Inventory/Textures/T_ItemClothTalisman.T_ItemClothTalisman"));
	for (const FLegacyExpectation& Expected : Legacy)
	{
		const FName Id(Expected.Id);
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Id);
		TestNotNull(FString::Printf(TEXT("legacy definition %s remains queryable"), Expected.Id), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(TEXT("legacy definition is in the Legacy set"), Definition->Set, EGameXXKEquipmentSet::Legacy);
		TestEqual(TEXT("legacy slot matches the existing item"), Definition->Slot, Expected.Slot);
		TestEqual(TEXT("legacy definition uses flat enhancement scaling"), Definition->ScalingRule, EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement);
		TestStats(*this, FString::Printf(TEXT("%s snapshot"), Expected.Id), Definition->LegacyBaseStatSnapshot, Expected.Stats);
		TestEqual(TEXT("legacy definition reuses its exact existing icon mapping"), Definition->IconSoftPath.ToString(), FString(Expected.IconPath));
		TestTrue(TEXT("legacy definition passes the same catalog validation boundary"), FGameXXKEquipmentCatalog::ValidateDefinition(*Definition));
		TestFalse(TEXT("legacy definitions never enter package candidates"), Packages.ContainsByPredicate([Id](const FGameXXKEquipmentDefinition& Candidate) { return Candidate.Id == Id; }));
	}

	const FGameXXKEquipmentDefinition* ValidModernDefinition = FGameXXKEquipmentCatalog::FindDefinition(TEXT("Equipment.PoJun.Weapon"));
	TestNotNull(TEXT("out-of-range slot validation fixture resolves a valid modern definition"), ValidModernDefinition);
	if (ValidModernDefinition)
	{
		FGameXXKEquipmentDefinition OutOfRangeSlotDefinition = *ValidModernDefinition;
		OutOfRangeSlotDefinition.Slot = static_cast<EGameXXKEquipmentSlot>(255);
		TestFalse(TEXT("equipment definitions reject out-of-range serialized slot values"), FGameXXKEquipmentCatalog::ValidateDefinition(OutOfRangeSlotDefinition));
	}

	FGameXXKEquipmentDefinition MalformedIconDefinition;
	MalformedIconDefinition.Id = TEXT("Equipment.PoJun.Weapon");
	MalformedIconDefinition.DisplayName = FText::FromString(TEXT("Malformed icon fixture"));
	MalformedIconDefinition.Set = EGameXXKEquipmentSet::PoJun;
	MalformedIconDefinition.Slot = EGameXXKEquipmentSlot::Weapon;
	MalformedIconDefinition.ScalingRule = EGameXXKEquipmentScalingRule::ModernPercentBase;
	MalformedIconDefinition.BaseStatCoefficients.Attack.LevelOne = 2;
	MalformedIconDefinition.BaseStatCoefficients.Attack.GrowthNumerator = 1;
	MalformedIconDefinition.BaseStatCoefficients.Attack.GrowthDivisor = 1;
	MalformedIconDefinition.IconSoftPath = FSoftObjectPath(TEXT("/NotMounted/GameXXK/BadIcon.BadIcon"));
	TestFalse(TEXT("the malformed icon fixture remains non-empty"), MalformedIconDefinition.IconSoftPath.IsNull());
	TestFalse(TEXT("a malformed non-empty modern icon path fails validation without loading it"), FGameXXKEquipmentCatalog::ValidateDefinition(MalformedIconDefinition));
	return true;
}

#endif
