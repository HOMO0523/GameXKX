#include "UI/GameXXKQuestDialogWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"

namespace
{
	const FVector2D QuestDialogSize(920.0f, 440.0f);
	const FVector2D QuestButtonSize(184.0f, 52.0f);
	const FString QuestDialogTextureRoot(TEXT("/Game/GameXXK/UI/QuestDialog/Textures/"));
	const FString QuestDialogFrameTexturePath(QuestDialogTextureRoot + TEXT("T_QuestDialogFrame.T_QuestDialogFrame"));
	const FString QuestDialogAcceptTexturePath(QuestDialogTextureRoot + TEXT("T_QuestDialogAccept.T_QuestDialogAccept"));
	const FString QuestDialogLeaveTexturePath(QuestDialogTextureRoot + TEXT("T_QuestDialogLeave.T_QuestDialogLeave"));

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const FVector2D& ImageSize = FVector2D::ZeroVector)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FSlateBrush MakeTextureOrSolidBrush(const FString& TexturePath, const FLinearColor& FallbackColor, const FVector2D& ImageSize)
	{
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = ImageSize;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			return Brush;
		}

		return MakeSolidBrush(FallbackColor, ImageSize);
	}

	FButtonStyle MakeInkButtonStyle(const FString& TexturePath, const FLinearColor& NormalColor)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureOrSolidBrush(TexturePath, NormalColor, QuestButtonSize));
		Style.SetHovered(MakeTextureOrSolidBrush(TexturePath, NormalColor * FLinearColor(1.10f, 1.10f, 1.10f, 1.0f), QuestButtonSize));
		Style.SetPressed(MakeTextureOrSolidBrush(TexturePath, NormalColor * FLinearColor(0.78f, 0.78f, 0.78f, 1.0f), QuestButtonSize));
		return Style;
	}

	UTextBlock* MakeDialogText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize, const FLinearColor& Color, const ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(Justification);
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	void AddCanvasChild(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FAnchors& Anchors = FAnchors(0.0f, 0.0f), const FVector2D& Alignment = FVector2D::ZeroVector)
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
}

TSharedRef<SWidget> UGameXXKQuestDialogWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKQuestDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
}

void UGameXXKQuestDialogWidget::OpenDialog()
{
	SetVisibility(ESlateVisibility::Visible);
}

bool UGameXXKQuestDialogWidget::CloseDialog()
{
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}

	SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

bool UGameXXKQuestDialogWidget::IsDialogOpen() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

FString UGameXXKQuestDialogWidget::GetDialogFrameResourcePathForTest() const
{
	return QuestDialogFrameTexturePath;
}

FString UGameXXKQuestDialogWidget::GetAcceptButtonResourcePathForTest() const
{
	return QuestDialogAcceptTexturePath;
}

FString UGameXXKQuestDialogWidget::GetLeaveButtonResourcePathForTest() const
{
	return QuestDialogLeaveTexturePath;
}

bool UGameXXKQuestDialogWidget::AcceptQuest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bAccepted = Subsystem && Subsystem->AcceptQuest();
	if (bAccepted)
	{
		OnQuestAccepted();
	}
	return bAccepted;
}

void UGameXXKQuestDialogWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("QuestDialogWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestDialogRoot"));
	WidgetTree->RootWidget = RootCanvas;

	ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestDialogBackdrop"));
	ModalBackdrop->SetBrushColor(FLinearColor(0.015f, 0.020f, 0.028f, 0.48f));
	AddCanvasChild(RootCanvas, ModalBackdrop, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	DialogFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestDialogFrame"));
	DialogFrame->SetBrush(MakeTextureOrSolidBrush(QuestDialogFrameTexturePath, FLinearColor(0.93f, 0.88f, 0.74f, 0.98f), QuestDialogSize));
	DialogFrame->SetBrushColor(FLinearColor::White);
	DialogFrame->SetPadding(FMargin(50.0f, 42.0f, 50.0f, 38.0f));
	AddCanvasChild(RootCanvas, DialogFrame, FVector2D::ZeroVector, QuestDialogSize, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuestDialogContent"));
	DialogFrame->AddChild(ContentBox);

	SpeakerTextBlock = MakeDialogText(WidgetTree, NSLOCTEXT("GameXXKQuestDialog", "Speaker", "青山镇 · 引路人"), 28, FLinearColor(0.10f, 0.16f, 0.15f, 1.0f));
	if (UVerticalBoxSlot* SpeakerSlot = ContentBox->AddChildToVerticalBox(SpeakerTextBlock))
	{
		SpeakerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UBorder* InkDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuestDialogInkDivider"));
	InkDivider->SetBrushColor(FLinearColor(0.12f, 0.18f, 0.17f, 0.66f));
	if (UVerticalBoxSlot* DividerSlot = ContentBox->AddChildToVerticalBox(InkDivider))
	{
		DividerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		DividerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		DividerSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	DialogTextBlock = MakeDialogText(WidgetTree, NSLOCTEXT("GameXXKQuestDialog", "Prompt", "少侠，山道上近来有妖物出没。可愿随我前去查探？"), 22, FLinearColor(0.11f, 0.09f, 0.07f, 1.0f));
	if (UVerticalBoxSlot* DialogSlot = ContentBox->AddChildToVerticalBox(DialogTextBlock))
	{
		DialogSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		DialogSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 28.0f));
	}

	UHorizontalBox* ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QuestDialogButtonRow"));
	if (UVerticalBoxSlot* ButtonBoxSlot = ContentBox->AddChildToVerticalBox(ButtonBox))
	{
		ButtonBoxSlot->SetHorizontalAlignment(HAlign_Right);
	}

	LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuestDialogLeaveButton"));
	LeaveButton->SetStyle(MakeInkButtonStyle(QuestDialogLeaveTexturePath, FLinearColor(0.45f, 0.38f, 0.27f, 1.0f)));
	LeaveButton->SetBackgroundColor(FLinearColor::White);
	LeaveButton->OnClicked.AddDynamic(this, &UGameXXKQuestDialogWidget::HandleLeaveClicked);
	UTextBlock* LeaveText = MakeDialogText(WidgetTree, NSLOCTEXT("GameXXKQuestDialog", "Leave", "暂且离开"), 18, FLinearColor::White, ETextJustify::Center);
	LeaveButton->AddChild(LeaveText);
	if (UHorizontalBoxSlot* LeaveSlot = ButtonBox->AddChildToHorizontalBox(LeaveButton))
	{
		LeaveSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));
		LeaveSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	AcceptButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuestDialogAcceptButton"));
	AcceptButton->SetStyle(MakeInkButtonStyle(QuestDialogAcceptTexturePath, FLinearColor(0.10f, 0.34f, 0.28f, 1.0f)));
	AcceptButton->SetBackgroundColor(FLinearColor::White);
	AcceptButton->OnClicked.AddDynamic(this, &UGameXXKQuestDialogWidget::HandleAcceptClicked);
	UTextBlock* AcceptText = MakeDialogText(WidgetTree, NSLOCTEXT("GameXXKQuestDialog", "Accept", "接取委托"), 18, FLinearColor::White, ETextJustify::Center);
	AcceptButton->AddChild(AcceptText);
	ButtonBox->AddChildToHorizontalBox(AcceptButton);

	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKQuestDialogWidget::HandleAcceptClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->AcceptQuestDialog();
		return;
	}

	if (AcceptQuest())
	{
		CloseDialog();
	}
}

void UGameXXKQuestDialogWidget::HandleLeaveClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->CloseQuestDialog();
		return;
	}

	CloseDialog();
}
