"""Deterministic text-free ink-and-paper assets for the GameXXK UI master."""

from __future__ import annotations

import json
import random
from pathlib import Path

from PIL import Image, ImageDraw


PAPER = (232, 215, 179, 255)
INK = (43, 40, 34, 255)
SECONDARY_INK = (93, 85, 72, 210)
CINNABAR = (161, 79, 54, 255)


def make_torn_paper(
    size: tuple[int, int], seed: int, edge_strength: int
) -> Image.Image:
    width, height = size
    rng = random.Random(seed)
    step = max(12, min(width, height) // 8)
    points: list[tuple[int, int]] = []
    for x in range(4, width - 3, step):
        points.append((x, rng.randint(2, 2 + edge_strength)))
    for y in range(4, height - 3, step):
        points.append((width - 3 - rng.randint(0, edge_strength), y))
    for x in range(width - 4, 3, -step):
        points.append((x, height - 3 - rng.randint(0, edge_strength)))
    for y in range(height - 4, 3, -step):
        points.append((2 + rng.randint(0, edge_strength), y))

    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    image = Image.new("RGBA", size, PAPER)
    image.putalpha(mask)
    draw = ImageDraw.Draw(image)
    for _ in range(max(80, width * height // 900)):
        x = rng.randrange(width)
        y = rng.randrange(height)
        radius = rng.choice((1, 1, 2))
        shade = rng.choice(((92, 78, 56, 12), (255, 248, 221, 15)))
        draw.ellipse(
            (x - radius, y - radius, x + radius, y + radius), fill=shade
        )
    draw.line(points + [points[0]], fill=SECONDARY_INK, width=2, joint="curve")
    image.putalpha(mask)
    return image


def make_ink_button(size: tuple[int, int], state: str) -> Image.Image:
    image = make_torn_paper(size, 240804 + sum(map(ord, state)), 3)
    draw = ImageDraw.Draw(image)
    width, height = size
    inset = 7 if state != "pressed" else 9
    outline = INK if state in {"hover", "primary", "danger"} else SECONDARY_INK
    draw.rectangle(
        (inset, inset, width - inset - 1, height - inset - 1),
        outline=outline,
        width=3,
    )
    if state == "hover":
        draw.line(
            (inset + 8, height - inset - 7, width - inset - 8, height - inset - 7),
            fill=INK,
            width=3,
        )
    elif state == "pressed":
        draw.rectangle(
            (inset + 2, inset + 2, width - inset - 3, height - inset - 3),
            fill=(43, 40, 34, 18),
        )
    elif state == "primary":
        draw.line(
            (inset + 12, height // 2 + 8, width - inset - 12, height // 2 + 5),
            fill=(43, 40, 34, 76),
            width=8,
        )
    elif state == "danger":
        draw.ellipse((width - 31, 10, width - 13, 28), fill=CINNABAR)
    elif state == "disabled":
        draw.line((22, height - 18, width - 22, 18), fill=(93, 85, 72, 95), width=3)
        draw.line((42, height - 14, width - 42, 22), fill=(93, 85, 72, 72), width=2)
    return image


def make_ink_tab(size: tuple[int, int], state: str) -> Image.Image:
    image = make_ink_button(size, "hover" if state == "selected" else state)
    if state == "selected":
        draw = ImageDraw.Draw(image)
        draw.line(
            (10, size[1] - 10, size[0] - 10, size[1] - 12),
            fill=INK,
            width=7,
        )
    return image


def make_nav_disc(size: int, state: str) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    outer = (3, 3, size - 4, size - 4)
    draw.ellipse(
        outer,
        fill=PAPER,
        outline=INK if state in {"hover", "selected"} else SECONDARY_INK,
        width=4,
    )
    if state == "selected":
        draw.arc((10, 10, size - 11, size - 11), 205, 515, fill=INK, width=7)
    elif state == "reminder":
        draw.ellipse((size - 24, 5, size - 6, 23), fill=CINNABAR)
    elif state == "locked":
        draw.ellipse(outer, fill=(43, 40, 34, 78), outline=INK, width=4)
    return image


def make_nav_icon(size: int, kind: str) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if kind == "backpack":
        draw.rounded_rectangle(
            (18, 24, size - 18, size - 13), 8, outline=INK, width=5
        )
        draw.arc((26, 10, size - 26, 38), 180, 360, fill=INK, width=5)
    elif kind == "companion":
        draw.ellipse((12, 15, 38, 41), outline=INK, width=5)
        draw.ellipse((size - 39, 22, size - 13, 48), outline=INK, width=5)
        draw.arc((7, 28, 45, size - 8), 190, 350, fill=INK, width=5)
        draw.arc((size - 46, 35, size - 8, size - 5), 190, 350, fill=INK, width=5)
    elif kind == "codex":
        draw.polygon(
            ((7, 20), (size // 2 - 2, 28), (size // 2 - 2, size - 12), (8, size - 20)),
            outline=INK,
        )
        draw.polygon(
            ((size - 7, 20), (size // 2 + 2, 28), (size // 2 + 2, size - 12), (size - 8, size - 20)),
            outline=INK,
        )
        draw.line((size // 2, 27, size // 2, size - 12), fill=INK, width=4)
    elif kind == "task":
        draw.rounded_rectangle((16, 9, size - 16, size - 9), 5, outline=INK, width=5)
        for y, end in ((25, size - 25), (38, size - 25), (51, size - 34)):
            draw.line((25, y, end, y), fill=INK, width=4)
    elif kind == "route":
        nodes = ((13, size - 18), (28, 24), (45, 43), (size - 13, 14))
        draw.line(tuple(value for node in nodes for value in node), fill=INK, width=5)
        for x, y in nodes:
            draw.ellipse((x - 5, y - 5, x + 5, y + 5), fill=PAPER, outline=INK, width=3)
    else:
        raise ValueError(f"unknown nav icon: {kind}")
    return image


def _draw_card_motif(draw: ImageDraw.ImageDraw, size: tuple[int, int], family: str) -> None:
    width, height = size
    if family == "role":
        draw.arc((18, 18, width - 19, 105), 185, 355, fill=INK, width=4)
    elif family == "monster":
        draw.polygon(((18, 48), (38, 18), (58, 48)), outline=INK)
        draw.polygon(((width - 58, 48), (width - 38, 18), (width - 18, 48)), outline=INK)
    elif family == "general":
        draw.ellipse((width // 2 - 14, 18, width // 2 + 14, 46), outline=INK, width=3)
    elif family == "terrain":
        draw.line((18, height - 35, 66, height - 66, 108, height - 42, width - 18, height - 88), fill=INK, width=4)
    elif family == "rare":
        draw.polygon(((width // 2, 12), (width // 2 + 15, 34), (width // 2, 56), (width // 2 - 15, 34)), outline=INK)
    elif family == "boss":
        draw.line((20, 32, 52, 15, 82, 38, width - 82, 38, width - 52, 15, width - 20, 32), fill=INK, width=5)


def make_slot(
    size: tuple[int, int],
    family: str,
    state: str,
    tint: tuple[int, int, int] | None = None,
) -> Image.Image:
    base = (*tint, 255) if tint else PAPER
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    width, height = size
    draw.rectangle(
        (4, 4, width - 5, height - 5), fill=base, outline=SECONDARY_INK, width=3
    )
    if family == "equipment":
        draw.line((12, 11, width - 13, 11), fill=(255, 248, 221, 120), width=3)
    elif family.startswith("card_"):
        _draw_card_motif(draw, size, family.removeprefix("card_"))
    if state in {"hover", "selected"}:
        draw.rectangle(
            (8, 8, width - 9, height - 9),
            outline=INK,
            width=4 if state == "selected" else 2,
        )
    if state == "locked":
        draw.rectangle(
            (4, 4, width - 5, height - 5),
            fill=(43, 40, 34, 82),
            outline=INK,
            width=3,
        )
    return image


def build_component_assets(output_root: Path) -> dict[str, dict]:
    output_root.mkdir(parents=True, exist_ok=True)
    records: dict[str, dict] = {}

    def write(key: str, image: Image.Image) -> None:
        filename = f"UIV4_{key}.png"
        image.save(output_root / filename)
        records[key] = {
            "file": filename,
            "size": list(image.size),
            "textBaked": False,
        }

    for state in ("normal", "hover", "pressed", "primary", "danger", "disabled"):
        write(f"button_{state}", make_ink_button((220, 72), state))
    for state in ("normal", "hover", "pressed", "selected", "disabled"):
        write(f"tab_{state}", make_ink_tab((160, 58), state))
    for state in ("normal", "hover", "selected", "reminder", "locked"):
        write(f"nav_{state}", make_nav_disc(112, state))
    for kind in ("backpack", "companion", "codex", "task", "route"):
        write(f"nav_{kind}", make_nav_icon(72, kind))
    for state in ("empty", "hover", "selected", "locked"):
        write(f"item_slot_{state}", make_slot((104, 104), "item", state))
    for state in ("empty", "hover", "selected"):
        write(
            f"equipment_slot_{state}",
            make_slot((124, 130), "equipment", state),
        )
    card_tints = {
        "role": (222, 205, 170),
        "monster": (210, 197, 178),
        "general": (218, 211, 185),
        "terrain": (196, 207, 184),
        "rare": (204, 194, 210),
        "boss": (213, 186, 167),
    }
    for family, tint in card_tints.items():
        write(
            f"card_frame_{family}",
            make_slot((300, 420), f"card_{family}", "empty", tint),
        )
    write("panel_large", make_torn_paper((1440, 780), 240811, 9))
    write("panel_medium", make_torn_paper((760, 520), 240812, 7))
    write("panel_small", make_torn_paper((420, 260), 240813, 5))
    write("progress_track", make_ink_button((420, 24), "normal"))
    fill = Image.new("RGBA", (280, 10), (0, 0, 0, 0))
    ImageDraw.Draw(fill).rounded_rectangle(
        (0, 0, 279, 9), 5, fill=(91, 126, 96, 255)
    )
    write("progress_fill", fill)
    write("resource_strip", make_torn_paper((680, 92), 240814, 4))
    write("tooltip_panel", make_torn_paper((520, 240), 240815, 5))
    (output_root / "component-assets.json").write_text(
        json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return records
