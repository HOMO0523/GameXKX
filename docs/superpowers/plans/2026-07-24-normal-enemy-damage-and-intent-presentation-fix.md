# Normal Enemy Damage and Intent Presentation Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the accidental 850%/540% normal-battle attack scales with the approved 250%, show non-damage enemy intents from their saved effects instead of as “damage 0,” and map appended combat statuses 20–26 explicitly.

**Architecture:** `FGameXXKEncounterRules` remains the single runtime owner for encounter stat scales. A new pure `FGameXXKEnemyText` formatter owns enemy-intent card and tooltip wording from persisted `FGameXXKResolvedEnemyIntentEffect` data, while the battle board only binds the resulting strings. `FGameXXKBattleStatusIconStyle` remains the single status badge style projection and gains explicit entries for all appended enum values.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, UMG, project UE MCP scripts, UBT cold builds.

**Workspace note:** Project `AGENTS.md` requires work directly on root `main` and forbids worktrees. Relevant files already contain uncommitted goal work, so this plan does not create commits or stage files; every edit is reviewed with targeted diffs.

---

### Task 1: Lock the approved normal-battle attack scale

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKEncounterFormationTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEncounterRules.cpp`
- Modify after measured simulation: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`
- Modify after measured simulation: `docs/verification/2026-07-24-route-balance-certification.md`

- [ ] **Step 1: Write the failing scale test**

Change the normal-node column in the locked expected attack table to 250 for all chapters while retaining existing elite and boss values:

```cpp
const int32 ExpectedAttack[4][3] = {
    {},
    {250, 270, 120},
    {250, 170, 100},
    {250, 180, 90}};
```

- [ ] **Step 2: Cold-build the test-only change and verify RED**

First save dirty packages through UE MCP, stop PIE, and run:

```powershell
$env:TEMP='D:\GameXXKBuildTemp'; $env:TMP='D:\GameXXKBuildTemp'
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 36
```

Then run `Automation RunTests GameXXK.Route.EncounterFormation.AuthoredStatScales` through UE MCP. Expected: FAIL because runtime still returns 850/540 for normal nodes.

- [ ] **Step 3: Implement the approved runtime scale**

In `FGameXXKEncounterRules::GetAuthoredStatScale`, change only the normal battle attack scale:

```cpp
case EGameXXKNodeKind::Battle:
    Scale.MaxHPPercent = 140;
    Scale.AttackPercent = 250;
    Scale.DefensePercent = 100;
    break;
```

- [ ] **Step 4: Cold-build and verify GREEN**

Run the same cold pipeline, then rerun `GameXXK.Route.EncounterFormation.AuthoredStatScales` and `GameXXK.MVP.Battle.EncounterRules`. Expected: both PASS, and a chapter-one rooster projects attack 20 rather than 68.

- [ ] **Step 5: Measure and re-certify the full matrix**

Update the final-candidate profile’s normal attack values to 250, run `GameXXK.RouteBalance.FinalCandidateTargets` and `GameXXK.RouteBalance.FullMatrixExecution`, record all nine actual win rates, then replace stale normal-node target bands with explicit measured/approved UX-aware bands that contain those deterministic results. Update the certification document with the 250% table, exact results, command evidence, and note that the old 850% calibration was rejected after real PIE showed a 61+68 first-turn same-target kill.

### Task 2: Format enemy intents from persisted effects

**Files:**
- Create: `Source/GameXXK/Public/GameXXKEnemyText.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

- [ ] **Step 1: Write failing formatter tests**

Construct three saved intents directly from real PIE evidence:

```cpp
FGameXXKCardEnemyIntent StinkFog;
StinkFog.CardDisplayName = TEXT("臭雾");
StinkFog.TargetRule = EGameXXKEnemyIntentTargetRule::AllLivingParty;
StinkFog.Effects.Add(FGameXXKResolvedEnemyIntentEffect{
    EGameXXKEnemyIntentEffectType::ApplyStatus,
    {TEXT("Player"), TEXT("Npc.TusiChief")},
    0, 0, EGameXXKCardStatus::None, 0, 0, false, 1,
    EGameXXKCardStatus::Weak, 1});
```

Use field assignment if aggregate construction does not match USTRUCT initialization. Assert:

```cpp
TestTrue(TEXT("stink fog names its all-party weak effect"),
    Card.Contains(TEXT("我方全体")) && Card.Contains(TEXT("虚弱 1层")));
TestFalse(TEXT("status-only intents never claim zero damage"), Card.Contains(TEXT("伤害 0")));
TestTrue(TEXT("escape lists speed and armor"),
    EscapeCard.Contains(TEXT("速度 +1")) && EscapeCard.Contains(TEXT("护甲 +5")));
TestTrue(TEXT("multi-hit damage shows per-hit magnitude and count"),
    DoublePeckCard.Contains(TEXT("37 × 2")));
```

- [ ] **Step 2: Verify RED**

Cold-build and run `GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation`. Expected: FAIL because the shared formatter does not exist and current UMG always emits `伤害 0`/`基础伤害 0`.

- [ ] **Step 3: Implement the pure shared formatter**

Create:

```cpp
struct GAMEXXK_API FGameXXKEnemyText
{
    static FString DescribeStatus(EGameXXKCardStatus Status);
    static FString FormatIntentCard(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
    static FString FormatIntentTooltip(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
};
```

Format every currently supported saved effect explicitly: direct damage (including hit count), armor, heal, status, shared-Qi loss, attack/defense/speed modifier, positive-status removal, next-card energy increase, counter, and charge. Use `TargetRule` for `我方全体`/`敌方全体`/`自身`; use saved slot labels for single targets. Only use legacy `Damage` and `OnHitStatuses` when `Effects` is empty for old-save compatibility.

- [ ] **Step 4: Bind the battle board to the formatter**

Replace local `BuildEnemyIntentCardBody`, `BuildEnemyIntentTooltip`, and status-name duplication with calls to `FGameXXKEnemyText`. Card bodies and hover tooltips must consume the same saved effects and never infer resolution by parsing localized text.

- [ ] **Step 5: Verify GREEN**

Cold-build and run:

```text
GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation
GameXXK.Integration.CardBattle.BoardEnemyIntentRefreshResume
GameXXK.Battle.EnemyIntentRules
```

Expected: PASS; non-damage intents show their real effects and direct multi-hit intent behavior remains unchanged.

### Task 3: Map appended statuses 20–26 explicitly

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEnemyText.cpp`

- [ ] **Step 1: Write failing status projection tests**

Append `Medicine`, `Weak`, `Wealth`, `Rage`, `Prey`, `Charge`, and `Counter` to `RequiredStatusIcons`. Assert each has a non-`UnknownStatus` ID, localized display name, nonempty mechanics tooltip/timing, deterministic priority, fallback glyph, and paper-ink fallback. Assert `Weak` is exactly displayed as `虚弱`.

- [ ] **Step 2: Verify RED**

Cold-build and run `GameXXK.MVP.Battle.SceneActors`. Expected: FAIL because enum value 21 currently resolves to `UnknownStatus`.

- [ ] **Step 3: Implement explicit styles and shared names**

Add explicit style IDs matching the approved asset language:

```text
MedicineHerbs, WeakBrokenBlade, WealthCoin, RageFlame,
PreyTargetEye, ChargeSpiralHorn, CounterHookBlade
```

Use truthful current mechanics: Medicine/Wealth/Rage are stored resources consumed by catalog mechanics; Weak is an adverse stack used by enemy/card conditions without inventing an unimplemented percentage penalty; Prey is the tiger’s persistent target; Charge represents a delayed locked intent; Counter retaliates against the next direct attack. Add the same localized names to `FGameXXKEnemyText::DescribeStatus`.

- [ ] **Step 4: Verify GREEN**

Cold-build and run `GameXXK.MVP.Battle.SceneActors` plus `GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation`. Expected: PASS and enum 21 no longer logs the fallback warning.

### Task 4: Cold verification and real PIE acceptance

**Files:**
- Review: all files above
- Remove after evidence capture: `Content/Python/gamexxk_probe_enemy_intent_cycle.py`

- [ ] **Step 1: Run the cold verification pipeline**

Save dirty packages via UE MCP, stop PIE, and run the non-Live-Coding pipeline. Verify UBT success and no new compiler warnings.

- [ ] **Step 2: Run focused and full automation**

Run all focused groups from Tasks 1–3, then the final-candidate and full-matrix route balance tests. Preserve exact pass/fail and deterministic rate evidence.

- [ ] **Step 3: Re-enter the same chapter-one normal battle in PIE**

Use route node 2 with rooster/weasel and record rounds 1–3. Acceptance:

```text
Rooster attack: 20
Weasel attack: 23
Peck forecast: 20
Harass forecast: 18
DoublePeck forecast: 11 × 2
StinkFog: “我方全体：虚弱 1层”, no “伤害 0”
Escape: “速度 +1；护甲 +5”, no “伤害 0”
Weak badge: localized “虚弱”, no UnknownStatus warning
```

- [ ] **Step 4: Remove the temporary PIE driver and review diffs**

Delete only `Content/Python/gamexxk_probe_enemy_intent_cycle.py`, run targeted `git diff --check`, and inspect exact diffs without staging unrelated user work.
