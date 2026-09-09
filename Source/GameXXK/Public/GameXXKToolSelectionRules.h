#pragma once
#include "GameXXKEquipmentToolRules.h"

/** Read-only selection shared by manual placement and anchored auto-fill. */
class GAMEXXK_API FGameXXKToolSelectionRules final
{
public:
    static int32 GetEntryQualityRank(const FGameXXKRuntimeState& State, const FGameXXKDesktopInventoryEntryKey& Entry);
    static bool BuildAutoFillForSelection(const FGameXXKRuntimeState& State, EGameXXKToolCombineKind Kind,
        bool bIncludeWarehouse, bool bDismantle, const TArray<FGameXXKToolInputRef>& Existing,
        TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError = nullptr);
};
