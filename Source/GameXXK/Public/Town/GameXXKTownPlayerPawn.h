#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "GameXXKTownPlayerPawn.generated.h"

class UGameXXKInteractionComponent;
class UFloatingPawnMovement;
class UInputComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EGameXXKTownFacingDirection : uint8
{
	South = 0,
	SouthWest = 1,
	West = 2,
	NorthWest = 3,
	North = 4,
	NorthEast = 5,
	East = 6,
	SouthEast = 7,
};

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKTownPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	AGameXXKTownPlayerPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UGameXXKInteractionComponent* GetInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UPrimitiveComponent* GetTownCollisionComponent() const;

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
	FSoftObjectPath GetTownFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const;

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

	void MoveRightPressed();
	void MoveRightReleased();
	void MoveLeftPressed();
	void MoveLeftReleased();
	void MoveForwardPressed();
	void MoveForwardReleased();
	void MoveBackwardPressed();
	void MoveBackwardReleased();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UPaperFlipbookComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UGameXXKInteractionComponent> Interaction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TSoftObjectPtr<UPaperFlipbook> DefaultTownFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	TMap<EGameXXKTownFacingDirection, TSoftObjectPtr<UPaperFlipbook>> TownDirectionFlipbookAssets;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GameXXK|Town|Visual")
	EGameXXKTownFacingDirection CurrentTownFacingDirection = EGameXXKTownFacingDirection::South;

	float HorizontalIntent = 0.0f;
	float VerticalIntent = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbook> DefaultTownFlipbookOverride;

	UPROPERTY(Transient)
	TMap<EGameXXKTownFacingDirection, TObjectPtr<UPaperFlipbook>> TownDirectionFlipbookOverrides;

	void ApplyDefaultTownFlipbook();
	void ApplyTownFacingFlipbook();
	UPaperFlipbook* GetTownFlipbookForDirection(EGameXXKTownFacingDirection Direction) const;
	void InitializeTownDirectionFlipbooks();
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
};
