#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"
#include "GameXXKTeachingChestRules.generated.h"

struct FGameXXKRuntimeState;

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTeachingChestProgress
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bEnabled=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 Stage=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bOpened=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 CompletedStages=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bDismissed=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bAutoFillPracticed=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bCombinePracticed=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 MaterialBoxesRemaining=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 MaterialBoxesOpened=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName WeaponId;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName TargetId;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName GemId;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 BaselineEnhancement=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 BaselineReforgeOrdinal=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bEnhancementReviewPending=false;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName EnhancementBaseEquipmentId;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 EnhancementAfterLevel=0;
    UPROPERTY(BlueprintReadWrite, SaveGame) FGameXXKCharacterStats EnhancementBeforeStats;
    UPROPERTY(BlueprintReadWrite, SaveGame) FGameXXKCharacterStats EnhancementAfterStats;
    UPROPERTY(BlueprintReadWrite, SaveGame) FGameXXKEquipmentInstance ReservedWeapon;
    UPROPERTY(BlueprintReadWrite, SaveGame) TArray<FName> CombineInputIds;
};

enum class EGameXXKTeachingChestEvidence : uint8 { Equip, Socket, Enhance, Reforge, Dismantle, Combine };

/** Dedicated teaching inventory; ordinary chest ordinals and RNG are independent. */
class GAMEXXK_API FGameXXKTeachingChestRules
{
public:
    static bool Initialize(FGameXXKRuntimeState& State,FString& Error);
    static bool Validate(const FGameXXKRuntimeState& State,FString& Error);
    static bool Open(FGameXXKRuntimeState& State,bool bMaterialBoxes,bool bAll,int32& Opened,FString& Error);
    static void Observe(FGameXXKRuntimeState& State,EGameXXKTeachingChestEvidence Evidence,const TArray<FName>& Inputs);
    static FName StageName(int32 Stage);
    static FName MaterialDropId(int32 Index);
    static bool IsFixedDropId(FName Id);
    /** One-time v44 migration and explicit test setup, never automatic reward repair on open. */
    static bool RestoreOrdinaryChestTokens(FGameXXKRuntimeState& State,FString& Error);
    static bool CompleteEnhancementReview(FGameXXKRuntimeState& State,FString& Error);
};
