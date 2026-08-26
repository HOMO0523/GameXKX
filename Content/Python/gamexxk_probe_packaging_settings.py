from __future__ import annotations

import unreal


def main() -> None:
    settings = unreal.get_default_object(unreal.ProjectPackagingSettings)
    try:
        maps = settings.get_editor_property("maps_to_cook")
        print("maps_to_cook count:", len(maps))
        for m in maps:
            print("  ", m.get("FilePath") if isinstance(m, dict) else str(m))
    except Exception as exc:
        print("maps_to_cook err:", exc)
    try:
        print("use_pak_file:", settings.get_editor_property("use_pak_file"))
    except Exception as exc:
        print("use_pak_file err:", exc)
    try:
        print("build_configuration:", settings.get_editor_property("build_configuration"))
    except Exception as exc:
        print("build_configuration err:", exc)


if __name__ == "__main__":
    main()
