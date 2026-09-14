#include "Misc/AutomationTest.h"
#include "UI/GameXXKDialoguePanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDialogueReadingTimeTest,
    "GameXXK.Dialogue.AutoPlay.ReadingTime", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDialogueReadingTimeTest::RunTest(const FString&)
{
    TestEqual(TEXT("Short lines retain a five-second minimum"),UGameXXKDialoguePanelWidget::CalculateAutoPlaySeconds(TEXT("你好。")),5.f);
    TestTrue(TEXT("Long Chinese lines allow reading time"),UGameXXKDialoguePanelWidget::CalculateAutoPlaySeconds(FString::ChrN(100,TEXT('字')))>=20.f);
    FString English;for(int32 I=0;I<60;++I)English+=TEXT("word ");
    TestTrue(TEXT("English is timed by words rather than letters"),FMath::IsNearlyEqual(UGameXXKDialoguePanelWidget::CalculateAutoPlaySeconds(English),20.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDialogueAutoPlayTest,
    "GameXXK.Dialogue.AutoPlay.TimingRefreshAndChoices", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDialogueAutoPlayTest::RunTest(const FString&)
{
    auto* Widget=NewObject<UGameXXKDialoguePanelWidget>();
    const TSharedRef<SWidget> Surface=Widget->TakeWidget();
    int32 Advances=0;
    Widget->SetAdvanceRequested(FGameXXKDialogueAdvanceRequested::CreateLambda([&](){++Advances;}));
    FGameXXKDialoguePresentationView View;View.NodeId=TEXT("Line.A");View.Text=FText::FromString(TEXT("你好。"));
    Widget->Present(View);
    TestFalse(TEXT("Auto play starts disabled"),Widget->IsAutoPlayEnabled());
    Widget->SetAutoPlayEnabled(true);
    Widget->TickAutoPlay(4.f);Widget->Present(View);Widget->TickAutoPlay(1.f);
    TestEqual(TEXT("Refreshing the same line does not postpone the advance"),Advances,1);
    Widget->TickAutoPlay(100.f);
    TestEqual(TEXT("One auto request per displayed line while awaiting the owner"),Advances,1);
    View.NodeId=TEXT("Line.B");Widget->Present(View);Widget->TickAutoPlay(4.f);
    TestEqual(TEXT("New line receives its full reading time"),Advances,1);
    Widget->TickAutoPlay(1.f);TestEqual(TEXT("Second line advances"),Advances,2);
    View.NodeId=TEXT("Choice");View.Options.AddDefaulted();Widget->Present(View);Widget->TickAutoPlay(100.f);
    TestEqual(TEXT("Never select a choice or enter-battle button automatically"),Advances,2);
    Widget->ClearPresentation();Widget->TickAutoPlay(100.f);
    TestEqual(TEXT("Closed dialogue never advances in the background"),Advances,2);
    View.NodeId=TEXT("Line.C");View.Options.Reset();Widget->Present(View);Widget->TickAutoPlay(4.f);
    Widget->RequestAdvanceForTest();Widget->TickAutoPlay(1.f);
    TestEqual(TEXT("Manual advance cannot be followed by a stale auto request"),Advances,3);
    TestTrue(TEXT("The UI toggle is actionable"),Widget->RequestOptionForTest(-4));
    TestFalse(TEXT("The UI toggle disables playback"),Widget->IsAutoPlayEnabled());
    return true;
}
#endif
