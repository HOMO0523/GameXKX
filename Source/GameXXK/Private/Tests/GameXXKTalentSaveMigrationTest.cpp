#include "Misc/AutomationTest.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKTalentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentSaveRoundTripTest,
	"GameXXK.Talents.SaveMigration.V26RoundTripAndLegacyCapacityFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentSaveRoundTripTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("permanent talents own save version twenty-six"),
		FGameXXKSaveMigration::PermanentTalentGraphIntroducedSaveVersion, 26);
	TestEqual(TEXT("the active card pool advances the current save schema to thirty-four"),
		FGameXXKSaveMigration::CurrentSaveVersion, 34);

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("talent save fixture starts a complete game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Combat"), 1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Combat.FlatAttack.01"), 3);
	const FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
	TestEqual(TEXT("new save writes the current schema"),
		Save.SaveVersion,
		FGameXXKSaveMigration::CurrentSaveVersion);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(FString::Printf(TEXT("v28 talent save restores: %s"), *Report.Error),
		FGameXXKSaveMigration::TryRestoreRuntimeState(Save, Restored, Report));
	TestEqual(TEXT("root rank persists"), Restored.Talents.NodeRanks.FindRef(TEXT("Talent.Root")), 1);
	TestEqual(TEXT("repeatable rank persists"),
		Restored.Talents.NodeRanks.FindRef(TEXT("Talent.Combat.FlatAttack.01")), 3);

	FGameXXKRuntimeState LegacyState = Subsystem->GetRuntimeStateCopy();
	LegacyState.Talents = FGameXXKTalentProgress();
	int32 FirstOccupied = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < LegacyState.DesktopInventory.BackpackSlots.Num(); ++SlotIndex)
	{
		if (LegacyState.DesktopInventory.BackpackSlots[SlotIndex].IsValid())
		{
			FirstOccupied = SlotIndex;
			break;
		}
	}
	if (!TestTrue(TEXT("legacy capacity fixture has one physical Backpack entry"), FirstOccupied != INDEX_NONE))
	{
		return false;
	}
	// A v25 save could legally contain entries anywhere in the old 200-cell
	// physical array. Construct that legacy payload directly instead of routing
	// it through the v26 logical-capacity guard that this migration is testing.
	const FGameXXKDesktopInventoryEntryKey LegacyHighEntry =
		LegacyState.DesktopInventory.BackpackSlots[FirstOccupied];
	LegacyState.DesktopInventory.BackpackSlots[FirstOccupied] = FGameXXKDesktopInventoryEntryKey();
	LegacyState.DesktopInventory.BackpackSlots[79] = LegacyHighEntry;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyState);
	LegacySave.SaveVersion = 25;
	LegacySave.RuntimeState.Talents = FGameXXKTalentProgress();
	FGameXXKRuntimeState Migrated;
	FGameXXKSaveMigrationReport LegacyReport;
	TestTrue(FString::Printf(TEXT("v25 save migrates without losing high occupied cells: %s"), *LegacyReport.Error),
		FGameXXKSaveMigration::TryRestoreRuntimeState(LegacySave, Migrated, LegacyReport));
	TestTrue(TEXT("legacy migration unlocks at least slot 80"),
		Migrated.Talents.MinimumBackpackCapacity >= 80);
	TestTrue(TEXT("legacy migration keeps the exact high physical entry"),
		Migrated.DesktopInventory.BackpackSlots.IsValidIndex(79)
			&& Migrated.DesktopInventory.BackpackSlots[79] == LegacyHighEntry);
	TestTrue(TEXT("legacy migration does not fabricate purchased talent ranks"),
		Migrated.Talents.NodeRanks.IsEmpty());
	return true;
}

#endif
