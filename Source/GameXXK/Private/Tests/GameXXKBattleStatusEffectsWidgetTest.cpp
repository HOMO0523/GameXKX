#include "Misc/AutomationTest.h"

#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/GameXXKBattleStatusIconWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardStatusStack MakeStatusStack(const EGameXXKCardStatus Status, const int32 Stacks)
	{
		FGameXXKCardStatusStack Result;
		Result.Status = Status;
		Result.Stacks = Stacks;
		return Result;
	}

	UGameXXKBattleStatusIconWidget* GetStatusIconAt(
		UGameXXKBattleUnitStatusEffectsWidget* const EffectsWidget,
		const int32 Index)
	{
		UHorizontalBox* const StatusIconRow = EffectsWidget
			? Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")))
			: nullptr;
		return StatusIconRow ? Cast<UGameXXKBattleStatusIconWidget>(StatusIconRow->GetChildAt(Index)) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleStatusEffectsWidgetTest,
	"GameXXK.UI.Battle.StatusEffectsWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleStatusEffectsWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleUnitStatusEffectsWidget* const EffectsWidget = NewObject<UGameXXKBattleUnitStatusEffectsWidget>();
	TestNotNull(TEXT("status effects widget is created"), EffectsWidget);
	if (!EffectsWidget)
	{
		return false;
	}

	TestTrue(TEXT("status effects widget prepares a native runtime tree for screen-space embedding"), EffectsWidget->PrepareForScreenSpaceEmbedding());
	TestTrue(TEXT("status effects widget retains its native runtime tree"), EffectsWidget->HasRuntimeWidgetTreeForTest());
	TestEqual(TEXT("effects root is input-transparent"), EffectsWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(
		TEXT("effects root exposes the required input transparency contract"),
		UGameXXKBattleUnitStatusEffectsWidget::GetRootHitTestVisibilityForTest(),
		ESlateVisibility::SelfHitTestInvisible);

	TArray<FGameXXKCardStatusStack> InitialStatuses;
	InitialStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Poison, 2));
	InitialStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Momentum, 1));
	EffectsWidget->SetStatusEffects(7, InitialStatuses);
	TestEqual(TEXT("armor and two valid statuses create three icon child widgets"), EffectsWidget->GetIconCountForTest(), 3);
	TestEqual(TEXT("armor is the first sorted icon"), EffectsWidget->GetIconIdForTest(0), FName(TEXT("ArmorShield")));
	TestEqual(TEXT("poison stack is displayed as its resolved stack count"), EffectsWidget->GetIconDisplayedStackForTest(1), FString(TEXT("2")));
	TestEqual(TEXT("normal icon stacks cap at a readable value"), UGameXXKBattleStatusIconWidget::FormatStackForTest(150), FString(TEXT("99+")));
	UHorizontalBox* StatusIconRow = Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")));
	TestNotNull(TEXT("effects widget exposes its live status icon row"), StatusIconRow);
	if (StatusIconRow)
	{
		TestEqual(TEXT("the live nonempty effects row is input-transparent"), StatusIconRow->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	EffectsWidget->SetVisibility(ESlateVisibility::Visible);
	TestTrue(TEXT("effects widget re-prepares after an outer visibility reset"), EffectsWidget->PrepareForScreenSpaceEmbedding());
	TestEqual(TEXT("repreparing restores effects outer input transparency"), EffectsWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	StatusIconRow = Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")));
	TestNotNull(TEXT("effects widget retains its live status icon row after reprepare"), StatusIconRow);
	if (StatusIconRow)
	{
		TestEqual(TEXT("the live nonempty effects row remains input-transparent after reprepare"), StatusIconRow->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}

	UGameXXKBattleStatusIconWidget* const FirstIcon = GetStatusIconAt(EffectsWidget, 0);
	TestNotNull(TEXT("effects row embeds actual native icon widgets"), FirstIcon);
	if (FirstIcon)
	{
		TestTrue(TEXT("embedded icon prepares its own native runtime tree"), FirstIcon->HasRuntimeWidgetTreeForTest());
		FirstIcon->SetVisibility(ESlateVisibility::Visible);
		TestTrue(TEXT("embedded icon re-prepares after an outer visibility reset"), FirstIcon->PrepareForScreenSpaceEmbedding());
		TestEqual(TEXT("repreparing restores embedded icon outer input transparency"), FirstIcon->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		USizeBox* const IconRoot = Cast<USizeBox>(FirstIcon->GetWidgetFromName(TEXT("BattleStatusIconRoot")));
		TestNotNull(TEXT("embedded icon exposes its native size-box root"), IconRoot);
		if (IconRoot)
		{
			TestEqual(TEXT("embedded icon root is input-transparent"), IconRoot->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		}
		UBorder* const HitTarget = Cast<UBorder>(FirstIcon->GetWidgetFromName(TEXT("BattleStatusIconHitTarget")));
		TestNotNull(TEXT("embedded icon exposes its pointer-active hit target"), HitTarget);
		if (HitTarget)
		{
			TestEqual(TEXT("the icon hit target is pointer-active"), HitTarget->GetVisibility(), ESlateVisibility::Visible);
			UWidget* const TooltipWidget = HitTarget->GetToolTip();
			TestNotNull(TEXT("the icon hit target owns a tooltip widget"), TooltipWidget);
			if (TooltipWidget)
			{
				TestEqual(TEXT("the tooltip itself does not block hit testing"), TooltipWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
			}
		}
	}
	TestEqual(
		TEXT("icon hit target exposes the required pointer visibility contract"),
		UGameXXKBattleStatusIconWidget::GetHitTargetVisibilityForTest(),
		ESlateVisibility::Visible);
	TestEqual(
		TEXT("icon tooltip exposes the required input transparency contract"),
		UGameXXKBattleStatusIconWidget::GetTooltipVisibilityForTest(),
		ESlateVisibility::HitTestInvisible);

	const int32 InitialGeneration = EffectsWidget->GetIconRebuildGenerationForTest();
	EffectsWidget->SetStatusEffects(7, InitialStatuses);
	TestEqual(TEXT("an unchanged status snapshot does not rebuild icon children"), EffectsWidget->GetIconRebuildGenerationForTest(), InitialGeneration);
	EffectsWidget->SetStatusEffects(0, TArray<FGameXXKCardStatusStack>());
	TestEqual(TEXT("clearing the snapshot removes all icon children"), EffectsWidget->GetIconCountForTest(), 0);
	TestEqual(TEXT("clearing a changed snapshot rebuilds the icon row"), EffectsWidget->GetIconRebuildGenerationForTest(), InitialGeneration + 1);

	TArray<FGameXXKCardStatusStack> OverflowStatuses;
	// Armor and five bleed entries fit; the sixth bleed entry plus mark must be listed by overflow.
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Mark, 1));
	EffectsWidget->SetStatusEffects(4, OverflowStatuses);
	TestEqual(TEXT("armor plus seven statuses uses six icons and one overflow icon"), EffectsWidget->GetIconCountForTest(), 7);
	TestEqual(TEXT("the final icon is the overflow indicator"), EffectsWidget->GetIconIdForTest(6), FName(TEXT("MoreStatuses")));
	TestEqual(TEXT("the overflow indicator reports the omitted count"), EffectsWidget->GetIconDisplayedStackForTest(6), FString(TEXT("+2")));
	UGameXXKBattleStatusIconWidget* const OverflowIcon = GetStatusIconAt(EffectsWidget, 6);
	TestNotNull(TEXT("the overflow entry is an embedded native icon widget"), OverflowIcon);
	if (OverflowIcon)
	{
		UBorder* const OverflowHitTarget = Cast<UBorder>(OverflowIcon->GetWidgetFromName(TEXT("BattleStatusIconHitTarget")));
		TestNotNull(TEXT("the overflow entry exposes its tooltip hit target"), OverflowHitTarget);
		UBorder* const OverflowTooltipPaper = OverflowHitTarget ? Cast<UBorder>(OverflowHitTarget->GetToolTip()) : nullptr;
		TestNotNull(TEXT("the overflow entry owns a tooltip paper"), OverflowTooltipPaper);
		UTextBlock* const OverflowTooltipText = OverflowTooltipPaper ? Cast<UTextBlock>(OverflowTooltipPaper->GetContent()) : nullptr;
		TestNotNull(TEXT("the overflow entry owns readable tooltip text"), OverflowTooltipText);
		if (OverflowTooltipText)
		{
			const FString OverflowTooltip = OverflowTooltipText->GetText().ToString();
			TestTrue(TEXT("overflow tooltip lists the omitted bleed status"), OverflowTooltip.Contains(TEXT("流血 × 1")));
			TestTrue(TEXT("overflow tooltip lists the omitted mark status"), OverflowTooltip.Contains(TEXT("标记 × 1")));
		}
	}

	return true;
}

#endif
