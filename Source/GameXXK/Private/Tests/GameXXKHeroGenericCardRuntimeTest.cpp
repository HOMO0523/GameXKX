#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroGenericCardRuntimeTest
{
	const FName HeroUnitId(TEXT("Hero"));
	const FName AllyUnitId(TEXT("Ally"));
	const FName SecondAllyUnitId(TEXT("SecondAlly"));
	const FName EnemyUnitId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
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
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeBasicUnits(
		const bool bIncludeAlly = false,
		const bool bIncludeSecondAlly = false,
		const int32 EnemyHP = 1000)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 10, 20, 20, 1)};
		if (bIncludeAlly)
		{
			Units.Add(MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 80, 100, 10, 20, 20, 2));
		}
		if (bIncludeSecondAlly)
		{
			Units.Add(MakeUnit(SecondAllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 60, 100, 10, 20, 20, 3));
		}
		Units.Add(MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, EnemyHP, 10, 0, 0, 10));
		return Units;
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const FName OwnerUnitId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Instance;
		Instance.InstanceId = FName(InstanceId);
		Instance.CardId = FName(CardId);
		Instance.CurrentQuality = EGameXXKCardQuality::Common;
		Instance.OwnerUnitId = OwnerUnitId;
		Instance.SourceEntryId = FName(*FString::Printf(TEXT("Generic.Source.%d"), AcquisitionOrdinal));
		Instance.AcquisitionOrdinal = AcquisitionOrdinal;
		return Instance;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardCombatUnit>& Units,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandInstanceIds,
		const int32 SharedEnergy = 10,
		const int32 Seed = 41001)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("generic-card runtime failed to initialize: %s"), *Error));
			return false;
		}

		TSet<FName> HandIds;
		for (const FName InstanceId : HandInstanceIds)
		{
			HandIds.Add(InstanceId);
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(HandIds.Contains(Card.InstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = SharedEnergy;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic generic-card fixture is invalid: %s"), *Error));
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

	EGameXXKCardZone FindZone(const FGameXXKCardBattleRuntime& Runtime, const FName InstanceId)
	{
		EGameXXKCardZone Zone = EGameXXKCardZone::Invalid;
		GameXXKCardRules::FindInstance(Runtime.Deck, InstanceId, Zone);
		return Zone;
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

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	TArray<FGameXXKCardInstance> MakeDrawFixture(
		const TCHAR* ActiveInstanceId,
		const TCHAR* ActiveCardId,
		const int32 HandFillers,
		const int32 DrawFillers,
		const FName OwnerUnitId = HeroUnitId)
	{
		TArray<FGameXXKCardInstance> Cards;
		Cards.Add(MakeCard(ActiveInstanceId, ActiveCardId, OwnerUnitId, 0));
		for (int32 Index = 0; Index < HandFillers; ++Index)
		{
			Cards.Add(MakeCard(
				*FString::Printf(TEXT("Generic.Hand.%d"), Index),
				TEXT("Route.General.PoJiaTuCi"),
				HeroUnitId,
				Cards.Num()));
		}
		for (int32 Index = 0; Index < DrawFillers; ++Index)
		{
			Cards.Add(MakeCard(
				*FString::Printf(TEXT("Generic.Draw.%d"), Index),
				TEXT("Route.General.PoJiaTuCi"),
				HeroUnitId,
				Cards.Num()));
		}
		return Cards;
	}

	TArray<FName> FirstCardAndHandFillers(const TArray<FGameXXKCardInstance>& Cards, const int32 HandFillers)
	{
		TArray<FName> Result;
		for (int32 Index = 0; Index < FMath::Min(Cards.Num(), HandFillers + 1); ++Index)
		{
			Result.Add(Cards[Index].InstanceId);
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericQingFengTest,
	"GameXXK.Data.HeroCards.Generic.QingFeng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericQingFengTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("QingFeng"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 0),
		MakeCard(TEXT("SameOwner"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1),
		MakeCard(TEXT("OtherOwner"), TEXT("Hero.Generic.SuiYanJi"), AllyUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(true), Cards, {TEXT("QingFeng"), TEXT("SameOwner"), TEXT("OtherOwner")}))
	{
		return false;
	}

	FGameXXKCardPlayResult QingFengResult;
	if (!Resolve(*this, Runtime, TEXT("QingFeng"), EnemyUnitId, QingFengResult, TEXT("Qing Feng")))
	{
		return true;
	}
	TestEqual(TEXT("Qing Feng emits one direct packet"), QingFengResult.DamageResults.Num(), 1);
	if (QingFengResult.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("Qing Feng deals exactly 140% of ten attack"), QingFengResult.DamageResults[0].BaseRequestedDamage, 14);
	}
	TestEqual(TEXT("Qing Feng registers one next-card modifier"), Runtime.Modifiers.Num(), 1);

	FGameXXKCardPlayPreview SameOwnerPreview;
	FString Error;
	TestTrue(FString::Printf(TEXT("same-owner preview builds: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("SameOwner"), SameOwnerPreview, &Error));
	TestEqual(TEXT("the source owner's card is not discounted"), SameOwnerPreview.EffectiveEnergyCost, 1);
	FGameXXKCardPlayResult SameOwnerResult;
	if (Resolve(*this, Runtime, TEXT("SameOwner"), EnemyUnitId, SameOwnerResult, TEXT("same-owner follow-up")))
	{
		TestEqual(TEXT("same-owner play does not consume the discount"), Runtime.Modifiers.Num(), 1);
	}

	FGameXXKCardPlayPreview OtherOwnerPreview;
	Error.Reset();
	TestTrue(FString::Printf(TEXT("other-owner preview builds: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("OtherOwner"), OtherOwnerPreview, &Error));
	TestEqual(TEXT("the next different owner's active card costs one less"), OtherOwnerPreview.EffectiveEnergyCost, 0);
	const int32 EnergyBeforeOtherOwner = Runtime.Deck.SharedEnergy;
	FGameXXKCardPlayResult OtherOwnerResult;
	if (Resolve(*this, Runtime, TEXT("OtherOwner"), EnemyUnitId, OtherOwnerResult, TEXT("other-owner follow-up")))
	{
		TestEqual(TEXT("the discounted play spends no energy"), Runtime.Deck.SharedEnergy, EnergyBeforeOtherOwner);
		TestEqual(TEXT("the exact previewed discount is consumed"), Runtime.Modifiers.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericHeYuTest,
	"GameXXK.Data.HeroCards.Generic.HeYu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericHeYuTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	auto RunCase = [this](
		const TCHAR* Label,
		const int32 Bleed,
		const int32 Poison,
		const int32 Burn,
		const int32 Rot,
		const EGameXXKCardDamageCause ExpectedCause,
		const EGameXXKCardStatus ExpectedConsumedStatus,
		const int32 ExpectedTriggeredStacks)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("HeYu"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 0),
			MakeCard(TEXT("Filler"), TEXT("Route.General.PoJiaTuCi"), HeroUnitId, 1)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, MakeBasicUnits(), Cards, {TEXT("HeYu"), TEXT("Filler")}, 3, 41100 + Bleed + Poison + Burn))
		{
			return;
		}
		FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyUnitId);
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Bleed, Bleed);
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Poison, Poison);
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Burn, Burn);
		GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::DamageOverTime, Rot);
		FGameXXKCardPlayPreview Preview;
		FString Error;
		const bool bPreviewBuilt = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("HeYu"), Preview, &Error);
		TestTrue(FString::Printf(TEXT("%s preview builds: %s"), Label, *Error), bPreviewBuilt);
		if (bPreviewBuilt)
		{
			TestEqual(FString::Printf(TEXT("%s costs one energy"), Label), Preview.EffectiveEnergyCost, 1);
			TestEqual(FString::Printf(TEXT("%s costs three mana"), Label), Preview.EffectiveManaCost, 3);
		}
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("HeYu"), EnemyUnitId, Result, Label))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("%s spends three mana"), Label), FindUnit(Runtime, HeroUnitId)->Mana, 17);
		const int32 ExpectedPacketCount = 1 + (Bleed > 0 ? 1 : 0) + (ExpectedTriggeredStacks > 0 ? 1 : 0);
		TestEqual(FString::Printf(TEXT("%s emits the expected packet count"), Label), Result.DamageResults.Num(), ExpectedPacketCount);
		if (Bleed > 0)
		{
			const FGameXXKCardDamageResult* AutomaticBleed = Result.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& DamageResult)
			{
				return DamageResult.Cause == EGameXXKCardDamageCause::Bleed;
			});
			TestNotNull(FString::Printf(TEXT("%s direct hit automatically triggers Bleed"), Label), AutomaticBleed);
			if (AutomaticBleed)
			{
				TestEqual(FString::Printf(TEXT("%s automatic Bleed snapshots its live layers"), Label), AutomaticBleed->StatusStacksBefore, Bleed);
				TestEqual(FString::Printf(TEXT("%s automatic Bleed consumes one layer"), Label), AutomaticBleed->StatusStacksConsumed, 1);
			}
			TestEqual(FString::Printf(TEXT("%s leaves Bleed one lower after the direct hit"), Label),
				Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), Bleed - 1);
		}
		if (ExpectedTriggeredStacks > 0)
		{
			const FGameXXKCardDamageResult* Trigger = Result.DamageResults.FindByPredicate([ExpectedCause](const FGameXXKCardDamageResult& DamageResult)
			{
				return DamageResult.Cause == ExpectedCause;
			});
			TestNotNull(FString::Printf(TEXT("%s explicitly triggers the highest remaining DoT"), Label), Trigger);
			if (Trigger)
			{
				TestEqual(FString::Printf(TEXT("%s uses the fixed status cause"), Label), Trigger->Cause, ExpectedCause);
				TestEqual(FString::Printf(TEXT("%s snapshots the highest remaining live stack"), Label), Trigger->StatusStacksBefore, ExpectedTriggeredStacks);
				TestEqual(FString::Printf(TEXT("%s adds the full Rot amplifier"), Label), Trigger->RotDamageBonus, Rot);
				TestEqual(FString::Printf(TEXT("%s consumes one explicitly triggered stack"), Label), Trigger->StatusStacksConsumed, 1);
			}
			TestEqual(FString::Printf(TEXT("%s leaves the explicitly chosen status one lower"), Label),
				Status(Runtime, EnemyUnitId, ExpectedConsumedStatus), ExpectedTriggeredStacks - 1);
		}
		TestEqual(FString::Printf(TEXT("%s never consumes Rot"), Label), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::DamageOverTime), Rot);
	};

	RunCase(TEXT("initial tie becomes Poison after the direct hit consumes Bleed"), 5, 5, 5, 20, EGameXXKCardDamageCause::Poison, EGameXXKCardStatus::Poison, 5);
	RunCase(TEXT("Poison wins when strictly highest"), 3, 6, 4, 2, EGameXXKCardDamageCause::Poison, EGameXXKCardStatus::Poison, 6);
	RunCase(TEXT("Burn wins when strictly highest"), 3, 4, 7, 2, EGameXXKCardDamageCause::Burn, EGameXXKCardStatus::Burn, 7);
	RunCase(TEXT("Rot alone is never selected"), 0, 0, 0, 20, EGameXXKCardDamageCause::Invalid, EGameXXKCardStatus::None, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericFengShenTest,
	"GameXXK.Data.HeroCards.Generic.FengShen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericFengShenTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = MakeDrawFixture(TEXT("FengShen"), TEXT("Hero.Generic.FengShenBu"), 3, 4);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(true), Cards, FirstCardAndHandFillers(Cards, 3), 3, 41201))
	{
		return false;
	}
	FGameXXKCardPlayPreview Preview;
	TestTrue(TEXT("Feng Shen preview builds"), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("FengShen"), Preview));
	TestEqual(TEXT("Feng Shen costs zero energy"), Preview.EffectiveEnergyCost, 0);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("FengShen"), AllyUnitId, Result, TEXT("Feng Shen")))
	{
		return true;
	}
	TestEqual(TEXT("Feng Shen grants two Agility"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Agility), 2);
	TestEqual(TEXT("Feng Shen draws two before opening discard"), Runtime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("Feng Shen requires one discard"), Runtime.Deck.PendingChoice.RequiredDiscardCount, 1);
	TestEqual(TEXT("Feng Shen enters ExhaustPile before the choice"), FindZone(Runtime, TEXT("FengShen")), EGameXXKCardZone::ExhaustPile);
	FString Error;
	const FName DiscardId = Runtime.Deck.Hand[0].InstanceId;
	TestTrue(FString::Printf(TEXT("Feng Shen forced discard resolves: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(Runtime, {DiscardId}, &Error));
	TestEqual(TEXT("Feng Shen leaves four cards after discarding one"), Runtime.Deck.Hand.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericSuiYanTest,
	"GameXXK.Data.HeroCards.Generic.SuiYan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericSuiYanTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("SuiYan"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(), Cards, {TEXT("SuiYan")}, 3, 41301))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("SuiYan"), EnemyUnitId, Result, TEXT("Sui Yan")))
	{
		return true;
	}
	TestEqual(TEXT("Sui Yan emits one packet"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("Sui Yan deals exactly 150% attack"), Result.DamageResults[0].BaseRequestedDamage, 15);
	}
	TestEqual(TEXT("Sui Yan applies Vulnerability3"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Vulnerability), 3);
	TestEqual(TEXT("Sui Yan applies Mark1"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericGuiYuanTest,
	"GameXXK.Data.HeroCards.Generic.GuiYuan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericGuiYuanTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("GuiYuan"), TEXT("Hero.Generic.GuiYuanShu"), HeroUnitId, 0),
		MakeCard(TEXT("HeroFollow"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1),
		MakeCard(TEXT("AllyFollow"), TEXT("Hero.Generic.SuiYanJi"), AllyUnitId, 2)};
	TArray<FGameXXKCardCombatUnit> Units = MakeBasicUnits(true);
	Units[1].HP = 30;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Units, Cards, {TEXT("GuiYuan"), TEXT("HeroFollow"), TEXT("AllyFollow")}, 5, 41401))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Ally = FindUnit(Runtime, AllyUnitId);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Bleed, 4);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Burn, 2);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::DamageOverTime, 7);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Mark, 4);
	GameXXKCardRules::AddCombatStatus(*Ally, EGameXXKCardStatus::Vulnerability, 3);

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("GuiYuan"), AllyUnitId, Result, TEXT("Gui Yuan")))
	{
		return true;
	}
	TestEqual(TEXT("Gui Yuan heals twelve"), FindUnit(Runtime, AllyUnitId)->HP, 42);
	TestEqual(TEXT("Gui Yuan clears all Bleed"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("Gui Yuan clears all Poison"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Poison), 0);
	TestEqual(TEXT("Gui Yuan clears all Burn"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Burn), 0);
	TestEqual(TEXT("Gui Yuan preserves Rot"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::DamageOverTime), 7);
	TestEqual(TEXT("Gui Yuan preserves Mark"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Mark), 4);
	TestEqual(TEXT("Gui Yuan preserves Vulnerability"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Vulnerability), 3);
	TestEqual(TEXT("Gui Yuan registers one target-bound discount"), Runtime.Modifiers.Num(), 1);

	FGameXXKCardPlayPreview HeroPreview;
	TestTrue(TEXT("unbound owner preview builds"), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("HeroFollow"), HeroPreview));
	TestEqual(TEXT("an unbound owner is not discounted"), HeroPreview.EffectiveEnergyCost, 1);
	FGameXXKCardPlayResult HeroResult;
	if (Resolve(*this, Runtime, TEXT("HeroFollow"), EnemyUnitId, HeroResult, TEXT("unbound owner follow-up")))
	{
		TestEqual(TEXT("an unbound active card does not consume the target discount"), Runtime.Modifiers.Num(), 1);
	}
	FGameXXKCardPlayPreview AllyPreview;
	TestTrue(TEXT("bound target-owner preview builds"), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("AllyFollow"), AllyPreview));
	TestEqual(TEXT("the selected target's next active card costs one less"), AllyPreview.EffectiveEnergyCost, 0);
	FGameXXKCardPlayResult AllyResult;
	if (Resolve(*this, Runtime, TEXT("AllyFollow"), EnemyUnitId, AllyResult, TEXT("bound target-owner follow-up")))
	{
		TestEqual(TEXT("the target-bound discount is consumed once"), Runtime.Modifiers.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericHengJianTest,
	"GameXXK.Data.HeroCards.Generic.HengJian",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericHengJianTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("HengJian"), TEXT("Hero.Generic.HengJianShouShi"), HeroUnitId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(true), Cards, {TEXT("HengJian")}, 3, 41501))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HengJian"), AllyUnitId, Result, TEXT("Heng Jian")))
	{
		return true;
	}
	TestEqual(TEXT("Heng Jian applies Mark2"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Mark), 2);
	TestEqual(TEXT("Heng Jian adds Armor16"), FindUnit(Runtime, AllyUnitId)->Armor, 16);
	TestEqual(TEXT("Heng Jian registers one visible Block use"), Status(Runtime, AllyUnitId, EGameXXKCardStatus::Block), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericNingShenTest,
	"GameXXK.Data.HeroCards.Generic.NingShen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericNingShenTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("NingShen"), TEXT("Hero.Generic.NingShenTuNa"), HeroUnitId, 0)};
	TArray<FGameXXKCardCombatUnit> Units = MakeBasicUnits();
	Units[0].Mana = 5;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Units, Cards, {TEXT("NingShen")}, 3, 41601))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("NingShen"), NAME_None, Result, TEXT("Ning Shen")))
	{
		return true;
	}
	TestEqual(TEXT("Ning Shen grants Momentum2"), Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 2);
	TestEqual(TEXT("Ning Shen restores ten mana"), FindUnit(Runtime, HeroUnitId)->Mana, 15);
	TestEqual(TEXT("Ning Shen enters ExhaustPile"), FindZone(Runtime, TEXT("NingShen")), EGameXXKCardZone::ExhaustPile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericGuanXiTest,
	"GameXXK.Data.HeroCards.Generic.GuanXi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericGuanXiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = MakeDrawFixture(TEXT("GuanXi"), TEXT("Hero.Generic.GuanXi"), 3, 5);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(), Cards, FirstCardAndHandFillers(Cards, 3), 3, 41701))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("GuanXi"), NAME_None, Result, TEXT("Guan Xi")))
	{
		return true;
	}
	TestEqual(TEXT("Guan Xi draws three before discard"), Runtime.Deck.Hand.Num(), 6);
	TestEqual(TEXT("Guan Xi requires one discard"), Runtime.Deck.PendingChoice.RequiredDiscardCount, 1);
	TestEqual(TEXT("Guan Xi enters ExhaustPile"), FindZone(Runtime, TEXT("GuanXi")), EGameXXKCardZone::ExhaustPile);
	FString Error;
	const FName DiscardId = Runtime.Deck.Hand[0].InstanceId;
	TestTrue(FString::Printf(TEXT("Guan Xi forced discard resolves: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(Runtime, {DiscardId}, &Error));
	TestEqual(TEXT("Guan Xi leaves five cards after discard"), Runtime.Deck.Hand.Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericPoYunTest,
	"GameXXK.Data.HeroCards.Generic.PoYun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericPoYunTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	auto MakeRuntime = [this](FGameXXKCardBattleRuntime& Runtime, const int32 Seed)
	{
		TArray<FGameXXKCardCombatUnit> Units = MakeBasicUnits();
		Units[0].Attack = 20;
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("PoYun"), TEXT("Hero.Generic.PoYunYiShan"), HeroUnitId, 0),
			MakeCard(TEXT("HandFiller"), TEXT("Route.General.PoJiaTuCi"), HeroUnitId, 1),
			MakeCard(TEXT("DrawFiller"), TEXT("Route.General.PoJiaTuCi"), HeroUnitId, 2)};
		return BuildRuntime(*this, Runtime, Units, Cards, {TEXT("PoYun"), TEXT("HandFiller")}, 3, Seed);
	};

	FGameXXKCardBattleRuntime WithAgility;
	if (MakeRuntime(WithAgility, 41801))
	{
		GameXXKCardRules::AddCombatStatus(*FindUnit(WithAgility, HeroUnitId), EGameXXKCardStatus::Agility, 2);
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, WithAgility, TEXT("PoYun"), EnemyUnitId, Result, TEXT("Po Yun with Agility")))
		{
			TestEqual(TEXT("Po Yun with Agility emits base and bonus packets"), Result.DamageResults.Num(), 2);
			if (Result.DamageResults.Num() == 2)
			{
				TestEqual(TEXT("Po Yun base packet is 160%"), Result.DamageResults[0].BaseRequestedDamage, 32);
				TestEqual(TEXT("Po Yun bonus packet is a separate 100%"), Result.DamageResults[1].BaseRequestedDamage, 20);
			}
			TestEqual(TEXT("Po Yun consumes exactly one Agility"), Status(WithAgility, HeroUnitId, EGameXXKCardStatus::Agility), 1);
			TestEqual(TEXT("Po Yun draws one only after successful consumption"), WithAgility.Deck.Hand.Num(), 2);
		}
	}

	FGameXXKCardBattleRuntime WithoutAgility;
	if (MakeRuntime(WithoutAgility, 41802))
	{
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, WithoutAgility, TEXT("PoYun"), EnemyUnitId, Result, TEXT("Po Yun without Agility")))
		{
			TestEqual(TEXT("Po Yun without Agility emits only its base packet"), Result.DamageResults.Num(), 1);
			TestEqual(TEXT("Po Yun without Agility does not draw"), WithoutAgility.Deck.Hand.Num(), 1);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericXingQiTest,
	"GameXXK.Data.HeroCards.Generic.XingQi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericXingQiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = MakeDrawFixture(TEXT("XingQi"), TEXT("Hero.Generic.XingQiHuiHuan"), 2, 4);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, MakeBasicUnits(), Cards, FirstCardAndHandFillers(Cards, 2), 3, 41901))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("XingQi"), NAME_None, Result, TEXT("Xing Qi")))
	{
		return true;
	}
	TestEqual(TEXT("Xing Qi draws two"), Runtime.Deck.Hand.Num(), 4);
	TestEqual(TEXT("Xing Qi restores one energy"), Runtime.Deck.SharedEnergy, 4);
	TestEqual(TEXT("Xing Qi enters ExhaustPile"), FindZone(Runtime, TEXT("XingQi")), EGameXXKCardZone::ExhaustPile);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericJianYiTest,
	"GameXXK.Data.HeroCards.Generic.JianYi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericJianYiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	auto RunCase = [this](
		const int32 StartingMomentum,
		const int32 ExpectedBaseRequested,
		const int32 ExpectedRequested,
		const int32 ExpectedEnergy)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("JianYi"), TEXT("Hero.Generic.JianYiGuanHong"), HeroUnitId, 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, MakeBasicUnits(), Cards, {TEXT("JianYi")}, 5, 42001 + StartingMomentum))
		{
			return;
		}
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroUnitId), EGameXXKCardStatus::Momentum, StartingMomentum);
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("JianYi"), EnemyUnitId, Result, TEXT("Jian Yi")))
		{
			return;
		}
		TestEqual(FString::Printf(TEXT("Jian Yi with %d Momentum emits one packet"), StartingMomentum), Result.DamageResults.Num(), 1);
		if (Result.DamageResults.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("Jian Yi with %d Momentum applies +20 percentage points per captured stack"), StartingMomentum),
				Result.DamageResults[0].BaseRequestedDamage, ExpectedBaseRequested);
			TestEqual(FString::Printf(TEXT("Jian Yi with %d Momentum retains the starting flat Momentum contribution"), StartingMomentum),
				Result.DamageResults[0].RequestedDamage, ExpectedRequested);
			TestEqual(FString::Printf(TEXT("Jian Yi with %d Momentum audits its packet-start snapshot"), StartingMomentum),
				Result.DamageResults[0].MomentumDamageBonus, StartingMomentum);
		}
		TestEqual(FString::Printf(TEXT("Jian Yi consumes all %d starting Momentum"), StartingMomentum),
			Status(Runtime, HeroUnitId, EGameXXKCardStatus::Momentum), 0);
		TestEqual(FString::Printf(TEXT("Jian Yi with %d Momentum applies the three-stack energy threshold"), StartingMomentum),
			Runtime.Deck.SharedEnergy, ExpectedEnergy);
	};

	RunCase(0, 26, 26, 3);
	RunCase(2, 30, 32, 3);
	RunCase(4, 34, 38, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroGenericGuiYuanFanZhaoTest,
	"GameXXK.Data.HeroCards.Generic.GuiYuanFanZhao",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroGenericGuiYuanFanZhaoTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroGenericCardRuntimeTest;
	TArray<FGameXXKCardCombatUnit> Units = MakeBasicUnits(true, true);
	Units[0].HP = 90;
	Units[1].HP = 80;
	Units[2].HP = 98;
	Units.Insert(MakeUnit(TEXT("DefeatedAlly"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 0, 100, 10, 10, 10, 4), 3);
	const TArray<FGameXXKCardInstance> Cards = MakeDrawFixture(TEXT("FanZhao"), TEXT("Hero.Generic.GuiYuanFanZhao"), 2, 4);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Units, Cards, FirstCardAndHandFillers(Cards, 2), 5, 42101))
	{
		return false;
	}
	for (const FName UnitId : {HeroUnitId, AllyUnitId, SecondAllyUnitId})
	{
		FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		GameXXKCardRules::AddCombatStatus(*Unit, EGameXXKCardStatus::Bleed, 4);
		GameXXKCardRules::AddCombatStatus(*Unit, EGameXXKCardStatus::Poison, 3);
		GameXXKCardRules::AddCombatStatus(*Unit, EGameXXKCardStatus::Burn, 2);
		GameXXKCardRules::AddCombatStatus(*Unit, EGameXXKCardStatus::DamageOverTime, 7);
		GameXXKCardRules::AddCombatStatus(*Unit, EGameXXKCardStatus::Mark, 1);
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("FanZhao"), NAME_None, Result, TEXT("Gui Yuan Fan Zhao")))
	{
		return true;
	}
	TestEqual(TEXT("Fan Zhao heals the hero by six"), FindUnit(Runtime, HeroUnitId)->HP, 96);
	TestEqual(TEXT("Fan Zhao heals the first ally by six"), FindUnit(Runtime, AllyUnitId)->HP, 86);
	TestEqual(TEXT("Fan Zhao clamps the second ally at max health"), FindUnit(Runtime, SecondAllyUnitId)->HP, 100);
	TestEqual(TEXT("Fan Zhao never revives a defeated ally"), FindUnit(Runtime, TEXT("DefeatedAlly"))->HP, 0);
	for (const FName UnitId : {HeroUnitId, AllyUnitId, SecondAllyUnitId})
	{
		TestEqual(FString::Printf(TEXT("Fan Zhao gives %s Armor12"), *UnitId.ToString()), FindUnit(Runtime, UnitId)->Armor, 12);
		TestEqual(FString::Printf(TEXT("Fan Zhao clears %s Bleed"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Bleed), 0);
		TestEqual(FString::Printf(TEXT("Fan Zhao clears %s Poison"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Poison), 0);
		TestEqual(FString::Printf(TEXT("Fan Zhao clears %s Burn"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Burn), 0);
		TestEqual(FString::Printf(TEXT("Fan Zhao preserves %s Rot"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::DamageOverTime), 7);
		TestEqual(FString::Printf(TEXT("Fan Zhao preserves %s Mark"), *UnitId.ToString()), Status(Runtime, UnitId, EGameXXKCardStatus::Mark), 1);
	}
	TestEqual(TEXT("Fan Zhao draws two"), Runtime.Deck.Hand.Num(), 4);
	return true;
}

#endif
