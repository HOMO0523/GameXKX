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
		const int32 Value,
		const int32 TriggersPerRound = INDEX_NONE,
		const int32 SecondaryValue = 0)
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
		Definition.SecondaryValue = SecondaryValue;
		Definition.TriggersPerRound = TriggersPerRound >= 0
			? TriggersPerRound
			: Hook == EGameXXKEquipmentSetBonusHook::Passive ? 0 : 1;
		return Definition;
	}

	TArray<FGameXXKEquipmentSetBonusDefinition> BuildDefinitions()
	{
		using K = EGameXXKEquipmentSetBonusKind;
		using S = EGameXXKEquipmentSetBonusScope;
		using H = EGameXXKEquipmentSetBonusHook;
		using U = EGameXXKEquipmentMagnitudeUnit;
		return {
			MakeBonus(TEXT("Set.PoJun.2"), TEXT("每回合首次由穿戴者产生的冲锋被消费后，抽1张牌。"), EGameXXKEquipmentSet::PoJun, 2, K::PoJunChargeDraw, S::Owner, H::PoJunChargeConsumed, U::FlatCount, 1),
			MakeBonus(TEXT("Set.PoJun.4"), TEXT("穿戴者收招后，将该牌的冲锋保存为下回合藏式。"), EGameXXKEquipmentSet::PoJun, 4, K::PoJunFinishStoresCharge, S::Owner, H::PoJunBladeFinish, U::FlatCount, 1),
			MakeBonus(TEXT("Set.PoJun.6"), TEXT("同回合消费冲锋并触发收招：下回合首张主动牌重放基础效果。"), EGameXXKEquipmentSet::PoJun, 6, K::PoJunOpeningFinishReplay, S::Owner, H::PoJunFirstActiveNextRound, U::FlatCount, 1),

			MakeBonus(TEXT("Set.XuanJia.2"), TEXT("穿戴者产生的护甲提高10%。"), EGameXXKEquipmentSet::XuanJia, 2, K::XuanJiaArmorGain, S::Owner, H::Passive, U::BasisPoints, 1000),
			MakeBonus(TEXT("Set.XuanJia.4"), TEXT("我方回合开始时保留50%护甲；每个敌方回合首次格挡后，追加80%攻击伤害。"), EGameXXKEquipmentSet::XuanJia, 4, K::XuanJiaArmorRetentionCounter, S::Owner, H::RoundStart, U::BasisPoints, 5000, 1, 80),
			MakeBonus(TEXT("Set.XuanJia.6"), TEXT("全队唯一。敌方回合首次有友方因攻击损失气血后，全体获得穿戴者40%防御的护甲；穿戴者援护其他友方各1次。"), EGameXXKEquipmentSet::XuanJia, 6, K::XuanJiaTeamGuard, S::Team, H::FirstAllyHealthDamagePerRound, U::BasisPoints, 4000, 1, 1),

			MakeBonus(TEXT("Set.QingNang.2"), TEXT("每回合首次打出2费及以上牌：抽1张牌。"), EGameXXKEquipmentSet::QingNang, 2, K::QingNangHighCostDraw, S::Team, H::QingNangHighCostActive, U::FlatCount, 1),
			MakeBonus(TEXT("Set.QingNang.4"), TEXT("每回合首次打出2费及以上牌：抽1张牌；全队失去至多1点气血，再回复2点。"), EGameXXKEquipmentSet::QingNang, 4, K::QingNangHighCostBloodCycle, S::Team, H::QingNangHighCostActive, U::FlatCount, 1),
			MakeBonus(TEXT("Set.QingNang.6"), TEXT("每回合首次打出2费及以上牌：抽1张牌；全队失去至多1点气血，再回复2点；回复1点气力。"), EGameXXKEquipmentSet::QingNang, 6, K::QingNangHighCostEnergyCycle, S::Team, H::QingNangHighCostActive, U::FlatCount, 1),

			MakeBonus(TEXT("Set.ZhuiFeng.2"), TEXT("全队每主动打出2张牌，抽1张牌。"), EGameXXKEquipmentSet::ZhuiFeng, 2, K::ZhuiFengPairDraw, S::Team, H::ZhuiFengActiveCardCount, U::FlatCount, 1, 0),
			MakeBonus(TEXT("Set.ZhuiFeng.4"), TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回复1点气力。"), EGameXXKEquipmentSet::ZhuiFeng, 4, K::ZhuiFengSecondCardEnergy, S::Team, H::ZhuiFengActiveCardCount, U::FlatCount, 1, 0),
			MakeBonus(TEXT("Set.ZhuiFeng.6"), TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回1气，第4张再回1气、全队蓄力1并抽1张。"), EGameXXKEquipmentSet::ZhuiFeng, 6, K::ZhuiFengFourthCardCycle, S::Team, H::ZhuiFengActiveCardCount, U::FlatCount, 1, 0),

			MakeBonus(TEXT("Set.ShiGu.2"), TEXT("每张牌首次对一个目标施加流血、中毒或灼烧时，施加1层蚀伤。"), EGameXXKEquipmentSet::ShiGu, 2, K::ShiGuCardTargetRot, S::Owner, H::ShiGuDotApplied, U::FlatCount, 1, 0),
			MakeBonus(TEXT("Set.ShiGu.4"), TEXT("每回合首次使目标同时具有至少2种流血、中毒或灼烧时，自动毒爆1次。"), EGameXXKEquipmentSet::ShiGu, 4, K::ShiGuFirstDualDotExplosion, S::Owner, H::ShiGuDualDotEstablished, U::FlatCount, 1),
			MakeBonus(TEXT("Set.ShiGu.6"), TEXT("每回合首次毒爆不减少流血、中毒和灼烧层数。"), EGameXXKEquipmentSet::ShiGu, 6, K::ShiGuFirstExplosionPreservesDots, S::Owner, H::ShiGuToxicExplosion, U::FlatCount, 1),

			MakeBonus(TEXT("Set.ShanHe.2"), TEXT("每回合首次打出地势牌后，抽1张。"), EGameXXKEquipmentSet::ShanHe, 2, K::ShanHeTerrainPower, S::Owner, H::TerrainSynergyCard, U::FlatCount, 1),
			MakeBonus(TEXT("Set.ShanHe.4"), TEXT("每回合首张地势牌少耗1气；结算后，其他友方各回复2内力。"), EGameXXKEquipmentSet::ShanHe, 4, K::ShanHeTerrainCardFormation, S::Owner, H::TerrainSynergyCard, U::FlatCount, 1, 1, 2),
			MakeBonus(TEXT("Set.ShanHe.6"), TEXT("全队唯一。每个我方回合开始时，由穿戴者触发1次当前地势。"), EGameXXKEquipmentSet::ShanHe, 6, K::ShanHeTeamFormationCore, S::Team, H::RoundStart, U::FlatCount, 1),
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

FText FGameXXKEquipmentSetCatalog::GetSetDisplayName(const EGameXXKEquipmentSet Set)
{
	switch (Set)
	{
	case EGameXXKEquipmentSet::PoJun: return NSLOCTEXT("GameXXKEquipmentSet", "SetPoJun", "破军");
	case EGameXXKEquipmentSet::XuanJia: return NSLOCTEXT("GameXXKEquipmentSet", "SetXuanJia", "玄甲");
	case EGameXXKEquipmentSet::QingNang: return NSLOCTEXT("GameXXKEquipmentSet", "SetQingNang", "青囊");
	case EGameXXKEquipmentSet::ZhuiFeng: return NSLOCTEXT("GameXXKEquipmentSet", "SetZhuiFeng", "追风");
	case EGameXXKEquipmentSet::ShiGu: return NSLOCTEXT("GameXXKEquipmentSet", "SetShiGu", "蚀骨");
	case EGameXXKEquipmentSet::ShanHe: return NSLOCTEXT("GameXXKEquipmentSet", "SetShanHe", "山河");
	case EGameXXKEquipmentSet::Starter: return NSLOCTEXT("GameXXKEquipmentSet", "SetStarter", "基础");
	default: return FText::GetEmpty();
	}
}
