#pragma once

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"

namespace GameXXKPermanentPartyTestFixtures
{
    inline void SkipTeachingChests(FGameXXKRuntimeState& State)
    {
        auto& Progress=State.GuideProgress.TeachingChests;
        if(!Progress.bEnabled)return;
        if(!Progress.ReservedWeapon.InstanceId.IsNone())
        {
            State.EquipmentCollection.EquipmentInstances.Add(Progress.ReservedWeapon);
            State.EquipmentCollection.WarehouseInstanceIds.Add(Progress.ReservedWeapon.InstanceId);
        }
        Progress=FGameXXKTeachingChestProgress();
        State.Training.OwnedChestTokens.RemoveAll([](const auto& Token){return !Token.FixedDropId.IsNone();});
        FGameXXKDesktopInventoryRules::Normalize(State);
        FGameXXKTrainingRules::StartTravel(State.Training,TEXT("Training.Normal.1-1"));
    }
	/**
	 * Opts an already-started runtime state out of the party-slot onboarding and
	 * materializes the established full party (hero, first owned partner, named NPC).
	 * Use this in fixtures that exercise the established full-party systems rather
	 * than the onboarding progression added by the progression-based party slots.
	 */
	inline void AdoptEstablishedParty(FGameXXKRuntimeState& State)
	{
		State.Training.bProgressivePartySlots=false;
		FGameXXKPartyFormationRules::BuildLegacyProjection(State,State.CardRun.OrderedFormation);
		FGameXXKPartyFormationRules::ProjectCompatibility(State);
	}

	inline void AdoptEstablishedParty(UGameXXKMVPSubsystem& Subsystem)
	{
		AdoptEstablishedParty(Subsystem.GetMutableRuntimeState());
	}

	inline FGameXXKRuntimeState MakeStartedState()
	{
		UGameXXKMVPSubsystem* Subsystem =
			NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		auto State = Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
        // This fixture exercises the established full-party systems, not onboarding.
        SkipTeachingChests(State);
        AdoptEstablishedParty(State);
        return State;
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
