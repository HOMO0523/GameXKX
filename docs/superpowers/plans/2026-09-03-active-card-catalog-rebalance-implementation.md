# Active Card Catalog Rebalance Implementation Plan

> **Execution:** Use superpowers:executing-plans in this task. The user prohibits sub-agents. Steps use checkbox (`- [ ]`) syntax for tracking. The user's latest “继续” resumes approved implementation from Task 3; unresolved 雷走 numeric candidates remain separate from approved work. Later pause notes retain their earlier review context.

**Goal:** Replace the 198-card legacy catalog with the approved 173-card pool and encode every approved player-card value without CardId-specific runtime branching.

**Architecture:** Card effects gain a declarative magnitude policy and optional Rare/Epic values. Catalog builders declare coefficients and policies; quality resolution and `GameXXKCardRules` interpret them through Plan 1's scaling authority. Retired run-local cards are removed by a v34 migration while five Boss IDs remain valid but unavailable in single-map rewards.

**Tech Stack:** Unreal Engine 5.8 C++, existing card catalog/rules, SaveGame migration, UE Automation, generated Markdown catalog tests.

---

## Preconditions and guard

- Complete `2026-09-03-combat-scaling-foundation-implementation.md` first; v33 and `FGameXXKCombatScalingRules` must be green.
- Work on `codex/overall-in-run-optimization` in the root checkout.
- Preserve unrelated dirty assets and never stage them.
- Use the exact CardIds and values in design sections 3, 4, and 6.
- The Energy baseline is approved independently of 雷走 Attack candidates: `docs/superpowers/specs/2026-09-03-sorcerer-energy-cost-design.md`. Partner JuLing/LieFu cost 0, the other sixteen cost 1; Hero Mage costs 1/1/1/0. Preserve Mana, explicit Armor coefficients, free replays and post-completion refunds. Approved work is resumed; remaining numeric candidates stay separate.
- The latest partner Ice revision in design section 6.3 and Mana-overflow formula in section 4.3.1 supersede the earlier 40%/80%-Defense base grants. Record the user-authored card behavior before resuming Task 6; do not use the first damage projection as current balance evidence.

## File map

### New tests

- `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp` — exact 173 IDs, qualities, costs, policies, and critical values.
- `Source/GameXXK/Private/Tests/GameXXKRetiredRouteCardMigrationTest.cpp` — v33-to-v34 retirement behavior.

### Existing files

- `Source/GameXXK/Public/GameXXKCardTypes.h` — magnitude policy and explicit values.
- `Source/GameXXK/Public/GameXXKCardCatalog.h`
- `Source/GameXXK/Private/GameXXKCardCatalog.cpp` — 173 definitions.
- `Source/GameXXK/Public/GameXXKCardQualityRules.h`
- `Source/GameXXK/Private/GameXXKCardQualityRules.cpp` — policy resolution and 173 quality counts.
- `Source/GameXXK/Private/GameXXKCardRules.cpp` — runtime composite policies.
- `Source/GameXXK/Private/GameXXKCardText.cpp` — resolved-number text.
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — reward/configuration filtering.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Existing Hero, partner, NPC, quality, documentation, playability, and reward tests.

---

### Task 1: Add declarative card magnitude policies

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardQualityRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp`

- [x] **Step 1: Write red policy tests**

Create fixture effects for every policy and assert: Continuous 101 -> 122/142; Explicit 2 -> 3/4; DOT coefficient remains 6 until runtime; PrintedCostArmor remains data-only; Unscaled draw remains 2.

```cpp
FGameXXKCardEffect ExplicitDraw;
ExplicitDraw.Type = EGameXXKCardEffectType::DrawCards;
ExplicitDraw.Magnitude = 2;
ExplicitDraw.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ExplicitByQuality;
ExplicitDraw.RareMagnitude = 3;
ExplicitDraw.EpicMagnitude = 4;
TestEqual(TEXT("Rare explicit draw"),
    FGameXXKCardQualityRules::ResolveEffectMagnitude(ExplicitDraw, EGameXXKCardQuality::Rare), 3);
```

- [x] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardQuality --automation-report InRun02_Task01_RED --json
```

- [x] **Step 3: Add policy fields**

```cpp
UENUM(BlueprintType)
enum class EGameXXKCardMagnitudePolicy : uint8
{
    Unscaled = 0,
    ContinuousQuality = 1,
    ExplicitByQuality = 2,
    DotCoefficient = 3,
    PrintedCostArmor = 4,
    DefensePercent = 5,
    MedicineCoefficient = 6
};
```

Add to `FGameXXKCardEffect` and `FGameXXKCardBattleModifier`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
EGameXXKCardMagnitudePolicy MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 RareMagnitude = INDEX_NONE;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 EpicMagnitude = INDEX_NONE;
```

Use the `SaveGame` metadata on the modifier copy because `FGameXXKCardBattleModifier` is embedded in active-battle runtime. Catalog-only effect fields may use the same declaration for structural consistency. v33 pending modifiers already store effective magnitudes; v34 migration leaves those magnitudes unchanged and initializes their policy to `Unscaled` so load never quality-scales them a second time.

Implement `ResolveEffectMagnitude`. Continuous uses `ScaleContinuousCeil`; explicit requires nonnegative Rare/Epic values; DOT/Armor/Medicine policies preserve the base coefficient for runtime context. Set `EffectiveDefinition.BaseQuality` to the resolved instance quality so runtime composite policies receive it.

- [x] **Step 4: Add catalog builder helpers**

```cpp
FGameXXKCardEffect Continuous(FGameXXKCardEffect Effect)
{
    Effect.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
    return Effect;
}

FGameXXKCardEffect Explicit(FGameXXKCardEffect Effect, const int32 Rare, const int32 Epic)
{
    Effect.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ExplicitByQuality;
    Effect.RareMagnitude = Rare;
    Effect.EpicMagnitude = Epic;
    return Effect;
}

FGameXXKCardEffect Dot(FGameXXKCardEffect Effect)
{
    Effect.MagnitudePolicy = EGameXXKCardMagnitudePolicy::DotCoefficient;
    return Effect;
}

FGameXXKCardEffect WithPolicy(FGameXXKCardEffect Effect, const EGameXXKCardMagnitudePolicy Policy)
{
    Effect.MagnitudePolicy = Policy;
    return Effect;
}

FGameXXKCardEffect PrintedArmor(FGameXXKCardEffect Effect)
{
    return WithPolicy(MoveTemp(Effect), EGameXXKCardMagnitudePolicy::PrintedCostArmor);
}

FGameXXKCardEffect DefensePercent(FGameXXKCardEffect Effect)
{
    return WithPolicy(MoveTemp(Effect), EGameXXKCardMagnitudePolicy::DefensePercent);
}

FGameXXKCardEffect MedicineCoefficient(FGameXXKCardEffect Effect)
{
    return WithPolicy(MoveTemp(Effect), EGameXXKCardMagnitudePolicy::MedicineCoefficient);
}
```

Validation rejects missing explicit values, negative coefficients, DOT policy on non-DOT status, and PrintedCostArmor on non-Armor effects.

In this first policy commit, update the shared catalog `Effect(...)`/modifier builders so every still-unconverted legacy definition receives the Plan 1 equivalent explicitly: direct/fixed/heal/ally-attack/AddArmor magnitudes use `ContinuousQuality`, while draw, resources, and discrete status/count values use `Unscaled`. This preserves the green v33 behavior between commits. Later card-family tasks replace Armor, DOT, Medicine, and quality-specific counts with their exact policies; no active definition may depend on an implicit effect-type fallback by Task 9.

- [x] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardQuality --automation-report InRun02_Task01_GREEN --json
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardQualityRules.h Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp
git diff --cached --check
git commit -m "feat: add declarative card value policies"
```

### Task 2: Retire 25 route cards and migrate v33 saves

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRetiredRouteCardMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteRewardGateTest.cpp`

- [x] **Step 1: Write red catalog and migration tests**

Assert active counts 173/102 Common/42 Rare/29 Epic, no IDs with `Route.General.`, `Route.Terrain.`, or `Route.Rare.`, and exactly five `Route.Boss.*` IDs. Build a v33 save containing retired instances in every deck zone, reward, and merchant field; migration removes them and repairs `ActiveInstanceIds`. A `Route.Boss.XiongPiPiJia` instance survives.

- [x] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardCatalog --automation-report InRun02_Task02_RED --json
```

- [x] **Step 3: Remove active definitions and reward sources**

Delete construction of indices 169-193 from the active catalog. Keep 194-198. Replace validation counts:

```cpp
return ValidateCatalog(
    TEXT("Card"), Definitions,
    173, 102,
    GetRareCardIds(), 42,
    GetEpicCardIds(), 29,
    [](const FName Id) { return FGameXXKCardQualityRules::GetCardBaseQuality(Id); },
    OutError);
```

Remove the five `Route.Rare.*` IDs from Rare classification. Filter every reward and merchant candidate through `FGameXXKCardCatalog::FindCardDefinition`; single-map logic must also reject `Route.Boss.*` acquisition.

- [x] **Step 4: Implement v34 retirement migration**

```cpp
static constexpr int32 ActiveCardPool173IntroducedSaveVersion = 34;
static constexpr int32 CurrentSaveVersion = 34;

bool IsRetiredRouteCard(const FName CardId)
{
    const FString Id = CardId.ToString();
    return Id.StartsWith(TEXT("Route.General."))
        || Id.StartsWith(TEXT("Route.Terrain."))
        || Id.StartsWith(TEXT("Route.Rare."));
}
```

Remove retired IDs from configured decks, every battle zone, pending automatic cards, pending choices/rewards, and merchant state; rebuild `ActiveInstanceIds`. Because these cards were run-local, grant no permanent compensation. If an active battle loses every card, clear only that battle and return to its route map/workbench recovery surface without awarding victory.

For surviving v33 active-battle modifiers, preserve their already-resolved `Magnitude`, set new policy metadata to `Unscaled`, and leave remaining triggers/expiry/owner bindings untouched; never reapply 120/140 quality during migration.

- [x] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardCatalog --automation-report InRun02_Task02_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun02_Task02_Save_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKRetiredRouteCardMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteRewardGateTest.cpp
git diff --cached --check
git commit -m "feat: retire legacy route cards"
```

### Task 3: Encode and verify all 36 Hero cards

Completed at `7a9b869`: cold UBT and Hero/CombatScaling/SaveMigration 123/123; simulation foundation 2/2. Scope and the deferred legacy quality-text check are recorded in `docs/production/2026-09-03-hero-rebalance-acceptance.md`.

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroBladeRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroGuardRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroHunterRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroMageRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroTaskResumeMigrationTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCombatScalingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatScalingRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`

Recovery audit refinement: this task must also migrate recognizable v33/early-v34 eight-card Hero tasks to the four equipped Mage requirements before current-state validation. Validate complete old snapshots/deck choices before filtering, preserve existing replay progress and paused manual choices, never execute cards or grant rewards during migration, and keep repeat loads idempotent. A narrow saved marker may retain an already-earned legacy search when it is the manual continuation of a saved replay; it does not change new-card search rules. Cover source versions 33/34, incomplete/full tasks, every replay cursor, forced discard, search, malformed data, and real choice continuation. This compatibility correction does not renumber the planned v35-v38 schemas. See `docs/production/2026-09-03-in-run-optimization-recovery-audit.md` for the reproduced interruption and original RED evidence.

- [x] **Step 1: Add exact red catalog rows**

Create one table-driven expected row for each Hero CardId. Assert Hero Mage Energy 1/1/1/0 and Mana 3/0/3/0. HanXu at zero Energy rejects without granting Armor/Mana; at sufficient Energy it pays one and retains its explicit 40%-Defense Armor. Four active Mage cards pay three Energy, with no repeated payment on replay; GuiXu retains its next-Hero-only one-use discount. The first critical rows are:

```cpp
struct FExpectedApprovedCard
{
    const TCHAR* CardId;
    int32 EnergyCost;
    int32 ManaCost;
    int32 PrimaryMagnitude;
};

const TArray<FExpectedApprovedCard> HeroExpected = {
    {TEXT("Hero.Generic.QingFengYiShi"), 1, 0, 100},
    {TEXT("Hero.Generic.FengShenBu"), 0, 0, 2},
    {TEXT("Hero.Guard.TieBiTongShou"), 1, 0, 80},
    {TEXT("Hero.Guard.XuanJiaZhenYue"), 2, 6, 200},
    {TEXT("Hero.Healer.HuiChunNiMai"), 1, 3, 25},
    {TEXT("Hero.Hunter.LieYuLianShi"), 1, 3, 140},
    {TEXT("Hero.Mage.YanXuLiaoYuan"), 1, 3, 100},
    {TEXT("Hero.Formation.GuanShiLuoZi"), 1, 3, 80}
};
```

Fill the remaining exact rows from design section 6.1-6.2; assert all 36 IDs appear exactly once and each effect carries the correct magnitude policy.

- [x] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards --automation-report InRun02_Task03_RED --json
```

- [x] **Step 3: Update catalog and composite resolution**

Use `Continuous`, `Explicit`, `Dot`, `PrintedArmor`, `DefensePercent`, and `MedicineCoefficient` builders. Implement no new `if (CardId == ...)` branch in `GameXXKCardRules.cpp`; extend generic effect/rule structs when a behavior is missing. In particular:

```cpp
AddHero(TEXT("Hero.Generic.FengShenBu"), TEXT("风身步"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
    {Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility),
     Explicit(Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), 3, 4),
     Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1)},
    EGameXXKCharacterRole::Invalid, 1, true);
```

Encode Hero Guard Armor with policies rather than flat points; encode DOT coefficients before level expansion; encode Hero Sorcerer task size 4 and Hero Healer four formula sources exactly as specified.

- [x] **Step 4: Run Hero green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards --automation-report InRun02_Task03_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: rebalance hero card catalog"
```

### Task 4: Rebalance partner Blade and Guard cards

Completed at `cda0190`: cold UBT and 57/57 scoped Blade, Guard, approved rebalance and equipment-integration checks, zero failures/warnings. Evidence: `Saved/Automation/InRun02_Task04_BladeGuardContracts_GREEN/index.json` and `Saved/HarnessReports/20260903-182736-ai-production-loop.md`. The existing serialized YinXue enum name is retained, while its healing budget now uses coefficient20 with quality/level scaling; prior smaller saved remaining budgets remain valid.

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCounterflowRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPartnerBladeGuardRebalanceTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerBloodEdgeRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCoreRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerMomentumRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuardPartnerRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGuardPartnerCatalogTest.cpp`

- [x] **Step 1: Add red assertions for CardIds 061-096**

Assert Blood Edge uses 2 points per resolved DOT, `ZhanJin` base 300, `HengYunKaiFeng` base 100, `BaoDaoShouYe` Agility 2, and all Guard primary Armor effects use PrintedCostArmor. Assert `ZhenYueLing` base 180 plus one per Armor and `BiLeiFanGong` base 220 plus one per Armor.

- [x] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards.Blade --automation-report InRun02_Task04_RED --json
```

- [x] **Step 3: Encode the 36 approved definitions**

Use exact values in design section 6.3. Preserve card ownership, target modes, and existing Charge/Finish behaviors unless overridden. For Armor conversion, store quality-scaled base in `Magnitude` and unscaled `+1 per Armor` in `SecondaryMagnitude=1`.

```cpp
Effect.Type = EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor;
Effect.Magnitude = 180;
Effect.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
Effect.SecondaryMagnitude = 1;
```

- [x] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards --automation-report InRun02_Task04_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKBladePartnerCounterflowRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp
git diff --cached --check
git commit -m "feat: rebalance blade and guard partner cards"
```

### Task 5: Rebalance partner Healer and Hunter cards

Completed at `c8ce175`: cold UBT and 147/147 partner/Hero Healer-Hunter/scaling/catalog/save contracts, zero warnings. Evidence and remaining display work: `docs/production/2026-09-03-healer-hunter-rebalance-acceptance.md`.

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPartnerHealerHunterRebalanceTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCombatScalingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatScalingRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerCoreRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerEnemyPhaseFormulaTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHunterPartnerHeavyArrowRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerIceLightningRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHunterPartnerCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHunterPartnerCoreRuntimeTest.cpp`

- [x] **Step 1: Add red rows for CardIds 097-132**

Assert raw coefficients rather than level-100 displays: `FuGuSan` Attack 60, Bleed 6, Poison 4; `YaoNangFeiTou` Attack 45, Bleed 3, Poison 1; `LianZhuJian` Bleed 8, Poison 6, Attack/Heavy 50; `DuanMaiShi` Bleed 8 and +30 per Charge; `PoJiaDing` Poison 1 and +25 per Charge.

- [x] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards.Healer --automation-report InRun02_Task05_RED --json
```

- [x] **Step 3: Encode all 36 definitions and formula rules**

Use design section 6.3 exactly. DOT fields use `DotCoefficient`, healing/reversal fields use `MedicineCoefficient`, Heavy-Arrow damage uses `ContinuousQuality`, and Charge/status counts remain explicit integers. Keep one owner-scoped formula per source CardId and forbid recursive formula satisfaction.

Latest user correction supersedes the earlier125/155 reference-quality interpretation: every healing coefficient is raw, and every healing/reversal uses `(coefficient + Medicine) * quality * (TeamMaxLevel/25 + 1)`, with one final ceiling. Rare HuiChunLu coefficient25 at level100 is150 with Medicine0 and180 with Medicine5; Epic gives175/210. Legacy reference-quality tags must not cancel scaling, and new catalog coefficients use the raw convention. Check single/group healing, reversal, Medicine-free supplemental healing, level boundaries and integer saturation. Freeze the original action's results before qualifying any opened formula, so one formula cannot satisfy another. QingXin counts cleared DOT types, including Rot, rather than the number of removed reservoir points.

- [x] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards --automation-report InRun02_Task05_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHunterPartnerCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHunterPartnerCoreRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: rebalance healer and hunter partner cards"
```

### Task 6: Rebalance partner Sorcerer and Formation Master cards

Sorcerer confirmed slice completed at 0eb0fe7:17 rules and all18 fees;164 full-scope tests plus1 resource/refund boundary test passed after cold UBT. LightningStorm numeric package remains unconfirmed and its old attack shape is not counted complete. Formation Master remains pending. Evidence: docs/production/2026-09-04-confirmed-sorcerer-rebalance-acceptance.md.
Formation Master completed at 64e6fcf plus 92bf1c1:18 definitions, target-free switches, quality recovery,12/14/16/18 unlocks and legacy-pool migration are green. Shared terrain-value scaling remains Plan3. Evidence: docs/production/2026-09-04-formation-card-rebalance-acceptance.md.

Resource prerequisite completed at 2e0db81: Hero30/Sorcerer34 fixed bases, equipment Mana exclusion for all characters, preserved explicit route/battle capacity and current-Mana save state, retired Mana affix generation, consistent previews, and legacy empty-socket normalization. Cold UBT and 316/316 equipment/card/save/simulation regressions passed. Acceptance: docs/production/2026-09-03-fixed-mana-equipment-acceptance.md. The card steps below remain incomplete.

User confirmation for the Mana prerequisite: use Hero30 and Sorcerer34 at every character level, with no ordinary star/equipment growth. Preserve explicit battle MaxMana +4/+8 and existing fixed route bonuses; current-Mana recovery8/16 or6 does not increase MaxMana. Add focused stat/equipment/battle-entry and saved-battle preservation checks before claiming the Ice projection fixtures match runtime.

Further user clarification: no equipment may increase any character's Mana. Remove base/level/enhancement Mana from effective item stats, disable legacy flat and percentage Mana contributions, retire MaxMana from new and reforge affix pools, and align item tooltips. Preserve owned-item IDs and recorded legacy rolls for load compatibility; do not silently assign replacement bonuses. Validate high-quality roll requests against the available distinct active affixes before indexing a candidate pool.

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerRewardRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerIceLightningRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRewardMatrixTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerSaveResumeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`

- [ ] **Step 1: Add red CardId 133-168 and unlock assertions**

Assert task size 5, Fire conversion 2 points per Burn, Ice base 100 plus one per Armor, `ZhenShaZhen` base 320/Epic 448, and Formation unlock counts 12/14/16/18 at levels 1/5/10/15 with all six switch cards present at level 1.

Add exact approved Energy rows for all 18 partner Mage cards: only JuLing/LieFu are 0, all others 1 at every legal quality; Mana stays unchanged. With enough Mana/cards and no external gain/discount, start at Energy 3 and play 照见→引雷→周天→雷走→连霆: active Energy becomes 3/2/2/1/0, free replay keeps 0, and the reward raises it to 1. For 引雷→索敌→周天→雷走→连霆, starting at 3 rejects final 连霆 at zero Energy and 4/5 records, with no final effects/reward; starting at 4 completes and ends at 1. For 斗转→寒息→六合→冰鉴→霜镜, starting at 4 rejects final 霜镜; starting at 5 completes and ends at 0, including the free extra replay. Failure preserves the rejected card transaction, while earlier successful records remain. Keep the existing 24/72 per-ally 六合 grants with at least one available Energy; increased printed fees must not inflate explicit Armor. Preserve partial tasks across round boundaries and save/resume; no advance credit from future refunds.

**雷走 direction amendment:** `docs/superpowers/specs/2026-09-03-lightning-single-hit-design.md` records the user-approved one-hit identity and normal use of at most one Mark per enemy. Keep Common / 1 Energy / 4 Mana and a valid zero-Mark base hit; retire the old all-Mark volley. The 120/180/220 base candidates, positions 3-4 window, and Mark-3-then-one-240 starter reward remain under numeric review. Do not turn those provisional coefficients into a claimed approved catalog gate. 引雷's proposed Vulnerability reward is excluded; its current reward stays unchanged. The user has resumed approved runtime work; only the unresolved 雷走 numeric package remains pending confirmation.

Once the candidate numeric package is confirmed, cover the following through actual preview/commit and Universal replay tests, using the final approved coefficients if they change:

- One effect against enemies with Marks 0/1/4 produces one hit per living enemy and leaves Marks 0/0/3 absent phase changes; it never fans out into 1/4 hits. At candidate values, positions 3-4 choose 120/220/220 respectively, while position 5 chooses 120/180/180. Multiply selected Attack coefficients by quality once, then use ordinary hit/Mark resolution.
- Common/Rare/Epic and recorded positions 1-5 preserve the same hit count. A marked fourth-position Common candidate at Attack 495 versus level-135 Defense 146 produces one potential hit of 705; a following fifth-position 连霆 with three remaining Marks produces three hits of 150. Higher starting Mark count does not add 雷走 hits.
- An active play pays its actual Energy and Mana costs; free task/斗转 replay pays neither and retains the card's own original quality and recorded position, while reading current target Marks. Repeated already-recorded active cards do not reacquire sequence bonuses. 雷走 remains a direct-damage predecessor for 六合.
- Only 雷走 starter reward adds its reward Marks and executes its single reward hit. With the current candidate, initial Marks 0/3 become 2/4 after one 240%-base hit (the confirmed five-Mark cap applies before attacking); old three 60% hits are not a valid replacement. A paused/resumed reward cannot seed or hit twice.
- A 斗转 reward from zero Marks, with a fifth-position Common 雷走 as last Lightning record, adds two Marks, replays one 180%-base hit, then resolves one remaining 56%-Attack Epic 斗转 lightning hit. Candidate tiger damage is 557+98=655. Do not use the 220% fourth-position boost, grant 雷走's own starter reward, or promote the replayed Common card to Epic.
- A hit that transitions an enemy clears that enemy's Marks before the following card; remaining old-phase Marks cannot feed 连霆 or 斗转. Mixed living/dead targets, Armor absorption, normal Mark consumption, and byte-stable save/resume follow the existing shared pipeline.

Add actual card-preview/commit fixtures for the revised Ice cards, resetting Mana to 34/34, Armor to 0, and TeamMaxLevel to 100 for each independent row:

| Card / quality | Mana recovery | Ending Mana / MaxMana | Generated Armor |
|---|---:|---|---:|
| 寒息回流 / Rare | 4 | 34 / 34 | 24 |
| 玄冰拓脉 / Common | 4, after MaxMana +4 | 38 / 38 | 0 |
| 霜镜叠甲 / Rare, zero initial Armor | 4 | 34 / 34 | 24 |
| 冰鉴索法 / Rare, legal search | 4 | 34 / 34 | 24 |
| 冰鉴索法 / Rare, unavailable search | 4 | 34 / 34 | 48 |

Also assert these boundaries through the real resolver:

- Current Mana 31/34: Rare 冰鉴 recovers 4, generates 6 Armor, and a failed search adds only 6 more; it does not recover Mana twice. Current Mana 30/34 recovers 3 to 33/34 and generates zero Armor even when search fails. Zero overflow must not fail an otherwise legal play or prevent task progress.
- Starting Armor 100: 霜镜 ends at Armor 200 and leaves Mana unchanged. Starting Mana 0 and Armor 0: percentage recovery and overflow grant zero; 玄冰 still increases MaxMana by 4.
- Vary Defense across 5/257/1000 while holding Mana, quality, and TeamMaxLevel fixed. The four base effects stay identical. For 霜镜's revised starter reward, hold consumed Armor fixed at 1003 and assert every unique living ally, including the caster, receives 250; vary Defense, legal starter quality, and team level without rescaling that resolved Armor refund. Repeat with zero consumed Armor, one defeated ally, and a phase-limited damage target. The grant uses consumed Armor, not actual HP damage or enemy count, and is not divided among recipients.
- Change only quality: recovered Mana remains 4 at 34/34 while generated Armor receives exactly one legal quality multiplier. TeamMaxLevel uses the shared battle snapshot and its real-valued level/25 factor; no DOT cap is imposed on Armor.
- At current Mana 41, 10% recovery rounds up to 5; replay must recompute it instead of reusing the 4 recovered at first-play Mana 34. Preserve sequence position and first-play quality separately from current Mana/Armor. For 玄冰 at Mana 42/42, increase the cap to 46 before recovery 5: one overflow produces Armor 5/6/7 by quality. Initial zero-Armor outcomes at 34/34 must not disable future overflow.
- Whole task 照见→寒息→玄冰→冰鉴→霜镜 at confirmed 10% recovery, minimum legal qualities, and its first automatic-hand budget: Armor 144 after active cards, 456 before the Ice reward, 114 refunded, ending Mana 42/42. For 斗转→寒息→玄冰→冰鉴→霜镜, including the starter's 2 Mana payment and extra mirror replay, pre-reward Armor is 816. At Attack 495 against level-135 Defense 146, the two potential Ice packets are 1694 and 2981 before phase protection.
- Using the same five cards in order 霜镜→寒息→玄冰→冰鉴→照见, with unavailable searches and the first non-starter Universal automatic-hand budget, assert Armor 121 after active cards, 351 before the reward, and 87 granted to every living ally afterward. Potential Ice damage is 1421 at the same target fixture. No mirror group reward may execute in the 照见/斗转 starter controls.
- Pause a task replay at an existing search choice, serialize/restore, and finish it. Already-applied recovery, MaxMana growth, Armor copies, and the final reward must not run twice. No new active task is advanced by automatic replay.

Add the latest 六合 base/sequence and Ice-reward cases to the actual Universal runtime/reward tests:

- At Rare / TeamMaxLevel 100 / Mana 34/34, an active 1-Energy, 4-Mana ordinary 六合 pays first, restores 8, and grants Armor 24 to each living ally exactly once. It works even when it is the first Universal card and no Ice branch is locked yet. Only the caster's current Mana changes; every MaxMana and other allies' Mana remain unchanged.
- With a previous recorded non-direct-damage card, replace 8 with 16: active grant 72 per ally. A first record or a predecessor containing direct damage uses 8 even if the attack dealt zero HP damage. An interleaved other-owner card must not replace the previous Sorcerer record. Free replay at full Mana gives 48/96 for the locked ordinary/conditional branch, without paying 4 or recomputing predecessor from the latest chronological action.
- Starting Mana 4, ordinary active 六合 ends Mana 8/34 with zero Armor for every ally; Mana 3 rejects without mutation. Fixed recovery 8/16 stays unchanged across legal qualities and levels; only actual overflow Armor is scaled. Vary allies' Mana, Defense, and level without recalculating their individual overflow. Exclude defeated recipients and never split one Armor budget among living allies.
- The generic Ice overflow listener must not add a second owner-only grant after the composite group grant. Zero overflow produces zero group Armor. Preview, committed results, and a resumed paused queue must agree on one Mana recovery and one grant per recipient.
- 六合's Ice reward consumes the caster's Armor for standard Ice, then adds floor(consumed/4) to each living ally, with no remaining 40%-Defense addition. At consumed Armor 1003, every living ally receives 250 even when actual HP damage is phase-limited. Preserve Armor already on the other recipients.
- With confirmed 10% Ice cards, 六合→寒息→周天→冰鉴→霜镜 at the shared minimum-quality benchmark yields pre-reward Armor 774, potential Ice damage 2782, and a new group reward of 193 per ally. The caster ends at 193; the other two initially unarmored allies end at 265 after retaining the base grants. Check the 16-Mana branch with 照见→寒息→六合→冰鉴→霜镜 (912 Armor / 3161 potential damage; final Armor 228/168/168) and 斗转→寒息→六合→冰鉴→霜镜 (1728 / 5915; final Armor 0/168/168). Prior 25%/20% outcomes are historical comparisons, not passing expectations.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests 'GameXXK.Data.PartnerCards.Sorcerer+GameXXK.Data.HeroCards.ApprovedCatalog' --automation-report InRun02_Task06_RED --json
```

- [ ] **Step 3: Encode all 36 definitions**

Use design section 6.3. Make all six switch cards cost 1, destination-trigger once, and grant explicit Mana 0/2/4. Terrain-only cards use `TargetMode=None`; cards with an independent attack retain that attack target. Preserve trigger counts 2 and 3 where approved.

For partner Ice, keep the stable CardIds, base qualities, costs, and reward families. Replace the former Defense-derived base effects with the user-confirmed 10%-Mana recovery on all four cards. Reuse the existing integer generation authority for overflow Armor, after upward-rounded recovery and capacity accounting. 玄冰 must increase capacity before recovery. 霜镜 must choose recovery or exact doubling based on Armor at execution time. 冰鉴's unavailable-search branch must copy the first recovery's resolved Armor delta, including zero, rather than cloning a Mana-recovery effect or the owner's complete Armor. Express this through the existing Sorcerer rule/effect dispatch; do not add CardId string checks. Do not rescale the copy or impose the DOT reservoir cap. Standard Ice consumes the resulting Armor once and uses `100 * starter Q + consumed Armor` attack-percentage points; scale only that 100-point base, then retain all authored secondary rewards. Add reward assertions for zero and 300 consumed Armor: legal quality bases are 100/120/140 and the latter totals 400/420/440. Exercise standard Ice through all four Ice starters and the 照见/六合/斗转 Ice branches. Do not bulk-replace the separate 220-point 万法 or other cards' authored conversion bases.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests 'GameXXK.Data.PartnerCards+GameXXK.Data.HeroCards.ApprovedCatalog' --automation-report InRun02_Task06_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerRewardRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerIceLightningRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRewardMatrixTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTextTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerSaveResumeTest.cpp Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp
git diff --cached --check
git commit -m "feat: rebalance sorcerer and formation cards"
```

### Task 7: Rebalance all 24 NPC cards and five Boss cards

Confirmed slice completed at `4d21696`: 23 NPC cards and all five Boss cards are green across 21 scoped tests. `HouXiangTuoShen` remains unchanged pending the explicit target decision. Evidence: `docs/production/2026-09-04-task-npc-boss-card-progress.md`.

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcAllCardsRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteRewardGateTest.cpp`

- [ ] **Step 1: Add red exact-ID assertions**

Assert `Npc.JinGui.ShiJingErMu` displays `市井耳目`; `Npc.SongJinBao.ErMuMiBao` remains `耳目密报`; terrain-only NPC cards require no target; `Route.Boss.*` definitions are Epic and match 196/252/196/196/322 final coefficients without entering single-map rewards.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.TaskNpcCards --automation-report InRun02_Task07_RED --json
```

- [ ] **Step 3: Encode sections 6.4-6.5**

Use exact full CardIds from the design. Retain NPC task ownership and at-most-one completion per player round. Treat values labeled Rare/Epic as final quality results by storing their unscaled base plus policy; for example Boss 252 stores base 180 with ContinuousQuality, while Vulnerability 5 remains unscaled.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.TaskNpcCards --automation-report InRun02_Task07_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcAllCardsRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteRewardGateTest.cpp
git diff --cached --check
git commit -m "feat: rebalance npc and boss cards"
```

### Task 8: Project runtime-resolved Armor, DOT, Medicine, and fixed values

**User review gate (latest instruction):** Before changing any pill text, tooltip copy, compact/expanded formatting or pill presentation, prepare a reviewable per-card text table and obtain the user's approval of that batch. Existing gameplay Tasks 6/7 remain authorized. The display rules and effect examples are in docs/design/2026-09-03-card-tooltip-text-review-draft.md; these are not approval of 173 final card texts. Attack must show calculated damage numbers on the card/compact text, using the actual effect source rather than always the Hero or card owner. The latest user instruction confirms that expanded attack text needs only a percentage sentence such as “造成120%的攻击伤害”; do not add the current Attack number, calculated damage, parenthesized arithmetic, a parameter list, or a separate quality note. Do not append the attacker to the attack sentence, even for borrowed Attack; the internal numeric source remains correct. Preserve recipient scope, card conditions, hit counts and additional effects. The user also confirmed “6点流血，600%增幅倍率” and “25点治疗，600%增幅倍率”: merge applicable quality/level scaling into the final amplification percentage, never a separate quality bonus or another multiplication. The complete173-card/419-quality-version review is now in docs/design/2026-09-03-all-card-text-review/README.md, with full branch and shared-tooltip text. Format approval is not approval of every per-card batch. Target Defense, combat statuses, Armor absorption, level-difference resolution and final HP loss belong only to the MONSTER tooltip shown on mouse hover. Keep the two surfaces separate in the reviewed table; do not append the target combat ledger to card detail. Preserve all previously confirmed gameplay formulas.

- [ ] **Step 0: Prepare and obtain review of the text table**

**Approved pill information split:** remove basic Energy/Mana resources from card pill help and explain them at their resource bars. When both Charge and Heavy Arrow are present, explain them together while retaining both source tags; show shared DOT behavior once for cards that involve DOT. Move the four Universal cards' automatic-hand trigger/once-per-battle rules into card copy, and put Heavy Arrow consumption timing into the card's own detail. Keep meaningful limits such as formula isolation and per-round task caps; omit zero-value skipping, redundant exceptions and generic “see card text” clauses. The right-click list remains card/quality/branch-scoped. Poison's approved line is “任意一方回合结束时，失去等同中毒值的生命。” and runtime timing must tick both sides at either boundary without accelerating Weak decay.

**Confirmed tooltip interaction:** default hover shows compact text; holding Shift temporarily shows detailed card effects. A fresh CTRL press toggles the current card's pill help on/off; releasing Ctrl must not close it and holding the key must not repeatedly toggle. Right and middle mouse buttons keep their existing actions. This supersedes the earlier right-click/middle-click proposals. Releasing Shift restores the current compact/pill-help mode. Mouse leave, Escape and window blur clear the temporary inspection state. Pill help lists only the current card/quality's actual pills, deduplicated in appearance order; a locked task branch excludes other branches. Keep each explanation to one or two short sentences containing its trigger, effect and essential limits. Target headings are not effect pills. The current `UGameXXKCardTooltipWidget` appends `AppendStatusPillExplanations` to `ExpandedBody`; split that into a dedicated Ctrl-toggle body during implementation. Preserve card-specific formula/task effects in detail, but move generic keyword definitions out of Shift detail. Add interaction checks for Ctrl-open/no-key-repeat/key-release-stays-open/Ctrl-close, Shift restore, card changes and correct pill membership. The review prototype and per-card help texts are in `docs/design/2026-09-03-all-card-text-review`, including `11-card-pill-descriptions.md`.

**Current approved integration slice:** shared Ctrl/Shift reading state, standalone recipient headings and concise current-card Pill help. The user approved the reviewed presentation and then chose Ctrl as the final input. This does not approve the two flagged card-design candidates or mark the remaining numeric-preview/card-catalog work complete. Follow the runtime definitions until their corresponding gameplay tasks are implemented.

- [x] **Shared tooltip interaction / typography slice:** runtime f31ca57 implements Ctrl press-toggle (no repeat; release persists), Shift override/restore, dedicated Pill help, recipient headings and 22/14/13 font hierarchy. Cold UBT and13/13 scoped Automation passed. See docs/production/2026-09-03-ctrl-card-tooltip-acceptance.md. Remaining numeric-display steps below are still open.

The user additionally requires a separate bold recipient line at the start of compact and expanded copy, containing only the recipient label without an “对象：” prefix or inline explanations: 单体友方, 单体敌方, 单体友方/敌方, 全体敌方, 全体友方. The full review now includes these in all419 quality variants and36 Universal reward variants. Mark self-only and automatic selections separately; for a selected-side group heal show both single-unit selection and whole-side application, and distinguish consumed-Armor donors from group damage recipients. These describe recipients, never reintroduce attack-source attribute annotations or alter gameplay targeting. Keep pure deck/resource actions targetless and retain terrain/replay-dependent scopes.

For each scoped card, list stable ID, quality/cost, actual attribute source, recipient/branch condition, pill names/order, complete numeric compact copy, card-only expanded copy, a separate monster-hover result sample, computation stage/example values, and unresolved candidates. Expanded attacks use the confirmed percentage sentence; the remaining effects' line structure stays subject to the table review. Review all branches and explicit quality differences. Do not use the representative effect-fragment sample as approval of unlisted card text. Keep uncertain future task rewards symbolic unless the actual finishing sequence is simulated. The short version must retain essential conditions rather than truncating them with ellipses. Visual layout/PIE acceptance follows approval of the table.

**Files:**
- Review draft: docs/design/2026-09-03-card-tooltip-text-review-draft.md
- Modify after text approval: Source/GameXXK/Private/UI/GameXXKCardTooltipWidget.cpp
- Modify after text approval: Source/GameXXK/Public/UI/GameXXKCardTooltipPresentation.h
- Modify after text approval: shared tooltip presentation functions in Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp`

- [ ] **Step 1: Write red value-projection and text-parity tests**

Add a strict surface-boundary check: Attack100 and effective card coefficient120% show numeric damage120 on the card, while expanded attack text says “造成120%的攻击伤害” without an Attack100 line, a repeated damage120 result or arithmetic. A borrowed attacker must still provide the internal numeric value, but is not appended to the attack sentence. Against an otherwise neutral same-level monster with Defense20 and Armor10, only the monster-hover tooltip reports Armor absorbed10 and HP loss90. Card detail must not acquire target Defense/status/Armor/HP-loss lines when the hovered monster changes. Verify each display against its own resolution stage rather than incorrectly equating card damage120 with final HP loss90.

The Blade Finish budget text must display the actual coefficient20/quality/TeamMaxLevel result (or current remaining budget in state tooltips), replacing the interim coefficient wording. Blood Edge remains +2 attack-percentage points per resolved Bleed point; no second DOT generation scale is applied to that conversion.

The Task 3 extended check found the existing `Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp` still assumes QingFeng base 140% and flat healing 12→Rare15. Update its numeric fixture to approved base 100%→Rare120% (Attack20 against zero Defense leaves target HP476) and Medicine coefficient15 (level1/Rare/Medicine0 resolves19 healing, so ally HP20→39). Verify numeric compact values against source generation and monster previews against committed target outcomes, while checking expanded attack percentages against the card's coefficient. Do not delete assertions or replace compact damage numbers with percentages. The expected healing coefficient remains15 until owner/quality/level context is applied once.

Verify all 22 Mage printed fees and effective-cost previews use the confirmed prices. Explicit discounts only alter payment. An unaffordable final card stays unavailable even when its prospective task reward returns Energy. Free queued replays do not rewrite printed costs. Regenerate card documentation so the old all-zero Universal rule cannot reappear.

At TeamMaxLevel 100, assert coefficient-6 DOT previews 30/36/42 by quality and cap 100; coefficient-25 healing with owner Medicine 6 previews 155/186/217; Defense 358 produces the approved printed-cost Armor values; and card fixed-damage numbers use their own generation/quality calculation, while target-level and other target-resolution effects remain in the monster-hover result. Assert numeric compact text, card-only formula detail, and no consumable-layer wording for DOT. Compact Armor shows the resolved number; its own Defense coefficient and quality belong in expanded card detail. Card values match their generation stage, while monster-hover values match the committed target outcome and combat log.

When legal targets produce different fixed damage or remaining DOT capacity, keep the card face on its own generated value and show each candidate monster's exact actual outcome in that monster's tooltip. A no-target or all-target card with one shared owner-context value shows one integer.

For the revised Ice cards, preview and committed results must agree on recovery, overflow, and Armor separately. Reuse the Task 6 34/34, 31/34, and 30/34 fixtures, including zero overflow and the failed-search Armor copy. A preview of 玄冰 accounts for its +4 capacity before predicting recovery, and a preview of 霜镜 distinguishes recovery from doubling. A Defense-only change must not change the displayed Mana-derived base Armor. Keep card text faithful to the percentage-Mana recovery plus overflow rule; do not display the retired 40%/80%-Defense base effect or call recovered Mana itself an Armor percentage.

霜镜's task tooltip/reward projection must describe and display 25% of this Ice blast's consumed Armor for each living ally, rounded down. Snapshot the consumed amount before the owner loses Armor; do not display the former 40%-Defense reward, a quarter of zero post-consumption Armor, or a quarter of dealt damage. Ordinary card preview still shows only the base effect unless that play will complete the starter-owned task.

六合's text and preview show current-Mana recovery 8/16 for the caster and the resolved shared overflow Armor for each ally, using cost-before-recovery and the locked sequence condition. Its Ice reward projection shows only the consumed-Armor quarter after the standard attack. Remove the old 40%/80%-Defense **base/sequence** and Ice-reward addition from this card's current text without rewriting its separately authored Normal/Fire/Lightning rewards.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardText --automation-report InRun02_Task08_RED --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.UI.CardOutcomePreview --automation-report InRun02_Task08_UI_RED --json
```

- [ ] **Step 3: Add non-mutating resolved display values to the preview**

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardResolvedDisplayValue
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 EffectIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FName TargetUnitId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 ResolvedMagnitude = 0;
    UPROPERTY(BlueprintReadOnly) int32 ActualMagnitude = 0;
    UPROPERTY(BlueprintReadOnly) int32 ReservoirCap = 0;
};
```

Add `TArray<FGameXXKCardResolvedDisplayValue> ResolvedDisplayValues` to `FGameXXKCardPlayPreview`. Populate CARD values using the same generation helpers as commit: actual source Attack and card multiplier, printed-cost/secondary Armor from caster Defense, DOT generation from TeamMaxLevel, Medicine healing from owner Medicine, and the card's own fixed-damage calculation. Retain target capacity, Defense/status/level-difference resolution, Armor absorption and HP loss in the separate monster-hover outcome projection. Card values use the card generation stage, not the final target outcome. Previewing uses state copies, consumes no live RNG/resources, and does not mutate live task/formula state.

- [ ] **Step 4: Replace legacy formula/layer wording**

`GameXXKCardText` consumes source-resolved display values. Compact card text shows numeric Attack, Armor, DOT, Medicine healing/reversal and fixed damage. Expanded attacks use the approved percentage sentence only; DOT/healing use base points plus the combined amplification percentage, and Armor uses its own coefficient plus the applicable amplification percentage. Quality is already included in those final multipliers and must not be listed or applied again; it does not include target Defense/status/Armor/HP-loss accounting. Out-of-battle cards use an available owner or an explicitly labeled reference context, never silently replace numeric Attack with only a percentage. Ordinary DOT triggers state that they read the current reservoir without consuming it; only explicitly consuming cards say consume. Full Bleed/all-DOT cleanses say `全部` and never show a layer count.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardText --automation-report InRun02_Task08_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.UI.CardOutcomePreview --automation-report InRun02_Task08_UI_GREEN --json
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardText.cpp Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp
git diff --cached --check
git commit -m "feat: show resolved card combat values"
```

### Task 9: Certify 173 cards and regenerate documentation

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardDocumentationTest.cpp`
- Modify: `docs/design/2026-08-11-full-card-catalog.md`
- Create: `docs/production/2026-09-03-173-card-review-status.md`

- [ ] **Step 1: Make the audit require exactly 173 active IDs**

Assert every active CardId can build a legal preview and commit in its positive fixture at every legal quality, retired prefixes are absent, and five Boss IDs are catalog-valid but reward-gated. Assert every numeric effect has an explicitly authored magnitude policy and no active definition relies on effect-type fallback/default inference.

- [ ] **Step 2: Run broad card green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data --automation-report InRun02_AllCards_GREEN --json
```

Expected: no failure/error/not-run. Investigate any warning whose CardId is in the active 173 rather than blessing it.

- [ ] **Step 3: Regenerate the human catalog from runtime definitions**

Update the catalog title/counts and output resolved Common/Rare/Epic semantics. Move the prior 198-card certification to a clearly historical section; do not rewrite historical hashes as new results.

- [ ] **Step 4: Commit certification docs**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp Source/GameXXK/Private/Tests/GameXXKCardDocumentationTest.cpp docs/design/2026-08-11-full-card-catalog.md docs/production/2026-09-03-173-card-review-status.md
git diff --cached --check
git commit -m "test: certify active 173 card pool"
```

Plan 2 is complete only when runtime, generated documentation, and tests independently report 173, no retired non-Boss route card is acquirable, and battle card text contains the same resolved numbers as preview and commit.
