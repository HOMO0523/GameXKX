#pragma once
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Narrative/GameXXKMainStoryCatalog.h"
struct FGameXXKRuntimeState;

/** Stateless adapter from saved main-story progress to the existing dialogue presenter. */
namespace GameXXKMainStoryDialoguePresentation
{
	GAMEXXK_API bool IsActive(const FGameXXKRuntimeState& State);
	GAMEXXK_API FGameXXKDialoguePresentationView LineView(const FGameXXKMainStoryNode& Node,int32 LineIndex);
	GAMEXXK_API FGameXXKDialoguePresentationView Build(const FGameXXKRuntimeState& State,const FText& Feedback);
	GAMEXXK_API bool SameView(const FGameXXKDialoguePresentationView& A,const FGameXXKDialoguePresentationView& B);
	GAMEXXK_API int32 ChoiceIndex(FName OptionId);
}
