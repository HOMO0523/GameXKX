# Workbench Progression Systems Execution Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved Workbench parent, route choice/merchant, ordered formation, and permanent talent systems as four independently green production units.

**Architecture:** Keep `/Game/GameXXK/Maps/L_DesktopTrainingHUD` and the existing paper/ink assets as the canonical surface. Implement parent lifetime first, then route overlays, ordered party data, and finally the 35-layer permanent talent graph and its rule projections.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, project UE MCP scripts, cold UBT.

---

## Source specification

`docs/superpowers/specs/2026-08-22-workbench-route-formation-talent-system-design.md`

## Execution order

1. `docs/superpowers/plans/2026-08-22-workbench-parent-close-stack.md`
2. `docs/superpowers/plans/2026-08-22-route-event-card-merchant.md`
3. `docs/superpowers/plans/2026-08-23-decoupled-party-formation.md`
4. `docs/superpowers/plans/2026-08-22-permanent-talent-graph.md`

Unit B initially reads the existing effective order (`Hero / active companion / active task NPC`) behind one party-card-pool seam. Unit C replaces that seam's source with the persisted `1P / 2P / 3P` array without changing merchant rules. This preserves the approved delivery order and avoids a partially unusable merchant.

## Global constraints

- Work directly on root `main`; do not create a worktree.
- Preserve every unrelated dirty file and every protected map/asset.
- Do not use UnrealBridge, Live Coding, or Hot Reload.
- Save dirty packages through UE MCP before editor restart or closure.
- Run the pure-2D map for PIE and screenshots.
- Every behavior change follows RED -> confirm intended failure -> minimal GREEN -> related suite -> commit.
- Do not combine production units into one commit.

## Cross-unit completion gate

- [ ] Unit A focused automation, full `GameXXK.DesktopTraining.Workbench`, cold UBT, real occupied-slot click, and close-stack visual checks are green.
- [ ] Unit B event/merchant rules, widget, player-flow suites, cold UBT, and real route event/shop loop are green.
- [ ] Unit C formation rules, save migration, battle/Travel/merchant consumers, Workbench suite, cold UBT, and real order swap are green.
- [ ] Unit D talent catalog/rules, save migration, inventory, Training/offline/chest/tool integration, widget suites, cold UBT, 35-layer graph PIE, and visual checks are green.
- [ ] `python scripts/harness_state_validator.py` reports no new production-record findings.
- [ ] `git diff --check` reports no whitespace errors in scoped files.
