#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopSettingsCentralPageTest,
    "GameXXK.DesktopTraining.Workbench.SettingsReplacesCentralPage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopSettingsCentralPageTest::RunTest(const FString&)
{
    const FString PreviousLanguage = GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT { GameXXKLocalization::SetLanguage(PreviousLanguage, false); };
    auto* MVP = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    auto* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    if (!TestTrue(TEXT("Independent fixture starts"), MVP->StartGame())) return false;
    Widget->SetMVPSubsystem(MVP); Widget->ConstructForTest();
    Widget->OpenWorkbench(); Widget->OpenBackpack();
    // Settings is a temporary view: it must retain the central page behind it,
    // while detaching that page's controls so there are no invisible hit targets.
    for (const int32 PageAction : {-1, 2, 1})
    {
        if (PageAction >= 0) { Widget->HandleDesktopActionForTest(PageAction); Widget->TickForTest(0); }
        const auto Page = Widget->GetActiveCenterPageForTest();
        const auto Draft = Widget->GetEmbeddedPendingDeckIdsForTest();
        for (const TCHAR* Language : {TEXT("zh-Hans"), TEXT("en")})
        {
            GameXXKLocalization::SetLanguage(Language, false);
            Widget->HandleDesktopActionForTest(19); Widget->TickForTest(0);
            TestTrue(TEXT("Settings is open from every tested central page"), Widget->IsSettingsPanelOpenForTest());
            TestEqual(TEXT("Previous central page remains selected"), Widget->GetActiveCenterPageForTest(), Page);
            TestEqual(TEXT("Underlying inventory controls are detached"), Widget->GetEmbeddedInventoryWidgetCountForTest(), 0);
            auto* Panel = Widget->WidgetTree->FindWidget(TEXT("DesktopHudSettingsPanel"));
            if (!TestNotNull(TEXT("Settings paper exists"), Panel)) return false;
            auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot);
            if (!TestNotNull(TEXT("Settings uses the shared canvas"), Slot)) return false;
            const auto Rect = GameXXKDesktopTrainingLayout::GetContentRect();
            TestTrue(TEXT("Same backpack position"), Slot->GetPosition().Equals(FVector2D(Rect.X, Rect.Y)));
            TestTrue(TEXT("Same backpack size"), Slot->GetSize().Equals(FVector2D(Rect.Z, Rect.W)));
            TestTrue(TEXT("No separate inverse HUD enlargement"), Panel->GetRenderTransform().Scale.Equals(FVector2D(1, 1)));
            auto* Title = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("HudSettingsTitle")));
            if (!TestNotNull(TEXT("Settings title exists"), Title)) return false;
            TestEqual(TEXT("Title follows the chosen language"), Title->GetText().ToString(),
                GameXXKLocalization::IsEnglish() ? FString(TEXT("Settings")) : FString(TEXT("设置")));
            Widget->HandleDesktopActionForTest(663); Widget->TickForTest(0);
            TestFalse(TEXT("Close exits settings only"), Widget->IsSettingsPanelOpenForTest());
            TestEqual(TEXT("Close returns to the original central page"), Widget->GetActiveCenterPageForTest(), Page);
            TestTrue(TEXT("Close retains the unconfirmed deck"), Widget->GetEmbeddedPendingDeckIdsForTest() == Draft);
            TestTrue(TEXT("The workbench stays expanded"), Widget->IsBackpackExpandedForTest());
        }
    }
    return true;
}
#endif
