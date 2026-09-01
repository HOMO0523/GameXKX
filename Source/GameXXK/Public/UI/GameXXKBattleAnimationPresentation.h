#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

enum class EGameXXKBattleAnimationAction : uint8
{
	Idle,
	Attack,
	Hit,
	Buff,
	Debuff,
	Death,
	Impact
};

struct GAMEXXK_API FGameXXKBattleAnimationClipDescriptor
{
	FString AssetId;
	FSoftObjectPath TexturePath;
	int32 FrameCount = 0;
	int32 Columns = 8;
	int32 Rows = 8;
	float SourceFramesPerSecond = 12.0f;
	float PlaybackRate = 1.0f;

	bool IsValid() const
	{
		if (AssetId.IsEmpty() || TexturePath.IsNull() || FrameCount <= 0 || Columns <= 0 || Rows <= 0
			|| !FMath::IsFinite(SourceFramesPerSecond) || SourceFramesPerSecond <= 0.0f
			|| !FMath::IsFinite(PlaybackRate) || PlaybackRate <= 0.0f)
		{
			return false;
		}

		const int64 AtlasCapacity = static_cast<int64>(Columns) * static_cast<int64>(Rows);
		return static_cast<int64>(FrameCount) <= AtlasCapacity;
	}
};

/** Compact Travel prefers the 1K sibling but can stream the matching 2K production clip. */
struct GAMEXXK_API FGameXXKBattleAnimationClipPair
{
	FGameXXKBattleAnimationClipDescriptor Preferred;
	FGameXXKBattleAnimationClipDescriptor Fallback;
};

/** Immutable presentation data captured from ordered combat results. */
struct GAMEXXK_API FGameXXKBattlePresentationEvent
{
	uint64 EventId = 0;
	int32 HitOrdinal = 0;
	FName AttackerUnitId = NAME_None;
	FName TargetUnitId = NAME_None;
	bool bAttackerEnemy = false;
	bool bTargetEnemy = false;
	int32 TargetArmorBefore = 0;
	int32 TargetArmorAfter = 0;
	int32 ArmorAbsorbed = 0;
	int32 HealthDamage = 0;
	int32 TargetHealthBefore = 0;
	int32 TargetHealthAfter = 0;
	bool bAvoided = false;
	bool bTargetDefeated = false;
};

/** Immutable presentation data captured from one unit's net status-stack change. */
struct GAMEXXK_API FGameXXKBattleStatusPresentationEvent
{
	uint64 EventId = 0;
	FName UnitId = NAME_None;
	bool bUnitEnemy = false;
	EGameXXKCardStatus Status = EGameXXKCardStatus::Invalid;
	int32 StackBefore = 0;
	int32 StackAfter = 0;
	int32 StackDelta = 0;
	EGameXXKBattleAnimationAction AnimationAction = EGameXXKBattleAnimationAction::Idle;
};

enum class EGameXXKBattlePresentationImpactTier : uint8
{
	None,
	Avoided,
	Light,
	Medium,
	Heavy,
	Lethal
};

/** Pure, layout-independent timing and feedback resolved from one immutable presentation event. */
struct GAMEXXK_API FGameXXKBattlePresentationRhythm
{
	float DurationSeconds = 0.0f;
	float ImpactSeconds = 0.0f;
	FVector2f ShakeAmplitude = FVector2f(0.0f, 0.0f);
	float ShakeDurationSeconds = 0.0f;
	float ReadoutPeakScale = 1.0f;
	EGameXXKBattlePresentationImpactTier ImpactTier = EGameXXKBattlePresentationImpactTier::None;

	bool IsValid() const
	{
		return FMath::IsFinite(DurationSeconds)
			&& DurationSeconds > 0.0f
			&& FMath::IsFinite(ImpactSeconds)
			&& ImpactSeconds >= 0.0f
			&& ImpactSeconds <= DurationSeconds
			&& FMath::IsFinite(ShakeAmplitude.X)
			&& FMath::IsFinite(ShakeAmplitude.Y)
			&& ShakeAmplitude.X >= 0.0f
			&& ShakeAmplitude.Y >= 0.0f
			&& FMath::IsFinite(ShakeDurationSeconds)
			&& ShakeDurationSeconds >= 0.0f
			&& FMath::IsFinite(ReadoutPeakScale)
			&& ReadoutPeakScale >= 1.0f;
	}
};

struct GAMEXXK_API FGameXXKBattleAnimationCombatRequest
{
	FName AttackerUnitId = NAME_None;
	FName TargetUnitId = NAME_None;
	bool bAttackerEnemy = false;
	bool bTargetEnemy = false;
	bool bTargetDefeated = false;
};

class GAMEXXK_API FGameXXKBattleAnimationPresentation
{
public:
	static FString ResolveUnitAssetId(FName RuntimeUnitId, bool bEnemy);
	static FGameXXKBattleAnimationClipDescriptor ResolveClip(
		FName RuntimeUnitId,
		bool bEnemy,
		EGameXXKBattleAnimationAction Action);
	/** Enemy close-ups prefer the catalog definition so opaque runtime IDs cannot fall back to legacy placeholder art. */
	static FGameXXKBattleAnimationClipDescriptor ResolveClipForDefinition(
		FName RuntimeUnitId,
		FName EnemyDefinitionId,
		bool bEnemy,
		EGameXXKBattleAnimationAction Action);
	/** Selects the corrected hero punch/kick pair stably; enemies and other party units retain their authored attack. */
	static FGameXXKBattleAnimationClipDescriptor ResolveAttackClipForEvent(
		FName RuntimeUnitId,
		FName EnemyDefinitionId,
		bool bEnemy,
		uint64 EventId);
	static FGameXXKBattleAnimationClipPair ResolveCompactTravelClipPair(
		FName RuntimeUnitId,
		bool bEnemy,
		EGameXXKBattleAnimationAction Action);
	static FGameXXKBattleAnimationClipDescriptor ResolveGenericClip(EGameXXKBattleAnimationAction Action);
	/** Selects one of four approved hit VFX with a save-stable battle seed and event ordinal. */
	static FGameXXKBattleAnimationClipDescriptor ResolveHitEffectClip(int32 BattleSeed, uint64 EventId);
	static FSoftObjectPath ResolveIdleFlipbookPath(FName RuntimeUnitId, bool bEnemy);
	/** Source-less damage stays target-only; the legacy fallback parameter is intentionally ignored here. */
	static TArray<FGameXXKBattlePresentationEvent> BuildPresentationEvents(
		const FGameXXKCardBattleRuntime& PostDamageBattle,
		FName IgnoredFallbackAttackerUnitId,
		const TArray<FGameXXKCardDamageResult>& DamageResults);
	static TArray<FGameXXKBattleStatusPresentationEvent> BuildStatusPresentationEvents(
		const FGameXXKCardBattleRuntime& BeforeBattle,
		const FGameXXKCardBattleRuntime& AfterBattle);
	/** @deprecated Compatibility wrapper for the current controller; migrate consumers to immutable presentation events. */
	static TArray<FGameXXKBattleAnimationCombatRequest> BuildCombatRequests(
		const FGameXXKCardBattleRuntime& PostDamageBattle,
		FName FallbackAttackerUnitId,
		const TArray<FGameXXKCardDamageResult>& DamageResults);
	static int32 CalculateFrameIndex(
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		float RuntimeElapsedSeconds,
		bool bLooping);
	static FBox2f CalculateUvRegion(const FGameXXKBattleAnimationClipDescriptor& Clip, int32 FrameIndex);
	static FGameXXKBattlePresentationRhythm ResolveCombatRhythm(
		const FGameXXKBattlePresentationEvent& Event);
	static FGameXXKBattlePresentationRhythm ResolveDeathRhythm();
	static FGameXXKBattlePresentationRhythm ResolveStatusRhythm();
	static FGameXXKBattleAnimationClipDescriptor FitClipToDuration(
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		float TargetDurationSeconds);
	static float GetRuntimeDuration(const FGameXXKBattleAnimationClipDescriptor& Clip);
	static float GetImpactRuntimeSeconds();
	static float GetHitEffectDurationSeconds();
	/** Half of the canonical 410px formation unit used by the procedural recoil. */
	static float GetProceduralHitRetreatDistance();
	/** Idle-pose recoil, mirrored away from the opposing side and returning to zero. */
	static FVector2D CalculateProceduralHitOffset(bool bTargetEnemy, float NormalizedProgress);
	/** Opacity-only death presentation; one at start and zero at completion. */
	static float CalculateProceduralDeathOpacity(float NormalizedProgress);
};
