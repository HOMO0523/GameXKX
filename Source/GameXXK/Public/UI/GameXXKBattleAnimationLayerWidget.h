#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKBattleAnimationLayerWidget.generated.h"

class UCanvasPanel;

/**
 * Deprecated serialized compatibility shell. Runtime battle presentation is
 * owned by UGameXXKBattleBoardWidget and its persistent unit visuals.
 */
UCLASS()
class GAMEXXK_API UGameXXKBattleAnimationLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	void ResetPresentation();

	UCanvasPanel* GetRootCanvasForTest() const;
	int32 GetDuplicateParticipantImageCountForTest() const { return 0; }

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;
};
