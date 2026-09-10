#pragma once
#include "CoreMinimal.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKEquipmentTypes.h"

struct FGameXXKRuntimeState;

/** Inventory-backed Hunt Orders. All spend/grant operations commit a complete candidate. */
class GAMEXXK_API FGameXXKHuntRules final
{
public:
    static bool IsHuntStage(FName StageId);
    static FName OrderId(EGameXXKTrainingDifficulty Difficulty);
    static FName RequiredOrder(FName StageId);
    static bool IsOrder(FName ItemId);
    static EGameXXKEquipmentQuality OrderQuality(FName ItemId);
    static FText OrderName(FName ItemId);
    static FText OrderDescription(FName ItemId);
    static int32 Balance(const FGameXXKRuntimeState& State,FName ItemId);
    static bool CanEnter(const FGameXXKRuntimeState& State,FName StageId);
    static bool Reserve(FGameXXKRuntimeState& State,FName StageId,bool bTravel,FString* OutError=nullptr);
    static void Release(FGameXXKRuntimeState& State);
    static bool ConsumeReserved(FGameXXKRuntimeState& State,FString* OutError=nullptr);
    static bool Grant(FGameXXKRuntimeState& State,FName ItemId,int32 Count,bool bAllowPending,FString* OutError=nullptr);
    static bool GrantFirstClear(FGameXXKRuntimeState& State,FName StageId,FString* OutError=nullptr);
    static bool DeliverPending(FGameXXKRuntimeState& State);
    static bool Validate(const FGameXXKRuntimeState& State,FString* OutError=nullptr);
    static bool AdvanceChapter(FGameXXKRuntimeState& State,FString* OutError=nullptr);
    static bool GrantDeferredExperience(FGameXXKRuntimeState& State,FString* OutError=nullptr);
    static void SynchronizeTravelBudget(FGameXXKRuntimeState& State);
    static bool SettleTravelOrders(FGameXXKRuntimeState& State,FString* OutError=nullptr);
    static constexpr const TCHAR* IconPath=TEXT("/Game/GameXXK/UI/Items/T_Item_HuntOrder.T_Item_HuntOrder");
};
