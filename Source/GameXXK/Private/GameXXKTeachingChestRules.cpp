#include "GameXXKTeachingChestRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKGemRules.h"

namespace
{
bool Fail(FString& Error,const TCHAR* Text){Error=Text;return false;}
bool Finish(FGameXXKRuntimeState& S,FString& Error)
{
    return FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(S)
        &&FGameXXKDesktopInventoryRules::Normalize(S,&Error)&&FGameXXKTeachingChestRules::Validate(S,Error);
}
FName PracticeKey(EGameXXKTeachingChestEvidence Kind)
{return FName(*FString::Printf(TEXT("TeachingChest.Practice.%d"),static_cast<int32>(Kind)));}
bool RoomForEquipment(const FGameXXKRuntimeState& S,FString& Error)
{
    return FGameXXKDesktopInventoryRules::FindFirstEmptySlot(S,EGameXXKDesktopItemContainer::Backpack)!=INDEX_NONE
        ||Fail(Error,TEXT("背包已满，剩余宝箱已保留"));
}
bool GiveStack(FGameXXKRuntimeState& S,FName Id,int32 Count,FString& Error)
{
    const bool Stored=S.DesktopInventory.WarehouseItems.FindRef(Id)>0;
    auto& Items=Stored?S.DesktopInventory.WarehouseItems:S.Inventory;
    if(Items.FindRef(Id)==0&&!RoomForEquipment(S,Error))return false;
    if(Count<=0||Items.FindRef(Id)>MAX_int32-Count)return Fail(Error,TEXT("教学物品数量无效"));
    Items.FindOrAdd(Id)+=Count;
    return FGameXXKDesktopInventoryRules::Normalize(S,&Error);
}
bool GiveEquipment(FGameXXKRuntimeState& S,int32 Ordinal,EGameXXKEquipmentSlot Slot,FName& Id,FString& Error)
{
    if(!RoomForEquipment(S,Error))return false;
    Id=FName(*FString::Printf(TEXT("TeachingEquipment.%d"),Ordinal));
    if(FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,Id))return Fail(Error,TEXT("教学物品已领取，不能重复发放"));
    // A separate fixed generator does not change ordinary equipment/chest RNG.
    FGameXXKEquipmentCollectionState Loot;Loot.CollectionSeed=0x54454143;Loot.NextInstanceOrdinal=Ordinal;
    FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::PoJun;
    Request.Quality=EGameXXKEquipmentQuality::Common;Request.ItemLevel=1;Request.bForceSlot=true;Request.ForcedSlot=Slot;
    FName Generated;
    if(!FGameXXKEquipmentRules::CreateRolledInstance(Loot,Request,Generated,&Error))return false;
    auto Item=*FGameXXKEquipmentRules::FindInstance(Loot,Generated);Item.InstanceId=Id;
    S.EquipmentCollection.EquipmentInstances.Add(Item);S.EquipmentCollection.WarehouseInstanceIds.Add(Id);
    return FGameXXKDesktopInventoryRules::Normalize(S,&Error);
}
FName FindStartingHead(const FGameXXKRuntimeState& S)
{
    for(const auto& Item:S.EquipmentCollection.EquipmentInstances)
        if(const auto* D=FGameXXKEquipmentCatalog::FindDefinition(Item.BaseEquipmentId))
            if(D->Slot==EGameXXKEquipmentSlot::Head&&Item.ItemLevel<=S.PlayerLevel&&!Item.SocketedGems.IsEmpty())return Item.InstanceId;
    return NAME_None;
}
bool AppendFixedChest(FGameXXKRuntimeState& S,FName Id,FString& Error)
{
    if(S.Training.OwnedChestTokens.ContainsByPredicate([Id](const auto& Token){return Token.FixedDropId==Id;}))return true;
    if(!FGameXXKTrainingRules::AppendChestToken(S.Training,EGameXXKTrainingRewardTier::NormalChest,TEXT("Training.Normal.1-1"),1,&Error))return false;
    S.Training.OwnedChestTokens.Last().FixedDropId=Id;return true;
}
bool ConsumeFixedChest(FGameXXKRuntimeState& S,FName Id,FString& Error)
{
    const int32 Index=S.Training.OwnedChestTokens.IndexOfByPredicate([Id](const auto& Token){return Token.FixedDropId==Id;});
    if(Index==INDEX_NONE)return Fail(Error,TEXT("没有可打开的普通宝箱"));
    if(S.Training.NextChestOpenOrdinal==MAX_int32)return Fail(Error,TEXT("宝箱开启序号耗尽"));
    S.Training.OwnedChestTokens.RemoveAt(Index);++S.Training.NextChestOpenOrdinal;return true;
}
bool CompleteStage(FGameXXKRuntimeState& S)
{
    auto& P=S.GuideProgress.TeachingChests;
    if(P.Stage<1||P.Stage>5||!P.bOpened)return false;
    FString Error;if(!AppendFixedChest(S,FGameXXKTeachingChestRules::StageName(P.Stage+1),Error))return false;
    const int32 Completed=P.Stage;P.CompletedStages=Completed;++P.Stage;P.bOpened=false;P.bDismissed=false;
    P.bEnhancementReviewPending=false;
    return true;
}
void QueueEnhancementReview(FGameXXKRuntimeState& S,FName Id,bool JustEnhanced)
{
    const auto* Item=FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,Id);if(!Item)return;
    auto& P=S.GuideProgress.TeachingChests;P.TargetId=Id;P.EnhancementBaseEquipmentId=Item->BaseEquipmentId;
    P.EnhancementAfterLevel=Item->EnhancementLevel;P.BaselineEnhancement=FMath::Max(0,Item->EnhancementLevel-(JustEnhanced?1:0));
    auto Before=S.EquipmentCollection;
    Before.EquipmentInstances.FindByPredicate([Id](const auto& Entry){return Entry.InstanceId==Id;})->EnhancementLevel=P.BaselineEnhancement;
    FGameXXKEquipmentTooltipSnapshot Old,Now;
    FGameXXKCharacterStats Bare;Bare.MaxHealth=1;Bare.MaxMana=1;
    if(FGameXXKEquipmentRules::BuildTooltipSnapshot(Before,Id,TEXT("Player"),Bare,Old)
        &&FGameXXKEquipmentRules::BuildTooltipSnapshot(S.EquipmentCollection,Id,TEXT("Player"),Bare,Now))
    {P.EnhancementBeforeStats=Old.ItemCurrentStats;P.EnhancementAfterStats=Now.ItemCurrentStats;P.bEnhancementReviewPending=true;}
}
}

FName FGameXXKTeachingChestRules::StageName(int32 Stage)
{return Stage>=1&&Stage<=6?FName(*FString::Printf(TEXT("TeachingChest.Stage.%d"),Stage)):NAME_None;}
FName FGameXXKTeachingChestRules::MaterialDropId(int32 Index)
{return Index>=1&&Index<=8?FName(*FString::Printf(TEXT("TeachingChest.Material.%d"),Index)):NAME_None;}
bool FGameXXKTeachingChestRules::IsFixedDropId(FName Id)
{
    for(int32 I=1;I<=6;++I)if(Id==StageName(I))return true;
    for(int32 I=1;I<=8;++I)if(Id==MaterialDropId(I))return true;
    return false;
}
bool FGameXXKTeachingChestRules::RestoreOrdinaryChestTokens(FGameXXKRuntimeState& State,FString& Error)
{
    Error.Reset();auto Candidate=State;const auto& P=Candidate.GuideProgress.TeachingChests;
    if(!P.bEnabled)return true;
    if(!P.bOpened&&!AppendFixedChest(Candidate,StageName(P.Stage),Error))return false;
    for(int32 I=P.MaterialBoxesOpened+1;I<=P.MaterialBoxesOpened+P.MaterialBoxesRemaining;++I)
        if(!AppendFixedChest(Candidate,MaterialDropId(I),Error))return false;
    if(!Validate(Candidate,Error))return false;State=MoveTemp(Candidate);return true;
}
bool FGameXXKTeachingChestRules::CompleteEnhancementReview(FGameXXKRuntimeState& State,FString& Error)
{
    Error.Reset();const auto& P=State.GuideProgress.TeachingChests;
    if(!P.bEnabled||P.Stage!=3||!P.bOpened||!P.bEnhancementReviewPending)return false;
    auto Candidate=State;if(!CompleteStage(Candidate))return Fail(Error,TEXT("下一只宝箱暂时无法发放"));
    if(!Finish(Candidate,Error))return false;State=MoveTemp(Candidate);return true;
}

bool FGameXXKTeachingChestRules::Initialize(FGameXXKRuntimeState& State,FString& Error)
{
    Error.Reset();if(State.GuideProgress.TeachingChests.bEnabled)return Validate(State,Error);
    auto S=State;auto& P=S.GuideProgress.TeachingChests;P=FGameXXKTeachingChestProgress();
    for(const auto& Item:S.EquipmentCollection.EquipmentInstances)
        if(const auto* D=FGameXXKEquipmentCatalog::FindDefinition(Item.BaseEquipmentId))
            if(D->Slot==EGameXXKEquipmentSlot::Weapon&&Item.OwnerKind==EGameXXKEquipmentOwnerKind::Warehouse)
            {P.ReservedWeapon=Item;break;}
    if(P.ReservedWeapon.InstanceId.IsNone())return Fail(Error,TEXT("新档起始武器缺失"));
    P.bEnabled=true;P.Stage=1;P.WeaponId=P.ReservedWeapon.InstanceId;P.TargetId=P.WeaponId;
    S.EquipmentCollection.WarehouseInstanceIds.Remove(P.WeaponId);
    S.EquipmentCollection.EquipmentInstances.RemoveAll([&](const auto& Item){return Item.InstanceId==P.WeaponId;});
    S.DesktopInventory.WarehouseEquipmentInstanceIds.Remove(P.WeaponId);
    if(!AppendFixedChest(S,StageName(1),Error))return false;
    if(!Finish(S,Error))return false;
    State=MoveTemp(S);return true;
}

bool FGameXXKTeachingChestRules::Validate(const FGameXXKRuntimeState& S,FString& Error)
{
    Error.Reset();const auto& P=S.GuideProgress.TeachingChests;
    TSet<FName> Expected;
    if(P.bEnabled&&!P.bOpened)Expected.Add(StageName(P.Stage));
    if(P.bEnabled&&P.Stage==6&&P.bOpened)
        for(int32 I=P.MaterialBoxesOpened+1;I<=8;++I)Expected.Add(MaterialDropId(I));
    for(const auto& Token:S.Training.OwnedChestTokens)
        if(!Token.FixedDropId.IsNone()&&(Token.Tier!=EGameXXKTrainingRewardTier::NormalChest||!Expected.Remove(Token.FixedDropId)))
            return Fail(Error,TEXT("配套普通宝箱与课程进度不一致"));
    if(!Expected.IsEmpty())return Fail(Error,TEXT("配套普通宝箱缺失"));
    if(P.bEnhancementReviewPending&&(!P.bEnabled||P.Stage!=3||!P.bOpened||P.EnhancementBaseEquipmentId.IsNone()
        ||P.EnhancementAfterLevel<P.BaselineEnhancement))return Fail(Error,TEXT("强化属性回顾状态无效"));
    if(!P.bEnabled)
        return (P.Stage==0&&!P.bOpened&&P.CompletedStages==0&&P.MaterialBoxesRemaining==0&&P.MaterialBoxesOpened==0
            &&P.ReservedWeapon.InstanceId.IsNone()&&P.CombineInputIds.IsEmpty())||Fail(Error,TEXT("未启用的教学箱存档包含发放状态"));
    if(P.Stage<1||P.Stage>6||P.CompletedStages<0||P.CompletedStages>6||P.WeaponId.IsNone()
        ||(P.Stage<6&&P.CompletedStages!=P.Stage-1)||(P.Stage==6&&P.CompletedStages<5)
        ||P.MaterialBoxesRemaining<0||P.MaterialBoxesRemaining>8||P.MaterialBoxesOpened<0||P.MaterialBoxesOpened>8)
        return Fail(Error,TEXT("教学箱阶段或数量无效"));
    const bool Reserved=P.Stage==1&&!P.bOpened;
    if(Reserved!=(P.ReservedWeapon.InstanceId==P.WeaponId)
        ||(Reserved&&FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,P.WeaponId))
        ||(!Reserved&&!P.ReservedWeapon.InstanceId.IsNone()))return Fail(Error,TEXT("教学箱起始武器状态无效"));
    if(Reserved)
    {
        const auto* D=FGameXXKEquipmentCatalog::FindDefinition(P.ReservedWeapon.BaseEquipmentId);
        if(!D||D->Slot!=EGameXXKEquipmentSlot::Weapon||P.ReservedWeapon.OwnerKind!=EGameXXKEquipmentOwnerKind::Warehouse
            ||P.ReservedWeapon.ItemLevel<1||!FGameXXKEquipmentQualityRules::IsValid(P.ReservedWeapon.Quality))
            return Fail(Error,TEXT("教学箱中的起始武器无效"));
    }
    const bool Sixth=P.Stage==6&&P.bOpened;
    if(P.MaterialBoxesRemaining+P.MaterialBoxesOpened!=(Sixth?8:0)
        ||P.CombineInputIds.Num()!=(Sixth?1+P.MaterialBoxesOpened:0)
        ||((P.bAutoFillPracticed||P.bCombinePracticed||P.CompletedStages==6)&&!Sixth)
        ||(P.CompletedStages==6&&!P.bAutoFillPracticed&&!P.bCombinePracticed))return Fail(Error,TEXT("合成材料箱回执不一致"));
    TSet<FName> Seen;
    for(FName Id:P.CombineInputIds){if(Id.IsNone()||Seen.Contains(Id))return Fail(Error,TEXT("合成教具回执重复"));Seen.Add(Id);}
    return true;
}

bool FGameXXKTeachingChestRules::Open(FGameXXKRuntimeState& State,bool bMaterials,bool bAll,int32& Opened,FString& Error)
{
    Opened=0;Error.Reset();
    if(!Validate(State,Error))return false;
    const auto& Before=State.GuideProgress.TeachingChests;
    if(!Before.bEnabled||State.Screen!=EGameXXKScreen::Town||State.bDungeonActive||State.Training.bChallengeActive
        ||State.CardRun.bHasActiveCardBattle||State.EquipmentCollection.PendingReforge.bActive)
        return Fail(Error,TEXT("请先结束当前操作，再打开宝箱"));
    auto S=State;auto& P=S.GuideProgress.TeachingChests;
    if(bMaterials)
    {
        if(P.Stage!=6||!P.bOpened||P.MaterialBoxesRemaining<=0)return Fail(Error,TEXT("没有待开的合成材料箱"));
        const int32 Bound=bAll?P.MaterialBoxesRemaining:1;
        for(int32 I=0;I<Bound;++I)
        {
            if(!RoomForEquipment(S,Error))break;
            FName Id;
            if(!GiveEquipment(S,60+P.MaterialBoxesOpened+1,EGameXXKEquipmentSlot::Armor,Id,Error)){Opened=0;return false;}
            if(!ConsumeFixedChest(S,MaterialDropId(P.MaterialBoxesOpened+1),Error)){Opened=0;return false;}
            P.CombineInputIds.Add(Id);++P.MaterialBoxesOpened;--P.MaterialBoxesRemaining;++Opened;
        }
        if(Opened==0)return false;
    }
    else
    {
        if(P.bOpened)return Fail(Error,TEXT("这只宝箱已经打开"));
        if(!ConsumeFixedChest(S,StageName(P.Stage),Error))return false;
        if(P.Stage==1)
        {
            if(!RoomForEquipment(S,Error))return false;
            S.EquipmentCollection.EquipmentInstances.Add(P.ReservedWeapon);
            S.EquipmentCollection.WarehouseInstanceIds.Add(P.WeaponId);P.ReservedWeapon=FGameXXKEquipmentInstance();P.TargetId=P.WeaponId;
        }
        else if(P.Stage==2)
        {
            P.TargetId=FindStartingHead(S);
            if(P.TargetId.IsNone()&&!GiveEquipment(S,2,EGameXXKEquipmentSlot::Head,P.TargetId,Error))return false;
            P.GemId=FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack,EGameXXKGemQuality::Common);
            if(!GiveStack(S,P.GemId,1,Error))return false;
        }
        else if(P.Stage==3)
        {
            const auto* Target=FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,P.TargetId);
            if(!Target||Target->EnhancementLevel>=FGameXXKEquipmentRules::MaxEnhancementLevel)
                if(!GiveEquipment(S,3,EGameXXKEquipmentSlot::Head,P.TargetId,Error))return false;
            Target=FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,P.TargetId);
            P.BaselineEnhancement=Target->EnhancementLevel;
            if(!GiveStack(S,UGameXXKMVPRules::ItemEnhancementStone(),1,Error))return false;
        }
        else if(P.Stage==4)
        {
            if(!GiveEquipment(S,4,EGameXXKEquipmentSlot::Head,P.TargetId,Error)
                ||!GiveStack(S,UGameXXKMVPRules::ItemRefinementSand(),1,Error))return false;
            P.BaselineReforgeOrdinal=S.EquipmentCollection.NextReforgeOrdinal;
        }
        else if(P.Stage==5)
        {
            if(!GiveEquipment(S,5,EGameXXKEquipmentSlot::Shoes,P.TargetId,Error))return false;
        }
        else
        {
            FName Id;if(!GiveEquipment(S,60,EGameXXKEquipmentSlot::Armor,Id,Error))return false;
            P.TargetId=Id;P.CombineInputIds={Id};P.MaterialBoxesRemaining=8;P.MaterialBoxesOpened=0;
            for(int32 I=1;I<=8;++I)if(!AppendFixedChest(S,MaterialDropId(I),Error))return false;
        }
        P.bOpened=true;P.bDismissed=false;Opened=1;
        // Natural practice is respected without consuming another lesson's resources.
        if(P.Stage<=5&&S.GuideProgress.CompletedGuideStepIds.Contains(PracticeKey(static_cast<EGameXXKTeachingChestEvidence>(P.Stage-1))))
        {if(P.Stage==3)QueueEnhancementReview(S,P.TargetId,false);else CompleteStage(S);}
    }
    FString Validation;
    if(!Finish(S,Validation)){Opened=0;Error=Validation;return false;}
    State=MoveTemp(S);return true;
}

void FGameXXKTeachingChestRules::Observe(FGameXXKRuntimeState& S,EGameXXKTeachingChestEvidence Evidence,const TArray<FName>& Inputs)
{
    auto& P=S.GuideProgress.TeachingChests;if(!P.bEnabled||Inputs.IsEmpty())return;
    if(Evidence==EGameXXKTeachingChestEvidence::Equip)
    {
        const auto* Hero=S.EquipmentCollection.CharacterLoadouts.Find(TEXT("Player"));
        if(!Hero||Hero->WeaponInstanceId.IsNone()||!Inputs.Contains(Hero->WeaponInstanceId))return;
    }
    S.GuideProgress.CompletedGuideStepIds.Add(PracticeKey(Evidence));
    if(!P.bOpened)return;
    if(P.Stage<=5&&static_cast<int32>(Evidence)==P.Stage-1)
    {if(P.Stage==3)QueueEnhancementReview(S,Inputs[0],true);else CompleteStage(S);}
    else if(P.Stage==6&&Evidence==EGameXXKTeachingChestEvidence::Combine&&Inputs.Num()==9)
    {P.bCombinePracticed=true;P.CompletedStages=6;}
}
