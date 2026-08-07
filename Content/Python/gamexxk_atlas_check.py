"""Perceptual-hash an atlas texture in the editor via sparse sampling.

Driven through UE MCP. Samples a 32x32 grid from the texture's CPU copy and
returns a compact hash string; the caller diffs it against the same sampling
applied to the production source PNGs.
"""

from __future__ import annotations

import json
import sys

import unreal


def main(argv: list[str]) -> dict:
    name = argv[0] if argv else "character_00_hero_attack"
    package = f"/Game/GameXXK/BattleAnimations/Atlases/T_{name}_atlas"
    texture = unreal.load_asset(package)
    info = {"name": name, "loaded": texture is not None}
    if texture is None:
        return info

    size_x = texture.blueprint_get_size_x()
    size_y = texture.blueprint_get_size_y()
    info["size_x"] = size_x
    info["size_y"] = size_y

    try:
        colors = texture.blueprint_get_cpu_copy()
    except Exception as exc:  # pragma: no cover
        info["copy_err"] = str(exc)
        return info

    grid = 32
    step_x = max(1, size_x // grid)
    step_y = max(1, size_y // grid)
    cells = []
    for cy in range(grid):
        for cx in range(grid):
            px = min(size_x - 1, cx * step_x + step_x // 2)
            py = min(size_y - 1, cy * step_y + step_y // 2)
            idx = py * size_x + px
            c = colors[idx]
            cells.append(f"{c.b >> 4:x}{c.g >> 4:x}{c.r >> 4:x}")
    info["hash"] = "".join(cells)
    info["total_pixels"] = len(colors)
    return info


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False))
