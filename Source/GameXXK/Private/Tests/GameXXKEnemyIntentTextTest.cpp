#include "GameXXKEnemyText.h"

#include "GameXXKCardRunTypes.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentTextTest,
	"GameXXK.UI.Battle.EnemyIntent.ResolvedText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentTextTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	State.CardRun.bHasActiveCardBattle = true;
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	State.CardRun.ActiveBattle.EnemyDifficulty = EGameXXKEnemyDifficulty::Hell;
	State.CardRun.ActiveBattle.EnemyDifficultyDamagePercent = 150;
	FGameXXKCardCombatUnit Hero;
	Hero.UnitId = TEXT("Player");
	Hero.Side = EGameXXKCardTargetSide::Party;
	Hero.Role = EGameXXKCharacterRole::Hero;
	Hero.HP = 500;
	Hero.MaxHP = 500;
	Hero.Speed = 10;
	Hero.StableSortOrder = 1;
	Hero.CombatLevel = 100;
	Hero.bLiving = true;
	State.CardRun.ActiveBattle.Units.Add(Hero);

	FGameXXKCardEnemyIntent Intent;
	Intent.CardDisplayName = TEXT("血羽焚天");
	Intent.SourceUnitId = TEXT("Ironfeather");
	Intent.SuggestedTargetUnitId = TEXT("Player");
	Intent.TargetSlotNumber = 2;
	Intent.TargetRule = EGameXXKEnemyIntentTargetRule::AllLivingParty;
	Intent.IntentDefinitionId = TEXT("BloodFeatherBurnsSky");
	Intent.PhaseNumber = 3;
	Intent.TotalPhases = 3;
	Intent.PhaseLabel = TEXT("血羽不熄");
	FGameXXKResolvedEnemyIntentEffect& Damage = Intent.Effects.AddDefaulted_GetRef();
	Damage.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
	Damage.TargetRule = EGameXXKEnemyIntentTargetRule::AllLivingParty;
	Damage.TargetUnitIds = {TEXT("Player")};
	Damage.Magnitude = 105;
	Damage.HitCount = 1;
	Damage.Status = EGameXXKCardStatus::Burn;
	Damage.StatusStacks = 15;
	FGameXXKResolvedEnemyIntentEffect& Mark = Intent.Effects.AddDefaulted_GetRef();
	Mark.Type = EGameXXKEnemyIntentEffectType::ApplyStatus;
	Mark.TargetRule = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
	Mark.TargetUnitIds = {TEXT("Player")};
	Mark.Status = EGameXXKCardStatus::Mark;
	Mark.StatusStacks = 5;

	const FString Compact = FGameXXKEnemyText::FormatIntentCard(State, Intent);
	TestTrue(TEXT("compact text starts with the card name"), Compact.StartsWith(TEXT("血羽焚天\n")));
	TestTrue(TEXT("target scope occupies its own emphasized row"), Compact.Contains(TEXT("\n【我方全体】\n")));
	TestTrue(TEXT("compact damage is the resolved Hell number"), Compact.Contains(TEXT("158伤害")));
	TestTrue(TEXT("compact status uses its final level-scaled amount"), Compact.Contains(TEXT("灼烧15")));

	const FString Detail = FGameXXKEnemyText::FormatIntentTooltip(State, Intent);
	TestTrue(TEXT("detail names the exact phase"), Detail.Contains(TEXT("阶段：3/3 · 血羽不熄")));
	TestTrue(TEXT("detail keeps target scope separate"), Detail.Contains(TEXT("对象：我方全体")));
	TestTrue(TEXT("detail includes resolved on-hit status"), Detail.Contains(TEXT("命中附加灼烧15")));
	TestFalse(TEXT("enemy intent text leaves target armor settlement to the unit tooltip"), Detail.Contains(TEXT("护甲结算")));
	return true;
}

#endif
