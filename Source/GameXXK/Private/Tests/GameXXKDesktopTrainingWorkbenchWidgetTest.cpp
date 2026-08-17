#include "GameXXKTrainingRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLayoutContractTest,
	"GameXXK.DesktopTraining.Workbench.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLayoutContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget can be constructed without a live viewport"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("warehouse uses four columns"), Widget->GetWarehouseColumnCountForTest(), 4);
	const FVector2D BackpackRatio = Widget->GetBackpackAspectRatioForTest();
	TestTrue(TEXT("backpack aspect ratio keeps the real wide proportion"), FMath::IsNearlyEqual(BackpackRatio.X / BackpackRatio.Y, 1.76f, 0.001f));

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("workbench read model fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.PlayerGold = 4242;
	State.Inventory.Empty();
	State.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 3);
	State.Inventory.Add(UGameXXKMVPRules::ItemTrainingNormalChest(), 2);
	for (int32 ExtraIndex = 0; ExtraIndex < 15; ++ExtraIndex)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::Starter;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1 + ExtraIndex;
		FName InstanceId;
		FString Error;
		TestTrue(TEXT("warehouse pagination fixture creates an equipment instance"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error));
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("Tab/backpack entry opens the formation-backed backpack view"), Widget->OpenBackpack());
	TestEqual(TEXT("backpack defaults to the hero character"),
		Widget->GetActiveBackpackCharacterIdForTest(),
		FGameXXKEquipmentRules::HeroCharacterId());
	const TArray<FName> BackpackCharacterIds = Widget->GetBackpackCharacterIdsForTest();
	TestEqual(TEXT("backpack exposes the hero and both starter companions"), BackpackCharacterIds.Num(), 3);
	TestTrue(TEXT("backpack character list keeps the hero first"),
		BackpackCharacterIds.Num() > 0
		&& BackpackCharacterIds[0] == FGameXXKEquipmentRules::HeroCharacterId());
	if (BackpackCharacterIds.Num() > 1)
	{
		TestTrue(TEXT("backpack can switch to a permanent companion"), Widget->SelectBackpackCharacterForTest(BackpackCharacterIds[1]));
		TestEqual(TEXT("selected companion becomes the backpack read-model owner"),
			Widget->GetActiveBackpackCharacterIdForTest(),
			BackpackCharacterIds[1]);
	}
	TestFalse(TEXT("backpack rejects an unknown character"), Widget->SelectBackpackCharacterForTest(FName(TEXT("Character.Unknown"))));
	Widget->HandleActionClicked(3);
	TestEqual(TEXT("tools navigation replaces the right-side map"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Tools);
	TestTrue(TEXT("tools panel is active outside challenge viewport"), Widget->IsToolsPanelActiveForTest());
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("training navigation returns to the map shell"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Training);
	TestFalse(TEXT("training navigation is not the tools panel"), Widget->IsToolsPanelActiveForTest());
	TestEqual(TEXT("warehouse exposes two pages at twenty slots per page"), Widget->GetWarehousePageCountForTest(), 2);
	TestEqual(TEXT("warehouse starts on its first page"), Widget->GetWarehousePageIndexForTest(), 0);
	TestEqual(TEXT("warehouse first page exposes twenty visible instances"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 20);
	TestTrue(TEXT("warehouse advances to the second page"), Widget->NextWarehousePageForTest());
	TestEqual(TEXT("warehouse page index advances without mutating the save"), Widget->GetWarehousePageIndexForTest(), 1);
	TestEqual(TEXT("warehouse second page exposes the remaining instance"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 1);
	TestTrue(TEXT("warehouse returns to the first page"), Widget->PreviousWarehousePageForTest());
	TestEqual(TEXT("warehouse page index clamps at zero"), Widget->GetWarehousePageIndexForTest(), 0);
	const int32 WarehouseBeforeEquip = State.EquipmentCollection.WarehouseInstanceIds.Num();
	TestTrue(TEXT("warehouse slot can quick-equip into the selected backpack character"), Widget->QuickEquipVisibleWarehouseSlotForTest(0));
	TestEqual(TEXT("quick-equip removes the moved instance from warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Num(), WarehouseBeforeEquip - 1);
	TestEqual(TEXT("workbench reads the authoritative runtime gold"), Widget->GetRuntimeGoldForTest(), 4242);
	TestEqual(TEXT("workbench warehouse occupancy comes from the equipment collection"),
		Widget->GetWarehouseOccupancyForTest(),
		State.EquipmentCollection.WarehouseInstanceIds.Num());
	const TArray<FName> VisibleItems = Widget->GetVisibleBackpackItemIdsForTest();
	TestTrue(TEXT("workbench backpack read model includes healing powder"), VisibleItems.Contains(UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("workbench backpack read model includes a travel chest"), VisibleItems.Contains(UGameXXKMVPRules::ItemTrainingNormalChest()));
	TestEqual(TEXT("three difficulty bands each expose nine stage definitions"), FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestEqual(TEXT("normal 1-1 id remains stable"), FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), FName(TEXT("Training.Normal.1-1")));
	return true;
}

#endif
