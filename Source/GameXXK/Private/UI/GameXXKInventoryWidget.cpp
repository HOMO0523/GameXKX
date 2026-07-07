#include "UI/GameXXKInventoryWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

int32 UGameXXKInventoryWidget::GetItemCount(FName ItemId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetItemCount(ItemId) : 0;
}

TArray<FName> UGameXXKInventoryWidget::GetInventoryItemIds() const
{
	TArray<FName> ItemIds;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return ItemIds;
	}

	for (const TPair<FName, int32>& Entry : Subsystem->GetRuntimeState().Inventory)
	{
		if (Entry.Value > 0)
		{
			ItemIds.Add(Entry.Key);
		}
	}
	ItemIds.Sort([](const FName& A, const FName& B)
	{
		return A.LexicalLess(B);
	});
	return ItemIds;
}

bool UGameXXKInventoryWidget::UseHealingItem()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bUsed = Subsystem && Subsystem->UseHealingItem();
	if (bUsed)
	{
		OnInventoryChanged();
	}
	return bUsed;
}

bool UGameXXKInventoryWidget::UseItemById(FName ItemId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bUsed = Subsystem && Subsystem->UseItem(ItemId);
	if (bUsed)
	{
		OnInventoryChanged();
	}
	return bUsed;
}

bool UGameXXKInventoryWidget::EquipItemById(FName ItemId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bEquipped = Subsystem && Subsystem->EquipItem(ItemId);
	if (bEquipped)
	{
		OnInventoryChanged();
	}
	return bEquipped;
}
