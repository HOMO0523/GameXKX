#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputCoreTypes.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "GameXXKHeroCharacter.generated.h"

class UGameXXKInteractionComponent;
class UGameXXKPlayerOcclusionRevealComponent;
class UInputComponent;
class UCameraComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UPrimitiveComponent;
class USpringArmComponent;

UENUM(BlueprintType)
enum class EGameXXKHeroTownAction : uint8
{
	Idle,
	WalkStart,
	WalkLoop,
	WalkStop,
	DeepBreath,
	AdjustBackpack,
	CollectItem,
	CombatIdle,
	Punch,
	Kick,
};

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKHeroCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGameXXKHeroCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UGameXXKInteractionComponent* GetInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UPrimitiveComponent* GetTownCollisionComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Camera")
	UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom.Get(); }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	UPaperFlipbookComponent* GetTownVisualComponent() const { return Visual.Get(); }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	UPaperFlipbookComponent* GetOcclusionRevealVisualComponent() const { return OcclusionRevealVisual.Get(); }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	UGameXXKPlayerOcclusionRevealComponent* GetOcclusionRevealComponent() const { return OcclusionReveal.Get(); }

	void SynchronizeOcclusionRevealVisualForTest();
	FString GetOcclusionRevealMaterialPathString() const;
	void InitializeOcclusionRevealMaterialForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool IsSupportedMovementKey(FKey Key) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool IsInteractionKey(FKey Key) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool HasTownVisual() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool HasAssignedTownFlipbook() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UPaperFlipbook* GetDefaultTownFlipbook() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	FSoftObjectPath GetDefaultTownFlipbookPath() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	FString GetDefaultTownFlipbookPathString() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UPaperFlipbook* GetCurrentTownFlipbook() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	EGameXXKTownFacingDirection GetTownFacingDirection() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	bool IsTownMoving() const { return bTownMoving; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	FVector GetTownMovementIntentVector() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	FSoftObjectPath GetTownFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	FSoftObjectPath GetTownIdleFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	FSoftObjectPath GetTownWalkFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	int32 CountTownInputBindingsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void MoveHorizontal(float Value);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void MoveVertical(float Value);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void Interact();

	/** Plays one approved town-state clip. One-shot actions return to the new Idle clip. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Visual")
	bool PlayTownAction(EGameXXKHeroTownAction Action);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Visual")
	void ReturnToTownIdle();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	EGameXXKHeroTownAction GetCurrentTownAction() const { return CurrentTownAction; }

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void ResetTownMovementInput();

	/** Test/automation-only key-state entry point that reuses the real town key handlers. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Automation")
	bool SetTownAutomationKeyState(FName KeyName, bool bPressed);

	void SetDefaultTownFlipbookForTest(UPaperFlipbook* InFlipbook);
	void SetTownDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook);
	void SetTownIdleDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook);
	void SetTownWalkDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook);

	void MoveRightPressed();
	void MoveRightReleased();
	void MoveLeftPressed();
	void MoveLeftReleased();
	void MoveForwardPressed();
	void MoveForwardReleased();
	void MoveBackwardPressed();
	void MoveBackwardReleased();

protected:
	void UpdateTownVisualFromMovementIntent(float Horizontal, float Vertical);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town|Camera")
	TObjectPtr<UCameraComponent> TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UPaperFlipbookComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TObjectPtr<UPaperFlipbookComponent> OcclusionRevealVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TObjectPtr<UGameXXKPlayerOcclusionRevealComponent> OcclusionReveal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UGameXXKInteractionComponent> Interaction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> DefaultTownFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TMap<EGameXXKTownFacingDirection, TSoftObjectPtr<UPaperFlipbook>> TownDirectionFlipbookAssets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TMap<EGameXXKTownFacingDirection, TSoftObjectPtr<UPaperFlipbook>> TownIdleDirectionFlipbookAssets;

	/**
	 * The controllable hero uses one authored left-facing three-clip locomotion
	 * set.  NPC subclasses disable this and retain their existing directional
	 * visual routing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	bool bUseHorizontalHeroLocomotion = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalIdleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalWalkStartFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalWalkLoopFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalWalkStopFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalDeepBreathFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalAdjustBackpackFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalCollectItemFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalCombatIdleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalPunchFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> TownHorizontalKickFlipbookAsset;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	EGameXXKTownFacingDirection CurrentTownFacingDirection = EGameXXKTownFacingDirection::West;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> DefaultTownFlipbookOverride;

	UPROPERTY(Transient)
	TMap<EGameXXKTownFacingDirection, TObjectPtr<UPaperFlipbook>> TownDirectionFlipbookOverrides;

	UPROPERTY(Transient)
	TMap<EGameXXKTownFacingDirection, TObjectPtr<UPaperFlipbook>> TownIdleDirectionFlipbookOverrides;

	void ApplyDefaultTownFlipbook();
	void ApplyTownFacingFlipbook();
	UPaperFlipbook* GetTownFlipbookForDirection(EGameXXKTownFacingDirection Direction) const;
	UPaperFlipbook* GetTownIdleFlipbookForDirection(EGameXXKTownFacingDirection Direction) const;
	UPaperFlipbook* GetTownWalkFlipbookForDirection(EGameXXKTownFacingDirection Direction) const;
	UPaperFlipbook* GetHorizontalTownIdleFlipbook() const;
	UPaperFlipbook* GetHorizontalTownWalkStartFlipbook() const;
	UPaperFlipbook* GetHorizontalTownWalkLoopFlipbook() const;
	UPaperFlipbook* GetHorizontalTownActionFlipbook(EGameXXKHeroTownAction Action) const;
	void InitializeTownDirectionFlipbooks();
	void SynchronizeOcclusionRevealVisual();
	void InitializeOcclusionRevealMaterial();
	void ApplyHorizontalTownFacingMirror();
	void UpdateHorizontalTownLocomotion(float Horizontal, float Vertical);
	void AdvanceHorizontalTownLocomotion();
	void TickHorizontalTownAmbient(float DeltaSeconds);
	void RefreshTownMovementIntent();
	void ReleaseHeldTownAutomationKeys();
	bool IsTownMovementBlockedByModalWindow() const;
	void UpdateTownFacingFromIntent(float Horizontal, float Vertical);
	float GetKeyboardHorizontalIntent() const;
	float GetKeyboardVerticalIntent() const;

	int32 RightInputPressCount = 0;
	int32 LeftInputPressCount = 0;
	int32 ForwardInputPressCount = 0;
	int32 BackwardInputPressCount = 0;
	bool bTownAutomationRightHeld = false;
	bool bTownAutomationLeftHeld = false;
	bool bTownAutomationForwardHeld = false;
	bool bTownAutomationBackwardHeld = false;
	float AxisHorizontalIntent = 0.0f;
	float AxisVerticalIntent = 0.0f;
	float HorizontalIntent = 0.0f;
	float VerticalIntent = 0.0f;
	bool bTownMoving = false;
	EGameXXKHeroTownAction CurrentTownAction = EGameXXKHeroTownAction::Idle;
	FRandomStream TownAmbientRandom{0x4A1C2026};
	float TownAmbientElapsedSeconds = 0.0f;
	float TownAmbientDelaySeconds = 10.0f;
	EGameXXKTownFacingDirection PendingStopDiagonalFacingDirection = EGameXXKTownFacingDirection::South;
	double PendingStopDiagonalReleaseTimeSeconds = -1.0;
	bool bHasPendingStopDiagonalFacingDirection = false;
};
