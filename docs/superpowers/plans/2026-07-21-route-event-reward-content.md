# Route Event and Reward Content Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fill route event and post-battle reward scenes with playable, PSD-consistent content that persists choices and always returns to the route map correctly.

**Architecture:** Keep route consequences authoritative in `FGameXXKCardRunState` and `UGameXXKMVPSubsystem`; UMG widgets only render an explicit presentation model and invoke existing controller actions. Events use a compact data table-like native catalog keyed by node/NPC, while rewards reuse the existing card instance and route-card replacement rules rather than creating a parallel deck system.

**Tech Stack:** Unreal Engine 5.8, C++ UMG, existing `GameXXKMVPRules`, `GameXXKCardBattleAdapter`, route save state, UE MCP/Python fixture scripts, UE automation tests, PSD-derived UI assets.

---

## Scope and content contract

| Route content | Player-facing scene | Choice result | Persistence |
|---|---|---|---|
| 牛欢事件 | 牛欢立绘、两段叙事、两项明确报酬 | 12 金或疗伤散 | 节点完成；不成为伙伴 |
| 任务 NPC 奇遇 | 对应 NPC 原画、职业/固定 12 张卡预览、同行与婉拒 | 临时支援或疗伤散 | `ActiveTemporaryQuestNpcId` 仅本路线有效 |
| 江湖补给事件 | 补给场景、全队状态摘要 | 全队恢复或少量金钱/普通路线卡 | 节点完成；无风险 |
| 宝箱 | 宝匣插画、三件物品横排 | 金钱、疗伤散、普通路线卡三选一 | 节点完成 |
| 营火 | 营火场景、全队状态摘要 | 满血休整或疗伤散 | 节点完成 |
| 行商 | 行商/货架插画、余额与商品 | 购买一项或离开 | 金钱、物品、节点完成 |
| 战斗胜利 | 敌方战利品、三张奖励卡、路线牌替换栏 | 选 1、替换（若满 30）或跳过 | `RouteCardIds`、节点完成后回路线 |
| Boss 胜利 | 老虎战利品、稀有三选一、结算横幅 | 选 1/替换/跳过 | 路线完成，清理临时 NPC |

### 本轮路线节点锁定

- `？` 节点只从牛欢、任务 NPC、江湖补给三类事件中按路线种子抽取；全部为正向结果，不扣血、不减金、不放负面状态、不强制进入战斗。
- 宝箱节点始终展示三件横排物品：金钱、疗伤散、普通路线卡；玩家只能领取其中一件，领取后才完成节点并返回路线。
- 战斗三选一卡继续只属于战斗/Boss 胜利。宝箱若送普通路线卡且路线牌已满，则复用同一条路线牌替换规则。
- `？` 与宝箱均为路线图上方的纯 HUD 弹层，不加载独立 3D 事件关卡或新场景：路线图保持可见但墨色压暗，左侧放事件角色/宝箱立绘，右侧放标题、简短文案、收益预览，底部横排放选择。

### Task 1: Establish content records and rule coverage

**Files:**
- Create: `Source/GameXXK/Public/GameXXKRouteEncounterContent.h`
- Create: `Source/GameXXK/Private/GameXXKRouteEncounterContent.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterRulesTest.cpp`

- [ ] **Step 1: Write failing rule tests for each supported event outcome.**

```cpp
TestTrue(TEXT("Niu Huan produces exactly gold and healing choices"),
    FGameXXKRouteEncounterContent::Find(TEXT("Npc.Event.NiuHuan")).Choices.Num() == 2);
TestFalse(TEXT("Niu Huan is never recruitable"),
    FGameXXKRouteEncounterContent::Find(TEXT("Npc.Event.NiuHuan")).bOffersPermanentRecruitment);
TestTrue(TEXT("task NPC support is route-temporary"),
    FGameXXKRouteEncounterContent::Find(TEXT("Npc.TusiChief")).bOffersTemporarySupport);
```

- [ ] **Step 2: Run the focused test and verify it fails because the catalog does not exist.**

Run: `python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 80`, then run `Automation RunTests GameXXK.Route.EncounterRules` through UE MCP.

Expected: FAIL with missing `FGameXXKRouteEncounterContent` behavior.

- [ ] **Step 3: Add a native catalog with explicit text, portrait path, node applicability, and actions.**

```cpp
struct FGameXXKRouteEncounterContent
{
    FName Id;
    FText Title;
    FText Speaker;
    FText Body;
    FString PortraitPath;
    bool bOffersTemporarySupport = false;
    bool bOffersPermanentRecruitment = false;
    TArray<FGameXXKRouteEncounterChoice> Choices;
    static const FGameXXKRouteEncounterContent& Find(FName Id);
};
```

Define records for `Npc.Event.NiuHuan`, all six task NPCs, Chest, Camp, Merchant, and BossClear. Do not add a permanent-recruit option to any event record.

- [ ] **Step 4: Store the selected encounter record ID in route-local pending state.**

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FName EncounterContentId = NAME_None;
```

Set it on node entry, clear it only after a successful action commits the node, and retain it across manual save/load.

- [ ] **Step 5: Re-run the focused rule test and save-state regression tests.**

Run: `Automation RunTests GameXXK.Route.EncounterRules+GameXXK.SaveGame`.

Expected: PASS; event ID and task-NPC route provenance survive save/load.

### Task 2: Build a real event scene instead of a text-only modal

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`
- Create: `Content/Python/gamexxk_import_route_encounter_ui_assets.py`
- Create: `Content/Python/gamexxk_validate_route_encounter_ui_assets.py`

- [ ] **Step 1: Write a failing widget test for portrait, choice metadata, and disabled support state.**

```cpp
TestTrue(TEXT("event panel exposes the selected encounter portrait"),
    Panel->GetPortraitResourcePathForTest().Contains(TEXT("NiuHuan")));
TestEqual(TEXT("occupied support slot disables only invite"),
    Panel->GetPrimaryActionForTest(), EGameXXKRouteEncounterAction::AcceptTaskNpcSupport);
TestTrue(TEXT("alternative reward remains enabled"), Panel->IsSecondaryActionEnabledForTest());
```

- [ ] **Step 2: Verify the test fails.**

Run: `Automation RunTests GameXXK.UI.RouteEncounterPanel`.

Expected: FAIL because the panel has no portrait/status summary API.

- [ ] **Step 3: Add the route-map HUD hierarchy.**

Use the existing backpack window/header/action textures for the frame. Keep the route map loaded behind a full-screen ink-wash dimmer; add a centered paper window with a 300×300 left portrait panel, right title/speaker/body, one compact reward/result row under the text, horizontal choices at the bottom, and a small “路线第 N 节点” footer. Do not load an event level or generate a separate scene backdrop. Use `UImage` portrait texture paths, not generated placeholder text.

- [ ] **Step 4: Render content-specific details.**

For task NPC events, show temporary status, role, and the NPC’s fixed 12-card deck summary; for 牛欢 show “事件 NPC · 不可招募”; for Chest/Camp/Merchant show the actual gold/healing/item delta before the button. Tooltip every card-like preview on hover.

- [ ] **Step 5: Import only missing portraits.**

The import script must first look for original character art in existing project assets. Only when an NPC or chest portrait is absent may it generate/import one PSD-consistent cutout under `/Game/GameXXK/UI/RouteEncounters/`; do not create a scene background and do not overwrite user-tuned characters or maps. The validator must report every path and texture size.

- [ ] **Step 6: Run widget and asset validation.**

Run: `Automation RunTests GameXXK.UI.RouteEncounterPanel`; run `Content/Python/gamexxk_validate_route_encounter_ui_assets.py` via UE MCP.

Expected: PASS; all event types render a portrait and two actionable choices.

### Task 3: Make post-battle rewards a dedicated complete scene

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteRewardChoiceTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing tests for reward scene visibility and terminal actions.**

```cpp
TestTrue(TEXT("victory opens a three-card reward scene before route completion"),
    Board->IsRouteRewardSceneVisibleForTest());
TestEqual(TEXT("reward scene presents three deterministic cards"),
    Board->GetVisibleRewardCardCountForTest(), 3);
TestFalse(TEXT("route node cannot complete before choose or skip"),
    Rules::CanCompleteRouteNode(State));
```

- [ ] **Step 2: Verify the tests fail against the current in-battle pending panel.**

Run: `Automation RunTests GameXXK.Card.RouteRewardChoice+GameXXK.UI.Battle.BoardWidget`.

Expected: FAIL because reward UI is not a complete victory scene.

- [ ] **Step 3: Implement a reward-scene state projection.**

When `PendingReward.CardIds` exists, render a centered paper panel over the cleared battle field: enemy/boss trophy at top, “胜利收获” heading, three full-size clickable cards, a skip action, and a route-card replacement scroll list only when `bRequiresRouteCardReplacement` is true.

- [ ] **Step 4: Keep card semantics in the adapter.**

```cpp
bool FGameXXKCardBattleAdapter::ChooseRouteReward(
    FGameXXKRuntimeState& State, FName OfferedCardId, FName ReplaceRouteCardId, FString* OutError);
bool FGameXXKCardBattleAdapter::SkipRouteReward(FGameXXKRuntimeState& State, FString* OutError);
```

The widget sends IDs only. It must never mutate `RouteCardIds` itself. Choosing or skipping clears `PendingReward`, marks `bActiveBattleRewardResolved`, and only then enables route continuation.

- [ ] **Step 5: Add tooltip and focus behavior to all reward cards.**

Reuse the existing card tooltip builder. Hover raises one reward card, shows its cost/target/effect/owner; click selects it; if replacement is needed, list route-local cards in a visible paper-track/ink-thumb scroll box before confirmation.

- [ ] **Step 6: Re-run reward tests.**

Run: `Automation RunTests GameXXK.Card.RouteRewardChoice+GameXXK.UI.Battle.BoardWidget`.

Expected: PASS for select, skip, replacement, save/load pending reward, and boss reward tier.

### Task 4: Connect scene transitions and route cleanup

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterSceneActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPFlowTest.cpp`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write a failing playable-flow test.**

```cpp
TestTrue(TEXT("event action resolves node then returns to route"), ExecuteEventGoldPath());
TestTrue(TEXT("battle victory requires reward resolution"), ExecuteVictoryThenRewardPath());
TestTrue(TEXT("route finish removes temporary task NPC"), FinishRouteAndAssertNoTaskNpc());
```

- [ ] **Step 2: Verify it fails at the current empty scene/return boundary.**

Run: `Automation RunTests GameXXK.MVP.Flow+GameXXK.Route.EncounterSceneActor`.

- [ ] **Step 3: Route every successful action through one completion method.**

```cpp
bool UGameXXKMVPSubsystem::CommitRouteNodeResolution(
    int32 NodeId, EGameXXKRouteResolutionKind ResolutionKind, FString* OutError);
```

It validates the pending node, applies reward/event result, clears pending scene state, returns to `RouteMap`, persists the save, and removes temporary NPC support when the route ends. Merchant “暂不离开” is explicitly non-terminal and only closes the modal.

- [ ] **Step 4: Extend the MCP playable-flow harness.**

Add deterministic commands: `--route-node event`, `--event-choice gold|heal|support`, `--battle-win`, `--reward choose|skip`, `--assert-route-return`. Each command must return JSON including screen, node state, reward state, route cards, gold, and temporary NPC ID.

- [ ] **Step 5: Run flow tests and a real PIE path.**

Run: `python scripts/gamexxk_real_play_flow_mcp.py --scenario event_to_route`; then `--scenario battle_reward_to_route`.

Expected: both return `ok: true`; no node completes before explicit choice; temporary NPC has no permanent roster entry after route completion.

### Task 5: Polish, visual acceptance, and handoff

**Files:**
- Modify: `docs/production/2026-07-16-party-deck-full-optimization-goal.md`
- Create: `docs/production/2026-07-21-route-event-reward-acceptance.md`
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`

- [ ] **Step 1: Add a failing acceptance probe for panel content.**

```python
assert result["event"]["portrait_path"]
assert result["event"]["choice_count"] == 2
assert result["reward"]["visible_card_count"] == 3
assert result["reward"]["tooltip_available"] is True
```

- [ ] **Step 2: Verify the probe fails before the completed fixtures are wired.**

Run: `python scripts/gamexxk_real_play_flow_mcp.py --scenario ui_acceptance`.

- [ ] **Step 3: Verify at 16:9, 16:10, and resized editor PIE.**

Check: portrait never overlaps choices; three reward cards stay legible; replacement list exposes a visible scroll thumb; all card-like UI has hover tooltip; no old bare text button remains in event/reward scenes.

- [ ] **Step 4: Perform final cold verification.**

Run: `python scripts/ue_tdd_pipeline.py --pie-duration 4 --log-lines 160`.

Expected: cold build succeeds, focused automation suites pass, and PIE opens without Live Coding/Hot Reload.

- [ ] **Step 5: Record acceptance evidence without committing unrelated dirty worktree changes.**

Write test commands, outputs, asset paths, and screenshots to `docs/production/2026-07-21-route-event-reward-acceptance.md`. Stage/commit only explicitly approved files after inspecting `git diff`; this repository currently contains unrelated user work and must not be bulk-staged.

## Self-review

- Coverage: Tasks 1–2 fill every event node; Task 3 fills combat/Boss rewards; Task 4 guarantees state/save/route continuity; Task 5 checks visual and cold-build acceptance.
- No permanent recruitment is introduced for 牛欢 or task NPCs.
- No user-tuned maps, camera, sprites, PaperZD assets, or HD2D plane settings are touched.
- Reward choice uses existing card instance IDs and route-card cap rules; no duplicate deck implementation is planned.
