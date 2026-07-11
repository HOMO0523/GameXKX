# Qingshan PCG Whitebox B0R Acceptance

Status: **DONE_WITH_CONCERNS — structural whitebox evidence only.** The saved B0R map, deterministic layout, four fixed cameras, production gameplay flow, and host validation pass. Visual review does not grant beauty, atmosphere, material, or final-style approval. Two requested reads remain ambiguous in the current canonical captures: the gate does not clearly read on the image's right side in the canonical top-down orientation, and the same-tone bridge/river/bank proxies do not make the bridge connection visually unambiguous.

## Canonical inputs and generated scope

- Config: `Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json`, schema `1`, revision `B0R`, seed `20260711`, `runtime_generation=false`.
- Protected source map: `/Game/GameXXK/Maps/L_QingshanInn`.
- Isolated generated map: `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R`.
- Generated asset root: `/Game/GameXXK/Environment/TownPCG/B0R`.
- Coordinate reference: `QingshanInn_TownExit` / `NorthGateFAnchor`.
- Graphs: `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Buildings`, `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Foliage`, and `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Mountains`.
- Saved instance counts: buildings `16`, foliage `100`, mountains `24`; roads `3`, river splines `1`, terrain zones `4`, crossings `2`, cameras `4`.
- Canonical expected/observed layout SHA-256: `15d1b585739a2a180a6022974ec0c365144cbc29511c2426fc0058f1fd43dfbd`. Live validation reported maximum deviations of `1.8189894035458565e-12 cm` location, `1.5202314443740761e-05 degrees` rotation, and `4.973799150320701e-14` scale.
- QuickRoad status: `proxy_spline`. The installation probe found the `/Quick_Road/` content root and 55 assets, but the B0R map uses UE spline proxies; no QuickRoad actor or QuickRoad content path is used by this whitebox.

## Determinism and protected snapshots

The committed compact A/B snapshots are byte-identical and each records counts `16/100/24` plus the canonical layout hash above:

- `layout-run-a.json`: file SHA-256 `806882e2738a8f9ec932bb6567fb411aa3b4442ded2ce4214bd06232198c080b`.
- `layout-run-b.json`: file SHA-256 `806882e2738a8f9ec932bb6567fb411aa3b4442ded2ce4214bd06232198c080b`.

Run limitation: the A/B files are the compact evidence from the preceding complete-regeneration task. Task 8 did not perform a third regeneration or commit raw coordinator transcripts; it performed a fresh MCP smoke test, read-only live validator, fixed-camera verification, viewport capture, gameplay regression, and host suites against the saved map.

The live validator preserved these protected source-map transforms exactly:

| Actor | Location cm | Rotation degrees (roll, pitch, yaw) | Scale |
|---|---:|---:|---:|
| `PlayerStart_QingshanInn` | `[-1250, -900, 120]` | `[-0, 35, 0]` | `[1, 1, 1]` |
| `QingshanInn_TownExit` | `[0, 1380, 120]` | `[0, 0, 0]` | `[1, 1, 1]` |

The validator also measured player-to-gate facing at `61.266437252561616 degrees`, bridge-to-road distance `1.9885572672162328 cm`, and bridge-to-river distance `2.575262923544422 cm`. It returned `success=true`, no errors, `source_map_clean=true`, and `runtime_generation_disabled=true` both before capture and after the gameplay regression.

## Fixed cameras and capture method

Each `view --camera-id` action verified the saved `CameraActor` transform and configured FOV before calling `EditorLevelLibrary.set_level_viewport_camera_info`. The top-down action used location `[-15500, 1380, 42120]` and rotation `[0, -90, 0]`. No ad hoc viewport camera offset was applied.

| Camera | World location cm | Rotation degrees (roll, pitch, yaw) | World target cm | FOV |
|---|---:|---:|---:|---:|
| `CAM_QS_GATE_ARRIVAL` | `[-2500, 3880, 3320]` | `[-0.000000499443445, -28.0228945094, -137.2311746080]` | `[-6500, 180, 420]` | `50` |
| `CAM_QS_TOWN_CORE` | `[-5000, 4880, 4620]` | `[-0.000000419893011, -22.9373955382, -139.0856167800]` | `[-12500, -1620, 420]` | `50` |
| `CAM_QS_MAIN_BRIDGE` | `[-9000, 380, 4120]` | `[-0.000000460341995, -25.1630197348, -139.7636416907]` | `[-15500, -5120, 120]` | `48` |
| `CAM_QS_SOUTH_DOCK` | `[-15000, -2620, 4320]` | `[-0.000000497028964, -27.3278313350, -139.7636416907]` | `[-21500, -8120, -80]` | `50` |

Windows Graphics Capture recorded the visible UE immersive viewport independently after each MCP camera action. The WGC JPEG payload was decoded and re-encoded as PNG without composition, generation, collage, or camera adjustment. Pillow verified the PNG signature `89504e470d0a1a0a`; every file is `1536 × 838` (wider than 16:9 and at least 1280×720).

| Evidence | SHA-256 |
|---|---|
| `topdown.png` | `7762529f4c8a9ab3fb3889b42efd12a9a22a26c791f568179b5e378cdfa462e1` |
| `gate-arrival.png` | `c01f031af86d7877148f58aa46731b65ddc886885707c47004d766ac421889ef` |
| `town-core.png` | `5689cbed164dff4d60f9d9f94796fdc0142cf73740d6f57772f226d844c97117` |
| `main-bridge.png` | `3bc6c6ba3de6443d0ab6c56151a729a0cdbc3a52c5fa7fa6d7400bbfce1d2ba7` |
| `south-dock.png` | `4e2a803d5af79c0102bb66b1e5fe40ad09fe396bcb3a29b4a4a23a9389480592` |

## Visual whitebox checklist

| Criterion | Result | Concrete evidence |
|---|---|---|
| North gate reads on the map's right side | **FAIL / concern** | The top-down view shows the entry/opening near the max-X/top edge in the canonical viewport orientation, not clearly on the image's right. Structural anchoring is valid, but the requested screen-space read is not. |
| Initial player-facing route does not point straight at the gate | **PASS** | Validator angle is `61.2664 degrees`; `gate-arrival.png` shows the route entering obliquely rather than aiming straight through the opening. |
| Main road bends left into town | **PASS** | `topdown.png` exposes the multi-segment main route, and `gate-arrival.png` shows the first broad leftward deflection into the clustered core. |
| Largest building is dominant but off-center | **PASS** | The largest block is visibly taller/wider than surrounding buildings and sits left of center in `town-core.png`, not on the central road axis. |
| Small buildings vary in size, height, setback, and yaw | **PASS** | `gate-arrival.png` and `town-core.png` show clearly different footprints/heights, irregular distances from the road, and non-parallel orientations. |
| River reads diagonally with at least two bends | **PASS** | `topdown.png` shows a diagonal course made from four proxy segments with multiple direction changes across the lower/central map. |
| Main bridge visibly connects both road and river banks | **FAIL / concern** | Structural distances to road and river are under 3 cm, but `main-bridge.png` uses the same light whitebox tone for road, bridge, bank, and river plates; both bank contacts are not visually unambiguous. |
| South dock is lower than the town core | **PASS** | `south-dock.png` shows the lower terrace/step; canonical targets are `-80 cm` at the dock versus `420 cm` at the town core, and terrain tops are `-550 cm` versus `-150 cm`. |
| Mountain ring encloses the map without blocking the gate opening | **PASS with whitebox limitation** | `topdown.png` shows perimeter masses on all four sides and an opening at the entry edge. The proxy mountains are oversized cuboids and partially intrude into perspective backgrounds, but the opening remains present. |
| Every fixed view has foreground, midground, and background | **PASS** | All four independent oblique captures contain near terrain/props, mid-distance route/buildings, and distant mountain-ring masses. The top-down image is a debug layout view and is not treated as a perspective depth-composition view. |

These results approve neither beauty nor style. The two failed/ambiguous visual reads are why this record is `DONE_WITH_CONCERNS` instead of unconditional visual acceptance.

## Verification and gameplay regression

- `python scripts/ue_mcp_smoke.py --report Saved/HarnessReports/qingshan-b0r-task8-smoke.json` — passed; MCP protocol `2025-11-25`, 68 toolsets, required editor/log/gameplay toolsets present, no GameFeature asset-manager error.
- Acceptance `snapshot`, `topdown`, and all four exact `view --camera-id` actions — passed; transform and FOV verification succeeded for every camera.
- Fresh read-only `Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py` after gameplay — `success=true`, no errors, map/hash/count/camera checks pass, PIE stopped, source map clean.
- `python scripts/gamexxk_real_play_flow_mcp.py --timeout 240 --report Saved/HarnessReports/qingshan-b0r-task8-real-flow.json` — `ok=true`; report SHA-256 `47e9d033e31d504bc7c17931f72b1db4844813be966dceb046d4a8e11ce934a8`.
- Gameplay covered player-facing `L_Main`, clickable Start/New Game, `/Game/GameXXK/Maps/L_QingshanInn`, visible town UI, real movement, `F` quest acceptance, follower chase, manual save with exact player and quest-NPC positions, north-gate `F`, route-map start/battle node clicks, and `L_BattleScene` with visible board, two party units, and two enemies. The harness stopped PIE and deleted its default save (`result=true`), so it left no harness save side effect.
- `python -m unittest scripts.test_qingshan_whitebox_config scripts.test_qingshan_whitebox_acceptance scripts.test_qingshan_whitebox_scripts scripts.test_run_qingshan_whitebox_b0r -v` — 115 tests passed.
- `python scripts/harness_state_validator.py` — exited `0`, `OK`.
- No C++ changed in Task 8, so no Live Coding, Hot Reload, or new compile claim was made.

## Known whitebox limitations and exclusions

- Buildings, foliage, mountains, terrain, road, river, bridge, and dock are primitive review proxies, not production art.
- The common white material and strong editor lighting reduce river/bank/bridge contrast. Mountain proxies are cuboid and can dominate the horizon.
- The canonical top-down camera proves layout extents but currently presents the entry edge at the top of the image rather than making the requested right-side gate read obvious.
- The captures include only the visible UE viewport and its viewport toolbar; no editor panels, collage, or generated overlay was composited.
- `Config/DefaultEditor.ini` and `docs/superpowers/specs/2026-07-11-qingshan-pcg-whitebox-b0r-design.md` were pre-existing unrelated unstaged changes and are excluded from this record's commit.
- No stylized keyframe, atmosphere image, Tripo asset, Paper2D asset, PaperZD asset, texture, material, or material instance was generated or modified for Task 8. Stylization remains a separate user-approved task.
