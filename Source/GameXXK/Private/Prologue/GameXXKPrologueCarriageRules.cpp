#include "Prologue/GameXXKPrologueCarriageRules.h"

namespace
{
	float SafeDeltaSeconds(const float DeltaSeconds)
	{
		return FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f
			? DeltaSeconds
			: 0.0f;
	}

	float SafeDuration(const float Duration)
	{
		return FMath::IsFinite(Duration) && Duration > KINDA_SMALL_NUMBER
			? Duration
			: KINDA_SMALL_NUMBER;
	}

	float SafeElapsedSeconds(const float ElapsedSeconds)
	{
		return FMath::IsFinite(ElapsedSeconds)
			? FMath::Max(0.0f, ElapsedSeconds)
			: 0.0f;
	}

	void FillPresentation(
		const FGameXXKPrologueCarriageConfig& Config,
		const FGameXXKPrologueCarriageState& State,
		FGameXXKPrologueCarriageStepOutput& OutStep)
	{
		switch (State.Phase)
		{
		case EGameXXKPrologueCarriagePhase::Arriving:
			OutStep.Atlas = EGameXXKPrologueCarriageAtlas::RunStop;
			OutStep.AtlasFrameIndex = FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
				State.Phase,
				State.PhaseElapsedSeconds,
				Config);
			OutStep.MotionAlpha = FMath::Clamp(
				State.PhaseElapsedSeconds / SafeDuration(Config.ArrivalSeconds),
				0.0f,
				1.0f);
			break;
		case EGameXXKPrologueCarriagePhase::Parked:
		case EGameXXKPrologueCarriagePhase::HeroRevealed:
		case EGameXXKPrologueCarriagePhase::HoldTwoSeconds:
			OutStep.Atlas = EGameXXKPrologueCarriageAtlas::PostStopIdle;
			OutStep.AtlasFrameIndex = FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
				State.Phase,
				State.PhaseElapsedSeconds,
				Config);
			OutStep.MotionAlpha = 1.0f;
			break;
		case EGameXXKPrologueCarriagePhase::Departing:
			OutStep.Atlas = EGameXXKPrologueCarriageAtlas::RunStop;
			OutStep.AtlasFrameIndex = FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
				State.Phase,
				State.PhaseElapsedSeconds,
				Config);
			OutStep.MotionAlpha = FMath::Clamp(
				State.PhaseElapsedSeconds / SafeDuration(Config.DepartureSeconds),
				0.0f,
				1.0f);
			break;
		default:
			break;
		}
	}
}

bool FGameXXKPrologueCarriageRules::Start(
	FGameXXKPrologueCarriageState& InOutState)
{
	if (InOutState.Phase != EGameXXKPrologueCarriagePhase::Dormant)
	{
		return false;
	}
	InOutState = FGameXXKPrologueCarriageState();
	InOutState.Phase = EGameXXKPrologueCarriagePhase::Arriving;
	return true;
}

bool FGameXXKPrologueCarriageRules::Advance(
	const float DeltaSeconds,
	const FGameXXKPrologueCarriageConfig& Config,
	FGameXXKPrologueCarriageState& InOutState,
	FGameXXKPrologueCarriageStepOutput& OutStep)
{
	OutStep = FGameXXKPrologueCarriageStepOutput();
	OutStep.PreviousPhase = InOutState.Phase;
	OutStep.CurrentPhase = InOutState.Phase;

	if (InOutState.bPaused)
	{
		FillPresentation(Config, InOutState, OutStep);
		return false;
	}

	const float SafeDelta = SafeDeltaSeconds(DeltaSeconds);
	switch (InOutState.Phase)
	{
	case EGameXXKPrologueCarriagePhase::Arriving:
		InOutState.PhaseElapsedSeconds += SafeDelta;
		FillPresentation(Config, InOutState, OutStep);
		if (InOutState.PhaseElapsedSeconds >= SafeDuration(Config.ArrivalSeconds))
		{
			InOutState.Phase = EGameXXKPrologueCarriagePhase::Parked;
			InOutState.PhaseElapsedSeconds = 0.0f;
			OutStep.Atlas = EGameXXKPrologueCarriageAtlas::RunStop;
			OutStep.AtlasFrameIndex = FMath::Max(0, Config.FullFrameCount - 1);
			OutStep.MotionAlpha = 1.0f;
		}
		break;
	case EGameXXKPrologueCarriagePhase::Parked:
		InOutState.Phase = EGameXXKPrologueCarriagePhase::HeroRevealed;
		InOutState.PhaseElapsedSeconds = 0.0f;
		OutStep.bSwitchToIdle = true;
		OutStep.bRevealHero = true;
		FillPresentation(Config, InOutState, OutStep);
		break;
	case EGameXXKPrologueCarriagePhase::HeroRevealed:
		InOutState.Phase = EGameXXKPrologueCarriagePhase::HoldTwoSeconds;
		InOutState.PhaseElapsedSeconds = 0.0f;
		FillPresentation(Config, InOutState, OutStep);
		break;
	case EGameXXKPrologueCarriagePhase::HoldTwoSeconds:
		InOutState.PhaseElapsedSeconds += SafeDelta;
		FillPresentation(Config, InOutState, OutStep);
		if (InOutState.PhaseElapsedSeconds >= SafeDuration(Config.HoldSeconds))
		{
			InOutState.Phase = EGameXXKPrologueCarriagePhase::Departing;
			InOutState.PhaseElapsedSeconds = 0.0f;
			OutStep.bBeginDeparture = true;
		}
		break;
	case EGameXXKPrologueCarriagePhase::Departing:
		InOutState.PhaseElapsedSeconds += SafeDelta;
		FillPresentation(Config, InOutState, OutStep);
		if (InOutState.PhaseElapsedSeconds >= SafeDuration(Config.DepartureSeconds))
		{
			InOutState.Phase = EGameXXKPrologueCarriagePhase::Handoff;
			InOutState.PhaseElapsedSeconds = 0.0f;
			OutStep.MotionAlpha = 1.0f;
			OutStep.bBeginHandoff = true;
		}
		break;
	case EGameXXKPrologueCarriagePhase::Handoff:
		InOutState.Phase = EGameXXKPrologueCarriagePhase::Finished;
		InOutState.PhaseElapsedSeconds = 0.0f;
		break;
	case EGameXXKPrologueCarriagePhase::Dormant:
	case EGameXXKPrologueCarriagePhase::Finished:
	case EGameXXKPrologueCarriagePhase::Cancelled:
	default:
		return false;
	}

	OutStep.CurrentPhase = InOutState.Phase;
	return true;
}

void FGameXXKPrologueCarriageRules::SetPaused(
	FGameXXKPrologueCarriageState& InOutState,
	const bool bPaused)
{
	if (InOutState.Phase != EGameXXKPrologueCarriagePhase::Dormant
		&& InOutState.Phase != EGameXXKPrologueCarriagePhase::Finished
		&& InOutState.Phase != EGameXXKPrologueCarriagePhase::Cancelled)
	{
		InOutState.bPaused = bPaused;
	}
}

bool FGameXXKPrologueCarriageRules::Cancel(
	FGameXXKPrologueCarriageState& InOutState)
{
	if (InOutState.Phase == EGameXXKPrologueCarriagePhase::Dormant
		|| InOutState.Phase == EGameXXKPrologueCarriagePhase::Finished
		|| InOutState.Phase == EGameXXKPrologueCarriagePhase::Cancelled)
	{
		return false;
	}
	InOutState.Phase = EGameXXKPrologueCarriagePhase::Cancelled;
	InOutState.PhaseElapsedSeconds = 0.0f;
	InOutState.bPaused = false;
	return true;
}

bool FGameXXKPrologueCarriageRules::ConsumeFinishBroadcast(
	FGameXXKPrologueCarriageState& InOutState)
{
	if (InOutState.Phase != EGameXXKPrologueCarriagePhase::Finished
		|| InOutState.bFinishBroadcastConsumed)
	{
		return false;
	}
	InOutState.bFinishBroadcastConsumed = true;
	return true;
}

int32 FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
	const EGameXXKPrologueCarriagePhase Phase,
	const float PhaseElapsedSeconds,
	const FGameXXKPrologueCarriageConfig& Config)
{
	const float SafeElapsed = SafeElapsedSeconds(PhaseElapsedSeconds);
	const float SafeFps = FMath::IsFinite(Config.FramesPerSecond)
		&& Config.FramesPerSecond > 0.0f
		? Config.FramesPerSecond
		: 1.0f;
	const int32 RawFrame = FMath::Max(0, FMath::FloorToInt(SafeElapsed * SafeFps));
	const int32 FullFrameCount = FMath::Max(1, Config.FullFrameCount);

	switch (Phase)
	{
	case EGameXXKPrologueCarriagePhase::Arriving:
		return FMath::Clamp(RawFrame, 0, FullFrameCount - 1);
	case EGameXXKPrologueCarriagePhase::Parked:
	case EGameXXKPrologueCarriagePhase::HeroRevealed:
	case EGameXXKPrologueCarriagePhase::HoldTwoSeconds:
		return RawFrame % FullFrameCount;
	case EGameXXKPrologueCarriagePhase::Departing:
	{
		const int32 FirstFrame = FMath::Max(0, Config.DepartureFirstFrame);
		const int32 LastFrame = FMath::Max(FirstFrame, Config.DepartureLastFrame);
		const int32 RangeLength = LastFrame - FirstFrame + 1;
		return FirstFrame + RawFrame % RangeLength;
	}
	default:
		return 0;
	}
}
