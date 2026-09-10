#include "Misc/AutomationTest.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKRelicSynergyRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "JsonObjectConverter.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicRedesignCatalogTest,
    "GameXXK.Relics.Redesign.CatalogAndOpeningCombo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicRedesignCatalogTest::RunTest(const FString&)
{
    int32 Common=0, Rare=0, Epic=0;
    TSet<FName> Ids;
    for (const auto& D : FGameXXKRelicCatalog::GetAllDefinitions())
    {
        TestFalse(TEXT("Stable relic IDs are unique"), Ids.Contains(D.Id)); Ids.Add(D.Id);
        Common += D.BaseQuality == EGameXXKCardQuality::Common;
        Rare += D.BaseQuality == EGameXXKCardQuality::Rare;
        Epic += D.BaseQuality == EGameXXKCardQuality::Epic;
    }
    TestEqual(TEXT("Preserved common and special relics"),Common,16);
    TestEqual(TEXT("Rare pool expanded"),Rare,20);
    TestEqual(TEXT("Epic pool expanded"),Epic,10);
    FGameXXKRuntimeState State;
    State.CardRun.bHasActiveCardBattle=true;
    auto& Battle=State.CardRun.ActiveBattle;
    Battle.RoundNumber=1;
    Battle.Phase=EGameXXKCardBattlePhase::Player;
    for(int32 Index=0;Index<3;++Index)
    {
        auto& Unit=Battle.Units.AddDefaulted_GetRef();
        Unit.UnitId=FName(*FString::Printf(TEXT("Party%d"),Index));
        Unit.Side=EGameXXKCardTargetSide::Party;Unit.StableSortOrder=Index;
        Unit.HP=Unit.MaxHP=100;Unit.Defense=50;Unit.Attack=100;
        Unit.bLiving=true;Unit.Speed=10;Unit.MaxMana=10;Unit.Mana=2;
    }
    FString Error;
    TestTrue(TEXT("Old TigerSeal ID remains valid"),FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.TigerSeal"),&Error));
    FGameXXKRelicRules::ApplyBattleStart(State);
    for(const auto& Unit:State.CardRun.ActiveBattle.Units)
    {
        TestEqual(TEXT("Opening momentum"),GameXXKCardRules::GetCombatStatusStacks(Unit,EGameXXKCardStatus::Momentum),6);
        TestEqual(TEXT("Opening charge"),GameXXKCardRules::GetCombatStatusStacks(Unit,EGameXXKCardStatus::Charge),4);
    }
    return true;
}

namespace
{
    FGameXXKRuntimeState SynergyFixture()
    {
        FGameXXKRuntimeState State;State.CardRun.bHasActiveCardBattle=true;
        auto& B=State.CardRun.ActiveBattle;B.RoundNumber=1;B.TeamMaxLevelSnapshot=50;
        B.Phase=EGameXXKCardBattlePhase::Player;B.Terrain=EGameXXKCardTerrain::Plain;
        B.Deck.PendingChoice.Kind=EGameXXKCardPendingChoiceKind::None;
        for(int32 N=0;N<4;++N)
        {
            auto& U=B.Units.AddDefaulted_GetRef();U.UnitId=N==3?FName(TEXT("Enemy")):FName(*FString::Printf(TEXT("Party%d"),N));
            U.Side=N==3?EGameXXKCardTargetSide::Enemy:EGameXXKCardTargetSide::Party;
            U.StableSortOrder=N;U.HP=N==3?1000:40;U.MaxHP=N==3?1000:100;
            U.bLiving=true;U.Speed=10;U.Attack=100;U.Defense=50;U.MaxMana=20;U.Mana=1;
            U.Role=N==0?EGameXXKCharacterRole::Hero:(N==1?EGameXXKCharacterRole::Healer:(N==2?EGameXXKCharacterRole::Guard:EGameXXKCharacterRole::Invalid));
            U.CombatLevel=50;
        }
        GameXXKCardRules::AddCombatStatus(B.Units[3],EGameXXKCardStatus::Bleed,10,TEXT("Party0"));
        GameXXKCardRules::AddCombatStatus(B.Units[3],EGameXXKCardStatus::Burn,10,TEXT("Party0"));
        TArray<FGameXXKCardInstance> Cards;
        for(int32 N=0;N<12;++N)
        {
            auto& Card=Cards.AddDefaulted_GetRef();Card.InstanceId=FName(*FString::Printf(TEXT("Draw%d"),N));Card.OwnerUnitId=TEXT("Party0");
            Card.CardId=TEXT("Hero.Generic.QingFengYiShi");Card.CurrentQuality=EGameXXKCardQuality::Common;
            Card.SourceEntryId=FName(*FString::Printf(TEXT("Entry%d"),N));Card.AcquisitionOrdinal=N;
        }
        const auto Units=B.Units;FString Error;
        if(!GameXXKCardRules::InitializeCardBattleRuntime(B,Cards,Units,EGameXXKCardTerrain::Plain,74123,&Error))
            UE_LOG(LogTemp,Error,TEXT("Relic fixture: %s"),*Error);
        return State;
    }
    FGameXXKRelicActionEvidence SynergyEvidence(const FGameXXKRuntimeState& State)
    {
        FGameXXKRelicActionEvidence E;E.BeforeUnits=State.CardRun.ActiveBattle.Units;
        E.CountBefore=0;E.CountAfter=1;E.PreviousOwner=TEXT("Party1");
        E.BeforeTerrain=EGameXXKCardTerrain::Plain;E.AfterTerrain=EGameXXKCardTerrain::Forest;
        E.Primary.OwnerUnitId=TEXT("Party0");E.Primary.CardInstanceId=TEXT("Active.Card");
        E.Primary.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;
        E.Primary.ActiveManaSpent=3;E.Primary.bActiveSpentLastMana=true;
        E.Primary.HeavyArrowChargeConsumed=2;E.Primary.ToxicExplosionDistinctDotTypeCounts.Add(2);
        auto& Heal=E.Primary.HealingResults.AddDefaulted_GetRef();Heal.TargetUnitId=TEXT("Party1");Heal.EffectiveHealing=20;
        auto& Armor=E.Primary.ArmorResults.AddDefaulted_GetRef();Armor.TargetUnitId=TEXT("Party0");Armor.EffectiveArmor=20;
        auto& Hit=E.Primary.DamageResults.AddDefaulted_GetRef();Hit.ResolvedTargetUnitId=TEXT("Enemy");
        Hit.SourceUnitId=TEXT("Party0");Hit.Kind=EGameXXKCardDamageKind::SingleTargetAttack;
        Hit.MarkStacksBeforeHit=2;Hit.BleedStacksBeforeHit=10;Hit.HealthDamage=20;
        return E;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicAllSynergiesTest,
    "GameXXK.Relics.Redesign.AllThirtyTriggersAndLimits",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicAllSynergiesTest::RunTest(const FString&)
{
    for(const auto& D:GameXXKRelicSynergyRules::Definitions())
    {
        auto State=SynergyFixture();auto E=SynergyEvidence(State);
        FGameXXKRelicInstance I;I.RelicId=D.Id;I.Stacks=9;I.SynergyRound=1;
        if(D.Id==TEXT("Relic.LotusSeed")||D.Id==TEXT("Relic.BambooTally")){E.CountBefore=2;E.CountAfter=3;}
        if(D.Id==TEXT("Relic.DrumCharm")){E.CountBefore=4;E.CountAfter=5;}
        if(D.Id==TEXT("Relic.ChessStone")){I.SynergyOwners.Add(TEXT("Party1"));I.SynergyOwners.Add(TEXT("Party2"));}
        if(D.Id==TEXT("Relic.HunterQuiver"))I.SynergyOwners.Add(TEXT("Party1"));
        if(D.Id==TEXT("Relic.ObsidianScale"))E.Primary.DamageResults[0].ResolvedTargetUnitId=TEXT("Party0");
        FGameXXKCardPlayResult Output;FString Error;
        TestTrue(*D.Id.ToString(),GameXXKRelicSynergyRules::Apply(State,D,I,D.Trigger,&E,&E.Primary.DamageResults,&Output,&Error));
        TestEqual(*(D.Id.ToString()+TEXT(" triggers once")),I.SynergyUses,1);
        TestEqual(TEXT("Primary evidence remains immutable"),E.Primary.HealingResults.Num(),1);
        if(D.Trigger==EGameXXKRelicTrigger::CardPlayed)
        {
            const int32 Energy=State.CardRun.ActiveBattle.Deck.SharedEnergy;
            const int32 Count=State.CardRun.ActiveBattle.Deck.Hand.Num();
            TestTrue(TEXT("Duplicate callback is harmless"),GameXXKRelicSynergyRules::Apply(State,D,I,D.Trigger,&E,&E.Primary.DamageResults,&Output,&Error));
            TestEqual(TEXT("No duplicate grant"),I.SynergyUses,1);TestEqual(TEXT("No duplicate energy"),State.CardRun.ActiveBattle.Deck.SharedEnergy,Energy);
            TestEqual(TEXT("No duplicate draw"),State.CardRun.ActiveBattle.Deck.Hand.Num(),Count);
        }
        if(D.Id==TEXT("Relic.RedCord"))TestEqual(TEXT("Old stacks never multiply redesigned armor"),State.CardRun.ActiveBattle.Units[1].Armor,20);
        if(D.Id==TEXT("Relic.DragonCarapace"))
        {TestEqual(TEXT("Real counter registrations"),State.CardRun.ActiveBattle.Reactions.Num(),3);TestEqual(TEXT("Defense scaling"),State.CardRun.ActiveBattle.Units[0].Armor,60);}
        if(D.Id==TEXT("Relic.IronKnot"))TestEqual(TEXT("Real block registrations"),State.CardRun.ActiveBattle.Reactions.Num(),2);
        if(D.Id==TEXT("Relic.BloodMoonBlade"))TestEqual(TEXT("Both DOT reservoirs triggered"),Output.DamageResults.Num(),2);
        if(!Error.IsEmpty())AddError(D.Id.ToString()+TEXT(": ")+Error);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicDeferredActionTest,
    "GameXXK.Relics.Redesign.DeferredImmutableAction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicDeferredActionTest::RunTest(const FString&)
{
    auto State=SynergyFixture();FString Error;
    FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.CloudMirror"),&Error);
    FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.RedCord"),&Error);
    const auto Before=State.CardRun.ActiveBattle;
    State.CardRun.ActiveBattle.ActiveCardsPlayedThisRound=1;
    State.CardRun.ActiveBattle.Deck.PendingChoice.Kind=EGameXXKCardPendingChoiceKind::ForcedDiscard;
    auto E=SynergyEvidence(State);E.Primary.ArmorResults.Reset();E.Primary.HealingResults.Reset();
    FGameXXKCardPlayResult Output=E.Primary;
    TestTrue(TEXT("Open choice defers relic action"),GameXXKRelicSynergyRules::BeginAction(State,Before,E.Primary,Output,&Error));
    TestEqual(TEXT("No early mana grant"),State.CardRun.ActiveBattle.Units[0].Mana,1);
    FString Serialized;FJsonObjectConverter::UStructToJsonObjectString(State.CardRun.PendingRelicAction,Serialized);
    FGameXXKRelicActionEvidence Restored;
    TestTrue(TEXT("Choice evidence survives reflected serialization"),FJsonObjectConverter::JsonObjectStringToUStruct(Serialized,&Restored));
    State.CardRun.PendingRelicAction=Restored;
    State.CardRun.ActiveBattle.Deck.PendingChoice.Kind=EGameXXKCardPendingChoiceKind::None;
    TArray<FGameXXKCardPlayResult> Resumed;auto& R=Resumed.AddDefaulted_GetRef();
    auto& H=R.HealingResults.AddDefaulted_GetRef();H.TargetUnitId=TEXT("Party1");H.EffectiveHealing=20;
    TestTrue(TEXT("Completed action applies later healing"),GameXXKRelicSynergyRules::ResumeAction(State,Resumed,&Error));
    TestEqual(TEXT("Delayed healing creates armor"),State.CardRun.ActiveBattle.Units[1].Armor,20);
    TestEqual(TEXT("Relic-created armor cannot trigger Mirror"),State.CardRun.ActiveBattle.Units[0].Mana,1);
    TestFalse(TEXT("Pending evidence cleared after commit"),State.CardRun.PendingRelicAction.bPending);
    TestTrue(TEXT("Repeating resume cannot grant again"),GameXXKRelicSynergyRules::ResumeAction(State,Resumed,&Error));
    TestEqual(TEXT("Armor unchanged after duplicate resume"),State.CardRun.ActiveBattle.Units[1].Armor,20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicRealCardComboTest,
    "GameXXK.Relics.Redesign.RealCardAdapterCombo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicRealCardComboTest::RunTest(const FString&)
{
    auto State=SynergyFixture();FString Error;
    // The adapter forecasts catalog enemy intents and requires an explicit presentation slot.
    auto& Enemy=State.CardRun.ActiveBattle.Units[3];
    Enemy.BattleSlotNumber=1;Enemy.EnemyDefinitionId=TEXT("Enemy.Ch1.Rooster");
    FGameXXKEnemyBattleState EnemyState;EnemyState.DefinitionId=Enemy.EnemyDefinitionId;
    EnemyState.CurrentPhase=1;EnemyState.TotalPhases=1;
    State.CardRun.ActiveBattle.EnemyStates.Add(Enemy.UnitId,EnemyState);
    for(FName Id:{FName(TEXT("Relic.BambooTally")),FName(TEXT("Relic.DrumCharm")),FName(TEXT("Relic.StarAbacus")),
        FName(TEXT("Relic.RedCord")),FName(TEXT("Relic.CloudMirror")),FName(TEXT("Relic.PhoenixCauldron"))})
        if(!TestTrue(TEXT("Acquire combo relic"),FGameXXKRelicRules::AcquireRelic(State,Id,&Error)))return false;
    State.CardRun.ActiveBattle.Deck.SharedEnergy=30;
    State.CardRun.ActiveBattle.Units[3].HP=State.CardRun.ActiveBattle.Units[3].MaxHP=100000;
    State.CardRun.ActiveBattle.Units[0].Mana=3;
    for(int32 Step=1;Step<=5;++Step)
    {
        auto& B=State.CardRun.ActiveBattle;
        if(!TestTrue(TEXT("Combo still has a real hand card"),!B.Deck.Hand.IsEmpty()))return false;
        auto& Card=B.Deck.Hand[0];
        Card.CardId=Step==1?FName(TEXT("Hero.Generic.HeYuZhan")):
            (Step==4?FName(TEXT("Hero.Generic.GuiYuanShu")):FName(TEXT("Hero.Generic.QingFengYiShi")));
        const FName CardId=Card.InstanceId;
        FGameXXKCardPlayPreview Preview;
        if(!TestTrue(TEXT("Real card preview"),GameXXKCardRules::BuildCardPlayPreview(B,CardId,Preview,&Error)))
        {AddError(Error);return false;}
        const int32 Energy=B.Deck.SharedEnergy;
        const int32 Mana=B.Units[0].Mana;
        const int32 Armor=B.Units[1].Armor;
        FGameXXKCardPlayResult Result;
        if(!TestTrue(TEXT("Commit through actual CardBattleAdapter"),FGameXXKCardBattleAdapter::ResolveCardPlay(
            State,CardId,Step==4?FName(TEXT("Party1")):FName(TEXT("Enemy")),Result,&Error)))
        {AddError(Error);return false;}
        if(Step==1)
        {
            TestTrue(TEXT("Actual paid last mana recorded"),Result.bActiveSpentLastMana);
            TestEqual(TEXT("Abacus refills the actual card owner"),State.CardRun.ActiveBattle.Units[0].Mana,20);
        }
        if(Step==3)TestEqual(TEXT("Third active returns2 energy after the real cost"),State.CardRun.ActiveBattle.Deck.SharedEnergy,Energy-Preview.EffectiveEnergyCost+2);
        if(Step==4)
        {
            if(TestTrue(TEXT("Real healing receipt"),!Result.HealingResults.IsEmpty()))
                TestEqual(TEXT("Cord copies only card healing, not Phoenix healing"),State.CardRun.ActiveBattle.Units[1].Armor,Armor+Result.HealingResults[0].EffectiveHealing);
            TestEqual(TEXT("Mirror ignores armor created by Cord"),State.CardRun.ActiveBattle.Units[0].Mana,Mana-Preview.EffectiveManaCost);
            TestTrue(TEXT("Phoenix appends party healing after real card healing"),Result.HealingResults.Num()>1);
        }
        if(Step==5)TestTrue(TEXT("Fifth active adds audited relic damage"),Result.DamageResults.ContainsByPredicate([](const auto& R){return R.Cause==EGameXXKCardDamageCause::Relic&&R.HealthDamage>0;}));
        TestFalse(TEXT("Normal card leaves no unresolved relic transaction"),State.CardRun.PendingRelicAction.bPending);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicChoicePhaseTest,
    "GameXXK.Relics.Redesign.ChoiceDefersTerminalPhase",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicChoicePhaseTest::RunTest(const FString&)
{
    auto State=SynergyFixture();auto& B=State.CardRun.ActiveBattle;
    B.Units[3].HP=0;B.Units[3].bLiving=false;
    B.Deck.PendingChoice.Kind=EGameXXKCardPendingChoiceKind::ForcedDiscard;
    FGameXXKCardPlayResult Output;FString Error;
    TestTrue(TEXT("Relic callbacks accept an unfinished active action"),FGameXXKRelicRules::ApplyCardPlayed(State,TEXT("Party0"),{},Output,&Error));
    TestEqual(TEXT("A mandatory card choice retains its player phase"),State.CardRun.ActiveBattle.Phase,EGameXXKCardBattlePhase::Player);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicNegativeConditionsTest,
    "GameXXK.Relics.Redesign.ConditionsRejectEmptyEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicNegativeConditionsTest::RunTest(const FString&)
{
    for(const auto& D:GameXXKRelicSynergyRules::Definitions())
    {
        if(D.Trigger!=EGameXXKRelicTrigger::CardPlayed||D.Id==TEXT("Relic.FlameCenser"))continue;
        auto State=SynergyFixture();FGameXXKRelicActionEvidence E;
        E.BeforeUnits=State.CardRun.ActiveBattle.Units;E.CountBefore=1;E.CountAfter=2;
        E.BeforeTerrain=E.AfterTerrain=EGameXXKCardTerrain::Plain;
        E.Primary.OwnerUnitId=TEXT("Party0");E.Primary.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;
        FGameXXKRelicInstance I;I.RelicId=D.Id;I.SynergyRound=1;I.SynergyOwners.Add(TEXT("Party0"));
        FString Error;FGameXXKCardPlayResult Output;
        TestTrue(*D.Id.ToString(),GameXXKRelicSynergyRules::Apply(State,D,I,D.Trigger,&E,&E.Primary.DamageResults,&Output,&Error));
        TestEqual(*(D.Id.ToString()+TEXT(" requires its real condition")),I.SynergyUses,0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKRelicSavedLimitTest,
    "GameXXK.Relics.Redesign.SavedLimitAndRoundReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKRelicSavedLimitTest::RunTest(const FString&)
{
    auto State=SynergyFixture();FString Error;
    FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.CloudMirror"),&Error);
    const auto* D=FGameXXKRelicCatalog::FindDefinition(TEXT("Relic.CloudMirror"));
    auto E=SynergyEvidence(State);FGameXXKCardPlayResult Output;
    for(int32 Ordinal=1;Ordinal<=5;++Ordinal)
    {
        E.CountBefore=Ordinal-1;E.CountAfter=Ordinal;
        TestTrue(TEXT("Limited armor-mana trigger"),GameXXKRelicSynergyRules::Apply(State,*D,State.CardRun.Relics[0],D->Trigger,&E,&E.Primary.DamageResults,&Output,&Error));
        if(Ordinal==2)
        {
            TArray<uint8> Bytes;FMemoryWriter Writer(Bytes,true);FObjectAndNameAsStringProxyArchive WA(Writer,false);WA.ArIsSaveGame=true;
            FGameXXKRelicInstance::StaticStruct()->SerializeItem(WA,&State.CardRun.Relics[0],nullptr);
            FGameXXKRelicInstance Restored;FMemoryReader Reader(Bytes,true);FObjectAndNameAsStringProxyArchive RA(Reader,false);RA.ArIsSaveGame=true;
            FGameXXKRelicInstance::StaticStruct()->SerializeItem(RA,&Restored,nullptr);
            TestFalse(TEXT("Real save archive succeeds"),Writer.IsError()||Reader.IsError());
            TestEqual(TEXT("Saved uses preserved"),Restored.SynergyUses,2);State.CardRun.Relics[0]=Restored;
        }
    }
    TestEqual(TEXT("Cap survives saving and excess actions"),State.CardRun.Relics[0].SynergyUses,3);
    TestEqual(TEXT("Only three grants paid"),State.CardRun.ActiveBattle.Units[0].Mana,10);
    ++State.CardRun.ActiveBattle.RoundNumber;E.CountBefore=0;E.CountAfter=1;
    TestTrue(TEXT("New round refreshes allowance"),GameXXKRelicSynergyRules::Apply(State,*D,State.CardRun.Relics[0],D->Trigger,&E,&E.Primary.DamageResults,&Output,&Error));
    TestEqual(TEXT("New round one use"),State.CardRun.Relics[0].SynergyUses,1);
    TestEqual(TEXT("New round paid once"),State.CardRun.ActiveBattle.Units[0].Mana,13);
    FGameXXKRelicRules::ApplyBattleStart(State);
    TestEqual(TEXT("New battle clears action allowance"),State.CardRun.Relics[0].SynergyUses,0);
    return true;
}
#endif
