#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.h"

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
	static constexpr float LaneTileWidth = 600.0f;
	static constexpr float LaneScrollSpeed = 96.0f;

	void Reset();

	/** Advances only while the authoritative Travel runner is walking. */
	void Tick(float DeltaSeconds, EGameXXKTrainingTravelPhase Phase);

	/** Synchronizes visual state with an encounter/stage settlement. */
	void NotifyTravelStep(bool bEncounterCompleted, bool bStageCompleted);

	float GetScrollOffset() const { return ScrollOffset; }
	int32 GetWalkFrameIndex() const { return WalkFrameIndex; }
	int32 GetCompletedLoopCount() const { return CompletedLoopCount; }
	bool IsWalking() const { return bWalking; }
	bool IsPausedForEncounter() const { return bPausedForEncounter; }

private:
	float ScrollOffset = 0.0f;
	float WalkFrameAccumulator = 0.0f;
	int32 WalkFrameIndex = 0;
	int32 CompletedLoopCount = 0;
	bool bWalking = false;
	bool bPausedForEncounter = false;
};
