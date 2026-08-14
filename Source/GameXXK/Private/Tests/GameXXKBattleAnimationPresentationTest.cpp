#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "PaperFlipbook.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleAnimationPresentationTest,
	"GameXXK.MVP.Battle.AnimationPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakePresentationUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.MaxMana = 10;
		Unit.Mana = 10;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattlePresentationRhythmTest,
	"GameXXK.Presentation.BattleAnimation.Rhythm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattlePresentationRhythmTest::RunTest(const FString& Parameters)
{
	const auto ExpectSeconds = [this](const TCHAR* What, const float Actual, const float Expected)
	{
		TestTrue(What, FMath::IsNearlyEqual(Actual, Expected, 0.0001f));
	};

	FGameXXKBattlePresentationEvent FirstLightEvent;
	FirstLightEvent.HitOrdinal = 0;
	FirstLightEvent.HealthDamage = 5;
	FirstLightEvent.TargetHealthBefore = 100;
	FirstLightEvent.TargetHealthAfter = 95;
	const FGameXXKBattlePresentationRhythm FirstLight =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(FirstLightEvent);
	TestTrue(TEXT("first-hit light rhythm is valid"), FirstLight.IsValid());
	ExpectSeconds(TEXT("first hit lasts zero-point-eight-two seconds"), FirstLight.DurationSeconds, 0.82f);
	ExpectSeconds(TEXT("first hit impacts at zero-point-three seconds"), FirstLight.ImpactSeconds, 0.30f);
	TestEqual(TEXT("five-percent damage resolves to light feedback"), FirstLight.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Light);
	ExpectSeconds(TEXT("light shake uses three horizontal units"), FirstLight.ShakeAmplitude.X, 3.0f);
	ExpectSeconds(TEXT("light shake uses one-point-five vertical units"), FirstLight.ShakeAmplitude.Y, 1.5f);
	ExpectSeconds(TEXT("light shake lasts zero-point-twelve seconds"), FirstLight.ShakeDurationSeconds, 0.12f);
	ExpectSeconds(TEXT("light readout peaks at one-point-twelve"), FirstLight.ReadoutPeakScale, 1.12f);
	const auto ResolveTierAtPercent = [](const int32 Damage)
	{
		FGameXXKBattlePresentationEvent Event;
		Event.HealthDamage = Damage;
		Event.TargetHealthBefore = 100;
		Event.TargetHealthAfter = 100 - Damage;
		return FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(Event).ImpactTier;
	};
	TestEqual(TEXT("nine percent remains light feedback"), ResolveTierAtPercent(9),
		EGameXXKBattlePresentationImpactTier::Light);
	TestEqual(TEXT("ten percent begins medium feedback"), ResolveTierAtPercent(10),
		EGameXXKBattlePresentationImpactTier::Medium);
	TestEqual(TEXT("twenty-nine percent remains medium feedback"), ResolveTierAtPercent(29),
		EGameXXKBattlePresentationImpactTier::Medium);
	TestEqual(TEXT("thirty percent begins heavy feedback"), ResolveTierAtPercent(30),
		EGameXXKBattlePresentationImpactTier::Heavy);
	TestEqual(TEXT("fifty-nine percent remains heavy feedback"), ResolveTierAtPercent(59),
		EGameXXKBattlePresentationImpactTier::Heavy);
	TestEqual(TEXT("sixty percent begins lethal feedback"), ResolveTierAtPercent(60),
		EGameXXKBattlePresentationImpactTier::Lethal);
	FGameXXKBattlePresentationEvent ArmorOnlyEvent = FirstLightEvent;
	ArmorOnlyEvent.HealthDamage = 0;
	ArmorOnlyEvent.ArmorAbsorbed = 35;
	ArmorOnlyEvent.TargetHealthAfter = ArmorOnlyEvent.TargetHealthBefore;
	TestEqual(TEXT("armor-only impact strength uses total mitigated plus health damage"),
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(ArmorOnlyEvent).ImpactTier,
		EGameXXKBattlePresentationImpactTier::Heavy);

	FGameXXKBattlePresentationEvent FollowMediumEvent = FirstLightEvent;
	FollowMediumEvent.HitOrdinal = 1;
	FollowMediumEvent.HealthDamage = 20;
	FollowMediumEvent.TargetHealthAfter = 80;
	const FGameXXKBattlePresentationRhythm FollowMedium =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(FollowMediumEvent);
	ExpectSeconds(TEXT("follow-up hit lasts zero-point-three seconds"), FollowMedium.DurationSeconds, 0.30f);
	ExpectSeconds(TEXT("follow-up hit impacts at zero-point-one seconds"), FollowMedium.ImpactSeconds, 0.10f);
	TestEqual(TEXT("twenty-percent damage resolves to medium feedback"), FollowMedium.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Medium);
	ExpectSeconds(TEXT("medium shake uses six horizontal units"), FollowMedium.ShakeAmplitude.X, 6.0f);
	ExpectSeconds(TEXT("medium shake uses three vertical units"), FollowMedium.ShakeAmplitude.Y, 3.0f);
	ExpectSeconds(TEXT("medium shake lasts zero-point-sixteen seconds"), FollowMedium.ShakeDurationSeconds, 0.16f);
	ExpectSeconds(TEXT("medium readout peaks at one-point-two"), FollowMedium.ReadoutPeakScale, 1.20f);

	FGameXXKBattlePresentationEvent HeavyEvent = FirstLightEvent;
	HeavyEvent.HealthDamage = 45;
	HeavyEvent.TargetHealthAfter = 55;
	const FGameXXKBattlePresentationRhythm Heavy =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(HeavyEvent);
	TestEqual(TEXT("forty-five-percent damage resolves to heavy feedback"), Heavy.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Heavy);
	ExpectSeconds(TEXT("heavy shake uses nine horizontal units"), Heavy.ShakeAmplitude.X, 9.0f);
	ExpectSeconds(TEXT("heavy shake uses four-point-five vertical units"), Heavy.ShakeAmplitude.Y, 4.5f);
	ExpectSeconds(TEXT("heavy shake lasts zero-point-two seconds"), Heavy.ShakeDurationSeconds, 0.20f);
	ExpectSeconds(TEXT("heavy readout peaks at one-point-three"), Heavy.ReadoutPeakScale, 1.30f);

	FGameXXKBattlePresentationEvent LethalEvent = FirstLightEvent;
	LethalEvent.HealthDamage = 60;
	LethalEvent.TargetHealthAfter = 40;
	const FGameXXKBattlePresentationRhythm LethalByRatio =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(LethalEvent);
	TestEqual(TEXT("sixty-percent damage resolves to lethal feedback even without defeat"), LethalByRatio.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Lethal);
	ExpectSeconds(TEXT("lethal shake uses fourteen horizontal units"), LethalByRatio.ShakeAmplitude.X, 14.0f);
	ExpectSeconds(TEXT("lethal shake uses seven vertical units"), LethalByRatio.ShakeAmplitude.Y, 7.0f);
	ExpectSeconds(TEXT("lethal shake lasts zero-point-two-six seconds"), LethalByRatio.ShakeDurationSeconds, 0.26f);
	ExpectSeconds(TEXT("lethal readout peaks at one-point-four-two"), LethalByRatio.ReadoutPeakScale, 1.42f);

	FGameXXKBattlePresentationEvent LethalTransitionEvent = FirstLightEvent;
	LethalTransitionEvent.HealthDamage = 1;
	LethalTransitionEvent.TargetHealthAfter = 0;
	LethalTransitionEvent.bTargetDefeated = true;
	TestEqual(TEXT("a real lethal transition always resolves to lethal feedback"),
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(LethalTransitionEvent).ImpactTier,
		EGameXXKBattlePresentationImpactTier::Lethal);

	FGameXXKBattlePresentationEvent AvoidedEvent = LethalTransitionEvent;
	AvoidedEvent.HitOrdinal = 3;
	AvoidedEvent.bAvoided = true;
	const FGameXXKBattlePresentationRhythm Avoided =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(AvoidedEvent);
	TestTrue(TEXT("avoid rhythm is valid"), Avoided.IsValid());
	TestEqual(TEXT("avoid overrides damage and lethal feedback"), Avoided.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Avoided);
	ExpectSeconds(TEXT("avoid lasts zero-point-four-five seconds"), Avoided.DurationSeconds, 0.45f);
	ExpectSeconds(TEXT("avoid marker occurs at zero-point-sixteen seconds"), Avoided.ImpactSeconds, 0.16f);
	ExpectSeconds(TEXT("avoid never shakes horizontally"), Avoided.ShakeAmplitude.X, 0.0f);
	ExpectSeconds(TEXT("avoid never shakes vertically"), Avoided.ShakeAmplitude.Y, 0.0f);
	ExpectSeconds(TEXT("avoid has no shake duration"), Avoided.ShakeDurationSeconds, 0.0f);
	ExpectSeconds(TEXT("avoid readout peaks at one-point-one"), Avoided.ReadoutPeakScale, 1.10f);

	const FGameXXKBattlePresentationRhythm Death =
		FGameXXKBattleAnimationPresentation::ResolveDeathRhythm();
	TestTrue(TEXT("death rhythm is valid"), Death.IsValid());
	ExpectSeconds(TEXT("death lasts zero-point-nine seconds"), Death.DurationSeconds, 0.90f);
	const FGameXXKBattlePresentationRhythm Status =
		FGameXXKBattleAnimationPresentation::ResolveStatusRhythm();
	TestTrue(TEXT("status rhythm is valid"), Status.IsValid());
	ExpectSeconds(TEXT("status pulse lasts zero-point-three seconds"), Status.DurationSeconds, 0.30f);

	FGameXXKBattlePresentationEvent InvalidMagnitudeEvent;
	InvalidMagnitudeEvent.HitOrdinal = -7;
	InvalidMagnitudeEvent.HealthDamage = MIN_int32;
	InvalidMagnitudeEvent.TargetHealthBefore = 0;
	InvalidMagnitudeEvent.TargetHealthAfter = MAX_int32;
	const FGameXXKBattlePresentationRhythm Sanitized =
		FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(InvalidMagnitudeEvent);
	TestTrue(TEXT("invalid health and negative damage still resolve to finite safe rhythm"), Sanitized.IsValid());
	TestEqual(TEXT("negative damage clamps to the lightest non-avoid tier"), Sanitized.ImpactTier,
		EGameXXKBattlePresentationImpactTier::Light);

	const FGameXXKBattleAnimationClipDescriptor SourceClip =
		FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Player"), false, EGameXXKBattleAnimationAction::Attack);
	const FGameXXKBattleAnimationClipDescriptor FittedFirst =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(SourceClip, FirstLight.DurationSeconds);
	TestTrue(TEXT("duration-fitted first attack remains a valid clip"), FittedFirst.IsValid());
	ExpectSeconds(TEXT("duration-fitted first attack occupies exactly the rhythm duration"),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(FittedFirst), 0.82f);
	TestEqual(TEXT("duration fitting preserves the source atlas path"), FittedFirst.TexturePath, SourceClip.TexturePath);
	TestEqual(TEXT("duration fitting preserves the source atlas frame count"), FittedFirst.FrameCount, SourceClip.FrameCount);
	TestEqual(TEXT("duration fitting preserves the source atlas columns"), FittedFirst.Columns, SourceClip.Columns);
	TestEqual(TEXT("duration fitting preserves the source atlas rows"), FittedFirst.Rows, SourceClip.Rows);
	TestEqual(TEXT("the fitted atlas reaches its final frame before the event boundary"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(FittedFirst, 0.819f, false),
		FittedFirst.FrameCount - 1);

	const FGameXXKBattleAnimationClipDescriptor FittedDeath =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(
			FGameXXKBattleAnimationPresentation::ResolveClip(
				TEXT("Player"), false, EGameXXKBattleAnimationAction::Death),
			Death.DurationSeconds);
	ExpectSeconds(TEXT("duration-fitted death occupies exactly zero-point-nine seconds"),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(FittedDeath), 0.90f);

	const FGameXXKBattleAnimationClipDescriptor NanFit =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(
			SourceClip, std::numeric_limits<float>::quiet_NaN());
	const FGameXXKBattleAnimationClipDescriptor InfiniteFit =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(
			SourceClip, std::numeric_limits<float>::infinity());
	const FGameXXKBattleAnimationClipDescriptor ZeroFit =
		FGameXXKBattleAnimationPresentation::FitClipToDuration(SourceClip, 0.0f);
	TestTrue(TEXT("NaN duration safely preserves a valid finite source clip"), NanFit.IsValid());
	TestTrue(TEXT("infinite duration safely preserves a valid finite source clip"), InfiniteFit.IsValid());
	TestTrue(TEXT("zero duration safely preserves a valid finite source clip"), ZeroFit.IsValid());
	ExpectSeconds(TEXT("NaN duration preserves source playback rate"), NanFit.PlaybackRate, SourceClip.PlaybackRate);
	ExpectSeconds(TEXT("infinite duration preserves source playback rate"), InfiniteFit.PlaybackRate, SourceClip.PlaybackRate);
	ExpectSeconds(TEXT("zero duration preserves source playback rate"), ZeroFit.PlaybackRate, SourceClip.PlaybackRate);

	return true;
}

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
	TestEqual(TEXT("attack actions play at two times speed"), HeroAttack.PlaybackRate, 2.0f);
	TestEqual(TEXT("a five-second source action occupies at most two-point-five runtime seconds"),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(HeroAttack), 2.5f);

	const FGameXXKBattleAnimationClipDescriptor HeroIdle =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Idle);
	const FGameXXKBattleAnimationClipDescriptor HeroHit =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Hit);
	const FGameXXKBattleAnimationClipDescriptor HeroDeath =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Death);
	TestEqual(TEXT("idle actions play at source speed"), HeroIdle.PlaybackRate, 1.0f);
	TestEqual(TEXT("hit actions play at two times speed"), HeroHit.PlaybackRate, 2.0f);
	TestEqual(TEXT("death actions play at source speed"), HeroDeath.PlaybackRate, 1.0f);
	TestEqual(TEXT("death actions retain their full five-second runtime"),
		FGameXXKBattleAnimationPresentation::GetRuntimeDuration(HeroDeath), 5.0f);

	const FGameXXKBattleAnimationClipDescriptor GenericImpact =
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Impact);
	const FGameXXKBattleAnimationClipDescriptor GenericBuff =
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Buff);
	const FGameXXKBattleAnimationClipDescriptor GenericDebuff =
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Debuff);
	TestTrue(TEXT("generic impact descriptor is valid"), GenericImpact.IsValid());
	TestTrue(TEXT("generic buff descriptor is valid"), GenericBuff.IsValid());
	TestTrue(TEXT("generic debuff descriptor is valid"), GenericDebuff.IsValid());
	TestEqual(TEXT("generic impact uses the exact approved atlas path"), GenericImpact.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_impact_ink_generic_atlas.T_impact_ink_generic_atlas")));
	TestEqual(TEXT("generic buff uses the exact approved atlas path"), GenericBuff.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_status_buff_generic_atlas.T_status_buff_generic_atlas")));
	TestEqual(TEXT("generic debuff uses the exact approved atlas path"), GenericDebuff.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_status_debuff_generic_atlas.T_status_debuff_generic_atlas")));
	TestEqual(TEXT("generic impact plays at four times speed"), GenericImpact.PlaybackRate, 4.0f);
	TestEqual(TEXT("generic buff plays at source speed"), GenericBuff.PlaybackRate, 1.0f);
	TestEqual(TEXT("generic debuff plays at source speed"), GenericDebuff.PlaybackRate, 1.0f);

	TestEqual(TEXT("the source 2.2-second marker lands at runtime 1.1 during two-times attack playback"),
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
	TestEqual(TEXT("negative elapsed time samples the first frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, -1.0f, true), 0);
	TestEqual(TEXT("raw frame sixty wraps to frame zero"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 2.5f, true), 0);
	TestEqual(TEXT("raw frame sixty clamps to frame fifty-nine"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 2.5f, false), 59);
	TestEqual(TEXT("raw frame sixty-three wraps to frame three"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 2.625f, true), 3);
	TestEqual(TEXT("raw frame sixty-three clamps to frame fifty-nine"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 2.625f, false), 59);
	TestEqual(TEXT("huge finite looping elapsed time stays inside the generated frame range"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 1.0e9f, true), 0);
	TestEqual(TEXT("huge finite non-looping elapsed time holds the last generated frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(HeroAttack, 1.0e9f, false), 59);
	TestEqual(TEXT("NaN elapsed time samples the first frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
			HeroAttack, std::numeric_limits<float>::quiet_NaN(), true), 0);
	TestEqual(TEXT("positive infinite elapsed time samples the first frame"),
		FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
			HeroAttack, std::numeric_limits<float>::infinity(), false), 0);

	const FBox2f FrameZeroUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 0);
	TestTrue(TEXT("frame zero begins at atlas column zero row zero"),
		FrameZeroUv.Min.Equals(FVector2f(0.0f, 0.0f), KINDA_SMALL_NUMBER));
	const FBox2f FrameSevenUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 7);
	TestTrue(TEXT("frame seven begins at atlas column seven row zero"),
		FrameSevenUv.Min.Equals(FVector2f(7.0f / 8.0f, 0.0f), KINDA_SMALL_NUMBER));
	const FBox2f FrameEightUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 8);
	TestTrue(TEXT("frame eight begins at atlas column zero row one"),
		FrameEightUv.Min.Equals(FVector2f(0.0f, 1.0f / 8.0f), KINDA_SMALL_NUMBER));
	const FBox2f FrameFiftyNineUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 59);
	TestTrue(TEXT("frame fifty-nine begins at atlas column three row seven"),
		FrameFiftyNineUv.Min.Equals(FVector2f(3.0f / 8.0f, 7.0f / 8.0f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("frame fifty-nine ends at atlas column four and bottom edge"),
		FrameFiftyNineUv.Max.Equals(FVector2f(4.0f / 8.0f, 1.0f), KINDA_SMALL_NUMBER));
	const FBox2f FrameSixtyUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 60);
	TestTrue(TEXT("direct frame sixty UV input clamps to generated frame fifty-nine"),
		FrameSixtyUv.Min.Equals(FrameFiftyNineUv.Min, KINDA_SMALL_NUMBER));
	const FBox2f FrameSixtyThreeUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 63);
	TestTrue(TEXT("direct frame sixty-three UV input clamps to generated frame fifty-nine"),
		FrameSixtyThreeUv.Min.Equals(FrameFiftyNineUv.Min, KINDA_SMALL_NUMBER));

	const FBox2f FrameNineUv = FGameXXKBattleAnimationPresentation::CalculateUvRegion(HeroAttack, 9);
	TestTrue(TEXT("frame nine begins at atlas column one row one"),
		FrameNineUv.Min.Equals(FVector2f(0.125f, 0.125f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("frame nine ends at atlas column two row two"),
		FrameNineUv.Max.Equals(FVector2f(0.25f, 0.25f), KINDA_SMALL_NUMBER));

	const FGameXXKBattleAnimationClipDescriptor MissingWolfAttack =
		FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Enemy.Ch2.GrayWolf"), true, EGameXXKBattleAnimationAction::Attack);
	TestFalse(TEXT("the one terminally failed gray-wolf attack is never presented as an imported clip"), MissingWolfAttack.IsValid());
	TestTrue(TEXT("gray-wolf idle remains available"),
		FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Enemy.Ch2.GrayWolf"), true, EGameXXKBattleAnimationAction::Idle).IsValid());
	TestTrue(TEXT("gray-wolf hit remains available"),
		FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Enemy.Ch2.GrayWolf"), true, EGameXXKBattleAnimationAction::Hit).IsValid());
	TestTrue(TEXT("gray-wolf death remains available"),
		FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Enemy.Ch2.GrayWolf"), true, EGameXXKBattleAnimationAction::Death).IsValid());
	TestEqual(TEXT("unknown Enemy_07 token intentionally uses the rooster fallback"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Enemy_07"), true),
		FString(TEXT("enemy_01_rooster")));
	TestTrue(TEXT("unknown Enemy_07 attack resolves through the rooster fallback"),
		FGameXXKBattleAnimationPresentation::ResolveClip(
			TEXT("Enemy_07"), true, EGameXXKBattleAnimationAction::Attack).IsValid());
	// Resolution-suffix convention: ".2K"/".1K" runtime tokens select sibling downscaled asset sets.
	TestEqual(TEXT("a 2K hero token resolves the 2K hero sibling asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Hero.Two.2K"), false),
		FString(TEXT("character_00_hero_2k")));
	TestEqual(TEXT("a 1K hero token resolves the 1K hero sibling asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Hero.Three.1K"), false),
		FString(TEXT("character_00_hero_1k")));
	TestEqual(TEXT("a 2K rooster token resolves the 2K rooster sibling asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Rooster.Two.2K"), true),
		FString(TEXT("enemy_01_rooster_2k")));
	TestEqual(TEXT("a 1K rooster token resolves the 1K rooster sibling asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Rooster.Three.1K"), true),
		FString(TEXT("enemy_01_rooster_1k")));
	TestEqual(TEXT("the unsuffixed pilot hero token keeps the original hero asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Hero.One"), false),
		FString(TEXT("character_00_hero")));
	TestEqual(TEXT("the unsuffixed pilot rooster token keeps the original rooster asset set"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Pilot.Rooster.One"), true),
		FString(TEXT("enemy_01_rooster")));
	TestEqual(TEXT("production unit ids never resolve through the suffix convention"),
		FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(TEXT("Player"), false),
		FString(TEXT("character_00_hero")));
	const FGameXXKBattleAnimationClipDescriptor AuthoritativeTigerAttack =
		FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			TEXT("OpaqueEnemy.P3"),
			TEXT("Enemy.Ch3.Tiger"),
			true,
			EGameXXKBattleAnimationAction::Attack);
	TestEqual(TEXT("enemy attacks prefer the authoritative definition instead of the legacy runtime-id fallback"),
		AuthoritativeTigerAttack.AssetId,
		FString(TEXT("enemy_21_tiger_boss_attack")));
	TestEqual(TEXT("the authoritative enemy attack resolves the production tiger close-up atlas"),
		AuthoritativeTigerAttack.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_enemy_21_tiger_boss_attack_atlas.T_enemy_21_tiger_boss_attack_atlas")));

	FGameXXKBattleAnimationClipDescriptor InvalidDescriptor = HeroAttack;
	InvalidDescriptor.AssetId.Reset();
	TestFalse(TEXT("empty asset ID invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.TexturePath = FSoftObjectPath();
	TestFalse(TEXT("empty texture path invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.FrameCount = 0;
	TestFalse(TEXT("zero frame count invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.Columns = 0;
	TestFalse(TEXT("zero columns invalidate a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.Rows = 0;
	TestFalse(TEXT("zero rows invalidate a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.SourceFramesPerSecond = 0.0f;
	TestFalse(TEXT("zero source FPS invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.PlaybackRate = 0.0f;
	TestFalse(TEXT("zero playback rate invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.FrameCount = 65;
	TestFalse(TEXT("frame counts exceeding atlas capacity invalidate a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.SourceFramesPerSecond = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("infinite source FPS invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.PlaybackRate = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("infinite playback rate invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.SourceFramesPerSecond = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN source FPS invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	InvalidDescriptor = HeroAttack;
	InvalidDescriptor.PlaybackRate = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("NaN playback rate invalidates a clip descriptor"), InvalidDescriptor.IsValid());
	FGameXXKBattleAnimationClipDescriptor HugeGridDescriptor = HeroAttack;
	HugeGridDescriptor.FrameCount = MAX_int32;
	HugeGridDescriptor.Columns = MAX_int32;
	HugeGridDescriptor.Rows = MAX_int32;
	TestTrue(TEXT("atlas capacity validation does not overflow int32 multiplication"), HugeGridDescriptor.IsValid());

	TArray<FGameXXKCardCombatUnit> MultiHitUnits;
	MultiHitUnits.Add(MakePresentationUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, 100, 1));
	MultiHitUnits.Add(MakePresentationUnit(TEXT("Enemy.Ch1.Rooster"), EGameXXKCardTargetSide::Enemy, 100, 10));
	TestEqual(TEXT("multi-hit presentation fixture adds one agility layer"), GameXXKCardRules::AddCombatStatus(
		MultiHitUnits[1], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardDamageContext MultiHitContext;
	MultiHitContext.SourceUnitId = TEXT("Player");
	MultiHitContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> MultiHitGuardLinks;
	TArray<FGameXXKCardDamageResult> MultiHitResults;
	for (int32 HitIndex = 0; HitIndex < 3; ++HitIndex)
	{
		FGameXXKCardDamageResult& Result = MultiHitResults.AddDefaulted_GetRef();
		if (!TestTrue(
			FString::Printf(TEXT("presentation multi-hit packet %d resolves through real rules"), HitIndex),
			GameXXKCardRules::ApplyCombatDirectDamage(
				MultiHitUnits,
				MultiHitGuardLinks,
				MultiHitContext,
				TEXT("Enemy.Ch1.Rooster"),
				14,
				Result)))
		{
			return false;
		}
	}
	FGameXXKCardBattleRuntime PostDamageBattle;
	PostDamageBattle.Units = MultiHitUnits;
	const TArray<FGameXXKBattlePresentationEvent> MultiHitEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			PostDamageBattle,
			TEXT("UnusedFallback"),
			MultiHitResults);
	TestEqual(TEXT("each real damage packet remains a distinct immutable presentation event"), MultiHitEvents.Num(), 3);
	if (MultiHitEvents.Num() == 3)
	{
		TestEqual(TEXT("first event gets a deterministic nonzero batch ID"), MultiHitEvents[0].EventId, static_cast<uint64>(1));
		TestEqual(TEXT("second event gets the next deterministic batch ID"), MultiHitEvents[1].EventId, static_cast<uint64>(2));
		TestEqual(TEXT("third event gets the final deterministic batch ID"), MultiHitEvents[2].EventId, static_cast<uint64>(3));
		TestEqual(TEXT("first event preserves original result ordinal"), MultiHitEvents[0].HitOrdinal, 0);
		TestEqual(TEXT("third event preserves original result ordinal"), MultiHitEvents[2].HitOrdinal, 2);
		TestEqual(TEXT("damage source wins over the unused fallback attacker"), MultiHitEvents[0].AttackerUnitId, FName(TEXT("Player")));
		TestEqual(TEXT("resolved target becomes the presentation target"), MultiHitEvents[0].TargetUnitId, FName(TEXT("Enemy.Ch1.Rooster")));
		TestFalse(TEXT("party source is routed as a character"), MultiHitEvents[0].bAttackerEnemy);
		TestTrue(TEXT("enemy target is routed as a monster"), MultiHitEvents[0].bTargetEnemy);
		TestEqual(TEXT("first hit health before"), MultiHitEvents[0].TargetHealthBefore, 100);
		TestEqual(TEXT("first hit health after"), MultiHitEvents[0].TargetHealthAfter, 100);
		TestTrue(TEXT("first hit avoided"), MultiHitEvents[0].bAvoided);
		TestEqual(TEXT("second hit health before"), MultiHitEvents[1].TargetHealthBefore, 100);
		TestEqual(TEXT("second hit health after"), MultiHitEvents[1].TargetHealthAfter, 86);
		TestEqual(TEXT("third hit health before"), MultiHitEvents[2].TargetHealthBefore, 86);
		TestEqual(TEXT("third hit health after"), MultiHitEvents[2].TargetHealthAfter, 72);
		TestEqual(TEXT("presentation copies health damage"), MultiHitEvents[1].HealthDamage, 14);
		TestFalse(TEXT("nonlethal multi-hit packets never infer death from post-state"), MultiHitEvents[2].bTargetDefeated);
	}

	TArray<FGameXXKCardCombatUnit> ArmoredUnits;
	ArmoredUnits.Add(MakePresentationUnit(TEXT("ArmoredAttacker"), EGameXXKCardTargetSide::Party, 100, 1));
	ArmoredUnits.Add(MakePresentationUnit(TEXT("ArmoredTarget"), EGameXXKCardTargetSide::Enemy, 100, 10));
	ArmoredUnits[1].Armor = 12;
	FGameXXKCardDamageContext ArmoredContext;
	ArmoredContext.SourceUnitId = TEXT("ArmoredAttacker");
	ArmoredContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> ArmoredGuardLinks;
	TArray<FGameXXKCardDamageResult> ArmoredResults;
	for (const int32 Damage : {5, 11})
	{
		FGameXXKCardDamageResult& Result = ArmoredResults.AddDefaulted_GetRef();
		if (!TestTrue(TEXT("armored presentation packet resolves through real rules"),
			GameXXKCardRules::ApplyCombatDirectDamage(
				ArmoredUnits, ArmoredGuardLinks, ArmoredContext, TEXT("ArmoredTarget"), Damage, Result)))
		{
			return false;
		}
	}
	FGameXXKCardBattleRuntime ArmoredPostState;
	ArmoredPostState.Units = ArmoredUnits;
	const TArray<FGameXXKBattlePresentationEvent> ArmoredEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			ArmoredPostState, NAME_None, ArmoredResults);
	TestEqual(TEXT("two armored packets remain two presentation events"), ArmoredEvents.Num(), 2);
	if (ArmoredEvents.Num() == 2)
	{
		TestEqual(TEXT("first armored packet snapshots twelve armor before impact"), ArmoredEvents[0].TargetArmorBefore, 12);
		TestEqual(TEXT("first armored packet leaves seven armor"), ArmoredEvents[0].TargetArmorAfter, 7);
		TestEqual(TEXT("first armored packet reports five absorbed"), ArmoredEvents[0].ArmorAbsorbed, 5);
		TestEqual(TEXT("first rules packet stores twelve armor before impact"), ArmoredResults[0].TargetArmorBefore, 12);
		TestEqual(TEXT("first rules packet stores seven armor after impact"), ArmoredResults[0].TargetArmorAfter, 7);
		TestEqual(TEXT("second armored packet begins from seven armor"), ArmoredEvents[1].TargetArmorBefore, 7);
		TestEqual(TEXT("second armored packet drains the remaining armor"), ArmoredEvents[1].TargetArmorAfter, 0);
		TestEqual(TEXT("second armored packet reports seven absorbed"), ArmoredEvents[1].ArmorAbsorbed, 7);
		TestEqual(TEXT("second armored packet preserves four health damage"), ArmoredEvents[1].HealthDamage, 4);
	}

	TArray<FGameXXKCardCombatUnit> RedirectUnits;
	RedirectUnits.Add(MakePresentationUnit(TEXT("ProtectedHero"), EGameXXKCardTargetSide::Party, 100, 1));
	RedirectUnits.Add(MakePresentationUnit(TEXT("ResolvedGuardian"), EGameXXKCardTargetSide::Party, 73, 2));
	RedirectUnits.Add(MakePresentationUnit(TEXT("RedirectEnemy"), EGameXXKCardTargetSide::Enemy, 100, 10));
	FGameXXKCardGuardLinkRuntime RedirectLink;
	RedirectLink.GuardianUnitId = TEXT("ResolvedGuardian");
	RedirectLink.ProtectedUnitId = TEXT("ProtectedHero");
	RedirectLink.Stacks = 1;
	RedirectLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> RedirectLinks = {RedirectLink};
	FGameXXKCardDamageContext RedirectContext;
	RedirectContext.SourceUnitId = TEXT("RedirectEnemy");
	RedirectContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult RedirectResult;
	if (!TestTrue(TEXT("presentation redirect fixture resolves through real guard rules"), GameXXKCardRules::ApplyCombatDirectDamage(
		RedirectUnits, RedirectLinks, RedirectContext, TEXT("ProtectedHero"), 5, RedirectResult)))
	{
		return false;
	}
	FGameXXKCardBattleRuntime RedirectPostState;
	RedirectPostState.Units = RedirectUnits;
	const TArray<FGameXXKBattlePresentationEvent> RedirectEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			RedirectPostState, TEXT("UnusedFallback"), {RedirectResult});
	TestEqual(TEXT("one real redirect creates one presentation event"), RedirectEvents.Num(), 1);
	if (RedirectEvents.Num() == 1)
	{
		TestEqual(TEXT("presentation targets the resolved guardian instead of the protected original"),
			RedirectEvents[0].TargetUnitId, FName(TEXT("ResolvedGuardian")));
		TestEqual(TEXT("presentation copies the resolved guardian health before"), RedirectEvents[0].TargetHealthBefore, 73);
		TestEqual(TEXT("presentation copies the resolved guardian health after"), RedirectEvents[0].TargetHealthAfter, 68);
		TestTrue(TEXT("redirect attacker side resolves from its final source ID"), RedirectEvents[0].bAttackerEnemy);
		TestFalse(TEXT("redirect target side resolves from the guardian final ID"), RedirectEvents[0].bTargetEnemy);
	}

	TArray<FGameXXKCardCombatUnit> LethalUnits;
	LethalUnits.Add(MakePresentationUnit(TEXT("LethalAttacker"), EGameXXKCardTargetSide::Party, 100, 1));
	LethalUnits.Add(MakePresentationUnit(TEXT("LethalTarget"), EGameXXKCardTargetSide::Enemy, 20, 10));
	FGameXXKCardDamageContext LethalContext;
	LethalContext.SourceUnitId = TEXT("LethalAttacker");
	LethalContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> LethalGuardLinks;
	TArray<FGameXXKCardDamageResult> LethalResults;
	FGameXXKCardDamageResult& NonlethalResult = LethalResults.AddDefaulted_GetRef();
	if (!TestTrue(TEXT("pre-lethal presentation packet resolves through real rules"), GameXXKCardRules::ApplyCombatDirectDamage(
		LethalUnits, LethalGuardLinks, LethalContext, TEXT("LethalTarget"), 8, NonlethalResult)))
	{
		return false;
	}
	FGameXXKCardDamageResult& LethalResult = LethalResults.AddDefaulted_GetRef();
	if (!TestTrue(TEXT("lethal presentation packet resolves through real rules"), GameXXKCardRules::ApplyCombatDirectDamage(
		LethalUnits, LethalGuardLinks, LethalContext, TEXT("LethalTarget"), 50, LethalResult)))
	{
		return false;
	}
	FGameXXKCardBattleRuntime LethalPostState;
	LethalPostState.Units = LethalUnits;
	const TArray<FGameXXKBattlePresentationEvent> LethalEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			LethalPostState, TEXT("UnusedFallback"), LethalResults);
	TestEqual(TEXT("real nonlethal and lethal packets both remain in presentation order"), LethalEvents.Num(), 2);
	if (LethalEvents.Num() == 2)
	{
		TestFalse(TEXT("post-state defeat never marks the earlier nonlethal transition"), LethalEvents[0].bTargetDefeated);
		TestTrue(TEXT("the actual positive-to-zero snapshot marks the lethal transition"), LethalEvents[1].bTargetDefeated);
		TestEqual(TEXT("lethal event snapshots its positive boundary"), LethalEvents[1].TargetHealthBefore, 12);
		TestEqual(TEXT("lethal event snapshots its zero boundary"), LethalEvents[1].TargetHealthAfter, 0);
	}
	const TArray<FGameXXKBattlePresentationEvent> PreLethalOnlyEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			LethalPostState, TEXT("UnusedFallback"), {LethalResults[0]});
	TestEqual(TEXT("one included pre-lethal packet remains one event"), PreLethalOnlyEvents.Num(), 1);
	if (PreLethalOnlyEvents.Num() == 1)
	{
		TestFalse(TEXT("later post-state death never rewrites a nonlethal immutable snapshot"),
			PreLethalOnlyEvents[0].bTargetDefeated);
	}

	FGameXXKCardBattleRuntime FallbackPostState;
	FallbackPostState.Units.Add(MakePresentationUnit(TEXT("FallbackAttacker"), EGameXXKCardTargetSide::Party, 100, 1));
	FallbackPostState.Units.Add(MakePresentationUnit(TEXT("OriginalOnlyTarget"), EGameXXKCardTargetSide::Enemy, 20, 10));
	FGameXXKCardDamageResult MissingTargetResult;
	FGameXXKCardDamageResult OriginalOnlyResult;
	OriginalOnlyResult.OriginalTargetUnitId = TEXT("OriginalOnlyTarget");
	OriginalOnlyResult.HealthDamage = 10;
	OriginalOnlyResult.TargetHealthBefore = 30;
	OriginalOnlyResult.TargetHealthAfter = 20;
	const TArray<FGameXXKBattlePresentationEvent> FallbackEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(
			FallbackPostState,
			TEXT("FallbackAttacker"),
			{MissingTargetResult, OriginalOnlyResult});
	TestEqual(TEXT("a damage result without either target identity is omitted"), FallbackEvents.Num(), 1);
	if (FallbackEvents.Num() == 1)
	{
		TestEqual(TEXT("event ID remains tied to the original result index"), FallbackEvents[0].EventId, static_cast<uint64>(2));
		TestEqual(TEXT("hit ordinal remains tied to the original result index"), FallbackEvents[0].HitOrdinal, 1);
		TestEqual(TEXT("empty damage source remains target-only despite a legacy fallback"), FallbackEvents[0].AttackerUnitId, NAME_None);
		TestEqual(TEXT("empty resolved target uses the original target"), FallbackEvents[0].TargetUnitId, FName(TEXT("OriginalOnlyTarget")));
		TestFalse(TEXT("target-only canonical event never invents an attacker side"), FallbackEvents[0].bAttackerEnemy);
		TestTrue(TEXT("original-only target side resolves from final stable identity"), FallbackEvents[0].bTargetEnemy);
	}
	const TArray<FGameXXKBattleAnimationCombatRequest> LegacyFallbackRequests =
		FGameXXKBattleAnimationPresentation::BuildCombatRequests(
			FallbackPostState,
			TEXT("FallbackAttacker"),
			{MissingTargetResult, OriginalOnlyResult});
	TestEqual(TEXT("the dormant legacy wrapper still adapts source-less damage to its explicit fallback"),
		LegacyFallbackRequests.Num() == 1 ? LegacyFallbackRequests[0].AttackerUnitId : NAME_None,
		FName(TEXT("FallbackAttacker")));

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
