# Dialogue Core Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the JSON-authored, compiled, deterministic and save-resumable dialogue core without any presentation or world-actor dependencies.

**Architecture:** JSON source files are validated outside UE and imported into `UGameXXKDialogueAsset`. `FGameXXKDialogueRules` advances a pure `FGameXXKDialogueSessionState`, emits presentation views, and returns stable OutcomeIds to its caller. It records only dialogue choices, reads and history; NarrativeSequence owns commands, waits and command idempotency in the following implementation plan. No task, shop, actor or Widget code is called from the rules layer.

**Tech Stack:** Unreal Engine 5.8 C++, UPrimaryDataAsset, UE JSON/Python editor import, GameXXK RuntimeState/save migration, Automation Tests, Python unittest.

---

## File map

- Create `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h` — serialized definitions, runtime session and emitted requests.
- Create `Source/GameXXK/Public/Dialogue/GameXXKDialogueAsset.h` — compiled primary data asset.
- Create `Source/GameXXK/Private/Dialogue/GameXXKDialogueAsset.cpp` — asset lookup and validation helpers.
- Create `Source/GameXXK/Public/Dialogue/GameXXKDialogueRules.h` — pure start/advance/choose/complete APIs.
- Create `Source/GameXXK/Private/Dialogue/GameXXKDialogueRules.cpp` — deterministic node machine.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h` — add dialogue state to `FGameXXKRuntimeState`.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` — add save v28 constant.
- Modify `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` — initialize and validate dialogue state.
- Create `Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp` — rules, branch, condition and outcome coverage.
- Create `Source/GameXXK/Private/Tests/GameXXKDialogueSaveMigrationTest.cpp` — v27→v28 migration coverage.
- Create `SourceAssets/Narrative/dialogue.schema.json` — source contract.
- Create `scripts/validate_dialogue_json.py` — pure JSON validation and canonical output.
- Create `scripts/test_dialogue_json_validation.py` — validator tests.
- Create `Content/Python/gamexxk_import_dialogue_json.py` — import validated JSON into UE assets.

### Task 1: Define dialogue types and the compiled asset

**Files:**
- Create: `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h`
- Create: `Source/GameXXK/Public/Dialogue/GameXXKDialogueAsset.h`
- Create: `Source/GameXXK/Private/Dialogue/GameXXKDialogueAsset.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp`

- [ ] **Step 1: Write the failing asset/type test**

Add an Automation test named `GameXXK.Dialogue.Core.AssetContract` that constructs a transient asset and expects a stable entry lookup:

```cpp
UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
Asset->DialogueId = TEXT("Dialogue.Test.Branching");
Asset->DialogueVersion = 1;
Asset->EntryNodeId = TEXT("start");
FGameXXKDialogueNodeDefinition Start;
Start.NodeId = TEXT("start");
Start.Type = EGameXXKDialogueNodeType::Line;
Start.NextNodeId = TEXT("end");
Asset->Nodes.Add(Start);
TestNotNull(TEXT("entry resolves"), Asset->FindNode(TEXT("start")));
TestNull(TEXT("missing node rejects"), Asset->FindNode(TEXT("missing")));
```

- [ ] **Step 2: Build to verify RED**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
```

Expected: compile failure because the dialogue headers/classes do not exist.

- [ ] **Step 3: Add the minimal complete type surface**

Define these enums and structures in `GameXXKDialogueTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKDialogueNodeType : uint8 { Line, Choice, End };

UENUM(BlueprintType)
enum class EGameXXKDialoguePresentation : uint8 { Bubble, DialoguePanel, None };

USTRUCT(BlueprintType)
struct FGameXXKDialogueOptionDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName OptionId;
    UPROPERTY(EditAnywhere) FName TextId;
    UPROPERTY(EditAnywhere) FText Text;
    UPROPERTY(EditAnywhere) FName OutcomeId;
    UPROPERTY(EditAnywhere) FName NextNodeId;
    UPROPERTY(EditAnywhere) TMap<FName, FString> Conditions;
    UPROPERTY(EditAnywhere) FText DisabledReason;
};

USTRUCT(BlueprintType)
struct FGameXXKDialogueNodeDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName NodeId;
    UPROPERTY(EditAnywhere) EGameXXKDialogueNodeType Type = EGameXXKDialogueNodeType::Line;
    UPROPERTY(EditAnywhere) EGameXXKDialoguePresentation Presentation = EGameXXKDialoguePresentation::DialoguePanel;
    UPROPERTY(EditAnywhere) FName SpeakerId;
    UPROPERTY(EditAnywhere) FName TextId;
    UPROPERTY(EditAnywhere) FText Text;
    UPROPERTY(EditAnywhere) FName EndOutcomeId;
    UPROPERTY(EditAnywhere) FName NextNodeId;
    UPROPERTY(EditAnywhere) TArray<FGameXXKDialogueOptionDefinition> Options;
    UPROPERTY(EditAnywhere) TMap<FName, FString> Conditions;
};
```

Define `UGameXXKDialogueAsset : public UPrimaryDataAsset` with `DialogueId`, `DialogueVersion`, `EntryNodeId`, `Nodes`, and a const `FindNode(FName)` lookup. Reject duplicate IDs in `IsDataValid`.

- [ ] **Step 4: Build and run GREEN test**

Run the Editor build above, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' '-ExecCmds=Automation RunTests GameXXK.Dialogue.Core.AssetContract; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE5 demo/GameXXK/Saved/Automation/DialogueAssetContract'
```

Expected: 1 passed, 0 failed.

- [ ] **Step 5: Commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue Source/GameXXK/Private/Dialogue Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp
git commit -m "feat: add compiled dialogue asset types"
```

### Task 2: Implement the deterministic Runner

**Files:**
- Create: `Source/GameXXK/Public/Dialogue/GameXXKDialogueRules.h`
- Create: `Source/GameXXK/Private/Dialogue/GameXXKDialogueRules.cpp`
- Modify: `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp`

- [ ] **Step 1: Add failing start/advance/choice tests**

Define a branching fixture `start -> choice -> left/right -> end`. Expect:

```cpp
FGameXXKDialogueSessionState Session;
FGameXXKDialogueOutput Output;
FGameXXKDialogueStartContext StartContext;
StartContext.StoryId = TEXT("Story.Test");
StartContext.TaskId = TEXT("Task.Test");
StartContext.StepId = TEXT("Step.Test");
StartContext.SequenceId = TEXT("Sequence.Test");
StartContext.StageContractId = TEXT("Stage.Test");
TestTrue(TEXT("start succeeds"), FGameXXKDialogueRules::Start(*Asset, StartContext, Session, Output));
TestEqual(TEXT("start node shown"), Output.NodeId, FName(TEXT("start")));
TestTrue(TEXT("line completes"), FGameXXKDialogueRules::CompletePresentedNode(*Asset, Session, Output));
TestEqual(TEXT("choice reached"), Session.CurrentNodeId, FName(TEXT("choice")));
TestTrue(TEXT("right choice accepted"), FGameXXKDialogueRules::Choose(*Asset, TEXT("right"), Session, Output));
TestEqual(TEXT("right branch reached"), Session.CurrentNodeId, FName(TEXT("right.line")));
```

Also assert invalid option, unavailable option and second active session are rejected without mutation.

- [ ] **Step 2: Run RED**

Run `GameXXK.Dialogue.Core` and expect missing `FGameXXKDialogueRules` APIs.

- [ ] **Step 3: Implement state and APIs**

Add session/output types:

```cpp
USTRUCT(BlueprintType)
struct FGameXXKDialogueHistoryEntry
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName SpeakerId;
    UPROPERTY(SaveGame) FName TextId;
    UPROPERTY(SaveGame) FText Text;
    UPROPERTY(SaveGame) FName SelectedOptionId;
};

USTRUCT(BlueprintType)
struct FGameXXKDialogueSessionState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) bool bActive = false;
    UPROPERTY(SaveGame) FName StoryId;
    UPROPERTY(SaveGame) int32 StoryVersion = 0;
    UPROPERTY(SaveGame) FName TaskId;
    UPROPERTY(SaveGame) FName StepId;
    UPROPERTY(SaveGame) FName SequenceId;
    UPROPERTY(SaveGame) FName StageContractId;
    UPROPERTY(SaveGame) FName DialogueId;
    UPROPERTY(SaveGame) int32 DialogueVersion = 0;
    UPROPERTY(SaveGame) FName CurrentNodeId;
    UPROPERTY(SaveGame) TSet<FName> SeenNodeIds;
    UPROPERTY(SaveGame) TArray<FName> SelectedOptionIds;
    UPROPERTY(SaveGame) TArray<FGameXXKDialogueHistoryEntry> History;
    UPROPERTY(SaveGame) FString PauseReason;
};

USTRUCT(BlueprintType)
struct FGameXXKDialogueVisibleOption
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName OptionId;
    UPROPERTY(BlueprintReadOnly) FText Text;
    UPROPERTY(BlueprintReadOnly) bool bEnabled = true;
    UPROPERTY(BlueprintReadOnly) FText DisabledReason;
};

USTRUCT(BlueprintType)
struct FGameXXKDialogueOutput
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName NodeId;
    UPROPERTY(BlueprintReadOnly) EGameXXKDialoguePresentation Presentation = EGameXXKDialoguePresentation::None;
    UPROPERTY(BlueprintReadOnly) FName SpeakerId;
    UPROPERTY(BlueprintReadOnly) FName TextId;
    UPROPERTY(BlueprintReadOnly) FText Text;
    UPROPERTY(BlueprintReadOnly) TArray<FGameXXKDialogueVisibleOption> Options;
    UPROPERTY(BlueprintReadOnly) FName OutcomeId;
    UPROPERTY(BlueprintReadOnly) bool bEnded = false;
};
```

Expose pure APIs:

```cpp
struct FGameXXKDialogueStartContext
{
    FName StoryId;
    int32 StoryVersion = 1;
    FName TaskId;
    FName StepId;
    FName SequenceId;
    FName StageContractId;
};

static bool Start(const UGameXXKDialogueAsset&, const FGameXXKDialogueStartContext&, FGameXXKDialogueSessionState&, FGameXXKDialogueOutput&, FString* OutError=nullptr);
static bool CompletePresentedNode(const UGameXXKDialogueAsset&, FGameXXKDialogueSessionState&, FGameXXKDialogueOutput&, FString* OutError=nullptr);
static bool Choose(const UGameXXKDialogueAsset&, FName OptionId, FGameXXKDialogueSessionState&, FGameXXKDialogueOutput&, FString* OutError=nullptr);
static bool Resume(const UGameXXKDialogueAsset&, FGameXXKDialogueSessionState&, FGameXXKDialogueOutput&, FString* OutError=nullptr);
```

Bound internal advancement to 256 immediate nodes per call. A selected option records its OutcomeId before entering `NextNodeId`; an End node returns `EndOutcomeId` and clears `bActive`. Append history only for displayed lines/selections and trim oldest entries above 100.

- [ ] **Step 4: Run GREEN**

Run `GameXXK.Dialogue.Core`; expected all Runner tests pass without loading a map.

- [ ] **Step 5: Commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue Source/GameXXK/Private/Dialogue Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp
git commit -m "feat: add deterministic dialogue runner"
```

### Task 3: Add condition evaluation and stable outcomes

**Files:**
- Modify: `Source/GameXXK/Public/Dialogue/GameXXKDialogueRules.h`
- Modify: `Source/GameXXK/Private/Dialogue/GameXXKDialogueRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp`

- [ ] **Step 1: Write failing condition/outcome tests**

Create `FGameXXKDialogueConditionContext` with flags, item counts, gold, unlocked companions, selected options and seen nodes. Test a hidden option, a disabled option with reason, a selected option OutcomeId and a terminal End OutcomeId:

```cpp
Context.Gold = 4999;
TestFalse(TEXT("5000-gold option unavailable"),
    FGameXXKDialogueRules::EvaluateConditions(Option.Conditions, Context));
TestTrue(TEXT("visible choice succeeds"),
    FGameXXKDialogueRules::Choose(*Asset, TEXT("salvage_scroll"), Session, Output));
TestEqual(TEXT("choice outcome returned"), Output.OutcomeId,
    FName(TEXT("Outcome.Tutorial.SalvageRiverMap")));
```

- [ ] **Step 2: Run RED**

Expected failure because condition APIs do not exist.

- [ ] **Step 3: Implement registered conditions**

Define the complete pure context before evaluation:

```cpp
struct FGameXXKDialogueConditionContext
{
    TSet<FName> Flags;
    TMap<FName, int32> ItemCounts;
    int32 Gold = 0;
    TSet<FName> UnlockedCompanionIds;
    TSet<FName> SelectedOptionIds;
    TSet<FName> SeenNodeIds;
    EGameXXKTutorialQuestState TutorialState = EGameXXKTutorialQuestState::NotStarted;
    TMap<FName, int32> TaskStateValues;
};
```

Support exactly: `flag`, `tutorialState`, `taskState`, `itemAtLeast`, `goldAtLeast`, `companionUnlocked`, `optionSelected`, `nodeSeen`. Unknown conditions fail closed and return an error. Reject empty/duplicate OutcomeIds and require every Choice option and End node to return a non-empty stable OutcomeId.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue Source/GameXXK/Private/Dialogue Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp
git commit -m "feat: validate dialogue conditions and outcomes"
```

### Task 4: Persist and migrate dialogue sessions

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDialogueSaveMigrationTest.cpp`

- [ ] **Step 1: Write failing v27→v28 migration tests**

Expect v27 saves to receive an empty session while preserving `TutorialQuest`; expect malformed active sessions to be normalized:

```cpp
FGameXXKSaveState Source;
Source.SaveVersion = 27;
Source.RuntimeState.TutorialQuest.State = EGameXXKTutorialQuestState::Active;
FGameXXKSaveState Migrated;
FGameXXKSaveMigrationReport Report;
TestTrue(TEXT("migration succeeds"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
TestEqual(TEXT("save reaches v28"), Migrated.SaveVersion, 28);
TestFalse(TEXT("dialogue defaults inactive"), Migrated.RuntimeState.DialogueSession.bActive);
TestEqual(TEXT("tutorial preserved"), Migrated.RuntimeState.TutorialQuest.State, EGameXXKTutorialQuestState::Active);
```

- [ ] **Step 2: Run RED**

Expected compile/test failure because v28/session field do not exist.

- [ ] **Step 3: Add migration and validation**

Add `DialogueRuntimeIntroducedSaveVersion = 28`, advance `CurrentSaveVersion`, add `DialogueSession` to `FGameXXKRuntimeState`, initialize it for older saves, and reject these invalid combinations:

- inactive with non-empty Story/Task/Step/Sequence/Stage/Dialogue/current-node context;
- active with empty Story/Task/Step/Sequence/Stage/Dialogue/current-node context or non-positive Story/Dialogue version;
- more than 100 history entries;
- empty option IDs in committed selections.

- [ ] **Step 4: Run save tests and commit**

Run `GameXXK.Dialogue.SaveMigration` and `GameXXK.MVP.SaveGame`; expect 0 failures.

```powershell
git add -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKDialogueSaveMigrationTest.cpp
git commit -m "feat: persist dialogue sessions in save v28"
```

### Task 5: Validate JSON sources outside UE

**Files:**
- Create: `SourceAssets/Narrative/dialogue.schema.json`
- Create: `scripts/validate_dialogue_json.py`
- Create: `scripts/test_dialogue_json_validation.py`

- [ ] **Step 1: Write failing Python tests**

Cover duplicate IDs, dangling `next`, unreachable nodes, unregistered condition, empty/duplicate OutcomeIds, choice count 5, exitless cycle, missing speaker/role IDs and canonical success. The success fixture must compile to a canonical dict sorted by stable IDs.

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_dialogue_json_validation -v
```

Expected: import failure because validator/schema are missing.

- [ ] **Step 3: Implement validator**

Expose:

```python
@dataclass(frozen=True)
class CatalogSnapshot:
    speakers: frozenset[str]
    roles: frozenset[str]
    outcomes: frozenset[str]

def validate_dialogue(payload: dict, catalogs: CatalogSnapshot) -> list[str]:
    errors: list[str] = []
    nodes = payload.get("nodes")
    if not isinstance(nodes, dict) or not nodes:
        return ["nodes must be a non-empty object"]
    entry = payload.get("entryNode")
    if entry not in nodes:
        errors.append(f"entry node does not exist: {entry!r}")
    allowed_nodes = {"line", "choice", "end"}
    allowed_conditions = REGISTERED_CONDITION_TYPES
    adjacency: dict[str, set[str]] = {node_id: set() for node_id in nodes}
    for node_id, node in nodes.items():
        if node.get("type") not in allowed_nodes:
            errors.append(f"{node_id}: unknown node type")
        targets = _node_targets(node)
        adjacency[node_id].update(targets)
        for target in targets:
            if target not in nodes:
                errors.append(f"{node_id}: missing target {target}")
        errors.extend(_resource_errors(node_id, node, catalogs))
        errors.extend(_registered_condition_and_outcome_errors(node_id, node, allowed_conditions, catalogs.outcomes))
    if entry in nodes:
        unreachable = set(nodes) - _reachable_nodes(entry, adjacency)
        errors.extend(f"unreachable node: {node_id}" for node_id in sorted(unreachable))
    errors.extend(_exitless_cycle_errors(nodes, adjacency))
    return sorted(set(errors))

def canonicalize_dialogue(payload: dict) -> dict:
    return json.loads(json.dumps(payload, ensure_ascii=False, sort_keys=True))

def validate_file(path: Path, catalogs: CatalogSnapshot) -> dict:
    payload = json.loads(path.read_text(encoding="utf-8"))
    errors = validate_dialogue(payload, catalogs)
    if errors:
        raise ValueError("\n".join(errors))
    return canonicalize_dialogue(payload)
```

Register only the node and condition names frozen in the design. Validate speaker/role and Outcome references, all graph edges with DFS and Tarjan SCC; an SCC is valid only when it contains an explicit conditional exit.

- [ ] **Step 4: Run GREEN and commit**

```powershell
python -m unittest scripts.test_dialogue_json_validation -v
git add -- SourceAssets/Narrative/dialogue.schema.json scripts/validate_dialogue_json.py scripts/test_dialogue_json_validation.py
git commit -m "feat: validate dialogue json sources"
```

### Task 6: Import validated JSON into UE and finish the core gate

**Files:**
- Create: `Content/Python/gamexxk_import_dialogue_json.py`
- Modify: `scripts/test_dialogue_json_validation.py`
- Test: `Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp`

- [ ] **Step 1: Add a failing importer source contract**

The Python test must assert that the importer calls `validate_file`, creates/loads `UGameXXKDialogueAsset`, assigns every compiled field, saves only the target package, and writes `Saved/HarnessReports/dialogue-import-report.json`.

- [ ] **Step 2: Implement idempotent import**

Use `unreal.AssetToolsHelpers.get_asset_tools().create_asset` for missing assets and `EditorAssetLibrary.load_asset` for existing ones. Destination naming is `DA_<dialogueId with dots replaced by underscores>` below `/Game/GameXXK/Narrative/Dialogues`. Abort before asset mutation when validation returns errors.

- [ ] **Step 3: Run the core verification gate**

```powershell
python -m unittest scripts.test_dialogue_json_validation -v
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' '-ExecCmds=Automation RunTests GameXXK.Dialogue; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE5 demo/GameXXK/Saved/Automation/DialogueCoreFinal'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXK Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
```

Expected: Python tests green, Editor and Game Target builds succeed, all `GameXXK.Dialogue` tests pass with 0 warnings/errors.

- [ ] **Step 4: Commit**

```powershell
git add -- Content/Python/gamexxk_import_dialogue_json.py scripts/test_dialogue_json_validation.py Source/GameXXK/Public/Dialogue Source/GameXXK/Private/Dialogue Source/GameXXK/Private/Tests/GameXXKDialogueRulesTest.cpp
git commit -m "feat: import validated dialogue assets"
```
