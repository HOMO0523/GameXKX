#include "UI/GameXXKInRunUiStyle.h"
#include "Audio/GameXXKSfx.h"
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

namespace
{
	/** Both project fonts stay loaded for the process lifetime; a weak cache keeps re-resolves cheap. */
	UFont* LoadProjectFont(const TCHAR* Path)
	{
		static TMap<FString, TWeakObjectPtr<UFont>> Cache;
		const FString Key(Path);
		if (const TWeakObjectPtr<UFont>* Found = Cache.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		UFont* FontAsset = LoadObject<UFont>(nullptr, Path, nullptr, LOAD_NoWarn);
		Cache.Add(Key, FontAsset);
		return FontAsset;
	}
}

const TCHAR* FGameXXKInRunUiStyle::FontPath(const EGameXXKFontRole Role)
{
	return Role == EGameXXKFontRole::Body ? BodyFontPath : TitleFontPath;
}

FSlateFontInfo FGameXXKInRunUiStyle::Font(const EGameXXKFontRole Role, const int32 Size, const bool bBold)
{
	if (UFont* FontAsset = LoadProjectFont(FontPath(Role)))
	{
		return FSlateFontInfo(FontAsset, Size, FName(TEXT("Default")));
	}
	// Missing-asset fallback only. Keeps headless commandlets and a project
	// opened before the fonts are imported rendering text instead of nothing.
	return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

FSlateFontInfo FGameXXKInRunUiStyle::TitleFont(const int32 Size, const bool bBold)
{
	return Font(EGameXXKFontRole::Title, Size, bBold);
}

FSlateFontInfo FGameXXKInRunUiStyle::BodyFont(const int32 Size, const bool bBold)
{
	return Font(EGameXXKFontRole::Body, Size, bBold);
}

FSlateFontInfo FGameXXKInRunUiStyle::OutlinedFont(const EGameXXKFontRole Role, const int32 Size, const int32 OutlineSize)
{
	FSlateFontInfo Result = Font(Role, Size, true);
	Result.OutlineSettings.OutlineSize = OutlineSize;
	Result.OutlineSettings.OutlineColor = FLinearColor(0.025f, 0.02f, 0.015f, 1.0f);
	return Result;
}

FSlateFontInfo FGameXXKInRunUiStyle::OutlinedTitleFont(const int32 Size, const int32 OutlineSize)
{
	return OutlinedFont(EGameXXKFontRole::Title, Size, OutlineSize);
}

FSlateFontInfo FGameXXKInRunUiStyle::OutlinedBodyFont(const int32 Size, const int32 OutlineSize)
{
	return OutlinedFont(EGameXXKFontRole::Body, Size, OutlineSize);
}

FSlateBrush FGameXXKInRunUiStyle::Paper(const FVector2D& Size)
{
	return Texture(PaperPath, Size, true);
}

FButtonStyle FGameXXKInRunUiStyle::Action(const FVector2D& Size, const bool bPrimary)
{
	const TCHAR* Path = bPrimary
		? TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase") : SlotPath;
	FButtonStyle Style; FGameXXKSfx::SetButtonSound(Style);
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
	FButtonStyle Style; FGameXXKSfx::SetButtonSound(Style);
	const FLinearColor Tint = bSelected ? FLinearColor(1.0f, 0.84f, 0.67f, 1.0f) : FLinearColor::White;
	Style.SetNormal(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, Tint));
	Style.SetHovered(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(1.08f, 1.03f, 0.91f, 1.0f)));
	Style.SetPressed(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(0.85f, 0.78f, 0.65f, 1.0f)));
	Style.SetDisabled(Texture(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame"), Size, true, FLinearColor(0.76f, 0.74f, 0.69f, 0.85f)));
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f, 1.0f, 0.0f, -1.0f));
	return Style;
}
