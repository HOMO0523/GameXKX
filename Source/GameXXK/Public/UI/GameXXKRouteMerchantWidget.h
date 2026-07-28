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
class UUniformGridPanel;
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

/** Carries one stable route-card EntryId from the replacement picker back to the merchant widget. */
UCLASS()
class GAMEXXK_API UGameXXKRouteMerchantReplacementButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKRouteMerchantWidget* InOwner, FName InReplacementEntryId);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteMerchantWidget> Owner;

	FName ReplacementEntryId = NAME_None;
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
	bool PurchaseOffer(FName OfferId, FName ReplacementEntryId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool SelectReplacementEntry(FName ReplacementEntryId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool RefreshStock();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMerchant")
	bool CancelPendingPurchase();

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
	FVector2D GetRelicFrameSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FString GetCardFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetRenderedCardOfferCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetRenderedRelicOfferCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	int32 GetOfferTooltipCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool HasOnlyButtonHitTargetsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool IsOfferPurchaseEnabledForTest(FName OfferId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FString GetOfferDisabledReasonForTest(FName OfferId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	FText GetRouteTravelMoneyTextForTest() const;

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	TArray<FName> GetEligibleReplacementEntryIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMerchant|Test")
	bool IsReplacementSelectionVisibleForTest() const;

private:
	friend class UGameXXKRouteMerchantOfferButton;
	friend class UGameXXKRouteMerchantReplacementButton;

	void BuildProgrammaticLayout();
	USizeBox* BuildOfferCell(EGameXXKRouteMerchantOfferKind Kind, int32 GlobalOfferIndex);
	void ApplyView(const FGameXXKRouteMerchantView& View);
	void ApplyOffer(int32 GlobalOfferIndex, const FGameXXKRouteMerchantOfferView* OfferView, EGameXXKRouteMerchantOfferKind ExpectedKind);
	void RestorePendingReplacementSelection(const UGameXXKMVPSubsystem* Subsystem, const FGameXXKRouteMerchantView& View);
	void ApplyReplacementSelection(const FGameXXKRouteMerchantView& View);
	void UpdateLastActionErrorDisplay();
	FText BuildReplacementEntryLabel(FName ReplacementEntryId) const;
	FText BuildOfferTooltip(const FGameXXKRouteMerchantOfferView* OfferView, EGameXXKRouteMerchantOfferKind ExpectedKind, const FString& DisabledReason) const;
	FString ResolveDisabledReason(const FGameXXKRouteMerchantOfferView* OfferView) const;
	void ClearTransientInteractionState();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleCancelClicked();

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
	TObjectPtr<UTextBlock> RouteTravelMoneyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LastActionErrorText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ReplacementSelectionPanel;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> ReplacementSelectionGrid;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RefreshButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CancelButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LeaveButtonText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteMerchantOfferButton>> OfferDisplayButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteMerchantOfferButton>> OfferPurchaseButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> OfferArtImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> OfferTitleBars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferPriceTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferStatusTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OfferPurchaseTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteMerchantReplacementButton>> ReplacementSelectionButtons;

	UPROPERTY(Transient)
	FGameXXKRouteMerchantView CachedView;

	UPROPERTY(Transient)
	FGameXXKRouteMerchantPurchaseResult LastPurchaseResult;

	TArray<FName> RenderedOfferIds;
	TArray<FText> OfferTooltips;
	TArray<FString> OfferDisabledReasons;
	TArray<FName> RenderedReplacementEntryIds;
	FString LastActionError;
};
