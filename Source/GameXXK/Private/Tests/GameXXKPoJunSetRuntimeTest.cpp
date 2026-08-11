#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKPoJunSetRuntimeTest
{
	constexpr const TCHAR* BladeAUnitId = TEXT("BladeA");
	constexpr const TCHAR* BladeBUnitId = TEXT("BladeB");
	constexpr const TCHAR* HeroUnitId = TEXT("Hero");
	constexpr const TCHAR* EnemyUnitId = TEXT("Enemy");

	constexpr EGameXXKEquipmentSlot OrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 Mana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = Mana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const TCHAR* OwnerUnitId,
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = FName(OwnerUnitId);
		Card.SourceEntryId = FName(*FString::Printf(TEXT("PoJun.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool AddPoJunEffects(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName SourceUnitId,
		const int32 Pieces)
	{
		if (Pieces != 2 && Pieces != 4 && Pieces != 6)
		{
			Test.AddError(TEXT("PoJun fixture requires exactly 2, 4, or 6 pieces."));
			return false;
		}
		FGameXXKEquipmentCollectionState Collection;
		Collection.CollectionSeed = 0x50204A;
		FGameXXKCompanionRosterState EmptyRoster;
		for (int32 Index = 0; Index < Pieces; ++Index)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::PoJun;
			Request.Quality = EGameXXKEquipmentQuality::Common;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = OrderedSlots[Index];
			FName InstanceId;
			FString Error;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, InstanceId, &Error))
			{
				Test.AddError(FString::Printf(TEXT("PoJun fixture item creation failed: %s"), *Error));
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
				Test.AddError(FString::Printf(TEXT("PoJun fixture equip failed: %s"), *EquipResult.Message.ToString()));
				return false;
			}
		}

		FGameXXKCharacterStats BareStats;
		BareStats.MaxHealth = 100;
		BareStats.MaxMana = 50;
		BareStats.Attack = 20;
		BareStats.Defense = 0;
		BareStats.Speed = 0;
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		FString Error;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			Collection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			BareStats,
			Snapshot,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("PoJun fixture projection failed: %s"), *Error));
			return false;
		}

		int32 Added = 0;
		for (const FGameXXKEquipmentActiveEffect& Effect : Snapshot.ActivePersonalEffects)
		{
			if (Effect.Set != EGameXXKEquipmentSet::PoJun
				|| Effect.RequiredPieces <= 0
				|| !Effect.EffectId.ToString().StartsWith(TEXT("Set.PoJun.")))
			{
				continue;
			}
			FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = Runtime.EquipmentEffects.AddDefaulted_GetRef();
			RuntimeEffect.ActiveEffect = Effect;
			RuntimeEffect.ActiveEffect.SourceCharacterId = SourceUnitId;
			RuntimeEffect.SourceCharacterId = SourceUnitId;
			++Added;
		}
		return Test.TestEqual(TEXT("PoJun fixture materializes every reached threshold exactly once"), Added, Pieces / 2);
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Hand,
		const TArray<FGameXXKCardInstance>& DrawPile,
		const TArray<TPair<FName, int32>>& PoJunWearers)
	{
		TArray<FGameXXKCardInstance> AllCards = Hand;
		AllCards.Append(DrawPile);
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeAUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 300, 20, 50, 1),
			MakeUnit(BladeBUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 300, 20, 50, 2),
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 300, 20, 50, 3),
			MakeUnit(EnemyUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 5000, 10, 0, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			AllCards,
			Units,
			EGameXXKCardTerrain::Plain,
			70611,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("PoJun runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Hand;
		OutRuntime.Deck.DrawPile = DrawPile;
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		for (const TPair<FName, int32>& Wearer : PoJunWearers)
		{
			if (!AddPoJunEffects(Test, OutRuntime, Wearer.Key, Wearer.Value))
			{
				return false;
			}
		}
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("PoJun exact fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
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

	bool EndPlayerPhase(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		const bool bEnded = GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error);
		Test.TestTrue(TEXT("PoJun player phase ends"), bEnded);
		if (!bEnded)
		{
			Test.AddError(Error);
		}
		return bEnded;
	}

	bool BeginNextPlayerRound(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		const bool bBegan = GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error);
		Test.TestTrue(TEXT("PoJun next player round begins"), bBegan);
		if (!bBegan)
		{
			Test.AddError(Error);
		}
		return bBegan;
	}

	const FGameXXKCardInstance* FindHandCard(const FGameXXKCardBattleRuntime& Runtime, const FName InstanceId)
	{
		return Runtime.Deck.Hand.FindByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
	}

	int32 CountEnemyDamagePackets(const FGameXXKCardPlayResult& Result)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Damage : Result.DamageResults)
		{
			Count += Damage.ResolvedTargetUnitId == EnemyUnitId ? 1 : 0;
		}
		return Count;
	}

	FGameXXKEquipmentBattleEffectRuntime* FindPoJunEffect(
		FGameXXKCardBattleRuntime& Runtime,
		const FName SourceUnitId,
		const EGameXXKEquipmentSetBonusHook Hook)
	{
		return Runtime.EquipmentEffects.FindByPredicate([SourceUnitId, Hook](const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime)
		{
			return EffectRuntime.ActiveEffect.Set == EGameXXKEquipmentSet::PoJun
				&& EffectRuntime.SourceCharacterId == SourceUnitId
				&& EffectRuntime.ActiveEffect.Hook == Hook;
		});
	}

	FName TargetForBladeCard(const FGameXXKCardDefinition& Definition)
	{
		return Definition.TargetSpec.Mode == EGameXXKCardTargetMode::SingleEnemy
			? FName(EnemyUnitId)
			: NAME_None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunTwoPieceConsumptionRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.TwoPiece.DrawsOnlyAfterRealConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunTwoPieceConsumptionRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Reward"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 2}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("PoJun opener resolves")))
	{
		return true;
	}
	FString Error;
	FGameXXKCardPlayResult FailedResult;
	TestFalse(TEXT("an illegal target rejects the would-be consumer"),
		GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Next"), NAME_None, FailedResult, &Error));
	TestNull(TEXT("a rejected play cannot trigger PoJun draw"), FindHandCard(Runtime, TEXT("Reward")));
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("PoJun consumer resolves")))
	{
		return true;
	}
	TestNotNull(TEXT("real Charge consumption triggers the two-piece draw"), FindHandCard(Runtime, TEXT("Reward")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunTwoPieceAutomaticIsolationRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.TwoPiece.AutomaticReplayDoesNotConsume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunTwoPieceAutomaticIsolationRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("Manual"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Reward"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 2}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("automatic-isolation opener resolves")))
	{
		return true;
	}
	FGameXXKResolvedCardSnapshot AutomaticSnapshot;
	AutomaticSnapshot.CardId = TEXT("Hero.Generic.HeYuZhan");
	AutomaticSnapshot.Quality = EGameXXKCardQuality::Common;
	AutomaticSnapshot.OwnerUnitId = HeroUnitId;
	AutomaticSnapshot.OriginalTargetUnitIds = {FName(EnemyUnitId)};
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {AutomaticSnapshot};
	Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
	TArray<FGameXXKCardPlayResult> AutomaticResults;
	FString Error;
	TestTrue(TEXT("fixture automatic replay resolves"),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, AutomaticResults, &Error));
	TestEqual(TEXT("automatic replay leaves the pending Charge intact"), Runtime.PendingBladeCharge.Rule, EGameXXKBladeChargeRule::MakeNextActiveEnergyFree);
	TestNull(TEXT("automatic replay cannot trigger PoJun draw"), FindHandCard(Runtime, TEXT("Reward")));
	if (!Resolve(*this, Runtime, TEXT("Manual"), EnemyUnitId, Result, TEXT("manual consumer resolves after automatic replay")))
	{
		return true;
	}
	TestNotNull(TEXT("the later manual consumer triggers PoJun draw"), FindHandCard(Runtime, TEXT("Reward")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunTwoPieceOncePerRoundRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.TwoPiece.OncePerWearerPerRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunTwoPieceOncePerRoundRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("FirstConsumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("SecondConsumer"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("RewardB"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 3),
		MakeCard(TEXT("RewardA"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 2}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("once-per-round opener resolves"))
		|| !Resolve(*this, Runtime, TEXT("FirstConsumer"), EnemyUnitId, Result, TEXT("first Charge consumer resolves")))
	{
		return true;
	}
	TestNotNull(TEXT("the first consumption draws RewardA"), FindHandCard(Runtime, TEXT("RewardA")));
	FGameXXKBladeChargeRuntime& InjectedCharge = Runtime.PendingBladeCharge;
	InjectedCharge.Rule = EGameXXKBladeChargeRule::MakeNextActiveEnergyFree;
	InjectedCharge.SourceCardId = TEXT("Profession.Blade.DuanYue");
	InjectedCharge.SourceQuality = EGameXXKCardQuality::Common;
	InjectedCharge.SourceOwnerUnitId = BladeAUnitId;
	InjectedCharge.CreatedRound = Runtime.RoundNumber;
	if (!Resolve(*this, Runtime, TEXT("SecondConsumer"), EnemyUnitId, Result, TEXT("second same-round Charge consumer resolves")))
	{
		return true;
	}
	TestNull(TEXT("the same wearer cannot trigger PoJun draw twice in one round"), FindHandCard(Runtime, TEXT("RewardB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunFourPieceStoredChargeRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.FourPiece.FinishStoresChargeForNextRound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunFourPieceStoredChargeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.ZhanYiFeiTeng"), BladeAUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 4}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("PoJun four-piece finisher resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("stored Refund Charge target resolves")))
	{
		return true;
	}
	TestEqual(TEXT("the stored Refund Charge restores the next card's paid Energy"), Runtime.Deck.SharedEnergy, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunFourPieceNativeCoexistenceRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.FourPiece.NativeAndPoJunStylesBothResolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunFourPieceNativeCoexistenceRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.LianXiGuiQiao"), BladeAUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("RewardB"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 1),
		MakeCard(TEXT("RewardA"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 2),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 4}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("native-plus-PoJun finisher resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("dual-style target resolves")))
	{
		return true;
	}
	TestNotNull(TEXT("native style draws one same-owner card"), FindHandCard(Runtime, TEXT("RewardA")));
	TestNotNull(TEXT("PoJun style independently draws the second same-owner card"), FindHandCard(Runtime, TEXT("RewardB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunFourPieceExpiryRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.FourPiece.UnusedStyleExpires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunFourPieceExpiryRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.ZhanYiFeiTeng"), BladeAUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Probe"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("Skip"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 4}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("expiring PoJun finisher resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Probe"), EnemyUnitId, Result, TEXT("post-expiry probe resolves")))
	{
		return true;
	}
	TestEqual(TEXT("an unused stored Refund Charge cannot survive its owning round"), Runtime.Deck.SharedEnergy, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunSixPieceLoopRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.SixPiece.SameWearerLoopReplaysBaseOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunSixPieceLoopRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.BaoDaoShouYe"), BladeAUnitId, 2)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("BladeRewardB"), TEXT("Profession.Blade.FengHou"), BladeAUnitId, 3),
		MakeCard(TEXT("BladeRewardA"), TEXT("Profession.Blade.LangDuan"), BladeAUnitId, 4),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 5),
		MakeCard(TEXT("TwoPieceReward"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 6)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 6}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("six-piece opener resolves"))
		|| !Resolve(*this, Runtime, TEXT("Consumer"), EnemyUnitId, Result, TEXT("six-piece Charge consumer resolves"))
		|| !Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("six-piece Finish resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("six-piece next-round first active resolves")))
	{
		return true;
	}
	TestEqual(TEXT("the next active card resolves one base hit plus one PoJun replay"), CountEnemyDamagePackets(Result), 2);
	TestEqual(TEXT("the PoJun replay never increments active-card count"), Runtime.ActiveCardsPlayedThisRound, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunSixPieceSourceIsolationRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.SixPiece.WearerSourcesAreIsolated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunSixPieceSourceIsolationRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("ChargeA"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("FinishB"), TEXT("Profession.Blade.BaoDaoShouYe"), BladeBUnitId, 2)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("BladeRewardB"), TEXT("Profession.Blade.FengHou"), BladeBUnitId, 3),
		MakeCard(TEXT("BladeRewardA"), TEXT("Profession.Blade.LangDuan"), BladeAUnitId, 4),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 5)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeBUnitId), 6}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ChargeA"), EnemyUnitId, Result, TEXT("non-wearer opener resolves"))
		|| !Resolve(*this, Runtime, TEXT("Consumer"), EnemyUnitId, Result, TEXT("non-wearer Charge consumer resolves"))
		|| !Resolve(*this, Runtime, TEXT("FinishB"), NAME_None, Result, TEXT("wearer Finish resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyUnitId, Result, TEXT("source-isolation probe resolves")))
	{
		return true;
	}
	TestEqual(TEXT("wearer B cannot complete a loop from wearer A's Charge"), CountEnemyDamagePackets(Result), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunTwoPieceIndependentWearersRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.TwoPiece.EachWearerOwnsAnIndependentRoundBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunTwoPieceIndependentWearersRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("ChargeA"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("FirstConsumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("SecondConsumer"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("RewardB"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 3),
		MakeCard(TEXT("RewardA"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 2}, {FName(BladeBUnitId), 2}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ChargeA"), EnemyUnitId, Result, TEXT("wearer A opener resolves"))
		|| !Resolve(*this, Runtime, TEXT("FirstConsumer"), EnemyUnitId, Result, TEXT("wearer A Charge is consumed")))
	{
		return true;
	}
	TestNotNull(TEXT("wearer A independently draws its reward"), FindHandCard(Runtime, TEXT("RewardA")));

	FGameXXKBladeChargeRuntime& InjectedCharge = Runtime.PendingBladeCharge;
	InjectedCharge.Rule = EGameXXKBladeChargeRule::MakeNextActiveEnergyFree;
	InjectedCharge.SourceCardId = TEXT("Profession.Blade.DuanYue");
	InjectedCharge.SourceQuality = EGameXXKCardQuality::Common;
	InjectedCharge.SourceOwnerUnitId = BladeBUnitId;
	InjectedCharge.CreatedRound = Runtime.RoundNumber;
	if (!Resolve(*this, Runtime, TEXT("SecondConsumer"), EnemyUnitId, Result, TEXT("wearer B Charge is consumed in the same round")))
	{
		return true;
	}
	TestNotNull(TEXT("wearer B still owns a separate same-round draw budget"), FindHandCard(Runtime, TEXT("RewardB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunSixPieceRequiresBothHalvesRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.SixPiece.ChargeOnlyOrFinishOnlyCannotScheduleReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunSixPieceRequiresBothHalvesRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	{
		const TArray<FGameXXKCardInstance> Hand = {
			MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
			MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
		const TArray<FGameXXKCardInstance> Draw = {
			MakeCard(TEXT("Reward"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 2)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 6}}))
		{
			return false;
		}
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("Charge-only opener resolves"))
			|| !Resolve(*this, Runtime, TEXT("Consumer"), EnemyUnitId, Result, TEXT("Charge-only consumer resolves"))
			|| !EndPlayerPhase(*this, Runtime))
		{
			return true;
		}
		FGameXXKEquipmentBattleEffectRuntime* SixPiece = FindPoJunEffect(
			Runtime,
			BladeAUnitId,
			EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound);
		TestNotNull(TEXT("Charge-only fixture retains its six-piece descriptor"), SixPiece);
		if (SixPiece)
		{
			TestEqual(TEXT("consuming Charge without a wearer Finish schedules no replay"), SixPiece->PendingPoJunReplayPlayerRound, 0);
		}
	}

	{
		const TArray<FGameXXKCardInstance> Hand = {
			MakeCard(TEXT("Finish"), TEXT("Profession.Blade.BaoDaoShouYe"), BladeAUnitId, 0)};
		const TArray<FGameXXKCardInstance> Draw = {
			MakeCard(TEXT("Probe"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 6}}))
		{
			return false;
		}
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("Finish"), NAME_None, Result, TEXT("Finish-only card resolves"))
			|| !EndPlayerPhase(*this, Runtime))
		{
			return true;
		}
		FGameXXKEquipmentBattleEffectRuntime* SixPiece = FindPoJunEffect(
			Runtime,
			BladeAUnitId,
			EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound);
		TestNotNull(TEXT("Finish-only fixture retains its six-piece descriptor"), SixPiece);
		if (SixPiece)
		{
			TestEqual(TEXT("a sole Finish cannot consume its own newly armed Charge"), SixPiece->PendingPoJunReplayPlayerRound, 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunSixPieceExpiryRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.SixPiece.UnusedOpeningReplayExpires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunSixPieceExpiryRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, 0),
		MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.BaoDaoShouYe"), BladeAUnitId, 2)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Probe"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 3),
		MakeCard(TEXT("Reward"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 6}}))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyUnitId, Result, TEXT("expiring replay opener resolves"))
		|| !Resolve(*this, Runtime, TEXT("Consumer"), EnemyUnitId, Result, TEXT("expiring replay consumer resolves"))
		|| !Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("expiring replay finisher resolves")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime))
	{
		return true;
	}
	FGameXXKEquipmentBattleEffectRuntime* SixPiece = FindPoJunEffect(
		Runtime,
		BladeAUnitId,
		EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound);
	TestNotNull(TEXT("expiry fixture retains its six-piece descriptor"), SixPiece);
	if (SixPiece)
	{
		TestEqual(TEXT("an unused next-round replay is cleared at that round end"), SixPiece->PendingPoJunReplayPlayerRound, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPoJunAllBladeFinishMatrixRuntimeTest,
	"GameXXK.Equipment.PoJunRuntime.Matrix.AllEighteenBladeCardsStoreChargeAndCompleteLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPoJunAllBladeFinishMatrixRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPoJunSetRuntimeTest;
	const TArray<FName> BladeCardIds = {
		TEXT("Profession.Blade.LieFengZhan"), TEXT("Profession.Blade.HuiFengJiaShi"),
		TEXT("Profession.Blade.FengHou"), TEXT("Profession.Blade.JiYuLianZhan"),
		TEXT("Profession.Blade.YinXueDao"), TEXT("Profession.Blade.LangDuan"),
		TEXT("Profession.Blade.DuanYue"), TEXT("Profession.Blade.PoJun"),
		TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("Profession.Blade.ZhanJin"),
		TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("Profession.Blade.ZhuYing"),
		TEXT("Profession.Blade.PoLangTuJin"), TEXT("Profession.Blade.YiShiDuanJiang"),
		TEXT("Profession.Blade.JingHongChuQiao"), TEXT("Profession.Blade.HengYunKaiFeng"),
		TEXT("Profession.Blade.LianXiGuiQiao"), TEXT("Profession.Blade.BaoDaoShouYe")};
	TestEqual(TEXT("PoJun compatibility matrix contains all eighteen Blade cards"), BladeCardIds.Num(), 18);

	for (int32 Index = 0; Index < BladeCardIds.Num(); ++Index)
	{
		const FName BladeCardId = BladeCardIds[Index];
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(BladeCardId);
		const FString Label = FString::Printf(TEXT("PoJun matrix [%s]"), *BladeCardId.ToString());
		TestNotNull(*FString::Printf(TEXT("%s definition exists"), *Label), Definition);
		if (!Definition)
		{
			continue;
		}
		TestNotEqual(*FString::Printf(TEXT("%s has Charge"), *Label), Definition->BladeSequence.ChargeRule, EGameXXKBladeChargeRule::None);
		TestNotEqual(*FString::Printf(TEXT("%s has Finish"), *Label), Definition->BladeSequence.FinishRule, EGameXXKBladeFinishRule::None);

		const TArray<FGameXXKCardInstance> Hand = {
			MakeCard(TEXT("Opener"), TEXT("Profession.Blade.DuanYue"), BladeAUnitId, Index * 10),
			MakeCard(TEXT("Consumer"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, Index * 10 + 1),
			MakeCard(TEXT("Finisher"), *BladeCardId.ToString(), BladeAUnitId, Index * 10 + 2)};
		const TArray<FGameXXKCardInstance> Draw = {
			MakeCard(TEXT("Probe"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, Index * 10 + 3),
			MakeCard(TEXT("TwoPieceReward"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, Index * 10 + 4)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, Hand, Draw, {{FName(BladeAUnitId), 6}}))
		{
			continue;
		}
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("Opener"), EnemyUnitId, Result, *FString::Printf(TEXT("%s opener resolves"), *Label))
			|| !Resolve(*this, Runtime, TEXT("Consumer"), EnemyUnitId, Result, *FString::Printf(TEXT("%s Charge consumer resolves"), *Label))
			|| !Resolve(*this, Runtime, TEXT("Finisher"), TargetForBladeCard(*Definition), Result, *FString::Printf(TEXT("%s Finish candidate resolves"), *Label))
			|| !EndPlayerPhase(*this, Runtime))
		{
			continue;
		}
		FGameXXKEquipmentBattleEffectRuntime* FourPiece = FindPoJunEffect(
			Runtime,
			BladeAUnitId,
			EGameXXKEquipmentSetBonusHook::PoJunBladeFinish);
		FGameXXKEquipmentBattleEffectRuntime* SixPiece = FindPoJunEffect(
			Runtime,
			BladeAUnitId,
			EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound);
		TestNotNull(*FString::Printf(TEXT("%s retains the four-piece descriptor"), *Label), FourPiece);
		TestNotNull(*FString::Printf(TEXT("%s retains the six-piece descriptor"), *Label), SixPiece);
		if (FourPiece)
		{
			TestEqual(*FString::Printf(TEXT("%s stores its own Charge rule"), *Label), FourPiece->PendingPoJunStyle.Rule, Definition->BladeSequence.ChargeRule);
			TestEqual(*FString::Printf(TEXT("%s stores its own card id"), *Label), FourPiece->PendingPoJunStyle.SourceCardId, BladeCardId);
			TestEqual(*FString::Printf(TEXT("%s stores its wearer source"), *Label), FourPiece->PendingPoJunStyle.SourceOwnerUnitId, FName(BladeAUnitId));
		}
		if (SixPiece)
		{
			TestEqual(*FString::Printf(TEXT("%s completes a same-wearer six-piece loop"), *Label), SixPiece->PendingPoJunReplayPlayerRound, Runtime.RoundNumber + 1);
		}
	}
	return true;
}

#endif
