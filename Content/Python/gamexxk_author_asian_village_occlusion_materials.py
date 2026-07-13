"""Author blend-safe project-owned Asian Village occlusion material families."""

from __future__ import annotations

import json
import sys
import uuid
from pathlib import Path

import unreal

import gamexxk_occlusion_material_naming as naming
from gamexxk_occlusion_material_naming import cutout_object_path
from gamexxk_occlusion_generation import (
    aggregate_file_digest,
    atomic_write_json,
    compute_content_signature,
)


DESTINATION = "/Game/GameXXK/Materials/Occlusion/AsianVillageFull"
if DESTINATION != naming.DESTINATION_PATH:
    raise RuntimeError("occlusion naming destination contract drifted")
MPC_PATH = "/Game/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion"
INVENTORY_PATH = (
    Path(unreal.Paths.project_dir())
    / "Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json"
)
REPORT_PATH = (
    Path(unreal.Paths.project_dir())
    / "Config/GameXXK/Occlusion/AsianVillageMaterialAuthoringReport.json"
)
ACTIVE_GENERATION_PATH = (
    Path(unreal.Paths.project_dir())
    / "Config/GameXXK/Occlusion/AsianVillageOcclusionActiveGeneration.json"
)
AUTHORING_SCHEMA_VERSION = 3
COMPLETION_FOLDER = (
    Path(unreal.Paths.project_dir())
    / "Config/GameXXK/Occlusion/Generations"
)
SUPPORTED_BLENDS = {
    "BLEND_OPAQUE": unreal.BlendMode.BLEND_OPAQUE,
    "BLEND_MASKED": unreal.BlendMode.BLEND_MASKED,
    "BLEND_TRANSLUCENT": unreal.BlendMode.BLEND_TRANSLUCENT,
}


def _set(obj, name, value):
    obj.set_editor_property(name, value)


def _node(material, cls, x, y):
    result = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    if result is None:
        raise RuntimeError(f"failed to create {cls} in {material.get_path_name()}")
    return result


def _connect(source, output, destination, input_name, exact_output=False):
    outputs = tuple(output) if isinstance(output, (tuple, list)) else (output,)
    inputs = tuple(input_name) if isinstance(input_name, (tuple, list)) else (input_name,)
    if exact_output and len(outputs) != 1:
        raise RuntimeError(f"exact output connection requires one name, got {outputs}")
    for candidate_output in outputs:
        for candidate_input in inputs:
            if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, candidate_output, destination, candidate_input
            ):
                return
    reflected = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            destination
        )
    ]
    raise RuntimeError(
        f"connection failed: {source.get_name()}[{output!r}] -> "
        f"{destination.get_name()}[{input_name!r}], inputs={reflected}"
    )


def _connect_property(source, output, material_property):
    if not unreal.MaterialEditingLibrary.connect_material_property(
        source, output, material_property
    ):
        raise RuntimeError(
            f"material-property connection failed: {source.get_name()}[{output!r}] "
            f"-> {material_property}"
        )


def _collection_parameter(material, collection, parameter_name, x, y):
    expression = _node(material, unreal.MaterialExpressionCollectionParameter, x, y)
    _set(expression, "collection", collection)
    _set(expression, "parameter_name", parameter_name)
    return expression


def _circle_keep_mask(material, collection):
    screen = _node(material, unreal.MaterialExpressionScreenPosition, -1250, 650)
    center = _collection_parameter(material, collection, "OcclusionCenter", -1250, 790)
    center_rg = _node(material, unreal.MaterialExpressionComponentMask, -1040, 790)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(center_rg, channel, enabled)
    # CollectionParameter's unnamed output is the same vector as RGB in UE 5.8.
    _connect(center, ("RGB", ""), center_rg, ("Input", ""))

    delta = _node(material, unreal.MaterialExpressionSubtract, -1000, 650)
    # ScreenPosition's unnamed output aliases ViewportUV in this material domain.
    _connect(screen, ("ViewportUV", ""), delta, "A")
    _connect(center_rg, "", delta, "B")

    aspect = _collection_parameter(material, collection, "OcclusionAspect", -1000, 930)
    aspect_rg = _node(material, unreal.MaterialExpressionComponentMask, -790, 930)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(aspect_rg, channel, enabled)
    _connect(aspect, ("RGB", ""), aspect_rg, ("Input", ""))
    corrected = _node(material, unreal.MaterialExpressionMultiply, -750, 650)
    _connect(delta, "", corrected, "A")
    _connect(aspect_rg, "", corrected, "B")

    dot = _node(material, unreal.MaterialExpressionDotProduct, -520, 650)
    _connect(corrected, "", dot, "A")
    _connect(corrected, "", dot, "B")
    distance = _node(material, unreal.MaterialExpressionSquareRoot, -320, 650)
    _connect(dot, "", distance, ("Input", ""))
    radius = _collection_parameter(material, collection, "OcclusionRadius", -320, 830)
    edge = _node(material, unreal.MaterialExpressionSubtract, -100, 690)
    _connect(distance, "", edge, "A")
    _connect(radius, "", edge, "B")
    feather = _node(material, unreal.MaterialExpressionConstant, -100, 850)
    _set(feather, "r", 0.02)
    normalized = _node(material, unreal.MaterialExpressionDivide, 120, 710)
    _connect(edge, "", normalized, "A")
    _connect(feather, "", normalized, "B")
    keep_mask = _node(material, unreal.MaterialExpressionSaturate, 340, 710)
    _connect(normalized, "", keep_mask, ("Input", ""))
    return keep_mask


def _existing_property_connection(material, material_property):
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, material_property
    )
    if node is None:
        return None
    output_name = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
        material, material_property
    )
    if output_name is None:
        raise RuntimeError(
            f"connected {material_property} has no reflected output name in "
            f"{material.get_path_name()}; refusing to overwrite it"
        )
    output_name = str(output_name)
    output_names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_output_names(node)
    ]
    if output_name not in output_names and not (output_name == "" and output_names):
        raise RuntimeError(
            f"connected {material_property} output {output_name!r} is not among "
            f"{output_names} for {node.get_name()}; refusing to overwrite it"
        )
    return node, output_name


def _patch_root(material, source_blend, collection):
    blend_name = str(source_blend).split(".")[-1].split(":", 1)[0]
    if source_blend == unreal.BlendMode.BLEND_OPAQUE:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        force_masked = True
        preserve_existing = False
    elif source_blend == unreal.BlendMode.BLEND_MASKED:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        force_masked = True
        preserve_existing = True
    elif source_blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        material_property = unreal.MaterialProperty.MP_OPACITY
        target_blend = unreal.BlendMode.BLEND_TRANSLUCENT
        force_masked = False
        preserve_existing = True
    else:
        raise RuntimeError(
            f"unsupported root blend mode {blend_name}: {material.get_path_name()}"
        )

    existing = (
        _existing_property_connection(material, material_property)
        if preserve_existing
        else None
    )
    circle = _circle_keep_mask(material, collection)
    final_node = circle
    if existing is not None:
        original_node, output_name = existing
        multiply = _node(material, unreal.MaterialExpressionMultiply, 580, 650)
        _connect(original_node, output_name, multiply, "A", exact_output=True)
        _connect(circle, "", multiply, "B")
        final_node = multiply
    _connect_property(final_node, "", material_property)
    _set(material, "blend_mode", target_blend)
    material.modify()
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    if force_masked:
        if not unreal.GameXXKMaterialAuthoringLibrary.force_masked_material_compilation(
            material
        ):
            raise RuntimeError(
                f"force_masked compilation failed for {material.get_path_name()}"
            )
    errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"recompile failed for {material.get_path_name()}: {errors}")


def _package_path(object_path):
    return str(object_path).split(".", 1)[0]


def _project_package_file(package_path):
    if not package_path.startswith("/Game/"):
        raise RuntimeError(f"source package is outside project Content:{package_path}")
    content = Path(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    ).resolve()
    filename = (content / (package_path.removeprefix("/Game/") + ".uasset")).resolve()
    if content not in filename.parents or not filename.is_file():
        raise RuntimeError(f"verified project package file missing:{filename}")
    return filename


def _collect_source_packages(eligible):
    packages = {}

    def add(asset):
        if asset is None:
            raise RuntimeError("missing source asset while collecting signature")
        object_path = str(asset.get_path_name())
        package_path = object_path.split(".", 1)[0]
        packages[package_path] = _project_package_file(package_path)
        if isinstance(asset, unreal.MaterialInstanceConstant):
            add(asset.get_editor_property("parent"))

    for entry in eligible:
        add(unreal.EditorAssetLibrary.load_asset(entry["source_path"]))
    return packages


def _generation_package_files(generation_folder):
    return [
        _project_package_file(_package_path(path))
        for path in unreal.EditorAssetLibrary.list_assets(
            generation_folder, recursive=False, include_folder=False
        )
    ]


def _generation_asset_path(source_path, generation_folder):
    return f"{generation_folder}/{naming.cutout_asset_name(source_path)}"


def _duplicate_family(source, collection, cache, generation_folder, fault_state=None):
    if source is None:
        raise RuntimeError("cannot duplicate a missing source asset")
    source_path = str(source.get_path_name())
    if source_path in cache:
        return cache[source_path]["asset"]
    concrete = _generation_asset_path(source_path, generation_folder)
    if not concrete.startswith(generation_folder + "/"):
        raise RuntimeError(f"refusing non-generation destination {concrete}")
    if source_path.startswith(DESTINATION + "/"):
        raise RuntimeError(f"refusing to use generated asset as source: {source_path}")

    if isinstance(source, unreal.MaterialInstanceConstant):
        parent = source.get_editor_property("parent")
        if parent is None:
            raise RuntimeError(f"material instance has no parent: {source_path}")
        copied_parent = _duplicate_family(
            parent, collection, cache, generation_folder, fault_state
        )
        depth = cache[str(parent.get_path_name())]["depth"] + 1
        duplicate = unreal.EditorAssetLibrary.duplicate_asset(source_path, concrete)
        if duplicate is None:
            raise RuntimeError(f"failed to duplicate {source_path} -> {concrete}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(duplicate, copied_parent)
        duplicate.modify()
    elif isinstance(source, unreal.Material):
        depth = 0
        duplicate = unreal.EditorAssetLibrary.duplicate_asset(source_path, concrete)
        if duplicate is None:
            raise RuntimeError(f"failed to duplicate {source_path} -> {concrete}")
        _patch_root(duplicate, source.get_blend_mode(), collection)
    else:
        raise RuntimeError(
            f"unsupported material ancestry asset {source_path}: "
            f"{source.get_class().get_name()}"
        )
    if not unreal.EditorAssetLibrary.save_asset(concrete, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save_asset {concrete}")
    cache[source_path] = {
        "asset": duplicate,
        "source": source_path,
        "concrete": concrete,
        "depth": depth,
    }
    if fault_state is not None:
        fault_state["created"] += 1
        if fault_state["limit"] and fault_state["created"] >= fault_state["limit"]:
            raise RuntimeError(
                f"intentional mid-generation fault after {fault_state['created']} assets"
            )
    return duplicate


def _load_generation_records(eligible, generation_folder):
    records = {}

    def add(source):
        source_path = str(source.get_path_name())
        if source_path in records:
            return records[source_path]
        if isinstance(source, unreal.MaterialInstanceConstant):
            parent = source.get_editor_property("parent")
            if parent is None:
                raise RuntimeError(f"material instance has no parent:{source_path}")
            parent_record = add(parent)
            depth = parent_record["depth"] + 1
        elif isinstance(source, unreal.Material):
            depth = 0
        else:
            raise RuntimeError(f"unsupported ancestry type:{source_path}")
        concrete = _generation_asset_path(source_path, generation_folder)
        records[source_path] = {
            "asset": unreal.EditorAssetLibrary.load_asset(concrete),
            "source": source_path,
            "concrete": concrete,
            "depth": depth,
        }
        return records[source_path]

    for entry in eligible:
        add(unreal.EditorAssetLibrary.load_asset(entry["source_path"]))
    return records


def _validate_family(eligible, records, generation_folder):
    errors = []
    expected_paths = {record["concrete"] for record in records.values()}
    target_paths = []
    for entry in eligible:
        record = records.get(entry["source_path"])
        if record is None:
            errors.append(f"mapping missing:{entry['source_path']}")
            continue
        path = record["concrete"]
        target_paths.append(path)
        target = unreal.EditorAssetLibrary.load_asset(path)
        source = unreal.EditorAssetLibrary.load_asset(entry["source_path"])
        if target is None or source is None:
            errors.append(f"reload failed:{entry['source_path']}:{path}")
            continue
        if isinstance(source, unreal.MaterialInstanceConstant) != isinstance(
            target, unreal.MaterialInstanceConstant
        ):
            errors.append(f"type mismatch:{entry['source_path']}:{path}")
        expected_blend = (
            unreal.BlendMode.BLEND_TRANSLUCENT
            if entry["blend_mode"] == "BLEND_TRANSLUCENT"
            else unreal.BlendMode.BLEND_MASKED
        )
        if target.get_blend_mode() != expected_blend:
            errors.append(f"blend mismatch:{path}:{target.get_blend_mode()}")
        visited = set()
        current = target
        while isinstance(current, unreal.MaterialInstanceConstant):
            current_path = _package_path(current.get_path_name())
            if current_path in visited:
                errors.append(f"parent cycle:{path}")
                break
            visited.add(current_path)
            parent = current.get_editor_property("parent")
            parent_path = "" if parent is None else _package_path(parent.get_path_name())
            if parent_path not in expected_paths:
                errors.append(f"non-owned parent:{current_path}:{parent_path}")
                break
            current = parent
        if isinstance(current, unreal.Material):
            prop = (
                unreal.MaterialProperty.MP_OPACITY
                if current.get_blend_mode() == unreal.BlendMode.BLEND_TRANSLUCENT
                else unreal.MaterialProperty.MP_OPACITY_MASK
            )
            final_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
                current, prop
            )
            if final_node is None:
                errors.append(f"missing property connection:{current.get_path_name()}:{prop}")
            source_root = source
            while isinstance(source_root, unreal.MaterialInstanceConstant):
                source_root = source_root.get_editor_property("parent")
            source_prop = (
                unreal.MaterialProperty.MP_OPACITY
                if source_root.get_blend_mode() == unreal.BlendMode.BLEND_TRANSLUCENT
                else unreal.MaterialProperty.MP_OPACITY_MASK
            )
            original = unreal.MaterialEditingLibrary.get_material_property_input_node(
                source_root, source_prop
            )
            if source_root.get_blend_mode() == unreal.BlendMode.BLEND_OPAQUE:
                if not isinstance(final_node, unreal.MaterialExpressionSaturate):
                    errors.append(f"opaque root is not circle-only:{current.get_path_name()}")
            elif original is not None:
                if not isinstance(final_node, unreal.MaterialExpressionMultiply):
                    errors.append(f"preserved root is not Multiply:{current.get_path_name()}")
                else:
                    inputs = list(
                        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                            current, final_node
                        )
                    )
                    input_names = [str(node.get_name()) for node in inputs]
                    original_input = next(
                        (
                            node
                            for node in inputs
                            if str(node.get_name()) == str(original.get_name())
                        ),
                        None,
                    )
                    if original_input is None:
                        errors.append(f"Multiply lost original expression:{current.get_path_name()}")
                    if not any(isinstance(node, unreal.MaterialExpressionSaturate) for node in inputs):
                        errors.append(f"Multiply lost circle input:{current.get_path_name()}")
                    expected_output = str(
                        unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
                            source_root, source_prop
                        )
                    )
                    observed_output = (
                        ""
                        if original_input is None
                        else str(
                            unreal.MaterialEditingLibrary.get_input_node_output_name_for_material_expression(
                                final_node, original_input
                            )
                        )
                    )
                    if original_input is not None and observed_output != expected_output:
                        errors.append(
                            f"Multiply original output changed:{current.get_path_name()}:"
                            f"{observed_output!r}!={expected_output!r}"
                        )
        else:
            errors.append(f"parent chain did not reach Material:{path}")
    if len(target_paths) != len(eligible) or len(set(target_paths)) != len(eligible):
        errors.append(
            f"inventory mapping count/duplicate mismatch:{len(target_paths)}:{len(set(target_paths))}"
        )
    for expected in expected_paths:
        if not unreal.EditorAssetLibrary.does_asset_exist(expected):
            errors.append(f"family asset missing:{expected}")
    actual = {
        _package_path(path)
        for path in unreal.EditorAssetLibrary.list_assets(
            generation_folder, recursive=False, include_folder=False
        )
    }
    if actual != expected_paths:
        errors.append(
            f"generation family mismatch:expected={len(expected_paths)} actual={len(actual)}"
        )
    return errors


def author():
    inventory_text = INVENTORY_PATH.read_text(encoding="utf-8")
    inventory = json.loads(inventory_text)
    entries = list(inventory["materials"])
    eligible = [entry for entry in entries if not entry["excluded"]]
    excluded = [entry for entry in entries if entry["excluded"]]
    if len(eligible) != 74 or len(excluded) != 6:
        raise RuntimeError(
            f"inventory count mismatch: eligible={len(eligible)} excluded={len(excluded)}"
        )
    for entry in entries:
        if entry["blend_mode"] not in SUPPORTED_BLENDS:
            raise RuntimeError(
                f"unsupported inventory blend {entry['blend_mode']}: {entry['source_path']}"
            )
        expected = cutout_object_path(entry["source_path"])
        if entry["target_path"] != expected:
            raise RuntimeError(
                f"inventory target mismatch: {entry['target_path']} != {expected}"
            )

    collection = unreal.EditorAssetLibrary.load_asset(MPC_PATH)
    if collection is None:
        raise RuntimeError(f"required parameter collection is missing: {MPC_PATH}")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)
    source_packages = _collect_source_packages(eligible)
    signature_salt = ""
    for argument in sys.argv[1:]:
        if argument.startswith("--integration-signature-salt="):
            signature_salt = argument.split("=", 1)[1]
    author_file = Path(__file__).resolve()
    recipe_files = {
        "author": author_file,
        "naming": Path(naming.__file__).resolve(),
        "generation_helpers": Path(
            sys.modules[compute_content_signature.__module__].__file__
        ).resolve(),
    }
    content_signature = compute_content_signature(
        inventory_bytes=(inventory_text + "\nintegration_salt=" + signature_salt).encode("utf-8"),
        source_packages=source_packages,
        recipe_files=recipe_files,
        mpc_package=_project_package_file(MPC_PATH),
        schema_version=AUTHORING_SCHEMA_VERSION,
    )
    base_generation_folder = f"{DESTINATION}/Gen_{content_signature}"
    active = {}
    if ACTIVE_GENERATION_PATH.is_file():
        active = json.loads(ACTIVE_GENERATION_PATH.read_text(encoding="utf-8"))
    active_folder = active.get("generation_folder", "")
    generation_folder = (
        active_folder
        if active.get("content_signature") == content_signature
        else base_generation_folder
    )
    completion_path = COMPLETION_FOLDER / (generation_folder.rsplit("/", 1)[-1] + ".json")
    completion = {}
    if completion_path.is_file():
        completion = json.loads(completion_path.read_text(encoding="utf-8"))
    generation_reused = False
    if unreal.EditorAssetLibrary.does_directory_exist(generation_folder) and completion:
        records = _load_generation_records(eligible, generation_folder)
        validation_errors = _validate_family(eligible, records, generation_folder)
        current_digest = aggregate_file_digest(
            _generation_package_files(generation_folder)
        )
        generation_reused = (
            not validation_errors
            and completion.get("content_signature") == content_signature
            and completion.get("generation_folder") == generation_folder
            and completion.get("aggregate_digest") == current_digest
        )
    if not generation_reused and unreal.EditorAssetLibrary.does_directory_exist(
        generation_folder
    ):
        generation_folder = (
            f"{base_generation_folder}_R{uuid.uuid4().hex[:8].upper()}"
        )
        completion_path = COMPLETION_FOLDER / (
            generation_folder.rsplit("/", 1)[-1] + ".json"
        )
    if generation_reused:
        pass
    else:
        unreal.EditorAssetLibrary.make_directory(generation_folder)
        records = {}
        fault_limit = 0
        for argument in sys.argv[1:]:
            if argument.startswith("--fault-mid-generation="):
                fault_limit = int(argument.split("=", 1)[1])
        fault_state = {"created": 0, "limit": fault_limit}
        for entry in eligible:
            source = unreal.EditorAssetLibrary.load_asset(entry["source_path"])
            _duplicate_family(
                source, collection, records, generation_folder, fault_state
            )
    validation_errors = _validate_family(eligible, records, generation_folder)
    if validation_errors:
        raise RuntimeError(f"immutable generation validation failed:{validation_errors}")
    aggregate_digest = aggregate_file_digest(
        _generation_package_files(generation_folder)
    )
    if not generation_reused:
        atomic_write_json(
            completion_path,
            {
                "schema_version": AUTHORING_SCHEMA_VERSION,
                "content_signature": content_signature,
                "generation_folder": generation_folder,
                "aggregate_digest": aggregate_digest,
                "asset_count": len(records),
                "validation_errors": validation_errors,
            },
        )
    concrete_mapping = {
        entry["source_path"]: records[entry["source_path"]]["concrete"]
        for entry in eligible
    }
    missing_targets = [
        path for path in concrete_mapping.values()
        if not unreal.EditorAssetLibrary.does_asset_exist(path)
    ]
    if "--fault-before-manifest-switch" in sys.argv[1:]:
        raise RuntimeError("intentional pre-switch fault")
    active_manifest = {
        "schema_version": AUTHORING_SCHEMA_VERSION,
        "content_signature": content_signature,
        "generation_folder": generation_folder,
        "inventory_mapping": concrete_mapping,
    }
    atomic_write_json(ACTIVE_GENERATION_PATH, active_manifest)
    counts = {
        name: sum(entry["blend_mode"] == name for entry in eligible)
        for name in SUPPORTED_BLENDS
    }
    report = {
        "status": "success",
        "eligible_count": len(eligible),
        "excluded_count": len(excluded),
        "created_count": len(records),
        "inventory_mapping_count": len(eligible),
        "ancestry_count": len(records),
        "root_count": sum(record["depth"] == 0 for record in records.values()),
        "content_signature": content_signature,
        "generation_folder": generation_folder,
        "generation_asset_count": len(records),
        "generation_reused": generation_reused,
        "aggregate_digest": aggregate_digest,
        "completion_record": str(
            completion_path.relative_to(Path(unreal.Paths.project_dir()))
        ).replace("\\", "/"),
        "active_generation_manifest": "Config/GameXXK/Occlusion/AsianVillageOcclusionActiveGeneration.json",
        "opaque_count": counts["BLEND_OPAQUE"],
        "masked_count": counts["BLEND_MASKED"],
        "translucent_count": counts["BLEND_TRANSLUCENT"],
        "opaque_source_count": counts["BLEND_OPAQUE"],
        "masked_source_count": counts["BLEND_MASKED"],
        "translucent_source_count": counts["BLEND_TRANSLUCENT"],
        "missing_targets": missing_targets,
        "failures": [],
        "validation_errors": validation_errors,
    }
    atomic_write_json(REPORT_PATH, report)
    print(json.dumps(report, ensure_ascii=False))
    if validation_errors or missing_targets:
        raise RuntimeError(f"authoring incomplete: {json.dumps(report, ensure_ascii=False)}")
    return report


def main():
    try:
        return author()
    except Exception as exc:
        atomic_write_json(
            REPORT_PATH,
            {
                "status": "failure",
                "error": repr(exc),
                "active_generation_manifest": "Config/GameXXK/Occlusion/AsianVillageOcclusionActiveGeneration.json",
                "validation_errors": [str(exc)],
            },
        )
        raise


if __name__ == "__main__":
    main()
