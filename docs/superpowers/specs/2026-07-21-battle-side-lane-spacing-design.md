# Battle Side-Lane Spacing Design

## Goal

Remove the overlap between the inner enemy and party fixed HUDs while preserving
the existing 3v3 mirrored, diagonal battle formation.

## Approved Layout Rule

Move every enemy lane 30 cm toward screen-left and every party lane 30 cm
toward screen-right. The battle camera looks along +X, therefore this is a
Y-axis translation:

| Side | Current local Y lanes | New local Y lanes |
|---|---|---|
| Enemy | -250, -180, -110 | -280, -210, -140 |
| Party | +250, +180, +110 | +280, +210, +140 |

Apply an equal fixed-HUD anchor shift, without using per-frame projection:

| Side | Current X anchors P1/P2/P3 | New X anchors P1/P2/P3 |
|---|---|---|
| Enemy | .14, .29, .44 | .11, .26, .41 |
| Party | .86, .71, .56 | .89, .74, .59 |

Y anchors, the 272x142 logical HUD size, unit role order, hit areas, and
BattleVisual-only P1/P3 art corrections remain unchanged.

## Acceptance

At the current 1114x626 PIE viewport, the inner HUDs must change from about
24 px horizontal overlap to at least 40 px of separation. Outer HUDs must
remain inside the safe stage and not overlap the end-turn rail. The actor
locations and their matching fixed HUD anchors must still identify the same
1P/2P/3P slots.

## Scope Boundary

This change does not alter resource-bar fill art or the party-Qi icon. Those
are separate visual fixes, avoiding a combined layout and material regression.
