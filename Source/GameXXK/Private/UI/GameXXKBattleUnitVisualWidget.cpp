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

	struct FCinematicAlphaHeights
	{
		const TCHAR* AssetToken;
		float Idle;
		float Attack;
		float Hit;
		float Death;
	};

	// Median occupied-cell heights from the approved atlas audit. Cinematic
	// widgets use one fixed 820px square, so action-specific transparent bounds
	// must be normalized against that identity's Idle height or the body visibly
	// pulses between close-up shots.
	constexpr FCinematicAlphaHeights CinematicAlphaHeights[] = {
		{TEXT("character_00_hero"), 0.8164f, 0.6016f, 0.6992f, 0.8125f},
		{TEXT("character_01_blade"), 0.7832f, 0.6484f, 0.5957f, 0.7109f},
		{TEXT("character_02_guard"), 0.7969f, 0.6367f, 0.7363f, 0.7617f},
		{TEXT("character_03_healer"), 0.7969f, 0.5879f, 0.8242f, 0.7930f},
		{TEXT("character_04_hunter"), 0.6582f, 0.5781f, 0.7129f, 0.6699f},
		{TEXT("character_05_sorcerer"), 0.7773f, 0.5566f, 0.6738f, 0.7461f},
		{TEXT("character_06_formation_master"), 0.7305f, 0.6094f, 0.5879f, 0.6699f},
		{TEXT("character_07_tusi_chief"), 0.6934f, 0.6074f, 0.5703f, 0.5664f},
		{TEXT("character_08_song_jin_bao"), 0.8340f, 0.8125f, 0.6367f, 0.7949f},
		{TEXT("character_09_yue_bai"), 0.7344f, 0.6250f, 0.6797f, 0.8281f},
		{TEXT("character_10_zhou_guang_zu"), 0.8262f, 0.7148f, 0.6680f, 0.7969f},
		{TEXT("character_11_jin_gui"), 0.6992f, 0.7207f, 0.5859f, 0.8242f},
		{TEXT("character_12_qiong_mei_er"), 0.8047f, 0.7383f, 0.7793f, 0.7715f},
		{TEXT("enemy_01_rooster"), 0.8047f, 0.5869f, 0.6816f, 0.7363f},
		{TEXT("enemy_02_goat"), 0.8320f, 0.6191f, 0.5352f, 0.7646f},
		{TEXT("enemy_03_weasel"), 0.5840f, 0.4307f, 0.5127f, 0.4824f},
		{TEXT("enemy_04_civet"), 0.5703f, 0.4688f, 0.5547f, 0.6133f},
		{TEXT("enemy_05_ironfeather"), 0.8066f, 0.5713f, 0.5723f, 0.6602f},
		{TEXT("enemy_06_bluehorn"), 0.7910f, 0.6094f, 0.5859f, 0.7090f},
		{TEXT("enemy_19_moneyrat_boss"), 0.6562f, 0.5713f, 0.5684f, 0.5781f},
	};

	struct FCorrectedCinematicAttackHeights
	{
		const TCHAR* AssetToken;
		float Idle;
		float Attack;
	};

	// The corrected 2026-08-27 2K/1K atlases use a different transparent-body
	// fit than the preserved 4K masters.  Keep this table resolution-specific so
	// explicit 4K regression views still use the legacy audit above.
	constexpr FCorrectedCinematicAttackHeights CorrectedCinematicAttackHeights[] = {
		{TEXT("enemy_01_rooster"), 0.824219f, 0.789062f},
		{TEXT("enemy_03_weasel"), 0.632812f, 0.406250f},
		{TEXT("enemy_05_ironfeather"), 0.783203f, 0.567383f},
		{TEXT("enemy_11_graymane"), 0.614258f, 0.578125f},
		{TEXT("enemy_16_toad"), 0.417969f, 0.321289f},
		{TEXT("enemy_18_deer"), 0.812500f, 0.789062f},
		{TEXT("character_09_yue_bai"), 0.808594f, 0.615234f},
	};

	float ResolveCorrectedCinematicContentScale(const FString& ClipAssetId)
	{
		const bool bCorrectedResolution = ClipAssetId.Contains(TEXT("_2k_"), ESearchCase::IgnoreCase)
			|| ClipAssetId.Contains(TEXT("_1k_"), ESearchCase::IgnoreCase);
		if (!bCorrectedResolution)
		{
			return 0.0f;
		}
		if (ClipAssetId.EndsWith(TEXT("_attack_punch"), ESearchCase::IgnoreCase))
		{
			return 0.828125f / 0.781250f;
		}
		if (ClipAssetId.EndsWith(TEXT("_attack_kick"), ESearchCase::IgnoreCase))
		{
			return 0.828125f / 0.794922f;
		}
		if (!ClipAssetId.EndsWith(TEXT("_attack"), ESearchCase::IgnoreCase))
		{
			return 0.0f;
		}
		for (const FCorrectedCinematicAttackHeights& Entry : CorrectedCinematicAttackHeights)
		{
			if (ClipAssetId.Contains(Entry.AssetToken))
			{
				return FMath::Clamp(Entry.Idle / FMath::Max(0.01f, Entry.Attack), 0.75f, 2.2f);
			}
		}
		return 0.0f;
	}

	float ResolveCinematicContentScale(const FString& ClipAssetId)
	{
		const float CorrectedScale = ResolveCorrectedCinematicContentScale(ClipAssetId);
		if (CorrectedScale > 0.0f)
		{
			return CorrectedScale;
		}
		for (const FCinematicAlphaHeights& Entry : CinematicAlphaHeights)
		{
			if (!ClipAssetId.Contains(Entry.AssetToken))
			{
				continue;
			}

			float ActionHeight = Entry.Idle;
			if (ClipAssetId.EndsWith(TEXT("_attack"), ESearchCase::IgnoreCase))
			{
				ActionHeight = Entry.Attack;
			}
			else if (ClipAssetId.EndsWith(TEXT("_hit"), ESearchCase::IgnoreCase))
			{
				ActionHeight = Entry.Hit;
			}
			else if (ClipAssetId.EndsWith(TEXT("_death"), ESearchCase::IgnoreCase))
			{
				ActionHeight = Entry.Death;
			}
			return FMath::Clamp(Entry.Idle / FMath::Max(0.01f, ActionHeight), 0.75f, 2.2f);
		}
		return 1.0f;
	}

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
	BuildProgrammaticLayout();
	if(UnitImage){const float Scale=IdleClip.AssetId.Contains(TEXT("enemy_16_toad"))?0.80f:1.0f;UnitImage->SetRenderTransformPivot(FVector2D(0.5f,0.70f));UnitImage->SetRenderScale(FVector2D(Scale,Scale));}
	bConfigured = !ConfiguredUnitId.IsNone();
	ResetProceduralPresentation();
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
	ResetProceduralPresentation();
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
	ResetProceduralPresentation();
	SetPlaybackClip(Clip, false);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const float ContentScale = ResolveCinematicContentScale(Clip.AssetId);
	SetRenderScale(FVector2D(ContentScale, ContentScale));
	ApplyCanvasLayout();
	RefreshImageVisibility();
}

void UGameXXKBattleUnitVisualWidget::HideForCinematic()
{
	if (bRemoved)
	{
		return;
	}

	ResetProceduralPresentation();
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

	ResetProceduralPresentation();
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

void UGameXXKBattleUnitVisualWidget::ApplyProceduralPresentation(
	const FVector2D& Translation,
	const float Opacity)
{
	if (bRemoved)
	{
		return;
	}
	SetRenderTranslation(Translation);
	SetRenderOpacity(FMath::IsFinite(Opacity) ? FMath::Clamp(Opacity, 0.0f, 1.0f) : 1.0f);
}

void UGameXXKBattleUnitVisualWidget::ResetProceduralPresentation()
{
	SetRenderTranslation(FVector2D::ZeroVector);
	SetRenderOpacity(1.0f);
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

FVector2D UGameXXKBattleUnitVisualWidget::GetProceduralTranslationForTest() const
{
	return GetRenderTransform().Translation;
}

float UGameXXKBattleUnitVisualWidget::GetProceduralOpacityForTest() const
{
	return GetRenderOpacity();
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
