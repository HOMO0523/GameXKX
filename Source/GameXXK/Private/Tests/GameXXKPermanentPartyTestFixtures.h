#pragma once

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"

namespace GameXXKPermanentPartyTestFixtures
{
	inline FGameXXKRuntimeState MakeStartedState()
	{
		UGameXXKMVPSubsystem* Subsystem =
			NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		return Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
	}

	inline bool SelectNpc(
		FGameXXKRuntimeState& State,
		const FName NpcId,
		FString* OutError = nullptr)
	{
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, OutError))
		{
			return false;
		}
		return FGameXXKPartyFormationRules::SetQuestNpc(State, NpcId, OutError);
	}

	inline FName ResolveNpc(const FGameXXKRuntimeState& State)
	{
		FName NpcId;
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcId);
		return NpcId;
	}
}
