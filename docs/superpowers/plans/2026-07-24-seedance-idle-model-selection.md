# Seedance Idle Model Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce six directly comparable 5-second 720p seamless hero idle tests and preserve their task metadata for model selection.

**Architecture:** Use the installed official Dreamina CLI `frames2video` command with the same image for `--first` and `--last`. Submit one paid smoke test first, verify terminal success and credit consumption, then run the remaining public models sequentially and download each result into a dedicated model folder.

**Tech Stack:** Dreamina CLI 1.4.14, Seedance frames2video models, PowerShell, PNG source, MP4 outputs.

---

### Task 1: Validate paid submission path

**Files:**
- Read: `SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png`
- Create: `SourceAssets/AnimationModelTests/seedance_idle_v1/hero/seedance1.5pro/`

- [ ] Record `dreamina user_credit` before submission.
- [ ] Submit one `seedance1.5pro` task with identical first/last frame, 5 seconds and 720p.
- [ ] Poll until `gen_status=success` or `gen_status=fail`; shell exit code alone is not success.
- [ ] Download the terminal result into the model directory.
- [ ] Record the post-task credit balance and the returned `submit_id`.

### Task 2: Run the remaining model matrix

**Files:**
- Create: `SourceAssets/AnimationModelTests/seedance_idle_v1/hero/<model>/`

- [ ] Repeat the exact same input, prompt, duration and resolution for `seedance2.0`.
- [ ] Repeat for `seedance2.0fast`.
- [ ] Repeat for `seedance2.0_vip`.
- [ ] Repeat for `seedance2.0fast_vip`.
- [ ] Repeat for `seedance2.0mini`.
- [ ] Query every accepted task until it reaches terminal success or failure and download successful MP4 files.

### Task 3: Verify comparability

**Files:**
- Create: `SourceAssets/AnimationModelTests/seedance_idle_v1/hero/results.json`

- [ ] Verify that every successful file exists and is non-empty.
- [ ] Inspect video duration, dimensions and frame rate with the available media probe.
- [ ] Record model, submit ID, status, failure reason, credits consumed and file path in `results.json`.
- [ ] Present the six clips for visual comparison; do not select a winner without user review.
