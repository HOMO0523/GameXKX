#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/GameXXKRewardPresentation.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKMainStoryPanelWidget.h"
#include "UI/GameXXKTrainingSettlementWidget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKMainStorySubsystem.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardRowTest,"GameXXK.RewardPresentation.ExactIconsAndAmounts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRewardRowTest::RunTest(const FString&)
{
    auto* Tree=NewObject<UWidgetTree>();
    Tree->RootWidget=GameXXKRewardPresentation::BuildRow(Tree,{101,2,3,0,10},TEXT("TestReward"));
    TestEqual(TEXT("Only actual reward types occupy the row"),Cast<UHorizontalBox>(Tree->RootWidget)->GetChildrenCount(),3);
    for(const auto Kind:{EGameXXKRewardIcon::Gold,EGameXXKRewardIcon::Normal,EGameXXKRewardIcon::Advanced})
    {
        auto* Icon=Cast<UImage>(Tree->FindWidget(GameXXKRewardPresentation::IconName(TEXT("TestReward"),Kind)));
        if(!TestNotNull(TEXT("Each type owns an icon"),Icon))return false;
        TestEqual(TEXT("Uses the same runtime asset as the desktop receiver"),Icon->GetBrush().GetResourceObject()->GetPathName(),FString(GameXXKRewardPresentation::Texture(Kind)));
        TestEqual(TEXT("Large story reward icon"),Icon->GetBrush().ImageSize.X,72.f);
    }
    TArray<FString> Labels;Tree->ForEachWidget([&](UWidget* W){if(auto* T=Cast<UTextBlock>(W))Labels.Add(T->GetText().ToString());});
    for(const FString Value:{FString(TEXT("+101")),FString(TEXT("×2")),FString(TEXT("×3"))})TestTrue(TEXT("Exact amount is readable without rounding down to ten-thousands"),Labels.Contains(Value));
    auto* Settlement=NewObject<UGameXXKTrainingSettlementWidget>();const auto Slate=Settlement->TakeWidget();
    FGameXXKTrainingSettlementReceipt R;R.ReceiptId=FGuid::NewGuid();R.NormalChestCount=2;R.AdvancedChestCount=3;R.HuntChestCount=1;R.ChestItemLevel=10;Settlement->SetReceipt(R);
    for(const auto Kind:{EGameXXKRewardIcon::Normal,EGameXXKRewardIcon::Advanced,EGameXXKRewardIcon::Hunt})
        TestNotNull(TEXT("Mixed battle rewards keep all three chest images"),Settlement->WidgetTree->FindWidget(GameXXKRewardPresentation::IconName(TEXT("Settlement"),Kind)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardFlightMathTest,"GameXXK.RewardPresentation.FlightAndIdempotence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRewardFlightMathTest::RunTest(const FString&)
{
    const FVector2D Start(700,620),End(990,82);
    for(bool Chest:{false,true})
    {
        const auto Flying=GameXXKRewardPresentation::Sample(Start,End,.6f,0,Chest);
        TestTrue(TEXT("Visible icons move away from their reward source"),Flying.Opacity>0&&!Flying.Position.Equals(Start));
        const auto Arrived=GameXXKRewardPresentation::Sample(Start,End,1.15f,0,Chest);
        TestTrue(TEXT("Arrival uses the exact receiver center"),Arrived.Position.Equals(End));
        TestEqual(TEXT("Icon disappears into its receiver"),Arrived.Opacity,0.f);
    }
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();
    const int32 Gold=M->GetRuntimeState().PlayerGold;
    const FGameXXKRewardBundle Bundle{100000,10,2,1,10};
    GameXXKRewardPresentation::Play(Host,Bundle,{},TEXT("Receipt.Probe"));
    auto* Flights=Cast<UGameXXKRewardFlightWidget>(Host->WidgetTree->FindWidget(TEXT("RewardFlightEffects")));
    if(!TestNotNull(TEXT("Persistent desktop layer is created"),Flights))return false;
    TestEqual(TEXT("Burst count is bounded, independent of gold amount"),Flights->GetFlightCountForTest(),11);
    GameXXKRewardPresentation::Play(Host,Bundle,{},TEXT("Receipt.Probe"));
    TestEqual(TEXT("Same receipt cannot duplicate presentation"),Flights->GetRewardEventCountForTest(),1);
    Host->OpenBackpack();Host->HandleActionClicked(2);Host->HandleActionClicked(4);
    TestTrue(TEXT("Layout changes preserve the same flight layer"),Host->WidgetTree->FindWidget(TEXT("RewardFlightEffects"))==Flights);
    TestEqual(TEXT("Animation never grants currency"),M->GetRuntimeState().PlayerGold,Gold);
    GameXXKRewardPresentation::Reset(Host);TestEqual(TEXT("Reset clears old profile flight history"),Flights->GetRewardEventCountForTest(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRewardClaimFailureTest,"GameXXK.RewardPresentation.ClaimSuccessOnly",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRewardClaimFailureTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>();auto* M=NewObject<UGameXXKMVPSubsystem>(GI);M->StartGame();
    M->GetMutableRuntimeState()=GameXXKPermanentPartyTestFixtures::MakeStartedState();
    auto& S=M->GetMutableRuntimeState();const auto* Node=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-01"));
    if(!FGameXXKMainStoryRules::StartNode(S,Node->Id))return false;
    for(int32 I=0;I<Node->Lines.Num();++I)if(!FGameXXKMainStoryRules::AdvanceDialogue(S))return false;
    auto* Story=NewObject<UGameXXKMainStorySubsystem>(GI);Story->SetMVPForTest(M);
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();
    auto* Root=Cast<UCanvasPanel>(Host->WidgetTree->RootWidget);
    auto* Panel=NewObject<UGameXXKMainStoryPanelWidget>(Host->WidgetTree);Root->AddChildToCanvas(Panel);
    Panel->SetContext(Story,TEXT("S00"));Panel->SelectNode(Node->Id);const auto Slate=Panel->TakeWidget();
    const int32 Before=S.PlayerGold;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
    Panel->HandleAction(4,Node->Id);
    TestEqual(TEXT("Failed persistence grants nothing"),S.PlayerGold,Before);
    TestNull(TEXT("Failed claim emits no flight"),Host->WidgetTree->FindWidget(TEXT("RewardFlightEffects")));
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    Panel->HandleAction(4,Node->Id);
    TestEqual(TEXT("Successful claim grants its exact reward once"),S.PlayerGold,Before+Node->Gold);
    auto* Flights=Cast<UGameXXKRewardFlightWidget>(Host->WidgetTree->FindWidget(TEXT("RewardFlightEffects")));
    if(!TestNotNull(TEXT("Successful claim emits reward feedback"),Flights))return false;
    Panel->HandleAction(4,Node->Id);
    TestEqual(TEXT("Repeated claim does not grant more gold"),S.PlayerGold,Before+Node->Gold);
    TestEqual(TEXT("Repeated claim does not play another burst"),Flights->GetRewardEventCountForTest(),1);
    return true;
}
#endif
