#include "UI/GameXXKMVPCommandRouter.h"

#include "GameXXKMVPRules.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	static const FName StartGame(TEXT("StartGame"));
	static const FName ContinueGame(TEXT("ContinueGame"));
	static const FName SelectQingshan(TEXT("SelectQingshan"));
	static const FName SelectTanjiang(TEXT("SelectTanjiang"));
	static const FName OpenWorldMap(TEXT("OpenWorldMap"));
	static const FName AcceptQuest(TEXT("AcceptQuest"));
	static const FName SaveGame(TEXT("SaveGame"));
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

	static FName MakeRouteNodeCommandName(int32 NodeId)
	{
		return FName(*FString::Printf(TEXT("RouteNode%d"), NodeId));
	}

	static bool TryGetRouteNodeCommandId(FName CommandName, int32& OutNodeId)
	{
		const FString CommandString = CommandName.ToString();
		const FString PrefixString(TEXT("RouteNode"));
		if (!CommandString.StartsWith(PrefixString))
		{
			return false;
		}
		const FString IndexString = CommandString.RightChop(PrefixString.Len());
		if (!IndexString.IsNumeric())
		{
			return false;
		}
		OutNodeId = FCString::Atoi(*IndexString);
		return OutNodeId >= 0;
	}

	static FName MakeIndexedCommandName(const TCHAR* Prefix, int32 SlotIndex)
	{
		return FName(*FString::Printf(TEXT("%s%d"), Prefix, SlotIndex + 1));
	}

	static bool TryGetIndexedCommandSlot(FName CommandName, const TCHAR* Prefix, int32& OutSlotIndex)
	{
		const FString CommandString = CommandName.ToString();
		const FString PrefixString(Prefix);
		if (!CommandString.StartsWith(PrefixString))
		{
			return false;
		}

		const FString IndexString = CommandString.RightChop(PrefixString.Len());
		if (!IndexString.IsNumeric())
		{
			return false;
		}

		const int32 OneBasedIndex = FCString::Atoi(*IndexString);
		if (OneBasedIndex < 1 || OneBasedIndex > UGameXXKMVPSubsystem::GetManualSaveSlotCount())
		{
			return false;
		}

		OutSlotIndex = OneBasedIndex - 1;
		return true;
	}

	static void AddCommand(TArray<FGameXXKMVPCommandDescriptor>& Commands, FName Name, const TCHAR* Label, bool bEnabled)
	{
		Commands.Emplace(Name, FText::FromString(Label), bEnabled);
	}

	static void AddSaveSlotCommands(TArray<FGameXXKMVPCommandDescriptor>& Commands)
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			Commands.Emplace(
				MakeIndexedCommandName(TEXT("SaveSlot"), SlotIndex),
				FText::FromString(FString::Printf(TEXT("Save Slot %d"), SlotIndex + 1)),
				true);
		}
	}

	static void AddContinueSlotCommands(TArray<FGameXXKMVPCommandDescriptor>& Commands, const UGameXXKMVPSubsystem* Subsystem, int32 UserIndex)
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
			const bool bSlotExists = Subsystem && Subsystem->DoesSaveGameExist(SlotName, UserIndex);
			Commands.Emplace(
				MakeIndexedCommandName(TEXT("ContinueSlot"), SlotIndex),
				FText::FromString(FString::Printf(TEXT("Slot %d"), SlotIndex + 1)),
				bSlotExists);
		}
	}

	static void AddDeleteSlotCommands(TArray<FGameXXKMVPCommandDescriptor>& Commands, const UGameXXKMVPSubsystem* Subsystem, int32 UserIndex)
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
			const bool bSlotExists = Subsystem && Subsystem->DoesSaveGameExist(SlotName, UserIndex);
			Commands.Emplace(
				MakeIndexedCommandName(TEXT("DeleteSlot"), SlotIndex),
				FText::FromString(FString::Printf(TEXT("Delete Slot %d"), SlotIndex + 1)),
				bSlotExists);
		}
	}

	static FName CommandForNode(EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Start:
			return SelectStart;
		case EGameXXKNodeKind::Battle:
			return SelectBattle;
		case EGameXXKNodeKind::Elite:
			return SelectBattle;
		case EGameXXKNodeKind::Event:
		case EGameXXKNodeKind::Chest:
		case EGameXXKNodeKind::Merchant:
			return ResolveEventGold;
		case EGameXXKNodeKind::Camp:
			return ResolveCampHeal;
		case EGameXXKNodeKind::Boss:
			return SelectBoss;
		default:
			return NAME_None;
		}
	}

	static const TCHAR* LabelForNode(EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Start:
			return TEXT("Start");
		case EGameXXKNodeKind::Battle:
			return TEXT("Battle");
		case EGameXXKNodeKind::Elite:
			return TEXT("Elite");
		case EGameXXKNodeKind::Event:
			return TEXT("Event");
		case EGameXXKNodeKind::Camp:
			return TEXT("Camp");
		case EGameXXKNodeKind::Chest:
			return TEXT("Chest");
		case EGameXXKNodeKind::Merchant:
			return TEXT("Merchant");
		case EGameXXKNodeKind::Boss:
			return TEXT("Boss");
		default:
			return TEXT("Node");
		}
	}

	static EGameXXKNodeKind GetCurrentNode(const FGameXXKRuntimeState& State)
	{
		if (State.bHasGeneratedRouteMap)
		{
			const int32 NodeId = State.PendingRouteNodeId != INDEX_NONE
				? State.PendingRouteNodeId
				: (State.ReachableRouteNodeIds.IsEmpty() ? INDEX_NONE : State.ReachableRouteNodeIds[0]);
			const FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Candidate)
			{
				return Candidate.NodeId == NodeId;
			});
			return Node ? Node->NodeKind : EGameXXKNodeKind::Start;
		}
		const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		return Nodes.IsValidIndex(State.DungeonNodeIndex) ? Nodes[State.DungeonNodeIndex] : EGameXXKNodeKind::Start;
	}

	static bool HasReachableNodeKind(const FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind)
	{
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Candidate)
			{
				return Candidate.NodeId == NodeId;
			});
			if (Node && Node->NodeKind == NodeKind)
			{
				return true;
			}
		}
		return false;
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

	static bool FinishCommandAndTravel(UGameXXKMVPSubsystem* Subsystem, bool bCommandSucceeded)
	{
		if (bCommandSucceeded)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		return bCommandSucceeded;
	}
}

TArray<FGameXXKMVPCommandDescriptor> GameXXKMVPCommandRouter::BuildVisibleCommands(const UGameXXKMVPSubsystem* Subsystem, const FString& SlotName, int32 UserIndex)
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
		AddCommand(Commands, StartGame, TEXT("New Game"), true);
		AddCommand(Commands, ContinueGame, TEXT("Continue"), Subsystem->DoesSaveGameExist(SlotName, UserIndex));
		AddContinueSlotCommands(Commands, Subsystem, UserIndex);
		AddDeleteSlotCommands(Commands, Subsystem, UserIndex);
		break;
	case EGameXXKScreen::WorldMap:
		AddCommand(Commands, SelectQingshan, TEXT("Qingshan Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionQingshan()));
		AddCommand(Commands, SelectTanjiang, TEXT("Tanjiang Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
		AddCommand(Commands, SaveGame, TEXT("Save Game"), true);
		AddSaveSlotCommands(Commands);
		break;
	case EGameXXKScreen::Town:
		AddCommand(Commands, OpenWorldMap, TEXT("World Map"), true);
		AddCommand(Commands, BuyHealingPowder, TEXT("Buy Healing Powder"), State.PlayerGold >= 10);
		AddCommand(Commands, SellHealingPowder, TEXT("Sell Healing Powder"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0);
		AddCommand(Commands, UseHealingPowder, TEXT("Use Healing Powder"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0 && State.PlayerHP < State.PlayerMaxHP);
		AddCommand(Commands, EnterDungeon, TEXT("Enter Route Map"), Subsystem->CanEnterDungeon());
		AddCommand(Commands, SaveGame, TEXT("Save Game"), true);
		AddSaveSlotCommands(Commands);
		break;
	case EGameXXKScreen::DungeonMap:
		if (State.bHasGeneratedRouteMap)
		{
			for (int32 NodeId : State.ReachableRouteNodeIds)
			{
				const FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Candidate)
				{
					return Candidate.NodeId == NodeId;
				});
				if (Node)
				{
					Commands.Emplace(
						MakeRouteNodeCommandName(Node->NodeId),
						FText::FromString(FString::Printf(TEXT("%s Node"), LabelForNode(Node->NodeKind))),
						true);
				}
			}
			if (HasReachableNodeKind(State, EGameXXKNodeKind::Start))
			{
				AddCommand(Commands, SelectStart, TEXT("Start Node"), true);
			}
			if (HasReachableNodeKind(State, EGameXXKNodeKind::Battle) || HasReachableNodeKind(State, EGameXXKNodeKind::Elite))
			{
				AddCommand(Commands, SelectBattle, TEXT("Battle Node"), true);
			}
			if (HasReachableNodeKind(State, EGameXXKNodeKind::Event) || HasReachableNodeKind(State, EGameXXKNodeKind::Chest) || HasReachableNodeKind(State, EGameXXKNodeKind::Merchant))
			{
				AddCommand(Commands, ResolveEventGold, TEXT("Event: Take Gold"), true);
			}
			if (HasReachableNodeKind(State, EGameXXKNodeKind::Camp))
			{
				AddCommand(Commands, ResolveCampHeal, TEXT("Camp: Heal"), true);
			}
			if (HasReachableNodeKind(State, EGameXXKNodeKind::Boss))
			{
				AddCommand(Commands, SelectBoss, TEXT("Boss Node"), true);
			}
		}
		else
		{
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
		}
		AddCommand(Commands, SaveGame, TEXT("Save Game"), true);
		AddSaveSlotCommands(Commands);
		break;
	case EGameXXKScreen::Battle:
		AddCommand(Commands, ResolveBattleVictory, TEXT("Win Battle"), true);
		AddCommand(Commands, FailBattle, TEXT("Fail / Return Town"), State.bDungeonActive);
		AddCommand(Commands, SaveGame, TEXT("Save Game"), true);
		AddSaveSlotCommands(Commands);
		break;
	default:
		break;
	}

	return Commands;
}

TArray<FGameXXKMVPRouteNodeDescriptor> GameXXKMVPCommandRouter::BuildRouteMapNodes(const UGameXXKMVPSubsystem* Subsystem)
{
	TArray<FGameXXKMVPRouteNodeDescriptor> Nodes;
	if (!Subsystem)
	{
		return Nodes;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (State.Screen != EGameXXKScreen::DungeonMap || !State.bDungeonActive)
	{
		return Nodes;
	}

	if (State.bHasGeneratedRouteMap)
	{
		for (const FGameXXKRouteMapNode& RouteNode : State.RouteMapNodes)
		{
			Nodes.Emplace(
				MakeRouteNodeCommandName(RouteNode.NodeId),
				FText::FromString(LabelForNode(RouteNode.NodeKind)),
				RouteNode.NodeKind,
				RouteNode.NodeId,
				State.ReachableRouteNodeIds.Contains(RouteNode.NodeId),
				RouteNode.NormalizedPosition,
				RouteNode.OutgoingNodeIds);
		}
		return Nodes;
	}

	const TArray<EGameXXKNodeKind> NodeKinds = UGameXXKMVPRules::GetFixedDungeonNodes(0);
	const float LastNodeIndex = FMath::Max(1, NodeKinds.Num() - 1);
	for (int32 NodeIndex = 0; NodeIndex < NodeKinds.Num(); ++NodeIndex)
	{
		const EGameXXKNodeKind NodeKind = NodeKinds[NodeIndex];
		const float X = (NodeIndex % 2) == 0 ? 0.42f : 0.58f;
		const float Y = 1.0f - (static_cast<float>(NodeIndex) / LastNodeIndex);
		Nodes.Emplace(
			CommandForNode(NodeKind),
			FText::FromString(LabelForNode(NodeKind)),
			NodeKind,
			NodeIndex,
			NodeIndex == State.DungeonNodeIndex,
			FVector2D(X, Y));
	}

	return Nodes;
}

bool GameXXKMVPCommandRouter::ExecuteVisibleCommand(UGameXXKMVPSubsystem* Subsystem, FName CommandName, const FString& SlotName, int32 UserIndex)
{
	if (!Subsystem)
	{
		return false;
	}

	const TArray<FGameXXKMVPCommandDescriptor> VisibleCommands = BuildVisibleCommands(Subsystem, SlotName, UserIndex);
	const FGameXXKMVPCommandDescriptor* MatchingCommand = VisibleCommands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});
	if (!MatchingCommand || !MatchingCommand->bEnabled)
	{
		return false;
	}

	auto FinishCommand = [](bool bCommandSucceeded)
	{
		return bCommandSucceeded;
	};

	if (CommandName == StartGame)
	{
		return FinishCommand(Subsystem->StartGame());
	}
	if (CommandName == ContinueGame)
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->ContinueGameFromSlot(SlotName, UserIndex));
	}
	int32 SlotIndex = INDEX_NONE;
	if (TryGetIndexedCommandSlot(CommandName, TEXT("ContinueSlot"), SlotIndex))
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->ContinueGameFromSlot(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), UserIndex));
	}
	if (TryGetIndexedCommandSlot(CommandName, TEXT("DeleteSlot"), SlotIndex))
	{
		return FinishCommand(Subsystem->DeleteSaveGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), UserIndex));
	}
	if (TryGetIndexedCommandSlot(CommandName, TEXT("SaveSlot"), SlotIndex))
	{
		return FinishCommand(Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), UserIndex));
	}
	int32 RouteNodeId = INDEX_NONE;
	if (TryGetRouteNodeCommandId(CommandName, RouteNodeId))
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->SelectRouteNodeById(RouteNodeId));
	}
	if (CommandName == SaveGame)
	{
		return SavePlayableSlot(Subsystem, SlotName, UserIndex);
	}
	if (CommandName == SelectQingshan)
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
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
		return FinishCommandAndTravel(Subsystem, Subsystem->OpenDungeonFromTownExit());
	}
	if (CommandName == SelectStart)
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start));
	}
	if (CommandName == SelectBattle)
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
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
		return FinishCommandAndTravel(Subsystem, Subsystem->SelectDungeonNode(EGameXXKNodeKind::Boss));
	}
	if (CommandName == ResolveBattleVictory)
	{
		const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
		return FinishCommandAndTravel(Subsystem, Subsystem->ResolveBattleVictory(State.bDungeonActive && GetCurrentNode(State) == EGameXXKNodeKind::Boss));
	}
	if (CommandName == FailBattle)
	{
		return FinishCommandAndTravel(Subsystem, Subsystem->FailDungeonToTown());
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
