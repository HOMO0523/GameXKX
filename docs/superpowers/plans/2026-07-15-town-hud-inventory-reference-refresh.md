# Town HUD and Backpack Reference Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the remaining placeholder town HUD bars and legacy ImageGen backpack shell with direct, dynamic-safe crops from the approved Tencent UI reference while keeping all inventory behavior functional.

**Architecture:** A deterministic Pillow cutter reads only `docs/reference-assets/2026-07-14-tencent-town-ui-source-2.png` and produces reusable UI atoms under `docs/ui/town/source_art`.  The existing Unreal importer brings those atoms into the isolated Town texture tree.  The HUD uses two `UProgressBar` widgets driven by runtime HP and level XP; the inventory window keeps its current interaction model but changes only its visual brush paths and layout primitives.

**Tech Stack:** Python/Pillow image cutting, Unreal Editor Python import through UE MCP, UE 5.8 UMG/C++, existing Unreal automation tests, UBT cold build.

---

## File structure

- Create: `scripts/cut_town_hud_backpack_reference.py` — deterministic direct-reference crop and nine-slice atom generator.
- Modify: `Content/Python/gamexxk_import_town_ui_assets.py` — imports the new HUD and Backpack atoms into `/Game/GameXXK/UI/Town/Textures`.
- Modify: `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h` — stores HUD progress-bar references.
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` — binds HP/XP bars to runtime state.
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp` — switches visual shell paths and uses the direct-reference paper/slot/action assets.
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp` — guards the dynamic HUD bars and Town-backpack brush bindings.

The two UI surfaces share one authoritative reference, one import pipeline, and one widget-level automation path, so they remain in this single focused plan.  Art cutting itself is intentionally not run through a TDD loop per the approved art workflow; only behavior/binding code receives a single red-green automation check.

### Task 1: Establish the behavior regression check before changing widget code

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [ ] **Step 1: Add the required widget test imports and HUD fixture**

Add the following imports beside the existing UMG and UI imports:

```cpp
#include "Components/ProgressBar.h"
#include "UI/GameXXKTownHudWidget.h"
```

Create and bind the widget next to `InventoryWindow`:

```cpp
UGameXXKTownHudWidget* TownHud = NewObject<UGameXXKTownHudWidget>();
TownHud->SetMVPSubsystem(Subsystem);
TownHud->TakeWidget();
```

- [ ] **Step 2: Specify the expected direct-reference visual bindings**

After the current free-inventory assertions, add these checks:

```cpp
TestTrue(TEXT("free inventory window frame uses Tencent backpack frame"),
    InventoryWindow->GetWindowFrameResourcePathForTest().Contains(
        TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame")));
TestTrue(TEXT("free inventory backpack slots use Tencent backpack slot frame"),
    InventoryWindow->GetBackpackSlotResourcePathForTest().Contains(
        TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_Slot")));
TestTrue(TEXT("free inventory equipment slots use Tencent backpack slot frame"),
    InventoryWindow->GetEquipmentSlotResourcePathForTest().Contains(
        TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_Slot")));
```

Seed a partial-health/partial-XP player, call `RefreshFromState`, and require both named bars and their live percentages:

```cpp
Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
Subsystem->GetMutableRuntimeState().PlayerHP = 25;
Subsystem->GetMutableRuntimeState().PlayerMaxHP = 100;
Subsystem->GetMutableRuntimeState().PlayerXP = 75;
Subsystem->GetMutableRuntimeState().PlayerLevel = 1;
TownHud->RefreshFromState();
UProgressBar* HealthBar = Cast<UProgressBar>(TownHud->GetWidgetFromName(TEXT("TownHudHealthBar")));
UProgressBar* ExperienceBar = Cast<UProgressBar>(TownHud->GetWidgetFromName(TEXT("TownHudExperienceBar")));
TestNotNull(TEXT("town HUD builds a named health progress bar"), HealthBar);
TestNotNull(TEXT("town HUD builds a named experience progress bar"), ExperienceBar);
if (HealthBar && ExperienceBar)
{
    TestEqual(TEXT("town HUD health fill follows PlayerHP"), HealthBar->GetPercent(), 0.25f);
    TestEqual(TEXT("town HUD experience fill follows current-level XP"), ExperienceBar->GetPercent(), 0.75f);
}
```

- [ ] **Step 3: Cold-build and run the targeted test to observe the expected red state**

Save all dirty editor packages through UE MCP, close the editor through the supported pipeline, then run:

```powershell
$env:UE-LocalDataCachePath='D:\UE5 demo\UnrealEngineCache'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development 'D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.WidgetBasesDriveRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result before implementation: the test compiles, then fails the old Inventory texture-path assertions and the missing named HUD bar assertions.

### Task 2: Cut dynamic-safe atoms from the approved reference image

**Files:**
- Create: `scripts/cut_town_hud_backpack_reference.py`
- Create (generated): `docs/ui/town/source_art/HUD/hud_health_bar_frame.png`
- Create (generated): `docs/ui/town/source_art/HUD/hud_health_bar_fill.png`
- Create (generated): `docs/ui/town/source_art/HUD/hud_experience_bar_frame.png`
- Create (generated): `docs/ui/town/source_art/HUD/hud_experience_bar_fill.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_window_frame.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_header.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_back_arrow.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_slot.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_action_blank.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_tab_all.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_tab_equipment.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_tab_prop.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_tab_material.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_tab_task.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_button_sort.png`
- Create (generated): `docs/ui/town/source_art/Backpack/backpack_button_disassemble.png`

- [ ] **Step 1: Define only direct source coordinates**

The source is exactly `1024x1024`.  Use these `x, y, width, height` donor regions:

```python
HUD_BARS = {
    "experience": {
        "fill": (388, 502, 54, 4),
        "track": (450, 502, 39, 4),
    },
    "health": {
        "fill": (388, 523, 102, 4),
        "track": (450, 502, 39, 4),
    },
}
BACKPACK = {
    "header": (656, 720, 120, 36),
    "back_arrow": (664, 731, 32, 24),
    "slot": (658, 789, 61, 56),
    "sort": (823, 977, 73, 31),
    "disassemble": (914, 977, 73, 31),
    "tab_all": (700, 758, 57, 29),
    "tab_equipment": (761, 758, 59, 29),
    "tab_prop": (824, 758, 57, 29),
    "tab_material": (887, 758, 57, 29),
    "tab_task": (949, 758, 57, 29),
}
```

- [ ] **Step 2: Generate the progress-bar frame/fill pairs without baked values**

For each HUD donor, create a `110x10` RGBA bar from only its direct colored fill core and a neutral track core.  Place the generated fill/track inside a transparent canvas at the shared bar interior.  This makes UMG clipping alter only the colored fill, with no sample values or full-health color baked underneath.

- [ ] **Step 3: Assemble generic backpack atoms only from source pixels**

Build `backpack_window_frame.png` from these dynamic-safe source donors: top-right corner `(1000,720,24,24)`, top edge `(760,720,160,12)`, right edge `(1014,800,10,170)`, bottom-right corner `(1000,1000,24,24)`, bottom edge `(760,1010,150,14)`, and plain paper interior `(992,800,16,160)`.  Mirror right-side donors for left-side use, tile the edges, and tile the plain interior.  This produces a clean 9-slice source with no sample backpack items, counts, title, capacity, or action labels.

For `backpack_slot.png`, retain only the 5-pixel outer ring from the direct slot crop, then fill its inner rectangle with the direct plain-paper donor.  For `backpack_action_blank.png`, retain the direct sort-button border and fill its text center with sampled sort-button paper so runtime labels can be drawn without baked Chinese text behind them.

- [ ] **Step 4: Validate generated image invariants once**

Run:

```powershell
python scripts/cut_town_hud_backpack_reference.py --check
```

Expected output: JSON with every listed output present, RGBA mode, non-zero size, and no opaque source sample text/item region in the generated frame/slot/fill assets.

### Task 3: Import the atoms into the Town texture tree

**Files:**
- Modify: `Content/Python/gamexxk_import_town_ui_assets.py`

- [ ] **Step 1: Add the four HUD imports**

Append these import tuples:

```python
("HUD", "hud_health_bar_frame.png", "T_TownHUD_HealthBarFrame"),
("HUD", "hud_health_bar_fill.png", "T_TownHUD_HealthBarFill"),
("HUD", "hud_experience_bar_frame.png", "T_TownHUD_ExperienceBarFrame"),
("HUD", "hud_experience_bar_fill.png", "T_TownHUD_ExperienceBarFill"),
```

- [ ] **Step 2: Add the new Backpack imports**

Append these import tuples:

```python
("Backpack", "backpack_window_frame.png", "T_TownBackpack_WindowFrame"),
("Backpack", "backpack_header.png", "T_TownBackpack_Header"),
("Backpack", "backpack_back_arrow.png", "T_TownBackpack_BackArrow"),
("Backpack", "backpack_slot.png", "T_TownBackpack_Slot"),
("Backpack", "backpack_action_blank.png", "T_TownBackpack_ActionBlank"),
```

Keep the seven tab/action asset names already in the importer and overwrite only their source PNG files with the direct crops.  Do not alter anything under `/Game/GameXXK/UI/Inventory/Textures`.

- [ ] **Step 3: Run the importer through the running UE 5.8 editor**

Run `Content/Python/gamexxk_import_town_ui_assets.py --assets T_TownHUD_HealthBarFrame T_TownHUD_HealthBarFill T_TownHUD_ExperienceBarFrame T_TownHUD_ExperienceBarFill T_TownBackpack_WindowFrame T_TownBackpack_Header T_TownBackpack_BackArrow T_TownBackpack_Slot T_TownBackpack_ActionBlank T_TownBackpack_TabAll T_TownBackpack_TabEquipment T_TownBackpack_TabProp T_TownBackpack_TabMaterial T_TownBackpack_TabTask T_TownBackpack_ButtonSort T_TownBackpack_ButtonDisassemble` through `scripts/ue_mcp_client.py`, then call the MCP dirty-package save.  Each `import_texture` saves only its resulting `Texture2D`; the importer deliberately does not resave a whole source group. The exact-asset filter keeps unrelated Town UI atoms untouched. Expected result: every listed object exists as a `Texture2D`, has `TEXTUREGROUP_UI`, `TMGS_NO_MIPMAPS`, clamp addressing, and remains in the Town tree.

### Task 4: Bind the live HUD bars

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`

- [ ] **Step 1: Add the `UProgressBar` forward declaration and retained references**

```cpp
class UProgressBar;
// ...
UPROPERTY(Transient)
TObjectPtr<UProgressBar> HealthBar;

UPROPERTY(Transient)
TObjectPtr<UProgressBar> ExperienceBar;
```

- [ ] **Step 2: Add texture paths and a progress-bar style helper**

```cpp
const FString HealthBarFrameTexturePath(HudTextureRoot + TEXT("T_TownHUD_HealthBarFrame.T_TownHUD_HealthBarFrame"));
const FString HealthBarFillTexturePath(HudTextureRoot + TEXT("T_TownHUD_HealthBarFill.T_TownHUD_HealthBarFill"));
const FString ExperienceBarFrameTexturePath(HudTextureRoot + TEXT("T_TownHUD_ExperienceBarFrame.T_TownHUD_ExperienceBarFrame"));
const FString ExperienceBarFillTexturePath(HudTextureRoot + TEXT("T_TownHUD_ExperienceBarFill.T_TownHUD_ExperienceBarFill"));

FProgressBarStyle MakeProgressStyle(const FString& FramePath, const FString& FillPath, const FVector2D& Size)
{
    FProgressBarStyle Style;
    Style.SetBackgroundImage(MakeTextureBrush(FramePath, Size));
    Style.SetFillImage(MakeTextureBrush(FillPath, Size));
    return Style;
}
```

- [ ] **Step 3: Build named bars and refresh their live values**

Create `TownHudHealthBar` and `TownHudExperienceBar` with the style helper, `EProgressBarFillType::LeftToRight`, and a `110x10` size.  Place them below the level label without covering the portrait or navigation rail.  During `RefreshFromState`, use:

```cpp
HealthBar->SetPercent(FMath::Clamp(
    static_cast<float>(State->PlayerHP) / static_cast<float>(FMath::Max(1, State->PlayerMaxHP)), 0.0f, 1.0f));
ExperienceBar->SetPercent(FMath::Clamp(
    static_cast<float>(State->PlayerXP) / static_cast<float>(FMath::Max(1, State->PlayerLevel * 100)), 0.0f, 1.0f));
```

Retain dynamic level/XP/power text; no reference number is baked into either bar.

### Task 5: Switch the inventory window to the Town-reference shell without changing actions

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`

- [ ] **Step 1: Introduce only Town backpack texture constants**

Replace the old frame, confirmation, close, slot, equipment, and generic-action paths with Town equivalents:

```cpp
const FString WindowFrameTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
const FString ConfirmationDialogTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
const FString CloseButtonTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_BackArrow.T_TownBackpack_BackArrow"));
const FString BackpackSlotTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_Slot.T_TownBackpack_Slot"));
const FString EquipmentSlotTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_Slot.T_TownBackpack_Slot"));
const FString ActionButtonTexturePath(TownBackpackTextureRoot + TEXT("T_TownBackpack_ActionBlank.T_TownBackpack_ActionBlank"));
```

Keep `T_InventorySlotStates` and all runtime item icon paths unchanged; they are interaction overlays, not the old visual shell.

- [ ] **Step 2: Use nine-slice drawing for all paper panels**

Add a box-brush helper that sets `DrawAs = ESlateBrushDrawType::Box` and uses `FMargin(0.0625f)`.  Use it for `WindowFrame`, `LeftRailFrame`, `BackpackFrame`, `DetailPanelFrame`, and `ConfirmationDialogFrame`.  The 9-slice outer ink line remains crisp while the plain source-paper center expands.

- [ ] **Step 3: Preserve the existing behavioral layout while replacing visual containers**

Keep the existing `1024x640` interaction-safe shell, 20 runtime backpack slots, five filter buttons, merchant stock grid, equipment buttons, selection overlays, sort/decompose buttons, detail actions, and confirmation callbacks.  Do not introduce a geometry-only migration that could disturb existing click targets.  The Town parchment assets are nine-sliced into those containers; the direct-reference slot art remains square and the runtime item icons stay centered within it.  Add `T_TownBackpack_Header` as a static image at the top-left, leave the title runtime text only for Merchant mode, and use `T_TownBackpack_BackArrow` as the clickable close overlay.

For action text, set a dark ink color, because `T_TownBackpack_ActionBlank` contains no label.  Draw that blank button with a nine-slice brush so its direct-source bevel does not stretch when used by wider runtime actions.  Do not place the full sample-item backpack panel anywhere in the widget tree.

### Task 6: Rebuild, import, and run the green verification pass

**Files:**
- Verify: all files above

- [ ] **Step 1: Save UE packages and cold-build the editor target**

Use UE MCP to save `/Game/GameXXK/UI/Town/Textures`, then close/restart through the supported pipeline and run:

```powershell
$env:UE-LocalDataCachePath='D:\UE5 demo\UnrealEngineCache'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development 'D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex
```

Expected output: `Result: Succeeded` and exit code `0`.

- [ ] **Step 2: Run the updated widget automation and existing inventory interaction automation**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.WidgetBasesDriveRules+GameXXK.MVP.Inventory.EnhancementAndStorage;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected output: both tests complete with `Result={Success}` and process exit code `0`.

- [ ] **Step 3: Run one visual town check through UE MCP**

Open the playable Qingshan town map, start PIE once, verify that the top-left HUD exposes separate red and green fills, then open the backpack and verify the Town-paper frame, tabs, slots, sort/decompose, and close/back control are present.  Do not edit user-placed maps, sprites, or camera values during this visual check.

- [ ] **Step 4: Save and commit only the focused files**

```powershell
git add docs/superpowers/plans/2026-07-15-town-hud-inventory-reference-refresh.md scripts/cut_town_hud_backpack_reference.py Content/Python/gamexxk_import_town_ui_assets.py docs/ui/town/source_art/HUD docs/ui/town/source_art/Backpack Source/GameXXK/Public/UI/GameXXKTownHudWidget.h Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
git commit -m "feat: refresh town hud and backpack reference art"
```

Do not stage `tmp/`, user-tuned map assets, or the legacy `/Game/GameXXK/UI/Inventory/Textures` tree.

## Self-review

- **Spec coverage:** Tasks 2–3 make direct-source HUD/backpack assets; Task 4 makes HP/XP dynamic; Task 5 removes the legacy backpack shell while preserving all interaction widgets; Task 6 verifies the compile, gameplay behavior, and one visual pass.
- **No baked gameplay data:** bar interior is separated from the frame; backpack sample icons/counts/title/capacity are never used as a background; capacity stays runtime text; confirmation has runtime text/buttons.
- **Consistency:** all new inventory paths resolve from `TownBackpackTextureRoot`; the three slot consumers share `T_TownBackpack_Slot`; exact widget names in the test match Task 4.
- **Completeness scan:** no unresolved implementation steps remain.
