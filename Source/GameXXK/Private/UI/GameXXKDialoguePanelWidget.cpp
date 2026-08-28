#include "UI/GameXXKDialoguePanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace GameXXKDialoguePanelPrivate
{
	constexpr const TCHAR* PaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	constexpr const TCHAR* ButtonTexturePath =
		TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_ActionBlank.T_TownBackpack_ActionBlank");

	FSlateBrush TextureBrush(const TCHAR* Path, const FVector2D Size, const bool bBox)
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
		{
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
			Brush.ImageSize = Size;
			Brush.Margin = bBox ? FMargin(0.065f) : FMargin(0.0f);
		}
		return Brush;
	}

	FButtonStyle ButtonStyle()
	{
		FButtonStyle Style;
		const FSlateBrush Brush = TextureBrush(ButtonTexturePath, FVector2D(390.0f, 52.0f), true);
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	UTextBlock* Text(UWidgetTree* Tree, const FName Name, const int32 Size)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.085f, 0.045f, 1.0f)));
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = Size;
		Result->SetFont(Font);
		return Result;
	}

	void Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* CanvasSlot = Canvas ? Canvas->AddChildToCanvas(Widget) : nullptr)
		{
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
			CanvasSlot->SetZOrder(ZOrder);
		}
	}
}

void UGameXXKDialogueOptionButton::Configure(
	UGameXXKDialoguePanelWidget* InOwner,
	const int32 InOptionIndex)
{
	Owner = InOwner;
	OptionIndex = InOptionIndex;
	OnClicked.RemoveDynamic(this, &UGameXXKDialogueOptionButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKDialogueOptionButton::HandleClicked);
}

void UGameXXKDialogueOptionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->RequestOption(OptionIndex);
	}
}

TSharedRef<SWidget> UGameXXKDialoguePanelWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDialoguePanelWidget::Present(const FGameXXKDialoguePresentationView& View)
{
	BuildProgrammaticLayout();
	CurrentView = View;
	if (SpeakerText) SpeakerText->SetText(View.SpeakerDisplayName);
	if (BodyText) BodyText->SetText(View.Text);
	if (PortraitImage)
	{
		UTexture2D* Portrait = View.PortraitPath.IsNull()
			? nullptr
			: LoadObject<UTexture2D>(nullptr, *View.PortraitPath.ToString());
		PortraitImage->SetBrushFromTexture(Portrait, true);
		PortraitImage->SetVisibility(Portrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	for (int32 Index = 0; Index < OptionButtons.Num(); ++Index)
	{
		UGameXXKDialogueOptionButton* Button = OptionButtons[Index];
		UTextBlock* Label = OptionTexts.IsValidIndex(Index) ? OptionTexts[Index] : nullptr;
		const bool bVisible = View.Options.IsValidIndex(Index);
		if (Button)
		{
			Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Button->SetIsEnabled(bVisible && View.Options[Index].bEnabled);
			Button->SetToolTipText(bVisible ? View.Options[Index].DisabledReason : FText::GetEmpty());
		}
		if (Label)
		{
			Label->SetText(bVisible ? View.Options[Index].Text : FText::GetEmpty());
			Label->SetColorAndOpacity(FSlateColor(
				bVisible && View.Options[Index].bEnabled
					? FLinearColor(0.12f, 0.085f, 0.045f, 1.0f)
					: FLinearColor(0.32f, 0.29f, 0.24f, 0.75f)));
		}
	}
	if (ContinueIndicator)
	{
		ContinueIndicator->SetVisibility(View.Options.IsEmpty()
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UGameXXKDialoguePanelWidget::ClearPresentation()
{
	CurrentView = FGameXXKDialoguePresentationView();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKDialoguePanelWidget::SetAdvanceRequested(FGameXXKDialogueAdvanceRequested Delegate)
{
	AdvanceRequested = MoveTemp(Delegate);
}

void UGameXXKDialoguePanelWidget::SetOptionRequested(FGameXXKDialogueOptionRequested Delegate)
{
	OptionRequested = MoveTemp(Delegate);
}

int32 UGameXXKDialoguePanelWidget::GetPaperFrameCountForTest() const { return PaperFrame ? 1 : 0; }
int32 UGameXXKDialoguePanelWidget::GetPortraitCountForTest() const { return PortraitImage ? 1 : 0; }
bool UGameXXKDialoguePanelWidget::HasContinueIndicatorForTest() const { return ContinueIndicator != nullptr; }
FText UGameXXKDialoguePanelWidget::GetSpeakerTextForTest() const { return SpeakerText ? SpeakerText->GetText() : FText::GetEmpty(); }
FText UGameXXKDialoguePanelWidget::GetBodyTextForTest() const { return BodyText ? BodyText->GetText() : FText::GetEmpty(); }

int32 UGameXXKDialoguePanelWidget::GetVisibleOptionCountForTest() const
{
	int32 Count = 0;
	for (const UGameXXKDialogueOptionButton* Button : OptionButtons)
	{
		if (Button && Button->GetVisibility() != ESlateVisibility::Collapsed)
		{
			++Count;
		}
	}
	return Count;
}

bool UGameXXKDialoguePanelWidget::IsOptionVisibleForTest(const int32 OptionIndex) const
{
	return OptionButtons.IsValidIndex(OptionIndex)
		&& OptionButtons[OptionIndex]
		&& OptionButtons[OptionIndex]->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKDialoguePanelWidget::IsOptionEnabledForTest(const int32 OptionIndex) const
{
	return OptionButtons.IsValidIndex(OptionIndex)
		&& OptionButtons[OptionIndex]
		&& OptionButtons[OptionIndex]->GetIsEnabled();
}

FText UGameXXKDialoguePanelWidget::GetOptionTooltipForTest(const int32 OptionIndex) const
{
	return OptionButtons.IsValidIndex(OptionIndex) && OptionButtons[OptionIndex]
		? OptionButtons[OptionIndex]->GetToolTipText()
		: FText::GetEmpty();
}

bool UGameXXKDialoguePanelWidget::IsContinueIndicatorVisibleForTest() const
{
	return ContinueIndicator && ContinueIndicator->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKDialoguePanelWidget::RequestOptionForTest(const int32 OptionIndex)
{
	return RequestOption(OptionIndex);
}

void UGameXXKDialoguePanelWidget::RequestAdvanceForTest()
{
	if (CurrentView.Options.IsEmpty() && AdvanceRequested.IsBound())
	{
		AdvanceRequested.Execute();
	}
}

bool UGameXXKDialoguePanelWidget::RequestOption(const int32 OptionIndex)
{
	if (!CurrentView.Options.IsValidIndex(OptionIndex)
		|| !CurrentView.Options[OptionIndex].bEnabled
		|| !OptionRequested.IsBound())
	{
		return false;
	}
	OptionRequested.Execute(CurrentView.Options[OptionIndex].OptionId);
	return true;
}

void UGameXXKDialoguePanelWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKDialoguePanelPrivate;
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DialoguePanelWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialoguePanelRoot"));
	WidgetTree->RootWidget = RootCanvas;
	PaperFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialoguePaperFrame"));
	PaperFrame->SetBrush(TextureBrush(PaperTexturePath, FVector2D(1440.0f, 320.0f), true));
	PaperFrame->SetBrushColor(FLinearColor::White);
	Place(RootCanvas, PaperFrame, FVector2D(240.0f, 720.0f), FVector2D(1440.0f, 320.0f), 0);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialoguePanelContent"));
	PaperFrame->SetContent(Content);
	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DialoguePortrait"));
	Place(Content, PortraitImage, FVector2D(34.0f, 44.0f), FVector2D(180.0f, 220.0f), 1);
	SpeakerText = Text(WidgetTree, TEXT("DialogueSpeaker"), 24);
	Place(Content, SpeakerText, FVector2D(238.0f, 28.0f), FVector2D(650.0f, 42.0f), 1);
	BodyText = Text(WidgetTree, TEXT("DialogueBody"), 22);
	BodyText->SetAutoWrapText(true);
	Place(Content, BodyText, FVector2D(238.0f, 78.0f), FVector2D(650.0f, 180.0f), 1);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UGameXXKDialogueOptionButton* Button =
			WidgetTree->ConstructWidget<UGameXXKDialogueOptionButton>(
				UGameXXKDialogueOptionButton::StaticClass(),
				*FString::Printf(TEXT("DialogueOption%d"), Index));
		Button->Configure(this, Index);
		Button->SetStyle(ButtonStyle());
		UTextBlock* Label = Text(WidgetTree, *FString::Printf(TEXT("DialogueOptionText%d"), Index), 18);
		Label->SetJustification(ETextJustify::Center);
		Button->SetContent(Label);
		Button->SetVisibility(ESlateVisibility::Collapsed);
		Place(Content, Button, FVector2D(930.0f, 28.0f + Index * 62.0f), FVector2D(430.0f, 54.0f), 1);
		OptionButtons.Add(Button);
		OptionTexts.Add(Label);
	}
	ContinueIndicator = Text(WidgetTree, TEXT("DialogueContinueIndicator"), 18);
	ContinueIndicator->SetText(FText::FromString(TEXT("点击 / 空格继续  ▶")));
	ContinueIndicator->SetJustification(ETextJustify::Right);
	Place(Content, ContinueIndicator, FVector2D(1030.0f, 258.0f), FVector2D(330.0f, 34.0f), 1);
	ClearPresentation();
}
