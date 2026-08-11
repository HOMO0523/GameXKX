#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcHealerFormationRuntimeTest
{
	const FName AllyUnitId(TEXT("Ally.Support"));
	const FName EnemyUnitId(TEXT("Enemy.Target"));

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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 2000 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = 20;
		Unit.MaxMana = 20;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName OwnerUnitId,
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("TaskNpc.HealerFormation.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const FName OwnerUnitId,
		const TCHAR* ActiveInstanceId,
		const TCHAR* ActiveCardId,
		const EGameXXKCardTerrain Terrain,
		const int32 Seed)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(OwnerUnitId, ActiveInstanceId, ActiveCardId, 0),
			MakeCard(OwnerUnitId, TEXT("Filler.1"), TEXT("Hero.Generic.QingFengYiShi"), 1),
			MakeCard(OwnerUnitId, TEXT("Filler.2"), TEXT("Hero.Generic.HeYuZhan"), 2),
			MakeCard(OwnerUnitId, TEXT("Filler.3"), TEXT("Hero.Generic.SuiYanJi"), 3),
			MakeCard(OwnerUnitId, TEXT("Filler.4"), TEXT("Hero.Generic.FengShenBu"), 4),
			MakeCard(OwnerUnitId, TEXT("Filler.5"), TEXT("Hero.Generic.GuiYuanShu"), 5),
			MakeCard(OwnerUnitId, TEXT("Filler.6"), TEXT("Hero.Generic.HengJianShouShi"), 6)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, Terrain, Seed, &Error))
		{
			Test.AddError(FString::Printf(TEXT("task-NPC healer/formation runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(Card.InstanceId == FName(ActiveInstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic task-NPC healer/formation fixture is invalid: %s"), *Error));
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

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcMedicineTargetSideTest,
	"GameXXK.Data.TaskNpcCards.Runtime.HealerFormation.MedicineConsumesOnceAndCleansesFriendlyDotsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcMedicineTargetSideTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcHealerFormationRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.ZhouGuangZu"));

	FGameXXKCardBattleRuntime FriendlyRuntime;
	if (!BuildRuntime(*this, FriendlyRuntime, OwnerUnitId, TEXT("YiCao.Friendly"), TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), EGameXXKCardTerrain::Plain, 58201)) return false;
	FindUnit(FriendlyRuntime, AllyUnitId)->HP = 20;
	GameXXKCardRules::AddCombatStatus(*FindUnit(FriendlyRuntime, AllyUnitId), EGameXXKCardStatus::Bleed, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(FriendlyRuntime, AllyUnitId), EGameXXKCardStatus::Poison, 4);
	GameXXKCardRules::AddCombatStatus(*FindUnit(FriendlyRuntime, AllyUnitId), EGameXXKCardStatus::Burn, 5);
	GameXXKCardRules::AddCombatStatus(*FindUnit(FriendlyRuntime, AllyUnitId), EGameXXKCardStatus::DamageOverTime, 7);
	FGameXXKCardPlayResult FriendlyResult;
	if (Resolve(*this, FriendlyRuntime, TEXT("YiCao.Friendly"), AllyUnitId, FriendlyResult, TEXT("异草辨识友方分支")))
	{
		TestEqual(TEXT("friendly branch heals base6 plus Medicine6"), FindUnit(FriendlyRuntime, AllyUnitId)->HP, 32);
		TestEqual(TEXT("friendly branch clears all Bleed"), Status(FriendlyRuntime, AllyUnitId, EGameXXKCardStatus::Bleed), 0);
		TestEqual(TEXT("friendly branch clears all Poison"), Status(FriendlyRuntime, AllyUnitId, EGameXXKCardStatus::Poison), 0);
		TestEqual(TEXT("friendly branch clears all Burn"), Status(FriendlyRuntime, AllyUnitId, EGameXXKCardStatus::Burn), 0);
		TestEqual(TEXT("friendly cleanse does not erase Rot"), Status(FriendlyRuntime, AllyUnitId, EGameXXKCardStatus::DamageOverTime), 7);
		TestEqual(TEXT("Medicine snapshot is consumed once"), Status(FriendlyRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 0);
		TestEqual(TEXT("gaining Medicine6 grants Momentum1"), Status(FriendlyRuntime, OwnerUnitId, EGameXXKCardStatus::Momentum), 1);
	}

	FGameXXKCardBattleRuntime EnemyRuntime;
	if (!BuildRuntime(*this, EnemyRuntime, OwnerUnitId, TEXT("YiCao.Enemy"), TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), EGameXXKCardTerrain::Plain, 58202)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(EnemyRuntime, EnemyUnitId), EGameXXKCardStatus::Bleed, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(EnemyRuntime, EnemyUnitId), EGameXXKCardStatus::Poison, 4);
	GameXXKCardRules::AddCombatStatus(*FindUnit(EnemyRuntime, EnemyUnitId), EGameXXKCardStatus::Burn, 5);
	FGameXXKCardPlayResult EnemyResult;
	if (Resolve(*this, EnemyRuntime, TEXT("YiCao.Enemy"), EnemyUnitId, EnemyResult, TEXT("异草辨识敌方分支")))
	{
		TestEqual(TEXT("enemy branch loses base6 plus Medicine6 health"), FindUnit(EnemyRuntime, EnemyUnitId)->HP, 1988);
		TestEqual(TEXT("enemy Bleed is not cleansed"), Status(EnemyRuntime, EnemyUnitId, EGameXXKCardStatus::Bleed), 3);
		TestEqual(TEXT("enemy Poison is not cleansed"), Status(EnemyRuntime, EnemyUnitId, EGameXXKCardStatus::Poison), 4);
		TestEqual(TEXT("enemy Burn is not cleansed"), Status(EnemyRuntime, EnemyUnitId, EGameXXKCardStatus::Burn), 5);
		TestEqual(TEXT("enemy branch also consumes the same Medicine snapshot"), Status(EnemyRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcGroupMedicineTest,
	"GameXXK.Data.TaskNpcCards.Runtime.HealerFormation.GroupMedicineUsesOneSnapshotAndHealthCostIsNonlethal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcGroupMedicineTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcHealerFormationRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.ZhouGuangZu"));
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, OwnerUnitId, TEXT("HuangShan"), TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), EGameXXKCardTerrain::Plain, 58203)) return false;
	FindUnit(Runtime, OwnerUnitId)->HP = 2;
	FindUnit(Runtime, AllyUnitId)->HP = 1;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HuangShan"), NAME_None, Result, TEXT("黄山敷治"))) return true;

	TestEqual(TEXT("owner loses one nonlethally then heals 6+6"), FindUnit(Runtime, OwnerUnitId)->HP, 13);
	TestEqual(TEXT("one-HP ally cannot be killed and still heals 6+6"), FindUnit(Runtime, AllyUnitId)->HP, 13);
	TestEqual(TEXT("group heal consumes Medicine only once"), Status(Runtime, OwnerUnitId, EGameXXKCardStatus::Medicine), 0);
	TestEqual(TEXT("one Medicine6 grant yields one Momentum"), Status(Runtime, OwnerUnitId, EGameXXKCardStatus::Momentum), 1);
	TestEqual(TEXT("only the owner produces an actual nonlethal-loss packet"), Result.DamageResults.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcToxicExplosionTest,
	"GameXXK.Data.TaskNpcCards.Runtime.HealerFormation.ToxicExplosionResolvesThreeDotsAndExcludesRot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcToxicExplosionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcHealerFormationRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.QiongMeiEr"));
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, OwnerUnitId, TEXT("GuWu"), TEXT("Npc.QiongMeiEr.GuWuMiZong"), EGameXXKCardTerrain::Plain, 58204)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Burn, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::DamageOverTime, 7);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("GuWu"), EnemyUnitId, Result, TEXT("蛊雾迷踪"))) return true;

	TestEqual(TEXT("toxic explosion emits Bleed, Poison, and Burn only"), Result.DamageResults.Num(), 3);
	TestEqual(TEXT("Bleed4 resolves then loses one layer"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 3);
	TestEqual(TEXT("Poison6 resolves then loses one layer"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Poison), 5);
	TestEqual(TEXT("existing Burn3 resolves then loses one layer"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Burn), 2);
	TestEqual(TEXT("Rot is not exploded or consumed"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::DamageOverTime), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcTerrainTargetingMatrixTest,
	"GameXXK.Data.TaskNpcCards.Runtime.HealerFormation.FormationCardsKeepOneEnemyTargetAcrossEveryTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcTerrainTargetingMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcHealerFormationRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.ZhouGuangZu"));
	const TArray<EGameXXKCardTerrain> Terrains = {
		EGameXXKCardTerrain::Plain,
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest,
		EGameXXKCardTerrain::WaterShore,
		EGameXXKCardTerrain::Ferry,
		EGameXXKCardTerrain::Village,
		EGameXXKCardTerrain::Cave};
	struct FCardCase
	{
		const TCHAR* InstanceId;
		const TCHAR* CardId;
	};
	const TArray<FCardCase> Cards = {
		{TEXT("DiZhi"), TEXT("Npc.ZhouGuangZu.DiZhiMoTu")},
		{TEXT("YanFen"), TEXT("Npc.ZhouGuangZu.YanFenFengMai")}};

	int32 CaseOrdinal = 0;
	for (const FCardCase& Card : Cards)
	{
		for (const EGameXXKCardTerrain Terrain : Terrains)
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!BuildRuntime(*this, Runtime, OwnerUnitId, Card.InstanceId, Card.CardId, Terrain, 58300 + CaseOrdinal++))
			{
				continue;
			}
			FGameXXKCardPlayResult Result;
			const FString Context = FString::Printf(TEXT("%s at %s"), Card.CardId, *UEnum::GetValueAsString(Terrain));
			if (Resolve(*this, Runtime, FName(Card.InstanceId), EnemyUnitId, Result, *Context))
			{
				TestEqual(*FString::Printf(TEXT("%s keeps the current terrain"), *Context), Runtime.Terrain, Terrain);
				TestEqual(*FString::Printf(TEXT("%s resolves the selected enemy without a target rollback"), *Context), Runtime.LastActiveCard.OriginalTargetUnitIds.Num(), 1);
				if (Runtime.LastActiveCard.OriginalTargetUnitIds.Num() == 1)
				{
					TestEqual(*FString::Printf(TEXT("%s keeps the selected enemy identity"), *Context), Runtime.LastActiveCard.OriginalTargetUnitIds[0], EnemyUnitId);
				}
			}
		}
	}
	return true;
}

#endif
