#include "UI/GameXXKRouteEncounterPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D EncounterPanelSize(900.0f, 520.0f);
	const FVector2D EncounterActionSize(226.0f, 56.0f);
	const FVector2D EncounterChoiceCardSize(226.0f, 250.0f);
	const FVector2D EncounterSelectionInkSize(54.0f, 54.0f);
	const FVector2D EncounterSelectionInkPosition(164.0f, 8.0f);
	const FVector2D EncounterCloseSize(74.0f, 74.0f);
	const FMargin WindowFrameMargin(0.065f);
	const FMargin ActionFrameMargin(5.0f / 73.0f, 5.0f / 31.0f, 5.0f / 73.0f, 5.0f / 31.0f);
	const FString BackpackTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/"));
	const FString WindowFrameTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
	const FString HeaderTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Header.T_TownBackpack_Header"));
	const FString ActionTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_ActionBlank.T_TownBackpack_ActionBlank"));
	const FString ApprovedTextureRoot(TEXT("/Game/GameXXK/UI/MasterV2/Approved/"));
	const FString CardFrameTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CardFrame.T_MasterV2_CardFrame"));
	const FString CloseInkTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CloseInk.T_MasterV2_CloseInk"));
	const FString SelectionInkTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_SquareSelected.T_MasterV2_SquareSelected"));
	const FString RewardIconTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CardLockedIcon.T_MasterV2_CardLockedIcon"));

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

	FButtonStyle MakeImageButtonStyle(const FString& TexturePath, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(TexturePath, ImageSize));
		Style.SetHovered(MakeTextureBrush(TexturePath, ImageSize));
		Style.SetPressed(MakeTextureBrush(TexturePath, ImageSize));
		Style.SetDisabled(MakeTextureBrush(TexturePath, ImageSize));
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

	UTextBlock* MakeInkText(
		UWidgetTree* WidgetTree,
		const FText& Text,
		const int32 FontSize,
		const FLinearColor& Ink = FLinearColor(0.10f, 0.075f, 0.045f, 1.0f),
		const FName WidgetName = NAME_None)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
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

	bool IsRouteEncounterScreen(const EGameXXKScreen Screen)
	{
		return Screen == EGameXXKScreen::RouteEvent
			|| Screen == EGameXXKScreen::RouteCamp
			|| Screen == EGameXXKScreen::RouteMerchant;
	}

	bool IsThreeCardChoiceAction(const EGameXXKRouteEncounterAction Action)
	{
		return Action == EGameXXKRouteEncounterAction::SelectChoice0
			|| Action == EGameXXKRouteEncounterAction::SelectChoice1
			|| Action == EGameXXKRouteEncounterAction::SelectChoice2;
	}

	FGameXXKRouteChoicePresentationIdentity BuildChoicePresentationIdentity(const FGameXXKRuntimeState& State)
	{
		FGameXXKRouteChoicePresentationIdentity Identity;
		Identity.Screen = State.Screen;
		Identity.PendingNodeId = State.PendingRouteNodeId;
		Identity.EventSourceNodeId = State.CardRun.PendingEvent.SourceNodeId;
		Identity.EventChoiceSeed = State.CardRun.PendingEvent.ChoiceSeed;
		Identity.EncounterId = State.CardRun.PendingEvent.EncounterId;
		Identity.RelicSourceNodeId = State.CardRun.PendingRelicOffer.SourceNodeId;
		Identity.RelicChoiceSeed = State.CardRun.PendingRelicOffer.ChoiceSeed;
		Identity.RelicIds = State.CardRun.PendingRelicOffer.RelicIds;
		return Identity;
	}
}

void UGameXXKRouteEncounterActionButton::Configure(UGameXXKRouteEncounterPanelWidget* InOwner, const EGameXXKRouteEncounterAction InAction)
{
	Owner = InOwner;
	Action = InAction;
	ChoiceIndex = INDEX_NONE;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKRouteEncounterActionButton::HandleClicked);
}

void UGameXXKRouteEncounterActionButton::ConfigureChoice(UGameXXKRouteEncounterPanelWidget* InOwner, const int32 InChoiceIndex)
{
	Owner = InOwner;
	Action = EGameXXKRouteEncounterAction::None;
	ChoiceIndex = InChoiceIndex;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKRouteEncounterActionButton::HandleClicked);
}

void UGameXXKRouteEncounterActionButton::HandleClicked()
{
	if (Owner)
	{
		if (ChoiceIndex != INDEX_NONE)
		{
			Owner->SelectChoice(ChoiceIndex);
		}
		else
		{
			Owner->ExecuteAction(Action);
		}
	}
}

TSharedRef<SWidget> UGameXXKRouteEncounterPanelWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	RegisterGuideTargets();
	return Super::RebuildWidget();
}

void UGameXXKRouteEncounterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	CloseEncounterPanel();
}

void UGameXXKRouteEncounterPanelWidget::NativeDestruct()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	Registry.UnregisterTarget(TEXT("Route.Event.ValidChoiceGroup"), FrameCanvas);
	Registry.UnregisterTarget(TEXT("Route.Camp.Heal"), PrimaryActionButton);
	Registry.UnregisterTarget(TEXT("Route.Camp.Gold"), SecondaryActionButton);
	for (UGameXXKRouteEncounterActionButton* Button : ChoiceCardButtons)
	{
		Registry.UnregisterTarget(TEXT("Route.Chest.Open"), Button);
	}
	Super::NativeDestruct();
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
	RegisterGuideTargets();
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	FName OpenEventId;
	if (State.Screen == EGameXXKScreen::RouteCamp)
	{
		OpenEventId = TEXT("Event.Route.CampOpened");
	}
	else
	{
		const FGameXXKRouteMapNode* PendingNode = State.RouteMapNodes.FindByPredicate(
			[&State](const FGameXXKRouteMapNode& Node) { return Node.NodeId == State.PendingRouteNodeId; });
		OpenEventId = PendingNode && PendingNode->NodeKind == EGameXXKNodeKind::Chest
			? FName(TEXT("Event.Route.ChestOpened"))
			: FName(TEXT("Event.Route.EventOpened"));
	}
	FGameXXKGuideTargetRegistry::Get().EmitEvent(OpenEventId);
	bGuideEncounterOpenedEmitted = true;
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::CloseEncounterPanel()
{
	const bool bWasOpen = IsEncounterPanelOpenForTest();
	SelectedChoiceIndex = INDEX_NONE;
	ChoicePresentationIdentity.Reset();
	RefreshChoiceCardStates();
	SetVisibility(ESlateVisibility::Collapsed);
	bGuideEncounterOpenedEmitted = false;
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
	return IsThreeCardChoiceAction(PrimaryAction)
		? SelectChoice(0)
		: PrimaryActionButton && PrimaryActionButton->GetIsEnabled() && ExecuteAction(PrimaryAction);
}

bool UGameXXKRouteEncounterPanelWidget::TriggerSecondaryActionForTest()
{
	return IsThreeCardChoiceAction(SecondaryAction)
		? SelectChoice(1)
		: SecondaryActionButton && SecondaryActionButton->GetIsEnabled() && ExecuteAction(SecondaryAction);
}

bool UGameXXKRouteEncounterPanelWidget::TriggerTertiaryActionForTest()
{
	return IsThreeCardChoiceAction(TertiaryAction)
		? SelectChoice(2)
		: TertiaryActionButton && TertiaryActionButton->GetIsEnabled() && ExecuteAction(TertiaryAction);
}

bool UGameXXKRouteEncounterPanelWidget::SelectChoiceForTest(const int32 ChoiceIndex)
{
	return SelectChoice(ChoiceIndex);
}

bool UGameXXKRouteEncounterPanelWidget::ConfirmSelectedChoiceForTest()
{
	return ConfirmSelectedChoice();
}

int32 UGameXXKRouteEncounterPanelWidget::GetRenderedChoiceCardCountForTest() const
{
	int32 Count = 0;
	for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceCardButtons.Num(); ++ChoiceIndex)
	{
		const UGameXXKRouteEncounterActionButton* Button = ChoiceCardButtons[ChoiceIndex];
		if (Button && Button->GetVisibility() != ESlateVisibility::Collapsed && ChoiceActions.IsValidIndex(ChoiceIndex)
			&& ChoiceActions[ChoiceIndex] != EGameXXKRouteEncounterAction::None)
		{
			++Count;
		}
	}
	return Count;
}

void UGameXXKRouteEncounterPanelWidget::BuildProgrammaticLayout()
{
	SetIsFocusable(true);
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

	ChoiceCardButtons.Reserve(3);
	ChoiceArtImages.Reserve(3);
	ChoiceNameTexts.Reserve(3);
	ChoiceDescriptionTexts.Reserve(3);
	ChoiceDisabledReasonTexts.Reserve(3);
	ChoiceSelectionInks.Reserve(3);
	for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
	{
		UGameXXKRouteEncounterActionButton* ChoiceButton = WidgetTree->ConstructWidget<UGameXXKRouteEncounterActionButton>(
			UGameXXKRouteEncounterActionButton::StaticClass(),
			*FString::Printf(TEXT("RouteEncounterChoiceCard%d"), ChoiceIndex));
		ChoiceButton->SetStyle(MakeImageButtonStyle(CardFrameTexturePath, EncounterChoiceCardSize));
		ChoiceButton->ConfigureChoice(this, ChoiceIndex);
		ChoiceButton->SetVisibility(ESlateVisibility::Collapsed);

		UCanvasPanel* Face = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("RouteEncounterChoiceFace%d"), ChoiceIndex));
		ChoiceButton->SetContent(Face);

		UImage* Art = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("RouteEncounterChoiceArt%d"), ChoiceIndex));
		Art->SetBrush(MakeTextureBrush(RewardIconTexturePath, FVector2D(104.0f, 82.0f)));
		Art->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddCanvasChild(Face, Art, FVector2D(61.0f, 27.0f), FVector2D(104.0f, 82.0f));

		UTextBlock* Name = MakeInkText(
			WidgetTree,
			FText::GetEmpty(),
			17,
			FLinearColor(0.10f, 0.075f, 0.045f, 1.0f),
			*FString::Printf(TEXT("RouteEncounterChoiceName%d"), ChoiceIndex));
		Name->SetJustification(ETextJustify::Center);
		AddCanvasChild(Face, Name, FVector2D(20.0f, 118.0f), FVector2D(186.0f, 32.0f));

		UTextBlock* Description = MakeInkText(
			WidgetTree,
			FText::GetEmpty(),
			13,
			FLinearColor(0.20f, 0.15f, 0.10f, 1.0f),
			*FString::Printf(TEXT("RouteEncounterChoiceDescription%d"), ChoiceIndex));
		Description->SetJustification(ETextJustify::Center);
		AddCanvasChild(Face, Description, FVector2D(17.0f, 153.0f), FVector2D(192.0f, 54.0f));

		UTextBlock* DisabledReason = MakeInkText(
			WidgetTree,
			FText::GetEmpty(),
			12,
			FLinearColor(0.48f, 0.12f, 0.08f, 1.0f),
			*FString::Printf(TEXT("RouteEncounterChoiceDisabledReason%d"), ChoiceIndex));
		DisabledReason->SetJustification(ETextJustify::Center);
		DisabledReason->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvasChild(Face, DisabledReason, FVector2D(17.0f, 211.0f), FVector2D(192.0f, 28.0f));

		UImage* SelectionInk = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("RouteEncounterChoiceSelectionInk%d"), ChoiceIndex));
		SelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, EncounterSelectionInkSize));
		SelectionInk->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvasChild(Face, SelectionInk, EncounterSelectionInkPosition, EncounterSelectionInkSize);

		ChoiceCardButtons.Add(ChoiceButton);
		ChoiceArtImages.Add(Art);
		ChoiceNameTexts.Add(Name);
		ChoiceDescriptionTexts.Add(Description);
		ChoiceDisabledReasonTexts.Add(DisabledReason);
		ChoiceSelectionInks.Add(SelectionInk);
		AddCanvasChild(FrameCanvas, ChoiceButton, FVector2D(58.0f + 260.0f * ChoiceIndex, 145.0f), EncounterChoiceCardSize);
	}

	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RouteEncounterConfirmAction"));
	ConfirmButton->SetStyle(MakeActionButtonStyle());
	ConfirmButton->OnClicked.AddDynamic(this, &UGameXXKRouteEncounterPanelWidget::HandleConfirmClicked);
	ConfirmTextBlock = MakeInkText(WidgetTree, NSLOCTEXT("GameXXKRouteEncounter", "ConfirmChoice", "确认选择"), 18);
	if (ConfirmTextBlock)
	{
		ConfirmTextBlock->SetJustification(ETextJustify::Center);
		ConfirmButton->SetContent(ConfirmTextBlock);
	}
	ConfirmButton->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(FrameCanvas, ConfirmButton, FVector2D(318.0f, 405.0f), EncounterActionSize);

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
	CloseButton->SetStyle(MakeImageButtonStyle(CloseInkTexturePath, EncounterCloseSize));
	CloseButton->Configure(this, EGameXXKRouteEncounterAction::ClosePanel);
	CloseButton->SetToolTipText(NSLOCTEXT("GameXXKRouteEncounter", "ReturnToRouteMap", "返回路线图（本节点不会结算）"));
	AddCanvasChild(FrameCanvas, CloseButton, FVector2D(738.0f, -8.0f), EncounterCloseSize);

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
				*Tooltips[ChoiceIndex] = Choice.Label;
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
	{
		Presentation.Title = NSLOCTEXT("GameXXKRouteEncounter", "CampTitle", "营火抉择");
		Presentation.Speaker = NSLOCTEXT("GameXXKRouteEncounter", "CampSpeaker", "山间营火");
		Presentation.Body = NSLOCTEXT("GameXXKRouteEncounter", "CampBody", "营火尚温。选择让全队休整，或领取本局行旅钱。");
		Presentation.PrimaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "CampHeal", "全队恢复30%气血");
		Presentation.SecondaryLabel = NSLOCTEXT("GameXXKRouteEncounter", "CampRouteMoney", "获得100局内金币");
		Presentation.PrimaryTooltip = NSLOCTEXT("GameXXKRouteEncounter", "CampHealTooltip", "每名当前队员恢复其最大气血的30%，不超过上限。");
		Presentation.SecondaryTooltip = NSLOCTEXT("GameXXKRouteEncounter", "CampRouteMoneyTooltip", "本局行旅钱增加100。");
		Presentation.PrimaryAction = EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman;
		Presentation.SecondaryAction = EGameXXKRouteEncounterAction::CampTakeRouteMoney;
		Presentation.bPrimaryEnabled = true;
		Presentation.bSecondaryEnabled = true;
		break;
	}

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

	ChoiceActions = {Presentation.PrimaryAction, Presentation.SecondaryAction, Presentation.TertiaryAction};
	const bool bThreeCardMode = IsThreeCardChoiceAction(Presentation.PrimaryAction)
		&& IsThreeCardChoiceAction(Presentation.SecondaryAction)
		&& IsThreeCardChoiceAction(Presentation.TertiaryAction);
	if (bThreeCardMode)
	{
		const FGameXXKRouteChoicePresentationIdentity NewIdentity = BuildChoicePresentationIdentity(State);
		if (!ChoicePresentationIdentity.IsSet() || !(ChoicePresentationIdentity.GetValue() == NewIdentity))
		{
			SelectedChoiceIndex = INDEX_NONE;
		}
		ChoicePresentationIdentity = NewIdentity;
	}
	else
	{
		SelectedChoiceIndex = INDEX_NONE;
		ChoicePresentationIdentity.Reset();
	}
	const FText ChoiceLabels[] = {Presentation.PrimaryLabel, Presentation.SecondaryLabel, Presentation.TertiaryLabel};
	const FText ChoiceDescriptions[] = {Presentation.PrimaryTooltip, Presentation.SecondaryTooltip, Presentation.TertiaryTooltip};
	const bool ChoiceEnabled[] = {Presentation.bPrimaryEnabled, Presentation.bSecondaryEnabled, Presentation.bTertiaryEnabled};
	if (UCanvasPanelSlot* BodySlot = BodyTextBlock ? Cast<UCanvasPanelSlot>(BodyTextBlock->Slot) : nullptr)
	{
		BodySlot->SetPosition(bThreeCardMode ? FVector2D(12.0f, 100.0f) : FVector2D(12.0f, 132.0f));
		BodySlot->SetSize(bThreeCardMode ? FVector2D(800.0f, 44.0f) : FVector2D(800.0f, 162.0f));
	}
	for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceCardButtons.Num(); ++ChoiceIndex)
	{
		UGameXXKRouteEncounterActionButton* ChoiceButton = ChoiceCardButtons[ChoiceIndex];
		const bool bVisibleChoice = bThreeCardMode && ChoiceActions.IsValidIndex(ChoiceIndex)
			&& IsThreeCardChoiceAction(ChoiceActions[ChoiceIndex]);
		if (ChoiceButton)
		{
			ChoiceButton->ConfigureChoice(this, ChoiceIndex);
			ChoiceButton->SetVisibility(bVisibleChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			ChoiceButton->SetIsEnabled(bVisibleChoice && ChoiceEnabled[ChoiceIndex]);
			ChoiceButton->SetToolTipText(ChoiceDescriptions[ChoiceIndex]);
		}
		if (ChoiceNameTexts.IsValidIndex(ChoiceIndex) && ChoiceNameTexts[ChoiceIndex])
		{
			ChoiceNameTexts[ChoiceIndex]->SetText(ChoiceLabels[ChoiceIndex]);
		}
		if (ChoiceDescriptionTexts.IsValidIndex(ChoiceIndex) && ChoiceDescriptionTexts[ChoiceIndex])
		{
			ChoiceDescriptionTexts[ChoiceIndex]->SetText(ChoiceEnabled[ChoiceIndex]
				? ChoiceDescriptions[ChoiceIndex]
				: ChoiceLabels[ChoiceIndex]);
		}
		if (ChoiceDisabledReasonTexts.IsValidIndex(ChoiceIndex) && ChoiceDisabledReasonTexts[ChoiceIndex])
		{
			ChoiceDisabledReasonTexts[ChoiceIndex]->SetText(ChoiceEnabled[ChoiceIndex] ? FText::GetEmpty() : ChoiceDescriptions[ChoiceIndex]);
			ChoiceDisabledReasonTexts[ChoiceIndex]->SetVisibility(
				bVisibleChoice && !ChoiceEnabled[ChoiceIndex] ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (ChoiceArtImages.IsValidIndex(ChoiceIndex) && ChoiceArtImages[ChoiceIndex])
		{
			FString ArtPath = RewardIconTexturePath;
			if (State.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(ChoiceIndex))
			{
				if (const FGameXXKRelicDefinition* Relic = FGameXXKRelicCatalog::FindDefinition(State.CardRun.PendingRelicOffer.RelicIds[ChoiceIndex]))
				{
					if (Relic->IconTexturePath.IsValid())
					{
						ArtPath = Relic->IconTexturePath.ToString();
					}
				}
			}
			ChoiceArtImages[ChoiceIndex]->SetBrush(MakeTextureBrush(ArtPath, FVector2D(104.0f, 82.0f)));
		}
	}
	if (PrimaryActionButton) PrimaryActionButton->SetVisibility(bThreeCardMode ? ESlateVisibility::Collapsed : PrimaryActionButton->GetVisibility());
	if (SecondaryActionButton) SecondaryActionButton->SetVisibility(bThreeCardMode ? ESlateVisibility::Collapsed : SecondaryActionButton->GetVisibility());
	if (TertiaryActionButton) TertiaryActionButton->SetVisibility(bThreeCardMode ? ESlateVisibility::Collapsed : TertiaryActionButton->GetVisibility());
	if (ConfirmButton)
	{
		ConfirmButton->SetVisibility(bThreeCardMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	RefreshChoiceCardStates();

	const bool bCanReturnUnresolved = State.Screen == EGameXXKScreen::RouteEvent || State.Screen == EGameXXKScreen::RouteCamp;
	ApplyActionButton(
		CloseButton,
		CloseTextBlock,
		EGameXXKRouteEncounterAction::ClosePanel,
		Presentation.CloseLabel,
		bCanReturnUnresolved);
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::SelectChoice(const int32 ChoiceIndex)
{
	if (!ChoiceActions.IsValidIndex(ChoiceIndex)
		|| !IsThreeCardChoiceAction(ChoiceActions[ChoiceIndex])
		|| !ChoiceCardButtons.IsValidIndex(ChoiceIndex)
		|| !ChoiceCardButtons[ChoiceIndex]
		|| ChoiceCardButtons[ChoiceIndex]->GetVisibility() == ESlateVisibility::Collapsed
		|| !ChoiceCardButtons[ChoiceIndex]->GetIsEnabled())
	{
		return false;
	}
	SelectedChoiceIndex = ChoiceIndex;
	RefreshChoiceCardStates();
	return true;
}

bool UGameXXKRouteEncounterPanelWidget::ConfirmSelectedChoice()
{
	if (!ChoiceActions.IsValidIndex(SelectedChoiceIndex)
		|| !IsThreeCardChoiceAction(ChoiceActions[SelectedChoiceIndex]))
	{
		return false;
	}
	return ExecuteAction(ChoiceActions[SelectedChoiceIndex]);
}

void UGameXXKRouteEncounterPanelWidget::RefreshChoiceCardStates()
{
	for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceSelectionInks.Num(); ++ChoiceIndex)
	{
		if (ChoiceSelectionInks[ChoiceIndex])
		{
			ChoiceSelectionInks[ChoiceIndex]->SetVisibility(
				ChoiceIndex == SelectedChoiceIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (ConfirmButton)
	{
		const bool bCanConfirm = ChoiceActions.IsValidIndex(SelectedChoiceIndex)
			&& IsThreeCardChoiceAction(ChoiceActions[SelectedChoiceIndex])
			&& ChoiceCardButtons.IsValidIndex(SelectedChoiceIndex)
			&& ChoiceCardButtons[SelectedChoiceIndex]
			&& ChoiceCardButtons[SelectedChoiceIndex]->GetIsEnabled();
		ConfirmButton->SetIsEnabled(bCanConfirm);
	}
}

bool UGameXXKRouteEncounterPanelWidget::ExecuteAction(const EGameXXKRouteEncounterAction InAction)
{
	if (InAction == EGameXXKRouteEncounterAction::None)
	{
		return false;
	}
	const FName GuideActionId = ResolveGuideActionId(InAction);
	if (!GuideActionId.IsNone()
		&& !FGameXXKGuideTargetRegistry::Get().IsActionAllowed(GuideActionId))
	{
		return false;
	}
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		const bool bResolved = PlayerController->ResolveRouteEncounterAction(InAction);
		if (bResolved)
		{
			if (InAction == EGameXXKRouteEncounterAction::CampRest
				|| InAction == EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman)
			{
				FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Route.CampHealResolved"));
			}
			else if (InAction == EGameXXKRouteEncounterAction::CampTakeRouteMoney)
			{
				FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Route.CampGoldResolved"));
			}
			const FName CompletionEventId = ResolveGuideCompletionEventId(InAction);
			if (!CompletionEventId.IsNone())
			{
				FGameXXKGuideTargetRegistry::Get().EmitEvent(CompletionEventId);
			}
		}
		return bResolved;
	}
	return InAction == EGameXXKRouteEncounterAction::ClosePanel && CloseEncounterPanel();
}

void UGameXXKRouteEncounterPanelWidget::RegisterGuideTargets()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	if (FrameCanvas)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Event.ValidChoiceGroup"), FrameCanvas);
	}
	if (PrimaryActionButton)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Camp.Heal"), PrimaryActionButton);
	}
	if (SecondaryActionButton)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Camp.Gold"), SecondaryActionButton);
	}
	if (!ChoiceCardButtons.IsEmpty() && ChoiceCardButtons[0])
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Chest.Open"), ChoiceCardButtons[0]);
	}
}

FName UGameXXKRouteEncounterPanelWidget::ResolveGuideActionId(
	const EGameXXKRouteEncounterAction InAction) const
{
	switch (InAction)
	{
	case EGameXXKRouteEncounterAction::CampRest:
	case EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman:
		return TEXT("Action.Route.CampHeal");
	case EGameXXKRouteEncounterAction::CampTakeRouteMoney:
		return TEXT("Action.Route.CampGold");
	case EGameXXKRouteEncounterAction::SelectChoice0:
	case EGameXXKRouteEncounterAction::SelectChoice1:
	case EGameXXKRouteEncounterAction::SelectChoice2:
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
		const FGameXXKRouteMapNode* PendingNode = State
			? State->RouteMapNodes.FindByPredicate(
				[State](const FGameXXKRouteMapNode& Node) { return Node.NodeId == State->PendingRouteNodeId; })
			: nullptr;
		return PendingNode && PendingNode->NodeKind == EGameXXKNodeKind::Chest
			? FName(TEXT("Action.Route.ChestOpen"))
			: FName(TEXT("Action.Route.EventChoose"));
	}
	default:
		return NAME_None;
	}
}

FName UGameXXKRouteEncounterPanelWidget::ResolveGuideCompletionEventId(
	const EGameXXKRouteEncounterAction InAction) const
{
	switch (InAction)
	{
	case EGameXXKRouteEncounterAction::CampRest:
	case EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman:
		return TEXT("Event.Route.CampResolved");
	case EGameXXKRouteEncounterAction::CampTakeRouteMoney:
		return TEXT("Event.Route.CampResolved");
	case EGameXXKRouteEncounterAction::SelectChoice0:
	case EGameXXKRouteEncounterAction::SelectChoice1:
	case EGameXXKRouteEncounterAction::SelectChoice2:
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
		if (State)
		{
			const FGameXXKRouteMapNode* PendingNode = State->RouteMapNodes.FindByPredicate(
				[State](const FGameXXKRouteMapNode& Node) { return Node.NodeId == State->PendingRouteNodeId; });
			if (PendingNode && PendingNode->NodeKind == EGameXXKNodeKind::Chest)
			{
				return TEXT("Event.Route.ChestRewardResolved");
			}
		}
		return TEXT("Event.Route.EventChoiceResolved");
	}
	default:
		return NAME_None;
	}
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

void UGameXXKRouteEncounterPanelWidget::HandleConfirmClicked()
{
	ConfirmSelectedChoice();
}
