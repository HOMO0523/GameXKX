#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerTaskStateTest
{
	const FName SorcererId(TEXT("Partner.Sorcerer"));
	const FName EnemyAId(TEXT("Enemy.A"));
	const FName EnemyBId(TEXT("Enemy.B"));

	const TArray<FName>& CarriedFive()
	{
		static const TArray<FName> CardIds = {
			TEXT("Profession.Sorcerer.LingHuoFu"),
			TEXT("Profession.Sorcerer.LiHuoYin"),
			TEXT("Profession.Sorcerer.SheLingHuo"),
			TEXT("Profession.Sorcerer.ChiXiaoFenXing"),
			TEXT("Profession.Sorcerer.YanMuHuTi")};
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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 1000 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 40 : 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Sorcerer.Card.%d"), Ordinal));
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = SorcererId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Sorcerer.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CarriedFive().Num(); ++Index)
		{
			Cards.Add(MakeCard(CarriedFive()[Index], Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			59101,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer task fixture failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.SharedEnergy = 20;
		return true;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		const int32 PaidMana,
		const int32 Position,
		const EGameXXKSorcererCardFamily PreviousFamily)
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = SorcererId;
		Snapshot.OriginalTargetUnitIds = {EnemyAId, EnemyBId};
		Snapshot.PaidManaCost = PaidMana;
		Snapshot.SorcererSequencePosition = Position;
		Snapshot.PreviousSorcererFamily = PreviousFamily;
		Snapshot.SorcererTaskBranch = EGameXXKSorcererTaskBranch::Normal;
		return Snapshot;
	}

	void InstallValidActiveTask(FGameXXKCardBattleRuntime& Runtime)
	{
		FGameXXKSorcererPartnerTaskRuntime& Task = Runtime.SorcererPartnerTasks.AddDefaulted_GetRef();
		Task.bActive = true;
		Task.OwnerUnitId = SorcererId;
		Task.LockedCardIds = CarriedFive();
		Task.CompletedCardIds = {
			TEXT("Profession.Sorcerer.LingHuoFu"),
			TEXT("Profession.Sorcerer.LiHuoYin")};
		Task.FirstPlayOrder = {
			MakeSnapshot(TEXT("Profession.Sorcerer.LingHuoFu"), 2, 1, EGameXXKSorcererCardFamily::None),
			MakeSnapshot(TEXT("Profession.Sorcerer.LiHuoYin"), 1, 2, EGameXXKSorcererCardFamily::Core)};
		Task.StarterReward = EGameXXKSorcererRewardRule::CoreSearch;
		Task.LockedBranch = EGameXXKSorcererTaskBranch::Normal;
		Task.AutoHandedUniversalCardIds = {TEXT("Profession.Sorcerer.YanMuHuTi")};
	}

	bool Validate(FAutomationTestBase& Test, const FGameXXKCardBattleRuntime& Runtime, const bool bExpected, const TCHAR* Label)
	{
		FString Error;
		const bool bActual = GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error);
		Test.TestEqual(FString::Printf(TEXT("%s validation result: %s"), Label, *Error), bActual, bExpected);
		return bActual == bExpected;
	}

	bool RoundTrip(
		FAutomationTestBase& Test,
		const FGameXXKCardBattleRuntime& Source,
		FGameXXKCardBattleRuntime& OutLoaded)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		Writer.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Writer, const_cast<FGameXXKCardBattleRuntime*>(&Source), nullptr);
		Writer.Close();
		if (!Test.TestTrue(TEXT("serialized Sorcerer task has bytes"), !Bytes.IsEmpty()))
		{
			return false;
		}

		FMemoryReader Reader(Bytes, true);
		Reader.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Reader, &OutLoaded, nullptr);
		Reader.Close();
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerTaskStateRoundTripTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskState.ValidStateRoundTrips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerTaskStateRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskStateTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime))
	{
		return false;
	}
	InstallValidActiveTask(Runtime);
	Validate(*this, Runtime, true, TEXT("valid two-of-five task"));

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTrip(*this, Runtime, Loaded))
	{
		return false;
	}
	TestEqual(TEXT("one owner-scoped task survives"), Loaded.SorcererPartnerTasks.Num(), 1);
	if (Loaded.SorcererPartnerTasks.Num() != 1)
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime& Task = Loaded.SorcererPartnerTasks[0];
	TestTrue(TEXT("active flag survives"), Task.bActive);
	TestEqual(TEXT("owner survives"), Task.OwnerUnitId, SorcererId);
	TestEqual(TEXT("five locked IDs survive"), Task.LockedCardIds, CarriedFive());
	TestEqual(TEXT("two completed IDs survive"), Task.CompletedCardIds.Num(), 2);
	TestEqual(TEXT("two first-play snapshots survive"), Task.FirstPlayOrder.Num(), 2);
	TestEqual(TEXT("starter reward survives"), Task.StarterReward, EGameXXKSorcererRewardRule::CoreSearch);
	TestEqual(TEXT("normal branch survives"), Task.LockedBranch, EGameXXKSorcererTaskBranch::Normal);
	TestEqual(TEXT("universal auto-hand history survives"), Task.AutoHandedUniversalCardIds, TArray<FName>{TEXT("Profession.Sorcerer.YanMuHuTi")});
	if (Task.FirstPlayOrder.Num() == 2)
	{
		TestEqual(TEXT("actual paid Mana survives"), Task.FirstPlayOrder[1].PaidManaCost, 1);
		TestEqual(TEXT("sequence position survives"), Task.FirstPlayOrder[1].SorcererSequencePosition, 2);
		TestEqual(TEXT("previous family survives"), Task.FirstPlayOrder[1].PreviousSorcererFamily, EGameXXKSorcererCardFamily::Core);
		TestEqual(TEXT("snapshot branch survives"), Task.FirstPlayOrder[1].SorcererTaskBranch, EGameXXKSorcererTaskBranch::Normal);
	}
	Validate(*this, Loaded, true, TEXT("loaded two-of-five task"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerTaskStateRejectsMalformedTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskState.RejectsMalformedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerTaskStateRejectsMalformedTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskStateTest;
	FGameXXKCardBattleRuntime Baseline;
	if (!BuildRuntime(*this, Baseline))
	{
		return false;
	}
	InstallValidActiveTask(Baseline);
	if (!Validate(*this, Baseline, true, TEXT("malformed-test baseline")))
	{
		return true;
	}

	FGameXXKCardBattleRuntime Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].LockedCardIds[1] = Mutated.SorcererPartnerTasks[0].LockedCardIds[0];
	Validate(*this, Mutated, false, TEXT("duplicate locked CardId"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].CompletedCardIds.Add(TEXT("Profession.Sorcerer.JuLing"));
	Validate(*this, Mutated, false, TEXT("completed ID outside lock"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder[1].SorcererSequencePosition = 4;
	Validate(*this, Mutated, false, TEXT("non-contiguous sequence position"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder[1].PreviousSorcererFamily = EGameXXKSorcererCardFamily::Ice;
	Validate(*this, Mutated, false, TEXT("wrong previous family"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].LockedBranch = EGameXXKSorcererTaskBranch::Fire;
	Validate(*this, Mutated, false, TEXT("branch disagrees with non-universal starter"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder[1].OwnerUnitId = EnemyAId;
	Validate(*this, Mutated, false, TEXT("snapshot owner mismatch"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].AutoHandedUniversalCardIds.Add(TEXT("Profession.Sorcerer.YanMuHuTi"));
	Validate(*this, Mutated, false, TEXT("duplicate universal auto-hand history"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].bActive = false;
	Validate(*this, Mutated, false, TEXT("inactive task retains progress"));

	Mutated = Baseline;
	Mutated.SorcererPartnerTasks[0].CompletedCardIds = Mutated.SorcererPartnerTasks[0].LockedCardIds;
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder.Add(MakeSnapshot(TEXT("Profession.Sorcerer.SheLingHuo"), 0, 3, EGameXXKSorcererCardFamily::Fire));
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder.Add(MakeSnapshot(TEXT("Profession.Sorcerer.ChiXiaoFenXing"), 1, 4, EGameXXKSorcererCardFamily::Ice));
	Mutated.SorcererPartnerTasks[0].FirstPlayOrder.Add(MakeSnapshot(TEXT("Profession.Sorcerer.YanMuHuTi"), 5, 5, EGameXXKSorcererCardFamily::Lightning));
	Validate(*this, Mutated, false, TEXT("completed task exists without its automatic queue"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerTaskInactiveHistoryTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskState.InactiveOwnerKeepsOnlyBattleHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerTaskInactiveHistoryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskStateTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime))
	{
		return false;
	}
	FGameXXKSorcererPartnerTaskRuntime& Task = Runtime.SorcererPartnerTasks.AddDefaulted_GetRef();
	Task.OwnerUnitId = SorcererId;
	Task.AutoHandedUniversalCardIds = {TEXT("Profession.Sorcerer.YanMuHuTi")};
	Validate(*this, Runtime, true, TEXT("inactive owner with only per-battle auto-hand history"));
	return true;
}

#endif
