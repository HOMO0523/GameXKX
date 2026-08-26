"""Convert the approved town-button review PNGs into real-alpha runtime sources."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "DesktopOverlay"
RAW_ROOT = SOURCE_ROOT / "Raw"
OUTPUT_SIZE = (512, 512)
ASSETS = {
    "enter": (
        RAW_ROOT / "T_DesktopTownEnterButton_checker.png",
        SOURCE_ROOT / "T_DesktopTownEnterButton.png",
    ),
    "exit": (
        RAW_ROOT / "T_DesktopTownExitButton_checker.png",
        SOURCE_ROOT / "T_DesktopTownExitButton.png",
    ),
}


def _foreground_pixel(red: int, green: int, blue: int) -> bool:
    brightest = max(red, green, blue)
    darkest = min(red, green, blue)
    darkness = 255 - brightest
    chroma = brightest - darkest
    # The review export contains a neutral 243-255 checkerboard. Approved art
    # is either visibly darker ink or warm/chromatic rice paper and state text.
    return darkness >= 18 or chroma >= 7


def _extract_alpha(source: Path) -> Image.Image:
    with Image.open(source) as opened:
        rgb = opened.convert("RGB")
    output = Image.new("RGBA", rgb.size, (0, 0, 0, 0))
    output.putdata(
        [
            (red, green, blue, 255)
            if _foreground_pixel(red, green, blue)
            else (0, 0, 0, 0)
            for red, green, blue in rgb.getdata()
        ]
    )
    # Resize in premultiplied space so the high-resolution binary matte becomes
    # a clean antialiased edge without reintroducing the neutral checkerboard.
    return output.convert("RGBa").resize(
        OUTPUT_SIZE,
        Image.Resampling.LANCZOS,
    ).convert("RGBA")


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _metrics(image: Image.Image) -> dict[str, object]:
    alpha = image.getchannel("A")
    values = list(alpha.getdata())
    corners = [
        alpha.getpixel((0, 0)),
        alpha.getpixel((image.width - 1, 0)),
        alpha.getpixel((0, image.height - 1)),
        alpha.getpixel((image.width - 1, image.height - 1)),
    ]
    return {
        "size": [image.width, image.height],
        "alpha_extrema": list(alpha.getextrema()),
        "alpha_bbox": list(alpha.getbbox() or (0, 0, 0, 0)),
        "corner_alpha": corners,
        "visible_ratio": sum(value > 8 for value in values) / len(values),
        "soft_alpha_ratio": sum(0 < value < 255 for value in values) / len(values),
    }


def main() -> int:
    report: dict[str, object] = {"schema_version": 1, "assets": {}}
    for label, (source, destination) in ASSETS.items():
        if not source.is_file():
            raise FileNotFoundError(source)
        image = _extract_alpha(source)
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination, format="PNG", optimize=True)
        metrics = _metrics(image)
        if metrics["size"] != [512, 512]:
            raise RuntimeError(f"{label} output is not 512 square")
        if any(value > 2 for value in metrics["corner_alpha"]):
            raise RuntimeError(f"{label} output corners are not transparent")
        if metrics["alpha_extrema"] != [0, 255]:
            raise RuntimeError(f"{label} output lacks full alpha range")
        report["assets"][label] = {
            "source": str(source.relative_to(PROJECT_ROOT)),
            "output": str(destination.relative_to(PROJECT_ROOT)),
            "sha256": _sha256(destination),
            **metrics,
        }

    report_path = SOURCE_ROOT / "desktop-town-button-alpha-report.json"
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
