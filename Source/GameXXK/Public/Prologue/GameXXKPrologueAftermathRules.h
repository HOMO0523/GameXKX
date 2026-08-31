#pragma once

#include "CoreMinimal.h"
#include "Prologue/GameXXKPrologueAftermathTypes.h"

class GAMEXXK_API FGameXXKPrologueAftermathRules final
{
public:
	static bool Start(FGameXXKPrologueAftermathState& InOutState);
	static bool ApplyEvent(
		EGameXXKPrologueAftermathEvent Event,
		FGameXXKPrologueAftermathState& InOutState);
	static void SetPaused(
		FGameXXKPrologueAftermathState& InOutState,
		bool bPaused);
	static bool IsBlockingPhase(EGameXXKPrologueAftermathPhase Phase);
};
