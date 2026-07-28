#include "UI/GameXXKRouteEncounterPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D EncounterPanelSize(900.0f, 520.0f);
	const FVector2D EncounterActionSize(226.0f, 56.0f);
	const FMargin WindowFrameMargin(0.065f);
	const FMargin ActionFrameMargin(5.0f / 73.0f, 5.0f / 31.0f, 5.0f / 73.0f, 5.0f / 31.0f);
	const FString BackpackTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/"));
	const FString WindowFrameTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
	const FString HeaderTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Header.T_TownBackpack_Header"));
	const FString ActionTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_ActionBlank.T_TownBackpack_ActionBlank"));

	struct FRouteEncounterPresentation
	{
		FText Title;
		FText Speaker;
		FText Body;
		FText PrimaryLabel;
		FText SecondaryLabel;
		FText TertiaryLabel;
		FText PrimaryTooltip;
		FText SecondaryTooltip;
		FText TertiaryTooltip;
		FText CloseLabel;
		EGameXXKRouteEncounterAction PrimaryAction = EGameXXKRouteEncounterAction::None;
		EGameXXKRouteEncounterAction SecondaryAction = EGameXXKRouteEncounterAction::None;
		EGameXXKRouteEncounterAction TertiaryAction = EGameXXKRouteEncounterAction::None;
		bool bPrimaryEnabled = false;
		bool bSecondaryEnabled = false;
		bool bTertiaryEnabled = false;
	};

	UTexture2D* LoadTexture(const FString& TexturePath)
	{
		return TexturePath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *TexturePath);
	}

	FSlateBrush MakeTextureBrush(
		const FString& TexturePath,
		const FVector2D& ImageSize,
		ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& Margin = WindowFrameMargin)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(TexturePath));
		Brush.DrawAs = DrawAs;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		if (DrawAs == ESlateBrushDrawType::Box)
		{
			Brush.Margin = Margin;
		}
		return Brush;
	}

	FButtonStyle MakeActionButtonStyle()
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(ActionTexturePath, EncounterActionSize, ESlateBrushDrawType::Box, ActionFrameMargin));
		Style.SetHovered(MakeTextureBrush(ActionTexturePath, EncounterActionSize, ESlateBrushDrawType::Box, ActionFrameMargin));
		Style.SetPressed(MakeTextureBrush(ActionTexturePath, EncounterActionSize, ESlateBrushDrawType::Box, ActionFrameMargin));
		Style.SetDisabled(MakeTextureBrush(ActionTexturePath, EncounterActionSize, ESlateBrushDrawType::Box, ActionFrameMargin));
		return Style;
	}

	void AddCanvasChild(
		UCanvasPanel* Canvas,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f),
		const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (!Canvas || !Child)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	UTextBlock* MakeInkText(UWidgetTree* WidgetTree, const FText& Text, const int32 FontSize, const FLinearColor& Ink = FLinearColor(0.10f, 0.075f, 0.045f, 1.0f))
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Ink));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	const FGameXXKRouteMapNode* FindPendingRouteNode(const FGameXXKRuntimeState& State)
	{
		return State.RouteMapNodes.FindByPredicate([&State](const FGameXXKRouteMapNode& Candidate)
		{
			return Candidate.NodeId == State.PendingRouteNodeId;
		});
	}

	FText TaskNpcDisplayName(const FName NpcId)
	{
		if (NpcId == TEXT("Npc.TusiChief")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcTusiChief", "土司首领");
		if (NpcId == TEXT("Npc.SongJinBao")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcSongJinBao", "宋金宝");
		if (NpcId == TEXT("Npc.YueBai")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcYueBai", "月白");
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcZhouGuangZu", "周光祖");
		if (NpcId == TEXT("Npc.JinGui")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcJinGui", "金贵");
		if (NpcId == TEXT("Npc.QiongMeiEr")) return NSLOCTEXT("GameXXKRouteEncounter", "NpcQiongMeiEr", "琼么儿");
		return FText::FromName(NpcId);
	}

	bool IsRouteEncounterScreen(const EGameXXKScreen Screen)
	{
		return Screen == EGameXXKScreen::RouteEvent
			|| Screen == EGameXXKScreen::RouteCamp
			|| Screen == EGameXXKScreen::RouteMerchant;
	}
}

void UGameXXKRouteEncounterActionButton::Configure(UGameXXKRouteEncounterPanelWidget* InOwner, const EGameXXKRouteEncounterAction InAction)
{
	Owner = InOwner;
	Action = InAction;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKRouteEncounterActionButton::HandleClicked);
}

void UGameXXKRouteEncounterActionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->ExecuteAction(Action);
	}
}

TSharedRef<SWidget> UGameXXKRouteEncounterPanelWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKRouteEncounterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	CloseEncounterPanel();
}

void UGameXXKRouteEncounterPanelWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsRouteEncounterScreen(Subsystem->GetRuntimeState().Screen))
	{
		CloseEncounterPanel();
		return;
	}
	if (IsEncounterPanelOpenForTest())
	{
		BuildPresentation();
	}
}

bool UGameXXKRouteEncounterPanelWidget::OpenEncounterPanel()
{
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsRouteEncounterScreen(Subsystem->GetRuntimeState().Screen) || !BuildPresentation())
	{
		return false;
	}
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::CloseEncounterPanel()
{
	const bool bWasOpen = IsEncounterPanelOpenForTest();
	SetVisibility(ESlateVisibility::Collapsed);
	return bWasOpen;
}

bool UGameXXKRouteEncounterPanelWidget::IsEncounterPanelOpenForTest() const
{
	return GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

FString UGameXXKRouteEncounterPanelWidget::GetWindowFrameResourcePathForTest() const
{
	return WindowFrameTexturePath;
}

FString UGameXXKRouteEncounterPanelWidget::GetHeaderResourcePathForTest() const
{
	return HeaderTexturePath;
}

FString UGameXXKRouteEncounterPanelWidget::GetActionResourcePathForTest() const
{
	return ActionTexturePath;
}

FText UGameXXKRouteEncounterPanelWidget::GetSpeakerTextForTest() const
{
	return SpeakerTextBlock ? SpeakerTextBlock->GetText() : FText::GetEmpty();
}

FText UGameXXKRouteEncounterPanelWidget::GetPrimaryActionTextForTest() const
{
	return PrimaryActionTextBlock ? PrimaryActionTextBlock->GetText() : FText::GetEmpty();
}

FText UGameXXKRouteEncounterPanelWidget::GetSecondaryActionTextForTest() const
{
	return SecondaryActionTextBlock ? SecondaryActionTextBlock->GetText() : FText::GetEmpty();
}

FText UGameXXKRouteEncounterPanelWidget::GetTertiaryActionTextForTest() const
{
	return TertiaryActionTextBlock ? TertiaryActionTextBlock->GetText() : FText::GetEmpty();
}

EGameXXKRouteEncounterAction UGameXXKRouteEncounterPanelWidget::GetPrimaryActionForTest() const
{
	return PrimaryAction;
}

EGameXXKRouteEncounterAction UGameXXKRouteEncounterPanelWidget::GetSecondaryActionForTest() const
{
	return SecondaryAction;
}

bool UGameXXKRouteEncounterPanelWidget::TriggerPrimaryActionForTest()
{
	return PrimaryActionButton && PrimaryActionButton->GetIsEnabled() && ExecuteAction(PrimaryAction);
}

bool UGameXXKRouteEncounterPanelWidget::TriggerSecondaryActionForTest()
{
	return SecondaryActionButton && SecondaryActionButton->GetIsEnabled() && ExecuteAction(SecondaryAction);
}

bool UGameXXKRouteEncounterPanelWidget::TriggerTertiaryActionForTest()
{
	return TertiaryActionButton && TertiaryActionButton->GetIsEnabled() && ExecuteAction(TertiaryAction);
}

void UGameXXKRouteEncounterPanelWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("RouteEncounterPanelWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteEncounterPanelRoot"));
	WidgetTree->RootWidget = RootCanvas;

	ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteEncounterModalBackdrop"));
	ModalBackdrop->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.022f, 0.53f));
	AddCanvasChild(RootCanvas, ModalBackdrop, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteEncounterPaperWindow"));
	WindowFrame->SetBrush(MakeTextureBrush(WindowFrameTexturePath, EncounterPanelSize, ESlateBrushDrawType::Box));
	WindowFrame->SetBrushColor(FLinearColor::White);
	WindowFrame->SetPadding(FMargin(38.0f, 34.0f, 38.0f, 34.0f));
	AddCanvasChild(RootCanvas, WindowFrame, FVector2D::ZeroVector, EncounterPanelSize, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteEncounterPaperContent"));
	WindowFrame->SetContent(FrameCanvas);

	HeaderImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RouteEncounterHeaderStrip"));
	HeaderImage->SetBrush(MakeTextureBrush(HeaderTexturePath, FVector2D(126.0f, 38.0f)));
	AddCanvasChild(FrameCanvas, HeaderImage, FVector2D(0.0f, 0.0f), FVector2D(126.0f, 38.0f));

	TitleTextBlock = MakeInkText(WidgetTree, NSLOCTEXT("GameXXKRouteEncounter", "DefaultTitle", "路线遭遇"), 25);
	if (TitleTextBlock)
	{
		TitleTextBlock->SetJustification(ETextJustify::Center);
		AddCanvasChild(FrameCanvas, TitleTextBlock, FVector2D(18.0f, 1.0f), FVector2D(270.0f, 38.0f));
	}

	SpeakerTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 28, FLinearColor(0.08f, 0.12f, 0.11f, 1.0f));
	AddCanvasChild(FrameCanvas, SpeakerTextBlock, FVector2D(12.0f, 74.0f), FVector2D(800.0f, 42.0f));

	BodyTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 20, FLinearColor(0.20f, 0.15f, 0.10f, 1.0f));
	AddCanvasChild(FrameCanvas, BodyTextBlock, FVector2D(12.0f, 132.0f), FVector2D(800.0f, 162.0f));

	PrimaryActionButton = WidgetTree->ConstructWidget<UGameXXKRouteEncounterActionButton>(UGameXXKRouteEncounterActionButton::StaticClass(), TEXT("RouteEncounterPrimaryAction"));
	PrimaryActionButton->SetStyle(MakeActionButtonStyle());
	PrimaryActionTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 18);
	if (PrimaryActionTextBlock)
	{
		PrimaryActionTextBlock->SetJustification(ETextJustify::Center);
		PrimaryActionButton->SetContent(PrimaryActionTextBlock);
	}
	AddCanvasChild(FrameCanvas, PrimaryActionButton, FVector2D(58.0f, 358.0f), EncounterActionSize);

	SecondaryActionButton = WidgetTree->ConstructWidget<UGameXXKRouteEncounterActionButton>(UGameXXKRouteEncounterActionButton::StaticClass(), TEXT("RouteEncounterSecondaryAction"));
	SecondaryActionButton->SetStyle(MakeActionButtonStyle());
	SecondaryActionTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 18);
	if (SecondaryActionTextBlock)
	{
		SecondaryActionTextBlock->SetJustification(ETextJustify::Center);
		SecondaryActionButton->SetContent(SecondaryActionTextBlock);
	}
	AddCanvasChild(FrameCanvas, SecondaryActionButton, FVector2D(318.0f, 358.0f), EncounterActionSize);

	TertiaryActionButton = WidgetTree->ConstructWidget<UGameXXKRouteEncounterActionButton>(UGameXXKRouteEncounterActionButton::StaticClass(), TEXT("RouteEncounterTertiaryAction"));
	TertiaryActionButton->SetStyle(MakeActionButtonStyle());
	TertiaryActionTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 18);
	if (TertiaryActionTextBlock)
	{
		TertiaryActionTextBlock->SetJustification(ETextJustify::Center);
		TertiaryActionButton->SetContent(TertiaryActionTextBlock);
	}
	AddCanvasChild(FrameCanvas, TertiaryActionButton, FVector2D(578.0f, 358.0f), EncounterActionSize);

	CloseButton = WidgetTree->ConstructWidget<UGameXXKRouteEncounterActionButton>(UGameXXKRouteEncounterActionButton::StaticClass(), TEXT("RouteEncounterCloseAction"));
	CloseButton->SetStyle(MakeActionButtonStyle());
	CloseTextBlock = MakeInkText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.25f, 0.20f, 0.14f, 1.0f));
	if (CloseTextBlock)
	{
		CloseTextBlock->SetJustification(ETextJustify::Center);
		CloseButton->SetContent(CloseTextBlock);
	}
	AddCanvasChild(FrameCanvas, CloseButton, FVector2D(318.0f, 430.0f), EncounterActionSize);

	SetVisibility(ESlateVisibility::Collapsed);
}

bool UGameXXKRouteEncounterPanelWidget::BuildPresentation()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (!IsRouteEncounterScreen(State.Screen))
	{
		return false;
	}

	FRouteEncounterPresentation Presentation;
	Presentation.CloseLabel = NSLOCTEXT("GameXXKRouteEncounter", "Close", "暂不决定");
	const FGameXXKRouteMapNode* PendingNode = FindPendingRouteNode(State);
	const FGameXXKRouteEncounterDefinition* CatalogEncounter = FGameXXKRouteEncounterCatalog::FindDefinition(State.CardRun.PendingEvent.EncounterId);
	if (State.Screen == EGameXXKScreen::RouteEvent && CatalogEncounter && PendingNode)
	{
		Presentation.Title = CatalogEncounter->Title;
		Presentation.Speaker = CatalogEncounter->Speaker;
		Presentation.Body = CatalogEncounter->Body;
		const EGameXXKRouteEncounterAction Actions[3] = {
			EGameXXKRouteEncounterAction::SelectChoice0,
			EGameXXKRouteEncounterAction::SelectChoice1,
			EGameXXKRouteEncounterAction::SelectChoice2};
		FText* Labels[3] = {&Presentation.PrimaryLabel, &Presentation.SecondaryLabel, &Presentation.TertiaryLabel};
		FText* Tooltips[3] = {&Presentation.PrimaryTooltip, &Presentation.SecondaryTooltip, &Presentation.TertiaryTooltip};
		EGameXXKRouteEncounterAction* PresentationActions[3] = {&Presentation.PrimaryAction, &Presentation.SecondaryAction, &Presentation.TertiaryAction};
		bool* Enabled[3] = {&Presentation.bPrimaryEnabled, &Presentation.bSecondaryEnabled, &Presentation.bTertiaryEnabled};
		for (int32 ChoiceIndex = 0; ChoiceIndex < CatalogEncounter->Choices.Num() && ChoiceIndex < 3; ++ChoiceIndex)
		{
			*PresentationActions[ChoiceIndex] = Actions[ChoiceIndex];
			*Enabled[ChoiceIndex] = true;
			const FGameXXKRouteEncounterChoiceDefinition& Choice = CatalogEncounter->Choices[ChoiceIndex];
			*Labels[ChoiceIndex] = Choice.Label;
			if (CatalogEncounter->Kind == EGameXXKRouteEncounterKind::Event)
			{
				if (Choice.RewardKind == EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport && !State.CardRun.PartySelection.QuestNpc.NpcId.IsNone())
				{
					*Enabled[ChoiceIndex] = false;
					*Tooltips[ChoiceIndex] = NSLOCTEXT("GameXXKRouteEncounter", "SupportOccupiedTooltip", "本次路线已有任务支援，不能替换。请选择属性祝福。");
				}
				else
				{
					*Tooltips[ChoiceIndex] = Choice.Label;
				}
			}
			else if (State.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(ChoiceIndex))
			{
				const FGameXXKRelicDefinition* Relic = FGameXXKRelicCatalog::FindDefinition(State.CardRun.PendingRelicOffer.RelicIds[ChoiceIndex]);
				if (Relic)
				{
					*Labels[ChoiceIndex] = FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "ChooseRelic", "选择 · {0}"), Relic->DisplayName);
					*Tooltips[ChoiceIndex] = FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "RelicTooltip", "{0}\n{1}"), Relic->DisplayName, Relic->Description);
				}
			}
		}
	}
	else
	{

	switch (State.Screen)
	{
	case EGameXXKScreen::RouteEvent:
		if (PendingNode && PendingNode->NodeKind == EGameXXKNodeKind::Chest)
		{
			Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "ChestTitle", "宝匣抉择");
			Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "ChestSpeaker", "路边宝匣");
			Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "ChestBody", "斑驳的木匣半掩在草丛中。选择带走一份明确的战利品。\n不作选择不会结算该节点。");
			const EGameXXKRouteEncounterAction Actions[3] = {
				EGameXXKRouteEncounterAction::SelectChoice0,
				EGameXXKRouteEncounterAction::SelectChoice1,
				EGameXXKRouteEncounterAction::SelectChoice2};
			FText* Labels[3] = {&Presentation.PrimaryLabel, &Presentation.SecondaryLabel, &Presentation.TertiaryLabel};
			FText* Tooltips[3] = {&Presentation.PrimaryTooltip, &Presentation.SecondaryTooltip, &Presentation.TertiaryTooltip};
			EGameXXKRouteEncounterAction* PresentationActions[3] = {&Presentation.PrimaryAction, &Presentation.SecondaryAction, &Presentation.TertiaryAction};
			bool* Enabled[3] = {&Presentation.bPrimaryEnabled, &Presentation.bSecondaryEnabled, &Presentation.bTertiaryEnabled};
			for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
			{
				if (!State.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(ChoiceIndex))
				{
					continue;
				}
				*PresentationActions[ChoiceIndex] = Actions[ChoiceIndex];
				*Enabled[ChoiceIndex] = true;
				const FName RelicId = State.CardRun.PendingRelicOffer.RelicIds[ChoiceIndex];
				if (const FGameXXKRelicDefinition* Relic = FGameXXKRelicCatalog::FindDefinition(RelicId))
				{
					*Labels[ChoiceIndex] = FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "FallbackChooseRelic", "选择 · {0}"), Relic->DisplayName);
					*Tooltips[ChoiceIndex] = FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "FallbackRelicTooltip", "{0}\n{1}"), Relic->DisplayName, Relic->Description);
				}
				else
				{
					*Labels[ChoiceIndex] = NSLOCTEXT("GameXXKRouteEncounter", "FallbackUnknownRelic", "选择遗物");
					*Tooltips[ChoiceIndex] = FText::FromName(RelicId);
				}
			}
		}
		else if (const FGameXXKQuestNpcDefinition* QuestNpc = FGameXXKCompanionCatalog::FindQuestNpcDefinition(State.CardRun.PendingEvent.EventNpcId))
		{
			const FText NpcName = TaskNpcDisplayName(QuestNpc->NpcId);
			const bool bSupportSlotAvailable = State.CardRun.PartySelection.QuestNpc.NpcId.IsNone();
			Presentation.Title = FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcTitle", "奇遇 · {0}"), NpcName);
			Presentation.Speaker = NpcName;
			Presentation.Body = bSupportSlotAvailable
				? FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcBody", "{0}愿以自己的固定技艺协助本次冒险。同行后会占用唯一的任务支援位，并在路线结束后离队。"), NpcName)
				: NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcOccupiedBody", "本次冒险已有任务支援。不能替换现有支援，但仍可选择领取替代奖励。 ");
			Presentation.PrimaryLabel = bSupportSlotAvailable
				? FText::Format(NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcAccept", "邀请{0}支援"), NpcName)
				: NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcOccupiedAction", "已有任务支援");
			Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "TaskNpcAlternative", "婉拒，领取疗伤散");
			Presentation.PrimaryAction = EGameXXKRouteEncounterAction::AcceptTaskNpcSupport;
			Presentation.SecondaryAction = EGameXXKRouteEncounterAction::TakeHealingPowder;
			Presentation.bPrimaryEnabled = bSupportSlotAvailable;
			Presentation.bSecondaryEnabled = true;
		}
		else if (State.CardRun.PendingEvent.EventNpcId == TEXT("Npc.Event.NiuHuan"))
		{
			Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "NiuHuanTitle", "山路偶遇");
			Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "NiuHuanSpeaker", "牛欢");
			Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "NiuHuanBody", "牛欢拦下你，递来两份不同的谢礼。选择一份后才会继续前行。 ");
			Presentation.PrimaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "NiuHuanGold", "收下 12 金");
			Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "NiuHuanSupply", "领取疗伤散");
			Presentation.PrimaryAction = EGameXXKRouteEncounterAction::TakeGold;
			Presentation.SecondaryAction = EGameXXKRouteEncounterAction::TakeHealingPowder;
			Presentation.bPrimaryEnabled = true;
			Presentation.bSecondaryEnabled = true;
		}
		else
		{
			Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "EventTitle", "山路偶遇");
			Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "EventSpeaker", "江湖来客");
			Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "EventBody", "前方的来客递来一份报酬。选择领取金钱或补给；按 F 本身不会结算事件。 ");
			Presentation.PrimaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "EventGold", "收下 12 金");
			Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "EventSupply", "领取疗伤散");
			Presentation.PrimaryAction = EGameXXKRouteEncounterAction::TakeGold;
			Presentation.SecondaryAction = EGameXXKRouteEncounterAction::TakeHealingPowder;
			Presentation.bPrimaryEnabled = true;
			Presentation.bSecondaryEnabled = true;
		}
		break;

	case EGameXXKScreen::RouteCamp:
		Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "CampTitle", "营火抉择");
		Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "CampSpeaker", "山间营火");
		Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "CampBody", "营火尚温。选择彻底休整，或把补给留给之后的战斗。 ");
		Presentation.PrimaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "CampRest", "整顿，恢复至满血");
		Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "CampSupply", "领取疗伤散");
		Presentation.PrimaryAction = EGameXXKRouteEncounterAction::CampRest;
		Presentation.SecondaryAction = EGameXXKRouteEncounterAction::CampTakeHealingPowder;
		Presentation.bPrimaryEnabled = true;
		Presentation.bSecondaryEnabled = true;
		break;

	case EGameXXKScreen::RouteMerchant:
		Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "MerchantTitle", "行商驻足");
		Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "MerchantSpeaker", "山路行商");
		Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "MerchantBody", "行商正在收摊。选择明确离开后才会继续路线；也可以暂不离开，返回查看。 ");
		Presentation.PrimaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "MerchantLeave", "离开商队");
		Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "MerchantStay", "暂不离开");
		Presentation.CloseLabel = NSLOCTEXT("GameXXKRouteEncounter", "MerchantClose", "返回查看");
		Presentation.PrimaryAction = EGameXXKRouteEncounterAction::MerchantLeave;
		Presentation.SecondaryAction = EGameXXKRouteEncounterAction::ClosePanel;
		Presentation.bPrimaryEnabled = true;
		Presentation.bSecondaryEnabled = true;
		break;

	default:
		return false;
	}
	}

	if (TitleTextBlock) TitleTextBlock->SetText(Presentation.Title);
	if (SpeakerTextBlock) SpeakerTextBlock->SetText(Presentation.Speaker);
	if (BodyTextBlock) BodyTextBlock->SetText(Presentation.Body);
	PrimaryAction = Presentation.PrimaryAction;
	SecondaryAction = Presentation.SecondaryAction;
	TertiaryAction = Presentation.TertiaryAction;
	ApplyActionButton(PrimaryActionButton, PrimaryActionTextBlock, Presentation.PrimaryAction, Presentation.PrimaryLabel, Presentation.bPrimaryEnabled);
	ApplyActionButton(SecondaryActionButton, SecondaryActionTextBlock, Presentation.SecondaryAction, Presentation.SecondaryLabel, Presentation.bSecondaryEnabled);
	ApplyActionButton(TertiaryActionButton, TertiaryActionTextBlock, Presentation.TertiaryAction, Presentation.TertiaryLabel, Presentation.bTertiaryEnabled);
	if (PrimaryActionButton) PrimaryActionButton->SetToolTipText(Presentation.PrimaryTooltip);
	if (SecondaryActionButton) SecondaryActionButton->SetToolTipText(Presentation.SecondaryTooltip);
	if (TertiaryActionButton) TertiaryActionButton->SetToolTipText(Presentation.TertiaryTooltip);
	const bool bRouteHudChoiceMustResolve = State.Screen == EGameXXKScreen::RouteEvent;
	ApplyActionButton(CloseButton, CloseTextBlock, EGameXXKRouteEncounterAction::ClosePanel, Presentation.CloseLabel, !bRouteHudChoiceMustResolve);
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::ExecuteAction(const EGameXXKRouteEncounterAction InAction)
{
	if (InAction == EGameXXKRouteEncounterAction::None)
	{
		return false;
	}
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		return PlayerController->ResolveRouteEncounterAction(InAction);
	}
	return InAction == EGameXXKRouteEncounterAction::ClosePanel && CloseEncounterPanel();
}

void UGameXXKRouteEncounterPanelWidget::ApplyActionButton(
	UGameXXKRouteEncounterActionButton* Button,
	UTextBlock* Label,
	const EGameXXKRouteEncounterAction InAction,
	const FText& Text,
	const bool bEnabled)
{
	if (Label)
	{
		Label->SetText(Text);
		Label->SetColorAndOpacity(FSlateColor(bEnabled ? FLinearColor(0.10f, 0.075f, 0.045f, 1.0f) : FLinearColor(0.32f, 0.29f, 0.24f, 0.72f)));
	}
	if (Button)
	{
		Button->Configure(this, InAction);
		Button->SetIsEnabled(bEnabled && InAction != EGameXXKRouteEncounterAction::None);
		Button->SetVisibility(InAction == EGameXXKRouteEncounterAction::None ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UGameXXKRouteEncounterPanelWidget::HandleCloseClicked()
{
	ExecuteAction(EGameXXKRouteEncounterAction::ClosePanel);
}
