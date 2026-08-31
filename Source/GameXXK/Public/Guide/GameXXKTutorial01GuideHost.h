#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "Guide/GameXXKGuideAsset.h"
#include "UObject/Object.h"

#include "GameXXKTutorial01GuideHost.generated.h"

class FGameXXKGuideTargetRegistry;
class UGameXXKGuideCoordinator;
class UGameXXKGuideOverlayWidget;

DECLARE_DELEGATE_OneParam(FGameXXKTutorial01GuideFailed, const FString&);

UCLASS()
class GAMEXXK_API UGameXXKTutorial01GuideHost : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void Bind(
		FGameXXKGuideProgress& InProgress,
		FGameXXKGuideTargetRegistry& InRegistry,
		UGameXXKGuideOverlayWidget& InOverlay,
		UGameXXKGuideAsset& InAsset,
		FGameXXKTutorial01GuideFailed OnFailed);
	bool Start();
	bool HandleContinue();
	void HandleGuideEvent(FName EventId);
	void Tick(
		float DeltaSeconds,
		bool bPaused,
		bool bBattleBusy,
		EGameXXKCardBattlePhase Phase);
	void Cancel(const FString& Diagnostic = FString());

	bool IsSuspendedForEnemyTurnForTest() const { return bSuspendedForEnemyTurn; }
	bool IsStarted() const { return bStarted; }

private:
	void ApplyActionGate();
	void ClearActionGate();
	void Finish();
	void Fail(const FString& Diagnostic);
	void HandleCoordinatorFault(const FString& Diagnostic);

	FGameXXKGuideProgress* Progress = nullptr;
	FGameXXKGuideTargetRegistry* Registry = nullptr;
	TWeakObjectPtr<UGameXXKGuideOverlayWidget> Overlay;
	TWeakObjectPtr<UGameXXKGuideAsset> Asset;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuideCoordinator> Coordinator;

	FGameXXKTutorial01GuideFailed FailedDelegate;
	FDelegateHandle GuideEventHandle;
	float PlayerTurnWatchdogElapsed = 0.0f;
	bool bStarted = false;
	bool bSuspendedForEnemyTurn = false;
	bool bFailed = false;
};
