#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKInventoryWindowWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

UENUM(BlueprintType)
enum class EGameXXKInventoryWindowMode : uint8
{
	None,
	FreeInventory,
	MerchantTrade
};

enum class EGameXXKInventorySlotSource : uint8
{
	None,
	PlayerBackpack,
	MerchantStock,
	Equipment
};

UCLASS()
class GAMEXXK_API UGameXXKInventorySlotButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKInventoryWindowWidget* InOwner, EGameXXKInventorySlotSource InSource, int32 InSlotIndex, FName InEquipmentSlotId = NAME_None);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKInventoryWindowWidget> Owner;

	EGameXXKInventorySlotSource Source = EGameXXKInventorySlotSource::None;
	int32 SlotIndex = INDEX_NONE;
	FName EquipmentSlotId;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKInventoryWindowWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow")
	bool OpenFreeInventory();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow")
	bool OpenMerchantTrade();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow")
	bool CloseInventoryWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool OpenFreeInventoryForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool OpenMerchantTradeForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	EGameXXKInventoryWindowMode GetWindowModeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool HasWindowFrameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool HasCloseButtonForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool IsWindowVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool IsModalInputLockActiveForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetWindowFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetCloseButtonResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 GetBackpackSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetBackpackSlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetBackpackSlotIconResourcePathForTest(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 GetEquipmentSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetEquipmentSlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 GetMerchantStockSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetMerchantStockSlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FText GetSelectedPrimaryActionTextForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool SelectPlayerBackpackItemForTest(FName ItemId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool ExecuteSelectedPrimaryActionForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FName GetEquippedItemForSlotForTest(FName SlotId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool SelectMerchantStockSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool RequestSelectedBuyForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool ConfirmDialogForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool CancelDialogForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool IsConfirmationDialogVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool HasConfirmationConfirmButtonForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool HasConfirmationCancelButtonForTest() const;

	void HandleConfiguredSlotClicked(EGameXXKInventorySlotSource Source, int32 SlotIndex, FName EquipmentSlotId);

private:
	enum class EConfirmationAction : uint8
	{
		None,
		Buy,
		Sell
	};

	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	void RefreshBackpackSlots();
	void RefreshMerchantStockSlots();
	void RefreshEquipmentSlots();
	void RefreshDetailPanel();
	void RefreshConfirmationDialog();
	bool OpenInventoryWindow(EGameXXKInventoryWindowMode InMode);
	bool SelectPlayerBackpackItem(FName ItemId);
	bool SelectPlayerBackpackSlot(int32 SlotIndex);
	bool SelectMerchantStockSlot(int32 SlotIndex);
	bool SelectEquipmentSlot(FName SlotId);
	bool RequestBuyForSelectedItem();
	bool RequestSellForSelectedItem();
	bool ConfirmDialog();
	bool CancelDialog();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleSecondaryActionClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ModalBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FrameCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LeftRailFrame;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> EquipmentPanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> MerchantStockGrid;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DetailPanelFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedNameTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedDetailTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrimaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SecondaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SecondaryActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ConfirmationDialogFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmationPromptTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmationSummaryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmationConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmationCancelButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKInventorySlotButton>> BackpackSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BackpackSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> BackpackSelectedOverlays;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKInventorySlotButton>> MerchantStockSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MerchantStockSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MerchantStockSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MerchantStockSelectedOverlays;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKInventorySlotButton>> EquipmentSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EquipmentSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EquipmentSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EquipmentSelectedOverlays;

	EGameXXKInventoryWindowMode WindowMode = EGameXXKInventoryWindowMode::None;
	EGameXXKInventorySlotSource SelectedSlotSource = EGameXXKInventorySlotSource::None;
	EConfirmationAction PendingConfirmationAction = EConfirmationAction::None;
	FName SelectedItemId;
	int32 SelectedSlotIndex = INDEX_NONE;
	FName SelectedEquipmentSlotId;
	FName PendingConfirmationItemId;
	int32 PendingConfirmationQuantity = 0;
	int32 PendingConfirmationPrice = 0;
	FText CurrentPrimaryActionText;
	FText CurrentSecondaryActionText;
	TArray<FName> CurrentBackpackSlotItemIds;
	TArray<FString> CurrentBackpackSlotIconPaths;
	TArray<FName> CurrentMerchantStockSlotItemIds;
	TArray<FName> CurrentEquipmentSlotItemIds;
};
