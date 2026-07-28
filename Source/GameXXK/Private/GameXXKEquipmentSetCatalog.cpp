#include "GameXXKEquipmentSetCatalog.h"

namespace
{
	FGameXXKEquipmentSetBonusDefinition MakeBonus(
		const TCHAR* Id,
		const TCHAR* Description,
		const EGameXXKEquipmentSet Set,
		const int32 RequiredPieces,
		const EGameXXKEquipmentSetBonusKind BonusKind,
		const EGameXXKEquipmentSetBonusScope Scope,
		const EGameXXKEquipmentSetBonusHook Hook,
		const EGameXXKEquipmentMagnitudeUnit Unit,
		const int32 Value)
	{
		FGameXXKEquipmentSetBonusDefinition Definition;
		Definition.Id = FName(Id);
		Definition.Description = FText::FromString(Description);
		Definition.Set = Set;
		Definition.RequiredPieces = RequiredPieces;
		Definition.BonusKind = BonusKind;
		Definition.Scope = Scope;
		Definition.Hook = Hook;
		Definition.Unit = Unit;
		Definition.Value = Value;
		Definition.TriggersPerRound = Hook == EGameXXKEquipmentSetBonusHook::Passive ? 0 : 1;
		return Definition;
	}

	TArray<FGameXXKEquipmentSetBonusDefinition> BuildDefinitions()
	{
		using K = EGameXXKEquipmentSetBonusKind;
		using S = EGameXXKEquipmentSetBonusScope;
		using H = EGameXXKEquipmentSetBonusHook;
		using U = EGameXXKEquipmentMagnitudeUnit;
		return {
			MakeBonus(TEXT("Set.PoJun.2"), TEXT("直接伤害提高5%。"), EGameXXKEquipmentSet::PoJun, 2, K::PoJunDirectDamage, S::Owner, H::Passive, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.PoJun.4"), TEXT("每回合首次多段攻击额外施加1层破甲。"), EGameXXKEquipmentSet::PoJun, 4, K::PoJunMultiHitArmorBreak, S::Owner, H::MultiHit, U::FlatCount, 1),
			MakeBonus(TEXT("Set.PoJun.6"), TEXT("每回合首次攻击破甲目标时触发120%追击。"), EGameXXKEquipmentSet::PoJun, 6, K::PoJunFirstAttackFollowUp, S::Owner, H::FirstAttackPerRound, U::BasisPoints, 1200),

			MakeBonus(TEXT("Set.XuanJia.2"), TEXT("获得的护甲提高5%。"), EGameXXKEquipmentSet::XuanJia, 2, K::XuanJiaArmorGain, S::Owner, H::Passive, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.XuanJia.4"), TEXT("回合开始保留护甲并使首次直接受击反击80%。"), EGameXXKEquipmentSet::XuanJia, 4, K::XuanJiaArmorRetentionCounter, S::Owner, H::RoundStart, U::BasisPoints, 800),
			MakeBonus(TEXT("Set.XuanJia.6"), TEXT("每回合首次有友方受到气血伤害时，为全队提供1次护甲与护援。"), EGameXXKEquipmentSet::XuanJia, 6, K::XuanJiaTeamGuard, S::Team, H::FirstAllyHealthDamagePerRound, U::FlatCount, 1),

			MakeBonus(TEXT("Set.QingNang.2"), TEXT("治疗与净化效果提高5%。"), EGameXXKEquipmentSet::QingNang, 2, K::QingNangHealingCleanse, S::Owner, H::Passive, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.QingNang.4"), TEXT("每回合首次净化返还内力，溢出治疗的80%转化为护甲。"), EGameXXKEquipmentSet::QingNang, 4, K::QingNangCleanseOverheal, S::Owner, H::CleanseOrOverheal, U::BasisPoints, 800),
			MakeBonus(TEXT("Set.QingNang.6"), TEXT("每回合首次治疗同时治疗全队并产生1点共享气力。"), EGameXXKEquipmentSet::QingNang, 6, K::QingNangTeamHealEnergy, S::Team, H::FirstHealPerRound, U::FlatCount, 1),

			MakeBonus(TEXT("Set.ZhuiFeng.2"), TEXT("速度提高5%，开战时增加抽牌。"), EGameXXKEquipmentSet::ZhuiFeng, 2, K::ZhuiFengSpeedOpeningDraw, S::Owner, H::BattleStart, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.ZhuiFeng.4"), TEXT("每回合首次连续打出低费牌时恢复1点共享气力。"), EGameXXKEquipmentSet::ZhuiFeng, 4, K::ZhuiFengLowCostEnergy, S::Owner, H::LowCostStreak, U::FlatCount, 1),
			MakeBonus(TEXT("Set.ZhuiFeng.6"), TEXT("每回合首次达到连击数时，下一张牌费用归零并补抽1张。"), EGameXXKEquipmentSet::ZhuiFeng, 6, K::ZhuiFengComboFreeCard, S::Owner, H::ComboThreshold, U::FlatCount, 1),

			MakeBonus(TEXT("Set.ShiGu.2"), TEXT("施加的持续伤害层数提高5%。"), EGameXXKEquipmentSet::ShiGu, 2, K::ShiGuDamageOverTimeStacks, S::Owner, H::Passive, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.ShiGu.4"), TEXT("每回合首次令目标具有多种持续伤害时，额外造成80%伤害。"), EGameXXKEquipmentSet::ShiGu, 4, K::ShiGuMixedDamageOverTime, S::Owner, H::MultipleDamageOverTime, U::BasisPoints, 800),
			MakeBonus(TEXT("Set.ShiGu.6"), TEXT("每回合结束时令持续伤害额外结算1次且不消耗层数。"), EGameXXKEquipmentSet::ShiGu, 6, K::ShiGuExtraDamageOverTimeTick, S::Owner, H::RoundEnd, U::FlatCount, 1),

			MakeBonus(TEXT("Set.ShanHe.2"), TEXT("当前地形效果提高5%。"), EGameXXKEquipmentSet::ShanHe, 2, K::ShanHeTerrainPower, S::Owner, H::Passive, U::BasisPoints, 500),
			MakeBonus(TEXT("Set.ShanHe.4"), TEXT("每回合首张地形联动牌费用降低1并强化相邻队友。"), EGameXXKEquipmentSet::ShanHe, 4, K::ShanHeTerrainCardFormation, S::Owner, H::TerrainSynergyCard, U::FlatCount, 1),
			MakeBonus(TEXT("Set.ShanHe.6"), TEXT("当前地形成为阵眼，向全队提供12%对应增益。"), EGameXXKEquipmentSet::ShanHe, 6, K::ShanHeTeamFormationCore, S::Team, H::Passive, U::BasisPoints, 1200),
		};
	}
}

const TArray<FGameXXKEquipmentSetBonusDefinition>& FGameXXKEquipmentSetCatalog::GetDefinitions()
{
	static const TArray<FGameXXKEquipmentSetBonusDefinition> Definitions = BuildDefinitions();
	return Definitions;
}

const FGameXXKEquipmentSetBonusDefinition* FGameXXKEquipmentSetCatalog::FindDefinition(const FName BonusId)
{
	return GetDefinitions().FindByPredicate(
		[BonusId](const FGameXXKEquipmentSetBonusDefinition& Definition) { return Definition.Id == BonusId; });
}
