#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKTalentRules.h"
#include "Narrative/GameXXKMainStoryRules.h"
#include "UI/GameXXKInterfaceHelpWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UGameXXKMVPSubsystem* StartSlotTestGame()
    {
        auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
        return Subsystem->StartGame() ? Subsystem : nullptr;
    }

    FGameXXKPartyMemberRef SlotMember(EGameXXKPartyMemberKind Kind, FName Id)
    {
        FGameXXKPartyMemberRef Ref;
        Ref.Kind = Kind;
        Ref.MemberId = Id;
        return Ref;
    }

    bool CompleteSlotTestStage(FGameXXKTrainingProgress& Progress, const TCHAR* Id)
    {
        return FGameXXKTrainingRules::StartChallenge(Progress, Id)
            && FGameXXKTrainingRules::CompleteChallenge(Progress, Id);
    }
    bool MeetFirstNpc(FGameXXKRuntimeState& State)
    {
        for(const FName Node:{FName(TEXT("S00-01")),FName(TEXT("S00-02"))})
        {
            if(!FGameXXKMainStoryRules::StartNode(State,Node))return false;
            for(int32 I=0;I<30&&!FGameXXKMainStoryRules::IsNodeCompleted(State,Node);++I)
                if(!FGameXXKMainStoryRules::AdvanceDialogue(State))return false;
            if(!FGameXXKMainStoryRules::IsNodeCompleted(State,Node))return false;
        }
        return true;
    }
    bool BuySlot(FGameXXKRuntimeState& State, EGameXXKPartyMemberKind Kind)
    {
        FGameXXKTalentPurchaseResult Result;
        return FGameXXKTalentRules::Purchase(State,FGameXXKPartyFormationRules::SlotTalentId(Kind),Result);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsNewGameTest,
    "GameXXK.PartySlots.NewGameAndUnlockSequence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsNewGameTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = StartSlotTestGame();
    if (!TestNotNull(TEXT("new game starts"), Subsystem)) return false;
    auto State = Subsystem->GetRuntimeStateCopy();
    TestEqual(TEXT("new game deploys only the hero"), State.CardRun.OrderedFormation.Members.Num(), 1);
    TestTrue(TEXT("initial 1-1 Travel remains available"), FGameXXKTrainingRules::CanTravel(State.Training, TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("initial 1-1 challenge is available"), FGameXXKTrainingRules::CanChallenge(State.Training, TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("the initial 1-1 clear makes 1-2 challengeable"), FGameXXKTrainingRules::CanChallenge(State.Training, TEXT("Training.Normal.1-2")));
    TestTrue(TEXT("no NPC is deployed"), State.CardRun.PartySelection.QuestNpc.NpcId.IsNone());
    TestTrue(TEXT("no companion is deployed"), State.CardRun.PartySelection.ActivePermanentCompanionInstanceId.IsNone());
    TestEqual(TEXT("Travel uses only the actual hero"), Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.Num(), 1);

    FGameXXKOrderedPartyFormation WithCompanion;
    WithCompanion.Members = {SlotMember(EGameXXKPartyMemberKind::Hero, TEXT("Player")),
        SlotMember(EGameXXKPartyMemberKind::PermanentCompanion, State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId)};
    auto WithNpc = WithCompanion;
    WithNpc.Members[1] = SlotMember(EGameXXKPartyMemberKind::QuestNpc, TEXT("Npc.YueBai"));
    TestFalse(TEXT("companion slot is initially locked"), FGameXXKPartyFormationRules::Validate(State, WithCompanion));
    TestFalse(TEXT("NPC slot is initially locked"), FGameXXKPartyFormationRules::Validate(State, WithNpc));
    State.PlayerGold=1000;
    auto EarlyPreparation=State;
    TestTrue(TEXT("a player may prepare the companion before choosing the already-open 1-2"), BuySlot(EarlyPreparation,EGameXXKPartyMemberKind::PermanentCompanion));
    TestTrue(TEXT("actual 1-1 completion"), CompleteSlotTestStage(State.Training, TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("1-2 now available"), FGameXXKTrainingRules::CanChallenge(State.Training, TEXT("Training.Normal.1-2")));
    TestFalse(TEXT("stage clear does not auto-buy the talent"), FGameXXKPartyFormationRules::Validate(State,WithCompanion));
    State.PlayerGold=199;
    TestFalse(TEXT("insufficient funds cannot unlock"),BuySlot(State,EGameXXKPartyMemberKind::PermanentCompanion));
    TestEqual(TEXT("failed purchase preserves gold"),State.PlayerGold,199);
    State.PlayerGold=1000;
    TestTrue(TEXT("pay for companion slot"),BuySlot(State,EGameXXKPartyMemberKind::PermanentCompanion));
    TestEqual(TEXT("slot costs 200 gold"),State.PlayerGold,800);
    TestFalse(TEXT("already unlocked cannot charge twice"),BuySlot(State,EGameXXKPartyMemberKind::PermanentCompanion));
    TestEqual(TEXT("duplicate purchase preserves gold"),State.PlayerGold,800);
    TestTrue(TEXT("1-2 permits the companion"), FGameXXKPartyFormationRules::Validate(State, WithCompanion));
    TestFalse(TEXT("NPC waits until 1-3"), FGameXXKPartyFormationRules::Validate(State, WithNpc));
    TestTrue(TEXT("actual 1-2 completion"), CompleteSlotTestStage(State.Training, TEXT("Training.Normal.1-2")));
    TestTrue(TEXT("1-3 now available"), FGameXXKTrainingRules::CanChallenge(State.Training, TEXT("Training.Normal.1-3")));
    TestTrue(TEXT("main story opens at 1-3"),FGameXXKMainStoryRules::IsChapterUnlocked(State,TEXT("S00")));
    TestFalse(TEXT("NPC unlock waits for meeting the spirit"),BuySlot(State,EGameXXKPartyMemberKind::QuestNpc));
    TestTrue(TEXT("meet first NPC through actual opening dialogue"),MeetFirstNpc(State));
    TestTrue(TEXT("pay for NPC slot"),BuySlot(State,EGameXXKPartyMemberKind::QuestNpc));
    TestTrue(TEXT("NPC can deploy without a companion"), FGameXXKPartyFormationRules::Validate(State, WithNpc));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsBattleShapesTest,
    "GameXXK.PartySlots.OptionalLineupsReachCardBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsBattleShapesTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = StartSlotTestGame();
    if (!TestNotNull(TEXT("new game starts"), Subsystem)) return false;
    auto Base = Subsystem->GetRuntimeStateCopy();
    TestTrue(TEXT("complete 1-1"), CompleteSlotTestStage(Base.Training, TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("complete 1-2"), CompleteSlotTestStage(Base.Training, TEXT("Training.Normal.1-2")));
    TestTrue(TEXT("meet first NPC"),MeetFirstNpc(Base));
    Base.PlayerGold=1000;
    TestTrue(TEXT("buy companion slot"),BuySlot(Base,EGameXXKPartyMemberKind::PermanentCompanion));
    TestTrue(TEXT("buy NPC slot"),BuySlot(Base,EGameXXKPartyMemberKind::QuestNpc));
    const FName CompanionId = Base.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId;
    for (int32 Mask = 0; Mask < 4; ++Mask)
    {
        auto State = Base;
        State.CardRun.OrderedFormation.Members = {SlotMember(EGameXXKPartyMemberKind::Hero, TEXT("Player"))};
        if (Mask & 1) State.CardRun.OrderedFormation.Members.Add(SlotMember(EGameXXKPartyMemberKind::PermanentCompanion, CompanionId));
        if (Mask & 2) State.CardRun.OrderedFormation.Members.Add(SlotMember(EGameXXKPartyMemberKind::QuestNpc, TEXT("Npc.YueBai")));
        FGameXXKPartyFormationRules::ProjectCompatibility(State);
        const int32 Count = 1 + ((Mask & 1) != 0) + ((Mask & 2) != 0);
        FString Error;
        const FString Label = FString::Printf(TEXT("lineup %d"), Mask);
        if (!TestTrue(*(Label + TEXT(" validates")), FGameXXKPartyFormationRules::Validate(State, State.CardRun.OrderedFormation, &Error))) continue;
        if (!TestTrue(*(Label + TEXT(" builds actual battle: ") + Error),
            UGameXXKMVPSubsystem::BuildDevelopmentTrainingBattle(State, TEXT("Training.Normal.1-1"), 0, 915, Error))) continue;
        const auto& Battle = State.CardRun.ActiveBattle;
        TestEqual(*(Label + TEXT(" has no phantom party units")),
            Battle.Units.FilterByPredicate([](const auto& Unit) { return Unit.Side == EGameXXKCardTargetSide::Party; }).Num(), Count);
        TestEqual(*(Label + TEXT(" has only deployed owners' cards")),
            Battle.Deck.ActiveInstanceIds.Num(), 8 + ((Mask & 1) ? 5 : 0) + ((Mask & 2) ? 3 : 0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsSaveTest,
    "GameXXK.PartySlots.OptionalSlotsPersist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsSaveTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = StartSlotTestGame();
    if (!TestNotNull(TEXT("new game starts"), Subsystem)) return false;
    auto State = Subsystem->GetRuntimeStateCopy();
    TestTrue(TEXT("complete 1-1"), CompleteSlotTestStage(State.Training, TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("complete 1-2"), CompleteSlotTestStage(State.Training, TEXT("Training.Normal.1-2")));
    State.PlayerGold=1000;
    TestTrue(TEXT("meet first NPC"),MeetFirstNpc(State));
    TestTrue(TEXT("buy companion slot"),BuySlot(State,EGameXXKPartyMemberKind::PermanentCompanion));
    TestTrue(TEXT("buy NPC slot"),BuySlot(State,EGameXXKPartyMemberKind::QuestNpc));
    State.CardRun.OrderedFormation.Members = {SlotMember(EGameXXKPartyMemberKind::Hero, TEXT("Player"))};
    FGameXXKPartyFormationRules::ProjectCompatibility(State);
    FString Error;
    TestTrue(TEXT("normalization accepts empty optional slots"), FGameXXKPartyFormationRules::Normalize(State, &Error));
    TestEqual(TEXT("normalization never refills the optional slots"), State.CardRun.OrderedFormation.Members.Num(), 1);
    auto* SaveObject=NewObject<UGameXXKSaveGame>();
    SaveObject->SaveState=UGameXXKMVPRules::MakeSaveState(State);
    TArray<uint8> Bytes;
    if(!TestTrue(TEXT("save serializes"),UGameplayStatics::SaveGameToMemory(SaveObject,Bytes)))return false;
    auto* Loaded=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if(!TestNotNull(TEXT("save deserializes"),Loaded))return false;
    const auto Save = Loaded->SaveState;
    FGameXXKRuntimeState Restored;
    FGameXXKSaveMigrationReport Report;
    if (TestTrue(*FString(TEXT("restore current save: ") + Report.Error), FGameXXKSaveMigration::TryRestoreRuntimeState(Save, Restored, Report)))
    {
        TestEqual(TEXT("saved empty formation remains hero-only"), Restored.CardRun.OrderedFormation.Members.Num(), 1);
        TestTrue(TEXT("saved NPC remains unequipped"), Restored.CardRun.PartySelection.QuestNpc.NpcId.IsNone());
        TestTrue(TEXT("saved companion remains unequipped"), Restored.CardRun.PartySelection.ActivePermanentCompanionInstanceId.IsNone());
        TestEqual(TEXT("owned companions are retained"), Restored.CardRun.CompanionRoster.PermanentCompanions.Num(), State.CardRun.CompanionRoster.PermanentCompanions.Num());
        TestEqual(TEXT("NPC personal loadouts are retained"), Restored.CardRun.PartySelection.QuestNpcCardLoadouts.Num(), State.CardRun.PartySelection.QuestNpcCardLoadouts.Num());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsShortGuideTest,
    "GameXXK.PartySlots.ShortGuideResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsShortGuideTest::RunTest(const FString& Parameters)
{
    auto* Help=NewObject<UGameXXKInterfaceHelpWidget>();
    for(const FName Group:{FName(TEXT("CompanionUnlock")),FName(TEXT("CompanionDeploy")),FName(TEXT("NpcUnlock")),FName(TEXT("NpcDeploy"))})
    {
        const FName First(*(TEXT("UI.Progression.V1.")+Group.ToString()+TEXT(".Select")));
        const FName Last(*(TEXT("UI.Progression.V1.")+Group.ToString()+TEXT(".Commit")));
        Help->ShowProgressionTutorial(nullptr,Group,{},100,[](FName){},[](FName){return true;});
        TestEqual(TEXT("each lesson offers at most two operations"),Help->GetStepCountForTest(),2);
        TestEqual(TEXT("first operation"),Help->GetCurrentCompletionIdForTest(),First);
        Help->Dismiss();
        Help->ShowProgressionTutorial(nullptr,Group,{First},100,[](FName){},[](FName){return true;});
        TestEqual(TEXT("resume skips completed operation"),Help->GetStepCountForTest(),1);
        TestEqual(TEXT("resume at commit"),Help->GetCurrentCompletionIdForTest(),Last);
        Help->ShowProgressionTutorial(nullptr,Group,{First,Last},100,[](FName){},[](FName){return true;});
        TestFalse(TEXT("completed short course does not replay"),Help->IsOpen());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsFacadeTest,
    "GameXXK.PartySlots.DeploymentTravelAndChallengeFreeze",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsFacadeTest::RunTest(const FString& Parameters)
{
    auto* Subsystem=StartSlotTestGame(); if(!TestNotNull(TEXT("fixture"),Subsystem))return false;
    auto& State=Subsystem->GetMutableRuntimeState();
    CompleteSlotTestStage(State.Training,TEXT("Training.Normal.1-1"));
    CompleteSlotTestStage(State.Training,TEXT("Training.Normal.1-2"));
    TestTrue(TEXT("meet first NPC"),MeetFirstNpc(State));
    State.PlayerGold=1000;
    FGameXXKTalentPurchaseResult Result;
    TestTrue(TEXT("facade buys companion talent"),Subsystem->PurchaseTalentNode(TEXT("Talent.Party.CompanionSlot"),Result));
    TestTrue(TEXT("facade buys NPC talent"),Subsystem->PurchaseTalentNode(TEXT("Talent.Party.NpcSlot"),Result));
    const FName Companion=State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId;
    TestTrue(TEXT("restart travel"),Subsystem->StartTrainingTravel(TEXT("Training.Normal.1-1")));
    TestTrue(TEXT("add companion"),Subsystem->SetActivePermanentCompanion(Companion));
    TestEqual(TEXT("travel immediately adds companion"),Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.Num(),2);
    TestTrue(TEXT("add NPC"),Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
    TestEqual(TEXT("travel immediately adds NPC"),Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.Num(),3);
    TestTrue(TEXT("remove companion"),Subsystem->ClearActivePermanentCompanion());
    TestEqual(TEXT("hero plus NPC travel"),Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.Num(),2);
    TestTrue(TEXT("remove NPC"),Subsystem->SelectTownQuestNpcForParty(NAME_None));
    TestEqual(TEXT("hero-only travel"),Subsystem->GetTrainingTravelRuntimeCopy().PartyUnits.Num(),1);
    TestEqual(TEXT("owned companions retained"),State.CardRun.CompanionRoster.PermanentCompanions.Num(),6);
    TestEqual(TEXT("owned NPC decks retained"),State.CardRun.PartySelection.QuestNpcCardLoadouts.Num(),6);
    TestTrue(TEXT("challenge starts"),Subsystem->StartTrainingChallenge(TEXT("Training.Normal.1-1")));
    TestFalse(TEXT("challenge pauses idle"),State.Training.bTravelActive);
    TestFalse(TEXT("cannot deploy while inside challenge"),Subsystem->SetActivePermanentCompanion(Companion));
    TestTrue(TEXT("return to workbench"),Subsystem->CancelTrainingChallengeToWorkbench());
    TestTrue(TEXT("deployment is editable again"),Subsystem->SetActivePermanentCompanion(Companion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsLegacyTest,
    "GameXXK.PartySlots.Legacy42RetainsUnlockedParty",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsLegacyTest::RunTest(const FString& Parameters)
{
    auto* Subsystem=StartSlotTestGame();if(!TestNotNull(TEXT("fixture"),Subsystem))return false;
    auto State=Subsystem->GetRuntimeStateCopy();
    GameXXKPermanentPartyTestFixtures::SkipTeachingChests(State);
    State.Training.bProgressivePartySlots=false;
    FGameXXKPartyFormationRules::BuildLegacyProjection(State,State.CardRun.OrderedFormation);
    FGameXXKPartyFormationRules::ProjectCompatibility(State);
    auto Save=UGameXXKMVPRules::MakeSaveState(State);Save.SaveVersion=42;
    FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
    if(!TestTrue(TEXT("v42 migrates"),FGameXXKSaveMigration::TryRestoreRuntimeState(Save,Restored,Report)))return false;
    TestFalse(TEXT("legacy does not enter new progression gates"),Restored.Training.bProgressivePartySlots);
    TestTrue(TEXT("legacy companion stays unlocked for free"),FGameXXKPartyFormationRules::IsSlotUnlocked(Restored,EGameXXKPartyMemberKind::PermanentCompanion));
    TestTrue(TEXT("legacy NPC stays unlocked for free"),FGameXXKPartyFormationRules::IsSlotUnlocked(Restored,EGameXXKPartyMemberKind::QuestNpc));
    TestEqual(TEXT("legacy roster preserved"),Restored.CardRun.OrderedFormation.Members.Num(),3);
    TestEqual(TEXT("migration does not charge gold"),Restored.PlayerGold,State.PlayerGold);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartySlotsOperationGuideTest,
    "GameXXK.PartySlots.ActualTalentAndDeploymentGuide",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPartySlotsOperationGuideTest::RunTest(const FString& Parameters)
{
    auto* Subsystem=StartSlotTestGame();if(!TestNotNull(TEXT("fixture"),Subsystem))return false;
    Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto& State=Subsystem->GetMutableRuntimeState();
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(Subsystem);Host->ConstructForTest();Host->OpenWorkbench();Host->OpenBackpack();
    Host->TickForTest(0);Host->RefreshBackpackFooterVisibility();
    for(const FName Name:{FName(TEXT("CharacterRosterCompanionButton")),FName(TEXT("CharacterRosterNpcButton"))})
        if(auto* Tab=Host->WidgetTree->FindWidget(Name))TestEqual(TEXT("footer refresh keeps locked role tabs hidden"),Tab->GetVisibility(),ESlateVisibility::Collapsed);
    Host->HandleActionClicked(1);Host->TickForTest(0);
    for(int32 I=1;I<=2;++I)
    {
        auto* Lock=Cast<UImage>(Host->WidgetTree->FindWidget(*FString::Printf(TEXT("FormationSlotLock_%d"),I)));
        if(TestNotNull(TEXT("locked slot uses a lock image"),Lock))
            TestTrue(TEXT("lock reuses approved art"),Lock->GetBrush().GetResourceObject()&&Lock->GetBrush().GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_CardLockedIcon")));
        auto* Edit=Host->WidgetTree->FindWidget(*FString::Printf(TEXT("FormationEditDeck_%d"),I));
        if(TestNotNull(TEXT("locked slot edit control exists"),Edit))TestEqual(TEXT("locked slot does not display disabled action text"),Edit->GetVisibility(),ESlateVisibility::Collapsed);
    }
    TestFalse(TEXT("failed unlock cannot be recorded as complete"),Host->RecordInterfaceTutorialStep(TEXT("UI.Progression.V1.CompanionUnlock.Commit")));
    TestFalse(TEXT("early task entry remains unavailable"),FGameXXKMainStoryRules::IsChapterUnlocked(State,TEXT("S00")));
    CompleteSlotTestStage(State.Training,TEXT("Training.Normal.1-1"));
    CompleteSlotTestStage(State.Training,TEXT("Training.Normal.1-2"));
    TestTrue(TEXT("meet the first NPC"),MeetFirstNpc(State));
    TestTrue(TEXT("claim the meeting task reward"),FGameXXKMainStoryRules::ClaimReward(State,TEXT("S00-02")));
    State.PlayerGold=1000;State.GuideProgress.Preference=EGameXXKGuidePreference::NewPlayer;
    for(const FName Group:{FName(TEXT("CompanionUnlock")),FName(TEXT("CompanionFormation")),FName(TEXT("CompanionDeploy")),
        FName(TEXT("NpcUnlock")),FName(TEXT("NpcFormation")),FName(TEXT("NpcDeploy"))})
    {
        const bool Navigation=Group.ToString().EndsWith(TEXT("Formation"));
        if(Navigation){Host->TickForTest(0);Host->OfferPartyProgressionGuide(true);}else Host->ShowPartyProgressionGuide(Group);
        auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
        if(!TestNotNull(TEXT("short guide surface"),Help))return false;
        auto Tick=[&](){Host->TickForTest(0);Help->NativeTick(FGeometry(),.05f);Host->TickForTest(0);Help->NativeTick(FGeometry(),.05f);};
        TestEqual(TEXT("navigation is one operation; other lessons are two"),Help->GetStepCountForTest(),Navigation?1:2);
        for(int32 I=0;I<(Navigation?1:2);++I)
        {
            Tick();const auto Completion=Help->GetCurrentCompletionIdForTest();
            auto* Target=Cast<UButton>(Help->GetCurrentTargetForTest());
            if(!TestNotNull(Group.ToString()+TEXT(" actual action target"),Target))return false;
            if(Navigation)TestEqual(TEXT("purchase automatically highlights the formation icon"),Target->GetFName(),FName(TEXT("BottomNavigationButton_1")));
            TestTrue(TEXT("target is enabled"),Target->GetIsEnabled());
            Target->OnClicked.Broadcast();Tick();
            TestTrue(Completion.ToString()+TEXT(" committed after the actual operation"),State.GuideProgress.CompletedGuideStepIds.Contains(Completion));
        }
        TestFalse(TEXT("two actions end the guide"),Help->IsOpen());
    }
    TestEqual(TEXT("two slot talents charge exactly 400"),State.PlayerGold,600);
    TestEqual(TEXT("first NPC is the spirit from the story"),State.CardRun.PartySelection.QuestNpc.NpcId,FName(TEXT("Npc.YueBai")));
    TestEqual(TEXT("formation actually contains three deployed members"),State.CardRun.OrderedFormation.Members.Num(),3);
    Subsystem->ResetSaveSlotWriteDelegateForTest();return true;
}
#endif
