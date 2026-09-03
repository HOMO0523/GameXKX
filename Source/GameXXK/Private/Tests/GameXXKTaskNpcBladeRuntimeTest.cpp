#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcBladeRuntimeTest
{
	const FName TusiId(TEXT("Npc.TusiChief"));
	const FName SongId(TEXT("Npc.SongJinBao"));
	const FName HeroId(TEXT("Hero"));
	const FName EnemyAId(TEXT("Enemy.A"));
	const FName EnemyBId(TEXT("Enemy.B"));

	struct FCardSpec
	{
		const TCHAR* InstanceId;
		const TCHAR* CardId;
		FName OwnerUnitId;
	};

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 5000 : 200;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FCardSpec& Spec, const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(Spec.InstanceId);
		Card.CardId = FName(Spec.CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = Spec.OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("TaskNpc.Blade.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FCardSpec>& Specs,
		const int32 Seed)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < Specs.Num(); ++Index)
		{
			Cards.Add(MakeCard(Specs[Index], Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TusiId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 10, 1),
			MakeUnit(SongId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 12, 2),
			MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 20, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("task-NPC Blade runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("task-NPC Blade fixture is invalid: %s"), *Error));
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

	int32 CountModifiers(const FGameXXKCardBattleRuntime& Runtime, const EGameXXKCardEffectType EffectType)
	{
		int32 Count = 0;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			Count += Modifier.Definition.EffectType == EffectType ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcDualFreeCostTest,
	"GameXXK.Data.TaskNpcCards.Runtime.BladeTiming.EnergyAndManaDiscountsPreviewAndConsumeOnActiveCardsOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcDualFreeCostTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBladeRuntimeTest;
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, {
			{TEXT("MengZhai"), TEXT("Npc.TusiChief.MengZhaiShiYue"), TusiId},
			{TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroId}}, 58501)) return false;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("MengZhai"), EnemyAId, Result, TEXT("盟寨誓约冲锋"))) return true;
		FGameXXKCardPlayPreview Preview;
		FString Error;
		const bool bPreview = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next"), Preview, &Error);
		TestTrue(FString::Printf(TEXT("盟寨誓约后的下一牌可预览: %s"), *Error), bPreview);
		if (bPreview)
		{
			TestEqual(TEXT("盟寨誓约把下一主动牌气力改为0"), Preview.EffectiveEnergyCost, 0);
			TestEqual(TEXT("盟寨誓约把下一主动牌内力改为0"), Preview.EffectiveManaCost, 0);
		}
		if (!Resolve(*this, Runtime, TEXT("Next"), EnemyAId, Result, TEXT("盟寨誓约后的免费牌"))) return true;
		TestEqual(TEXT("一次性气力免费已消费"), CountModifiers(Runtime, EGameXXKCardEffectType::ModifyEnergyCost), 0);
		TestEqual(TEXT("一次性内力免费已消费"), CountModifiers(Runtime, EGameXXKCardEffectType::ModifyManaCost), 0);
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, {
			{TEXT("YiNuo"), TEXT("Npc.SongJinBao.YiNuoQianJin"), SongId},
			{TEXT("ErMu"), TEXT("Npc.SongJinBao.ErMuMiBao"), SongId},
			{TEXT("GuiKe"), TEXT("Npc.SongJinBao.GuiKeLing"), SongId},
			{TEXT("Next1"), TEXT("Hero.Generic.HeYuZhan"), HeroId},
			{TEXT("Next2"), TEXT("Hero.Generic.HeYuZhan"), HeroId},
			{TEXT("Next3"), TEXT("Hero.Generic.HeYuZhan"), HeroId}}, 58502)) return false;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("YiNuo"), NAME_None, Result, TEXT("一诺千金"))) return true;
		for (const FName InstanceId : {FName(TEXT("Next1")), FName(TEXT("Next2"))})
		{
			FGameXXKCardPlayPreview Preview;
			FString Error;
			const bool bPreview = GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error);
			TestTrue(FString::Printf(TEXT("一诺千金免费牌可预览: %s"), *Error), bPreview);
			if (bPreview)
			{
				TestEqual(TEXT("一诺千金免费气力"), Preview.EffectiveEnergyCost, 0);
				TestEqual(TEXT("一诺千金免费内力"), Preview.EffectiveManaCost, 0);
			}
			if (!Resolve(*this, Runtime, InstanceId, EnemyAId, Result, TEXT("一诺千金后续牌"))) return true;
		}
		FGameXXKCardPlayPreview ThirdPreview;
		FString Error;
		const bool bThirdPreview = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next3"), ThirdPreview, &Error);
		TestTrue(FString::Printf(TEXT("第三张恢复普通费用: %s"), *Error), bThirdPreview);
		if (bThirdPreview)
		{
			TestEqual(TEXT("第三张恢复基础气力"), ThirdPreview.EffectiveEnergyCost, 1);
			TestEqual(TEXT("第三张恢复基础内力"), ThirdPreview.EffectiveManaCost, 3);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcWidenNextCardTest,
	"GameXXK.Data.TaskNpcCards.Runtime.BladeTiming.WidenedSingleTargetPreviewMatchesGroupResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcWidenNextCardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBladeRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		{TEXT("TuSi"), TEXT("Npc.TusiChief.TuSiJunLing"), TusiId},
		{TEXT("Next"), TEXT("Hero.Generic.HeYuZhan"), HeroId}}, 58503)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TuSi"), EnemyAId, Result, TEXT("土司军令冲锋"))) return true;

	FGameXXKCardPlayPreview Preview;
	FString Error;
	const bool bPreview = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("Next"), Preview, &Error);
	TestTrue(FString::Printf(TEXT("群体改写后的牌可预览: %s"), *Error), bPreview);
	if (!bPreview) return true;
	TestEqual(TEXT("单体敌方预览改为全体敌方"), Preview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::AllEnemies);
	TestFalse(TEXT("群体改写后不再要求手动单点"), Preview.TargetRequest.bRequiresManualSelection);
	TestEqual(TEXT("群体改写预览锁定两名敌人"), Preview.TargetRequest.AutomaticTargetUnitIds.Num(), 2);
	const int32 EnemyABefore = FindUnit(Runtime, EnemyAId)->HP;
	const int32 EnemyBBefore = FindUnit(Runtime, EnemyBId)->HP;
	if (!Resolve(*this, Runtime, TEXT("Next"), NAME_None, Result, TEXT("群体改写后的下一牌"))) return true;
	TestEqual(TEXT("实际结算也包含两名敌人"), Result.TargetUnitIds.Num(), 2);
	TestTrue(TEXT("敌人A受到改写后的基础效果"), FindUnit(Runtime, EnemyAId)->HP < EnemyABefore);
	TestTrue(TEXT("敌人B受到改写后的基础效果"), FindUnit(Runtime, EnemyBId)->HP < EnemyBBefore);
	TestEqual(TEXT("群体改写只消费一次"), CountModifiers(Runtime, EGameXXKCardEffectType::WidenNextActiveSingleTarget), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcWidenedFinishTargetTest,
	"GameXXK.Data.TaskNpcCards.Runtime.BladeTiming.WidenedSingleTargetFinishMatchesGroupResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcWidenedFinishTargetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBladeRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		{TEXT("TuSi"), TEXT("Npc.TusiChief.TuSiJunLing"), TusiId},
		{TEXT("ShiMen"), TEXT("Npc.TusiChief.ShiMenShouShi"), TusiId}}, 58506)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TuSi"), EnemyAId, Result, TEXT("土司军令冲锋候选"))) return true;

	FGameXXKCardPlayPreview Preview;
	FString Error;
	const bool bPreview = GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("ShiMen"), Preview, &Error);
	TestTrue(FString::Printf(TEXT("石门守势可被扩展为友方群体: %s"), *Error), bPreview);
	if (!bPreview) return true;
	TestEqual(TEXT("扩展后的石门守势预览为全体友方"), Preview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::AllAllies);
	TestFalse(TEXT("扩展后的石门守势不再要求单点"), Preview.TargetRequest.bRequiresManualSelection);
	if (!Resolve(*this, Runtime, TEXT("ShiMen"), NAME_None, Result, TEXT("扩展后的石门守势"))) return true;
	TestEqual(TEXT("石门守势基础效果覆盖三名友方"), Result.TargetUnitIds.Num(), 3);

	TArray<FGameXXKCardDamageResult> BoundaryResults;
	const bool bEnded = GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryResults, &Error);
	TestTrue(FString::Printf(TEXT("扩展后的石门守势收招可结束回合: %s"), *Error), bEnded);
	if (!bEnded) return true;
	for (const FName PartyId : {TusiId, SongId, HeroId})
	{
		TestEqual(
			FString::Printf(TEXT("扩展后的收招给 %s 登记一次单攻转移"), *PartyId.ToString()),
			Status(Runtime, PartyId, EGameXXKCardStatus::RedirectSingleTargetEnemyAttack),
			1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcPreserveReactionTest,
	"GameXXK.Data.TaskNpcCards.Runtime.BladeTiming.FinishPreservesExactlyNextReactionUse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcPreserveReactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBladeRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		{TEXT("TuSi"), TEXT("Npc.TusiChief.TuSiJunLing"), TusiId}}, 58504)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TuSi"), EnemyAId, Result, TEXT("土司军令收招候选"))) return true;
	TArray<FGameXXKCardDamageResult> BoundaryResults;
	FString Error;
	const bool bEnded = GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryResults, &Error);
	TestTrue(FString::Printf(TEXT("土司军令收招可结束回合: %s"), *Error), bEnded);
	if (!bEnded) return true;
	TestEqual(TEXT("协战者持有一个守卫型反击来源"), Runtime.Reactions.Num(), 1);

	for (int32 EnemyCardIndex = 0; EnemyCardIndex < 2; ++EnemyCardIndex)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyAId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		Error.Reset();
		if (!TestTrue(TEXT("enemy packet resolves"), GameXXKCardRules::ResolveEnemyDirectAttack(
			Runtime, Context, HeroId, 1, Incoming, nullptr, &Error, true))) return true;
		TArray<FGameXXKCardDamageResult> Reactions;
		Error.Reset();
		if (!TestTrue(TEXT("reaction boundary resolves"), GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
			Runtime, EnemyAId, EGameXXKCardDamageKind::SingleTargetAttack, HeroId, Reactions, &Error))) return true;
		TestEqual(TEXT("each eligible enemy card resolves one stored Block"), Reactions.Num(), 1);
		TestEqual(
			EnemyCardIndex == 0 ? TEXT("first reaction use is preserved") : TEXT("second reaction use is consumed"),
			Runtime.Reactions.Num(),
			EnemyCardIndex == 0 ? 1 : 0);
	}
	TestEqual(TEXT("visible Block mirrors the consumed independent sources"), Status(Runtime, HeroId, EGameXXKCardStatus::Block), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcRetainArmorTest,
	"GameXXK.Data.TaskNpcCards.Runtime.BladeTiming.FinishRetainsPartyArmorForNextRoundOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcRetainArmorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBladeRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, {
		{TEXT("MengZhai"), TEXT("Npc.TusiChief.MengZhaiShiYue"), TusiId}}, 58505)) return false;
	FindUnit(Runtime, TusiId)->Defense = 16;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("MengZhai"), EnemyAId, Result, TEXT("盟寨誓约收招候选"))) return true;
	TestEqual(TEXT("base grants all party members Armor8"), FindUnit(Runtime, HeroId)->Armor, 8);
	TArray<FGameXXKCardDamageResult> BoundaryResults;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("player phase ends with armor-retain Finish: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryResults, &Error))) return true;
	Error.Reset();
	if (!TestTrue(FString::Printf(TEXT("next player phase begins: %s"), *Error),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, BoundaryResults, &Error))) return true;
	TestEqual(TEXT("Tusi armor survives the next-round decay point"), FindUnit(Runtime, TusiId)->Armor, 8);
	TestEqual(TEXT("Song armor survives the next-round decay point"), FindUnit(Runtime, SongId)->Armor, 8);
	TestEqual(TEXT("Hero armor survives the next-round decay point"), FindUnit(Runtime, HeroId)->Armor, 8);
	return true;
}

#endif
