#pragma once

#include "CoreMinimal.h"
#include "GameXXKRouteBalanceTypes.h"

/** Expands the locked route-balance matrix without owning a second combat rule set. */
class GAMEXXK_API FGameXXKRouteBalanceRules final
{
public:
	static FGameXXKRouteBalanceMatrix MakeLockedFullMatrix();
	static bool ExpandCases(
		const FGameXXKRouteBalanceMatrix& Matrix,
		TArray<FGameXXKRouteBalanceCase>& OutCases,
		FString* OutError = nullptr);
	static bool RunCase(
		const FGameXXKRouteBalanceCase& Case,
		FGameXXKRouteBalanceCaseResult& OutResult,
		FString* OutError = nullptr,
		const FGameXXKRouteBalanceCalibrationProfile* CalibrationProfile = nullptr,
		TArray<FGameXXKSimulationTraceEntry>* OutTrace = nullptr);
	static bool RunFullMatrix(
		const FGameXXKRouteBalanceMatrix& Matrix,
		FGameXXKRouteBalanceReport& OutReport,
		FString* OutError = nullptr);
};
