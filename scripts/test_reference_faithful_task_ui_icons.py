import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "docs" / "ui" / "tasks" / "reference_crops"
SOURCE = ROOT / "docs" / "ui" / "tasks" / "source_art"
NAMES = ("reward_coin.png", "reward_exp.png", "reward_token.png")


def info(path: Path):
    with Image.open(path) as image:
        return image.size, image.mode


def main():
    parser = argparse.ArgumentParser(
        description="Validate immutable task reward icon references."
    )
    parser.add_argument(
        "--anchors-only",
        action="store_true",
        help="validate only immutable reference crops, not production icons",
    )
    args = parser.parse_args()
    failures = []
    for name in NAMES:
        reference, production = REFERENCE / name, SOURCE / name
        if not reference.is_file():
            failures.append(f"missing reference crop: {reference}")
            continue
        reference_size, reference_mode = info(reference)
        if reference_mode not in {"RGB", "RGBA"}:
            failures.append(f"unsupported image mode: {name}")
        if args.anchors_only:
            continue
        if not production.is_file():
            failures.append(f"missing production icon: {production}")
            continue
        production_size, production_mode = info(production)
        if abs(reference_size[0] / reference_size[1] - production_size[0] / production_size[1]) > 0.02:
            failures.append(f"aspect drift: {name}")
        if production_size[0] < reference_size[0] or production_size[1] < reference_size[1]:
            failures.append(f"production icon shrank: {name}")
        if production_mode not in {"RGB", "RGBA"}:
            failures.append(f"unsupported image mode: {name}")
    print("reference_faithful_task_icons: " + ("PASS" if not failures else "FAIL"))
    print("validation_mode: " + ("anchors-only" if args.anchors_only else "strict"))
    print("\n".join(failures))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
