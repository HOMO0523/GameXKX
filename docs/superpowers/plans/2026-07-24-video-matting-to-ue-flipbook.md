# Video Matting to UE PaperFlipbook Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the existing local video matting tool and prove one Seedance 2.0 VIP hero idle clip can be exported as a stable transparent 12fps PaperFlipbook and displayed in the live UE battle scene.

**Architecture:** The Vite UI remains the visual tuning surface and exports a versioned JSON matting recipe. A deterministic Python exporter in the same tool directory decodes the real video stream, applies the recipe, stabilizes alpha, computes one union crop, places every frame on a fixed 512-square bottom-center canvas, and writes both PNG frames and a 4096 atlas. A focused UE Python importer creates isolated pilot assets; a PIE-only probe applies the pilot flipbook to the hero without changing production asset references.

**Tech Stack:** Vite/ES modules, Node built-in test runner, Python 3.12, NumPy, Pillow, imageio-ffmpeg, Unreal Engine 5.8 Python through project UE MCP, Paper2D.

---

### Task 1: Define the UE export recipe contract

**Files:**
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\src\ue-export-config.js`
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\tests\ue-export-config.test.mjs`
- Modify: `D:\UE5 demo\视频抠图工具-带裁剪功能\package.json`

- [ ] **Step 1: Write the failing contract tests**

```js
import test from 'node:test'
import assert from 'node:assert/strict'
import { buildUEExportConfig, validateUEExportConfig } from '../src/ue-export-config.js'

test('builds the fixed 5 second 12fps UE contract', () => {
  const value = buildUEExportConfig({
    mode: 'chroma', bgColor: { r: 255, g: 0, b: 255 }, bgColors: [],
    tolerance: 40, feather: 15, spillSuppress: true, spillStrength: 60,
    colorSpace: 'rgb', alphaBlur: 2, alphaContrast: 0.3,
  }, 'hero_idle.mp4')
  assert.equal(value.output.fps, 12)
  assert.equal(value.output.frameCount, 60)
  assert.equal(value.output.canvasSize, 512)
  assert.equal(value.output.anchor, 'bottom_center')
  assert.equal(validateUEExportConfig(value).length, 0)
})

test('rejects recipes without a chroma sample', () => {
  const value = buildUEExportConfig({ mode: 'chroma', bgColor: null, bgColors: [] }, 'bad.mp4')
  assert.match(validateUEExportConfig(value).join('\n'), /background sample/i)
})
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `npm test -- --test-name-pattern="UE contract"`

Expected: FAIL because `src/ue-export-config.js` does not exist.

- [ ] **Step 3: Implement the versioned recipe builder and validator**

```js
export const UE_EXPORT_DEFAULTS = Object.freeze({
  fps: 12, durationSeconds: 5, frameCount: 60, canvasSize: 512,
  anchor: 'bottom_center', paddingRatio: 0.08, temporalAlphaWindow: 3,
})

export function buildUEExportConfig(state, sourceFileName) {
  return {
    schemaVersion: 1,
    sourceFileName,
    matting: {
      mode: state.mode,
      backgroundSamples: state.bgColors?.length ? state.bgColors : (state.bgColor ? [state.bgColor] : []),
      tolerance: Number(state.tolerance ?? 40), feather: Number(state.feather ?? 15),
      spillSuppress: Boolean(state.spillSuppress), spillStrength: Number(state.spillStrength ?? 60),
      colorSpace: state.colorSpace ?? 'auto', alphaBlur: Number(state.alphaBlur ?? 2),
      alphaContrast: Number(state.alphaContrast ?? 0.3),
    },
    output: { ...UE_EXPORT_DEFAULTS },
  }
}

export function validateUEExportConfig(value) {
  const errors = []
  if (value?.schemaVersion !== 1) errors.push('unsupported schema version')
  if (value?.matting?.mode !== 'chroma') errors.push('UE pilot requires chroma mode')
  if (!value?.matting?.backgroundSamples?.length) errors.push('background sample is required')
  if (value?.output?.fps !== 12 || value?.output?.frameCount !== 60) errors.push('output must be 12fps and 60 frames')
  if (value?.output?.canvasSize !== 512 || value?.output?.anchor !== 'bottom_center') errors.push('output must use a 512 bottom-center canvas')
  return errors
}
```

- [ ] **Step 4: Add and run the Node test script**

Modify `package.json` scripts to include `"test": "node --test tests/*.test.mjs"`.

Run: `npm test`

Expected: all recipe contract tests PASS.

### Task 2: Add the deterministic frame exporter

**Files:**
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\requirements-ue-export.txt`
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\scripts\ue_export_core.py`
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\scripts\export_ue_animation.py`
- Create: `D:\UE5 demo\视频抠图工具-带裁剪功能\tests\test_ue_export_core.py`

- [ ] **Step 1: Write failing unit tests for matte, union bounds, timestamps, and anchoring**

```python
import unittest
import numpy as np
from PIL import Image
from scripts.ue_export_core import frame_times, chroma_rgba, union_alpha_bounds, place_bottom_center

class UEExportCoreTests(unittest.TestCase):
    def test_frame_times_exclude_duplicate_endpoint(self):
        values = frame_times(5.0, 12)
        self.assertEqual(len(values), 60)
        self.assertEqual(values[0], 0.0)
        self.assertLess(values[-1], 5.0)

    def test_magenta_becomes_transparent(self):
        rgb = np.full((4, 4, 3), [255, 0, 255], dtype=np.uint8)
        rgba = chroma_rgba(rgb, [(255, 0, 255)], tolerance=20, feather=10,
                           spill_strength=60, alpha_blur=0, alpha_contrast=0)
        self.assertEqual(int(rgba[..., 3].max()), 0)

    def test_bottom_center_anchor_is_stable(self):
        subject = Image.new('RGBA', (100, 200), (255, 255, 255, 255))
        placed = place_bottom_center(subject, 512, bottom_margin=20)
        self.assertEqual(placed.getbbox(), (206, 292, 306, 492))

if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run tests and confirm they fail**

Run: `python -m unittest discover -s tests -p "test_*.py" -v`

Expected: FAIL because `scripts.ue_export_core` does not exist.

- [ ] **Step 3: Implement pure export primitives**

Implement `frame_times`, `chroma_rgba`, `stabilize_alpha`, `union_alpha_bounds`, `normalize_union_frames`, `place_bottom_center`, and `build_atlas` as public functions in `ue_export_core.py`. `frame_times(5.0, 12)` must return `[index / 12 for index in range(60)]`. The matte uses minimum Euclidean RGB distance to all sampled background colors, smoothstep between `tolerance` and `tolerance + feather`, optional magenta spill suppression, Pillow Gaussian blur, contrast remapping around 0.5, then a centered three-frame median only on Alpha. `union_alpha_bounds` uses the union of pixels whose Alpha exceeds 4. Normalization expands that one box by 8%, derives one scale that fits the padded subject inside 512 square pixels, and uses that same scale and bottom-center placement for every frame. `build_atlas` allocates an 8×8 transparent RGBA image and pastes the 60 normalized frames row-major into 512-pixel cells.

- [ ] **Step 4: Implement the CLI orchestration**

`export_ue_animation.py` must accept:

```text
--video <path> --config <json> --output <directory> --name <asset_slug>
```

It must decode exactly the recipe time range with `imageio_ffmpeg`, reject non-square or non-5-second pilot inputs, emit `frames/frame_0000.png` through `frame_0059.png`, `atlas/<name>_idle_atlas.png`, and `manifest.json`, and fail non-zero on a missing frame, empty matte, clipped subject, wrong atlas size, or wrong output count.

- [ ] **Step 5: Install the isolated decoder dependency and run tests**

`requirements-ue-export.txt`:

```text
imageio-ffmpeg==0.6.0
numpy>=2.0,<3
Pillow>=11,<12
```

Run: `python -m pip install -r requirements-ue-export.txt`

Run: `python -m unittest discover -s tests -p "test_*.py" -v`

Expected: all exporter core tests PASS.

### Task 3: Add the UE recipe control to the existing UI

**Files:**
- Modify: `D:\UE5 demo\视频抠图工具-带裁剪功能\src\main.js`
- Modify: `D:\UE5 demo\视频抠图工具-带裁剪功能\src\style.css`
- Test: `D:\UE5 demo\视频抠图工具-带裁剪功能\tests\ue-export-config.test.mjs`

- [ ] **Step 1: Add a failing source-level UI contract test**

```js
import fs from 'node:fs'
test('main UI exposes UE recipe export', () => {
  const source = fs.readFileSync(new URL('../src/main.js', import.meta.url), 'utf8')
  assert.match(source, /id="btnExportUEConfig"/)
  assert.match(source, /buildUEExportConfig/)
  assert.match(source, /ue_export_recipe\.json/)
})
```

- [ ] **Step 2: Run the Node tests and confirm the new test fails**

Run: `npm test`

Expected: FAIL because the UE recipe control is absent.

- [ ] **Step 3: Add the UE export card and handler**

Import the config helpers, add a third export card labelled `UE 动画导出配置`, and bind `btnExportUEConfig`. The handler validates the current chroma state, downloads `<video-name>_ue_export_recipe.json`, and reports validation errors without exporting an invalid recipe.

- [ ] **Step 4: Build and run all UI tests**

Run: `npm test && npm run build`

Expected: tests PASS and Vite build exits 0.

### Task 4: Operate the tool and export the VIP hero idle pilot

**Files:**
- Read: `D:\UE5 demo\GameXXK\SourceAssets\AnimationModelTests\seedance_idle_v1\hero\seedance2.0_vip\dcb705c6-4f80-48de-a17c-d3e7c6e88ad9_video_1.mp4`
- Create: `D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle\recipe.json`
- Create: `D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle\frames\*.png`
- Create: `D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle\atlas\hero_idle_atlas.png`
- Create: `D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle\manifest.json`

- [ ] **Step 1: Start Vite and open the local tool**

Run: `npm run dev -- --host 127.0.0.1`

Use browser control to import the VIP MP4, sample several magenta border points, and inspect representative frames at 0%, 25%, 50%, 75%, and just before 5 seconds against checker, white, and black previews.

- [ ] **Step 2: Tune and export the recipe**

Choose the lowest tolerance that fully removes the background, then adjust feather, spill suppression, Alpha blur, and contrast until the sword, hair, and cloth edges remain intact with no obvious magenta fringe. Save the resulting JSON as `recipe.json` in the pilot directory.

- [ ] **Step 3: Run the deterministic exporter**

Run:

```powershell
python scripts/export_ue_animation.py --video "D:\UE5 demo\GameXXK\SourceAssets\AnimationModelTests\seedance_idle_v1\hero\seedance2.0_vip\dcb705c6-4f80-48de-a17c-d3e7c6e88ad9_video_1.mp4" --config "D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle\recipe.json" --output "D:\UE5 demo\GameXXK\SourceAssets\AnimationProcessing\Pilot\Hero\Idle" --name hero
```

Expected: 60 RGBA frames, one 4096×4096 atlas, and a valid manifest.

- [ ] **Step 4: Verify output mechanically and visually**

Run a validator that checks PNG mode RGBA, dimensions 512×512, 60 unique numbered frames, non-empty but unclipped Alpha bounds, atlas 4096×4096, and manifest fps 12. Inspect a contact sheet and compare frame 0 with frame 59 for a smooth loop handoff.

### Task 5: Import isolated Paper2D pilot assets

**Files:**
- Create: `D:\UE5 demo\GameXXK\scripts\test_battle_animation_pilot_pipeline.py`
- Create: `D:\UE5 demo\GameXXK\Content\Python\gamexxk_import_battle_animation_pilot.py`
- Create: `D:\UE5 demo\GameXXK\Content\Python\gamexxk_apply_battle_animation_pilot.py`

- [ ] **Step 1: Write the failing import contract test**

The test must assert that the importer targets `/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle`, imports the atlas from the pilot manifest, creates 60 `PaperSprite` assets with 512×512 source cells and `BOTTOM_CENTER` pivots, creates `FB_Pilot_Hero_Idle` at 12fps, and never references a production Hero/Enemy flipbook path.

- [ ] **Step 2: Run the Python contract test and confirm failure**

Run: `python scripts/test_battle_animation_pilot_pipeline.py -v`

Expected: FAIL because the UE importer and PIE probe do not exist.

- [ ] **Step 3: Implement the isolated UE importer**

Use `unreal.AssetImportTask` for the atlas, `unreal.PaperSpriteFactory` for 60 grid sprites, `unreal.PaperFlipbookFactory` and `unreal.PaperFlipbookKeyFrame` for the flipbook. Configure nearest/2D texture settings, bottom-center pivot, 12fps, save only assets under the pilot root, and print a JSON report.

- [ ] **Step 4: Implement the PIE-only application probe**

Find `AGameXXKBattleSceneUnitActor` instances in the PIE world, select the non-enemy actor with `get_unit_id() == 'Hero'`, load `FB_Pilot_Hero_Idle`, call `get_battle_visual_component().set_flipbook(...)`, enable looping, play from start, and print the actor, asset path, fps, and frame count. Do not save the level or modify C++ asset defaults.

- [ ] **Step 5: Run the contract test and UE MCP import**

Run: `python scripts/test_battle_animation_pilot_pipeline.py -v`

Then use `scripts/ue_mcp_client.py` to run `gamexxk_import_battle_animation_pilot.py`.

Expected: tests PASS; UE report confirms 60 sprites and one 12fps flipbook under the isolated pilot root.

### Task 6: Validate the pilot in the live battle scene

**Files:**
- Read: `D:\UE5 demo\GameXXK\scripts\gamexxk_real_play_flow_mcp.py`
- Read: generated UE pilot assets under `/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle`

- [ ] **Step 1: Save dirty UE packages through MCP**

Use the project MCP save operation before any editor restart. Do not force-close the editor when MCP is unavailable.

- [ ] **Step 2: Enter the real battle flow in PIE**

Use `scripts/gamexxk_real_play_flow_mcp.py` to reach the route battle screen, then run `gamexxk_apply_battle_animation_pilot.py` through MCP.

- [ ] **Step 3: Verify runtime asset and loop behavior**

Query the hero component at multiple times spanning more than one five-second cycle. Confirm the same pilot flipbook remains assigned, playback is looping at 12fps, the hero remains left-facing, and its visual foot does not drift.

- [ ] **Step 4: Capture visual evidence and check layout**

Capture PIE screenshots near the start, midpoint, and loop boundary. Confirm no magenta fringe, no opaque square, no clipping, stable scale, stable bottom anchor, and no HUD overlap/regression.

- [ ] **Step 5: Record the pilot result**

Write the exact recipe, exporter manifest path, UE asset paths, screenshots, and any remaining visual defects into the production handoff notes. Stop before bulk generation or bulk conversion unless all pilot acceptance checks pass.
