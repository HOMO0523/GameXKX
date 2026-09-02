# Task, Formula, Terrain, and Status UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Hero/partner/NPC tasks, Healer formulas, Medicine, and terrain benefits owner-correct, save-stable, and continuously visible below the proper battle unit.

**Architecture:** Rules state remains in `FGameXXKCardBattleRuntime`; a new pure badge projector converts that state into UI models without treating task/formula/phase markers as removable combat statuses. The existing unit status widget accepts projected badges, while terrain auto-income becomes a single saved round-boundary transaction owned by a living Formation Master partner.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame state, existing card rules/catalog, UMG/Slate status widgets, UE Automation.

---

## Preconditions and file map

Complete Plans 1 and 2 first. Work on `codex/overall-in-run-optimization` and preserve unrelated dirty assets.

Create:

- `Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h`
- `Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleStateBadgeRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKFormationTerrainRoundStartTest.cpp`

Modify:

- `Source/GameXXK/Public/GameXXKCardTypes.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp`
- `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleStatusIconStyle.h`
- `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- existing task/formula/terrain/status widget tests.

---

### Task 1: Make Sorcerer task sizes data-driven and Exhaust-safe

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTaskLifecycleTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcSpellTaskRuntimeTest.cpp`

- [ ] **Step 1: Write red 4/5/3 task tests**

Assert Hero locks exactly four equipped Hero Mage cards, partner locks five selected partner Sorcerer cards, and NPC locks its three carried cards. Move one Hero Mage card to `ExhaustPile`, record it as actively played, and assert the four-card task completes once rather than deadlocking search.

```cpp
TestEqual(TEXT("Hero task pieces"), Runtime.HeroSpellTask.LockedHeroCardIds.Num(), 4);
TestEqual(TEXT("partner task pieces"), Runtime.SorcererPartnerTasks[0].LockedCardIds.Num(), 5);
TestEqual(TEXT("NPC task pieces"), Runtime.TaskNpcSpellTasks[0].LockedCardIds.Num(), 3);
TestEqual(TEXT("one completion this round"), CountTaskRewardOrigins(Result), 1);
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards.SpellTask --automation-report InRun03_Task01_RED --json
```

- [ ] **Step 3: Derive required IDs from runtime ownership**

Remove the Hero hard-coded eight-card lock. Build distinct required IDs from the owner/role-selected cards at battle start. Completion checks `Completed*Ids`, not zone membership; search may select Draw/Discard, while an already completed Exhausted ID remains complete. Add the following field to `FGameXXKHeroSpellTaskRuntime`, `FGameXXKSorcererPartnerTaskRuntime`, and `FGameXXKTaskNpcSpellTaskRuntime`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 CompletedOnPlayerRound = 0;
```

Reject a second completion while `CompletedOnPlayerRound == Runtime.RoundNumber`; clear the completed presentation state at the next player-round boundary.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards.SpellTask --automation-report InRun03_Task01_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.SorcererPartner --automation-report InRun03_Task01_Partner_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.TaskNpcCards.SpellTask --automation-report InRun03_Task01_Npc_GREEN --json
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTaskLifecycleTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcSpellTaskRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: make spell tasks owner driven"
```

### Task 2: Lock Hero and partner Healer formula ownership

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerEnemyPhaseFormulaTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`

- [ ] **Step 1: Write red cross-owner tests**

Open the Hero `DuHuoTongLu` formula and partner `Profession.Healer.LianQiaoJieDu` formula. Resolve one qualifying explosion and assert each owner updates only its own formula and Medicine. Then make only the Hero qualify and assert partner progress remains unchanged.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Healer.Formula --automation-report InRun03_Task02_RED --json
```

- [ ] **Step 3: Key every lookup by owner plus source CardId**

```cpp
FGameXXKHealerFormulaRuntime* FindFormula(
    FGameXXKCardBattleRuntime& Runtime,
    const FName OwnerUnitId,
    const FName SourceCardId)
{
    return Runtime.HealerFormulas.FindByPredicate([&](const FGameXXKHealerFormulaRuntime& Formula)
    {
        return Formula.OwnerUnitId == OwnerUnitId && Formula.SourceCardId == SourceCardId;
    });
}
```

All event resolvers pass the actual source owner. Formula-produced effects carry a formula origin and cannot recursively satisfy that same formula. Preserve per-round caps from the design.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Healer.Formula --automation-report InRun03_Task02_GREEN --json
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHealerPartnerEnemyPhaseFormulaTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp
git diff --cached --check
git commit -m "fix: isolate healer formula ownership"
```

### Task 3: Implement Formation unlocks and terrain trigger ownership

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKFormationTerrainRoundStartTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcHealerFormationRuntimeTest.cpp`

- [ ] **Step 1: Write red unlock and trigger-boundary tests**

Assert Formation card counts 12/14/16/18 at levels 1/5/10/15. Start Plain with and without a living Formation partner: the first and second player rounds apply Burn coefficient 2 exactly once only with the partner. Kill the partner and assert the next round adds none. Explicit `TriggerTerrainBenefit` still works without a partner and uses the card owner's Defense for Village/Cave Armor.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.FormationTerrain --automation-report InRun03_Task03_RED --json
```

- [ ] **Step 3: Add the saved automatic-trigger boundary**

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 LastAutomaticTerrainBenefitPlayerRound = INDEX_NONE;
```

At initial player-round setup and `BeginNextPlayerCardRound`, find one living permanent Formation Master party unit. If its round marker differs, resolve the current terrain benefit once with implicit quality 1.0 and store the round before the final enemy-intent preview refresh. No Formation unit means no automatic trigger. Explicit card triggers bypass this gate and retain their printed count.

- [ ] **Step 4: Encode no-target terrain cards and unlock order**

Set all six switch cards and Hero `YiZhenHuiXiang` to `TargetMode=None`. Keep single targets only for cards with independent attacks/debuffs. Encode level batches exactly as design section 6.3.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.FormationTerrain --automation-report InRun03_Task03_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKFormationTerrainRoundStartTest.cpp Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcHealerFormationRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: bind terrain income to formation partner"
```

### Task 4: Build pure task/formula state badges

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleStateBadgeRulesTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconStyle.h`

- [ ] **Step 1: Write red badge-order tests**

Build a Hero task with four locked IDs/two completed, a Hero formula, and Medicine 6. Assert badge order is task-main, four fixed thumbnails, formula thumbnail, Medicine; unused task thumbnails are gray and completed are lit. Build partner/NPC fixtures and assert five/three thumbnails.

```cpp
const TArray<FGameXXKBattleStatusBadgeModel> Models =
    FGameXXKBattleStateBadgeRules::BuildForUnit(Runtime, TEXT("Hero"));
TestEqual(TEXT("main task first"), Models[0].Style.IconId, FName(TEXT("Task.Hero.Main")));
TestEqual(TEXT("first card thumbnail"), Models[1].Style.IconId, FName(TEXT("Task.Card.Hero.Mage.YanXuLiaoYuan")));
TestTrue(TEXT("completed card lit"), Models[1].Style.Tint != FLinearColor::Gray);
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.StateBadges --automation-report InRun03_Task04_RED --json
```

- [ ] **Step 3: Implement the pure projector**

```cpp
class GAMEXXK_API FGameXXKBattleStateBadgeRules final
{
public:
    static TArray<FGameXXKBattleStatusBadgeModel> BuildForUnit(
        const FGameXXKCardBattleRuntime& Runtime,
        FName UnitId);
};
```

Use `FGameXXKCardCatalog::FindCardVisualDefinition(SourceCardId)` for stable miniature identity; use the existing fallback glyph if no texture is loaded. Task/formula badges are projection-only and never enter `Unit.Statuses`. Tooltip text includes owner, exact formula/reward, progress, per-round cap, and completion state.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.StateBadges --automation-report InRun03_Task04_GREEN --json
git add Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp Source/GameXXK/Private/Tests/GameXXKBattleStateBadgeRulesTest.cpp Source/GameXXK/Public/UI/GameXXKBattleStatusIconStyle.h
git diff --cached --check
git commit -m "feat: project battle task and formula badges"
```

### Task 5: Extend the existing unit status strip

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`

- [ ] **Step 1: Write red custom-badge widget tests**

Pass two task models plus Armor/status models. Assert the task models render in a dedicated state row in supplied order, ordinary Armor/status models render in their existing sorted row, tooltip hit targets remain available, and either row's overflow lists omitted badge names without intercepting battle clicks.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.StatusEffectsWidget --automation-report InRun03_Task05_RED --json
```

- [ ] **Step 3: Extend the widget API**

```cpp
void SetStatusEffects(
    int32 InArmor,
    const TArray<FGameXXKCardStatusStack>& InStatuses,
    const TArray<FGameXXKBattleStatusBadgeModel>& InStateBadges = {});
```

Replace the one-row root with a `UVerticalBox` containing `StateIconRow` above `StatusIconRow`. State badges retain supplied order and use their own overflow; `BuildBadgeModels` continues sorting only Armor/combat statuses by priority. Equality includes IconId, texture, tint, stacks, and tooltip so gray-to-lit task changes rebuild once. A five-card partner task therefore shows main+five thumbnails without consuming all ordinary combat-status slots.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.StatusEffectsWidget --automation-report InRun03_Task05_GREEN --json
git add Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp
git diff --cached --check
git commit -m "feat: render projected battle state badges"
```

### Task 6: Integrate state badges into BattleBoard and save schema v35

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTaskFormulaTerrainSaveMigrationTest.cpp`

- [ ] **Step 1: Write red Board and migration tests**

Assert Board passes `BuildForUnit` models to each unit status widget, completed icons survive serialization during their completion round, disappear next round, and v34 saves initialize `CompletedOnPlayerRound=0` plus `LastAutomaticTerrainBenefitPlayerRound=INDEX_NONE` without triggering terrain during migration.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle --automation-report InRun03_Task06_RED --json
```

- [ ] **Step 3: Wire Board refresh without owning state**

In `RefreshUnitVisuals`, obtain the authoritative battle runtime once, build badge models by stable UnitId, and call the extended `SetStatusEffects`. Do not cache or mutate task/formula state in the widget. Register no new tick-time polling beyond the existing stale-projection guard.

- [ ] **Step 4: Add v35 migration**

```cpp
static constexpr int32 TaskFormulaTerrainPresentationIntroducedSaveVersion = 35;
static constexpr int32 CurrentSaveVersion = 35;
```

Initialize only new round-marker fields. Existing task/formula arrays remain intact and reconstruct badges after load.

- [ ] **Step 5: Run focused, cold, and broad green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle --automation-report InRun03_UI_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.FormationTerrain --automation-report InRun03_Terrain_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun03_Save_GREEN --json
```

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskFormulaTerrainSaveMigrationTest.cpp
git diff --cached --check
git commit -m "feat: surface task formula and terrain state"
```

Plan 3 is complete when task/formula badges reconstruct from state, terrain auto-income is Formation-owned, and no task/formula badge can be removed as a combat status.
