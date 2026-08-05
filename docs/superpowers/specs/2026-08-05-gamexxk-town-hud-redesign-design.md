# GameXXK `02_城镇HUD` Redesign

## Scope

Only rebuild top-level PSD page `02_城镇HUD`. Keep every other master-PSD page unchanged. This pass edits the master PSD only and does not migrate assets or layout into Unreal Engine.

## Approved Direction

Use a clear runtime town view with edge-mounted HUD components. Do not place a central paper window, a persistent interaction prompt, or a full-screen dim layer over the scene.

## Layout

- Upper left: reuse the already calibrated shop hero-identity component without changing its portrait placement, level, identity, or combat-power alignment.
- Upper right: use a compact paper currency strip 320 px wide. Center the currency icon and value as one visual unit.
- Left edge: keep the five real kit navigation icons permanently visible in this top-to-bottom order: backpack, companion, codex, quest, route.
- Center: leave the playable town view unobstructed.
- Lower left: show no persistent prompt panel in the normal HUD state. NPC and interactable prompts are runtime-only conditional UI.
- Background: use the actual town gameplay view without a full-screen dark overlay.

## Currency Rule

- Out-of-run pages use the ingot icon and out-of-run currency value.
- In-run pages use the copper-coin icon and in-run currency value.
- Both currencies share the same neutral paper-strip asset; only the icon and runtime value change.
- In-run copper coins convert to out-of-run ingots at run completion according to gameplay configuration. The exchange ratio is data, not baked into this PSD.
- `02_城镇HUD` is an out-of-run page and therefore displays the ingot icon.

## Editable PSD Structure

The page must keep the following independently editable concerns:

- town background placeholder/reference;
- hero identity component;
- compact currency strip;
- five navigation buttons and their real icons;
- runtime text/value layers.

Legacy HUD content may be renamed and hidden inside `02_城镇HUD`, but must not be deleted.

## Acceptance

- Canvas export is 1920 x 1080.
- No central paper panel, lower-left persistent prompt, or full-screen dim layer is visible.
- The approved shop hero-identity component is reused without position drift.
- The upper-right strip is 320 px wide and shows the ingot icon plus value centered together.
- All five left navigation icons use the existing real kit assets and remain aligned.
- Other top-level PSD pages retain their original signatures.
- Art validation uses PSD structure, alignment checks, and visual export review; TDD is not required.
