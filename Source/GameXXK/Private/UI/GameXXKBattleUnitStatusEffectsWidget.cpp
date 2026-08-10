#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "GameXXKCardText.h"
#include "UI/GameXXKBattleStatusIconWidget.h"

namespace
{
	constexpr int32 MaxReadableStatusModels = 6;

	FString ResolveStatusAbbreviation(const EGameXXKCardStatus Status)
	{
		return GameXXKCardText::DescribeStatusName(Status);
	}

	bool AreBadgeModelsEqual(
		const TArray<FGameXXKBattleStatusBadgeModel>& Left,
		const TArray<FGameXXKBattleStatusBadgeModel>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const FGameXXKBattleStatusBadgeModel& LeftModel = Left[Index];
			const FGameXXKBattleStatusBadgeModel& RightModel = Right[Index];
			if (LeftModel.Style.IconId != RightModel.Style.IconId
				|| LeftModel.Style.TexturePath != RightModel.Style.TexturePath
				|| LeftModel.Stacks != RightModel.Stacks
				|| LeftModel.Tooltip != RightModel.Tooltip)
			{
				return false;
			}
		}

		return true;
	}

	FGameXXKBattleStatusBadgeModel MakeOverflowBadge(
		const TArray<FGameXXKBattleStatusBadgeModel>& BadgeModels,
		const int32 VisibleCount)
	{
		FGameXXKBattleStatusBadgeModel OverflowBadge;
		OverflowBadge.Style.IconId = TEXT("MoreStatuses");
		OverflowBadge.Style.DisplayName = TEXT("更多状态");
		OverflowBadge.Style.Tint = FLinearColor(0.28f, 0.20f, 0.13f, 1.0f);
		OverflowBadge.Style.FallbackGlyph = TEXT("⋯");
		OverflowBadge.Stacks = BadgeModels.Num() - VisibleCount;
		TArray<FString> OmittedEntries;
		for (int32 BadgeIndex = VisibleCount; BadgeIndex < BadgeModels.Num(); ++BadgeIndex)
		{
			const FGameXXKBattleStatusBadgeModel& OmittedModel = BadgeModels[BadgeIndex];
			OmittedEntries.Add(FString::Printf(TEXT("%s × %d"), *OmittedModel.Style.DisplayName, OmittedModel.Stacks));
		}
		OverflowBadge.Tooltip = FString::Join(OmittedEntries, TEXT("\n"));
		return OverflowBadge;
	}
}

void UGameXXKBattleUnitStatusEffectsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshStatusIcons();
}

void UGameXXKBattleUnitStatusEffectsWidget::SetStatusEffects(
	const int32 InArmor,
	const TArray<FGameXXKCardStatusStack>& InStatuses)
{
	TArray<FGameXXKBattleStatusBadgeModel> NextBadgeModels = BuildBadgeModels(InArmor, InStatuses);
	if (AreBadgeModelsEqual(CachedBadgeModels, NextBadgeModels))
	{
		return;
	}

	CachedBadgeModels = MoveTemp(NextBadgeModels);
	RefreshStatusIcons();
	++IconRebuildGeneration;
}

bool UGameXXKBattleUnitStatusEffectsWidget::PrepareForScreenSpaceEmbedding()
{
	Initialize();
	EnsureWidgetTree();
	RefreshStatusIcons();
	return StatusIconRow && WidgetTree && WidgetTree->RootWidget == StatusIconRow;
}

bool UGameXXKBattleUnitStatusEffectsWidget::HasRuntimeWidgetTreeForTest() const
{
	return StatusIconRow && WidgetTree && WidgetTree->RootWidget == StatusIconRow;
}

FString UGameXXKBattleUnitStatusEffectsWidget::BuildStatusText(const TArray<FGameXXKCardStatusStack>& InStatuses)
{
	TArray<FString> Parts;
	for (const FGameXXKCardStatusStack& Status : InStatuses)
	{
		if (Status.Stacks <= 0
			|| Status.Status == EGameXXKCardStatus::Invalid
			|| Status.Status == EGameXXKCardStatus::None)
		{
			continue;
		}

		Parts.Add(FString::Printf(TEXT("%s %d"), *ResolveStatusAbbreviation(Status.Status), Status.Stacks));
	}
	return FString::Join(Parts, TEXT(" · "));
}

TArray<FGameXXKBattleStatusBadgeModel> UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(
	const int32 InArmor,
	const TArray<FGameXXKCardStatusStack>& InStatuses)
{
	TArray<FGameXXKBattleStatusBadgeModel> Result;
	if (InArmor > 0)
	{
		FGameXXKBattleStatusBadgeModel& ArmorBadge = Result.AddDefaulted_GetRef();
		ArmorBadge.Style = FGameXXKBattleStatusIconStyle::ResolveArmorIconStyle();
		ArmorBadge.Stacks = InArmor;
		ArmorBadge.Tooltip = FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(ArmorBadge.Style, ArmorBadge.Stacks);
	}

	for (const FGameXXKCardStatusStack& Status : InStatuses)
	{
		if (Status.Stacks <= 0
			|| Status.Status == EGameXXKCardStatus::Invalid
			|| Status.Status == EGameXXKCardStatus::None)
		{
			continue;
		}

		FGameXXKBattleStatusBadgeModel& Badge = Result.AddDefaulted_GetRef();
		Badge.Style = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(Status.Status);
		Badge.Stacks = Status.Stacks;
		Badge.Tooltip = FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(Badge.Style, Badge.Stacks);
	}

	Result.Sort([](const FGameXXKBattleStatusBadgeModel& Left, const FGameXXKBattleStatusBadgeModel& Right)
	{
		if (Left.Style.Priority != Right.Style.Priority)
		{
			return Left.Style.Priority > Right.Style.Priority;
		}
		return Left.Style.DisplayName < Right.Style.DisplayName;
	});
	return Result;
}

int32 UGameXXKBattleUnitStatusEffectsWidget::GetIconCountForTest() const
{
	return StatusIconRow ? StatusIconRow->GetChildrenCount() : 0;
}

FName UGameXXKBattleUnitStatusEffectsWidget::GetIconIdForTest(const int32 Index) const
{
	const UGameXXKBattleStatusIconWidget* const IconWidget = StatusIconRow
		? Cast<UGameXXKBattleStatusIconWidget>(StatusIconRow->GetChildAt(Index))
		: nullptr;
	return IconWidget ? IconWidget->GetIconIdForTest() : NAME_None;
}

FString UGameXXKBattleUnitStatusEffectsWidget::GetIconDisplayedStackForTest(const int32 Index) const
{
	const UGameXXKBattleStatusIconWidget* const IconWidget = StatusIconRow
		? Cast<UGameXXKBattleStatusIconWidget>(StatusIconRow->GetChildAt(Index))
		: nullptr;
	return IconWidget ? IconWidget->GetDisplayedStackForTest() : FString();
}

int32 UGameXXKBattleUnitStatusEffectsWidget::GetIconRebuildGenerationForTest() const
{
	return IconRebuildGeneration;
}

ESlateVisibility UGameXXKBattleUnitStatusEffectsWidget::GetRootHitTestVisibilityForTest()
{
	return ESlateVisibility::SelfHitTestInvisible;
}

void UGameXXKBattleUnitStatusEffectsWidget::EnsureWidgetTree()
{
	SetVisibility(GetRootHitTestVisibilityForTest());

	if (StatusIconRow || !WidgetTree)
	{
		return;
	}

	StatusIconRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattleUnitStatusEffectsRow"));
	WidgetTree->RootWidget = StatusIconRow;
	StatusIconRow->SetVisibility(GetRootHitTestVisibilityForTest());
}

void UGameXXKBattleUnitStatusEffectsWidget::RefreshStatusIcons()
{
	if (!StatusIconRow || !WidgetTree)
	{
		return;
	}

	StatusIconRow->ClearChildren();
	const int32 VisibleCount = FMath::Min(MaxReadableStatusModels, CachedBadgeModels.Num());
	for (int32 BadgeIndex = 0; BadgeIndex < VisibleCount; ++BadgeIndex)
	{
		UGameXXKBattleStatusIconWidget* const IconWidget = WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>();
		IconWidget->PrepareForScreenSpaceEmbedding();
		IconWidget->SetBadgeModel(CachedBadgeModels[BadgeIndex]);
		if (UHorizontalBoxSlot* const IconSlot = StatusIconRow->AddChildToHorizontalBox(IconWidget))
		{
			IconSlot->SetPadding(FMargin(0.5f, 0.0f));
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	if (CachedBadgeModels.Num() > VisibleCount)
	{
		UGameXXKBattleStatusIconWidget* const OverflowIcon = WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>();
		OverflowIcon->PrepareForScreenSpaceEmbedding();
		OverflowIcon->SetBadgeModel(MakeOverflowBadge(CachedBadgeModels, VisibleCount), true);
		if (UHorizontalBoxSlot* const OverflowSlot = StatusIconRow->AddChildToHorizontalBox(OverflowIcon))
		{
			OverflowSlot->SetPadding(FMargin(0.5f, 0.0f));
			OverflowSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	StatusIconRow->SetVisibility(CachedBadgeModels.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}
