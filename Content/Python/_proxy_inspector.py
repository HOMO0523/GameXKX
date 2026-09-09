import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def _geom_summary(geom):
    out = {}
    for attr in ("local_size", "absolute_size", "absolute_position"):
        try:
            value = getattr(geom, attr)
            out[attr] = [float(value.x), float(value.y)]
        except Exception as exc:
            out[attr] = "err:" + str(exc)
    return out


def main():
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    out = {"proxies": [], "proxy_like": []}
    try:
        widgets = board.widget_tree.get_all_widgets()
    except Exception as exc:
        print(json.dumps({"error": "widget_tree:" + str(exc)}, ensure_ascii=False))
        return
    for widget in widgets:
        name = str(widget.get_name())
        if "TargetProxy" in name:
            entry = {"name": name, "class": str(widget.get_class().get_name())}
            for getter in ("get_visibility", "is_visible"):
                try:
                    entry["visibility"] = str(getattr(widget, getter)())
                    break
                except Exception:
                    entry["visibility"] = "?"
            try:
                entry["enabled"] = bool(widget.get_is_enabled())
            except Exception:
                try:
                    entry["enabled"] = bool(widget.is_enabled())
                except Exception as exc:
                    entry["enabled"] = "err:" + str(exc)
            try:
                entry["opacity"] = float(widget.get_render_opacity())
            except Exception:
                entry["opacity"] = None
            try:
                entry["geometry"] = _geom_summary(widget.get_cached_geometry())
            except Exception as exc:
                entry["geometry"] = "err:" + str(exc)
            out["proxies"].append(entry)
        elif name.startswith("BattleUnitVisual"):
            entry = {"name": name}
            try:
                entry["geometry"] = _geom_summary(widget.get_cached_geometry())
            except Exception as exc:
                entry["geometry"] = "err:" + str(exc)
            out["proxy_like"].append(entry)
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
