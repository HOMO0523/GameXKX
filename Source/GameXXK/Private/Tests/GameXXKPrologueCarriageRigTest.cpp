#include "Town/GameXXKPrologueCarriageRig.h"

#include "Components/WidgetComponent.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "Prologue/GameXXKPrologueCarriageTypes.h"
#include "UI/GameXXKPrologueCarriageWidget.h"

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
	TestEqual(TEXT("carriage sorts one layer behind the town hero"),
		Defaults->GetCarriageSortPriorityForTest(), 9);
	TestEqual(TEXT("carriage canvas matches authored five-twelve cells"),
		Defaults->GetCarriageDrawSizeForTest(),
		FVector2D(512.0f, 512.0f));
	TestEqual(TEXT("carriage uses bottom-center world pivot"),
		Defaults->GetCarriagePivotForTest(),
		FVector2D(0.5f, 1.0f));
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

	return true;
}

#endif
