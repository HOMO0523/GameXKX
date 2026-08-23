#include "GameXXKRelicCatalog.h"

#include "GameXXKCardQualityRules.h"

namespace
{
	FGameXXKRelicDefinition MakeRelic(
		const TCHAR* Id,
		const TCHAR* Name,
		const TCHAR* Description,
		const TCHAR* IconSlug,
		EGameXXKRelicTrigger Trigger,
		EGameXXKRelicEffectKind Effect,
		int32 Magnitude,
		bool bStackable = false,
		bool bOfferEligible = true)
	{
		FGameXXKRelicDefinition Definition;
		Definition.Id = FName(Id);
		Definition.DisplayName = FText::FromString(Name);
		Definition.Description = FText::FromString(Description);
		Definition.IconTexturePath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_%s.T_Relic_%s"), IconSlug, IconSlug));
		Definition.BaseQuality = FGameXXKCardQualityRules::GetRelicBaseQuality(Definition.Id);
		Definition.Trigger = Trigger;
		Definition.EffectKind = Effect;
		Definition.Magnitude = Magnitude;
		Definition.bStackable = bStackable;
		Definition.bOfferEligible = bOfferEligible;
		return Definition;
	}

	TArray<FGameXXKRelicDefinition> BuildRelics()
	{
		using T = EGameXXKRelicTrigger;
		using E = EGameXXKRelicEffectKind;
		return {
			MakeRelic(TEXT("Relic.AncientCoin"), TEXT("古铜方孔钱"), TEXT("战斗开始时，全队获得4点护甲。"), TEXT("AncientCoin"), T::BattleStart, E::GainPartyArmor, 4),
			MakeRelic(TEXT("Relic.JadeBell"), TEXT("青玉铃"), TEXT("战斗开始时，全队恢复3点内力。"), TEXT("JadeBell"), T::BattleStart, E::RestorePartyMana, 3),
			MakeRelic(TEXT("Relic.BambooTally"), TEXT("竹节令"), TEXT("战斗开始时，额外获得1点气力。"), TEXT("BambooTally"), T::BattleStart, E::GainSharedEnergy, 1),
			MakeRelic(TEXT("Relic.TigerSeal"), TEXT("虎纹兵符"), TEXT("战斗开始时，全队攻击提高2点。"), TEXT("TigerSeal"), T::BattleStart, E::IncreasePartyAttack, 2),
			MakeRelic(TEXT("Relic.MedicineGourd"), TEXT("青釉药葫"), TEXT("战斗开始时，全队恢复8点气血。"), TEXT("MedicineGourd"), T::BattleStart, E::HealParty, 8),
			MakeRelic(TEXT("Relic.InkTalisman"), TEXT("镇煞墨符"), TEXT("战斗开始时，所有敌人获得2层中毒。"), TEXT("InkTalisman"), T::BattleStart, E::PoisonAllEnemies, 2),
			MakeRelic(TEXT("Relic.CloudMirror"), TEXT("云纹古镜"), TEXT("战斗开始时，全队防御提高2点。"), TEXT("CloudMirror"), T::BattleStart, E::IncreasePartyDefense, 2),
			MakeRelic(TEXT("Relic.StoneBead"), TEXT("山石念珠"), TEXT("每回合开始时，主角获得3点护甲。"), TEXT("StoneBead"), T::PlayerRoundStart, E::GainHeroArmor, 3, true),
			MakeRelic(TEXT("Relic.CraneFeather"), TEXT("鹤羽"), TEXT("每回合开始时，额外抽1张牌。"), TEXT("CraneFeather"), T::PlayerRoundStart, E::DrawCards, 1),
			MakeRelic(TEXT("Relic.IronKnot"), TEXT("玄铁结"), TEXT("每回合开始时，全队获得2点护甲。"), TEXT("IronKnot"), T::PlayerRoundStart, E::GainPartyArmor, 2, true),
			MakeRelic(TEXT("Relic.TeaBrick"), TEXT("陈香茶砖"), TEXT("每回合开始时，主角恢复2点内力。"), TEXT("TeaBrick"), T::PlayerRoundStart, E::RestoreHeroMana, 2),
			MakeRelic(TEXT("Relic.Compass"), TEXT("寻路司南"), TEXT("每回合开始时，额外揭示1张敌方意图。"), TEXT("Compass"), T::PlayerRoundStart, E::RevealEnemyIntent, 1),
			MakeRelic(TEXT("Relic.RedCord"), TEXT("同心红绳"), TEXT("每回合结束时，全队恢复3点气血。"), TEXT("RedCord"), T::PlayerRoundEnd, E::HealParty, 3, true),
			MakeRelic(TEXT("Relic.BronzeNeedle"), TEXT("定脉铜针"), TEXT("每回合结束时，全队恢复2点内力。"), TEXT("BronzeNeedle"), T::PlayerRoundEnd, E::RestorePartyMana, 2),
			MakeRelic(TEXT("Relic.RainCape"), TEXT("旧蓑衣"), TEXT("每回合结束时，全队获得2点护甲。"), TEXT("RainCape"), T::PlayerRoundEnd, E::GainPartyArmor, 2),
			MakeRelic(TEXT("Relic.ChessStone"), TEXT("残局黑子"), TEXT("打出卡牌后，牌的主人获得1点护甲。"), TEXT("ChessStone"), T::CardPlayed, E::GainHeroArmor, 1, true),
			MakeRelic(TEXT("Relic.DrumCharm"), TEXT("震山鼓坠"), TEXT("打出卡牌后，对所有敌人造成1点伤害。"), TEXT("DrumCharm"), T::CardPlayed, E::DamageAllEnemies, 1, true),
			MakeRelic(TEXT("Relic.LotusSeed"), TEXT("清心莲子"), TEXT("打出卡牌后，主角恢复1点内力。"), TEXT("LotusSeed"), T::CardPlayed, E::RestoreHeroMana, 1),
			MakeRelic(TEXT("Relic.SwordGuard"), TEXT("旧剑镡"), TEXT("打出卡牌后，全队获得1点护甲。"), TEXT("SwordGuard"), T::CardPlayed, E::GainPartyArmor, 1),
			MakeRelic(TEXT("Relic.OldMap"), TEXT("残山旧图"), TEXT("打出卡牌后，额外抽1张牌。"), TEXT("OldMap"), T::CardPlayed, E::DrawCards, 1),
			MakeRelic(TEXT("Relic.PineCone"), TEXT("雷击松果"), TEXT("受到伤害后，受击者获得2点护甲。"), TEXT("PineCone"), T::DamageTaken, E::ArmorDamagedUnit, 2, true),
			MakeRelic(TEXT("Relic.RiverPearl"), TEXT("江心珠"), TEXT("受到伤害后，受击者恢复1点气血。"), TEXT("RiverPearl"), T::DamageTaken, E::HealDamagedUnit, 1),
			MakeRelic(TEXT("Relic.CandleStub"), TEXT("长明烛心"), TEXT("击败敌人时，全队恢复4点气血。"), TEXT("CandleStub"), T::EnemyDefeated, E::HealParty, 4),
			MakeRelic(TEXT("Relic.FoxMask"), TEXT("旧狐面"), TEXT("击败敌人时，其余敌人获得1层流血。"), TEXT("FoxMask"), T::EnemyDefeated, E::BleedAllEnemies, 1),
			MakeRelic(TEXT("Relic.StoneLion"), TEXT("袖珍石狮"), TEXT("击败敌人时，全队获得3点护甲。"), TEXT("StoneLion"), T::EnemyDefeated, E::GainPartyArmor, 3),
			MakeRelic(TEXT("Relic.WineCup"), TEXT("缺口酒盏"), TEXT("完成路线节点时获得3行旅钱。"), TEXT("WineCup"), T::RouteNodeCompleted, E::GainRouteTravelMoney, 3, true),
			MakeRelic(TEXT("Relic.HerbBasket"), TEXT("百草小篓"), TEXT("完成路线节点时，主角恢复3点气血。"), TEXT("HerbBasket"), T::RouteNodeCompleted, E::HealPlayer, 3),
			MakeRelic(TEXT("Relic.PaperCrane"), TEXT("祈愿纸鹤"), TEXT("完成路线节点时，本路线最大气血提高2点。"), TEXT("PaperCrane"), T::RouteNodeCompleted, E::GainRouteMaxHealth, 2),
			MakeRelic(TEXT("Relic.BrokenArrow"), TEXT("折锋箭簇"), TEXT("完成路线节点时，本路线攻击提高1点。"), TEXT("BrokenArrow"), T::RouteNodeCompleted, E::GainRouteAttack, 1),
			MakeRelic(TEXT("Relic.MoonDisc"), TEXT("月白玉璧"), TEXT("完成路线节点时，本路线最大内力提高1点。"), TEXT("MoonDisc"), T::RouteNodeCompleted, E::GainRouteMaxMana, 1),
			MakeRelic(TEXT("Relic.LifeSavingTalisman"), TEXT("保命护符"), TEXT("战斗中任一角色气血低于50%时，消耗此遗物，使全队恢复30%最大气血。"), TEXT("LifeSavingTalisman"), T::DamageTaken, E::EmergencyHealPartyPercent, 30, false, false)
		};
	}
}

const TArray<FGameXXKRelicDefinition>& FGameXXKRelicCatalog::GetAllDefinitions()
{
	static const TArray<FGameXXKRelicDefinition> Definitions = []
	{
		TArray<FGameXXKRelicDefinition> Relics = BuildRelics();
		FString QualityValidationError;
		if (!FGameXXKCardQualityRules::ValidateRelicCatalog(Relics, QualityValidationError))
		{
			UE_LOG(LogTemp, Fatal, TEXT("Invalid relic quality catalog: %s"), *QualityValidationError);
		}
		return Relics;
	}();
	return Definitions;
}

const FGameXXKRelicDefinition* FGameXXKRelicCatalog::FindDefinition(const FName RelicId)
{
	return GetAllDefinitions().FindByPredicate([RelicId](const FGameXXKRelicDefinition& Definition)
	{
		return Definition.Id == RelicId;
	});
}
