"""Transient playtest helper: collect runtime memory/performance evidence."""

import json

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main():
    out = {}
    world = _world()
    if not world:
        print(json.dumps({"ok": False, "reason": "no_pie_world"}, ensure_ascii=False))
        return
    # Console values.
    for name in ("r.Streaming.PoolSize", "r.TextureStreaming", "r.Streaming.MaxNumTexturesToStreamPerFrame"):
        try:
            value = unreal.SystemLibrary.execute_console_command(world, name)
            out[name] = str(value)
        except Exception as exc:
            out[name] = f"ERR:{exc}"
    # Enable stat output for the log.
    try:
        unreal.SystemLibrary.execute_console_command(world, "stat unit")
        out["stat_unit"] = "enabled"
    except Exception as exc:
        out["stat_unit"] = f"ERR:{exc}"
    # Atlas cache stats through the public board seam.
    try:
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        board = pc.get_battle_board_widget_for_test() if pc else None
        if board:
            stats = board.get_atlas_cache_stats_for_test()
            out["atlas_cache"] = {
                "resident_bytes": int(stats.resident_bytes) if hasattr(stats, "resident_bytes") else None,
                "resident_count": int(stats.resident_count) if hasattr(stats, "resident_count") else None,
                "fallback_count": int(stats.fallback_count) if hasattr(stats, "fallback_count") else None,
                "eviction_count": int(stats.eviction_count) if hasattr(stats, "eviction_count") else None,
            }
        else:
            out["atlas_cache"] = "board_missing"
    except Exception as exc:
        out["atlas_cache"] = f"ERR:{exc}"
    print(json.dumps(out, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
