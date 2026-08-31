#include "UI/GameXXKInventoryItemPresentation.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialMapInventoryUiTest,
	"GameXXK.Prologue.Aftermath.TutorialMapInventoryUi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialMapInventoryUiTest::RunTest(const FString& Parameters)
{
	const FName MapItemId = UGameXXKMVPRules::ItemTutorialRiverMap();
	TestEqual(TEXT("map icon path"),
		FGameXXKInventoryItemPresentation::ResolveIconPath(MapItemId),
		FString(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap.T_Relic_OldMap")));
	TestTrue(TEXT("map is inspectable"),
		FGameXXKInventoryItemPresentation::IsInspectable(MapItemId));
	TestEqual(TEXT("map inspection path"),
		FGameXXKInventoryItemPresentation::InspectTexturePath(MapItemId),
		FString(TEXT("/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.T_Tutorial_XuXiakeTravelRouteInspect")));
	TestFalse(TEXT("ordinary item is not inspectable"),
		FGameXXKInventoryItemPresentation::IsInspectable(
			UGameXXKMVPRules::ItemEnhancementStone()));

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("inventory UI fixture starts"),
		Subsystem && Subsystem->StartGame() && Workbench))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Inventory.Add(MapItemId, 1);
	TestTrue(TEXT("map physical entry normalizes"),
		Subsystem->NormalizeDesktopInventoryState());
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	TestTrue(TEXT("workbench opens"),
		Workbench->OpenWorkbench() && Workbench->OpenBackpack());

	int32 InspectRequests = 0;
	Workbench->SetTutorialMapInspectionRequestedForTest(
		FGameXXKTutorialMapInspectionRequested::CreateLambda(
			[&InspectRequests]()
			{
				++InspectRequests;
				return true;
			}));
	const int32 BackpackSlot = Workbench->FindBackpackItemSlotForTest(MapItemId);
	TestTrue(TEXT("map is visible in backpack"), BackpackSlot != INDEX_NONE);
	TestTrue(TEXT("backpack right-click opens inspection"),
		Workbench->RightClickBackpackSlotForTest(BackpackSlot));
	TestEqual(TEXT("one backpack inspection request"), InspectRequests, 1);
	TestEqual(TEXT("right-click keeps map in backpack"),
		Workbench->FindBackpackItemSlotForTest(MapItemId),
		BackpackSlot);
	TestFalse(TEXT("right-click does not start drag"),
		Workbench->IsCarryingItemForTest());

	TestTrue(TEXT("left click still picks up map"),
		Workbench->PickUpBackpackSlotForTest(BackpackSlot));
	TestFalse(TEXT("task map cannot enter tool slot"),
		Workbench->DropCarriedOnToolSlotForTest(0));
	TestTrue(TEXT("illegal tool drop keeps carry transaction"),
		Workbench->IsCarryingItemForTest());
	TestTrue(TEXT("carry can return to backpack"),
		Workbench->DropCarriedOnBackpackSlotForTest(BackpackSlot));

	const FGameXXKDesktopInventoryEntryKey MapEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(MapItemId);
	const int32 WarehouseSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Warehouse);
	FString Error;
	TestTrue(TEXT("map can move to warehouse through existing transaction"),
		Subsystem->MoveDesktopInventoryEntry(
			EGameXXKDesktopItemContainer::Backpack,
			BackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			WarehouseSlot,
			&Error));
	TestTrue(TEXT("warehouse right-click opens inspection"),
		Workbench->RightClickWarehouseSlotForTest(WarehouseSlot));
	TestEqual(TEXT("one warehouse inspection request"), InspectRequests, 2);
	TestEqual(TEXT("inspection leaves warehouse slot unchanged"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			WarehouseSlot),
		MapEntry);

	return true;
}

#endif
