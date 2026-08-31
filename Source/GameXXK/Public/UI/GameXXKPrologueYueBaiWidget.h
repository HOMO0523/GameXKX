#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKPrologueYueBaiWidget.generated.h"

class UImage;
class UTexture2D;

UCLASS()
class GAMEXXK_API UGameXXKPrologueYueBaiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	bool SetAtlasFrame(UTexture2D* Texture, int32 FrameIndex);

	static FBox2f FrameUvForTest(int32 FrameIndex);
	static constexpr int32 GetFrameCountForTest() { return 60; }
	static constexpr float GetFramesPerSecondForTest() { return 56.074766f; }
	static constexpr float GetIntroDurationSecondsForTest() { return 1.07f; }
	static FString GetTexturePathForTest(bool bLowResolution);
	int32 GetImageCountForTest() const { return YueBaiImage ? 1 : 0; }
	int32 GetPresentedFrameForTest() const { return PresentedFrameIndex; }

private:
	void BuildProgrammaticLayout();
	static FBox2f FrameUv(int32 FrameIndex);

	UPROPERTY(Transient)
	TObjectPtr<UImage> YueBaiImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PresentedTexture;

	int32 PresentedFrameIndex = INDEX_NONE;
};
