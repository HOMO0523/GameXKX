#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKTravelLootWidget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/CanvasPanelSlot.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelLootSurfaceTest,
    "GameXXK.DesktopTraining.TravelLoot.Surface", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTravelLootSurfaceTest::RunTest(const FString&)
{
    auto* Host = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    Host->TakeWidget();
    Host->ConstructForTest();
    TestNotNull(TEXT("Travel strip has a local noninteractive reward surface"), Host->WidgetTree->FindWidget(TEXT("TravelLootEffects")));
    auto* Tab=Host->WidgetTree->FindWidget(TEXT("BackpackTabToggleButton"));
    auto* Strip=Host->WidgetTree->FindWidget(TEXT("TrainingTravelStrip"));
    if(!TestNotNull(TEXT("Existing bottom Tab is present"),Tab) || !Strip)return false;
    auto* TabSlot=Cast<UCanvasPanelSlot>(Tab->Slot);
    auto* StripSlot=Cast<UCanvasPanelSlot>(Strip->Slot);
    if(!TabSlot || !StripSlot)return false;
    TestTrue(TEXT("Coin arrival is the actual existing Tab center"),
        (StripSlot->GetPosition()+GameXXKTravelLoot::Target(EGameXXKTrainingRewardTier::None)).Equals(TabSlot->GetPosition()+TabSlot->GetSize()*0.5,0.1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelLootDeathTest,
    "GameXXK.DesktopTraining.TravelLoot.PaidDeathTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTravelLootDeathTest::RunTest(const FString&)
{
    FGameXXKTrainingTravelRuntime Before, After;
    Before.Phase=EGameXXKTrainingTravelPhase::Combat;
    Before.EnemyHP=1; Before.EnemyMaxHP=10; Before.PlayerHP=100; Before.PlayerMaxHP=100;
    Before.ActiveEnemyIndex=2;
    After=Before;After.EnemyHP=0;After.Phase=EGameXXKTrainingTravelPhase::Walking;
    FGameXXKTrainingReward Reward;Reward.Gold=23;Reward.bChestRolled=true;Reward.ChestTier=EGameXXKTrainingRewardTier::AdvancedChest;
    FGameXXKTrainingTravelVisualRuntime R;
    R.NotifyTravelStep(Before,After,true,false,false,Reward);
    TestTrue(TEXT("Reward does not fly before its death"),R.GetLootBursts().IsEmpty());
    R.Tick(0.42f);
    TestEqual(TEXT("Exactly one reward burst begins with death"),R.GetLootBursts().Num(),1);
    if(R.GetLootBursts().Num()!=1)return false;
    TestEqual(TEXT("Paid gold retained"),R.GetLootBursts()[0].Reward.Gold,23);
    TestEqual(TEXT("Original dying formation slot retained"),R.GetLootBursts()[0].EnemySlotIndex,2);
    TestEqual(TEXT("Correct chest tier retained"),R.GetLootBursts()[0].Reward.ChestTier,EGameXXKTrainingRewardTier::AdvancedChest);
    R.Synchronize(After);R.Tick(0.1f);
    TestEqual(TEXT("Snapshot refresh does not duplicate reward"),R.GetLootBursts().Num(),1);
    R.Tick(2);
    TestTrue(TEXT("Visual expires without waiting for another kill"),R.GetLootBursts().IsEmpty());
    R.NotifyTravelStep(Before,After,false,false,false,Reward);R.Tick(0.42f);
    TestTrue(TEXT("Intermediate monster death does not fabricate an encounter reward"),R.GetLootBursts().IsEmpty());
    R.Reset();R.NotifyTravelStep(Before,After,true,false,true,Reward);R.Tick(1);
    TestTrue(TEXT("Player defeat never awards visual loot"),R.GetLootBursts().IsEmpty());
    R.Reset();R.NotifyTravelStep(Before,After,true,false,false);R.Tick(1);
    TestTrue(TEXT("No paid reward means no burst"),R.GetLootBursts().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelLootPathTest,
    "GameXXK.DesktopTraining.TravelLoot.PathsStayInsideStrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTravelLootPathTest::RunTest(const FString&)
{
    for(int32 Slot=0;Slot<3;++Slot)for(auto Tier:{EGameXXKTrainingRewardTier::NormalChest,EGameXXKTrainingRewardTier::AdvancedChest,EGameXXKTrainingRewardTier::HuntChest})
    {
        FGameXXKTravelLootBurst B;B.EnemySlotIndex=Slot;B.Reward.ChestTier=Tier;B.Ordinal=static_cast<uint32>(Slot+5);
        for(int32 I=0;I<6;++I)for(int32 Frame=0;Frame<150;++Frame)
        {
            const bool Chest=I==5;const int32 Index=Chest?0:I;
            B.Age=Frame/100.0f;const auto S=GameXXKTravelLoot::Sample(B,Index,Chest);
            if(S.Opacity<=0)continue;
            const float Radius=(Chest?58.0f:32.0f)*S.Scale;
            if(S.Position.X-Radius<0||S.Position.X+Radius>1118||S.Position.Y-Radius<0||S.Position.Y+Radius>226)
                {AddError(TEXT("Visible reward escaped strip bounds"));return false;}
        }
        B.Age=GameXXKTravelLoot::Arrival(true);
        TestTrue(TEXT("Chest reaches its own entrance"),GameXXKTravelLoot::Sample(B,0,true).Position.Equals(GameXXKTravelLoot::Target(Tier),0.01));
        B.Age=GameXXKTravelLoot::Arrival(false,4);
        TestTrue(TEXT("Coin disappears at bottom Tab"),GameXXKTravelLoot::Sample(B,4,false).Position.Equals(GameXXKTravelLoot::Target(EGameXXKTrainingRewardTier::None),0.01));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelLootActualRewardTest,
    "GameXXK.DesktopTraining.TravelLoot.ActualTravelPaysOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTravelLootActualRewardTest::RunTest(const FString&)
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Start isolated new game"),M->StartGame()))return false;
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    Host->SetMVPSubsystem(M);Host->OpenWorkbench();Host->TakeWidget();
    const int32 GoldBefore=M->GetRuntimeState().PlayerGold;
    for(int32 I=0;I<1200 && Host->GetTravelPresentation().GetLootBursts().IsEmpty();++I) Host->TickForTest(0.1f);
    const auto& Bursts=Host->GetTravelPresentation().GetLootBursts();
    if(!TestEqual(TEXT("Actual first encounter emits one death burst"),Bursts.Num(),1))return false;
    const int32 GoldPaid=M->GetRuntimeState().PlayerGold-GoldBefore;
    TestTrue(TEXT("Encounter paid real currency"),GoldPaid>0);
    TestEqual(TEXT("Visual amount equals actual balance increase"),Bursts[0].Reward.Gold,GoldPaid);
    const auto Ordinal=Bursts[0].Ordinal;
    Host->OpenBackpack();
    TestEqual(TEXT("Layout change preserves active visual instead of restarting it"),Host->GetTravelPresentation().GetLootBursts()[0].Ordinal,Ordinal);
    for(int32 I=0;I<18;++I)Host->TickForTest(0.1f);
    TestEqual(TEXT("Absorption never pays a second time"),M->GetRuntimeState().PlayerGold,GoldBefore+GoldPaid);
    TestTrue(TEXT("Finished burst retires"),Host->GetTravelPresentation().GetLootBursts().IsEmpty());
    return true;
}
#endif
