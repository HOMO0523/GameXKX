#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveStorage.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Narrative/GameXXKMainStorySubsystem.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryActualPlaythrough,
    "GameXXK.MainStory.ActualCombatAndDiskFullPlaythrough",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryActualPlaythrough::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGameXXKMVPSubsystem> MVP(NewObject<UGameXXKMVPSubsystem>(Instance.Get()));
    TStrongObjectPtr<UGameXXKMainStorySubsystem> Story(NewObject<UGameXXKMainStorySubsystem>(Instance.Get()));
    Story->SetMVPForTest(MVP.Get());
    const FString Slot=TEXT("GameXXK_Automation_ActualStory_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
    Report->SetBoolField(TEXT("complete"),false);
    Report->SetStringField(TEXT("conditions"),TEXT("Independent flow-test save: level 50 party, first six ordinary stages available. Actual card combat; no injected victory. Not a balance/grinding-duration test."));
    const FString Folder=FPaths::Combine(FPaths::ProjectDir(),TEXT("Saved/StorySystem/FullPlaythrough"));
    IFileManager::Get().MakeDirectory(*Folder,true);
    ON_SCOPE_EXIT
    {
        Report->SetArrayField(TEXT("nodes"),Rows);FString Json;
        FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Folder,TEXT("report.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UGameplayStatics::DeleteGameInSlot(Slot,0);
        for(int32 I=1;I<=FGameXXKSaveStorage::BackupCount;++I)
            UGameplayStatics::DeleteGameInSlot(Slot+FString::Printf(TEXT(".Previous%d"),I),0);
    };
    FString LastError;
    auto Require=[&](bool OK,const FString& Description)
    {
        if(!OK){Report->SetStringField(TEXT("failure"),Description+TEXT(" / ")+LastError+TEXT(" / ")+Story->Feedback().ToString());AddError(Description+TEXT(" / ")+LastError);}
        return OK;
    };
    if(!Require(MVP->StartGame(),TEXT("new test game")))return false;
    auto& Setup=MVP->GetMutableRuntimeState();
    while(Setup.PlayerLevel<50)UGameXXKMVPRules::ApplyPlayerExperience(Setup,UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(Setup.PlayerLevel));
    for(auto& Companion:Setup.CardRun.CompanionRoster.PermanentCompanions)
        while(Companion.Level<50)if(!Require(FGameXXKCompanionRules::AwardExperience(Companion,FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(Companion.Level),&LastError),TEXT("party setup")))return false;
    if(!Require(FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Setup,&LastError)
        &&FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Setup),TEXT("setup mirrors")))return false;
    for(int32 I=1;I<=6;++I)Setup.Training.ClearedStageIds.Add(FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,I));
    if(!Require(FGameXXKSaveMigration::ValidateRuntimeState(Setup,LastError),TEXT("valid test preconditions")))return false;
    MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
        [&](USaveGame* Save,const FString&,int32 UserIndex){return FGameXXKSaveStorage::Write(Save,Slot,UserIndex,&LastError);}));
    const int32 InitialGold=Setup.PlayerGold;
    const int32 InitialNormal=FGameXXKTrainingRules::CountChestTokens(Setup.Training,EGameXXKTrainingRewardTier::NormalChest);
    const int32 InitialAdvanced=FGameXXKTrainingRules::CountChestTokens(Setup.Training,EGameXXKTrainingRewardTier::AdvancedChest);
    int32 Battles=0,Steps=0,Reloads=0;TSet<FGuid> Journeys;
    for(const auto& Chapter:FGameXXKMainStoryCatalog::Chapters())
    {
        if(!Require(Story->OpenChapter(Chapter.Id),Chapter.Id.ToString()+TEXT(" open")))return false;
        for(int32 Done=0;Done<Chapter.Nodes.Num();++Done)
        {
            FName Next=FGameXXKMainStoryRules::NextMainlineNode(MVP->GetRuntimeState(),Chapter.Id);
            if(Next.IsNone())for(FName Id:Chapter.Nodes)if(FGameXXKMainStoryRules::NodeState(MVP->GetRuntimeState(),Id)==EGameXXKTaskState::Available){Next=Id;break;}
            if(!Require(!Next.IsNone(),Chapter.Id.ToString()+TEXT(" no dead end")))return false;
            const auto* Node=FGameXXKMainStoryCatalog::FindNode(Next);
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("id"),Next.ToString());Row->SetStringField(TEXT("title"),Node->Title.ToString());
            Row->SetBoolField(TEXT("optional"),Node->bOptional);Rows.Add(MakeShared<FJsonValueObject>(Row));
            if(!Require(Story->StartTask(Next),Next.ToString()+TEXT(" start")))return false;
            if(Node->Kind==EGameXXKMainStoryNodeKind::JourneyBattle)
                for(int32 I=0;I<Node->Lines.Num();++I)if(!Require(Story->AdvanceDialogue(),Next.ToString()+TEXT(" outside dialogue")))return false;
            if(Node->IsJourney())
            {
                if(!Require(!MVP->GetRuntimeState().Training.bChallengeActive,Next.ToString()+TEXT(" waits outside")))return false;
                if(!Require(Story->BeginTaskJourney(),Next.ToString()+TEXT(" explicit departure")))return false;
                Journeys.Add(MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId);
                if(!Require(FGameXXKMainStoryRules::IsDedicatedJourney(MVP->GetRuntimeState()),Next.ToString()+TEXT(" dedicated map")))return false;
                const int32 Gate=MVP->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds.Array()[0];
                // The short map exposes the task naturally: do not fake reachability.
                if(!Require(MVP->GetRuntimeState().ReachableRouteNodeIds.Contains(Gate)&&Story->EnterJourneyGate(Gate),Next.ToString()+TEXT(" reachable task gate")))return false;
            }
            if(Node->Kind!=EGameXXKMainStoryNodeKind::JourneyBattle)
                for(int32 I=0;I<Node->Lines.Num();++I)if(!Require(Story->AdvanceDialogue(),Next.ToString()+TEXT(" dialogue")))return false;
            if(Node->IsInvestigation())
            {
                for(int32 I=0;I<Node->Options.Num();++I)if(!Node->Options[I].bCorrect)
                {if(!Require(!Story->ChooseAnswer(I)&&!FGameXXKMainStoryRules::IsNodeCompleted(MVP->GetRuntimeState(),Next),Next.ToString()+TEXT(" wrong answer stays active")))return false;break;}
                if(!Require(Story->RevealHint(),Next.ToString()+TEXT(" hint")))return false;
                for(int32 I=0;I<Node->Options.Num();++I)if(Node->Options[I].bCorrect)
                {if(!Require(Story->ChooseAnswer(I),Next.ToString()+TEXT(" correct answer")))return false;break;}
            }
            if(Node->Kind==EGameXXKMainStoryNodeKind::JourneyBattle)
            {
                ++Battles;int32 BattleSteps=0,MaxRound=0;TArray<TSharedPtr<FJsonValue>> Enemies;
                TArray<FName> ActualEnemyIds;
                for(const auto& Enemy:MVP->GetRuntimeState().ActiveBattleEnemies)ActualEnemyIds.Add(Enemy.EnemyDefinitionId);
                if(!Require(ActualEnemyIds==Node->EnemyDefinitionIds,Next.ToString()+TEXT(" battle matches approved illustration roster")))return false;
                for(const auto& Enemy:MVP->GetRuntimeState().ActiveBattleEnemies)Enemies.Add(MakeShared<FJsonValueString>(Enemy.EnemyDefinitionId.ToString()));
                Row->SetArrayField(TEXT("enemies"),Enemies);
                while(MVP->GetRuntimeState().CardRun.bHasActiveCardBattle&&BattleSteps<512)
                {
                    MaxRound=FMath::Max(MaxRound,MVP->GetRuntimeState().CardRun.ActiveBattle.RoundNumber);
                    bool Cleared=false;FGameXXKTrainingReward Reward;
                    if(!Require(MVP->AdvanceTrainingChallengeEncounter(Cleared,Reward),Next.ToString()+TEXT(" automatic card action")))return false;
                    if(!Require(!Cleared,Next.ToString()+TEXT(" does not clear ordinary stage")))return false;
                    ++BattleSteps;++Steps;
                    if(BattleSteps==1&&MVP->GetRuntimeState().CardRun.bHasActiveCardBattle)
                    {
                        const auto Units=MVP->GetRuntimeState().CardRun.ActiveBattle.Units;
                        if(!Require(MVP->SaveCurrentGame(Slot,0)&&MVP->LoadGameFromSlot(Slot,0),Next.ToString()+TEXT(" in-battle disk reload")))return false;
                        ++Reloads;const auto& Loaded=MVP->GetRuntimeState().CardRun.ActiveBattle.Units;
                        if(!Require(Units.Num()==Loaded.Num(),Next.ToString()+TEXT(" reload unit count")))return false;
                        for(int32 I=0;I<Units.Num();++I)if(!Require(Units[I].HP==Loaded[I].HP&&Units[I].Mana==Loaded[I].Mana&&Units[I].Armor==Loaded[I].Armor,
                            Next.ToString()+TEXT(" reload preserves unit resources")))return false;
                    }
                }
                Row->SetNumberField(TEXT("actual_combat_steps"),BattleSteps);Row->SetNumberField(TEXT("max_round"),MaxRound);
                if(!Require(BattleSteps<512&&FGameXXKMainStoryRules::HasBattleVictory(MVP->GetRuntimeState(),Next),Next.ToString()+TEXT(" actual victory")))return false;
                if(!Require(!Story->ClaimReward(Next),Next.ToString()+TEXT(" aftermath before reward")))return false;
                Row->SetNumberField(TEXT("after_battle_lines"),Node->AfterBattleLines.Num());
                for(int32 I=0;I<Node->AfterBattleLines.Num();++I)
                {
                    if(!Require(Story->AdvanceDialogue(),Next.ToString()+TEXT(" aftermath")))return false;
                    if(I==0)
                    {
                        if(!Require(MVP->SaveCurrentGame(Slot,0)&&MVP->LoadGameFromSlot(Slot,0),Next.ToString()+TEXT(" mid-aftermath disk reload")))return false;
                        ++Reloads;
                        if(!Require(MVP->GetRuntimeState().NarrativeProgress.MainStory.LineIndex==1&&!MVP->GetRuntimeState().CardRun.bHasActiveCardBattle,
                            Next.ToString()+TEXT(" aftermath resumes without another battle")))return false;
                    }
                }
                if(!Require(FGameXXKMainStoryRules::IsNodeCompleted(MVP->GetRuntimeState(),Next),Next.ToString()+TEXT(" aftermath completes task")))return false;
            }
            const int32 Gold=MVP->GetRuntimeState().PlayerGold;
            if(!Require(Story->ClaimReward(Next),Next.ToString()+TEXT(" claim")))return false;
            Row->SetNumberField(TEXT("gold_awarded"),MVP->GetRuntimeState().PlayerGold-Gold);
            Row->SetBoolField(TEXT("duplicate_claim_rejected"),!Story->ClaimReward(Next));
            if(!Require(Row->GetBoolField(TEXT("duplicate_claim_rejected")),Next.ToString()+TEXT(" no duplicate award")))return false;
            if(!Require(MVP->SaveCurrentGame(Slot,0)&&MVP->LoadGameFromSlot(Slot,0),Next.ToString()+TEXT(" node disk roundtrip")))return false;
            ++Reloads;Row->SetBoolField(TEXT("rewarded_after_reload"),FGameXXKMainStoryRules::NodeState(MVP->GetRuntimeState(),Next)==EGameXXKTaskState::Rewarded);
            if(!Require(Row->GetBoolField(TEXT("rewarded_after_reload")),Next.ToString()+TEXT(" receipt persists")))return false;
        }
        if(MVP->GetRuntimeState().Training.bChallengeActive&&!Require(MVP->CancelTrainingChallengeToWorkbench(),Chapter.Id.ToString()+TEXT(" exit")))return false;
    }
    const auto& Final=MVP->GetRuntimeState();
    Report->SetNumberField(TEXT("node_count"),Rows.Num());Report->SetNumberField(TEXT("journeys"),Journeys.Num());
    Report->SetNumberField(TEXT("real_battles"),Battles);Report->SetNumberField(TEXT("actual_combat_steps"),Steps);Report->SetNumberField(TEXT("disk_reload_count"),Reloads);
    Report->SetNumberField(TEXT("gold_awarded"),Final.PlayerGold-InitialGold);
    Report->SetNumberField(TEXT("normal_boxes"),FGameXXKTrainingRules::CountChestTokens(Final.Training,EGameXXKTrainingRewardTier::NormalChest)-InitialNormal);
    Report->SetNumberField(TEXT("advanced_boxes"),FGameXXKTrainingRules::CountChestTokens(Final.Training,EGameXXKTrainingRewardTier::AdvancedChest)-InitialAdvanced);
    const bool Complete=Rows.Num()==61&&Journeys.Num()==6&&Battles==4&&Final.PlayerGold-InitialGold==6100000
        &&Report->GetNumberField(TEXT("normal_boxes"))==60&&Report->GetNumberField(TEXT("advanced_boxes"))==60;
    Report->SetBoolField(TEXT("complete"),Complete);
    return Require(Complete,TEXT("full six-chapter totals"));
}
#endif
