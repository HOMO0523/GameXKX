#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "Components/TextBlock.h"
#include "Guide/GameXXKGuideAsset.h"
#include "Guide/GameXXKGuideRules.h"
#include "Guide/GameXXKGuideTargetRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKGuideRulesTestPrivate
{
	UGameXXKGuideAsset* MakeGuide(const FName GuideId = TEXT("Guide.Test.Route"))
	{
		UGameXXKGuideAsset* Asset = NewObject<UGameXXKGuideAsset>();
		Asset->GuideId = GuideId;
		Asset->GuideVersion = 1;
		Asset->EntryStepId = TEXT("forced");

		FGameXXKGuideStepDefinition Forced;
		Forced.StepId = TEXT("forced");
		Forced.TriggerEventId = TEXT("Event.Route.Opened");
		Forced.TargetId = TEXT("Route.Tutorial.NextNode");
		Forced.InputPolicy = EGameXXKGuideInputPolicy::Forced;
		Forced.Text = FText::FromString(TEXT("选择下一个节点"));
		Forced.AllowedActionIds = {TEXT("Action.Route.SelectNext")};
		Forced.CompletionEventId = TEXT("Event.Route.NextNodeSelected");
		Forced.NextStepId = TEXT("soft");
		Asset->Steps.Add(Forced);

		FGameXXKGuideStepDefinition Soft;
		Soft.StepId = TEXT("soft");
		Soft.TriggerEventId = TEXT("Event.Battle.Opened");
		Soft.TargetId = TEXT("Battle.Hud.PartyQi");
		Soft.InputPolicy = EGameXXKGuideInputPolicy::Soft;
		Soft.Text = FText::FromString(TEXT("观察全队气力"));
		Soft.CompletionEventId = TEXT("Event.Guide.Done");
		Asset->Steps.Add(Soft);
		return Asset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideAssetContractTest,
	"GameXXK.Guide.Core.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideAssetContractTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideRulesTestPrivate;
	UGameXXKGuideAsset* Asset = MakeGuide();
	TestNotNull(TEXT("entry guide step resolves"), Asset->FindStep(TEXT("forced")));
	TestNull(TEXT("missing guide step rejects"), Asset->FindStep(TEXT("missing")));

#if WITH_EDITOR
	FDataValidationContext ValidContext;
	TestEqual(TEXT("canonical guide validates"), Asset->IsDataValid(ValidContext), EDataValidationResult::Valid);

	const FGameXXKGuideStepDefinition DuplicateStep = Asset->Steps[0];
	Asset->Steps.Add(DuplicateStep);
	FDataValidationContext DuplicateContext;
	TestEqual(TEXT("duplicate step IDs reject"), Asset->IsDataValid(DuplicateContext), EDataValidationResult::Invalid);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideRulesLifecycleTest,
	"GameXXK.Guide.Core.RulesLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideRulesLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideRulesTestPrivate;
	UGameXXKGuideAsset* Asset = MakeGuide();
	FGameXXKGuideProgress Progress;
	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	FGameXXKGuideOutput Output;
	FString Error;

	TestFalse(TEXT("unmatched trigger does not start guide"),
		FGameXXKGuideRules::TryStart(*Asset, TEXT("Event.Other"), Progress, Output, &Error));
	TestTrue(TEXT("unmatched trigger is not a malformed-guide error"), Error.IsEmpty());
	TestTrue(FString::Printf(TEXT("matching trigger starts guide: %s"), *Error),
		FGameXXKGuideRules::TryStart(*Asset, TEXT("Event.Route.Opened"), Progress, Output, &Error));
	TestEqual(TEXT("forced step active"), Progress.ActiveGuideStepId, FName(TEXT("forced")));
	TestEqual(TEXT("forced output policy"), Output.InputPolicy, EGameXXKGuideInputPolicy::Forced);
	TestFalse(TEXT("forced guide rejects unrelated action"),
		FGameXXKGuideRules::CanExecuteAction(*Asset, Progress, TEXT("Action.Unrelated")));
	TestTrue(TEXT("forced guide permits registered action"),
		FGameXXKGuideRules::CanExecuteAction(*Asset, Progress, TEXT("Action.Route.SelectNext")));

	FGameXXKGuideOutput Resumed;
	TestTrue(TEXT("active guide resumes"), FGameXXKGuideRules::Resume(*Asset, Progress, Resumed, &Error));
	TestEqual(TEXT("resume retains step"), Resumed.StepId, FName(TEXT("forced")));

	UGameXXKGuideAsset* Other = MakeGuide(TEXT("Guide.Test.Other"));
	TestFalse(TEXT("only one guide may be active"),
		FGameXXKGuideRules::TryStart(*Other, TEXT("Event.Route.Opened"), Progress, Output, &Error));
	TestEqual(TEXT("failed second guide leaves first active"), Progress.ActiveGuideId, Asset->GuideId);

	const FName BeforeWrongEvent = Progress.ActiveGuideStepId;
	TestFalse(TEXT("unrelated event does not complete active step"),
		FGameXXKGuideRules::HandleEvent(*Asset, TEXT("Event.Other"), Progress, Output, &Error));
	TestEqual(TEXT("unrelated event is atomic"), Progress.ActiveGuideStepId, BeforeWrongEvent);

	TestTrue(TEXT("completion advances to soft step"),
		FGameXXKGuideRules::HandleEvent(
			*Asset, TEXT("Event.Route.NextNodeSelected"), Progress, Output, &Error));
	TestTrue(TEXT("completed step recorded"), Progress.CompletedGuideStepIds.Contains(TEXT("forced")));
	TestEqual(TEXT("soft step active"), Progress.ActiveGuideStepId, FName(TEXT("soft")));
	TestTrue(TEXT("soft guide never blocks unrelated actions"),
		FGameXXKGuideRules::CanExecuteAction(*Asset, Progress, TEXT("Action.Unrelated")));

	TestTrue(TEXT("terminal completion finishes guide"),
		FGameXXKGuideRules::HandleEvent(*Asset, TEXT("Event.Guide.Done"), Progress, Output, &Error));
	TestTrue(TEXT("terminal step recorded"), Progress.CompletedGuideStepIds.Contains(TEXT("soft")));
	TestTrue(TEXT("guide session clears at terminal"), Progress.ActiveGuideId.IsNone());
	TestFalse(TEXT("inactive guide cannot resume"), FGameXXKGuideRules::Resume(*Asset, Progress, Output, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideMissingForcedTargetTest,
	"GameXXK.Guide.Core.MissingForcedTargetUnlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideMissingForcedTargetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideRulesTestPrivate;
	UGameXXKGuideAsset* Asset = MakeGuide();
	FGameXXKGuideProgress Progress;
	Progress.Preference = EGameXXKGuidePreference::NewPlayer;
	FGameXXKGuideOutput Output;
	FString Error;
	TestTrue(TEXT("guide starts"),
		FGameXXKGuideRules::TryStart(*Asset, TEXT("Event.Route.Opened"), Progress, Output, &Error));
	TestTrue(TEXT("matching forced target failure advances without trapping input"),
		FGameXXKGuideRules::HandleTargetUnavailable(
			*Asset, TEXT("Route.Tutorial.NextNode"), Progress, Output, &Error));
	TestTrue(TEXT("missing target creates diagnostic"), Progress.LastDiagnostic.Contains(TEXT("Route.Tutorial.NextNode")));
	TestTrue(TEXT("unavailable forced step is completed"), Progress.CompletedGuideStepIds.Contains(TEXT("forced")));
	TestEqual(TEXT("next soft step remains available"), Progress.ActiveGuideStepId, FName(TEXT("soft")));
	TestTrue(TEXT("input is unlocked after forced target disappears"),
		FGameXXKGuideRules::CanExecuteAction(*Asset, Progress, TEXT("Action.Unrelated")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideTargetRegistryLifecycleTest,
	"GameXXK.Guide.Core.TargetRegistryLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideTargetRegistryLifecycleTest::RunTest(const FString& Parameters)
{
	FGameXXKGuideTargetRegistry Registry;
	UTextBlock* First = NewObject<UTextBlock>();
	UTextBlock* Second = NewObject<UTextBlock>();
	FString Error;
	const FSlateRect Expected(10.0f, 20.0f, 110.0f, 70.0f);
	TestTrue(TEXT("semantic target registers"), Registry.RegisterTarget(
		TEXT("Route.Tutorial.NextNode"),
		First,
		[Expected](FSlateRect& OutRect)
		{
			OutRect = Expected;
			return true;
		},
		&Error));
	TestFalse(TEXT("duplicate live target ID rejects"), Registry.RegisterTarget(
		TEXT("Route.Tutorial.NextNode"),
		Second,
		[](FSlateRect& OutRect)
		{
			OutRect = FSlateRect(0.0f, 0.0f, 1.0f, 1.0f);
			return true;
		},
		&Error));

	FSlateRect Resolved;
	TestTrue(TEXT("registered target resolves"),
		Registry.ResolveTargetRect(TEXT("Route.Tutorial.NextNode"), Resolved));
	TestEqual(TEXT("resolved left"), Resolved.Left, Expected.Left);
	TestEqual(TEXT("resolved bottom"), Resolved.Bottom, Expected.Bottom);
	Registry.UnregisterTarget(TEXT("Route.Tutorial.NextNode"), Second);
	TestTrue(TEXT("wrong owner cannot unregister target"),
		Registry.IsTargetRegistered(TEXT("Route.Tutorial.NextNode")));
	Registry.UnregisterTarget(TEXT("Route.Tutorial.NextNode"), First);
	TestFalse(TEXT("owner unregisters target"),
		Registry.IsTargetRegistered(TEXT("Route.Tutorial.NextNode")));
	TestTrue(TEXT("semantic target catalog contains tutorial route"),
		FGameXXKGuideTargetRegistry::IsKnownTargetId(TEXT("Route.Tutorial.NextNode")));
	TestFalse(TEXT("unknown semantic target rejects"),
		FGameXXKGuideTargetRegistry::IsKnownTargetId(TEXT("Unknown.Widget.Name")));
	int32 EventCount = 0;
	Registry.OnGuideEvent().AddLambda([&EventCount](const FName EventId)
	{
		++EventCount;
	});
	TestTrue(TEXT("known guide event emits"), Registry.EmitEvent(TEXT("Event.Route.Opened"), &Error));
	TestEqual(TEXT("known guide event reaches listener once"), EventCount, 1);
	TestFalse(TEXT("unknown guide event rejects"), Registry.EmitEvent(TEXT("Event.Unknown"), &Error));
	TestEqual(TEXT("unknown event never broadcasts"), EventCount, 1);
	Registry.SetActionGate(First, [](const FName ActionId)
	{
		return ActionId == TEXT("Action.Route.SelectNext");
	});
	TestTrue(TEXT("semantic gate permits allowed action"), Registry.IsActionAllowed(TEXT("Action.Route.SelectNext")));
	TestFalse(TEXT("semantic gate rejects unrelated action"), Registry.IsActionAllowed(TEXT("Action.Unrelated")));
	Registry.ClearActionGate(Second);
	TestFalse(TEXT("wrong owner cannot clear semantic gate"), Registry.IsActionAllowed(TEXT("Action.Unrelated")));
	Registry.ClearActionGate(First);
	TestTrue(TEXT("owner clears semantic gate"), Registry.IsActionAllowed(TEXT("Action.Unrelated")));
	return true;
}

#endif
