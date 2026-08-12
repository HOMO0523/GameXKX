#include "GameXXKCardOutcomePreview.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKCardOutcomePreviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardOutcomePreviewWidgetTest
{
	FGameXXKCardOutcomeTextSegment Segment(const TCHAR* Text, const EGameXXKCardOutcomeTone Tone)
	{
		FGameXXKCardOutcomeTextSegment Result;
		Result.Text = FText::FromString(Text);
		Result.Tone = Tone;
		return Result;
	}

	FGameXXKCardOutcomeTextLine Line(
		std::initializer_list<FGameXXKCardOutcomeTextSegment> Segments)
	{
		FGameXXKCardOutcomeTextLine Result;
		for (const FGameXXKCardOutcomeTextSegment& Value : Segments)
		{
			Result.Segments.Add(Value);
		}
		return Result;
	}

	int32 CountWidgetsOfClass(const UGameXXKCardOutcomePreviewWidget* Widget, const UClass* Class)
	{
		TArray<UWidget*> Widgets;
		if (Widget && Widget->WidgetTree)
		{
			Widget->WidgetTree->GetAllWidgets(Widgets);
		}
		return Widgets.FilterByPredicate([Class](const UWidget* Child)
		{
			return Child && Child->IsA(Class);
		}).Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewWidgetTest,
	"GameXXK.UI.Battle.CardOutcomePreviewWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewWidgetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewWidgetTest;

	UGameXXKCardOutcomePreviewWidget* Widget = NewObject<UGameXXKCardOutcomePreviewWidget>();
	TestNotNull(TEXT("card outcome preview widget is created without a viewport"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TArray<FGameXXKCardOutcomeTextLine> FocusedLines = {
		Line({Segment(TEXT("伤害 12"), EGameXXKCardOutcomeTone::Damage), Segment(TEXT(" · "), EGameXXKCardOutcomeTone::Neutral), Segment(TEXT("流血 3"), EGameXXKCardOutcomeTone::Dot)}),
		Line({Segment(TEXT("护甲 +5"), EGameXXKCardOutcomeTone::Armor), Segment(TEXT("致死"), EGameXXKCardOutcomeTone::Lethal)})};
	Widget->SetLines(FocusedLines);
	TestEqual(TEXT("representative focused input keeps its rules-owned two rows"), Widget->GetVisibleLineCountForTest(), 2);
	TestEqual(TEXT("first row preserves segment order and concatenates exactly"), Widget->GetPlainLineForTest(0), FString(TEXT("伤害 12 · 流血 3")));
	TestEqual(TEXT("second row remains after the first without widget reordering"), Widget->GetPlainLineForTest(1), FString(TEXT("护甲 +5致死")));
	TestEqual(TEXT("non-empty preview is input-transparent"), Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);

	TSharedRef<SWidget> FirstSlate = Widget->TakeWidget();
	TestEqual(TEXT("non-empty rebuilt Slate root is hit-test invisible"), FirstSlate->GetVisibility(), EVisibility::HitTestInvisible);
	TestEqual(TEXT("two input lines create two horizontal rows"), CountWidgetsOfClass(Widget, UHorizontalBox::StaticClass()), 2);
	TestEqual(TEXT("all five ordered segments create text children"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 5);
	TArray<UWidget*> FirstChildren;
	Widget->WidgetTree->GetAllWidgets(FirstChildren);
	for (const UWidget* Child : FirstChildren)
	{
		if (Child)
		{
			TestTrue(TEXT("every generated child is input-transparent"),
				Child->GetVisibility() == ESlateVisibility::HitTestInvisible
				|| Child->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
		}
	}

	const TArray<TPair<EGameXXKCardOutcomeTone, FLinearColor>> ToneColors = {
		{EGameXXKCardOutcomeTone::Damage, FLinearColor(0.66f, 0.24f, 0.20f, 1.0f)},
		{EGameXXKCardOutcomeTone::Dot, FLinearColor(0.25f, 0.48f, 0.31f, 1.0f)},
		{EGameXXKCardOutcomeTone::Medicine, FLinearColor(0.58f, 0.39f, 0.20f, 1.0f)},
		{EGameXXKCardOutcomeTone::Healing, FLinearColor(0.24f, 0.55f, 0.46f, 1.0f)},
		{EGameXXKCardOutcomeTone::Armor, FLinearColor(0.34f, 0.45f, 0.55f, 1.0f)},
		{EGameXXKCardOutcomeTone::Neutral, FLinearColor(0.79f, 0.75f, 0.66f, 1.0f)},
		{EGameXXKCardOutcomeTone::Lethal, FLinearColor(0.82f, 0.34f, 0.26f, 1.0f)}};
	FGameXXKCardOutcomeTextLine ToneLine;
	for (int32 ToneIndex = 0; ToneIndex < ToneColors.Num(); ++ToneIndex)
	{
		ToneLine.Segments.Add(Segment(*FString::Printf(TEXT("tone%d"), ToneIndex), ToneColors[ToneIndex].Key));
	}
	Widget->SetLines({ToneLine});
	for (int32 ToneIndex = 0; ToneIndex < ToneColors.Num(); ++ToneIndex)
	{
		TestTrue(*FString::Printf(TEXT("tone %d uses the exact low-saturation ink color"), ToneIndex),
			Widget->GetSegmentColorForTest(0, ToneIndex) == ToneColors[ToneIndex].Value);
	}

	const TArray<FGameXXKCardOutcomeTextLine> GroupLines = {
		Line({Segment(TEXT("1P 伤害 8"), EGameXXKCardOutcomeTone::Damage)}),
		Line({Segment(TEXT("2P 中毒 4"), EGameXXKCardOutcomeTone::Dot)}),
		Line({Segment(TEXT("3P 治疗 +2"), EGameXXKCardOutcomeTone::Healing)})};
	Widget->SetLines(GroupLines);
	TestEqual(TEXT("representative group input keeps its rules-owned three rows"), Widget->GetVisibleLineCountForTest(), 3);
	TestEqual(TEXT("group category order remains exactly as supplied by rules"), Widget->GetPlainLineForTest(1), FString(TEXT("2P 中毒 4")));

	TArray<FGameXXKCardOutcomeTextLine> DefensiveInput = GroupLines;
	DefensiveInput.Add(Line({Segment(TEXT("unexpected fourth row"), EGameXXKCardOutcomeTone::Neutral)}));
	Widget->SetLines(DefensiveInput);
	TestEqual(TEXT("mode-independent safety cap trims a fourth row"), Widget->GetVisibleLineCountForTest(), 3);
	TestEqual(TEXT("safety cap keeps the first three rows in order"), Widget->GetPlainLineForTest(2), FString(TEXT("3P 治疗 +2")));
	TestEqual(TEXT("trimmed row cannot be observed"), Widget->GetPlainLineForTest(3), FString());

	Widget->SetLines({Line({Segment(TEXT("replacement"), EGameXXKCardOutcomeTone::Medicine)})});
	TestEqual(TEXT("replacement SetLines removes old rows"), Widget->GetVisibleLineCountForTest(), 1);
	TestEqual(TEXT("replacement SetLines removes old segments"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 1);
	TestEqual(TEXT("replacement text contains no stale content"), Widget->GetPlainLineForTest(0), FString(TEXT("replacement")));

	Widget->Clear();
	TestEqual(TEXT("Clear removes all visible rows"), Widget->GetVisibleLineCountForTest(), 0);
	TestEqual(TEXT("Clear removes generated row children"), CountWidgetsOfClass(Widget, UHorizontalBox::StaticClass()), 0);
	TestEqual(TEXT("Clear removes generated segment children"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 0);
	TestEqual(TEXT("Clear collapses the preview"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("cleared Slate root is collapsed"), FirstSlate->GetVisibility(), EVisibility::Collapsed);

	Widget->ReleaseSlateResources(false);
	TSharedRef<SWidget> RebuiltSlate = Widget->TakeWidget();
	TestEqual(TEXT("rebuild after Clear remains empty"), Widget->GetVisibleLineCountForTest(), 0);
	TestEqual(TEXT("rebuild after Clear remains collapsed"), RebuiltSlate->GetVisibility(), EVisibility::Collapsed);
	Widget->SetLines({Line({Segment(TEXT("fresh"), EGameXXKCardOutcomeTone::Healing), Segment(TEXT(" row"), EGameXXKCardOutcomeTone::Neutral)})});
	TestEqual(TEXT("SetLines after rebuild creates only the new row"), Widget->GetVisibleLineCountForTest(), 1);
	TestEqual(TEXT("SetLines after rebuild creates only the new segments"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 2);
	TestEqual(TEXT("SetLines after rebuild has no stale line text"), Widget->GetPlainLineForTest(0), FString(TEXT("fresh row")));
	TestEqual(TEXT("SetLines after rebuild restores transparent visibility"), Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);
	return true;
}

#endif
