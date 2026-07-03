#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKMainMenuWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static void DeleteManualSaveSlots(int32 UserIndex)
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			UGameplayStatics::DeleteGameInSlot(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), UserIndex);
		}
	}

	struct FScopedManualSaveSlotBackup
	{
		explicit FScopedManualSaveSlotBackup(int32 InUserIndex)
			: UserIndex(InUserIndex)
		{
			const int32 SlotCount = UGameXXKMVPSubsystem::GetManualSaveSlotCount();
			Backups.SetNumZeroed(SlotCount);

			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
				USaveGame* ExistingSave = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)
					? UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)
					: nullptr;
				if (ExistingSave)
				{
					ExistingSave->AddToRoot();
					Backups[SlotIndex] = ExistingSave;
				}
			}

			DeleteManualSaveSlots(UserIndex);
		}

		~FScopedManualSaveSlotBackup()
		{
			const int32 SlotCount = UGameXXKMVPSubsystem::GetManualSaveSlotCount();
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
				UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
				if (Backups.IsValidIndex(SlotIndex) && Backups[SlotIndex])
				{
					UGameplayStatics::SaveGameToSlot(Backups[SlotIndex], SlotName, UserIndex);
					Backups[SlotIndex]->RemoveFromRoot();
				}
			}
		}

		int32 UserIndex = 0;
		TArray<USaveGame*> Backups;
	};

	static UGameXXKMVPSubsystem* CreateSeededSubsystem(UGameInstance* GameInstance, EGameXXKScreen Screen, int32 PlayerLevel)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		FGameXXKRuntimeState& RuntimeState = Subsystem->GetMutableRuntimeState();
		RuntimeState = UGameXXKMVPRules::CreateNewGame();
		RuntimeState.Screen = Screen;
		RuntimeState.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
		RuntimeState.PlayerLevel = PlayerLevel;
		return Subsystem;
	}

	static bool RowLabelContains(const FGameXXKMainMenuSaveSlotRow& Row, const TCHAR* ExpectedText)
	{
		return Row.Label.ToString().Contains(ExpectedText);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMainMenuPlayerFlowTest,
	"GameXXK.MVP.UI.MainMenuPlayerFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMainMenuPlayerFlowTest::RunTest(const FString& Parameters)
{
	const int32 UserIndex = 9103;
	FScopedManualSaveSlotBackup SlotBackup(UserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Slot1Subsystem = CreateSeededSubsystem(TestGameInstance, EGameXXKScreen::Town, 2);
	UGameXXKMVPSubsystem* Slot3Subsystem = CreateSeededSubsystem(TestGameInstance, EGameXXKScreen::DungeonMap, 3);
	TestTrue(TEXT("seed player-facing slot 1 as Qingshan town level 2"), Slot1Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), UserIndex));
	TestTrue(TEXT("seed player-facing slot 3 as Qingshan route map level 3"), Slot3Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(2), UserIndex));

	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMainMenuWidget* MainMenu = NewObject<UGameXXKMainMenuWidget>();
	MainMenu->SetMVPSubsystem(Subsystem);
	MainMenu->SetSaveSlotUserIndexForTest(UserIndex);

	TestEqual(TEXT("main menu starts on landing layer"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::Landing);
	TestEqual(TEXT("main menu is visible on main menu screen"), MainMenu->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("main menu is enabled on main menu screen"), MainMenu->GetIsEnabled());

	TestEqual(TEXT("landing exposes exactly four player actions"), MainMenu->BuildLandingActionsForTest().Num(), 4);
	TestTrue(TEXT("landing exposes New Game"), MainMenu->HasLandingActionForTest(FName(TEXT("NewGame")), true));
	TestTrue(TEXT("landing exposes Continue"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenContinue")), true));
	TestTrue(TEXT("landing exposes Options"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenOptions")), true));
	TestTrue(TEXT("landing exposes Quit"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenQuit")), true));
	TestFalse(TEXT("landing does not expose slot continue"), MainMenu->HasLandingActionForTest(FName(TEXT("ContinueSlot1")), true));
	TestFalse(TEXT("landing does not expose slot delete"), MainMenu->HasLandingActionForTest(FName(TEXT("DeleteSlot1")), true));
	TestFalse(TEXT("landing does not expose battle selection"), MainMenu->HasLandingActionForTest(FName(TEXT("SelectBattle")), true));

	TestTrue(TEXT("continue action opens continue modal"), MainMenu->OpenContinueModal());
	TestEqual(TEXT("continue modal is active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);

	const TArray<FGameXXKMainMenuSaveSlotRow> Rows = MainMenu->BuildSaveSlotRowsForTest();
	TestEqual(TEXT("continue modal shows five compact save rows"), Rows.Num(), UGameXXKMVPSubsystem::GetManualSaveSlotCount());
	if (Rows.Num() == UGameXXKMVPSubsystem::GetManualSaveSlotCount())
	{
		TestTrue(TEXT("slot 1 row is occupied"), Rows[0].bOccupied);
		TestTrue(TEXT("slot 1 can load"), Rows[0].bCanLoad);
		TestTrue(TEXT("slot 1 can delete"), Rows[0].bCanDelete);
		TestTrue(TEXT("slot 1 mentions Qingshan Town"), RowLabelContains(Rows[0], TEXT("Qingshan Town")));

		TestFalse(TEXT("slot 2 row is empty"), Rows[1].bOccupied);
		TestFalse(TEXT("slot 2 cannot load"), Rows[1].bCanLoad);
		TestFalse(TEXT("slot 2 cannot delete"), Rows[1].bCanDelete);
		TestTrue(TEXT("slot 2 mentions Empty"), RowLabelContains(Rows[1], TEXT("Empty")));

		TestTrue(TEXT("slot 3 row is occupied"), Rows[2].bOccupied);
		TestTrue(TEXT("slot 3 can load"), Rows[2].bCanLoad);
		TestTrue(TEXT("slot 3 can delete"), Rows[2].bCanDelete);
		TestTrue(TEXT("slot 3 mentions Route Map"), RowLabelContains(Rows[2], TEXT("Route Map")));
	}

	TestFalse(TEXT("continue rejects empty slot"), MainMenu->ContinueFromSlotIndex(1));
	TestTrue(TEXT("continue loads populated slot 1"), MainMenu->ContinueFromSlotIndex(0));
	TestEqual(TEXT("loaded slot restores town screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("loaded slot restores level 2"), Subsystem->GetRuntimeState().PlayerLevel, 2);
	TestEqual(TEXT("continue requests playable Qingshan town map"), MainMenu->GetLastRequestedTownMapForTest(), FName(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
	TestEqual(TEXT("continue hides main menu after load"), MainMenu->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("continue disables main menu after load"), MainMenu->GetIsEnabled());

	TestTrue(TEXT("reopen continue modal after load"), MainMenu->OpenContinueModal());
	TestFalse(TEXT("delete request rejects empty slot"), MainMenu->RequestDeleteSlot(1));
	TestTrue(TEXT("delete request accepts populated slot"), MainMenu->RequestDeleteSlot(0));
	TestEqual(TEXT("delete confirmation modal is active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::DeleteConfirmModal);
	TestEqual(TEXT("delete confirmation tracks pending slot 1"), MainMenu->GetPendingDeleteSlotIndexForTest(), 0);
	TestTrue(TEXT("cancel delete returns to continue modal"), MainMenu->CancelDeleteSlot());
	TestEqual(TEXT("cancel delete leaves continue modal active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);
	TestTrue(TEXT("cancel delete preserves save"), UGameplayStatics::DoesSaveGameExist(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), UserIndex));
	TestTrue(TEXT("request delete populated slot again"), MainMenu->RequestDeleteSlot(0));
	TestTrue(TEXT("confirm delete succeeds"), MainMenu->ConfirmDeleteSlot());
	TestEqual(TEXT("confirm delete returns to continue modal"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);
	TestFalse(TEXT("confirm delete removes save"), UGameplayStatics::DoesSaveGameExist(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), UserIndex));

	TestTrue(TEXT("options opens unavailable modal"), MainMenu->OpenOptionsModal());
	TestEqual(TEXT("options unavailable modal is active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::OptionsModal);
	TestTrue(TEXT("closing options modal returns to landing"), MainMenu->CloseActiveModal());
	TestEqual(TEXT("options close returns to landing"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::Landing);
	TestTrue(TEXT("quit opens unavailable modal"), MainMenu->OpenQuitUnavailableModal());
	TestEqual(TEXT("quit unavailable modal is active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::QuitUnavailableModal);

	TestTrue(TEXT("main menu start creates new game and opens Qingshan town"), MainMenu->StartGame());
	TestEqual(TEXT("town screen after player-facing main menu start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("Qingshan selected after player-facing main menu start"), Subsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());
	TestEqual(TEXT("new game requests playable Qingshan town map"), MainMenu->GetLastRequestedTownMapForTest(), FName(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
	TestEqual(TEXT("new game hides main menu after entering town"), MainMenu->GetVisibility(), ESlateVisibility::Collapsed);
	TestFalse(TEXT("new game disables main menu after entering town"), MainMenu->GetIsEnabled());

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	TestNotNull(TEXT("HUD creates player main menu widget"), HUD->CreateMainMenuWidgetForTest());
	TestTrue(TEXT("HUD has main menu widget"), HUD->HasMainMenuWidget());
	TestFalse(TEXT("HUD debug shell is disabled by default"), HUD->IsDebugPlayableShellEnabledForTest());
	HUD->SetDebugPlayableShellEnabledForTest(true);
	TestTrue(TEXT("HUD debug shell can be enabled"), HUD->IsDebugPlayableShellEnabledForTest());
	TestNotNull(TEXT("HUD can still create playable root widget"), HUD->CreatePlayableRootWidgetForTest());

	return true;
}

#endif
