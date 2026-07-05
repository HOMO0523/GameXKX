#include "MVP/GameXXKOneGameIslandRouteMapBridge.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "TimerManager.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UObject/UnrealType.h"

AGameXXKOneGameIslandRouteMapBridge::AGameXXKOneGameIslandRouteMapBridge()
{
	PrimaryActorTick.bCanEverTick = false;
}

float AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(float NodeCanvasY, float TopPadding)
{
	return FMath::Max(0.0f, NodeCanvasY - FMath::Max(0.0f, TopPadding));
}

bool AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForOriginalLevelAdvance(
	int32 PreviousLevel,
	int32 CurrentLevel,
	int32 BattleStartLevel)
{
	return BattleStartLevel > 0
		&& PreviousLevel != INDEX_NONE
		&& CurrentLevel > PreviousLevel
		&& PreviousLevel < BattleStartLevel
		&& CurrentLevel >= BattleStartLevel;
}

bool AGameXXKOneGameIslandRouteMapBridge::PrimeBattleSubsystemForIslandRoute(UGameXXKMVPSubsystem& Subsystem)
{
	return Subsystem.StartGame()
		&& Subsystem.SelectWorldRegion(UGameXXKMVPRules::RegionQingshan())
		&& Subsystem.AcceptQuest()
		&& Subsystem.OpenDungeonFromTownExit()
		&& Subsystem.SelectDungeonNode(EGameXXKNodeKind::Start)
		&& Subsystem.SelectDungeonNode(EGameXXKNodeKind::Battle)
		&& Subsystem.GetRuntimeState().Screen == EGameXXKScreen::Battle;
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

	SynchronizeOriginalBattleLayout(RouteMapWidget);

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

void AGameXXKOneGameIslandRouteMapBridge::SynchronizeOriginalBattleLayout(UUserWidget* RouteMapWidget)
{
	if (bBattleLayoutOpen)
	{
		const UGameXXKMVPSubsystem* Subsystem = BattleSubsystem.Get();
		if (Subsystem && Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			if (BattleBoardWidget)
			{
				BattleBoardWidget->RemoveFromParent();
				BattleBoardWidget = nullptr;
			}
			if (RouteMapWidget)
			{
				RouteMapWidget->SetVisibility(ESlateVisibility::Visible);
			}
			bBattleLayoutOpen = false;
		}
		return;
	}

	int32 CurrentLevel = INDEX_NONE;
	if (!TryReadOriginalCurrentLevel(CurrentLevel))
	{
		return;
	}

	if (LastObservedOriginalLevel == INDEX_NONE)
	{
		LastObservedOriginalLevel = CurrentLevel;
		return;
	}

	if (ShouldOpenBattleLayoutForOriginalLevelAdvance(LastObservedOriginalLevel, CurrentLevel, OriginalBattleStartLevel))
	{
		if (OpenBattleLayoutFromOriginalRoute(RouteMapWidget))
		{
			LastObservedOriginalLevel = CurrentLevel;
		}
		return;
	}

	LastObservedOriginalLevel = CurrentLevel;
}

bool AGameXXKOneGameIslandRouteMapBridge::TryReadOriginalCurrentLevel(int32& OutCurrentLevel) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return false;
	}

	FIntProperty* CurrentLevelProperty = FindOriginalCurrentLevelProperty(PlayerController->GetClass());
	if (!CurrentLevelProperty)
	{
		return false;
	}

	OutCurrentLevel = CurrentLevelProperty->GetPropertyValue_InContainer(PlayerController);
	return true;
}

FIntProperty* AGameXXKOneGameIslandRouteMapBridge::FindOriginalCurrentLevelProperty(UClass* PlayerControllerClass) const
{
	if (!PlayerControllerClass || OriginalCurrentLevelIntPropertyIndex < 0)
	{
		return nullptr;
	}

	int32 DirectIntPropertyIndex = 0;
	for (TFieldIterator<FIntProperty> PropertyIt(PlayerControllerClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FIntProperty* IntProperty = *PropertyIt;
		if (!IntProperty || IntProperty->GetOwnerClass() != PlayerControllerClass)
		{
			continue;
		}

		if (DirectIntPropertyIndex == OriginalCurrentLevelIntPropertyIndex)
		{
			return IntProperty;
		}
		++DirectIntPropertyIndex;
	}
	return nullptr;
}

bool AGameXXKOneGameIslandRouteMapBridge::OpenBattleLayoutFromOriginalRoute(UUserWidget* RouteMapWidget)
{
	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!PlayerController)
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveBattleSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	TSubclassOf<UGameXXKBattleBoardWidget> WidgetClass = BattleBoardWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKBattleBoardWidget::StaticClass();
	}
	if (!BattleBoardWidget)
	{
		BattleBoardWidget = CreateWidget<UGameXXKBattleBoardWidget>(PlayerController, WidgetClass);
	}
	if (!BattleBoardWidget)
	{
		return false;
	}

	if (RouteMapWidget)
	{
		RouteMapWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	BattleBoardWidget->SetMVPSubsystem(Subsystem);
	if (!BattleBoardWidget->IsInViewport())
	{
		BattleBoardWidget->AddToViewport(250);
	}
	BattleBoardWidget->RefreshFromState();

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(BattleBoardWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	PlayerController->bShowMouseCursor = true;

	bBattleLayoutOpen = true;
	return true;
}

UGameXXKMVPSubsystem* AGameXXKOneGameIslandRouteMapBridge::ResolveBattleSubsystem()
{
	if (BattleSubsystem)
	{
		return BattleSubsystem;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UObject* SubsystemOuter = GameInstance ? static_cast<UObject*>(GameInstance) : static_cast<UObject*>(this);
	BattleSubsystem = NewObject<UGameXXKMVPSubsystem>(SubsystemOuter);
	if (!BattleSubsystem || !PrimeBattleSubsystemForIslandRoute(*BattleSubsystem))
	{
		BattleSubsystem = nullptr;
	}
	return BattleSubsystem;
}
