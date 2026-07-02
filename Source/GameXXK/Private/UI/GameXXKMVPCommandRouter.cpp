#include "UI/GameXXKMVPCommandRouter.h"

#include "GameXXKMVPRules.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	static const FName StartGame(TEXT("StartGame"));
	static const FName SelectQingshan(TEXT("SelectQingshan"));
	static const FName SelectTanjiang(TEXT("SelectTanjiang"));
	static const FName OpenWorldMap(TEXT("OpenWorldMap"));
	static const FName AcceptQuest(TEXT("AcceptQuest"));
	static const FName BuyHealingPowder(TEXT("BuyHealingPowder"));
	static const FName SellHealingPowder(TEXT("SellHealingPowder"));
	static const FName UseHealingPowder(TEXT("UseHealingPowder"));
	static const FName EnterDungeon(TEXT("EnterDungeon"));
	static const FName SelectStart(TEXT("SelectStart"));
	static const FName SelectBattle(TEXT("SelectBattle"));
	static const FName ResolveEventGold(TEXT("ResolveEventGold"));
	static const FName ResolveCampHeal(TEXT("ResolveCampHeal"));
	static const FName SelectBoss(TEXT("SelectBoss"));
	static const FName ResolveBattleVictory(TEXT("ResolveBattleVictory"));
	static const FName FailBattle(TEXT("FailBattle"));
	static const FName QingshanTownMap(TEXT("/Game/GameXXK/Maps/L_QingshanInn"));

	static void AddCommand(TArray<FGameXXKMVPCommandDescriptor>& Commands, FName Name, const TCHAR* Label, bool bEnabled)
	{
		Commands.Emplace(Name, FText::FromString(Label), bEnabled);
	}

	static EGameXXKNodeKind GetCurrentNode(const FGameXXKRuntimeState& State)
	{
		const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		return Nodes.IsValidIndex(State.DungeonNodeIndex) ? Nodes[State.DungeonNodeIndex] : EGameXXKNodeKind::Start;
	}

	static FString ScreenToString(EGameXXKScreen Screen)
	{
		switch (Screen)
		{
		case EGameXXKScreen::MainMenu:
			return TEXT("Main Menu");
		case EGameXXKScreen::WorldMap:
			return TEXT("World Map");
		case EGameXXKScreen::Town:
			return TEXT("Town");
		case EGameXXKScreen::DungeonMap:
			return TEXT("Dungeon Map");
		case EGameXXKScreen::Battle:
			return TEXT("Battle");
		default:
			return TEXT("Unknown");
		}
	}

	static FString QuestToString(EGameXXKQuestState QuestState)
	{
		switch (QuestState)
		{
		case EGameXXKQuestState::NotAccepted:
			return TEXT("Not Accepted");
		case EGameXXKQuestState::Accepted:
			return TEXT("Accepted");
		case EGameXXKQuestState::Completed:
			return TEXT("Completed");
		default:
			return TEXT("Unknown");
		}
	}

	static bool SavePlayableSlot(UGameXXKMVPSubsystem* Subsystem, const FString& SlotName, int32 UserIndex)
	{
		if (!Subsystem)
		{
			return false;
		}
		return SlotName.IsEmpty()
			? Subsystem->SaveCurrentGame(TEXT(""), 0)
			: Subsystem->SaveCurrentGame(SlotName, UserIndex);
	}

	static void OpenPlayableTownMap(UGameXXKMVPSubsystem* Subsystem, FName MapName)
	{
		UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
		if (!World || !World->IsGameWorld())
		{
			return;
		}

		UGameplayStatics::OpenLevel(World, MapName);
	}
}

TArray<FGameXXKMVPCommandDescriptor> GameXXKMVPCommandRouter::BuildVisibleCommands(const UGameXXKMVPSubsystem* Subsystem)
{
	TArray<FGameXXKMVPCommandDescriptor> Commands;
	if (!Subsystem)
	{
		return Commands;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	switch (State.Screen)
	{
	case EGameXXKScreen::MainMenu:
		AddCommand(Commands, StartGame, TEXT("Start / Continue"), true);
		break;
	case EGameXXKScreen::WorldMap:
		AddCommand(Commands, SelectQingshan, TEXT("Qingshan Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionQingshan()));
		AddCommand(Commands, SelectTanjiang, TEXT("Tanjiang Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
		break;
	case EGameXXKScreen::Town:
		AddCommand(Commands, OpenWorldMap, TEXT("World Map"), true);
		AddCommand(Commands, AcceptQuest, TEXT("Accept Route Quest"), State.QuestState == EGameXXKQuestState::NotAccepted);
		AddCommand(Commands, BuyHealingPowder, TEXT("Buy Healing Powder"), State.PlayerGold >= 10);
		AddCommand(Commands, SellHealingPowder, TEXT("Sell Healing Powder"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0);
		AddCommand(Commands, UseHealingPowder, TEXT("Use Healing Powder"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0 && State.PlayerHP < State.PlayerMaxHP);
		AddCommand(Commands, EnterDungeon, TEXT("Enter Route Map"), Subsystem->CanEnterDungeon());
		break;
	case EGameXXKScreen::DungeonMap:
		switch (GetCurrentNode(State))
		{
		case EGameXXKNodeKind::Start:
			AddCommand(Commands, SelectStart, TEXT("Start Node"), true);
			break;
		case EGameXXKNodeKind::Battle:
			AddCommand(Commands, SelectBattle, TEXT("Battle Node"), true);
			break;
		case EGameXXKNodeKind::Event:
			AddCommand(Commands, ResolveEventGold, TEXT("Event: Take Gold"), true);
			break;
		case EGameXXKNodeKind::Camp:
			AddCommand(Commands, ResolveCampHeal, TEXT("Camp: Heal"), true);
			break;
		case EGameXXKNodeKind::Boss:
			AddCommand(Commands, SelectBoss, TEXT("Boss Node"), true);
			break;
		default:
			break;
		}
		break;
	case EGameXXKScreen::Battle:
		AddCommand(Commands, ResolveBattleVictory, TEXT("Win Battle"), true);
		AddCommand(Commands, FailBattle, TEXT("Fail / Return Town"), State.bDungeonActive);
		break;
	default:
		break;
	}

	return Commands;
}

bool GameXXKMVPCommandRouter::ExecuteVisibleCommand(UGameXXKMVPSubsystem* Subsystem, FName CommandName, const FString& SlotName, int32 UserIndex, bool bAutosave)
{
	if (!Subsystem)
	{
		return false;
	}

	const TArray<FGameXXKMVPCommandDescriptor> VisibleCommands = BuildVisibleCommands(Subsystem);
	const FGameXXKMVPCommandDescriptor* MatchingCommand = VisibleCommands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});
	if (!MatchingCommand || !MatchingCommand->bEnabled)
	{
		return false;
	}

	auto FinishCommand = [Subsystem, &SlotName, UserIndex, bAutosave](bool bCommandSucceeded)
	{
		if (!bCommandSucceeded)
		{
			return false;
		}
		return bAutosave ? SavePlayableSlot(Subsystem, SlotName, UserIndex) : true;
	};

	if (CommandName == StartGame)
	{
		return FinishCommand(SlotName.IsEmpty()
			? Subsystem->StartGame()
			: Subsystem->StartGameFromSlot(SlotName, UserIndex));
	}
	if (CommandName == SelectQingshan)
	{
		const bool bSucceeded = FinishCommand(Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
		if (bSucceeded)
		{
			OpenPlayableTownMap(Subsystem, QingshanTownMap);
		}
		return bSucceeded;
	}
	if (CommandName == SelectTanjiang)
	{
		return FinishCommand(Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionTanjiang()));
	}
	if (CommandName == OpenWorldMap)
	{
		return FinishCommand(Subsystem->OpenWorldMap());
	}
	if (CommandName == AcceptQuest)
	{
		return FinishCommand(Subsystem->AcceptQuest());
	}
	if (CommandName == BuyHealingPowder)
	{
		return FinishCommand(Subsystem->BuyItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	}
	if (CommandName == SellHealingPowder)
	{
		return FinishCommand(Subsystem->SellItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	}
	if (CommandName == UseHealingPowder)
	{
		return FinishCommand(Subsystem->UseHealingItem());
	}
	if (CommandName == EnterDungeon)
	{
		return FinishCommand(Subsystem->OpenDungeonFromTownExit());
	}
	if (CommandName == SelectStart)
	{
		return FinishCommand(Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start));
	}
	if (CommandName == SelectBattle)
	{
		return FinishCommand(Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
	}
	if (CommandName == ResolveEventGold)
	{
		return FinishCommand(Subsystem->ResolveEventReward(true));
	}
	if (CommandName == ResolveCampHeal)
	{
		return FinishCommand(Subsystem->ResolveCampReward(true));
	}
	if (CommandName == SelectBoss)
	{
		return FinishCommand(Subsystem->SelectDungeonNode(EGameXXKNodeKind::Boss));
	}
	if (CommandName == ResolveBattleVictory)
	{
		const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
		return FinishCommand(Subsystem->ResolveBattleVictory(State.bDungeonActive && GetCurrentNode(State) == EGameXXKNodeKind::Boss));
	}
	if (CommandName == FailBattle)
	{
		return FinishCommand(Subsystem->FailDungeonToTown());
	}

	return false;
}

FText GameXXKMVPCommandRouter::BuildStatusText(const UGameXXKMVPSubsystem* Subsystem)
{
	if (!Subsystem)
	{
		return FText::FromString(TEXT("GameXXK MVP"));
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FString Summary = FString::Printf(
		TEXT("Screen: %s | Quest: %s | Level %d XP %d | Gold %d | HP %d/%d | Follower: %s"),
		*ScreenToString(State.Screen),
		*QuestToString(State.QuestState),
		State.PlayerLevel,
		State.PlayerXP,
		State.PlayerGold,
		State.PlayerHP,
		State.PlayerMaxHP,
		State.bFollowerJoined ? TEXT("Yes") : TEXT("No"));
	return FText::FromString(Summary);
}
