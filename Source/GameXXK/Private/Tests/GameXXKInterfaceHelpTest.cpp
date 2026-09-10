#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInterfaceHelpWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "UI/GameXXKLocalization.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKInterfaceOperationsTest,
    "GameXXK.Guide.InterfaceHelp.ActualOperations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKInterfaceOperationsTest::RunTest(const FString&)
{
    const FString Language=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Language,false);};
    for(const TCHAR* Culture:{TEXT("zh-Hans"),TEXT("en")})
    {
        GameXXKLocalization::SetLanguage(Culture,false);
        auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
        MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
        if(!MVP->StartGame())return false;
        MVP->GetMutableRuntimeState().GuideProgress.CompletedGuideStepIds.Add(TEXT("Existing.Step"));
        MVP->GetMutableRuntimeState().GuideProgress.AcademyRewardedCourses.Add(TEXT("Academy.Basic"));
        MVP->GetMutableRuntimeState().GuideProgress.AcademyCompletedLessons.Add(TEXT("Academy.Basic"),2);
        FString FixtureError;
        if(!TestTrue(TEXT("onboarding starts from a valid savable state: ")+FixtureError,FGameXXKSaveMigration::ValidateRuntimeState(MVP->GetRuntimeState(),FixtureError)))
        {AddError(FixtureError);return false;}
        const int32 Gold=MVP->GetRuntimeState().PlayerGold;
        auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(MVP);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
        const auto Draft=Host->GetEmbeddedPendingDeckIdsForTest();
        Host->HandleActionClicked(652);Host->HandleActionClicked(662);
        auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
        if(!TestNotNull(TEXT("operation course exists"),Help))return false;
        TestTrue(TEXT("tutorial mask fills the window outside the scaled backpack"),Help->GetParent()==Host->WidgetTree->RootWidget);
        Help->WidgetTree->ForEachWidget([&](UWidget* Widget)
        {
            if(const auto* Backing=Cast<UBorder>(Widget))
                TestTrue(TEXT("white guide copy has no local background panel"),Backing->Background.DrawAs==ESlateBrushDrawType::NoDrawType);
        });
        auto Tick=[&](){Host->TickForTest(0);Help->NativeTick(FGeometry(),.05f);Host->TickForTest(0);Help->NativeTick(FGeometry(),.05f);};
        for(int32 Index=0;Index<23;++Index)
        {
            Tick();
            if(!TestEqual(TEXT("one current step"),Help->GetCurrentStepForTest(),Index))break;
            const FName Completion=Help->GetCurrentCompletionIdForTest();
            auto* Target=Help->GetCurrentTargetForTest();
            if(!TestNotNull(Completion.ToString()+TEXT(" is present in the real workbench"),Target))break;
            if(Index==2)
            {
                Help->Dismiss();Host->HandleActionClicked(652);Host->HandleActionClicked(662);Tick();
                TestEqual(TEXT("reopening resumes saved operation"),Help->GetCurrentStepForTest(),Index);
                Target=Help->GetCurrentTargetForTest();
            }
            if(Completion==TEXT("UI.Basics.V1.InspectCard"))
            {
                Help->ConfirmReadingForTest();TestEqual(TEXT("hover lesson cannot skip inspection"),Help->GetCurrentStepForTest(),Index);
                auto Slate=Target->TakeWidget();Slate->OnMouseEnter(FGeometry(),FPointerEvent());Help->NativeTick(FGeometry(),1.1f);
                Help->ConfirmReadingForTest();Slate->OnMouseLeave(FPointerEvent());
            }
            else if(Completion==TEXT("UI.Basics.V1.ToolUse"))Help->ConfirmReadingForTest();
            else if(auto* Button=Cast<UButton>(Target))
            {
                Help->ConfirmReadingForTest();TestEqual(TEXT("next cannot replace the real click"),Help->GetCurrentStepForTest(),Index);
                Button->OnClicked.Broadcast();
            }
            else Help->ConfirmReadingForTest();
            Tick();TestTrue(Completion.ToString()+TEXT(" persisted only after the operation"),MVP->GetRuntimeState().GuideProgress.CompletedGuideStepIds.Contains(Completion));
        }
        TestFalse(TEXT("course closes after actual final action"),Help->IsOpen());
        TestEqual(TEXT("guide spends no gold"),MVP->GetRuntimeState().PlayerGold,Gold);
        TestTrue(TEXT("prior guide progress is preserved"),MVP->GetRuntimeState().GuideProgress.CompletedGuideStepIds.Contains(TEXT("Existing.Step")));
        TestTrue(TEXT("battle tutorial rewards are preserved"),MVP->GetRuntimeState().GuideProgress.AcademyRewardedCourses.Contains(TEXT("Academy.Basic")));
        TestTrue(TEXT("draft remains intact"),Host->GetEmbeddedPendingDeckIdsForTest()==Draft);
        Host->HandleActionClicked(652);Host->HandleActionClicked(662);Tick();
        TestEqual(TEXT("a completed course can be replayed"),Help->GetCurrentStepForTest(),0);Help->Dismiss();
        MVP->ResetSaveSlotWriteDelegateForTest();
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKInterfaceHelpProgressTest,
	"GameXXK.Guide.InterfaceHelp.ReadOnlyAndDismissible", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKInterfaceHelpProgressTest::RunTest(const FString&)
{
	auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts"), Subsystem->StartGame())) return false;
	auto& Progress = Subsystem->GetMutableRuntimeState().GuideProgress;
	Progress.CompletedGuideStepIds.Add(TEXT("Existing.Step"));
	Progress.AcademyRewardedCourses.Add(TEXT("Academy.Basics"));
	auto* Workbench = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem); Workbench->ConstructForTest();
	Workbench->OpenWorkbench(); Workbench->OpenBackpack();
	const bool AllowedBefore = FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Battle.EndTurn"));
	Workbench->ShowInterfaceHelp();
	auto* Help = Cast<UGameXXKInterfaceHelpWidget>(Workbench->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
	if (!TestNotNull(TEXT("a reusable help surface exists"), Help)) return false;
	TestTrue(TEXT("help opens on demand"), Help->IsOpen());
	TestTrue(TEXT("help includes an overview and visible interface areas"), Help->GetStepCountForTest() >= 2);
	Help->Dismiss();
	TestFalse(TEXT("help closes without an input gate"), Help->IsOpen());
	TestNull(TEXT("No question-mark button beside Quit"),Workbench->WidgetTree->FindWidget(TEXT("TopToolbarHelp")));
	Workbench->HandleActionClicked(652);
	TestNotNull(TEXT("Tutorials own the interface guide entry"),Workbench->WidgetTree->FindWidget(TEXT("TutorialInterfaceHelpButton")));
	Workbench->HandleActionClicked(662);
	TestTrue(TEXT("Tutorial entry opens the complete interface guide"),Help->IsOpen()&&Help->GetStepCountForTest()>=17);
	Help->Dismiss();
	TestEqual(TEXT("help does not change gameplay action gates"), FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Battle.EndTurn")), AllowedBefore);
	TestTrue(TEXT("completed guide steps remain"), Progress.CompletedGuideStepIds.Contains(TEXT("Existing.Step")));
	TestTrue(TEXT("first-clear reward records remain"), Progress.AcademyRewardedCourses.Contains(TEXT("Academy.Basics")));
	return true;
}
#endif
