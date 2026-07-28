#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKWorldMapWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKWorldMapWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|WorldMap")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|WorldMap")
	bool TrySelectRegion(FName RegionId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap")
	FName GetLastSelectedRegion() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap")
	bool WasLastSelectionUnlocked() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
	bool IsWorldMapVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
	bool IsRegionEnabledForTest(FName RegionId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
	FText GetSelectionNoticeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap|Test")
	FString GetTerrainResourcePathForTest() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|WorldMap")
	void OnLockedRegionSelected(FName RegionId);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|WorldMap")
	void OnUnlockedRegionSelected(FName RegionId);

private:
	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	bool HasPlayableTarget(FName RegionId) const;
	void SetSelectionNotice(const FText& Notice);

	UFUNCTION()
	void HandleQingshanClicked();

	UFUNCTION()
	void HandleTanjiangClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TerrainImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RegionPathsImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PlayerMarkerImage;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QingshanButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TanjiangButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectionNoticeText;

	UPROPERTY(Transient)
	FText SelectionNotice;

	UPROPERTY(Transient)
	FName LastSelectedRegion;

	UPROPERTY(Transient)
	bool bLastSelectionUnlocked = false;
};
