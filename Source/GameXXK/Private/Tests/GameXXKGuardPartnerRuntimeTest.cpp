#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKGuardPartnerRuntimeTest
{
	const FName GuardId(TEXT("Guard"));
	const FName AllyAId(TEXT("AllyA"));
	const FName AllyBId(TEXT("AllyB"));
	const FName EnemyAId(TEXT("EnemyA"));
	const FName EnemyBId(TEXT("EnemyB"));

	FGameXXKCardCombatUnit MakeUnit(const FName UnitId, const EGameXXKCardTargetSide Side, const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? EGameXXKCharacterRole::Guard : EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = 1000;
		Unit.MaxHP = 1000;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 50 : 0;
		Unit.Attack = UnitId == GuardId ? 12 : 8;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = Sort;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const TCHAR* InstanceId, const TCHAR* CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = GuardId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("GuardPartner.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(FAutomationTestBase& Test, const TArray<FGameXXKCardInstance>& Cards, FGameXXKCardBattleRuntime& OutRuntime, const int32 Seed, const int32 GuardDefense = 0)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(GuardId, EGameXXKCardTargetSide::Party, 1),
			MakeUnit(AllyAId, EGameXXKCardTargetSide::Party, 2),
			MakeUnit(AllyBId, EGameXXKCardTargetSide::Party, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, Seed, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Guard partner runtime initializes: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 20;
		for (FGameXXKCardCombatUnit& Unit : OutRuntime.Units)
		{
			if (Unit.UnitId == GuardId) Unit.Defense = GuardDefense;
		}
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Guard partner fixture validates: %s"), *Error));
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate) { return Candidate.UnitId == UnitId; });
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Found = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate) { return Candidate.UnitId == UnitId; });
		return Found ? GameXXKCardRules::GetCombatStatusStacks(*Found, StatusType) : INDEX_NONE;
	}

	bool Resolve(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName InstanceId, const FName TargetId, FGameXXKCardPlayResult& OutResult)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), *InstanceId.ToString(), *Error), bResolved);
		return bResolved;
	}

	bool HasGuardLink(const FGameXXKCardBattleRuntime& Runtime, const FName GuardianId, const FName ProtectedId, const int32 Stacks)
	{
		return Runtime.GuardLinks.ContainsByPredicate([GuardianId, ProtectedId, Stacks](const FGameXXKCardGuardLinkRuntime& Link)
		{
			return Link.GuardianUnitId == GuardianId && Link.ProtectedUnitId == ProtectedId && Link.Stacks == Stacks;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerCoreProtectionRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.CoreProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerCoreProtectionRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TieBi"), TEXT("Profession.Guard.TieBi"), 0),
		MakeCard(TEXT("HuZhu"), TEXT("Profession.Guard.HuZhu"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime, 71001, 20)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TieBi"), NAME_None, Result)) return true;
	TestEqual(TEXT("铁壁按防御20的80%获得16护甲"), Unit(Runtime, GuardId)->Armor, 16);
	TestEqual(TEXT("铁壁精确获得1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 1);
	TestEqual(TEXT("铁壁为守卫叠加1层嘲讽标记"), Status(Runtime, GuardId, EGameXXKCardStatus::Mark), 1);
	if (!Resolve(*this, Runtime, TEXT("HuZhu"), AllyAId, Result)) return true;
	TestEqual(TEXT("护主为指定友军增加16护甲"), Unit(Runtime, AllyAId)->Armor, 16);
	TestEqual(TEXT("护主为守卫再增加16护甲"), Unit(Runtime, GuardId)->Armor, 32);
	TestEqual(TEXT("护主再增加1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 2);
	TestEqual(TEXT("护主继续叠加1层嘲讽标记"), Status(Runtime, GuardId, EGameXXKCardStatus::Mark), 2);
	TestTrue(TEXT("护主注册守卫保护指定友军一次"), HasGuardLink(Runtime, GuardId, AllyAId, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerSelfProtectionEdgeRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.SelfProtectionNeverStrands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerSelfProtectionEdgeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	{
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("HuZhu"), TEXT("Profession.Guard.HuZhu"), 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, Runtime, 71002, 20)) return false;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("HuZhu"), GuardId, Result)) return true;
		TestEqual(TEXT("护主选自己时两段16护甲都结算"), Unit(Runtime, GuardId)->Armor, 32);
		TestEqual(TEXT("护主选自己时仍获得1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 1);
		TestEqual(TEXT("护主选自己不会生成自指守护关系"), Runtime.GuardLinks.Num(), 0);
	}
	{
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("YuanHu"), TEXT("Profession.Guard.YuanHuBu"), 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, Runtime, 71003, 20)) return false;
		Unit(Runtime, GuardId)->HP = 100;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("YuanHu"), NAME_None, Result)) return true;
		TestEqual(TEXT("援护步选自己时两段40%防御护甲都结算"), Unit(Runtime, GuardId)->Armor, 16);
		TestEqual(TEXT("援护步自动选中自己时仍获得1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 1);
		TestEqual(TEXT("援护步不会生成自指守护关系"), Runtime.GuardLinks.Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerSingleConversionRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.SingleConversionNeverConsumesArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerSingleConversionRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	const FGameXXKCardInstance SuiJia = MakeCard(TEXT("SuiJia"), TEXT("Profession.Guard.SuiJiaHuiJi"), 0);
	const FGameXXKCardInstance Drawn = MakeCard(TEXT("Drawn"), TEXT("Profession.Guard.TieBi"), 1);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, {SuiJia, Drawn}, Runtime, 71004)) return false;
	Runtime.Deck.Hand = {SuiJia};
	Runtime.Deck.DrawPile = {Drawn};
	Unit(Runtime, GuardId)->Armor = 12;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("SuiJia"), EnemyAId, Result)) return true;
	TestEqual(TEXT("碎甲回击只生成一个伤害包"), Result.DamageResults.Num(), 1);
	if (Result.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("碎甲回击为100%攻击12加当前护甲12"), Result.DamageResults[0].BaseRequestedDamage, 24);
	}
	TestEqual(TEXT("单体护甲转伤不消耗护甲"), Unit(Runtime, GuardId)->Armor, 12);
	TestEqual(TEXT("碎甲回击获得1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 1);
	TestTrue(TEXT("护甲不低于12时抽1张"), Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card) { return Card.InstanceId == TEXT("Drawn"); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerBlockConsumesOneLayerRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.BlockConsumesOneLayerPerSourcePerEnemyCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerBlockConsumesOneLayerRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("FanZhenJia"), TEXT("Profession.Guard.FanZhenJia"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime, 71010)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("FanZhenJia"), NAME_None, Result)) return true;
	TestEqual(TEXT("反震甲登记同一来源的2层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 2);

	TArray<FGameXXKCardDamageResult> EndPhaseDamage;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("反震甲可进入敌方阶段: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, EndPhaseDamage, &Error)))
	{
		return true;
	}
	Unit(Runtime, GuardId)->Armor = 12;

	for (int32 EnemyCardIndex = 0; EnemyCardIndex < 2; ++EnemyCardIndex)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyAId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		Error.Reset();
		if (!TestTrue(
			FString::Printf(TEXT("第%d张敌方单体牌伤害包结算: %s"), EnemyCardIndex + 1, *Error),
			GameXXKCardRules::ResolveEnemyDirectAttack(
				Runtime,
				Context,
				GuardId,
				1,
				Incoming,
				nullptr,
				&Error,
				true)))
		{
			return true;
		}
		const int32 ArmorAfterEnemyCard = Unit(Runtime, GuardId)->Armor;
		TArray<FGameXXKCardDamageResult> Reactions;
		Error.Reset();
		if (!TestTrue(
			FString::Printf(TEXT("第%d张敌方单体牌后的格挡边界结算: %s"), EnemyCardIndex + 1, *Error),
			GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
				Runtime,
				EnemyAId,
				EGameXXKCardDamageKind::SingleTargetAttack,
				GuardId,
				Reactions,
				&Error)))
		{
			return true;
		}
		TestEqual(TEXT("一张敌方单体牌只触发一个格挡伤害包"), Reactions.Num(), 1);
		if (Reactions.Num() == 1)
		{
			TestEqual(
				TEXT("格挡伤害始终使用当前攻击加受击后的当前护甲"),
				Reactions[0].BaseRequestedDamage,
				12 + ArmorAfterEnemyCard);
		}
		TestEqual(TEXT("格挡结算本身不消耗护甲"), Unit(Runtime, GuardId)->Armor, ArmorAfterEnemyCard);
		TestEqual(
			TEXT("同一来源每张敌方牌只消耗1层格挡"),
			Status(Runtime, GuardId, EGameXXKCardStatus::Block),
			1 - EnemyCardIndex);
		TestEqual(
			TEXT("独立格挡记录与可见层数同步"),
			Runtime.Reactions.Num(),
			1 - EnemyCardIndex);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerReplayedBlockCreatesIndependentSourceRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.ReplayedBlockCreatesIndependentSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerReplayedBlockCreatesIndependentSourceRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("FanZhenJia"), TEXT("Profession.Guard.FanZhenJia"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, Runtime, 71011)) return false;

	FGameXXKResolvedCardSnapshot Snapshot;
	Snapshot.CardId = TEXT("Profession.Guard.FanZhenJia");
	Snapshot.Quality = EGameXXKCardQuality::Common;
	Snapshot.OwnerUnitId = GuardId;
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {Snapshot, Snapshot};
	TArray<FGameXXKCardPlayResult> ReplayResults;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("同一张反震甲的两次重放都可结算: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, ReplayResults, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("两次重放分别结算一次基础效果"), ReplayResults.Num(), 2);
	TestEqual(TEXT("两次重放合计登记4层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 4);
	TestEqual(TEXT("两次重放保留4条独立格挡记录"), Runtime.Reactions.Num(), 4);

	TArray<FGameXXKCardDamageResult> EndPhaseDamage;
	Error.Reset();
	if (!TestTrue(
		FString::Printf(TEXT("重放夹具可进入敌方阶段: %s"), *Error),
		GameXXKCardRules::EndPlayerCardPhase(Runtime, EndPhaseDamage, &Error)))
	{
		return true;
	}
	Unit(Runtime, GuardId)->Armor = 12;
	for (int32 EnemyCardIndex = 0; EnemyCardIndex < 2; ++EnemyCardIndex)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = EnemyAId;
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		FGameXXKCardDamageResult Incoming;
		Error.Reset();
		if (!TestTrue(TEXT("重放格挡夹具的敌方单体牌伤害包可结算"), GameXXKCardRules::ResolveEnemyDirectAttack(
			Runtime, Context, GuardId, 1, Incoming, nullptr, &Error, true)))
		{
			return true;
		}
		TArray<FGameXXKCardDamageResult> Reactions;
		Error.Reset();
		if (!TestTrue(TEXT("重放格挡夹具的敌方牌边界可结算"), GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
			Runtime,
			EnemyAId,
			EGameXXKCardDamageKind::SingleTargetAttack,
			GuardId,
			Reactions,
			&Error)))
		{
			return true;
		}
		TestEqual(TEXT("每个重放登记批次各触发一个格挡包"), Reactions.Num(), 2);
		TestEqual(
			TEXT("每个重放登记批次各消耗一层"),
			Status(Runtime, GuardId, EGameXXKCardStatus::Block),
			EnemyCardIndex == 0 ? 2 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerAllArmorReleaseRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.AllArmorRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerAllArmorReleaseRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	{
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("ZhenYue"), TEXT("Profession.Guard.ZhenYueLing"), 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, Runtime, 71005, 20)) return false;
		Unit(Runtime, GuardId)->Armor = 4;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("ZhenYue"), NAME_None, Result)) return true;
		TestEqual(TEXT("镇岳令对两名敌人各生成一个伤害包"), Result.DamageResults.Num(), 2);
		for (const FGameXXKCardDamageResult& Damage : Result.DamageResults)
		{
			TestEqual(TEXT("镇岳令护甲4时按184%攻击造成22点伤害"), Damage.BaseRequestedDamage, 22);
		}
		for (const FName UnitId : {GuardId, AllyAId, AllyBId})
		{
			TestEqual(TEXT("镇岳令按50%守卫防御为全队各重建10护甲"), Unit(Runtime, UnitId)->Armor, 10);
			TestEqual(TEXT("镇岳令为全队获得1层格挡"), Status(Runtime, UnitId, EGameXXKCardStatus::Block), 1);
		}
	}
	{
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("BiLei"), TEXT("Profession.Guard.BiLeiFanGong"), 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, Runtime, 71006, 20)) return false;
		Unit(Runtime, GuardId)->Armor = 4;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("BiLei"), NAME_None, Result)) return true;
		TestEqual(TEXT("壁垒反攻对两名敌人各生成一个伤害包"), Result.DamageResults.Num(), 2);
		for (const FGameXXKCardDamageResult& Damage : Result.DamageResults)
		{
			TestEqual(TEXT("壁垒反攻护甲4时按224%攻击造成26点伤害"), Damage.BaseRequestedDamage, 26);
		}
		TestEqual(TEXT("壁垒反攻消耗旧护甲后为自己重建10护甲"), Unit(Runtime, GuardId)->Armor, 10);
		TestEqual(TEXT("壁垒反攻为自己获得1层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 1);
		TestEqual(TEXT("壁垒反攻不为其他友军重建护甲"), Unit(Runtime, AllyAId)->Armor, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerConditionalAndRetentionRuntimeTest,
	"GameXXK.Data.PartnerCards.Guard.ConditionalAndOwnerRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerConditionalAndRetentionRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerRuntimeTest;
	{
		const FGameXXKCardInstance PanShi = MakeCard(TEXT("PanShi"), TEXT("Profession.Guard.PanShiTuNa"), 0);
		const FGameXXKCardInstance Drawn = MakeCard(TEXT("Drawn"), TEXT("Profession.Guard.TieBi"), 1);
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, {PanShi, Drawn}, Runtime, 71007, 20)) return false;
		Runtime.Deck.Hand = {PanShi};
		Runtime.Deck.DrawPile = {Drawn};
		Unit(Runtime, GuardId)->Armor = 0;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("PanShi"), NAME_None, Result)) return true;
		TestEqual(TEXT("零护甲时磐石吐纳按40%防御获得8护甲"), Unit(Runtime, GuardId)->Armor, 8);
		TestEqual(TEXT("零护甲分支不回复内力"), Unit(Runtime, GuardId)->Mana, 20);
		TestEqual(TEXT("零护甲分支不抽牌"), Runtime.Deck.Hand.Num(), 0);
	}
	{
		const FGameXXKCardInstance PanShi = MakeCard(TEXT("PanShi"), TEXT("Profession.Guard.PanShiTuNa"), 0);
		const FGameXXKCardInstance Drawn = MakeCard(TEXT("Drawn"), TEXT("Profession.Guard.TieBi"), 1);
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, {PanShi, Drawn}, Runtime, 71008, 20)) return false;
		Runtime.Deck.Hand = {PanShi};
		Runtime.Deck.DrawPile = {Drawn};
		Unit(Runtime, GuardId)->Armor = 1;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("PanShi"), NAME_None, Result)) return true;
		TestEqual(TEXT("一护甲已足够进入资源分支且不再加甲"), Unit(Runtime, GuardId)->Armor, 1);
		TestEqual(TEXT("正护甲分支回复5内力"), Unit(Runtime, GuardId)->Mana, 25);
		TestTrue(TEXT("正护甲分支抽1张"), Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card) { return Card.InstanceId == TEXT("Drawn"); }));
	}
	{
		const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("BuDong"), TEXT("Profession.Guard.BuDongRuShan"), 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, Runtime, 71009, 20)) return false;
		Unit(Runtime, GuardId)->Armor = 5;
		Unit(Runtime, AllyAId)->Armor = 7;
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, TEXT("BuDong"), NAME_None, Result)) return true;
		TestEqual(TEXT("不动如山按防御20的200%增加40护甲"), Unit(Runtime, GuardId)->Armor, 45);
		TestEqual(TEXT("不动如山获得3层格挡"), Status(Runtime, GuardId, EGameXXKCardStatus::Block), 3);
		TestEqual(TEXT("不动如山只登记一名护甲保留单位"), Runtime.RetainArmorAtNextPartyPhaseUnitIds.Num(), 1);
		TestTrue(TEXT("不动如山保留的是守卫自身"), Runtime.RetainArmorAtNextPartyPhaseUnitIds.Contains(GuardId));
		TestEqual(TEXT("不动如山不改动其他友军当前护甲"), Unit(Runtime, AllyAId)->Armor, 7);
		TArray<FGameXXKCardDamageResult> BoundaryDamage;
		FString Error;
		if (!TestTrue(
			FString::Printf(TEXT("不动如山可结束当前玩家阶段: %s"), *Error),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryDamage, &Error)))
		{
			return true;
		}
		BoundaryDamage.Reset();
		Error.Reset();
		if (!TestTrue(
			FString::Printf(TEXT("不动如山护甲保留可推进到下一玩家阶段: %s"), *Error),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, BoundaryDamage, &Error)))
		{
			return true;
		}
		TestEqual(TEXT("不动如山在下一玩家阶段完整保留45护甲"), Unit(Runtime, GuardId)->Armor, 45);
		TestEqual(TEXT("不动如山不替其他友军保留护甲"), Unit(Runtime, AllyAId)->Armor, 0);
		TestEqual(TEXT("护甲保留凭据在一次阶段边界后清空"), Runtime.RetainArmorAtNextPartyPhaseUnitIds.Num(), 0);
	}
	return true;
}

#endif
