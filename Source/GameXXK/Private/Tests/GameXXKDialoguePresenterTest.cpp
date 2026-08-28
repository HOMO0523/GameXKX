#include "Misc/AutomationTest.h"

#include "Dialogue/GameXXKDialogueTypes.h"
#include "UI/GameXXKDialoguePanelWidget.h"

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

#endif
