#!/usr/bin/env python3
"""Build deterministic aspect variants from the approved MasterV2 paper panel."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = (
    PROJECT_ROOT
    / "SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_PanelLarge.png"
)
TARGET = SOURCE.with_name("T_MasterV2_PanelTall.png")
REPORT = PROJECT_ROOT / "Saved/Codex/UIAudit-20260827/panel-variant-report.json"

SOURCE_EDGE = 40
TARGET_SIZE = (726, 1816)  # 2x the authored 363x908 warehouse panel.
TARGET_EDGE = 50  # Renders as 25 logical px, matching the full backpack paper.


def _resize(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    return image.resize(size, Image.Resampling.LANCZOS)


def build_tall_panel(source: Image.Image) -> Image.Image:
    source = source.convert("RGBA")
    source_width, source_height = source.size
    inner_width = TARGET_SIZE[0] - TARGET_EDGE * 2
    inner_height = TARGET_SIZE[1] - TARGET_EDGE * 2
    source_inner_height = source_height - SOURCE_EDGE * 2

    # Preserve the paper grain scale horizontally by taking only the centered
    # source band needed by the tall panel. The vertical center is extended by
    # roughly 11% at final authored scale instead of squeezing the full wide
    # sheet into the warehouse aspect ratio.
    center_source_width = round(inner_width * SOURCE_EDGE / TARGET_EDGE)
    center_left = (source_width - center_source_width) // 2
    center_right = center_left + center_source_width

    output = Image.new("RGBA", TARGET_SIZE, (0, 0, 0, 0))

    corners = {
        (0, 0): (0, 0, SOURCE_EDGE, SOURCE_EDGE),
        (TARGET_SIZE[0] - TARGET_EDGE, 0): (
            source_width - SOURCE_EDGE,
            0,
            source_width,
            SOURCE_EDGE,
        ),
        (0, TARGET_SIZE[1] - TARGET_EDGE): (
            0,
            source_height - SOURCE_EDGE,
            SOURCE_EDGE,
            source_height,
        ),
        (TARGET_SIZE[0] - TARGET_EDGE, TARGET_SIZE[1] - TARGET_EDGE): (
            source_width - SOURCE_EDGE,
            source_height - SOURCE_EDGE,
            source_width,
            source_height,
        ),
    }
    for destination, source_box in corners.items():
        output.alpha_composite(
            _resize(source.crop(source_box), (TARGET_EDGE, TARGET_EDGE)),
            destination,
        )

    output.alpha_composite(
        _resize(
            source.crop((center_left, 0, center_right, SOURCE_EDGE)),
            (inner_width, TARGET_EDGE),
        ),
        (TARGET_EDGE, 0),
    )
    output.alpha_composite(
        _resize(
            source.crop(
                (
                    center_left,
                    source_height - SOURCE_EDGE,
                    center_right,
                    source_height,
                )
            ),
            (inner_width, TARGET_EDGE),
        ),
        (TARGET_EDGE, TARGET_SIZE[1] - TARGET_EDGE),
    )
    output.alpha_composite(
        _resize(
            source.crop((0, SOURCE_EDGE, SOURCE_EDGE, source_height - SOURCE_EDGE)),
            (TARGET_EDGE, inner_height),
        ),
        (0, TARGET_EDGE),
    )
    output.alpha_composite(
        _resize(
            source.crop(
                (
                    source_width - SOURCE_EDGE,
                    SOURCE_EDGE,
                    source_width,
                    source_height - SOURCE_EDGE,
                )
            ),
            (TARGET_EDGE, inner_height),
        ),
        (TARGET_SIZE[0] - TARGET_EDGE, TARGET_EDGE),
    )
    output.alpha_composite(
        _resize(
            source.crop(
                (
                    center_left,
                    SOURCE_EDGE,
                    center_right,
                    source_height - SOURCE_EDGE,
                )
            ),
            (inner_width, inner_height),
        ),
        (TARGET_EDGE, TARGET_EDGE),
    )
    return output


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    if not SOURCE.is_file():
        raise FileNotFoundError(SOURCE)
    result = build_tall_panel(Image.open(SOURCE))
    TARGET.parent.mkdir(parents=True, exist_ok=True)
    result.save(TARGET, format="PNG", optimize=False)
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(
        json.dumps(
            {
                "source": str(SOURCE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "source_sha256": sha256(SOURCE),
                "target": str(TARGET.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "target_sha256": sha256(TARGET),
                "target_size": list(result.size),
                "source_edge": SOURCE_EDGE,
                "target_edge": TARGET_EDGE,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(REPORT.read_text(encoding="utf-8"), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
