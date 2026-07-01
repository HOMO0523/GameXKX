#include "UI/GameXXKInventoryWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

int32 UGameXXKInventoryWidget::GetItemCount(FName ItemId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetItemCount(ItemId) : 0;
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
