#include "Dialogue/GameXXKDialogueCoordinator.h"

#include "Dialogue/GameXXKDialogueAsset.h"
#include "Narrative/GameXXKCharacterCatalog.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKSpeechBubbleWidget.h"

namespace GameXXKDialogueCoordinatorPrivate
{
	const TCHAR* CharacterCatalogObjectPath =
		TEXT("/Game/GameXXK/Narrative/Characters/DA_CharacterCatalog.DA_CharacterCatalog");

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

void UGameXXKDialogueCoordinator::Bind(
	FGameXXKDialogueSessionState& InSession,
	UGameXXKDialoguePanelWidget* InPanel,
	UGameXXKSpeechBubbleWidget* InBubble,
	UGameXXKDialogueHistoryWidget* InHistory)
{
	Session = &InSession;
	Panel = InPanel;
	Bubble = InBubble;
	History = InHistory;
	if (InPanel)
	{
		InPanel->SetAdvanceRequested(FGameXXKDialogueAdvanceRequested::CreateUObject(
			this,
			&UGameXXKDialogueCoordinator::HandlePanelAdvance));
		InPanel->SetOptionRequested(FGameXXKDialogueOptionRequested::CreateUObject(
			this,
			&UGameXXKDialogueCoordinator::HandlePanelOption));
	}
}

void UGameXXKDialogueCoordinator::SetBubbleAnchorResolver(FGameXXKDialogueBubbleAnchorResolver InResolver)
{
	BubbleAnchorResolver = MoveTemp(InResolver);
}

void UGameXXKDialogueCoordinator::SetPresenterPausedDelegate(FGameXXKDialoguePresenterPaused InDelegate)
{
	PresenterPausedDelegate = MoveTemp(InDelegate);
}

bool UGameXXKDialogueCoordinator::StartDialogue(
	const UGameXXKDialogueAsset& Asset,
	const FGameXXKDialogueStartContext& Context,
	FGameXXKDialogueFinished OnFinished,
	FString* OutError)
{
	using namespace GameXXKDialogueCoordinatorPrivate;
	ClearError(OutError);
	if (!Session)
	{
		return SetError(OutError, TEXT("Dialogue coordinator has no bound session."));
	}
	if (Session->bActive || ActiveAsset.IsValid())
	{
		return SetError(OutError, TEXT("A blocking dialogue presentation is already active."));
	}

	FGameXXKDialogueSessionState Candidate = *Session;
	FGameXXKDialogueOutput Output;
	if (!FGameXXKDialogueRules::Start(Asset, Context, Candidate, Output, OutError))
	{
		return false;
	}
	*Session = MoveTemp(Candidate);
	ActiveAsset = &Asset;
	ActiveDialogueId = Asset.DialogueId;
	FinishedDelegate = MoveTemp(OnFinished);
	bFinishDispatched = false;
	bPaused = false;
	return PresentOutput(Output, OutError);
}

bool UGameXXKDialogueCoordinator::ResumeDialogue(
	const UGameXXKDialogueAsset& Asset,
	FString* OutError)
{
	FGameXXKDialogueFinished ExistingFinished = FinishedDelegate;
	return ResumeDialogue(Asset, MoveTemp(ExistingFinished), OutError);
}

bool UGameXXKDialogueCoordinator::ResumeDialogue(
	const UGameXXKDialogueAsset& Asset,
	FGameXXKDialogueFinished OnFinished,
	FString* OutError)
{
	using namespace GameXXKDialogueCoordinatorPrivate;
	ClearError(OutError);
	if (!Session || !Session->bActive)
	{
		return SetError(OutError, TEXT("No dialogue session can be resumed."));
	}
	FGameXXKDialogueSessionState Candidate = *Session;
	Candidate.PauseReason.Reset();
	FGameXXKDialogueOutput Output;
	if (!FGameXXKDialogueRules::Resume(Asset, Candidate, Output, OutError))
	{
		return false;
	}
	*Session = MoveTemp(Candidate);
	ActiveAsset = &Asset;
	ActiveDialogueId = Asset.DialogueId;
	FinishedDelegate = MoveTemp(OnFinished);
	bFinishDispatched = false;
	bPaused = false;
	return PresentOutput(Output, OutError);
}

bool UGameXXKDialogueCoordinator::Advance(FString* OutError)
{
	using namespace GameXXKDialogueCoordinatorPrivate;
	const UGameXXKDialogueAsset* Asset = ActiveAsset.Get();
	if (!Session || !Asset || !Session->bActive || bPaused || !CurrentOutput.Options.IsEmpty())
	{
		return SetError(OutError, TEXT("Dialogue cannot advance from the current presentation."));
	}
	FGameXXKDialogueSessionState Candidate = *Session;
	FGameXXKDialogueOutput Output;
	if (!FGameXXKDialogueRules::CompletePresentedNode(*Asset, Candidate, Output, OutError))
	{
		return false;
	}
	*Session = MoveTemp(Candidate);
	return PresentOutput(Output, OutError);
}

bool UGameXXKDialogueCoordinator::ChooseOption(const FName OptionId, FString* OutError)
{
	using namespace GameXXKDialogueCoordinatorPrivate;
	const UGameXXKDialogueAsset* Asset = ActiveAsset.Get();
	if (!Session || !Asset || !Session->bActive || bPaused || OptionId.IsNone())
	{
		return SetError(OutError, TEXT("Dialogue option cannot be chosen now."));
	}
	FGameXXKDialogueSessionState Candidate = *Session;
	FGameXXKDialogueOutput Output;
	if (!FGameXXKDialogueRules::Choose(*Asset, OptionId, Candidate, Output, OutError))
	{
		return false;
	}
	*Session = MoveTemp(Candidate);
	return PresentOutput(Output, OutError);
}

bool UGameXXKDialogueCoordinator::SkipSeenCurrentNode(FString* OutError)
{
	if (!Session || !Session->bActive || !Session->SeenNodeIds.Contains(Session->CurrentNodeId))
	{
		return GameXXKDialogueCoordinatorPrivate::SetError(
			OutError,
			TEXT("Only a previously seen current line may be skipped."));
	}
	return Advance(OutError);
}

void UGameXXKDialogueCoordinator::PauseAndExit()
{
	if (Session && Session->bActive)
	{
		Session->PauseReason = TEXT("Dialogue presenter paused by player.");
	}
	bPaused = true;
	bBlockingPresentation = false;
	ActiveAsset.Reset();
	HidePresenters();
	if (PresenterPausedDelegate.IsBound())
	{
		PresenterPausedDelegate.Execute();
	}
}

void UGameXXKDialogueCoordinator::SetAutoEnabled(const bool bEnabled)
{
	bAutoEnabled = bEnabled;
	AutoElapsedSeconds = 0.0f;
}

bool UGameXXKDialogueCoordinator::IsAutoEnabled() const
{
	return bAutoEnabled;
}

void UGameXXKDialogueCoordinator::SetPresentationDurations(
	const float InVoiceDurationSeconds,
	const float InAnimationDurationSeconds)
{
	VoiceDurationSeconds = FMath::Max(0.0f, InVoiceDurationSeconds);
	AnimationDurationSeconds = FMath::Max(0.0f, InAnimationDurationSeconds);
}

bool UGameXXKDialogueCoordinator::TickAuto(const float DeltaSeconds, FString* OutError)
{
	if (!bAutoEnabled
		|| bPaused
		|| !bBlockingPresentation
		|| !CurrentOutput.Options.IsEmpty()
		|| DeltaSeconds < 0.0f)
	{
		return false;
	}
	AutoElapsedSeconds += DeltaSeconds;
	const float RequiredDelay = ComputeAutoDelayForTest(
		CountVisibleCharacters(CurrentOutput.Text),
		VoiceDurationSeconds,
		AnimationDurationSeconds);
	if (AutoElapsedSeconds + UE_KINDA_SMALL_NUMBER < RequiredDelay)
	{
		return false;
	}
	return Advance(OutError);
}

float UGameXXKDialogueCoordinator::ComputeAutoDelayForTest(
	const int32 VisibleCharacters,
	const float InVoiceDurationSeconds,
	const float InAnimationDurationSeconds)
{
	const float TextDelay = FMath::Clamp(FMath::Max(0, VisibleCharacters) * 0.06f, 1.2f, 6.0f);
	return FMath::Max3(
		TextDelay,
		FMath::Max(0.0f, InVoiceDurationSeconds),
		FMath::Max(0.0f, InAnimationDurationSeconds));
}

bool UGameXXKDialogueCoordinator::IsBlockingPresentation() const
{
	return bBlockingPresentation;
}

FName UGameXXKDialogueCoordinator::GetCurrentNodeIdForTest() const
{
	return CurrentOutput.NodeId;
}

const FGameXXKDialogueOutput& UGameXXKDialogueCoordinator::GetCurrentOutputForTest() const
{
	return CurrentOutput;
}

bool UGameXXKDialogueCoordinator::PresentOutput(
	const FGameXXKDialogueOutput& Output,
	FString* OutError)
{
	CurrentOutput = Output;
	AutoElapsedSeconds = 0.0f;
	VoiceDurationSeconds = 0.0f;
	AnimationDurationSeconds = 0.0f;
	RefreshHistory();
	if (Output.bEnded)
	{
		HidePresenters();
		bBlockingPresentation = false;
		ActiveAsset.Reset();
		FinishOnce(Output.OutcomeId);
		GameXXKDialogueCoordinatorPrivate::ClearError(OutError);
		return true;
	}

	const FGameXXKDialoguePresentationView View = BuildPresentationView(Output);
	if (Output.Presentation == EGameXXKDialoguePresentation::DialoguePanel)
	{
		if (UGameXXKDialoguePanelWidget* PanelWidget = Panel.Get())
		{
			PanelWidget->Present(View);
		}
		else
		{
			return GameXXKDialogueCoordinatorPrivate::SetError(OutError, TEXT("Formal dialogue panel is unavailable."));
		}
		if (UGameXXKSpeechBubbleWidget* BubbleWidget = Bubble.Get()) BubbleWidget->ClearBubble();
	}
	else if (Output.Presentation == EGameXXKDialoguePresentation::Bubble)
	{
		UGameXXKSpeechBubbleWidget* BubbleWidget = Bubble.Get();
		USceneComponent* Anchor = BubbleAnchorResolver.IsBound()
			? BubbleAnchorResolver.Execute(Output.SpeakerId)
			: nullptr;
		if (!BubbleWidget || !BubbleWidget->PresentBubble(View, Anchor))
		{
			if (Session) Session->PauseReason = TEXT("Dialogue bubble anchor is unavailable.");
			bPaused = true;
			bBlockingPresentation = false;
			return GameXXKDialogueCoordinatorPrivate::SetError(OutError, TEXT("Dialogue bubble anchor is unavailable."));
		}
		if (UGameXXKDialoguePanelWidget* PanelWidget = Panel.Get()) PanelWidget->ClearPresentation();
	}
	else
	{
		return GameXXKDialogueCoordinatorPrivate::SetError(OutError, TEXT("Dialogue output has no presenter."));
	}
	bPaused = false;
	bBlockingPresentation = true;
	GameXXKDialogueCoordinatorPrivate::ClearError(OutError);
	return true;
}

FGameXXKDialoguePresentationView UGameXXKDialogueCoordinator::BuildPresentationView(
	const FGameXXKDialogueOutput& Output) const
{
	FGameXXKDialoguePresentationView View;
	View.NodeId = Output.NodeId;
	View.SpeakerDisplayName = FText::FromName(Output.SpeakerId);
	if (const UGameXXKCharacterCatalog* Catalog =
		LoadObject<UGameXXKCharacterCatalog>(nullptr, GameXXKDialogueCoordinatorPrivate::CharacterCatalogObjectPath))
	{
		if (const FGameXXKCharacterDefinition* Character = Catalog->FindCharacter(Output.SpeakerId))
		{
			View.SpeakerDisplayName = Character->DisplayName;
			View.PortraitPath = Character->PortraitPath;
		}
	}
	View.Text = Output.Text;
	View.Options = Output.Options;
	return View;
}

void UGameXXKDialogueCoordinator::RefreshHistory()
{
	if (Session)
	{
		if (UGameXXKDialogueHistoryWidget* HistoryWidget = History.Get())
		{
			HistoryWidget->PresentHistory(Session->History);
			HistoryWidget->HideHistory();
		}
	}
}

void UGameXXKDialogueCoordinator::FinishOnce(const FName OutcomeId)
{
	if (bFinishDispatched)
	{
		return;
	}
	bFinishDispatched = true;
	const FName DialogueId = ActiveAsset.IsValid()
		? ActiveAsset->DialogueId
		: ActiveDialogueId;
	if (FinishedDelegate.IsBound())
	{
		FGameXXKDialogueFinished Callback = MoveTemp(FinishedDelegate);
		FinishedDelegate.Unbind();
		Callback.Execute(DialogueId, OutcomeId);
	}
	ActiveDialogueId = NAME_None;
}

void UGameXXKDialogueCoordinator::HidePresenters()
{
	if (UGameXXKDialoguePanelWidget* PanelWidget = Panel.Get()) PanelWidget->ClearPresentation();
	if (UGameXXKSpeechBubbleWidget* BubbleWidget = Bubble.Get()) BubbleWidget->ClearBubble();
	if (UGameXXKDialogueHistoryWidget* HistoryWidget = History.Get()) HistoryWidget->HideHistory();
}

int32 UGameXXKDialogueCoordinator::CountVisibleCharacters(const FText& Text)
{
	int32 Count = 0;
	for (const TCHAR Character : Text.ToString())
	{
		if (!FChar::IsWhitespace(Character))
		{
			++Count;
		}
	}
	return Count;
}

void UGameXXKDialogueCoordinator::HandlePanelAdvance()
{
	Advance(nullptr);
}

void UGameXXKDialogueCoordinator::HandlePanelOption(const FName OptionId)
{
	ChooseOption(OptionId, nullptr);
}
