#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

/** Pure preview plus copy-validated terminal award application for a completed, failed, or abandoned route. */
class GAMEXXK_API FGameXXKRouteSettlementRules
{
public:
	static bool Preview(
		const FGameXXKRuntimeState& State,
		EGameXXKRouteTerminalOutcome Outcome,
		FGameXXKRouteSettlementReceipt& OutReceipt,
		FString* OutError = nullptr);

	/** Applies only the persisted matching pending receipt. Replaying an already applied ID is a safe no-op cleanup. */
	static bool Apply(
		FGameXXKRuntimeState& State,
		const FGameXXKRouteSettlementReceipt& Receipt,
		FString* OutError = nullptr);
};
