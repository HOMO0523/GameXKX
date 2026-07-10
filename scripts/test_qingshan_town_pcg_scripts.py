"""Host-side source contract for the Qingshan town PCG editor library."""

from __future__ import annotations

from pathlib import Path
import importlib.util
import json
import re
import sys
import types
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
HEADER = PROJECT_ROOT / "Source" / "GameXXKEditor" / "Public" / "GameXXKTownPCGAutomationLibrary.h"
IMPLEMENTATION = (
    PROJECT_ROOT
    / "Source"
    / "GameXXKEditor"
    / "Private"
    / "GameXXKTownPCGAutomationLibrary.cpp"
)
BUILD_RULES = PROJECT_ROOT / "Source" / "GameXXKEditor" / "GameXXKEditor.Build.cs"
ASSEMBLY_SCRIPT = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_assemble_qingshan_town_pcg_vertical_slice.py"
)
VALIDATOR_SCRIPT = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_validate_qingshan_town_pcg_vertical_slice.py"
)


def _import_assembly_script_with_unreal_stub():
    unreal_stub = types.ModuleType("unreal")
    original_unreal = sys.modules.get("unreal")
    content_python = str(ASSEMBLY_SCRIPT.parent)
    path_added = content_python not in sys.path
    if path_added:
        sys.path.insert(0, content_python)
    sys.modules["unreal"] = unreal_stub
    try:
        spec = importlib.util.spec_from_file_location(
            "gamexxk_assemble_qingshan_town_pcg_vertical_slice_contract",
            ASSEMBLY_SCRIPT,
        )
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if original_unreal is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = original_unreal
        if path_added:
            sys.path.remove(content_python)


def _import_validator_script_with_unreal_stub():
    unreal_stub = types.ModuleType("unreal")
    original_unreal = sys.modules.get("unreal")
    content_python = str(VALIDATOR_SCRIPT.parent)
    path_added = content_python not in sys.path
    if path_added:
        sys.path.insert(0, content_python)
    sys.modules["unreal"] = unreal_stub
    try:
        spec = importlib.util.spec_from_file_location(
            "gamexxk_validate_qingshan_town_pcg_vertical_slice_contract",
            VALIDATOR_SCRIPT,
        )
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if original_unreal is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = original_unreal
        if path_added:
            sys.path.remove(content_python)


class QingshanTownPCGScriptsTests(unittest.TestCase):
    def test_vertical_slice_validator_exposes_stable_host_safe_contract(self):
        self.assertTrue(VALIDATOR_SCRIPT.is_file(), f"missing validator: {VALIDATOR_SCRIPT}")
        module = _import_validator_script_with_unreal_stub()

        self.assertEqual(module.SOURCE_MAP, "/Game/GameXXK/Maps/L_QingshanInn")
        self.assertEqual(
            module.PROTOTYPE_MAP,
            "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype",
        )
        self.assertEqual(module.PCG_ACTOR_LABEL, "QingshanTown_PCG_Buildings")
        self.assertEqual(module.BUILDING_HARD_CAP, 16)
        self.assertEqual(module.EXPECTED_POINT_COUNT, 12)
        self.assertEqual(module.EXPECTED_NODE_COUNT, 2)
        self.assertEqual(module.EXPECTED_EDGE_COUNT, 2)
        self.assertEqual(module.EXPECTED_INSTANCE_COUNT, 12)
        self.assertEqual(
            module.EXPECTED_GRAPH_PATH,
            "/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice",
        )
        self.assertEqual(
            module.EXPECTED_BUILDING_MESH,
            "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K",
        )
        self.assertEqual(
            set(module.REQUIRED_FIXED_LABELS),
            {
                "QingshanTown_CityScope",
                "QingshanTown_MainRoad",
                "QingshanTown_RoadEdge_Left",
                "QingshanTown_RoadEdge_Right",
                "QingshanTown_River",
                "QingshanTown_Bridge",
                "QingshanTown_Market",
                "QingshanTown_SouthWharf",
            },
        )
        self.assertEqual(
            module.REQUIRED_QUICK_ROAD_TAGS,
            {
                "Quick_Road_CityScope": 1,
                "Quick_Road_MainRoad": 1,
                "Quick_Road_RoadEdge": 2,
            },
        )
        for helper_name in ("validate_vertical_slice", "_new_result", "_final_json", "main"):
            with self.subTest(helper_name=helper_name):
                self.assertTrue(callable(getattr(module, helper_name, None)))

    def test_vertical_slice_validator_source_is_strictly_read_only(self):
        source = VALIDATOR_SCRIPT.read_text(encoding="utf-8")
        forbidden_api_tokens = (
            "save_current_level",
            "save_loaded_asset",
            "save_asset",
            "duplicate_asset",
            "generate_town_pcg",
            "clear_town_pcg",
            "create_or_update_town_pcg_graph",
            "attach_town_pcg_graph",
            "create_or_update_tagged_spline",
            "destroy_actor",
            "spawn_actor",
            "set_editor_property",
        )
        for token in forbidden_api_tokens:
            with self.subTest(token=token):
                self.assertNotIn(token, source.lower())
        self.assertIn("EditorLoadingAndSavingUtils.load_map(PROTOTYPE_MAP)", source)
        self.assertNotIn("load_map(SOURCE_MAP)", source)
        self.assertIn("get_town_pcg_status(PCG_ACTOR_LABEL)", source)

    def test_vertical_slice_validator_contract_covers_structural_evidence(self):
        source = VALIDATOR_SCRIPT.read_text(encoding="utf-8")
        required_fields = (
            "dirty_map_packages_before",
            "dirty_map_packages_after",
            "required_label_counts",
            "quick_road_tag_counts",
            "spline_evidence",
            "pcg_graph",
            "point_count",
            "node_count",
            "verified_edge_count",
            "pcg_actor",
            "runtime_generation_disabled",
            "generated_component_count",
            "building_instance_count",
            "building_hard_cap",
            "generated_mesh",
            "tree_markers",
            "alignment",
            "collision",
            "preserved_labels",
            "source_map_not_current",
        )
        for field in required_fields:
            with self.subTest(field=field):
                self.assertIn(field, source)
        self.assertIn("get_current_level()", source)
        self.assertIn("get_editor_subsystem(unreal.LevelEditorSubsystem)", source)
        self.assertIn("get_actor_label()", source)
        self.assertIn("get_path_name", source)
        self.assertIn("pcg_component.get_graph()", source)
        self.assertIn("_package_path(candidate)", source)
        self.assertIn('get_editor_property("output_pins")', source)
        self.assertIn('get_editor_property("edges")', source)
        self.assertIn("tagged_marker_actors", source)
        self.assertIn("unexpected_marker_labels", source)
        self.assertIn("current_level_pcg_volumes", source)
        self.assertIn("current_level_pcg_components", source)
        self.assertIn("global_pcg_component_count", source)
        self.assertIn("current_level_generated_isms", source)
        self.assertIn("global_generated_ism_count", source)
        self.assertIn('PCG_GENERATED_COMPONENT_TAG = "PCG Generated Component"', source)
        self.assertIn("PCG_GENERATED_COMPONENT_TAG in _tags(component)", source)

    def test_vertical_slice_validator_final_json_helper_is_compact_and_stable(self):
        module = _import_validator_script_with_unreal_stub()
        result = module._new_result()
        self.assertEqual(result["success"], False)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["warnings"], [])
        self.assertIsInstance(result["evidence"], dict)
        encoded = module._final_json(result)
        self.assertEqual(json.loads(encoded), result)
        self.assertNotIn("\n", encoded)
        self.assertIn('"success":false', encoded)

    def test_vertical_slice_assembly_exposes_host_safe_idempotent_contract(self):
        self.assertTrue(ASSEMBLY_SCRIPT.is_file(), f"missing assembly script: {ASSEMBLY_SCRIPT}")
        module = _import_assembly_script_with_unreal_stub()

        self.assertEqual(module.SOURCE_MAP, "/Game/GameXXK/Maps/L_QingshanInn")
        self.assertEqual(
            module.PROTOTYPE_MAP,
            "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype",
        )
        self.assertEqual(module.PCG_ACTOR_LABEL, "QingshanTown_PCG_Buildings")
        self.assertIn("QingshanInn_TownExit", module.PRESERVED_LABELS)
        self.assertIn("PlayerStart_QingshanInn", module.PRESERVED_LABELS)
        self.assertEqual(
            set(module.QUICK_ROAD_TAGS),
            {"Quick_Road_CityScope", "Quick_Road_MainRoad", "Quick_Road_RoadEdge"},
        )
        self.assertEqual(module.TREE_MARKER_LABEL_PREFIX, "QingshanTown_TreeMarker_")
        self.assertEqual(module.TREE_MARKER_COUNT, 12)
        self.assertEqual(
            tuple(module.TREE_MARKER_MESH_CANDIDATES),
            ("/Quick_Road/Mesh/SM_Cone.SM_Cone", "/Engine/BasicShapes/Cone.Cone"),
        )
        expected_managed_labels = {
                "QingshanTown_CityScope",
                "QingshanTown_MainRoad",
                "QingshanTown_RoadEdge_Left",
                "QingshanTown_RoadEdge_Right",
                "QingshanTown_River",
                "QingshanTown_Bridge",
                "QingshanTown_Market",
                "QingshanTown_SouthWharf",
                "QingshanTown_PCG_Buildings",
        }
        expected_managed_labels.update(
            f"QingshanTown_TreeMarker_{index:02d}" for index in range(12)
        )
        self.assertEqual(set(module.REQUIRED_MANAGED_LABELS), expected_managed_labels)
        for helper_name in (
            "_find_unique_actor_by_label",
            "_snapshot_preserved_actors",
            "_create_or_update_spline_actor",
            "_create_or_update_anchor_actor",
            "_create_or_update_tree_marker_actor",
            "_create_or_update_tree_markers",
            "_count_generated_building_instances",
            "setup_vertical_slice",
            "finalize_vertical_slice",
            "assemble_vertical_slice",
        ):
            with self.subTest(helper_name=helper_name):
                self.assertTrue(callable(getattr(module, helper_name, None)))
        self.assertEqual(module.PHASE_SETUP, "setup")
        self.assertEqual(module.PHASE_FINALIZE, "finalize")

    def test_tree_debug_markers_are_real_idempotent_static_mesh_actors(self):
        source = ASSEMBLY_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("unreal.StaticMeshActor", source)
        self.assertIn('f"{TREE_MARKER_LABEL_PREFIX}{index:02d}"', source)
        self.assertIn("_find_unique_actor_by_label(label, unreal.StaticMeshActor)", source)
        self.assertIn("TownPCG_TreeDebug", source)
        self.assertIn("PrototypeOnly", source)
        self.assertIn("get_current_level()", source)
        self.assertIn("destroy_actor(obsolete)", source)
        self.assertIn("unreal.Vector(4.0, 4.0, 8.0)", source)
        self.assertNotIn(
            '"QingshanTown_TreeMarkers",\n        north_transform,\n        points,',
            source,
        )

    def test_vertical_slice_duplicate_world_is_saved_released_and_unloaded_before_load(self):
        source = ASSEMBLY_SCRIPT.read_text(encoding="utf-8")
        ensure_start = source.index("def _ensure_prototype_map()")
        ensure_end = source.index("\ndef _require_graph_counts", ensure_start)
        ensure_body = source[ensure_start:ensure_end]
        self.assertIn("save_loaded_asset", ensure_body)
        self.assertIn("del duplicate", ensure_body)
        self.assertIn("unload_packages([duplicate_package])", ensure_body)

    def test_vertical_slice_uses_ue58_transform_constructor_keywords(self):
        source = ASSEMBLY_SCRIPT.read_text(encoding="utf-8")
        self.assertNotIn("translation=world_location", source)
        self.assertIn("location=world_location", source)
        self.assertIn("yaw=north_yaw + float(point[\"yaw_degrees\"])", source)

    def test_vertical_slice_clears_previous_generation_before_rescheduling(self):
        source = ASSEMBLY_SCRIPT.read_text(encoding="utf-8")
        clear_index = source.index("library.clear_town_pcg(PCG_ACTOR_LABEL)")
        generate_index = source.index("library.generate_town_pcg(PCG_ACTOR_LABEL)")
        self.assertLess(clear_index, generate_index)

    def test_editor_library_declares_required_blueprint_operations(self):
        self.assertTrue(HEADER.is_file(), f"missing editor library header: {HEADER}")
        self.assertTrue(
            IMPLEMENTATION.is_file(),
            f"missing editor library implementation: {IMPLEMENTATION}",
        )

        header = HEADER.read_text(encoding="utf-8")
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("UGameXXKTownPCGAutomationLibrary", header)
        self.assertIn("UBlueprintFunctionLibrary", header)
        for operation in (
            "CreateOrUpdateTaggedSpline",
            "CreateOrUpdateTownPCGGraph",
            "AttachTownPCGGraph",
            "GenerateTownPCG",
            "GetTownPCGStatus",
            "ClearTownPCG",
        ):
            with self.subTest(operation=operation):
                self.assertRegex(header, rf"\b{operation}\s*\(")
                self.assertRegex(implementation, rf"\b{operation}\s*\(")

    def test_editor_module_declares_pcg_dependencies(self):
        build_rules = BUILD_RULES.read_text(encoding="utf-8")
        public_match = re.search(
            r"PublicDependencyModuleNames\.AddRange\s*\(.*?\{(?P<body>.*?)\}\s*\)",
            build_rules,
            re.DOTALL,
        )
        self.assertIsNotNone(public_match, "missing public dependency declaration")
        public_dependencies = set(re.findall(r'"([A-Za-z0-9_]+)"', public_match.group("body")))
        self.assertIn("PCG", public_dependencies)

    def test_generated_component_count_uses_public_pcg_component_tags(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertNotIn("GetManagedResources", implementation)
        self.assertIn("PCGHelpers::DefaultPCGTag", implementation)

    def test_mutating_operations_are_transactional_and_generation_is_nonblocking(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("FScopedTransaction", implementation)
        self.assertIn("GenerateLocalGetTaskId", implementation)
        self.assertIn("IsGenerating()", implementation)
        self.assertNotIn("GenerateLocal(true)", implementation)

    def test_generation_never_blocks_or_nests_the_live_editor_frame(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        generate_start = implementation.index("FString UGameXXKTownPCGAutomationLibrary::GenerateTownPCG")
        generate_end = implementation.index("FString UGameXXKTownPCGAutomationLibrary::ClearTownPCG", generate_start)
        generate_body = implementation[generate_start:generate_end]
        self.assertNotIn("FakeEngineTick", generate_body)
        self.assertNotIn("ProcessThreadUntilIdle", generate_body)
        self.assertNotIn("SleepNoStats", generate_body)
        self.assertNotIn("while (", generate_body)
        self.assertIn('SetBoolField(TEXT("scheduled")', generate_body)
        self.assertIn('SetBoolField(TEXT("generating")', generate_body)
        self.assertIn('SetBoolField(TEXT("generated")', generate_body)

    def test_read_only_status_operation_reports_public_pcg_state(self):
        header = HEADER.read_text(encoding="utf-8")
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("GetTownPCGStatus(const FString& ActorLabel)", header)
        self.assertIn("GetTownPCGStatus(const FString& ActorLabel)", implementation)
        self.assertIn('SetStringField(TEXT("state")', implementation)
        self.assertIn('SetBoolField(TEXT("generating")', implementation)
        self.assertIn('SetBoolField(TEXT("generated")', implementation)
        self.assertIn('SetNumberField(TEXT("generated_component_count")', implementation)

    def test_read_only_status_does_not_apply_mutator_lock_or_read_only_restrictions(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        status_start = implementation.index("FString UGameXXKTownPCGAutomationLibrary::GetTownPCGStatus")
        status_end = implementation.index("FString UGameXXKTownPCGAutomationLibrary::ClearTownPCG", status_start)
        status_body = implementation[status_start:status_end]
        self.assertIn("ValidatePrototypeReadContext", implementation)
        self.assertIn("ValidatePrototypeReadContext(World, nullptr, Error)", status_body)
        self.assertIn("ValidatePrototypeReadContext(World, Volume, Error)", status_body)
        self.assertNotIn("ValidatePrototypeMutationContext", status_body)

    def test_graph_authoring_rejects_an_existing_object_of_the_wrong_class(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("LoadAssetByPackagePath<UObject>", implementation)
        self.assertIn("FindObject<UObject>", implementation)
        self.assertIn("requested graph object exists but is not a PCG graph", implementation)

    def test_graph_edges_are_verified_without_boolean_testing_add_labeled_edge(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertNotRegex(implementation, r"if\s*\(\s*!?\s*Graph->AddLabeledEdge")
        self.assertIn("VerifyDirectedEdge", implementation)
        self.assertIn('SetNumberField(TEXT("node_count")', implementation)
        self.assertIn('SetNumberField(TEXT("verified_edge_count")', implementation)

    def test_mutations_are_restricted_to_managed_graph_and_prototype_map_roots(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("/Game/GameXXK/Environment/TownPCG/VerticalSlice/", implementation)
        self.assertIn("/Game/GameXXK/Maps/Prototype/", implementation)
        self.assertIn("FLevelUtils::IsLevelLocked", implementation)
        self.assertIn("IFileManager::Get().IsReadOnly", implementation)
        self.assertIn("CurrentLevel->GetOutermost()->GetName()", implementation)
        self.assertIn("ActorToInspect->GetLevel() != CurrentLevel", implementation)

    def test_all_actor_mutators_share_the_same_prototype_context_validator(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("ValidatePrototypeMutationContext", implementation)
        self.assertEqual(
            implementation.count("ValidatePrototypeMutationContext(World, nullptr, Error)"),
            4,
        )
        self.assertEqual(
            implementation.count("ValidatePrototypeMutationContext(World, Volume, Error)"),
            3,
        )
        self.assertEqual(
            implementation.count("ValidatePrototypeMutationContext(World, SplineActor, Error)"),
            1,
        )

    def test_tagged_spline_helper_is_deterministic_transactional_and_component_safe(self):
        header = HEADER.read_text(encoding="utf-8")
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("const TArray<FVector>& WorldPoints", header)
        self.assertIn("const TArray<FName>& Tags", header)
        self.assertIn("Components/SplineComponent.h", implementation)
        self.assertIn("AActor::StaticClass()", implementation)
        self.assertIn("GetClass() != AActor::StaticClass()", implementation)
        self.assertIn("SplineComponents.Num() != 1", implementation)
        self.assertIn("NewObject<USplineComponent>", implementation)
        self.assertIn("AddInstanceComponent", implementation)
        self.assertIn("ClearSplinePoints(false)", implementation)
        self.assertIn("SetSplinePoints(WorldPoints, ESplineCoordinateSpace::World, false)", implementation)
        self.assertIn("SetClosedLoop(bClosedLoop, false)", implementation)
        self.assertIn("SetArrayField(TEXT(\"tags\")", implementation)
        self.assertIn("PrototypeOnlyTag", implementation)
        self.assertIn("ResolveManagedSplineSemanticTag", implementation)
        self.assertIn("existing spline actor is not owned by town PCG assembly", implementation)

    def test_existing_python_managed_actors_require_level_and_ownership_before_mutation(self):
        source = ASSEMBLY_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("_require_actor_in_current_level(actor, label)", source)
        self.assertIn("_require_existing_actor_tags", source)
        self.assertIn("TownPCG_FixedAnchor", source)
        self.assertIn("TownPCG_TreeDebug", source)
        self.assertIn("TownPCG_TreeMarker", source)
        self.assertIn("existing managed actor", source)


if __name__ == "__main__":
    unittest.main()
