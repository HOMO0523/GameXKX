// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProceduralMeshComponent.h"

#include "Quick_RoadLayoutRoadMeshComponent.generated.h"

/** 主路网络程序化网格（由场景中所有 MainRoad 样条合并生成）。 */

UCLASS(ClassGroup = (QuickRoad), meta = (BlueprintSpawnableComponent))

class QUICK_ROAD_API UQuick_RoadLayoutRoadMeshComponent : public UProceduralMeshComponent

{

	GENERATED_BODY()

public:

	UQuick_RoadLayoutRoadMeshComponent(const FObjectInitializer& ObjectInitializer);

};