#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 DifficultyPercent(const EGameXXKEnemyDifficulty Difficulty)
	{
		return Difficulty == EGameXXKEnemyDifficulty::Hell
			? 150
			: Difficulty == EGameXXKEnemyDifficulty::Hard ? 125 : 100;
	}

	bool BeginMatrixBattle(
		const FGameXXKEnemyDefinition& Definition,
		const EGameXXKEnemyDifficulty Difficulty,
		FGameXXKRuntimeState& OutState,
		FString& OutError)
	{
		OutState = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		OutState.PlayerLevel = 100;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}
		FGameXXKBattleRuntimeUnit Enemy;
		Enemy.Id = TEXT("MatrixEnemy");
		Enemy.DisplayName = Definition.DisplayName;
		Enemy.HP = 10000;
		Enemy.MaxHP = 10000;
		Enemy.Attack = 100;
		Enemy.Defense = 50;
		Enemy.Speed = Definition.Speed;
		Enemy.Shield = 0;
		Enemy.bEnemy = true;
		Enemy.EnemyDefinitionId = Definition.Id;
		Enemy.BattleSlotNumber = 1;
		Enemy.CombatLevel = 100;
		OutState.ActiveBattleEnemies = {Enemy};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = 9951;
		if (!FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			Definition.Tier == EGameXXKEnemyTier::Boss ? EGameXXKNodeKind::Boss : EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			9951,
			&OutError,
			DifficultyPercent(Difficulty)))
		{
			return false;
		}
		for (FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Unit.MaxHP = 100000;
				Unit.HP = 100000;
				Unit.Defense = 0;
				Unit.CombatLevel = 100;
				Unit.bLiving = true;
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Mark, 5);
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Weak, 5);
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Bleed, 20);
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Poison, 20);
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Burn, 20);
				GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Prey, 1);
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentMatrixTest,
	"GameXXK.Battle.EnemyApproved.All351Forecasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentMatrixTest::RunTest(const FString& Parameters)
{
	int32 ResolvedCases = 0;
	for (const EGameXXKEnemyDifficulty Difficulty : {
		EGameXXKEnemyDifficulty::Normal,
		EGameXXKEnemyDifficulty::Hard,
		EGameXXKEnemyDifficulty::Hell})
	{
		for (const FGameXXKEnemyDefinition& Definition : FGameXXKEnemyCatalog::GetAllDefinitions())
		{
			FString Error;
			FGameXXKRuntimeState Baseline;
			if (!TestTrue(Definition.Id.ToString() + TEXT(" matrix fixture begins"),
				BeginMatrixBattle(Definition, Difficulty, Baseline, Error)))
			{
				AddError(Error);
				return false;
			}
			const int32 TotalPhases = FGameXXKEnemyCatalog::ResolveTotalPhases(Definition.Tier, Difficulty);
			for (int32 PhaseNumber = 1; PhaseNumber <= TotalPhases; ++PhaseNumber)
			{
				const TArray<FGameXXKEnemyIntentDefinition>* Intents =
					FGameXXKEnemyCatalog::GetPhaseIntents(Definition, PhaseNumber);
				if (!TestNotNull(TEXT("matrix phase deck exists"), Intents))
				{
					return false;
				}
				for (const FGameXXKEnemyIntentDefinition& DefinitionIntent : *Intents)
				{
					FGameXXKRuntimeState State = Baseline;
					FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
					FGameXXKCardCombatUnit* Enemy = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
					{
						return Unit.UnitId == TEXT("MatrixEnemy");
					});
					FGameXXKCardCombatUnit* PartyTarget = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
					{
						return Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party;
					});
					if (!Enemy || !PartyTarget)
					{
						AddError(TEXT("matrix fixture lost its source or party target"));
						return false;
					}
					Enemy->HP = Enemy->MaxHP / 2;
					GameXXKCardRules::ConsumeCombatStatus(*Enemy, EGameXXKCardStatus::Wealth, MAX_int32);
					GameXXKCardRules::ConsumeCombatStatus(*Enemy, EGameXXKCardStatus::Rage, MAX_int32);
					GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Wealth, 8);
					GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Rage, 5);
					FGameXXKEnemyBattleState& EnemyState = Runtime.EnemyStates.FindChecked(TEXT("MatrixEnemy"));
					EnemyState.CurrentPhase = PhaseNumber;
					EnemyState.TotalPhases = TotalPhases;
					EnemyState.bPhaseTwo = PhaseNumber >= 2;
					EnemyState.IntentCursor = 0;
					EnemyState.HealingCooldownRounds = 0;
					EnemyState.PendingHealingAmplificationPercent = 6;
					EnemyState.PersistentTargetUnitId = PartyTarget->UnitId;
					EnemyState.PersistentTargetStatus = static_cast<uint8>(EGameXXKCardStatus::Prey);
					EnemyState.PendingChargedIntentId = NAME_None;
					EnemyState.ChargeRoundsRemaining = 0;
					EnemyState.PendingChargeTargetUnitIds.Reset();
					if (DefinitionIntent.ChargeRounds > 0)
					{
						EnemyState.PendingChargedIntentId = DefinitionIntent.Id;
						EnemyState.PendingChargeTargetUnitIds = {PartyTarget->UnitId};
					}
					Runtime.LockedEnemyIntents = {
						{TEXT("MatrixEnemy"), DefinitionIntent.Id, PhaseNumber, Runtime.RoundNumber}};
					State.CardRun.EnemyIntents.Reset();
					State.CardRun.NextEnemyIntentIndex = 0;
					const FString Label = FString::Printf(
						TEXT("%s P%d %s D%d"),
						*Definition.Id.ToString(),
						PhaseNumber,
						*DefinitionIntent.Id.ToString(),
						static_cast<int32>(Difficulty));
					if (!TestTrue(Label + TEXT(" forecast resolves"),
						FGameXXKCardBattleAdapter::RefreshEnemyIntentForecast(State, &Error)))
					{
						AddError(Label + TEXT(": ") + Error);
						return false;
					}
					if (!TestEqual(Label + TEXT(" produces one intent"), State.CardRun.EnemyIntents.Num(), 1))
					{
						return false;
					}
					const FGameXXKCardEnemyIntent& Intent = State.CardRun.EnemyIntents[0];
					TestEqual(Label + TEXT(" identity"), Intent.IntentDefinitionId, DefinitionIntent.Id);
					TestEqual(Label + TEXT(" phase"), Intent.PhaseNumber, PhaseNumber);
					TestEqual(Label + TEXT(" total phases"), Intent.TotalPhases, TotalPhases);
					TestTrue(Label + TEXT(" has resolved effects"), !Intent.Effects.IsEmpty());
					for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
					{
						if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
							|| Effect.Type == EGameXXKEnemyIntentEffectType::AddArmorDefensePercent)
						{
							TestTrue(Label + TEXT(" resolves a positive continuous value"), Effect.Magnitude > 0);
						}
						if (Effect.Type == EGameXXKEnemyIntentEffectType::ApplyStatus)
						{
							TestTrue(Label + TEXT(" resolves a positive status amount"), Effect.StatusStacks > 0);
						}
					}
					++ResolvedCases;
				}
			}
		}
	}
	TestEqual(TEXT("the runtime forecast matrix covers all approved cases"), ResolvedCases, 351);
	return true;
}

#endif
