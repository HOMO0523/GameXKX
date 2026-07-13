"""Pure hashing and atomic JSON helpers for immutable occlusion generations."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path


def _hash_labeled_files(hasher, files):
    for label, filename in sorted((str(k), Path(v)) for k, v in files.items()):
        data = filename.read_bytes()
        hasher.update(label.encode("utf-8"))
        hasher.update(len(data).to_bytes(8, "big"))
        hasher.update(data)


def compute_content_signature(
    *, inventory_bytes, source_packages, recipe_files, mpc_package, schema_version
):
    hasher = hashlib.sha256()
    hasher.update(f"schema={int(schema_version)}\n".encode("ascii"))
    hasher.update(bytes(inventory_bytes))
    _hash_labeled_files(hasher, source_packages)
    _hash_labeled_files(hasher, recipe_files)
    _hash_labeled_files(hasher, {"mpc": Path(mpc_package)})
    return hasher.hexdigest()[:16].upper()


def aggregate_file_digest(files):
    hasher = hashlib.sha256()
    for filename in sorted(Path(value) for value in files):
        data = filename.read_bytes()
        hasher.update(filename.name.encode("utf-8"))
        hasher.update(len(data).to_bytes(8, "big"))
        hasher.update(data)
    return hasher.hexdigest().upper()


def atomic_write_json(path, payload):
    target = Path(path)
    temporary = target.with_suffix(target.suffix + ".tmp")
    temporary.parent.mkdir(parents=True, exist_ok=True)
    temporary.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, target)
