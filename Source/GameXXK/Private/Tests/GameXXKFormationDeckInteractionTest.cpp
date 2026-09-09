#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDeckDensityDraftTest,
    "GameXXK.MVP.UI.DeckInteraction.DensityExpansionPreservesDraft",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDeckDensityDraftTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    Subsystem->EnsureQingshanTownRuntimeForDirectMap();
    Subsystem->PrepareCompanionRosterForTown();
    auto* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
    Inventory->SetMVPSubsystem(Subsystem);
    Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
    Inventory->TakeWidget();
    Inventory->OpenFreeInventoryForTest();
    Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck);
    const auto Original = Subsystem->GetHeroCardLoadout();
    auto Button = [Inventory](const TCHAR* Name) { return Cast<UButton>(Inventory->WidgetTree->FindWidget(Name)); };
    if (!TestNotNull(TEXT("deck offers density switch"), Button(TEXT("InventoryDeckDensityButton")))
        || !TestNotNull(TEXT("deck offers full-page selection"), Button(TEXT("InventoryDeckExpandButton")))) return false;
    TestTrue(TEXT("remove one card into a draft"), Inventory->ToggleHeroDeckCardForTest(Original[0]));
    const auto Draft = Inventory->GetPendingHeroDeckIdsForTest();
    Button(TEXT("InventoryDeckDensityButton"))->OnClicked.Broadcast();
    auto* Grid = Cast<UUniformGridPanel>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckGrid")));
    const auto* Fourth = Grid ? Cast<UUniformGridSlot>(Grid->GetChildAt(3)->Slot) : nullptr;
    TestTrue(TEXT("compact density places four cards across"), Fourth && Fourth->GetRow() == 0 && Fourth->GetColumn() == 3);
    Button(TEXT("InventoryDeckExpandButton"))->OnClicked.Broadcast();
    if (!TestNotNull(TEXT("expanded page offers return"), Button(TEXT("InventoryDeckCollapseButton")))) return false;
    Button(TEXT("InventoryDeckCollapseButton"))->OnClicked.Broadcast();
    TestEqual(TEXT("reflow and expansion retain the uncommitted selection"), Inventory->GetPendingHeroDeckIdsForTest(), Draft);
    TestEqual(TEXT("presentation changes do not write the actual deck"), Subsystem->GetHeroCardLoadout(), Original);
    for (const auto Card : Draft) Inventory->ToggleHeroDeckCardForTest(Card);
    TestEqual(TEXT("an intentionally empty draft stays empty"), Inventory->GetPendingHeroDeckIdsForTest().Num(), 0);
    const auto Session = Inventory->CaptureEmbeddedSessionState();
    Inventory->RestoreEmbeddedSessionState(Session);
    TestEqual(TEXT("restoring an empty draft never silently reloads the saved deck"), Inventory->GetPendingHeroDeckIdsForTest().Num(), 0);
    TestFalse(TEXT("incomplete selection cannot apply"), Inventory->ApplyHeroDeckForTest());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFormationDeckEntryTest,
    "GameXXK.MVP.UI.DeckInteraction.FormationUsesSharedEditor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationDeckEntryTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    Subsystem->EnsureQingshanTownRuntimeForDirectMap();
    Subsystem->PrepareCompanionRosterForTown();
    auto* Workbench = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    Workbench->SetMVPSubsystem(Subsystem);
    Workbench->TakeWidget();
    Workbench->OpenBackpack();
    const auto Original = Subsystem->GetHeroCardLoadout();
    Workbench->HandleActionClicked(1);
    auto* Edit = Cast<UButton>(Workbench->WidgetTree->FindWidget(TEXT("FormationEditDeck_0")));
    if (!TestNotNull(TEXT("formation exposes the hero's deck editor"), Edit)) return false;
    Edit->OnClicked.Broadcast();
    Workbench->TickForTest(0.0f);
    auto* Inventory = Cast<UGameXXKInventoryWindowWidget>(Workbench->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")));
    if (!TestNotNull(TEXT("formation reuses the real inventory editor"), Inventory)) return false;
    TestEqual(TEXT("formation enters the deck tab"), Inventory->GetActiveCharacterBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
    Inventory->ToggleHeroDeckCardForTest(Original[0]);
    Workbench->HandleActionClicked(19);
    TestEqual(TEXT("opening HUD settings keeps the deck draft"),Workbench->GetEmbeddedPendingDeckIdsForTest().Num(),Original.Num()-1);
    Workbench->HandleActionClicked(19);
    TestEqual(TEXT("closing HUD settings keeps the deck tab"),Workbench->GetEmbeddedBackpackTabForTest(),EGameXXKCharacterBackpackTab::Deck);
    auto* Back = Cast<UButton>(Workbench->WidgetTree->FindWidget(TEXT("FormationDeckBack")));
    if (!TestNotNull(TEXT("deck returns directly to formation"), Back)) return false;
    Back->OnClicked.Broadcast();
    Workbench->TickForTest(0.0f);
    TestEqual(TEXT("return keeps formation context"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
    Edit = Cast<UButton>(Workbench->WidgetTree->FindWidget(TEXT("FormationEditDeck_0")));
    if (!Edit) return false;
    Edit->OnClicked.Broadcast();
    Workbench->TickForTest(0.0f);
    TestEqual(TEXT("reopening from formation preserves the seven-card draft"), Workbench->GetEmbeddedPendingDeckIdsForTest().Num(), Original.Num() - 1);
    TestEqual(TEXT("opening and returning do not mutate the saved deck"), Subsystem->GetHeroCardLoadout(), Original);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSorcererIncompleteDeckBoundaryTest,
    "GameXXK.MVP.UI.DeckInteraction.SorcererIncompleteDraftCannotEnterBattle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererIncompleteDeckBoundaryTest::RunTest(const FString& Parameters)
{
    auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if (!TestTrue(TEXT("start the current game"), Subsystem->StartGame())) return false;
    const auto* Mage = Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
        [](const auto& C) { return C.Role == EGameXXKCharacterRole::Sorcerer; });
    if (!TestNotNull(TEXT("current roster contains the mage"), Mage)) return false;
    const FName MageId = Mage->InstanceId;
    const TArray<FName> Original = Mage->SelectedCardIds;
    if (!TestEqual(TEXT("mage starts with exactly five cards"), Original.Num(), 5)) return false;
    auto MakeEditor = [Subsystem]()
    {
        auto* Editor = NewObject<UGameXXKInventoryWindowWidget>();
        Editor->SetMVPSubsystem(Subsystem); Editor->ConfigureDesktopTrainingEmbeddedMode(true);
        Editor->TakeWidget(); Editor->OpenFreeInventoryForTest();
        return Editor;
    };
    auto* Editor = MakeEditor();
    Editor->ConfigureDesktopTrainingCharacter(MageId);
    Editor->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck);
    TestTrue(TEXT("remove one mage card into the draft"), Editor->ToggleHeroDeckCardForTest(Original[0]));
    TestEqual(TEXT("the draft contains four cards"), Editor->GetPendingHeroDeckIdsForTest().Num(), 4);
    auto* Apply = Cast<UButton>(Editor->WidgetTree->FindWidget(TEXT("InventoryApplyHeroDeckButton")));
    TestTrue(TEXT("four-card draft disables the real Apply button"), Apply && !Apply->GetIsEnabled());
    TestFalse(TEXT("direct Apply also rejects four cards"), Editor->ApplyHeroDeckForTest());
    const auto FourCardSession = Editor->CaptureEmbeddedSessionState();
    Editor->ConfigureDesktopTrainingCharacter(TEXT("Player"));
    auto* Reopened = MakeEditor();
    Reopened->RestoreEmbeddedSessionState(FourCardSession);
    TestEqual(TEXT("return and reconstructed editor preserve the four-card draft"), Reopened->GetPendingHeroDeckIdsForTest().Num(), 4);
    TestFalse(TEXT("reopening cannot commit the incomplete draft"), Reopened->ApplyHeroDeckForTest());
    for (const FName CardId : FourCardSession.PendingDeckIds) Reopened->ToggleHeroDeckCardForTest(CardId);
    const auto EmptySession = Reopened->CaptureEmbeddedSessionState();
    Reopened->RestoreEmbeddedSessionState(EmptySession);
    TestEqual(TEXT("an empty draft stays empty"), Reopened->GetPendingHeroDeckIdsForTest().Num(), 0);
    TestFalse(TEXT("empty draft cannot apply"), Reopened->ApplyHeroDeckForTest());
    TArray<FName> Four = Original; Four.Pop();
    TArray<FName> Duplicate = Original; Duplicate[4] = Duplicate[0];
    for (const TArray<FName>& Invalid : {TArray<FName>(), Four, Duplicate})
        TestFalse(TEXT("facade rejects missing and duplicate selections"), Subsystem->SetPermanentCompanionCardLoadout(MageId, Invalid));
    FGameXXKPermanentCompanion ReadBack;
    TestTrue(TEXT("mage remains readable"), Subsystem->TryGetPermanentCompanionView(MageId, ReadBack));
    TestEqual(TEXT("all rejected edits retain the original valid five"), ReadBack.SelectedCardIds, Original);
    TestTrue(TEXT("select mage through the normal party facade"), Subsystem->SetActivePermanentCompanion(MageId));
    TestTrue(TEXT("the normal desktop challenge accepts the saved five, ignoring the UI draft"), Subsystem->StartTrainingChallenge(TEXT("Training.Normal.1-1")));
    const FGameXXKRuntimeState Ready = Subsystem->GetRuntimeState();
    const auto* BattleNode = Ready.RouteMapNodes.FindByPredicate([&Ready](const auto& Node)
    {
        return Ready.ReachableRouteNodeIds.Contains(Node.NodeId) && Node.NodeKind == EGameXXKNodeKind::Battle;
    });
    if (!TestNotNull(TEXT("the challenge exposes a reachable combat node"), BattleNode)) return false;
    const int32 BattleNodeId = BattleNode->NodeId;
    if (!TestTrue(TEXT("valid five-card battle starts through the real route entry"),
        Subsystem->SelectRouteNodeById(BattleNodeId))) return false;
    TestTrue(TEXT("the legal selection actually creates a card battle"), Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);
    for (const TArray<FName>& Invalid : {TArray<FName>(), Four, Duplicate})
    {
        FGameXXKRuntimeState Candidate = Ready;
        Candidate.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
            [MageId](const auto& C) { return C.InstanceId == MageId; })->SelectedCardIds = Invalid;
        Subsystem->GetMutableRuntimeState() = Candidate;
        const bool bEntered = Subsystem->SelectRouteNodeById(BattleNodeId);
        if (bEntered)
        {
            const auto& Deck = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck;
            TSet<FName> MageCards; int32 Count = 0;
            for (const auto* Zone : {&Deck.Hand, &Deck.DrawPile, &Deck.DiscardPile, &Deck.ExhaustPile, &Deck.PendingAutomaticHandCards})
                for (const auto& Card : *Zone) if (Card.OwnerUnitId == MageId && !Card.bTemporary)
                { ++Count; MageCards.Add(Card.CardId); }
            TestEqual(TEXT("any accepted repair must still carry exactly five mage instances"), Count, 5);
            TestEqual(TEXT("any accepted repair must carry five distinct mage cards"), MageCards.Num(), 5);
        }
        else TestFalse(TEXT("rejected selection never produces an active battle"), Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);
        AddInfo(FString::Printf(TEXT("forged selection count=%d: %s"), Invalid.Num(), bEntered ? TEXT("normalized to a valid five-card battle") : TEXT("rejected before battle")));
    }
    return true;
}
#endif
