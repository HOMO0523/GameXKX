#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "GameXXKTownPlayerPawn.generated.h"

class UGameXXKInteractionComponent;
class UFloatingPawnMovement;
class UInputComponent;
class UPaperFlipbookComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKTownPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	AGameXXKTownPlayerPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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
	int32 CountTownInputBindingsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void MoveHorizontal(float Value);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void MoveVertical(float Value);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void Interact();

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

	float HorizontalIntent = 0.0f;
	float VerticalIntent = 0.0f;
};
