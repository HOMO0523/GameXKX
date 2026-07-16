#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleRuntimeTest,
	"GameXXK.Data.CardBattleRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeRuntimeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 MaxHP,
		const int32 Attack,
		const int32 Mana,
		const int32 MaxMana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeRuntimeInstances(const TCHAR* CardId, const int32 Count)
	{
		TArray<FGameXXKCardInstance> Instances;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Instance.%s.%d"), CardId, Index));
			Instance.CardId = FName(CardId);
			Instance.OwnerUnitId = TEXT("Hero");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Source.%s.%d"), CardId, Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	FGameXXKCardCombatUnit* FindRuntimeUnit(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool IsInDiscard(const FGameXXKBattleDeckState& Deck, const FName InstanceId)
	{
		return Deck.DiscardPile.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
	}
}

bool FGameXXKCardBattleRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> AttackUnits;
	AttackUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 10, 20, 1));
	AttackUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime AttackRuntime;
	TestTrue(TEXT("battle runtime initializes a serializable player phase from materialized card instances"),
		GameXXKCardRules::InitializeCardBattleRuntime(AttackRuntime, MakeRuntimeInstances(TEXT("Hero.QingFengYiShi"), 6), AttackUnits, EGameXXKCardTerrain::Plain, 771));
	TestEqual(TEXT("new battle runtime starts in the player card phase"), AttackRuntime.Phase, EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("new battle runtime starts with the shared three energy"), AttackRuntime.Deck.SharedEnergy, 3);
	TestEqual(TEXT("new battle runtime materializes the first five cards into hand"), AttackRuntime.Deck.Hand.Num(), 5);
	const FName AttackInstanceId = AttackRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview AttackPreview;
	TestTrue(TEXT("a hand attack builds a manual target preview without mutating battle state"), GameXXKCardRules::BuildCardPlayPreview(AttackRuntime, AttackInstanceId, AttackPreview));
	TestTrue(TEXT("single-enemy attack preview explicitly requires a player target"), AttackPreview.TargetRequest.bRequiresManualSelection);
	TestTrue(TEXT("single-enemy attack preview exposes the stable enemy UnitId"), GameXXKCardRules::IsManualTargetLegal(AttackPreview.TargetRequest, TEXT("Enemy")));
	TestFalse(TEXT("single-enemy attack preview refuses a friendly stable UnitId"), GameXXKCardRules::IsManualTargetLegal(AttackPreview.TargetRequest, TEXT("Hero")));
	const int32 AttackEnergyBeforeRejectedTarget = AttackRuntime.Deck.SharedEnergy;
	const int32 AttackHandBeforeRejectedTarget = AttackRuntime.Deck.Hand.Num();
	FGameXXKCardPlayResult RejectedAttackResult;
	RejectedAttackResult.CardInstanceId = TEXT("PriorResult");
	TestFalse(TEXT("submitting an illegal selected UnitId does not spend the card"), GameXXKCardRules::ResolveCardPlay(AttackRuntime, AttackInstanceId, TEXT("Hero"), RejectedAttackResult));
	TestEqual(TEXT("illegal selected UnitId leaves shared energy intact"), AttackRuntime.Deck.SharedEnergy, AttackEnergyBeforeRejectedTarget);
	TestEqual(TEXT("illegal selected UnitId leaves the hand intact"), AttackRuntime.Deck.Hand.Num(), AttackHandBeforeRejectedTarget);
	TestEqual(TEXT("illegal selected UnitId preserves caller output"), RejectedAttackResult.CardInstanceId, FName(TEXT("PriorResult")));
	FGameXXKCardPlayResult AttackResult;
	TestTrue(TEXT("submitting a fresh legal stable enemy UnitId resolves the played card"), GameXXKCardRules::ResolveCardPlay(AttackRuntime, AttackInstanceId, TEXT("Enemy"), AttackResult));
	TestEqual(TEXT("a resolved attack spends its shared energy exactly once"), AttackRuntime.Deck.SharedEnergy, 2);
	TestEqual(TEXT("the source gains the declared mana after costs are paid"), FindRuntimeUnit(AttackRuntime.Units, TEXT("Hero"))->Mana, 12);
	TestEqual(TEXT("the target takes the source attack percentage as direct damage"), FindRuntimeUnit(AttackRuntime.Units, TEXT("Enemy"))->HP, 80);
	TestTrue(TEXT("the successful play records the selected stable target"), AttackResult.TargetUnitIds.Contains(TEXT("Enemy")));
	TestTrue(TEXT("the successful play moves the exact hand instance into discard"), IsInDiscard(AttackRuntime.Deck, AttackInstanceId));

	TArray<FGameXXKCardCombatUnit> HealUnits;
	HealUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1));
	HealUnits.Add(MakeRuntimeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 30, 100, 10, 10, 10, 2));
	HealUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("healing fixture adds a removable bleed stack"), GameXXKCardRules::AddCombatStatus(HealUnits[1], EGameXXKCardStatus::Bleed, 1), 1);
	FGameXXKCardBattleRuntime HealRuntime;
	TestTrue(TEXT("healing runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(HealRuntime, MakeRuntimeInstances(TEXT("Hero.GuiYuanShu"), 6), HealUnits, EGameXXKCardTerrain::Plain, 772));
	const FName HealInstanceId = HealRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview HealPreview;
	TestTrue(TEXT("a friendly card builds a fresh manual target preview"), GameXXKCardRules::BuildCardPlayPreview(HealRuntime, HealInstanceId, HealPreview));
	TestTrue(TEXT("friendly preview includes the stable wounded ally"), GameXXKCardRules::IsManualTargetLegal(HealPreview.TargetRequest, TEXT("Ally")));
	const int32 HealEnergyBeforeRejectedTarget = HealRuntime.Deck.SharedEnergy;
	FGameXXKCardPlayResult RejectedHealResult;
	TestFalse(TEXT("enemy target cannot resolve a single-ally card"), GameXXKCardRules::ResolveCardPlay(HealRuntime, HealInstanceId, TEXT("Enemy"), RejectedHealResult));
	TestEqual(TEXT("rejected friendly-card target leaves energy intact"), HealRuntime.Deck.SharedEnergy, HealEnergyBeforeRejectedTarget);
	FGameXXKCardPlayResult HealResult;
	TestTrue(TEXT("a valid friendly target resolves healing and cleansing"), GameXXKCardRules::ResolveCardPlay(HealRuntime, HealInstanceId, TEXT("Ally"), HealResult));
	TestEqual(TEXT("healing clamps at its fixed declared amount"), FindRuntimeUnit(HealRuntime.Units, TEXT("Ally"))->HP, 66);
	TestEqual(TEXT("healing card removes its declared one DoT stack"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(HealRuntime.Units, TEXT("Ally")), EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("healing card spends both shared energy and owner mana"), HealRuntime.Deck.SharedEnergy, 1);
	TestEqual(TEXT("healing card pays owner mana before effects"), FindRuntimeUnit(HealRuntime.Units, TEXT("Hero"))->Mana, 10);

	TArray<FGameXXKCardCombatUnit> SelfUnits;
	SelfUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 10, 20, 1));
	SelfUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime SelfRuntime;
	TestTrue(TEXT("self-target runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(SelfRuntime, MakeRuntimeInstances(TEXT("Hero.FengShenBu"), 6), SelfUnits, EGameXXKCardTerrain::Plain, 773));
	const FName SelfInstanceId = SelfRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview SelfPreview;
	TestTrue(TEXT("self-target card preview succeeds"), GameXXKCardRules::BuildCardPlayPreview(SelfRuntime, SelfInstanceId, SelfPreview));
	TestFalse(TEXT("self-target card preview does not enter the mouse-arrow selection mode"), SelfPreview.TargetRequest.bRequiresManualSelection);
	TestEqual(TEXT("self-target card automatically resolves its owner UnitId"), SelfPreview.TargetRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("Hero")});
	FGameXXKCardPlayResult SelfResult;
	TestTrue(TEXT("self-target card resolves without a submitted UnitId"), GameXXKCardRules::ResolveCardPlay(SelfRuntime, SelfInstanceId, NAME_None, SelfResult));
	TestEqual(TEXT("self-target card applies its owner status"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(SelfRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::Agility), 1);
	TestEqual(TEXT("moving a card to discard before its draw lets the replacement return to a five-card hand"), SelfRuntime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("self-target card creates one discard entry while drawing its replacement"), SelfRuntime.Deck.DiscardPile.Num(), 1);

	return true;
}

#endif
