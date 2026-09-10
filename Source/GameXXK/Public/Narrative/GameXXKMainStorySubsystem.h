#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Narrative/GameXXKMainStoryRules.h"
#include "GameXXKMainStorySubsystem.generated.h"

class UGameXXKMVPSubsystem;
class UGameXXKMainStoryPanelWidget;
class UGameXXKDialoguePanelWidget;

DECLARE_MULTICAST_DELEGATE(FGameXXKMainStoryChanged);

/** Owns authored story activities; all player-state mutations are validated and persisted first. */
UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKMainStorySubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	UGameXXKMVPSubsystem* MVP() const;
	const FGameXXKRuntimeState* State() const;
	const FGameXXKMainStoryNode* ActiveNode() const;
	FName PreferredChapter() const;
	const FText& Feedback() const { return LastFeedback; }
	FGameXXKMainStoryChanged OnChanged;

	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool OpenChapter(FName ChapterId);
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool StartTask(FName NodeId);
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool BeginTaskJourney();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool AdvanceDialogue();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool ChooseAnswer(int32 OptionIndex);
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool RevealHint();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool ClaimReward(FName NodeId);
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") bool BeginTaskBattle();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") void PauseActivity();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") void OpenJourneyTree();
	UFUNCTION(BlueprintCallable, Category="GameXXK|MainStory") void CloseJourneyTree();
	UFUNCTION(BlueprintPure, Category="GameXXK|MainStory") FString GetProgressJson() const;

	bool EnterJourneyGate(int32 RouteNodeId);
	bool WantsRouteOverlay() const;
	void SetMVPForTest(UGameXXKMVPSubsystem* InMVP) { MVPOverride = InMVP; }

private:
	bool Commit(FGameXXKRuntimeState&& Candidate, bool bRefreshFlow = false);
	bool BeginJourney(FGameXXKRuntimeState& Candidate, const FGameXXKMainStoryNode& Node, FString& Error);
	bool PrepareTaskBattle(FGameXXKRuntimeState& Candidate, FString& Error);
	bool CompleteNonBattleGate(FGameXXKRuntimeState& Candidate, FString& Error);
	void RefreshViews(bool bRefreshFlow);
	void FlushPresentationViews();
	bool WantsRouteDialogue() const;
	void SetError(const FString& Error);
	FText LastFeedback;
	FName SelectedChapterId;
	bool bRefreshingViews = false;
	bool bPresentationRefreshQueued = false;
	bool bFlowRefreshPending = false;
	int32 LastLegacyMapAttemptRevision = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UGameXXKMVPSubsystem> MVPOverride;
	UPROPERTY(Transient) TObjectPtr<UGameXXKMainStoryPanelWidget> RoutePanel;
	UPROPERTY(Transient) TObjectPtr<UGameXXKDialoguePanelWidget> RouteDialoguePanel;
};
