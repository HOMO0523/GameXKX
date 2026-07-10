// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Quick_RoadTerrainPlaceholderTool.generated.h"

UENUM()
enum class EQuick_RoadTerrainToolKind : uint8
{
	GenerateTerrain,
	AutoErosion,
	ConformToMesh
};

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadTerrainToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadTerrainToolProperties();

	UPROPERTY(EditAnywhere, Category = "Options", meta = (DisplayName = "Seed"))
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "Options", meta = (DisplayName = "Strength"))
	float Strength;

	UPROPERTY(VisibleAnywhere, Category = "Tool", meta = (DisplayName = "Description"))
	FText Description;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadTerrainPlaceholderToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	EQuick_RoadTerrainToolKind ToolKind = EQuick_RoadTerrainToolKind::GenerateTerrain;

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadTerrainPlaceholderTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	void SetToolKind(EQuick_RoadTerrainToolKind InToolKind);

	virtual void Setup() override;

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadTerrainToolProperties> Properties;

	EQuick_RoadTerrainToolKind ToolKind = EQuick_RoadTerrainToolKind::GenerateTerrain;
};
