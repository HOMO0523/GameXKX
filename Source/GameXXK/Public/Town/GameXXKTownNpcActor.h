#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GameXXKInteractable.h"
#include "GameXXKTownNpcActor.generated.h"

class UPaperFlipbookComponent;
class USphereComponent;
class UGameXXKMVPSubsystem;

UENUM(BlueprintType)
enum class EGameXXKTownNpcRole : uint8
{
	Quest,
	Merchant,
	Follower,
	Generic
};

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKTownNpcActor : public AActor, public IGameXXKInteractable
{
	GENERATED_BODY()

public:
	AGameXXKTownNpcActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void SetNpcRole(EGameXXKTownNpcRole NewRole);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	EGameXXKTownNpcRole GetNpcRole() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool CanOfferQuest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool CanTrade() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void ActivateFollower(AActor* Target, float Distance = 96.0f);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void DismissFollower();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool IsFollowerActive() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	AActor* GetFollowTarget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	float GetFollowDistance() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	USphereComponent* GetInteractionArea() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool WasLastInteractionSuccessful() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool ApplyDefaultInteraction(APawn* InstigatorPawn);

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnQuestInteract(APawn* InstigatorPawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnMerchantInteract(APawn* InstigatorPawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnDefaultInteractionResolved(APawn* InstigatorPawn, bool bSucceeded);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<UPaperFlipbookComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<USphereComponent> InteractionArea;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	EGameXXKTownNpcRole NpcRole = EGameXXKTownNpcRole::Generic;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town")
	bool bFollowerActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	float FollowDistance = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	float FollowSpeed = 240.0f;

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem(APawn* InstigatorPawn) const;
	bool SaveInteractionProgress(UGameXXKMVPSubsystem* Subsystem) const;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	bool bLastInteractionSuccessful = false;
};
