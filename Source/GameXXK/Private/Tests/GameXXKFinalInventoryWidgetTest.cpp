#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FName CreateInventoryWidgetEquipment(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::XuanJia;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 4;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(
			TEXT("final inventory fixture creates an instance-based equipment item"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalInventoryWidgetTest,
	"GameXXK.MVP.UI.FinalInventory.SixSlotsAndRightClickActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	const FName FirstWeapon = CreateInventoryWidgetEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);
	const FName ReplacementWeapon = CreateInventoryWidgetEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);

	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->TakeWidget();
	TestTrue(TEXT("final hero inventory opens"), Inventory->OpenFreeInventoryForTest());
	TestEqual(TEXT("final hero inventory exposes six square equipment slots"), Inventory->GetEquipmentSlotCountForTest(), 6);
	TestEqual(TEXT("final hero inventory exposes twenty backpack cells"), Inventory->GetBackpackSlotCountForTest(), 20);
	TestEqual(
		TEXT("final hero inventory scrolls through the full two-hundred-slot storage"),
		Inventory->GetBackpackStorageCapacityForTest(),
		FGameXXKEquipmentRules::WarehouseCapacity);
	TestTrue(TEXT("final hero inventory owns a real scroll box"), Inventory->HasBackpackScrollBoxForTest());
	TestEqual(TEXT("final hero inventory uses four backpack columns"), Inventory->GetBackpackColumnCountForTest(), 4);
	TestTrue(
		TEXT("final hero inventory uses the approved ink close button"),
		Inventory->GetCloseButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk")));
	TestTrue(
		TEXT("final hero inventory uses the approved right scrollbar"),
		Inventory->GetScrollbarResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight")));
	TestTrue(
		TEXT("final hero inventory reuses the approved shared selection ink"),
		Inventory->GetSelectionInkResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SelectionInk")));
	TestTrue(
		TEXT("final hero inventory composes tooltips from the approved item-slot paper"),
		Inventory->GetTooltipResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot")));

	const TArray<FName> WarehouseEntries = Inventory->GetVisibleBackpackEquipmentInstanceIdsForTest();
	TestTrue(TEXT("warehouse equipment appears in the scrollable backpack"), WarehouseEntries.Contains(FirstWeapon));
	TestTrue(TEXT("replacement equipment also appears in the scrollable backpack"), WarehouseEntries.Contains(ReplacementWeapon));
	const int32 FirstWeaponSlot = Inventory->FindBackpackEquipmentInstanceSlotForTest(FirstWeapon);
	TestTrue(TEXT("warehouse equipment resolves to a clickable backpack cell"), FirstWeaponSlot >= 0);
	Inventory->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::PlayerBackpack, FirstWeaponSlot, NAME_None);
	TestTrue(
		TEXT("clicking an instance equipment cell exposes its composed tooltip detail"),
		Inventory->GetSelectedDetailTextForTest().ToString().Contains(TEXT("装备等级 4")));

	TestTrue(
		TEXT("right-clicking a warehouse equipment cell equips it"),
		Inventory->HandleConfiguredSlotRightClicked(
			EGameXXKInventorySlotSource::PlayerBackpack,
			FirstWeaponSlot,
			NAME_None));
	TestEqual(
		TEXT("first quick-equip fills the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon),
		FirstWeapon);
	TestTrue(TEXT("right-click action replaces occupied equipment"), Inventory->QuickEquipBackpackInstanceForTest(ReplacementWeapon));
	TestEqual(
		TEXT("replacement instance becomes visible in the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon),
		ReplacementWeapon);
	TestTrue(TEXT("replaced instance returns to the shared warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(FirstWeapon));
	TestTrue(TEXT("right-click equipment-slot action unequips"), Inventory->QuickUnequipSlotForTest(EGameXXKEquipmentSlot::Weapon));
	TestTrue(
		TEXT("right-click unequip clears the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon).IsNone());
	TestTrue(TEXT("unequipped replacement returns to the shared warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(ReplacementWeapon));
	return true;
}

#endif
