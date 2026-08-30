#include "UI/GameXXKProloguePauseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FName Name,
		const TCHAR* Text,
		const int32 FontSize)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			Name);
		Result->SetText(FText::FromString(Text));
		Result->SetJustification(ETextJustify::Center);
		Result->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = FontSize;
		Result->SetFont(Font);
		return Result;
	}

	UButton* AddButton(
		UWidgetTree* Tree,
		UVerticalBox* Column,
		const FName Name,
		const TCHAR* Label)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetContent(MakeText(Tree, *FString::Printf(TEXT("%sLabel"), *Name.ToString()), Label, 20));
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Button))
		{
			Slot->SetPadding(FMargin(12.0f, 8.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Button;
	}
}

TSharedRef<SWidget> UGameXXKProloguePauseWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKProloguePauseWidget::SetResumeRequested(
	FGameXXKPrologueResumeRequested InDelegate)
{
	ResumeRequested = MoveTemp(InDelegate);
}

void UGameXXKProloguePauseWidget::SetReturnDesktopRequested(
	FGameXXKPrologueReturnDesktopRequested InDelegate)
{
	ReturnDesktopRequested = MoveTemp(InDelegate);
}

FText UGameXXKProloguePauseWidget::GetTitleTextForTest() const
{
	return TitleText ? TitleText->GetText() : FText::GetEmpty();
}

int32 UGameXXKProloguePauseWidget::GetButtonCountForTest() const
{
	return (ResumeButton ? 1 : 0) + (ReturnDesktopButton ? 1 : 0);
}

void UGameXXKProloguePauseWidget::RequestResumeForTest()
{
	HandleResumeClicked();
}

void UGameXXKProloguePauseWidget::RequestReturnDesktopForTest()
{
	HandleReturnDesktopClicked();
}

void UGameXXKProloguePauseWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("ProloguePauseWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		TEXT("ProloguePauseRoot"));
	WidgetTree->RootWidget = RootOverlay;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("ProloguePauseDim"));
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	RootOverlay->AddChildToOverlay(Dim);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("ProloguePauseColumn"));
	if (UOverlaySlot* OverlaySlot = RootOverlay->AddChildToOverlay(Column))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
		OverlaySlot->SetPadding(FMargin(80.0f));
	}
	TitleText = MakeText(
		WidgetTree,
		TEXT("ProloguePauseTitle"),
		TEXT("剧情已暂停"),
		28);
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetPadding(FMargin(12.0f, 16.0f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}
	ResumeButton = AddButton(
		WidgetTree,
		Column,
		TEXT("ProloguePauseResume"),
		TEXT("继续"));
	ReturnDesktopButton = AddButton(
		WidgetTree,
		Column,
		TEXT("ProloguePauseReturnDesktop"),
		TEXT("返回桌面"));
	ResumeButton->OnClicked.AddDynamic(this, &UGameXXKProloguePauseWidget::HandleResumeClicked);
	ReturnDesktopButton->OnClicked.AddDynamic(
		this,
		&UGameXXKProloguePauseWidget::HandleReturnDesktopClicked);
	SetIsFocusable(true);
}

void UGameXXKProloguePauseWidget::HandleResumeClicked()
{
	if (ResumeRequested.IsBound())
	{
		ResumeRequested.Execute();
	}
}

void UGameXXKProloguePauseWidget::HandleReturnDesktopClicked()
{
	if (ReturnDesktopRequested.IsBound())
	{
		ReturnDesktopRequested.Execute();
	}
}
