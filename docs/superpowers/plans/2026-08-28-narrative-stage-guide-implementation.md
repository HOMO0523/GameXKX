# Narrative Stage and Tutorial Guide Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add multi-story/multi-task orchestration, scene-replaceable stage contracts/profiles, scene-independent encounter presentation, and a resumable semantic-target guide system with the fixed tutorial route.

**Architecture:** Story/Task progress owns why and when content runs; Dialogue and NarrativeSequence are separate reusable state machines. Sequence invokes Dialogue and branches on returned OutcomeIds, while namespaced Sequence commands own all world/gameplay side effects. StageContract lists semantic slots, SceneRegistry selects a map-specific SceneProfile that provides them, BattleProfile owns full-screen battle positions, and GuideAsset targets semantic UI IDs without storing coordinates.

**Tech Stack:** UE 5.8 C++, UPrimaryDataAsset, existing GameXXK RuntimeState/routes/BattleBoard/settings, save migration v29, Automation Tests, UE MCP validation.

---

## File map

- Create `Source/GameXXK/Public/Narrative/GameXXKCharacterCatalog.h` and private `.cpp` — character identity/assets/actions without placement.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceTypes.h` — compiled steps, requests, results and saved session.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceAsset.h` and private `.cpp` — compiled Sequence asset.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceRules.h` and private `.cpp` — pure step/outcome/idempotency state machine.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h` and private `.cpp` — one active Sequence, request dispatch and input-token ownership.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeCommandExecutor.h` — typed executor contract.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeTypes.h` — Story/Task/Stage/Guide runtime and definition types.
- Create `Source/GameXXK/Public/Narrative/GameXXKStoryCatalog.h` and private `.cpp` — story/task definitions and lookups.
- Create `Source/GameXXK/Public/Narrative/GameXXKStoryRules.h` and private `.cpp` — pure multi-story/task progress rules.
- Create `Source/GameXXK/Public/Narrative/GameXXKStageContract.h` and private `.cpp` — required semantic slots.
- Create `Source/GameXXK/Public/Narrative/GameXXKSceneProfile.h` and private `.cpp` — map-specific slot/NPC/trigger bindings.
- Create `Source/GameXXK/Public/Narrative/GameXXKSceneRegistry.h` and private `.cpp` — StageContract→active profile mapping.
- Create `Source/GameXXK/Public/Narrative/GameXXKBattleProfile.h` and private `.cpp` — full-screen battle semantic positions.
- Create `Source/GameXXK/Public/Narrative/GameXXKNarrativeEncounterCatalog.h` and private `.cpp` — encounter enemy/rule/reward/profile lookup.
- Create `Source/GameXXK/Public/Guide/GameXXKGuideAsset.h` and private `.cpp` — compiled guide definitions.
- Create `Source/GameXXK/Public/Guide/GameXXKGuideRules.h` and private `.cpp` — pure guide state machine.
- Create `Source/GameXXK/Public/Guide/GameXXKGuideTargetRegistry.h` and private `.cpp` — semantic UI target registration.
- Create `Source/GameXXK/Public/Guide/GameXXKGuideCoordinator.h` and private `.cpp` — overlay/input ownership.
- Create `Source/GameXXK/Public/UI/GameXXKGuideOverlayWidget.h` and private `.cpp` — arrow/mask/text.
- Create `Source/GameXXK/Public/UI/GameXXKGuidePreferenceWidget.h` and private `.cpp` — first-entry old/new player prompt.
- Create `Source/GameXXK/Public/GameXXKTutorialRouteRules.h` and private `.cpp` — fixed route.
- Create `SourceAssets/Narrative/characters.json` — stable reusable character roles/assets/actions.
- Create `SourceAssets/Narrative/sequence.schema.json` and `guide.schema.json` — authored source contracts.
- Create `scripts/validate_narrative_sequence_json.py` and test — pure Sequence graph/reference validation.
- Create `scripts/validate_guide_json.py` and test — pure Guide graph/semantic-target validation.
- Create `Content/Python/gamexxk_import_character_catalog.py` — deterministic CharacterCatalog import.
- Create `Content/Python/gamexxk_import_narrative_sequence_json.py` — deterministic Sequence import.
- Create `Content/Python/gamexxk_import_guide_json.py` — deterministic Guide import.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h` — progress maps, tracked task and guide state.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` and private `.cpp` — v29 migration from TutorialQuest.
- Modify `GameXXKBattleBoardWidget`, `GameXXKOneGameRouteMapWidget`, `GameXXKRouteMerchantWidget`, `GameXXKRouteEncounterPanelWidget` and `GameXXKDesktopTrainingWorkbenchWidget` to register semantic guide targets/events.
- Create tests for Story/Task, Stage/Profile, Guide, TutorialRoute and migration.

### Task 1: Define CharacterCatalog and the pure NarrativeSequence runtime

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKCharacterCatalog.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKCharacterCatalog.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceTypes.h`
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceAsset.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKNarrativeSequenceAsset.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceRules.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKNarrativeSequenceRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKNarrativeSequenceRulesTest.cpp`

- [ ] **Step 1: Write failing character/sequence contract tests**

Create a transient CharacterCatalog containing Hero/YueBai and a Sequence `start → dialogue → outcome branch → grant → end`. Assert Character entries contain no transform/map fields, dialogue Outcome selects the correct branch, and the grant request remains at its step until committed:

```cpp
FGameXXKNarrativeSequenceSessionState Session;
FGameXXKNarrativeRequest Request;
FGameXXKNarrativeStartContext Context;
Context.StoryId = TEXT("Story.Test");
Context.TaskId = TEXT("Task.Test");
Context.StepId = TEXT("Step.Test");
Context.StageContractId = TEXT("Stage.Test");
TestTrue(TEXT("sequence starts"), FGameXXKNarrativeSequenceRules::Start(*Asset, Context, Session, Request));
TestEqual(TEXT("dialogue requested"), Request.Type, EGameXXKNarrativeRequestType::Dialogue);
TestTrue(TEXT("dialogue outcome resumes"), FGameXXKNarrativeSequenceRules::CompleteDialogue(*Asset, TEXT("Outcome.Accept"), Session, Request));
TestEqual(TEXT("accept branch requests command"), Request.Command.CommandId, FName(TEXT("grant_once")));
```

- [ ] **Step 2: Define the complete runtime surface**

```cpp
UENUM(BlueprintType)
enum class EGameXXKNarrativeStepType : uint8 { Command, Wait, Dialogue, BranchOnOutcome, End };
UENUM(BlueprintType)
enum class EGameXXKNarrativeRequestType : uint8 { None, Command, Wait, Dialogue, Ended };
UENUM(BlueprintType)
enum class EGameXXKNarrativeCommandStatus : uint8 { Completed, Pending, Failed };

USTRUCT(BlueprintType)
struct FGameXXKNarrativeCommandDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName CommandId;
    UPROPERTY(EditAnywhere) FName CommandType;
    UPROPERTY(EditAnywhere) TMap<FName, FString> Arguments;
    UPROPERTY(EditAnywhere) bool bOptional = false;
};

USTRUCT(BlueprintType)
struct FGameXXKNarrativeSequenceStepDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName StepId;
    UPROPERTY(EditAnywhere) EGameXXKNarrativeStepType Type = EGameXXKNarrativeStepType::End;
    UPROPERTY(EditAnywhere) FGameXXKNarrativeCommandDefinition Command;
    UPROPERTY(EditAnywhere) FName WaitType;
    UPROPERTY(EditAnywhere) TMap<FName, FString> WaitArguments;
    UPROPERTY(EditAnywhere) FName DialogueId;
    UPROPERTY(EditAnywhere) TMap<FName, FName> OutcomeToStepId;
    UPROPERTY(EditAnywhere) FName NextStepId;
};

USTRUCT(BlueprintType)
struct FGameXXKNarrativeSequenceSessionState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) bool bActive = false;
    UPROPERTY(SaveGame) FName StoryId;
    UPROPERTY(SaveGame) int32 StoryVersion = 0;
    UPROPERTY(SaveGame) FName TaskId;
    UPROPERTY(SaveGame) FName StepId;
    UPROPERTY(SaveGame) FName SequenceId;
    UPROPERTY(SaveGame) int32 SequenceVersion = 0;
    UPROPERTY(SaveGame) FName StageContractId;
    UPROPERTY(SaveGame) FName CurrentSequenceStepId;
    UPROPERTY(SaveGame) FName AwaitedDialogueId;
    UPROPERTY(SaveGame) FName LastOutcomeId;
    UPROPERTY(SaveGame) TSet<FName> ExecutedCommandKeys;
    UPROPERTY(SaveGame) TMap<FName, FName> CharacterIdByRole;
    UPROPERTY(SaveGame) FString PauseReason;
};

struct FGameXXKNarrativeStartContext
{
    FName StoryId;
    int32 StoryVersion = 1;
    FName TaskId;
    FName StepId;
    FName StageContractId;
    TMap<FName, FName> CharacterIdByRole;
};

struct FGameXXKNarrativeRequest
{
    EGameXXKNarrativeRequestType Type = EGameXXKNarrativeRequestType::None;
    FName SequenceStepId;
    FGameXXKNarrativeCommandDefinition Command;
    FName WaitType;
    TMap<FName, FString> WaitArguments;
    FName DialogueId;
    bool bEnded = false;
};
```

`UGameXXKCharacterCatalog` entries contain `CharacterId`, display name, Actor class/portrait/animation soft references, supported ActionIds and default interaction SequenceId only. `UGameXXKNarrativeSequenceAsset` contains SequenceId/version/entry/StageContract/role defaults/steps and validates stable unique IDs.

- [ ] **Step 3: Implement deterministic step boundaries**

Expose the exact pure API below and bound immediate branch/end traversal to 256 steps:

```cpp
static bool Start(const UGameXXKNarrativeSequenceAsset&, const FGameXXKNarrativeStartContext&, FGameXXKNarrativeSequenceSessionState&, FGameXXKNarrativeRequest&, FString* OutError = nullptr);
static bool Resume(const UGameXXKNarrativeSequenceAsset&, FGameXXKNarrativeSequenceSessionState&, FGameXXKNarrativeRequest&, FString* OutError = nullptr);
static bool CompleteCommand(const UGameXXKNarrativeSequenceAsset&, EGameXXKNarrativeCommandStatus, FGameXXKNarrativeSequenceSessionState&, FGameXXKNarrativeRequest&, FString* OutError = nullptr);
static bool CompleteWait(const UGameXXKNarrativeSequenceAsset&, FGameXXKNarrativeSequenceSessionState&, FGameXXKNarrativeRequest&, FString* OutError = nullptr);
static bool CompleteDialogue(const UGameXXKNarrativeSequenceAsset&, FName OutcomeId, FGameXXKNarrativeSequenceSessionState&, FGameXXKNarrativeRequest&, FString* OutError = nullptr);
static FName MakeCommandKey(FName StoryId, FName TaskId, FName StepId, FName CommandId);
```

A command key is exactly `StoryId/TaskId/StepId/CommandId`; record it only after `Completed`. On reload an already-recorded command is skipped, a `Pending` command is re-issued from the same step, and a failed required command pauses safely.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative/GameXXKCharacterCatalog.h Source/GameXXK/Private/Narrative/GameXXKCharacterCatalog.cpp Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceTypes.h Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceAsset.h Source/GameXXK/Private/Narrative/GameXXKNarrativeSequenceAsset.cpp Source/GameXXK/Public/Narrative/GameXXKNarrativeSequenceRules.h Source/GameXXK/Private/Narrative/GameXXKNarrativeSequenceRules.cpp Source/GameXXK/Private/Tests/GameXXKNarrativeSequenceRulesTest.cpp
git commit -m "feat: add reusable narrative sequence runtime"
```

### Task 2: Validate and import CharacterCatalog and Sequence JSON

**Files:**
- Create: `SourceAssets/Narrative/characters.json`
- Create: `SourceAssets/Narrative/sequence.schema.json`
- Create: `scripts/validate_narrative_sequence_json.py`
- Create: `scripts/test_narrative_sequence_json_validation.py`
- Create: `Content/Python/gamexxk_import_character_catalog.py`
- Create: `Content/Python/gamexxk_import_narrative_sequence_json.py`

- [ ] **Step 1: Write failing source-validator tests**

Cover duplicate Step/Command IDs, dangling next/Outcome targets, unreachable steps, exitless immediate cycles, unknown CharacterId/role/action/StageContract/SlotId/command/wait type, numeric world coordinates, direct map paths and canonical success. Assert `characters.json` contains stable entries for Hero, YueBai, Horse, Carriage and the six current task NPCs without placement fields.

- [ ] **Step 2: Implement pure validation**

Use a `NarrativeCatalogSnapshot` containing CharacterIds, per-character ActionIds, DialogueIds, StageContract→SlotIds, command types, wait types and OutcomeIds. Reject keys `map`, `mapPath`, `worldLocation`, `transform`, `x`, `y`, `z` anywhere in Sequence source. Validate graph reachability and exitless SCCs; canonicalize with UTF-8, `ensure_ascii=False` and sorted keys.

- [ ] **Step 3: Implement idempotent import**

`gamexxk_import_character_catalog.py` imports `/Game/GameXXK/Narrative/Characters/DA_CharacterCatalog`. `gamexxk_import_narrative_sequence_json.py` validates before mutation, imports `DA_<SequenceId dots replaced by underscores>` below `/Game/GameXXK/Narrative/Sequences`, saves only target packages and writes `Saved/HarnessReports/narrative-sequence-import-report.json`.

- [ ] **Step 4: Run GREEN and commit**

```powershell
python -m unittest scripts.test_narrative_sequence_json_validation -v
git add -- SourceAssets/Narrative/characters.json SourceAssets/Narrative/sequence.schema.json scripts/validate_narrative_sequence_json.py scripts/test_narrative_sequence_json_validation.py Content/Python/gamexxk_import_character_catalog.py Content/Python/gamexxk_import_narrative_sequence_json.py
git commit -m "feat: validate and import narrative sequences"
```

### Task 3: Dispatch Sequence requests through one NarrativeCoordinator

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeCommandExecutor.h`
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKNarrativeCoordinator.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKNarrativeCoordinatorTest.cpp`

- [ ] **Step 1: Write failing dispatch/lifecycle tests**

Use fake executors and a fake Dialogue host. Cover duplicate executor registration, unknown required command, optional failure diagnostics, Pending completion, Dialogue Outcome callback, one active Sequence, one input token, pause/exit/resume and map-travel cancellation. Assert a command is committed before the next request is emitted.

- [ ] **Step 2: Define the typed executor/host boundary**

```cpp
struct FGameXXKNarrativeCommandResult
{
    EGameXXKNarrativeCommandStatus Status = EGameXXKNarrativeCommandStatus::Failed;
    FString Error;
};

struct FGameXXKRuntimeState;

class IGameXXKNarrativeCommandExecutor
{
public:
    virtual ~IGameXXKNarrativeCommandExecutor() = default;
    virtual bool Supports(FName CommandType) const = 0;
    virtual FGameXXKNarrativeCommandResult Execute(
        const FGameXXKNarrativeCommandDefinition& Command,
        FGameXXKRuntimeState& InOutCandidateState) = 0;
    virtual void CancelPending() = 0;
};

DECLARE_DELEGATE_OneParam(FGameXXKNarrativeDialogueCompleted, FName /*OutcomeId*/);
```

NarrativeCoordinator receives an injected Dialogue-start delegate rather than including a Widget or actor type. It owns exactly one world-input token for blocking Sequence work; subordinate Dialogue/Guide presenters never stack a second token.

- [ ] **Step 3: Implement safe failure and resume**

For a synchronous command, NarrativeCoordinator clones the complete RuntimeState, lets the executor mutate only that candidate, calls `CompleteCommand` on the candidate SequenceSession, validates the whole candidate, then commits mutation + command key + next step atomically before dispatching the next request. A `Pending` world command does not retain a stale candidate; its completion callback creates a fresh candidate and advances the step. On required failure, store SequenceId/step/command/error, release the input token and restore captured presentation through the registered restore executor. On exit, keep the Sequence active at its replayable boundary. On resume, re-resolve Character roles and Stage slots; never persist Actor pointers or world transforms.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative/GameXXKNarrativeCommandExecutor.h Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h Source/GameXXK/Private/Narrative/GameXXKNarrativeCoordinator.cpp Source/GameXXK/Private/Tests/GameXXKNarrativeCoordinatorTest.cpp
git commit -m "feat: coordinate typed narrative sequence requests"
```

### Task 4: Define Story/Task catalogs and multi-active progress

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeTypes.h`
- Create: `Source/GameXXK/Public/Narrative/GameXXKStoryCatalog.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKStoryCatalog.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKStoryRules.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKStoryRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKStoryTaskCatalogTest.cpp`

- [ ] **Step 1: Write failing catalog/progress tests**

Expect one Story with multiple tasks, concurrent active stories/tasks and one tracked task:

```cpp
const FGameXXKStoryDefinition* Story = FGameXXKStoryCatalog::FindStory(TEXT("Story.Main.XuXiakeTreasure"));
const FGameXXKTaskDefinition* Prologue = FGameXXKStoryCatalog::FindTask(TEXT("Task.Main.XuXiake.Prologue"));
TestNotNull(TEXT("main story exists"), Story);
TestNotNull(TEXT("prologue task exists"), Prologue);
TestTrue(TEXT("prologue belongs to story"), Story->TaskIds.Contains(TEXT("Task.Main.XuXiake.Prologue")));
FGameXXKNarrativeProgress Progress;
TestTrue(TEXT("main story starts"), FGameXXKStoryRules::StartStory(*Story, Progress, Error));
TestTrue(TEXT("first task starts"), FGameXXKStoryRules::StartTask(*Prologue, Progress, Error));
FGameXXKStoryDefinition SideStory;
SideStory.StoryId = TEXT("Story.Side.Test");
SideStory.Version = 1;
SideStory.TaskIds = {TEXT("Task.Side.Test")};
FGameXXKTaskDefinition SideTask;
SideTask.TaskId = TEXT("Task.Side.Test");
SideTask.StoryId = SideStory.StoryId;
SideTask.EntryStepId = TEXT("Step.Side.Test");
FGameXXKTaskStepDefinition SideStep;
SideStep.StepId = SideTask.EntryStepId;
SideTask.Steps = {SideStep};
TestTrue(TEXT("side story may also start"), FGameXXKStoryRules::StartStory(SideStory, Progress, Error));
TestTrue(TEXT("side task may also start"), FGameXXKStoryRules::StartTask(SideTask, Progress, Error));
TestTrue(TEXT("track main task"), FGameXXKStoryRules::TrackTask(Prologue->TaskId, Progress, Error));
TestEqual(TEXT("tracking does not close side story"), Progress.StoryProgressById.Num(), 2);
TestEqual(TEXT("tracking does not close side task"), Progress.TaskProgressById.Num(), 2);
```

- [ ] **Step 2: Define complete types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKStoryState : uint8 { Inactive, Active, Completed };
UENUM(BlueprintType)
enum class EGameXXKTaskState : uint8 { Locked, Available, Active, Completed, Rewarded };

USTRUCT(BlueprintType)
struct FGameXXKTaskStepDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName StepId;
    UPROPERTY(EditAnywhere) FName SequenceId;
    UPROPERTY(EditAnywhere) FName EncounterId;
    UPROPERTY(EditAnywhere) FName RouteId;
    UPROPERTY(EditAnywhere) FName StageContractId;
    UPROPERTY(EditAnywhere) FName GuideId;
    UPROPERTY(EditAnywhere) TArray<FName> NextStepIds;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName TaskId;
    UPROPERTY(EditAnywhere) FName StoryId;
    UPROPERTY(EditAnywhere) TArray<FName> PrerequisiteTaskIds;
    UPROPERTY(EditAnywhere) FName EntryStepId;
    UPROPERTY(EditAnywhere) TArray<FGameXXKTaskStepDefinition> Steps;
};

USTRUCT(BlueprintType)
struct FGameXXKStoryDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName StoryId;
    UPROPERTY(EditAnywhere) int32 Version = 1;
    UPROPERTY(EditAnywhere) TArray<FName> TaskIds;
};

USTRUCT(BlueprintType)
struct FGameXXKStoryProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 Version = 1;
    UPROPERTY(SaveGame) EGameXXKStoryState State = EGameXXKStoryState::Inactive;
    UPROPERTY(SaveGame) TSet<FName> ActiveTaskIds;
    UPROPERTY(SaveGame) TSet<FName> CompletedTaskIds;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) EGameXXKTaskState State = EGameXXKTaskState::Locked;
    UPROPERTY(SaveGame) FName CurrentStepId;
    UPROPERTY(SaveGame) TMap<FName, int32> ObjectiveCounts;
    UPROPERTY(SaveGame) bool bRewardCommitted = false;
};

USTRUCT(BlueprintType)
struct FGameXXKNarrativeProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TMap<FName, FGameXXKStoryProgress> StoryProgressById;
    UPROPERTY(SaveGame) TMap<FName, FGameXXKTaskProgress> TaskProgressById;
    UPROPERTY(SaveGame) FName TrackedTaskId;
};
```

- [ ] **Step 3: Implement pure story rules**

Add start/advance/complete/reward/track APIs. Validate prerequisites, StoryId ownership, stable IDs and one-time rewards. Tracking changes only `TrackedTaskId`.

Seed the production catalog with `Story.Main.XuXiakeTreasure` and `Task.Main.XuXiake.Prologue`. Its entry `Step.Main.XuXiake.RiverScroll` references `Sequence.Main.XuXiake.CarriageArrival` plus `Stage.Tutorial.River` and advances only to `Step.Main.XuXiake.CombatTutorial`; that second step references `Route.Tutorial.CombatBasics`, `Encounter.Main.XuXiake.0-1` and the tutorial Guide set. Settlement completes the task exactly once.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative/GameXXKNarrativeTypes.h Source/GameXXK/Public/Narrative/GameXXKStoryCatalog.h Source/GameXXK/Private/Narrative/GameXXKStoryCatalog.cpp Source/GameXXK/Public/Narrative/GameXXKStoryRules.h Source/GameXXK/Private/Narrative/GameXXKStoryRules.cpp Source/GameXXK/Private/Tests/GameXXKStoryTaskCatalogTest.cpp
git commit -m "feat: add multi-story task orchestration"
```

### Task 5: Add StageContract, SceneProfile and replaceable registry

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKStageContract.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKStageContract.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKSceneProfile.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKSceneProfile.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKSceneRegistry.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKSceneRegistry.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKSceneProfileTest.cpp`

- [ ] **Step 1: Write failing profile validation/swap tests**

Create a StageContract requiring carriage, hero, YueBai, camera, recovery and encounter slots. Expect incomplete profiles to be rejected and two complete profiles to swap without changing Story/Task/Dialogue IDs.

- [ ] **Step 2: Define binding types**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKSceneSlotBinding
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName SlotId;
    UPROPERTY(EditAnywhere) FTransform RelativeTransform;
};

USTRUCT(BlueprintType)
struct FGameXXKNpcScenePlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName CharacterId;
    UPROPERTY(EditAnywhere) FName HomeSlotId;
    UPROPERTY(EditAnywhere) FName InteractionAnchorSlotId;
    UPROPERTY(EditAnywhere) FName PatrolRegionId;
};
```

`UGameXXKStageContract` holds required slot categories. `UGameXXKSceneProfile` holds `SceneProfileId`, `StageContractId`, map soft path, root tag, bindings, NPC placements, trigger regions and safe slot. `UGameXXKSceneRegistry` maps StageContractId to one active profile.

- [ ] **Step 3: Implement validation and resolution**

Reject duplicate/missing slots, invalid NPC CharacterIds, missing safe slot, map mismatch and profile/contract mismatch. Resolve world transforms relative to one scene root. Runtime missing-slot failure returns an error and safe slot; it never substitutes zero coordinates.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative/GameXXKStageContract.h Source/GameXXK/Private/Narrative/GameXXKStageContract.cpp Source/GameXXK/Public/Narrative/GameXXKSceneProfile.h Source/GameXXK/Private/Narrative/GameXXKSceneProfile.cpp Source/GameXXK/Public/Narrative/GameXXKSceneRegistry.h Source/GameXXK/Private/Narrative/GameXXKSceneRegistry.cpp Source/GameXXK/Private/Tests/GameXXKSceneProfileTest.cpp
git commit -m "feat: resolve replaceable narrative scene profiles"
```

### Task 6: Separate encounter triggers from BattleProfile

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKNarrativeEncounterCatalog.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKNarrativeEncounterCatalog.cpp`
- Create: `Source/GameXXK/Public/Narrative/GameXXKBattleProfile.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKBattleProfile.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKNarrativeEncounterProfileTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Write failing boundary tests**

Expect `Encounter.Main.XuXiake.0-1` to resolve enemy/rule/reward data plus `BattleProfile.Tutorial.0-1`, while SceneProfile exposes only `Tutorial.River.EncounterTrigger`. Assert no scene transform appears in encounter/battle rules and triggering launches the existing full-screen BattleBoard.

- [ ] **Step 2: Define BattleProfile**

`FGameXXKNarrativeEncounterDefinition` stores EncounterId, enemy IDs, rule-set ID, reward-table ID and BattleProfileId. BattleProfile stores normalized party/enemy anchors, camera anchors and VFX slots. Validate 0–1 normalized coordinates, unique IDs and required 3-party/1–3-enemy slots. Neither type references a town map.

- [ ] **Step 3: Integrate and commit**

```powershell
git add -- Source/GameXXK/Public/Narrative/GameXXKNarrativeEncounterCatalog.h Source/GameXXK/Private/Narrative/GameXXKNarrativeEncounterCatalog.cpp Source/GameXXK/Public/Narrative/GameXXKBattleProfile.h Source/GameXXK/Private/Narrative/GameXXKBattleProfile.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKNarrativeEncounterProfileTest.cpp
git commit -m "feat: separate encounter triggers from battle layout"
```

### Task 7: Implement GuideAsset, pure rules and semantic target registry

**Files:**
- Create: `Source/GameXXK/Public/Guide/GameXXKGuideAsset.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKGuideAsset.cpp`
- Create: `Source/GameXXK/Public/Guide/GameXXKGuideRules.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKGuideRules.cpp`
- Create: `Source/GameXXK/Public/Guide/GameXXKGuideTargetRegistry.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKGuideTargetRegistry.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKGuideRulesTest.cpp`
- Create: `SourceAssets/Narrative/guide.schema.json`
- Create: `scripts/validate_guide_json.py`
- Create: `scripts/test_guide_json_validation.py`
- Create: `Content/Python/gamexxk_import_guide_json.py`

- [ ] **Step 1: Write failing guide tests**

Cover Forced/Soft steps, trigger/completion events, semantic target lookup, resume, completed steps, invalid target unlock and one guide at a time.

- [ ] **Step 2: Define guide types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKGuideInputPolicy : uint8 { Soft, Forced };
UENUM(BlueprintType)
enum class EGameXXKGuidePreference : uint8 { Unset, NewPlayer, ExperiencedPlayer };

USTRUCT(BlueprintType)
struct FGameXXKGuideStepDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName StepId;
    UPROPERTY(EditAnywhere) FName TriggerEventId;
    UPROPERTY(EditAnywhere) FName TargetId;
    UPROPERTY(EditAnywhere) EGameXXKGuideInputPolicy InputPolicy = EGameXXKGuideInputPolicy::Soft;
    UPROPERTY(EditAnywhere) FText Text;
    UPROPERTY(EditAnywhere) TSet<FName> AllowedActionIds;
    UPROPERTY(EditAnywhere) FName CompletionEventId;
    UPROPERTY(EditAnywhere) FName NextStepId;
};

USTRUCT(BlueprintType)
struct FGameXXKGuideProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) EGameXXKGuidePreference Preference = EGameXXKGuidePreference::Unset;
    UPROPERTY(SaveGame) FName ActiveGuideId;
    UPROPERTY(SaveGame) FName ActiveGuideStepId;
    UPROPERTY(SaveGame) TSet<FName> CompletedGuideStepIds;
    UPROPERTY(SaveGame) FString LastDiagnostic;
};
```

- [ ] **Step 3: Implement target lifecycle**

Widgets register `TargetId`, weak Widget pointer and screen-rect resolver. Duplicate live IDs are rejected. Unregister on reconstruction/destruction. If a Forced step target disappears, rules mark the step diagnostic, coordinator releases its input token, and the player may continue.

- [ ] **Step 4: Write and run failing Guide JSON tests**

Write Python tests for duplicate/dangling/unreachable guide steps, unknown TriggerEventId/CompletionEventId/TargetId/AllowedActionId, empty Forced target, more than one terminal path and canonical success.

```powershell
python -m unittest scripts.test_guide_json_validation -v
```

Expected: RED because the schema/validator/importer do not exist.

- [ ] **Step 5: Implement Guide JSON validation/import**

The importer validates before mutation, writes `UGameXXKGuideAsset` below `/Game/GameXXK/Narrative/Guides`, saves only target packages and emits `Saved/HarnessReports/guide-import-report.json`.

- [ ] **Step 6: Run GREEN and commit**

```powershell
python -m unittest scripts.test_guide_json_validation -v
git add -- Source/GameXXK/Public/Guide Source/GameXXK/Private/Guide Source/GameXXK/Private/Tests/GameXXKGuideRulesTest.cpp SourceAssets/Narrative/guide.schema.json scripts/validate_guide_json.py scripts/test_guide_json_validation.py Content/Python/gamexxk_import_guide_json.py
git commit -m "feat: add semantic tutorial guide rules"
```

### Task 8: Add fixed tutorial route and per-node GuideAssets

**Files:**
- Create: `Source/GameXXK/Public/GameXXKTutorialRouteRules.h`
- Create: `Source/GameXXK/Private/GameXXKTutorialRouteRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTutorialRouteRulesTest.cpp`
- Create: `SourceAssets/Narrative/Guides/Guide.RouteMap.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Battle.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Merchant.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Event.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Camp.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Chest.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Boss.Basic.guide.json`
- Create: `SourceAssets/Narrative/Guides/Guide.Settlement.Basic.guide.json`

- [ ] **Step 1: Write failing exact-route tests**

Assert exact node order/kinds and only adjacent directed edges:

```text
Tutorial.Start(Start)
Tutorial.Battle.0-1(Battle)
Tutorial.Merchant.0-1(Merchant)
Tutorial.Event.0-1(Event)
Tutorial.Camp.0-1(Camp)
Tutorial.Chest.0-1(Chest)
Tutorial.Boss.0-1(Boss)
Tutorial.Settlement(terminal settlement)
```

Start is auto-occupied; only the next incomplete node is reachable; no random seed changes the graph.

- [ ] **Step 2: Implement fixed route and guide definitions**

Create `Route.Tutorial.CombatBasics` plus the eight named Guide sources. Use dynamic IDs `Battle.Hand.FirstPlayableTargetedCard` and `Battle.Enemy.FirstLegalTarget`; `Guide.Boss.Basic` is Soft only. Camp actions are exactly `CurrentHealth = Min(MaxHealth, CurrentHealth + Ceil(MaxHealth * 0.30))` for every active party member or gain 100 route gold; no talisman/item action appears in this route.

- [ ] **Step 3: Validate/import all eight GuideAssets**

Run:

```powershell
python scripts/validate_guide_json.py SourceAssets/Narrative/Guides
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_import_guide_json.py'), ensure_ascii=False)); print(json.dumps(c.save_dirty_packages(), ensure_ascii=False))"
```

Expect exactly eight assets under `/Game/GameXXK/Narrative/Guides`, every TargetId present in the frozen semantic-target catalog, and 0 coordinate/widget-name fields.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKTutorialRouteRules.h Source/GameXXK/Private/GameXXKTutorialRouteRules.cpp Source/GameXXK/Private/Tests/GameXXKTutorialRouteRulesTest.cpp SourceAssets/Narrative/Guides Content/GameXXK/Narrative/Guides
git commit -m "feat: add fixed combat tutorial route"
```

### Task 9: Build guide overlay, first-entry choice and settings reset

**Files:**
- Create: `Source/GameXXK/Public/Guide/GameXXKGuideCoordinator.h`
- Create: `Source/GameXXK/Private/Guide/GameXXKGuideCoordinator.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKGuideOverlayWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKGuideOverlayWidget.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKGuidePreferenceWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKGuidePreferenceWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKGuideWidgetTest.cpp`

- [ ] **Step 1: Write failing prompt/overlay tests**

Expect exact buttons `我是老玩家，跳过` and `我是新手，继续`; prompt only when preference is Unset; ExperiencedPlayer runs route with no guide; NewPlayer starts guide; reset returns Unset. Forced overlay allows only registered AllowedActionIds, Soft overlay never blocks input.

- [ ] **Step 2: Implement one tokenized input lock**

Overlay owns arrow, dim mask and text anchored to the target rect. Coordinator acquires/releases one input token per Forced step, never stacks locks, and releases on missing target, completion, cancel, Widget destruction or map travel.

- [ ] **Step 3: Implement settings reset**

Add `重置战斗引导` to the existing HUD settings panel built by `UGameXXKDesktopTrainingWorkbenchWidget::BuildHudSettingsPanel`. Confirming sets Preference to Unset, clears only active/completed combat-guide state, saves immediately and does not reset route/task rewards.

- [ ] **Step 4: Run GREEN and commit**

```powershell
python scripts/ue_mcp_client.py automation run GameXXK.Guide
git add -- Source/GameXXK/Public/Guide/GameXXKGuideCoordinator.h Source/GameXXK/Private/Guide/GameXXKGuideCoordinator.cpp Source/GameXXK/Public/UI/GameXXKGuideOverlayWidget.h Source/GameXXK/Private/UI/GameXXKGuideOverlayWidget.cpp Source/GameXXK/Public/UI/GameXXKGuidePreferenceWidget.h Source/GameXXK/Private/UI/GameXXKGuidePreferenceWidget.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKGuideWidgetTest.cpp
git commit -m "feat: present configurable combat guidance"
```

### Task 10: Persist/migrate v29 and register live UI targets

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKNarrativeGuideSaveMigrationTest.cpp`

- [ ] **Step 1: Write failing v28→v29 tests**

Expect empty multi-story maps and an inactive Sequence session for ordinary saves; migrate active/completed v27 TutorialQuest to `Story.Main.XuXiakeTreasure` / `Task.Main.XuXiake.Prologue`; preserve the v28 Dialogue session; initialize GuidePreference Unset and empty completed steps.

- [ ] **Step 2: Add v29 state**

Add `NarrativeStageGuideIntroducedSaveVersion = 29`, `NarrativeSequenceSession`, Story/Task progress maps, TrackedTaskId and guide progress to RuntimeState. Validate Sequence/Story/Task/Step/Stage/command-key IDs against catalogs and reject an active guide without an active task/step. Invalid Actor pointers/transforms cannot occur because neither is serializable in the state type.

- [ ] **Step 3: Register semantic targets/events**

Register/unregister these exact IDs in their owning Widgets and emit completion events after authoritative mutations, not raw clicks:

```text
GameXXKOneGameRouteMapWidget: Route.Tutorial.NextNode, Route.Settlement.Confirm
GameXXKBattleBoardWidget: Battle.Hud.PartyQi, Battle.Hand.FirstPlayableTargetedCard,
                          Battle.Enemy.FirstLegalTarget, Battle.EndTurn
GameXXKRouteMerchantWidget: Route.Merchant.CardRow, Route.Merchant.RelicRow, Route.Merchant.Leave
GameXXKRouteEncounterPanelWidget: Route.Event.ValidChoiceGroup, Route.Camp.Heal,
                                  Route.Camp.Gold, Route.Chest.Open
GameXXKDesktopTrainingWorkbenchWidget: Desktop.Settings.ResetCombatGuide
```

No guide code searches Widget names or stores screen coordinates.

- [ ] **Step 4: Run final gates**

Save dirty packages through MCP, stop PIE and close the editor before the cold builds. Then run:

```powershell
python -m unittest scripts.test_narrative_sequence_json_validation scripts.test_guide_json_validation -v
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' '-ExecCmds=Automation RunTests GameXXK.Narrative; Automation RunTests GameXXK.Guide; Automation RunTests GameXXK.MVP.RouteMap; Automation RunTests GameXXK.Integration.CardBattle; Automation RunTests GameXXK.DesktopTraining.BattleBoard; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE5 demo/GameXXK/Saved/Automation/NarrativeStageGuideFinal'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXK Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
```

Expected: all Python/Automation tests pass, both targets compile, and a test SceneProfile swap changes no Story/Task/Dialogue ID.

- [ ] **Step 5: Commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKNarrativeGuideSaveMigrationTest.cpp
git commit -m "feat: persist narrative stage and guide progress"
```
