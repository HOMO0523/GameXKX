#include "UI/GameXXKTutorial01ResultWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace GameXXKTutorial01ResultWidgetPrivate
{
	const TCHAR* PaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");

	UButton* MakeButton(
		UWidgetTree& WidgetTree,
		const FName ButtonName,
		const FName TextName,
		const FText& Text)
	{
		UButton* Button = WidgetTree.ConstructWidget<UButton>(
			UButton::StaticClass(),
			ButtonName);
		Button->SetBackgroundColor(FLinearColor(0.44f, 0.52f, 0.48f, 0.96f));
		UTextBlock* Label = WidgetTree.ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TextName);
		Label->SetText(Text);
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 24;
		Label->SetFont(Font);
		Button->AddChild(Label);
		return Button;
	}
}

TSharedRef<SWidget> UGameXXKTutorial01ResultWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKTutorial01ResultWidget::PresentFailure(const FText& Reason)
{
	BuildProgrammaticLayout();
	if (ReasonText)
	{
		ReasonText->SetText(Reason);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UGameXXKTutorial01ResultWidget::Dismiss()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKTutorial01ResultWidget::SetRetryRequested(
	FGameXXKTutorial01RetryRequested InDelegate)
{
	RetryRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKTutorial01ResultWidget::SetReturnTownRequested(
	FGameXXKTutorial01ReturnTownRequested InDelegate)
{
	ReturnTownRequestedDelegate = MoveTemp(InDelegate);
}

bool UGameXXKTutorial01ResultWidget::IsVisibleForTest() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

FString UGameXXKTutorial01ResultWidget::GetPaperTexturePathForTest() const
{
	return GameXXKTutorial01ResultWidgetPrivate::PaperTexturePath;
}

void UGameXXKTutorial01ResultWidget::ChooseRetryForTest()
{
	HandleRetryClicked();
}

void UGameXXKTutorial01ResultWidget::ChooseReturnTownForTest()
{
	HandleReturnTownClicked();
}

void UGameXXKTutorial01ResultWidget::HandleRetryClicked()
{
	Dismiss();
	if (RetryRequestedDelegate.IsBound())
	{
		RetryRequestedDelegate.Execute();
	}
}

void UGameXXKTutorial01ResultWidget::HandleReturnTownClicked()
{
	Dismiss();
	if (ReturnTownRequestedDelegate.IsBound())
	{
		ReturnTownRequestedDelegate.Execute();
	}
}

void UGameXXKTutorial01ResultWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKTutorial01ResultWidgetPrivate;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("Tutorial01ResultRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Tutorial01ResultDim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
	if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("Tutorial01ResultPanelSize"));
	PanelSize->SetWidthOverride(680.0f);
	PanelSize->SetHeightOverride(360.0f);
	PaperPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("Tutorial01ResultPaper"));
	PaperPanel->SetPadding(FMargin(58.0f, 52.0f, 58.0f, 42.0f));
	if (UTexture2D* PaperTexture = LoadObject<UTexture2D>(nullptr, PaperTexturePath))
	{
		FSlateBrush PaperBrush;
		PaperBrush.SetResourceObject(PaperTexture);
		PaperBrush.DrawAs = ESlateBrushDrawType::Box;
		PaperBrush.Margin = FMargin(0.07f);
		PaperBrush.ImageSize = FVector2D(100.0f, 101.0f);
		PaperBrush.TintColor = FSlateColor(FLinearColor::White);
		PaperPanel->SetBrush(PaperBrush);
	}
	PaperPanel->SetBrushColor(FLinearColor::White);
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("Tutorial01ResultBody"));
	PaperPanel->SetContent(Body);
	ReasonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("Tutorial01ResultReason"));
	ReasonText->SetAutoWrapText(true);
	ReasonText->SetJustification(ETextJustify::Center);
	ReasonText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.18f, 0.11f, 0.06f, 1.0f)));
	FSlateFontInfo ReasonFont = ReasonText->GetFont();
	ReasonFont.Size = 28;
	ReasonFont.TypefaceFontName = TEXT("Bold");
	ReasonText->SetFont(ReasonFont);
	if (UVerticalBoxSlot* ReasonSlot = Body->AddChildToVerticalBox(ReasonText))
	{
		ReasonSlot->SetHorizontalAlignment(HAlign_Fill);
		ReasonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 58.0f));
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("Tutorial01ResultButtons"));
	RetryButton = MakeButton(
		*WidgetTree,
		TEXT("Tutorial01RetryButton"),
		TEXT("Tutorial01RetryText"),
		FText::FromString(TEXT("重新挑战")));
	ReturnTownButton = MakeButton(
		*WidgetTree,
		TEXT("Tutorial01ReturnTownButton"),
		TEXT("Tutorial01ReturnTownText"),
		FText::FromString(TEXT("返回城镇")));
	RetryButton->OnClicked.AddDynamic(this, &UGameXXKTutorial01ResultWidget::HandleRetryClicked);
	ReturnTownButton->OnClicked.AddDynamic(this, &UGameXXKTutorial01ResultWidget::HandleReturnTownClicked);
	if (UHorizontalBoxSlot* RetrySlot = Buttons->AddChildToHorizontalBox(RetryButton))
	{
		RetrySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RetrySlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}
	if (UHorizontalBoxSlot* ReturnSlot = Buttons->AddChildToHorizontalBox(ReturnTownButton))
	{
		ReturnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ReturnSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
	}
	Body->AddChildToVerticalBox(Buttons);
	PanelSize->AddChild(PaperPanel);
	if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}
	Dismiss();
}
