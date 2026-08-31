# Life-Saving Charm and Workbench Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Camp healing-powder flow with a one-use emergency-heal relic or 100 route money, eliminate white Travel character blocks, and separate the Backpack Tab from its paper-local close button.

**Architecture:** Extend the existing relic trigger system with a consumable percentage-heal effect, keep Camp settlement transactional, resolve compact Travel atlases through a 1K-to-2K fallback pair, and route all Backpack close inputs through the existing global close action.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, SaveGame USTRUCTs, UE Automation Tests, UE MCP asset import and PIE.

---

## Source specification

`docs/superpowers/specs/2026-08-23-life-saving-charm-and-workbench-polish-design.md`

### Task 1: import the approved relic art and catalog definition

**Files:**
- Import: `SourceArt/UI/Relics/final/T_Relic_LifeSavingTalisman_v1.png`
- Modify: `Source/GameXXK/Private/GameXXKRelicCatalog.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp`

- [ ] **Step 1: Verify source art deterministically**

Assert 512x512, 32-bit alpha, transparent pixels, and the approved SHA256.

- [ ] **Step 2: Import through UE MCP**

Import as:

```text
/Game/GameXXK/UI/Relics/Icons/T_Relic_LifeSavingTalisman
```

Use UI texture settings consistent with the existing relic icons and save the package through MCP.

- [ ] **Step 3: Add a catalog RED test and definition**

Add a test for stable ID, Chinese display name, DamageTaken trigger, 30-percent magnitude, unique/non-stackable behavior, and exact icon path.

Append the effect enum; never renumber existing serialized enum values.

- [ ] **Step 4: Run relic catalog tests and commit**

```powershell
git commit -m "feat: add life-saving talisman relic"
```

### Task 2: RED/GREEN — emergency heal and one-use consumption

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKRelicTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKRelicRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp`
- Modify: relevant card battle rule/adapter tests.

- [ ] **Step 1: Write failing trigger tests**

Cover:

```cpp
TestFalse(TEXT("exactly fifty percent does not trigger"), TriggerAtRatio(50, 100));
TestTrue(TEXT("forty-nine percent triggers"), TriggerAtRatio(49, 100));
TestEqual(TEXT("lethal protected damage stops at one hp"), ProtectedHealthAfterLethalDamage(), 1);
```

Assert all currently living party members receive `CeilToInt(MaxHP * 0.30)`, capped at MaxHP; the protected unit never enters the dead state; the relic is removed exactly once; and a later lethal packet follows the unchanged death logic. Add explicit no-relic compatibility coverage for HP, terminal phases, and defeated-owner card cleanup.

- [ ] **Step 2: Implement pre-death one-HP protection**

At the central party health-loss boundary, arm the battle from the unique relic and check the packet's predicted target health with integer arithmetic:

```cpp
const bool bBelowHalf = static_cast<int64>(PredictedHP) * 100
	< static_cast<int64>(Unit.MaxHP) * 50;
```

If the packet triggers the charm, clamp only the protected target to at least `1 HP` before existing death logic runs, heal every currently living party unit, append healing audit results when available, disarm and remove the relic, synchronize legacy projection, and commit only after all checks succeed. Do not alter death logic or restore `bLiving`.

- [ ] **Step 3: Run focused and broad card/relic GREEN**

Run relic, damage, DoT, self-damage, multi-hit, resolution queue, BattleBoard, auto battle, retreat, save, and player-flow tests.

- [ ] **Step 4: Commit**

```powershell
git commit -m "feat: consume life-saving talisman on emergency heal"
```

### Task 3: RED/GREEN — Camp choices

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`
- Modify: route economy/relic tests.

- [ ] **Step 1: Add Camp RED tests**

Assert two actions only:

- `获得保命护符`
- `获得100局内金币`

No action or tooltip may contain `金疮药` or direct healing copy.

- [ ] **Step 2: Implement transactional choices**

The relic choice calls `AcquireRelic`; when already owned it is disabled. The money choice adds exactly 100 through the route economy authority, never `PlayerGold`, then settles the Camp node exactly once.

- [ ] **Step 3: Run GREEN and commit**

Run RouteEncounter Panel, route economy, relic, route settlement, PlayerFlow, and SaveGame suites.

```powershell
git commit -m "feat: replace camp rewards with charm or route money"
```

### Task 4: RED/GREEN — transparent Travel atlas fallback

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: focused battle-animation presentation tests if a shared resolver is introduced.

- [ ] **Step 1: Add atlas coverage RED tests**

For all six fixed companions and six Quest NPCs, resolve Idle/Attack/Hit/Death. Assert the preferred 1K package or the fallback 2K package exists. Explicitly cover Guard, Healer, Hunter, Sorcerer, FormationMaster, SongJinBao, YueBai, ZhouGuangZu, JinGui, and QiongMeiEr.

- [ ] **Step 2: Add transparent-pending widget tests**

Switch from Blade/Tusi to a member without 1K. Before async load completes, assert the companion Image brush has no resource and render opacity is zero. After fallback load, assert the 2K resource is applied and opacity returns to one.

- [ ] **Step 3: Implement preferred/fallback clip pairs**

Request 1K first. If unavailable or failed, request the same asset/action at 2K. On identity/path changes, clear stale frames immediately. Never retain a default UImage brush.

- [ ] **Step 4: Run Workbench/atlas GREEN and commit**

```powershell
git commit -m "fix: fall back travel party animations without white blocks"
```

### Task 5: RED/GREEN — separate Backpack Tab and paper-local X

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add control-layout RED tests**

Expanded state must assert:

- Tab uses `CharacterTabSelectedTexturePath`;
- Tab label is `▲`;
- no CloseInk exists in the Tab container;
- one CloseInk button exists inside the Backpack paper top-right and dispatches global action 60.

Collapsed state must assert normal Tab texture, `▼`, and no Backpack-local X.

- [ ] **Step 2: Implement separate controls**

Keep `BuildBackpackTabToggle` as a normal/selected tab only. Add the parent CloseInk from the Backpack paper builder at a panel-local top-right position. Both click paths and keyboard Tab reuse action 60.

- [ ] **Step 3: Run close-stack GREEN and commit**

Run ParentCloseStack, EmbeddedDeferredRefresh, LocalClosePreservesSession, CarriedRightCancel, full Workbench, and PlayerFlow suites.

```powershell
git commit -m "fix: place backpack close ink on the paper panel"
```

### Task 6: full PIE and user handoff

- [ ] **Step 1: Cold UBT and full regression**

Run relic/card battle, RouteEncounter, route economy, Workbench, PlayerFlow, SaveGame, and route settlement suites.

- [ ] **Step 2: Real pure-2D PIE**

On `L_DesktopTrainingHUD`:

1. verify expanded selected Tab and paper-local X;
2. verify collapsed normal Tab;
3. cycle every companion and Quest NPC through the Travel strip, observing all four actions where practical;
4. enter Camp, acquire the charm, verify duplicate option disabled;
5. trigger below-50 and lethal cases, verify all-party healing and relic removal;
6. run Camp +100 route-money choice and verify ordinary gold is unchanged.

- [ ] **Step 3: Direct visual review and leave PIE running**

Review the screenshots with a method suitable for the available evidence, then leave the project PIE open at a clean Workbench for the user.
