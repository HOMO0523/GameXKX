#pragma once

#include "CoreMinimal.h"
#include "GameXXKTalentTypes.h"

struct FGameXXKRuntimeState;

class GAMEXXK_API FGameXXKTalentRules final
{
public:
	static constexpr int32 MaximumCostTier = 35;
	static constexpr int32 PhysicalBackpackCapacity = 200;
	static constexpr int32 PhysicalWarehouseCapacity = 200;

	static int64 GetPriceForCostTier(int32 CostTier);
	static int64 GetRankPrice(const FGameXXKTalentNodeDefinition& Node, int32 CurrentRank);
	static int64 GetFullCapacityPathPrice();
	static bool ValidateProgress(const FGameXXKTalentProgress& Progress, FString* OutError = nullptr);
	static bool BuildProjection(const FGameXXKTalentProgress& Progress, FGameXXKTalentProjection& OutProjection, FString* OutError = nullptr);
	static bool IsRevealed(const FGameXXKTalentProgress& Progress, const FGameXXKTalentNodeDefinition& Node);
	static bool ArePrerequisitesMet(const FGameXXKTalentProgress& Progress, const FGameXXKTalentNodeDefinition& Node, FText* OutReason = nullptr);
	static TArray<FGameXXKTalentNodeView> BuildNodeViews(const FGameXXKRuntimeState& State);
	static bool Purchase(FGameXXKRuntimeState& InOutState, FName NodeId, FGameXXKTalentPurchaseResult& OutResult);
	static int32 GetUnlockedBackpackCapacity(const FGameXXKRuntimeState& State);
	static int32 GetUnlockedWarehousePageCount(const FGameXXKRuntimeState& State);
	static int32 GetUnlockedWarehouseCapacity(const FGameXXKRuntimeState& State);
	static FText DescribeEffect(const FGameXXKTalentNodeDefinition& Node, int32 RankDelta = 1);
};
