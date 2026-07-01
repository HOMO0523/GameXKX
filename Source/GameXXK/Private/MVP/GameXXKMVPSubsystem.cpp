#include "MVP/GameXXKMVPSubsystem.h"

UGameXXKMVPSubsystem::UGameXXKMVPSubsystem()
{
	RuntimeState = UGameXXKMVPRules::CreateNewGame();
}

const FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetRuntimeState() const
{
	return RuntimeState;
}

FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetMutableRuntimeState()
{
	return RuntimeState;
}

FGameXXKRuntimeState UGameXXKMVPSubsystem::GetRuntimeStateCopy() const
{
	return RuntimeState;
}

bool UGameXXKMVPSubsystem::StartGame()
{
	RuntimeState = UGameXXKMVPRules::CreateNewGame();
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::OpenWorldMap()
{
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectWorldRegion(FName RegionId)
{
	return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, RegionId);
}

bool UGameXXKMVPSubsystem::IsRegionUnlocked(FName RegionId) const
{
	return RuntimeState.UnlockedRegions.Contains(RegionId);
}

bool UGameXXKMVPSubsystem::AcceptQuest()
{
	return UGameXXKMVPRules::AcceptTownQuest(RuntimeState);
}

bool UGameXXKMVPSubsystem::CanEnterDungeon() const
{
	return UGameXXKMVPRules::CanEnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::OpenDungeonFromTownExit()
{
	return UGameXXKMVPRules::EnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectDungeonNode(EGameXXKNodeKind ExpectedNode)
{
	return UGameXXKMVPRules::AdvanceDungeonNode(RuntimeState, ExpectedNode);
}

bool UGameXXKMVPSubsystem::ResolveBattleVictory(bool bBossBattle)
{
	return UGameXXKMVPRules::ResolveBattleVictory(RuntimeState, bBossBattle);
}

bool UGameXXKMVPSubsystem::ResolveEventReward(bool bTakeGold)
{
	return UGameXXKMVPRules::ResolveEventReward(RuntimeState, bTakeGold);
}

bool UGameXXKMVPSubsystem::ResolveCampReward(bool bHealNow)
{
	return UGameXXKMVPRules::ResolveCampReward(RuntimeState, bHealNow);
}

bool UGameXXKMVPSubsystem::FailDungeonToTown()
{
	return UGameXXKMVPRules::FailDungeonToTown(RuntimeState);
}

bool UGameXXKMVPSubsystem::BuyItem(FName ItemId, int32 Quantity)
{
	return UGameXXKMVPRules::BuyItem(RuntimeState, ItemId, Quantity);
}

bool UGameXXKMVPSubsystem::SellItem(FName ItemId, int32 Quantity)
{
	return UGameXXKMVPRules::SellItem(RuntimeState, ItemId, Quantity);
}

bool UGameXXKMVPSubsystem::UseHealingItem()
{
	return UGameXXKMVPRules::UseHealingItem(RuntimeState);
}

bool UGameXXKMVPSubsystem::EquipItem(FName ItemId)
{
	return UGameXXKMVPRules::EquipItem(RuntimeState, ItemId);
}

int32 UGameXXKMVPSubsystem::GetItemCount(FName ItemId) const
{
	return UGameXXKMVPRules::GetItemCount(RuntimeState, ItemId);
}

TArray<FName> UGameXXKMVPSubsystem::BuildTurnOrder(bool bBossBattle) const
{
	return UGameXXKMVPRules::BuildTurnOrder(RuntimeState, bBossBattle);
}
