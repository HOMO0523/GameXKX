#include "GameXXKHuntRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "UI/GameXXKLocalization.h"

namespace
{
    bool Fail(FString* Error,const TCHAR* Key)
    {if(Error)*Error=GameXXKLocalization::Text(Key).ToString();return false;}
    bool InsertOrder(FGameXXKRuntimeState& State,FName Id,int32 Count)
    {
        const int64 Total=static_cast<int64>(State.Inventory.FindRef(Id))+State.DesktopInventory.WarehouseItems.FindRef(Id);
        if(Count<=0||Total+Count>MAX_int32)return false;
        if(State.Inventory.FindRef(Id)>0){State.Inventory.FindChecked(Id)+=Count;return true;}
        if(State.DesktopInventory.WarehouseItems.FindRef(Id)>0){State.DesktopInventory.WarehouseItems.FindChecked(Id)+=Count;return true;}
        for(auto Container:{EGameXXKDesktopItemContainer::Backpack,EGameXXKDesktopItemContainer::Warehouse})
        {
            const int32 Slot=FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State,Container);
            if(Slot==INDEX_NONE)continue;
            auto& Slots=Container==EGameXXKDesktopItemContainer::Backpack?State.DesktopInventory.BackpackSlots:State.DesktopInventory.WarehouseSlots;
            auto& Items=Container==EGameXXKDesktopItemContainer::Backpack?State.Inventory:State.DesktopInventory.WarehouseItems;
            if(!Slots.IsValidIndex(Slot))continue;
            Slots[Slot]=FGameXXKDesktopInventoryRules::MakeItemEntry(Id);Items.Add(Id,Count);return true;
        }
        return false;
    }
}

bool FGameXXKHuntRules::IsHuntStage(FName Id)
{
    return Id==TEXT("Training.Normal.3-4")||Id==TEXT("Training.Hard.3-4")||Id==TEXT("Training.Hell.3-4");
}
FName FGameXXKHuntRules::OrderId(EGameXXKTrainingDifficulty Difficulty)
{
    return Difficulty==EGameXXKTrainingDifficulty::Hard?FName(TEXT("Item.HuntOrder.Epic")):
        Difficulty==EGameXXKTrainingDifficulty::Hell?FName(TEXT("Item.HuntOrder.Immortal")):FName(TEXT("Item.HuntOrder.Common"));
}
FName FGameXXKHuntRules::RequiredOrder(FName StageId)
{return IsHuntStage(StageId)?OrderId(FGameXXKTrainingRules::DifficultyFromStageId(StageId)):NAME_None;}
bool FGameXXKHuntRules::IsOrder(FName Id)
{return Id==OrderId(EGameXXKTrainingDifficulty::Normal)||Id==OrderId(EGameXXKTrainingDifficulty::Hard)||Id==OrderId(EGameXXKTrainingDifficulty::Hell);}
EGameXXKEquipmentQuality FGameXXKHuntRules::OrderQuality(FName Id)
{
    if(Id==OrderId(EGameXXKTrainingDifficulty::Normal))return EGameXXKEquipmentQuality::Common;
    if(Id==OrderId(EGameXXKTrainingDifficulty::Hard))return EGameXXKEquipmentQuality::Epic;
    if(Id==OrderId(EGameXXKTrainingDifficulty::Hell))return EGameXXKEquipmentQuality::Immortal;
    return EGameXXKEquipmentQuality::Invalid;
}
FText FGameXXKHuntRules::OrderName(FName Id)
{return IsOrder(Id)?GameXXKLocalization::Text(*(Id.ToString()+TEXT(".Name"))):FText::GetEmpty();}
FText FGameXXKHuntRules::OrderDescription(FName Id)
{return IsOrder(Id)?GameXXKLocalization::Text(*(Id.ToString()+TEXT(".Description"))):FText::GetEmpty();}
int32 FGameXXKHuntRules::Balance(const FGameXXKRuntimeState& State,FName Id)
{
    if(!IsOrder(Id))return 0;
    return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(State.Inventory.FindRef(Id))+State.DesktopInventory.WarehouseItems.FindRef(Id),0,MAX_int32));
}
bool FGameXXKHuntRules::CanEnter(const FGameXXKRuntimeState& State,FName StageId)
{
    if(!IsHuntStage(StageId))return false;
    const FName Id=RequiredOrder(StageId);
    return FGameXXKTrainingRules::IsStageCleared(State.Training,FGameXXKTrainingRules::MakeStageId(FGameXXKTrainingRules::DifficultyFromStageId(StageId),9))
        && Balance(State,Id)>State.Training.PendingHuntTravelOrdersConsumed
        && !FGameXXKDesktopInventoryRules::IsEntryLocked(State,FGameXXKDesktopInventoryRules::MakeItemEntry(Id));
}
bool FGameXXKHuntRules::Reserve(FGameXXKRuntimeState& State,FName StageId,bool bTravel,FString* Error)
{
    if(State.Training.PendingHuntTravelOrdersConsumed>0)return Fail(Error,TEXT("Hunt.Error.PendingSettlement"));
    if(!CanEnter(State,StageId))return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    if(!bTravel){State.Training.HuntReferenceHeroExperience=0;State.Training.HuntReferenceCompanionExperience=0;}
    State.Training.ReservedHuntOrderId=RequiredOrder(StageId);
    State.Training.HuntReservationId=FGuid::NewGuid();
    State.Training.HuntTravelOrderId=bTravel?RequiredOrder(StageId):NAME_None;
    State.Training.HuntTravelOrderBudget=bTravel?Balance(State,RequiredOrder(StageId)):0;
    return true;
}
void FGameXXKHuntRules::Release(FGameXXKRuntimeState& State)
{
    State.Training.ReservedHuntOrderId=NAME_None;State.Training.HuntReservationId.Invalidate();
    if(State.Training.PendingHuntTravelOrdersConsumed==0)
    {State.Training.HuntTravelOrderId=NAME_None;State.Training.HuntTravelOrderBudget=0;}
}
bool FGameXXKHuntRules::ConsumeReserved(FGameXXKRuntimeState& State,FString* Error)
{
    const FName Id=State.Training.ReservedHuntOrderId;
    if(!IsOrder(Id)||!State.Training.HuntReservationId.IsValid()||Balance(State,Id)<1)
        return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    if(State.Training.HuntReservationId==State.Training.LastCompletedHuntReservationId)
        return Fail(Error,TEXT("Hunt.Error.AlreadySettled"));
    FGameXXKRuntimeState Candidate=State;
    auto& Items=Candidate.Inventory.FindRef(Id)>0?Candidate.Inventory:Candidate.DesktopInventory.WarehouseItems;
    const int32 Remaining=Items.FindRef(Id)-1;
    if(Remaining>0)Items.Add(Id,Remaining);else Items.Remove(Id);
    Candidate.Training.LastCompletedHuntReservationId=Candidate.Training.HuntReservationId;
    Release(Candidate);
    if(!FGameXXKDesktopInventoryRules::Normalize(Candidate,Error))return false;
    State=MoveTemp(Candidate);return true;
}
bool FGameXXKHuntRules::Grant(FGameXXKRuntimeState& State,FName Id,int32 Count,bool bAllowPending,FString* Error)
{
    if(!IsOrder(Id)||Count<=0)return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    FGameXXKRuntimeState Candidate=State;
    if(!FGameXXKDesktopInventoryRules::Normalize(Candidate,Error))return false;
    if(!InsertOrder(Candidate,Id,Count))
    {
        const int64 Pending=static_cast<int64>(Candidate.Training.PendingHuntOrders.FindRef(Id))+Count;
        if(!bAllowPending||Pending>MAX_int32)return Fail(Error,TEXT("Hunt.Error.InventoryFull"));
        Candidate.Training.PendingHuntOrders.Add(Id,static_cast<int32>(Pending));
    }
    if(!FGameXXKDesktopInventoryRules::Normalize(Candidate,Error))return false;
    State=MoveTemp(Candidate);return true;
}
bool FGameXXKHuntRules::GrantFirstClear(FGameXXKRuntimeState& State,FName StageId,FString* Error)
{
    const auto Difficulty=FGameXXKTrainingRules::DifficultyFromStageId(StageId);
    if(StageId!=FGameXXKTrainingRules::MakeStageId(Difficulty,9)||State.Training.HuntFirstClearOrderGrants.Contains(StageId))return true;
    if(!State.Training.ClearedStageIds.Contains(StageId))return Fail(Error,TEXT("Hunt.Error.SourceNotCleared"));
    if(!Grant(State,OrderId(Difficulty),1,true,Error))return false;
    State.Training.HuntFirstClearOrderGrants.Add(StageId);return true;
}
bool FGameXXKHuntRules::DeliverPending(FGameXXKRuntimeState& State)
{
    TArray<FName> Ids;State.Training.PendingHuntOrders.GetKeys(Ids);Ids.Sort(FNameLexicalLess());
    bool Changed=false;
    for(FName Id:Ids)if(IsOrder(Id)&&InsertOrder(State,Id,State.Training.PendingHuntOrders.FindRef(Id)))
    {State.Training.PendingHuntOrders.Remove(Id);Changed=true;}
    return Changed;
}
bool FGameXXKHuntRules::Validate(const FGameXXKRuntimeState& State,FString* Error)
{
    const auto& P=State.Training;
    if(P.bChallengeActive&&IsHuntStage(P.ActiveChallengeStageId)
        &&(P.ReservedHuntOrderId!=RequiredOrder(P.ActiveChallengeStageId)||!P.HuntReservationId.IsValid()))
        return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    if(P.bTravelActive&&IsHuntStage(P.CurrentTravelStageId)
        &&(P.HuntTravelOrderId!=RequiredOrder(P.CurrentTravelStageId)||P.HuntTravelOrderBudget<=0))
        return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    for(const auto& Pair:P.PendingHuntOrders)if(!IsOrder(Pair.Key)||Pair.Value<1)return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    for(FName Id:P.HuntFirstClearOrderGrants)
        if(Id!=FGameXXKTrainingRules::MakeStageId(FGameXXKTrainingRules::DifficultyFromStageId(Id),9)||!P.ClearedStageIds.Contains(Id))
            return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    if(P.HuntReferenceHeroExperience<0||P.HuntReferenceCompanionExperience<0)return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    if(P.HuntTravelOrderBudget<0||P.PendingHuntTravelOrdersConsumed<0)return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    if(!P.ReservedHuntOrderId.IsNone()&&(!IsOrder(P.ReservedHuntOrderId)||!P.HuntReservationId.IsValid()||Balance(State,P.ReservedHuntOrderId)<1))
        return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    if(P.PendingHuntTravelOrdersConsumed>0&&(!IsOrder(P.HuntTravelOrderId)||Balance(State,P.HuntTravelOrderId)<P.PendingHuntTravelOrdersConsumed))
        return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    return true;
}

bool FGameXXKHuntRules::GrantDeferredExperience(FGameXXKRuntimeState& State,FString* Error)
{
    if(State.Training.HuntReferenceHeroExperience<0||State.Training.HuntReferenceCompanionExperience<0)return Fail(Error,TEXT("Hunt.Error.InvalidOrder"));
    FGameXXKRuntimeState Candidate=State;
    const int32 HeroExtra=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(Candidate.Training.HuntReferenceHeroExperience)*3/2));
    const int32 CompanionExtra=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(Candidate.Training.HuntReferenceCompanionExperience)*3/2));
    UGameXXKMVPRules::ApplyPlayerExperience(Candidate,HeroExtra);
    const FName CompanionId=Candidate.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
    auto* Companion=Candidate.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CompanionId](const auto& C){return C.InstanceId==CompanionId;});
    if(CompanionExtra>0&&(!Companion||!FGameXXKCompanionRules::AwardExperience(*Companion,CompanionExtra,Error)))return false;
    Candidate.Training.HuntReferenceHeroExperience=0;Candidate.Training.HuntReferenceCompanionExperience=0;
    State=MoveTemp(Candidate);return true;
}

void FGameXXKHuntRules::SynchronizeTravelBudget(FGameXXKRuntimeState& State)
{
    auto& P=State.Training;
    if(P.bChallengeActive||!IsHuntStage(P.CurrentTravelStageId))return;
    const FName Id=RequiredOrder(P.CurrentTravelStageId);
    P.HuntTravelOrderId=Id;
    P.HuntTravelOrderBudget=FMath::Max(0,Balance(State,Id)-P.PendingHuntTravelOrdersConsumed);
    if(FGameXXKDesktopInventoryRules::IsEntryLocked(State,FGameXXKDesktopInventoryRules::MakeItemEntry(Id)))
        P.HuntTravelOrderBudget=FMath::Min(P.HuntTravelOrderBudget,P.ReservedHuntOrderId==Id?1:0);
    if(P.HuntTravelOrderBudget==0)
    {
        P.bTravelActive=false;P.ActiveTravelEncounterIndex=INDEX_NONE;
        if(P.PendingHuntTravelOrdersConsumed==0)Release(State);
    }
    else if(P.bTravelActive&&P.ReservedHuntOrderId.IsNone())
    {P.ReservedHuntOrderId=Id;P.HuntReservationId=FGuid::NewGuid();}
}

bool FGameXXKHuntRules::SettleTravelOrders(FGameXXKRuntimeState& State,FString* Error)
{
    const int32 Count=State.Training.PendingHuntTravelOrdersConsumed;
    if(Count==0)
    {
        if(!State.Training.bChallengeActive&&!IsHuntStage(State.Training.CurrentTravelStageId))Release(State);
        return true;
    }
    const FName Id=State.Training.HuntTravelOrderId;
    if(Count<0||!IsOrder(Id)||Balance(State,Id)<Count)return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    FGameXXKRuntimeState Candidate=State;int32 Remaining=Count;
    for(auto* Items:{&Candidate.Inventory,&Candidate.DesktopInventory.WarehouseItems})
    {
        const int32 Owned=Items->FindRef(Id),Spent=FMath::Min(Owned,Remaining);
        if(Spent>0){if(Owned==Spent)Items->Remove(Id);else Items->Add(Id,Owned-Spent);Remaining-=Spent;}
    }
    if(Remaining!=0)return Fail(Error,TEXT("Hunt.Error.NoOrder"));
    Candidate.Training.PendingHuntTravelOrdersConsumed=0;
    Candidate.Training.LastCompletedHuntReservationId=Candidate.Training.HuntReservationId;
    Candidate.Training.ReservedHuntOrderId=NAME_None;Candidate.Training.HuntReservationId.Invalidate();
    if(!FGameXXKDesktopInventoryRules::Normalize(Candidate,Error))return false;
    if(Candidate.Training.bTravelActive&&IsHuntStage(Candidate.Training.CurrentTravelStageId)&&CanEnter(Candidate,Candidate.Training.CurrentTravelStageId))
    {if(!Reserve(Candidate,Candidate.Training.CurrentTravelStageId,true,Error))return false;}
    else
    {
        if(IsHuntStage(Candidate.Training.CurrentTravelStageId)){Candidate.Training.bTravelActive=false;Candidate.Training.ActiveTravelEncounterIndex=INDEX_NONE;}
        Release(Candidate);
    }
    State=MoveTemp(Candidate);return true;
}
