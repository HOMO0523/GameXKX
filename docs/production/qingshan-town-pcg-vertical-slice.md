# Qingshan Town PCG Vertical Slice — Acceptance Record

Date: 2026-07-10 (Asia/Shanghai)

Evidence base commit: `903abaddf30c2151bda45f10186494c2b6d5cc8c`

Acceptance record commit: the commit containing this file, message `docs: record qingshan pcg vertical slice`

## Outcome and scope

The prototype vertical slice passed its structural validator, deterministic regeneration check, blockout visual review, content budgets, and the existing production-map gameplay regression. Performance was measured on an RTX 4060 Laptop GPU and is evidence only: this machine is not the agreed RTX 2060/RX 6600 target class, and the 60 FPS-capped sample does not establish uncapped or target-hardware performance acceptance.

Only `/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype` was cleared/regenerated. `/Game/GameXXK/Maps/L_QingshanInn` was never redirected or saved by the PCG workflow and was clean in both post-flow validators.

## Reproducible configuration

- Quick Road descriptor: `Plugins/Quick_Road/Quick_Road.uplugin`, version `1.0`, SHA-256 `e60210af589acc592ec67f908479225173eb92eaf2c195a61d914cc54ad8fb02`.
- Quick Road boundary: used for spline semantic tags and the 12 green cone markers only. Vendor plugin files were not changed. No startup descriptor rewrite was observed, so the authorized vendor UTF-8 restore was unnecessary.
- Config: `Config/GameXXK/TownPCG/QingshanTownVerticalSlice.json`; seed `20260710`; runtime generation disabled.
- Prototype map: `/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype`.
- Protected production map: `/Game/GameXXK/Maps/L_QingshanInn`.
- Graph: `/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice`; 12 CreatePoints transforms, 2 authored nodes, 2 verified edges.
- PCG owner: one `PCGVolume` labelled `QingshanTown_PCG_Buildings`, one `PCGComponent`, one generated instanced-static-mesh component, 12 instances (hard cap 16).
- Building mesh: `/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K`; one material slot; Nanite enabled; one accepted simple collision primitive; not complex-as-simple.
- Material: `/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/M_Qingshan_Building_Toon`.
- Albedo: `/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/T_Qingshan_Shop_A_Albedo`, 4096 × 4096.
- Fixed topology: city scope 4 points/closed; main road 4/open; both road edges 4/open; river 4/open; bridge at `(0,-10620,170)` cm.
- Marker budgets: buildings 12/16, green tree cones 12/48, props 0/24.
- Preserved actor observation: `PlayerStart_QingshanInn` remained at `(-1250,-900,120)` cm with pitch `35`, yaw `0`; `QingshanInn_TownExit` remained at `(0,1380,120)` cm.

## Preflight and structural validation

Commands and results:

```text
python -m unittest scripts.test_quick_road_install scripts.test_qingshan_shop_import scripts.test_qingshan_town_pcg_config scripts.test_qingshan_town_pcg_scripts scripts.test_run_qingshan_town_pcg_vertical_slice
60 tests, PASS

python scripts/harness_state_validator.py --require-units --json
ok=true, findings=[]

python scripts/ue_mcp_smoke.py --report Saved/HarnessReports/task6-preflight-mcp-smoke.json
ok=true; UE 5.8 MCP protocol 2025-11-25

run_project_python_file Content/Python/gamexxk_validate_qingshan_town_pcg_vertical_slice.py
success=true; errors=[]; building instances=12; graph=12/2/2; tree markers=12; source and prototype dirty sets=[]

python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
UBT GameXXKEditor Win64 Development: Result Succeeded; no Live Coding/Hot Reload
```

The Task 5 read-only validator was run before acceptance work and again after the full production gameplay regression. Both runs returned `success=true`, no errors, no dirty-package delta, and no dirty source map.

## Visual evidence

UE/MCP placed the level-editor camera with `EditorLevelLibrary.set_level_viewport_camera_info`; Windows Graphics Capture then recorded the visible Unreal window because UE editor-world `HighResShot` acknowledged the command but did not emit a file. Captures are 1536 × 835 and show the editor viewport at playable oblique angles:

- [North gate](evidence/qingshan-town-pcg/north-gate.png), SHA-256 `db2ff3d0c77142d2e3b111ba28a8abb42ee4e316065375fce665536120b1e02f`.
- [Market](evidence/qingshan-town-pcg/market.png), SHA-256 `79087cd7ac07b23ba2006bc3d7cca02fd0e499966473072159062a28189bafe5`.
- [Bridge](evidence/qingshan-town-pcg/bridge.png), SHA-256 `b332572e5f52ec7f9b9218458f7efeaa70dd35db6250413c3870062a0f6323e3`.
- [South wharf](evidence/qingshan-town-pcg/south-wharf.png), SHA-256 `6da4b74dc8a40017398b602e92f20db69a36728f0153c42e54e8387e289f670d`.

Inspection result: north gate → paired shop rows/main road → market → bridge → wharf reads as one route; shop fronts face the road; bridge, road, and river spline guides align; all 12 green cones remain outside the shop corridor at the perimeter/riverbank; no obvious shop/shop, shop/road, shop/bridge, or shop/river overlap; no missing or magenta shop material was visible.

Known visual limitations are intentional and not production-art approval: all 12 buildings reuse the single Qingshan shop; river/bridge/market/wharf/tree representations are prototype blockouts (river spline guide, gray plates, and green cones), not production water, structures, foliage, or final terrain dressing.

## Deterministic regeneration

The [stable layout evidence](evidence/qingshan-town-pcg/deterministic-layout.json) contains all 12 sorted world transforms. Procedure:

1. `--action snapshot` recorded 12 generated ISM transforms.
2. `--action clear` called `ClearTownPCG` while the current map was the prototype; result `success=true`, generated component count `0`.
3. `python scripts/run_qingshan_town_pcg_vertical_slice.py --timeout-seconds 60` used separate setup/finalize MCP calls and host polling; task `6` completed in 2 polls/2.0 seconds.
4. A second snapshot recorded 12 transforms and empty dirty map/content sets.

Both canonical representations use four decimal places (declared absolute tolerance `0.0001`) and hash to `b6492ad617270749d422fecbcf5ead6d19168ad57fb5cfe52b3bc335207dd7b7`. Counts remained 12 instances and graph `12/2/2`. There was no nested editor tick or game-thread wait.

## Performance measurement

Hardware: Intel Core i7-14650HX (16 cores/24 threads), NVIDIA RTX 4060 Laptop GPU, driver 572.83/internal 32.0.15.7283, 51,263,873,024 bytes RAM; Windows 11; D3D12 SM6. This does not match the target-hardware class.

Prototype PIE requested `1920x1080w`. The five representative route viewpoints were visited with `BugItGo` over 20 seconds while `Stat Unit`, `Stat GPU`, `Stat RHI`, and the UE CSV profiler were enabled. Exact console commands and counters are in [metrics.json](evidence/qingshan-town-pcg/metrics.json).

- 1,209 frames; average/median `16.6668 ms`; p95 `16.6709 ms`; p99 `16.7006 ms`; average `59.9994 FPS`; zero samples above 50 ms.
- Average game/render/GPU/RHI thread times: `7.6910 / 6.9768 / 5.4464 / 3.4128 ms`.
- RHI draw calls average/max: `322.8892 / 356`; primitives average/max: `127,631.9644 / 363,515`.
- GPU local memory used average/max: `4,782.9106 / 4,790.8359 MB` (whole editor/PIE process, not slice-only).
- Raw profile: `Saved/Profiling/CSV/Profile(20260710_183601).csv`, SHA-256 `2724a9583ce1c7cb21cee03b69e2cbed262f3c04ffe0ea38fd067c148bd6f73e`; deliberately uncommitted.
- Parser: `scripts/qingshan_town_acceptance.py::parse_csv_profile`, positive `FrameTime` samples with nearest-rank p95/p99.

The sample is visibly capped at approximately 60 FPS. Therefore this record does not claim sustained uncapped `<=16.67 ms`, performance margin, or target-hardware acceptance.

## Production gameplay regression

Command:

```text
python scripts/gamexxk_real_play_flow_mcp.py --timeout 45 --report Saved/HarnessReports/task6-gameplay-regression.json
```

Result `ok=true`; report SHA-256 `97ddad506b49f12cdc498603564e2212413a750c215505b96c5ccf830bc2ff14`. The current accepted production flow passed: player-facing main menu; Start/New Game → `L_QingshanInn`; visible town UI; real movement; `F` quest acceptance; follower chase; manual save with follower state, exact player location, and exact moved quest-NPC location; walk to north-gate exit and `F` → `L_RouteMap`; start node advancement; battle node → `L_BattleScene` with visible battle board, 2 party units, and 2 enemies. The harness stopped PIE and deleted its default save.

## Repository boundary

`Config/DefaultEditor.ini` was already modified before this task, remained unrelated, and is explicitly excluded. Raw MCP reports, gameplay screenshots, logs, and CSV profiles remain under `Saved`. The regenerated graph/map binary serialization was not included in the acceptance commit; the committed scope is this record, compact evidence, and focused acceptance helpers/tests only.
