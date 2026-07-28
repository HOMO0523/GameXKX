# Route Relics and Positive Encounters

## Locked player-facing contract

- A route run owns an unlimited ordered relic list. New acquisitions appear first.
- Route map and battle share the same list. Six square icons render per row in the upper-right and additional rows grow downward.
- Every icon uses a high-fill simplified low-saturation Chinese ink illustration, transparent background, stack number when applicable, and hover tooltip with name and exact effect.
- Relics remain active for the current route and are cleared on route failure, boss clear, or explicit route abandonment.
- The catalog contains exactly 30 distinct relics with live effects covering battle start, round start/end, card play, damage/kill, node completion, and positive event rewards.
- The encounter catalog contains exactly 12 positive entries split between question-mark events and treasure chests. Every entry is a HUD-only illustration/paper panel with explicit clickable choices; no node resolves until a choice succeeds.
- Question-mark events grant route-local character attributes. Task-NPC encounters retain the confirmed temporary-support path and add an explicit route attribute blessing.
- Every treasure/reward node displays three distinct relics and resolves only after the player selects one. It does not fall back to direct gold or consumables.

## Visual contract

- Icon canvas is square; the subject occupies about 85% of the canvas in both dimensions.
- One dominant silhouette, broad ink shapes, few interior lines, no text, no frame, no floor or cast shadow.
- Low-saturation mineral/ink palette on transparent alpha; readable at a 72px HUD slot.
- All final source PNGs live in `docs/ui/relics/source_art`; imported textures live in `/Game/GameXXK/UI/Relics/Icons`.
