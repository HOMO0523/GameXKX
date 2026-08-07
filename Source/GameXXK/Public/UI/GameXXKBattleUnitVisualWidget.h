#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "GameXXKBattleUnitVisualWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * One persistent battle-unit image that moves between formation and cinematic
 * layout without being duplicated, mirrored, or reparented.
 */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	void ConfigureUnit(
		FName UnitId,
		bool bEnemy,
		const FVector2D& FormationAnchor,
		const FGameXXKBattleAnimationClipDescriptor& IdleClip);
	void SetAtlas(UTexture2D* AtlasTexture);
	void SetPlaybackClip(const FGameXXKBattleAnimationClipDescriptor& Clip, bool bLooping);
	void ShowFormationIdle();
	void ShowCinematic(const FGameXXKBattleAnimationClipDescriptor& Clip, const FVector2D& Anchor);
	void HideForCinematic();
	void RestoreFormation();
	void RemoveAfterDeath();
	void AdvanceAtRealTime(double AbsoluteSeconds);
	void SetCardTargetingAvailability(bool bTargeting, bool bLegalTarget);

	FVector2D GetPresentedSize() const;
	FVector2D GetStageCenter() const;

	UImage* GetUnitImageForTest() const;
	UMaterialInstanceDynamic* GetAtlasMaterialForTest() const;
	UTexture2D* GetAtlasForTest() const;
	int32 GetCurrentFrameForTest() const;
	int32 GetFrameParameterWriteCountForTest() const;
	bool IsRemovedForTest() const;
	bool IsDimmedForCardTargetingForTest() const;

private:
	void BuildProgrammaticLayout();
	void ApplyCanvasLayout();
	void RefreshImageVisibility();
	void ResetPlaybackClock();
	void ApplyFrame(int32 FrameIndex);

	UPROPERTY(Transient)
	TObjectPtr<UImage> UnitImage;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AtlasMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> AtlasTexture;

	FName ConfiguredUnitId = NAME_None;
	FGameXXKBattleAnimationClipDescriptor IdlePlaybackClip;
	FGameXXKBattleAnimationClipDescriptor ActivePlaybackClip;
	FVector2D FormationAnchor = FVector2D(0.5f, 0.5f);
	FVector2D CurrentAnchor = FVector2D(0.5f, 0.5f);
	FVector2D PresentedSize = FVector2D(410.0f, 410.0f);
	double PlaybackEpochSeconds = 0.0;
	double LastAbsoluteSeconds = 0.0;
	int32 CurrentFrame = INDEX_NONE;
	int32 FrameParameterWriteCount = 0;
	int32 CurrentZOrder = 10;
	bool bConfiguredEnemy = false;
	bool bPlaybackLooping = true;
	bool bPlaybackClockInitialized = false;
	bool bHasLastAbsoluteSeconds = false;
	bool bConfigured = false;
	bool bRemoved = false;
	bool bMaterialLoadAttempted = false;
	bool bDimmedForCardTargeting = false;
};
