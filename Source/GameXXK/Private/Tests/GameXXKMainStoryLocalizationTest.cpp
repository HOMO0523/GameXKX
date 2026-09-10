#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Narrative/GameXXKMainStoryCatalog.h"
#include "UI/GameXXKMainStoryDialoguePresentation.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKLocalization.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryEnglishCoverageTest,
    "GameXXK.MainStory.EnglishFullTextAndDialoguePresentation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryEnglishCoverageTest::RunTest(const FString&)
{
    const FString OriginalLanguage=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(OriginalLanguage,false);};
    FString Error;
    if(!TestTrue(TEXT("reviewed bilingual catalog reloads"),GameXXKLocalization::ReloadTextCatalog(&Error)))
    {AddError(Error);return false;}
    if(!GameXXKLocalization::SetLanguage(TEXT("en"),false))return false;
    const auto HasHan=[](const FString& Text)
    {for(TCHAR C:Text)if(C>=0x3400&&C<=0x9fff)return true;return false;};
    int32 Fields=0,DialogueLines=0;
    const auto Check=[&](const FText& Source,const FString& Context)
    {
        ++Fields;const FText English=GameXXKLocalization::Localize(Source);
        return TestTrue(*Context,!English.IsEmpty()&&!HasHan(English.ToString()));
    };
    TStrongObjectPtr<UGameXXKDialoguePanelWidget> Panel(NewObject<UGameXXKDialoguePanelWidget>());
    Panel->SetCompactLayout(true);
    for(const auto& Chapter:FGameXXKMainStoryCatalog::Chapters())
    {
        Check(Chapter.Title,Chapter.Id.ToString()+TEXT(" title"));
        Check(Chapter.Summary,Chapter.Id.ToString()+TEXT(" summary"));
    }
    for(const auto& Node:FGameXXKMainStoryCatalog::Nodes())
    {
        for(const FText& Text:{Node.Title,Node.Summary,Node.Objective,Node.Result})Check(Text,Node.Id.ToString()+TEXT(" detail"));
        for(const FText& Hint:Node.Hints)Check(Hint,Node.Id.ToString()+TEXT(" hint"));
        for(const auto& Option:Node.Options)
        {Check(Option.Text,Node.Id.ToString()+TEXT(" option"));Check(Option.Feedback,Node.Id.ToString()+TEXT(" answer feedback"));}
        for(int32 I=0;I<Node.ReplayLineCount();++I)
        {
            const auto View=GameXXKMainStoryDialoguePresentation::LineView(Node,I);
            Check(View.Text,Node.Id.ToString()+FString::Printf(TEXT(" dialogue %d"),I));
            Panel->Present(View);++DialogueLines;
            TestFalse(TEXT("actual dialogue body displays no untranslated Chinese"),HasHan(Panel->GetBodyTextForTest().ToString()));
            TestFalse(TEXT("actual speaker name displays no untranslated Chinese"),HasHan(Panel->GetSpeakerTextForTest().ToString()));
        }
    }
    TestEqual(TEXT("all authored dialogue including aftermath is covered"),DialogueLines,486);
    TestEqual(TEXT("all chapter/node/line/choice/hint/result positions are covered"),Fields,985);
    const auto* Opening=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-02"));
    const auto View=GameXXKMainStoryDialoguePresentation::LineView(*Opening,0);
    Panel->Present(View);const FString English=Panel->GetBodyTextForTest().ToString();
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    TestEqual(TEXT("language switch restores the full original line without advancing"),Panel->GetBodyTextForTest().ToString(),View.Text.ToString());
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    TestEqual(TEXT("language switch updates the already-visible dialogue"),Panel->GetBodyTextForTest().ToString(),English);
    return true;
}
#endif
