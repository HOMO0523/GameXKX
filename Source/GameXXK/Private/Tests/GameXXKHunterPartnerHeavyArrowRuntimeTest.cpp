#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHunterPartnerHeavyArrowRuntimeTest
{
	const FName HunterUnitId(TEXT("Partner.Hunter"));
	const FName EnemyUnitId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 HP,
		const int32 MaxHP,
		const int32 Defense,
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
		Unit.Defense = Defense;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 50 : 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName InstanceId, const FName CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = HunterUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Hunter.Heavy.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		const int32 Charge,
		const int32 EnemyHP,
		const int32 EnemyMaxHP,
		const int32 EnemyDefense,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& ExtraCards = {})
	{
		TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Subject"), CardId, 0)};
		Cards.Append(ExtraCards);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("Filler%d"), Index)),
				Index % 2 == 0 ? FName(TEXT("Hero.Generic.QingFengYiShi")) : FName(TEXT("Hero.Generic.HeYuZhan")),
				Index + 1));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(HunterUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 10, 100, 100, 0, 1),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, EnemyHP, EnemyMaxHP, EnemyDefense, 10)};
		FString Error;
		if (!Test.TestTrue(TEXT("Hunter Heavy Arrow runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, 57001 + GetTypeHash(CardId), &Error)))
		{
			Test.AddError(Error);
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.Hand.Add(Cards[0]);
		for (int32 Index = 1; Index < Cards.Num(); ++Index)
		{
			OutRuntime.Deck.DrawPile.Add(Cards[Index]);
		}
		OutRuntime.Deck.SharedEnergy = 10;
		if (Charge > 0)
		{
			GameXXKCardRules::AddCombatStatus(OutRuntime.Units[0], EGameXXKCardStatus::Charge, Charge);
		}
		if (!Test.TestTrue(TEXT("deterministic Heavy Arrow fixture validates"), GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error)))
		{
			Test.AddError(Error);
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

	int32 CountCause(const FGameXXKCardPlayResult& Result, const EGameXXKCardDamageCause Cause)
	{
		return Result.DamageResults.FilterByPredicate([Cause](const FGameXXKCardDamageResult& Damage)
		{
			return Damage.Cause == Cause;
		}).Num();
	}

	const FGameXXKCardDamageResult* FindDirect(const FGameXXKCardPlayResult& Result)
	{
		return Result.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Damage)
		{
			return Damage.Cause == EGameXXKCardDamageCause::DirectAttack;
		});
	}

	bool Resolve(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, FGameXXKCardPlayResult& OutResult, const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Subject"), EnemyUnitId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterPartnerHeavyArrowPayloadsTest,
	"GameXXK.Data.PartnerCards.Hunter.HeavyArrowPayloads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterPartnerHeavyArrowPayloadsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHunterPartnerHeavyArrowRuntimeTest;
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.FuZuShi"), 2, 1000, 1000, 0, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Cui Du Shi")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Cui Du has one direct packet"), Direct))
			{
				TestEqual(TEXT("Cui Du adds twenty percent per Charge to its base hit"), Direct->BaseRequestedDamage, 11);
			}
			TestEqual(TEXT("Cui Du resolves its base explosion plus two Heavy Arrow explosions"), CountCause(Result, EGameXXKCardDamageCause::ToxicExplosionPoison), 3);
			TestEqual(TEXT("three Poison explosions leave Poison3"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Poison), 3);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.DuanMaiShi"), 2, 1000, 1000, 0, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Duan Mai Shi")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Duan Mai has one direct packet"), Direct))
			{
				TestEqual(TEXT("Duan Mai adds thirty percent per Charge"), Direct->BaseRequestedDamage, 16);
			}
			TestEqual(TEXT("Duan Mai triggers Bleed once for the hit and once per Charge"), CountCause(Result, EGameXXKCardDamageCause::Bleed), 3);
			TestEqual(TEXT("three triggers consume Bleed8 down to Bleed5"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Bleed), 5);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.XunXiJian"), 2, 1000, 1000, 0, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Xun Xi Jian")))
		{
			TestEqual(TEXT("Xun Xi applies base Mark2 plus one Mark per Charge"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 4);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.PoJiaDing"), 2, 1000, 1000, 8, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Po Jia Ding")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Po Jia has one direct packet"), Direct))
			{
				TestEqual(TEXT("Po Jia ignores two Defense per Charge in addition to its damage multiplier"), Direct->DamageAfterDefense, 8);
			}
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.ShouHun"), 2, 1000, 1000, 0, Runtime)) return false;
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Mark, 3);
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Shou Hun")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Shou Hun has one direct packet"), Direct))
			{
				TestEqual(TEXT("Shou Hun uses all three snapshotted Mark layers plus Charge2"), Direct->BaseRequestedDamage, 28);
			}
			TestEqual(TEXT("Shou Hun consumes one Mark then restores one per Charge"), Status(Runtime, EnemyUnitId, EGameXXKCardStatus::Mark), 4);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.ChuanYang"), 2, 1000, 1000, 12, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Chuan Yang")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Chuan Yang has one direct packet"), Direct))
			{
				TestEqual(TEXT("Chuan Yang ignores base six plus two Defense per Charge"), Direct->DamageAfterDefense, 23);
			}
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.YingLuo"), 2, 40, 100, 0, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Ying Luo")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Ying Luo has one direct packet"), Direct))
			{
				TestEqual(TEXT("Ying Luo widens its execute threshold by five points per Charge"), Direct->BaseRequestedDamage, 42);
			}
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.LueYingJian"), 2, 1000, 1000, 0, Runtime)) return false;
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyUnitId), EGameXXKCardStatus::Mark, 1);
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Lue Ying Jian")))
		{
			TestEqual(TEXT("Lue Ying grants one Agility per consumed Charge"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Agility), 2);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHunterPartnerSequenceAndSetupTest,
	"GameXXK.Data.PartnerCards.Hunter.SequenceAndSetup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterPartnerSequenceAndSetupTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHunterPartnerHeavyArrowRuntimeTest;
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.ZhuiLie"), 0, 1000, 1000, 0, Runtime)) return false;
		Runtime.ActiveCardsPlayedThisRound = 2;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Zhui Lie after two cards")))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(Result);
			if (TestNotNull(TEXT("Zhui Lie has one direct packet"), Direct))
			{
				TestEqual(TEXT("Zhui Lie gains fifteen percent for each prior active card"), Direct->BaseRequestedDamage, 10);
			}
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.HuiHuanJian"), 2, 1000, 1000, 0, Runtime)) return false;
		Runtime.ActiveCardsPlayedThisRound = 5;
		const int32 ManaBefore = FindUnit(Runtime, HunterUnitId)->Mana;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Hui Huan after five cards")))
		{
			TestEqual(TEXT("Hui Huan draws one base, one for the completed three-card interval, and two for Charge"), Runtime.Deck.Hand.Num(), 4);
			TestEqual(TEXT("Hui Huan restores two Mana per Charge after paying two"), FindUnit(Runtime, HunterUnitId)->Mana, ManaBefore - 2 + 4);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.LueYingJian"), 2, 1000, 1000, 0, Runtime)) return false;
		Runtime.ActiveCardsPlayedThisRound = 6;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, Result, TEXT("Lue Ying after six cards")))
		{
			TestEqual(TEXT("Lue Ying grants two interval Agility plus two Charge Agility"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Agility), 4);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(
			*this,
			TEXT("Profession.Hunter.FuBu"),
			0,
			1000,
			1000,
			20,
			Runtime,
			{MakeCard(TEXT("Arrow1"), TEXT("Profession.Hunter.ChuanYang"), 100), MakeCard(TEXT("Arrow2"), TEXT("Profession.Hunter.ChuanYang"), 101)})) return false;
		for (const FName ArrowId : {FName(TEXT("Arrow1")), FName(TEXT("Arrow2"))})
		{
			const int32 ArrowIndex = Runtime.Deck.DrawPile.IndexOfByPredicate([ArrowId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == ArrowId;
			});
			if (!TestTrue(FString::Printf(TEXT("%s is present in the deterministic draw pile"), *ArrowId.ToString()), ArrowIndex != INDEX_NONE))
			{
				return false;
			}
			Runtime.Deck.Hand.Add(Runtime.Deck.DrawPile[ArrowIndex]);
			Runtime.Deck.DrawPile.RemoveAt(ArrowIndex, 1, EAllowShrinking::No);
		}
		FGameXXKCardPlayResult EagleEyeResult;
		FString Error;
		if (!TestTrue(TEXT("Eagle Eye resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Subject"), NAME_None, EagleEyeResult, &Error)))
		{
			AddError(Error);
			return false;
		}
		FGameXXKCardPlayResult FirstArrowResult;
		Error.Reset();
		if (TestTrue(TEXT("the empowered Chuan Yang resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Arrow1"), EnemyUnitId, FirstArrowResult, &Error)))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(FirstArrowResult);
			if (TestNotNull(TEXT("the empowered Chuan Yang has one direct packet"), Direct))
			{
				TestEqual(TEXT("Eagle Eye adds six Defense ignore to the next Heavy Arrow"), Direct->DamageAfterDefense, 28);
			}
		}
		else
		{
			AddError(Error);
		}
		FGameXXKCardPlayResult SecondArrowResult;
		Error.Reset();
		if (TestTrue(TEXT("the following unempowered Chuan Yang resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Arrow2"), EnemyUnitId, SecondArrowResult, &Error)))
		{
			const FGameXXKCardDamageResult* Direct = FindDirect(SecondArrowResult);
			if (TestNotNull(TEXT("the unempowered Chuan Yang has one direct packet"), Direct))
			{
				TestEqual(TEXT("Eagle Eye is consumed by exactly one Heavy Arrow"), Direct->DamageAfterDefense, 1);
			}
		}
		else
		{
			AddError(Error);
		}
	}
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Hunter.YinZong"), 0, 1000, 1000, 0, Runtime)) return false;
		FGameXXKCardPlayResult YinZongResult;
		FString Error;
		if (!TestTrue(TEXT("Yin Zong resolves"), GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Subject"), NAME_None, YinZongResult, &Error)))
		{
			AddError(Error);
			return false;
		}
		Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
		Runtime.CombatRandomState = 3;
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyUnitId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		Error.Reset();
		if (TestTrue(TEXT("the deterministic enemy hit resolves"), GameXXKCardRules::ResolveEnemyDirectAttack(
			Runtime, Context, HunterUnitId, 10, Incoming, nullptr, &Error, true)))
		{
			TestTrue(TEXT("seed three produces the required perfect dodge"), Incoming.bPerfectAgilityDodge);
			TestEqual(TEXT("a perfect dodge consumes one Agility"), Incoming.AgilityStacksConsumed, 1);
			TestEqual(TEXT("Yin Zong grants two Charge only when the perfect dodge succeeds"), Status(Runtime, HunterUnitId, EGameXXKCardStatus::Charge), 3);
		}
		else
		{
			AddError(Error);
		}
	}
	return true;
}

#endif
