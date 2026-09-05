#include "GameXXKEnemyText.h"

#include "GameXXKBattlePresentation.h"
#include "GameXXKCardRunTypes.h"
#include "GameXXKCardText.h"
#include "GameXXKCombatScalingRules.h"
#include "GameXXKMVPRules.h"

namespace
{
	FString SkillName(const FGameXXKCardEnemyIntent& Intent)
	{
		return Intent.CardDisplayName.IsEmpty() ? TEXT("攻击") : Intent.CardDisplayName;
	}

	FString SlotLabel(
		const FGameXXKRuntimeState& State,
		const FName UnitId)
	{
		const FGameXXKCardCombatUnit* Unit = State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Unit
			? FGameXXKBattlePresentation::FormatSlotLabel(
				Unit->Side,
				FGameXXKBattlePresentation::GetSlotNumber(State.CardRun.ActiveBattle, UnitId))
			: FString();
	}

	FString TargetLabel(
		const FGameXXKRuntimeState& State,
		const EGameXXKEnemyIntentTargetRule Rule,
		const TArray<FName>& TargetUnitIds,
		const FGameXXKCardEnemyIntent& Intent)
	{
		switch (Rule)
		{
		case EGameXXKEnemyIntentTargetRule::Self: return TEXT("自身");
		case EGameXXKEnemyIntentTargetRule::AllLivingParty: return TEXT("我方全体");
		case EGameXXKEnemyIntentTargetRule::AllEnemyAllies: return TEXT("敌方全体");
		case EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly: return TEXT("生命最低的敌方单位");
		default:
			break;
		}
		const FName TargetId = TargetUnitIds.IsEmpty() ? Intent.SuggestedTargetUnitId : TargetUnitIds[0];
		const FString ResolvedSlot = SlotLabel(State, TargetId);
		return ResolvedSlot.IsEmpty() ? TEXT("待定目标") : ResolvedSlot;
	}

	FString Signed(const int32 Value)
	{
		return Value >= 0 ? FString::Printf(TEXT("+%d"), Value) : FString::FromInt(Value);
	}

	int32 FinalGeneratedDamage(
		const FGameXXKRuntimeState& State,
		const FGameXXKResolvedEnemyIntentEffect& Effect)
	{
		return FGameXXKCombatScalingRules::ScaleByPercentCeil(
			Effect.Magnitude,
			State.CardRun.ActiveBattle.EnemyDifficultyDamagePercent);
	}

	int32 ResolvedMaximumHealthHealing(
		const FGameXXKRuntimeState& State,
		const FGameXXKResolvedEnemyIntentEffect& Effect)
	{
		if (Effect.TargetUnitIds.IsEmpty())
		{
			return 0;
		}
		const FGameXXKCardCombatUnit* Target = State.CardRun.ActiveBattle.Units.FindByPredicate([&Effect](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == Effect.TargetUnitIds[0];
		});
		return Target
			? static_cast<int32>((static_cast<int64>(Target->MaxHP) * Effect.Magnitude + 99) / 100)
			: 0;
	}

	FString CompactEffect(
		const FGameXXKRuntimeState& State,
		const FGameXXKResolvedEnemyIntentEffect& Effect)
	{
		switch (Effect.Type)
		{
		case EGameXXKEnemyIntentEffectType::DirectDamage:
		{
			FString Result = Effect.HitCount > 1
				? FString::Printf(TEXT("%d伤害 × %d"), FinalGeneratedDamage(State, Effect), Effect.HitCount)
				: FString::Printf(TEXT("%d伤害"), FinalGeneratedDamage(State, Effect));
			if (Effect.Status != EGameXXKCardStatus::None && Effect.StatusStacks > 0)
			{
				Result += FString::Printf(
					TEXT("；%s%d"),
					*GameXXKCardText::DescribeStatusName(Effect.Status),
					Effect.StatusStacks);
			}
			return Result;
		}
		case EGameXXKEnemyIntentEffectType::AddArmor:
		case EGameXXKEnemyIntentEffectType::AddArmorDefensePercent:
			return FString::Printf(TEXT("%s护甲"), *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::Heal:
		case EGameXXKEnemyIntentEffectType::ConsumeWealthForHealing:
			return FString::Printf(TEXT("回复%d生命"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::HealMaxHealthPercent:
			return FString::Printf(TEXT("回复%d生命"), ResolvedMaximumHealthHealing(State, Effect));
		case EGameXXKEnemyIntentEffectType::ApplyStatus:
			return Effect.Status == EGameXXKCardStatus::Bleed
				|| Effect.Status == EGameXXKCardStatus::Poison
				|| Effect.Status == EGameXXKCardStatus::Burn
				|| Effect.Status == EGameXXKCardStatus::DamageOverTime
				? FString::Printf(TEXT("%s%d"), *GameXXKCardText::DescribeStatusName(Effect.Status), Effect.StatusStacks)
				: FString::Printf(TEXT("%s %d层"), *GameXXKCardText::DescribeStatusName(Effect.Status), Effect.StatusStacks);
		case EGameXXKEnemyIntentEffectType::QueueNextRoundEnergyPenalty:
			return FString::Printf(TEXT("下回合气力-%d"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy:
			return FString::Printf(TEXT("下一张牌气力+%d"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::ModifyAttack:
			return FString::Printf(TEXT("攻击%s"), *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::ModifySpeed:
			return FString::Printf(TEXT("速度%s"), *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::RemovePositiveStatus:
			return FString::Printf(TEXT("移除%d层增益"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::RemoveNegativeStatus:
			return FString::Printf(TEXT("移除%d层减益"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::TriggerDamageOverTime:
			return FString::Printf(TEXT("触发%s%s"), *GameXXKCardText::DescribeStatusName(Effect.Status), Effect.HitCount > 1 ? *FString::Printf(TEXT(" × %d"), Effect.HitCount) : TEXT(""));
		case EGameXXKEnemyIntentEffectType::RefreshHealingAmplification:
			return FString::Printf(TEXT("下次卷舌回复+%d%%最大生命"), Effect.Magnitude);
		default:
			return FString();
		}
	}

	FString DetailEffect(
		const FGameXXKRuntimeState& State,
		const FGameXXKCardEnemyIntent& Intent,
		const FGameXXKResolvedEnemyIntentEffect& Effect)
	{
		const FString Target = TargetLabel(State, Effect.TargetRule, Effect.TargetUnitIds, Intent);
		FString Payload = Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
			? (Effect.HitCount > 1
				? FString::Printf(TEXT("%d伤害 × %d"), FinalGeneratedDamage(State, Effect), Effect.HitCount)
				: FString::Printf(TEXT("%d伤害"), FinalGeneratedDamage(State, Effect)))
			: CompactEffect(State, Effect);
		if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
			&& Effect.Status != EGameXXKCardStatus::None
			&& Effect.StatusStacks > 0)
		{
			Payload += FString::Printf(
				TEXT("；命中附加%s%d"),
				*GameXXKCardText::DescribeStatusName(Effect.Status),
				Effect.StatusStacks);
		}
		return FString::Printf(TEXT("%s：%s"), *Target, *Payload);
	}

	FString ConditionalNote(const FName IntentId)
	{
		if (IntentId == TEXT("Pursuit")) return TEXT("目标有标记时使用强化伤害。");
		if (IntentId == TEXT("ToxicPursuit") || IntentId == TEXT("Tongue")) return TEXT("目标中毒时使用强化伤害。");
		if (IntentId == TEXT("BloodPursuit")
			|| IntentId == TEXT("MountainShakingSweep")
			|| IntentId == TEXT("BloodBattleNeverRetreats")
			|| IntentId == TEXT("GroundedPounce")
			|| IntentId == TEXT("FatalAmbush")) return TEXT("目标流血时获得强化。");
		if (IntentId == TEXT("BloodClawRend")
			|| IntentId == TEXT("BloodBattleThroatRend")
			|| IntentId == TEXT("CorneredBeastPounce")
			|| IntentId == TEXT("DeathPounce")) return TEXT("目标虚弱时追加伤害。");
		return FString();
	}
}

FString FGameXXKEnemyText::FormatIntentCard(
	const FGameXXKRuntimeState& State,
	const FGameXXKCardEnemyIntent& Intent)
{
	TArray<FString> Payloads;
	for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
	{
		const FString Payload = CompactEffect(State, Effect);
		if (!Payload.IsEmpty())
		{
			Payloads.Add(Payload);
		}
	}
	for (const FGameXXKCardStatusStack& Status : Intent.OnHitStatuses)
	{
		const bool bAlreadyShown = Intent.Effects.ContainsByPredicate([&Status](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Status == Status.Status && Effect.StatusStacks == Status.Stacks;
		});
		if (!bAlreadyShown && Status.Status != EGameXXKCardStatus::None && Status.Stacks > 0)
		{
			Payloads.Add(FString::Printf(
				TEXT("%s %d层"),
				*GameXXKCardText::DescribeStatusName(Status.Status),
				Status.Stacks));
		}
	}
	const FString Target = TargetLabel(State, Intent.TargetRule, {}, Intent);
	return FString::Printf(
		TEXT("%s\n【%s】\n%s"),
		*SkillName(Intent),
		*Target,
		*FString::Join(Payloads, TEXT("；")));
}

FString FGameXXKEnemyText::FormatIntentTooltip(
	const FGameXXKRuntimeState& State,
	const FGameXXKCardEnemyIntent& Intent)
{
	TArray<FString> Lines;
	Lines.Add(SkillName(Intent));
	if (Intent.TotalPhases > 1)
	{
		Lines.Add(FString::Printf(
			TEXT("阶段：%d/%d%s"),
			Intent.PhaseNumber,
			Intent.TotalPhases,
			Intent.PhaseLabel.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" · %s"), *Intent.PhaseLabel)));
	}
	Lines.Add(FString::Printf(
		TEXT("对象：%s"),
		*TargetLabel(State, Intent.TargetRule, {}, Intent)));
	for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
	{
		const FString Detail = DetailEffect(State, Intent, Effect);
		if (!Detail.EndsWith(TEXT("：")))
		{
			Lines.Add(Detail);
		}
	}
	for (const FGameXXKCardStatusStack& Status : Intent.OnHitStatuses)
	{
		const bool bAlreadyShown = Intent.Effects.ContainsByPredicate([&Status](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Status == Status.Status && Effect.StatusStacks == Status.Stacks;
		});
		if (!bAlreadyShown && Status.Status != EGameXXKCardStatus::None && Status.Stacks > 0)
		{
			Lines.Add(FString::Printf(
				TEXT("命中附加：%s %d层"),
				*GameXXKCardText::DescribeStatusName(Status.Status),
				Status.Stacks));
		}
	}
	if (Intent.bCharging)
	{
		Lines.Add(FString::Printf(TEXT("蓄力：剩余%d回合"), FMath::Max(0, Intent.ChargeRounds)));
	}
	const FString Condition = ConditionalNote(Intent.IntentDefinitionId);
	if (!Condition.IsEmpty())
	{
		Lines.Add(Condition);
	}
	return FString::Join(Lines, TEXT("\n"));
}
