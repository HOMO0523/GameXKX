#include "GameXXKMVPRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKMainMenuWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static void DeleteManualSaveSlots(int32 UserIndex)
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
			UGameplayStatics::DeleteGameInSlot(SlotName + TEXT(".PreV7Backup"), UserIndex);
		}
	}

	struct FScopedManualSaveSlotBackup
	{
		explicit FScopedManualSaveSlotBackup(int32 InUserIndex)
			: UserIndex(InUserIndex)
		{
			const int32 SlotCount = UGameXXKMVPSubsystem::GetManualSaveSlotCount();
			Backups.SetNumZeroed(SlotCount);
			PreV7Backups.SetNumZeroed(SlotCount);

			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
				if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
				{
					USaveGame* ExistingSave = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
					if (!ExistingSave)
					{
						return;
					}
					ExistingSave->AddToRoot();
					Backups[SlotIndex] = ExistingSave;
				}

				const FString PreV7BackupSlotName = SlotName + TEXT(".PreV7Backup");
				if (UGameplayStatics::DoesSaveGameExist(PreV7BackupSlotName, UserIndex))
				{
					USaveGame* ExistingPreV7Backup = UGameplayStatics::LoadGameFromSlot(PreV7BackupSlotName, UserIndex);
					if (!ExistingPreV7Backup)
					{
						return;
					}
					ExistingPreV7Backup->AddToRoot();
					PreV7Backups[SlotIndex] = ExistingPreV7Backup;
				}
			}

			bReady = true;
			DeleteManualSaveSlots(UserIndex);
		}

		~FScopedManualSaveSlotBackup()
		{
			if (!bReady)
			{
				for (USaveGame* ExistingSave : Backups)
				{
					if (ExistingSave)
					{
						ExistingSave->RemoveFromRoot();
					}
				}
				for (USaveGame* ExistingPreV7Backup : PreV7Backups)
				{
					if (ExistingPreV7Backup)
					{
						ExistingPreV7Backup->RemoveFromRoot();
					}
				}
				return;
			}

			const int32 SlotCount = UGameXXKMVPSubsystem::GetManualSaveSlotCount();
			for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
			{
				const FString SlotName = UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
				UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
				UGameplayStatics::DeleteGameInSlot(SlotName + TEXT(".PreV7Backup"), UserIndex);
				if (Backups.IsValidIndex(SlotIndex) && Backups[SlotIndex])
				{
					UGameplayStatics::SaveGameToSlot(Backups[SlotIndex], SlotName, UserIndex);
					Backups[SlotIndex]->RemoveFromRoot();
				}
				if (PreV7Backups.IsValidIndex(SlotIndex) && PreV7Backups[SlotIndex])
				{
					UGameplayStatics::SaveGameToSlot(PreV7Backups[SlotIndex], SlotName + TEXT(".PreV7Backup"), UserIndex);
					PreV7Backups[SlotIndex]->RemoveFromRoot();
				}
			}
		}

		int32 UserIndex = 0;
		TArray<USaveGame*> Backups;
		TArray<USaveGame*> PreV7Backups;
		bool bReady = false;
	};

	static UGameXXKMVPSubsystem* CreateSeededSubsystem(UGameInstance* GameInstance, EGameXXKScreen Screen, int32 PlayerLevel)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		FGameXXKRuntimeState& RuntimeState = Subsystem->GetMutableRuntimeState();
		RuntimeState = UGameXXKMVPRules::CreateNewGame();
		RuntimeState.Screen = Screen;
		RuntimeState.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
		RuntimeState.PlayerLevel = PlayerLevel;
		UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(RuntimeState);
		return Subsystem;
	}

	static bool RowLabelContains(const FGameXXKMainMenuSaveSlotRow& Row, const TCHAR* ExpectedText)
	{
		return Row.Label.ToString().Contains(ExpectedText);
	}

	static FString GetResourceObjectPath(const UObject* ResourceObject)
	{
		return ResourceObject ? ResourceObject->GetPathName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMainMenuPlayerFlowTest,
	"GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMainMenuPlayerFlowTest::RunTest(const FString& Parameters)
{
	const int32 UserIndex = 9103;
	FScopedManualSaveSlotBackup SlotBackup(UserIndex);
	if (!TestTrue(TEXT("main-menu flow safely isolates all player manual save slots and migration backups"), SlotBackup.bReady))
	{
		return false;
	}

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Slot1Subsystem = CreateSeededSubsystem(TestGameInstance, EGameXXKScreen::Town, 2);
	UGameXXKMVPSubsystem* Slot3Subsystem = CreateSeededSubsystem(TestGameInstance, EGameXXKScreen::DungeonMap, 3);
	TestTrue(TEXT("seed player-facing slot 1 as Qingshan town level 2"), Slot1Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), UserIndex));
	TestTrue(TEXT("seed player-facing slot 3 as Qingshan route map level 3"), Slot3Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(2), UserIndex));
	UGameXXKSaveGame* LegacyPreviewSave = NewObject<UGameXXKSaveGame>();
	LegacyPreviewSave->SaveState = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	LegacyPreviewSave->SaveState.SaveVersion = 6;
	// A true pre-v7 preview owns equipment through legacy Inventory mirrors only.
	LegacyPreviewSave->SaveState.RuntimeState.EquipmentCollection = FGameXXKEquipmentCollectionState();
	TestTrue(TEXT("seed pre-v7 preview slot"), UGameplayStatics::SaveGameToSlot(LegacyPreviewSave, UGameXXKMVPSubsystem::GetManualSaveSlotName(3), UserIndex));
	UGameXXKSaveGame* FuturePreviewSave = NewObject<UGameXXKSaveGame>();
	FuturePreviewSave->SaveState = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	FuturePreviewSave->SaveState.SaveVersion = FGameXXKSaveMigration::CurrentSaveVersion + 1;
	TestTrue(TEXT("seed incompatible future preview slot"), UGameplayStatics::SaveGameToSlot(FuturePreviewSave, UGameXXKMVPSubsystem::GetManualSaveSlotName(4), UserIndex));

	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMainMenuWidget* MainMenu = NewObject<UGameXXKMainMenuWidget>();
	MainMenu->SetMVPSubsystem(Subsystem);
	MainMenu->SetSaveSlotUserIndexForTest(UserIndex);
	MainMenu->NativeConstruct();

	TestEqual(TEXT("main menu starts on landing layer"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::Landing);
	TestEqual(TEXT("main menu is visible on main menu screen"), MainMenu->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("main menu is enabled on main menu screen"), MainMenu->GetIsEnabled());

	const TArray<FGameXXKMVPCommandDescriptor> LandingActions = MainMenu->BuildLandingActionsForTest();
	TestEqual(TEXT("landing exposes exactly four player actions"), LandingActions.Num(), 4);
	TestTrue(TEXT("landing exposes New Game"), MainMenu->HasLandingActionForTest(FName(TEXT("NewGame")), true));
	TestTrue(TEXT("landing exposes Continue"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenContinue")), true));
	TestTrue(TEXT("landing exposes Options"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenOptions")), true));
	TestTrue(TEXT("landing exposes Quit"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenQuit")), true));
	if (LandingActions.Num() == 4)
	{
		TestEqual(TEXT("landing action 1 is localized start"), LandingActions[0].Label.ToString(), FString(TEXT("开始游戏")));
		TestEqual(TEXT("landing action 2 is localized continue"), LandingActions[1].Label.ToString(), FString(TEXT("加载存档")));
		TestEqual(TEXT("landing action 3 is localized options"), LandingActions[2].Label.ToString(), FString(TEXT("设置游戏")));
		TestEqual(TEXT("landing action 4 is localized quit"), LandingActions[3].Label.ToString(), FString(TEXT("退出")));
	}
	TestFalse(TEXT("landing does not expose slot continue"), MainMenu->HasLandingActionForTest(FName(TEXT("ContinueSlot1")), true));
	TestFalse(TEXT("landing does not expose slot delete"), MainMenu->HasLandingActionForTest(FName(TEXT("DeleteSlot1")), true));
	TestFalse(TEXT("landing does not expose battle selection"), MainMenu->HasLandingActionForTest(FName(TEXT("SelectBattle")), true));

	UImage* CoverImage = Cast<UImage>(MainMenu->GetWidgetFromName(TEXT("GameXXKMainMenuCover")));
	TestNotNull(TEXT("main menu owns a full-screen cover image"), CoverImage);
	if (CoverImage)
	{
		TestTrue(
			TEXT("cover image uses the GameXXK tiger duel cover texture"),
			GetResourceObjectPath(CoverImage->GetBrush().GetResourceObject()).Contains(TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_MainMenuCover")));
	}

	UVerticalBox* LandingBox = Cast<UVerticalBox>(MainMenu->GetWidgetFromName(TEXT("LandingBox")));
	TestNotNull(TEXT("landing button stack exists"), LandingBox);
	if (LandingBox)
	{
		const UOverlaySlot* LandingSlot = Cast<UOverlaySlot>(LandingBox->Slot);
		TestNotNull(TEXT("landing button stack is placed by root overlay"), LandingSlot);
		if (LandingSlot)
		{
			TestEqual(TEXT("landing button stack is left aligned"), LandingSlot->GetHorizontalAlignment(), HAlign_Left);
			TestEqual(TEXT("landing button stack is vertically centered"), LandingSlot->GetVerticalAlignment(), VAlign_Center);
			TestTrue(TEXT("landing button stack sits in from the left edge"), LandingSlot->GetPadding().Left >= 96.0f);
		}
	}

	UButton* StartButton = Cast<UButton>(MainMenu->GetWidgetFromName(TEXT("MainMenuStartButton")));
	UTextBlock* StartLabel = Cast<UTextBlock>(MainMenu->GetWidgetFromName(TEXT("MainMenuStartButtonLabel")));
	UTextBlock* ContinueLabel = Cast<UTextBlock>(MainMenu->GetWidgetFromName(TEXT("MainMenuContinueButtonLabel")));
	UTextBlock* OptionsLabel = Cast<UTextBlock>(MainMenu->GetWidgetFromName(TEXT("MainMenuOptionsButtonLabel")));
	UTextBlock* QuitLabel = Cast<UTextBlock>(MainMenu->GetWidgetFromName(TEXT("MainMenuQuitButtonLabel")));
	TestNotNull(TEXT("start button is named for automation"), StartButton);
	TestNotNull(TEXT("start label is named for automation"), StartLabel);
	TestNotNull(TEXT("continue label is named for automation"), ContinueLabel);
	TestNotNull(TEXT("options label is named for automation"), OptionsLabel);
	TestNotNull(TEXT("quit label is named for automation"), QuitLabel);
	if (StartLabel && ContinueLabel && OptionsLabel && QuitLabel)
	{
		TestEqual(TEXT("visible start button label is Chinese"), StartLabel->GetText().ToString(), FString(TEXT("开始游戏")));
		TestEqual(TEXT("visible continue button label is Chinese"), ContinueLabel->GetText().ToString(), FString(TEXT("加载存档")));
		TestEqual(TEXT("visible options button label is Chinese"), OptionsLabel->GetText().ToString(), FString(TEXT("设置游戏")));
		TestEqual(TEXT("visible quit button label is Chinese"), QuitLabel->GetText().ToString(), FString(TEXT("退出")));
	}
	if (StartButton)
	{
		TestTrue(
			TEXT("main menu buttons use the generated ink button texture"),
			GetResourceObjectPath(StartButton->GetStyle().Normal.GetResourceObject()).Contains(TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase")));
	}

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

		TestTrue(TEXT("pre-v7 preview remains occupied"), Rows[3].bOccupied);
		TestTrue(TEXT("pre-v7 preview is loadable"), Rows[3].bCanLoad);
		TestTrue(TEXT("pre-v7 preview is deletable"), Rows[3].bCanDelete);
		TestFalse(TEXT("pure pre-v7 preview creates no backup"), UGameplayStatics::DoesSaveGameExist(UGameXXKMVPSubsystem::GetManualSaveSlotName(3) + TEXT(".PreV7Backup"), UserIndex));

		TestTrue(TEXT("future preview remains occupied"), Rows[4].bOccupied);
		TestFalse(TEXT("future preview is not loadable"), Rows[4].bCanLoad);
		TestTrue(TEXT("future preview remains deletable"), Rows[4].bCanDelete);
		TestTrue(TEXT("future preview has stable incompatible label"), RowLabelContains(Rows[4], TEXT("Incompatible Save")));
	}

	TestFalse(TEXT("continue rejects empty slot"), MainMenu->ContinueFromSlotIndex(1));
	TestFalse(TEXT("direct continue rejects incompatible future slot"), MainMenu->ContinueGameFromSlot(UGameXXKMVPSubsystem::GetManualSaveSlotName(4), UserIndex));
	UVerticalBox* ErrorModalBox = Cast<UVerticalBox>(MainMenu->GetWidgetFromName(TEXT("ModalBox")));
	bool bFoundMigrationError = false;
	if (ErrorModalBox)
	{
		for (UWidget* Child : ErrorModalBox->GetAllChildren())
		{
			const UTextBlock* ErrorText = Cast<UTextBlock>(Child);
			bFoundMigrationError = bFoundMigrationError
				|| (ErrorText && ErrorText->GetText().ToString() == TEXT("存档迁移失败，已保留原存档。"));
		}
	}
	TestTrue(TEXT("failed continue displays approved migration error in modal"), bFoundMigrationError);
	TestTrue(TEXT("continue loads populated slot 1"), MainMenu->ContinueFromSlotIndex(0));
	TestEqual(TEXT("loaded slot restores town screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("loaded slot restores level 2"), Subsystem->GetRuntimeState().PlayerLevel, 2);
	TestEqual(
		TEXT("continue keeps the accepted 3D town entry until the HUD migration is approved"),
		MainMenu->GetLastRequestedTownMapForTest(),
		FName(TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")));
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

	TestTrue(TEXT("main menu start creates a new game and lands in Qingshan town"), MainMenu->StartGame());
	TestEqual(TEXT("town screen after player-facing main menu start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("new game selects Qingshan town directly"), Subsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());
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
