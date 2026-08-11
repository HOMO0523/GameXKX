#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerCoreRuntimeTest
{
	const FName BladeUnitId(TEXT("BladePartner"));
	const FName EnemyUnitId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = BladeUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("BladePartner.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 10, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			61001,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("partner Blade runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("partner Blade fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool BuildRuntime(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& OutRuntime)
	{
		return BuildRuntime(Test, OutRuntime, {
			MakeCard(TEXT("LieFeng"), TEXT("Profession.Blade.LieFengZhan"), 0),
			MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1)});
	}

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	int32 Status(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId,
		const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetUnitId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}

	bool EndRoundAndBeginNext(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		if (!Test.TestTrue(FString::Printf(TEXT("player phase ends: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		Error.Reset();
		if (!Test.TestTrue(FString::Printf(TEXT("next player round begins: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error)))
		{
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerLieFengChargeReplayTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.LieFengChargeReplaysNextActiveBaseOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladePartnerLieFengChargeReplayTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCoreRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("LieFeng"), EnemyUnitId, Result, TEXT("Lie Feng opener")))
	{
		return true;
	}
	TestEqual(TEXT("Lie Feng emits its direct packet and the newly applied Bleed trigger"), Result.DamageResults.Num(), 2);
	if (Result.DamageResults.Num() == 2)
	{
		TestEqual(TEXT("Lie Feng base deals exactly one-hundred-percent attack"), Result.DamageResults[0].HealthDamage, 10);
		TestEqual(TEXT("Lie Feng's newly applied Bleed triggers for one damage"), Result.DamageResults[1].HealthDamage, 1);
		TestEqual(TEXT("the Bleed trigger consumes its one applied layer"), Result.DamageResults[1].StatusStacksConsumed, 1);
	}
	TestEqual(TEXT("Lie Feng's direct hit and Bleed leave the exact target health"), FindUnit(Runtime, EnemyUnitId)->HP, 989);
	TestEqual(TEXT("Lie Feng is the first active card"), Runtime.ActiveCardsPlayedThisRound, 1);
	TestEqual(TEXT("Lie Feng registers its declarative Charge"),
		Runtime.PendingBladeCharge.Rule,
		EGameXXKBladeChargeRule::ReplayNextActiveBase);
	TestEqual(TEXT("the pending Charge keeps the exact source card"),
		Runtime.PendingBladeCharge.SourceCardId,
		FName(TEXT("Profession.Blade.LieFengZhan")));

	const int32 EnergyBeforeNext = Runtime.Deck.SharedEnergy;
	const int32 ManaBeforeNext = FindUnit(Runtime, BladeUnitId)->Mana;
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("next active card")))
	{
		return true;
	}

	TestEqual(TEXT("the next active base resolves once and its replay resolves once"), FindUnit(Runtime, EnemyUnitId)->HP, 957);
	TestEqual(TEXT("the replay is reported as one automatic resolution"), Result.AutomaticResolutionCount, 1);
	TestEqual(TEXT("the replay does not count as another active card"), Runtime.ActiveCardsPlayedThisRound, 2);
	TestEqual(TEXT("the replay spends no additional shared energy"), Runtime.Deck.SharedEnergy, EnergyBeforeNext - 1);
	TestEqual(TEXT("the replay spends no additional mana"), FindUnit(Runtime, BladeUnitId)->Mana, ManaBeforeNext - 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerLieFengFinishReturnTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.LieFengFinishReturnsNextRoundFirstActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladePartnerLieFengFinishReturnTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCoreRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("LieFeng"), EnemyUnitId, Result, TEXT("Lie Feng final card")))
	{
		return true;
	}
	if (!EndRoundAndBeginNext(*this, Runtime))
	{
		return true;
	}
	TestTrue(TEXT("the original next-card instance is drawn for the next round"),
		Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == FName(TEXT("Next"));
		}));

	const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
	const int32 ManaBefore = FindUnit(Runtime, BladeUnitId)->Mana;
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("next-round first active card")))
	{
		return true;
	}
	TestTrue(TEXT("the exact resolved instance returns from discard to hand"),
		Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == FName(TEXT("Next"))
				&& Card.CardId == FName(TEXT("Hero.Generic.HeYuZhan"));
		}));
	TestFalse(TEXT("the returned instance is not duplicated in discard"),
		Runtime.Deck.DiscardPile.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == FName(TEXT("Next"));
		}));
	TestEqual(TEXT("the returned card paid its original shared-energy cost"), Runtime.Deck.SharedEnergy, EnergyBefore - 1);
	TestEqual(TEXT("the returned card paid its original mana cost"), FindUnit(Runtime, BladeUnitId)->Mana, ManaBefore - 3);
	TestEqual(TEXT("returning the instance does not add another active play"), Runtime.ActiveCardsPlayedThisRound, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerHuiFengChargeCopyTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.HuiFengChargeCreatesZeroCostTemporaryCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladePartnerHuiFengChargeCopyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCoreRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		MakeCard(TEXT("HuiFeng"), TEXT("Profession.Blade.HuiFengJiaShi"), 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), 1)}))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiFeng"), NAME_None, Result, TEXT("Hui Feng opener")))
	{
		return true;
	}
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("copied next active card")))
	{
		return true;
	}

	const FGameXXKCardInstance* TemporaryCopy = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.CardId == FName(TEXT("Hero.Generic.HeYuZhan"))
			&& Card.InstanceId != FName(TEXT("Next"));
	});
	TestNotNull(TEXT("exactly one distinct temporary copy is added to hand"), TemporaryCopy);
	if (!TemporaryCopy)
	{
		return true;
	}
	const FName TemporaryInstanceId = TemporaryCopy->InstanceId;
	FGameXXKCardPlayPreview Preview;
	FString Error;
	TestTrue(FString::Printf(TEXT("temporary-copy preview builds: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TemporaryInstanceId, Preview, &Error));
	TestEqual(TEXT("temporary copy costs zero shared energy"), Preview.EffectiveEnergyCost, 0);
	TestEqual(TEXT("temporary copy costs zero mana"), Preview.EffectiveManaCost, 0);

	const int32 EnergyBeforeCopy = Runtime.Deck.SharedEnergy;
	const int32 ManaBeforeCopy = FindUnit(Runtime, BladeUnitId)->Mana;
	if (!Resolve(*this, Runtime, TemporaryInstanceId, EnemyUnitId, Result, TEXT("temporary copied card")))
	{
		return true;
	}
	TestEqual(TEXT("manual temporary play still counts as an active card"), Runtime.ActiveCardsPlayedThisRound, 3);
	TestEqual(TEXT("temporary play spends no shared energy"), Runtime.Deck.SharedEnergy, EnergyBeforeCopy);
	TestEqual(TEXT("temporary play spends no mana"), FindUnit(Runtime, BladeUnitId)->Mana, ManaBeforeCopy);
	TestFalse(TEXT("the temporary play does not create another copy"),
		Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.CardId == FName(TEXT("Hero.Generic.HeYuZhan"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerHuiFengFinishEnemyCardsTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.HuiFengFinishPreparesFirstTwoEnemyCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladePartnerHuiFengFinishEnemyCardsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerCoreRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		MakeCard(TEXT("HuiFeng"), TEXT("Profession.Blade.HuiFengJiaShi"), 0),
		MakeCard(TEXT("Filler"), TEXT("Hero.Generic.HeYuZhan"), 1)}))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiFeng"), NAME_None, Result, TEXT("Hui Feng final card")))
	{
		return true;
	}
	TArray<FGameXXKCardDamageResult> EndResults;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("Hui Feng player phase ends: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, EndResults, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("Hui Feng Finish immediately grants Mark2"), Status(Runtime, BladeUnitId, EGameXXKCardStatus::Mark), 2);
	GameXXKCardRules::ConsumeCombatStatus(
		*Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == BladeUnitId;
		}),
		EGameXXKCardStatus::Agility,
		MAX_int32);

	for (int32 EnemyCardIndex = 0; EnemyCardIndex < 3; ++EnemyCardIndex)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyUnitId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		Error.Reset();
		if (!TestTrue(FString::Printf(TEXT("enemy card %d direct packet resolves: %s"), EnemyCardIndex + 1, *Error),
			GameXXKCardRules::ResolveEnemyDirectAttack(
				Runtime,
				Context,
				BladeUnitId,
				10,
				Incoming,
				nullptr,
				&Error,
				true)))
		{
			return true;
		}

		TArray<FGameXXKCardDamageResult> Reactions;
		Error.Reset();
		if (!TestTrue(FString::Printf(TEXT("enemy card %d reaction boundary resolves: %s"), EnemyCardIndex + 1, *Error),
			GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
				Runtime,
				EnemyUnitId,
				EGameXXKCardDamageKind::SingleTargetAttack,
				BladeUnitId,
				Reactions,
				&Error)))
		{
			return true;
		}

		if (EnemyCardIndex < 2)
		{
			TestTrue(FString::Printf(TEXT("enemy card %d is dodged after pre-hit Agility2"), EnemyCardIndex + 1), Incoming.bAvoidedByAgility);
			TestEqual(FString::Printf(TEXT("enemy card %d triggers one independent Counter"), EnemyCardIndex + 1), Reactions.Num(), 1);
		}
		else
		{
			TestFalse(TEXT("the third enemy card receives no Finish Agility"), Incoming.bAvoidedByAgility);
			TestEqual(TEXT("the third enemy card registers no Finish Counter"), Reactions.Num(), 0);
		}
		FGameXXKCardCombatUnit* Blade = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == BladeUnitId;
		});
		GameXXKCardRules::ConsumeCombatStatus(*Blade, EGameXXKCardStatus::Agility, MAX_int32);
	}
	return true;
}

#endif
