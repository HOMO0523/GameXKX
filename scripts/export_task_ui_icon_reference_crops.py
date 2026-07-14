import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REVISION = "33b1d3ae2e042731ed4ac3c9b8989b2a76c32e60"
DESTINATION = ROOT / "docs" / "ui" / "tasks" / "reference_crops"
NAMES = ("reward_coin.png", "reward_exp.png", "reward_token.png")


def main():
    DESTINATION.mkdir(parents=True, exist_ok=True)
    exports = []
    for name in NAMES:
        result = subprocess.run(
            ["git", "show", f"{REVISION}:docs/ui/tasks/source_art/{name}"],
            cwd=ROOT,
            check=True,
            capture_output=True,
        )
        destination = DESTINATION / name
        destination.write_bytes(result.stdout)
        exports.append(destination.relative_to(ROOT).as_posix())
    print(
        json.dumps(
            {
                "reference_revision": REVISION,
                "exported_count": len(exports),
                "exports": exports,
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
