#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/GameInstance.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKLocalization.h"
#include "../UI/GameXXKChestReceipt.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKChestReceiptsTest,"GameXXK.Training.Chests.OneReportPerBox",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKChestReceiptsTest::RunTest(const FString&)
{
    const FString Language=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Language,false);};
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());if(!MVP->StartGame())return false;
    auto Base=MVP->GetRuntimeState();Base.Screen=EGameXXKScreen::Town;Base.Talents.MinimumBackpackCapacity=200;
    Base.Training.bTravelActive=false;Base.Training.ActiveTravelEncounterIndex=INDEX_NONE;Base.Training.OwnedChestTokens.Reset();
    Base.Training.ChallengeRewardSeed=20260910;FString Error;
    const auto Tier=EGameXXKTrainingRewardTier::NormalChest;
    for(int32 Index=0;Index<30;++Index)TestTrue(TEXT("append authored box"),FGameXXKTrainingRules::AppendChestToken(Base.Training,Tier,TEXT("Training.Normal.1-1"),5,&Error));
    auto BatchState=Base;FGameXXKTrainingChestOpenResult Batch;
    if(!TestTrue(TEXT("batch opens"),FGameXXKTrainingChestRules::OpenAll(BatchState,Tier,Batch)))return false;
    TestEqual(TEXT("30 boxes create 30 separate records"),Batch.Receipts.Num(),30);
    auto SingleState=Base;
    for(int32 Index=0;Index<Batch.Receipts.Num();++Index)
    {
        FGameXXKTrainingChestOpenResult Single;
        TestTrue(TEXT("individual box opens"),FGameXXKTrainingChestRules::OpenOne(SingleState,Tier,Single));
        TestEqual(TEXT("single opening has exactly one record"),Single.Receipts.Num(),1);
        if(Single.Receipts.Num()==1)TestTrue(TEXT("batch preserves the actual per-box opening order and content"),
            FGameXXKTrainingChestOpenReceipt::StaticStruct()->CompareScriptStruct(&Single.Receipts[0],&Batch.Receipts[Index],PPF_None));
    }
    for(const TCHAR* Culture:{TEXT("zh-Hans"),TEXT("en")})
    {
        GameXXKLocalization::SetLanguage(Culture,false);const auto Reports=GameXXKChestReceipt::BuildReports(Batch);
        TestEqual(TEXT("every box has its own displayed report"),Reports.Num(),Batch.OpenedCount);
        for(int32 Index=0;Index<Reports.Num();++Index)
        {
            const auto& Receipt=Batch.Receipts[Index];const FString Text=Reports[Index].ToString();
            TestTrue(TEXT("opening identity appears in full report"),Text.Contains(FString::FromInt(Receipt.OpenOrdinal)));
            if(!Receipt.EquipmentBaseId.IsNone())
            {
                const auto* Definition=FGameXXKEquipmentCatalog::FindDefinition(Receipt.EquipmentBaseId);
                TestTrue(TEXT("equipment has a concrete set-and-slot name, not only a count"),Definition&&Text.Contains(GameXXKLocalization::Localize(Definition->DisplayName).ToString()));
                TestTrue(TEXT("equipment item level is recorded"),Receipt.ItemLevel==5);
            }
            if(GameXXKLocalization::IsEnglish())for(TCHAR Character:Text)if(Character>=0x3400&&Character<=0x9fff){AddError(TEXT("Report is not translated: ")+Text);break;}
        }
    }
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    MVP->GetMutableRuntimeState()=Base;
    MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Widget->SetMVPSubsystem(MVP);Widget->ConstructForTest();Widget->OpenWorkbench();Widget->OpenBackpack();
    TestTrue(TEXT("real right-click action commits the batch"),Widget->HandleActionRightClicked(600));
    TestEqual(TEXT("message history contains one report per box"),Widget->GetChestReportsForTest().Num(),30);
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    for(const FText& Text:Widget->GetChestReportsForTest())
    {
        // ToString() returns a reference owned by FText. Keep an actual string copy
        // alive while iterating instead of dangling through a temporary FText.
        const FString Localized=GameXXKLocalization::Localize(Text).ToString();
        for(TCHAR Character:Localized)
            if(Character>=0x3400&&Character<=0x9fff){AddError(TEXT("Existing report did not switch language: ")+Localized);break;}
    }
    MVP->GetMutableRuntimeState()=Base;
    MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
    const int32 Before=Widget->GetChestReportsForTest().Num();
    TestFalse(TEXT("failed checkpoint does not publish more reports"),Widget->HandleActionRightClicked(600));
    TestEqual(TEXT("failed batch adds no phantom drop record"),Widget->GetChestReportsForTest().Num(),Before);
    TestEqual(TEXT("failed batch preserves boxes"),MVP->GetRuntimeState().Training.OwnedChestTokens.Num(),30);
    MVP->ResetSaveSlotWriteDelegateForTest();return true;
}
#endif
