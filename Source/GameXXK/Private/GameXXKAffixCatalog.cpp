#include "GameXXKAffixCatalog.h"

namespace
{
	FGameXXKAffixDefinition MakeAffix(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const EGameXXKEquipmentSet Set,
		const EGameXXKEquipmentModifierKind ModifierKind,
		const EGameXXKEquipmentMagnitudeUnit Unit)
	{
		FGameXXKAffixDefinition Definition;
		Definition.Id = FName(Id);
		Definition.DisplayName = FText::FromString(DisplayName);
		Definition.Set = Set;
		Definition.ModifierKind = ModifierKind;
		Definition.Unit = Unit;
		return Definition;
	}

	TArray<FGameXXKAffixDefinition> BuildUniversalDefinitions()
	{
		using K = EGameXXKEquipmentModifierKind;
		using U = EGameXXKEquipmentMagnitudeUnit;
		return {
			MakeAffix(TEXT("Affix.Universal.MaxHealth"), TEXT("强身"), EGameXXKEquipmentSet::Invalid, K::MaxHealth, U::BasisPoints),
			MakeAffix(TEXT("Affix.Universal.MaxMana"), TEXT("纳息"), EGameXXKEquipmentSet::Invalid, K::MaxMana, U::BasisPoints),
			MakeAffix(TEXT("Affix.Universal.Attack"), TEXT("劲力"), EGameXXKEquipmentSet::Invalid, K::Attack, U::BasisPoints),
			MakeAffix(TEXT("Affix.Universal.Defense"), TEXT("坚骨"), EGameXXKEquipmentSet::Invalid, K::Defense, U::BasisPoints),
			MakeAffix(TEXT("Affix.Universal.Speed"), TEXT("轻身"), EGameXXKEquipmentSet::Invalid, K::Speed, U::BasisPoints),
		};
	}

	TArray<FGameXXKAffixDefinition> BuildSetDefinitions(const EGameXXKEquipmentSet Set)
	{
		using K = EGameXXKEquipmentModifierKind;
		using U = EGameXXKEquipmentMagnitudeUnit;
		switch (Set)
		{
		case EGameXXKEquipmentSet::PoJun:
			return {
				MakeAffix(TEXT("Affix.PoJun.DirectDamage"), TEXT("破阵"), Set, K::DirectDamage, U::BasisPoints),
				MakeAffix(TEXT("Affix.PoJun.MultiHitDamage"), TEXT("连锋"), Set, K::MultiHitDamage, U::BasisPoints),
				MakeAffix(TEXT("Affix.PoJun.ArmorBreakStacks"), TEXT("摧甲"), Set, K::ArmorBreakStacks, U::FlatCount),
				MakeAffix(TEXT("Affix.PoJun.VulnerableTargetDamage"), TEXT("乘隙"), Set, K::VulnerableTargetDamage, U::BasisPoints),
				MakeAffix(TEXT("Affix.PoJun.FirstAttackDamage"), TEXT("先声"), Set, K::FirstAttackDamage, U::BasisPoints),
			};
		case EGameXXKEquipmentSet::XuanJia:
			return {
				MakeAffix(TEXT("Affix.XuanJia.ArmorGain"), TEXT("固垒"), Set, K::ArmorGain, U::BasisPoints),
				MakeAffix(TEXT("Affix.XuanJia.ArmorRetention"), TEXT("守一"), Set, K::ArmorRetention, U::BasisPoints),
				MakeAffix(TEXT("Affix.XuanJia.CounterDamage"), TEXT("反震"), Set, K::CounterDamage, U::BasisPoints),
				MakeAffix(TEXT("Affix.XuanJia.GuardReduction"), TEXT("护援"), Set, K::GuardReduction, U::BasisPoints),
				MakeAffix(TEXT("Affix.XuanJia.LowHealthProtection"), TEXT("危守"), Set, K::LowHealthProtection, U::BasisPoints),
			};
		case EGameXXKEquipmentSet::QingNang:
			return {
				MakeAffix(TEXT("Affix.QingNang.Healing"), TEXT("回春"), Set, K::Healing, U::BasisPoints),
				MakeAffix(TEXT("Affix.QingNang.Cleanse"), TEXT("涤尘"), Set, K::Cleanse, U::FlatCount),
				MakeAffix(TEXT("Affix.QingNang.OverhealConversion"), TEXT("余泽"), Set, K::OverhealConversion, U::BasisPoints),
				MakeAffix(TEXT("Affix.QingNang.ManaRecovery"), TEXT("养息"), Set, K::ManaRecovery, U::BasisPoints),
				MakeAffix(TEXT("Affix.QingNang.EmergencyHealing"), TEXT("济危"), Set, K::EmergencyHealing, U::BasisPoints),
			};
		case EGameXXKEquipmentSet::ZhuiFeng:
			return {
				MakeAffix(TEXT("Affix.ZhuiFeng.Draw"), TEXT("掠影"), Set, K::Draw, U::FlatCount),
				MakeAffix(TEXT("Affix.ZhuiFeng.LowCostBonus"), TEXT("轻策"), Set, K::LowCostBonus, U::BasisPoints),
				MakeAffix(TEXT("Affix.ZhuiFeng.SharedEnergy"), TEXT("聚势"), Set, K::SharedEnergy, U::FlatCount),
				MakeAffix(TEXT("Affix.ZhuiFeng.ComboCount"), TEXT("疾连"), Set, K::ComboCount, U::FlatCount),
				MakeAffix(TEXT("Affix.ZhuiFeng.TemporaryCostReduction"), TEXT("省力"), Set, K::TemporaryCostReduction, U::FlatCount),
			};
		case EGameXXKEquipmentSet::ShiGu:
			return {
				MakeAffix(TEXT("Affix.ShiGu.Poison"), TEXT("淬毒"), Set, K::Poison, U::FlatCount),
				MakeAffix(TEXT("Affix.ShiGu.Bleed"), TEXT("蚀血"), Set, K::Bleed, U::FlatCount),
				MakeAffix(TEXT("Affix.ShiGu.Burn"), TEXT("灼骨"), Set, K::Burn, U::FlatCount),
				MakeAffix(TEXT("Affix.ShiGu.DamageOverTime"), TEXT("绵毒"), Set, K::DamageOverTime, U::BasisPoints),
				MakeAffix(TEXT("Affix.ShiGu.StatusRetention"), TEXT("留煞"), Set, K::StatusRetention, U::FlatCount),
			};
		case EGameXXKEquipmentSet::ShanHe:
			return {
				MakeAffix(TEXT("Affix.ShanHe.TerrainPower"), TEXT("借势"), Set, K::TerrainPower, U::BasisPoints),
				MakeAffix(TEXT("Affix.ShanHe.TerrainCostReduction"), TEXT("循地"), Set, K::TerrainCostReduction, U::FlatCount),
				MakeAffix(TEXT("Affix.ShanHe.AdjacentAllyPower"), TEXT("连营"), Set, K::AdjacentAllyPower, U::BasisPoints),
				MakeAffix(TEXT("Affix.ShanHe.FormationPower"), TEXT("布阵"), Set, K::FormationPower, U::BasisPoints),
				MakeAffix(TEXT("Affix.ShanHe.TeamTerrainPower"), TEXT("山河同势"), Set, K::TeamTerrainPower, U::BasisPoints),
			};
		default:
			return {};
		}
	}

	const TArray<FGameXXKAffixDefinition>& UniversalDefinitions()
	{
		static const TArray<FGameXXKAffixDefinition> Definitions = BuildUniversalDefinitions();
		return Definitions;
	}

	const TArray<FGameXXKAffixDefinition>& DefinitionsForSet(const EGameXXKEquipmentSet Set)
	{
		static const TArray<FGameXXKAffixDefinition> Empty;
		static const TArray<FGameXXKAffixDefinition> PoJun = BuildSetDefinitions(EGameXXKEquipmentSet::PoJun);
		static const TArray<FGameXXKAffixDefinition> XuanJia = BuildSetDefinitions(EGameXXKEquipmentSet::XuanJia);
		static const TArray<FGameXXKAffixDefinition> QingNang = BuildSetDefinitions(EGameXXKEquipmentSet::QingNang);
		static const TArray<FGameXXKAffixDefinition> ZhuiFeng = BuildSetDefinitions(EGameXXKEquipmentSet::ZhuiFeng);
		static const TArray<FGameXXKAffixDefinition> ShiGu = BuildSetDefinitions(EGameXXKEquipmentSet::ShiGu);
		static const TArray<FGameXXKAffixDefinition> ShanHe = BuildSetDefinitions(EGameXXKEquipmentSet::ShanHe);
		switch (Set)
		{
		case EGameXXKEquipmentSet::PoJun: return PoJun;
		case EGameXXKEquipmentSet::XuanJia: return XuanJia;
		case EGameXXKEquipmentSet::QingNang: return QingNang;
		case EGameXXKEquipmentSet::ZhuiFeng: return ZhuiFeng;
		case EGameXXKEquipmentSet::ShiGu: return ShiGu;
		case EGameXXKEquipmentSet::ShanHe: return ShanHe;
		default: return Empty;
		}
	}

	TArray<FGameXXKAffixDefinition> BuildAllDefinitions()
	{
		TArray<FGameXXKAffixDefinition> Definitions = UniversalDefinitions();
		Definitions.Reserve(35);
		for (uint8 Value = static_cast<uint8>(EGameXXKEquipmentSet::PoJun); Value <= static_cast<uint8>(EGameXXKEquipmentSet::ShanHe); ++Value)
		{
			Definitions.Append(DefinitionsForSet(static_cast<EGameXXKEquipmentSet>(Value)));
		}
		return Definitions;
	}
}

const TArray<FGameXXKAffixDefinition>& FGameXXKAffixCatalog::GetUniversalDefinitions()
{
	return UniversalDefinitions();
}

const TArray<FGameXXKAffixDefinition>& FGameXXKAffixCatalog::GetSetDefinitions(const EGameXXKEquipmentSet Set)
{
	return DefinitionsForSet(Set);
}

const TArray<FGameXXKAffixDefinition>& FGameXXKAffixCatalog::GetAllDefinitions()
{
	static const TArray<FGameXXKAffixDefinition> Definitions = BuildAllDefinitions();
	return Definitions;
}

const FGameXXKAffixDefinition* FGameXXKAffixCatalog::FindDefinition(const FName AffixId)
{
	return GetAllDefinitions().FindByPredicate(
		[AffixId](const FGameXXKAffixDefinition& Definition) { return Definition.Id == AffixId; });
}

FGameXXKAffixTierWeights FGameXXKAffixCatalog::GetTierWeights(const EGameXXKEquipmentQuality Quality)
{
	FGameXXKAffixTierWeights Weights;
	switch (Quality)
	{
	case EGameXXKEquipmentQuality::Common:
		Weights.Common = 100;
		break;
	case EGameXXKEquipmentQuality::Rare:
		Weights.Common = 70;
		Weights.Rare = 30;
		break;
	case EGameXXKEquipmentQuality::Epic:
		Weights.Common = 50;
		Weights.Rare = 35;
		Weights.Epic = 15;
		break;
	default:
		break;
	}
	return Weights;
}

FGameXXKAffixMagnitudeRange FGameXXKAffixCatalog::GetMagnitudeRange(
	const EGameXXKEquipmentMagnitudeUnit Unit,
	const EGameXXKAffixTier Tier)
{
	FGameXXKAffixMagnitudeRange Range;
	if (Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints)
	{
		switch (Tier)
		{
		case EGameXXKAffixTier::Common: Range.Minimum = 300; Range.Maximum = 500; break;
		case EGameXXKAffixTier::Rare: Range.Minimum = 600; Range.Maximum = 900; break;
		case EGameXXKAffixTier::Epic: Range.Minimum = 1000; Range.Maximum = 1400; break;
		default: break;
		}
	}
	else if (Unit == EGameXXKEquipmentMagnitudeUnit::FlatCount)
	{
		switch (Tier)
		{
		case EGameXXKAffixTier::Common: Range.Minimum = 1; Range.Maximum = 1; break;
		case EGameXXKAffixTier::Rare: Range.Minimum = 1; Range.Maximum = 2; break;
		case EGameXXKAffixTier::Epic: Range.Minimum = 2; Range.Maximum = 3; break;
		default: break;
		}
	}
	return Range;
}
