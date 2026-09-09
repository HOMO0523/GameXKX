import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def main():
    out = {}
    world = probe._get_game_world()
    if world is None:
        world = unreal.EditorLevelLibrary.get_editor_world()
    for cmd in ("memreport -full", "stat memory"):
        try:
            unreal.SystemLibrary.execute_console_command(world, cmd)
            out[cmd] = "executed"
        except Exception as exc:
            out[cmd] = "err:" + str(exc)[:160]
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
