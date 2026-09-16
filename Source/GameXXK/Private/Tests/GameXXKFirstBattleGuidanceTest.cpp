#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Kismet/GameplayStatics.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "Guide/GameXXKFirstBattleGuideRules.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKInterfaceHelpWidget.h"
#include "UI/GameXXKBattleStatusIconWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UGameXXKMVPSubsystem* NewProfile()
    {
        auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
        M->StartGame();
        M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
        return M;
    }
    bool EnterFirstMonster(UGameXXKMVPSubsystem* M)
    {
        if(!M->StartTrainingChallenge(TEXT("Training.Normal.1-1")))return false;
        const auto& S=M->GetRuntimeState();
        const auto* N=S.RouteMapNodes.FindByPredicate([&](const auto& Node){return Node.NodeKind==EGameXXKNodeKind::Battle && S.ReachableRouteNodeIds.Contains(Node.NodeId);});
        return N&&M->SelectRouteNodeById(N->NodeId);
    }
    bool HasCard(const TArray<FGameXXKCardInstance>& Zone,FName Id)
    {return Zone.ContainsByPredicate([&](const auto& C){return C.CardId==Id;});}
    bool NextRound(FGameXXKRuntimeState& S,FString& Error)
    {
        TArray<FGameXXKCardDamageResult> Damage;
        return FGameXXKCardBattleAdapter::EndPlayerCardPhase(S,Damage,&Error)
            && FGameXXKCardBattleAdapter::ResolveEnemyPhase(S,Damage,&Error);
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleOpeningHandTest,"GameXXK.FirstBattleGuide.OpeningHand",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleOpeningHandTest::RunTest(const FString&)
{
    for(int32 Seed=1;Seed<=12;++Seed)
    {
        auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
        M->GetMutableRuntimeState().Training.ChallengeRewardSeed=Seed;
        if(!M->StartTrainingChallenge(TEXT("Training.Normal.1-1")))return false;
        const auto& S=M->GetRuntimeState();
        const auto* N=S.RouteMapNodes.FindByPredicate([&](const auto& Node){return Node.NodeKind==EGameXXKNodeKind::Battle && S.ReachableRouteNodeIds.Contains(Node.NodeId);});
        if(!N || !M->SelectRouteNodeById(N->NodeId))return false;
        const auto& Hand=M->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand;
        TestTrue(TEXT("Guided attack is always in the opening hand"),Hand.ContainsByPredicate([](const auto& C){return C.CardId==TEXT("Hero.Generic.QingFengYiShi");}));
        TestFalse(TEXT("Healing waits until after the enemy turn"),Hand.ContainsByPredicate([](const auto& C){return C.CardId==TEXT("Hero.Generic.GuiYuanShu");}));
        TestFalse(TEXT("Armor waits until after the enemy turn"),Hand.ContainsByPredicate([](const auto& C){return C.CardId==TEXT("Hero.Generic.HengJianShouShi");}));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleSupportDrawTest,"GameXXK.FirstBattleGuide.SupportAfterEnemy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleSupportDrawTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!TestTrue(TEXT("First monster starts"),EnterFirstMonster(M)))return false;
    auto& S=M->GetMutableRuntimeState();auto& D=S.CardRun.ActiveBattle.Deck;FString Error;
    const auto Ledger=D.ActiveInstanceIds;
    // An early extra-draw effect must also respect the opening support holdback.
    auto DrawProbe=D;DrawProbe.DiscardPile.Append(DrawProbe.Hand);DrawProbe.Hand.Reset();
    if(!TestTrue(TEXT("Extra draw succeeds"),GameXXKCardRules::DrawCards(DrawProbe,5,0,&Error))){AddError(Error);return false;}
    for(const auto& C:DrawProbe.Hand)TestFalse(TEXT("Extra opening draws exclude all hero heal/armor cards"),C.OwnerUnitId==TEXT("Player")&&GameXXKFirstBattleGuide::IsSupportCard(C.CardId));
    TestTrue(TEXT("Extra draw preserves card conservation"),GameXXKCardRules::ValidateDeckState(DrawProbe,&Error));
    if(!TestTrue(TEXT("Real enemy turn resolves"),NextRound(S,Error))){AddError(Error);return false;}
    TestEqual(TEXT("Next player round"),S.CardRun.ActiveBattle.RoundNumber,2);
    TestTrue(TEXT("Heal is ready after enemy action"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.GuiYuanShu")));
    TestTrue(TEXT("Armor is ready after enemy action"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.HengJianShouShi")));
    auto BeforeIds=Ledger,AfterIds=S.CardRun.ActiveBattle.Deck.ActiveInstanceIds;
    BeforeIds.Sort(FNameLexicalLess());AfterIds.Sort(FNameLexicalLess());
    TestEqual(TEXT("Schedule neither duplicates nor deletes battle cards"),AfterIds,BeforeIds);
    TestTrue(TEXT("Scheduled runtime is valid"),GameXXKCardRules::ValidateCardBattleRuntime(S.CardRun.ActiveBattle,&Error));
    TestTrue(TEXT("Enemy acted before support lesson"),GameXXKFirstBattleGuide::Has(S,TEXT("EndTurn")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleBorrowedDeckTest,"GameXXK.FirstBattleGuide.CustomDeckBattleOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleBorrowedDeckTest::RunTest(const FString&)
{
    auto* M=NewProfile();auto& Before=M->GetMutableRuntimeState();FString Error;TArray<FName> Selected;
    for(const auto Id:Before.CardRun.HeroUnlockedCardIds)
        if(Id!=TEXT("Hero.Generic.QingFengYiShi")&&Id!=TEXT("Hero.Generic.GuiYuanShu")&&Id!=TEXT("Hero.Generic.HengJianShouShi")&&Selected.Num()<8)Selected.Add(Id);
    if(!TestTrue(TEXT("Custom loadout removes all three lesson cards"),FGameXXKCardBattleAdapter::SetHeroSelectedCards(Before,Selected,&Error))){AddError(Error);return false;}
    const auto Unlocked=Before.CardRun.HeroUnlockedCardIds;
    if(!TestTrue(TEXT("Custom deck starts first monster"),EnterFirstMonster(M)))return false;
    auto& S=M->GetMutableRuntimeState();
    TestEqual(TEXT("Only battle gains three borrowed instances"),S.CardRun.ActiveBattle.Deck.ActiveInstanceIds.Num(),11);
    TestTrue(TEXT("Missing attack guaranteed"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.QingFengYiShi")));
    if(!TestTrue(TEXT("Custom deck reaches round two"),NextRound(S,Error))){AddError(Error);return false;}
    TestTrue(TEXT("Borrowed healing delivered"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.GuiYuanShu")));
    TestTrue(TEXT("Borrowed armor delivered"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.HengJianShouShi")));
    TestEqual(TEXT("Permanent selections unchanged during battle"),S.CardRun.HeroSelectedCardIds,Selected);
    TestEqual(TEXT("Unlocked pool unchanged"),S.CardRun.HeroUnlockedCardIds,Unlocked);
    FGameXXKCardBattleAdapter::ClearActiveCardBattle(S);
    TestTrue(TEXT("Borrowed cards disappear with the battle deck"),S.CardRun.ActiveBattle.Deck.ActiveInstanceIds.IsEmpty());
    TestEqual(TEXT("Permanent selections unchanged after battle"),S.CardRun.HeroSelectedCardIds,Selected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleOutcomeTest,"GameXXK.FirstBattleGuide.AlternateCardOutcomes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleOutcomeTest::RunTest(const FString&)
{
    FGameXXKRuntimeState S;S.GuideProgress.bFirstBattleGuideEnabled=true;S.Training.bChallengeActive=true;S.CardRun.bHasActiveCardBattle=true;
    TArray<FGameXXKCardInstance> Cards;
    for(FName Id:{FName(TEXT("Hero.Generic.HeYuZhan")),FName(TEXT("Hero.Generic.GuiYuanFanZhao"))})
    {FGameXXKCardInstance C;C.InstanceId=Id;C.CardId=Id;C.OwnerUnitId=TEXT("Player");C.SourceEntryId=Id;C.AcquisitionOrdinal=Cards.Num();Cards.Add(C);}
    FGameXXKCardCombatUnit Hero;Hero.UnitId=TEXT("Player");Hero.Side=EGameXXKCardTargetSide::Party;Hero.Role=EGameXXKCharacterRole::Hero;
    Hero.bLiving=true;Hero.HP=50;Hero.MaxHP=100;Hero.Attack=10;Hero.Defense=20;Hero.Mana=20;Hero.MaxMana=20;Hero.Speed=1;Hero.StableSortOrder=1;
    auto Enemy=Hero;Enemy.UnitId=TEXT("Enemy");Enemy.Side=EGameXXKCardTargetSide::Enemy;Enemy.Role=EGameXXKCharacterRole::Invalid;Enemy.HP=Enemy.MaxHP=1000;Enemy.StableSortOrder=10;
    FString Error;auto& B=S.CardRun.ActiveBattle;
    if(!TestTrue(TEXT("Alternate-card combat initializes"),GameXXKCardRules::InitializeCardBattleRuntime(B,Cards,{Hero,Enemy},EGameXXKCardTerrain::Plain,17,&Error))){AddError(Error);return false;}
    B.Deck.bFirstBattleGuidance=true;B.Deck.FirstBattleDrawPhase=2;B.RoundNumber=2;
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Qi")));
    FGameXXKCardPlayResult Result;
    if(!TestTrue(TEXT("Another attack resolves"),GameXXKCardRules::ResolveCardPlay(B,Cards[0].InstanceId,TEXT("Enemy"),Result,&Error))){AddError(Error);return false;}
    GameXXKFirstBattleGuide::ObserveDamage(S,Result.DamageResults);
    TestTrue(TEXT("Real alternate attack skips attack tutorial"),GameXXKFirstBattleGuide::Has(S,TEXT("Damage")));
    TestEqual(TEXT("Healing has priority after enemy turn"),GameXXKFirstBattleGuide::NextTopic(S),FName(TEXT("Heal")));
    if(!TestTrue(TEXT("Alternate combined heal/armor card resolves"),GameXXKCardRules::ResolveCardPlay(B,Cards[1].InstanceId,NAME_None,Result,&Error))){AddError(Error);return false;}
    GameXXKFirstBattleGuide::Observe(S);
    TestTrue(TEXT("Real alternate healing skips heal tutorial"),GameXXKFirstBattleGuide::Has(S,TEXT("Heal")));
    TestTrue(TEXT("Real alternate armor skips armor tutorial"),GameXXKFirstBattleGuide::Has(S,TEXT("Armor")));
    TestEqual(TEXT("Still introduces actual armor status icon"),GameXXKFirstBattleGuide::NextTopic(S),FName(TEXT("ArmorView")));
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("ArmorView")));
    TestEqual(TEXT("Auto is introduced last"),GameXXKFirstBattleGuide::NextTopic(S),FName(TEXT("Auto")));
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Auto")));GameXXKFirstBattleGuide::Observe(S);
    TestTrue(TEXT("Lesson completes by outcomes"),GameXXKFirstBattleGuide::Has(S,TEXT("Complete")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleSaveTest,"GameXXK.FirstBattleGuide.SaveResumeAndLegacy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleSaveTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!EnterFirstMonster(M))return false;
    auto* Save=NewObject<UGameXXKSaveGame>();Save->SaveState.RuntimeState=M->GetRuntimeState();Save->SaveState.SaveVersion=FGameXXKSaveMigration::CurrentSaveVersion;
    TArray<uint8> Bytes;if(!TestTrue(TEXT("Runtime serializes"),UGameplayStatics::SaveGameToMemory(Save,Bytes)))return false;
    auto* Loaded=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));if(!TestNotNull(TEXT("Runtime reloads"),Loaded))return false;
    auto& S=Loaded->SaveState.RuntimeState;FString Error;
    TestTrue(TEXT("New profile guide enable persists"),S.GuideProgress.bFirstBattleGuideEnabled);
    TestEqual(TEXT("Opening holdback persists"),S.CardRun.ActiveBattle.Deck.FirstBattleDrawPhase,1);
    TestEqual(TEXT("Exact opening hand count resumes"),S.CardRun.ActiveBattle.Deck.Hand.Num(),M->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.Num());
    if(!TestTrue(TEXT("Resumed enemy turn resolves"),NextRound(S,Error))){AddError(Error);return false;}
    TestTrue(TEXT("Resumed schedule supplies healing"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.GuiYuanShu")));
    auto Legacy=Loaded->SaveState;Legacy.SaveVersion=46;FGameXXKSaveState Migrated;FGameXXKSaveMigrationReport Report;
    if(!TestTrue(TEXT("v46 migration succeeds"),FGameXXKSaveMigration::MigrateToCurrent(Legacy,Migrated,Report))){AddError(Report.Error);return false;}
    TestFalse(TEXT("Existing saves are not enrolled"),Migrated.RuntimeState.GuideProgress.bFirstBattleGuideEnabled);
    TestFalse(TEXT("Existing battle draw schedule stays ordinary"),Migrated.RuntimeState.CardRun.ActiveBattle.Deck.bFirstBattleGuidance);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleDismissTest,"GameXXK.FirstBattleGuide.DismissAndExperienced",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleDismissTest::RunTest(const FString&)
{
    auto* M=NewProfile();M->GetMutableRuntimeState().GuideProgress.Preference=EGameXXKGuidePreference::ExperiencedPlayer;
    if(!EnterFirstMonster(M))return false;
    TestFalse(TEXT("Experienced preference opts out of draw scheduling"),M->GetRuntimeState().CardRun.ActiveBattle.Deck.bFirstBattleGuidance);
    M=NewProfile();if(!EnterFirstMonster(M))return false;
    TestTrue(TEXT("Player can dismiss the lesson"),M->CommitFirstBattleGuideStep(TEXT("Dismiss")));
    TestTrue(TEXT("Dismiss hides this battle's hints"),GameXXKFirstBattleGuide::NextTopic(M->GetRuntimeState()).IsNone());
    TestEqual(TEXT("Dismiss removes future forced draws"),M->GetRuntimeState().CardRun.ActiveBattle.Deck.FirstBattleDrawPhase,0);
    TestFalse(TEXT("Dismiss does not fake mastery"),GameXXKFirstBattleGuide::Has(M->GetRuntimeState(),TEXT("Complete")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleBoardSequenceTest,"GameXXK.FirstBattleGuide.BoardSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleBoardSequenceTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!EnterFirstMonster(M))return false;
    auto* Board=NewObject<UGameXXKBattleBoardWidget>();Board->SetMVPSubsystem(M);const auto BoardSlate=Board->TakeWidget();BoardSlate->SlatePrepass();Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
    if(!TestEqual(TEXT("Board shows actual attack-card introduction"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("AttackView"))))
    {AddError(FString::Printf(TEXT("Board probe: next=%s world=%s visibility=%d auto=%d debug=%s"),*GameXXKFirstBattleGuide::NextTopic(M->GetRuntimeState()).ToString(),*GetNameSafe(Board->GetWorld()),int32(Board->GetVisibility()),Board->IsAutoBattleEnabled(),*Board->GetBattleBoardDebugStateForTest()));return false;}
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Board->WidgetTree->FindWidget(TEXT("FirstBattleHelp")));
    if(!TestNotNull(TEXT("Lesson belongs to battle board"),Help))return false;
    auto Read=[&]()
    {
        auto* Target=Help->GetCurrentTargetForTest();if(!Target)return false;
        const auto TargetSlate=Target->TakeWidget();
        TargetSlate->OnMouseEnter(FGeometry(),FPointerEvent());
        if(!TestTrue(TEXT("Hovered target retains its live Slate widget"),Target->IsHovered()))return false;
        Help->NativeTick(FGeometry(),1.1f);Help->CloseForTest();
        TargetSlate->OnMouseLeave(FPointerEvent());
        Board->NativeTick(FGeometry(),1.f);return true;
    };
    if(!Read())return false;
    TestEqual(TEXT("After hover/read asks to play attack"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("Attack")));
    auto& S=M->GetMutableRuntimeState();FString Error;FGameXXKCardPlayResult Result;
    const auto* Attack=S.CardRun.ActiveBattle.Deck.Hand.FindByPredicate([](const auto& C){return C.CardId==TEXT("Hero.Generic.QingFengYiShi");});
    const auto* Enemy=S.CardRun.ActiveBattle.Units.FindByPredicate([](const auto& U){return U.Side==EGameXXKCardTargetSide::Enemy&&U.bLiving;});
    if(!Attack||!Enemy)return false;const FName AttackInstance=Attack->InstanceId,EnemyId=Enemy->UnitId;
    if(!TestTrue(TEXT("Attack produces actual result"),FGameXXKCardBattleAdapter::ResolveCardPlay(S,AttackInstance,EnemyId,Result,&Error))){AddError(Error);return false;}
    Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
    TestEqual(TEXT("Actual damage triggers Qi introduction"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("Qi")));
    Help->CloseForTest();Board->NativeTick(FGeometry(),1.f);
    TestEqual(TEXT("Qi reading leads to End Turn button"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("EndTurn")));
    if(!NextRound(S,Error)){AddError(Error);return false;}
    Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
    TestEqual(TEXT("After enemy acts, healing comes first"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("Heal")));
    for(const auto CardId:{FName(TEXT("Hero.Generic.GuiYuanShu")),FName(TEXT("Hero.Generic.HengJianShouShi"))})
    {
        const auto* Card=S.CardRun.ActiveBattle.Deck.Hand.FindByPredicate([&](const auto& C){return C.CardId==CardId;});
        if(!Card)return false;const FName Id=Card->InstanceId;
        if(!TestTrue(TEXT("Support produces actual result"),FGameXXKCardBattleAdapter::ResolveCardPlay(S,Id,TEXT("Player"),Result,&Error))){AddError(Error);return false;}
        Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
        TestEqual(TEXT("Guide follows healed/armored result"),Board->GetFirstBattleGuideStepForTest(),FName(CardId==TEXT("Hero.Generic.GuiYuanShu")?TEXT("Armor"):TEXT("ArmorView")));
    }
    auto* ArmorIcon=Cast<UGameXXKBattleStatusIconWidget>(Help->GetCurrentTargetForTest());
    if(!TestNotNull(TEXT("Highlights the actual armor status icon"),ArmorIcon))return false;
    TestEqual(TEXT("Correct status icon"),ArmorIcon->GetIconIdForTest(),FName(TEXT("ArmorShield")));
    if(!Read())return false;
    TestEqual(TEXT("Auto is the last introduction"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("Auto")));
    Help->CloseForTest();Board->NativeTick(FGeometry(),1.f);
    TestTrue(TEXT("Guide closes after actual sequence"),Board->GetFirstBattleGuideStepForTest().IsNone());
    TestFalse(TEXT("Introducing auto never turns it on"),Board->IsAutoBattleEnabled());
    TestTrue(TEXT("Actual sequence is remembered"),GameXXKFirstBattleGuide::Has(M->GetRuntimeState(),TEXT("Complete")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleQuickVictoryTest,"GameXXK.FirstBattleGuide.QuickVictoryNeverBlocked",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleQuickVictoryTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!EnterFirstMonster(M))return false;
    auto& S=M->GetMutableRuntimeState();auto& B=S.CardRun.ActiveBattle;
    FName EnemyId;
    for(auto& U:B.Units)if(U.Side==EGameXXKCardTargetSide::Enemy)
    {
        if(EnemyId.IsNone()){U.HP=1;U.Armor=0;U.Defense=0;EnemyId=U.UnitId;}
        else {U.HP=0;U.bLiving=false;}
    }
    const auto* Attack=B.Deck.Hand.FindByPredicate([](const auto& C){return C.CardId==TEXT("Hero.Generic.QingFengYiShi");});
    if(!Attack)return false;const FName Id=Attack->InstanceId;FGameXXKCardPlayResult Result;FString Error;
    if(!TestTrue(TEXT("Real card can win before completing guide"),FGameXXKCardBattleAdapter::ResolveCardPlay(S,Id,EnemyId,Result,&Error))){AddError(Error);return false;}
    TestEqual(TEXT("First turn victory remains victory"),S.CardRun.ActiveBattle.Phase,EGameXXKCardBattlePhase::Victory);
    TestTrue(TEXT("No stale manual step after victory"),GameXXKFirstBattleGuide::NextTopic(S).IsNone());
    bool Done=false;FGameXXKTrainingReward Reward;
    TestTrue(TEXT("Normal challenge reward handler remains available"),M->AdvanceTrainingChallengeEncounter(Done,Reward));
    TestFalse(TEXT("Quick victory does not fake tutorial completion"),GameXXKFirstBattleGuide::Has(M->GetRuntimeState(),TEXT("Complete")));
    TestFalse(TEXT("First monster is not whole-stage completion"),Done);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleClosePromptContinuationTest,"GameXXK.FirstBattleGuide.ClosePromptContinuesSameBattle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleClosePromptContinuationTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!EnterFirstMonster(M))return false;
    auto& S=M->GetMutableRuntimeState();
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Damage")));
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Qi")));
    auto* Board=NewObject<UGameXXKBattleBoardWidget>();Board->SetMVPSubsystem(M);
    const auto Slate=Board->TakeWidget();Slate->SlatePrepass();Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
    if(!TestEqual(TEXT("First round asks player to let enemy act"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("EndTurn"))))return false;
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Board->WidgetTree->FindWidget(TEXT("FirstBattleHelp")));if(!Help)return false;
    Help->CloseForTest();Board->NativeTick(FGeometry(),.1f);
    TestFalse(TEXT("Closing the small prompt does not immediately reopen it"),Help->IsOpen());
    TestTrue(TEXT("Closing one prompt preserves this battle's support lesson"),S.CardRun.ActiveBattle.Deck.bFirstBattleGuidance);
    // Exercise the real Board end-turn / enemy presentation boundary, not a direct rules-only jump.
    if(!TestTrue(TEXT("Real End Turn action succeeds"),Board->EndCardPlayerPhase()))return false;
    for(int32 I=0;I<60;++I)
    {Board->AdvanceEnemyIntentPresentationForTest(.5f);Board->NativeTick(FGeometry(),.5f);}
    Board->RefreshFromState();Board->NativeTick(FGeometry(),1.f);
    TestEqual(TEXT("Same ordinary battle reaches round two"),S.CardRun.ActiveBattle.RoundNumber,2);
    TestTrue(TEXT("Healing guaranteed despite closing first prompt"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.GuiYuanShu")));
    TestTrue(TEXT("Armor guaranteed despite closing first prompt"),HasCard(S.CardRun.ActiveBattle.Deck.Hand,TEXT("Hero.Generic.HengJianShouShi")));
    TestEqual(TEXT("Second round introduces healing before another End Turn"),Board->GetFirstBattleGuideStepForTest(),FName(TEXT("Heal")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleMissedEndTurnMarkerTest,"GameXXK.FirstBattleGuide.SecondRoundUsesLivePhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleMissedEndTurnMarkerTest::RunTest(const FString&)
{
    auto* M=NewProfile();if(!EnterFirstMonster(M))return false;
    auto& S=M->GetMutableRuntimeState();FString Error;
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Damage")));
    S.GuideProgress.CompletedGuideStepIds.Add(GameXXKFirstBattleGuide::Marker(TEXT("Qi")));
    if(!NextRound(S,Error)){AddError(Error);return false;}
    S.GuideProgress.CompletedGuideStepIds.Remove(GameXXKFirstBattleGuide::Marker(TEXT("EndTurn")));
    TestEqual(TEXT("An older missing marker cannot request another End Turn before support"),GameXXKFirstBattleGuide::NextTopic(S),FName(TEXT("Heal")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirstBattleDirect12Test,"GameXXK.FirstBattleGuide.FirstBattleCanBe12",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFirstBattleDirect12Test::RunTest(const FString&)
{
    auto* M=NewProfile();
    TestEqual(TEXT("No travel completion needed"),M->GetRuntimeState().Training.TravelVictories,0);
    if(!TestTrue(TEXT("The first ever challenge can be 1-2"),M->StartTrainingChallenge(TEXT("Training.Normal.1-2"))))return false;
    const auto& S=M->GetRuntimeState();
    TestFalse(TEXT("Opening the route alone is not combat instruction"),GameXXKFirstBattleGuide::Has(S,TEXT("Started")));
    const auto* Node=S.RouteMapNodes.FindByPredicate([&](const auto& N){return N.NodeKind==EGameXXKNodeKind::Battle&&S.ReachableRouteNodeIds.Contains(N.NodeId);});
    if(!Node||!M->SelectRouteNodeById(Node->NodeId))return false;
    const auto& B=M->GetRuntimeState().CardRun.ActiveBattle;
    TestTrue(TEXT("Actual first monster in 1-2 starts teaching"),B.Deck.bFirstBattleGuidance);
    TestTrue(TEXT("Attack guarantee also applies in 1-2"),HasCard(B.Deck.Hand,TEXT("Hero.Generic.QingFengYiShi")));
    TestFalse(TEXT("Healing still waits until enemy action"),HasCard(B.Deck.Hand,TEXT("Hero.Generic.GuiYuanShu")));
    TestFalse(TEXT("Armor still waits until enemy action"),HasCard(B.Deck.Hand,TEXT("Hero.Generic.HengJianShouShi")));
    TestEqual(TEXT("First guide targets attack rather than a hard-coded 1-1 event"),GameXXKFirstBattleGuide::NextTopic(M->GetRuntimeState()),FName(TEXT("AttackView")));
    return true;
}
#endif
