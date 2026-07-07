#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKMVPRules.h"
#include "Interaction/GameXXKInteractable.h"
#include "GameXXKRouteEncounterSceneActor.generated.h"

class UBoxComponent;
class UGameXXKMVPSubsystem;
class UTextRenderComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKRouteEncounterSceneActor : public AActor, public IGameXXKInteractable
{
	GENERATED_BODY()

public:
	AGameXXKRouteEncounterSceneActor();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	EGameXXKScreen GetEncounterScreen() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	bool MatchesRuntimeScreen(EGameXXKScreen Screen) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	UBoxComponent* GetInteractionArea() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	UTextRenderComponent* GetFeedbackTextComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	bool WasLastInteractionSuccessful() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter")
	FText GetLastFailureReason() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter")
	bool ApplyDefaultInteraction(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	void SetEncounterScreenForTest(EGameXXKScreen InEncounterScreen);

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|RouteEncounter")
	void OnEncounterInteractionResolved(APawn* InstigatorPawn, bool bSucceeded, const FText& FailureReason);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|RouteEncounter")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|RouteEncounter")
	TObjectPtr<UBoxComponent> InteractionArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|RouteEncounter")
	TObjectPtr<UTextRenderComponent> FeedbackText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GameXXK|RouteEncounter")
	EGameXXKScreen EncounterScreen = EGameXXKScreen::RouteEvent;

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem(APawn* InstigatorPawn) const;
	FText BuildDefaultLabel() const;
	void RefreshDefaultLabel();
	void RefreshPlayerFlowWidgets(APawn* InstigatorPawn) const;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	bool bLastInteractionSuccessful = false;

	UPROPERTY(Transient)
	FText LastFailureReason;
};
