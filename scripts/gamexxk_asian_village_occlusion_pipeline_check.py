#!/usr/bin/env python3
"""Run the full immutable Asian Village occlusion integration contract via UE MCP."""

from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
sys.path.insert(0, str(ROOT / "Content/Python"))

from gamexxk_occlusion_generation import (  # noqa: E402
    aggregate_file_digest,
    atomic_write_json,
    compute_content_signature,
)
from ue_mcp_client import UnrealMCPClient  # noqa: E402

AUTHOR = "Content/Python/gamexxk_author_asian_village_occlusion_materials.py"
ACTIVE = ROOT / "Config/GameXXK/Occlusion/AsianVillageOcclusionActiveGeneration.json"
REPORT = ROOT / "Config/GameXXK/Occlusion/AsianVillageMaterialAuthoringReport.json"
EVIDENCE = (
    ROOT
    / "docs/production/evidence/asian-village-integration/occlusion-full-coverage"
    / "immutable-generation-harness.json"
)


def _run(client, argv=None, expect_failure=False):
    try:
        response = client.run_project_python_file(AUTHOR, argv or [])
    except Exception as exc:
        if not expect_failure:
            raise
        return {"failed_as_expected": True, "error": str(exc)[:1000]}
    if expect_failure:
        raise RuntimeError("fault injection unexpectedly succeeded")
    stdout = str(response.get("stdout", "")).strip().splitlines()
    return json.loads(stdout[-1])


def _manifest_hash():
    return hashlib.sha256(ACTIVE.read_bytes()).hexdigest().upper()


def _generation_files(folder):
    relative = folder.removeprefix("/Game/")
    return sorted((ROOT / "Content" / relative).glob("*.uasset"))


def main():
    client = UnrealMCPClient(timeout=240)
    client.require_connected()
    client.stop_pie()
    client.save_dirty_packages()
    salt_arg = f"--integration-signature-salt={uuid.uuid4().hex}"

    active_before_mid = _manifest_hash()
    mid_fault = _run(
        client, [salt_arg, "--fault-mid-generation=5"], expect_failure=True
    )
    if _manifest_hash() != active_before_mid:
        raise RuntimeError("mid-generation fault changed active manifest")

    recovery = _run(client, [salt_arg])
    if recovery["validation_errors"] or recovery["generation_asset_count"] != 98:
        raise RuntimeError(f"recovery validation failed:{recovery}")
    if "_R" not in recovery["generation_folder"]:
        raise RuntimeError("poisoned base generation was not abandoned for a fresh folder")
    files = _generation_files(recovery["generation_folder"])
    first_digest = aggregate_file_digest(files)
    if len(files) != 98 or first_digest != recovery["aggregate_digest"]:
        raise RuntimeError("host generation digest/count mismatch")

    second = _run(client, [salt_arg])
    second_digest = aggregate_file_digest(_generation_files(second["generation_folder"]))
    if not second["generation_reused"]:
        raise RuntimeError("second run did not reuse completed generation")
    if second["generation_folder"] != recovery["generation_folder"]:
        raise RuntimeError("second run changed generation folder")
    if second_digest != first_digest:
        raise RuntimeError("second run mutated generation packages")

    active_before_switch_fault = _manifest_hash()
    switch_fault = _run(
        client, [salt_arg, "--fault-before-manifest-switch"], expect_failure=True
    )
    if _manifest_hash() != active_before_switch_fault:
        raise RuntimeError("pre-switch fault changed active manifest")
    failure_report = json.loads(REPORT.read_text(encoding="utf-8"))
    if failure_report.get("status") != "failure":
        raise RuntimeError("fault did not atomically publish failure report")
    final_run = _run(client, [salt_arg])

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        source = temp / "source.uasset"
        recipe = temp / "recipe.py"
        mpc = temp / "mpc.uasset"
        source.write_bytes(b"source-a")
        recipe.write_bytes(b"recipe-a")
        mpc.write_bytes(b"mpc-a")
        args = dict(
            inventory_bytes=b"inventory",
            source_packages={"source": source},
            recipe_files={"recipe": recipe},
            mpc_package=mpc,
            schema_version=3,
        )
        signature_a = compute_content_signature(**args)
        source.write_bytes(b"source-b")
        signature_source_changed = compute_content_signature(**args)
        source.write_bytes(b"source-a")
        recipe.write_bytes(b"recipe-b")
        signature_recipe_changed = compute_content_signature(**args)
    if signature_a in (signature_source_changed, signature_recipe_changed):
        raise RuntimeError("signature sensitivity contract failed")

    evidence = {
        "status": "success",
        "mid_generation_fault": mid_fault,
        "mid_fault_active_manifest_unchanged": True,
        "recovery_run": recovery,
        "second_run": second,
        "second_run_digest_unchanged": True,
        "pre_switch_fault": switch_fault,
        "pre_switch_active_manifest_unchanged": True,
        "failure_report": failure_report,
        "final_run": final_run,
        "integration_signature_salt": salt_arg.split("=", 1)[1],
        "generation_file_count": len(files),
        "aggregate_digest": first_digest,
        "signature_baseline": signature_a,
        "signature_source_changed": signature_source_changed,
        "signature_recipe_changed": signature_recipe_changed,
    }
    atomic_write_json(EVIDENCE, evidence)
    print(json.dumps(evidence, ensure_ascii=False))


if __name__ == "__main__":
    main()
