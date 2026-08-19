#include "GameXXKTrainingRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString GetButtonNormalResourcePath(const UButton* Button)
	{
		const UObject* Resource = Button ? Button->GetStyle().Normal.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	FString GetBorderResourcePath(const UBorder* Border)
	{
		const UObject* Resource = Border ? Border->Background.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest,
	"GameXXK.DesktopTraining.Workbench.SlateBuildContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the Slate build contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	TestNotNull(TEXT("workbench creates a WidgetTree root before Slate paints"), Widget->WidgetTree ? Widget->WidgetTree->RootWidget.Get() : nullptr);
	TestTrue(TEXT("workbench TakeWidget is not the null Slate placeholder"), SlateWidget != SNullWidget::NullWidget);
	UScaleBox* ScaleRoot = Widget->WidgetTree ? Cast<UScaleBox>(Widget->WidgetTree->RootWidget) : nullptr;
	TestNotNull(TEXT("workbench root is a uniform ScaleBox"), ScaleRoot);
	TestTrue(TEXT("workbench root uses ScaleToFit"), ScaleRoot && ScaleRoot->GetStretch() == EStretch::ScaleToFit);
	USizeBox* ReferenceBox = ScaleRoot ? Cast<USizeBox>(ScaleRoot->GetContent()) : nullptr;
	TestNotNull(TEXT("ScaleBox owns the fixed reference SizeBox"), ReferenceBox);
	TestTrue(TEXT("reference width is 1672"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetWidthOverride(), 1672.0f));
	TestTrue(TEXT("reference height is 941"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetHeightOverride(), 941.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest,
	"GameXXK.DesktopTraining.Workbench.NativeConstructDoesNotRebuildSlateTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the native construct lifecycle contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->TakeWidget();
	Widget->ConstructForTest();
	TestEqual(TEXT("NativeConstruct leaves the Slate tree built by RebuildWidget intact"),
		Widget->GetProgrammaticLayoutBuildCountForTest(),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest,
	"GameXXK.DesktopTraining.Workbench.MasterV2ResourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("resource contract widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	const TArray<FString> ResourcePaths = Widget->GetMasterV2ResourcePathsForTest();

	int32 ApprovedResourceCount = 0;
	bool bHasPanelLarge = false;
	bool bHasItemSlot = false;
	bool bHasEquipmentSlot = false;
	bool bHasHeroFullBody = false;
	bool bHasCloseInk = false;
	bool bHasIngot = false;
	bool bHasNeutralButton = false;
	bool bHasPrimaryButton = false;
	bool bHasDangerButton = false;
	bool bHasCharacterTabNormal = false;
	bool bHasCharacterTabSelected = false;
	int32 NavDiscCount = 0;
	for (const FString& Path : ResourcePaths)
	{
		if (Path.Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/")))
		{
			++ApprovedResourceCount;
			bHasPanelLarge |= Path.Contains(TEXT("T_MasterV2_PanelLarge"));
			bHasItemSlot |= Path.Contains(TEXT("T_MasterV2_ItemSlot"));
			bHasEquipmentSlot |= Path.Contains(TEXT("T_MasterV2_EquipmentSlot"));
			bHasHeroFullBody |= Path.Contains(TEXT("T_MasterV2_HeroFullBody"));
			bHasCloseInk |= Path.Contains(TEXT("T_MasterV2_CloseInk"));
			bHasIngot |= Path.Contains(TEXT("T_MasterV2_Ingot"));
			bHasNeutralButton |= Path.Contains(TEXT("T_MasterV2_ButtonNeutral"));
			bHasPrimaryButton |= Path.Contains(TEXT("T_MasterV2_ButtonPrimary"));
			bHasDangerButton |= Path.Contains(TEXT("T_MasterV2_ButtonDanger"));
			bHasCharacterTabNormal |= Path.Contains(TEXT("003_tab_1"));
			bHasCharacterTabSelected |= Path.Contains(TEXT("004_tab_2"));
			NavDiscCount += Path.Contains(TEXT("T_MasterV2_NavDisc")) ? 1 : 0;
		}
	}
	TestTrue(TEXT("workbench uses approved MasterV2 brush resources"), ApprovedResourceCount >= 3);
	TestTrue(TEXT("workbench uses the approved large panel texture"), bHasPanelLarge);
	TestTrue(TEXT("workbench uses the approved item slot texture"), bHasItemSlot);
	TestTrue(TEXT("workbench uses the approved equipment slot texture"), bHasEquipmentSlot);
	TestTrue(TEXT("workbench reuses the approved PSD backpack hero"), bHasHeroFullBody);
	TestTrue(TEXT("workbench reuses the approved PSD close ink"), bHasCloseInk);
	TestTrue(TEXT("workbench reuses the approved PSD ingot"), bHasIngot);
	TestTrue(TEXT("workbench exposes the approved neutral button family"), bHasNeutralButton);
	TestTrue(TEXT("workbench exposes the approved primary button family"), bHasPrimaryButton);
	TestTrue(TEXT("workbench exposes the approved danger button family"), bHasDangerButton);
	TestTrue(TEXT("workbench reuses the approved normal character tab"), bHasCharacterTabNormal);
	TestTrue(TEXT("workbench reuses the approved selected character tab"), bHasCharacterTabSelected);
	TestEqual(TEXT("workbench no longer advertises legacy MasterV2 navigation discs"), NavDiscCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchImageTruthNavigationBindingTest,
	"GameXXK.DesktopTraining.Workbench.ImageTruthNavigationBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchImageTruthNavigationBindingTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("image-truth navigation fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TArray<FString> Paths = Widget->GetBottomNavigationIconResourcePathsForTest();
	TestEqual(TEXT("five bottom navigation icons are bound from the image truth set"), Paths.Num(), 5);
	for (const FString& Path : Paths)
	{
		TestTrue(
			TEXT("bottom navigation icon path stays inside ImageTruth/Training"),
			Path.StartsWith(TEXT("/Game/GameXXK/UI/ImageTruth/Training/")));
	}
	TestTrue(TEXT("warehouse truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Warehouse).Contains(TEXT("T_TrainingNavWarehouse")));
	TestTrue(TEXT("formation truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Formation).Contains(TEXT("T_TrainingNavFormation")));
	TestTrue(TEXT("talents truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Talents).Contains(TEXT("T_TrainingNavTalents")));
	TestTrue(TEXT("tools truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Tools).Contains(TEXT("T_TrainingNavTools")));
	TestTrue(TEXT("training truth glyph is selected"),
		Widget->GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav::Training).Contains(TEXT("T_TrainingNavTraining")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchApprovedControlBindingTest,
	"GameXXK.DesktopTraining.Workbench.ApprovedControlBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchApprovedControlBindingTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("approved-control fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("approved-control workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("approved-control fixture expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestApprovedButton = [this, Widget](
		const FName WidgetName,
		const TCHAR* ExpectedResourceToken,
		const bool bMustBeImageBrush)
	{
		UButton* Button = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Button);
		if (!Button)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s binds the approved PSD resource %s"), *WidgetName.ToString(), ExpectedResourceToken),
			GetButtonNormalResourcePath(Button).Contains(ExpectedResourceToken));
		TestEqual(
			*FString::Printf(TEXT("%s does not dark-color multiply approved art"), *WidgetName.ToString()),
			Button->GetBackgroundColor(),
			FLinearColor::White);
		if (bMustBeImageBrush)
		{
			TestTrue(
				*FString::Printf(TEXT("%s preserves its non-stretch image brush"), *WidgetName.ToString()),
				Button->GetStyle().Normal.DrawAs == ESlateBrushDrawType::Image);
		}
	};

	UWidget* CollectButton = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TravelCollectButton"))
		: nullptr;
	TestNull(TEXT("travel strip has no harvest/collect button"), CollectButton);
	TestApprovedButton(TEXT("TravelRetryButton"), TEXT("T_MasterV2_ButtonDanger"), false);
	TestApprovedButton(TEXT("TrainingDifficultyTab_0"), TEXT("T_MasterV2_TabSelected"), false);
	TestApprovedButton(TEXT("TrainingDifficultyTab_1"), TEXT("T_MasterV2_TabNormal"), false);
	TestApprovedButton(TEXT("TrainingNode_1"), TEXT("T_MasterV2_NavRoute"), true);
	TestApprovedButton(TEXT("TrainingChallengeButton"), TEXT("T_MasterV2_ButtonDanger"), false);
	TestApprovedButton(TEXT("TrainingTravelButton"), TEXT("T_MasterV2_ButtonPrimary"), false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchApprovedSecondaryControlBindingTest,
	"GameXXK.DesktopTraining.Workbench.ApprovedSecondaryControlBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchApprovedSecondaryControlBindingTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("secondary-control fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("secondary-control workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("secondary-control fixture expands the backpack"), Widget->OpenBackpack());
	Widget->TakeWidget();

	const auto TestApprovedButton = [this, Widget](const FName WidgetName, const TCHAR* ExpectedResourceToken)
	{
		UButton* Button = Widget->WidgetTree
			? Cast<UButton>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Button);
		if (!Button)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s binds %s"), *WidgetName.ToString(), ExpectedResourceToken),
			GetButtonNormalResourcePath(Button).Contains(ExpectedResourceToken));
		TestEqual(
			*FString::Printf(TEXT("%s keeps the approved source color"), *WidgetName.ToString()),
			Button->GetBackgroundColor(),
			FLinearColor::White);
	};

	Widget->HandleActionClicked(3);
	TestApprovedButton(TEXT("ToolButton_0"), TEXT("T_MasterV2_TabSelected"));
	TestApprovedButton(TEXT("ToolButton_1"), TEXT("T_MasterV2_TabNormal"));
	Widget->HandleActionClicked(4);

	const FName FirstUnclearedStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("fixture selects the first uncleared stage"), Widget->SelectStageForTest(FirstUnclearedStage));
	TestTrue(TEXT("fixture enters challenge viewport"), Widget->ClickChallengeForTest());
	TestApprovedButton(TEXT("ChallengeAutoButton"), TEXT("T_MasterV2_ButtonPrimary"));
	TestApprovedButton(TEXT("ChallengeAdvanceButton"), TEXT("T_MasterV2_ButtonDanger"));

	UBorder* ReadOnlyNode = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TrainingNodeReadOnly_1")))
		: nullptr;
	TestNotNull(TEXT("challenge keeps the read-only training node"), ReadOnlyNode);
	if (ReadOnlyNode)
	{
		TestTrue(TEXT("read-only node binds the approved route-node art"),
			GetBorderResourcePath(ReadOnlyNode).Contains(TEXT("T_MasterV2_NavRoute")));
		TestTrue(TEXT("read-only node preserves a non-stretch image brush"),
			ReadOnlyNode->Background.DrawAs == ESlateBrushDrawType::Image);
		TestEqual(TEXT("read-only node does not dark-color multiply approved art"),
			ReadOnlyNode->GetBrushColor(), FLinearColor::White);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingReferenceGeometryTest,
	"GameXXK.DesktopTraining.Workbench.ReferenceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingReferenceGeometryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestEqual(TEXT("reference canvas is the approved UI Master size"), GetReferenceCanvasSize(), FVector2D(1672.0f, 941.0f));
	TestEqual(TEXT("warehouse matches the selected layout"), GetWarehouseRect(), FVector4(10.0f, 17.0f, 363.0f, 908.0f));
	TestEqual(TEXT("center shell matches the selected layout"), GetCenterShellRect(), FVector4(386.0f, 17.0f, 970.0f, 908.0f));
	TestEqual(TEXT("right shell matches the selected layout"), GetRightShellRect(), FVector4(1369.0f, 17.0f, 291.0f, 908.0f));
	TestEqual(TEXT("idle strip matches the selected layout"), GetIdleStripRect(), FVector4(394.0f, 21.0f, 953.0f, 202.0f));
	TestEqual(TEXT("backpack surface matches the selected layout"), GetContentRect(), FVector4(397.0f, 244.0f, 945.0f, 533.0f));
	TestEqual(TEXT("navigation matches the selected layout"), GetNavigationRect(), FVector4(397.0f, 788.0f, 945.0f, 137.0f));

	const FFitTransform FullHD = MakeFitTransform(FVector2D(1920.0f, 1080.0f));
	const FFitTransform QHD = MakeFitTransform(FVector2D(2560.0f, 1440.0f));
	TestTrue(TEXT("Full HD uses one uniform scale"), FMath::IsNearlyEqual(FullHD.Scale, 1080.0f / 941.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD uses one uniform scale"), FMath::IsNearlyEqual(QHD.Scale, 1440.0f / 941.0f, KINDA_SMALL_NUMBER));
	const FVector4 FullHDNode = FullHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	const FVector4 QHDNode = QHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	TestTrue(TEXT("Full HD nodes remain circular"), FMath::IsNearlyEqual(FullHDNode.Z, FullHDNode.W, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD nodes remain circular"), FMath::IsNearlyEqual(QHDNode.Z, QHDNode.W, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchStructuralGeometryTest,
	"GameXXK.DesktopTraining.Workbench.StructuralGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchStructuralGeometryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("structural geometry subsystem exists"), Subsystem) || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("structural geometry widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("structural geometry expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestNamedRect = [this, Widget](const FName WidgetName, const FVector4& Expected)
	{
		const FString Name = WidgetName.ToString();
		UWidget* Child = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(WidgetName) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Child);
		const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s is placed on the reference canvas"), *Name), Slot);
		if (Slot)
		{
			TestEqual(*FString::Printf(TEXT("%s position"), *Name), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
			TestEqual(*FString::Printf(TEXT("%s size"), *Name), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
		}
	};

	TestNamedRect(TEXT("WarehousePanel"), GameXXKDesktopTrainingLayout::GetWarehouseRect());
	TestNamedRect(TEXT("CenterWorkbenchFrame"), GameXXKDesktopTrainingLayout::GetCenterShellRect());
	TestNamedRect(TEXT("TrainingTravelStrip"), GameXXKDesktopTrainingLayout::GetIdleStripRect());
	TestNamedRect(TEXT("BackpackPanel"), GameXXKDesktopTrainingLayout::GetContentRect());
	TestNamedRect(TEXT("TrainingMapPanel"), GameXXKDesktopTrainingLayout::GetRightShellRect());
	TestNamedRect(TEXT("BottomNavigationPanel"), GameXXKDesktopTrainingLayout::GetNavigationRect());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTransparentDesktopPlacementTest,
	"GameXXK.DesktopTraining.Workbench.TransparentDesktopPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTransparentDesktopPlacementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("transparent-placement subsystem exists"), Subsystem) || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("transparent-placement workbench exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("transparent-placement expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();

	const auto TestTransparentSurface = [this, Widget](const FName WidgetName)
	{
		UBorder* Surface = Widget->WidgetTree
			? Cast<UBorder>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *WidgetName.ToString()), Surface);
		if (!Surface)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s does not paint a backing surface"), *WidgetName.ToString()),
			Surface->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
		TestNull(
			*FString::Printf(TEXT("%s has no backing texture resource"), *WidgetName.ToString()),
			Surface->Background.GetResourceObject());
		TestTrue(
			*FString::Printf(TEXT("%s remains fully transparent"), *WidgetName.ToString()),
			FMath::IsNearlyZero(Surface->GetBrushColor().A));
	};

	const auto TestFunctionalSurface = [this, Widget](const FName WidgetName)
	{
		UBorder* Surface = Widget->WidgetTree
			? Cast<UBorder>(Widget->WidgetTree->FindWidget(WidgetName))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("%s functional surface exists"), *WidgetName.ToString()), Surface);
		if (!Surface)
		{
			return;
		}
		TestTrue(
			*FString::Printf(TEXT("%s keeps the approved panel art"), *WidgetName.ToString()),
			GetBorderResourcePath(Surface).Contains(TEXT("T_MasterV2_PanelLarge")));
	};

	TestTransparentSurface(TEXT("WorkbenchBackground"));
	TestTransparentSurface(TEXT("CenterWorkbenchFrame"));
	TestTransparentSurface(TEXT("BottomNavigationPanel"));
	TestFunctionalSurface(TEXT("WarehousePanel"));
	TestTransparentSurface(TEXT("TrainingTravelStrip"));
	TestFunctionalSurface(TEXT("TrainingMapPanel"));
	TestNotNull(
		TEXT("the functional backpack remains the approved embedded inventory"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchInnerGeometryTest,
	"GameXXK.DesktopTraining.Workbench.InnerGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchInnerGeometryTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("inner geometry subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("inner geometry widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("inner geometry expands the backpack"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4);
	Widget->TakeWidget();
	TestEqual(TEXT("warehouse exposes nine visible rows"), Widget->GetWarehouseRowCountForTest(), 9);

	const auto TestNamedRect = [this, Widget](const FName WidgetName, const FVector4& Expected)
	{
		const FString Name = WidgetName.ToString();
		UWidget* Child = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(WidgetName) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Child);
		const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
		TestNotNull(*FString::Printf(TEXT("%s is on the reference canvas"), *Name), Slot);
		if (Slot)
		{
			TestEqual(*FString::Printf(TEXT("%s position"), *Name), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
			TestEqual(*FString::Printf(TEXT("%s size"), *Name), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
		}
	};

	TestNamedRect(TEXT("WarehouseSlot_0"), FVector4(30.0f, 142.0f, 68.0f, 68.0f));
	TestNamedRect(TEXT("EmbeddedApprovedBackpack"), FVector4(-311.0f, -173.0f, 1920.0f, 1080.0f));
	TestNamedRect(TEXT("BackpackGoldIcon"), FVector4(1098.0f, 263.0f, 30.0f, 30.0f));
	TestNamedRect(TEXT("TrainingNode_1"), FVector4(1390.0f, 158.0f, 58.0f, 58.0f));
	TestNamedRect(TEXT("BottomNavigationButton_0"), FVector4(421.0f, 800.0f, 151.0f, 112.0f));

	UButton* NavigationButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BottomNavigationButton_0")))
		: nullptr;
	TestNotNull(TEXT("bottom navigation button exists for PSD tint contract"), NavigationButton);
	TestEqual(TEXT("approved navigation art is not dark-color multiplied"),
		NavigationButton ? NavigationButton->GetBackgroundColor() : FLinearColor::Black,
		FLinearColor::White);

	UGameXXKInventoryWindowWidget* EmbeddedBackpack = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	TestNotNull(TEXT("center panel embeds the approved PSD backpack widget"), EmbeddedBackpack);
	if (EmbeddedBackpack)
	{
		TestTrue(TEXT("approved backpack runs in desktop-training embedded mode"), EmbeddedBackpack->IsDesktopTrainingEmbeddedModeForTest());
		TestEqual(TEXT("embedded backpack keeps the approved four-column grid"), EmbeddedBackpack->GetBackpackColumnCountForTest(), 4);
		TestEqual(TEXT("embedded backpack keeps twenty visible PSD slots"), EmbeddedBackpack->GetBackpackSlotCountForTest(), 20);
		TestEqual(TEXT("embedded backpack keeps six approved equipment slots"), EmbeddedBackpack->GetEquipmentSlotCountForTest(), 6);

		const auto TestEmbeddedRect = [this, EmbeddedBackpack](const FName WidgetName, const FVector4& Expected)
		{
			UWidget* Child = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(WidgetName) : nullptr;
			TestNotNull(*FString::Printf(TEXT("embedded %s exists"), *WidgetName.ToString()), Child);
			const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
			TestNotNull(*FString::Printf(TEXT("embedded %s keeps PSD canvas placement"), *WidgetName.ToString()), Slot);
			if (Slot)
			{
				TestEqual(*FString::Printf(TEXT("embedded %s position"), *WidgetName.ToString()), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
				TestEqual(*FString::Printf(TEXT("embedded %s size"), *WidgetName.ToString()), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
			}
		};
		TestEmbeddedRect(TEXT("InventoryCentralHeroIdle"), FVector4(478.0f, 304.0f, 518.0f, 518.0f));
		TestEmbeddedRect(TEXT("InventoryEquipmentSlot_Weapon"), FVector4(420.0f, 340.0f, 118.0f, 124.0f));
		TestEmbeddedRect(TEXT("InventoryCharacterTab_0"), FVector4(514.0f, 220.0f, 105.0f, 62.0f));
		UWidget* RemovedTalentTab = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_3")) : nullptr;
		UWidget* RemovedTitleTab = EmbeddedBackpack->WidgetTree ? EmbeddedBackpack->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_4")) : nullptr;
		TestTrue(TEXT("embedded mode removes the top talent tab"), RemovedTalentTab && RemovedTalentTab->GetVisibility() == ESlateVisibility::Collapsed);
		TestTrue(TEXT("embedded mode removes the top title tab"), RemovedTitleTab && RemovedTitleTab->GetVisibility() == ESlateVisibility::Collapsed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLayoutContractTest,
	"GameXXK.DesktopTraining.Workbench.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLayoutContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget can be constructed without a live viewport"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("warehouse uses four columns"), Widget->GetWarehouseColumnCountForTest(), 4);
	TestEqual(TEXT("warehouse uses nine visible rows"), Widget->GetWarehouseRowCountForTest(), 9);
	const FVector2D BackpackRatio = Widget->GetBackpackAspectRatioForTest();
	TestTrue(TEXT("backpack aspect ratio keeps the real wide proportion"), FMath::IsNearlyEqual(BackpackRatio.X / BackpackRatio.Y, 1.76f, 0.001f));

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("workbench read model fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.PlayerGold = 4242;
	State.Inventory.Empty();
	State.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 3);
	State.Inventory.Add(UGameXXKMVPRules::ItemTrainingNormalChest(), 2);
	for (int32 ExtraIndex = 0; ExtraIndex < 31; ++ExtraIndex)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::Starter;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1 + (ExtraIndex % FGameXXKEquipmentRules::MaxItemLevel);
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		FString Error;
		TestTrue(TEXT("warehouse pagination fixture creates an equipment instance"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error));
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("desktop workbench opens as the persistent idle strip"), Widget->OpenWorkbench());
	TestFalse(TEXT("desktop workbench starts with the backpack collapsed"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("collapsed workbench still owns the live Travel strip"), Widget->HasTravelVisualStripForTest());
	TestNotNull(TEXT("collapsed workbench exposes the down-arrow Tab control"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton")) : nullptr);
	TestTrue(TEXT("Tab/backpack entry opens the formation-backed backpack view"), Widget->OpenBackpack());
	TestTrue(TEXT("opening backpack expands the center surface"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("expanded backpack exposes five small top-toolbar controls"), Widget->GetTopToolbarButtonCountForTest(), 5);
	TestEqual(TEXT("topmost toolbar uses the confirmed black pushpin truth asset"),
		Widget->GetTopToolbarAlwaysOnTopResourcePathForTest(),
		FString(TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTop.T_TrainingTopToolbarAlwaysOnTop")));
	TestEqual(TEXT("topmost toolbar exposes the confirmed gray disabled pushpin truth asset"),
		Widget->GetTopToolbarAlwaysOnTopOffResourcePathForTest(),
		FString(TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTopOffGray.T_TrainingTopToolbarAlwaysOnTopOffGray")));
	TestFalse(TEXT("warehouse is not forced open with the backpack"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("right-side pages are not forced open with the backpack"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("backpack defaults to the hero character"),
		Widget->GetActiveBackpackCharacterIdForTest(),
		FGameXXKEquipmentRules::HeroCharacterId());
	const TArray<FName> BackpackCharacterIds = Widget->GetBackpackCharacterIdsForTest();
	TestEqual(TEXT("backpack exposes the fixed hero and all six starter-role companions"), BackpackCharacterIds.Num(), 7);
	TestTrue(TEXT("backpack character list keeps the hero first"),
		BackpackCharacterIds.Num() > 0
		&& BackpackCharacterIds[0] == FGameXXKEquipmentRules::HeroCharacterId());
	if (BackpackCharacterIds.Num() > 1)
	{
		TestTrue(TEXT("backpack can switch to a permanent companion"), Widget->SelectBackpackCharacterForTest(BackpackCharacterIds[1]));
		TestEqual(TEXT("selected companion becomes the backpack read-model owner"),
			Widget->GetActiveBackpackCharacterIdForTest(),
			BackpackCharacterIds[1]);
	}
	TestFalse(TEXT("backpack rejects an unknown character"), Widget->SelectBackpackCharacterForTest(FName(TEXT("Character.Unknown"))));
	Widget->HandleActionClicked(3);
	TestEqual(TEXT("tools navigation replaces the right-side map"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Tools);
	TestTrue(TEXT("tools panel is active outside challenge viewport"), Widget->IsToolsPanelActiveForTest());
	TestTrue(TEXT("tools navigation opens the right-side panel"), Widget->IsRightPanelOpenForTest());
	TestEqual(TEXT("tools panel exposes a fixed three-by-three input grid"), Widget->GetToolSlotCountForTest(), 9);
	TestEqual(TEXT("tools panel exposes five unified modes"), Widget->GetToolModeCountForTest(), 5);
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("training navigation returns to the map shell"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Training);
	TestFalse(TEXT("training navigation is not the tools panel"), Widget->IsToolsPanelActiveForTest());
	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse navigation independently opens the left panel"), Widget->IsWarehousePanelOpenForTest());
	TestEqual(TEXT("new warehouse partition starts on one empty page"), Widget->GetWarehousePageCountForTest(), 1);
	TestEqual(TEXT("warehouse starts on its first page"), Widget->GetWarehousePageIndexForTest(), 0);
	TestEqual(TEXT("new warehouse partition has no duplicated backpack equipment"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 0);
	TestEqual(TEXT("workbench reads the authoritative runtime gold"), Widget->GetRuntimeGoldForTest(), 4242);
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel fixture starts the default cleared stage"), Subsystem->StartTrainingTravel(TravelStage));
	FGameXXKTrainingOfflineReward SimulatedTravelReward;
	TestTrue(TEXT("travel fixture creates a pending full-health offline reward"),
		Subsystem->SimulateTrainingTravelOffline(512, SimulatedTravelReward));
	TestTrue(TEXT("workbench exposes pending travel gold for collection"), Widget->GetPendingTravelGoldForTest() > 0);
	TestEqual(TEXT("workbench exposes pending normal travel chests"),
		Widget->GetPendingTravelNormalChestCountForTest(), SimulatedTravelReward.NormalChestCount);
	TestEqual(TEXT("workbench exposes pending advanced travel chests"),
		Widget->GetPendingTravelAdvancedChestCountForTest(), SimulatedTravelReward.AdvancedChestCount);
	const int32 GoldBeforeCollect = Widget->GetRuntimeGoldForTest();
	TestTrue(TEXT("workbench collect action deposits pending travel rewards"), Widget->CollectTravelRewardsForTest());
	TestEqual(TEXT("collect action deposits pending travel gold"),
		Widget->GetRuntimeGoldForTest(), GoldBeforeCollect + SimulatedTravelReward.Gold);
	TestEqual(TEXT("collect action clears pending travel gold"), Widget->GetPendingTravelGoldForTest(), 0);
	TestEqual(TEXT("workbench warehouse occupancy comes from the persisted warehouse partition"),
		Widget->GetWarehouseOccupancyForTest(),
		State.DesktopInventory.WarehouseEquipmentInstanceIds.Num());
	const TArray<FName> VisibleItems = Widget->GetVisibleBackpackItemIdsForTest();
	TestTrue(TEXT("workbench backpack read model includes healing powder"), VisibleItems.Contains(UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("workbench backpack read model includes a travel chest"), VisibleItems.Contains(UGameXXKMVPRules::ItemTrainingNormalChest()));
	TestEqual(TEXT("three difficulty bands each expose nine stage definitions"), FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestEqual(TEXT("normal 1-1 id remains stable"), FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), FName(TEXT("Training.Normal.1-1")));
	const FName ChallengeStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("challenge fixture selects the first uncompleted stage"), Widget->SelectStageForTest(ChallengeStage));
	TestTrue(TEXT("challenge fixture enters the enlarged challenge viewport"), Widget->ClickChallengeForTest());
	TestTrue(TEXT("challenge refresh keeps the workbench shell visible"), Widget->IsWorkbenchVisibleForTest());
	TestTrue(TEXT("challenge keeps warehouse and map shells read-only"), Widget->AreChallengeSidePanelsReadOnlyForTest());
	TestTrue(TEXT("challenge viewport exposes active auto-battle control"), Widget->IsAutoBattleVisibleForTest());
	TestFalse(TEXT("challenge viewport does not expose travel retry control"), Widget->IsRetryVisibleForTest());
	const FVector4 ChallengeViewportRect = Widget->GetChallengeViewportRectForTest();
	const FVector4 ChallengeCombatStripRect = Widget->GetChallengeCombatStripRectForTest();
	const FVector4 ChallengeBattleBoardRect = Widget->GetChallengeBattleBoardRectForTest();
	TestTrue(TEXT("challenge uses the full continuous center canvas"),
		FMath::IsNearlyEqual(ChallengeViewportRect.Z, 960.0f)
		&& FMath::IsNearlyEqual(ChallengeViewportRect.W, 968.0f));
	TestEqual(TEXT("challenge combat strip reserves three enemy and three party slots"),
		Widget->GetChallengeCombatSlotCountForTest(), 6);
	TestTrue(TEXT("challenge combat strip precedes the battle board inside the center canvas"),
		ChallengeCombatStripRect.Y + ChallengeCombatStripRect.W <= ChallengeBattleBoardRect.Y
		&& ChallengeBattleBoardRect.Z >= 710.0f
		&& ChallengeBattleBoardRect.W >= 535.0f);
	const EGameXXKDesktopTrainingNav NavDuringChallenge = Widget->GetActiveNavForTest();
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("challenge locks bottom navigation while side shells remain visible"), Widget->GetActiveNavForTest(), NavDuringChallenge);
	TestTrue(TEXT("backpack entry returns to the workbench after challenge shell assertion"), Widget->OpenBackpack());
	Widget->HandleActionClicked(14);
	TestTrue(TEXT("topmost toolbar action toggles its state"), Widget->IsAlwaysOnTopForTest());
	Widget->HandleActionClicked(17);
	TestTrue(TEXT("mute toolbar action toggles its state"), Widget->IsMutedForTest());
	Widget->HandleActionClicked(15);
	TestTrue(TEXT("exit toolbar opens confirmation instead of closing immediately"), Widget->IsExitConfirmationOpenForTest());
	TestTrue(TEXT("exit confirmation keeps the idle workbench visible"), Widget->IsWorkbenchVisibleForTest());
	TestTrue(TEXT("cancelling exit confirmation succeeds"), Widget->CancelExitForTest());
	Widget->HandleActionClicked(60);
	TestFalse(TEXT("backpack collapse keeps the workbench alive"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("backpack collapse keeps the idle strip visible"), Widget->IsWorkbenchVisibleForTest() && Widget->HasTravelVisualStripForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingCollapsedResourceHibernateTest,
	"GameXXK.DesktopTraining.Workbench.CollapsedResourceHibernate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingCollapsedResourceHibernateTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("hibernation fixture starts the owned roster"), Subsystem->StartGame());
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("hibernation fixture starts travel"), Subsystem->StartTrainingTravel(TravelStage));

	UGameXXKDesktopTrainingWorkbenchWidget* Workbench = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("hibernation fixture opens the workbench"), Workbench->OpenWorkbench());
	TestTrue(TEXT("hibernation fixture expands the backpack"), Workbench->OpenBackpack());
	Workbench->HandleActionClicked(3);
	Workbench->HandleActionClicked(0);
	TestTrue(TEXT("fixture has warehouse open"), Workbench->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("fixture has tools open"), Workbench->IsToolsPanelActiveForTest());

	const TArray<FName> NpcIds = Workbench->GetNpcCharacterIdsForTest();
	TestTrue(TEXT("fixture exposes a configurable NPC"), NpcIds.Num() > 0);
	if (NpcIds.IsEmpty())
	{
		return false;
	}
	TestTrue(TEXT("fixture selects an NPC backpack"), Workbench->SelectBackpackCharacterForTest(NpcIds[0]));
	UGameXXKInventoryWindowWidget* Embedded = Workbench->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	TestNotNull(TEXT("expanded workbench owns one embedded inventory"), Embedded);
	if (!Embedded)
	{
		return false;
	}
	TestTrue(TEXT("embedded deck tab opens before collapse"),
		Embedded->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));
	const TArray<FName> PendingDeckBeforeCollapse = Embedded->GetPendingHeroDeckIdsForTest();

	const int32 TravelTickBeforeCollapse = Workbench->GetTravelVisualNativeTickCountForTest();
	Workbench->HandleActionClicked(60);
	TestFalse(TEXT("collapse hides the backpack"), Workbench->IsBackpackExpandedForTest());
	TestTrue(TEXT("collapse schedules a delayed release"), Workbench->IsCollapsedResourceUnloadPendingForTest());
	TestFalse(TEXT("collapse does not mark resources released immediately"), Workbench->AreCollapsedResourcesReleasedForTest());
	TestTrue(TEXT("collapse keeps the top travel strip"), Workbench->HasTravelVisualStripForTest());

	Workbench->TickForTest(2.9f);
	TestTrue(TEXT("release is still pending at 2.9 seconds"), Workbench->IsCollapsedResourceUnloadPendingForTest());
	TestFalse(TEXT("resources remain unreleased at 2.9 seconds"), Workbench->AreCollapsedResourcesReleasedForTest());
	TestEqual(TEXT("no GC request occurs before the deadline"), Workbench->GetCollapsedGcRequestCountForTest(), 0);
	TestTrue(TEXT("travel keeps ticking while collapsed"),
		Workbench->GetTravelVisualNativeTickCountForTest() > TravelTickBeforeCollapse);

	Workbench->TickForTest(0.1f);
	TestFalse(TEXT("release is no longer pending at 3.0 seconds"), Workbench->IsCollapsedResourceUnloadPendingForTest());
	TestTrue(TEXT("resources are released at 3.0 seconds"), Workbench->AreCollapsedResourcesReleasedForTest());
	TestEqual(TEXT("the deadline requests GC exactly once"), Workbench->GetCollapsedGcRequestCountForTest(), 1);
	Workbench->TickForTest(1.0f);
	TestEqual(TEXT("later collapsed ticks do not request another GC"), Workbench->GetCollapsedGcRequestCountForTest(), 1);
	TestTrue(TEXT("top travel strip survives resource release"), Workbench->HasTravelVisualStripForTest());

	Workbench->HandleActionClicked(60);
	TestTrue(TEXT("cold reopen expands the backpack"), Workbench->IsBackpackExpandedForTest());
	TestEqual(TEXT("cold reopen creates one embedded inventory"), Workbench->GetEmbeddedInventoryWidgetCountForTest(), 1);
	TestTrue(TEXT("cold reopen restores the warehouse page"), Workbench->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("cold reopen restores the tools page"), Workbench->IsToolsPanelActiveForTest());
	TestEqual(TEXT("cold reopen restores the NPC owner"), Workbench->GetEmbeddedBackpackCharacterIdForTest(), NpcIds[0]);
	TestEqual(TEXT("cold reopen restores the deck subpage"),
		Workbench->GetEmbeddedBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
	TestEqual(TEXT("cold reopen restores the uncommitted deck selection"),
		Workbench->GetEmbeddedPendingDeckIdsForTest(), PendingDeckBeforeCollapse);

	UGameXXKDesktopTrainingWorkbenchWidget* WarmReopen = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	WarmReopen->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("warm fixture opens the workbench"), WarmReopen->OpenWorkbench());
	TestTrue(TEXT("warm fixture expands the backpack"), WarmReopen->OpenBackpack());
	WarmReopen->HandleActionClicked(60);
	WarmReopen->TickForTest(2.9f);
	WarmReopen->HandleActionClicked(60);
	TestFalse(TEXT("warm reopen cancels the pending release"), WarmReopen->IsCollapsedResourceUnloadPendingForTest());
	TestFalse(TEXT("warm reopen keeps active resources"), WarmReopen->AreCollapsedResourcesReleasedForTest());
	TestEqual(TEXT("warm reopen avoids a GC request"), WarmReopen->GetCollapsedGcRequestCountForTest(), 0);

	UGameXXKDesktopTrainingWorkbenchWidget* TalentsReopen = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TalentsReopen->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("talents fixture opens the workbench"), TalentsReopen->OpenWorkbench());
	TestTrue(TEXT("talents fixture expands the backpack"), TalentsReopen->OpenBackpack());
	TalentsReopen->HandleActionClicked(2);
	TestEqual(TEXT("talents fixture selects the talents page"),
		TalentsReopen->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Talents);
	TalentsReopen->HandleActionClicked(60);
	TalentsReopen->TickForTest(1.0f);
	TalentsReopen->HandleActionClicked(60);
	TestEqual(TEXT("a collapsed page without an embedded inventory still restores"),
		TalentsReopen->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Talents);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryStateMachineTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryStateMachineTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("item-carry fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("item-carry fixture opens backpack"), Widget->OpenBackpack());

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const int32 StoneSlot = Widget->FindBackpackItemSlotForTest(StoneId);
	TestTrue(TEXT("fixture finds a draggable backpack stack"), StoneSlot != INDEX_NONE);
	TestTrue(TEXT("left click picks the stack up without mutating inventory"), Widget->PickUpBackpackSlotForTest(StoneSlot));
	TestTrue(TEXT("picked item is attached to the cursor state"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("pickup is non-committing"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("right click cancels the carried item"), Widget->CancelCarriedItemForTest());
	TestFalse(TEXT("cancel clears cursor attachment"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("cancel restores the exact authoritative source"), Subsystem->GetItemCount(StoneId), 10);

	TestTrue(TEXT("item can be picked up again"), Widget->PickUpBackpackSlotForTest(StoneSlot));
	Widget->HandleActionClicked(60);
	TestFalse(TEXT("Tab collapse automatically returns the carried item"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("Tab collapse never mutates the item stack"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("reopening backpack after cancellation succeeds"), Widget->OpenBackpack());

	Widget->HandleActionClicked(3);
	TestTrue(TEXT("only tools are open for right-click routing"), Widget->IsToolsPanelActiveForTest() && !Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("right click routes backpack item into first empty tool slot"), Widget->RightClickBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	TestEqual(TEXT("tool reservation does not consume source item"), Subsystem->GetItemCount(StoneId), 10);
	TestEqual(TEXT("first tool slot records the reserved item"), Widget->GetToolSlotItemIdForTest(0), StoneId);

	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse and tools can be visible together"), Widget->IsWarehousePanelOpenForTest() && Widget->IsToolsPanelActiveForTest());
	const int32 EquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	TestTrue(TEXT("fixture finds another backpack equipment entry"), EquipmentSlot != INDEX_NONE);
	TestTrue(TEXT("warehouse wins right-click priority while both side panels are open"), Widget->RightClickBackpackSlotForTest(EquipmentSlot));
	TestEqual(TEXT("warehouse priority does not add another tool reservation"), Widget->GetOccupiedToolSlotCountForTest(), 1);
	TestEqual(TEXT("warehouse receives the equipment entry"), Widget->GetWarehouseOccupancyForTest(), 1);

	TestTrue(TEXT("combine mode can be selected"), Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Combine));
	const int32 StonesBeforeUnavailableConfirm = Subsystem->GetItemCount(StoneId);
	TestFalse(TEXT("unimplemented combine recipe rejects confirm"), Widget->ConfirmToolForTest());
	TestEqual(TEXT("unimplemented combine never consumes material"), Subsystem->GetItemCount(StoneId), StonesBeforeUnavailableConfirm);
	TestEqual(TEXT("failed combine keeps tool input in place"), Widget->GetToolSlotItemIdForTest(0), StoneId);

	TestTrue(TEXT("picking the reserved tool item attaches it to cursor"), Widget->PickUpToolSlotForTest(0));
	TestTrue(TEXT("tool source is now carried"), Widget->IsCarryingItemForTest());
	TestTrue(TEXT("right-click cancellation returns it to its tool cell"), Widget->CancelCarriedItemForTest());
	TestEqual(TEXT("cancel restores original tool slot"), Widget->GetToolSlotItemIdForTest(0), StoneId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingItemCarryBoundaryRollbackTest,
	"GameXXK.DesktopTraining.Workbench.ItemCarryBoundaryRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingItemCarryBoundaryRollbackTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestNotNull(TEXT("boundary fixture subsystem exists"), Subsystem)
		|| !Subsystem->StartGame())
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestNotNull(TEXT("boundary fixture widget exists"), Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("boundary fixture opens backpack"), Widget->OpenBackpack());

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const int32 OriginalStoneSlot = Widget->FindBackpackItemSlotForTest(StoneId);
	const int32 OccupiedEquipmentSlot = Widget->FindFirstBackpackEquipmentSlotForTest();
	TestTrue(TEXT("fixture exposes an occupied non-origin destination"),
		OriginalStoneSlot != INDEX_NONE && OccupiedEquipmentSlot != INDEX_NONE && OccupiedEquipmentSlot != OriginalStoneSlot);
	TestTrue(TEXT("pickup before invalid drop succeeds"), Widget->PickUpBackpackSlotForTest(OriginalStoneSlot));
	TestFalse(TEXT("occupied destination rejects placement"), Widget->DropCarriedOnBackpackSlotForTest(OccupiedEquipmentSlot));
	TestTrue(TEXT("invalid placement keeps the item attached"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("invalid placement preserves the exact origin slot"), Widget->FindBackpackItemSlotForTest(StoneId), OriginalStoneSlot);
	TestEqual(TEXT("invalid placement preserves stack quantity"), Subsystem->GetItemCount(StoneId), 10);
	TestTrue(TEXT("explicit cancellation resets invalid-drop carry"), Widget->CancelCarriedItemForTest());

	const TArray<FName> CharacterIds = Widget->GetBackpackCharacterIdsForTest();
	if (TestTrue(TEXT("new game exposes a second backpack owner"), CharacterIds.Num() > 1))
	{
		TestTrue(TEXT("pickup before owner switch succeeds"), Widget->PickUpBackpackSlotForTest(OriginalStoneSlot));
		TestTrue(TEXT("switching role/partner succeeds"), Widget->SelectBackpackCharacterForTest(CharacterIds[1]));
		TestFalse(TEXT("owner switch cancels cursor payload"), Widget->IsCarryingItemForTest());
	}

	const int32 StoneSlotAfterOwnerSwitch = Widget->FindBackpackItemSlotForTest(StoneId);
	TestTrue(TEXT("pickup before backpack sort succeeds"), Widget->PickUpBackpackSlotForTest(StoneSlotAfterOwnerSwitch));
	Widget->HandleActionClicked(61);
	TestFalse(TEXT("backpack sort cancels cursor payload"), Widget->IsCarryingItemForTest());

	Widget->HandleActionClicked(0);
	TestTrue(TEXT("warehouse opens independently"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("pickup before warehouse close succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->HandleActionClicked(0);
	TestFalse(TEXT("closing the warehouse cancels cursor payload"), Widget->IsCarryingItemForTest());

	Widget->HandleActionClicked(3);
	TestTrue(TEXT("tools panel opens independently"), Widget->IsToolsPanelActiveForTest());
	TestTrue(TEXT("tool reservation is created"),
		Widget->RightClickBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	TestTrue(TEXT("reserved tool input can be picked up"), Widget->PickUpToolSlotForTest(0));
	Widget->HandleActionClicked(4);
	TestFalse(TEXT("switching right-side page cancels cursor payload"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("switching away from tools clears transient reservations"), Widget->GetOccupiedToolSlotCountForTest(), 0);
	TestEqual(TEXT("switching away from tools preserves authoritative quantity"), Subsystem->GetItemCount(StoneId), 10);

	TestTrue(TEXT("pickup before exit confirmation succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->HandleActionClicked(15);
	TestFalse(TEXT("opening exit confirmation cancels cursor payload"), Widget->IsCarryingItemForTest());
	Widget->HandleActionClicked(53);

	TestTrue(TEXT("pickup before application deactivation succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->NotifyApplicationDeactivatedForTest();
	TestFalse(TEXT("application focus loss cancels cursor payload"), Widget->IsCarryingItemForTest());

	TestTrue(TEXT("pickup before external Slate rebuild succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->ForceExternalSlateRebuildForTest();
	TestFalse(TEXT("external widget rebuild cancels cursor payload"), Widget->IsCarryingItemForTest());

	Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[](USaveGame*, const FString&, const int32)
		{
			return true;
		}));
	TestTrue(TEXT("pickup before save boundary succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	TestTrue(TEXT("save boundary fixture succeeds without filesystem IO"),
		Subsystem->SaveCurrentGame(TEXT("DesktopCarryBoundary"), 991));
	TestFalse(TEXT("save/load boundary cancels cursor payload before serialization"), Widget->IsCarryingItemForTest());
	Subsystem->ResetSaveSlotWriteDelegateForTest();

	TestTrue(TEXT("pickup before widget destruction succeeds"),
		Widget->PickUpBackpackSlotForTest(Widget->FindBackpackItemSlotForTest(StoneId)));
	Widget->DestructForTest();
	TestFalse(TEXT("widget destruction cancels cursor payload"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("all rollback boundaries preserve stack quantity"), Subsystem->GetItemCount(StoneId), 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelTickAvoidsSlateRebuildTest,
	"GameXXK.DesktopTraining.Workbench.TravelTickAvoidsSlateRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelTickAvoidsSlateRebuildTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel tick fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel tick fixture starts a cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel tick fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel tick fixture opens the workbench"), Widget->OpenWorkbench());
	const int32 InitialLayoutBuildCount = Widget->GetProgrammaticLayoutBuildCountForTest();

	// Logical combat now uses the same 1-1 health as active challenge. The first
	// few seconds may still be walking or fighting, but must update the existing
	// strip rather than scheduling a whole-tree rebuild.
	for (int32 TickIndex = 0; TickIndex < 4; ++TickIndex)
	{
		Widget->TickForTest(1.0f);
	}
	TestFalse(TEXT("travel NativeTick schedules no layout rebuild"), Widget->HasPendingLayoutRefreshForTest());
	TestEqual(TEXT("travel settlement preserves the existing widget tree"), Widget->GetProgrammaticLayoutBuildCountForTest(), InitialLayoutBuildCount);
	TestTrue(TEXT("in-place travel refresh keeps the workbench visible"), Widget->IsWorkbenchVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualStrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("top strip creates a live travel visual surface"), Widget->HasTravelVisualStripForTest());
	TestTrue(TEXT("travel visual surface declares the generated walkloop atlas"),
		Widget->GetTravelVisualAtlasResourcePathForTest().Contains(TEXT("walkloop_pilot_v1")));
	TestTrue(TEXT("compact travel combat requests only 1K battle atlas siblings"),
		Widget->AreTravelCombatAtlasesOneKForTest());
	TestTrue(TEXT("travel visual surface declares the seamless background"),
		Widget->GetTravelVisualBackgroundResourcePathForTest().Contains(TEXT("TrainingIdleStrip_Background")));
	TestTrue(TEXT("travel visual background resolves from confirmed ImageTruth"),
		Widget->GetTravelVisualBackgroundResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/ImageTruth/Training/")));

	UBorder* TravelStrip = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip")))
		: nullptr;
	TestNotNull(TEXT("travel visual strip owns a clipping container"), TravelStrip);
	if (TravelStrip)
	{
		TestTrue(
			TEXT("travel strip has no paper-panel backing behind the transparent scene"),
			TravelStrip->Background.DrawAs == ESlateBrushDrawType::NoDrawType);
		TestNull(
			TEXT("travel strip transparent container has no backing texture"),
			TravelStrip->Background.GetResourceObject());
	}

	TArray<UImage*> BackgroundTiles;
	for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
	{
		UImage* Tile = Widget->WidgetTree
			? Cast<UImage>(Widget->WidgetTree->FindWidget(
				*FString::Printf(TEXT("TravelBackgroundTile_%d"), TileIndex)))
			: nullptr;
		TestNotNull(*FString::Printf(TEXT("seamless background tile %d exists"), TileIndex), Tile);
		BackgroundTiles.Add(Tile);
	}
	if (BackgroundTiles.Num() == 3
		&& BackgroundTiles[0]
		&& BackgroundTiles[1]
		&& BackgroundTiles[2])
	{
		const UCanvasPanelSlot* LeftSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[0]->Slot);
		const UCanvasPanelSlot* CenterSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[1]->Slot);
		const UCanvasPanelSlot* RightSlot = Cast<UCanvasPanelSlot>(BackgroundTiles[2]->Slot);
		TestNotNull(TEXT("left seamless tile uses canvas geometry"), LeftSlot);
		TestNotNull(TEXT("center seamless tile uses canvas geometry"), CenterSlot);
		TestNotNull(TEXT("right seamless tile uses canvas geometry"), RightSlot);
		if (LeftSlot && CenterSlot && RightSlot)
		{
			const FVector2D TileSize = CenterSlot->GetSize();
			TestTrue(
				TEXT("background is enlarged enough for its opaque road to continue below the character feet"),
				TileSize.Y >= 290.0f);
			TestTrue(
				TEXT("background enlargement preserves the authored two-point-five-to-one aspect"),
				FMath::IsNearlyEqual(TileSize.X / TileSize.Y, 2.5f, 0.001f));
			TestTrue(
				TEXT("background Y keeps the authored yellow road line on the character foot plane"),
				FMath::Abs(CenterSlot->GetPosition().Y) <= 8.0f);
			TestEqual(TEXT("rightward loop starts with one tile left of the viewport"), LeftSlot->GetPosition().X, -TileSize.X);
			TestEqual(TEXT("rightward loop keeps the middle tile at the origin"), CenterSlot->GetPosition().X, 0.0);
			TestEqual(TEXT("rightward loop keeps one tile to the right"), RightSlot->GetPosition().X, TileSize.X);
		}
	}

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip moves while the runner is walking"), Widget->GetTravelVisualScrollOffsetForTest() > 0.0f);
	TestEqual(TEXT("travel strip displays the generated 12 fps walkloop frame"), Widget->GetTravelVisualWalkFrameForTest(), 6);
	for (const UImage* Tile : BackgroundTiles)
	{
		if (Tile)
		{
			TestTrue(
				TEXT("left-walking hero drives the seamless scene to the right"),
				Tile->GetRenderTransform().Translation.X > 0.0f);
		}
	}

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip keeps the same visual runtime across deferred layout refresh"),
		Widget->GetTravelVisualScrollOffsetForTest() >= 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual loop fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual loop fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual loop fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual loop fixture opens the workbench"), Widget->OpenWorkbench());

	// Advance until the authored route reports a completed loop. The guard is a
	// hang detector; it is deliberately not an assertion about combat duration.
	for (int32 Guard = 0;
		Guard < 512 && Widget->GetTravelVisualCompletedLoopCountForTest() < 1;
		++Guard)
	{
		Widget->TickForTest(1.0f);
	}
	TestEqual(TEXT("one completed travel route increments the visual loop count"),
		Widget->GetTravelVisualCompletedLoopCountForTest(), 1);
	TestEqual(TEXT("the travel runner returns to the same 1-1 stage after its loop"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId, StageId);
	TestTrue(TEXT("the next encounter is walking after the visual loop reset"),
		Subsystem->GetTrainingTravelRuntimeCopy().Phase == EGameXXKTrainingTravelPhase::Walking);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelCombatPresentationTest,
	"GameXXK.DesktopTraining.Workbench.TravelCombatPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelCombatPresentationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel combat fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	FGameXXKRuntimeState& PartyState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPermanentCompanion* ActiveCompanion = PartyState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.bIsActive && !Candidate.InstanceId.IsNone();
		});
	TestNotNull(TEXT("travel combat fixture has an active permanent companion"), ActiveCompanion);
	if (!ActiveCompanion)
	{
		return false;
	}
	PartyState.CardRun.PartySelection.ActivePermanentCompanionInstanceId = ActiveCompanion->InstanceId;
	PartyState.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.YueBai");
	PartyState.CardRun.PartySelection.QuestNpc.NpcId = TEXT("Npc.YueBai");

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel combat fixture starts cleared 1-1"), Subsystem->StartTrainingTravel(StageId));
	const FName FirstEnemyId = Subsystem->GetTrainingTravelRuntimeCopy().EnemyDefinitionId;
	TestFalse(TEXT("travel combat fixture has an authored first enemy"), FirstEnemyId.IsNone());

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel combat fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel combat fixture opens the workbench"), Widget->OpenWorkbench());

	UImage* EnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_0")))
		: nullptr;
	UImage* SecondEnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_1")))
		: nullptr;
	UImage* ThirdEnemyImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyAnimatedUnit_2")))
		: nullptr;
	UImage* HeroImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelHeroAnimatedUnit")))
		: nullptr;
	UImage* PermanentCompanionImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_0")))
		: nullptr;
	UImage* QuestCompanionImage = Widget->WidgetTree
		? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionAnimatedUnit_1")))
		: nullptr;
	UProgressBar* EnemyHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_0")))
		: nullptr;
	UProgressBar* SecondEnemyHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_1")))
		: nullptr;
	UProgressBar* ThirdEnemyHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelEnemyHealth_2")))
		: nullptr;
	UProgressBar* HeroHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelHeroHealth")))
		: nullptr;
	UProgressBar* PermanentCompanionHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionHealth_0")))
		: nullptr;
	UProgressBar* QuestCompanionHealth = Widget->WidgetTree
		? Cast<UProgressBar>(Widget->WidgetTree->FindWidget(TEXT("TravelCompanionHealth_1")))
		: nullptr;
	TestNotNull(TEXT("top strip owns a real animated enemy image"), EnemyImage);
	TestNotNull(TEXT("top strip owns the second enemy formation slot"), SecondEnemyImage);
	TestNotNull(TEXT("top strip owns the third enemy formation slot"), ThirdEnemyImage);
	TestNotNull(TEXT("top strip owns a real animated hero image"), HeroImage);
	TestNotNull(TEXT("top strip owns the selected permanent companion image"), PermanentCompanionImage);
	TestNotNull(TEXT("top strip owns the selected quest companion image"), QuestCompanionImage);
	TestNotNull(TEXT("top strip owns the enemy HP bar"), EnemyHealth);
	TestNotNull(TEXT("top strip owns the second enemy HP bar"), SecondEnemyHealth);
	TestNotNull(TEXT("top strip owns the third enemy HP bar"), ThirdEnemyHealth);
	TestNotNull(TEXT("top strip owns the hero HP bar"), HeroHealth);
	TestNotNull(TEXT("top strip owns the permanent companion HP bar"), PermanentCompanionHealth);
	TestNotNull(TEXT("top strip owns the quest companion HP bar"), QuestCompanionHealth);
	TestNull(TEXT("travel strip has no harvest/collect button"), Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("TravelCollectButton"))
		: nullptr);
	TestEqual(TEXT("953 px lane uses three overlapping seamless tiles"), Widget->GetTravelBackgroundTileCountForTest(), 3);
	TestEqual(TEXT("player-facing strip removes verbose diagnostic text"), Widget->GetTravelVerboseTextBlockCountForTest(), 0);
	if (HeroImage)
	{
		TestEqual(
			TEXT("walking hero keeps a bottom-center ground anchor"),
			HeroImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestTrue(
			TEXT("walking hero uses the authored left-facing atlas without mirroring"),
			HeroImage->GetRenderTransform().Scale.Equals(FVector2D(1.0f, 1.0f), 0.001f));
	}
	if (EnemyImage)
	{
		TestEqual(
			TEXT("enemy keeps the same bottom-center ground anchor as the battle board"),
			EnemyImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
	}
	if (PermanentCompanionImage && QuestCompanionImage && PermanentCompanionHealth && QuestCompanionHealth)
	{
		TestEqual(
			TEXT("permanent companion shares the party ground anchor"),
			PermanentCompanionImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestEqual(
			TEXT("quest companion shares the party ground anchor"),
			QuestCompanionImage->GetRenderTransformPivot(),
			FVector2D(0.5f, 1.0f));
		TestEqual(
			TEXT("companions stay hidden during the walk approach"),
			PermanentCompanionImage->GetVisibility(),
			ESlateVisibility::Collapsed);
		TestEqual(
			TEXT("quest companion stays hidden during the walk approach"),
			QuestCompanionImage->GetVisibility(),
			ESlateVisibility::Collapsed);
	}

	Widget->TickForTest(1.0f);
	Widget->TickForTest(1.0f);
	TestEqual(
		TEXT("standing at an encounter switches to idle presentation"),
		Widget->GetTravelVisualPhaseForTest(),
		EGameXXKTrainingTravelVisualPhase::EncounterIdle);
	TestTrue(TEXT("the authored enemy is visible while standing"), Widget->IsTravelVisualEnemyVisibleForTest());
	if (EnemyImage && SecondEnemyImage && ThirdEnemyImage)
	{
		TestEqual(TEXT("ordinary encounter shows its first enemy slot"), EnemyImage->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("ordinary encounter shows its second enemy slot"), SecondEnemyImage->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("ordinary encounter leaves its third enemy slot empty"), ThirdEnemyImage->GetVisibility(), ESlateVisibility::Collapsed);
	}
	TestEqual(TEXT("encounter idle retains the first authored enemy"), Widget->GetTravelVisualEnemyDefinitionIdForTest(), FirstEnemyId);
	TestEqual(TEXT("standing hero uses the battle idle action"), Widget->GetTravelVisualHeroActionForTest(), EGameXXKBattleAnimationAction::Idle);
	TestEqual(TEXT("PIE probe exposes the visual idle phase by name"), Widget->GetTravelVisualPhaseNameForTest(), FString(TEXT("EncounterIdle")));
	TestEqual(TEXT("PIE probe exposes the hero idle action by name"), Widget->GetTravelVisualHeroActionNameForTest(), FString(TEXT("Idle")));
	TestEqual(TEXT("Blade is independently idle at encounter entry"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Idle")));
	TestEqual(TEXT("Tusi Chief is independently idle at encounter entry"), Widget->GetTravelVisualPartyActionNameForTest(2), FString(TEXT("Idle")));
	TestTrue(TEXT("Blade exposes a real normalized HP fraction"), Widget->GetTravelVisualPartyHealthFractionForTest(1) > 0.0f);
	TestTrue(TEXT("Tusi Chief exposes a real normalized HP fraction"), Widget->GetTravelVisualPartyHealthFractionForTest(2) > 0.0f);
	TestEqual(TEXT("PIE probe exposes the enemy idle action by name"), Widget->GetTravelVisualEnemyActionNameForTest(), FString(TEXT("Idle")));
	TestTrue(TEXT("PIE probe exposes a normalized hero HP fraction"), Widget->GetTravelVisualHeroHealthFractionForTest() > 0.0f);
	TestTrue(TEXT("PIE probe exposes a normalized enemy HP fraction"), Widget->GetTravelVisualEnemyHealthFractionForTest() > 0.0f);
	if (HeroImage)
	{
		const FVector2D IdleScale = HeroImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("battle idle preserves the production atlas facing instead of reversing the hero"),
			IdleScale.X > 0.0f);
		TestTrue(
			TEXT("battle idle uses a uniform content normalization scale"),
			FMath::IsNearlyEqual(IdleScale.X, IdleScale.Y, 0.001f));
		TestTrue(
			TEXT("battle idle normalizes its 81.2 percent alpha height to the 90.6 percent walk height"),
			FMath::IsNearlyEqual(IdleScale.Y, 1.116f, 0.01f));
	}
	if (EnemyImage)
	{
		const FVector2D EnemyIdleScale = EnemyImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("enemy idle preserves the battle-board authored facing toward the hero"),
			EnemyIdleScale.X > 0.0f);
		TestTrue(
			TEXT("enemy idle uses a uniform content-normalization scale"),
			FMath::IsNearlyEqual(EnemyIdleScale.X, EnemyIdleScale.Y, 0.001f));
		TestTrue(
			TEXT("enemy idle is enlarged to approximately the normalized hero height"),
			EnemyIdleScale.Y >= 1.11f);
	}
	if (PermanentCompanionImage && QuestCompanionImage && PermanentCompanionHealth && QuestCompanionHealth)
	{
		TestEqual(
			TEXT("selected permanent companion joins the encounter"),
			PermanentCompanionImage->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
		TestEqual(
			TEXT("selected quest companion joins the encounter"),
			QuestCompanionImage->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
		TestTrue(
			TEXT("permanent companion uses a positive uniform authored-facing scale"),
			PermanentCompanionImage->GetRenderTransform().Scale.X > 0.0f
			&& FMath::IsNearlyEqual(
				PermanentCompanionImage->GetRenderTransform().Scale.X,
				PermanentCompanionImage->GetRenderTransform().Scale.Y,
				0.001f));
		TestEqual(TEXT("selected permanent companion health bar joins the encounter"), PermanentCompanionHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("selected quest companion health bar joins the encounter"), QuestCompanionHealth->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}

	Widget->TickForTest(1.0f);
	TestEqual(
		TEXT("the logical kill begins a real-time hero attack instead of jumping to walk"),
		Widget->GetTravelVisualPhaseForTest(),
		EGameXXKTrainingTravelVisualPhase::HeroAttack);
	TestEqual(
		TEXT("the presentation retains the defeated enemy after the gameplay runner advances"),
		Widget->GetTravelVisualEnemyDefinitionIdForTest(),
		FirstEnemyId);
	TestEqual(TEXT("PIE probe distinguishes the visual attack from the advanced logical runner"), Widget->GetTravelVisualPhaseNameForTest(), FString(TEXT("HeroAttack")));
	TestEqual(TEXT("PIE probe exposes the hero attack action by name"), Widget->GetTravelVisualHeroActionNameForTest(), FString(TEXT("Attack")));
	TestEqual(TEXT("Blade stays idle during the hero action"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Idle")));
	if (HeroImage)
	{
		const FVector2D AttackScale = HeroImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("hero attack keeps its authored left-facing direction"),
			AttackScale.X > 0.0f);
		TestTrue(
			TEXT("hero attack normalizes its 59.8 percent alpha height to the 90.6 percent walk height"),
			FMath::IsNearlyEqual(AttackScale.Y, 1.516f, 0.01f));
	}
	Widget->TickForTest(0.5f);
	Widget->TickForTest(0.5f);
	TestEqual(TEXT("the next logical action belongs to Blade"), Widget->GetTravelVisualPartyActionNameForTest(1), FString(TEXT("Attack")));
	TestEqual(TEXT("hero returns idle during Blade's action"), Widget->GetTravelVisualPartyActionNameForTest(0), FString(TEXT("Idle")));

	if (EnemyImage)
	{
		const float EnemyIdleScale = EnemyImage->GetRenderTransform().Scale.Y;
		Widget->TickForTest(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds);
		TestEqual(
			TEXT("hero attack advances into the enemy hit presentation"),
			Widget->GetTravelVisualPhaseForTest(),
			EGameXXKTrainingTravelVisualPhase::EnemyHit);
		const FVector2D EnemyHitScale = EnemyImage->GetRenderTransform().Scale;
		TestTrue(
			TEXT("enemy hit keeps the authored direction and uniform scale"),
			EnemyHitScale.X > 0.0f
			&& FMath::IsNearlyEqual(EnemyHitScale.X, EnemyHitScale.Y, 0.001f));
		TestTrue(
			TEXT("enemy hit compensates its tighter transparent bounds instead of shrinking"),
			EnemyHitScale.Y > EnemyIdleScale);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchCharacterRosterTest,
	"GameXXK.DesktopTraining.Workbench.CharacterRosterTabsAndClickToParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchCharacterRosterTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!TestTrue(TEXT("character-roster fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("character-roster fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("character-roster fixture expands the backpack"), Widget->OpenBackpack());
	TestNotNull(TEXT("character page exposes the hero roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterHeroButton")) : nullptr);
	TestNotNull(TEXT("character page exposes the partner roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterCompanionButton")) : nullptr);
	TestNotNull(TEXT("character page exposes the NPC roster tab"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("CharacterRosterNpcButton")) : nullptr);

	const TArray<FName> CompanionIds = Widget->GetCompanionCharacterIdsForTest();
	const TArray<FName> NpcIds = Widget->GetNpcCharacterIdsForTest();
	TestEqual(TEXT("partner roster exposes one owned member per profession"), CompanionIds.Num(), 6);
	TestEqual(TEXT("NPC roster exposes all six owned named NPCs"), NpcIds.Num(), 6);
	if (CompanionIds.Num() != 6 || NpcIds.Num() != 6)
	{
		return false;
	}

	const FName SelectedCompanionId = CompanionIds[1];
	TestTrue(TEXT("clicking a partner portrait selects and joins that partner"),
		Widget->SelectBackpackCharacterForTest(SelectedCompanionId));
	TestEqual(TEXT("partner portrait becomes the active permanent party slot"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		SelectedCompanionId);
	TestEqual(TEXT("running Travel immediately rebuilds its permanent party slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[1].UnitId,
		SelectedCompanionId);
	TestEqual(TEXT("embedded backpack switches to the selected partner owner"),
		Widget->GetEmbeddedBackpackCharacterIdForTest(), SelectedCompanionId);

	const FName SelectedNpcId = NpcIds[1];
	TestTrue(TEXT("clicking an NPC portrait selects and joins that NPC"),
		Widget->SelectBackpackCharacterForTest(SelectedNpcId));
	TestEqual(TEXT("NPC portrait becomes the active NPC party slot"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId,
		SelectedNpcId);
	TestEqual(TEXT("running Travel immediately rebuilds its NPC party slot"),
		Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits[2].UnitId,
		SelectedNpcId);
	TestEqual(TEXT("embedded backpack switches to the selected NPC owner"),
		Widget->GetEmbeddedBackpackCharacterIdForTest(), SelectedNpcId);
	return true;
}

#endif
