# Route Merchant Implementation Plan

> **执行状态（2026-07-24）：** Task 1（商店存档词汇与迁移）已完成并通过冷编译、自动化测试及两阶段复审。本文 Task 2–5 不再作为后续执行权威，因为原 Task 2 曾错误地建议把 `Precious` 塞入旧的来源枚举 `EGameXXKCardRarity`。后续必须改按 [`2026-07-22-route-merchant-card-quality-upgrade.md`](./2026-07-22-route-merchant-card-quality-upgrade.md) 从 Task 1 起执行：保留旧 `Rarity`，新增独立 `EGameXXKCardQuality`，再建立 `FGameXXKRouteCardEntry` 有序运行配方。已完成的商店 Task 1 类型作为完整计划 Task 6 的现有基础保留，不重复创建。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a save-authoritative route merchant that sells three route cards and three run relics with run-currency pricing, correct card-capacity handling, real tooltips, and a safe return to the existing random route map.

**Architecture:** Keep all merchant inventory and purchase state in `FGameXXKCardRunState`; `FGameXXKRouteMerchantRules` is the only code that may deduct `RouteTravelMoney` or mark an offer sold. Refactor route-card acquisition into a reusable transactional adapter operation so post-battle rewards and merchant purchases share card-capacity, duplicate/merge, and replacement behavior. Present the data through a dedicated `UGameXXKRouteMerchantWidget`, selected by the player controller only for `RouteMerchant`; keep the event/chest/camp panel unchanged.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate programmatic widgets, UHT `USTRUCT` SaveGame serialization, UE Automation Tests, UE MCP real PIE harness, UBT `-NoHotReload` cold build.

---

## File structure

- Create: `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h` — persisted offer, shop snapshot and pending card-purchase vocabulary.
- Create: `Source/GameXXK/Public/GameXXKRouteMerchantRules.h` / `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp` — deterministic offer construction and atomic purchase operations.
- Create: `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h` / `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp` — six-slot paper/ink merchant view and test read-model seams.
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp` / `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`.
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`, `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`, `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`.
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`, `Source/GameXXK/Private/GameXXKMVPRules.cpp`, `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`, `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`.
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`, `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`, `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`.
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`, `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`, `scripts/gamexxk_real_play_flow_mcp.py`.
- Create: `docs/verification/2026-07-24-route-merchant-real-pie.md`.

## Task 1: Persist merchant state and safely migrate old saves

**Files:**
- Create: `Source/GameXXK/Public/GameXXKRouteMerchantTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [ ] **Step 1: Write the failing default/migration test**

~~~
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKRouteMerchantSaveStateTest,
    "GameXXK.Route.Merchant.SaveState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSaveStateTest::RunTest(const FString&)
{
    FGameXXKRuntimeState State = UGameXXKMVPRules::CreateInitialState();
    TestTrue(TEXT("new run starts empty"), State.CardRun.RouteMerchant.Offers.IsEmpty());
    TestFalse(TEXT("new run has no pending merchant purchase"), State.CardRun.RouteMerchant.PendingPurchase.bActive);

    FGameXXKSaveState Legacy = UGameXXKMVPRules::MakeSaveState(State);
    Legacy.Version = UGameXXKMVPRules::GetCurrentSaveVersion() - 1;
    FGameXXKSaveState Migrated;
    FGameXXKSaveMigrationReport Report;
    TestTrue(TEXT("old save migrates"), FGameXXKSaveMigration::MigrateToCurrent(Legacy, Migrated, Report));
    TestTrue(TEXT("migration does not invent offers"), Migrated.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty());
    return true;
}
~~~

- [ ] **Step 2: Run it red**

Run: `python scripts/ue_mcp_client.py automation run GameXXK.Route.Merchant.SaveState`

Expected: test fails to compile/discover because no merchant state exists.

- [ ] **Step 3: Add serializable merchant-only types**

Add exact `SaveGame` fields: offer ID, kind (`Card` or `Relic`), content ID, positive price, sold flag; shop source node ID, deterministic offer seed, offers array, and pending card purchase (active flag, offer ID, card ID, price). Add `FGameXXKRouteMerchantState RouteMerchant;` to `FGameXXKCardRunState`. Do not store localized price text, widget state, `PlayerGold`, permanent inventory IDs, or an actor reference.

In the same public header, define the non-saving read/result vocabulary used by every consumer: `FGameXXKRouteMerchantOfferView` contains one saved offer plus `bAffordable`, `bPurchaseEnabled`, and a stable disabled-reason string; `FGameXXKRouteMerchantView` contains `RouteTravelMoney`, `CardOffers`, `RelicOffers`, `bHasPendingReplacement`, and `bCanLeave`; `FGameXXKRouteMerchantPurchaseResult` contains `bPurchased`, `bRequiresReplacement`, `OfferId`, and `CardId`. The UI receives only these views/results rather than mutable runtime references.

- [ ] **Step 4: Add the migration/validation rule**

An old or absent merchant state becomes exactly the empty default. A populated saved state is accepted only when it has one valid source node, six unique offer IDs, exactly three card offers and three relic offers, non-negative prices, and a pending purchase that references an unsold card offer. Invalid populated data produces the project’s stable load/migration error; no silent reroll occurs.

- [ ] **Step 5: Run migration tests green**

Run:
~~~
python scripts/ue_mcp_client.py automation run GameXXK.Equipment.SaveMigration
python scripts/ue_mcp_client.py automation run GameXXK.Route.Merchant.SaveState
~~~

Expected: migration regressions stay green; a populated snapshot round-trips exactly.

- [ ] **Step 6: Commit the persistence slice**

~~~
git add Source/GameXXK/Public/GameXXKRouteMerchantTypes.h Source/GameXXK/Public/GameXXKCardRunTypes.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp
git commit -m "feat: persist route merchant offers"
~~~

## Task 2: Extract source-neutral route-card acquisition

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing direct, merge, and replacement tests**

~~~
FGameXXKRouteCardAcquisitionPreview Preview;
TestTrue(TEXT("preview succeeds"),
    FGameXXKCardBattleAdapter::PreviewRouteCardAcquisition(State, OfferedCardId, NAME_None, Preview, &Error));
TestTrue(TEXT("equal-quality pair merges without replacement"), Preview.bMergesExistingCopies && !Preview.bRequiresReplacement);
TestTrue(TEXT("commit succeeds"),
    FGameXXKCardBattleAdapter::CommitRouteCardAcquisition(State, Preview, NAME_None, &Error));
~~~

Use one fixture with two same-card/same-quality copies at capacity. Assert a Common pair becomes one Rare card, a Rare pair becomes one Precious card, and a Precious pair cannot grow past the quality cap. Verify attack/defense/heal use the next-quality doubled definition while draw, mana, and status-stack values use the frozen additive definition.

- [ ] **Step 2: Run it red**

Run: `python scripts/ue_mcp_client.py automation run GameXXK.CardBattleAdapter.RouteCardAcquisition`

Expected: the source-neutral preview/commit API does not exist.

- [ ] **Step 3: Add the shared preview and commit methods**

First add `Precious = 5` to `EGameXXKCardRarity` without changing the existing numeric values of `Permanent`, `Common`, `Rare`, or `Boss`, and make the route-card catalog expose deterministic next-quality definitions. Define `FGameXXKRouteCardAcquisitionPreview` in `GameXXKCardTypes.h` with `OfferedCardId`, `FinalCardId`, `ConsumedRouteCardIds`, `bMergesExistingCopies`, `bRequiresReplacement`, and `bIncreasesRouteCardCount`. `Boss` and `Permanent` cards are not legal merge inputs.

Declare:

~~~
static bool PreviewRouteCardAcquisition(
    const FGameXXKRuntimeState& State,
    FName OfferedCardId,
    FName ReplacedRouteCardId,
    FGameXXKRouteCardAcquisitionPreview& OutPreview,
    FString* OutError = nullptr);

static bool CommitRouteCardAcquisition(
    FGameXXKRuntimeState& InOutState,
    const FGameXXKRouteCardAcquisitionPreview& Preview,
    FName ReplacedRouteCardId,
    FString* OutError = nullptr);
~~~

The preview is pure and includes offered/final card IDs, source copies consumed, `bMergesExistingCopies`, `bRequiresReplacement`, and `bIncreasesRouteCardCount`. The commit revalidates IDs and capacity against the current state, then mutates only a candidate state.

- [ ] **Step 4: Refactor post-battle reward to call the same operation**

`ChoosePendingRouteReward` validates its saved offer, then calls preview/commit and sets `bActiveBattleRewardResolved` only after commit. Merchant acquisition uses preview/commit but never touches that battle-only flag. Remove the old third-copy rejection path that conflicts with the locked pair-merge rule.

- [ ] **Step 5: Run all route-card regressions green**

Run:
~~~
python scripts/ue_mcp_client.py automation run GameXXK.CardBattleAdapter
python scripts/ue_mcp_client.py automation run GameXXK.MVP.RouteMap
~~~

Expected: existing battle reward behavior remains green plus all three merge/cap cases pass.

- [ ] **Step 6: Commit**

~~~
git add Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git commit -m "feat: unify route card acquisition"
~~~

## Task 3: Implement deterministic merchant rules and atomic purchases

**Files:**
- Create: `Source/GameXXK/Public/GameXXKRouteMerchantRules.h`
- Create: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`

- [ ] **Step 1: Write the failing transaction tests**

~~~
TestTrue(TEXT("merchant opens"), FGameXXKRouteMerchantRules::Open(State, NodeId, View, &Error));
TestEqual(TEXT("three card offers"), View.CardOffers.Num(), 3);
TestEqual(TEXT("three relic offers"), View.RelicOffers.Num(), 3);
TestTrue(TEXT("relic buy succeeds"),
    FGameXXKRouteMerchantRules::PurchaseOffer(State, RelicOfferId, NAME_None, Result, &Error));
TestEqual(TEXT("permanent gold unchanged"), State.PlayerGold, GoldBefore);
TestEqual(TEXT("travel money charged once"), State.CardRun.RouteTravelMoney, MoneyBefore - RelicPrice);
~~~

Also assert deterministic reopening, different-node isolation, insufficient money, sold offer, wrong screen, direct card buy, merge buy, at-cap pending replacement, cancellation, replacement completion, blocked leave, terminal cleanup, and exact save/load behavior.

- [ ] **Step 2: Run rule tests red**

Run: `python scripts/ue_mcp_client.py automation run GameXXK.Route.Merchant`

Expected: merchant rules are absent.

- [ ] **Step 3: Implement deterministic offer opening**

Expose:

~~~
static bool Open(FGameXXKRuntimeState& InOutState, int32 SourceNodeId, FGameXXKRouteMerchantView& OutView, FString* OutError = nullptr);
static bool GetView(const FGameXXKRuntimeState& State, FGameXXKRouteMerchantView& OutView, FString* OutError = nullptr);
~~~

`Open` requires `RouteMerchant` screen and the pending Merchant node. First open derives a non-zero seed from `RouteRandomSeed`, `SourceNodeId` and a fixed merchant salt, produces exactly three legal cards and three unique relics, and saves those six offers. Later opens validate/reuse the saved snapshot. Card selection uses public/general cards, hero cards, and only the currently active permanent companion instance's `PersonalCardIds[12]`—never its entire profession pool, a task NPC, or an inactive companion.

- [ ] **Step 4: Implement copy-then-commit purchases**

Expose:

~~~
static bool PurchaseOffer(
    FGameXXKRuntimeState& InOutState,
    FName OfferId,
    FName ReplacedRouteCardId,
    FGameXXKRouteMerchantPurchaseResult& OutResult,
    FString* OutError = nullptr);
static bool CancelPendingPurchase(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
~~~

Copy the input state. Validate screen, source node, offer, unsold state and money before mutation. Relics call `FGameXXKRelicRules::AcquireRelic`; cards call Task 2 preview/commit. If a replacement is needed but absent, write only `PendingPurchase` and return `bRequiresReplacement=true`; do not charge or sell. Otherwise deduct exactly the offer price, mark exactly that offer sold, clear pending purchase, and move the candidate back only on success.

- [ ] **Step 5: Bind cleanup/leave to existing route authority**

`ResolveMerchantRouteNode` rejects an active pending purchase, completes the node only after a real leave, and never changes `PlayerGold`. `ClearRouteLocalCardState` resets merchant state; terminal settlement, defeat and abandon use that existing clear path.

- [ ] **Step 6: Run focused rule regressions green**

Run:
~~~
python scripts/ue_mcp_client.py automation run GameXXK.Route.Merchant
python scripts/ue_mcp_client.py automation run GameXXK.Route.Settlement
python scripts/ue_mcp_client.py automation run GameXXK.MVP.RouteMap.SeedRules
~~~

Expected: merchant authority passes and existing route, settlement, chest, and relic behavior remains green.

- [ ] **Step 7: Commit**

~~~
git add Source/GameXXK/Public/GameXXKRouteMerchantRules.h Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp
git commit -m "feat: add route merchant transactions"
~~~

## Task 4: Add dedicated merchant HUD and controller routing

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`

- [ ] **Step 1: Write failing UI/controller tests**

~~~
PlayerController->RefreshPlayerFlowWidgetsForTest();
TestTrue(TEXT("merchant opens dedicated HUD"), PlayerController->IsRouteMerchantWidgetOpenForTest());
TestFalse(TEXT("merchant does not open encounter HUD"), PlayerController->IsRouteEncounterPanelOpenForTest());
TestEqual(TEXT("three visible card slots"), MerchantWidget->GetCardOfferCountForTest(), 3);
TestEqual(TEXT("three visible relic slots"), MerchantWidget->GetRelicOfferCountForTest(), 3);
TestFalse(TEXT("insufficient money disables purchase"), MerchantWidget->IsOfferPurchaseEnabledForTest(OfferId));
TestFalse(TEXT("card tooltip exists"), MerchantWidget->GetOfferTooltipForTest(CardOfferId).IsEmpty());
~~~

- [ ] **Step 2: Run it red**

Run: `python scripts/ue_mcp_client.py automation run GameXXK.MVP.RouteMerchant.Widget`

Expected: current Merchant uses the leave-only encounter panel.

- [ ] **Step 3: Implement a six-offer paper/ink widget**

Build three card and three relic buttons. Each has art, title, direct price, buy button and hover Tooltip; card slots use the existing card frame and rarity text. Disable buttons for insufficient balance, sold state, invalid data or an active replacement; each disabled state has a reason Tooltip. Do not create a new generic rectangular UI system or alter protected PSD assets.

- [ ] **Step 4: Add narrow controller actions**

Expose:

~~~
bool OpenRouteMerchant();
bool PurchaseRouteMerchantOffer(FName OfferId, FName ReplacedRouteCardId = NAME_None);
bool CancelRouteMerchantPurchase();
bool LeaveRouteMerchant();
~~~

Each delegates to the subsystem/rules then refreshes the read model and input mode. `RefreshPlayerFlowWidgets` selects this widget only for `RouteMerchant`, closes the encounter panel first, and returns to the route map only after `LeaveRouteMerchant` succeeds.

- [ ] **Step 5: Connect existing real card replacement presentation**

A card purchase that returns `bRequiresReplacement` opens the existing route-card replacement UI for that merchant offer. Selecting a route card calls the same purchase action with the replacement ID; cancel calls `CancelRouteMerchantPurchase`. No widget is allowed to subtract money or set sold state directly.

- [ ] **Step 6: Run UI, tooltip and route-panel regressions green**

Run:
~~~
python scripts/ue_mcp_client.py automation run GameXXK.MVP.RouteMerchant.Widget
python scripts/ue_mcp_client.py automation run GameXXK.MVP.RouteEncounter
python scripts/ue_mcp_client.py automation run GameXXK.MVP.PlayableShell
~~~

Expected: Merchant uses dedicated HUD, events/chest/camp still use their panel, and every card/relic offer has a real Tooltip.

- [ ] **Step 7: Commit**

~~~
git add Source/GameXXK/Public/UI/GameXXKRouteMerchantWidget.h Source/GameXXK/Private/UI/GameXXKRouteMerchantWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp
git commit -m "feat: show route merchant HUD"
~~~

## Task 5: Real PIE, asset binding and cold verification

**Files:**
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify after import evidence exists: `docs/production/2026-07-22-image-asset-production-manifest.md`
- Create: `docs/verification/2026-07-24-route-merchant-real-pie.md`

- [ ] **Step 1: Add public-flow merchant evidence**

The harness must reach a Merchant node via normal route selection, then capture a read model containing `route_travel_money`, six offer IDs, prices, button state and non-empty card/relic Tooltip text. It must not write private state or use a widget-only fake success seam.

- [ ] **Step 2: Prove four real interaction paths**

~~~
1. enough money: buy a relic; prove the relic bar changes;
2. insufficient money: button is disabled; money and offer stay unchanged;
3. full route deck: buy a card, choose a replacement, prove one charge and sold offer;
4. leave: prove original Merchant node is visited and the same generated map is visible.
~~~

A click that only closes a panel is not merchant proof.

- [ ] **Step 3: Bind B01 art only after real import evidence**

Record source SHA-256, imported package paths and UE asset validation for `ROUTE.MERCHANT.TRAVEL_MONEY`, `ROUTE.MERCHANT.PORTRAIT`, and `ROUTE.MERCHANT.PAPER`. Then bind the verified paths. Keep their approval status as initial draft until individual user art review; never call it final art.

- [ ] **Step 4: Save, stop PIE, cold-build and run final checks**

Run:
~~~
$env:TEMP='D:\GameXXKBuildTemp'; $env:TMP='D:\GameXXKBuildTemp'
python scripts\ue_tdd_pipeline.py --pie-duration 3 --filter "GameXXK.Route.Merchant"
python scripts\gamexxk_real_play_flow_mcp.py --timeout 90 --report D:\GameXXKBuildTemp\route-merchant-real-flow.json
git diff --check
git status --short
~~~

Expected: UBT reports `Result: Succeeded` without Live Coding/Hot Reload; merchant proof succeeds; C drive is checked before the build and stays above the user’s 1GB cleanup threshold. Document only merchant evidence, not overall-goal completion.
