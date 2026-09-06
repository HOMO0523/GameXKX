#pragma once
#include "CoreMinimal.h"
#include "GameXXKTrainingSettlementTypes.h"

struct FGameXXKRuntimeState;
struct FGameXXKTrainingReward;
struct FGameXXKCardBattleRuntime;

/** Freezes the existing authoritative reward transaction; never grants a second copy. */
class GAMEXXK_API FGameXXKTrainingSettlementRules
{
public:
	static FGameXXKBattleSessionStats CaptureBattleStats(const FGameXXKCardBattleRuntime& Battle);
	static bool CaptureAppliedResult(const FGameXXKRuntimeState& Before, FGameXXKRuntimeState& After,
		const FGameXXKTrainingReward& Reward, FString* OutError = nullptr);
	static bool ValidatePending(const FGameXXKRuntimeState& State, FString* OutError = nullptr);
	static bool Acknowledge(FGameXXKRuntimeState& State, FGuid ReceiptId, FString* OutError = nullptr);
};
