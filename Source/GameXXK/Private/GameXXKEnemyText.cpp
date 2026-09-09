#include "GameXXKEnemyText.h"

#include "GameXXKBattlePresentation.h"
#include "GameXXKCardRunTypes.h"
#include "GameXXKCardText.h"
#include "GameXXKCombatScalingRules.h"
#include "GameXXKCombatGemRules.h"
#include "GameXXKResistanceRules.h"
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

	FString StatusAmount(const EGameXXKCardStatus Status, const int32 Amount)
	{
		const bool bDot = Status == EGameXXKCardStatus::Bleed || Status == EGameXXKCardStatus::Poison
			|| Status == EGameXXKCardStatus::Burn || Status == EGameXXKCardStatus::DamageOverTime;
		return FString::Printf(TEXT("%s %d%s"), *GameXXKCardText::DescribeStatusName(Status), Amount, bDot ? TEXT("") : TEXT("层"));
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
				? FString::Printf(TEXT("%d%s伤害 × %d"), FinalGeneratedDamage(State, Effect), *FGameXXKCombatGemRules::GetElementLabel(Effect.DamageElement), Effect.HitCount)
				: FString::Printf(TEXT("%d%s伤害"), FinalGeneratedDamage(State, Effect), *FGameXXKCombatGemRules::GetElementLabel(Effect.DamageElement));
			if (Effect.Status != EGameXXKCardStatus::None && Effect.StatusStacks > 0)
			{
				Result += TEXT("；") + StatusAmount(Effect.Status, Effect.StatusStacks);
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
			return StatusAmount(Effect.Status, Effect.StatusStacks);
		case EGameXXKEnemyIntentEffectType::QueueNextRoundEnergyPenalty:
			return FString::Printf(TEXT("下回合气力-%d"), Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::DrainMana:
			return FString::Printf(TEXT("%s吸取%d点内力%s"), Effect.bRequiresPreviousDirectHit ? TEXT("命中后") : TEXT(""),
				Effect.Magnitude, Effect.TargetRule == EGameXXKEnemyIntentTargetRule::HighestManaParty ? TEXT("（内力最高者）") : TEXT(""));
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
				? FString::Printf(TEXT("%d%s伤害 × %d"), FinalGeneratedDamage(State, Effect), *FGameXXKCombatGemRules::GetElementLabel(Effect.DamageElement), Effect.HitCount)
				: FString::Printf(TEXT("%d%s伤害"), FinalGeneratedDamage(State, Effect), *FGameXXKCombatGemRules::GetElementLabel(Effect.DamageElement)))
			: CompactEffect(State, Effect);
		if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
			&& Effect.Status != EGameXXKCardStatus::None
			&& Effect.StatusStacks > 0)
		{
			Payload += TEXT("；命中附加") + StatusAmount(Effect.Status, Effect.StatusStacks);
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
			Payloads.Add(StatusAmount(Status.Status, Status.Stacks));
		}
	}
	const FString Target = TargetLabel(State, Intent.TargetRule, {}, Intent);
	return FString::Printf(
		TEXT("%s\n【%s】\n%s"),
		*SkillName(Intent),
		*Target,
		*FString::Join(Payloads, TEXT("；")));
}

FGameXXKEnemyIntentCardText FGameXXKEnemyText::BuildIntentCardText(
	const FGameXXKRuntimeState& State,
	const FGameXXKCardEnemyIntent& Intent)
{
	FGameXXKEnemyIntentCardText Text;
	Text.Title = SkillName(Intent);
	Text.PrimaryLabel = TEXT("效果");
	Text.Target = TargetLabel(State, Intent.TargetRule, {}, Intent);
	const FGameXXKResolvedEnemyIntentEffect* Primary = Intent.Effects.FindByPredicate([](const auto& Effect)
	{
		return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
	});
	if (!Primary && !Intent.Effects.IsEmpty()) Primary = &Intent.Effects[0];
	TArray<FString> Details;
	auto AddStatus = [&](EGameXXKCardStatus Status, int32 Amount, const FString& Target)
	{
		if (Status == EGameXXKCardStatus::None || Amount <= 0) return;
		if (!Text.Statuses.ContainsByPredicate([&](const auto& Row) { return Row.Status == Status && Row.Amount == Amount && Row.Target == Target; }))
			Text.Statuses.Add({Status, Amount, Target});
	};
	if (Primary)
	{
		Text.Target = TargetLabel(State, Primary->TargetRule, Primary->TargetUnitIds, Intent);
		Text.bDamage = Primary->Type == EGameXXKEnemyIntentEffectType::DirectDamage;
		if (Text.bDamage)
		{
			Text.PrimaryLabel = FGameXXKCombatGemRules::GetElementLabel(Primary->DamageElement) + TEXT("伤害");
			Text.Primary = Primary->HitCount > 1
				? FString::Printf(TEXT("%d × %d"), FinalGeneratedDamage(State, *Primary), Primary->HitCount)
				: FString::FromInt(FinalGeneratedDamage(State, *Primary));
			if (Primary->Status != EGameXXKCardStatus::None && Primary->StatusStacks > 0)
				AddStatus(Primary->Status, Primary->StatusStacks, Text.Target);
		}
		else
		{
			Text.Primary = CompactEffect(State, *Primary);
			switch (Primary->Type)
			{
			case EGameXXKEnemyIntentEffectType::AddArmor:
			case EGameXXKEnemyIntentEffectType::AddArmorDefensePercent:
				Text.Primary = Signed(Primary->Magnitude); Text.PrimaryLabel = TEXT("护甲"); break;
			case EGameXXKEnemyIntentEffectType::Heal:
			case EGameXXKEnemyIntentEffectType::ConsumeWealthForHealing:
				Text.Primary = FString::FromInt(Primary->Magnitude); Text.PrimaryLabel = TEXT("治疗"); break;
			case EGameXXKEnemyIntentEffectType::HealMaxHealthPercent:
				Text.Primary = FString::FromInt(ResolvedMaximumHealthHealing(State, *Primary)); Text.PrimaryLabel = TEXT("治疗"); break;
			case EGameXXKEnemyIntentEffectType::ModifySpeed:
				Text.Primary = Signed(Primary->Magnitude); Text.PrimaryLabel = TEXT("速度"); break;
			case EGameXXKEnemyIntentEffectType::ModifyAttack:
				Text.Primary = Signed(Primary->Magnitude); Text.PrimaryLabel = TEXT("攻击"); break;
			case EGameXXKEnemyIntentEffectType::ApplyStatus:
			{
				Text.PrimaryStatus = Primary->Status; Text.PrimaryStatusAmount = Primary->StatusStacks;
				const bool Dot = Primary->Status == EGameXXKCardStatus::Poison || Primary->Status == EGameXXKCardStatus::Bleed
					|| Primary->Status == EGameXXKCardStatus::Burn || Primary->Status == EGameXXKCardStatus::DamageOverTime;
				Text.Primary = FString::FromInt(Primary->StatusStacks) + (Dot ? TEXT("") : TEXT("层"));
				Text.PrimaryLabel = GameXXKCardText::DescribeStatusName(Primary->Status);
				break;
			}
			case EGameXXKEnemyIntentEffectType::RefreshHealingAmplification:
				Text.Primary = FString::Printf(TEXT("+%d%%"), Primary->Magnitude); Text.PrimaryLabel = TEXT("回复增幅");
				Details.Add(TEXT("下次卷舌\n按最大生命")); break;
			default: break;
			}
		}
	}
	for (const auto& Effect : Intent.Effects)
	{
		if (&Effect == Primary) continue;
		const FString Target = TargetLabel(State, Effect.TargetRule, Effect.TargetUnitIds, Intent);
		if (Effect.Type == EGameXXKEnemyIntentEffectType::ApplyStatus)
		{
			AddStatus(Effect.Status, Effect.StatusStacks, Target);
			continue;
		}
		auto WithoutStatus = Effect;
		if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage)
		{
			AddStatus(Effect.Status, Effect.StatusStacks, Target);
			WithoutStatus.Status = EGameXXKCardStatus::None; WithoutStatus.StatusStacks = 0;
		}
		const FString Payload = CompactEffect(State, WithoutStatus);
		if (Payload.IsEmpty()) continue;
		Details.Add(Target == Text.Target ? Payload : FString::Printf(TEXT("%s·%s"), *Target, *Payload));
	}
	for (const auto& Status : Intent.OnHitStatuses)
	{
		if (Status.Status == EGameXXKCardStatus::None || Status.Stacks <= 0) continue;
		if (!Intent.Effects.ContainsByPredicate([&](const auto& Effect)
			{ return Effect.Status == Status.Status && Effect.StatusStacks == Status.Stacks; }))
			AddStatus(Status.Status, Status.Stacks, Text.Target);
	}
	if (Intent.bCharging) Details.Add(FString::Printf(TEXT("蓄力%d回合"), FMath::Max(0, Intent.ChargeRounds)));
	if (Text.Primary.IsEmpty()) Text.Primary = TEXT("蓄势");
	Text.Details = FString::Join(Details, TEXT("\n"));
	return Text;
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
	const FString Target = TargetLabel(State, Intent.TargetRule, {}, Intent);
	Lines.Add(TEXT("对象：") + Target);
	for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
	{
		FString Detail = DetailEffect(State, Intent, Effect);
		Detail.RemoveFromStart(Target + TEXT("："));
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
			Lines.Add(TEXT("命中附加：") + StatusAmount(Status.Status, Status.Stacks));
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
	if (const auto* Source = State.CardRun.ActiveBattle.Units.FindByPredicate([&Intent](const auto& Unit){return Unit.UnitId==Intent.SourceUnitId;}))
	{
		Lines.Add(FString::Printf(TEXT("自身抗性：火 %.2f%% · 冰 %.2f%% · 雷 %.2f%%"),
			FGameXXKResistanceRules::GetEffectiveBasisPoints(*Source,EGameXXKCardDamageElement::Fire)/100.0,
			FGameXXKResistanceRules::GetEffectiveBasisPoints(*Source,EGameXXKCardDamageElement::Frost)/100.0,
			FGameXXKResistanceRules::GetEffectiveBasisPoints(*Source,EGameXXKCardDamageElement::Lightning)/100.0));
	}
	return FString::Join(Lines, TEXT("\n"));
}
