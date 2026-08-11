#pragma once

#include "CoreMinimal.h"
#include "GameXXKCombatSimulationTypes.h"

/** Deterministic policy runner. It delegates every battle mutation to FGameXXKCardBattleAdapter. */
class GAMEXXK_API FGameXXKCombatSimulationRules final
{
public:
	static bool RunScenario(
		const FGameXXKSimulationScenario& Scenario,
		FGameXXKSimulationMetrics& OutMetrics,
		TArray<FGameXXKSimulationTraceEntry>& OutTrace,
		FString* OutError = nullptr);

#if WITH_DEV_AUTOMATION_TESTS
	/** Deterministic read-only policy seam; production simulation uses the same private chooser. */
	static bool ChooseSkilledDecisionForTest(
		const FGameXXKRuntimeState& State,
		FGameXXKSimulationDecision& OutDecision,
		FString* OutError = nullptr);
#endif
};
