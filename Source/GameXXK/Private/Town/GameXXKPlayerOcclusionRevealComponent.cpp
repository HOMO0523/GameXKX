#include "Town/GameXXKPlayerOcclusionRevealComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKOcclusionMaterialMap.h"

UGameXXKPlayerOcclusionRevealComponent::UGameXXKPlayerOcclusionRevealComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGameXXKPlayerOcclusionRevealComponent::BeginPlay()
{
	Super::BeginPlay();
	ParameterCollection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion.MPC_PlayerOcclusion"), nullptr, LOAD_NoWarn);
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
	UpdateMaterialParameters();
}

void UGameXXKPlayerOcclusionRevealComponent::UpdateRevealForTest(float DeltaSeconds)
{
	UpdateReveal(DeltaSeconds);
}

void UGameXXKPlayerOcclusionRevealComponent::ApplyCutoutMaterialsForTest(UPrimitiveComponent* Component)
{
	ApplyCutoutMaterials(Component);
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
	if (bVisible)
	{
		ApplyCutoutToBlockingComponents();
	}
	if (!bVisible)
	{
		RestoreAllModifiedComponents();
	}
}

void UGameXXKPlayerOcclusionRevealComponent::ApplyCutoutToBlockingComponents()
{
	for (UPrimitiveComponent* Component : BlockingComponents)
	{
		ApplyCutoutMaterials(Component);
	}
}

void UGameXXKPlayerOcclusionRevealComponent::ApplyCutoutMaterials(UPrimitiveComponent* Component)
{
	if (!IsValid(Component))
	{
		return;
	}
	for (const FGameXXKOcclusionOriginalMaterials& Existing : ModifiedComponents)
	{
		if (Existing.Component == Component)
		{
			return;
		}
	}

	FGameXXKOcclusionMaterialMap MaterialMap;
	FGameXXKOcclusionOriginalMaterials Entry;
	Entry.Component = Component;
	bool bChangedAnySlot = false;
	const int32 SlotCount = Component->GetNumMaterials();
	Entry.Slots.Reserve(SlotCount);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMaterialInterface* Original = Component->GetMaterial(SlotIndex);
		Entry.Slots.Add(Original);
		if (UMaterialInterface* Cutout = MaterialMap.Resolve(Original))
		{
			Component->SetMaterial(SlotIndex, Cutout);
			bChangedAnySlot = true;
		}
	}
	if (bChangedAnySlot)
	{
		ModifiedComponents.Add(MoveTemp(Entry));
	}
}

void UGameXXKPlayerOcclusionRevealComponent::UpdateMaterialParameters()
{
	if (!ParameterCollection)
	{
		return;
	}
	const AGameXXKHeroCharacter* Hero = Cast<AGameXXKHeroCharacter>(GetOwner());
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UPaperFlipbookComponent* HeroVisual = Hero ? Hero->GetTownVisualComponent() : nullptr;
	if (!Controller || !HeroVisual)
	{
		return;
	}

	int32 ViewX = 1;
	int32 ViewY = 1;
	Controller->GetViewportSize(ViewX, ViewY);
	FVector2D PixelPosition;
	if (!Controller->ProjectWorldLocationToScreen(HeroVisual->Bounds.Origin, PixelPosition, false))
	{
		return;
	}
	UKismetMaterialLibrary::SetVectorParameterValue(
		this, ParameterCollection, TEXT("OcclusionCenter"),
		FLinearColor(PixelPosition.X / FMath::Max(1, ViewX), PixelPosition.Y / FMath::Max(1, ViewY), 0.0f, 0.0f));
	UKismetMaterialLibrary::SetVectorParameterValue(
		this, ParameterCollection, TEXT("OcclusionAspect"),
		FLinearColor(static_cast<float>(ViewX) / FMath::Max(1, ViewY), 1.0f, 0.0f, 0.0f));
	UKismetMaterialLibrary::SetScalarParameterValue(
		this, ParameterCollection, TEXT("OcclusionRadius"), RevealRadiusNormalized);
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
