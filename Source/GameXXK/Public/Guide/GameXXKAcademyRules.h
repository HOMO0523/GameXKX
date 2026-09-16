#pragma once
#include "CoreMinimal.h"
#include "Guide/GameXXKGuideAsset.h"
#include "GameXXKCardTypes.h"
struct FGameXXKRuntimeState;

enum class EGameXXKAcademyGoal : uint8
{
	ActiveCards, EndRound, Damage, Armor, Healing, Cleanse, Medicine, Formula,
	Reaction, ArmorDamage, Charge, HeavyArrow, TerrainChange, TerrainBenefit,
	SpellTask, BladeOpening, BladeFinish, ToxicExplosion
};

struct GAMEXXK_API FGameXXKAcademyGoalDefinition
{
	EGameXXKAcademyGoal Kind = EGameXXKAcademyGoal::ActiveCards;
	int32 Required = 1;
	FText Text;
};

struct GAMEXXK_API FGameXXKAcademyLesson
{
	FText Title;
	FText Instruction;
	TArray<FName> Cards;
	TArray<FGameXXKAcademyGoalDefinition> Goals;
};

struct GAMEXXK_API FGameXXKAcademyCourse
{
	FName Id;
	FText Title;
	FText Summary;
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Hero;
	FName NpcId;
	TArray<FGameXXKAcademyLesson> Lessons;
};

struct GAMEXXK_API FGameXXKAcademyEvidence
{
	TMap<EGameXXKAcademyGoal,int32> Counts;
	TSet<FName> ActiveCardIds;
	void Record(EGameXXKAcademyGoal Goal,int32 Count=1) { Counts.FindOrAdd(Goal)+=Count; }
	bool Satisfies(const FGameXXKAcademyLesson& Lesson) const;
};

/** Why a course cannot start yet. Ordered by the first blocking reason. */
enum class EGameXXKAcademyBlock : uint8
{
	None = 0,
	CompanionSlotLocked,
	CompanionNotRecruited,
	NpcSlotLocked,
	NpcNotOwned,
	ActiveBattle,
	CourseMissing
};

struct GAMEXXK_API FGameXXKAcademyEligibility
{
	bool bAvailable = false;
	EGameXXKAcademyBlock Block = EGameXXKAcademyBlock::None;
	/** Player-facing reason; empty when the course is available. */
	FText Reason;
};

class GAMEXXK_API FGameXXKAcademyRules
{
public:
	static constexpr int32 FirstClearGold = 100000;
	static const TArray<FGameXXKAcademyCourse>& Courses();
	static const FGameXXKAcademyCourse* Find(FName Id);
	/**
	 * Whether the player can start this course right now, and if not, which
	 * prerequisite is missing. Mirrors the loadout the course battle builds, so a
	 * course reported unavailable is one BuildLoadout would refuse anyway.
	 */
	static FGameXXKAcademyEligibility EvaluateEligibility(const FGameXXKRuntimeState& State,const FGameXXKAcademyCourse& Course);
	/** Convenience overload that also reports a course id that does not exist. */
	static FGameXXKAcademyEligibility EvaluateEligibility(const FGameXXKRuntimeState& State,FName CourseId);
    /** Same one-step planner used by the visible tutorial and its playable acceptance. */
    static void Recommend(const FGameXXKRuntimeState& State,FName FocusUnitId,const FGameXXKAcademyLesson& Lesson,
        const FGameXXKAcademyEvidence& Evidence,FName RestrictedCard,FName& Card,FName& Target,bool& EndTurn);
	static void ObserveCommittedResult(const FGameXXKCardPlayResult& Result,FName FocusUnitId,FGameXXKAcademyEvidence& Evidence);
	static void Observe(const FGameXXKCardBattleRuntime& Before,const FGameXXKCardBattleRuntime& After,
		const TArray<FGameXXKCardDamageResult>& Damage,FName PlayedInstance,FName FocusUnitId,FGameXXKAcademyEvidence& Evidence);
	/** Atomic pure candidate update; callers persist it before publishing it to the player. */
	static bool CompleteLesson(FName CourseId,int32 LessonIndex,bool bWon,const FGameXXKAcademyEvidence& Evidence,
		FGameXXKGuideProgress& Progress,int32& Gold,int32& Award,FString& Error);
	static bool ValidateProgress(const FGameXXKGuideProgress& Progress,FString& Error);
};
