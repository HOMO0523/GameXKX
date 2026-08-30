#pragma once

#include "CoreMinimal.h"
#include "Prologue/GameXXKPrologueCarriageTypes.h"

class GAMEXXK_API FGameXXKPrologueCarriageRules final
{
public:
	static bool Start(FGameXXKPrologueCarriageState& InOutState);
	static bool Advance(
		float DeltaSeconds,
		const FGameXXKPrologueCarriageConfig& Config,
		FGameXXKPrologueCarriageState& InOutState,
		FGameXXKPrologueCarriageStepOutput& OutStep);
	static void SetPaused(FGameXXKPrologueCarriageState& InOutState, bool bPaused);
	static bool Cancel(FGameXXKPrologueCarriageState& InOutState);
	static bool ConsumeFinishBroadcast(FGameXXKPrologueCarriageState& InOutState);
	static int32 ResolveAtlasFrame(
		EGameXXKPrologueCarriagePhase Phase,
		float PhaseElapsedSeconds,
		const FGameXXKPrologueCarriageConfig& Config);
};
