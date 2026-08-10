#include "GameXXKCardText.h"

#include "GameXXKCardQualityRules.h"

namespace
{
	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	EGameXXKCardQuality ResolveQuality(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardQuality RequestedQuality)
	{
		if (IsConcreteQuality(RequestedQuality))
		{
			return RequestedQuality;
		}
		return IsConcreteQuality(Definition.BaseQuality)
			? Definition.BaseQuality
			: EGameXXKCardQuality::Common;
	}

	FString DescribeStatus(EGameXXKCardStatus Status)
	{
		switch (Status)
		{
		case EGameXXKCardStatus::None: return TEXT("无状态");
		case EGameXXKCardStatus::Momentum: return TEXT("气势");
		case EGameXXKCardStatus::Agility: return TEXT("灵动");
		case EGameXXKCardStatus::Vulnerability: return TEXT("破绽");
		case EGameXXKCardStatus::Bleed: return TEXT("流血");
		case EGameXXKCardStatus::Poison: return TEXT("中毒");
		case EGameXXKCardStatus::Burn: return TEXT("灼烧");
		case EGameXXKCardStatus::Mark: return TEXT("标记");
		case EGameXXKCardStatus::Guard: return TEXT("守护");
		case EGameXXKCardStatus::DamageOverTime: return TEXT("蚀伤");
		case EGameXXKCardStatus::CannotReceiveVulnerability: return TEXT("破绽免疫");
		case EGameXXKCardStatus::NextAttackBonus: return TEXT("追击标记");
		case EGameXXKCardStatus::NextAttackAppliesVulnerability: return TEXT("破绽追击");
		case EGameXXKCardStatus::NextHealingBonus: return TEXT("疗愈增幅");
		case EGameXXKCardStatus::TerrainBonusDouble: return TEXT("地形双效");
		case EGameXXKCardStatus::NextTerrainCardFree: return TEXT("地形免耗");
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction: return TEXT("地形减耗");
		case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack: return TEXT("代挡");
		case EGameXXKCardStatus::TerrainBonusDoubleThisRound: return TEXT("本回合地形双效");
		case EGameXXKCardStatus::Medicine: return TEXT("药效");
		case EGameXXKCardStatus::Weak: return TEXT("虚弱");
		case EGameXXKCardStatus::Wealth: return TEXT("财富");
		case EGameXXKCardStatus::Rage: return TEXT("狂怒");
		case EGameXXKCardStatus::Prey: return TEXT("猎物");
		case EGameXXKCardStatus::Charge: return TEXT("蓄力");
		case EGameXXKCardStatus::Counter: return TEXT("反击");
		case EGameXXKCardStatus::Block: return TEXT("格挡");
		case EGameXXKCardStatus::Invalid:
		default: return TEXT("未知状态");
		}
	}

	FString DescribeTerrain(EGameXXKCardTerrain Terrain)
	{
		switch (Terrain)
		{
		case EGameXXKCardTerrain::Plain: return TEXT("平原");
		case EGameXXKCardTerrain::Cliff: return TEXT("断崖");
		case EGameXXKCardTerrain::Forest: return TEXT("山林");
		case EGameXXKCardTerrain::WaterShore: return TEXT("水岸");
		case EGameXXKCardTerrain::Ferry: return TEXT("渡口");
		case EGameXXKCardTerrain::Village: return TEXT("村镇");
		case EGameXXKCardTerrain::Cave: return TEXT("洞窟");
		default: return TEXT("无效地形");
		}
	}

	FString DescribeEffectTarget(EGameXXKCardEffectTarget Target)
	{
		switch (Target)
		{
		case EGameXXKCardEffectTarget::CardOwner: return TEXT("出牌者");
		case EGameXXKCardEffectTarget::SelectedTarget: return TEXT("所选目标");
		case EGameXXKCardEffectTarget::AllEnemies: return TEXT("全体敌方");
		case EGameXXKCardEffectTarget::AllAllies: return TEXT("全体友方");
		case EGameXXKCardEffectTarget::AllOtherAllies: return TEXT("其他全体友方");
		case EGameXXKCardEffectTarget::EachLivingAlly: return TEXT("每名存活友方");
		case EGameXXKCardEffectTarget::LowestHealthAlly: return TEXT("生命最低友方");
		case EGameXXKCardEffectTarget::LowestHealthOtherAlly: return TEXT("生命最低的其他友方");
		case EGameXXKCardEffectTarget::Attacker: return TEXT("攻击者");
		case EGameXXKCardEffectTarget::PlayedCard: return TEXT("本次打出的牌");
		case EGameXXKCardEffectTarget::HighestArmorAlly: return TEXT("护甲最高友方");
		default: return TEXT("无效对象");
		}
	}

	FString DescribeTargetMode(EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::None: return TEXT("无需选择（直接施放）");
		case EGameXXKCardTargetMode::Self: return TEXT("自身（直接施放）");
		case EGameXXKCardTargetMode::SingleEnemy: return TEXT("单体敌方（点击后选择目标）");
		case EGameXXKCardTargetMode::SingleAlly: return TEXT("单体友方（点击后选择目标）");
		case EGameXXKCardTargetMode::OtherAlly: return TEXT("其他单体友方（点击后选择目标）");
		case EGameXXKCardTargetMode::AllEnemies: return TEXT("全体敌方（直接施放）");
		case EGameXXKCardTargetMode::AllAllies: return TEXT("全体友方（直接施放）");
		case EGameXXKCardTargetMode::AllOtherAllies: return TEXT("除自身外全体友方（直接施放）");
		case EGameXXKCardTargetMode::RandomEnemy: return TEXT("随机敌方（自动结算）");
		case EGameXXKCardTargetMode::LowestHealthAlly: return TEXT("生命最低友方（自动结算）");
		case EGameXXKCardTargetMode::LowestHealthOtherAlly: return TEXT("生命最低的其他友方（自动结算）");
		case EGameXXKCardTargetMode::AnyLivingUnit: return TEXT("任意存活单位（点击后选择目标）");
		default: return TEXT("无效目标");
		}
	}

	FString DescribeCondition(const FGameXXKCardEffectCondition& Condition)
	{
		TArray<FString> Clauses;
		FString Gate;
		switch (Condition.Type)
		{
		case EGameXXKCardEffectConditionType::None: break;
		case EGameXXKCardEffectConditionType::TargetHasStatus:
			Gate = FString::Printf(TEXT("所选目标具有%s%s层"), *DescribeStatus(Condition.Status), Condition.MinimumStatusStacks > 1 ? *FString::Printf(TEXT("至少%d"), Condition.MinimumStatusStacks) : TEXT(""));
			break;
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime: Gate = TEXT("所选目标具有流血、中毒、灼烧或蚀伤"); break;
		case EGameXXKCardEffectConditionType::OwnerHasStatus:
			Gate = FString::Printf(TEXT("出牌者具有%s%s层"), *DescribeStatus(Condition.Status), Condition.MinimumStatusStacks > 1 ? *FString::Printf(TEXT("至少%d"), Condition.MinimumStatusStacks) : TEXT(""));
			break;
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast: Gate = FString::Printf(TEXT("出牌者护甲不少于%d"), Condition.MinimumArmor); break;
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent: Gate = FString::Printf(TEXT("出牌者生命低于%.0f%%"), Condition.HealthPercentThreshold); break;
		case EGameXXKCardEffectConditionType::TargetHealthBelowPercent: Gate = FString::Printf(TEXT("所选目标生命低于%.0f%%"), Condition.HealthPercentThreshold); break;
		case EGameXXKCardEffectConditionType::TerrainIsAny:
			Gate = Condition.AlternateTerrain == EGameXXKCardTerrain::Invalid
				? FString::Printf(TEXT("地形为%s"), *DescribeTerrain(Condition.Terrain))
				: FString::Printf(TEXT("地形为%s或%s"), *DescribeTerrain(Condition.Terrain), *DescribeTerrain(Condition.AlternateTerrain));
			break;
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime: Gate = TEXT("出牌者具有流血、中毒、灼烧或蚀伤"); break;
		default: Gate = TEXT("无效条件"); break;
		}
		if (!Gate.IsEmpty())
		{
			Clauses.Add(Condition.bNegate ? FString::Printf(TEXT("当%s不成立"), *Gate) : FString::Printf(TEXT("当%s"), *Gate));
		}
		if (Condition.bConsumeStatus)
		{
			Clauses.Add(Condition.MaxConsumedStatusStacks == 0
				? FString::Printf(TEXT("消耗全部%s"), *DescribeStatus(Condition.Status))
				: FString::Printf(TEXT("消耗至多%d层%s"), Condition.MaxConsumedStatusStacks, *DescribeStatus(Condition.Status)));
		}
		if (Condition.bScaleMagnitudeByConsumedStacks)
		{
			Clauses.Add(TEXT("数值按消耗层数结算"));
		}
		if (Condition.bConsumeOwnerArmor)
		{
			Clauses.Add(Condition.MaxConsumedArmor == 0
				? TEXT("消耗出牌者全部护甲")
				: FString::Printf(TEXT("消耗出牌者至多%d点护甲"), Condition.MaxConsumedArmor));
		}
		return FString::Join(Clauses, TEXT("；"));
	}

	FString DescribeModifierTrigger(EGameXXKCardBattleModifierTrigger Trigger)
	{
		switch (Trigger)
		{
		case EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound: return TEXT("本回合首次受到直接伤害时");
		case EGameXXKCardBattleModifierTrigger::OnCardPlayed: return TEXT("打出牌时");
		case EGameXXKCardBattleModifierTrigger::OnNextAttack: return TEXT("下次攻击时");
		case EGameXXKCardBattleModifierTrigger::OnNextHealing: return TEXT("下次治疗时");
		case EGameXXKCardBattleModifierTrigger::EndOfRound: return TEXT("回合结束时");
		case EGameXXKCardBattleModifierTrigger::OnSingleTargetEnemyAttack: return TEXT("敌方单体攻击牌结算时");
		case EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard: return TEXT("下一张主动牌结算前");
		case EGameXXKCardBattleModifierTrigger::AfterNextActiveCard: return TEXT("下一张主动牌结算后");
		case EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart: return TEXT("下个玩家回合开始时");
		case EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound: return TEXT("下个玩家回合第一张主动牌结算前");
		case EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound: return TEXT("下个玩家回合第一张主动牌结算后");
		case EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound: return TEXT("下个玩家回合首次主动攻击指定状态目标时");
		case EGameXXKCardBattleModifierTrigger::AfterEachActiveCard: return TEXT("每张主动牌结算后");
		default: return TEXT("无效触发");
		}
	}

	FString DescribeModifierExpiry(EGameXXKCardModifierExpiry Expiry, int32 RemainingTriggers)
	{
		switch (Expiry)
		{
		case EGameXXKCardModifierExpiry::AfterTriggerCount: return FString::Printf(TEXT("触发%d次后失效"), RemainingTriggers);
		case EGameXXKCardModifierExpiry::EndOfCurrentRound: return TEXT("本回合结束时失效");
		case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount: return FString::Printf(TEXT("本回合结束或触发%d次后失效"), RemainingTriggers);
		default: return TEXT("无效持续时间");
		}
	}

	FString DescribeEffectSource(const EGameXXKCardEffectSource Source)
	{
		switch (Source)
		{
		case EGameXXKCardEffectSource::CardOwner: return TEXT("出牌者");
		case EGameXXKCardEffectSource::SelectedTarget: return TEXT("所选目标");
		case EGameXXKCardEffectSource::HighestArmorAlly: return TEXT("护甲最高友方");
		case EGameXXKCardEffectSource::Invalid:
		default: return TEXT("未知来源");
		}
	}

	FString DescribeEffectType(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget TargetValue,
		const EGameXXKCardEffectSource SourceValue,
		const int32 Magnitude,
		const int32 SecondaryMagnitude,
		const int32 HitCount,
		const EGameXXKCardStatus Status)
	{
		const FString Target = DescribeEffectTarget(TargetValue);
		const FString Source = DescribeEffectSource(SourceValue);
		const FString HitSuffix = HitCount > 1 ? FString::Printf(TEXT("，共%d段"), HitCount) : FString();
		switch (Type)
		{
		case EGameXXKCardEffectType::DamagePercentAttack: return FString::Printf(TEXT("%s造成%d%%攻击伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::DamageFlat: return FString::Printf(TEXT("%s受到%d点直接伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::LoseHealth: return FString::Printf(TEXT("%s失去%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::Heal:
			return SecondaryMagnitude > 0
				? FString::Printf(TEXT("%s按本牌造成生命伤害的%d%%恢复生命，最多%d点"), *Target, Magnitude, SecondaryMagnitude)
				: FString::Printf(TEXT("%s恢复%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::AddArmor: return FString::Printf(TEXT("%s获得%d点护甲"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainMana: return FString::Printf(TEXT("%s获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainEnergy: return FString::Printf(TEXT("%s回复%d点气力"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainManaPerConsumedStatus: return FString::Printf(TEXT("%s每消耗1层状态获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::DrawCards: return FString::Printf(TEXT("%s抽%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ApplyStatus: return FString::Printf(TEXT("%s获得%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveStatus: return FString::Printf(TEXT("移除%s%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveAnyDamageOverTime: return FString::Printf(TEXT("从%s的流血、中毒、灼烧、蚀伤中依次清除至多%d层"), *Target, Magnitude);
		case EGameXXKCardEffectType::Insight: return FString::Printf(TEXT("%s洞察牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscoverCards: return FString::Printf(TEXT("%s发现%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ReorderCards: return FString::Printf(TEXT("%s重排牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscardCards: return FString::Printf(TEXT("%s弃置%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::IgnoreDefense: return FString::Printf(TEXT("%s本段伤害无视%d点防御"), *Target, Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercent:
			return SecondaryMagnitude > 0
				? FString::Printf(TEXT("目标每有1层指定状态，本段攻击倍率+%d个百分点，最多计算%d层"), Magnitude, SecondaryMagnitude)
				: FString::Printf(TEXT("本段攻击倍率+%d个百分点"), Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus: return FString::Printf(TEXT("每消耗1层状态，本段攻击倍率+%d个百分点"), Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor: return FString::Printf(TEXT("每消耗1点护甲，本段攻击倍率+%d个百分点"), Magnitude);
		case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget: return FString::Printf(TEXT("每名存活友方对%s造成%d%%各自攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::ApplyGuardLink: return FString::Printf(TEXT("%s建立守护关系"), *Target);
		case EGameXXKCardEffectType::ApplyBattleModifier: return FString::Printf(TEXT("%s获得持续效果"), *Target);
		case EGameXXKCardEffectType::ModifyHealingPercent: return FString::Printf(TEXT("%s治疗量调整%d%%"), *Target, Magnitude);
		case EGameXXKCardEffectType::ModifyEnergyCost: return FString::Printf(TEXT("%s气力消耗%+d"), *Target, Magnitude);
		case EGameXXKCardEffectType::RevealEnemyIntent: return FString::Printf(TEXT("%s揭示%d个敌方意图"), *Target, Magnitude);
		case EGameXXKCardEffectType::DoubleTerrainBonus: return FString::Printf(TEXT("%s接下来%d次地势收益翻倍"), *Target, Magnitude);
		case EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks: return FString::Printf(TEXT("%s转移%d次敌方单体攻击"), *Target, Magnitude);
		case EGameXXKCardEffectType::RegisterReaction: return FString::Printf(TEXT("%s登记%d次%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::LoseHealthNonlethal: return FString::Printf(TEXT("%s失去%d点生命，最低保留1点"), *Target, Magnitude);
		case EGameXXKCardEffectType::Cleanse: return FString::Printf(TEXT("清除%s的全部流血、中毒和灼烧"), *Target);
		case EGameXXKCardEffectType::TriggerHighestDamageOverTime: return FString::Printf(TEXT("触发%s层数最高的流血、中毒或灼烧1次，并减少对应状态1层"), *Target);
		case EGameXXKCardEffectType::ResolveToxicExplosion: return FString::Printf(TEXT("对%s毒爆：分别结算流血、中毒、灼烧并各减少1层；蚀伤只追加伤害"), *Target);
		case EGameXXKCardEffectType::HealOrReverseWithMedicine: return FString::Printf(TEXT("消耗出牌者全部药效；若%s为友方，恢复%d+药效层数生命并清除流血、中毒、灼烧；若为敌方，失去%d+药效层数生命"), *Target, Magnitude, Magnitude);
		case EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss: return FString::Printf(TEXT("本牌每实际扣血1名友方，%s获得%d层药效；本次获得至少%d层时再获得1层气势"), *Target, Magnitude, SecondaryMagnitude);
		case EGameXXKCardEffectType::DamagePercentAttackPlusArmor: return FString::Printf(TEXT("由%s对%s造成%d%%攻击+当前护甲的伤害，不消耗护甲"), *Source, *Target, Magnitude);
		case EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor: return FString::Printf(TEXT("消耗%s全部护甲，对%s造成（%d%%+每点护甲%d个百分点）攻击伤害"), *Source, *Target, Magnitude, SecondaryMagnitude);
		case EGameXXKCardEffectType::TriggerTerrainBenefit: return FString::Printf(TEXT("触发当前地势收益%d次"), Magnitude);
		case EGameXXKCardEffectType::GainArmorFromCurrentManaPercent: return FString::Printf(TEXT("%s获得等于当前内力%d%%的护甲"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainManaOverflowToArmor: return FString::Printf(TEXT("%s回复%d点内力；溢出内力按%d%%转为护甲"), *Target, SecondaryMagnitude, Magnitude);
		case EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard: return FString::Printf(TEXT("从抽牌堆或弃牌堆检索%d张尚未完成的主角法术任务牌加入手牌"), Magnitude);
		case EGameXXKCardEffectType::TriggerStatus: return FString::Printf(TEXT("触发%s的%s%d次；每次按当前层数造成生命伤害并减少1层"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot: return FString::Printf(TEXT("按%s当前%s层数，逐层造成%d%%攻击伤害"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::ReplayTriggeredCardBase: return TEXT("重放本次触发牌的基础效果");
		case EGameXXKCardEffectType::ReplaySourceCardBase: return TEXT("重放本牌的基础效果");
		case EGameXXKCardEffectType::Invalid:
		default: return TEXT("未知效果");
		}
	}

	FString DescribeModifier(const FGameXXKCardBattleModifier& Modifier)
	{
		if (!Modifier.bPersistent)
		{
			return FString();
		}
		const FString Core = DescribeEffectType(
			Modifier.EffectType,
			Modifier.Target,
			EGameXXKCardEffectSource::CardOwner,
			Modifier.Magnitude,
			0,
			1,
			Modifier.Status);
		TArray<FString> Clauses;
		Clauses.Add(FString::Printf(TEXT("持续效果：%s，%s"), *DescribeModifierTrigger(Modifier.Trigger), *Core));
		if (Modifier.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid)
		{
			Clauses.Add(TEXT("限指定职业触发"));
		}
		if (!Modifier.RequiredTriggeredOwnerId.IsNone())
		{
			Clauses.Add(TEXT("限指定角色触发"));
		}
		if (Modifier.bActivePlayOnly)
		{
			Clauses.Add(TEXT("仅主动出牌触发"));
		}
		if (Modifier.bExcludeSourceUnit)
		{
			Clauses.Add(TEXT("不作用于效果来源单位"));
		}
		if (Modifier.bPreserveTriggeredStatus)
		{
			Clauses.Add(TEXT("触发状态伤害时不减层"));
		}
		if (Modifier.MinimumResult > 0)
		{
			Clauses.Add(FString::Printf(TEXT("前序结果至少为%d"), Modifier.MinimumResult));
		}
		Clauses.Add(DescribeModifierExpiry(Modifier.Expiry, Modifier.RemainingTriggers));
		const FString Condition = DescribeCondition(Modifier.Condition);
		if (!Condition.IsEmpty())
		{
			Clauses.Add(Condition);
		}
		return FString::Join(Clauses, TEXT("；"));
	}

	FString DescribeGuardLink(const FGameXXKCardGuardLink& GuardLink)
	{
		if (GuardLink.RedirectPolicy == EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian)
		{
			return FString::Printf(TEXT("由%s援护%s，转移%d次下一次单体直接攻击"), *DescribeEffectTarget(GuardLink.Guardian), *DescribeEffectTarget(GuardLink.ProtectedUnit), GuardLink.Stacks);
		}
		return TEXT("无效援护关系");
	}

	FString DescribeModeOverride(const FGameXXKCardTargetModeOverride& Override)
	{
		FString Gate;
		switch (Override.ConditionType)
		{
		case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
			Gate = Override.AlternateTerrain == EGameXXKCardTerrain::Invalid
				? FString::Printf(TEXT("地形为%s"), *DescribeTerrain(Override.Terrain))
				: FString::Printf(TEXT("地形为%s或%s"), *DescribeTerrain(Override.Terrain), *DescribeTerrain(Override.AlternateTerrain));
			break;
		case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus: Gate = FString::Printf(TEXT("出牌者具有%s"), *DescribeStatus(Override.Status)); break;
		case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus: Gate = FString::Printf(TEXT("所选目标具有%s"), *DescribeStatus(Override.Status)); break;
		default: return TEXT("无效目标切换条件");
		}
		return FString::Printf(TEXT("当%s时，目标改为%s"), *Gate, *DescribeTargetMode(Override.Mode));
	}

	FString DescribeOwner(const FGameXXKCardDefinition& Definition)
	{
		switch (Definition.Owner)
		{
		case EGameXXKCardOwner::Hero: return TEXT("主角卡");
		case EGameXXKCardOwner::Profession: return TEXT("伙伴职业卡");
		case EGameXXKCardOwner::QuestNpc: return TEXT("任务 NPC 支援卡");
		case EGameXXKCardOwner::Route: return TEXT("路线临时卡");
		default: return TEXT("无效归属");
		}
	}

	FString DescribeRarity(EGameXXKCardRarity Rarity)
	{
		switch (Rarity)
		{
		case EGameXXKCardRarity::Permanent: return TEXT("固定");
		case EGameXXKCardRarity::Common: return TEXT("普通");
		case EGameXXKCardRarity::Rare: return TEXT("稀有");
		case EGameXXKCardRarity::Boss: return TEXT("首领");
		default: return TEXT("无效稀有度");
		}
	}

	FString DescribePreviewState(const FGameXXKCardPlayPreview& Preview)
	{
		if (!Preview.bCanPlay)
		{
			return FString::Printf(TEXT("当前不可出：%s"), Preview.FailureReason.IsEmpty() ? TEXT("不满足出牌条件") : *Preview.FailureReason);
		}
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			int32 LegalCount = 0;
			for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
			{
				LegalCount += Candidate.bCanSelect ? 1 : 0;
			}
			return FString::Printf(TEXT("请选择 %d 个高亮合法目标，箭头会跟随鼠标"), LegalCount);
		}
		if (Preview.TargetRequest.bRequiresRandomResolution)
		{
			return TEXT("点击后自动随机结算");
		}
		return TEXT("点击后立即施放");
	}

	FString DescribeEffect(const FGameXXKCardEffect& Effect)
	{
		FString Line;
		if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			Line = DescribeModifier(Effect.Modifier);
		}
		else if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
		{
			Line = DescribeGuardLink(Effect.GuardLink);
		}
		else if (Effect.Type == EGameXXKCardEffectType::TriggerTerrainBenefit)
		{
			const FString Terrain = Effect.TerrainOverride == EGameXXKCardTerrain::Invalid
				? TEXT("当前地势")
				: DescribeTerrain(Effect.TerrainOverride);
			Line = Effect.SecondaryMagnitude > 0
				? FString::Printf(TEXT("触发%s收益%d次；若本回合已实际换场，改为%d次"), *Terrain, Effect.Magnitude, Effect.SecondaryMagnitude)
				: FString::Printf(TEXT("触发%s收益%d次"), *Terrain, Effect.Magnitude);
		}
		else
		{
			Line = DescribeEffectType(
				Effect.Type,
				Effect.Target,
				Effect.Source,
				Effect.Magnitude,
				Effect.SecondaryMagnitude,
				Effect.HitCount,
				Effect.Status);
		}

		const FString Condition = DescribeCondition(Effect.Condition);
		if (!Condition.IsEmpty())
		{
			Line += FString::Printf(TEXT("（%s）"), *Condition);
		}
		if (!Effect.ResultRef.IsNone())
		{
			Line += TEXT("（仅当前述结果成功时）");
		}
		if (!Effect.ConsumedStackResultRef.IsNone())
		{
			Line += Effect.Type == EGameXXKCardEffectType::GainEnergy && Effect.SecondaryMagnitude > 0
				? FString::Printf(TEXT("（前述消耗至少%d层时，仅结算一次）"), Effect.SecondaryMagnitude)
				: TEXT("（仅当前述消耗成功时）");
		}
		return Line;
	}

	FString DescribeEffectArray(const TArray<FGameXXKCardEffect>& Effects)
	{
		TArray<FString> Lines;
		Lines.Reserve(Effects.Num());
		for (const FGameXXKCardEffect& Effect : Effects)
		{
			Lines.Add(DescribeEffect(Effect));
		}
		return FString::Join(Lines, TEXT("；"));
	}

	bool EffectUsesStatus(const FGameXXKCardEffect& Effect, const EGameXXKCardStatus Status)
	{
		return Effect.Status == Status
			|| Effect.Condition.Status == Status
			|| (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& (Effect.Modifier.Status == Status || Effect.Modifier.Condition.Status == Status));
	}

	template <typename PredicateType>
	bool AnyDefinitionEffectMatches(const FGameXXKCardDefinition& Definition, PredicateType&& Predicate)
	{
		return Definition.Effects.ContainsByPredicate(Predicate)
			|| Definition.ChargeEffects.ContainsByPredicate(Predicate)
			|| Definition.FinishEffects.ContainsByPredicate(Predicate);
	}

	FString DescribeHeavyArrowPayload(const FGameXXKHeavyArrowRule& Rule)
	{
		switch (Rule.Kind)
		{
		case EGameXXKHeavyArrowKind::ExtraAttackPerCharge:
			return FString::Printf(TEXT("每消耗1层，追加1段%d%%攻击伤害。"), Rule.MagnitudePerCharge);
		case EGameXXKHeavyArrowKind::ToxicExplosionPerCharge:
			return TEXT("每消耗1层，再触发1次毒爆。");
		case EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge:
		{
			FString Text = FString::Printf(
				TEXT("每消耗1层，本牌首段攻击倍率+%d个百分点并抽%d张牌"),
				Rule.MagnitudePerCharge,
				Rule.DrawPerCharge);
			if (Rule.MinimumChargeForEnergy > 0)
			{
				Text += FString::Printf(TEXT("；消耗至少%d层时回复%d点气力一次"), Rule.MinimumChargeForEnergy, Rule.EnergyGain);
			}
			return Text + TEXT("。");
		}
		case EGameXXKHeavyArrowKind::None:
		default:
			return FString();
		}
	}

	FString DescribeSpellTaskReward(const EGameXXKHeroSpellTaskReward Reward)
	{
		switch (Reward)
		{
		case EGameXXKHeroSpellTaskReward::Fire:
			return TEXT("首牌奖励·炎：全体敌方获得8层灼烧，再触发2次灼烧伤害。");
		case EGameXXKHeroSpellTaskReward::Ice:
			return TEXT("首牌奖励·冰：消耗自身全部护甲；按护甲快照层数，对敌方全体逐段造成20%攻击伤害。");
		case EGameXXKHeroSpellTaskReward::Lightning:
			return TEXT("首牌奖励·雷：全体敌方获得3层标记，再按各目标标记快照逐层造成60%攻击伤害。");
		case EGameXXKHeroSpellTaskReward::Universal:
			return TEXT("首牌奖励·通用：抽4张牌、回复2点气力；本回合后续主角牌气力消耗-1。");
		case EGameXXKHeroSpellTaskReward::None:
		default:
			return FString();
		}
	}

	FString DescribeEffectsResolved(const FGameXXKCardDefinition& EffectiveDefinition)
	{
		TArray<FString> Lines;
		for (const FGameXXKCardEffect& Effect : EffectiveDefinition.Effects)
		{
			Lines.Add(DescribeEffect(Effect));
		}

		if (EffectiveDefinition.bExhaustOnPlay)
		{
			Lines.Add(TEXT("消耗：打出后进入本局消耗区。"));
		}
		if (!EffectiveDefinition.ChargeEffects.IsEmpty())
		{
			Lines.Add(TEXT("冲锋：本回合第一张主动牌时触发。"));
			Lines.Add(FString::Printf(TEXT("冲锋效果：%s"), *DescribeEffectArray(EffectiveDefinition.ChargeEffects)));
		}
		if (!EffectiveDefinition.FinishEffects.IsEmpty())
		{
			Lines.Add(TEXT("收招：作为结束回合前最后一张主动牌时触发。"));
			Lines.Add(FString::Printf(TEXT("收招效果：%s"), *DescribeEffectArray(EffectiveDefinition.FinishEffects)));
		}
		if (EffectiveDefinition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None)
		{
			Lines.Add(TEXT("重箭：消耗全部蓄力，逐层触发本牌重箭效果。"));
			Lines.Add(FString::Printf(TEXT("重箭效果：%s"), *DescribeHeavyArrowPayload(EffectiveDefinition.HeavyArrow)));
		}

		const bool bUsesMedicine = AnyDefinitionEffectMatches(EffectiveDefinition, [](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::HealOrReverseWithMedicine
				|| Effect.Type == EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss
				|| EffectUsesStatus(Effect, EGameXXKCardStatus::Medicine);
		});
		if (bUsesMedicine)
		{
			Lines.Add(TEXT("药效：下一次治疗或治疗反转每层＋1；结算时全部消耗。"));
		}

		const bool bUsesCounter = AnyDefinitionEffectMatches(EffectiveDefinition, [](const FGameXXKCardEffect& Effect)
		{
			return EffectUsesStatus(Effect, EGameXXKCardStatus::Counter);
		});
		if (bUsesCounter)
		{
			Lines.Add(TEXT("反击：敌方单体攻击牌结算后，造成100%攻击并消耗1次。"));
		}
		const bool bUsesBlock = AnyDefinitionEffectMatches(EffectiveDefinition, [](const FGameXXKCardEffect& Effect)
		{
			return EffectUsesStatus(Effect, EGameXXKCardStatus::Block);
		});
		if (bUsesBlock)
		{
			Lines.Add(TEXT("格挡：敌方单体攻击牌结算后，造成100%攻击＋当前护甲并消耗1次。"));
		}

		const bool bUsesTerrainBenefit = AnyDefinitionEffectMatches(EffectiveDefinition, [](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::TriggerTerrainBenefit;
		});
		if (bUsesTerrainBenefit)
		{
			Lines.Add(TEXT("当前地势收益：按当前地势触发对应效果。"));
		}

		if (EffectiveDefinition.SpellTaskReward != EGameXXKHeroSpellTaskReward::None)
		{
			Lines.Add(TEXT("法术任务：主角8张装备牌各主动打出一次后，依序重放基础效果并触发首牌奖励。"));
			Lines.Add(DescribeSpellTaskReward(EffectiveDefinition.SpellTaskReward));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString DescribeDetailResolved(
		const FGameXXKCardDefinition& EffectiveDefinition,
		const EGameXXKCardQuality ResolvedQuality,
		const FGameXXKCardPlayPreview* Preview)
	{
		TArray<FString> Lines;
		Lines.Add(EffectiveDefinition.DisplayName.ToString());
		Lines.Add(FString::Printf(
			TEXT("来源：%s · %s"),
			*DescribeOwner(EffectiveDefinition),
			*DescribeRarity(EffectiveDefinition.Rarity)));
		Lines.Add(FString::Printf(
			TEXT("品质：%s"),
			*FGameXXKCardQualityRules::GetDisplayName(ResolvedQuality).ToString()));
		Lines.Add(FString::Printf(TEXT("费用：%d 气 / %d 内"), EffectiveDefinition.EnergyCost, EffectiveDefinition.ManaCost));
		Lines.Add(GameXXKCardText::DescribeTarget(EffectiveDefinition.TargetSpec));
		Lines.Add(FString::Printf(TEXT("效果：\n%s"), *DescribeEffectsResolved(EffectiveDefinition)));
		if (Preview)
		{
			Lines.Add(DescribePreviewState(*Preview));
		}
		return FString::Join(Lines, TEXT("\n"));
	}
}

FString GameXXKCardText::DescribeStatusName(const EGameXXKCardStatus Status)
{
	return DescribeStatus(Status);
}

FString GameXXKCardText::DescribeTarget(const FGameXXKCardTargetSpec& TargetSpec)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("目标：%s"), *DescribeTargetMode(TargetSpec.Mode)));
	if (TargetSpec.RequiredStatus != EGameXXKCardStatus::None)
	{
		Lines.Add(FString::Printf(TEXT("目标需具有%s%d层"), *DescribeStatus(TargetSpec.RequiredStatus), TargetSpec.RequiredStatusMinimumStacks));
	}
	if (TargetSpec.ForbiddenStatus != EGameXXKCardStatus::None)
	{
		Lines.Add(FString::Printf(TEXT("目标不能具有%s"), *DescribeStatus(TargetSpec.ForbiddenStatus)));
	}
	if (TargetSpec.MinimumHealthPercent > 0.0f || TargetSpec.MaximumHealthPercent < 100.0f)
	{
		Lines.Add(FString::Printf(TEXT("目标生命范围 %.0f%%–%.0f%%"), TargetSpec.MinimumHealthPercent, TargetSpec.MaximumHealthPercent));
	}
	if (TargetSpec.RequiredTerrain != EGameXXKCardTerrain::Invalid)
	{
		Lines.Add(TargetSpec.AlternateRequiredTerrain == EGameXXKCardTerrain::Invalid
			? FString::Printf(TEXT("需要地形：%s"), *DescribeTerrain(TargetSpec.RequiredTerrain))
			: FString::Printf(TEXT("需要地形：%s或%s"), *DescribeTerrain(TargetSpec.RequiredTerrain), *DescribeTerrain(TargetSpec.AlternateRequiredTerrain)));
	}
	if (TargetSpec.bRequireDifferentFromOwner)
	{
		Lines.Add(TEXT("不能选择出牌者自身"));
	}
	for (const FGameXXKCardTargetModeOverride& Override : TargetSpec.ModeOverrides)
	{
		Lines.Add(DescribeModeOverride(Override));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString GameXXKCardText::DescribeEffects(const FGameXXKCardDefinition& Definition)
{
	return DescribeEffects(Definition, Definition.BaseQuality);
}

FString GameXXKCardText::DescribeEffects(
	const FGameXXKCardDefinition& Definition,
	const EGameXXKCardQuality Quality)
{
	const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(Definition, Quality);
	return DescribeEffectsResolved(EffectiveDefinition);
}

FString GameXXKCardText::DescribeDetail(const FGameXXKCardDefinition& Definition, const FGameXXKCardPlayPreview* Preview)
{
	return DescribeDetail(Definition, Definition.BaseQuality, Preview);
}

FString GameXXKCardText::DescribeDetail(
	const FGameXXKCardDefinition& Definition,
	const EGameXXKCardQuality Quality,
	const FGameXXKCardPlayPreview* Preview)
{
	const EGameXXKCardQuality ResolvedQuality = ResolveQuality(Definition, Quality);
	const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(Definition, Quality);
	return DescribeDetailResolved(EffectiveDefinition, ResolvedQuality, Preview);
}

FString GameXXKCardText::DescribeTooltip(
	const FGameXXKCardDefinition& Definition,
	const FGameXXKCardPlayPreview* Preview,
	const FGameXXKCardTooltipContext& Context)
{
	return DescribeTooltip(Definition, Definition.BaseQuality, Preview, Context);
}

FString GameXXKCardText::DescribeTooltip(
	const FGameXXKCardDefinition& Definition,
	const EGameXXKCardQuality Quality,
	const FGameXXKCardPlayPreview* Preview,
	const FGameXXKCardTooltipContext& Context)
{
	const EGameXXKCardQuality ResolvedQuality = ResolveQuality(Definition, Quality);
	const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(Definition, Quality);
	TArray<FString> Lines;
	Lines.Add(DescribeDetailResolved(EffectiveDefinition, ResolvedQuality, Preview));
	if (!Context.InteractionResult.IsEmpty())
	{
		Lines.Add(Context.InteractionResult);
	}
	if (!Context.UnavailableReason.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("当前不可用：%s"), *Context.UnavailableReason));
	}
	return FString::Join(Lines, TEXT("\n"));
}
