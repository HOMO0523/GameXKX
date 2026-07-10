// Copyright Aoife. All Rights Reserved.

#include "UObject/CoreRedirects.h"
#include "Misc/DelayedAutoRegister.h"

namespace QuickRoadCoreRedirects
{
	static void Register()
	{
		TArray<FCoreRedirect> Redirects;
		Redirects.Reserve(64);

		Redirects.Emplace(
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5)
			ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring,
#else
			ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchPrefix,
#endif
			TEXT("/Quick_City/"),
			TEXT("/Quick_Road/"));
		Redirects.Emplace(
#if (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5)
			ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchSubstring,
#else
			ECoreRedirectFlags::Type_Package | ECoreRedirectFlags::Option_MatchPrefix,
#endif
			TEXT("/Script/Quick_City"),
			TEXT("/Script/Quick_Road"));

		const auto AddClass = [&Redirects](const TCHAR* OldName, const TCHAR* NewName)
		{
			Redirects.Emplace(ECoreRedirectFlags::Type_Class, OldName, NewName);
		};

		AddClass(TEXT("Quick_CityEditorMode"), TEXT("Quick_RoadEditorMode"));
		AddClass(TEXT("Quick_CitySettings"), TEXT("Quick_RoadSettings"));

		AddClass(TEXT("Quick_CityLayoutMeshComponent"), TEXT("Quick_RoadLayoutMeshComponent"));
		AddClass(TEXT("Quick_CityLayoutRoadMeshComponent"), TEXT("Quick_RoadLayoutRoadMeshComponent"));
		AddClass(TEXT("Quick_CityLayoutLandscapeConformComponent"), TEXT("Quick_RoadLayoutLandscapeConformComponent"));
		AddClass(TEXT("Quick_CityRoadInfluenceComponent"), TEXT("Quick_RoadRoadInfluenceComponent"));
		AddClass(TEXT("Quick_CityRoadSplineWidthComponent"), TEXT("Quick_RoadRoadSplineWidthComponent"));
		AddClass(TEXT("Quick_CityRoadIntersectionSplitComponent"), TEXT("Quick_RoadLayoutRoadMeshComponent"));

		AddClass(TEXT("Quick_CityAutoErosionTool"), TEXT("Quick_RoadAutoErosionTool"));
		AddClass(TEXT("Quick_CityAutoErosionToolBuilder"), TEXT("Quick_RoadAutoErosionToolBuilder"));
		AddClass(TEXT("Quick_CityAutoErosionToolProperties"), TEXT("Quick_RoadAutoErosionToolProperties"));
		AddClass(TEXT("Quick_CityBakeTool"), TEXT("Quick_RoadBakeTool"));
		AddClass(TEXT("Quick_CityBakeToolBuilder"), TEXT("Quick_RoadBakeToolBuilder"));
		AddClass(TEXT("Quick_CityBakeToolProperties"), TEXT("Quick_RoadBakeToolProperties"));
		AddClass(TEXT("Quick_CityCityPlaceholderTool"), TEXT("Quick_RoadCityPlaceholderTool"));
		AddClass(TEXT("Quick_CityCityPlaceholderToolBuilder"), TEXT("Quick_RoadCityPlaceholderToolBuilder"));
		AddClass(TEXT("Quick_CityCityToolProperties"), TEXT("Quick_RoadCityToolProperties"));
		AddClass(TEXT("Quick_CityCityScopeTool"), TEXT("Quick_RoadCityScopeTool"));
		AddClass(TEXT("Quick_CityCityScopeToolBuilder"), TEXT("Quick_RoadCityScopeToolBuilder"));
		AddClass(TEXT("Quick_CityCityScopeToolProperties"), TEXT("Quick_RoadCityScopeToolProperties"));
		AddClass(TEXT("Quick_CityConformToMeshToolProperties"), TEXT("Quick_RoadLayoutLandscapeConformComponent"));
		AddClass(TEXT("Quick_CityGenerateCityTool"), TEXT("Quick_RoadGenerateCityTool"));
		AddClass(TEXT("Quick_CityGenerateCityToolBuilder"), TEXT("Quick_RoadGenerateCityToolBuilder"));
		AddClass(TEXT("Quick_CityGenerateCityToolProperties"), TEXT("Quick_RoadGenerateCityToolProperties"));
		AddClass(TEXT("Quick_CityGenerateTerrainTool"), TEXT("Quick_RoadGenerateTerrainTool"));
		AddClass(TEXT("Quick_CityGenerateTerrainToolBuilder"), TEXT("Quick_RoadGenerateTerrainToolBuilder"));
		AddClass(TEXT("Quick_CityGenerateTerrainToolProperties"), TEXT("Quick_RoadGenerateTerrainToolProperties"));
		AddClass(TEXT("Quick_CityHeightfieldNoiseTool"), TEXT("Quick_RoadHeightfieldNoiseTool"));
		AddClass(TEXT("Quick_CityHeightfieldNoiseToolBuilder"), TEXT("Quick_RoadHeightfieldNoiseToolBuilder"));
		AddClass(TEXT("Quick_CityHeightfieldNoiseToolProperties"), TEXT("Quick_RoadHeightfieldNoiseToolProperties"));
		AddClass(TEXT("Quick_CityInteractiveTool"), TEXT("Quick_RoadInteractiveTool"));
		AddClass(TEXT("Quick_CityInteractiveToolBuilder"), TEXT("Quick_RoadInteractiveToolBuilder"));
		AddClass(TEXT("Quick_CityInteractiveToolProperties"), TEXT("Quick_RoadInteractiveToolProperties"));
		AddClass(TEXT("Quick_CityMainRoadTool"), TEXT("Quick_RoadMainRoadTool"));
		AddClass(TEXT("Quick_CityMainRoadToolBuilder"), TEXT("Quick_RoadMainRoadToolBuilder"));
		AddClass(TEXT("Quick_CityMainRoadToolProperties"), TEXT("Quick_RoadMainRoadToolProperties"));
		AddClass(TEXT("Quick_CitySimpleTool"), TEXT("Quick_RoadSimpleTool"));
		AddClass(TEXT("Quick_CitySimpleToolBuilder"), TEXT("Quick_RoadSimpleToolBuilder"));
		AddClass(TEXT("Quick_CitySimpleToolProperties"), TEXT("Quick_RoadSimpleToolProperties"));
		AddClass(TEXT("Quick_CityTerrainPlaceholderTool"), TEXT("Quick_RoadTerrainPlaceholderTool"));
		AddClass(TEXT("Quick_CityTerrainPlaceholderToolBuilder"), TEXT("Quick_RoadTerrainPlaceholderToolBuilder"));
		AddClass(TEXT("Quick_CityTerrainToolProperties"), TEXT("Quick_RoadTerrainToolProperties"));

		FCoreRedirects::AddRedirectList(Redirects, TEXT("QuickRoadPluginMigration"));
	}

	static FDelayedAutoRegisterHelper RegisterHelper(
		EDelayedRegisterRunPhase::EarliestPossiblePluginsLoaded,
		[] { Register(); });
}
