#include "UI/GameXXKMVPHUD.h"

#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKPlayableRootWidget.h"

namespace
{
	const FVector2D RouteMapViewportPosition(420.0f, 124.0f);
	const FVector2D RouteMapViewportSize(560.0f, 520.0f);
}

AGameXXKMVPHUD::AGameXXKMVPHUD()
{
	MainMenuWidgetClass = UGameXXKMainMenuWidget::StaticClass();
	PlayableRootWidgetClass = UGameXXKPlayableRootWidget::StaticClass();
	RouteMapWidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
	BattleBoardWidgetClass = UGameXXKBattleBoardWidget::StaticClass();
}

void AGameXXKMVPHUD::BeginPlay()
{
	Super::BeginPlay();

	if (bShowDebugPlayableShell)
	{
		if (UGameXXKPlayableRootWidget* RootWidget = CreatePlayableRootWidget())
		{
			if (!RootWidget->IsInViewport())
			{
				RootWidget->AddToViewport(30);
			}
		}
	}
}

UGameXXKMVPSubsystem* AGameXXKMVPHUD::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

void AGameXXKMVPHUD::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(InSubsystem);
		MainMenuWidget->RefreshFromState();
	}
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetMVPSubsystem(InSubsystem);
		PlayableRootWidget->RefreshFromState();
	}
	if (RouteMapWidget)
	{
		RouteMapWidget->SetMVPSubsystem(InSubsystem);
		RouteMapWidget->RefreshFromState();
	}
	if (BattleBoardWidget)
	{
		BattleBoardWidget->SetMVPSubsystem(InSubsystem);
		BattleBoardWidget->RefreshFromState();
	}
}

void AGameXXKMVPHUD::SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex)
{
	StartGameSlotNameOverride = SlotName;
	StartGameUserIndexOverride = UserIndex;
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetStartGameSlotForTest(SlotName, UserIndex);
	}
}

UGameXXKMainMenuWidget* AGameXXKMVPHUD::CreateMainMenuWidgetForTest()
{
	return CreateMainMenuWidget();
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::CreatePlayableRootWidgetForTest()
{
	return CreatePlayableRootWidget();
}

UGameXXKOneGameRouteMapWidget* AGameXXKMVPHUD::CreateRouteMapWidgetForTest()
{
	return CreateRouteMapWidget();
}

UGameXXKBattleBoardWidget* AGameXXKMVPHUD::CreateBattleBoardWidgetForTest()
{
	return CreateBattleBoardWidget();
}

bool AGameXXKMVPHUD::HasMainMenuWidget() const
{
	return MainMenuWidget != nullptr;
}

UGameXXKMainMenuWidget* AGameXXKMVPHUD::GetMainMenuWidget() const
{
	return MainMenuWidget;
}

void AGameXXKMVPHUD::SetDebugPlayableShellEnabledForTest(bool bEnabled)
{
	bShowDebugPlayableShell = bEnabled;
}

bool AGameXXKMVPHUD::IsDebugPlayableShellEnabledForTest() const
{
	return bShowDebugPlayableShell;
}

bool AGameXXKMVPHUD::HasPlayableRootWidget() const
{
	return PlayableRootWidget != nullptr;
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::GetPlayableRootWidget() const
{
	return PlayableRootWidget;
}

bool AGameXXKMVPHUD::HasRouteMapWidget() const
{
	return RouteMapWidget != nullptr;
}

UGameXXKOneGameRouteMapWidget* AGameXXKMVPHUD::GetRouteMapWidget() const
{
	return RouteMapWidget;
}

bool AGameXXKMVPHUD::ShouldDrawLegacyRouteMapForTest() const
{
	return ShouldDrawLegacyRouteMap();
}

void AGameXXKMVPHUD::RefreshAuxiliaryWidgetsForTest()
{
	RefreshAuxiliaryWidgets();
}

bool AGameXXKMVPHUD::HasBattleBoardWidget() const
{
	return BattleBoardWidget != nullptr;
}

UGameXXKBattleBoardWidget* AGameXXKMVPHUD::GetBattleBoardWidget() const
{
	return BattleBoardWidget;
}

UGameXXKMainMenuWidget* AGameXXKMVPHUD::CreateMainMenuWidget()
{
	if (MainMenuWidget)
	{
		return MainMenuWidget;
	}

	TSubclassOf<UGameXXKMainMenuWidget> WidgetClass = MainMenuWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKMainMenuWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		MainMenuWidget = CreateWidget<UGameXXKMainMenuWidget>(PlayerController, WidgetClass);
	}
	if (!MainMenuWidget)
	{
		MainMenuWidget = NewObject<UGameXXKMainMenuWidget>(this, WidgetClass);
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		MainMenuWidget->RefreshFromState();
	}
	return MainMenuWidget;
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::CreatePlayableRootWidget()
{
	if (PlayableRootWidget)
	{
		return PlayableRootWidget;
	}

	TSubclassOf<UGameXXKPlayableRootWidget> WidgetClass = PlayableRootWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKPlayableRootWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayableRootWidget = CreateWidget<UGameXXKPlayableRootWidget>(PlayerController, WidgetClass);
	}
	if (!PlayableRootWidget)
	{
		PlayableRootWidget = NewObject<UGameXXKPlayableRootWidget>(this, WidgetClass);
	}
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		PlayableRootWidget->SetStartGameSlotForTest(StartGameSlotNameOverride, StartGameUserIndexOverride);
		PlayableRootWidget->RefreshFromState();
	}
	return PlayableRootWidget;
}

UGameXXKOneGameRouteMapWidget* AGameXXKMVPHUD::CreateRouteMapWidget()
{
	if (RouteMapWidget)
	{
		return RouteMapWidget;
	}

	TSubclassOf<UGameXXKOneGameRouteMapWidget> WidgetClass = RouteMapWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		RouteMapWidget = CreateWidget<UGameXXKOneGameRouteMapWidget>(PlayerController, WidgetClass);
	}
	if (!RouteMapWidget)
	{
		RouteMapWidget = NewObject<UGameXXKOneGameRouteMapWidget>(this, WidgetClass);
	}
	if (RouteMapWidget)
	{
		RouteMapWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		ConfigureRouteMapWidgetViewport(RouteMapWidget);
		RouteMapWidget->RefreshFromState();
	}
	return RouteMapWidget;
}

UGameXXKBattleBoardWidget* AGameXXKMVPHUD::CreateBattleBoardWidget()
{
	if (BattleBoardWidget)
	{
		return BattleBoardWidget;
	}

	TSubclassOf<UGameXXKBattleBoardWidget> WidgetClass = BattleBoardWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKBattleBoardWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		BattleBoardWidget = CreateWidget<UGameXXKBattleBoardWidget>(PlayerController, WidgetClass);
	}
	if (!BattleBoardWidget)
	{
		BattleBoardWidget = NewObject<UGameXXKBattleBoardWidget>(this, WidgetClass);
	}
	if (BattleBoardWidget)
	{
		BattleBoardWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		BattleBoardWidget->RefreshFromState();
	}
	return BattleBoardWidget;
}

void AGameXXKMVPHUD::RefreshMainMenuWidget()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RefreshFromState();
	}
}

void AGameXXKMVPHUD::RefreshRouteMapWidget()
{
	if (RouteMapWidget)
	{
		ConfigureRouteMapWidgetViewport(RouteMapWidget);
		RouteMapWidget->RefreshFromState();
	}
}

void AGameXXKMVPHUD::ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const
{
	if (!RouteWidget)
	{
		return;
	}

	RouteWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	RouteWidget->SetPositionInViewport(RouteMapViewportPosition, false);
	RouteWidget->SetDesiredSizeInViewport(RouteMapViewportSize);
	RouteWidget->SetRouteMapViewportGeometry(RouteMapViewportPosition, RouteMapViewportSize);
}

void AGameXXKMVPHUD::RefreshBattleBoardWidget()
{
	if (BattleBoardWidget)
	{
		BattleBoardWidget->RefreshFromState();
	}
}

void AGameXXKMVPHUD::RefreshAuxiliaryWidgets()
{
	RefreshRouteMapWidget();
	RefreshBattleBoardWidget();
}

TArray<FGameXXKMVPCommandDescriptor> AGameXXKMVPHUD::BuildVisibleCommands() const
{
	return GameXXKMVPCommandRouter::BuildVisibleCommands(ResolveMVPSubsystem(), StartGameSlotNameOverride, StartGameUserIndexOverride);
}

bool AGameXXKMVPHUD::HandleDemoCommand(FName CommandName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bSucceeded = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, CommandName, StartGameSlotNameOverride, StartGameUserIndexOverride);
	RefreshMainMenuWidget();
	if (PlayableRootWidget)
	{
		PlayableRootWidget->RefreshFromState();
	}
	RefreshRouteMapWidget();
	RefreshBattleBoardWidget();
	return bSucceeded;
}

FText AGameXXKMVPHUD::BuildStatusText() const
{
	return GameXXKMVPCommandRouter::BuildStatusText(ResolveMVPSubsystem());
}

void AGameXXKMVPHUD::DrawHUD()
{
	Super::DrawHUD();
	RefreshAuxiliaryWidgets();
	if (!Canvas)
	{
		return;
	}

	if (ShouldDrawLegacyRouteMap())
	{
		DrawRouteMap(ResolveMVPSubsystem());
	}

	if (!ShouldDrawDebugPlayableShell())
	{
		return;
	}

	const float PanelX = 24.0f;
	float CursorY = 24.0f;
	const float PanelWidth = 360.0f;
	const float ButtonHeight = 34.0f;
	const float Gap = 8.0f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.03f, 0.86f), PanelX - 12.0f, CursorY - 12.0f, PanelWidth + 24.0f, 520.0f);
	DrawText(TEXT("GameXXK MVP Playable Shell"), FColor::White, PanelX, CursorY, nullptr, 1.15f);
	CursorY += 34.0f;
	DrawText(BuildStatusText().ToString(), FColor(210, 220, 225), PanelX, CursorY, nullptr, 0.72f);
	CursorY += 42.0f;

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		const FGameXXKMVPCommandDescriptor& Command = Commands[Index];
		const FLinearColor Fill = Command.bEnabled ? FLinearColor(0.08f, 0.30f, 0.23f, 0.94f) : FLinearColor(0.16f, 0.16f, 0.16f, 0.82f);
		const FColor TextColor = Command.bEnabled ? FColor::White : FColor(150, 150, 150);
		DrawRect(Fill, PanelX, CursorY, PanelWidth, ButtonHeight);
		DrawText(Command.Label.ToString(), TextColor, PanelX + 12.0f, CursorY + 8.0f, nullptr, 0.86f);
		if (Command.bEnabled)
		{
			AddHitBox(FVector2D(PanelX, CursorY), FVector2D(PanelWidth, ButtonHeight), Command.CommandName, true, Index);
		}
		CursorY += ButtonHeight + Gap;
	}

}

bool AGameXXKMVPHUD::ShouldDrawDebugPlayableShell() const
{
	return bShowDebugPlayableShell;
}

bool AGameXXKMVPHUD::ShouldDrawLegacyRouteMap() const
{
	return bShowDebugPlayableShell && RouteMapWidget == nullptr;
}

void AGameXXKMVPHUD::DrawRouteMap(const UGameXXKMVPSubsystem* Subsystem)
{
	if (!Canvas || !Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::DungeonMap)
	{
		return;
	}

	const TArray<FGameXXKMVPRouteNodeDescriptor> RouteNodes = GameXXKMVPCommandRouter::BuildRouteMapNodes(Subsystem);
	if (RouteNodes.IsEmpty())
	{
		return;
	}

	const FVector2D Origin(460.0f, 150.0f);
	const FVector2D Size(FMath::Max(260.0f, Canvas->SizeX - Origin.X - 96.0f), FMath::Max(360.0f, Canvas->SizeY - Origin.Y - 96.0f));
	const float NodeSize = 44.0f;

	DrawText(TEXT("Route Map"), FColor::White, Origin.X, Origin.Y - 42.0f, nullptr, 1.08f);

	for (int32 Index = 1; Index < RouteNodes.Num(); ++Index)
	{
		const FVector2D Previous = GetRouteNodeCanvasPosition(RouteNodes[Index - 1].NormalizedPosition, Origin, Size);
		const FVector2D Current = GetRouteNodeCanvasPosition(RouteNodes[Index].NormalizedPosition, Origin, Size);
		DrawLine(Previous.X, Previous.Y, Current.X, Current.Y, FLinearColor(0.35f, 0.40f, 0.42f, 0.95f), 3.0f);
	}

	for (const FGameXXKMVPRouteNodeDescriptor& Node : RouteNodes)
	{
		const FVector2D Position = GetRouteNodeCanvasPosition(Node.NormalizedPosition, Origin, Size);
		const FVector2D TopLeft(Position.X - NodeSize * 0.5f, Position.Y - NodeSize * 0.5f);
		const FLinearColor Fill = Node.bEnabled
			? FLinearColor(0.10f, 0.48f, 0.36f, 0.96f)
			: FLinearColor(0.13f, 0.15f, 0.16f, 0.86f);
		const FColor LabelColor = Node.bEnabled ? FColor::White : FColor(150, 155, 158);

		DrawRect(FLinearColor(0.01f, 0.012f, 0.014f, 0.90f), TopLeft.X - 3.0f, TopLeft.Y - 3.0f, NodeSize + 6.0f, NodeSize + 6.0f);
		DrawRect(Fill, TopLeft.X, TopLeft.Y, NodeSize, NodeSize);
		DrawText(Node.Label.ToString(), LabelColor, Position.X + 34.0f, Position.Y - 9.0f, nullptr, 0.86f);

		if (Node.bEnabled)
		{
			AddHitBox(TopLeft, FVector2D(NodeSize, NodeSize), Node.CommandName, true, Node.NodeIndex + 100);
		}
	}
}

FVector2D AGameXXKMVPHUD::GetRouteNodeCanvasPosition(const FVector2D& NormalizedPosition, const FVector2D& Origin, const FVector2D& Size) const
{
	return FVector2D(
		Origin.X + NormalizedPosition.X * Size.X,
		Origin.Y + NormalizedPosition.Y * Size.Y);
}

void AGameXXKMVPHUD::NotifyHitBoxClick(FName BoxName)
{
	HandleDemoCommand(BoxName);
}
