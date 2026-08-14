#!/usr/bin/env python3
"""Resolve the Unreal Engine installation used by GameXXK automation.

Resolution order:

1. ``GAMEXXK_UE_ROOT`` environment variable (set-but-invalid raises).
2. Known local install candidates.
3. Windows registry ``InstalledDirectory`` entries.

Nothing here launches or kills the editor; it only answers "where is UE".
"""

from __future__ import annotations

import os
from pathlib import Path


_UE_CANDIDATES = [
    Path(r"E:\epic\UE_5.8"),
    Path(r"E:\UE_5.8"),
    Path(r"D:\UE_5.8"),
    Path(r"C:\Program Files\Epic Games\UE_5.8"),
    Path(r"E:\epic\UE_5.7"),
    Path(r"E:\UE_5.7"),
    Path(r"D:\UE_5.7"),
    Path(r"C:\Program Files\Epic Games\UE_5.7"),
]

_REGISTRY_KEY_PATHS = (
    r"SOFTWARE\EpicGames\Unreal Engine",
    r"SOFTWARE\WOW6432Node\EpicGames\Unreal Engine",
)


def is_valid_ue_root(candidate: Path) -> bool:
    return (candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe").is_file()


def _registry_ue_roots() -> list[Path]:
    roots: list[Path] = []
    try:
        import winreg
    except ImportError:
        return roots
    for key_path in _REGISTRY_KEY_PATHS:
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path) as base:
                index = 0
                while True:
                    try:
                        version = winreg.EnumKey(base, index)
                    except OSError:
                        break
                    index += 1
                    try:
                        with winreg.OpenKey(base, version) as version_key:
                            installed, _ = winreg.QueryValueEx(version_key, "InstalledDirectory")
                        if installed:
                            roots.append(Path(str(installed)))
                    except OSError:
                        continue
        except OSError:
            continue
    return roots


def find_ue_root() -> Path:
    env_root = os.environ.get("GAMEXXK_UE_ROOT", "").strip()
    if env_root:
        candidate = Path(env_root)
        if not is_valid_ue_root(candidate):
            raise RuntimeError(
                f"GAMEXXK_UE_ROOT points to an invalid UE root (no UnrealEditor.exe): {env_root}"
            )
        return candidate
    for candidate in _UE_CANDIDATES:
        if is_valid_ue_root(candidate):
            return candidate
    for candidate in _registry_ue_roots():
        if is_valid_ue_root(candidate):
            return candidate
    raise RuntimeError(
        "Cannot find an Unreal Engine installation. "
        f"Checked candidates: {', '.join(str(c) for c in _UE_CANDIDATES)} and the registry. "
        "Set GAMEXXK_UE_ROOT to the engine root (e.g. D:\\UE_5.8)."
    )


def ue_editor_exe(root: Path | None = None) -> Path:
    return (root or find_ue_root()) / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"


def ue_editor_cmd_exe(root: Path | None = None) -> Path:
    return (root or find_ue_root()) / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"


def ue_build_bat(root: Path | None = None) -> Path:
    return (root or find_ue_root()) / "Engine" / "Build" / "BatchFiles" / "Build.bat"


if __name__ == "__main__":
    root = find_ue_root()
    print(f"UE root: {root}")
    print(f"Editor:  {ue_editor_exe(root)}")
    print(f"Cmd:     {ue_editor_cmd_exe(root)}")
    print(f"Build:   {ue_build_bat(root)}")
