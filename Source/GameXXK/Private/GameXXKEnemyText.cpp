#include "GameXXKEnemyText.h"

#include "GameXXKBattlePresentation.h"
#include "GameXXKCardRunTypes.h"
#include "GameXXKCardText.h"
#include "GameXXKMVPRules.h"

namespace
{
	FString SourceName(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
	{
		const FGameXXKBattleRuntimeUnit* Source = State.ActiveBattleEnemies.FindByPredicate([&Intent](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == Intent.SourceUnitId;
		});
		return Source && !Source->DisplayName.IsEmpty() ? Source->DisplayName.ToString() : Intent.SourceUnitId.ToString();
	}

	FString SkillName(const FGameXXKCardEnemyIntent& Intent)
	{
		return Intent.CardDisplayName.IsEmpty() ? TEXT("攻击") : Intent.CardDisplayName;
	}

	FString TargetLabel(const EGameXXKEnemyIntentTargetRule Rule, const FGameXXKCardEnemyIntent& Intent)
	{
		switch (Rule)
		{
		case EGameXXKEnemyIntentTargetRule::Self: return TEXT("自身");
		case EGameXXKEnemyIntentTargetRule::AllLivingParty: return TEXT("我方全体");
		case EGameXXKEnemyIntentTargetRule::AllEnemyAllies: return TEXT("敌方全体");
		case EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly: return TEXT("生命最低的敌方单位");
		default:
			return FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Party, Intent.TargetSlotNumber);
		}
	}

	FString Signed(const int32 Value)
	{
		return Value >= 0 ? FString::Printf(TEXT("+%d"), Value) : FString::FromInt(Value);
	}

	FString FormatEffect(const FGameXXKResolvedEnemyIntentEffect& Effect, const FGameXXKCardEnemyIntent& Intent)
	{
		const FString Target = TargetLabel(Effect.TargetRule, Intent);
		switch (Effect.Type)
		{
		case EGameXXKEnemyIntentEffectType::DirectDamage:
			return FString::Printf(TEXT("%s：伤害 %d%s"), *Target, Effect.Magnitude,
				Effect.HitCount > 1 ? *FString::Printf(TEXT(" × %d"), Effect.HitCount) : TEXT(""));
		case EGameXXKEnemyIntentEffectType::AddArmor:
			return FString::Printf(TEXT("%s：护甲 %s"), *Target, *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::Heal:
			return FString::Printf(TEXT("%s：恢复 %d%% 最大生命"), *Target, Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::ApplyStatus:
			return FString::Printf(TEXT("%s：%s %d层"), *Target, *GameXXKCardText::DescribeStatusName(Effect.Status), Effect.StatusStacks);
		case EGameXXKEnemyIntentEffectType::ConsumeSharedQi:
			return FString::Printf(TEXT("我方共享内力 %s"), *Signed(-FMath::Abs(Effect.Magnitude)));
		case EGameXXKEnemyIntentEffectType::ModifyAttack:
			return FString::Printf(TEXT("%s：攻击 %s（本敌方阶段）"), *Target, *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::ModifyDefense:
			return FString::Printf(TEXT("%s：防御 %s"), *Target, *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::ModifySpeed:
			return FString::Printf(TEXT("%s：速度 %s（下一敌方阶段）"), *Target, *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::RemovePositiveStatus:
			return FString::Printf(TEXT("%s：移除 %d层正面状态"), *Target, Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy:
			return FString::Printf(TEXT("我方下一张可用牌费用 %s"), *Signed(Effect.Magnitude));
		case EGameXXKEnemyIntentEffectType::SetCounter:
			return FString::Printf(TEXT("%s：反击 %d"), *Target, Effect.Magnitude);
		case EGameXXKEnemyIntentEffectType::SetCharge:
			return FString::Printf(TEXT("%s：蓄力 %d回合"), *Target, FMath::Max(1, Effect.Magnitude));
		default:
			return TEXT("未识别效果");
		}
	}

	TArray<FString> EffectLines(const FGameXXKCardEnemyIntent& Intent)
	{
		TArray<FString> Lines;
		for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
		{
			Lines.Add(FormatEffect(Effect, Intent));
		}
		if (Lines.IsEmpty() && Intent.Damage > 0)
		{
			Lines.Add(FString::Printf(TEXT("%s：伤害 %d"), *TargetLabel(Intent.TargetRule, Intent), Intent.Damage));
		}
		for (const FGameXXKCardStatusStack& Status : Intent.OnHitStatuses)
		{
			Lines.Add(FString::Printf(TEXT("命中附加：%s %d层"), *GameXXKCardText::DescribeStatusName(Status.Status), Status.Stacks));
		}
		return Lines;
	}
}

FString FGameXXKEnemyText::FormatIntentCard(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
{
	TArray<FString> Lines = {SourceName(State, Intent), SkillName(Intent)};
	Lines.Append(EffectLines(Intent));
	return FString::Join(Lines, TEXT("\n"));
}

FString FGameXXKEnemyText::FormatIntentTooltip(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
{
	const FString SourceSlot = FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Enemy, Intent.SourceSlotNumber);
	const FString TargetSlot = FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Party, Intent.TargetSlotNumber);
	TArray<FString> Lines = {
		FString::Printf(TEXT("攻击者：%s · %s"), *SourceSlot, *SourceName(State, Intent)),
		FString::Printf(TEXT("技能：%s"), *SkillName(Intent)),
		FString::Printf(TEXT("实际目标：%s"), *TargetSlot)};
	const TArray<FString> Effects = EffectLines(Intent);
	for (const FString& Effect : Effects)
	{
		Lines.Add(FString::Printf(TEXT("效果：%s"), *Effect));
	}
	if (Intent.Damage > 0)
	{
		Lines.Add(FString::Printf(TEXT("基础伤害 %d"), Intent.Damage));
		Lines.Add(TEXT("护甲结算：目标当前护甲先抵扣直接伤害。"));
		Lines.Add(TEXT("生命伤害：护甲抵扣后的剩余伤害写入目标生命。"));
	}
	Lines.Add(TEXT("触发时机：敌方阶段结算。"));
	return FString::Join(Lines, TEXT("\n"));
}
