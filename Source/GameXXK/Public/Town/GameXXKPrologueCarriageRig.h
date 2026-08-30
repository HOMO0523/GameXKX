#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Prologue/GameXXKPrologueCarriageTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "GameXXKPrologueCarriageRig.generated.h"

class AGameXXKHeroCharacter;
class AGameXXKMVPPlayerController;
class UCameraComponent;
class UGameXXKPrologueCarriageWidget;
class UGameXXKProloguePauseWidget;
class USceneComponent;
class USpringArmComponent;
class UTexture2D;
class UWidgetComponent;

DECLARE_MULTICAST_DELEGATE(FGameXXKPrologueCarriageFinished);

UCLASS()
class GAMEXXK_API AGameXXKPrologueCarriageRig : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKPrologueCarriageRig();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool CancelPresentation();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")
	bool IsPresentationActive() const { return bPresentationActive; }

	bool TogglePauseFromController();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")
	bool IsSequencePaused() const { return TimelineState.bPaused; }
	FGameXXKPrologueCarriageFinished& OnFinished() { return FinishedDelegate; }

	FVector GetStartOffsetForTest() const;
	FVector GetStopOffsetForTest() const;
	FVector GetExitOffsetForTest() const;
	FVector GetHeroRevealOffsetForTest() const;
	int32 GetCarriageSortPriorityForTest() const;
	FVector2D GetCarriageDrawSizeForTest() const;
	FVector2D GetCarriagePivotForTest() const;
	FRotator GetCarriageDisplayRotationForTest() const;
	bool UsesHeroCameraPoseForTest() const { return bCopyHeroCameraPose; }
	static FRotator ResolveCarriageFacingRotationForTest(
		const FVector& CarriageLocation,
		const FVector& CameraLocation);
	static FVector ApplyDisplayGroundOffsetForTest(const FVector& MarkerLocation);
	TSubclassOf<UGameXXKPrologueCarriageWidget> GetCarriageWidgetClassForTest() const;
	bool ShouldActivateForOptionsForTest(const FString& Options) const;
	FString GetRunStopTexturePathForTest(bool bLowResolution) const;
	FString GetPostStopIdleTexturePathForTest(bool bLowResolution) const;
	bool StartTimelineForTest();
	bool AdvanceTimelineForTest(float DeltaSeconds);
	bool SetSequencePausedForTest(bool bPaused);
	void SetPresentationActiveForTest(bool bActive) { bPresentationActive = bActive; }
	UFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")
	FGameXXKPrologueCarriageState GetTimelineStateForTest() const
	{
		return TimelineState;
	}

	UFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")
	int32 GetPresentedFrameForTest() const { return LastPresentedFrameIndex; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")
	bool IsPauseOverlayVisibleForTest() const { return PauseWidget != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Prologue|Carriage|Automation")
	void ForceFailureForTest(EGameXXKPrologueCarriageFailure Failure);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USceneComponent> CarriageStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USceneComponent> CarriageStop;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USceneComponent> CarriageExit;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USceneComponent> HeroReveal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<UCameraComponent> IntroCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	TObjectPtr<UWidgetComponent> CarriageDisplay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	FGameXXKPrologueCarriageConfig TimelineConfig;

private:
	friend class AGameXXKMVPPlayerController;
	void SetPresentationController(AGameXXKMVPPlayerController* InController)
	{
		Controller = InController;
	}
	void TryStartPresentation();
	bool LoadCarriageTextures();
	bool CaptureAndHideHero();
	void AdvancePresentation(float DeltaSeconds);
	void ApplyStep(const FGameXXKPrologueCarriageStepOutput& Step);
	void RevealHeroAtMarker();
	void CompletePresentation();
	void FailOpen(const TCHAR* Reason);
	void CleanupPresentation(bool bSuccessfulHandoff);
	UGameXXKPrologueCarriageWidget* ResolveCarriageWidget();
	bool SetSequencePaused(bool bPaused);
	bool ShowPauseOverlay();
	void HidePauseOverlay();
	void HandleResumeRequested();
	void HandleReturnDesktopRequested();
	void UpdateCarriageFacing();
	bool CopyHeroCameraPose();

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage|Assets")
	TSoftObjectPtr<UTexture2D> RunStopTexture2K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage|Assets")
	TSoftObjectPtr<UTexture2D> PostStopIdleTexture2K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage|Assets")
	TSoftObjectPtr<UTexture2D> RunStopTexture1K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage|Assets")
	TSoftObjectPtr<UTexture2D> PostStopIdleTexture1K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage")
	TSubclassOf<UGameXXKPrologueCarriageWidget> CarriageWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Carriage")
	bool bCopyHeroCameraPose = true;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedRunStopTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedPostStopIdleTexture;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKPrologueCarriageWidget> CarriageWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKProloguePauseWidget> PauseWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKHeroCharacter> Hero;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKMVPPlayerController> Controller;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage", meta = (AllowPrivateAccess = "true"))
	FGameXXKPrologueCarriageState TimelineState;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage", meta = (AllowPrivateAccess = "true"))
	int32 LastPresentedFrameIndex = INDEX_NONE;
	FTransform HeroOriginalTransform;
	TEnumAsByte<ECollisionEnabled::Type> HeroOriginalCollision = ECollisionEnabled::QueryAndPhysics;
	uint8 HeroOriginalMovementMode = 0;
	bool bHeroSnapshotValid = false;
	bool bHeroWasHidden = false;
	bool bPresentationActive = false;
	bool bCleanupInProgress = false;
	int32 StartRetryCount = 0;
	static constexpr int32 MaximumStartRetries = 120;
	static constexpr float CarriageDisplayGroundOffsetZ = -72.0f;
	FGameXXKPrologueCarriageFinished FinishedDelegate;
};
