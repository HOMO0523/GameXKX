// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLayoutRoadGenerator.h"
#include "Quick_RoadLayoutDrawStageParams.h"
#include "Quick_RoadLayoutRoadTags.h"
#include "Quick_RoadLayoutSplineTags.h"
#include "Quick_RoadLayoutMeshTags.h"
#include "Quick_RoadLayoutMeshComponent.h"
#include "Quick_RoadLayoutRoadMeshComponent.h"
#include "Quick_RoadRoadInfluenceComponent.h"
#include "Quick_RoadRoadSplineWidthComponent.h"
#include "Quick_RoadRoadMeshTypes.h"
#include "Tools/Quick_RoadLayoutMeshGenerator.h"
#include "Tools/Quick_RoadLandscapeHeightEdit.h"
#include "Tools/Quick_RoadRoadMeshBaker.h"
#include "Quick_RoadLog.h"

#include "Components/SplineComponent.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "CollisionQueryParams.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MaterialDomain.h"
#include "UObject/SoftObjectPath.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "Selection.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLayoutRoadGenerator"

namespace Quick_RoadRoadLocals
{
	// 弧长 UV 基准：1 UV 单位 = 100cm 世界距离；贴图重复密度由材质 Tiling 控制（与城市布局面片一致）。
	static constexpr float RoadMeshUVWorldScaleCm = 100.f;

	static FVector2D MakeRoadRibbonUV(float DistanceAlongSplineCm, float LateralOffsetCm)
	{
		return FVector2D(
			DistanceAlongSplineCm / RoadMeshUVWorldScaleCm,
			LateralOffsetCm / RoadMeshUVWorldScaleCm);
	}

	// 路口 hub：世界 XY 平面 UV，插值平滑；主路与臂条带仍用道路坐标 UV。
	static FVector2D MakeRoadWorldPlanarUV(const FVector& WorldPosition)
	{
		return FVector2D(
			WorldPosition.X / RoadMeshUVWorldScaleCm,
			WorldPosition.Y / RoadMeshUVWorldScaleCm);
	}

	static float RoadRingLateralOffsetCm(int32 RingIndex, int32 RingCount, float HalfWidthCm)
	{
		if (RingCount <= 1)
		{
			return 0.f;
		}

		const float Alpha = static_cast<float>(RingIndex) / static_cast<float>(RingCount - 1);
		return FMath::Lerp(-HalfWidthCm, HalfWidthCm, Alpha);
	}

	static void ComputeNormals(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		TArray<FVector>& OutNormals);

	static float DistancePointToSegment2D(
		const FVector2D& Point,
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		float& OutProjectionAlpha)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentLengthSq = Segment.SizeSquared();
		if (SegmentLengthSq <= KINDA_SMALL_NUMBER)
		{
			OutProjectionAlpha = 0.f;
			return FVector2D::Distance(Point, SegmentStart);
		}

		const float Projection = FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentLengthSq;
		OutProjectionAlpha = FMath::Clamp(Projection, 0.f, 1.f);
		const FVector2D ClosestPoint = SegmentStart + Segment * OutProjectionAlpha;
		return FVector2D::Distance(Point, ClosestPoint);
	}

	static float ComputeCorridorWeight(float DistanceCm, float HalfWidthCm, float FalloffCm)
	{
		if (DistanceCm <= HalfWidthCm)
		{
			return 1.f;
		}

		if (FalloffCm <= KINDA_SMALL_NUMBER || DistanceCm >= HalfWidthCm + FalloffCm)
		{
			return 0.f;
		}

		const float Normalized = 1.f - (DistanceCm - HalfWidthCm) / FalloffCm;
		return FMath::SmoothStep(0.f, 1.f, Normalized);
	}

	static float WorldZToLandscapeLocalHeight(ALandscape* Landscape, float WorldZ, float WorldX, float WorldY)
	{
		const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
		return LandscapeToWorld.InverseTransformPosition(FVector(WorldX, WorldY, WorldZ)).Z;
	}

	static FVector LandscapeVertexToWorldXY(ALandscape* Landscape, int32 VertX, int32 VertY)
	{
		const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
		return LandscapeToWorld.TransformPosition(FVector(static_cast<float>(VertX), static_cast<float>(VertY), 0.f));
	}

	static constexpr float IntersectionMergeThresholdCm = 80.f;

	struct FRoadPolyLineSegment
	{
		TWeakObjectPtr<USplineComponent> OwnerSpline;
		uint32 SegmentIndex = 0;
		uint32 LastSegmentIndex = 0;
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D End = FVector2D::ZeroVector;
		float StartZ = 0.f;
		float EndZ = 0.f;
		uint32 GlobalIndex = 0;
	};

	static uint32 GSegmentGlobalIndex = 0;

	static bool Get2DSegmentIntersection(
		const FVector2D& AStart,
		const FVector2D& AEnd,
		const FVector2D& BStart,
		const FVector2D& BEnd,
		FVector2D& OutIntersection)
	{
		if (FMath::Max(AStart.X, AEnd.X) < FMath::Min(BStart.X, BEnd.X) ||
			FMath::Max(BStart.X, BEnd.X) < FMath::Min(AStart.X, AEnd.X) ||
			FMath::Max(AStart.Y, AEnd.Y) < FMath::Min(BStart.Y, BEnd.Y) ||
			FMath::Max(BStart.Y, BEnd.Y) < FMath::Min(AStart.Y, AEnd.Y))
		{
			return false;
		}

		const FVector2D VectorA = AEnd - AStart;
		const FVector2D VectorB = BEnd - BStart;
		const FVector2D VectorABStart = BStart - AStart;
		const float Denominator = FVector2D::CrossProduct(VectorA, VectorB);
		if (FMath::IsNearlyZero(Denominator))
		{
			return false;
		}

		const float T = FVector2D::CrossProduct(VectorABStart, VectorB) / Denominator;
		const float S = FVector2D::CrossProduct(VectorABStart, VectorA) / Denominator;
		if (T >= 0.f && T <= 1.f && S >= 0.f && S <= 1.f)
		{
			OutIntersection = AStart + VectorA * T;
			return true;
		}

		return false;
	}

	static void BuildPolyLineSegments(
		const TArray<USplineComponent*>& Splines,
		float SampleDistanceCm,
		TArray<FRoadPolyLineSegment>& OutSegments)
	{
		OutSegments.Reset();
		GSegmentGlobalIndex = 0;
		const float MinSampleDistance = FMath::Max(SampleDistanceCm, 25.f);

		for (USplineComponent* Spline : Splines)
		{
			if (!Spline)
			{
				continue;
			}

			const float SplineLength = Spline->GetSplineLength();
			if (SplineLength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const int32 SampleCount = FMath::Max(FMath::CeilToInt(SplineLength / MinSampleDistance) + 1, 2);
			const uint32 LastSegmentIndex = static_cast<uint32>(SampleCount - 2);
			FVector PrevLocation = Spline->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World);

			for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
			{
				const float Distance = (SampleCount > 1)
					? (SplineLength * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1))
					: SplineLength;
				const FVector CurrentLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

				FRoadPolyLineSegment Segment;
				Segment.OwnerSpline = Spline;
				Segment.SegmentIndex = static_cast<uint32>(SampleIndex - 1);
				Segment.LastSegmentIndex = LastSegmentIndex;
				Segment.Start = FVector2D(PrevLocation.X, PrevLocation.Y);
				Segment.End = FVector2D(CurrentLocation.X, CurrentLocation.Y);
				Segment.StartZ = PrevLocation.Z;
				Segment.EndZ = CurrentLocation.Z;
				Segment.GlobalIndex = GSegmentGlobalIndex++;
				OutSegments.Add(Segment);

				PrevLocation = CurrentLocation;
			}
		}
	}

	static void DetectIntersectionsInternal(
		const TArray<FRoadPolyLineSegment>& Segments,
		float MergeThresholdCm,
		TArray<FQuick_RoadRoadIntersectionInfo>& OutIntersections)
	{
		OutIntersections.Reset();
		TSet<TPair<uint32, uint32>> ProcessedPairs;

		for (int32 IndexA = 0; IndexA < Segments.Num(); ++IndexA)
		{
			const FRoadPolyLineSegment& SegmentA = Segments[IndexA];
			for (int32 IndexB = IndexA + 1; IndexB < Segments.Num(); ++IndexB)
			{
				const FRoadPolyLineSegment& SegmentB = Segments[IndexB];

				if (SegmentA.OwnerSpline == SegmentB.OwnerSpline)
				{
					const uint32 IndexGap = SegmentA.SegmentIndex > SegmentB.SegmentIndex
						? SegmentA.SegmentIndex - SegmentB.SegmentIndex
						: SegmentB.SegmentIndex - SegmentA.SegmentIndex;
					if (IndexGap <= 1 || IndexGap == SegmentA.LastSegmentIndex)
					{
						continue;
					}
				}

				const TPair<uint32, uint32> PairKey(
					FMath::Min(SegmentA.GlobalIndex, SegmentB.GlobalIndex),
					FMath::Max(SegmentA.GlobalIndex, SegmentB.GlobalIndex));
				if (ProcessedPairs.Contains(PairKey))
				{
					continue;
				}
				ProcessedPairs.Add(PairKey);

				FVector2D Intersection2D;
				if (!Get2DSegmentIntersection(SegmentA.Start, SegmentA.End, SegmentB.Start, SegmentB.End, Intersection2D))
				{
					continue;
				}

				float ProjectionAlpha = 0.f;
				DistancePointToSegment2D(Intersection2D, SegmentA.Start, SegmentA.End, ProjectionAlpha);
				const float IntersectionZ = FMath::Lerp(SegmentA.StartZ, SegmentA.EndZ, ProjectionAlpha);

				bool bMerged = false;
				for (FQuick_RoadRoadIntersectionInfo& Existing : OutIntersections)
				{
					if (FVector2D::DistSquared(FVector2D(Existing.WorldLocation), Intersection2D) <=
						FMath::Square(MergeThresholdCm))
					{
						if (USplineComponent* SplineA = SegmentA.OwnerSpline.Get())
						{
							Existing.Splines.AddUnique(SplineA);
						}
						if (USplineComponent* SplineB = SegmentB.OwnerSpline.Get())
						{
							Existing.Splines.AddUnique(SplineB);
						}
						Existing.WorldLocation.Z = (Existing.WorldLocation.Z + IntersectionZ) * 0.5f;
						bMerged = true;
						break;
					}
				}

				if (!bMerged)
				{
					FQuick_RoadRoadIntersectionInfo NewIntersection;
					NewIntersection.WorldLocation = FVector(Intersection2D.X, Intersection2D.Y, IntersectionZ);
					if (USplineComponent* SplineA = SegmentA.OwnerSpline.Get())
					{
						NewIntersection.Splines.Add(SplineA);
					}
					if (USplineComponent* SplineB = SegmentB.OwnerSpline.Get())
					{
						NewIntersection.Splines.Add(SplineB);
					}
					OutIntersections.Add(NewIntersection);
				}
			}
		}
	}

	static void MergeDistanceIntervals(TArray<TPair<float, float>>& InOutIntervals)
	{
		if (InOutIntervals.Num() <= 1)
		{
			return;
		}

		InOutIntervals.Sort([](const TPair<float, float>& A, const TPair<float, float>& B)
		{
			return A.Key < B.Key;
		});

		TArray<TPair<float, float>> Merged;
		Merged.Add(InOutIntervals[0]);
		for (int32 Index = 1; Index < InOutIntervals.Num(); ++Index)
		{
			TPair<float, float>& Last = Merged.Last();
			if (InOutIntervals[Index].Key <= Last.Value)
			{
				Last.Value = FMath::Max(Last.Value, InOutIntervals[Index].Value);
			}
			else
			{
				Merged.Add(InOutIntervals[Index]);
			}
		}

		InOutIntervals = MoveTemp(Merged);
	}

	static void BuildExclusionIntervalsForSpline(
		USplineComponent* Spline,
		const TArray<FQuick_RoadRoadIntersectionInfo>& Intersections,
		float ExtendLengthCm,
		TArray<TPair<float, float>>& OutIntervals)
	{
		OutIntervals.Reset();
		if (!Spline || ExtendLengthCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		for (const FQuick_RoadRoadIntersectionInfo& Intersection : Intersections)
		{
			bool bUsesSpline = Intersection.Splines.Contains(Spline);
			if (!bUsesSpline)
			{
				continue;
			}

			const float Distance = Spline->GetDistanceAlongSplineAtLocation(
				Intersection.WorldLocation,
				ESplineCoordinateSpace::World);
			OutIntervals.Emplace(
				FMath::Max(0.f, Distance - ExtendLengthCm),
				Distance + ExtendLengthCm);
		}

		MergeDistanceIntervals(OutIntervals);
	}

	static FVector ComputeRibbonRightVector(USplineComponent* Spline, float Distance)
	{
		FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::ForwardVector;
		}
		else
		{
			Tangent.Normalize();
		}

		return FVector::CrossProduct(Tangent, FVector::UpVector).GetSafeNormal();
	}

	static void BuildCrossSectionRing(
		USplineComponent* Spline,
		float Distance,
		float HalfWidthCm,
		int32 WidthSubdivisions,
		TArray<FVector>& OutRing)
	{
		OutRing.Reset();
		const FVector Center = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		const FVector Right = ComputeRibbonRightVector(Spline, Distance);
		const int32 NumPoints = FMath::Max(WidthSubdivisions, 1) + 1;

		OutRing.Reserve(NumPoints);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(NumPoints - 1);
			const float LateralOffset = FMath::Lerp(-HalfWidthCm, HalfWidthCm, Alpha);
			OutRing.Add(Center + Right * LateralOffset);
		}
	}

	static void CollectAdaptiveSampleDistances(
		USplineComponent* Spline,
		float DistanceStart,
		float DistanceEnd,
		float MinSpacingCm,
		float CurvatureThresholdDeg,
		int32 RemainingSubdivisions,
		TArray<float>& InOutDistances)
	{
		if (RemainingSubdivisions <= 0 || DistanceEnd - DistanceStart <= MinSpacingCm * 0.5f)
		{
			return;
		}

		const float MidDistance = (DistanceStart + DistanceEnd) * 0.5f;
		FVector TangentStart = Spline->GetTangentAtDistanceAlongSpline(DistanceStart, ESplineCoordinateSpace::World);
		FVector TangentEnd = Spline->GetTangentAtDistanceAlongSpline(DistanceEnd, ESplineCoordinateSpace::World);
		TangentStart.Z = 0.f;
		TangentEnd.Z = 0.f;
		if (TangentStart.IsNearlyZero() || TangentEnd.IsNearlyZero())
		{
			return;
		}

		TangentStart.Normalize();
		TangentEnd.Normalize();
		const float AngleDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(TangentStart, TangentEnd), -1.f, 1.f)));
		if (AngleDeg < CurvatureThresholdDeg)
		{
			return;
		}

		CollectAdaptiveSampleDistances(
			Spline,
			DistanceStart,
			MidDistance,
			MinSpacingCm,
			CurvatureThresholdDeg,
			RemainingSubdivisions - 1,
			InOutDistances);
		InOutDistances.Add(MidDistance);
		CollectAdaptiveSampleDistances(
			Spline,
			MidDistance,
			DistanceEnd,
			MinSpacingCm,
			CurvatureThresholdDeg,
			RemainingSubdivisions - 1,
			InOutDistances);
	}

	static void BuildRunSampleDistances(
		USplineComponent* Spline,
		float RunStart,
		float RunEnd,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		TArray<float>& OutDistances)
	{
		OutDistances.Reset();
		if (!Spline || RunEnd - RunStart <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float SampleDistanceCm = FMath::Max(MeshParams.SampleDistanceCm, 25.f);
		OutDistances.Add(RunStart);
		for (float Distance = RunStart + SampleDistanceCm;
			Distance < RunEnd - SampleDistanceCm * 0.25f;
			Distance += SampleDistanceCm)
		{
			OutDistances.Add(Distance);
		}
		OutDistances.Add(RunEnd);

		if (MeshParams.bAdaptiveCurvature && OutDistances.Num() >= 2)
		{
			TArray<float> RefinedDistances;
			RefinedDistances.Add(OutDistances[0]);
			for (int32 Index = 0; Index < OutDistances.Num() - 1; ++Index)
			{
				const float SegmentStart = OutDistances[Index];
				const float SegmentEnd = OutDistances[Index + 1];
				TArray<float> InsertedDistances;
				CollectAdaptiveSampleDistances(
					Spline,
					SegmentStart,
					SegmentEnd,
					SampleDistanceCm,
					MeshParams.CurvatureThresholdDeg,
					MeshParams.MaxCurvatureSubdivisions,
					InsertedDistances);
				InsertedDistances.Sort();
				for (const float InsertedDistance : InsertedDistances)
				{
					if (InsertedDistance > RefinedDistances.Last() + KINDA_SMALL_NUMBER &&
						InsertedDistance < SegmentEnd - KINDA_SMALL_NUMBER)
					{
						RefinedDistances.Add(InsertedDistance);
					}
				}
				RefinedDistances.Add(SegmentEnd);
			}
			OutDistances = MoveTemp(RefinedDistances);
		}
	}

	static void AppendRibbonQuadStrip(
		const TArray<FVector>& PrevRing,
		const TArray<FVector>& CurrentRing,
		float PrevDistance,
		float CurrentDistance,
		float HalfWidthCm,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs,
		const TArray<int32>* PrevRingVertexIndices = nullptr)
	{
		if (PrevRing.Num() != CurrentRing.Num() || PrevRing.Num() < 2)
		{
			return;
		}

		const int32 RingCount = PrevRing.Num();
		TArray<int32> RowAIndices;
		TArray<int32> RowBIndices;
		RowAIndices.Reserve(RingCount);
		RowBIndices.Reserve(RingCount);

		for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
		{
			const float LateralOffsetCm = RoadRingLateralOffsetCm(RingIndex, RingCount, HalfWidthCm);

			int32 IndexA = INDEX_NONE;
			if (PrevRingVertexIndices && PrevRingVertexIndices->Num() == RingCount)
			{
				IndexA = (*PrevRingVertexIndices)[RingIndex];
			}
			else
			{
				IndexA = InOutVertices.Add(PrevRing[RingIndex]);
				InOutUVs.Add(MakeRoadRibbonUV(PrevDistance, LateralOffsetCm));
			}
			RowAIndices.Add(IndexA);

			const int32 IndexB = InOutVertices.Add(CurrentRing[RingIndex]);
			InOutUVs.Add(MakeRoadRibbonUV(CurrentDistance, LateralOffsetCm));
			RowBIndices.Add(IndexB);
		}

		for (int32 RingIndex = 0; RingIndex < RingCount - 1; ++RingIndex)
		{
			const int32 I00 = RowAIndices[RingIndex];
			const int32 I01 = RowBIndices[RingIndex];
			const int32 I10 = RowAIndices[RingIndex + 1];
			const int32 I11 = RowBIndices[RingIndex + 1];

			InOutTriangles.Add(I00);
			InOutTriangles.Add(I11);
			InOutTriangles.Add(I10);

			InOutTriangles.Add(I00);
			InOutTriangles.Add(I01);
			InOutTriangles.Add(I11);
		}
	}

	static int32 AppendRibbonStripForSplineInternal(
		USplineComponent* Spline,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		const TArray<FQuick_RoadRoadIntersectionInfo>* Intersections,
		float ExtendLengthCm,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		if (!Spline)
		{
			return 0;
		}

		const float SplineLength = Spline->GetSplineLength();
		if (SplineLength <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}

		TArray<TPair<float, float>> Runs;
		if (Intersections && ExtendLengthCm > KINDA_SMALL_NUMBER)
		{
			TArray<TPair<float, float>> ExclusionIntervals;
			BuildExclusionIntervalsForSpline(Spline, *Intersections, ExtendLengthCm, ExclusionIntervals);

			float RunStart = 0.f;
			for (const TPair<float, float>& Interval : ExclusionIntervals)
			{
				if (RunStart < Interval.Key - KINDA_SMALL_NUMBER)
				{
					Runs.Emplace(RunStart, Interval.Key);
				}
				RunStart = FMath::Max(RunStart, Interval.Value);
			}
			if (RunStart < SplineLength - KINDA_SMALL_NUMBER)
			{
				Runs.Emplace(RunStart, SplineLength);
			}
			if (Runs.Num() == 0 && ExclusionIntervals.Num() == 0)
			{
				Runs.Emplace(0.f, SplineLength);
			}
		}
		else
		{
			Runs.Emplace(0.f, SplineLength);
		}

		int32 RibbonSegmentCount = 0;
		for (const TPair<float, float>& Run : Runs)
		{
			const float RunLength = Run.Value - Run.Key;
			if (RunLength < MeshParams.SampleDistanceCm * 0.25f)
			{
				continue;
			}

			TArray<float> SampleDistances;
			BuildRunSampleDistances(Spline, Run.Key, Run.Value, MeshParams, SampleDistances);
			if (SampleDistances.Num() < 2)
			{
				continue;
			}

			TArray<FVector> PrevRing;
			BuildCrossSectionRing(
				Spline,
				SampleDistances[0],
				MeshParams.HalfWidthCm,
				MeshParams.WidthSubdivisions,
				PrevRing);

			for (int32 SampleIndex = 1; SampleIndex < SampleDistances.Num(); ++SampleIndex)
			{
				TArray<FVector> CurrentRing;
				BuildCrossSectionRing(
					Spline,
					SampleDistances[SampleIndex],
					MeshParams.HalfWidthCm,
					MeshParams.WidthSubdivisions,
					CurrentRing);

				AppendRibbonQuadStrip(
					PrevRing,
					CurrentRing,
					SampleDistances[SampleIndex - 1],
					SampleDistances[SampleIndex],
					MeshParams.HalfWidthCm,
					InOutVertices,
					InOutTriangles,
					InOutUVs);

				PrevRing = MoveTemp(CurrentRing);
				++RibbonSegmentCount;
			}
		}

		return RibbonSegmentCount;
	}

	static int32 AppendRibbonStripForSpline(
		USplineComponent* Spline,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		return AppendRibbonStripForSplineInternal(
			Spline,
			MeshParams,
			nullptr,
			0.f,
			InOutVertices,
			InOutTriangles,
			InOutUVs);
	}

	static bool GenerateRibbonMeshForDistanceRange(
		USplineComponent* Spline,
		float StartDist,
		float EndDist,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		bool bIncludeStartCap,
		bool bIncludeEndCap,
		TArray<FVector>& OutVertices,
		TArray<int32>& OutTriangles,
		TArray<FVector2D>& OutUVs)
	{
		const float RunLength = EndDist - StartDist;
		if (!Spline || RunLength < MeshParams.SampleDistanceCm * 0.25f)
		{
			return false;
		}

		TArray<float> SampleDistances;
		BuildRunSampleDistances(Spline, StartDist, EndDist, MeshParams, SampleDistances);
		if (SampleDistances.Num() < 2)
		{
			return false;
		}

		const int32 FirstRingIndex = bIncludeStartCap ? 0 : 1;
		const int32 LastRingIndex = bIncludeEndCap
			? SampleDistances.Num() - 1
			: SampleDistances.Num() - 2;
		if (FirstRingIndex >= LastRingIndex)
		{
			return false;
		}

		TArray<FVector> PrevRing;
		BuildCrossSectionRing(
			Spline,
			SampleDistances[FirstRingIndex],
			MeshParams.HalfWidthCm,
			MeshParams.WidthSubdivisions,
			PrevRing);

		for (int32 SampleIndex = FirstRingIndex + 1; SampleIndex <= LastRingIndex; ++SampleIndex)
		{
			TArray<FVector> CurrentRing;
			BuildCrossSectionRing(
				Spline,
				SampleDistances[SampleIndex],
				MeshParams.HalfWidthCm,
				MeshParams.WidthSubdivisions,
				CurrentRing);

			AppendRibbonQuadStrip(
				PrevRing,
				CurrentRing,
				SampleDistances[SampleIndex - 1],
				SampleDistances[SampleIndex],
				MeshParams.HalfWidthCm,
				OutVertices,
				OutTriangles,
				OutUVs);

			PrevRing = MoveTemp(CurrentRing);
		}

		return OutTriangles.Num() >= 3;
	}

	static bool IsExclusionApproachBoundary(
		float Distance,
		const TArray<TPair<float, float>>& ExclusionIntervals,
		float ToleranceCm = 2.f)
	{
		for (const TPair<float, float>& Interval : ExclusionIntervals)
		{
			if (FMath::IsNearlyEqual(Distance, Interval.Key, ToleranceCm))
			{
				return true;
			}
		}
		return false;
	}

	static bool IsExclusionDepartureBoundary(
		float Distance,
		const TArray<TPair<float, float>>& ExclusionIntervals,
		float ToleranceCm = 2.f)
	{
		for (const TPair<float, float>& Interval : ExclusionIntervals)
		{
			if (FMath::IsNearlyEqual(Distance, Interval.Value, ToleranceCm))
			{
				return true;
			}
		}
		return false;
	}

	// 交叉口的一条「臂」：从路口中心沿某条样条向外（+/- 方向）延伸的路段。
	struct FIntersectionArm
	{
		USplineComponent* Spline = nullptr;
		float CenterDistance = 0.f;   // 交点在该样条上的弧长
		float Sign = 1.f;             // +1 沿样条正向，-1 反向
		float InnerDistance = 0.f;    // 路口外缘（臂的内端）弧长
		float OuterDistance = 0.f;    // 与路段衔接处（臂的外端）弧长
		float OutwardAngleRad = 0.f;  // 出射方向在 XY 平面的极角
		float HalfWidthCm = 600.f;    // 该臂所属样条的路带半宽
		TArray<FVector> InnerRing;    // 内端横截面顶点：沿 cross(Outward,Up) 取点（与主路一致）
	};

	static FQuick_RoadRoadRibbonBuildParams MakeRibbonParamsForSpline(
		USplineComponent* Spline,
		const FQuick_RoadRoadRibbonBuildParams& BaseParams)
	{
		FQuick_RoadRoadRibbonBuildParams Result = BaseParams;
		Result.HalfWidthCm = UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(
			Spline,
			BaseParams.HalfWidthCm);
		return Result;
	}

	static float ComputeCCWGapRad(float AngleA, float AngleB)
	{
		float Gap = AngleB - AngleA;
		if (Gap < 0.f)
		{
			Gap += 2.f * PI;
		}
		return Gap;
	}

	static float ComputeArmInnerTrimCm(
		float HalfWidthCm,
		float PrevGapRad,
		float NextGapRad,
		float ExtendLengthCm)
	{
		float InnerTrimCm = HalfWidthCm;

		auto AccumulateGap = [&InnerTrimCm, HalfWidthCm](float GapRad)
		{
			const float Gap = FMath::Clamp(
				GapRad,
				FMath::DegreesToRadians(15.f),
				FMath::DegreesToRadians(345.f));
			const float HalfGapTan = FMath::Tan(FMath::Min(Gap, 2.f * PI - Gap) * 0.5f);
			if (HalfGapTan > KINDA_SMALL_NUMBER)
			{
				InnerTrimCm = FMath::Max(InnerTrimCm, HalfWidthCm / HalfGapTan);
			}
		};

		AccumulateGap(PrevGapRad);
		AccumulateGap(NextGapRad);

		return FMath::Clamp(
			InnerTrimCm,
			HalfWidthCm * 0.5f,
			FMath::Max(ExtendLengthCm * 0.9f, HalfWidthCm * 0.5f));
	}

	// 以「出射方向」为基准构造横截面顶点环：ring[0]=右侧角点，ring[last]=左侧角点。
	// 不论 Sign 正负，环始终按统一的左右约定排列，保证 junction 与臂的绕序一致。
	static void BuildArmCrossRing(
		USplineComponent* Spline,
		float Distance,
		float Sign,
		float HalfWidthCm,
		int32 WidthSubdivisions,
		TArray<FVector>& OutRing)
	{
		OutRing.Reset();
		if (!Spline)
		{
			return;
		}

		const FVector Center = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		Tangent.Z = 0.f;
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::ForwardVector;
		}
		Tangent.Normalize();

		const FVector Outward = Tangent * Sign;
		// 横向基向量与主路 BuildCrossSectionRing 保持一致：Right = cross(Outward, Up)。
		// 这样臂的 quad strip 复用 AppendRibbonQuadStrip 时绕序与主路相同，法线自然朝上，
		// 无需再靠 bForceUpwardNormals 兜底。
		const FVector RightDir(Outward.Y, -Outward.X, 0.f);
		const int32 NumPoints = FMath::Max(WidthSubdivisions, 1) + 1;

		OutRing.Reserve(NumPoints);
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			const float Alpha = static_cast<float>(Index) / static_cast<float>(NumPoints - 1);
			const float Lateral = FMath::Lerp(-HalfWidthCm, HalfWidthCm, Alpha);
			OutRing.Add(Center + RightDir * Lateral);
		}
	}

	// 收集交叉口所有有效臂，按各样条半宽计算内缘 trim 与横截面，按出射极角排序。
	static int32 CollectIntersectionArms(
		const FQuick_RoadRoadIntersectionInfo& Intersection,
		float ExtendLengthCm,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		float CornerRadiusScale,
		TArray<FIntersectionArm>& OutArms)
	{
		OutArms.Reset();

		struct FArmSeed
		{
			USplineComponent* Spline = nullptr;
			float CenterDistance = 0.f;
			float Sign = 1.f;
			float OutwardAngleRad = 0.f;
			float HalfWidthCm = 600.f;
		};

		TArray<FArmSeed> Seeds;
		const float FallbackHalfWidthCm = FMath::Max(MeshParams.HalfWidthCm, 50.f);

		for (const TObjectPtr<USplineComponent>& SplinePtr : Intersection.Splines)
		{
			USplineComponent* Spline = SplinePtr.Get();
			if (!Spline)
			{
				continue;
			}

			const float SplineHalfWidthCm = UQuick_RoadRoadSplineWidthComponent::ResolveHalfWidthCm(
				Spline,
				FallbackHalfWidthCm);
			const float MinRoom = FMath::Max(SplineHalfWidthCm * 0.25f, 25.f);

			const float SplineLength = Spline->GetSplineLength();
			if (SplineLength <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float CenterDistance = Spline->GetDistanceAlongSplineAtLocation(
				Intersection.WorldLocation,
				ESplineCoordinateSpace::World);
			FVector Tangent = Spline->GetTangentAtDistanceAlongSpline(CenterDistance, ESplineCoordinateSpace::World);
			Tangent.Z = 0.f;
			if (Tangent.IsNearlyZero())
			{
				continue;
			}
			Tangent.Normalize();

			for (int32 SignIndex = 0; SignIndex < 2; ++SignIndex)
			{
				const float Sign = SignIndex == 0 ? 1.f : -1.f;
				const float Room = Sign > 0.f ? (SplineLength - CenterDistance) : CenterDistance;
				if (Room < MinRoom)
				{
					continue;
				}

				const FVector Outward = Tangent * Sign;
				FArmSeed Seed;
				Seed.Spline = Spline;
				Seed.CenterDistance = CenterDistance;
				Seed.Sign = Sign;
				Seed.OutwardAngleRad = FMath::Atan2(Outward.Y, Outward.X);
				Seed.HalfWidthCm = SplineHalfWidthCm;
				Seeds.Add(Seed);
			}
		}

		if (Seeds.Num() < 2)
		{
			return 0;
		}

		Seeds.Sort([](const FArmSeed& A, const FArmSeed& B)
		{
			return A.OutwardAngleRad < B.OutwardAngleRad;
		});

		const float Scale = FMath::Max(CornerRadiusScale, 0.01f);
		const int32 SeedCount = Seeds.Num();

		for (int32 SeedIndex = 0; SeedIndex < SeedCount; ++SeedIndex)
		{
			const FArmSeed& Seed = Seeds[SeedIndex];
			const FArmSeed& PrevSeed = Seeds[(SeedIndex + SeedCount - 1) % SeedCount];
			const FArmSeed& NextSeed = Seeds[(SeedIndex + 1) % SeedCount];

			const float PrevGapRad = ComputeCCWGapRad(PrevSeed.OutwardAngleRad, Seed.OutwardAngleRad);
			const float NextGapRad = ComputeCCWGapRad(Seed.OutwardAngleRad, NextSeed.OutwardAngleRad);

			const float BaseInnerTrimCm = ComputeArmInnerTrimCm(
				Seed.HalfWidthCm,
				PrevGapRad,
				NextGapRad,
				ExtendLengthCm);
			const float InnerTrimCm = FMath::Clamp(
				BaseInnerTrimCm * Scale,
				Seed.HalfWidthCm * 0.5f,
				FMath::Max(ExtendLengthCm * 0.95f, Seed.HalfWidthCm * 0.5f));

			const float SplineLength = Seed.Spline->GetSplineLength();
			const float InnerDistance = FMath::Clamp(Seed.CenterDistance + Seed.Sign * InnerTrimCm, 0.f, SplineLength);
			const float OuterDistance = FMath::Clamp(Seed.CenterDistance + Seed.Sign * ExtendLengthCm, 0.f, SplineLength);

			FIntersectionArm Arm;
			Arm.Spline = Seed.Spline;
			Arm.CenterDistance = Seed.CenterDistance;
			Arm.Sign = Seed.Sign;
			Arm.InnerDistance = InnerDistance;
			Arm.OuterDistance = OuterDistance;
			Arm.OutwardAngleRad = Seed.OutwardAngleRad;
			Arm.HalfWidthCm = Seed.HalfWidthCm;
			BuildArmCrossRing(
				Seed.Spline,
				InnerDistance,
				Seed.Sign,
				Arm.HalfWidthCm,
				MeshParams.WidthSubdivisions,
				Arm.InnerRing);
			OutArms.Add(Arm);
		}

		return OutArms.Num();
	}

	static void AppendIntersectionJunctionFan(
		const FQuick_RoadRoadIntersectionInfo& Intersection,
		const TArray<FIntersectionArm>& Arms,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		if (Arms.Num() < 2)
		{
			return;
		}

		TArray<FVector> RingPoints;
		RingPoints.Reserve(Arms.Num() * (MeshParams.WidthSubdivisions + 1));
		for (const FIntersectionArm& Arm : Arms)
		{
			for (int32 PointIndex = Arm.InnerRing.Num() - 1; PointIndex >= 0; --PointIndex)
			{
				RingPoints.Add(Arm.InnerRing[PointIndex]);
			}
		}

		if (RingPoints.Num() < 3)
		{
			return;
		}

		FVector FanCenter = Intersection.WorldLocation;
		{
			double AverageZ = 0.0;
			for (const FVector& Point : RingPoints)
			{
				AverageZ += Point.Z;
			}
			FanCenter.Z = static_cast<float>(AverageZ / static_cast<double>(RingPoints.Num()));
		}

		const int32 FanCenterIndex = InOutVertices.Add(FanCenter);
		InOutUVs.Add(MakeRoadWorldPlanarUV(FanCenter));

		TArray<int32> RingIndices;
		RingIndices.Reserve(RingPoints.Num());
		for (const FVector& Point : RingPoints)
		{
			RingIndices.Add(InOutVertices.Add(Point));
			InOutUVs.Add(MakeRoadWorldPlanarUV(Point));
		}

		// T 字口等大缺口跳过，避免向无路一侧铺面。
		const float MaxFanGapRad = FMath::DegreesToRadians(135.f);
		for (int32 Index = 0; Index < RingIndices.Num(); ++Index)
		{
			const int32 NextIndex = (Index + 1) % RingIndices.Num();
			const FVector& PointA = RingPoints[Index];
			const FVector& PointB = RingPoints[NextIndex];
			const float AngleA = FMath::Atan2(PointA.Y - FanCenter.Y, PointA.X - FanCenter.X);
			const float AngleB = FMath::Atan2(PointB.Y - FanCenter.Y, PointB.X - FanCenter.X);
			const float GapRad = FMath::Abs(FMath::FindDeltaAngleRadians(AngleA, AngleB));
			if (GapRad > MaxFanGapRad)
			{
				continue;
			}

			InOutTriangles.Add(FanCenterIndex);
			InOutTriangles.Add(RingIndices[NextIndex]);
			InOutTriangles.Add(RingIndices[Index]);
		}
	}

	// 单条臂：从内端（路口外缘）沿样条采样到外端（路段衔接处），逐段 quad strip。
	static int32 AppendIntersectionArmStrip(
		const FIntersectionArm& Arm,
		float ArmSampleSpacingCm,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		if (!Arm.Spline)
		{
			return 0;
		}

		const float Span = FMath::Abs(Arm.OuterDistance - Arm.InnerDistance);
		if (Span <= Arm.HalfWidthCm * 0.1f)
		{
			return 0;
		}

		const float Spacing = FMath::Max(ArmSampleSpacingCm, 10.f);
		const int32 SampleCount = FMath::Max(FMath::CeilToInt(Span / Spacing) + 1, 2);

		int32 SegmentCount = 0;
		TArray<FVector> PrevRing = Arm.InnerRing;
		float PrevDistance = Arm.InnerDistance;

		for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1);
			const float Distance = FMath::Lerp(Arm.InnerDistance, Arm.OuterDistance, Alpha);

			TArray<FVector> CurrentRing;
			BuildArmCrossRing(
				Arm.Spline,
				Distance,
				Arm.Sign,
				Arm.HalfWidthCm,
				MeshParams.WidthSubdivisions,
				CurrentRing);
			if (CurrentRing.Num() != PrevRing.Num())
			{
				break;
			}

			AppendRibbonQuadStrip(
				PrevRing,
				CurrentRing,
				PrevDistance,
				Distance,
				Arm.HalfWidthCm,
				InOutVertices,
				InOutTriangles,
				InOutUVs);

			PrevRing = MoveTemp(CurrentRing);
			PrevDistance = Distance;
			++SegmentCount;
		}

		return SegmentCount;
	}

	static int32 AppendIntersectionPatchMesh(
		const FQuick_RoadRoadIntersectionInfo& Intersection,
		float ExtendLengthCm,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		float CornerRadiusScale,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		if (ExtendLengthCm <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}

		TArray<FIntersectionArm> Arms;
		if (CollectIntersectionArms(Intersection, ExtendLengthCm, MeshParams, CornerRadiusScale, Arms) < 2)
		{
			return 0;
		}

		AppendIntersectionJunctionFan(
			Intersection,
			Arms,
			MeshParams,
			InOutVertices,
			InOutTriangles,
			InOutUVs);

		int32 ArmSegmentCount = 0;
		for (const FIntersectionArm& Arm : Arms)
		{
			const float SingleSegmentSpacing = FMath::Max(FMath::Abs(Arm.OuterDistance - Arm.InnerDistance), 10.f);
			ArmSegmentCount += AppendIntersectionArmStrip(
				Arm,
				SingleSegmentSpacing,
				MeshParams,
				InOutVertices,
				InOutTriangles,
				InOutUVs);
		}

		return ArmSegmentCount + 1;
	}

	static int32 AppendIntersectionSplineTopologyMesh(
		const FQuick_RoadRoadIntersectionInfo& Intersection,
		float ExtendLengthCm,
		float ArmSampleSpacingCm,
		const FQuick_RoadRoadRibbonBuildParams& MeshParams,
		float CornerRadiusScale,
		TArray<FVector>& InOutVertices,
		TArray<int32>& InOutTriangles,
		TArray<FVector2D>& InOutUVs)
	{
		if (ExtendLengthCm <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}

		TArray<FIntersectionArm> Arms;
		if (CollectIntersectionArms(Intersection, ExtendLengthCm, MeshParams, CornerRadiusScale, Arms) < 2)
		{
			return 0;
		}

		AppendIntersectionJunctionFan(
			Intersection,
			Arms,
			MeshParams,
			InOutVertices,
			InOutTriangles,
			InOutUVs);

		int32 TotalSegments = 0;
		for (const FIntersectionArm& Arm : Arms)
		{
			TotalSegments += AppendIntersectionArmStrip(
				Arm,
				ArmSampleSpacingCm,
				MeshParams,
				InOutVertices,
				InOutTriangles,
				InOutUVs);
		}

		return TotalSegments + 1;
	}


	static void WorldToLocalVertices(
		const TArray<FVector>& WorldVertices,
		const FVector& Pivot,
		TArray<FVector>& OutLocalVertices)
	{
		OutLocalVertices.Reset();
		OutLocalVertices.Reserve(WorldVertices.Num());
		for (const FVector& Vertex : WorldVertices)
		{
			OutLocalVertices.Add(Vertex - Pivot);
		}
	}

	static void ApplyProcMeshSection(
		UProceduralMeshComponent* MeshComponent,
		const TArray<FVector>& WorldVertices,
		const FVector& Pivot,
		const TArray<int32>& Triangles,
		const TArray<FVector2D>& UVs,
		UMaterialInterface* Material)
	{
		if (!MeshComponent || Triangles.Num() < 3)
		{
			return;
		}

		TArray<FVector> LocalVertices;
		WorldToLocalVertices(WorldVertices, Pivot, LocalVertices);

		// 主路、路口臂、路口中心扇的三角绕序已在源头统一（见 BuildArmCrossRing /
		// AppendIntersectionJunctionFan），法线自然朝上，无需再翻正。
		TArray<FVector> Normals;
		TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
			LocalVertices,
			Triangles,
			UVs,
			Normals,
			Tangents);

		MeshComponent->Modify();
		MeshComponent->ClearAllMeshSections();
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
		if (Material)
		{
			MeshComponent->SetMaterial(0, Material);
		}
	}

	static void RemoveIntersectionPatchComponents(AActor* NetworkActor)
	{
		if (!NetworkActor)
		{
			return;
		}

		TArray<UQuick_RoadLayoutRoadMeshComponent*> PatchComponents;
		NetworkActor->GetComponents(PatchComponents);
		for (UQuick_RoadLayoutRoadMeshComponent* Patch : PatchComponents)
		{
			if (Patch && Patch->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch))
			{
				Patch->Modify();
				Patch->DestroyComponent();
			}
		}
	}

	static UQuick_RoadLayoutRoadMeshComponent* CreateIntersectionPatchComponent(
		AActor* NetworkActor,
		USceneComponent* RootComponent,
		int32 IntersectionIndex)
	{
		const FName ComponentName(*FString::Printf(TEXT("Intersection_%03d"), IntersectionIndex));
		UQuick_RoadLayoutRoadMeshComponent* PatchComponent = NewObject<UQuick_RoadLayoutRoadMeshComponent>(
			NetworkActor,
			UQuick_RoadLayoutRoadMeshComponent::StaticClass(),
			ComponentName,
			RF_Transactional);
		PatchComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		PatchComponent->SetMobility(EComponentMobility::Static);
		PatchComponent->SetupAttachment(RootComponent);
		PatchComponent->ComponentTags.Add(Quick_RoadLayoutRoadTags::IntersectionPatch);
		NetworkActor->AddInstanceComponent(PatchComponent);
		PatchComponent->RegisterComponent();
		return PatchComponent;
	}

	static UMaterialInterface* ResolveRoadMaterial(UObject* Outer, UMaterialInterface* OverrideMaterial = nullptr)
	{
		if (OverrideMaterial)
		{
			return OverrideMaterial;
		}

		if (UMaterialInterface* Material = Cast<UMaterialInterface>(
			FSoftObjectPath(Quick_RoadLayoutMeshTags::DefaultLayoutMeshMaterialPath).TryLoad()))
		{
			return Material;
		}

		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	static void BuildVertexAdjacency(int32 VertexCount, const TArray<int32>& Triangles, TArray<TArray<int32>>& OutAdjacency)
	{
		OutAdjacency.SetNum(VertexCount);
		for (int32 TriangleStart = 0; TriangleStart + 2 < Triangles.Num(); TriangleStart += 3)
		{
			const int32 Indices[3] = {
				Triangles[TriangleStart],
				Triangles[TriangleStart + 1],
				Triangles[TriangleStart + 2]
			};

			for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				const int32 A = Indices[EdgeIndex];
				const int32 B = Indices[(EdgeIndex + 1) % 3];
				OutAdjacency[A].AddUnique(B);
				OutAdjacency[B].AddUnique(A);
			}
		}
	}

	struct FBoundaryEdgeKey
	{
		int32 A = 0;
		int32 B = 0;

		FBoundaryEdgeKey() = default;

		FBoundaryEdgeKey(int32 InA, int32 InB)
		{
			if (InA <= InB)
			{
				A = InA;
				B = InB;
			}
			else
			{
				A = InB;
				B = InA;
			}
		}

		bool operator==(const FBoundaryEdgeKey& Other) const
		{
			return A == Other.A && B == Other.B;
		}

		friend uint32 GetTypeHash(const FBoundaryEdgeKey& Key)
		{
			return HashCombine(GetTypeHash(Key.A), GetTypeHash(Key.B));
		}
	};

	static float ComputePolylineLength(const TArray<FVector>& Points)
	{
		float Length = 0.f;
		for (int32 Index = 1; Index < Points.Num(); ++Index)
		{
			Length += FVector::Dist(Points[Index - 1], Points[Index]);
		}
		return Length;
	}

	struct FBoundaryPolyline
	{
		TArray<FVector> Points;
		bool bClosed = false;
	};

	static bool IsPolylineClosed(const TArray<FVector>& Points, float ToleranceCm)
	{
		return Points.Num() >= 3 &&
			FVector::Dist(Points[0], Points.Last()) <= ToleranceCm;
	}

	static double ComputeAbsoluteAreaXY(const TArray<FVector>& Points, bool bClosed)
	{
		if (Points.Num() < 3)
		{
			return 0.0;
		}

		const int32 Count = bClosed ? Points.Num() : Points.Num() - 1;
		double SignedArea = 0.0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D A(Points[Index].X, Points[Index].Y);
			const FVector2D B(Points[(Index + 1) % Points.Num()].X, Points[(Index + 1) % Points.Num()].Y);
			SignedArea += static_cast<double>(A.X * B.Y - B.X * A.Y);
		}

		return FMath::Abs(SignedArea * 0.5);
	}

	static int32 FindOrAddWeldedVertex(
		const FVector& WorldPosition,
		float ToleranceCm,
		TArray<FVector>& WeldedVertices,
		TMap<FIntVector, TArray<int32>>& SpatialGrid)
	{
		const float CellSize = FMath::Max(ToleranceCm, 0.1f);
		const FIntVector CenterCell(
			FMath::FloorToInt(WorldPosition.X / CellSize),
			FMath::FloorToInt(WorldPosition.Y / CellSize),
			FMath::FloorToInt(WorldPosition.Z / CellSize));
		const float ToleranceSq = FMath::Square(ToleranceCm);

		for (int32 OffsetZ = -1; OffsetZ <= 1; ++OffsetZ)
		{
			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const FIntVector Cell = CenterCell + FIntVector(OffsetX, OffsetY, OffsetZ);
					if (const TArray<int32>* Bucket = SpatialGrid.Find(Cell))
					{
						for (const int32 VertexIndex : *Bucket)
						{
							if (WeldedVertices.IsValidIndex(VertexIndex) &&
								FVector::DistSquared(WeldedVertices[VertexIndex], WorldPosition) <= ToleranceSq)
							{
								return VertexIndex;
							}
						}
					}
				}
			}
		}

		const int32 NewIndex = WeldedVertices.Add(WorldPosition);
		SpatialGrid.FindOrAdd(CenterCell).Add(NewIndex);
		return NewIndex;
	}

	static bool MergeRoadMeshComponents(
		const TArray<UQuick_RoadLayoutRoadMeshComponent*>& MeshComponents,
		float WeldToleranceCm,
		TArray<FVector>& OutWorldVertices,
		TArray<int32>& OutTriangles)
	{
		OutWorldVertices.Reset();
		OutTriangles.Reset();
		TMap<FIntVector, TArray<int32>> SpatialGrid;

		for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent)
			{
				continue;
			}

			const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
			const int32 SectionCount = MeshComponent->GetNumSections();
			for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
			{
				FProcMeshSection* Section = MeshComponent->GetProcMeshSection(SectionIndex);
				if (!Section || Section->ProcVertexBuffer.Num() < 3 || Section->ProcIndexBuffer.Num() < 3)
				{
					continue;
				}

				TArray<int32> RemappedIndices;
				RemappedIndices.Reserve(Section->ProcVertexBuffer.Num());
				for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
				{
					const FVector WorldPosition = ComponentToWorld.TransformPosition(Vertex.Position);
					RemappedIndices.Add(FindOrAddWeldedVertex(
						WorldPosition,
						WeldToleranceCm,
						OutWorldVertices,
						SpatialGrid));
				}

				for (int32 TriangleStart = 0; TriangleStart + 2 < static_cast<int32>(Section->ProcIndexBuffer.Num()); TriangleStart += 3)
				{
					const int32 I0 = RemappedIndices[Section->ProcIndexBuffer[TriangleStart]];
					const int32 I1 = RemappedIndices[Section->ProcIndexBuffer[TriangleStart + 1]];
					const int32 I2 = RemappedIndices[Section->ProcIndexBuffer[TriangleStart + 2]];
					if (I0 == I1 || I1 == I2 || I2 == I0)
					{
						continue;
					}

					OutTriangles.Add(I0);
					OutTriangles.Add(I1);
					OutTriangles.Add(I2);
				}
			}
		}

		return OutWorldVertices.Num() >= 3 && OutTriangles.Num() >= 3;
	}

	static bool SelectOutermostPolyline(
		const TArray<FBoundaryPolyline>& Candidates,
		float MinPolylineLengthCm,
		FBoundaryPolyline& OutPolyline)
	{
		int32 BestClosedIndex = INDEX_NONE;
		double BestClosedArea = 0.0;
		int32 BestOpenIndex = INDEX_NONE;
		float BestOpenLength = 0.f;

		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FBoundaryPolyline& Candidate = Candidates[Index];
			const float Length = ComputePolylineLength(Candidate.Points);
			if (Length < MinPolylineLengthCm)
			{
				continue;
			}

			if (Candidate.bClosed)
			{
				const double Area = ComputeAbsoluteAreaXY(Candidate.Points, true);
				if (Area > BestClosedArea)
				{
					BestClosedArea = Area;
					BestClosedIndex = Index;
				}
			}
			else if (Length > BestOpenLength)
			{
				BestOpenLength = Length;
				BestOpenIndex = Index;
			}
		}

		const int32 SelectedIndex = BestClosedIndex != INDEX_NONE ? BestClosedIndex : BestOpenIndex;
		if (SelectedIndex == INDEX_NONE)
		{
			return false;
		}

		OutPolyline = Candidates[SelectedIndex];
		return true;
	}

	static FVector2D ComputePolylineCentroid2D(const TArray<FVector>& Points)
	{
		if (Points.Num() == 0)
		{
			return FVector2D::ZeroVector;
		}

		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector& Point : Points)
		{
			Sum.X += Point.X;
			Sum.Y += Point.Y;
		}
		const float InvCount = 1.f / static_cast<float>(Points.Num());
		return Sum * InvCount;
	}

	static bool AreRingsGeometricallySimilar(
		const TArray<FVector>& RingA,
		const TArray<FVector>& RingB,
		float CenterThresholdCm,
		float AreaRatioTolerance)
	{
		if (RingA.Num() < 3 || RingB.Num() < 3)
		{
			return false;
		}

		const FVector2D CentroidA = ComputePolylineCentroid2D(RingA);
		const FVector2D CentroidB = ComputePolylineCentroid2D(RingB);
		if (FVector2D::DistSquared(CentroidA, CentroidB) > FMath::Square(CenterThresholdCm))
		{
			return false;
		}

		const double AreaA = ComputeAbsoluteAreaXY(RingA, true);
		const double AreaB = ComputeAbsoluteAreaXY(RingB, true);
		const double MaxArea = FMath::Max(AreaA, AreaB);
		if (MaxArea <= KINDA_SMALL_NUMBER)
		{
			return true;
		}

		const double MinArea = FMath::Min(AreaA, AreaB);
		return (MaxArea - MinArea) / MaxArea <= AreaRatioTolerance;
	}

	static void SelectInnerHolePolylines(
		const TArray<FBoundaryPolyline>& Candidates,
		float MinPolylineLengthCm,
		const FBoundaryPolyline* OutermostPolyline,
		TArray<FBoundaryPolyline>& OutInnerLoops)
	{
		OutInnerLoops.Reset();
		double OutermostArea = 0.0;
		if (OutermostPolyline && OutermostPolyline->bClosed && OutermostPolyline->Points.Num() >= 3)
		{
			OutermostArea = ComputeAbsoluteAreaXY(OutermostPolyline->Points, true);
		}

		for (const FBoundaryPolyline& Candidate : Candidates)
		{
			if (!Candidate.bClosed || Candidate.Points.Num() < 3)
			{
				continue;
			}

			const float Length = ComputePolylineLength(Candidate.Points);
			if (Length < MinPolylineLengthCm)
			{
				continue;
			}

			const double Area = ComputeAbsoluteAreaXY(Candidate.Points, true);
			if (OutermostArea > KINDA_SMALL_NUMBER && Area >= OutermostArea * 0.98)
			{
				continue;
			}

			OutInnerLoops.Add(Candidate);
		}
	}

	static void ExtractBoundaryPolylinesFromMesh(
		const TArray<FVector>& WorldVertices,
		const TArray<int32>& Triangles,
		float CloseToleranceCm,
		TArray<FBoundaryPolyline>& OutPolylines);

	static void CollectRoadMeshComponentsFromNetworkActor(
		AActor* NetworkActor,
		bool bIncludeIntersectionPatches,
		TArray<UQuick_RoadLayoutRoadMeshComponent*>& OutRoadMeshComponents)
	{
		OutRoadMeshComponents.Reset();
		if (!NetworkActor || !NetworkActor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			return;
		}

		TInlineComponentArray<UQuick_RoadLayoutRoadMeshComponent*> MeshComponents(NetworkActor);
		for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent)
			{
				continue;
			}

			if (!bIncludeIntersectionPatches &&
				MeshComponent->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch))
			{
				continue;
			}

			OutRoadMeshComponents.Add(MeshComponent);
		}
	}

	static bool ExtractInnerHoleBoundariesFromRoadMesh(
		AActor* RoadNetworkActor,
		float WeldToleranceCm,
		float MinInnerLoopLengthCm,
		bool bIncludeIntersectionPatches,
		TArray<FBoundaryPolyline>& OutInnerLoops)
	{
		OutInnerLoops.Reset();
		if (!RoadNetworkActor)
		{
			return false;
		}

		TArray<UQuick_RoadLayoutRoadMeshComponent*> RoadMeshComponents;
		CollectRoadMeshComponentsFromNetworkActor(
			RoadNetworkActor,
			bIncludeIntersectionPatches,
			RoadMeshComponents);

		if (RoadMeshComponents.Num() == 0)
		{
			return false;
		}

		TArray<FVector> MergedVertices;
		TArray<int32> MergedTriangles;
		if (!MergeRoadMeshComponents(
			RoadMeshComponents,
			WeldToleranceCm,
			MergedVertices,
			MergedTriangles))
		{
			return false;
		}

		TArray<FBoundaryPolyline> BoundaryPolylines;
		ExtractBoundaryPolylinesFromMesh(
			MergedVertices,
			MergedTriangles,
			WeldToleranceCm,
			BoundaryPolylines);

		FBoundaryPolyline OutermostPolyline;
		SelectOutermostPolyline(BoundaryPolylines, MinInnerLoopLengthCm, OutermostPolyline);

		SelectInnerHolePolylines(
			BoundaryPolylines,
			MinInnerLoopLengthCm,
			OutermostPolyline.bClosed ? &OutermostPolyline : nullptr,
			OutInnerLoops);

		return OutInnerLoops.Num() > 0;
	}

	static void AppendUniqueIntersectionRing(
		const TArray<FVector>& RingPoints,
		TArray<TArray<FVector>>& InOutRings)
	{
		if (RingPoints.Num() < 3)
		{
			return;
		}

		for (const TArray<FVector>& ExistingRing : InOutRings)
		{
			if (AreRingsGeometricallySimilar(ExistingRing, RingPoints, 300.f, 0.35f))
			{
				return;
			}
		}

		InOutRings.Add(RingPoints);
	}

	static void ExtractBoundaryPolylinesFromMesh(
		const TArray<FVector>& WorldVertices,
		const TArray<int32>& Triangles,
		float CloseToleranceCm,
		TArray<FBoundaryPolyline>& OutPolylines)
	{
		TMap<FBoundaryEdgeKey, int32> EdgeUseCount;
		for (int32 TriangleStart = 0; TriangleStart + 2 < Triangles.Num(); TriangleStart += 3)
		{
			const int32 I0 = Triangles[TriangleStart];
			const int32 I1 = Triangles[TriangleStart + 1];
			const int32 I2 = Triangles[TriangleStart + 2];
			EdgeUseCount.FindOrAdd(FBoundaryEdgeKey(I0, I1))++;
			EdgeUseCount.FindOrAdd(FBoundaryEdgeKey(I1, I2))++;
			EdgeUseCount.FindOrAdd(FBoundaryEdgeKey(I2, I0))++;
		}

		TSet<FBoundaryEdgeKey> UnusedBoundaryEdges;
		TMultiMap<int32, int32> BoundaryAdjacency;
		for (const TPair<FBoundaryEdgeKey, int32>& Pair : EdgeUseCount)
		{
			if (Pair.Value != 1)
			{
				continue;
			}

			UnusedBoundaryEdges.Add(Pair.Key);
			BoundaryAdjacency.Add(Pair.Key.A, Pair.Key.B);
			BoundaryAdjacency.Add(Pair.Key.B, Pair.Key.A);
		}

		while (!UnusedBoundaryEdges.IsEmpty())
		{
			FBoundaryEdgeKey StartEdge = *UnusedBoundaryEdges.CreateConstIterator();
			UnusedBoundaryEdges.Remove(StartEdge);

			TArray<int32> Chain;
			Chain.Add(StartEdge.A);
			Chain.Add(StartEdge.B);
			bool bClosed = false;

			int32 Current = StartEdge.B;
			while (true)
			{
				int32 NextVertex = INDEX_NONE;
				for (auto AdjIt = BoundaryAdjacency.CreateKeyIterator(Current); AdjIt; ++AdjIt)
				{
					const FBoundaryEdgeKey CandidateEdge(Current, AdjIt.Value());
					if (UnusedBoundaryEdges.Contains(CandidateEdge))
					{
						NextVertex = AdjIt.Value();
						UnusedBoundaryEdges.Remove(CandidateEdge);
						break;
					}
				}

				if (NextVertex == INDEX_NONE)
				{
					break;
				}

				if (NextVertex == Chain[0])
				{
					bClosed = true;
					break;
				}

				Chain.Add(NextVertex);
				Current = NextVertex;
			}

			if (!bClosed)
			{
				Current = StartEdge.A;
				while (true)
				{
					int32 PrevVertex = INDEX_NONE;
					for (auto AdjIt = BoundaryAdjacency.CreateKeyIterator(Current); AdjIt; ++AdjIt)
					{
						const FBoundaryEdgeKey CandidateEdge(Current, AdjIt.Value());
						if (UnusedBoundaryEdges.Contains(CandidateEdge))
						{
							PrevVertex = AdjIt.Value();
							UnusedBoundaryEdges.Remove(CandidateEdge);
							break;
						}
					}

					if (PrevVertex == INDEX_NONE)
					{
						break;
					}

					Chain.Insert(PrevVertex, 0);
					Current = PrevVertex;
				}
			}

			if (!bClosed && Chain.Num() >= 3)
			{
				for (auto AdjIt = BoundaryAdjacency.CreateKeyIterator(Chain[0]); AdjIt; ++AdjIt)
				{
					if (AdjIt.Value() == Chain.Last())
					{
						bClosed = true;
						break;
					}
				}
			}

			TArray<FVector> Polyline;
			Polyline.Reserve(Chain.Num());
			for (const int32 VertexIndex : Chain)
			{
				if (WorldVertices.IsValidIndex(VertexIndex))
				{
					Polyline.Add(WorldVertices[VertexIndex]);
				}
			}

			if (Polyline.Num() < 2)
			{
				continue;
			}

			FBoundaryPolyline BoundaryPolyline;
			BoundaryPolyline.bClosed = bClosed || IsPolylineClosed(Polyline, CloseToleranceCm);
			if (BoundaryPolyline.bClosed && Polyline.Num() >= 2 &&
				Polyline.Last().Equals(Polyline[0], CloseToleranceCm))
			{
				Polyline.Pop();
			}
			BoundaryPolyline.Points = MoveTemp(Polyline);
			OutPolylines.Add(MoveTemp(BoundaryPolyline));
		}
	}

	static void ResampleSplineComponent(
		USplineComponent* SplineComponent,
		float SampleDistanceCm,
		ESplinePointType::Type SplinePointType)
	{
		if (!SplineComponent || SampleDistanceCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		SplineComponent->UpdateSpline();
		const float SplineLength = SplineComponent->GetSplineLength();
		if (SplineLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const bool bClosedLoop = SplineComponent->IsClosedLoop();
		const float MinDistance = FMath::Max(SampleDistanceCm, 10.f);
		TArray<FVector> ResampledPoints;
		ResampledPoints.Reserve(FMath::Max(FMath::CeilToInt(SplineLength / MinDistance) + 1, 2));
		ResampledPoints.Add(SplineComponent->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World));

		if (bClosedLoop)
		{
			for (float Distance = MinDistance; Distance < SplineLength - KINDA_SMALL_NUMBER; Distance += MinDistance)
			{
				ResampledPoints.Add(
					SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
			}
		}
		else
		{
			for (float Distance = MinDistance; Distance < SplineLength - MinDistance * 0.25f; Distance += MinDistance)
			{
				ResampledPoints.Add(
					SplineComponent->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World));
			}

			const FVector EndPoint = SplineComponent->GetLocationAtDistanceAlongSpline(
				SplineLength,
				ESplineCoordinateSpace::World);
			if (ResampledPoints.Num() == 0 ||
				FVector::Dist(ResampledPoints.Last(), EndPoint) > MinDistance * 0.25f)
			{
				ResampledPoints.Add(EndPoint);
			}
		}

		if (ResampledPoints.Num() < 2)
		{
			return;
		}

		SplineComponent->ClearSplinePoints(false);
		SplineComponent->SetSplinePoints(ResampledPoints, ESplineCoordinateSpace::World, false);
		SplineComponent->SetClosedLoop(bClosedLoop, false);
		for (int32 PointIndex = 0; PointIndex < SplineComponent->GetNumberOfSplinePoints(); ++PointIndex)
		{
			SplineComponent->SetSplinePointType(PointIndex, SplinePointType, false);
		}
		SplineComponent->bSplineHasBeenEdited = true;
		SplineComponent->UpdateSpline();
	}

	static AActor* SpawnLayoutSplineActor(
		UWorld* World,
		const TArray<FVector>& Polyline,
		bool bClosedLoop,
		ESplinePointType::Type SplinePointType,
		bool bEnableSplineResample,
		float SplineResampleDistanceCm,
		FName SplineKindTag,
		const FString& ActorLabel)
	{
		if (!World || Polyline.Num() < 2 || SplineKindTag.IsNone())
		{
			return nullptr;
		}

		const FVector PivotLocation = Polyline[0];
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags = RF_Transactional;

		AActor* SplineActor = World->SpawnActor<AActor>(
			AActor::StaticClass(),
			PivotLocation,
			FRotator::ZeroRotator,
			SpawnParams);
		if (!SplineActor)
		{
			return nullptr;
		}

		SplineActor->Modify();
		SplineActor->SetActorLabel(ActorLabel);
		SplineActor->Tags.AddUnique(Quick_RoadLayoutSplineTags::LayoutSpline);
		SplineActor->Tags.AddUnique(SplineKindTag);

		USplineComponent* SplineComponent = NewObject<USplineComponent>(
			SplineActor,
			USplineComponent::StaticClass(),
			TEXT("SplineComponent"),
			RF_Transactional);
		SplineComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		SplineComponent->SetMobility(EComponentMobility::Static);
		SplineComponent->SetRelativeTransform(FTransform::Identity);

		SplineActor->SetRootComponent(SplineComponent);
		SplineActor->AddInstanceComponent(SplineComponent);
		SplineComponent->RegisterComponent();
		SplineActor->SetActorLocation(PivotLocation, false, nullptr, ETeleportType::TeleportPhysics);

		SplineComponent->Modify();
		SplineComponent->ClearSplinePoints(false);
		SplineComponent->SetSplinePoints(Polyline, ESplineCoordinateSpace::World, false);
		SplineComponent->SetClosedLoop(bClosedLoop, false);
		for (int32 PointIndex = 0; PointIndex < SplineComponent->GetNumberOfSplinePoints(); ++PointIndex)
		{
			SplineComponent->SetSplinePointType(PointIndex, SplinePointType, false);
		}
		SplineComponent->bSplineHasBeenEdited = true;
		SplineComponent->UpdateSpline();

		if (bEnableSplineResample)
		{
			ResampleSplineComponent(SplineComponent, SplineResampleDistanceCm, SplinePointType);
		}

		SplineComponent->ComponentTags.AddUnique(Quick_RoadLayoutSplineTags::LayoutSpline);
		SplineComponent->ComponentTags.AddUnique(SplineKindTag);

		return SplineActor;
	}

	static int32 GetNextRoadEdgeSplineIndex(UWorld* World)
	{
		int32 MaxIndex = 0;
		if (!World)
		{
			return 1;
		}

		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			const AActor* Actor = *ActorIt;
			if (!Actor)
			{
				continue;
			}

			const FString Label = Actor->GetActorLabel();
			int32 ParsedIndex = 0;
			if (Label.StartsWith(TEXT("RoadEdge_")))
			{
				ParsedIndex = FCString::Atoi(*Label.Mid(9));
			}
			else if (Label.StartsWith(TEXT("IntersectionSpline_")))
			{
				ParsedIndex = FCString::Atoi(*Label.Mid(19));
			}
			else
			{
				continue;
			}

			MaxIndex = FMath::Max(MaxIndex, ParsedIndex);
		}

		return MaxIndex + 1;
	}

	static AActor* SpawnRoadEdgeSplineActor(
		UWorld* World,
		const TArray<FVector>& Polyline,
		bool bClosedLoop,
		ESplinePointType::Type SplinePointType,
		bool bEnableSplineResample,
		float SplineResampleDistanceCm,
		int32 SplineIndex)
	{
		return SpawnLayoutSplineActor(
			World,
			Polyline,
			bClosedLoop,
			SplinePointType,
			bEnableSplineResample,
			SplineResampleDistanceCm,
			Quick_RoadLayoutSplineTags::RoadEdge,
			FString::Printf(TEXT("RoadEdge_%03d"), SplineIndex));
	}

	struct FRoadNetworkSceneSettings
	{
		AActor* NetworkActor = nullptr;
		FName RoadTag = NAME_None;
		FQuick_RoadRoadRibbonBuildParams RibbonParams;
		float ExtendLengthCm = 800.f;
	};

	static bool ResolveRoadNetworkSceneSettings(UWorld* World, FRoadNetworkSceneSettings& OutSettings)
	{
		OutSettings = FRoadNetworkSceneSettings();
		if (!World)
		{
			return false;
		}

		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
			{
				OutSettings.NetworkActor = Actor;
				break;
			}
		}

		if (!OutSettings.NetworkActor)
		{
			return false;
		}

		if (const UQuick_RoadRoadInfluenceComponent* InfluenceComponent =
			OutSettings.NetworkActor->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>())
		{
			OutSettings.RoadTag = InfluenceComponent->RoadSplineTag;
			OutSettings.RibbonParams = InfluenceComponent->RibbonParams;
			OutSettings.ExtendLengthCm = InfluenceComponent->ExtendLengthCm;
			if (OutSettings.RibbonParams.HalfWidthCm <= KINDA_SMALL_NUMBER)
			{
				OutSettings.RibbonParams.HalfWidthCm = InfluenceComponent->HalfWidthCm;
			}
		}

		return !OutSettings.RoadTag.IsNone();
	}

	static void ComputeNormals(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		TArray<FVector>& OutNormals)
	{
		OutNormals.Init(FVector::UpVector, Vertices.Num());
		TArray<FVector> NormalAccum;
		NormalAccum.Init(FVector::ZeroVector, Vertices.Num());

		for (int32 TriangleStart = 0; TriangleStart + 2 < Triangles.Num(); TriangleStart += 3)
		{
			const int32 I0 = Triangles[TriangleStart];
			const int32 I1 = Triangles[TriangleStart + 1];
			const int32 I2 = Triangles[TriangleStart + 2];
			if (!Vertices.IsValidIndex(I0) || !Vertices.IsValidIndex(I1) || !Vertices.IsValidIndex(I2))
			{
				continue;
			}

			const FVector FaceNormal = FVector::CrossProduct(
				Vertices[I1] - Vertices[I0],
				Vertices[I2] - Vertices[I0]).GetSafeNormal();
			NormalAccum[I0] += FaceNormal;
			NormalAccum[I1] += FaceNormal;
			NormalAccum[I2] += FaceNormal;
		}

		for (int32 Index = 0; Index < Vertices.Num(); ++Index)
		{
			OutNormals[Index] = NormalAccum[Index].GetSafeNormal();
			if (OutNormals[Index].IsNearlyZero())
			{
				OutNormals[Index] = FVector::UpVector;
			}
		}
	}
}

namespace Quick_RoadRoadLocals
{
	static bool ComputeTriangleBarycentric2D(
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

	static float ComputeEdgeFalloffWeight(float DistanceToEdgeCm, float FalloffCm)
	{
		if (FalloffCm <= KINDA_SMALL_NUMBER || DistanceToEdgeCm >= FalloffCm)
		{
			return 0.f;
		}

		return FMath::SmoothStep(0.f, 1.f, 1.f - DistanceToEdgeCm / FalloffCm);
	}

	static void GatherProcMeshSurfaceTriangles(
		const UProceduralMeshComponent* MeshComponent,
		TArray<FQuick_RoadRoadMeshSurfaceTriangle>& OutTriangles)
	{
		if (!MeshComponent)
		{
			return;
		}

		const FTransform ComponentToWorld = MeshComponent->GetComponentTransform();
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

				FQuick_RoadRoadMeshSurfaceTriangle Triangle;
				Triangle.A = ComponentToWorld.TransformPosition(Vertices[I0].Position);
				Triangle.B = ComponentToWorld.TransformPosition(Vertices[I1].Position);
				Triangle.C = ComponentToWorld.TransformPosition(Vertices[I2].Position);
				OutTriangles.Add(Triangle);
			}
		}
	}
}

void FQuick_RoadRoadMeshInfluenceField::BuildFromProcMeshComponents(
	const TArray<UProceduralMeshComponent*>& MeshComponents)
{
	Triangles.Reset();
	for (const UProceduralMeshComponent* MeshComponent : MeshComponents)
	{
		Quick_RoadRoadLocals::GatherProcMeshSurfaceTriangles(MeshComponent, Triangles);
	}
}

FQuick_RoadRoadCorridorQuery FQuick_RoadRoadMeshInfluenceField::Query(
	const FVector2D& WorldXY,
	float InfluenceFalloffCm) const
{
	FQuick_RoadRoadCorridorQuery Result;
	if (Triangles.Num() == 0)
	{
		return Result;
	}

	const float FalloffCm = FMath::Max(InfluenceFalloffCm, 0.f);
	bool bInsideAnyTriangle = false;
	float BestInsideZ = TNumericLimits<float>::Lowest();
	float MinInsideBoundaryDistance = TNumericLimits<float>::Max();
	float MinOutsideDistance = TNumericLimits<float>::Max();
	float BestOutsideZ = 0.f;

	for (const FQuick_RoadRoadMeshSurfaceTriangle& Triangle : Triangles)
	{
		const FVector2D Vertices2D[3] = {
			FVector2D(Triangle.A),
			FVector2D(Triangle.B),
			FVector2D(Triangle.C)
		};
		const float VertexZ[3] = { Triangle.A.Z, Triangle.B.Z, Triangle.C.Z };

		FVector Barycentric;
		if (Quick_RoadRoadLocals::ComputeTriangleBarycentric2D(
			WorldXY,
			Vertices2D[0],
			Vertices2D[1],
			Vertices2D[2],
			Barycentric))
		{
			bInsideAnyTriangle = true;
			const float InsideZ =
				Barycentric.X * VertexZ[0] +
				Barycentric.Y * VertexZ[1] +
				Barycentric.Z * VertexZ[2];
			BestInsideZ = FMath::Max(BestInsideZ, InsideZ);

			for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
			{
				const int32 StartIndex = EdgeIndex;
				const int32 EndIndex = (EdgeIndex + 1) % 3;
				float ProjectionAlpha = 0.f;
				const float BoundaryDistance = Quick_RoadRoadLocals::DistancePointToSegment2D(
					WorldXY,
					Vertices2D[StartIndex],
					Vertices2D[EndIndex],
					ProjectionAlpha);
				MinInsideBoundaryDistance = FMath::Min(MinInsideBoundaryDistance, BoundaryDistance);
			}
			continue;
		}

		for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
		{
			const int32 StartIndex = EdgeIndex;
			const int32 EndIndex = (EdgeIndex + 1) % 3;
			float ProjectionAlpha = 0.f;
			const float EdgeDistance = Quick_RoadRoadLocals::DistancePointToSegment2D(
				WorldXY,
				Vertices2D[StartIndex],
				Vertices2D[EndIndex],
				ProjectionAlpha);
			if (EdgeDistance < MinOutsideDistance)
			{
				MinOutsideDistance = EdgeDistance;
				BestOutsideZ = FMath::Lerp(VertexZ[StartIndex], VertexZ[EndIndex], ProjectionAlpha);
			}
		}
	}

	if (bInsideAnyTriangle)
	{
		Result.Weight = 1.f;
		Result.TargetWorldZ = BestInsideZ;
		Result.DistanceToCenterCm = MinInsideBoundaryDistance;
		return Result;
	}

	if (MinOutsideDistance <= FalloffCm)
	{
		Result.Weight = Quick_RoadRoadLocals::ComputeEdgeFalloffWeight(MinOutsideDistance, FalloffCm);
		Result.TargetWorldZ = BestOutsideZ;
		Result.DistanceToCenterCm = MinOutsideDistance;
	}

	return Result;
}

void FQuick_RoadRoadCorridorField::BuildFromSplines(
	const TArray<USplineComponent*>& Splines,
	float SampleDistanceCm)
{
	Segments.Reset();
	const float MinSampleDistance = FMath::Max(SampleDistanceCm, 25.f);

	for (USplineComponent* Spline : Splines)
	{
		if (!Spline)
		{
			continue;
		}

		const float SplineLength = Spline->GetSplineLength();
		if (SplineLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const int32 SampleCount = FMath::Max(FMath::CeilToInt(SplineLength / MinSampleDistance) + 1, 2);
		FVector PrevLocation = Spline->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World);
		for (int32 SampleIndex = 1; SampleIndex < SampleCount; ++SampleIndex)
		{
			const float Distance = (SampleCount > 1)
				? (SplineLength * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount - 1))
				: SplineLength;
			const FVector CurrentLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

			FQuick_RoadRoadCorridorSample Segment;
			Segment.Start = FVector2D(PrevLocation.X, PrevLocation.Y);
			Segment.End = FVector2D(CurrentLocation.X, CurrentLocation.Y);
			Segment.StartZ = PrevLocation.Z;
			Segment.EndZ = CurrentLocation.Z;
			Segments.Add(Segment);

			PrevLocation = CurrentLocation;
		}
	}
}

FQuick_RoadRoadCorridorQuery FQuick_RoadRoadCorridorField::Query(
	const FVector2D& WorldXY,
	float HalfWidthCm,
	float InfluenceFalloffCm) const
{
	FQuick_RoadRoadCorridorQuery Result;
	if (Segments.Num() == 0)
	{
		return Result;
	}

	float BestDistance = TNumericLimits<float>::Max();
	float BestTargetZ = 0.f;

	for (const FQuick_RoadRoadCorridorSample& Segment : Segments)
	{
		float ProjectionAlpha = 0.f;
		const float Distance = Quick_RoadRoadLocals::DistancePointToSegment2D(
			WorldXY,
			Segment.Start,
			Segment.End,
			ProjectionAlpha);

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestTargetZ = FMath::Lerp(Segment.StartZ, Segment.EndZ, ProjectionAlpha);
		}
	}

	Result.DistanceToCenterCm = BestDistance;
	Result.TargetWorldZ = BestTargetZ;
	Result.Weight = Quick_RoadRoadLocals::ComputeCorridorWeight(
		BestDistance,
		HalfWidthCm,
		InfluenceFalloffCm);

	return Result;
}

namespace Quick_RoadLayoutRoadLocals
{
	static bool SplineHasUserRoadTag(const AActor* SplineActor, const USplineComponent* SplineComponent, FName UserRoadTag)
	{
		return Quick_RoadLayoutRoadTags::IsValidUserRoadTag(UserRoadTag)
			&& ((SplineActor && SplineActor->ActorHasTag(UserRoadTag))
				|| (SplineComponent && SplineComponent->ComponentHasTag(UserRoadTag)));
	}
}

void FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplinesByTagExpression(
	UWorld* World,
	TArray<USplineComponent*>& OutSplines,
	FName RoadTagExpression)
{
	OutSplines.Reset();
	if (!World || !Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		return;
	}

	TArray<FName> UserRoadTags;
	Quick_RoadLayoutRoadTags::ParseRoadTagExpression(RoadTagExpression, UserRoadTags);

	TSet<USplineComponent*> SeenSplines;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			continue;
		}

		TArray<USplineComponent*> SplineComponents;
		Actor->GetComponents<USplineComponent>(SplineComponents);
		for (USplineComponent* SplineComponent : SplineComponents)
		{
			if (!SplineComponent || SeenSplines.Contains(SplineComponent))
			{
				continue;
			}

			if (!Quick_RoadLayoutRoadTags::SplineHasAnyUserRoadTag(Actor, SplineComponent, UserRoadTags))
			{
				continue;
			}

			SeenSplines.Add(SplineComponent);
			OutSplines.Add(SplineComponent);
		}
	}
}

void FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplines(
	UWorld* World,
	TArray<USplineComponent*>& OutSplines,
	FName RoadTag)
{
	OutSplines.Reset();
	if (!World || !Quick_RoadLayoutRoadTags::IsValidUserRoadTag(RoadTag))
	{
		return;
	}

	TSet<USplineComponent*> SeenSplines;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			continue;
		}

		TArray<USplineComponent*> SplineComponents;
		Actor->GetComponents<USplineComponent>(SplineComponents);
		for (USplineComponent* SplineComponent : SplineComponents)
		{
			if (!SplineComponent || SeenSplines.Contains(SplineComponent))
			{
				continue;
			}

			if (!Quick_RoadLayoutRoadLocals::SplineHasUserRoadTag(Actor, SplineComponent, RoadTag))
			{
				continue;
			}

			SeenSplines.Add(SplineComponent);
			OutSplines.Add(SplineComponent);
		}
	}
}

void FQuick_RoadLayoutRoadGenerator::CollectIntersectionSourceSplines(
	UWorld* World,
	TArray<USplineComponent*>& OutSplines)
{
	OutSplines.Reset();
	if (!World)
	{
		return;
	}

	const FName SourceTag = Quick_RoadLayoutRoadTags::IntersectionSourceSpline;
	TSet<USplineComponent*> SeenSplines;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		USplineComponent* SplineComponent = Actor->FindComponentByClass<USplineComponent>();
		if (!SplineComponent || SeenSplines.Contains(SplineComponent))
		{
			continue;
		}

		if (Actor->ActorHasTag(SourceTag) || SplineComponent->ComponentHasTag(SourceTag))
		{
			SeenSplines.Add(SplineComponent);
			OutSplines.Add(SplineComponent);
		}
	}
}

void FQuick_RoadLayoutRoadGenerator::TagSplinesAsIntersectionSource(
	const TArray<USplineComponent*>& Splines,
	float DefaultHalfWidthCm)
{
	const FName SourceTag = Quick_RoadLayoutRoadTags::IntersectionSourceSpline;
	const float ClampedHalfWidthCm = FMath::Max(DefaultHalfWidthCm, 50.f);

	for (USplineComponent* Spline : Splines)
	{
		if (!Spline)
		{
			continue;
		}

		const bool bAlreadySource =
			Spline->ComponentHasTag(SourceTag)
			|| (Spline->GetOwner() && Spline->GetOwner()->ActorHasTag(SourceTag));

		Spline->Modify();
		Spline->ComponentTags.AddUnique(SourceTag);

		if (AActor* Owner = Spline->GetOwner())
		{
			Owner->Modify();
			Owner->Tags.AddUnique(SourceTag);
		}

		if (!bAlreadySource || !UQuick_RoadRoadSplineWidthComponent::HasStoredHalfWidthCm(Spline))
		{
			UQuick_RoadRoadSplineWidthComponent::SetHalfWidthCm(Spline, ClampedHalfWidthCm);
		}
	}
}

bool FQuick_RoadLayoutRoadGenerator::GenerateRoadMeshes(
	UWorld* World,
	const FQuick_RoadLayoutMainRoadStageParams& StageParams,
	const FQuick_RoadLayoutMainRoadIntersectionParams& IntersectionParams,
	TArray<FQuick_RoadLayoutRoadResult>& OutResults,
	FText& OutErrorMessage)
{
	OutResults.Reset();
	if (!World)
	{
		OutErrorMessage = LOCTEXT("GenerateNoWorld", "当前世界无效。");
		return false;
	}

	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTag(StageParams.RoadTag))
	{
		OutErrorMessage = LOCTEXT("GenerateNoRoadTag", "请先在「道路」的「道路 Tag」中填写有效 Tag。");
		return false;
	}

	const FName UserRoadTag = StageParams.RoadTag;

	TArray<USplineComponent*> Splines;
	CollectMainRoadSplines(World, Splines, UserRoadTag);
	if (Splines.Num() == 0)
	{
		OutErrorMessage = FText::Format(
			LOCTEXT("GenerateNoSplines", "场景中未找到带有 Tag「{0}」的样条线。"),
			FText::FromName(UserRoadTag));
		return false;
	}

	const float RoadWidthCm = FMath::Max(StageParams.MainRoadWidthCm, 100.f);
	const float HalfWidthCm = RoadWidthCm * 0.5f;
	const float InfluenceSampleDistanceCm = FMath::Max(StageParams.SampleDistanceCm, 25.f);

	FQuick_RoadRoadRibbonBuildParams RibbonParams = MakeRibbonBuildParamsFromStage(StageParams);

	TagSplinesAsIntersectionSource(Splines, HalfWidthCm);

	TArray<FVector> WorldVertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;

	int32 TotalRibbonSegments = 0;
	for (USplineComponent* Spline : Splines)
	{
		const FQuick_RoadRoadRibbonBuildParams SplineRibbonParams =
			Quick_RoadRoadLocals::MakeRibbonParamsForSpline(Spline, RibbonParams);
		TotalRibbonSegments += Quick_RoadRoadLocals::AppendRibbonStripForSpline(
			Spline,
			SplineRibbonParams,
			WorldVertices,
			Triangles,
			UVs);
	}

	if (Triangles.Num() < 3)
	{
		OutErrorMessage = LOCTEXT("GenerateNoMeshData", "无法从样条线生成道路网格，请检查样条长度与路宽。");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("GenerateMainRoadNetwork", "Generate Main Road Network"));

	TArray<AActor*> ExistingNetworkActors;
	CollectRoadNetworkActorsByTag(World, UserRoadTag, ExistingNetworkActors);
	for (AActor* ExistingNetworkActor : ExistingNetworkActors)
	{
		if (ExistingNetworkActor)
		{
			ExistingNetworkActor->Modify();
			World->DestroyActor(ExistingNetworkActor);
		}
	}

	int32 ExistingNetworkCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			++ExistingNetworkCount;
		}
	}

	FVector NetworkPivot = WorldVertices[0];

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;

	AActor* NetworkActor = World->SpawnActor<AActor>(
		AActor::StaticClass(),
		NetworkPivot,
		FRotator::ZeroRotator,
		SpawnParams);
	if (!NetworkActor)
	{
		OutErrorMessage = LOCTEXT("GenerateSpawnFailed", "无法创建道路网络 Actor。");
		return false;
	}

	NetworkActor->Modify();
	NetworkActor->SetActorLabel(FString::Printf(TEXT("MainRoadNetwork_%03d"), ExistingNetworkCount + 1));
	NetworkActor->Tags.AddUnique(Quick_RoadLayoutRoadTags::MainRoadNetwork);
	NetworkActor->Tags.AddUnique(Quick_RoadLayoutRoadTags::MainRoadMesh);

	USceneComponent* RootComponent = NewObject<USceneComponent>(
		NetworkActor,
		USceneComponent::StaticClass(),
		TEXT("Root"),
		RF_Transactional);
	RootComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	RootComponent->SetMobility(EComponentMobility::Static);
	NetworkActor->SetRootComponent(RootComponent);
	NetworkActor->AddInstanceComponent(RootComponent);
	RootComponent->RegisterComponent();
	NetworkActor->SetActorLocation(NetworkPivot, false, nullptr, ETeleportType::TeleportPhysics);

	UQuick_RoadLayoutRoadMeshComponent* RoadMeshComponent = NewObject<UQuick_RoadLayoutRoadMeshComponent>(
		NetworkActor,
		UQuick_RoadLayoutRoadMeshComponent::StaticClass(),
		TEXT("RoadMesh"),
		RF_Transactional);
	RoadMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	RoadMeshComponent->SetMobility(EComponentMobility::Static);
	RoadMeshComponent->SetupAttachment(RootComponent);
	NetworkActor->AddInstanceComponent(RoadMeshComponent);
	RoadMeshComponent->RegisterComponent();

	TArray<FVector> LocalVertices;
	LocalVertices.Reserve(WorldVertices.Num());
	for (const FVector& Vertex : WorldVertices)
	{
		LocalVertices.Add(Vertex - NetworkPivot);
	}

	TArray<FVector> Normals;
	Quick_RoadRoadLocals::ComputeNormals(LocalVertices, Triangles, Normals);
	TArray<FProcMeshTangent> Tangents;
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
		LocalVertices,
		Triangles,
		UVs,
		Normals,
		Tangents);

	RoadMeshComponent->Modify();
	RoadMeshComponent->CreateMeshSection(
		0,
		LocalVertices,
		Triangles,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents,
		true);
	RoadMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (UMaterialInterface* RoadMaterial = Quick_RoadRoadLocals::ResolveRoadMaterial(
		RoadMeshComponent,
		StageParams.DefaultRoadMaterial))
	{
		RoadMeshComponent->SetMaterial(0, RoadMaterial);
	}

	UQuick_RoadRoadInfluenceComponent* InfluenceComponent = NewObject<UQuick_RoadRoadInfluenceComponent>(
		NetworkActor,
		UQuick_RoadRoadInfluenceComponent::StaticClass(),
		TEXT("RoadInfluence"),
		RF_Transactional);
	InfluenceComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	NetworkActor->AddInstanceComponent(InfluenceComponent);
	InfluenceComponent->RegisterComponent();
	InfluenceComponent->HalfWidthCm = HalfWidthCm;
	InfluenceComponent->InfluenceFalloffCm = StageParams.InfluenceFalloffCm;
	InfluenceComponent->BlendStrength = StageParams.InfluenceBlendStrength;
	InfluenceComponent->SampleDistanceCm = InfluenceSampleDistanceCm;
	InfluenceComponent->RoadSplineTag = UserRoadTag;
	InfluenceComponent->ExtendLengthCm = FMath::Max(IntersectionParams.ExtendLengthCm, 50.f);
	InfluenceComponent->IntersectionDetectSampleCm =
		FMath::Max(IntersectionParams.IntersectionDetectSampleCm, 25.f);
	InfluenceComponent->RibbonParams = RibbonParams;

	FQuick_RoadLayoutRoadResult Result;
	Result.RoadActor = NetworkActor;
	Result.SplineCount = Splines.Num();
	Result.RibbonSegmentCount = TotalRibbonSegments;
	Result.VertexCount = LocalVertices.Num();
	Result.TriangleCount = Triangles.Num() / 3;
	OutResults.Add(Result);

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 道路网络：Tag=%s，删除旧网络 %d 个，样条 %d，Ribbon 段 %d，顶点 %d，三角面 %d"),
		*UserRoadTag.ToString(),
		ExistingNetworkActors.Num(),
		Result.SplineCount,
		Result.RibbonSegmentCount,
		Result.VertexCount,
		Result.TriangleCount);

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(NetworkActor, true, true, true);
	}
#endif

	return true;
}

FQuick_RoadRoadInfluenceParams FQuick_RoadLayoutRoadGenerator::MakeInfluenceParamsFromStage(
	const FQuick_RoadLayoutMainRoadStageParams& StageParams)
{
	FQuick_RoadRoadInfluenceParams Params;
	Params.HalfWidthCm = FMath::Max(StageParams.MainRoadWidthCm, 100.f) * 0.5f;
	Params.InfluenceFalloffCm = FMath::Max(StageParams.InfluenceFalloffCm, 0.f);
	Params.BlendStrength = FMath::Clamp(StageParams.InfluenceBlendStrength, 0.f, 1.f);
	return Params;
}

FQuick_RoadRoadRibbonBuildParams FQuick_RoadLayoutRoadGenerator::MakeRibbonBuildParamsFromStage(
	const FQuick_RoadLayoutMainRoadStageParams& StageParams)
{
	FQuick_RoadRoadRibbonBuildParams Params;
	Params.HalfWidthCm = FMath::Max(StageParams.MainRoadWidthCm, 100.f) * 0.5f;
	Params.SampleDistanceCm = FMath::Max(StageParams.RoadMeshSampleDistanceCm, 25.f);
	Params.WidthSubdivisions = FMath::Clamp(StageParams.RoadMeshWidthSubdivisions, 1, 16);
	Params.bAdaptiveCurvature = StageParams.bRoadMeshAdaptiveCurvature;
	Params.CurvatureThresholdDeg = FMath::Clamp(StageParams.RoadMeshCurvatureThresholdDeg, 1.f, 45.f);
	Params.MaxCurvatureSubdivisions = FMath::Clamp(StageParams.RoadMeshMaxCurvatureSubdivisions, 0, 6);
	return Params;
}

FQuick_RoadRoadSplitRebuildSettings FQuick_RoadLayoutRoadGenerator::MakeSplitRebuildSettingsFromStage(
	const FQuick_RoadLayoutMainRoadStageParams& StageParams,
	const FQuick_RoadLayoutMainRoadIntersectionParams& IntersectionParams)
{
	FQuick_RoadRoadSplitRebuildSettings Settings;
	Settings.ExtendLengthCm = FMath::Max(IntersectionParams.ExtendLengthCm, 50.f);
	Settings.IntersectionDetectSampleCm = FMath::Max(IntersectionParams.IntersectionDetectSampleCm, 25.f);
	Settings.RibbonParams = MakeRibbonBuildParamsFromStage(StageParams);
	Settings.IntersectionGridCellSizeCm = FMath::Max(IntersectionParams.IntersectionGridCellSizeCm, 10.f);
	Settings.bGridRebuildIntersectionPatches = IntersectionParams.bGridRebuildIntersectionPatches;
	Settings.IntersectionCornerRadiusScale = FMath::Clamp(IntersectionParams.IntersectionCornerRadiusScale, 0.25f, 4.f);
	return Settings;
}

void FQuick_RoadLayoutRoadGenerator::CollectRoadNetworkActorsByTag(
	UWorld* World,
	FName RoadTagExpression,
	TArray<AActor*>& OutActors)
{
	OutActors.Reset();
	if (!World)
	{
		return;
	}

	if (RoadTagExpression.IsNone())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
			{
				OutActors.Add(Actor);
			}
		}
		return;
	}

	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		return;
	}

	TArray<FName> UserRoadTags;
	Quick_RoadLayoutRoadTags::ParseRoadTagExpression(RoadTagExpression, UserRoadTags);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
		{
			continue;
		}

		const UQuick_RoadRoadInfluenceComponent* Influence =
			Actor->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>();
		if (Influence && UserRoadTags.Contains(Influence->RoadSplineTag))
		{
			OutActors.Add(Actor);
		}
	}
}

int32 FQuick_RoadLayoutRoadGenerator::DetectMainRoadIntersections(
	UWorld* World,
	FName RoadTagExpression,
	float DetectSampleDistanceCm,
	TArray<FQuick_RoadRoadIntersectionInfo>& OutIntersections)
{
	OutIntersections.Reset();
	if (!World || !Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		return 0;
	}

	TArray<USplineComponent*> Splines;
	CollectMainRoadSplinesByTagExpression(World, Splines, RoadTagExpression);
	if (Splines.Num() < 2)
	{
		return 0;
	}

	TArray<Quick_RoadRoadLocals::FRoadPolyLineSegment> Segments;
	Quick_RoadRoadLocals::BuildPolyLineSegments(Splines, DetectSampleDistanceCm, Segments);
	Quick_RoadRoadLocals::DetectIntersectionsInternal(
		Segments,
		Quick_RoadRoadLocals::IntersectionMergeThresholdCm,
		OutIntersections);

	return OutIntersections.Num();
}

bool FQuick_RoadLayoutRoadGenerator::SplitAndRebuildRoadNetwork(
	AActor* NetworkActor,
	FName RoadTagExpression,
	const FQuick_RoadRoadSplitRebuildSettings& Settings,
	FText& OutErrorMessage,
	int32* OutIntersectionPatchCount,
	int32* OutRibbonSegmentCount)
{
	if (!NetworkActor)
	{
		OutErrorMessage = LOCTEXT("SplitNoActor", "道路网络 Actor 无效。");
		return false;
	}

	UWorld* World = NetworkActor->GetWorld();
	if (!World)
	{
		OutErrorMessage = LOCTEXT("SplitNoWorld", "世界无效。");
		return false;
	}

	USceneComponent* RootComponent = NetworkActor->GetRootComponent();
	if (!RootComponent)
	{
		OutErrorMessage = LOCTEXT("SplitNoRoot", "MainRoadNetwork 缺少 Root 组件。");
		return false;
	}

	UQuick_RoadLayoutRoadMeshComponent* RoadMeshComponent = nullptr;
	TArray<UQuick_RoadLayoutRoadMeshComponent*> MeshComponents;
	NetworkActor->GetComponents(MeshComponents);
	for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent &&
			!MeshComponent->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch) &&
			MeshComponent->GetFName() == TEXT("RoadMesh"))
		{
			RoadMeshComponent = MeshComponent;
			break;
		}
	}
	if (!RoadMeshComponent)
	{
		for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent && !MeshComponent->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch))
			{
				RoadMeshComponent = MeshComponent;
				break;
			}
		}
	}
	if (!RoadMeshComponent)
	{
		OutErrorMessage = LOCTEXT("SplitNoRoadMesh", "未找到 RoadMesh 组件。");
		return false;
	}

	const UQuick_RoadRoadInfluenceComponent* InfluenceComponent =
		NetworkActor->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>();
	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		OutErrorMessage = LOCTEXT("SplitNoRoadSplineTag", "道路 Tag 表达式无效，请先填写如 aa 或 aa|dd。");
		return false;
	}

	TArray<USplineComponent*> Splines;
	CollectMainRoadSplinesByTagExpression(World, Splines, RoadTagExpression);
	if (Splines.Num() == 0)
	{
		OutErrorMessage = FText::Format(
			LOCTEXT("SplitNoSplines", "场景中未找到匹配 Tag 表达式「{0}」的样条线。请先点击「生成道路」。"),
			FText::FromName(RoadTagExpression));
		return false;
	}

	const float ExtendLengthCm = FMath::Max(Settings.ExtendLengthCm, 50.f);
	const float DetectSampleCm = FMath::Max(Settings.IntersectionDetectSampleCm, 25.f);
	FQuick_RoadRoadRibbonBuildParams RibbonParams = Settings.RibbonParams;
	RibbonParams.HalfWidthCm = FMath::Max(RibbonParams.HalfWidthCm, 50.f);
	if (InfluenceComponent && InfluenceComponent->RibbonParams.HalfWidthCm > KINDA_SMALL_NUMBER)
	{
		RibbonParams.HalfWidthCm = InfluenceComponent->RibbonParams.HalfWidthCm;
	}
	else if (InfluenceComponent && InfluenceComponent->HalfWidthCm > KINDA_SMALL_NUMBER)
	{
		RibbonParams.HalfWidthCm = InfluenceComponent->HalfWidthCm;
	}
	RibbonParams.SampleDistanceCm = FMath::Max(RibbonParams.SampleDistanceCm, 25.f);
	RibbonParams.WidthSubdivisions = FMath::Clamp(RibbonParams.WidthSubdivisions, 1, 16);

	TArray<FQuick_RoadRoadIntersectionInfo> Intersections;
	DetectMainRoadIntersections(World, RoadTagExpression, DetectSampleCm, Intersections);

	const FScopedTransaction Transaction(LOCTEXT("SplitRoadIntersections", "Split And Rebuild Road Intersections"));
	NetworkActor->Modify();
	Quick_RoadRoadLocals::RemoveIntersectionPatchComponents(NetworkActor);

	if (UQuick_RoadRoadInfluenceComponent* MutableInfluenceComponent =
		NetworkActor->FindComponentByClass<UQuick_RoadRoadInfluenceComponent>())
	{
		MutableInfluenceComponent->Modify();
		MutableInfluenceComponent->ExtendLengthCm = ExtendLengthCm;
		MutableInfluenceComponent->IntersectionDetectSampleCm = DetectSampleCm;
		MutableInfluenceComponent->RibbonParams = RibbonParams;
	}

	const FVector Pivot = NetworkActor->GetActorLocation();
	UMaterialInterface* RoadMaterial = RoadMeshComponent->GetMaterial(0);
	if (!RoadMaterial)
	{
		RoadMaterial = Quick_RoadRoadLocals::ResolveRoadMaterial(RoadMeshComponent, nullptr);
	}

	TArray<FVector> RoadVertices;
	TArray<int32> RoadTriangles;
	TArray<FVector2D> RoadUVs;
	int32 TotalRibbonSegments = 0;

	if (Intersections.Num() > 0)
	{
		for (USplineComponent* Spline : Splines)
		{
			const FQuick_RoadRoadRibbonBuildParams SplineRibbonParams =
				Quick_RoadRoadLocals::MakeRibbonParamsForSpline(Spline, RibbonParams);
			TotalRibbonSegments += Quick_RoadRoadLocals::AppendRibbonStripForSplineInternal(
				Spline,
				SplineRibbonParams,
				&Intersections,
				ExtendLengthCm,
				RoadVertices,
				RoadTriangles,
				RoadUVs);
		}
	}
	else
	{
		for (USplineComponent* Spline : Splines)
		{
			const FQuick_RoadRoadRibbonBuildParams SplineRibbonParams =
				Quick_RoadRoadLocals::MakeRibbonParamsForSpline(Spline, RibbonParams);
			TotalRibbonSegments += Quick_RoadRoadLocals::AppendRibbonStripForSpline(
				Spline,
				SplineRibbonParams,
				RoadVertices,
				RoadTriangles,
				RoadUVs);
		}
	}

	if (RoadTriangles.Num() < 3 && Intersections.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("SplitNoMeshData", "无法生成道路网格。");
		return false;
	}

	Quick_RoadRoadLocals::ApplyProcMeshSection(
		RoadMeshComponent,
		RoadVertices,
		Pivot,
		RoadTriangles,
		RoadUVs,
		RoadMaterial);

	int32 PatchCount = 0;
	for (int32 IntersectionIndex = 0; IntersectionIndex < Intersections.Num(); ++IntersectionIndex)
	{
		const FQuick_RoadRoadIntersectionInfo& Intersection = Intersections[IntersectionIndex];
		TArray<FVector> PatchVertices;
		TArray<int32> PatchTriangles;
		TArray<FVector2D> PatchUVs;

		int32 PatchSegmentCount = 0;
		if (Settings.bGridRebuildIntersectionPatches)
		{
			PatchSegmentCount = Quick_RoadRoadLocals::AppendIntersectionSplineTopologyMesh(
				Intersection,
				ExtendLengthCm,
				FMath::Max(Settings.IntersectionGridCellSizeCm, 10.f),
				RibbonParams,
				Settings.IntersectionCornerRadiusScale,
				PatchVertices,
				PatchTriangles,
				PatchUVs);
		}
		else
		{
			PatchSegmentCount = Quick_RoadRoadLocals::AppendIntersectionPatchMesh(
				Intersection,
				ExtendLengthCm,
				RibbonParams,
				Settings.IntersectionCornerRadiusScale,
				PatchVertices,
				PatchTriangles,
				PatchUVs);
		}

		if (PatchSegmentCount <= 0 || PatchTriangles.Num() < 3)
		{
			continue;
		}

		UQuick_RoadLayoutRoadMeshComponent* PatchComponent = Quick_RoadRoadLocals::CreateIntersectionPatchComponent(
			NetworkActor,
			RootComponent,
			PatchCount);
		Quick_RoadRoadLocals::ApplyProcMeshSection(
			PatchComponent,
			PatchVertices,
			Pivot,
			PatchTriangles,
			PatchUVs,
			RoadMaterial);
		++PatchCount;
	}

	if (OutIntersectionPatchCount)
	{
		*OutIntersectionPatchCount = PatchCount;
	}
	if (OutRibbonSegmentCount)
	{
		*OutRibbonSegmentCount = TotalRibbonSegments;
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 交叉口拆分：补丁 %d，路段 Ribbon 段 %d，延伸 %g cm"),
		PatchCount,
		TotalRibbonSegments,
		ExtendLengthCm);

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::SplitAndRebuildConsolidatedRoadNetworks(
	UWorld* World,
	FName RoadTagExpression,
	const FQuick_RoadRoadSplitRebuildSettings& Settings,
	FText& OutErrorMessage,
	int32* OutRemovedNetworkCount,
	int32* OutIntersectionPatchCount,
	int32* OutRibbonSegmentCount)
{
	if (OutRemovedNetworkCount)
	{
		*OutRemovedNetworkCount = 0;
	}

	if (!World)
	{
		OutErrorMessage = LOCTEXT("SplitConsolidatedNoWorld", "当前世界无效。");
		return false;
	}

	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		OutErrorMessage = LOCTEXT("SplitConsolidatedNoRoadTag", "请先在「交叉口」填写有效道路 Tag 或 Tag 表达式（如 aa|dd）。");
		return false;
	}

	TArray<AActor*> NetworkActors;
	CollectRoadNetworkActorsByTag(World, RoadTagExpression, NetworkActors);
	if (NetworkActors.Num() == 0)
	{
		OutErrorMessage = FText::Format(
			LOCTEXT("SplitConsolidatedNoNetwork", "未找到匹配 Tag 表达式「{0}」的道路网络。请先为各 Tag 分别点击「生成道路」。"),
			FText::FromName(RoadTagExpression));
		return false;
	}

	AActor* PrimaryNetwork = NetworkActors.Last();
	if (!PrimaryNetwork)
	{
		OutErrorMessage = LOCTEXT("SplitConsolidatedNoPrimary", "无法确定保留的道路网络 Actor。");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("SplitConsolidatedRoadNetworks", "Split And Consolidate Road Networks"));

	int32 PatchCount = 0;
	int32 RibbonSegments = 0;
	if (!SplitAndRebuildRoadNetwork(
		PrimaryNetwork,
		RoadTagExpression,
		Settings,
		OutErrorMessage,
		&PatchCount,
		&RibbonSegments))
	{
		return false;
	}

	int32 RemovedCount = 0;
	for (AActor* NetworkActor : NetworkActors)
	{
		if (NetworkActor && NetworkActor != PrimaryNetwork)
		{
			World->DestroyActor(NetworkActor);
			++RemovedCount;
		}
	}

	if (OutRemovedNetworkCount)
	{
		*OutRemovedNetworkCount = RemovedCount;
	}
	if (OutIntersectionPatchCount)
	{
		*OutIntersectionPatchCount = PatchCount;
	}
	if (OutRibbonSegmentCount)
	{
		*OutRibbonSegmentCount = RibbonSegments;
	}

#if WITH_EDITOR
	if (GEditor && PrimaryNetwork)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(PrimaryNetwork, true, true, true);
	}
#endif

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::ApplyRoadInfluenceDual(
	UWorld* World,
	const TArray<USplineComponent*>& RoadSplines,
	const FQuick_RoadRoadInfluenceParams& Params,
	FText& OutErrorMessage,
	int32* OutLandscapeHits,
	int32* OutLayoutHits)
{
	if (!World || RoadSplines.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("InfluenceInvalidInput", "道路样条无效。");
		return false;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
	if (!Landscape)
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			Landscape = *It;
			break;
		}
	}

	FQuick_RoadRoadCorridorField CorridorField;
	CorridorField.BuildFromSplines(RoadSplines, FMath::Max(Params.HalfWidthCm * 0.25f, 50.f));

	const float HalfWidthCm = FMath::Max(Params.HalfWidthCm, 50.f);
	const float FalloffCm = FMath::Max(Params.InfluenceFalloffCm, 0.f);
	const float Blend = FMath::Clamp(Params.BlendStrength, 0.f, 1.f);
	const float VerticalOffset = Params.VerticalOffsetCm;

	int32 LandscapeHits = 0;
	int32 LayoutHits = 0;

	const FScopedTransaction Transaction(LOCTEXT("ApplyRoadInfluenceDual", "Apply Road Influence"));

	if (Landscape)
	{
		FQuick_RoadLandscapeHeightBuffer Buffer;
		if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
		{
			return false;
		}

		TArray<float> OriginalHeights = Buffer.FloatHeights;
		for (int32 VertY = Buffer.MinY; VertY <= Buffer.MaxY; ++VertY)
		{
			for (int32 VertX = Buffer.MinX; VertX <= Buffer.MaxX; ++VertX)
			{
				const FVector WorldXY = Quick_RoadRoadLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);
				const FQuick_RoadRoadCorridorQuery Query = CorridorField.Query(
					FVector2D(WorldXY.X, WorldXY.Y),
					HalfWidthCm,
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const int32 LocalX = VertX - Buffer.MinX;
				const int32 LocalY = VertY - Buffer.MinY;
				const int32 Index = LocalY * Buffer.Width + LocalX;
				const float TargetLocalZ = Quick_RoadRoadLocals::WorldZToLandscapeLocalHeight(
					Landscape,
					Query.TargetWorldZ + VerticalOffset,
					WorldXY.X,
					WorldXY.Y);

				Buffer.FloatHeights[Index] = FMath::Lerp(
					OriginalHeights[Index],
					TargetLocalZ,
					Query.Weight * Blend);
				++LandscapeHits;
			}
		}

		if (LandscapeHits > 0)
		{
			if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
				Landscape,
				Buffer,
				LOCTEXT("RoadInfluenceLandscape", "Quick Road Road Influence Landscape")))
			{
				OutErrorMessage = LOCTEXT("InfluenceLandscapeWriteFailed", "写入 Landscape 高度失败。");
				return false;
			}
		}
	}

	AActor* LayoutActor = nullptr;
	UQuick_RoadLayoutMeshComponent* LayoutMesh = nullptr;
	if (FQuick_RoadLayoutMeshGenerator::FindCityScopeLayoutMesh(World, LayoutActor, LayoutMesh) && LayoutMesh)
	{
		FProcMeshSection* Section = LayoutMesh->GetProcMeshSection(0);
		if (Section && Section->ProcVertexBuffer.Num() > 0)
		{
			LayoutActor->Modify();
			LayoutMesh->Modify();

			const FTransform ComponentToWorld = LayoutMesh->GetComponentTransform();
			const FTransform WorldToComponent = ComponentToWorld.Inverse();

			TArray<FVector> LocalVertices;
			TArray<FVector2D> UVs;
			LocalVertices.Reserve(Section->ProcVertexBuffer.Num());
			UVs.Reserve(Section->ProcVertexBuffer.Num());

			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				LocalVertices.Add(Vertex.Position);
				UVs.Add(Vertex.UV0);
			}

			for (int32 VertexIndex = 0; VertexIndex < LocalVertices.Num(); ++VertexIndex)
			{
				FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				const FQuick_RoadRoadCorridorQuery Query = CorridorField.Query(
					FVector2D(WorldPosition.X, WorldPosition.Y),
					HalfWidthCm,
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				WorldPosition.Z = FMath::Lerp(
					WorldPosition.Z,
					Query.TargetWorldZ + VerticalOffset,
					Query.Weight * Blend);
				LocalVertices[VertexIndex] = WorldToComponent.TransformPosition(WorldPosition);
				++LayoutHits;
			}

			TArray<int32> Triangles;
			Triangles.Reserve(Section->ProcIndexBuffer.Num());
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				Triangles.Add(static_cast<int32>(Index));
			}

			TArray<FVector> Normals;
			Quick_RoadRoadLocals::ComputeNormals(LocalVertices, Triangles, Normals);
			TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);

			LayoutMesh->UpdateMeshSection(
				0,
				LocalVertices,
				Normals,
				UVs,
				TArray<FColor>(),
				Tangents);
		}
	}

	if (OutLandscapeHits)
	{
		*OutLandscapeHits = LandscapeHits;
	}
	if (OutLayoutHits)
	{
		*OutLayoutHits = LayoutHits;
	}

	if (LandscapeHits == 0 && LayoutHits == 0)
	{
		OutErrorMessage = LOCTEXT("InfluenceNoHits", "道路路带与 Landscape / 布局面片无交集。");
		return false;
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 道路影响：Landscape %d 点，LayoutMesh %d 点"),
		LandscapeHits,
		LayoutHits);

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::ApplyRoadInfluenceDualFromProcMeshes(
	UWorld* World,
	const TArray<UProceduralMeshComponent*>& RoadMeshes,
	const FQuick_RoadRoadInfluenceParams& Params,
	FText& OutErrorMessage,
	int32* OutLandscapeHits,
	int32* OutLayoutHits)
{
	if (!World || RoadMeshes.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("InfluenceInvalidProcMeshes", "道路网络 Actor 上没有可用的 ProcMesh。");
		return false;
	}

	FQuick_RoadRoadMeshInfluenceField MeshField;
	MeshField.BuildFromProcMeshComponents(RoadMeshes);
	if (MeshField.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("InfluenceEmptyProcMeshes", "道路 ProcMesh 没有有效三角面。");
		return false;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
	if (!Landscape)
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			Landscape = *It;
			break;
		}
	}

	const float FalloffCm = FMath::Max(Params.InfluenceFalloffCm, 0.f);
	const float Blend = FMath::Clamp(Params.BlendStrength, 0.f, 1.f);
	const float VerticalOffset = Params.VerticalOffsetCm;

	int32 LandscapeHits = 0;
	int32 LayoutHits = 0;

	const FScopedTransaction Transaction(LOCTEXT("ApplyRoadInfluenceFromProcMeshes", "Apply Road Influence From ProcMeshes"));

	if (Landscape)
	{
		FQuick_RoadLandscapeHeightBuffer Buffer;
		if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
		{
			return false;
		}

		TArray<float> OriginalHeights = Buffer.FloatHeights;
		for (int32 VertY = Buffer.MinY; VertY <= Buffer.MaxY; ++VertY)
		{
			for (int32 VertX = Buffer.MinX; VertX <= Buffer.MaxX; ++VertX)
			{
				const FVector WorldXY = Quick_RoadRoadLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);
				const FQuick_RoadRoadCorridorQuery Query = MeshField.Query(
					FVector2D(WorldXY.X, WorldXY.Y),
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const int32 LocalX = VertX - Buffer.MinX;
				const int32 LocalY = VertY - Buffer.MinY;
				const int32 Index = LocalY * Buffer.Width + LocalX;
				const float TargetLocalZ = Quick_RoadRoadLocals::WorldZToLandscapeLocalHeight(
					Landscape,
					Query.TargetWorldZ + VerticalOffset,
					WorldXY.X,
					WorldXY.Y);

				Buffer.FloatHeights[Index] = FMath::Lerp(
					OriginalHeights[Index],
					TargetLocalZ,
					Query.Weight * Blend);
				++LandscapeHits;
			}
		}

		if (LandscapeHits > 0)
		{
			if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
				Landscape,
				Buffer,
				LOCTEXT("RoadInfluenceLandscape", "Quick Road Road Influence Landscape")))
			{
				OutErrorMessage = LOCTEXT("InfluenceLandscapeWriteFailed", "写入 Landscape 高度失败。");
				return false;
			}
		}
	}

	AActor* LayoutActor = nullptr;
	UQuick_RoadLayoutMeshComponent* LayoutMesh = nullptr;
	if (FQuick_RoadLayoutMeshGenerator::FindCityScopeLayoutMesh(World, LayoutActor, LayoutMesh) && LayoutMesh)
	{
		FProcMeshSection* Section = LayoutMesh->GetProcMeshSection(0);
		if (Section && Section->ProcVertexBuffer.Num() > 0)
		{
			LayoutActor->Modify();
			LayoutMesh->Modify();

			const FTransform ComponentToWorld = LayoutMesh->GetComponentTransform();
			const FTransform WorldToComponent = ComponentToWorld.Inverse();

			TArray<FVector> LocalVertices;
			TArray<FVector2D> UVs;
			LocalVertices.Reserve(Section->ProcVertexBuffer.Num());
			UVs.Reserve(Section->ProcVertexBuffer.Num());

			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				LocalVertices.Add(Vertex.Position);
				UVs.Add(Vertex.UV0);
			}

			for (int32 VertexIndex = 0; VertexIndex < LocalVertices.Num(); ++VertexIndex)
			{
				FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				const FQuick_RoadRoadCorridorQuery Query = MeshField.Query(
					FVector2D(WorldPosition.X, WorldPosition.Y),
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				WorldPosition.Z = FMath::Lerp(
					WorldPosition.Z,
					Query.TargetWorldZ + VerticalOffset,
					Query.Weight * Blend);
				LocalVertices[VertexIndex] = WorldToComponent.TransformPosition(WorldPosition);
				++LayoutHits;
			}

			TArray<int32> Triangles;
			Triangles.Reserve(Section->ProcIndexBuffer.Num());
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				Triangles.Add(static_cast<int32>(Index));
			}

			TArray<FVector> Normals;
			Quick_RoadRoadLocals::ComputeNormals(LocalVertices, Triangles, Normals);
			TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);

			LayoutMesh->UpdateMeshSection(
				0,
				LocalVertices,
				Normals,
				UVs,
				TArray<FColor>(),
				Tangents);
		}
	}

	if (OutLandscapeHits)
	{
		*OutLandscapeHits = LandscapeHits;
	}
	if (OutLayoutHits)
	{
		*OutLayoutHits = LayoutHits;
	}

	if (LandscapeHits == 0 && LayoutHits == 0)
	{
		OutErrorMessage = LOCTEXT("InfluenceNoHits", "道路路带与 Landscape / 布局面片无交集。");
		return false;
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 道路影响（ProcMesh）：%d 个网格，Landscape %d 点，LayoutMesh %d 点"),
		RoadMeshes.Num(),
		LandscapeHits,
		LayoutHits);

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::SmoothRoadCorridorDual(
	UWorld* World,
	const TArray<USplineComponent*>& RoadSplines,
	const FQuick_RoadRoadInfluenceParams& Params,
	FText& OutErrorMessage)
{
	if (!World || RoadSplines.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("SmoothInvalidInput", "道路样条无效。");
		return false;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
	if (!Landscape)
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			Landscape = *It;
			break;
		}
	}

	FQuick_RoadRoadCorridorField CorridorField;
	CorridorField.BuildFromSplines(RoadSplines, FMath::Max(Params.HalfWidthCm * 0.25f, 50.f));

	const float HalfWidthCm = FMath::Max(Params.HalfWidthCm, 50.f);
	const float FalloffCm = FMath::Max(Params.InfluenceFalloffCm, 0.f);
	const float SmoothWidthCm = FMath::Max(FalloffCm, 50.f);
	const float SmoothStrength = FMath::Clamp(Params.EdgeSmoothStrength, 0.f, 1.f);
	const int32 Iterations = FMath::Clamp(Params.EdgeSmoothIterations, 1, 32);

	const FScopedTransaction Transaction(LOCTEXT("SmoothRoadCorridorDual", "Smooth Road Corridor Edge"));

	if (Landscape)
	{
		FQuick_RoadLandscapeHeightBuffer Buffer;
		if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
		{
			return false;
		}

		TArray<float> SmoothWeights;
		SmoothWeights.Init(0.f, Buffer.FloatHeights.Num());

		for (int32 VertY = Buffer.MinY; VertY <= Buffer.MaxY; ++VertY)
		{
			for (int32 VertX = Buffer.MinX; VertX <= Buffer.MaxX; ++VertX)
			{
				const FVector WorldXY = Quick_RoadRoadLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);
				const FQuick_RoadRoadCorridorQuery Query = CorridorField.Query(
					FVector2D(WorldXY.X, WorldXY.Y),
					HalfWidthCm,
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float EdgeDistance = FMath::Abs(Query.DistanceToCenterCm - HalfWidthCm);
				if (EdgeDistance > SmoothWidthCm)
				{
					continue;
				}

				const float Weight = SmoothStrength * (1.f - FMath::Clamp(EdgeDistance / SmoothWidthCm, 0.f, 1.f));
				const int32 Index = (VertY - Buffer.MinY) * Buffer.Width + (VertX - Buffer.MinX);
				SmoothWeights[Index] = Weight;
			}
		}

		TArray<float> Heights = Buffer.FloatHeights;
		const int32 Width = Buffer.Width;
		const int32 Height = Buffer.Height;
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

					if (Count > 0)
					{
						const float Average = static_cast<float>(Sum / static_cast<double>(Count));
						NextHeights[Index] = FMath::Lerp(Heights[Index], Average, Weight);
					}
				}
			}

			Heights = MoveTemp(NextHeights);
		}

		Buffer.FloatHeights = Heights;
		if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
			Landscape,
			Buffer,
			LOCTEXT("RoadSmoothLandscape", "Quick Road Smooth Road Edge Landscape")))
		{
			OutErrorMessage = LOCTEXT("SmoothLandscapeWriteFailed", "写入 Landscape 高度失败。");
			return false;
		}
	}

	AActor* LayoutActor = nullptr;
	UQuick_RoadLayoutMeshComponent* LayoutMesh = nullptr;
	if (FQuick_RoadLayoutMeshGenerator::FindCityScopeLayoutMesh(World, LayoutActor, LayoutMesh) && LayoutMesh)
	{
		FProcMeshSection* Section = LayoutMesh->GetProcMeshSection(0);
		if (Section && Section->ProcVertexBuffer.Num() > 0)
		{
			LayoutActor->Modify();
			LayoutMesh->Modify();

			const FTransform ComponentToWorld = LayoutMesh->GetComponentTransform();
			TArray<FVector> LocalVertices;
			TArray<FVector2D> UVs;
			TArray<float> WorldHeights;
			LocalVertices.Reserve(Section->ProcVertexBuffer.Num());
			UVs.Reserve(Section->ProcVertexBuffer.Num());
			WorldHeights.Reserve(Section->ProcVertexBuffer.Num());

			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				LocalVertices.Add(Vertex.Position);
				UVs.Add(Vertex.UV0);
				WorldHeights.Add(ComponentToWorld.TransformPosition(Vertex.Position).Z);
			}

			TArray<float> SmoothWeights;
			SmoothWeights.Init(0.f, WorldHeights.Num());
			for (int32 VertexIndex = 0; VertexIndex < WorldHeights.Num(); ++VertexIndex)
			{
				const FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				const FQuick_RoadRoadCorridorQuery Query = CorridorField.Query(
					FVector2D(WorldPosition.X, WorldPosition.Y),
					HalfWidthCm,
					FalloffCm);
				const float EdgeDistance = FMath::Abs(Query.DistanceToCenterCm - HalfWidthCm);
				if (EdgeDistance <= SmoothWidthCm && Query.Weight > KINDA_SMALL_NUMBER)
				{
					SmoothWeights[VertexIndex] = SmoothStrength * (1.f - FMath::Clamp(EdgeDistance / SmoothWidthCm, 0.f, 1.f));
				}
			}

			TArray<int32> Triangles;
			Triangles.Reserve(Section->ProcIndexBuffer.Num());
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				Triangles.Add(static_cast<int32>(Index));
			}

			TArray<TArray<int32>> Adjacency;
			Quick_RoadRoadLocals::BuildVertexAdjacency(LocalVertices.Num(), Triangles, Adjacency);

			for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
			{
				TArray<float> NextHeights = WorldHeights;
				for (int32 VertexIndex = 0; VertexIndex < WorldHeights.Num(); ++VertexIndex)
				{
					const float Weight = SmoothWeights[VertexIndex];
					if (Weight <= KINDA_SMALL_NUMBER || !Adjacency.IsValidIndex(VertexIndex))
					{
						continue;
					}

					double Sum = 0.0;
					int32 Count = 0;
					for (int32 NeighborIndex : Adjacency[VertexIndex])
					{
						Sum += WorldHeights[NeighborIndex];
						++Count;
					}

					if (Count > 0)
					{
						const float Average = static_cast<float>(Sum / static_cast<double>(Count));
						NextHeights[VertexIndex] = FMath::Lerp(WorldHeights[VertexIndex], Average, Weight);
					}
				}

				WorldHeights = MoveTemp(NextHeights);
			}

			const FTransform WorldToComponent = ComponentToWorld.Inverse();
			for (int32 VertexIndex = 0; VertexIndex < LocalVertices.Num(); ++VertexIndex)
			{
				FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				WorldPosition.Z = WorldHeights[VertexIndex];
				LocalVertices[VertexIndex] = WorldToComponent.TransformPosition(WorldPosition);
			}

			TArray<FVector> Normals;
			Quick_RoadRoadLocals::ComputeNormals(LocalVertices, Triangles, Normals);
			TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);
			LayoutMesh->UpdateMeshSection(0, LocalVertices, Normals, UVs, TArray<FColor>(), Tangents);
		}
	}

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::SmoothRoadCorridorDualFromProcMeshes(
	UWorld* World,
	const TArray<UProceduralMeshComponent*>& RoadMeshes,
	const FQuick_RoadRoadInfluenceParams& Params,
	FText& OutErrorMessage)
{
	if (!World || RoadMeshes.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("SmoothInvalidProcMeshes", "道路网络 Actor 上没有可用的 ProcMesh。");
		return false;
	}

	FQuick_RoadRoadMeshInfluenceField MeshField;
	MeshField.BuildFromProcMeshComponents(RoadMeshes);
	if (MeshField.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("SmoothEmptyProcMeshes", "道路 ProcMesh 没有有效三角面。");
		return false;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
	if (!Landscape)
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			Landscape = *It;
			break;
		}
	}

	const float FalloffCm = FMath::Max(Params.InfluenceFalloffCm, 0.f);
	const float SmoothWidthCm = FMath::Max(FalloffCm, 50.f);
	const float SmoothStrength = FMath::Clamp(Params.EdgeSmoothStrength, 0.f, 1.f);
	const int32 Iterations = FMath::Clamp(Params.EdgeSmoothIterations, 1, 32);

	const FScopedTransaction Transaction(LOCTEXT("SmoothRoadCorridorFromProcMeshes", "Smooth Road Edge From ProcMeshes"));

	if (Landscape)
	{
		FQuick_RoadLandscapeHeightBuffer Buffer;
		if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
		{
			return false;
		}

		TArray<float> SmoothWeights;
		SmoothWeights.Init(0.f, Buffer.FloatHeights.Num());

		for (int32 VertY = Buffer.MinY; VertY <= Buffer.MaxY; ++VertY)
		{
			for (int32 VertX = Buffer.MinX; VertX <= Buffer.MaxX; ++VertX)
			{
				const FVector WorldXY = Quick_RoadRoadLocals::LandscapeVertexToWorldXY(Landscape, VertX, VertY);
				const FQuick_RoadRoadCorridorQuery Query = MeshField.Query(
					FVector2D(WorldXY.X, WorldXY.Y),
					FalloffCm);
				if (Query.Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float EdgeDistance = Query.DistanceToCenterCm;
				if (EdgeDistance > SmoothWidthCm)
				{
					continue;
				}

				const float Weight = SmoothStrength * (1.f - FMath::Clamp(EdgeDistance / SmoothWidthCm, 0.f, 1.f));
				const int32 Index = (VertY - Buffer.MinY) * Buffer.Width + (VertX - Buffer.MinX);
				SmoothWeights[Index] = Weight;
			}
		}

		TArray<float> Heights = Buffer.FloatHeights;
		const int32 Width = Buffer.Width;
		const int32 Height = Buffer.Height;
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

					if (Count > 0)
					{
						const float Average = static_cast<float>(Sum / static_cast<double>(Count));
						NextHeights[Index] = FMath::Lerp(Heights[Index], Average, Weight);
					}
				}
			}

			Heights = MoveTemp(NextHeights);
		}

		Buffer.FloatHeights = MoveTemp(Heights);
		if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
			Landscape,
			Buffer,
			LOCTEXT("RoadSmoothLandscape", "Quick Road Smooth Road Edge Landscape")))
		{
			OutErrorMessage = LOCTEXT("SmoothLandscapeWriteFailed", "写入 Landscape 高度失败。");
			return false;
		}
	}

	AActor* LayoutActor = nullptr;
	UQuick_RoadLayoutMeshComponent* LayoutMesh = nullptr;
	if (FQuick_RoadLayoutMeshGenerator::FindCityScopeLayoutMesh(World, LayoutActor, LayoutMesh) && LayoutMesh)
	{
		FProcMeshSection* Section = LayoutMesh->GetProcMeshSection(0);
		if (Section && Section->ProcVertexBuffer.Num() > 0)
		{
			LayoutActor->Modify();
			LayoutMesh->Modify();

			const FTransform ComponentToWorld = LayoutMesh->GetComponentTransform();
			TArray<FVector> LocalVertices;
			TArray<FVector2D> UVs;
			TArray<float> WorldHeights;
			LocalVertices.Reserve(Section->ProcVertexBuffer.Num());
			UVs.Reserve(Section->ProcVertexBuffer.Num());
			WorldHeights.Reserve(Section->ProcVertexBuffer.Num());

			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				LocalVertices.Add(Vertex.Position);
				UVs.Add(Vertex.UV0);
				WorldHeights.Add(ComponentToWorld.TransformPosition(Vertex.Position).Z);
			}

			TArray<float> SmoothWeights;
			SmoothWeights.Init(0.f, WorldHeights.Num());
			for (int32 VertexIndex = 0; VertexIndex < WorldHeights.Num(); ++VertexIndex)
			{
				const FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				const FQuick_RoadRoadCorridorQuery Query = MeshField.Query(
					FVector2D(WorldPosition.X, WorldPosition.Y),
					FalloffCm);
				const float EdgeDistance = Query.DistanceToCenterCm;
				if (EdgeDistance <= SmoothWidthCm && Query.Weight > KINDA_SMALL_NUMBER)
				{
					SmoothWeights[VertexIndex] = SmoothStrength * (1.f - FMath::Clamp(EdgeDistance / SmoothWidthCm, 0.f, 1.f));
				}
			}

			TArray<int32> Triangles;
			Triangles.Reserve(Section->ProcIndexBuffer.Num());
			for (uint32 Index : Section->ProcIndexBuffer)
			{
				Triangles.Add(static_cast<int32>(Index));
			}

			TArray<TArray<int32>> Adjacency;
			Quick_RoadRoadLocals::BuildVertexAdjacency(LocalVertices.Num(), Triangles, Adjacency);

			for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
			{
				TArray<float> NextHeights = WorldHeights;
				for (int32 VertexIndex = 0; VertexIndex < WorldHeights.Num(); ++VertexIndex)
				{
					const float Weight = SmoothWeights[VertexIndex];
					if (Weight <= KINDA_SMALL_NUMBER || !Adjacency.IsValidIndex(VertexIndex))
					{
						continue;
					}

					double Sum = 0.0;
					int32 Count = 0;
					for (int32 NeighborIndex : Adjacency[VertexIndex])
					{
						Sum += WorldHeights[NeighborIndex];
						++Count;
					}

					if (Count > 0)
					{
						const float Average = static_cast<float>(Sum / static_cast<double>(Count));
						NextHeights[VertexIndex] = FMath::Lerp(WorldHeights[VertexIndex], Average, Weight);
					}
				}

				WorldHeights = MoveTemp(NextHeights);
			}

			const FTransform WorldToComponent = ComponentToWorld.Inverse();
			for (int32 VertexIndex = 0; VertexIndex < LocalVertices.Num(); ++VertexIndex)
			{
				FVector WorldPosition = ComponentToWorld.TransformPosition(LocalVertices[VertexIndex]);
				WorldPosition.Z = WorldHeights[VertexIndex];
				LocalVertices[VertexIndex] = WorldToComponent.TransformPosition(WorldPosition);
			}

			TArray<FVector> Normals;
			Quick_RoadRoadLocals::ComputeNormals(LocalVertices, Triangles, Normals);
			TArray<FProcMeshTangent> Tangents;
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(LocalVertices, Triangles, UVs, Normals, Tangents);
			LayoutMesh->UpdateMeshSection(0, LocalVertices, Normals, UVs, TArray<FColor>(), Tangents);
		}
	}

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::ExtractRoadEdgeSplines(
	UWorld* World,
	AActor* RoadNetworkActor,
	float MinPolylineLengthCm,
	float VertexWeldToleranceCm,
	bool bIncludeIntersectionPatches,
	ESplinePointType::Type SplinePointType,
	bool bEnableSplineResample,
	float SplineResampleDistanceCm,
	FText& OutErrorMessage,
	int32* OutSplineCount)
{
	if (OutSplineCount)
	{
		*OutSplineCount = 0;
	}

	if (!World)
	{
		OutErrorMessage = LOCTEXT("ExtractNoWorld", "当前世界无效。");
		return false;
	}

	if (!RoadNetworkActor || !RoadNetworkActor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
	{
		OutErrorMessage = LOCTEXT("ExtractNoNetwork", "请先选中一个 MainRoadNetwork 道路网络 Actor。");
		return false;
	}

	TArray<UQuick_RoadLayoutRoadMeshComponent*> RoadMeshComponents;
	Quick_RoadRoadLocals::CollectRoadMeshComponentsFromNetworkActor(
		RoadNetworkActor,
		bIncludeIntersectionPatches,
		RoadMeshComponents);

	if (RoadMeshComponents.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("ExtractNoRoadMesh", "所选 MainRoadNetwork 未找到道路程序化网格。请先在「路网绘制」中生成道路。");
		return false;
	}

	const float MinLengthCm = FMath::Max(MinPolylineLengthCm, 10.f);
	const float WeldToleranceCm = FMath::Max(VertexWeldToleranceCm, 0.1f);

	TArray<FVector> MergedVertices;
	TArray<int32> MergedTriangles;
	if (!Quick_RoadRoadLocals::MergeRoadMeshComponents(
		RoadMeshComponents,
		WeldToleranceCm,
		MergedVertices,
		MergedTriangles))
	{
		OutErrorMessage = LOCTEXT("ExtractMergeFailed", "无法合并道路网格数据。");
		return false;
	}

	TArray<Quick_RoadRoadLocals::FBoundaryPolyline> BoundaryPolylines;
	Quick_RoadRoadLocals::ExtractBoundaryPolylinesFromMesh(
		MergedVertices,
		MergedTriangles,
		WeldToleranceCm,
		BoundaryPolylines);

	Quick_RoadRoadLocals::FBoundaryPolyline OutermostPolyline;
	if (!Quick_RoadRoadLocals::SelectOutermostPolyline(BoundaryPolylines, MinLengthCm, OutermostPolyline))
	{
		OutErrorMessage = LOCTEXT("ExtractNoBoundary", "未能从道路网格提取最外圈边界。");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ExtractRoadEdgeSplines", "Extract Road Edge Splines"));
	AActor* SplineActor = Quick_RoadRoadLocals::SpawnRoadEdgeSplineActor(
		World,
		OutermostPolyline.Points,
		OutermostPolyline.bClosed,
		SplinePointType,
		bEnableSplineResample,
		SplineResampleDistanceCm,
		Quick_RoadRoadLocals::GetNextRoadEdgeSplineIndex(World));

	if (!SplineActor)
	{
		OutErrorMessage = LOCTEXT("ExtractSpawnAllFailed", "最外圈边界提取成功，但创建样条 Actor 失败。");
		return false;
	}

	if (OutSplineCount)
	{
		*OutSplineCount = 1;
	}

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(SplineActor, true, true, true);
	}
#endif

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 路边样条：从 %d 个道路网格组件提取最外圈（%s，%d 点）。"),
		RoadMeshComponents.Num(),
		OutermostPolyline.bClosed ? TEXT("闭合") : TEXT("开放"),
		OutermostPolyline.Points.Num());

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::ExtractIntersectionSplines(
	UWorld* World,
	AActor* RoadNetworkActor,
	ESplinePointType::Type SplinePointType,
	bool bEnableSplineResample,
	float SplineResampleDistanceCm,
	float VertexWeldToleranceCm,
	float MinInnerLoopLengthCm,
	bool bIncludeIntersectionPatches,
	FText& OutErrorMessage,
	int32* OutSplineCount)
{
	if (OutSplineCount)
	{
		*OutSplineCount = 0;
	}

	if (!World)
	{
		OutErrorMessage = LOCTEXT("ExtractIntersectionNoWorld", "当前世界无效。");
		return false;
	}

	if (!RoadNetworkActor || !RoadNetworkActor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
	{
		OutErrorMessage = LOCTEXT("ExtractIntersectionNoNetwork", "请先选中一个 MainRoadNetwork 道路网络 Actor。");
		return false;
	}

	const float WeldToleranceCm = FMath::Max(VertexWeldToleranceCm, 0.1f);
	const float MinLoopLengthCm = FMath::Max(MinInnerLoopLengthCm, 20.f);

	TArray<TArray<FVector>> IntersectionRings;

	TArray<Quick_RoadRoadLocals::FBoundaryPolyline> InnerHoleLoops;
	if (Quick_RoadRoadLocals::ExtractInnerHoleBoundariesFromRoadMesh(
		RoadNetworkActor,
		WeldToleranceCm,
		MinLoopLengthCm,
		bIncludeIntersectionPatches,
		InnerHoleLoops))
	{
		for (const Quick_RoadRoadLocals::FBoundaryPolyline& InnerLoop : InnerHoleLoops)
		{
			Quick_RoadRoadLocals::AppendUniqueIntersectionRing(InnerLoop.Points, IntersectionRings);
		}
	}

	if (IntersectionRings.Num() == 0)
	{
		OutErrorMessage = LOCTEXT(
			"ExtractIntersectionNone",
			"所选 MainRoadNetwork 未找到道路网格内孔。请确认已「生成道路」，且路网存在内孔（如三岔口中央三角区）。");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("ExtractIntersectionSplines", "Extract Intersection Splines"));
	TArray<AActor*> CreatedActors;
	CreatedActors.Reserve(IntersectionRings.Num());
	int32 CreatedSplineCount = 0;
	int32 NextRoadEdgeIndex = Quick_RoadRoadLocals::GetNextRoadEdgeSplineIndex(World);

	for (const TArray<FVector>& RingPoints : IntersectionRings)
	{
		AActor* SplineActor = Quick_RoadRoadLocals::SpawnRoadEdgeSplineActor(
			World,
			RingPoints,
			true,
			SplinePointType,
			bEnableSplineResample,
			SplineResampleDistanceCm,
			NextRoadEdgeIndex++);
		if (!SplineActor)
		{
			continue;
		}

		CreatedActors.Add(SplineActor);
		++CreatedSplineCount;
	}

	if (CreatedSplineCount == 0)
	{
		OutErrorMessage = LOCTEXT("ExtractIntersectionFailed", "找到内环边界，但创建样条 Actor 失败。");
		return false;
	}

	if (OutSplineCount)
	{
		*OutSplineCount = CreatedSplineCount;
	}

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		for (AActor* CreatedActor : CreatedActors)
		{
			GEditor->SelectActor(CreatedActor, true, false, true);
		}
	}
#endif

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 交叉口样条：从网格内孔提取 %d 个，共生成 %d 条内环样线。"),
		InnerHoleLoops.Num(),
		CreatedSplineCount);

	return true;
}

bool FQuick_RoadLayoutRoadGenerator::BakeRoadMeshesToStaticMeshes(
	UWorld* World,
	const FString& SaveDirectory,
	float StraightSegmentSplitDistanceCm,
	bool bDeleteSourceProcMesh,
	FText& OutErrorMessage,
	int32* OutBakedCount)
{
	if (OutBakedCount)
	{
		*OutBakedCount = 0;
	}

	if (!World)
	{
		OutErrorMessage = LOCTEXT("BakeNoWorld", "当前世界无效。");
		return false;
	}

	if (SaveDirectory.IsEmpty() || !FPackageName::IsValidLongPackageName(SaveDirectory))
	{
		OutErrorMessage = LOCTEXT("BakeInvalidPath", "保存路径无效，请使用 /Game/ 开头的 Content 路径。");
		return false;
	}

#if WITH_EDITOR
	AActor* NetworkActor = nullptr;
	if (GEditor)
	{
		TArray<AActor*> SelectedNetworks;
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = Cast<AActor>(*It);
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
			{
				SelectedNetworks.Add(Actor);
			}
		}

		if (SelectedNetworks.Num() == 1)
		{
			NetworkActor = SelectedNetworks[0];
		}
		else if (SelectedNetworks.Num() > 1)
		{
			OutErrorMessage = LOCTEXT("BakeMultiSelect", "请只选中一个 MainRoadNetwork Actor。");
			return false;
		}
	}

	if (!NetworkActor)
	{
		OutErrorMessage = LOCTEXT("BakeNoSelection", "请在场景中选中一个 MainRoadNetwork Actor。");
		return false;
	}
#else
	OutErrorMessage = LOCTEXT("BakeEditorOnly", "烘焙仅在编辑器中可用。");
	return false;
#endif

	const FScopedTransaction Transaction(LOCTEXT("BakeRoadMeshes", "Bake Road Static Meshes"));
	const FTransform NetworkTransform = NetworkActor->GetActorTransform();

	TArray<UQuick_RoadLayoutRoadMeshComponent*> MeshComponents;
	NetworkActor->GetComponents(MeshComponents);
	MeshComponents.Sort([](const UQuick_RoadLayoutRoadMeshComponent& A, const UQuick_RoadLayoutRoadMeshComponent& B)
	{
		return A.GetFName().LexicalLess(B.GetFName());
	});

	UQuick_RoadLayoutRoadMeshComponent* RoadMeshComponent = nullptr;
	for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent &&
			!MeshComponent->ComponentHasTag(Quick_RoadLayoutRoadTags::IntersectionPatch) &&
			MeshComponent->GetNumSections() > 0)
		{
			RoadMeshComponent = MeshComponent;
			break;
		}
	}

	TArray<USplineComponent*> Splines;
	CollectIntersectionSourceSplines(World, Splines);

	TArray<FQuick_RoadBakedMeshPlacement> BlueprintPlacements;
	int32 BakedCount = 0;

	// 步骤一：逐个 ProcMesh 执行「创建 StaticMesh」
	for (UQuick_RoadLayoutRoadMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent || MeshComponent->GetNumSections() == 0)
		{
			continue;
		}

		const FString AssetName = MeshComponent->GetFName().ToString();
		UStaticMesh* StaticMesh = FQuick_RoadRoadMeshBaker::ConvertProcMeshToStaticMeshAsset(
			MeshComponent,
			SaveDirectory,
			AssetName);
		if (!StaticMesh)
		{
			continue;
		}

		FQuick_RoadBakedMeshPlacement Placement;
		Placement.StaticMesh = StaticMesh;
		Placement.RelativeTransform =
			MeshComponent->GetComponentTransform().GetRelativeTransform(NetworkTransform);
		Placement.ComponentName = AssetName;
		BlueprintPlacements.Add(Placement);
		++BakedCount;
	}

	if (BakedCount == 0)
	{
		OutErrorMessage = LOCTEXT("BakeNoMeshes", "未找到可烘焙的 ProcMesh 组件。");
		return false;
	}

	// 步骤二：拆分已保存的 RoadMesh StaticMesh
	if (StraightSegmentSplitDistanceCm > KINDA_SMALL_NUMBER && RoadMeshComponent && Splines.Num() > 0)
	{
		const FString RoadAssetName = RoadMeshComponent->GetFName().ToString();
		int32 RoadPlacementIndex = INDEX_NONE;
		UStaticMesh* RoadStaticMesh = nullptr;
		FTransform RoadRelativeTransform = FTransform::Identity;

		for (int32 Index = 0; Index < BlueprintPlacements.Num(); ++Index)
		{
			if (BlueprintPlacements[Index].ComponentName == RoadAssetName)
			{
				RoadPlacementIndex = Index;
				RoadStaticMesh = BlueprintPlacements[Index].StaticMesh;
				RoadRelativeTransform = BlueprintPlacements[Index].RelativeTransform;
				break;
			}
		}

		if (RoadStaticMesh)
		{
			const FTransform RoadWorldTransform = RoadRelativeTransform * NetworkTransform;
			TArray<FQuick_RoadBakedMeshPlacement> SplitPlacements;
			const int32 SplitCount = FQuick_RoadRoadMeshBaker::SplitStaticMeshAssetByDistance(
				RoadStaticMesh,
				RoadWorldTransform,
				Splines,
				StraightSegmentSplitDistanceCm,
				SaveDirectory,
				RoadAssetName,
				SplitPlacements);

			if (SplitCount > 0)
			{
				BlueprintPlacements.RemoveAt(RoadPlacementIndex);
				for (FQuick_RoadBakedMeshPlacement& SplitPlacement : SplitPlacements)
				{
					SplitPlacement.RelativeTransform = RoadRelativeTransform;
					BlueprintPlacements.Add(SplitPlacement);
				}

				BakedCount += SplitCount - 1;
			}
		}
	}

	// 步骤三：拼装蓝图并放入场景
	const FString BlueprintAssetName = FString::Printf(TEXT("BP_%s"), *NetworkActor->GetActorLabel());
	if (!FQuick_RoadRoadMeshBaker::CreateBlueprintAssemblyAndSpawn(
		World,
		SaveDirectory,
		BlueprintAssetName,
		NetworkTransform,
		BlueprintPlacements))
	{
		OutErrorMessage = LOCTEXT("BakeBlueprintFailed", "拼装蓝图或放入场景失败。");
		return false;
	}

	if (bDeleteSourceProcMesh)
	{
		NetworkActor->Modify();
		World->DestroyActor(NetworkActor);
	}

	if (OutBakedCount)
	{
		*OutBakedCount = BakedCount;
	}

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 烘焙：步骤一~三完成，共 %d 个 StaticMesh，蓝图 %s"),
		BakedCount,
		*BlueprintAssetName);

	return true;
}

#undef LOCTEXT_NAMESPACE
