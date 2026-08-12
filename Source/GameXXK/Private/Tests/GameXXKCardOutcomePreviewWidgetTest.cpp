#include "GameXXKCardOutcomePreview.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKCardOutcomePreviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

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

	UBorder* RenderedBackground(const UGameXXKCardOutcomePreviewWidget* Widget)
	{
		return Widget && Widget->WidgetTree
			? Cast<UBorder>(Widget->WidgetTree->RootWidget)
			: nullptr;
	}

	UVerticalBox* RenderedRoot(const UGameXXKCardOutcomePreviewWidget* Widget)
	{
		if (!Widget || !Widget->WidgetTree)
		{
			return nullptr;
		}
		if (UBorder* const Background = RenderedBackground(Widget))
		{
			return Cast<UVerticalBox>(Background->GetContent());
		}
		return Cast<UVerticalBox>(Widget->WidgetTree->RootWidget);
	}

	UHorizontalBox* RenderedRow(const UGameXXKCardOutcomePreviewWidget* Widget, const int32 LineIndex)
	{
		UVerticalBox* Root = RenderedRoot(Widget);
		return Root && LineIndex >= 0 && LineIndex < Root->GetChildrenCount()
			? Cast<UHorizontalBox>(Root->GetChildAt(LineIndex))
			: nullptr;
	}

	UTextBlock* RenderedSegment(
		const UGameXXKCardOutcomePreviewWidget* Widget,
		const int32 LineIndex,
		const int32 SegmentIndex)
	{
		UHorizontalBox* Row = RenderedRow(Widget, LineIndex);
		return Row && SegmentIndex >= 0 && SegmentIndex < Row->GetChildrenCount()
			? Cast<UTextBlock>(Row->GetChildAt(SegmentIndex))
			: nullptr;
	}

	FLinearColor ExpectedToneColor(const EGameXXKCardOutcomeTone Tone)
	{
		switch (Tone)
		{
		case EGameXXKCardOutcomeTone::Damage:
			return FLinearColor(0.66f, 0.24f, 0.20f, 1.0f);
		case EGameXXKCardOutcomeTone::Dot:
			return FLinearColor(0.25f, 0.48f, 0.31f, 1.0f);
		case EGameXXKCardOutcomeTone::Medicine:
			return FLinearColor(0.58f, 0.39f, 0.20f, 1.0f);
		case EGameXXKCardOutcomeTone::Healing:
			return FLinearColor(0.24f, 0.55f, 0.46f, 1.0f);
		case EGameXXKCardOutcomeTone::Armor:
			return FLinearColor(0.34f, 0.45f, 0.55f, 1.0f);
		case EGameXXKCardOutcomeTone::Lethal:
			return FLinearColor(0.82f, 0.34f, 0.26f, 1.0f);
		case EGameXXKCardOutcomeTone::Neutral:
		default:
			return FLinearColor(0.79f, 0.75f, 0.66f, 1.0f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePreviewWidgetTest,
	"GameXXK.UI.Battle.CardOutcomePreviewWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePreviewWidgetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomePreviewWidgetTest;

	{
		UGameXXKCardOutcomePreviewWidget* ClearBeforeBuildWidget = NewObject<UGameXXKCardOutcomePreviewWidget>();
		TestNotNull(TEXT("pre-build Clear fixture is created"), ClearBeforeBuildWidget);
		if (ClearBeforeBuildWidget)
		{
			ClearBeforeBuildWidget->Clear();
			TestEqual(TEXT("Clear before first TakeWidget is safe and collapsed"),
				ClearBeforeBuildWidget->GetVisibility(), ESlateVisibility::Collapsed);
			TSharedRef<SWidget> ClearBeforeBuildSlate = ClearBeforeBuildWidget->TakeWidget();
			TestEqual(TEXT("Clear before first TakeWidget builds a collapsed Slate wrapper"),
				ClearBeforeBuildSlate->GetVisibility(), EVisibility::Collapsed);
			UVerticalBox* EmptyRoot = RenderedRoot(ClearBeforeBuildWidget);
			TestNotNull(TEXT("Clear before first TakeWidget still builds a real vertical root"), EmptyRoot);
			TestEqual(TEXT("Clear before first TakeWidget leaves the real root empty"),
				EmptyRoot ? EmptyRoot->GetChildrenCount() : 0, 0);
		}
	}

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

	TSharedPtr<SWidget> FirstSlate = Widget->TakeWidget();
	TWeakPtr<SWidget> OldSlateWeak = FirstSlate;
	TestEqual(TEXT("non-empty rebuilt Slate root is hit-test invisible"), FirstSlate->GetVisibility(), EVisibility::HitTestInvisible);
	UBorder* const FocusedBackground = RenderedBackground(Widget);
	TestNotNull(TEXT("the runtime WidgetTree root is a real tooltip-paper background"), FocusedBackground);
	if (FocusedBackground)
	{
		TestEqual(TEXT("the tooltip-paper background never intercepts input"),
			FocusedBackground->GetVisibility(), ESlateVisibility::HitTestInvisible);
		TestEqual(TEXT("the tooltip-paper background uses a nine-slice box brush"),
			FocusedBackground->Background.DrawAs, ESlateBrushDrawType::Box);
		const UObject* const BackgroundResource = FocusedBackground->Background.GetResourceObject();
		TestNotNull(TEXT("the tooltip-paper background has a loaded project texture"), BackgroundResource);
		TestEqual(TEXT("the tooltip reuses the approved low-saturation paper asset"),
			BackgroundResource ? BackgroundResource->GetPathName() : FString(),
			FString(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TooltipPaper.T_MasterV2_TooltipPaper")));
		TestEqual(TEXT("the tooltip-paper background keeps compact readable padding"),
			FocusedBackground->GetPadding(), FMargin(10.0f, 6.0f));
	}
	UVerticalBox* FocusedRoot = RenderedRoot(Widget);
	TestNotNull(TEXT("the tooltip-paper background contains the rendered vertical line box"), FocusedRoot);
	if (!FocusedRoot)
	{
		return false;
	}
	TestEqual(TEXT("the rendered root directly contains the two focused rows"), FocusedRoot->GetChildrenCount(), 2);
	TestEqual(TEXT("the rendered root never intercepts input"), FocusedRoot->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("two input lines create two horizontal rows"), CountWidgetsOfClass(Widget, UHorizontalBox::StaticClass()), 2);
	TestEqual(TEXT("all five ordered segments create text children"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 5);
	for (int32 LineIndex = 0; LineIndex < FocusedLines.Num(); ++LineIndex)
	{
		UHorizontalBox* Row = Cast<UHorizontalBox>(FocusedRoot->GetChildAt(LineIndex));
		TestNotNull(*FString::Printf(TEXT("rendered focused row %d is horizontal"), LineIndex), Row);
		if (!Row)
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("rendered focused row %d preserves its segment count"), LineIndex),
			Row->GetChildrenCount(), FocusedLines[LineIndex].Segments.Num());
		TestEqual(*FString::Printf(TEXT("rendered focused row %d never intercepts input"), LineIndex),
			Row->GetVisibility(), ESlateVisibility::HitTestInvisible);
		for (int32 SegmentIndex = 0; SegmentIndex < FocusedLines[LineIndex].Segments.Num(); ++SegmentIndex)
		{
			const FGameXXKCardOutcomeTextSegment& ExpectedSegment = FocusedLines[LineIndex].Segments[SegmentIndex];
			UTextBlock* TextBlock = Cast<UTextBlock>(Row->GetChildAt(SegmentIndex));
			TestNotNull(*FString::Printf(TEXT("focused row %d segment %d is rendered as text"), LineIndex, SegmentIndex), TextBlock);
			if (!TextBlock)
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("focused row %d segment %d keeps exact text order"), LineIndex, SegmentIndex),
				TextBlock->GetText().ToString(), ExpectedSegment.Text.ToString());
			TestTrue(*FString::Printf(TEXT("focused row %d segment %d uses its exact rendered tone"), LineIndex, SegmentIndex),
				TextBlock->GetColorAndOpacity().GetSpecifiedColor() == ExpectedToneColor(ExpectedSegment.Tone));
			TestEqual(*FString::Printf(TEXT("focused row %d segment %d uses font size 18"), LineIndex, SegmentIndex),
				TextBlock->GetFont().Size, 18.0f);
			TestEqual(*FString::Printf(TEXT("focused row %d segment %d uses one-pixel outline"), LineIndex, SegmentIndex),
				TextBlock->GetFont().OutlineSettings.OutlineSize, 1);
			TestEqual(*FString::Printf(TEXT("focused row %d segment %d never intercepts input"), LineIndex, SegmentIndex),
				TextBlock->GetVisibility(), ESlateVisibility::HitTestInvisible);
		}
	}
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
	UVerticalBox* ToneRoot = RenderedRoot(Widget);
	TestNotNull(TEXT("tone fixture retains the real rendered root"), ToneRoot);
	TestEqual(TEXT("tone fixture renders one row"), ToneRoot ? ToneRoot->GetChildrenCount() : 0, 1);
	UHorizontalBox* ToneRow = ToneRoot ? Cast<UHorizontalBox>(ToneRoot->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("tone fixture renders one horizontal row"), ToneRow);
	TestEqual(TEXT("tone fixture renders all seven segments"), ToneRow ? ToneRow->GetChildrenCount() : 0, ToneColors.Num());
	for (int32 ToneIndex = 0; ToneIndex < ToneColors.Num(); ++ToneIndex)
	{
		UTextBlock* ToneText = ToneRow ? Cast<UTextBlock>(ToneRow->GetChildAt(ToneIndex)) : nullptr;
		TestNotNull(*FString::Printf(TEXT("tone %d has a real rendered text child"), ToneIndex), ToneText);
		if (ToneText)
		{
			TestEqual(*FString::Printf(TEXT("tone %d rendered text keeps segment order"), ToneIndex),
				ToneText->GetText().ToString(), FString::Printf(TEXT("tone%d"), ToneIndex));
			TestTrue(*FString::Printf(TEXT("tone %d real text uses the exact low-saturation ink color"), ToneIndex),
				ToneText->GetColorAndOpacity().GetSpecifiedColor() == ToneColors[ToneIndex].Value);
			TestEqual(*FString::Printf(TEXT("tone %d real text uses font size 18"), ToneIndex), ToneText->GetFont().Size, 18.0f);
			TestEqual(*FString::Printf(TEXT("tone %d real text uses one-pixel outline"), ToneIndex),
				ToneText->GetFont().OutlineSettings.OutlineSize, 1);
			TestEqual(*FString::Printf(TEXT("tone %d real text never intercepts input"), ToneIndex),
				ToneText->GetVisibility(), ESlateVisibility::HitTestInvisible);
		}
		TestTrue(*FString::Printf(TEXT("tone %d uses the exact low-saturation ink color"), ToneIndex),
			Widget->GetSegmentColorForTest(0, ToneIndex) == ToneColors[ToneIndex].Value);
	}
	TestEqual(TEXT("negative line text access is safe"), Widget->GetPlainLineForTest(-1), FString());
	TestEqual(TEXT("out-of-range line text access is safe"), Widget->GetPlainLineForTest(1), FString());
	TestTrue(TEXT("negative line color access is transparent"),
		Widget->GetSegmentColorForTest(-1, 0) == FLinearColor::Transparent);
	TestTrue(TEXT("negative segment color access is transparent"),
		Widget->GetSegmentColorForTest(0, -1) == FLinearColor::Transparent);
	TestTrue(TEXT("out-of-range segment color access is transparent"),
		Widget->GetSegmentColorForTest(0, ToneColors.Num()) == FLinearColor::Transparent);

	Widget->SetLines({Line({Segment(TEXT("invalid tone"), static_cast<EGameXXKCardOutcomeTone>(255))})});
	UTextBlock* InvalidToneText = RenderedSegment(Widget, 0, 0);
	TestNotNull(TEXT("invalid tone still creates a real rendered text child"), InvalidToneText);
	TestEqual(TEXT("invalid tone real text preserves its supplied text"),
		InvalidToneText ? InvalidToneText->GetText().ToString() : FString(), FString(TEXT("invalid tone")));
	TestTrue(TEXT("invalid tone real text falls back to Neutral color"),
		InvalidToneText
		&& InvalidToneText->GetColorAndOpacity().GetSpecifiedColor()
			== FLinearColor(0.79f, 0.75f, 0.66f, 1.0f));

	Widget->SetLines({});
	TestEqual(TEXT("empty SetLines collapses the wrapper"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("empty SetLines clears every real rendered row"),
		RenderedRoot(Widget) ? RenderedRoot(Widget)->GetChildrenCount() : 0, 0);

	Widget->SetLines({Line({})});
	TestEqual(TEXT("empty-segment line remains one explicit rendered row"),
		RenderedRoot(Widget) ? RenderedRoot(Widget)->GetChildrenCount() : 0, 1);
	UHorizontalBox* EmptySegmentRow = RenderedRow(Widget, 0);
	TestNotNull(TEXT("empty-segment line renders a real horizontal row"), EmptySegmentRow);
	TestEqual(TEXT("empty-segment line renders zero text children"),
		EmptySegmentRow ? EmptySegmentRow->GetChildrenCount() : 0, 0);
	TestEqual(TEXT("empty-segment line keeps the wrapper input-transparent"),
		Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);

	Widget->SetLines({Line({Segment(TEXT(""), EGameXXKCardOutcomeTone::Neutral)})});
	UTextBlock* EmptyText = RenderedSegment(Widget, 0, 0);
	TestNotNull(TEXT("empty text segment still creates a real text child"), EmptyText);
	TestEqual(TEXT("empty text segment renders an explicitly empty value"),
		EmptyText ? EmptyText->GetText().ToString() : FString(TEXT("missing")), FString());
	TestEqual(TEXT("empty text segment remains input-transparent"),
		EmptyText ? EmptyText->GetVisibility() : ESlateVisibility::Visible, ESlateVisibility::HitTestInvisible);

	const TArray<FGameXXKCardOutcomeTextLine> GroupLines = {
		Line({Segment(TEXT("1P 伤害 8"), EGameXXKCardOutcomeTone::Damage)}),
		Line({Segment(TEXT("2P 中毒 4"), EGameXXKCardOutcomeTone::Dot)}),
		Line({Segment(TEXT("3P 治疗 +2"), EGameXXKCardOutcomeTone::Healing)})};
	Widget->SetLines(GroupLines);
	TestEqual(TEXT("representative group input keeps its rules-owned three rows"), Widget->GetVisibleLineCountForTest(), 3);
	TestEqual(TEXT("group category order remains exactly as supplied by rules"), Widget->GetPlainLineForTest(1), FString(TEXT("2P 中毒 4")));
	UVerticalBox* GroupRoot = RenderedRoot(Widget);
	TestEqual(TEXT("the real group tree contains three rows in rules order"), GroupRoot ? GroupRoot->GetChildrenCount() : 0, 3);
	for (int32 LineIndex = 0; LineIndex < GroupLines.Num(); ++LineIndex)
	{
		UHorizontalBox* Row = GroupRoot ? Cast<UHorizontalBox>(GroupRoot->GetChildAt(LineIndex)) : nullptr;
		UTextBlock* TextBlock = Row && Row->GetChildrenCount() == 1 ? Cast<UTextBlock>(Row->GetChildAt(0)) : nullptr;
		TestNotNull(*FString::Printf(TEXT("group row %d has its real text child"), LineIndex), TextBlock);
		if (TextBlock)
		{
			TestEqual(*FString::Printf(TEXT("group row %d real text preserves rules order"), LineIndex),
				TextBlock->GetText().ToString(), GroupLines[LineIndex].Segments[0].Text.ToString());
		}
	}

	TArray<FGameXXKCardOutcomeTextLine> DefensiveInput = GroupLines;
	DefensiveInput.Add(Line({Segment(TEXT("unexpected fourth row"), EGameXXKCardOutcomeTone::Neutral)}));
	Widget->SetLines(DefensiveInput);
	TestEqual(TEXT("mode-independent safety cap trims a fourth row"), Widget->GetVisibleLineCountForTest(), 3);
	TestEqual(TEXT("safety cap keeps the first three rows in order"), Widget->GetPlainLineForTest(2), FString(TEXT("3P 治疗 +2")));
	TestEqual(TEXT("trimmed row cannot be observed"), Widget->GetPlainLineForTest(3), FString());
	UVerticalBox* DefensiveRoot = RenderedRoot(Widget);
	TestEqual(TEXT("real rendered tree applies the absolute three-row safety cap"),
		DefensiveRoot ? DefensiveRoot->GetChildrenCount() : 0, 3);
	TArray<UWidget*> DefensiveRenderedObjects;
	if (DefensiveRoot)
	{
		for (int32 LineIndex = 0; LineIndex < DefensiveRoot->GetChildrenCount(); ++LineIndex)
		{
			if (UHorizontalBox* Row = Cast<UHorizontalBox>(DefensiveRoot->GetChildAt(LineIndex)))
			{
				DefensiveRenderedObjects.Add(Row);
				for (int32 SegmentIndex = 0; SegmentIndex < Row->GetChildrenCount(); ++SegmentIndex)
				{
					DefensiveRenderedObjects.Add(Row->GetChildAt(SegmentIndex));
				}
			}
		}
	}

	Widget->SetLines({Line({Segment(TEXT("replacement"), EGameXXKCardOutcomeTone::Medicine)})});
	TestEqual(TEXT("replacement SetLines removes old rows"), Widget->GetVisibleLineCountForTest(), 1);
	TestEqual(TEXT("replacement SetLines removes old segments"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 1);
	TestEqual(TEXT("replacement text contains no stale content"), Widget->GetPlainLineForTest(0), FString(TEXT("replacement")));
	UVerticalBox* ReplacementRoot = RenderedRoot(Widget);
	TestEqual(TEXT("replacement real tree has exactly one row"), ReplacementRoot ? ReplacementRoot->GetChildrenCount() : 0, 1);
	UHorizontalBox* ReplacementRow = RenderedRow(Widget, 0);
	TestNotNull(TEXT("replacement real row exists"), ReplacementRow);
	TestEqual(TEXT("replacement real row has exactly one segment"), ReplacementRow ? ReplacementRow->GetChildrenCount() : 0, 1);
	UTextBlock* ReplacementText = RenderedSegment(Widget, 0, 0);
	TestNotNull(TEXT("replacement real text exists"), ReplacementText);
	TestEqual(TEXT("replacement real text contains only the new value"),
		ReplacementText ? ReplacementText->GetText().ToString() : FString(), FString(TEXT("replacement")));
	TestTrue(TEXT("replacement row is absent from every detached defensive object"),
		ReplacementRow && !DefensiveRenderedObjects.Contains(ReplacementRow));
	TestTrue(TEXT("replacement text is absent from every detached defensive object"),
		ReplacementText && !DefensiveRenderedObjects.Contains(ReplacementText));
	const TArray<UWidget*> ReplacementRenderedObjects = {ReplacementRow, ReplacementText};
	TWeakPtr<SWidget> OldReplacementTextSlateWeak;
	{
		TSharedPtr<SWidget> OldReplacementTextSlate = ReplacementText ? ReplacementText->GetCachedWidget() : nullptr;
		TestTrue(TEXT("replacement text is attached to the original live Slate tree"), OldReplacementTextSlate.IsValid());
		OldReplacementTextSlateWeak = OldReplacementTextSlate;
	}

	Widget->Clear();
	TestEqual(TEXT("Clear removes all visible rows"), Widget->GetVisibleLineCountForTest(), 0);
	TestEqual(TEXT("Clear removes generated row children"), CountWidgetsOfClass(Widget, UHorizontalBox::StaticClass()), 0);
	TestEqual(TEXT("Clear removes generated segment children"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 0);
	UVerticalBox* ClearedRoot = RenderedRoot(Widget);
	TestNotNull(TEXT("Clear retains the real root for reuse"), ClearedRoot);
	TestEqual(TEXT("Clear directly removes every real row child"), ClearedRoot ? ClearedRoot->GetChildrenCount() : 0, 0);
	TestEqual(TEXT("Clear collapses the preview"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("cleared Slate root is collapsed"), FirstSlate->GetVisibility(), EVisibility::Collapsed);

	Widget->ReleaseSlateResources(false);
	FirstSlate.Reset();
	TestFalse(TEXT("releasing Slate resources and dropping all old wrapper refs destroys the old wrapper"), OldSlateWeak.IsValid());
	TestFalse(TEXT("releasing the old tree destroys the old replacement text Slate widget"), OldReplacementTextSlateWeak.IsValid());
	TSharedPtr<SWidget> RebuiltSlate = Widget->TakeWidget();
	TestTrue(TEXT("TakeWidget after the old wrapper expires returns a new live wrapper"), RebuiltSlate.IsValid());
	TestEqual(TEXT("rebuild after Clear remains empty"), Widget->GetVisibleLineCountForTest(), 0);
	TestEqual(TEXT("rebuild after Clear remains collapsed"), RebuiltSlate->GetVisibility(), EVisibility::Collapsed);
	UVerticalBox* RebuiltRoot = RenderedRoot(Widget);
	TestNotNull(TEXT("Slate rebuild exposes a real vertical root"), RebuiltRoot);
	TestEqual(TEXT("Slate rebuild keeps the real root free of stale rows"), RebuiltRoot ? RebuiltRoot->GetChildrenCount() : 0, 0);
	Widget->SetLines({Line({Segment(TEXT("fresh"), EGameXXKCardOutcomeTone::Healing), Segment(TEXT(" row"), EGameXXKCardOutcomeTone::Neutral)})});
	TestEqual(TEXT("SetLines after rebuild creates only the new row"), Widget->GetVisibleLineCountForTest(), 1);
	TestEqual(TEXT("SetLines after rebuild creates only the new segments"), CountWidgetsOfClass(Widget, UTextBlock::StaticClass()), 2);
	TestEqual(TEXT("SetLines after rebuild has no stale line text"), Widget->GetPlainLineForTest(0), FString(TEXT("fresh row")));
	TestEqual(TEXT("SetLines after rebuild restores transparent visibility"), Widget->GetVisibility(), ESlateVisibility::HitTestInvisible);
	UVerticalBox* FreshRoot = RenderedRoot(Widget);
	TestEqual(TEXT("fresh real tree has exactly one row"), FreshRoot ? FreshRoot->GetChildrenCount() : 0, 1);
	UHorizontalBox* FreshRow = RenderedRow(Widget, 0);
	TestNotNull(TEXT("fresh real row exists"), FreshRow);
	TestEqual(TEXT("fresh real row has exactly two segments"), FreshRow ? FreshRow->GetChildrenCount() : 0, 2);
	UTextBlock* FreshText = RenderedSegment(Widget, 0, 0);
	UTextBlock* FreshSuffix = RenderedSegment(Widget, 0, 1);
	TSharedPtr<SWidget> FreshTextSlate = FreshText ? FreshText->GetCachedWidget() : nullptr;
	TSharedPtr<SWidget> FreshSuffixSlate = FreshSuffix ? FreshSuffix->GetCachedWidget() : nullptr;
	TestTrue(TEXT("fresh first UTextBlock has a live cached widget after the old text Slate expires"),
		!OldReplacementTextSlateWeak.IsValid() && FreshTextSlate.IsValid());
	TestTrue(TEXT("fresh second UTextBlock has a live cached widget after the old text Slate expires"),
		!OldReplacementTextSlateWeak.IsValid() && FreshSuffixSlate.IsValid());
	TestEqual(TEXT("fresh first real segment contains exact text"),
		FreshText ? FreshText->GetText().ToString() : FString(), FString(TEXT("fresh")));
	TestEqual(TEXT("fresh second real segment contains exact text"),
		FreshSuffix ? FreshSuffix->GetText().ToString() : FString(), FString(TEXT(" row")));
	TestTrue(TEXT("fresh row is absent from every detached replacement object"),
		FreshRow && !ReplacementRenderedObjects.Contains(FreshRow));
	TestTrue(TEXT("fresh first text is absent from every detached replacement object"),
		FreshText && !ReplacementRenderedObjects.Contains(FreshText));
	TestTrue(TEXT("fresh second text is absent from every detached replacement object"),
		FreshSuffix && !ReplacementRenderedObjects.Contains(FreshSuffix));
	TestTrue(TEXT("fresh real text contains no replacement value"),
		FreshText && FreshSuffix
		&& FreshText->GetText().ToString() + FreshSuffix->GetText().ToString() == TEXT("fresh row"));
	return true;
}

#endif
