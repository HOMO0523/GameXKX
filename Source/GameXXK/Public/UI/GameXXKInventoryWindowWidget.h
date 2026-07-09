#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKInventoryWindowWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

UENUM(BlueprintType)
enum class EGameXXKInventoryWindowMode : uint8
{
	None,
	FreeInventory,
	MerchantTrade
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
	bool IsModalInputLockActiveForTest() const;

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

private:
	enum class ESelectedSlotSource : uint8
	{
		None,
		PlayerBackpack,
		MerchantStock,
		Equipment
	};

	enum class EConfirmationAction : uint8
	{
		None,
		Buy,
		Sell
	};

	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	bool OpenInventoryWindow(EGameXXKInventoryWindowMode InMode);
	bool SelectPlayerBackpackItem(FName ItemId);
	bool SelectMerchantStockSlot(int32 SlotIndex);
	bool RequestBuyForSelectedItem();
	bool ConfirmDialog();
	bool CancelDialog();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ConfirmationDialogFrame;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmationConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmationCancelButton;

	EGameXXKInventoryWindowMode WindowMode = EGameXXKInventoryWindowMode::None;
	ESelectedSlotSource SelectedSlotSource = ESelectedSlotSource::None;
	EConfirmationAction PendingConfirmationAction = EConfirmationAction::None;
	FName SelectedItemId;
	FName PendingConfirmationItemId;
	int32 PendingConfirmationQuantity = 0;
};
