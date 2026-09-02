# Active Card Catalog Rebalance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 198-card legacy catalog with the approved 173-card pool and encode every approved player-card value without CardId-specific runtime branching.

**Architecture:** Card effects gain a declarative magnitude policy and optional Rare/Epic values. Catalog builders declare coefficients and policies; quality resolution and `GameXXKCardRules` interpret them through Plan 1's scaling authority. Retired run-local cards are removed by a v34 migration while five Boss IDs remain valid but unavailable in single-map rewards.

**Tech Stack:** Unreal Engine 5.8 C++, existing card catalog/rules, SaveGame migration, UE Automation, generated Markdown catalog tests.

---

## Preconditions and guard

- Complete `2026-09-03-combat-scaling-foundation-implementation.md` first; v33 and `FGameXXKCombatScalingRules` must be green.
- Work on `codex/overall-in-run-optimization` in the root checkout.
- Preserve unrelated dirty assets and never stage them.
- Use the exact CardIds and values in design sections 3, 4, and 6.

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

- [ ] **Step 1: Write red policy tests**

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

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardQuality --automation-report InRun02_Task01_RED --json
```

- [ ] **Step 3: Add policy fields**

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

- [ ] **Step 4: Add catalog builder helpers**

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

- [ ] **Step 5: Run green and commit**

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

- [ ] **Step 1: Write red catalog and migration tests**

Assert active counts 173/102 Common/42 Rare/29 Epic, no IDs with `Route.General.`, `Route.Terrain.`, or `Route.Rare.`, and exactly five `Route.Boss.*` IDs. Build a v33 save containing retired instances in every deck zone, reward, and merchant field; migration removes them and repairs `ActiveInstanceIds`. A `Route.Boss.XiongPiPiJia` instance survives.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardCatalog --automation-report InRun02_Task02_RED --json
```

- [ ] **Step 3: Remove active definitions and reward sources**

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

- [ ] **Step 4: Implement v34 retirement migration**

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

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardCatalog --automation-report InRun02_Task02_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun02_Task02_Save_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKRetiredRouteCardMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteRewardGateTest.cpp
git diff --cached --check
git commit -m "feat: retire legacy route cards"
```

### Task 3: Encode and verify all 36 Hero cards

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp`

- [ ] **Step 1: Add exact red catalog rows**

Create one table-driven expected row for each Hero CardId. The first critical rows are:

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

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards --automation-report InRun02_Task03_RED --json
```

- [ ] **Step 3: Update catalog and composite resolution**

Use `Continuous`, `Explicit`, `Dot`, `PrintedArmor`, `DefensePercent`, and `MedicineCoefficient` builders. Implement no new `if (CardId == ...)` branch in `GameXXKCardRules.cpp`; extend generic effect/rule structs when a behavior is missing. In particular:

```cpp
AddHero(TEXT("Hero.Generic.FengShenBu"), TEXT("风身步"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
    {Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility),
     Explicit(Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), 3, 4),
     Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1)},
    EGameXXKCharacterRole::Invalid, 1, true);
```

Encode Hero Guard Armor with policies rather than flat points; encode DOT coefficients before level expansion; encode Hero Sorcerer task size 4 and Hero Healer four formula sources exactly as specified.

- [ ] **Step 4: Run Hero green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards --automation-report InRun02_Task03_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: rebalance hero card catalog"
```

### Task 4: Rebalance partner Blade and Guard cards

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCounterflowRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`

- [ ] **Step 1: Add red assertions for CardIds 061-096**

Assert Blood Edge uses 2 points per resolved DOT, `ZhanJin` base 300, `HengYunKaiFeng` base 100, `BaoDaoShouYe` Agility 2, and all Guard primary Armor effects use PrintedCostArmor. Assert `ZhenYueLing` base 180 plus one per Armor and `BiLeiFanGong` base 220 plus one per Armor.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards.Blade --automation-report InRun02_Task04_RED --json
```

- [ ] **Step 3: Encode the 36 approved definitions**

Use exact values in design section 6.3. Preserve card ownership, target modes, and existing Charge/Finish behaviors unless overridden. For Armor conversion, store quality-scaled base in `Magnitude` and unscaled `+1 per Armor` in `SecondaryMagnitude=1`.

```cpp
Effect.Type = EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor;
Effect.Magnitude = 180;
Effect.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
Effect.SecondaryMagnitude = 1;
```

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards --automation-report InRun02_Task04_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKBladePartnerCounterflowRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp
git diff --cached --check
git commit -m "feat: rebalance blade and guard partner cards"
```

### Task 5: Rebalance partner Healer and Hunter cards

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHunterPartnerCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHunterPartnerCoreRuntimeTest.cpp`

- [ ] **Step 1: Add red rows for CardIds 097-132**

Assert raw coefficients rather than level-100 displays: `FuGuSan` Attack 60, Bleed 6, Poison 4; `YaoNangFeiTou` Attack 45, Bleed 3, Poison 1; `LianZhuJian` Bleed 8, Poison 6, Attack/Heavy 50; `DuanMaiShi` Bleed 8 and +30 per Charge; `PoJiaDing` Poison 1 and +25 per Charge.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards.Healer --automation-report InRun02_Task05_RED --json
```

- [ ] **Step 3: Encode all 36 definitions and formula rules**

Use design section 6.3 exactly. DOT fields use `DotCoefficient`, healing/reversal fields use `MedicineCoefficient`, Heavy-Arrow damage uses `ContinuousQuality`, and Charge/status counts remain explicit integers. Keep one owner-scoped formula per source CardId and forbid recursive formula satisfaction.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards --automation-report InRun02_Task05_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHunterPartnerCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHunterPartnerCoreRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: rebalance healer and hunter partner cards"
```

### Task 6: Rebalance partner Sorcerer and Formation Master cards

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerRewardRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`

- [ ] **Step 1: Add red CardId 133-168 and unlock assertions**

Assert task size 5, Fire conversion 2 points per Burn, Ice base 200 plus one per Armor, `ZhenShaZhen` base 320/Epic 448, and Formation unlock counts 12/14/16/18 at levels 1/5/10/15 with all six switch cards present at level 1.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards.Sorcerer --automation-report InRun02_Task06_RED --json
```

- [ ] **Step 3: Encode all 36 definitions**

Use design section 6.3. Make all six switch cards cost 1, destination-trigger once, and grant explicit Mana 0/2/4. Terrain-only cards use `TargetMode=None`; cards with an independent attack retain that attack target. Preserve trigger counts 2 and 3 where approved.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.PartnerCards --automation-report InRun02_Task06_GREEN --json
git add Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKSorcererPartnerRewardRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp
git diff --cached --check
git commit -m "feat: rebalance sorcerer and formation cards"
```

### Task 7: Rebalance all 24 NPC cards and five Boss cards

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

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp`

- [ ] **Step 1: Write red value-projection and text-parity tests**

At TeamMaxLevel 100, assert coefficient-6 DOT previews 30/36/42 by quality and cap 100; coefficient-25 healing with owner Medicine 6 previews 155/186/217; Defense 358 produces the approved printed-cost Armor values; and fixed damage reflects quality plus source/target level difference while bypassing Defense. Assert card detail/compact/expanded text contains these resolved integers, contains no raw Defense percentage for a resolved Armor grant, does not call DOT a consumable layer, and matches the committed `FGameXXKCardPlayResult`/combat-log number.

When legal targets produce different fixed damage or remaining DOT capacity, assert the card face shows the resolved minimum-maximum range and each candidate tooltip shows its exact actual value. A no-target or all-target card with one shared owner-context value shows one integer.

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

Add `TArray<FGameXXKCardResolvedDisplayValue> ResolvedDisplayValues` to `FGameXXKCardPlayPreview`. Populate it with the same generic helpers used by commit: printed-cost/secondary Armor from owner Defense, DOT additions from TeamMaxLevel and remaining target capacity, Medicine healing from owner Medicine, and fixed damage from quality plus source/target level. Previewing uses state copies, consumes no live RNG/resources, and never runs task/formula side effects. If target values differ, aggregate a min-max card-face range while retaining exact candidate rows.

- [ ] **Step 4: Replace legacy formula/layer wording**

`GameXXKCardText` consumes `ResolvedDisplayValues` when a battle preview exists and falls back to authored coefficients only in out-of-battle catalog documentation. In battle, render direct integers for Armor, DOT, Medicine healing/reversal, and fixed damage. Ordinary DOT triggers state that they read the current reservoir without consuming it; only explicitly consuming cards say consume. Full Bleed/all-DOT cleanses say `全部` and never show a layer count.

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
