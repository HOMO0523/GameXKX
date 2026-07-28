#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "PaperFlipbook.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAnimationPresentationTest,
	"GameXXK.MVP.Battle.AnimationPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleAnimationPresentationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Player resolves the approved hero source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Player"), false),
		FString(TEXT("character_00_hero")));
	TestEqual(TEXT("persistent Blade resolves its distinct source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("CompanionInstance.Companion_Blade_01"), false),
		FString(TEXT("character_01_blade")));
	TestEqual(TEXT("Zhou Guang Zu resolves the approved fox-and-scroll source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Npc.ZhouGuangZu"), false),
		FString(TEXT("character_10_zhou_guang_zu")));
	TestEqual(TEXT("chapter-one rooster resolves its own monster source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Enemy.Ch1.Rooster"), true),
		FString(TEXT("enemy_01_rooster")));
	TestEqual(TEXT("chapter-two gray wolf resolves its own monster source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Enemy.Ch2.GrayWolf"), true),
		FString(TEXT("enemy_07_graywolf")));
	TestEqual(TEXT("legacy BlackBear resolves the approved boss source"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("BlackBear"), true),
		FString(TEXT("enemy_20_blackbear_boss")));

	const FGameXXKBattleAnimationClipDescriptor HeroAttack =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Attack);
	TestTrue(TEXT("hero attack descriptor is valid"), HeroAttack.IsValid());
	TestEqual(TEXT("hero attack uses the imported production atlas"), HeroAttack.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_attack_atlas.T_character_00_hero_attack_atlas")));
	TestEqual(TEXT("production clips retain sixty generated frames"), HeroAttack.FrameCount, 60);
	TestEqual(TEXT("production atlases use an eight by eight grid"), HeroAttack.Columns, 8);
	TestEqual(TEXT("production source playback is twelve fps"), HeroAttack.SourceFramesPerSecond, 12.0f);
	TestEqual(TEXT("attack and hit actions play at two times speed"), HeroAttack.PlaybackRate, 2.0f);
	TestEqual(TEXT("a five-second source action occupies at most two-point-five runtime seconds"),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(HeroAttack), 2.5f);
	TestEqual(TEXT("the source 2.2-second impact becomes runtime 1.1 seconds at two-times playback"),
		FGameXXKBattleAnimationPresentation::GetImpactRuntimeSeconds(), 1.1f);
	TestEqual(TEXT("runtime impact samples the synchronized source frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 1.1f, false), 26);
	TestEqual(TEXT("hero scene idle resolves the production PaperFlipbook path"),
		FGameXXKBattleAnimationPresentation::ResolveIdleFlipbookPath(TEXT("Player"), false).ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_character_00_hero_idle.FB_character_00_hero_idle")));
	TestEqual(TEXT("rooster scene idle resolves the production PaperFlipbook path"),
		FGameXXKBattleAnimationPresentation::ResolveIdleFlipbookPath(TEXT("Enemy.Ch1.Rooster"), true).ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_enemy_01_rooster_idle.FB_enemy_01_rooster_idle")));
	TestEqual(TEXT("non-looping playback holds the last generated frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 3.0f, false), 59);

	const FBox2f FrameNineUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 9);
	TestTrue(TEXT("frame nine begins at atlas column one row one"),
		FrameNineUv.Min.Equals(FVector2f(0.125f, 0.125f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("frame nine ends at atlas column two row two"),
		FrameNineUv.Max.Equals(FVector2f(0.25f, 0.25f), KINDA_SMALL_NUMBER));

	const FGameXXKBattleAnimationClipDescriptor MissingWolfAttack =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Enemy.Ch2.GrayWolf"), true, EGameXXKBattleAnimationAction::Attack);
	TestFalse(TEXT("the one terminally failed gray-wolf attack is never presented as an imported clip"), MissingWolfAttack.IsValid());

	FGameXXKCardBattleRuntime PostDamageBattle;
	FGameXXKCardCombatUnit AttackingHero;
	AttackingHero.UnitId = TEXT("Player");
	AttackingHero.Side = EGameXXKCardTargetSide::Party;
	AttackingHero.bLiving = true;
	PostDamageBattle.Units.Add(AttackingHero);
	FGameXXKCardCombatUnit DefeatedRooster;
	DefeatedRooster.UnitId = TEXT("Enemy.Ch1.Rooster");
	DefeatedRooster.Side = EGameXXKCardTargetSide::Enemy;
	DefeatedRooster.bLiving = false;
	DefeatedRooster.HP = 0;
	PostDamageBattle.Units.Add(DefeatedRooster);
	FGameXXKCardDamageResult FirstHit;
	FirstHit.SourceUnitId = AttackingHero.UnitId;
	FirstHit.ResolvedTargetUnitId = DefeatedRooster.UnitId;
	FirstHit.HealthDamage = 8;
	FGameXXKCardDamageResult FinalHit = FirstHit;
	FinalHit.HealthDamage = 12;
	const TArray<FGameXXKBattleAnimationCombatRequest> MultiHitRequests =
		FGameXXKBattleAnimationPresentation::BuildCombatRequests(
			PostDamageBattle,
			AttackingHero.UnitId,
			{FirstHit, FinalHit});
	TestEqual(TEXT("each damage packet remains a distinct cinematic for multi-hit cards"), MultiHitRequests.Num(), 2);
	if (MultiHitRequests.Num() == 2)
	{
		TestFalse(TEXT("an earlier packet never plays death merely because post-state is defeated"),
			MultiHitRequests[0].bTargetDefeated);
		TestTrue(TEXT("only the final packet for the defeated target appends death"),
			MultiHitRequests[1].bTargetDefeated);
		TestFalse(TEXT("party source is routed as a character"), MultiHitRequests[0].bAttackerEnemy);
		TestTrue(TEXT("enemy target is routed as a monster"), MultiHitRequests[0].bTargetEnemy);
	}

	FGameXXKBattleRuntimeUnit HeroUnit;
	HeroUnit.Id = TEXT("Player");
	HeroUnit.HP = 100;
	HeroUnit.MaxHP = 100;
	AGameXXKBattleSceneUnitActor* HeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
	HeroActor->ConfigureFromRuntimeUnit(false, 0, HeroUnit, 2);
	TestNotNull(TEXT("imported production hero idle is loadable by the scene actor"), HeroActor->GetCurrentBattleFlipbook());
	if (HeroActor->GetCurrentBattleFlipbook())
	{
		TestTrue(TEXT("scene hero prefers the imported production idle over the legacy portrait flipbook"),
			HeroActor->GetCurrentBattleFlipbook()->GetPathName().Contains(TEXT("FB_character_00_hero_idle")));
	}

	FGameXXKBattleRuntimeUnit RoosterUnit = HeroUnit;
	RoosterUnit.Id = TEXT("Enemy.Ch1.Rooster");
	RoosterUnit.bEnemy = true;
	AGameXXKBattleSceneUnitActor* RoosterActor = NewObject<AGameXXKBattleSceneUnitActor>();
	RoosterActor->ConfigureFromRuntimeUnit(true, 0, RoosterUnit, 1);
	TestNotNull(TEXT("imported production rooster idle is loadable by the scene actor"), RoosterActor->GetCurrentBattleFlipbook());
	if (RoosterActor->GetCurrentBattleFlipbook())
	{
		TestTrue(TEXT("scene rooster prefers its own production idle instead of the money-rat placeholder"),
			RoosterActor->GetCurrentBattleFlipbook()->GetPathName().Contains(TEXT("FB_enemy_01_rooster_idle")));
	}

	return true;
}

#endif
