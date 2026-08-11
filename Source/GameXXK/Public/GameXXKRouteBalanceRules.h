#pragma once

#include "CoreMinimal.h"
#include "GameXXKRouteBalanceTypes.h"

/** Expands the locked route-balance matrix without owning a second combat rule set. */
class GAMEXXK_API FGameXXKRouteBalanceRules final
{
public:
	static FGameXXKRouteBalanceMatrix MakeLockedFullMatrix();
	/** Builds five deterministic, single-variable diagnostic dimensions (2,520 total cases). */
	static bool MakeOrthogonalCases(
		TArray<FGameXXKRouteBalanceCase>& OutCases,
		FString* OutError = nullptr);
	/** Builds a naked plus 3-quality x 3-item-level grid at the fixed chapter-two control point. */
	static bool MakeEquipmentBudgetCases(
		TArray<FGameXXKRouteBalanceCase>& OutCases,
		FString* OutError = nullptr);
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
