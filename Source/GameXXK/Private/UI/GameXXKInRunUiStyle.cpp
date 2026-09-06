#include "UI/GameXXKInRunUiStyle.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateBrush Texture(const TCHAR* Path, const FVector2D& Size, bool bBox, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, Path));
		Brush.ImageSize = Size;
		Brush.DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
		Brush.Margin = FMargin(0.045f);
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}
}

FLinearColor FGameXXKInRunUiStyle::Ink() { return FLinearColor::FromSRGBColor(FColor(47, 41, 33)); }
FLinearColor FGameXXKInRunUiStyle::MutedInk() { return FLinearColor::FromSRGBColor(FColor(105, 92, 75)); }
FLinearColor FGameXXKInRunUiStyle::Vermilion() { return FLinearColor::FromSRGBColor(FColor(148, 63, 46)); }
FLinearColor FGameXXKInRunUiStyle::Jade() { return FLinearColor::FromSRGBColor(FColor(53, 87, 78)); }

FSlateFontInfo FGameXXKInRunUiStyle::Font(const int32 Size, const bool bDisplay, const bool bBold)
{
	const TCHAR* Path = bDisplay
		? TEXT("/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font.FF_Trial_ZhHans_JiangHuGuFeng_Font")
		: TEXT("/Game/GameXXK/UI/Fonts/Readability/F_ReadableCJK.F_ReadableCJK");
	if (UFont* FontAsset = LoadObject<UFont>(nullptr, Path, nullptr, LOAD_NoWarn))
	{
		return FSlateFontInfo(FontAsset, Size, bDisplay ? FName(TEXT("Default")) : FName(bBold ? TEXT("Bold") : TEXT("Regular")));
	}
	return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

FSlateBrush FGameXXKInRunUiStyle::Paper(const FVector2D& Size)
{
	return Texture(PaperPath, Size, true);
}

FButtonStyle FGameXXKInRunUiStyle::Action(const FVector2D& Size, const bool bPrimary)
{
	const TCHAR* Path = bPrimary
		? TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase") : SlotPath;
	FButtonStyle Style;
	Style.SetNormal(Texture(Path, Size, !bPrimary));
	Style.SetHovered(Texture(Path, Size, !bPrimary, FLinearColor(1.1f, 1.06f, 0.97f, 1.0f)));
	Style.SetPressed(Texture(Path, Size, !bPrimary, FLinearColor(0.72f, 0.69f, 0.62f, 1.0f)));
	Style.SetDisabled(Texture(Path, Size, !bPrimary, FLinearColor(0.68f, 0.66f, 0.61f, 0.82f)));
	Style.SetNormalPadding(FMargin(12.0f, 6.0f));
	Style.SetPressedPadding(FMargin(12.0f, 7.0f, 12.0f, 5.0f));
	return Style;
}

FButtonStyle FGameXXKInRunUiStyle::Choice(const FVector2D& Size, const bool bSelected)
{
	FButtonStyle Style;
	const FLinearColor Tint = bSelected ? FLinearColor(1.0f, 0.84f, 0.67f, 1.0f) : FLinearColor::White;
	Style.SetNormal(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, Tint));
	Style.SetHovered(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(1.08f, 1.03f, 0.91f, 1.0f)));
	Style.SetPressed(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(0.85f, 0.78f, 0.65f, 1.0f)));
	Style.SetDisabled(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(0.76f, 0.74f, 0.69f, 0.85f)));
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
	return Style;
}
