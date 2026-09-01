#include "Misc/AutomationTest.h"

#include "Components/BoxComponent.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKSpeechBubbleWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialoguePanelViewModelTest,
	"GameXXK.Dialogue.Presenter.FormalPanelViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialoguePanelViewModelTest::RunTest(const FString& Parameters)
{
	UGameXXKDialoguePanelWidget* Widget = NewObject<UGameXXKDialoguePanelWidget>();
	Widget->TakeWidget();
	FGameXXKDialoguePresentationView View;
	View.NodeId = TEXT("choice");
	View.SpeakerDisplayName = FText::FromString(TEXT("月白"));
	View.Text = FText::FromString(TEXT("你是谁？"));
	FGameXXKDialogueVisibleOption First;
	First.OptionId = TEXT("one");
	First.Text = FText::FromString(TEXT("选项一"));
	First.bEnabled = true;
	View.Options.Add(First);
	FGameXXKDialogueVisibleOption Second;
	Second.OptionId = TEXT("two");
	Second.Text = FText::FromString(TEXT("选项二"));
	Second.bEnabled = false;
	Second.DisabledReason = FText::FromString(TEXT("条件不足"));
	View.Options.Add(Second);
	Widget->Present(View);

	TestEqual(TEXT("formal panel owns one paper frame"), Widget->GetPaperFrameCountForTest(), 1);
	TestEqual(TEXT("formal panel owns one portrait"), Widget->GetPortraitCountForTest(), 1);
	TestTrue(TEXT("formal panel has continue indicator"), Widget->HasContinueIndicatorForTest());
	TestEqual(TEXT("two options visible"), Widget->GetVisibleOptionCountForTest(), 2);
	TestEqual(TEXT("speaker rendered"), Widget->GetSpeakerTextForTest(), FText::FromString(TEXT("月白")));
	TestEqual(TEXT("body rendered"), Widget->GetBodyTextForTest(), FText::FromString(TEXT("你是谁？")));
	TestTrue(TEXT("first option enabled"), Widget->IsOptionEnabledForTest(0));
	TestFalse(TEXT("second option disabled"), Widget->IsOptionEnabledForTest(1));
	TestEqual(TEXT("disabled reason is tooltip"),
		Widget->GetOptionTooltipForTest(1), FText::FromString(TEXT("条件不足")));
	TestFalse(TEXT("unused third option is not visible"), Widget->IsOptionVisibleForTest(2));
	TestFalse(TEXT("choice node hides continue indicator"), Widget->IsContinueIndicatorVisibleForTest());

	FName RequestedOption;
	int32 AdvanceCount = 0;
	Widget->SetAdvanceRequested(FGameXXKDialogueAdvanceRequested::CreateLambda(
		[&AdvanceCount]() { ++AdvanceCount; }));
	Widget->SetOptionRequested(FGameXXKDialogueOptionRequested::CreateLambda(
		[&RequestedOption](const FName OptionId) { RequestedOption = OptionId; }));
	TestTrue(TEXT("enabled option dispatches"), Widget->RequestOptionForTest(0));
	TestEqual(TEXT("enabled option keeps stable ID"), RequestedOption, FName(TEXT("one")));
	RequestedOption = NAME_None;
	TestFalse(TEXT("disabled option does not dispatch"), Widget->RequestOptionForTest(1));
	TestTrue(TEXT("disabled option leaves callback untouched"), RequestedOption.IsNone());

	View.NodeId = TEXT("line");
	View.Options.Reset();
	View.Text = FText::FromString(TEXT("前方路远，当心脚下。"));
	Widget->Present(View);
	TestEqual(TEXT("line has no visible choices"), Widget->GetVisibleOptionCountForTest(), 0);
	TestTrue(TEXT("line shows continue indicator"), Widget->IsContinueIndicatorVisibleForTest());
	Widget->RequestAdvanceForTest();
	TestEqual(TEXT("line advance dispatches once"), AdvanceCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSpeechBubbleAnchorAndClampTest,
	"GameXXK.Dialogue.Presenter.SpeechBubbleAnchorAndClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSpeechBubbleAnchorAndClampTest::RunTest(const FString& Parameters)
{
	UGameXXKSpeechBubbleWidget* Bubble = NewObject<UGameXXKSpeechBubbleWidget>();
	Bubble->TakeWidget();
	FGameXXKDialoguePresentationView View;
	View.NodeId = TEXT("bubble.one");
	View.Text = FText::FromString(TEXT("你是谁？"));
	TestFalse(TEXT("missing anchor rejects presentation"), Bubble->PresentBubble(View, nullptr));
	TestFalse(TEXT("missing anchor leaves no bubble at origin"), Bubble->IsBubbleVisibleForTest());

	AActor* Actor = NewObject<AActor>();
	USceneComponent* Anchor = NewObject<USceneComponent>(Actor);
	Actor->SetRootComponent(Anchor);
	Anchor->SetRelativeLocation(FVector(120.0, 30.0, 180.0));
	TestTrue(TEXT("live component anchors bubble"), Bubble->PresentBubble(View, Anchor));
	TestTrue(TEXT("bubble becomes visible"), Bubble->IsBubbleVisibleForTest());
	TestTrue(TEXT("bubble is non-intercepting"), Bubble->IsBubbleHitTestInvisibleForTest());
	TestEqual(TEXT("bubble presenter owns one reusable bubble"), Bubble->GetBubbleCountForTest(), 1);
	TestEqual(TEXT("bubble renders body only"), Bubble->GetBodyTextForTest(), View.Text);
	TestEqual(TEXT("bubble layout is capped at two lines"), Bubble->GetMaximumLineCountForTest(), 2);
	TestEqual(TEXT("visual top adds only the rendered Z extent"),
		UGameXXKSpeechBubbleWidget::VisualBoundsTopForTest(
			FVector(10.0f, 20.0f, 30.0f),
			FVector(40.0f, 50.0f, 60.0f)),
		FVector(10.0f, 20.0f, 90.0f));
	UBoxComponent* VisualAnchor = NewObject<UBoxComponent>(Actor);
	VisualAnchor->SetBoxExtent(FVector(40.0f, 50.0f, 60.0f));
	TestTrue(TEXT("passive prompt accepts rendered visual-top anchoring"),
		Bubble->PresentBubbleAtVisualTop(View, VisualAnchor));
	TestTrue(TEXT("passive prompt records visual-top anchor mode"),
		Bubble->UsesVisualBoundsTopForTest());

	View.NodeId = TEXT("bubble.two");
	View.Text = FText::FromString(TEXT("本座问你话。你是何人？"));
	TestTrue(TEXT("second line reuses bubble"), Bubble->PresentBubble(View, Anchor));
	TestEqual(TEXT("still exactly one bubble"), Bubble->GetBubbleCountForTest(), 1);
	TestEqual(TEXT("second line replaces body"), Bubble->GetBodyTextForTest(), View.Text);

	const FVector2D ClampedMinimum = UGameXXKSpeechBubbleWidget::ClampToViewportForTest(
		FVector2D(-50.0f, -20.0f), FVector2D(800.0f, 600.0f), FVector2D(320.0f, 100.0f));
	TestEqual(TEXT("left clamp keeps padding"), ClampedMinimum.X, 12.0);
	TestEqual(TEXT("top clamp keeps padding"), ClampedMinimum.Y, 12.0);
	const FVector2D ClampedMaximum = UGameXXKSpeechBubbleWidget::ClampToViewportForTest(
		FVector2D(790.0f, 590.0f), FVector2D(800.0f, 600.0f), FVector2D(320.0f, 100.0f));
	TestEqual(TEXT("right clamp accounts for bubble width"), ClampedMaximum.X, 468.0);
	TestEqual(TEXT("bottom clamp accounts for bubble height"), ClampedMaximum.Y, 488.0);

	TestFalse(TEXT("missing projection controller reports failure"), Bubble->UpdateAnchor(nullptr));
	Bubble->ClearBubble();
	TestFalse(TEXT("clear removes bubble"), Bubble->IsBubbleVisibleForTest());
	return true;
}

#endif
