#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "MVP/GameXXKSaveGame.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    bool BuildBurnBattle(FGameXXKRuntimeState& State,FString& Error,bool Two=true,bool Catalog=false)
    {
        auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();State=M->GetRuntimeStateCopy();
        State.Screen=EGameXXKScreen::Battle;State.bHasActiveBattle=true;State.ActiveBattleNodeId=411;
        FGameXXKBattleRuntimeUnit Enemy;Enemy.Id=TEXT("BurnEnemy.A");Enemy.DisplayName=FText::FromString(TEXT("Burn enemy"));
        Enemy.bEnemy=true;Enemy.HP=Enemy.MaxHP=100;Enemy.Attack=2;Enemy.Defense=0;Enemy.Speed=8;
        Enemy.BattleSlotNumber=Catalog?1:INDEX_NONE;
        if(Catalog){Enemy.EnemyDefinitionId=TEXT("Enemy.Ch1.Rooster");Enemy.CombatLevel=5;}
        State.ActiveBattleEnemies={Enemy};
        if(Two){Enemy.Id=TEXT("BurnEnemy.B");Enemy.BattleSlotNumber=Catalog?2:INDEX_NONE;State.ActiveBattleEnemies.Add(Enemy);}
        if(!FGameXXKCardBattleAdapter::BeginCardBattle(State,EGameXXKNodeKind::Battle,EGameXXKCardTerrain::Plain,916,&Error))return false;
        for(auto& U:State.CardRun.ActiveBattle.Units)if(U.Side==EGameXXKCardTargetSide::Enemy)
        {U.Armor=50;GameXXKCardRules::AddCombatStatus(U,EGameXXKCardStatus::Burn,6,TEXT("Player"));}
        TArray<FGameXXKCardDamageResult> Damage;
        return FGameXXKCardBattleAdapter::EndPlayerCardPhase(State,Damage,&Error);
    }
    FGameXXKCardCombatUnit* Enemy(FGameXXKRuntimeState& S,FName Id)
    {return S.CardRun.ActiveBattle.Units.FindByPredicate([&](const auto& U){return U.UnitId==Id;});}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBurnPerIntentTest,"GameXXK.EnemyBurn.AfterEachActualIntent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyBurnPerIntentTest::RunTest(const FString&)
{
    for(int32 Mode=0;Mode<4;++Mode)
    {
        FGameXXKRuntimeState S;FString Error;
        if(!TestTrue(TEXT("Burn fixture starts"),BuildBurnBattle(S,Error))){AddError(Error);return false;}
        TestEqual(TEXT("Forecasting did not apply Burn to live units"),Enemy(S,TEXT("BurnEnemy.A"))->HP,100);
        auto& I=S.CardRun.EnemyIntents[0];const FName Acting=I.SourceUnitId;
        const FName Other=Acting==TEXT("BurnEnemy.A")?FName(TEXT("BurnEnemy.B")):FName(TEXT("BurnEnemy.A"));
        if(Mode>0)
        {
            FGameXXKResolvedEnemyIntentEffect E;
            E.Type=Mode==1?EGameXXKEnemyIntentEffectType::DirectDamage:EGameXXKEnemyIntentEffectType::AddArmor;
            E.TargetRule=Mode==1?EGameXXKEnemyIntentTargetRule::LowestHealthParty:EGameXXKEnemyIntentTargetRule::Self;
            E.TargetUnitIds={Mode==1?FName(TEXT("Player")):Acting};E.Magnitude=2;E.HitCount=Mode==1?2:1;
            I.Effects={E};I.bCharging=Mode==3;
        }
        const int32 ArmorBefore=Enemy(S,Acting)->Armor;
        FGameXXKCardEnemyIntent Resolved;TArray<FGameXXKCardDamageResult> Damage;bool Finished=false;
        if(!TestTrue(TEXT("Actual intent resolves"),FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(S,Resolved,Damage,Finished,&Error))){AddError(Error);return false;}
        TestEqual(TEXT("Only the acting enemy takes one Burn tick, including multi-hit/support/charge"),Enemy(S,Acting)->HP,94);
        TestEqual(TEXT("Other enemies wait for their own intent"),Enemy(S,Other)->HP,100);
        TestEqual(TEXT("Burn is reported exactly once to the presentation queue"),Damage.FilterByPredicate([&](const auto& D){return D.Cause==EGameXXKCardDamageCause::Burn&&D.ResolvedTargetUnitId==Acting;}).Num(),1);
        TestEqual(TEXT("Burn does not consume armor"),Enemy(S,Acting)->Armor,ArmorBefore+(Mode==2?2:0));
        TestEqual(TEXT("Natural Burn preserves its reservoir"),GameXXKCardRules::GetCombatStatusStacks(*Enemy(S,Acting),EGameXXKCardStatus::Burn),6);
        if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(S,Resolved,Damage,Finished,&Error)){AddError(Error);return false;}
        TestEqual(TEXT("The next enemy burns after its own action"),Enemy(S,Other)->HP,94);
        TestEqual(TEXT("Finished actor is not charged again"),Enemy(S,Acting)->HP,94);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBurnLethalTest,"GameXXK.EnemyBurn.LethalAndSkippedSource",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyBurnLethalTest::RunTest(const FString&)
{
    FGameXXKRuntimeState S;FString Error;if(!BuildBurnBattle(S,Error,false)){AddError(Error);return false;}
    Enemy(S,TEXT("BurnEnemy.A"))->HP=5;
    FGameXXKCardEnemyIntent I;TArray<FGameXXKCardDamageResult> Damage;bool Finished=false;
    if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(S,I,Damage,Finished,&Error)){AddError(Error);return false;}
    TestEqual(TEXT("Burn can defeat the final enemy after its action"),S.CardRun.ActiveBattle.Phase,EGameXXKCardBattlePhase::Victory);
    TestFalse(TEXT("Lethal Burn marks the owner defeated"),Enemy(S,TEXT("BurnEnemy.A"))->bLiving);
    if(!BuildBurnBattle(S,Error)){AddError(Error);return false;}
    const FName Dead=S.CardRun.EnemyIntents[0].SourceUnitId;Enemy(S,Dead)->HP=0;Enemy(S,Dead)->bLiving=false;
    if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(S,I,Damage,Finished,&Error)){AddError(Error);return false;}
    TestTrue(TEXT("A skipped dead source executes no card and no Burn tick"),Damage.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyShowcaseLayerTest,"GameXXK.EnemyBurn.ActiveIntentForeground",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyShowcaseLayerTest::RunTest(const FString&)
{
    auto* Board=NewObject<UGameXXKBattleBoardWidget>();const auto Slate=Board->TakeWidget();
    auto* Showcase=Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentShowcaseCard"));
    if(!TestNotNull(TEXT("Active intent showcase"),Showcase))return false;
    const auto* Layer=Cast<UCanvasPanelSlot>(Showcase->Slot);if(!Layer)return false;
    for(FName Name:{FName(TEXT("BattleCinematicDimmer")),FName(TEXT("BattleCinematicImpact"))})
    {
        const auto* W=Board->WidgetTree->FindWidget(Name);const auto* Other=W?Cast<UCanvasPanelSlot>(W->Slot):nullptr;
        TestTrue(TEXT("Expanded intent is above character/impact cinematic layers"),Other&&Layer->GetZOrder()>Other->GetZOrder());
    }
    TestTrue(TEXT("Foreground showcase remains below exit/confirmation controls"),Layer->GetZOrder()<90);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBurnSavedIntentTest,"GameXXK.EnemyBurn.AuthoredIntentSaveResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEnemyBurnSavedIntentTest::RunTest(const FString&)
{
    FGameXXKRuntimeState S;FString Error;if(!BuildBurnBattle(S,Error,true,true)){AddError(Error);return false;}
    const FName First=S.CardRun.EnemyIntents[0].SourceUnitId,Second=S.CardRun.EnemyIntents[1].SourceUnitId;
    FGameXXKCardEnemyIntent Intent;TArray<FGameXXKCardDamageResult> Damage;bool Finished=false;
    if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(S,Intent,Damage,Finished,&Error)){AddError(Error);return false;}
    TestEqual(TEXT("Authored enemy intent also resolves natural Burn"),Enemy(S,First)->HP,94);
    auto* Save=NewObject<UGameXXKSaveGame>();Save->SaveState.RuntimeState=S;TArray<uint8> Bytes;
    if(!UGameplayStatics::SaveGameToMemory(Save,Bytes))return false;
    auto* Loaded=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));if(!Loaded)return false;
    auto Restored=Loaded->SaveState.RuntimeState;
    if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(Restored,Intent,Damage,Finished,&Error)){AddError(Error);return false;}
    TestEqual(TEXT("Resumed source burns once after its action"),Enemy(Restored,Second)->HP,94);
    TestEqual(TEXT("Reload does not repeat the already completed source's Burn"),Enemy(Restored,First)->HP,94);
    TestEqual(TEXT("Resumed cursor consumed exactly two intents"),Restored.CardRun.NextEnemyIntentIndex,2);
    return true;
}
#endif
