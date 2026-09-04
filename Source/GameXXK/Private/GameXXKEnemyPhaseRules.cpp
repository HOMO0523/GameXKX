#include "GameXXKEnemyPhaseRules.h"

namespace
{
	bool IsClearedAtEnemyPhaseTransition(const EGameXXKCardStatus Status)
	{
		switch (Status)
		{
		case EGameXXKCardStatus::Vulnerability:
		case EGameXXKCardStatus::Bleed:
		case EGameXXKCardStatus::Poison:
		case EGameXXKCardStatus::Burn:
		case EGameXXKCardStatus::Mark:
		case EGameXXKCardStatus::DamageOverTime:
		case EGameXXKCardStatus::Weak:
		case EGameXXKCardStatus::Prey:
			return true;
		default:
			return false;
		}
	}

	FGameXXKCardCombatUnit* FindUnit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKEnemyBattleState* FindPhaseState(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId)
	{
		return Runtime.EnemyStates.Find(UnitId);
	}

	bool Fail(FString* OutError, const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}
}

int32 FGameXXKEnemyPhaseRules::ClampHealthDamageForRemainingPhase(
	const FGameXXKCardBattleRuntime& Runtime,
	const FGameXXKCardCombatUnit& Target,
	const int32 RequestedHealthDamage)
{
	if (RequestedHealthDamage <= 0
		|| !Target.bLiving
		|| Target.Side != EGameXXKCardTargetSide::Enemy)
	{
		return FMath::Max(0, RequestedHealthDamage);
	}
	const FGameXXKEnemyBattleState* State = FindPhaseState(Runtime, Target.UnitId);
	if (!State || State->CurrentPhase >= State->TotalPhases)
	{
		return FMath::Min(Target.HP, RequestedHealthDamage);
	}
	return FMath::Min(FMath::Max(0, Target.HP - 1), RequestedHealthDamage);
}

bool FGameXXKEnemyPhaseRules::ResolveAfterDamagePacket(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName EnemyUnitId,
	FGameXXKEnemyPhaseTransitionResult& OutResult,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	OutResult = FGameXXKEnemyPhaseTransitionResult();
	if (EnemyUnitId.IsNone())
	{
		return Fail(OutError, TEXT("Enemy phase resolution requires a stable unit ID."));
	}

	FGameXXKCardCombatUnit* Enemy = FindUnit(InOutRuntime.Units, EnemyUnitId);
	FGameXXKEnemyBattleState* State = InOutRuntime.EnemyStates.Find(EnemyUnitId);
	if (!Enemy || Enemy->Side != EGameXXKCardTargetSide::Enemy || !State)
	{
		return Fail(OutError, TEXT("Enemy phase resolution requires a matching enemy unit and saved phase state."));
	}
	if (Enemy->MaxHP <= 0
		|| Enemy->HP < 0
		|| Enemy->HP > Enemy->MaxHP
		|| State->CurrentPhase < 1
		|| State->TotalPhases < 1
		|| State->CurrentPhase > State->TotalPhases)
	{
		return Fail(OutError, TEXT("Enemy phase resolution found invalid health or phase counters."));
	}
	if (State->CurrentPhase >= State->TotalPhases)
	{
		return true;
	}

	const int32 TransitionBoundary = FMath::Max(1, Enemy->MaxHP / 100);
	if (Enemy->HP > TransitionBoundary)
	{
		return true;
	}

	OutResult.bTransitioned = true;
	OutResult.EnemyUnitId = EnemyUnitId;
	OutResult.PreviousPhase = State->CurrentPhase;
	OutResult.NewPhase = State->CurrentPhase + 1;
	OutResult.Healing = Enemy->MaxHP - Enemy->HP;
	for (const FGameXXKCardStatusStack& Stack : Enemy->Statuses)
	{
		if (Stack.Stacks > 0 && IsClearedAtEnemyPhaseTransition(Stack.Status))
		{
			OutResult.ClearedStatuses.AddUnique(Stack.Status);
		}
	}
	Enemy->Statuses.RemoveAll([](const FGameXXKCardStatusStack& Stack)
	{
		return IsClearedAtEnemyPhaseTransition(Stack.Status);
	});

	Enemy->HP = Enemy->MaxHP;
	Enemy->bLiving = true;
	State->CurrentPhase = OutResult.NewPhase;
	State->bPhaseTwo = State->CurrentPhase >= 2;
	++State->PhaseTransitionSerial;
	State->IntentCursor = 0;
	State->PendingChargedIntentId = NAME_None;
	State->ChargeRoundsRemaining = 0;
	State->PendingChargeTargetUnitIds.Reset();
	State->HealingCooldownRounds = 0;
	State->bHealingCooldownStartedThisEnemyPhase = false;
	State->PendingHealingAmplificationPercent = 0;
	State->PhasePassiveTriggerCount = 0;
	State->bFirstHitPassiveAvailable = true;
	State->bFirstStatusPassiveAvailable = true;
	InOutRuntime.LockedEnemyIntents.RemoveAll([EnemyUnitId](const FGameXXKEnemyIntentLock& Lock)
	{
		return Lock.SourceUnitId == EnemyUnitId;
	});
	return true;
}
