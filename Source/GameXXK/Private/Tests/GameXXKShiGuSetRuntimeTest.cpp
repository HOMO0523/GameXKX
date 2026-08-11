#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentSetCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKShiGuSetRuntimeTest
{
	constexpr uint8 ShiGuDotAppliedHookValue = 18;
	constexpr uint8 ShiGuDualDotHookValue = 19;
	constexpr uint8 ShiGuToxicExplosionHookValue = 20;
	constexpr uint8 ShiGuModifierKindValue = 40;

	constexpr EGameXXKEquipmentSlot OrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	EGameXXKEquipmentSetBonusHook ExpectedHook(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusHook>(
			Pieces == 2 ? ShiGuDotAppliedHookValue : Pieces == 4 ? ShiGuDualDotHookValue : ShiGuToxicExplosionHookValue);
	}

	EGameXXKEquipmentSetBonusKind ExpectedBonusKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 25 : Pieces == 4 ? 26 : 27);
	}

	EGameXXKEquipmentModifierKind ExpectedModifierKind()
	{
		return static_cast<EGameXXKEquipmentModifierKind>(ShiGuModifierKindValue);
	}

	const TCHAR* ExpectedDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("每张牌首次对一个目标施加流血、中毒或灼烧时，施加1层蚀伤。");
		case 4: return TEXT("每回合首次使目标同时具有至少2种流血、中毒或灼烧时，自动毒爆1次。");
		case 6: return TEXT("每回合首次毒爆不减少流血、中毒和灼烧层数。");
		default: return TEXT("");
		}
	}

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party
			? EGameXXKCharacterRole::Hero
			: EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 200 : 1000;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
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
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = TEXT("Hero");
		Card.SourceEntryId = FName(*FString::Printf(TEXT("ShiGu.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	FGameXXKEquipmentBattleEffectRuntime* FindEffect(FGameXXKCardBattleRuntime& Runtime, const int32 Pieces)
	{
		return Runtime.EquipmentEffects.FindByPredicate([Pieces](const FGameXXKEquipmentBattleEffectRuntime& Candidate)
		{
			return Candidate.ActiveEffect.Set == EGameXXKEquipmentSet::ShiGu
				&& Candidate.ActiveEffect.RequiredPieces == Pieces;
		});
	}

	bool AddShiGuEffects(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const int32 Pieces,
		const bool bOnlyExactTier = false)
	{
		FGameXXKEquipmentCollectionState Collection;
		Collection.CollectionSeed = 0x534847;
		FGameXXKCompanionRosterState EmptyRoster;
		for (int32 Index = 0; Index < Pieces; ++Index)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::ShiGu;
			Request.Quality = EGameXXKEquipmentQuality::Common;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = OrderedSlots[Index];
			FName InstanceId;
			FString Error;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, InstanceId, &Error))
			{
				Test.AddError(FString::Printf(TEXT("ShiGu fixture item creation failed: %s"), *Error));
				return false;
			}
			const FGameXXKEquipmentTransactionResult EquipResult = FGameXXKEquipmentRules::EquipInstance(
				Collection,
				EmptyRoster,
				FGameXXKEquipmentRules::HeroCharacterId(),
				OrderedSlots[Index],
				InstanceId);
			if (!EquipResult.bSucceeded)
			{
				Test.AddError(FString::Printf(TEXT("ShiGu fixture equip failed: %s"), *EquipResult.Message.ToString()));
				return false;
			}
		}

		FGameXXKCharacterStats BareStats;
		BareStats.MaxHealth = 200;
		BareStats.MaxMana = 100;
		BareStats.Attack = 10;
		BareStats.Defense = 0;
		BareStats.Speed = 1;
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		FString Error;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			Collection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			BareStats,
			Snapshot,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("ShiGu fixture projection failed: %s"), *Error));
			return false;
		}

		int32 Added = 0;
		for (const FGameXXKEquipmentActiveEffect& Effect : Snapshot.ActivePersonalEffects)
		{
			if (Effect.Set != EGameXXKEquipmentSet::ShiGu
				|| Effect.RequiredPieces <= 0
				|| (bOnlyExactTier && Effect.RequiredPieces != Pieces))
			{
				continue;
			}
			FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = Runtime.EquipmentEffects.AddDefaulted_GetRef();
			RuntimeEffect.ActiveEffect = Effect;
			RuntimeEffect.ActiveEffect.SourceCharacterId = TEXT("Hero");
			RuntimeEffect.SourceCharacterId = TEXT("Hero");
			++Added;
		}
		return Test.TestEqual(
			TEXT("ShiGu fixture materializes the requested thresholds"),
			Added,
			bOnlyExactTier ? 1 : Pieces / 2);
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TArray<FGameXXKCardInstance>& Hand,
		const int32 Pieces,
		FGameXXKCardBattleRuntime& OutRuntime,
		const int32 EnemyCount = 1,
		const bool bOnlyExactTier = false)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 1)};
		for (int32 EnemyIndex = 0; EnemyIndex < EnemyCount; ++EnemyIndex)
		{
			Units.Add(MakeUnit(
				*FString::Printf(TEXT("Enemy%d"), EnemyIndex + 1),
				EGameXXKCardTargetSide::Enemy,
				10 + EnemyIndex));
		}
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Hand,
			Units,
			EGameXXKCardTerrain::Plain,
			71107 + Pieces + EnemyCount,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("ShiGu runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Hand;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		return AddShiGuEffects(Test, OutRuntime, Pieces, bOnlyExactTier);
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Label)
	{
		FString Error;
		OutResult = FGameXXKCardPlayResult();
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(Label, bResolved);
		if (!bResolved)
		{
			Test.AddError(Error);
		}
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShiGuCatalogContractTest,
	"GameXXK.Equipment.ShiGu.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShiGuCatalogContractTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShiGuSetRuntimeTest;
	for (const int32 Pieces : {2, 4, 6})
	{
		const FName Id(*FString::Printf(TEXT("Set.ShiGu.%d"), Pieces));
		const FGameXXKEquipmentSetBonusDefinition* Definition = FGameXXKEquipmentSetCatalog::FindDefinition(Id);
		TestNotNull(FString::Printf(TEXT("%s resolves"), *Id.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(TEXT("ShiGu uses its redesigned serialized kind"), Definition->BonusKind, ExpectedBonusKind(Pieces));
		TestEqual(TEXT("ShiGu is independently owned by each wearer"), Definition->Scope, EGameXXKEquipmentSetBonusScope::Owner);
		TestEqual(TEXT("ShiGu exposes the exact event hook"), Definition->Hook, ExpectedHook(Pieces));
		TestEqual(TEXT("ShiGu uses one flat operation payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
		TestEqual(TEXT("ShiGu uses one operation per trigger"), Definition->Value, 1);
		TestEqual(TEXT("only the four- and six-piece tiers are once per round"), Definition->TriggersPerRound, Pieces >= 4 ? 1 : 0);
		TestEqual(TEXT("ShiGu exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedDescription(Pieces)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShiGuTwoPiecePerCardTargetTest,
	"GameXXK.Equipment.ShiGu.TwoPiece.EachTargetGetsOneRotPerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShiGuTwoPiecePerCardTargetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShiGuSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("DualDot"), TEXT("Hero.Healer.BaiCaoJiZhen"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Hand, 2, Runtime, 2))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("DualDot"), NAME_None, Result, TEXT("the group dual-DoT card resolves")))
	{
		return true;
	}
	for (const FName EnemyId : {FName(TEXT("Enemy1")), FName(TEXT("Enemy2"))})
	{
		FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, EnemyId);
		TestNotNull(TEXT("the deterministic enemy remains present"), Enemy);
		if (Enemy)
		{
			TestEqual(TEXT("multiple DoT applications from one card add Rot exactly once per target"),
				GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::DamageOverTime),
				1);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShiGuFourPieceQualifyingTriggerTest,
	"GameXXK.Equipment.ShiGu.FourPiece.FailedAttemptDoesNotSpendFirstDualDot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShiGuFourPieceQualifyingTriggerTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShiGuSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("BleedOnly"), TEXT("Hero.Hunter.LieYuLianShi"), 0),
		MakeCard(TEXT("DualDot"), TEXT("Hero.Healer.BaiCaoJiZhen"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Hand, 4, Runtime))
	{
		return false;
	}
	FGameXXKCardPlayResult FirstResult;
	if (!Resolve(*this, Runtime, TEXT("BleedOnly"), TEXT("Enemy1"), FirstResult, TEXT("the single-DoT probe resolves")))
	{
		return true;
	}
	FGameXXKEquipmentBattleEffectRuntime* FourPiece = FindEffect(Runtime, 4);
	TestNotNull(TEXT("the four-piece runtime exists"), FourPiece);
	if (FourPiece)
	{
		TestEqual(TEXT("a card that leaves only one DoT type does not spend the trigger"), FourPiece->CurrentRoundTriggerCount, 0);
	}
	TestEqual(TEXT("the failed qualifying attempt produces no Toxic Explosion"), FirstResult.ToxicExplosionDistinctDotTypeCounts.Num(), 0);

	FGameXXKCardPlayResult SecondResult;
	if (!Resolve(*this, Runtime, TEXT("DualDot"), NAME_None, SecondResult, TEXT("the later qualifying dual-DoT card resolves")))
	{
		return true;
	}
	TestEqual(TEXT("the first real qualifying application automatically explodes once"), SecondResult.ToxicExplosionDistinctDotTypeCounts.Num(), 1);
	if (SecondResult.ToxicExplosionDistinctDotTypeCounts.Num() == 1)
	{
		TestEqual(TEXT("the automatic Toxic Explosion snapshots all three present DoT types"), SecondResult.ToxicExplosionDistinctDotTypeCounts[0], 3);
	}
	FourPiece = FindEffect(Runtime, 4);
	if (FourPiece)
	{
		TestEqual(TEXT("the four-piece trigger is consumed only by the qualifying card"), FourPiece->CurrentRoundTriggerCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShiGuSixPieceFirstExplosionTest,
	"GameXXK.Equipment.ShiGu.SixPiece.OnlyFirstExplosionPreservesAllThreeDots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShiGuSixPieceFirstExplosionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShiGuSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("ExplosionA"), TEXT("Hero.Hunter.CuiDuChuanXin"), 0),
		MakeCard(TEXT("ExplosionB"), TEXT("Hero.Hunter.CuiDuChuanXin"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Hand, 6, Runtime, 1, true))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, TEXT("Enemy1"));
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Bleed, 3);
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Poison, 2);
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Burn, 4);

	FGameXXKCardPlayResult FirstResult;
	if (!Resolve(*this, Runtime, TEXT("ExplosionA"), TEXT("Enemy1"), FirstResult, TEXT("the first Toxic Explosion card resolves")))
	{
		return true;
	}
	Enemy = FindUnit(Runtime, TEXT("Enemy1"));
	TestEqual(TEXT("the first Toxic Explosion preserves Bleed after the direct hit spends its normal one stack"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed), 2);
	TestEqual(TEXT("the first Toxic Explosion preserves the post-application Poison snapshot"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Poison), 8);
	TestEqual(TEXT("the first Toxic Explosion preserves Burn"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 4);

	FGameXXKCardPlayResult SecondResult;
	if (!Resolve(*this, Runtime, TEXT("ExplosionB"), TEXT("Enemy1"), SecondResult, TEXT("the second Toxic Explosion card resolves")))
	{
		return true;
	}
	Enemy = FindUnit(Runtime, TEXT("Enemy1"));
	TestEqual(TEXT("the second direct hit and non-preserved Toxic Explosion each consume one Bleed"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("the second same-round Toxic Explosion consumes one Poison after adding six"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Poison), 13);
	TestEqual(TEXT("the second same-round Toxic Explosion consumes one Burn"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 3);
	FGameXXKEquipmentBattleEffectRuntime* SixPiece = FindEffect(Runtime, 6);
	TestNotNull(TEXT("the six-piece runtime exists"), SixPiece);
	if (SixPiece)
	{
		TestEqual(TEXT("the six-piece preservation budget is consumed once"), SixPiece->CurrentRoundTriggerCount, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShiGuFullTierInteractionTest,
	"GameXXK.Equipment.ShiGu.FullTier.AutoExplosionUsesFirstPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShiGuFullTierInteractionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShiGuSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("DualDot"), TEXT("Hero.Healer.BaiCaoJiZhen"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Hand, 6, Runtime))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("DualDot"), NAME_None, Result, TEXT("the full-tier dual-DoT card resolves")))
	{
		return true;
	}
	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime, TEXT("Enemy1"));
	TestEqual(TEXT("two-piece adds Rot before the automatic explosion"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::DamageOverTime), 1);
	TestEqual(TEXT("six-piece preserves Poison on the four-piece automatic explosion"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Poison), 1);
	TestEqual(TEXT("six-piece preserves Burn on the four-piece automatic explosion"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("the automatic explosion resolves exactly once"), Result.ToxicExplosionDistinctDotTypeCounts.Num(), 1);
	return true;
}

#endif
