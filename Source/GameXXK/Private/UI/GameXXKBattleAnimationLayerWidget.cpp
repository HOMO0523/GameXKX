#include "UI/GameXXKBattleAnimationLayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"

TSharedRef<SWidget> UGameXXKBattleAnimationLayerWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleAnimationLayerWidget::NativeDestruct()
{
	ResetPresentation();
	Super::NativeDestruct();
}

void UGameXXKBattleAnimationLayerWidget::ResetPresentation()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKBattleAnimationLayerWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleAnimationLayerCompatibilityTree"));
	}
	if (!WidgetTree || RootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleAnimationLayerCompatibilityRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetVisibility(ESlateVisibility::Collapsed);
}

UCanvasPanel* UGameXXKBattleAnimationLayerWidget::GetRootCanvasForTest() const
{
	return RootCanvas;
}
