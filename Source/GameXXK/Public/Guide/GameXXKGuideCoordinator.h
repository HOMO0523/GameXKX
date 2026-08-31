#pragma once

#include "CoreMinimal.h"
#include "Guide/GameXXKGuideRules.h"
#include "UObject/Object.h"

#include "GameXXKGuideCoordinator.generated.h"

class FGameXXKGuideTargetRegistry;
class UGameXXKGuideOverlayWidget;

DECLARE_DELEGATE_RetVal_OneParam(
	bool,
	FGameXXKGuidePersistenceDelegate,
	const FGameXXKGuideProgress&);
DECLARE_DELEGATE_OneParam(FGameXXKGuideCoordinatorFault, const FString&);

/** Owns one guide session and exactly one logical input-lock token. */
UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKGuideCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void Bind(
		FGameXXKGuideProgress& InProgress,
		FGameXXKGuideTargetRegistry& InRegistry,
		UGameXXKGuideOverlayWidget* InOverlay);
	void SetPersistenceDelegate(FGameXXKGuidePersistenceDelegate InDelegate);
	void SetFaultDelegate(FGameXXKGuideCoordinatorFault InDelegate);

	bool ApplyPreference(EGameXXKGuidePreference Preference, FString* OutError = nullptr);
	bool ResetCombatGuide(FString* OutError = nullptr);
	bool StartGuide(UGameXXKGuideAsset& Asset, FName TriggerEventId, FString* OutError = nullptr);
	bool ResumeGuide(UGameXXKGuideAsset& Asset, FString* OutError = nullptr);
	bool HandleEvent(FName EventId, FString* OutError = nullptr);
	bool RefreshTarget(FString* OutError = nullptr);
	bool CanExecuteAction(FName ActionId) const;
	void SuspendPresentation();

	void Cancel(const FString& Diagnostic = FString());
	void CancelForMapTravel();
	void NotifyOverlayDestroyed();

	bool IsInputTokenHeld() const;
	int32 GetInputTokenAcquisitionCountForTest() const;
	static bool ShouldShowPreferencePrompt(const FGameXXKGuideProgress& Progress);

private:
	bool PersistAndCommit(const FGameXXKGuideProgress& Candidate, FString* OutError);
	bool ResolveUnavailableForcedTargets(
		UGameXXKGuideAsset& Asset,
		FGameXXKGuideProgress& InOutCandidate,
		FGameXXKGuideOutput& InOutOutput,
		FString* OutError);
	void PresentOutput(const FGameXXKGuideOutput& Output);
	void AcquireInputToken();
	void ReleaseInputToken();
	void DismissOverlay();
	void NotifyFault(const FString& Diagnostic);

	FGameXXKGuideProgress* Progress = nullptr;
	FGameXXKGuideTargetRegistry* Registry = nullptr;
	TWeakObjectPtr<UGameXXKGuideOverlayWidget> Overlay;
	TWeakObjectPtr<UGameXXKGuideAsset> ActiveAsset;
	FGameXXKGuidePersistenceDelegate PersistenceDelegate;
	FGameXXKGuideCoordinatorFault FaultDelegate;
	bool bInputTokenHeld = false;
	bool bFaultNotified = false;
	int32 InputTokenAcquisitionCount = 0;
};
