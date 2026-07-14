# GameXXK UI, World-Map, and Asset-Layer Design

**Status:** User-approved design discussion on 2026-07-14.
**Scope:** Player-facing UI composition and the missing Start -> world-map -> town entry screen. This specification does not change quest, save, battle, or route rules.

## 1. Product Decision

GameXXK has two different map screens. They must not share the same visual role or navigation contract.

```text
Main Menu
  -> Jianghu Travel world-map UI (town/region selection)
      -> Qingshan Post Town (Asian Village demo level)
          -> quest NPC interaction
              -> north-gate interaction
                  -> dungeon route map (combat/event/camp/shop/Boss nodes)
```

### 1.1 Jianghu Travel World Map

- Shown immediately after New Game or an eligible Continue operation.
- Runs as a full-screen UI on `L_Main`; it is not a walkable level.
- `Qingshan Post` is the only enabled entry in the first release.
- `Huangshan Perilous Path` and `Tanjiang Ferry` both appear as independent illustrated region cards, but are locked, disabled, and present only as future-content previews.
- Clicking a locked card shows a visible lock/future-content message and must not travel or mutate progression state.
- Clicking Qingshan runs `SelectWorldRegion(Qingshan)` and only then asks `GameXXKLevelFlow` to travel to the Asian Village town map.

### 1.2 North-Gate Dungeon Route

- It remains a separate gameplay screen entered only by the town north-gate `F` interaction after the quest accepts.
- It keeps the generated branch/node rules for battle, event, camp, merchant, elite, and Boss progression.
- It may be reskinned in the common ink-and-paper UI language, but must never be presented as the Jianghu Travel town-selection map.

## 2. State and Navigation Contract

Existing `UGameXXKMVPSubsystem`, `GameXXKMVPCommandRouter`, and `GameXXKLevelFlow` remain the only authority for gameplay state and map loading.

| User action | UI command/state transition | Player-visible result |
|---|---|---|
| Main-menu Start | `StartGame` / `StartNewGame` | Runtime screen becomes `WorldMap`; Qingshan is not auto-selected |
| Continue | Restore save state | Restore to the saved eligible screen; a saved WorldMap returns to Jianghu Travel |
| Qingshan card | `SelectWorldRegion(Qingshan)` | State becomes `Town`, then LevelFlow opens the Asian Village demo town |
| Huangshan/Tanjiang card | no state mutation | Locked/future-content modal appears |
| Town world-map control | `OpenWorldMap` | Return to Jianghu Travel |
| North gate `F` | existing quest-gated `EnterDungeon` | Open the independent route-map level |

No visual widget opens a level directly. A button only emits the existing command; the state transition and LevelFlow decide whether travel is legal.

## 3. UI Composition Model

Keep the existing screen-specific widgets and tests. Add the missing WorldMap player-facing widget, then give every screen the same layer contract rather than replacing the project with one monolithic UI widget.

```text
Screen widget root
  0 Background      scene dimmer, parchment, map art
 10 Decoration      frames, ink strokes, corners, title plaques
 20 Content         cards, slots, portraits, region images
 30 Interaction     UButtons, tabs, route nodes, dynamic values
 40 Modal           confirmation, lock prompt, tooltip, reward
 50 Focus           cursor, selected-target ink, controller focus
```

- Full-screen menus and maps use UI focus and visible mouse input.
- Town gameplay returns focus to the game viewport so WASD, F, I, and Esc continue to work.
- Modal windows must consume input only while visible. Hidden layers are `Collapsed` or `SelfHitTestInvisible` so transparent widgets never block clicks.
- The design canvas is 1920x1080. Art exports are 2x transparent PNGs; placement uses anchors and scale boxes. Window frames use nine-slice margins instead of stretching a full screenshot.

## 4. Art and Text Rules

### 4.1 Static Raster Content

These may be exported as part of a transparent visual asset:

- Decorative paper, ink borders, corner marks, title plaques, tabs, separators, slot frames, locks, node icons, and ornamental illustrations.
- Fixed button labels and fixed title text, including `Start Game`, `Jianghu Travel`, `Qingshan Post`, and fixed page-tab labels.

An image-generation model must not be trusted to render Chinese labels. Create the no-text base artwork first, compose approved Chinese lettering as a correctly typeset raster layer, then export the final state texture.

### 4.2 Live UMG Content

The following is always rendered live, never baked into an image:

- Gold, level, HP/MP, XP, inventory counts, equipment, prices, quest objectives/progress, save-slot timestamps, player names, unlock state, rewards, and combat values.
- Any text that can change after an input or a save load.

### 4.3 Button States

Every interactive visual has a separate `UButton` hit target and visual states.

```text
BTN_<Screen>_<Action>_Normal
BTN_<Screen>_<Action>_Hover
BTN_<Screen>_<Action>_Pressed
BTN_<Screen>_<Action>_Disabled
```

Static wording can be included in each state image. The UButton owns hit testing, enabled state, focus state, and command dispatch; the texture never owns behavior.

## 5. Screen Inventory

| Release page | Required visual atoms | Dynamic/interactive content |
|---|---|---|
| Main menu and save select | cover, title, menu buttons, save-card frame, confirm modal | save summary, slot timestamp, delete confirmation |
| Jianghu Travel | parchment map, Qingshan/Huangshan/Tanjiang cards, lock chain, region title, player panel | Qingshan selection, locked-card modal, current progress |
| Town HUD | resource strip, quest strip, interaction prompt, shortcut icon base | currencies, quest text, nearby F prompt |
| Character / quest / inventory / shop | window frame, tabs, equipment/item slots, action-button bases | stats, item contents, prices, purchase confirmation |
| North-gate route | route background, connector strokes, node and Boss icons | reachability, node selection, completion progress |
| Event / camp / merchant / battle | option cards, action base, target ink, result/reward modal | choices, HP, target state, rewards, battle results |
| System overlays | pause, settings, save, tooltip, loading and error frames | runtime messages and current settings |

## 6. Source, Runtime Assets, and Layout Data

Source artwork is kept outside UE Content so auto-reimport never mutates runtime packages.

```text
docs/ui/v1/source_art/
  UIStyle_Master.json
  WorldMap/
  MainMenu/
  Town/
  Route/
  Battle/

Content/GameXXK/UI/V1/
  Atoms/
  WorldMap/
  MainMenu/
  Town/
  Route/
  Battle/
```

Each screen has a layout JSON recording logical canvas, anchors, Z order, art resource id, and linked command. Each reusable atom has its own art JSON with size, alpha requirement, nine-slice margins if applicable, states, static text policy, and dynamic slots. UE imports only the approved transparent runtime assets.

## 7. Delivery Batches

1. **U0 Foundation** — paper/ink palette, frames, title plaques, tabs, buttons, locked state, tooltip, modal, and item-slot primitives.
2. **U1 Entry loop** — main menu, save select, Jianghu Travel world map, Qingshan entry, Huangshan/Tanjiang locked cards.
3. **U2 Town loop** — town HUD, quest, character, inventory, merchant, manual save.
4. **U3 Route loop** — north-gate route map, all node state visuals, event/camp/merchant screen presentation.
5. **U4 Battle loop** — battle command surfaces, targeting ink, results/rewards, Boss feedback.
6. **U5 System pass** — settings, loading, tooltips, error messages, focus and accessibility pass.

Each batch must be visually reviewed in PIE before the next batch starts. Do not generate all full-screen art before a batch proves its layout, hit boxes, and state changes.

## 8. Acceptance and Test Plan

Each batch has three evidence levels.

1. **Asset verification** — every referenced texture exists, has alpha, exposes all required states, and respects dimensions/nine-slice metadata.
2. **Widget verification** — each control exists at its intended layout position; visible, hover, pressed, disabled, and modal states are correct; dynamic values refresh from state.
3. **Real PIE verification** — mouse and keyboard interaction reaches the same game command and produces player-visible results.

The initial U1 playable proof is:

```text
L_Main
  -> click Start Game
  -> Jianghu Travel appears
  -> click Qingshan Post
  -> Asian Village town opens
  -> return to Jianghu Travel
  -> click Huangshan and Tanjiang
  -> each displays a lock prompt and never changes level or progression
```

Full-loop UI proof additionally checks town F/I/Esc behavior, quest acceptance, north-gate route entry, route-node handoff, battle resolution, save/restore, and Boss unlock feedback.

## 9. Out of Scope for This Design

- Huangshan or Tanjiang gameplay maps, missions, and unlocked traversal.
- New battle rules, quest rules, inventory economy, or town layout changes.
- Replacing the existing MVP state authority with UI-owned state.
- Reusing composited reference screenshots as runtime full-screen UI backgrounds.
