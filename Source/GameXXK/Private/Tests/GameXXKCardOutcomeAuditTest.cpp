#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardRulesTestBridge
{
	bool IsMedicineReverseDamage(const FGameXXKCardDamageResult& Result, FName OwnerUnitId);
}

namespace GameXXKCardOutcomeAuditTest
{
	const FName OwnerUnitId(TEXT("Outcome.Owner"));
	const FName AllyUnitId(TEXT("Outcome.Ally"));
	const FName EnemyAUnitId(TEXT("Outcome.EnemyA"));
	const FName EnemyBUnitId(TEXT("Outcome.EnemyB"));
	const FName MedicineCardInstanceId(TEXT("Outcome.HuiChun"));
	const FName MedicineCardId(TEXT("Hero.Healer.HuiChunNiMai"));
	const FName FlatReverseCardId(TEXT("Profession.Healer.LingZhiXuMing"));
	const FName BelowThresholdCardId(TEXT("Profession.Healer.CaoMuFuZhi"));
	const FName FirstHealingFormulaCardId(TEXT("Profession.Healer.CaoMuFuZhi"));
	const FName LargeHealingFormulaCardId(TEXT("Profession.Healer.WenYangGao"));

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
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName CardId)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = MedicineCardInstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = TEXT("Outcome.Source.HuiChun");
		Card.AcquisitionOrdinal = 0;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const int32 Seed,
		const FName CardId = MedicineCardId)
	{
		const FGameXXKCardInstance TriggerCard = MakeCard(CardId);
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			{TriggerCard},
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand = {TriggerCard};
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit fixture is invalid: %s"), *Error));
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

	int32 Status(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId,
		const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool InstallFormula(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName FormulaCardId)
	{
		const FGameXXKCardDefinition* FormulaDefinition = FGameXXKCardCatalog::FindCardDefinition(FormulaCardId);
		if (!FormulaDefinition || FormulaDefinition->HealerRule.FormulaKind == EGameXXKHealerFormulaKind::None)
		{
			Test.AddError(FString::Printf(TEXT("outcome audit formula source is missing: %s"), *FormulaCardId.ToString()));
			return false;
		}
		FGameXXKHealerFormulaRuntime& Formula = Runtime.HealerFormulas.AddDefaulted_GetRef();
		Formula.OwnerUnitId = OwnerUnitId;
		Formula.SourceCardId = FormulaDefinition->Id;
		Formula.Kind = FormulaDefinition->HealerRule.FormulaKind;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit formula fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	const FGameXXKHealerFormulaRuntime* FindFormula(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKHealerFormulaKind Kind)
	{
		return Runtime.HealerFormulas.FindByPredicate([Kind](const FGameXXKHealerFormulaRuntime& Formula)
		{
			return Formula.OwnerUnitId == OwnerUnitId && Formula.Kind == Kind;
		});
	}

	bool ResolveCard(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName TargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* ContextText)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(
			Runtime,
			MedicineCardInstanceId,
			TargetUnitId,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves through real card rules: %s"), ContextText, *Error), bResolved);
		return bResolved;
	}

	bool ApplySharedDirectDamage(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardDamageKind Kind,
		const FName TargetUnitId,
		FGameXXKCardDamageResult& OutResult,
		const TCHAR* ContextText)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = OwnerUnitId;
		Context.Kind = Kind;
		Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
		FString Error;
		const bool bApplied = GameXXKCardRules::ApplyPlayerCardDirectDamage(
			Runtime,
			Context,
			TargetUnitId,
			3,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s uses the shared damage resolver: %s"), ContextText, *Error), bApplied);
		return bApplied;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomeDamageAuditTest,
	"GameXXK.Data.CardOutcomePreview.Audit.DamageKindAndCause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomeDamageAuditTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomeAuditTest;

	FGameXXKCardBattleRuntime DirectRuntime;
	if (!BuildRuntime(*this, DirectRuntime, 61001))
	{
		return false;
	}
	FGameXXKCardDamageResult Single;
	if (!ApplySharedDirectDamage(
		*this,
		DirectRuntime,
		EGameXXKCardDamageKind::SingleTargetAttack,
		EnemyAUnitId,
		Single,
		TEXT("single-target packet")))
	{
		return false;
	}
	FGameXXKCardDamageResult Group;
	if (!ApplySharedDirectDamage(
		*this,
		DirectRuntime,
		EGameXXKCardDamageKind::GroupAttack,
		EnemyBUnitId,
		Group,
		TEXT("group packet")))
	{
		return false;
	}

	FGameXXKCardBattleRuntime PoisonRuntime;
	if (!BuildRuntime(*this, PoisonRuntime, 61002))
	{
		return false;
	}
	FGameXXKCardCombatUnit* PoisonTarget = FindUnit(PoisonRuntime, AllyUnitId);
	if (!TestNotNull(TEXT("poison fixture has its ally target"), PoisonTarget))
	{
		return false;
	}
	TestEqual(
		TEXT("poison fixture applies three real stacks"),
		GameXXKCardRules::AddCombatStatus(*PoisonTarget, EGameXXKCardStatus::Poison, 3),
		3);
	TArray<FGameXXKCardDamageResult> EndPhaseResults;
	FString EndPhaseError;
	if (!TestTrue(
		TEXT("player end phase resolves ordinary poison"),
		GameXXKCardRules::EndPlayerCardPhase(PoisonRuntime, EndPhaseResults, &EndPhaseError)))
	{
		AddError(EndPhaseError);
		return false;
	}
	const FGameXXKCardDamageResult* PoisonPacket = EndPhaseResults.FindByPredicate([](const FGameXXKCardDamageResult& Result)
	{
		return Result.Cause == EGameXXKCardDamageCause::Poison;
	});
	if (!TestNotNull(TEXT("ordinary poison emits a real damage packet"), PoisonPacket))
	{
		return false;
	}
	const FGameXXKCardDamageResult Poison = *PoisonPacket;

	FGameXXKCardBattleRuntime MedicineRuntime;
	if (!BuildRuntime(*this, MedicineRuntime, 61003))
	{
		return false;
	}
	FGameXXKCardCombatUnit* MedicineOwner = FindUnit(MedicineRuntime, OwnerUnitId);
	if (!TestNotNull(TEXT("medicine fixture has its card owner"), MedicineOwner))
	{
		return false;
	}
	TestEqual(
		TEXT("medicine fixture applies five real stacks"),
		GameXXKCardRules::AddCombatStatus(*MedicineOwner, EGameXXKCardStatus::Medicine, 5),
		5);
	FGameXXKCardPlayResult ReverseResult;
	FString ReverseError;
	if (!TestTrue(
		TEXT("real medicine card reverses healing against an enemy"),
		GameXXKCardRules::ResolveCardPlay(
			MedicineRuntime,
			MedicineCardInstanceId,
			EnemyAUnitId,
			ReverseResult,
			&ReverseError)))
	{
		AddError(ReverseError);
		return false;
	}
	if (!TestEqual(TEXT("medicine reverse emits one damage packet"), ReverseResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& Reverse = ReverseResult.DamageResults[0];

	TestEqual(TEXT("direct single keeps kind"), Single.Kind, EGameXXKCardDamageKind::SingleTargetAttack);
	TestEqual(TEXT("group packet keeps kind"), Group.Kind, EGameXXKCardDamageKind::GroupAttack);
	TestEqual(TEXT("ordinary poison keeps dot kind"), Poison.Kind, EGameXXKCardDamageKind::DamageOverTime);
	TestEqual(TEXT("medicine reverse has medicine cause"), Reverse.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("medicine source remains the card owner"), Reverse.SourceUnitId, OwnerUnitId);
	TestTrue(
		TEXT("real Medicine reverse is accepted by the Medicine classifier"),
		GameXXKCardRulesTestBridge::IsMedicineReverseDamage(Reverse, OwnerUnitId));

	FGameXXKCardBattleRuntime FlatRuntime;
	if (!BuildRuntime(*this, FlatRuntime, 61004, FlatReverseCardId))
	{
		return false;
	}
	FGameXXKCardCombatUnit* FlatOwner = FindUnit(FlatRuntime, OwnerUnitId);
	FGameXXKCardCombatUnit* FlatTarget = FindUnit(FlatRuntime, EnemyAUnitId);
	if (!TestNotNull(TEXT("flat reverse fixture has its owner"), FlatOwner)
		|| !TestNotNull(TEXT("flat reverse fixture has its enemy"), FlatTarget))
	{
		return false;
	}
	FlatTarget->HP = 30;
	FlatTarget->Defense = 40;
	FlatTarget->Armor = 50;
	TestEqual(
		TEXT("flat reverse fixture applies five Medicine"),
		GameXXKCardRules::AddCombatStatus(*FlatOwner, EGameXXKCardStatus::Medicine, 5),
		5);
	FGameXXKCardPlayResult FlatResult;
	if (!ResolveCard(*this, FlatRuntime, EnemyAUnitId, FlatResult, TEXT("flat reverse card")))
	{
		return false;
	}
	TestEqual(TEXT("flat reverse card emits primary and flat packets"), FlatResult.DamageResults.Num(), 2);
	const FGameXXKCardDamageResult* FlatReverse = FlatResult.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Result)
	{
		return Result.RequestedDamage == 2;
	});
	if (!TestNotNull(TEXT("flat reverse emits its real two-point packet"), FlatReverse))
	{
		return false;
	}
	TestEqual(TEXT("flat reverse has Medicine cause"), FlatReverse->Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("flat reverse source remains the card owner"), FlatReverse->SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("flat reverse loses exactly two health"), FlatReverse->HealthDamage, 2);
	TestEqual(TEXT("flat reverse absorbs no Armor"), FlatReverse->ArmorAbsorbed, 0);
	TestEqual(TEXT("flat reverse keeps enemy Armor"), FindUnit(FlatRuntime, EnemyAUnitId)->Armor, 50);
	TestEqual(TEXT("flat reverse card loses seventeen total enemy health"), FindUnit(FlatRuntime, EnemyAUnitId)->HP, 13);
	TestEqual(TEXT("flat reverse card consumes the Medicine snapshot once"), Status(FlatRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 0);

	FGameXXKCardBattleRuntime FirstFormulaRuntime;
	if (!BuildRuntime(*this, FirstFormulaRuntime, 61005)
		|| !InstallFormula(*this, FirstFormulaRuntime, FirstHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult FirstFormulaResult;
	if (!ResolveCard(*this, FirstFormulaRuntime, EnemyAUnitId, FirstFormulaResult, TEXT("first-healing formula enemy reverse")))
	{
		return false;
	}
	TestEqual(TEXT("enemy Medicine reverse triggers first-healing Medicine2"), Status(FirstFormulaRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 2);
	const FGameXXKHealerFormulaRuntime* TriggeredFirstFormula = FindFormula(
		FirstFormulaRuntime,
		EGameXXKHealerFormulaKind::FirstHealingMedicine);
	if (!TestNotNull(TEXT("first-healing formula remains installed"), TriggeredFirstFormula))
	{
		return false;
	}
	TestEqual(TEXT("first-healing formula spends this round's budget"), TriggeredFirstFormula->LastTriggeredRound, FirstFormulaRuntime.RoundNumber);

	FGameXXKCardBattleRuntime EnvironmentalControlRuntime;
	if (!BuildRuntime(*this, EnvironmentalControlRuntime, 61006))
	{
		return false;
	}
	FGameXXKCardDamageContext EnvironmentalContext;
	EnvironmentalContext.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
	EnvironmentalContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult EnvironmentalResult;
	FString EnvironmentalError;
	if (!TestTrue(
		TEXT("non-Medicine environmental damage resolves through shared rules"),
		GameXXKCardRules::ApplyCombatDirectDamage(
			EnvironmentalControlRuntime.Units,
			EnvironmentalControlRuntime.GuardLinks,
			EnvironmentalContext,
			EnemyAUnitId,
			3,
			EnvironmentalResult,
			&EnvironmentalError)))
	{
		AddError(EnvironmentalError);
		return false;
	}
	TestEqual(TEXT("environmental control remains Environment"), EnvironmentalResult.Cause, EGameXXKCardDamageCause::Environment);
	TestFalse(
		TEXT("real environmental packet is rejected by the Medicine classifier"),
		GameXXKCardRulesTestBridge::IsMedicineReverseDamage(EnvironmentalResult, EnvironmentalResult.SourceUnitId));

	FGameXXKCardBattleRuntime BelowThresholdRuntime;
	if (!BuildRuntime(*this, BelowThresholdRuntime, 61007, BelowThresholdCardId)
		|| !InstallFormula(*this, BelowThresholdRuntime, LargeHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult BelowThresholdResult;
	if (!ResolveCard(*this, BelowThresholdRuntime, EnemyAUnitId, BelowThresholdResult, TEXT("below-threshold enemy reverse")))
	{
		return false;
	}
	if (!TestEqual(TEXT("below-threshold reverse emits one packet"), BelowThresholdResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& BelowThresholdDamage = BelowThresholdResult.DamageResults[0];
	TestEqual(TEXT("below-threshold reverse requests eight"), BelowThresholdDamage.RequestedDamage, 8);
	TestEqual(TEXT("below-threshold reverse keeps Medicine cause"), BelowThresholdDamage.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("below-threshold reverse keeps owner source"), BelowThresholdDamage.SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("below-threshold reverse grants no Vulnerability"), Status(BelowThresholdRuntime, EnemyAUnitId, EGameXXKCardStatus::Vulnerability), 0);
	const FGameXXKHealerFormulaRuntime* BelowThresholdFormula = FindFormula(
		BelowThresholdRuntime,
		EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability);
	if (!TestNotNull(TEXT("below-threshold formula remains installed"), BelowThresholdFormula))
	{
		return false;
	}
	TestEqual(TEXT("below-threshold reverse spends no formula budget"), BelowThresholdFormula->LastTriggeredRound, 0);

	FGameXXKCardBattleRuntime BoundaryRuntime;
	if (!BuildRuntime(*this, BoundaryRuntime, 61008)
		|| !InstallFormula(*this, BoundaryRuntime, LargeHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult BoundaryResult;
	if (!ResolveCard(*this, BoundaryRuntime, EnemyAUnitId, BoundaryResult, TEXT("ten-point boundary enemy reverse")))
	{
		return false;
	}
	if (!TestEqual(TEXT("ten-point boundary reverse emits one packet"), BoundaryResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& BoundaryDamage = BoundaryResult.DamageResults[0];
	TestEqual(TEXT("ten-point boundary reverse requests ten"), BoundaryDamage.RequestedDamage, 10);
	TestEqual(TEXT("ten-point boundary reverse keeps Medicine cause"), BoundaryDamage.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("ten-point boundary reverse keeps owner source"), BoundaryDamage.SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("ten-point boundary reverse grants Vulnerability1"), Status(BoundaryRuntime, EnemyAUnitId, EGameXXKCardStatus::Vulnerability), 1);
	const FGameXXKHealerFormulaRuntime* BoundaryFormula = FindFormula(
		BoundaryRuntime,
		EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability);
	if (!TestNotNull(TEXT("ten-point boundary formula remains installed"), BoundaryFormula))
	{
		return false;
	}
	TestEqual(TEXT("ten-point boundary reverse spends this round's formula budget"), BoundaryFormula->LastTriggeredRound, BoundaryRuntime.RoundNumber);
	return true;
}

#endif
