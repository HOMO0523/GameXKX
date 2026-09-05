#include "UI/GameXXKBattleUnitHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

void UGameXXKBattleUnitHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshFromView();
}

void UGameXXKBattleUnitHudWidget::SetUnitView(const FGameXXKBattleUnitHudView& InView)
{
	UE_LOG(LogTemp, Verbose, TEXT("[HudView] unit=%s setHP=%d hud=%s"), *InView.UnitId.ToString(), InView.CurrentHP, *GetPathName());
	CachedView = InView;
	bHasUnitView = true;
	PrepareForBoardEmbedding();
	RefreshFromView();
}

bool UGameXXKBattleUnitHudWidget::MatchesUnitView(const FGameXXKBattleUnitHudView& InView) const
{
	if (!bHasUnitView
		|| CachedView.UnitId != InView.UnitId
		|| CachedView.Side != InView.Side
		|| CachedView.Role != InView.Role
		|| CachedView.SlotNumber != InView.SlotNumber
		|| CachedView.bLiving != InView.bLiving
		|| CachedView.bShowMana != InView.bShowMana
		|| CachedView.CurrentHP != InView.CurrentHP
		|| CachedView.MaxHP != InView.MaxHP
		|| CachedView.CurrentMana != InView.CurrentMana
		|| CachedView.MaxMana != InView.MaxMana
		|| CachedView.Armor != InView.Armor
		|| CachedView.CurrentEnemyPhase != InView.CurrentEnemyPhase
		|| CachedView.TotalEnemyPhases != InView.TotalEnemyPhases
		|| !CachedView.DisplayName.ToString().Equals(InView.DisplayName.ToString(), ESearchCase::CaseSensitive))
	{
		return false;
	}

	TArray<FGameXXKCardStatusStack> CachedStatuses = CachedView.Statuses;
	TArray<FGameXXKCardStatusStack> IncomingStatuses = InView.Statuses;
	const auto SortStatuses = [](TArray<FGameXXKCardStatusStack>& Statuses)
	{
		Statuses.Sort([](const FGameXXKCardStatusStack& Left, const FGameXXKCardStatusStack& Right)
		{
			if (Left.Status != Right.Status)
			{
				return static_cast<uint8>(Left.Status) < static_cast<uint8>(Right.Status);
			}
			return Left.Stacks < Right.Stacks;
		});
	};
	SortStatuses(CachedStatuses);
	SortStatuses(IncomingStatuses);
	if (CachedStatuses.Num() != IncomingStatuses.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < CachedStatuses.Num(); ++Index)
	{
		if (CachedStatuses[Index].Status != IncomingStatuses[Index].Status
			|| CachedStatuses[Index].Stacks != IncomingStatuses[Index].Stacks)
		{
			return false;
		}
	}
	return true;
}

bool UGameXXKBattleUnitHudWidget::PrepareForBoardEmbedding()
{
	Initialize();
	EnsureWidgetTree();
	const bool bChildrenPrepared = ResourceWidget
		&& ResourceWidget->PrepareForScreenSpaceEmbedding()
		&& StatusEffectsWidget
		&& StatusEffectsWidget->PrepareForScreenSpaceEmbedding();
	SetVisibility(!bHasUnitView || CachedView.bLiving ? GetRootHitTestVisibilityForTest() : ESlateVisibility::Collapsed);
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox && bChildrenPrepared;
}

FName UGameXXKBattleUnitHudWidget::GetUnitIdForTest() const
{
	return CachedView.UnitId;
}

EGameXXKCardTargetSide UGameXXKBattleUnitHudWidget::GetSideForTest() const
{
	return CachedView.Side;
}

int32 UGameXXKBattleUnitHudWidget::GetSlotNumberForTest() const
{
	return CachedView.SlotNumber;
}

UGameXXKBattleUnitResourceWidget* UGameXXKBattleUnitHudWidget::GetResourceWidgetForTest() const
{
	return ResourceWidget;
}

UGameXXKBattleUnitStatusEffectsWidget* UGameXXKBattleUnitHudWidget::GetStatusEffectsWidgetForTest() const
{
	return StatusEffectsWidget;
}

ESlateVisibility UGameXXKBattleUnitHudWidget::GetRootHitTestVisibilityForTest()
{
	return ESlateVisibility::SelfHitTestInvisible;
}

void UGameXXKBattleUnitHudWidget::EnsureWidgetTree()
{
	if (RootBox || !WidgetTree)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleUnitHudRoot"));
	WidgetTree->RootWidget = RootBox;
	RootBox->SetVisibility(GetRootHitTestVisibilityForTest());

	ResourceWidget = WidgetTree->ConstructWidget<UGameXXKBattleUnitResourceWidget>(UGameXXKBattleUnitResourceWidget::StaticClass(), TEXT("BattleUnitHudResource"));
	StatusEffectsWidget = WidgetTree->ConstructWidget<UGameXXKBattleUnitStatusEffectsWidget>(UGameXXKBattleUnitStatusEffectsWidget::StaticClass(), TEXT("BattleUnitHudStatuses"));
	if (UVerticalBoxSlot* const ResourceSlot = RootBox->AddChildToVerticalBox(ResourceWidget))
	{
		ResourceSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	if (UVerticalBoxSlot* const StatusSlot = RootBox->AddChildToVerticalBox(StatusEffectsWidget))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UGameXXKBattleUnitHudWidget::RefreshFromView()
{
	if (!bHasUnitView || !ResourceWidget || !StatusEffectsWidget)
	{
		return;
	}

	ResourceWidget->SetUnitVitals(
		FGameXXKBattlePresentation::FormatSlotLabel(CachedView.Side, CachedView.SlotNumber),
		CachedView.DisplayName,
		CachedView.CurrentHP,
		CachedView.MaxHP,
		CachedView.CurrentMana,
		CachedView.MaxMana,
		CachedView.bShowMana && CachedView.Side == EGameXXKCardTargetSide::Party);
	StatusEffectsWidget->SetStatusEffects(
		CachedView.Armor,
		CachedView.Statuses,
		CachedView.CurrentEnemyPhase,
		CachedView.TotalEnemyPhases);
	SetVisibility(CachedView.bLiving ? GetRootHitTestVisibilityForTest() : ESlateVisibility::Collapsed);
}
