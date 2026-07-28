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
		case EGameXXKCardStatus::Momentum: return TEXT("势");
		case EGameXXKCardStatus::Agility: return TEXT("灵动");
		case EGameXXKCardStatus::Vulnerability: return TEXT("破绽");
		case EGameXXKCardStatus::Bleed: return TEXT("流血");
		case EGameXXKCardStatus::Poison: return TEXT("中毒");
		case EGameXXKCardStatus::Burn: return TEXT("灼烧");
		case EGameXXKCardStatus::Mark: return TEXT("标记");
		case EGameXXKCardStatus::Guard: return TEXT("守护");
		case EGameXXKCardStatus::DamageOverTime: return TEXT("持续伤害");
		case EGameXXKCardStatus::CannotReceiveVulnerability: return TEXT("免疫破绽");
		case EGameXXKCardStatus::NextAttackBonus: return TEXT("下次攻击强化");
		case EGameXXKCardStatus::NextAttackAppliesVulnerability: return TEXT("下次攻击附加破绽");
		case EGameXXKCardStatus::NextHealingBonus: return TEXT("下次治疗强化");
		case EGameXXKCardStatus::TerrainBonusDouble: return TEXT("地形加成翻倍");
		case EGameXXKCardStatus::NextTerrainCardFree: return TEXT("下一张地形牌免费");
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction: return TEXT("下一张地形牌减气");
		case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack: return TEXT("转移单体敌袭");
		case EGameXXKCardStatus::TerrainBonusDoubleThisRound: return TEXT("本回合地形加成翻倍");
		default: return TEXT("无效状态");
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
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime: Gate = TEXT("所选目标带有持续伤害"); break;
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
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime: Gate = TEXT("出牌者带有持续伤害"); break;
		default: Gate = TEXT("无效条件"); break;
		}
		if (!Gate.IsEmpty())
		{
			Clauses.Add(Condition.bNegate ? FString::Printf(TEXT("当%s不成立"), *Gate) : FString::Printf(TEXT("当%s"), *Gate));
		}
		if (Condition.bConsumeStatus)
		{
			Clauses.Add(FString::Printf(TEXT("消耗%s至多%d层"), *DescribeStatus(Condition.Status), Condition.MaxConsumedStatusStacks));
		}
		if (Condition.bScaleMagnitudeByConsumedStacks)
		{
			Clauses.Add(TEXT("数值按消耗层数结算"));
		}
		if (Condition.bConsumeOwnerArmor)
		{
			Clauses.Add(FString::Printf(TEXT("消耗出牌者护甲至多%d点"), Condition.MaxConsumedArmor));
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
		case EGameXXKCardBattleModifierTrigger::OnSingleTargetEnemyAttack: return TEXT("遭受单体敌方攻击时");
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

	FString DescribeEffectType(EGameXXKCardEffectType Type, int32 Magnitude, int32 HitCount, EGameXXKCardStatus Status, const FString& Target)
	{
		const FString HitSuffix = HitCount > 1 ? FString::Printf(TEXT("，共%d段"), HitCount) : FString();
		switch (Type)
		{
		case EGameXXKCardEffectType::DamagePercentAttack: return FString::Printf(TEXT("%s造成%d%%攻击伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::DamageFlat: return FString::Printf(TEXT("%s受到%d点直接伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::LoseHealth: return FString::Printf(TEXT("%s失去%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::Heal: return FString::Printf(TEXT("%s恢复%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::AddArmor: return FString::Printf(TEXT("%s获得%d点护甲"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainMana: return FString::Printf(TEXT("%s获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainEnergy: return FString::Printf(TEXT("%s获得%d点气"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainManaPerConsumedStatus: return FString::Printf(TEXT("%s按消耗状态获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::DrawCards: return FString::Printf(TEXT("%s抽%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ApplyStatus: return FString::Printf(TEXT("%s获得%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveStatus: return FString::Printf(TEXT("移除%s%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveAnyDamageOverTime: return FString::Printf(TEXT("清除%s%d层持续伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::Insight: return FString::Printf(TEXT("%s洞察牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscoverCards: return FString::Printf(TEXT("%s发现%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ReorderCards: return FString::Printf(TEXT("%s重排牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscardCards: return FString::Printf(TEXT("%s弃置%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::IgnoreDefense: return FString::Printf(TEXT("%s本次伤害无视%d点防御"), *Target, Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercent: return FString::Printf(TEXT("%s额外造成%d%%攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus: return FString::Printf(TEXT("%s每层消耗状态额外造成%d%%攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor: return FString::Printf(TEXT("%s每点消耗护甲额外造成%d%%攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget: return FString::Printf(TEXT("每名存活友方对%s造成%d%%攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::ApplyGuardLink: return FString::Printf(TEXT("%s建立援护关系"), *Target);
		case EGameXXKCardEffectType::ApplyBattleModifier: return FString::Printf(TEXT("%s获得持续战斗效果"), *Target);
		case EGameXXKCardEffectType::ModifyHealingPercent: return FString::Printf(TEXT("%s治疗效果调整%d%%"), *Target, Magnitude);
		case EGameXXKCardEffectType::ModifyEnergyCost: return FString::Printf(TEXT("%s费用调整%d点气"), *Target, Magnitude);
		case EGameXXKCardEffectType::RevealEnemyIntent: return FString::Printf(TEXT("%s揭示%d个敌方意图"), *Target, Magnitude);
		case EGameXXKCardEffectType::DoubleTerrainBonus: return FString::Printf(TEXT("%s的地形加成翻倍%d次"), *Target, Magnitude);
		case EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks: return FString::Printf(TEXT("%s转移%d次单体敌方攻击"), *Target, Magnitude);
		default: return TEXT("无效效果");
		}
	}

	FString DescribeModifier(const FGameXXKCardBattleModifier& Modifier)
	{
		if (!Modifier.bPersistent)
		{
			return FString();
		}
		const FString Core = DescribeEffectType(Modifier.EffectType, Modifier.Magnitude, 1, Modifier.Status, DescribeEffectTarget(Modifier.Target));
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

	FString DescribeEffectsResolved(const FGameXXKCardDefinition& EffectiveDefinition)
	{
		TArray<FString> Lines;
		for (const FGameXXKCardEffect& Effect : EffectiveDefinition.Effects)
		{
			FString Line = DescribeEffectType(Effect.Type, Effect.Magnitude, Effect.HitCount, Effect.Status, DescribeEffectTarget(Effect.Target));
			const FString Condition = DescribeCondition(Effect.Condition);
			if (!Condition.IsEmpty())
			{
				Line += FString::Printf(TEXT("（%s）"), *Condition);
			}
			if (!Effect.ConsumptionGroupId.IsNone())
			{
				Line += TEXT("（记录本次消耗）");
			}
			if (!Effect.ConsumedStackResultRef.IsNone())
			{
				Line += TEXT("（按本次消耗层数结算）");
			}
			if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
			{
				const FString ModifierText = DescribeModifier(Effect.Modifier);
				if (!ModifierText.IsEmpty())
				{
					Line = ModifierText;
				}
			}
			if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
			{
				Line = DescribeGuardLink(Effect.GuardLink);
			}
			Lines.Add(Line);
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
