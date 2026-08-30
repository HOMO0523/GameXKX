#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKPrologueCarriageWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class GAMEXXK_API UGameXXKPrologueCarriageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	bool SetAtlasFrame(UTexture2D* Texture, int32 FrameIndex);

	static FBox2f FrameUvForTest(int32 FrameIndex);
	UImage* GetCarriageImageForTest() const { return CarriageImage.Get(); }
	int32 GetPresentedFrameForTest() const { return PresentedFrameIndex; }
	UTexture2D* GetPresentedTextureForTest() const { return PresentedTexture.Get(); }
	FBox2f GetPresentedUvForTest() const { return PresentedUv; }

private:
	void BuildProgrammaticLayout();
	static FBox2f FrameUv(int32 FrameIndex);

	UPROPERTY(Transient)
	TObjectPtr<UImage> CarriageImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PresentedTexture;

	int32 PresentedFrameIndex = INDEX_NONE;
	FBox2f PresentedUv = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
};
