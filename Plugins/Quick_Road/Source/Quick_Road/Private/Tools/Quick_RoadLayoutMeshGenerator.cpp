// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLayoutMeshGenerator.h"
#include "Quick_RoadLayoutMeshTags.h"
#include "Quick_RoadLayoutSplineTags.h"
#include "Quick_RoadLayoutMeshComponent.h"
#include "Quick_RoadLayoutLandscapeConformComponent.h"
#include "Quick_RoadLog.h"

#include "Components/SplineComponent.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPath.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLayoutMeshGenerator"

namespace Quick_RoadLayoutMeshLocals
{
	struct FGridNeighborOffset
	{
		int32 DX;
		int32 DY;
		float DistanceScale;
	};

	static constexpr FGridNeighborOffset NeighborOffsets8[] = {
		{1, 0, 1.f},
		{-1, 0, 1.f},
		{0, 1, 1.f},
		{0, -1, 1.f},
		{1, 1, UE_SQRT_2},
		{1, -1, UE_SQRT_2},
		{-1, 1, UE_SQRT_2},
		{-1, -1, UE_SQRT_2},
	};

	static int32 GridIndex(int32 NumX, int32 GridX, int32 GridY)
	{
		return GridY * NumX + GridX;
	}

	static bool IsActiveCell(const TArray<uint8>& ActiveMask, int32 NumX, int32 NumY, int32 GridX, int32 GridY)
	{
		if (GridX < 0 || GridY < 0 || GridX >= NumX || GridY >= NumY)
		{
			return false;
		}

		return ActiveMask[GridIndex(NumX, GridX, GridY)] != 0;
	}

	static bool IsPointInPolygon2D(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return false;
		}

		bool bInside = false;
		const int32 Count = Polygon.Num();
		for (int32 Index = 0, PrevIndex = Count - 1; Index < Count; PrevIndex = Index++)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[PrevIndex];
			const bool bCrossesScanline = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (!bCrossesScanline)
			{
				continue;
			}

			const float IntersectX = (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y + KINDA_SMALL_NUMBER) + A.X;
			if (Point.X < IntersectX)
			{
				bInside = !bInside;
			}
		}

		return bInside;
	}

	static void BuildBoundaryPolygon2D(USplineComponent* Spline, int32 SampleCount, TArray<FVector2D>& OutPolygon)
	{
		OutPolygon.Reset();
		if (!Spline || SampleCount < 3)
		{
			return;
		}

		const float SplineLength = Spline->GetSplineLength();
		if (SplineLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		OutPolygon.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const float Distance = (SampleCount > 1)
				? (SplineLength * static_cast<float>(Index) / static_cast<float>(SampleCount))
				: 0.f;
			const FVector WorldLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
			OutPolygon.Add(FVector2D(WorldLocation.X, WorldLocation.Y));
		}
	}

	static bool SampleGroundZ(UWorld* World, const FVector2D& XY, float& OutZ)
	{
		if (!World)
		{
			return false;
		}

		const FVector TraceStart(XY.X, XY.Y, 1000000.f);
		const FVector TraceEnd(XY.X, XY.Y, -1000000.f);
		FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::AllObjects);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(QuickRoadLayoutMeshSample), false);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutMeshTags::CityScopeLayout))
			{
				QueryParams.AddIgnoredActor(Actor);
			}
		}

		FHitResult HitResult;
		if (World->LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectParams, QueryParams))
		{
			OutZ = HitResult.ImpactPoint.Z;
			return true;
		}

		return false;
	}

	static void BuildInitialHeights(
		const TArray<float>& GroundHeights,
		const TArray<uint8>& ActiveMask,
		float TerrainBlendStrength,
		TArray<float>& OutInitialHeights)
	{
		double SumZ = 0.0;
		int32 ActiveCount = 0;
		for (int32 Index = 0; Index < GroundHeights.Num(); ++Index)
		{
			if (!ActiveMask[Index])
			{
				continue;
			}

			SumZ += GroundHeights[Index];
			++ActiveCount;
		}

		const float AverageZ = (ActiveCount > 0) ? static_cast<float>(SumZ / static_cast<double>(ActiveCount)) : 0.f;
		const float TerrainFollow = FMath::Clamp(TerrainBlendStrength, 0.f, 1.f);
		const float FlattenFactor = 1.f - TerrainFollow;

		OutInitialHeights = GroundHeights;
		for (int32 Index = 0; Index < OutInitialHeights.Num(); ++Index)
		{
			if (!ActiveMask[Index])
			{
				continue;
			}

			// 仅当「贴合强度」较低时才向区域平均高度靠拢；默认应保留地形起伏。
			OutInitialHeights[Index] = FMath::Lerp(GroundHeights[Index], AverageZ, FlattenFactor);
		}
	}

	static void BoxBlurActiveHeights(
		TArray<float>& Heights,
		const TArray<uint8>& ActiveMask,
		int32 NumX,
		int32 NumY,
		int32 Passes)
	{
		TArray<float> Temp = Heights;
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			Temp = Heights;
			for (int32 GridY = 0; GridY < NumY; ++GridY)
			{
				for (int32 GridX = 0; GridX < NumX; ++GridX)
				{
					const int32 Index = GridIndex(NumX, GridX, GridY);
					if (!ActiveMask[Index])
					{
						continue;
					}

					double Sum = 0.0;
					int32 Count = 0;
					for (const FGridNeighborOffset& Offset : NeighborOffsets8)
					{
						const int32 NX = GridX + Offset.DX;
						const int32 NY = GridY + Offset.DY;
						if (!IsActiveCell(ActiveMask, NumX, NumY, NX, NY))
						{
							continue;
						}

						Sum += Temp[GridIndex(NumX, NX, NY)];
						++Count;
					}

					if (Count > 0)
					{
						Heights[Index] = static_cast<float>(Sum / static_cast<double>(Count));
					}
				}
			}
		}
	}

	static void ProjectEdgeHeightPair(float& HeightA, float& HeightB, float MaxDelta)
	{
		const float Delta = HeightA - HeightB;
		const float AbsDelta = FMath::Abs(Delta);
		if (AbsDelta <= MaxDelta)
		{
			return;
		}

		const float Excess = AbsDelta - MaxDelta;
		const float Correction = Excess * 0.5f;
		if (Delta > 0.f)
		{
			HeightA -= Correction;
			HeightB += Correction;
		}
		else
		{
			HeightA += Correction;
			HeightB -= Correction;
		}
	}

	static void EnforceGridSlopeConstraints(
		TArray<float>& Heights,
		const TArray<uint8>& ActiveMask,
		int32 NumX,
		int32 NumY,
		float CellSize,
		float MaxDeltaZPerCm,
		float& OutMaxChange)
	{
		OutMaxChange = 0.f;
		for (int32 GridY = 0; GridY < NumY; ++GridY)
		{
			for (int32 GridX = 0; GridX < NumX; ++GridX)
			{
				const int32 IndexA = GridIndex(NumX, GridX, GridY);
				if (!ActiveMask[IndexA])
				{
					continue;
				}

				for (const FGridNeighborOffset& Offset : NeighborOffsets8)
				{
					const int32 NX = GridX + Offset.DX;
					const int32 NY = GridY + Offset.DY;
					if (!IsActiveCell(ActiveMask, NumX, NumY, NX, NY))
					{
						continue;
					}

					// 仅处理每个无向边一次（正方向邻居）。
					if (Offset.DX < 0 || (Offset.DX == 0 && Offset.DY < 0))
					{
						continue;
					}

					const int32 IndexB = GridIndex(NumX, NX, NY);
					const float MaxDelta = CellSize * Offset.DistanceScale * MaxDeltaZPerCm;
					const float OldA = Heights[IndexA];
					const float OldB = Heights[IndexB];
					float NewA = OldA;
					float NewB = OldB;
					ProjectEdgeHeightPair(NewA, NewB, MaxDelta);
					Heights[IndexA] = NewA;
					Heights[IndexB] = NewB;
					OutMaxChange = FMath::Max(OutMaxChange, FMath::Abs(NewA - OldA));
					OutMaxChange = FMath::Max(OutMaxChange, FMath::Abs(NewB - OldB));
				}
			}
		}
	}

	static int32 SmoothHeightsWithSlopeConstraint(
		TArray<float>& Heights,
		const TArray<float>& GroundHeights,
		const TArray<float>& TargetHeights,
		const TArray<uint8>& ActiveMask,
		int32 NumX,
		int32 NumY,
		float CellSize,
		float MaxDeltaZPerCm,
		float TerrainBlendStrength,
		int32 MaxIterations,
		float ConvergenceThresholdCm)
	{
		const float TerrainFollow = FMath::Clamp(TerrainBlendStrength, 0.f, 1.f);
		const float FlattenFactor = 1.f - TerrainFollow;
		const float LaplacianStrength = FlattenFactor * 0.2f;
		const float TargetPullStrength = FlattenFactor * 0.06f;
		const int32 SlopeSubPasses = 6;
		int32 IterationsUsed = 0;

		for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
		{
			IterationsUsed = Iteration + 1;

			if (LaplacianStrength > KINDA_SMALL_NUMBER || TargetPullStrength > KINDA_SMALL_NUMBER)
			{
				TArray<float> NextHeights = Heights;
				for (int32 GridY = 0; GridY < NumY; ++GridY)
				{
					for (int32 GridX = 0; GridX < NumX; ++GridX)
					{
						const int32 Index = GridIndex(NumX, GridX, GridY);
						if (!ActiveMask[Index])
						{
							continue;
						}

						if (LaplacianStrength > KINDA_SMALL_NUMBER)
						{
							double NeighborSum = 0.0;
							int32 NeighborCount = 0;
							for (const FGridNeighborOffset& Offset : NeighborOffsets8)
							{
								const int32 NX = GridX + Offset.DX;
								const int32 NY = GridY + Offset.DY;
								if (!IsActiveCell(ActiveMask, NumX, NumY, NX, NY))
								{
									continue;
								}

								NeighborSum += Heights[GridIndex(NumX, NX, NY)];
								++NeighborCount;
							}

							if (NeighborCount > 0)
							{
								const float NeighborAverage = static_cast<float>(NeighborSum / static_cast<double>(NeighborCount));
								NextHeights[Index] = FMath::Lerp(Heights[Index], NeighborAverage, LaplacianStrength);
							}
						}

						if (TargetPullStrength > KINDA_SMALL_NUMBER)
						{
							NextHeights[Index] = FMath::Lerp(NextHeights[Index], TargetHeights[Index], TargetPullStrength);
						}
					}
				}

				Heights = MoveTemp(NextHeights);
			}

			float MaxSlopeChange = 0.f;
			for (int32 SubPass = 0; SubPass < SlopeSubPasses; ++SubPass)
			{
				float PassChange = 0.f;
				EnforceGridSlopeConstraints(Heights, ActiveMask, NumX, NumY, CellSize, MaxDeltaZPerCm, PassChange);
				MaxSlopeChange = FMath::Max(MaxSlopeChange, PassChange);
			}

			if (MaxSlopeChange <= ConvergenceThresholdCm)
			{
				break;
			}
		}

		// 在坡度允许范围内，轻微拉回真实地形采样以恢复起伏（仅当用户希望贴地时）。
		if (TerrainFollow > 0.5f)
		{
			const float GroundRestore = (TerrainFollow - 0.5f) * 0.6f;
			for (int32 Index = 0; Index < Heights.Num(); ++Index)
			{
				if (!ActiveMask[Index])
				{
					continue;
				}

				Heights[Index] = FMath::Lerp(Heights[Index], GroundHeights[Index], GroundRestore);
			}

			for (int32 FinalRestorePass = 0; FinalRestorePass < 8; ++FinalRestorePass)
			{
				float PassChange = 0.f;
				EnforceGridSlopeConstraints(Heights, ActiveMask, NumX, NumY, CellSize, MaxDeltaZPerCm, PassChange);
				if (PassChange <= ConvergenceThresholdCm)
				{
					break;
				}
			}
		}

		return IterationsUsed;
	}

	static void RemoveExistingCityScopeLayoutMeshes(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		TArray<AActor*> ActorsToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutMeshTags::CityScopeLayout))
			{
				ActorsToDestroy.Add(Actor);
			}
		}

		for (AActor* Actor : ActorsToDestroy)
		{
			Actor->Destroy();
		}
	}

	static float QuantizeTerraceWorldHeight(
		float WorldHeight,
		float MinHeight,
		float MaxHeight,
		float StepSizeCm,
		float Fade)
	{
		if (StepSizeCm <= KINDA_SMALL_NUMBER || WorldHeight < MinHeight || WorldHeight > MaxHeight)
		{
			return WorldHeight;
		}

		const float Normalized = (WorldHeight - MinHeight) / StepSizeCm;
		const float TerracedHeight = FMath::Floor(Normalized) * StepSizeCm + MinHeight;
		return FMath::Lerp(TerracedHeight, WorldHeight, FMath::Clamp(Fade, 0.f, 1.f));
	}

	static void BuildVertexAdjacency(
		int32 VertexCount,
		const TArray<int32>& Triangles,
		TArray<TArray<int32>>& OutAdjacency)
	{
		OutAdjacency.SetNum(VertexCount);
		for (int32 TriangleIndex = 0; TriangleIndex + 2 < Triangles.Num(); TriangleIndex += 3)
		{
			const int32 I0 = Triangles[TriangleIndex];
			const int32 I1 = Triangles[TriangleIndex + 1];
			const int32 I2 = Triangles[TriangleIndex + 2];
			if (!OutAdjacency.IsValidIndex(I0) || !OutAdjacency.IsValidIndex(I1) || !OutAdjacency.IsValidIndex(I2))
			{
				continue;
			}

			OutAdjacency[I0].AddUnique(I1);
			OutAdjacency[I0].AddUnique(I2);
			OutAdjacency[I1].AddUnique(I0);
			OutAdjacency[I1].AddUnique(I2);
			OutAdjacency[I2].AddUnique(I0);
			OutAdjacency[I2].AddUnique(I1);
		}
	}

	static void SmoothTerraceCliffEdges(
		TArray<float>& WorldHeights,
		const TArray<TArray<int32>>& Adjacency,
		float StepSizeCm,
		float SmoothStrength)
	{
		if (SmoothStrength <= KINDA_SMALL_NUMBER || StepSizeCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float CliffThreshold = StepSizeCm * 0.85f;
		TArray<float> NextHeights = WorldHeights;
		for (int32 VertexIndex = 0; VertexIndex < WorldHeights.Num(); ++VertexIndex)
		{
			if (!Adjacency.IsValidIndex(VertexIndex))
			{
				continue;
			}

			float MaxNeighborDiff = 0.f;
			double NeighborSum = 0.0;
			int32 NeighborCount = 0;
			for (const int32 NeighborIndex : Adjacency[VertexIndex])
			{
				if (!WorldHeights.IsValidIndex(NeighborIndex))
				{
					continue;
				}

				MaxNeighborDiff = FMath::Max(MaxNeighborDiff, FMath::Abs(WorldHeights[VertexIndex] - WorldHeights[NeighborIndex]));
				NeighborSum += WorldHeights[NeighborIndex];
				++NeighborCount;
			}

			if (NeighborCount > 0 && MaxNeighborDiff >= CliffThreshold)
			{
				const float NeighborAverage = static_cast<float>(NeighborSum / static_cast<double>(NeighborCount));
				NextHeights[VertexIndex] = FMath::Lerp(WorldHeights[VertexIndex], NeighborAverage, SmoothStrength);
			}
		}

		WorldHeights = MoveTemp(NextHeights);
	}

	static bool FindCityScopeLayoutMesh(
		UWorld* World,
		AActor*& OutActor,
		UQuick_RoadLayoutMeshComponent*& OutMeshComponent)
	{
		OutActor = nullptr;
		OutMeshComponent = nullptr;
		if (!World)
		{
			return false;
		}

		auto TryActor = [&](AActor* Actor) -> bool
		{
			if (!Actor || !Actor->ActorHasTag(Quick_RoadLayoutMeshTags::CityScopeLayout))
			{
				return false;
			}

			UQuick_RoadLayoutMeshComponent* MeshComponent = Actor->FindComponentByClass<UQuick_RoadLayoutMeshComponent>();
			if (!MeshComponent || MeshComponent->GetProcMeshSection(0) == nullptr)
			{
				return false;
			}

			OutActor = Actor;
			OutMeshComponent = MeshComponent;
			return true;
		};

#if WITH_EDITOR
		if (GEditor)
		{
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
			{
				if (TryActor(Cast<AActor>(*It)))
				{
					return true;
				}
			}
		}
#endif

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (TryActor(*It))
			{
				return true;
			}
		}

		return false;
	}

	static void ComputeNormals(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		TArray<FVector>& OutNormals)
	{
		OutNormals.Init(FVector::ZeroVector, Vertices.Num());
		for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); TriangleIndex += 3)
		{
			const int32 I0 = Triangles[TriangleIndex];
			const int32 I1 = Triangles[TriangleIndex + 1];
			const int32 I2 = Triangles[TriangleIndex + 2];
			const FVector Edge1 = Vertices[I1] - Vertices[I0];
			const FVector Edge2 = Vertices[I2] - Vertices[I0];
			const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2);
			if (!FaceNormal.IsNearlyZero())
			{
				OutNormals[I0] += FaceNormal;
				OutNormals[I1] += FaceNormal;
				OutNormals[I2] += FaceNormal;
			}
		}

		for (FVector& Normal : OutNormals)
		{
			Normal = Normal.GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				Normal = FVector::UpVector;
			}
		}
	}

	static UMaterialInterface* ResolveDefaultLayoutMeshMaterial(UObject* Outer)
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(
			FSoftObjectPath(Quick_RoadLayoutMeshTags::DefaultLayoutMeshMaterialPath).TryLoad()))
		{
			return Material;
		}

		static const TCHAR* FallbackMaterialPaths[] = {
			TEXT("/Game/StarterContent/Materials/M_CobbleStone_Pebblex.M_CobbleStone_Pebblex"),
			TEXT("/Game/StarterContent/Materials/M_CobbleStone_Smooth.M_CobbleStone_Smooth"),
		};

		for (const TCHAR* Path : FallbackMaterialPaths)
		{
			if (UMaterialInterface* Material = Cast<UMaterialInterface>(FSoftObjectPath(Path).TryLoad()))
			{
				UE_LOG(
					LogQuickRoad,
					Warning,
					TEXT("Quick_Road 布局面片：未找到 M_CobbleStone_Rough，回退材质 %s"),
					Path);
				return Material;
			}
		}

		UTexture2D* Texture = Cast<UTexture2D>(
			FSoftObjectPath(Quick_RoadLayoutMeshTags::DefaultLayoutMeshTexturePath).TryLoad());
		if (!Texture)
		{
			UE_LOG(
				LogQuickRoad,
				Warning,
				TEXT("Quick_Road 布局面片：未找到默认材质/贴图 %s，将使用引擎默认材质。"),
				Quick_RoadLayoutMeshTags::DefaultLayoutMeshTexturePath);
			return nullptr;
		}

		UMaterial* BaseMaterial = LoadObject<UMaterial>(
			nullptr,
			TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
		if (!BaseMaterial || !Outer)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
		if (DynamicMaterial)
		{
			DynamicMaterial->SetTextureParameterValue(TEXT("Texture"), Texture);
			DynamicMaterial->SetTextureParameterValue(TEXT("BaseColor"), Texture);
		}

		return DynamicMaterial;
	}

	static UMaterialInterface* ResolveLayoutMeshMaterial(UObject* Outer, UMaterialInterface* OverrideMaterial = nullptr)
	{
		if (OverrideMaterial)
		{
			return OverrideMaterial;
		}

		return ResolveDefaultLayoutMeshMaterial(Outer);
	}

	static USplineComponent* GetCityScopeSplineFromActor(AActor* Actor)
	{
		if (!Actor || !Actor->ActorHasTag(Quick_RoadLayoutSplineTags::CityScope))
		{
			return nullptr;
		}

		return Actor->FindComponentByClass<USplineComponent>();
	}

	static bool TryGetSelectedCityScopeSpline(USplineComponent*& OutSpline)
	{
		OutSpline = nullptr;
#if WITH_EDITOR
		if (!GEditor)
		{
			return false;
		}

		for (FSelectionIterator It(GEditor->GetSelectedComponentIterator()); It; ++It)
		{
			USplineComponent* SplineComponent = Cast<USplineComponent>(*It);
			if (!SplineComponent)
			{
				continue;
			}

			AActor* Owner = SplineComponent->GetOwner();
			if (!Owner || !Owner->ActorHasTag(Quick_RoadLayoutSplineTags::CityScope))
			{
				continue;
			}

			OutSpline = SplineComponent;
			return true;
		}

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (USplineComponent* SplineComponent = GetCityScopeSplineFromActor(Cast<AActor>(*It)))
			{
				OutSpline = SplineComponent;
				return true;
			}
		}
#endif
		return false;
	}
}

bool FQuick_RoadLayoutMeshGenerator::FindCityScopeSpline(
	UWorld* World,
	USplineComponent*& OutSpline,
	FText& OutErrorMessage)
{
	OutSpline = nullptr;
	if (!World)
	{
		OutErrorMessage = LOCTEXT("NoWorld", "没有有效的关卡世界。");
		return false;
	}

	if (Quick_RoadLayoutMeshLocals::TryGetSelectedCityScopeSpline(OutSpline))
	{
		if (!OutSpline->IsClosedLoop())
		{
			OutErrorMessage = LOCTEXT("SplineNotClosed", "区域样条线未闭合，无法生成布局面片。");
			OutSpline = nullptr;
			return false;
		}

		return true;
	}

	TArray<USplineComponent*> FoundSplines;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (USplineComponent* SplineComponent = Quick_RoadLayoutMeshLocals::GetCityScopeSplineFromActor(*It))
		{
			FoundSplines.Add(SplineComponent);
		}
	}

	if (FoundSplines.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("NoCityScopeSpline", "未找到区域样条线。请先使用「绘制区域」，或在视口中选中一条区域样线。");
		return false;
	}

	if (FoundSplines.Num() > 1)
	{
		OutErrorMessage = LOCTEXT("MultipleCityScopeSplines", "存在多条区域样条线，请先在视口中选中要生成面片的一条。");
		return false;
	}

	OutSpline = FoundSplines[0];
	if (!OutSpline->IsClosedLoop())
	{
		OutErrorMessage = LOCTEXT("SplineNotClosed", "区域样条线未闭合，无法生成布局面片。");
		OutSpline = nullptr;
		return false;
	}

	return true;
}

bool FQuick_RoadLayoutMeshGenerator::FindCityScopeLayoutMesh(
	UWorld* World,
	AActor*& OutActor,
	UQuick_RoadLayoutMeshComponent*& OutMeshComponent)
{
	return Quick_RoadLayoutMeshLocals::FindCityScopeLayoutMesh(World, OutActor, OutMeshComponent);
}

bool FQuick_RoadLayoutMeshGenerator::GenerateFromCityScope(
	UWorld* World,
	USplineComponent* Spline,
	const FQuick_RoadLayoutMeshParams& Params,
	FQuick_RoadLayoutMeshResult& OutResult,
	FText& OutErrorMessage)
{
	OutResult = FQuick_RoadLayoutMeshResult();
	if (!World || !Spline)
	{
		OutErrorMessage = LOCTEXT("InvalidGenerateInput", "生成布局面片的输入无效。");
		return false;
	}

	const int32 Subdivisions = FMath::Max(Params.Subdivisions, 1);
	const int32 BoundarySamples = FMath::Max(Subdivisions * 4, 12);

	TArray<FVector2D> BoundaryPolygon;
	Quick_RoadLayoutMeshLocals::BuildBoundaryPolygon2D(Spline, BoundarySamples, BoundaryPolygon);
	if (BoundaryPolygon.Num() < 3)
	{
		OutErrorMessage = LOCTEXT("InvalidBoundary", "无法从样条线提取有效边界。");
		return false;
	}

	FVector2D BoundsMin(BoundaryPolygon[0]);
	FVector2D BoundsMax(BoundaryPolygon[0]);
	for (const FVector2D& Point : BoundaryPolygon)
	{
		BoundsMin.X = FMath::Min(BoundsMin.X, Point.X);
		BoundsMin.Y = FMath::Min(BoundsMin.Y, Point.Y);
		BoundsMax.X = FMath::Max(BoundsMax.X, Point.X);
		BoundsMax.Y = FMath::Max(BoundsMax.Y, Point.Y);
	}

	const float BoundsWidth = BoundsMax.X - BoundsMin.X;
	const float BoundsHeight = BoundsMax.Y - BoundsMin.Y;
	if (BoundsWidth <= KINDA_SMALL_NUMBER || BoundsHeight <= KINDA_SMALL_NUMBER)
	{
		OutErrorMessage = LOCTEXT("DegenerateBounds", "城市范围边界过小，无法生成网格。");
		return false;
	}

	const float MaxExtent = FMath::Max(BoundsWidth, BoundsHeight);
	const float CellSize = MaxExtent / static_cast<float>(Subdivisions);
	const int32 NumX = FMath::Max(2, FMath::CeilToInt(BoundsWidth / CellSize) + 1);
	const int32 NumY = FMath::Max(2, FMath::CeilToInt(BoundsHeight / CellSize) + 1);

	TArray<uint8> ActiveMask;
	TArray<float> GroundHeights;
	ActiveMask.Init(0, NumX * NumY);
	GroundHeights.Init(0.f, NumX * NumY);

	int32 ActiveVertexCount = 0;
	for (int32 GridY = 0; GridY < NumY; ++GridY)
	{
		for (int32 GridX = 0; GridX < NumX; ++GridX)
		{
			const int32 Index = Quick_RoadLayoutMeshLocals::GridIndex(NumX, GridX, GridY);
			const FVector2D GridPoint(
				BoundsMin.X + static_cast<float>(GridX) * CellSize,
				BoundsMin.Y + static_cast<float>(GridY) * CellSize);

			if (!Quick_RoadLayoutMeshLocals::IsPointInPolygon2D(GridPoint, BoundaryPolygon))
			{
				continue;
			}

			float GroundZ = 0.f;
			if (!Quick_RoadLayoutMeshLocals::SampleGroundZ(World, GridPoint, GroundZ))
			{
				OutErrorMessage = LOCTEXT("HeightSampleFailed", "无法采样地面高度，请确认场景中存在 Landscape 或可碰撞地面。");
				return false;
			}

			ActiveMask[Index] = 1;
			GroundHeights[Index] = GroundZ;
			++ActiveVertexCount;
		}
	}

	if (ActiveVertexCount < 3)
	{
		OutErrorMessage = LOCTEXT("NotEnoughVertices", "布局面片有效顶点过少，请增大城市范围或提高细分。");
		return false;
	}

	TArray<float> TargetHeights;
	Quick_RoadLayoutMeshLocals::BuildInitialHeights(
		GroundHeights,
		ActiveMask,
		Params.TerrainBlendStrength,
		TargetHeights);

	const float TerrainFollow = FMath::Clamp(Params.TerrainBlendStrength, 0.f, 1.f);
	const float FlattenFactor = 1.f - TerrainFollow;
	const int32 BlurPasses = FMath::Clamp(FMath::RoundToInt(FlattenFactor * 3.f), 0, 3);
	if (BlurPasses > 0)
	{
		Quick_RoadLayoutMeshLocals::BoxBlurActiveHeights(TargetHeights, ActiveMask, NumX, NumY, BlurPasses);
	}

	TArray<float> Heights = GroundHeights;
	const float MaxDeltaZPerCm = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Params.MaxSlopeAngleDegrees, 0.f, 89.9f)));
	const int32 RelaxIterationsUsed = Quick_RoadLayoutMeshLocals::SmoothHeightsWithSlopeConstraint(
		Heights,
		GroundHeights,
		TargetHeights,
		ActiveMask,
		NumX,
		NumY,
		CellSize,
		MaxDeltaZPerCm,
		Params.TerrainBlendStrength,
		FMath::Max(Params.MaxSlopeRelaxIterations, 1),
		FMath::Max(Params.SlopeConvergenceThresholdCm, 0.01f));

	// 最终坡度投影，确保网格边（含对角）满足约束。
	for (int32 FinalPass = 0; FinalPass < 12; ++FinalPass)
	{
		float PassChange = 0.f;
		Quick_RoadLayoutMeshLocals::EnforceGridSlopeConstraints(
			Heights, ActiveMask, NumX, NumY, CellSize, MaxDeltaZPerCm, PassChange);
		if (PassChange <= Params.SlopeConvergenceThresholdCm)
		{
			break;
		}
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	Vertices.Reserve(ActiveVertexCount);
	UVs.Reserve(ActiveVertexCount);

	TArray<int32> GridToCompactIndex;
	GridToCompactIndex.Init(INDEX_NONE, NumX * NumY);

	for (int32 GridY = 0; GridY < NumY; ++GridY)
	{
		for (int32 GridX = 0; GridX < NumX; ++GridX)
		{
			const int32 Index = Quick_RoadLayoutMeshLocals::GridIndex(NumX, GridX, GridY);
			if (!ActiveMask[Index])
			{
				continue;
			}

			GridToCompactIndex[Index] = Vertices.Num();
			const FVector2D GridPoint(
				BoundsMin.X + static_cast<float>(GridX) * CellSize,
				BoundsMin.Y + static_cast<float>(GridY) * CellSize);
			Vertices.Add(FVector(GridPoint.X, GridPoint.Y, Heights[Index]));
			UVs.Add(FVector2D(
				(GridPoint.X - BoundsMin.X) / BoundsWidth,
				(GridPoint.Y - BoundsMin.Y) / BoundsHeight));
		}
	}

	const auto GetCornerIndex = [&](int32 GridX, int32 GridY) -> int32
	{
		if (GridX < 0 || GridY < 0 || GridX >= NumX || GridY >= NumY)
		{
			return INDEX_NONE;
		}

		const int32 GridIndex = Quick_RoadLayoutMeshLocals::GridIndex(NumX, GridX, GridY);
		return GridToCompactIndex[GridIndex];
	};

	for (int32 GridY = 0; GridY < NumY - 1; ++GridY)
	{
		for (int32 GridX = 0; GridX < NumX - 1; ++GridX)
		{
			const FVector2D CellCenter(
				BoundsMin.X + (static_cast<float>(GridX) + 0.5f) * CellSize,
				BoundsMin.Y + (static_cast<float>(GridY) + 0.5f) * CellSize);
			if (!Quick_RoadLayoutMeshLocals::IsPointInPolygon2D(CellCenter, BoundaryPolygon))
			{
				continue;
			}

			const int32 I00 = GetCornerIndex(GridX, GridY);
			const int32 I10 = GetCornerIndex(GridX + 1, GridY);
			const int32 I01 = GetCornerIndex(GridX, GridY + 1);
			const int32 I11 = GetCornerIndex(GridX + 1, GridY + 1);
			if (I00 == INDEX_NONE || I10 == INDEX_NONE || I01 == INDEX_NONE || I11 == INDEX_NONE)
			{
				continue;
			}

			Triangles.Add(I00);
			Triangles.Add(I01);
			Triangles.Add(I11);
			Triangles.Add(I00);
			Triangles.Add(I11);
			Triangles.Add(I10);
		}
	}

	if (Triangles.Num() < 3)
	{
		OutErrorMessage = LOCTEXT("NoTriangles", "未能生成有效三角面，请调整细分或城市范围形状。");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateCityScopeLayoutMesh", "Create City Scope Layout Mesh"));
	Quick_RoadLayoutMeshLocals::RemoveExistingCityScopeLayoutMeshes(World);

	const FVector MeshPivot = FVector(BoundsMin.X, BoundsMin.Y, 0.f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;

	AActor* LayoutActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		MeshPivot,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!LayoutActor)
	{
		OutErrorMessage = LOCTEXT("SpawnFailed", "无法创建布局面片 Actor。");
		return false;
	}

	LayoutActor->Modify();
	LayoutActor->SetActorLabel(TEXT("CityScopeLayoutMesh"));
	LayoutActor->Tags.AddUnique(Quick_RoadLayoutMeshTags::LayoutMesh);
	LayoutActor->Tags.AddUnique(Quick_RoadLayoutMeshTags::CityScopeLayout);

	UQuick_RoadLayoutMeshComponent* MeshComponent = NewObject<UQuick_RoadLayoutMeshComponent>(
		LayoutActor,
		UQuick_RoadLayoutMeshComponent::StaticClass(),
		TEXT("LayoutMesh"),
		RF_Transactional);
	MeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	MeshComponent->SetMobility(EComponentMobility::Static);
	MeshComponent->SetRelativeTransform(FTransform::Identity);

	LayoutActor->SetRootComponent(MeshComponent);
	LayoutActor->AddInstanceComponent(MeshComponent);
	MeshComponent->RegisterComponent();
	LayoutActor->SetActorLocation(MeshPivot, false, nullptr, ETeleportType::TeleportPhysics);

	TArray<FVector> LocalVertices;
	LocalVertices.Reserve(Vertices.Num());
	for (const FVector& Vertex : Vertices)
	{
		LocalVertices.Add(Vertex - MeshPivot);
	}

	TArray<FVector> Normals;
	Quick_RoadLayoutMeshLocals::ComputeNormals(LocalVertices, Triangles, Normals);

	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);

	MeshComponent->Modify();
	MeshComponent->CreateMeshSection(
		0,
		LocalVertices,
		Triangles,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents,
		true);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (UMaterialInterface* LayoutMaterial = Quick_RoadLayoutMeshLocals::ResolveLayoutMeshMaterial(
		MeshComponent,
		Params.DefaultMaterial))
	{
		MeshComponent->SetMaterial(0, LayoutMaterial);
	}

	UQuick_RoadLayoutLandscapeConformComponent* ConformComponent = NewObject<UQuick_RoadLayoutLandscapeConformComponent>(
		LayoutActor,
		UQuick_RoadLayoutLandscapeConformComponent::StaticClass(),
		TEXT("GroundConform"),
		RF_Transactional);
	ConformComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	LayoutActor->AddInstanceComponent(ConformComponent);
	ConformComponent->RegisterComponent();

	OutResult.LayoutActor = LayoutActor;
	OutResult.VertexCount = Vertices.Num();
	OutResult.TriangleCount = Triangles.Num() / 3;

	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(LayoutActor, true, true, true);
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 布局面片：顶点 %d，三角面 %d，坡度 %.1f°，贴合 %.2f，松弛 %d 轮"),
		OutResult.VertexCount,
		OutResult.TriangleCount,
		Params.MaxSlopeAngleDegrees,
		Params.TerrainBlendStrength,
		RelaxIterationsUsed);

	return true;
}

bool FQuick_RoadLayoutMeshGenerator::ApplyTerraceToLayoutMesh(
	UWorld* World,
	const FQuick_RoadLayoutTerraceParams& Params,
	FQuick_RoadLayoutMeshResult& OutResult,
	FText& OutErrorMessage)
{
	OutResult = FQuick_RoadLayoutMeshResult();
	if (!World)
	{
		OutErrorMessage = LOCTEXT("TerraceInvalidWorld", "当前世界无效。");
		return false;
	}

	AActor* LayoutActor = nullptr;
	UQuick_RoadLayoutMeshComponent* MeshComponent = nullptr;
	if (!Quick_RoadLayoutMeshLocals::FindCityScopeLayoutMesh(World, LayoutActor, MeshComponent))
	{
		OutErrorMessage = LOCTEXT("TerraceNoLayoutMesh", "未找到布局面片。请先生成布局面片，或在视口中选中 CityScopeLayoutMesh。");
		return false;
	}

	FProcMeshSection* Section = MeshComponent->GetProcMeshSection(0);
	if (!Section || Section->ProcVertexBuffer.Num() < 3 || Section->ProcIndexBuffer.Num() < 3)
	{
		OutErrorMessage = LOCTEXT("TerraceNoMeshData", "布局面片没有有效的网格数据。");
		return false;
	}

	const float StepSizeCm = FMath::Max(Params.StepSizeCm, 10.f);
	const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
	const FTransform WorldToComponent = ComponentToWorld.Inverse();

	TArray<FVector> LocalVertices;
	TArray<FVector2D> UVs;
	LocalVertices.Reserve(Section->ProcVertexBuffer.Num());
	UVs.Reserve(Section->ProcVertexBuffer.Num());

	TArray<float> WorldHeights;
	WorldHeights.Reserve(Section->ProcVertexBuffer.Num());

	float AutoMinHeight = TNumericLimits<float>::Max();
	float AutoMaxHeight = TNumericLimits<float>::Lowest();
	for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
	{
		const FVector WorldPosition = ComponentToWorld.TransformPosition(Vertex.Position);
		AutoMinHeight = FMath::Min(AutoMinHeight, WorldPosition.Z);
		AutoMaxHeight = FMath::Max(AutoMaxHeight, WorldPosition.Z);
		LocalVertices.Add(Vertex.Position);
		UVs.Add(Vertex.UV0);
		WorldHeights.Add(WorldPosition.Z);
	}

	float MinHeight = Params.MinHeightCm;
	float MaxHeight = Params.MaxHeightCm;
	if (Params.bAutoHeightRange)
	{
		MinHeight = AutoMinHeight;
		MaxHeight = AutoMaxHeight;
	}
	else if (MaxHeight < MinHeight)
	{
		Swap(MinHeight, MaxHeight);
	}

	if (MaxHeight - MinHeight <= KINDA_SMALL_NUMBER)
	{
		OutErrorMessage = LOCTEXT("TerraceFlatRange", "阶梯化高度范围过小，请调整最低/最高高度或重新生成布局面片。");
		return false;
	}

	for (float& WorldHeight : WorldHeights)
	{
		WorldHeight = Quick_RoadLayoutMeshLocals::QuantizeTerraceWorldHeight(
			WorldHeight,
			MinHeight,
			MaxHeight,
			StepSizeCm,
			Params.Fade);
	}

	if (Params.SmoothEdges > KINDA_SMALL_NUMBER)
	{
		TArray<int32> Triangles;
		Triangles.Reserve(Section->ProcIndexBuffer.Num());
		for (uint32 Index : Section->ProcIndexBuffer)
		{
			Triangles.Add(static_cast<int32>(Index));
		}

		TArray<TArray<int32>> Adjacency;
		Quick_RoadLayoutMeshLocals::BuildVertexAdjacency(LocalVertices.Num(), Triangles, Adjacency);
		const int32 SmoothPasses = FMath::Clamp(FMath::RoundToInt(Params.SmoothEdges * 4.f), 1, 4);
		for (int32 Pass = 0; Pass < SmoothPasses; ++Pass)
		{
			Quick_RoadLayoutMeshLocals::SmoothTerraceCliffEdges(WorldHeights, Adjacency, StepSizeCm, Params.SmoothEdges);
		}
	}

	for (int32 VertexIndex = 0; VertexIndex < LocalVertices.Num(); ++VertexIndex)
	{
		FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
		WorldPosition.Z = WorldHeights[VertexIndex];
		LocalVertices[VertexIndex] = WorldToComponent.TransformPosition(WorldPosition);
	}

	TArray<int32> Triangles;
	Triangles.Reserve(Section->ProcIndexBuffer.Num());
	for (uint32 Index : Section->ProcIndexBuffer)
	{
		Triangles.Add(static_cast<int32>(Index));
	}

	TArray<FVector> Normals;
	Quick_RoadLayoutMeshLocals::ComputeNormals(LocalVertices, Triangles, Normals);

	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);

	const FScopedTransaction Transaction(LOCTEXT("TerraceCityScopeLayoutMesh", "Terrace City Scope Layout Mesh"));
	LayoutActor->Modify();
	MeshComponent->Modify();
	MeshComponent->UpdateMeshSection(
		0,
		LocalVertices,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents);

	OutResult.LayoutActor = LayoutActor;
	OutResult.VertexCount = LocalVertices.Num();
	OutResult.TriangleCount = Triangles.Num() / 3;

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(LayoutActor, true, true, true);
	}
#endif

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 布局面片阶梯化：顶点 %d，台阶 %.1f cm，高度范围 [%.1f, %.1f]"),
		OutResult.VertexCount,
		StepSizeCm,
		MinHeight,
		MaxHeight);

	return true;
}

#undef LOCTEXT_NAMESPACE