// Copyright Aoife. All Rights Reserved.

#include "Quick_RoadLocalization.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	struct FQuickRoadLocalizedEntry
	{
		const TCHAR* Key;
		const TCHAR* Chinese;
		const TCHAR* English;
	};

	static const FQuickRoadLocalizedEntry GEntries[] = {
		{ TEXT("EditorModeName"), TEXT("Quick Road"), TEXT("Quick Road") },

		{ TEXT("Category.Terrain"), TEXT("地形"), TEXT("Terrain") },
		{ TEXT("Category.Layout"), TEXT("布局"), TEXT("Layout") },
		{ TEXT("Category.Bake"), TEXT("烘焙"), TEXT("Bake") },

		{ TEXT("Tool.GenerateTerrainTool"), TEXT("生成地形"), TEXT("Generate Terrain") },
		{ TEXT("Tool.HeightfieldNoiseTool"), TEXT("高度噪声"), TEXT("Heightfield Noise") },
		{ TEXT("Tool.AutoErosionTool"), TEXT("自动侵蚀"), TEXT("Auto Erosion") },
		{ TEXT("Tool.CityScopeTool"), TEXT("区域绘制"), TEXT("Draw Scope") },
		{ TEXT("Tool.RoadTool"), TEXT("路网绘制"), TEXT("Draw Roads") },
		{ TEXT("Tool.RiverTool"), TEXT("获取数据"), TEXT("Extract Data") },
		{ TEXT("Tool.BakeTool"), TEXT("烘焙"), TEXT("Bake") },

		{ TEXT("Tooltip.GenerateTerrainTool"), TEXT("从高度图创建 Landscape 地形"), TEXT("Create landscape from heightmap") },
		{ TEXT("Tooltip.HeightfieldNoiseTool"), TEXT("对 Landscape 应用 Houdini 风格高度噪声"), TEXT("Apply Houdini-style heightfield noise to landscape") },
		{ TEXT("Tooltip.AutoErosionTool"), TEXT("对地形应用自动侵蚀"), TEXT("Apply automatic erosion to terrain") },
		{ TEXT("Tooltip.CityScopeTool"), TEXT("在 Landscape 上绘制布局区域边界样条"), TEXT("Draw layout area boundary spline on landscape") },
		{ TEXT("Tooltip.RoadTool"), TEXT("绘制主路样条并生成道路网格"), TEXT("Draw main road splines and generate road mesh") },
		{ TEXT("Tooltip.RiverTool"), TEXT("从已生成的道路网格提取布局数据"), TEXT("Extract layout data from generated road meshes") },
		{ TEXT("Tooltip.BakeTool"), TEXT("将程序化道路网格烘焙为 StaticMesh 资产"), TEXT("Bake procedural road meshes to static mesh assets") },

		{ TEXT("PluginDescription"),
			TEXT("Quick Road V1（免费）—— UE5 关卡/世界搭建 Editor 插件：地形、区域布局、样条路网、路口与道路网格烘焙。产出可直供 PCG Graph 使用，与 PCG 分区/散布/生态管线自然衔接。"),
			TEXT("Quick Road V1 (Free) — UE5 level/world building editor plugin: terrain, layout scoping, spline roads, intersections, and road mesh baking. Outputs feed PCG Graph workflows for partitioning, scattering, and ecology pipelines.") },
	};

	static FQuickRoadLocalizationChanged GLocalizationChangedDelegate;
	static FDelegateHandle GCultureChangedHandle;

	static const FQuickRoadLocalizedEntry* FindEntry(const TCHAR* Key)
	{
		for (const FQuickRoadLocalizedEntry& Entry : GEntries)
		{
			if (FCString::Strcmp(Entry.Key, Key) == 0)
			{
				return &Entry;
			}
		}
		return nullptr;
	}
}

void QuickRoadLocalization::Initialize()
{
	if (GCultureChangedHandle.IsValid())
	{
		return;
	}

	GCultureChangedHandle = FInternationalization::Get().OnCultureChanged().AddLambda([]()
	{
		ApplyLocalizedPluginDescriptor();
		GLocalizationChangedDelegate.Broadcast();
	});

	ApplyLocalizedPluginDescriptor();
}

void QuickRoadLocalization::Shutdown()
{
	if (GCultureChangedHandle.IsValid())
	{
		FInternationalization::Get().OnCultureChanged().Remove(GCultureChangedHandle);
		GCultureChangedHandle.Reset();
	}
}

FQuickRoadLocalizationChanged& QuickRoadLocalization::OnLocalizationChanged()
{
	return GLocalizationChangedDelegate;
}

bool QuickRoadLocalization::UsesEnglishEditorCulture()
{
	const TSharedPtr<const FCulture> CurrentLanguage = FInternationalization::Get().GetCurrentLanguage();
	if (CurrentLanguage.IsValid())
	{
		const FString LanguageName = CurrentLanguage->GetName();
		if (LanguageName.StartsWith(TEXT("en")))
		{
			return true;
		}
		if (LanguageName.StartsWith(TEXT("zh")))
		{
			return false;
		}
	}

	return true;
}

FText QuickRoadLocalization::GetText(const TCHAR* Key)
{
	if (const FQuickRoadLocalizedEntry* Entry = FindEntry(Key))
	{
		return FText::FromString(UsesEnglishEditorCulture() ? Entry->English : Entry->Chinese);
	}
	return FText::FromString(Key);
}

FText QuickRoadLocalization::GetEditorModeName()
{
	return GetText(TEXT("EditorModeName"));
}

FText QuickRoadLocalization::GetCategoryLabel(FName CategoryId)
{
	if (CategoryId == TEXT("Terrain"))
	{
		return GetText(TEXT("Category.Terrain"));
	}
	if (CategoryId == TEXT("City"))
	{
		return GetText(TEXT("Category.Layout"));
	}
	if (CategoryId == TEXT("Bake"))
	{
		return GetText(TEXT("Category.Bake"));
	}
	return FText::FromName(CategoryId);
}

FText QuickRoadLocalization::GetToolLabel(FName CommandName)
{
	return GetText(*FString::Printf(TEXT("Tool.%s"), *CommandName.ToString()));
}

FText QuickRoadLocalization::GetToolTooltip(FName CommandName)
{
	return GetText(*FString::Printf(TEXT("Tooltip.%s"), *CommandName.ToString()));
}

FText QuickRoadLocalization::GetSettingsSectionDescription()
{
	return GetText(TEXT("PluginDescription"));
}

void QuickRoadLocalization::ApplyLocalizedPluginDescriptor()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Quick_Road"));
	if (!Plugin.IsValid())
	{
		return;
	}

	FPluginDescriptor NewDescriptor = Plugin->GetDescriptor();
	NewDescriptor.Description = GetSettingsSectionDescription().ToString();

	FText FailReason;
	Plugin->UpdateDescriptor(NewDescriptor, FailReason);
}
