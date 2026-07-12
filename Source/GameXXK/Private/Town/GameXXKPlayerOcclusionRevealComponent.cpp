#include "Town/GameXXKPlayerOcclusionRevealComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"

UGameXXKPlayerOcclusionRevealComponent::UGameXXKPlayerOcclusionRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGameXXKPlayerOcclusionRevealComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyRevealState(false);
}

void UGameXXKPlayerOcclusionRevealComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreAllModifiedComponents();
	Super::EndPlay(EndPlayReason);
}

void UGameXXKPlayerOcclusionRevealComponent::BindRevealVisual(UPaperFlipbookComponent* InRevealVisual)
{
	RevealVisual = InRevealVisual;
}

void UGameXXKPlayerOcclusionRevealComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (OcclusionOverride.IsSet())
	{
		UpdateReveal(DeltaTime);
		return;
	}

	TraceAccumulator += DeltaTime;
	if (TraceAccumulator < TraceInterval)
	{
		return;
	}

	const float SampleDelta = TraceAccumulator;
	TraceAccumulator = 0.0f;
	UpdateReveal(SampleDelta);
}

void UGameXXKPlayerOcclusionRevealComponent::UpdateRevealForTest(float DeltaSeconds)
{
	UpdateReveal(DeltaSeconds);
}

void UGameXXKPlayerOcclusionRevealComponent::UpdateReveal(float DeltaSeconds)
{
	const bool bAllowed = OcclusionOverride.IsSet() || IsRevealAllowedInCurrentScreen();
	const bool bBlocked = bAllowed && (OcclusionOverride.IsSet() ? OcclusionOverride.GetValue() : CollectWorldOccluders());

	if (bBlocked)
	{
		ContinuousOccludedSeconds += FMath::Max(0.0f, DeltaSeconds);
		if (ContinuousOccludedSeconds >= ActivationDelay)
		{
			ReleaseRemainingSeconds = ReleaseDuration;
			ApplyRevealState(true);
		}
		return;
	}

	ContinuousOccludedSeconds = 0.0f;
	ReleaseRemainingSeconds = FMath::Max(0.0f, ReleaseRemainingSeconds - FMath::Max(0.0f, DeltaSeconds));
	ApplyRevealState(ReleaseRemainingSeconds > 0.0f);
}

bool UGameXXKPlayerOcclusionRevealComponent::CollectWorldOccluders()
{
	BlockingComponents.Reset();
	const AGameXXKHeroCharacter* Hero = Cast<AGameXXKHeroCharacter>(GetOwner());
	const UWorld* World = GetWorld();
	const UPaperFlipbookComponent* HeroVisual = Hero ? Hero->GetTownVisualComponent() : nullptr;
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (!Hero || !World || !HeroVisual || !CameraManager)
	{
		return false;
	}

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector TargetCenter = HeroVisual->Bounds.Origin;
	const FRotationMatrix CameraAxes(CameraManager->GetCameraRotation());
	const FVector CameraRight = CameraAxes.GetScaledAxis(EAxis::Y);
	const FVector CameraUp = CameraAxes.GetScaledAxis(EAxis::Z);
	const float TargetDistance = FVector::Distance(CameraLocation, TargetCenter);
	const float SampleRingRadius = 2.0f * TargetDistance
		* FMath::Tan(FMath::DegreesToRadians(CameraManager->GetFOVAngle() * 0.5f))
		* RevealRadiusNormalized;

	for (int32 SampleIndex = 0; SampleIndex < OcclusionSampleCount; ++SampleIndex)
	{
		FVector SampleOffset = FVector::ZeroVector;
		if (SampleIndex > 0)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(SampleIndex - 1) / static_cast<float>(OcclusionSampleCount - 1);
			SampleOffset = (CameraRight * FMath::Cos(Angle) + CameraUp * FMath::Sin(Angle)) * SampleRingRadius;
		}

		const FVector SampleTarget = TargetCenter + SampleOffset;
		const float SampleDistance = FVector::Distance(CameraLocation, SampleTarget);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GameXXKPlayerOcclusionCutout), false, Hero);
		for (int32 LayerIndex = 0; LayerIndex < MaxOcclusionLayers; ++LayerIndex)
		{
			FHitResult Hit;
			if (!World->SweepSingleByObjectType(
				Hit, CameraLocation, SampleTarget, FQuat::Identity, Objects,
				FCollisionShape::MakeSphere(TraceRadius), Params))
			{
				break;
			}

			UPrimitiveComponent* Component = Hit.GetComponent();
			if (!IsValid(Component) || Hit.Distance >= SampleDistance - TraceRadius * 1.5f)
			{
				break;
			}
			BlockingComponents.AddUnique(Component);
			Params.AddIgnoredComponent(Component);
		}
	}
	return !BlockingComponents.IsEmpty();
}

bool UGameXXKPlayerOcclusionRevealComponent::IsRevealAllowedInCurrentScreen() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UGameXXKMVPSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
	return Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town;
}

void UGameXXKPlayerOcclusionRevealComponent::ApplyRevealState(bool bVisible)
{
	bRevealActive = bVisible;
	if (RevealVisual)
	{
		RevealVisual->SetVisibility(false, true);
		RevealVisual->SetHiddenInGame(true, true);
	}
	if (!bVisible)
	{
		RestoreAllModifiedComponents();
	}
}

void UGameXXKPlayerOcclusionRevealComponent::RestoreAllModifiedComponents()
{
	for (const FGameXXKOcclusionOriginalMaterials& Entry : ModifiedComponents)
	{
		if (!IsValid(Entry.Component))
		{
			continue;
		}
		for (int32 SlotIndex = 0; SlotIndex < Entry.Slots.Num(); ++SlotIndex)
		{
			Entry.Component->SetMaterial(SlotIndex, Entry.Slots[SlotIndex]);
		}
	}
	ModifiedComponents.Reset();
}
