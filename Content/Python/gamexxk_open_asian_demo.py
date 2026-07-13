"""Open the project-owned Asian Village demo map for focused PIE verification."""

import unreal


MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"


def main():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem is None or not subsystem.load_level(MAP):
        raise RuntimeError(f"failed to load {MAP}")
    print(MAP)


if __name__ == "__main__":
    main()
