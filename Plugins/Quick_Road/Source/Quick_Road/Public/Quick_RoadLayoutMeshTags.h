// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Quick_RoadLayoutMeshTags
{
	/** 所有布局面片 Actor 的公共 Tag */
	inline const FName LayoutMesh = TEXT("Quick_Road_LayoutMesh");

	/** 城市范围阶段生成的布局面片 */
	inline const FName CityScopeLayout = TEXT("Quick_Road_CityScopeLayout");

	/** 布局面片默认贴图（StarterContent） */
	inline const TCHAR* DefaultLayoutMeshTexturePath =
		TEXT("/Game/StarterContent/Textures/T_CobbleStone_Rough_D.T_CobbleStone_Rough_D");

	/** 布局面片默认材质（使用该贴图的 StarterContent 鹅卵石材质） */
	inline const TCHAR* DefaultLayoutMeshMaterialPath =
		TEXT("/Game/StarterContent/Materials/M_CobbleStone_Rough.M_CobbleStone_Rough");
}
