#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardOutcomeAuditTest
{
	const FName OwnerUnitId(TEXT("Outcome.Owner"));
	const FName AllyUnitId(TEXT("Outcome.Ally"));
	const FName EnemyAUnitId(TEXT("Outcome.EnemyA"));
	const FName EnemyBUnitId(TEXT("Outcome.EnemyB"));
	const FName MedicineCardInstanceId(TEXT("Outcome.HuiChun"));

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

	FGameXXKCardInstance MakeMedicineCard()
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = MedicineCardInstanceId;
		Card.CardId = TEXT("Hero.Healer.HuiChunNiMai");
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = TEXT("Outcome.Source.HuiChun");
		Card.AcquisitionOrdinal = 0;
		return Card;
	}

	bool BuildRuntime(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& OutRuntime, const int32 Seed)
	{
		const FGameXXKCardInstance MedicineCard = MakeMedicineCard();
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			{MedicineCard},
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand = {MedicineCard};
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
	return true;
}

#endif
