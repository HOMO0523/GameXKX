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

/** Immutable presentation data captured from ordered combat results. */
struct GAMEXXK_API FGameXXKBattlePresentationEvent
{
	uint64 EventId = 0;
	int32 HitOrdinal = 0;
	FName AttackerUnitId = NAME_None;
	FName TargetUnitId = NAME_None;
	bool bAttackerEnemy = false;
	bool bTargetEnemy = false;
	int32 HealthDamage = 0;
	int32 TargetHealthBefore = 0;
	int32 TargetHealthAfter = 0;
	bool bAvoided = false;
	bool bTargetDefeated = false;
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
	static FGameXXKBattleAnimationClipDescriptor ResolveGenericClip(EGameXXKBattleAnimationAction Action);
	static FSoftObjectPath ResolveIdleFlipbookPath(FName RuntimeUnitId, bool bEnemy);
	static TArray<FGameXXKBattlePresentationEvent> BuildPresentationEvents(
		const FGameXXKCardBattleRuntime& PostDamageBattle,
		FName FallbackAttackerUnitId,
		const TArray<FGameXXKCardDamageResult>& DamageResults);
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
	static float GetRuntimeDuration(const FGameXXKBattleAnimationClipDescriptor& Clip);
	static float GetImpactRuntimeSeconds();
};
