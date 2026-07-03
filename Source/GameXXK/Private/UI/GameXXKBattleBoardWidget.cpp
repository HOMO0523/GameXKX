#include "UI/GameXXKBattleBoardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/TextBlock.h"
#include "GameXXKMVPRules.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static const FName ResolveBattleVictoryCommand(TEXT("ResolveBattleVictory"));
	const FVector2D EnemyBattleSlotPosition(560.0f, 170.0f);
	const FVector2D EnemyBattleSlotSize(180.0f, 180.0f);
	const FVector2D PartyBattleSlotPosition(1240.0f, 118.0f);
	const FVector2D PartyBattleSlotSize(160.0f, 140.0f);
	const float PartyBattleSlotVerticalGap = 170.0f;
}

TSharedRef<SWidget> UGameXXKBattleBoardWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

FReply UGameXXKBattleBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ExecutePrimaryEnemyAction())
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGameXXKBattleBoardWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	ConfigureSlots();

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	SetVisibility(Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
}

bool UGameXXKBattleBoardWidget::ExecutePrimaryEnemyAction()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}

	const bool bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, ResolveBattleVictoryCommand);
	if (bExecuted)
	{
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::IsBattleBoardVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

int32 UGameXXKBattleBoardWidget::GetEnemySlotCount() const
{
	return EnemySlots.Num();
}

int32 UGameXXKBattleBoardWidget::GetPartySlotCount() const
{
	return PartySlots.Num();
}

FString UGameXXKBattleBoardWidget::GetEnemySlotSide() const
{
	return TEXT("Left");
}

FString UGameXXKBattleBoardWidget::GetPartySlotSide() const
{
	return TEXT("Right");
}

FVector2D UGameXXKBattleBoardWidget::GetEnemySlotPositionForTest(int32 SlotIndex) const
{
	if (!EnemySlots.IsValidIndex(SlotIndex) || !EnemySlots[SlotIndex])
	{
		return FVector2D::ZeroVector;
	}

	const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EnemySlots[SlotIndex]->Slot);
	return CanvasSlot ? CanvasSlot->GetPosition() : FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetPartySlotPositionForTest(int32 SlotIndex) const
{
	if (!PartySlots.IsValidIndex(SlotIndex) || !PartySlots[SlotIndex])
	{
		return FVector2D::ZeroVector;
	}

	const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PartySlots[SlotIndex]->Slot);
	return CanvasSlot ? CanvasSlot->GetPosition() : FVector2D::ZeroVector;
}

void UGameXXKBattleBoardWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleBoardWidgetTree"));
	}
	if (!WidgetTree || RootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKBattleBoardRoot"));
	WidgetTree->RootWidget = RootCanvas;

	if (UBorder* EnemySlot = CreateSlotBorder(TEXT("BattleEnemySlot0"), FLinearColor(0.36f, 0.11f, 0.10f, 0.92f), FText::FromString(TEXT("Monster"))))
	{
		if (UCanvasPanelSlot* EnemyCanvasSlot = RootCanvas->AddChildToCanvas(EnemySlot))
		{
			EnemyCanvasSlot->SetPosition(EnemyBattleSlotPosition);
			EnemyCanvasSlot->SetSize(EnemyBattleSlotSize);
		}
		EnemySlots.Add(EnemySlot);
	}

	for (int32 SlotIndex = 0; SlotIndex < 2; ++SlotIndex)
	{
		const FText Label = SlotIndex == 0 ? FText::FromString(TEXT("Hero")) : FText::FromString(TEXT("Follower"));
		if (UBorder* PartySlot = CreateSlotBorder(
			FString::Printf(TEXT("BattlePartySlot%d"), SlotIndex),
			SlotIndex == 0 ? FLinearColor(0.12f, 0.31f, 0.44f, 0.92f) : FLinearColor(0.12f, 0.38f, 0.28f, 0.92f),
			Label))
		{
			if (UCanvasPanelSlot* PartyCanvasSlot = RootCanvas->AddChildToCanvas(PartySlot))
			{
				PartyCanvasSlot->SetPosition(PartyBattleSlotPosition + FVector2D(0.0f, SlotIndex * PartyBattleSlotVerticalGap));
				PartyCanvasSlot->SetSize(PartyBattleSlotSize);
			}
			PartySlots.Add(PartySlot);
		}
	}
}

void UGameXXKBattleBoardWidget::ConfigureSlots()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bBattleVisible = State && State->Screen == EGameXXKScreen::Battle;
	const bool bFollowerVisible = bBattleVisible && State->bFollowerJoined;

	if (EnemyLabel)
	{
		EnemyLabel->SetText(FText::FromString(State && State->DungeonNodeIndex >= 4 ? TEXT("Boss") : TEXT("Monster")));
	}

	for (int32 SlotIndex = 0; SlotIndex < EnemySlots.Num(); ++SlotIndex)
	{
		if (EnemySlots[SlotIndex])
		{
			EnemySlots[SlotIndex]->SetVisibility(bBattleVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < PartySlots.Num(); ++SlotIndex)
	{
		const bool bShowSlot = bBattleVisible && (SlotIndex == 0 || bFollowerVisible);
		if (PartySlots[SlotIndex])
		{
			PartySlots[SlotIndex]->SetVisibility(bShowSlot ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (PartyLabels.IsValidIndex(SlotIndex) && PartyLabels[SlotIndex])
		{
			PartyLabels[SlotIndex]->SetText(SlotIndex == 0
				? FText::FromString(TEXT("Hero"))
				: FText::FromString(TEXT("Follower")));
		}
	}
}

UBorder* UGameXXKBattleBoardWidget::CreateSlotBorder(const FString& Name, const FLinearColor& Color, const FText& Label)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*Name));
	UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name + TEXT("Label"))));
	if (!Border || !LabelWidget)
	{
		return Border;
	}

	Border->SetPadding(FMargin(12.0f));
	Border->SetBrushColor(Color);
	LabelWidget->SetJustification(ETextJustify::Center);
	LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LabelWidget->SetFontSize(18);
	LabelWidget->SetText(Label);
	Border->SetContent(LabelWidget);

	if (Name.Contains(TEXT("Enemy")))
	{
		EnemyLabel = LabelWidget;
	}
	else
	{
		PartyLabels.Add(LabelWidget);
	}
	return Border;
}
