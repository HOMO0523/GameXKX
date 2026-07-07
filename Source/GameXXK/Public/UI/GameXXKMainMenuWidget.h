#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKMainMenuWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UOverlay;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

UENUM(BlueprintType)
enum class EGameXXKMainMenuLayer : uint8
{
	Landing,
	ContinueModal,
	DeleteConfirmModal,
	OptionsModal,
	QuitUnavailableModal
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMainMenuSaveSlotRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bCanLoad = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|MainMenu")
	bool bCanDelete = false;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKMainMenuWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ContinueGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu")
	bool DoesSaveGameExist(FString SlotName, int32 UserIndex = 0) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool DeleteSaveGame(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ContinueFromSlotIndex(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenContinueModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool RequestDeleteSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool ConfirmDeleteSlot();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool CancelDeleteSlot();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenOptionsModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool OpenQuitUnavailableModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool CloseActiveModal();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	void RefreshFromState();

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	TArray<FGameXXKMVPCommandDescriptor> BuildLandingActionsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	bool HasLandingActionForTest(FName CommandName, bool bRequireEnabled) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	TArray<FGameXXKMVPCommandDescriptor> BuildEncounterActionsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	bool HasEncounterActionForTest(FName CommandName, bool bRequireEnabled) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu|Test")
	bool ExecuteEncounterActionForTest(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	TArray<FGameXXKMainMenuSaveSlotRow> BuildSaveSlotRowsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	EGameXXKMainMenuLayer GetMenuLayerForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	int32 GetPendingDeleteSlotIndexForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu|Test")
	void SetSaveSlotUserIndexForTest(int32 UserIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MainMenu|Test")
	FName GetLastRequestedTownMapForTest() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|MainMenu")
	void OnStartGameSucceeded();

private:
	FGameXXKMainMenuSaveSlotRow BuildSaveSlotRow(int32 SlotIndex) const;
	FText BuildScreenLabel(const struct FGameXXKRuntimeState& State) const;
	void RequestPlayableMapForRuntimeState();
	void RequestOpenTownMap(FName MapName);
	void SetMenuLayer(EGameXXKMainMenuLayer NewLayer);
	bool IsValidManualSlotIndex(int32 SlotIndex) const;
	FString GetManualSlotNameChecked(int32 SlotIndex) const;
	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	bool IsEncounterScreen() const;
	bool ExecuteEncounterCommand(FName CommandName);
	UTextBlock* AddTextBlock(UVerticalBox* Parent, const FText& Text) const;
	UButton* AddMenuButton(UVerticalBox* Parent, const FText& Label, FName ButtonName = NAME_None, FName LabelName = NAME_None) const;
	void AddSaveSlotRow(UVerticalBox* Parent, const FGameXXKMainMenuSaveSlotRow& Row);
	void AddCloseButton(UVerticalBox* Parent);
	void HandleSaveSlotClicked(int32 SlotIndex);
	void HandleDeleteSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleStartGameClicked();

	UFUNCTION()
	void HandleOpenContinueClicked();

	UFUNCTION()
	void HandleOpenOptionsClicked();

	UFUNCTION()
	void HandleOpenQuitClicked();

	UFUNCTION()
	void HandleConfirmDeleteClicked();

	UFUNCTION()
	void HandleCancelDeleteClicked();

	UFUNCTION()
	void HandleCloseModalClicked();

	UFUNCTION()
	void HandleLoadSlot0Clicked();

	UFUNCTION()
	void HandleLoadSlot1Clicked();

	UFUNCTION()
	void HandleLoadSlot2Clicked();

	UFUNCTION()
	void HandleLoadSlot3Clicked();

	UFUNCTION()
	void HandleLoadSlot4Clicked();

	UFUNCTION()
	void HandleDeleteSlot0Clicked();

	UFUNCTION()
	void HandleDeleteSlot1Clicked();

	UFUNCTION()
	void HandleDeleteSlot2Clicked();

	UFUNCTION()
	void HandleDeleteSlot3Clicked();

	UFUNCTION()
	void HandleDeleteSlot4Clicked();

	UFUNCTION()
	void HandleResolveEventGoldClicked();

	UFUNCTION()
	void HandleResolveCampHealClicked();

	UFUNCTION()
	void HandleBuyHealingPowderClicked();

	UFUNCTION()
	void HandleSellHealingPowderClicked();

	UFUNCTION()
	void HandleUseHealingPowderClicked();

	UFUNCTION()
	void HandleCompleteMerchantNodeClicked();

	UPROPERTY(Transient)
	EGameXXKMainMenuLayer MenuLayer = EGameXXKMainMenuLayer::Landing;

	UPROPERTY(Transient)
	int32 PendingDeleteSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	int32 SaveSlotUserIndex = 0;

	UPROPERTY(Transient)
	FName LastRequestedTownMap;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CoverImage;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LandingBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> EncounterBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ModalBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ModalBox;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedMainMenuCoverTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedInkButtonTexture;
};
