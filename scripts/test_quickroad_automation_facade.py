import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FACADE_HEADER = ROOT / "Plugins/Quick_Road/Source/Quick_Road/Public/Quick_RoadAutomationLibrary.h"
FACADE_SOURCE = ROOT / "Plugins/Quick_Road/Source/Quick_Road/Private/Quick_RoadAutomationLibrary.cpp"
BUILD_RULES = ROOT / "Plugins/Quick_Road/Source/Quick_Road/Quick_Road.Build.cs"
PCG_HEADER = ROOT / "Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h"
PCG_SOURCE = ROOT / "Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp"

REQUIRED_API = {
    "ResetRoadInfrastructure",
    "EnsureLandscapeInfrastructure",
    "GenerateRoadNetwork",
    "RebuildRoadIntersections",
    "ClearAndApplyAllRoadInfluence",
    "ExtractRoadEdges",
    "BakeSingleRoadNetwork",
    "AuditInfrastructure",
}


def _function_body(source: str, qualified_name: str) -> str:
    if qualified_name not in source:
        return ""
    start = source.index(qualified_name)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError(f"unclosed function body: {qualified_name}")


class QuickRoadFacadeSourceContractTests(unittest.TestCase):
    def setUp(self):
        self.header = FACADE_HEADER.read_text(encoding="utf-8") if FACADE_HEADER.exists() else ""
        self.source = FACADE_SOURCE.read_text(encoding="utf-8") if FACADE_SOURCE.exists() else ""

    def test_declares_exactly_the_eight_callable_operations(self):
        declared = set(re.findall(r"static FString (\w+)\s*\(", self.header))
        self.assertEqual(declared, REQUIRED_API)
        self.assertIn("UQuick_RoadAutomationLibrary", self.header)
        self.assertIn("UBlueprintFunctionLibrary", self.header)

    def test_every_operation_is_guarded_to_the_exact_b1_map(self):
        exact_map = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
        self.assertIn(f'TEXT("{exact_map}")', self.source)
        for api in REQUIRED_API:
            with self.subTest(api=api):
                body = _function_body(
                    self.source,
                    f"UQuick_RoadAutomationLibrary::{api}",
                )
                self.assertIn("ValidateB1MutationContext", body)
        self.assertIn("WorldPackageName != B1MapPackage", self.source)
        self.assertIn("CurrentLevelPackageName != B1MapPackage", self.source)

    def test_landscape_creation_uses_ue58_layers_safe_rollback_and_grid_center(self):
        required = (
            '#include "LandscapeEditLayer.h"',
            '#include "LandscapeEditTypes.h"',
            "FQuick_RoadLandscapeCreator::ComputeGridStats",
            "FQuick_RoadLandscapeCreator::TryCreateLandscape",
            "SnapshotLandscapes",
            "DestroyNewLandscapes",
            "NewLandscapes.Num() != 1",
            "GetEditLayerConst(EditLayerName)",
            "CreateLayer(EditLayerName)",
            "IsLocked()",
            "SetEditingLayer(RoadLayer->GetGuid())",
            "RotationScale.TransformVector(HalfGrid)",
            "DesiredGridCenterWorld -",
            "ReconstructedCenter",
            "ReimportHeightmapFilePath",
        )
        for token in required:
            with self.subTest(token=token):
                self.assertIn(token, self.source)
        self.assertNotIn("SetCanHaveLayersContent", self.source)
        self.assertNotIn("CanHaveLayersContent", self.source)
        body = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::EnsureLandscapeInfrastructure",
        )
        self.assertIn("FPaths::IsRelative(AbsoluteHeightmapPath)", body)
        self.assertLess(
            body.index("FPaths::IsRelative(AbsoluteHeightmapPath)"),
            body.index("FPaths::ConvertRelativePathToFull"),
        )

    def test_road_generation_rejects_unowned_networks_and_reapplies_width_material(self):
        body = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::GenerateRoadNetwork",
        )
        for token in (
            "RejectUnownedMatchingNetworks",
            "UQuick_RoadRoadSplineWidthComponent::SetHalfWidthCm",
            "FQuick_RoadLayoutRoadGenerator::GenerateRoadMeshes",
            "Results.Num() != 1",
            "OwnerTag",
            "NetworkCategoryTag",
            "ApplyRoadMaterial",
        ):
            with self.subTest(token=token):
                self.assertIn(token, body)
        self.assertNotIn("HasStoredHalfWidthCm", body)
        self.assertIn("WidthGroupTag", body)
        self.assertIn("ResolveHalfWidthCm(Spline, HalfWidthCm)", body)

    def test_intersection_rebuild_preserves_full_ribbon_contract(self):
        body = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::RebuildRoadIntersections",
        )
        for token in (
            "SplitAndRebuildConsolidatedRoadNetworks",
            "RibbonParams.SampleDistanceCm",
            "RibbonParams.WidthSubdivisions",
            "RibbonParams.bAdaptiveCurvature",
            "RibbonParams.CurvatureThresholdDeg",
            "RibbonParams.MaxCurvatureSubdivisions",
            "IntersectionGridCellSizeCm",
            "IntersectionCornerRadiusScale",
            "ApplyRoadMaterial",
            "QS_B1_QR_RoadNetwork",
        ):
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_additive_road_layer_reads_combined_base_and_writes_only_delta(self):
        body = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::ClearAndApplyAllRoadInfluence",
        )
        required = (
            "ClearEditLayer(",
            "ELandscapeToolTargetTypeFlags::Heightmap",
            "ForceHeightMerge",
            "FLandscapeEditDataInterface BaseLandscapeEdit(LandscapeInfo, FGuid()",
            "FQuick_RoadRoadMeshInfluenceField",
            "MeshField.Query",
            "DesiredFinalHeights",
            "BaseLocalHeights",
            "DesiredFinalHeights[Index] - BaseLocalHeights[Index]",
            "LandscapeDataAccess::GetTexHeight(DeltaLocal)",
            "FLandscapeEditDataInterface DeltaLandscapeEdit(LandscapeInfo, RoadLayer->GetGuid()",
            "SetHeightData(",
            "Query.Weight < 1.0f",
            "4.0f * Query.Weight * (1.0f - Query.Weight)",
        )
        for token in required:
            with self.subTest(token=token):
                self.assertIn(token, body)
        self.assertNotIn("ApplyRoadInfluenceDualFromProcMeshes", body)
        self.assertNotIn("SmoothRoadCorridorDualFromProcMeshes", body)
        self.assertIn("refusing clipped additive deltas", body)
        self.assertLess(
            body.index("refusing clipped additive deltas"),
            body.index("FLandscapeEditDataInterface DeltaLandscapeEdit"),
        )
        self.assertIn("FScopedRoadLayerDeltaRollback", body)
        self.assertIn("DeltaRollback.Commit()", body)
        self.assertLess(body.index("PreviousRawDelta"), body.index("ClearEditLayer("))
        merge = _function_body(self.source, "bool ForceHeightMerge")
        self.assertIn("RequestLayersContentUpdateForceAll", merge)
        self.assertIn("ForceUpdateLayersContent", merge)
        self.assertIn("IsUpToDate", merge)

    def test_reset_edges_bake_and_audit_are_owned_deterministic_and_selection_safe(self):
        self.assertIn("FScopedActorSelectionRestore", self.source)
        reset = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::ResetRoadInfrastructure",
        )
        for category in ("NetworkCategoryTag", "EdgeCategoryTag", "BakeCategoryTag"):
            self.assertIn(category, reset)
        for literal in (
            "QingshanB1QuickRoadOwned",
            "QingshanB1QuickRoadNetwork",
            "QingshanB1QuickRoadEdge",
            "QingshanB1QuickRoadBake",
        ):
            self.assertIn(literal, self.source)
        edge = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::ExtractRoadEdges",
        )
        for token in (
            "ExtractRoadEdgeSplines",
            "NewEdges.Num() != 1",
            "QS_B1_QR_Edge_000",
            "CanonicalSplineDigest",
            "ExtractAllRoadBoundaryPolylines",
            "AddBoundarySplineComponents",
            "OldOwnedEdges",
        ):
            self.assertIn(token, edge)
        self.assertNotIn("PluginSplineCount != 1", edge)
        self.assertLess(edge.index("ExtractRoadEdgeSplines"), edge.index("OldOwnedEdges[0]->Modify"))
        self.assertLess(edge.index("CandidateDigest"), edge.index("OldOwnedEdges[0]->Modify"))
        self.assertNotIn("return ErrorJson", edge[edge.index("EdgeActor->SetActorLabel"):])
        self.assertIn("WeldedVertexBuckets", self.source)
        self.assertIn("FVector::DistSquared", self.source)
        bake = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::BakeSingleRoadNetwork",
        )
        self.assertIn("BakeRoadMeshesToStaticMeshes", bake)
        self.assertIn("QingshanB1QuickRoadBake", bake)
        self.assertIn("/*bDeleteSourceProcMesh=*/false", bake)
        self.assertIn("if (bDeleteSourceProcMesh)", bake)
        self.assertLess(bake.index("NewActors.Num() != 1"), bake.index("World->DestroyActor(Network)"))
        audit = _function_body(
            self.source,
            "UQuick_RoadAutomationLibrary::AuditInfrastructure",
        )
        self.assertIn("ReadRawEditLayerHeights", audit)
        self.assertIn("non_neutral_sample_count", audit)
        self.assertIn("edit_layer_delta_sha256", audit)

    def test_facade_returns_structured_json_and_build_links_json(self):
        rules = BUILD_RULES.read_text(encoding="utf-8")
        self.assertRegex(rules, r'"Json"')
        self.assertIn('#include "Serialization/JsonSerializer.h"', self.source)
        self.assertIn('#include "Serialization/JsonWriter.h"', self.source)
        self.assertIn("TCondensedJsonPrintPolicy", self.source)


class B1PCGAdvancedContractTests(unittest.TestCase):
    def setUp(self):
        self.header = PCG_HEADER.read_text(encoding="utf-8")
        self.source = PCG_SOURCE.read_text(encoding="utf-8")

    def test_adds_unique_advanced_apis_without_changing_legacy_signatures(self):
        self.assertEqual(self.header.count("CreateOrUpdateTownPCGGraph("), 1)
        self.assertEqual(self.header.count("AttachTownPCGGraph("), 1)
        self.assertEqual(self.header.count("CreateOrUpdateTownPCGGraphAdvanced("), 1)
        self.assertEqual(self.header.count("AttachTownPCGGraphAdvanced("), 1)
        for token in (
            "int32 BaseSeed",
            "const TArray<FString>& MaterialOverridePaths",
            "const TArray<FString>& ConsumedRoadEdgeActorLabels",
            "const FString& ConsumedRoadEdgeGeometryDigest",
            "float MinimumRoadClearanceCm",
            "int32 ComponentSeed",
        ):
            self.assertIn(token, self.header)

    def test_exact_b1_root_is_a_third_managed_graph_root(self):
        self.assertIn(
            'B1GraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/B1/")',
            self.source,
        )
        helper = _function_body(self.source, "bool IsManagedGraphPath")
        self.assertIn("VerticalSliceGraphRoot", helper)
        self.assertIn("B0RGraphRoot", helper)
        self.assertIn("B1GraphRoot", helper)
        self.assertNotIn('TEXT("/Game/GameXXK/Environment/TownPCG/")', helper)
        create = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTownPCGGraphAdvanced",
        )
        attach = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::AttachTownPCGGraphAdvanced",
        )
        self.assertIn("IsB1GraphPath", create)
        self.assertIn("IsB1GraphPath", attach)
        self.assertIn("ValidateB1MutationContext", attach)
        self.assertIn("L_Qingshan_PCG_Dress_B1", self.source)

    def test_advanced_graph_persists_stable_seed_edge_and_material_contract(self):
        body = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTownPCGGraphAdvanced",
        )
        required = (
            "ConsumedRoadEdgeActorLabels",
            "Sort",
            "geometry digest must be 64 lowercase hexadecimal characters",
            "MinimumRoadClearanceCm",
            "StableTransformHash",
            "PCGHelpers::ComputeSeed(BaseSeed",
            "Entry.Descriptor.OverrideMaterials",
            "Graph->Description",
            "point_seed_sha256",
            "road_edge_geometry_digest",
            "minimum_road_clearance_cm",
            "PointRecords.Sort",
            "Entry.Descriptor.ComputeHash",
            '"schema_version"',
            '"static_mesh_path"',
            '"point_count"',
        )
        for token in required:
            with self.subTest(token=token):
                self.assertIn(token, body)
        self.assertNotIn("MaterialOverrides_DEPRECATED", body)
        self.assertNotIn("bOverrideMaterials_DEPRECATED", body)
        self.assertIn("at least one point transform is required", body)
        self.assertIn("duplicate canonical point transform", body)
        self.assertLess(
            body.index("duplicate canonical point transform"),
            body.index("FRevertibleEditorTransaction"),
        )

    def test_advanced_attach_sets_component_seed_and_status_reports_it(self):
        attach = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::AttachTownPCGGraphAdvanced",
        )
        self.assertIn("PCGComponent->Seed = ComponentSeed", attach)
        self.assertIn("PCGComponent->SetGraph(Graph)", attach)
        self.assertNotIn("AttachTownPCGGraph(ActorLabel", attach)
        status = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::GetTownPCGStatus",
        )
        self.assertIn('"component_seed"', status)

    def test_legacy_point_seed_and_graph_behavior_remain_present(self):
        create = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::CreateOrUpdateTownPCGGraph(",
        )
        self.assertIn("Index + 1", create)
        self.assertNotIn("Graph->Description", create)
        attach = _function_body(
            self.source,
            "UGameXXKTownPCGAutomationLibrary::AttachTownPCGGraph(",
        )
        self.assertNotIn("PCGComponent->Seed", attach)


if __name__ == "__main__":
    unittest.main()
