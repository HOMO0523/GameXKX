#include "UI/GameXXKBattleBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr int32 Keys[] = {1,11,21,28,33,35,37,40,41,50,62,69,73,75,77,82,85,89,93,96,97,103,108,115,120,125,130,136,140,142,148,154,159,164,166,176,181,185,190,197,201,209,218,220,222,223,225,231,248,260,264,267,268,274,286,296};
	// Replace the cloud-wipe/camera cut with six painted exit/trail/re-entry poses.
	constexpr int32 PlaybackSlots[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,56,57,58,59,60,61,44,45,46,47,48,49,50,51,52,53,54,55};
	double FrameDuration(int32 Index)
	{
		const int32 SourceSlot = PlaybackSlots[Index];
		if (SourceSlot >= UE_ARRAY_COUNT(Keys)) return 0.04;
		const int32 Key = Keys[SourceSlot];
		const int32 Next = SourceSlot + 1 < UE_ARRAY_COUNT(Keys) ? Keys[SourceSlot + 1] : 297;
		double Duration = (Next - Key) / 90.0;
		if (Key < 34) Duration *= .8;
		else if (Key >= 74 && Key < 209) Duration *= .72;
		else if (Key >= 274) Duration *= .75;
		if (Key == 225) Duration += .09;
		if (Key == 268) Duration += .06;
		return FMath::Max(Duration, 0.04);
	}
}

bool UGameXXKBattleBoardWidget::BeginLightningUltimate(double AbsoluteSeconds)
{
	if (!WidgetTree || !BattleDesignStage) return false;
	if (LightningUltimateAtlases.IsEmpty())
	{
		for (int32 Index = 1; Index <= 8; ++Index)
		{
			const TCHAR* Suffix = Index >= 4 ? TEXT("_Unified") : TEXT("");
			const FString Name = Index == 8 ? TEXT("T_UltimateLightning057_TransitionV3")
				: FString::Printf(TEXT("T_UltimateLightning057_Atlas_%02d%s"), Index, Suffix);
			const FString Path = FString::Printf(TEXT("/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/%s.%s"), *Name, *Name);
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path);
			if (!Texture) { LightningUltimateAtlases.Reset(); return false; }
			LightningUltimateAtlases.Add(Texture);
		}
	}
	if (!LightningUltimateImage)
	{
		LightningUltimateImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleLightningUltimate"));
		UCanvasPanelSlot* ImageSlot = BattleDesignStage->AddChildToCanvas(LightningUltimateImage);
		ImageSlot->SetAnchors(FAnchors(0,0,1,1));
		ImageSlot->SetOffsets(FMargin(0));
		ImageSlot->SetZOrder(1000);
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/M_UltimateLightning057.M_UltimateLightning057"));
		if (!Parent) { LightningUltimateImage->SetVisibility(ESlateVisibility::Collapsed); return false; }
		LightningUltimateMaterial = UMaterialInstanceDynamic::Create(Parent, this);
		LightningUltimateImage->SetBrushFromMaterial(LightningUltimateMaterial);
	}
	if (!LightningUltimateMaterial) return false;
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("FlipX"), 1);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("OffsetX"), .12f);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("EdgeFeather"), .09f);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("CloudCompact"), 1);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("FrameScale"), .9f);
	LightningUltimateStart = AbsoluteSeconds;
	LightningUltimateImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AdvanceLightningUltimate(AbsoluteSeconds);
	return true;
}

bool UGameXXKBattleBoardWidget::AdvanceLightningUltimate(double AbsoluteSeconds)
{
	if (LightningUltimateStart < 0 || !LightningUltimateImage || !LightningUltimateMaterial) return false;
	double Remaining = FMath::Max(0.0, AbsoluteSeconds - LightningUltimateStart);
	int32 Index = 0;
	for (; Index < UE_ARRAY_COUNT(PlaybackSlots); ++Index)
	{
		const double Duration = FrameDuration(Index);
		if (Remaining < Duration) break;
		Remaining -= Duration;
	}
	if (Index == UE_ARRAY_COUNT(PlaybackSlots) && Remaining >= .25)
	{
		ResetLightningUltimate();
		return false;
	}
	LightningUltimateImage->SetRenderOpacity(Index == UE_ARRAY_COUNT(PlaybackSlots) ? 1.0f - static_cast<float>(Remaining / .25) : 1.0f);
	Index = FMath::Min(Index, static_cast<int32>(UE_ARRAY_COUNT(PlaybackSlots)) - 1);
	const int32 SourceSlot = PlaybackSlots[Index];
	const int32 Cell = SourceSlot % 8;
	const float X = (Cell % 4) * 648.0f + 4.0f;
	const float Y = (Cell / 4) * 368.0f + 4.0f;
	LightningUltimateMaterial->SetTextureParameterValue(TEXT("AtlasTexture"), LightningUltimateAtlases[SourceSlot / 8]);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("U0"), (X + .5f) / 2592.0f);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("V0"), (Y + .5f) / 736.0f);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("U1"), (X + 639.5f) / 2592.0f);
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("V1"), (Y + 359.5f) / 736.0f);
	const FVector2D Size = LightningUltimateImage->GetCachedGeometry().GetLocalSize();
	LightningUltimateMaterial->SetScalarParameterValue(TEXT("ViewportAspect"), Size.Y > 0 ? Size.X / Size.Y : 16.0f / 9.0f);
	return true;
}

void UGameXXKBattleBoardWidget::ResetLightningUltimate()
{
	LightningUltimateStart = -1.0;
	if (LightningUltimateImage) LightningUltimateImage->SetVisibility(ESlateVisibility::Collapsed);
}
