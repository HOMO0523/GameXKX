# Battle Card Readability Design

## Goal

Make every battle card readable at gameplay resolution without changing card rules, serialised save data, approved PSD assets, or the existing target-selection contract.

## Confirmed constraints

- Preserve the approved first-row PSD frame, its 452:516 aspect ratio, portrait sources, and owner-colour lower strip.
- A battle hand still exposes five cards. The battle’s end-turn action must remain reachable at a 1280 pixel-wide viewport.
- The existing card runtime remains the authority: card definitions provide `TargetSpec` and `Effects`; previews provide affordability, legal candidates, and failure reasons.
- A player clicks a manual-target card, sees only legal units highlighted, moves the established ink-arrow with the pointer, then chooses a highlighted unit or cancels.
- A card that cannot currently be played is still inspectable and must state the precise reason; clicking it must remain non-mutating.

## Chosen design

### Hand layout

Render five hand cards at 150 x 171 pixels (the same PSD ratio as 113 x 129). The fixed hand row becomes 790 x 171 pixels: five 150-pixel cards plus the existing 4-pixel left and right slot padding. Its centred bottom anchor leaves at least 15 pixels before the 190-pixel end-turn button at 1280 wide.

### Hover and selected state

Hover scales a card to 1.16 and raises it 26 pixels. A selected manual-target card uses 1.20 and -32 pixels. The PSD frame remains white; the interaction emphasis is a paper/ink tint and existing owner strip rather than a replacement card frame. The visual detail panel is `HitTestInvisible` so crossing it cannot clear hover.

### Detail panel

A 360 x 230 parchment panel appears above the active card (hovered card, or the selected card while target selection is active). It shows:

1. card name, owner/role, and rarity;
2. energy and mana cost;
3. an explicit target instruction;
4. a multiline effect summary generated from the definition’s declarative effects;
5. condition, status-consumption, terrain and delayed-modifier clauses;
6. current interaction state: immediate play, choose a highlighted target, or the exact non-playable reason.

The panel uses the existing Town backpack paper frame and battle ink colour. It is not a new raster asset or a second card design.

### Canonical card text

`GameXXKCardText` is a pure, read-only formatter. It has no save fields and no runtime mutation. It translates every current target mode, effect type, status, terrain, condition, modifier trigger, modifier expiry, recipient scope, and guard policy into player-facing Chinese text. It exposes three consumers:

- detail target title and effect body;
- compact UMG tooltip text for accessibility/fallback;
- current target-selection instruction beside the existing arrow.

The catalogue test iterates all 174 definitions and rejects an empty or fallback/unknown summary. This prevents a new catalogue row from silently becoming unreadable.

### Disabled and target-selection interaction

Hand-card buttons remain pointer-interactive when a card is temporarily unplayable so their detail panel and reason remain available. Their opacity and ink tint convey disabled state; the click path continues to call the existing non-mutating preview and therefore cannot play an invalid card.

After selecting a manual target card, the panel stays open and changes to `请选择 1 名敌方/友方目标` derived from the effective preview. Legal candidate count and the arrow instruction are visible. Cancelling restores the normal hover state.

## Non-goals

- Do not change the 174 definitions, costs, effects, randomness, route reward rules, or save schema.
- Do not replace the approved PSD card/portrait assets.
- Do not introduce grid/ground targeting; the current combat model targets stable combat units only.

## Acceptance evidence

1. A focused automation test verifies 150 x 171 frame size, five-card row placement, hover/selected motion and the detail panel lifecycle.
2. A focused formatter test verifies each catalogue card has target/effect text and covers manual enemy, manual ally, group, automatic, conditional, consumption and persistent-modifier examples.
3. A disabled-card fixture verifies hover/detail remains available while the existing preview still refuses play.
4. Existing targeting, pending-choice, NPC support, route reward and full battle tests continue to pass.
5. Cold UBT compilation and a PIE smoke run verify the UI behaves in the actual board.
