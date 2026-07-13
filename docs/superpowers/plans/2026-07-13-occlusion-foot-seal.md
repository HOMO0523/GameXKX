# Player Occlusion Foot Seal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent the player occlusion circle from cutting any foreground geometry at or below the hero's feet.

**Architecture:** The hero component already projects its visual bounds and writes `OcclusionFootY` to `MPC_PlayerOcclusion`. Remove its below-feet padding. The authored cutout material compares `ViewportUV.G` with that scalar and only multiplies the circle/depth reveal while the pixel is above the projected feet.

**Tech Stack:** Unreal Engine 5.8 C++, Material Parameter Collection, editor Python material authoring, Python `unittest`.

---

### Task 1: Lock the zero-padding behavior with a failing asset contract

**Files:**
- Modify: `scripts/test_occlusion_cutout_pilot_assets.py`
- Modify: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`

- [ ] **Step 1: Write the failing test**

```python
def test_authoring_script_uses_a_hard_foot_seal_without_below_feet_padding(self):
    source = AUTHOR.read_text(encoding="utf-8")
    self.assertIn("OcclusionFootY", source)
    self.assertIn("foot_gate", source)
    self.assertIn("MaterialExpressionMultiply", source)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m unittest scripts.test_occlusion_cutout_pilot_assets -v`

Expected: FAIL because the foot-seal material graph contract is not yet declared.

- [ ] **Step 3: Keep the material foot gate as a multiply before `OneMinus`**

```python
gated_reveal_above_feet = _node(material, unreal.MaterialExpressionMultiply, 420, 860)
_connect(gated_reveal, ("",), gated_reveal_above_feet, ("A",))
_connect(foot_gate, ("",), gated_reveal_above_feet, ("B",))
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m unittest scripts.test_occlusion_cutout_pilot_assets -v`

Expected: all tests pass.

### Task 2: Publish the precise foot line from C++

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write the failing automation assertion**

```cpp
TestEqual(TEXT("foot seal has no below-feet screen padding"),
    RevealController->GetRevealBelowFeetPaddingNormalized(), 0.0f);
```

- [ ] **Step 2: Run a cold editor build to verify it fails**

Run: `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject"`

Expected: compile error because the test getter does not exist.

- [ ] **Step 3: Implement the minimal runtime change**

```cpp
float GetRevealBelowFeetPaddingNormalized() const { return RevealBelowFeetPaddingNormalized; }
float RevealBelowFeetPaddingNormalized = 0.0f;

FMath::Clamp(FootPixelPosition.Y / FMath::Max(1, ViewY), 0.0f, 1.0f)
```

- [ ] **Step 4: Rebuild and run the material contract suite**

Run the same `Build.bat` command and `python -m unittest scripts.test_occlusion_cutout_pilot_assets -v`.

Expected: build exit code 0; all Python tests pass.

### Task 3: Regenerate and inspect the project-owned material copies

**Files:**
- Run: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`
- Inspect: `Content/GameXXK/Materials/Occlusion/AsianVillage/*_Cutout.uasset`

- [ ] **Step 1: Run the authoring script through UE MCP**

```python
client.run_project_python_file("Content/Python/gamexxk_author_occlusion_cutout_pilot.py")
```

- [ ] **Step 2: Save dirty packages through UE MCP**

```python
client.save_dirty_packages()
```

- [ ] **Step 3: PIE-check the rail/stair regression case**

Run PIE on `L_Qingshan_AsianVillage_Demo`; place the hero above a rail or wall. Confirm the wall remains opaque below the boots and a roof/wall overlapping the upper body still reveals the hero.
