# GameXXK — AI Agent Constraints

## Hard Constraints

### Canonical Workflow
- Work in the root project on `main`; do not create or use git worktrees for this project unless the user explicitly reverses this rule.
- Keep repository scans targeted. Prefer `rg`/specific file reads over whole-project enumeration because UE assets make the working tree large.
- Do not use UnrealBridge for this project. Use UE 5.8 MCP, UBT, command-line scripts, Visual Studio tooling, or focused editor Python through MCP.
- Do not revert or overwrite user-tuned assets, especially character sprites, PaperZD assets, placed levels, camera transforms, and manually adjusted HD2D plane values.
- The canonical default surface is `/Game/GameXXK/Maps/L_DesktopTrainingHUD`. Unless the user explicitly asks for 3D work, keep editor startup, PIE, screenshots, and gameplay verification on the pure-2D desktop-to-BattleBoard flow; do not load a 3D town/scene as a routine fallback.

### Art Workflow
- Pure art work does not use TDD. This includes image generation, chroma-key removal, cutting or splitting assets, compositing, layout, PSD assembly, and visual calibration.
- Verify art work after implementation with deterministic asset checks such as dimensions, alpha edges, hashes, manifests, build reports, and visual review.
- Runtime code or gameplay behavior changes remain subject to the project's normal code verification rules.

### Visual / Presentation Issues
- 表现类问题(渲染错位、视觉校准、截图取证、界面呈现判断)优先**委托 lunamax** 处理:通过 `~/.claude/skills/codex-vision/scripts/codex_vision.ps1` 的 luna 视觉代理(`-Effort max`)截图/读图、定位与修复,不要自行盲改表现层代码。
- 教训(2026-08-15 箭头错位事件):坐标换算链经数值验证自洽时,优先怀疑绘制端局部公式(如贴图锚点/旋转枢轴),而不是反复改换算;取证截图前排除用户鼠标移动与同类色块(如立绘与箭头同为橙色墨)的干扰;方向向量只参与旋转/法线,不得参与位置平移。
- 浮动 PIE 的目标箭头只允许使用 viewport-client / SafeStage 本地坐标。禁止在 `NativePaint` 中用 `StageGeometry.LocalToAbsolute -> AllottedGeometry.AbsoluteToLocal` 跨 Geometry 往返；它会把浮动窗口桌面原点重新加到箭头与墨点上。

### UE MCP Automation
- Project UE automation should use `scripts/ue_mcp_client.py`, `scripts/ue_mcp_smoke.py`, `scripts/ue_tdd_pipeline.py`, and project scripts under `Content/Python`.
- If the editor is running, save dirty packages through UE MCP before closing or restarting it.
- If UE MCP is unavailable and the editor may have unsaved changes, do not force-close the editor unless the user explicitly accepts the risk.

### Compile Rule
- Do not use Live Coding or Hot Reload as verification.
- For C++ verification, save through MCP, close/restart the editor when needed, then run UBT or `scripts/ue_tdd_pipeline.py`.
- `--check-only` can inspect logs but is not a compile verification.

### Current MVP Acceptance
- Current-goal status and semantic freezes live in the rolling pointer `docs/production/current-goal-acceptance.md`; this list is only the baseline regression floor.
- UE editor and game defaults open `/Game/GameXXK/Maps/L_DesktopTrainingHUD`.
- The pure-2D desktop workbench is visible without entering a 3D town or accepting its quest.
- The desktop `挑战` action is directly clickable for an unlocked/replayable stage, leaves the quest state unchanged, and displays the existing full-screen `UGameXXKBattleBoardWidget` on the same map.
- Leaving that training battle restores the 2D workbench without loading a 3D map.
- Legacy `L_Main`/Qingshan town, NPC `F`, north-gate and route-map flows remain explicit regression surfaces only; run them when the user or the scoped task specifically requests 3D/legacy verification.

## Navigation

| Resource | Purpose |
|---|---|
| `docs/design/agent-operating-guide.md` | Low-context handoff checklist |
| `docs/production/current-goal-acceptance.md` | Rolling "current goal" pointer (source of truth for current state) |
| `docs/production/optimization-plan.md` | Project self-optimization plan |
| `docs/production/*` | Production-unit state files and acceptance records |
| `scripts/README.md` | Script index and entry navigation |
| `scripts/harness_state_validator.py` | Validate production-unit files |
| `scripts/ai_production_loop.py` | Run lightweight project-management and optional gameplay verification loop |
| `scripts/gamexxk_real_play_flow_mcp.py` | Real PIE/MCP playable-flow harness |
