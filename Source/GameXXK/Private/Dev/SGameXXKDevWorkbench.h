#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "UObject/GCObject.h"

class UGameXXKDevToolsSubsystem;
class SVerticalBox;
class SEditableTextBox;
class SComboBoxBase;
class SScrollBox;
class SScrollBar;

/** Slate-only tooling surface, using the game's approved paper and ink resources. */
class SGameXXKDevWorkbench : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SGameXXKDevWorkbench) {}
		SLATE_ARGUMENT(UGameXXKDevToolsSubsystem*, Tools)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void RefreshFromGame() { Rebuild(); RebuildInspector(); }
	virtual void Tick(const FGeometry& Geometry,double CurrentTime,float DeltaTime) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SGameXXKDevWorkbench"); }
private:
	TWeakObjectPtr<UGameXXKDevToolsSubsystem> Tools;
	TArray<TObjectPtr<UObject>> Resources;
	FSlateBrush PaperBrush, ShadeBrush, DividerBrush;
	FButtonStyle ActionStyle, QuietStyle;
	FComboBoxStyle ComboStyle;
	FTableRowStyle ComboRowStyle;
	FEditableTextBoxStyle EditStyle;
	FScrollBarStyle ScrollStyle;
	TSharedPtr<SVerticalBox> Body, Inspector;
	TSharedPtr<SEditableTextBox> Search;
	TSharedPtr<SEditableTextBox> CommandInput;
	TMap<FString,TSharedPtr<SEditableTextBox>> Fields;
	TMap<FString,FString> Choices;
	FString ActiveTab = TEXT("home");
	FString ActiveItem;
	FString CharacterId = TEXT("Player");
	FString SearchText;
	FString LastResponse;
	TSharedPtr<class FJsonObject> InspectorState;
	double LastInspectionTime=0;
	struct FInkScrollPair { TWeakPtr<SScrollBox> Box; TWeakPtr<SScrollBar> Bar; float Fraction=0.2f; };
	TArray<TSharedPtr<FInkScrollPair>> ScrollPairs;
	TSharedRef<SWidget> Scroller(TSharedRef<SWidget> Content);
	TSharedRef<SWidget> Text(const FString& Value, int32 Size=16, bool bDisplay=false, FLinearColor Color=FLinearColor::Black);
	TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool bPrimary=false);
	TSharedRef<SWidget> Field(const FString& Key, const FString& Label, const FString& Default, float Width=120);
	TSharedRef<SWidget> Choice(const FString& Key, const FString& Label, const TArray<TPair<FString,FString>>& Options, const FString& Default);
	FString Value(const FString& Key) const;
	TSharedRef<SWidget> Section(const FString& Title, TSharedRef<SWidget> Content);
	void Rebuild();
	void RebuildInspector();
	void Run(const FString& Command, const TSharedPtr<class FJsonObject>& Args=nullptr);
	TSharedRef<SWidget> BuildHome();
	TSharedRef<SWidget> BuildItems();
	TSharedRef<SWidget> BuildEquipment();
	TSharedRef<SWidget> BuildBattle();
	TSharedRef<SWidget> BuildRecords();
};
