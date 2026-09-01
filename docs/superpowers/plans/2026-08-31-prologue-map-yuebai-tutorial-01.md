# Prologue Map, YueBai Companion, and Tutorial 0-1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Continue the accepted carriage preview through the inspectable Xu Xiake map, YueBai's first dialogue and non-blocking follower guidance, then enter and return from an isolated pure-2D tutorial 0-1 battle map.

**Architecture:** A map-placed `AGameXXKPrologueAftermathController` subscribes to the existing carriage completion delegate and owns only the post-carriage state machine, transient dialogue/UI presentation, YueBai reveal/follow state, passive statue prompt, and tutorial travel request. A new GameInstance tutorial-session subsystem snapshots ordinary runtime state across map travel, while a `TutorialBattleOnly` player-flow boot profile hosts the existing BattleBoard and a tutorial-local `Guide.Battle.Basic`; ordinary desktop, town, route, challenge, travel, reward, formation, and window paths stay unchanged.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Paper2D/atlas widgets, JSON-authored Dialogue assets, GameInstance subsystems, UE Automation Framework, focused Unreal Python through UE MCP, cold UBT, visual evidence review.

---

## Scope and repository guard

- Work directly in the root repository on `main`; do not create or use a worktree.
- Do not use UnrealBridge, Live Coding, Hot Reload, synthetic mouse/keyboard input, window hiding, automatic minimization, or per-frame viewport/window manipulation.
- Preserve every unrelated dirty change, especially battle/town animations, PaperZD assets, `L_Main.umap`, `L_DesktopTrainingHUD.umap`, inventory tuning, and user probes.
- Do not modify or replace the giant statue, authored NPC transforms, buildings, camera assets, HD2D planes, or existing YueBai animation source.
- These user-staged deletions must never enter a task commit:

```powershell
$ProtectedDeletions = @(
  'Content/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.uasset',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png',
  'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_BackpackScrollbarRight.png'
)
```

- Before every commit: unstage the three protected deletions, stage only named files/hunks, inspect `git diff --cached --name-status`, run `git diff --cached --check`, commit, then restore exactly those three staged deletions.
- If `.git/index.lock` exists, recover it only when it is 0 bytes, older than 60 seconds, and no writer process is active; move it to `.git/index.lock.stale-<timestamp>` rather than deleting it.
- Save dirty UE packages through MCP before any editor close/restart. Compile only with cold UBT/`scripts/ue_tdd_pipeline.py` and `-NoHotReload`.
- Pure asset copying/import and map assembly use deterministic validators rather than TDD; runtime behavior and gameplay changes use red-green TDD.

## File map

### New pure/runtime units

- `Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathTypes.h`
- `Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathRules.h`
- `Source/GameXXK/Private/Prologue/GameXXKPrologueAftermathRules.cpp`
- `Source/GameXXK/Public/Town/GameXXKPrologueAftermathController.h`
- `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- `Source/GameXXK/Public/UI/GameXXKPrologueMapWidget.h`
- `Source/GameXXK/Private/UI/GameXXKPrologueMapWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKPrologueYueBaiWidget.h`
- `Source/GameXXK/Private/UI/GameXXKPrologueYueBaiWidget.cpp`
- `Source/GameXXK/Public/MVP/GameXXKTutorial01SessionSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKTutorial01SessionSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKTutorial01GameMode.h`
- `Source/GameXXK/Private/MVP/GameXXKTutorial01GameMode.cpp`
- `Source/GameXXK/Public/Guide/GameXXKTutorial01GuideHost.h`
- `Source/GameXXK/Private/Guide/GameXXKTutorial01GuideHost.cpp`
- `Source/GameXXK/Public/UI/GameXXKTutorial01ResultWidget.h`
- `Source/GameXXK/Private/UI/GameXXKTutorial01ResultWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKInventoryItemPresentation.h`
- `Source/GameXXK/Private/UI/GameXXKInventoryItemPresentation.cpp`

### Dialogue and visual assets

- `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.CarriageNotice.dialogue.json`
- `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.YueBaiFirstMeeting.dialogue.json`
- `SourceAssets/Narrative/runtime-catalog.json`
- `SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.png`
- `Content/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.uasset`
- `Content/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_CarriageNotice.uasset`
- `Content/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_YueBaiFirstMeeting.uasset`

### Existing integration points

- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap`
- `Content/GameXXK/Maps/Tutorial/L_Tutorial_0_1.umap`

### Tests and guarded editor scripts

- `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPrologueMapWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTutorialMapItemTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTutorial01SessionTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTutorial01PlayerFlowTest.cpp`
- `scripts/test_prologue_aftercare_policy.py`
- `Content/Python/gamexxk_import_tutorial_route_map.py`
- `Content/Python/gamexxk_probe_tutorial_statue_anchor.py`
- `Content/Python/gamexxk_place_prologue_aftermath.py`
- `Content/Python/gamexxk_create_tutorial01_map.py`
- `Content/Python/gamexxk_validate_prologue_tutorial01.py`

---

### Task 1: Add the deterministic post-carriage state machine

**Files:**
- Create: `Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathTypes.h`
- Create: `Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathRules.h`
- Create: `Source/GameXXK/Private/Prologue/GameXXKPrologueAftermathRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathRulesTest.cpp`
- Create: `scripts/test_prologue_aftercare_policy.py`

- [ ] **Step 1: Write the compile-red phase and input tests**

Create a test named `GameXXK.Prologue.Aftermath.Rules` with these exact expectations:

```cpp
FGameXXKPrologueAftermathState State;
TestTrue(TEXT("aftermath starts at hero notice"),
    FGameXXKPrologueAftermathRules::Start(State));
TestEqual(TEXT("notice phase"), State.Phase,
    EGameXXKPrologueAftermathPhase::HeroNotice);

TestTrue(TEXT("notice completion opens thumbnail"),
    FGameXXKPrologueAftermathRules::ApplyEvent(
        EGameXXKPrologueAftermathEvent::DialogueCompleted, State));
TestEqual(TEXT("thumbnail phase"), State.Phase,
    EGameXXKPrologueAftermathPhase::MapThumbnail);

TestTrue(TEXT("inspect opens"),
    FGameXXKPrologueAftermathRules::ApplyEvent(
        EGameXXKPrologueAftermathEvent::OpenInspection, State));
TestEqual(TEXT("inspection phase"), State.Phase,
    EGameXXKPrologueAftermathPhase::MapInspection);
TestFalse(TEXT("space cannot leave inspection"),
    FGameXXKPrologueAftermathRules::ApplyEvent(
        EGameXXKPrologueAftermathEvent::ContinuePressed, State));
TestEqual(TEXT("inspection remains open"), State.Phase,
    EGameXXKPrologueAftermathPhase::MapInspection);
TestTrue(TEXT("close returns to thumbnail"),
    FGameXXKPrologueAftermathRules::ApplyEvent(
        EGameXXKPrologueAftermathEvent::CloseInspection, State));
TestTrue(TEXT("thumbnail space begins reveal"),
    FGameXXKPrologueAftermathRules::ApplyEvent(
        EGameXXKPrologueAftermathEvent::ContinuePressed, State));
TestEqual(TEXT("reveal phase"), State.Phase,
    EGameXXKPrologueAftermathPhase::YueBaiIntro);
```

Continue through these exact transitions:

```text
YueBaiIntro + YueBaiIntroCompleted → FoodDialogue
FoodDialogue + GuideDialogueStarted → GuideDialogue
GuideDialogue + DialogueCompleted → YueBaiFollowing
YueBaiFollowing + FollowerActivated → StatuePrompt
StatuePrompt + StatueInteracted → TutorialTravelPending
TutorialTravelPending + TutorialReturned → Finished
```

Then cover `SetPaused(State, true/false)`, cancel idempotency, and reject every out-of-order event. `ApplyEvent` must reject all non-cancel events while paused.

- [ ] **Step 2: Prove red with cold UBT**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120 --filter "[TDD]"
```

Expected: compilation fails on missing `FGameXXKPrologueAftermathState`, phase/event enums, and rules class.

- [ ] **Step 3: Implement the pure types and transition table**

Define exactly:

```cpp
UENUM(BlueprintType)
enum class EGameXXKPrologueAftermathPhase : uint8
{
    Dormant,
    HeroNotice,
    MapThumbnail,
    MapInspection,
    YueBaiIntro,
    FoodDialogue,
    GuideDialogue,
    YueBaiFollowing,
    StatuePrompt,
    TutorialTravelPending,
    Finished,
    Cancelled,
};

UENUM()
enum class EGameXXKPrologueAftermathEvent : uint8
{
    DialogueCompleted,
    OpenInspection,
    CloseInspection,
    ContinuePressed,
    YueBaiIntroCompleted,
    GuideDialogueStarted,
    FollowerActivated,
    StatueInteracted,
    TutorialReturned,
    Cancel,
};

USTRUCT(BlueprintType)
struct FGameXXKPrologueAftermathState
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EGameXXKPrologueAftermathPhase Phase = EGameXXKPrologueAftermathPhase::Dormant;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bPaused = false;
};
```

`ApplyEvent` must use an explicit `switch` over phase and accept only the transitions exercised by the test. `ContinuePressed` in `MapInspection` returns false without changing state. `Cancel` is legal from every nonterminal phase and idempotent in `Cancelled`.

The public rules API is fixed to:

```cpp
class FGameXXKPrologueAftermathRules final
{
public:
    static bool Start(FGameXXKPrologueAftermathState& InOutState);
    static bool ApplyEvent(
        EGameXXKPrologueAftermathEvent Event,
        FGameXXKPrologueAftermathState& InOutState);
    static void SetPaused(
        FGameXXKPrologueAftermathState& InOutState,
        bool bPaused);
    static bool IsBlockingPhase(EGameXXKPrologueAftermathPhase Phase);
};
```

- [ ] **Step 4: Add static policy tests**

`scripts/test_prologue_aftercare_policy.py` must scan only the new aftermath/tutorial files and reject `SendInput`, `SetWindowsHookEx`, `mouse_event`, `HideWindow`, `ShowWindow`, `bIdleStripFolded`, `OrderedFormation`, `StartTrainingChallenge`, and `GenerateChallengeRouteMap`.

- [ ] **Step 5: Run green gates and commit**

Run cold UBT, `GameXXK.Prologue.Aftermath.Rules`, and:

```powershell
python -m unittest scripts.test_prologue_aftercare_policy -v
```

Expected: one Automation test and the policy suite pass with zero failures.

Commit only Task 1 files as:

```text
feat: add prologue aftermath state rules
```

---

### Task 2: Author and import the two frozen Dialogue JSON assets

**Files:**
- Create: `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.CarriageNotice.dialogue.json`
- Create: `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.YueBaiFirstMeeting.dialogue.json`
- Modify: `SourceAssets/Narrative/runtime-catalog.json`
- Modify: `scripts/test_dialogue_json_validation.py`
- Create: `Content/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_CarriageNotice.uasset`
- Create: `Content/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_YueBaiFirstMeeting.uasset`

- [ ] **Step 1: Add failing source-contract tests**

Extend `scripts/test_dialogue_json_validation.py` to require both files, exact IDs, `Character.Hero`/`Npc.YueBai` speakers, and these terminal outcomes:

```python
self.assertEqual(
    payloads["Dialogue.Tutorial.CarriageNotice"]["nodes"]["end"]["outcomeId"],
    "Outcome.Tutorial.MapReady",
)
self.assertEqual(
    payloads["Dialogue.Tutorial.YueBaiFirstMeeting"]["nodes"]["end"]["outcomeId"],
    "Outcome.Tutorial.YueBaiFollowing",
)
```

Also assert the full ordered Chinese line list so later edits cannot silently rewrite approved dialogue.

- [ ] **Step 2: Run the Python test red**

```powershell
python -m unittest scripts.test_dialogue_json_validation -v
```

Expected: failure because the two source JSON files and catalog entries do not exist.

- [ ] **Step 3: Add catalog identities and exact JSON**

Add `Character.Hero` to `speakers`, both dialogue IDs to `dialogueIds`, and both outcomes to `outcomes`/`outcomeIds` in `runtime-catalog.json`.

Create `Dialogue.Tutorial.CarriageNotice.dialogue.json`:

```json
{
  "schemaVersion": 1,
  "dialogueId": "Dialogue.Tutorial.CarriageNotice",
  "dialogueVersion": 1,
  "entryNode": "notice",
  "nodes": {
    "notice": {
      "type": "line",
      "presentation": "dialogue",
      "speaker": "Character.Hero",
      "textId": "tutorial.carriage.notice",
      "text": "再往前就是天台山了……这是什么？",
      "next": "end"
    },
    "end": {"type": "end", "outcomeId": "Outcome.Tutorial.MapReady"}
  }
}
```

Create `Dialogue.Tutorial.YueBaiFirstMeeting.dialogue.json` exactly as:

```json
{
  "schemaVersion": 1,
  "dialogueId": "Dialogue.Tutorial.YueBaiFirstMeeting",
  "dialogueVersion": 1,
  "entryNode": "food.yuebai.hungry",
  "nodes": {
    "food.yuebai.hungry": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.food.yuebai.hungry",
      "text": "你……可有吃的？本座已经好几日没吃东西了。",
      "next": "food.hero.tease"
    },
    "food.hero.tease": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.food.hero.tease",
      "text": "你从地图里钻出来，第一件事就是讨饭？",
      "next": "food.yuebai.ask"
    },
    "food.yuebai.ask": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.food.yuebai.ask",
      "text": "……有便给些，没有便罢。",
      "next": "food.hero.offer"
    },
    "food.hero.offer": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.food.hero.offer",
      "text": "行了，拿去吧。就剩这点干粮，省着吃。",
      "next": "food.yuebai.thanks"
    },
    "food.yuebai.thanks": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.food.yuebai.thanks",
      "text": "多谢恩公。救命赠食之恩，本座定会报答。",
      "next": "guide.yuebai.destination"
    },
    "guide.yuebai.destination": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.guide.yuebai.destination",
      "text": "恩公欲往何处？",
      "next": "guide.hero.tiantai"
    },
    "guide.hero.tiantai": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.guide.hero.tiantai",
      "text": "天台山。听说前面有活干，运气好还能混口饭吃。",
      "next": "guide.yuebai.map"
    },
    "guide.yuebai.map": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.guide.yuebai.map",
      "text": "天台山……此图上正有一条路通往那里。",
      "next": "guide.hero.read"
    },
    "guide.hero.read": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.guide.hero.read",
      "text": "你看得懂这玩意？",
      "next": "guide.yuebai.yes"
    },
    "guide.yuebai.yes": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.guide.yuebai.yes",
      "text": "自然。",
      "next": "guide.hero.ask"
    },
    "guide.hero.ask": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.guide.hero.ask",
      "text": "那正好，我不识字。你替我认路，就算报恩了。",
      "next": "guide.yuebai.agree"
    },
    "guide.yuebai.agree": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.guide.yuebai.agree",
      "text": "……也罢。本座随你同行，替你指路。",
      "next": "guide.hero.arrangement"
    },
    "guide.hero.arrangement": {
      "type": "line", "presentation": "dialogue", "speaker": "Character.Hero",
      "textId": "tutorial.guide.hero.arrangement",
      "text": "那说好了，我在前面走，你在后面指。",
      "next": "guide.yuebai.reply"
    },
    "guide.yuebai.reply": {
      "type": "line", "presentation": "dialogue", "speaker": "Npc.YueBai",
      "textId": "tutorial.guide.yuebai.reply",
      "text": "……恩公倒是安排得明白。",
      "next": "end"
    },
    "end": {"type": "end", "outcomeId": "Outcome.Tutorial.YueBaiFollowing"}
  }
}
```

- [ ] **Step 4: Validate and import through UE MCP**

Run the Python validation test green, then call `gamexxk_import_dialogue_json.py` through `UnrealMCPClient.run_project_python_file` with both source paths and `--catalog SourceAssets/Narrative/runtime-catalog.json`.

Expected report: `ok=true`, `importedCount=2`, exact asset IDs and no unrelated dirty packages.

- [ ] **Step 5: Run Dialogue Automation and commit**

Run `GameXXK.Dialogue.Rules`, `GameXXK.Dialogue.Coordinator`, and the JSON validation suite. Commit only the source/catalog/test and two generated DataAssets as:

```text
feat: author YueBai first-meeting dialogue
```

---

### Task 3: Import the approved map image and build the reusable inspect widget

**Files:**
- Create: `SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.png`
- Create: `Content/Python/gamexxk_import_tutorial_route_map.py`
- Create: `Content/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.uasset`
- Create: `Source/GameXXK/Public/UI/GameXXKPrologueMapWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKPrologueMapWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueMapWidgetTest.cpp`
- Modify: `scripts/test_prologue_aftercare_policy.py`

- [ ] **Step 1: Copy and verify the exact approved source image**

Copy the user source from:

```text
C:/Users/shxuw/xwechat_files/wxid_g90er9r4o8p312_cd3c/temp/RWTemp/2026-08/1f0f6da890b579e80094c634191d562a/a7a21b7944259977d52cf78423a1af12.png
```

to `SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.png` without recompression. Require:

```text
size = 2388×1668
SHA256 = 3F4DEB047ABE7F73DD1A4EE4C29BFF527524B4B95ED153FC09080A73CC82782A
```

- [ ] **Step 2: Write the widget test before production code**

Create `GameXXK.Prologue.Aftermath.MapWidget` and require:

```cpp
UGameXXKPrologueMapWidget* Widget = NewObject<UGameXXKPrologueMapWidget>();
Widget->TakeWidget();
TestTrue(TEXT("thumbnail starts visible"), Widget->IsThumbnailVisibleForTest());
TestFalse(TEXT("inspection starts closed"), Widget->IsInspectionOpenForTest());
TestEqual(TEXT("story card has no title"), Widget->GetTitleTextForTest(), FText::GetEmpty());
TestTrue(TEXT("inspect button exists"), Widget->HasInspectButtonForTest());
TestTrue(TEXT("continue prompt exists"), Widget->HasContinuePromptForTest());
Widget->RequestInspectionForTest();
TestTrue(TEXT("inspection opens"), Widget->IsInspectionOpenForTest());
TestFalse(TEXT("space is blocked while inspecting"), Widget->RequestContinueForTest());
Widget->RequestCloseInspectionForTest();
TestFalse(TEXT("close returns to thumbnail"), Widget->IsInspectionOpenForTest());
TestTrue(TEXT("space continues from thumbnail"), Widget->RequestContinueForTest());
```

Also require the full-image brush path and task-icon path:

```text
/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect
/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap
```

- [ ] **Step 3: Prove compile red and implement the importer**

Cold-build to prove the missing widget. Then implement `gamexxk_import_tutorial_route_map.py` with one exact source/destination pair, source hash/dimension guards, `TextureGroup.UI`, sRGB enabled, no lossy source rewrite, and save only the imported texture package.

The importer constants and guard are fixed to:

```python
SOURCE = PROJECT_ROOT / "SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.png"
DESTINATION = "/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect"
EXPECTED_SHA256 = "3f4deb047abe7f73dd1a4ee4c29bff527524b4b95ed153fc09080a73cc82782a"
EXPECTED_SIZE = (2388, 1668)

if hashlib.sha256(SOURCE.read_bytes()).hexdigest() != EXPECTED_SHA256:
    raise RuntimeError("Xu Xiake route-map source hash drifted")
```

- [ ] **Step 4: Implement one responsive two-state widget**

`UGameXXKPrologueMapWidget` must expose delegates for inspect, close, and continue. Programmatic layout uses a 1920×1080 reference canvas, a landscape center thumbnail card, `检视`, `空格继续`, and an inspection paper sized by `min(860, ViewportHeight * 0.80)` with width derived from `2388 / 1668`. The close button is anchored at the inspection paper's top-right outer edge. Inspection consumes Space without firing Continue.

The same widget supports `StoryCard` and `InspectOnly` modes; inventory right-click opens directly in `InspectOnly` with no continue prompt.

The public API is fixed to:

```cpp
UENUM()
enum class EGameXXKPrologueMapMode : uint8 { StoryCard, InspectOnly };

DECLARE_DELEGATE(FGameXXKPrologueMapInspectRequested);
DECLARE_DELEGATE(FGameXXKPrologueMapCloseRequested);
DECLARE_DELEGATE(FGameXXKPrologueMapContinueRequested);

void Configure(EGameXXKPrologueMapMode Mode);
bool RequestInspection();
bool RequestCloseInspection();
bool RequestContinue();
bool IsInspectionOpenForTest() const;
```

- [ ] **Step 5: Import, validate, run tests, and commit**

Import through UE MCP, require no unrelated dirty packages, cold-build, run `GameXXK.Prologue.Aftermath.MapWidget`, and rerun the policy suite. Commit source image, importer, texture, widget, and test as:

```text
feat: add inspectable Xu Xiake route map
```

---

### Task 4: Add the unique persistent tutorial map item and v31 migration

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorialMapItemTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKNarrativeGuideSaveMigrationTest.cpp`

- [ ] **Step 1: Write red item, uniqueness, delivery, and migration tests**

Create `GameXXK.Prologue.Aftermath.TutorialMapItem` and require:

```cpp
bool bFound = false;
const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(
    UGameXXKMVPRules::ItemTutorialRiverMap(), bFound);
TestTrue(TEXT("map item is catalogued"), bFound);
TestEqual(TEXT("approved inventory name"), Def.DisplayName,
    FText::FromString(TEXT("徐霞客游历路线")));
TestEqual(TEXT("map is a task item"), Def.Kind, EGameXXKItemKind::Task);
State.Inventory.Add(UGameXXKMVPRules::ItemTutorialRiverMap(), 1);
TestFalse(TEXT("map cannot be sold"), UGameXXKMVPRules::CanSellItem(
    State, UGameXXKMVPRules::ItemTutorialRiverMap()));
```

Exercise `GrantTutorialRiverMap` twice and require combined backpack/warehouse/pending ownership exactly one. Fill backpack only and require warehouse placement; fill both and require `PendingTaskItemIds` contains the map. Free one slot, normalize, and require pending delivery occurs exactly once.

Add a v30→v31 migration test requiring an empty pending set and unchanged unrelated runtime state.

- [ ] **Step 2: Prove red**

Cold-build. Expected failure: missing item accessor, pending field, v31 constant, and grant API.

- [ ] **Step 3: Add the v31 state boundary and item definition**

Add to `FGameXXKDesktopInventoryState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TSet<FName> PendingTaskItemIds;
```

Set:

```cpp
static constexpr int32 TutorialMapItemIntroducedSaveVersion = 31;
static constexpr int32 CurrentSaveVersion = 31;
```

Add `Item.Tutorial.RiverMap` to known items with display name `徐霞客游历路线`, kind `Task`, buy/sell zero, and a public `ItemTutorialRiverMap()` accessor. `CanSellItem`, `UseItem`, enhance, decompose, and equip remain false through existing kind/equipment guards.

- [ ] **Step 4: Implement atomic unique grant and pending delivery**

Add:

```cpp
bool UGameXXKMVPSubsystem::GrantTutorialRiverMap(FString* OutError);
bool UGameXXKMVPSubsystem::OwnsTutorialRiverMap() const;
```

Ownership checks `State.Inventory`, `DesktopInventory.WarehouseItems`, and `PendingTaskItemIds`. Grant on an already-owned map returns true without mutation. First grant tries backpack normalization, then warehouse partition, then pending set. Commit and save atomically; save failure restores the pre-grant state.

At the beginning of `FGameXXKDesktopInventoryRules::Normalize`, attempt to deliver pending task items into a newly available backpack slot first, warehouse slot second, and retain the ID if both are full.

- [ ] **Step 5: Run save/inventory gates and commit**

Run cold UBT and exact Automation:

```text
GameXXK.Prologue.Aftermath.TutorialMapItem
GameXXK.MVP.SaveGame
GameXXK.DesktopInventory
GameXXK.Narrative.SaveMigration
```

Commit only task hunks as:

```text
feat: persist the Xu Xiake tutorial map item
```

---

### Task 5: Route inventory icon, drag, and right-click inspection safely

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKInventoryItemPresentation.h`
- Create: `Source/GameXXK/Private/UI/GameXXKInventoryItemPresentation.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorialMapInventoryUiTest.cpp`

- [ ] **Step 1: Add red presentation and routing tests**

Require:

```cpp
TestEqual(TEXT("map icon path"),
    FGameXXKInventoryItemPresentation::ResolveIconPath(
        UGameXXKMVPRules::ItemTutorialRiverMap()),
    FString(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap.T_Relic_OldMap")));
TestTrue(TEXT("map is inspectable"),
    FGameXXKInventoryItemPresentation::IsInspectable(
        UGameXXKMVPRules::ItemTutorialRiverMap()));
```

Populate the map in backpack and warehouse slots. Right-click each and require exactly one inspect delegate call, no container move, no tool-slot entry, no equip, and unchanged item count. Left-drag backpack↔warehouse must still use the existing atomic move transaction. Dropping the map onto tools/equipment must reject and leave its origin intact.

- [ ] **Step 2: Prove red and centralize item icon resolution**

Cold-build for missing presentation helper. Implement `ResolveIconPath` by moving existing duplicated item-path switches into the new helper and adding only the tutorial map case. Existing item paths must remain byte-for-byte equivalent in tests.

```cpp
class FGameXXKInventoryItemPresentation final
{
public:
    static FString ResolveIconPath(FName ItemId);
    static bool IsInspectable(FName ItemId)
    {
        return ItemId == UGameXXKMVPRules::ItemTutorialRiverMap();
    }
    static FString InspectTexturePath(FName ItemId);
};
```

`InspectTexturePath` returns the full route-map texture only for the tutorial map and an empty string for every other item.

- [ ] **Step 3: Add inspection request seams**

Add a return-valued delegate to Workbench and InventoryWindow test seams. In production, route to:

```cpp
bool AGameXXKMVPPlayerController::OpenTutorialMapInspection();
void AGameXXKMVPPlayerController::CloseTutorialMapInspection();
```

The controller creates `UGameXXKPrologueMapWidget` in `InspectOnly` mode at a UI Z-order above inventory and below system pause. Closing returns focus to the existing inventory/workbench without changing its open/drag/session state.

- [ ] **Step 4: Intercept right-click before generic actions**

In `RouteBackpackRightClick`, `HandleActionRightClicked` for warehouse slots, and standalone inventory right-click, inspect the map before warehouse transfer, tools, quick equip, or legacy equipment logic. Preserve existing behavior for every other item.

- [ ] **Step 5: Run focused UI tests and commit**

Run:

```text
GameXXK.Prologue.Aftermath.TutorialMapInventoryUi
GameXXK.DesktopTraining.Workbench.ItemCarry
GameXXK.DesktopTraining.Workbench
GameXXK.MVP.Inventory
```

Commit only focused hunks/new files as:

```text
feat: inspect the tutorial map from inventory
```

---

### Task 6: Add YueBai intro playback and 220–300 UU narrative following

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKPrologueYueBaiWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKPrologueYueBaiWidget.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueYueBaiPresentationTest.cpp`
- Modify: `scripts/test_prologue_aftercare_policy.py`

- [ ] **Step 1: Write red atlas and follower tests**

Require an 8×8/60-frame atlas widget and the approved source paths:

```text
/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_2k_atlas
/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_1k_atlas
```

Require 56.074766 FPS, 1.07 seconds, bottom-center pivot, no input hit testing, and frame 0/59 UVs.

For `AGameXXKTownNpcCharacter`, require:

```cpp
Npc->ActivateNarrativeFollower(Hero, 220.0f, 260.0f, 300.0f);
TestTrue(TEXT("narrative follower active"), Npc->IsNarrativeFollowerActive());
TestEqual(TEXT("minimum band"), Npc->GetNarrativeFollowMinimumForTest(), 220.0f);
TestEqual(TEXT("target band"), Npc->GetFollowDistance(), 260.0f);
TestEqual(TEXT("maximum band"), Npc->GetNarrativeFollowMaximumForTest(), 300.0f);
```

Advance at distances 210, 260, 290, and 310 UU. Require retreat only below 220, no movement inside 220–300, and pursuit above 300. Require no quest-location write and collision disabled only for narrative following, then restored on dismiss.

- [ ] **Step 2: Prove red with cold UBT**

Expected missing widget and follower APIs.

- [ ] **Step 3: Implement YueBai atlas playback**

The widget mirrors the proven 8×8 UV crop pattern without modifying `UGameXXKPrologueCarriageWidget`. The owning controller advances frames for exactly 1.07 seconds, then hides the atlas widget and reveals the existing NPC. Missing 2K falls back to 1K; missing both invokes fail-open cleanup.

```cpp
static constexpr int32 YueBaiIntroFrameCount = 60;
static constexpr float YueBaiIntroFramesPerSecond = 56.074766f;
static constexpr float YueBaiIntroDurationSeconds = 1.07f;

bool UGameXXKPrologueYueBaiWidget::SetAtlasFrame(
    UTexture2D* Texture,
    int32 FrameIndex);
```

- [ ] **Step 4: Implement isolated narrative follow mode**

Add separate narrative-follow fields and methods; do not change `ActivateFollower` legacy semantics. Narrative Tick keeps the idle/hover visual, moves with hysteresis toward 260 UU, does not call `RecordQuestNpcMovedLocation`, and uses a bounded offscreen catch-up threshold. `DismissNarrativeFollower` restores collision and prior interaction state.

```cpp
void ActivateNarrativeFollower(
    AActor* Target,
    float MinimumDistance = 220.0f,
    float TargetDistance = 260.0f,
    float MaximumDistance = 300.0f);
void DismissNarrativeFollower();
bool IsNarrativeFollowerActive() const;
```

- [ ] **Step 5: Run tests and commit**

Run `GameXXK.Prologue.Aftermath.YueBaiPresentation`, existing Town NPC/follower tests, and policy. Commit as:

```text
feat: present and follow with YueBai
```

---

### Task 7: Implement the independent aftermath controller and passive statue bubble

**Files:**
- Create: `Source/GameXXK/Public/Town/GameXXKPrologueAftermathController.h`
- Create: `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `scripts/test_prologue_aftercare_policy.py`

- [ ] **Step 1: Write red ownership, input, and passive-bubble tests**

Require the CDO to expose:

```cpp
TestEqual(TEXT("approved YueBai offset from the carriage root"),
    Defaults->GetYueBaiRevealOffsetForTest(),
    FVector(250.623f, 666.139f, 0.0f));
TestEqual(TEXT("follow bubble text"),
    Defaults->GetStatuePromptTextForTest(),
    FText::FromString(TEXT("前往巨大雕像旁按F交互")));
```

Bind a fake carriage Rig and require one finished broadcast starts exactly one Aftermath session. Verify Space transitions, map inspection loop, intro wait, both dialogues, `AdjustBackpack` request on `food.hero.offer`, follower activation, and statue prompt.

Create the passive bubble directly through `UGameXXKSpeechBubbleWidget::PresentBubble`, without a DialogueCoordinator. Require:

```cpp
TestTrue(TEXT("passive bubble visible"), Bubble->IsBubbleVisibleForTest());
TestFalse(TEXT("passive bubble never owns aftermath input token"),
    Controller->IsPrologueAftermathInputLockedForTest());
TestFalse(TEXT("movement remains enabled"), Controller->IsMoveInputIgnored());
TestFalse(TEXT("look remains enabled"), Controller->IsLookInputIgnored());
```

- [ ] **Step 2: Prove red**

Cold-build for the missing controller and player-controller token APIs.

- [ ] **Step 3: Build the dormant map actor**

The actor owns root, `YueBaiReveal`, `StatueInteractionArea`, `StatuePromptAnchor`, world-space YueBai intro widget component, transient DialogueCoordinator/session, map widget, dialogue panel, and a separate passive `UGameXXKSpeechBubbleWidget` instance. Without the carriage URL option or tutorial-return option it stays dormant and tickless.

In `BeginPlay`, find exactly one managed carriage Rig and bind `OnFinished`. Resolve the existing NPC by `GetNpcId() == Npc.YueBai`; reject zero or duplicates. Do not spawn an NPC.

- [ ] **Step 4: Route controlled input before ordinary town shortcuts**

Add one active-controller weak pointer to PlayerController. While blocking aftermath phases are active, Space/Enter/left-click advance the controller, Esc shows the existing `UGameXXKProloguePauseWidget` with Continue/Return Desktop, and all town shortcuts are consumed. Continue calls `FGameXXKPrologueAftermathRules::SetPaused(State, false)` without resetting the current phase; Return Desktop runs idempotent cleanup before normal desktop travel. In following/statue phases, release the token and let normal movement/UI inputs pass.

The controller creates its own transient Dialogue session, so no aftermath node is written into the save-authoritative `State.DialogueSession`.

- [ ] **Step 5: Implement food gesture, following, and passive prompt**

When the current meeting node becomes `food.hero.offer`, request `Hero->PlayTownAction(EGameXXKHeroTownAction::AdjustBackpack)`; missing optional action does not block dialogue. On dialogue end, activate narrative follow, present the passive bubble at YueBai's prompt anchor, and keep updating only its projected position.

- [ ] **Step 6: Implement idempotent fail-open cleanup**

Cancel/map exit/resource failure removes all widgets, cancels dialogue, hides intro atlas, restores hero input/view target, restores YueBai snapshot unless successful following has begun, releases the token, and unbinds the carriage delegate. The already committed map task item is never removed.

- [ ] **Step 7: Run focused tests and commit**

Run:

```text
GameXXK.Prologue.Carriage
GameXXK.Prologue.Aftermath
GameXXK.Dialogue.Coordinator
GameXXK.MVP.Town
```

Commit as:

```text
feat: continue the prologue after the carriage
```

---

### Task 7A: Correct accepted YueBai follow grounding, idle playback, and bubble anchoring

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKSpeechBubbleWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKSpeechBubbleWidget.cpp`
- Modify: `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueYueBaiPresentationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp`

- [ ] **Step 1: Write the red follower and bubble tests**

Extend `GameXXK.Prologue.Aftermath.YueBaiPresentation` to require that a ground hit at impact Z `300` resolves to root Z `372` for a `72` UU capsule half-height, while a missing hit preserves the previous root Z. Stop the NPC visual before narrative activation, then require the first follow tick to restore `IsLooping()` and `IsPlaying()` while `IsTownMoving()` stays false.

Extend `GameXXK.Dialogue.Presenter.SpeechBubbleAnchorAndClamp` to require a visual-bounds-top presentation mode and exact bounds-top resolution:

```cpp
TestEqual(TEXT("visual top adds only Z extent"),
    UGameXXKSpeechBubbleWidget::VisualBoundsTopForTest(
        FVector(10.0f, 20.0f, 30.0f),
        FVector(40.0f, 50.0f, 60.0f)),
    FVector(10.0f, 20.0f, 90.0f));
```

- [ ] **Step 2: Prove red with cold UBT/Automation**

Run cold UBT, then `GameXXK.Prologue.Aftermath.YueBaiPresentation` and `GameXXK.Dialogue.Presenter.SpeechBubbleAnchorAndClamp`. Expected: compile-red on the missing grounding and visual-top APIs.

- [ ] **Step 3: Ground the non-blocking follower without changing collision policy**

Keep narrative capsule/interaction collision disabled. On every narrative-follow tick, trace down at the chosen horizontal destination, ignore YueBai and the followed hero, and resolve root Z as `Hit.ImpactPoint.Z + CapsuleHalfHeight`. Smooth the full 3D destination with the existing follow speed; a missing hit retains current root Z. Do not alter legacy `ActivateFollower`, formation, or NPC save state.

- [ ] **Step 4: Guarantee hover-idle playback**

Add a narrative-only helper that selects the authored South idle flipbook, sets looping true, starts it when changed, and resumes it when stopped. Invoke it on activation and narrative follow ticks. Movement remains visually Idle even while YueBai glides.

- [ ] **Step 5: Anchor the passive prompt to rendered visual bounds**

Add `PresentBubbleAtVisualTop` as a separate speech-bubble path so ordinary Dialogue bubbles keep their existing anchor semantics. It projects the PaperFlipbook bounds top through `UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition`, which yields viewport-local DPI-correct UMG coordinates. The aftermath controller passes `GetTownVisualComponent()`; do not add screenshot-specific pixel offsets.

- [ ] **Step 6: Run green gates and player checkpoint**

Run cold UBT, both focused tests, `GameXXK.Prologue.Aftermath`, and the static policy suite. Start floating PIE without synthetic input and stop at the follower checkpoint for the player to verify stair ascent, looping idle, prompt placement, and unrestricted hero movement.

Commit with Task 7 as:

```text
feat: continue the prologue after the carriage
```

---

### Task 7B: Retire the disconnected legacy tutorial task and town-NPC options

**Files:**
- Modify: `SourceAssets/Narrative/characters.json`
- Modify: `SourceAssets/Narrative/runtime-catalog.json`
- Delete: `SourceAssets/Narrative/Dialogues/Dialogue.Npc.TusiChief.Default.dialogue.json`
- Delete: `SourceAssets/Narrative/Dialogues/Dialogue.Npc.SongJinBao.Default.dialogue.json`
- Delete: `SourceAssets/Narrative/Sequences/Sequence.Npc.TusiChief.Default.sequence.json`
- Delete: `SourceAssets/Narrative/Sequences/Sequence.Npc.SongJinBao.Default.sequence.json`
- Delete: matching generated Dialogue/Sequence DataAssets
- Modify: `Source/GameXXK/Private/Narrative/GameXXKStoryCatalog.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify/Delete: old task/NPC interaction tests

- [ ] **Step 1: Write red retirement contracts**

Require the old story/task catalog lookups to return null, Tusi Chief and Song Jinbao to have no default interaction sequence, the runtime catalog/source tree to contain neither NPC option nor `openTaskOffer`/`openShop`, and save v32 migration to remove only the retired story/task/session IDs while preserving unrelated runtime state. Require YueBai narrative follow to select `/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_character_09_yue_bai_2k_idle` with more than one frame.

- [ ] **Step 2: Prove red**

Cold UBT and focused Python/C++ tests must fail against the existing legacy catalog, JSON sources, v31 save boundary, and single-frame follower selection.

- [ ] **Step 3: Remove player-facing legacy task/shop data**

Remove the two default interaction sequence IDs from `characters.json`, delete their Dialogue/Sequence source and generated assets, and remove now-unused catalog outcomes/command types. Remove PlayerController registration of `openTaskOffer` and `openShop` and the NPC-specific task-offer bridge. Keep route merchant/MetaShop implementations and internal `AcceptTownQuest` compatibility intact.

- [ ] **Step 4: Retire old tutorial StoryTask persistence**

Remove `BeginTutorialQuest` and the old StoryCatalog definitions. Bump to save v32 and migrate old story/task/session identities out of `NarrativeProgress` instead of recreating them. Do not reset unrelated guide, inventory, formation, route, reward, or companion state.

- [ ] **Step 5: Use the animated YueBai production idle**

Give narrative follow a YueBai-only soft path to the production 2K multi-frame idle, with no 1K fallback. Activation/tick selects it, loops it, resumes it, and applies a narrative-only `2.5×` visual scale; dismiss restores the exact previous relative scale. Ordinary static NPC and PartyDeck consumers retain their existing assets.

- [ ] **Step 6: Import, verify, and stop for acceptance**

Reimport the character catalog, delete only the four retired generated assets through a guarded UE script, run cold UBT plus narrative/source/migration/NPC/Aftermath tests, then leave PIE for manual confirmation that Tusi/Song have no stale choices and YueBai visibly animates.

---

### Task 8: Add statue F travel and the transient tutorial return context

**Files:**
- Create: `Source/GameXXK/Public/MVP/GameXXKTutorial01SessionSubsystem.h`
- Create: `Source/GameXXK/Private/MVP/GameXXKTutorial01SessionSubsystem.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorial01SessionTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKPrologueAftermathController.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPrologueAftermathController.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Write red LevelFlow and session tests**

Require:

```cpp
TestEqual(TEXT("tutorial map"), GameXXKLevelFlow::Tutorial01Map(),
    FName(TEXT("/Game/GameXXK/Maps/Tutorial/L_Tutorial_0_1")));
TestEqual(TEXT("entry option"), GameXXKLevelFlow::Tutorial01TravelOptions(),
    FString(TEXT("GameXXKTutorial=0-1")));
TestTrue(TEXT("entry option parses"),
    GameXXKLevelFlow::HasTutorial01TravelOption(TEXT("?GameXXKTutorial=0-1")));
TestFalse(TEXT("ordinary battle is not tutorial 0-1"),
    GameXXKLevelFlow::HasTutorial01TravelOption(TEXT("")));
TestEqual(TEXT("victory return options"),
    GameXXKLevelFlow::Tutorial01ReturnTravelOptions(true),
    FString(TEXT("GameXXKIntro=Tutorial01Return?GameXXKTutorialResult=Victory")));
TestEqual(TEXT("defeat return options"),
    GameXXKLevelFlow::Tutorial01ReturnTravelOptions(false),
    FString(TEXT("GameXXKIntro=Tutorial01Return?GameXXKTutorialResult=Defeat")));
TestTrue(TEXT("return option parses"),
    GameXXKLevelFlow::HasTutorial01ReturnOption(
        TEXT("?GameXXKIntro=Tutorial01Return?GameXXKTutorialResult=Victory")));
```

Create a runtime snapshot with distinctive Training, route, formation, inventory, CardRun, and reward values. Begin a tutorial session, mutate battle fields, resolve victory/defeat, and require restore equality for every non-tutorial field plus preservation of `Item.Tutorial.RiverMap`.

- [ ] **Step 2: Prove red**

Cold-build for missing LevelFlow and session subsystem APIs.

- [ ] **Step 3: Implement the GameInstance tutorial session**

Define:

```cpp
UENUM()
enum class EGameXXKTutorial01ReturnReason : uint8 { None, Victory, Defeat };

USTRUCT()
struct FGameXXKTutorial01ReturnContext
{
    GENERATED_BODY()
    FGameXXKRuntimeState RuntimeBeforeTutorial;
    FTransform StatueReturnTransform;
    EGameXXKTutorial01ReturnReason ReturnReason = EGameXXKTutorial01ReturnReason::None;
    bool bActive = false;
};
```

The subsystem supports `BeginFromTown`, `PrepareRetry`, `RestoreForTownReturn`, and one-shot `ConsumeTownReturn`. It is transient and never saved.

LevelFlow adds these exact declarations:

```cpp
GAMEXXK_API FName Tutorial01Map();
GAMEXXK_API FString Tutorial01TravelOptions();
GAMEXXK_API bool HasTutorial01TravelOption(const FString& Options);
GAMEXXK_API FString Tutorial01ReturnTravelOptions(bool bVictory);
GAMEXXK_API bool HasTutorial01ReturnOption(const FString& Options);
GAMEXXK_API bool IsTutorial01MapPackage(const FString& PackageName);
```

- [ ] **Step 4: Make the aftermath actor an F interactable only in StatuePrompt**

The actor implements `IGameXXKInteractable`, uses its `StatueInteractionArea` overlap to register with the possessed hero's existing InteractionComponent, returns prompt `F`, and accepts interaction only in `StatuePrompt`. F first opens an explicitly requested `UGameXXKGuidePreferenceWidget`; this widget never auto-opens from Workbench or unset guide preference and uses `T_MasterV2_PanelLarge` as its paper background. After the player selects skip/continue, the controller snapshots runtime/return transform, sets a travel-pending guard, clears the passive bubble, and calls absolute OpenLevel to `Tutorial01Map()` with the exact option and selected tutorial-guide preference.

Out-of-range F never focuses the actor; repeated F after pending returns false.

- [ ] **Step 5: Run tests and commit**

Run `GameXXK.MVP.LevelFlow`, `GameXXK.Prologue.Aftermath.StatueInteraction`, and `GameXXK.Tutorial01.Session`. Commit as:

```text
feat: route the statue into tutorial 0-1
```

---

### Task 9: Build the tutorial-only BattleBoard host, Guide, and result flow

**Files:**
- Create: `Source/GameXXK/Public/MVP/GameXXKTutorial01GameMode.h`
- Create: `Source/GameXXK/Private/MVP/GameXXKTutorial01GameMode.cpp`
- Create: `Source/GameXXK/Public/Guide/GameXXKTutorial01GuideHost.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKTutorial01GuideHost.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKTutorial01ResultWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTutorial01ResultWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorial01PlayerFlowTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

- [ ] **Step 1: Write red tutorial boot and isolation tests**

Add `TutorialBattleOnly` to the player-flow profile and require the tutorial map resolves it while desktop/town maps retain their old values. In the tutorial profile require no Workbench, TownOverlay, route map, or Pawn; require exactly one full-screen BattleBoard.

Start `Encounter.Main.XuXiake.0-1` and require one `Enemy.Ch1.Rooster`, `BattleProfile.Tutorial.0-1`, and no generated route/challenge/travel mutation.

Force victory and require no pending reward, then town-return request. Force defeat and require a result widget with exactly `重新挑战` and `返回城镇`.

- [ ] **Step 2: Prove red**

Cold-build for missing boot profile, game mode, guide host, result widget, and tutorial terminal APIs.

- [ ] **Step 3: Implement a Pawn-less tutorial GameMode and boot profile**

`AGameXXKTutorial01GameMode` uses `AGameXXKMVPPlayerController`, no default Pawn, and existing HUD/GameState defaults. `ResolvePlayerFlowBootProfile` returns `TutorialBattleOnly` only for the exact tutorial map.

On tutorial BeginPlay, PlayerController validates the URL option and active session context, calls `StartNarrativeEncounter(Encounter.Main.XuXiake.0-1)`, creates the BattleBoard, enters full-screen overlay, and initializes the local Guide host. Missing option/session fails open to the default desktop map.

The dedicated GameMode constructor must set:

```cpp
PlayerControllerClass = AGameXXKMVPPlayerController::StaticClass();
DefaultPawnClass = nullptr;
HUDClass = AHUD::StaticClass();
```

- [ ] **Step 4: Implement the tutorial-local Guide host**

The host owns a transient `FGameXXKGuideProgress` with `Preference=NewPlayer`, one GuideCoordinator, and one GuideOverlay at BattleBoard Z-order. It subscribes to `Event.Battle.Opened`, loads `DA_Guide_Battle_Basic`, and installs an Action Gate owned by itself. Shutdown always calls `ClearActionGate(this)` and removes the event handle.

It never binds to or persists `RuntimeState.GuideProgress`, so ordinary battles are unchanged.

- [ ] **Step 5: Intercept tutorial terminal state before ordinary rewards**

At the top of `UGameXXKBattleBoardWidget::ResolveCardBattleTerminalState`, check `Subsystem->IsTutorial01BattleActive()`. On victory, clear the active card battle without `ResolveBattleVictory`, restore the pre-tutorial snapshot, mark one-shot victory return, and travel to Qingshan with the tutorial-return option. On defeat, do not call `FailDungeonToTown`; show the result widget.

Retry restores the pre-tutorial snapshot, starts a fresh 0-1 encounter, resets the local Guide session, and refreshes the same BattleBoard. Return restores the snapshot and travels to Qingshan with defeat reason.

The result widget API is fixed to:

```cpp
DECLARE_DELEGATE(FGameXXKTutorial01RetryRequested);
DECLARE_DELEGATE(FGameXXKTutorial01ReturnTownRequested);

void PresentDefeat();
void Dismiss();
bool RequestRetryForTest();
bool RequestReturnTownForTest();
```

- [ ] **Step 6: Resume YueBai after town return**

When Qingshan loads with the return option, the aftermath controller consumes the session context, places the hero at the statue return transform, reuses/moves the unique YueBai NPC into 220–300 UU following, and restores normal input. Victory hides the statue bubble; defeat re-presents it and re-enables F.

- [ ] **Step 7: Run focused tests and commit**

Run:

```text
GameXXK.Tutorial01
GameXXK.Narrative.Encounter
GameXXK.Guide
GameXXK.MVP.PlayableShell.GameModeDefaults
GameXXK.UI.Battle
```

Commit as:

```text
feat: add the isolated tutorial 0-1 battle flow
```

---

### Task 10: Create and validate the two map surfaces without moving protected actors

**Files:**
- Create: `Content/Python/gamexxk_probe_tutorial_statue_anchor.py`
- Create: `Content/Python/gamexxk_place_prologue_aftermath.py`
- Create: `Content/Python/gamexxk_create_tutorial01_map.py`
- Create: `Content/Python/gamexxk_validate_prologue_tutorial01.py`
- Modify: `Content/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo.umap`
- Create: `Content/GameXXK/Maps/Tutorial/L_Tutorial_0_1.umap`
- Modify: `scripts/test_prologue_aftercare_policy.py`

This task is map/art assembly: use guarded pre/post validation and visual evidence rather than TDD for binary map edits.

- [ ] **Step 1: Write a read-only statue candidate probe**

The probe requires the exact Qingshan map, enumerates static-mesh/instanced-mesh bounds in front of the approved prologue anchor, and emits actor/component/mesh paths, bounds, and walkable base points. It contains no save, spawn, transform, input, or map-load method. Policy rejects all mutation tokens.

Run it in the already-open Qingshan editor map through UE MCP, capture the viewport with actor annotations, and use a suitable review method to identify the visible giant center statue. Record the selected immutable actor/component reference and computed base interaction point in the placement report; do not modify the statue.

- [ ] **Step 2: Write the guarded Qingshan placement script**

The script must:

1. refuse dirty packages before loading;
2. require the exact Qingshan map and exactly one managed carriage Rig;
3. find zero or one actor labeled `GameXXK_PrologueAftermath` with tag `GameXXKManaged.PrologueAftermath`;
4. refuse unowned same-label actors;
5. spawn/update only the native aftermath controller;
6. anchor its root to the carriage Rig transform;
7. set `YueBaiReveal` so its world location is exactly `(16929.215, 5936.139, 1075.711)`;
8. set the statue interaction component to the computed walkable base point with a 300 UU overlap radius;
9. save only the Qingshan map and emit before/after transforms plus dirty package lists.

- [ ] **Step 3: Create the pure-2D tutorial map**

`gamexxk_create_tutorial01_map.py` creates/updates only `/Game/GameXXK/Maps/Tutorial/L_Tutorial_0_1`, sets `AGameXXKTutorial01GameMode`, and keeps no world actors beyond WorldSettings/default infrastructure required to host PlayerController/UI. It must not copy `L_DesktopTrainingHUD` actors or open Workbench content.

- [ ] **Step 4: Validate both maps and assets**

`gamexxk_validate_prologue_tutorial01.py` requires:

- one carriage Rig and one managed aftermath controller in Qingshan;
- exact YueBai reveal anchor;
- one statue interaction sphere, 300 UU radius, whose center is adjacent to the selected statue base;
- unchanged PlayerStart, carriage Rig, six fixed NPC transforms, statue transform, and map actor count outside the managed addition;
- tutorial map GameMode exact and no 3D/town/route/workbench actors;
- two Dialogue assets with exact IDs;
- map icon 512×512 alpha and inspect image 2388×1668 with source hash;
- no dirty package after validation.

- [ ] **Step 5: Save, visually inspect, and commit**

Save through MCP, run placement/map creation/validator, inspect `git status` for only the two maps/scripts, and capture Qingshan statue/aftermath plus an empty tutorial-map editor viewport. Commit as:

```text
feat: place the prologue aftermath and tutorial map
```

---

### Task 11: End-to-end recovery gates, visual acceptance, docs, and remote push

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPrologueAftermathControllerTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTutorial01SessionTest.cpp`
- Modify: `scripts/test_prologue_aftercare_policy.py`
- Create: `Content/Python/gamexxk_probe_prologue_aftercare.py`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Add failure-path and invariant tests**

Snapshot all non-tutorial runtime fields before story/tutor entry. Cover missing inspect texture, missing dialogue, missing/duplicate YueBai, missing intro atlas, missing statue marker, missing tutorial map/session, retry, defeat return, victory return, pause/continue, return desktop, map exit, and repeated cleanup.

Every path must release move/look input, remove overlays/bubbles, clear action gates/travel pending, preserve ordinary state, and retain at most one tutorial map item.

- [ ] **Step 2: Add one read-only runtime probe**

The probe reports only current map/options, aftermath phase, map-card/inspection visibility, map-item ownership/container/slot, YueBai count/location/follow distance, passive-bubble visibility, input locks, statue focus/prompt, tutorial session status, battle phase/enemy count, guide step, and result overlay. Policy rejects input, map load, save, click, key, and window methods.

- [ ] **Step 3: Run the fresh full focused gate**

Run cold UBT, then exact Automation groups:

```text
GameXXK.Prologue.Carriage
GameXXK.Prologue.Aftermath
GameXXK.Tutorial01
GameXXK.Dialogue
GameXXK.Guide
GameXXK.Narrative.Encounter
GameXXK.MVP.LevelFlow
GameXXK.DesktopInventory
GameXXK.DesktopTraining.Workbench
GameXXK.MVP.PlayableShell.GameModeDefaults
```

Also run:

```powershell
python -m unittest scripts.test_dialogue_json_validation scripts.test_prologue_carriage_policy scripts.test_prologue_aftercare_policy -v
python -m py_compile Content/Python/gamexxk_import_tutorial_route_map.py Content/Python/gamexxk_probe_tutorial_statue_anchor.py Content/Python/gamexxk_place_prologue_aftermath.py Content/Python/gamexxk_create_tutorial01_map.py Content/Python/gamexxk_validate_prologue_tutorial01.py Content/Python/gamexxk_probe_prologue_aftercare.py
python scripts/harness_state_validator.py
git diff --check
```

Record unrelated baseline failures separately; do not weaken them or claim a full project suite if it was not run.

- [ ] **Step 4: Capture and review visual states**

Without synthetic input, capture:

1. hero notice paper panel;
2. unnamed map thumbnail card;
3. full map inspection and top-right close;
4. YueBai intro at the approved anchor with unchanged camera;
5. food/guide dialogue;
6. YueBai at 220–300 UU following;
7. passive statue bubble while the hero is moving;
8. tutorial-only BattleBoard and forced guide target;
9. defeat retry/return overlay;
10. victory return beside the statue.

The selected review method must check crop, readability, anchor positions, overlap, follow distance, bubble visibility, unchanged camera, absence of Workbench/town UI on the tutorial map, and no input-block visual symptom.

- [ ] **Step 5: Ask the player to perform the real chain**

Prepare PIE on the default 2D map with no input driver. The player manually verifies the full chain, optional inspect, inventory drag/right-click inspect, pause/continue/return, statue F, tutorial forced steps, defeat retry/return, victory return, replay uniqueness, and restart default. Record unperformed chains as unverified.

- [ ] **Step 6: Update rolling acceptance and push**

Record commits, cold UBT, exact Automation counts, JSON/policy/map validators, probe observations, visual-review report, manual steps actually performed, and explicit non-goals in `docs/production/current-goal-acceptance.md`.

Commit docs only, restore the three protected staged deletions, push `main`, and verify:

```powershell
git ls-remote origin refs/heads/main
git rev-parse HEAD
git diff --cached --name-status
```

Remote and local hashes must match; cached paths must be exactly the three protected deletions.

---

## Completion checklist

- [ ] Carriage preview behavior and normal town entry remain unchanged.
- [ ] Hero notice opens immediately after carriage completion; no world scroll exists.
- [ ] Map thumbnail is unnamed, inspect is optional, full image is uncropped, and inspection Space is inert.
- [ ] `Item.Tutorial.RiverMap` is unique, persistent, draggable, right-click inspectable, and never usable/sellable/equippable.
- [ ] Existing YueBai is reused at `(16929.215, 5936.139, 1075.711)` with unchanged camera and approved intro atlas.
- [ ] Food and guide dialogue text exactly matches the approved JSON.
- [ ] YueBai follows with hover idle and 220/260/300 hysteresis without changing formation or NPC save authority.
- [ ] Statue guidance bubble is passive and never blocks controller movement/look.
- [ ] Statue F is range-gated, one-shot per pending travel, and the only entry into `L_Tutorial_0_1`.
- [ ] Tutorial map is pure 2D, Pawn-less, and never reachable from ordinary flows.
- [ ] Tutorial uses one rooster and local `Guide.Battle.Basic`; ordinary battles are unchanged.
- [ ] Victory returns without rewards; defeat offers retry/return and restores the statue prompt on return.
- [ ] Ordinary challenge, route, travel, rewards, formation, inventory beyond the map item, window, and idle states are invariant.
- [ ] Pause, cancel, failure, travel, shutdown, and late callbacks cannot leave input, UI, action gates, NPCs, or map pending locked.
- [ ] Cold UBT, focused Automation, source/policy/map checks, read-only probes, visual review, and player-performed acceptance are recorded honestly.
- [ ] Existing dirty work and the three protected staged deletions remain intact.
