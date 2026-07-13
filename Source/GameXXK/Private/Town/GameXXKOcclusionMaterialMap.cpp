#include "Town/GameXXKOcclusionMaterialMap.h"

#include "Materials/MaterialInterface.h"

namespace
{
	constexpr const TCHAR* CutoutFolder = TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage");
}

UMaterialInterface* FGameXXKOcclusionMaterialMap::Resolve(const UMaterialInterface* OriginalMaterial) const
{
	if (!OriginalMaterial)
	{
		return nullptr;
	}
	const FString OriginalName = OriginalMaterial->GetName();
	const FString CutoutName = OriginalName + TEXT("_Cutout");
	const FString CutoutPath = FString::Printf(
		TEXT("%s/%s.%s"),
		CutoutFolder,
		*CutoutName,
		*CutoutName);
	return LoadObject<UMaterialInterface>(nullptr, *CutoutPath, nullptr, LOAD_NoWarn);
}
