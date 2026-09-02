#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKStoryCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRetiredLegacyNarrativeTest,
	"GameXXK.Prologue.RetiredLegacyNarrative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRetiredLegacyNarrativeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("legacy narrative retirement owns v32 boundary"),
		FGameXXKSaveMigration::RetiredLegacyTutorialNarrativeSaveVersion,
		32);
	TestEqual(TEXT("combat scaling is current save boundary"),
		FGameXXKSaveMigration::CurrentSaveVersion,
		33);
	TestNull(TEXT("retired Xu Xiake story is absent"),
		FGameXXKStoryCatalog::FindStory(TEXT("Story.Main.XuXiakeTreasure")));
	TestNull(TEXT("retired Xu Xiake task is absent"),
		FGameXXKStoryCatalog::FindTask(TEXT("Task.Main.XuXiake.Prologue")));

	UGameXXKMVPSubsystem* Fixture =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("migration fixture starts"), Fixture && Fixture->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState Runtime = Fixture->GetRuntimeStateCopy();
	Runtime.Training.PendingTravelGold = 456;
	Runtime.GuideProgress.Preference = EGameXXKGuidePreference::ExperiencedPlayer;

	const FName StoryId(TEXT("Story.Main.XuXiakeTreasure"));
	const FName TaskId(TEXT("Task.Main.XuXiake.Prologue"));
	FGameXXKStoryProgress Story;
	Story.Version = 1;
	Story.State = EGameXXKStoryState::Active;
	Story.ActiveTaskIds.Add(TaskId);
	Runtime.NarrativeProgress.StoryProgressById.Add(StoryId, Story);
	FGameXXKTaskProgress Task;
	Task.State = EGameXXKTaskState::Active;
	Task.CurrentStepId = TEXT("Step.Main.XuXiake.RiverScroll");
	Runtime.NarrativeProgress.TaskProgressById.Add(TaskId, Task);
	Runtime.NarrativeProgress.TrackedTaskId = TaskId;
	Runtime.NarrativeSequenceSession.bActive = true;
	Runtime.NarrativeSequenceSession.StoryId = StoryId;
	Runtime.NarrativeSequenceSession.TaskId = TaskId;
	Runtime.NarrativeSequenceSession.StepId = Task.CurrentStepId;
	Runtime.NarrativeSequenceSession.SequenceId = TEXT("Sequence.Main.XuXiake.CarriageArrival");
	Runtime.NarrativeSequenceSession.StageContractId = TEXT("Stage.Tutorial.River");

	FGameXXKSaveState V31 = UGameXXKMVPRules::MakeSaveState(Runtime);
	V31.SaveVersion = 31;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(FString::Printf(TEXT("v31 retires legacy narrative: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(V31, Migrated, Report));
	TestEqual(TEXT("migration reaches current v34"), Migrated.SaveVersion, 34);
	TestFalse(TEXT("retired story progress is removed"),
		Migrated.RuntimeState.NarrativeProgress.StoryProgressById.Contains(StoryId));
	TestFalse(TEXT("retired task progress is removed"),
		Migrated.RuntimeState.NarrativeProgress.TaskProgressById.Contains(TaskId));
	TestTrue(TEXT("retired task is no longer tracked"),
		Migrated.RuntimeState.NarrativeProgress.TrackedTaskId.IsNone());
	TestFalse(TEXT("retired sequence session is cleared"),
		Migrated.RuntimeState.NarrativeSequenceSession.bActive);
	TestEqual(TEXT("unrelated training state survives"),
		Migrated.RuntimeState.Training.PendingTravelGold,
		456);
	TestEqual(TEXT("unrelated guide preference survives"),
		Migrated.RuntimeState.GuideProgress.Preference,
		EGameXXKGuidePreference::ExperiencedPlayer);
	return true;
}

#endif
