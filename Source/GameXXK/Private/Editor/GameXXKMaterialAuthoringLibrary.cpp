#include "Editor/GameXXKMaterialAuthoringLibrary.h"

#include "Materials/Material.h"

bool UGameXXKMaterialAuthoringLibrary::ForceMaskedMaterialCompilation(UMaterial* Material)
{
#if WITH_EDITOR
	if (!IsValid(Material))
	{
		return false;
	}

	Material->Modify();
	Material->BlendMode = BLEND_Masked;
	Material->bCanMaskedBeAssumedOpaque = false;
	Material->ForceRecompileForRendering();
	Material->MarkPackageDirty();
	return Material->GetBlendMode() == BLEND_Masked;
#else
	return false;
#endif
}
