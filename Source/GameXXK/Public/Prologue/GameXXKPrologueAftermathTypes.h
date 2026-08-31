#pragma once

#include "CoreMinimal.h"

#include "GameXXKPrologueAftermathTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKPrologueAftermathPhase : uint8
{
	Dormant,
	HeroNotice,
	MapThumbnail,
	MapInspection,
	YueBaiIntro,
	FoodDialogue,
	GuideDialogue,
	YueBaiFollowing,
	StatuePrompt,
	TutorialTravelPending,
	Finished,
	Cancelled,
};

UENUM()
enum class EGameXXKPrologueAftermathEvent : uint8
{
	DialogueCompleted,
	OpenInspection,
	CloseInspection,
	ContinuePressed,
	YueBaiIntroCompleted,
	GuideDialogueStarted,
	FollowerActivated,
	StatueInteracted,
	TutorialReturned,
	Cancel,
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPrologueAftermathState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Aftermath")
	EGameXXKPrologueAftermathPhase Phase = EGameXXKPrologueAftermathPhase::Dormant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Aftermath")
	bool bPaused = false;
};
