#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKMetaShopTypes.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKMetaShopWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UUniformGridPanel;
class UGameXXKMetaShopWidget;

UCLASS()
class GAMEXXK_API UGameXXKMetaShopProductButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKMetaShopWidget* InOwner, EGameXXKMetaShopProductId InProductId);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMetaShopWidget> Owner;

	EGameXXKMetaShopProductId ProductId = EGameXXKMetaShopProductId::Invalid;
};

/** Dedicated player-facing seven-product permanent-currency shop. */
UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKMetaShopWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	bool SelectProduct(EGameXXKMetaShopProductId ProductId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	bool RequestPurchase();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	bool ConfirmPurchase();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	bool CancelPurchase();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
	void CloseMetaShop();

	void SetCloseRequestedDelegate(FSimpleDelegate InDelegate);
	void SetCompanionReplacementRequestedDelegate(FSimpleDelegate InDelegate);

	bool OpenMetaShopForTest();
	bool SelectProductForTest(EGameXXKMetaShopProductId ProductId);
	bool RequestPurchaseForTest();
	bool ConfirmPurchaseForTest();
	bool CancelPurchaseForTest();
	int32 GetProductCardCountForTest() const;
	FText GetDisabledReasonForTest() const;
	FGameXXKMetaShopPurchaseResult GetLastPurchaseResultForTest() const;

private:
	friend class UGameXXKMetaShopProductButton;

	void BuildProgrammaticLayout();
	void ApplyProducts(const TArray<FGameXXKMetaShopProductDefinition>& Products);
	void UpdateSelectedProduct();
	FText BuildProductDescription(const FGameXXKMetaShopProductDefinition& Product) const;
	FText BuildPurchaseResultText() const;

	UFUNCTION()
	void HandlePurchaseClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> ProductGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailPriceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DisabledReasonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ConfirmOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ResultPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKMetaShopProductButton>> ProductButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ProductImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ProductNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ProductPriceTexts;

	TArray<FGameXXKMetaShopProductDefinition> CurrentProducts;
	EGameXXKMetaShopProductId SelectedProductId = EGameXXKMetaShopProductId::Invalid;
	FGameXXKMetaShopPurchasePreview SelectedPreview;
	FGameXXKMetaShopPurchaseResult LastPurchaseResult;
	FText DisabledReason;
	FSimpleDelegate CloseRequestedDelegate;
	FSimpleDelegate CompanionReplacementRequestedDelegate;
};
