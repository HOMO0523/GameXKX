#include "UI/GameXXKTrainingTravelVisualRuntime.h"

void FGameXXKTrainingTravelVisualRuntime::Reset()
{
	LatestRuntime = FGameXXKTrainingTravelRuntime();
	PendingCombatEvents.Reset();
	ActiveCombatEvent = FCombatEvent();
	VisualPhase = EGameXXKTrainingTravelVisualPhase::Paused;
	VisualPhaseElapsedSeconds = 0.0f;
	ScrollOffset = 0.0f;
	CurrentScrollSpeed = 0.0f;
	WalkFrameAccumulator = 0.0f;
	WalkFrameIndex = 0;
	CompletedLoopCount = 0;
	bHasAuthoritativeSnapshot = false;
	bHasActiveCombatEvent = false;
}

void FGameXXKTrainingTravelVisualRuntime::Synchronize(const FGameXXKTrainingTravelRuntime& Runtime)
{
	LatestRuntime = Runtime;
	bHasAuthoritativeSnapshot = true;
	if (!bHasActiveCombatEvent && PendingCombatEvents.IsEmpty())
	{
		ApplyLatestAuthoritativePhase();
	}
}

void FGameXXKTrainingTravelVisualRuntime::Tick(const float DeltaSeconds)
	{
	float RemainingSeconds = FMath::Max(0.0f, DeltaSeconds);
	while (RemainingSeconds > UE_SMALL_NUMBER)
	{
		const float PhaseDuration = GetCurrentPhaseDuration();
		if (PhaseDuration <= 0.0f)
		{
			AdvanceMotion(RemainingSeconds);
			VisualPhaseElapsedSeconds += RemainingSeconds;
			break;
		}

		const float PhaseRemaining = FMath::Max(0.0f, PhaseDuration - VisualPhaseElapsedSeconds);
		const float ConsumedSeconds = FMath::Min(RemainingSeconds, PhaseRemaining);
		AdvanceMotion(ConsumedSeconds);
		VisualPhaseElapsedSeconds += ConsumedSeconds;
		RemainingSeconds -= ConsumedSeconds;
		if (VisualPhaseElapsedSeconds + UE_SMALL_NUMBER >= PhaseDuration)
		{
			CompleteTimedPhase();
			continue;
		}
		break;
	}
}

void FGameXXKTrainingTravelVisualRuntime::Tick(
	const float DeltaSeconds,
	const EGameXXKTrainingTravelPhase Phase)
	{
	FGameXXKTrainingTravelRuntime Snapshot = LatestRuntime;
	Snapshot.Phase = Phase;
	Synchronize(Snapshot);
	Tick(DeltaSeconds);
}

void FGameXXKTrainingTravelVisualRuntime::NotifyTravelStep(
	const FGameXXKTrainingTravelRuntime& Before,
	const FGameXXKTrainingTravelRuntime& After,
	const bool bEncounterCompleted,
	const bool bStageCompleted,
	const bool bDefeated)
	{
	LatestRuntime = After;
	bHasAuthoritativeSnapshot = true;
	if (bStageCompleted)
	{
		++CompletedLoopCount;
	}

	const bool bCombatMutation = Before.Phase == EGameXXKTrainingTravelPhase::Combat
		&& (bEncounterCompleted
			|| bDefeated
			|| After.LastAttackingPartyIndex != INDEX_NONE
			|| After.LastDamagedPartyIndex != INDEX_NONE
			|| Before.ActiveEnemyIndex != After.ActiveEnemyIndex
			|| Before.EnemyHP != After.EnemyHP
			|| Before.PlayerHP != After.PlayerHP);
	if (bCombatMutation)
	{
		EnqueueCombatEvent(Before, After, bEncounterCompleted, bDefeated);
		return;
	}

	if (!bHasActiveCombatEvent && PendingCombatEvents.IsEmpty())
	{
		ApplyLatestAuthoritativePhase();
	}
}

void FGameXXKTrainingTravelVisualRuntime::NotifyTravelStep(
	const bool bEncounterCompleted,
	const bool bStageCompleted)
{
	if (bStageCompleted)
	{
		++CompletedLoopCount;
	}
	if (bEncounterCompleted && !bHasActiveCombatEvent && PendingCombatEvents.IsEmpty())
	{
		ApplyLatestAuthoritativePhase();
	}
}

EGameXXKBattleAnimationAction FGameXXKTrainingTravelVisualRuntime::GetHeroAction() const
{
	return GetPartyAction(0);
}

EGameXXKBattleAnimationAction FGameXXKTrainingTravelVisualRuntime::GetPartyAction(const int32 PartyIndex) const
{
	const int32 EffectiveAttacker = ActiveCombatEvent.AttackingPartyIndex == INDEX_NONE
		? 0
		: ActiveCombatEvent.AttackingPartyIndex;
	const int32 EffectiveDamaged = ActiveCombatEvent.DamagedPartyIndex == INDEX_NONE
		? 0
		: ActiveCombatEvent.DamagedPartyIndex;
	switch (VisualPhase)
	{
	case EGameXXKTrainingTravelVisualPhase::HeroAttack:
		return PartyIndex == EffectiveAttacker
			? EGameXXKBattleAnimationAction::Attack
			: EGameXXKBattleAnimationAction::Idle;
	case EGameXXKTrainingTravelVisualPhase::HeroHit:
		return PartyIndex == EffectiveDamaged
			? EGameXXKBattleAnimationAction::Hit
			: EGameXXKBattleAnimationAction::Idle;
	case EGameXXKTrainingTravelVisualPhase::HeroDeath:
		return PartyIndex == EffectiveDamaged
			? EGameXXKBattleAnimationAction::Death
			: EGameXXKBattleAnimationAction::Idle;
	default:
		return EGameXXKBattleAnimationAction::Idle;
	}
}

EGameXXKBattleAnimationAction FGameXXKTrainingTravelVisualRuntime::GetEnemyAction() const
{
	switch (VisualPhase)
	{
	case EGameXXKTrainingTravelVisualPhase::EnemyAttack:
		return EGameXXKBattleAnimationAction::Attack;
	case EGameXXKTrainingTravelVisualPhase::EnemyHit:
		return EGameXXKBattleAnimationAction::Hit;
	case EGameXXKTrainingTravelVisualPhase::EnemyDeath:
		return EGameXXKBattleAnimationAction::Death;
	default:
		return EGameXXKBattleAnimationAction::Idle;
	}
}

FName FGameXXKTrainingTravelVisualRuntime::GetEnemyDefinitionId() const
{
	if (bHasActiveCombatEvent)
	{
		return ActiveCombatEvent.EnemyDefinitionId;
	}
	if (VisualPhase == EGameXXKTrainingTravelVisualPhase::EncounterIdle
		|| (VisualPhase == EGameXXKTrainingTravelVisualPhase::Paused
			&& LatestRuntime.Phase == EGameXXKTrainingTravelPhase::Defeated))
	{
		return LatestRuntime.EnemyDefinitionId;
	}
	return NAME_None;
}

int32 FGameXXKTrainingTravelVisualRuntime::GetEnemyFormationSlotCount() const
{
	if (bHasActiveCombatEvent && !ActiveCombatEvent.EnemiesBefore.IsEmpty())
	{
		return ActiveCombatEvent.EnemiesBefore.Num();
	}
	if (!LatestRuntime.Enemies.IsEmpty())
	{
		return LatestRuntime.Enemies.Num();
	}
	return LatestRuntime.EnemyDefinitionId.IsNone() ? 0 : 1;
}

int32 FGameXXKTrainingTravelVisualRuntime::GetPresentedEnemySlotIndex() const
{
	if (bHasActiveCombatEvent)
	{
		return ActiveCombatEvent.EnemySlotIndex;
	}
	return LatestRuntime.ActiveEnemyIndex == INDEX_NONE && !LatestRuntime.EnemyDefinitionId.IsNone()
		? 0
		: LatestRuntime.ActiveEnemyIndex;
}

FName FGameXXKTrainingTravelVisualRuntime::GetEnemyDefinitionIdForSlot(const int32 SlotIndex) const
{
	if (bHasActiveCombatEvent)
	{
		// A combat event owns one immutable presentation boundary. Never fall
		// through to LatestRuntime here: it may already contain the next authored
		// formation while the previous target is still playing its death clip.
		if (!ActiveCombatEvent.EnemiesBefore.IsEmpty())
		{
			return ActiveCombatEvent.EnemiesBefore.IsValidIndex(SlotIndex)
				? ActiveCombatEvent.EnemiesBefore[SlotIndex].EnemyDefinitionId
				: NAME_None;
		}
		return SlotIndex == 0 ? ActiveCombatEvent.EnemyDefinitionId : NAME_None;
	}
	if (LatestRuntime.Enemies.IsValidIndex(SlotIndex))
	{
		return LatestRuntime.Enemies[SlotIndex].EnemyDefinitionId;
	}
	return SlotIndex == 0 ? GetEnemyDefinitionId() : NAME_None;
}

bool FGameXXKTrainingTravelVisualRuntime::IsEnemySlotVisible(const int32 SlotIndex) const
{
	if (VisualPhase == EGameXXKTrainingTravelVisualPhase::Walking
		|| GetEnemyDefinitionIdForSlot(SlotIndex).IsNone())
	{
		return false;
	}
	if (bHasActiveCombatEvent && SlotIndex == ActiveCombatEvent.EnemySlotIndex)
	{
		// Retain the current target until its hit/death presentation completes.
		return true;
	}
	if (bHasActiveCombatEvent && ActiveCombatEvent.EnemiesAfter.IsValidIndex(SlotIndex))
	{
		return ActiveCombatEvent.EnemiesAfter[SlotIndex].HP > 0;
	}
	if (LatestRuntime.Enemies.IsValidIndex(SlotIndex))
	{
		return LatestRuntime.Enemies[SlotIndex].HP > 0;
	}
	return SlotIndex == 0 && IsEnemyVisible();
}

float FGameXXKTrainingTravelVisualRuntime::GetEnemyHealthFractionForSlot(const int32 SlotIndex) const
{
	if (SlotIndex == GetPresentedEnemySlotIndex())
	{
		return GetEnemyHealthFraction();
	}
	if (bHasActiveCombatEvent)
	{
		if (!ActiveCombatEvent.EnemiesAfter.IsValidIndex(SlotIndex))
		{
			return 0.0f;
		}
		const FGameXXKTrainingTravelEnemyRuntime& Enemy = ActiveCombatEvent.EnemiesAfter[SlotIndex];
		return Enemy.MaxHP > 0
			? FMath::Clamp(static_cast<float>(Enemy.HP) / static_cast<float>(Enemy.MaxHP), 0.0f, 1.0f)
			: 0.0f;
	}
	if (!LatestRuntime.Enemies.IsValidIndex(SlotIndex))
	{
		return 0.0f;
	}
	const FGameXXKTrainingTravelEnemyRuntime& Enemy = LatestRuntime.Enemies[SlotIndex];
	return Enemy.MaxHP > 0
		? FMath::Clamp(static_cast<float>(Enemy.HP) / static_cast<float>(Enemy.MaxHP), 0.0f, 1.0f)
		: 0.0f;
}

bool FGameXXKTrainingTravelVisualRuntime::IsEnemyVisible() const
{
	return !GetEnemyDefinitionId().IsNone()
		&& VisualPhase != EGameXXKTrainingTravelVisualPhase::Walking;
}

float FGameXXKTrainingTravelVisualRuntime::GetEnemyHealthFraction() const
{
	if (!bHasActiveCombatEvent)
	{
		return LatestRuntime.EnemyMaxHP > 0
			? FMath::Clamp(static_cast<float>(LatestRuntime.EnemyHP) / static_cast<float>(LatestRuntime.EnemyMaxHP), 0.0f, 1.0f)
			: 0.0f;
	}

	float Health = static_cast<float>(ActiveCombatEvent.EnemyHealthBefore);
	if (VisualPhase == EGameXXKTrainingTravelVisualPhase::EnemyHit)
	{
		Health = FMath::Lerp(
			static_cast<float>(ActiveCombatEvent.EnemyHealthBefore),
			static_cast<float>(ActiveCombatEvent.EnemyHealthAfter),
			GetPhaseProgress(EnemyHitSeconds));
	}
	else if (VisualPhase == EGameXXKTrainingTravelVisualPhase::EnemyAttack
		|| VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroHit
		|| VisualPhase == EGameXXKTrainingTravelVisualPhase::EnemyDeath
		|| VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroDeath)
	{
		Health = static_cast<float>(ActiveCombatEvent.EnemyHealthAfter);
	}
	return FMath::Clamp(Health / static_cast<float>(FMath::Max(1, ActiveCombatEvent.EnemyMaxHealth)), 0.0f, 1.0f);
}

float FGameXXKTrainingTravelVisualRuntime::GetHeroHealthFraction() const
{
	return GetPartyHealthFraction(0);
}

float FGameXXKTrainingTravelVisualRuntime::GetPartyHealthFraction(const int32 PartyIndex) const
{
	if (!bHasActiveCombatEvent)
	{
		if (LatestRuntime.PartyUnits.IsValidIndex(PartyIndex))
		{
			const FGameXXKTrainingTravelPartyUnitRuntime& PartyUnit = LatestRuntime.PartyUnits[PartyIndex];
			return PartyUnit.MaxHP > 0
				? FMath::Clamp(static_cast<float>(PartyUnit.HP) / static_cast<float>(PartyUnit.MaxHP), 0.0f, 1.0f)
				: 0.0f;
		}
		return PartyIndex == 0 && LatestRuntime.PlayerMaxHP > 0
			? FMath::Clamp(static_cast<float>(LatestRuntime.PlayerHP) / static_cast<float>(LatestRuntime.PlayerMaxHP), 0.0f, 1.0f)
			: 0.0f;
	}

	if (!ActiveCombatEvent.PartyBefore.IsValidIndex(PartyIndex))
	{
		if (PartyIndex != 0)
		{
			return 0.0f;
		}
		float LegacyHeroHealth = static_cast<float>(ActiveCombatEvent.HeroHealthBefore);
		if (VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroHit)
		{
			LegacyHeroHealth = FMath::Lerp(
				static_cast<float>(ActiveCombatEvent.HeroHealthBefore),
				static_cast<float>(ActiveCombatEvent.HeroHealthAfter),
				GetPhaseProgress(HeroHitSeconds));
		}
		else if (VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroDeath)
		{
			LegacyHeroHealth = static_cast<float>(ActiveCombatEvent.HeroHealthAfter);
		}
		return FMath::Clamp(
			LegacyHeroHealth / static_cast<float>(FMath::Max(1, ActiveCombatEvent.HeroMaxHealth)),
			0.0f,
			1.0f);
	}

	const FGameXXKTrainingTravelPartyUnitRuntime& BeforeUnit = ActiveCombatEvent.PartyBefore[PartyIndex];
	const FGameXXKTrainingTravelPartyUnitRuntime* AfterUnit =
		ActiveCombatEvent.PartyAfter.IsValidIndex(PartyIndex)
			? &ActiveCombatEvent.PartyAfter[PartyIndex]
			: &BeforeUnit;
	float Health = static_cast<float>(BeforeUnit.HP);
	if (PartyIndex == ActiveCombatEvent.DamagedPartyIndex
		&& VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroHit)
	{
		Health = FMath::Lerp(
			static_cast<float>(BeforeUnit.HP),
			static_cast<float>(AfterUnit->HP),
			GetPhaseProgress(HeroHitSeconds));
	}
	else if (PartyIndex == ActiveCombatEvent.DamagedPartyIndex
		&& VisualPhase == EGameXXKTrainingTravelVisualPhase::HeroDeath)
	{
		Health = static_cast<float>(AfterUnit->HP);
	}
	return FMath::Clamp(Health / static_cast<float>(FMath::Max(1, BeforeUnit.MaxHP)), 0.0f, 1.0f);
}

void FGameXXKTrainingTravelVisualRuntime::ApplyLatestAuthoritativePhase()
{
	if (!bHasAuthoritativeSnapshot)
	{
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::Paused);
		return;
	}

	switch (LatestRuntime.Phase)
	{
	case EGameXXKTrainingTravelPhase::Walking:
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::Walking);
		break;
	case EGameXXKTrainingTravelPhase::Combat:
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::EncounterIdle);
		break;
	case EGameXXKTrainingTravelPhase::Defeated:
	case EGameXXKTrainingTravelPhase::Idle:
	default:
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::Paused);
		break;
	}
}

void FGameXXKTrainingTravelVisualRuntime::EnqueueCombatEvent(
	const FGameXXKTrainingTravelRuntime& Before,
	const FGameXXKTrainingTravelRuntime& After,
	const bool bEncounterCompleted,
	const bool bDefeated)
{
	FCombatEvent Event;
	Event.EnemyDefinitionId = Before.EnemyDefinitionId;
	Event.EnemySlotIndex = Before.ActiveEnemyIndex == INDEX_NONE ? 0 : Before.ActiveEnemyIndex;
	Event.EnemiesBefore = Before.Enemies;
	Event.EnemiesAfter = Before.Enemies;
	if (!bEncounterCompleted
		&& Before.StageId == After.StageId
		&& Before.EncounterIndex == After.EncounterIndex)
	{
		Event.EnemiesAfter = After.Enemies;
	}
	else if (Event.EnemiesAfter.IsValidIndex(Event.EnemySlotIndex))
	{
		Event.EnemiesAfter[Event.EnemySlotIndex].HP = 0;
	}
	const FGameXXKTrainingTravelEnemyRuntime* BeforeEnemy = Before.Enemies.IsValidIndex(Event.EnemySlotIndex)
		? &Before.Enemies[Event.EnemySlotIndex]
		: nullptr;
	const FGameXXKTrainingTravelEnemyRuntime* AfterEnemy = After.Enemies.IsValidIndex(Event.EnemySlotIndex)
		? &After.Enemies[Event.EnemySlotIndex]
		: nullptr;
	Event.EnemyHealthBefore = FMath::Max(0, BeforeEnemy ? BeforeEnemy->HP : Before.EnemyHP);
	Event.EnemyMaxHealth = FMath::Max(1, BeforeEnemy ? BeforeEnemy->MaxHP : Before.EnemyMaxHP);
	Event.EnemyHealthAfter = bEncounterCompleted
		? 0
		: FMath::Clamp(AfterEnemy ? AfterEnemy->HP : After.EnemyHP, 0, Event.EnemyMaxHealth);
	Event.HeroHealthBefore = FMath::Max(0, Before.PlayerHP);
	Event.HeroHealthAfter = FMath::Clamp(After.PlayerHP, 0, FMath::Max(1, Before.PlayerMaxHP));
	Event.HeroMaxHealth = FMath::Max(1, Before.PlayerMaxHP);
	Event.PartyBefore = Before.PartyUnits;
	Event.PartyAfter = After.PartyUnits;
	Event.AttackingPartyIndex = After.LastAttackingPartyIndex;
	Event.DamagedPartyIndex = After.LastDamagedPartyIndex;
	Event.bEnemyDefeated = bEncounterCompleted || Event.EnemyHealthAfter <= 0;
	const bool bDamagedPartyUnitDefeated = Event.DamagedPartyIndex != INDEX_NONE
		&& Event.PartyAfter.IsValidIndex(Event.DamagedPartyIndex)
		&& Event.PartyAfter[Event.DamagedPartyIndex].HP <= 0;
	Event.bHeroDefeated = bDefeated || bDamagedPartyUnitDefeated
		|| (Event.PartyAfter.IsEmpty() && Event.HeroHealthAfter <= 0);
	PendingCombatEvents.Add(MoveTemp(Event));
	constexpr int32 MaximumQueuedCombatEvents = 8;
	if (PendingCombatEvents.Num() > MaximumQueuedCombatEvents)
	{
		PendingCombatEvents.RemoveAt(0, PendingCombatEvents.Num() - MaximumQueuedCombatEvents, EAllowShrinking::No);
	}
	if (!bHasActiveCombatEvent)
	{
		StartNextCombatEvent();
	}
}

void FGameXXKTrainingTravelVisualRuntime::StartNextCombatEvent()
{
	if (PendingCombatEvents.IsEmpty())
	{
		ApplyLatestAuthoritativePhase();
		return;
	}
	ActiveCombatEvent = MoveTemp(PendingCombatEvents[0]);
	PendingCombatEvents.RemoveAt(0, 1, EAllowShrinking::No);
	bHasActiveCombatEvent = true;
	SetVisualPhase(EGameXXKTrainingTravelVisualPhase::HeroAttack);
}

void FGameXXKTrainingTravelVisualRuntime::CompleteActiveCombatEvent()
{
	bHasActiveCombatEvent = false;
	ActiveCombatEvent = FCombatEvent();
	if (!PendingCombatEvents.IsEmpty())
	{
		StartNextCombatEvent();
		return;
	}
	ApplyLatestAuthoritativePhase();
}

void FGameXXKTrainingTravelVisualRuntime::SetVisualPhase(const EGameXXKTrainingTravelVisualPhase Phase)
{
	if (VisualPhase == Phase)
	{
		return;
	}
	VisualPhase = Phase;
	VisualPhaseElapsedSeconds = 0.0f;
}

void FGameXXKTrainingTravelVisualRuntime::CompleteTimedPhase()
{
	switch (VisualPhase)
	{
	case EGameXXKTrainingTravelVisualPhase::HeroAttack:
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::EnemyHit);
		break;
	case EGameXXKTrainingTravelVisualPhase::EnemyHit:
		if (ActiveCombatEvent.bEnemyDefeated)
		{
			SetVisualPhase(EGameXXKTrainingTravelVisualPhase::EnemyDeath);
		}
		else if ((ActiveCombatEvent.DamagedPartyIndex != INDEX_NONE
				&& ActiveCombatEvent.PartyBefore.IsValidIndex(ActiveCombatEvent.DamagedPartyIndex)
				&& ActiveCombatEvent.PartyAfter.IsValidIndex(ActiveCombatEvent.DamagedPartyIndex)
				&& ActiveCombatEvent.PartyAfter[ActiveCombatEvent.DamagedPartyIndex].HP
					< ActiveCombatEvent.PartyBefore[ActiveCombatEvent.DamagedPartyIndex].HP)
			|| (ActiveCombatEvent.PartyBefore.IsEmpty()
				&& ActiveCombatEvent.HeroHealthAfter < ActiveCombatEvent.HeroHealthBefore))
		{
			SetVisualPhase(EGameXXKTrainingTravelVisualPhase::EnemyAttack);
		}
		else
		{
			CompleteActiveCombatEvent();
		}
		break;
	case EGameXXKTrainingTravelVisualPhase::EnemyAttack:
		SetVisualPhase(EGameXXKTrainingTravelVisualPhase::HeroHit);
		break;
	case EGameXXKTrainingTravelVisualPhase::HeroHit:
		if (ActiveCombatEvent.bHeroDefeated)
		{
			SetVisualPhase(EGameXXKTrainingTravelVisualPhase::HeroDeath);
		}
		else
		{
			CompleteActiveCombatEvent();
		}
		break;
	case EGameXXKTrainingTravelVisualPhase::EnemyDeath:
	case EGameXXKTrainingTravelVisualPhase::HeroDeath:
		CompleteActiveCombatEvent();
		break;
	default:
		break;
	}
}

float FGameXXKTrainingTravelVisualRuntime::GetCurrentPhaseDuration() const
{
	switch (VisualPhase)
	{
	case EGameXXKTrainingTravelVisualPhase::HeroAttack: return HeroAttackSeconds;
	case EGameXXKTrainingTravelVisualPhase::EnemyHit: return EnemyHitSeconds;
	case EGameXXKTrainingTravelVisualPhase::EnemyAttack: return EnemyAttackSeconds;
	case EGameXXKTrainingTravelVisualPhase::HeroHit: return HeroHitSeconds;
	case EGameXXKTrainingTravelVisualPhase::EnemyDeath: return EnemyDeathSeconds;
	case EGameXXKTrainingTravelVisualPhase::HeroDeath: return HeroDeathSeconds;
	default: return 0.0f;
	}
}

void FGameXXKTrainingTravelVisualRuntime::AdvanceMotion(const float DeltaSeconds)
{
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (SafeDeltaSeconds <= 0.0f)
	{
		return;
	}
	const float TargetScrollSpeed = VisualPhase == EGameXXKTrainingTravelVisualPhase::Walking
		? LaneScrollSpeed
		: 0.0f;
	const float Blend = 1.0f - FMath::Exp(-ScrollResponse * SafeDeltaSeconds);
	CurrentScrollSpeed = FMath::Lerp(CurrentScrollSpeed, TargetScrollSpeed, Blend);
	if (CurrentScrollSpeed < KINDA_SMALL_NUMBER)
	{
		CurrentScrollSpeed = 0.0f;
	}
	ScrollOffset = FMath::Fmod(ScrollOffset + CurrentScrollSpeed * SafeDeltaSeconds, LaneTileWidth);
	if (ScrollOffset < 0.0f)
	{
		ScrollOffset += LaneTileWidth;
	}

	if (VisualPhase != EGameXXKTrainingTravelVisualPhase::Walking)
	{
		return;
	}
	WalkFrameAccumulator = FMath::Fmod(
		WalkFrameAccumulator + SafeDeltaSeconds * WalkFramesPerSecond,
		static_cast<float>(WalkFrameCount));
	if (WalkFrameAccumulator < 0.0f)
	{
		WalkFrameAccumulator += static_cast<float>(WalkFrameCount);
	}
	WalkFrameIndex = FMath::Clamp(FMath::FloorToInt(WalkFrameAccumulator), 0, WalkFrameCount - 1);
}

float FGameXXKTrainingTravelVisualRuntime::GetPhaseProgress(const float DurationSeconds) const
{
	return DurationSeconds > 0.0f
		? FMath::Clamp(VisualPhaseElapsedSeconds / DurationSeconds, 0.0f, 1.0f)
		: 1.0f;
}
