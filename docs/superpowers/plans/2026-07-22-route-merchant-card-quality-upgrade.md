# Route Merchant, Card Quality, Upgrades, and Travel-Money Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved three-quality card/relic economy, deterministic card merging, route-only travel money, six-slot route merchant, save migration, and PSD-consistent route/merchant HUD without changing the permanent town economy.

**Architecture:** Keep immutable catalog data separate from route-runtime instances. Materialize an effective card definition from `BaseQuality` plus the runtime entry's `CurrentQuality`, and make battle resolution, previews, tooltips, rewards, and merchant purchases consume that same effective definition. Put travel money, the 12-entry temporary route deck, merchant stock, refresh count, and node-payout receipts inside the save-authoritative route state. Route merchant rules own stock generation and atomic purchase transactions; widgets only render snapshots and call the subsystem facade.

**Tech Stack:** Unreal Engine 5.8, C++20/UE reflection and automation tests, UMG/Slate built in C++, project UE MCP scripts, Python asset-import tests, UBT cold builds (no Live Coding/Hot Reload), image generation for the three missing route-merchant source assets.

**Authoritative specification:** [`docs/superpowers/specs/2026-07-22-route-merchant-card-quality-upgrade-design.md`](../specs/2026-07-22-route-merchant-card-quality-upgrade-design.md)

---

## Implementation guardrails

- Work in `D:\UE5 demo\GameXXK` on `main`. Do not create a worktree and do not use UnrealBridge.
- The worktree already contains user-owned edits and assets. Before each task, run `git status --short` and `git diff -- <touched-files>`. Never revert or overwrite unrelated hunks.
- For new files, stage the exact paths. For already-dirty files, commit only when the feature hunks can be isolated safely; otherwise leave them unstaged and record the passing verification in the task notes.
- Before any cold build, use `python scripts/ue_tdd_pipeline.py` so the running editor is saved through UE MCP and closed safely. If UE MCP cannot save dirty packages, stop; do not force-close the editor.
- Do not use Live Coding or Hot Reload as evidence. `--check-only` is diagnostic only.
- Do not modify character sprites, PaperZD assets, placed levels, cameras, or HD2D transforms in this feature.
- Use fixed seeds in all catalog, merge, merchant, and save tests. UI may render random-looking offers, but the underlying stock must be deterministic and saved.

## File map

### New gameplay files

- `Source/GameXXK/Public/GameXXKCardQualityRules.h` — quality metadata, catalog classification, price/color/text helpers, effect scaling, and effective-definition materialization.
- `Source/GameXXK/Private/GameXXKCardQualityRules.cpp` — exact 174-card and 30-relic quality maps plus scaling implementation.
- `Source/GameXXK/Public/GameXXKRunDeckRules.h` — route entry creation, deterministic pair merging, merge preview, capacity/replacement queries, and legacy migration.
- `Source/GameXXK/Private/GameXXKRunDeckRules.cpp` — run-deck implementation with stable entry identity.
- `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h` — serializable merchant offer/state/preview/result structs.
- `Source/GameXXK/Public/GameXXKRouteEconomyRules.h` — route currency initialization, payout receipts, and route-local reset.
- `Source/GameXXK/Private/GameXXKRouteEconomyRules.cpp` — travel-money implementation.
- `Source/GameXXK/Public/GameXXKRouteMerchantRules.h` — stable stock generation, refresh, purchase preview, and atomic commit API.
- `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp` — merchant rules implementation.
- `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h` — dedicated merchant HUD and test seams.
- `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp` — fixed-safe-area merchant HUD, cards/relics, confirmation/replacement flow, and input-safe close.

### Existing gameplay files to modify

- `Source/GameXXK/Public/GameXXKCardTypes.h` — add `EGameXXKCardQuality`, `BaseQuality`, and runtime `CurrentQuality`.
- `Source/GameXXK/Public/GameXXKCardRunTypes.h` — replace temporary `RouteCardIds` ownership with persistent `FGameXXKRouteCardEntry` instances and add route economy/merchant state.
- `Source/GameXXK/Private/GameXXKCardCatalog.cpp` — assign every card's `BaseQuality` from the exact classification.
- `Source/GameXXK/Public/GameXXKRelicTypes.h` and `Source/GameXXK/Private/GameXXKRelicCatalog.cpp` — add relic `BaseQuality` and `GainRouteTravelMoney`.
- `Source/GameXXK/Private/GameXXKRelicRules.cpp` — reject duplicate relic ownership and route `WineCup` money into travel money.
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — materialize battle cards from persistent run entries, merge route rewards, and clear all route-local state.
- `Source/GameXXK/Private/GameXXKCardRules.cpp` — resolve the already-materialized effective definition; no CardId-specific quality branches.
- `Source/GameXXK/Public/GameXXKCardText.h` and `Source/GameXXK/Private/GameXXKCardText.cpp` — quality-aware text and tooltip presentation.
- `Source/GameXXK/Public/GameXXKMVPRules.h` and `Source/GameXXK/Private/GameXXKMVPRules.cpp` — award/reset route currency, delegate merchant transactions, and migrate save version 6 to 7.
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` and `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp` — expose merchant/economy facade calls.
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` and `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — own/focus the dedicated merchant widget and remove merchant from the generic encounter panel path.
- `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h` and `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp` — retain only event/camp handling; remove unconditional close during active construction.
- `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h` and `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp` — top-left travel money/progress/temp-card HUD.
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` and `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` — quality on hand/reward/replacement faces and stable-entry replacement.
- `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h` and `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp` — base-quality labels/tooltips for companion cards.
- `Source/GameXXK/Public/UI/GameXXKRelicBarWidget.h` and `Source/GameXXK/Private/UI/GameXXKRelicBarWidget.cpp` — quality in relic tooltip while preserving the current six-column unlimited wrap.

### New tests and asset pipeline

- `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRunDeckRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteEconomyTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`
- `scripts/test_route_merchant_asset_pipeline.py`
- `Content/Python/gamexxk_import_route_merchant_ui.py`
- `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_TravelMoney.png`
- `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_Portrait.png`
- `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_Paper.png`
- Imported assets under `Content/GameXXK/UI/RouteMerchant/Textures/`.

---

## Task 1: Add quality types and exact catalog classification

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCardQualityRules.h`
- Create: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Public/GameXXKRelicTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKRelicCatalog.cpp`

- [x] **Step 1: Write the failing parameterized catalog test**

Add automation assertions for:

```cpp
TestEqual(TEXT("card total"), FGameXXKCardCatalog::All().Num(), 174);
TestEqual(TEXT("common cards"), CountCards(EGameXXKCardQuality::Common), 92);
TestEqual(TEXT("rare cards"), CountCards(EGameXXKCardQuality::Rare), 51);
TestEqual(TEXT("epic cards"), CountCards(EGameXXKCardQuality::Epic), 31);
TestEqual(TEXT("relic total"), FGameXXKRelicCatalog::All().Num(), 30);
TestEqual(TEXT("common relics"), CountRelics(EGameXXKCardQuality::Common), 15);
TestEqual(TEXT("rare relics"), CountRelics(EGameXXKCardQuality::Rare), 10);
TestEqual(TEXT("epic relics"), CountRelics(EGameXXKCardQuality::Epic), 5);
```

Also iterate every ID in appendices A and B of the approved specification and assert its exact quality. Assert that `EGameXXKCardRarity` remains unchanged and still expresses acquisition/source semantics.

- [x] **Step 2: Run the red build**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload
```

Expected: compile failure because `EGameXXKCardQuality` and `BaseQuality` do not exist.

- [x] **Step 3: Add the reflected quality fields**

Add without renumbering `EGameXXKCardRarity`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKCardQuality : uint8
{
    Invalid = 0 UMETA(Hidden),
    Common = 1,
    Rare = 2,
    Epic = 3
};
```

Add `EGameXXKCardQuality BaseQuality = EGameXXKCardQuality::Common;` to both `FGameXXKCardDefinition` and `FGameXXKRelicDefinition`. Add `CurrentQuality` to `FGameXXKCardInstance` so battle snapshots carry the resolved route-entry quality.

- [x] **Step 4: Implement exact classification with Common as the default**

`GetCardBaseQuality(FName)` must return Epic for exactly these 31 IDs, Rare for exactly these 51 IDs, and Common for all other catalog IDs:

```text
Epic cards:
Hero.JianYiGuanHong, Hero.GuiYuanFanZhao,
Npc.TusiChief.MengZhaiShiYue, Npc.SongJinBao.YiNuoQianJin,
Npc.YueBai.ShanHeCanTu, Npc.ZhouGuangZu.YanFenFengMai,
Npc.JinGui.HouXiangTuoShen, Npc.QiongMeiEr.ShanGeHuanLing,
Profession.Blade.CanYueSanDie, Profession.Blade.ZhanJin, Profession.Blade.YiShiDuanJiang,
Profession.Guard.BuDongRuShan, Profession.Guard.TieSuoHengJiang, Profession.Guard.YiFuDangGuan,
Profession.Healer.YaoWangGuiYuan, Profession.Healer.YaoNangFeiTou, Profession.Healer.WuWeiTiaoHe,
Profession.Hunter.ShouHun, Profession.Hunter.BaiBuChuanYang, Profession.Hunter.YingLuo,
Profession.Sorcerer.XingHuoLiaoYuan, Profession.Sorcerer.FenTianJue, Profession.Sorcerer.ChiYanFengJie,
Profession.FormationMaster.ZhenShaZhen, Profession.FormationMaster.WanXiangGuiZhen,
Profession.FormationMaster.SiXiangLianHuan,
Route.Boss.XiongPiPiJia, Route.Boss.HanDiYiShi, Route.Boss.HuPoZhenDan,
Route.Boss.DuKouLieFeng, Route.Boss.FuHuDuanJiang

Rare cards:
Hero.SuiYanJi, Hero.GuanXi, Hero.PoYunYiShan, Hero.HuiFengZhuiJian,
Npc.TusiChief.ZhaiZhuHaoLing, Npc.SongJinBao.ErMuMiBao,
Npc.YueBai.CanJuanPiZhu, Npc.ZhouGuangZu.DiZhiMoTu,
Npc.JinGui.ShiJingErMu, Npc.QiongMeiEr.TengQiaoFeiDu,
Profession.Blade.DuanYue, Profession.Blade.YinXueDao, Profession.Blade.PoJun,
Profession.Blade.ZhanYiFeiTeng, Profession.Blade.XiaoJiaLianJi, Profession.Blade.DaoYiShouShu,
Profession.Guard.FanZhenJia, Profession.Guard.ZhenYueLing, Profession.Guard.QinWangDunJi,
Profession.Guard.TieBiRuShan, Profession.Guard.BiLeiFanGong, Profession.Guard.SuiJiaHuiJi,
Profession.Healer.LingZhiXuMing, Profession.Healer.HuiChunLu, Profession.Healer.WenYangGao,
Profession.Healer.FuGuSan, Profession.Healer.JinChuangXuMing, Profession.Healer.KuShenMaSan,
Profession.Hunter.ChuanYang, Profession.Hunter.LianZhuJian, Profession.Hunter.YinZong,
Profession.Hunter.DuanMaiShi, Profession.Hunter.PoJiaDing, Profession.Hunter.FuYeXianJing,
Profession.Sorcerer.BaoYanShu, Profession.Sorcerer.SheLingHuo, Profession.Sorcerer.LingYanLianDan,
Profession.Sorcerer.HuLingMu, Profession.Sorcerer.ChiXiaoFenXing, Profession.Sorcerer.XingHuoHuiShou,
Profession.FormationMaster.CunZhaiYuanZhen, Profession.FormationMaster.HuiShengZhenSha,
Profession.FormationMaster.BaMenLunZhuan, Profession.FormationMaster.ShuiJingZheGuang,
Profession.FormationMaster.ZhenQiGuWu, Profession.FormationMaster.DiMaiJieLi,
Route.Rare.GuJuanCanZhang, Route.Rare.TieYiYiJue, Route.Rare.LingQuanYiYin,
Route.Rare.JueJingFanJi, Route.Rare.TongXinHeBi
```

Relic Epic IDs are `BambooTally`, `CraneFeather`, `ChessStone`, `DrumCharm`, `OldMap`; Rare IDs are `TigerSeal`, `InkTalisman`, `CloudMirror`, `StoneBead`, `IronKnot`, `Compass`, `RedCord`, `BronzeNeedle`, `LotusSeed`, `SwordGuard`, all with the `Relic.` prefix. The remaining 15 are Common.

- [x] **Step 5: Assign catalog definitions and run green verification**

`GameXXKCardCatalog.cpp` and `GameXXKRelicCatalog.cpp` must assign `BaseQuality` at definition construction. Run the cold build, then:

```powershell
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests GameXXK.Data.CardQuality;Quit" -TestExit="Automation Test Queue Empty" -log
```

Expected: all catalog count and exact-ID assertions pass.

- [x] **Step 6: Leave the verified feature slice unstaged on the dirty main worktree**

```powershell
git add Source/GameXXK/Public/GameXXKCardQualityRules.h Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp
git diff --check
git commit -m "feat: classify card and relic quality"
```

No files were staged or committed because the project is intentionally being developed on a dirty `main` worktree and the touched type/catalog files contain user-owned work. Verification evidence: cold UBT `Result: Succeeded`; `GameXXK.Data.CardQuality` passed; spec review and code-quality review both approved.

---

## Task 2: Build one effective-definition scaling path

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCardQualityRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardText.h`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`

- [x] **Step 1: Add failing table-driven effect tests**

For each base effect, assert Common/Rare/Epic results:

| Effect | Base | Common | Rare | Epic |
|---|---:|---:|---:|---:|
| damage / armor / heal | 7 | 7 | 14 | 28 |
| draw | 2 | 2 | 3 | 4 |
| mana | 3 | 3 | 5 | 7 |
| apply/remove status | 2 | 2 | 3 | 4 |
| energy cost | 1 | 1 | 1 | 1 |
| duration/trigger count | 2 | 2 | 2 | 2 |
| percentage condition | 50 | 50 | 50 | 50 |

Include nested `FGameXXKCardBattleModifier.Magnitude` cases so delayed damage/status magnitudes scale but `RemainingTriggers`, expiry, and conditions do not.

- [x] **Step 2: Run the targeted red test**

Run `GameXXK.Data.CardQuality.Scaling`; expected failures show unscaled Rare/Epic values.

- [x] **Step 3: Implement `BuildEffectiveDefinition`**

Expose:

```cpp
struct GAMEXXK_API FGameXXKCardQualityRules
{
    static FGameXXKCardDefinition BuildEffectiveDefinition(
        const FGameXXKCardDefinition& BaseDefinition,
        EGameXXKCardQuality CurrentQuality);
    static int32 GetCardPrice(EGameXXKCardQuality Quality);
    static int32 GetRelicPrice(EGameXXKCardQuality Quality);
    static FText GetDisplayName(EGameXXKCardQuality Quality);
    static FLinearColor GetDisplayColor(EGameXXKCardQuality Quality);
};
```

Use multiplier `1/2/4` only for `DamagePercentAttack`, `DamageFlat`, `EachLivingAllyAttackSelectedTarget`, `Heal`, and `AddArmor`. Use additive `0/1/2` for `DrawCards`, `ApplyStatus`, `RemoveStatus`, and `RemoveAnyDamageOverTime`. Use additive `0/2/4` for `GainMana` and `GainManaPerConsumedStatus`. Do not scale `LoseHealth`, costs, conditions, hit counts, durations, trigger counts, or percentage modifiers. Apply the same effect-type rule to nested modifier magnitudes.

- [x] **Step 4: Make text consume the effective definition**

Change `DescribeEffects`, `DescribeDetail`, and `DescribeTooltip` to accept a quality. Build the effective definition once, show the effective values, and prefix or badge the quality using:

```cpp
Common: FLinearColor(0.94f, 0.91f, 0.82f, 1.0f)
Rare:   FLinearColor(0.30f, 0.58f, 0.86f, 1.0f)
Epic:   FLinearColor(0.55f, 0.35f, 0.78f, 1.0f)
```

The Chinese labels are `普通`, `稀有`, `珍稀`.

- [x] **Step 5: Run green tests and leave the verified slice unstaged**

Run `GameXXK.Data.CardQuality.Scaling` and `GameXXK.Integration.CardText`. Expected: effective numerical descriptions match resolution inputs and all prior target descriptions remain unchanged.

No files were staged or committed on the dirty `main` worktree. Verification evidence: cold UBT `Result: Succeeded`; `GameXXK.Data.CardQuality.Catalog`, `GameXXK.Data.CardQuality.Scaling`, and `GameXXK.Integration.CardText` passed; spec and code-quality reviews approved.

---

## Task 3: Persist route card entries and deterministic pair merging

> **Execution split:** Task 3A implements the persisted entry vocabulary and pure deterministic merge core only. The capacity boundary is now locked by the approved design: the twelve slots count only temporary route-card entries; hero, companion, quest-NPC, and route-start fallback/base recipe entries do not consume those slots. Capacity/replacement assertions remain Task 3B. Task 3A must not change `RouteCardIds` consumers or battle/reward behavior.

**Files:**

- Create: `Source/GameXXK/Public/GameXXKRunDeckRules.h`
- Create: `Source/GameXXK/Private/GameXXKRunDeckRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRunDeckRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`

- [x] **Step 1A: Write failing entry/merge-core tests**

Cover:

- two Common same-ID entries become one Rare;
- four Common become one Epic through a chain;
- two Rare become one Epic;
- Epic never upgrades and never disappears;
- different CardIds never merge;
- base recipe entry survives over a temporary entry, then lower `AcquisitionOrdinal` wins;
- preview and commit return the same survivor ID, consumed IDs, final quality, and temporary-count delta.

Capacity assertions are intentionally deferred to Task 3B. The locked boundary is twelve temporary route-card entries; base/fallback recipe entries are excluded.

- [x] **Step 2A: Run the merge-core red build/test**

Expected: compile failure because route entry and merge APIs do not exist.

- [x] **Step 3A: Add serializable route-entry types**

Add:

```cpp
UENUM(BlueprintType)
enum class EGameXXKRouteCardSourceKind : uint8
{
    Invalid = 0 UMETA(Hidden),
    HeroBase = 1,
    CompanionBase = 2,
    QuestNpcBase = 3,
    RouteReward = 4,
    Merchant = 5
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteCardEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName EntryId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName CardId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKCardQuality CurrentQuality = EGameXXKCardQuality::Common;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKRouteCardSourceKind SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName OwnerUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bTemporaryRouteCard = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 AcquisitionOrdinal = INDEX_NONE;
};
```

Replace save authority of `RouteCardIds` with `TArray<FGameXXKRouteCardEntry> RouteCardEntries`. Keep the legacy array temporarily only as a migration input and never append new rewards to it.

- [x] **Step 4A: Implement deterministic preview and commit**

Expose:

```cpp
struct FGameXXKCardMergePreview
{
    bool bWillMerge = false;
    FName SurvivorEntryId = NAME_None;
    TArray<FName> ConsumedEntryIds;
    EGameXXKCardQuality FinalQuality = EGameXXKCardQuality::Common;
    int32 TemporaryCountDelta = 0;
};

static FGameXXKCardMergePreview PreviewAdd(
    const TArray<FGameXXKRouteCardEntry>& Entries,
    const FGameXXKRouteCardEntry& Candidate);
static bool AddAndMerge(
    TArray<FGameXXKRouteCardEntry>& InOutEntries,
    const FGameXXKRouteCardEntry& Candidate,
    FGameXXKCardMergePreview& OutApplied,
    FString* OutError);
```

Sort merge candidates by `bTemporaryRouteCard` ascending and then `AcquisitionOrdinal` ascending. Pair exactly two equal CardId/equal-quality entries at a time. Continue until no Common/Common or Rare/Rare pair remains. Preserve array order by replacing the survivor in place and removing consumed entries from highest index to lowest.

- [x] **Step 5A: Run green merge-core tests**

Run `GameXXK.Route.RunDeck.Merge`. Expected: all identity, chain, validation, ordering, and preview/commit parity assertions pass. Capacity/replacement remains Task 3B.

```powershell
git add Source/GameXXK/Public/GameXXKRunDeckRules.h Source/GameXXK/Private/GameXXKRunDeckRules.cpp Source/GameXXK/Private/Tests/GameXXKRunDeckRulesTest.cpp
git commit -m "feat: persist and merge route card entries"
```

Task 3A verification evidence (2026-07-24): cold UBT `Result: Succeeded`; `GameXXK.Route.RunDeck.Merge` found 1 / succeeded 1 / failed 0; `GameXXK.Data.CardQuality` found 2 / succeeded 2 / failed 0. Spec and static code-quality reviews approved. Nothing was staged or committed on the intentionally dirty `main` worktree.

### Task 3B: Shared acquisition, capacity, and stable replacement

- [x] Add persisted `bConsumesRouteCapacity` to each route-card entry. It is the only authority for the twelve acquired-card slots; `bTemporaryRouteCard` remains an orthogonal lifetime/merge-priority flag.
- [x] Append `RouteBase = 6` without renumbering existing source kinds. Hero, companion, quest-NPC, fallback, and fixed route-base recipe entries consume zero capacity; route rewards and merchant acquisitions consume capacity.
- [x] Add one shared preview/commit simulation in `FGameXXKRunDeckRules`. Simulate merge before deciding replacement, re-simulate from the original state after removing a stable replacement `EntryId`, reject an unnecessary replacement, and increment the route acquisition counter only on successful commit.
- [x] Register `GameXXK.Route.RunDeck.Acquisition` for 0/11/12-slot boundaries, merge-at-capacity, different-quality replacement, exact duplicate-CardId identity, invalid/negative/`MAX_int32` rollback, validated capacity queries, and preview/commit parity.
- [x] Keep this pure-rule slice separate from save migration, battle adapter, reward, merchant, and UI integration; those follow in Task 4 and Task 7.

Task 3B pure-rule verification evidence (2026-07-24): initial missing-contract, validated-query, non-capacity acquisition, and negative-counter REDs were reproduced. Cold UBT GREEN; `GameXXK.Route.RunDeck` found 2 / succeeded 2 / failed 0. Card-quality, card-text, route-economy, save-v9, and settlement regressions found 11 / succeeded 11 / failed 0. Independent review reported no Critical or Important findings; nothing was staged or committed.

### Task 3C: Canonical base recipe, v8 migration, and v9 single authority

- [x] Add a shared pure builder for the deterministic 18-card base recipe plus a route-seed/ordinal `EntryId` helper. Add the dedicated persisted `NextRouteCardEntryOrdinal`; do not reuse reward-offer ordinals.
- [x] For every `<9` save, discard any prerelease `RouteCardEntries`, rebuild the canonical base recipe, migrate at most twelve valid legacy `RouteCardIds` in their original index order through deterministic merge rules, then clear the legacy array. Invalid legacy cards warn and consume their ordinal hole; migration must not increment acquisition history.
- [x] Switch new-route initialization and battle materialization to persisted entries. Stable/unique identity and ordinals, at most twelve capacity entries, and inactive-route cleanup of entries plus the next ordinal are validated now.
- [ ] After reward, merchant, and every remaining producer/consumer use entries, remove the read-only compatibility projection and enforce the final v9 single-authority rule that `RouteCardIds` is empty.
- [x] Keep an already-active migrated v8 battle's zones/RNG/instance IDs intact; the next battle becomes the first strict projection from route entries.

Task 3C Phase 2A verification evidence (2026-07-24): two genuine REDs were reproduced (missing recipe contract, then a same-role but locked companion card being accepted). Final cold UBT succeeded; `GameXXK.Route.CardRecipe` found 4 / succeeded 4; RunDeck, CardBattleAdapter, Save v9, RouteEconomy, and Settlement regressions found 11 / succeeded 11. Two independent static reviews found no remaining Critical or Important issues. Nothing was staged or committed.

Phase 2B is a deliberately playable transition: new routes persist and battle-project canonical entries, while an empty-entry direct test fixture and any still-legacy reward IDs retain a read-only compatibility projection. This compatibility path may not create or mutate entries and is removed only after reward selection and v8 migration both write the new authority. Task-NPC changes replace the complete support-slot slice at ordinals 13–15; those three slots reset to the new support recipe's catalog base quality, while every other entry and acquisition counter remains exact.

Task 3C Phase 2B verification evidence (2026-07-24): behavior REDs reproduced missing route initialization, legacy battle materialization/max-two rejection, missing NPC support-slot replacement, and incomplete lifecycle cleanup. Final cold UBT succeeded. CardRecipe, CardBattleAdapter, BattleEntry, QuestNpc, Lifecycle, QingshanTaskNpc, ThreeChapter, Settlement, RouteEconomy, and RouteEconomyV9 filters found 20 / succeeded 20, and all five new canonical-entry integration tests passed individually. Independent review reported zero Critical, Important, or Minor findings. Nothing was staged or committed.

Task 3C Phase 2C1 verification evidence (2026-07-24): the focused v8-to-v9 migration suite first reproduced four behavioral REDs, then passed 4 / 4. A fresh cold UBT succeeded, and the specified save, equipment, run-deck, battle-entry, route-economy, three-chapter, and settlement regressions passed 20 / 20 (24 / 24 total). Migration is candidate-copy atomic, version-beats-fields, preserves active battle snapshots exactly, consumes invalid legacy ordinal holes, rejects int32 sequence overflow, and leaves transitional current-v9 legacy-only data readable until every reward and merchant writer switches authority. Independent review reported no Critical or Important findings; both Minor test-coverage findings were strengthened before the final build. Nothing was staged or committed.

---

## Task 4: Integrate quality entries into battle and route rewards

**Files:**

- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteRewardChoiceTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRulesTest.cpp`

- [x] **Step 1: Add failing adapter/reward tests**

Assert that starting battle instances retain each route entry's `EntryId`, `AcquisitionOrdinal`, owner, and `CurrentQuality`; base hero/companion/NPC recipes are stable across battles; reward selection automatically merges; replacement targets `EntryId`, not CardId; and a merge with no net temporary-card increase skips replacement at capacity.

- [x] **Step 2: Run red targeted tests**

Run `GameXXK.Integration.CardBattleAdapter` and `GameXXK.Integration.CardRoute.RewardChoice`. Expected failures point to regenerated battle IDs and legacy `RouteCardIds` behavior.

- [x] **Step 3: Materialize battle instances from route entries**

At route start, after the route/root seed and task-NPC provenance are finalized, build and persist the exact 18-card base recipe once: eight selected hero cards, the active companion's five selected cards (or shared-cursor fallback cards), three active task-NPC cards (or continued shared-cursor fallback cards), and two fixed route cards. Base and fallback entries do not consume the twelve acquired-card slots. On battle start, copy route entries into `FGameXXKCardInstance` and build an effective definition from catalog definition plus `CurrentQuality`. Do not mutate the immutable catalog entry.

- [x] **Step 4: Route all reward adds through `FGameXXKRunDeckRules`**

`CreateRouteRewardOffer` still offers catalog BaseQuality. `ChoosePendingRouteReward` constructs a new temporary entry with a stable ID derived from route seed and the dedicated `NextRouteCardEntryOrdinal`, previews merge/capacity per chosen candidate, optionally replaces by stable `EntryId`, then commits the merge. Increase the ordinal only after a successful commit, including merge-only commits. Remove the two-copy CardId cap; Epic cap is the quality cap. The legacy global `bRequiresRouteCardReplacement` remains serialization-only and must not decide the new flow.

- [ ] **Step 5: Verify actual combat equals preview/text**

Add a Rare damage, Epic armor, Rare draw, Epic mana, and Epic status card test. Resolve them through normal card rules and compare results to `BuildEffectiveDefinition` and `GameXXKCardText`.

- [ ] **Step 6: Run green tests and commit isolated hunks**

Expected: battle, reward, card rules, and card text tests pass; no existing enemy intent or target-selection test regresses.

---

## Task 5: Add route-only travel money and payout receipts

**Files:**

- Create: `Source/GameXXK/Public/GameXXKRouteEconomyRules.h`
- Create: `Source/GameXXK/Private/GameXXKRouteEconomyRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteEconomyTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKRelicTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKRelicCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKRelicRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEconomyIntegrationTest.cpp`
- Modify: route-flow, relic, settlement, save-migration, HUD, probe, and acceptance tests listed in the Task 5B checkpoint below

- [x] **Step 1A: Write failing pure economy-rule tests**

Assert receipt/default/reflection contracts; exact start balance 60; idempotent initialization; chapter-scoped node awards; zero-value receipts; checked overflow/subtraction; invalid-input rollback; uninitialized `CanAfford`/`Spend`; and economy-only clearing that preserves the complete remaining `CardRun` state.

- [x] **Step 2A: Run the pure-rule red test**

Run `GameXXK.Route.Economy.Rules`. The recorded RED was the expected missing receipt/state/rules API compilation failure.

- [x] **Step 3A: Add route economy state and pure rules**

Add to `FGameXXKCardRunState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RouteTravelMoney = 0;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bRouteEconomyInitialized = false;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKRouteTravelMoneyReceipt> RewardedTravelMoneyNodes;
```

`FGameXXKRouteTravelMoneyReceipt` stores `Chapter`, `NodeId`, and `Amount`. `AwardNodeOnce(Chapter, NodeId, Amount)` uses `(Chapter, NodeId)` as its stable key because generated node IDs restart in every chapter. Implement `InitializeRoute(60)`, `CanAfford`, `Spend`, and `ClearRouteEconomy`; reject invalid inputs and use candidate-copy validation plus checked arithmetic.

- [x] **Step 4B: Write failing route-economy integration tests**

Create `GameXXKRouteEconomyIntegrationTest.cpp`. Assert exact new-route 60; Normal/Elite/Boss 20/35/50; no award before the saved card-reward gate resolves; zero-value nodes still receive receipts; WineCup `3 x stacks`; legacy take-money event 20; formal catalog events do not silently add money; PlayerGold never changes; duplicate completion has no reward side effects; Chapter 1/2 preserve economy; failure/abandon/Chapter 3 terminal settlement clears economy while preserving `LastAppliedRouteSettlementId`; and an invalid/overflowing economy leaves the complete runtime state unchanged.

- [x] **Step 5B: Centralize node settlement and replace direct mutations**

Initialize every new route through `InitializeRoute(60)`. Before one structural node completion, aggregate the node's base award with `WineCup` (`3 x stacks`) and call `AwardNodeOnce` exactly once. Only a newly awarded receipt may apply XP/items/attributes/NPC/relic side effects. Normal/Elite/Boss use 20/35/50; legacy `ResolveEventReward(true)` uses explicit 20 and `false` uses 0; formal event-catalog choices remain attribute/NPC/relic only. Boss settlement must run this same node path before `ResolveBossClear`. Remove direct `AddRouteTravelMoney`, reject negative merchant state instead of clamping, and leave town purchases/sales on `PlayerGold`.

Append `GainRouteTravelMoney` to `EGameXXKRelicEffectKind` without renumbering old values; change only `Relic.WineCup` to the new effect. Split route-node relic handling into a pure money-bonus query plus one-time non-money effects so the receipt gates every side effect.

- [x] **Step 6B: Integrate terminal clearing**

Have settlement cleanup call `ClearRouteEconomy` after the unique atomic receipt is applied, preserving `LastAppliedRouteSettlementId`. An already-applied receipt may clean a matching partially committed snapshot, but replay against a later route is a pure no-op.

- [x] **Step 6C: Add save-version 9 migration and validation**

Freeze the existing three-chapter and merchant feature-version constants at 8, bump the current save version to 9, and migrate valid version-8 active routes into an initialized economy without resetting their chapter or merchant snapshot. Validate receipt fields, unique chapter/node keys, active/inactive initialization invariants, and settlement-source consistency.

- [ ] **Step 7B: Update HUD text, probes, and all affected fixtures**

Change the legacy event copy to “收下 20 行旅钱”, expose route balance/initialization/chapter/receipts through the real-play probes, remove stale `PlayerGold +18/+12` assertions, and initialize every manual active-route fixture explicitly. Route HUD and merchant UI must read the rule-owned balance rather than mutate it.

- [ ] **Step 8B: Run green tests and review**

Run `GameXXK.Route.Economy` plus MVP flow/UI, route map, relic, settlement, three-chapter, save/migration, merchant, lifecycle, playable-shell, and Python probe/acceptance regressions. Expected: exact values and once-only receipts pass; town economy remains unchanged. Cold UBT only, then independent specification and code-quality review; do not stage or commit without an explicit user request.

**Task 5A verification checkpoint (2026-07-24):**

- Cold UBT GREEN: 118 actions, `Result: Succeeded`; reviewer-fix rebuild: 4 actions, `Result: Succeeded`.
- `GameXXK.Route.Economy.Rules`: Found 1, Success.
- `GameXXK.Data.CardQuality` plus `GameXXK.Route.RunDeck.Merge`: Found 3, all Success.
- Independent specification review resolved the full-CardRun clear-preservation test gap. Final code-quality review found one initialized-negative-start ordering issue; its test-first fix plus malformed/duplicate receipt regressions passed cold UBT and both automation filters. Read-only re-review found no remaining Critical or Important issue and approved Step 4B.

**Task 5B1 verification checkpoint (2026-07-24):**

- Real-behavior RED covered route start `0` vs `60`, old `18/24/35/12` payouts, missing receipts, duplicate side effects, overflow partial commit, and terminal economy leakage.
- Review-driven RED/GREEN cycles additionally closed formal-event legacy bypass, battle settlement without a resolved card-reward gate, and destructive replay of an old settlement receipt against a new route.
- Cold UBT GREEN after final fixes. Twelve-prefix route/MVP/relic/settlement/merchant/lifecycle regression union: Found 21, all Success. Direct consumers plus Task 5A: Found 6, all Success. OneGameAdapter: Found 1, Success.
- Independent final review found no remaining Critical or Important issue. No editor, assets, Python probes, save-version migration, staging, or commit were involved.

**Task 5B2 verification checkpoint (2026-07-24):**

- Save feature gates are frozen at three-chapter `8`, merchant snapshot `8`, route economy `9`, and current save version `9`.
- RED covered the version contract, v8 active/inactive migration, legacy-chain ordering, active/inactive invariants, malformed/duplicate node receipts, detached settlement sources, outcome-only and undefined settlement outcomes, plus zero-balance preservation.
- Cold UBT GREEN: `D:\GameXXKBuildTemp\Task5B2\final2_cold_ubt.log`. Focused save/migration/economy/settlement/three-chapter/merchant regression union: Found 19, all Success (`final2_focused_regression.log`).
- Independent review findings on migration ordering, exact empty-receipt comparison, enum validation, version-test drift, and full-runtime preservation were fixed test-first. Final re-review reported no Critical, Important, or Minor findings. Nothing was staged or committed.

---

## Task 6: Generate stable six-slot merchant stock

**Files:**

- Create: `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h`
- Create: `Source/GameXXK/Public/GameXXKRouteMerchantRules.h`
- Create: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [x] **Step 1: Write failing deterministic stock tests**

Cover active companion and no-companion cases, exact three card/three relic slots, stable stock across refreshes of widgets and save/load, no quest-NPC cards, only unlocked hero cards, active companion's `PersonalCardIds`, route cards, catalog BaseQuality only, exclusion of Epic-owned card IDs, exclusion of owned/duplicate relics, and fallback behavior when a source pool has fewer candidates than slots.

- [x] **Step 2: Run red test**

Run `GameXXK.Route.Merchant.Stock`; expected compile failure for missing merchant types/rules.

- [x] **Step 3: Add serializable merchant state**

Use:

```cpp
UENUM(BlueprintType)
enum class EGameXXKRouteMerchantOfferKind : uint8 { Card, Relic };

USTRUCT(BlueprintType)
struct FGameXXKRouteMerchantOffer
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName OfferId = NAME_None;
    UPROPERTY(SaveGame) EGameXXKRouteMerchantOfferKind Kind = EGameXXKRouteMerchantOfferKind::Card;
    UPROPERTY(SaveGame) FName ContentId = NAME_None;
    UPROPERTY(SaveGame) EGameXXKCardQuality Quality = EGameXXKCardQuality::Common;
    UPROPERTY(SaveGame) int32 Price = 0;
    UPROPERTY(SaveGame) bool bSold = false;
};

USTRUCT(BlueprintType)
struct FGameXXKRouteMerchantState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 SourceNodeId = INDEX_NONE;
    UPROPERTY(SaveGame) int32 StockSeed = 0;
    UPROPERTY(SaveGame) int32 RefreshCount = 0;
    UPROPERTY(SaveGame) TArray<FGameXXKRouteMerchantOffer> Offers;
};
```

- [x] **Step 4: Implement stock generation**

With an active companion, slot sources are hero / active-companion / route. Without one, hero / route / route. Relics are three unique unowned IDs. Card prices are 25/40/60 and relic prices 70/100/140 by quality. Store the generated offers immediately; reopening the screen only reads them.

- [x] **Step 5: Implement refresh**

Refresh cost is `20, 30, 40, 50, 50...`, charged only after validation. One refresh replaces all six offers and increments `RefreshCount`. It may repeat previous cards but never offers owned relics or an already-Epic CardId.

- [ ] **Step 6: Expose subsystem facade and run green tests**

Facade methods: `EnsureRouteMerchantStock`, `GetRouteMerchantSnapshot`, `RefreshRouteMerchant`, and later purchase calls. UI must not mutate runtime arrays directly. Run stock/refresh tests and commit the new rule/type/test files.

---

## Task 7: Implement atomic purchase, merge preview, and replacement

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKRouteMerchantRules.h`
- Modify: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [x] **Step 1: Write failing transaction tests**

Test insufficient money, sold offers, stale offer IDs, duplicate relic rejection, card purchase with immediate merge/no replacement, card purchase at capacity requiring replacement, invalid replacement, valid replacement, exact one-time debit, exact one-time sold marking, and rollback of every field when any validation fails.

- [x] **Step 2: Run red test**

Run `GameXXK.Route.Merchant.Purchase`; expected failures show absent preview/atomic commit.

- [x] **Step 3: Add purchase preview/result types**

The preview must include offer ID, balance before/after, price, merge survivor/consumed IDs/final quality, temporary-count delta, `bRequiresReplacement`, and eligible replacement entry IDs. The commit call must accept offer ID plus optional replacement `EntryId` and return a typed failure reason for UI tooltip/dialog text.

- [x] **Step 4: Implement validate-copy-commit**

Copy `CardRun` to a local candidate. Validate offer and balance; for cards, run `PreviewAdd`, validate replacement if needed, then `AddAndMerge`; for relics, acquire only when unowned. Debit candidate travel money and mark candidate offer sold. Assign the candidate back only after every operation succeeds.

- [x] **Step 5: Run green transaction tests**

Expected: failed transactions produce byte-equivalent merchant/economy/deck/relic state; successful transactions debit once and leave a persistent `已售` offer.

---

## Task 8: Migrate save version 6 and clear route-local state correctly

**Files:**

- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp`

- [ ] **Step 1: Write failing migration/lifecycle tests**

Create explicit version-6 save fixtures containing legacy `RouteCardIds`, duplicate/stacked relics, an active route with no travel-money initialization, and invalid IDs. Assert version-7 output preserves order (maximum 12 temporary cards), uses each card's BaseQuality, collapses relic duplicates to one, initializes active-route travel money once to 60, skips invalid IDs with readable warnings, and preserves permanent PlayerGold.

- [ ] **Step 2: Run red tests**

Run `GameXXK.Save` and `GameXXK.Route.Lifecycle`; expected failures show current save version 6 and missing migration.

- [ ] **Step 3: Implement explicit version-7 migration**

Set `CurrentSaveVersion = 7`. In `RestoreFromSaveState`, migrate before normal validation. Stable migrated entry IDs use `LegacyRoute.<ordinal>.<CardId>`. Do not initialize 60 for a completed/inactive route. Set a route-state version field so repeated loads do not grant money again.

- [ ] **Step 4: Centralize route-local clear**

`ClearRouteLocalCardState` must reset temporary/base route entries, battle/reward/event state, relics, attribute bonuses, travel money, payout receipts, merchant state, route state version, and initialization flags. Invoke it only on route failure, third-chapter final Boss completion, and explicit abandon; first- and second-chapter Boss completion must preserve route-local state, and leaving/reopening the merchant screen must not clear anything.

- [ ] **Step 5: Run green tests**

Expected: version-6 fixtures migrate once, version-7 round-trips exactly, and route end clears only route-local data.

---

## Task 9: Create and import the missing route-merchant visual assets

**Files:**

- Create: `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_TravelMoney.png`
- Create: `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_Portrait.png`
- Create: `SourceArt/UI/RouteMerchant/final/T_RouteMerchant_Paper.png`
- Create: `scripts/test_route_merchant_asset_pipeline.py`
- Create: `Content/Python/gamexxk_import_route_merchant_ui.py`
- Create through import: `Content/GameXXK/UI/RouteMerchant/Textures/*.uasset`

- [ ] **Step 1: Write the failing source/import contract test**

Assert exact dimensions and alpha:

- travel-money icon: 512×512 RGBA, opaque subject covers 65–88% of the square;
- merchant portrait: 1024×1024 RGBA, transparent background, subject covers 55–88%;
- merchant paper: 1536×864 RGBA, transparent outside the irregular paper edge, nine-slice-safe 72 px perimeter;
- no magenta chroma remains;
- importer targets `/Game/GameXXK/UI/RouteMerchant/Textures` and uses UI texture group, transparent compression, no mipmaps.

- [ ] **Step 2: Run the red Python test**

```powershell
python -m pytest scripts/test_route_merchant_asset_pipeline.py -q
```

Expected: failure because the three PNGs/importer are absent.

- [ ] **Step 3: Generate the three assets with the imagegen skill**

Use the approved PSD preview and existing relic/card assets as visual references. Exact prompts:

```text
Travel money icon: square transparent PNG, simplified Chinese ink-wash game UI icon,
one low-saturation warm brass travel coin fused with a tiny mountain-road knot,
single compact silhouette, strong fill inside the square, soft paper-grain brush edge,
no text, no numbers, no glow, no hard black outline, no magenta, transparent background.

Merchant portrait: square transparent PNG, friendly mountain-route peddler in the same
chibi Chinese ink-wash style as the GameXXK PSD, straw rain cape, cloth pack and small
hand scale, low saturation, readable face and hands, compact full-body silhouette,
no scenery, no text, transparent background.

Merchant paper: wide transparent PNG, pale warm handmade xuan paper merchant panel,
irregular dry-brush ink border, faint bottom-corner mountain wash only, empty bright
center for UI content, low contrast and saturation, no symbols, no text, transparent outside.
```

Do not regenerate the existing 30 relic icons, PSD card frame, PSD buttons, or scrollbars.

- [ ] **Step 4: Implement deterministic import**

Follow `gamexxk_import_relic_icons.py`: import the three exact source files, configure UI-friendly texture properties, save only the destination directory, and return a JSON report with source hash, asset path, imported dimensions, alpha, and save result.

- [ ] **Step 5: Run source tests and import through UE MCP**

```powershell
python -m pytest scripts/test_route_merchant_asset_pipeline.py -q
python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=120); c.connect(); print(c.run_project_python_file('Content/Python/gamexxk_import_route_merchant_ui.py'))"
```

Expected: three source tests pass and three destination textures are saved. If the editor is unavailable, defer only the import step; do not bypass it with file copying.

---

## Task 10: Add route HUD and the dedicated merchant widget

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`

- [ ] **Step 1: Write failing widget-structure tests**

Assert:

- route map top-left shows travel money, completed/total node progress, and temporary cards/12;
- refresh cost appears only beside/on the refresh button;
- merchant root is `SelfHitTestInvisible` and interactive paper/buttons are hit-testable;
- fixed 1920×1080 safe-area composition is 23% portrait/info and 77% goods;
- top row has three tall PSD first-row card faces;
- bottom row has three square, high-fill relic faces;
- every item has price and a purchase button below it;
- insufficient money makes the button disabled/gray and tooltip includes the shortfall;
- sold slots display `已售`;
- item hover uses full card/relic tooltip;
- confirm dialog shows current balance, price, balance after purchase, and merge preview;
- capacity path exposes existing reward replacement panel by stable `EntryId`.

- [ ] **Step 2: Run red tests**

Run `GameXXK.UI.RouteMerchant` and route-map adapter tests; expected failures show absent dedicated widget/HUD.

- [ ] **Step 3: Build the route map summary**

Use the new travel-money texture at top left. Keep values bound to snapshots refreshed on state changes, not per-frame Tick. Count completed nodes from `VisitedRouteNodeIds`, total actionable nodes from route map nodes excluding Start, and occupied acquired-card slots exclusively through `FGameXXKRunDeckRules::GetCapacityUsed` / `bConsumesRouteCapacity`. Do not count `bTemporaryRouteCard`: fallback/base entries may be temporary in lifecycle but explicitly consume zero of the twelve acquired-card slots.

- [ ] **Step 4: Build merchant presentation**

Use the imported paper/portrait plus existing `/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057`, first-row card proportions, relic icons, `T_TownPsd_ButtonPrimary`, `T_TownPsd_ButtonNeutral`, and PSD paper scrollbars. Do not stretch buttons vertically; preserve texture aspect and use nine-slice margins on paper/button frames.

- [ ] **Step 5: Remove merchant from the generic encounter panel**

`IsRouteEncounterScreen` for the generic panel handles event/camp only. Delete its placeholder merchant copy/actions. `NativeConstruct` must not immediately collapse an already-active route panel; initialize hidden only when runtime screen does not match.

- [ ] **Step 6: Run green widget tests**

Expected: all structural, tooltip, disabled-state, and layout tests pass at 1920×1080, 1600×900, and 1280×720 viewport geometry without moving outside the safe area.

---

## Task 11: Wire player controller input lifecycle and all quality presentation sites

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKRelicBarWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRelicBarWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Write failing input/quality presentation tests**

Assert that RouteMerchant creates, opens, focuses, and closes the dedicated widget; clickability remains after purchase/confirmation; leaving resolves the node and returns RouteMap; card quality is visible on hand, reward, replacement, companion roster, and merchant cards; relic quality is visible in reward/merchant/bar tooltips; and every displayed effective value matches the runtime quality.

- [ ] **Step 2: Run red tests**

Run `GameXXK.UI.PlayerFlow`, `GameXXK.UI.BattleBoard`, and companion/relic UI filters.

- [ ] **Step 3: Own merchant widget in the player controller**

Add class/member/getter/ensure/open/close methods parallel to the encounter panel. Add it at viewport Z-order 180, refresh it from state, focus it in `ApplyPlayerFlowInputMode`, show mouse cursor, and restore route-map focus when closed. Never call `SetInputMode` from item buttons; the controller owns input mode.

- [ ] **Step 4: Use runtime quality everywhere a card appears**

Hand/replacement cards read `CurrentQuality` from the instance/source entry. Catalog-only reward and roster cards read `BaseQuality`. Merchant cards use offer quality. Display the Chinese quality label in Common white, Rare blue, Epic purple; tooltip detail uses the effective definition. Replacement selection carries `EntryId` so equal CardIds remain distinguishable.

- [ ] **Step 5: Preserve unlimited six-column relic bar**

Do not change wrapping or newest-first ordering. Add quality label/color to tooltip and merchant/reward face only. Relic ownership remains unique.

- [ ] **Step 6: Run green UI/controller tests**

Expected: mouse focus survives a purchase, confirmation dismissal, replacement cancellation, and merchant reopen; all card/relic sites expose the correct quality and tooltip.

---

## Task 12: Full cold verification, PIE acceptance, and documentation handoff

**Files:**

- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Create: `docs/production/2026-07-22-route-merchant-card-quality-acceptance.md`
- Update if required by validator: corresponding `docs/production/*` state file.

- [ ] **Step 1: Extend the real-play probe**

Add read-only snapshots and explicit actions for route money, merchant stock, refresh, purchase, merge preview, replacement, and leave. The harness must verify Start → town → quest → route map → battle → merchant → purchase → map, and separately save/reload inside merchant.

- [ ] **Step 2: Run all targeted automation groups**

After saving/closing through the project pipeline, run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 8 --log-lines 500 --filter "[TDD]"
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -unattended -nop4 -nosplash -nullrhi -ExecCmds="Automation RunTests GameXXK.Data.CardQuality;Automation RunTests GameXXK.Route.RunDeck;Automation RunTests GameXXK.Route.Economy;Automation RunTests GameXXK.Route.Merchant;Automation RunTests GameXXK.UI.RouteMerchant;Automation RunTests GameXXK.Save;Quit" -TestExit="Automation Test Queue Empty" -log
```

Expected: UBT says `Result: Succeeded`, no Live Coding/Hot Reload was used, and every listed automation group reports zero failures.

- [ ] **Step 3: Run regression automation**

Run existing card catalog/rules/text, route reward/event/relic, player-flow, battle-board, companion roster, MVP flow, and inventory/town merchant tests. Expected: town store still spends `PlayerGold`; route store spends only `RouteTravelMoney`.

- [ ] **Step 4: Run PIE acceptance through UE MCP**

Verify visibly and via probe:

1. Route starts with 60 travel money at top left.
2. A completed normal battle pays exactly 20 once.
3. Merchant shows stable 3-card/3-relic stock with prices/buttons.
4. Insufficient buttons are gray; tooltip states the shortfall.
5. Buying a duplicate Common card previews and performs Common → Rare; four copies chain to Epic.
6. At 12 temporary cards, a merging purchase skips replacement and a non-merging purchase opens replacement.
7. Active companion produces exactly one companion-source offer; task NPC produces none.
8. Relics exclude owned IDs, remain unique, and appear in the right-top unlimited six-column bar.
9. Refresh charges 20/30/40/50/50 and replaces all six offers.
10. Save/reload preserves stock, sold state, money, qualities, and receipts.
11. Leaving the merchant returns to the route map with working mouse input.
12. Route end clears route-local money/cards/qualities/relics/merchant but preserves permanent gold and collections.

- [ ] **Step 5: Record acceptance evidence**

Document exact build/test commands, result counts, PIE screenshot paths under `Saved/Screenshots`, migrated save fixture results, known non-blocking warnings, and any deferred visual polish. Do not mark complete if clicks, stock persistence, scaling, or save migration lack direct evidence.

- [ ] **Step 6: Final diff and commit gate**

```powershell
git diff --check
python scripts/harness_state_validator.py
git status --short
```

Review every touched pre-existing dirty file to ensure no user-tuned asset/code hunk was overwritten. Stage only feature-owned new files and isolated feature hunks. Commit message:

```text
feat: add route merchant card quality economy
```

---

## Completion checklist

- [ ] 174 cards classify as 92 Common / 51 Rare / 31 Epic, with exact appendix-A IDs.
- [ ] 30 relics classify as 15 Common / 10 Rare / 5 Epic, with no route duplicates.
- [ ] Effective values use the approved multiply/add rules in rules, previews, and tooltips.
- [ ] Pair merging is deterministic, chainable, capped at Epic, and stable across save/load.
- [ ] Route money is independent from PlayerGold, receipt-protected, and reset on route end.
- [ ] Merchant stock is 3 cards + 3 relics, deterministic, saved, companion-aware, NPC-excluding, and refreshable.
- [ ] Purchases are atomic, show confirmation/merge preview, and use stable entry replacement at capacity.
- [ ] Route HUD and merchant HUD match the PSD proportions and remain clickable at supported aspect ratios.
- [ ] All card/relic display sites show quality and hover tooltips.
- [ ] Version-6 saves migrate to version 7 without duplicate grants or crashes.
- [ ] Cold build, targeted automation, regression automation, and PIE real-play flow all pass.
