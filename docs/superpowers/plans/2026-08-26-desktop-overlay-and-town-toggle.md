# Desktop Overlay and Town Toggle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the erroneous full-canvas paper, present the background-free HUD through a project-only GPU-composited Windows overlay, and add the approved circular button that preserves HUD state while entering and exiting the existing 3D town.

**Architecture:** A Win64 project plugin supplies a narrowly scoped DirectComposition swap-chain provider for only the tagged desktop overlay window. The workbench renders without `WorkbenchBackground`, derives an OS-owned native window region from stable layout geometry, and uses a transient `UGameInstanceSubsystem` snapshot to restore stable UI state after the town map transition. The 3D town continues to use the ordinary viewport-hosted UMG path.

**Tech Stack:** Unreal Engine 5.8 C++, Slate/UMG, D3D12, DXGI, DirectComposition, Win32, UE Automation Tests, project Python asset import, PowerShell process/GPU metrics, UBT and Development packaging.

---

## Working rules

- Work directly in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- Do not use UnrealBridge, Live Coding, or Hot Reload.
- Do not modify any file under `D:\UE_5.8\Engine\Source`.
- Save dirty editor packages through UE MCP before any editor shutdown required for a cold build.
- Existing overlapping source files are already dirty. Never revert them, and never commit unrelated pre-existing hunks. New isolated files may be committed independently; overlapping files remain uncommitted unless an index-only patch can safely isolate the new hunks.
- The visual source is the approved text-only design in `docs/superpowers/specs/2026-08-26-desktop-town-toggle-button-design.md`; do not reintroduce any deleted speckled rectangular button family.

## File structure

### New project plugin

- `Plugins/GameXXKDesktopOverlay/GameXXKDesktopOverlay.uplugin` — Win64 runtime-plugin manifest.
- `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/GameXXKDesktopOverlay.Build.cs` — RHI/DirectComposition dependencies and Windows system libraries.
- `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Public/IGameXXKDesktopOverlayModule.h` — the only API consumed by GameXXK.
- `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopOverlayModule.cpp` — module lifecycle and public API forwarding.
- `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopSwapchainProvider.h` — scoped provider state and per-HWND association ownership.
- `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopSwapchainProvider.cpp` — DXGI/DirectComposition creation, fallback delegation, and deterministic release.

### Game runtime

- `GameXXK.uproject` — enable only the new project plugin.
- `Source/GameXXK/GameXXK.Build.cs` — add the plugin dependency on Win64.
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — scoped overlay creation, attach/fallback/restore, and map-toggle orchestration.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp` — expanded left extent, town-button rect, and native window-region shape resolution.
- `Source/GameXXK/Public/UI/GameXXKDesktopWorkbenchSessionState.h` — shared enums and the stable session snapshot.
- `Source/GameXXK/Public/UI/GameXXKDesktopHudSessionSubsystem.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopHudSessionSubsystem.cpp` — one-shot GameInstance-owned snapshot.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp` — remove root paper, build/move the town button, apply the native region, and capture/restore session state.
- `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp` — pure enter/exit target resolution.

### Art and automation

- `SourceArt/UI/DesktopOverlay/T_DesktopTownToggleInk.png` — approved transparent ink-circle source without text.
- `Content/Python/gamexxk_import_desktop_town_toggle.py` — deterministic UI texture import.
- `Content/GameXXK/UI/DesktopOverlay/T_DesktopTownToggleInk.uasset` — runtime icon.
- `scripts/test_desktop_town_toggle_asset.py` — dimensions/alpha/edge/occupancy contract.
- `scripts/measure_desktop_overlay_cycles.ps1` — standalone 60-toggle memory and GPU sampler.
- `scripts/test_measure_desktop_overlay_cycles.py` — sampler safety/contract tests.
- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp` — all runtime/layout/lifecycle contracts.

---

### Task 1: Record the baseline and lock the first red behavior

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Evidence: `Saved/HarnessReports/desktop-overlay-engine-hashes-before.json`
- Evidence: `Saved/Automation/DesktopOverlayRed/`

- [ ] **Step 1: Record installed-engine source hashes without modifying the engine**

Run:

```powershell
$engineFiles = @(
  'D:\UE_5.8\Engine\Source\Runtime\ApplicationCore\Public\GenericPlatform\GenericWindowDefinition.h',
  'D:\UE_5.8\Engine\Source\Runtime\ApplicationCore\Private\Windows\WindowsWindow.cpp',
  'D:\UE_5.8\Engine\Source\Runtime\D3D12RHI\Private\D3D12Viewport.h',
  'D:\UE_5.8\Engine\Source\Runtime\D3D12RHI\Private\D3D12Viewport.cpp',
  'D:\UE_5.8\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Viewport.cpp',
  'D:\UE_5.8\Engine\Source\Runtime\D3D12RHI\D3D12RHI.Build.cs'
)
$report = $engineFiles | ForEach-Object {
  $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $_
  [ordered]@{ path = $_; sha256 = $hash.Hash }
}
$report | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 `
  'Saved\HarnessReports\desktop-overlay-engine-hashes-before.json'
```

Expected: six records and no engine timestamp changes.

- [ ] **Step 2: Change the existing transparent-placement assertion to require the root paper to be absent**

Replace the current functional-surface assertion with:

```cpp
TestNull(
	TEXT("the erroneous full-canvas workbench paper is not constructed"),
	Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("WorkbenchBackground"))
		: nullptr);
```

- [ ] **Step 3: Change the window contract to require the approved desktop behavior**

Use the existing two-argument test seam and assert:

```cpp
const TSharedRef<SWindow> Window =
	AGameXXKMVPPlayerController::BuildDesktopTrainingOverlayWindowForTest(
		WindowPosition,
		WindowSize);
TestEqual(
	TEXT("desktop HUD requests per-pixel Slate output"),
	Window->GetTransparencySupport(),
	EWindowTransparency::PerPixel);
TestTrue(
	TEXT("desktop HUD starts topmost"),
	Window->IsTopmostWindow());
TestEqual(
	TEXT("desktop overlay uses its unique provider tag"),
	Window->GetTitle().ToString(),
	FString(TEXT("GameXXKDesktopOverlay")));
```

- [ ] **Step 4: Cold-build and run the two focused tests to prove RED**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
& scripts\run_mvp_test_suites.ps1 `
  -Suites @(
    'GameXXK.DesktopTraining.Workbench.TransparentDesktopPlacement',
    'GameXXK.DesktopTraining.Workbench.TransparentSlateWindowContract'
  ) `
  -TimeoutSeconds 300
```

Expected: the background assertion fails because `WorkbenchBackground` still exists; the window contract fails because it still uses `EWindowTransparency::None`, the old title, and non-topmost startup.

---

### Task 2: Add the Win64 overlay plugin boundary

**Files:**
- Create: `Plugins/GameXXKDesktopOverlay/GameXXKDesktopOverlay.uplugin`
- Create: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/GameXXKDesktopOverlay.Build.cs`
- Create: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Public/IGameXXKDesktopOverlayModule.h`
- Create: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopOverlayModule.cpp`
- Create: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopSwapchainProvider.h`
- Modify: `GameXXK.uproject`
- Modify: `Source/GameXXK/GameXXK.Build.cs`

- [ ] **Step 1: Create the plugin manifest**

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "GameXXK Desktop Overlay",
  "Description": "Project-only DirectComposition host for the GameXXK desktop HUD.",
  "Category": "GameXXK",
  "CanContainContent": false,
  "Installed": false,
  "Modules": [
    {
      "Name": "GameXXKDesktopOverlay",
      "Type": "Runtime",
      "LoadingPhase": "PreDefault",
      "PlatformAllowList": ["Win64"]
    }
  ]
}
```

- [ ] **Step 2: Create the module rules**

```csharp
using UnrealBuildTool;

public class GameXXKDesktopOverlay : ModuleRules
{
    public GameXXKDesktopOverlay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "RHI" });
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "ApplicationCore", "CoreUObject", "Projects", "RenderCore"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicSystemLibraries.AddRange(new[] { "dcomp.lib", "dwmapi.lib", "dxgi.lib" });
        }
    }
}
```

- [ ] **Step 3: Define the narrow public interface**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "RHIDefinitions.h"

class GAMEXXKDESKTOPOVERLAY_API IGameXXKDesktopOverlayModule : public IModuleInterface
{
public:
	static constexpr const TCHAR* OverlayWindowTitle = TEXT("GameXXKDesktopOverlay");

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("GameXXKDesktopOverlay"));
	}

	static IGameXXKDesktopOverlayModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IGameXXKDesktopOverlayModule>(
			TEXT("GameXXKDesktopOverlay"));
	}

	virtual bool IsRuntimeSupported() const = 0;
	virtual void BeginOverlayWindowCreation() = 0;
	virtual bool EndOverlayWindowCreation(void* NativeWindowHandle) = 0;
	virtual bool IsOverlayAttached(void* NativeWindowHandle) const = 0;
	virtual void ReleaseOverlayWindow(void* NativeWindowHandle) = 0;

	virtual bool ShouldInterceptForTest(
		const FString& WindowTitle,
		bool bCreationScopeActive,
		ERHIInterfaceType RHIType) const = 0;
};
```

- [ ] **Step 4: Add a compile-only module/provider skeleton**

The module must register one `IDXGISwapchainProvider` in `StartupModule`, unregister it in `ShutdownModule`, and forward the public interface to the provider. At this red/compile boundary, non-overlay calls delegate to the supplied DXGI factory and an armed overlay call records failure rather than pretending it attached.

- [ ] **Step 5: Enable and depend on the plugin only on Win64**

Add this project plugin entry:

```json
{
  "Name": "GameXXKDesktopOverlay",
  "Enabled": true,
  "PlatformAllowList": ["Win64"]
}
```

Add to `GameXXK.Build.cs`:

```csharp
if (Target.Platform == UnrealTargetPlatform.Win64)
{
    PrivateDependencyModuleNames.Add("GameXXKDesktopOverlay");
}
```

- [ ] **Step 6: Cold-build the plugin boundary**

Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 160`.

Expected: UBT succeeds; the original two behavior tests remain red because the controller and workbench have not changed.

- [ ] **Step 7: Commit only the isolated new plugin files if safe**

Do not include pre-existing staged or dirty files. Use `git commit --only` with only the new plugin paths after inspecting `git diff --cached --name-only`.

---

### Task 3: Implement the DirectComposition swap-chain provider

**Files:**
- Modify: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopSwapchainProvider.h`
- Create: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopSwapchainProvider.cpp`
- Modify: `Plugins/GameXXKDesktopOverlay/Source/GameXXKDesktopOverlay/Private/GameXXKDesktopOverlayModule.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add a failing provider-routing contract**

```cpp
IGameXXKDesktopOverlayModule& Overlay = IGameXXKDesktopOverlayModule::Get();
TestTrue(TEXT("only the armed D3D12 overlay title is intercepted"),
	Overlay.ShouldInterceptForTest(
		IGameXXKDesktopOverlayModule::OverlayWindowTitle,
		true,
		ERHIInterfaceType::D3D12));
TestFalse(TEXT("an unarmed window is delegated"),
	Overlay.ShouldInterceptForTest(
		IGameXXKDesktopOverlayModule::OverlayWindowTitle,
		false,
		ERHIInterfaceType::D3D12));
TestFalse(TEXT("an unrelated Slate window is delegated"),
	Overlay.ShouldInterceptForTest(TEXT("GameXXK"), true, ERHIInterfaceType::D3D12));
TestFalse(TEXT("a non-D3D12 RHI is delegated"),
	Overlay.ShouldInterceptForTest(
		IGameXXKDesktopOverlayModule::OverlayWindowTitle,
		true,
		ERHIInterfaceType::Vulkan));
```

Run the focused test and require a behavior assertion failure from the skeleton.

- [ ] **Step 2: Implement strict interception and preflight**

`IsRuntimeSupported()` returns true only when all of these hold:

```cpp
#if PLATFORM_WINDOWS
BOOL bCompositionEnabled = FALSE;
return GDynamicRHI
	&& GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12
	&& SUCCEEDED(::DwmIsCompositionEnabled(&bCompositionEnabled))
	&& bCompositionEnabled;
#else
return false;
#endif
```

`ShouldInterceptForTest` must require the exact title, active creation scope, and D3D12.

- [ ] **Step 3: Implement the composition swap chain**

For the one intercepted HWND:

```cpp
DXGI_SWAP_CHAIN_DESC1 CompositionDesc = *Description;
CompositionDesc.Scaling = DXGI_SCALING_STRETCH;
CompositionDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
CompositionDesc.Flags &= ~(DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	| DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);

TRefCountPtr<IDXGISwapChain1> CompositionSwapChain;
HRESULT Result = Factory->CreateSwapChainForComposition(
	DeviceOrQueue,
	&CompositionDesc,
	RestrictToOutput,
	CompositionSwapChain.GetInitReference());
```

Then create/reuse one `IDCompositionDevice`, create the HWND target and visual, set the swap chain as content, set the visual as root, and call `Commit`. Store the target, visual, swap chain, and original extended style in a map keyed by HWND.

Before attaching, remove `WS_EX_COMPOSITED`, add `WS_EX_NOREDIRECTIONBITMAP`, apply `SWP_FRAMECHANGED`, and clear the extended DWM glass margins. Do not add `WS_EX_LAYERED`, a color key, or `UpdateLayeredWindow`.

- [ ] **Step 4: Implement transparent fallback delegation**

If an unscoped window arrives, call the original `CreateSwapChainForHwnd` unchanged. If an armed overlay cannot create or attach DirectComposition, also delegate to the normal HWND swap chain, record the overlay attempt as failed, and let the controller destroy that hidden window and retain the ordinary viewport.

- [ ] **Step 5: Implement deterministic release**

`ReleaseOverlayWindow(HWND)` clears the target root, commits, releases visual/target/swap-chain references, restores the original extended style when the HWND remains valid, and removes the map entry. `ShutdownModule` releases every remaining association before unregistering the modular feature.

- [ ] **Step 6: Cold-build and run the provider-routing test GREEN**

Expected: UBT succeeds; all four routing assertions pass. This is not yet proof of real composition; that comes from the standalone Windows verification.

---

### Task 4: Integrate safe overlay creation and fallback into the controller

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add compile-safe test seams and the active flag**

```cpp
static TSharedRef<SWindow> BuildDesktopTrainingOverlayWindowForTest(
	const FVector2D& WindowPosition,
	const FVector2D& WindowSize);

static bool ShouldHideViewportAfterOverlayAttachForTest(
	bool bOverlayRequested,
	bool bOverlayAttached);

bool bDesktopTrainingOverlayCompositionActive = false;
```

- [ ] **Step 2: Make the Slate window contract pass**

The public test seam always requests composition. The private production builder receives `bRequestComposition` and uses:

```cpp
.Title(FText::FromString(IGameXXKDesktopOverlayModule::OverlayWindowTitle))
.SupportsTransparency(
	bRequestComposition
		? EWindowTransparency::PerPixel
		: EWindowTransparency::None)
.IsTopmostWindow(true)
```

Keep borderless fixed sizing, input acceptance, no title bar, and no close/minimize/maximize boxes.

- [ ] **Step 3: Scope AddWindow through the plugin**

`EnsureDesktopTrainingOverlayWindow()` performs this exact ordering:

```cpp
IGameXXKDesktopOverlayModule& Overlay = IGameXXKDesktopOverlayModule::Get();
if (!Overlay.IsRuntimeSupported())
{
	return false;
}

Overlay.BeginOverlayWindowCreation();
FSlateApplication::Get().AddWindow(DesktopTrainingOverlayWindow.ToSharedRef(), false);
const void* NativeHandle = DesktopTrainingOverlayWindow->GetNativeWindow().IsValid()
	? DesktopTrainingOverlayWindow->GetNativeWindow()->GetOSWindowHandle()
	: nullptr;
bDesktopTrainingOverlayCompositionActive =
	Overlay.EndOverlayWindowCreation(const_cast<void*>(NativeHandle));
```

If attachment is false, release/destroy the hidden overlay, reset the pointer and flag, and return false.

- [ ] **Step 4: Keep the ordinary viewport until attachment is proven**

Only `ShowDesktopTrainingOverlayWindow()` may hide the game viewport, and only when `bDesktopTrainingOverlayCompositionActive` is true. `PlayerTick` must no longer hide the viewport merely because an `SWindow` exists.

When overlay creation fails, attach the workbench through the existing viewport path and leave the ordinary game window visible.

- [ ] **Step 5: Release in safe order**

`DestroyDesktopTrainingOverlayWindow()` restores the game viewport, obtains the native handle, calls the plugin release API, detaches Slate content, requests window destruction, then clears the active flag and pointers.

- [ ] **Step 6: Run focused lifecycle and window tests**

Expected: the window contract is green; static fallback tests prove that a requested-but-unattached overlay never hides the viewport.

---

### Task 5: Remove the root paper and add a geometry-based native window region

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add failing pure native-region tests**

Define a platform-neutral region state and shape description:

```cpp
struct GAMEXXK_API FDesktopNativeRegionState
{
	bool bExpanded = false;
	bool bWarehouseOpen = false;
	bool bRightPanelOpen = false;
	bool bExitConfirmationOpen = false;
	float NoticeHeight = 0.0f;
	float Scale = 1.0f;
};

enum class EDesktopNativeRegionShapeType : uint8
{
	Rectangle,
	Ellipse
};

struct GAMEXXK_API FDesktopNativeRegionShape
{
	EDesktopNativeRegionShapeType Type = EDesktopNativeRegionShapeType::Rectangle;
	FVector4 Rect = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
};

GAMEXXK_API TArray<FDesktopNativeRegionShape> BuildDesktopNativeRegionShapes(
	const FDesktopNativeRegionState& State);
```

Tests must cover the collapsed strip, collapsed notice rail, expanded center shell, conditional warehouse/right panel, town-button ellipse (added in Task 7), exclusion of a gap between panels, and full modal coverage.

- [ ] **Step 2: Remove only the erroneous root layer**

Delete this construction from `BuildWorkbenchShell()`:

```cpp
AddCanvas(
	RootCanvas,
	MakePanel(WidgetTree, PanelAlt, TEXT("WorkbenchBackground")),
	FVector2D::ZeroVector,
	GetCurrentDesignCanvasSize());
```

Do not change the feature-panel surfaces below it.

- [ ] **Step 3: Implement the pure geometry region list**

Collapsed state emits the strip and notice rectangles. Expanded state emits the shifted idle strip, center shell/navigation/content, an open warehouse shell, an open right shell, settings/confirmation surfaces, and registered action rectangles. No shape is emitted for transparent gaps.

- [ ] **Step 4: Convert the shape list to one owned Win32 region before showing the window**

Add `AttachDesktopNativeWindowForPresentation(void* NativeHandle)` to the workbench. It installs the existing project-side WndProc, applies native layout, converts rectangles with `CreateRectRgn`, ellipses with `CreateEllipticRgn`, combines them with `CombineRgn(..., RGN_OR)`, and calls `SetWindowRgn(WindowHandle, CombinedRegion, TRUE)`. On success Windows owns the combined `HRGN`; delete only temporary component regions. On failure, delete all still-owned regions, uninstall the hook, and return false.

The controller calls this method while the new overlay remains hidden. It may show the overlay and hide the ordinary viewport only when both DirectComposition attachment and native-region application succeed.

Within the accepted window region, `WM_NCHITTEST` uses ordinary client behavior. It must not return `HTTRANSPARENT` as the cross-process mechanism. Preserve existing Tab, DPI, display-change, and move-completed handling.

Update the unique title check from `GameXXK` to `GameXXKDesktopOverlay`.

- [ ] **Step 5: Reapply the region whenever layout, DPI, scale, panel state, or native size changes**

`ApplyDesktopNativeWindowLayout` updates `SetWindowPos` first and applies a freshly built region second. Replacing a successful region transfers ownership of the new region to Windows. `ReleaseDesktopNativeWindow` calls `SetWindowRgn(WindowHandle, nullptr, FALSE)` before restoring the previous WndProc.

- [ ] **Step 6: Run background and native-region tests GREEN**

Expected: `WorkbenchBackground` is null, all individual panel assertions remain green, and the pure native-region shape matrix passes.

---

### Task 6: Produce and import the approved circular town asset

**Files:**
- Create: `SourceArt/UI/DesktopOverlay/T_DesktopTownToggleInk.png`
- Create: `Content/Python/gamexxk_import_desktop_town_toggle.py`
- Create: `Content/GameXXK/UI/DesktopOverlay/T_DesktopTownToggleInk.uasset`
- Create: `scripts/test_desktop_town_toggle_asset.py`

- [ ] **Step 1: Generate one clean source using the imagegen skill**

Use this constrained prompt:

```text
Create one square transparent-background Chinese ink UI icon for a compact desktop RPG.
One irregular circular brush ring, strong variation between thick and thin strokes,
visible tapered brush endings and a few controlled dry-brush gaps. On the upper arc,
integrate only a minimal ancient Chinese roofline, a simple town gate, and two tiny
building silhouettes. Keep the center completely empty for runtime Chinese text.
Flat simplified ink-cartoon style, high shape fill, dark neutral ink with at most one
subtle warm-brown accent. No text, no letters, no rectangular plate, no paper rectangle,
no speckles, no glow, no shadow, no photorealism, no dense architectural detail.
```

- [ ] **Step 2: Visually inspect the source before import**

Reject and edit if it contains text-like marks, a rectangular backing, a mechanically uniform ring, excessive buildings, low fill, or stretched geometry. The final source must remain a true RGBA square.

- [ ] **Step 3: Add deterministic image checks**

The Python test must require:

```python
with Image.open(SOURCE) as image:
    rgba = image.convert("RGBA")
    assert rgba.width == rgba.height
    assert rgba.width >= 512
    alpha = rgba.getchannel("A")
    assert alpha.getbbox() is not None
    assert all(alpha.getpixel(point) <= 8 for point in CORNER_POINTS)
    occupancy = sum(value > 8 for value in alpha.getdata()) / (rgba.width * rgba.height)
    assert 0.12 <= occupancy <= 0.55
```

- [ ] **Step 4: Import with UI texture settings**

The Unreal Python script creates `/Game/GameXXK/UI/DesktopOverlay`, imports the PNG as `T_DesktopTownToggleInk`, sets `TextureGroup=UI`, disables mip generation, preserves alpha, uses transparent border addressing, and saves only that package.

- [ ] **Step 5: Verify dimensions, alpha and asset loading**

Run the Python image contract, execute the import through UE MCP/editor Python, then add a C++ asset-load assertion for the exact runtime path.

---

### Task 7: Add the town-button layout and presentation

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing layout assertions**

Require:

```cpp
TestEqual(TEXT("town button is 144 square"), GetTownToggleButtonSize(), FVector2D(144.0f));
TestEqual(TEXT("closed warehouse needs no left extension"),
	GetExpandedLeftExtension(false), 0.0f);
TestEqual(TEXT("open warehouse extends left by 148"),
	GetExpandedLeftExtension(true), 148.0f);
TestTrue(TEXT("warehouse toggle sits left of warehouse"),
	TownRect.X + TownRect.Z + 14.0f <= ShiftedWarehouseRect.X + KINDA_SMALL_NUMBER);
TestEqual(TEXT("strip anchor is unchanged when warehouse opens"),
	ClosedPlacement.StripTopLeft,
	OpenPlacement.StripTopLeft);
```

- [ ] **Step 2: Extend placement without shifting authored content coordinates**

Add `ContentOffset` and `TownToggleRect` to `FDesktopOverlayPlacement`. When expanded with the warehouse open, set a 148-pixel left extension, increase design width from 1672 to 1820, add that extension to the idle-strip offset used to calculate native placement, and keep the requested normalized strip anchor unchanged.

- [ ] **Step 3: Add an outer host canvas**

Keep existing code writing to `RootCanvas`, but make it a 1672 x 941 inner canvas hosted by a new outer canvas. Position the inner canvas at `DesktopOverlayPlacement.ContentOffset / Scale`. Add the town button to the outer canvas so it can occupy the new left extension without rewriting every existing panel coordinate.

- [ ] **Step 4: Build the town button only while expanded**

Use one named `UGameXXKDesktopTrainingActionButton`:

```cpp
TownToggleButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
	UGameXXKDesktopTrainingActionButton::StaticClass(),
	TEXT("TownToggleButton"));
TownToggleButton->Configure(this, ActionToggleTown);
TownToggleButton->SetStyle(MakeImageButtonStyle(
	TownToggleTexturePath,
	FVector2D(144.0f, 144.0f)));
TownToggleButton->SetContent(MakeButtonText(
	WidgetTree,
	ResolveTownToggleLabel(),
	23,
	Ink));
TownToggleButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
TownToggleButton->SetScaleOnPress(true);
```

`ResolveTownToggleLabel()` returns `进入\n城镇` on the desktop map and `退出\n城镇` on a Qingshan town map.

- [ ] **Step 5: Implement the approved interaction presentation**

Add pressed/released delegates to the custom action button. Only when `bScaleOnPress` is true, pressed sets render scale to `0.96`, released restores `1.0`. Hover uses the existing slight tint increase; no arrow or rectangular backing is added.

- [ ] **Step 6: Include the button in the native window region**

Add one padded ellipse for the circular button. Do not add its complete square bounding box; the square corners and every other transparent point in the left extension remain outside the native window region.

- [ ] **Step 7: Run layout/widget/asset tests GREEN**

Verify absent while collapsed, present while expanded, correct two-line label in each map context, correct rect in both warehouse states, and unchanged strip anchor.

---

### Task 8: Preserve stable HUD state across the town transition

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKDesktopWorkbenchSessionState.h`
- Create: `Source/GameXXK/Public/UI/GameXXKDesktopHudSessionSubsystem.h`
- Create: `Source/GameXXK/Private/UI/GameXXKDesktopHudSessionSubsystem.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Extract shared workbench enums and define the snapshot**

Move the existing navigation/center/right/roster/presentation/tool/notice enums unchanged into `GameXXKDesktopWorkbenchSessionState.h`. Define `FGameXXKDesktopWorkbenchSessionState` with an explicit `bValid` plus every stable field listed in the approved spec. Include `FGameXXKEmbeddedInventorySessionState` for the character tab and pending deck IDs.

- [ ] **Step 2: Write a failing one-shot subsystem test**

```cpp
UGameXXKDesktopHudSessionSubsystem* Session =
	NewObject<UGameXXKDesktopHudSessionSubsystem>(NewObject<UGameInstance>());
FGameXXKDesktopWorkbenchSessionState Expected;
Expected.bValid = true;
Expected.bBackpackExpanded = true;
Expected.bWarehousePanelOpen = true;
Expected.WarehousePageIndex = 2;
Expected.ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Talents;
Session->StoreForMapTravel(Expected);

FGameXXKDesktopWorkbenchSessionState Actual;
TestTrue(TEXT("pending state is consumed once"), Session->ConsumeAfterMapTravel(Actual));
TestTrue(TEXT("stable workbench state round-trips"), Actual == Expected);
TestFalse(TEXT("consumed state cannot leak into a later map"), Session->ConsumeAfterMapTravel(Actual));
```

Define `operator==` on the session struct so the test compares every stable field explicitly.

- [ ] **Step 3: Implement the GameInstance subsystem**

Store one `TOptional<FGameXXKDesktopWorkbenchSessionState>`. `StoreForMapTravel` replaces it, `ConsumeAfterMapTravel` moves it out and resets it, and `DiscardPending` resets it.

- [ ] **Step 4: Add safe workbench capture/restore**

`CaptureSessionStateForMapTravel()` first calls `AbortTransientInventoryInteraction(true, false)`, closes dropdown/modal/pointer state, then copies every stable field. `RestoreSessionStateAfterMapTravel()` validates IDs/pages through existing catalog and capacity helpers, applies presentation mode first, applies stable fields, marks layout/native placement dirty, and performs one `RefreshLayout()`.

- [ ] **Step 5: Add pure map-target resolution tests**

```cpp
TestEqual(TEXT("desktop enters Qingshan"),
	GameXXKLevelFlow::TownToggleTargetForMapPackage(
		TEXT("/Game/GameXXK/Maps/L_DesktopTrainingHUD")),
	FName(TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")));
TestEqual(TEXT("Qingshan exits to desktop"),
	GameXXKLevelFlow::TownToggleTargetForMapPackage(
		TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")),
	FName(TEXT("/Game/GameXXK/Maps/L_DesktopTrainingHUD")));
```

Also cover PIE-prefixed and legacy Qingshan names.

- [ ] **Step 6: Implement one guarded controller transition**

On the town action:

1. reject if `bDesktopTownMapTravelPending` is already true;
2. resolve and validate the target package;
3. capture and store the workbench session;
4. set pending and disable the button;
5. close the current presentation through its normal overlay/viewport lifecycle;
6. call `UGameplayStatics::OpenLevel` with the exact target.

If validation fails before `OpenLevel`, clear pending state, discard the snapshot, re-enable the button, and stay on the current map.

- [ ] **Step 7: Restore once in the destination controller**

After `EnsureDesktopTrainingWidgets()` selects `DesktopWindow` or `TownViewport` and `OpenWorkbench()` succeeds, consume and apply the session snapshot. Town mode then resolves the default anchor and remains non-draggable; desktop mode restores the persisted desktop anchor.

- [ ] **Step 8: Run all session/map/presentation tests GREEN**

Include invalid-ID/page clamping, canceled carry/tool slots, no second click, fixed town anchor, desktop drag, and all stable snapshot fields.

---

### Task 9: Add a standalone memory-cycle verifier

**Files:**
- Create: `scripts/measure_desktop_overlay_cycles.ps1`
- Create: `scripts/test_measure_desktop_overlay_cycles.py`

- [ ] **Step 1: Write the sampler contract test first**

Require the script to:

- use `System.Diagnostics.ProcessStartInfo.ArgumentList`;
- launch only the configured `UnrealEditor.exe -game` process;
- search only for the exact `GameXXKDesktopOverlay` HWND;
- post Tab to that HWND rather than globally injecting keyboard input;
- sample working set, private bytes, dedicated GPU memory, and shared GPU memory;
- warm once, run exactly 60 toggles, sample every ten toggles, cool down, and report the final delta;
- never call `BuildCookRun`, package, terminate unrelated processes, or use a wildcard process kill.

- [ ] **Step 2: Prove the contract test RED**

Run `python scripts\test_measure_desktop_overlay_cycles.py` and expect failure because the sampler is absent.

- [ ] **Step 3: Implement the isolated PowerShell sampler**

Use a small `Add-Type` declaration for `FindWindowW` and `PostMessageW`. Record JSON under `Saved/HarnessReports/desktop-overlay-cycle-<timestamp>.json`. After warm-up, require final private memory no more than 32 MiB above warm baseline and report, but do not conceal, any positive monotonic GPU trend.

- [ ] **Step 4: Run the static script test GREEN**

Expected: all safety and schema assertions pass without launching Unreal.

---

### Task 10: Cold build, regression, visual proof, memory proof and packaging

**Files:**
- Remove diagnostic-only additions from:
  - `Source/GameXXKEditor/GameXXKEditor.Build.cs`
  - `Source/GameXXKEditor/Public/GameXXKEditorCaptureAutomationLibrary.h`
  - `Source/GameXXKEditor/Private/GameXXKEditorCaptureAutomationLibrary.cpp`
  - `Content/Python/gamexxk_capture_hud_layer_audit.py`
- Remove validated temporary artifact: `D:\GameXXKCaptureAudit.py`
- Evidence: `Saved/HarnessReports/`
- Package: `Saved/Packaged/DesktopOverlayDevelopment/`

- [ ] **Step 1: Remove only this task's editor capture probe**

Delete the `CaptureDesktopHudLayerAudit` UFUNCTION/implementation and its RenderCore-only editor dependency if no other editor code needs it. Delete the one-shot Python launcher and saved analysis helper. Do not revert any unrelated editor-capture functionality or user changes.

- [ ] **Step 2: Remove the stale abandoned plugin artifact safely**

Resolve and verify that `Plugins\GameXXKDesktopComposition` contains only the obsolete empty directories/PDB from the abandoned prototype and is inside the project `Plugins` directory. Remove only that exact resolved directory; do not touch `GameXXKDesktopOverlay` or `Quick_Road`.

- [ ] **Step 3: Cold-build without Live Coding/Hot Reload**

If the editor is open, save through UE MCP and close safely. Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 240
```

Expected: `GameXXKEditor Win64 Development` and the new plugin build successfully.

- [ ] **Step 4: Run the complete Workbench regression**

```powershell
& scripts\run_mvp_test_suites.ps1 `
  -Suites @('GameXXK.DesktopTraining.Workbench') `
  -TimeoutSeconds 600
```

Expected: zero failed and zero errors, including the new background, provider, native-region, town-layout, snapshot and map-target contracts.

- [ ] **Step 5: Verify the real standalone desktop overlay**

Launch the canonical map with `UnrealEditor.exe -game`, not PIE. Place a white, gray, and dark application surface behind the overlay and capture each state. Verify:

- no full-canvas paper or black rectangle;
- opaque colors match the ordinary Slate reference;
- no bright, dark, or colored halo on mountain, character, text, chest, button or circular-town-icon edges;
- empty gaps click through to the application behind;
- the strip drags, Tab works, every button works, and close restores input immediately.

- [ ] **Step 6: Verify the 60-toggle memory budget**

Run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File scripts\measure_desktop_overlay_cycles.ps1
```

Expected: no monotonic growth; after cooldown private memory is no more than 32 MiB above the warmed baseline. Record actual collapsed/expanded GPU values instead of reporting estimates as measurements.

- [ ] **Step 7: Verify the real town round trip**

Open backpack, warehouse, a non-default page/tab and character, then click the circular button. Verify the existing Qingshan map loads, the same state is restored at the default fixed HUD position, dragging is disabled, controls work, the text reads `退出 / 城镇`, and clicking it returns to the desktop with stable state intact.

- [ ] **Step 8: Package Development Win64**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
  -project='D:\UE5 demo\GameXXK\GameXXK.uproject' `
  -noP4 -platform=Win64 -clientconfig=Development `
  -build -cook -stage -pak -archive `
  -archivedirectory='D:\UE5 demo\GameXXK\Saved\Packaged\DesktopOverlayDevelopment'
```

Expected: package succeeds and contains the plugin DLL, desktop map, Qingshan map and town-toggle texture. Repeat the desktop/town/input checks in the packaged executable.

- [ ] **Step 9: Compare engine hashes**

Regenerate `desktop-overlay-engine-hashes-after.json` from the same six paths and compare the JSON hash values to the before report. Expected: every SHA256 is identical.

- [ ] **Step 10: Leave the correct project surface ready for user verification**

Open `GameXXK.uproject` on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start the appropriate direct-test game window, and leave the HUD at the collapsed canonical state unless a failure requires inspection. Report exact build, automation, package, memory, edge-color and round-trip evidence; do not claim success from tests that were not run.
