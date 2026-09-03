#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCombatSimulationRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EGameXXKEquipmentSlot SimulationOrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	constexpr EGameXXKEquipmentSet SimulationModernSets[] = {
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentSet::XuanJia,
		EGameXXKEquipmentSet::QingNang,
		EGameXXKEquipmentSet::ZhuiFeng,
		EGameXXKEquipmentSet::ShiGu,
		EGameXXKEquipmentSet::ShanHe};

	constexpr EGameXXKEquipmentQuality SimulationQualities[] = {
		EGameXXKEquipmentQuality::Common,
		EGameXXKEquipmentQuality::Rare,
		EGameXXKEquipmentQuality::Epic};

	FGameXXKBattleRuntimeUnit MakeSimulationEnemy()
	{
		FGameXXKBattleRuntimeUnit Enemy;
		Enemy.Id = TEXT("MoneyRat");
		Enemy.DisplayName = FText::FromString(TEXT("金钱鼠"));
		Enemy.HP = 36;
		Enemy.MaxHP = 36;
		Enemy.MP = 0;
		Enemy.MaxMP = 0;
		Enemy.Attack = 6;
		Enemy.Defense = 0;
		Enemy.Speed = 8;
		Enemy.bEnemy = true;
		return Enemy;
	}

	FGameXXKSimulationScenario MakeScenario(const int32 Seed)
	{
		FGameXXKSimulationScenario Scenario;
		Scenario.Seed = Seed;
		Scenario.NodeKind = EGameXXKNodeKind::Battle;
		Scenario.Terrain = EGameXXKCardTerrain::Plain;
		Scenario.Policy = EGameXXKSimulationPolicy::Skilled;
		Scenario.MaxRounds = 30;
		Scenario.MaxDecisions = 500;
		Scenario.InitialRuntimeState =
			GameXXKPermanentPartyTestFixtures::MakeStartedState();
		for (FGameXXKPermanentCompanion& Companion :
			Scenario.InitialRuntimeState.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (!Companion.bIsActive)
			{
				continue;
			}
			TArray<FName> RebuiltFullCards;
			FString RebuildError;
			if (FGameXXKCompanionRules::BuildFullProfessionCardPool(
				Companion.Role,
				Companion.CardSeed,
				RebuiltFullCards,
				&RebuildError))
			{
				Companion.PersonalCardIds = MoveTemp(RebuiltFullCards);
			}
			break;
		}
		Scenario.InitialRuntimeState.ActiveBattleNodeId = INDEX_NONE;
		Scenario.InitialRuntimeState.ActiveBattleEnemies = {MakeSimulationEnemy()};
		Scenario.InitialRuntimeState.bHasActiveBattle = true;
		return Scenario;
	}

	bool PrepareHeroEquipmentScenario(
		const int32 Seed,
		const int32 HeroLevel,
		const EGameXXKEquipmentSet Set,
		const EGameXXKEquipmentQuality Quality,
		const bool bEnhanceToMaximum,
		FGameXXKSimulationScenario& OutScenario,
		FString& OutError)
	{
		OutScenario = MakeScenario(Seed);
		FGameXXKRuntimeState& State = OutScenario.InitialRuntimeState;
		State.Screen = EGameXXKScreen::Town;
		State.PlayerLevel = HeroLevel;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &OutError)
			|| !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Could not initialize the hero equipment simulation fixture.");
			}
			return false;
		}

		State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemEnhancementStone()) = 1000000;
		for (const EGameXXKEquipmentSlot Slot : SimulationOrderedSlots)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = Set;
			Request.Quality = Quality;
			Request.ItemLevel = HeroLevel;
			Request.bForceSlot = true;
			Request.ForcedSlot = Slot;
			FName InstanceId;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &OutError))
			{
				return false;
			}
			if (bEnhanceToMaximum)
			{
				for (int32 EnhancementIndex = 0; EnhancementIndex < FGameXXKEquipmentRules::MaxEnhancementLevel; ++EnhancementIndex)
				{
					FGameXXKEquipmentTransactionResult Enhancement;
					if (!FGameXXKEquipmentEconomyRules::EnhanceInstance(State, InstanceId, Enhancement))
					{
						OutError = Enhancement.Message.ToString();
						return false;
					}
				}
			}

			FGameXXKEquipmentTransactionResult Equip;
			if (!FGameXXKEquipmentEconomyRules::Equip(State, FGameXXKEquipmentRules::HeroCharacterId(), Slot, InstanceId, Equip))
			{
				OutError = Equip.Message.ToString();
				return false;
			}
		}
		return FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
			State.EquipmentCollection,
			State.CardRun.CompanionRoster,
			&OutError);
	}

	TArray<uint8> SerializeSimulationResult(
		const FGameXXKSimulationMetrics& Metrics,
		const TArray<FGameXXKSimulationTraceEntry>& Trace)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKSimulationMetrics MetricsCopy = Metrics;
		FGameXXKSimulationMetrics::StaticStruct()->SerializeItem(Archive, &MetricsCopy, nullptr);
		int32 TraceCount = Trace.Num();
		Archive << TraceCount;
		for (const FGameXXKSimulationTraceEntry& Entry : Trace)
		{
			FGameXXKSimulationTraceEntry EntryCopy = Entry;
			FGameXXKSimulationTraceEntry::StaticStruct()->SerializeItem(Archive, &EntryCopy, nullptr);
		}
		return Bytes;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatSimulationFoundationTest,
	"GameXXK.Simulation.Foundation.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatSimulationFoundationTest::RunTest(const FString& Parameters)
{
	const FGameXXKSimulationScenario Scenario = MakeScenario(20260723);
	const FGameXXKRuntimeState OriginalInput = Scenario.InitialRuntimeState;
	FGameXXKSimulationMetrics FirstMetrics;
	FGameXXKSimulationMetrics SecondMetrics;
	TArray<FGameXXKSimulationTraceEntry> FirstTrace;
	TArray<FGameXXKSimulationTraceEntry> SecondTrace;
	FString Error;
	TestTrue(FString::Printf(TEXT("simulation completes a finite real-rule battle: %s"), *Error),
		FGameXXKCombatSimulationRules::RunScenario(Scenario, FirstMetrics, FirstTrace, &Error));
	TestTrue(FString::Printf(TEXT("same scenario and seed repeat exactly: %s"), *Error),
		FGameXXKCombatSimulationRules::RunScenario(Scenario, SecondMetrics, SecondTrace, &Error));
	TestTrue(TEXT("simulation finishes in a terminal victory or defeat state"), FirstMetrics.bVictory || FirstMetrics.FailureReason == TEXT("Simulation.Defeat"));
	TestTrue(TEXT("simulation records a positive finite round count"), FirstMetrics.Rounds > 0 && FirstMetrics.Rounds <= Scenario.MaxRounds);
	TestTrue(TEXT("simulation emits a non-empty authoritative trace"), !FirstTrace.IsEmpty());
	TestEqual(TEXT("same scenario and seed produce byte-identical metrics and trace"),
		SerializeSimulationResult(FirstMetrics, FirstTrace), SerializeSimulationResult(SecondMetrics, SecondTrace));
	TestTrue(TEXT("simulation never mutates the scenario input"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&OriginalInput, &Scenario.InitialRuntimeState, PPF_None));
	TestEqual(TEXT("simulation records the requested terrain identity"), FirstMetrics.Terrain, Scenario.Terrain);
	TestTrue(TEXT("initial and newly drawn hand cards are attributed by CardId"), !FirstMetrics.CardsSeenById.IsEmpty());
	TestTrue(TEXT("active plays are attributed by CardId"), !FirstMetrics.CardsPlayedById.IsEmpty());
	int64 TotalAttributedActivePlays = 0;
	for (const TPair<FName, int64>& Pair : FirstMetrics.CardsPlayedById)
	{
		TotalAttributedActivePlays += Pair.Value;
	}
	TestEqual(TEXT("automatic replays never masquerade as active card plays"),
		TotalAttributedActivePlays,
		static_cast<int64>(FirstMetrics.ActivelyPlayedCards));
	TestTrue(TEXT("effective enemy damage is attributed to the active CardId"), !FirstMetrics.DamageByCardId.IsEmpty());
	TestTrue(TEXT("phase-end unused energy is always non-negative"), FirstMetrics.EnergyUnspentAtPhaseEnd >= 0);
	TestTrue(TEXT("phase-end unused living-party Mana is always non-negative"), FirstMetrics.ManaUnspentAtPhaseEnd >= 0);
	TestTrue(TEXT("overkill is always non-negative"), FirstMetrics.OverkillDamage >= 0);
	TestTrue(TEXT("overhealing is always non-negative"), FirstMetrics.Overhealing >= 0);

	const bool bRecordedEnemyDamage = FirstTrace.ContainsByPredicate([](const FGameXXKSimulationTraceEntry& Entry)
	{
		return Entry.Action == TEXT("PlayCard") && Entry.HealthDelta < 0;
	});
	TestTrue(TEXT("a player damage card records its authoritative negative total-health delta"), bRecordedEnemyDamage);

	FGameXXKSimulationScenario OverkillScenario = MakeScenario(20260724);
	OverkillScenario.InitialRuntimeState.ActiveBattleEnemies[0].HP = 1;
	OverkillScenario.InitialRuntimeState.ActiveBattleEnemies[0].MaxHP = 1;
	OverkillScenario.InitialRuntimeState.ActiveBattleEnemies[0].Attack = 0;
	FGameXXKSimulationMetrics OverkillMetrics;
	TArray<FGameXXKSimulationTraceEntry> OverkillTrace;
	Error.Reset();
	TestTrue(FString::Printf(TEXT("one-health fixed battle resolves for overkill attribution: %s"), *Error),
		FGameXXKCombatSimulationRules::RunScenario(OverkillScenario, OverkillMetrics, OverkillTrace, &Error));
	TestTrue(TEXT("damage beyond the one-health enemy is recorded as overkill"), OverkillMetrics.OverkillDamage > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatSimulationBenchmarkTest,
	"GameXXK.Simulation.Benchmark.NakedHundred",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatSimulationBenchmarkTest::RunTest(const FString& Parameters)
{
	constexpr int32 ScenarioCount = 100;
	const double BatchStartSeconds = FPlatformTime::Seconds();
	double FirstScenarioSeconds = 0.0;
	bool bAllFinished = true;
	for (int32 Seed = 1; Seed <= ScenarioCount; ++Seed)
	{
		FGameXXKSimulationMetrics Metrics;
		TArray<FGameXXKSimulationTraceEntry> Trace;
		FString Error;
		const double ScenarioStartSeconds = FPlatformTime::Seconds();
		const bool bFinished = FGameXXKCombatSimulationRules::RunScenario(MakeScenario(Seed), Metrics, Trace, &Error);
		const double ScenarioSeconds = FPlatformTime::Seconds() - ScenarioStartSeconds;
		if (Seed == 1)
		{
			FirstScenarioSeconds = ScenarioSeconds;
		}
		if (!bFinished)
		{
			bAllFinished = false;
			AddError(FString::Printf(TEXT("seed %d failed: %s"), Seed, *Error));
		}
	}
	const double BatchSeconds = FPlatformTime::Seconds() - BatchStartSeconds;
	AddInfo(FString::Printf(TEXT("[SimulationBenchmark] naked first=%.6fs, 100=%.6fs, projected2400=%.3fs"),
		FirstScenarioSeconds,
		BatchSeconds,
		BatchSeconds * 24.0));
	TestTrue(TEXT("all 100 naked real-rule scenarios finish without a runner failure"), bAllFinished);
	TestTrue(TEXT("one naked real-rule battle is fast enough for interactive balance iteration"), FirstScenarioSeconds < 1.0);
	TestTrue(TEXT("100 naked real-rule scenarios leave a practical 2400-scenario budget under 30 minutes"), BatchSeconds * 24.0 < 1800.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatSimulationEquipmentMatrixTest,
	"GameXXK.Simulation.Foundation.EquipmentMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatSimulationEquipmentMatrixTest::RunTest(const FString& Parameters)
{
	constexpr int32 Levels[] = {1, 5, 10, 15, 20};
	int32 ScenarioCount = 0;
	int32 ScenarioSeed = 42000;
	const double StartSeconds = FPlatformTime::Seconds();
	const auto VerifyScenario = [this, &ScenarioCount, &ScenarioSeed](FGameXXKSimulationScenario&& Scenario, const FString& Label)
	{
		FGameXXKSimulationMetrics FirstMetrics;
		FGameXXKSimulationMetrics SecondMetrics;
		TArray<FGameXXKSimulationTraceEntry> FirstTrace;
		TArray<FGameXXKSimulationTraceEntry> SecondTrace;
		FString Error;
		const bool bFirstFinished = FGameXXKCombatSimulationRules::RunScenario(Scenario, FirstMetrics, FirstTrace, &Error);
		TestTrue(FString::Printf(TEXT("%s first real-rule simulation completes: %s"), *Label, *Error), bFirstFinished);
		const bool bSecondFinished = FGameXXKCombatSimulationRules::RunScenario(Scenario, SecondMetrics, SecondTrace, &Error);
		TestTrue(FString::Printf(TEXT("%s repeat real-rule simulation completes: %s"), *Label, *Error), bSecondFinished);
		TestTrue(FString::Printf(TEXT("%s ends in victory or a normal defeat"), *Label),
			FirstMetrics.bVictory || FirstMetrics.FailureReason == TEXT("Simulation.Defeat"));
		TestTrue(FString::Printf(TEXT("%s stays within its finite round bound"), *Label),
			FirstMetrics.Rounds > 0 && FirstMetrics.Rounds <= Scenario.MaxRounds);
		TestEqual(FString::Printf(TEXT("%s is byte-repeatable"), *Label),
			SerializeSimulationResult(FirstMetrics, FirstTrace),
			SerializeSimulationResult(SecondMetrics, SecondTrace));
		++ScenarioCount;
		++ScenarioSeed;
	};

	for (const int32 Level : Levels)
	{
		FGameXXKSimulationScenario Naked = MakeScenario(ScenarioSeed);
		Naked.InitialRuntimeState.PlayerLevel = Level;
		VerifyScenario(MoveTemp(Naked), FString::Printf(TEXT("naked level %d"), Level));
		for (const EGameXXKEquipmentSet Set : SimulationModernSets)
		{
			for (const EGameXXKEquipmentQuality Quality : SimulationQualities)
			{
				for (const bool bEnhanced : {false, true})
				{
					FGameXXKSimulationScenario Scenario;
					FString FixtureError;
					const FString Label = FString::Printf(TEXT("set %d quality %d enhance %d level %d"),
						static_cast<int32>(Set),
						static_cast<int32>(Quality),
						bEnhanced ? FGameXXKEquipmentRules::MaxEnhancementLevel : 0,
						Level);
					if (!PrepareHeroEquipmentScenario(ScenarioSeed, Level, Set, Quality, bEnhanced, Scenario, FixtureError))
					{
						AddError(FString::Printf(TEXT("%s fixture failed: %s"), *Label, *FixtureError));
						++ScenarioSeed;
						continue;
					}
					VerifyScenario(MoveTemp(Scenario), Label);
				}
			}
		}
	}

	const double Seconds = FPlatformTime::Seconds() - StartSeconds;
	AddInfo(FString::Printf(TEXT("[SimulationMatrix] scenarios=%d, executions=%d, seconds=%.6f"), ScenarioCount, ScenarioCount * 2, Seconds));
	TestEqual(TEXT("the real-rule matrix covers every naked/set/quality/+10/level fixture"), ScenarioCount, 185);
	return true;
}

#endif
