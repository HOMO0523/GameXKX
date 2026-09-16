#include "Misc/AutomationTest.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKSaveGame.h"
#include "GameXXKTeachingChestRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKTalentRules.h"
#include "GameXXKGemRules.h"
#include "Kismet/GameplayStatics.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInterfaceHelpWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "../UI/GameXXKChestReceipt.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UGameXXKMVPSubsystem* ChestMvp()
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    return M->StartGame()?M:nullptr;
}
int32 FreeSlots(const FGameXXKRuntimeState& S)
{
    int32 N=0;
    for(int32 I=0;I<FGameXXKTalentRules::GetUnlockedBackpackCapacity(S);++I)
        N+=!FGameXXKDesktopInventoryRules::GetEntryAt(S,EGameXXKDesktopItemContainer::Backpack,I).IsValid();
    return N;
}
bool FixtureStage(FGameXXKRuntimeState& S,int32 Stage)
{
    auto& P=S.GuideProgress.TeachingChests;P.Stage=Stage;P.CompletedStages=Stage-1;P.bOpened=false;
    P.bEnhancementReviewPending=false;P.MaterialBoxesRemaining=0;P.MaterialBoxesOpened=0;P.CombineInputIds.Reset();
    S.Training.OwnedChestTokens.RemoveAll([](const auto& Token){return !Token.FixedDropId.IsNone();});
    FString Error;return FGameXXKTeachingChestRules::RestoreOrdinaryChestTokens(S,Error);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestNewGameTest,"GameXXK.TeachingChests.NewGameOnlyFirst",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestNewGameTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("new game"),M))return false;
    const auto& P=M->GetRuntimeState().GuideProgress.TeachingChests;
    TestTrue(TEXT("new profile owns teaching sequence"),P.bEnabled);
    TestEqual(TEXT("only stage one issued"),P.Stage,1);
    TestFalse(TEXT("first box is unopened"),P.bOpened);
    TestEqual(TEXT("no lessons claimed complete"),P.CompletedStages,0);
    TestEqual(TEXT("no combine child boxes issued"),P.MaterialBoxesRemaining,0);
    TestFalse(TEXT("weapon awaits its box"),P.ReservedWeapon.InstanceId.IsNone());
    TestTrue(TEXT("new profile already travels during unopened first box"),M->GetRuntimeState().Training.bTravelActive);
    TestEqual(TEXT("onboarding starts on the existing 1-1 Travel route"),M->GetRuntimeState().Training.CurrentTravelStageId,FName(TEXT("Training.Normal.1-1")));
    FString Error;TestTrue(TEXT("new profile is a valid save"),FGameXXKSaveMigration::ValidateRuntimeState(M->GetRuntimeState(),Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestClickTest,"GameXXK.TeachingChests.OpenAndResumeAreNotCompletion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestClickTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    int32 Opened=0;FString Error;
    if(!TestTrue(TEXT("open first box"),M->OpenTeachingChest(false,true,Opened,Error)))return false;
    TestEqual(TEXT("right click opens only current box"),Opened,1);
    const auto& P=M->GetRuntimeState().GuideProgress.TeachingChests;
    TestTrue(TEXT("box records opened"),P.bOpened);
    TestEqual(TEXT("opening does not grant stage two"),P.Stage,1);
    TestFalse(TEXT("repeated right click cannot consume future boxes"),M->OpenTeachingChest(false,true,Opened,Error));
    TestTrue(TEXT("dismiss works"),M->SetTeachingChestDismissed(true,Error));
    TestEqual(TEXT("closing never grants next box"),P.Stage,1);
    auto* Save=NewObject<UGameXXKSaveGame>();Save->SaveState=UGameXXKMVPRules::MakeSaveState(M->GetRuntimeState());
    TArray<uint8> Bytes;TestTrue(TEXT("serialize opened/dismissed"),UGameplayStatics::SaveGameToMemory(Save,Bytes));
    auto* Loaded=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if(!TestNotNull(TEXT("reload bytes"),Loaded))return false;
    FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("restore live state"),FGameXXKSaveMigration::TryRestoreRuntimeState(Loaded->SaveState,Restored,Report));
    TestTrue(TEXT("dismiss persisted"),Restored.GuideProgress.TeachingChests.bDismissed);
    TestTrue(TEXT("opening persisted"),Restored.GuideProgress.TeachingChests.bOpened);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestCapacityTest,"GameXXK.TeachingChests.CombineCapacityZeroOneThreeNine",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestCapacityTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto Base=M->GetRuntimeStateCopy();FString Error;int32 Opened=0;
    if(!TestTrue(TEXT("first payload exists"),FGameXXKTeachingChestRules::Open(Base,false,false,Opened,Error)))return false;
    TestTrue(TEXT("sixth-stage fixture owns its ordinary token"),FixtureStage(Base,6));
    for(int32 Space:{0,1,3,9})
    {
        auto S=Base;
        while(FreeSlots(S)>Space)
        {
            FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::PoJun;Request.Quality=EGameXXKEquipmentQuality::Common;
            Request.bForceSlot=true;Request.ForcedSlot=EGameXXKEquipmentSlot::Head;FName Id;
            if(!FGameXXKEquipmentRules::CreateRolledInstance(S.EquipmentCollection,Request,Id,&Error)
                ||!FGameXXKDesktopInventoryRules::Normalize(S,&Error)){AddError(Error);return false;}
        }
        const int32 Before=S.EquipmentCollection.EquipmentInstances.Num();
        const bool Parent=FGameXXKTeachingChestRules::Open(S,false,true,Opened,Error);
        TestEqual(TEXT("parent needs only one equipment cell"),Parent,Space>0);
        const auto& P=S.GuideProgress.TeachingChests;
        if(!Space){TestEqual(TEXT("no children on failed parent"),P.MaterialBoxesRemaining,0);continue;}
        TestEqual(TEXT("parent produces exactly one equipment"),S.EquipmentCollection.EquipmentInstances.Num(),Before+1);
        TestEqual(TEXT("all eight children owned immediately"),P.MaterialBoxesRemaining,8);
        FGameXXKTeachingChestRules::Open(S,true,true,Opened,Error);
        TestEqual(TEXT("batch stops at available capacity"),Opened,FMath::Min(8,Space-1));
        TestEqual(TEXT("unopened children retained"),P.MaterialBoxesRemaining,8-FMath::Min(8,Space-1));
        TestEqual(TEXT("no automatic combine"),P.bCombinePracticed,false);
        TestEqual(TEXT("same course despite child opens"),P.Stage,6);
        S.GuideProgress.TeachingChests.bDismissed=true;
        FGameXXKRuntimeState Reloaded;FGameXXKSaveMigrationReport Report;
        if(TestTrue(TEXT("partial children survive reload"),FGameXXKSaveMigration::TryRestoreRuntimeState(UGameXXKMVPRules::MakeSaveState(S),Reloaded,Report)))
        {
            TestEqual(TEXT("reload retains every unopened child"),Reloaded.GuideProgress.TeachingChests.MaterialBoxesRemaining,P.MaterialBoxesRemaining);
            TestTrue(TEXT("reload preserves closed lesson"),Reloaded.GuideProgress.TeachingChests.bDismissed);
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestSequenceTest,"GameXXK.TeachingChests.RealSixLessonSequence",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestSequenceTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto S=M->GetRuntimeStateCopy();FString Error;int32 Opened=0;
    const int32 Ordinal=S.EquipmentCollection.NextInstanceOrdinal;
    FGameXXKEquipmentTransactionResult Result;
    auto Ref=[&](FName Id,bool Equipment=true)
    {
        FGameXXKToolInputRef R;R.ExpectedEntry=Equipment?FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id):FGameXXKDesktopInventoryRules::MakeItemEntry(Id);
        R.Container=EGameXXKDesktopItemContainer::Backpack;R.SlotIndex=FGameXXKDesktopInventoryRules::FindEntrySlot(S,R.Container,R.ExpectedEntry);R.Quantity=1;return R;
    };
    for(int32 Stage=1;Stage<=6;++Stage)
    {
        if(!TestTrue(FString::Printf(TEXT("open stage %d: %s"),Stage,*Error),FGameXXKTeachingChestRules::Open(S,false,true,Opened,Error)))return false;
        TestEqual(TEXT("opening alone retains current lesson"),S.GuideProgress.TeachingChests.Stage,Stage);
        const auto P=S.GuideProgress.TeachingChests;
        bool Applied=false;
        if(Stage==1)Applied=FGameXXKEquipmentEconomyRules::Equip(S,TEXT("Player"),EGameXXKEquipmentSlot::Weapon,P.TargetId,Result);
        if(Stage==2){FGameXXKSocketGemRequest R;R.EquipmentInput=Ref(P.TargetId);R.GemInput=Ref(P.GemId,false);R.SocketIndex=0;Applied=FGameXXKEquipmentToolRules::SocketGem(S,R,Result);}
        if(Stage==3)Applied=FGameXXKEquipmentToolRules::Enhance(S,Ref(P.TargetId),Result);
        if(Stage==4)
        {
            if(!TestTrue(TEXT("generate reforge candidate"),FGameXXKEquipmentToolRules::BeginReforge(S,Ref(P.TargetId),0,Result)))return false;
            TestEqual(TEXT("preview alone is not reforge completion"),S.GuideProgress.TeachingChests.Stage,4);
            Applied=FGameXXKEquipmentToolRules::ResolveReforge(S,false,Result);
        }
        if(Stage==5)Applied=FGameXXKEquipmentToolRules::Dismantle(S,{Ref(P.TargetId)},true,Result);
        if(Stage==6)
        {
            TestTrue(TEXT("open eight supplied children"),FGameXXKTeachingChestRules::Open(S,true,true,Opened,Error));
            TestEqual(TEXT("all nine legal inputs delivered"),S.GuideProgress.TeachingChests.CombineInputIds.Num(),9);
            TestEqual(TEXT("teaching loot preserves normal item ordinal"),S.EquipmentCollection.NextInstanceOrdinal,Ordinal);
            TArray<FGameXXKToolInputRef> Inputs;for(FName Id:S.GuideProgress.TeachingChests.CombineInputIds)Inputs.Add(Ref(Id));
            Applied=FGameXXKEquipmentToolRules::CombineEquipment(S,Inputs,Result);
        }
        if(!TestTrue(FString::Printf(TEXT("actual lesson %d operation"),Stage),Applied))return false;
        if(Stage==3)
        {
            TestTrue(TEXT("enhancement pauses on saved attribute result"),S.GuideProgress.TeachingChests.bEnhancementReviewPending);
            TestEqual(TEXT("enhancement level displayed before and after"),S.GuideProgress.TeachingChests.EnhancementAfterLevel,S.GuideProgress.TeachingChests.BaselineEnhancement+1);
            TestTrue(TEXT("player acknowledges attribute review"),FGameXXKTeachingChestRules::CompleteEnhancementReview(S,Error));
        }
        TestEqual(TEXT("exactly one lesson completed"),S.GuideProgress.TeachingChests.CompletedStages,Stage);
        TestTrue(TEXT("teaching state remains valid"),FGameXXKTeachingChestRules::Validate(S,Error));
        FGameXXKRuntimeState Reloaded;FGameXXKSaveMigrationReport Report;
        if(!TestTrue(TEXT("each lesson survives reload"),FGameXXKSaveMigration::TryRestoreRuntimeState(UGameXXKMVPRules::MakeSaveState(S),Reloaded,Report)))return false;
        S=MoveTemp(Reloaded);
    }
    TestTrue(TEXT("actual combine recorded separately"),S.GuideProgress.TeachingChests.bCombinePracticed);
    TestFalse(TEXT("manual combine does not fake autofill click"),S.GuideProgress.TeachingChests.bAutoFillPracticed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestAutoFillTest,"GameXXK.TeachingChests.PlayerAutoFillDoesNotCombine",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestAutoFillTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto& S=M->GetMutableRuntimeState();FString Error;int32 Opened=0;
    TestTrue(TEXT("restore weapon"),M->OpenTeachingChest(false,false,Opened,Error));
    auto& P=S.GuideProgress.TeachingChests;TestTrue(TEXT("sixth-stage fixture token"),FixtureStage(S,6));
    TestTrue(TEXT("sixth parent"),M->OpenTeachingChest(false,false,Opened,Error));
    TestTrue(TEXT("children"),M->OpenTeachingChest(true,true,Opened,Error));
    const int32 Count=S.EquipmentCollection.EquipmentInstances.Num();const int32 Gold=S.PlayerGold;
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->HandleActionClicked(3);Host->SetToolModeForTest(EGameXXKDesktopToolMode::Combine);
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    if(!TestNotNull(TEXT("teaching surface"),Help))return false;
    Host->TickForTest(0);Help->NativeTick(FGeometry(),.2f);
    auto* Button=Cast<UButton>(Help->GetCurrentTargetForTest());
    if(!TestNotNull(TEXT("guide points to real button"),Button))return false;
    TestEqual(TEXT("target is auto fill"),Button->GetFName(),FName(TEXT("ToolAutoFill")));
    TestEqual(TEXT("guide has not filled for player"),Host->GetOccupiedToolSlotCountForTest(),0);
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
    Button->OnClicked.Broadcast();Help->NativeTick(FGeometry(),.2f);
    TestFalse(TEXT("failed checkpoint cannot claim auto fill completion"),P.bAutoFillPracticed);
    TestEqual(TEXT("failed checkpoint retains selected preview"),Host->GetOccupiedToolSlotCountForTest(),9);
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    Button->OnClicked.Broadcast();Help->NativeTick(FGeometry(),.2f);
    TestEqual(TEXT("player click fills nine slots"),Host->GetOccupiedToolSlotCountForTest(),9);
    TestTrue(TEXT("actual auto fill checkpoint"),P.bAutoFillPracticed);
    TestFalse(TEXT("no automatic combine"),P.bCombinePracticed);
    TestEqual(TEXT("all inputs remain owned"),S.EquipmentCollection.EquipmentInstances.Num(),Count);
    TestEqual(TEXT("no gold consumed by preview"),S.PlayerGold,Gold);
    TestTrue(TEXT("close after preview"),M->SetTeachingChestDismissed(true,Error));
    TestFalse(TEXT("closing does not combine"),P.bCombinePracticed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestPersistenceTest,"GameXXK.TeachingChests.SaveFailureAndLegacy",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestPersistenceTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
    const int32 Count=M->GetRuntimeState().EquipmentCollection.EquipmentInstances.Num();FString Error;int32 Opened=0;
    TestFalse(TEXT("failed save rejects opening"),M->OpenTeachingChest(false,true,Opened,Error));
    TestEqual(TEXT("no credited opening"),Opened,0);
    TestFalse(TEXT("box retained"),M->GetRuntimeState().GuideProgress.TeachingChests.bOpened);
    TestEqual(TEXT("no leaked reward"),M->GetRuntimeState().EquipmentCollection.EquipmentInstances.Num(),Count);
    TestFalse(TEXT("failed close checkpoint rejected"),M->SetTeachingChestDismissed(true,Error));
    TestFalse(TEXT("dismiss flag rolled back"),M->GetRuntimeState().GuideProgress.TeachingChests.bDismissed);
    auto Legacy=M->GetRuntimeStateCopy();GameXXKPermanentPartyTestFixtures::SkipTeachingChests(Legacy);
    auto Save=UGameXXKMVPRules::MakeSaveState(Legacy);Save.SaveVersion=43;
    FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    if(!TestTrue(TEXT("legacy 43 migrates"),FGameXXKSaveMigration::TryRestoreRuntimeState(Save,Restored,Report)))return false;
    TestFalse(TEXT("old profiles do not receive teaching rewards"),Restored.GuideProgress.TeachingChests.bEnabled);
    TestEqual(TEXT("existing weapon preserved by migration"),Restored.EquipmentCollection.EquipmentInstances.Num(),Count+1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestGuidedSequenceTest,"GameXXK.TeachingChests.RealGuidedUISequence",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestGuidedSequenceTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    auto& S=M->GetMutableRuntimeState();FName LastStep;int32 Repeats=0;
    for(int32 Attempt=0;Attempt<100&&!S.GuideProgress.TeachingChests.bAutoFillPracticed;++Attempt)
    {
        if(!TestTrue(TEXT("every guide step leaves travel running"),S.Training.bTravelActive))return false;
        TestEqual(TEXT("guides do not change the 1-1 travel route"),S.Training.CurrentTravelStageId,FName(TEXT("Training.Normal.1-1")));
        Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
        auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
        if(!Help||!Help->IsOpen())
        {
            if(!S.GuideProgress.TeachingChests.bOpened)
                if(auto* Chest=Cast<UButton>(Host->WidgetTree->FindWidget(TEXT("TrainingNormalChestButton"))))Chest->OnClicked.Broadcast();
            continue;
        }
        Help->NativeTick(FGeometry(),.2f);Host->TickForTest(0);
        if(!Help->IsOpen())continue;
        Help->NativeTick(FGeometry(),.2f);
        const auto Step=Help->GetCurrentCompletionIdForTest();
        Repeats=Step==LastStep?Repeats+1:0;LastStep=Step;
        if(Repeats>10){AddError(FString(TEXT("guide stuck: "))+Step.ToString());return false;}
        TestTrue(TEXT("at most two actions per guide"),Help->GetStepCountForTest()<=2);
        TArray<FString> Parts;Step.ToString().ParseIntoArray(Parts,TEXT("."));if(Parts.Num()!=4)continue;
        const FString Part=Parts[2];const int32 Index=FCString::Atoi(*Parts[3]);
        if((Part==TEXT("Review")&&Parts[1]==TEXT("3"))||(Part==TEXT("Intro")&&Parts[1]==TEXT("5")))
        {TestNotNull(TEXT("hint points at the existing item or tool"),Help->GetCurrentTargetForTest());Help->CloseForTest();continue;}
        auto* Target=Help->GetCurrentTargetForTest();
        if(!TestNotNull(Step.ToString()+TEXT(" actual target exists"),Target))return false;
        if((Part==TEXT("Equip")||Part==TEXT("Target")||Part==TEXT("Gem"))&&Index==0)
        {
            const auto& P=S.GuideProgress.TeachingChests;
            const auto Key=Part==TEXT("Gem")?FGameXXKDesktopInventoryRules::MakeItemEntry(P.GemId):FGameXXKDesktopInventoryRules::MakeEquipmentEntry(P.TargetId);
            Host->PickUpBackpackSlotForTest(FGameXXKDesktopInventoryRules::FindEntrySlot(S,EGameXXKDesktopItemContainer::Backpack,Key));
        }
        else if(Part==TEXT("Resolve"))Host->HandleActionClicked(316);
        else if(auto* Button=Cast<UButton>(Target))Button->OnClicked.Broadcast();
        else {AddError(Step.ToString()+TEXT(" target is not an action"));return false;}
        Help->NativeTick(FGeometry(),.2f);
    }
    TestTrue(TEXT("all six UI lessons reach player-filled preview"),S.GuideProgress.TeachingChests.bAutoFillPracticed);
    TestEqual(TEXT("nine preview inputs survive guide checkpoints"),Host->GetOccupiedToolSlotCountForTest(),9);
    TestFalse(TEXT("guide never combines"),S.GuideProgress.TeachingChests.bCombinePracticed);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestEquipRecoveryTest,"GameXXK.TeachingChests.ResumeEquipmentFromWarehouseWithToolsOpen",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestEquipRecoveryTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    FString Error;int32 Opened=0;TestTrue(TEXT("open first"),M->OpenTeachingChest(false,false,Opened,Error));
    const auto Key=FGameXXKDesktopInventoryRules::MakeEquipmentEntry(M->GetRuntimeState().GuideProgress.TeachingChests.WeaponId);
    const int32 Source=FGameXXKDesktopInventoryRules::FindEntrySlot(M->GetRuntimeState(),EGameXXKDesktopItemContainer::Backpack,Key);
    if(!TestTrue(TEXT("player stores weapon"),M->MoveDesktopInventoryEntry(EGameXXKDesktopItemContainer::Backpack,Source,EGameXXKDesktopItemContainer::Warehouse,0,&Error)))return false;
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();Host->HandleActionClicked(3);
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    if(!TestNotNull(TEXT("equipment guidance"),Help))return false;
    for(int32 I=0;I<2;++I)
    {
        Host->TickForTest(0);Help->NativeTick(FGeometry(),.2f);Host->TickForTest(0);Help->NativeTick(FGeometry(),.2f);
        auto* Target=Cast<UButton>(Help->GetCurrentTargetForTest());
        if(!TestNotNull(TEXT("actual pickup/equipment target"),Target))return false;
        Target->OnClicked.Broadcast();Help->NativeTick(FGeometry(),.2f);
    }
    TestEqual(TEXT("weapon equipped despite tools and warehouse being open"),M->GetRuntimeState().GuideProgress.TeachingChests.Stage,2);
    TestTrue(TEXT("first lesson starts travel"),M->GetRuntimeState().Training.bTravelActive);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingOrdinaryEntranceTest,"GameXXK.TeachingChests.OrdinaryChestEntranceAndFixedFirstDrop",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingOrdinaryEntranceTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    TestEqual(TEXT("first lesson is one ordinary chest"),M->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest),1);
    const FName Weapon=M->GetRuntimeState().GuideProgress.TeachingChests.WeaponId;
    FGameXXKTrainingChestOpenResult Result;
    TestTrue(TEXT("normal chest action delivers lesson supplies"),M->OpenAllTrainingChests(EGameXXKTrainingRewardTier::NormalChest,Result));
    TestEqual(TEXT("only the originally owned chest opens"),Result.OpenedCount,1);
    TestTrue(TEXT("fixed starter weapon through normal loot result"),Result.EquipmentInstanceIds.Contains(Weapon));
    if(!Result.Receipts.IsEmpty())
    {
        const auto* D=FGameXXKEquipmentCatalog::FindDefinition(Result.Receipts[0].EquipmentBaseId);
        TestTrue(TEXT("normal receipt names the actual item"),D&&GameXXKChestReceipt::BuildRecord(Result.Receipts[0]).ToString().Contains(GameXXKLocalization::Localize(D->DisplayName).ToString()));
    }
    TestTrue(TEXT("normal opening triggers fixed lesson"),M->GetRuntimeState().GuideProgress.TeachingChests.bOpened);
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    TestNull(TEXT("no separate teaching chest control"),Host->WidgetTree->FindWidget(TEXT("TeachingChestButton")));
    TestNull(TEXT("no separate material chest control"),Host->WidgetTree->FindWidget(TEXT("TeachingMaterialButton")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingEnhancementReviewTest,"GameXXK.TeachingChests.EnhancementWaitsForAttributeReview",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingEnhancementReviewTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto S=M->GetRuntimeStateCopy();FString Error;int32 Opened=0;
    TestTrue(TEXT("restore starter payload"),FGameXXKTeachingChestRules::Open(S,false,false,Opened,Error));
    auto& P=S.GuideProgress.TeachingChests;TestTrue(TEXT("third-stage fixture token"),FixtureStage(S,3));
    TestTrue(TEXT("enhancement supplies"),FGameXXKTeachingChestRules::Open(S,false,false,Opened,Error));
    FGameXXKToolInputRef Ref;Ref.ExpectedEntry=FGameXXKDesktopInventoryRules::MakeEquipmentEntry(P.TargetId);
    Ref.Container=EGameXXKDesktopItemContainer::Backpack;Ref.SlotIndex=FGameXXKDesktopInventoryRules::FindEntrySlot(S,Ref.Container,Ref.ExpectedEntry);Ref.Quantity=1;
    FGameXXKEquipmentTransactionResult Result;
    TestTrue(TEXT("enhancement really commits"),FGameXXKEquipmentToolRules::Enhance(S,Ref,Result));
    TestEqual(TEXT("keep focus on enhancement until attributes reviewed"),P.Stage,3);
    TestEqual(TEXT("next chest waits for review"),P.CompletedStages,2);
    TestTrue(TEXT("attribute review is pending"),P.bEnhancementReviewPending);
    TestTrue(TEXT("actual equipment attributes were captured"),P.EnhancementAfterStats.Attack>0);
    TestEqual(TEXT("no next ordinary chest before review"),FGameXXKTrainingRules::CountChestTokens(S.Training,EGameXXKTrainingRewardTier::NormalChest),0);
    P.bDismissed=true;FGameXXKRuntimeState Reloaded;FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("closed attribute review persists"),FGameXXKSaveMigration::TryRestoreRuntimeState(UGameXXKMVPRules::MakeSaveState(S),Reloaded,Report));
    TestTrue(TEXT("review survives reload"),Reloaded.GuideProgress.TeachingChests.bEnhancementReviewPending);
    M->GetMutableRuntimeState()=Reloaded;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    M->SetTeachingChestDismissed(false,Error);
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    if(!TestNotNull(TEXT("automatic hover hint"),Help))return false;
    Help->NativeTick(FGeometry(),.2f);
    TestEqual(TEXT("pending enhancement opens attribute review"),Help->GetCurrentCompletionIdForTest(),FName(TEXT("Teaching.3.Review.0")));
    TestEqual(TEXT("hover uses the actual equipment widget"),Help->GetCurrentTargetForTest(),Host->ResolveTeachingChestTarget(TEXT("TeachingTarget.EnhancedItem")));
    Help->CloseForTest();
    TestEqual(TEXT("closing the equipment hint gives next chest"),M->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest),1);
    TestEqual(TEXT("review ends enhancement lesson"),M->GetRuntimeState().GuideProgress.TeachingChests.Stage,4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingOrdinaryBatchTest,"GameXXK.TeachingChests.OrdinaryBatchFreezesOwnedBoxesAndMigrates44",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingOrdinaryBatchTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto& S=M->GetMutableRuntimeState();FString Error;FName Id;
    FGameXXKEquipmentCreateRequest Request;Request.Set=EGameXXKEquipmentSet::PoJun;Request.Quality=EGameXXKEquipmentQuality::Common;
    Request.bForceSlot=true;Request.ForcedSlot=EGameXXKEquipmentSlot::Weapon;
    TestTrue(TEXT("independent weapon acquired"),FGameXXKEquipmentRules::CreateRolledInstance(S.EquipmentCollection,Request,Id,&Error));
    FGameXXKDesktopInventoryRules::Normalize(S,&Error);FGameXXKEquipmentTransactionResult Equip;
    TestTrue(TEXT("natural equipment practice before first fixed box"),M->EquipEquipmentInstance(TEXT("Player"),EGameXXKEquipmentSlot::Weapon,Id,Equip));
    FGameXXKTrainingChestOpenResult Result;
    TestTrue(TEXT("batch opens original ordinary box"),M->OpenAllTrainingChests(EGameXXKTrainingRewardTier::NormalChest,Result));
    TestEqual(TEXT("newly issued lesson box excluded from same batch"),Result.OpenedCount,1);
    TestEqual(TEXT("next fixed ordinary box is retained"),M->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest),1);
    TestEqual(TEXT("natural practice advances exactly one lesson"),S.GuideProgress.TeachingChests.Stage,2);
    TestTrue(TEXT("sixth fixture"),FixtureStage(S,6));
    TestTrue(TEXT("sixth parent opens through normal batch"),M->OpenAllTrainingChests(EGameXXKTrainingRewardTier::NormalChest,Result));
    TestEqual(TEXT("new eight children not consumed in parent click"),Result.OpenedCount,1);
    TestEqual(TEXT("eight ordinary boxes shown"),M->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest),8);
    auto Legacy=UGameXXKMVPRules::MakeSaveState(S);Legacy.SaveVersion=44;
    Legacy.RuntimeState.Training.OwnedChestTokens.RemoveAll([](const auto& Token){return !Token.FixedDropId.IsNone();});
    FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("v44 pending supplies become ordinary tokens"),FGameXXKSaveMigration::TryRestoreRuntimeState(Legacy,Restored,Report));
    TestEqual(TEXT("migration restores exactly eight owned boxes"),FGameXXKTrainingRules::CountChestTokens(Restored.Training,EGameXXKTrainingRewardTier::NormalChest),8);
    TestEqual(TEXT("migration never repeats equipment drops"),Restored.EquipmentCollection.EquipmentInstances.Num(),S.EquipmentCollection.EquipmentInstances.Num());
    S=MoveTemp(Restored);
    TestTrue(TEXT("separate explicit batch opens eight"),M->OpenAllTrainingChests(EGameXXKTrainingRewardTier::NormalChest,Result));
    TestEqual(TEXT("eight child opens"),Result.OpenedCount,8);
    TestEqual(TEXT("total nine supplied combine inputs"),S.GuideProgress.TeachingChests.CombineInputIds.Num(),9);
    TestFalse(TEXT("batch never combines"),S.GuideProgress.TeachingChests.bCombinePracticed);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingDismantleOptionalTest,"GameXXK.TeachingChests.DismantleIntroductionDoesNotConsumeEquipment",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingDismantleOptionalTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    int32 Opened=0;FString Error;M->OpenTeachingChest(false,false,Opened,Error);auto& S=M->GetMutableRuntimeState();
    TestTrue(TEXT("fifth ordinary box fixture"),FixtureStage(S,5));FGameXXKTrainingChestOpenResult Result;
    TestTrue(TEXT("open ordinary chest"),M->OpenOneTrainingChest(EGameXXKTrainingRewardTier::NormalChest,Result));
    const FName Item=S.GuideProgress.TeachingChests.TargetId;const int32 Count=S.EquipmentCollection.EquipmentInstances.Num();
    const auto Items=S.Inventory;const int64 Experience=S.ToolProgress.Experience;
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->HandleActionClicked(3);Host->SetToolModeForTest(EGameXXKDesktopToolMode::Dismantle);
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    if(!TestNotNull(TEXT("dismantle explanation"),Help))return false;Help->NativeTick(FGeometry(),.2f);
    TestEqual(TEXT("introduction instead of forced dismantle"),Help->GetCurrentCompletionIdForTest(),FName(TEXT("Teaching.5.Intro.0")));
    TestEqual(TEXT("does not put any item into dismantle"),Host->GetOccupiedToolSlotCountForTest(),0);
    Help->CloseForTest();
    TestEqual(TEXT("closing explanation gives sixth ordinary box"),M->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest),1);
    TestEqual(TEXT("no equipment consumed"),S.EquipmentCollection.EquipmentInstances.Num(),Count);
    TestNotNull(TEXT("the opened equipment remains owned"),FGameXXKEquipmentRules::FindInstance(S.EquipmentCollection,Item));
    TestTrue(TEXT("no dismantle material fabricated"),S.Inventory.OrderIndependentCompareEqual(Items));
    TestEqual(TEXT("no tool experience fabricated"),S.ToolProgress.Experience,Experience);
    TestFalse(TEXT("knowing the feature is not recorded as actual dismantling"),S.GuideProgress.CompletedGuideStepIds.Contains(TEXT("TeachingChest.Practice.4")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingChestNaturalPromptTest,"GameXXK.TeachingChests.NextBoxWaitsForPlayerOpening",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingChestNaturalPromptTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    int32 Opened=0;FString Error;M->OpenTeachingChest(false,false,Opened,Error);
    FGameXXKEquipmentTransactionResult Result;
    TestTrue(TEXT("equip first weapon"),M->EquipEquipmentInstance(TEXT("Player"),EGameXXKEquipmentSlot::Weapon,M->GetRuntimeState().GuideProgress.TeachingChests.WeaponId,Result));
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    TestFalse(TEXT("new reward does not automatically start the next prompt"),Help&&Help->IsOpen());
    auto* Chest=Cast<UButton>(Host->WidgetTree->FindWidget(TEXT("TrainingNormalChestButton")));
    if(!TestNotNull(TEXT("ordinary chest entrance"),Chest))return false;
    Chest->OnClicked.Broadcast();Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    TestTrue(TEXT("player opening starts the relevant prompt"),Help&&Help->IsOpen());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingTravelContinuousTest,"GameXXK.TeachingChests.TravelAdvancesBeforeDuringAndAfterGuide",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingTravelContinuousTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->OfferTeachingChestGuide(true);Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
    TestTrue(TEXT("actual runner advances while first box hint is open"),Host->AdvanceTravelForTest(1));
    FGameXXKTrainingChestOpenResult Result;TestTrue(TEXT("ordinary entrance opens fixed first box"),M->OpenOneTrainingChest(EGameXXKTrainingRewardTier::NormalChest,Result));
    TestTrue(TEXT("opening does not pause the runner"),Host->AdvanceTravelForTest(1));
    FString Error;M->SetTeachingChestDismissed(true,Error);
    TestTrue(TEXT("closing keeps travel active"),M->GetRuntimeState().Training.bTravelActive);
    auto Save=UGameXXKMVPRules::MakeSaveState(M->GetRuntimeState());FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("active travel and unfinished lesson survive reload"),FGameXXKSaveMigration::TryRestoreRuntimeState(Save,Restored,Report));
    TestTrue(TEXT("reload retains running travel"),Restored.Training.bTravelActive);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTeachingPriorityOverOlderTest,"GameXXK.TeachingChests.FixedDropPrecedesOlderOrdinaryBoxes",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTeachingPriorityOverOlderTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    auto S=M->GetRuntimeStateCopy();FString Error;int32 Opened=0;
    TestTrue(TEXT("open initial fixed box"),FGameXXKTeachingChestRules::Open(S,false,false,Opened,Error));
    TestTrue(TEXT("ordinary box earned before next lesson box"),FGameXXKTrainingRules::AppendChestToken(S.Training,EGameXXKTrainingRewardTier::NormalChest,TEXT("Training.Normal.1-1"),1,&Error));
    TestTrue(TEXT("later fixed box issued"),FixtureStage(S,6));
    TestTrue(TEXT("fixture really has ordinary box first in ledger"),S.Training.OwnedChestTokens[0].FixedDropId.IsNone());
    for(bool Batch:{false,true})
    {
        auto Candidate=S;FGameXXKTrainingChestOpenResult Result;
        const bool Success=Batch?FGameXXKTrainingChestRules::OpenAll(Candidate,EGameXXKTrainingRewardTier::NormalChest,Result):FGameXXKTrainingChestRules::OpenOne(Candidate,EGameXXKTrainingRewardTier::NormalChest,Result);
        if(!TestTrue(TEXT("opening succeeds"),Success)||!TestTrue(TEXT("receipt exists"),!Result.Receipts.IsEmpty()))return false;
        TestEqual(TEXT("fixed drop always opens first"),Result.Receipts[0].FixedDropId,FGameXXKTeachingChestRules::StageName(6));
        TestEqual(TEXT("new eight boxes excluded from this click"),Candidate.GuideProgress.TeachingChests.MaterialBoxesRemaining,8);
        TestEqual(TEXT("batch only consumes originally owned boxes"),Result.OpenedCount,Batch?2:1);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStarterGearDoubledTravelTest,"GameXXK.TeachingChests.StarterGearDoubledAndSolo11Clears",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStarterGearDoubledTravelTest::RunTest(const FString&)
{
    auto* Naked=ChestMvp();if(!TestNotNull(TEXT("unarmed comparison"),Naked))return false;
    bool NakedCleared=false,NakedDefeated=false;int32 NormalWins=0;EGameXXKTrainingEncounterKind FailedKind=EGameXXKTrainingEncounterKind::Normal;int32 FailedWave=0;
    for(int32 Step=0;Step<512&&!NakedCleared&&!NakedDefeated;++Step)
    {
        const auto Before=Naked->GetTrainingTravelRuntimeCopy();bool Won=false;FGameXXKTrainingReward Reward;
        if(!TestTrue(TEXT("unarmed actual combat step"),Naked->AdvanceTrainingTravelStep(Won,NakedCleared,NakedDefeated,Reward)))return false;
        if(Won&&Before.EncounterKind==EGameXXKTrainingEncounterKind::Normal)++NormalWins;
        if(Won||NakedDefeated)AddInfo(FString::Printf(TEXT("UNARMED wave=%d kind=%d won=%d defeated=%d heroMaxHP=%d attack=%d"),Before.EncounterIndex+1,static_cast<int32>(Before.EncounterKind),Won,NakedDefeated,Before.PartyUnits[0].MaxHP,Before.PartyUnits[0].Attack));
        if(NakedDefeated){FailedKind=Before.EncounterKind;FailedWave=Before.EncounterIndex+1;}
    }
    TestEqual(TEXT("unarmed hero clears all four normal waves"),NormalWins,4);
    TestTrue(TEXT("unarmed hero hits the equipment gate"),NakedDefeated);
    TestEqual(TEXT("unarmed defeat is at an elite, not a normal wave"),FailedKind,EGameXXKTrainingEncounterKind::Elite);
    AddInfo(FString::Printf(TEXT("UNARMED RESULT: normalWins=%d defeatWave=%d"),NormalWins,FailedWave));
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    FGameXXKTrainingChestOpenResult Loot;TestTrue(TEXT("open weapon through ordinary chest"),M->OpenOneTrainingChest(EGameXXKTrainingRewardTier::NormalChest,Loot));
    TArray<FName> Ids;for(const auto& Item:M->GetRuntimeState().EquipmentCollection.EquipmentInstances)
        if(const auto* D=FGameXXKEquipmentCatalog::FindDefinition(Item.BaseEquipmentId);D&&D->Set==EGameXXKEquipmentSet::Starter)Ids.Add(Item.InstanceId);
    TestEqual(TEXT("six starting equipment pieces"),Ids.Num(),6);
    FGameXXKCharacterStats Bare;Bare.MaxHealth=1;Bare.MaxMana=1;
    for(FName Id:Ids)
    {
        const auto* Item=FGameXXKEquipmentRules::FindInstance(M->GetRuntimeState().EquipmentCollection,Id);
        const auto* D=FGameXXKEquipmentCatalog::FindDefinition(Item->BaseEquipmentId);
        const auto Base=D->BaseStatCoefficients.Resolve(Item->ItemLevel);FGameXXKEquipmentTooltipSnapshot View;
        TestTrue(TEXT("real equipment projection"),FGameXXKEquipmentRules::BuildTooltipSnapshot(M->GetRuntimeState().EquipmentCollection,Id,TEXT("Player"),Bare,View));
        TestEqual(TEXT("starter health doubled"),View.ItemBaseStats.MaxHealth,Base.MaxHealth*2);
        TestEqual(TEXT("starter attack doubled"),View.ItemBaseStats.Attack,Base.Attack*2);
        TestEqual(TEXT("starter defense doubled"),View.ItemBaseStats.Defense,Base.Defense*2);
        TestEqual(TEXT("starter speed doubled"),View.ItemBaseStats.Speed,Base.Speed*2);
        FGameXXKEquipmentTransactionResult Result;if(!TestTrue(TEXT("player equips this starter piece"),M->EquipEquipmentInstance(TEXT("Player"),D->Slot,Id,Result)))return false;
    }
    TestTrue(TEXT("retry 1-1 with actual equipped stats"),M->StartTrainingTravel(TEXT("Training.Normal.1-1")));
    TestEqual(TEXT("still hero only"),M->GetTrainingTravelRuntimeCopy().PartyUnits.Num(),1);
    bool Cleared=false,Defeated=false;int32 Encounters=0;
    for(int32 Step=0;Step<512&&!Cleared&&!Defeated;++Step)
    {
        const auto Before=M->GetTrainingTravelRuntimeCopy();
        bool Encounter=false;FGameXXKTrainingReward Reward;
        if(!TestTrue(TEXT("actual 1-1 combat step"),M->AdvanceTrainingTravelStep(Encounter,Cleared,Defeated,Reward)))return false;
        Encounters+=Encounter?1:0;
        if(Encounter||Defeated)AddInfo(FString::Printf(TEXT("EQUIPPED wave=%d kind=%d won=%d defeated=%d heroMaxHP=%d attack=%d"),Before.EncounterIndex+1,static_cast<int32>(Before.EncounterKind),Encounter,Defeated,Before.PartyUnits[0].MaxHP,Before.PartyUnits[0].Attack));
    }
    TestFalse(TEXT("fully equipped solo hero survives"),Defeated);
    TestTrue(TEXT("fully equipped hero clears actual 1-1 loop"),Cleared);
    TestEqual(TEXT("all seven encounters completed"),Encounters,7);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstDefeatGearGuideTest,"GameXXK.TeachingChests.FirstDefeatTeachesAllStarterSlotsThenRetry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstDefeatGearGuideTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    for(int32 Step=0;Step<80&&M->GetRuntimeState().Training.TravelFailures==0;++Step)
        if(!Host->AdvanceTravelForTest(1))break;
    if(!TestTrue(TEXT("real unequipped defeat triggers recovery"),M->GetRuntimeState().Training.TravelFailures>0))return false;
    TestFalse(TEXT("first defeat waits for equipment instead of repeated naked retries"),M->GetRuntimeState().Training.bTravelActive);
    FName LastStep;int32 Repeats=0;
    for(int32 Step=0;Step<60&&!M->GetRuntimeState().GuideProgress.CompletedGuideStepIds.Contains(TEXT("Teaching.0.Retry.0"));++Step)
    {
        Host->TickForTest(0);Host->OfferTeachingChestGuide(true);
        auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
        if(!Help||!Help->IsOpen())continue;
        Help->NativeTick(FGeometry(),.2f);Host->TickForTest(0);Help->NativeTick(FGeometry(),.2f);
        if(!Help->IsOpen())continue;
        const FName Id=Help->GetCurrentCompletionIdForTest();Repeats=Id==LastStep?Repeats+1:0;LastStep=Id;
        if(Repeats>4){AddError(TEXT("Recovery stuck at ")+Id.ToString());return false;}
        TestTrue(TEXT("defeat guidance has priority over box lessons"),Id.ToString().StartsWith(TEXT("Teaching.0.")));
        TestTrue(TEXT("one or two operations per step"),Help->GetStepCountForTest()<=2);
        auto* Button=Cast<UButton>(Help->GetCurrentTargetForTest());
        if(!TestNotNull(Id.ToString()+TEXT(" actual target"),Button))return false;
        Button->OnClicked.Broadcast();Help->NativeTick(FGeometry(),.2f);
    }
    const auto& S=M->GetRuntimeState();const auto* Loadout=S.EquipmentCollection.CharacterLoadouts.Find(TEXT("Player"));
    if(!TestNotNull(TEXT("hero loadout"),Loadout))return false;
    for(int32 I=1;I<=6;++I)TestFalse(TEXT("every basic slot is equipped"),FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*Loadout,static_cast<EGameXXKEquipmentSlot>(I)).IsNone());
    TestTrue(TEXT("player resumed 1-1 after equipping"),S.GuideProgress.CompletedGuideStepIds.Contains(TEXT("Teaching.0.Retry.0")));
    TestTrue(TEXT("travel running again"),S.Training.bTravelActive);
    TestEqual(TEXT("keeps hero-only formation"),S.CardRun.OrderedFormation.Members.Num(),1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStarterOldSaveMigrationTest,"GameXXK.TeachingChests.StarterPowerMigratesOldEquippedSave",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStarterOldSaveMigrationTest::RunTest(const FString&)
{
    auto* M=ChestMvp();if(!TestNotNull(TEXT("fixture"),M))return false;
    int32 Opened=0;FString Error;M->OpenTeachingChest(false,false,Opened,Error);
    auto& S=M->GetMutableRuntimeState();const FName Id=S.GuideProgress.TeachingChests.WeaponId;
    auto* Item=S.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& E){return E.InstanceId==Id;});
    if(!TestNotNull(TEXT("starter weapon"),Item))return false;
    FGameXXKEquipmentTransactionResult Result;
    if(!TestTrue(TEXT("equip weapon"),M->EquipEquipmentInstance(TEXT("Player"),EGameXXKEquipmentSlot::Weapon,Id,Result)))return false;
    const int32 CurrentAttack=S.PlayerAttack;
    auto Old=UGameXXKMVPRules::MakeSaveState(S);Old.SaveVersion=45;Old.RuntimeState.PlayerAttack-=2;
    const int32 HP=Old.RuntimeState.PlayerHP;const int32 Gold=Old.RuntimeState.PlayerGold;
    FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("v45 equipped starter remains loadable"),FGameXXKSaveMigration::TryRestoreRuntimeState(Old,Restored,Report));
    TestEqual(TEXT("old cache upgraded to new starter power"),Restored.PlayerAttack,CurrentAttack);
    TestEqual(TEXT("migration preserves current HP"),Restored.PlayerHP,HP);
    TestEqual(TEXT("migration does not pay or charge gold"),Restored.PlayerGold,Gold);
    TestEqual(TEXT("migration does not duplicate items"),Restored.EquipmentCollection.EquipmentInstances.Num(),S.EquipmentCollection.EquipmentInstances.Num());
    Old.SaveVersion=FGameXXKSaveMigration::CurrentSaveVersion;
    TestFalse(TEXT("current version still rejects stale attribute caches"),FGameXXKSaveMigration::TryRestoreRuntimeState(Old,Restored,Report));
    return true;
}
#endif
