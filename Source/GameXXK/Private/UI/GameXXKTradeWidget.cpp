#include "UI/GameXXKTradeWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKTradeWidget::BuyItemById(FName ItemId, int32 Quantity)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bBought = Subsystem && Subsystem->BuyItem(ItemId, Quantity);
	if (bBought)
	{
		OnTradeChanged();
	}
	return bBought;
}

bool UGameXXKTradeWidget::SellItemById(FName ItemId, int32 Quantity)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bSold = Subsystem && Subsystem->SellItem(ItemId, Quantity);
	if (bSold)
	{
		OnTradeChanged();
	}
	return bSold;
}
