#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKBladePartnerSheathedRuntimeTest
{
	constexpr const TCHAR* BladeUnitId = TEXT("BladePartner");
	constexpr const TCHAR* HeroUnitId = TEXT("Hero");
	constexpr const TCHAR* EnemyAUnitId = TEXT("EnemyA");
	constexpr const TCHAR* EnemyBUnitId = TEXT("EnemyB");

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
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Sheathed.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Hand,
		const TArray<FGameXXKCardInstance>& DrawPile)
	{
		TArray<FGameXXKCardInstance> AllCards = Hand;
		AllCards.Append(DrawPile);
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(BladeUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 200, 20, 50, 1),
			MakeUnit(HeroUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Invalid, 200, 20, 50, 2),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 0, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 10, 0, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			AllCards,
			Units,
			EGameXXKCardTerrain::Plain,
			61201,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Sheathed runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Hand;
		OutRuntime.Deck.DrawPile = DrawPile;
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Sheathed exact fixture is invalid: %s"), *Error));
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
		return Test.TestTrue(
			FString::Printf(TEXT("%s resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error));
	}

	bool Preview(
		FAutomationTestBase& Test,
		const FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		FGameXXKCardPlayPreview& OutPreview,
		const TCHAR* Label)
	{
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("%s preview builds: %s"), Label, *Error),
			GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, OutPreview, &Error));
	}

	bool EndPlayerPhase(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("Sheathed player phase ends: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error));
	}

	bool BeginNextPlayerRound(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("Sheathed next player round begins: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error));
	}

	const FGameXXKCardInstance* FindHandCard(const FGameXXKCardBattleRuntime& Runtime, const FName InstanceId)
	{
		return Runtime.Deck.Hand.FindByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJingHongLightLoadRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.JingHongChargeReducesEnergyAndDrawsOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJingHongLightLoadRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.JingHongChuQiao"), BladeUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Reward"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), EnemyAUnitId, Result, TEXT("Jing Hong opener")))
	{
		return true;
	}
	FGameXXKCardPlayPreview NextPreview;
	if (!Preview(*this, Runtime, TEXT("Next"), NextPreview, TEXT("Light Load target")))
	{
		return true;
	}
	TestEqual(TEXT("Light Load lowers the next active Energy cost by one"), NextPreview.EffectiveEnergyCost, 0);
	if (!Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("Light Load target")))
	{
		return true;
	}
	TestNotNull(TEXT("Light Load draws one after the next active resolves"), FindHandCard(Runtime, TEXT("Reward")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHengYunDrawTwoRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.HengYunChargeDrawsTwoAfterNextActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHengYunDrawTwoRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.HengYunKaiFeng"), BladeUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("DrawA"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2),
		MakeCard(TEXT("DrawB"), TEXT("Profession.Blade.DuanYue"), BladeUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), NAME_None, Result, TEXT("Heng Yun opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("Heng Yun draw target")))
	{
		return true;
	}
	TestNotNull(TEXT("Heng Yun draws the first queued card"), FindHandCard(Runtime, TEXT("DrawA")));
	TestNotNull(TEXT("Heng Yun draws the second queued card"), FindHandCard(Runtime, TEXT("DrawB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLianXiSameOwnerDrawRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.LianXiChargeDrawsSameOwnerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLianXiSameOwnerDrawRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.LianXiGuiQiao"), BladeUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Eligible"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2),
		MakeCard(TEXT("Decoy"), TEXT("Profession.Blade.DuanYue"), BladeUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), NAME_None, Result, TEXT("Lian Xi opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("same-owner target")))
	{
		return true;
	}
	TestNotNull(TEXT("Same Owner selects the next active character's card"), FindHandCard(Runtime, TEXT("Eligible")));
	TestNull(TEXT("Same Owner leaves a different character's top card in the draw pile"), FindHandCard(Runtime, TEXT("Decoy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBaoDaoOtherOwnerDrawRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.BaoDaoChargeDrawsOtherOwnerCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBaoDaoOtherOwnerDrawRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Charge"), TEXT("Profession.Blade.BaoDaoShouYe"), BladeUnitId, 0),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Eligible"), TEXT("Profession.Blade.DuanYue"), BladeUnitId, 2),
		MakeCard(TEXT("Decoy"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Charge"), NAME_None, Result, TEXT("Bao Dao opener"))
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("other-owner target")))
	{
		return true;
	}
	TestNotNull(TEXT("Other Owner selects another character's card"), FindHandCard(Runtime, TEXT("Eligible")));
	TestNull(TEXT("Other Owner leaves the same character's top card in the draw pile"), FindHandCard(Runtime, TEXT("Decoy")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSheathedNonOpeningFinishStoresStyleRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.NonOpeningFinisherStoresItsChargeAsNativeStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSheathedNonOpeningFinishStoresStyleRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("First"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 0),
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.LianXiGuiQiao"), BladeUnitId, 1)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Eligible"), TEXT("Hero.Generic.SuiYanJi"), HeroUnitId, 2),
		MakeCard(TEXT("Next"), TEXT("Hero.Generic.QingFengYiShi"), HeroUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("First"), EnemyAUnitId, Result, TEXT("non-Blade first active"))
		|| !Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("non-opening Lian Xi finisher")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Next"), EnemyAUnitId, Result, TEXT("native Same Owner target")))
	{
		return true;
	}
	TestNotNull(TEXT("a non-opening Sheathed finisher stores its Charge for next round"), FindHandCard(Runtime, TEXT("Eligible")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKUnusedNativeStyleExpiresRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.UnusedNativeStyleExpiresAtRoundEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKUnusedNativeStyleExpiresRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.LianXiGuiQiao"), BladeUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Probe"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("style finisher")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !EndPlayerPhase(*this, Runtime))
	{
		return true;
	}
	const int32 FinisherIndex = Runtime.Deck.DiscardPile.IndexOfByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == TEXT("Finisher");
	});
	if (!TestTrue(TEXT("the old finisher remains available for fixture isolation"), FinisherIndex != INDEX_NONE))
	{
		return true;
	}
	Runtime.Deck.ExhaustPile.Add(MoveTemp(Runtime.Deck.DiscardPile[FinisherIndex]));
	Runtime.Deck.DiscardPile.RemoveAt(FinisherIndex, 1, EAllowShrinking::No);
	if (!BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Probe"), EnemyAUnitId, Result, TEXT("post-expiry first active")))
	{
		return true;
	}
	TestEqual(TEXT("an unused native style cannot draw after its owning round ends"), Runtime.Deck.Hand.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKJingHongOpenBladeRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.JingHongOpenAddsExactlyOneAttack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKJingHongOpenBladeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.LianXiGuiQiao"), BladeUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Open"), TEXT("Profession.Blade.JingHongChuQiao"), BladeUnitId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), NAME_None, Result, TEXT("stored-style finisher")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 1;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime)
		|| !Resolve(*this, Runtime, TEXT("Open"), EnemyAUnitId, Result, TEXT("Jing Hong Open Blade")))
	{
		return true;
	}
	int32 EnemyAHits = 0;
	for (const FGameXXKCardDamageResult& Damage : Result.DamageResults)
	{
		EnemyAHits += Damage.ResolvedTargetUnitId == EnemyAUnitId
			&& Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::ActivePlay
			? 1
			: 0;
	}
	TestEqual(TEXT("Jing Hong consumes one style and adds exactly one 90-percent attack"), EnemyAHits, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHengYunResidualStyleRuntimeTest,
	"GameXXK.Data.PartnerCards.BladeRuntime.Sheathed.HengYunResidualStyleAppliesOnceWithoutChaining",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHengYunResidualStyleRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBladePartnerSheathedRuntimeTest;
	const TArray<FGameXXKCardInstance> Hand = {
		MakeCard(TEXT("Finisher"), TEXT("Profession.Blade.JingHongChuQiao"), BladeUnitId, 0)};
	const TArray<FGameXXKCardInstance> Draw = {
		MakeCard(TEXT("Third"), TEXT("Hero.Generic.HeYuZhan"), HeroUnitId, 1),
		MakeCard(TEXT("HengYunB"), TEXT("Profession.Blade.HengYunKaiFeng"), BladeUnitId, 2),
		MakeCard(TEXT("HengYunA"), TEXT("Profession.Blade.HengYunKaiFeng"), BladeUnitId, 3)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Hand, Draw))
	{
		return false;
	}
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Finisher"), EnemyAUnitId, Result, TEXT("Light Load style finisher")))
	{
		return true;
	}
	Runtime.Deck.HandLimit = 3;
	if (!EndPlayerPhase(*this, Runtime) || !BeginNextPlayerRound(*this, Runtime))
	{
		return true;
	}
	FGameXXKCardPlayPreview PreviewA;
	if (!Preview(*this, Runtime, TEXT("HengYunA"), PreviewA, TEXT("native-style Heng Yun")))
	{
		return true;
	}
	TestEqual(TEXT("the stored Light Load reduces the first Heng Yun"), PreviewA.EffectiveEnergyCost, 1);
	if (!Resolve(*this, Runtime, TEXT("HengYunA"), NAME_None, Result, TEXT("native-style Heng Yun")))
	{
		return true;
	}
	FGameXXKCardPlayPreview PreviewB;
	if (!Preview(*this, Runtime, TEXT("HengYunB"), PreviewB, TEXT("residual-style Heng Yun")))
	{
		return true;
	}
	TestEqual(TEXT("Heng Yun copies the consumed native Light Load once"), PreviewB.EffectiveEnergyCost, 1);
	if (!Resolve(*this, Runtime, TEXT("HengYunB"), NAME_None, Result, TEXT("residual-style Heng Yun")))
	{
		return true;
	}
	FGameXXKCardPlayPreview ThirdPreview;
	if (!Preview(*this, Runtime, TEXT("Third"), ThirdPreview, TEXT("post-residual third active")))
	{
		return true;
	}
	TestEqual(TEXT("a residual style cannot copy itself onto the third active"), ThirdPreview.EffectiveEnergyCost, 1);
	return true;
}

#endif
