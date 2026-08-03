#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "PaperFlipbook.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

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
		TestEqual(TEXT("empty damage source uses the explicit fallback attacker"), FallbackEvents[0].AttackerUnitId, FName(TEXT("FallbackAttacker")));
		TestEqual(TEXT("empty resolved target uses the original target"), FallbackEvents[0].TargetUnitId, FName(TEXT("OriginalOnlyTarget")));
		TestFalse(TEXT("fallback attacker side resolves from final stable identity"), FallbackEvents[0].bAttackerEnemy);
		TestTrue(TEXT("original-only target side resolves from final stable identity"), FallbackEvents[0].bTargetEnemy);
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
