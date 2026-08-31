#include "UI/GameXXKPrologueYueBaiWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace GameXXKPrologueYueBaiWidgetPrivate
{
	constexpr int32 AtlasColumns = 8;
	constexpr int32 AtlasRows = 8;
	constexpr int32 ValidFrameCount = 60;
	constexpr float LogicalCanvasSize = 512.0f;
	const TCHAR* Texture2K =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_2k_atlas.T_character_09_yue_bai_intro_2k_atlas");
	const TCHAR* Texture1K =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_1k_atlas.T_character_09_yue_bai_intro_1k_atlas");
}

TSharedRef<SWidget> UGameXXKPrologueYueBaiWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

bool UGameXXKPrologueYueBaiWidget::SetAtlasFrame(
	UTexture2D* Texture,
	const int32 FrameIndex)
{
	if (!Texture)
	{
		return false;
	}
	BuildProgrammaticLayout();
	if (!YueBaiImage)
	{
		return false;
	}
	const int32 SafeFrame = FMath::Clamp(
		FrameIndex,
		0,
		GameXXKPrologueYueBaiWidgetPrivate::ValidFrameCount - 1);
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(
		GameXXKPrologueYueBaiWidgetPrivate::LogicalCanvasSize);
	Brush.SetUVRegion(FrameUv(SafeFrame));
	YueBaiImage->SetBrush(Brush);
	PresentedTexture = Texture;
	PresentedFrameIndex = SafeFrame;
	return true;
}

FBox2f UGameXXKPrologueYueBaiWidget::FrameUvForTest(const int32 FrameIndex)
{
	return FrameUv(FrameIndex);
}

FString UGameXXKPrologueYueBaiWidget::GetTexturePathForTest(
	const bool bLowResolution)
{
	return bLowResolution
		? FString(GameXXKPrologueYueBaiWidgetPrivate::Texture1K)
		: FString(GameXXKPrologueYueBaiWidgetPrivate::Texture2K);
}

void UGameXXKPrologueYueBaiWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("PrologueYueBaiWidgetTree"));
	}
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	YueBaiImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("PrologueYueBaiImage"));
	YueBaiImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = YueBaiImage;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FBox2f UGameXXKPrologueYueBaiWidget::FrameUv(const int32 FrameIndex)
{
	const int32 SafeFrame = FMath::Clamp(
		FrameIndex,
		0,
		GameXXKPrologueYueBaiWidgetPrivate::ValidFrameCount - 1);
	const int32 Column = SafeFrame % GameXXKPrologueYueBaiWidgetPrivate::AtlasColumns;
	const int32 Row = SafeFrame / GameXXKPrologueYueBaiWidgetPrivate::AtlasColumns;
	const FVector2f CellSize(
		1.0f / static_cast<float>(GameXXKPrologueYueBaiWidgetPrivate::AtlasColumns),
		1.0f / static_cast<float>(GameXXKPrologueYueBaiWidgetPrivate::AtlasRows));
	const FVector2f Minimum(
		static_cast<float>(Column) * CellSize.X,
		static_cast<float>(Row) * CellSize.Y);
	return FBox2f(Minimum, Minimum + CellSize);
}
