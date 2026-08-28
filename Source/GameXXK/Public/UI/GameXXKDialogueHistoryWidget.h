#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKDialogueHistoryWidget.generated.h"

class UBorder;
class UScrollBox;
class UVerticalBox;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKDialogueHistoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void PresentHistory(const TArray<FGameXXKDialogueHistoryEntry>& Entries);
	void HideHistory();
	int32 GetHistoryCountForTest() const;
	FGameXXKDialogueHistoryEntry GetHistoryEntryForTest(int32 Index) const;
	bool IsReadOnlyForTest() const;

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PaperFrame;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> HistoryScroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> HistoryRows;

	TArray<FGameXXKDialogueHistoryEntry> VisibleEntries;
};
