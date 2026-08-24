#pragma once

#include "CoreMinimal.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"

enum class EGameXXKToolCombineKind : uint8
{
	Equipment,
	Gem
};

struct GAMEXXK_API FGameXXKToolInputRef
{
	EGameXXKDesktopItemContainer Container = EGameXXKDesktopItemContainer::Backpack;
	int32 SlotIndex = INDEX_NONE;
	FGameXXKDesktopInventoryEntryKey ExpectedEntry;
};

struct GAMEXXK_API FGameXXKSocketGemRequest
{
	FGameXXKToolInputRef EquipmentInput;
	FGameXXKToolInputRef GemInput;
	int32 SocketIndex = INDEX_NONE;
};

/** Atomic rules for progression and all five equipment workbench modes. */
class GAMEXXK_API FGameXXKEquipmentToolRules final
{
public:
	static constexpr int32 MinimumLevel = 1;
	static constexpr int32 MaximumLevel = 10;

	static int64 GetQualityExperienceMultiplier(int32 QualityRank);
	static int64 GetExperienceForNextLevel(int32 CurrentLevel);
	static FInt32Interval GetCraftedItemLevelRange(int32 SelectedCraftingLevel);
	static bool NormalizeProgress(FGameXXKToolProgress& InOutProgress);
	static bool ValidateProgress(const FGameXXKToolProgress& Progress, FString* OutError = nullptr);
	static bool AddExperience(FGameXXKToolProgress& InOutProgress, int64 BaseAward, int32 QualityRank, int64* OutAward = nullptr);
	static bool AddRawExperience(FGameXXKToolProgress& InOutProgress, int64 Award);

	static bool Dismantle(FGameXXKRuntimeState& InOutState, const TArray<FGameXXKToolInputRef>& Inputs, bool bConfirmed, FGameXXKEquipmentTransactionResult& OutResult);
	static bool CombineEquipment(FGameXXKRuntimeState& InOutState, const TArray<FGameXXKToolInputRef>& Inputs, FGameXXKEquipmentTransactionResult& OutResult);
	static bool CombineGem(FGameXXKRuntimeState& InOutState, const FGameXXKToolInputRef& Input, FGameXXKEquipmentTransactionResult& OutResult);
	static bool Enhance(FGameXXKRuntimeState& InOutState, const FGameXXKToolInputRef& Input, FGameXXKEquipmentTransactionResult& OutResult);
	static bool BeginReforge(FGameXXKRuntimeState& InOutState, const FGameXXKToolInputRef& Input, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);
	static bool ResolveReforge(FGameXXKRuntimeState& InOutState, bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);
	static bool SocketGem(FGameXXKRuntimeState& InOutState, const FGameXXKSocketGemRequest& Request, FGameXXKEquipmentTransactionResult& OutResult);
	static bool BuildCombineAutoFill(const FGameXXKRuntimeState& State, EGameXXKToolCombineKind Kind, bool bIncludeWarehouse, TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError = nullptr);
};
