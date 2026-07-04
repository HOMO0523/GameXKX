#include "MVP/GameXXKOneGameIslandRouteMapBridge.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

AGameXXKOneGameIslandRouteMapBridge::AGameXXKOneGameIslandRouteMapBridge()
{
	PrimaryActorTick.bCanEverTick = false;
}

float AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(float NodeCanvasY, float TopPadding)
{
	return FMath::Max(0.0f, NodeCanvasY - FMath::Max(0.0f, TopPadding));
}

void AGameXXKOneGameIslandRouteMapBridge::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SyncTimerHandle,
			this,
			&AGameXXKOneGameIslandRouteMapBridge::SynchronizeRouteMapScroll,
			SyncIntervalSeconds,
			true,
			SyncIntervalSeconds);
	}
}

void AGameXXKOneGameIslandRouteMapBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SyncTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void AGameXXKOneGameIslandRouteMapBridge::SynchronizeRouteMapScroll()
{
	++SyncAttempts;
	if (MaxSyncAttempts > 0 && SyncAttempts > MaxSyncAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SyncTimerHandle);
		}
		return;
	}

	UClass* RouteMapClass = ResolveRouteMapWidgetClass();
	UClass* RouteNodeClass = ResolveRouteNodeWidgetClass();
	if (!RouteMapClass || !RouteNodeClass)
	{
		return;
	}

	UUserWidget* RouteMapWidget = FindRouteMapWidget(RouteMapClass);
	if (!RouteMapWidget || !RouteMapWidget->WidgetTree)
	{
		return;
	}

	FBoolProperty* ClickableProperty = FindRouteNodeClickableProperty(RouteNodeClass);
	if (!ClickableProperty)
	{
		return;
	}

	UUserWidget* BestNode = nullptr;
	UScrollBox* BestScrollBox = nullptr;
	float BestNodeY = TNumericLimits<float>::Max();
	RouteMapWidget->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		UUserWidget* RouteNodeWidget = Cast<UUserWidget>(Widget);
		if (!RouteNodeWidget || !RouteNodeWidget->IsA(RouteNodeClass) || !IsRouteNodeClickable(RouteNodeWidget, ClickableProperty))
		{
			return;
		}

		UCanvasPanelSlot* CanvasSlot = FindCanvasSlot(RouteNodeWidget);
		UScrollBox* ScrollBox = FindParentScrollBox(RouteNodeWidget);
		if (!CanvasSlot || !ScrollBox)
		{
			return;
		}

		const float NodeY = CanvasSlot->GetPosition().Y;
		if (NodeY < BestNodeY)
		{
			BestNodeY = NodeY;
			BestNode = RouteNodeWidget;
			BestScrollBox = ScrollBox;
		}
	});

	if (!BestNode || !BestScrollBox || BestNodeY == TNumericLimits<float>::Max())
	{
		return;
	}

	const float TargetOffset = CalculateTargetScrollOffset(BestNodeY, ScrollTopPadding);
	if (!FMath::IsNearlyEqual(TargetOffset, LastAppliedScrollOffset, 1.0f))
	{
		BestScrollBox->SetScrollOffset(TargetOffset);
		LastAppliedScrollOffset = TargetOffset;
	}
}

UClass* AGameXXKOneGameIslandRouteMapBridge::ResolveRouteMapWidgetClass() const
{
	return RouteMapWidgetClass.IsNull() ? nullptr : RouteMapWidgetClass.LoadSynchronous();
}

UClass* AGameXXKOneGameIslandRouteMapBridge::ResolveRouteNodeWidgetClass() const
{
	return RouteNodeWidgetClass.IsNull() ? nullptr : RouteNodeWidgetClass.LoadSynchronous();
}

UUserWidget* AGameXXKOneGameIslandRouteMapBridge::FindRouteMapWidget(UClass* RouteMapClass) const
{
	if (!RouteMapClass)
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || !Widget->IsA(RouteMapClass))
		{
			continue;
		}

		if (!World || Widget->GetWorld() == World)
		{
			return Widget;
		}
	}
	return nullptr;
}

FBoolProperty* AGameXXKOneGameIslandRouteMapBridge::FindRouteNodeClickableProperty(UClass* RouteNodeClass) const
{
	if (!RouteNodeClass)
	{
		return nullptr;
	}

	for (TFieldIterator<FBoolProperty> PropertyIt(RouteNodeClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FBoolProperty* BoolProperty = *PropertyIt;
		if (BoolProperty && BoolProperty->GetOwnerClass() == RouteNodeClass)
		{
			return BoolProperty;
		}
	}
	return nullptr;
}

bool AGameXXKOneGameIslandRouteMapBridge::IsRouteNodeClickable(UUserWidget* RouteNodeWidget, FBoolProperty* ClickableProperty) const
{
	return RouteNodeWidget && ClickableProperty && ClickableProperty->GetPropertyValue_InContainer(RouteNodeWidget);
}

UScrollBox* AGameXXKOneGameIslandRouteMapBridge::FindParentScrollBox(UUserWidget* RouteNodeWidget) const
{
	for (UWidget* Current = RouteNodeWidget; Current; Current = Current->GetParent())
	{
		if (UScrollBox* ScrollBox = Cast<UScrollBox>(Current->GetParent()))
		{
			return ScrollBox;
		}
	}
	return nullptr;
}

UCanvasPanelSlot* AGameXXKOneGameIslandRouteMapBridge::FindCanvasSlot(UUserWidget* RouteNodeWidget) const
{
	return RouteNodeWidget ? Cast<UCanvasPanelSlot>(RouteNodeWidget->Slot) : nullptr;
}
