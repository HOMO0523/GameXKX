import argparse
import ctypes
import hashlib
import json
import os
import shutil
import stat
import subprocess
import sys
import unicodedata
import secrets
from ctypes import wintypes
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


def _snapshot_file(path: Path) -> tuple[str, int, tuple[int, ...]]:
    """Hash and identify one file from a single stable open handle."""
    path = Path(path)
    _check_path_components(path.parent)
    initial_path = _lstat_safe(path)
    try:
        digest = hashlib.sha256()
        with path.open("rb") as handle:
            before = os.fstat(handle.fileno())
            if not stat.S_ISREG(before.st_mode):
                raise MigrationError(f"not a regular file: {path}")
            _check_path_components(path.parent)
            opened_path = _lstat_safe(path)
            if not _path_matches_handle(before, initial_path) or not _path_matches_handle(
                before, opened_path
            ):
                raise MigrationError(f"file was replaced while opening: {path}")
            while True:
                block = handle.read(1024 * 1024)
                if not block:
                    break
                digest.update(block)
            after = os.fstat(handle.fileno())
            before_close_path = _lstat_safe(path)
            if _identity(before) != _identity(after):
                raise MigrationError(f"file changed while hashing: {path}")
            if not _path_matches_handle(after, before_close_path):
                raise MigrationError(f"file was replaced while hashing: {path}")
        _check_path_components(path.parent)
        closed_path = _lstat_safe(path)
        if not _path_matches_handle(after, closed_path):
            raise MigrationError(f"file was replaced after hashing: {path}")
        return digest.hexdigest(), after.st_size, _identity(after)
    except MigrationError:
        raise
    except (OSError, ValueError) as exc:
        raise _error("unable to hash", path, exc) from exc


def sha256_file(path: Path) -> str:
    return _snapshot_file(path)[0]


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
        _validate_relative_path(relative)
        digest, size, _ = _snapshot_file(path)
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
    data = canonical_json_bytes(manifest)
    created_temporary = False
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        if temporary.exists():
            raise MigrationError(f"temporary manifest already exists: {temporary}")
        with temporary.open("xb") as handle:
            created_temporary = True
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        if replace:
            os.replace(temporary, path)
        else:
            _atomic_no_replace(temporary, path)
    except MigrationError:
        if created_temporary:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass
        raise
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
    segments = value.split("/")
    if (
        "\\" in value
        or "\0" in value
        or ":" in value
        or value.endswith("/")
        or any(segment in ("", ".", "..") for segment in segments)
    ):
        raise MigrationError(f"unsafe manifest path: {value!r}")
    if any(unicodedata.category(character) in {"Cc", "Cf"} for character in value):
        raise MigrationError(f"unsafe manifest path: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or pure.as_posix() != value:
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


def _directory_identity(path: Path) -> tuple[int, int]:
    _check_path_components(path)
    value = _lstat_safe(path)
    if not stat.S_ISDIR(value.st_mode):
        raise MigrationError(f"not a directory: {path}")
    return value.st_dev, value.st_ino


def _assert_directory_identity(path: Path, expected: tuple[int, int]) -> None:
    if _directory_identity(path) != expected:
        raise MigrationError(f"directory identity changed: {path}")


def _lexists(path: Path) -> bool:
    try:
        return os.path.lexists(path)
    except (OSError, ValueError) as exc:
        raise _error("unable to inspect path", path, exc) from exc


def _validate_staging_owner(
    staging: Path,
    ownership: dict,
    parent_identity: tuple[int, int],
) -> None:
    _assert_directory_identity(staging.parent, parent_identity)
    if _directory_identity(staging) != ownership["identity"]:
        raise MigrationError(f"staging ownership identity changed: {staging}")
    marker = ownership["marker"]
    value = _lstat_safe(marker)
    if not stat.S_ISREG(value.st_mode):
        raise MigrationError(f"invalid staging owner marker: {marker}")
    if _identity(value) != ownership["marker_identity"]:
        raise MigrationError(f"staging owner marker identity changed: {marker}")
    try:
        nonce = marker.read_text(encoding="ascii")
    except (OSError, ValueError, UnicodeError) as exc:
        raise _error("unable to read staging owner marker", marker, exc) from exc
    if nonce != ownership["nonce"]:
        raise MigrationError(f"staging owner nonce changed: {marker}")


def _remove_owned_staging(
    staging: Path,
    project_root: Path,
    ownership: dict,
    parent_identity: tuple[int, int],
    hooks: dict | None = None,
) -> None:
    expected = (
        project_root.resolve(strict=True)
        / "Saved"
        / "MigrationStaging"
        / "Asian_Village"
    )
    if staging.absolute() != expected or staging.resolve(strict=False) != expected:
        raise MigrationError(f"refusing to clean unsafe staging path: {staging}")
    if os.name == "nt":
        _remove_owned_staging_windows(
            staging, ownership, parent_identity, hooks
        )
        return
    _validate_staging_owner(staging, ownership, parent_identity)
    quarantine = staging.with_name(
        f".{staging.name}.quarantine-{secrets.token_hex(16)}"
    )
    _promote_no_replace(staging, quarantine)
    _validate_staging_owner(quarantine, ownership, parent_identity)
    shutil.rmtree(quarantine)
    ownership["marker"].unlink()


def _run_hook(hooks: dict | None, name: str) -> None:
    if hooks is not None and name in hooks:
        hooks[name]()


def _win_open_directory(
    path: Path, *, delete: bool = False, share_delete: bool = True
):
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create_file = kernel32.CreateFileW
    create_file.argtypes = (
        wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
        wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE,
    )
    create_file.restype = wintypes.HANDLE
    access = 0x80 | 0x20
    if delete:
        access |= 0x10000
    share_mode = 0x1 | 0x2
    if share_delete:
        share_mode |= 0x4
    handle = create_file(
        str(Path(path).absolute()),
        access,
        share_mode,
        None,
        3,
        0x02000000 | 0x00200000,
        None,
    )
    if handle == wintypes.HANDLE(-1).value:
        raise _error("unable to open stable directory handle", path, ctypes.WinError(ctypes.get_last_error()))
    return handle


def _win_close_handle(handle) -> None:
    if handle is not None:
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        if not kernel32.CloseHandle(handle):
            raise ctypes.WinError(ctypes.get_last_error())


class _FILE_ATTRIBUTE_TAG_INFO(ctypes.Structure):
    _fields_ = [
        ("FileAttributes", wintypes.DWORD),
        ("ReparseTag", wintypes.DWORD),
    ]


class _BY_HANDLE_FILE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("FileAttributes", wintypes.DWORD),
        ("CreationTime", wintypes.FILETIME),
        ("LastAccessTime", wintypes.FILETIME),
        ("LastWriteTime", wintypes.FILETIME),
        ("VolumeSerialNumber", wintypes.DWORD),
        ("FileSizeHigh", wintypes.DWORD),
        ("FileSizeLow", wintypes.DWORD),
        ("NumberOfLinks", wintypes.DWORD),
        ("FileIndexHigh", wintypes.DWORD),
        ("FileIndexLow", wintypes.DWORD),
    ]


def _win_handle_final_path(handle) -> Path:
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_name = kernel32.GetFinalPathNameByHandleW
    get_name.argtypes = (
        wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD
    )
    get_name.restype = wintypes.DWORD
    size = get_name(handle, None, 0, 0)
    if not size:
        raise ctypes.WinError(ctypes.get_last_error())
    buffer = ctypes.create_unicode_buffer(size + 1)
    if not get_name(handle, buffer, len(buffer), 0):
        raise ctypes.WinError(ctypes.get_last_error())
    value = buffer.value
    if value.startswith("\\\\?\\UNC\\"):
        value = "\\\\" + value[8:]
    elif value.startswith("\\\\?\\"):
        value = value[4:]
    return Path(value)


def _win_validate_directory_handle(
    handle,
    expected_path: Path,
    *,
    expected_ino: int | None = None,
) -> tuple[int, int]:
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    get_ex = kernel32.GetFileInformationByHandleEx
    get_ex.argtypes = (
        wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD
    )
    get_ex.restype = wintypes.BOOL
    tag = _FILE_ATTRIBUTE_TAG_INFO()
    if not get_ex(handle, 9, ctypes.byref(tag), ctypes.sizeof(tag)):
        raise ctypes.WinError(ctypes.get_last_error())
    if not tag.FileAttributes & 0x10 or tag.FileAttributes & 0x400:
        raise MigrationError(f"directory handle is not a plain directory: {expected_path}")
    information = _BY_HANDLE_FILE_INFORMATION()
    get_info = kernel32.GetFileInformationByHandle
    get_info.argtypes = (wintypes.HANDLE, ctypes.c_void_p)
    get_info.restype = wintypes.BOOL
    if not get_info(handle, ctypes.byref(information)):
        raise ctypes.WinError(ctypes.get_last_error())
    identity = (
        information.VolumeSerialNumber,
        (information.FileIndexHigh << 32) | information.FileIndexLow,
    )
    if expected_ino is not None and identity[1] != expected_ino:
        raise MigrationError(f"directory handle identity changed: {expected_path}")
    final_path = _win_handle_final_path(handle)
    if os.path.normcase(os.path.abspath(final_path)) != os.path.normcase(
        os.path.abspath(expected_path)
    ):
        raise MigrationError(
            f"directory handle path changed: expected {expected_path}, got {final_path}"
        )
    return identity


def _win_rename_handle(source_handle, parent_handle, new_name: str) -> None:
    padding_size = ctypes.sizeof(ctypes.c_void_p) - 1

    class _RENAME_HEADER(ctypes.Structure):
        _pack_ = 1
        _fields_ = [
            ("ReplaceIfExists", ctypes.c_ubyte),
            ("Padding", ctypes.c_ubyte * padding_size),
            ("RootDirectory", ctypes.c_void_p),
            ("FileNameLength", wintypes.DWORD),
        ]

    destination = _win_handle_final_path(parent_handle) / new_name
    encoded = str(destination).encode("utf-16-le")
    buffer = ctypes.create_string_buffer(
        ctypes.sizeof(_RENAME_HEADER) + len(encoded) + 2
    )
    header = _RENAME_HEADER.from_buffer(buffer)
    header.ReplaceIfExists = 0
    header.RootDirectory = None
    header.FileNameLength = len(encoded)
    ctypes.memmove(
        ctypes.addressof(buffer) + ctypes.sizeof(_RENAME_HEADER),
        encoded,
        len(encoded),
    )
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    set_info = kernel32.SetFileInformationByHandle
    set_info.argtypes = (
        wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD
    )
    set_info.restype = wintypes.BOOL
    if not set_info(source_handle, 3, buffer, len(buffer)):
        raise _error(
            "unable to rename owned directory handle",
            Path(new_name),
            ctypes.WinError(ctypes.get_last_error()),
        )


def _remove_owned_staging_windows(
    staging: Path,
    ownership: dict,
    parent_identity: tuple[int, int],
    hooks: dict | None,
) -> None:
    parent_handle = None
    staging_handle = None
    quarantine = staging.with_name(
        f".{staging.name}.quarantine-{secrets.token_hex(16)}"
    )
    try:
        parent_handle = _win_open_directory(
            staging.parent, share_delete=False
        )
        staging_handle = _win_open_directory(staging, delete=True)
        _win_validate_directory_handle(
            parent_handle, staging.parent, expected_ino=parent_identity[1]
        )
        _win_validate_directory_handle(
            staging_handle, staging, expected_ino=ownership["identity"][1]
        )
        _validate_staging_owner(staging, ownership, parent_identity)
        _run_hook(hooks, "before_cleanup_handle_rename")
        _win_rename_handle(staging_handle, parent_handle, quarantine.name)
    finally:
        close_error = None
        for handle in (staging_handle, parent_handle):
            try:
                _win_close_handle(handle)
            except OSError as exc:
                close_error = close_error or exc
        if close_error is not None and sys.exc_info()[0] is None:
            raise close_error
    _validate_staging_owner(quarantine, ownership, parent_identity)
    shutil.rmtree(quarantine)
    ownership["marker"].unlink()


def _promote_no_replace(source: Path, target: Path) -> None:
    source = Path(source)
    target = Path(target)
    try:
        if os.name == "nt":
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            move_file = kernel32.MoveFileW
            move_file.argtypes = (ctypes.c_wchar_p, ctypes.c_wchar_p)
            move_file.restype = ctypes.c_int
            if not move_file(str(source), str(target)):
                raise ctypes.WinError(ctypes.get_last_error())
            return
        libc = ctypes.CDLL(None, use_errno=True)
        if sys.platform.startswith("linux") and hasattr(libc, "renameat2"):
            renameat2 = libc.renameat2
            renameat2.argtypes = (
                ctypes.c_int, ctypes.c_char_p, ctypes.c_int,
                ctypes.c_char_p, ctypes.c_uint,
            )
            renameat2.restype = ctypes.c_int
            if renameat2(-100, os.fsencode(source), -100, os.fsencode(target), 1):
                raise OSError(ctypes.get_errno(), os.strerror(ctypes.get_errno()))
            return
        if sys.platform == "darwin" and hasattr(libc, "renamex_np"):
            renamex = libc.renamex_np
            renamex.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
            renamex.restype = ctypes.c_int
            if renamex(os.fsencode(source), os.fsencode(target), 4):
                raise OSError(ctypes.get_errno(), os.strerror(ctypes.get_errno()))
            return
        raise MigrationError("atomic no-replace directory promotion is unavailable")
    except MigrationError:
        raise
    except (OSError, ValueError) as exc:
        raise _error("unable to atomically promote without replacement", target, exc) from exc


def _atomic_no_replace(source: Path, target: Path) -> None:
    _promote_no_replace(source, target)


def _promote_owned_staging(
    staging: Path,
    target: Path,
    ownership: dict,
    content_identity: tuple[int, int],
    hooks: dict | None,
) -> None:
    if os.name != "nt":
        _promote_no_replace(staging, target)
        return
    content_handle = None
    staging_handle = None
    try:
        content_handle = _win_open_directory(
            target.parent, share_delete=False
        )
        staging_handle = _win_open_directory(staging, delete=True)
        _win_validate_directory_handle(
            content_handle, target.parent, expected_ino=content_identity[1]
        )
        _win_validate_directory_handle(
            staging_handle, staging, expected_ino=ownership["identity"][1]
        )
        _run_hook(hooks, "before_promotion_handle_rename")
        _win_rename_handle(staging_handle, content_handle, target.name)
    finally:
        close_error = None
        for handle in (staging_handle, content_handle):
            try:
                _win_close_handle(handle)
            except OSError as exc:
                close_error = close_error or exc
        if close_error is not None and sys.exc_info()[0] is None:
            raise close_error


def stage_and_promote(
    source: Path,
    target: Path,
    staging: Path,
    manifest: dict,
    *,
    copier=shutil.copy2,
    hooks: dict | None = None,
) -> dict:
    source = Path(source)
    root, target, staging = _project_and_staging(target, staging)
    _validate_manifest(manifest)
    owner_marker = staging.with_name(staging.name + ".owner")
    if _lexists(target):
        raise MigrationError(f"target already exists: {target}")
    if _lexists(staging) or _lexists(owner_marker):
        raise MigrationError(f"staging already exists: {staging}")
    verify_manifest(source, manifest)

    ownership = None
    promoted = False
    try:
        staging.parent.mkdir(parents=True, exist_ok=True)
        content_identity = _directory_identity(target.parent)
        staging_parent_identity = _directory_identity(staging.parent)
        _assert_directory_identity(target.parent, content_identity)
        _assert_directory_identity(staging.parent, staging_parent_identity)
        staging.mkdir(exist_ok=False)
        nonce = secrets.token_hex(32)
        staging_identity = _directory_identity(staging)
        with owner_marker.open("x", encoding="ascii", newline="") as handle:
            handle.write(nonce)
            handle.flush()
            os.fsync(handle.fileno())
        ownership = {
            "identity": staging_identity,
            "nonce": nonce,
            "marker": owner_marker,
            "marker_identity": _identity(_lstat_safe(owner_marker)),
        }
        _validate_staging_owner(staging, ownership, staging_parent_identity)
        _assert_directory_identity(target.parent, content_identity)
        shutil.copytree(
            source, staging, copy_function=copier, dirs_exist_ok=True
        )
        _validate_staging_owner(staging, ownership, staging_parent_identity)
        _assert_directory_identity(target.parent, content_identity)
        verify_manifest(source, manifest)
        verify_manifest(staging, manifest)
        _validate_staging_owner(staging, ownership, staging_parent_identity)
        _assert_directory_identity(target.parent, content_identity)
        if _lexists(target):
            raise MigrationError(f"target appeared before promotion: {target}")
        _promote_owned_staging(
            staging, target, ownership, content_identity, hooks
        )
        promoted = True
        owner_marker.unlink()
        verify_manifest(target, manifest)
        return {
            "status": "promoted",
            "target": str(target),
            "counts": dict(manifest["counts"]),
        }
    except BaseException as exc:
        if promoted:
            if isinstance(exc, KeyboardInterrupt):
                raise
            if isinstance(exc, MigrationError):
                detail = str(exc)
            else:
                detail = f"{type(exc).__name__}: {exc}"
            raise MigrationError(
                f"terminal failure after promotion; preserve target for isolation: {detail}"
            ) from exc
        cleanup_error = None
        if ownership is not None:
            try:
                _remove_owned_staging(
                    staging, root, ownership, staging_parent_identity, hooks
                )
            except (MigrationError, OSError, ValueError) as cleanup_exc:
                cleanup_error = cleanup_exc
        if isinstance(exc, KeyboardInterrupt):
            raise
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


def _safe_stderr_error(exc: BaseException) -> None:
    text = str(exc).encode("ascii", "backslashreplace").decode("ascii")
    print(f"error: {text}", file=sys.stderr)


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
        _safe_stderr_error(exc)
        return 1
    except (OSError, RuntimeError, ValueError) as exc:
        _safe_stderr_error(exc)
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
