// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Quick_RoadRoadMeshTypes.h"
#include "Components/SplineComponent.h"

class UWorld;

class USplineComponent;

class UProceduralMeshComponent;

class AActor;

class ALandscape;

class UQuick_RoadLayoutMeshComponent;

struct FQuick_RoadLayoutMainRoadStageParams;

struct FQuick_RoadLayoutMainRoadIntersectionParams;

struct FQuick_RoadRoadInfluenceParams

{

	float HalfWidthCm = 600.f;

	float InfluenceFalloffCm = 300.f;

	float BlendStrength = 1.f;

	float VerticalOffsetCm = -5.f;

	int32 EdgeSmoothIterations = 4;

	float EdgeSmoothStrength = 0.65f;

};

struct FQuick_RoadRoadCorridorSample

{

	FVector2D Start = FVector2D::ZeroVector;

	FVector2D End = FVector2D::ZeroVector;

	float StartZ = 0.f;

	float EndZ = 0.f;

};

struct FQuick_RoadRoadCorridorQuery

{

	float Weight = 0.f;

	float TargetWorldZ = 0.f;

	float DistanceToCenterCm = TNumericLimits<float>::Max();

};

struct FQuick_RoadLayoutRoadResult
{
	TObjectPtr<AActor> RoadActor = nullptr;
	int32 SplineCount = 0;
	int32 RibbonSegmentCount = 0;
	int32 VertexCount = 0;
	int32 TriangleCount = 0;
};

struct FQuick_RoadRoadIntersectionInfo
{
	FVector WorldLocation = FVector::ZeroVector;
	TArray<TObjectPtr<USplineComponent>> Splines;
};

class FQuick_RoadRoadCorridorField

{

public:

	void BuildFromSplines(const TArray<USplineComponent*>& Splines, float SampleDistanceCm);

	FQuick_RoadRoadCorridorQuery Query(

		const FVector2D& WorldXY,

		float HalfWidthCm,

		float InfluenceFalloffCm) const;

private:

	TArray<FQuick_RoadRoadCorridorSample> Segments;

};

struct FQuick_RoadRoadMeshSurfaceTriangle

{

	FVector A = FVector::ZeroVector;

	FVector B = FVector::ZeroVector;

	FVector C = FVector::ZeroVector;

};

class FQuick_RoadRoadMeshInfluenceField

{

public:

	void BuildFromProcMeshComponents(const TArray<UProceduralMeshComponent*>& MeshComponents);

	bool IsEmpty() const { return Triangles.Num() == 0; }

	FQuick_RoadRoadCorridorQuery Query(const FVector2D& WorldXY, float InfluenceFalloffCm) const;

private:

	TArray<FQuick_RoadRoadMeshSurfaceTriangle> Triangles;

};

class FQuick_RoadLayoutRoadGenerator

{

public:

	static void CollectMainRoadSplines(UWorld* World, TArray<USplineComponent*>& OutSplines, FName RoadTag);

	/** 按道路 Tag 表达式收集样条，支持 aa|dd 表示多个 Tag 的并集。 */
	static void CollectMainRoadSplinesByTagExpression(
		UWorld* World,
		TArray<USplineComponent*>& OutSplines,
		FName RoadTagExpression);

	/** 收集已「生成道路」并打上 IntersectionSourceSpline 标签的样条（交叉口专用） */
	static void CollectIntersectionSourceSplines(UWorld* World, TArray<USplineComponent*>& OutSplines);

	static void TagSplinesAsIntersectionSource(
		const TArray<USplineComponent*>& Splines,
		float DefaultHalfWidthCm);

	static bool GenerateRoadMeshes(

		UWorld* World,

		const FQuick_RoadLayoutMainRoadStageParams& StageParams,

		const FQuick_RoadLayoutMainRoadIntersectionParams& IntersectionParams,

		TArray<FQuick_RoadLayoutRoadResult>& OutResults,

		FText& OutErrorMessage);

	static bool ApplyRoadInfluenceDual(
		UWorld* World,
		const TArray<USplineComponent*>& RoadSplines,
		const FQuick_RoadRoadInfluenceParams& Params,
		FText& OutErrorMessage,
		int32* OutLandscapeHits = nullptr,
		int32* OutLayoutHits = nullptr);

	static bool ApplyRoadInfluenceDualFromProcMeshes(
		UWorld* World,
		const TArray<UProceduralMeshComponent*>& RoadMeshes,
		const FQuick_RoadRoadInfluenceParams& Params,
		FText& OutErrorMessage,
		int32* OutLandscapeHits = nullptr,
		int32* OutLayoutHits = nullptr);

	static bool SmoothRoadCorridorDual(
		UWorld* World,
		const TArray<USplineComponent*>& RoadSplines,
		const FQuick_RoadRoadInfluenceParams& Params,
		FText& OutErrorMessage);

	static bool SmoothRoadCorridorDualFromProcMeshes(
		UWorld* World,
		const TArray<UProceduralMeshComponent*>& RoadMeshes,
		const FQuick_RoadRoadInfluenceParams& Params,
		FText& OutErrorMessage);

	static FQuick_RoadRoadInfluenceParams MakeInfluenceParamsFromStage(

		const FQuick_RoadLayoutMainRoadStageParams& StageParams);

	static FQuick_RoadRoadRibbonBuildParams MakeRibbonBuildParamsFromStage(

		const FQuick_RoadLayoutMainRoadStageParams& StageParams);

	static FQuick_RoadRoadSplitRebuildSettings MakeSplitRebuildSettingsFromStage(
		const FQuick_RoadLayoutMainRoadStageParams& StageParams,
		const FQuick_RoadLayoutMainRoadIntersectionParams& IntersectionParams);

	static void CollectRoadNetworkActorsByTag(UWorld* World, FName RoadTagExpression, TArray<AActor*>& OutActors);

	static int32 DetectMainRoadIntersections(
		UWorld* World,
		FName RoadTagExpression,
		float DetectSampleDistanceCm,
		TArray<FQuick_RoadRoadIntersectionInfo>& OutIntersections);

	static bool SplitAndRebuildRoadNetwork(
		AActor* NetworkActor,
		FName RoadTagExpression,
		const FQuick_RoadRoadSplitRebuildSettings& Settings,
		FText& OutErrorMessage,
		int32* OutIntersectionPatchCount = nullptr,
		int32* OutRibbonSegmentCount = nullptr);

	/** 在保留的一个道路网络上执行拆分重构，并删除表达式匹配的其他道路网络 Actor。 */
	static bool SplitAndRebuildConsolidatedRoadNetworks(
		UWorld* World,
		FName RoadTagExpression,
		const FQuick_RoadRoadSplitRebuildSettings& Settings,
		FText& OutErrorMessage,
		int32* OutRemovedNetworkCount = nullptr,
		int32* OutIntersectionPatchCount = nullptr,
		int32* OutRibbonSegmentCount = nullptr);

	/** 从程序化道路网格最外圈边界提取一条样条线。OutSplineCount 成功时为 1。 */
	static bool ExtractRoadEdgeSplines(
		UWorld* World,
		AActor* RoadNetworkActor,
		float MinPolylineLengthCm,
		float VertexWeldToleranceCm,
		bool bIncludeIntersectionPatches,
		ESplinePointType::Type SplinePointType,
		bool bEnableSplineResample,
		float SplineResampleDistanceCm,
		FText& OutErrorMessage,
		int32* OutSplineCount = nullptr);

	/** 从道路网格内孔（如三岔口中央三角区）提取内环样条。 */
	static bool ExtractIntersectionSplines(
		UWorld* World,
		AActor* RoadNetworkActor,
		ESplinePointType::Type SplinePointType,
		bool bEnableSplineResample,
		float SplineResampleDistanceCm,
		float VertexWeldToleranceCm,
		float MinInnerLoopLengthCm,
		bool bIncludeIntersectionPatches,
		FText& OutErrorMessage,
		int32* OutSplineCount = nullptr);

	/** 将选中 MainRoadNetwork 烘焙为 StaticMesh：①逐个转换 ProcMesh ②可选拆分 RoadMesh ③拼装蓝图入场景。 */
	static bool BakeRoadMeshesToStaticMeshes(
		UWorld* World,
		const FString& SaveDirectory,
		float StraightSegmentSplitDistanceCm,
		bool bDeleteSourceProcMesh,
		FText& OutErrorMessage,
		int32* OutBakedCount = nullptr);

};