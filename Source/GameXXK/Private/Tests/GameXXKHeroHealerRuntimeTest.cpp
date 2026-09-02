#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroHealerRuntimeTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName AllyAUnitId(TEXT("AllyA"));
	const FName AllyBUnitId(TEXT("AllyB"));
	const FName EnemyAUnitId(TEXT("EnemyA"));
	const FName EnemyBUnitId(TEXT("EnemyB"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits()
	{
		return {
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 10, 1),
			MakeUnit(AllyAUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 8, 2),
			MakeUnit(AllyBUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 8, 3),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 11)};
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
		Card.OwnerUnitId = HeroUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Healer.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandInstanceIds,
		const int32 Seed)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(),
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("healer runtime failed to initialize: %s"), *Error));
			return false;
		}

		TSet<FName> HandIds;
		for (const FName HandInstanceId : HandInstanceIds)
		{
			HandIds.Add(HandInstanceId);
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(HandIds.Contains(Card.InstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic healer fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	int32 CountCause(const TArray<FGameXXKCardDamageResult>& Results, const EGameXXKCardDamageCause Cause)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.Cause == Cause ? 1 : 0;
		}
		return Count;
	}

	const FGameXXKCardDamageResult* FindCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardDamageCause Cause)
	{
		return Results.FindByPredicate([Cause](const FGameXXKCardDamageResult& Result)
		{
			return Result.Cause == Cause;
		});
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

	TArray<FGameXXKCardInstance> YiXueDeck()
	{
		return {
			MakeCard(TEXT("YiXue"), TEXT("Hero.Healer.YiXueCuiFang"), 0),
			MakeCard(TEXT("Filler"), TEXT("Hero.Generic.QingFengYiShi"), 1)};
	}

	void AddAfterNextMedicineModifier(FGameXXKCardBattleRuntime& Runtime)
	{
		FGameXXKCardBattleModifierRuntime& Modifier = Runtime.Modifiers.AddDefaulted_GetRef();
		Modifier.ModifierId = TEXT("Medicine.AfterNext");
		Modifier.SourceCardInstanceId = TEXT("Medicine.Listener");
		Modifier.SourceUnitId = HeroUnitId;
		Modifier.RecipientUnitIds.Add(HeroUnitId);
		Modifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::AfterNextActiveCard;
		Modifier.Definition.EffectType = EGameXXKCardEffectType::ApplyStatus;
		Modifier.Definition.Target = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
		Modifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Modifier.Definition.Status = EGameXXKCardStatus::Medicine;
		Modifier.Definition.Magnitude = 6;
		Modifier.Definition.RemainingTriggers = 1;
		Modifier.Definition.bPersistent = true;
		Modifier.Definition.bActivePlayOnly = true;
		Modifier.SourceCardSnapshot.CardId = TEXT("Hero.Healer.YiXueCuiFang");
		Modifier.SourceCardSnapshot.Quality = EGameXXKCardQuality::Common;
		Modifier.SourceCardSnapshot.OwnerUnitId = HeroUnitId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerNonlethalFloorTest,
	"GameXXK.Data.HeroCards.Healer.PartyHealthLossStopsAtOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerNonlethalFloorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, YiXueDeck(), {TEXT("YiXue")}, 54001)) return false;
	for (const FName UnitId : {HeroUnitId, AllyAUnitId, AllyBUnitId})
	{
		FindUnit(Runtime, UnitId)->HP = 1;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YiXue"), NAME_None, Result, TEXT("nonlethal Yi Xue"))) return true;
	for (const FName UnitId : {HeroUnitId, AllyAUnitId, AllyBUnitId})
	{
		TestEqual(FString::Printf(TEXT("%s remains at one HP"), *UnitId.ToString()), FindUnit(Runtime, UnitId)->HP, 1);
		TestTrue(FString::Printf(TEXT("%s remains living"), *UnitId.ToString()), FindUnit(Runtime, UnitId)->bLiving);
	}
	TestEqual(TEXT("zero actual loss emits no self-loss packet"), Result.DamageResults.Num(), 0);
	TestEqual(TEXT("zero actual loss grants no Medicine"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerActualLossMedicineTest,
	"GameXXK.Data.HeroCards.Healer.YiXueCountsOnlyActualLossAndAwardsMedicineOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerActualLossMedicineTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, YiXueDeck(), {TEXT("YiXue")}, 54002)) return false;
	FindUnit(Runtime, HeroUnitId)->HP = 1;
	FindUnit(Runtime, AllyAUnitId)->HP = 2;
	FindUnit(Runtime, AllyBUnitId)->HP = 3;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YiXue"), NAME_None, Result, TEXT("partially effective Yi Xue"))) return true;
	TestEqual(TEXT("the one-HP Hero is unchanged"), FindUnit(Runtime, HeroUnitId)->HP, 1);
	TestEqual(TEXT("AllyA loses exactly one"), FindUnit(Runtime, AllyAUnitId)->HP, 1);
	TestEqual(TEXT("AllyB loses exactly one"), FindUnit(Runtime, AllyBUnitId)->HP, 2);
	TestEqual(TEXT("only two actual self-loss packets are reported"), Result.DamageResults.Num(), 2);
	TestEqual(TEXT("both actual losses retain SelfLoss cause"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::SelfLoss), 2);
	TestEqual(TEXT("two affected allies grant Medicine4 once"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 4);
	TestEqual(TEXT("a grant below six does not create Momentum"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 0);
	TestTrue(TEXT("Yi Xue draws its one filler card"), Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == FName(TEXT("Filler"));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerSixMedicineMomentumTest,
	"GameXXK.Data.HeroCards.Healer.SixMedicineGrantedAtOnceAlsoGrantsOneMomentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerSixMedicineMomentumTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, YiXueDeck(), {TEXT("YiXue")}, 54003)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YiXue"), NAME_None, Result, TEXT("three-target Yi Xue"))) return true;
	TestEqual(TEXT("three actual losses grant Medicine6 in one award"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 6);
	TestEqual(TEXT("a single Medicine6 award grants Momentum1"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerMedicineNoLegacyCapTest,
	"GameXXK.Data.HeroCards.Healer.MedicineCanExceedTheRetiredEightStackCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerMedicineNoLegacyCapTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, YiXueDeck(), {TEXT("YiXue")}, 54004)) return false;
	TestEqual(TEXT("fixture starts with Medicine7"), GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 7), 7);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("YiXue"), NAME_None, Result, TEXT("uncapped Medicine Yi Xue"))) return true;
	TestEqual(TEXT("Medicine grows from seven to thirteen"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 13);
	TestEqual(TEXT("the new six-stack award still grants exactly one Momentum"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerFriendlyReverseTest,
	"GameXXK.Data.HeroCards.Healer.FriendlyReverseCardHealsTenPlusSnapshotAndCleanses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerFriendlyReverseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("HuiChun"), TEXT("Hero.Healer.HuiChunNiMai"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HuiChun")}, 54005)) return false;
	FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyAUnitId);
	Ally->HP = 40;
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Bleed, 4);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Burn, 2);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 5);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiChun"), AllyAUnitId, Result, TEXT("friendly Hui Chun"))) return true;
	TestEqual(TEXT("level-one friendly Hui Chun scales base ten plus Medicine5 to sixteen"), FindUnit(Runtime, AllyAUnitId)->HP, 56);
	TestEqual(TEXT("friendly Hui Chun consumes the full snapshot"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 0);
	TestEqual(TEXT("friendly Hui Chun cleanses Bleed"), Status(Runtime, AllyAUnitId, EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("friendly Hui Chun cleanses Poison"), Status(Runtime, AllyAUnitId, EGameXXKCardStatus::Poison), 0);
	TestEqual(TEXT("friendly Hui Chun cleanses Burn"), Status(Runtime, AllyAUnitId, EGameXXKCardStatus::Burn), 0);
	TestEqual(TEXT("healing does not emit damage"), Result.DamageResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerEnemyReverseTest,
	"GameXXK.Data.HeroCards.Healer.EnemyReverseCardLosesTenPlusSnapshotIgnoringDefenseAndArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerEnemyReverseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("HuiChun"), TEXT("Hero.Healer.HuiChunNiMai"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HuiChun")}, 54006)) return false;
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyAUnitId);
	Enemy->HP = 15;
	Enemy->Defense = 40;
	Enemy->Armor = 50;
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 5);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiChun"), EnemyAUnitId, Result, TEXT("enemy Hui Chun"))) return true;
	TestEqual(TEXT("enemy Hui Chun removes base ten plus Medicine5 HP even when lethal"), FindUnit(Runtime, EnemyAUnitId)->HP, 0);
	TestFalse(TEXT("enemy Hui Chun can defeat its target"), FindUnit(Runtime, EnemyAUnitId)->bLiving);
	TestEqual(TEXT("enemy Hui Chun bypasses Armor"), FindUnit(Runtime, EnemyAUnitId)->Armor, 50);
	TestEqual(TEXT("enemy Hui Chun does not cleanse enemy Poison"), Status(Runtime, EnemyAUnitId, EGameXXKCardStatus::Poison), 3);
	TestEqual(TEXT("enemy Hui Chun consumes Medicine once"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 0);
	TestEqual(TEXT("enemy Hui Chun emits one health-loss audit"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("reverse healing has Medicine cause"), Result.DamageResults[0].Cause, EGameXXKCardDamageCause::Medicine);
		TestEqual(TEXT("reverse healing is not a direct-attack base packet"), Result.DamageResults[0].BaseRequestedDamage, 0);
		TestEqual(TEXT("reverse healing loses exactly fifteen health"), Result.DamageResults[0].HealthDamage, 15);
		TestEqual(TEXT("reverse healing absorbs no Armor"), Result.DamageResults[0].ArmorAbsorbed, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerNewMedicineSurvivesTest,
	"GameXXK.Data.HeroCards.Healer.MedicineCreatedDuringResolutionSurvivesOldSnapshotConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerNewMedicineSurvivesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("HuiChun"), TEXT("Hero.Healer.HuiChunNiMai"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HuiChun")}, 54007)) return false;
	FindUnit(Runtime, AllyAUnitId)->HP = 40;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 5);
	AddAfterNextMedicineModifier(Runtime);
	FString ValidationError;
	TestTrue(FString::Printf(TEXT("listener fixture validates: %s"), *ValidationError), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &ValidationError));
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuiChun"), AllyAUnitId, Result, TEXT("snapshot-safe Hui Chun"))) return true;
	TestEqual(TEXT("the old snapshot still scales to healing sixteen"), FindUnit(Runtime, AllyAUnitId)->HP, 56);
	TestEqual(TEXT("Medicine created after the action snapshot survives"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 6);
	TestEqual(TEXT("the post-resolution Medicine6 award grants Momentum1"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerGroupSnapshotTest,
	"GameXXK.Data.HeroCards.Healer.GroupHealAppliesSixPlusSnapshotToEveryAllyButConsumesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerGroupSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("BaiCao"), TEXT("Hero.Healer.BaiCaoJiZhen"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("BaiCao")}, 54008)) return false;
	for (const FName UnitId : {HeroUnitId, AllyAUnitId, AllyBUnitId})
	{
		FindUnit(Runtime, UnitId)->HP = 50;
	}
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 5);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("BaiCao"), NAME_None, Result, TEXT("Bai Cao group snapshot"))) return true;
	for (const FName UnitId : {HeroUnitId, AllyAUnitId, AllyBUnitId})
	{
		TestEqual(FString::Printf(TEXT("%s receives full level-one scaled 6+5 healing"), *UnitId.ToString()), FindUnit(Runtime, UnitId)->HP, 62);
	}
	TestEqual(TEXT("the group action consumes Medicine only once"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerDuHuoTest,
	"GameXXK.Data.HeroCards.Healer.DuHuoAppliesPoison6Burn2ThenExplodesThenGrantsMedicine6",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerDuHuoTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("DuHuo"), TEXT("Hero.Healer.DuHuoTongLu"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("DuHuo")}, 54009)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("DuHuo"), EnemyAUnitId, Result, TEXT("Du Huo"))) return true;
	TestEqual(TEXT("Du Huo emits direct, Poison, and Burn packets"), Result.DamageResults.Num(), 3);
	TestEqual(TEXT("Du Huo emits one direct packet"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::DirectAttack), 1);
	TestEqual(TEXT("Du Huo explodes Poison once"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionPoison), 1);
	TestEqual(TEXT("Du Huo explodes Burn once"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionBurn), 1);
	TestEqual(TEXT("Poison6 remains in its reservoir after exploding"), Status(Runtime, EnemyAUnitId, EGameXXKCardStatus::Poison), 6);
	TestEqual(TEXT("Burn2 remains in its reservoir after exploding"), Status(Runtime, EnemyAUnitId, EGameXXKCardStatus::Burn), 2);
	TestEqual(TEXT("Du Huo grants Medicine6 at the end"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Medicine), 6);
	TestEqual(TEXT("Du Huo's Medicine6 grants Momentum1"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerBaiCaoStatusTest,
	"GameXXK.Data.HeroCards.Healer.BaiCaoAddsOnlyPoison1Burn1PerEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerBaiCaoStatusTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("BaiCao"), TEXT("Hero.Healer.BaiCaoJiZhen"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("BaiCao")}, 54010)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Medicine, 12);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("BaiCao"), NAME_None, Result, TEXT("Bai Cao status spread"))) return true;
	for (const FName UnitId : {EnemyAUnitId, EnemyBUnitId})
	{
		TestEqual(FString::Printf(TEXT("%s receives exactly Poison1"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Poison), 1);
		TestEqual(FString::Printf(TEXT("%s receives exactly Burn1"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Burn), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerToxicExplosionRotReservoirTest,
	"GameXXK.Data.HeroCards.Healer.ToxicExplosionTriggersRotAsIndependentReservoir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerToxicExplosionRotReservoirTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroHealerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("DuHuo"), TEXT("Hero.Healer.DuHuoTongLu"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("DuHuo")}, 54011)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyAUnitId), EGameXXKCardStatus::DamageOverTime, 7);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("DuHuo"), EnemyAUnitId, Result, TEXT("Rot-safe Du Huo"))) return true;
	TestEqual(TEXT("Du Huo emits direct, Poison, Burn, and Rot packets"), Result.DamageResults.Num(), 4);
	TestEqual(TEXT("toxic explosion emits one independent Rot packet"), CountCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionRot), 1);
	TestEqual(TEXT("toxic explosion preserves all Rot stacks"), Status(Runtime, EnemyAUnitId, EGameXXKCardStatus::DamageOverTime), 7);
	const FGameXXKCardDamageResult* PoisonResult = FindCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionPoison);
	const FGameXXKCardDamageResult* BurnResult = FindCause(Result.DamageResults, EGameXXKCardDamageCause::ToxicExplosionBurn);
	TestNotNull(TEXT("Poison explosion packet exists"), PoisonResult);
	TestNotNull(TEXT("Burn explosion packet exists"), BurnResult);
	if (PoisonResult && BurnResult)
	{
		TestEqual(TEXT("Rot no longer multiplies Poison"), PoisonResult->RotDamageBonus, 0);
		TestEqual(TEXT("Rot no longer multiplies Burn"), BurnResult->RotDamageBonus, 0);
	}
	return true;
}

#endif
