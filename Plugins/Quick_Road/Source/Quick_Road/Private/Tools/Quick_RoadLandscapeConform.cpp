// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLandscapeConform.h"
#include "Tools/Quick_RoadLandscapeHeightEdit.h"
#include "Tools/Quick_RoadLayoutMeshGenerator.h"
#include "Quick_RoadLog.h"

#include "Quick_RoadLandscapeCompat.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeDataAccess.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"

#include "ScopedTransaction.h"
#include "Containers/Set.h"
#include "Containers/Queue.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLandscapeConform"

namespace Quick_RoadConformLocals
{
	static constexpr float BottomFaceNormalThreshold = 0.5f;
	static constexpr float TopFaceNormalThreshold = 0.5f;

	struct FSurfaceTriangle
	{
		FVector A;
		FVector B;
		FVector C;
	};

	FVector LandscapeVertexToWorldXY(ALandscapeProxy* Landscape, int32 VertX, int32 VertY)
	{
		const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
		return LandscapeToWorld.TransformPosition(FVector(static_cast<float>(VertX), static_cast<float>(VertY), 0.f));
	}

	float WorldZToLocalHeight(ALandscapeProxy* Landscape, float WorldZ, float WorldX, float WorldY)
	{
		const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
		const FVector LocalPosition = LandscapeToWorld.InverseTransformPosition(FVector(WorldX, WorldY, WorldZ));
		return LocalPosition.Z;
	}

	bool ComputeTriangleBarycentric2D(
		const FVector2D& Point,
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		FVector& OutBarycentric)
	{
		const FVector2D V0 = B - A;
		const FVector2D V1 = C - A;
		const FVector2D V2 = Point - A;
		const float Dot00 = FVector2D::DotProduct(V0, V0);
		const float Dot01 = FVector2D::DotProduct(V0, V1);
		const float Dot11 = FVector2D::DotProduct(V1, V1);
		const float Dot20 = FVector2D::DotProduct(V2, V0);
		const float Dot21 = FVector2D::DotProduct(V2, V1);
		const float Denominator = Dot00 * Dot11 - Dot01 * Dot01;
		if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float InvDen = 1.f / Denominator;
		const float V = (Dot11 * Dot20 - Dot01 * Dot21) * InvDen;
		const float W = (Dot00 * Dot21 - Dot01 * Dot20) * InvDen;
		const float U = 1.f - V - W;
		const float Epsilon = -1e-4f;
		if (U < Epsilon || V < Epsilon || W < Epsilon)
		{
			return false;
		}

		OutBarycentric = FVector(U, V, W);
		return true;
	}

	void AppendFallbackBottomPlane(const FBox& Bounds, TArray<FSurfaceTriangle>& OutTriangles)
	{
		if (!Bounds.IsValid)
		{
			return;
		}

		const float Z = Bounds.Min.Z;
		const FVector Corners[4] = {
			FVector(Bounds.Min.X, Bounds.Min.Y, Z),
			FVector(Bounds.Max.X, Bounds.Min.Y, Z),
			FVector(Bounds.Max.X, Bounds.Max.Y, Z),
			FVector(Bounds.Min.X, Bounds.Max.Y, Z)
		};

		OutTriangles.Add({ Corners[0], Corners[1], Corners[2] });
		OutTriangles.Add({ Corners[0], Corners[2], Corners[3] });
	}

	bool GatherBottomTrianglesFromStaticMesh(
		const UStaticMeshComponent* MeshComponent,
		TArray<FSurfaceTriangle>& OutTriangles)
	{
		if (!MeshComponent || !MeshComponent->GetStaticMesh())
		{
			return false;
		}

		const FStaticMeshRenderData* RenderData = MeshComponent->GetStaticMesh()->GetRenderData();
		if (!RenderData || RenderData->LODResources.Num() == 0)
		{
			return false;
		}

		const FStaticMeshLODResources& LODResources = RenderData->LODResources[0];
		const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
		const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
		const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
		bool bAddedAny = false;

		for (const FStaticMeshSection& Section : LODResources.Sections)
		{
			if (Section.NumTriangles == 0)
			{
				continue;
			}

			for (uint32 TriIndex = 0; TriIndex < Section.NumTriangles; ++TriIndex)
			{
				const uint32 IndexOffset = Section.FirstIndex + TriIndex * 3;
				const uint32 I0 = IndexBuffer.GetIndex(IndexOffset + 0);
				const uint32 I1 = IndexBuffer.GetIndex(IndexOffset + 1);
				const uint32 I2 = IndexBuffer.GetIndex(IndexOffset + 2);

				const FVector LocalA(PositionBuffer.VertexPosition(I0));
				const FVector LocalB(PositionBuffer.VertexPosition(I1));
				const FVector LocalC(PositionBuffer.VertexPosition(I2));
				const FVector WorldA = ComponentToWorld.TransformPosition(LocalA);
				const FVector WorldB = ComponentToWorld.TransformPosition(LocalB);
				const FVector WorldC = ComponentToWorld.TransformPosition(LocalC);

				const FVector EdgeAB = WorldB - WorldA;
				const FVector EdgeAC = WorldC - WorldA;
				const FVector WorldNormal = FVector::CrossProduct(EdgeAB, EdgeAC).GetSafeNormal();
				if (FVector::DotProduct(WorldNormal, FVector::DownVector) < BottomFaceNormalThreshold)
				{
					continue;
				}

				OutTriangles.Add({ WorldA, WorldB, WorldC });
				bAddedAny = true;
			}
		}

		return bAddedAny;
	}

	bool GatherTopTrianglesFromProceduralMesh(
		const UProceduralMeshComponent* MeshComponent,
		TArray<FSurfaceTriangle>& OutTriangles)
	{
		if (!MeshComponent)
		{
			return false;
		}

		const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
		bool bAddedAny = false;

		for (int32 SectionIndex = 0; SectionIndex < MeshComponent->GetNumSections(); ++SectionIndex)
		{
			const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(MeshComponent)->GetProcMeshSection(SectionIndex);
			if (!Section || Section->ProcIndexBuffer.Num() < 3)
			{
				continue;
			}

			const TArray<FProcMeshVertex>& Vertices = Section->ProcVertexBuffer;
			const TArray<uint32>& Indices = Section->ProcIndexBuffer;
			for (int32 TriangleStart = 0; TriangleStart + 2 < Indices.Num(); TriangleStart += 3)
			{
				const uint32 I0 = Indices[TriangleStart];
				const uint32 I1 = Indices[TriangleStart + 1];
				const uint32 I2 = Indices[TriangleStart + 2];
				if (!Vertices.IsValidIndex(I0) || !Vertices.IsValidIndex(I1) || !Vertices.IsValidIndex(I2))
				{
					continue;
				}

				const FVector WorldA = ComponentToWorld.TransformPosition(Vertices[I0].Position);
				const FVector WorldB = ComponentToWorld.TransformPosition(Vertices[I1].Position);
				const FVector WorldC = ComponentToWorld.TransformPosition(Vertices[I2].Position);

				// 布局面片为纯顶面网格：三角 winding 可能与法线过滤不一致，优先用法线朝上的面，否则纳入全部有效三角。
				const FVector EdgeAB = WorldB - WorldA;
				const FVector EdgeAC = WorldC - WorldA;
				const float NormalUpDot = FVector::DotProduct(
					FVector::CrossProduct(EdgeAB, EdgeAC).GetSafeNormal(),
					FVector::UpVector);

				if (NormalUpDot >= TopFaceNormalThreshold)
				{
					OutTriangles.Add({ WorldA, WorldB, WorldC });
				}
				else if (NormalUpDot <= -TopFaceNormalThreshold)
				{
					OutTriangles.Add({ WorldA, WorldC, WorldB });
				}
				else
				{
					OutTriangles.Add({ WorldA, WorldB, WorldC });
				}

				bAddedAny = true;
			}
		}

		return bAddedAny;
	}

	bool GatherSurfaceTriangles(
		const TArray<UPrimitiveComponent*>& ConformComponents,
		EQuick_RoadConformSurfaceMode SurfaceMode,
		TArray<FSurfaceTriangle>& OutTriangles,
		FBox& OutBounds)
	{
		OutTriangles.Reset();
		OutBounds = FBox(ForceInit);

		for (UPrimitiveComponent* Component : ConformComponents)
		{
			if (!Component)
			{
				continue;
			}

			OutBounds += Component->Bounds.GetBox();

			if (SurfaceMode == EQuick_RoadConformSurfaceMode::Top)
			{
				if (const UProceduralMeshComponent* ProceduralMeshComponent = Cast<UProceduralMeshComponent>(Component))
				{
					GatherTopTrianglesFromProceduralMesh(ProceduralMeshComponent, OutTriangles);
				}
				continue;
			}

			if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				GatherBottomTrianglesFromStaticMesh(StaticMeshComponent, OutTriangles);
			}
		}

		if (OutTriangles.Num() == 0 && SurfaceMode == EQuick_RoadConformSurfaceMode::Bottom)
		{
			AppendFallbackBottomPlane(OutBounds, OutTriangles);
		}

		return OutTriangles.Num() > 0 && OutBounds.IsValid;
	}

	bool SampleSurfaceWorldZ(
		const TArray<FSurfaceTriangle>& SurfaceTriangles,
		float WorldX,
		float WorldY,
		bool bUseHighestZ,
		float& OutWorldZ)
	{
		const FVector2D QueryPoint(WorldX, WorldY);
		float BestZ = bUseHighestZ ? TNumericLimits<float>::Lowest() : TNumericLimits<float>::Max();
		bool bFound = false;

		for (const FSurfaceTriangle& Triangle : SurfaceTriangles)
		{
			FVector Barycentric;
			if (!ComputeTriangleBarycentric2D(
				QueryPoint,
				FVector2D(Triangle.A),
				FVector2D(Triangle.B),
				FVector2D(Triangle.C),
				Barycentric))
			{
				continue;
			}

			const float CandidateZ =
				Barycentric.X * Triangle.A.Z +
				Barycentric.Y * Triangle.B.Z +
				Barycentric.Z * Triangle.C.Z;

			if (bUseHighestZ)
			{
				if (CandidateZ > BestZ)
				{
					BestZ = CandidateZ;
					bFound = true;
				}
			}
			else if (CandidateZ < BestZ)
			{
				BestZ = CandidateZ;
				bFound = true;
			}
		}

		if (bFound)
		{
			OutWorldZ = BestZ;
		}
		return bFound;
	}

	bool SampleBottomWorldZ(
		const TArray<FSurfaceTriangle>& BottomTriangles,
		float WorldX,
		float WorldY,
		float& OutWorldZ)
	{
		return SampleSurfaceWorldZ(BottomTriangles, WorldX, WorldY, false, OutWorldZ);
	}

	void DilateMask(TArray<uint8>& Mask, int32 Width, int32 Height, int32 Radius)
	{
		if (Radius <= 0)
		{
			return;
		}

		TArray<uint8> SourceMask = Mask;
		for (int32 Pass = 0; Pass < Radius; ++Pass)
		{
			TArray<uint8> Expanded = SourceMask;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					if (SourceMask[Y * Width + X] != 0)
					{
						continue;
					}

					for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
					{
						for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
						{
							const int32 NeighborX = X + OffsetX;
							const int32 NeighborY = Y + OffsetY;
							if (NeighborX < 0 || NeighborY < 0 || NeighborX >= Width || NeighborY >= Height)
							{
								continue;
							}

							if (SourceMask[NeighborY * Width + NeighborX] != 0)
							{
								Expanded[Y * Width + X] = 1;
								break;
							}
						}
					}
				}
			}
			SourceMask = MoveTemp(Expanded);
		}
		Mask = MoveTemp(SourceMask);
	}

	void FillMissingTargetsFromNeighbors(
		TArray<float>& TargetHeights,
		const TArray<uint8>& Mask,
		int32 Width,
		int32 Height,
		int32 MaxPasses)
	{
		for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
		{
			TArray<float> Filled = TargetHeights;
			bool bAnyFilled = false;
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 Index = Y * Width + X;
					if (Mask[Index] == 0 || TargetHeights[Index] > TNumericLimits<float>::Lowest() + 1.f)
					{
						continue;
					}

					float Sum = 0.f;
					int32 Count = 0;
					for (int32 OffsetY = -2; OffsetY <= 2; ++OffsetY)
					{
						for (int32 OffsetX = -2; OffsetX <= 2; ++OffsetX)
						{
							const int32 NeighborX = X + OffsetX;
							const int32 NeighborY = Y + OffsetY;
							if (NeighborX < 0 || NeighborY < 0 || NeighborX >= Width || NeighborY >= Height)
							{
								continue;
							}

							const int32 NeighborIndex = NeighborY * Width + NeighborX;
							if (Mask[NeighborIndex] != 0 && TargetHeights[NeighborIndex] > TNumericLimits<float>::Lowest() + 1.f)
							{
								Sum += TargetHeights[NeighborIndex];
								++Count;
							}
						}
					}

					if (Count > 0)
					{
						Filled[Index] = Sum / static_cast<float>(Count);
						bAnyFilled = true;
					}
				}
			}

			TargetHeights = MoveTemp(Filled);
			if (!bAnyFilled)
			{
				break;
			}
		}
	}

	float GetLandscapeQuadSizeCm(ALandscape* Landscape)
	{
		const FVector LandscapeScale = Landscape->GetActorScale3D();
		return FMath::Max(FMath::Abs(LandscapeScale.X), FMath::Abs(LandscapeScale.Y));
	}

	int32 ComputeExpandVerts(ALandscape* Landscape, float EdgeOffsetCm)
	{
		const float QuadSizeCm = GetLandscapeQuadSizeCm(Landscape);
		if (EdgeOffsetCm <= 0.f || QuadSizeCm <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}

		return FMath::Max(0, FMath::CeilToInt32(EdgeOffsetCm / QuadSizeCm));
	}

	void ComputeDistanceFromMaskEdge(
		const TArray<uint8>& Mask,
		int32 Width,
		int32 Height,
		TArray<int32>& OutDistance)
	{
		const int32 PixelCount = Width * Height;
		OutDistance.Init(TNumericLimits<int32>::Max(), PixelCount);

		TQueue<TPair<int32, int32>> Queue;
		auto EnqueueIfMasked = [&](int32 X, int32 Y)
		{
			if (X < 0 || Y < 0 || X >= Width || Y >= Height)
			{
				return;
			}

			const int32 Index = Y * Width + X;
			if (Mask[Index] == 0 || OutDistance[Index] != TNumericLimits<int32>::Max())
			{
				return;
			}

			OutDistance[Index] = 0;
			Queue.Enqueue(TPair<int32, int32>(X, Y));
		};

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 Index = Y * Width + X;
				if (Mask[Index] == 0)
				{
					continue;
				}

				const bool bOnEdge =
					X == 0 || Y == 0 || X == Width - 1 || Y == Height - 1 ||
					Mask[Index - 1] == 0 || Mask[Index + 1] == 0 ||
					Mask[Index - Width] == 0 || Mask[Index + Width] == 0;
				if (bOnEdge)
				{
					EnqueueIfMasked(X, Y);
				}
			}
		}

		static const int32 NeighborOffsets[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
		while (!Queue.IsEmpty())
		{
			TPair<int32, int32> Current;
			Queue.Dequeue(Current);
			const int32 CurrentIndex = Current.Value * Width + Current.Key;
			const int32 NextDistance = OutDistance[CurrentIndex] + 1;

			for (const int32 (&Offset)[2] : NeighborOffsets)
			{
				const int32 NeighborX = Current.Key + Offset[0];
				const int32 NeighborY = Current.Value + Offset[1];
				if (NeighborX < 0 || NeighborY < 0 || NeighborX >= Width || NeighborY >= Height)
				{
					continue;
				}

				const int32 NeighborIndex = NeighborY * Width + NeighborX;
				if (Mask[NeighborIndex] == 0 || OutDistance[NeighborIndex] <= NextDistance)
				{
					continue;
				}

				OutDistance[NeighborIndex] = NextDistance;
				Queue.Enqueue(TPair<int32, int32>(NeighborX, NeighborY));
			}
		}
	}

	float ComputeEdgeFalloffAlpha(int32 DistanceFromEdge, int32 FalloffRadius)
	{
		if (FalloffRadius <= 0)
		{
			return 1.f;
		}

		if (DistanceFromEdge >= FalloffRadius)
		{
			return 1.f;
		}

		return FMath::SmoothStep(0.f, 1.f, static_cast<float>(DistanceFromEdge) / static_cast<float>(FalloffRadius));
	}

	void BuildBoundaryPolygonFromSpline(USplineComponent* Spline, int32 SampleCount, TArray<FVector2D>& OutPolygon)
	{
		OutPolygon.Reset();
		if (!Spline)
		{
			return;
		}

		const int32 NumSamples = FMath::Max(SampleCount, 3);
		const float SplineLength = Spline->GetSplineLength();
		if (SplineLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		OutPolygon.Reserve(NumSamples);
		for (int32 Index = 0; Index < NumSamples; ++Index)
		{
			const float Distance = (NumSamples > 1)
				? (SplineLength * static_cast<float>(Index) / static_cast<float>(NumSamples))
				: 0.f;
			const FVector WorldLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
			OutPolygon.Add(FVector2D(WorldLocation.X, WorldLocation.Y));
		}
	}

	float DistancePointToSegment2D(const FVector2D& Point, const FVector2D& SegmentStart, const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentLengthSq = Segment.SizeSquared();
		if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
		{
			return FVector2D::Distance(Point, SegmentStart);
		}

		const float Projection = FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSq;
		const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
		const FVector2D ClosestPoint = SegmentStart + Segment * ClampedProjection;
		return FVector2D::Distance(Point, ClosestPoint);
	}

	float DistancePointToPolygonBoundary2D(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 2)
		{
			return TNumericLimits<float>::Max();
		}

		float MinDistance = TNumericLimits<float>::Max();
		const int32 EdgeCount = Polygon.Num();
		for (int32 EdgeIndex = 0; EdgeIndex < EdgeCount; ++EdgeIndex)
		{
			const FVector2D& A = Polygon[EdgeIndex];
			const FVector2D& B = Polygon[(EdgeIndex + 1) % EdgeCount];
			MinDistance = FMath::Min(MinDistance, DistancePointToSegment2D(Point, A, B));
		}

		return MinDistance;
	}

	float ComputeBoundarySmoothWeight(float DistanceCm, float SmoothWidthCm, float SmoothStrength)
	{
		if (SmoothWidthCm <= KINDA_SMALL_NUMBER || SmoothStrength <= KINDA_SMALL_NUMBER)
		{
			return 0.f;
		}

		if (DistanceCm >= SmoothWidthCm)
		{
			return 0.f;
		}

		const float Normalized = 1.f - FMath::Clamp(DistanceCm / SmoothWidthCm, 0.f, 1.f);
		return SmoothStrength * FMath::SmoothStep(0.f, 1.f, Normalized);
	}

	void ComputeNeighborAverageHeight(
		const TArray<float>& Heights,
		int32 Width,
		int32 Height,
		int32 LocalX,
		int32 LocalY,
		float& OutAverage,
		int32& OutCount)
	{
		double Sum = 0.0;
		int32 Count = 0;
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				if (OffsetX == 0 && OffsetY == 0)
				{
					continue;
				}

				const int32 NeighborX = LocalX + OffsetX;
				const int32 NeighborY = LocalY + OffsetY;
				if (NeighborX < 0 || NeighborY < 0 || NeighborX >= Width || NeighborY >= Height)
				{
					continue;
				}

				Sum += Heights[NeighborY * Width + NeighborX];
				++Count;
			}
		}

		OutAverage = (Count > 0) ? static_cast<float>(Sum / static_cast<double>(Count)) : Heights[LocalY * Width + LocalX];
		OutCount = Count;
	}
}

bool FQuick_RoadLandscapeConform::ApplyConformToLandscape(
	ALandscape* Landscape,
	const TArray<UPrimitiveComponent*>& ConformComponents,
	const FQuick_RoadConformToMeshParams& Params,
	FText& OutErrorMessage,
	EQuick_RoadConformSurfaceMode SurfaceMode)
{
	if (!Landscape || ConformComponents.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("InvalidInput", "Landscape 或模型组件无效。");
		return false;
	}

	TArray<Quick_RoadConformLocals::FSurfaceTriangle> SurfaceTriangles;
	FBox MeshBounds(ForceInit);
	if (!Quick_RoadConformLocals::GatherSurfaceTriangles(ConformComponents, SurfaceMode, SurfaceTriangles, MeshBounds))
	{
		OutErrorMessage = (SurfaceMode == EQuick_RoadConformSurfaceMode::Top)
			? LOCTEXT("NoTopFaces", "无法识别布局面片顶面，请确认已生成程序化网格。")
			: LOCTEXT("NoBottomFaces", "无法识别模型底面，请确认模型包含 StaticMesh 组件。");
		return false;
	}

	const bool bUseHighestZ = (SurfaceMode == EQuick_RoadConformSurfaceMode::Top);

	FQuick_RoadLandscapeHeightBuffer Buffer;
	if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
	{
		return false;
	}

	const TArray<float> OriginalHeights = Buffer.FloatHeights;

	const FGuid EditLayerGuid = FQuick_RoadLandscapeHeightEdit::ResolveEditingLayerGuid(Landscape);
	const FString LayerDisplayName = QuickRoadLandscapeCompat::GetLayerDisplayName(Landscape, EditLayerGuid);
	if (!LayerDisplayName.IsEmpty())
	{
		UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 贴合模型：写入 Edit Layer「%s」"), *LayerDisplayName);
	}

	const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
	const FVector BoundsMinLocal = LandscapeToWorld.InverseTransformPosition(MeshBounds.Min);
	const FVector BoundsMaxLocal = LandscapeToWorld.InverseTransformPosition(MeshBounds.Max);
	const int32 Expand = Quick_RoadConformLocals::ComputeExpandVerts(Landscape, Params.EdgeOffsetCm);
	const float QuadSizeCm = Quick_RoadConformLocals::GetLandscapeQuadSizeCm(Landscape);
	const int32 MinVertX = FMath::FloorToInt(FMath::Min(BoundsMinLocal.X, BoundsMaxLocal.X)) - Expand;
	const int32 MaxVertX = FMath::CeilToInt(FMath::Max(BoundsMinLocal.X, BoundsMaxLocal.X)) + Expand;
	const int32 MinVertY = FMath::FloorToInt(FMath::Min(BoundsMinLocal.Y, BoundsMaxLocal.Y)) - Expand;
	const int32 MaxVertY = FMath::CeilToInt(FMath::Max(BoundsMinLocal.Y, BoundsMaxLocal.Y)) + Expand;

	const int32 Width = Buffer.Width;
	const int32 Height = Buffer.Height;
	TArray<float> TargetHeights;
	TArray<uint8> ConformMask;
	TargetHeights.Init(TNumericLimits<float>::Lowest(), Buffer.FloatHeights.Num());
	ConformMask.Init(0, Buffer.FloatHeights.Num());

	int32 HitCount = 0;
	float MinLocalHeight = TNumericLimits<float>::Max();
	float MaxLocalHeight = TNumericLimits<float>::Lowest();

	for (int32 VertY = MinVertY; VertY <= MaxVertY; ++VertY)
	{
		for (int32 VertX = MinVertX; VertX <= MaxVertX; ++VertX)
		{
			if (VertX < Buffer.MinX || VertY < Buffer.MinY || VertX > Buffer.MaxX || VertY > Buffer.MaxY)
			{
				continue;
			}

			const int32 LocalX = VertX - Buffer.MinX;
			const int32 LocalY = VertY - Buffer.MinY;
			const int32 Index = LocalY * Width + LocalX;
			const FVector WorldXY = Quick_RoadConformLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);

			float SurfaceWorldZ = 0.f;
			if (!Quick_RoadConformLocals::SampleSurfaceWorldZ(
				SurfaceTriangles, WorldXY.X, WorldXY.Y, bUseHighestZ, SurfaceWorldZ))
			{
				continue;
			}

			const float TargetWorldZ = SurfaceWorldZ + Params.VerticalOffset;
			const float LocalHeight = Quick_RoadConformLocals::WorldZToLocalHeight(
				Landscape, TargetWorldZ, WorldXY.X, WorldXY.Y);
			TargetHeights[Index] = LocalHeight;
			ConformMask[Index] = 1;
			MinLocalHeight = FMath::Min(MinLocalHeight, LocalHeight);
			MaxLocalHeight = FMath::Max(MaxLocalHeight, LocalHeight);
			++HitCount;
		}
	}

	if (HitCount == 0)
	{
		OutErrorMessage = (SurfaceMode == EQuick_RoadConformSurfaceMode::Top)
			? LOCTEXT("NoTopHits", "布局面片与 Landscape 范围无交集，请确认面片位于地形上方且 Edit Layer 可编辑。")
			: LOCTEXT(
				"NoHits",
				"模型底面与 Landscape 范围无交集，请确认模型位于地形上方且 Edit Layer 可编辑。");
		return false;
	}

	Quick_RoadConformLocals::DilateMask(ConformMask, Width, Height, Expand);
	Quick_RoadConformLocals::FillMissingTargetsFromNeighbors(
		TargetHeights, ConformMask, Width, Height, Expand + 4);

	const int32 FalloffRadius = FMath::Max(0, Params.Smoothness);
	TArray<int32> DistanceFromEdge;
	if (FalloffRadius > 0)
	{
		Quick_RoadConformLocals::ComputeDistanceFromMaskEdge(ConformMask, Width, Height, DistanceFromEdge);
	}

	const float Blend = Params.BlendStrength;
	for (int32 Index = 0; Index < Buffer.FloatHeights.Num(); ++Index)
	{
		if (ConformMask[Index] == 0 || TargetHeights[Index] <= TNumericLimits<float>::Lowest() + 1.f)
		{
			continue;
		}

		float EdgeAlpha = 1.f;
		if (FalloffRadius > 0 && DistanceFromEdge.IsValidIndex(Index))
		{
			EdgeAlpha = Quick_RoadConformLocals::ComputeEdgeFalloffAlpha(DistanceFromEdge[Index], FalloffRadius);
		}

		Buffer.FloatHeights[Index] = FMath::Lerp(
			OriginalHeights[Index],
			TargetHeights[Index],
			EdgeAlpha * Blend);
	}

	int32 MeshActorCount = 0;
	{
		TSet<AActor*> MeshActors;
		for (UPrimitiveComponent* Component : ConformComponents)
		{
			if (Component && Component->GetOwner())
			{
				MeshActors.Add(Component->GetOwner());
			}
		}
		MeshActorCount = MeshActors.Num();
	}

	UE_LOG(LogQuickRoad, Display,
		TEXT("Quick_Road 贴合模型：模型 %d 个，底面三角 %d 个，命中 %d 个顶点，边缘偏移 %.0f cm（%d 格），边缘过渡 %d 格，Quad=%.0f cm，本地高度 [%.2f, %.2f]"),
		MeshActorCount, SurfaceTriangles.Num(), HitCount, Params.EdgeOffsetCm, Expand, FalloffRadius, QuadSizeCm, MinLocalHeight, MaxLocalHeight);

	if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
		Landscape, Buffer, LOCTEXT("ConformTransaction", "Quick Road Conform To Mesh")))
	{
		OutErrorMessage = LOCTEXT("WriteFailed", "写入 Landscape 高度失败。");
		return false;
	}

	return true;
}

bool FQuick_RoadLandscapeConform::SmoothLandscapeAtCityScopeBoundary(
	UWorld* World,
	ALandscape* Landscape,
	const FQuick_RoadConformEdgeSmoothParams& Params,
	FText& OutErrorMessage)
{
	if (!World || !Landscape)
	{
		OutErrorMessage = LOCTEXT("EdgeSmoothInvalidInput", "世界或 Landscape 无效。");
		return false;
	}

	USplineComponent* CityScopeSpline = nullptr;
	if (!FQuick_RoadLayoutMeshGenerator::FindCityScopeSpline(World, CityScopeSpline, OutErrorMessage))
	{
		return false;
	}

	TArray<FVector2D> BoundaryPolygon;
	Quick_RoadConformLocals::BuildBoundaryPolygonFromSpline(
		CityScopeSpline,
		FMath::Max(Params.SplineSampleCount, 12),
		BoundaryPolygon);
	if (BoundaryPolygon.Num() < 3)
	{
		OutErrorMessage = LOCTEXT("EdgeSmoothInvalidSpline", "城市范围样条无效，无法构建边界。");
		return false;
	}

	FQuick_RoadLandscapeHeightBuffer Buffer;
	if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
	{
		return false;
	}

	const float SmoothWidthCm = FMath::Max(Params.SmoothWidthCm, 50.f);
	const float SmoothStrength = FMath::Clamp(Params.SmoothStrength, 0.f, 1.f);
	const int32 Iterations = FMath::Clamp(Params.SmoothIterations, 1, 32);
	const int32 Width = Buffer.Width;
	const int32 Height = Buffer.Height;

	FBox2D PolygonBounds(ForceInit);
	for (const FVector2D& Point : BoundaryPolygon)
	{
		PolygonBounds += Point;
	}
	PolygonBounds = PolygonBounds.ExpandBy(SmoothWidthCm + 100.f);

	TArray<float> SmoothWeights;
	SmoothWeights.Init(0.f, Buffer.FloatHeights.Num());
	int32 WeightedVertexCount = 0;

	for (int32 VertY = Buffer.MinY; VertY <= Buffer.MaxY; ++VertY)
	{
		for (int32 VertX = Buffer.MinX; VertX <= Buffer.MaxX; ++VertX)
		{
			const FVector WorldXY = Quick_RoadConformLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);
			const FVector2D WorldPoint(WorldXY.X, WorldXY.Y);
			if (!PolygonBounds.IsInside(WorldPoint))
			{
				continue;
			}

			const float DistanceToBoundary = Quick_RoadConformLocals::DistancePointToPolygonBoundary2D(
				WorldPoint,
				BoundaryPolygon);
			const float Weight = Quick_RoadConformLocals::ComputeBoundarySmoothWeight(
				DistanceToBoundary,
				SmoothWidthCm,
				SmoothStrength);
			if (Weight <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const int32 LocalX = VertX - Buffer.MinX;
			const int32 LocalY = VertY - Buffer.MinY;
			const int32 Index = LocalY * Width + LocalX;
			SmoothWeights[Index] = Weight;
			++WeightedVertexCount;
		}
	}

	if (WeightedVertexCount == 0)
	{
		OutErrorMessage = LOCTEXT("EdgeSmoothNoVertices", "城市范围边界与 Landscape 无交集，请确认样条覆盖地形。");
		return false;
	}

	TArray<float> Heights = Buffer.FloatHeights;
	for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
	{
		TArray<float> NextHeights = Heights;
		for (int32 LocalY = 0; LocalY < Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Width; ++LocalX)
			{
				const int32 Index = LocalY * Width + LocalX;
				const float Weight = SmoothWeights[Index];
				if (Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				float NeighborAverage = Heights[Index];
				int32 NeighborCount = 0;
				Quick_RoadConformLocals::ComputeNeighborAverageHeight(
					Heights,
					Width,
					Height,
					LocalX,
					LocalY,
					NeighborAverage,
					NeighborCount);
				if (NeighborCount <= 0)
				{
					continue;
				}

				NextHeights[Index] = FMath::Lerp(Heights[Index], NeighborAverage, Weight);
			}
		}

		Heights = MoveTemp(NextHeights);
	}

	Buffer.FloatHeights = MoveTemp(Heights);

	const FGuid EditLayerGuid = FQuick_RoadLandscapeHeightEdit::ResolveEditingLayerGuid(Landscape);
	const FString LayerDisplayName = QuickRoadLandscapeCompat::GetLayerDisplayName(Landscape, EditLayerGuid);
	if (!LayerDisplayName.IsEmpty())
	{
		UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 边缘平滑：写入 Edit Layer「%s」"), *LayerDisplayName);
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 边缘平滑：样条边界 %d 点，平滑带 %.0f cm，顶点 %d，迭代 %d"),
		BoundaryPolygon.Num(),
		SmoothWidthCm,
		WeightedVertexCount,
		Iterations);

	if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
		Landscape,
		Buffer,
		LOCTEXT("EdgeSmoothTransaction", "Quick Road Smooth Conform Edge")))
	{
		OutErrorMessage = LOCTEXT("EdgeSmoothWriteFailed", "写入 Landscape 高度失败。");
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
