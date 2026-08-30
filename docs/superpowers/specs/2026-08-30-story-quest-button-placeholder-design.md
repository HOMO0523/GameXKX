# Story Quest Placeholder Button Design

Date: 2026-08-30

## Goal

Restore only the desktop story-quest button as an inert, testable UI control. The button must be visible and pressable in the expanded desktop workbench, but it must not open a task panel, start narrative playback, alter task state, fold the idle strip, change maps, or affect any other workbench state.

## Current Baseline

The active worktree already retains the layout-only scaffolding introduced by the later prototype:

- `GetStoryQuestButtonSize()` resolves to `144 x 144` logical pixels.
- `GetStoryQuestRect(false)` places the button above the town toggle.
- `GetStoryQuestRect(true)` moves it with the town toggle when the left warehouse drawer is open.
- desktop native-region types can represent a story-quest hit surface.

The active runtime does not construct the button. The approved source image exists at `SourceArt/UI/DesktopOverlay/T_DesktopStoryQuestButton.png`, with SHA-256 `182a5003cf948f7aa5957e63354381d1ad001e11cc17b41a25c9dd852a8c89f9`, but the corresponding Texture2D UAsset is absent from the active content tree.

## Selected Asset Approach

Import only `T_DesktopStoryQuestButton` from the hash-verified PNG through focused Unreal editor Python. Do not run a bulk directory save and do not replace or resave the existing enter-town and exit-town button assets.

The imported Texture2D must use the established UI settings:

- no mipmaps;
- UI/editor-icon compression and UI texture group;
- sRGB enabled;
- never stream;
- bilinear filtering;
- clamp addressing on both axes.

The existing deterministic source test remains authoritative for the `512 x 512` dimensions, real alpha channel, transparent corners, visible-alpha ratio, and soft-edge ratio.

## Runtime Behavior

Add an explicit action ID `654` and construct `StoryQuestButton` only while the workbench Tab is expanded.

The button:

- uses the imported story-quest texture;
- has a `144 x 144` logical size;
- appears above the town toggle when no left drawer is open;
- shifts with the town toggle when the warehouse drawer is open;
- uses the existing press-scale animation and a centered transform pivot;
- owns a native interactive mouse surface so the desktop window does not pass the click through;
- remains enabled unless town map travel is already pending, matching the town-toggle safety rule.

Clicking action `654` must be explicitly handled and return immediately. It must produce no notice, delegate call, panel change, task-state change, map travel, story transition, input lock, or layout rebuild.

## State Invariance

A placeholder click must preserve all of the following:

- expanded/folded workbench state;
- warehouse-open state and warehouse page;
- active center page, navigation, and right panel;
- carried inventory and tool reservations;
- town-map travel pending state;
- HUD scale, window position, and always-on-top setting;
- narrative and guide state;
- current notice text;
- programmatic layout-build count.

The press animation is purely Slate presentation and is not persistent workbench state.

## Explicit Non-goals

Do not restore, reference, or instantiate:

- `GameXXKStoryTaskDrawerWidget`;
- story-task drawer rules or story catalog orchestration;
- desktop narrative layers or fullscreen narrative surfaces;
- guide coordinators or onboarding behavior;
- task red dots, tabs, rewards, or continuation actions;
- Tab locking, automatic folding, input suppression, viewport visibility changes, or window minimization;
- any story result, reward, save migration, or task progress mutation.

No warehouse paper or text change is included in this button-only unit; that remains a separate selectable recovery.

## Verification

### Asset checks

- Run `scripts/test_desktop_town_toggle_asset.py` and require all source-image checks to pass.
- Verify the imported asset path is exactly `/Game/GameXXK/UI/DesktopOverlay/T_DesktopStoryQuestButton.T_DesktopStoryQuestButton`.
- Verify no existing town-toggle UAsset timestamp or hash changes during the focused import.

### Runtime tests

Add a focused workbench automation test that proves:

- the collapsed workbench has no `StoryQuestButton`;
- the expanded workbench has one button with action ID `654` and the approved texture;
- the closed-left-drawer position equals `GetStoryQuestRect(false)`;
- the warehouse-open position equals `GetStoryQuestRect(true)`;
- the native region includes the button;
- invoking the real button click leaves the complete observable workbench/runtime snapshot unchanged.

Add a source contract that rejects references to `StoryTaskDrawer`, `DesktopNarrative`, `EnterNarrativePresentation`, or guide APIs in the placeholder button implementation.

### Build and manual acceptance

After tests pass, perform a cold UBT build with no Live Coding or Hot Reload. Launch only a normal manual test surface with no automation runner. The user verifies the icon, location, press animation, hit area, and warehouse-open shift; clicking must visibly press and otherwise do nothing.
