#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Guide/GameXXKGuideAsset.h"
#include "Guide/GameXXKGuideCoordinator.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "UI/GameXXKGuidePreferenceWidget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKGuideWidgetTestPrivate
{
	UGameXXKGuideAsset* MakeGuide()
	{
		UGameXXKGuideAsset* Asset = NewObject<UGameXXKGuideAsset>();
		Asset->GuideId = TEXT("Guide.Test.Widget");
		Asset->GuideVersion = 1;
		Asset->EntryStepId = TEXT("forced");

		FGameXXKGuideStepDefinition Forced;
		Forced.StepId = TEXT("forced");
		Forced.TriggerEventId = TEXT("Event.Route.Opened");
		Forced.TargetId = TEXT("Route.Tutorial.NextNode");
		Forced.InputPolicy = EGameXXKGuideInputPolicy::Forced;
		Forced.Text = FText::FromString(TEXT("选择节点"));
		Forced.AllowedActionIds = {TEXT("Action.Route.SelectNext")};
		Forced.CompletionEventId = TEXT("Event.Route.NextNodeSelected");
		Forced.NextStepId = TEXT("soft");
		Asset->Steps.Add(Forced);

		FGameXXKGuideStepDefinition Soft;
		Soft.StepId = TEXT("soft");
		Soft.TriggerEventId = TEXT("Event.Battle.Opened");
		Soft.TargetId = TEXT("Battle.Hud.PartyQi");
		Soft.InputPolicy = EGameXXKGuideInputPolicy::Soft;
		Soft.Text = FText::FromString(TEXT("观察气力"));
		Soft.CompletionEventId = TEXT("Event.Guide.Done");
		Asset->Steps.Add(Soft);
		return Asset;
	}

	void RegisterTarget(
		FGameXXKGuideTargetRegistry& Registry,
		const FName TargetId,
		UTextBlock* Widget,
		const FSlateRect Rect)
	{
		Registry.RegisterTarget(
			TargetId,
			Widget,
			[Rect](FSlateRect& OutRect)
			{
				OutRect = Rect;
				return true;
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuidePreferencePromptTest,
	"GameXXK.Guide.Widget.PreferencePrompt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuidePreferencePromptTest::RunTest(const FString& Parameters)
{
	UGameXXKGuidePreferenceWidget* Widget = NewObject<UGameXXKGuidePreferenceWidget>();
	Widget->TakeWidget();
	FGameXXKGuideProgress Progress;
	Widget->RefreshFromProgress(Progress);
	TestTrue(TEXT("unset preference shows prompt"), Widget->IsPromptVisibleForTest());
	TestEqual(TEXT("experienced button copy is exact"),
		Widget->GetExperiencedButtonTextForTest().ToString(), FString(TEXT("我是老玩家，跳过")));
	TestEqual(TEXT("new-player button copy is exact"),
		Widget->GetNewPlayerButtonTextForTest().ToString(), FString(TEXT("我是新手，继续")));

	EGameXXKGuidePreference Selected = EGameXXKGuidePreference::Unset;
	Widget->SetPreferenceChosenDelegate(FGameXXKGuidePreferenceChosen::CreateLambda(
		[&Selected](const EGameXXKGuidePreference Preference)
		{
			Selected = Preference;
		}));
	Widget->ChooseExperiencedPlayerForTest();
	TestEqual(TEXT("experienced choice emits preference"), Selected, EGameXXKGuidePreference::ExperiencedPlayer);
	Widget->ChooseNewPlayerForTest();
	TestEqual(TEXT("new-player choice emits preference"), Selected, EGameXXKGuidePreference::NewPlayer);

	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	Widget->RefreshFromProgress(Progress);
	TestFalse(TEXT("resolved preference hides prompt"), Widget->IsPromptVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideCoordinatorInputTokenTest,
	"GameXXK.Guide.Widget.CoordinatorInputToken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideCoordinatorInputTokenTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideWidgetTestPrivate;
	UGameXXKGuideAsset* Asset = MakeGuide();
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideTargetRegistry Registry;
	UTextBlock* RouteTarget = NewObject<UTextBlock>();
	UTextBlock* QiTarget = NewObject<UTextBlock>();
	RegisterTarget(Registry, TEXT("Route.Tutorial.NextNode"), RouteTarget, FSlateRect(10, 20, 110, 70));
	RegisterTarget(Registry, TEXT("Battle.Hud.PartyQi"), QiTarget, FSlateRect(200, 30, 380, 70));

	FGameXXKGuideProgress Progress;
	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKGuideCoordinator* Coordinator = NewObject<UGameXXKGuideCoordinator>();
	Coordinator->Bind(Progress, Registry, Overlay);
	int32 PersistCount = 0;
	Coordinator->SetPersistenceDelegate(FGameXXKGuidePersistenceDelegate::CreateLambda(
		[&PersistCount](const FGameXXKGuideProgress& Candidate)
		{
			++PersistCount;
			return true;
		}));

	FString Error;
	TestTrue(FString::Printf(TEXT("forced guide starts: %s"), *Error),
		Coordinator->StartGuide(*Asset, TEXT("Event.Route.Opened"), &Error));
	TestTrue(TEXT("forced guide acquires input token"), Coordinator->IsInputTokenHeld());
	TestEqual(TEXT("input token acquired once"), Coordinator->GetInputTokenAcquisitionCountForTest(), 1);
	TestTrue(TEXT("overlay visible"), Overlay->IsGuideVisibleForTest());
	TestTrue(TEXT("forced overlay blocks input"), Overlay->IsBlockingInputForTest());
	TestTrue(TEXT("forced action whitelist passes"), Coordinator->CanExecuteAction(TEXT("Action.Route.SelectNext")));
	TestFalse(TEXT("forced unrelated action rejects"), Coordinator->CanExecuteAction(TEXT("Action.Unrelated")));
	TestFalse(TEXT("starting a second guide cannot stack tokens"),
		Coordinator->StartGuide(*Asset, TEXT("Event.Route.Opened"), &Error));
	TestEqual(TEXT("failed restart does not acquire another token"),
		Coordinator->GetInputTokenAcquisitionCountForTest(), 1);

	TestTrue(TEXT("completion transitions to soft guide"),
		Coordinator->HandleEvent(TEXT("Event.Route.NextNodeSelected"), &Error));
	TestFalse(TEXT("soft guide releases input token"), Coordinator->IsInputTokenHeld());
	TestFalse(TEXT("soft overlay never blocks"), Overlay->IsBlockingInputForTest());
	TestTrue(TEXT("soft guide allows any action"), Coordinator->CanExecuteAction(TEXT("Action.Unrelated")));
	TestTrue(TEXT("guide mutations persisted"), PersistCount >= 2);

	Coordinator->CancelForMapTravel();
	TestFalse(TEXT("map travel keeps token released"), Coordinator->IsInputTokenHeld());
	TestFalse(TEXT("map travel dismisses overlay"), Overlay->IsGuideVisibleForTest());
	TestTrue(TEXT("map travel clears active guide"), Progress.ActiveGuideId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideCoordinatorMissingTargetAndDestroyTest,
	"GameXXK.Guide.Widget.MissingTargetAndDestroyRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideCoordinatorMissingTargetAndDestroyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideWidgetTestPrivate;
	UGameXXKGuideAsset* Asset = MakeGuide();
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideTargetRegistry EmptyRegistry;
	FGameXXKGuideProgress MissingProgress;
	MissingProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKGuideCoordinator* MissingCoordinator = NewObject<UGameXXKGuideCoordinator>();
	MissingCoordinator->Bind(MissingProgress, EmptyRegistry, Overlay);
	FString Error;
	TestTrue(TEXT("missing forced target is handled instead of failing closed"),
		MissingCoordinator->StartGuide(*Asset, TEXT("Event.Route.Opened"), &Error));
	TestFalse(TEXT("missing target never leaves input token held"), MissingCoordinator->IsInputTokenHeld());
	TestTrue(TEXT("missing forced step records diagnostic"),
		MissingProgress.LastDiagnostic.Contains(TEXT("Route.Tutorial.NextNode")));

	FGameXXKGuideTargetRegistry LiveRegistry;
	UTextBlock* RouteTarget = NewObject<UTextBlock>();
	UTextBlock* QiTarget = NewObject<UTextBlock>();
	RegisterTarget(LiveRegistry, TEXT("Route.Tutorial.NextNode"), RouteTarget, FSlateRect(1, 1, 20, 20));
	RegisterTarget(LiveRegistry, TEXT("Battle.Hud.PartyQi"), QiTarget, FSlateRect(30, 1, 60, 20));
	FGameXXKGuideProgress DestroyProgress;
	DestroyProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKGuideCoordinator* DestroyCoordinator = NewObject<UGameXXKGuideCoordinator>();
	DestroyCoordinator->Bind(DestroyProgress, LiveRegistry, Overlay);
	TestTrue(TEXT("second fixture starts forced guide"),
		DestroyCoordinator->StartGuide(*Asset, TEXT("Event.Route.Opened"), &Error));
	TestTrue(TEXT("second fixture holds token"), DestroyCoordinator->IsInputTokenHeld());
	DestroyCoordinator->NotifyOverlayDestroyed();
	TestFalse(TEXT("overlay destruction releases token"), DestroyCoordinator->IsInputTokenHeld());
	TestFalse(TEXT("overlay destruction preserves resumable guide progress"), DestroyProgress.ActiveGuideId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuidePreferencePersistenceTest,
	"GameXXK.Guide.Widget.PreferencePersistenceAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuidePreferencePersistenceTest::RunTest(const FString& Parameters)
{
	FGameXXKGuideProgress Progress;
	FGameXXKGuideTargetRegistry Registry;
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	UGameXXKGuideCoordinator* Coordinator = NewObject<UGameXXKGuideCoordinator>();
	Coordinator->Bind(Progress, Registry, Overlay);
	int32 PersistCount = 0;
	Coordinator->SetPersistenceDelegate(FGameXXKGuidePersistenceDelegate::CreateLambda(
		[&PersistCount](const FGameXXKGuideProgress& Candidate)
		{
			++PersistCount;
			return true;
		}));
	FString Error;
	TestTrue(TEXT("experienced preference persists"),
		Coordinator->ApplyPreference(EGameXXKGuidePreference::ExperiencedPlayer, &Error));
	TestEqual(TEXT("experienced preference stored"), Progress.Preference, EGameXXKGuidePreference::ExperiencedPlayer);
	TestFalse(TEXT("experienced player never starts guide"),
		Coordinator->StartGuide(*GameXXKGuideWidgetTestPrivate::MakeGuide(), TEXT("Event.Route.Opened"), &Error));

	Progress.ActiveGuideId = TEXT("Guide.Stale");
	Progress.ActiveGuideStepId = TEXT("Step.Stale");
	Progress.CompletedGuideStepIds = {TEXT("Step.Done")};
	TestTrue(TEXT("combat guide reset persists"), Coordinator->ResetCombatGuide(&Error));
	TestEqual(TEXT("reset returns preference to unset"), Progress.Preference, EGameXXKGuidePreference::Unset);
	TestTrue(TEXT("reset clears active guide"), Progress.ActiveGuideId.IsNone());
	TestTrue(TEXT("reset clears completed guide steps"), Progress.CompletedGuideStepIds.IsEmpty());
	TestEqual(TEXT("preference and reset each saved"), PersistCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideWorkbenchSettingsPersistenceTest,
	"GameXXK.Guide.Widget.WorkbenchSettingsPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideWorkbenchSettingsPersistenceTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	if (!TestTrue(TEXT("workbench guide fixture starts"),
		Subsystem && Subsystem->StartGame() && Subsystem->BeginTutorialQuest() && Workbench))
	{
		return false;
	}
	int32 SaveCount = 0;
	Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&SaveCount](USaveGame* SaveGame, const FString& SlotName, const int32 UserIndex)
		{
			++SaveCount;
			return SaveGame != nullptr;
		}));
	Subsystem->GetMutableRuntimeState().Training.PendingTravelGold = 777;
	const FName TaskId(TEXT("Task.Main.XuXiake.Prologue"));
	const FName TaskStepBefore =
		Subsystem->GetRuntimeState().NarrativeProgress.TaskProgressById.FindChecked(TaskId).CurrentStepId;

	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	if (!TestTrue(TEXT("workbench opens backpack"),
		Workbench->OpenWorkbench() && Workbench->OpenBackpack()))
	{
		return false;
	}
	TestTrue(TEXT("started tutorial with unset preference shows first-entry prompt"),
		Workbench->IsGuidePreferencePromptVisibleForTest());
	UGameXXKGuidePreferenceWidget* PreferenceWidget = Workbench->WidgetTree
		? Cast<UGameXXKGuidePreferenceWidget>(
			Workbench->WidgetTree->FindWidget(TEXT("DesktopGuidePreference")))
		: nullptr;
	if (!TestNotNull(TEXT("workbench owns preference widget"), PreferenceWidget))
	{
		return false;
	}
	PreferenceWidget->ChooseNewPlayerForTest();
	TestEqual(TEXT("new-player selection updates save-authoritative preference"),
		Subsystem->GetRuntimeState().GuideProgress.Preference,
		EGameXXKGuidePreference::NewPlayer);
	TestEqual(TEXT("new-player selection saves immediately"), SaveCount, 1);

	UGameXXKDesktopTrainingActionButton* SettingsButton = Workbench->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Workbench->WidgetTree->FindWidget(TEXT("TopToolbarSettings")))
		: nullptr;
	if (!TestNotNull(TEXT("workbench owns settings button"), SettingsButton))
	{
		return false;
	}
	SettingsButton->OnClicked.Broadcast();
	Workbench->TickForTest(0.0f);
	UGameXXKDesktopTrainingActionButton* ResetButton = Workbench->WidgetTree
		? Cast<UGameXXKDesktopTrainingActionButton>(
			Workbench->WidgetTree->FindWidget(TEXT("ResetCombatGuideButton")))
		: nullptr;
	if (!TestNotNull(TEXT("settings exposes reset combat guide"), ResetButton))
	{
		return false;
	}
	TestTrue(TEXT("test facade sees reset button"), Workbench->HasResetCombatGuideButtonForTest());
	ResetButton->OnClicked.Broadcast();
	TestEqual(TEXT("reset returns preference to unset"),
		Subsystem->GetRuntimeState().GuideProgress.Preference,
		EGameXXKGuidePreference::Unset);
	TestEqual(TEXT("reset saves immediately"), SaveCount, 2);
	TestEqual(TEXT("reset preserves pending route reward gold"),
		Subsystem->GetRuntimeState().Training.PendingTravelGold,
		777);
	TestEqual(TEXT("reset preserves narrative task step"),
		Subsystem->GetRuntimeState().NarrativeProgress.TaskProgressById.FindChecked(TaskId).CurrentStepId,
		TaskStepBefore);

	FGameXXKGuideProgress NewPlayerAgain = Subsystem->GetRuntimeState().GuideProgress;
	NewPlayerAgain.Preference = EGameXXKGuidePreference::NewPlayer;
	TestTrue(TEXT("failure fixture restores new-player preference"),
		Subsystem->CommitGuideProgress(NewPlayerAgain));
	Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[](USaveGame* SaveGame, const FString& SlotName, const int32 UserIndex)
		{
			return false;
		}));
	TestFalse(TEXT("failed immediate save rejects reset"), Workbench->ResetCombatGuideForTest());
	TestEqual(TEXT("failed save rolls preference back"),
		Subsystem->GetRuntimeState().GuideProgress.Preference,
		EGameXXKGuidePreference::NewPlayer);
	return true;
}

#endif
