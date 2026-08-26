#include "Misc/AutomationTest.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKTalentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentLogicalCapacityIntegrationTest,
	"GameXXK.Talents.Capacity.LogicalBackpackAndWarehouseGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentLogicalCapacityIntegrationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("capacity fixture starts a complete game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	TestEqual(TEXT("new game Backpack starts at twenty logical slots"),
		FGameXXKTalentRules::GetUnlockedBackpackCapacity(State), 20);
	TestEqual(TEXT("new game Warehouse starts at one page"),
		FGameXXKTalentRules::GetUnlockedWarehousePageCount(State), 1);

	int32 SourceSlot = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < State.DesktopInventory.BackpackSlots.Num(); ++SlotIndex)
	{
		if (State.DesktopInventory.BackpackSlots[SlotIndex].IsValid())
		{
			SourceSlot = SlotIndex;
			break;
		}
	}
	if (!TestTrue(TEXT("capacity fixture has one movable Backpack entry"), SourceSlot != INDEX_NONE))
	{
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey Entry = State.DesktopInventory.BackpackSlots[SourceSlot];
	auto Move = [&State, &Entry](
		const EGameXXKDesktopItemContainer FromContainer,
		const int32 FromSlot,
		const EGameXXKDesktopItemContainer ToContainer,
		const int32 ToSlot,
		FString& OutError)
	{
		FGameXXKDesktopInventoryMoveRequest Request;
		Request.FromContainer = FromContainer;
		Request.FromSlotIndex = FromSlot;
		Request.ToContainer = ToContainer;
		Request.ToSlotIndex = ToSlot;
		Request.ExpectedEntry = Entry;
		return FGameXXKDesktopInventoryRules::MoveOrSwap(State, Request, &OutError);
	};

	FString Error;
	TestFalse(TEXT("slot twenty-one is locked before capacity talents"),
		Move(EGameXXKDesktopItemContainer::Backpack, SourceSlot,
			EGameXXKDesktopItemContainer::Backpack, 20, Error));
	TestFalse(TEXT("Warehouse page two is locked before the root"),
		Move(EGameXXKDesktopItemContainer::Backpack, SourceSlot,
			EGameXXKDesktopItemContainer::Warehouse, 36, Error));

	State.PlayerGold = 2500;
	FGameXXKTalentPurchaseResult Purchase;
	TestTrue(TEXT("capacity fixture purchases root"),
		FGameXXKTalentRules::Purchase(State, TEXT("Talent.Root"), Purchase));
	TestEqual(TEXT("root unlocks Warehouse page two"),
		FGameXXKTalentRules::GetUnlockedWarehousePageCount(State), 2);
	TestTrue(TEXT("root permits an exact page-two Warehouse cell"),
		Move(EGameXXKDesktopItemContainer::Backpack, SourceSlot,
			EGameXXKDesktopItemContainer::Warehouse, 36, Error));

	State.PlayerGold = 2500;
	TestTrue(TEXT("capacity fixture purchases capacity entry"),
		FGameXXKTalentRules::Purchase(State, TEXT("Talent.Entry.CapacityChest"), Purchase));
	TestEqual(TEXT("capacity entry raises Backpack to twenty-five"),
		FGameXXKTalentRules::GetUnlockedBackpackCapacity(State), 25);
	TestTrue(TEXT("capacity entry permits the twenty-fifth Backpack cell"),
		Move(EGameXXKDesktopItemContainer::Warehouse, 36,
			EGameXXKDesktopItemContainer::Backpack, 24, Error));
	TestFalse(TEXT("capacity entry still locks the twenty-sixth Backpack cell"),
		Move(EGameXXKDesktopItemContainer::Backpack, 24,
			EGameXXKDesktopItemContainer::Backpack, 25, Error));

	State.PlayerGold = 3400 * 5;
	for (int32 Rank = 0; Rank < 5; ++Rank)
	{
		TestTrue(TEXT("first capacity node purchases one fine-grained slot rank"),
			FGameXXKTalentRules::Purchase(State, TEXT("Talent.Capacity.Backpack.01"), Purchase));
	}
	TestEqual(TEXT("five fine-grained ranks raise Backpack to thirty"),
		FGameXXKTalentRules::GetUnlockedBackpackCapacity(State), 30);
	TestTrue(TEXT("ranked capacity permits the thirtieth Backpack cell"),
		Move(EGameXXKDesktopItemContainer::Backpack, 24,
			EGameXXKDesktopItemContainer::Backpack, 29, Error));
	return true;
}

#endif
