#include "UnrealBridgeUMGLibrary.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieScenePossessable.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"

// ─── Helpers ────────────────────────────────────────────────

namespace BridgeUMGImpl
{
	UWidgetBlueprint* LoadWBP(const FString& Path)
	{
		UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *Path);
		if (!WBP)
		{
			UE_LOG(LogTemp, Warning, TEXT("UnrealBridge: Could not load Widget Blueprint '%s'"), *Path);
		}
		return WBP;
	}

	FString VisibilityToString(ESlateVisibility V)
	{
		switch (V)
		{
		case ESlateVisibility::Visible:				return TEXT("Visible");
		case ESlateVisibility::Collapsed:			return TEXT("Collapsed");
		case ESlateVisibility::Hidden:				return TEXT("Hidden");
		case ESlateVisibility::HitTestInvisible:	return TEXT("HitTestInvisible");
		case ESlateVisibility::SelfHitTestInvisible:return TEXT("SelfHitTestInvisible");
		default:									return TEXT("Unknown");
		}
	}

	FString PropertyTypeToString(const FProperty* Prop)
	{
		if (!Prop) return TEXT("Unknown");
		return Prop->GetCPPType();
	}

	void GatherWidgets(UWidget* Widget, const FString& ParentName, TArray<FBridgeWidgetInfo>& Out)
	{
		if (!Widget) return;

		FBridgeWidgetInfo Info;
		Info.Name = Widget->GetName();
		Info.WidgetClass = Widget->GetClass()->GetName();
		Info.ParentName = ParentName;
		Info.bIsVariable = Widget->bIsVariable;
		Info.Visibility = VisibilityToString(Widget->GetVisibility());

		if (UPanelSlot* Slot = Widget->Slot)
		{
			Info.SlotType = Slot->GetClass()->GetName();
		}

		Out.Add(Info);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
			{
				GatherWidgets(Panel->GetChildAt(i), Info.Name, Out);
			}
		}
	}

	UWidget* FindWidgetByName(UWidgetBlueprint* WBP, const FString& WidgetName)
	{
		if (!WBP || !WBP->WidgetTree) return nullptr;

		UWidget* Found = nullptr;
		WBP->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if (W && W->GetName() == WidgetName)
			{
				Found = W;
			}
		});
		return Found;
	}

	template <typename TWidget>
	TWidget* ConstructDesignerWidget(UWidgetBlueprint* WBP, const FName& Name, TArray<FString>& RequiredNames)
	{
		if (!WBP || !WBP->WidgetTree) return nullptr;
		TWidget* Widget = WBP->WidgetTree->ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
		if (Widget)
		{
			Widget->bIsVariable = true;
			RequiredNames.Add(Name.ToString());
		}
		return Widget;
	}

	void SetDesignerText(UTextBlock* Text, const FString& Label, int32 FontSize)
	{
		if (!Text) return;
		Text->SetText(FText::FromString(Label));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
	}

	UTextBlock* AddDesignerText(
		UWidgetBlueprint* WBP,
		UVerticalBox* Parent,
		const FName& Name,
		const FString& Label,
		int32 FontSize,
		float Padding,
		TArray<FString>& RequiredNames)
	{
		UTextBlock* Text = ConstructDesignerWidget<UTextBlock>(WBP, Name, RequiredNames);
		SetDesignerText(Text, Label, FontSize);
		if (Parent && Text)
		{
			if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Text))
			{
				Slot->SetPadding(FMargin(Padding));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return Text;
	}

	UButton* AddDesignerButton(
		UWidgetBlueprint* WBP,
		UVerticalBox* Parent,
		const FName& ButtonName,
		const FName& TextName,
		const FString& Label,
		float Padding,
		TArray<FString>& RequiredNames)
	{
		UButton* Button = ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		UTextBlock* Text = ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		SetDesignerText(Text, Label, 20);
		if (Button && Text)
		{
			Button->SetContent(Text);
		}
		if (Parent && Button)
		{
			if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
			{
				Slot->SetPadding(FMargin(Padding));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return Button;
	}

	UButton* AddDesignerIconButton(
		UWidgetBlueprint* WBP,
		UHorizontalBox* Parent,
		const FName& ButtonName,
		const FName& IconName,
		const FName& TextName,
		const FString& Label,
		const FVector2D& Size,
		TArray<FString>& RequiredNames)
	{
		UButton* Button = ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		USizeBox* Box = ConstructDesignerWidget<USizeBox>(
			WBP, FName(*(ButtonName.ToString() + TEXT("Size"))), RequiredNames);
		UOverlay* Stack = ConstructDesignerWidget<UOverlay>(
			WBP, FName(*(ButtonName.ToString() + TEXT("Stack"))), RequiredNames);
		UBorder* Swatch = ConstructDesignerWidget<UBorder>(
			WBP, FName(*(ButtonName.ToString() + TEXT("Swatch"))), RequiredNames);
		UImage* Icon = ConstructDesignerWidget<UImage>(WBP, IconName, RequiredNames);
		UTextBlock* Text = ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		if (!Button || !Box || !Stack || !Swatch || !Icon || !Text) return Button;

		Box->SetWidthOverride(Size.X);
		Box->SetHeightOverride(Size.Y);
		Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		Swatch->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.11f, 0.94f));
		Swatch->SetVisibility(ESlateVisibility::HitTestInvisible);
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		Icon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));
		SetDesignerText(Text, Label, FMath::RoundToInt(Size.Y * 0.42f));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);

		Stack->AddChildToOverlay(Swatch);
		if (UOverlaySlot* IconSlot = Stack->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* TextSlot = Stack->AddChildToOverlay(Text))
		{
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		Box->SetContent(Stack);
		Button->SetContent(Box);
		if (Parent)
		{
			if (UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Button))
			{
				Slot->SetPadding(FMargin(10.0f));
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}
		return Button;
	}

	void AddCanvasWidget(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
	{
		if (!Canvas || !Widget) return;
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
			Slot->SetAutoSize(false);
		}
	}
}

// ─── GetWidgetTree ──────────────────────────────────────────

TArray<FBridgeWidgetInfo> UUnrealBridgeUMGLibrary::GetWidgetTree(const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetInfo> Result;

	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	if (WBP->WidgetTree && WBP->WidgetTree->RootWidget)
	{
		BridgeUMGImpl::GatherWidgets(WBP->WidgetTree->RootWidget, TEXT(""), Result);
	}

	return Result;
}

// ─── GetWidgetProperties ────────────────────────────────────

TArray<FBridgeWidgetPropertyValue> UUnrealBridgeUMGLibrary::GetWidgetProperties(
	const FString& WidgetBlueprintPath, const FString& WidgetName)
{
	TArray<FBridgeWidgetPropertyValue> Result;

	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	UWidget* Widget = BridgeUMGImpl::FindWidgetByName(WBP, WidgetName);
	if (!Widget) return Result;

	UObject* CDO = Widget->GetClass()->GetDefaultObject();
	if (!CDO) return Result;

	for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		// Skip internal/transient properties
		if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
			continue;

		void* WidgetValue = Prop->ContainerPtrToValuePtr<void>(Widget);
		void* CDOValue = Prop->ContainerPtrToValuePtr<void>(CDO);

		if (!Prop->Identical(WidgetValue, CDOValue))
		{
			FBridgeWidgetPropertyValue PV;
			PV.Name = Prop->GetName();
			PV.Type = BridgeUMGImpl::PropertyTypeToString(Prop);

			FString ExportedValue;
			Prop->ExportTextItem_Direct(ExportedValue, WidgetValue, CDOValue, Widget, PPF_None);
			PV.Value = ExportedValue;

			Result.Add(PV);
		}
	}

	return Result;
}

// ─── GetWidgetAnimations ────────────────────────────────────

TArray<FBridgeWidgetAnimationInfo> UUnrealBridgeUMGLibrary::GetWidgetAnimations(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetAnimationInfo> Result;

	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	for (UWidgetAnimation* Anim : WBP->Animations)
	{
		if (!Anim) continue;

		FBridgeWidgetAnimationInfo Info;
#if WITH_EDITOR
		Info.Name = Anim->GetDisplayLabel();
		if (Info.Name.IsEmpty())
#endif
		{
			Info.Name = Anim->GetName();
		}

		Info.Duration = Anim->GetEndTime() - Anim->GetStartTime();

		UMovieScene* Scene = Anim->GetMovieScene();
		if (Scene)
		{
			// Build Guid -> widget name map from animation bindings
			TMap<FGuid, FString> GuidToWidget;
			for (const FWidgetAnimationBinding& Binding : Anim->AnimationBindings)
			{
				GuidToWidget.Add(Binding.AnimationGuid, Binding.WidgetName.ToString());
			}

			const TArray<FMovieSceneBinding>& Bindings = const_cast<const UMovieScene*>(Scene)->GetBindings();
			for (const FMovieSceneBinding& Binding : Bindings)
			{
				FString TargetWidget;
				// Find possessable to get bound widget name
				FMovieScenePossessable* Possessable = Scene->FindPossessable(Binding.GetObjectGuid());
				if (Possessable)
				{
					TargetWidget = Possessable->GetName();
				}
				// Fallback: look up via animation binding guid
				if (TargetWidget.IsEmpty())
				{
					if (FString* Found = GuidToWidget.Find(Binding.GetObjectGuid()))
					{
						TargetWidget = *Found;
					}
				}

				for (UMovieSceneTrack* Track : Binding.GetTracks())
				{
					if (!Track) continue;

					FBridgeWidgetAnimTrack TrackInfo;
					TrackInfo.WidgetName = TargetWidget;
					TrackInfo.TrackType = Track->GetClass()->GetName();
					TrackInfo.DisplayName = Track->GetDisplayName().ToString();
					Info.Tracks.Add(TrackInfo);
				}
			}

			// Master/global tracks (not bound to a widget)
			for (UMovieSceneTrack* Track : Scene->GetTracks())
			{
				if (!Track) continue;

				FBridgeWidgetAnimTrack TrackInfo;
				TrackInfo.TrackType = Track->GetClass()->GetName();
				TrackInfo.DisplayName = Track->GetDisplayName().ToString();
				Info.Tracks.Add(TrackInfo);
			}
		}

		Result.Add(Info);
	}

	return Result;
}

// ─── GetWidgetBindings ──────────────────────────────────────

TArray<FBridgeWidgetBindingInfo> UUnrealBridgeUMGLibrary::GetWidgetBindings(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetBindingInfo> Result;

	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	for (const FDelegateEditorBinding& Binding : WBP->Bindings)
	{
		FBridgeWidgetBindingInfo Info;
		Info.WidgetName = Binding.ObjectName;
		Info.PropertyName = Binding.PropertyName.ToString();
		Info.FunctionName = Binding.FunctionName.ToString();

		if (Info.FunctionName.IsEmpty() && !Binding.SourceProperty.IsNone())
		{
			Info.FunctionName = Binding.SourceProperty.ToString();
		}

		Info.Kind = (Binding.Kind == EBindingKind::Function) ? TEXT("Function") : TEXT("Property");

		Result.Add(Info);
	}

	return Result;
}

// ─── GetWidgetEvents ────────────────────────────────────────

TArray<FBridgeWidgetEventInfo> UUnrealBridgeUMGLibrary::GetWidgetEvents(
	const FString& WidgetBlueprintPath)
{
	TArray<FBridgeWidgetEventInfo> Result;

	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return Result;

	// Scan event graph for component bound event nodes (OnClicked, etc.)
	for (UEdGraph* Graph : WBP->UbergraphPages)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_ComponentBoundEvent* EventNode = Cast<UK2Node_ComponentBoundEvent>(Node))
			{
				FBridgeWidgetEventInfo Info;
				Info.WidgetName = EventNode->ComponentPropertyName.ToString();
				Info.EventName = EventNode->DelegatePropertyName.ToString();
				Info.HandlerName = EventNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
				Result.Add(Info);
			}
		}
	}

	return Result;
}

// ─── SearchWidgets ──────────────────────────────────────────

TArray<FBridgeWidgetInfo> UUnrealBridgeUMGLibrary::SearchWidgets(
	const FString& WidgetBlueprintPath, const FString& Query)
{
	TArray<FBridgeWidgetInfo> All = GetWidgetTree(WidgetBlueprintPath);
	TArray<FBridgeWidgetInfo> Result;

	FString Q = Query.ToLower();
	for (const FBridgeWidgetInfo& W : All)
	{
		if (W.Name.ToLower().Contains(Q) || W.WidgetClass.ToLower().Contains(Q))
		{
			Result.Add(W);
		}
	}

	return Result;
}

// ─── SetWidgetProperty ──────────────────────────────────────

bool UUnrealBridgeUMGLibrary::SetWidgetProperty(
	const FString& WidgetBlueprintPath, const FString& WidgetName,
	const FString& PropertyName, const FString& Value)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP) return false;

	UWidget* Widget = BridgeUMGImpl::FindWidgetByName(WBP, WidgetName);
	if (!Widget) return false;

	FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop) return false;

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
	if (!Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None))
		return false;

	WBP->MarkPackageDirty();
	return true;
}

bool UUnrealBridgeUMGLibrary::RebuildBallSelectDesignerWidget(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	TArray<FString> RequiredNames;
	WBP->Modify();
	WBP->WidgetTree->Modify();

	if (WBP->WidgetTree->RootWidget)
	{
		WBP->WidgetTree->RemoveWidget(WBP->WidgetTree->RootWidget);
	}

	UCanvasPanel* Root = BridgeUMGImpl::ConstructDesignerWidget<UCanvasPanel>(
		WBP, FName(TEXT("BallSelectDesignerRoot")), RequiredNames);
	if (!Root) return false;
	WBP->WidgetTree->RootWidget = Root;

	UBorder* Panel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
		WBP, FName(TEXT("BallSelectPanel")), RequiredNames);
	if (!Panel) return false;
	Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.82f));
	BridgeUMGImpl::AddCanvasWidget(Root, Panel, FVector2D(40.0f, 80.0f), FVector2D(520.0f, 650.0f), 1);

	UScrollBox* Scroll = BridgeUMGImpl::ConstructDesignerWidget<UScrollBox>(
		WBP, FName(TEXT("BallSelectScroll")), RequiredNames);
	UVerticalBox* Column = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
		WBP, FName(TEXT("BallSelectColumn")), RequiredNames);
	if (!Scroll || !Column) return false;
	Panel->SetContent(Scroll);
	Scroll->AddChild(Column);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("TitleText")), TEXT("Ball Loadout"), 30, 8.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("SelectionText")), TEXT("Loadout: 4 slots"), 18, 4.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("PrimaryActionButton")), FName(TEXT("PrimaryActionButtonText")), TEXT("OK"), 8.0f, RequiredNames);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("BallBackpackTitleText")), TEXT("Ball Backpack"), 20, 10.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("BallPackToggleButton")), FName(TEXT("BallPackToggleText")), TEXT("Ball Pack (17) Collapse"), 5.0f, RequiredNames);
	const TArray<FString> BallLabels = {
		TEXT("VegetableBall"),
		TEXT("ShieldDogBall"),
		TEXT("BasicBall"),
		TEXT("HasakiBall"),
		TEXT("LoveRobotBall"),
		TEXT("DeathRobotBall"),
		TEXT("ProtonBall"),
	};
	for (int32 Index = 0; Index < BallLabels.Num(); ++Index)
	{
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("BallBackpack_%d"), Index)),
			FName(*FString::Printf(TEXT("BallBackpack_%dText"), Index)),
			BallLabels[Index],
			5.0f,
			RequiredNames);
	}

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("PreviewTitleText")), TEXT("Preview"), 20, 10.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(
		WBP,
		Column,
		FName(TEXT("PreviewText")),
		TEXT("Preview: VegetableBall | Bead: None | Yaw: 0.0"),
		18,
		4.0f,
		RequiredNames);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("LoadoutSlotsTitleText")), TEXT("Loadout Slots"), 20, 10.0f, RequiredNames);
	const TArray<FString> SlotLabels = {
		TEXT("VegetableBall"),
		TEXT("VegetableBall"),
		TEXT("ShieldDogBall"),
		TEXT("HasakiBall"),
	};
	for (int32 Index = 0; Index < SlotLabels.Num(); ++Index)
	{
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("LoadoutSlot_%d"), Index)),
			FName(*FString::Printf(TEXT("LoadoutSlot_%dText"), Index)),
			FString::Printf(TEXT("Slot %d: %s"), Index + 1, *SlotLabels[Index]),
			5.0f,
			RequiredNames);
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("BeadSlot_%d"), Index)),
			FName(*FString::Printf(TEXT("BeadSlot_%dText"), Index)),
			FString::Printf(TEXT("Bead %d: Empty"), Index + 1),
			5.0f,
			RequiredNames);
	}

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("BeadBackpackTitleText")), TEXT("Bead Backpack"), 20, 10.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("BeadBackpack_0")), FName(TEXT("BeadBackpack_0Text")), TEXT("CentroidBead"), 5.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ConfigText")), TEXT("Config: Folded"), 16, 8.0f, RequiredNames);

	UButton* RightConfirmButton = BridgeUMGImpl::ConstructDesignerWidget<UButton>(
		WBP, FName(TEXT("RightConfirmButton")), RequiredNames);
	UTextBlock* RightConfirmText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
		WBP, FName(TEXT("RightConfirmButtonText")), RequiredNames);
	BridgeUMGImpl::SetDesignerText(RightConfirmText, TEXT("OK"), 20);
	if (RightConfirmButton && RightConfirmText)
	{
		RightConfirmButton->SetContent(RightConfirmText);
		BridgeUMGImpl::AddCanvasWidget(Root, RightConfirmButton, FVector2D(600.0f, 136.0f), FVector2D(120.0f, 48.0f), 20);
	}

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_BallSelectDesignerAssetControls: actual=%d expected=%d result=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

bool UUnrealBridgeUMGLibrary::EnsureBallSelectSynthesisDesignerControls(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	WBP->Modify();
	WBP->WidgetTree->Modify();

	TArray<FString> CreatedNames;
	const TArray<FString> RequiredNames = {
		TEXT("SynthesisModeButton"),
		TEXT("SynthesisModeButtonText"),
		TEXT("SynthesisClearButton"),
		TEXT("SynthesisClearButtonText"),
		TEXT("SynthesisExecuteButton"),
		TEXT("SynthesisExecuteButtonText"),
		TEXT("SynthesisStatusText"),
		TEXT("SynthesisSlot_0"),
		TEXT("SynthesisSlot_0Text"),
		TEXT("SynthesisSlot_1"),
		TEXT("SynthesisSlot_1Text"),
		TEXT("SynthesisSlot_2"),
		TEXT("SynthesisSlot_2Text"),
		TEXT("SynthesisSlot_3"),
		TEXT("SynthesisSlot_3Text"),
	};

	UCanvasPanel* Root = Cast<UCanvasPanel>(WBP->WidgetTree->RootWidget);
	UVerticalBox* Column = Cast<UVerticalBox>(BridgeUMGImpl::FindWidgetByName(WBP, TEXT("BallSelectColumn")));
	const bool bUseCanvasLayout = Root && BridgeUMGImpl::FindWidgetByName(WBP, TEXT("CarrySlot_0")) != nullptr;

	auto EnsureButtonContent = [&](UButton* Button, UTextBlock*& Text, const FName& TextName, const FString& Label, int32 FontSize)
	{
		if (!Text)
		{
			Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, CreatedNames);
		}
		BridgeUMGImpl::SetDesignerText(Text, Label, FontSize);
		if (Button && Text)
		{
			Button->SetContent(Text);
		}
	};

	auto EnsureVerticalButton = [&](const FName& ButtonName, const FName& TextName, const FString& Label)
	{
		UButton* Button = Cast<UButton>(BridgeUMGImpl::FindWidgetByName(WBP, ButtonName.ToString()));
		UTextBlock* Text = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, TextName.ToString()));
		if (!Button)
		{
			Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, CreatedNames);
			if (Column && Button)
			{
				if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Button))
				{
					Slot->SetPadding(FMargin(18.0f, 6.0f, 18.0f, 2.0f));
					Slot->SetHorizontalAlignment(HAlign_Fill);
				}
			}
		}
		EnsureButtonContent(Button, Text, TextName, Label, 20);
	};

	auto EnsureCanvasButton = [&](const FName& ButtonName, const FName& TextName, const FString& Label, const FVector2D& Position, const FVector2D& Size)
	{
		UButton* Button = Cast<UButton>(BridgeUMGImpl::FindWidgetByName(WBP, ButtonName.ToString()));
		UTextBlock* Text = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, TextName.ToString()));
		if (!Button)
		{
			Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, CreatedNames);
			BridgeUMGImpl::AddCanvasWidget(Root, Button, Position, Size, 45);
		}
		EnsureButtonContent(Button, Text, TextName, Label, 15);
	};

	if (bUseCanvasLayout)
	{
		EnsureCanvasButton(
			FName(TEXT("SynthesisModeButton")),
			FName(TEXT("SynthesisModeButtonText")),
			TEXT("进入合成"),
			FVector2D(1358.0f, 856.0f),
			FVector2D(190.0f, 38.0f));
		EnsureCanvasButton(
			FName(TEXT("SynthesisClearButton")),
			FName(TEXT("SynthesisClearButtonText")),
			TEXT("清空材料"),
			FVector2D(1358.0f, 900.0f),
			FVector2D(92.0f, 34.0f));
		EnsureCanvasButton(
			FName(TEXT("SynthesisExecuteButton")),
			FName(TEXT("SynthesisExecuteButtonText")),
			TEXT("执行合成"),
			FVector2D(1456.0f, 900.0f),
			FVector2D(92.0f, 34.0f));
		UTextBlock* StatusText = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, TEXT("SynthesisStatusText")));
		if (!StatusText)
		{
			StatusText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, FName(TEXT("SynthesisStatusText")), CreatedNames);
			BridgeUMGImpl::AddCanvasWidget(Root, StatusText, FVector2D(1358.0f, 940.0f), FVector2D(280.0f, 28.0f), 45);
		}
		BridgeUMGImpl::SetDesignerText(StatusText, TEXT("合成：选择4个背包球作为材料"), 14);
		for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
		{
			EnsureCanvasButton(
				FName(*FString::Printf(TEXT("SynthesisSlot_%d"), SlotIndex)),
				FName(*FString::Printf(TEXT("SynthesisSlot_%dText"), SlotIndex)),
				FString::Printf(TEXT("材料槽 %d：空"), SlotIndex + 1),
				FVector2D(1358.0f, 704.0f + SlotIndex * 36.0f),
				FVector2D(280.0f, 32.0f));
		}
	}
	else
	{
		if (!Column && Root)
		{
			Column = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(WBP, FName(TEXT("BallSelectColumn")), CreatedNames);
			BridgeUMGImpl::AddCanvasWidget(Root, Column, FVector2D(40.0f, 80.0f), FVector2D(520.0f, 700.0f), 10);
		}
		if (Column)
		{
			EnsureVerticalButton(FName(TEXT("SynthesisModeButton")), FName(TEXT("SynthesisModeButtonText")), TEXT("进入合成"));
			for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
			{
				EnsureVerticalButton(
					FName(*FString::Printf(TEXT("SynthesisSlot_%d"), SlotIndex)),
					FName(*FString::Printf(TEXT("SynthesisSlot_%dText"), SlotIndex)),
					FString::Printf(TEXT("材料槽 %d：空"), SlotIndex + 1));
			}
			EnsureVerticalButton(FName(TEXT("SynthesisClearButton")), FName(TEXT("SynthesisClearButtonText")), TEXT("清空材料"));
			EnsureVerticalButton(FName(TEXT("SynthesisExecuteButton")), FName(TEXT("SynthesisExecuteButtonText")), TEXT("执行合成"));
			UTextBlock* StatusText = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, TEXT("SynthesisStatusText")));
			if (!StatusText)
			{
				StatusText = BridgeUMGImpl::AddDesignerText(
					WBP,
					Column,
					FName(TEXT("SynthesisStatusText")),
					TEXT("合成：选择4个背包球作为材料"),
					16,
					6.0f,
					CreatedNames);
			}
			else
			{
				BridgeUMGImpl::SetDesignerText(StatusText, TEXT("合成：选择4个背包球作为材料"), 16);
			}
		}
	}

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_BallSelectSynthesisDesignerControls: actual=%d expected=%d result=%s asset=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"),
		*WidgetBlueprintPath);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

bool UUnrealBridgeUMGLibrary::RebuildPegInventoryDesignerWidget(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	TArray<FString> RequiredNames;
	WBP->Modify();
	WBP->WidgetTree->Modify();

	if (WBP->WidgetTree->RootWidget)
	{
		WBP->WidgetTree->RemoveWidget(WBP->WidgetTree->RootWidget);
	}

	UCanvasPanel* Root = BridgeUMGImpl::ConstructDesignerWidget<UCanvasPanel>(
		WBP, FName(TEXT("PegInventoryDesignerRoot")), RequiredNames);
	if (!Root) return false;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WBP->WidgetTree->RootWidget = Root;

	UTextBlock* HeaderText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
		WBP, FName(TEXT("HeaderText")), RequiredNames);
	BridgeUMGImpl::SetDesignerText(HeaderText, TEXT("Peg Backpack"), 22);
	if (HeaderText)
	{
		HeaderText->SetVisibility(ESlateVisibility::HitTestInvisible);
		BridgeUMGImpl::AddCanvasWidget(Root, HeaderText, FVector2D(44.0f, 610.0f), FVector2D(260.0f, 34.0f), 2);
	}

	UBorder* DraftPanel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
		WBP, FName(TEXT("DraftPanel")), RequiredNames);
	if (!DraftPanel) return false;
	DraftPanel->SetBrushColor(FLinearColor(0.025f, 0.026f, 0.032f, 0.42f));
	DraftPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BridgeUMGImpl::AddCanvasWidget(Root, DraftPanel, FVector2D(418.0f, 280.0f), FVector2D(444.0f, 158.0f), 10);

	UHorizontalBox* DraftIconRow = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
		WBP, FName(TEXT("DraftIconRow")), RequiredNames);
	if (!DraftIconRow) return false;
	DraftIconRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DraftPanel->SetContent(DraftIconRow);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, DraftIconRow, FName(TEXT("DraftChoice0Button")), FName(TEXT("DraftChoice0Icon")),
		FName(TEXT("DraftChoice0Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, DraftIconRow, FName(TEXT("DraftChoice1Button")), FName(TEXT("DraftChoice1Icon")),
		FName(TEXT("DraftChoice1Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, DraftIconRow, FName(TEXT("DraftChoice2Button")), FName(TEXT("DraftChoice2Icon")),
		FName(TEXT("DraftChoice2Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);

	UBorder* BackpackPanel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
		WBP, FName(TEXT("BackpackPanel")), RequiredNames);
	if (!BackpackPanel) return false;
	BackpackPanel->SetBrushColor(FLinearColor(0.018f, 0.020f, 0.025f, 0.58f));
	BackpackPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BridgeUMGImpl::AddCanvasWidget(Root, BackpackPanel, FVector2D(406.0f, 596.0f), FVector2D(468.0f, 108.0f), 11);

	UHorizontalBox* BackpackIconRow = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
		WBP, FName(TEXT("BackpackIconRow")), RequiredNames);
	if (!BackpackIconRow) return false;
	BackpackIconRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackpackPanel->SetContent(BackpackIconRow);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, BackpackIconRow, FName(TEXT("Backpack0Button")), FName(TEXT("Backpack0Icon")),
		FName(TEXT("Backpack0Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, BackpackIconRow, FName(TEXT("Backpack1Button")), FName(TEXT("Backpack1Icon")),
		FName(TEXT("Backpack1Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, BackpackIconRow, FName(TEXT("Backpack2Button")), FName(TEXT("Backpack2Icon")),
		FName(TEXT("Backpack2Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
	BridgeUMGImpl::AddDesignerIconButton(
		WBP, BackpackIconRow, FName(TEXT("Backpack3Button")), FName(TEXT("Backpack3Icon")),
		FName(TEXT("Backpack3Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);

	UButton* ConfirmButton = BridgeUMGImpl::ConstructDesignerWidget<UButton>(
		WBP, FName(TEXT("ConfirmButton")), RequiredNames);
	UTextBlock* ConfirmText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
		WBP, FName(TEXT("ConfirmText")), RequiredNames);
	BridgeUMGImpl::SetDesignerText(ConfirmText, TEXT("OK"), 20);
	if (ConfirmButton && ConfirmText)
	{
		ConfirmButton->SetContent(ConfirmText);
		BridgeUMGImpl::AddCanvasWidget(Root, ConfirmButton, FVector2D(1164.0f, 592.0f), FVector2D(72.0f, 36.0f), 20);
	}

	UButton* CancelButton = BridgeUMGImpl::ConstructDesignerWidget<UButton>(
		WBP, FName(TEXT("CancelButton")), RequiredNames);
	UTextBlock* CancelText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
		WBP, FName(TEXT("CancelText")), RequiredNames);
	BridgeUMGImpl::SetDesignerText(CancelText, TEXT("X"), 20);
	if (CancelButton && CancelText)
	{
		CancelButton->SetContent(CancelText);
		BridgeUMGImpl::AddCanvasWidget(Root, CancelButton, FVector2D(1164.0f, 636.0f), FVector2D(72.0f, 36.0f), 20);
	}

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_PegInventoryDesignerAssetControls: actual=%d expected=%d result=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

bool UUnrealBridgeUMGLibrary::RebuildShopDesignerWidget(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	TArray<FString> RequiredNames;
	WBP->Modify();
	WBP->WidgetTree->Modify();

	if (WBP->WidgetTree->RootWidget)
	{
		WBP->WidgetTree->RemoveWidget(WBP->WidgetTree->RootWidget);
	}

	UCanvasPanel* Root = BridgeUMGImpl::ConstructDesignerWidget<UCanvasPanel>(
		WBP, FName(TEXT("ShopDesignerRoot")), RequiredNames);
	if (!Root) return false;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WBP->WidgetTree->RootWidget = Root;

	UBorder* Panel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
		WBP, FName(TEXT("ShopPanel")), RequiredNames);
	if (!Panel) return false;
	Panel->SetBrushColor(FLinearColor(0.018f, 0.020f, 0.025f, 0.82f));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BridgeUMGImpl::AddCanvasWidget(Root, Panel, FVector2D(1120.0f, 84.0f), FVector2D(430.0f, 780.0f), 50);

	UScrollBox* Scroll = BridgeUMGImpl::ConstructDesignerWidget<UScrollBox>(
		WBP, FName(TEXT("ShopScroll")), RequiredNames);
	UVerticalBox* Column = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
		WBP, FName(TEXT("ShopColumn")), RequiredNames);
	if (!Scroll || !Column) return false;
	Panel->SetContent(Scroll);
	Scroll->AddChild(Column);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopTitleText")), TEXT("SHOP"), 28, 8.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopGoldText")), TEXT("SHOP 0"), 20, 4.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopScoreText")), TEXT("SCORE 0"), 20, 4.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopWaveKindText")), TEXT("Next Wave 5: Elite"), 20, 4.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopStatusText")), TEXT("Shop"), 18, 6.0f, RequiredNames);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("RunBallPoolTitleText")), TEXT("PLAYER BALL POOL"), 20, 8.0f, RequiredNames);
	for (int32 Index = 0; Index < 24; ++Index)
	{
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("RunBall_%dButton"), Index)),
			FName(*FString::Printf(TEXT("RunBall_%dText"), Index)),
			FString::Printf(TEXT("Run Ball %d"), Index + 1),
			4.0f,
			RequiredNames);
	}
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("DeleteBallButton")), FName(TEXT("DeleteBallText")), TEXT("Delete 50"), 8.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("ForgeBallButton")), FName(TEXT("ForgeBallText")), TEXT("Forge 150"), 8.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ForgePillChoiceTitleText")), TEXT("CHOOSE FORGE PILL"), 18, 6.0f, RequiredNames);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("ForgePillChoice_%dButton"), Index)),
			FName(*FString::Printf(TEXT("ForgePillChoice_%dText"), Index)),
			FString::Printf(TEXT("Pill %d"), Index + 1),
			4.0f,
			RequiredNames);
	}
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("RefreshShopButton")), FName(TEXT("RefreshShopText")), TEXT("Refresh 150"), 8.0f, RequiredNames);
	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("BuyCarrySlotUpgradeButton")), FName(TEXT("BuyCarrySlotUpgradeText")), TEXT("Carry Slot +1 10000"), 8.0f, RequiredNames);

	BridgeUMGImpl::AddDesignerText(WBP, Column, FName(TEXT("ShopOfferTitleText")), TEXT("OFFERS"), 20, 8.0f, RequiredNames);
	UEditableTextBox* SearchBox = BridgeUMGImpl::ConstructDesignerWidget<UEditableTextBox>(
		WBP, FName(TEXT("ShopSearchBox")), RequiredNames);
	if (SearchBox)
	{
		SearchBox->SetHintText(FText::FromString(TEXT("Search")));
		if (UVerticalBoxSlot* SearchSlot = Column->AddChildToVerticalBox(SearchBox))
		{
			SearchSlot->SetPadding(FMargin(8.0f));
			SearchSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	const TArray<FString> OfferLabels = {
		TEXT("VegetableBall 200"),
		TEXT("ShieldDogBall 200"),
		TEXT("HasakiBall 200"),
		TEXT("ProtonBall 200"),
		TEXT("LoveRobotBall 200"),
		TEXT("DeathRobotBall 200"),
		TEXT("MoneyBagBall 200"),
		TEXT("BountyBall 200"),
	};
	for (int32 Index = 0; Index < OfferLabels.Num(); ++Index)
	{
		BridgeUMGImpl::AddDesignerButton(
			WBP,
			Column,
			FName(*FString::Printf(TEXT("ShopOffer_%dButton"), Index)),
			FName(*FString::Printf(TEXT("ShopOffer_%dText"), Index)),
			OfferLabels[Index],
			5.0f,
			RequiredNames);
	}

	BridgeUMGImpl::AddDesignerButton(
		WBP, Column, FName(TEXT("ShopContinueButton")), FName(TEXT("ShopContinueText")), TEXT("Continue"), 8.0f, RequiredNames);

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_ShopDesignerAssetControls: actual=%d expected=%d result=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

bool UUnrealBridgeUMGLibrary::EnsureShopForgePillDesignerControls(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	UVerticalBox* Column = Cast<UVerticalBox>(BridgeUMGImpl::FindWidgetByName(WBP, TEXT("ShopColumn")));
	if (!Column)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TDD] UI_ShopForgePillDesignerColumnFound: actual=0 expected=1 result=FAIL asset=%s"), *WidgetBlueprintPath);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_ShopForgePillDesignerColumnFound: actual=1 expected=1 result=PASS asset=%s"), *WidgetBlueprintPath);

	TArray<FString> CreatedNames;
	WBP->Modify();
	WBP->WidgetTree->Modify();
	Column->Modify();

	auto EnsureColumnChild = [Column](UWidget* Widget, const FMargin& Padding)
	{
		if (!Widget || !Column) return;
		if (Widget->GetParent() != Column)
		{
			if (UPanelWidget* Parent = Widget->GetParent())
			{
				Parent->RemoveChild(Widget);
			}
			if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Widget))
			{
				Slot->SetPadding(Padding);
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		else if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	auto EnsureText = [&](const FName& Name, const FString& Label, int32 FontSize, const FMargin& Padding) -> UTextBlock*
	{
		UTextBlock* Text = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, Name.ToString()));
		if (!Text)
		{
			Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, Name, CreatedNames);
		}
		BridgeUMGImpl::SetDesignerText(Text, Label, FontSize);
		if (Text)
		{
			Text->SetVisibility(ESlateVisibility::Visible);
			EnsureColumnChild(Text, Padding);
		}
		return Text;
	};

	auto EnsureButton = [&](const FName& ButtonName, const FName& TextName, const FString& Label, const FMargin& Padding) -> UButton*
	{
		UButton* Button = Cast<UButton>(BridgeUMGImpl::FindWidgetByName(WBP, ButtonName.ToString()));
		if (!Button)
		{
			Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, CreatedNames);
		}
		UTextBlock* Text = Cast<UTextBlock>(BridgeUMGImpl::FindWidgetByName(WBP, TextName.ToString()));
		if (!Text)
		{
			Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, CreatedNames);
		}
		BridgeUMGImpl::SetDesignerText(Text, Label, 18);
		if (Button && Text && Button->GetContent() != Text)
		{
			if (UPanelWidget* Parent = Text->GetParent())
			{
				Parent->RemoveChild(Text);
			}
			Button->SetContent(Text);
		}
		if (Button)
		{
			Button->SetVisibility(ESlateVisibility::Visible);
			EnsureColumnChild(Button, Padding);
		}
		return Button;
	};

	auto MoveWidgetAfter = [Column](UWidget* Widget, const UWidget* Anchor)
	{
		if (!Widget || !Anchor || !Column) return;
		const int32 AnchorIndex = Column->GetChildIndex(Anchor);
		const int32 WidgetIndex = Column->GetChildIndex(Widget);
		if (AnchorIndex != INDEX_NONE && WidgetIndex != INDEX_NONE && WidgetIndex != AnchorIndex + 1)
		{
			Column->ShiftChild(AnchorIndex + 1, Widget);
		}
	};

	UWidget* ForgeButton = BridgeUMGImpl::FindWidgetByName(WBP, TEXT("ForgeBallButton"));
	UTextBlock* ChoiceTitle = EnsureText(TEXT("ForgePillChoiceTitleText"), TEXT("CHOOSE FORGE PILL"), 18, FMargin(8.0f, 6.0f));
	MoveWidgetAfter(ChoiceTitle, ForgeButton);

	UWidget* Anchor = ChoiceTitle;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UButton* Button = EnsureButton(
			FName(*FString::Printf(TEXT("ForgePillChoice_%dButton"), Index)),
			FName(*FString::Printf(TEXT("ForgePillChoice_%dText"), Index)),
			FString::Printf(TEXT("Pill %d"), Index + 1),
			FMargin(8.0f, 4.0f));
		MoveWidgetAfter(Button, Anchor);
		Anchor = Button;
	}

	UButton* RefreshButton = EnsureButton(
		TEXT("RefreshShopButton"),
		TEXT("RefreshShopText"),
		TEXT("Refresh 150"),
		FMargin(8.0f, 8.0f));
	MoveWidgetAfter(RefreshButton, Anchor);

	UButton* CarryButton = EnsureButton(
		TEXT("BuyCarrySlotUpgradeButton"),
		TEXT("BuyCarrySlotUpgradeText"),
		TEXT("Carry Slot +1 10000"),
		FMargin(8.0f, 4.0f));
	MoveWidgetAfter(CarryButton, RefreshButton);

	const TArray<FString> RequiredNames = {
		TEXT("ForgePillChoiceTitleText"),
		TEXT("ForgePillChoice_0Button"),
		TEXT("ForgePillChoice_0Text"),
		TEXT("ForgePillChoice_1Button"),
		TEXT("ForgePillChoice_1Text"),
		TEXT("ForgePillChoice_2Button"),
		TEXT("ForgePillChoice_2Text"),
		TEXT("RefreshShopButton"),
		TEXT("RefreshShopText"),
		TEXT("BuyCarrySlotUpgradeButton"),
		TEXT("BuyCarrySlotUpgradeText"),
	};

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_ShopForgePillDesignerControls: actual=%d expected=%d result=%s created=%d asset=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"),
		CreatedNames.Num(),
		*WidgetBlueprintPath);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

bool UUnrealBridgeUMGLibrary::RebuildTooltipPanelDesignerWidget(const FString& WidgetBlueprintPath)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree) return false;

	TArray<FString> RequiredNames;
	WBP->Modify();
	WBP->WidgetTree->Modify();

	if (WBP->WidgetTree->RootWidget)
	{
		WBP->WidgetTree->RemoveWidget(WBP->WidgetTree->RootWidget);
	}

	UCanvasPanel* Root = BridgeUMGImpl::ConstructDesignerWidget<UCanvasPanel>(
		WBP, FName(TEXT("TooltipRoot")), RequiredNames);
	if (!Root) return false;
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WBP->WidgetTree->RootWidget = Root;

	UBorder* Panel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
		WBP, FName(TEXT("TooltipPanel")), RequiredNames);
	if (!Panel) return false;
	Panel->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.025f, 0.88f));
	Panel->SetPadding(FMargin(12.0f, 10.0f, 12.0f, 10.0f));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BridgeUMGImpl::AddCanvasWidget(Root, Panel, FVector2D(18.0f, 18.0f), FVector2D(380.0f, 220.0f), 70);

	UVerticalBox* Body = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
		WBP, FName(TEXT("TooltipBody")), RequiredNames);
	if (!Body) return false;
	Body->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Panel->SetContent(Body);

	UTextBlock* Title = BridgeUMGImpl::AddDesignerText(
		WBP, Body, FName(TEXT("TooltipTitleText")), TEXT("VegetableBall"), 30, 0.0f, RequiredNames);
	if (Title)
	{
		Title->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Title->GetFont();
		Font.TypefaceFontName = FName(TEXT("Bold"));
		Title->SetFont(Font);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f)));
	}

	UVerticalBox* RowsBox = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
		WBP, FName(TEXT("TooltipRowsBox")), RequiredNames);
	if (!RowsBox) return false;
	UTextBlock* DetailBody = BridgeUMGImpl::AddDesignerText(
		WBP, Body, FName(TEXT("TooltipDetailBodyText")), TEXT(""), 20, 0.0f, RequiredNames);
	if (DetailBody)
	{
		DetailBody->SetVisibility(ESlateVisibility::Collapsed);
		DetailBody->SetAutoWrapText(true);
	}

	UWrapBox* DescriptionPillBox = BridgeUMGImpl::ConstructDesignerWidget<UWrapBox>(
		WBP, FName(TEXT("TooltipDescriptionPillBox")), RequiredNames);
	if (!DescriptionPillBox) return false;
	DescriptionPillBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* DescriptionSlot = Body->AddChildToVerticalBox(DescriptionPillBox))
	{
		DescriptionSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));
		DescriptionSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	RowsBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* Slot = Body->AddChildToVerticalBox(RowsBox))
	{
		Slot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	UHorizontalBox* Footer = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
		WBP, FName(TEXT("TooltipFooterBox")), RequiredNames);
	if (!Footer) return false;
	Footer->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* FooterSlot = Body->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		FooterSlot->SetHorizontalAlignment(HAlign_Right);
	}

	UButton* CloseButton = BridgeUMGImpl::ConstructDesignerWidget<UButton>(
		WBP, FName(TEXT("TooltipCloseButton")), RequiredNames);
	UTextBlock* CloseText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
		WBP, FName(TEXT("TooltipCloseText")), RequiredNames);
	BridgeUMGImpl::SetDesignerText(CloseText, TEXT("X"), 18);
	if (CloseButton && CloseText)
	{
		CloseButton->SetContent(CloseText);
		if (UHorizontalBoxSlot* CloseButtonSlot = Footer->AddChildToHorizontalBox(CloseButton))
		{
			CloseButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	int32 FoundCount = 0;
	for (const FString& Name : RequiredNames)
	{
		if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
		{
			++FoundCount;
		}
	}

	const bool bSuccess = FoundCount == RequiredNames.Num();
	UE_LOG(LogTemp, Log, TEXT("[TDD] UI_TooltipPanelDesignerAssetControls: actual=%d expected=%d result=%s"),
		FoundCount,
		RequiredNames.Num(),
		bSuccess ? TEXT("PASS") : TEXT("FAIL"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	FKismetEditorUtilities::CompileBlueprint(WBP);
	WBP->MarkPackageDirty();
	return bSuccess;
}

namespace
{
	bool ResetVendingTree(
		UWidgetBlueprint* WBP,
		const FName& RootName,
		TArray<FString>& RequiredNames,
		UCanvasPanel*& OutRoot)
	{
		OutRoot = nullptr;
		if (!WBP || !WBP->WidgetTree)
		{
			return false;
		}

		WBP->Modify();
		WBP->WidgetTree->Modify();
		if (WBP->WidgetTree->RootWidget)
		{
			WBP->WidgetTree->RemoveWidget(WBP->WidgetTree->RootWidget);
		}

		OutRoot = BridgeUMGImpl::ConstructDesignerWidget<UCanvasPanel>(WBP, RootName, RequiredNames);
		if (!OutRoot)
		{
			return false;
		}
		OutRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WBP->WidgetTree->RootWidget = OutRoot;
		return true;
	}

	void SetVendingTextColor(UTextBlock* Text, const FLinearColor& Color)
	{
		if (Text)
		{
			Text->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	FSlateBrush MakeVendingRoundedBrush(
		const FLinearColor& Fill,
		float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent,
		float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	void ApplyVendingRoundedBrush(
		UBorder* Border,
		const FLinearColor& Fill,
		float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent,
		float OutlineWidth = 0.0f)
	{
		if (Border)
		{
			Border->SetBrush(MakeVendingRoundedBrush(Fill, Radius, Outline, OutlineWidth));
		}
	}

	FString NormalizeVendingObjectPath(const FString& AssetPath)
	{
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
		return AssetName.IsEmpty()
			? AssetPath
			: FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
	}

	UObject* LoadVendingResource(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UObject>(nullptr, *NormalizeVendingObjectPath(AssetPath));
	}

	FSlateBrush MakeVendingResourceBrush(
		const FString& AssetPath,
		const FVector2D& ImageSize,
		const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		if (UObject* Resource = LoadVendingResource(AssetPath))
		{
			Brush.SetResourceObject(Resource);
		}
		return Brush;
	}

	UImage* AddVendingCanvasImage(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& Name,
		const FString& AssetPath,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames,
		const FLinearColor& Tint = FLinearColor::White)
	{
		UImage* Image = BridgeUMGImpl::ConstructDesignerWidget<UImage>(WBP, Name, RequiredNames);
		if (Image)
		{
			Image->SetBrush(MakeVendingResourceBrush(AssetPath, Size, Tint));
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			BridgeUMGImpl::AddCanvasWidget(Root, Image, Position, Size, ZOrder);
		}
		return Image;
	}

	UImage* AddVendingHorizontalImage(
		UWidgetBlueprint* WBP,
		UHorizontalBox* Parent,
		const FName& Name,
		const FString& AssetPath,
		const FVector2D& Size,
		TArray<FString>& RequiredNames,
		const FLinearColor& Tint = FLinearColor::White,
		const FMargin& Padding = FMargin(0.0f, 0.0f, 12.0f, 0.0f))
	{
		UImage* Image = BridgeUMGImpl::ConstructDesignerWidget<UImage>(WBP, Name, RequiredNames);
		if (Image)
		{
			Image->SetBrush(MakeVendingResourceBrush(AssetPath, Size, Tint));
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (Parent)
			{
				if (UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Image))
				{
					Slot->SetPadding(Padding);
					Slot->SetHorizontalAlignment(HAlign_Center);
					Slot->SetVerticalAlignment(VAlign_Center);
				}
			}
		}
		return Image;
	}

	void StyleVendingButton(
		UButton* Button,
		const FLinearColor& Normal,
		const FLinearColor& Hovered,
		const FLinearColor& Pressed,
		float Radius = 20.0f,
		const FLinearColor& Outline = FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
		float OutlineWidth = 1.0f)
	{
		if (!Button)
		{
			return;
		}

		FButtonStyle Style = Button->GetStyle();
		Style.Normal = MakeVendingRoundedBrush(Normal, Radius, Outline, OutlineWidth);
		Style.Hovered = MakeVendingRoundedBrush(Hovered, Radius, FLinearColor(1.0f, 0.70f, 0.18f, 0.48f), FMath::Max(OutlineWidth, 1.5f));
		Style.Pressed = MakeVendingRoundedBrush(Pressed, Radius, FLinearColor(1.0f, 0.86f, 0.34f, 0.72f), FMath::Max(OutlineWidth, 2.0f));
		Style.Disabled = MakeVendingRoundedBrush(FLinearColor(0.10f, 0.11f, 0.13f, 0.55f), Radius, FLinearColor(1.0f, 1.0f, 1.0f, 0.06f), 1.0f);
		Style.SetNormalPadding(FMargin(8.0f, 7.0f, 8.0f, 9.0f));
		Style.SetPressedPadding(FMargin(8.0f, 9.0f, 8.0f, 7.0f));
		Button->SetStyle(Style);
	}

	UTextBlock* AddVendingCanvasText(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& Name,
		const FString& Label,
		int32 FontSize,
		const FLinearColor& Color,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames)
	{
		UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, Name, RequiredNames);
		BridgeUMGImpl::SetDesignerText(Text, Label, FontSize);
		SetVendingTextColor(Text, Color);
		if (Text)
		{
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			BridgeUMGImpl::AddCanvasWidget(Root, Text, Position, Size, ZOrder);
		}
		return Text;
	}

	UBorder* AddVendingPanel(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& Name,
		const FLinearColor& Color,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames,
		const FMargin& Padding = FMargin(18.0f, 16.0f, 18.0f, 16.0f))
	{
		UBorder* Panel = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(WBP, Name, RequiredNames);
		if (Panel)
		{
			ApplyVendingRoundedBrush(Panel, Color, 28.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), 1.5f);
			Panel->SetPadding(Padding);
			Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			BridgeUMGImpl::AddCanvasWidget(Root, Panel, Position, Size, ZOrder);
		}
		return Panel;
	}

	UVerticalBox* AddVendingColumnPanel(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& PanelName,
		const FName& ColumnName,
		const FLinearColor& Color,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames)
	{
		UBorder* Panel = AddVendingPanel(WBP, Root, PanelName, Color, Position, Size, ZOrder, RequiredNames);
		UVerticalBox* Column = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(WBP, ColumnName, RequiredNames);
		if (Panel && Column)
		{
			Panel->SetContent(Column);
		}
		return Column;
	}

	UScrollBox* AddVendingScrollColumnPanel(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& PanelName,
		const FName& ScrollName,
		const FName& ColumnName,
		const FLinearColor& Color,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames,
		UVerticalBox*& OutColumn)
	{
		OutColumn = nullptr;
		UBorder* Panel = AddVendingPanel(WBP, Root, PanelName, Color, Position, Size, ZOrder, RequiredNames);
		UScrollBox* Scroll = BridgeUMGImpl::ConstructDesignerWidget<UScrollBox>(WBP, ScrollName, RequiredNames);
		UVerticalBox* Column = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(WBP, ColumnName, RequiredNames);
		if (Panel && Scroll && Column)
		{
			Panel->SetContent(Scroll);
			Scroll->AddChild(Column);
			OutColumn = Column;
		}
		return Scroll;
	}

	UButton* AddVendingCanvasButton(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FName& ButtonName,
		const FName& TextName,
		const FString& Label,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder,
		TArray<FString>& RequiredNames,
		int32 FontSize = 18)
	{
		UButton* Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		BridgeUMGImpl::SetDesignerText(Text, Label, FontSize);
		SetVendingTextColor(Text, FLinearColor(1.0f, 0.94f, 0.80f, 1.0f));
		if (Button && Text)
		{
			StyleVendingButton(
				Button,
				FLinearColor(0.09f, 0.10f, 0.13f, 0.96f),
				FLinearColor(0.22f, 0.17f, 0.08f, 1.00f),
				FLinearColor(0.95f, 0.42f, 0.08f, 1.00f));
			Button->SetContent(Text);
			BridgeUMGImpl::AddCanvasWidget(Root, Button, Position, Size, ZOrder);
		}
		return Button;
	}

	int32 CountVendingWidgetsByPrefixAndClass(
		UWidgetBlueprint* WBP,
		const FString& NamePrefix,
		const UClass* WidgetClass)
	{
		if (!WBP || !WBP->WidgetTree || !WidgetClass)
		{
			return 0;
		}

		int32 Count = 0;
		WBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (Widget
				&& Widget->GetName().StartsWith(NamePrefix)
				&& Widget->IsA(WidgetClass))
			{
				++Count;
			}
		});
		return Count;
	}

	int32 CountVendingResourceImagesByPrefix(
		UWidgetBlueprint* WBP,
		const FString& NamePrefix)
	{
		if (!WBP || !WBP->WidgetTree)
		{
			return 0;
		}

		int32 Count = 0;
		WBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (UImage* Image = Cast<UImage>(Widget))
			{
				if (Image->GetName().StartsWith(NamePrefix)
					&& Image->GetBrush().GetResourceObject() != nullptr)
				{
					++Count;
				}
			}
		});
		return Count;
	}

	int32 CountVendingAnimationTracks(UWidgetBlueprint* WBP)
	{
		if (!WBP)
		{
			return 0;
		}

		int32 TrackCount = 0;
		for (UWidgetAnimation* Animation : WBP->Animations)
		{
			if (!Animation)
			{
				continue;
			}
			if (UMovieScene* Scene = Animation->GetMovieScene())
			{
				const TArray<FMovieSceneBinding>& Bindings = const_cast<const UMovieScene*>(Scene)->GetBindings();
				for (const FMovieSceneBinding& Binding : Bindings)
				{
					TrackCount += Binding.GetTracks().Num();
				}
				TrackCount += Scene->GetTracks().Num();
			}
		}
		return TrackCount;
	}

	void AddVendingFloatKeys(FMovieSceneFloatChannel& Channel, float StartValue, float EndValue, int32 EndFrame)
	{
		TArray<FFrameNumber> Times;
		Times.Add(FFrameNumber(0));
		Times.Add(FFrameNumber(EndFrame));

		TArray<FMovieSceneFloatValue> Values;
		Values.Emplace(StartValue);
		Values.Emplace(EndValue);
		Channel.UpdateOrAddKeys(Times, Values);
	}

	void AddVendingTransformTrack(
		UWidgetAnimation* Animation,
		UMovieScene* Scene,
		const FName& WidgetName,
		const FVector2D& StartTranslation,
		const FVector2D& StartScale,
		int32 EndFrame)
	{
		if (!Animation || !Scene)
		{
			return;
		}

		const FGuid Guid = Scene->AddPossessable(WidgetName.ToString(), UWidget::StaticClass());
		FWidgetAnimationBinding Binding;
		Binding.WidgetName = WidgetName;
		Binding.AnimationGuid = Guid;
		Binding.bIsRootWidget = false;
		Animation->AnimationBindings.Add(Binding);

		UMovieScene2DTransformTrack* Track = Scene->AddTrack<UMovieScene2DTransformTrack>(Guid);
		if (!Track)
		{
			return;
		}
		Track->SetPropertyNameAndPath(FName(TEXT("RenderTransform")), TEXT("RenderTransform"));

		UMovieSceneSection* Section = Track->CreateNewSection();
		UMovieScene2DTransformSection* TransformSection = Cast<UMovieScene2DTransformSection>(Section);
		if (!Section || !TransformSection)
		{
			return;
		}

		Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(EndFrame + 1)));
		TransformSection->SetMask(EMovieScene2DTransformChannel::Translation | EMovieScene2DTransformChannel::Scale);
		AddVendingFloatKeys(TransformSection->Translation[0], StartTranslation.X, 0.0f, EndFrame);
		AddVendingFloatKeys(TransformSection->Translation[1], StartTranslation.Y, 0.0f, EndFrame);
		AddVendingFloatKeys(TransformSection->Scale[0], StartScale.X, 1.0f, EndFrame);
		AddVendingFloatKeys(TransformSection->Scale[1], StartScale.Y, 1.0f, EndFrame);
		Track->AddSection(*Section);
		Track->SetSectionToKey(Section);
	}

	void RebuildVendingBallSelectAnimations(UWidgetBlueprint* WBP)
	{
		if (!WBP)
		{
			return;
		}

		for (UWidgetAnimation* Animation : WBP->Animations)
		{
			if (Animation)
			{
				Animation->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			}
		}
		WBP->Animations.Empty();

		const FName AnimationName = MakeUniqueObjectName(WBP, UWidgetAnimation::StaticClass(), FName(TEXT("VendingIntro")));
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(WBP, AnimationName, RF_Transactional);
		if (!Animation)
		{
			return;
		}
		Animation->SetDisplayLabel(TEXT("VendingIntro"));
		Animation->MovieScene = NewObject<UMovieScene>(Animation, FName(TEXT("VendingIntroScene")), RF_Transactional);
		if (!Animation->MovieScene)
		{
			return;
		}

		UMovieScene* Scene = Animation->MovieScene;
		Scene->SetDisplayRate(FFrameRate(60, 1));
		Scene->SetTickResolutionDirectly(FFrameRate(60, 1));
		Scene->SetPlaybackRange(FFrameNumber(0), 36);
		Scene->GetEditorData().WorkStart = 0.0;
		Scene->GetEditorData().WorkEnd = 0.6;

		AddVendingTransformTrack(Animation, Scene, FName(TEXT("BallSelectPanel")), FVector2D(-38.0f, 0.0f), FVector2D(0.96f, 1.0f), 18);
		AddVendingTransformTrack(Animation, Scene, FName(TEXT("VendingPreviewPanel")), FVector2D(0.0f, 28.0f), FVector2D(0.92f, 0.92f), 24);
		AddVendingTransformTrack(Animation, Scene, FName(TEXT("VendingLoadoutPanel")), FVector2D(42.0f, 0.0f), FVector2D(0.96f, 1.0f), 30);
		WBP->Animations.Add(Animation);
	}

	UButton* AddVendingReadableLoadoutSlot(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		int32 Index,
		const FString& Label,
		const FString& ThumbnailAssetPath,
		const FVector2D& Position,
		const FVector2D& Size,
		const FLinearColor& BallTint,
		bool bSelected,
		TArray<FString>& RequiredNames)
	{
		const FName ButtonName(*FString::Printf(TEXT("LoadoutSlot_%d"), Index));
		const FName TextName(*FString::Printf(TEXT("LoadoutSlot_%dText"), Index));
		UButton* Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		UOverlay* Stack = BridgeUMGImpl::ConstructDesignerWidget<UOverlay>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dStack"), Index)), RequiredNames);
		UHorizontalBox* Row = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dRow"), Index)), RequiredNames);
		if (!Button || !Stack || !Row)
		{
			return Button;
		}

		const FLinearColor Normal = bSelected
			? FLinearColor(0.19f, 0.13f, 0.055f, 0.96f)
			: FLinearColor(0.070f, 0.088f, 0.112f, 0.94f);
		StyleVendingButton(
			Button,
			Normal,
			FLinearColor(0.20f, 0.16f, 0.075f, 1.0f),
			FLinearColor(0.92f, 0.38f, 0.08f, 1.0f),
			22.0f,
			bSelected ? FLinearColor(1.0f, 0.70f, 0.12f, 0.90f) : FLinearColor(0.55f, 0.95f, 0.36f, 0.36f),
			bSelected ? 2.2f : 1.6f);

		USizeBox* IndexBox = BridgeUMGImpl::ConstructDesignerWidget<USizeBox>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dIndexBox"), Index)), RequiredNames);
		UBorder* IndexBadge = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dIndexBadge"), Index)), RequiredNames);
		UTextBlock* IndexText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dIndexText"), Index)), RequiredNames);
		if (IndexBox && IndexBadge && IndexText)
		{
			IndexBox->SetWidthOverride(34.0f);
			IndexBox->SetHeightOverride(34.0f);
			ApplyVendingRoundedBrush(
				IndexBadge,
				bSelected ? FLinearColor(1.0f, 0.70f, 0.12f, 1.0f) : FLinearColor(0.52f, 0.88f, 0.34f, 1.0f),
				18.0f,
				FLinearColor(0.0f, 0.0f, 0.0f, 0.42f),
				1.0f);
			BridgeUMGImpl::SetDesignerText(IndexText, FString::Printf(TEXT("%02d"), Index + 1), 13);
			SetVendingTextColor(IndexText, FLinearColor(0.04f, 0.04f, 0.035f, 1.0f));
			IndexText->SetJustification(ETextJustify::Center);
			IndexText->SetVisibility(ESlateVisibility::HitTestInvisible);
			IndexBadge->SetContent(IndexText);
			IndexBox->SetContent(IndexBadge);
			if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(IndexBox))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}

		USizeBox* MiniBox = BridgeUMGImpl::ConstructDesignerWidget<USizeBox>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dMiniBox"), Index)), RequiredNames);
		UOverlay* MiniStack = BridgeUMGImpl::ConstructDesignerWidget<UOverlay>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dMiniStack"), Index)), RequiredNames);
		UBorder* MiniBall = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dMiniBall"), Index)), RequiredNames);
		UImage* MiniImage = BridgeUMGImpl::ConstructDesignerWidget<UImage>(
			WBP, FName(*FString::Printf(TEXT("BallThumb_Loadout_%d"), Index)), RequiredNames);
		if (MiniBox && MiniStack && MiniBall && MiniImage)
		{
			MiniBox->SetWidthOverride(54.0f);
			MiniBox->SetHeightOverride(54.0f);
			ApplyVendingRoundedBrush(
				MiniBall,
				BallTint,
				32.0f,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.28f),
				1.0f);
			MiniBall->SetVisibility(ESlateVisibility::HitTestInvisible);
			MiniImage->SetBrush(MakeVendingResourceBrush(ThumbnailAssetPath, FVector2D(54.0f, 54.0f)));
			MiniImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* MiniBallSlot = MiniStack->AddChildToOverlay(MiniBall))
			{
				MiniBallSlot->SetHorizontalAlignment(HAlign_Fill);
				MiniBallSlot->SetVerticalAlignment(VAlign_Fill);
			}
			if (UOverlaySlot* MiniImageSlot = MiniStack->AddChildToOverlay(MiniImage))
			{
				MiniImageSlot->SetHorizontalAlignment(HAlign_Fill);
				MiniImageSlot->SetVerticalAlignment(VAlign_Fill);
			}
			MiniBox->SetContent(MiniStack);
			if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(MiniBox))
			{
				Slot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
		}

		UVerticalBox* Info = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dInfo"), Index)), RequiredNames);
		UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		UTextBlock* MetaText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dMetaText"), Index)), RequiredNames);
		if (Info && Text && MetaText)
		{
			BridgeUMGImpl::SetDesignerText(Text, Label, 17);
			SetVendingTextColor(Text, FLinearColor(0.98f, 0.96f, 0.90f, 1.0f));
			Text->SetJustification(ETextJustify::Left);
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			BridgeUMGImpl::SetDesignerText(MetaText, TEXT("A   Lv.0   锻造 0"), 12);
			SetVendingTextColor(MetaText, FLinearColor(1.0f, 0.88f, 0.35f, 1.0f));
			MetaText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UVerticalBoxSlot* TextSlot = Info->AddChildToVerticalBox(Text))
			{
				TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
				TextSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			if (UVerticalBoxSlot* MetaSlot = Info->AddChildToVerticalBox(MetaText))
			{
				MetaSlot->SetPadding(FMargin(0.0f));
				MetaSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			if (UHorizontalBoxSlot* InfoSlot = Row->AddChildToHorizontalBox(Info))
			{
				InfoSlot->SetPadding(FMargin(0.0f));
				InfoSlot->SetHorizontalAlignment(HAlign_Fill);
				InfoSlot->SetVerticalAlignment(VAlign_Center);
				InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		UTextBlock* PowerText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
			WBP, FName(*FString::Printf(TEXT("LoadoutSlot_%dPowerText"), Index)), RequiredNames);
		if (PowerText)
		{
			BridgeUMGImpl::SetDesignerText(PowerText, TEXT("碰撞 50"), 12);
			SetVendingTextColor(PowerText, FLinearColor(1.0f, 0.86f, 0.34f, 1.0f));
			PowerText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UHorizontalBoxSlot* PowerSlot = Row->AddChildToHorizontalBox(PowerText))
			{
				PowerSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
				PowerSlot->SetHorizontalAlignment(HAlign_Right);
				PowerSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		if (UOverlaySlot* RowSlot = Stack->AddChildToOverlay(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetVerticalAlignment(VAlign_Fill);
			RowSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 8.0f));
		}
		Button->SetContent(Stack);
		BridgeUMGImpl::AddCanvasWidget(Root, Button, Position, Size, 44 + Index);
		return Button;
	}

	UButton* AddVendingBackpackBallButton(
		UWidgetBlueprint* WBP,
		UVerticalBox* Parent,
		int32 Index,
		const FString& Label,
		const FString& ThumbnailAssetPath,
		float Padding,
		TArray<FString>& RequiredNames)
	{
		const FName ButtonName(*FString::Printf(TEXT("BallBackpack_%d"), Index));
		const FName TextName(*FString::Printf(TEXT("BallBackpack_%dText"), Index));
		UButton* Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		UHorizontalBox* Row = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
			WBP, FName(*FString::Printf(TEXT("BallBackpack_%dRow"), Index)), RequiredNames);
		UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		USizeBox* RankBox = BridgeUMGImpl::ConstructDesignerWidget<USizeBox>(
			WBP, FName(*FString::Printf(TEXT("BallBackpack_%dRankBox"), Index)), RequiredNames);
		UBorder* RankBadge = BridgeUMGImpl::ConstructDesignerWidget<UBorder>(
			WBP, FName(*FString::Printf(TEXT("BallBackpack_%dRankBadge"), Index)), RequiredNames);
		UTextBlock* RankText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
			WBP, FName(*FString::Printf(TEXT("BallBackpack_%dRankText"), Index)), RequiredNames);
		if (!Button || !Row || !Text)
		{
			return Button;
		}

		StyleVendingButton(
			Button,
			Index == 0 ? FLinearColor(0.17f, 0.10f, 0.035f, 0.96f) : FLinearColor(0.060f, 0.070f, 0.090f, 0.96f),
			FLinearColor(0.145f, 0.112f, 0.052f, 1.00f),
			FLinearColor(0.98f, 0.44f, 0.08f, 1.00f),
			18.0f,
			Index == 0 ? FLinearColor(1.0f, 0.66f, 0.10f, 0.90f) : FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
			Index == 0 ? 2.0f : 1.0f);

		AddVendingHorizontalImage(
			WBP,
			Row,
			FName(*FString::Printf(TEXT("BallThumb_List_%d"), Index)),
			ThumbnailAssetPath,
			FVector2D(42.0f, 42.0f),
			RequiredNames,
			FLinearColor::White,
			FMargin(0.0f, 0.0f, 10.0f, 0.0f));

		BridgeUMGImpl::SetDesignerText(Text, Label, 17);
		SetVendingTextColor(Text, FLinearColor(0.99f, 0.96f, 0.88f, 1.0f));
		Text->SetJustification(ETextJustify::Left);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text))
		{
			TextSlot->SetPadding(FMargin(0.0f));
			TextSlot->SetHorizontalAlignment(HAlign_Fill);
			TextSlot->SetVerticalAlignment(VAlign_Center);
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		if (RankBox && RankBadge && RankText)
		{
			RankBox->SetWidthOverride(34.0f);
			RankBox->SetHeightOverride(34.0f);
			ApplyVendingRoundedBrush(
				RankBadge,
				Index == 0 ? FLinearColor(1.0f, 0.68f, 0.12f, 1.0f) : FLinearColor(0.97f, 0.58f, 0.12f, 1.0f),
				18.0f,
				FLinearColor(0.0f, 0.0f, 0.0f, 0.42f),
				1.0f);
			BridgeUMGImpl::SetDesignerText(RankText, Index == 1 ? TEXT("B") : TEXT("A"), 18);
			SetVendingTextColor(RankText, FLinearColor(0.04f, 0.035f, 0.025f, 1.0f));
			RankText->SetJustification(ETextJustify::Center);
			RankText->SetVisibility(ESlateVisibility::HitTestInvisible);
			RankBadge->SetContent(RankText);
			RankBox->SetContent(RankBadge);
			if (UHorizontalBoxSlot* RankSlot = Row->AddChildToHorizontalBox(RankBox))
			{
				RankSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
				RankSlot->SetHorizontalAlignment(HAlign_Right);
				RankSlot->SetVerticalAlignment(VAlign_Center);
			}
		}

		Button->SetContent(Row);
		if (Parent)
		{
			if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
			{
				Slot->SetPadding(FMargin(Padding));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return Button;
	}

	UButton* AddVendingColumnButton(
		UWidgetBlueprint* WBP,
		UVerticalBox* Parent,
		const FName& ButtonName,
		const FName& TextName,
		const FString& Label,
		float Padding,
		TArray<FString>& RequiredNames,
		int32 FontSize = 18,
		const FLinearColor& TextColor = FLinearColor(0.98f, 0.95f, 0.84f, 1.0f))
	{
		UButton* Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, ButtonName, RequiredNames);
		UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, TextName, RequiredNames);
		BridgeUMGImpl::SetDesignerText(Text, Label, FontSize);
		SetVendingTextColor(Text, TextColor);
		if (Text)
		{
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			Text->SetJustification(ETextJustify::Center);
		}
		if (Button && Text)
		{
			StyleVendingButton(
				Button,
				FLinearColor(0.060f, 0.070f, 0.090f, 0.96f),
				FLinearColor(0.145f, 0.112f, 0.052f, 1.00f),
				FLinearColor(0.98f, 0.44f, 0.08f, 1.00f));
			Button->SetContent(Text);
		}
		if (Parent && Button)
		{
			if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
			{
				Slot->SetPadding(FMargin(Padding));
				Slot->SetHorizontalAlignment(HAlign_Fill);
			}
		}
		return Button;
	}

	UEditableTextBox* AddVendingSearchBox(
		UWidgetBlueprint* WBP,
		UVerticalBox* Parent,
		const FName& Name,
		const FString& HintText,
		TArray<FString>& RequiredNames)
	{
		UEditableTextBox* SearchBox = BridgeUMGImpl::ConstructDesignerWidget<UEditableTextBox>(WBP, Name, RequiredNames);
		if (SearchBox)
		{
			SearchBox->SetHintText(FText::FromString(HintText));
			if (Parent)
			{
				if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(SearchBox))
				{
					Slot->SetPadding(FMargin(8.0f, 7.0f, 8.0f, 7.0f));
					Slot->SetHorizontalAlignment(HAlign_Fill);
				}
			}
		}
		return SearchBox;
	}

	void AddVendingTopMetrics(
		UWidgetBlueprint* WBP,
		UCanvasPanel* Root,
		const FString& PhaseLabel,
		TArray<FString>& RequiredNames)
	{
		UBorder* Panel = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingTopHudPanel")),
			FLinearColor(0.015f, 0.018f, 0.024f, 0.88f),
			FVector2D(22.0f, 18.0f),
			FVector2D(1780.0f, 58.0f),
			80,
			RequiredNames,
			FMargin(10.0f, 8.0f, 10.0f, 8.0f));
		UHorizontalBox* Row = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
			WBP, FName(TEXT("TopRow")), RequiredNames);
		if (!Panel || !Row)
		{
			return;
		}
		Panel->SetContent(Row);

		const TArray<TPair<FName, FString>> Labels = {
			TPair<FName, FString>(FName(TEXT("PhaseText")), PhaseLabel),
			TPair<FName, FString>(FName(TEXT("ScoreText")), TEXT("SCORE 0000")),
			TPair<FName, FString>(FName(TEXT("WaveText")), TEXT("WAVE 01")),
			TPair<FName, FString>(FName(TEXT("BallsText")), TEXT("BALLS 04")),
			TPair<FName, FString>(FName(TEXT("MonsterHPText")), TEXT("1000 / 1000")),
		};
		for (const TPair<FName, FString>& Item : Labels)
		{
			UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, Item.Key, RequiredNames);
			BridgeUMGImpl::SetDesignerText(Text, Item.Value, 22);
			SetVendingTextColor(Text, FLinearColor(1.0f, 0.88f, 0.36f, 1.0f));
			if (Text)
			{
				Text->SetVisibility(ESlateVisibility::HitTestInvisible);
				if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Text))
				{
					Slot->SetPadding(FMargin(20.0f, 4.0f, 22.0f, 2.0f));
					Slot->SetVerticalAlignment(VAlign_Center);
				}
			}
		}
	}

	bool FinishVendingRebuild(UWidgetBlueprint* WBP, const FString& Kind, const TArray<FString>& RequiredNames)
	{
		int32 FoundCount = 0;
		for (const FString& Name : RequiredNames)
		{
			if (BridgeUMGImpl::FindWidgetByName(WBP, Name))
			{
				++FoundCount;
			}
		}

		const bool bSuccess = FoundCount == RequiredNames.Num();
		UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_%sSemanticControls: actual=%d expected=%d result=%s"),
			*Kind,
			FoundCount,
			RequiredNames.Num(),
			bSuccess ? TEXT("PASS") : TEXT("FAIL"));

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
		FKismetEditorUtilities::CompileBlueprint(WBP);
		WBP->MarkPackageDirty();
		return bSuccess;
	}

	bool RebuildVendingHUD(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("RuntimeHUDVendingRoot")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingTopMetrics(WBP, Root, TEXT("WavePrepare"), RequiredNames);
		UVerticalBox* LeftColumn = AddVendingColumnPanel(
			WBP,
			Root,
			FName(TEXT("LeftPanel")),
			FName(TEXT("LeftColumn")),
			FLinearColor(0.026f, 0.032f, 0.043f, 0.86f),
			FVector2D(24.0f, 104.0f),
			FVector2D(350.0f, 500.0f),
			30,
			RequiredNames);
		if (LeftColumn)
		{
			UTextBlock* Status = BridgeUMGImpl::AddDesignerText(
				WBP, LeftColumn, FName(TEXT("StatusText")), TEXT("Vending ready"), 22, 4.0f, RequiredNames);
			SetVendingTextColor(Status, FLinearColor(1.0f, 0.92f, 0.68f, 1.0f));
			UTextBlock* Skills = BridgeUMGImpl::AddDesignerText(
				WBP, LeftColumn, FName(TEXT("SkillsText")), TEXT("CD 1 0.0 | 2 0.0 | 3 0.0 | 4 0.0"), 16, 4.0f, RequiredNames);
			SetVendingTextColor(Skills, FLinearColor(0.74f, 0.86f, 0.96f, 1.0f));
			AddVendingColumnButton(WBP, LeftColumn, FName(TEXT("PrimaryActionButton")), FName(TEXT("PrimaryActionText")), TEXT("OK"), 7.0f, RequiredNames);
			AddVendingColumnButton(WBP, LeftColumn, FName(TEXT("PauseButton")), FName(TEXT("PauseText")), TEXT("Pause"), 7.0f, RequiredNames);
			AddVendingColumnButton(WBP, LeftColumn, FName(TEXT("RuntimeRetryButton")), FName(TEXT("RuntimeRetryText")), TEXT("Retry"), 7.0f, RequiredNames);
			AddVendingColumnButton(
				WBP,
				LeftColumn,
				FName(TEXT("RuntimeQuitButton")),
				FName(TEXT("RuntimeQuitText")),
				TEXT("Quit"),
				7.0f,
				RequiredNames,
				18,
				FLinearColor(1.0f, 0.74f, 0.70f, 1.0f));
		}

		UBorder* Bottom = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("BottomPanel")),
			FLinearColor(0.018f, 0.020f, 0.026f, 0.78f),
			FVector2D(24.0f, 982.0f),
			FVector2D(900.0f, 56.0f),
			50,
			RequiredNames,
			FMargin(12.0f, 10.0f, 12.0f, 10.0f));
		if (Bottom)
		{
			UTextBlock* ControlsText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(
				WBP, FName(TEXT("ControlsText")), RequiredNames);
			BridgeUMGImpl::SetDesignerText(ControlsText, TEXT("BallGameChaos Roguelike"), 20);
			SetVendingTextColor(ControlsText, FLinearColor(0.95f, 0.95f, 0.90f, 1.0f));
			Bottom->SetContent(ControlsText);
		}
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingBallSelect(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("BallSelectDesignerRoot")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingBackdrop")),
			FLinearColor(0.010f, 0.014f, 0.021f, 0.94f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1920.0f, 1080.0f),
			0,
			RequiredNames);
		AddVendingCanvasImage(
			WBP,
			Root,
			FName(TEXT("VendingSlice_0")),
			TEXT("/Game/UI/Generated/sub_Tex/main_settings_panel_4"),
			FVector2D(0.0f, 88.0f),
			FVector2D(540.0f, 910.0f),
			1,
			RequiredNames,
			FLinearColor(1.0f, 0.58f, 0.20f, 0.30f));
		AddVendingCanvasImage(
			WBP,
			Root,
			FName(TEXT("VendingSlice_1")),
			TEXT("/Game/UI/Generated/sub_Tex/main_settings_panel_18"),
			FVector2D(500.0f, 72.0f),
			FVector2D(1040.0f, 940.0f),
			1,
			RequiredNames,
			FLinearColor(0.48f, 0.76f, 1.0f, 0.16f));
		AddVendingCanvasImage(
			WBP,
			Root,
			FName(TEXT("VendingSlice_2")),
			TEXT("/Game/UI/Generated/sub_Tex/sidebar_panel_1"),
			FVector2D(1300.0f, 82.0f),
			FVector2D(560.0f, 906.0f),
			1,
			RequiredNames,
			FLinearColor(1.0f, 0.72f, 0.24f, 0.20f));
		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingStagePanel")),
			FLinearColor(0.018f, 0.022f, 0.030f, 0.72f),
			FVector2D(20.0f, 94.0f),
			FVector2D(1880.0f, 890.0f),
			2,
			RequiredNames,
			FMargin(0.0f));
		if (UBorder* OrangeSlab = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingOrangeSlab")),
			FLinearColor(1.0f, 0.34f, 0.08f, 0.14f),
			FVector2D(430.0f, 92.0f),
			FVector2D(42.0f, 890.0f),
			3,
			RequiredNames,
			FMargin(0.0f)))
		{
			OrangeSlab->SetRenderTransformAngle(-13.0f);
		}
		AddVendingTopMetrics(WBP, Root, TEXT("WavePrepare"), RequiredNames);

		UVerticalBox* LeftColumn = nullptr;
		AddVendingScrollColumnPanel(
			WBP,
			Root,
			FName(TEXT("BallSelectPanel")),
			FName(TEXT("BallSelectScroll")),
			FName(TEXT("BallSelectColumn")),
			FLinearColor(0.035f, 0.042f, 0.055f, 0.96f),
			FVector2D(42.0f, 100.0f),
			FVector2D(360.0f, 860.0f),
			20,
			RequiredNames,
			LeftColumn);
		if (LeftColumn)
		{
			UTextBlock* Title = BridgeUMGImpl::AddDesignerText(
				WBP, LeftColumn, FName(TEXT("TitleText")), TEXT("Ball Pack"), 28, 2.0f, RequiredNames);
			SetVendingTextColor(Title, FLinearColor(1.0f, 0.94f, 0.78f, 1.0f));
			UTextBlock* Selection = BridgeUMGImpl::AddDesignerText(
				WBP, LeftColumn, FName(TEXT("SelectionText")), TEXT("Loadout: 4 slots"), 16, 2.0f, RequiredNames);
			SetVendingTextColor(Selection, FLinearColor(0.86f, 0.90f, 0.96f, 1.0f));
			if (UButton* Primary = AddVendingColumnButton(
				WBP, LeftColumn, FName(TEXT("PrimaryActionButton")), FName(TEXT("PrimaryActionButtonText")), TEXT("OK"), 1.0f, RequiredNames))
			{
				Primary->SetVisibility(ESlateVisibility::Collapsed);
			}
			AddVendingSearchBox(WBP, LeftColumn, FName(TEXT("BallPackSearchBox")), TEXT("Search Ball Pack"), RequiredNames);
			AddVendingColumnButton(
				WBP, LeftColumn, FName(TEXT("BallPackToggleButton")), FName(TEXT("BallPackToggleText")), TEXT("Ball Pack (38/38) Collapse"), 4.0f, RequiredNames);

			const TArray<FString> BallLabels = {
				TEXT("VegetableBall"), TEXT("ShieldDogBall"), TEXT("HasakiBall"), TEXT("LoveRobotBall"),
				TEXT("DeathRobotBall"), TEXT("ProtonBall"), TEXT("FurnaceBall"), TEXT("InterestBall"),
				TEXT("MagnetBall"), TEXT("GoldBall"), TEXT("PoisonPotionBall"), TEXT("BurnPotionBall"),
				TEXT("BurstPotionBall"), TEXT("DiamondBall"), TEXT("AgateBall"), TEXT("EmeraldBall"),
				TEXT("LapisBall"), TEXT("ApatiteBall"), TEXT("VineBall"), TEXT("PaladinBall"),
				TEXT("NunBall"), TEXT("AngelBall"), TEXT("CrossPegBall"), TEXT("SunBall"),
				TEXT("MoonBall"), TEXT("MercuryBall"), TEXT("VenusBall"), TEXT("MarsBall"),
				TEXT("JupiterBall"), TEXT("SaturnBall"), TEXT("UranusBall"), TEXT("NeptuneBall"),
				TEXT("CometBall"), TEXT("MeteorBall"), TEXT("OrbitBall"), TEXT("ChickenHeadBall"),
				TEXT("OverallsBall"), TEXT("Basketball"),
			};
			const TArray<FString> BackpackThumbnailAssets = {
				TEXT("/Game/Mesh/BallVegetable/Ball_Vagetable_Base"),
				TEXT("/Game/Mesh/ML_Ball"),
				TEXT("/Game/Mesh/BallHasaki/Ball_Hasaki_Base"),
				TEXT("/Game/Mesh/BallLove/Ball_Love_Base"),
			};
			for (int32 Index = 0; Index < BallLabels.Num(); ++Index)
			{
				if (BackpackThumbnailAssets.IsValidIndex(Index))
				{
					AddVendingBackpackBallButton(
						WBP,
						LeftColumn,
						Index,
						BallLabels[Index],
						BackpackThumbnailAssets[Index],
						2.0f,
						RequiredNames);
				}
				else
				{
					AddVendingColumnButton(
						WBP,
						LeftColumn,
						FName(*FString::Printf(TEXT("BallBackpack_%d"), Index)),
						FName(*FString::Printf(TEXT("BallBackpack_%dText"), Index)),
						BallLabels[Index],
						2.0f,
						RequiredNames,
						18);
				}
			}
		}

		UVerticalBox* CenterColumn = AddVendingColumnPanel(
			WBP,
			Root,
			FName(TEXT("VendingPreviewPanel")),
			FName(TEXT("VendingPreviewColumn")),
			FLinearColor(0.018f, 0.024f, 0.034f, 0.24f),
			FVector2D(440.0f, 112.0f),
			FVector2D(790.0f, 760.0f),
			15,
			RequiredNames);
		if (UBorder* ArenaDisc = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingArenaDisc")),
			FLinearColor(0.04f, 0.18f, 0.32f, 0.30f),
			FVector2D(640.0f, 252.0f),
			FVector2D(560.0f, 560.0f),
			16,
			RequiredNames,
			FMargin(0.0f)))
		{
			ApplyVendingRoundedBrush(
				ArenaDisc,
				FLinearColor(0.04f, 0.18f, 0.32f, 0.30f),
				300.0f,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.10f),
				1.0f);
		}
		if (CenterColumn)
		{
			UTextBlock* Subtitle = BridgeUMGImpl::AddDesignerText(
				WBP, CenterColumn, FName(TEXT("VendingSubtitleText")), TEXT("MARBLE VEND READY"), 20, 0.0f, RequiredNames);
			SetVendingTextColor(Subtitle, FLinearColor(1.0f, 0.64f, 0.12f, 1.0f));
			UTextBlock* Name = BridgeUMGImpl::AddDesignerText(
				WBP, CenterColumn, FName(TEXT("VendingHeroNameText")), TEXT("VegetableBall"), 62, 0.0f, RequiredNames);
			SetVendingTextColor(Name, FLinearColor(1.0f, 0.97f, 0.88f, 1.0f));
			UTextBlock* Preview = BridgeUMGImpl::AddDesignerText(
				WBP, CenterColumn, FName(TEXT("PreviewText")), TEXT("Preview: VegetableBall | Bead: None | Yaw: 0.0"), 18, 2.0f, RequiredNames);
			SetVendingTextColor(Preview, FLinearColor(0.86f, 0.90f, 0.96f, 1.0f));
			UTextBlock* Config = BridgeUMGImpl::AddDesignerText(
				WBP, CenterColumn, FName(TEXT("ConfigText")), TEXT("Config: Folded"), 15, 2.0f, RequiredNames);
			SetVendingTextColor(Config, FLinearColor(0.72f, 0.78f, 0.86f, 1.0f));
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("VendingLoadoutPanel")),
			FLinearColor(0.060f, 0.070f, 0.086f, 0.96f),
			FVector2D(1320.0f, 100.0f),
			FVector2D(520.0f, 860.0f),
			20,
			RequiredNames,
			FMargin(24.0f, 20.0f, 24.0f, 20.0f));
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("LoadoutSlotsTitleText")),
			TEXT("Loadout Slots"),
			28,
			FLinearColor(1.0f, 0.94f, 0.78f, 1.0f),
			FVector2D(1356.0f, 124.0f),
			FVector2D(420.0f, 40.0f),
			30,
			RequiredNames);
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("NextMonsterPreviewText")),
			TEXT("Next Monster: Small"),
			17,
			FLinearColor(1.0f, 0.82f, 0.36f, 1.0f),
			FVector2D(1358.0f, 166.0f),
			FVector2D(430.0f, 34.0f),
			30,
			RequiredNames);
		if (UBorder* LoadoutStateCard = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("LoadoutStateCard")),
			FLinearColor(0.026f, 0.030f, 0.040f, 0.72f),
			FVector2D(1354.0f, 210.0f),
			FVector2D(430.0f, 82.0f),
			30,
			RequiredNames,
			FMargin(14.0f, 10.0f, 14.0f, 10.0f)))
		{
			UVerticalBox* StateColumn = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(
				WBP, FName(TEXT("LoadoutStateColumn")), RequiredNames);
			if (StateColumn)
			{
				LoadoutStateCard->SetContent(StateColumn);
				UTextBlock* SeedTitle = BridgeUMGImpl::AddDesignerText(
					WBP, StateColumn, FName(TEXT("LoadoutStateTitleText")), TEXT("Initial Seed"), 16, 0.0f, RequiredNames);
				SetVendingTextColor(SeedTitle, FLinearColor(1.0f, 0.94f, 0.78f, 1.0f));
				UTextBlock* SeedMeta = BridgeUMGImpl::AddDesignerText(
					WBP, StateColumn, FName(TEXT("LoadoutStateMetaText")), TEXT("Loadout Index 4 / 4"), 20, 3.0f, RequiredNames);
				SetVendingTextColor(SeedMeta, FLinearColor(1.0f, 0.84f, 0.25f, 1.0f));
			}
		}
		if (UBorder* RailPanel = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("LoadoutRailPanel")),
			FLinearColor(0.025f, 0.030f, 0.040f, 0.62f),
			FVector2D(1346.0f, 314.0f),
			FVector2D(454.0f, 396.0f),
			31,
			RequiredNames,
			FMargin(0.0f)))
		{
			ApplyVendingRoundedBrush(
				RailPanel,
				FLinearColor(0.025f, 0.030f, 0.040f, 0.62f),
				26.0f,
				FLinearColor(1.0f, 1.0f, 1.0f, 0.12f),
				1.0f);
		}
		if (UBorder* RailStripe = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("LoadoutRailStripe")),
			FLinearColor(0.55f, 0.92f, 0.33f, 0.22f),
			FVector2D(1540.0f, 304.0f),
			FVector2D(8.0f, 430.0f),
			32,
			RequiredNames,
			FMargin(0.0f)))
		{
			ApplyVendingRoundedBrush(RailStripe, FLinearColor(0.55f, 0.92f, 0.33f, 0.22f), 6.0f);
			RailStripe->SetRenderTransformAngle(17.0f);
		}

		const TArray<FString> LoadoutLabels = {
			TEXT("PoisonPotionBall"),
			TEXT("BurnPotionBall"),
			TEXT("BurstPotionBall"),
			TEXT("VineBall"),
		};
		const TArray<FLinearColor> LoadoutBallTints = {
			FLinearColor(0.78f, 0.20f, 0.78f, 1.0f),
			FLinearColor(1.0f, 0.48f, 0.14f, 1.0f),
			FLinearColor(0.90f, 0.22f, 0.30f, 1.0f),
			FLinearColor(0.54f, 0.94f, 0.34f, 1.0f),
		};
		const TArray<FString> LoadoutThumbnailAssets = {
			TEXT("/Game/Mesh/ML_PoisonPotion"),
			TEXT("/Game/Mesh/ML_BurnPotion"),
			TEXT("/Game/Mesh/ML_BurstPotion"),
			TEXT("/Game/Mesh/ML_Vine"),
		};
		int32 PerspectiveSlotCount = 0;
		int32 ReadableSlotCardCount = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector2D SlotPos(1368.0f - Index * 18.0f, 324.0f + Index * 100.0f);
			const FVector2D SlotSize(402.0f, 82.0f);
			AddVendingReadableLoadoutSlot(
				WBP,
				Root,
				Index,
				LoadoutLabels[Index],
				LoadoutThumbnailAssets.IsValidIndex(Index) ? LoadoutThumbnailAssets[Index] : FString(),
				SlotPos,
				SlotSize,
				LoadoutBallTints.IsValidIndex(Index) ? LoadoutBallTints[Index] : FLinearColor(0.65f, 0.68f, 0.70f, 1.0f),
				Index == 0,
				RequiredNames);
			AddVendingCanvasButton(
				WBP,
				Root,
				FName(*FString::Printf(TEXT("BeadSlot_%d"), Index)),
				FName(*FString::Printf(TEXT("BeadSlot_%dText"), Index)),
				FString::Printf(TEXT("Bead %d: Empty"), Index + 1),
				FVector2D(SlotPos.X + 274.0f, SlotPos.Y + 54.0f),
				FVector2D(124.0f, 24.0f),
				52 + Index,
				RequiredNames,
				10);
			++PerspectiveSlotCount;
			++ReadableSlotCardCount;
		}
		RebuildVendingBallSelectAnimations(WBP);
		UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_ball_selectPerspectiveSlots: actual=%d expected=4 result=%s"),
			PerspectiveSlotCount,
			PerspectiveSlotCount == 4 ? TEXT("PASS") : TEXT("FAIL"));
		UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_ball_selectReadableSlotCards: actual=%d expected=4 result=%s"),
			ReadableSlotCardCount,
			ReadableSlotCardCount == 4 ? TEXT("PASS") : TEXT("FAIL"));
		{
			const int32 ThumbnailImageCount = CountVendingResourceImagesByPrefix(WBP, TEXT("BallThumb_"));
			UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_ball_selectRealThumbnailImages: actual=%d expected=8 result=%s"),
				ThumbnailImageCount,
				ThumbnailImageCount == 8 ? TEXT("PASS") : TEXT("FAIL"));
		}
		{
			const int32 BackgroundSliceCount = CountVendingWidgetsByPrefixAndClass(WBP, TEXT("VendingSlice_"), UImage::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_ball_selectBackgroundSlices: actual=%d expected=3 result=%s"),
				BackgroundSliceCount,
				BackgroundSliceCount == 3 ? TEXT("PASS") : TEXT("FAIL"));
		}
		{
			const int32 AnimationTrackCount = CountVendingAnimationTracks(WBP);
			UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_ball_selectAnimationTracks: actual=%d expected=3 result=%s"),
				AnimationTrackCount,
				AnimationTrackCount == 3 ? TEXT("PASS") : TEXT("FAIL"));
		}

		AddVendingCanvasButton(
			WBP,
			Root,
			FName(TEXT("BeadBackpack_0")),
			FName(TEXT("BeadBackpack_0Text")),
			TEXT("Centroid Bead"),
			FVector2D(1358.0f, 742.0f),
			FVector2D(190.0f, 48.0f),
			42,
			RequiredNames,
			14);
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("BackpackStatusText")),
			TEXT("Backpack: select a ball"),
			15,
			FLinearColor(0.76f, 0.86f, 0.95f, 1.0f),
			FVector2D(1562.0f, 746.0f),
			FVector2D(252.0f, 44.0f),
			42,
			RequiredNames);
		AddVendingCanvasButton(
			WBP,
			Root,
			FName(TEXT("BackpackCarryActionButton")),
			FName(TEXT("BackpackCarryActionButtonText")),
			TEXT("Carry"),
			FVector2D(1358.0f, 804.0f),
			FVector2D(190.0f, 46.0f),
			42,
			RequiredNames,
			15);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const int32 Col = Index % 6;
			const int32 Row = Index / 6;
			AddVendingCanvasButton(
				WBP,
				Root,
				FName(*FString::Printf(TEXT("CarrySlot_%d"), Index)),
				FName(*FString::Printf(TEXT("CarrySlot_%dText"), Index)),
				FString::Printf(TEXT("%02d"), Index + 1),
				FVector2D(1570.0f + Col * 42.0f, 806.0f + Row * 36.0f),
				FVector2D(36.0f, 30.0f),
				42,
				RequiredNames,
				11);
		}

		AddVendingCanvasButton(
			WBP,
			Root,
			FName(TEXT("RightConfirmButton")),
			FName(TEXT("RightConfirmButtonText")),
			TEXT("OK"),
			FVector2D(1676.0f, 970.0f),
			FVector2D(142.0f, 52.0f),
			40,
			RequiredNames,
			18);
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingPegInventory(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("PegInventoryDesignerRoot")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("PegBackdrop")),
			FLinearColor(0.010f, 0.014f, 0.020f, 0.25f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1920.0f, 1080.0f),
			0,
			RequiredNames);
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("HeaderText")),
			TEXT("Peg Backpack"),
			26,
			FLinearColor(1.0f, 0.92f, 0.68f, 1.0f),
			FVector2D(720.0f, 250.0f),
			FVector2D(300.0f, 40.0f),
			10,
			RequiredNames);

		UBorder* DraftPanel = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("DraftPanel")),
			FLinearColor(0.035f, 0.040f, 0.052f, 0.86f),
			FVector2D(600.0f, 320.0f),
			FVector2D(720.0f, 170.0f),
			20,
			RequiredNames,
			FMargin(12.0f));
		UHorizontalBox* DraftRow = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
			WBP, FName(TEXT("DraftIconRow")), RequiredNames);
		if (DraftPanel && DraftRow)
		{
			DraftPanel->SetContent(DraftRow);
			BridgeUMGImpl::AddDesignerIconButton(WBP, DraftRow, FName(TEXT("DraftChoice0Button")), FName(TEXT("DraftChoice0Icon")), FName(TEXT("DraftChoice0Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);
			BridgeUMGImpl::AddDesignerIconButton(WBP, DraftRow, FName(TEXT("DraftChoice1Button")), FName(TEXT("DraftChoice1Icon")), FName(TEXT("DraftChoice1Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);
			BridgeUMGImpl::AddDesignerIconButton(WBP, DraftRow, FName(TEXT("DraftChoice2Button")), FName(TEXT("DraftChoice2Icon")), FName(TEXT("DraftChoice2Text")), TEXT("+"), FVector2D(116.0f, 116.0f), RequiredNames);
		}

		UBorder* BackpackPanel = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("BackpackPanel")),
			FLinearColor(0.025f, 0.030f, 0.040f, 0.84f),
			FVector2D(620.0f, 778.0f),
			FVector2D(680.0f, 142.0f),
			20,
			RequiredNames,
			FMargin(12.0f));
		UHorizontalBox* BackpackRow = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(
			WBP, FName(TEXT("BackpackIconRow")), RequiredNames);
		if (BackpackPanel && BackpackRow)
		{
			BackpackPanel->SetContent(BackpackRow);
			BridgeUMGImpl::AddDesignerIconButton(WBP, BackpackRow, FName(TEXT("Backpack0Button")), FName(TEXT("Backpack0Icon")), FName(TEXT("Backpack0Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
			BridgeUMGImpl::AddDesignerIconButton(WBP, BackpackRow, FName(TEXT("Backpack1Button")), FName(TEXT("Backpack1Icon")), FName(TEXT("Backpack1Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
			BridgeUMGImpl::AddDesignerIconButton(WBP, BackpackRow, FName(TEXT("Backpack2Button")), FName(TEXT("Backpack2Icon")), FName(TEXT("Backpack2Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
			BridgeUMGImpl::AddDesignerIconButton(WBP, BackpackRow, FName(TEXT("Backpack3Button")), FName(TEXT("Backpack3Icon")), FName(TEXT("Backpack3Text")), TEXT("+"), FVector2D(86.0f, 86.0f), RequiredNames);
		}

		AddVendingCanvasButton(WBP, Root, FName(TEXT("EditBallSlotsModeButton")), FName(TEXT("EditBallSlotsModeText")), TEXT("Edit Ball"), FVector2D(42.0f, 920.0f), FVector2D(120.0f, 46.0f), 40, RequiredNames);
		AddVendingCanvasButton(WBP, Root, FName(TEXT("EditPegsModeButton")), FName(TEXT("EditPegsModeText")), TEXT("Edit Peg"), FVector2D(184.0f, 920.0f), FVector2D(120.0f, 46.0f), 40, RequiredNames);
		AddVendingCanvasButton(WBP, Root, FName(TEXT("ConfirmButton")), FName(TEXT("ConfirmText")), TEXT("OK"), FVector2D(1670.0f, 920.0f), FVector2D(120.0f, 46.0f), 40, RequiredNames);
		AddVendingCanvasButton(WBP, Root, FName(TEXT("CancelButton")), FName(TEXT("CancelText")), TEXT("X"), FVector2D(1800.0f, 920.0f), FVector2D(80.0f, 46.0f), 40, RequiredNames);
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingUpgradePick(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("UpgradePickRoot")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("UpgradeBackdrop")),
			FLinearColor(0.012f, 0.016f, 0.024f, 0.92f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1920.0f, 1080.0f),
			0,
			RequiredNames);
		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("UpgradePickPanel")),
			FLinearColor(0.034f, 0.038f, 0.048f, 0.92f),
			FVector2D(320.0f, 170.0f),
			FVector2D(1280.0f, 680.0f),
			20,
			RequiredNames);
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("UpgradeTitleText")),
			TEXT("Reward Cards"),
			48,
			FLinearColor(1.0f, 0.92f, 0.68f, 1.0f),
			FVector2D(380.0f, 214.0f),
			FVector2D(560.0f, 60.0f),
			30,
			RequiredNames);
		UVerticalBox* Column = AddVendingColumnPanel(
			WBP,
			Root,
			FName(TEXT("UpgradeListPanel")),
			FName(TEXT("UpgradePickColumn")),
			FLinearColor(0.018f, 0.022f, 0.030f, 0.72f),
			FVector2D(1128.0f, 260.0f),
			FVector2D(360.0f, 430.0f),
			30,
			RequiredNames);
		if (Column)
		{
			AddVendingSearchBox(WBP, Column, FName(TEXT("UpgradeBallSearchBox")), TEXT("Search Ball Pack"), RequiredNames);
			AddVendingColumnButton(WBP, Column, FName(TEXT("UpgradeBallListToggleButton")), FName(TEXT("UpgradeBallListToggleText")), TEXT("Ball List"), 8.0f, RequiredNames);
			AddVendingColumnButton(WBP, Column, FName(TEXT("UpgradeNoBallButton")), FName(TEXT("UpgradeNoBallText")), TEXT("No Ball"), 8.0f, RequiredNames);
		}
		const TArray<FString> RewardLabels = {
			TEXT("A  LoveRobotBall\n强化  解锁  碰撞 50"),
			TEXT("B  DeathRobotBall\n冲锋  爆发  碰撞 50"),
			TEXT("A  ProtonBall\n返利  经济  碰撞 50"),
		};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			AddVendingCanvasButton(
				WBP,
				Root,
				FName(*FString::Printf(TEXT("UpgradeChoice%dButton"), Index + 1)),
				FName(*FString::Printf(TEXT("UpgradeChoice%dText"), Index + 1)),
				RewardLabels[Index],
				FVector2D(390.0f + Index * 238.0f, 336.0f + Index * 30.0f),
				FVector2D(220.0f, 290.0f),
				40 + Index,
				RequiredNames,
				20);
		}
		UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_upgrade_pickRewardCards: actual=3 expected=3 result=PASS"));
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingShop(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("ShopRootCanvas")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("ShopBackdrop")),
			FLinearColor(0.012f, 0.016f, 0.024f, 0.92f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1920.0f, 1080.0f),
			0,
			RequiredNames);
		AddVendingTopMetrics(WBP, Root, TEXT("Shop"), RequiredNames);

		UVerticalBox* RunBallColumn = nullptr;
		AddVendingScrollColumnPanel(
			WBP,
			Root,
			FName(TEXT("ShopRunBallPanel")),
			FName(TEXT("ShopRunBallScroll")),
			FName(TEXT("ShopRunBallColumn")),
			FLinearColor(0.032f, 0.038f, 0.050f, 0.94f),
			FVector2D(42.0f, 110.0f),
			FVector2D(500.0f, 870.0f),
			20,
			RequiredNames,
			RunBallColumn);
		if (RunBallColumn)
		{
			const TArray<TPair<FName, FString>> Labels = {
				TPair<FName, FString>(FName(TEXT("ShopGoldText")), TEXT("SHOP 0")),
				TPair<FName, FString>(FName(TEXT("ShopScoreText")), TEXT("SCORE 0")),
				TPair<FName, FString>(FName(TEXT("ShopWaveKindText")), TEXT("Next Wave: Small")),
				TPair<FName, FString>(FName(TEXT("ShopStatusText")), TEXT("Choose a ball")),
			};
			for (const TPair<FName, FString>& Item : Labels)
			{
				UTextBlock* Text = BridgeUMGImpl::AddDesignerText(WBP, RunBallColumn, Item.Key, Item.Value, 22, 4.0f, RequiredNames);
				SetVendingTextColor(Text, FLinearColor(1.0f, 0.86f, 0.34f, 1.0f));
			}
			AddVendingSearchBox(WBP, RunBallColumn, FName(TEXT("ShopSearchBox")), TEXT("Search Backpack"), RequiredNames);
			for (int32 Index = 0; Index < 24; ++Index)
			{
				BridgeUMGImpl::AddDesignerButton(
					WBP,
					RunBallColumn,
					FName(*FString::Printf(TEXT("RunBall_%dButton"), Index)),
					FName(*FString::Printf(TEXT("RunBall_%dText"), Index)),
					FString::Printf(TEXT("Run Ball %02d"), Index + 1),
					2.0f,
					RequiredNames);
			}
		}

		UBorder* ActionPanel = AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("ShopActionPanel")),
			FLinearColor(0.035f, 0.040f, 0.052f, 0.90f),
			FVector2D(580.0f, 740.0f),
			FVector2D(760.0f, 180.0f),
			25,
			RequiredNames);
		UHorizontalBox* ActionRow = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(WBP, FName(TEXT("ShopActionRow")), RequiredNames);
		if (ActionPanel && ActionRow)
		{
			ActionPanel->SetContent(ActionRow);
			const TArray<TPair<TPair<FName, FName>, FString>> Actions = {
				TPair<TPair<FName, FName>, FString>(TPair<FName, FName>(FName(TEXT("DeleteBallButton")), FName(TEXT("DeleteBallText"))), TEXT("Delete")),
				TPair<TPair<FName, FName>, FString>(TPair<FName, FName>(FName(TEXT("ForgeBallButton")), FName(TEXT("ForgeBallText"))), TEXT("Forge")),
				TPair<TPair<FName, FName>, FString>(TPair<FName, FName>(FName(TEXT("RefreshShopButton")), FName(TEXT("RefreshShopText"))), TEXT("Refresh")),
				TPair<TPair<FName, FName>, FString>(TPair<FName, FName>(FName(TEXT("BuyCarrySlotUpgradeButton")), FName(TEXT("BuyCarrySlotUpgradeText"))), TEXT("Carry Slot")),
			};
			for (const TPair<TPair<FName, FName>, FString>& Action : Actions)
			{
				UButton* Button = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, Action.Key.Key, RequiredNames);
				UTextBlock* Text = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, Action.Key.Value, RequiredNames);
				BridgeUMGImpl::SetDesignerText(Text, Action.Value, 15);
				if (Button && Text)
				{
					StyleVendingButton(
						Button,
						Action.Value == TEXT("Delete") ? FLinearColor(0.20f, 0.045f, 0.040f, 0.96f) : FLinearColor(0.06f, 0.07f, 0.09f, 0.96f),
						Action.Value == TEXT("Delete") ? FLinearColor(0.36f, 0.070f, 0.060f, 1.00f) : FLinearColor(0.18f, 0.13f, 0.06f, 1.00f),
						Action.Value == TEXT("Delete") ? FLinearColor(0.62f, 0.10f, 0.08f, 1.00f) : FLinearColor(0.95f, 0.42f, 0.08f, 1.00f));
					SetVendingTextColor(Text, FLinearColor(1.0f, 0.92f, 0.78f, 1.0f));
					Text->SetJustification(ETextJustify::Center);
					Text->SetVisibility(ESlateVisibility::HitTestInvisible);
					Button->SetContent(Text);
					if (UHorizontalBoxSlot* Slot = ActionRow->AddChildToHorizontalBox(Button))
					{
						Slot->SetPadding(FMargin(8.0f));
						Slot->SetHorizontalAlignment(HAlign_Fill);
						Slot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("ShopOfferPanel")),
			FLinearColor(0.040f, 0.045f, 0.058f, 0.94f),
			FVector2D(620.0f, 114.0f),
			FVector2D(1180.0f, 570.0f),
			20,
			RequiredNames,
			FMargin(24.0f, 20.0f, 24.0f, 20.0f));
		AddVendingCanvasText(
			WBP,
			Root,
			FName(TEXT("ShopOfferTitleText")),
			TEXT("Vending Offers"),
			34,
			FLinearColor(1.0f, 0.94f, 0.78f, 1.0f),
			FVector2D(656.0f, 150.0f),
			FVector2D(480.0f, 48.0f),
			30,
			RequiredNames);
		int32 OfferGridCount = 0;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const int32 Col = Index % 4;
			const int32 Row = Index / 4;
			AddVendingCanvasButton(
				WBP,
				Root,
				FName(*FString::Printf(TEXT("ShopOffer_%dButton"), Index)),
				FName(*FString::Printf(TEXT("ShopOffer_%dText"), Index)),
				FString::Printf(TEXT("Offer %d\n¢ %d\nBuy"), Index + 1, 120 + Index * 40),
				FVector2D(656.0f + Col * 270.0f, 230.0f + Row * 194.0f),
				FVector2D(226.0f, 150.0f),
				35 + Index,
				RequiredNames,
				18);
			++OfferGridCount;
		}
		UE_LOG(LogTemp, Log, TEXT("[TDD] UI_VendingBridge_shopOfferGrid: actual=%d expected=8 result=%s"),
			OfferGridCount,
			OfferGridCount == 8 ? TEXT("PASS") : TEXT("FAIL"));
		AddVendingCanvasButton(WBP, Root, FName(TEXT("ShopContinueButton")), FName(TEXT("ShopContinueText")), TEXT("Continue"), FVector2D(1640.0f, 990.0f), FVector2D(220.0f, 58.0f), 50, RequiredNames, 20);
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingTooltip(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("TooltipRoot")), RequiredNames, Root))
		{
			return false;
		}

		UVerticalBox* Body = AddVendingColumnPanel(
			WBP,
			Root,
			FName(TEXT("TooltipPanel")),
			FName(TEXT("TooltipBody")),
			FLinearColor(0.026f, 0.031f, 0.042f, 0.92f),
			FVector2D(18.0f, 18.0f),
			FVector2D(380.0f, 260.0f),
			70,
			RequiredNames);
		if (Body)
		{
			UTextBlock* Title = BridgeUMGImpl::AddDesignerText(WBP, Body, FName(TEXT("TooltipTitleText")), TEXT("VegetableBall"), 30, 0.0f, RequiredNames);
			SetVendingTextColor(Title, FLinearColor(1.0f, 0.95f, 0.85f, 1.0f));
			UTextBlock* DetailBody = BridgeUMGImpl::AddDesignerText(WBP, Body, FName(TEXT("TooltipDetailBodyText")), TEXT(""), 20, 0.0f, RequiredNames);
			UVerticalBox* RowsBox = BridgeUMGImpl::ConstructDesignerWidget<UVerticalBox>(WBP, FName(TEXT("TooltipRowsBox")), RequiredNames);
			UHorizontalBox* Footer = BridgeUMGImpl::ConstructDesignerWidget<UHorizontalBox>(WBP, FName(TEXT("TooltipFooterBox")), RequiredNames);
			if (RowsBox)
			{
				Body->AddChildToVerticalBox(RowsBox);
			}
			if (DetailBody)
			if (Footer)
			{
				Footer->SetVisibility(ESlateVisibility::Collapsed);
				Body->AddChildToVerticalBox(Footer);
				UButton* CloseButton = BridgeUMGImpl::ConstructDesignerWidget<UButton>(WBP, FName(TEXT("TooltipCloseButton")), RequiredNames);
				UTextBlock* CloseText = BridgeUMGImpl::ConstructDesignerWidget<UTextBlock>(WBP, FName(TEXT("TooltipCloseText")), RequiredNames);
				BridgeUMGImpl::SetDesignerText(CloseText, TEXT("X"), 16);
				if (CloseButton && CloseText)
				{
					CloseButton->SetContent(CloseText);
					Footer->AddChildToHorizontalBox(CloseButton);
				}
			}
		}
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}

	bool RebuildVendingSettlement(UWidgetBlueprint* WBP, const FString& Kind)
	{
		TArray<FString> RequiredNames;
		UCanvasPanel* Root = nullptr;
		if (!ResetVendingTree(WBP, FName(TEXT("SettlementVendingRoot")), RequiredNames, Root))
		{
			return false;
		}

		AddVendingPanel(
			WBP,
			Root,
			FName(TEXT("SettlementBackdrop")),
			FLinearColor(0.010f, 0.012f, 0.016f, 0.94f),
			FVector2D(0.0f, 0.0f),
			FVector2D(1920.0f, 1080.0f),
			0,
			RequiredNames);
		UVerticalBox* Column = AddVendingColumnPanel(
			WBP,
			Root,
			FName(TEXT("SettlementPanel")),
			FName(TEXT("SettlementColumn")),
			FLinearColor(0.032f, 0.038f, 0.050f, 0.94f),
			FVector2D(360.0f, 180.0f),
			FVector2D(1200.0f, 690.0f),
			20,
			RequiredNames);
		if (Column)
		{
			UTextBlock* Title = BridgeUMGImpl::AddDesignerText(
				WBP, Column, FName(TEXT("SettlementTitleText")), TEXT("Settlement"), 48, 8.0f, RequiredNames);
			SetVendingTextColor(Title, FLinearColor(1.0f, 0.92f, 0.68f, 1.0f));
			UTextBlock* Summary = BridgeUMGImpl::AddDesignerText(
				WBP, Column, FName(TEXT("SettlementSummaryText")), TEXT("Run complete"), 26, 8.0f, RequiredNames);
			SetVendingTextColor(Summary, FLinearColor(0.92f, 0.94f, 0.98f, 1.0f));
			AddVendingColumnButton(WBP, Column, FName(TEXT("SettlementReturnMenuButton")), FName(TEXT("SettlementReturnMenuText")), TEXT("Return Menu"), 10.0f, RequiredNames);
			AddVendingColumnButton(WBP, Column, FName(TEXT("SettlementRestartButton")), FName(TEXT("SettlementRestartText")), TEXT("Restart"), 10.0f, RequiredNames, 22);
			AddVendingColumnButton(WBP, Column, FName(TEXT("SettlementCloseButton")), FName(TEXT("SettlementCloseText")), TEXT("Close"), 10.0f, RequiredNames);
		}
		return FinishVendingRebuild(WBP, Kind, RequiredNames);
	}
}


bool UUnrealBridgeUMGLibrary::RemoveWidgetFromBlueprint(const FString& WidgetBlueprintPath, const FString& WidgetName)
{
    UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
    if (!WBP || !WBP->WidgetTree)
    {
        return false;
    }

    UWidget* Found = BridgeUMGImpl::FindWidgetByName(WBP, WidgetName);
    if (!Found)
    {
        return false;
    }

    if (UPanelWidget* Parent = Found->GetParent())
    {
        Parent->RemoveChild(Found);
    }
    else if (WBP->WidgetTree->RootWidget == Found)
    {
        WBP->WidgetTree->RootWidget = nullptr;
    }

    return true;
}

bool UUnrealBridgeUMGLibrary::RebuildVendingDesignerWidget(const FString& WidgetBlueprintPath, const FString& WidgetKind)
{
	UWidgetBlueprint* WBP = BridgeUMGImpl::LoadWBP(WidgetBlueprintPath);
	if (!WBP || !WBP->WidgetTree)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TDD] UI_VendingBridge_%sAssetLoaded: actual=0 expected=1 result=FAIL"),
			*WidgetKind);
		return false;
	}

	const FString Kind = WidgetKind.ToLower();
	if (Kind == TEXT("hud"))
	{
		return RebuildVendingHUD(WBP, Kind);
	}
	if (Kind == TEXT("ball_select"))
	{
		return RebuildVendingBallSelect(WBP, Kind);
	}
	if (Kind == TEXT("peg_inventory"))
	{
		return RebuildVendingPegInventory(WBP, Kind);
	}
	if (Kind == TEXT("upgrade_pick"))
	{
		return RebuildVendingUpgradePick(WBP, Kind);
	}
	if (Kind == TEXT("shop"))
	{
		return RebuildVendingShop(WBP, Kind);
	}
	if (Kind == TEXT("tooltip"))
	{
		return RebuildVendingTooltip(WBP, Kind);
	}
	if (Kind == TEXT("settlement"))
	{
		return RebuildVendingSettlement(WBP, Kind);
	}

	UE_LOG(LogTemp, Warning, TEXT("[TDD] UI_VendingBridge_%sKnownKind: actual=0 expected=1 result=FAIL"),
		*WidgetKind);
	return false;
}
