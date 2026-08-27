#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKRouteMerchantTypes.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKRouteMerchantWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class USafeZone;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UGameXXKRouteMerchantWidget;

/** Carries one stable offer identity from a programmatic UMG button to the merchant widget. */
UCLASS()
class GAMEXXK_API UGameXXKRouteMerchantOfferButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKRouteMerchantWidget* InOwner, FName InOfferId, bool bInPurchaseAction);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteMerchantWidget> Owner;

	FName OfferId = NAME_None;
	bool bPurchaseAction = false;
};

/**
 * Dedicated 1920x1080 safe-area merchant HUD. It renders the subsystem read-model,
 * and every mutation is routed back through the subsystem facade.
 */
UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKRouteMerchantWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool PurchaseOffer(FName OfferId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool RefreshStock();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool LeaveMerchant();

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FVector2D GetDesignResolutionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	float GetMerchantColumnFractionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	float GetOffersColumnFractionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FVector2D GetCardFrameSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FString GetCardFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetRenderedCardOfferCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetRenderedRelicOfferCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool HasTopRightCloseButtonForTest() const { return false; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetOfferTooltipCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool HasOnlyButtonHitTargetsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool IsOfferPurchaseEnabledForTest(FName OfferId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FString GetOfferDisabledReasonForTest(FName OfferId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FText GetOrdinaryGoldTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FText GetRefreshButtonTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FText GetLeaveButtonTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FGameXXKRouteMerchantPurchaseResult GetLastPurchaseResultForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FString GetLastActionErrorForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FText GetDisplayedLastActionErrorForTest() const;

private:
	friend class UGameXXKRouteMerchantOfferButton;

	void BuildProgrammaticLayout();
	USizeBox* BuildOfferCell(EGameXXKRouteMerchantOfferKind Kind, int32 GlobalOfferIndex);
	void ApplyView(const FGameXXKRouteMerchantView& View);
	void ApplyOffer(int32 GlobalOfferIndex, const FGameXXKRouteMerchantOfferView* OfferView, EGameXXKRouteMerchantOfferKind ExpectedKind);
	void UpdateLastActionErrorDisplay();
	FText BuildOfferTooltip(const FGameXXKRouteMerchantOfferView* OfferView, EGameXXKRouteMerchantOfferKind ExpectedKind, const FString& DisabledReason) const;
	FText ResolveOwnerLabel(FName OwnerMemberId) const;
	FString ResolveDisabledReason(const FGameXXKRouteMerchantOfferView* OfferView) const;
	void ClearTransientInteractionState();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(Transient)
	TObjectPtr<USafeZone> RootSafeArea;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> ResponsiveScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> DesignSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> CardOfferRow;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RelicOfferRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OrdinaryGoldText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LastActionErrorText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefreshButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LeaveButtonText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteMerchantOfferButton>> OfferDisplayButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteMerchantOfferButton>> OfferPurchaseButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> OfferArtImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferArtUnavailableTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferTitleBars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferOwnerTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferQualityTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferEffectTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferPriceTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferStatusTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferPurchaseTexts;

	UPROPERTY(Transient)
	FGameXXKRouteMerchantPurchaseResult LastPurchaseResult;

	TArray<FName> RenderedOfferIds;
	TArray<FText> OfferTooltips;
	TArray<FString> OfferDisabledReasons;
	FString LastActionError;
};
