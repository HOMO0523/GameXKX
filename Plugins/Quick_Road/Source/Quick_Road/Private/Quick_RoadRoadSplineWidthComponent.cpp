// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadRoadSplineWidthComponent.h"

#include "Components/SplineComponent.h"

UQuick_RoadRoadSplineWidthComponent::UQuick_RoadRoadSplineWidthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UQuick_RoadRoadSplineWidthComponent::HasStoredHalfWidthCm(const USplineComponent* Spline)
{
	if (!Spline)
	{
		return false;
	}

	const AActor* Owner = Spline->GetOwner();
	if (!Owner)
	{
		return false;
	}

	const UQuick_RoadRoadSplineWidthComponent* WidthComponent =
		Owner->FindComponentByClass<UQuick_RoadRoadSplineWidthComponent>();
	return WidthComponent && WidthComponent->HalfWidthCm > KINDA_SMALL_NUMBER;
}

float UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(
	const USplineComponent* Spline,
	float FallbackHalfWidthCm)
{
	const float SafeFallback = FMath::Max(FallbackHalfWidthCm, 50.f);
	if (!Spline)
	{
		return SafeFallback;
	}

	const AActor* Owner = Spline->GetOwner();
	if (!Owner)
	{
		return SafeFallback;
	}

	const UQuick_RoadRoadSplineWidthComponent* WidthComponent =
		Owner->FindComponentByClass<UQuick_RoadRoadSplineWidthComponent>();
	if (!WidthComponent || WidthComponent->HalfWidthCm <= KINDA_SMALL_NUMBER)
	{
		return SafeFallback;
	}

	return FMath::Max(WidthComponent->HalfWidthCm, 50.f);
}

void UQuick_RoadRoadSplineWidthComponent::SetHalfWidthCm(USplineComponent* Spline, float InHalfWidthCm)
{
	if (!Spline)
	{
		return;
	}

	AActor* Owner = Spline->GetOwner();
	if (!Owner)
	{
		return;
	}

	const float ClampedHalfWidth = FMath::Max(InHalfWidthCm, 50.f);
	Owner->Modify();

	UQuick_RoadRoadSplineWidthComponent* WidthComponent =
		Owner->FindComponentByClass<UQuick_RoadRoadSplineWidthComponent>();
	if (!WidthComponent)
	{
		WidthComponent = NewObject<UQuick_RoadRoadSplineWidthComponent>(
			Owner,
			UQuick_RoadRoadSplineWidthComponent::StaticClass(),
			TEXT("RoadSplineWidth"),
			RF_Transactional);
		WidthComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Owner->AddInstanceComponent(WidthComponent);
		WidthComponent->RegisterComponent();
	}

	WidthComponent->Modify();
	WidthComponent->HalfWidthCm = ClampedHalfWidth;
}
