#include "UI/GameXXKMVPHUD.h"

#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace GameXXKMVPHUDCommands
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

	static void AddCommand(TArray<FGameXXKMVPCommandDescriptor>& Commands, FName Name, const TCHAR* Label, bool bEnabled)
	{
		Commands.Emplace(Name, FText::FromString(Label), bEnabled);
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

	static EGameXXKNodeKind GetCurrentNode(const FGameXXKRuntimeState& State)
	{
		const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		return Nodes.IsValidIndex(State.DungeonNodeIndex) ? Nodes[State.DungeonNodeIndex] : EGameXXKNodeKind::Start;
	}
}

FGameXXKMVPCommandDescriptor::FGameXXKMVPCommandDescriptor(FName InCommandName, const FText& InLabel, bool bInEnabled)
	: CommandName(InCommandName)
	, Label(InLabel)
	, bEnabled(bInEnabled)
{
}

UGameXXKMVPSubsystem* AGameXXKMVPHUD::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

void AGameXXKMVPHUD::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

void AGameXXKMVPHUD::SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex)
{
	StartGameSlotNameOverride = SlotName;
	StartGameUserIndexOverride = UserIndex;
}

bool AGameXXKMVPHUD::SavePlayableSlot(UGameXXKMVPSubsystem* Subsystem) const
{
	if (!Subsystem)
	{
		return false;
	}
	return StartGameSlotNameOverride.IsEmpty()
		? Subsystem->SaveCurrentGame(TEXT(""), 0)
		: Subsystem->SaveCurrentGame(StartGameSlotNameOverride, StartGameUserIndexOverride);
}

TArray<FGameXXKMVPCommandDescriptor> AGameXXKMVPHUD::BuildVisibleCommands() const
{
	TArray<FGameXXKMVPCommandDescriptor> Commands;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Commands;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	using namespace GameXXKMVPHUDCommands;

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

bool AGameXXKMVPHUD::HandleDemoCommand(FName CommandName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const TArray<FGameXXKMVPCommandDescriptor> VisibleCommands = BuildVisibleCommands();
	const FGameXXKMVPCommandDescriptor* MatchingCommand = VisibleCommands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});
	if (!MatchingCommand || !MatchingCommand->bEnabled)
	{
		return false;
	}

	using namespace GameXXKMVPHUDCommands;

	auto FinishCommand = [this, Subsystem](bool bCommandSucceeded)
	{
		if (!bCommandSucceeded)
		{
			return false;
		}
		return SavePlayableSlot(Subsystem);
	};

	if (CommandName == StartGame)
	{
		return FinishCommand(StartGameSlotNameOverride.IsEmpty()
			? Subsystem->StartGame()
			: Subsystem->StartGameFromSlot(StartGameSlotNameOverride, StartGameUserIndexOverride));
	}
	if (CommandName == SelectQingshan)
	{
		return FinishCommand(Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
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

FText AGameXXKMVPHUD::BuildStatusText() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return FText::FromString(TEXT("GameXXK MVP"));
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FString Summary = FString::Printf(
		TEXT("Screen: %s | Quest: %s | Level %d XP %d | Gold %d | HP %d/%d | Follower: %s"),
		*GameXXKMVPHUDCommands::ScreenToString(State.Screen),
		*GameXXKMVPHUDCommands::QuestToString(State.QuestState),
		State.PlayerLevel,
		State.PlayerXP,
		State.PlayerGold,
		State.PlayerHP,
		State.PlayerMaxHP,
		State.bFollowerJoined ? TEXT("Yes") : TEXT("No"));
	return FText::FromString(Summary);
}

void AGameXXKMVPHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float PanelX = 24.0f;
	float CursorY = 24.0f;
	const float PanelWidth = 360.0f;
	const float ButtonHeight = 34.0f;
	const float Gap = 8.0f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.03f, 0.86f), PanelX - 12.0f, CursorY - 12.0f, PanelWidth + 24.0f, 520.0f);
	DrawText(TEXT("GameXXK MVP Playable Shell"), FColor::White, PanelX, CursorY, nullptr, 1.15f);
	CursorY += 34.0f;
	DrawText(BuildStatusText().ToString(), FColor(210, 220, 225), PanelX, CursorY, nullptr, 0.72f);
	CursorY += 42.0f;

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		const FGameXXKMVPCommandDescriptor& Command = Commands[Index];
		const FLinearColor Fill = Command.bEnabled ? FLinearColor(0.08f, 0.30f, 0.23f, 0.94f) : FLinearColor(0.16f, 0.16f, 0.16f, 0.82f);
		const FColor TextColor = Command.bEnabled ? FColor::White : FColor(150, 150, 150);
		DrawRect(Fill, PanelX, CursorY, PanelWidth, ButtonHeight);
		DrawText(Command.Label.ToString(), TextColor, PanelX + 12.0f, CursorY + 8.0f, nullptr, 0.86f);
		if (Command.bEnabled)
		{
			AddHitBox(FVector2D(PanelX, CursorY), FVector2D(PanelWidth, ButtonHeight), Command.CommandName, true, Index);
		}
		CursorY += ButtonHeight + Gap;
	}
}

void AGameXXKMVPHUD::NotifyHitBoxClick(FName BoxName)
{
	HandleDemoCommand(BoxName);
}
