#include "MVP/GameXXKMVPSubsystem.h"

#include "MVP/GameXXKSaveGame.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	static const FString DefaultSaveSlotName(TEXT("GameXXK_MVP_Autosave"));

	static FString ResolveSaveSlotName(const FString& SlotName)
	{
		return SlotName.IsEmpty() ? UGameXXKMVPSubsystem::GetDefaultSaveSlotName() : SlotName;
	}
}

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
	return StartGameFromSlot(TEXT(""), 0);
}

bool UGameXXKMVPSubsystem::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	return LoadOrCreateGame(SlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::SaveCurrentGame(FString SlotName, int32 UserIndex)
{
	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(RuntimeState);
	return UGameplayStatics::SaveGameToSlot(SaveGame, ResolveSaveSlotName(SlotName), UserIndex);
}

bool UGameXXKMVPSubsystem::LoadGameFromSlot(FString SlotName, int32 UserIndex)
{
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	if (!UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex))
	{
		return false;
	}

	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(ResolvedSlotName, UserIndex));
	if (!SaveGame)
	{
		return false;
	}

	RuntimeState = UGameXXKMVPRules::RestoreFromSaveState(SaveGame->SaveState);
	return true;
}

bool UGameXXKMVPSubsystem::LoadOrCreateGame(FString SlotName, int32 UserIndex)
{
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	if (UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex))
	{
		if (!LoadGameFromSlot(ResolvedSlotName, UserIndex))
		{
			return false;
		}
		return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
	}

	RuntimeState = UGameXXKMVPRules::CreateNewGame();
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

FString UGameXXKMVPSubsystem::GetDefaultSaveSlotName()
{
	return DefaultSaveSlotName;
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
