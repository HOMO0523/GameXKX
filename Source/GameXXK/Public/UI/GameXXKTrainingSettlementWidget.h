#pragma once
#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTrainingSettlementTypes.h"
#include "GameXXKTrainingSettlementWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UCanvasPanel;
class UProgressBar;

/** Read-only projection of already-applied clear rewards; confirmation never grants loot. */
UCLASS()
class GAMEXXK_API UGameXXKTrainingSettlementWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	void SetReceipt(const FGameXXKTrainingSettlementReceipt& InReceipt);
	UFUNCTION(BlueprintPure, Category="GameXXK|Training|Test") FGuid GetReceiptIdForTest() const { return Receipt.ReceiptId; }
	UFUNCTION(BlueprintPure, Category="GameXXK|Training|Test") int32 GetDisplayedGoldForTest() const { return Receipt.Gold; }
	UFUNCTION(BlueprintCallable, Category="GameXXK|Training|Test") bool ConfirmForTest();
private:
	void EnsureLayout();
	void RefreshReceipt();
	UFUNCTION() void HandleConfirm();
	UPROPERTY(Transient) FGameXXKTrainingSettlementReceipt Receipt;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Page;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StageText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FirstClearText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GoldText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GoldDetail;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ExperienceText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ChestText;
	UPROPERTY(Transient) TObjectPtr<UImage> ChestImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatsText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UnlockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ErrorText;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmButton;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MemberNames;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MemberLevels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MemberExperience;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> MemberBars;
	bool bConfirmed = false;
};
