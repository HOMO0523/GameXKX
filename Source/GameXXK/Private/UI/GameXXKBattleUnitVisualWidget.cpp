#include "UI/GameXXKBattleUnitVisualWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include <cmath>
#include <limits>

namespace
{
	const TCHAR* BattleUnitAtlasMaterialPath =
		TEXT("/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI.M_BattleUnitAtlasUI");
	const FName AtlasTextureParameter(TEXT("AtlasTexture"));
	const FName FrameColumnParameter(TEXT("FrameColumn"));
	const FName FrameRowParameter(TEXT("FrameRow"));
	const FVector2D FormationVisualSize(410.0f, 410.0f);
	const FVector2D CinematicVisualSize(820.0f, 820.0f);
	const FVector2D DesignStageSize(1920.0f, 1080.0f);
	const FLinearColor FullColorTint = FLinearColor::White;
	const FLinearColor InvalidCardTargetTint(0.30f, 0.30f, 0.30f, 0.58f);
	constexpr int32 FormationZOrder = 10;
	constexpr int32 CinematicZOrder = 40;

	bool IsHiddenVisibility(const ESlateVisibility Visibility)
	{
		return Visibility == ESlateVisibility::Hidden || Visibility == ESlateVisibility::Collapsed;
	}

	int32 CalculateAbsoluteClockFrame(
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		const double AbsoluteSeconds,
		const double EpochSeconds,
		const bool bLooping)
	{
		const double ElapsedSeconds = FMath::Max(0.0, AbsoluteSeconds - EpochSeconds);
		double FramePosition = ElapsedSeconds
			* static_cast<double>(Clip.PlaybackRate)
			* static_cast<double>(Clip.SourceFramesPerSecond);
		if (!FMath::IsFinite(FramePosition))
		{
			return 0;
		}

		// Subtracting two large absolute clock samples can land an exact frame
		// boundary a few double ULPs below its integer. Snap only when the
		// discrepancy is within the uncertainty introduced by that subtraction;
		// ordinary samples immediately before a boundary remain on the prior frame.
		const double ClockMagnitude = FMath::Max(
			1.0,
			FMath::Max(FMath::Abs(AbsoluteSeconds), FMath::Abs(EpochSeconds)));
		const double ClockUlp = std::nextafter(ClockMagnitude, std::numeric_limits<double>::infinity())
			- ClockMagnitude;
		const double BoundaryToleranceFrames = ClockUlp
			* static_cast<double>(Clip.PlaybackRate)
			* static_cast<double>(Clip.SourceFramesPerSecond)
			* 4.0;
		const double NearestFrame = FMath::RoundToDouble(FramePosition);
		if (FMath::Abs(FramePosition - NearestFrame) <= BoundaryToleranceFrames)
		{
			FramePosition = NearestFrame;
		}

		const double UnboundedFrame = FMath::FloorToDouble(FramePosition);
		const double LastFrame = static_cast<double>(Clip.FrameCount - 1);
		const double SafeFrame = bLooping
			? FMath::Clamp(FMath::Fmod(UnboundedFrame, static_cast<double>(Clip.FrameCount)), 0.0, LastFrame)
			: FMath::Clamp(UnboundedFrame, 0.0, LastFrame);
		return static_cast<int32>(SafeFrame);
	}
}

TSharedRef<SWidget> UGameXXKBattleUnitVisualWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	RefreshImageVisibility();
	return Super::RebuildWidget();
}

void UGameXXKBattleUnitVisualWidget::NativeDestruct()
{
	ResetPlaybackClock();
	SetAtlas(nullptr);
	Super::NativeDestruct();
}

void UGameXXKBattleUnitVisualWidget::ConfigureUnit(
	const FName UnitId,
	const bool bEnemy,
	const FVector2D& InFormationAnchor,
	const FGameXXKBattleAnimationClipDescriptor& IdleClip)
{
	if (bRemoved)
	{
		return;
	}

	ConfiguredUnitId = UnitId;
	bConfiguredEnemy = bEnemy;
	FormationAnchor = InFormationAnchor;
	CurrentAnchor = FormationAnchor;
	IdlePlaybackClip = IdleClip;
	bConfigured = !ConfiguredUnitId.IsNone();
	SetPlaybackClip(IdlePlaybackClip, true);
	ApplyCanvasLayout();
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::SetAtlas(UTexture2D* const InAtlasTexture)
{
	BuildProgrammaticLayout();
	if (AtlasTexture == InAtlasTexture)
	{
		RefreshImageVisibility();
		return;
	}

	AtlasTexture = InAtlasTexture;
	CurrentFrame = INDEX_NONE;
	ResetPlaybackClock();
	if (AtlasMaterial)
	{
		if (AtlasTexture)
		{
			AtlasMaterial->SetTextureParameterValue(AtlasTextureParameter, AtlasTexture);
		}
		else
		{
			AtlasMaterial->ClearParameterValues();
		}
	}
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::SetPlaybackClip(
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const bool bLooping)
{
	ActivePlaybackClip = Clip;
	bPlaybackLooping = bLooping;
	CurrentFrame = INDEX_NONE;
	ResetPlaybackClock();
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::ShowFormationIdle()
{
	if (bRemoved)
	{
		return;
	}

	CurrentAnchor = FormationAnchor;
	PresentedSize = FormationVisualSize;
	CurrentZOrder = FormationZOrder;
	SetPlaybackClip(IdlePlaybackClip, true);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetRenderScale(FVector2D(1.0f, 1.0f));
	ApplyCanvasLayout();
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::ShowCinematic(
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const FVector2D& Anchor)
{
	if (bRemoved)
	{
		return;
	}

	CurrentAnchor = Anchor;
	PresentedSize = CinematicVisualSize;
	CurrentZOrder = CinematicZOrder;
	SetPlaybackClip(Clip, false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetRenderScale(FVector2D(1.0f, 1.0f));
	ApplyCanvasLayout();
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::HideForCinematic()
{
	if (bRemoved)
	{
		return;
	}

	SetVisibility(ESlateVisibility::Hidden);
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::RestoreFormation()
{
	if (!bRemoved)
	{
		ShowFormationIdle();
	}
}

void UGameXXKBattleUnitVisualWidget::RemoveAfterDeath()
{
	if (bRemoved)
	{
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);
	if (UnitImage)
	{
		UnitImage->SetVisibility(ESlateVisibility::Hidden);
	}
	ResetPlaybackClock();
	SetAtlas(nullptr);
	bRemoved = true;
	RemoveFromParent();
}

void UGameXXKBattleUnitVisualWidget::AdvanceAtRealTime(const double AbsoluteSeconds)
{
	if (!FMath::IsFinite(AbsoluteSeconds)
		|| bRemoved
		|| !bConfigured
		|| !UnitImage
		|| !AtlasMaterial
		|| !AtlasTexture
		|| !ActivePlaybackClip.IsValid()
		|| IsHiddenVisibility(GetVisibility())
		|| IsHiddenVisibility(UnitImage->GetVisibility()))
	{
		return;
	}

	if (!bPlaybackClockInitialized || (bHasLastAbsoluteSeconds && AbsoluteSeconds < LastAbsoluteSeconds))
	{
		PlaybackEpochSeconds = AbsoluteSeconds;
		bPlaybackClockInitialized = true;
	}
	LastAbsoluteSeconds = AbsoluteSeconds;
	bHasLastAbsoluteSeconds = true;

	const int32 FrameIndex = CalculateAbsoluteClockFrame(
		ActivePlaybackClip,
		AbsoluteSeconds,
		PlaybackEpochSeconds,
		bPlaybackLooping);
	if (FrameIndex != CurrentFrame)
	{
		ApplyFrame(FrameIndex);
	}
}

void UGameXXKBattleUnitVisualWidget::SetCardTargetingAvailability(
	const bool bTargeting,
	const bool bLegalTarget)
{
	const bool bShouldDim = bTargeting && !bLegalTarget && !bRemoved;
	if (bDimmedForCardTargeting == bShouldDim)
	{
		return;
	}
	bDimmedForCardTargeting = bShouldDim;
	if (UnitImage)
	{
		UnitImage->SetColorAndOpacity(bDimmedForCardTargeting ? InvalidCardTargetTint : FullColorTint);
	}
}

FVector2D UGameXXKBattleUnitVisualWidget::GetPresentedSize() const
{
	return PresentedSize;
}

FVector2D UGameXXKBattleUnitVisualWidget::GetStageCenter() const
{
	return FVector2D(CurrentAnchor.X * DesignStageSize.X, CurrentAnchor.Y * DesignStageSize.Y);
}

UImage* UGameXXKBattleUnitVisualWidget::GetUnitImageForTest() const
{
	return UnitImage;
}

UMaterialInstanceDynamic* UGameXXKBattleUnitVisualWidget::GetAtlasMaterialForTest() const
{
	return AtlasMaterial;
}

UTexture2D* UGameXXKBattleUnitVisualWidget::GetAtlasForTest() const
{
	return AtlasTexture;
}

int32 UGameXXKBattleUnitVisualWidget::GetCurrentFrameForTest() const
{
	return CurrentFrame;
}

int32 UGameXXKBattleUnitVisualWidget::GetFrameParameterWriteCountForTest() const
{
	return FrameParameterWriteCount;
}

bool UGameXXKBattleUnitVisualWidget::IsRemovedForTest() const
{
	return bRemoved;
}

bool UGameXXKBattleUnitVisualWidget::IsDimmedForCardTargetingForTest() const
{
	return bDimmedForCardTargeting;
}

void UGameXXKBattleUnitVisualWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleUnitVisualWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}

	if (!UnitImage)
	{
		UnitImage = Cast<UImage>(WidgetTree->RootWidget);
		if (!UnitImage && !WidgetTree->RootWidget)
		{
			UnitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleUnitAtlasImage"));
			WidgetTree->RootWidget = UnitImage;
		}
	}
	if (!UnitImage)
	{
		return;
	}

	if (!AtlasMaterial && !bMaterialLoadAttempted)
	{
		bMaterialLoadAttempted = true;
		UMaterialInterface* const ParentMaterial = LoadObject<UMaterialInterface>(nullptr, BattleUnitAtlasMaterialPath);
		AtlasMaterial = ParentMaterial ? UMaterialInstanceDynamic::Create(ParentMaterial, this) : nullptr;
		if (!AtlasMaterial)
		{
			UE_LOG(LogTemp, Error, TEXT("Battle unit visual could not load atlas UI material: %s"), BattleUnitAtlasMaterialPath);
		}
	}

	if (AtlasMaterial)
	{
		UnitImage->SetBrushFromMaterial(AtlasMaterial);
		if (AtlasTexture)
		{
			AtlasMaterial->SetTextureParameterValue(AtlasTextureParameter, AtlasTexture);
		}
	}
	UnitImage->SetVisibility(ESlateVisibility::Hidden);
	UnitImage->SetColorAndOpacity(bDimmedForCardTargeting ? InvalidCardTargetTint : FullColorTint);
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SetRenderScale(FVector2D(1.0f, 1.0f));
}

void UGameXXKBattleUnitVisualWidget::ApplyCanvasLayout()
{
	UCanvasPanelSlot* const CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!CanvasSlot)
	{
		return;
	}

	CanvasSlot->SetAnchors(FAnchors(CurrentAnchor.X, CurrentAnchor.Y));
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetPosition(FVector2D::ZeroVector);
	CanvasSlot->SetSize(PresentedSize);
	CanvasSlot->SetZOrder(CurrentZOrder);
}

void UGameXXKBattleUnitVisualWidget::RefreshImageVisibility()
{
	if (!UnitImage)
	{
		return;
	}

	const bool bCanShow = !bRemoved
		&& bConfigured
		&& AtlasMaterial
		&& AtlasTexture
		&& ActivePlaybackClip.IsValid()
		&& !IsHiddenVisibility(GetVisibility());
	UnitImage->SetVisibility(bCanShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void UGameXXKBattleUnitVisualWidget::ResetPlaybackClock()
{
	bPlaybackClockInitialized = false;
	bHasLastAbsoluteSeconds = false;
	PlaybackEpochSeconds = 0.0;
	LastAbsoluteSeconds = 0.0;
}

void UGameXXKBattleUnitVisualWidget::ApplyFrame(const int32 FrameIndex)
{
	if (!AtlasMaterial || !ActivePlaybackClip.IsValid())
	{
		return;
	}

	const int32 SafeFrame = FMath::Clamp(FrameIndex, 0, ActivePlaybackClip.FrameCount - 1);
	AtlasMaterial->SetScalarParameterValue(
		FrameColumnParameter,
		static_cast<float>(SafeFrame % ActivePlaybackClip.Columns));
	AtlasMaterial->SetScalarParameterValue(
		FrameRowParameter,
		static_cast<float>(SafeFrame / ActivePlaybackClip.Columns));
	CurrentFrame = SafeFrame;
	++FrameParameterWriteCount;
}
