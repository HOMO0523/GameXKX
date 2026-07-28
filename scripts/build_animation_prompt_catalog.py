#!/usr/bin/env python3
"""Build the full per-unit five-action Seedance prompt catalog without submitting tasks."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = ROOT / "SourceAssets/AnimationProduction/unit_profiles.json"
OUTPUT_ROOT = ROOT / "SourceAssets/AnimationProduction/prompts_v1"
LEDGER_PATH = ROOT / "SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json"

HARD_PREFIX = (
    "\u6700\u9ad8\u4f18\u5148\u7ea7\u786c\u6027\u6784\u56fe\u9650\u5236\uff1a"
    "\u5168\u7247\u6bcf\u4e00\u5e27\u4e2d\uff0c\u4e3b\u4f53\u5168\u8eab\u3001"
    "\u6240\u6709\u80a2\u4f53\u3001\u6b66\u5668\u3001\u670d\u9970\u3001"
    "\u6bdb\u53d1\u3001\u5c3e\u7fbd\u3001\u968f\u8eab\u7269\u4ef6\u3001"
    "\u9644\u5c5e\u7269\u548c\u5141\u8bb8\u7684\u7b80\u6d01\u7279\u6548\u90fd"
    "\u5fc5\u987b\u5b8c\u6574\u4f4d\u4e8e\u753b\u5e45\u4e2d\u592e80%\u5b89"
    "\u5168\u6d3b\u52a8\u533a\u5185\uff0c\u5e76\u4e0e\u56db\u8fb9\u4fdd\u6301"
    "\u6e05\u6670\u6d0b\u7ea2\u7a7a\u9699\uff1b\u4efb\u4f55\u90e8\u5206\u4e0d"
    "\u5f97\u63a5\u89e6\u3001\u8d8a\u8fc7\u6216\u88ab\u753b\u9762\u8fb9\u7f18"
    "\u88c1\u5207\u3002\u5373\u4f7f\u56e0\u6b64\u5fc5\u987b\u7f29\u77ed\u6b66"
    "\u5668\u8f68\u8ff9\u6216\u6536\u5c0f\u52a8\u4f5c\uff0c\u4e5f\u7edd\u4e0d"
    "\u5141\u8bb8\u51fa\u753b\u3002"
)

COMMON_END = (
    "\u955c\u5934\u5b8c\u5168\u56fa\u5b9a\uff0c\u7eaf#FF00FF\u6d0b\u7ea2"
    "\u80cc\u666f\u5168\u7a0b\u4e0d\u53d8\uff0c\u65e0\u5730\u9762\u3001\u9634"
    "\u5f71\u3001\u573a\u666f\u3001\u6587\u5b57\u548c\u989d\u5916\u89d2\u8272"
    "\u3002\u7981\u6b62\u8eab\u4efd\u6f02\u79fb\u3001\u6bd4\u4f8b\u53d8\u5316"
    "\u3001\u6362\u88c5\u3001\u7ed3\u6784\u53d8\u5f62\u3001\u989d\u5916\u80a2"
    "\u4f53\u3001\u65b9\u5411\u7ffb\u8f6c\u548c\u753b\u98ce\u53d8\u5316\u3002"
)

ACTION_ORDER = ("idle", "attack", "hit", "buff", "death")
ACTION_FIELDS = {
    "idle": "idle_behavior",
    "attack": "signature_attack",
    "hit": "hit_behavior",
    "buff": "buff_behavior",
    "death": "death_behavior",
}


def _current_credit_snapshot() -> int:
    ledger = json.loads(LEDGER_PATH.read_text(encoding="utf-8"))
    submissions = ledger.get("submissions", [])
    if submissions:
        return int(submissions[-1]["credit_after_submit"])
    return int(ledger["live_credit_at_start"])


def _identity_block(unit: dict[str, object]) -> str:
    facing = "\u5de6" if unit["facing"] == "left" else "\u53f3"
    return (
        "5\u79d2\u6218\u6597\u52a8\u753b\uff0c\u9996\u5e27\u548c\u5c3e\u5e27"
        "\u5fc5\u987b\u7cbe\u786e\u56de\u5230\u8f93\u5165\u56fe\u7684\u540c\u4e00"
        "Idle\u59ff\u52bf\u3002\u4e25\u683c\u4fdd\u6301\u8f93\u5165\u56fe\u7684"
        "\u8eab\u4efd\u3001\u8138\u90e8\u3001\u4f53\u578b\u3001\u6b66\u5668\u3001"
        "\u9644\u5c5e\u7269\u3001\u6c34\u58a8\u6c34\u5f69\u7b14\u89e6\u3001\u989c"
        "\u8272\u548c\u56fe\u5f62\u5316\u8f6e\u5ed3\u4e0d\u53d8\u3002\u5934\u4e0e"
        f"\u8eab\u4f53\u59cb\u7ec8\u671d\u753b\u9762{facing}\u4fa7\u76843/4\u59ff"
        "\u52bf\uff0c\u4e0d\u8f6c\u8eab\u3001\u4e0d\u7ffb\u5411\u3002"
        f" Identity/personality: {unit['personality']}."
        f" Silhouette: {unit['silhouette']}."
        f" Preserve props exactly: {unit['signature_props']}."
    )


def _action_block(action: str, behavior: str, side: str) -> str:
    if action == "idle":
        return (
            "0.00-0.70\u79d2\u4fdd\u6301\u539fIdle\uff1b0.70-1.10\u79d2\u9010"
            "\u6e10\u8fdb\u5165\u7b26\u5408\u6027\u683c\u7684\u547c\u5438\u3001\u91cd"
            "\u5fc3\u548c\u5c0f\u5e45\u9644\u5c5e\u7269\u5ef6\u8fdf\u6446\u52a8\uff1b"
            "1.10\u79d2\u8fbe\u5230\u7b2c\u4e00\u4e2a\u6e05\u695a\u4f46\u514b\u5236"
            "\u7684\u8d77\u4f0f\u5cf0\u503c\uff1b1.10-4.30\u79d2\u4ee5\u6709\u8bbe"
            "\u8ba1\u611f\u7684\u8282\u594f\u518d\u5b8c\u6210\u4e00\u6b21\u5e45\u5ea6"
            "\u7a0d\u5927\u7684\u547c\u5438\u5faa\u73af\uff1b4.30-5.00\u79d2\u7cbe\u786e"
            "\u56de\u5230\u9996\u5e27Idle\u3002\u7981\u6b62\u8d70\u8def\u3001\u6ed1"
            "\u6b65\u548c\u79bb\u5f00\u539f\u5730\u3002"
            f" Character-specific motion: {behavior}."
        )
    if action == "attack":
        target = "\u5de6" if side == "character" else "\u53f3"
        return (
            "0.00-0.70\u79d2Idle\uff1b0.70\u79d2\u5feb\u901f\u542f\u52a8\u5e76"
            "\u84c4\u529b\uff1b1.10\u79d2\u5fc5\u987b\u5b8c\u6210\u552f\u4e00\u4e00"
            "\u6b21\u6e05\u695a\u653b\u51fb\u5cf0\u503c\uff0c\u653b\u51fb\u6307\u5411"
            f"\u753b\u9762{target}\u4fa7\u5e76\u7ecf\u8fc7\u4e2d\u592e\u547d\u4e2d\u70b9"
            "\uff1b1.10-1.35\u79d2\u5b8c\u6210\u51b2\u51fb\uff1b1.35-4.30\u79d2"
            "\u5feb\u8fdb\u6162\u51fa\u5730\u6536\u52bf\uff1b4.30-5.00\u79d2\u7cbe"
            "\u786e\u56de\u5230\u9996\u5e27Idle\u3002\u53ea\u5141\u8bb8\u5728\u547d"
            "\u4e2d\u77ac\u95f4\u51fa\u73b0\u4e00\u7b14\u8d34\u8fd1\u653b\u51fb\u7aef"
            "\u70b9\u7684\u7ec6\u77ed\u6de1\u91d1\u6c34\u58a8\u5f27\u5149\uff0c\u4e0d"
            "\u8d85\u8fc7\u753b\u5bbd20%\uff0c1.35\u79d2\u524d\u6d88\u6563\uff0c\u7981"
            "\u6b62\u957f\u5e26\u3001\u5706\u73af\u548c\u6574\u5c4f\u7279\u6548\u3002"
            f" Signature attack: {behavior}."
        )
    if action == "hit":
        incoming = "\u5de6" if side == "character" else "\u53f3"
        recoil = "\u53f3" if side == "character" else "\u5de6"
        return (
            "0.00-0.70\u79d2Idle\uff1b0.70\u79d2\u611f\u53d7\u5230\u6765\u81ea"
            f"\u753b\u9762{incoming}\u4fa7\u7684\u653b\u51fb\u5e76\u5feb\u901f\u7ef7"
            "\u7d27\uff1b1.10\u79d2\u5fc5\u987b\u8fbe\u5230\u552f\u4e00\u4e00\u6b21"
            f"\u6e05\u695a\u53d7\u51fb\u5cf0\u503c\uff0c\u8eab\u4f53\u5411{recoil}\u4fa7"
            "\u77ed\u4fc3\u538b\u7f29\u540e\u4ef0\u4f46\u811a\u4e0b\u951a\u70b9\u7a33"
            "\u5b9a\uff1b1.10-1.35\u79d2\u5b8c\u6210\u51b2\u51fb\uff1b1.35-4.30"
            "\u79d2\u5feb\u8fdb\u6162\u51fa\u5730\u6062\u590d\uff1b4.30-5.00\u79d2"
            "\u7cbe\u786e\u56de\u5230\u9996\u5e27Idle\u3002\u65e0\u653b\u51fb\u8005"
            "\u3001\u65e0\u7206\u70b9\u548c\u65e0\u5916\u90e8\u7279\u6548\u3002"
            f" Character-specific reaction: {behavior}."
        )
    if action == "buff":
        return (
            "0.00-0.70\u79d2Idle\uff1b0.70\u79d2\u5f00\u59cb\u505a\u7b26\u5408"
            "\u6027\u683c\u7684\u589e\u76ca\u63a5\u53d7\u52a8\u4f5c\uff1b1.10\u79d2"
            "\u8fbe\u5230\u81ea\u4fe1\u3001\u632f\u594b\u6216\u84c4\u52bf\u7684\u59ff"
            "\u6001\u5cf0\u503c\uff1b1.10-3.80\u79d2\u4fdd\u6301\u5e76\u7f13\u6162"
            "\u6536\u52bf\uff1b3.80-5.00\u79d2\u7cbe\u786e\u56de\u5230\u9996\u5e27"
            "Idle\u3002\u89d2\u8272\u52a8\u753b\u5185\u4e0d\u751f\u6210\u5149\u73af"
            "\u3001\u661f\u661f\u3001\u6587\u5b57\u6216\u72b6\u6001\u56fe\u6807\uff0c"
            "\u901a\u7528Buff/Debuff\u7279\u6548\u7531\u5f15\u64ce\u53e6\u884c\u53e0\u52a0"
            "\u3002"
            f" Character-specific buff motion: {behavior}."
        )
    if action == "death":
        return (
            "0.00-0.50\u79d2Idle\uff1b0.50\u79d2\u5f00\u59cb\u5931\u53bb\u529b"
            "\u91cf\uff1b1.10\u79d2\u51fa\u73b0\u6e05\u695a\u7684\u5931\u8861\u5cf0"
            "\u503c\uff1b1.10-2.20\u79d2\u6162\u901f\u8fdb\u5165\u7b26\u5408\u6027"
            "\u683c\u7684\u6b7b\u4ea1\u59ff\u6001\uff1b2.20-4.20\u79d2\u4fdd\u6301"
            "\u6b7b\u4ea1\u59ff\u6001\u4e14\u53ea\u6709\u6781\u5f31\u6b8b\u4f59\u6446"
            "\u52a8\uff1b4.20-5.00\u79d2\u4e3a\u4e86\u9996\u5c3e\u5e27\u7ea6\u675f"
            "\u5e73\u6ed1\u56de\u5230\u9996\u5e27Idle\u3002\u5f15\u64ce\u5b9e\u9645"
            "\u64ad\u653e\u65f6\u53ea\u4f7f\u75280.00-4.20\u79d2\u5e76\u505c\u5728"
            "\u6b7b\u4ea1\u59ff\u6001\uff0c\u4e0d\u64ad\u653e\u6062\u590d\u6bb5\u3002"
            f" Character-specific death: {behavior}."
        )
    raise ValueError(action)


def build() -> dict[str, object]:
    profile_data = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    sequence = 0
    for unit in profile_data["units"]:
        unit_dir = OUTPUT_ROOT / unit["id"]
        unit_dir.mkdir(parents=True, exist_ok=True)
        for action in ACTION_ORDER:
            sequence += 1
            prompt = (
                HARD_PREFIX
                + _identity_block(unit)
                + _action_block(action, unit[ACTION_FIELDS[action]], unit["side"])
                + COMMON_END
                + "\n"
            )
            prompt_path = unit_dir / f"{action}.txt"
            prompt_path.write_text(prompt, encoding="utf-8", newline="\n")
            model = "seedance2.0_vip" if action == "attack" else "seedance1.5pro"
            credits = 70 if action == "attack" else 40
            entries.append(
                {
                    "sequence": sequence,
                    "asset_id": f"{unit['id']}_{action}",
                    "unit_id": unit["id"],
                    "side": unit["side"],
                    "facing": unit["facing"],
                    "action": action,
                    "source_1600": unit["source_1600"],
                    "first_frame": unit["source_1600"],
                    "last_frame": unit["source_1600"],
                    "prompt_file": prompt_path.relative_to(ROOT).as_posix(),
                    "model": model,
                    "resolution": "720p",
                    "duration_seconds": 5,
                    "expected_credits": credits,
                    "max_submissions": 1,
                    "automatic_retry": False,
                    "status": "not_submitted",
                }
            )
    unit_action_total = sum(int(entry["expected_credits"]) for entry in entries)
    available = _current_credit_snapshot()
    manifest: dict[str, object] = {
        "schema_version": 1,
        "generated_from": PROFILE_PATH.relative_to(ROOT).as_posix(),
        "entry_count": len(entries),
        "unit_action_credit_total": unit_action_total,
        "generic_effect_credit_total": 120,
        "five_action_plus_generic_total": unit_action_total + 120,
        "budget_preflight": {
            "available_credit_snapshot": available,
            "baseline_credit": 7145,
            "retry_cap": 565,
            "can_submit_all_five_actions": available >= unit_action_total + 120,
            "shortfall_for_five_actions_plus_generic": max(
                0, unit_action_total + 120 - available
            ),
            "four_unit_actions_plus_generic_total": 6580,
            "can_submit_four_unit_actions_plus_generic": available >= 6580,
        },
        "entries": entries,
    }
    (OUTPUT_ROOT / "generation_manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return manifest


if __name__ == "__main__":
    result = build()
    print(
        json.dumps(
            {
                "entry_count": result["entry_count"],
                "unit_action_credit_total": result["unit_action_credit_total"],
                "budget_preflight": result["budget_preflight"],
            },
            ensure_ascii=False,
            indent=2,
        )
    )
