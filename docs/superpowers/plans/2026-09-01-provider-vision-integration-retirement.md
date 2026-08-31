# Provider Vision Integration Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the project-local provider-specific vision client and its PIE review flow, while removing rules that force or forbid a named visual reviewer.

**Architecture:** Delete the provider client at its Python boundary, preserve the unrelated Slate screenshot path by moving its sole live consumer onto neutral helpers, and add a small source-policy regression test. Rewrite only prescriptive reviewer language; retain historical review facts and optional reviewer tools.

**Tech Stack:** Python 3 stdlib/unittest, PowerShell, Git, UE 5.8 MCP helper modules, Markdown production/spec/plan documents.

---

## File map

- Delete provider runtime: `scripts/gamexxk_vision.py`, `scripts/gamexxk_vision_pie.py`.
- Delete provider tests: `scripts/test_gamexxk_vision.py`, `scripts/test_gamexxk_vision_pie.py`.
- Create retirement guard: `scripts/test_provider_vision_retirement.py`.
- Modify the current untracked consumer in place: `scripts/run_town_hero_horizontal_pie_probe.py`.
- Modify script index: `scripts/README.md`.
- Preserve optional reviewer helper but remove compulsory wording: `scripts/ui_psd_pipeline/review-visual.ps1`.
- Remove `docs/production/2026-08-19-deepseek-handoff.md` and repair its production-document links.
- Rewrite prescriptive named-reviewer clauses in the exact production/spec/plan files listed below.
- Delete only reviewed, provider-specific ignored artifacts under `Saved`; do not delete a broad directory.

The workspace is intentionally dirty. Work directly on root `main`, do not create a worktree, and use path-limited staging/commits so the three pre-existing staged binary deletions and all unrelated changes remain untouched.

### Task 1: Add a failing retirement guard

**Files:**
- Create: `scripts/test_provider_vision_retirement.py`
- Read: `AGENTS.md`
- Read: `scripts/run_town_hero_horizontal_pie_probe.py`

- [ ] **Step 1: Create the source-policy test**

```python
"""Regression guard for the retired project-local vision integration."""

from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RETIRED_FILES = (
    PROJECT_ROOT / "scripts/gamexxk_vision.py",
    PROJECT_ROOT / "scripts/gamexxk_vision_pie.py",
    PROJECT_ROOT / "scripts/test_gamexxk_vision.py",
    PROJECT_ROOT / "scripts/test_gamexxk_vision_pie.py",
)
TEXT_ROOTS = (
    PROJECT_ROOT / "scripts",
    PROJECT_ROOT / "docs/production",
    PROJECT_ROOT / "Content/Python",
    PROJECT_ROOT / "Config",
    PROJECT_ROOT / "Source",
)
TEXT_SUFFIXES = {".md", ".txt", ".py", ".ps1", ".json", ".ini", ".cs", ".h", ".cpp"}
SELF = Path(__file__).resolve()


class ProviderVisionRetirementTests(unittest.TestCase):
    def test_retired_modules_are_absent(self) -> None:
        existing = [str(path.relative_to(PROJECT_ROOT)) for path in RETIRED_FILES if path.exists()]
        self.assertEqual(existing, [])

    def test_live_project_text_has_no_provider_integration_reference(self) -> None:
        provider_token = "deep" + "seek"
        module_token = "gamexxk_" + "vision"
        hits: list[str] = []
        for root in TEXT_ROOTS:
            if not root.exists():
                continue
            for path in root.rglob("*"):
                if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                    continue
                if path.resolve() == SELF:
                    continue
                text = path.read_text(encoding="utf-8", errors="ignore").lower()
                if provider_token in text or module_token in text:
                    hits.append(str(path.relative_to(PROJECT_ROOT)))
        self.assertEqual(hits, [])

    def test_global_agent_policy_does_not_name_a_visual_reviewer(self) -> None:
        policy = (PROJECT_ROOT / "AGENTS.md").read_text(encoding="utf-8").lower()
        self.assertNotIn("lu" + "na", policy)
        self.assertNotIn("codex" + "-vision", policy)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the test and record the expected red state**

Run:

```powershell
python -m unittest scripts.test_provider_vision_retirement -v
```

Expected: `test_retired_modules_are_absent` and `test_live_project_text_has_no_provider_integration_reference` fail because the four files and their documentation still exist. The AGENTS policy test should already pass because the user has an in-scope unstaged deletion of the old global rule.

- [ ] **Step 3: Confirm the test did not touch the editor or user assets**

Run:

```powershell
git status --short -- scripts/test_provider_vision_retirement.py AGENTS.md Content/GameXXK
```

Expected: only the new test plus the pre-existing `AGENTS.md` modification appear in this scoped view; no new `Content/GameXXK` change is created.

### Task 2: Remove the provider Python boundary and preserve neutral screenshot capture

**Files:**
- Delete: `scripts/gamexxk_vision.py`
- Delete: `scripts/gamexxk_vision_pie.py`
- Delete: `scripts/test_gamexxk_vision.py`
- Delete: `scripts/test_gamexxk_vision_pie.py`
- Modify: `scripts/run_town_hero_horizontal_pie_probe.py:18-70`
- Modify: `scripts/README.md:1-85`
- Test: `scripts/test_provider_vision_retirement.py`

- [ ] **Step 1: Replace the untracked probe's retired import with neutral helpers**

Add `_slate_preview_window_ref` to the existing import from `gamexxk_real_play_flow_mcp`, remove `from gamexxk_vision_pie import capture_pie_screenshot`, and replace `capture_live_window` with:

```python
def capture_live_window(client: UnrealMCPClient, name: str) -> tuple[Path, tuple[int, int]]:
    deadline = time.monotonic() + 15.0
    snapshot = ""
    preview_ref = ""
    while time.monotonic() < deadline:
        snapshot = str(
            client.call_tool(
                "Snapshot",
                {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
                toolset_name=SLATE_TOOLSET,
                timeout=client.timeout,
            )
        )
        preview_ref = _slate_preview_window_ref(snapshot)
        if preview_ref:
            break
        time.sleep(0.10)

    if not preview_ref:
        match = re.search(r'window "GameXXK - Unreal Editor"[^\n]*\[ref=(w\d+)\]', snapshot)
        if not match:
            raise RuntimeError(
                "GameXXK Preview or editor Slate window was not found; "
                f"last snapshot prefix: {snapshot[:500]}"
            )
        preview_ref = match.group(1)

    payload = client.call_tool(
        "Screenshot",
        {"ref": preview_ref},
        toolset_name=SLATE_TOOLSET,
        timeout=client.timeout,
    )
    data = _decode_slate_screenshot_png(payload)
    size = _png_size(data)
    if size[0] <= 0 or size[1] <= 0:
        raise RuntimeError(f"Slate screenshot returned invalid size: {size}")
    path = PROJECT_ROOT / "Saved/Codex" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path, size
```

Do not add this previously untracked user probe to Git. The change keeps the current local probe usable without absorbing the rest of that user-owned file into a commit.

- [ ] **Step 2: Delete the four tracked provider files**

Use `apply_patch` with `Delete File` for each exact path. Do not leave a compatibility module, import alias, disabled endpoint, or environment-variable fallback.

- [ ] **Step 3: Remove the script-index entry and usage section**

Delete both provider rows and the complete provider image-understanding section from `scripts/README.md`. Recalculate the displayed Python-script count instead of decrementing a stale number by hand:

```powershell
(rg --files scripts -g '*.py' | Measure-Object).Count
```

Keep the next heading, `## 交互编辑器启动`, and all following unrelated script documentation unchanged.

- [ ] **Step 4: Run the focused guard**

Run:

```powershell
python -m unittest scripts.test_provider_vision_retirement.ProviderVisionRetirementTests.test_retired_modules_are_absent -v
python -m py_compile scripts/run_town_hero_horizontal_pie_probe.py scripts/gamexxk_real_play_flow_mcp.py
```

Expected: both commands pass. The full text-reference test is still expected to fail until Tasks 3 and 4 remove documentation references.

- [ ] **Step 5: Commit only the clean tracked source boundary**

Run:

```powershell
git add -- scripts/test_provider_vision_retirement.py scripts/README.md scripts/gamexxk_vision.py scripts/gamexxk_vision_pie.py scripts/test_gamexxk_vision.py scripts/test_gamexxk_vision_pie.py
git diff --cached --check -- scripts/test_provider_vision_retirement.py scripts/README.md scripts/gamexxk_vision.py scripts/gamexxk_vision_pie.py scripts/test_gamexxk_vision.py scripts/test_gamexxk_vision_pie.py
git commit --only -m "chore: retire project vision integration" -- scripts/test_provider_vision_retirement.py scripts/README.md scripts/gamexxk_vision.py scripts/gamexxk_vision_pie.py scripts/test_gamexxk_vision.py scripts/test_gamexxk_vision_pie.py
```

Expected: the commit excludes the pre-existing staged binary deletions, `AGENTS.md`, the untracked town probe, and every unrelated dirty file.

### Task 3: Remove live provider documentation and global reviewer rules

**Files:**
- Modify: `AGENTS.md:17`
- Modify: `docs/production/current-goal-acceptance.md:31`
- Modify: `docs/production/2026-08-19-task6-two-level-exit-continuation-plan.md:14-183`
- Delete: `docs/production/2026-08-19-deepseek-handoff.md`
- Modify: `docs/production/2026-08-21-next-goals-and-plan.md:97-171,270`
- Modify: `docs/production/2026-08-21-next-goals-roadmap.md:64-165,243`
- Modify: `docs/production/2026-08-19-goal-progress-evidence.md:75`
- Modify: `docs/production/2026-08-16-full-project-optimization-proposal.md:84,150`
- Modify: `docs/production/2026-08-16-optimization-followup.md:42`
- Test: `scripts/test_provider_vision_retirement.py`

- [ ] **Step 1: Preserve the user's existing AGENTS rule deletion**

Confirm the only in-scope hunk is the removed named-reviewer preference. Keep the coordinate/arrow incident lesson and the floating-PIE SafeStage rule intact.

- [ ] **Step 2: Remove the dedicated provider handoff and repair links**

Delete the handoff document. In every referring production document, replace it with the still-valid neutral source (`docs/production/2026-08-19-task6-two-level-exit-continuation-plan.md`) or remove the redundant link when the surrounding paragraph already names that source.

Remove the provider-generated report claim from `current-goal-acceptance.md:31` while retaining the real PIE state evidence in the same paragraph.

- [ ] **Step 3: Rewrite active production rules as outcome-based evidence**

Use these semantic replacements consistently:

```text
Before: visual/presentation evidence is delegated to or must pass a named reviewer
After:  capture the required runtime evidence and verify the listed visible conditions with a suitable method

Before: use a named review script, with a deterministic fallback only if unavailable
After:  select a suitable review method; deterministic geometry, real input, and multi-size screenshots remain valid evidence

Before: a named reviewer or the user must approve before updating a contract
After:  review the visible contract drift before updating the contract
```

Do not rewrite past-tense statements that merely record an already-completed review result.

- [ ] **Step 4: Run the production-document scan**

Run:

```powershell
rg -n -i "deepseek|gamexxk_vision|DEEPSEEK_API_KEY|api\.deepseek\.com" docs/production scripts/README.md
rg -n -i "优先.*luna|project-required luna|required luna|must.*luna|invoke.*codex[_-]vision|do not (invoke|use) luna|不使用luna|不要使用luna" AGENTS.md docs/production
```

Expected: the provider scan has no output. The policy scan has no actionable instruction; a past-tense evidence sentence may remain only if it does not direct future work.

- [ ] **Step 5: Commit the production-policy change without unrelated files**

Use `git diff -- <paths>` to inspect every hunk, then `git commit --only` with the exact production paths and `AGENTS.md`. This deliberately includes the user's already-present AGENTS rule deletion because it is the requested policy change. Confirm `git show --stat HEAD` contains no Unreal asset.

### Task 4: Remove named-reviewer mandates from actionable specs and plans

**Files:**
- Modify prescriptive clauses in:
  - `docs/superpowers/specs/2026-08-14-battle-reward-tiering-design.md`
  - `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`
  - `docs/superpowers/specs/2026-08-19-route-owned-auto-battle-correction-design.md`
  - `docs/superpowers/specs/2026-08-22-desktop-training-wave-health-reset-design.md`
  - `docs/superpowers/specs/2026-08-22-gamexxk-graduation-showcase-design.md`
  - `docs/superpowers/specs/2026-08-23-decoupled-party-formation-design.md`
  - `docs/superpowers/specs/2026-08-23-gamexxk-graduation-ppt-design.md`
  - `docs/superpowers/specs/2026-08-23-life-saving-charm-and-workbench-polish-design.md`
  - `docs/superpowers/specs/2026-08-24-shared-inventory-equipment-tools-chests-design.md`
  - `docs/superpowers/specs/2026-08-28-current-main-flow-ui-art-handoff-design.md`
  - `docs/superpowers/specs/2026-08-29-desktop-2d-narrative-task-drawer-design.md`
  - `docs/superpowers/specs/2026-08-30-desktop-single-viewport-rollback-design.md`
  - `docs/superpowers/specs/2026-08-31-qingshan-carriage-preview-design.md`
- Modify prescriptive clauses in:
  - `docs/superpowers/plans/2026-08-18-desktop-training-workbench-layout-reflow.md`
  - `docs/superpowers/plans/2026-08-18-training-strip-combat-presentation.md`
  - `docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md`
  - `docs/superpowers/plans/2026-08-19-route-owned-auto-battle-correction.md`
  - `docs/superpowers/plans/2026-08-22-desktop-training-wave-health-reset.md`
  - `docs/superpowers/plans/2026-08-22-ordered-party-formation.md`
  - `docs/superpowers/plans/2026-08-22-permanent-talent-graph.md`
  - `docs/superpowers/plans/2026-08-22-route-event-card-merchant.md`
  - `docs/superpowers/plans/2026-08-22-workbench-parent-close-stack.md`
  - `docs/superpowers/plans/2026-08-22-workbench-progression-systems-index.md`
  - `docs/superpowers/plans/2026-08-23-decoupled-party-formation.md`
  - `docs/superpowers/plans/2026-08-23-gamexxk-graduation-ppt.md`
  - `docs/superpowers/plans/2026-08-23-gamexxk-graduation-showcase.md`
  - `docs/superpowers/plans/2026-08-23-life-saving-charm-and-workbench-polish.md`
  - `docs/superpowers/plans/2026-08-24-gem-icon-progression.md`
  - `docs/superpowers/plans/2026-08-24-training-chest-wallet-strip-icons.md`
  - `docs/superpowers/plans/2026-08-28-desktop-workbench-ui-art-pilot.md`
  - `docs/superpowers/plans/2026-08-29-desktop-2d-narrative-task-drawer-implementation.md`
  - `docs/superpowers/plans/2026-08-29-dual-window-presentation-state-machine-implementation.md`
  - `docs/superpowers/plans/2026-08-30-permanent-npc-formation-and-route-event-retirement.md`
  - `docs/superpowers/plans/2026-08-31-prologue-map-yuebai-tutorial-01.md`
  - `docs/superpowers/plans/2026-08-31-qingshan-carriage-preview.md`
- Modify: `scripts/ui_psd_pipeline/review-visual.ps1:1-8`

- [ ] **Step 1: Rewrite only future-facing constraints**

For each listed file, change required/forbidden named-reviewer wording to the concrete visible acceptance criteria already present in that sentence. Keep report paths and prior PASS/FAIL findings when they describe historical evidence.

Use these replacements for headings and steps:

```text
“Run/Request/Invoke [named reviewer] review” -> “Review the captured visual evidence”
“[named reviewer] must check X”             -> “Verify X in the captured evidence”
“Do not use [named reviewer]”               -> remove the tool prohibition; retain the direct checks
“[named reviewer] review passes”            -> “the listed visual acceptance checks pass”
```

For `2026-08-28-desktop-workbench-ui-art-pilot.md`, also replace the deleted screenshot-module command with a neutral instruction to capture the current PIE Slate window through UE MCP. Do not invent a replacement provider.

- [ ] **Step 2: Make the PowerShell reviewer helper explicitly optional**

Replace its opening comment with:

```powershell
# review-visual.ps1 — optional model-assisted before/after comparison helper
#
# This helper is not a project gate. Agents may use it, another suitable review
# method, or direct inspection according to the task and available evidence.
```

Keep the helper's parameters and implementation unchanged.

- [ ] **Step 3: Protect the already-dirty prologue plan**

`docs/superpowers/plans/2026-08-31-prologue-map-yuebai-tutorial-01.md` had unrelated user edits before this task. Apply only narrow named-reviewer wording changes, inspect its full diff, and do not include the file in an automatic whole-file commit. Leave it unstaged unless a clean index-only hunk can be proven to contain only this task's lines.

- [ ] **Step 4: Run the actionable-plan policy scan**

Run:

```powershell
rg -n -i "project-required luna|required luna|must.*luna|invoke.*codex[_-]vision|run.*codex[_-]vision|use.*codex[_-]vision|do not (invoke|use) luna|without luna|不使用luna|不要使用luna|优先.*luna" docs/superpowers scripts/ui_psd_pipeline/review-visual.ps1
```

Expected: no future-facing mandate or prohibition. Historical evidence names and existing report filenames may remain.

- [ ] **Step 5: Run the full retirement guard**

Run:

```powershell
python -m unittest scripts.test_provider_vision_retirement -v
```

Expected: all three tests pass.

- [ ] **Step 6: Commit only clean documentation files**

Stage and commit the clean listed specs/plans and `review-visual.ps1` with `git commit --only`. Exclude the pre-existing dirty prologue plan unless its task-only hunk was staged independently and verified. Confirm the commit contains no `Content`, `SourceArt`, or unrelated production file.

### Task 5: Remove provider-specific ignored evidence safely

**Files:**
- Delete reviewed matches only under `Saved/Codex`
- Delete reviewed matches only under `Saved/HarnessReports`
- Preserve all other `Saved` content, including gameplay saves and unrelated review evidence

- [ ] **Step 1: Resolve and print the exact targets**

Run this read-only PowerShell preflight:

```powershell
$projectRoot = (Resolve-Path -LiteralPath '.').Path
$savedRoot = (Resolve-Path -LiteralPath 'Saved').Path
$targets = Get-ChildItem -LiteralPath $savedRoot -Recurse -File | Where-Object {
    $_.Name -like 'vision_pie_*.png' -or
    $_.Name -like 'gamexxk_vision_pie_*.json' -or
    $_.Name -like 'deepseek-vision-*.json'
} | Sort-Object FullName -Unique
$targets | ForEach-Object {
    if (-not $_.FullName.StartsWith($savedRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Target escaped Saved: $($_.FullName)"
    }
    $_.FullName
}
```

Expected: only timestamped provider screenshots/reports previously enumerated during design. No directory, gameplay save, art source, or unrelated report appears.

- [ ] **Step 2: Review report-linked screenshots**

For each provider-named JSON report, inspect its `image`/`image_path` field. Add a referenced screenshot to the target list only when it is under `Saved/Codex` and was created solely for that provider report. Preserve shared screenshots used by another acceptance record.

- [ ] **Step 3: Delete exact files without recursion**

After the printed list is reviewed, rerun the complete Step 1 preflight block in the same PowerShell process and then append:

```powershell
$targets | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
```

Do not use `Remove-Item -Recurse`, a wildcard deletion, or a computed directory target.

- [ ] **Step 4: Verify the target patterns are gone**

Run:

```powershell
Get-ChildItem -LiteralPath 'Saved' -Recurse -File | Where-Object {
    $_.Name -like 'vision_pie_*.png' -or
    $_.Name -like 'gamexxk_vision_pie_*.json' -or
    $_.Name -like 'deepseek-vision-*.json'
}
```

Expected: no output. Report the number of deleted files and that ignored `Saved` evidence is not recoverable from Git.

### Task 6: Final verification and handoff

**Files:**
- Verify all task paths
- Do not modify Unreal runtime code or assets

- [ ] **Step 1: Run focused Python verification**

```powershell
python -m unittest scripts.test_provider_vision_retirement -v
python -m py_compile scripts/run_town_hero_horizontal_pie_probe.py scripts/gamexxk_real_play_flow_mcp.py
```

Expected: all retirement tests pass and both remaining local scripts compile.

- [ ] **Step 2: Run project script/state gates**

```powershell
python scripts/harness_state_validator.py --json
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
```

Expected: validator exits 0; the manifest-defined headless script suite exits 0. Record findings or unrelated baseline failures exactly rather than hiding them.

- [ ] **Step 3: Run final negative scans**

```powershell
rg -n -i --hidden --glob '!Binaries/**' --glob '!DerivedDataCache/**' --glob '!Intermediate/**' --glob '!Saved/**' --glob '!.git/**' --glob '!docs/superpowers/specs/2026-09-01-provider-vision-integration-retirement-design.md' --glob '!docs/superpowers/plans/2026-09-01-provider-vision-integration-retirement.md' --glob '!scripts/test_provider_vision_retirement.py' "deepseek|gamexxk_vision|DEEPSEEK_API_KEY|api\.deepseek\.com" .
rg -n -i "project-required luna|required luna|must.*luna|invoke.*codex[_-]vision|do not (invoke|use) luna|不使用luna|不要使用luna|优先.*luna" AGENTS.md docs/production docs/superpowers scripts/ui_psd_pipeline/review-visual.ps1
git ls-files | rg -i "deepseek|gamexxk_vision"
```

Expected: the provider scans have no output or tracked filename. The named-reviewer scan has no actionable mandate/prohibition; retained past-tense evidence is reviewed line by line.

- [ ] **Step 4: Verify patch integrity and workspace isolation**

```powershell
git diff --check
git status --short
git diff --name-only
git diff --cached --name-only
```

Expected: no whitespace errors introduced by this task. The pre-existing staged binary deletions remain staged but were never included in a task commit. Unrelated dirty assets and source files remain untouched.

- [ ] **Step 5: Review commits and report completion**

```powershell
git log -5 --oneline --decorate
git show --stat --oneline HEAD
```

Report the exact deleted source/tests/docs, the neutral screenshot dependency, the policy outcome, deleted ignored-evidence count, test results, validator findings, and any intentionally uncommitted task hunk in the pre-existing dirty prologue plan. Do not claim UBT or PIE verification; neither is required for this automation/documentation-only change.
