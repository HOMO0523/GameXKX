# 1Game UI Framework Analysis

Generated from UE MCP plus `UGameXXKBlueprintInspectorLibrary`.

## Assets Read

- `/Game/1Game/UI/UI_地图选择`: main Slay-the-Spire-style route widget.
- `/Game/1Game/UI/UI_地图选择-关卡`: route node widget.
- `/Game/1Game/UI/UI_地图选择-关卡-线`: route line widget.
- `/Game/1Game/UI/UI_地图选择-Boss`: boss node widget, parented from the route node widget.
- `/Game/1Game/Control/BP_PlayerController`: original controller for route flow.
- `/Game/1Game/Control/BP_Gamemode`: original GameMode using that controller.
- `/Game/1Game/Data/*`: route structs, node enum, and scroll curve.

## Data Model

- `E_地图关卡类型`: `小怪`, `精英怪`, `篝火`, `宝箱`, `商人`, `随机事件`.
- `ST_地图选择`: array of `ST_地图选择信息`.
- `ST_地图选择信息`: node grid index, exists flag, route node widget reference, outgoing node indices, UI position, pulled flag, node type, visited flag.
- `ST_地图选择连到这个点上的线`: points, incoming indices, line-data indices.
- `ST_地图已经创建的线条`: source and target `FIntPoint`.
- `ST_线条数据`: line position, offsets, angle.

## Blueprint Logic Read

- `UI_地图选择` has functions:
  - `判断可以运行的x` with 62 nodes: picks a valid next X position while avoiding illegal overlaps.
  - `判断是否有同样的线条` with 13 nodes: deduplicates route lines.
  - `获得地图关卡类型` with 85 nodes: chooses room type from weighted rules and incoming room context.
  - `设置章节名称` with 9 nodes: writes chapter labels from controller state.
  - `EventGraph` with 432 nodes: creates node widgets, creates line widgets, randomizes/pulls positions, creates boss node, scrolls map, handles node-click selection, and reopens the map after a room.
- `UI_地图选择-关卡` has:
  - `EventGraph` with 106 nodes: initializes dynamic material, handles hover/click animation, sets icon by room type, calls `Ev 点击地图的关卡` on the parent map widget, and toggles clickability.
  - `On_点击` delegate graph.
- `BP_PlayerController` has:
  - `EventGraph` with 45 nodes.
  - `BeginPlay`: creates `UI_地图选择` and adds it to viewport.
  - `Ev_开始新的关卡`: records selected node info, switches on `E_地图关卡类型`, fades/hides map, and moves into the current room placeholder.
  - `Ev_结束当前关卡`: increments chapter/node counters, removes or reopens the map, and creates the next map after boss/section boundaries.

## Integration Decision

Do not directly drop `UI_地图选择` into the current GameXXK HUD as-is. Its click path casts to `/Game/1Game/Control/BP_PlayerController` and calls controller events, while GameXXK uses `UGameXXKMVPSubsystem` plus a C++ player controller/HUD. Direct use would display the map but make clicks unreliable.

Stable integration path:

1. Keep `UGameXXKMVPSubsystem` as the source of truth for save/load, quest, route, battle, and current screen.
2. Port the 1Game route data shape into a GameXXK route view model, preserving node type, grid index, position, visited, and outgoing edges.
3. Use 1Game UI assets as the visual layer only after an adapter owns click callbacks and forwards them to `SelectDungeonNode`, `ResolveEventReward`, `ResolveCampReward`, or battle entry.
4. Keep the old HUD route commands as automation fallback until the UMG map is verified in PIE.

## Adapter Status

Implemented safe adapter pass:

- `UGameXXKOneGameRouteMapWidget` keeps a soft reference to `/Game/1Game/UI/UI_地图选择.UI_地图选择_C`.
- It builds route nodes from `GameXXKMVPCommandRouter::BuildRouteMapNodes`.
- It maps GameXXK route node kinds to 1Game-style room types: Start, SmallEnemy, EliteEnemy, RandomEvent, Camp, Chest, Merchant, Boss.
- It rejects future-node clicks and forwards only enabled current-node clicks into `GameXXKMVPCommandRouter`.
- `AGameXXKMVPHUD` creates and refreshes this adapter.
- It creates one visual node for each generated route node and one connector line for each generated route edge.

This intentionally avoids executing 1Game's original `BP_PlayerController` logic. The next visual pass should replace the adapter's plain programmatic buttons with 1Game node/line assets while preserving the adapter-owned click path.

## GameXXK Route Model

The current MVP no longer treats the route as a hidden fixed linear index. `FGameXXKRuntimeState` stores a generated route graph:

- `RouteMapNodes`: node id, layer, column, room kind, normalized UI position, and outgoing node ids.
- `RouteMapEdges`: source and target node ids used by the route widget to draw connector lines.
- `VisitedRouteNodeIds`: completed nodes.
- `ReachableRouteNodeIds`: currently clickable choices.
- `CurrentRouteNodeId` and `PendingRouteNodeId`: active map node and active battle node.
- `RouteSeed`: reserved for later random route generation.

`EnterDungeon` currently builds a deterministic 12-node, 6-layer graph so the MVP has a stable testable structure while matching 1Game's data shape closely enough to swap in seeded/randomized generation later.

The command router exposes each real node as `RouteNode<ID>`. Legacy commands remain visible as compatibility shortcuts, but the route widget should use concrete node commands so branch choice, visited state, and save/load state stay unambiguous.

## Visual Dependency Note

The first direct widget-visual attempt showed that loading `/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡_C` can compile/load the parent route map dependency `/Game/1Game/UI/UI_地图选择`, which currently fails because a Blueprint function dependency named `FunCL_GetRandomArrayToIndex` is missing. That failure is outside the GameXXK command path, but it can still break automation if the adapter synchronously loads the 1Game widget classes.

Current stable rule:

1. Keep 1Game widget classes as soft references only.
2. Default `UGameXXKOneGameRouteMapWidget::bUseOneGameBlueprintVisualWidgets` to false.
3. Build safe GameXXK-owned fallback node and line visuals in the adapter's `WidgetTree`.
4. Only enable 1Game Blueprint visual widgets after the missing dependency is repaired or after the visual layer is rebuilt from safe textures/materials.

Fallback visual structure as of 2026-07-03:

- Route nodes are top-level `UBorder` widgets with child `UOverlay` content.
- Each fallback node overlay contains a `UImage` icon loaded from safe 1Game `Texture2D` assets and a `UTextBlock` label.
- Route connector lines are top-level `UBorder` widgets, sized and rotated between adjacent route nodes.
- Click handling remains on transparent `UButton` overlays so the visual layer can be swapped later without changing command routing.
- The adapter exposes read-only diagnostics for created visual widget class names and node labels, covered by `GameXXK.MVP.RouteMap.OneGameAdapter`.

Safe texture mapping:

- Start: `/Game/1Game/Texture/脚印.脚印`
- Small enemy: `/Game/1Game/Texture/小怪.小怪`, disabled `/Game/1Game/Texture/小怪灰色.小怪灰色`
- Elite/Boss: `/Game/1Game/Texture/精英怪.精英怪`, disabled `/Game/1Game/Texture/精英怪灰色.精英怪灰色`
- Camp: `/Game/1Game/Texture/篝火.篝火`, disabled `/Game/1Game/Texture/篝火灰色.篝火灰色`
- Chest: `/Game/1Game/Texture/宝箱.宝箱`, disabled `/Game/1Game/Texture/宝箱灰色.宝箱灰色`
- Merchant: `/Game/1Game/Texture/钱.钱`, disabled `/Game/1Game/Texture/钱灰色.钱灰色`
- Random event: `/Game/1Game/Texture/问号.问号`, disabled `/Game/1Game/Texture/问号灰色.问号灰色`
