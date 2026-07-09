#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTownOverlayWidget.generated.h"

class UButton;
class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UVerticalBox;

enum class EGameXXKTownItemSlotSource : uint8
{
	PlayerInventory,
	ShopStock,
	Equipment
};

struct FGameXXKTownItemSlotView
{
	EGameXXKTownItemSlotSource Source = EGameXXKTownItemSlotSource::PlayerInventory;
	FName ItemId;
	int32 Quantity = 0;
	FText Label;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTownOverlayWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool EnterRouteMap();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool SaveToSlotOne();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Test")
	bool ExecuteTownCommandForTest(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	TArray<FGameXXKMVPCommandDescriptor> BuildTownActionsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	bool HasTownActionForTest(FName CommandName, bool bRequireEnabled) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	bool IsTownOverlayVisible() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FText GetStatusTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	EGameXXKTownPanelMode GetActiveTownPanelForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	int32 GetInventorySlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FString GetInventorySlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	int32 GetShopStockSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FString GetShopStockSlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FName GetShopStockSlotItemIdForTest(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FName GetPlayerBackpackSlotItemIdForTest(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FString GetItemIconResourcePathForTest(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Test")
	bool SelectShopStockSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	bool IsPurchaseConfirmationVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FName GetPendingPurchaseItemIdForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Test")
	bool ConfirmPendingPurchaseForTest();

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnRouteMapEntered();

private:
	void BuildProgrammaticLayout();
	void ConfigureProgrammaticLayout();
	bool ExecuteTownCommand(FName CommandName);
	UTextBlock* AddTextBlock(UVerticalBox* Parent, const FText& Text) const;
	UButton* AddCommandButton(UVerticalBox* Parent, const FText& Label) const;
	USizeBox* BuildItemSlotWidget(const FName& SlotName, UTextBlock*& OutSlotLabel, UImage*& OutSlotIcon, UButton*& OutSlotButton) const;
	void ApplyItemSlotVisual(UTextBlock* SlotLabel, UImage* SlotIcon, const FGameXXKTownItemSlotView& SlotView) const;
	FString ResolveItemIconTexturePath(FName ItemId) const;
	UTexture2D* LoadItemIconTexture(FName ItemId) const;
	TArray<FGameXXKTownItemSlotView> BuildPlayerBackpackSlotViews(const FGameXXKRuntimeState& State) const;
	TArray<FGameXXKTownItemSlotView> BuildShopStockSlotViews() const;
	bool SelectShopStockSlot(int32 SlotIndex);
	bool ConfirmPendingPurchase();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleEnterRouteClicked();

	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleCharacterClicked();

	UFUNCTION()
	void HandleTradeClicked();

	UFUNCTION()
	void HandleClosePanelClicked();

	UFUNCTION()
	void HandleConfirmPurchaseClicked();

	UFUNCTION()
	void HandleCancelPurchaseClicked();

	UFUNCTION()
	void HandleShopStockSlot0Clicked();

	UFUNCTION()
	void HandleShopStockSlot1Clicked();

	UFUNCTION()
	void HandleShopStockSlot2Clicked();

	UFUNCTION()
	void HandleShopStockSlot3Clicked();

	UFUNCTION()
	void HandleShopStockSlot4Clicked();

	UFUNCTION()
	void HandleShopStockSlot5Clicked();

	UFUNCTION()
	void HandleShopStockSlot6Clicked();

	UFUNCTION()
	void HandleShopStockSlot7Clicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ActivePanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ActivePanelBackplate;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ActivePanelColumns;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActivePanelTitleBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActivePanelBodyBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> EquipmentPanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeaponSlotTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ArmorSlotTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AccessorySlotTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ShopStockPanel;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> ShopStockGrid;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PurchaseConfirmationPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PurchaseConfirmationTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmPurchaseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelPurchaseButton;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> InventorySlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> InventorySlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ShopStockSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ShopStockSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> ShopStockSlotButtons;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InventoryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CharacterButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClosePanelButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnterRouteButton;

	UPROPERTY(Transient)
	FText CachedStatusText;

	TArray<FName> CurrentPlayerBackpackSlotItemIds;
	TArray<FName> CurrentShopStockSlotItemIds;
	FName PendingPurchaseItemId;
};
