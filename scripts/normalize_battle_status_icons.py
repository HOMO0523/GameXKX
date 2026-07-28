"""Build the one-accent-color battle-status icon revision from approved v3 art.

The original v3 icon silhouettes, paper badges, ink outlines and alpha edges are
kept intact.  Only coloured interior pixels become one muted mineral accent per
icon; the rice-paper base and dark ink remain neutral presentation materials.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "StatusIcons"

# Muted mineral colours: one accent per icon, intentionally no pure primaries.
ACCENTS: dict[str, tuple[int, int, int]] = {
    "armor_shield": (102, 122, 136),
    "momentum_seal": (146, 106, 69),
    "agility_wing": (98, 128, 132),
    "vulnerability_mask": (142, 95, 111),
    "bleed_drop": (139, 79, 75),
    "poison_vial": (104, 125, 82),
    "burn_flame": (166, 99, 77),
    "mark_target": (139, 83, 86),
    "guard_shield": (85, 111, 132),
    "rot_spiral": (103, 91, 128),
    "immunity_talisman": (101, 132, 127),
    "tactic_seal": (111, 94, 130),
    "terrain_redirect": (112, 133, 98),
}


def _is_colored_interior(r: int, g: int, b: int, a: int) -> bool:
    """Keep neutral paper and charcoal; collapse only the coloured glyph paint."""
    if a == 0:
        return False
    maximum = max(r, g, b)
    minimum = min(r, g, b)
    chroma = maximum - minimum
    # Paper lives in the bright band, ink in the dark band.  The middle band
    # with meaningful chroma is the generated icon silhouette/accent.
    return 80 <= maximum <= 205 and chroma >= 25


def _convert_icon(icon_id: str, accent: tuple[int, int, int]) -> dict[str, object]:
    source = SOURCE_DIR / f"battle_status_{icon_id}_inkflat_v3.png"
    if not source.is_file():
        raise RuntimeError(f"missing approved v3 icon: {source}")
    with Image.open(source) as opened:
        image = opened.convert("RGBA")
    pixels = list(image.get_flattened_data())
    converted = [
        (*accent, a) if _is_colored_interior(r, g, b, a) else (r, g, b, a)
        for r, g, b, a in pixels
    ]
    image.putdata(converted)

    final_path = SOURCE_DIR / f"battle_status_{icon_id}_inkflat_v4.png"
    image.save(final_path)

    # Preserve an equivalent source image for review/re-import while keeping
    # the colour-key convention used by the previous approved status set.
    chroma = Image.new("RGBA", image.size, (0, 255, 0, 255))
    chroma.alpha_composite(image)
    chroma_path = SOURCE_DIR / f"battle_status_{icon_id}_inkflat_v4_chroma_source.png"
    chroma.convert("RGB").save(chroma_path)

    return {
        "id": icon_id,
        "size": image.size,
        "final": str(final_path),
        "chroma_source": str(chroma_path),
        "accent": accent,
        "transparent_corner": image.getpixel((0, 0))[3] == 0,
    }


def main() -> None:
    results = [_convert_icon(icon_id, accent) for icon_id, accent in ACCENTS.items()]
    if not all(result["transparent_corner"] for result in results):
        raise RuntimeError("one or more generated status icons lost transparent corners")
    print({"ok": True, "count": len(results), "icons": results})


if __name__ == "__main__":
    main()
