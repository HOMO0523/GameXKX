#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
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
#endif
