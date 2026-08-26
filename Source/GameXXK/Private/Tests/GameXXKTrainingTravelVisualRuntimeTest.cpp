#include "UI/GameXXKTrainingTravelVisualRuntime.h"

#include "UI/GameXXKBattleAnimationPresentation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKTrainingTravelRuntime MakeTravelRuntime(
		const EGameXXKTrainingTravelPhase Phase,
		const FName EnemyId = TEXT("Enemy.Ch1.Rooster"),
		const int32 EnemyHP = 1,
		const int32 EnemyMaxHP = 1,
		const int32 PlayerHP = 100,
		const int32 PlayerMaxHP = 100)
	{
		FGameXXKTrainingTravelRuntime Runtime;
		Runtime.StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
		Runtime.EncounterIndex = 0;
		Runtime.EnemyDefinitionId = EnemyId;
		Runtime.Phase = Phase;
		Runtime.PlayerHP = PlayerHP;
		Runtime.PlayerMaxHP = PlayerMaxHP;
		Runtime.EnemyHP = EnemyHP;
		Runtime.EnemyMaxHP = EnemyMaxHP;
		FGameXXKTrainingTravelEnemyRuntime Enemy;
		Enemy.EnemyDefinitionId = EnemyId;
		Enemy.HP = EnemyHP;
		Enemy.MaxHP = EnemyMaxHP;
		Enemy.Attack = 1;
		Runtime.Enemies = {Enemy};
		Runtime.ActiveEnemyIndex = 0;
		return Runtime;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeCombatPresentationTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.CombatPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeCombatPresentationTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;
	const FGameXXKTrainingTravelRuntime Walking = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Walking);
	FGameXXKTrainingTravelRuntime Combat = Walking;
	Combat.Phase = EGameXXKTrainingTravelPhase::Combat;
	const FGameXXKTrainingTravelRuntime NextWalking = MakeTravelRuntime(
		EGameXXKTrainingTravelPhase::Walking,
		TEXT("Enemy.Ch1.Civet"));

	Runtime.Synchronize(Walking);
	Runtime.Tick(0.5f);
	TestEqual(TEXT("walking uses the walk presentation"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::Walking);
	TestTrue(TEXT("walking advances the seamless lane"), Runtime.GetScrollOffset() > 0.0f);
	TestEqual(TEXT("walking advances the 12 fps walkloop by six frames"), Runtime.GetWalkFrameIndex(), 6);

	Runtime.NotifyTravelStep(Walking, Combat, false, false, false);
	TestEqual(TEXT("encounter entry switches the standing hero to idle"), Runtime.GetHeroAction(), EGameXXKBattleAnimationAction::Idle);
	TestTrue(TEXT("encounter entry exposes the authored enemy"), Runtime.IsEnemyVisible());
	TestEqual(TEXT("encounter entry keeps the authored enemy identity"), Runtime.GetEnemyDefinitionId(), FName(TEXT("Enemy.Ch1.Rooster")));

	Runtime.NotifyTravelStep(Combat, NextWalking, true, false, false);
	TestEqual(TEXT("settlement starts with the hero attack"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::HeroAttack);
	TestEqual(TEXT("lethal settlement retains the defeated enemy instead of the next encounter"), Runtime.GetEnemyDefinitionId(), FName(TEXT("Enemy.Ch1.Rooster")));

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds);
	TestEqual(TEXT("hero attack is followed by the enemy hit"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyHit);
	TestEqual(TEXT("enemy uses the shared battle hit action"), Runtime.GetEnemyAction(), EGameXXKBattleAnimationAction::Hit);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds);
	TestEqual(TEXT("a lethal hit is followed by enemy death"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyDeath);
	TestEqual(TEXT("enemy uses the shared battle death action"), Runtime.GetEnemyAction(), EGameXXKBattleAnimationAction::Death);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds);
	TestEqual(TEXT("death completion resumes the latest authoritative walking phase"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::Walking);
	TestFalse(TEXT("the defeated enemy leaves after the death clip"), Runtime.IsEnemyVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeNonLethalExchangeTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.NonLethalExchange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeNonLethalExchangeTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;
	const FGameXXKTrainingTravelRuntime Before = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Combat, TEXT("Enemy.Ch1.Rooster"), 10, 10, 100, 100);
	const FGameXXKTrainingTravelRuntime After = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Combat, TEXT("Enemy.Ch1.Rooster"), 6, 10, 98, 100);

	Runtime.Synchronize(Before);
	Runtime.NotifyTravelStep(Before, After, false, false, false);
	TestEqual(TEXT("non-lethal exchange starts with hero attack"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::HeroAttack);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds);
	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds * 0.5f);
	TestEqual(TEXT("enemy remains in hit while HP is tweening"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyHit);
	TestTrue(TEXT("enemy HP tween has started"), Runtime.GetEnemyHealthFraction() < 1.0f);
	TestTrue(TEXT("enemy HP tween has not jumped to the final value"), Runtime.GetEnemyHealthFraction() > 0.6f);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds * 0.5f);
	TestEqual(TEXT("surviving enemy retaliates"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyAttack);
	TestEqual(TEXT("enemy retaliation uses the shared attack action"), Runtime.GetEnemyAction(), EGameXXKBattleAnimationAction::Attack);
	TestTrue(TEXT("enemy HP reaches the post-hit fraction"), FMath::IsNearlyEqual(Runtime.GetEnemyHealthFraction(), 0.6f));

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyAttackSeconds);
	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds * 0.5f);
	TestEqual(TEXT("hero receives the retaliation hit"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::HeroHit);
	TestTrue(TEXT("hero HP tween has started"), Runtime.GetHeroHealthFraction() < 1.0f);
	TestTrue(TEXT("hero HP tween has not jumped to the final value"), Runtime.GetHeroHealthFraction() > 0.98f);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds * 0.5f);
	TestEqual(TEXT("non-lethal exchange returns to encounter idle"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EncounterIdle);
	TestTrue(TEXT("hero HP reaches the post-hit fraction"), FMath::IsNearlyEqual(Runtime.GetHeroHealthFraction(), 0.98f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeAuthoritativeEnemyHealthTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.AuthoritativeEnemyHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeAuthoritativeEnemyHealthTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelRuntime Snapshot = MakeTravelRuntime(
		EGameXXKTrainingTravelPhase::Combat,
		TEXT("Enemy.Ch1.Rooster"),
		90,
		100);
	Snapshot.Enemies[0].HP = 25;
	Snapshot.Enemies[0].MaxHP = 100;
	Snapshot.EnemyHP = 90;
	Snapshot.EnemyMaxHP = 100;

	FGameXXKTrainingTravelVisualRuntime Visual;
	Visual.Synchronize(Snapshot);
	TestTrue(TEXT("active enemy bar reads the authoritative slot array"),
		FMath::IsNearlyEqual(Visual.GetEnemyHealthFractionForSlot(0), 0.25f));
	TestTrue(TEXT("compatibility getter delegates to the authoritative active slot"),
		FMath::IsNearlyEqual(Visual.GetEnemyHealthFraction(), 0.25f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeFormationAdvanceTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.FormationAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeFormationAdvanceTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;
	FGameXXKTrainingTravelRuntime Before = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Combat);
	FGameXXKTrainingTravelEnemyRuntime Civet;
	Civet.EnemyDefinitionId = TEXT("Enemy.Ch1.Civet");
	Civet.HP = 1;
	Civet.MaxHP = 1;
	Civet.Attack = 1;
	Before.Enemies.Add(Civet);

	FGameXXKTrainingTravelRuntime After = Before;
	After.Enemies[0].HP = 0;
	After.ActiveEnemyIndex = 1;
	After.EnemyDefinitionId = After.Enemies[1].EnemyDefinitionId;
	After.EnemyHP = After.Enemies[1].HP;
	After.EnemyMaxHP = After.Enemies[1].MaxHP;

	Runtime.Synchronize(Before);
	Runtime.NotifyTravelStep(Before, After, false, false, false);
	TestEqual(TEXT("formation presentation retains both authored slots"), Runtime.GetEnemyFormationSlotCount(), 2);
	TestEqual(TEXT("the defeated first slot remains the presented target"), Runtime.GetPresentedEnemySlotIndex(), 0);
	TestEqual(TEXT("the first slot retains the rooster identity"), Runtime.GetEnemyDefinitionIdForSlot(0), FName(TEXT("Enemy.Ch1.Rooster")));
	TestEqual(TEXT("the second slot retains the civet identity"), Runtime.GetEnemyDefinitionIdForSlot(1), FName(TEXT("Enemy.Ch1.Civet")));
	TestTrue(TEXT("the dying first slot remains visible for its clip"), Runtime.IsEnemySlotVisible(0));
	TestTrue(TEXT("the waiting second slot remains visible"), Runtime.IsEnemySlotVisible(1));
	TestFalse(TEXT("an unused third slot never appears during the target handoff"), Runtime.IsEnemySlotVisible(2));

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds);
	TestEqual(TEXT("formation resumes on the next living enemy"), Runtime.GetPresentedEnemySlotIndex(), 1);
	TestFalse(TEXT("the completed death clip removes the first slot"), Runtime.IsEnemySlotVisible(0));
	TestTrue(TEXT("the second slot remains ready for combat"), Runtime.IsEnemySlotVisible(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeFormationBoundaryIsolationTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.FormationBoundaryIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeFormationBoundaryIsolationTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;
	FGameXXKTrainingTravelRuntime Before = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Combat);
	FGameXXKTrainingTravelEnemyRuntime Civet;
	Civet.EnemyDefinitionId = TEXT("Enemy.Ch1.Civet");
	Civet.HP = 1;
	Civet.MaxHP = 1;
	Civet.Attack = 1;
	Before.Enemies.Add(Civet);
	Before.ActiveEnemyIndex = 1;
	Before.EnemyDefinitionId = Civet.EnemyDefinitionId;
	Before.EnemyHP = Civet.HP;
	Before.EnemyMaxHP = Civet.MaxHP;

	FGameXXKTrainingTravelRuntime NextFormation = MakeTravelRuntime(
		EGameXXKTrainingTravelPhase::Walking,
		TEXT("Enemy.Ch1.Rooster"));
	NextFormation.EncounterIndex = 1;
	FGameXXKTrainingTravelEnemyRuntime Goat;
	Goat.EnemyDefinitionId = TEXT("Enemy.Ch1.Goat");
	Goat.HP = 1;
	Goat.MaxHP = 1;
	Goat.Attack = 1;
	FGameXXKTrainingTravelEnemyRuntime NextCivet = Civet;
	NextFormation.Enemies = {NextFormation.Enemies[0], Goat, NextCivet};

	Runtime.Synchronize(Before);
	Runtime.NotifyTravelStep(Before, NextFormation, true, false, false);
	TestEqual(TEXT("the dying two-member formation remains the presentation boundary"), Runtime.GetEnemyFormationSlotCount(), 2);
	TestTrue(TEXT("the defeated second slot remains visible for its death presentation"), Runtime.IsEnemySlotVisible(1));
	TestFalse(TEXT("the next encounter third slot cannot leak into the prior death presentation"), Runtime.IsEnemySlotVisible(2));
	TestTrue(TEXT("the out-of-bound presentation slot has no enemy identity"), Runtime.GetEnemyDefinitionIdForSlot(2).IsNone());
	TestTrue(TEXT("the out-of-bound presentation slot has no leaked health"), FMath::IsNearlyZero(Runtime.GetEnemyHealthFractionForSlot(2)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeSmoothLoopTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.SmoothSeamlessLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeSmoothLoopTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelVisualRuntime Runtime;
	const FGameXXKTrainingTravelRuntime Walking = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Walking);
	FGameXXKTrainingTravelRuntime Combat = Walking;
	Combat.Phase = EGameXXKTrainingTravelPhase::Combat;
	const FGameXXKTrainingTravelRuntime NextWalking = MakeTravelRuntime(EGameXXKTrainingTravelPhase::Walking, TEXT("Enemy.Ch1.Civet"));

	Runtime.Synchronize(Walking);
	Runtime.Tick(0.5f);
	const float OffsetBeforeEncounter = Runtime.GetScrollOffset();
	const float SpeedBeforeEncounter = Runtime.GetScrollVelocity();
	const int32 FrameBeforeSettlement = Runtime.GetWalkFrameIndex();
	TestTrue(TEXT("walking reaches a positive scroll speed"), SpeedBeforeEncounter > 0.0f);

	Runtime.NotifyTravelStep(Walking, Combat, false, false, false);
	Runtime.Tick(0.05f);
	TestTrue(TEXT("encounter entry eases instead of snapping the lane to a stop"), Runtime.GetScrollOffset() > OffsetBeforeEncounter);
	TestTrue(TEXT("encounter entry starts decelerating"), Runtime.GetScrollVelocity() > 0.0f && Runtime.GetScrollVelocity() < SpeedBeforeEncounter);

	const float OffsetBeforeSettlement = Runtime.GetScrollOffset();
	Runtime.NotifyTravelStep(Combat, NextWalking, true, true, false);
	TestEqual(TEXT("stage settlement increments the visible loop count"), Runtime.GetCompletedLoopCount(), 1);
	TestTrue(TEXT("stage settlement does not reset the seamless lane origin"), Runtime.GetScrollOffset() >= OffsetBeforeSettlement);
	TestEqual(TEXT("stage settlement does not reset the walk atlas"), Runtime.GetWalkFrameIndex(), FrameBeforeSettlement);

	Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds
		+ 1.0f);
	TestEqual(TEXT("large delta consumes the whole death presentation and resumes walking"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::Walking);
	TestTrue(TEXT("walking resumes with forward velocity"), Runtime.GetScrollVelocity() > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimePartyActionOwnershipTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.PartyActionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimePartyActionOwnershipTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelRuntime Before = MakeTravelRuntime(
		EGameXXKTrainingTravelPhase::Combat,
		TEXT("Enemy.Ch1.Rooster"),
		10,
		10,
		100,
		100);
	Before.PartyUnits = {
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Hero"), 100, 100, 4),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Companion.Blade.Test"), 80, 80, 4),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Npc.TusiChief"), 90, 90, 4)};
	FGameXXKTrainingTravelRuntime After = Before;
	After.Enemies[0].HP = 6;
	After.EnemyHP = 6;
	After.LastAttackingPartyIndex = 1;
	After.LastDamagedPartyIndex = INDEX_NONE;

	FGameXXKTrainingTravelVisualRuntime Visual;
	Visual.Synchronize(Before);
	Visual.NotifyTravelStep(Before, After, false, false, false);
	TestEqual(TEXT("Blade owns the attack presentation"),
		Visual.GetPartyAction(1), EGameXXKBattleAnimationAction::Attack);
	TestEqual(TEXT("hero remains idle during Blade's attack"),
		Visual.GetPartyAction(0), EGameXXKBattleAnimationAction::Idle);
	TestEqual(TEXT("Tusi Chief remains idle during Blade's attack"),
		Visual.GetPartyAction(2), EGameXXKBattleAnimationAction::Idle);

	Visual.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds);
	TestEqual(TEXT("party-only hit without retaliation returns to encounter idle"),
		Visual.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EncounterIdle);

	FGameXXKTrainingTravelRuntime SecondBefore = After;
	FGameXXKTrainingTravelRuntime SecondAfter = SecondBefore;
	SecondAfter.Enemies[0].HP = 2;
	SecondAfter.EnemyHP = 2;
	SecondAfter.PartyUnits[1].HP = 78;
	SecondAfter.LastAttackingPartyIndex = 2;
	SecondAfter.LastDamagedPartyIndex = 1;
	SecondAfter.LastDamageToPlayer = 2;
	Visual.NotifyTravelStep(SecondBefore, SecondAfter, false, false, false);
	TestEqual(TEXT("Tusi Chief owns the next attack presentation"),
		Visual.GetPartyAction(2), EGameXXKBattleAnimationAction::Attack);

	Visual.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds
		+ FGameXXKTrainingTravelVisualRuntime::EnemyAttackSeconds);
	TestEqual(TEXT("enemy retaliation applies hit only to Blade"),
		Visual.GetPartyAction(1), EGameXXKBattleAnimationAction::Hit);
	TestEqual(TEXT("Tusi Chief returns idle during Blade hit"),
		Visual.GetPartyAction(2), EGameXXKBattleAnimationAction::Idle);
	Visual.Tick(FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds * 0.5f);
	TestTrue(TEXT("Blade HP bar tweens from its own runtime values"),
		Visual.GetPartyHealthFraction(1) < 1.0f
			&& Visual.GetPartyHealthFraction(1) > 78.0f / 80.0f);
	TestTrue(TEXT("hero HP remains independent from Blade damage"),
		FMath::IsNearlyEqual(Visual.GetPartyHealthFraction(0), 1.0f));
	return true;
}

#endif
