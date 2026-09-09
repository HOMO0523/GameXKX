#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

enum class EGameXXKMainStoryNodeKind : uint8 { Dialogue, Investigation, JourneyBattle, JourneyInvestigation };

struct GAMEXXK_API FGameXXKMainStoryLine
{
	FName SpeakerId;
	FText SpeakerName;
	FText Text;
};

struct GAMEXXK_API FGameXXKMainStoryOption
{
	FText Text;
	FText Feedback;
	bool bCorrect = false;
};

struct GAMEXXK_API FGameXXKMainStoryNode
{
	FName Id;
	FName ChapterId;
	FName StoryId;
	FName StepId;
	FText Title;
	FText Summary;
	FText Objective;
	FText Result;
	EGameXXKMainStoryNodeKind Kind = EGameXXKMainStoryNodeKind::Dialogue;
	int32 StageNumber = 1;
	int32 Gold = 100000;
	int32 AdvancedBoxes = 0;
	int32 NormalBoxes = 0;
	int32 BoxLevel = 5;
	bool bOptional = false;
	bool bInsideJourney = false;
	TArray<FName> RequiresAll;
	TArray<FName> RequiresAny;
	TArray<FGameXXKMainStoryLine> Lines;
	TArray<FGameXXKMainStoryOption> Options;
	TArray<FText> Hints;
	FSoftObjectPath Illustration;
	bool IsJourney() const { return Kind == EGameXXKMainStoryNodeKind::JourneyBattle || Kind == EGameXXKMainStoryNodeKind::JourneyInvestigation; }
	bool IsInvestigation() const { return Kind == EGameXXKMainStoryNodeKind::Investigation || Kind == EGameXXKMainStoryNodeKind::JourneyInvestigation; }
};

struct GAMEXXK_API FGameXXKMainStoryChapter
{
	FName Id;
	FName StoryId;
	FName MainlineEnd;
	FText Title;
	FText Summary;
	int32 StageNumber = 1;
	TArray<FName> Nodes;
};

class GAMEXXK_API FGameXXKMainStoryCatalog final
{
public:
	static const TArray<FGameXXKMainStoryChapter>& Chapters();
	static const TArray<FGameXXKMainStoryNode>& Nodes();
	static const FGameXXKMainStoryNode* FindNode(FName Id);
	static const FGameXXKMainStoryChapter* FindChapter(FName Id);
	static FText CharacterName(FName Id);
	static FName StageId(const FGameXXKMainStoryChapter& Chapter);
	static bool Validate(FString* OutError = nullptr);
};
