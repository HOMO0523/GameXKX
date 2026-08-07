#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKInventoryWindowWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UOverlay;
class UScrollBox;
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

UENUM(BlueprintType)
enum class EGameXXKInventoryFilter : uint8
{
	All,
	Equipment,
	Props,
	Materials,
	Tasks
};

UENUM(BlueprintType)
enum class EGameXXKCharacterBackpackTab : uint8
{
	Attributes,
	Equipment,
	Deck,
	Talents,
	Titles
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

	bool HandleRightMouseButtonDown();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKInventoryWindowWidget> Owner;

	EGameXXKInventorySlotSource Source = EGameXXKInventorySlotSource::None;
	int32 SlotIndex = INDEX_NONE;
	FName EquipmentSlotId;
};

UCLASS()
class GAMEXXK_API UGameXXKInventoryFilterButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKInventoryWindowWidget* InOwner, EGameXXKInventoryFilter InFilter);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKInventoryWindowWidget> Owner;

	EGameXXKInventoryFilter Filter = EGameXXKInventoryFilter::All;
};

UCLASS()
class GAMEXXK_API UGameXXKCharacterBackpackTabButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKInventoryWindowWidget* InOwner, EGameXXKCharacterBackpackTab InTab);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKInventoryWindowWidget> Owner;

	EGameXXKCharacterBackpackTab Tab = EGameXXKCharacterBackpackTab::Equipment;
};

UCLASS()
class GAMEXXK_API UGameXXKHeroDeckCardButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKInventoryWindowWidget* InOwner, FName InCardId);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKInventoryWindowWidget> Owner;

	FName CardId = NAME_None;
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
	int32 GetBackpackColumnCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 GetBackpackStorageCapacityForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	bool HasBackpackScrollBoxForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetScrollbarResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetSelectionInkResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetTooltipResourcePathForTest() const;

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
	bool QuickEquipBackpackInstanceForTest(FName InstanceId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool QuickUnequipSlotForTest(EGameXXKEquipmentSlot EquipmentSlot);

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FName GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot EquipmentSlot) const;

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	EGameXXKInventoryFilter GetActiveInventoryFilterForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool SelectInventoryFilterForTest(EGameXXKInventoryFilter Filter);

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	TArray<FName> GetVisibleBackpackItemIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	TArray<FName> GetVisibleBackpackEquipmentInstanceIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 FindBackpackEquipmentInstanceSlotForTest(FName InstanceId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 FindBackpackItemSlotForTest(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	int32 GetSelectedBackpackSlotIndexForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool SortInventoryForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool RequestSelectedDecomposeForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|InventoryWindow|Test")
	bool RequestSelectedEnhanceForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FText GetSelectedDetailTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|InventoryWindow|Test")
	FString GetInventoryFilterTexturePathForTest(EGameXXKInventoryFilter Filter) const;
	int32 GetCharacterTabButtonCountForTest() const;
	EGameXXKCharacterBackpackTab GetActiveCharacterBackpackTabForTest() const;
	bool OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab Tab);
	FText GetCharacterTabBodyTextForTest() const;
	TArray<FName> GetHeroCardBackpackIdsForTest() const;
	TArray<FName> GetPendingHeroDeckIdsForTest() const;
	bool ToggleHeroDeckCardForTest(FName CardId);
	bool ApplyHeroDeckForTest();
	FString GetHeroCardFrameResourcePathForTest() const;
	FString GetHeroLockedCardIconResourcePathForTest() const;

	void HandleConfiguredSlotClicked(EGameXXKInventorySlotSource Source, int32 SlotIndex, FName EquipmentSlotId);
	bool HandleConfiguredSlotRightClicked(EGameXXKInventorySlotSource Source, int32 SlotIndex, FName EquipmentSlotId);
	void HandleInventoryFilterClicked(EGameXXKInventoryFilter Filter);
	void HandleCharacterBackpackTabClicked(EGameXXKCharacterBackpackTab Tab);
	void HandleHeroDeckCardClicked(FName CardId);

private:
	enum class EConfirmationAction : uint8
	{
		None,
		Buy,
		Sell,
		Decompose,
		Enhance
	};

	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	void RefreshBackpackSlots();
	void RefreshMerchantStockSlots();
	void RefreshEquipmentSlots();
	void RefreshDetailPanel();
	void RefreshConfirmationDialog();
	void RefreshCharacterTabs();
	void RefreshHeroDeckCards();
	bool OpenInventoryWindow(EGameXXKInventoryWindowMode InMode);
	bool SelectPlayerBackpackItem(FName ItemId);
	bool SelectPlayerBackpackSlot(int32 SlotIndex);
	bool SelectMerchantStockSlot(int32 SlotIndex);
	bool SelectEquipmentSlot(FName SlotId);
	bool SelectInventoryFilter(EGameXXKInventoryFilter Filter);
	bool SortInventory();
	bool RequestBuyForSelectedItem();
	bool RequestSellForSelectedItem();
	bool RequestDecomposeForSelectedItem();
	bool RequestEnhanceForSelectedItem();
	bool ConfirmDialog();
	bool CancelDialog();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleSecondaryActionClicked();

	UFUNCTION()
	void HandleSortClicked();

	UFUNCTION()
	void HandleBackpackScrolled(float CurrentOffset);

	void UpdateBackpackScrollbarThumb();

	UFUNCTION()
	void HandleDecomposeClicked();

	UFUNCTION()
	void HandleEnhanceMainClicked();

	UFUNCTION()
	void HandleReforgeMainClicked();

	void ShowActionErrorText(const FText& Message);

	UFUNCTION()
	void HandleEnhanceClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleApplyHeroDeckClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ModalBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FrameCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCharacterBackpackTabButton>> CharacterTabButtons;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CharacterTabBodyPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CharacterTabBodyText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HeroDeckPanel;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> HeroDeckGrid;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> HeroDeckScrollBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKHeroDeckCardButton>> HeroDeckCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeroDeckCardPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeroDeckCardLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeroDeckLockedIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeroDeckSelectedInks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeroDeckCostLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeroDeckManaCostLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeroDeckTooltipNameBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeroDeckTooltipDetailBlocks;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyHeroDeckButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroDeckCountText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LeftRailFrame;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> EquipmentPanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CentralHeroIdleImage;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> MerchantStockGrid;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BackpackScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> InventoryScrollbarThumb;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BackpackSelectionInk;

	/** Frame-canvas selection ink that hovers above the selected equipment slot. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> EquipmentSelectionInk;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKInventoryFilterButton>> InventoryFilterButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> InventoryFilterTextBlocks;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DecomposeButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnhanceMainButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReforgeMainButton;

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
	TObjectPtr<UButton> EnhanceButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnhanceActionTextBlock;

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
	TArray<TObjectPtr<UBorder>> BackpackTooltipFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackTooltipNameTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackTooltipDetailTextBlocks;

	/** Replaced-slot comparison rows (red gain / green loss) inside each backpack tooltip. */
	TArray<TArray<TObjectPtr<UTextBlock>>> BackpackCompareTextBlocks;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> EquipmentTooltipFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EquipmentTooltipNameTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EquipmentTooltipDetailTextBlocks;

	EGameXXKInventoryWindowMode WindowMode = EGameXXKInventoryWindowMode::None;
	EGameXXKInventorySlotSource SelectedSlotSource = EGameXXKInventorySlotSource::None;
	EConfirmationAction PendingConfirmationAction = EConfirmationAction::None;
	FName SelectedItemId;
	FName SelectedEquipmentInstanceId;
	int32 SelectedSlotIndex = INDEX_NONE;
	FName SelectedEquipmentSlotId;
	FName PendingConfirmationItemId;
	FName PendingConfirmationEquipmentInstanceId;
	int32 PendingConfirmationQuantity = 0;
	int32 PendingConfirmationPrice = 0;
	FText CurrentPrimaryActionText;
	FText CurrentSecondaryActionText;
	EGameXXKInventoryFilter ActiveInventoryFilter = EGameXXKInventoryFilter::All;
	EGameXXKCharacterBackpackTab ActiveCharacterTab = EGameXXKCharacterBackpackTab::Equipment;
	bool bBackpackSorted = false;
	TArray<FName> CurrentBackpackSlotItemIds;
	TArray<FName> CurrentBackpackSlotEquipmentInstanceIds;
	TArray<int32> CurrentBackpackSlotQuantities;
	TArray<FString> CurrentBackpackSlotIconPaths;
	TArray<FName> CurrentMerchantStockSlotItemIds;
	TArray<FName> CurrentEquipmentSlotItemIds;
	TArray<FName> HeroCardBackpackIds;
	TArray<FName> UnlockedHeroCardIds;
	TArray<FName> PendingHeroDeckIds;
	FGameXXKCharacterBackpackModel CharacterBackpackModel;
};
