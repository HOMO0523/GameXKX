#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKLocalization.h"
#include "GameXXKTalentCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKExitOfflineHintTest,"GameXXK.DesktopTraining.Workbench.ExitOfflineCaps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKExitOfflineHintTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
    auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Widget->SetMVPSubsystem(MVP);Widget->ConstructForTest();Widget->OpenWorkbench();Widget->OpenBackpack();
    auto& Talents=MVP->GetMutableRuntimeState().Talents;
    TFunction<void(FName,int32)> Rank=[&](FName Id,int32 Count)
    {
        const auto* Node=FGameXXKTalentCatalog::Find(Id);if(!Node){AddError(Id.ToString());return;}
        for(FName Dependency:Node->PrerequisiteIds)if(!Talents.NodeRanks.Contains(Dependency))Rank(Dependency,1);
        Talents.NodeRanks.Add(Id,Count);
    };
    for(const TCHAR* Language:{TEXT("zh-Hans"),TEXT("en")})for(int32 Case=0;Case<3;++Case)
    {
        GameXXKLocalization::SetLanguage(Language,false);Talents.NodeRanks.Reset();
        if(Case>0)Rank(TEXT("Talent.Entry.IdleOffline"),1);
        if(Case==2){Rank(TEXT("Talent.Chest.OfflineTime.01"),1);Rank(TEXT("Talent.Idle.OfflineGoldTime.01"),1);Rank(TEXT("Talent.Idle.OfflineExperienceTime.01"),2);}
        Widget->HandleDesktopActionForTest(15);Widget->TickForTest(0);
        auto* Hint=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("ExitOfflineHint")));
        if(!TestNotNull(TEXT("exit prompt shows the offline hint"),Hint))return false;
        const bool English=GameXXKLocalization::IsEnglish();const FString Copy=Hint->GetText().ToString();
        if(Case==0)TestEqual(TEXT("locked talent has one concise line"),Copy,English?FString(TEXT("Unlock offline rewards in Talents.")):FString(TEXT("天赋解锁离线奖励")));
        else
        {
            const auto Projection=MVP->GetTalentProjection();TestTrue(TEXT("fixture really unlocked offline rewards"),Projection.bOfflineRewardsUnlocked);
            const FString Expected=Case==1?(English?TEXT("Chest cap: 8h\nGold cap: 24h\nXP cap: 24h"):TEXT("宝箱上限 8小时\n金币上限 24小时\n经验上限 24小时")):
                (English?TEXT("Chest cap: 8h 3m\nGold cap: 24h 28m 48s\nXP cap: 24h 57m 36s"):TEXT("宝箱上限 8时3分\n金币上限 24时28分48秒\n经验上限 24时57分36秒"));
            TestEqual(TEXT("separate actual caps, without discarded minutes or seconds"),Copy,Expected);
        }
        Widget->HandleDesktopActionForTest(53);Widget->TickForTest(0);
        TestNull(TEXT("cancel removes the confirmation"),Widget->WidgetTree->FindWidget(TEXT("ExitOfflineHint")));
    }
    return true;
}
#endif
