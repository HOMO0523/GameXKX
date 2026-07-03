# Main Menu Player Flow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the default dense MVP command shell on the main menu with a clean player-facing main menu: `New Game`, `Continue`, `Options`, and `Quit`, with Continue and Delete hidden behind modal states.

**Architecture:** Keep `UGameXXKPlayableRootWidget` and `GameXXKMVPCommandRouter` available for debug automation, but stop using them as the default player-facing main menu. Put the player menu state machine and programmatic UI inside `UGameXXKMainMenuWidget`, then have `AGameXXKMVPHUD` create that widget by default and draw the legacy Canvas command shell only when a debug flag is enabled.

**Tech Stack:** Unreal Engine C++, UMG programmatic widgets, existing `UGameXXKMVPSubsystem`, `UGameplayStatics` save slots, UE Automation Tests, UBT.

---

## File Structure

- Modify `Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h`: add the main-menu modal state enum, save-slot row data, player-facing action/query methods, and transient widget references.
- Modify `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`: implement menu state, compact slot rows, Continue modal, delete confirmation modal, unavailable Options/Quit modals, and `New Game -> Qingshan Town` player flow.
- Modify `Source/GameXXK/Public/UI/GameXXKMVPHUD.h`: add `MainMenuWidgetClass`, `MainMenuWidget`, debug-shell controls, and test accessors.
- Modify `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`: create the player main menu widget by default, refresh it, and hide the old Canvas shell unless debug is enabled.
- Modify `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`: update the existing main-menu behavior expectation from world map to town for the player-facing `UGameXXKMainMenuWidget`.
- Create `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`: focused tests for landing actions, Continue modal, compact rows, delete confirmation modal, Options/Quit unavailable modals, and HUD debug-shell gating.

---

### Task 1: Player Main Menu Behavior Tests

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [ ] **Step 1: Write the failing focused main-menu test**

Create `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp` with this test. It intentionally references APIs that do not exist yet.

```cpp
#include "GameXXKMVPRules.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKMainMenuWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static const int32 MainMenuPlayerFlowUserIndex = 0;

	static void DeleteManualSlots()
	{
		for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
		{
			UGameplayStatics::DeleteGameInSlot(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), MainMenuPlayerFlowUserIndex);
		}
	}

	static void SeedSlot(UGameXXKMVPSubsystem* Subsystem, int32 SlotIndex, EGameXXKScreen Screen, FName Region, int32 Level)
	{
		check(Subsystem);
		check(Subsystem->StartGame());
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State.Screen = Screen;
		State.CurrentRegion = Region;
		State.CurrentMapId = Region.ToString();
		State.PlayerLevel = Level;
		check(Subsystem->SaveCurrentGame(UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex), MainMenuPlayerFlowUserIndex));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMainMenuPlayerFlowTest,
	"GameXXK.MVP.UI.MainMenuPlayerFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMainMenuPlayerFlowTest::RunTest(const FString& Parameters)
{
	DeleteManualSlots();

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* RuntimeSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMVPSubsystem* Slot1SeedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMVPSubsystem* Slot3SeedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);

	SeedSlot(Slot1SeedSubsystem, 0, EGameXXKScreen::Town, UGameXXKMVPRules::RegionQingshan(), 2);
	SeedSlot(Slot3SeedSubsystem, 2, EGameXXKScreen::DungeonMap, UGameXXKMVPRules::RegionQingshan(), 3);

	UGameXXKMainMenuWidget* MainMenu = NewObject<UGameXXKMainMenuWidget>();
	MainMenu->SetMVPSubsystem(RuntimeSubsystem);
	MainMenu->SetSaveSlotUserIndexForTest(MainMenuPlayerFlowUserIndex);

	TestEqual(TEXT("main menu starts on landing layer"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::Landing);
	TestEqual(TEXT("landing exposes four player actions"), MainMenu->BuildLandingActionsForTest().Num(), 4);
	TestTrue(TEXT("landing exposes New Game"), MainMenu->HasLandingActionForTest(FName(TEXT("NewGame")), true));
	TestTrue(TEXT("landing exposes Continue"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenContinue")), true));
	TestTrue(TEXT("landing exposes Options"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenOptions")), true));
	TestTrue(TEXT("landing exposes Quit"), MainMenu->HasLandingActionForTest(FName(TEXT("OpenQuit")), true));
	TestFalse(TEXT("landing does not expose continue slot commands"), MainMenu->HasLandingActionForTest(FName(TEXT("ContinueSlot1")), true));
	TestFalse(TEXT("landing does not expose delete slot commands"), MainMenu->HasLandingActionForTest(FName(TEXT("DeleteSlot1")), true));
	TestFalse(TEXT("landing does not expose route commands"), MainMenu->HasLandingActionForTest(FName(TEXT("SelectBattle")), true));

	TestTrue(TEXT("continue modal opens"), MainMenu->OpenContinueModal());
	TestEqual(TEXT("continue modal layer active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);
	const TArray<FGameXXKMainMenuSaveSlotRow> SlotRows = MainMenu->BuildSaveSlotRowsForTest();
	TestEqual(TEXT("continue modal has five compact rows"), SlotRows.Num(), UGameXXKMVPSubsystem::GetManualSaveSlotCount());
	TestTrue(TEXT("slot 1 is occupied"), SlotRows[0].bOccupied);
	TestTrue(TEXT("slot 1 can load"), SlotRows[0].bCanLoad);
	TestTrue(TEXT("slot 1 can delete"), SlotRows[0].bCanDelete);
	TestTrue(TEXT("slot 1 row mentions town"), SlotRows[0].Label.ToString().Contains(TEXT("Qingshan Town")));
	TestFalse(TEXT("slot 2 is empty"), SlotRows[1].bOccupied);
	TestFalse(TEXT("slot 2 cannot load"), SlotRows[1].bCanLoad);
	TestFalse(TEXT("slot 2 cannot delete"), SlotRows[1].bCanDelete);
	TestTrue(TEXT("slot 2 row says empty"), SlotRows[1].Label.ToString().Contains(TEXT("Empty")));
	TestTrue(TEXT("slot 3 row mentions route map"), SlotRows[2].Label.ToString().Contains(TEXT("Route Map")));

	TestFalse(TEXT("empty slot does not load"), MainMenu->ContinueFromSlotIndex(1));
	TestTrue(TEXT("slot 1 loads"), MainMenu->ContinueFromSlotIndex(0));
	TestEqual(TEXT("slot 1 restores town"), RuntimeSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("slot 1 restores level"), RuntimeSubsystem->GetRuntimeState().PlayerLevel, 2);

	RuntimeSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::MainMenu;
	TestTrue(TEXT("continue modal reopens for delete"), MainMenu->OpenContinueModal());
	TestFalse(TEXT("empty slot cannot request delete"), MainMenu->RequestDeleteSlot(1));
	TestTrue(TEXT("populated slot requests delete confirmation"), MainMenu->RequestDeleteSlot(0));
	TestEqual(TEXT("delete confirmation layer active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::DeleteConfirmModal);
	TestEqual(TEXT("pending delete slot is slot 1"), MainMenu->GetPendingDeleteSlotIndexForTest(), 0);
	TestTrue(TEXT("cancel delete returns to continue modal"), MainMenu->CancelDeleteSlot());
	TestEqual(TEXT("cancel delete restores continue modal"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);
	TestTrue(TEXT("cancel keeps slot 1"), UGameplayStatics::DoesSaveGameExist(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), MainMenuPlayerFlowUserIndex));
	TestTrue(TEXT("slot 1 requests delete again"), MainMenu->RequestDeleteSlot(0));
	TestTrue(TEXT("confirm delete removes slot 1"), MainMenu->ConfirmDeleteSlot());
	TestEqual(TEXT("confirm delete returns to continue modal"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::ContinueModal);
	TestFalse(TEXT("slot 1 save was removed"), UGameplayStatics::DoesSaveGameExist(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), MainMenuPlayerFlowUserIndex));
	TestFalse(TEXT("slot 1 row is now empty"), MainMenu->BuildSaveSlotRowsForTest()[0].bOccupied);

	TestTrue(TEXT("options modal opens"), MainMenu->OpenOptionsModal());
	TestEqual(TEXT("options modal layer active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::OptionsModal);
	TestTrue(TEXT("close returns options modal to landing"), MainMenu->CloseActiveModal());
	TestEqual(TEXT("options close lands on landing"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::Landing);
	TestTrue(TEXT("quit unavailable modal opens"), MainMenu->OpenQuitUnavailableModal());
	TestEqual(TEXT("quit modal layer active"), MainMenu->GetMenuLayerForTest(), EGameXXKMainMenuLayer::QuitUnavailableModal);

	RuntimeSubsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	RuntimeSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::MainMenu;
	TestTrue(TEXT("New Game enters Qingshan town in player menu flow"), MainMenu->StartGame());
	TestEqual(TEXT("New Game player menu flow enters town"), RuntimeSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("New Game player menu flow selects Qingshan"), RuntimeSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(RuntimeSubsystem);
	UGameXXKMainMenuWidget* HUDMainMenu = HUD->CreateMainMenuWidgetForTest();
	TestNotNull(TEXT("HUD creates player main menu widget"), HUDMainMenu);
	TestTrue(TEXT("HUD retains player main menu widget"), HUD->HasMainMenuWidget());
	TestFalse(TEXT("HUD debug shell is disabled by default"), HUD->IsDebugPlayableShellEnabledForTest());

	HUD->SetDebugPlayableShellEnabledForTest(true);
	TestTrue(TEXT("HUD debug shell can be enabled for automation"), HUD->IsDebugPlayableShellEnabledForTest());
	TestNotNull(TEXT("HUD can still create playable root debug widget"), HUD->CreatePlayableRootWidgetForTest());

	DeleteManualSlots();
	return true;
}

#endif
```

- [ ] **Step 2: Update the existing UI widget test expectation**

In `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`, replace the three assertions immediately after `MainMenu->StartGame()` with the player-facing town expectation:

```cpp
	TestFalse(TEXT("main menu continue rejects missing slot"), MainMenu->ContinueGameFromSlot(UiTestSlot, UserIndex));
	TestTrue(TEXT("main menu start creates new game and opens Qingshan town"), MainMenu->StartGame());
	TestEqual(TEXT("town screen after player-facing main menu start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("Qingshan selected after player-facing main menu start"), Subsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());
	TestFalse(TEXT("main menu start does not autosave"), MainMenu->DoesSaveGameExist(UiTestSlot, UserIndex));
```

- [ ] **Step 3: Run tests and verify RED**

Run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.MainMenuPlayerFlow;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compilation or test failure mentioning missing `EGameXXKMainMenuLayer`, `FGameXXKMainMenuSaveSlotRow`, or missing `UGameXXKMainMenuWidget` methods.

- [ ] **Step 4: Commit the failing tests**

```powershell
git add -- Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
git commit -m "test: cover player main menu flow"
```

---

### Task 2: Main Menu State Model And Behavior API

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`

- [ ] **Step 1: Add declarations for menu layers and slot rows**

In `Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h`, replace the current header content with this expanded interface while preserving the existing public behavior methods:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKMainMenuWidget.generated.h"

class UBorder;
class UButton;
class UOverlay;
class UTextBlock;
class UVerticalBox;

UENUM(BlueprintType)
enum class EGameXXKMainMenuLayer : uint8
{
	Landing,
	ContinueModal,
	DeleteConfirmModal,
	OptionsModal,
	QuitUnavailableModal
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMainMenuSaveSlotRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bCanLoad = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bCanDelete = false;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKMainMenuWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ContinueGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ContinueFromSlotIndex(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	bool DoesSaveGameExist(FString SlotName, int32 UserIndex = 0) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool DeleteSaveGame(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenContinueModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool RequestDeleteSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ConfirmDeleteSlot();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool CancelDeleteSlot();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenOptionsModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenQuitUnavailableModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool CloseActiveModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	void RefreshFromState();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	TArray<FGameXXKMVPCommandDescriptor> BuildLandingActionsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	bool HasLandingActionForTest(FName ActionName, bool bExpectedEnabled) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	TArray<FGameXXKMainMenuSaveSlotRow> BuildSaveSlotRowsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	EGameXXKMainMenuLayer GetMenuLayerForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	int32 GetPendingDeleteSlotIndexForTest() const;

	void SetSaveSlotUserIndexForTest(int32 UserIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|MainMenu")
	void OnStartGameSucceeded();

private:
	FGameXXKMainMenuSaveSlotRow BuildSaveSlotRow(int32 SlotIndex) const;
	FText BuildSlotLabel(const FGameXXKMainMenuSaveSlotRow& Row) const;
	FString BuildScreenLabel(const FGameXXKRuntimeState& State) const;
	void SetMenuLayer(EGameXXKMainMenuLayer NewLayer);
	bool IsValidManualSlotIndex(int32 SlotIndex) const;
	FString GetManualSlotNameChecked(int32 SlotIndex) const;
	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();

	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleOptionsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleCloseModalClicked();

	UFUNCTION()
	void HandleConfirmDeleteClicked();

	UFUNCTION()
	void HandleCancelDeleteClicked();

	void HandleSaveSlotClicked(int32 SlotIndex);
	void HandleDeleteSlotClicked(int32 SlotIndex);

	UPROPERTY(Transient)
	EGameXXKMainMenuLayer MenuLayer = EGameXXKMainMenuLayer::Landing;

	UPROPERTY(Transient)
	int32 SaveSlotUserIndex = 0;

	UPROPERTY(Transient)
	int32 PendingDeleteSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LandingBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ContinueModal;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContinueSlotList;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DeleteConfirmModal;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OptionsModal;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> QuitUnavailableModal;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SlotLoadButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SlotDeleteButtons;
};
```

- [ ] **Step 2: Implement behavior-only methods**

In `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`, add these includes:

```cpp
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKSaveGame.h"
```

Then update the existing behavior functions and add the new state methods:

```cpp
namespace
{
	static const FName MainMenuNewGame(TEXT("NewGame"));
	static const FName MainMenuOpenContinue(TEXT("OpenContinue"));
	static const FName MainMenuOpenOptions(TEXT("OpenOptions"));
	static const FName MainMenuOpenQuit(TEXT("OpenQuit"));
}

void UGameXXKMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

bool UGameXXKMainMenuWidget::StartGame()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const bool bEnteredTown = Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());
	if (bEnteredTown)
	{
		SetMenuLayer(EGameXXKMainMenuLayer::Landing);
		OnStartGameSucceeded();
		RefreshFromState();
	}
	return bEnteredTown;
}

bool UGameXXKMainMenuWidget::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	return ContinueGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMainMenuWidget::ContinueGameFromSlot(FString SlotName, int32 UserIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bStarted = Subsystem && Subsystem->ContinueGameFromSlot(SlotName, UserIndex);
	if (bStarted)
	{
		SetMenuLayer(EGameXXKMainMenuLayer::Landing);
		OnStartGameSucceeded();
		RefreshFromState();
	}
	return bStarted;
}

bool UGameXXKMainMenuWidget::ContinueFromSlotIndex(int32 SlotIndex)
{
	if (!IsValidManualSlotIndex(SlotIndex))
	{
		return false;
	}
	const FGameXXKMainMenuSaveSlotRow Row = BuildSaveSlotRow(SlotIndex);
	return Row.bCanLoad && ContinueGameFromSlot(Row.SlotName, SaveSlotUserIndex);
}

bool UGameXXKMainMenuWidget::DoesSaveGameExist(FString SlotName, int32 UserIndex) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->DoesSaveGameExist(SlotName, UserIndex);
}

bool UGameXXKMainMenuWidget::DeleteSaveGame(FString SlotName, int32 UserIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bDeleted = Subsystem && Subsystem->DeleteSaveGame(SlotName, UserIndex);
	if (bDeleted)
	{
		RefreshFromState();
	}
	return bDeleted;
}

bool UGameXXKMainMenuWidget::OpenContinueModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::RequestDeleteSlot(int32 SlotIndex)
{
	if (!IsValidManualSlotIndex(SlotIndex))
	{
		return false;
	}
	const FGameXXKMainMenuSaveSlotRow Row = BuildSaveSlotRow(SlotIndex);
	if (!Row.bCanDelete)
	{
		return false;
	}

	PendingDeleteSlotIndex = SlotIndex;
	SetMenuLayer(EGameXXKMainMenuLayer::DeleteConfirmModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::ConfirmDeleteSlot()
{
	if (!IsValidManualSlotIndex(PendingDeleteSlotIndex))
	{
		return false;
	}

	const int32 SlotIndexToDelete = PendingDeleteSlotIndex;
	PendingDeleteSlotIndex = INDEX_NONE;
	const bool bDeleted = DeleteSaveGame(GetManualSlotNameChecked(SlotIndexToDelete), SaveSlotUserIndex);
	SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	RefreshFromState();
	return bDeleted;
}

bool UGameXXKMainMenuWidget::CancelDeleteSlot()
{
	PendingDeleteSlotIndex = INDEX_NONE;
	SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::OpenOptionsModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::OptionsModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::OpenQuitUnavailableModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::QuitUnavailableModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::CloseActiveModal()
{
	PendingDeleteSlotIndex = INDEX_NONE;
	SetMenuLayer(EGameXXKMainMenuLayer::Landing);
	RefreshFromState();
	return true;
}

void UGameXXKMainMenuWidget::RefreshFromState()
{
	RefreshProgrammaticLayout();
}

TArray<FGameXXKMVPCommandDescriptor> UGameXXKMainMenuWidget::BuildLandingActionsForTest() const
{
	return {
		FGameXXKMVPCommandDescriptor(MainMenuNewGame, FText::FromString(TEXT("New Game")), true),
		FGameXXKMVPCommandDescriptor(MainMenuOpenContinue, FText::FromString(TEXT("Continue")), true),
		FGameXXKMVPCommandDescriptor(MainMenuOpenOptions, FText::FromString(TEXT("Options")), true),
		FGameXXKMVPCommandDescriptor(MainMenuOpenQuit, FText::FromString(TEXT("Quit")), true)
	};
}

bool UGameXXKMainMenuWidget::HasLandingActionForTest(FName ActionName, bool bExpectedEnabled) const
{
	const FGameXXKMVPCommandDescriptor* Action = BuildLandingActionsForTest().FindByPredicate([ActionName](const FGameXXKMVPCommandDescriptor& Candidate)
	{
		return Candidate.CommandName == ActionName;
	});
	return Action && Action->bEnabled == bExpectedEnabled;
}

TArray<FGameXXKMainMenuSaveSlotRow> UGameXXKMainMenuWidget::BuildSaveSlotRowsForTest() const
{
	TArray<FGameXXKMainMenuSaveSlotRow> Rows;
	for (int32 SlotIndex = 0; SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount(); ++SlotIndex)
	{
		Rows.Add(BuildSaveSlotRow(SlotIndex));
	}
	return Rows;
}

EGameXXKMainMenuLayer UGameXXKMainMenuWidget::GetMenuLayerForTest() const
{
	return MenuLayer;
}

int32 UGameXXKMainMenuWidget::GetPendingDeleteSlotIndexForTest() const
{
	return PendingDeleteSlotIndex;
}

void UGameXXKMainMenuWidget::SetSaveSlotUserIndexForTest(int32 UserIndex)
{
	SaveSlotUserIndex = UserIndex;
	RefreshFromState();
}

FGameXXKMainMenuSaveSlotRow UGameXXKMainMenuWidget::BuildSaveSlotRow(int32 SlotIndex) const
{
	FGameXXKMainMenuSaveSlotRow Row;
	if (!IsValidManualSlotIndex(SlotIndex))
	{
		Row.Label = FText::FromString(TEXT("Invalid Slot"));
		return Row;
	}

	Row.SlotIndex = SlotIndex;
	Row.SlotName = GetManualSlotNameChecked(SlotIndex);
	Row.bOccupied = DoesSaveGameExist(Row.SlotName, SaveSlotUserIndex);
	Row.bCanLoad = Row.bOccupied;
	Row.bCanDelete = Row.bOccupied;
	Row.Label = BuildSlotLabel(Row);
	return Row;
}

FText UGameXXKMainMenuWidget::BuildSlotLabel(const FGameXXKMainMenuSaveSlotRow& Row) const
{
	const int32 DisplayIndex = Row.SlotIndex + 1;
	if (!Row.bOccupied)
	{
		return FText::FromString(FString::Printf(TEXT("Slot %d · Empty"), DisplayIndex));
	}

	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(Row.SlotName, SaveSlotUserIndex));
	if (!SaveGame)
	{
		return FText::FromString(FString::Printf(TEXT("Slot %d · Unreadable"), DisplayIndex));
	}

	const FGameXXKRuntimeState State = UGameXXKMVPRules::RestoreFromSaveState(SaveGame->SaveState);
	return FText::FromString(FString::Printf(TEXT("Slot %d · %s · Lv %d"), DisplayIndex, *BuildScreenLabel(State), State.PlayerLevel));
}

FString UGameXXKMainMenuWidget::BuildScreenLabel(const FGameXXKRuntimeState& State) const
{
	switch (State.Screen)
	{
	case EGameXXKScreen::Town:
		return State.CurrentRegion == UGameXXKMVPRules::RegionQingshan() ? TEXT("Qingshan Town") : TEXT("Town");
	case EGameXXKScreen::DungeonMap:
		return TEXT("Route Map");
	case EGameXXKScreen::Battle:
		return TEXT("Battle");
	case EGameXXKScreen::WorldMap:
		return TEXT("World Map");
	case EGameXXKScreen::MainMenu:
		return TEXT("Main Menu");
	default:
		return TEXT("Unknown");
	}
}

void UGameXXKMainMenuWidget::SetMenuLayer(EGameXXKMainMenuLayer NewLayer)
{
	MenuLayer = NewLayer;
}

bool UGameXXKMainMenuWidget::IsValidManualSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount();
}

FString UGameXXKMainMenuWidget::GetManualSlotNameChecked(int32 SlotIndex) const
{
	return UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
}
```

- [ ] **Step 3: Run the focused test and verify behavior RED changes to layout gaps**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoLiveCoding -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.MainMenuPlayerFlow;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compilation can still fail because `BuildProgrammaticLayout`, `RefreshProgrammaticLayout`, and button handlers are declared but not implemented. If it compiles, the behavior assertions pass and layout visibility assertions remain uncovered until Task 3.

- [ ] **Step 4: Commit the behavior API**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp
git commit -m "feat: add player main menu state model"
```

---

### Task 3: Programmatic Player Main Menu Layout

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`

- [ ] **Step 1: Implement the landing and modal layout**

In `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`, implement these helper functions. The local lambdas intentionally keep the C++ UI plain and testable; this plan covers structure and interaction, not final art polish.

```cpp
void UGameXXKMainMenuWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree || RootOverlay || WidgetTree->RootWidget)
	{
		return;
	}

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameXXKMainMenuRoot"));
	WidgetTree->RootWidget = RootOverlay;

	auto MakeLabel = [this](const FString& Name, const FText& Text, int32 FontSize)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*Name));
		Label->SetText(Text);
		Label->SetFontSize(FontSize);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return Label;
	};

	auto MakeButton = [this, &MakeLabel](const FString& Name, const FText& Text)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*Name));
		UTextBlock* Label = MakeLabel(Name + TEXT("Label"), Text, 18);
		Button->AddChild(Label);
		return Button;
	};

	LandingBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainMenuLanding"));
	RootOverlay->AddChild(LandingBox);
	LandingBox->AddChildToVerticalBox(MakeLabel(TEXT("MainMenuTitle"), FText::FromString(TEXT("GameXXK")), 42));

	UButton* NewGameButton = MakeButton(TEXT("MainMenuNewGameButton"), FText::FromString(TEXT("New Game")));
	UButton* ContinueButton = MakeButton(TEXT("MainMenuContinueButton"), FText::FromString(TEXT("Continue")));
	UButton* OptionsButton = MakeButton(TEXT("MainMenuOptionsButton"), FText::FromString(TEXT("Options")));
	UButton* QuitButton = MakeButton(TEXT("MainMenuQuitButton"), FText::FromString(TEXT("Quit")));
	NewGameButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleNewGameClicked);
	ContinueButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleContinueClicked);
	OptionsButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleOptionsClicked);
	QuitButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleQuitClicked);
	LandingBox->AddChildToVerticalBox(NewGameButton);
	LandingBox->AddChildToVerticalBox(ContinueButton);
	LandingBox->AddChildToVerticalBox(OptionsButton);
	LandingBox->AddChildToVerticalBox(QuitButton);

	ContinueModal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContinueModal"));
	ContinueModal->SetPadding(FMargin(20.0f));
	ContinueModal->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.96f));
	ContinueSlotList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContinueSlotList"));
	ContinueModal->SetContent(ContinueSlotList);
	RootOverlay->AddChild(ContinueModal);

	DeleteConfirmModal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DeleteConfirmModal"));
	DeleteConfirmModal->SetPadding(FMargin(18.0f));
	DeleteConfirmModal->SetBrushColor(FLinearColor(0.20f, 0.05f, 0.06f, 0.98f));
	UVerticalBox* DeleteBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DeleteConfirmBox"));
	DeleteBox->AddChildToVerticalBox(MakeLabel(TEXT("DeleteConfirmText"), FText::FromString(TEXT("Delete Slot?")), 18));
	UButton* ConfirmDeleteButton = MakeButton(TEXT("ConfirmDeleteButton"), FText::FromString(TEXT("Delete")));
	UButton* CancelDeleteButton = MakeButton(TEXT("CancelDeleteButton"), FText::FromString(TEXT("Cancel")));
	ConfirmDeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleConfirmDeleteClicked);
	CancelDeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCancelDeleteClicked);
	DeleteBox->AddChildToVerticalBox(ConfirmDeleteButton);
	DeleteBox->AddChildToVerticalBox(CancelDeleteButton);
	DeleteConfirmModal->SetContent(DeleteBox);
	RootOverlay->AddChild(DeleteConfirmModal);

	OptionsModal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OptionsUnavailableModal"));
	OptionsModal->SetPadding(FMargin(18.0f));
	OptionsModal->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.98f));
	UVerticalBox* OptionsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OptionsBox"));
	OptionsBox->AddChildToVerticalBox(MakeLabel(TEXT("OptionsUnavailableText"), FText::FromString(TEXT("Options are not available yet.")), 18));
	UButton* CloseOptionsButton = MakeButton(TEXT("CloseOptionsButton"), FText::FromString(TEXT("Close")));
	CloseOptionsButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCloseModalClicked);
	OptionsBox->AddChildToVerticalBox(CloseOptionsButton);
	OptionsModal->SetContent(OptionsBox);
	RootOverlay->AddChild(OptionsModal);

	QuitUnavailableModal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuitUnavailableModal"));
	QuitUnavailableModal->SetPadding(FMargin(18.0f));
	QuitUnavailableModal->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.98f));
	UVerticalBox* QuitBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("QuitBox"));
	QuitBox->AddChildToVerticalBox(MakeLabel(TEXT("QuitUnavailableText"), FText::FromString(TEXT("Quit is not available in editor.")), 18));
	UButton* CloseQuitButton = MakeButton(TEXT("CloseQuitButton"), FText::FromString(TEXT("Close")));
	CloseQuitButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCloseModalClicked);
	QuitBox->AddChildToVerticalBox(CloseQuitButton);
	QuitUnavailableModal->SetContent(QuitBox);
	RootOverlay->AddChild(QuitUnavailableModal);
}
```

- [ ] **Step 2: Implement slot-row refresh**

Add this function below `BuildProgrammaticLayout()`:

```cpp
void UGameXXKMainMenuWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();

	if (LandingBox)
	{
		LandingBox->SetVisibility(ESlateVisibility::Visible);
	}
	if (ContinueModal)
	{
		ContinueModal->SetVisibility(MenuLayer == EGameXXKMainMenuLayer::ContinueModal || MenuLayer == EGameXXKMainMenuLayer::DeleteConfirmModal
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	if (DeleteConfirmModal)
	{
		DeleteConfirmModal->SetVisibility(MenuLayer == EGameXXKMainMenuLayer::DeleteConfirmModal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (OptionsModal)
	{
		OptionsModal->SetVisibility(MenuLayer == EGameXXKMainMenuLayer::OptionsModal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (QuitUnavailableModal)
	{
		QuitUnavailableModal->SetVisibility(MenuLayer == EGameXXKMainMenuLayer::QuitUnavailableModal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (!ContinueSlotList)
	{
		return;
	}

	ContinueSlotList->ClearChildren();
	SlotLoadButtons.Reset();
	SlotLabels.Reset();
	SlotDeleteButtons.Reset();

	for (const FGameXXKMainMenuSaveSlotRow& Row : BuildSaveSlotRowsForTest())
	{
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*FString::Printf(TEXT("SaveSlotRow%d"), Row.SlotIndex)));
		UButton* LoadButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("SaveSlotLoadButton%d"), Row.SlotIndex)));
		UTextBlock* RowLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("SaveSlotLabel%d"), Row.SlotIndex)));
		UButton* DeleteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("SaveSlotDeleteButton%d"), Row.SlotIndex)));
		UTextBlock* DeleteLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("SaveSlotDeleteLabel%d"), Row.SlotIndex)));

		RowLabel->SetText(Row.Label);
		RowLabel->SetFontSize(16);
		RowLabel->SetColorAndOpacity(FSlateColor(Row.bOccupied ? FLinearColor::White : FLinearColor(0.45f, 0.48f, 0.52f, 1.0f)));
		DeleteLabel->SetText(FText::FromString(TEXT("Delete")));
		DeleteLabel->SetFontSize(14);
		DeleteLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.55f, 0.55f, 1.0f)));

		LoadButton->AddChild(RowLabel);
		LoadButton->SetIsEnabled(Row.bCanLoad);
		DeleteButton->AddChild(DeleteLabel);
		DeleteButton->SetIsEnabled(Row.bCanDelete);
		RowBox->AddChildToHorizontalBox(LoadButton);
		RowBox->AddChildToHorizontalBox(DeleteButton);
		ContinueSlotList->AddChildToVerticalBox(RowBox);

		SlotLoadButtons.Add(LoadButton);
		SlotLabels.Add(RowLabel);
		SlotDeleteButtons.Add(DeleteButton);
	}
}
```

- [ ] **Step 3: Implement button handlers**

Add these handlers at the bottom of `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`:

```cpp
void UGameXXKMainMenuWidget::HandleNewGameClicked()
{
	StartGame();
}

void UGameXXKMainMenuWidget::HandleContinueClicked()
{
	OpenContinueModal();
}

void UGameXXKMainMenuWidget::HandleOptionsClicked()
{
	OpenOptionsModal();
}

void UGameXXKMainMenuWidget::HandleQuitClicked()
{
	OpenQuitUnavailableModal();
}

void UGameXXKMainMenuWidget::HandleCloseModalClicked()
{
	CloseActiveModal();
}

void UGameXXKMainMenuWidget::HandleConfirmDeleteClicked()
{
	ConfirmDeleteSlot();
}

void UGameXXKMainMenuWidget::HandleCancelDeleteClicked()
{
	CancelDeleteSlot();
}

void UGameXXKMainMenuWidget::HandleSaveSlotClicked(int32 SlotIndex)
{
	ContinueFromSlotIndex(SlotIndex);
}

void UGameXXKMainMenuWidget::HandleDeleteSlotClicked(int32 SlotIndex)
{
	RequestDeleteSlot(SlotIndex);
}
```

The programmatic row buttons are intentionally not dynamically bound per slot in this pass because Unreal dynamic delegates cannot bind an extra `int32` parameter directly. The behavior API remains fully testable, and Blueprint subclasses can bind row actions with the slot index.

- [ ] **Step 4: Run test and verify GREEN for widget behavior**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoLiveCoding -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.MainMenuPlayerFlow;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: `GameXXK.MVP.UI.MainMenuPlayerFlow` passes except for HUD integration assertions that require Task 4 if those assertions were added in Task 1.

- [ ] **Step 5: Commit the programmatic main menu widget**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp
git commit -m "feat: build clean player main menu widget"
```

---

### Task 4: HUD Integration And Debug Shell Gating

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKMVPHUD.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`

- [ ] **Step 1: Add HUD fields and accessors**

In `Source/GameXXK/Public/UI/GameXXKMVPHUD.h`, add a forward declaration:

```cpp
class UGameXXKMainMenuWidget;
```

Add public test methods near the existing widget accessors:

```cpp
	UGameXXKMainMenuWidget* CreateMainMenuWidgetForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasMainMenuWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKMainMenuWidget* GetMainMenuWidget() const;

	void SetDebugPlayableShellEnabledForTest(bool bEnabled);
	bool IsDebugPlayableShellEnabledForTest() const;
```

Add private declarations and properties:

```cpp
	UGameXXKMainMenuWidget* CreateMainMenuWidget();
	void RefreshMainMenuWidget();
	bool ShouldDrawDebugPlayableShell() const;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	bool bShowDebugPlayableShell = false;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMainMenuWidget> MainMenuWidget;
```

- [ ] **Step 2: Implement HUD main menu creation**

In `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`, add:

```cpp
#include "UI/GameXXKMainMenuWidget.h"
```

Update the constructor:

```cpp
AGameXXKMVPHUD::AGameXXKMVPHUD()
{
	MainMenuWidgetClass = UGameXXKMainMenuWidget::StaticClass();
	PlayableRootWidgetClass = UGameXXKPlayableRootWidget::StaticClass();
	RouteMapWidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
	BattleBoardWidgetClass = UGameXXKBattleBoardWidget::StaticClass();
}
```

At the start of `BeginPlay()`, create the player menu. Keep the debug root creation behind `bShowDebugPlayableShell`:

```cpp
	if (UGameXXKMainMenuWidget* MenuWidget = CreateMainMenuWidget())
	{
		if (!MenuWidget->IsInViewport())
		{
			MenuWidget->AddToViewport(20);
		}
		MenuWidget->RefreshFromState();
	}

	if (bShowDebugPlayableShell)
	{
		if (UGameXXKPlayableRootWidget* RootWidget = CreatePlayableRootWidget())
		{
			if (!RootWidget->IsInViewport())
			{
				RootWidget->AddToViewport(30);
			}
		}
	}
```

Remove the old unconditional `CreatePlayableRootWidget()` block from `BeginPlay()`.

- [ ] **Step 3: Implement HUD accessors and refresh**

Add these functions near the existing widget accessors:

```cpp
UGameXXKMainMenuWidget* AGameXXKMVPHUD::CreateMainMenuWidgetForTest()
{
	return CreateMainMenuWidget();
}

bool AGameXXKMVPHUD::HasMainMenuWidget() const
{
	return MainMenuWidget != nullptr;
}

UGameXXKMainMenuWidget* AGameXXKMVPHUD::GetMainMenuWidget() const
{
	return MainMenuWidget;
}

void AGameXXKMVPHUD::SetDebugPlayableShellEnabledForTest(bool bEnabled)
{
	bShowDebugPlayableShell = bEnabled;
}

bool AGameXXKMVPHUD::IsDebugPlayableShellEnabledForTest() const
{
	return bShowDebugPlayableShell;
}

UGameXXKMainMenuWidget* AGameXXKMVPHUD::CreateMainMenuWidget()
{
	if (MainMenuWidget)
	{
		return MainMenuWidget;
	}

	TSubclassOf<UGameXXKMainMenuWidget> WidgetClass = MainMenuWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKMainMenuWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		MainMenuWidget = CreateWidget<UGameXXKMainMenuWidget>(PlayerController, WidgetClass);
	}
	if (!MainMenuWidget)
	{
		MainMenuWidget = NewObject<UGameXXKMainMenuWidget>(this, WidgetClass);
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		MainMenuWidget->RefreshFromState();
	}
	return MainMenuWidget;
}

void AGameXXKMVPHUD::RefreshMainMenuWidget()
{
	if (MainMenuWidget)
	{
		MainMenuWidget->RefreshFromState();
	}
}

bool AGameXXKMVPHUD::ShouldDrawDebugPlayableShell() const
{
	return bShowDebugPlayableShell;
}
```

Update `SetMVPSubsystemForTest()` to refresh `MainMenuWidget`:

```cpp
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(InSubsystem);
		MainMenuWidget->RefreshFromState();
	}
```

Update `HandleDemoCommand()` to call `RefreshMainMenuWidget()` before refreshing route and battle widgets.

Update `DrawHUD()` so the old Canvas shell is skipped by default:

```cpp
	Super::DrawHUD();
	RefreshAuxiliaryWidgets();
	RefreshMainMenuWidget();
	if (!Canvas || !ShouldDrawDebugPlayableShell())
	{
		return;
	}
```

- [ ] **Step 4: Run HUD-focused tests**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoLiveCoding -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.MainMenuPlayerFlow;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: `GameXXK.MVP.UI.MainMenuPlayerFlow` passes, including HUD assertions.

- [ ] **Step 5: Commit HUD integration**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKMVPHUD.h Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp
git commit -m "feat: show player main menu by default"
```

---

### Task 5: Regression Suite And Manual PIE Check

**Files:**
- Read: `docs/superpowers/specs/2026-07-03-main-menu-player-flow-design.md`
- Verify: `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`
- Verify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Verify: `Source/GameXXK/Private/Tests/GameXXKPlayableRootWidgetTest.cpp`

- [ ] **Step 1: Run targeted automation**

Run:

```powershell
$tests = @(
  'GameXXK.MVP.UI.MainMenuPlayerFlow',
  'GameXXK.MVP.UI.WidgetBasesDriveRules',
  'GameXXK.MVP.PIE.MainMenuContinueWorldMap',
  'GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop'
)
foreach ($test in $tests) {
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests $test;Quit" '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
  if ($LASTEXITCODE -ne 0) { throw "Automation failed: $test" }
}
```

Expected: all four tests complete with `Result={Success}`.

- [ ] **Step 2: Run whitespace and status checks**

Run:

```powershell
git diff --check -- Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp Source/GameXXK/Public/UI/GameXXKMVPHUD.h Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
git status --short -- Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp Source/GameXXK/Public/UI/GameXXKMVPHUD.h Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
```

Expected: `git diff --check` prints no whitespace errors. `git status` lists only the intended files for this plan plus any pre-existing unrelated project changes outside this file list.

- [ ] **Step 3: Manual PIE visual check**

Run the editor:

```powershell
Start-Process -FilePath 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList '"D:\UE5 demo\GameXXK\GameXXK.uproject"'
```

In PIE, verify these visible states:

- Startup shows only `GameXXK`, `New Game`, `Continue`, `Options`, and `Quit`.
- The old left-side `GameXXK MVP Playable Shell` command panel is not visible.
- Clicking `Continue` opens a centered modal with five one-line slots.
- Empty slots are disabled.
- Clicking `Delete` on an occupied slot opens a small confirmation modal.
- `Cancel` returns to the Continue modal without deleting the slot.
- `Options` opens an unavailable-options modal with a close action.
- `Quit` opens an unavailable-in-editor modal with a close action.
- `New Game` enters the Qingshan town runtime state.

- [ ] **Step 4: Commit final verification notes if docs changed**

If implementation adds verification notes to an existing plan or docs file, commit only those docs and the implementation files:

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp Source/GameXXK/Public/UI/GameXXKMVPHUD.h Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp docs/superpowers/plans/2026-07-03-main-menu-player-flow.md
git commit -m "test: verify main menu player flow"
```

If no docs changed after Task 4, skip this commit.

---

## Self-Review

- Spec coverage: the plan covers clean landing actions, center Continue modal, compact save rows, small delete confirmation modal, Options unavailable modal, Quit unavailable modal, hiding debug shell from the player flow, and preserving debug/automation access.
- Scope check: this plan does not redesign town, route map, battle, pause, or save-in-game UI.
- Type consistency: `EGameXXKMainMenuLayer`, `FGameXXKMainMenuSaveSlotRow`, `BuildLandingActionsForTest`, `BuildSaveSlotRowsForTest`, and HUD debug-shell accessors are introduced before tests rely on them.
- Red-flag scan: no marker text or unspecified implementation steps are left in the plan.

