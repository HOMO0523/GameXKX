from __future__ import annotations

import unreal


def main() -> None:
    print("SpriteEditorSubsystem exists:", hasattr(unreal, "SpriteEditorSubsystem"))
    if hasattr(unreal, "SpriteEditorSubsystem"):
        subsys = unreal.get_editor_subsystem(unreal.SpriteEditorSubsystem)
        print("members:", [m for m in dir(subsys) if not m.startswith("_")])
    print("PaperSpriteSheetImporter exists:", hasattr(unreal, "PaperSpriteSheetImporter"))
    for name in dir(unreal):
        if "Sprite" in name and ("Factory" in name or "Importer" in name or "Subsystem" in name):
            print("unreal.", name)


if __name__ == "__main__":
    main()
