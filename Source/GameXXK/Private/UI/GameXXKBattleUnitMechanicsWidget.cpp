#include "UI/GameXXKBattleUnitMechanicsWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKCardTooltipPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FSlateBrush TileBrush(const bool bBack)
	{
		return FSlateRoundedBoxBrush(bBack ? FLinearColor(0.19f,0.24f,0.22f,0.75f) : FLinearColor(0.88f,0.82f,0.7f,0.55f),
			3.0f,FLinearColor(0.19f,0.15f,0.11f,0.65f),0.8f);
	}
	UTextBlock* NumberText(UWidgetTree* Tree, const FString& Text, const int32 Size)
	{
		auto* Result=Tree->ConstructWidget<UTextBlock>();
		Result->SetText(FText::FromString(Text));
		Result->SetFont(FGameXXKInRunUiStyle::OutlinedBodyFont(Size, 1));
		Result->SetColorAndOpacity(FLinearColor(1.0f,0.98f,0.89f));
		Result->SetJustification(ETextJustify::Center);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}
}

void UGameXXKBattleUnitMechanicsWidget::NativeConstruct()
{
	Super::NativeConstruct();EnsureWidgetTree();RebuildPresentation();
}

bool UGameXXKBattleUnitMechanicsWidget::PrepareForBoardEmbedding()
{
	Initialize();EnsureWidgetTree();return Rows!=nullptr;
}

void UGameXXKBattleUnitMechanicsWidget::EnsureWidgetTree()
{
	if (Rows || !WidgetTree) return;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Rows=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("BattleMechanicRows"));
	Rows->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget=Rows;
}

const FGameXXKMechanicTaskView& UGameXXKBattleUnitMechanicsWidget::DisplayedTask() const
{
	return CompletionSeconds>0.0f ? Completion : Current.Task;
}

void UGameXXKBattleUnitMechanicsWidget::SetMechanicView(const FGameXXKUnitMechanicView& View)
{
	PrepareForBoardEmbedding();
	const FString Signature=View.Signature();
	if (bInitializedView && Signature==CurrentSignature) {Current=View;return;}
	const FGameXXKUnitMechanicView Before=Current;
	if (!View.bLiving || !View.Task.bAvailable || View.OwnerUnitId!=Current.OwnerUnitId || View.Task.bActive) CompletionSeconds=0.0f;
	if (bInitializedView && GameXXKBattleMechanicPresentation::CaptureCompletion(Before,View,Completion)) CompletionSeconds=1.15f;
	Current=View;CurrentSignature=Signature;
	RebuildPresentation(bInitializedView ? &Before : nullptr);
	bInitializedView=true;
}

bool UGameXXKBattleUnitMechanicsWidget::MatchesMechanicView(const FGameXXKUnitMechanicView& View) const
{
	return bInitializedView && CurrentSignature==View.Signature();
}

float UGameXXKBattleUnitMechanicsWidget::GetLogicalHeight() const
{
	return Current.LogicalHeight();
}

void UGameXXKBattleUnitMechanicsWidget::ResetPresentation()
{
	bInitializedView=false;CurrentSignature.Reset();CompletionSeconds=0.0f;Pulses.Reset();
}

void UGameXXKBattleUnitMechanicsWidget::RebuildPresentation(const FGameXXKUnitMechanicView* Previous)
{
	if (!Rows) return;
	Rows->ClearChildren();Pulses.Reset();
	SetVisibility(Current.bLiving && Current.bReserveSpace ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!Current.bLiving || !Current.bReserveSpace) return;
	const auto& Task=DisplayedTask();
	auto AddRow=[&]()
	{
		auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(34);
		auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Size->SetContent(Row);Rows->AddChildToVerticalBox(Size)->SetHorizontalAlignment(HAlign_Center);
		return Row;
	};
	struct FCell {USizeBox* Size;UBorder* Hit;UOverlay* Layers;UImage* Art;};
	auto MakeCell=[&](UHorizontalBox* Row,const FVector2D Size,EGameXXKMechanicElement Element,const FString& Title,const FString& Text,const bool bBack)
	{
		auto* Box=WidgetTree->ConstructWidget<USizeBox>();Box->SetWidthOverride(Size.X);Box->SetHeightOverride(Size.Y);
		Box->SetVisibility(ESlateVisibility::Visible);
		auto* Hit=WidgetTree->ConstructWidget<UBorder>();Hit->SetBrush(TileBrush(bBack));Hit->SetPadding(FMargin(1.5f));
		Hit->SetVisibility(ESlateVisibility::Visible);Box->SetContent(Hit);
		Hit->SetToolTip(GameXXKCardTooltipPresentation::BuildCompactTooltip(WidgetTree,FText::FromString(Title),Text));
		auto* Layers=WidgetTree->ConstructWidget<UOverlay>();Layers->SetVisibility(ESlateVisibility::HitTestInvisible);Hit->SetContent(Layers);
		auto* Image=WidgetTree->ConstructWidget<UImage>();Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		Image->SetBrushFromMaterial(LoadObject<UMaterialInterface>(nullptr,*GameXXKBattleMechanicPresentation::MaterialPath(Element)));
		Image->SetDesiredSizeOverride(Size-FVector2D(3));
		auto* ImageSlot=Layers->AddChildToOverlay(Image);ImageSlot->SetHorizontalAlignment(HAlign_Fill);ImageSlot->SetVerticalAlignment(VAlign_Fill);
		auto* Slot=Row->AddChildToHorizontalBox(Box);Slot->SetPadding(FMargin(2,1));Slot->SetVerticalAlignment(VAlign_Center);
		return FCell{Box,Hit,Layers,Image};
	};
	if (Task.bAvailable)
	{
		auto* Row=AddRow();
		auto Main=MakeCell(Row,FVector2D(32),Task.Element,Task.Title,Task.Tooltip,false);
		Main.Size->SetRenderOpacity(Task.bActive || CompletionSeconds>0 ? 1.0f : 0.42f);
		for (const auto& Mark:Task.Cards)
		{
			auto Cell=MakeCell(Row,FVector2D(22,28),Task.Element,Mark.Name,Mark.Tooltip,Mark.PlayOrder==0);
			auto* Back=NumberText(WidgetTree,TEXT("·"),18);
			auto* BackSlot=Cell.Layers->AddChildToOverlay(Back);BackSlot->SetHorizontalAlignment(HAlign_Center);BackSlot->SetVerticalAlignment(VAlign_Center);
			auto* Number=NumberText(WidgetTree,Mark.PlayOrder>0 ? FString::FromInt(Mark.PlayOrder) : FString(),12);
			auto* NumberSlot=Cell.Layers->AddChildToOverlay(Number);NumberSlot->SetHorizontalAlignment(HAlign_Right);NumberSlot->SetVerticalAlignment(VAlign_Top);
			const auto* OldMark=Previous ? Previous->Task.Cards.FindByPredicate([&](const auto& Old){return Old.CardId==Mark.CardId;}) : nullptr;
			const bool bFlip=OldMark && OldMark->PlayOrder==0 && Mark.PlayOrder>0;
			Cell.Art->SetVisibility(Mark.PlayOrder>0 && !bFlip ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Back->SetVisibility(Mark.PlayOrder==0 || bFlip ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Number->SetVisibility(Mark.PlayOrder>0 && !bFlip ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Cell.Size->SetRenderOpacity(Mark.PlayOrder>0 ? 1.0f : 0.48f);
			if (bFlip) Pulses.Add({Cell.Size,Cell.Art,Back,Number,0.0f,true});
		}
	}
	if (!Current.Formulas.IsEmpty())
	{
		auto* Row=AddRow();
		for (const auto& Formula:Current.Formulas)
		{
			auto Cell=MakeCell(Row,FVector2D(32),EGameXXKMechanicElement::Formula,Formula.Title,Formula.Tooltip,false);
			auto* Number=NumberText(WidgetTree,FString::FromInt(Formula.Number),14);
			auto* NumberSlot=Cell.Layers->AddChildToOverlay(Number);NumberSlot->SetHorizontalAlignment(HAlign_Right);NumberSlot->SetVerticalAlignment(VAlign_Top);
			Cell.Size->SetRenderOpacity(Formula.bSpentThisRound ? 0.48f : 1.0f);
			const auto* Old=Previous ? Previous->Formulas.FindByPredicate([&](const auto& Entry){return Entry.CardId==Formula.CardId;}) : nullptr;
			if (Previous && Previous->RoundNumber==Current.RoundNumber && (!Old || Old->TriggerState!=Formula.TriggerState)) Pulses.Add({Cell.Size,nullptr,nullptr,nullptr,0.0f,false});
		}
	}
	if (!Task.bAvailable && Current.Formulas.IsEmpty()) AddRow();
}

void UGameXXKBattleUnitMechanicsWidget::AdvancePresentation(const float DeltaSeconds, const bool bCombatPresentationPending)
{
	if (CompletionSeconds>0 && !bCombatPresentationPending)
	{
		CompletionSeconds=FMath::Max(0.0f,CompletionSeconds-DeltaSeconds);
		if (CompletionSeconds<=0) RebuildPresentation();
	}
	for (int32 Index=Pulses.Num()-1;Index>=0;--Index)
	{
		auto& Pulse=Pulses[Index];auto* Tile=Pulse.Tile.Get();
		if (!Tile) {Pulses.RemoveAtSwap(Index);continue;}
		Pulse.Elapsed+=DeltaSeconds;
		const float T=FMath::Clamp(Pulse.Elapsed/(Pulse.bFlip?0.28f:0.34f),0.0f,1.0f);
		Tile->SetRenderTransformPivot(FVector2D(0.5));
		Tile->SetRenderScale(Pulse.bFlip ? FVector2D(FMath::Max(0.06f,FMath::Abs(FMath::Cos(PI*T))),1.0f) : FVector2D(1.0f+0.12f*FMath::Sin(PI*T)));
		if (Pulse.bFlip && T>=0.5f)
		{
			if (Pulse.Front.IsValid()) Pulse.Front->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (Pulse.Number.IsValid()) Pulse.Number->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (Pulse.Back.IsValid()) Pulse.Back->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (T>=1.0f) {Tile->SetRenderScale(FVector2D(1));Pulses.RemoveAtSwap(Index);}
	}
}

FString UGameXXKBattleUnitMechanicsWidget::GetTaskElementForTest() const {return GameXXKBattleMechanicPresentation::ElementName(DisplayedTask().Element);}
int32 UGameXXKBattleUnitMechanicsWidget::GetTaskCardCountForTest() const {return DisplayedTask().Cards.Num();}
int32 UGameXXKBattleUnitMechanicsWidget::GetTaskCardOrderForTest(const int32 Index) const {return DisplayedTask().Cards.IsValidIndex(Index)?DisplayedTask().Cards[Index].PlayOrder:0;}
int32 UGameXXKBattleUnitMechanicsWidget::GetFormulaCountForTest() const {return Current.Formulas.Num();}
FString UGameXXKBattleUnitMechanicsWidget::GetTaskTooltipForTest() const {return DisplayedTask().Tooltip;}
FString UGameXXKBattleUnitMechanicsWidget::GetFormulaTooltipForTest(const int32 Index) const {return Current.Formulas.IsValidIndex(Index)?Current.Formulas[Index].Tooltip:FString();}
