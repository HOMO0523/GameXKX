#include "Misc/AutomationTest.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKHuntRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntStageCatalogTest,
    "GameXXK.Hunt.CatalogAndThreeChapterRosters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntStageCatalogTest::RunTest(const FString&)
{
    TestEqual(TEXT("27 base stages plus3 Hunts"),FGameXXKTrainingRules::GetStageDefinitions().Num(),30);
    const TCHAR* Names[]={TEXT("Normal"),TEXT("Hard"),TEXT("Hell")};
    for(int32 Difficulty=0;Difficulty<3;++Difficulty)
    {
        FGameXXKTrainingStageDefinition Last,Hunt;
        const auto Tier=static_cast<EGameXXKTrainingDifficulty>(Difficulty);
        const FName HuntId(*FString::Printf(TEXT("Training.%s.3-4"),Names[Difficulty]));
        TestTrue(TEXT("Original3-3 stays valid"),FGameXXKTrainingRules::TryGetStageDefinition(FGameXXKTrainingRules::MakeStageId(Tier,9),Last));
        TestEqual(TEXT("Original level bands are not shifted"),Last.CombatLevel,(Difficulty+1)*45);
        if(!TestTrue(TEXT("New3-4 is an authored stage"),FGameXXKTrainingRules::TryGetStageDefinition(HuntId,Hunt)))continue;
        TestEqual(TEXT("Hunt uses corresponding3-3 level"),Hunt.CombatLevel,Last.CombatLevel);
        TestEqual(TEXT("Base gold is150%"),Hunt.TravelGold,Last.TravelGold*3/2);
        TestEqual(TEXT("Base experience is150%"),Hunt.TravelExperience,Last.TravelExperience*3/2);
        const auto Encounters=FGameXXKTrainingRules::BuildEncounterSequence(HuntId);
        if(!TestEqual(TEXT("Three complete seven-encounter pools"),Encounters.Num(),21))continue;
        for(int32 Chapter=0;Chapter<3;++Chapter)
        {
            const auto Reference=FGameXXKTrainingRules::BuildEncounterSequence(FGameXXKTrainingRules::MakeStageId(Tier,(Chapter+1)*3));
            for(int32 N=0;N<7;++N)
            {
                TestTrue(TEXT("Hunt preserves authored three-slot composition"),Encounters[Chapter*7+N].EnemyDefinitionIds==Reference[N].EnemyDefinitionIds);
                TestEqual(TEXT("Every chapter uses final-stage level"),Encounters[Chapter*7+N].CombatLevel,Last.CombatLevel);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntAdvancedQualityTest,
    "GameXXK.Hunt.AdvancedChestHigherQuality",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntAdvancedQualityTest::RunTest(const FString&)
{
    FGameXXKRuntimeState Base=UGameXXKMVPRules::CreateNewGame();FString Error;
    if(!TestTrue(TEXT("Independent inventory fixture normalizes"),FGameXXKDesktopInventoryRules::Normalize(Base,&Error)))return false;
    int32 Epic=0,Legendary=0,Immortal=0;
    for(int32 Seed=1;Seed<=2048;++Seed)
    {
        auto State=Base;State.Training.ChallengeRewardSeed=Seed;
        if(!FGameXXKTrainingRules::AppendChestToken(State.Training,EGameXXKTrainingRewardTier::AdvancedChest,
            TEXT("Training.Normal.3-3"),37,&Error)){AddError(Error);return false;}
        FGameXXKTrainingChestOpenResult Result;
        if(!FGameXXKTrainingChestRules::OpenOne(State,EGameXXKTrainingRewardTier::AdvancedChest,Result)){AddError(Result.Message.ToString());return false;}
        auto Count=[&](int32 Rank){Epic+=Rank==static_cast<int32>(EGameXXKEquipmentQuality::Epic);Legendary+=Rank==static_cast<int32>(EGameXXKEquipmentQuality::Legendary);Immortal+=Rank==static_cast<int32>(EGameXXKEquipmentQuality::Immortal);};
        for(FName Id:Result.EquipmentInstanceIds)if(const auto* Item=FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection,Id))Count(static_cast<int32>(Item->Quality));
        for(const auto& Pair:Result.ItemDeltas){EGameXXKGemType Type;EGameXXKGemQuality Quality;if(FGameXXKGemRules::TryParseItemId(Pair.Key,Type,Quality))Count(static_cast<int32>(Quality));}
    }
    TestTrue(TEXT("Approved advanced table produces Epic"),Epic>0);
    TestTrue(TEXT("Approved advanced table produces Legendary"),Legendary>0);
    TestTrue(TEXT("Approved advanced table produces Immortal"),Immortal>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntQualityWeightsTest,
    "GameXXK.Hunt.ExactQualityWeights",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntQualityWeightsTest::RunTest(const FString&)
{
    // Approved 2026-09-11 three-chest table, in basis points. All columns sum to
    // FGameXXKTrainingChestRules::LootRollDomain and the 珍稀-and-above mass is a strict x4 ladder
    // (500 -> 2000 -> 8000, i.e. 1:4:16). The hunt chest is the only source of the top three ranks:
    // 天界 80bp / 登神 40bp on every difficulty, plus 宇宙 16bp from 地狱 only. The hunt column top
    // decays monotonically (200 > 100 > 80 > 40 > 16). 普通箱 stops at 至宝 because the design's
    // 0.001% 超凡 tail is below one basis point; 高级箱 stops at 超凡.
    const EGameXXKEquipmentQuality Ranks[]={EGameXXKEquipmentQuality::Common,EGameXXKEquipmentQuality::Rare,EGameXXKEquipmentQuality::Epic,EGameXXKEquipmentQuality::Legendary,EGameXXKEquipmentQuality::Immortal,EGameXXKEquipmentQuality::Treasure,EGameXXKEquipmentQuality::Transcendent,EGameXXKEquipmentQuality::Celestial,EGameXXKEquipmentQuality::Ascendant,EGameXXKEquipmentQuality::Cosmic};
    const EGameXXKTrainingRewardTier Tiers[4]={EGameXXKTrainingRewardTier::NormalChest,EGameXXKTrainingRewardTier::AdvancedChest,EGameXXKTrainingRewardTier::HuntChest,EGameXXKTrainingRewardTier::HuntChest};
    const EGameXXKTrainingDifficulty Difficulties[4]={EGameXXKTrainingDifficulty::Normal,EGameXXKTrainingDifficulty::Normal,EGameXXKTrainingDifficulty::Normal,EGameXXKTrainingDifficulty::Hell};
    const int32 Approved[4][10]={
        {7000,2500,400,90,9,1,0,0,0,0},               // 普通箱  珍稀及以上合计 500
        {5500,2500,1200,600,150,40,10,0,0,0},         // 高级箱  珍稀及以上合计 2000 (x4)
        {280,1500,4800,2400,600,200,100,80,40,0},     // 讨伐箱(非地狱)  珍稀及以上合计 8000 (x16)
        {264,1500,4800,2400,600,200,100,80,40,16}};   // 讨伐箱(地狱) 多一枚 16bp 宇宙直出
    // The normal chest stops at 至宝; only the hunt chest reaches 天界/登神, and only 地狱 reaches 宇宙.
    const int32 ExpectedRanks[4]={6,7,9,10};
    for(int32 Box=0;Box<4;++Box)
    {
        TMap<EGameXXKEquipmentQuality,int32> Counts;
        for(int32 Roll=0;Roll<FGameXXKTrainingChestRules::LootRollDomain;++Roll)++Counts.FindOrAdd(FGameXXKTrainingChestRules::ResolveLootQuality(Tiers[Box],Difficulties[Box],Roll));
        TestEqual(TEXT("Only approved ranks are reachable"),Counts.Num(),ExpectedRanks[Box]);
        for(int32 Rank=0;Rank<10;++Rank)TestEqual(TEXT("Complete basis-point domain matches approved table"),Counts.FindRef(Ranks[Rank]),Approved[Box][Rank]);
    }
    TestEqual(TEXT("Malformed draw is rejected"),FGameXXKTrainingChestRules::ResolveLootQuality(EGameXXKTrainingRewardTier::HuntChest,EGameXXKTrainingDifficulty::Hell,FGameXXKTrainingChestRules::LootRollDomain),EGameXXKEquipmentQuality::Invalid);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntOrderReplacementTest,
    "GameXXK.Hunt.OrderReplacementAndSource",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntOrderReplacementTest::RunTest(const FString&)
{
    for(auto Tier:{EGameXXKTrainingRewardTier::NormalChest,EGameXXKTrainingRewardTier::AdvancedChest,EGameXXKTrainingRewardTier::HuntChest})
    {
        int32 Count=0;for(int32 Roll=0;Roll<FGameXXKTrainingChestRules::LootRollDomain;++Roll)Count+=FGameXXKTrainingChestRules::ResolveOrderDrop(Tier,Roll);
        TestEqual(TEXT("Exact replacement probability"),Count,Tier==EGameXXKTrainingRewardTier::NormalChest?200:Tier==EGameXXKTrainingRewardTier::AdvancedChest?800:0);
    }
    for(auto Difficulty:{EGameXXKTrainingDifficulty::Normal,EGameXXKTrainingDifficulty::Hard,EGameXXKTrainingDifficulty::Hell})
    for(auto Tier:{EGameXXKTrainingRewardTier::NormalChest,EGameXXKTrainingRewardTier::AdvancedChest})
    {
        bool Found=false;
        for(int32 Seed=1;Seed<=1000&&!Found;++Seed)
        {
            auto State=UGameXXKMVPRules::CreateNewGame();FString Error;
            if(!FGameXXKDesktopInventoryRules::Normalize(State,&Error)){AddError(Error);return false;}
            State.Training.ChallengeRewardSeed=Seed;
            FGameXXKTrainingRules::AppendChestToken(State.Training,Tier,FGameXXKTrainingRules::MakeStageId(Difficulty,9),37,&Error);
            State.Training.SelectedStageId=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,1);
            FGameXXKTrainingChestOpenResult R;
            if(!FGameXXKTrainingChestRules::OpenOne(State,Tier,R)){AddError(R.Message.ToString());return false;}
            const FName Expected=FGameXXKHuntRules::OrderId(Difficulty);
            if(R.ItemDeltas.Contains(Expected))
            {
                Found=true;TestTrue(TEXT("Order replaces equipment"),R.EquipmentInstanceIds.IsEmpty());
                TestEqual(TEXT("Order is the only item reward"),R.ItemDeltas.Num(),1);
                TestEqual(TEXT("Exactly one source-quality order"),R.ItemDeltas.FindRef(Expected),1);
                TestEqual(TEXT("One chest consumed"),R.OpenedCount,1);
            }
        }
        TestTrue(TEXT("Deterministic sweep finds the source-quality order"),Found);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntOrderLifecycleTest,
    "GameXXK.Hunt.FirstClearReservationAndRefund",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntOrderLifecycleTest::RunTest(const FString&)
{
    auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();FString Error;
    const FName Stage(TEXT("Training.Normal.3-4")),Source(TEXT("Training.Normal.3-3"));
    TestFalse(TEXT("No ticket cannot enter"),FGameXXKHuntRules::CanEnter(State,Stage));
    State.Training.ClearedStageIds.Add(Source);
    TestTrue(TEXT("First clear grants"),FGameXXKHuntRules::GrantFirstClear(State,Source,&Error));
    TestTrue(TEXT("Repeated clear is harmless"),FGameXXKHuntRules::GrantFirstClear(State,Source,&Error));
    const FName Order=FGameXXKHuntRules::RequiredOrder(Stage);
    TestEqual(TEXT("First-clear order paid once"),FGameXXKHuntRules::Balance(State,Order),1);
    TestTrue(TEXT("Reserve a challenge"),FGameXXKHuntRules::Reserve(State,Stage,false,&Error));
    TestEqual(TEXT("Entry does not consume"),FGameXXKHuntRules::Balance(State,Order),1);
    FGameXXKHuntRules::Release(State);
    TestEqual(TEXT("Failure or exit refunds the reservation"),FGameXXKHuntRules::Balance(State,Order),1);
    TestTrue(TEXT("Retry reserves"),FGameXXKHuntRules::Reserve(State,Stage,false,&Error));
    TestTrue(TEXT("Success consumes one"),FGameXXKHuntRules::ConsumeReserved(State,&Error));
    TestEqual(TEXT("One success used one order"),FGameXXKHuntRules::Balance(State,Order),0);
    TestFalse(TEXT("Duplicate settlement cannot consume again"),FGameXXKHuntRules::ConsumeReserved(State,&Error));
    TestFalse(TEXT("No remaining order cannot restart"),FGameXXKHuntRules::CanEnter(State,Stage));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntPendingDeliveryTest,
    "GameXXK.Hunt.FullInventoryFirstClearDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntPendingDeliveryTest::RunTest(const FString&)
{
    auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();FString Error;FName Removable;
    for(FName Gem:FGameXXKGemRules::GetAllItemIds())
    {
        auto Container=FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State,EGameXXKDesktopItemContainer::Backpack)!=INDEX_NONE?
            EGameXXKDesktopItemContainer::Backpack:EGameXXKDesktopItemContainer::Warehouse;
        if(FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State,Container)==INDEX_NONE)break;
        if(Container==EGameXXKDesktopItemContainer::Backpack){State.Inventory.Add(Gem,1);Removable=Gem;}
        else State.DesktopInventory.WarehouseItems.Add(Gem,1);
        if(!FGameXXKDesktopInventoryRules::Normalize(State,&Error)){AddError(Error);return false;}
    }
    TestEqual(TEXT("Backpack really full"),FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State,EGameXXKDesktopItemContainer::Backpack),INDEX_NONE);
    TestEqual(TEXT("Warehouse really full"),FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State,EGameXXKDesktopItemContainer::Warehouse),INDEX_NONE);
    const FName Source(TEXT("Training.Normal.3-3")),Order=FGameXXKHuntRules::OrderId(EGameXXKTrainingDifficulty::Normal);
    State.Training.ClearedStageIds.Add(Source);
    TestTrue(TEXT("Full inventory does not lose mandatory order"),FGameXXKHuntRules::GrantFirstClear(State,Source,&Error));
    TestEqual(TEXT("Order kept for delivery"),State.Training.PendingHuntOrders.FindRef(Order),1);
    FGameXXKHuntRules::GrantFirstClear(State,Source,&Error);
    TestEqual(TEXT("Repeated clear cannot duplicate queued order"),State.Training.PendingHuntOrders.FindRef(Order),1);
    State.Inventory.Remove(Removable);
    TestTrue(TEXT("Freeing one cell delivers the order"),FGameXXKDesktopInventoryRules::Normalize(State,&Error));
    TestEqual(TEXT("Delivered exactly one"),FGameXXKHuntRules::Balance(State,Order),1);
    TestFalse(TEXT("Delivery clears pending entry"),State.Training.PendingHuntOrders.Contains(Order));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntChapterContinuityTest,
    "GameXXK.Hunt.ChapterContinuityAndRewardBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntChapterContinuityTest::RunTest(const FString&)
{
    auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();FString Error;
    const FName Stage(TEXT("Training.Normal.3-4"));State.Training.ClearedStageIds.Add(TEXT("Training.Normal.3-3"));
    FGameXXKHuntRules::GrantFirstClear(State,TEXT("Training.Normal.3-3"),&Error);
    FGameXXKHuntRules::Reserve(State,Stage,false,&Error);FGameXXKTrainingRules::StartChallenge(State.Training,Stage);
    FGameXXKTrainingRules::GenerateChallengeRouteMap(State,Stage,43219);
    if(!TestTrue(TEXT("First chapter generated"),State.bHasGeneratedRouteMap))return false;
    FGameXXKRelicRules::AcquireRelic(State,TEXT("Relic.TigerSeal"),&Error);
    State.CardRun.UpgradedCardQualities.Add(TEXT("Hero.Generic.QingFengYiShi"),EGameXXKCardQuality::Rare);
    State.PlayerHP=FMath::Max(1,State.PlayerHP-5);const int32 HP=State.PlayerHP;
    for(int32 Chapter=1;Chapter<=3;++Chapter)
    {
        TestEqual(TEXT("Current chapter"),State.CardRun.RouteProgress.CurrentChapter,Chapter);
        const auto Encounters=FGameXXKTrainingRules::BuildEncounterSequence(Stage);
        for(const auto& Node:State.RouteMapNodes)
        {
            const auto* Index=State.Training.ChallengeRouteNodeEncounterIndices.Find(Node.NodeId);if(!Index)continue;
            TestTrue(TEXT("Encounter belongs to current chapter"),*Index>=(Chapter-1)*7&&*Index<Chapter*7);
            const auto Expected=Node.NodeKind==EGameXXKNodeKind::Boss?EGameXXKTrainingEncounterKind::Boss:Node.NodeKind==EGameXXKNodeKind::Elite?EGameXXKTrainingEncounterKind::Elite:EGameXXKTrainingEncounterKind::Normal;
            TestEqual(TEXT("Map node uses matching encounter kind"),Encounters[*Index].Kind,Expected);
        }
        if(Chapter<3)
        {
            TestTrue(TEXT("Advance chapter without ending the run"),FGameXXKHuntRules::AdvanceChapter(State,&Error));
            TestEqual(TEXT("Wounds carry forward"),State.PlayerHP,HP);
            TestEqual(TEXT("Relic remains"),State.CardRun.Relics.Num(),1);
            TestTrue(TEXT("Deck upgrades remain"),State.CardRun.UpgradedCardQualities.Contains(TEXT("Hero.Generic.QingFengYiShi")));
            TestEqual(TEXT("Interchapter transition does not consume ticket"),FGameXXKHuntRules::Balance(State,FGameXXKHuntRules::RequiredOrder(Stage)),1);
        }
    }
    const auto Base=FGameXXKTrainingRules::BuildTravelReward(TEXT("Training.Normal.3-3"));
    const auto Travel=FGameXXKTrainingRules::BuildTravelReward(Stage);
    TestEqual(TEXT("Whole21-fight travel is150% of7-fight reference"),Travel.Gold,Base.Gold*21/2);
    const auto Boss=FGameXXKTrainingRules::ResolveChallengeReward(Stage,EGameXXKTrainingEncounterKind::Boss,1,0);
    TestEqual(TEXT("One final challenge payout"),Boss.Gold,Base.Gold*3);
    TestEqual(TEXT("Guaranteed final Hunt Chest"),Boss.ChestTier,EGameXXKTrainingRewardTier::HuntChest);
    const auto Mid=FGameXXKTrainingRules::ResolveTravelReward(Stage,EGameXXKTrainingEncounterKind::Boss,1,0,0,0,false);
    TestFalse(TEXT("Intermediate travel boss cannot drop the final chest"),Mid.bChestRolled);TestEqual(TEXT("No intermediate payout"),Mid.Gold,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntOfflineBudgetTest,
    "GameXXK.Hunt.OfflineConsumesOnlyCompletedRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntOfflineBudgetTest::RunTest(const FString&)
{
    auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();FString Error;
    const FName Stage(TEXT("Training.Normal.3-4")),Order=FGameXXKHuntRules::RequiredOrder(Stage);
    State.Training.ClearedStageIds.Add(TEXT("Training.Normal.3-3"));
    if(!TestTrue(TEXT("Two tickets granted"),FGameXXKHuntRules::Grant(State,Order,2,false,&Error)))return false;
    if(!TestTrue(TEXT("Travel reserves"),FGameXXKHuntRules::Reserve(State,Stage,true,&Error)))return false;
    if(!TestTrue(TEXT("Ticketed travel starts"),FGameXXKTrainingRules::StartTravel(State.Training,Stage)))return false;
    TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
    for(int32 Index=0;Index<3;++Index)
    {
        auto& U=Party.AddDefaulted_GetRef();U.UnitId=FName(*FString::Printf(TEXT("Budget.Party%d"),Index));
        U.MaxHP=U.HP=100000;U.Attack=100000;
    }
    FGameXXKTrainingTravelRuntime Runner;
    if(!TestTrue(TEXT("Real travel runner initializes"),FGameXXKTrainingRules::InitializeTravelRunner(State.Training,Runner,Party)))return false;
    FGameXXKTrainingOfflineReward Reward;
    if(!TestTrue(TEXT("Offline runner completes bounded routes"),FGameXXKTrainingRules::AdvanceTravelOffline(State.Training,Runner,3600,Reward)))return false;
    TestEqual(TEXT("Two tickets bound completed routes"),Reward.CompletedStages,2);
    TestEqual(TEXT("One guaranteed chest per completed route"),Reward.HuntChestCount,2);
    TestEqual(TEXT("Each route has21 encounters"),Reward.CompletedEncounters,42);
    const auto Single=FGameXXKTrainingRules::BuildTravelReward(Stage);
    TestEqual(TEXT("No per-chapter repeated gold"),Reward.Gold,Single.Gold*2);
    TestEqual(TEXT("No per-chapter repeated experience"),Reward.Experience,Single.Experience*2);
    TestFalse(TEXT("Zero budget stops the loop"),State.Training.bTravelActive);
    TestEqual(TEXT("Pure runner records inventory debt"),State.Training.PendingHuntTravelOrdersConsumed,2);
    TestTrue(TEXT("State boundary consumes the exact debt"),FGameXXKHuntRules::SettleTravelOrders(State,&Error));
    TestEqual(TEXT("Two completed routes used two orders"),FGameXXKHuntRules::Balance(State,Order),0);
    TestTrue(TEXT("Repeated debt settlement is harmless"),FGameXXKHuntRules::SettleTravelOrders(State,&Error));
    TestFalse(TEXT("No order cannot restart travel"),FGameXXKTrainingRules::StartTravel(State.Training,Stage));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntDeferredExperienceTest,
    "GameXXK.Hunt.ExactDeferredExperience",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntDeferredExperienceTest::RunTest(const FString&)
{
    auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();FString Error;
    State.PlayerLevel=1;State.PlayerXP=0;
    const FName CompanionId=State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
    auto* C=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CompanionId](const auto& V){return V.InstanceId==CompanionId;});
    if(!TestNotNull(TEXT("Companion present"),C))return false;C->Level=1;C->Experience=0;
    State.Training.HuntReferenceHeroExperience=270;State.Training.HuntReferenceCompanionExperience=90;
    TestTrue(TEXT("Reference experience granted at150%"),FGameXXKHuntRules::GrantDeferredExperience(State,&Error));
    auto Total=[](int32 Level,int32 XP){return (Level-1)*Level*50+XP;};
    TestEqual(TEXT("Hero receives405 across level boundaries"),Total(State.PlayerLevel,State.PlayerXP),405);
    C=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CompanionId](const auto& V){return V.InstanceId==CompanionId;});
    TestEqual(TEXT("Companion receives135 across level boundaries"),Total(C->Level,C->Experience),135);
    TestTrue(TEXT("Deferred reference cannot pay twice"),FGameXXKHuntRules::GrantDeferredExperience(State,&Error));
    TestEqual(TEXT("Repeated deferred grant gives no moreXP"),Total(State.PlayerLevel,State.PlayerXP),405);
    return true;
}

#endif
