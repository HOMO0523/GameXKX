# Companion Role Display Name Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` inline. The user explicitly prohibited subagents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace random permanent-companion names and inconsistent role synonyms with six canonical profession labels while preserving stable companion identity and saves.

**Architecture:** `FGameXXKCompanionRules::GetCompanionDisplayName` becomes the single role-name authority. UI surfaces call that authority and add level/star only where several same-role roster entries must be distinguished. `NameSeed` remains serialized but has no display behavior.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, existing UMG widgets and save structs.

---

## Execution preflight

Before the first C++ edit or cold build, save and close only this project's interactive editor:

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); from ue_tdd_pipeline import save_running_editor_before_close,kill_editor; ok=save_running_editor_before_close(); ok=ok and kill_editor(); raise SystemExit(0 if ok else 1)"
```

### Task 1: Make profession labels the only companion display names

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`

- [ ] **Step 1: Replace random-name expectations with exact RED assertions**

```cpp
const TMap<EGameXXKCharacterRole, FString> Expected = {
    {EGameXXKCharacterRole::Blade, TEXT("刀客")},
    {EGameXXKCharacterRole::Guard, TEXT("守卫")},
    {EGameXXKCharacterRole::Healer, TEXT("药师")},
    {EGameXXKCharacterRole::Hunter, TEXT("弓手")},
    {EGameXXKCharacterRole::Sorcerer, TEXT("法师")},
    {EGameXXKCharacterRole::FormationMaster, TEXT("阵师")},
};
for (const TPair<EGameXXKCharacterRole, FString>& Pair : Expected)
{
    TestEqual(TEXT("seed one uses profession label"),
        FGameXXKCompanionRules::GetCompanionDisplayName(Pair.Key, 1), Pair.Value);
    TestEqual(TEXT("seed two uses the same profession label"),
        FGameXXKCompanionRules::GetCompanionDisplayName(Pair.Key, 999999), Pair.Value);
}
TestTrue(TEXT("invalid role has no companion label"),
    FGameXXKCompanionRules::GetCompanionDisplayName(EGameXXKCharacterRole::Invalid, 1).IsEmpty());
```

- [ ] **Step 2: Run and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.CompanionRules
```

Expected: FAIL because seeds still select random surname/given-name pairs.

- [ ] **Step 3: Replace the name pool with one switch**

```cpp
FString FGameXXKCompanionRules::GetCompanionDisplayName(
    const EGameXXKCharacterRole Role,
    const int32 NameSeed)
{
    static_cast<void>(NameSeed); // Serialized compatibility tombstone.
    switch (Role)
    {
    case EGameXXKCharacterRole::Blade: return TEXT("刀客");
    case EGameXXKCharacterRole::Guard: return TEXT("守卫");
    case EGameXXKCharacterRole::Healer: return TEXT("药师");
    case EGameXXKCharacterRole::Hunter: return TEXT("弓手");
    case EGameXXKCharacterRole::Sorcerer: return TEXT("法师");
    case EGameXXKCharacterRole::FormationMaster: return TEXT("阵师");
    default: return FString();
    }
}
```

Delete the surname/given-name arrays and hash mixing.

- [ ] **Step 4: Run the companion rules test and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.CompanionRules
git add Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp
git commit -m "fix: use profession names for companions"
```

### Task 2: Remove UI synonyms and duplicate role text

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterPlayerFlowTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`

- [ ] **Step 1: Add RED UI text assertions**

Require the six canonical labels, `获得伙伴：刀客`, no parenthesized duplicate, and a roster tooltip containing `刀客 · Lv.` and `★`.

- [ ] **Step 2: Run the three UI filters and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.MVP.UI.CompanionRosterPlayerFlow+GameXXK.MetaShop.Widget+GameXXK.MVP.RouteMerchant.Widget.MixedPartyOwnerLabelsAreFriendly"
```

Expected: old synonyms and `随机名（职业）` remain.

- [ ] **Step 3: Delegate all role text to the rules authority**

Replace local valid-role switches with:

```cpp
FText GetRoleDisplayName(const EGameXXKCharacterRole Role)
{
    const FString Name = FGameXXKCompanionRules::GetCompanionDisplayName(Role, 0);
    return Name.IsEmpty() ? FText::FromString(TEXT("未知职业")) : FText::FromString(Name);
}
```

Use the same helper in the meta shop. Change purchase result formatting to:

```cpp
return FText::FromString(FString::Printf(
    TEXT("获得伙伴：%s"),
    *FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed)));
```

Use that same single label in `BuildResultTooltip`; remove the existing `职业名（职业名）` formatter.

Format roster slot tooltips as:

```cpp
FString::Printf(TEXT("%s · Lv.%d · ★%d"),
    *FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed),
    Companion.Level,
    Companion.Star)
```

- [ ] **Step 4: Run UI tests and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.MVP.UI.CompanionRosterPlayerFlow+GameXXK.MetaShop.Widget+GameXXK.MVP.RouteMerchant.Widget.MixedPartyOwnerLabelsAreFriendly"
git add Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRosterPlayerFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp
git commit -m "fix: unify companion profession labels"
```

### Task 3: Verify save compatibility and document the change

**Files:**
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`
- Modify: `docs/design/2026-08-11-gamexxk-project-plan/03-rosters-deckbuilding-and-archetypes.md`
- Modify: `docs/design/2026-08-11-gamexxk-project-plan/10-implementation-testing-and-change-log.md`
- Create: `docs/production/2026-09-05-companion-role-display-name-acceptance.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Add a save round-trip assertion**

In the existing companion save fixture, preserve `NameSeed`, load it, and assert the display is the profession label:

```cpp
TestEqual(TEXT("NameSeed remains serialized"), Reloaded.NameSeed, Original.NameSeed);
TestEqual(TEXT("loaded display ignores NameSeed"),
    FGameXXKCompanionRules::GetCompanionDisplayName(Reloaded.Role, Reloaded.NameSeed),
    FString(TEXT("刀客")));
```

- [ ] **Step 2: Run companion/save regressions**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Data.CompanionRules+GameXXK.MVP.UI.CompanionRosterPlayerFlow+GameXXK.Equipment.SaveMigration"
```

Expected: PASS without a save-version change.

- [ ] **Step 3: Update current documentation**

Record the six canonical labels, `NameSeed` compatibility-only status, and the removal of the 72-name display pool. Mark random companion names superseded; do not delete historical save fields.

- [ ] **Step 4: Run diff check and commit**

```powershell
git diff --check
git add Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp docs/design/2026-08-11-gamexxk-project-plan/03-rosters-deckbuilding-and-archetypes.md docs/design/2026-08-11-gamexxk-project-plan/10-implementation-testing-and-change-log.md docs/production/2026-09-05-companion-role-display-name-acceptance.md docs/production/current-goal-acceptance.md
git commit -m "docs: accept companion profession names"
```
