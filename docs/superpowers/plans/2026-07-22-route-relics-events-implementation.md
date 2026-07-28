# Route Relics and Positive Encounters Implementation Plan

1. Add red automation contracts for 30 relic definitions, 12 positive encounters, route lifetime, six-column wrapping, and tooltips.
2. Implement serializable relic instances, catalogs, deterministic acquisition, trigger application, and route cleanup.
3. Replace hard-coded event identities with a deterministic 12-entry encounter catalog; question-mark choices grant route-local character attributes while retaining the six named task-NPC support paths.
4. Implement deterministic treasure/reward three-relic offers and explicit one-of-three acquisition.
5. Add one shared right-top relic bar owned by the player flow and visible on route map, event/chest panels, and battle.
6. Generate 30 individual high-fill simplified ink relic PNGs, remove chroma-key backgrounds, validate alpha/coverage, import through focused editor Python, and bind paths in the catalog.
7. Cold-build, run focused automation, then verify question-mark, chest, relic acquisition, battle effects, tooltip, wrapping, and route cleanup in PIE.
