// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Quick_RoadCityPlaceholderTool.generated.h"

UENUM()
enum class EQuick_RoadCityToolKind : uint8
{
	CityScope,
	Road,
	River,
	Lake
};

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadCityToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadCityToolProperties();

	UPROPERTY(EditAnywhere, Category = "Options", meta = (DisplayName = "Seed"))
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "Options", meta = (DisplayName = "Strength"))
	float Strength;

	UPROPERTY(VisibleAnywhere, Category = "Tool", meta = (DisplayName = "Description"))
	FText Description;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadCityPlaceholderToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	EQuick_RoadCityToolKind ToolKind = EQuick_RoadCityToolKind::CityScope;

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadCityPlaceholderTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	void SetToolKind(EQuick_RoadCityToolKind InToolKind);

	virtual void Setup() override;

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadCityToolProperties> Properties;

	EQuick_RoadCityToolKind ToolKind = EQuick_RoadCityToolKind::CityScope;
};
