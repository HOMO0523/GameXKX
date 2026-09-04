#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKApprovedTerrainPayloadTest
{
	const FName SourceId(TEXT("Terrain.Source"));
	const FName AllyAId(TEXT("Terrain.AllyA"));
	const FName AllyBId(TEXT("Terrain.AllyB"));
	const FName EnemyAId(TEXT("Terrain.EnemyA"));
	const FName EnemyBId(TEXT("Terrain.EnemyB"));
	const FName EnemyCId(TEXT("Terrain.EnemyC"));

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
		Unit.MaxHP = Side == EGameXXKCardTargetSide::Party ? 200 : 1000;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 100 : 1000;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 10 : 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 1 : 0;
		Unit.Attack = 10;
		Unit.Defense = UnitId == SourceId ? 301 : 20;
		Unit.Speed = 10;
		Unit.StableSortOrder = StableSortOrder;
		Unit.bLiving = true;
		return Unit;
	}

	FGameXXKCardBattleRuntime MakeRuntime()
	{
		FGameXXKCardBattleRuntime Runtime;
		Runtime.RoundNumber = 1;
		Runtime.TeamMaxLevelSnapshot = 100;
		Runtime.NextReactionOrdinal = 1;
		Runtime.Units = {
			MakeUnit(SourceId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1),
			MakeUnit(AllyAId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
			MakeUnit(AllyBId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11),
			MakeUnit(EnemyCId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 12)};
		FGameXXKCardInstance DrawCard;
		DrawCard.InstanceId = TEXT("Terrain.Draw.1");
		DrawCard.CardId = TEXT("Hero.Generic.QingFengYiShi");
		DrawCard.CurrentQuality = EGameXXKCardQuality::Common;
		DrawCard.OwnerUnitId = SourceId;
		DrawCard.SourceEntryId = TEXT("Terrain.Draw.Source");
		DrawCard.AcquisitionOrdinal = 1;
		Runtime.Deck.DrawPile.Add(DrawCard);
		Runtime.Deck.ActiveInstanceIds.Add(DrawCard.InstanceId);
		Runtime.Deck.HandLimit = 5;
		Runtime.Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::None;
		return Runtime;
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

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardTerrain Terrain,
		FGameXXKCardPlayResult& OutResult)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveTerrainBenefitForTest(
			Runtime,
			SourceId,
			Terrain,
			1,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("terrain %d resolves: %s"), static_cast<int32>(Terrain), *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedTerrainPayloadTest,
	"GameXXK.Data.Terrain.ApprovedPayloads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedTerrainPayloadTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKApprovedTerrainPayloadTest;

	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, EGameXXKCardTerrain::Plain, Result))
		{
			for (const FName EnemyId : {EnemyAId, EnemyBId, EnemyCId})
			{
				TestEqual(TEXT("Plain applies level-scaled Burn to every enemy"), Status(Runtime, EnemyId, EGameXXKCardStatus::Burn), 10);
			}
			TestEqual(TEXT("Plain records every Burn application"), Result.StatusChanges.Num(), 3);
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, EGameXXKCardTerrain::Cliff, Result))
		{
			for (const FName EnemyId : {EnemyAId, EnemyBId, EnemyCId})
			{
				TestEqual(TEXT("Cliff applies Vulnerability to every enemy"), Status(Runtime, EnemyId, EGameXXKCardStatus::Vulnerability), 2);
				TestEqual(TEXT("Cliff applies Mark to every enemy"), Status(Runtime, EnemyId, EGameXXKCardStatus::Mark), 1);
			}
			TestEqual(TEXT("Cliff records both statuses on all enemies"), Result.StatusChanges.Num(), 6);
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, EGameXXKCardTerrain::Forest, Result))
		{
			TestEqual(TEXT("Forest records one healing packet per ally"), Result.HealingResults.Num(), 3);
			for (const FGameXXKCardHealingResult& Healing : Result.HealingResults)
			{
				TestEqual(TEXT("Forest uses healing coefficient ten at level one hundred"), Healing.RequestedHealing, 50);
			}
		}
	}

	for (const EGameXXKCardTerrain Terrain : {EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry})
	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Terrain, Result))
		{
			for (const FName AllyId : {SourceId, AllyAId, AllyBId})
			{
				const FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyId);
				TestEqual(TEXT("Water terrain restores three Mana to every ally"), Ally ? Ally->Mana : INDEX_NONE, 4);
			}
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, EGameXXKCardTerrain::Village, Result))
		{
			TestEqual(TEXT("Village draws one card"), Runtime.Deck.Hand.Num(), 1);
			for (const FName AllyId : {SourceId, AllyAId, AllyBId})
			{
				const FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyId);
				TestEqual(TEXT("Village grants twenty percent of source Defense"), Ally ? Ally->Armor : INDEX_NONE, 61);
			}
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime = MakeRuntime();
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, EGameXXKCardTerrain::Cave, Result))
		{
			for (const FName AllyId : {SourceId, AllyAId, AllyBId})
			{
				const FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyId);
				TestEqual(TEXT("Cave grants forty percent of source Defense"), Ally ? Ally->Armor : INDEX_NONE, 121);
			}
			TestEqual(TEXT("Cave registers one Block for every ally"), Runtime.Reactions.Num(), 3);
		}
	}

	return true;
}

#endif
