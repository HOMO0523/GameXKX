#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
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
	FVector2D ScreenHitBoxPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D ScreenHitBoxCenter = FVector2D::ZeroVector;

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

	bool IsRouteScrollBarHiddenForTest() const;
	float GetMaxScrollOffsetForTest() const;
	bool HasRouteDragSurfaceForTest() const;
	bool ApplyRouteMapDragDeltaForTest(float PointerDeltaY);
	bool IsRouteNodeButtonBoundForTest(int32 ButtonIndex) const;

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

	UFUNCTION()
	void HandleNodeButton12Clicked();

	UFUNCTION()
	void HandleNodeButton13Clicked();

	UFUNCTION()
	void HandleNodeButton14Clicked();

	UFUNCTION()
	void HandleNodeButton15Clicked();

	UFUNCTION()
	void HandleNodeButton16Clicked();

	UFUNCTION()
	void HandleNodeButton17Clicked();

	UFUNCTION()
	void HandleNodeButton18Clicked();

	UFUNCTION()
	void HandleNodeButton19Clicked();

	UFUNCTION()
	void HandleNodeButton20Clicked();

	UFUNCTION()
	void HandleNodeButton21Clicked();

	UFUNCTION()
	void HandleNodeButton22Clicked();

	UFUNCTION()
	void HandleNodeButton23Clicked();

	UFUNCTION()
	FEventReply HandleRouteDragSurfaceMouseButtonDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleRouteDragSurfaceMouseButtonUp(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UFUNCTION()
	FEventReply HandleRouteDragSurfaceMouseMove(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	void BuildProgrammaticLayout();
	void ConfigureNodeButton(int32 ButtonIndex, const FGameXXKOneGameRouteNode* Node);
	void ConfigureLineVisual(int32 LineIndex, const TArray<FGameXXKOneGameRouteNode>& Nodes);
	void ExecuteNodeButtonAtIndex(int32 ButtonIndex);
	bool TryExecuteRouteNodeAtScreenPosition(const FVector2D& ScreenPosition);
	void BindNodeButton(UButton* Button, int32 ButtonIndex);
	UWidget* ConstructNodeVisualWidget(int32 NodeIndex);
	UWidget* ConstructLineVisualWidget(int32 LineIndex);
	UWidget* ConstructOneGameVisualWidget(TSoftClassPtr<UUserWidget>& WidgetClass, const FString& Name);
	UWidget* ConstructFallbackNodeVisualWidget(int32 NodeIndex);
	UWidget* ConstructFallbackLineVisualWidget(int32 LineIndex);
	TSoftObjectPtr<UTexture2D> GetTextureForNode(const FGameXXKOneGameRouteNode& Node) const;
	FVector2D GetNodeCanvasPosition(const FGameXXKOneGameRouteNode& Node) const;
	FVector2D CalculateRouteContentSize(const TArray<FGameXXKOneGameRouteNode>& Nodes) const;
	int32 GetRenderedRouteNodeCount(const TArray<FGameXXKOneGameRouteNode>& Nodes) const;
	int32 GetRenderedRouteLineCount(const TArray<FGameXXKOneGameRouteNode>& Nodes) const;
	bool TryGetRenderedRouteEdge(int32 LineIndex, const TArray<FGameXXKOneGameRouteNode>& Nodes, FGameXXKRouteMapEdge& OutEdge) const;
	void ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes);
	float CalculateMaxScrollOffset() const;
	void SetRouteScrollOffset(float NewScrollOffset);
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
	TSoftObjectPtr<UTexture2D> OneGameRouteLineTexture;

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
	TObjectPtr<UBorder> RouteDragSurface;

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
	FVector2D LastBuiltRouteContentSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float LastAppliedScrollOffset = 0.0f;

	UPROPERTY(Transient)
	bool bRouteMapDragActive = false;

	UPROPERTY(Transient)
	bool bRouteMapDragMoved = false;

	UPROPERTY(Transient)
	FVector2D RouteMapDragStartScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastRouteMapDragScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	int32 LastBuiltRouteNodeCount = INDEX_NONE;

	UPROPERTY(Transient)
	int32 LastBuiltRouteLineCount = INDEX_NONE;

	UPROPERTY(Transient)
	bool bLastBuiltUseOneGameBlueprintVisualWidgets = false;
};
