# Tutorial Prologue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the playable carriage-to-river prologue in the existing town, including post-carriage naming, the scroll choice, YueBai dialogue, one-time rewards and safe resume/exit.

**Architecture:** A tutorial coordinator snapshots the ordinary town and binds stable narrative markers/actor aliases. Typed dialogue executors drive PaperZD/flipbook actors, camera, VFX, naming and gameplay mutations. The full story lives in `Dialogue.Tutorial.001.dialogue.json`; the coordinator restores the town on completion, pause or required-command failure.

**Tech Stack:** UE 5.8 C++, Paper2D/PaperZD, GameXXK Dialogue core, UMG, project UE MCP scripts, JSON importer, Automation and real PIE harnesses.

---

## File map

- Create `Content/Python/gamexxk_import_prologue_animation_flipbooks.py` — deterministic prologue flipbooks from approved atlases.
- Create `/Game/GameXXK/Cinematics/Prologue/Flipbooks/*` and sprite assets — generated UE assets, not hand-edited.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeMarker.h` and private `.cpp` — stable marker actor.
- Create `Source/GameXXK/Public/Narrative/GameXXKTutorialPrologueCoordinator.h` and private `.cpp` — snapshot/binding/restore.
- Create `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueTownExecutor.h` and private `.cpp`.
- Create `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueCameraExecutor.h` and private `.cpp`.
- Create `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialoguePresentationExecutor.h` and private `.cpp`.
- Create `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueGameplayExecutor.h` and private `.cpp`.
- Create `Source/GameXXK/Public/UI/GameXXKHeroNamingWidget.h` and private `.cpp`.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h` — persistent player display name and river-map item.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` and private `.cpp` — v29 player identity migration.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` and private `.cpp` — idempotent tutorial reward/objective transaction.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` and private `.cpp` — story entry/resume and coordinator ownership.
- Create `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.001.dialogue.json` — full approved prologue.
- Create `Content/Python/gamexxk_install_tutorial_river_markers.py` — targeted marker/actor placement in the existing town.
- Create `Content/Python/gamexxk_probe_tutorial_prologue.py` — live state/action probe.
- Create `scripts/run_tutorial_prologue_pie.py` — complete real-flow harness.
- Create `Source/GameXXK/Private/Tests/GameXXKDialogueCommandExecutorTest.cpp`.
- Create `Source/GameXXK/Private/Tests/GameXXKHeroNamingWidgetTest.cpp`.
- Create `Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp`.

### Task 1: Create runtime flipbooks for approved prologue atlases

**Files:**
- Create: `Content/Python/gamexxk_import_prologue_animation_flipbooks.py`
- Modify: `scripts/test_animation_upgrade_runtime_import.py`
- Generate: `Content/GameXXK/Cinematics/Prologue/Flipbooks/`
- Generate: `Content/GameXXK/Cinematics/Prologue/Sprites/`

- [ ] **Step 1: Add a failing asset contract**

Expect these flipbooks with 60 frames, manifest FPS and explicit loop policy:

```text
FB_Prologue_Horse_Idle                 loop
FB_Prologue_Horse_StartRunStop         once
FB_Prologue_Carriage_RunStop           once
FB_Prologue_Carriage_PostStopIdle      loop
FB_Prologue_YueBai_Intro               once
FB_Prologue_YueBai_Idle                loop
FB_Prologue_YueBai_Outro               once
```

The test must also assert that no deer-bow source appears in the importer.

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_animation_upgrade_runtime_import -v
```

Expected: missing prologue flipbook importer/contracts.

- [ ] **Step 3: Implement deterministic import**

Read the existing `runtime-import-report.json`, bind each approved 2K atlas, create 256×256 bottom-center sprites on the 8×8 grid, set manifest FPS, and save only Prologue packages. Write `Saved/HarnessReports/animation-upgrade-20260827-corrected/prologue-flipbook-report.json` with frame count, FPS, source hash and asset path.

- [ ] **Step 4: Verify art assets**

Run the importer through UE MCP with PIE stopped. Verify all assets exist, every flipbook has 60 keyframes, all source atlas outer alpha edges are zero, and render a contact sheet for visual review.

- [ ] **Step 5: Commit**

```powershell
git add -- Content/Python/gamexxk_import_prologue_animation_flipbooks.py scripts/test_animation_upgrade_runtime_import.py Content/GameXXK/Cinematics/Prologue/Flipbooks Content/GameXXK/Cinematics/Prologue/Sprites
git commit -m "feat: assemble prologue animation flipbooks"
```

### Task 2: Add narrative markers and the town snapshot coordinator

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeMarker.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKNarrativeMarker.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKTutorialPrologueCoordinator.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKTutorialPrologueCoordinator.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp`

- [ ] **Step 1: Write failing binding/snapshot tests**

Build a transient world with unique markers and assert alias binding, duplicate rejection, snapshot and restore:

```cpp
TestTrue(TEXT("all required markers bind"), Coordinator->BindScene(World, Error));
TestEqual(TEXT("hero spawn equals carriage stop"),
    Coordinator->ResolveMarker(TEXT("Tutorial.River.CarriageStop")),
    Coordinator->ResolveMarker(TEXT("Tutorial.River.HeroSpawn")));
Coordinator->CaptureTownState();
Coordinator->HideOrdinaryTown();
Coordinator->RestoreTownState();
TestFalse(TEXT("input restored"), Coordinator->IsInputLockedForTest());
```

- [ ] **Step 2: Run RED**

Expected missing marker/coordinator types.

- [ ] **Step 3: Implement exact marker contract**

Require unique IDs:

```text
Tutorial.River.CarriageEntry
Tutorial.River.CarriageStop
Tutorial.River.HeroSpawn
Tutorial.River.CarriageExit
Tutorial.River.ScrollSpawn
Tutorial.River.YueBaiSpawn
Tutorial.River.YueBaiAdvance
Tutorial.River.TownRelease
```

`HeroSpawn` and `CarriageStop` may occupy the same transform but remain separate IDs. Snapshot player transform, camera/view target, HUD visibility, hidden state of ordinary NPCs and input mode. Restore is idempotent and callable after partial startup.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative Source/GameXXK/Private/Narrative Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp
git commit -m "feat: coordinate tutorial river scene state"
```

### Task 3: Implement typed town, camera and presentation executors

**Files:**
- Create: `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueTownExecutor.h`
- Create: `Source/GameXXK/Private/Dialogue/Executors/GameXXKDialogueTownExecutor.cpp`
- Create: `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueCameraExecutor.h`
- Create: `Source/GameXXK/Private/Dialogue/Executors/GameXXKDialogueCameraExecutor.cpp`
- Create: `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialoguePresentationExecutor.h`
- Create: `Source/GameXXK/Private/Dialogue/Executors/GameXXKDialoguePresentationExecutor.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDialogueCommandExecutorTest.cpp`

- [ ] **Step 1: Write failing command tests**

Cover `spawnActor`, `showActor`, `hideActor`, `setFacing`, `moveToMarker`, `moveRelativeUnits`, `playAction`, `restoreIdle`, `cameraLock`, `cameraFocus`, `cameraRestore`, `screenFlash`, `playVfx`, `playSfx`, `showToast`. Assert unknown aliases/markers fail required commands and optional SFX failures complete with diagnostics.

- [ ] **Step 2: Implement asynchronous completion**

Movement and one-shot actions return `Pending` and complete through delegates/timers. A second command cannot overwrite a pending required command. `CancelPending` stops optional work and asks the coordinator to restore the captured town.

- [ ] **Step 3: Bind approved actions**

Use `AGameXXKHeroCharacter::PlayTownAction` for hero states, Prologue flipbooks for horse/carriage/YueBai, and the four approved hit/VFX atlases where referenced. Direction uses horizontal component mirroring; no new eight-direction state machine is introduced.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue/Executors Source/GameXXK/Private/Dialogue/Executors Source/GameXXK/Private/Tests/GameXXKDialogueCommandExecutorTest.cpp
git commit -m "feat: execute dialogue world presentation commands"
```

### Task 4: Add naming and persistent player identity

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKHeroNamingWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKHeroNamingWidget.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroNamingWidgetTest.cpp`

- [ ] **Step 1: Write failing name validation/migration tests**

Test default “小侠客”, edge trimming, 1–12 displayed Unicode characters, rejection of blank/control/newline input, one pending request, confirm/cancel behavior and v28→v29 default migration.

- [ ] **Step 2: Add persistent field and v29 migration**

Add `FString PlayerDisplayName = TEXT("小侠客")` to RuntimeState and `PlayerIdentityIntroducedSaveVersion = 29`. Normalize old/invalid names to the default without changing any visual asset.

- [ ] **Step 3: Build naming Widget/executor**

The naming Widget contains only title, editable text, validation message and confirm button. `openNaming` stays `Pending` until valid confirmation, commits `PlayerDisplayName` atomically, saves immediately, then resumes the dialogue. There are no appearance controls.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKHeroNamingWidget.h Source/GameXXK/Private/UI/GameXXKHeroNamingWidget.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKHeroNamingWidgetTest.cpp
git commit -m "feat: save player name during tutorial"
```

### Task 5: Add gameplay executor and one-time tutorial outcome

**Files:**
- Create: `Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueGameplayExecutor.h`
- Create: `Source/GameXXK/Private/Dialogue/Executors/GameXXKDialogueGameplayExecutor.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp`

- [ ] **Step 1: Write failing atomic/idempotent outcome tests**

Expect `grantItem(Item.Tutorial.RiverMap)`, `unlockQuestNpc(Npc.YueBai)` and `setTutorialStep(Tutorial.Main.Tiantai)` to commit once, survive reload and skip when command IDs are already recorded.

- [ ] **Step 2: Implement registered gameplay commands**

Add `UGameXXKMVPRules::ItemTutorialRiverMap()` and display name “河中旧图”. Add subsystem transaction `CommitTutorialDialogueCommand(Command, CommandId, OutError)` which clones RuntimeState, applies the typed mutation, normalizes inventory/owned NPC state, validates save state, records the command ID and commits once.

- [ ] **Step 3: Preserve old quest separation**

The completion command updates only `TutorialQuest` and the tutorial main objective. It must not mutate `QuestState`, `TrackedTaskId` to `Task.QingshanMain`, or `bFollowerJoined`.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue/Executors/GameXXKDialogueGameplayExecutor.h Source/GameXXK/Private/Dialogue/Executors/GameXXKDialogueGameplayExecutor.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp
git commit -m "feat: commit tutorial dialogue rewards once"
```

### Task 6: Author/import the complete prologue and place the town scene

**Files:**
- Create: `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.001.dialogue.json`
- Create: `Content/Python/gamexxk_install_tutorial_river_markers.py`
- Modify: `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap` through targeted UE MCP only.
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Write the complete JSON from the approved story**

Encode every line from `docs/design/2026-08-27-tutorial-prologue-story.md`, including the salvage choice, YueBai reveal, all movement/action annotations, two toasts and final reward/objective commands. Opening command order must be exactly:

```text
captureTown → lockInput → hideTownUiAndNpcs
→ spawn Horse/Carriage at CarriageEntry
→ move carriage to CarriageStop → play stop/idle
→ spawn Hero at HeroSpawn
→ move carriage to CarriageExit → hide Horse/Carriage
→ openNaming
→ begin river notice/scroll mainline
```

- [ ] **Step 2: Validate/import JSON before map mutation**

Run pure validator; expected 0 errors. Import through `gamexxk_import_dialogue_json.py`; expected asset `/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_001` and an import report with every referenced resource resolved.

- [ ] **Step 3: Install markers with exact ownership checks**

The UE Python script loads only `L_Qingshan_AsianVillage_Demo`, creates/updates actors labelled `GameXXK_TutorialRiver_<Id>`, refuses duplicate unowned labels, never moves unrelated actors, and saves only that map after reporting before/after transforms. Inspect the map visually before accepting the save.

- [ ] **Step 4: Route the desktop story entry**

`RequestDesktopTutorialQuestFromWorkbench` enters the town, asks the coordinator to resume an active tutorial session or start `Dialogue.Tutorial.001`, and never opens the old QuestDialog.

- [ ] **Step 5: Commit**

```powershell
git add -- SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.001.dialogue.json Content/Python/gamexxk_install_tutorial_river_markers.py Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Content/GameXXK/Narrative/Dialogues
git commit -m "feat: author river scroll tutorial prologue"
```

### Task 7: Real PIE acceptance and cold build gate

**Files:**
- Create: `Content/Python/gamexxk_probe_tutorial_prologue.py`
- Create: `scripts/run_tutorial_prologue_pie.py`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp`

- [ ] **Step 1: Implement phase-based live probe**

Expose read-only state plus bounded actions: click story entry, submit name, advance, choose, pause, exit, resume. Report map, node, actor aliases/actions/transforms, input lock, UI visibility, reward IDs and tutorial step.

- [ ] **Step 2: Implement full real-flow runner**

Start from `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, enter through the real `剧情任务` button, capture these checkpoints:

```text
carriage_moving
carriage_stopped
hero_spawned
carriage_gone_before_naming
naming_open
scroll_choice
yuebai_intro
dialogue_midpoint
reward_complete
town_restored
```

Repeat with exits at three different node boundaries and verify no repeated item/YueBai/objective.

- [ ] **Step 3: Run final automated gates**

Run `GameXXK.Dialogue`, `GameXXK.MVP.Town`, `GameXXK.DesktopTraining.Workbench.TutorialQuestActivation`, save migration, Editor Target and Game Target cold builds. Expected 0 failures/errors.

- [ ] **Step 4: Leave correct PIE for review**

Restore `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start PIE, verify the map name through MCP, and leave it running for the user.

- [ ] **Step 5: Commit harness**

```powershell
git add -- Content/Python/gamexxk_probe_tutorial_prologue.py scripts/run_tutorial_prologue_pie.py Source/GameXXK/Private/Tests/GameXXKTutorialPrologueTest.cpp
git commit -m "test: verify tutorial prologue real flow"
```

