# Dialogue Presenters and NPC Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Present dialogue through a bottom paper panel or actor-following bubble, coordinate input/auto/skip/history/pause, and replace NPC `F` routing with deterministic circular-range targeting.

**Architecture:** `UGameXXKDialogueCoordinator` owns one active blocking presentation, consumes outputs from the pure DialogueRunner and returns one OutcomeId to its calling NarrativeCoordinator. Presenter Widgets expose typed view-model APIs only. Each NPC owns a query-only 300-unit circular overlap trigger and registers/unregisters itself with the hero interaction component; the hero selects one registered candidate deterministically and starts the configured NarrativeSequence. Task/shop actions are Sequence commands, never Dialogue nodes.

**Tech Stack:** UE 5.8 UMG/Slate C++, existing GameXXK paper textures, PlayerController input routing, world-to-widget projection, Automation Tests.

---

## File map

- Create `Source/GameXXK/Public/Dialogue/GameXXKDialogueCoordinator.h` — session/presenter/outcome coordinator.
- Create `Source/GameXXK/Private/Dialogue/GameXXKDialogueCoordinator.cpp` — DialogueRunner integration and modal presentation ownership.
- Modify `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h` — presenter view types.
- Create `Source/GameXXK/Public/UI/GameXXKDialoguePanelWidget.h` and private `.cpp` — formal bottom panel.
- Create `Source/GameXXK/Public/UI/GameXXKSpeechBubbleWidget.h` and private `.cpp` — actor-following bubble.
- Create `Source/GameXXK/Public/UI/GameXXKDialogueHistoryWidget.h` and private `.cpp` — read-only 100-entry history.
- Create `Source/GameXXK/Public/Interaction/GameXXKInteractableComponent.h` and private `.cpp` — stable NPC interaction metadata.
- Create `Source/GameXXK/Public/Interaction/GameXXKInteractionRules.h` and private `.cpp` — pure registered-candidate validation/sorting; collision Overlap owns range filtering.
- Modify `Source/GameXXK/Public/Interaction/GameXXKInteractionComponent.h` and private `.cpp` — overlap-candidate selection without world scans.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` and private `.cpp` — coordinator ownership and input routing.
- Modify `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h` and private `.cpp` — configure interactable/dialogue IDs.
- Modify `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h` and private `.cpp` — remove recruit/primary hardcoding after consumers migrate.
- Create `Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp`.
- Create `Source/GameXXK/Private/Tests/GameXXKDialogueCoordinatorTest.cpp`.
- Create `Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp`.

### Task 1: Build the formal paper dialogue panel

**Files:**
- Modify: `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h`
- Create: `Source/GameXXK/Public/UI/GameXXKDialoguePanelWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKDialoguePanelWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp`

- [ ] **Step 1: Write a failing layout/view-model test**

Construct the Widget, call `Present`, and assert one paper frame, one portrait, speaker/name/body, four reusable option buttons, and a continue indicator:

```cpp
FGameXXKDialoguePresentationView View;
View.NodeId = TEXT("choice");
View.SpeakerDisplayName = FText::FromString(TEXT("月白"));
View.Text = FText::FromString(TEXT("你是谁？"));
View.Options = {
    {TEXT("one"), FText::FromString(TEXT("选项一")), true, FText::GetEmpty()},
    {TEXT("two"), FText::FromString(TEXT("选项二")), false, FText::FromString(TEXT("条件不足"))}};
Widget->Present(View);
TestEqual(TEXT("two options visible"), Widget->GetVisibleOptionCountForTest(), 2);
TestEqual(TEXT("speaker rendered"), Widget->GetSpeakerTextForTest(), FText::FromString(TEXT("月白")));
```

- [ ] **Step 2: Run RED**

Run `GameXXK.Dialogue.Presenter`; expect missing class/type failures.

- [ ] **Step 3: Implement the panel**

Use one bottom-center `UBorder` with an approved paper brush, portrait `UImage`, speaker/body `UTextBlock`, continue indicator, and four vertical `UButton` slots. Expose delegates:

```cpp
USTRUCT(BlueprintType)
struct FGameXXKDialoguePresentationView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName NodeId;
    UPROPERTY(BlueprintReadOnly) FText SpeakerDisplayName;
    UPROPERTY(BlueprintReadOnly) FText Text;
    UPROPERTY(BlueprintReadOnly) FSoftObjectPath PortraitPath;
    UPROPERTY(BlueprintReadOnly) TArray<FGameXXKDialogueVisibleOption> Options;
};

DECLARE_DELEGATE(FGameXXKDialogueAdvanceRequested);
DECLARE_DELEGATE_OneParam(FGameXXKDialogueOptionRequested, FName);
void Present(const FGameXXKDialoguePresentationView& View);
void SetAdvanceRequested(FGameXXKDialogueAdvanceRequested Delegate);
void SetOptionRequested(FGameXXKDialogueOptionRequested Delegate);
```

Hidden options use `Collapsed`; disabled options remain visible, disabled, and carry the configured reason as Tooltip.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h Source/GameXXK/Public/UI/GameXXKDialoguePanelWidget.h Source/GameXXK/Private/UI/GameXXKDialoguePanelWidget.cpp Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp
git commit -m "feat: add formal dialogue panel"
```

### Task 2: Build actor-following speech bubbles

**Files:**
- Modify: `Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h`
- Create: `Source/GameXXK/Public/UI/GameXXKSpeechBubbleWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKSpeechBubbleWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp`

- [ ] **Step 1: Write failing anchor/wrap tests**

Use a transient Actor/component and expect world projection, viewport clamping and exactly one bubble per active line. Test that a missing anchor returns a presentation failure instead of placing at `(0,0)`.

- [ ] **Step 2: Implement the bubble presenter**

Expose:

```cpp
bool PresentBubble(const FGameXXKDialoguePresentationView& View, USceneComponent* Anchor);
void UpdateAnchor(APlayerController* Controller);
void ClearBubble();
```

Use approved paper/ink assets, body text only, `HitTestInvisible`, maximum two-line layout and viewport-safe clamping. Blocking bubbles accept advance through the coordinator; environment bubbles auto-expire and never claim modal input.

- [ ] **Step 3: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKSpeechBubbleWidget.h Source/GameXXK/Private/UI/GameXXKSpeechBubbleWidget.cpp Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp
git commit -m "feat: add actor speech bubbles"
```

### Task 3: Coordinate advance, auto, skip, history, pause and outcome return

**Files:**
- Create: `Source/GameXXK/Public/Dialogue/GameXXKDialogueCoordinator.h`
- Create: `Source/GameXXK/Private/Dialogue/GameXXKDialogueCoordinator.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKDialogueHistoryWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKDialogueHistoryWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDialogueCoordinatorTest.cpp`

- [ ] **Step 1: Write failing coordinator tests**

Cover:

- manual advance;
- auto delay `Clamp(visibleCharacters * 0.06, 1.2, 6.0)`;
- animation/voice duration taking the maximum;
- choices pausing auto;
- Ctrl skip working only for `SeenNodeIds`;
- history trimming to 100;
- one blocking session at a time;
- Choice/End OutcomeId returned exactly once to the NarrativeCoordinator;
- pause/exit keeping the dialogue session at a replayable node boundary.

- [ ] **Step 2: Implement the outcome callback contract**

```cpp
DECLARE_DELEGATE_TwoParams(FGameXXKDialogueFinished, FName /*DialogueId*/, FName /*OutcomeId*/);

bool StartDialogue(
    const UGameXXKDialogueAsset& Asset,
    const FGameXXKDialogueStartContext& Context,
    FGameXXKDialogueFinished OnFinished,
    FString* OutError = nullptr);
```

The coordinator rejects a second blocking session, never fires `OnFinished` while paused, and fires it once only after an End node. Choice outcomes remain available to the DialogueRunner for in-dialogue branching; the final End outcome is returned to NarrativeSequence.

- [ ] **Step 3: Implement modal behavior**

Coordinator owns presenter visibility and pause state, while the calling NarrativeCoordinator owns the single world-input lock token. `Esc` shows continue/exit. Exit hides the presenter, leaves `DialogueSession` active at the current node, releases presenter resources and asks NarrativeCoordinator to release its token; it does not cancel or complete Sequence commands.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Dialogue/GameXXKDialogueTypes.h Source/GameXXK/Public/Dialogue/GameXXKDialogueCoordinator.h Source/GameXXK/Private/Dialogue/GameXXKDialogueCoordinator.cpp Source/GameXXK/Public/UI/GameXXKDialogueHistoryWidget.h Source/GameXXK/Private/UI/GameXXKDialogueHistoryWidget.cpp Source/GameXXK/Private/Tests/GameXXKDialogueCoordinatorTest.cpp
git commit -m "feat: coordinate dialogue presentation and input"
```

### Task 4: Implement deterministic circular interaction

**Files:**
- Create: `Source/GameXXK/Public/Interaction/GameXXKInteractableComponent.h`
- Create: `Source/GameXXK/Private/Interaction/GameXXKInteractableComponent.cpp`
- Create: `Source/GameXXK/Public/Interaction/GameXXKInteractionRules.h`
- Create: `Source/GameXXK/Private/Interaction/GameXXKInteractionRules.cpp`
- Modify: `Source/GameXXK/Public/Interaction/GameXXKInteractionComponent.h`
- Modify: `Source/GameXXK/Private/Interaction/GameXXKInteractionComponent.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp`

- [ ] **Step 1: Write failing pure selection tests**

Create candidates at radius boundaries and assert sorting:

```cpp
FGameXXKInteractionCandidate A{TEXT("Npc.A"), 1, 200.0f};
FGameXXKInteractionCandidate B{TEXT("Npc.B"), 2, 100.0f};
TestEqual(TEXT("priority wins"), FGameXXKInteractionRules::Choose({A, B}).InteractionId, FName(TEXT("Npc.B")));
```

Test that overlap-registered candidates are not range-filtered again, then test priority descending, distance ascending and ID ascending. Facing direction must not affect selection.

- [ ] **Step 2: Implement candidate registration and query**

Define the rules input independently from Actor/components:

```cpp
struct FGameXXKInteractionCandidate
{
    FName InteractionId;
    int32 Priority = 0;
    float Distance = 0.0f;
};

class FGameXXKInteractionRules final
{
public:
    static TOptional<FGameXXKInteractionCandidate> Choose(
        const TArray<FGameXXKInteractionCandidate>& Candidates);
};
```

`UGameXXKInteractableComponent` stores `InteractionId`, display name, `NarrativeSequenceId`, priority, enabled state and prompt anchor. Each NPC uses its existing `InteractionArea` as a query-only Pawn-overlap sphere with radius 300. Begin/end overlap registers or unregisters the NPC with the hero component. The overlap set is authoritative, so the hero does not apply a second center-distance filter. It applies no facing-direction filter, performs no world scan on `F`, and emits only a target-changed delegate. It performs no UI opening during target selection. A talk-only NPC still uses a one-Dialogue Sequence so all F interactions share the same resume/command boundary.

- [ ] **Step 3: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/Interaction/GameXXKInteractableComponent.h Source/GameXXK/Private/Interaction/GameXXKInteractableComponent.cpp Source/GameXXK/Public/Interaction/GameXXKInteractionRules.h Source/GameXXK/Private/Interaction/GameXXKInteractionRules.cpp Source/GameXXK/Public/Interaction/GameXXKInteractionComponent.h Source/GameXXK/Private/Interaction/GameXXKInteractionComponent.cpp Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp
git commit -m "feat: select npc interactions in a circular range"
```

### Task 5: Route PlayerController and NPCs through the coordinator

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp`

- [ ] **Step 1: Add failing integration tests**

Expect `F` on a selected NPC to start its NarrativeSequenceId, no target to do nothing, and an active narrative/backpack/shop to disable targeting. Assert a Sequence may branch from a Dialogue Outcome to talk/task/shop commands, and no Widget or PlayerController function exposes `RecruitPendingTownNpc` through the new interaction path.

- [ ] **Step 2: Integrate coordinator ownership**

PlayerController owns the NarrativeCoordinator and its DialogueCoordinator/presenters, forwards mouse/Space/Enter/1–4/Ctrl/Esc, and lets NarrativeCoordinator own one tokenized input lock. `TownNpcCharacter` configures its interactable SequenceId instead of opening `QuestDialog` directly.

- [ ] **Step 3: Retire QuestDialog hardcoding**

After every call site uses DialogueCoordinator, remove the recruit/primary-action dual-button mode and `ConfigureTownNpcInteraction`. Keep approved textures available to the new formal presenter; do not delete asset packages.

- [ ] **Step 4: Run integration gate and commit**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' '-ExecCmds=Automation RunTests GameXXK.Dialogue; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE5 demo/GameXXK/Saved/Automation/DialogueInteractionFinal'
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' '-ExecCmds=Automation RunTests GameXXK.MVP.Town; Quit' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UE5 demo/GameXXK/Saved/Automation/TownInteractionFinal'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXK Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=3
```

Expected: builds green, all Dialogue/Town tests pass, no recruit option remains.

```powershell
git add -- Source/GameXXK/Public/Dialogue Source/GameXXK/Private/Dialogue Source/GameXXK/Public/UI/GameXXKDialoguePanelWidget.h Source/GameXXK/Private/UI/GameXXKDialoguePanelWidget.cpp Source/GameXXK/Public/UI/GameXXKSpeechBubbleWidget.h Source/GameXXK/Private/UI/GameXXKSpeechBubbleWidget.cpp Source/GameXXK/Public/Interaction Source/GameXXK/Private/Interaction Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp Source/GameXXK/Private/Tests/GameXXKDialoguePresenterTest.cpp Source/GameXXK/Private/Tests/GameXXKDialogueCoordinatorTest.cpp Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp
git commit -m "feat: route npc interaction through dialogue"
```
