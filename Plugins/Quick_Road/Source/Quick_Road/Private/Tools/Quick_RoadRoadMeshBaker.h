// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;
class UStaticMesh;
class AActor;
class UWorld;
class USplineComponent;

/** 蓝图拼装用的静态网格条目。 */
struct FQuick_RoadBakedMeshPlacement
{
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;
	FTransform RelativeTransform = FTransform::Identity;
	FString ComponentName;
};

/** 将程序化网格烘焙为 StaticMesh 资产（与引擎 Details「创建 StaticMesh」等价）。 */
class FQuick_RoadRoadMeshBaker
{
public:
	/** 步骤一：ProcMesh → StaticMesh 资产。 */
	static UStaticMesh* ConvertProcMeshToStaticMeshAsset(
		UProceduralMeshComponent* ProcMesh,
		const FString& PackagePath,
		const FString& AssetName);

	/** 步骤二：将已保存的 RoadMesh StaticMesh 按样条弧长距离拆分为多个资产。 */
	static int32 SplitStaticMeshAssetByDistance(
		UStaticMesh* RoadStaticMesh,
		const FTransform& ComponentWorldTransform,
		const TArray<USplineComponent*>& Splines,
		float SplitDistanceCm,
		const FString& PackagePath,
		const FString& AssetNamePrefix,
		TArray<FQuick_RoadBakedMeshPlacement>& OutSplitPlacements);

	/** 步骤三：将所有 StaticMesh 拼装为蓝图并放入场景。 */
	static AActor* CreateBlueprintAssemblyAndSpawn(
		UWorld* World,
		const FString& PackagePath,
		const FString& BlueprintAssetName,
		const FTransform& WorldTransform,
		const TArray<FQuick_RoadBakedMeshPlacement>& Placements);
};
