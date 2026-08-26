# Desktop HUD Background-Layer Cutout Design

**Date:** 2026-08-26  
**Status:** User-approved design  
**Canonical surface:** `/Game/GameXXK/Maps/L_DesktopTrainingHUD`

## Goal

Present the existing desktop-placement HUD as a movable Windows overlay while removing only the incorrectly added full-canvas paper background. Preserve the authored color and opacity of characters, terrain, text, icons, buttons, and individual feature panels. Empty pixels must reveal and pass input through to the desktop.

The implementation must remain entirely inside the GameXXK project. Unreal Engine source under `D:\UE_5.8\Engine\Source` must not be modified.

## Confirmed visual contract

- Remove `WorkbenchBackground` from `UGameXXKDesktopTrainingWorkbenchWidget::BuildWorkbenchShell()` instead of fading or recoloring it.
- Do not replace it with another full-window paper, solid color, chroma-key color, or post-process mask.
- Preserve the existing paper surfaces owned by backpack, warehouse, talent, tools, training, settings, confirmation, and other actual panels.
- Desktop mode reveals the Windows desktop through the empty space.
- Town mode uses the same root-background-free widget tree and therefore reveals the 3D world through the empty space.
- Tab expansion changes the overlay bounds and visible content, not the configured HUD scale.

## Evidence behind the decision

The editor-only layer audit captured the same HUD in full-reference, root-background-only, and foreground-only forms.

- The foreground-only capture retained the terrain, characters, text, icons, controls, individual paper panels, and their native texture alpha.
- Empty pixels contained zero RGB and zero alpha.
- Opaque foreground pixels recomposed with effectively zero error.
- Error was isolated to partially transparent antialiasing pixels.
- Applying alpha repeatedly was approximately 1.9 times worse than a single correct composite at those edges and visibly produced brighter or washed-out halos.

Therefore the runtime must not read back the complete HUD and apply alpha again. It must render the foreground once into a compositor surface whose alpha convention matches the Windows compositor.

## Architecture

### 1. Root-background removal

The workbench widget stops constructing `WorkbenchBackground`. All feature panels continue to construct their own authored surfaces. This change applies to both desktop-window and town-viewport presentation modes.

### 2. Project-scoped Windows composition plugin

Add a Windows runtime plugin named `GameXXKDesktopOverlay`. Its responsibility is limited to presenting one tagged GameXXK desktop overlay through DirectComposition.

The plugin:

- implements a project-side `IDXGISwapchainProvider` for D3D12;
- activates only inside an explicit, game-thread-scoped overlay creation token and for the uniquely named GameXXK overlay window;
- creates a composition swap chain with `DXGI_ALPHA_MODE_PREMULTIPLIED`;
- attaches that swap chain to a DirectComposition visual and target for the overlay HWND;
- leaves every editor, game viewport, town, PIE, and unrelated Slate window on Unreal's normal swap-chain path;
- owns and releases its DirectComposition device, target, visual, and per-window swap-chain association without modifying Unreal Engine source.

The overlay `SWindow` requests per-pixel transparency so Slate clears its render surface to transparent. Slate draws the background-free widget tree directly into the composition surface. No CPU code changes foreground RGB, extracts a chroma key, or multiplies the completed HUD alpha a second time.

### 3. Presentation modes

**Desktop window:** The plugin-backed native overlay is sized to the current visible HUD bounds. It is movable, topmost by default, and resizes when Tab changes between collapsed and expanded content.

**Town viewport:** The existing UMG viewport path remains in use. The HUD is fixed at its authored initial position, cannot be dragged, and remains fully interactive. The desktop composition plugin is not involved.

## Input contract

The native overlay must never intercept the whole rectangular window merely because it is topmost. `HTTRANSPARENT` is not used as the cross-process click-through mechanism because Windows only guarantees its forwarding behavior between covered windows on the same thread.

A layout-derived native window-region snapshot is maintained for the current state. Project code builds an `HRGN` union and applies it with `SetWindowRgn` before the overlay is shown:

- the idle strip and notice rail contribute rectangular regions;
- expanded backpack, warehouse, talents, tools, training, settings, confirmation, navigation, and notice surfaces contribute their visible layout regions;
- the circular town control contributes an elliptical region with enough padding to preserve its complete ink edge;
- transparent gaps between those shapes are excluded from the native window region and therefore pass input to any underlying process;
- action buttons and tabs inside the region continue through Slate's normal input routing;
- non-control portions of the idle strip retain the current drag behavior.

The region is geometry-based rather than read from a GPU alpha buffer, so it adds no frame readback and cannot lag one rendered frame behind the visuals. `WM_NCHITTEST` remains available for normal client/drag behavior inside the accepted region, but it does not claim to provide cross-process click-through.

## Lifecycle and fallback

1. Preflight DirectComposition, D3D12, and plugin availability before hiding the ordinary game viewport.
2. Create the tagged overlay without showing it.
3. Confirm that the composition swap chain and DirectComposition target were attached successfully.
4. Install the native hook and apply the current `SetWindowRgn` union while the overlay remains hidden.
5. Only after composition and region application both succeed, show the overlay and hide the ordinary desktop-map game viewport.
6. On close, map transition, or EndPlay, restore the game viewport first, then detach and release the composition resources and destroy the overlay window.

If preflight, attachment, native-hook installation, or region application fails, the controller does not show a black, rectangular-input-blocking, or partially initialized overlay. It keeps or restores the ordinary UE game viewport and hosts the same background-free workbench through the existing viewport path. No fallback is allowed to reintroduce the deleted full-canvas paper.

Only one overlay association may exist at a time. Repeated opening, closing, Tab expansion, and monitor/DPI changes reuse or resize the existing swap chain and release obsolete buffers.

## Memory and performance boundaries

There is no continuously updated `UTextureRenderTarget2D`, CPU pixel array, `ReadPixels`, `UpdateLayeredWindow`, or full-screen capture buffer in the runtime path.

At the currently measured 100% sizes, a BGRA8 buffer is approximately:

- collapsed `1038 x 254`: 1.0 MiB;
- expanded `1672 x 941`: 6.0 MiB.

A three-buffer swap chain therefore requires approximately 3.0 MiB collapsed or 18.0 MiB expanded before driver bookkeeping. At 50% HUD scale, pixel-buffer cost is approximately one quarter of the 100% value.

After one warm expanded cycle, 60 additional collapse/expand cycles followed by a cooldown must show no monotonic private-memory or dedicated-GPU-memory growth. The warmed private-memory result may not exceed its baseline by more than 32 MiB. The plugin must not add more than 1 ms median frame time relative to the ordinary Slate window on the same HUD state.

## Verification

### Deterministic tests

- `WorkbenchBackground` is absent from the constructed widget tree.
- The desktop window requests per-pixel transparency; town presentation does not use the plugin.
- The swap-chain provider activates only for the scoped GameXXK overlay.
- Overlay creation failure leaves the ordinary viewport visible.
- Window destruction releases the plugin association and restores the viewport.
- Native-region tests cover excluded gaps, controls, drag surface, expanded panels, the circular town control, modal surfaces, and state changes.
- Tab changes bounds without changing the HUD scale setting.

### Real Windows tests

- Compare the foreground over white, mid-gray, and dark desktop test surfaces against the ordinary Slate reference.
- Opaque pixels must remain visually identical; soft edges must show no systematic bright, dark, or colored halo.
- Verify desktop click-through using an interactive application behind the overlay.
- Verify every visible button, Tab, dragging, topmost behavior, DPI scaling, monitor movement, closing, and reopening.
- Run the memory cycle and frame-time measurements after an initial warm-up.
- Verify a Development packaged build separately from PIE.

### Regression floor

- Build `GameXXKEditor Win64 Development` without Live Coding or Hot Reload.
- Run the complete `GameXXK.DesktopTraining.Workbench` automation group.
- Recheck the canonical pure-2D desktop-to-BattleBoard flow.
- Verify the town presentation only as the scoped 3D regression surface.
- Record checksums of the relevant installed-engine source files before and after implementation; they must remain unchanged.

## Non-goals

- No Unreal Engine source patch.
- No CPU layered-window renderer.
- No chroma-key removal.
- No full-screen transparent host or full-screen render target.
- No redesign of individual HUD panels, icons, layout, gameplay, or combat systems.
