#include "GameXXKTravelMoneyRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelMoneyWalletTest,
	"GameXXK.RouteTravelMoney.PhysicalWallet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTravelMoneyWalletTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.PlayerGold = 1000000;
	State.CardRun.RouteTravelMoney = 10000;
	TestEqual(TEXT("ordinary gold and retired route counters cannot fund the merchant"), FGameXXKTravelMoneyRules::GetBalance(State), int64(0));
	TestFalse(TEXT("rich permanent-gold balance cannot substitute for travel money"), FGameXXKTravelMoneyRules::CanAfford(State, 1));
	State.Inventory.Add(FGameXXKTravelMoneyRules::ItemId(), 3);
	State.DesktopInventory.WarehouseItems.Add(FGameXXKTravelMoneyRules::ItemId(), 10);
	FString Error;
	TestTrue(TEXT("currency stacks normalize into both existing containers"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	TestEqual(TEXT("both accessible containers fund the same wallet"), FGameXXKTravelMoneyRules::GetBalance(State), int64(13));
	TestFalse(TEXT("failed payment is rejected"), FGameXXKTravelMoneyRules::Spend(State, 14, &Error));
	TestEqual(TEXT("failed payment leaves both stacks intact"), FGameXXKTravelMoneyRules::GetBalance(State), int64(13));
	TestTrue(TEXT("small payment works while both containers retain stacks"), FGameXXKTravelMoneyRules::Spend(State, 1, &Error));
	TestEqual(TEXT("small payment preserves warehouse"), State.DesktopInventory.WarehouseItems.FindRef(FGameXXKTravelMoneyRules::ItemId()), 10);
	TestTrue(TEXT("payment spans backpack then warehouse"), FGameXXKTravelMoneyRules::Spend(State, 7, &Error));
	TestEqual(TEXT("backpack stack is consumed first"), State.Inventory.FindRef(FGameXXKTravelMoneyRules::ItemId()), 0);
	TestEqual(TEXT("warehouse pays only the remaining amount"), State.DesktopInventory.WarehouseItems.FindRef(FGameXXKTravelMoneyRules::ItemId()), 5);
	TestEqual(TEXT("merchant currency never debits ordinary gold"), State.PlayerGold, 1000000);
	TestEqual(TEXT("retired route counter is not treated as physical currency"), State.CardRun.RouteTravelMoney, 10000);
	TestFalse(TEXT("negative amount cannot mint travel money"), FGameXXKTravelMoneyRules::Spend(State, -1, &Error));
	TestEqual(TEXT("rejected negative payment preserves balance"), FGameXXKTravelMoneyRules::GetBalance(State), int64(5));
	State.Inventory.Add(FGameXXKTravelMoneyRules::ItemId(), 4);
	TestTrue(TEXT("both currency stacks can be normalized again"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	FGameXXKDesktopInventoryMoveRequest Move;
	Move.FromContainer = EGameXXKDesktopItemContainer::Backpack;
	Move.ToContainer = EGameXXKDesktopItemContainer::Warehouse;
	Move.ExpectedEntry = FGameXXKDesktopInventoryRules::MakeItemEntry(FGameXXKTravelMoneyRules::ItemId());
	Move.FromSlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Move.FromContainer, Move.ExpectedEntry);
	Move.ToSlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Move.ToContainer, Move.ExpectedEntry);
	TestTrue(TEXT("moving to the existing warehouse stack merges without duplication"), FGameXXKDesktopInventoryRules::MoveOrSwap(State, Move, &Error));
	TestEqual(TEXT("merge consumes the source stack"), State.Inventory.FindRef(FGameXXKTravelMoneyRules::ItemId()), 0);
	TestEqual(TEXT("merge preserves all nine coins"), FGameXXKTravelMoneyRules::GetBalance(State), int64(9));
	State.Inventory.Add(FGameXXKTravelMoneyRules::ItemId(), MAX_int32);
	TestTrue(TEXT("each container may hold its own bounded stack"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	Move.FromSlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Move.FromContainer, Move.ExpectedEntry);
	const auto BeforeOverflow = State;
	TestFalse(TEXT("merged stack overflow is rejected"), FGameXXKDesktopInventoryRules::MoveOrSwap(State, Move, &Error));
	TestTrue(TEXT("merge overflow preserves both stacks"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforeOverflow, PPF_None));
	State.DesktopInventory.BackpackSlots[Move.FromSlotIndex + 1] = Move.ExpectedEntry;
	TestFalse(TEXT("currency still cannot occupy duplicate slots within one container"), FGameXXKDesktopInventoryRules::Validate(State, &Error));
	return true;
}
#endif
