#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameXXKBattleMechanicPresentation.h"
#include "GameXXKBattleUnitMechanicsWidget.generated.h"

class UVerticalBox;
class UImage;
class UTextBlock;

/** First row of the status area: real spell-task slots and opened prescriptions. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitMechanicsWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	bool PrepareForBoardEmbedding();
	void SetMechanicView(const FGameXXKUnitMechanicView& View);
	bool MatchesMechanicView(const FGameXXKUnitMechanicView& View) const;
	void AdvancePresentation(float DeltaSeconds, bool bCombatPresentationPending = false);
	void ResetPresentation();
	float GetLogicalHeight() const;

	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	FString GetTaskElementForTest() const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	int32 GetTaskCardCountForTest() const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	int32 GetTaskCardOrderForTest(int32 Index) const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	int32 GetFormulaCountForTest() const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	FString GetTaskTooltipForTest() const;
	UFUNCTION(BlueprintPure, Category="GameXXK|Battle|Test", meta=(DevelopmentOnly))
	FString GetFormulaTooltipForTest(int32 Index) const;

protected:
	virtual void NativeConstruct() override;
private:
	void EnsureWidgetTree();
	void RebuildPresentation(const FGameXXKUnitMechanicView* Previous = nullptr);
	const FGameXXKMechanicTaskView& DisplayedTask() const;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
	FGameXXKUnitMechanicView Current;
	FGameXXKMechanicTaskView Completion;
	FString CurrentSignature;
	bool bInitializedView = false;
	float CompletionSeconds = 0.0f;
	struct FPulse
	{
		TWeakObjectPtr<UWidget> Tile;
		TWeakObjectPtr<UImage> Front;
		TWeakObjectPtr<UTextBlock> Back;
		TWeakObjectPtr<UTextBlock> Number;
		float Elapsed = 0.0f;
		bool bFlip = false;
	};
	TArray<FPulse> Pulses;
};
