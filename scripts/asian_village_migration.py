import argparse
import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
import unicodedata
from pathlib import Path, PurePosixPath


SOURCE_ASSET_DIR = Path(r"D:\UE5 demo\zzz\我的项目\Content\Asian_Village")
TARGET_RELATIVE_DIR = Path("Content") / "Asian_Village"
EXPECTED_COUNTS = {"files": 505, "uasset": 503, "umap": 2}


class MigrationError(RuntimeError):
    pass


_IDENTITY_FIELDS = ("st_dev", "st_ino", "st_size", "st_mtime_ns", "st_ctime_ns")
_DEVICE_NAMES = {"CON", "PRN", "AUX", "NUL"} | {
    f"{prefix}{number}" for prefix in ("COM", "LPT") for number in range(1, 10)
}


def _identity(value: os.stat_result) -> tuple[int, ...]:
    return tuple(getattr(value, field) for field in _IDENTITY_FIELDS)


def _path_matches_handle(
    handle_value: os.stat_result, path_value: os.stat_result
) -> bool:
    handle_identity = list(_identity(handle_value))
    path_identity = _identity(path_value)
    handle_birth = getattr(handle_value, "st_birthtime_ns", None)
    path_birth = getattr(path_value, "st_birthtime_ns", None)
    if (
        os.name == "nt"
        and handle_birth is not None
        and handle_birth == path_birth == path_identity[4]
    ):
        handle_identity[4] = handle_birth
    return tuple(handle_identity) == path_identity


def _error(action: str, path: Path, exc: BaseException) -> MigrationError:
    return MigrationError(f"{action} {path}: {exc}")


def _is_reparse(value: os.stat_result) -> bool:
    return bool(
        getattr(value, "st_file_attributes", 0)
        & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    )


def _lstat_safe(path: Path) -> os.stat_result:
    try:
        value = path.lstat()
    except (OSError, ValueError) as exc:
        raise _error("unable to inspect", path, exc) from exc
    if stat.S_ISLNK(value.st_mode) or _is_reparse(value):
        raise MigrationError(f"symbolic link or reparse point is forbidden: {path}")
    return value


def _check_path_components(path: Path) -> None:
    """Reject links and reparse points in every existing lexical component."""
    absolute = Path(path).absolute()
    current = Path(absolute.anchor)
    _lstat_safe(current)
    for part in absolute.parts[1:]:
        current = current / part
        _lstat_safe(current)


def sha256_file(path: Path) -> str:
    """Hash one stable regular file through a single open handle."""
    path = Path(path)
    _lstat_safe(path)
    try:
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            before = os.fstat(handle.fileno())
            if not stat.S_ISREG(before.st_mode):
                raise MigrationError(f"not a regular file: {path}")
            while True:
                block = handle.read(1024 * 1024)
                if not block:
                    break
                digest.update(block)
            after = os.fstat(handle.fileno())
            path_after = path.stat()
        if _identity(before) != _identity(after):
            raise MigrationError(f"file changed while hashing: {path}")
        if not _path_matches_handle(after, path_after):
            raise MigrationError(f"file was replaced while hashing: {path}")
        return digest.hexdigest()
    except MigrationError:
        raise
    except (OSError, ValueError) as exc:
        raise _error("unable to hash", path, exc) from exc


def build_manifest(source: Path) -> dict:
    source = Path(source)
    _check_path_components(source)
    try:
        root = source.resolve(strict=True)
    except (OSError, RuntimeError, ValueError) as exc:
        raise _error("unable to resolve source", source, exc) from exc
    if not root.is_dir():
        raise MigrationError(f"source is not a directory: {source}")

    discovered: list[tuple[str, Path]] = []

    def walk_error(exc: OSError) -> None:
        failed_path = Path(exc.filename) if exc.filename else root
        raise _error("unable to enumerate source", failed_path, exc) from exc

    try:
        for current, directories, filenames in os.walk(
            root, followlinks=False, onerror=walk_error
        ):
            current_path = Path(current)
            _lstat_safe(current_path)
            for name in directories:
                _lstat_safe(current_path / name)
            for name in filenames:
                path = current_path / name
                value = _lstat_safe(path)
                if not stat.S_ISREG(value.st_mode):
                    continue
                relative = path.relative_to(root).as_posix()
                discovered.append((relative, path))
    except MigrationError:
        raise
    except (OSError, ValueError) as exc:
        raise _error("unable to enumerate source", root, exc) from exc

    files = []
    for relative, path in sorted(discovered, key=lambda item: item[0]):
        digest = sha256_file(path)
        try:
            size = path.stat().st_size
        except (OSError, ValueError) as exc:
            raise _error("unable to inspect hashed file", path, exc) from exc
        files.append({"path": relative, "bytes": size, "sha256": digest})
    counts = {
        "files": len(files),
        "uasset": sum(item["path"].lower().endswith(".uasset") for item in files),
        "umap": sum(item["path"].lower().endswith(".umap") for item in files),
    }
    return {"schema_version": 1, "counts": counts, "files": files}


def canonical_json_bytes(value: object) -> bytes:
    try:
        text = json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2)
        return (text + "\n").encode("utf-8")
    except (TypeError, ValueError, UnicodeError) as exc:
        raise MigrationError(f"unable to encode canonical JSON: {exc}") from exc


def write_manifest(path: Path, manifest: dict, *, replace: bool = False) -> None:
    path = Path(path)
    temporary = path.with_name(path.name + ".tmp")
    if path.exists() and not replace:
        raise MigrationError(f"manifest already exists: {path}")
    if temporary.exists():
        raise MigrationError(f"temporary manifest already exists: {temporary}")
    data = canonical_json_bytes(manifest)
    try:
        with temporary.open("xb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        temporary.replace(path)
    except (OSError, ValueError) as exc:
        try:
            temporary.unlink(missing_ok=True)
        except OSError:
            pass
        raise _error("unable to write manifest", path, exc) from exc


def _plain_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _validate_relative_path(value: object) -> str:
    if not isinstance(value, str) or not value:
        raise MigrationError("manifest file path must be a non-empty string")
    if "\\" in value or "\0" in value or ":" in value:
        raise MigrationError(f"unsafe manifest path: {value!r}")
    if any(unicodedata.category(character) == "Cc" for character in value):
        raise MigrationError(f"unsafe manifest path: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or any(part in ("", ".", "..") for part in pure.parts):
        raise MigrationError(f"unsafe manifest path: {value!r}")
    for part in pure.parts:
        if part.endswith((".", " ")):
            raise MigrationError(f"unsafe manifest path: {value!r}")
        stem = part.split(".", 1)[0].upper()
        if stem in _DEVICE_NAMES:
            raise MigrationError(f"unsafe manifest path: {value!r}")
    return value


def _validate_manifest(manifest: object) -> dict:
    if not isinstance(manifest, dict) or set(manifest) != {
        "schema_version", "counts", "files"
    }:
        raise MigrationError("manifest fields are invalid")
    if not _plain_int(manifest["schema_version"]) or manifest["schema_version"] != 1:
        raise MigrationError("manifest schema_version must be integer 1")
    counts = manifest["counts"]
    if not isinstance(counts, dict) or set(counts) != {"files", "uasset", "umap"}:
        raise MigrationError("manifest counts fields are invalid")
    if any(not _plain_int(value) or value < 0 for value in counts.values()):
        raise MigrationError("manifest counts must be non-negative integers")
    files = manifest["files"]
    if not isinstance(files, list):
        raise MigrationError("manifest files must be a list")
    paths = []
    for item in files:
        if not isinstance(item, dict) or set(item) != {"path", "bytes", "sha256"}:
            raise MigrationError("manifest file fields are invalid")
        path = _validate_relative_path(item["path"])
        if not _plain_int(item["bytes"]) or item["bytes"] < 0:
            raise MigrationError(f"invalid byte count for {path}")
        digest = item["sha256"]
        if not isinstance(digest, str) or len(digest) != 64 or any(
            character not in "0123456789abcdef" for character in digest
        ):
            raise MigrationError(f"invalid SHA256 for {path}")
        paths.append(path)
    if paths != sorted(paths) or len(paths) != len(set(paths)):
        raise MigrationError("manifest paths must be unique and sorted")
    calculated = {
        "files": len(files),
        "uasset": sum(path.lower().endswith(".uasset") for path in paths),
        "umap": sum(path.lower().endswith(".umap") for path in paths),
    }
    if counts != calculated:
        raise MigrationError("manifest counts do not match file records")
    return manifest


def verify_manifest(root: Path, manifest: dict) -> dict:
    expected = _validate_manifest(manifest)
    actual = build_manifest(root)
    if actual != expected:
        expected_by_path = {item["path"]: item for item in expected["files"]}
        actual_by_path = {item["path"]: item for item in actual["files"]}
        difference = next(
            (path for path in sorted(set(expected_by_path) | set(actual_by_path))
             if expected_by_path.get(path) != actual_by_path.get(path)),
            "counts",
        )
        raise MigrationError(f"manifest verification failed for {difference}")
    return {"status": "verified", "counts": dict(actual["counts"])}


def _project_and_staging(target: Path, staging: Path) -> tuple[Path, Path, Path]:
    target = Path(target)
    staging = Path(staging)
    try:
        project_root = target.parents[1]
    except IndexError as exc:
        raise MigrationError(f"invalid target path: {target}") from exc
    canonical_target = resolve_target(target, project_root)
    try:
        root = project_root.resolve(strict=True)
        canonical_staging = (
            root / "Saved" / "MigrationStaging" / "Asian_Village"
        )
        if staging != canonical_staging or staging.resolve(strict=False) != canonical_staging:
            raise MigrationError(
                f"staging must be exactly {canonical_staging}"
            )
        canonical_staging.relative_to(root / "Saved")
    except MigrationError:
        raise
    except (OSError, RuntimeError, ValueError) as exc:
        raise _error("unable to validate staging", staging, exc) from exc
    return root, canonical_target, canonical_staging


def _remove_safe_staging(staging: Path, project_root: Path) -> None:
    """Remove only the exact migration staging directory after lexical checks."""
    expected = (
        project_root.resolve(strict=True)
        / "Saved"
        / "MigrationStaging"
        / "Asian_Village"
    )
    absolute = staging.absolute()
    resolved = staging.resolve(strict=False)
    if absolute != expected or resolved != expected:
        raise MigrationError(f"refusing to clean unsafe staging path: {staging}")
    if staging.exists():
        shutil.rmtree(staging)


def stage_and_promote(
    source: Path,
    target: Path,
    staging: Path,
    manifest: dict,
    *,
    copier=shutil.copy2,
) -> dict:
    source = Path(source)
    root, target, staging = _project_and_staging(target, staging)
    _validate_manifest(manifest)
    if target.exists():
        raise MigrationError(f"target already exists: {target}")
    if staging.exists():
        raise MigrationError(f"staging already exists: {staging}")
    verify_manifest(source, manifest)

    created_staging = False
    promoted = False
    try:
        staging.parent.mkdir(parents=True, exist_ok=True)
        created_staging = True
        shutil.copytree(source, staging, copy_function=copier)
        verify_manifest(source, manifest)
        verify_manifest(staging, manifest)
        staging.replace(target)
        promoted = True
        verify_manifest(target, manifest)
        return {
            "status": "promoted",
            "target": str(target),
            "counts": dict(manifest["counts"]),
        }
    except Exception as exc:
        if promoted:
            if isinstance(exc, MigrationError):
                detail = str(exc)
            else:
                detail = f"{type(exc).__name__}: {exc}"
            raise MigrationError(
                f"terminal failure after promotion; preserve target for isolation: {detail}"
            ) from exc
        cleanup_error = None
        if created_staging:
            try:
                _remove_safe_staging(staging, root)
            except (MigrationError, OSError, ValueError) as cleanup_exc:
                cleanup_error = cleanup_exc
        detail = str(exc)
        if cleanup_error is not None:
            detail += f"; safe staging cleanup failed: {cleanup_error}"
        raise MigrationError(f"migration copy failure: {detail}") from exc


def _unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise MigrationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _load_manifest(path: Path) -> dict:
    path = Path(path)
    try:
        with path.open("r", encoding="utf-8", newline="") as handle:
            value = json.load(handle, object_pairs_hook=_unique_object)
    except MigrationError:
        raise
    except (OSError, ValueError, UnicodeError, json.JSONDecodeError) as exc:
        raise _error("unable to read manifest", path, exc) from exc
    return _validate_manifest(value)


def unreal_editor_running() -> bool:
    try:
        if os.name == "nt":
            result = subprocess.run(
                [
                    "tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe",
                    "/FO", "CSV", "/NH",
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode != 0:
                raise MigrationError(
                    f"unable to inspect processes: {result.stderr.strip()}"
                )
            return "unrealeditor.exe" in result.stdout.lower()
        result = subprocess.run(
            ["ps", "-A", "-o", "comm="],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            raise MigrationError(
                f"unable to inspect processes: {result.stderr.strip()}"
            )
        return any(
            Path(line.strip()).name.lower() == "unrealeditor.exe"
            for line in result.stdout.splitlines()
        )
    except MigrationError:
        raise
    except (OSError, ValueError) as exc:
        raise MigrationError(f"unable to inspect processes: {exc}") from exc


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Deterministic Asian Village asset migration"
    )
    commands = parser.add_subparsers(dest="command", required=True)

    inventory = commands.add_parser("inventory")
    inventory.add_argument("--source", required=True, type=Path)
    inventory.add_argument("--output", required=True, type=Path)

    copy = commands.add_parser("copy")
    copy.add_argument("--source", required=True, type=Path)
    copy.add_argument("--project-root", required=True, type=Path)
    copy.add_argument("--manifest", required=True, type=Path)

    verify = commands.add_parser("verify")
    verify.add_argument("--root", required=True, type=Path)
    verify.add_argument("--manifest", required=True, type=Path)
    verify.add_argument("--output", type=Path)
    return parser


def _emit(value: object) -> None:
    data = canonical_json_bytes(value)
    stream = sys.stdout
    if hasattr(stream, "buffer"):
        stream.buffer.write(data)
        stream.buffer.flush()
    else:
        stream.write(data.decode("utf-8"))
        stream.flush()


def main(argv: list[str] | None = None, *, process_checker=unreal_editor_running) -> int:
    try:
        arguments = _parser().parse_args(argv)
        if arguments.command == "inventory":
            manifest = build_manifest(arguments.source)
            try:
                real_source = arguments.source.resolve(strict=True)
                canonical_source = SOURCE_ASSET_DIR.resolve(strict=False)
            except (OSError, RuntimeError, ValueError) as exc:
                raise _error("unable to resolve inventory source", arguments.source, exc)
            if real_source == canonical_source and manifest["counts"] != EXPECTED_COUNTS:
                raise MigrationError(
                    f"real source counts {manifest['counts']} do not match "
                    f"expected {EXPECTED_COUNTS}"
                )
            write_manifest(arguments.output, manifest)
            result = manifest
        elif arguments.command == "copy":
            if process_checker():
                raise MigrationError(
                    "UnrealEditor.exe is running; save and close the editor first"
                )
            manifest = _load_manifest(arguments.manifest)
            try:
                project_root = arguments.project_root.resolve(strict=True)
            except (OSError, RuntimeError, ValueError) as exc:
                raise _error("unable to resolve project root", arguments.project_root, exc)
            target = project_root / TARGET_RELATIVE_DIR
            staging = (
                project_root / "Saved" / "MigrationStaging" / "Asian_Village"
            )
            result = stage_and_promote(
                arguments.source, target, staging, manifest
            )
        else:
            manifest = _load_manifest(arguments.manifest)
            result = verify_manifest(arguments.root, manifest)
            if arguments.output is not None:
                write_manifest(arguments.output, result)
        _emit(result)
        return 0
    except MigrationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except (OSError, RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


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


if __name__ == "__main__":
    raise SystemExit(main())
