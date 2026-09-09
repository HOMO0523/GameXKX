#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

enum class EGameXXKSfxCue : uint8
{
	None, HitLight, HitHeavy, CardPlay, Block, Lightning, Fire, Frost,
	Heal, Down, Victory, Defeat, Reward, Button, Tool
};

/** Presentation-only routing. Never changes combat, inventory, or save state. */
class GAMEXXK_API FGameXXKSfxPolicy
{
public:
	static EGameXXKSfxCue ResolveImpact(const FGameXXKBattlePresentationEvent& Event);
	static bool HasActualHealing(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After);
	bool Accept(EGameXXKSfxCue Cue, double NowSeconds, bool bMuted);
	void Reset() { LastAccepted.Reset(); }

private:
	TMap<EGameXXKSfxCue, double> LastAccepted;
};
