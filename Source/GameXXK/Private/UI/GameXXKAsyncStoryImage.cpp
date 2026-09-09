#include "UI/GameXXKAsyncStoryImage.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UGameXXKAsyncStoryImage::ClearStoryTexture()
{
	CancelImageStreaming();
	RequestedPath.Reset();
	LoadedTexture=nullptr;
	if(SoftEdgeMaterial) SoftEdgeMaterial->ClearParameterValues();
	SoftEdgeMaterial=nullptr;
	FSlateBrush Empty;Empty.ImageSize=FVector2D(1536,512);Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
	SetBrush(Empty);
}

void UGameXXKAsyncStoryImage::SetStoryTexture(const FSoftObjectPath& Path)
{
	if(RequestedPath==Path && LoadedTexture) return;
	ClearStoryTexture();
	RequestedPath=Path;
	if(Path.IsNull()) return;
	const TWeakObjectPtr<UGameXXKAsyncStoryImage> WeakThis(this);
	const TSoftObjectPtr<UTexture2D> Texture(Path);
	RequestAsyncLoad(Texture,[WeakThis,Texture,Path]()
	{
		if(auto* Self=WeakThis.Get(); Self && Self->RequestedPath==Path)
			Self->Present(Texture.Get());
	});
}

void UGameXXKAsyncStoryImage::Present(UTexture2D* Texture)
{
	LoadedTexture=Texture;
	if(!Texture) return;
	FSlateBrush ImageBrush;ImageBrush.ImageSize=FVector2D(1536,512);ImageBrush.DrawAs=ESlateBrushDrawType::Image;
	// SetBrushFromTexture(..., true) waits on editor compilation and forces residency.
	// A fixed-ratio brush needs neither and keeps the async completion non-reentrant.
	if(auto* Base=bSoftEdges?LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/GameXXK/UI/StoryNodes/M_StoryIllustrationSoftEdge.M_StoryIllustrationSoftEdge")):nullptr)
	{
		SoftEdgeMaterial=UMaterialInstanceDynamic::Create(Base,this);
		SoftEdgeMaterial->SetTextureParameterValue(TEXT("Illustration"),Texture);
		ImageBrush.SetResourceObject(SoftEdgeMaterial);
	}
	else {SoftEdgeMaterial=nullptr;ImageBrush.SetResourceObject(Texture);}
	SetBrush(ImageBrush);
}

void UGameXXKAsyncStoryImage::ReleaseSlateResources(bool bReleaseChildren)
{
	const FSoftObjectPath DesiredPath=RequestedPath;
	ClearStoryTexture();
	// Releasing a Slate instance is not a semantic close. Reattachment must be
	// able to restore its desired picture without another gameplay state change.
	RequestedPath=DesiredPath;
	Super::ReleaseSlateResources(bReleaseChildren);
}

TSharedRef<SWidget> UGameXXKAsyncStoryImage::RebuildWidget()
{
	const TSharedRef<SWidget> Result=Super::RebuildWidget();
	const FSoftObjectPath DesiredPath=RequestedPath;
	if(!DesiredPath.IsNull())SetStoryTexture(DesiredPath);
	return Result;
}
