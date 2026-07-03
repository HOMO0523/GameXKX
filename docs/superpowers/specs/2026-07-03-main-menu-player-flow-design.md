# GameXXK Main Menu Player Flow Design

## Goal

GameXXK's main menu should feel like the player's entry point into a game, not a developer command panel. The first pass should remove the dense always-visible shell from the normal player path and make each menu state show only the controls needed for that state.

## Confirmed Decisions

- Main menu direction: clean landing page.
- Continue interaction: center modal.
- Save-slot density: compact one-line rows.
- Delete confirmation: small secondary confirmation modal.
- Landing buttons: `New Game`, `Continue`, `Options`, `Quit`.
- Debug shell: hidden from the default player flow and available only through a development switch, hotkey, or editor-only path.

## Player Flow

### Landing

The default main menu shows only four primary actions:

- `New Game`
- `Continue`
- `Options`
- `Quit`

No save slots, debug commands, route commands, battle commands, or state-machine controls are visible on the landing state.

### New Game

`New Game` creates a fresh runtime state and enters the first real gameplay scene, currently the town scene. It does not inspect, load, or mutate any existing save slot.

### Continue

`Continue` opens a centered modal over the clean landing screen. The modal contains five manual save slots in a compact list. Each row occupies one line.

Populated slots show concise state, for example:

- `Slot 1 · Qingshan Town · Lv 2`
- `Slot 3 · Route Map · Lv 3`
- `Slot 5 · Battle · Lv 4`

Empty slots show `Slot X · Empty` and cannot be entered.

Clicking a populated slot loads exactly that save slot and resumes from the saved runtime state.

### Delete Save

Each populated save-slot row has a `Delete` affordance. Clicking it opens a small confirmation modal above the Continue modal:

`Delete Slot X?`

The confirmation modal has only two actions:

- `Cancel`
- `Delete`

Confirming delete removes only that save slot. It does not change the current runtime state, start a new game, or close the main menu unless the implementation explicitly decides to refresh the Continue modal after deletion.

### Options

`Options` belongs to the player-facing menu, but it does not need full settings behavior in this pass. For this MVP, it opens a small player-facing modal that says options are not available yet and offers a single close action. It must not expose debug commands.

### Quit

`Quit` is part of the player-facing menu. In PIE/editor builds, it shows a small "Quit is not available in editor" response and keeps the player on the main menu. Packaged behavior can be implemented later.

## UI State Rules

- Only one main menu layer is active at a time: landing, Continue modal, delete confirmation modal, or Options modal.
- Continue modal hides save-slot complexity until the player asks for it.
- Delete confirmation is a secondary modal and should block accidental slot clicks until dismissed.
- The debug shell is not rendered in the normal player menu.
- Debug commands may still exist for automation or editor workflows, but they must not share the same visible surface as the player main menu.

## Acceptance Criteria

- On startup, the visible player menu contains only `New Game`, `Continue`, `Options`, and `Quit`.
- `Continue` opens a centered modal with five compact one-line slot rows.
- Empty slots are visibly empty and cannot load a game.
- Populated slots can be loaded.
- Each populated slot exposes `Delete`.
- `Delete` opens a small confirmation modal with `Cancel` and `Delete`.
- Confirming delete removes only that slot.
- `New Game` ignores existing save slots and starts fresh.
- No route-map, battle, shop, save-game, or debug command buttons are visible on the landing state.

## Non-Goals

- Do not redesign town, route-map, battle, pause, or save-in-game UI in this spec.
- Do not implement final art direction, animation, or audio for the menu.
- Do not remove debug command routing or automation support; only hide it from the default player-facing UI.
- Do not implement full Options behavior beyond the safe player-facing unavailable-options modal.

## Next Design Step

After this main menu spec is accepted, the next scene should be designed separately. The likely next target is the town scene: minimal HUD, HD2D player visibility, NPC interaction prompt, quest accept flow, follower state, and transition into the route map.
