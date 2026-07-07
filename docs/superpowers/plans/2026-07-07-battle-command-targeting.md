# Battle Command Targeting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the battle command interaction where right-clicking a party actor opens an ink-style command menu, attack skills enter mouse targeting with a curved ink arrow, and left-clicking an enemy executes the selected skill.

**Architecture:** Keep battle rules in `UGameXXKMVPRules` and `UGameXXKMVPSubsystem`. Put world hit-testing and mouse routing in `AGameXXKMVPPlayerController`, battle UI state and arrow rendering in `UGameXXKBattleBoardWidget`, and role/clickability predicates in `AGameXXKBattleSceneUnitActor`. Process generated PNG source art with deterministic scripts before UE import.

**Tech Stack:** Unreal Engine 5.8 C++, UMG, Paper2D scene actors, project UE MCP scripts, Python/Pillow for source-art slicing, UBT, `scripts/gamexxk_mvp_playtest.py`.

---

## Constraints

- Work in `D:\UE5 demo\GameXXK` on `main`.
- Do not create or use a git worktree.
- Do not use UnrealBridge.
- Keep scans targeted; use `rg` and specific file reads.
- Do not use Live Coding or Hot Reload as verification.
- Use UBT for C++ verification.
- Use UE MCP only for editor asset import/save steps.
- If UE editor is running, save dirty packages through UE MCP before closing or restarting it.
- Do not overwrite user-tuned Paper2D/PaperZD assets or level placements.

## File Map

Create:

- `scripts/gamexxk_slice_battle_target_atlas.py`
  Slices `battle_target_ink_dots_atlas.png` into 12 deterministic alpha PNG dab files.

- `scripts/gamexxk_battle_ui_asset_check.py`
  Verifies the arrow head, atlas, and sliced dab files without launching UE.

- `Content/Python/gamexxk_import_battle_ui_assets.py`
  Imports the arrow head and sliced dab PNGs into `/Game/GameXXK/UI/Battle/Textures`.

- `Content/Python/gamexxk_validate_battle_ui_assets.py`
  Validates imported texture packages and reports paths for automation.

Modify:

- `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
  Add party-command and targeted-enemy predicates. Remove direct enemy right-click attack from the player-facing path.

- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
  Add battle UI interaction mode, contextual command menu, targeting methods, ink-button styling, and arrow paint state.

- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
  Route right-click, left-click, mouse move, and Esc to battle board state.

Modify tests:

- `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp` or a new focused test file if the controller test becomes too crowded.

Do not modify battle rules unless a failing test proves a rules bug. The UI should call existing subsystem methods:

- `ExecuteBattleBasicAttack(0, EnemyIndex)`
- `ExecuteBattleCraneWingSlash(0, EnemyIndex)`
- `ExecuteBattleGuiyuanArt(0)`
- `ExecuteBattleDefend(0)`
- `ExecuteBattleHealingPowder(0)`

---

## Task 1: Slice And Validate Ink Dab Assets

**Files:**

- Create: `scripts/gamexxk_battle_ui_asset_check.py`
- Create: `scripts/gamexxk_slice_battle_target_atlas.py`
- Output directory: `Content/GameXXK/SourceArt/UI/Battle/Generated/`

- [ ] **Step 1: Write the failing asset check**

Create `scripts/gamexxk_battle_ui_asset_check.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Battle"
GENERATED_DIR = SOURCE_DIR / "Generated"
ARROW_HEAD = SOURCE_DIR / "battle_target_arrow_head.png"
INK_ATLAS = SOURCE_DIR / "battle_target_ink_dots_atlas.png"
DAB_COUNT = 12


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def validate_alpha_png(path: Path) -> None:
    require(path.exists(), f"missing file: {path}")
    image = Image.open(path).convert("RGBA")
    width, height = image.size
    require(width > 0 and height > 0, f"invalid size: {path}")
    corners = [
        image.getpixel((0, 0))[3],
        image.getpixel((width - 1, 0))[3],
        image.getpixel((0, height - 1))[3],
        image.getpixel((width - 1, height - 1))[3],
    ]
    require(all(alpha == 0 for alpha in corners), f"corners are not transparent: {path}")
    alpha_values = list(image.getchannel("A").getdata())
    require(any(alpha > 0 for alpha in alpha_values), f"no visible pixels: {path}")


def main() -> None:
    validate_alpha_png(ARROW_HEAD)
    validate_alpha_png(INK_ATLAS)
    require(GENERATED_DIR.exists(), f"missing generated directory: {GENERATED_DIR}")
    dabs = sorted(GENERATED_DIR.glob("battle_target_ink_dab_*.png"))
    require(len(dabs) == DAB_COUNT, f"expected {DAB_COUNT} dabs, found {len(dabs)}")
    expected_names = [f"battle_target_ink_dab_{index:02d}.png" for index in range(DAB_COUNT)]
    require([path.name for path in dabs] == expected_names, "dab filenames are not deterministic")
    for dab in dabs:
        validate_alpha_png(dab)
    print({"ok": True, "dab_count": len(dabs), "generated_dir": str(GENERATED_DIR)})


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the check and verify RED**

Run:

```powershell
python scripts\gamexxk_battle_ui_asset_check.py
```

Expected: FAIL with `missing generated directory` or `expected 12 dabs, found 0`.

- [ ] **Step 3: Implement the slicer**

Create `scripts/gamexxk_slice_battle_target_atlas.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Battle"
DEFAULT_ATLAS = SOURCE_DIR / "battle_target_ink_dots_atlas.png"
DEFAULT_OUT_DIR = SOURCE_DIR / "Generated"
ROWS = 3
COLUMNS = 4
DAB_COUNT = ROWS * COLUMNS


def alpha_bounds(image: Image.Image, threshold: int) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    width, height = image.size
    xs: list[int] = []
    ys: list[int] = []
    for y in range(height):
        for x in range(width):
            if alpha.getpixel((x, y)) > threshold:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise ValueError("cell has no visible alpha")
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def expand_box(box: tuple[int, int, int, int], size: tuple[int, int], padding: int) -> tuple[int, int, int, int]:
    left, top, right, bottom = box
    width, height = size
    return (
        max(0, left - padding),
        max(0, top - padding),
        min(width, right + padding),
        min(height, bottom + padding),
    )


def slice_atlas(atlas_path: Path, out_dir: Path, padding: int, alpha_threshold: int) -> list[Path]:
    image = Image.open(atlas_path).convert("RGBA")
    cell_width = image.width // COLUMNS
    cell_height = image.height // ROWS
    out_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []

    for index in range(DAB_COUNT):
        row = index // COLUMNS
        column = index % COLUMNS
        cell_box = (
            column * cell_width,
            row * cell_height,
            image.width if column == COLUMNS - 1 else (column + 1) * cell_width,
            image.height if row == ROWS - 1 else (row + 1) * cell_height,
        )
        cell = image.crop(cell_box)
        bounds = expand_box(alpha_bounds(cell, alpha_threshold), cell.size, padding)
        dab = cell.crop(bounds)
        output = out_dir / f"battle_target_ink_dab_{index:02d}.png"
        dab.save(output)
        outputs.append(output)
    return outputs


def main() -> None:
    parser = argparse.ArgumentParser(description="Slice GameXXK battle targeting ink dab atlas")
    parser.add_argument("--atlas", type=Path, default=DEFAULT_ATLAS)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--padding", type=int, default=12)
    parser.add_argument("--alpha-threshold", type=int, default=8)
    args = parser.parse_args()

    outputs = slice_atlas(args.atlas, args.out_dir, args.padding, args.alpha_threshold)
    print({"ok": True, "count": len(outputs), "out_dir": str(args.out_dir)})


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run the slicer**

Run:

```powershell
python scripts\gamexxk_slice_battle_target_atlas.py
```

Expected: prints `{'ok': True, 'count': 12, ...}` and writes `Content\GameXXK\SourceArt\UI\Battle\Generated\battle_target_ink_dab_00.png` through `_11.png`.

- [ ] **Step 5: Run the asset check and verify GREEN**

Run:

```powershell
python scripts\gamexxk_battle_ui_asset_check.py
```

Expected: PASS with `{'ok': True, 'dab_count': 12, ...}`.

- [ ] **Step 6: Commit asset processing scripts and generated dab PNGs**

Run:

```powershell
git add scripts\gamexxk_battle_ui_asset_check.py scripts\gamexxk_slice_battle_target_atlas.py Content\GameXXK\SourceArt\UI\Battle
git commit -m "feat: slice battle targeting ink assets"
```

---

## Task 2: Import Battle UI Textures Through UE MCP

**Files:**

- Create: `Content/Python/gamexxk_import_battle_ui_assets.py`
- Create: `Content/Python/gamexxk_validate_battle_ui_assets.py`
- Create: `scripts/gamexxk_battle_ui_assets_apply.py`
- Create: `scripts/gamexxk_battle_ui_assets_check.py`

- [ ] **Step 1: Write the UE validator first**

Create `Content/Python/gamexxk_validate_battle_ui_assets.py`:

```python
from __future__ import annotations

import json
import unreal

TEXTURE_PATHS = [
    "/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead",
] + [
    f"/Game/GameXXK/UI/Battle/Textures/T_BattleTargetInkDab_{index:02d}"
    for index in range(12)
]


def main() -> None:
    missing = []
    wrong_class = []
    for path in TEXTURE_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            missing.append(path)
        elif not isinstance(asset, unreal.Texture2D):
            wrong_class.append(path)
    result = {"ok": not missing and not wrong_class, "missing": missing, "wrong_class": wrong_class}
    print(json.dumps(result, ensure_ascii=False))
    if not result["ok"]:
        raise RuntimeError(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Write the command-line validator wrapper**

Create `scripts/gamexxk_battle_ui_assets_check.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ue_mcp_client import UnrealMCPClient

VALIDATOR = "Content/Python/gamexxk_validate_battle_ui_assets.py"


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate imported GameXXK battle UI textures through UE MCP")
    parser.add_argument("--timeout", type=float, default=180.0)
    args = parser.parse_args()
    client = UnrealMCPClient(timeout=args.timeout)
    client.require_connected()
    result = client.run_project_python_file(VALIDATOR)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if not result.get("ok", False):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run the UE validator and verify RED**

If the editor is already running with MCP:

```powershell
python scripts\gamexxk_battle_ui_assets_check.py
```

Expected: FAIL with missing `/Game/GameXXK/UI/Battle/Textures/...` assets.

If the editor is not running, record this as blocked for this task and continue with C++ tasks after noting that import verification will run later.

- [ ] **Step 4: Write the importer**

Create `Content/Python/gamexxk_import_battle_ui_assets.py`:

```python
from __future__ import annotations

import json
from pathlib import Path
import unreal

PROJECT_ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Battle"
GENERATED_DIR = SOURCE_DIR / "Generated"
DESTINATION = "/Game/GameXXK/UI/Battle/Textures"

IMPORTS = [
    (SOURCE_DIR / "battle_target_arrow_head.png", "T_BattleTargetArrowHead"),
] + [
    (GENERATED_DIR / f"battle_target_ink_dab_{index:02d}.png", f"T_BattleTargetInkDab_{index:02d}")
    for index in range(12)
]


def import_texture(source: Path, asset_name: str) -> str:
    if not source.exists():
        raise RuntimeError(f"missing source image: {source}")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"{DESTINATION}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import Texture2D: {asset_path}")
    texture.compression_settings = unreal.TextureCompressionSettings.TC_EDITOR_ICON
    texture.mip_gen_settings = unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    texture.srgb = True
    texture.modify()
    unreal.EditorAssetLibrary.save_asset(asset_path)
    return asset_path


def main() -> None:
    imported = [import_texture(source, name) for source, name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
```

- [ ] **Step 5: Write the importer wrapper**

Create `scripts/gamexxk_battle_ui_assets_apply.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ue_mcp_client import UnrealMCPClient

IMPORTER = "Content/Python/gamexxk_import_battle_ui_assets.py"
VALIDATOR = "Content/Python/gamexxk_validate_battle_ui_assets.py"


def main() -> None:
    parser = argparse.ArgumentParser(description="Import GameXXK battle UI textures through UE MCP")
    parser.add_argument("--timeout", type=float, default=240.0)
    args = parser.parse_args()
    client = UnrealMCPClient(timeout=args.timeout)
    client.require_connected()
    import_result = client.run_project_python_file(IMPORTER)
    validate_result = client.run_project_python_file(VALIDATOR)
    result = {"ok": bool(import_result.get("ok")) and bool(validate_result.get("ok")), "import": import_result, "validate": validate_result}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if not result["ok"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
```

- [ ] **Step 6: Run importer and validator when UE MCP is available**

Run:

```powershell
python scripts\gamexxk_battle_ui_assets_apply.py
python scripts\gamexxk_battle_ui_assets_check.py
```

Expected: both commands exit 0 and report 13 imported/validated textures.

- [ ] **Step 7: Commit imported asset scripts and `.uasset` texture packages**

Run:

```powershell
git add Content\Python\gamexxk_import_battle_ui_assets.py Content\Python\gamexxk_validate_battle_ui_assets.py scripts\gamexxk_battle_ui_assets_apply.py scripts\gamexxk_battle_ui_assets_check.py Content\GameXXK\UI\Battle\Textures
git commit -m "feat: import battle targeting UI textures"
```

---

## Task 3: Update Battle Scene Unit Actor Interaction Predicates

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Write failing actor tests**

In `GameXXKBattleSceneActorTest.cpp`, replace the direct right-click attack assertions with role predicates:

```cpp
AGameXXKBattleSceneUnitActor* EnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
EnemyActor->SetMVPSubsystemForTest(Subsystem);
EnemyActor->ConfigureFromRuntimeUnit(true, 0, Subsystem->GetRuntimeState().ActiveBattleEnemies[0]);
const int32 EnemyHPBefore = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
TestFalse(TEXT("enemy actor cannot open party command menu"), EnemyActor->CanOpenPartyCommandMenu());
TestTrue(TEXT("living enemy actor can receive targeted battle action"), EnemyActor->CanReceiveTargetedBattleAction());
TestFalse(TEXT("enemy actor direct primary attack shortcut is disabled for player-facing input"), EnemyActor->ApplyPrimaryPartyAttack(nullptr));
TestEqual(TEXT("enemy actor shortcut does not damage enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBefore);

AGameXXKBattleSceneUnitActor* PartyActor = NewObject<AGameXXKBattleSceneUnitActor>();
PartyActor->SetMVPSubsystemForTest(Subsystem);
PartyActor->ConfigureFromRuntimeUnit(false, 0, Subsystem->GetRuntimeState().ActiveBattleParty[0]);
TestTrue(TEXT("living party actor can open command menu"), PartyActor->CanOpenPartyCommandMenu());
TestFalse(TEXT("party actor cannot receive targeted enemy action"), PartyActor->CanReceiveTargetedBattleAction());

FGameXXKBattleRuntimeUnit DefeatedParty = Subsystem->GetRuntimeState().ActiveBattleParty[0];
DefeatedParty.HP = 0;
DefeatedParty.bDefeated = true;
AGameXXKBattleSceneUnitActor* DefeatedPartyActor = NewObject<AGameXXKBattleSceneUnitActor>();
DefeatedPartyActor->ConfigureFromRuntimeUnit(false, 0, DefeatedParty);
TestFalse(TEXT("defeated party actor cannot open command menu"), DefeatedPartyActor->CanOpenPartyCommandMenu());

FGameXXKBattleRuntimeUnit DefeatedEnemy = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
DefeatedEnemy.HP = 0;
DefeatedEnemy.bDefeated = true;
AGameXXKBattleSceneUnitActor* DefeatedEnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
DefeatedEnemyActor->ConfigureFromRuntimeUnit(true, 0, DefeatedEnemy);
TestFalse(TEXT("defeated enemy actor cannot receive targeted battle action"), DefeatedEnemyActor->CanReceiveTargetedBattleAction());
```

Also replace the final-blow shortcut test with a note that final blows are handled through `UGameXXKBattleBoardWidget::ConfirmTargetingEnemy` in Task 4.

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-actor-red.json
```

Expected: compile fails first because `CanOpenPartyCommandMenu` and `CanReceiveTargetedBattleAction` do not exist, or automation fails if compilation already included stubs.

- [ ] **Step 3: Add actor predicates**

In `GameXXKBattleSceneUnitActor.h`, add:

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
bool CanOpenPartyCommandMenu() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
bool CanReceiveTargetedBattleAction() const;
```

In `GameXXKBattleSceneUnitActor.cpp`, add:

```cpp
bool AGameXXKBattleSceneUnitActor::CanOpenPartyCommandMenu() const
{
	return !bEnemy && UnitIndex != INDEX_NONE && !bDefeated && CurrentHP > 0;
}

bool AGameXXKBattleSceneUnitActor::CanReceiveTargetedBattleAction() const
{
	return bEnemy && UnitIndex != INDEX_NONE && !bDefeated && CurrentHP > 0;
}
```

Change `CanReceivePrimaryPartyAttack()` to delegate to the new targeted predicate for compatibility:

```cpp
bool AGameXXKBattleSceneUnitActor::CanReceivePrimaryPartyAttack() const
{
	return CanReceiveTargetedBattleAction();
}
```

Disable `ApplyPrimaryPartyAttack` for the player-facing path:

```cpp
bool AGameXXKBattleSceneUnitActor::ApplyPrimaryPartyAttack(APawn* InstigatorPawn)
{
	return false;
}
```

- [ ] **Step 4: Run tests and verify GREEN for actor changes**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-actor-green.json
```

Expected: `GameXXK.MVP.Battle.SceneActors` passes after tests are updated away from direct attack shortcuts.

- [ ] **Step 5: Commit actor predicate changes**

Run:

```powershell
git add Source\GameXXK\Public\MVP\GameXXKBattleSceneUnitActor.h Source\GameXXK\Private\MVP\GameXXKBattleSceneUnitActor.cpp Source\GameXXK\Private\Tests\GameXXKBattleSceneActorTest.cpp
git commit -m "feat: expose battle scene unit targeting roles"
```

---

## Task 4: Add Battle Board Command Menu And Targeting State

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing widget tests**

Update `GameXXKBattleBoardWidgetTest.cpp` so it no longer expects permanent buttons or right-click primary attacks. Add these assertions after `RefreshFromState()`:

```cpp
TestFalse(TEXT("battle command menu is hidden in idle battle state"), BattleWidget->IsCommandMenuVisibleForTest());
TestFalse(TEXT("battle board is not targeting in idle battle state"), BattleWidget->IsTargetingBattleActionForTest());
TestTrue(
	TEXT("battle board opens command menu for hero"),
	BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
TestTrue(TEXT("command menu becomes visible"), BattleWidget->IsCommandMenuVisibleForTest());
TestEqual(TEXT("command menu tracks selected party index"), BattleWidget->GetSelectedPartyIndexForTest(), 0);
TestEqual(TEXT("command menu stores menu anchor"), BattleWidget->GetCommandMenuAnchorForTest(), FVector2D(900.0f, 520.0f));
TestTrue(TEXT("basic attack command is enabled"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleBasicAttack")), true));
TestTrue(TEXT("Crane Wing Slash command is enabled"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleCraneWingSlash")), true));
TestTrue(TEXT("defend command is enabled"), BattleWidget->HasBattleActionForTest(FName(TEXT("BattleDefend")), true));

const int32 EnemyHPBeforeTargeting = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
TestTrue(TEXT("basic attack button enters targeting"), BattleWidget->ExecuteBasicAttackAction());
TestTrue(TEXT("targeting mode is active"), BattleWidget->IsTargetingBattleActionForTest());
TestEqual(TEXT("targeting action is basic attack"), BattleWidget->GetTargetingActionForTest(), FName(TEXT("BattleBasicAttack")));
TestEqual(TEXT("entering targeting does not damage first enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBeforeTargeting);
TestTrue(TEXT("empty targeting click keeps targeting active"), BattleWidget->KeepTargetingAfterEmptyClickForTest());
TestTrue(TEXT("targeting still active after empty click"), BattleWidget->IsTargetingBattleActionForTest());
TestTrue(TEXT("cancel targeting succeeds"), BattleWidget->CancelBattleTargeting());
TestFalse(TEXT("cancel hides targeting arrow"), BattleWidget->IsTargetingBattleActionForTest());
```

Replace the old direct `ExecutePrimaryEnemyAction()` victory assertions with:

```cpp
TestTrue(TEXT("reopen menu for final attack"), BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
TestTrue(TEXT("final basic attack enters targeting"), BattleWidget->ExecuteBasicAttackAction());
TestTrue(TEXT("confirming first enemy executes targeted basic attack"), BattleWidget->ConfirmTargetingEnemy(0));
TestEqual(TEXT("battle victory returns to dungeon route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
```

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
```

Expected: compile fails on missing widget methods and mode types.

- [ ] **Step 3: Add public widget API and state**

In `GameXXKBattleBoardWidget.h`, add an enum:

```cpp
UENUM(BlueprintType)
enum class EGameXXKBattleInteractionMode : uint8
{
	Hidden,
	Idle,
	CommandMenuOpen,
	TargetingBasicAttack,
	TargetingCraneWingSlash,
};
```

Add public methods:

```cpp
bool OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);
bool BeginTargetingBattleAction(FName ActionName);
void UpdateTargetingPointer(FVector2D ScreenPosition);
bool ConfirmTargetingEnemy(int32 EnemyIndex);
bool CancelBattleTargeting();
bool KeepTargetingAfterEmptyClickForTest();
bool IsCommandMenuVisibleForTest() const;
bool IsTargetingBattleActionForTest() const;
int32 GetSelectedPartyIndexForTest() const;
FVector2D GetCommandMenuAnchorForTest() const;
FVector2D GetTargetingStartForTest() const;
FVector2D GetTargetingEndForTest() const;
FName GetTargetingActionForTest() const;
```

Add private state:

```cpp
EGameXXKBattleInteractionMode InteractionMode = EGameXXKBattleInteractionMode::Hidden;
int32 SelectedPartyIndex = INDEX_NONE;
FVector2D CommandMenuAnchor = FVector2D::ZeroVector;
FVector2D SelectedPartyScreenPosition = FVector2D::ZeroVector;
FVector2D TargetingPointerPosition = FVector2D::ZeroVector;
FName TargetingActionName = NAME_None;
```

- [ ] **Step 4: Implement minimal state behavior**

In `GameXXKBattleBoardWidget.cpp`, change `ExecuteBasicAttackAction` and `ExecuteCraneWingSlashAction`:

```cpp
bool UGameXXKBattleBoardWidget::ExecuteBasicAttackAction()
{
	return BeginTargetingBattleAction(BasicAttackAction);
}

bool UGameXXKBattleBoardWidget::ExecuteCraneWingSlashAction()
{
	return BeginTargetingBattleAction(CraneWingSlashAction);
}
```

Add the new methods:

```cpp
bool UGameXXKBattleBoardWidget::OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || State->Screen != EGameXXKScreen::Battle || !State->ActiveBattleParty.IsValidIndex(PartyIndex))
	{
		return false;
	}
	const FGameXXKBattleRuntimeUnit& Unit = State->ActiveBattleParty[PartyIndex];
	if (Unit.bEnemy || Unit.bDefeated || Unit.HP <= 0)
	{
		return false;
	}
	SelectedPartyIndex = PartyIndex;
	CommandMenuAnchor = MenuScreenPosition;
	SelectedPartyScreenPosition = UnitScreenPosition;
	TargetingPointerPosition = UnitScreenPosition;
	TargetingActionName = NAME_None;
	InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKBattleBoardWidget::BeginTargetingBattleAction(FName ActionName)
{
	if (InteractionMode != EGameXXKBattleInteractionMode::CommandMenuOpen || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}
	if (ActionName == BasicAttackAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingBasicAttack;
	}
	else if (ActionName == CraneWingSlashAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
	}
	else
	{
		return false;
	}
	TargetingActionName = ActionName;
	TargetingPointerPosition = SelectedPartyScreenPosition;
	RefreshProgrammaticLayout();
	return true;
}

void UGameXXKBattleBoardWidget::UpdateTargetingPointer(FVector2D ScreenPosition)
{
	if (IsTargetingBattleActionForTest())
	{
		TargetingPointerPosition = ScreenPosition;
		InvalidateLayoutAndVolatility();
	}
}

bool UGameXXKBattleBoardWidget::ConfirmTargetingEnemy(int32 EnemyIndex)
{
	if (!IsTargetingBattleActionForTest())
	{
		return false;
	}
	const FName ActionToExecute = TargetingActionName;
	const int32 PartyIndex = SelectedPartyIndex;
	InteractionMode = EGameXXKBattleInteractionMode::Idle;
	TargetingActionName = NAME_None;
	SelectedPartyIndex = INDEX_NONE;
	bool bExecuted = false;
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem && ActionToExecute == BasicAttackAction)
	{
		bExecuted = Subsystem->ExecuteBattleBasicAttack(PartyIndex, EnemyIndex);
	}
	else if (Subsystem && ActionToExecute == CraneWingSlashAction)
	{
		bExecuted = Subsystem->ExecuteBattleCraneWingSlash(PartyIndex, EnemyIndex);
	}
	if (bExecuted)
	{
		if (Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		NotifyPlayerFlowStateChanged();
	}
	RefreshFromState();
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::CancelBattleTargeting()
{
	if (IsTargetingBattleActionForTest())
	{
		InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	if (InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		RefreshProgrammaticLayout();
		return true;
	}
	return false;
}
```

Implement test getters directly from state.

- [ ] **Step 5: Hide command menu in idle**

Change `RefreshActionButtons()` so the action/menu container visibility depends on `InteractionMode`:

```cpp
if (ActionBox)
{
	const bool bShowMenu = InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingBasicAttack
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
	ActionBox->SetVisibility(bShowMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	ActionBox->SetIsEnabled(InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen);
	ActionBox->SetRenderOpacity(IsTargetingBattleActionForTest() ? 0.55f : 1.0f);
}
```

When refreshing from non-battle state, set `InteractionMode = Hidden`; when battle state is active and mode is `Hidden`, set `Idle`.

- [ ] **Step 6: Run widget tests and verify GREEN**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-widget-green.json
```

Expected: `GameXXK.MVP.Battle.BoardWidget` passes and existing MVP flow still passes.

- [ ] **Step 7: Commit widget state changes**

Run:

```powershell
git add Source\GameXXK\Public\UI\GameXXKBattleBoardWidget.h Source\GameXXK\Private\UI\GameXXKBattleBoardWidget.cpp Source\GameXXK\Private\Tests\GameXXKBattleBoardWidgetTest.cpp
git commit -m "feat: add contextual battle command targeting"
```

---

## Task 5: Route PlayerController Mouse Input To Battle Board

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: add to `Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp` or create `Source/GameXXK/Private/Tests/GameXXKBattleInputControllerTest.cpp`

- [ ] **Step 1: Write failing controller tests with test hooks**

Add test hook declarations to the header first, then write tests that use them:

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
bool OpenBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
bool ConfirmBattleTargetForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor);

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
bool CancelBattleTargetingForTest();
```

Create a test subsystem in battle state, a player controller, a HUD/battle board, one party actor, and one enemy actor. Assert:

```cpp
const int32 EnemyHPBeforeRightClick = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
TestTrue(TEXT("controller opens command menu for party actor"), Controller->OpenBattleCommandMenuForUnitForTest(PartyActor, FVector2D(900.0f, 520.0f), FVector2D(830.0f, 420.0f)));
TestTrue(TEXT("battle board menu is visible after controller opens it"), Controller->GetBattleBoardWidgetForTest()->IsCommandMenuVisibleForTest());
TestFalse(TEXT("controller refuses enemy actor as command source"), Controller->OpenBattleCommandMenuForUnitForTest(EnemyActor, FVector2D(300.0f, 320.0f), FVector2D(300.0f, 320.0f)));
TestEqual(TEXT("right-click enemy path no longer damages enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBeforeRightClick);
TestTrue(TEXT("attack command enters targeting"), Controller->GetBattleBoardWidgetForTest()->ExecuteBasicAttackAction());
TestTrue(TEXT("controller confirms enemy target"), Controller->ConfirmBattleTargetForUnitForTest(EnemyActor));
TestTrue(TEXT("confirmed target takes damage"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP < EnemyHPBeforeRightClick);
TestTrue(TEXT("controller can cancel targeting"), Controller->CancelBattleTargetingForTest() || !Controller->GetBattleBoardWidgetForTest()->IsTargetingBattleActionForTest());
```

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
```

Expected: compile fails on missing controller test hooks.

- [ ] **Step 3: Implement controller helpers**

In `GameXXKMVPPlayerController.cpp`, implement:

```cpp
bool AGameXXKMVPPlayerController::OpenBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	if (!UnitActor || !UnitActor->CanOpenPartyCommandMenu())
	{
		return false;
	}
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->OpenCommandMenuForPartyUnit(UnitActor->GetUnitIndex(), MenuScreenPosition, UnitScreenPosition);
}

bool AGameXXKMVPPlayerController::ConfirmBattleTargetForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor)
{
	if (!UnitActor || !UnitActor->CanReceiveTargetedBattleAction())
	{
		return false;
	}
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->ConfirmTargetingEnemy(UnitActor->GetUnitIndex());
}

bool AGameXXKMVPPlayerController::CancelBattleTargetingForTest()
{
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting();
}
```

- [ ] **Step 4: Update real input routing**

Replace `TryHandleBattleSceneRightClick()` with behavior that opens party menu or cancels targeting:

```cpp
bool AGameXXKMVPPlayerController::TryHandleBattleSceneRightClick()
{
	EnsurePlayerFlowWidgets();
	if (BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting())
	{
		return true;
	}
	AGameXXKBattleSceneUnitActor* UnitActor = FindBattleSceneUnitUnderCursor(false);
	if (!UnitActor || !UnitActor->CanOpenPartyCommandMenu())
	{
		return false;
	}
	FVector2D CursorPosition;
	if (!GetMousePosition(CursorPosition.X, CursorPosition.Y))
	{
		return false;
	}
	FVector2D UnitScreenPosition = CursorPosition;
	ProjectWorldLocationToScreen(UnitActor->GetActorLocation(), UnitScreenPosition, true);
	return BattleBoardWidget && BattleBoardWidget->OpenCommandMenuForPartyUnit(UnitActor->GetUnitIndex(), CursorPosition, UnitScreenPosition);
}
```

Add a left-click battle handler before route-map primary click:

```cpp
bool AGameXXKMVPPlayerController::TryHandleBattleSceneLeftClick()
{
	if (!BattleBoardWidget || !BattleBoardWidget->IsTargetingBattleActionForTest())
	{
		return false;
	}
	AGameXXKBattleSceneUnitActor* UnitActor = FindBattleSceneUnitUnderCursor(true);
	if (UnitActor && UnitActor->CanReceiveTargetedBattleAction())
	{
		return BattleBoardWidget->ConfirmTargetingEnemy(UnitActor->GetUnitIndex());
	}
	return BattleBoardWidget->KeepTargetingAfterEmptyClickForTest();
}
```

Add Esc handling in `InputKey`:

```cpp
if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
{
	EnsurePlayerFlowWidgets();
	if (BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting())
	{
		return true;
	}
}
```

Refactor `FindBattleSceneUnitUnderCursorOrFallback()` into a non-fallback hit-test for real input:

```cpp
AGameXXKBattleSceneUnitActor* FindBattleSceneUnitUnderCursor(bool bRequireEnemyTarget) const;
```

Do not use fallback-to-first-enemy for player-facing clicks. Keep fallback only in tests if needed.

- [ ] **Step 5: Run controller tests and full MVP automation**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-controller-green.json
```

Expected: player-controller tests and full `GameXXK.MVP` pass.

- [ ] **Step 6: Commit controller routing**

Run:

```powershell
git add Source\GameXXK\Public\MVP\GameXXKMVPPlayerController.h Source\GameXXK\Private\MVP\GameXXKMVPPlayerController.cpp Source\GameXXK\Private\Tests
git commit -m "feat: route battle mouse input through command targeting"
```

---

## Task 6: Style Command Buttons And Render Ink Arrow

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify tests in `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing visual contract tests**

Add test getters to `UGameXXKBattleBoardWidget`:

```cpp
FString GetBattleActionButtonResourcePathForTest(FName ActionName) const;
FLinearColor GetBattleActionButtonTintForTest(FName ActionName) const;
int32 GetTargetingInkDabTextureCountForTest() const;
FString GetTargetingArrowHeadResourcePathForTest() const;
```

Add test assertions:

```cpp
TestTrue(TEXT("basic attack button uses ink texture"), BattleWidget->GetBattleActionButtonResourcePathForTest(FName(TEXT("BattleBasicAttack"))).Contains(TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase")));
TestTrue(TEXT("basic attack tint is reddish"), BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleBasicAttack"))).R > BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleBasicAttack"))).B);
TestTrue(TEXT("defend tint is bluish"), BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleDefend"))).B > BattleWidget->GetBattleActionButtonTintForTest(FName(TEXT("BattleDefend"))).R);
TestEqual(TEXT("targeting arrow has twelve dab textures"), BattleWidget->GetTargetingInkDabTextureCountForTest(), 12);
TestTrue(TEXT("targeting arrow head uses imported texture"), BattleWidget->GetTargetingArrowHeadResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead")));
```

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
```

Expected: compile fails on missing visual test getters.

- [ ] **Step 3: Load ink button and arrow textures**

Add soft paths in `GameXXKBattleBoardWidget.cpp`:

```cpp
static const FSoftObjectPath InkButtonTexturePath(TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase"));
static const FSoftObjectPath ArrowHeadTexturePath(TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead.T_BattleTargetArrowHead"));
```

Add a helper to load the 12 dabs:

```cpp
static TArray<UTexture2D*> LoadInkDabTextures()
{
	TArray<UTexture2D*> Textures;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const FString AssetName = FString::Printf(TEXT("T_BattleTargetInkDab_%02d"), Index);
		const FString Path = FString::Printf(TEXT("/Game/GameXXK/UI/Battle/Textures/%s.%s"), *AssetName, *AssetName);
		if (UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(Path).TryLoad()))
		{
			Textures.Add(Texture);
		}
	}
	return Textures;
}
```

- [ ] **Step 4: Style buttons with ink texture and tints**

Change `AddBattleActionButton` to accept action name or add a `StyleBattleActionButton(Button, ActionName)` helper:

```cpp
static FLinearColor GetBattleActionTint(FName ActionName)
{
	if (ActionName == BasicAttackAction || ActionName == CraneWingSlashAction)
	{
		return FLinearColor(0.62f, 0.23f, 0.20f, 0.95f);
	}
	if (ActionName == DefendAction)
	{
		return FLinearColor(0.18f, 0.36f, 0.62f, 0.95f);
	}
	return FLinearColor(0.08f, 0.23f, 0.19f, 0.95f);
}
```

Use `FButtonStyle` with `FSlateBrush` resource set to `T_InkButtonBase`.

- [ ] **Step 5: Render the arrow in `NativePaint`**

Add `NativePaint` override to the widget. In targeting mode:

- compute a quadratic Bezier from `SelectedPartyScreenPosition` to `TargetingPointerPosition`
- draw 8 to 14 dab images using the loaded dab textures
- draw the arrow head at the end
- use `FSlateDrawElement::MakeBox` for dabs
- use `FSlateDrawElement::MakeRotatedBox` for arrow head

Keep allocations out of mouse movement. Cache loaded textures in transient properties:

```cpp
UPROPERTY(Transient)
TObjectPtr<UTexture2D> TargetingArrowHeadTexture;

UPROPERTY(Transient)
TArray<TObjectPtr<UTexture2D>> TargetingInkDabTextures;
```

- [ ] **Step 6: Run tests and verify GREEN**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-visual-green.json
```

Expected: visual contract tests pass. If texture assets are not imported yet, run Task 2 before this step.

- [ ] **Step 7: Commit visual styling and arrow rendering**

Run:

```powershell
git add Source\GameXXK\Public\UI\GameXXKBattleBoardWidget.h Source\GameXXK\Private\UI\GameXXKBattleBoardWidget.cpp Source\GameXXK\Private\Tests\GameXXKBattleBoardWidgetTest.cpp
git commit -m "feat: render ink battle targeting UI"
```

---

## Task 7: Final Verification And Real PIE Check

**Files:**

- No expected source modifications unless tests expose a bug.
- Optional report outputs under `Saved/HarnessReports/`.

- [ ] **Step 1: Verify there is no running editor blocking compile**

Run:

```powershell
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,Path
```

Expected: no output, or if an editor is running, save dirty packages through UE MCP before any restart/close action.

- [ ] **Step 2: Run UBT**

Run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:/UE5 demo/GameXXK/GameXXK.uproject"
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Run full MVP automation**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py --skip-build --report Saved\HarnessReports\battle-command-targeting-mvp.json
```

Expected: `"ok": true`, including:

- `GameXXK.MVP.Battle.BoardWidget`
- `GameXXK.MVP.Battle.SceneActors`
- player-controller flow tests
- full MVP flow tests

- [ ] **Step 4: Run asset checks**

Run:

```powershell
python scripts\gamexxk_battle_ui_asset_check.py
python scripts\gamexxk_battle_ui_assets_check.py
```

Expected: both pass. If the second command cannot connect to MCP because the editor is closed, run it after the next editor/MCP validation session and record that it was not run in the command-line-only pass.

- [ ] **Step 5: Optional UE MCP real play smoke**

If the editor is available and stable:

```powershell
python scripts\gamexxk_real_play_flow_mcp.py --delete-default-save --report Saved\HarnessReports\battle-command-targeting-real-pie.json
```

Expected: the existing real flow still reaches battle. This harness may not yet drag the new arrow; the automation tests are the primary proof for the new battle targeting contract.

- [ ] **Step 6: Inspect git status**

Run:

```powershell
git status --short
```

Expected: only intended source files, scripts, generated SourceArt, imported UI textures, and Saved reports changed. Do not stage unrelated user changes.

- [ ] **Step 7: Final commit**

If prior tasks were not committed individually, commit all completed battle targeting changes:

```powershell
git add scripts\gamexxk_battle_ui_asset_check.py scripts\gamexxk_slice_battle_target_atlas.py scripts\gamexxk_battle_ui_assets_apply.py scripts\gamexxk_battle_ui_assets_check.py Content\Python\gamexxk_import_battle_ui_assets.py Content\Python\gamexxk_validate_battle_ui_assets.py Content\GameXXK\SourceArt\UI\Battle Content\GameXXK\UI\Battle\Textures Source\GameXXK\Public\MVP\GameXXKBattleSceneUnitActor.h Source\GameXXK\Private\MVP\GameXXKBattleSceneUnitActor.cpp Source\GameXXK\Public\UI\GameXXKBattleBoardWidget.h Source\GameXXK\Private\UI\GameXXKBattleBoardWidget.cpp Source\GameXXK\Public\MVP\GameXXKMVPPlayerController.h Source\GameXXK\Private\MVP\GameXXKMVPPlayerController.cpp Source\GameXXK\Private\Tests
git commit -m "feat: add battle command targeting"
```

---

## Self-Review Notes

Spec coverage:

- Contextual command menu is covered by Tasks 4 and 5.
- Targeting mode and confirm/cancel behavior are covered by Tasks 4 and 5.
- Ink arrow visuals and button tints are covered by Task 6.
- Automatic 12-dab slicing is covered by Task 1.
- UE texture import is covered by Task 2.
- Actor predicates and removal of direct enemy right-click attack are covered by Task 3.
- Victory/failure regression is covered by Tasks 4, 5, and 7.

Implementation risk controls:

- All C++ behavior changes have a RED/GREEN test step before implementation.
- Source-art slicing is verified without UE.
- UE asset import is isolated behind MCP and can be run when the editor is stable.
- No task uses Live Coding, Hot Reload, UnrealBridge, or git worktrees.
