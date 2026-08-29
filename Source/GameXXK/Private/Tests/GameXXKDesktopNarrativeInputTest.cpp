#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "InputKeyEventArgs.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKTownHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKDesktopNarrativeInputTestPrivate
{
	struct FWorkbenchFingerprint
	{
		bool bVisible = false;
		bool bExpanded = false;
		EGameXXKDesktopTrainingLeftPanel LeftPanel = EGameXXKDesktopTrainingLeftPanel::None;
		EGameXXKDesktopTrainingCenterPage CenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
		EGameXXKDesktopTrainingNav ActiveNav = EGameXXKDesktopTrainingNav::None;

		bool operator==(const FWorkbenchFingerprint& Other) const
		{
			return bVisible == Other.bVisible
				&& bExpanded == Other.bExpanded
				&& LeftPanel == Other.LeftPanel
				&& CenterPage == Other.CenterPage
				&& ActiveNav == Other.ActiveNav;
		}
	};

	struct FInputRoutingFingerprint
	{
		FWorkbenchFingerprint Workbench;
		EGameXXKInventoryWindowMode InventoryMode = EGameXXKInventoryWindowMode::None;
		bool bQuestDialogOpen = false;
		bool bTaskPanelOpen = false;
		bool bCompanionRosterOpen = false;
		bool bMetaShopOpen = false;

		bool operator==(const FInputRoutingFingerprint& Other) const
		{
			return Workbench == Other.Workbench
				&& InventoryMode == Other.InventoryMode
				&& bQuestDialogOpen == Other.bQuestDialogOpen
				&& bTaskPanelOpen == Other.bTaskPanelOpen
				&& bCompanionRosterOpen == Other.bCompanionRosterOpen
				&& bMetaShopOpen == Other.bMetaShopOpen;
		}
	};

	FWorkbenchFingerprint CaptureFingerprint(const UGameXXKDesktopTrainingWorkbenchWidget& Workbench)
	{
		FWorkbenchFingerprint Result;
		Result.bVisible = Workbench.IsWorkbenchVisibleForTest();
		Result.bExpanded = Workbench.IsBackpackExpandedForTest();
		Result.LeftPanel = Workbench.GetLeftPanelForTest();
		Result.CenterPage = Workbench.GetActiveCenterPageForTest();
		Result.ActiveNav = Workbench.GetActiveNavForTest();
		return Result;
	}

	FInputRoutingFingerprint CaptureInputFingerprint(
		const AGameXXKMVPPlayerController& Controller,
		const UGameXXKDesktopTrainingWorkbenchWidget& Workbench)
	{
		FInputRoutingFingerprint Result;
		Result.Workbench = CaptureFingerprint(Workbench);
		Result.InventoryMode = Controller.GetInventoryWindowWidgetForTest()
			? Controller.GetInventoryWindowWidgetForTest()->GetWindowModeForTest()
			: EGameXXKInventoryWindowMode::None;
		Result.bQuestDialogOpen = Controller.IsQuestDialogOpenForTest();
		Result.bTaskPanelOpen = Controller.IsTaskPanelOpenForTest();
		Result.bCompanionRosterOpen = Controller.IsCompanionRosterOpenForTest();
		Result.bMetaShopOpen = Controller.IsMetaShopOpenForTest();
		return Result;
	}

	bool Send(AGameXXKMVPPlayerController& Controller, const FKey& Key, const EInputEvent Event)
	{
		return Controller.InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, 1.0f));
	}

	bool Press(AGameXXKMVPPlayerController& Controller, const FKey& Key)
	{
		return Send(Controller, Key, IE_Pressed);
	}

	UButton* FindButton(UGameXXKTownHudWidget& TownHud, const FName Name)
	{
		return TownHud.WidgetTree ? Cast<UButton>(TownHud.WidgetTree->FindWidget(Name)) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeInputRoutingTest,
	"GameXXK.DesktopNarrative.InputRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeInputRoutingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopNarrativeInputTestPrivate;

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	if (!TestTrue(TEXT("shortcut fixture starts in Town"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	if (!TestNotNull(TEXT("shortcut fixture creates a controller"), Controller))
	{
		return false;
	}
	Controller->SetMVPSubsystemForTest(Subsystem);
	AddExpectedError(
		TEXT("[BattleVisual] RefreshUnitVisuals SKIPPED"),
		EAutomationExpectedErrorFlags::Contains,
		1,
		false);
	if (!TestTrue(TEXT("shortcut fixture creates legacy and Town HUD widgets"), Controller->EnsurePlayerFlowWidgetsForTest()))
	{
		return false;
	}
	Controller->SetDesktopTrainingWorkbenchEnabledForTest(true);
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench = Controller->GetDesktopTrainingWorkbenchWidgetForTest();
	UGameXXKInventoryWindowWidget* InventoryWindow = Controller->GetInventoryWindowWidgetForTest();
	if (!TestNotNull(TEXT("shortcut fixture owns the Workbench"), Workbench)
		|| !TestNotNull(TEXT("shortcut fixture owns independent Inventory"), InventoryWindow)
		|| !TestNotNull(TEXT("shortcut fixture owns legacy TaskPanel"), Controller->GetTaskPanelWidgetForTest()))
	{
		return false;
	}

	TestFalse(TEXT("Workbench starts collapsed before physical Tab sequence"), Workbench->IsBackpackExpandedForTest());
	TestTrue(TEXT("first physical Tab expands Workbench"), Press(*Controller, EKeys::Tab));
	TestTrue(TEXT("first physical Tab keeps the canonical Workbench pointer"),
		Controller->GetDesktopTrainingWorkbenchWidgetForTest() == Workbench);
	TestTrue(TEXT("first physical Tab expands the canonical Workbench"), Workbench->IsBackpackExpandedForTest());
	TestTrue(TEXT("expanded Town Workbench owns movement lock"), Controller->IsMoveInputIgnored());
	TestTrue(TEXT("second physical Tab collapses Workbench"), Press(*Controller, EKeys::Tab));
	TestFalse(TEXT("second physical Tab collapses the canonical Workbench"), Workbench->IsBackpackExpandedForTest());
	TestFalse(TEXT("second physical Tab releases movement lock"), Controller->IsMoveInputIgnored());
	TestTrue(TEXT("third physical Tab reopens Workbench"), Press(*Controller, EKeys::Tab));
	TestTrue(TEXT("third physical Tab reuses the canonical Workbench pointer"),
		Controller->GetDesktopTrainingWorkbenchWidgetForTest() == Workbench);
	TestTrue(TEXT("third physical Tab expands Workbench again"), Workbench->IsBackpackExpandedForTest());

	TestTrue(TEXT("Q opens StoryTasks"), Press(*Controller, EKeys::Q));
	TestTrue(TEXT("Q expands the Workbench"), Workbench->IsBackpackExpandedForTest());
	TestEqual(TEXT("Q selects the StoryTasks left drawer"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
	TestTrue(TEXT("repeated Q is handled"), Press(*Controller, EKeys::Q));
	TestEqual(TEXT("repeated Q keeps StoryTasks open"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
	TestTrue(TEXT("Escape closes the actual StoryTasks drawer path"), Press(*Controller, EKeys::Escape));
	TestEqual(TEXT("Escape clears only the StoryTasks drawer"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestTrue(TEXT("Escape keeps the Workbench expanded"), Workbench->IsBackpackExpandedForTest());

	TestTrue(TEXT("Q reopens StoryTasks before I"), Press(*Controller, EKeys::Q));
	TestTrue(TEXT("I redirects from StoryTasks to Backpack"), Press(*Controller, EKeys::I));
	TestEqual(TEXT("I closes StoryTasks"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestEqual(TEXT("I selects Backpack"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("I leaves embedded Backpack nav neutral"), Workbench->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::None);
	TestEqual(TEXT("I never opens independent Inventory"), Controller->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestTrue(TEXT("repeated I is idempotently handled"), Press(*Controller, EKeys::I));
	TestTrue(TEXT("repeated I keeps the Workbench expanded"), Workbench->IsBackpackExpandedForTest());

	TestTrue(TEXT("Q reopens StoryTasks before C"), Press(*Controller, EKeys::Q));
	TestTrue(TEXT("C redirects from StoryTasks to Formation"), Press(*Controller, EKeys::C));
	TestEqual(TEXT("C closes StoryTasks"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestEqual(TEXT("C selects Formation center page"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
	TestEqual(TEXT("C selects Formation nav"), Workbench->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Formation);
	TestFalse(TEXT("C never opens independent CompanionRoster"), Controller->IsCompanionRosterOpenForTest());

	TestTrue(TEXT("legacy QuestDialog opens before physical I"), Controller->OpenQuestDialogPreviewForTest());
	TestTrue(TEXT("physical I replaces QuestDialog with embedded Backpack"), Press(*Controller, EKeys::I));
	TestFalse(TEXT("physical I closes legacy QuestDialog"), Controller->IsQuestDialogOpenForTest());
	TestEqual(TEXT("physical I selects Backpack after QuestDialog"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	if (Controller->IsQuestDialogOpenForTest()) Controller->CloseQuestDialog();
	TestTrue(TEXT("legacy QuestDialog opens before physical Q"), Controller->OpenQuestDialogPreviewForTest());
	TestTrue(TEXT("physical Q replaces QuestDialog with StoryTasks"), Press(*Controller, EKeys::Q));
	TestFalse(TEXT("physical Q closes legacy QuestDialog"), Controller->IsQuestDialogOpenForTest());
	TestEqual(TEXT("physical Q opens StoryTasks after QuestDialog"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
	if (Controller->IsQuestDialogOpenForTest()) Controller->CloseQuestDialog();
	TestTrue(TEXT("legacy QuestDialog opens before physical C"), Controller->OpenQuestDialogPreviewForTest());
	TestTrue(TEXT("physical C replaces QuestDialog with Formation"), Press(*Controller, EKeys::C));
	TestFalse(TEXT("physical C closes legacy QuestDialog"), Controller->IsQuestDialogOpenForTest());
	TestEqual(TEXT("physical C opens Formation after QuestDialog"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
	if (Controller->IsQuestDialogOpenForTest()) Controller->CloseQuestDialog();
	const bool bExpandedBeforeQuestTab = Workbench->IsBackpackExpandedForTest();
	TestTrue(TEXT("legacy QuestDialog opens before physical Tab"), Controller->OpenQuestDialogPreviewForTest());
	TestTrue(TEXT("physical Tab replaces QuestDialog and toggles Workbench"), Press(*Controller, EKeys::Tab));
	TestFalse(TEXT("physical Tab closes legacy QuestDialog"), Controller->IsQuestDialogOpenForTest());
	TestEqual(TEXT("physical Tab toggles expansion after QuestDialog"), Workbench->IsBackpackExpandedForTest(), !bExpandedBeforeQuestTab);
	if (Controller->IsQuestDialogOpenForTest()) Controller->CloseQuestDialog();

	TestTrue(TEXT("legacy TaskPanel opens before physical I"), Controller->OpenTaskPanel());
	TestTrue(TEXT("physical I replaces TaskPanel with embedded Backpack"), Press(*Controller, EKeys::I));
	TestFalse(TEXT("physical I closes legacy TaskPanel"), Controller->IsTaskPanelOpenForTest());
	TestEqual(TEXT("physical I selects Backpack after TaskPanel"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	if (Controller->IsTaskPanelOpenForTest()) Controller->CloseTaskPanel();
	TestTrue(TEXT("legacy TaskPanel opens before physical Q"), Controller->OpenTaskPanel());
	TestTrue(TEXT("physical Q replaces TaskPanel with StoryTasks"), Press(*Controller, EKeys::Q));
	TestFalse(TEXT("physical Q closes legacy TaskPanel"), Controller->IsTaskPanelOpenForTest());
	TestEqual(TEXT("physical Q opens StoryTasks after TaskPanel"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
	if (Controller->IsTaskPanelOpenForTest()) Controller->CloseTaskPanel();
	TestTrue(TEXT("legacy TaskPanel opens before physical C"), Controller->OpenTaskPanel());
	TestTrue(TEXT("physical C replaces TaskPanel with Formation"), Press(*Controller, EKeys::C));
	TestFalse(TEXT("physical C closes legacy TaskPanel"), Controller->IsTaskPanelOpenForTest());
	TestEqual(TEXT("physical C opens Formation after TaskPanel"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
	if (Controller->IsTaskPanelOpenForTest()) Controller->CloseTaskPanel();
	const bool bExpandedBeforeTaskTab = Workbench->IsBackpackExpandedForTest();
	TestTrue(TEXT("legacy TaskPanel opens before physical Tab"), Controller->OpenTaskPanel());
	TestTrue(TEXT("physical Tab replaces TaskPanel and toggles Workbench"), Press(*Controller, EKeys::Tab));
	TestFalse(TEXT("physical Tab closes legacy TaskPanel"), Controller->IsTaskPanelOpenForTest());
	TestEqual(TEXT("physical Tab toggles expansion after TaskPanel"), Workbench->IsBackpackExpandedForTest(), !bExpandedBeforeTaskTab);
	if (Controller->IsTaskPanelOpenForTest()) Controller->CloseTaskPanel();
	TestTrue(TEXT("legacy TaskPanel opens before physical F"), Controller->OpenTaskPanel());
	const FWorkbenchFingerprint BeforeTaskPanelF = CaptureFingerprint(*Workbench);
	TestTrue(TEXT("legacy TaskPanel consumes physical F"), Press(*Controller, EKeys::F));
	TestTrue(TEXT("physical F keeps legacy TaskPanel open"), Controller->IsTaskPanelOpenForTest());
	TestTrue(TEXT("physical F does not route Workbench"), CaptureFingerprint(*Workbench) == BeforeTaskPanelF);
	TestTrue(TEXT("legacy TaskPanel closes explicitly after F"), Controller->CloseTaskPanel());

	TestTrue(TEXT("legacy free Inventory can be open before a shortcut"), Controller->OpenFreeInventoryWindow());
	TestTrue(TEXT("physical I closes legacy Inventory and routes Backpack"), Press(*Controller, EKeys::I));
	TestEqual(TEXT("legacy Inventory is closed after shortcut"), Controller->GetInventoryWindowWidgetForTest()->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);
	TestTrue(TEXT("legacy CompanionRoster can be open before a shortcut"), Controller->OpenCompanionRoster());
	TestTrue(TEXT("physical Q closes legacy CompanionRoster and routes StoryTasks"), Press(*Controller, EKeys::Q));
	TestFalse(TEXT("legacy CompanionRoster is closed after shortcut"), Controller->IsCompanionRosterOpenForTest());
	TestTrue(TEXT("legacy MetaShop can be open before a shortcut"), Controller->OpenMetaShopWindow());
	TestTrue(TEXT("physical I closes legacy MetaShop and routes Backpack"), Press(*Controller, EKeys::I));
	TestFalse(TEXT("legacy MetaShop is closed after shortcut"), Controller->IsMetaShopOpenForTest());
	TestFalse(TEXT("legacy MetaShop releases its own modal lock"), Controller->IsMetaShopInputLockedForTest());
	TestTrue(TEXT("expanded Town Workbench owns the expected move lock"), Controller->IsMoveInputIgnored());
	TestTrue(TEXT("physical Tab collapses after legacy modal replacement"), Press(*Controller, EKeys::Tab));
	TestFalse(TEXT("collapsing Workbench proves legacy modals left no stale move lock"), Controller->IsMoveInputIgnored());
	TestTrue(TEXT("physical Tab re-expands Workbench for TownHud parity"), Press(*Controller, EKeys::Tab));

	UGameXXKTownHudWidget* TownHud = Controller->GetTownHudWidgetForTest();
	if (!TestNotNull(TEXT("shortcut fixture owns TownHud"), TownHud))
	{
		return false;
	}
	UButton* InventoryButton = FindButton(*TownHud, TEXT("TownHudInventory"));
	UButton* TaskButton = FindButton(*TownHud, TEXT("TownHudTask"));
	UButton* CompanionButton = FindButton(*TownHud, TEXT("TownHudCompanion"));
	TestNotNull(TEXT("TownHud exposes Inventory button"), InventoryButton);
	TestNotNull(TEXT("TownHud exposes Task button"), TaskButton);
	TestNotNull(TEXT("TownHud exposes Companion button"), CompanionButton);
	if (InventoryButton) InventoryButton->OnClicked.Broadcast();
	TestEqual(TEXT("TownHud Inventory matches I center page"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestEqual(TEXT("TownHud Inventory closes StoryTasks"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	if (TaskButton) TaskButton->OnClicked.Broadcast();
	TestEqual(TEXT("TownHud Task matches Q drawer"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
	if (CompanionButton) CompanionButton->OnClicked.Broadcast();
	TestEqual(TEXT("TownHud Companion matches C Formation page"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
	TestEqual(TEXT("TownHud Companion matches C Formation nav"), Workbench->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Formation);
	TestFalse(TEXT("TownHud Companion never opens independent CompanionRoster"), Controller->IsCompanionRosterOpenForTest());

	const FInputRoutingFingerprint BeforeNarrative = CaptureInputFingerprint(*Controller, *Workbench);
	Subsystem->GetMutableRuntimeState().NarrativeSequenceSession.bActive = true;
	for (const EInputEvent BlockedEvent : {IE_Pressed, IE_Released, IE_Repeat})
	{
		for (const FKey& BlockedKey : {EKeys::Tab, EKeys::I, EKeys::Q, EKeys::C, EKeys::F})
		{
			TestTrue(
				FString::Printf(TEXT("active narrative consumes %s event %d"), *BlockedKey.ToString(), static_cast<int32>(BlockedEvent)),
				Send(*Controller, BlockedKey, BlockedEvent));
			TestTrue(
				FString::Printf(TEXT("active narrative leaves UI fingerprint unchanged for %s event %d"), *BlockedKey.ToString(), static_cast<int32>(BlockedEvent)),
				CaptureInputFingerprint(*Controller, *Workbench) == BeforeNarrative);
		}
	}
	TestFalse(TEXT("narrative C cannot fall through to CompanionRoster"), Controller->IsCompanionRosterOpenForTest());
	Subsystem->GetMutableRuntimeState().NarrativeSequenceSession.bActive = false;
	TestTrue(TEXT("unlocked C redirects to Workbench Formation"), Press(*Controller, EKeys::C));
	TestEqual(TEXT("unlocked C reaches Formation"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);

	TestTrue(TEXT("fixture can collapse Workbench before non-Town check"), Controller->CloseDesktopTrainingWorkbench());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::MainMenu;
	TestFalse(TEXT("semantic shortcut rejects non-Town state"), Controller->RouteWorkbenchShortcut(EGameXXKWorkbenchShortcut::Backpack));
	TestFalse(TEXT("non-Town I falls through"), Press(*Controller, EKeys::I));
	TestFalse(TEXT("non-Town shortcut does not reopen Workbench"), Workbench->IsWorkbenchVisibleForTest());
	return true;
}

#endif
