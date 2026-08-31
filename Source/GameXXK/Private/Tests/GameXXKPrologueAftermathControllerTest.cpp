#include "Town/GameXXKPrologueAftermathController.h"

#include "GameXXKMVPRules.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Engine/GameInstance.h"
#include "Guide/GameXXKGuideAsset.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01SessionSubsystem.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "UI/GameXXKGuidePreferenceWidget.h"
#include "UI/GameXXKPrologueMapWidget.h"
#include "UI/GameXXKPrologueYueBaiWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueAftermathControllerTest,
	"GameXXK.Prologue.Aftermath.Controller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueAftermathControllerTest::RunTest(const FString& Parameters)
{
	const AGameXXKPrologueAftermathController* Defaults =
		GetDefault<AGameXXKPrologueAftermathController>();
	if (!TestNotNull(TEXT("aftermath CDO exists"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("approved YueBai offset from carriage root"),
		Defaults->GetYueBaiRevealOffsetForTest(),
		FVector(250.623f, 666.139f, 0.0f));
	TestEqual(TEXT("building-side statue interaction offset from carriage root"),
		Defaults->GetStatueInteractionOffsetForTest(),
		FVector(3251.408f, -30.0f, 534.289f));
	TestEqual(TEXT("building-side statue interaction radius"),
		Defaults->GetStatueInteractionRadiusForTest(),
		450.0f);
	const UGameXXKInteractableComponent* StatueMetadata =
		Defaults->FindComponentByClass<UGameXXKInteractableComponent>();
	if (TestNotNull(TEXT("statue owns deterministic focus metadata"), StatueMetadata))
	{
		TestEqual(TEXT("statue interaction wins over the legacy town exit"),
			StatueMetadata->GetPriority(),
			100);
		TestFalse(TEXT("statue metadata leaves interaction to its interface"),
			StatueMetadata->IsInteractionEnabled());
	}
	TestEqual(TEXT("guide choice widget class"),
		Defaults->GetGuidePreferenceWidgetClassForTest().Get(),
		UGameXXKGuidePreferenceWidget::StaticClass());
	TestEqual(TEXT("passive statue prompt text"),
		Defaults->GetStatuePromptTextForTest(),
		FText::FromString(TEXT("前往巨大雕像旁按F交互")));
	TestEqual(TEXT("notice dialogue ID"),
		Defaults->GetNoticeDialogueIdForTest(),
		FName(TEXT("Dialogue.Tutorial.CarriageNotice")));
	TestEqual(TEXT("meeting dialogue ID"),
		Defaults->GetMeetingDialogueIdForTest(),
		FName(TEXT("Dialogue.Tutorial.YueBaiFirstMeeting")));
	TestEqual(TEXT("map widget class"),
		Defaults->GetMapWidgetClassForTest().Get(),
		UGameXXKPrologueMapWidget::StaticClass());
	TestEqual(TEXT("intro widget class"),
		Defaults->GetYueBaiWidgetClassForTest().Get(),
		UGameXXKPrologueYueBaiWidget::StaticClass());
	TestTrue(TEXT("carriage option activates aftermath binding"),
		Defaults->ShouldActivateForOptionsForTest(
			TEXT("?GameXXKIntro=CarriagePreview")));
	TestTrue(TEXT("victory return option activates aftermath return"),
		Defaults->ShouldActivateForOptionsForTest(
			TEXT("?GameXXKTutorialReturn=Victory")));
	TestTrue(TEXT("defeat return option activates aftermath return"),
		Defaults->ShouldActivateForOptionsForTest(
			TEXT("?GameXXKTutorialReturn=Defeat")));
	TestFalse(TEXT("ordinary town stays dormant"),
		Defaults->ShouldActivateForOptionsForTest(TEXT("")));
	AGameXXKPrologueAftermathController* VictoryReturnController =
		NewObject<AGameXXKPrologueAftermathController>();
	TestTrue(TEXT("victory return prepares a nonblocking finished state"),
		VictoryReturnController->ApplyTutorialReturnReasonForTest(
			EGameXXKTutorial01ReturnReason::Victory));
	TestEqual(TEXT("victory return state"),
		VictoryReturnController->GetAftermathStateForTest().Phase,
		EGameXXKPrologueAftermathPhase::Finished);
	TestFalse(TEXT("victory return does not reopen statue choice"),
		VictoryReturnController->CanOpenGuideChoiceForTest());
	TestFalse(TEXT("victory return is nonblocking"),
		VictoryReturnController->IsBlockingInputForTest());
	AGameXXKPrologueAftermathController* DefeatReturnController =
		NewObject<AGameXXKPrologueAftermathController>();
	TestTrue(TEXT("defeat return restores statue prompt state"),
		DefeatReturnController->ApplyTutorialReturnReasonForTest(
			EGameXXKTutorial01ReturnReason::Defeat));
	TestEqual(TEXT("defeat return state"),
		DefeatReturnController->GetAftermathStateForTest().Phase,
		EGameXXKPrologueAftermathPhase::StatuePrompt);
	TestTrue(TEXT("defeat return reopens statue choice"),
		DefeatReturnController->CanOpenGuideChoiceForTest());
	TestFalse(TEXT("defeat return remains nonblocking"),
		DefeatReturnController->IsBlockingInputForTest());

	AGameXXKPrologueAftermathController* Controller =
		NewObject<AGameXXKPrologueAftermathController>();
	TestTrue(TEXT("test aftermath starts"), Controller->StartRulesForTest());
	TestEqual(TEXT("test aftermath starts at notice"),
		Controller->GetAftermathStateForTest().Phase,
		EGameXXKPrologueAftermathPhase::HeroNotice);
	TestTrue(TEXT("notice outcome opens map card"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::DialogueCompleted));
	TestTrue(TEXT("map opens inspection"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::OpenInspection));
	TestFalse(TEXT("inspection consumes continue"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::ContinuePressed));
	TestTrue(TEXT("inspection closes"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::CloseInspection));
	TestTrue(TEXT("map continues to intro"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::ContinuePressed));
	TestTrue(TEXT("intro completes"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted));
	TestTrue(TEXT("guide boundary is explicit"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::GuideDialogueStarted));
	TestTrue(TEXT("meeting dialogue completes"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::DialogueCompleted));
	TestTrue(TEXT("follower activates statue prompt"),
		Controller->ApplyEventForTest(
			EGameXXKPrologueAftermathEvent::FollowerActivated));
	TestEqual(TEXT("statue prompt is nonblocking"),
		Controller->IsBlockingInputForTest(),
		false);
	TestTrue(TEXT("only statue prompt can open guide choice"),
		Controller->CanOpenGuideChoiceForTest());
	UGameInstance* TutorialGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* TutorialRuntime =
		NewObject<UGameXXKMVPSubsystem>(TutorialGameInstance);
	UGameXXKTutorial01SessionSubsystem* ExperiencedSession =
		NewObject<UGameXXKTutorial01SessionSubsystem>(TutorialGameInstance);
	TutorialRuntime->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	TutorialRuntime->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FTransform TutorialReturn(
		FRotator::ZeroRotator,
		FVector(19930.0f, 5240.0f, 1610.0f));
	TestTrue(TEXT("experienced choice prepares tutorial 0-1 travel"),
		Controller->PrepareTutorial01TravelForTest(
			EGameXXKGuidePreference::ExperiencedPlayer,
			TutorialRuntime,
			ExperiencedSession,
			TutorialReturn));
	TestEqual(TEXT("experienced choice enters the guarded travel phase"),
		Controller->GetAftermathStateForTest().Phase,
		EGameXXKPrologueAftermathPhase::TutorialTravelPending);
	TestEqual(TEXT("experienced choice remains available to the tutorial map"),
		ExperiencedSession->GetGuidePreference(),
		EGameXXKGuidePreference::ExperiencedPlayer);
	TestFalse(TEXT("pending travel rejects a second button click"),
		Controller->PrepareTutorial01TravelForTest(
			EGameXXKGuidePreference::NewPlayer,
			TutorialRuntime,
			ExperiencedSession,
			TutorialReturn));

	AGameXXKPrologueAftermathController* NewPlayerController =
		NewObject<AGameXXKPrologueAftermathController>();
	TestTrue(TEXT("new-player fixture starts"), NewPlayerController->StartRulesForTest());
	TestTrue(TEXT("new-player notice completes"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::DialogueCompleted));
	TestTrue(TEXT("new-player map continues"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::ContinuePressed));
	TestTrue(TEXT("new-player intro completes"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted));
	TestTrue(TEXT("new-player guide begins"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::GuideDialogueStarted));
	TestTrue(TEXT("new-player dialogue completes"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::DialogueCompleted));
	TestTrue(TEXT("new-player follower activates"), NewPlayerController->ApplyEventForTest(EGameXXKPrologueAftermathEvent::FollowerActivated));
	UGameXXKTutorial01SessionSubsystem* NewPlayerSession =
		NewObject<UGameXXKTutorial01SessionSubsystem>(TutorialGameInstance);
	TestTrue(TEXT("new-player choice prepares the same tutorial 0-1 travel"),
		NewPlayerController->PrepareTutorial01TravelForTest(
			EGameXXKGuidePreference::NewPlayer,
			TutorialRuntime,
			NewPlayerSession,
			TutorialReturn));
	TestEqual(TEXT("new-player choice enables the local guide preference"),
		NewPlayerSession->GetGuidePreference(),
		EGameXXKGuidePreference::NewPlayer);
	AGameXXKPrologueAftermathController* DormantController =
		NewObject<AGameXXKPrologueAftermathController>();
	TestFalse(TEXT("ordinary town cannot open guide choice"),
		DormantController->CanOpenGuideChoiceForTest());

	AGameXXKMVPPlayerController* PlayerController =
		NewObject<AGameXXKMVPPlayerController>();
	AGameXXKPrologueAftermathController* OwnedAftermath =
		NewObject<AGameXXKPrologueAftermathController>();
	TestTrue(TEXT("blocking aftermath acquires its independent input token"),
		PlayerController->BeginPrologueAftermathPresentation(OwnedAftermath));
	TestTrue(TEXT("aftermath token is observable"),
		PlayerController->IsPrologueAftermathInputLockedForTest());
	TestTrue(TEXT("aftermath token locks movement while blocking"),
		PlayerController->IsMoveInputIgnored());
	TestTrue(TEXT("aftermath token locks look while blocking"),
		PlayerController->IsLookInputIgnored());
	PlayerController->EndPrologueAftermathPresentation(OwnedAftermath);
	TestFalse(TEXT("following release clears only the aftermath token"),
		PlayerController->IsPrologueAftermathInputLockedForTest());
	TestFalse(TEXT("following release restores movement"),
		PlayerController->IsMoveInputIgnored());
	TestFalse(TEXT("following release restores look"),
		PlayerController->IsLookInputIgnored());

	AGameXXKMVPPlayerController* PreIgnoredController =
		NewObject<AGameXXKMVPPlayerController>();
	AGameXXKPrologueAftermathController* PreIgnoredAftermath =
		NewObject<AGameXXKPrologueAftermathController>();
	PreIgnoredController->SetIgnoreMoveInput(true);
	PreIgnoredController->SetIgnoreLookInput(true);
	TestTrue(TEXT("aftermath may borrow an already locked controller"),
		PreIgnoredController->BeginPrologueAftermathPresentation(PreIgnoredAftermath));
	PreIgnoredController->EndPrologueAftermathPresentation(PreIgnoredAftermath);
	TestTrue(TEXT("release preserves a pre-existing movement lock"),
		PreIgnoredController->IsMoveInputIgnored());
	TestTrue(TEXT("release preserves a pre-existing look lock"),
		PreIgnoredController->IsLookInputIgnored());

	AGameXXKTownNpcCharacter* YueBai = NewObject<AGameXXKTownNpcCharacter>();
	YueBai->SetNpcId(TEXT("Npc.YueBai"));
	AGameXXKTownNpcCharacter* Other = NewObject<AGameXXKTownNpcCharacter>();
	Other->SetNpcId(TEXT("Npc.JinGui"));
	TestTrue(TEXT("unique resolver finds YueBai"),
		AGameXXKPrologueAftermathController::FindUniqueYueBaiForTest(
			{Other, YueBai}) == YueBai);
	TestNull(TEXT("duplicate YueBai rejects"),
		AGameXXKPrologueAftermathController::FindUniqueYueBaiForTest(
			{YueBai, YueBai}));

	return true;
}

#endif
