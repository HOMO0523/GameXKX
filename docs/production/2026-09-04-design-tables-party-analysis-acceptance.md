# Design Tables and Party Analysis Acceptance

## Corrected authority

The first `3f11796` export incorrectly treated the still-legacy equipment, enemy, phase, and Training runtime as the approved redesign. Its level-10, chapter-2, no-equipment simulation and old Boss-only phase table are superseded and must not be cited.

The corrected artifacts use `docs/superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md` as authority wherever it conflicts with current C++. That specification is approved design but its new equipment curves, 1/2/3-phase enemy runtime, fixed 189 formations, and level-135 Training support remain implementation work. The revised HTML therefore presents a transparent design projection rather than an old-runtime win-rate claim.

## Corrected deliverables

- Card workbook: unchanged 173 active cards, 419 legal quality variants, 36 branches, and 1,498 per-card Pill rows. The two pending cards remain explicit.
- Equipment workbook: level-100 post-reduction reference stats, six Treasure pieces, twelve Treasure gems split 4 Attack / 4 Defense / 4 HP, corrected post-Immortal gem values, item-level 101-135 support, no equipment Mana, and implementation gaps. Legacy item templates remain as identifiers only where exact new per-slot integer curves are not yet frozen.
- Enemy workbook: all 21 retained stat curves extrapolated through levels 100/125/135; the approved 27 stage levels and 189 fixed formations; 36 ordinary intents; 42 phase-one intents; 39 phase-two and 39 phase-three intents; difficulty/status conversions; and a dedicated Hell 3-1 sheet.
- Composition HTML: level-100 approved equipped role stats against level-125 Hell 3-1. The stage-end formation is Vulture / White Ape / Giant Toad with phase counts 1 / 3 / 1 and 11,178 raw phase HP. Difficulty multiplies enemy damage by 150% and does not multiply enemy HP or Defense.

## Projection method

The HTML uses the approved final level-100 Hero and six partner-role attributes, already including six Treasure equipment pieces and the 4/4/4 Treasure-gem package. NPC final equipped values are not frozen by the specification, so their rows explicitly use level-100 NPC naked attributes plus the average equipment budget of their two linked professions; they are marked as projections.

For each of 36 partner/NPC combinations, the model uses the highest legal, non-pending card versions. It constructs an eight-card Hero candidate package, a five-card partner package, and a three-card NPC package, then finds a maximum five-card burst within the shared three-Energy budget. Attack coefficients come from expanded card text; full-party attacks resolve against the three Hell 3-1 stage-end targets; player level 100 versus enemy level 125 applies the approved 75% level-difference factor after Defense. DOT output counts one generated-reservoir trigger. Sequence rewards, task replays, random draw order, Mana failure, healing, Armor, enemy actions, and phase transition timing are excluded, so the result is a damage-budget comparison rather than a win-rate or round-count certification.

`Profession.Sorcerer.RanLingHuanYuan` and `Npc.JinGui.HouXiangTuoShen` are excluded from the primary ranking because their numeric/target decisions remain pending. The HTML still identifies the associated sensitivity.

## Verification

- Approved-spec parser asserts 21 enemies, 27 stages, 189 formations, 36 ordinary intents, 42 phase-one intents, 39 phase-two intents, and 39 phase-three intents.
- Hell 3-1 is asserted at level 125; Hell 3-3 is level 135 and its approved three-phase stage-end raw HP sums to 33,318.
- Workbook reload checks pass for card, equipment, and enemy workbooks.
- Exporter Python compilation and `git diff --check` pass.
- HTML JavaScript passes Node syntax validation and Chrome renders 49 heatmap cells and 36 ranking rows with no console/page errors.
- The obsolete `GameXXK.Diagnostics.PartyCompositionObservation` fixture is removed so the old level-10/no-equipment matrix cannot be mistaken for the new design.
