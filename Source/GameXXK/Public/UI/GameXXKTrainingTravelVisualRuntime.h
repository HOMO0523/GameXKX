#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

/** Real-time presentation phase; deliberately separate from the one-second Travel simulation phase. */
enum class EGameXXKTrainingTravelVisualPhase : uint8
{
	Walking,
	EncounterIdle,
	HeroAttack,
	EnemyHit,
	EnemyAttack,
	HeroHit,
	EnemyDeath,
	HeroDeath,
	Paused
};

/**
 * Presentation-only state for the desktop Training travel strip.
 *
 * The durable encounter cursor and combat state remain owned by
 * FGameXXKTrainingProgress/FGameXXKTrainingTravelRuntime.  This class only
 * advances the seamless lane offset and the generated hero walk atlas frame,
 * so Slate can update the strip without rebuilding the widget tree every tick.
 */
class GAMEXXK_API FGameXXKTrainingTravelVisualRuntime final
{
public:
	static constexpr int32 WalkFrameCount = 60;
	static constexpr float WalkFramesPerSecond = 12.0f;
	static constexpr float LaneTileWidth = 750.0f;
	static constexpr float LaneScrollSpeed = 96.0f;
	static constexpr float ScrollResponse = 8.0f;
	static constexpr float HeroAttackSeconds = 0.28f;
	static constexpr float EnemyHitSeconds = 0.14f;
	static constexpr float EnemyAttackSeconds = 0.28f;
	static constexpr float HeroHitSeconds = 0.14f;
	static constexpr float EnemyDeathSeconds = 0.45f;
	static constexpr float HeroDeathSeconds = 0.55f;

	void Reset();

	/** Refreshes the latest authoritative snapshot without interrupting a queued combat presentation. */
	void Synchronize(const FGameXXKTrainingTravelRuntime& Runtime);

	/** Advances motion and any queued combat presentation using real frame time. */
	void Tick(float DeltaSeconds);

	/** Compatibility wrapper for callers that have not yet moved snapshot capture to the mutation boundary. */
	void Tick(float DeltaSeconds, EGameXXKTrainingTravelPhase Phase);

	/** Captures the immutable combat result before the gameplay runner replaces it with the next encounter. */
	void NotifyTravelStep(
		const FGameXXKTrainingTravelRuntime& Before,
		const FGameXXKTrainingTravelRuntime& After,
		bool bEncounterCompleted,
		bool bStageCompleted,
		bool bDefeated);

	/** Compatibility wrapper retained until every workbench call site supplies snapshots. */
	void NotifyTravelStep(bool bEncounterCompleted, bool bStageCompleted);

	float GetScrollOffset() const { return ScrollOffset; }
	float GetScrollVelocity() const { return CurrentScrollSpeed; }
	int32 GetWalkFrameIndex() const { return WalkFrameIndex; }
	int32 GetCompletedLoopCount() const { return CompletedLoopCount; }
	EGameXXKTrainingTravelVisualPhase GetVisualPhase() const { return VisualPhase; }
	float GetVisualPhaseElapsedSeconds() const { return VisualPhaseElapsedSeconds; }
	EGameXXKBattleAnimationAction GetHeroAction() const;
	EGameXXKBattleAnimationAction GetPartyAction(int32 PartyIndex) const;
	EGameXXKBattleAnimationAction GetEnemyAction() const;
	FName GetEnemyDefinitionId() const;
	int32 GetEnemyFormationSlotCount() const;
	int32 GetPresentedEnemySlotIndex() const;
	FName GetEnemyDefinitionIdForSlot(int32 SlotIndex) const;
	bool IsEnemySlotVisible(int32 SlotIndex) const;
	float GetEnemyHealthFractionForSlot(int32 SlotIndex) const;
	FName GetStageId() const { return LatestRuntime.StageId; }
	bool IsEnemyVisible() const;
	float GetEnemyHealthFraction() const;
	float GetHeroHealthFraction() const;
	float GetPartyHealthFraction(int32 PartyIndex) const;
	bool IsWalking() const { return VisualPhase == EGameXXKTrainingTravelVisualPhase::Walking; }
	bool IsPausedForEncounter() const { return VisualPhase != EGameXXKTrainingTravelVisualPhase::Walking; }

private:
	struct FCombatEvent
	{
		FName EnemyDefinitionId = NAME_None;
		int32 EnemySlotIndex = INDEX_NONE;
		TArray<FGameXXKTrainingTravelEnemyRuntime> EnemiesBefore;
		TArray<FGameXXKTrainingTravelEnemyRuntime> EnemiesAfter;
		int32 EnemyHealthBefore = 0;
		int32 EnemyHealthAfter = 0;
		int32 EnemyMaxHealth = 1;
		int32 HeroHealthBefore = 0;
		int32 HeroHealthAfter = 0;
		int32 HeroMaxHealth = 1;
		TArray<FGameXXKTrainingTravelPartyUnitRuntime> PartyBefore;
		TArray<FGameXXKTrainingTravelPartyUnitRuntime> PartyAfter;
		int32 AttackingPartyIndex = INDEX_NONE;
		int32 DamagedPartyIndex = INDEX_NONE;
		bool bEnemyDefeated = false;
		bool bHeroDefeated = false;
	};

	void ApplyLatestAuthoritativePhase();
	void EnqueueCombatEvent(
		const FGameXXKTrainingTravelRuntime& Before,
		const FGameXXKTrainingTravelRuntime& After,
		bool bEncounterCompleted,
		bool bDefeated);
	void StartNextCombatEvent();
	void CompleteActiveCombatEvent();
	void SetVisualPhase(EGameXXKTrainingTravelVisualPhase Phase);
	void CompleteTimedPhase();
	float GetCurrentPhaseDuration() const;
	void AdvanceMotion(float DeltaSeconds);
	float GetPhaseProgress(float DurationSeconds) const;

	FGameXXKTrainingTravelRuntime LatestRuntime;
	TArray<FCombatEvent> PendingCombatEvents;
	FCombatEvent ActiveCombatEvent;
	EGameXXKTrainingTravelVisualPhase VisualPhase = EGameXXKTrainingTravelVisualPhase::Paused;
	float VisualPhaseElapsedSeconds = 0.0f;
	float ScrollOffset = 0.0f;
	float CurrentScrollSpeed = 0.0f;
	float WalkFrameAccumulator = 0.0f;
	int32 WalkFrameIndex = 0;
	int32 CompletedLoopCount = 0;
	bool bHasAuthoritativeSnapshot = false;
	bool bHasActiveCombatEvent = false;
};
