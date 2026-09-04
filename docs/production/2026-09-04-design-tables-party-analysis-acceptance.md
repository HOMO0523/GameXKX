# Design Tables and Party Analysis Acceptance

## Deliverables

Commit `3f11796` adds reproducible exporters and the generated current-design artifacts under `docs/design/2026-09-04-project-design-tables/`:

- Card workbook: 173 active cards, 419 legal quality variants, 36 task/reward branches, 1,498 per-card Pill rows, shared glossary, state text, formulas, and coverage checks.
- Equipment workbook: 49 templates, 35 affixes, 18 set descriptors, 30 gems, quality/tier ranges, enhancement, crafting, sockets, acquisition, and economy rules.
- Enemy workbook: 21 enemies, 78 intents, 90 intent-effect rows, three boss phase transitions, level curves, standard route projections, formations, passives, and source hashes.
- Self-contained composition HTML and its aggregate JSON: 36 hero/partner/NPC combinations, profession and NPC statistics, archetype results, equipment/terrain orthogonal results, card contribution tables, heatmap, rankings, and build recommendations.

## Simulation evidence

`GameXXK.Diagnostics.PartyCompositionObservation` ran 3,240 current-rule cases and passed without warnings. Every combination uses the same level-10 standard hero, chapter 2, Plain terrain, no equipment, one of six permanent-partner roles, and one of six task NPCs. Each of the 36 combinations receives 30 seeds at Battle, Elite, and Boss nodes, or 90 cases per combination. The test also verifies six birth cards, five selected partner cards, no stranded target, and deterministic replay samples.

`GameXXK.Diagnostics.OrthogonalBalanceObservation` ran the existing 2,520-case current orthogonal matrix. The test succeeded; its only warning was an unrelated Google `generate_204` connectivity timeout. The HTML uses its 630 equipment-set cases and 540 terrain cases without treating incomplete set descriptors as runtime effects.

The current top aggregate is standard hero + Hunter + Song Jinbao: 100% wins across 90 cases, 144.6 effective friendly damage per round, 6.08 average rounds, and 368.3 average remaining party health. Hunter leads the role aggregate at 99.1% wins and 129.5 damage per round. Song Jinbao is the highest-scoring NPC for all six partner roles, which is recorded as a balance-risk signal. Zhui Feng leads the fixed-party hero equipment orthogonal slice at 211.8 damage per round; Sorcerer retains high damage but only 76.7% wins, while Formation Master has the longest average battle duration.

These results measure the current Skilled automatic policy, not manual-play ceiling. All composition cases share the standard hero deck, so partner + NPC ranks are direct simulation results while hero profession-card recommendations remain mechanism-based. `Profession.Sorcerer.RanLingHuanYuan` and `Npc.JinGui.HouXiangTuoShen` remain pending decisions. Xuan Jia and Shan He remain explicit incomplete-descriptor surfaces.

## Artifact verification

- Cold UBT `GameXXKEditor Win64 Development -NoHotReload`: succeeded.
- Workbook reload validation: card 10 sheets, equipment 12 sheets, enemy 10 sheets; all expected row counts matched.
- Exporter Python compilation: all three scripts passed `py_compile`.
- HTML JavaScript: Node syntax check passed.
- Chrome render: no console/page errors; 49 heatmap cells, 36 ranking rows, seven recommendation cards, and five balance findings rendered.
- Source artifacts pass `git diff --check`.
