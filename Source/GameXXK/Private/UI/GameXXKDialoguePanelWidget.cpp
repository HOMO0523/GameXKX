#include "UI/GameXXKDialoguePanelWidget.h"
#include "Audio/GameXXKSfx.h"
#include "UI/GameXXKLocalization.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "UI/GameXXKPartyDeckUiStyle.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKDesktopPaperStyle.h"

namespace GameXXKDialoguePanelPrivate
{
	FBox2f BustRegion(const FString& Path)
	{
		if(Path.Contains(TEXT("HeroFullBody")))return FBox2f(FVector2f(.2266f,.0879f),FVector2f(.7441f,.6075f));
		// Youbai is a fire spirit: keep the entire flame and its trailing tail.
		if(Path.Contains(TEXT("YueBai")))return FBox2f(FVector2f(.2188f,.1914f),FVector2f(.7285f,.8965f));
		if(Path.Contains(TEXT("JinGui")))return FBox2f(FVector2f(.2461f,.2227f),FVector2f(.8203f,.6575f));
		if(Path.Contains(TEXT("ZhouGuangZu")))return FBox2f(FVector2f(.1172f,.0957f),FVector2f(.8438f,.6104f));
		if(Path.Contains(TEXT("TusiChief")))return FBox2f(FVector2f(.1094f,.2188f),FVector2f(.7656f,.6572f));
		if(Path.Contains(TEXT("SongJinBao")))return FBox2f(FVector2f(.2871f,.0781f),FVector2f(.7949f,.6038f));
		if(Path.Contains(TEXT("QiongMeiEr")))return FBox2f(FVector2f(.2051f,.1055f),FVector2f(.8242f,.6141f));
		return FBox2f(FVector2f(0,0),FVector2f(1,1));
	}

	FButtonStyle ButtonStyle()
	{
		return FGameXXKInRunUiStyle::Action(FVector2D(620,46),true);
	}

	UTextBlock* Text(UWidgetTree* Tree, const FName Name, const int32 Size, const EGameXXKFontRole Role = EGameXXKFontRole::Body)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.085f, 0.045f, 1.0f)));
		Result->SetFont(FGameXXKInRunUiStyle::Font(Role, Size));
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
	const bool bPortraitChanged=CurrentView.PortraitPath!=View.PortraitPath;
	const bool bOptionLayoutChanged=CurrentView.Options.Num()!=View.Options.Num();
	const bool bNodeChanged=CurrentView.NodeId!=View.NodeId;
	const bool bTextChanged=!CurrentView.Text.EqualTo(View.Text);
	CurrentView = View;
	if (SpeakerText) SpeakerText->SetText(GameXXKLocalization::Localize(View.SpeakerDisplayName));
	if (BodyText) BodyText->SetText(GameXXKLocalization::Localize(View.Text));
	if (PortraitImage && (bPortraitChanged || !PortraitImage->GetBrush().GetResourceObject()))
	{
		UTexture2D* Portrait = View.PortraitPath.IsNull()
			? nullptr
			: LoadObject<UTexture2D>(nullptr, *View.PortraitPath.ToString());
		PortraitImage->SetBrushFromTexture(Portrait, !bCompactLayout);
		if(bCompactLayout && Portrait)
		{
			FSlateBrush Brush=PortraitImage->GetBrush();const FBox2f Region=GameXXKDialoguePanelPrivate::BustRegion(View.PortraitPath.ToString());
			Brush.SetUVRegion(Region);Brush.ImageSize=FVector2D(Region.GetSize())*512;PortraitImage->SetBrush(Brush);
		}
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
			Button->SetToolTipText(bVisible ? GameXXKLocalization::Localize(View.Options[Index].DisabledReason) : FText::GetEmpty());
		}
		if (Label)
		{
			Label->SetText(bVisible ? GameXXKLocalization::Localize(View.Options[Index].Text) : FText::GetEmpty());
			Label->SetAutoWrapText(true);
			Label->SetColorAndOpacity(FSlateColor(
				bVisible && View.Options[Index].bEnabled
					? FLinearColor(.96f,.92f,.81f,1)
					: FLinearColor(.66f,.64f,.57f,.8f)));
		}
	}
	if (ContinueIndicator)
	{
		ContinueIndicator->SetVisibility(View.Options.IsEmpty()
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if(bCompactLayout && (bOptionLayoutChanged || bPortraitChanged))RefreshCompactLayout();
	if(bCompactLayout)if(auto* Reading=Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("DialogueBodyScroll"))))
	{
		if(bNodeChanged)Reading->ScrollToStart();
		else if(bTextChanged && !View.Options.IsEmpty())Reading->ScrollToEnd();
	}
	if(CloseButton)CloseButton->SetVisibility(PauseRequested.IsBound()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	if(HintButton)HintButton->SetVisibility(HintRequested.IsBound() && !CurrentView.Options.IsEmpty() && CurrentView.Options[0].OptionId.ToString().StartsWith(TEXT("MainStory.Choice."))?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Visible);
}

void UGameXXKDialoguePanelWidget::ClearPresentation()
{
	CurrentView = FGameXXKDialoguePresentationView();
	if(PortraitImage)PortraitImage->SetBrushFromTexture(nullptr,false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKDialoguePanelWidget::SetPauseRequested(FGameXXKDialogueAdvanceRequested Delegate){PauseRequested=MoveTemp(Delegate);if(CloseButton)CloseButton->SetVisibility(PauseRequested.IsBound()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
void UGameXXKDialoguePanelWidget::SetHintRequested(FGameXXKDialogueAdvanceRequested Delegate){HintRequested=MoveTemp(Delegate);if(HintButton)HintButton->SetVisibility(HintRequested.IsBound() && !CurrentView.Options.IsEmpty() && CurrentView.Options[0].OptionId.ToString().StartsWith(TEXT("MainStory.Choice."))?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}

void UGameXXKDialoguePanelWidget::SetCompactLayout(bool bCompact)
{
	BuildProgrammaticLayout();
	if(!bCompact || bCompactLayout)return;
	bCompactLayout=true;
	UCanvasPanel* TextCanvas=Cast<UCanvasPanel>(BodyText->GetParent());
	BodyText->RemoveFromParent();
	auto* ReadingArea=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("DialogueBodyScroll"));
	ReadingArea->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(ReadingArea,5);
	ReadingArea->SetScrollbarThickness(FVector2D(5,5));
	ReadingArea->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	FSlateBrush None;None.DrawAs=ESlateBrushDrawType::NoDrawType;
	FScrollBoxStyle ReadingStyle;ReadingStyle.SetTopShadowBrush(None).SetBottomShadowBrush(None).SetLeftShadowBrush(None).SetRightShadowBrush(None);
	ReadingArea->SetWidgetStyle(ReadingStyle);
	ReadingArea->AddChild(BodyText);BodyText->SetWrapTextAt(630);
	GameXXKDialoguePanelPrivate::Place(TextCanvas,ReadingArea,FVector2D(36,70),FVector2D(650,88),1);
	PortraitImage->RemoveFromParent();
	CompactPortraitScale=WidgetTree->ConstructWidget<UScaleBox>();
	CompactPortraitScale->SetStretch(EStretch::ScaleToFit);
	CompactPortraitScale->SetContent(PortraitImage);
	GameXXKDialoguePanelPrivate::Place(TextCanvas,CompactPortraitScale,FVector2D(24,18),FVector2D(210,184),2);
	CompactPortraitScale->SetClipping(EWidgetClipping::ClipToBounds);
	auto* Design=WidgetTree->ConstructWidget<USizeBox>();Design->SetWidthOverride(945);Design->SetHeightOverride(430);Design->SetContent(RootCanvas);
	auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);Fit->SetContent(Design);
	WidgetTree->RootWidget=Fit;
	SetIsFocusable(true);
	RefreshCompactLayout();
}

void UGameXXKDialoguePanelWidget::RefreshCompactLayout()
{
	if(!bCompactLayout)return;
	const auto Move=[](UWidget* Widget,FVector2D Position,FVector2D Size)
	{
		if(auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot)){Slot->SetPosition(Position);Slot->SetSize(Size);}
	};
	const bool bChoices=!CurrentView.Options.IsEmpty();
	const float Height=bChoices?196.f+CurrentView.Options.Num()*48.f:220.f;
	Move(PaperFrame,FVector2D(0,424-Height),FVector2D(924,Height));
	const bool PortraitVisible=!CurrentView.PortraitPath.IsNull();
	const float TextLeft=PortraitVisible?260.f:40.f;
	const float TextWidth=884.f-TextLeft;
	Move(CompactPortraitScale,FVector2D(24,18),FVector2D(210,Height-38));
	CompactPortraitScale->SetVisibility(PortraitVisible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
	Move(SpeakerText,FVector2D(TextLeft,22),FVector2D(TextWidth-42,36));
	SpeakerText->SetFont(FGameXXKInRunUiStyle::TitleFont(25));
	SpeakerText->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Jade()));
	Move(WidgetTree->FindWidget(TEXT("DialogueBodyScroll")),FVector2D(TextLeft,70),FVector2D(TextWidth,bChoices?76:88));
	BodyText->SetFont(FGameXXKInRunUiStyle::BodyFont(22));
	BodyText->SetWrapTextAt(TextWidth-12);
	BodyText->SetLineHeightPercentage(1.22f);
	Move(ContinueIndicator,FVector2D(654,Height-42),FVector2D(232,26));
	ContinueIndicator->SetFont(FGameXXKInRunUiStyle::BodyFont(15));
	ContinueIndicator->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::MutedInk()));
	for(int32 I=0;I<OptionButtons.Num();++I)Move(OptionButtons[I],FVector2D(TextLeft,154+I*48),FVector2D(TextWidth,43));
	for(UTextBlock* Option:OptionTexts)if(Option){Option->SetWrapTextAt(TextWidth-24);Option->SetLineHeightPercentage(1.f);}
	Move(CloseButton,FVector2D(868,15),FVector2D(40,40));
	Move(HintButton,FVector2D(TextLeft,Height-39),FVector2D(88,28));
}

FReply UGameXXKDialoguePanelWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	if(Event.GetKey()==EKeys::Escape && PauseRequested.IsBound()){PauseRequested.Execute();return FReply::Handled();}
	if((Event.GetKey()==EKeys::SpaceBar || Event.GetKey()==EKeys::Enter) && CurrentView.Options.IsEmpty())
	{RequestAdvanceForTest();return FReply::Handled();}
	return Super::NativeOnKeyDown(Geometry,Event);
}

FReply UGameXXKDialoguePanelWidget::NativeOnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
	if(bCompactLayout && Event.GetEffectingButton()==EKeys::LeftMouseButton)
	{if(CurrentView.Options.IsEmpty())RequestAdvanceForTest();return FReply::Handled();}
	return Super::NativeOnMouseButtonDown(Geometry,Event);
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
	if(OptionIndex==-2 && PauseRequested.IsBound()){PauseRequested.Execute();return true;}
	if(OptionIndex==-3 && HintRequested.IsBound()){HintRequested.Execute();return true;}
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
	PaperFrame = GameXXKDesktopPaperStyle::MakePanel(WidgetTree,TEXT("DialoguePaperFrame"),FVector2D(945,533),FLinearColor(.8f,.74f,.61f,1));
	Place(RootCanvas, PaperFrame, FVector2D(240.0f, 720.0f), FVector2D(1440.0f, 320.0f), 0);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialoguePanelContent"));
	GameXXKDesktopPaperStyle::SetPanelContent(PaperFrame,Content,FMargin(0));
	PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DialoguePortrait"));
	Place(Content, PortraitImage, FVector2D(34.0f, 44.0f), FVector2D(180.0f, 220.0f), 1);
	SpeakerText = Text(WidgetTree, TEXT("DialogueSpeaker"), 24, EGameXXKFontRole::Title);
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
	ContinueIndicator->SetText(GameXXKLocalization::Source(TEXT("点击 / 空格继续  ▶")));
	ContinueIndicator->SetJustification(ETextJustify::Right);
	Place(Content, ContinueIndicator, FVector2D(1030.0f, 258.0f), FVector2D(330.0f, 34.0f), 1);
	CloseButton=WidgetTree->ConstructWidget<UGameXXKDialogueOptionButton>();CloseButton->Configure(this,-2);
	FButtonStyle CloseStyle; FGameXXKSfx::SetButtonSound(CloseStyle);FSlateBrush NoBox;NoBox.DrawAs=ESlateBrushDrawType::NoDrawType;
	CloseStyle.SetNormal(NoBox);CloseStyle.SetHovered(NoBox);CloseStyle.SetPressed(NoBox);CloseStyle.SetNormalPadding(FMargin(0));CloseStyle.SetPressedPadding(FMargin(0));CloseButton->SetStyle(CloseStyle);
	auto* CloseIcon=WidgetTree->ConstructWidget<UImage>();CloseIcon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk")),false);
	CloseIcon->SetVisibility(ESlateVisibility::HitTestInvisible);CloseButton->SetContent(CloseIcon);CloseButton->SetToolTipText(GameXXKLocalization::Source(TEXT("暂歇对话")));
	Place(Content,CloseButton,FVector2D(1372,8),FVector2D(54,54),3);
	HintButton=WidgetTree->ConstructWidget<UGameXXKDialogueOptionButton>();HintButton->Configure(this,-3);HintButton->SetStyle(CloseStyle);
	auto* HintText=Text(WidgetTree,TEXT("DialogueHintText"),18);HintText->SetText(GameXXKLocalization::Source(TEXT("提示")));HintText->SetJustification(ETextJustify::Center);HintButton->SetContent(HintText);
	Place(Content,HintButton,FVector2D(910,260),FVector2D(100,32),3);
	CloseButton->SetVisibility(ESlateVisibility::Collapsed);HintButton->SetVisibility(ESlateVisibility::Collapsed);
	ClearPresentation();
}
