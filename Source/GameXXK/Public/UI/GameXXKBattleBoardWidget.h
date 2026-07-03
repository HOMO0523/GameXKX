#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "Input/Reply.h"
#include "GameXXKBattleBoardWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKBattleBoardWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecutePrimaryEnemyAction();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	bool IsBattleBoardVisible() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	int32 GetEnemySlotCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	int32 GetPartySlotCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	FString GetEnemySlotSide() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	FString GetPartySlotSide() const;

	FVector2D GetEnemySlotPositionForTest(int32 SlotIndex) const;
	FVector2D GetPartySlotPositionForTest(int32 SlotIndex) const;

private:
	void BuildProgrammaticLayout();
	void ConfigureSlots();
	UBorder* CreateSlotBorder(const FString& Name, const FLinearColor& Color, const FText& Label);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> EnemySlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PartySlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PartyLabels;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnemyLabel;
};
