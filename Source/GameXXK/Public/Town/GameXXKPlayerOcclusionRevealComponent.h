#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameXXKPlayerOcclusionRevealComponent.generated.h"

class UMaterialInterface;
class UPaperFlipbookComponent;
class UPrimitiveComponent;

USTRUCT()
struct FGameXXKOcclusionOriginalMaterials
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Component;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Slots;
};

UCLASS(ClassGroup=(GameXXK), meta=(BlueprintSpawnableComponent))
class GAMEXXK_API UGameXXKPlayerOcclusionRevealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGameXXKPlayerOcclusionRevealComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	int32 GetModifiedComponentCount() const { return ModifiedComponents.Num(); }
	int32 GetBlockingComponentCount() const { return BlockingComponents.Num(); }
	int32 GetOcclusionSampleCount() const { return OcclusionSampleCount; }
	int32 GetMaxOcclusionLayers() const { return MaxOcclusionLayers; }
	float GetRevealRadiusNormalized() const { return RevealRadiusNormalized; }
	bool RestoresOriginalMaterials() const { return true; }
	bool IsRevealActive() const { return bRevealActive; }

	UFUNCTION(BlueprintPure, Category="GameXXK|Town|Occlusion")
	float GetTraceInterval() const { return TraceInterval; }

	UFUNCTION(BlueprintPure, Category="GameXXK|Town|Occlusion")
	float GetTraceRadius() const { return TraceRadius; }

	UFUNCTION(BlueprintPure, Category="GameXXK|Town|Occlusion")
	float GetActivationDelay() const { return ActivationDelay; }

	UFUNCTION(BlueprintPure, Category="GameXXK|Town|Occlusion")
	float GetReleaseDuration() const { return ReleaseDuration; }

	void BindRevealVisual(UPaperFlipbookComponent* InRevealVisual);
	void SetOcclusionOverrideForTest(TOptional<bool> InOverride) { OcclusionOverride = InOverride; }
	void UpdateRevealForTest(float DeltaSeconds);

private:
	void UpdateReveal(float DeltaSeconds);
	bool CollectWorldOccluders();
	bool IsRevealAllowedInCurrentScreen() const;
	void ApplyRevealState(bool bVisible);
	void RestoreAllModifiedComponents();

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion")
	float TraceInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion")
	float TraceRadius = 36.0f;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion")
	float ActivationDelay = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion")
	float ReleaseDuration = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion", meta=(ClampMin="1"))
	int32 OcclusionSampleCount = 9;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion", meta=(ClampMin="1"))
	int32 MaxOcclusionLayers = 6;

	UPROPERTY(EditDefaultsOnly, Category="GameXXK|Town|Occlusion", meta=(ClampMin="0.01", ClampMax="0.5"))
	float RevealRadiusNormalized = 0.18f;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> RevealVisual;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> BlockingComponents;

	UPROPERTY(Transient)
	TArray<FGameXXKOcclusionOriginalMaterials> ModifiedComponents;

	float TraceAccumulator = 0.0f;
	float ContinuousOccludedSeconds = 0.0f;
	float ReleaseRemainingSeconds = 0.0f;
	TOptional<bool> OcclusionOverride;
	bool bRevealActive = false;
};
