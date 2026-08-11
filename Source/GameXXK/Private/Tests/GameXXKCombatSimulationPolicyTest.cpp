#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCombatSimulationRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCombatSimulationPolicyTest
{
	const FName EnemyUnitId(TEXT("Policy.Enemy"));

	FName OwnerIdForRole(const EGameXXKCharacterRole Role)
	{
		return FName(*FString::Printf(TEXT("Policy.Owner.%d"), static_cast<int32>(Role)));
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
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const FName OwnerUnitId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(
			TEXT("Policy.Source.%s.%d"),
			CardId,
			AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildState(
		FAutomationTestBase& Test,
		const EGameXXKCharacterRole Role,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandInstanceIds,
		const EGameXXKCardTerrain Terrain,
		const int32 SharedEnergy,
		const int32 OwnerHealth,
		const int32 OwnerMana,
		FGameXXKRuntimeState& OutState)
	{
		const FName OwnerUnitId = OwnerIdForRole(Role);
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, Role, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		Units[0].HP = FMath::Clamp(OwnerHealth, 1, Units[0].MaxHP);
		Units[0].Mana = FMath::Clamp(OwnerMana, 0, Units[0].MaxMana);

		FGameXXKCardBattleRuntime Runtime;
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			Cards,
			Units,
			Terrain,
			73101 + static_cast<int32>(Role),
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("policy fixture initializes: %s"), *Error));
			return false;
		}

		Runtime.Deck.Hand.Reset();
		Runtime.Deck.DrawPile.Reset();
		Runtime.Deck.DiscardPile.Reset();
		Runtime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(HandInstanceIds.Contains(Card.InstanceId)
				? Runtime.Deck.Hand
				: Runtime.Deck.DrawPile).Add(Card);
		}
		Runtime.Deck.SharedEnergy = SharedEnergy;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("policy fixture validates: %s"), *Error));
			return false;
		}

		OutState = FGameXXKRuntimeState();
		OutState.Screen = EGameXXKScreen::Battle;
		OutState.CardRun.bHasActiveCardBattle = true;
		OutState.CardRun.ActiveBattle = MoveTemp(Runtime);
		return true;
	}

	bool ExpectStableDecision(
		FAutomationTestBase& Test,
		const FString& Label,
		const FGameXXKRuntimeState& State,
		const FName ExpectedCardInstanceId,
		const FName ExpectedTargetUnitId = NAME_None)
	{
		FGameXXKCardPlayPreview ExpectedPreview;
		FString PreviewError;
		const bool bExpectedPreviewBuilt = FGameXXKCardBattleAdapter::BuildCardPlayPreview(
			State,
			ExpectedCardInstanceId,
			ExpectedPreview,
			&PreviewError);
		Test.TestTrue(FString::Printf(TEXT("%s expected setup previews: %s"), *Label, *PreviewError), bExpectedPreviewBuilt);
		Test.TestTrue(Label + TEXT(" expected setup is payable"), bExpectedPreviewBuilt && ExpectedPreview.bCanPlay);
		FGameXXKRuntimeState ResolvedState = State;
		FGameXXKCardPlayResult ExpectedResult;
		FString ResolveError;
		const bool bExpectedResolves = FGameXXKCardBattleAdapter::ResolveCardPlay(
			ResolvedState,
			ExpectedCardInstanceId,
			ExpectedTargetUnitId,
			ExpectedResult,
			&ResolveError);
		Test.TestTrue(FString::Printf(TEXT("%s expected setup resolves through the real adapter: %s"), *Label, *ResolveError), bExpectedResolves);

		FGameXXKSimulationDecision First;
		FGameXXKSimulationDecision Second;
		FString FirstError;
		FString SecondError;
		const bool bFirstChosen = FGameXXKCombatSimulationRules::ChooseSkilledDecisionForTest(
			State,
			First,
			&FirstError);
		const bool bSecondChosen = FGameXXKCombatSimulationRules::ChooseSkilledDecisionForTest(
			State,
			Second,
			&SecondError);
		Test.TestTrue(FString::Printf(TEXT("%s first decision resolves: %s"), *Label, *FirstError), bFirstChosen);
		Test.TestTrue(FString::Printf(TEXT("%s repeated decision resolves: %s"), *Label, *SecondError), bSecondChosen);
		Test.TestFalse(Label + TEXT(" does not end the phase while its setup is useful"), First.bEndPlayerPhase);
		Test.TestEqual(Label + TEXT(" chooses the expected setup card"), First.CardInstanceId, ExpectedCardInstanceId);
		Test.TestEqual(Label + TEXT(" chooses the stable target"), First.TargetUnitId, ExpectedTargetUnitId);
		Test.TestEqual(Label + TEXT(" repeats the same card decision"), Second.CardInstanceId, First.CardInstanceId);
		Test.TestEqual(Label + TEXT(" repeats the same target decision"), Second.TargetUnitId, First.TargetUnitId);
		Test.TestEqual(Label + TEXT(" repeats the same end-phase decision"), Second.bEndPlayerPhase, First.bEndPlayerPhase);
		return bExpectedPreviewBuilt && bExpectedResolves && bFirstChosen && bSecondChosen;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatSimulationPolicyTest,
	"GameXXK.Simulation.Policy.SetupAwareProfessionPuzzles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatSimulationPolicyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCombatSimulationPolicyTest;

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::Blade);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("BladeMana"), TEXT("Profession.Blade.LianXiGuiQiao"), Owner, 0),
			MakeCard(TEXT("BladeFollow"), TEXT("Profession.Blade.FengHou"), Owner, 1)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::Blade, Cards, {TEXT("BladeMana")}, EGameXXKCardTerrain::Plain, 1, 100, 0, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("blade Mana setup"), State, TEXT("BladeMana"));
	}

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::Guard);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("GuardDefense"), TEXT("Profession.Guard.TieBi"), Owner, 0),
			MakeCard(TEXT("GuardAttack"), TEXT("Profession.Guard.ZhenDun"), Owner, 1)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::Guard, Cards, {TEXT("GuardDefense"), TEXT("GuardAttack")}, EGameXXKCardTerrain::Plain, 1, 5, 20, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("low-health guard defense"), State, TEXT("GuardDefense"));
	}

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::Healer);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("HealerFormula"), TEXT("Profession.Healer.XingQiZhen"), Owner, 0),
			MakeCard(TEXT("HealerPayoff"), TEXT("Profession.Healer.CaoMuFuZhi"), Owner, 1)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::Healer, Cards, {TEXT("HealerFormula")}, EGameXXKCardTerrain::Plain, 3, 80, 20, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("healer formula setup"), State, TEXT("HealerFormula"));
	}

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::Hunter);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("HunterSetup"), TEXT("Profession.Hunter.YingYan"), Owner, 0),
			MakeCard(TEXT("HunterHeavy"), TEXT("Profession.Hunter.LianZhuJian"), Owner, 1),
			MakeCard(TEXT("HunterDrawA"), TEXT("Profession.Hunter.FuBu"), Owner, 2),
			MakeCard(TEXT("HunterDrawB"), TEXT("Profession.Hunter.XunXiJian"), Owner, 3)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::Hunter, Cards, {TEXT("HunterSetup"), TEXT("HunterHeavy")}, EGameXXKCardTerrain::Plain, 1, 100, 20, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("hunter Charge setup"), State, TEXT("HunterSetup"));
	}

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::Sorcerer);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("SorcererMana"), TEXT("Profession.Sorcerer.JuLing"), Owner, 0),
			MakeCard(TEXT("SorcererFireA"), TEXT("Profession.Sorcerer.LiHuoYin"), Owner, 1),
			MakeCard(TEXT("SorcererFireB"), TEXT("Profession.Sorcerer.YanQiang"), Owner, 2),
			MakeCard(TEXT("SorcererFireC"), TEXT("Profession.Sorcerer.BaoYanShu"), Owner, 3),
			MakeCard(TEXT("SorcererSearch"), TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), Owner, 4)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::Sorcerer, Cards, {TEXT("SorcererMana"), TEXT("SorcererFireA")}, EGameXXKCardTerrain::Plain, 1, 100, 0, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("sorcerer Mana setup"), State, TEXT("SorcererMana"));
	}

	{
		const FName Owner = OwnerIdForRole(EGameXXKCharacterRole::FormationMaster);
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("FormationSwitch"), TEXT("Profession.FormationMaster.GuanShi"), Owner, 0),
			MakeCard(TEXT("FormationPayoff"), TEXT("Profession.FormationMaster.HuiShengZhenSha"), Owner, 1)};
		FGameXXKRuntimeState State;
		if (!BuildState(*this, EGameXXKCharacterRole::FormationMaster, Cards, {TEXT("FormationSwitch")}, EGameXXKCardTerrain::Cave, 1, 100, 20, State))
		{
			return false;
		}
		ExpectStableDecision(*this, TEXT("formation terrain setup"), State, TEXT("FormationSwitch"), EnemyUnitId);
	}

	return true;
}

#endif
