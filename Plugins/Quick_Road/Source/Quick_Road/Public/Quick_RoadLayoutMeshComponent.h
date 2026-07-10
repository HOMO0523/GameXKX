// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Quick_RoadLayoutMeshComponent.generated.h"

/** 城市布局面片程序化网格（仅负责显示网格，贴合功能见 GroundConform 组件）。 */
UCLASS(ClassGroup = (QuickRoad), meta = (BlueprintSpawnableComponent))
class QUICK_ROAD_API UQuick_RoadLayoutMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UQuick_RoadLayoutMeshComponent(const FObjectInitializer& ObjectInitializer);
};
