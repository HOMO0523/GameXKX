# Player Occlusion Circle with Hero-Silhouette Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the player occlusion circle, while preventing it from cutting foreground geometry that does not overlap the hero sprite.

**Architecture:** The hero Paper2D visual writes a dedicated Custom Stencil value. The authored cutout material samples Custom Stencil at the current screen pixel, and multiplies that silhouette hit with the existing circle and foreground-depth gates. The circle remains a scope/activation aid rather than a command to erase every foreground pixel inside it.

**Tech Stack:** Unreal Engine 5.8 C++, Material Parameter Collection, editor Python material authoring, Python `unittest`.

---

### Task 1: Lock the hero-silhouette material gate with a failing asset contract

**Files:**
- Modify: `scripts/test_occlusion_cutout_pilot_assets.py`
- Modify: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`

- [ ] **Step 1: Write the failing test**

```python
def test_authoring_script_combines_circle_depth_and_hero_stencil(self):
    source = AUTHOR.read_text(encoding="utf-8")
    self.assertIn("PPI_CustomStencil", source)
    self.assertIn("OcclusionHeroStencilValue", source)
    self.assertIn("hero_silhouette_gate", source)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m unittest scripts.test_occlusion_cutout_pilot_assets -v`

Expected: FAIL because the Custom Stencil silhouette material graph is not yet declared.

- [ ] **Step 3: Keep the material foot gate as a multiply before `OneMinus`**

```python
gated_reveal_above_feet = _node(material, unreal.MaterialExpressionMultiply, 420, 860)
_connect(gated_reveal, ("",), gated_reveal_above_feet, ("A",))
_connect(foot_gate, ("",), gated_reveal_above_feet, ("B",))
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python -m unittest scripts.test_occlusion_cutout_pilot_assets -v`

Expected: all tests pass.

### Task 2: Configure the hero Paper2D visual for Custom Stencil

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKPlayerOcclusionRevealComponent.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKPlayerOcclusionRevealComponent.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write the failing automation assertion**

```cpp
TestTrue(TEXT("hero visual writes the occlusion custom stencil"),
    HeroVisual->bRenderCustomDepth);
```

- [ ] **Step 2: Run a cold editor build to verify it fails**

Run: `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject"`

Expected: test failure because the hero visual does not yet write Custom Stencil.

- [ ] **Step 3: Implement the minimal runtime change**

```cpp
Visual->SetRenderCustomDepth(true);
Visual->SetCustomDepthStencilValue(13);
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
