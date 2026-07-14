# Town Quest Dialog First Interface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace immediate quest acceptance at the town quest NPC with a reference-styled, clickable task dialogue that preserves the existing quest, follower, location-recording, and save behavior.

**Architecture:** `AGameXXKMVPPlayerController` owns one `UGameXXKQuestDialogWidget`, opens it for the quest NPC that received `F`, and owns the pending NPC/pawn context. The widget owns only presentation and button clicks; the controller asks the original NPC to execute its existing acceptance path, so follower activation and persisted NPC location remain where they already work. Both legacy `AGameXXKTownNpcActor` and the placed `AGameXXKTownNpcCharacter` use the same controller entry point.

**Tech Stack:** Unreal Engine 5.8 C++, programmatic UMG, cropped Tencent Docs reference textures, UE MCP asset import/save, UE Automation Tests, PIE/MCP flow verification.

---

## File map

| File | Responsibility |
| --- | --- |
| `Content/Python/gamexxk_import_quest_dialog_ui_assets.py` | Imports the cropped Tencent Docs dialogue frame and the two button states as never-streamed UI textures. |
| `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h` | Declares the dialogue presentation API, its test-visible state, and widget callbacks. |
| `Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp` | Builds the paper dialogue layout, binds Accept/Leave, and delegates game decisions to its owner controller. |
| `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` | Exposes dialogue open/accept/cancel commands and owns the pending NPC/pawn context. |
| `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` | Creates the dialogue at a higher viewport layer, applies modal focus, routes acceptance back to the original NPC, and restores town input when closed. |
| `Source/GameXXK/Public/Town/GameXXKTownNpcActor.h` | Declares a shared public confirmation method for the actor-form quest NPC. |
| `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp` | Opens the dialogue on initial `F` and only performs the old quest side effects after confirmation. |
| `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h` | Declares the matching confirmation method for the placed Character-form quest NPC. |
| `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp` | Opens the dialogue on initial `F` and only performs the old quest side effects after confirmation. |
| `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp` | Verifies controller ownership, open/cancel visibility, and modal state without requiring PIE. |
| `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp` | Verifies dialogue fallback acceptance still reaches `UGameXXKMVPSubsystem::AcceptQuest()` for isolated widget tests. |

### Task 1: Create the dialogue-controller test contract

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets`

- [ ] **Step 1: Add the failing assertions immediately after the existing inventory-window ownership assertion.**

```cpp
TestNotNull(TEXT("player controller owns quest dialogue widget"), PlayerController->GetQuestDialogWidgetForTest());
TestFalse(TEXT("quest dialogue starts closed"), PlayerController->IsQuestDialogOpenForTest());
TestTrue(TEXT("controller can open an empty preview quest dialogue for visual test"), PlayerController->OpenQuestDialogPreviewForTest());
TestTrue(TEXT("quest dialogue reports open after preview command"), PlayerController->IsQuestDialogOpenForTest());
TestTrue(TEXT("quest dialogue is modal while it is visible"), PlayerController->IsQuestDialogModalInputLockedForTest());
TestTrue(TEXT("controller cancel closes quest dialogue"), PlayerController->CloseQuestDialog());
TestFalse(TEXT("quest dialogue reports closed after cancel"), PlayerController->IsQuestDialogOpenForTest());
TestFalse(TEXT("quest dialogue restores town input after cancel"), PlayerController->IsQuestDialogModalInputLockedForTest());

AGameXXKTownNpcActor* QuestNpc = NewObject<AGameXXKTownNpcActor>();
AGameXXKTownPlayerPawn* QuestPawn = NewObject<AGameXXKTownPlayerPawn>();
QuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
QuestNpc->SetMVPSubsystemForTest(Subsystem);
TestTrue(TEXT("F quest path opens a dialogue before it mutates quest state"), PlayerController->OpenQuestDialogForNpc(QuestNpc, QuestPawn));
TestEqual(TEXT("opening dialogue does not accept quest yet"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Available);
TestTrue(TEXT("accept button resolves the original quest NPC path"), PlayerController->AcceptQuestDialog());
TestEqual(TEXT("accept button marks quest accepted"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
TestTrue(TEXT("accept button preserves follower activation"), QuestNpc->IsFollowerActive());
TestTrue(TEXT("accept button preserves follower target"), QuestNpc->GetFollowTarget() == QuestPawn);
```

- [ ] **Step 2: Run the focused automation test and confirm it fails to compile because the quest-dialog controller API does not exist.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: UBT fails with unresolved `GetQuestDialogWidgetForTest`, `OpenQuestDialogPreviewForTest`, `IsQuestDialogOpenForTest`, and `IsQuestDialogModalInputLockedForTest` symbols.

- [ ] **Step 3: Add the complete public test/flow API to `AGameXXKMVPPlayerController`.**

```cpp
class UGameXXKQuestDialogWidget;

UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
UGameXXKQuestDialogWidget* GetQuestDialogWidgetForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
bool IsQuestDialogOpenForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
bool IsQuestDialogModalInputLockedForTest() const;

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
bool OpenQuestDialogPreviewForTest();

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
bool OpenQuestDialogForNpc(AActor* QuestNpc, APawn* InstigatorPawn);

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
bool AcceptQuestDialog();

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
bool CloseQuestDialog();
```

Add private declarations and state:

```cpp
UGameXXKQuestDialogWidget* EnsureQuestDialogWidget();
bool ConfirmPendingQuestNpc();

UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
TSubclassOf<UGameXXKQuestDialogWidget> QuestDialogWidgetClass;

UPROPERTY(Transient)
TObjectPtr<UGameXXKQuestDialogWidget> QuestDialogWidget;

UPROPERTY(Transient)
TWeakObjectPtr<AActor> PendingQuestNpc;

UPROPERTY(Transient)
TWeakObjectPtr<APawn> PendingQuestInstigator;
```

- [ ] **Step 4: Implement controller-owned creation and close-only preview behavior.**

Add `#include "UI/GameXXKQuestDialogWidget.h"`, initialize `QuestDialogWidgetClass = UGameXXKQuestDialogWidget::StaticClass();` in the constructor, and call `EnsureQuestDialogWidget()` from `EnsurePlayerFlowWidgets()`.

```cpp
UGameXXKQuestDialogWidget* AGameXXKMVPPlayerController::EnsureQuestDialogWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	if (!QuestDialogWidget)
	{
		const TSubclassOf<UGameXXKQuestDialogWidget> WidgetClass = QuestDialogWidgetClass
			? QuestDialogWidgetClass
			: UGameXXKQuestDialogWidget::StaticClass();
		QuestDialogWidget = bCanAddToViewport
			? CreateWidget<UGameXXKQuestDialogWidget>(this, WidgetClass)
			: NewObject<UGameXXKQuestDialogWidget>(this, WidgetClass);
	}
	if (QuestDialogWidget)
	{
		QuestDialogWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		QuestDialogWidget->CloseDialog();
		if (bCanAddToViewport && !QuestDialogWidget->IsInViewport())
		{
			QuestDialogWidget->AddToViewport(160);
		}
	}
	return QuestDialogWidget;
}

bool AGameXXKMVPPlayerController::OpenQuestDialogPreviewForTest()
{
	if (UGameXXKQuestDialogWidget* Dialog = EnsureQuestDialogWidget())
	{
		Dialog->OpenDialog();
		ApplyPlayerFlowInputMode();
		return true;
	}
	return false;
}

bool AGameXXKMVPPlayerController::CloseQuestDialog()
{
	if (!QuestDialogWidget || !QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}
	QuestDialogWidget->CloseDialog();
	PendingQuestNpc.Reset();
	PendingQuestInstigator.Reset();
	ApplyPlayerFlowInputMode();
	return true;
}
```

`IsQuestDialogModalInputLockedForTest()` returns `QuestDialogWidget && QuestDialogWidget->IsDialogOpen()`. Add Esc handling before inventory close: `if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen()) { return CloseQuestDialog(); }`.

- [ ] **Step 5: Run the focused test.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: PASS after the widget API exists in Task 2; until then the only permitted failure is compilation against missing widget methods.

### Task 2: Build the reference-styled dialogue widget and import its three source crops

**Files:**
- Create: `Content/Python/gamexxk_import_quest_dialog_ui_assets.py`
- Modify: `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp`
- Test: `GameXXK.MVP.UI.WidgetCommands`

- [ ] **Step 1: Capture the single Tencent Docs upper-panel reference at 1920×1080 and save it as `docs/reference-assets/2026-07-14-tencent-town-ui.png`.**

Use the signed-in Tencent Docs page `https://docs.qq.com/doc/DTW5XRVRybHBBVmNW`. Capture the frame containing the parchment panels, ink borders, and green/ochre button language. Do not capture or import the lower `江湖行` cards.

- [ ] **Step 2: Crop exactly these UI atoms from that source image.**

| Output PNG | Required visual content | Import destination |
| --- | --- | --- |
| `docs/reference-assets/quest_dialog_frame.png` | Empty parchment/ink panel with no text or character portrait, enough border margin for 9-slice stretching | `/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogFrame` |
| `docs/reference-assets/quest_dialog_accept.png` | Green-teal primary button with no printed label | `/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogAccept` |
| `docs/reference-assets/quest_dialog_leave.png` | Ochre/neutral secondary button with no printed label | `/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialogLeave` |

The crop script must set `CompressionSettings = TC_EditorIcon`, `MipGenSettings = TMGS_NoMipmaps`, `NeverStream = true`, and `SRGB = true` for every imported texture.

- [ ] **Step 3: Create the exact import script.**

```python
import os
import unreal

PROJECT_ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.normpath(os.path.join(PROJECT_ROOT, "docs", "reference-assets"))
IMPORTS = {
    "quest_dialog_frame.png": "/Game/GameXXK/UI/QuestDialog/Textures",
    "quest_dialog_accept.png": "/Game/GameXXK/UI/QuestDialog/Textures",
    "quest_dialog_leave.png": "/Game/GameXXK/UI/QuestDialog/Textures",
}

for filename, destination in IMPORTS.items():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE_ROOT, filename)
    task.destination_path = destination
    task.destination_name = "T_" + os.path.splitext(filename)[0].replace("quest_dialog_", "QuestDialog").title().replace("_", "")
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(destination + "/" + task.destination_name)
    if asset:
        asset.compression_settings = unreal.TextureCompressionSettings.TC_EDITOR_ICON
        asset.mip_gen_settings = unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
        asset.never_stream = True
        asset.srgb = True
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
```

- [ ] **Step 4: Extend `UGameXXKQuestDialogWidget` with explicit presentation state.**

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
void OpenDialog();

UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
bool CloseDialog();

UFUNCTION(BlueprintPure, Category = "GameXXK|Quest|Test")
bool IsDialogOpen() const;

virtual void NativeConstruct() override;

private:
	void BuildProgrammaticLayout();
	void HandleAcceptClicked();
	void HandleLeaveClicked();
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> QuestTitleText;
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> QuestBodyText;
	UPROPERTY(Transient) TObjectPtr<class UButton> AcceptButton;
	UPROPERTY(Transient) TObjectPtr<class UButton> LeaveButton;
	bool bDialogOpen = false;
```

Use a full-screen `UCanvasPanel`: a 0.48-alpha black backdrop, a center-aligned 920×440 panel at screen center, a `UImage` brush from `T_QuestDialogFrame`, `任务委托` as the live title, `青山镇北门似有异动，随我去看看。` as the live preview body, and two 220×64 buttons labelled `接受` and `离开`. `OpenDialog()` sets `Visible`; `CloseDialog()` sets `Collapsed`; `NativeConstruct()` builds the layout then closes it.

- [ ] **Step 5: Route button handlers without duplicating game state.**

```cpp
void UGameXXKQuestDialogWidget::HandleAcceptClicked()
{
	if (AGameXXKMVPPlayerController* Controller = ResolveMVPPlayerController())
	{
		Controller->AcceptQuestDialog();
		return;
	}
	AcceptQuest();
	CloseDialog();
}

void UGameXXKQuestDialogWidget::HandleLeaveClicked()
{
	if (AGameXXKMVPPlayerController* Controller = ResolveMVPPlayerController())
	{
		Controller->CloseQuestDialog();
		return;
	}
	CloseDialog();
}
```

Keep `AcceptQuest()` as the isolated-widget fallback and call `OnQuestAccepted()` only after success.

- [ ] **Step 6: Import assets through UE MCP and save dirty packages.**

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); c.require_connected(); print(c.run_project_python_file('Content/Python/gamexxk_import_quest_dialog_ui_assets.py')); print(c.save_dirty_packages())"
```

Expected: three `/Game/GameXXK/UI/QuestDialog/Textures/T_QuestDialog*` assets are imported and saved.

- [ ] **Step 7: Run the widget automation test.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.WidgetBasesDriveRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: PASS, including the existing isolated `quest dialog accepts quest` assertion.

### Task 3: Defer quest NPC side effects until the player confirms

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcActor.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `GameXXK.MVP.Town.ShellInputInteractionFollower`

- [ ] **Step 1: Add the same public method to both NPC headers.**

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
bool ConfirmQuestDialogInteraction(APawn* InstigatorPawn);
```

- [ ] **Step 2: Move the existing event and refresh tail into that method in both NPC implementations.**

```cpp
bool AGameXXKTownNpcCharacter::ConfirmQuestDialogInteraction(APawn* InstigatorPawn)
{
	bLastInteractionSuccessful = ApplyDefaultInteraction(InstigatorPawn);
	OnQuestInteract(InstigatorPawn);
	OnDefaultInteractionResolved(InstigatorPawn, bLastInteractionSuccessful);
	if (bLastInteractionSuccessful)
	{
		if (AGameXXKMVPPlayerController* PlayerController = InstigatorPawn ? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()) : nullptr)
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
	return bLastInteractionSuccessful;
}
```

`AGameXXKTownNpcActor::ConfirmQuestDialogInteraction` has the same body with its actor class name. Leave `ApplyDefaultInteraction()` unchanged so follower activation and `RecordQuestNpcLocation()` remain exactly where they are.

- [ ] **Step 3: Change only a quest NPC's initial interaction.**

At the beginning of each `Interact_Implementation`:

```cpp
if (CanOfferQuest())
{
	if (AGameXXKMVPPlayerController* PlayerController = InstigatorPawn ? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()) : nullptr)
	{
		if (PlayerController->OpenQuestDialogForNpc(this, InstigatorPawn))
		{
			bLastInteractionSuccessful = true;
			return;
		}
	}
	return ConfirmQuestDialogInteraction(InstigatorPawn);
}
```

Keep the current merchant/generic branch exactly as it is after this early return. The final return is deliberately the old direct acceptance path: headless automation and legacy NPC calls without an `AGameXXKMVPPlayerController` must continue to accept immediately, activate the follower, and record its location.

- [ ] **Step 4: Implement the controller’s pending-context methods.**

```cpp
bool AGameXXKMVPPlayerController::OpenQuestDialogForNpc(AActor* QuestNpc, APawn* InstigatorPawn)
{
	if (!QuestNpc || !InstigatorPawn || !EnsureQuestDialogWidget())
	{
		return false;
	}
	PendingQuestNpc = QuestNpc;
	PendingQuestInstigator = InstigatorPawn;
	QuestDialogWidget->OpenDialog();
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::ConfirmPendingQuestNpc()
{
	APawn* InstigatorPawn = PendingQuestInstigator.Get();
	if (AGameXXKTownNpcCharacter* CharacterNpc = Cast<AGameXXKTownNpcCharacter>(PendingQuestNpc.Get()))
	{
		return CharacterNpc->ConfirmQuestDialogInteraction(InstigatorPawn);
	}
	if (AGameXXKTownNpcActor* ActorNpc = Cast<AGameXXKTownNpcActor>(PendingQuestNpc.Get()))
	{
		return ActorNpc->ConfirmQuestDialogInteraction(InstigatorPawn);
	}
	return false;
}

bool AGameXXKMVPPlayerController::AcceptQuestDialog()
{
	const bool bAccepted = ConfirmPendingQuestNpc();
	if (bAccepted && QuestDialogWidget)
	{
		QuestDialogWidget->OnQuestAccepted();
	}
	CloseQuestDialog();
	RefreshPlayerFlowWidgets();
	return bAccepted;
}
```

Add the required NPC headers to the controller cpp. In `ApplyPlayerFlowInputMode()`, when the town screen has an open quest dialogue, set widget focus to `QuestDialogWidget->TakeWidget()` instead of `SetAllUserFocusToGameViewport`; otherwise preserve the current town path exactly.

- [ ] **Step 5: Run the existing town-shell test.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: PASS. Existing direct `ApplyDefaultInteraction()` tests preserve the follower and saved quest-NPC location behavior.

### Task 4: Verify the real player-facing interaction in PIE

**Files:**
- No source changes.
- Verify: `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap`

- [ ] **Step 1: Save all dirty packages through UE MCP before the PIE run.**

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); c.require_connected(); print(c.save_dirty_packages())"
```

- [ ] **Step 2: Run the real playable-flow harness.**

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --map /Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo
```

Expected: the town opens with the normal HUD, approaching the placed quest NPC and pressing `F` opens the dialogue, `离开` or Esc returns control without accepting, and `接受` activates the existing follower/state path.

- [ ] **Step 3: Record the visual acceptance result before moving to HUD or inventory.**

Capture one 16:9 PIE screenshot with the dialogue visible. Accept it only if the border is readable at 1080p, both buttons have distinct hierarchy, no lower `江湖行` board appears, and the town scene remains visibly dimmed behind the panel.

- [ ] **Step 4: Commit only the dialogue source, import script, and focused tests.**

```powershell
git add Content/Python/gamexxk_import_quest_dialog_ui_assets.py Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/Town/GameXXKTownNpcActor.h Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp
git commit -m "feat: add town quest dialogue interface"
```

Do not add any unrelated dirty map, material, occlusion, character, or asset file.
