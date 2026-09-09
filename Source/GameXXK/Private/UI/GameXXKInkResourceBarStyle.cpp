#include "UI/GameXXKInkResourceBarStyle.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UMaterialInstanceDynamic* GameXXKInkResourceBarStyle::Create(UObject* Outer, const FVector2D& Size, const bool bMana)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	UMaterialInstanceDynamic* Material = Parent ? UMaterialInstanceDynamic::Create(Parent, Outer) : nullptr;
	if (Material)
	{
		Material->SetScalarParameterValue(TEXT("BarAspect"), Size.X / FMath::Max(1.0, Size.Y));
		Material->SetVectorParameterValue(TEXT("FillColor"), FLinearColor::FromSRGBColor(bMana ? FColor(112,165,139) : FColor(196,81,53)));
	}
	return Material;
}

FSlateBrush GameXXKInkResourceBarStyle::Brush(UMaterialInstanceDynamic* Material, const FVector2D& Size)
{
	FSlateBrush Result;
	Result.DrawAs = ESlateBrushDrawType::Image;
	Result.ImageSize = Size;
	Result.SetResourceObject(Material);
	return Result;
}

void GameXXKInkResourceBarStyle::Update(UMaterialInstanceDynamic* Material, const float Percent)
{
	if (Material) Material->SetScalarParameterValue(TEXT("FillPercent"), FMath::Clamp(Percent, 0.0f, 1.0f));
}

void GameXXKInkResourceBarStyle::Apply(UProgressBar* Bar, const FVector2D& Size, const bool bMana)
{
	if (!Bar) return;
	UMaterialInstanceDynamic* Material = Create(Bar, Size, bMana);
	FProgressBarStyle Style;
	Style.SetBackgroundImage(Brush(Material, Size));
	// The material preserves the rail's endcaps; the native fill must not scale it again.
	FSlateBrush NoDraw;
	NoDraw.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.SetFillImage(NoDraw);
	Style.SetMarqueeImage(NoDraw);
	Bar->SetWidgetStyle(Style);
	Bar->SetFillColorAndOpacity(FLinearColor::White);
	Update(Material, Bar->GetPercent());
}

void GameXXKInkResourceBarStyle::Update(UProgressBar* Bar, const float Percent)
{
	if (!Bar) return;
	Bar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	Update(Cast<UMaterialInstanceDynamic>(Bar->GetWidgetStyle().BackgroundImage.GetResourceObject()), Percent);
}
