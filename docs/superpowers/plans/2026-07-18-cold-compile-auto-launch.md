# Cold Compile Auto Launch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the default GameXXK TDD pipeline cold-compile and automatically launch the project reliably on this machine.

**Architecture:** Retain `scripts/ue_tdd_pipeline.py` as the workflow owner. Add only the compile and editor-start flags required by project policy and the current DDC environment; isolate subprocess assertions in a new standard-library unit test module.

**Tech Stack:** Python standard library `unittest`, Unreal Build.bat, UnrealEditor command line.

---

### Task 1: Lock the automatic cold-build/start command contract

**Files:**
- Create: `scripts/test_ue_tdd_pipeline.py`
- Modify: `scripts/ue_tdd_pipeline.py`
- Test: `scripts/test_ue_tdd_pipeline.py`

- [ ] **Step 1: Write failing subprocess-contract tests**

Create tests which patch `subprocess.run` and `subprocess.Popen` on `scripts.ue_tdd_pipeline` and assert:

```python
self.assertIn("-NoHotReload", captured_build_command)
self.assertEqual(captured_editor_command[-1], "-DDC-ForceMemoryCache")
self.assertIn(str(pipeline.UPROJECT), captured_editor_command)
self.assertIn("-ModelContextProtocolStartServer", captured_editor_command)
```

The tests must not invoke Build.bat or UnrealEditor.

- [ ] **Step 2: Run the tests to verify RED**

Run:

```powershell
& "C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" scripts\test_ue_tdd_pipeline.py
```

Expected: the test fails because the current build and launch commands omit the two required flags.

- [ ] **Step 3: Apply the minimal workflow change**

Append `-NoHotReload` to `build_project()`'s Build.bat argument list. Append `-DDC-ForceMemoryCache` to `launch_editor()`'s `subprocess.Popen` command list. Do not change MCP save/close logic, process scope, PIE behavior, or command defaults.

- [ ] **Step 4: Run GREEN verification**

Run the focused unit test, then:

```powershell
& "C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" -m py_compile scripts\ue_tdd_pipeline.py scripts\test_ue_tdd_pipeline.py
```

Expected: both succeed without launching UE.

- [ ] **Step 5: Run the full user-facing chain**

When no unsaved editor instance is active, run:

```powershell
& "C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
```

Expected: save/close guard, cold compile, automatic project launch, MCP readiness, and PIE start/stop occur in that order.
