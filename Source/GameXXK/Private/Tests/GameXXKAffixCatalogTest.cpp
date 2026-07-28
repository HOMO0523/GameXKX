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
	TestEqual(TEXT("there are exactly five universal affix families"), Universal.Num(), 5);
	TestEqual(TEXT("five universal plus thirty set-specific families are exposed"), All.Num(), 35);

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

	const FGameXXKAffixTierWeights CommonWeights = FGameXXKAffixCatalog::GetTierWeights(EGameXXKEquipmentQuality::Common);
	const FGameXXKAffixTierWeights RareWeights = FGameXXKAffixCatalog::GetTierWeights(EGameXXKEquipmentQuality::Rare);
	const FGameXXKAffixTierWeights EpicWeights = FGameXXKAffixCatalog::GetTierWeights(EGameXXKEquipmentQuality::Epic);
	TestEqual(TEXT("common quality common-tier weight"), CommonWeights.Common, 100);
	TestEqual(TEXT("common quality rare-tier weight"), CommonWeights.Rare, 0);
	TestEqual(TEXT("common quality epic-tier weight"), CommonWeights.Epic, 0);
	TestEqual(TEXT("rare quality common-tier weight"), RareWeights.Common, 70);
	TestEqual(TEXT("rare quality rare-tier weight"), RareWeights.Rare, 30);
	TestEqual(TEXT("rare quality epic-tier weight"), RareWeights.Epic, 0);
	TestEqual(TEXT("epic quality common-tier weight"), EpicWeights.Common, 50);
	TestEqual(TEXT("epic quality rare-tier weight"), EpicWeights.Rare, 35);
	TestEqual(TEXT("epic quality epic-tier weight"), EpicWeights.Epic, 15);

	struct FRangeExpectation
	{
		EGameXXKEquipmentMagnitudeUnit Unit;
		EGameXXKAffixTier Tier;
		int32 Minimum;
		int32 Maximum;
	};
	const FRangeExpectation Ranges[] = {
		{EGameXXKEquipmentMagnitudeUnit::BasisPoints, EGameXXKAffixTier::Common, 300, 500},
		{EGameXXKEquipmentMagnitudeUnit::BasisPoints, EGameXXKAffixTier::Rare, 600, 900},
		{EGameXXKEquipmentMagnitudeUnit::BasisPoints, EGameXXKAffixTier::Epic, 1000, 1400},
		{EGameXXKEquipmentMagnitudeUnit::FlatCount, EGameXXKAffixTier::Common, 1, 1},
		{EGameXXKEquipmentMagnitudeUnit::FlatCount, EGameXXKAffixTier::Rare, 1, 2},
		{EGameXXKEquipmentMagnitudeUnit::FlatCount, EGameXXKAffixTier::Epic, 2, 3},
	};
	for (const FRangeExpectation& Expected : Ranges)
	{
		const FGameXXKAffixMagnitudeRange Actual = FGameXXKAffixCatalog::GetMagnitudeRange(Expected.Unit, Expected.Tier);
		TestEqual(TEXT("tier range minimum is frozen"), Actual.Minimum, Expected.Minimum);
		TestEqual(TEXT("tier range maximum is frozen"), Actual.Maximum, Expected.Maximum);
	}
	return true;
}

#endif
