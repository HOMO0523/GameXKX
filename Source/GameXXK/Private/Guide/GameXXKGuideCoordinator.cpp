#include "Guide/GameXXKGuideCoordinator.h"

#include "Guide/GameXXKGuideRules.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "UI/GameXXKGuideOverlayWidget.h"

namespace GameXXKGuideCoordinatorPrivate
{
	constexpr int32 MaximumMissingTargetSkips = 64;

	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}
}

void UGameXXKGuideCoordinator::Bind(
	FGameXXKGuideProgress& InProgress,
	FGameXXKGuideTargetRegistry& InRegistry,
	UGameXXKGuideOverlayWidget* InOverlay)
{
	Progress = &InProgress;
	Registry = &InRegistry;
	Overlay = InOverlay;
	if (InOverlay)
	{
		InOverlay->SetDestroyedDelegate(FGameXXKGuideOverlayDestroyed::CreateUObject(
			this,
			&UGameXXKGuideCoordinator::NotifyOverlayDestroyed));
	}
}

void UGameXXKGuideCoordinator::SetPersistenceDelegate(FGameXXKGuidePersistenceDelegate InDelegate)
{
	PersistenceDelegate = MoveTemp(InDelegate);
}

void UGameXXKGuideCoordinator::SetFaultDelegate(FGameXXKGuideCoordinatorFault InDelegate)
{
	FaultDelegate = MoveTemp(InDelegate);
}

bool UGameXXKGuideCoordinator::ApplyPreference(
	const EGameXXKGuidePreference Preference,
	FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	if (!Progress || Preference == EGameXXKGuidePreference::Unset)
	{
		return SetError(OutError, TEXT("Guide preference requires bound progress and a resolved choice."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	if (!Candidate.ActiveGuideId.IsNone())
	{
		FGameXXKGuideRules::Cancel(Candidate, TEXT("Guide preference changed."));
	}
	Candidate.Preference = Preference;
	if (!PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	ActiveAsset.Reset();
	ReleaseInputToken();
	DismissOverlay();
	return true;
}

bool UGameXXKGuideCoordinator::ResetCombatGuide(FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	if (!Progress)
	{
		return SetError(OutError, TEXT("Guide coordinator has no bound progress."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	FGameXXKGuideRules::ResetCombatGuide(Candidate);
	if (!PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	ActiveAsset.Reset();
	ReleaseInputToken();
	DismissOverlay();
	return true;
}

bool UGameXXKGuideCoordinator::StartGuide(
	UGameXXKGuideAsset& Asset,
	const FName TriggerEventId,
	FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	bFaultNotified = false;
	if (!Progress || !Registry)
	{
		return SetError(OutError, TEXT("Guide coordinator is not bound."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	FGameXXKGuideOutput Output;
	if (!FGameXXKGuideRules::TryStart(
		Asset,
		TriggerEventId,
		Candidate,
		Output,
		OutError))
	{
		return false;
	}
	if (!ResolveUnavailableForcedTargets(Asset, Candidate, Output, OutError)
		|| !PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	ActiveAsset = Candidate.ActiveGuideId.IsNone() ? nullptr : &Asset;
	PresentOutput(Output);
	return true;
}

bool UGameXXKGuideCoordinator::HandleEvent(const FName EventId, FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	UGameXXKGuideAsset* Asset = ActiveAsset.Get();
	if (!Progress || !Registry || !Asset)
	{
		return SetError(OutError, TEXT("Guide coordinator has no active asset."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	FGameXXKGuideOutput Output;
	if (!FGameXXKGuideRules::HandleEvent(*Asset, EventId, Candidate, Output, OutError))
	{
		return false;
	}
	if (!ResolveUnavailableForcedTargets(*Asset, Candidate, Output, OutError)
		|| !PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	if (Candidate.ActiveGuideId.IsNone())
	{
		ActiveAsset.Reset();
	}
	PresentOutput(Output);
	return true;
}

bool UGameXXKGuideCoordinator::ResumeGuide(UGameXXKGuideAsset& Asset, FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	bFaultNotified = false;
	if (!Progress || !Registry)
	{
		return SetError(OutError, TEXT("Guide coordinator is not bound."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	FGameXXKGuideOutput Output;
	if (!FGameXXKGuideRules::Resume(Asset, Candidate, Output, OutError)
		|| !ResolveUnavailableForcedTargets(Asset, Candidate, Output, OutError))
	{
		return false;
	}
	if (!(Candidate.ActiveGuideId == Progress->ActiveGuideId
		&& Candidate.ActiveGuideStepId == Progress->ActiveGuideStepId
		&& Candidate.LastDiagnostic == Progress->LastDiagnostic)
		&& !PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	ActiveAsset = Candidate.ActiveGuideId.IsNone() ? nullptr : &Asset;
	PresentOutput(Output);
	return true;
}

bool UGameXXKGuideCoordinator::RefreshTarget(FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	ClearError(OutError);
	UGameXXKGuideAsset* Asset = ActiveAsset.Get();
	if (!Progress || !Registry || !Asset)
	{
		return SetError(OutError, TEXT("Guide coordinator has no active guide to refresh."));
	}
	FGameXXKGuideProgress Candidate = *Progress;
	FGameXXKGuideOutput Output;
	if (!FGameXXKGuideRules::Resume(*Asset, Candidate, Output, OutError)
		|| !ResolveUnavailableForcedTargets(*Asset, Candidate, Output, OutError))
	{
		return false;
	}
	if (!(Candidate.ActiveGuideId == Progress->ActiveGuideId
		&& Candidate.ActiveGuideStepId == Progress->ActiveGuideStepId
		&& Candidate.LastDiagnostic == Progress->LastDiagnostic)
		&& !PersistAndCommit(Candidate, OutError))
	{
		return false;
	}
	if (Candidate.ActiveGuideId.IsNone())
	{
		ActiveAsset.Reset();
	}
	PresentOutput(Output);
	return true;
}

bool UGameXXKGuideCoordinator::CanExecuteAction(const FName ActionId) const
{
	if (!Progress)
	{
		return true;
	}
	if (const UGameXXKGuideAsset* Asset = ActiveAsset.Get())
	{
		return FGameXXKGuideRules::CanExecuteAction(*Asset, *Progress, ActionId);
	}
	return true;
}

void UGameXXKGuideCoordinator::SuspendPresentation()
{
	ReleaseInputToken();
	DismissOverlay();
}

void UGameXXKGuideCoordinator::Cancel(const FString& Diagnostic)
{
	if (Progress)
	{
		FGameXXKGuideProgress Candidate = *Progress;
		FGameXXKGuideRules::Cancel(Candidate, Diagnostic);
		if (!PersistenceDelegate.IsBound() || PersistenceDelegate.Execute(Candidate))
		{
			*Progress = MoveTemp(Candidate);
		}
	}
	ActiveAsset.Reset();
	ReleaseInputToken();
	DismissOverlay();
}

void UGameXXKGuideCoordinator::CancelForMapTravel()
{
	Cancel(TEXT("Guide cancelled for map travel."));
}

void UGameXXKGuideCoordinator::NotifyOverlayDestroyed()
{
	const bool bHadActivePresentation = bInputTokenHeld || ActiveAsset.IsValid();
	Overlay.Reset();
	ActiveAsset.Reset();
	ReleaseInputToken();
	if (bHadActivePresentation)
	{
		NotifyFault(TEXT("Guide overlay was destroyed during an active guide."));
	}
}

bool UGameXXKGuideCoordinator::IsInputTokenHeld() const
{
	return bInputTokenHeld;
}

int32 UGameXXKGuideCoordinator::GetInputTokenAcquisitionCountForTest() const
{
	return InputTokenAcquisitionCount;
}

bool UGameXXKGuideCoordinator::ShouldShowPreferencePrompt(const FGameXXKGuideProgress& Progress)
{
	return Progress.Preference == EGameXXKGuidePreference::Unset;
}

bool UGameXXKGuideCoordinator::PersistAndCommit(
	const FGameXXKGuideProgress& Candidate,
	FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	if (!Progress)
	{
		return SetError(OutError, TEXT("Guide coordinator has no bound progress."));
	}
	if (PersistenceDelegate.IsBound() && !PersistenceDelegate.Execute(Candidate))
	{
		return SetError(OutError, TEXT("Guide progress persistence failed."));
	}
	*Progress = Candidate;
	return true;
}

bool UGameXXKGuideCoordinator::ResolveUnavailableForcedTargets(
	UGameXXKGuideAsset& Asset,
	FGameXXKGuideProgress& InOutCandidate,
	FGameXXKGuideOutput& InOutOutput,
	FString* OutError)
{
	using namespace GameXXKGuideCoordinatorPrivate;
	for (int32 SkipIndex = 0; SkipIndex < MaximumMissingTargetSkips; ++SkipIndex)
	{
		if (!InOutOutput.bActive || InOutOutput.InputPolicy != EGameXXKGuideInputPolicy::Forced)
		{
			return true;
		}
		TArray<FName> RequiredTargetIds = InOutOutput.TargetIds;
		if (!InOutOutput.BubbleAnchorTargetId.IsNone())
		{
			RequiredTargetIds.AddUnique(InOutOutput.BubbleAnchorTargetId);
		}
		UGameXXKGuideOverlayWidget* OverlayWidget = Overlay.Get();
		FName MissingTargetId = NAME_None;
		for (const FName TargetId : RequiredTargetIds)
		{
			FSlateRect TargetRect;
			if (Registry && OverlayWidget
				&& Registry->ResolveTargetRect(TargetId, *OverlayWidget, TargetRect))
			{
				continue;
			}
			// A live widget may be registered one frame before Slate assigns non-zero
			// geometry. That is pending layout, not a permanently missing target.
			if (Registry && Registry->IsTargetRegistered(TargetId))
			{
				continue;
			}
			MissingTargetId = TargetId;
			break;
		}
		if (MissingTargetId.IsNone())
		{
			return true;
		}
		if (!FGameXXKGuideRules::HandleTargetUnavailable(
			Asset,
			MissingTargetId,
			InOutCandidate,
			InOutOutput,
			OutError))
		{
			const FString Diagnostic = OutError && !OutError->IsEmpty()
				? *OutError
				: FString::Printf(
					TEXT("Guide target unavailable: %s"),
					*MissingTargetId.ToString());
			ReleaseInputToken();
			DismissOverlay();
			NotifyFault(Diagnostic);
			return false;
		}
	}
	return SetError(OutError, TEXT("Guide exceeded the missing-target skip limit."));
}

void UGameXXKGuideCoordinator::PresentOutput(const FGameXXKGuideOutput& Output)
{
	ReleaseInputToken();
	UGameXXKGuideOverlayWidget* OverlayWidget = Overlay.Get();
	if (!OverlayWidget || !Output.bActive || !Registry)
	{
		DismissOverlay();
		return;
	}
	if (Output.TargetIds.IsEmpty())
	{
		DismissOverlay();
		return;
	}
	TArray<FSlateRect> TargetRects;
	TargetRects.Reserve(Output.TargetIds.Num());
	for (const FName TargetId : Output.TargetIds)
	{
		FSlateRect TargetRect;
		if (!Registry->ResolveTargetRect(TargetId, *OverlayWidget, TargetRect))
		{
			DismissOverlay();
			return;
		}
		TargetRects.Add(TargetRect);
	}
	TOptional<FSlateRect> BubbleAnchorRect;
	if (!Output.BubbleAnchorTargetId.IsNone())
	{
		FSlateRect ResolvedBubbleAnchor;
		if (!Registry->ResolveTargetRect(
				Output.BubbleAnchorTargetId,
				*OverlayWidget,
				ResolvedBubbleAnchor))
		{
			DismissOverlay();
			return;
		}
		BubbleAnchorRect = ResolvedBubbleAnchor;
	}
	OverlayWidget->PresentGuide(Output, TargetRects, BubbleAnchorRect);
	if (Output.InputPolicy == EGameXXKGuideInputPolicy::Forced)
	{
		AcquireInputToken();
	}
}

void UGameXXKGuideCoordinator::AcquireInputToken()
{
	if (!bInputTokenHeld)
	{
		bInputTokenHeld = true;
		++InputTokenAcquisitionCount;
	}
}

void UGameXXKGuideCoordinator::ReleaseInputToken()
{
	bInputTokenHeld = false;
}

void UGameXXKGuideCoordinator::DismissOverlay()
{
	if (UGameXXKGuideOverlayWidget* OverlayWidget = Overlay.Get())
	{
		OverlayWidget->DismissGuide();
	}
}

void UGameXXKGuideCoordinator::NotifyFault(const FString& Diagnostic)
{
	ReleaseInputToken();
	if (!bFaultNotified && FaultDelegate.IsBound())
	{
		bFaultNotified = true;
		FaultDelegate.Execute(Diagnostic);
	}
}
