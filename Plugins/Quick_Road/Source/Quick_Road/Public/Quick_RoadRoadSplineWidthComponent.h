// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quick_RoadRoadSplineWidthComponent.generated.h"

class USplineComponent;

/** 挂在样条 Actor 上，记录该条道路生成时的路带半宽 (cm)。 */
UCLASS(ClassGroup = (QuickRoad), meta = (BlueprintSpawnableComponent, DisplayName = "样条线宽度"))
class QUICK_ROAD_API UQuick_RoadRoadSplineWidthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuick_RoadRoadSplineWidthComponent();

	UPROPERTY(EditAnywhere, Category = "道路宽度", meta = (
		DisplayName = "路带半宽 (cm)",
		ClampMin = "50",
		ToolTip = "「生成道路」时写入；「拆开并重构」时按此宽度重建该样条路段与路口臂。"))
	float HalfWidthCm = 0.f;

	/** 读取样条半宽；未设置时返回 FallbackHalfWidthCm。 */
	static float ResolveHalfWidthCm(const USplineComponent* Spline, float FallbackHalfWidthCm);

	/** 写入样条半宽（自动创建/更新组件）。 */
	static void SetHalfWidthCm(USplineComponent* Spline, float InHalfWidthCm);

	static bool HasStoredHalfWidthCm(const USplineComponent* Spline);
};
