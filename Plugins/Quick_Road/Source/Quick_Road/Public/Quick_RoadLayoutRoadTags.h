// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Quick_RoadLayoutRoadTags

{

	/** 合并生成的道路网络程序化网格 Actor */

	inline const FName MainRoadMesh = TEXT("Quick_Road_MainRoadMesh");

	/** 道路网络 Actor 标签 */
	inline const FName MainRoadNetwork = TEXT("Quick_Road_MainRoadNetwork");

	/** 拆分后的交叉口补丁 ProcMesh 组件标签 */
	inline const FName IntersectionPatch = TEXT("Quick_Road_IntersectionPatch");

	/** 「生成道路」时写入样条，供交叉口检测 / 拆分专用（与用户道路 Tag 并存） */
	inline const FName IntersectionSourceSpline = TEXT("Quick_Road_IntersectionSource");

	inline bool IsValidUserRoadTag(FName RoadTag)
	{
		return !RoadTag.IsNone() && !RoadTag.ToString().IsEmpty();
	}

	inline void ParseRoadTagExpression(FName TagExpression, TArray<FName>& OutTags)
	{
		OutTags.Reset();
		if (TagExpression.IsNone())
		{
			return;
		}

		TArray<FString> Parts;
		TagExpression.ToString().ParseIntoArray(Parts, TEXT("|"), true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty())
			{
				OutTags.AddUnique(FName(*Part));
			}
		}
	}

	inline bool IsValidUserRoadTagExpression(FName TagExpression)
	{
		TArray<FName> Tags;
		ParseRoadTagExpression(TagExpression, Tags);
		return Tags.Num() > 0;
	}

	inline bool SplineHasAnyUserRoadTag(
		const AActor* SplineActor,
		const USplineComponent* SplineComponent,
		const TArray<FName>& UserRoadTags)
	{
		for (const FName UserRoadTag : UserRoadTags)
		{
			if ((SplineActor && SplineActor->ActorHasTag(UserRoadTag))
				|| (SplineComponent && SplineComponent->ComponentHasTag(UserRoadTag)))
			{
				return true;
			}
		}

		return false;
	}

	inline void ApplyUserRoadTagToSpline(AActor* SplineActor, USplineComponent* SplineComponent, FName UserRoadTag)
	{
		if (!IsValidUserRoadTag(UserRoadTag))
		{
			return;
		}

		if (SplineActor)
		{
			SplineActor->Tags.AddUnique(UserRoadTag);
		}

		if (SplineComponent)
		{
			SplineComponent->ComponentTags.AddUnique(UserRoadTag);
		}
	}

	inline void ApplyIntersectionSourceTagToSpline(AActor* SplineActor, USplineComponent* SplineComponent)
	{
		if (SplineComponent)
		{
			SplineComponent->ComponentTags.AddUnique(IntersectionSourceSpline);
		}

		if (SplineActor)
		{
			SplineActor->Tags.AddUnique(IntersectionSourceSpline);
		}
	}
}