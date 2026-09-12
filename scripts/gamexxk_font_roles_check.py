#!/usr/bin/env python3
"""Static guard for the GameXXK two-font rollout (no UE required).

Checks that:
  1. every row of docs/design/2026-09-12-body-font-rollout/font-roles.tsv is accounted for;
  2. no legacy role-less font call survives anywhere under Source/GameXXK;
  3. the style header exposes both role fonts and the body asset path used by the importer;
  4. the body-face source file on disk still matches the hash the importer pins.

Usage:
    python scripts/gamexxk_font_roles_check.py [--json]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "Source" / "GameXXK"
ROLE_TABLE = PROJECT_ROOT / "docs" / "design" / "2026-09-12-body-font-rollout" / "font-roles.tsv"
STYLE_HEADER = SOURCE_ROOT / "Public" / "UI" / "GameXXKInRunUiStyle.h"
IMPORTER = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_body_font.py"

BODY_FONT_PATH = (
    "/Game/GameXXK/UI/Fonts/Body/FF_Body_KeinannMaruPOP_Font."
    "FF_Body_KeinannMaruPOP_Font"
)
TITLE_FONT_PATH = (
    "/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font."
    "FF_Trial_ZhHans_JiangHuGuFeng_Font"
)
LEGACY_CALL = re.compile(r"FGameXXKInRunUiStyle::(?:Outlined)?Font\(")
ROLE_CALL = re.compile(
    r"FGameXXKInRunUiStyle::(?:TitleFont|BodyFont|OutlinedTitleFont|OutlinedBodyFont)\("
)


def first_argument(text: str, open_paren: int) -> str | None:
    """Return the first top-level argument of the call whose '(' is at open_paren."""
    depth, cursor, quote, escaped, current = 0, open_paren, None, False, []
    while cursor < len(text):
        ch = text[cursor]
        if quote:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            if depth >= 1:
                current.append(ch)
        elif ch in "\"'":
            quote = ch
            if depth >= 1:
                current.append(ch)
        elif ch == "(":
            depth += 1
            if depth >= 2:
                current.append(ch)
        elif ch == ")":
            depth -= 1
            if depth == 0:
                break
            current.append(ch)
        elif ch == "," and depth == 1:
            break
        elif depth >= 1:
            current.append(ch)
        cursor += 1
    if depth == 0:
        return None
    return "".join(current).strip()


def is_role_first(first_arg: str | None) -> bool:
    if not first_arg:
        return False
    return "EGameXXKFontRole" in first_arg or first_arg == "Role"


def iter_sources() -> list[Path]:
    return sorted(
        path
        for path in SOURCE_ROOT.rglob("*.cpp")
        if path.is_file()
    ) + sorted(path for path in SOURCE_ROOT.rglob("*.h") if path.is_file())


def load_role_rows() -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    for raw in ROLE_TABLE.read_text(encoding="utf-8").splitlines():
        line = raw.rstrip()
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = [part.strip() for part in line.split("\t")]
        if len(parts) < 3:
            raise SystemExit(f"bad role row: {raw!r}")
        rows.append((parts[2], parts[0]))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    findings: list[str] = []
    role_rows = load_role_rows()
    roles = {role for role, _path in role_rows}
    unknown = roles - {"Title", "Body", "Manual"}
    if unknown:
        findings.append(f"unknown role kinds in the table: {sorted(unknown)}")

    legacy_hits: list[str] = []
    role_call_count = 0
    for path in iter_sources():
        text = path.read_text(encoding="utf-8", errors="replace")
        role_call_count += len(ROLE_CALL.findall(text))
        for match in LEGACY_CALL.finditer(text):
            if is_role_first(first_argument(text, match.end() - 1)):
                continue
            line = text.count("\n", 0, match.start()) + 1
            snippet = text[match.start() : text.find("\n", match.start())]
            legacy_hits.append(
                f"{path.relative_to(PROJECT_ROOT).as_posix()}:{line}: {snippet.strip()[:120]}"
            )
    if legacy_hits:
        findings.append(
            f"{len(legacy_hits)} role-less FGameXXKInRunUiStyle font call(s) survive; "
            f"first: {legacy_hits[0]}"
        )

    header = STYLE_HEADER.read_text(encoding="utf-8")
    for needle, label in (
        ("enum class EGameXXKFontRole", "EGameXXKFontRole enum"),
        ("TitleFontPath", "title font path constant"),
        ("BodyFontPath", "body font path constant"),
    ):
        if needle not in header:
            findings.append(f"{STYLE_HEADER.name} is missing the {label}")
    if BODY_FONT_PATH.split(".")[0].split("/")[-1] not in header:
        findings.append("style header does not reference the imported body font asset name")
    if TITLE_FONT_PATH.split(".")[0].split("/")[-1] not in header:
        findings.append("style header does not reference the title font asset name")

    importer = IMPORTER.read_text(encoding="utf-8")
    pinned = re.search(r'SOURCE_SHA256\s*=\s*"([0-9a-f]{64})"', importer)
    source_file = re.search(r'SOURCE_FILE\s*=\s*SOURCE_DIR\s*/\s*"([^"]+)"', importer)
    if not pinned or not source_file:
        findings.append("importer does not pin a source file and SHA-256")
        source_sha = None
    else:
        face = (
            PROJECT_ROOT
            / "Saved"
            / "FontPreview"
            / "20260912-keinann-maru-pop"
            / "fonts"
            / source_file.group(1)
        )
        if not face.is_file():
            findings.append(f"body font source file is missing: {face}")
            source_sha = None
        else:
            digest = hashlib.sha256(face.read_bytes()).hexdigest()
            source_sha = digest
            if digest != pinned.group(1):
                findings.append(
                    f"body font source hash drifted: {digest} != pinned {pinned.group(1)}"
                )

    counted = {"Title": 0, "Body": 0, "Manual": 0}
    for role, _path in role_rows:
        if role in counted:
            counted[role] += 1

    report = {
        "ok": not findings,
        "roleTableRows": len(role_rows),
        "roleTableCounts": counted,
        "roleCallsInSource": role_call_count,
        "legacyCalls": legacy_hits,
        "bodyFontPath": BODY_FONT_PATH,
        "titleFontPath": TITLE_FONT_PATH,
        "bodyFontSourceSha256": source_sha,
        "findings": findings,
    }
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print(f"role table rows : {report['roleTableRows']} {counted}")
        print(f"role font calls : {role_call_count}")
        print(f"legacy calls    : {len(legacy_hits)}")
        for finding in findings:
            print(f"FAIL {finding}")
        print("PASS" if report["ok"] else "FAIL")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
