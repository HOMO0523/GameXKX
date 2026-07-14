#include "Editor/GameXXKMaterialAuthoringLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"

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

bool UGameXXKMaterialAuthoringLibrary::BindCollectionParameter(
	UMaterialExpressionCollectionParameter* Expression,
	UMaterialParameterCollection* Collection,
	FName ParameterName)
{
#if WITH_EDITOR
	if (!IsValid(Expression) || !IsValid(Collection) || ParameterName.IsNone())
	{
		return false;
	}

	const FGuid ParameterId = Collection->GetParameterId(ParameterName);
	if (!ParameterId.IsValid())
	{
		return false;
	}

	Expression->Modify();
	Expression->Collection = Collection;
	Expression->ParameterName = ParameterName;
	Expression->ParameterId = ParameterId;
	return Expression->Collection == Collection
		&& Expression->ParameterName == ParameterName
		&& Expression->ParameterId == ParameterId;
#else
	return false;
#endif
}
