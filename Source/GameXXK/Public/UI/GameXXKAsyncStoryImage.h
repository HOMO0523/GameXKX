#pragma once
#include "Components/Image.h"
#include "GameXXKAsyncStoryImage.generated.h"
class UMaterialInstanceDynamic;

/** Owns one cancellable image request; source geometry is independent of texture compilation. */
UCLASS()
class GAMEXXK_API UGameXXKAsyncStoryImage : public UImage
{
	GENERATED_BODY()
public:
	void SetStoryTexture(const FSoftObjectPath& Path);
	void SetSoftEdges(bool bEnabled) { bSoftEdges=bEnabled; if(LoadedTexture)Present(LoadedTexture); }
	void ClearStoryTexture();
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	FSoftObjectPath GetRequestedPath() const { return RequestedPath; }
	UTexture2D* GetLoadedTexture() const { return LoadedTexture; }
private:
	void Present(UTexture2D* Texture);
	FSoftObjectPath RequestedPath;
	bool bSoftEdges = true;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadedTexture;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SoftEdgeMaterial;
};
