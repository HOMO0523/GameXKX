from pathlib import Path


SOURCE_ASSET_DIR = Path(r"D:\UE5 demo\zzz\我的项目\Content\Asian_Village")
TARGET_RELATIVE_DIR = Path("Content") / "Asian_Village"
EXPECTED_COUNTS = {"files": 505, "uasset": 503, "umap": 2}


class MigrationError(RuntimeError):
    pass


def resolve_target(target: Path, project_root: Path) -> Path:
    expected_label = TARGET_RELATIVE_DIR.as_posix()
    try:
        root = project_root.resolve(strict=True)
        expected = root / TARGET_RELATIVE_DIR
        content = (root / "Content").resolve(strict=False)

        if target != expected:
            raise MigrationError(f"target must be exactly {expected_label}")

        resolved_target = target.resolve(strict=False)
        content.relative_to(root)
        resolved_target.relative_to(content)
        if resolved_target != expected:
            raise MigrationError(f"target must remain inside {expected_label}")
        return resolved_target
    except MigrationError:
        raise
    except (OSError, RuntimeError, ValueError) as exc:
        raise MigrationError(
            f"unable to resolve required target {expected_label}: {exc}"
        ) from exc
