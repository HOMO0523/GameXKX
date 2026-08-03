#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleAnimationLayerWidget.h"
#include "UI/GameXXKBattleBoardWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAnimationLayerWidgetTest,
	"GameXXK.MVP.Battle.AnimationLayerWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAnimationLayerWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleAnimationLayerWidget* Layer = NewObject<UGameXXKBattleAnimationLayerWidget>();
	Layer->TakeWidget();
	TestNotNull(TEXT("battle cinematic owns an ordinary Canvas root"), Layer->GetRootCanvasForTest());
	TestNotNull(TEXT("battle cinematic owns a full-screen dimmer"), Layer->GetDimmerForTest());
	TestNotNull(TEXT("battle cinematic owns the enlarged attacker image"), Layer->GetAttackerImageForTest());
	TestNotNull(TEXT("battle cinematic owns the enlarged target image"), Layer->GetTargetImageForTest());
	TestNotNull(TEXT("battle cinematic owns the fixed central impact image"), Layer->GetImpactImageForTest());
	TestEqual(TEXT("battle cinematic dims the battle HUD by exactly fifty percent"),
		Layer->GetDimOpacityForTest(), 0.5f);
	TestEqual(TEXT("each unit cinematic is enlarged to the approved near-full-screen scale"),
		Layer->GetUnitImageSizeForTest(), FVector2D(820.0f, 820.0f));
	TestEqual(TEXT("the impact remains fixed at screen center"),
		Layer->GetImpactAnchorForTest(), FVector2D(0.5f, 0.5f));
	TestEqual(TEXT("idle layer starts hidden"), Layer->GetVisibility(), ESlateVisibility::Collapsed);

	Layer->QueueCombatSequence(TEXT("Player"), false, TEXT("Enemy.Ch1.Rooster"), true, true);
	TestTrue(TEXT("queued combat immediately activates the presentation layer"), Layer->IsPresentationActiveForTest());
	TestEqual(TEXT("lethal combat queues hit presentation then a death presentation"), Layer->GetQueuedSequenceCountForTest(), 1);
	TestFalse(TEXT("impact is hidden before the synchronized hit moment"), Layer->IsImpactVisibleForTest());

	Layer->AdvanceAnimationForTest(1.09f);
	TestFalse(TEXT("impact remains hidden before runtime 1.1 seconds"), Layer->IsImpactVisibleForTest());
	Layer->AdvanceAnimationForTest(0.02f);
	TestTrue(TEXT("generic ink impact appears at runtime 1.1 seconds"), Layer->IsImpactVisibleForTest());
	TestEqual(TEXT("impact starts from its first atlas frame"), Layer->GetImpactFrameForTest(), 0);
	TestNotNull(TEXT("active attack brush owns its atlas texture"),
		Layer->GetAttackerImageForTest()->GetBrush().GetResourceObject());
	TestNotNull(TEXT("active hit brush owns its atlas texture"),
		Layer->GetTargetImageForTest()->GetBrush().GetResourceObject());
	TestNotNull(TEXT("active impact brush owns its atlas texture"),
		Layer->GetImpactImageForTest()->GetBrush().GetResourceObject());
	Layer->AdvanceAnimationForTest(1.39f);
	TestTrue(TEXT("lethal target death starts after the paired attack-hit clip"), Layer->IsPresentationActiveForTest());
	TestEqual(TEXT("death sequence drains the remaining queue when it starts"), Layer->GetQueuedSequenceCountForTest(), 0);
	Layer->AdvanceAnimationForTest(2.5f);
	TestTrue(TEXT("death presentation remains active halfway through its five-second runtime"),
		Layer->IsPresentationActiveForTest());
	Layer->AdvanceAnimationForTest(2.5f);
	TestFalse(TEXT("completed death returns the cinematic layer to hidden idle"), Layer->IsPresentationActiveForTest());
	TestEqual(TEXT("completed cinematic collapses above-HUD overlay"), Layer->GetVisibility(), ESlateVisibility::Collapsed);
	TestNull(TEXT("completed cinematic releases the attack atlas from its Slate brush"),
		Layer->GetAttackerImageForTest()->GetBrush().GetResourceObject());
	TestNull(TEXT("completed cinematic releases the hit atlas from its Slate brush"),
		Layer->GetTargetImageForTest()->GetBrush().GetResourceObject());
	TestNull(TEXT("completed cinematic releases the impact atlas from its Slate brush"),
		Layer->GetImpactImageForTest()->GetBrush().GetResourceObject());

	Layer->QueueCombatSequence(TEXT("Player"), false, TEXT("Enemy.Ch1.Rooster"), true, false);
	Layer->QueueCombatSequence(TEXT("Player"), false, TEXT("Enemy.Ch1.Rooster"), true, false);
	TestTrue(TEXT("manual reset fixture begins with one active cinematic"), Layer->IsPresentationActiveForTest());
	TestEqual(TEXT("manual reset fixture retains one queued cinematic"), Layer->GetQueuedSequenceCountForTest(), 1);
	Layer->ResetPresentation();
	TestFalse(TEXT("manual reset stops the active cinematic"), Layer->IsPresentationActiveForTest());
	TestEqual(TEXT("manual reset discards queued cinematics"), Layer->GetQueuedSequenceCountForTest(), 0);
	TestNull(TEXT("manual reset releases the active attack brush"),
		Layer->GetAttackerImageForTest()->GetBrush().GetResourceObject());
	TestNull(TEXT("manual reset releases the active hit brush"),
		Layer->GetTargetImageForTest()->GetBrush().GetResourceObject());

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->NativeConstruct();
	TestNotNull(TEXT("battle board owns one dedicated above-HUD animation layer"), Board->GetBattleAnimationLayerForTest());
	Board->QueueCombatAnimation(TEXT("Player"), false, TEXT("Enemy.Ch1.Rooster"), true, false);
	TestTrue(TEXT("battle board bridge activates its dedicated cinematic layer"),
		Board->GetBattleAnimationLayerForTest() && Board->GetBattleAnimationLayerForTest()->IsPresentationActiveForTest());
	Board->RefreshFromState();
	TestFalse(TEXT("battle board clears transient cinematics when it is outside battle"),
		Board->GetBattleAnimationLayerForTest() && Board->GetBattleAnimationLayerForTest()->IsPresentationActiveForTest());
	TestEqual(TEXT("battle board discards queued cinematics when it is outside battle"),
		Board->GetBattleAnimationLayerForTest()
			? Board->GetBattleAnimationLayerForTest()->GetQueuedSequenceCountForTest()
			: -1,
		0);

	return true;
}

#endif
