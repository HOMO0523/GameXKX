#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKOneGameRouteMapWidget.generated.h"

class UButton;
class UBorder;
class UCanvasPanel;
class UImage;
class UOverlay;
class UScrollBox;
class USizeBox;
class UTexture2D;
class UTextBlock;
class UUserWidget;
class UWidget;

UENUM(BlueprintType)
enum class EGameXXKOneGameRouteRoomType : uint8
{
	Start,
	SmallEnemy,
	EliteEnemy,
	Camp,
	Chest,
	Merchant,
	RandomEvent,
	Boss
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKOneGameRouteNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName CommandName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Label;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Start;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKOneGameRouteRoomType RoomType = EGameXXKOneGameRouteRoomType::Start;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 NodeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bEnabled = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bVisited = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKOneGameRouteNodeVisualState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 NodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 VisualIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName CommandName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Label;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Start;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKOneGameRouteRoomType RoomType = EGameXXKOneGameRouteRoomType::Start;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bVisited = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D CanvasPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D HitBoxPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D HitBoxSize = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D ViewportHitBoxPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D ViewportHitBoxCenter = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString IconPath;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKOneGameRouteMapWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UGameXXKOneGameRouteMapWidget();

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMap")
	void RefreshFromState();

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	TArray<FGameXXKOneGameRouteNode> BuildAdapterNodes() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMap")
	bool ExecuteRouteNode(int32 NodeIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteMap")
	bool ExecuteRouteNodeById(int32 NodeId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	TArray<FGameXXKOneGameRouteNodeVisualState> GetRouteNodeVisualStatesForTest() const;

	void SetRouteMapViewportGeometry(FVector2D InViewportPosition, FVector2D InViewportSize);

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool IsOneGameRouteWidgetClassConfigured() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetOneGameRouteWidgetClassPath() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool AreOneGameVisualClassesConfigured() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool AreOneGameTextureVisualsConfigured() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetOneGameNodeWidgetClassPath() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetOneGameLineWidgetClassPath() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetOneGameBossWidgetClassPath() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	int32 GetCreatedNodeVisualWidgetCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	int32 GetCreatedLineVisualWidgetCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool ShouldUseOneGameBlueprintVisualWidgets() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetCreatedNodeVisualWidgetClassName(int32 WidgetIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetCreatedLineVisualWidgetClassName(int32 WidgetIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FText GetCreatedNodeVisualLabel(int32 WidgetIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetCreatedNodeVisualIconPath(int32 WidgetIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool HasRouteBackgroundVisualForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetRouteBackgroundTexturePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FVector2D GetRouteContentSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	float GetLastAppliedScrollOffsetForTest() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|RouteMap")
	void OnRouteNodeExecuted(const FGameXXKOneGameRouteNode& Node);

private:
	UFUNCTION()
	void HandleNodeButton0Clicked();

	UFUNCTION()
	void HandleNodeButton1Clicked();

	UFUNCTION()
	void HandleNodeButton2Clicked();

	UFUNCTION()
	void HandleNodeButton3Clicked();

	UFUNCTION()
	void HandleNodeButton4Clicked();

	UFUNCTION()
	void HandleNodeButton5Clicked();

	UFUNCTION()
	void HandleNodeButton6Clicked();

	UFUNCTION()
	void HandleNodeButton7Clicked();

	UFUNCTION()
	void HandleNodeButton8Clicked();

	UFUNCTION()
	void HandleNodeButton9Clicked();

	UFUNCTION()
	void HandleNodeButton10Clicked();

	UFUNCTION()
	void HandleNodeButton11Clicked();

	void BuildProgrammaticLayout();
	void ConfigureNodeButton(int32 ButtonIndex, const FGameXXKOneGameRouteNode* Node);
	void ConfigureLineVisual(int32 LineIndex, const TArray<FGameXXKOneGameRouteNode>& Nodes);
	void ExecuteNodeButtonAtIndex(int32 ButtonIndex);
	void BindNodeButton(UButton* Button, int32 ButtonIndex);
	UWidget* ConstructNodeVisualWidget(int32 NodeIndex);
	UWidget* ConstructLineVisualWidget(int32 LineIndex);
	UWidget* ConstructOneGameVisualWidget(TSoftClassPtr<UUserWidget>& WidgetClass, const FString& Name);
	UWidget* ConstructFallbackNodeVisualWidget(int32 NodeIndex);
	UWidget* ConstructFallbackLineVisualWidget(int32 LineIndex);
	TSoftObjectPtr<UTexture2D> GetTextureForNode(const FGameXXKOneGameRouteNode& Node) const;
	FVector2D GetNodeCanvasPosition(const FGameXXKOneGameRouteNode& Node) const;
	FVector2D CalculateRouteContentSize(const TArray<FGameXXKOneGameRouteNode>& Nodes) const;
	void ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes);
	static EGameXXKOneGameRouteRoomType MapRoomType(EGameXXKNodeKind NodeKind);

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap")
	TSoftClassPtr<UUserWidget> OneGameRouteWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap")
	TSoftClassPtr<UUserWidget> OneGameNodeWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap")
	TSoftClassPtr<UUserWidget> OneGameLineWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap")
	TSoftClassPtr<UUserWidget> OneGameBossWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap")
	bool bUseOneGameBlueprintVisualWidgets = false;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameStartTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameBattleTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameBattleDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameEliteTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameEliteDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameCampTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameCampDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameChestTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameChestDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameMerchantTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameMerchantDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameEventTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameEventDisabledTexture;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameRouteBackgroundTexture;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RouteBackgroundImage;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RouteScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RouteContentSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> NodeVisualWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> LineVisualWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> NodeVisualBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NodeVisualLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> NodeVisualImages;

	UPROPERTY(Transient)
	TArray<FString> NodeVisualIconPaths;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LineVisualBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> NodeButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NodeButtonLabels;

	UPROPERTY(Transient)
	TArray<int32> NodeButtonIndices;

	UPROPERTY(Transient)
	FVector2D RouteMapViewportPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D RouteMapViewportSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D RouteContentSize = FVector2D(640.0f, 1040.0f);

	UPROPERTY(Transient)
	float LastAppliedScrollOffset = 0.0f;
};
