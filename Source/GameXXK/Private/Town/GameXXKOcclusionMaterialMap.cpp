#include "Town/GameXXKOcclusionMaterialMap.h"

#include "Materials/MaterialInterface.h"

namespace
{
	struct FGameXXKOcclusionMaterialPathPair
	{
		const TCHAR* Original;
		const TCHAR* Cutout;
	};

	constexpr FGameXXKOcclusionMaterialPathPair PilotMappings[] =
	{
		{
			TEXT("/Game/Asian_Village/materials/building_materials/M_thatched_roof.M_thatched_roof"),
			TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/M_thatched_roof_Cutout.M_thatched_roof_Cutout")
		},
		{
			TEXT("/Game/Asian_Village/materials/building_materials/MI_building_wood_06_Inst.MI_building_wood_06_Inst"),
			TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_building_wood_06_Inst_Cutout.MI_building_wood_06_Inst_Cutout")
		}
	};
}

UMaterialInterface* FGameXXKOcclusionMaterialMap::Resolve(const UMaterialInterface* OriginalMaterial) const
{
	if (!OriginalMaterial)
	{
		return nullptr;
	}
	const FString OriginalPath = OriginalMaterial->GetPathName();
	for (const FGameXXKOcclusionMaterialPathPair& Pair : PilotMappings)
	{
		if (OriginalPath == Pair.Original)
		{
			return LoadObject<UMaterialInterface>(nullptr, Pair.Cutout, nullptr, LOAD_NoWarn);
		}
	}
	return nullptr;
}
