from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_blueprint_inspector_check.json"
MAP_WIDGET = "/Game/1Game/UI/UI_地图选择.UI_地图选择"
NODE_WIDGET = "/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡"
PLAYER_CONTROLLER = "/Game/1Game/Control/BP_PlayerController.BP_PlayerController"


def _fail(message: str, **extra):
    result = {"ok": False, "error": message, **extra}
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    raise RuntimeError(message)


def _inspect(asset_path: str) -> dict:
    library = getattr(unreal, "GameXXKBlueprintInspectorLibrary", None)
    if library is None:
        _fail("GameXXKBlueprintInspectorLibrary is not exposed to Python")

    inspector = getattr(library, "inspect_asset_to_json", None)
    if inspector is None:
        _fail("inspect_asset_to_json is not exposed to Python")

    raw = inspector(asset_path)
    try:
        parsed = json.loads(raw)
    except Exception as exc:
        _fail("inspector returned non-json", asset_path=asset_path, raw=raw, exception=str(exc))
    if not parsed.get("ok"):
        _fail("inspector returned ok=false", asset_path=asset_path, parsed=parsed)
    return parsed


def _require_graph_with_nodes(report: dict, graph_name_fragment: str) -> dict:
    graphs = report.get("graphs") or []
    for graph in graphs:
        if graph_name_fragment in graph.get("name", "") and graph.get("node_count", 0) > 0:
            return graph
    _fail(
        "expected graph with nodes was not exported",
        asset_path=report.get("asset_path"),
        graph_name_fragment=graph_name_fragment,
        graph_names=[g.get("name") for g in graphs],
        graph_node_counts=[g.get("node_count") for g in graphs],
    )


def main():
    map_report = _inspect(MAP_WIDGET)
    node_report = _inspect(NODE_WIDGET)
    controller_report = _inspect(PLAYER_CONTROLLER)

    _require_graph_with_nodes(map_report, "EventGraph")
    _require_graph_with_nodes(map_report, "获得地图关卡类型")
    _require_graph_with_nodes(node_report, "On_点击")
    _require_graph_with_nodes(controller_report, "EventGraph")

    result = {
        "ok": True,
        "assets": {
            MAP_WIDGET: {
                "class": map_report.get("class"),
                "graph_count": len(map_report.get("graphs") or []),
                "variable_count": len(map_report.get("variables") or []),
            },
            NODE_WIDGET: {
                "class": node_report.get("class"),
                "graph_count": len(node_report.get("graphs") or []),
                "variable_count": len(node_report.get("variables") or []),
            },
            PLAYER_CONTROLLER: {
                "class": controller_report.get("class"),
                "graph_count": len(controller_report.get("graphs") or []),
                "variable_count": len(controller_report.get("variables") or []),
            },
        },
        "reports": {
            "map_widget": map_report,
            "node_widget": node_report,
            "player_controller": controller_report,
        },
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": True, "report": str(REPORT_PATH)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
