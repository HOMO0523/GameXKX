#include "Prologue/GameXXKPrologueAftermathRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueAftermathRulesTest,
	"GameXXK.Prologue.Aftermath.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueAftermathRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKPrologueAftermathState State;
	TestEqual(TEXT("new aftermath is dormant"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::Dormant);
	TestTrue(TEXT("aftermath starts at hero notice"),
		FGameXXKPrologueAftermathRules::Start(State));
	TestFalse(TEXT("active aftermath rejects duplicate start"),
		FGameXXKPrologueAftermathRules::Start(State));
	TestEqual(TEXT("notice phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::HeroNotice);

	TestTrue(TEXT("notice completion opens thumbnail"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::DialogueCompleted,
			State));
	TestEqual(TEXT("thumbnail phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::MapThumbnail);

	TestTrue(TEXT("inspection opens"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::OpenInspection,
			State));
	TestEqual(TEXT("inspection phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::MapInspection);
	TestFalse(TEXT("space cannot leave inspection"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::ContinuePressed,
			State));
	TestEqual(TEXT("inspection remains open"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::MapInspection);
	TestTrue(TEXT("close returns to thumbnail"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::CloseInspection,
			State));
	TestTrue(TEXT("thumbnail space begins reveal"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::ContinuePressed,
			State));
	TestEqual(TEXT("reveal phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::YueBaiIntro);

	FGameXXKPrologueAftermathRules::SetPaused(State, true);
	TestTrue(TEXT("state records pause"), State.bPaused);
	TestFalse(TEXT("paused aftermath rejects progress"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted,
			State));
	TestEqual(TEXT("pause freezes phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::YueBaiIntro);
	FGameXXKPrologueAftermathRules::SetPaused(State, false);

	TestTrue(TEXT("intro completion opens food dialogue"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted,
			State));
	TestEqual(TEXT("food phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::FoodDialogue);
	TestTrue(TEXT("guide node changes dialogue phase"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::GuideDialogueStarted,
			State));
	TestEqual(TEXT("guide phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::GuideDialogue);
	TestTrue(TEXT("dialogue completion begins follower handoff"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::DialogueCompleted,
			State));
	TestEqual(TEXT("follower phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::YueBaiFollowing);
	TestTrue(TEXT("follower activation shows statue prompt"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::FollowerActivated,
			State));
	TestEqual(TEXT("statue prompt phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::StatuePrompt);
	TestTrue(TEXT("statue interaction starts guarded travel"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::StatueInteracted,
			State));
	TestEqual(TEXT("travel pending phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::TutorialTravelPending);
	TestFalse(TEXT("duplicate statue interaction rejects"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::StatueInteracted,
			State));
	TestTrue(TEXT("tutorial return finishes current aftermath"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::TutorialReturned,
			State));
	TestEqual(TEXT("finished phase"),
		State.Phase,
		EGameXXKPrologueAftermathPhase::Finished);
	TestFalse(TEXT("finished phase rejects progress"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::ContinuePressed,
			State));

	TestTrue(TEXT("notice, map, reveal, and dialogue phases block input"),
		FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::HeroNotice)
		&& FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::MapThumbnail)
		&& FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::MapInspection)
		&& FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::YueBaiIntro)
		&& FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::FoodDialogue)
		&& FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::GuideDialogue));
	TestFalse(TEXT("following never blocks movement"),
		FGameXXKPrologueAftermathRules::IsBlockingPhase(
			EGameXXKPrologueAftermathPhase::StatuePrompt));

	FGameXXKPrologueAftermathState Cancelled;
	TestTrue(TEXT("cancel fixture starts"),
		FGameXXKPrologueAftermathRules::Start(Cancelled));
	TestTrue(TEXT("active aftermath can cancel"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::Cancel,
			Cancelled));
	TestEqual(TEXT("cancel terminal"),
		Cancelled.Phase,
		EGameXXKPrologueAftermathPhase::Cancelled);
	TestFalse(TEXT("repeated cancel is idempotent"),
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::Cancel,
			Cancelled));

	return true;
}

#endif
