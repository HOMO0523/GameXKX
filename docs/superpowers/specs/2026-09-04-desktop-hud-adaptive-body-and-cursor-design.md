# Desktop HUD Adaptive Body and Cursor Design

## Goal

Keep the draggable idle dock fixed while every expanded workbench surface remains visible inside the current monitor work area. Eliminate downward-drag clipping, auxiliary-button attachment drift, and carried-item cursor offsets across manual HUD scales and window DPI.

## Layer model

The desktop HUD has four independent coordinate layers:

1. **Native host** — one transparent Win32/Slate window covering the current monitor work area. Its rectangle does not move or resize when the HUD is dragged or Tab changes state. `SetWindowRgn` still limits visible and interactive pixels to the actual UI surfaces.
2. **Dock layer** — idle strip, notice/control rail, Tab, chest buttons, and retry button. It follows only the player's persisted drag anchor.
3. **Body layer** — warehouse, backpack, training, tools, bottom navigation, toolbar, gold, close controls, and the story/town circular buttons. It keeps its authored internal relationships but receives one shared body-fit translation.
4. **Cursor layer** — carried-item and drag-preview visuals. It uses native client coordinates converted directly through window DPI and never inherits dock or body transforms.

## Horizontal body fitting

The warehouse, backpack, and right panel may coexist. Their union remains in the authored left/center/right order. After the dock anchor is resolved, the body union is translated as one unit until it fits the work area:

- right overflow produces an equal left translation;
- left overflow produces an equal right translation;
- the dock receives zero body translation;
- closing a panel recomputes the union and removes unnecessary correction.

At a right-docked 100% layout, the authored right panel exceeds the strip edge by 304 logical units. The body therefore moves left by 304 while the dock remains fixed. No panel flips sides and panels are not forced to be mutually exclusive.

## Vertical behavior

Auxiliary paper surfaces match the backpack body height:

- downward body: logical Y `244..777`;
- upward body: logical Y `34..567`, using the shared `-210` body translation.

Warehouse keeps the existing 36-slot logical page and six talent pages. Its 4×9 grid is placed in a vertical scroll viewport so capacity, save data, batch-transfer ranges, and talent semantics do not change. Training uses three compact horizontal nodes. Tools uses the same body-height shell.

The full-work-area host removes the false internal clipping edge shown in the recording. While dragging an already-expanded dock, the current direction remains stable so Slate capture is not destroyed by a rebuild. On mouse release, direction is recalculated from the final dock anchor and the body is rebuilt above or below as needed.

## Attached controls

Story and town circular controls move into the body coordinate domain. They receive the same horizontal fit and upward offset as the backpack instead of switching between outer-canvas and root-canvas origins. Dock controls remain in the dock domain. Chest geometry and drag-exclusion hit boxes use one shared value.

## Carried item

The carried-item image lives in a dedicated cursor canvas over the work-area host. Its physical center equals the current native client cursor position. Its Slate position and size are:

```text
slate center = client physical pixels / window DPI scale
slate size   = 56 × manual HUD scale / window DPI scale
slate top-left = slate center - slate size / 2
```

Warehouse `+148`, upward `-210`, and horizontal body-fit translation never enter this calculation. The implementation also verifies that the authoritative slot selected by Slate matches the carried entry.

## Region and input

UMG render transforms, Win32 Region shapes, drag exclusion rectangles, and test geometry consume the same final dock/body/cursor values. During HUD drag or item carry, the existing full-input-region policy remains active; after release, the Region returns to the union of visible UI surfaces.

## Acceptance

- Full native host equals the monitor work area at 50%, 75%, and 100%; Tab and drag do not resize it.
- Left/right/corner dock positions preserve the dock screen rectangle.
- Any active warehouse/backpack/training/tools union is translated fully into the work area when it physically fits.
- Downward drag has no internal clipping edge; direction is reselected after release.
- Warehouse, training, and tools paper heights equal the backpack body height.
- Story/town controls remain attached to the body before and after warehouse toggles and upward layout.
- A carried item remains within two physical pixels of the cursor for 96/120 DPI, every manual scale, both directions, and warehouse closed/open.
- Backpack slot identity remains unchanged by dock/body translations.

