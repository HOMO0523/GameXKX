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
	StatusEffectsWidget->SetStatusEffects(CachedView.Armor, CachedView.Statuses);
	SetVisibility(CachedView.bLiving ? GetRootHitTestVisibilityForTest() : ESlateVisibility::Collapsed);
}
