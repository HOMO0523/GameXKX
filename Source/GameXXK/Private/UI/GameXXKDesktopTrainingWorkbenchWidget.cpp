#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 WarehouseColumns = 4;
	const FVector2D ShellSize(1920.0f, 1080.0f);
	const FLinearColor Ink(0.06f, 0.045f, 0.035f, 0.98f);
	const FLinearColor Panel(0.13f, 0.09f, 0.055f, 0.97f);
	const FLinearColor PanelAlt(0.20f, 0.13f, 0.07f, 0.98f);
	const FLinearColor Accent(0.82f, 0.43f, 0.08f, 1.0f);
	const FLinearColor Gold(1.0f, 0.78f, 0.25f, 1.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Result->SetAutoWrapText(true);
		return Result;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Result->SetBrushColor(Color);
		return Result;
	}

	template <typename T>
	void AddCanvas(UCanvasPanel* Canvas, T* Child, const FVector2D& Position, const FVector2D& Size)
	{
		if (!Canvas || !Child)
		{
			return;
		}
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Canvas->AddChild(Child));
		if (Slot)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	FText NavText(const EGameXXKDesktopTrainingNav Nav)
	{
		switch (Nav)
		{
		case EGameXXKDesktopTrainingNav::Warehouse: return FText::FromString(TEXT("仓库"));
		case EGameXXKDesktopTrainingNav::Formation: return FText::FromString(TEXT("编队"));
		case EGameXXKDesktopTrainingNav::Talents: return FText::FromString(TEXT("天赋"));
		case EGameXXKDesktopTrainingNav::Tools: return FText::FromString(TEXT("工具"));
		default: return FText::FromString(TEXT("历练"));
		}
	}
}

void UGameXXKDesktopTrainingStageButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const FName InStageId)
{
	Owner = InOwner;
	StageId = InStageId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingStageButton::HandleClicked);
}

void UGameXXKDesktopTrainingStageButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleStageClicked(StageId);
	}
}

void UGameXXKDesktopTrainingActionButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const int32 InActionId)
{
	Owner = InOwner;
	ActionId = InActionId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleClicked);
}

void UGameXXKDesktopTrainingActionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleActionClicked(ActionId);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SelectedStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	BuildProgrammaticLayout();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ViewMode != EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		const UGameXXKMVPSubsystem* TravelSubsystem = ResolveMVPSubsystem();
		if (!TravelSubsystem || !TravelSubsystem->GetRuntimeState().Training.bTravelActive)
		{
			return;
		}
		TravelAccumulator += InDeltaTime;
		if (TravelAccumulator >= 1.5f)
		{
			TravelAccumulator = 0.0f;
			AdvanceTravelForTest();
		}
		return;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().Training.bChallengeActive || !Subsystem->GetRuntimeState().Training.bChallengeAutoBattle)
	{
		return;
	}
	AutoBattleAccumulator += InDeltaTime;
	if (AutoBattleAccumulator < 0.75f)
	{
		return;
	}
	AutoBattleAccumulator = 0.0f;
	AdvanceChallengeForTest();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenWorkbench()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	SelectedStageId = Progress.SelectedStageId.IsNone() ? Progress.CurrentTravelStageId : Progress.SelectedStageId;
	ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	TravelAccumulator = 0.0f;
	RefreshLayout();
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CloseWorkbench()
{
	const bool bWasVisible = IsInViewport() && GetVisibility() != ESlateVisibility::Collapsed;
	SetVisibility(ESlateVisibility::Collapsed);
	return bWasVisible;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenBackpack()
{
	ActiveNav = EGameXXKDesktopTrainingNav::Formation;
	ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsWorkbenchVisibleForTest() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehouseColumnCountForTest() const
{
	return WarehouseColumns;
}

FVector2D UGameXXKDesktopTrainingWorkbenchWidget::GetBackpackAspectRatioForTest() const
{
	return BackpackAspectRatio;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTrainingStageButtonCountForTest() const
{
	return StageButtons.Num();
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetSelectedStageIdForTest() const
{
	return SelectedStageId;
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetCurrentTravelStageIdForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetTrainingProgressCopy().CurrentTravelStageId : NAME_None;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsChallengeViewportActiveForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsAutoBattleVisibleForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsRetryVisibleForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::Workbench;
}

FText UGameXXKDesktopTrainingWorkbenchWidget::GetStageTooltipForTest(const FName StageId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->BuildTrainingStageTooltip(StageId) : FText::GetEmpty();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectStageForTest(const FName StageId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FGameXXKTrainingStageDefinition Definition;
	if (!Subsystem || !FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
	{
		return false;
	}
	SelectedStageId = StageId;
	return Subsystem->SelectTrainingStage(StageId);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickChallengeForTest()
{
	ApplyAction(6);
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickTravelForTest()
{
	ApplyAction(7);
	return ViewMode == EGameXXKDesktopTrainingViewMode::Workbench;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleAutoBattleForTest(const bool bEnabled)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport
		&& Subsystem && Subsystem->SetTrainingChallengeAutoBattle(bEnabled);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AdvanceChallengeForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ViewMode != EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		return false;
	}
	bool bCompleted = false;
	FGameXXKTrainingReward Reward;
	if (!Subsystem->AdvanceTrainingChallengeEncounter(bCompleted, Reward))
	{
		return false;
	}
	if (bCompleted)
	{
		ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
		SetNotice(FText::FromString(TEXT("挑战完成：已结算金币、经验与宝箱")));
		RefreshLayout();
	}
	else
	{
		SetNotice(FText::FromString(TEXT("自动战斗：击败当前遭遇，继续路线")));
	}
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AdvanceTravelForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ViewMode != EGameXXKDesktopTrainingViewMode::Workbench)
	{
		return false;
	}
	bool bCompleted = false;
	FGameXXKTrainingReward Reward;
	if (!Subsystem->AdvanceTrainingTravelEncounter(bCompleted, Reward))
	{
		return false;
	}
	if (bCompleted)
	{
		SetNotice(FText::FromString(FString::Printf(TEXT("游历结算：+%d 金币 / +%d 经验，继续循环"), Reward.Gold, Reward.Experience)));
		RefreshLayout();
	}
	else
	{
		SetNotice(FText::FromString(TEXT("游历中：走动、遭遇、自动战斗")));
	}
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SetRetryOnFailureForTest(const bool bEnabled)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->SetTrainingRetryOnFailure(bEnabled);
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleStageClicked(const FName StageId)
{
	SelectStageForTest(StageId);
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleActionClicked(const int32 ActionId)
{
	ApplyAction(ActionId);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildProgrammaticLayout()
{
	if (!RootCanvas)
	{
		return;
	}
	if (ChallengeBattleBoard && ChallengeBattleVisualSessionToken != 0)
	{
		ChallengeBattleBoard->CancelBattleVisualSession(ChallengeBattleVisualSessionToken);
		ChallengeBattleVisualSessionToken = 0;
	}
	RootCanvas->ClearChildren();
	StageButtons.Reset();
	ActionButtons.Reset();
	BuildWorkbenchShell();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWorkbenchShell()
{
	AddCanvas(RootCanvas, MakePanel(WidgetTree, Ink), FVector2D::ZeroVector, ShellSize);
	if (ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		BuildChallengeViewport();
	}
	else
	{
		BuildTopIdleStrip();
		BuildWarehousePanel();
		BuildBackpackPanel();
		BuildTrainingMapPanel();
	}
	BuildBottomNavigation();
	NoticePanel = MakePanel(WidgetTree, FLinearColor(0.08f, 0.05f, 0.03f, 0.96f));
	NoticeText = MakeText(WidgetTree, LastNotice, 22, Gold);
	NoticePanel->SetContent(NoticeText);
	AddCanvas(RootCanvas, NoticePanel.Get(), FVector2D(700.0f, 18.0f), FVector2D(520.0f, 48.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTopIdleStrip()
{
	UBorder* Strip = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Strip, FVector2D(360.0f, 22.0f), FVector2D(1200.0f, 108.0f));
	UTextBlock* Label = MakeText(WidgetTree, FText::FromString(TEXT("游历挂机  ·  3 敌方 / 3 我方")), 22, Gold);
	AddCanvas(RootCanvas, Label, FVector2D(385.0f, 38.0f), FVector2D(330.0f, 40.0f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UTextBlock* Enemy = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("敌 %d\n走动"), Index + 1)), 18, FLinearColor(1.0f, 0.65f, 0.55f, 1.0f));
		AddCanvas(RootCanvas, Enemy, FVector2D(725.0f + Index * 115.0f, 35.0f), FVector2D(95.0f, 58.0f));
		UTextBlock* Party = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("角 %d\n待机"), Index + 1)), 18, FLinearColor(0.55f, 0.85f, 1.0f, 1.0f));
		AddCanvas(RootCanvas, Party, FVector2D(1080.0f + Index * 115.0f, 35.0f), FVector2D(95.0f, 58.0f));
	}
	UGameXXKDesktopTrainingActionButton* RetryButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	RetryButton->Configure(this, 10);
	RetryButton->SetBackgroundColor(Accent);
	RetryButton->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("失败重试")), 18));
	AddCanvas(RootCanvas, RetryButton, FVector2D(1485.0f, 42.0f), FVector2D(135.0f, 50.0f));
	ActionButtons.Add(RetryButton);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWarehousePanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(24.0f, 150.0f), FVector2D(320.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("仓库  ·  4 列")), 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(48.0f, 174.0f), FVector2D(260.0f, 42.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	TArray<FName> Warehouse;
	if (Subsystem)
	{
		Subsystem->GetEquipmentWarehouseSnapshot(Warehouse);
	}
	for (int32 SlotIndex = 0; SlotIndex < 20; ++SlotIndex)
	{
		const int32 Column = SlotIndex % WarehouseColumns;
		const int32 Row = SlotIndex / WarehouseColumns;
		UBorder* Cell = MakePanel(WidgetTree, FLinearColor(0.07f, 0.06f, 0.05f, 1.0f));
		AddCanvas(RootCanvas, Cell, FVector2D(46.0f + Column * 68.0f, 235.0f + Row * 68.0f), FVector2D(58.0f, 58.0f));
		if (Warehouse.IsValidIndex(SlotIndex))
		{
			UTextBlock* IdText = MakeText(WidgetTree, FText::FromName(Warehouse[SlotIndex]), 10, FLinearColor::White);
			AddCanvas(RootCanvas, IdText, FVector2D(48.0f + Column * 68.0f, 255.0f + Row * 68.0f), FVector2D(54.0f, 32.0f));
		}
	}
	UTextBlock* Footer = MakeText(WidgetTree, FText::FromString(TEXT("仓库仅显示装备实例\n不显示角色身份卡")), 16, FLinearColor(0.75f, 0.68f, 0.55f, 1.0f));
	AddCanvas(RootCanvas, Footer, FVector2D(48.0f, 830.0f), FVector2D(240.0f, 70.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBackpackPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(365.0f, 150.0f), FVector2D(960.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, ActiveNav == EGameXXKDesktopTrainingNav::Tools
		? FText::FromString(TEXT("工具  ·  魔方 / 合成 / 制作"))
		: ActiveNav == EGameXXKDesktopTrainingNav::Formation ? FText::FromString(TEXT("编队  ·  角色 / 伙伴"))
		: FText::FromString(TEXT("背包  ·  角色装备")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(400.0f, 175.0f), FVector2D(700.0f, 46.0f));
	UTextBlock* GoldText = MakeText(WidgetTree, FText::FromString(TEXT("金币  0  ·  数据来自存档")), 20, Gold);
	AddCanvas(RootCanvas, GoldText, FVector2D(1010.0f, 180.0f), FVector2D(260.0f, 40.0f));
	for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
	{
		UBorder* Equip = MakePanel(WidgetTree, FLinearColor(0.10f, 0.07f, 0.05f, 1.0f));
		AddCanvas(RootCanvas, Equip, FVector2D(405.0f + (SlotIndex % 3) * 90.0f, 240.0f + (SlotIndex / 3) * 90.0f), FVector2D(78.0f, 78.0f));
	}
	UTextBlock* Identity = MakeText(WidgetTree, FText::FromString(TEXT("角色 / 伙伴\n六装备槽\n角色与伙伴在背包内部切换")), 18, FLinearColor::White);
	AddCanvas(RootCanvas, Identity, FVector2D(720.0f, 250.0f), FVector2D(300.0f, 100.0f));
	for (int32 SlotIndex = 0; SlotIndex < 20; ++SlotIndex)
	{
		const int32 Column = SlotIndex % 4;
		const int32 Row = SlotIndex / 4;
		UBorder* Cell = MakePanel(WidgetTree, FLinearColor(0.06f, 0.05f, 0.04f, 1.0f));
		AddCanvas(RootCanvas, Cell, FVector2D(700.0f + Column * 118.0f, 410.0f + Row * 66.0f), FVector2D(105.0f, 56.0f));
	}
	UGameXXKDesktopTrainingActionButton* Sort = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Sort->Configure(this, 5);
	Sort->SetBackgroundColor(Accent);
	Sort->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("物品排序")), 18));
	AddCanvas(RootCanvas, Sort, FVector2D(1120.0f, 840.0f), FVector2D(150.0f, 54.0f));
	ActionButtons.Add(Sort);
	UTextBlock* Ratio = MakeText(WidgetTree, FText::FromString(TEXT("背包比例锁定：1.76 : 1  ·  4 × 5 可视格")), 16, FLinearColor(0.78f, 0.70f, 0.60f, 1.0f));
	AddCanvas(RootCanvas, Ratio, FVector2D(400.0f, 925.0f), FVector2D(460.0f, 30.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTrainingMapPanel()
{
	UBorder* Map = MakePanel(WidgetTree, Panel);
	AddCanvas(RootCanvas, Map, FVector2D(1340.0f, 150.0f), FVector2D(556.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, ActiveNav == EGameXXKDesktopTrainingNav::Tools ? FText::FromString(TEXT("工具替换右侧地图")) : FText::FromString(TEXT("历练地图")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1370.0f, 175.0f), FVector2D(450.0f, 48.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKTrainingStageDefinition> Definitions = Subsystem ? Subsystem->GetTrainingStageDefinitions() : TArray<FGameXXKTrainingStageDefinition>();
	const EGameXXKTrainingDifficulty ActiveDifficulty = FGameXXKTrainingRules::DifficultyFromStageId(SelectedStageId);
	const TArray<EGameXXKTrainingDifficulty> Difficulties = {
		EGameXXKTrainingDifficulty::Normal,
		EGameXXKTrainingDifficulty::Hard,
		EGameXXKTrainingDifficulty::Hell};
	for (int32 DifficultyIndex = 0; DifficultyIndex < Difficulties.Num(); ++DifficultyIndex)
	{
		UGameXXKDesktopTrainingActionButton* DifficultyTab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		DifficultyTab->Configure(this, 11 + DifficultyIndex);
		DifficultyTab->SetBackgroundColor(Difficulties[DifficultyIndex] == ActiveDifficulty ? Accent : PanelAlt);
		const TCHAR* Label = DifficultyIndex == 0 ? TEXT("普通") : DifficultyIndex == 1 ? TEXT("困难") : TEXT("地狱");
		DifficultyTab->SetContent(MakeText(WidgetTree, FText::FromString(Label), 18));
		AddCanvas(RootCanvas, DifficultyTab, FVector2D(1380.0f + DifficultyIndex * 155.0f, 225.0f), FVector2D(135.0f, 42.0f));
		ActionButtons.Add(DifficultyTab);
	}
	for (const FGameXXKTrainingStageDefinition& Definition : Definitions)
	{
		if (Definition.Difficulty != ActiveDifficulty)
		{
			continue;
		}
		UGameXXKDesktopTrainingStageButton* Node = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingStageButton>(UGameXXKDesktopTrainingStageButton::StaticClass());
		Node->Configure(this, Definition.StageId);
		Node->SetBackgroundColor(Definition.StageId == SelectedStageId ? Gold : FLinearColor(0.35f, 0.25f, 0.13f, 1.0f));
		Node->SetToolTipText(Subsystem ? Subsystem->BuildTrainingStageTooltip(Definition.StageId) : FText::GetEmpty());
		Node->SetContent(MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%d-%d"), Definition.Chapter, ((Definition.StageNumber - 1) % 3) + 1)), 18, Ink));
		const int32 LocalIndex = Definition.StageNumber - 1;
		AddCanvas(RootCanvas, Node, FVector2D(1380.0f + (LocalIndex % 3) * 160.0f, 300.0f + (LocalIndex / 3) * 78.0f), FVector2D(120.0f, 54.0f));
		StageButtons.Add(Node);
	}
	TravelStageText = MakeText(WidgetTree, FText::FromString(TEXT("当前游历关卡：未选择")), 20, FLinearColor::White);
	if (Subsystem)
	{
		const FName Current = Subsystem->GetTrainingProgressCopy().CurrentTravelStageId;
		TravelStageText->SetText(FText::FromString(FString::Printf(TEXT("当前游历关卡：%s"), *Current.ToString())));
	}
	AddCanvas(RootCanvas, TravelStageText.Get(), FVector2D(1375.0f, 735.0f), FVector2D(480.0f, 46.0f));
	UGameXXKDesktopTrainingActionButton* Challenge = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Challenge->Configure(this, 6);
	Challenge->SetBackgroundColor(Accent);
	Challenge->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("挑战")), 24));
	if (Subsystem)
	{
		const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
		const bool bCanChallenge = FGameXXKTrainingRules::CanChallenge(Progress, SelectedStageId);
		Challenge->SetIsEnabled(bCanChallenge);
		if (!bCanChallenge && FGameXXKTrainingRules::IsStageCleared(Progress, SelectedStageId))
		{
			Challenge->SetToolTipText(FText::FromString(TEXT("已通关；全部关卡完成后期待新内容")));
		}
	}
	AddCanvas(RootCanvas, Challenge, FVector2D(1380.0f, 820.0f), FVector2D(175.0f, 64.0f));
	ActionButtons.Add(Challenge);
	UGameXXKDesktopTrainingActionButton* Travel = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Travel->Configure(this, 7);
	Travel->SetBackgroundColor(FLinearColor(0.28f, 0.20f, 0.12f, 1.0f));
	Travel->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("游历")), 24));
	AddCanvas(RootCanvas, Travel, FVector2D(1580.0f, 820.0f), FVector2D(175.0f, 64.0f));
	ActionButtons.Add(Travel);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildChallengeViewport()
{
	UBorder* Viewport = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Viewport, FVector2D(365.0f, 22.0f), FVector2D(960.0f, 968.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("挑战路线 / 局内战斗")), 32, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(405.0f, 55.0f), FVector2D(600.0f, 48.0f));
	ChallengeStatusText = MakeText(WidgetTree, FText::FromString(TEXT("路线加载中：普通怪 → 次级精英 → 首领")), 22, FLinearColor::White);
	AddCanvas(RootCanvas, ChallengeStatusText.Get(), FVector2D(405.0f, 115.0f), FVector2D(800.0f, 44.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem)
	{
		const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = Subsystem->GetTrainingEncounterSequence(Progress.ActiveChallengeStageId);
		if (Subsystem->IsTrainingChallengeBattleActive())
		{
			if (!ChallengeBattleBoard)
			{
				ChallengeBattleBoard = WidgetTree->ConstructWidget<UGameXXKBattleBoardWidget>(UGameXXKBattleBoardWidget::StaticClass());
			}
			if (ChallengeBattleBoard)
			{
				ChallengeBattleBoard->SetMVPSubsystem(const_cast<UGameXXKMVPSubsystem*>(Subsystem));
				AddCanvas(RootCanvas, ChallengeBattleBoard.Get(), FVector2D(395.0f, 175.0f), FVector2D(710.0f, 535.0f));
				ChallengeBattleBoard->SetVisibility(ESlateVisibility::Visible);
				ChallengeBattleVisualSessionToken = 1;
				ChallengeBattleBoard->BeginBattleVisualSession(ChallengeBattleVisualSessionToken);
				ChallengeBattleBoard->RefreshFromState();
			}
		}
		for (int32 Index = 0; Index < Encounters.Num(); ++Index)
		{
			const bool bActive = Index == Progress.ActiveChallengeEncounterIndex;
			UTextBlock* Encounter = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%s %s"), bActive ? TEXT("▶") : TEXT("○"), *Encounters[Index].DisplayName.ToString())), 20, bActive ? Gold : FLinearColor(0.75f, 0.70f, 0.62f, 1.0f));
			AddCanvas(RootCanvas, Encounter, FVector2D(1115.0f + (Index % 2) * 145.0f, 230.0f + (Index / 2) * 56.0f), FVector2D(135.0f, 40.0f));
		}
	}
	UGameXXKDesktopTrainingActionButton* Auto = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Auto->Configure(this, 8);
	Auto->SetBackgroundColor(Accent);
	Auto->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("自动战斗")), 22));
	AddCanvas(RootCanvas, Auto, FVector2D(450.0f, 780.0f), FVector2D(190.0f, 62.0f));
	ActionButtons.Add(Auto);
	UGameXXKDesktopTrainingActionButton* Advance = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Advance->Configure(this, 9);
	Advance->SetBackgroundColor(FLinearColor(0.25f, 0.18f, 0.11f, 1.0f));
	Advance->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("击败当前遭遇")), 22));
	AddCanvas(RootCanvas, Advance, FVector2D(675.0f, 780.0f), FVector2D(260.0f, 62.0f));
	ActionButtons.Add(Advance);
	UTextBlock* Hint = MakeText(WidgetTree, FText::FromString(TEXT("自动战斗只在挑战局内显示；游历条仅提供失败重试。")), 18, FLinearColor(0.78f, 0.70f, 0.60f, 1.0f));
	AddCanvas(RootCanvas, Hint, FVector2D(450.0f, 880.0f), FVector2D(700.0f, 36.0f));
	}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBottomNavigation()
{
	const TArray<EGameXXKDesktopTrainingNav> Navs = {
		EGameXXKDesktopTrainingNav::Warehouse,
		EGameXXKDesktopTrainingNav::Formation,
		EGameXXKDesktopTrainingNav::Talents,
		EGameXXKDesktopTrainingNav::Tools,
		EGameXXKDesktopTrainingNav::Training};
	for (int32 Index = 0; Index < Navs.Num(); ++Index)
	{
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Button->Configure(this, Index);
		Button->SetBackgroundColor(Navs[Index] == ActiveNav ? Accent : PanelAlt);
		Button->SetContent(MakeText(WidgetTree, NavText(Navs[Index]), 22));
		AddCanvas(RootCanvas, Button, FVector2D(365.0f + Index * 190.0f, 1000.0f), FVector2D(175.0f, 58.0f));
		ActionButtons.Add(Button);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshLayout()
{
	BuildProgrammaticLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyAction(const int32 ActionId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (ActionId >= 0 && ActionId <= 4)
	{
		ActiveNav = static_cast<EGameXXKDesktopTrainingNav>(ActionId);
		if (ActiveNav == EGameXXKDesktopTrainingNav::Training)
		{
			ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
		}
		RefreshLayout();
		return;
	}
	if (ActionId >= 11 && ActionId <= 13)
	{
		const EGameXXKTrainingDifficulty Difficulty = static_cast<EGameXXKTrainingDifficulty>(ActionId - 11);
		SelectedStageId = FGameXXKTrainingRules::MakeStageId(Difficulty, 1);
		Subsystem->SelectTrainingStage(SelectedStageId);
		RefreshLayout();
		return;
	}
	switch (ActionId)
	{
	case 5:
		SetNotice(FText::FromString(TEXT("背包按物品排序")));
		break;
	case 6:
		if (Subsystem->StartTrainingChallenge(SelectedStageId))
		{
			ViewMode = EGameXXKDesktopTrainingViewMode::ChallengeViewport;
			AutoBattleAccumulator = 0.0f;
			RefreshLayout();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("该关卡尚未解锁或已经通关")));
		}
		break;
	case 7:
		if (Subsystem->StartTrainingTravel(SelectedStageId))
		{
			ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
			SetNotice(FText::FromString(TEXT("开始游历：走动、遭遇、自动战斗、结算后循环")));
			RefreshLayout();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("未通关关卡不能游历")));
		}
		break;
	case 8:
		if (ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
		{
			const bool bAuto = !Subsystem->GetTrainingProgressCopy().bChallengeAutoBattle;
			Subsystem->SetTrainingChallengeAutoBattle(bAuto);
			SetNotice(bAuto ? FText::FromString(TEXT("自动战斗已开启")) : FText::FromString(TEXT("自动战斗已暂停")));
			RefreshLayout();
		}
		break;
	case 9:
		AdvanceChallengeForTest();
		break;
	case 10:
		Subsystem->SetTrainingRetryOnFailure(!Subsystem->GetTrainingProgressCopy().bRetryOnFailure);
		SetNotice(FText::FromString(TEXT("已切换游历失败重试策略")));
		break;
	default:
		break;
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetNotice(const FText& Notice)
{
	LastNotice = Notice;
	if (NoticeText)
	{
		NoticeText->SetText(Notice);
	}
}
