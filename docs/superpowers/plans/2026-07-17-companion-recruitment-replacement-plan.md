# Companion Recruitment and Full-Roster Replacement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the town companion backpack recruit a deterministic permanent companion, retain a full-roster candidate until the player explicitly replaces or dismisses it, and expose only real progression state and sigil promotion.

**Architecture:** `FGameXXKCompanionRosterState` owns a save-compatible deterministic sequence state and the already-existing fixed pending recruit ticket/candidate. `UGameXXKMVPSubsystem` provides town- and route-lock-guarded transactions, while `UGameXXKCompanionRosterWidget` only presents those transactions through existing TownBackpack PSD frame/action assets. Existing companion rules remain the sole source of profile generation, card pool generation, replacement, and star promotion.

**Tech Stack:** Unreal Engine C++ USTRUCT SaveGame fields, UGameInstanceSubsystem facade, programmatic UMG, Unreal automation tests.

---

### Task 1: Specify save-compatible deterministic recruitment transactions

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`

- [x] **Step 1: Write failing tests for a persisted order sequence and full-roster candidate disposal**

```cpp
FGameXXKCompanionRosterState Roster;
FGameXXKCompanionRecruitResult First;
TestTrue(TEXT("first sequence claim succeeds"),
    FGameXXKCompanionRules::CreateAndResolveNextRecruitment(Roster, First, nullptr));
TestEqual(TEXT("first recruit has twelve personal cards"), First.Companion.PersonalCardIds.Num(), 12);
TestTrue(TEXT("discarding a fixed full-roster candidate clears it"),
    FGameXXKCompanionRules::DiscardPendingRecruitment(FullRoster, nullptr));
```

- [ ] **Step 2: Run a focused compile/test target to verify failure**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.Data.Companion.RecruitmentFlow`

Expected: compile failure because the two rule APIs do not exist.

- [x] **Step 3: Add the minimal persistent sequence and rule APIs**

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 RecruitSequenceSeed = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 RecruitSequenceOrdinal = 0;

static bool CreateAndResolveNextRecruitment(
    FGameXXKCompanionRosterState& InOutRoster,
    FGameXXKCompanionRecruitResult& OutResult,
    FString* OutError = nullptr);
static bool DiscardPendingRecruitment(FGameXXKCompanionRosterState& InOutRoster, FString* OutError = nullptr);
```

Use a non-zero fixed fallback seed when deserializing old saves. The first 24 sequence entries are a seed-ordered permutation of the approved templates, so new companions vary before duplicate-template behavior can occur. Advance the saved ordinal only when creating a new ticket; leave it unchanged while a full-roster candidate is pending. The discard operation clears the ticket/candidate but does not rewind or re-roll it.

- [ ] **Step 4: Run the focused test to verify it passes**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.Data.Companion.RecruitmentFlow`

Expected: PASS; the same saved roster state produces the same candidate, a candidate survives reopening, and discard does not mutate existing roster entries.

### Task 2: Expose atomic town-only facade operations

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`

- [x] **Step 1: Write failing facade tests**

```cpp
FGameXXKCompanionRecruitResult Result;
TestTrue(TEXT("town facade starts a random recruit"), Subsystem->StartRandomPermanentCompanionRecruitment(Result));
TestTrue(TEXT("full roster candidate can be dismissed without reroll"),
    Subsystem->DiscardPendingPermanentCompanionRecruitment());
```

- [ ] **Step 2: Run the focused compile/test target to verify failure**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.MVP.Companion.RecruitmentFlow`

Expected: compile failure because the facade methods do not exist.

- [x] **Step 3: Add guarded facade methods and copy-safe pending reads**

```cpp
bool StartRandomPermanentCompanionRecruitment(FGameXXKCompanionRecruitResult& OutResult);
bool ResolvePendingPermanentCompanionReplacement(FName DismissedInstanceId, FName ActiveAfterReplacement);
bool DiscardPendingPermanentCompanionRecruitment();
bool TryGetPendingPermanentCompanionRecruitment(FGameXXKPermanentCompanion& OutCandidate) const;
int32 GetPermanentCompanionSigilCount() const;
```

Every mutation must reject non-town screens and both route/card-battle locks. Replacement delegates to `FGameXXKCompanionRules::ResolvePendingRecruitment`; it must never silently select an active party member. Existing seeded `RecruitPermanentCompanionFromSeed` remains intact for data tests.

- [ ] **Step 4: Run the focused test to verify it passes**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.MVP.Companion.RecruitmentFlow`

Expected: PASS; loaded runtime copies retain the sequence/ticket, full-roster replacement is explicit, discard releases only the saved candidate, and non-town/locked calls fail.

### Task 3: Present real recruitment and progression controls in the isolated backpack

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`

- [x] **Step 1: Write failing widget interaction tests**

```cpp
TestTrue(TEXT("the PSD backpack exposes a real random recruit action"), Widget->BeginRandomRecruitment());
TestTrue(TEXT("a full-roster candidate is visible without reroll"), Widget->HasPendingRecruitmentForTest());
TestTrue(TEXT("the pending candidate can be explicitly discarded"), Widget->DiscardPendingRecruitment());
TestTrue(TEXT("the sigil button calls the canonical promotion facade"), Widget->PromoteSelectedCompanionStar());
```

- [ ] **Step 2: Run the focused compile/test target to verify failure**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.UI.CompanionRoster.Recruitment`

Expected: compile failure because the widget commands and test seams do not exist.

- [x] **Step 3: Add PSD-backed controls without creating assets or currencies**

```cpp
bool BeginRandomRecruitment();
bool ResolvePendingRecruitmentWithSelectedCompanion();
bool DiscardPendingRecruitment();
bool PromoteSelectedCompanionStar();
```

Build action buttons with `T_TownBackpack_ActionBlank` and keep the existing catalog/codex untouched. The profile panel displays current XP/next-level threshold and `SigilCount`; no XP grant is added. Enable promote only with a selected companion, an available sigil, and unlocked town state. At capacity, render the fixed candidate summary and require a selected existing slot for replacement; expose a separate discard button.

- [ ] **Step 4: Run the focused test to verify it passes**

Run: `scripts/ue_tdd_pipeline.py --tests GameXXK.UI.CompanionRoster.Recruitment`

Expected: PASS; clicks perform facade calls, no duplicate candidate is created on refresh, and promotion consumes an already-existing sigil through rules.

### Task 4: Static review and focused regression evidence

**Files:**
- Verify: changed companion types/rules/subsystem/widget/tests

- [x] **Step 1: Run static hygiene checks before any broad build**

Run: `git diff --check -- Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Public/GameXXKCompanionRules.h Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`

Expected: no whitespace errors.

- [x] **Step 2: Read changed public signatures and test names side by side**

Run: `rg -n "CreateAndResolveNextRecruitment|DiscardPendingRecruitment|StartRandomPermanentCompanionRecruitment|ResolvePendingPermanentCompanionReplacement|BeginRandomRecruitment|PromoteSelectedCompanionStar" Source/GameXXK/Public Source/GameXXK/Private`

Expected: each declared public operation has a definition and direct automation coverage.

- [ ] **Step 3: Hand off cold-build and full automation to the root integration pass**

Do not run a full UBT build in this isolated task. The root pass will perform one cold build and execute the matching `GameXXK.Data.Companion.RecruitmentFlow`, `GameXXK.MVP.Companion.RecruitmentFlow`, and `GameXXK.UI.CompanionRoster.Recruitment` suites alongside integration regressions.
