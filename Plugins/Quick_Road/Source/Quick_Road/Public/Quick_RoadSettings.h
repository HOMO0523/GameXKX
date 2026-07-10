// Copyright Aoife. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Quick_RoadSettings.generated.h"

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Quick Road"))
class QUICK_ROAD_API UQuickRoadSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UQuickRoadSettings();

	virtual FName GetCategoryName() const override;

	virtual FText GetSectionDescription() const override;

	static UQuickRoadSettings* Get();
};
