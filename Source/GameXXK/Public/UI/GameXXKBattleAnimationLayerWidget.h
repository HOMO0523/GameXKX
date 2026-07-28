#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "GameXXKBattleAnimationLayerWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTexture2D;

UCLASS()
class GAMEXXK_API UGameXXKBattleAnimationLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	void QueueCombatSequence(
		FName AttackerUnitId,
		bool bAttackerEnemy,
		FName TargetUnitId,
		bool bTargetEnemy,
		bool bTargetDefeated);
	void QueueStatusSequence(FName UnitId, bool bEnemy, bool bBuff);
	void ResetPresentation();

	UCanvasPanel* GetRootCanvasForTest() const;
	UBorder* GetDimmerForTest() const;
	UImage* GetAttackerImageForTest() const;
	UImage* GetTargetImageForTest() const;
	UImage* GetImpactImageForTest() const;
	float GetDimOpacityForTest() const;
	FVector2D GetUnitImageSizeForTest() const;
	FVector2D GetImpactAnchorForTest() const;
	bool IsPresentationActiveForTest() const;
	bool IsImpactVisibleForTest() const;
	int32 GetImpactFrameForTest() const;
	int32 GetQueuedSequenceCountForTest() const;
	void AdvanceAnimationForTest(float DeltaSeconds);

private:
	struct FQueuedSequence
	{
		FGameXXKBattleAnimationClipDescriptor LeftClip;
		FGameXXKBattleAnimationClipDescriptor RightClip;
		bool bHasLeft = false;
		bool bHasRight = false;
		bool bShowImpact = false;
	};

	void BuildProgrammaticLayout();
	void StartSequence(const FQueuedSequence& Sequence);
	void StartNextSequence();
	void AdvancePresentation(float DeltaSeconds);
	void FinishPresentation();
	void UpdateImageFrame(UImage* Image, UTexture2D* Texture, const FGameXXKBattleAnimationClipDescriptor& Clip, int32 FrameIndex);
	UTexture2D* LoadClipTexture(const FGameXXKBattleAnimationClipDescriptor& Clip) const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Dimmer;

	UPROPERTY(Transient)
	TObjectPtr<UImage> AttackerImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TargetImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ImpactImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LeftTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RightTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ImpactTexture;

	TArray<FQueuedSequence> QueuedSequences;
	FQueuedSequence ActiveSequence;
	FGameXXKBattleAnimationClipDescriptor ImpactClip;
	float PresentationElapsed = 0.0f;
	float PresentationDuration = 0.0f;
	int32 ImpactFrame = 0;
	bool bPresentationActive = false;
	bool bImpactStarted = false;
};
