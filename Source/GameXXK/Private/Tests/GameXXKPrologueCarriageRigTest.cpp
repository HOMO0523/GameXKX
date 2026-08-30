#include "Town/GameXXKPrologueCarriageRig.h"

#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Prologue/GameXXKPrologueCarriageTypes.h"
#include "UI/GameXXKPrologueCarriageWidget.h"
#include "UI/GameXXKProloguePauseWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueCarriageRigTest,
	"GameXXK.Prologue.Carriage.Rig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueCarriageRigTest::RunTest(const FString& Parameters)
{
	const AGameXXKPrologueCarriageRig* Defaults =
		GetDefault<AGameXXKPrologueCarriageRig>();
	if (!TestNotNull(TEXT("carriage Rig CDO exists"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("start is camera-left by four hundred units"),
		Defaults->GetStartOffsetForTest(),
		FVector(0.0f, -400.0f, 0.0f));
	TestEqual(TEXT("stop is the Rig anchor"),
		Defaults->GetStopOffsetForTest(),
		FVector::ZeroVector);
	TestEqual(TEXT("exit continues in the same direction"),
		Defaults->GetExitOffsetForTest(),
		FVector(0.0f, 800.0f, 0.0f));
	TestEqual(TEXT("hero appears closer to camera than the carriage path"),
		Defaults->GetHeroRevealOffsetForTest(),
		FVector(-80.0f, 0.0f, 0.0f));
	TestEqual(TEXT("carriage display applies capsule-to-ground offset"),
		AGameXXKPrologueCarriageRig::ApplyDisplayGroundOffsetForTest(
			FVector(16678.592f, 5270.0f, 1075.711f)),
		FVector(16678.592f, 5270.0f, 1003.711f));
	TestEqual(TEXT("carriage sorts one layer behind the town hero"),
		Defaults->GetCarriageSortPriorityForTest(), 9);
	TestEqual(TEXT("carriage canvas matches authored five-twelve cells"),
		Defaults->GetCarriageDrawSizeForTest(),
		FVector2D(512.0f, 512.0f));
	TestEqual(TEXT("carriage uses bottom-center world pivot"),
		Defaults->GetCarriagePivotForTest(),
		FVector2D(0.5f, 1.0f));
	TestEqual(TEXT("carriage default plane faces the town camera axis"),
		Defaults->GetCarriageDisplayRotationForTest(),
		FRotator(0.0f, 180.0f, 0.0f));
	TestTrue(TEXT("intro camera copies the live hero camera pose"),
		Defaults->UsesHeroCameraPoseForTest());
	TestEqual(TEXT("billboard faces a camera on negative X"),
		AGameXXKPrologueCarriageRig::ResolveCarriageFacingRotationForTest(
			FVector::ZeroVector,
			FVector(-400.0f, 0.0f, 700.0f)),
		FRotator(0.0f, 180.0f, 0.0f));
	TestEqual(TEXT("billboard faces a camera on positive Y"),
		AGameXXKPrologueCarriageRig::ResolveCarriageFacingRotationForTest(
			FVector::ZeroVector,
			FVector(0.0f, 400.0f, 700.0f)),
		FRotator(0.0f, 90.0f, 0.0f));
	TestEqual(TEXT("carriage display uses the real atlas widget"),
		Defaults->GetCarriageWidgetClassForTest().Get(),
		UGameXXKPrologueCarriageWidget::StaticClass());

	TestFalse(TEXT("ordinary town URL leaves the Rig dormant"),
		Defaults->ShouldActivateForOptionsForTest(TEXT("")));
	TestFalse(TEXT("unrelated URL option leaves the Rig dormant"),
		Defaults->ShouldActivateForOptionsForTest(TEXT("?GameXXKIntro=Other")));
	TestTrue(TEXT("explicit preview URL activates the Rig"),
		Defaults->ShouldActivateForOptionsForTest(
			TEXT("?GameXXKIntro=CarriagePreview")));

	TestEqual(TEXT("run-stop 2K texture path is isolated"),
		Defaults->GetRunStopTexturePathForTest(false),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_run_stop_2k_atlas."
			"T_cinematic_carriage_run_stop_2k_atlas")));
	TestEqual(TEXT("idle 2K texture path is isolated"),
		Defaults->GetPostStopIdleTexturePathForTest(false),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_post_stop_idle_2k_atlas."
			"T_cinematic_carriage_post_stop_idle_2k_atlas")));
	TestEqual(TEXT("run-stop 1K fallback path is isolated"),
		Defaults->GetRunStopTexturePathForTest(true),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_run_stop_1k_atlas."
			"T_cinematic_carriage_run_stop_1k_atlas")));
	TestEqual(TEXT("idle 1K fallback path is isolated"),
		Defaults->GetPostStopIdleTexturePathForTest(true),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_post_stop_idle_1k_atlas."
			"T_cinematic_carriage_post_stop_idle_1k_atlas")));

	AGameXXKPrologueCarriageRig* TimelineRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	if (!TestNotNull(TEXT("timeline Rig fixture exists"), TimelineRig))
	{
		return false;
	}
	TestTrue(TEXT("test seam starts the same pure timeline"),
		TimelineRig->StartTimelineForTest());
	TimelineRig->AdvanceTimelineForTest(4.04f);
	TestEqual(TEXT("Rig timeline reaches parked"),
		TimelineRig->GetTimelineStateForTest().Phase,
		EGameXXKPrologueCarriagePhase::Parked);
	TimelineRig->AdvanceTimelineForTest(0.0f);
	TestEqual(TEXT("Rig timeline reaches hero reveal"),
		TimelineRig->GetTimelineStateForTest().Phase,
		EGameXXKPrologueCarriagePhase::HeroRevealed);
	TimelineRig->AdvanceTimelineForTest(0.0f);
	TimelineRig->AdvanceTimelineForTest(2.0f);
	TestEqual(TEXT("Rig timeline begins departure after the exact hold"),
		TimelineRig->GetTimelineStateForTest().Phase,
		EGameXXKPrologueCarriagePhase::Departing);

	AGameXXKPrologueCarriageRig* PauseTimelineRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	TestTrue(TEXT("pause timeline fixture starts"),
		PauseTimelineRig && PauseTimelineRig->StartTimelineForTest());
	PauseTimelineRig->AdvanceTimelineForTest(0.5f);
	TestTrue(TEXT("test seam pauses the real timeline"),
		PauseTimelineRig->SetSequencePausedForTest(true));
	const FGameXXKPrologueCarriageState PausedState =
		PauseTimelineRig->GetTimelineStateForTest();
	PauseTimelineRig->AdvanceTimelineForTest(10.0f);
	TestTrue(TEXT("paused Rig cannot advance"),
		PauseTimelineRig->GetTimelineStateForTest() == PausedState);
	TestTrue(TEXT("test seam resumes the real timeline"),
		PauseTimelineRig->SetSequencePausedForTest(false));
	PauseTimelineRig->AdvanceTimelineForTest(0.5f);
	TestTrue(TEXT("resumed Rig advances from the same point"),
		PauseTimelineRig->GetTimelineStateForTest().PhaseElapsedSeconds
			> PausedState.PhaseElapsedSeconds);

	UGameXXKProloguePauseWidget* PauseWidget =
		NewObject<UGameXXKProloguePauseWidget>();
	if (!TestNotNull(TEXT("pause overlay fixture exists"), PauseWidget))
	{
		return false;
	}
	PauseWidget->TakeWidget();
	TestEqual(TEXT("pause overlay title is explicit"),
		PauseWidget->GetTitleTextForTest(),
		FText::FromString(TEXT("剧情已暂停")));
	TestEqual(TEXT("pause overlay exposes two recovery buttons"),
		PauseWidget->GetButtonCountForTest(), 2);
	int32 ResumeRequests = 0;
	int32 ReturnRequests = 0;
	PauseWidget->SetResumeRequested(
		FGameXXKPrologueResumeRequested::CreateLambda([&ResumeRequests]()
		{
			++ResumeRequests;
		}));
	PauseWidget->SetReturnDesktopRequested(
		FGameXXKPrologueReturnDesktopRequested::CreateLambda([&ReturnRequests]()
		{
			++ReturnRequests;
		}));
	PauseWidget->RequestResumeForTest();
	PauseWidget->RequestReturnDesktopForTest();
	TestEqual(TEXT("continue callback fires once"), ResumeRequests, 1);
	TestEqual(TEXT("return callback fires once"), ReturnRequests, 1);

	AGameXXKMVPPlayerController* Controller =
		NewObject<AGameXXKMVPPlayerController>();
	AGameXXKPrologueCarriageRig* OwnedRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	AGameXXKPrologueCarriageRig* ForeignRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	if (!TestTrue(TEXT("controller ownership fixtures exist"),
		Controller && OwnedRig && ForeignRig))
	{
		return false;
	}
	Controller->bShowMouseCursor = true;
	Controller->bEnableClickEvents = true;
	Controller->bEnableMouseOverEvents = false;
	Controller->SetIgnoreMoveInput(true);
	Controller->SetTrackedInputModeForTest(EGameXXKTrackedInputMode::UIOnly);

	TestFalse(TEXT("null Rig cannot acquire presentation"),
		Controller->BeginPrologueCarriagePresentation(nullptr));
	TestTrue(TEXT("one Rig acquires presentation"),
		Controller->BeginPrologueCarriagePresentation(OwnedRig));
	TestTrue(TEXT("controller exposes active presentation"),
		Controller->HasActivePrologueCarriageForTest());
	TestFalse(TEXT("second Rig cannot steal presentation"),
		Controller->BeginPrologueCarriagePresentation(ForeignRig));
	TestTrue(TEXT("pre-existing move ignore remains active"),
		Controller->IsMoveInputIgnored());
	TestTrue(TEXT("presentation owns a new look ignore"),
		Controller->IsLookInputIgnored());
	TestFalse(TEXT("cinematic hides mouse cursor"), Controller->bShowMouseCursor);
	TestFalse(TEXT("cinematic disables click events"), Controller->bEnableClickEvents);
	TestFalse(TEXT("cinematic disables mouse-over events"),
		Controller->bEnableMouseOverEvents);
	TestEqual(TEXT("cinematic uses game-only tracked input"),
		Controller->GetTrackedInputModeForTest(),
		EGameXXKTrackedInputMode::GameOnly);

	Controller->EndPrologueCarriagePresentation(ForeignRig);
	TestTrue(TEXT("foreign release cannot clear active owner"),
		Controller->HasActivePrologueCarriageForTest());
	Controller->EndPrologueCarriagePresentation(OwnedRig);
	TestFalse(TEXT("owner release clears active presentation"),
		Controller->HasActivePrologueCarriageForTest());
	TestTrue(TEXT("pre-existing move ignore restores exactly"),
		Controller->IsMoveInputIgnored());
	TestFalse(TEXT("owned look ignore is released"),
		Controller->IsLookInputIgnored());
	TestTrue(TEXT("cursor restores exactly"), Controller->bShowMouseCursor);
	TestTrue(TEXT("click events restore exactly"), Controller->bEnableClickEvents);
	TestFalse(TEXT("mouse-over events restore exactly"),
		Controller->bEnableMouseOverEvents);
	TestEqual(TEXT("tracked input mode restores exactly"),
		Controller->GetTrackedInputModeForTest(),
		EGameXXKTrackedInputMode::UIOnly);
	Controller->EndPrologueCarriagePresentation(OwnedRig);
	TestFalse(TEXT("repeated release remains harmless"),
		Controller->HasActivePrologueCarriageForTest());
	Controller->SetIgnoreMoveInput(false);

	AGameXXKMVPPlayerController* InputController =
		NewObject<AGameXXKMVPPlayerController>();
	AGameXXKPrologueCarriageRig* InputRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	if (!TestTrue(TEXT("input routing fixtures exist"),
		InputController && InputRig))
	{
		return false;
	}
	TestTrue(TEXT("input Rig timeline starts"), InputRig->StartTimelineForTest());
	InputRig->SetPresentationActiveForTest(true);
	TestTrue(TEXT("input Rig acquires presentation"),
		InputController->BeginPrologueCarriagePresentation(InputRig));
	TestTrue(TEXT("Escape pauses active carriage first"),
		InputController->TriggerPrologueInputForTest(EKeys::Escape));
	TestTrue(TEXT("Escape leaves the Rig paused"), InputRig->IsSequencePaused());
	TestEqual(TEXT("pause restores UI-capable input"),
		InputController->GetTrackedInputModeForTest(),
		EGameXXKTrackedInputMode::GameAndUI);
	TestTrue(TEXT("I cannot open inventory through a paused carriage"),
		InputController->TriggerPrologueInputForTest(EKeys::I));
	TestTrue(TEXT("Tab cannot open Workbench through a paused carriage"),
		InputController->TriggerPrologueInputForTest(EKeys::Tab));
	TestTrue(TEXT("second Escape resumes active carriage"),
		InputController->TriggerPrologueInputForTest(EKeys::Escape));
	TestFalse(TEXT("second Escape clears pause"), InputRig->IsSequencePaused());
	TestEqual(TEXT("resume restores cinematic game-only input"),
		InputController->GetTrackedInputModeForTest(),
		EGameXXKTrackedInputMode::GameOnly);
	InputRig->SetPresentationActiveForTest(false);
	InputController->EndPrologueCarriagePresentation(InputRig);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	if (!TestTrue(TEXT("failure invariant runtime starts"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FGameXXKRuntimeState RuntimeBefore = Subsystem->GetRuntimeState();
	AGameXXKMVPPlayerController* FailureController =
		NewObject<AGameXXKMVPPlayerController>();
	AGameXXKPrologueCarriageRig* FailureRig =
		NewObject<AGameXXKPrologueCarriageRig>();
	if (!TestTrue(TEXT("failure recovery fixtures exist"),
		FailureController && FailureRig))
	{
		return false;
	}
	TestTrue(TEXT("failure Rig timeline starts"), FailureRig->StartTimelineForTest());
	FailureRig->SetPresentationActiveForTest(true);
	TestTrue(TEXT("failure Rig acquires input"),
		FailureController->BeginPrologueCarriagePresentation(FailureRig));
	AddExpectedError(
		TEXT("Prologue carriage preview failed open: forced missing arrival texture"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	FailureRig->ForceFailureForTest(
		EGameXXKPrologueCarriageFailure::MissingArrivalTexture);
	TestFalse(TEXT("failure ends presentation"),
		FailureRig->IsPresentationActive());
	TestFalse(TEXT("failure releases controller ownership"),
		FailureController->HasActivePrologueCarriageForTest());
	TestFalse(TEXT("failure releases owned move ignore"),
		FailureController->IsMoveInputIgnored());
	TestFalse(TEXT("failure releases owned look ignore"),
		FailureController->IsLookInputIgnored());
	TestFalse(TEXT("repeated cancellation remains harmless"),
		FailureRig->CancelPresentation());

	const FGameXXKRuntimeState& RuntimeAfter = Subsystem->GetRuntimeState();
	TestEqual(TEXT("failure preserves player gold"),
		RuntimeAfter.PlayerGold, RuntimeBefore.PlayerGold);
	TestEqual(TEXT("failure preserves player experience"),
		RuntimeAfter.PlayerXP, RuntimeBefore.PlayerXP);
	TestEqual(TEXT("failure preserves inventory entry count"),
		RuntimeAfter.Inventory.Num(), RuntimeBefore.Inventory.Num());
	bool bInventoryMatches = true;
	for (const TPair<FName, int32>& Pair : RuntimeBefore.Inventory)
	{
		bInventoryMatches = bInventoryMatches
			&& RuntimeAfter.Inventory.FindRef(Pair.Key) == Pair.Value;
	}
	TestTrue(TEXT("failure preserves every inventory quantity"),
		bInventoryMatches);
	TestEqual(TEXT("failure preserves ordered formation"),
		RuntimeAfter.CardRun.OrderedFormation.Members,
		RuntimeBefore.CardRun.OrderedFormation.Members);
	TestEqual(TEXT("failure preserves Travel active state"),
		RuntimeAfter.Training.bTravelActive,
		RuntimeBefore.Training.bTravelActive);
	TestEqual(TEXT("failure preserves Travel encounter"),
		RuntimeAfter.Training.ActiveTravelEncounterIndex,
		RuntimeBefore.Training.ActiveTravelEncounterIndex);
	TestEqual(TEXT("failure preserves pending Travel gold"),
		RuntimeAfter.Training.PendingTravelGold,
		RuntimeBefore.Training.PendingTravelGold);
	TestEqual(TEXT("failure preserves pending Travel experience"),
		RuntimeAfter.Training.PendingTravelExperience,
		RuntimeBefore.Training.PendingTravelExperience);
	TestEqual(TEXT("failure preserves story records"),
		RuntimeAfter.NarrativeProgress.StoryProgressById.Num(),
		RuntimeBefore.NarrativeProgress.StoryProgressById.Num());
	TestEqual(TEXT("failure preserves task records"),
		RuntimeAfter.NarrativeProgress.TaskProgressById.Num(),
		RuntimeBefore.NarrativeProgress.TaskProgressById.Num());
	TestEqual(TEXT("failure preserves Guide identity"),
		RuntimeAfter.GuideProgress.ActiveGuideId,
		RuntimeBefore.GuideProgress.ActiveGuideId);
	TestEqual(TEXT("failure preserves Guide step"),
		RuntimeAfter.GuideProgress.ActiveGuideStepId,
		RuntimeBefore.GuideProgress.ActiveGuideStepId);

	return true;
}

#endif
