#include "UI/GameXXKPrologueMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace GameXXKPrologueMapWidgetPrivate
{
	const FVector2D ReferenceViewport(1920.0f, 1080.0f);
	const FVector2D ThumbnailCardSize(420.0f, 470.0f);
	const FVector2D ThumbnailImageSize(320.0f, 320.0f);
	constexpr float InspectionMaximumHeight = 860.0f;
	constexpr float InspectionViewportHeightRatio = 0.80f;
	constexpr float InspectionAspect = 1279.0f / 1706.0f;
	const TCHAR* TaskIconPath =
		TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap.T_Relic_OldMap");
	const TCHAR* InspectionTexturePath =
		TEXT("/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.T_Tutorial_XuXiakeTravelRouteInspect");
	const TCHAR* PaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");

	void Place(
		UCanvasPanel* Canvas,
		UWidget* Widget,
		const FVector2D Position,
		const FVector2D Size,
		const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas && Widget
			? Canvas->AddChildToCanvas(Widget)
			: nullptr)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	FSlateBrush TextureBrush(const TCHAR* Path, const FVector2D Size, const bool bBox)
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path))
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = Size;
			Brush.DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
			if (bBox)
			{
				Brush.Margin = FMargin(0.08f);
			}
		}
		return Brush;
	}

	UTextBlock* MakeButtonText(UWidgetTree* Tree, const TCHAR* Text)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Text));
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 20;
		Label->SetFont(Font);
		return Label;
	}
}

TSharedRef<SWidget> UGameXXKPrologueMapWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	RefreshPresentation();
	return Super::RebuildWidget();
}

void UGameXXKPrologueMapWidget::Configure(const EGameXXKPrologueMapMode InMode)
{
	Mode = InMode;
	bInspectionOpen = Mode == EGameXXKPrologueMapMode::InspectOnly;
	BuildProgrammaticLayout();
	RefreshPresentation();
}

bool UGameXXKPrologueMapWidget::RequestInspection()
{
	if (bInspectionOpen)
	{
		return false;
	}
	bInspectionOpen = true;
	RefreshPresentation();
	if (InspectRequested.IsBound())
	{
		InspectRequested.Execute();
	}
	return true;
}

bool UGameXXKPrologueMapWidget::RequestCloseInspection()
{
	if (!bInspectionOpen)
	{
		return false;
	}
	bInspectionOpen = false;
	RefreshPresentation();
	if (CloseRequested.IsBound())
	{
		CloseRequested.Execute();
	}
	return true;
}

bool UGameXXKPrologueMapWidget::RequestContinue()
{
	if (Mode != EGameXXKPrologueMapMode::StoryCard || bInspectionOpen)
	{
		return false;
	}
	if (ContinueRequested.IsBound())
	{
		ContinueRequested.Execute();
	}
	return true;
}

FVector2D UGameXXKPrologueMapWidget::FitInspectionImageForTest(
	const FVector2D ViewportSize)
{
	const float Height = FMath::Max(
		1.0f,
		FMath::Min(
			GameXXKPrologueMapWidgetPrivate::InspectionMaximumHeight,
			ViewportSize.Y * GameXXKPrologueMapWidgetPrivate::InspectionViewportHeightRatio));
	return FVector2D(Height * GameXXKPrologueMapWidgetPrivate::InspectionAspect, Height);
}

bool UGameXXKPrologueMapWidget::IsThumbnailVisibleForTest() const
{
	return ThumbnailCard
		&& ThumbnailCard->GetVisibility() == ESlateVisibility::Visible;
}

bool UGameXXKPrologueMapWidget::HasContinuePromptForTest() const
{
	return ContinuePrompt
		&& ContinuePrompt->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

FString UGameXXKPrologueMapWidget::GetTaskIconPathForTest() const
{
	return GameXXKPrologueMapWidgetPrivate::TaskIconPath;
}

FString UGameXXKPrologueMapWidget::GetInspectionTexturePathForTest() const
{
	return GameXXKPrologueMapWidgetPrivate::InspectionTexturePath;
}

void UGameXXKPrologueMapWidget::HandleInspectClicked()
{
	RequestInspection();
}

void UGameXXKPrologueMapWidget::HandleCloseClicked()
{
	RequestCloseInspection();
}

void UGameXXKPrologueMapWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKPrologueMapWidgetPrivate;
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PrologueMapWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PrologueMapRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	DimMask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PrologueMapDim"));
	DimMask->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.58f));
	DimMask->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(RootCanvas, DimMask, FVector2D::ZeroVector, ReferenceViewport, 0);

	ThumbnailCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PrologueMapThumbnailCard"));
	ThumbnailCard->SetBrush(TextureBrush(PaperTexturePath, ThumbnailCardSize, true));
	ThumbnailCard->SetPadding(FMargin(42.0f, 34.0f));
	Place(
		RootCanvas,
		ThumbnailCard,
		(ReferenceViewport - ThumbnailCardSize) * 0.5f,
		ThumbnailCardSize,
		10);

	UCanvasPanel* ThumbnailContent = WidgetTree->ConstructWidget<UCanvasPanel>();
	ThumbnailCard->SetContent(ThumbnailContent);
	ThumbnailImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PrologueMapThumbnail"));
	ThumbnailImage->SetBrush(TextureBrush(InspectionTexturePath, ThumbnailImageSize, false));
	ThumbnailImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(ThumbnailContent, ThumbnailImage, FVector2D(8.0f, 0.0f), ThumbnailImageSize, 0);

	InspectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PrologueMapInspectButton"));
	InspectButton->AddChild(MakeButtonText(WidgetTree, TEXT("检视")));
	InspectButton->OnClicked.AddDynamic(this, &UGameXXKPrologueMapWidget::HandleInspectClicked);
	Place(ThumbnailContent, InspectButton, FVector2D(78.0f, 338.0f), FVector2D(180.0f, 48.0f), 1);

	ContinuePrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PrologueMapContinuePrompt"));
	ContinuePrompt->SetText(FText::FromString(TEXT("空格继续")));
	ContinuePrompt->SetJustification(ETextJustify::Center);
	ContinuePrompt->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
	ContinuePrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
	FSlateFontInfo PromptFont = ContinuePrompt->GetFont();
	PromptFont.Size = 17;
	ContinuePrompt->SetFont(PromptFont);
	Place(ThumbnailContent, ContinuePrompt, FVector2D(78.0f, 393.0f), FVector2D(180.0f, 30.0f), 2);

	const FVector2D InspectionSize = FitInspectionImageForTest(ReferenceViewport);
	InspectionPaper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PrologueMapInspectionPaper"));
	InspectionPaper->SetBrush(TextureBrush(PaperTexturePath, InspectionSize + FVector2D(42.0f), true));
	InspectionPaper->SetPadding(FMargin(21.0f));
	Place(
		RootCanvas,
		InspectionPaper,
		(ReferenceViewport - InspectionSize) * 0.5f,
		InspectionSize,
		20);
	InspectionImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PrologueMapInspectionImage"));
	InspectionImage->SetBrush(TextureBrush(InspectionTexturePath, InspectionSize - FVector2D(42.0f), false));
	InspectionImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	InspectionPaper->SetContent(InspectionImage);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PrologueMapCloseButton"));
	CloseButton->AddChild(MakeButtonText(WidgetTree, TEXT("关闭")));
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKPrologueMapWidget::HandleCloseClicked);
	const FVector2D InspectionPosition = (ReferenceViewport - InspectionSize) * 0.5f;
	Place(
		RootCanvas,
		CloseButton,
		FVector2D(InspectionPosition.X + InspectionSize.X + 8.0f, InspectionPosition.Y),
		FVector2D(92.0f, 42.0f),
		21);
}

void UGameXXKPrologueMapWidget::RefreshPresentation()
{
	const bool bShowThumbnail = Mode == EGameXXKPrologueMapMode::StoryCard
		&& !bInspectionOpen;
	if (ThumbnailCard)
	{
		ThumbnailCard->SetVisibility(bShowThumbnail
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (ContinuePrompt)
	{
		ContinuePrompt->SetVisibility(bShowThumbnail
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (InspectionPaper)
	{
		InspectionPaper->SetVisibility(bInspectionOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(bInspectionOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
