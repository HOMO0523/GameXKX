#!/usr/bin/env python3
"""One-shot patch: replace the legacy 370x136 reward-box offsets with full card-face row geometry."""

from __future__ import annotations

import sys
from pathlib import Path

TARGET = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\UI\GameXXKBattleBoardWidget.cpp")

OLD = "\t\tRewardSlot->SetOffsets(FMargin(-185.0f, -116.0f, 370.0f, 136.0f));"
NEW_LINES = [
    "\t\t// Three vertical BuildCardFace reward cards (RewardCardSize each, 5px",
    "\t\t// slot padding per side) need a row sized for the full card faces.",
    "\t\t// The previous 370x136 strip was the legacy small-card container and",
    "\t\t// clipped the 206x285 faces down to horizontal slivers.",
    "\t\tRewardCardBoxSlotOffsets = FMargin(",
    "\t\t\t-185.0f,",
    "\t\t\t-RewardCardSize.Y * 0.5f,",
    "\t\t\tstatic_cast<float>(MaximumVisibleRewardCards) * (RewardCardSize.X + 10.0f),",
    "\t\t\tRewardCardSize.Y);",
    "\t\tRewardSlot->SetOffsets(RewardCardBoxSlotOffsets);",
]


def main() -> int:
    raw = TARGET.read_bytes()
    crlf = b"\r\n" in raw
    eol = "\r\n" if crlf else "\n"
    old = OLD.replace("\n", eol)
    new = (eol.join(NEW_LINES)).replace("\n", eol)
    old_b = old.encode("utf-8")
    new_b = new.encode("utf-8")
    count = raw.count(old_b)
    if count != 1:
        print(f"ABORT: expected exactly 1 occurrence, found {count}")
        return 1
    patched = raw.replace(old_b, new_b, 1)
    TARGET.write_bytes(patched)
    print(f"patched 1 occurrence (crlf={crlf})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
