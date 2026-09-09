#include "GameXXKTravelMoneyRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMVPRules.h"

FName FGameXXKTravelMoneyRules::ItemId()
{
	static const FName Id(TEXT("Item.TravelMoney"));
	return Id;
}

int64 FGameXXKTravelMoneyRules::GetBalance(const FGameXXKRuntimeState& State)
{
	return static_cast<int64>(FMath::Max(0, State.Inventory.FindRef(ItemId())))
		+ FMath::Max(0, State.DesktopInventory.WarehouseItems.FindRef(ItemId()));
}

bool FGameXXKTravelMoneyRules::CanAfford(const FGameXXKRuntimeState& State, int32 Amount)
{
	return Amount >= 0 && GetBalance(State) >= Amount;
}

bool FGameXXKTravelMoneyRules::Spend(FGameXXKRuntimeState& State, int32 Amount, FString* OutError)
{
	if (OutError) OutError->Reset();
	if (!CanAfford(State, Amount))
	{
		if (OutError) *OutError = TEXT("行旅钱不足");
		return false;
	}
	if (Amount == 0) return true;
	FGameXXKRuntimeState Candidate = State;
	int32 Remaining = Amount;
	for (TMap<FName, int32>* Container : { &Candidate.Inventory, &Candidate.DesktopInventory.WarehouseItems })
	{
		const int32 Held = FMath::Max(0, Container->FindRef(ItemId()));
		const int32 Used = FMath::Min(Held, Remaining);
		if (Used > 0)
		{
			if (Held == Used) Container->Remove(ItemId());
			else Container->Add(ItemId(), Held - Used);
			Remaining -= Used;
		}
	}
	if (Remaining != 0 || !FGameXXKDesktopInventoryRules::Normalize(Candidate, OutError)) return false;
	State = MoveTemp(Candidate);
	return true;
}
