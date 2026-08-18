#include "UI/GameXXKTrainingTravelVisualRuntime.h"

void FGameXXKTrainingTravelVisualRuntime::Reset()
{
	ScrollOffset = 0.0f;
	WalkFrameAccumulator = 0.0f;
	WalkFrameIndex = 0;
	CompletedLoopCount = 0;
	bWalking = false;
	bPausedForEncounter = false;
}

void FGameXXKTrainingTravelVisualRuntime::Tick(
	const float DeltaSeconds,
	const EGameXXKTrainingTravelPhase Phase)
{
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	bWalking = Phase == EGameXXKTrainingTravelPhase::Walking;
	bPausedForEncounter = Phase == EGameXXKTrainingTravelPhase::Combat
		|| Phase == EGameXXKTrainingTravelPhase::Defeated;

	if (!bWalking || SafeDeltaSeconds <= 0.0f)
	{
		return;
	}

	ScrollOffset = FMath::Fmod(
		ScrollOffset + SafeDeltaSeconds * LaneScrollSpeed,
		LaneTileWidth);
	if (ScrollOffset < 0.0f)
	{
		ScrollOffset += LaneTileWidth;
	}

	WalkFrameAccumulator = FMath::Fmod(
		WalkFrameAccumulator + SafeDeltaSeconds * WalkFramesPerSecond,
		static_cast<float>(WalkFrameCount));
	if (WalkFrameAccumulator < 0.0f)
	{
		WalkFrameAccumulator += static_cast<float>(WalkFrameCount);
	}
	WalkFrameIndex = FMath::Clamp(
		FMath::FloorToInt(WalkFrameAccumulator),
		0,
		WalkFrameCount - 1);
}

void FGameXXKTrainingTravelVisualRuntime::NotifyTravelStep(
	const bool bEncounterCompleted,
	const bool bStageCompleted)
{
	if (bStageCompleted)
	{
		ScrollOffset = 0.0f;
		WalkFrameAccumulator = 0.0f;
		WalkFrameIndex = 0;
		++CompletedLoopCount;
		bWalking = false;
		bPausedForEncounter = false;
		return;
	}

	if (bEncounterCompleted)
	{
		// The authoritative runner will return to Walking on its next frame.
		// Keep the current phase flags untouched until that snapshot arrives.
		bPausedForEncounter = false;
	}
}
