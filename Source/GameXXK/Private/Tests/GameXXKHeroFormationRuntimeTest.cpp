#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroFormationRuntimeTest
{
	const FName HeroId(TEXT("Hero"));
	const FName AllyId(TEXT("Ally"));
	const FName EnemyAId(TEXT("EnemyA"));
	const FName EnemyBId(TEXT("EnemyB"));

	const TArray<EGameXXKCardTerrain>& AllTerrains()
	{
		static const TArray<EGameXXKCardTerrain> Terrains = {
			EGameXXKCardTerrain::Plain,
			EGameXXKCardTerrain::Cliff,
			EGameXXKCardTerrain::Forest,
			EGameXXKCardTerrain::WaterShore,
			EGameXXKCardTerrain::Ferry,
			EGameXXKCardTerrain::Village,
			EGameXXKCardTerrain::Cave};
		return Terrains;
	}

	const TArray<FName>& FormationCardIds()
	{
		static const TArray<FName> CardIds = {
			TEXT("Hero.Formation.GuanShiLuoZi"),
			TEXT("Hero.Formation.YiZhenHuiXiang"),
			TEXT("Hero.Formation.LianYingBuShi"),
			TEXT("Hero.Formation.LiuHeGuiYi")};
		return CardIds;
	}

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? (UnitId == HeroId ? 60 : 50) : 5000;
		Unit.MaxHP = Side == EGameXXKCardTargetSide::Party ? 100 : 5000;
		Unit.Attack = UnitId == HeroId ? 10 : 8;
		Unit.Defense = 0;
		Unit.Armor = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? (UnitId == HeroId ? 30 : 20) : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits()
	{
		return {
			MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 2),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
	}

	FGameXXKCardInstance MakeCard(
		const FName InstanceId,
		const FName CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HeroId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Formation.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	TArray<FGameXXKCardInstance> MakeCards(const FName PrimaryCardId)
	{
		TArray<FGameXXKCardInstance> Cards;
		Cards.Add(MakeCard(TEXT("Primary"), PrimaryCardId, 0));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("Active.%d"), Index)),
				TEXT("Hero.Generic.NingShenTuNa"),
				Index + 1));
		}
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("Draw.%d"), Index)),
				TEXT("Hero.Generic.QingFengYiShi"),
				Index + 4));
		}
		return Cards;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const FName PrimaryCardId,
		const EGameXXKCardTerrain Terrain,
		const int32 ActiveCardsInHand,
		const int32 Seed)
	{
		const TArray<FGameXXKCardInstance> Cards = MakeCards(PrimaryCardId);
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(),
			Terrain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Formation runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			const bool bPrimary = Card.InstanceId == TEXT("Primary");
			const bool bRequestedActive = Card.InstanceId.ToString().StartsWith(TEXT("Active."))
				&& FCString::Atoi(*Card.InstanceId.ToString().RightChop(7)) < ActiveCardsInHand;
			(bPrimary || bRequestedActive ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Deterministic Formation fixture is invalid: %s"), *Error));
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

	int32 CountReactions(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardStatus StatusType)
	{
		return Runtime.Reactions.FilterByPredicate([StatusType](const FGameXXKReactionRuntime& Reaction)
		{
			return Reaction.Status == StatusType;
		}).Num();
	}

	int32 CountModifiers(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardBattleModifierTrigger Trigger)
	{
		return Runtime.Modifiers.FilterByPredicate([Trigger](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return Modifier.Definition.Trigger == Trigger;
		}).Num();
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const FString& Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), *Context, *Error), bResolved);
		return bResolved;
	}

	void AssertBenefit(
		FAutomationTestBase& Test,
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardTerrain Terrain,
		const int32 Repetitions,
		const int32 OwnerManaAfterCost,
		const int32 ExpectedHandCount,
		const FString& Context)
	{
		const FGameXXKCardCombatUnit* Hero = FindUnit(Runtime, HeroId);
		const FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyId);
		Test.TestNotNull(FString::Printf(TEXT("%s retains Hero"), *Context), Hero);
		Test.TestNotNull(FString::Printf(TEXT("%s retains ally"), *Context), Ally);
		if (!Hero || !Ally)
		{
			return;
		}

		Test.TestEqual(FString::Printf(TEXT("%s Plain Burn"), *Context), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), Terrain == EGameXXKCardTerrain::Plain ? 2 * Repetitions : 0);
		Test.TestEqual(FString::Printf(TEXT("%s Cliff Vulnerability"), *Context), Status(Runtime, EnemyAId, EGameXXKCardStatus::Vulnerability), Terrain == EGameXXKCardTerrain::Cliff ? FMath::Min(5, 2 * Repetitions) : 0);
		Test.TestEqual(FString::Printf(TEXT("%s Cliff Mark"), *Context), Status(Runtime, EnemyAId, EGameXXKCardStatus::Mark), Terrain == EGameXXKCardTerrain::Cliff ? FMath::Min(5, Repetitions) : 0);
		Test.TestEqual(FString::Printf(TEXT("%s Forest Hero healing"), *Context), Hero->HP, 60 + (Terrain == EGameXXKCardTerrain::Forest ? 4 * Repetitions : 0));
		Test.TestEqual(FString::Printf(TEXT("%s Forest ally healing"), *Context), Ally->HP, 50 + (Terrain == EGameXXKCardTerrain::Forest ? 4 * Repetitions : 0));
		const bool bWater = Terrain == EGameXXKCardTerrain::WaterShore || Terrain == EGameXXKCardTerrain::Ferry;
		Test.TestEqual(FString::Printf(TEXT("%s water Hero Mana"), *Context), Hero->Mana, OwnerManaAfterCost + (bWater ? 3 * Repetitions : 0));
		Test.TestEqual(FString::Printf(TEXT("%s water ally Mana"), *Context), Ally->Mana, 20 + (bWater ? 3 * Repetitions : 0));
		const int32 ExpectedArmor = Terrain == EGameXXKCardTerrain::Village
			? 4 * Repetitions
			: Terrain == EGameXXKCardTerrain::Cave ? 8 * Repetitions : 0;
		Test.TestEqual(FString::Printf(TEXT("%s Hero Armor"), *Context), Hero->Armor, ExpectedArmor);
		Test.TestEqual(FString::Printf(TEXT("%s ally Armor"), *Context), Ally->Armor, ExpectedArmor);
		Test.TestEqual(FString::Printf(TEXT("%s Cave Block sources"), *Context), CountReactions(Runtime, EGameXXKCardStatus::Block), Terrain == EGameXXKCardTerrain::Cave ? 2 * Repetitions : 0);
		Test.TestEqual(FString::Printf(TEXT("%s final hand count"), *Context), Runtime.Deck.Hand.Num(), ExpectedHandCount);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationTargetMatrixTest,
	"GameXXK.Data.HeroCards.Formation.TargetMatrixKeepsOneEnemyAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationTargetMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	int32 PreviewCount = 0;
	int32 ResolveCount = 0;
	for (const FName CardId : FormationCardIds())
	{
		for (const EGameXXKCardTerrain Terrain : AllTerrains())
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!BuildRuntime(*this, Runtime, CardId, Terrain, 0, 61000 + PreviewCount))
			{
				continue;
			}
			const FString Context = FString::Printf(TEXT("%s terrain=%d"), *CardId.ToString(), static_cast<int32>(Terrain));
			FGameXXKCardPlayPreview Preview;
			FString Error;
			const bool bPreviewed = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Primary"), Preview, &Error);
			TestTrue(FString::Printf(TEXT("%s previews: %s"), *Context, *Error), bPreviewed);
			if (!bPreviewed)
			{
				continue;
			}
			++PreviewCount;
			TestTrue(FString::Printf(TEXT("%s requires one manual target"), *Context), Preview.TargetRequest.bRequiresManualSelection);
			TestEqual(FString::Printf(TEXT("%s remains SingleEnemy"), *Context), Preview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::SingleEnemy);
			FGameXXKCardPlayResult Result;
			if (Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, Context))
			{
				++ResolveCount;
				TestEqual(FString::Printf(TEXT("%s commits the selected anchor"), *Context), Result.TargetUnitIds.Num(), 1);
				if (Result.TargetUnitIds.Num() == 1)
				{
					TestEqual(FString::Printf(TEXT("%s keeps EnemyA as anchor"), *Context), Result.TargetUnitIds[0], EnemyAId);
				}
			}
		}
	}
	TestEqual(TEXT("all 4x7 Formation cards preview"), PreviewCount, 4 * 7);
	TestEqual(TEXT("all 4x7 Formation cards resolve"), ResolveCount, 4 * 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationGuanShiTest,
	"GameXXK.Data.HeroCards.Formation.GuanShiResolvesBaseAndOneLiveTerrainBenefit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationGuanShiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	int32 CaseIndex = 0;
	for (const EGameXXKCardTerrain Terrain : AllTerrains())
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.GuanShiLuoZi"), Terrain, 0, 61100 + CaseIndex++))
		{
			continue;
		}
		FGameXXKCardPlayResult Result;
		const FString Context = FString::Printf(TEXT("GuanShi terrain=%d"), static_cast<int32>(Terrain));
		if (!Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, Context))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s has one 80%% attack packet"), *Context), Result.DamageResults.Num(), 1);
		if (Result.DamageResults.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("%s requests 8 damage"), *Context), Result.DamageResults[0].RequestedDamage, 8);
		}
		AssertBenefit(*this, Runtime, Terrain, 1, 27, Terrain == EGameXXKCardTerrain::Village ? 2 : 1, Context);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationYiZhenTest,
	"GameXXK.Data.HeroCards.Formation.YiZhenRepeatsOnlyAfterRealChangeFlag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationYiZhenTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	int32 CaseIndex = 0;
	for (const bool bChanged : {false, true})
	{
		for (const EGameXXKCardTerrain Terrain : AllTerrains())
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.YiZhenHuiXiang"), Terrain, 0, 61200 + CaseIndex++))
			{
				continue;
			}
			Runtime.bTerrainChangedThisRound = bChanged;
			FGameXXKCardPlayResult Result;
			const FString Context = FString::Printf(TEXT("YiZhen changed=%d terrain=%d"), bChanged, static_cast<int32>(Terrain));
			if (!Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, Context))
			{
				continue;
			}
			const int32 Repetitions = bChanged ? 3 : 2;
			AssertBenefit(*this, Runtime, Terrain, Repetitions, 27, Terrain == EGameXXKCardTerrain::Village ? Repetitions : 0, Context);
			TestEqual(FString::Printf(TEXT("%s energy reward follows the real-change branch"), *Context), Runtime.Deck.SharedEnergy, bChanged ? 10 : 9);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationListenerTest,
	"GameXXK.Data.HeroCards.Formation.LianYingUsesThreeLiveActiveCardTerrainsAndRetargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationListenerTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.LianYingBuShi"), EGameXXKCardTerrain::Plain, 3, 61300))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, TEXT("LianYing registration")))
	{
		return true;
	}
	TestEqual(TEXT("LianYing does not consume itself"), CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::AfterEachActiveCard), 1);
	const FGameXXKCardBattleModifierRuntime* Listener = Runtime.Modifiers.FindByPredicate([](const FGameXXKCardBattleModifierRuntime& Modifier)
	{
		return Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::AfterEachActiveCard;
	});
	TestNotNull(TEXT("LianYing registers one listener"), Listener);
	if (Listener)
	{
		TestEqual(TEXT("LianYing listener starts with three uses"), Listener->Definition.RemainingTriggers, 3);
		TestEqual(TEXT("LianYing retains the enemy anchor"), Listener->OriginalSelectedTargetUnitId, EnemyAId);
	}

	Runtime.Terrain = EGameXXKCardTerrain::Plain;
	if (Resolve(*this, Runtime, TEXT("Active.0"), NAME_None, Result, TEXT("first listener trigger")))
	{
		TestEqual(TEXT("first live terrain adds Burn2 to the anchor"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 2);
	}
	FGameXXKCardCombatUnit* EnemyA = FindUnit(Runtime, EnemyAId);
	if (EnemyA)
	{
		EnemyA->HP = 0;
		EnemyA->bLiving = false;
	}
	Runtime.Terrain = EGameXXKCardTerrain::Cliff;
	if (Resolve(*this, Runtime, TEXT("Active.1"), NAME_None, Result, TEXT("second listener trigger")))
	{
		TestEqual(TEXT("dead anchor retargets to stable EnemyB Vulnerability"), Status(Runtime, EnemyBId, EGameXXKCardStatus::Vulnerability), 2);
		TestEqual(TEXT("dead anchor retargets to stable EnemyB Mark"), Status(Runtime, EnemyBId, EGameXXKCardStatus::Mark), 1);
	}
	Runtime.Terrain = EGameXXKCardTerrain::Forest;
	if (Resolve(*this, Runtime, TEXT("Active.2"), NAME_None, Result, TEXT("third listener trigger")))
	{
		TestEqual(TEXT("third live terrain heals Hero4"), FindUnit(Runtime, HeroId)->HP, 64);
		TestEqual(TEXT("third live terrain heals ally4"), FindUnit(Runtime, AllyId)->HP, 54);
	}
	TestEqual(TEXT("LianYing listener expires after exactly three later active cards"), CountModifiers(Runtime, EGameXXKCardBattleModifierTrigger::AfterEachActiveCard), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationListenerTerrainMatrixTest,
	"GameXXK.Data.HeroCards.Formation.LianYingReadsEveryLiveTerrainAndIgnoresAutomaticReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationListenerTerrainMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	int32 CaseIndex = 0;
	for (const EGameXXKCardTerrain Terrain : AllTerrains())
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.LianYingBuShi"), EGameXXKCardTerrain::Plain, 1, 61350 + CaseIndex++))
		{
			continue;
		}
		FGameXXKCardPlayResult Result;
		const FString Context = FString::Printf(TEXT("LianYing live terrain=%d"), static_cast<int32>(Terrain));
		if (!Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, Context + TEXT(" registration")))
		{
			continue;
		}

		Runtime.Terrain = Terrain;
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
		Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
		FGameXXKResolvedCardSnapshot& Replay = Runtime.AutomaticResolutionQueue.PendingCards.AddDefaulted_GetRef();
		Replay.CardId = TEXT("Hero.Generic.NingShenTuNa");
		Replay.Quality = EGameXXKCardQuality::Common;
		Replay.OwnerUnitId = HeroId;
		TArray<FGameXXKCardPlayResult> ReplayResults;
		FString Error;
		TestTrue(
			FString::Printf(TEXT("%s automatic replay resolves: %s"), *Context, *Error),
			GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, ReplayResults, &Error));
		const FGameXXKCardBattleModifierRuntime* Listener = Runtime.Modifiers.FindByPredicate([](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::AfterEachActiveCard;
		});
		TestNotNull(FString::Printf(TEXT("%s retains its listener after automatic replay"), *Context), Listener);
		if (Listener)
		{
			TestEqual(FString::Printf(TEXT("%s automatic replay consumes no listener use"), *Context), Listener->Definition.RemainingTriggers, 3);
		}

		if (!Resolve(*this, Runtime, TEXT("Active.0"), NAME_None, Result, Context + TEXT(" active trigger")))
		{
			continue;
		}
		AssertBenefit(
			*this,
			Runtime,
			Terrain,
			1,
			50,
			Terrain == EGameXXKCardTerrain::Village ? 1 : 0,
			Context);
		Listener = Runtime.Modifiers.FindByPredicate([](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::AfterEachActiveCard;
		});
		TestNotNull(FString::Printf(TEXT("%s retains two later uses"), *Context), Listener);
		if (Listener)
		{
			TestEqual(FString::Printf(TEXT("%s consumes exactly one active-play use"), *Context), Listener->Definition.RemainingTriggers, 2);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationLiuHeTest,
	"GameXXK.Data.HeroCards.Formation.LiuHeRunsSixFixedBenefitsThenCurrentWithoutSwitching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationLiuHeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.LiuHeGuiYi"), EGameXXKCardTerrain::Ferry, 0, 61400))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Primary"), EnemyAId, Result, TEXT("LiuHe")))
	{
		return true;
	}
	TestEqual(TEXT("LiuHe fixed Plain adds Burn2"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 2);
	TestEqual(TEXT("LiuHe fixed Cliff adds Vulnerability2"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Vulnerability), 2);
	TestEqual(TEXT("LiuHe fixed Cliff adds Mark1"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Mark), 1);
	TestEqual(TEXT("LiuHe fixed Forest heals Hero4"), FindUnit(Runtime, HeroId)->HP, 64);
	TestEqual(TEXT("LiuHe fixed Forest heals ally4"), FindUnit(Runtime, AllyId)->HP, 54);
	TestEqual(TEXT("LiuHe fixed Water plus current Ferry grants Hero Mana6 after cost"), FindUnit(Runtime, HeroId)->Mana, 30);
	TestEqual(TEXT("LiuHe fixed Water plus current Ferry grants ally Mana6"), FindUnit(Runtime, AllyId)->Mana, 26);
	TestEqual(TEXT("LiuHe Village plus Cave grants Hero Armor12"), FindUnit(Runtime, HeroId)->Armor, 12);
	TestEqual(TEXT("LiuHe Village plus Cave grants ally Armor12"), FindUnit(Runtime, AllyId)->Armor, 12);
	TestEqual(TEXT("LiuHe Cave registers one Block for each ally"), CountReactions(Runtime, EGameXXKCardStatus::Block), 2);
	TestEqual(TEXT("LiuHe Village draws one card"), Runtime.Deck.Hand.Num(), 1);
	TestEqual(TEXT("LiuHe keeps the current terrain"), Runtime.Terrain, EGameXXKCardTerrain::Ferry);
	TestFalse(TEXT("LiuHe never claims a terrain change"), Runtime.bTerrainChangedThisRound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroFormationTerrainChangeTest,
	"GameXXK.Data.HeroCards.Formation.RealTerrainChangeMarksOnlyCurrentRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroFormationTerrainChangeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroFormationRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, TEXT("Hero.Formation.GuanShiLuoZi"), EGameXXKCardTerrain::Plain, 0, 61500))
	{
		return false;
	}
	FString Error;
	TestFalse(
		TEXT("the current terrain is not a real change"),
		GameXXKCardRules::NotifyTerrainChanged(Runtime, EGameXXKCardTerrain::Plain, &Error));
	TestEqual(TEXT("same-terrain rejection preserves Plain"), Runtime.Terrain, EGameXXKCardTerrain::Plain);
	TestFalse(TEXT("same-terrain rejection does not set the flag"), Runtime.bTerrainChangedThisRound);
	Error.Reset();
	TestTrue(
		FString::Printf(TEXT("a concrete different terrain is accepted: %s"), *Error),
		GameXXKCardRules::NotifyTerrainChanged(Runtime, EGameXXKCardTerrain::Cliff, &Error));
	TestEqual(TEXT("the new terrain commits"), Runtime.Terrain, EGameXXKCardTerrain::Cliff);
	TestTrue(TEXT("a real change sets the current-round flag"), Runtime.bTerrainChangedThisRound);

	TArray<FGameXXKCardDamageResult> DamageResults;
	TestTrue(
		FString::Printf(TEXT("the player phase can end after a terrain change: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error));
	DamageResults.Reset();
	TestTrue(
		FString::Printf(TEXT("the next player round can begin: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error));
	TestFalse(TEXT("the next player round clears the terrain-change flag"), Runtime.bTerrainChangedThisRound);
	TestEqual(TEXT("the selected terrain persists across rounds"), Runtime.Terrain, EGameXXKCardTerrain::Cliff);
	return true;
}

#endif
