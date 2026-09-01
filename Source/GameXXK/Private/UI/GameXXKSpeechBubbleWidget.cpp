#include "UI/GameXXKSpeechBubbleWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"

namespace GameXXKSpeechBubblePrivate
{
	const FVector2D BubbleSize(320.0f, 100.0f);
	constexpr float ViewportPadding = 12.0f;
	constexpr float AnchorVerticalGap = 24.0f;
	constexpr int32 MaximumLineCount = 2;
	constexpr const TCHAR* BubblePaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");

	FSlateBrush BubbleBrush()
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, BubblePaperTexturePath))
		{
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = BubbleSize;
			Brush.Margin = FMargin(0.065f);
		}
		return Brush;
	}
}

TSharedRef<SWidget> UGameXXKSpeechBubbleWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

bool UGameXXKSpeechBubbleWidget::PresentBubble(
	const FGameXXKDialoguePresentationView& View,
	USceneComponent* Anchor)
{
	return PresentBubbleInternal(View, Anchor, false);
}

bool UGameXXKSpeechBubbleWidget::PresentBubbleAtVisualTop(
	const FGameXXKDialoguePresentationView& View,
	UPrimitiveComponent* VisualAnchor)
{
	return PresentBubbleInternal(View, VisualAnchor, true);
}

bool UGameXXKSpeechBubbleWidget::PresentBubbleInternal(
	const FGameXXKDialoguePresentationView& View,
	USceneComponent* Anchor,
	const bool bInUseVisualBoundsTop)
{
	BuildProgrammaticLayout();
	if (!IsValid(Anchor) || View.NodeId.IsNone() || View.Text.IsEmpty())
	{
		LastPresentationError = TEXT("Speech bubble requires a live anchor, node ID and text.");
		ClearBubble();
		return false;
	}
	AnchorComponent = Anchor;
	bUseVisualBoundsTop = bInUseVisualBoundsTop;
	CurrentView = View;
	if (BodyText)
	{
		BodyText->SetText(View.Text);
	}
	bBubbleVisible = true;
	LastPresentationError.Reset();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool UGameXXKSpeechBubbleWidget::UpdateAnchor(APlayerController* Controller)
{
	USceneComponent* Anchor = AnchorComponent.Get();
	if (!IsValid(Controller) || !IsValid(Anchor) || !BubbleFrame)
	{
		LastPresentationError = TEXT("Speech bubble anchor projection is unavailable.");
		return false;
	}
	FVector AnchorWorldLocation = Anchor->GetComponentLocation();
	if (bUseVisualBoundsTop)
	{
		const UPrimitiveComponent* PrimitiveAnchor = Cast<UPrimitiveComponent>(Anchor);
		if (!PrimitiveAnchor)
		{
			LastPresentationError = TEXT("Speech bubble visual-top anchor is not renderable.");
			return false;
		}
		AnchorWorldLocation = VisualBoundsTopForTest(
			PrimitiveAnchor->Bounds.Origin,
			PrimitiveAnchor->Bounds.BoxExtent);
	}
	FVector2D Projected;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			Controller,
			AnchorWorldLocation,
			Projected,
			true))
	{
		LastPresentationError = TEXT("Speech bubble anchor is outside the active projection.");
		return false;
	}
	FVector2D ViewportSize =
		UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(Controller).GetLocalSize();
	if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		Controller->GetViewportSize(ViewportWidth, ViewportHeight);
		const float ViewportScale = FMath::Max(
			KINDA_SMALL_NUMBER,
			UWidgetLayoutLibrary::GetViewportScale(Controller));
		ViewportSize = FVector2D(ViewportWidth, ViewportHeight) / ViewportScale;
		if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
		{
			LastPresentationError = TEXT("Speech bubble viewport has no valid size.");
			return false;
		}
	}
	Projected.X -= GameXXKSpeechBubblePrivate::BubbleSize.X * 0.5f;
	Projected.Y -= GameXXKSpeechBubblePrivate::BubbleSize.Y + GameXXKSpeechBubblePrivate::AnchorVerticalGap;
	const FVector2D Position = ClampToViewport(
		Projected,
		ViewportSize,
		GameXXKSpeechBubblePrivate::BubbleSize);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BubbleFrame->Slot))
	{
		CanvasSlot->SetPosition(Position);
	}
	LastPresentationError.Reset();
	return true;
}

void UGameXXKSpeechBubbleWidget::ClearBubble()
{
	AnchorComponent.Reset();
	CurrentView = FGameXXKDialoguePresentationView();
	bUseVisualBoundsTop = false;
	bBubbleVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

FVector2D UGameXXKSpeechBubbleWidget::ClampToViewportForTest(
	const FVector2D ProjectedPosition,
	const FVector2D ViewportSize,
	const FVector2D InBubbleSize)
{
	return ClampToViewport(ProjectedPosition, ViewportSize, InBubbleSize);
}

FVector UGameXXKSpeechBubbleWidget::VisualBoundsTopForTest(
	const FVector BoundsOrigin,
	const FVector BoundsExtent)
{
	return BoundsOrigin + FVector(0.0f, 0.0f, FMath::Max(0.0f, BoundsExtent.Z));
}

bool UGameXXKSpeechBubbleWidget::IsBubbleVisibleForTest() const { return bBubbleVisible; }
bool UGameXXKSpeechBubbleWidget::IsBubbleHitTestInvisibleForTest() const
{
	return GetVisibility() == ESlateVisibility::HitTestInvisible;
}
int32 UGameXXKSpeechBubbleWidget::GetBubbleCountForTest() const { return BubbleFrame ? 1 : 0; }
FText UGameXXKSpeechBubbleWidget::GetBodyTextForTest() const { return BodyText ? BodyText->GetText() : FText::GetEmpty(); }
int32 UGameXXKSpeechBubbleWidget::GetMaximumLineCountForTest() const { return GameXXKSpeechBubblePrivate::MaximumLineCount; }

FVector2D UGameXXKSpeechBubbleWidget::ClampToViewport(
	const FVector2D ProjectedPosition,
	const FVector2D ViewportSize,
	const FVector2D InBubbleSize)
{
	const float MaximumX = FMath::Max(
		GameXXKSpeechBubblePrivate::ViewportPadding,
		ViewportSize.X - InBubbleSize.X - GameXXKSpeechBubblePrivate::ViewportPadding);
	const float MaximumY = FMath::Max(
		GameXXKSpeechBubblePrivate::ViewportPadding,
		ViewportSize.Y - InBubbleSize.Y - GameXXKSpeechBubblePrivate::ViewportPadding);
	return FVector2D(
		FMath::Clamp(ProjectedPosition.X, GameXXKSpeechBubblePrivate::ViewportPadding, MaximumX),
		FMath::Clamp(ProjectedPosition.Y, GameXXKSpeechBubblePrivate::ViewportPadding, MaximumY));
}

void UGameXXKSpeechBubbleWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("SpeechBubbleWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpeechBubbleRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;
	BubbleFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SpeechBubbleFrame"));
	BubbleFrame->SetBrush(GameXXKSpeechBubblePrivate::BubbleBrush());
	BubbleFrame->SetPadding(FMargin(22.0f, 14.0f));
	BubbleFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(BubbleFrame))
	{
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
		CanvasSlot->SetSize(GameXXKSpeechBubblePrivate::BubbleSize);
	}
	BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeechBubbleBody"));
	BodyText->SetAutoWrapText(true);
	BodyText->SetWrapTextAt(276.0f);
	BodyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.085f, 0.045f, 1.0f)));
	BodyText->SetJustification(ETextJustify::Center);
	FSlateFontInfo Font = BodyText->GetFont();
	Font.Size = 18;
	BodyText->SetFont(Font);
	BubbleFrame->SetContent(BodyText);
	ClearBubble();
}
