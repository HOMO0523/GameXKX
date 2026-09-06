#include "Misc/AutomationTest.h"

#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKTalentTreeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentTreeWidgetTest,
	"GameXXK.Talents.Widget.GraphIconsSelectionAndPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentTreeWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKTalentTreeWidget* Widget = NewObject<UGameXXKTalentTreeWidget>();
	if (!TestTrue(TEXT("talent widget fixture starts"),
		Subsystem && Subsystem->StartGame() && Widget))
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	Widget->RebuildForTest();
	TestEqual(TEXT("only the root is revealed before the first purchase"),
		Widget->GetVisibleNodeCountForTest(),
		1);
	TestTrue(TEXT("root node is visible"), Widget->IsNodeVisibleForTest(TEXT("Talent.Root")));
	TestTrue(TEXT("selected root uses the approved square selected-state base"),
		Widget->GetNodeFrameResourcePathForTest(TEXT("Talent.Root")).Contains(TEXT("T_MasterV2_SquareSelected")));
	TestTrue(TEXT("root reuses the current talent navigation icon"),
		Widget->GetNodeIconResourcePathForTest(TEXT("Talent.Root")).Contains(TEXT("T_TrainingNavTalents")));
	TestEqual(TEXT("root icon keeps a square 74x74 desired size"),
		Widget->GetNodeIconDesiredSizeForTest(TEXT("Talent.Root")),
		FVector2D(74.0f, 74.0f));
	TestEqual(TEXT("root name is outside and below the Item Slot"),
		Widget->GetNodeNameForTest(TEXT("Talent.Root")),
		FText::FromString(TEXT("行旅根基")));
	TestEqual(TEXT("root rank is outside and below the Item Slot"),
		Widget->GetNodeRankForTest(TEXT("Talent.Root")),
		FText::FromString(TEXT("0/1")));
	TestFalse(TEXT("the Item Slot button contains no name or rank text"),
		Widget->IsNodeTextInsideButtonForTest(TEXT("Talent.Root")));
	TestTrue(TEXT("upgrade action uses the existing paper button"),
		Widget->GetPurchaseButtonResourcePathForTest().Contains(TEXT("T_MasterV2_ButtonPurchase")));
	TestEqual(TEXT("upgrade action is never called light-up"),
		Widget->GetPurchaseButtonLabelForTest(),
		FText::FromString(TEXT("升级")));
	UTextBlock* DetailName = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TalentDetailName")))
		: nullptr;
	UTextBlock* DetailBody = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TalentDetailBody")))
		: nullptr;
	UTextBlock* UpgradePrice = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("TalentUpgradePriceText")))
		: nullptr;
	UBorder* DetailPanel = Widget->WidgetTree
		? Cast<UBorder>(Widget->WidgetTree->FindWidget(TEXT("TalentDetailPanel")))
		: nullptr;
	TestEqual(TEXT("talent details show the current talent name on one line only"),
		DetailName ? DetailName->GetText() : FText::GetEmpty(),
		FText::FromString(TEXT("行旅根基")));
	TestEqual(TEXT("talent details show only the concise current effect"),
		DetailBody ? DetailBody->GetText() : FText::GetEmpty(),
		FText::FromString(TEXT("仓库页数 +1")));
	TestEqual(TEXT("talent details label only the current upgrade price"),
		UpgradePrice ? UpgradePrice->GetText() : FText::GetEmpty(),
		FText::FromString(TEXT("升级售价：2500")));
	TestNull(TEXT("talent details no longer duplicate the shared gold display"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("TalentGoldText")) : nullptr);
	TestTrue(TEXT("talent detail column has no white backing"),
		DetailPanel && FMath::IsNearlyZero(DetailPanel->GetBrushColor().A));
	TestTrue(TEXT("root can be selected"), Widget->SelectNodeForTest(TEXT("Talent.Root")));
	int32 PurchaseCommitCount = 0;
	Widget->OnPurchaseCommitted().AddLambda([&PurchaseCommitCount]()
	{
		++PurchaseCommitCount;
	});
	const int32 GoldBefore = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("the real purchase button delegates to the authoritative subsystem"),
		Widget->ClickPurchaseButtonForTest());
	TestEqual(TEXT("a successful purchase notifies the parent presentation once"),
		PurchaseCommitCount,
		1);
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("a maxed one-rank talent replaces its current price with maxed text"),
		UpgradePrice ? UpgradePrice->GetText() : FText::GetEmpty(),
		FText::FromString(TEXT("升级售价：已满级")));
	TestEqual(TEXT("root costs the approved 2500 gold"),
		Subsystem->GetRuntimeState().PlayerGold,
		GoldBefore - 2500);
	TestEqual(TEXT("root purchase reveals the four 45-degree branch entries"),
		Widget->GetVisibleNodeCountForTest(),
		5);
	TestTrue(TEXT("combat entry is revealed"),
		Widget->IsNodeVisibleForTest(TEXT("Talent.Entry.Combat")));
	TestTrue(TEXT("capacity entry is revealed"),
		Widget->IsNodeVisibleForTest(TEXT("Talent.Entry.CapacityChest")));
	TestTrue(TEXT("idle entry is revealed"),
		Widget->IsNodeVisibleForTest(TEXT("Talent.Entry.IdleOffline")));
	TestTrue(TEXT("tools entry is revealed"),
		Widget->IsNodeVisibleForTest(TEXT("Talent.Entry.Tools")));
	TestTrue(TEXT("purchased root uses the lit icon state"),
		Widget->GetNodeIconTintForTest(TEXT("Talent.Root")).GetLuminance() > 0.6f);
	for (const float Angle : Widget->GetRenderedConnectionAnglesForTest())
	{
		const float Normalized = FMath::Fmod(FMath::Abs(Angle), 180.0f);
		const float AcuteAngle = FMath::Min(Normalized, 180.0f - Normalized);
		TestTrue(TEXT("every rendered root connection is snapped to 45 degrees"),
			FMath::IsNearlyEqual(AcuteAngle, 45.0f, 0.01f));
	}
	for (const FVector2D Offset : Widget->GetRenderedConnectionBoundaryOffsetsForTest())
	{
		TestTrue(TEXT("each root line reaches the exact 45-degree Item Slot corner"),
			FMath::IsNearlyEqual(FMath::Abs(Offset.X), 45.0f, 0.01f)
			&& FMath::IsNearlyEqual(FMath::Abs(Offset.Y), 45.0f, 0.01f));
	}

	Widget->HandleNodeClicked(TEXT("Talent.Entry.Combat"));
	TestTrue(TEXT("previous root returns to the normal Item Slot base"),
		Widget->GetNodeFrameResourcePathForTest(TEXT("Talent.Root")).Contains(TEXT("T_MasterV2_ItemSlot")));
	TestTrue(TEXT("newly selected combat node uses the square selected-state base"),
		Widget->GetNodeFrameResourcePathForTest(TEXT("Talent.Entry.Combat")).Contains(TEXT("T_MasterV2_SquareSelected")));
	TestTrue(TEXT("combat entry purchases from the real Upgrade action"),
		Widget->ClickPurchaseButtonForTest());
	Widget->TickForTest(0.0f);
	TestEqual(TEXT("one diagonal main reveals horizontal, vertical, and next diagonal nodes"),
		Widget->GetVisibleNodeCountForTest(), 8);
	bool bHasHorizontalBoundary = false;
	bool bHasVerticalBoundary = false;
	bool bHasDiagonalBoundary = false;
	for (const FVector2D Offset : Widget->GetRenderedConnectionBoundaryOffsetsForTest())
	{
		bHasHorizontalBoundary = bHasHorizontalBoundary
			|| (FMath::IsNearlyEqual(FMath::Abs(Offset.X), 45.0f, 0.01f)
				&& FMath::IsNearlyZero(Offset.Y, 0.01f));
		bHasVerticalBoundary = bHasVerticalBoundary
			|| (FMath::IsNearlyZero(Offset.X, 0.01f)
				&& FMath::IsNearlyEqual(FMath::Abs(Offset.Y), 45.0f, 0.01f));
		bHasDiagonalBoundary = bHasDiagonalBoundary
			|| (FMath::IsNearlyEqual(FMath::Abs(Offset.X), 45.0f, 0.01f)
				&& FMath::IsNearlyEqual(FMath::Abs(Offset.Y), 45.0f, 0.01f));
	}
	TestTrue(TEXT("revealed branch contains a horizontal line touching a slot edge"), bHasHorizontalBoundary);
	TestTrue(TEXT("revealed branch contains a vertical line touching a slot edge"), bHasVerticalBoundary);
	TestTrue(TEXT("revealed branch contains a 45-degree continuation touching slot corners"), bHasDiagonalBoundary);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentWorkbenchCrossPanelRefreshTest,
	"GameXXK.Talents.Widget.CrossPanelToolUnlockRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentWorkbenchCrossPanelRefreshTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("cross-panel fixture starts"),
		Subsystem && Subsystem->StartGame() && Workbench))
	{
		return false;
	}
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	if (!TestTrue(TEXT("cross-panel fixture opens the visible workbench"), Workbench->OpenWorkbench())
		|| !TestTrue(TEXT("cross-panel fixture opens Backpack"), Workbench->OpenBackpack()))
	{
		return false;
	}
	Workbench->HandleActionClicked(2);
	Workbench->HandleActionClicked(3);
	UGameXXKTalentTreeWidget* TalentTree = Workbench->WidgetTree
		? Cast<UGameXXKTalentTreeWidget>(
			Workbench->WidgetTree->FindWidget(TEXT("PermanentTalentTreeWidget")))
		: nullptr;
	UWidget* LockedToolsPanel = Workbench->WidgetTree
		? Workbench->WidgetTree->FindWidget(TEXT("ToolsTalentLockedPanel"))
		: nullptr;
	if (!TestNotNull(TEXT("workbench embeds the permanent talent tree"), TalentTree)
		|| !TestNotNull(TEXT("tools begin behind the talent lock overlay"), LockedToolsPanel))
	{
		return false;
	}
	UImage* SharedGoldIcon = Workbench->WidgetTree
		? Cast<UImage>(Workbench->WidgetTree->FindWidget(TEXT("BackpackGoldIcon")))
		: nullptr;
	UTextBlock* SharedGoldText = Workbench->WidgetTree
		? Cast<UTextBlock>(Workbench->WidgetTree->FindWidget(TEXT("BackpackGoldText")))
		: nullptr;
	const UObject* SharedGoldResource = SharedGoldIcon
		? SharedGoldIcon->GetBrush().GetResourceObject()
		: nullptr;
	TestTrue(TEXT("talent and tools pages keep the Backpack-position ingot icon"),
		SharedGoldResource
		&& SharedGoldResource->GetPathName().Contains(TEXT("T_MasterV2_Ingot")));
	TestNotNull(TEXT("talent and tools pages keep the shared live gold number"), SharedGoldText);
	Subsystem->GetMutableRuntimeState().PlayerGold = 9876;
	Workbench->TickForTest(1.0f);
	TestEqual(TEXT("shared gold number refreshes in place while Talents are open"),
		SharedGoldText ? SharedGoldText->GetText() : FText::GetEmpty(),
		FText::FromString(TEXT("9876")));
	TalentTree->RebuildForTest();

	UWidget* ToolModeButton = Workbench->WidgetTree->FindWidget(TEXT("ToolButton_0"));
	TestTrue(TEXT("locked tool controls do not show through the shared paper"),
		ToolModeButton && ToolModeButton->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("real root button purchases"), TalentTree->ClickPurchaseButtonForTest());
	TalentTree->TickForTest(0.0f);
	TestTrue(TEXT("tools entry can be selected after root refresh"),
		TalentTree->SelectNodeForTest(TEXT("Talent.Entry.Tools")));
	TestTrue(TEXT("real tools-entry button purchases"), TalentTree->ClickPurchaseButtonForTest());
	TalentTree->TickForTest(0.0f);
	TestEqual(TEXT("talent purchase keeps the same central graph widget alive"),
		Workbench->WidgetTree->FindWidget(TEXT("PermanentTalentTreeWidget")),
		static_cast<UWidget*>(TalentTree));
	TestEqual(TEXT("tools-entry purchase collapses the lock overlay in place"),
		LockedToolsPanel->GetVisibility(),
		ESlateVisibility::Collapsed);
	TestTrue(TEXT("unlocking tools restores the existing controls without rebuilding the talent graph"),
		ToolModeButton && ToolModeButton->GetVisibility() == ESlateVisibility::Visible);
	TestNotNull(TEXT("five-mode tool controls remain ready behind the overlay"),
		Workbench->WidgetTree->FindWidget(TEXT("ToolButton_0")));
	return true;
}

#endif
