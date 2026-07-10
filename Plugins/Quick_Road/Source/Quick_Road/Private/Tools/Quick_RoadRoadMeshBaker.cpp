// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadRoadMeshBaker.h"

#include "ProceduralMeshComponent.h"
#include "ProceduralMeshConversion.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "Quick_RoadRoadMeshBaker"

namespace Quick_RoadRoadMeshBakerLocals
{
	struct FRoadMeshChunkKey
	{
		int32 SplineIndex = 0;
		int32 PartIndex = 0;

		bool operator==(const FRoadMeshChunkKey& Other) const
		{
			return SplineIndex == Other.SplineIndex && PartIndex == Other.PartIndex;
		}
	};

	inline uint32 GetTypeHash(const FRoadMeshChunkKey& Key)
	{
		return HashCombine(::GetTypeHash(Key.SplineIndex), ::GetTypeHash(Key.PartIndex));
	}

	struct FRoadMeshChunkGeometry
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		TMap<int32, int32> GlobalVertexToLocal;
		UMaterialInterface* Material = nullptr;
	};

	struct FFlatProcMesh
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;
		UMaterialInterface* Material = nullptr;
	};

	static bool ExtractFlatProcMesh(UProceduralMeshComponent* ProcMesh, FFlatProcMesh& OutMesh)
	{
		if (!ProcMesh)
		{
			return false;
		}

		OutMesh = FFlatProcMesh();
		for (int32 SectionIndex = 0; SectionIndex < ProcMesh->GetNumSections(); ++SectionIndex)
		{
			const FProcMeshSection* Section = ProcMesh->GetProcMeshSection(SectionIndex);
			if (!Section || Section->ProcIndexBuffer.Num() < 3)
			{
				continue;
			}

			const int32 BaseVertex = OutMesh.Positions.Num();
			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				OutMesh.Positions.Add(Vertex.Position);
				OutMesh.Normals.Add(Vertex.Normal);
				OutMesh.UVs.Add(Vertex.UV0);
				OutMesh.Tangents.Add(Vertex.Tangent);
			}

			for (const int32 Index : Section->ProcIndexBuffer)
			{
				OutMesh.Triangles.Add(BaseVertex + Index);
			}

			if (!OutMesh.Material)
			{
				OutMesh.Material = ProcMesh->GetMaterial(SectionIndex);
			}
		}

		return OutMesh.Triangles.Num() >= 3;
	}

	static bool ExtractFlatMeshFromStaticMesh(UStaticMesh* StaticMesh, FFlatProcMesh& OutMesh)
	{
		if (!StaticMesh || !StaticMesh->GetRenderData() || StaticMesh->GetRenderData()->LODResources.Num() == 0)
		{
			return false;
		}

		OutMesh = FFlatProcMesh();
		const FStaticMeshLODResources& LODResources = StaticMesh->GetRenderData()->LODResources[0];
		const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
		const FStaticMeshVertexBuffer& VertexBuffer = LODResources.VertexBuffers.StaticMeshVertexBuffer;
		const uint32 NumVertices = PositionBuffer.GetNumVertices();

		OutMesh.Positions.Reserve(NumVertices);
		OutMesh.Normals.Reserve(NumVertices);
		OutMesh.UVs.Reserve(NumVertices);
		OutMesh.Tangents.Reserve(NumVertices);

		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			OutMesh.Positions.Add(FVector(PositionBuffer.VertexPosition(VertexIndex)));
			OutMesh.Normals.Add(FVector(VertexBuffer.VertexTangentZ(VertexIndex)));
			OutMesh.UVs.Add(FVector2D(VertexBuffer.GetVertexUV(VertexIndex, 0)));
			const FVector4f TangentX = VertexBuffer.VertexTangentX(VertexIndex);
			const bool bFlipTangentY = TangentX.W < 0.f;
			OutMesh.Tangents.Add(FProcMeshTangent(FVector(TangentX), bFlipTangentY));
		}

		for (const FStaticMeshSection& Section : LODResources.Sections)
		{
			const uint32 FirstIndex = Section.FirstIndex;
			const uint32 NumIndices = Section.NumTriangles * 3;
			for (uint32 IndexOffset = 0; IndexOffset < NumIndices; ++IndexOffset)
			{
				OutMesh.Triangles.Add(LODResources.IndexBuffer.GetIndex(FirstIndex + IndexOffset));
			}

			if (!OutMesh.Material &&
				StaticMesh->GetStaticMaterials().IsValidIndex(Section.MaterialIndex))
			{
				OutMesh.Material = StaticMesh->GetMaterial(Section.MaterialIndex);
			}
		}

		return OutMesh.Triangles.Num() >= 3;
	}

	static void FindClosestSplineDistance(
		const FVector& WorldPosition,
		const TArray<USplineComponent*>& Splines,
		int32& OutSplineIndex,
		float& OutDistanceAlongSplineCm)
	{
		OutSplineIndex = INDEX_NONE;
		OutDistanceAlongSplineCm = 0.f;
		float BestDistSq = TNumericLimits<float>::Max();

		for (int32 SplineIndex = 0; SplineIndex < Splines.Num(); ++SplineIndex)
		{
			USplineComponent* Spline = Splines[SplineIndex];
			if (!Spline)
			{
				continue;
			}

			const float InputKey = Spline->FindInputKeyClosestToWorldLocation(WorldPosition);
			const FVector ClosestLocation = Spline->GetLocationAtSplineInputKey(
				InputKey,
				ESplineCoordinateSpace::World);
			const float DistSq = FVector::DistSquared2D(WorldPosition, ClosestLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutSplineIndex = SplineIndex;
				OutDistanceAlongSplineCm = Spline->GetDistanceAlongSplineAtLocation(
					ClosestLocation,
					ESplineCoordinateSpace::World);
			}
		}
	}

	static int32 MapGlobalVertex(FRoadMeshChunkGeometry& Chunk, const FFlatProcMesh& FlatMesh, int32 GlobalIndex)
	{
		if (const int32* ExistingIndex = Chunk.GlobalVertexToLocal.Find(GlobalIndex))
		{
			return *ExistingIndex;
		}

		const int32 LocalIndex = Chunk.Positions.Num();
		Chunk.GlobalVertexToLocal.Add(GlobalIndex, LocalIndex);
		Chunk.Positions.Add(FlatMesh.Positions[GlobalIndex]);
		Chunk.Normals.Add(FlatMesh.Normals[GlobalIndex]);
		Chunk.UVs.Add(FlatMesh.UVs[GlobalIndex]);
		Chunk.Tangents.Add(FlatMesh.Tangents[GlobalIndex]);
		return LocalIndex;
	}

	static void AddTriangleToChunk(
		FRoadMeshChunkGeometry& Chunk,
		const FFlatProcMesh& FlatMesh,
		int32 Index0,
		int32 Index1,
		int32 Index2)
	{
		Chunk.Triangles.Add(MapGlobalVertex(Chunk, FlatMesh, Index0));
		Chunk.Triangles.Add(MapGlobalVertex(Chunk, FlatMesh, Index1));
		Chunk.Triangles.Add(MapGlobalVertex(Chunk, FlatMesh, Index2));
	}

	static UStaticMesh* FindOrCreateStaticMeshForBake(const FString& FullPackageName, const FString& AssetName)
	{
		UPackage* Package = FindObject<UPackage>(nullptr, *FullPackageName);
		if (!Package && FPackageName::DoesPackageExist(FullPackageName))
		{
			Package = LoadPackage(nullptr, *FullPackageName, LOAD_None);
		}
		if (!Package)
		{
			Package = CreatePackage(*FullPackageName);
		}
		if (!Package)
		{
			return nullptr;
		}

		Package->FullyLoad();

		UStaticMesh* StaticMesh = FindObject<UStaticMesh>(Package, *AssetName);
		if (StaticMesh)
		{
			StaticMesh->ConditionalPostLoad();
			StaticMesh->SetNumSourceModels(0);
			StaticMesh->GetStaticMaterials().Empty();
		}
		else
		{
			StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
		}

		if (StaticMesh)
		{
			StaticMesh->InitResources();
			StaticMesh->SetLightingGuid();
		}

		return StaticMesh;
	}

	static UStaticMesh* SaveStaticMeshPackage(UStaticMesh* StaticMesh, const FString& FullPackageName)
	{
		if (!StaticMesh)
		{
			return nullptr;
		}

		StaticMesh->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(StaticMesh);

		const FString PackageFileName = FPackageName::LongPackageNameToFilename(
			FullPackageName,
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(StaticMesh->GetOutermost(), StaticMesh, *PackageFileName, SaveArgs);
		return StaticMesh;
	}

	static UStaticMesh* ConvertFlatMeshToStaticMeshAsset(
		const FRoadMeshChunkGeometry& Chunk,
		const FString& PackagePath,
		const FString& AssetName)
	{
		if (Chunk.Triangles.Num() < 3)
		{
			return nullptr;
		}

		UProceduralMeshComponent* TempProcMesh = NewObject<UProceduralMeshComponent>(GetTransientPackage());
		if (!TempProcMesh)
		{
			return nullptr;
		}

		TempProcMesh->CreateMeshSection(
			0,
			Chunk.Positions,
			Chunk.Triangles,
			Chunk.Normals,
			Chunk.UVs,
			TArray<FColor>(),
			Chunk.Tangents,
			true);
		if (Chunk.Material)
		{
			TempProcMesh->SetMaterial(0, Chunk.Material);
		}

		return FQuick_RoadRoadMeshBaker::ConvertProcMeshToStaticMeshAsset(
			TempProcMesh,
			PackagePath,
			AssetName);
	}

	static int32 SplitFlatMeshByDistance(
		const FFlatProcMesh& FlatMesh,
		const FTransform& ComponentWorldTransform,
		const TArray<USplineComponent*>& Splines,
		float SplitDistanceCm,
		const FString& PackagePath,
		const FString& AssetNamePrefix,
		TArray<FQuick_RoadBakedMeshPlacement>& OutSplitPlacements)
	{
		if (SplitDistanceCm <= KINDA_SMALL_NUMBER || Splines.Num() == 0)
		{
			return 0;
		}

		TMap<FRoadMeshChunkKey, FRoadMeshChunkGeometry> ChunkMap;
		for (int32 TriangleStart = 0; TriangleStart + 2 < FlatMesh.Triangles.Num(); TriangleStart += 3)
		{
			const int32 Index0 = FlatMesh.Triangles[TriangleStart];
			const int32 Index1 = FlatMesh.Triangles[TriangleStart + 1];
			const int32 Index2 = FlatMesh.Triangles[TriangleStart + 2];
			if (!FlatMesh.Positions.IsValidIndex(Index0) ||
				!FlatMesh.Positions.IsValidIndex(Index1) ||
				!FlatMesh.Positions.IsValidIndex(Index2))
			{
				continue;
			}

			const FVector CentroidLocal =
				(FlatMesh.Positions[Index0] + FlatMesh.Positions[Index1] + FlatMesh.Positions[Index2]) / 3.f;
			const FVector CentroidWorld = ComponentWorldTransform.TransformPosition(CentroidLocal);

			int32 SplineIndex = INDEX_NONE;
			float DistanceAlongSplineCm = 0.f;
			FindClosestSplineDistance(CentroidWorld, Splines, SplineIndex, DistanceAlongSplineCm);
			if (SplineIndex == INDEX_NONE)
			{
				continue;
			}

			FRoadMeshChunkKey Key;
			Key.SplineIndex = SplineIndex;
			Key.PartIndex = FMath::Max(0, FMath::FloorToInt(DistanceAlongSplineCm / SplitDistanceCm));

			FRoadMeshChunkGeometry& Chunk = ChunkMap.FindOrAdd(Key);
			if (!Chunk.Material)
			{
				Chunk.Material = FlatMesh.Material;
			}

			AddTriangleToChunk(Chunk, FlatMesh, Index0, Index1, Index2);
		}

		TArray<FRoadMeshChunkKey> SortedKeys;
		ChunkMap.GetKeys(SortedKeys);
		SortedKeys.Sort([](const FRoadMeshChunkKey& A, const FRoadMeshChunkKey& B)
		{
			if (A.SplineIndex != B.SplineIndex)
			{
				return A.SplineIndex < B.SplineIndex;
			}
			return A.PartIndex < B.PartIndex;
		});

		int32 BakedCount = 0;
		for (const FRoadMeshChunkKey& Key : SortedKeys)
		{
			const FRoadMeshChunkGeometry& Chunk = ChunkMap.FindChecked(Key);
			const FString AssetName = FString::Printf(
				TEXT("%s_Spline%02d_Part%02d"),
				*AssetNamePrefix,
				Key.SplineIndex,
				Key.PartIndex);

			if (UStaticMesh* StaticMesh = ConvertFlatMeshToStaticMeshAsset(Chunk, PackagePath, AssetName))
			{
				FQuick_RoadBakedMeshPlacement Placement;
				Placement.StaticMesh = StaticMesh;
				Placement.RelativeTransform = FTransform::Identity;
				Placement.ComponentName = AssetName;
				OutSplitPlacements.Add(Placement);
				++BakedCount;
			}
		}

		return BakedCount;
	}
}

UStaticMesh* FQuick_RoadRoadMeshBaker::ConvertProcMeshToStaticMeshAsset(
	UProceduralMeshComponent* ProcMesh,
	const FString& PackagePath,
	const FString& AssetName)
{
	using namespace Quick_RoadRoadMeshBakerLocals;

	if (!ProcMesh || ProcMesh->GetNumSections() == 0)
	{
		return nullptr;
	}

	FMeshDescription MeshDescription = BuildMeshDescription(ProcMesh);
	if (MeshDescription.Polygons().Num() == 0)
	{
		return nullptr;
	}

	const FString FullPackageName = PackagePath / AssetName;
	if (!FPackageName::IsValidLongPackageName(FullPackageName))
	{
		return nullptr;
	}

	UStaticMesh* StaticMesh = FindOrCreateStaticMeshForBake(FullPackageName, AssetName);
	if (!StaticMesh)
	{
		return nullptr;
	}

	FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
	SourceModel.BuildSettings.bRecomputeNormals = false;
	SourceModel.BuildSettings.bRecomputeTangents = false;
	SourceModel.BuildSettings.bRemoveDegenerates = false;
	SourceModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
	SourceModel.BuildSettings.bUseFullPrecisionUVs = false;
	SourceModel.BuildSettings.bGenerateLightmapUVs = true;
	SourceModel.BuildSettings.SrcLightmapIndex = 0;
	SourceModel.BuildSettings.DstLightmapIndex = 1;

	StaticMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
	StaticMesh->CommitMeshDescription(0);

	if (!ProcMesh->bUseComplexAsSimpleCollision)
	{
		if (UBodySetup* ProcBodySetup = ProcMesh->GetBodySetup())
		{
			StaticMesh->CreateBodySetup();
			if (UBodySetup* StaticBodySetup = StaticMesh->GetBodySetup())
			{
				StaticBodySetup->BodySetupGuid = FGuid::NewGuid();
				StaticBodySetup->AggGeom.ConvexElems = ProcBodySetup->AggGeom.ConvexElems;
				StaticBodySetup->bGenerateMirroredCollision = false;
				StaticBodySetup->bDoubleSidedGeometry = true;
				StaticBodySetup->CollisionTraceFlag = CTF_UseDefault;
				StaticBodySetup->CreatePhysicsMeshes();
			}
		}
	}

	TSet<UMaterialInterface*> UniqueMaterials;
	for (int32 SectionIndex = 0; SectionIndex < ProcMesh->GetNumSections(); ++SectionIndex)
	{
		UniqueMaterials.Add(ProcMesh->GetMaterial(SectionIndex));
	}
	for (UMaterialInterface* Material : UniqueMaterials)
	{
		StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
	}

#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
	StaticMesh->SetImportVersion(EImportStaticMeshVersion::LastVersion);
#endif
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();

	return SaveStaticMeshPackage(StaticMesh, FullPackageName);
}

int32 FQuick_RoadRoadMeshBaker::SplitStaticMeshAssetByDistance(
	UStaticMesh* RoadStaticMesh,
	const FTransform& ComponentWorldTransform,
	const TArray<USplineComponent*>& Splines,
	float SplitDistanceCm,
	const FString& PackagePath,
	const FString& AssetNamePrefix,
	TArray<FQuick_RoadBakedMeshPlacement>& OutSplitPlacements)
{
	using namespace Quick_RoadRoadMeshBakerLocals;

	OutSplitPlacements.Reset();
	if (!RoadStaticMesh || SplitDistanceCm <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	FFlatProcMesh FlatMesh;
	if (!ExtractFlatMeshFromStaticMesh(RoadStaticMesh, FlatMesh))
	{
		return 0;
	}

	return SplitFlatMeshByDistance(
		FlatMesh,
		ComponentWorldTransform,
		Splines,
		SplitDistanceCm,
		PackagePath,
		AssetNamePrefix,
		OutSplitPlacements);
}

AActor* FQuick_RoadRoadMeshBaker::CreateBlueprintAssemblyAndSpawn(
	UWorld* World,
	const FString& PackagePath,
	const FString& BlueprintAssetName,
	const FTransform& WorldTransform,
	const TArray<FQuick_RoadBakedMeshPlacement>& Placements)
{
	if (!World || Placements.Num() == 0 || BlueprintAssetName.IsEmpty())
	{
		return nullptr;
	}

	const FString BlueprintPackageName = PackagePath / BlueprintAssetName;
	if (!FPackageName::IsValidLongPackageName(BlueprintPackageName))
	{
		return nullptr;
	}

	FActorSpawnParameters TempSpawnParams;
	TempSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TempSpawnParams.ObjectFlags = RF_Transactional;

	AActor* TempActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		WorldTransform.GetLocation(),
		WorldTransform.Rotator(),
		TempSpawnParams);
	if (!TempActor)
	{
		return nullptr;
	}

	TempActor->Modify();
	TempActor->SetActorTransform(WorldTransform);

	USceneComponent* RootComponent = NewObject<USceneComponent>(
		TempActor,
		USceneComponent::StaticClass(),
		TEXT("Root"),
		RF_Transactional);
	RootComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	RootComponent->SetMobility(EComponentMobility::Static);
	TempActor->SetRootComponent(RootComponent);
	TempActor->AddInstanceComponent(RootComponent);
	RootComponent->RegisterComponent();

	for (const FQuick_RoadBakedMeshPlacement& Placement : Placements)
	{
		if (!Placement.StaticMesh)
		{
			continue;
		}

		const FName ComponentName = FName(*Placement.ComponentName);
		UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(
			TempActor,
			UStaticMeshComponent::StaticClass(),
			ComponentName,
			RF_Transactional);
		MeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		MeshComponent->SetMobility(EComponentMobility::Static);
		MeshComponent->SetupAttachment(RootComponent);
		MeshComponent->SetStaticMesh(Placement.StaticMesh);
		MeshComponent->SetRelativeTransform(Placement.RelativeTransform);
		TempActor->AddInstanceComponent(MeshComponent);
		MeshComponent->RegisterComponent();
	}

	FKismetEditorUtilities::FCreateBlueprintFromActorParams BlueprintParams;
	BlueprintParams.bReplaceActor = false;
	BlueprintParams.bKeepMobility = true;
	BlueprintParams.bOpenBlueprint = false;

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(
		BlueprintPackageName,
		TempActor,
		BlueprintParams);

	World->DestroyActor(TempActor);

	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		Blueprint->GeneratedClass,
		WorldTransform.GetLocation(),
		WorldTransform.Rotator(),
		SpawnParams);
	if (SpawnedActor)
	{
		SpawnedActor->Modify();
		SpawnedActor->SetActorLabel(BlueprintAssetName);
		SpawnedActor->SetActorTransform(WorldTransform);
	}

	return SpawnedActor;
}

#undef LOCTEXT_NAMESPACE
