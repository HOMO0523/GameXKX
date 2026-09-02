#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialMapItemTest,
	"GameXXK.Prologue.Aftermath.TutorialMapItem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialMapItemTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("tutorial map item owns v31 boundary"),
		FGameXXKSaveMigration::TutorialMapItemIntroducedSaveVersion,
		31);
	TestEqual(TEXT("current schema advances to active-card-pool v34"),
		FGameXXKSaveMigration::CurrentSaveVersion,
		34);

	bool bFound = false;
	const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(
		UGameXXKMVPRules::ItemTutorialRiverMap(),
		bFound);
	TestTrue(TEXT("map item is catalogued"), bFound);
	TestEqual(TEXT("approved inventory name"),
		Definition.DisplayName,
		FText::FromString(TEXT("徐霞客游历路线")));
	TestEqual(TEXT("map is a task item"),
		Definition.Kind,
		EGameXXKItemKind::Task);

	FGameXXKRuntimeState SellFixture = UGameXXKMVPRules::CreateNewGame();
	SellFixture.Inventory.Add(UGameXXKMVPRules::ItemTutorialRiverMap(), 1);
	TestFalse(TEXT("map cannot be sold"),
		UGameXXKMVPRules::CanSellItem(
			SellFixture,
			UGameXXKMVPRules::ItemTutorialRiverMap()));
	TestFalse(TEXT("map cannot be used"),
		UGameXXKMVPRules::UseItem(
			SellFixture,
			UGameXXKMVPRules::ItemTutorialRiverMap()));

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("grant fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	int32 SaveCount = 0;
	Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&SaveCount](USaveGame* SaveGame, const FString& SlotName, const int32 UserIndex)
		{
			++SaveCount;
			return SaveGame != nullptr;
		}));

	FString Error;
	TestTrue(FString::Printf(TEXT("first map grant succeeds: %s"), *Error),
		Subsystem->GrantTutorialRiverMap(&Error));
	TestTrue(TEXT("map is owned after grant"), Subsystem->OwnsTutorialRiverMap());
	TestEqual(TEXT("map saved once"), SaveCount, 1);
	const FGameXXKDesktopInventoryEntryKey MapEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(
			UGameXXKMVPRules::ItemTutorialRiverMap());
	const int32 BackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		MapEntry);
	TestTrue(TEXT("first grant enters backpack"), BackpackSlot != INDEX_NONE);

	const int32 WarehouseSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Warehouse);
	TestTrue(TEXT("map can move to warehouse"),
		Subsystem->MoveDesktopInventoryEntry(
			EGameXXKDesktopItemContainer::Backpack,
			BackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			WarehouseSlot,
			&Error));
	TestTrue(TEXT("warehouse map still counts as owned"), Subsystem->OwnsTutorialRiverMap());
	TestTrue(TEXT("repeat grant succeeds idempotently"),
		Subsystem->GrantTutorialRiverMap(&Error));
	TestEqual(TEXT("repeat grant performs no second save"), SaveCount, 1);
	TestEqual(TEXT("map remains in warehouse after replay"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			MapEntry),
		WarehouseSlot);

	UGameXXKMVPSubsystem* InvalidFormationSubsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("invalid-formation grant fixture starts"),
		InvalidFormationSubsystem && InvalidFormationSubsystem->StartGame()))
	{
		return false;
	}
	InvalidFormationSubsystem->GetMutableRuntimeState()
		.CardRun.OrderedFormation.Members.Reset();
	int32 InvalidFormationSaveCount = 0;
	InvalidFormationSubsystem->SetSaveSlotWriteDelegateForTest(
		FGameXXKSaveSlotWriteDelegate::CreateLambda(
			[&InvalidFormationSaveCount](
				USaveGame* SaveGame,
				const FString& SlotName,
				const int32 UserIndex)
			{
				++InvalidFormationSaveCount;
				return SaveGame != nullptr;
			}));
	Error.Reset();
	TestTrue(
		FString::Printf(
			TEXT("unrelated invalid formation cannot block the map story item: %s"),
			*Error),
		InvalidFormationSubsystem->GrantTutorialRiverMap(&Error));
	TestTrue(TEXT("failed persistence still leaves the map in the live backpack"),
		InvalidFormationSubsystem->OwnsTutorialRiverMap());
	TestEqual(TEXT("story grant never rewrites the unrelated formation"),
		InvalidFormationSubsystem->GetRuntimeState()
			.CardRun.OrderedFormation.Members.Num(),
		0);
	TestEqual(TEXT("invalid full-state save never reaches the slot writer"),
		InvalidFormationSaveCount,
		0);
	InvalidFormationSubsystem->ResetSaveSlotWriteDelegateForTest();

	UGameXXKMVPSubsystem* MigrationSubsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("migration fixture starts with a legal v30 formation"),
		MigrationSubsystem && MigrationSubsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState V30Runtime = MigrationSubsystem->GetRuntimeStateCopy();
	V30Runtime.Inventory.Remove(UGameXXKMVPRules::ItemTutorialRiverMap());
	V30Runtime.DesktopInventory.WarehouseItems.Remove(
		UGameXXKMVPRules::ItemTutorialRiverMap());
	V30Runtime.DesktopInventory.PendingTaskItemIds.Reset();
	FGameXXKDesktopInventoryRules::Normalize(V30Runtime, nullptr);
	FGameXXKSaveState V30 = UGameXXKMVPRules::MakeSaveState(V30Runtime);
	V30.SaveVersion = 30;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(FString::Printf(TEXT("v30 migrates through v31-v33 to v34: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(V30, Migrated, Report));
	TestEqual(TEXT("migration reaches v34"), Migrated.SaveVersion, 34);
	TestTrue(TEXT("old saves start with no pending task item"),
		Migrated.RuntimeState.DesktopInventory.PendingTaskItemIds.IsEmpty());

	Subsystem->ResetSaveSlotWriteDelegateForTest();
	return true;
}

#endif
