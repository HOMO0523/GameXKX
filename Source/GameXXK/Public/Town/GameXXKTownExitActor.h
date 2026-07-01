#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GameXXKInteractable.h"
#include "GameXXKTownExitActor.generated.h"

class UBoxComponent;
class UGameXXKMVPSubsystem;
class UTextRenderComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKTownExitActor : public AActor, public IGameXXKInteractable
{
	GENERATED_BODY()

public:
	AGameXXKTownExitActor();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UBoxComponent* GetInteractionArea() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	UTextRenderComponent* GetFeedbackTextComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool WasLastInteractionSuccessful() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	FText GetLastFailureReason() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool ApplyDefaultInteraction(APawn* InstigatorPawn);

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnExitInteractionResolved(APawn* InstigatorPawn, bool bSucceeded, const FText& FailureReason);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UBoxComponent> InteractionArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UTextRenderComponent> FeedbackText;

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem(APawn* InstigatorPawn) const;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	bool bLastInteractionSuccessful = false;

	UPROPERTY(Transient)
	FText LastFailureReason;
};
