#include "Misc/AutomationTest.h"

#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool AssertCloseGeometry(
		FAutomationTestBase& Test,
		UGameXXKDesktopTrainingWorkbenchWidget* Widget,
		const FName CloseName,
		const FVector4& PanelRect)
	{
		UButton* Close = Widget && Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(CloseName))
			: nullptr;
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s exists"), *CloseName.ToString()), Close))
		{
			return false;
		}
		const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Close->Slot);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s has canvas geometry"), *CloseName.ToString()), Slot))
		{
			return false;
		}
		const FVector2D Position = Slot->GetPosition();
		const FVector2D Size = Slot->GetSize();
		Test.TestTrue(*FString::Printf(TEXT("%s width"), *CloseName.ToString()),
			FMath::IsNearlyEqual(Size.X, 44.0f));
		Test.TestTrue(*FString::Printf(TEXT("%s height"), *CloseName.ToString()),
			FMath::IsNearlyEqual(Size.Y, 44.0f));
		Test.TestTrue(*FString::Printf(TEXT("%s uses the shared 14px top inset"), *CloseName.ToString()),
			FMath::IsNearlyEqual(Position.Y - PanelRect.Y, 14.0f));
		Test.TestTrue(*FString::Printf(TEXT("%s uses the shared 14px right inset"), *CloseName.ToString()),
			FMath::IsNearlyEqual(PanelRect.X + PanelRect.Z - Position.X - Size.X, 14.0f));
		const UObject* Resource = Close->GetStyle().Normal.GetResourceObject();
		Test.TestTrue(*FString::Printf(TEXT("%s uses CloseInk"), *CloseName.ToString()),
			Resource && Resource->GetPathName().Contains(TEXT("T_MasterV2_CloseInk")));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWorkbenchUnifiedCloseGeometryTest,
	"GameXXK.DesktopTraining.Layout.UnifiedPanelCloseGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWorkbenchUnifiedCloseGeometryTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("close-layout fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	Widget->OpenWorkbench();
	Widget->OpenBackpack();

	AssertCloseGeometry(*this, Widget, TEXT("BackpackPanelCloseButton"),
		GameXXKDesktopTrainingLayout::GetContentRect());
	Widget->HandleActionClicked(0);
	AssertCloseGeometry(*this, Widget, TEXT("WarehouseCloseButton"),
		GameXXKDesktopTrainingLayout::GetWarehouseRect());
	Widget->HandleActionClicked(1);
	AssertCloseGeometry(*this, Widget, TEXT("FormationCloseButton"),
		GameXXKDesktopTrainingLayout::GetContentRect());
	Widget->HandleActionClicked(2);
	AssertCloseGeometry(*this, Widget, TEXT("TalentsCloseButton"),
		GameXXKDesktopTrainingLayout::GetContentRect());
	Widget->HandleActionClicked(3);
	AssertCloseGeometry(*this, Widget, TEXT("ToolsCloseButton"),
		GameXXKDesktopTrainingLayout::GetRightShellRect());
	Widget->HandleActionClicked(4);
	AssertCloseGeometry(*this, Widget, TEXT("TrainingCloseButton"),
		GameXXKDesktopTrainingLayout::GetRightShellRect());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWorkbenchTrainingRoutePresentationTest,
	"GameXXK.DesktopTraining.Layout.TrainingRouteUsesConnectedRoundNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWorkbenchTrainingRoutePresentationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("training-route fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	Widget->OpenWorkbench();
	Widget->OpenBackpack();
	Widget->HandleActionClicked(4);

	for (int32 StageIndex = 0; StageIndex < 9; ++StageIndex)
	{
		const int32 StageNumber = StageIndex + 1;
		UButton* Node = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("TrainingNode_%d"), StageNumber)))
			: nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("stage %d owns a round node"), StageNumber), Node))
		{
			continue;
		}
		const UObject* Resource = Node->GetStyle().Normal.GetResourceObject();
		TestTrue(*FString::Printf(TEXT("stage %d uses the approved round route disc"), StageNumber),
			Resource && Resource->GetPathName().Contains(TEXT("T_MasterV2_NavDiscRoute")));
		const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Node->Slot);
		if (TestNotNull(*FString::Printf(TEXT("stage %d owns route geometry"), StageNumber), Slot))
		{
			TestTrue(*FString::Printf(TEXT("stage %d remains square"), StageNumber),
				FMath::IsNearlyEqual(Slot->GetSize().X, Slot->GetSize().Y));
			if (StageIndex > 0)
			{
				const int32 Row = StageIndex / 3;
				const int32 PreviousRow = (StageIndex - 1) / 3;
				UButton* PreviousNode = Cast<UButton>(Widget->WidgetTree->FindWidget(
					*FString::Printf(TEXT("TrainingNode_%d"), StageNumber - 1)));
				const UCanvasPanelSlot* PreviousSlot = PreviousNode
					? Cast<UCanvasPanelSlot>(PreviousNode->Slot)
					: nullptr;
				if (PreviousSlot && Row == PreviousRow)
				{
					const bool bEvenRow = Row % 2 == 0;
					TestTrue(*FString::Printf(TEXT("stage %d follows the snake direction"), StageNumber),
						bEvenRow
							? Slot->GetPosition().X > PreviousSlot->GetPosition().X
							: Slot->GetPosition().X < PreviousSlot->GetPosition().X);
				}
			}
		}
	}
	for (int32 LineIndex = 0; LineIndex < 8; ++LineIndex)
	{
		TestNotNull(*FString::Printf(TEXT("route connection %d exists behind nodes"), LineIndex),
			Widget->WidgetTree
				? Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("TrainingPathLine_%d"), LineIndex))
				: nullptr);
	}
	UTextBlock* CurrentStage = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TrainingCurrentStageText")))
		: nullptr;
	TestTrue(TEXT("current stage uses a player-facing label instead of a raw ID"),
		CurrentStage
			&& CurrentStage->GetText().ToString().Contains(TEXT("普通 1-1"))
			&& !CurrentStage->GetText().ToString().Contains(TEXT("Training.Normal")));
	return true;
}

#endif
