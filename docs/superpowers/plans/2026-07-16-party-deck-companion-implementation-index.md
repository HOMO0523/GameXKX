# Party Deck and Companion System Implementation Plan Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved permanent-companion, task-NPC, shared-card-deck system without regressing the current world-map, town, quest, route, save, and battle flow.

**Architecture:** The approved design is deliberately split into three implementation layers plus one visual-production layer. Core card rules and battle phase behavior become stable before persistent companion/route state is attached; the PSD UI only consumes those stable subsystem APIs; card and character art production remains last because card frames, original NPC identity references, and the existing eight-direction pipeline must be audited before visual output is made.

**Tech Stack:** Unreal Engine 5.8 C++, UMG programmatic widgets, Unreal Automation Tests, UBT cold builds, project UE MCP scripts, PSD cut assets, and image generation only in the final approved art plan.

---

## Detailed delivery goal and definition of done

Deliver one coherent, persisted playable expansion—not disconnected mockups—with the following observable result:

1. **Preserved mainline:** `L_Main → Start/New Game → L_QingshanInn → F` accepts the existing quest, manual save/load preserves follower/quest NPC state, the town button opens the route map, a route node opens battle, and no existing town/world/camera/sprite tuning is overwritten.
2. **Playable party progression:** the player can recruit up to 12 permanent partners, handle deterministic duplicates and full-roster replacement, level/star them, inspect their unique seeded 12-card libraries, equip one active partner, and enter a route with exactly Hero + at most one permanent partner + at most one temporary task NPC.
3. **Task/event/reward flow:** 土司首领、宋金宝、月白、周光祖、金贵、琼么儿 are route-temporary task NPCs with 4 choose 3 cards and differentiated passives; 牛欢 stays an event NPC; Money Rat/Black Bear/Tiger stay normal/elite/Boss. Battle victory waits for a legal card reward choice or skip, and temporary content cleans up at route end without damaging permanent progression.
4. **Complete card game:** all 174 approved card definitions are data-driven, selectable/configurable, visibly rendered, test-covered and reachable through their intended source; start decks, replacement cap, draw/hand/discard/Insight behavior, target conditions, keywords, terrain, enemy intents and end-turn phase rules are functional. Every manual target card follows: click card → legal actor outlines → ink arrow follows mouse from card owner → click a monster or friendly character by stable UnitId → revalidation → resolution.
5. **PSD-faithful interface:** `角色、伙伴图鉴、任务、地图、背包`, battle hand, pile viewers, intent strip and reward overlays all use approved PSD cuts. Companion/reward cards use only first-row `057–059` geometry; all overflowing grids/lists have the right-side paper/ink scrollbar; no generic blank cards, white browser scrollbar or square second-row frame remains.
6. **Visual character package:** all six named task NPCs and six permanent-support role archetypes receive a validated 8-direction world sprite package compatible with the existing Hero/Follower PaperZD/Flipbook convention (eight idle facings plus eight-direction walk sheets), derived from approved NPC references and consistent with the project’s ink/pixel style. Named NPC card art preserves their approved identity reference; partner role art is uniform by role. The engine imports, maps and verifies every new sheet before use.
7. **Evidence before handoff:** every phase is red→green tested, cold-built without Live Coding/Hot Reload, visually checked in PIE, and run through `scripts/ue_tdd_pipeline.py` plus `scripts/gamexxk_real_play_flow_mcp.py`. Any missing PSD or character reference asset is surfaced as a named blocking gap; it is never silently replaced by invented UI or a different character.

The final result is accepted only when a new or migrated save can demonstrate the entire loop from town configuration through one route combat/reward cycle, with a target-card arrow aimed at both an enemy and a friendly target, followed by save/load continuity and clean route exit.

---

## Preconditions that apply to every plan

- [ ] Work in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- [ ] Before changing a target, run `git diff -- <target>` and preserve every pre-existing user change. The repository is already dirty in HUD, inventory, route and rules files; never reset, checkout, mass-stage, or silently overwrite those hunks.
- [ ] If a planned source file contains overlapping uncommitted work that cannot be cleanly separated, stop that task and ask the user before modifying it. New test/source files may still be committed independently; a mixed existing file must not be committed with unrelated user work.
- [ ] Before any cold C++ build, save dirty editor packages through the project UE MCP workflow. Do not force-close a running editor if MCP cannot save its dirty state. Do not use Live Coding or Hot Reload.
- [ ] Every behavior change follows a red → green test cycle and a cold build. The final end-to-end verification uses `scripts/ue_tdd_pipeline.py` and `scripts/gamexxk_real_play_flow_mcp.py`.

## Execution order

| Order | Plan | Why it is isolated | Completion gate |
| ---: | --- | --- | --- |
| 1 | [Card runtime and battle](2026-07-16-card-runtime-and-battle-implementation.md) | Establishes all card definitions, deck/hand state, keywords, turn phase and enemy intent logic without UMG dependencies. | Battle can run a 5-card shared hand, end the player phase once, and resolve one enemy phase deterministically. |
| 2 | [Companion progression and route](2026-07-16-companion-progression-and-route-implementation.md) | Adds persistent roster, random card seeds, stars, temporary NPCs, terrain, reward choices and save migration on top of stable card APIs. | Recruit → save/load → select one partner/NPC → route → reward → cleanup has automated coverage. |
| 3 | [PSD party/deck UI](2026-07-16-psd-party-deck-ui-implementation.md) | Replaces the flat codex and action buttons only after model operations are stable. | Five panels, battle hand, rewards, map deck and paper-ink scrollbars work with actual first-row PSD geometry. |
| 4 | [Card art and eight-direction sprites](2026-07-16-card-art-and-eight-direction-sprite-implementation.md) | Requires a source-art audit and uses generated assets only after no-face-reinvention rules are enforceable. | All 174 card visual recipes resolve; named NPC identity art and six role archetypes are imported as validated eight-direction sprite packages. |

The source specification is [2026-07-16-party-deck-companion-system-design.md](../specs/2026-07-16-party-deck-companion-system-design.md). Every plan below implements a disjoint subset of that document; the combined plans cover all 174 definitions, two temporary starter cards, three-person party cap, old-save compatibility, five PSD panels, and art policy.

## Repository file map locked by the plan set

| File / directory | Responsibility in the plan set |
| --- | --- |
| `Source/GameXXK/Public/GameXXKCardTypes.h` | New public USTRUCT/UENUM card, target-selection, status, terrain, deck and intent types that are safe to serialize in RuntimeState. |
| `Source/GameXXK/Public/GameXXKCompanionTypes.h` | New public USTRUCT/UENUM persistent partner, task-NPC, recruitment, route-reward and adventure-run types; added by the companion/route plan. |
| `Source/GameXXK/Public/GameXXKCardCatalog.h` / `Private/GameXXKCardCatalog.cpp` | Immutable 174-card catalogue, archetype/NPC/encounter definitions, stable IDs and card view builders. |
| `Source/GameXXK/Public/GameXXKCardRules.h` / `Private/GameXXKCardRules.cpp` | Pure deck construction, deterministic random selection, status/keyword resolution, card effects, player/end phase and intent resolution. |
| `Source/GameXXK/Public/GameXXKMVPRules.h` / `Private/GameXXKMVPRules.cpp` | Existing runtime state and route lifecycle integration only; keeps compatibility with the old mainline fields. |
| `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` / `Private/MVP/GameXXKMVPSubsystem.cpp` | Sole game/UI command boundary for deck configuration, recruitment, reward choice and card play. |
| `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` | Replaces fixed command buttons with hand, intent, shared energy and End Turn controls while retaining validated scene targeting input. |
| `Source/GameXXK/Public/UI/GameXXKCharacterPanelWidget.h` / `Private/UI/GameXXKCharacterPanelWidget.cpp` | Shared PSD character panel supporting Hero, active permanent partner and temporary task NPC modes. |
| `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h` / `Private/UI/GameXXKCompanionRosterWidget.cpp` | Replaces the flat Town HUD companion codex with a real 12-slot roster/codex grid. |
| `Source/GameXXK/Public/UI/GameXXKRouteDeckWidget.h` / `Private/UI/GameXXKRouteDeckWidget.cpp` | Route deck viewer and card-reward picker, hosted by the map/battle HUD rather than rules code. |
| `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h` / `Private/UI/GameXXKTownHudWidget.cpp` | Retains only town navigation and opens the dedicated roster/character panels; removes the old generic codex overlay. |
| `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h` / `Private/UI/GameXXKInventoryWindowWidget.cpp` | Adds a shared companion-backpack source without moving normal consumables out of the existing backpack. |
| `docs/ui/town/source_art/PartyDeck/` and `Content/GameXXK/UI/Town/Textures/PartyDeck/` | Audited exports/imported UE textures from the approved PSD first-row card and paper/ink components. |
| `SourceAssets/PartyDeck/`, `Content/GameXXK/UI/PartyDeck/CardArt/`, `Content/GameXXK/Sprites/Generated/PartyDeck/` | Source-ledgered card art, generated role/NPC source sheets, and new imported assets only. These directories must never overwrite Hero, Follower, Merchant, PaperZD, placed-level or camera assets. |
| `Source/GameXXK/Private/Tests/GameXXK*Card*.cpp`, `GameXXK*Companion*.cpp`, `GameXXK*Battle*.cpp` | Focused rule, persistence, UI and scene-regression automation tests. |

## Shared commands

Use these exact project commands; replace only the automation filter named in the individual task.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
```

For a focused cold build without running PIE:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
```

For a focused automation test after a successful cold build:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests <FILTER>;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

For the final playable-flow check after the UI plan:

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --timeout 600 --report Saved/HarnessReports/party-deck-real-flow.json
```

## Compatibility assertions that cannot be traded away

- `bFollowerJoined`, quest NPC location persistence and the north-gate route entry remain supported until a separate approved migration replaces them.
- `BattleScenePresenter` already limits the player side to three visible unit slots; the new party builder must use those slots and not add a fourth actor.
- `Wolf → 牛欢` visual mapping is invalid under the approved design. The battle/scene plan replaces it with a real task/event NPC path; no combat mapping may show 牛欢.
- Existing blank/generic codex cards and square second-row PSD frames may not survive the UI plan.
- A battle victory may not mark the route node complete until a pending three-choice card reward has been selected or explicitly skipped.
