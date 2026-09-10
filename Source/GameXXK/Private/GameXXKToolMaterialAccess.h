#pragma once
#include "GameXXKMVPRules.h"

/** Physical inventory remains authoritative; legacy material mirrors are not a wallet. */
namespace GameXXKToolMaterialAccess
{
    inline int32 Count(const FGameXXKRuntimeState& State,FName Id)
    {
        return static_cast<int32>(FMath::Min<int64>(MAX_int32,
            int64(FMath::Max(0,State.Inventory.FindRef(Id)))+FMath::Max(0,State.DesktopInventory.WarehouseItems.FindRef(Id))));
    }

    inline bool Consume(FGameXXKRuntimeState& State,FName Id,int32 Quantity)
    {
        if(Quantity<=0||State.Inventory.FindRef(Id)<0||State.DesktopInventory.WarehouseItems.FindRef(Id)<0||Count(State,Id)<Quantity)return false;
        const int32 FromBag=FMath::Min(Quantity,State.Inventory.FindRef(Id));
        // Retain an existing zero key until the normalizer refreshes legacy mirrors;
        // removing it early could reactivate the pre-item refinement-sand migration.
        if(FromBag>0)State.Inventory.FindOrAdd(Id)-=FromBag;
        if(Quantity>FromBag)State.DesktopInventory.WarehouseItems.FindOrAdd(Id)-=Quantity-FromBag;
        return true;
    }

    inline bool Add(FGameXXKRuntimeState& State,FName Id,int32 Quantity)
    {
        if(Quantity<0||State.Inventory.FindRef(Id)<0||State.DesktopInventory.WarehouseItems.FindRef(Id)<0
            ||int64(Count(State,Id))+Quantity>MAX_int32)return false;
        auto& Items=State.DesktopInventory.WarehouseItems.FindRef(Id)>0?State.DesktopInventory.WarehouseItems:State.Inventory;
        Items.FindOrAdd(Id)+=Quantity;return true;
    }
}
