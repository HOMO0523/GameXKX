"""Read the live battle resource-bar material parameters without mutating PIE state.

This is intentionally a diagnostic companion to ``gamexxk_probe_real_play_flow``.
It verifies the boundary from authoritative vitals -> UMG widget -> dynamic material
parameters, so visual fill defects can be located without relying on screenshots.
"""

from __future__ import annotations

import json

import unreal


def _path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _read_material_parameter(material, parameter_name):
    if material is None:
        return {"value": None, "error": "material_missing"}
    try:
        return {"value": float(material.get_scalar_parameter_value(parameter_name))}
    except Exception as exc:
        return {"value": None, "error": str(exc)}


def _image_material_summary(resource_widget, image_name, progress_name):
    result = {"image": image_name, "progress_bar": progress_name, "material_path": "", "parameters": {}}
    if resource_widget is None:
        result["error"] = "resource_widget_missing"
        return result
    image = None
    progress = None
    try:
        resource_path = _path(resource_widget)
        result["resource_widget_path"] = resource_path
        # Native UMG children are private transient UPROPERTY fields, rather
        # than standalone loadable widget assets. Probe the actual instance.
        image_property = "health_bar" if image_name == "HealthBar" else "mana_bar"
        progress_property = "health_progress_bar" if progress_name == "HealthProgressBar" else "mana_progress_bar"
        image = resource_widget.get_editor_property(image_property)
        progress = resource_widget.get_editor_property(progress_property)
    except Exception as exc:
        result["error"] = f"image_lookup_failed:{exc}"
        return result
    if progress is not None:
        # Python bindings do not expose UProgressBar::GetPercent/GetBarFillType
        # consistently in UE 5.8; the authoritative percent is read from the
        # native resource widget below and the native automation test verifies
        # the LeftToRight fill type.
        result["widget_present"] = True
        result["expected_fill_direction"] = "LeftToRight"
    if image is None:
        result["error"] = "legacy_image_missing"
        return result
    try:
        material = image.get_dynamic_material()
    except Exception as exc:
        result["error"] = f"dynamic_material_unavailable:{exc}"
        return result
    result["material_path"] = _path(material)
    for parameter_name in ("FillPercent", "FillLeft", "FillRight", "FillTop", "FillBottom"):
        result["parameters"][parameter_name] = _read_material_parameter(material, parameter_name)
    return result


def _get_world():
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        return subsystem.get_game_world() if subsystem else None
    except Exception:
        return None


def main():
    result = {"ok": False, "units": {}, "errors": []}
    world = _get_world()
    if world is None:
        result["errors"].append("pie_world_missing")
        print(json.dumps(result, ensure_ascii=False))
        return
    try:
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        board = controller.get_battle_board_widget_for_test() if controller else None
    except Exception as exc:
        board = None
        result["errors"].append(f"battle_board_lookup_failed:{exc}")
    if board is None:
        result["errors"].append("battle_board_missing")
        print(json.dumps(result, ensure_ascii=False))
        return
    for unit_id in ("MoneyRat", "BlackBear", "Tiger", "CompanionInstance.Companion_Blade_01.HudFixture", "Player", "Npc.TusiChief"):
        unit_result = {}
        try:
            unit_hud = board.get_projected_unit_hud_for_test(unreal.Name(unit_id))
            resource = unit_hud.get_resource_widget_for_test() if unit_hud else None
            unit_result["health_percent"] = float(resource.get_health_percent_for_test()) if resource else None
            unit_result["mana_percent"] = float(resource.get_mana_percent_for_test()) if resource else None
            unit_result["health"] = _image_material_summary(resource, "HealthBar", "HealthProgressBar")
            unit_result["mana"] = _image_material_summary(resource, "ManaBar", "ManaProgressBar")
        except Exception as exc:
            unit_result["error"] = str(exc)
        result["units"][unit_id] = unit_result
    result["ok"] = True
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
