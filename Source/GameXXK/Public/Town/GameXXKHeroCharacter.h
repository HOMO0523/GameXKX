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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void ResetTownMovementInput();

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

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	EGameXXKTownFacingDirection CurrentTownFacingDirection = EGameXXKTownFacingDirection::South;

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
	void InitializeTownDirectionFlipbooks();
	void SynchronizeOcclusionRevealVisual();
	void InitializeOcclusionRevealMaterial();
	void RefreshTownMovementIntent();
	bool IsTownMovementBlockedByModalWindow() const;
	void UpdateTownFacingFromIntent(float Horizontal, float Vertical);
	float GetKeyboardHorizontalIntent() const;
	float GetKeyboardVerticalIntent() const;

	int32 RightInputPressCount = 0;
	int32 LeftInputPressCount = 0;
	int32 ForwardInputPressCount = 0;
	int32 BackwardInputPressCount = 0;
	float AxisHorizontalIntent = 0.0f;
	float AxisVerticalIntent = 0.0f;
	float HorizontalIntent = 0.0f;
	float VerticalIntent = 0.0f;
	bool bTownMoving = false;
	EGameXXKTownFacingDirection PendingStopDiagonalFacingDirection = EGameXXKTownFacingDirection::South;
	double PendingStopDiagonalReleaseTimeSeconds = -1.0;
	bool bHasPendingStopDiagonalFacingDirection = false;
};
