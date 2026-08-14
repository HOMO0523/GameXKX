#!/usr/bin/env python3
"""One-shot patch: insert Phase-2 anchors with verified byte-exact strings."""

from __future__ import annotations

import sys
from pathlib import Path

ADAPTER = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\GameXXKCardBattleAdapter.cpp")
RULES_H = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Public\GameXXKMVPRules.h")

PATCHES = [
    (
        ADAPTER,
        (
            "\tif (!BuildCardCombatUnits(NewState, Units, OutError)\n"
            "\t\t|| !BuildStartingCardInstances(NewState, NewState.ActiveBattleNodeId, Instances, OutError))\n"
            "\t{\n"
            "\t\treturn false;\n"
            "\t}\n"
        ),
        (
            "\tif (!BuildCardCombatUnits(NewState, Units, OutError)\n"
            "\t\t|| !BuildStartingCardInstances(NewState, NewState.ActiveBattleNodeId, Instances, OutError))\n"
            "\t{\n"
            "\t\treturn false;\n"
            "\t}\n"
            "\t// Permanent deck-card quality upgrades (tiered battle rewards) override base quality.\n"
            "\tfor (FGameXXKCardInstance& Instance : Instances)\n"
            "\t{\n"
            "\t\tif (const EGameXXKCardQuality* Upgraded = Run.UpgradedCardQualities.Find(Instance.CardId))\n"
            "\t\t{\n"
            "\t\t\tInstance.CurrentQuality = *Upgraded;\n"
            "\t\t}\n"
            "\t}\n"
        ),
    ),
    (
        ADAPTER,
        "\tNewRuntime.EquippedHeroCardIds = Run.HeroSelectedCardIds;\n",
        (
            "\tNewRuntime.EquippedHeroCardIds = Run.HeroSelectedCardIds;\n"
            "\tNewRuntime.BonusSharedEnergyCap = Run.BonusSharedEnergyCap;\n"
            "\tNewRuntime.BonusRoundDrawCount = Run.BonusRoundDrawCount;\n"
            "\tif (NewRuntime.BonusRoundDrawCount > 0)\n"
            "\t{\n"
            "\t\tFString DrawError;\n"
            "\t\tif (!GameXXKCardRules::DrawCards(NewRuntime.Deck, NewRuntime.BonusRoundDrawCount, 0, &DrawError))\n"
            "\t\t{\n"
            "\t\t\treturn SetFailure(OutError, FString::Printf(TEXT(\"Opening-hand bonus draw failed: %s\"), *DrawError));\n"
            "\t\t}\n"
            "\t}\n"
        ),
    ),
    (
        RULES_H,
        (
            "\tstatic bool SkipPendingRouteRewardAndFinish(\n"
            "\t\tFGameXXKRuntimeState& State,\n"
            "\t\tFString* OutError = nullptr);\n"
        ),
        (
            "\tstatic bool SkipPendingRouteRewardAndFinish(\n"
            "\t\tFGameXXKRuntimeState& State,\n"
            "\t\tFString* OutError = nullptr);\n"
            "\n"
            "\t/** Atomically commits one tiered battle reward option and the gated battle victory settlement. */\n"
            "\tstatic bool ResolvePendingBattleRewardChoiceAndFinish(\n"
            "\t\tFGameXXKRuntimeState& State,\n"
            "\t\tint32 OptionIndex,\n"
            "\t\tFName ReplacementEntryId = NAME_None,\n"
            "\t\tFString* OutError = nullptr);\n"
        ),
    ),
]


def main() -> int:
    for path, old, new in PATCHES:
        raw = path.read_bytes()
        crlf = b"\r\n" in raw
        eol = "\r\n" if crlf else "\n"
        old_b = old.replace("\n", eol).encode("utf-8")
        new_b = new.replace("\n", eol).encode("utf-8")
        count = raw.count(old_b)
        if count != 1:
            print(f"ABORT {path.name}: expected 1 occurrence, found {count}")
            return 1
        path.write_bytes(raw.replace(old_b, new_b, 1))
        print(f"patched {path.name} (crlf={crlf})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
