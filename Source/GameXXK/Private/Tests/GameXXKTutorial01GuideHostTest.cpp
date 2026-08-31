#include "Misc/AutomationTest.h"

#include "Components/TextBlock.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "Guide/GameXXKTutorial01GuideHost.h"
#include "UI/GameXXKGuideOverlayWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTutorial01GuideHostTestPrivate
{
	UGameXXKGuideAsset* LoadTutorialGuide()
	{
		return LoadObject<UGameXXKGuideAsset>(
			nullptr,
			TEXT("/Game/GameXXK/Narrative/Guides/DA_Guide_Battle_Tutorial01_NewPlayer.DA_Guide_Battle_Tutorial01_NewPlayer"));
	}

	void RegisterAllTargets(
		FGameXXKGuideTargetRegistry& Registry,
		TArray<TObjectPtr<UTextBlock>>& Widgets)
	{
		const TArray<FName> TargetIds = {
			TEXT("Battle.Hud.PartyQi"),
			TEXT("Battle.Unit.Hero.Health"),
			TEXT("Battle.Unit.Hero.Mana"),
			TEXT("Battle.Enemy.Intent"),
			TEXT("Battle.Hand.HengJianShouShi"),
			TEXT("Battle.Hand.SuiYanJi"),
			TEXT("Battle.Hand.FengShenBu"),
			TEXT("Battle.Unit.Hero.Target"),
			TEXT("Battle.Unit.Enemy.Target"),
			TEXT("Battle.Unit.YueBai.Visual"),
			TEXT("Battle.Pending.ForcedDiscard"),
			TEXT("Battle.EndTurn"),
			TEXT("Battle.AutoBattle")};
		for (int32 Index = 0; Index < TargetIds.Num(); ++Index)
		{
			UTextBlock* Widget = NewObject<UTextBlock>();
			Widgets.Add(Widget);
			const FSlateRect Rect(
				40.0f + Index * 80.0f,
				60.0f + Index * 10.0f,
				100.0f + Index * 80.0f,
				120.0f + Index * 10.0f);
			Registry.RegisterTarget(
				TargetIds[Index],
				Widget,
				[Rect](const UWidget& HostWidget, FSlateRect& OutRect)
				{
					(void)HostWidget;
					OutRect = Rect;
					return true;
				});
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01GuideHostExactChainTest,
	"GameXXK.Tutorial01.GuideHost.ExactNineStepChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01GuideHostExactChainTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTutorial01GuideHostTestPrivate;
	UGameXXKGuideAsset* Asset = LoadTutorialGuide();
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideTargetRegistry Registry;
	TArray<TObjectPtr<UTextBlock>> TargetWidgets;
	RegisterAllTargets(Registry, TargetWidgets);
	FGameXXKGuideProgress Progress;
	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKTutorial01GuideHost* Host = NewObject<UGameXXKTutorial01GuideHost>();
	int32 FailureCount = 0;
	Host->Bind(
		Progress,
		Registry,
		*Overlay,
		*Asset,
		FGameXXKTutorial01GuideFailed::CreateLambda(
			[&FailureCount](const FString& Diagnostic) { ++FailureCount; }));

	TestTrue(TEXT("new-player guide host starts"), Host->Start());
	TestEqual(TEXT("guide starts at Qi"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.Qi")));
	TestTrue(TEXT("Qi permits Space continue"),
		Registry.IsActionAllowed(TEXT("Action.Guide.Continue")));
	TestFalse(TEXT("Qi blocks card input"),
		Registry.IsActionAllowed(TEXT("Action.Battle.SelectCard.HengJianShouShi")));

	TestTrue(TEXT("first Space advances to combined vitals"), Host->HandleContinue());
	TestEqual(TEXT("combined vitals step active"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.HeroVitals")));
	TestTrue(TEXT("second Space advances to intent"), Host->HandleContinue());
	TestEqual(TEXT("enemy intent step active"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.EnemyIntent")));
	TestTrue(TEXT("third Space advances to HengJian"), Host->HandleContinue());
	TestEqual(TEXT("HengJian step active"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.HengJian")));
	TestFalse(TEXT("Space cannot skip the first action step"), Host->HandleContinue());

	Registry.EmitEvent(TEXT("Event.Tutorial01.SuiYanResolved"));
	TestEqual(TEXT("wrong event cannot advance HengJian"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.HengJian")));
	Registry.EmitEvent(TEXT("Event.Tutorial01.HengJianResolved"));
	TestEqual(TEXT("HengJian advances to SuiYan"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.SuiYan")));
	Registry.EmitEvent(TEXT("Event.Tutorial01.SuiYanResolved"));
	TestEqual(TEXT("SuiYan advances to FengShen"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.FengShen")));
	Registry.EmitEvent(TEXT("Event.Tutorial01.FengShenForcedDiscardOpened"));
	TestEqual(TEXT("FengShen advances to forced discard"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.ForcedDiscard")));
	Registry.EmitEvent(TEXT("Event.Tutorial01.ForcedDiscardResolved"));
	TestEqual(TEXT("discard advances to EndTurn"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.EndTurn")));

	Registry.EmitEvent(TEXT("Event.Battle.EndTurnResolved"));
	TestTrue(TEXT("EndTurn suspends guide through enemy phase"),
		Host->IsSuspendedForEnemyTurnForTest());
	TestFalse(TEXT("suspension releases the action gate"), Registry.HasActionGate());
	TestFalse(TEXT("suspension dismisses the overlay"), Overlay->IsGuideVisibleForTest());
	Host->Tick(30.0f, false, false, EGameXXKCardBattlePhase::Enemy);
	TestEqual(TEXT("normal enemy phase never expires watchdog"), FailureCount, 0);

	Registry.EmitEvent(TEXT("Event.Tutorial01.PlayerTurnReady"));
	TestFalse(TEXT("next player turn resumes guide"),
		Host->IsSuspendedForEnemyTurnForTest());
	TestEqual(TEXT("AutoBattle step active"), Progress.ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.AutoBattle")));
	TestTrue(TEXT("AutoBattle action is exclusively allowed"),
		Registry.IsActionAllowed(TEXT("Action.Battle.EnableAuto")));
	Registry.EmitEvent(TEXT("Event.Tutorial01.AutoBattleEnabled"));
	TestTrue(TEXT("AutoBattle completion clears active guide"),
		Progress.ActiveGuideId.IsNone());
	TestFalse(TEXT("completed guide releases gate"), Registry.HasActionGate());
	TestFalse(TEXT("completed guide dismisses overlay"), Overlay->IsGuideVisibleForTest());
	TestEqual(TEXT("successful chain emits no failure"), FailureCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01GuideHostFailureReleaseTest,
	"GameXXK.Tutorial01.GuideHost.FailureAndExperiencedRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01GuideHostFailureReleaseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTutorial01GuideHostTestPrivate;
	UGameXXKGuideAsset* Asset = LoadTutorialGuide();
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();

	FGameXXKGuideTargetRegistry MissingRegistry;
	FGameXXKGuideProgress MissingProgress;
	MissingProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKTutorial01GuideHost* MissingHost = NewObject<UGameXXKTutorial01GuideHost>();
	int32 MissingFailureCount = 0;
	MissingHost->Bind(
		MissingProgress,
		MissingRegistry,
		*Overlay,
		*Asset,
		FGameXXKTutorial01GuideFailed::CreateLambda(
			[&MissingFailureCount](const FString& Diagnostic)
			{
				++MissingFailureCount;
			}));
	TestFalse(TEXT("missing abort target fails start"), MissingHost->Start());
	TestEqual(TEXT("missing target reports failure once"), MissingFailureCount, 1);
	TestFalse(TEXT("missing target leaves no action gate"), MissingRegistry.HasActionGate());

	FGameXXKGuideTargetRegistry WatchdogRegistry;
	TArray<TObjectPtr<UTextBlock>> TargetWidgets;
	RegisterAllTargets(WatchdogRegistry, TargetWidgets);
	FGameXXKGuideProgress WatchdogProgress;
	WatchdogProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKTutorial01GuideHost* WatchdogHost = NewObject<UGameXXKTutorial01GuideHost>();
	int32 WatchdogFailureCount = 0;
	WatchdogHost->Bind(
		WatchdogProgress,
		WatchdogRegistry,
		*Overlay,
		*Asset,
		FGameXXKTutorial01GuideFailed::CreateLambda(
			[&WatchdogFailureCount](const FString& Diagnostic)
			{
				++WatchdogFailureCount;
			}));
	TestTrue(TEXT("watchdog fixture starts"), WatchdogHost->Start());
	WatchdogHost->HandleContinue();
	WatchdogHost->HandleContinue();
	WatchdogHost->HandleContinue();
	WatchdogRegistry.EmitEvent(TEXT("Event.Tutorial01.HengJianResolved"));
	WatchdogRegistry.EmitEvent(TEXT("Event.Tutorial01.SuiYanResolved"));
	WatchdogRegistry.EmitEvent(TEXT("Event.Tutorial01.FengShenForcedDiscardOpened"));
	WatchdogRegistry.EmitEvent(TEXT("Event.Tutorial01.ForcedDiscardResolved"));
	WatchdogRegistry.EmitEvent(TEXT("Event.Battle.EndTurnResolved"));
	WatchdogHost->Tick(8.0f, false, false, EGameXXKCardBattlePhase::Player);
	WatchdogHost->Tick(8.0f, false, false, EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("stalled player-turn recovery fails once"), WatchdogFailureCount, 1);
	TestFalse(TEXT("watchdog failure releases gate"), WatchdogRegistry.HasActionGate());

	FGameXXKGuideTargetRegistry ExperiencedRegistry;
	TArray<TObjectPtr<UTextBlock>> ExperiencedTargets;
	RegisterAllTargets(ExperiencedRegistry, ExperiencedTargets);
	FGameXXKGuideProgress ExperiencedProgress;
	ExperiencedProgress.Preference = EGameXXKGuidePreference::ExperiencedPlayer;
	UGameXXKTutorial01GuideHost* ExperiencedHost = NewObject<UGameXXKTutorial01GuideHost>();
	ExperiencedHost->Bind(
		ExperiencedProgress,
		ExperiencedRegistry,
		*Overlay,
		*Asset,
		FGameXXKTutorial01GuideFailed());
	TestFalse(TEXT("experienced player creates no guide session"), ExperiencedHost->Start());
	TestFalse(TEXT("experienced player has no action gate"), ExperiencedRegistry.HasActionGate());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01GuideHostLateGeometryTest,
	"GameXXK.Tutorial01.GuideHost.LateGeometryRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01GuideHostLateGeometryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTutorial01GuideHostTestPrivate;
	UGameXXKGuideAsset* Asset = LoadTutorialGuide();
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideTargetRegistry Registry;
	TArray<TObjectPtr<UTextBlock>> Widgets;
	bool bGeometryReady = false;
	for (const FName TargetId : {
		FName(TEXT("Battle.Hud.PartyQi")),
		FName(TEXT("Battle.Unit.YueBai.Visual"))})
	{
		UTextBlock* Widget = NewObject<UTextBlock>();
		Widgets.Add(Widget);
		Registry.RegisterTarget(
			TargetId,
			Widget,
			[&bGeometryReady](const UWidget& HostWidget, FSlateRect& OutRect)
			{
				(void)HostWidget;
				if (!bGeometryReady)
				{
					return false;
				}
				OutRect = FSlateRect(100.0f, 100.0f, 220.0f, 180.0f);
				return true;
			});
	}
	FGameXXKGuideProgress Progress;
	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	UGameXXKTutorial01GuideHost* Host = NewObject<UGameXXKTutorial01GuideHost>();
	Host->Bind(
		Progress,
		Registry,
		*Overlay,
		*Asset,
		FGameXXKTutorial01GuideFailed());
	TestTrue(TEXT("registered zero-geometry targets keep guide resumable"), Host->Start());
	TestFalse(TEXT("overlay waits while geometry is zero"), Overlay->IsGuideVisibleForTest());
	bGeometryReady = true;
	Host->Tick(0.016f, false, false, EGameXXKCardBattlePhase::Player);
	TestTrue(TEXT("next tick presents guide after geometry settles"),
		Overlay->IsGuideVisibleForTest());
	Host->Cancel();
	return true;
}

#endif
