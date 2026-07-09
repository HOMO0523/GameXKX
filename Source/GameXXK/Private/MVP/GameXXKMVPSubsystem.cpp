#include "MVP/GameXXKMVPSubsystem.h"

#include "MVP/GameXXKSaveGame.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	static constexpr int32 ManualSaveSlotCount = 5;
	static const FString ManualSaveSlotPrefix(TEXT("GameXXK_MVP_SaveSlot_"));
	static const FString DefaultSaveSlotName(TEXT("GameXXK_MVP_SaveSlot_1"));

	static FString ResolveSaveSlotName(const FString& SlotName)
	{
		return SlotName.IsEmpty() ? UGameXXKMVPSubsystem::GetDefaultSaveSlotName() : SlotName;
	}

	static APawn* GetLivePlayerPawnForSave(const UGameXXKMVPSubsystem* Subsystem)
	{
		UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
		return World && World->IsGameWorld() ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
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
	return StartNewGame();
}

bool UGameXXKMVPSubsystem::StartNewGame()
{
	RuntimeState = UGameXXKMVPRules::CreateNewGame();
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	return ContinueGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::ContinueGameFromSlot(FString SlotName, int32 UserIndex)
{
	return LoadGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::SaveCurrentGame(FString SlotName, int32 UserIndex)
{
	if (APawn* PlayerPawn = GetLivePlayerPawnForSave(this))
	{
		RecordPlayerLocation(PlayerPawn->GetActorLocation());
	}

	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(RuntimeState);
	return UGameplayStatics::SaveGameToSlot(SaveGame, ResolveSaveSlotName(SlotName), UserIndex);
}

bool UGameXXKMVPSubsystem::DoesSaveGameExist(FString SlotName, int32 UserIndex) const
{
	return UGameplayStatics::DoesSaveGameExist(ResolveSaveSlotName(SlotName), UserIndex);
}

bool UGameXXKMVPSubsystem::DeleteSaveGame(FString SlotName, int32 UserIndex)
{
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	return UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex)
		&& UGameplayStatics::DeleteGameInSlot(ResolvedSlotName, UserIndex);
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
		return ContinueGameFromSlot(ResolvedSlotName, UserIndex);
	}

	return StartNewGame();
}

FString UGameXXKMVPSubsystem::GetDefaultSaveSlotName()
{
	return DefaultSaveSlotName;
}

int32 UGameXXKMVPSubsystem::GetManualSaveSlotCount()
{
	return ManualSaveSlotCount;
}

FString UGameXXKMVPSubsystem::GetManualSaveSlotName(int32 SlotIndex)
{
	const int32 ClampedSlotIndex = FMath::Clamp(SlotIndex, 0, ManualSaveSlotCount - 1);
	return FString::Printf(TEXT("%s%d"), *ManualSaveSlotPrefix, ClampedSlotIndex + 1);
}

bool UGameXXKMVPSubsystem::OpenWorldMap()
{
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectWorldRegion(FName RegionId)
{
	return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, RegionId);
}

bool UGameXXKMVPSubsystem::EnsureQingshanTownRuntimeForDirectMap()
{
	if (RuntimeState.Screen == EGameXXKScreen::Town && RuntimeState.CurrentRegion == UGameXXKMVPRules::RegionQingshan())
	{
		return true;
	}
	if (RuntimeState.Screen == EGameXXKScreen::MainMenu)
	{
		RuntimeState = UGameXXKMVPRules::CreateNewGame();
		UGameXXKMVPRules::OpenWorldMap(RuntimeState);
		return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, UGameXXKMVPRules::RegionQingshan());
	}
	if (RuntimeState.Screen == EGameXXKScreen::WorldMap)
	{
		return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, UGameXXKMVPRules::RegionQingshan());
	}
	return false;
}

bool UGameXXKMVPSubsystem::IsRegionUnlocked(FName RegionId) const
{
	return RuntimeState.UnlockedRegions.Contains(RegionId);
}

bool UGameXXKMVPSubsystem::AcceptQuest()
{
	return UGameXXKMVPRules::AcceptTownQuest(RuntimeState);
}

void UGameXXKMVPSubsystem::RecordQuestNpcLocation(FVector Location)
{
	RuntimeState.bHasQuestNpcLocation = true;
	RuntimeState.QuestNpcLocation = Location;
}

void UGameXXKMVPSubsystem::RecordPlayerLocation(FVector Location)
{
	RuntimeState.bHasPlayerLocation = true;
	RuntimeState.PlayerLocation = Location;
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

bool UGameXXKMVPSubsystem::SelectRouteNodeById(int32 NodeId)
{
	return UGameXXKMVPRules::SelectRouteNodeById(RuntimeState, NodeId);
}

bool UGameXXKMVPSubsystem::ResolveBattleVictory(bool bBossBattle)
{
	return UGameXXKMVPRules::ResolveBattleVictory(RuntimeState, bBossBattle);
}

bool UGameXXKMVPSubsystem::ExecuteBattleBasicAttack(int32 PartyIndex, int32 EnemyIndex)
{
	return UGameXXKMVPRules::ExecuteBattleBasicAttack(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleCraneWingSlash(int32 PartyIndex, int32 EnemyIndex)
{
	return UGameXXKMVPRules::ExecuteBattleCraneWingSlash(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleGuiyuanArt(int32 PartyIndex)
{
	return UGameXXKMVPRules::ExecuteBattleGuiyuanArt(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleDefend(int32 PartyIndex)
{
	return UGameXXKMVPRules::ExecuteBattleDefend(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleHealingPowder(int32 PartyIndex)
{
	return UGameXXKMVPRules::ExecuteBattleHealingPowder(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ResolveEventReward(bool bTakeGold)
{
	return UGameXXKMVPRules::ResolveEventReward(RuntimeState, bTakeGold);
}

bool UGameXXKMVPSubsystem::ResolveCampReward(bool bHealNow)
{
	return UGameXXKMVPRules::ResolveCampReward(RuntimeState, bHealNow);
}

bool UGameXXKMVPSubsystem::ResolveMerchantRouteNode()
{
	return UGameXXKMVPRules::ResolveMerchantRouteNode(RuntimeState);
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

bool UGameXXKMVPSubsystem::UseItem(FName ItemId)
{
	return UGameXXKMVPRules::UseItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::EquipItem(FName ItemId)
{
	return UGameXXKMVPRules::EquipItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::OpenTownPanel(EGameXXKTownPanelMode PanelMode)
{
	return UGameXXKMVPRules::OpenTownPanel(RuntimeState, PanelMode);
}

bool UGameXXKMVPSubsystem::CloseTownPanel()
{
	return UGameXXKMVPRules::CloseTownPanel(RuntimeState);
}

int32 UGameXXKMVPSubsystem::GetItemCount(FName ItemId) const
{
	return UGameXXKMVPRules::GetItemCount(RuntimeState, ItemId);
}

TArray<FName> UGameXXKMVPSubsystem::BuildTurnOrder(bool bBossBattle) const
{
	return UGameXXKMVPRules::BuildTurnOrder(RuntimeState, bBossBattle);
}
