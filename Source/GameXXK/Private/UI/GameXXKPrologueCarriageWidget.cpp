#include "UI/GameXXKPrologueCarriageWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	constexpr int32 AtlasColumns = 8;
	constexpr int32 AtlasRows = 8;
	constexpr int32 ValidFrameCount = 60;
	constexpr float LogicalCanvasSize = 512.0f;
}

TSharedRef<SWidget> UGameXXKPrologueCarriageWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

bool UGameXXKPrologueCarriageWidget::SetAtlasFrame(
	UTexture2D* Texture,
	const int32 FrameIndex)
{
	if (!Texture)
	{
		return false;
	}
	BuildProgrammaticLayout();
	if (!CarriageImage)
	{
		return false;
	}

	const int32 SafeFrameIndex = FMath::Clamp(FrameIndex, 0, ValidFrameCount - 1);
	const FBox2f Uv = FrameUv(SafeFrameIndex);
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(LogicalCanvasSize, LogicalCanvasSize);
	Brush.SetUVRegion(Uv);
	CarriageImage->SetBrush(Brush);
	PresentedTexture = Texture;
	PresentedFrameIndex = SafeFrameIndex;
	PresentedUv = Uv;
	return true;
}

FBox2f UGameXXKPrologueCarriageWidget::FrameUvForTest(
	const int32 FrameIndex)
{
	return FrameUv(FrameIndex);
}

void UGameXXKPrologueCarriageWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PrologueCarriageWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	CarriageImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("PrologueCarriageImage"));
	CarriageImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = CarriageImage;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FBox2f UGameXXKPrologueCarriageWidget::FrameUv(const int32 FrameIndex)
{
	const int32 SafeFrameIndex = FMath::Clamp(FrameIndex, 0, ValidFrameCount - 1);
	const int32 Column = SafeFrameIndex % AtlasColumns;
	const int32 Row = SafeFrameIndex / AtlasColumns;
	const FVector2f CellSize(
		1.0f / static_cast<float>(AtlasColumns),
		1.0f / static_cast<float>(AtlasRows));
	const FVector2f Minimum(
		static_cast<float>(Column) * CellSize.X,
		static_cast<float>(Row) * CellSize.Y);
	return FBox2f(Minimum, Minimum + CellSize);
}
