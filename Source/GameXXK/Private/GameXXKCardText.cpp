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
		case EGameXXKCardEffectTarget::HighestAttackAlly: return TEXT("攻击最高友方");
		case EGameXXKCardEffectTarget::PriorityEnemy: return TEXT("标记最高敌方（同层优先生命比例最低）");
		case EGameXXKCardEffectTarget::SelectedTargetSide: return TEXT("所选目标同阵营全体");
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
		case EGameXXKCardEffectConditionType::TargetIsAlly: Gate = TEXT("所选目标是友方"); break;
		case EGameXXKCardEffectConditionType::TargetIsEnemy: Gate = TEXT("所选目标是敌方"); break;
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
		case EGameXXKCardEffectSource::HighestAttackAlly: return TEXT("攻击最高友方");
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
		case EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard: return FString::Printf(TEXT("从抽牌堆或弃牌堆检索%d张该任务 NPC 尚未完成的任务牌加入手牌"), Magnitude);
		case EGameXXKCardEffectType::TriggerStatus: return FString::Printf(TEXT("触发%s的%s%d次；每次按当前层数造成生命伤害并减少1层"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot: return FString::Printf(TEXT("按%s当前%s层数，逐层造成%d%%攻击伤害"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::ReplayTriggeredCardBase: return TEXT("重放本次触发牌的基础效果");
		case EGameXXKCardEffectType::ReplaySourceCardBase: return TEXT("重放本牌的基础效果");
		case EGameXXKCardEffectType::ModifyManaCost: return FString::Printf(TEXT("%s内力消耗%+d"), *Target, Magnitude);
		case EGameXXKCardEffectType::WidenNextActiveSingleTarget: return FString::Printf(TEXT("%s的单体效果扩展为目标所在阵营全体"), *Target);
		case EGameXXKCardEffectType::PreserveNextReactionUse: return TEXT("全队下一次反击或格挡不消耗次数");
		case EGameXXKCardEffectType::RetainArmorNextRound: return FString::Printf(TEXT("%s下回合保留当前护甲"), *Target);
		case EGameXXKCardEffectType::CleanseFriendlyDamageOverTime: return FString::Printf(TEXT("若%s为友方，清除其全部流血、中毒和灼烧"), *Target);
		case EGameXXKCardEffectType::HealOrReverseFlat: return FString::Printf(TEXT("若%s为友方，恢复%d点生命；若为敌方，失去%d点生命"), *Target, Magnitude, Magnitude);
		case EGameXXKCardEffectType::ChangeTerrain: return TEXT("切换地势");
		case EGameXXKCardEffectType::DamagePercentAttackPerTargetStatus:
			return FString::Printf(
				TEXT("%s造成（%d%%+每层%s%d个百分点）攻击伤害，不消耗%s"),
				*Target,
				Magnitude,
				*DescribeStatus(Status),
				SecondaryMagnitude,
				*DescribeStatus(Status));
		case EGameXXKCardEffectType::IncreaseMaxMana:
			return FString::Printf(TEXT("%s内力上限+%d，当前内力不变"), *Target, Magnitude);
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
		else if (Effect.Type == EGameXXKCardEffectType::ChangeTerrain)
		{
			Line = FString::Printf(TEXT("切换至%s"), *DescribeTerrain(Effect.TerrainOverride));
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

	const FGameXXKCardEffect* FindSorcererEffect(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		return Definition.Effects.FindByPredicate([Type, Status](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == Type && (Status == EGameXXKCardStatus::None || Effect.Status == Status);
		});
	}

	int32 SorcererMagnitude(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardEffectType Type,
		const int32 Fallback,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		const FGameXXKCardEffect* Effect = FindSorcererEffect(Definition, Type, Status);
		return Effect ? Effect->Magnitude : Fallback;
	}

	bool IsPermanentSorcererCard(const FGameXXKCardDefinition& Definition)
	{
		return Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.OwnerId == FName(TEXT("Profession.Sorcerer"))
			&& Definition.Role == EGameXXKCharacterRole::Sorcerer
			&& Definition.SorcererRule.Family != EGameXXKSorcererCardFamily::None
			&& Definition.SorcererRule.SequenceRule != EGameXXKSorcererSequenceRule::None
			&& Definition.SorcererRule.RewardRule != EGameXXKSorcererRewardRule::None;
	}

	void AppendSorcererRewardText(
		const EGameXXKSorcererRewardRule Reward,
		TArray<FString>& OutLines)
	{
		switch (Reward)
		{
		case EGameXXKSorcererRewardRule::CoreSearch:
			OutLines.Add(TEXT("首牌奖励：回复1点气力、8点内力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::CoreManaEcho:
			OutLines.Add(TEXT("首牌奖励：我方全体回复8点内力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::FireLamp:
			OutLines.Add(TEXT("首牌奖励：敌方全体当前灼烧翻倍。"));
			break;
		case EGameXXKSorcererRewardRule::FireSpread:
			OutLines.Add(TEXT("首牌奖励：按场上最高灼烧补齐敌方全体，再各获得3层灼烧。"));
			break;
		case EGameXXKSorcererRewardRule::FireBurst:
			OutLines.Add(TEXT("首牌奖励：敌方全体结算2次当前灼烧伤害，均不减层。"));
			break;
		case EGameXXKSorcererRewardRule::FireSearch:
			OutLines.Add(TEXT("首牌奖励：敌方全体获得6层灼烧；回复1点气力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::IceCurrentManaRestore:
			OutLines.Add(TEXT("首牌奖励：执行标准寒冰伤害；回复1点气力，抽1张牌。"));
			break;
		case EGameXXKSorcererRewardRule::IceMaxMana:
			OutLines.Add(TEXT("首牌奖励：执行标准寒冰伤害；自身内力上限再+8并补满内力。"));
			break;
		case EGameXXKSorcererRewardRule::IceArmorDouble:
			OutLines.Add(TEXT("首牌奖励：执行标准寒冰伤害；我方全体获得6点护甲。"));
			break;
		case EGameXXKSorcererRewardRule::IceSearch:
			OutLines.Add(TEXT("首牌奖励：执行标准寒冰伤害；敌方全体获得2层虚弱。"));
			break;
		case EGameXXKSorcererRewardRule::LightningMark:
			OutLines.Add(TEXT("首牌奖励：敌方全体获得5层标记；回复1点气力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::LightningSearch:
			OutLines.Add(TEXT("首牌奖励：敌方全体获得3层标记；回复1点气力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::LightningMarkHits:
			OutLines.Add(TEXT("首牌奖励：敌方全体先获得5层标记，再各触发5次70%落雷。"));
			break;
		case EGameXXKSorcererRewardRule::LightningStorm:
			OutLines.Add(TEXT("首牌奖励：敌方全体先获得3层标记，再各触发3次60%落雷。"));
			break;
		case EGameXXKSorcererRewardRule::UniversalScalingAttack:
			OutLines.Add(TEXT("首牌奖励·普通：敌方全体造成300%攻击伤害。"));
			OutLines.Add(TEXT("首牌奖励·炎法：敌方全体获得3层灼烧，再结算1次灼烧且不减层。"));
			OutLines.Add(TEXT("首牌奖励·寒冰：消耗自身全部护甲，对敌方全体造成（120%+每点护甲25个百分点）攻击伤害。"));
			OutLines.Add(TEXT("首牌奖励·雷法：敌方全体获得3层标记，再按各自标记逐层触发60%落雷。"));
			break;
		case EGameXXKSorcererRewardRule::UniversalDraw:
			OutLines.Add(TEXT("首牌奖励·普通：回复2点气力，抽3张牌；我方全体回复6点内力。"));
			OutLines.Add(TEXT("首牌奖励·炎法：敌方全体获得4层灼烧；回复1点气力，抽3张牌。"));
			OutLines.Add(TEXT("首牌奖励·寒冰：执行标准寒冰伤害，返还25%所耗护甲；回复1点气力，抽2张牌。"));
			OutLines.Add(TEXT("首牌奖励·雷法：敌方全体获得2层标记并逐层触发40%落雷；回复1点气力，抽2张牌。"));
			break;
		case EGameXXKSorcererRewardRule::UniversalPartyArmor:
			OutLines.Add(TEXT("首牌奖励·普通：我方全体获得12点护甲；敌方全体获得2层虚弱。"));
			OutLines.Add(TEXT("首牌奖励·炎法：我方全体获得8点护甲；敌方全体获得4层灼烧、1层虚弱。"));
			OutLines.Add(TEXT("首牌奖励·寒冰：执行标准寒冰伤害；我方全体获得6+所耗护甲25%的护甲。"));
			OutLines.Add(TEXT("首牌奖励·雷法：敌方全体获得2层标记并逐层触发30%落雷；我方全体获得6点护甲。"));
			break;
		case EGameXXKSorcererRewardRule::UniversalSearch:
			OutLines.Add(TEXT("首牌奖励·普通：额外重放第5张记录牌，抽1张牌。"));
			OutLines.Add(TEXT("首牌奖励·炎法：额外重放最后一张炎牌，其施加灼烧翻倍；敌方全体再获得2层灼烧。"));
			OutLines.Add(TEXT("首牌奖励·寒冰：额外重放最后一张冰牌，再执行标准寒冰伤害，抽1张牌。"));
			OutLines.Add(TEXT("首牌奖励·雷法：敌方全体获得2层标记，额外重放最后一张雷牌；再逐层触发40%落雷，抽1张牌。"));
			break;
		case EGameXXKSorcererRewardRule::None:
		default:
			break;
		}
	}

	FString DescribeSorcererEffects(const FGameXXKCardDefinition& Definition)
	{
		TArray<FString> Lines;
		const int32 Attack = SorcererMagnitude(Definition, EGameXXKCardEffectType::DamagePercentAttack, 0);
		const int32 Burn = SorcererMagnitude(
			Definition,
			EGameXXKCardEffectType::ApplyStatus,
			0,
			EGameXXKCardStatus::Burn);
		const int32 Mark = SorcererMagnitude(
			Definition,
			EGameXXKCardEffectType::ApplyStatus,
			0,
			EGameXXKCardStatus::Mark);
		const int32 Lightning = SorcererMagnitude(Definition, EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, 0);
		const int32 Armor = SorcererMagnitude(Definition, EGameXXKCardEffectType::AddArmor, 0);
		const int32 Draw = SorcererMagnitude(Definition, EGameXXKCardEffectType::DrawCards, 0);

		switch (Definition.SorcererRule.SequenceRule)
		{
		case EGameXXKSorcererSequenceRule::CoreSearch:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害；检索1张尚未完成的携带法师牌；无合法牌时再结算一次同等伤害。"), Attack));
			Lines.Add(TEXT("编序：第1～2位时，检索牌本回合内力消耗-3。"));
			break;
		case EGameXXKSorcererSequenceRule::CoreManaEcho:
			Lines.Add(FString::Printf(TEXT("基础：自身回复%d点内力。"), SorcererMagnitude(Definition, EGameXXKCardEffectType::GainMana, 3)));
			Lines.Add(TEXT("编序：再回复此前记录牌实际支付内力总和的50%，向下取整。"));
			break;
		case EGameXXKSorcererSequenceRule::FireLamp:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害并获得%d层灼烧。"), Attack, Burn));
			Lines.Add(TEXT("编序：第1～2位时，灼烧改为4层。"));
			break;
		case EGameXXKSorcererSequenceRule::FireSpread:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体获得%d层灼烧。"), Burn));
			Lines.Add(TEXT("编序：前一张记录牌为炎牌时，灼烧改为3层。"));
			break;
		case EGameXXKSorcererSequenceRule::FireBurst:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害。"), Attack));
			Lines.Add(TEXT("编序：第3～5位时，每层灼烧使倍率+10个百分点，不消耗灼烧。"));
			break;
		case EGameXXKSorcererSequenceRule::FireSearch:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害；检索1张尚未完成的携带法师牌；无合法牌时再结算一次同等伤害。"), Attack));
			Lines.Add(TEXT("编序：第4～5位时，每段改为70%攻击伤害。"));
			break;
		case EGameXXKSorcererSequenceRule::IceCurrentManaRestore:
			Lines.Add(TEXT("基础：自身回复当前内力25%的内力，向下取整；溢出内力100%转为护甲。"));
			break;
		case EGameXXKSorcererSequenceRule::IceMaxMana:
			Lines.Add(TEXT("基础：自身内力上限+4并获得4点护甲，当前内力不变。"));
			break;
		case EGameXXKSorcererSequenceRule::IceArmorDouble:
			Lines.Add(TEXT("基础：自身护甲为0时获得4点护甲，否则当前护甲翻倍，最高99。"));
			break;
		case EGameXXKSorcererSequenceRule::IceSearch:
			Lines.Add(TEXT("基础：自身获得当前内力25%的护甲；检索1张尚未完成的携带法师牌；无合法牌时再获得一次等量护甲。"));
			break;
		case EGameXXKSorcererSequenceRule::LightningMark:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害，伤害后获得%d层标记。"), Attack, Mark));
			Lines.Add(TEXT("编序：第1～2位时，标记改为3层。"));
			break;
		case EGameXXKSorcererSequenceRule::LightningSearch:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害；检索1张尚未完成的携带法师牌，随后获得%d层标记；无合法牌时再结算一次同等伤害。"), Attack, Mark));
			Lines.Add(TEXT("编序：第1～2位时，标记改为3层。"));
			break;
		case EGameXXKSorcererSequenceRule::LightningMarkHits:
			Lines.Add(FString::Printf(TEXT("基础：按敌方各自标记快照逐层落雷，每次造成%d%%攻击伤害。"), Lightning));
			Lines.Add(TEXT("编序：第4～5位时，每次改为65%攻击伤害。"));
			break;
		case EGameXXKSorcererSequenceRule::LightningStorm:
			Lines.Add(FString::Printf(TEXT("基础：按敌方各自标记快照逐层落雷，每次造成%d%%攻击伤害。"), Lightning));
			Lines.Add(TEXT("编序：第4～5位时，每次改为45%攻击伤害。"));
			break;
		case EGameXXKSorcererSequenceRule::UniversalScalingAttack:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害。"), Attack));
			Lines.Add(TEXT("编序：此前每记录1张牌，倍率+25个百分点；第1～5位依次为60/85/110/135/160%。"));
			break;
		case EGameXXKSorcererSequenceRule::UniversalDraw:
			Lines.Add(FString::Printf(TEXT("基础：自身抽%d张牌。"), Draw));
			Lines.Add(TEXT("编序：第3～5位时，额外回复5点内力。"));
			break;
		case EGameXXKSorcererSequenceRule::UniversalPartyArmor:
			Lines.Add(FString::Printf(TEXT("基础：我方全体获得%d点护甲。"), Armor));
			Lines.Add(TEXT("编序：前一张记录牌不含直接伤害时，改为6点护甲。"));
			break;
		case EGameXXKSorcererSequenceRule::UniversalSearch:
			Lines.Add(FString::Printf(TEXT("基础：敌方全体造成%d%%攻击伤害；检索1张尚未完成的携带法师牌；无合法牌时再结算一次同等伤害。"), Attack));
			Lines.Add(TEXT("编序：第4～5位时，每段改为90%攻击伤害。"));
			break;
		case EGameXXKSorcererSequenceRule::None:
		default:
			return TEXT("未知法师牌规则");
		}

		if (Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Ice
			|| Definition.SorcererRule.RewardRule == EGameXXKSorcererRewardRule::UniversalDraw
			|| Definition.SorcererRule.RewardRule == EGameXXKSorcererRewardRule::UniversalPartyArmor
			|| Definition.SorcererRule.RewardRule == EGameXXKSorcererRewardRule::UniversalSearch)
		{
			Lines.Add(TEXT("标准寒冰伤害：消耗自身全部护甲，对敌方全体造成（100%+每点护甲20个百分点）攻击伤害。"));
		}
		if (Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Universal)
		{
			Lines.Add(TEXT("任务分支：本牌作为首牌时，由第二张法师牌决定普通、炎法、寒冰或雷法。"));
		}
		AppendSorcererRewardText(Definition.SorcererRule.RewardRule, Lines);
		Lines.Add(TEXT("五牌任务：携带的5张法师牌各主动打出一次后，按首次顺序免费重放基础与锁定编序，最后执行首牌奖励。"));
		return FString::Join(Lines, TEXT("\n"));
	}

	FString DescribeEffectsResolved(const FGameXXKCardDefinition& EffectiveDefinition)
	{
		if (IsPermanentSorcererCard(EffectiveDefinition))
		{
			return DescribeSorcererEffects(EffectiveDefinition);
		}
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
