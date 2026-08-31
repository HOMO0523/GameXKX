#include "Prologue/GameXXKPrologueAftermathRules.h"

bool FGameXXKPrologueAftermathRules::Start(
	FGameXXKPrologueAftermathState& InOutState)
{
	if (InOutState.Phase != EGameXXKPrologueAftermathPhase::Dormant)
	{
		return false;
	}
	InOutState.Phase = EGameXXKPrologueAftermathPhase::HeroNotice;
	InOutState.bPaused = false;
	return true;
}

bool FGameXXKPrologueAftermathRules::ApplyEvent(
	const EGameXXKPrologueAftermathEvent Event,
	FGameXXKPrologueAftermathState& InOutState)
{
	if (Event == EGameXXKPrologueAftermathEvent::Cancel)
	{
		if (InOutState.Phase == EGameXXKPrologueAftermathPhase::Finished
			|| InOutState.Phase == EGameXXKPrologueAftermathPhase::Cancelled)
		{
			return false;
		}
		InOutState.Phase = EGameXXKPrologueAftermathPhase::Cancelled;
		InOutState.bPaused = false;
		return true;
	}
	if (InOutState.bPaused)
	{
		return false;
	}

	auto AdvanceTo = [&InOutState](const EGameXXKPrologueAftermathPhase Next)
	{
		InOutState.Phase = Next;
		return true;
	};

	switch (InOutState.Phase)
	{
	case EGameXXKPrologueAftermathPhase::HeroNotice:
		return Event == EGameXXKPrologueAftermathEvent::DialogueCompleted
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::MapThumbnail);
	case EGameXXKPrologueAftermathPhase::MapThumbnail:
		if (Event == EGameXXKPrologueAftermathEvent::OpenInspection)
		{
			return AdvanceTo(EGameXXKPrologueAftermathPhase::MapInspection);
		}
		return Event == EGameXXKPrologueAftermathEvent::ContinuePressed
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::YueBaiIntro);
	case EGameXXKPrologueAftermathPhase::MapInspection:
		return Event == EGameXXKPrologueAftermathEvent::CloseInspection
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::MapThumbnail);
	case EGameXXKPrologueAftermathPhase::YueBaiIntro:
		return Event == EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::FoodDialogue);
	case EGameXXKPrologueAftermathPhase::FoodDialogue:
		return Event == EGameXXKPrologueAftermathEvent::GuideDialogueStarted
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::GuideDialogue);
	case EGameXXKPrologueAftermathPhase::GuideDialogue:
		return Event == EGameXXKPrologueAftermathEvent::DialogueCompleted
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::YueBaiFollowing);
	case EGameXXKPrologueAftermathPhase::YueBaiFollowing:
		return Event == EGameXXKPrologueAftermathEvent::FollowerActivated
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::StatuePrompt);
	case EGameXXKPrologueAftermathPhase::StatuePrompt:
		return Event == EGameXXKPrologueAftermathEvent::StatueInteracted
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::TutorialTravelPending);
	case EGameXXKPrologueAftermathPhase::TutorialTravelPending:
		return Event == EGameXXKPrologueAftermathEvent::TutorialReturned
			&& AdvanceTo(EGameXXKPrologueAftermathPhase::Finished);
	default:
		return false;
	}
}

void FGameXXKPrologueAftermathRules::SetPaused(
	FGameXXKPrologueAftermathState& InOutState,
	const bool bPaused)
{
	if (InOutState.Phase == EGameXXKPrologueAftermathPhase::Finished
		|| InOutState.Phase == EGameXXKPrologueAftermathPhase::Cancelled)
	{
		InOutState.bPaused = false;
		return;
	}
	InOutState.bPaused = bPaused;
}

bool FGameXXKPrologueAftermathRules::IsBlockingPhase(
	const EGameXXKPrologueAftermathPhase Phase)
{
	switch (Phase)
	{
	case EGameXXKPrologueAftermathPhase::HeroNotice:
	case EGameXXKPrologueAftermathPhase::MapThumbnail:
	case EGameXXKPrologueAftermathPhase::MapInspection:
	case EGameXXKPrologueAftermathPhase::YueBaiIntro:
	case EGameXXKPrologueAftermathPhase::FoodDialogue:
	case EGameXXKPrologueAftermathPhase::GuideDialogue:
	case EGameXXKPrologueAftermathPhase::TutorialTravelPending:
		return true;
	default:
		return false;
	}
}
