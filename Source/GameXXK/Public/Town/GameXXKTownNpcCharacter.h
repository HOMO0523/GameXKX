#pragma once

#include "CoreMinimal.h"
#include "Interaction/GameXXKInteractable.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownNpcActor.h"
#include "GameXXKTownNpcCharacter.generated.h"

class UGameXXKMVPSubsystem;
class UGameXXKInteractableComponent;
class USphereComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKTownNpcCharacter : public AGameXXKHeroCharacter, public IGameXXKInteractable
{
	GENERATED_BODY()

public:
	AGameXXKTownNpcCharacter();

	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void SetNpcRole(EGameXXKTownNpcRole NewRole);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void SetNpcId(FName NewNpcId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	FName GetNpcId() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	EGameXXKTownNpcRole GetNpcRole() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool CanOfferQuest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool CanTrade() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool HasPrimaryInteractionAction() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool CanJoinParty() const;

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

	void ActivateNarrativeFollower(
		AActor* Target,
		float MinimumDistance = 220.0f,
		float TargetDistance = 260.0f,
		float MaximumDistance = 300.0f);
	void DismissNarrativeFollower();
	bool IsNarrativeFollowerActive() const { return bNarrativeFollowerActive; }
	float GetNarrativeFollowMinimumForTest() const
	{
		return NarrativeFollowMinimumDistance;
	}
	float GetNarrativeFollowMaximumForTest() const
	{
		return NarrativeFollowMaximumDistance;
	}

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	USphereComponent* GetInteractionArea() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	bool WasLastInteractionSuccessful() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool ApplyDefaultInteraction(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool ConfirmQuestDialogInteraction(APawn* InstigatorPawn);

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
	TObjectPtr<USphereComponent> InteractionArea;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	EGameXXKTownNpcRole NpcRole = EGameXXKTownNpcRole::Generic;

	/** Stable catalog identity. Tusi Chief and Song Jinbao force their fixed story/shop roles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	FName NpcId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town")
	bool bFollowerActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town")
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	float FollowDistance = 96.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Town")
	float FollowSpeed = 240.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town|Narrative")
	bool bNarrativeFollowerActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town|Narrative")
	float NarrativeFollowMinimumDistance = 220.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GameXXK|Town|Narrative")
	float NarrativeFollowMaximumDistance = 300.0f;

private:
	void RefreshNarrativeInteractionMetadata();
	void ConfigureStaticIdleVisual();
	UGameXXKMVPSubsystem* ResolveMVPSubsystem(APawn* InstigatorPawn) const;
	void RecordQuestNpcMovedLocation(UGameXXKMVPSubsystem* Subsystem, const FVector& Location);
	float GetGroundedRootZ() const;
	void ConfigureGroundedPlaneConstraint();
	void RaiseRootToGroundedHeightIfNeeded();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	bool bLastInteractionSuccessful = false;

	TEnumAsByte<ECollisionEnabled::Type> NarrativePreviousCapsuleCollision =
		ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<ECollisionEnabled::Type> NarrativePreviousInteractionCollision =
		ECollisionEnabled::QueryOnly;
	bool bNarrativeCollisionSnapshotValid = false;

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Interaction")
	TObjectPtr<UGameXXKInteractableComponent> NarrativeInteraction;
};
