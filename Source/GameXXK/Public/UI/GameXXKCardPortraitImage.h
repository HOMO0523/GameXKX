#pragma once
#include "Components/Image.h"
#include "GameXXKCardPortraitImage.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;

/** UImage-compatible card art with a shared material mask in the card's local space. */
UCLASS()
class GAMEXXK_API UGameXXKCardPortraitImage : public UImage
{
	GENERATED_BODY()
public:
	void SetCardFace(UWidget* Face) { CardFace = Face; }
	const FSlateBrush* PrepareMaskedBrush(const FGeometry& ImageGeometry, const FSlateBrush& SourceBrush);
	UFUNCTION(BlueprintPure, Category="GameXXK|UI|Test")
	bool UsesCardGeometryForTest() const { return bUsesCardGeometry; }
	UFUNCTION(BlueprintPure, Category="GameXXK|UI|Test")
	FVector2D GetMaskCardSizeForTest() const { return FVector2D(CachedCardSize); }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TWeakObjectPtr<UWidget> CardFace;
	TWeakObjectPtr<UTexture2D> CachedTexture;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaskMaterial;
	FSlateBrush MaskBrush;
	FVector2f CachedCardSize = FVector2f::ZeroVector;
	FVector2f CachedOrigin = FVector2f::ZeroVector;
	FVector2f CachedAxisX = FVector2f::ZeroVector;
	FVector2f CachedAxisY = FVector2f::ZeroVector;
	bool bUsesCardGeometry = false;
};
