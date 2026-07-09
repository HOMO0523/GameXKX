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
class USizeBox;
class UTextBlock;
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

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnRouteMapEntered();

private:
	void BuildProgrammaticLayout();
	void ConfigureProgrammaticLayout();
	bool ExecuteTownCommand(FName CommandName);
	UTextBlock* AddTextBlock(UVerticalBox* Parent, const FText& Text) const;
	UButton* AddCommandButton(UVerticalBox* Parent, const FText& Label) const;
	USizeBox* BuildItemSlotWidget(const FName& SlotName, UTextBlock*& OutSlotLabel) const;
	void ApplyItemSlotLabel(UTextBlock* SlotLabel, const FGameXXKTownItemSlotView& SlotView) const;
	TArray<FGameXXKTownItemSlotView> BuildPlayerBackpackSlotViews(const FGameXXKRuntimeState& State) const;
	TArray<FGameXXKTownItemSlotView> BuildShopStockSlotViews() const;

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
	TObjectPtr<UUniformGridPanel> InventoryGrid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> InventorySlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ShopStockSlotLabels;

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
};
