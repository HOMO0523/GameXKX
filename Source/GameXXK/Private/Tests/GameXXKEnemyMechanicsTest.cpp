#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GameXXKCardRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakeIronfeatherFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? EGameXXKCharacterRole::Hero : EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.Attack = Attack;
		Unit.Mana = 0;
		Unit.MaxMana = 0;
		Unit.Defense = Defense;
		Unit.StableSortOrder = StableSortOrder;
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.EnemyDefinitionId = TEXT("Enemy.Ch1.IronfeatherRooster");
		}
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeIronfeatherFixtureCards()
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Ironfeather.Card.%d"), Index));
			Card.CardId = TEXT("Profession.Guard.ZhenDun");
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Ironfeather.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool InitializeIronfeatherFixture(FGameXXKCardBattleRuntime& OutRuntime, FString& OutError, const int32 EnemyDefense = 4)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeIronfeatherFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1));
		Units.Add(MakeIronfeatherFixtureUnit(TEXT("Ironfeather"), EGameXXKCardTargetSide::Enemy, 1000, 0, EnemyDefense, 2));
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, MakeIronfeatherFixtureCards(), Units, EGameXXKCardTerrain::Plain, 1977, &OutError))
		{
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit MakeBlackBearFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeIronfeatherFixtureUnit(UnitId, Side, Health, Attack, Defense, StableSortOrder);
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.EnemyDefinitionId = TEXT("Enemy.Ch2.BlackBear");
		}
		return Unit;
	}

	FGameXXKCardCombatUnit MakeBluehornFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeIronfeatherFixtureUnit(UnitId, Side, Health, Attack, Defense, StableSortOrder);
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.EnemyDefinitionId = TEXT("Enemy.Ch1.BluehornGoatKing");
		}
		return Unit;
	}

	FGameXXKCardCombatUnit MakeRedtuskFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeIronfeatherFixtureUnit(UnitId, Side, Health, Attack, Defense, StableSortOrder);
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.EnemyDefinitionId = TEXT("Enemy.Ch2.RedtuskBoarKing");
		}
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeBlackBearFixtureCards(const FName CardId = TEXT("Hero.Generic.QingFengYiShi"))
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("BlackBear.Card.%d"), Index));
			Card.CardId = CardId;
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("BlackBear.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool InitializeBlackBearFixture(
		FGameXXKCardBattleRuntime& OutRuntime,
		FString& OutError,
		const int32 HeroAttack = 20,
		const int32 EnemyHealth = 1000,
		const int32 EnemyDefense = 4)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeBlackBearFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, HeroAttack, 0, 1));
		Units.Add(MakeBlackBearFixtureUnit(TEXT("BlackBear"), EGameXXKCardTargetSide::Enemy, EnemyHealth, 0, EnemyDefense, 2));
		return GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeBlackBearFixtureCards(TEXT("Profession.Guard.ZhenDun")),
			Units,
			EGameXXKCardTerrain::Plain,
			2048,
			&OutError);
	}

	bool InitializeBluehornFixture(
		FGameXXKCardBattleRuntime& OutRuntime,
		FString& OutError,
		const bool bIncludeNonPassiveEnemy = false)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeBluehornFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1));
		Units.Add(MakeBluehornFixtureUnit(TEXT("Bluehorn"), EGameXXKCardTargetSide::Enemy, 1000, 0, 0, 2));
		if (bIncludeNonPassiveEnemy)
		{
			FGameXXKCardCombatUnit& OtherEnemy = Units.Add_GetRef(MakeIronfeatherFixtureUnit(TEXT("OtherEnemy"), EGameXXKCardTargetSide::Enemy, 1000, 0, 0, 3));
			OtherEnemy.EnemyDefinitionId = TEXT("Enemy.Ch1.Goat");
		}
		return GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, MakeBlackBearFixtureCards(), Units, EGameXXKCardTerrain::Plain, 2050, &OutError);
	}

	bool InitializeRedtuskFixture(
		FGameXXKCardBattleRuntime& OutRuntime,
		FString& OutError,
		const bool bIncludeGuardian = false)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeRedtuskFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1));
		Units.Add(MakeRedtuskFixtureUnit(TEXT("Redtusk"), EGameXXKCardTargetSide::Enemy, 1000, 0, 0, 2));
		if (bIncludeGuardian)
		{
			FGameXXKCardCombatUnit Guardian = MakeIronfeatherFixtureUnit(TEXT("Guardian"), EGameXXKCardTargetSide::Enemy, 1000, 0, 0, 3);
			Guardian.EnemyDefinitionId = TEXT("Enemy.Ch2.Boar");
			Units.Add(Guardian);
		}
		return GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, MakeBlackBearFixtureCards(), Units, EGameXXKCardTerrain::Plain, 2051, &OutError);
	}

	TArray<FGameXXKCardInstance> MakeBlackBearGroupFixtureCards()
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("BlackBear.GroupCard.%d"), Index));
			Card.CardId = TEXT("Profession.Sorcerer.XingHuoLiaoYuan");
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("BlackBear.GroupSource.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool InitializeBlackBearGroupFixture(FGameXXKCardBattleRuntime& OutRuntime, FString& OutError)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		FGameXXKCardCombatUnit Hero = MakeBlackBearFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1);
		Hero.Mana = 30;
		Hero.MaxMana = 30;
		Units.Add(Hero);
		Units.Add(MakeBlackBearFixtureUnit(TEXT("BlackBear"), EGameXXKCardTargetSide::Enemy, 100, 0, 0, 2));
		FGameXXKCardCombatUnit OtherEnemy = MakeIronfeatherFixtureUnit(TEXT("OtherEnemy"), EGameXXKCardTargetSide::Enemy, 100, 0, 0, 3);
		OtherEnemy.EnemyDefinitionId = NAME_None;
		Units.Add(OtherEnemy);
		return GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, MakeBlackBearGroupFixtureCards(), Units, EGameXXKCardTerrain::Plain, 2049, &OutError);
	}

	FGameXXKCardCombatUnit* FindFixtureUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	TArray<uint8> SerializeRuntimeForSaveGame(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime Copy = Runtime;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool DeserializeRuntimeFromSaveGame(const TArray<uint8>& Bytes, FGameXXKCardBattleRuntime& OutRuntime)
	{
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Archive, &OutRuntime, nullptr);
		return !Archive.IsError();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBlackBearThickHidePassiveTest,
	"GameXXK.Battle.EnemyMechanics.BlackBearThickHideDirectPlayerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBlackBearThickHidePassiveTest::RunTest(const FString& Parameters)
{
	FString Error;

	FGameXXKCardBattleRuntime SingleTargetRuntime;
	if (!TestTrue(TEXT("the Black Bear single-target fixture initializes"), InitializeBlackBearFixture(SingleTargetRuntime, Error)))
	{
		return false;
	}
	TestNull(TEXT("the Black Bear fixture starts without a pre-seeded enemy state"), SingleTargetRuntime.EnemyStates.Find(TEXT("BlackBear")));
	FGameXXKCardPlayResult SingleTargetResult;
	TestTrue(TEXT("a normal player card hit resolves against Black Bear"), GameXXKCardRules::ResolveCardPlay(SingleTargetRuntime, SingleTargetRuntime.Deck.Hand[0].InstanceId, TEXT("BlackBear"), SingleTargetResult, &Error));
	if (!TestTrue(TEXT("the normal Black Bear hit records one direct damage result"), SingleTargetResult.DamageResults.Num() == 1))
	{
		return false;
	}
	TestEqual(TEXT("Black Bear applies thick hide after normal defense"), SingleTargetResult.DamageResults[0].DamageAfterDefense, 16);
	TestEqual(TEXT("Black Bear applies no implicit vulnerability in the fixture"), SingleTargetResult.DamageResults[0].DamageAfterVulnerability, 16);
	TestEqual(TEXT("Black Bear absorbs no armor in the normal fixture"), SingleTargetResult.DamageResults[0].ArmorAbsorbed, 0);
	TestEqual(TEXT("Black Bear thick hide floors the post-armor direct damage to eighty-five percent"), SingleTargetResult.DamageResults[0].HealthDamage, 13);
	TestEqual(TEXT("Black Bear loses only the thick-hide-reduced health amount"), FindFixtureUnit(SingleTargetRuntime, TEXT("BlackBear"))->HP, 987);
	const FGameXXKEnemyBattleState* SingleTargetState = SingleTargetRuntime.EnemyStates.Find(TEXT("BlackBear"));
	TestNotNull(TEXT("a Black Bear player-card hit initializes its persistent enemy state"), SingleTargetState);
	if (!SingleTargetState)
	{
		return false;
	}
	TestEqual(TEXT("the initialized Black Bear state retains the catalog definition identity"), SingleTargetState->DefinitionId, FName(TEXT("Enemy.Ch2.BlackBear")));

	FGameXXKCardBattleRuntime ArmorRuntime;
	if (!TestTrue(TEXT("the Black Bear armor fixture initializes"), InitializeBlackBearFixture(ArmorRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ArmorBlackBear = FindFixtureUnit(ArmorRuntime, TEXT("BlackBear"));
	if (!TestNotNull(TEXT("the Black Bear armor fixture exposes the living target"), ArmorBlackBear))
	{
		return false;
	}
	ArmorBlackBear->Armor = 5;
	FGameXXKCardPlayResult ArmorResult;
	if (!TestTrue(TEXT("a player card hit resolves through Black Bear armor"), GameXXKCardRules::ResolveCardPlay(ArmorRuntime, ArmorRuntime.Deck.Hand[0].InstanceId, TEXT("BlackBear"), ArmorResult, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the armored Black Bear hit records one direct damage result"), ArmorResult.DamageResults.Num() == 1))
	{
		return false;
	}
	TestEqual(TEXT("Black Bear armor absorbs before thick hide"), ArmorResult.DamageResults[0].ArmorAbsorbed, 5);
	TestEqual(TEXT("Black Bear thick hide floors only the post-armor remainder"), ArmorResult.DamageResults[0].HealthDamage, 9);
	TestEqual(TEXT("Black Bear armor and thick hide leave the expected health"), FindFixtureUnit(ArmorRuntime, TEXT("BlackBear"))->HP, 991);

	FGameXXKCardBattleRuntime HealthCapRuntime;
	if (!TestTrue(TEXT("the Black Bear health-cap fixture initializes"), InitializeBlackBearFixture(HealthCapRuntime, Error, 22, 20, 0)))
	{
		return false;
	}
	FGameXXKCardPlayResult HealthCapResult;
	if (!TestTrue(TEXT("a high-damage player card hit resolves against low-health Black Bear"), GameXXKCardRules::ResolveCardPlay(HealthCapRuntime, HealthCapRuntime.Deck.Hand[0].InstanceId, TEXT("BlackBear"), HealthCapResult, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the low-health Black Bear hit records one direct damage result"), HealthCapResult.DamageResults.Num() == 1))
	{
		return false;
	}
	TestEqual(TEXT("Black Bear health cap fixture retains its unmodified damage before thick hide"), HealthCapResult.DamageResults[0].DamageAfterDefense, 22);
	TestEqual(TEXT("Black Bear applies thick hide before capping the damage to remaining health"), HealthCapResult.DamageResults[0].HealthDamage, 18);
	TestEqual(TEXT("Black Bear retains the expected health after post-thick-hide cap ordering"), FindFixtureUnit(HealthCapRuntime, TEXT("BlackBear"))->HP, 2);

	FGameXXKCardBattleRuntime GroupRuntime;
	if (!TestTrue(TEXT("the Black Bear group-card fixture initializes"), InitializeBlackBearGroupFixture(GroupRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardPlayResult GroupResult;
	TestTrue(TEXT("a real all-enemies player card resolves against Black Bear"), GameXXKCardRules::ResolveCardPlay(GroupRuntime, GroupRuntime.Deck.Hand[0].InstanceId, NAME_None, GroupResult, &Error));
	const FGameXXKCardDamageResult* BlackBearGroupDamage = GroupResult.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Damage)
	{
		return Damage.OriginalTargetUnitId == TEXT("BlackBear");
	});
	const FGameXXKCardDamageResult* OtherEnemyGroupDamage = GroupResult.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Damage)
	{
		return Damage.OriginalTargetUnitId == TEXT("OtherEnemy");
	});
	TestNotNull(TEXT("the all-enemies player card reports a Black Bear group hit"), BlackBearGroupDamage);
	TestNotNull(TEXT("the all-enemies player card reports the non-passive group hit"), OtherEnemyGroupDamage);
	if (!BlackBearGroupDamage || !OtherEnemyGroupDamage)
	{
		return false;
	}
	TestEqual(TEXT("Black Bear thick hide applies to the all-enemies card entry"), BlackBearGroupDamage->HealthDamage, 11);
	TestEqual(TEXT("a non-passive enemy keeps the unreduced group-card damage"), OtherEnemyGroupDamage->HealthDamage, 14);
	TestEqual(TEXT("the all-enemies player card leaves Black Bear at reduced direct-card health"), FindFixtureUnit(GroupRuntime, TEXT("BlackBear"))->HP, 89);
	TestEqual(TEXT("the all-enemies player card leaves the non-passive enemy at normal health"), FindFixtureUnit(GroupRuntime, TEXT("OtherEnemy"))->HP, 86);

	FGameXXKCardBattleRuntime DotRuntime;
	if (!TestTrue(TEXT("the Black Bear DOT fixture initializes"), InitializeBlackBearFixture(DotRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* DotBlackBear = FindFixtureUnit(DotRuntime, TEXT("BlackBear"));
	if (!TestNotNull(TEXT("the Black Bear DOT fixture exposes the living target"), DotBlackBear))
	{
		return false;
	}
	TestEqual(TEXT("the Black Bear DOT fixture applies one poison stack"), GameXXKCardRules::AddCombatStatus(*DotBlackBear, EGameXXKCardStatus::Poison, 1), 1);
	int32 DotDamage = 0;
	TestTrue(TEXT("poison resolves through the non-player-card damage path"), GameXXKCardRules::ApplyCombatEndPhaseDot(DotRuntime.Units, DotRuntime.GuardLinks, TEXT("BlackBear"), DotDamage, &Error));
	TestEqual(TEXT("Black Bear thick hide does not reduce DOT damage"), DotDamage, 1);
	TestNull(TEXT("DOT alone does not create a Black Bear player-card enemy state"), DotRuntime.EnemyStates.Find(TEXT("BlackBear")));

	FGameXXKCardBattleRuntime GenericRuntime;
	if (!TestTrue(TEXT("the Black Bear generic-direct-damage fixture initializes"), InitializeBlackBearFixture(GenericRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardDamageContext GenericContext;
	GenericContext.SourceUnitId = TEXT("Hero");
	GenericContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult GenericResult;
	TestTrue(TEXT("a generic non-card direct-damage packet resolves against Black Bear"), GameXXKCardRules::ApplyCombatDirectDamage(GenericRuntime.Units, GenericRuntime.GuardLinks, GenericContext, TEXT("BlackBear"), 20, GenericResult, &Error));
	TestEqual(TEXT("Black Bear thick hide does not alter generic non-card direct damage"), GenericResult.HealthDamage, 16);
	TestNull(TEXT("generic non-card direct damage does not create a Black Bear player-card enemy state"), GenericRuntime.EnemyStates.Find(TEXT("BlackBear")));

	FGameXXKCardBattleRuntime MismatchedStateRuntime;
	if (!TestTrue(TEXT("the Black Bear mismatched-state fixture initializes"), InitializeBlackBearFixture(MismatchedStateRuntime, Error)))
	{
		return false;
	}
	FGameXXKEnemyBattleState& MismatchedState = MismatchedStateRuntime.EnemyStates.FindOrAdd(TEXT("BlackBear"));
	MismatchedState.DefinitionId = TEXT("Enemy.Ch1.Goat");
	const TArray<uint8> BeforeRejectedHit = SerializeRuntimeForSaveGame(MismatchedStateRuntime);
	FGameXXKCardPlayResult RejectedResult;
	RejectedResult.CardInstanceId = TEXT("BlackBear.OutputMustRemainUnchanged");
	TestFalse(TEXT("a mismatched Black Bear persisted enemy definition rejects the player-card hit"), GameXXKCardRules::ResolveCardPlay(MismatchedStateRuntime, MismatchedStateRuntime.Deck.Hand[0].InstanceId, TEXT("BlackBear"), RejectedResult, &Error));
	TestEqual(TEXT("a rejected Black Bear mismatched-state hit preserves its caller output"), RejectedResult.CardInstanceId, FName(TEXT("BlackBear.OutputMustRemainUnchanged")));
	TestEqual(TEXT("a rejected Black Bear mismatched-state hit is atomic for the full runtime"), SerializeRuntimeForSaveGame(MismatchedStateRuntime), BeforeRejectedHit);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKIronfeatherFirstHitPassiveTest,
	"GameXXK.Battle.EnemyMechanics.IronfeatherFirstHitDirectPlayerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKIronfeatherFirstHitPassiveTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("the Ironfeather fixture initializes a real player-card runtime"), InitializeIronfeatherFixture(Runtime, Error)))
	{
		return false;
	}
	TestNull(TEXT("the fixture starts without a pre-seeded enemy battle state"), Runtime.EnemyStates.Find(TEXT("Ironfeather")));

	const FName FirstCardId = Runtime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult FirstResult;
	TestTrue(TEXT("the first equal player card hit resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, FirstCardId, TEXT("Ironfeather"), FirstResult, &Error));
	TestEqual(TEXT("the first hit retains normal defense mitigation before the passive"), FirstResult.DamageResults[0].DamageAfterDefense, 16);
	TestEqual(TEXT("the first Ironfeather hit loses exactly half of the normally mitigated health damage"), FirstResult.DamageResults[0].HealthDamage, 8);
	const FGameXXKEnemyBattleState* FirstState = Runtime.EnemyStates.Find(TEXT("Ironfeather"));
	TestNotNull(TEXT("the Ironfeather state remains addressable after the first card hit"), FirstState);
	if (!FirstState)
	{
		return false;
	}
	TestFalse(TEXT("the first eligible direct player card hit consumes the passive"), FirstState->bFirstHitPassiveAvailable);

	const TArray<uint8> SavedRuntimeBytes = SerializeRuntimeForSaveGame(Runtime);
	FGameXXKCardBattleRuntime ReloadedRuntime;
	TestTrue(TEXT("the runtime deserializes through the SaveGame archive"), DeserializeRuntimeFromSaveGame(SavedRuntimeBytes, ReloadedRuntime));
	const FGameXXKEnemyBattleState* ReloadedState = ReloadedRuntime.EnemyStates.Find(TEXT("Ironfeather"));
	TestNotNull(TEXT("the consumed first-hit state persists through runtime serialization"), ReloadedState);
	if (!ReloadedState)
	{
		return false;
	}
	TestFalse(TEXT("runtime persistence retains the consumed first-hit flag"), ReloadedState->bFirstHitPassiveAvailable);

	const FName SecondCardId = ReloadedRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult SecondResult;
	TestTrue(TEXT("the second equal player card hit resolves after runtime persistence"), GameXXKCardRules::ResolveCardPlay(ReloadedRuntime, SecondCardId, TEXT("Ironfeather"), SecondResult, &Error));
	TestEqual(TEXT("the second direct player card hit has the same normal defense result"), SecondResult.DamageResults[0].DamageAfterDefense, 16);
	TestEqual(TEXT("the consumed first-hit passive leaves the second direct player card hit unmodified"), SecondResult.DamageResults[0].HealthDamage, 16);
	TestEqual(TEXT("the two equal hits leave the expected total Ironfeather health"), FindFixtureUnit(ReloadedRuntime, TEXT("Ironfeather"))->HP, 976);

	FGameXXKCardBattleRuntime ArmorRuntime;
	if (!TestTrue(TEXT("the armor-nullification fixture initializes"), InitializeIronfeatherFixture(ArmorRuntime, Error, 0)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ArmorTarget = FindFixtureUnit(ArmorRuntime, TEXT("Ironfeather"));
	TestNotNull(TEXT("the armor-nullification fixture keeps a living Ironfeather target"), ArmorTarget);
	if (!ArmorTarget)
	{
		return false;
	}
	ArmorTarget->Armor = 20;
	FGameXXKCardPlayResult ArmorBlockedResult;
	TestTrue(TEXT("an armor-nullified player card hit resolves"), GameXXKCardRules::ResolveCardPlay(ArmorRuntime, ArmorRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), ArmorBlockedResult, &Error));
	TestEqual(TEXT("armor can leave the first attempted player card hit with zero health damage"), ArmorBlockedResult.DamageResults[0].HealthDamage, 0);
	const FGameXXKEnemyBattleState* ArmorState = ArmorRuntime.EnemyStates.Find(TEXT("Ironfeather"));
	if (!TestNotNull(TEXT("armor-nullified damage initializes the enemy state"), ArmorState))
	{
		return false;
	}
	TestTrue(TEXT("armor-nullified damage leaves the first-hit passive available"), ArmorState->bFirstHitPassiveAvailable);
	FGameXXKCardPlayResult ArmorFollowupResult;
	TestTrue(TEXT("the later positive player card hit resolves after armor is spent"), GameXXKCardRules::ResolveCardPlay(ArmorRuntime, ArmorRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), ArmorFollowupResult, &Error));
	TestEqual(TEXT("the later positive player card hit receives the first-hit reduction"), ArmorFollowupResult.DamageResults[0].HealthDamage, 10);
	TestFalse(TEXT("the later positive player card hit consumes the first-hit passive"), ArmorRuntime.EnemyStates.Find(TEXT("Ironfeather"))->bFirstHitPassiveAvailable);

	FGameXXKCardBattleRuntime OnePointRuntime;
	if (!TestTrue(TEXT("the one-point fixture initializes"), InitializeIronfeatherFixture(OnePointRuntime, Error, 19)))
	{
		return false;
	}
	FGameXXKCardPlayResult OnePointResult;
	TestTrue(TEXT("a one-point pre-reduction player card hit resolves"), GameXXKCardRules::ResolveCardPlay(OnePointRuntime, OnePointRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), OnePointResult, &Error));
	TestEqual(TEXT("a one-point pre-reduction hit rounds down to zero final health damage"), OnePointResult.DamageResults[0].HealthDamage, 0);
	const FGameXXKEnemyBattleState* OnePointState = OnePointRuntime.EnemyStates.Find(TEXT("Ironfeather"));
	if (!TestNotNull(TEXT("a one-point pre-reduction hit initializes the enemy state"), OnePointState))
	{
		return false;
	}
	TestTrue(TEXT("a one-point pre-reduction hit leaves the first-hit passive available"), OnePointState->bFirstHitPassiveAvailable);
	FindFixtureUnit(OnePointRuntime, TEXT("Ironfeather"))->Defense = 0;
	FGameXXKCardPlayResult OnePointFollowupResult;
	TestTrue(TEXT("a later positive player card hit resolves after the one-point hit"), GameXXKCardRules::ResolveCardPlay(OnePointRuntime, OnePointRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), OnePointFollowupResult, &Error));
	TestEqual(TEXT("the later positive player card hit is reduced after the one-point hit"), OnePointFollowupResult.DamageResults[0].HealthDamage, 10);
	TestFalse(TEXT("the later positive player card hit consumes the first-hit passive after the one-point hit"), OnePointRuntime.EnemyStates.Find(TEXT("Ironfeather"))->bFirstHitPassiveAvailable);

	FGameXXKCardBattleRuntime AgilityRuntime;
	if (!TestTrue(TEXT("the agility fixture initializes"), InitializeIronfeatherFixture(AgilityRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* AgilityTarget = FindFixtureUnit(AgilityRuntime, TEXT("Ironfeather"));
	TestNotNull(TEXT("the agility fixture keeps a living Ironfeather target"), AgilityTarget);
	if (!AgilityTarget)
	{
		return false;
	}
	TestEqual(TEXT("the agility fixture applies one dodge stack"), GameXXKCardRules::AddCombatStatus(*AgilityTarget, EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardPlayResult AgilityResult;
	TestTrue(TEXT("an agility-avoided player card hit resolves"), GameXXKCardRules::ResolveCardPlay(AgilityRuntime, AgilityRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), AgilityResult, &Error));
	TestTrue(TEXT("agility marks the direct player card hit as avoided"), AgilityResult.DamageResults[0].bAvoidedByAgility);
	const FGameXXKEnemyBattleState* AgilityState = AgilityRuntime.EnemyStates.Find(TEXT("Ironfeather"));
	TestTrue(TEXT("agility avoidance does not consume a first-hit state when one exists"), !AgilityState || AgilityState->bFirstHitPassiveAvailable);

	FGameXXKCardBattleRuntime DotRuntime;
	if (!TestTrue(TEXT("the non-direct poison fixture initializes"), InitializeIronfeatherFixture(DotRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* DotTarget = FindFixtureUnit(DotRuntime, TEXT("Ironfeather"));
	TestNotNull(TEXT("the poison fixture keeps a living Ironfeather target"), DotTarget);
	if (!DotTarget)
	{
		return false;
	}
	DotTarget->Armor = 20;
	FGameXXKCardPlayResult DotSetupResult;
	TestTrue(TEXT("an armor-nullified player-card hit resolves before poison"), GameXXKCardRules::ResolveCardPlay(DotRuntime, DotRuntime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), DotSetupResult, &Error));
	TestEqual(TEXT("the armor-nullified poison setup deals no health damage"), DotSetupResult.DamageResults[0].HealthDamage, 0);
	DotTarget = FindFixtureUnit(DotRuntime, TEXT("Ironfeather"));
	if (!TestNotNull(TEXT("the poison target remains addressable after the staged card-play commit"), DotTarget))
	{
		return false;
	}
	TestEqual(TEXT("the poison fixture applies one end-phase poison stack"), GameXXKCardRules::AddCombatStatus(*DotTarget, EGameXXKCardStatus::Poison, 1), 1);
	int32 DotDamage = 0;
	TestTrue(TEXT("end-phase poison resolves through the non-direct damage path"), GameXXKCardRules::ApplyCombatEndPhaseDot(DotRuntime.Units, DotRuntime.GuardLinks, TEXT("Ironfeather"), DotDamage, &Error));
	TestEqual(TEXT("one poison stack deals its normal end-phase damage"), DotDamage, 1);
	const FGameXXKEnemyBattleState* DotState = DotRuntime.EnemyStates.Find(TEXT("Ironfeather"));
	TestNotNull(TEXT("the non-direct poison fixture retains the enemy state"), DotState);
	if (DotState)
	{
		TestTrue(TEXT("poison/end-phase damage never consumes Ironfeather's direct-player-card passive"), DotState->bFirstHitPassiveAvailable);
	}

	FGameXXKCardBattleRuntime AtomicRuntime;
	if (!TestTrue(TEXT("the player-card direct-damage atomicity fixture initializes"), InitializeIronfeatherFixture(AtomicRuntime, Error)))
	{
		return false;
	}
	const TArray<uint8> BeforeInvalidPlayerCardDamage = SerializeRuntimeForSaveGame(AtomicRuntime);
	FGameXXKCardDamageContext InvalidSourceContext;
	InvalidSourceContext.SourceUnitId = TEXT("Ironfeather");
	InvalidSourceContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult RejectedDamageResult;
	RejectedDamageResult.OriginalTargetUnitId = TEXT("PriorResult");
	TestFalse(TEXT("player-card direct damage rejects a non-party source"), GameXXKCardRules::ApplyPlayerCardDirectDamage(AtomicRuntime, InvalidSourceContext, TEXT("Hero"), 10, RejectedDamageResult, &Error));
	TestEqual(TEXT("a rejected non-party player-card damage call leaves the runtime unchanged"), SerializeRuntimeForSaveGame(AtomicRuntime), BeforeInvalidPlayerCardDamage);
	TestEqual(TEXT("a rejected non-party player-card damage call preserves its caller result"), RejectedDamageResult.OriginalTargetUnitId, FName(TEXT("PriorResult")));

	FGameXXKCardDamageContext InvalidTargetContext;
	InvalidTargetContext.SourceUnitId = TEXT("Hero");
	InvalidTargetContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	RejectedDamageResult.OriginalTargetUnitId = TEXT("PriorInvalidTargetResult");
	TestFalse(TEXT("player-card direct damage rejects an absent target"), GameXXKCardRules::ApplyPlayerCardDirectDamage(AtomicRuntime, InvalidTargetContext, TEXT("Absent"), 10, RejectedDamageResult, &Error));
	TestEqual(TEXT("a rejected absent-target player-card damage call leaves the runtime unchanged"), SerializeRuntimeForSaveGame(AtomicRuntime), BeforeInvalidPlayerCardDamage);
	TestEqual(TEXT("a rejected absent-target player-card damage call preserves its caller result"), RejectedDamageResult.OriginalTargetUnitId, FName(TEXT("PriorInvalidTargetResult")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKIronfeatherEnemyStateIntegrityTest,
	"GameXXK.Battle.EnemyMechanics.IronfeatherStateIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKIronfeatherEnemyStateIntegrityTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("the mismatched-state fixture initializes"), InitializeIronfeatherFixture(Runtime, Error)))
	{
		return false;
	}
	TestNull(TEXT("the mismatched-state fixture starts without an enemy battle state"), Runtime.EnemyStates.Find(TEXT("Ironfeather")));

	FGameXXKEnemyBattleState& MismatchedState = Runtime.EnemyStates.FindOrAdd(TEXT("Ironfeather"));
	MismatchedState.DefinitionId = TEXT("Enemy.Ch1.Goat");
	MismatchedState.bFirstHitPassiveAvailable = true;

	const TArray<uint8> BeforeRejectedHit = SerializeRuntimeForSaveGame(Runtime);
	FGameXXKCardPlayResult RejectedResult;
	RejectedResult.CardInstanceId = TEXT("ResultMustRemainUnchanged");
	TestFalse(
		TEXT("a mismatched enemy-state definition rejects a player-card hit"),
		GameXXKCardRules::ResolveCardPlay(Runtime, Runtime.Deck.Hand[0].InstanceId, TEXT("Ironfeather"), RejectedResult, &Error));
	TestEqual(TEXT("a rejected mismatched-state hit preserves its output result"), RejectedResult.CardInstanceId, FName(TEXT("ResultMustRemainUnchanged")));
	TestEqual(
		TEXT("a rejected mismatched-state hit is atomic for the runtime"),
		SerializeRuntimeForSaveGame(Runtime),
		BeforeRejectedHit);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBluehornArmorRetentionTest,
	"GameXXK.Battle.EnemyMechanics.BluehornArmorRetentionAtEnemyPhaseStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBluehornArmorRetentionTest::RunTest(const FString& Parameters)
{
	FString Error;

	FGameXXKCardBattleRuntime CycleRuntime;
	if (!TestTrue(TEXT("the Bluehorn mixed fixture initializes"), InitializeBluehornFixture(CycleRuntime, Error, true)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Bluehorn = FindFixtureUnit(CycleRuntime, TEXT("Bluehorn"));
	FGameXXKCardCombatUnit* OtherEnemy = FindFixtureUnit(CycleRuntime, TEXT("OtherEnemy"));
	FGameXXKCardCombatUnit* Hero = FindFixtureUnit(CycleRuntime, TEXT("Hero"));
	if (!TestNotNull(TEXT("the Bluehorn fixture exposes the retained-armor target"), Bluehorn)
		|| !TestNotNull(TEXT("the Bluehorn fixture exposes a non-passive enemy"), OtherEnemy)
		|| !TestNotNull(TEXT("the Bluehorn fixture exposes the party control"), Hero))
	{
		return false;
	}
	Bluehorn->Armor = 9;
	OtherEnemy->Armor = 9;
	Hero->Armor = 7;

	TArray<FGameXXKCardDamageResult> FirstBoundaryResults;
	TestTrue(TEXT("ending the player phase enters the next enemy armor-expiry boundary"),
		GameXXKCardRules::EndPlayerCardPhase(CycleRuntime, FirstBoundaryResults, &Error));
	TestEqual(TEXT("Bluehorn floors nine armor to four at the existing enemy-phase-start boundary"), FindFixtureUnit(CycleRuntime, TEXT("Bluehorn"))->Armor, 4);
	TestEqual(TEXT("a non-passive enemy still loses all armor at the same boundary"), FindFixtureUnit(CycleRuntime, TEXT("OtherEnemy"))->Armor, 0);
	TestEqual(TEXT("the enemy armor boundary never clears party armor"), FindFixtureUnit(CycleRuntime, TEXT("Hero"))->Armor, 7);
	const FGameXXKEnemyBattleState* BluehornState = CycleRuntime.EnemyStates.Find(TEXT("Bluehorn"));
	TestNotNull(TEXT("Bluehorn armor retention initializes missing persisted enemy state"), BluehornState);
	if (!BluehornState)
	{
		return false;
	}
	TestEqual(TEXT("Bluehorn armor retention initializes the catalog definition identity"), BluehornState->DefinitionId, FName(TEXT("Enemy.Ch1.BluehornGoatKing")));

	TArray<FGameXXKCardDamageResult> EnemyEndResults;
	TestTrue(TEXT("the Bluehorn boundary fixture can return to a player phase"),
		GameXXKCardRules::BeginNextPlayerCardRound(CycleRuntime, EnemyEndResults, &Error));
	const TArray<uint8> SavedRuntimeBytes = SerializeRuntimeForSaveGame(CycleRuntime);
	FGameXXKCardBattleRuntime ReloadedRuntime;
	TestTrue(TEXT("Bluehorn retained armor survives the SaveGame archive"), DeserializeRuntimeFromSaveGame(SavedRuntimeBytes, ReloadedRuntime));
	FGameXXKCardCombatUnit* ReloadedBluehorn = FindFixtureUnit(ReloadedRuntime, TEXT("Bluehorn"));
	TestNotNull(TEXT("the reloaded Bluehorn remains addressable"), ReloadedBluehorn);
	if (!ReloadedBluehorn)
	{
		return false;
	}
	TestEqual(TEXT("the reloaded Bluehorn keeps the first retained amount"), ReloadedBluehorn->Armor, 4);
	TestTrue(TEXT("the next player phase reaches a second Bluehorn armor boundary"),
		GameXXKCardRules::EndPlayerCardPhase(ReloadedRuntime, FirstBoundaryResults, &Error));
	TestEqual(TEXT("Bluehorn retains four armor as two on the following enemy phase"), FindFixtureUnit(ReloadedRuntime, TEXT("Bluehorn"))->Armor, 2);

	FGameXXKCardBattleRuntime EvenRuntime;
	if (!TestTrue(TEXT("the even-rounding fixture initializes"), InitializeBluehornFixture(EvenRuntime, Error)))
	{
		return false;
	}
	FindFixtureUnit(EvenRuntime, TEXT("Bluehorn"))->Armor = 8;
	TestTrue(TEXT("the even-rounding fixture reaches its enemy armor boundary"),
		GameXXKCardRules::EndPlayerCardPhase(EvenRuntime, FirstBoundaryResults, &Error));
	TestEqual(TEXT("Bluehorn retains exactly half of even armor"), FindFixtureUnit(EvenRuntime, TEXT("Bluehorn"))->Armor, 4);

	FGameXXKCardBattleRuntime MinimumRuntime;
	if (!TestTrue(TEXT("the minimum-rounding fixture initializes"), InitializeBluehornFixture(MinimumRuntime, Error)))
	{
		return false;
	}
	FindFixtureUnit(MinimumRuntime, TEXT("Bluehorn"))->Armor = 1;
	TestTrue(TEXT("the one-armor fixture reaches its enemy armor boundary"),
		GameXXKCardRules::EndPlayerCardPhase(MinimumRuntime, FirstBoundaryResults, &Error));
	TestEqual(TEXT("Bluehorn floors one armor to zero"), FindFixtureUnit(MinimumRuntime, TEXT("Bluehorn"))->Armor, 0);
	TestTrue(TEXT("the zero-armor fixture returns to a player phase"),
		GameXXKCardRules::BeginNextPlayerCardRound(MinimumRuntime, EnemyEndResults, &Error));
	TestTrue(TEXT("the zero-armor fixture reaches another enemy armor boundary"),
		GameXXKCardRules::EndPlayerCardPhase(MinimumRuntime, FirstBoundaryResults, &Error));
	TestEqual(TEXT("Bluehorn retains zero armor as zero"), FindFixtureUnit(MinimumRuntime, TEXT("Bluehorn"))->Armor, 0);

	FGameXXKCardBattleRuntime MismatchedStateRuntime;
	if (!TestTrue(TEXT("the Bluehorn mismatched-state fixture initializes"), InitializeBluehornFixture(MismatchedStateRuntime, Error)))
	{
		return false;
	}
	FindFixtureUnit(MismatchedStateRuntime, TEXT("Bluehorn"))->Armor = 9;
	FGameXXKEnemyBattleState& MismatchedState = MismatchedStateRuntime.EnemyStates.FindOrAdd(TEXT("Bluehorn"));
	MismatchedState.DefinitionId = TEXT("Enemy.Ch1.Goat");
	const TArray<uint8> BeforeRejectedBoundary = SerializeRuntimeForSaveGame(MismatchedStateRuntime);
	TArray<FGameXXKCardDamageResult> PreservedResults;
	FGameXXKCardDamageResult& PreservedResult = PreservedResults.AddDefaulted_GetRef();
	PreservedResult.SourceUnitId = TEXT("Bluehorn.OutputMustRemainUnchanged");
	PreservedResult.OriginalTargetUnitId = TEXT("Bluehorn.OutputTargetMustRemainUnchanged");
	PreservedResult.RequestedDamage = 77;
	Error.Reset();
	TestFalse(TEXT("a mismatched Bluehorn persisted definition rejects its armor-retention boundary"),
		GameXXKCardRules::EndPlayerCardPhase(MismatchedStateRuntime, PreservedResults, &Error));
	TestEqual(TEXT("a rejected Bluehorn boundary preserves the runtime byte-for-byte"),
		SerializeRuntimeForSaveGame(MismatchedStateRuntime), BeforeRejectedBoundary);
	if (!TestEqual(TEXT("a rejected Bluehorn boundary preserves output count"), PreservedResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("a rejected Bluehorn boundary preserves output source"), PreservedResults[0].SourceUnitId, FName(TEXT("Bluehorn.OutputMustRemainUnchanged")));
	TestEqual(TEXT("a rejected Bluehorn boundary preserves output target"), PreservedResults[0].OriginalTargetUnitId, FName(TEXT("Bluehorn.OutputTargetMustRemainUnchanged")));
	TestEqual(TEXT("a rejected Bluehorn boundary preserves output payload"), PreservedResults[0].RequestedDamage, 77);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRedtuskRagePassiveTest,
	"GameXXK.Battle.EnemyMechanics.RedtuskRageOnlyOnActualPlayerCardHealthHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRedtuskRagePassiveTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardDamageContext PlayerCardHit;
	PlayerCardHit.SourceUnitId = TEXT("Hero");
	PlayerCardHit.Kind = EGameXXKCardDamageKind::SingleTargetAttack;

	FGameXXKCardBattleRuntime CapRuntime;
	if (!TestTrue(TEXT("the Redtusk rage-cap fixture initializes"), InitializeRedtuskFixture(CapRuntime, Error)))
	{
		return false;
	}
	for (int32 ExpectedRage = 1; ExpectedRage <= 6; ++ExpectedRage)
	{
		FGameXXKCardDamageResult HitResult;
		if (!TestTrue(FString::Printf(TEXT("the Redtusk direct player-card hit %d resolves"), ExpectedRage),
			GameXXKCardRules::ApplyPlayerCardDirectDamage(CapRuntime, PlayerCardHit, TEXT("Redtusk"), 10, HitResult, &Error)))
		{
			return false;
		}
		TestTrue(FString::Printf(TEXT("the Redtusk direct player-card hit %d actually reaches health"), ExpectedRage), HitResult.HealthDamage > 0);
		const FGameXXKCardCombatUnit* Redtusk = FindFixtureUnit(CapRuntime, TEXT("Redtusk"));
		if (!TestNotNull(TEXT("the Redtusk rage-cap fixture keeps its target addressable"), Redtusk))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("Redtusk saves one rage stack per eligible player-card hit %d"), ExpectedRage),
			GameXXKCardRules::GetCombatStatusStacks(*Redtusk, EGameXXKCardStatus::Rage), FMath::Min(ExpectedRage, 5));
	}
	const TArray<uint8> SavedCapRuntime = SerializeRuntimeForSaveGame(CapRuntime);
	FGameXXKCardBattleRuntime ReloadedCapRuntime;
	if (!TestTrue(TEXT("the Redtusk rage-cap fixture reloads through the SaveGame archive"), DeserializeRuntimeFromSaveGame(SavedCapRuntime, ReloadedCapRuntime)))
	{
		return false;
	}
	const FGameXXKCardCombatUnit* ReloadedRedtusk = FindFixtureUnit(ReloadedCapRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the reloaded Redtusk keeps its combat-status source of truth"), ReloadedRedtusk))
	{
		return false;
	}
	TestEqual(TEXT("Redtusk rage persists as the saved combat status rather than transient enemy state"),
		GameXXKCardRules::GetCombatStatusStacks(*ReloadedRedtusk, EGameXXKCardStatus::Rage), 5);

	FGameXXKCardBattleRuntime ExternalRageCapRuntime;
	if (!TestTrue(TEXT("the externally-applied Redtusk rage fixture initializes"), InitializeRedtuskFixture(ExternalRageCapRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ExternallyRagedRedtusk = FindFixtureUnit(ExternalRageCapRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the externally-applied Redtusk rage target exists"), ExternallyRagedRedtusk))
	{
		return false;
	}
	TestEqual(TEXT("six externally applied Rage stacks clamp to Redtusk's approved five-stack ceiling"),
		GameXXKCardRules::AddCombatStatus(*ExternallyRagedRedtusk, EGameXXKCardStatus::Rage, 6),
		5);
	TestEqual(TEXT("the externally applied Rage state never stores more than five stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*ExternallyRagedRedtusk, EGameXXKCardStatus::Rage),
		5);
	TestTrue(TEXT("the five-stack externally applied Rage runtime remains valid for SaveGame persistence"),
		GameXXKCardRules::ValidateCardBattleRuntime(ExternalRageCapRuntime, &Error));

	FGameXXKCardBattleRuntime InvalidSavedRageRuntime;
	if (!TestTrue(TEXT("the invalid saved Rage fixture initializes"), InitializeRedtuskFixture(InvalidSavedRageRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* InvalidSavedRageRedtusk = FindFixtureUnit(InvalidSavedRageRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the invalid saved Rage target exists"), InvalidSavedRageRedtusk))
	{
		return false;
	}
	InvalidSavedRageRedtusk->Statuses = {FGameXXKCardStatusStack{EGameXXKCardStatus::Rage, 6}};
	const TArray<uint8> SerializedInvalidSavedRageRuntime = SerializeRuntimeForSaveGame(InvalidSavedRageRuntime);
	FGameXXKCardBattleRuntime ReloadedInvalidSavedRageRuntime;
	if (!TestTrue(TEXT("the six-stack Rage SaveGame fixture round-trips for validation"),
		DeserializeRuntimeFromSaveGame(SerializedInvalidSavedRageRuntime, ReloadedInvalidSavedRageRuntime)))
	{
		return false;
	}
	Error.Reset();
	TestFalse(TEXT("a six-stack Rage state is rejected by the persisted combat-runtime validator"),
		GameXXKCardRules::ValidateCardBattleRuntime(ReloadedInvalidSavedRageRuntime, &Error));

	FGameXXKCardBattleRuntime ArmorRuntime;
	if (!TestTrue(TEXT("the Redtusk armor exclusion fixture initializes"), InitializeRedtuskFixture(ArmorRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ArmoredRedtusk = FindFixtureUnit(ArmorRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the Redtusk armor exclusion target exists"), ArmoredRedtusk))
	{
		return false;
	}
	ArmoredRedtusk->Armor = 20;
	FGameXXKCardDamageResult ArmorResult;
	if (!TestTrue(TEXT("an armor-absorbed player-card hit against Redtusk resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(ArmorRuntime, PlayerCardHit, TEXT("Redtusk"), 10, ArmorResult, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the armor exclusion leaves no Redtusk health damage"), ArmorResult.HealthDamage, 0);
	TestEqual(TEXT("the armor exclusion grants no Redtusk rage"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(ArmorRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 0);

	FGameXXKCardBattleRuntime AgilityRuntime;
	if (!TestTrue(TEXT("the Redtusk agility exclusion fixture initializes"), InitializeRedtuskFixture(AgilityRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* AgileRedtusk = FindFixtureUnit(AgilityRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the Redtusk agility exclusion target exists"), AgileRedtusk))
	{
		return false;
	}
	GameXXKCardRules::AddCombatStatus(*AgileRedtusk, EGameXXKCardStatus::Agility, 1);
	FGameXXKCardDamageResult AgilityResult;
	if (!TestTrue(TEXT("an agility-avoided player-card hit against Redtusk resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(AgilityRuntime, PlayerCardHit, TEXT("Redtusk"), 10, AgilityResult, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the agility exclusion marks the Redtusk hit avoided"), AgilityResult.bAvoidedByAgility);
	TestEqual(TEXT("the agility exclusion grants no Redtusk rage"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(AgilityRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 0);

	FGameXXKCardBattleRuntime RedirectRuntime;
	if (!TestTrue(TEXT("the Redtusk redirect exclusion fixture initializes"), InitializeRedtuskFixture(RedirectRuntime, Error, true)))
	{
		return false;
	}
	FGameXXKCardGuardLinkRuntime& Redirect = RedirectRuntime.GuardLinks.AddDefaulted_GetRef();
	Redirect.GuardianUnitId = TEXT("Guardian");
	Redirect.ProtectedUnitId = TEXT("Redtusk");
	Redirect.Stacks = 1;
	Redirect.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	FGameXXKCardDamageResult RedirectResult;
	if (!TestTrue(TEXT("a player-card hit redirected away from Redtusk resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(RedirectRuntime, PlayerCardHit, TEXT("Redtusk"), 10, RedirectResult, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the Redtusk redirect exclusion reports the actual receiver change"), RedirectResult.bRedirected);
	TestEqual(TEXT("the Redtusk redirect exclusion resolves damage to the guardian"), RedirectResult.ResolvedTargetUnitId, FName(TEXT("Guardian")));
	TestEqual(TEXT("the Redtusk redirect exclusion grants no rage when Redtusk is not the actual receiver"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(RedirectRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 0);

	FGameXXKCardBattleRuntime RedirectIntoRedtuskRuntime;
	if (!TestTrue(TEXT("the Redtusk redirect-into fixture initializes"), InitializeRedtuskFixture(RedirectIntoRedtuskRuntime, Error, true)))
	{
		return false;
	}
	FGameXXKCardGuardLinkRuntime& RedirectIntoRedtusk = RedirectIntoRedtuskRuntime.GuardLinks.AddDefaulted_GetRef();
	RedirectIntoRedtusk.GuardianUnitId = TEXT("Redtusk");
	RedirectIntoRedtusk.ProtectedUnitId = TEXT("Guardian");
	RedirectIntoRedtusk.Stacks = 1;
	RedirectIntoRedtusk.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	FGameXXKCardDamageResult RedirectIntoRedtuskResult;
	if (!TestTrue(TEXT("a player-card hit against another enemy can redirect into Redtusk"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(RedirectIntoRedtuskRuntime, PlayerCardHit, TEXT("Guardian"), 10, RedirectIntoRedtuskResult, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the redirect-into result exposes Redtusk as the actual receiver"), RedirectIntoRedtuskResult.bRedirected);
	TestEqual(TEXT("the redirect-into result resolves damage to Redtusk"), RedirectIntoRedtuskResult.ResolvedTargetUnitId, FName(TEXT("Redtusk")));
	TestEqual(TEXT("a redirected player-card health hit into Redtusk grants exactly one Rage stack"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(RedirectIntoRedtuskRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 1);

	FGameXXKCardBattleRuntime GenericDamageRuntime;
	if (!TestTrue(TEXT("the Redtusk generic-damage exclusion fixture initializes"), InitializeRedtuskFixture(GenericDamageRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardDamageResult GenericDamageResult;
	if (!TestTrue(TEXT("a generic non-player-card direct-damage packet against Redtusk resolves"),
		GameXXKCardRules::ApplyCombatDirectDamage(
			GenericDamageRuntime.Units,
			GenericDamageRuntime.GuardLinks,
			PlayerCardHit,
			TEXT("Redtusk"),
			10,
			GenericDamageResult,
			&Error)))
	{
		return false;
	}
	TestEqual(TEXT("generic direct damage grants no Redtusk Rage"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(GenericDamageRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 0);

	FGameXXKCardBattleRuntime DotRuntime;
	if (!TestTrue(TEXT("the Redtusk DoT exclusion fixture initializes"), InitializeRedtuskFixture(DotRuntime, Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* DotRedtusk = FindFixtureUnit(DotRuntime, TEXT("Redtusk"));
	if (!TestNotNull(TEXT("the Redtusk DoT exclusion target exists"), DotRedtusk))
	{
		return false;
	}
	GameXXKCardRules::AddCombatStatus(*DotRedtusk, EGameXXKCardStatus::Bleed, 1);
	GameXXKCardRules::AddCombatStatus(*DotRedtusk, EGameXXKCardStatus::Poison, 1);
	GameXXKCardRules::AddCombatStatus(*DotRedtusk, EGameXXKCardStatus::Burn, 1);
	DotRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	TArray<FGameXXKCardDamageResult> DotResults;
	if (!TestTrue(TEXT("the Redtusk end-phase bleed poison burn damage resolves"),
		GameXXKCardRules::BeginNextPlayerCardRound(DotRuntime, DotResults, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the Redtusk DoT exclusion actually dealt end-phase status damage"),
		DotResults.ContainsByPredicate([](const FGameXXKCardDamageResult& Result)
		{
			return Result.OriginalTargetUnitId == TEXT("Redtusk") && Result.HealthDamage > 0;
		}));
	TestEqual(TEXT("bleed poison burn and end-phase damage grant no Redtusk rage"),
		GameXXKCardRules::GetCombatStatusStacks(*FindFixtureUnit(DotRuntime, TEXT("Redtusk")), EGameXXKCardStatus::Rage), 0);

	return true;
}

namespace
{
	FGameXXKCardCombatUnit MakeWhiteApeStatusGuardFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 Attack,
		const int32 Defense,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeIronfeatherFixtureUnit(UnitId, Side, Health, Attack, Defense, StableSortOrder);
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.EnemyDefinitionId = TEXT("Enemy.Ch3.WhiteApe");
		}
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeWhiteApeStatusGuardFixtureCards(const FName CardId)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("WhiteApeStatusGuard.Card.%d"), Index));
			Card.CardId = CardId;
			Card.OwnerUnitId = TEXT("Hero");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("WhiteApeStatusGuard.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool InitializeWhiteApeStatusGuardFixture(
		FGameXXKCardBattleRuntime& OutRuntime,
		FString& OutError,
		const FName CardId)
	{
		TArray<FGameXXKCardCombatUnit> Units;
		Units.Add(MakeWhiteApeStatusGuardFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1));
		Units.Add(MakeWhiteApeStatusGuardFixtureUnit(TEXT("WhiteApe"), EGameXXKCardTargetSide::Enemy, 1000, 21, 0, 2));
		return GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeWhiteApeStatusGuardFixtureCards(CardId),
			Units,
			EGameXXKCardTerrain::Plain,
			2052,
			&OutError);
	}

	FGameXXKCardCombatUnit* FindWhiteApeStatusGuardFixtureUnit(FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("WhiteApe");
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardPrimitivePurityTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardAddCombatStatusIsPure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardPrimitivePurityTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("the White Ape primitive-purity fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(Runtime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardFixtureUnit(Runtime);
	if (!TestNotNull(TEXT("the primitive-purity fixture exposes White Ape"), WhiteApe))
	{
		return false;
	}
	TestNull(TEXT("the primitive-purity fixture starts without runtime enemy state"), Runtime.EnemyStates.Find(TEXT("WhiteApe")));
	TestEqual(TEXT("the primitive still applies its requested concrete status"),
		GameXXKCardRules::AddCombatStatus(*WhiteApe, EGameXXKCardStatus::Poison, 1), 1);
	TestEqual(TEXT("the primitive records its applied status normally"),
		GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Poison), 1);
	TestEqual(TEXT("the primitive never grants White Ape armor without a runtime application context"), WhiteApe->Armor, 0);
	TestNull(TEXT("the primitive never creates White Ape runtime state without a runtime application context"), Runtime.EnemyStates.Find(TEXT("WhiteApe")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardPlayerCardOnHitTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardPlayerCardOnHitPersistsOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardPlayerCardOnHitTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime Runtime;
	FString Error;
	if (!TestTrue(TEXT("the White Ape player-card on-hit fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(Runtime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	Runtime.EnemyStates.Reset();
	FGameXXKCardDamageContext FirstOnHitContext;
	FirstOnHitContext.SourceUnitId = TEXT("Hero");
	FirstOnHitContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& FirstOnHitStatus = FirstOnHitContext.OnHitStatuses.AddDefaulted_GetRef();
	FirstOnHitStatus.Status = EGameXXKCardStatus::Poison;
	FirstOnHitStatus.Stacks = 1;
	FGameXXKCardDamageResult FirstOnHitResult;
	if (!TestTrue(TEXT("a player-card direct hit can attach a status to White Ape"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, FirstOnHitContext, TEXT("WhiteApe"), 1, FirstOnHitResult, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardFixtureUnit(Runtime);
	const FGameXXKEnemyBattleState* WhiteApeState = Runtime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("a missing White Ape state initializes with the first applied player-card status"), WhiteApeState)
		|| !TestNotNull(TEXT("the player-card on-hit fixture retains White Ape"), WhiteApe))
	{
		return false;
	}
	TestEqual(TEXT("the first attached status reaches White Ape"),
		GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Poison), 1);
	TestEqual(TEXT("the first actually applied player-card status grants White Ape exactly eight armor"), WhiteApe->Armor, 8);
	TestFalse(TEXT("the first actually applied player-card status consumes White Ape's round guard"), WhiteApeState->bFirstStatusPassiveAvailable);
	TestEqual(TEXT("the initialized White Ape state records the authoritative catalog identity"),
		WhiteApeState->DefinitionId, FName(TEXT("Enemy.Ch3.WhiteApe")));

	const TArray<uint8> SavedRuntimeBytes = SerializeRuntimeForSaveGame(Runtime);
	FGameXXKCardBattleRuntime ReloadedRuntime;
	if (!TestTrue(TEXT("the consumed White Ape status guard SaveGame fixture reloads"),
		DeserializeRuntimeFromSaveGame(SavedRuntimeBytes, ReloadedRuntime)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ReloadedWhiteApe = FindWhiteApeStatusGuardFixtureUnit(ReloadedRuntime);
	const FGameXXKEnemyBattleState* ReloadedWhiteApeState = ReloadedRuntime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("the reloaded player-card fixture retains White Ape"), ReloadedWhiteApe)
		|| !TestNotNull(TEXT("the reloaded player-card fixture retains White Ape state"), ReloadedWhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("the White Ape armor grant survives the SaveGame archive"), ReloadedWhiteApe->Armor, 8);
	TestFalse(TEXT("the consumed White Ape guard survives the SaveGame archive"), ReloadedWhiteApeState->bFirstStatusPassiveAvailable);

	FGameXXKCardDamageContext SecondOnHitContext;
	SecondOnHitContext.SourceUnitId = TEXT("Hero");
	SecondOnHitContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& SecondOnHitStatus = SecondOnHitContext.OnHitStatuses.AddDefaulted_GetRef();
	SecondOnHitStatus.Status = EGameXXKCardStatus::Mark;
	SecondOnHitStatus.Stacks = 1;
	FGameXXKCardDamageResult SecondOnHitResult;
	const int32 ArmorBeforeSecondOnHit = ReloadedWhiteApe->Armor;
	if (!TestTrue(TEXT("a second player-card status attachment resolves after reload"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(ReloadedRuntime, SecondOnHitContext, TEXT("WhiteApe"), 1, SecondOnHitResult, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("a second actually applied player-card status does not stack White Ape guard armor beyond its direct-hit absorption"),
		FindWhiteApeStatusGuardFixtureUnit(ReloadedRuntime)->Armor, ArmorBeforeSecondOnHit - SecondOnHitResult.ArmorAbsorbed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardPerUnitIsolationTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardTracksEachLivingWhiteApeSeparately",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardPerUnitIsolationTest::RunTest(const FString& Parameters)
{
	FString Error;
	TArray<FGameXXKCardCombatUnit> Units;
	Units.Add(MakeWhiteApeStatusGuardFixtureUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 20, 0, 1));
	Units.Add(MakeWhiteApeStatusGuardFixtureUnit(TEXT("WhiteApe.Alpha"), EGameXXKCardTargetSide::Enemy, 1000, 21, 0, 2));
	Units.Add(MakeWhiteApeStatusGuardFixtureUnit(TEXT("WhiteApe.Beta"), EGameXXKCardTargetSide::Enemy, 1000, 21, 0, 3));
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("the two-White-Ape isolation fixture initializes"),
		GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeWhiteApeStatusGuardFixtureCards(TEXT("Hero.Generic.QingFengYiShi")),
			Units,
			EGameXXKCardTerrain::Plain,
			2053,
			&Error)))
	{
		return false;
	}
	Runtime.EnemyStates.Reset();
	const auto FindWhiteApe = [&Runtime](const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	};

	FGameXXKCardDamageContext AlphaStatusContext;
	AlphaStatusContext.SourceUnitId = TEXT("Hero");
	AlphaStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& AlphaStatus = AlphaStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	AlphaStatus.Status = EGameXXKCardStatus::Poison;
	AlphaStatus.Stacks = 1;
	FGameXXKCardDamageResult AlphaStatusResult;
	if (!TestTrue(TEXT("the first White Ape receives its own player-card status"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, AlphaStatusContext, TEXT("WhiteApe.Alpha"), 1, AlphaStatusResult, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Alpha = FindWhiteApe(TEXT("WhiteApe.Alpha"));
	FGameXXKCardCombatUnit* Beta = FindWhiteApe(TEXT("WhiteApe.Beta"));
	const FGameXXKEnemyBattleState* AlphaState = Runtime.EnemyStates.Find(TEXT("WhiteApe.Alpha"));
	if (!TestNotNull(TEXT("the first White Ape remains addressable after its status"), Alpha)
		|| !TestNotNull(TEXT("the untouched second White Ape remains addressable"), Beta)
		|| !TestNotNull(TEXT("the first White Ape initializes its own state"), AlphaState))
	{
		return false;
	}
	TestEqual(TEXT("the first White Ape receives exactly its own guard armor"), Alpha->Armor, 8);
	TestEqual(TEXT("an untouched White Ape receives no sibling guard armor"), Beta->Armor, 0);
	TestFalse(TEXT("the first White Ape consumes only its own guard"), AlphaState->bFirstStatusPassiveAvailable);

	FGameXXKCardDamageContext BetaStatusContext;
	BetaStatusContext.SourceUnitId = TEXT("Hero");
	BetaStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& BetaStatus = BetaStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	BetaStatus.Status = EGameXXKCardStatus::Mark;
	BetaStatus.Stacks = 1;
	FGameXXKCardDamageResult BetaStatusResult;
	if (!TestTrue(TEXT("the second White Ape receives its own first player-card status"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, BetaStatusContext, TEXT("WhiteApe.Beta"), 1, BetaStatusResult, &Error)))
	{
		return false;
	}
	Alpha = FindWhiteApe(TEXT("WhiteApe.Alpha"));
	Beta = FindWhiteApe(TEXT("WhiteApe.Beta"));
	const FGameXXKEnemyBattleState* BetaState = Runtime.EnemyStates.Find(TEXT("WhiteApe.Beta"));
	if (!TestNotNull(TEXT("the second White Ape initializes its own state"), BetaState)
		|| !TestNotNull(TEXT("the first White Ape survives the second status"), Alpha)
		|| !TestNotNull(TEXT("the second White Ape survives its status"), Beta))
	{
		return false;
	}
	TestEqual(TEXT("the first White Ape keeps exactly one armor grant"), Alpha->Armor, 8);
	TestEqual(TEXT("the second White Ape independently receives exactly one armor grant"), Beta->Armor, 8);
	TestFalse(TEXT("the second White Ape independently consumes its own guard"), BetaState->bFirstStatusPassiveAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardCardApplyStatusTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardCardApplyStatusInitializesStateAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardCardApplyStatusTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("the White Ape normal ApplyStatus fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(Runtime, Error, TEXT("Route.General.TieJiLi"))))
	{
		return false;
	}
	Runtime.EnemyStates.Reset();
	FGameXXKCardPlayResult FirstStatusCardResult;
	TestTrue(TEXT("a normal card ApplyStatus resolves against White Ape"),
		GameXXKCardRules::ResolveCardPlay(Runtime, Runtime.Deck.Hand[0].InstanceId, TEXT("WhiteApe"), FirstStatusCardResult, &Error));
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardFixtureUnit(Runtime);
	const FGameXXKEnemyBattleState* WhiteApeState = Runtime.EnemyStates.Find(TEXT("WhiteApe"));
	TestNotNull(TEXT("normal ApplyStatus initializes missing White Ape state"), WhiteApeState);
	TestNotNull(TEXT("the normal ApplyStatus fixture retains White Ape"), WhiteApe);
	if (WhiteApe)
	{
		TestEqual(TEXT("the normal card applies its first poison packet"),
			GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Poison), 2);
		TestEqual(TEXT("the normal card applies its second vulnerability packet"),
			GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Vulnerability), 1);
		TestEqual(TEXT("multiple actual status packets from one normal card grant White Ape armor only once"), WhiteApe->Armor, 8);
	}
	if (WhiteApeState)
	{
		TestEqual(TEXT("normal ApplyStatus records White Ape's catalog identity"), WhiteApeState->DefinitionId, FName(TEXT("Enemy.Ch3.WhiteApe")));
		TestFalse(TEXT("the first normal ApplyStatus packet consumes White Ape's round guard"), WhiteApeState->bFirstStatusPassiveAvailable);
	}

	const TArray<uint8> SavedRuntimeBytes = SerializeRuntimeForSaveGame(Runtime);
	FGameXXKCardBattleRuntime ReloadedRuntime;
	TestTrue(TEXT("the normal ApplyStatus White Ape state round-trips through SaveGame"),
		DeserializeRuntimeFromSaveGame(SavedRuntimeBytes, ReloadedRuntime));
	FGameXXKCardCombatUnit* ReloadedWhiteApe = FindWhiteApeStatusGuardFixtureUnit(ReloadedRuntime);
	const FGameXXKEnemyBattleState* ReloadedWhiteApeState = ReloadedRuntime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("the reloaded normal ApplyStatus fixture retains White Ape"), ReloadedWhiteApe)
		|| !TestNotNull(TEXT("the reloaded normal ApplyStatus fixture retains White Ape state"), ReloadedWhiteApeState))
	{
		return false;
	}
	if (!TestTrue(TEXT("the reloaded normal ApplyStatus fixture retains a subsequent status card"),
		ReloadedRuntime.Deck.Hand.IsValidIndex(0)))
	{
		return false;
	}
	FGameXXKCardPlayResult ReloadedStatusCardResult;
	TestTrue(TEXT("a subsequent normal card status resolves after reload"),
		GameXXKCardRules::ResolveCardPlay(ReloadedRuntime, ReloadedRuntime.Deck.Hand[0].InstanceId, TEXT("WhiteApe"), ReloadedStatusCardResult, &Error));
	ReloadedWhiteApe = FindWhiteApeStatusGuardFixtureUnit(ReloadedRuntime);
	ReloadedWhiteApeState = ReloadedRuntime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("the subsequent normal card status retains White Ape"), ReloadedWhiteApe)
		|| !TestNotNull(TEXT("the subsequent normal card status retains White Ape state"), ReloadedWhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("a subsequent normal card status does not stack saved White Ape guard armor"), ReloadedWhiteApe->Armor, 8);
	TestFalse(TEXT("a subsequent normal card status keeps White Ape guard consumed"), ReloadedWhiteApeState->bFirstStatusPassiveAvailable);

	FGameXXKCardBattleRuntime MismatchedRuntime;
	if (!TestTrue(TEXT("the White Ape normal ApplyStatus mismatch fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(MismatchedRuntime, Error, TEXT("Route.General.TieJiLi"))))
	{
		return false;
	}
	FGameXXKEnemyBattleState& MismatchedState = MismatchedRuntime.EnemyStates.FindOrAdd(TEXT("WhiteApe"));
	MismatchedState.DefinitionId = TEXT("Enemy.Ch1.Goat");
	MismatchedState.bFirstStatusPassiveAvailable = true;
	const TArray<uint8> BeforeRejectedStatusCard = SerializeRuntimeForSaveGame(MismatchedRuntime);
	FGameXXKCardPlayResult RejectedStatusCardResult;
	RejectedStatusCardResult.CardInstanceId = TEXT("WhiteApe.StatusGuard.ResultMustRemainUnchanged");
	TestFalse(TEXT("a mismatched persisted White Ape definition rejects normal ApplyStatus"),
		GameXXKCardRules::ResolveCardPlay(MismatchedRuntime, MismatchedRuntime.Deck.Hand[0].InstanceId, TEXT("WhiteApe"), RejectedStatusCardResult, &Error));
	TestEqual(TEXT("a rejected mismatched normal ApplyStatus preserves its caller result"),
		RejectedStatusCardResult.CardInstanceId, FName(TEXT("WhiteApe.StatusGuard.ResultMustRemainUnchanged")));
	TestEqual(TEXT("a rejected mismatched normal ApplyStatus leaks no partial runtime state"),
		SerializeRuntimeForSaveGame(MismatchedRuntime), BeforeRejectedStatusCard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardUnappliedStatusTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardIgnoresUnappliedStatuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardUnappliedStatusTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("the White Ape unapplied-status fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(Runtime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	Runtime.EnemyStates.Reset();
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardFixtureUnit(Runtime);
	if (!TestNotNull(TEXT("the unapplied-status fixture exposes White Ape"), WhiteApe))
	{
		return false;
	}
	TestEqual(TEXT("the immunity fixture can apply CannotReceiveVulnerability"),
		GameXXKCardRules::AddCombatStatus(*WhiteApe, EGameXXKCardStatus::CannotReceiveVulnerability, 1), 1);

	FGameXXKCardDamageContext ImmuneStatusContext;
	ImmuneStatusContext.SourceUnitId = TEXT("Hero");
	ImmuneStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& ImmuneStatus = ImmuneStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	ImmuneStatus.Status = EGameXXKCardStatus::Vulnerability;
	ImmuneStatus.Stacks = 1;
	FGameXXKCardDamageResult ImmuneStatusResult;
	TestTrue(TEXT("an immune player-card status packet resolves without applying vulnerability"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, ImmuneStatusContext, TEXT("WhiteApe"), 1, ImmuneStatusResult, &Error));
	const FGameXXKEnemyBattleState* WhiteApeState = Runtime.EnemyStates.Find(TEXT("WhiteApe"));
	TestNotNull(TEXT("the immune-status fixture retains White Ape state"), WhiteApeState);
	if (WhiteApeState)
	{
		TestTrue(TEXT("an immune status does not consume White Ape guard"), WhiteApeState->bFirstStatusPassiveAvailable);
	}
	TestEqual(TEXT("an immune status does not grant White Ape armor"), FindWhiteApeStatusGuardFixtureUnit(Runtime)->Armor, 0);

	TestEqual(TEXT("a no-op invalid status cannot be added directly"),
		GameXXKCardRules::AddCombatStatus(*FindWhiteApeStatusGuardFixtureUnit(Runtime), EGameXXKCardStatus::None, 1), 0);
	TestEqual(TEXT("a no-op zero-stack status cannot be added directly"),
		GameXXKCardRules::AddCombatStatus(*FindWhiteApeStatusGuardFixtureUnit(Runtime), EGameXXKCardStatus::Burn, 0), 0);

	FGameXXKCardDamageContext ValidStatusContext;
	ValidStatusContext.SourceUnitId = TEXT("Hero");
	ValidStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& ValidStatus = ValidStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	ValidStatus.Status = EGameXXKCardStatus::Burn;
	ValidStatus.Stacks = 1;
	FGameXXKCardDamageResult ValidStatusResult;
	TestTrue(TEXT("the first actually applied status after immune and no-op packets resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(Runtime, ValidStatusContext, TEXT("WhiteApe"), 1, ValidStatusResult, &Error));
	WhiteApeState = Runtime.EnemyStates.Find(TEXT("WhiteApe"));
	if (WhiteApeState)
	{
		TestFalse(TEXT("the first actually applied status after no-op packets consumes White Ape guard"), WhiteApeState->bFirstStatusPassiveAvailable);
	}
	TestEqual(TEXT("the first actually applied status after no-op packets grants White Ape armor"),
		FindWhiteApeStatusGuardFixtureUnit(Runtime)->Armor, 8);

	FGameXXKCardBattleRuntime LethalRuntime;
	if (!TestTrue(TEXT("the White Ape lethal-status fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(LethalRuntime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	FGameXXKCardCombatUnit* LethalWhiteApe = FindWhiteApeStatusGuardFixtureUnit(LethalRuntime);
	if (!TestNotNull(TEXT("the lethal-status fixture exposes White Ape"), LethalWhiteApe))
	{
		return false;
	}
	FGameXXKEnemyBattleState& LethalWhiteApeState = LethalRuntime.EnemyStates.FindOrAdd(TEXT("WhiteApe"));
	LethalWhiteApeState.DefinitionId = TEXT("Enemy.Ch3.WhiteApe");
	LethalWhiteApeState.bFirstStatusPassiveAvailable = true;
	LethalWhiteApe->HP = 1;
	FGameXXKCardDamageResult LethalStatusResult;
	TestTrue(TEXT("a lethal player-card hit carrying an on-hit status resolves"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(LethalRuntime, ValidStatusContext, TEXT("WhiteApe"), 10, LethalStatusResult, &Error));
	LethalWhiteApe = FindWhiteApeStatusGuardFixtureUnit(LethalRuntime);
	const FGameXXKEnemyBattleState* ResolvedLethalWhiteApeState = LethalRuntime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("the lethal-status result retains the defeated White Ape record"), LethalWhiteApe)
		|| !TestNotNull(TEXT("the lethal-status result retains White Ape state"), ResolvedLethalWhiteApeState))
	{
		return false;
	}
	TestFalse(TEXT("the lethal status target is defeated before its on-hit status can apply"), LethalWhiteApe->bLiving);
	TestEqual(TEXT("a lethal on-hit packet adds no status to a defeated White Ape"),
		GameXXKCardRules::GetCombatStatusStacks(*LethalWhiteApe, EGameXXKCardStatus::Burn), 0);
	TestEqual(TEXT("a lethal on-hit packet grants no White Ape guard armor"), LethalWhiteApe->Armor, 0);
	TestTrue(TEXT("a lethal on-hit packet does not consume White Ape guard"), ResolvedLethalWhiteApeState->bFirstStatusPassiveAvailable);

	FGameXXKCardBattleRuntime DeadRuntime;
	if (!TestTrue(TEXT("the White Ape defeated-status fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(DeadRuntime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	FGameXXKCardCombatUnit* DeadWhiteApe = FindWhiteApeStatusGuardFixtureUnit(DeadRuntime);
	FGameXXKEnemyBattleState& DeadWhiteApeState = DeadRuntime.EnemyStates.FindOrAdd(TEXT("WhiteApe"));
	DeadWhiteApeState.DefinitionId = TEXT("Enemy.Ch3.WhiteApe");
	DeadWhiteApeState.bFirstStatusPassiveAvailable = true;
	DeadWhiteApe->HP = 0;
	DeadWhiteApe->bLiving = false;
	const TArray<uint8> BeforeDefeatedStatus = SerializeRuntimeForSaveGame(DeadRuntime);
	FGameXXKCardDamageResult RejectedDefeatedStatusResult;
	RejectedDefeatedStatusResult.OriginalTargetUnitId = TEXT("WhiteApe.StatusGuard.DeadResultMustRemainUnchanged");
	TestFalse(TEXT("a defeated White Ape cannot receive a player-card status packet"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(DeadRuntime, ValidStatusContext, TEXT("WhiteApe"), 1, RejectedDefeatedStatusResult, &Error));
	TestEqual(TEXT("a rejected defeated status packet preserves White Ape runtime state"),
		SerializeRuntimeForSaveGame(DeadRuntime), BeforeDefeatedStatus);
	TestEqual(TEXT("a rejected defeated status packet preserves its caller result"),
		RejectedDefeatedStatusResult.OriginalTargetUnitId, FName(TEXT("WhiteApe.StatusGuard.DeadResultMustRemainUnchanged")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardReactiveStatusTest,
	"GameXXK.Battle.EnemyMechanics.WhiteApeStatusGuardReactiveFirstDirectDamageStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardReactiveStatusTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("the White Ape reactive-status fixture initializes"),
		InitializeWhiteApeStatusGuardFixture(Runtime, Error, TEXT("Hero.Generic.QingFengYiShi"))))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the reactive-status fixture reaches the enemy phase"),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, EndPlayerResults, &Error)))
	{
		return false;
	}
	FGameXXKCardBattleModifierRuntime& ReactiveStatusModifier = Runtime.Modifiers.AddDefaulted_GetRef();
	ReactiveStatusModifier.ModifierId = TEXT("WhiteApe.StatusGuard.ReactiveStatus");
	ReactiveStatusModifier.SourceCardInstanceId = TEXT("WhiteApe.StatusGuard.ReactiveSource");
	ReactiveStatusModifier.SourceUnitId = TEXT("Hero");
	ReactiveStatusModifier.RecipientUnitIds = {TEXT("Hero")};
	ReactiveStatusModifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound;
	ReactiveStatusModifier.Definition.EffectType = EGameXXKCardEffectType::ApplyStatus;
	ReactiveStatusModifier.Definition.Target = EGameXXKCardEffectTarget::Attacker;
	ReactiveStatusModifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
	ReactiveStatusModifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
	ReactiveStatusModifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	ReactiveStatusModifier.Definition.Status = EGameXXKCardStatus::Poison;
	ReactiveStatusModifier.Definition.Magnitude = 1;
	ReactiveStatusModifier.Definition.RemainingTriggers = 1;
	ReactiveStatusModifier.Definition.bPersistent = true;
	if (!TestTrue(TEXT("the reactive status modifier fixture remains a valid battle runtime"),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error)))
	{
		return false;
	}

	FGameXXKCardDamageContext EnemyAttackContext;
	EnemyAttackContext.SourceUnitId = TEXT("WhiteApe");
	EnemyAttackContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult EnemyAttackResult;
	TArray<FGameXXKCardDamageResult> ReactiveDamageResults;
	if (!TestTrue(TEXT("a White Ape attack triggers the first-direct-damage status reaction"),
		GameXXKCardRules::ResolveEnemyDirectAttack(Runtime, EnemyAttackContext, TEXT("Hero"), 5, EnemyAttackResult, &ReactiveDamageResults, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardFixtureUnit(Runtime);
	const FGameXXKEnemyBattleState* WhiteApeState = Runtime.EnemyStates.Find(TEXT("WhiteApe"));
	if (!TestNotNull(TEXT("the reactive status path initializes White Ape state"), WhiteApeState)
		|| !TestNotNull(TEXT("the reactive status path retains White Ape"), WhiteApe))
	{
		return false;
	}
	TestEqual(TEXT("the first-direct-damage reaction actually applies its status to White Ape"),
		GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Poison), 1);
	TestEqual(TEXT("the first-direct-damage reaction grants White Ape exactly eight armor"), WhiteApe->Armor, 8);
	TestFalse(TEXT("the first-direct-damage reaction consumes White Ape's round guard"), WhiteApeState->bFirstStatusPassiveAvailable);
	TestTrue(TEXT("the consumed first-direct-damage status reaction is removed"), Runtime.Modifiers.IsEmpty());
	return true;
}

#endif
