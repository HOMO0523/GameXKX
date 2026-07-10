// Copyright Aoife. All Rights Reserved.

#include "Quick_RoadSettings.h"

#include "Quick_RoadLocalization.h"

UQuickRoadSettings::UQuickRoadSettings()
{
}

FName UQuickRoadSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FText UQuickRoadSettings::GetSectionDescription() const
{
	return QuickRoadLocalization::GetSettingsSectionDescription();
}

UQuickRoadSettings* UQuickRoadSettings::Get()
{
	return GetMutableDefault<UQuickRoadSettings>();
}
