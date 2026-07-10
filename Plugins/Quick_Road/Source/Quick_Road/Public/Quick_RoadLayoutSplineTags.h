// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Quick_RoadLayoutSplineTags
{
	/** 所有布局绘制样条线 Actor 的公共 Tag */
	inline const FName LayoutSpline = TEXT("Quick_Road_LayoutSpline");

	inline const FName CityScope = TEXT("Quick_Road_CityScope");
	inline const FName MainRoad = TEXT("Quick_Road_MainRoad");

	/** 从道路 ProcMesh 边界提取的路边样条（含外圈与内孔内环） */
	inline const FName RoadEdge = TEXT("Quick_Road_RoadEdge");
}
