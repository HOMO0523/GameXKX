#include "UI/GameXXKGuidePreferenceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace GameXXKGuidePreferenceWidgetPrivate
{
	const FText ExperiencedCopy = NSLOCTEXT("GameXXKGuide", "ExperiencedChoice", "我是老玩家，跳过");
	const FText NewPlayerCopy = NSLOCTEXT("GameXXKGuide", "NewPlayerChoice", "我是新手，继续");

	void Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas ? Canvas->AddChildToCanvas(Widget) : nullptr)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	UTextBlock* ButtonText(UWidgetTree* Tree, const FName Name, const FText& Text)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(Text);
		Result->SetJustification(ETextJustify::Center);
		Result->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = 20;
		Result->SetFont(Font);
		return Result;
	}
}

TSharedRef<SWidget> UGameXXKGuidePreferenceWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKGuidePreferenceWidget::RefreshFromProgress(const FGameXXKGuideProgress& Progress)
{
	BuildProgrammaticLayout();
	bPromptVisible = Progress.Preference == EGameXXKGuidePreference::Unset;
	SetVisibility(bPromptVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UGameXXKGuidePreferenceWidget::SetPreferenceChosenDelegate(FGameXXKGuidePreferenceChosen InDelegate)
{
	PreferenceChosenDelegate = MoveTemp(InDelegate);
}

void UGameXXKGuidePreferenceWidget::ChooseExperiencedPlayerForTest()
{
	HandleExperiencedClicked();
}

void UGameXXKGuidePreferenceWidget::ChooseNewPlayerForTest()
{
	HandleNewPlayerClicked();
}

bool UGameXXKGuidePreferenceWidget::IsPromptVisibleForTest() const
{
	return bPromptVisible;
}

FText UGameXXKGuidePreferenceWidget::GetExperiencedButtonTextForTest() const
{
	return ExperiencedText ? ExperiencedText->GetText() : GameXXKGuidePreferenceWidgetPrivate::ExperiencedCopy;
}

FText UGameXXKGuidePreferenceWidget::GetNewPlayerButtonTextForTest() const
{
	return NewPlayerText ? NewPlayerText->GetText() : GameXXKGuidePreferenceWidgetPrivate::NewPlayerCopy;
}

void UGameXXKGuidePreferenceWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKGuidePreferenceWidgetPrivate;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GuidePreferenceRoot"));
	WidgetTree->RootWidget = RootCanvas;
	PromptPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GuidePreferencePanel"));
	PromptPanel->SetBrushColor(FLinearColor(0.88f, 0.82f, 0.70f, 0.98f));
	Place(RootCanvas, PromptPanel, FVector2D(590.0f, 360.0f), FVector2D(740.0f, 260.0f), 0);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GuidePreferenceContent"));
	PromptPanel->SetContent(Content);
	UTextBlock* Title = ButtonText(
		WidgetTree,
		TEXT("GuidePreferenceTitle"),
		NSLOCTEXT("GameXXKGuide", "PreferenceQuestion", "是否开启战斗引导？"));
	Place(Content, Title, FVector2D(40.0f, 30.0f), FVector2D(660.0f, 52.0f), 1);

	ExperiencedButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("GuideExperiencedButton"));
	ExperiencedText = ButtonText(WidgetTree, TEXT("GuideExperiencedText"), ExperiencedCopy);
	ExperiencedButton->SetContent(ExperiencedText);
	ExperiencedButton->OnClicked.AddDynamic(this, &UGameXXKGuidePreferenceWidget::HandleExperiencedClicked);
	Place(Content, ExperiencedButton, FVector2D(50.0f, 125.0f), FVector2D(290.0f, 72.0f), 1);

	NewPlayerButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("GuideNewPlayerButton"));
	NewPlayerText = ButtonText(WidgetTree, TEXT("GuideNewPlayerText"), NewPlayerCopy);
	NewPlayerButton->SetContent(NewPlayerText);
	NewPlayerButton->OnClicked.AddDynamic(this, &UGameXXKGuidePreferenceWidget::HandleNewPlayerClicked);
	Place(Content, NewPlayerButton, FVector2D(400.0f, 125.0f), FVector2D(290.0f, 72.0f), 1);
}

void UGameXXKGuidePreferenceWidget::HandleExperiencedClicked()
{
	if (PreferenceChosenDelegate.IsBound())
	{
		PreferenceChosenDelegate.Execute(EGameXXKGuidePreference::ExperiencedPlayer);
	}
}

void UGameXXKGuidePreferenceWidget::HandleNewPlayerClicked()
{
	if (PreferenceChosenDelegate.IsBound())
	{
		PreferenceChosenDelegate.Execute(EGameXXKGuidePreference::NewPlayer);
	}
}
