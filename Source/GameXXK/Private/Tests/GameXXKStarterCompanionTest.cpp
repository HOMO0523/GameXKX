#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 CountActivePermanentCompanions(const FGameXXKCompanionRosterState& Roster)
	{
		int32 ActiveCount = 0;
		for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
		{
			if (Companion.bIsActive)
			{
				++ActiveCount;
			}
		}
		return ActiveCount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStarterCompanionTest,
	"GameXXK.MVP.StarterCompanion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStarterCompanionTest::RunTest(const FString& Parameters)
{
	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("starter companion subsystem exists"), Subsystem);
	if (!Subsystem || !TestTrue(TEXT("StartNewGame succeeds with a starter companion"), Subsystem->StartNewGame()))
	{
		return false;
	}

	const FGameXXKRuntimeState StartedState = Subsystem->GetRuntimeStateCopy();
	const FGameXXKCompanionRosterState& StartedRoster = StartedState.CardRun.CompanionRoster;
	TestEqual(TEXT("a new game owns exactly one permanent companion"), StartedRoster.PermanentCompanions.Num(), 1);
	TestEqual(TEXT("a new game exposes exactly one active permanent companion"), CountActivePermanentCompanions(StartedRoster), 1);
	TestTrue(TEXT("a new game persists a non-zero recruit sequence seed"), StartedRoster.RecruitSequenceSeed != 0);
	TestNotEqual(TEXT("a new game never uses the reserved minimum recruit sequence seed"), StartedRoster.RecruitSequenceSeed, MIN_int32);
	if (StartedRoster.PermanentCompanions.Num() != 1)
	{
		return false;
	}

	const FGameXXKPermanentCompanion& StarterCompanion = StartedRoster.PermanentCompanions[0];
	TestFalse(TEXT("the starter companion has a stable instance id"), StarterCompanion.InstanceId.IsNone());
	TestTrue(TEXT("the only starter companion is active"), StarterCompanion.bIsActive);
	TestEqual(
		TEXT("party selection points at the active starter companion"),
		StartedState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		StarterCompanion.InstanceId);
	TestEqual(TEXT("the starter companion owns twelve personal cards"), StarterCompanion.PersonalCardIds.Num(), 12);
	TestEqual(TEXT("the starter companion equips five selected cards"), StarterCompanion.SelectedCardIds.Num(), 5);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(StartedState);
	FGameXXKRuntimeState RestoredState;
	FGameXXKSaveMigrationReport RestoreReport;
	if (!TestTrue(
		TEXT("starter-companion save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, RestoredState, RestoreReport)))
	{
		return false;
	}
	const FGameXXKCompanionRosterState& RestoredRoster = RestoredState.CardRun.CompanionRoster;
	TestEqual(TEXT("save restore keeps the one starter companion"), RestoredRoster.PermanentCompanions.Num(), 1);
	TestEqual(TEXT("save restore keeps the recruit sequence seed"), RestoredRoster.RecruitSequenceSeed, StartedRoster.RecruitSequenceSeed);
	TestEqual(
		TEXT("save restore keeps the active party selection"),
		RestoredState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		StarterCompanion.InstanceId);
	if (RestoredRoster.PermanentCompanions.Num() != 1)
	{
		return false;
	}

	const FGameXXKPermanentCompanion& RestoredStarter = RestoredRoster.PermanentCompanions[0];
	TestEqual(TEXT("save restore keeps the starter instance id"), RestoredStarter.InstanceId, StarterCompanion.InstanceId);
	TestEqual(TEXT("save restore keeps the starter personal cards"), RestoredStarter.PersonalCardIds, StarterCompanion.PersonalCardIds);
	TestEqual(TEXT("save restore keeps the starter selected cards"), RestoredStarter.SelectedCardIds, StarterCompanion.SelectedCardIds);
	TestEqual(TEXT("save restore keeps the starter card seed"), RestoredStarter.CardSeed, StarterCompanion.CardSeed);
	TestEqual(TEXT("save restore keeps the starter active flag"), RestoredStarter.bIsActive, StarterCompanion.bIsActive);
	return true;
}

#endif
