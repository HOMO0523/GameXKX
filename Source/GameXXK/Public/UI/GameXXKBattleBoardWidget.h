#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKCardOutcomePreview.h"
#include "GameXXKCardTypes.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKMVPRules.h"
#include "Math/Box2D.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "Input/Reply.h"
#include "GameXXKBattleBoardWidget.generated.h"

class UCanvasPanel;
class UScaleBox;
class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UGameXXKBattlePartyQiWidget;
class UGameXXKBattleStatusIconWidget;
class UGameXXKBattleUnitHudWidget;
class UGameXXKBattleUnitVisualWidget;
class UGameXXKBattleBoardWidget;
class UGameXXKCardOutcomePreviewWidget;

enum class EGameXXKBattleHudLayer : uint8
{
	Backdrop,
	Formation,
	TargetProxy,
	Controls
};

/** Pure Board layout result for the passive shared-Qi rail and its collision safety envelopes. */
struct FGameXXKBattlePartyQiLayout
{
	FMargin SlotOffsets;
	FBox2D QiRect = FBox2D(EForceInit::ForceInit);
	FBox2D ExpandedHandRect = FBox2D(EForceInit::ForceInit);
	FBox2D EndTurnRect = FBox2D(EForceInit::ForceInit);
	bool bUsesHandSafeFallback = false;
};

/** Legacy test-only layout result retained while older automation is migrated off foot projection. */
struct FGameXXKBattleProjectedUnitHudLayout
{
	FVector2D SlotPosition = FVector2D::ZeroVector;
	FBox2D Rect = FBox2D(EForceInit::ForceInit);
	bool bLiftedForObstacle = false;
};

/**
 * Deterministic geometry for the centered 16:9 battle HUD safe stage.  It
 * mirrors the ScaleToFit stage used at runtime so automation can validate
 * letterbox/pillarbox behaviour without relying on actor projection.
 */
struct FGameXXKBattleHudSafeStageLayout
{
	FVector2D Offset = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	float Scale = 1.0f;
};

UENUM(BlueprintType)
enum class EGameXXKBattleInteractionMode : uint8
{
	Hidden,
	Idle,
	CommandMenuOpen,
	TargetingBasicAttack,
	TargetingCraneWingSlash,
	/** A card passed CardCheck and now awaits one manually selected stable unit ID. */
	TargetingCard
};

/** Timed, display-only progression for one already-saved enemy intent. */
enum class EGameXXKEnemyIntentPresentationState : uint8
{
	None,
	Reveal,
	Resolve,
	Settle
};

class UGameXXKBattleBoardWidget;

/** A dynamic UMG button needs an object-bound handler; it carries one stable route-card EntryId. */
UCLASS()
class GAMEXXK_API UGameXXKRouteRewardReplacementButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKBattleBoardWidget* InOwner, FName InEntryId);

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> Owner;

	UPROPERTY(Transient)
	FName EntryId;
};

/** A dynamic UMG button needs an object-bound handler; it carries a stable pending-choice card instance ID. */
UCLASS()
class GAMEXXK_API UGameXXKPendingChoiceCardButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UGameXXKBattleBoardWidget* InOwner,
		FName InCandidateInstanceId,
		EGameXXKCardPendingChoiceKind InChoiceKind);

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> Owner;

	UPROPERTY(Transient)
	FName CandidateInstanceId;

	UPROPERTY(Transient)
	EGameXXKCardPendingChoiceKind ChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
};

/** Stable, transparent click proxy for one authoritative battle UnitId. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitTargetProxyButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKBattleBoardWidget* InOwner, FName InUnitId);

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> Owner;

	UPROPERTY(Transient)
	FName UnitId;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKBattleBoardWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	/** Transient diagnostics: how many battle-board widgets currently exist. */
	static int32 GAliveBattleBoardInstances;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void RefreshFromState();

	/** Queues one immutable, already-resolved combat packet for marker-driven Board presentation. */
	void QueuePresentation(const FGameXXKBattlePresentationEvent& Event);
	bool BeginBattleVisualSession(uint64 SessionToken);
	void CancelBattleVisualSession(uint64 ClosingSessionToken);
	void AdvanceVisualsAtRealTime(double AbsoluteSeconds);
	void HandleUnitTargetProxyClicked(FName UnitId);
	void HandleUnitTargetProxyHoverChanged(FName UnitId, bool bHovered);
	int32 GetLayerZ(EGameXXKBattleHudLayer Layer) const;

	UCanvasPanel* GetBattleViewportRootForTest() const;
	UCanvasPanel* GetBattleDesignStageForTest() const;
	UCanvasPanel* GetBattleControlsLayerForTest() const;
	UScaleBox* GetBattleBackdropScaleBoxForTest() const;
	UImage* GetBattleBackdropImageForTest() const;
	FString GetBattleBackdropResourcePathForTest() const;
	/** Resolves the battle backdrop texture asset path for a terrain; Invalid falls back to the riverside asset. */
	static FString ResolveBattleBackdropTexturePath(EGameXXKCardTerrain Terrain);
	/** Loads and applies the backdrop texture for the given terrain; called at construction and on terrain switches. */
	void ApplyBattleBackdropForTerrain(EGameXXKCardTerrain Terrain);
	UGameXXKBattleUnitVisualWidget* GetUnitVisualForTest(FName UnitId) const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UButton* GetUnitTargetProxyForTest(FName UnitId) const;
	int32 GetUnitVisualCountForTest() const;
	bool IsUnitTargetPlaceholderVisibleForTest(FName UnitId) const;
	uint64 GetActiveBattleVisualSessionTokenForTest() const;
	int32 GetPinnedBattleAtlasCountForTest() const;
	int32 GetDuplicateParticipantImageCountForTest() const;
	FGameXXKBattleAtlasCacheStats GetAtlasCacheStatsForTest() const;
	bool IsBattlePresentationActiveForTest() const;
	bool IsBattlePresentationLockedForTest() const;
	bool IsBattleDeathPresentationActiveForTest() const;
	bool IsBattleStatusPresentationActiveForTest() const;
	int32 GetBattlePresentationQueueCountForTest() const;
	uint64 GetActiveBattlePresentationEventIdForTest() const;
	FName GetActiveBattlePresentationAttackerUnitIdForTest() const;
	FName GetActiveBattlePresentationTargetUnitIdForTest() const;
	double GetActiveBattlePresentationElapsedForTest() const;
	double GetActiveBattlePresentationDurationForTest() const;
	int32 GetBattlePresentationImpactCountForTest() const;
	int32 GetBattlePresentationCompletionCountForTest() const;
	int32 GetBattlePresentationHudShakeCountForTest() const;
	FVector2D GetBattlePresentationShakeAmplitudeForTest() const;
	double GetBattlePresentationShakeDurationForTest() const;
	int32 GetExecutedBattlePresentationContinuationCountForTest() const;
	bool IsPlayedCardCommitActiveForTest() const;
	FName GetPlayedCardCommitInstanceIdForTest() const;
	double GetPlayedCardCommitElapsedForTest() const;
	FVector2D GetPlayedCardCommitTranslationForTest() const;
	FVector2D GetPlayedCardCommitScaleForTest() const;
	float GetPlayedCardCommitOpacityForTest() const;
	int32 GetPlayedCardCommitCompletionCountForTest() const;
	FString GetActiveBattleStatusAnimationAssetIdForTest() const;
	int32 GetActiveBattleStatusDeltaForTest() const;
	FName GetActiveBattleStatusIconIdForTest() const;
	int32 GetDisplayedHealthForTest(FName UnitId) const;
	float GetActiveAttackerPlaybackRateForTest() const;
	float GetActiveTargetPlaybackRateForTest() const;
	float GetActiveImpactPlaybackRateForTest() const;
	FString GetBattlePresentationReadoutForTest() const;
	FVector2D GetBattlePresentationReadoutScaleForTest() const;
	float GetBattlePresentationReadoutOpacityForTest() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetAtlasCacheForTest(TUniquePtr<FGameXXKBattleAtlasCache> InAtlasCache);
#endif

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecutePrimaryEnemyAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteBasicAttackAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteCraneWingSlashAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteGuiyuanArtAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteDefendAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteHealingPowderAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ToggleCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void UpdateTargetingPointer(FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void UpdateTargetingPointerFromSlateAbsolutePosition(FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ConfirmTargetingEnemy(int32 EnemyIndex);

	/** Runs CardCheck for a stable hand-card instance. Manual cards enter arrow targeting; automatic cards commit immediately. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool ClickCardInHand(FName CardInstanceId);

	/** Revalidates and commits the currently previewed card using a stable card-runtime unit ID. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool ConfirmTargetingUnit(FName UnitId);

	/** Ends the player phase, saves enemy intents, and starts their board presentation. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool EndCardPlayerPhase();

	/** Retries a terminal enemy-phase completion after a recoverable board error. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool RetryEnemyIntentCompletion();

	/** Resolves the currently visible insight offer by stable runtime-card ID. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool SubmitPendingInsightChoice(FName PickedInstanceId);

	/** Resolves the visible protagonist spell-task search through the shared pending-choice panel. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool SubmitPendingHeroTaskSearchChoice(FName PickedInstanceId);

	/** Resolves the currently visible one-card forced-discard choice by stable runtime-card ID. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool SubmitPendingForcedDiscard(FName DiscardedInstanceId);

	/** Cancels a visible cancellable insight while preserving the inspected draw-pile top. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	bool CancelPendingInsightChoice();

	/** The scene/controller bridge registers projected unit locations so a card arrow starts at the card owner's actor. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	void RegisterBattleUnitScreenPosition(FName UnitId, FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	void ClearBattleUnitScreenPositions();

	/**
	 * Deprecated compatibility entry point. Unit resource HUDs use fixed P-slots;
	 * actor projections remain owned by RegisterBattleUnitScreenPosition for arrows.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Cards")
	void RegisterBattleUnitHudScreenPosition(FName UnitId, FVector2D ScreenPosition);

	void RefreshProjectedUnitHuds();
	void RefreshProjectedUnitHudPositions();

	/** True only for legal CandidateViews returned by the current non-mutating CardCheck preview. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Cards")
	bool IsTargetUnitHighlighted(FName UnitId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Cards")
	bool IsCardTargetingActive() const;

	/** Commits an existing saved route reward through the adapter, then lets the existing Rules victory gate advance the route. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Rewards")
	bool ChoosePendingRouteReward(FName RewardCardId, FName ReplacementEntryId);

	/** Commits one tiered battle reward option (card upgrade, boss card, relic, or attribute bonus). */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Rewards")
	bool ChoosePendingBattleRewardOption(int32 OptionIndex, FName ReplacementEntryId = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Rewards")
	bool SkipPendingRouteReward();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Rewards")
	bool HasPendingRouteReward() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Rewards")
	TArray<FName> GetPendingRouteRewardCardIds() const;

	/** Selects one exact eligible temporary route-card EntryId for the currently awaiting reward candidate. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Rewards")
	bool SelectRouteRewardReplacementEntry(FName EntryId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Rewards")
	TArray<FName> GetRouteRewardReplacementEntryIds() const;

	/** Clears only the Board-owned replacement chooser; saved runtime state is never touched. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle|Rewards")
	bool CancelRouteRewardReplacement();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool CancelBattleTargeting();

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

	/** Read-only development seams used by the real-PIE visual probe. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UGameXXKBattlePartyQiWidget* GetPartyQiWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UHorizontalBox* GetHandCardBoxForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UButton* GetEndTurnButtonForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UCanvasPanel* GetBattleProjectedUnitHudLayerForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UGameXXKBattleUnitHudWidget* GetProjectedUnitHudForTest(FName UnitId) const;

	int32 GetProjectedUnitHudCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetProjectedUnitHudAnchorPositionForTest(FName UnitId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetBattleUnitScreenPositionForTest(FName UnitId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool HasBattleUnitScreenPositionForTest(FName UnitId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool HasProjectedUnitHudScreenPositionForTest(FName UnitId) const;

	/** Read-only mirror of the battle HUD's ScaleToFit safe-stage geometry. */
	FGameXXKBattleHudSafeStageLayout ResolveBattleHudSafeStageLayoutForTest(FVector2D ViewportSize) const;

	/** Deprecated test compatibility only; production resource HUDs never use foot projection. */
	FGameXXKBattleProjectedUnitHudLayout ResolveProjectedUnitHudLayoutForTest(
		FVector2D Anchor, FVector2D WidgetSize, FVector2D CanvasSize,
		const FBox2D& HandRect, const FBox2D& QiRect,
		const FBox2D& EndTurnRect, const FBox2D& ShowcaseRect) const;

	FVector2D GetEnemySlotPositionForTest(int32 SlotIndex) const;
	FVector2D GetPartySlotPositionForTest(int32 SlotIndex) const;
	bool HasBattleActionForTest(FName ActionName, bool bRequireEnabled) const;
	bool IsCommandMenuVisibleForTest() const;
	bool IsTargetingBattleActionForTest() const;
	bool IsCardTargetingForTest() const;
	bool KeepTargetingAfterEmptyClickForTest() const;
	int32 GetSelectedPartyIndexForTest() const;
	FName GetTargetingActionNameForTest() const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetTargetingPointerPositionForTest() const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetTargetingSourcePositionForTest() const;
	/** Direct probe for malformed/stale card-owner fallback placement. */
	FVector2D ResolveCardTargetingSourcePositionForTest(FName OwnerUnitId) const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FName GetPendingCardInstanceIdForTest() const;
	int32 GetVisibleHandCardCountForTest() const;
	FVector2D GetCommandMenuAnchorForTest() const;
	FVector2D ResolveCommandSourcePositionForTest(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition, FVector2D LocalSize) const;
	FVector2D ResolveSlateAbsolutePositionToLocalForTest(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D LocalSize) const;
	FVector2D ResolveSlateAbsolutePositionToLocalForTest(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D WidgetAbsoluteSize, FVector2D LocalSize) const;
	FString GetBattleActionButtonResourcePathForTest(FName ActionName);
	FLinearColor GetBattleActionButtonTintForTest(FName ActionName) const;
	FString GetTargetingArrowHeadResourcePathForTest();
	int32 GetTargetingInkDabTextureCountForTest();
	FString GetCardFrameResourcePathForTest();
	FVector2D GetCardFrameRuntimeSizeForTest() const;
	/** The approved PSD frame is immutable parchment/ink; ownership color belongs only to a lower strip. */
	FLinearColor GetCardFrameTintForTest() const;
	/** The single ink name-band color shared by every BuildCardFace card face. */
	FLinearColor GetCardFaceLabelColorForTest() const;
	FString GetCardPortraitResourcePathForTest(FName CardId) const;
	FLinearColor GetCardInfoStripTintForTest(FName CardId) const;
	/** True while the given reward slot keeps a non-collapsed portrait (relic icon or card art). */
	bool IsRewardPortraitVisibleForTest(int32 SlotIndex) const;
	FName GetSelectedRouteRewardReplacementEntryIdForTest() const;
	FName GetRouteRewardCardIdAwaitingReplacementForTest() const;
	int32 GetVisibleEnemyIntentCardCountForTest() const;
	int32 GetActiveEnemyIntentPresentationIndexForTest() const;
	FString GetEnemyIntentSlotLabelForTest(int32 VisibleSlotIndex) const;
	FString GetEnemyIntentTooltipForTest(int32 VisibleSlotIndex) const;
	FString GetEnemyIntentPortraitResourcePathForTest(int32 VisibleSlotIndex) const;
	bool IsHandCardSlotEnabledForTest(int32 SlotIndex) const;
	FString GetCardTooltipTextForTest() const;
	bool IsCardTooltipVisibleForTest() const;
	bool IsCardTooltipHitTestInvisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UButton* GetHandCardButtonForTest(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool IsCardOutcomePreviewVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetCardOutcomePreviewClassForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FName GetCardOutcomePreviewCardInstanceIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FName GetCardOutcomePreviewTargetUnitIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	TArray<FString> GetCardOutcomePreviewLinesForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	int32 GetCardOutcomePreviewBuildCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetSingleOutcomePreviewAnchorForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetGroupOutcomePreviewAnchorForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FMargin GetSingleOutcomePreviewOffsetsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FVector2D GetSingleOutcomePreviewAlignmentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetCardOutcomePreviewBackgroundResourceForTest() const;

#if WITH_DEV_AUTOMATION_TESTS
	UCanvasPanel* GetBattleOutcomePreviewLayerForTest() const;
	int32 GetBattleOutcomePreviewLayerZForTest() const;
	FMargin GetGroupOutcomePreviewOffsetsForTest() const;
	/** Reward row container slot geometry (legacy small-card strip replaced by full card faces). */
	FMargin GetRewardCardBoxOffsetsForTest() const;
	/** Current tooltip panel offsets (follows the hovered card slot). */
	FMargin GetHandCardDetailPanelOffsetsForTest() const;
	FVector2D GetGroupOutcomePreviewAlignmentForTest() const;
	/** Drives the production visual-removal path without mutating authoritative runtime state. */
	void RemoveUnitVisualForTest(FName UnitId);
	/** Drives the production Victory/Defeat terminal handler against the supplied authoritative fixture state. */
	bool ResolveCardBattleTerminalStateForTest();
	/** Pure layout seam: callers supply a canvas size to validate the right rail against expanded hand and end-turn bounds. */
	FGameXXKBattlePartyQiLayout ResolvePartyQiLayoutForTest(FVector2D CanvasSize) const;
	/** Runs the same responsive Party Qi refresh used when NativeTick observes settled or resized canvas geometry. */
	void RefreshPartyQiForCanvasSizeForTest(FVector2D CanvasSize);
#endif

	/** Dynamic card subclasses forward pure hover transitions here; these never mutate card runtime state. */
	void HandlePendingChoiceCardHoverChanged(FName CandidateInstanceId, EGameXXKCardPendingChoiceKind ChoiceKind, bool bHovered);
	void HandleRouteRewardReplacementEntryHoverChanged(FName EntryId, bool bHovered);

#if WITH_DEV_AUTOMATION_TESTS
	/** Test seam for deterministic hover-motion assertions; production animation advances through NativeTick. */
	void AdvanceHandCardHoverMotionForTest(float InDeltaTime);
	/** Test seam for the timed saved-enemy-intent presentation; production animation advances through NativeTick. */
	void AdvanceEnemyIntentPresentationForTest(float InDeltaTime);
#endif

private:
	enum class EBattlePresentationKind : uint8
	{
		AttackHit,
		Death,
		Status
	};

	enum class EBattlePresentationAtlasRole : uint8
	{
		Attacker,
		Target,
		Impact,
		Status
	};

	/** Typed Board-owned resumptions can be discarded safely when a visual session is cancelled. */
	enum class EBattlePresentationContinuation : uint8
	{
		None,
		FinalizeCardMutation,
		BeginEnemyIntentAfterPlayerPhase,
		ResumeEnemyIntentAfterMutation,
		FinalizeEnemyPhase
	};

	/** One immutable event plus its absolute-time, exact-once presentation state. */
	struct FBattlePresentationQueueEntry
	{
		FGameXXKBattlePresentationEvent Event;
		FGameXXKBattleStatusPresentationEvent StatusEvent;
		EBattlePresentationKind Kind = EBattlePresentationKind::AttackHit;
		uint64 QueueSerial = 0;
		double StartSeconds = 0.0;
		bool bStarted = false;
		bool bImpactFired = false;
		bool bCompletionFired = false;
		FGameXXKBattlePresentationRhythm Rhythm;
		FGameXXKBattleAnimationClipDescriptor AttackerClip;
		FGameXXKBattleAnimationClipDescriptor TargetClip;
		FGameXXKBattleAnimationClipDescriptor ImpactClip;
		FGameXXKBattleAnimationClipDescriptor StatusClip;
		FGameXXKBattleAnimationClipDescriptor PresentedAttackerClip;
		FGameXXKBattleAnimationClipDescriptor PresentedTargetClip;
		TWeakObjectPtr<UTexture2D> AttackerAtlas;
		TWeakObjectPtr<UTexture2D> TargetAtlas;
		TWeakObjectPtr<UTexture2D> ImpactAtlas;
		TWeakObjectPtr<UTexture2D> StatusAtlas;
		TArray<FSoftObjectPath> PinnedAtlasPaths;
	};

	void BuildProgrammaticLayout();
	void QueuePresentationInternal(
		const FGameXXKBattlePresentationEvent& Event,
		bool bRefreshBaseline);
	void QueueStatusPresentation(const FGameXXKBattleStatusPresentationEvent& Event);
	bool QueueMutationPresentation(
		const FGameXXKCardBattleRuntime& Before,
		const TArray<FGameXXKCardDamageResult>& DamageResults,
		EBattlePresentationContinuation Continuation,
		FName PlayedCardInstanceId = NAME_None);
	bool BeginPlayedCardCommit(FName PlayedCardInstanceId);
	TOptional<double> AdvancePlayedCardCommit(double AbsoluteSeconds);
	void CompletePlayedCardCommit();
	void ResetPlayedCardCommit(bool bRestoreInitialVisual);
	void CapturePresentationHudSnapshot(const FGameXXKCardBattleRuntime& Runtime);
	void DiscardPresentationHudSnapshot();
	void ApplyDisplayedStatusDelta(const FGameXXKBattleStatusPresentationEvent& Event);
	bool IsBattlePresentationPending() const;
	bool RejectBattlePresentationMutation();
	void ApplyBattlePresentationInteractionLock();
	bool ExecuteBattlePresentationContinuation(EBattlePresentationContinuation Continuation);
	void HandleBattlePresentationQueueDrained();
	void PrefetchPresentationEntry(uint64 QueueSerial);
	void PrefetchPresentationAtlas(
		uint64 QueueSerial,
		const FSoftObjectPath& TexturePath,
		EBattlePresentationAtlasRole Role);
	FBattlePresentationQueueEntry* FindPresentationEntry(uint64 QueueSerial);
	const FBattlePresentationQueueEntry* GetActivePresentationEntry() const;
	void AdvanceBattlePresentation(double AbsoluteSeconds);
	void StartPresentationEntry(FBattlePresentationQueueEntry& Entry, double StartSeconds);
	void FirePresentationImpact(FBattlePresentationQueueEntry& Entry);
	void CompletePresentationEntry(FBattlePresentationQueueEntry& Entry);
	void EnqueueDeathPresentationAfterActive(const FGameXXKBattlePresentationEvent& Event);
	void ReleasePresentationPins(FBattlePresentationQueueEntry& Entry);
	void ResetBattlePresentation();
	void HideFormationForPresentation();
	void RestoreFormationAfterPresentation(FName RemovedUnitId = NAME_None);
	void RestoreUnitIdleAtlas(FName UnitId, UGameXXKBattleUnitVisualWidget* Visual);
	FGameXXKBattleAnimationClipDescriptor ResolveUnitAnimationClip(
		FName UnitId,
		bool bEnemy,
		EGameXXKBattleAnimationAction Action) const;
	void SetTargetProxiesVisible(bool bVisible);
	void SetDisplayedHealthOverlay(FName UnitId, int32 Health, int32 Armor = INDEX_NONE);
	void ApplyDisplayedDamagePacket(const FGameXXKBattlePresentationEvent& Event);
	void ClearDisplayedHealthOverlay(FName UnitId);
	bool IsUnitRetainedByPresentation(FName UnitId) const;
	void UpdateBattlePresentationShake(double AbsoluteSeconds);
	void UpdateBattlePresentationReadout(double AbsoluteSeconds);
	void ResetBattlePresentationFeedback();
	void RefreshUnitVisuals();
	void RefreshUnitTargetingPresentation();
	void RefreshCinematicViewportCoverLayout(FVector2D ViewportSize);
	void ReleasePinnedAtlasForUnit(FName UnitId);
	void RemoveUnitVisual(FName UnitId);
	void SetUnitTargetPlaceholderVisible(FName UnitId, bool bVisible);
	UButton* AddBattleActionButton(const FText& Label, FName ButtonName, FName ActionName);
	void RefreshProgrammaticLayout();
	void RefreshActionButtons();
	void RefreshHandCards();
	void RefreshPartyQiWidget();
	void RefreshPartyQiWidgetForCanvasSize(FVector2D CanvasSize);
	FText ResolveProjectedUnitHudDisplayName(FName UnitId) const;
	FBox2D ResolveExpandedHandRect(FVector2D CanvasSize) const;
	FGameXXKBattlePartyQiLayout ResolvePartyQiLayout(FVector2D CanvasSize) const;
	/** Shows a readable, non-intercepting paper tooltip only for the card currently under the mouse. */
	void RefreshCardTooltip();
	void RefreshEnemyIntentCards();
	void RefreshEnemyIntentDetail();
	void RefreshEnemyIntentShowcase();
	void RefreshEnemyIntentRecoveryControl();
	void RefreshPendingCardChoices();
	void RefreshPendingRewardChoices();
	void RefreshRouteRewardReplacementChoices();
	void ClearCardTooltipHoverState();
	void SetHandCardHoverState(int32 SlotIndex, bool bHovered);
	void SetRewardCardHoverState(int32 SlotIndex, bool bHovered);
	void AdvanceHandCardHoverMotion(float InDeltaTime);
	void SetEnemyIntentHoverState(int32 VisibleSlotIndex, bool bHovered);
	void AdvanceEnemyIntentPresentation(float InDeltaTime);
	void BeginEnemyIntentPresentation();
	bool ResolveCurrentEnemyIntentPresentation();
	bool CompleteEnemyIntentPresentation();
	/** Clears transient enemy-presentation state without touching card-runtime state. */
	void ResetEnemyIntentPresentationState();
	/** Returns true after safely rejecting an interaction that would mutate the raw battle behind a fixture overlay. */
	bool RejectBattleHudFixtureMutation();
	bool IsBattleHudFixtureReadOnly() const;
	bool IsEnemyIntentPresentationActive() const;
	int32 GetEnemyIntentPersistentIndexForVisibleSlot(int32 VisibleSlotIndex) const;
	void EnsureBattleVisualResourcesLoaded();
	void StyleBattleActionButton(UButton* Button, FName ActionName);
	void StyleCardButton(UButton* Button, const FVector2D& CardImageSize);
	void BuildCardFace(UButton* CardButton, const FString& NamePrefix, UTextBlock*& OutLabel, UImage*& OutPortrait, UBorder*& OutInfoStrip, bool bUsePlayerHandSize = false);
	/** Approved ink label color for every BuildCardFace name band. */
	static FLinearColor ResolveCardFaceLabelColor();
	void BuildEnemyIntentCardFace(UButton* CardButton, const FString& NamePrefix, UTextBlock*& OutBody, UImage*& OutPortrait);
	FString ResolveEnemyIntentPortraitResourcePath(FName EnemyDefinitionId) const;
	UTexture2D* ResolveEnemyIntentPortraitTexture(FName EnemyDefinitionId) const;
	void ApplyCardPresentation(UButton* CardButton, UTextBlock* CardLabel, UImage* PortraitImage, UBorder* InfoStrip, const FGameXXKCardDefinition* Definition);
	UTexture2D* ResolveCardPortraitTexture(const FGameXXKCardDefinition& Definition);
	FString ResolveCardPortraitResourcePath(const FGameXXKCardDefinition& Definition) const;
	FLinearColor ResolveBattleActionButtonTint(FName ActionName) const;
	FVector2D ResolveCommandSourcePosition(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition, FVector2D LocalSize) const;
	FVector2D ResolveCommandMenuAnchor(FVector2D UnitScreenPosition) const;
	FVector2D ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition) const;
	FVector2D ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D LocalSize) const;
	FVector2D ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D WidgetAbsoluteSize, FVector2D LocalSize) const;
	bool BeginTargetingBattleAction(FName ActionName);
	bool BeginCardTargeting(const FGameXXKCardPlayPreview& Preview);
	bool ResolveAutomaticCardPlay(FName CardInstanceId);
	bool RefreshPendingCardTargetingPreview();
	void ClearCardTargetingState();
	void ClearCardOutcomePreview();
	bool BuildCardOutcomePreview(FName CardInstanceId, FName RequestedTargetUnitId);
	void ApplyCardOutcomePreview(const FGameXXKCardOutcomePreview& Preview);
	void RefreshSingleOutcomePreviewPlacement(FName UnitId);
	bool TryResolveUnitTargetStageCenter(FName UnitId, FVector2D& OutStageCenter) const;
	bool ResolveCardBattleTerminalState();
	FVector2D ResolveCardTargetingSourcePosition(FName OwnerUnitId) const;
	bool ResolveAndRefreshCardBattleAfterMutation();
	bool ExecuteBattleAction(FName ActionName);
	int32 FindFirstLivingEnemyIndex() const;

	UFUNCTION()
	void HandleBasicAttackClicked();

	UFUNCTION()
	void HandleCraneWingSlashClicked();

	UFUNCTION()
	void HandleGuiyuanArtClicked();

	UFUNCTION()
	void HandleDefendClicked();

	UFUNCTION()
	void HandleHealingPowderClicked();

	UFUNCTION()
	void HandleHandCardSlot0Clicked();

	UFUNCTION()
	void HandleHandCardSlot1Clicked();

	UFUNCTION()
	void HandleHandCardSlot2Clicked();

	UFUNCTION()
	void HandleHandCardSlot3Clicked();

	UFUNCTION()
	void HandleHandCardSlot4Clicked();

	UFUNCTION()
	void HandleHandCardSlot0Hovered();

	UFUNCTION()
	void HandleHandCardSlot1Hovered();

	UFUNCTION()
	void HandleHandCardSlot2Hovered();

	UFUNCTION()
	void HandleHandCardSlot3Hovered();

	UFUNCTION()
	void HandleHandCardSlot4Hovered();

	UFUNCTION()
	void HandleHandCardSlot0Unhovered();

	UFUNCTION()
	void HandleHandCardSlot1Unhovered();

	UFUNCTION()
	void HandleHandCardSlot2Unhovered();

	UFUNCTION()
	void HandleHandCardSlot3Unhovered();

	UFUNCTION()
	void HandleHandCardSlot4Unhovered();

	UFUNCTION()
	void HandleEnemyIntentSlot0Hovered();

	UFUNCTION()
	void HandleEnemyIntentSlot1Hovered();

	UFUNCTION()
	void HandleEnemyIntentSlot2Hovered();

	UFUNCTION()
	void HandleEnemyIntentSlot0Unhovered();

	UFUNCTION()
	void HandleEnemyIntentSlot1Unhovered();

	UFUNCTION()
	void HandleEnemyIntentSlot2Unhovered();

	UFUNCTION()
	void HandleEnemyIntentRecoveryClicked();

	UFUNCTION()
	void HandlePendingInsightCancelClicked();

	UFUNCTION()
	void HandleEndTurnClicked();

	UFUNCTION()
	void HandleRewardCardSlot0Clicked();

	UFUNCTION()
	void HandleRewardCardSlot1Clicked();

	UFUNCTION()
	void HandleRewardCardSlot2Clicked();

	UFUNCTION()
	void HandleRewardCardSlot0Hovered();

	UFUNCTION()
	void HandleRewardCardSlot1Hovered();

	UFUNCTION()
	void HandleRewardCardSlot2Hovered();

	UFUNCTION()
	void HandleRewardCardSlot0Unhovered();

	UFUNCTION()
	void HandleRewardCardSlot1Unhovered();

	UFUNCTION()
	void HandleRewardCardSlot2Unhovered();

	UFUNCTION()
	void HandleSkipRewardClicked();

	void HandleHandCardSlotClicked(int32 SlotIndex);
	void HandleRewardCardSlotClicked(int32 SlotIndex);
	void HandleRouteRewardReplacementClicked(FName EntryId);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ViewportRootCanvas;

	/** Canonical 1920x1080 canvas under the centered ScaleToFit viewport stage. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BattleDesignStage;

	/** Existing card/status controls now share one z=20 container in the design stage. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> BattleBackdropScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BattleBackdropImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BattleBackdropTexture;

	/** Asset path of the backdrop chosen for the battle terrain active at construction. */
	FString BattleBackdropResourcePath;

	/** Terrain the displayed backdrop currently represents; Invalid until applied. */
	EGameXXKCardTerrain BattleBackdropTerrain = EGameXXKCardTerrain::Invalid;

	/** One ordinary input-transparent Canvas inside the centered 16:9 battle safe stage. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BattleProjectedUnitHudLayer;

	/** Input-transparent sibling above the fixed unit HUD projection; existing layout stays untouched. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BattleOutcomePreviewLayer;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCardOutcomePreviewWidget> SingleOutcomeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCardOutcomePreviewWidget> GroupOutcomeWidget;

	/** Battle-only full-screen action layer; it never replaces card or story portraits. */
	/** Marker-driven presentation elements live directly in the common design stage. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BattleCinematicDimmer;

	/** Four margin-only ink strips cover the viewport outside the centered 16:9 safe stage. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BattleCinematicViewportCover;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> BattleCinematicViewportCoverStrips;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleUnitVisualWidget> BattleCinematicImpact;

	/** Existing status badge, enlarged only for the active central status presentation. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleStatusIconWidget> BattleCinematicStatusIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BattleCinematicReadout;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKBattleUnitHudWidget>> ProjectedUnitHuds;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>> UnitVisuals;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKBattleUnitTargetProxyButton>> UnitTargetProxies;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextBlock>> UnitTargetPlaceholders;

	TMap<FName, FSoftObjectPath> PinnedUnitAtlasPaths;
	/** One idle request per unit and visual session, including terminal fallback results. */
	TMap<FName, FSoftObjectPath> RequestedUnitAtlasPaths;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> UnitIdleAtlasTextures;
	TUniquePtr<FGameXXKBattleAtlasCache> AtlasCache;
	uint64 ActiveBattleVisualSessionToken = 0;
	double LastSlateSeconds = 0.0;
	TArray<FBattlePresentationQueueEntry> BattlePresentationQueue;
	TMap<FName, int32> DisplayedHealthOverrides;
	TMap<FName, FGameXXKBattleUnitHudView> DisplayedUnitHudOverrides;
	/** Pre-mutation party Qi retained until the complete immutable presentation batch drains or is cancelled. */
	TOptional<int32> DisplayedSharedEnergyOverride;
	TSet<FName> DefeatedUnitVisualsPendingRemoval;
	EBattlePresentationContinuation DeferredBattlePresentationContinuation = EBattlePresentationContinuation::None;
	uint64 NextBattlePresentationQueueSerial = 1;
	int32 BattlePresentationImpactCount = 0;
	int32 BattlePresentationCompletionCount = 0;
	int32 BattlePresentationHudShakeCount = 0;
	int32 ExecutedBattlePresentationContinuationCount = 0;
	bool bPlayedCardCommitActive = false;
	bool bPlayedCardCommitStarted = false;
	FName PlayedCardCommitInstanceId = NAME_None;
	TWeakObjectPtr<UButton> PlayedCardCommitButton;
	FWidgetTransform PlayedCardCommitInitialTransform;
	float PlayedCardCommitInitialOpacity = 1.0f;
	double PlayedCardCommitStartSeconds = 0.0;
	double PlayedCardCommitElapsedSeconds = 0.0;
	int32 PlayedCardCommitCompletionCount = 0;
	double BattlePresentationShakeStartSeconds = 0.0;
	double BattlePresentationShakeDurationSeconds = 0.0;
	FVector2D BattlePresentationShakeAmplitude = FVector2D::ZeroVector;
	bool bBattlePresentationShakeActive = false;
	double BattlePresentationReadoutStartSeconds = 0.0;
	double BattlePresentationReadoutDurationSeconds = 0.0;
	float BattlePresentationReadoutPeakScale = 1.0f;
	bool bBattlePresentationReadoutActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ActionBox;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HandCardBox;

	/** Passive Board-owned projection of CardRun.ActiveBattle.Deck.SharedEnergy. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattlePartyQiWidget> PartyQiWidget;

	/** Top rail contains only saved enemy intents, never player played cards. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> EnemyIntentCardBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnemyIntentShowcaseCard;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnemyIntentRecoveryButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnemyIntentShowcaseBody;

	/** Paper tooltip for a hovered enemy intent; it remains input-transparent. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> EnemyIntentDetailPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnemyIntentDetailBody;

	/** Shared PSD paper tooltip for every playable Battle Board card. It never intercepts card input. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> HandCardDetailPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HandCardDetailTitle;
	TObjectPtr<UTextBlock> HandCardDetailBody;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RewardCardBox;

	FMargin RewardCardBoxSlotOffsets = FMargin(0.0f);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PendingChoicePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PendingChoicePromptText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PendingChoiceCardBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PendingChoiceCancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RouteRewardReplacementScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BasicAttackButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CraneWingSlashButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GuiyuanArtButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DefendButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HealingPowderButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EndTurnButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SkipRewardButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> HandCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HandCardLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HandCardPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> HandCardInfoStrips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> EnemyIntentCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EnemyIntentSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> EnemyIntentCardBodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EnemyIntentCardPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RewardCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RewardCardLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RewardCardPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RewardCardInfoStrips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PendingChoiceCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PendingChoiceCardLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PendingChoiceCardPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> PendingChoiceCardInfoStrips;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RouteRewardReplacementButtons;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BattleActionInkButtonTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BattleStatusWindowFrameTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CardFrameTexture;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> CardPortraitTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TargetingArrowHeadTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TargetingInkDabTextures;

	UPROPERTY(Transient)
	EGameXXKBattleInteractionMode InteractionMode = EGameXXKBattleInteractionMode::Hidden;

	UPROPERTY(Transient)
	int32 SelectedPartyIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FName TargetingActionName;

	UPROPERTY(Transient)
	FGameXXKCardPlayPreview PendingCardPreview;

	UPROPERTY(Transient)
	TSet<FName> LegalCardTargetUnitIds;

	FName CachedOutcomeCardInstanceId = NAME_None;
	FName CachedOutcomeTargetUnitId = NAME_None;
	TOptional<FGameXXKRuntimeState> CachedOutcomeSourceState;
	FGameXXKCardOutcomePreview CachedOutcomePreview;
	int32 OutcomePreviewBuildCountForTest = 0;

	UPROPERTY(Transient)
	TArray<FName> HandCardInstanceIds;

	UPROPERTY(Transient)
	int32 HoveredHandCardSlot = INDEX_NONE;

	/** Reward hover tracks the option slot index so relic and attribute options tooltip too. */
	int32 HoveredRewardCardSlot = INDEX_NONE;

	enum class ECardTooltipSource : uint8
	{
		None,
		Hand,
		PendingChoice,
		Reward,
		RouteReplacement
	};

	ECardTooltipSource HoveredCardTooltipSource = ECardTooltipSource::None;

	FName HoveredCardTooltipId = NAME_None;

	EGameXXKCardPendingChoiceKind HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;

	UPROPERTY(Transient)
	TArray<int32> VisibleEnemyIntentIndices;

	UPROPERTY(Transient)
	int32 HoveredEnemyIntentSlot = INDEX_NONE;

	EGameXXKEnemyIntentPresentationState EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;

	float EnemyIntentPresentationElapsed = 0.0f;

	/** Index within the saved intent array; it remains stable while the resolve/settle showcase is visible. */
	int32 ActiveEnemyIntentPresentationIndex = INDEX_NONE;

	bool bEnemyIntentCompletionRecoveryPending = false;

	UPROPERTY(Transient)
	TArray<FName> PendingRewardCardIds;

	TArray<FGameXXKBattleRewardOption> PendingRewardOptions;

	UPROPERTY(Transient)
	FName SelectedRouteRewardReplacementEntryId = NAME_None;

	/** UI-only candidate key. Saved runtime state remains untouched until a complete atomic commit. */
	UPROPERTY(Transient)
	FName RouteRewardCardIdAwaitingReplacement = NAME_None;

	UPROPERTY(Transient)
	TMap<FName, FVector2D> RegisteredBattleUnitScreenPositions;

	UPROPERTY(Transient)
	FString LastCardInteractionError;

	UPROPERTY(Transient)
	FVector2D CommandMenuAnchor = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D SelectedPartyScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D TargetingSourcePosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D TargetingPointerPosition = FVector2D::ZeroVector;

	FVector2D LastPartyQiCanvasSize = FVector2D::ZeroVector;

};
