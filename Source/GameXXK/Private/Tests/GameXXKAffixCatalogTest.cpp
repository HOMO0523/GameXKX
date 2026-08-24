#include "Misc/AutomationTest.h"

#include "GameXXKAffixCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAffixCatalogTest,
	"GameXXK.Equipment.AffixCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAffixCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKAffixDefinition>& Universal = FGameXXKAffixCatalog::GetUniversalDefinitions();
	const TArray<FGameXXKAffixDefinition>& All = FGameXXKAffixCatalog::GetAllDefinitions();
	TestEqual(TEXT("there are exactly four rollable universal affix families"), Universal.Num(), 4);
	TestEqual(TEXT("five universal plus thirty set-specific families are exposed"), All.Num(), 35);
	TestFalse(TEXT("retired Speed is absent from the new-roll universal pool"), Universal.ContainsByPredicate(
		[](const FGameXXKAffixDefinition& Definition)
		{
			return Definition.ModifierKind == EGameXXKEquipmentModifierKind::Speed;
		}));
	TestNotNull(TEXT("legacy Speed affixes remain readable for existing saves"),
		FGameXXKAffixCatalog::FindDefinition(TEXT("Affix.Universal.Speed")));

	struct FAffixExpectation
	{
		const TCHAR* Id;
		EGameXXKEquipmentSet Set;
		EGameXXKEquipmentModifierKind ModifierKind;
		EGameXXKEquipmentMagnitudeUnit Unit;
	};
	using S = EGameXXKEquipmentSet;
	using K = EGameXXKEquipmentModifierKind;
	using U = EGameXXKEquipmentMagnitudeUnit;
	const FAffixExpectation ExpectedDefinitions[] = {
		{TEXT("Affix.Universal.MaxHealth"), S::Invalid, K::MaxHealth, U::BasisPoints},
		{TEXT("Affix.Universal.MaxMana"), S::Invalid, K::MaxMana, U::BasisPoints},
		{TEXT("Affix.Universal.Attack"), S::Invalid, K::Attack, U::BasisPoints},
		{TEXT("Affix.Universal.Defense"), S::Invalid, K::Defense, U::BasisPoints},
		{TEXT("Affix.Universal.Speed"), S::Invalid, K::Speed, U::BasisPoints},
		{TEXT("Affix.PoJun.DirectDamage"), S::PoJun, K::DirectDamage, U::BasisPoints},
		{TEXT("Affix.PoJun.MultiHitDamage"), S::PoJun, K::MultiHitDamage, U::BasisPoints},
		{TEXT("Affix.PoJun.ArmorBreakStacks"), S::PoJun, K::ArmorBreakStacks, U::FlatCount},
		{TEXT("Affix.PoJun.VulnerableTargetDamage"), S::PoJun, K::VulnerableTargetDamage, U::BasisPoints},
		{TEXT("Affix.PoJun.FirstAttackDamage"), S::PoJun, K::FirstAttackDamage, U::BasisPoints},
		{TEXT("Affix.XuanJia.ArmorGain"), S::XuanJia, K::ArmorGain, U::BasisPoints},
		{TEXT("Affix.XuanJia.ArmorRetention"), S::XuanJia, K::ArmorRetention, U::BasisPoints},
		{TEXT("Affix.XuanJia.CounterDamage"), S::XuanJia, K::CounterDamage, U::BasisPoints},
		{TEXT("Affix.XuanJia.GuardReduction"), S::XuanJia, K::GuardReduction, U::BasisPoints},
		{TEXT("Affix.XuanJia.LowHealthProtection"), S::XuanJia, K::LowHealthProtection, U::BasisPoints},
		{TEXT("Affix.QingNang.Healing"), S::QingNang, K::Healing, U::BasisPoints},
		{TEXT("Affix.QingNang.Cleanse"), S::QingNang, K::Cleanse, U::FlatCount},
		{TEXT("Affix.QingNang.OverhealConversion"), S::QingNang, K::OverhealConversion, U::BasisPoints},
		{TEXT("Affix.QingNang.ManaRecovery"), S::QingNang, K::ManaRecovery, U::BasisPoints},
		{TEXT("Affix.QingNang.EmergencyHealing"), S::QingNang, K::EmergencyHealing, U::BasisPoints},
		{TEXT("Affix.ZhuiFeng.Draw"), S::ZhuiFeng, K::Draw, U::FlatCount},
		{TEXT("Affix.ZhuiFeng.LowCostBonus"), S::ZhuiFeng, K::LowCostBonus, U::BasisPoints},
		{TEXT("Affix.ZhuiFeng.SharedEnergy"), S::ZhuiFeng, K::SharedEnergy, U::FlatCount},
		{TEXT("Affix.ZhuiFeng.ComboCount"), S::ZhuiFeng, K::ComboCount, U::FlatCount},
		{TEXT("Affix.ZhuiFeng.TemporaryCostReduction"), S::ZhuiFeng, K::TemporaryCostReduction, U::FlatCount},
		{TEXT("Affix.ShiGu.Poison"), S::ShiGu, K::Poison, U::FlatCount},
		{TEXT("Affix.ShiGu.Bleed"), S::ShiGu, K::Bleed, U::FlatCount},
		{TEXT("Affix.ShiGu.Burn"), S::ShiGu, K::Burn, U::FlatCount},
		{TEXT("Affix.ShiGu.DamageOverTime"), S::ShiGu, K::DamageOverTime, U::BasisPoints},
		{TEXT("Affix.ShiGu.StatusRetention"), S::ShiGu, K::StatusRetention, U::FlatCount},
		{TEXT("Affix.ShanHe.TerrainPower"), S::ShanHe, K::TerrainPower, U::BasisPoints},
		{TEXT("Affix.ShanHe.TerrainCostReduction"), S::ShanHe, K::TerrainCostReduction, U::FlatCount},
		{TEXT("Affix.ShanHe.AdjacentAllyPower"), S::ShanHe, K::AdjacentAllyPower, U::BasisPoints},
		{TEXT("Affix.ShanHe.FormationPower"), S::ShanHe, K::FormationPower, U::BasisPoints},
		{TEXT("Affix.ShanHe.TeamTerrainPower"), S::ShanHe, K::TeamTerrainPower, U::BasisPoints},
	};
	TestEqual(TEXT("the exact affix expectation table contains all 35 rows"), static_cast<int32>(UE_ARRAY_COUNT(ExpectedDefinitions)), 35);
	for (const FAffixExpectation& Expected : ExpectedDefinitions)
	{
		const FGameXXKAffixDefinition* Definition = FGameXXKAffixCatalog::FindDefinition(FName(Expected.Id));
		TestNotNull(FString::Printf(TEXT("stable affix ID %s resolves"), Expected.Id), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s set is frozen"), Expected.Id), Definition->Set, Expected.Set);
		TestEqual(FString::Printf(TEXT("%s modifier kind is frozen"), Expected.Id), Definition->ModifierKind, Expected.ModifierKind);
		TestEqual(FString::Printf(TEXT("%s magnitude unit is frozen"), Expected.Id), Definition->Unit, Expected.Unit);
	}

	TSet<FName> Ids;
	TSet<uint8> ModifierKinds;
	for (const FGameXXKAffixDefinition& Definition : All)
	{
		TestFalse(TEXT("affix stable IDs are populated"), Definition.Id.IsNone());
		TestFalse(TEXT("affix localized names are populated"), Definition.DisplayName.IsEmpty());
		TestNotEqual(TEXT("affix modifiers are valid"), Definition.ModifierKind, EGameXXKEquipmentModifierKind::Invalid);
		TestTrue(TEXT("affix units are supported"), Definition.Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints || Definition.Unit == EGameXXKEquipmentMagnitudeUnit::FlatCount);
		Ids.Add(Definition.Id);
		ModifierKinds.Add(static_cast<uint8>(Definition.ModifierKind));
		TestNotNull(TEXT("affix lookup uses its stable row ID"), FGameXXKAffixCatalog::FindDefinition(Definition.Id));
	}
	TestEqual(TEXT("affix row IDs are unique"), Ids.Num(), 35);
	TestEqual(TEXT("affix uniqueness is defined by ModifierKind"), ModifierKinds.Num(), 35);

	for (const FGameXXKAffixDefinition& Definition : Universal)
	{
		TestEqual(TEXT("universal affixes are not tied to a modern set"), Definition.Set, EGameXXKEquipmentSet::Invalid);
	}
	for (uint8 SetValue = static_cast<uint8>(EGameXXKEquipmentSet::PoJun); SetValue <= static_cast<uint8>(EGameXXKEquipmentSet::ShanHe); ++SetValue)
	{
		const EGameXXKEquipmentSet Set = static_cast<EGameXXKEquipmentSet>(SetValue);
		const TArray<FGameXXKAffixDefinition>& Definitions = FGameXXKAffixCatalog::GetSetDefinitions(Set);
		TestEqual(TEXT("each modern set has exactly five exclusive families"), Definitions.Num(), 5);
		for (const FGameXXKAffixDefinition& Definition : Definitions)
		{
			TestEqual(TEXT("set pool contains only its own affixes"), Definition.Set, Set);
		}
	}
	TestEqual(TEXT("Legacy has no modern exclusive affix pool"), FGameXXKAffixCatalog::GetSetDefinitions(EGameXXKEquipmentSet::Legacy).Num(), 0);

	const EGameXXKEquipmentQuality Qualities[] = {
		EGameXXKEquipmentQuality::Common,
		EGameXXKEquipmentQuality::Rare,
		EGameXXKEquipmentQuality::Epic,
		EGameXXKEquipmentQuality::Legendary,
		EGameXXKEquipmentQuality::Immortal,
		EGameXXKEquipmentQuality::Treasure,
		EGameXXKEquipmentQuality::Transcendent,
		EGameXXKEquipmentQuality::Celestial,
		EGameXXKEquipmentQuality::Ascendant,
		EGameXXKEquipmentQuality::Cosmic,
	};
	const EGameXXKAffixTier Tiers[] = {
		EGameXXKAffixTier::Common,
		EGameXXKAffixTier::Rare,
		EGameXXKAffixTier::Epic,
		EGameXXKAffixTier::Legendary,
		EGameXXKAffixTier::Immortal,
		EGameXXKAffixTier::Treasure,
		EGameXXKAffixTier::Transcendent,
		EGameXXKAffixTier::Celestial,
		EGameXXKAffixTier::Ascendant,
		EGameXXKAffixTier::Cosmic,
	};
	for (int32 QualityIndex = 0; QualityIndex < static_cast<int32>(UE_ARRAY_COUNT(Qualities)); ++QualityIndex)
	{
		const int32 QualityRank = QualityIndex + 1;
		const FGameXXKAffixTierWeights Weights = FGameXXKAffixCatalog::GetTierWeights(Qualities[QualityIndex]);
		int32 TotalWeight = 0;
		for (int32 TierIndex = 0; TierIndex < static_cast<int32>(UE_ARRAY_COUNT(Tiers)); ++TierIndex)
		{
			const int32 TierRank = TierIndex + 1;
			const int32 ExpectedWeight = QualityRank == 1
				? (TierRank == 1 ? 100 : 0)
				: QualityRank == 2
					? (TierRank == 1 ? 70 : TierRank == 2 ? 30 : 0)
					: (TierRank == QualityRank - 2 ? 50 : TierRank == QualityRank - 1 ? 35 : TierRank == QualityRank ? 15 : 0);
			const int32 ActualWeight = Weights.GetWeight(Tiers[TierIndex]);
			TestEqual(
				FString::Printf(TEXT("quality %d tier %d has exact deterministic weight"), QualityRank, TierRank),
				ActualWeight,
				ExpectedWeight);
			TotalWeight += ActualWeight;
		}
		TestEqual(FString::Printf(TEXT("quality %d tier weights sum to 100"), QualityRank), TotalWeight, 100);
	}

	FGameXXKAffixTierWeights MutableWeights;
	for (int32 TierIndex = 0; TierIndex < static_cast<int32>(UE_ARRAY_COUNT(Tiers)); ++TierIndex)
	{
		MutableWeights.SetWeight(Tiers[TierIndex], 11 + TierIndex);
		TestEqual(TEXT("SetWeight roundtrips through GetWeight"), MutableWeights.GetWeight(Tiers[TierIndex]), 11 + TierIndex);
	}
	TestEqual(TEXT("legacy Common named field remains first and writable"), MutableWeights.Common, 11);
	TestEqual(TEXT("legacy Rare named field remains second and writable"), MutableWeights.Rare, 12);
	TestEqual(TEXT("legacy Epic named field remains third and writable"), MutableWeights.Epic, 13);
	TestEqual(TEXT("Legendary named field is appended"), MutableWeights.Legendary, 14);
	TestEqual(TEXT("Immortal named field is appended"), MutableWeights.Immortal, 15);
	TestEqual(TEXT("Treasure named field is appended"), MutableWeights.Treasure, 16);
	TestEqual(TEXT("Transcendent named field is appended"), MutableWeights.Transcendent, 17);
	TestEqual(TEXT("Celestial named field is appended"), MutableWeights.Celestial, 18);
	TestEqual(TEXT("Ascendant named field is appended"), MutableWeights.Ascendant, 19);
	TestEqual(TEXT("Cosmic named field is appended"), MutableWeights.Cosmic, 20);
	TestEqual(TEXT("invalid tier reads zero weight"), MutableWeights.GetWeight(EGameXXKAffixTier::Invalid), 0);

	for (int32 TierIndex = 0; TierIndex < static_cast<int32>(UE_ARRAY_COUNT(Tiers)); ++TierIndex)
	{
		const int64 Rank = TierIndex + 1;
		const int32 ExpectedBasisMin = static_cast<int32>(100LL * (Rank + 1) * (Rank + 2) / 2);
		const int32 ExpectedBasisMax = ExpectedBasisMin + static_cast<int32>(100LL * (Rank + 1));
		const int32 ExpectedFlatMin = static_cast<int32>((Rank + 1) / 2);
		const int32 ExpectedFlatMax = static_cast<int32>(Rank);
		const FGameXXKAffixMagnitudeRange BasisRange = FGameXXKAffixCatalog::GetMagnitudeRange(
			EGameXXKEquipmentMagnitudeUnit::BasisPoints,
			Tiers[TierIndex]);
		const FGameXXKAffixMagnitudeRange FlatRange = FGameXXKAffixCatalog::GetMagnitudeRange(
			EGameXXKEquipmentMagnitudeUnit::FlatCount,
			Tiers[TierIndex]);
		TestEqual(FString::Printf(TEXT("tier %lld basis minimum is exact"), Rank), BasisRange.Minimum, ExpectedBasisMin);
		TestEqual(FString::Printf(TEXT("tier %lld basis maximum is exact"), Rank), BasisRange.Maximum, ExpectedBasisMax);
		TestEqual(FString::Printf(TEXT("tier %lld flat minimum is exact"), Rank), FlatRange.Minimum, ExpectedFlatMin);
		TestEqual(FString::Printf(TEXT("tier %lld flat maximum is exact"), Rank), FlatRange.Maximum, ExpectedFlatMax);
	}
	return true;
}

#endif
