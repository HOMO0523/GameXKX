#include "Prologue/GameXXKPrologueCarriageRules.h"

#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueCarriageRulesTest,
	"GameXXK.Prologue.Carriage.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueCarriageRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKPrologueCarriageConfig Config;
	TestTrue(TEXT("arrival duration matches the authored run-stop clip"),
		FMath::IsNearlyEqual(Config.ArrivalSeconds, 4.04f));
	TestTrue(TEXT("parked hold lasts exactly two seconds"),
		FMath::IsNearlyEqual(Config.HoldSeconds, 2.0f));
	TestTrue(TEXT("departure duration is bounded"),
		FMath::IsNearlyEqual(Config.DepartureSeconds, 4.04f));
	TestEqual(TEXT("full atlas contains sixty frames"), Config.FullFrameCount, 60);
	TestEqual(TEXT("audited departure begins at frame zero"),
		Config.DepartureFirstFrame, 0);
	TestEqual(TEXT("audited departure excludes braking frames"),
		Config.DepartureLastFrame, 35);

	FGameXXKPrologueCarriageState State;
	FGameXXKPrologueCarriageStepOutput Output;
	TestEqual(TEXT("new state is dormant"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Dormant);
	TestTrue(TEXT("preview starts once"),
		FGameXXKPrologueCarriageRules::Start(State));
	TestFalse(TEXT("an active preview rejects duplicate start"),
		FGameXXKPrologueCarriageRules::Start(State));
	TestEqual(TEXT("preview starts in arrival"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Arriving);

	TestTrue(TEXT("zero delta still presents the first arrival frame"),
		FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output));
	TestEqual(TEXT("arrival uses the run-stop atlas"),
		Output.Atlas,
		EGameXXKPrologueCarriageAtlas::RunStop);
	TestEqual(TEXT("arrival begins on frame zero"), Output.AtlasFrameIndex, 0);
	TestTrue(TEXT("arrival begins at the start marker"),
		FMath::IsNearlyZero(Output.MotionAlpha));

	FGameXXKPrologueCarriageRules::Advance(2.02f, Config, State, Output);
	TestEqual(TEXT("half arrival remains active"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Arriving);
	TestTrue(TEXT("half arrival has intermediate motion"),
		Output.MotionAlpha > 0.49f && Output.MotionAlpha < 0.51f);
	TestTrue(TEXT("half arrival uses a valid full-clip frame"),
		Output.AtlasFrameIndex >= 0 && Output.AtlasFrameIndex < 60);

	FGameXXKPrologueCarriageRules::SetPaused(State, true);
	const FGameXXKPrologueCarriageState PausedBefore = State;
	FGameXXKPrologueCarriageRules::Advance(10.0f, Config, State, Output);
	TestTrue(TEXT("pause freezes the complete timeline state"), State == PausedBefore);
	TestFalse(TEXT("paused output cannot request a transition"), Output.bRevealHero);
	FGameXXKPrologueCarriageRules::SetPaused(State, false);

	FGameXXKPrologueCarriageRules::Advance(2.02f, Config, State, Output);
	TestEqual(TEXT("arrival reaches an explicit parked boundary"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Parked);
	TestEqual(TEXT("arrival ends on frame fifty-nine"), Output.AtlasFrameIndex, 59);
	TestTrue(TEXT("arrival reaches the stop marker"),
		FMath::IsNearlyEqual(Output.MotionAlpha, 1.0f));

	FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output);
	TestEqual(TEXT("parking enters the hero reveal boundary"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::HeroRevealed);
	TestTrue(TEXT("parking switches to idle"), Output.bSwitchToIdle);
	TestTrue(TEXT("parking reveals the existing hero"), Output.bRevealHero);
	TestEqual(TEXT("parked frame uses the idle atlas"),
		Output.Atlas,
		EGameXXKPrologueCarriageAtlas::PostStopIdle);

	FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output);
	TestEqual(TEXT("hero reveal advances into the timed hold"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::HoldTwoSeconds);
	FGameXXKPrologueCarriageRules::Advance(1.99f, Config, State, Output);
	TestEqual(TEXT("hold cannot depart early"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::HoldTwoSeconds);
	FGameXXKPrologueCarriageRules::Advance(0.01f, Config, State, Output);
	TestEqual(TEXT("two unpaused seconds begins departure"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Departing);
	TestTrue(TEXT("departure transition is explicit"), Output.bBeginDeparture);

	FGameXXKPrologueCarriageRules::Advance(0.01f, Config, State, Output);
	TestEqual(TEXT("departure reuses the run-stop texture"),
		Output.Atlas,
		EGameXXKPrologueCarriageAtlas::RunStop);
	TestTrue(TEXT("departure uses only audited running frames"),
		Output.AtlasFrameIndex >= 0 && Output.AtlasFrameIndex <= 35);
	TestTrue(TEXT("departure begins near the stop marker"), Output.MotionAlpha < 0.01f);

	FGameXXKPrologueCarriageRules::Advance(4.03f, Config, State, Output);
	TestEqual(TEXT("bounded departure reaches handoff"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Handoff);
	TestTrue(TEXT("handoff request is explicit"), Output.bBeginHandoff);
	FGameXXKPrologueCarriageRules::Advance(0.0f, Config, State, Output);
	TestEqual(TEXT("handoff finishes the timeline"),
		State.Phase,
		EGameXXKPrologueCarriagePhase::Finished);
	TestTrue(TEXT("finish broadcasts once"),
		FGameXXKPrologueCarriageRules::ConsumeFinishBroadcast(State));
	TestFalse(TEXT("finish cannot broadcast twice"),
		FGameXXKPrologueCarriageRules::ConsumeFinishBroadcast(State));
	TestFalse(TEXT("finished timeline cannot restart in-place"),
		FGameXXKPrologueCarriageRules::Start(State));

	TestEqual(TEXT("arrival frame clamp begins at zero"),
		FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
			EGameXXKPrologueCarriagePhase::Arriving,
			-1.0f,
			Config),
		0);
	TestEqual(TEXT("arrival frame clamp ends at fifty-nine"),
		FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
			EGameXXKPrologueCarriagePhase::Arriving,
			100.0f,
			Config),
		59);
	TestEqual(TEXT("idle frames loop after one source duration"),
		FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
			EGameXXKPrologueCarriagePhase::HoldTwoSeconds,
			Config.FullFrameCount / Config.FramesPerSecond,
			Config),
		0);
	TestEqual(TEXT("departure wraps before braking frame thirty-six"),
		FGameXXKPrologueCarriageRules::ResolveAtlasFrame(
			EGameXXKPrologueCarriagePhase::Departing,
			36.0f / Config.FramesPerSecond,
			Config),
		0);

	FGameXXKPrologueCarriageState Cancelled;
	TestTrue(TEXT("cancel fixture starts"),
		FGameXXKPrologueCarriageRules::Start(Cancelled));
	TestTrue(TEXT("active timeline can cancel"),
		FGameXXKPrologueCarriageRules::Cancel(Cancelled));
	TestEqual(TEXT("cancel enters terminal cancelled state"),
		Cancelled.Phase,
		EGameXXKPrologueCarriagePhase::Cancelled);
	TestFalse(TEXT("repeated cancel is idempotent"),
		FGameXXKPrologueCarriageRules::Cancel(Cancelled));
	TestFalse(TEXT("cancelled timeline never broadcasts success"),
		FGameXXKPrologueCarriageRules::ConsumeFinishBroadcast(Cancelled));

	FGameXXKPrologueCarriageState InvalidDelta;
	FGameXXKPrologueCarriageRules::Start(InvalidDelta);
	FGameXXKPrologueCarriageRules::Advance(-10.0f, Config, InvalidDelta, Output);
	TestTrue(TEXT("negative delta clamps to zero"),
		FMath::IsNearlyZero(InvalidDelta.PhaseElapsedSeconds));
	FGameXXKPrologueCarriageRules::Advance(
		std::numeric_limits<float>::quiet_NaN(),
		Config,
		InvalidDelta,
		Output);
	TestTrue(TEXT("NaN delta clamps to zero"),
		FMath::IsNearlyZero(InvalidDelta.PhaseElapsedSeconds));

	return true;
}

#endif
