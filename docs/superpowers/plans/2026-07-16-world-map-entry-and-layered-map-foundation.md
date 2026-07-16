# World Map Entry and Layered Map Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a new game stop on a real, PSD-styled, clickable world map before entering QingShan Town, and establish owned separated map assets without using the PSD's baked map as a runtime interaction background.

**Architecture:** Extend the existing `UGameXXKWorldMapWidget` into the production full-screen UMG page, but keep region selection flowing through `GameXXKMVPCommandRouter` so travel, saving, and runtime rules remain centralized. Create a GameXXK-owned map-art import pipeline; the map terrain is a clean noninteractive layer, while markers, labels, lock state, and input are separate runtime widgets.

**Tech Stack:** UE 5.8 C++/UMG, existing MVP command router and level flow, built-in UE MCP import/save tools, ImageGen for missing non-text art, local Pillow/JSON validation only, Unreal automation tests, UBT cold build.

---

## File structure

- Create: `docs/ui/maps/source_art/WorldMap/*` — generated, reviewed non-text source atoms; never copy `094.png` into this directory.
- Create: `docs/ui/maps/source_art/WorldMap/manifest.json` — dimensions, alpha, provenance and semantic layer names for each generated atom.
- Create: `scripts/validate_map_ui_source_art.py` — validates the source atoms and manifest; it never generates or crops art.
- Create: `Content/Python/gamexxk_import_map_ui_assets.py` — imports only GameXXK-owned map textures to `/Game/GameXXK/UI/Maps/Textures`.
- Create: `Content/Python/gamexxk_validate_map_ui_assets.py` — UE-side `Texture2D` and settings verification.
- Modify: `Source/GameXXK/Public/UI/GameXXKWorldMapWidget.h` — production layout API, widget references and test helpers.
- Modify: `Source/GameXXK/Private/UI/GameXXKWorldMapWidget.cpp` — layered map composition and routed region selection.
- Modify: `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp` — stop new game at WorldMap and notify the owned player-flow widgets.
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` — make the map navigation invoke `OpenWorldMap` rather than display an unavailable-map notice.
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` — world-map class/member/getters.
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — create, refresh, focus and viewport-own the world map widget.
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp` — make `OpenWorldMap` travel through `GameXXKLevelFlow`.
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp` — specify menu-to-world-map and layered-widget behavior.
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp` — specify player-controller ownership/visibility/input flow.
- Modify: `scripts/gamexxk_real_play_flow_mcp.py` — change the first gameplay assertion to Start → WorldMap → QingShan.

## Asset contract

Runtime asset roots and names are fixed before any C++ brush path is written:

| Source atom | Runtime object path | Role |
| --- | --- | --- |
| `WorldMap/world_map_terrain.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_Terrain` | scenery only: no route, dot, skull, marker, label or player icon |
| `WorldMap/world_map_region_paths.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_RegionPaths` | optional decorative relationship lines only, no stateful dots |
| `WorldMap/world_map_qingshan_marker.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_QingshanMarker` | selectable QingShan marker, transparent background |
| `WorldMap/world_map_locked_marker.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_LockedMarker` | disabled location marker, transparent background |
| `WorldMap/world_map_player_marker.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_PlayerMarker` | optional current-position marker, transparent background |
| `WorldMap/world_map_label_plate.png` | `/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_RegionLabelPlate` | non-text paper label backing, transparent background |

All region names, lock notices and current-region text are UMG text. The known PSD `clean_assets_v2/094.png` stays outside the project as a visual reference only. `095.png` can inform the player-marker composition but must be copied/re-authored under the source-art root and recorded in the new manifest.

### Task 1: Establish source-art provenance and validation before importing assets

**Files:**
- Create: `docs/ui/maps/source_art/WorldMap/manifest.json`
- Create: `scripts/validate_map_ui_source_art.py`

- [ ] **Step 1: Generate the six non-text WorldMap atoms with ImageGen, before any local image processing.**

Use the PSD map only as geometry/style reference and create each requested layer independently. Use this prompt for the terrain atom:

```text
Create a square Chinese ink-wash martial-arts world-map terrain layer matching the supplied reference map's parchment, mountains, rivers and muted olive/ink palette. Preserve the same broad geographic composition, but remove every route, dotted line, circular waypoint, skull, settlement marker, player marker, UI frame, and all text. The result is a clean scenery-only map background with no interactive information baked in.
```

Use this prompt for every marker atom, changing only the named object:

```text
Create one isolated game UI atom: <Qingshan town marker | locked town marker | player location marker | blank parchment region label plate | decorative noninteractive regional path line>. Chinese ink-and-parchment martial-arts style, no words or numbers, centered object, transparent background, no full map, no frame, no unrelated UI.
```

Save only the accepted PNG results under `docs/ui/maps/source_art/WorldMap/` with the six filenames in the asset contract. Do not use Pillow, OpenCV, manual cropping, alpha extraction, or the baked `094.png` to manufacture the art.

- [ ] **Step 2: Create the manifest after ImageGen output exists.**

Write `manifest.json` with this required structure and actual generated dimensions/checksums:

```json
{
  "canvas": { "width": 1920, "height": 1080 },
  "sourceReference": {
    "path": "PSD clean_assets_v2/094.png",
    "usage": "visual reference only; not a runtime source and not a source-art layer"
  },
  "layers": [
    { "name": "world_map_terrain", "file": "world_map_terrain.png", "kind": "terrain", "requiresAlpha": false, "forbiddenContent": ["route", "node", "label", "player_marker"] },
    { "name": "world_map_region_paths", "file": "world_map_region_paths.png", "kind": "decorative_path", "requiresAlpha": true, "forbiddenContent": ["node", "label", "player_marker"] },
    { "name": "world_map_qingshan_marker", "file": "world_map_qingshan_marker.png", "kind": "marker", "requiresAlpha": true },
    { "name": "world_map_locked_marker", "file": "world_map_locked_marker.png", "kind": "marker", "requiresAlpha": true },
    { "name": "world_map_player_marker", "file": "world_map_player_marker.png", "kind": "marker", "requiresAlpha": true },
    { "name": "world_map_label_plate", "file": "world_map_label_plate.png", "kind": "label_plate", "requiresAlpha": true }
  ]
}
```

- [ ] **Step 3: Write the failing source-art validator.**

Create a Python script that accepts `--check` and rejects missing layers, a forbidden `094.png` filename in any runtime layer, non-RGBA marker atoms, and markers without transparent pixels. The provenance field may mention `094.png` only when it explicitly says it is reference-only and not a runtime source. It may inspect images and write a JSON report but must never redraw pixels. The core validation must be:

```python
def validate_layer(root: Path, layer: dict[str, object]) -> dict[str, object]:
    file_name = str(layer["file"])
    if file_name == "094.png" or "完整底图" in file_name:
        raise ValueError("baked PSD map cannot be imported as runtime source")
    image_path = root / file_name
    if not image_path.is_file():
        raise ValueError(f"missing map source atom: {image_path}")
    with Image.open(image_path) as raw:
        image = raw.convert("RGBA")
        alpha = image.getchannel("A")
        if layer.get("requiresAlpha") and alpha.getextrema()[0] >= 255:
            raise ValueError(f"transparent atom has no transparent pixels: {image_path.name}")
        return {"file": file_name, "size": list(image.size), "alpha": list(alpha.getextrema())}
```

- [ ] **Step 4: Run the validator after assets are generated.**

Run:

```powershell
python scripts\validate_map_ui_source_art.py --check
```

Expected: JSON with `"ok": true`, six source atoms, and no `094.png` in any runtime layer.

- [ ] **Step 5: Commit the reviewed source contract.**

```powershell
git add docs/ui/maps/source_art/WorldMap scripts/validate_map_ui_source_art.py
git commit -m "assets: add layered world map source contract"
```

### Task 2: Import the owned world-map atoms through UE MCP

**Files:**
- Create: `Content/Python/gamexxk_import_map_ui_assets.py`
- Create: `Content/Python/gamexxk_validate_map_ui_assets.py`

- [ ] **Step 1: Implement a dedicated importer, not a modification of the Town importer.**

Mirror the texture setup of `gamexxk_import_town_ui_assets.py`, but use these constants and tuples:

```python
SOURCE_ROOT = PROJECT_ROOT / "docs" / "ui" / "maps" / "source_art"
DESTINATION_ROOT = "/Game/GameXXK/UI/Maps/Textures"
IMPORTS = (
    ("WorldMap", "world_map_terrain.png", "T_WorldMap_Terrain"),
    ("WorldMap", "world_map_region_paths.png", "T_WorldMap_RegionPaths"),
    ("WorldMap", "world_map_qingshan_marker.png", "T_WorldMap_QingshanMarker"),
    ("WorldMap", "world_map_locked_marker.png", "T_WorldMap_LockedMarker"),
    ("WorldMap", "world_map_player_marker.png", "T_WorldMap_PlayerMarker"),
    ("WorldMap", "world_map_label_plate.png", "T_WorldMap_RegionLabelPlate"),
)
```

`configure_texture()` must set `TMGS_NO_MIPMAPS`, `TC_EDITOR_ICON`, `TEXTUREGROUP_UI`, bilinear filter, clamp X/Y, sRGB, never-stream and `compression_no_alpha = False`. Each asset must be saved individually after import.

- [ ] **Step 2: Implement the UE-side validator before running the importer.**

For each expected object path, load the asset and require `unreal.Texture2D`, UI LOD group, no mipmaps, clamp addressing and retained alpha. Reject an imported asset whose source path contains `094.png` or whose object path begins `/Game/1Game/`.

- [ ] **Step 3: Run the importer through the supported UE MCP client.**

Against a running UE 5.8 editor, invoke the project's MCP client API with the exact-asset filter, then save dirty packages through that same client session:

```powershell
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); result=c.run_project_python_file('Content/Python/gamexxk_import_map_ui_assets.py',['--assets','T_WorldMap_Terrain','T_WorldMap_RegionPaths','T_WorldMap_QingshanMarker','T_WorldMap_LockedMarker','T_WorldMap_PlayerMarker','T_WorldMap_RegionLabelPlate']); print(json.dumps(result, ensure_ascii=False)); print(json.dumps(c.save_dirty_packages(), ensure_ascii=False))"
```

Expected: six `Texture2D` objects below `/Game/GameXXK/UI/Maps/Textures/WorldMap` and no modified `/Game/1Game` objects.

- [ ] **Step 4: Run the UE-side validation.**

```powershell
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_validate_map_ui_assets.py'), ensure_ascii=False))"
```

Expected: JSON `"ok": true` and all six paths reported as UI textures.

- [ ] **Step 5: Commit the importer and map texture assets.**

```powershell
git add Content/Python/gamexxk_import_map_ui_assets.py Content/Python/gamexxk_validate_map_ui_assets.py Content/GameXXK/UI/Maps/Textures
git commit -m "assets: import owned world map ui layers"
```

### Task 3: Add failing tests for Start → WorldMap → QingShan

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Change the existing menu expectation to world map before code changes.**

Replace the old direct-town assertions in `FGameXXKMVPUIWidgetTest::RunTest` with:

```cpp
TestTrue(TEXT("main menu start creates a new game"), MainMenu->StartGame());
TestEqual(TEXT("main menu start remains on world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
WorldMap->TakeWidget();
TestTrue(TEXT("Qingshan is initially selectable"), WorldMap->IsRegionEnabledForTest(UGameXXKMVPRules::RegionQingshan()));
TestFalse(TEXT("Tanjiang is not a playable town target yet"), WorldMap->IsRegionEnabledForTest(UGameXXKMVPRules::RegionTanjiang()));
TestFalse(TEXT("locked or unavailable town selection is rejected"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionTanjiang()));
TestFalse(TEXT("unavailable town selection explains why"), WorldMap->GetSelectionNoticeForTest().IsEmpty());
TestTrue(TEXT("Qingshan selection enters town"), WorldMap->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
TestEqual(TEXT("Qingshan selection changes screen to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
```

- [ ] **Step 2: Specify widget structure and source-owned texture bindings.**

After `WorldMap->TakeWidget()`, assert the named terrain, QingShan button and notice widgets exist, and assert their brushes resolve only the GameXXK-owned paths:

```cpp
TestNotNull(TEXT("world map builds terrain image"), WorldMap->GetWidgetFromName(TEXT("WorldMapTerrain")));
TestNotNull(TEXT("world map builds Qingshan button"), WorldMap->GetWidgetFromName(TEXT("WorldMapRegionQingshan")));
TestNotNull(TEXT("world map builds selection notice"), WorldMap->GetWidgetFromName(TEXT("WorldMapSelectionNotice")));
TestTrue(TEXT("world map terrain is GameXXK owned"),
    WorldMap->GetTerrainResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_Terrain")));
TestFalse(TEXT("world map never resolves 1Game background"),
    WorldMap->GetTerrainResourcePathForTest().Contains(TEXT("/Game/1Game/")));
```

- [ ] **Step 3: Add controller ownership expectations.**

In `FGameXXKPlayerControllerOwnsFlowWidgetsTest`, add:

```cpp
TestNotNull(TEXT("controller owns a world map widget"), PlayerController->GetWorldMapWidgetForTest());
TestTrue(TEXT("new game reaches world map"), Subsystem->StartGame());
PlayerController->RefreshPlayerFlowWidgetsForTest();
TestTrue(TEXT("world map is visible after new game"), PlayerController->GetWorldMapWidgetForTest()->IsWorldMapVisibleForTest());
TestTrue(TEXT("world map exists in the viewport on a game world"), PlayerController->HasWorldMapWidgetInViewportForTest());
TestTrue(TEXT("controller-routed Qingshan click succeeds"), PlayerController->GetWorldMapWidgetForTest()->TrySelectRegion(UGameXXKMVPRules::RegionQingshan()));
TestEqual(TEXT("controller-routed Qingshan click enters town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
```

- [ ] **Step 4: Run the focused automation test to observe the red state.**

Save packages through UE MCP, close the editor through the supported pipeline, cold-build, then run:

```powershell
$env:UE-LocalDataCachePath='D:\UE5 demo\UnrealEngineCache'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development 'D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.WidgetBasesDriveRules+GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected before implementation: either compile failure for the new WorldMap test helpers or assertion failure because the menu still enters Town.

### Task 4: Correct state transitions and command travel

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`

- [ ] **Step 1: Stop `UGameXXKMainMenuWidget::StartGame()` from auto-selecting QingShan.**

Replace its start expression and refresh block with:

```cpp
UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
const bool bStarted = Subsystem && Subsystem->StartGame();
if (bStarted)
{
    RequestPlayableMapForRuntimeState();
    SetMenuLayer(EGameXXKMainMenuLayer::Landing);
    OnStartGameSucceeded();
    if (!NotifyPlayerFlowStateChanged())
    {
        RefreshFromState();
    }
}
return bStarted;
```

`RequestPlayableMapForRuntimeState()` remains Town-only, so a WorldMap new game stays on `L_Main` and can render the map UMG.

- [ ] **Step 2: Ensure `OpenWorldMap` travels from Town back to `L_Main`.**

In `GameXXKMVPCommandRouter::ExecuteVisibleCommand`, replace the current `OpenWorldMap` branch with:

```cpp
if (CommandName == OpenWorldMap)
{
    return FinishCommandAndTravel(Subsystem, Subsystem->OpenWorldMap());
}
```

This retains command-router ownership of state changes and makes the later HUD map button leave the town level correctly.

- [ ] **Step 3: Replace the town HUD's unavailable-map response with the routed command.**

In `UGameXXKTownHudWidget::HandleMapClicked`, execute `OpenWorldMap` through `GameXXKMVPCommandRouter::ExecuteVisibleCommand`, then call `NotifyPlayerFlowStateChanged()` and refresh fallback state. Do not set `MapNoticeText` to “暂未开放”; retain a failure message only if the command returns false.

- [ ] **Step 4: Re-run the focused automation test.**

Run the command from Task 3, Step 4. Expected: compilation succeeds; the menu now remains in `WorldMap`, while WorldMap layout assertions still fail until Task 5.

- [ ] **Step 5: Commit the corrected state flow.**

```powershell
git add Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
git commit -m "feat: route menu and town map navigation through world map"
```

### Task 5: Build and own the production WorldMap widget

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKWorldMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKWorldMapWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Add the WorldMap widget API and references declared by Task 3.**

Add these declarations to `UGameXXKWorldMapWidget`:

```cpp
virtual TSharedRef<SWidget> RebuildWidget() override;
virtual void NativeConstruct() override;

UFUNCTION(BlueprintCallable, Category = "GameXXK|WorldMap")
void RefreshFromState();

UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
bool IsWorldMapVisibleForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
bool IsRegionEnabledForTest(FName RegionId) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
FText GetSelectionNoticeForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
FString GetTerrainResourcePathForTest() const;
```

Store `UCanvasPanel* RootCanvas`, `UImage* TerrainImage`, `UButton* QingshanButton`, `UButton* TanjiangButton`, `UTextBlock* SelectionNoticeText`, `FText SelectionNotice`, and a `TMap<FName, TObjectPtr<UButton>> RegionButtons`. Add private `BuildProgrammaticLayout`, `RefreshProgrammaticLayout`, `HasPlayableTarget`, `HandleQingshanClicked` and `HandleTanjiangClicked` methods.

- [ ] **Step 2: Build the layered UMG hierarchy with named, nonblocking layers.**

`BuildProgrammaticLayout()` must create these canvas children, in ascending visual order: `WorldMapTerrain` (hit-test invisible), `WorldMapRegionPaths` (hit-test invisible), `WorldMapPlayerMarker` (hit-test invisible), `WorldMapRegionQingshan` (button), `WorldMapRegionTanjiang` (button), three `TextBlock` labels, and `WorldMapSelectionNotice`. All source brushes must use the Task 2 GameXXK-owned object paths. The terrain contains no labels or click behavior; only buttons own input.

Use fixed design-space placement inside a centered 1920×1080 canvas so marker positions remain proportional when the fullscreen viewport slot scales. For example:

```cpp
AddCanvasChild(RootCanvas, TerrainImage, FVector2D(230.0f, 70.0f), FVector2D(1460.0f, 900.0f));
AddCanvasChild(RootCanvas, QingshanButton, FVector2D(590.0f, 375.0f), FVector2D(86.0f, 86.0f));
AddCanvasChild(RootCanvas, TanjiangButton, FVector2D(1110.0f, 605.0f), FVector2D(86.0f, 86.0f));
```

Region text uses live `FText::FromString` labels, never the generated art.

- [ ] **Step 3: Route a selectable region through the existing command router.**

Implement the core of `TrySelectRegion` as follows:

```cpp
LastSelectedRegion = RegionId;
bLastSelectionUnlocked = false;
UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::WorldMap)
{
    SelectionNotice = NSLOCTEXT("GameXXKWorldMap", "WrongScreen", "请先进入世界地图。");
    RefreshProgrammaticLayout();
    return false;
}
if (!HasPlayableTarget(RegionId))
{
    SelectionNotice = NSLOCTEXT("GameXXKWorldMap", "UnavailableTown", "该城镇暂未开放。");
    OnLockedRegionSelected(RegionId);
    RefreshProgrammaticLayout();
    return false;
}
const bool bSucceeded = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, TEXT("SelectQingshan"));
bLastSelectionUnlocked = bSucceeded;
SelectionNotice = bSucceeded ? FText::GetEmpty() : NSLOCTEXT("GameXXKWorldMap", "LockedTown", "该城镇尚未解锁。");
if (bSucceeded) { OnUnlockedRegionSelected(RegionId); }
else { OnLockedRegionSelected(RegionId); }
if (!NotifyPlayerFlowStateChanged()) { RefreshFromState(); }
return bSucceeded;
```

`HasPlayableTarget` returns true only for `UGameXXKMVPRules::RegionQingshan()` in this release. This intentionally displays a discovered Tanjiang as unavailable until it owns a town level.

- [ ] **Step 4: Add WorldMap ownership to the player controller.**

Add `UGameXXKWorldMapWidget` forward declaration, `WorldMapWidgetClass`, `WorldMapWidget`, `EnsureWorldMapWidget`, `GetWorldMapWidgetForTest` and `HasWorldMapWidgetInViewportForTest`. Construct with the same `CreateWidget`/`NewObject` fallback as RouteMap, add it at Z-order 45 and configure the existing full-screen viewport slot helper.

Call `WorldMapWidget->SetMVPSubsystem(Subsystem)` and `RefreshFromState()` from `RefreshPlayerFlowWidgets`; include it in the success return condition. In `ApplyPlayerFlowInputMode`, before the DungeonMap branch, focus it when `ActiveScreen == EGameXXKScreen::WorldMap`. The widget's own `RefreshFromState` sets visibility to `Visible` only for WorldMap and `Collapsed` otherwise.

- [ ] **Step 5: Run the two focused tests for green.**

Run the Task 3 command. Expected: both tests report `Result={Success}`, WorldMap terrain uses only `/Game/GameXXK/UI/Maps/Textures/WorldMap`, Start stops at WorldMap, and QingShan click enters Town.

- [ ] **Step 6: Commit the production world map.**

```powershell
git add Source/GameXXK/Public/UI/GameXXKWorldMapWidget.h Source/GameXXK/Private/UI/GameXXKWorldMapWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp
git commit -m "feat: add playable world map entry screen"
```

### Task 6: Update real-flow evidence and perform cold verification

**Files:**
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Make the harness wait for a world map after Start.**

Replace the `StartGame opens Qingshan town map` wait with a probe requirement that `RuntimeState.Screen` is `WorldMap`, `L_Main` remains active, and the production WorldMap widget is visible. Add a second action that clicks the QingShan marker and waits for `L_Qingshan_AsianVillage_Demo` plus `Town` state before the existing F-quest steps.

- [ ] **Step 2: Cold-build and run all relevant tests.**

```powershell
$env:UE-LocalDataCachePath='D:\UE5 demo\UnrealEngineCache'
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 300
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.WidgetBasesDriveRules+GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets+GameXXK.MVP.PIE.MainMenuContinueWorldMap;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
python scripts\gamexxk_real_play_flow_mcp.py --timeout 45
```

Expected: no Live Coding/Hot Reload, successful UBT build, all targeted automation tests pass, and the harness captures world-map then town screenshots before the existing route/battle assertions.

- [ ] **Step 3: Commit the real-flow update.**

```powershell
git add scripts/gamexxk_real_play_flow_mcp.py
git commit -m "test: verify world map before town entry"
```

## Plan self-review

- The plan covers the design's first end-to-end change: clean source atoms, imported UE textures, main-menu state correction, routed city selection, owned world-map UMG, town return navigation, and end-to-end validation.
- It does not import the baked PSD map or replace `/Game/1Game` content. The following plan must restyle the dynamic route map with its own source-art contract while preserving its generated nodes, lines and hitboxes.
- Each behavioral change has a red test before implementation and a targeted green command after implementation.
