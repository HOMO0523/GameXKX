#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCombatSimulationRules.h"
#include "GameXXKCompanionRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroCardIntegrationTest
{
	const FName HeroId(TEXT("Hero"));
	const FName AllyId(TEXT("Ally"));
	const FName EnemyId(TEXT("Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = FMath::Max(1, HP);
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 50 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits(
		const int32 EnemyHP = 500,
		const int32 HeroHP = 100,
		const int32 HeroAttack = 10)
	{
		return {
			MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, HeroHP, HeroAttack, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 100, 8, 2),
			MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, 8, 10)};
	}

	FGameXXKCardInstance MakeCard(
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const int32 AcquisitionOrdinal,
		const FName OwnerUnitId = HeroId)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Integration.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandIds,
		const int32 Seed,
		const int32 EnemyHP = 500,
		const int32 HeroHP = 100,
		const int32 HeroAttack = 10,
		const EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Plain)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(EnemyHP, HeroHP, HeroAttack),
			Terrain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("integration runtime failed to initialize: %s"), *Error));
			return false;
		}

		const TSet<FName> RequestedHand(HandIds);
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(RequestedHand.Contains(Card.InstanceId) ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic integration fixture is invalid: %s"), *Error));
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

	int32 Status(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId,
		const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		const TArray<FName>& Targets = {},
		const FName OwnerUnitId = HeroId)
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = OwnerUnitId;
		Snapshot.OriginalTargetUnitIds = Targets;
		return Snapshot;
	}

	TArray<FGameXXKResolvedCardSnapshot> MakeMageQueue(const FName LastCardId)
	{
		return {
			MakeSnapshot(TEXT("Hero.Mage.YanXuLiaoYuan")),
			MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), {EnemyId}),
			MakeSnapshot(TEXT("Hero.Generic.HeYuZhan"), {EnemyId}),
			MakeSnapshot(TEXT("Hero.Generic.SuiYanJi"), {EnemyId}),
			MakeSnapshot(TEXT("Hero.Generic.PoYunYiShan"), {EnemyId}),
			MakeSnapshot(TEXT("Hero.Blade.XueLuXiangCheng"), {EnemyId}),
			MakeSnapshot(TEXT("Hero.Hunter.LieYuLianShi"), {EnemyId}),
			MakeSnapshot(LastCardId, {EnemyId})};
	}

	void SetMageQueue(
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FGameXXKResolvedCardSnapshot>& Snapshots,
		const int32 NextCardIndex)
	{
		TArray<FName> LockedIds;
		for (const FGameXXKResolvedCardSnapshot& Snapshot : Snapshots)
		{
			LockedIds.Add(Snapshot.CardId);
		}
		Runtime.EquippedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.bActive = true;
		Runtime.HeroSpellTask.LockedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.CompletedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.FirstPlayOrder = Snapshots;
		Runtime.HeroSpellTask.StarterReward = EGameXXKHeroSpellTaskReward::Fire;
		Runtime.HeroSpellTask.StarterOwnerUnitId = HeroId;
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::MageTaskReplay;
		Runtime.AutomaticResolutionQueue.PendingCards = Snapshots;
		Runtime.AutomaticResolutionQueue.NextCardIndex = NextCardIndex;
		Runtime.AutomaticResolutionQueue.PendingReward = EGameXXKHeroSpellTaskReward::Fire;
		Runtime.AutomaticResolutionQueue.RewardOwnerUnitId = HeroId;
	}

	int32 CountDamage(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardResolutionOrigin Origin,
		const EGameXXKCardDamageCause Cause = EGameXXKCardDamageCause::Invalid)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.ResolutionOrigin == Origin
				&& (Cause == EGameXXKCardDamageCause::Invalid || Result.Cause == Cause)
				? 1
				: 0;
		}
		return Count;
	}

	int32 CountCause(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardDamageCause Cause)
	{
		return Results.FilterByPredicate([Cause](const FGameXXKCardDamageResult& Result)
		{
			return Result.Cause == Cause;
		}).Num();
	}

	FString SerializeMetricMap(const TMap<FName, int64>& Metrics)
	{
		TArray<FName> Keys;
		Metrics.GenerateKeyArray(Keys);
		Keys.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
		TArray<FString> Parts;
		for (const FName Key : Keys)
		{
			Parts.Add(FString::Printf(TEXT("%s=%lld"), *Key.ToString(), Metrics.FindChecked(Key)));
		}
		return FString::Join(Parts, TEXT(";"));
	}

	void AddReaction(
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardStatus StatusType,
		const TCHAR* SourceCardInstanceId)
	{
		const int32 Ordinal = Runtime.NextReactionOrdinal++;
		FGameXXKReactionRuntime& Reaction = Runtime.Reactions.AddDefaulted_GetRef();
		Reaction.ReactionId = FName(*FString::Printf(TEXT("Integration.Reaction.%d"), Ordinal));
		Reaction.Status = StatusType;
		Reaction.RecipientUnitId = HeroId;
		Reaction.GrantedByUnitId = HeroId;
		Reaction.SourceCardInstanceId = FName(SourceCardInstanceId);
		Reaction.RemainingTriggers = 1;
		Reaction.ExpireBeforePlayerRound = Runtime.RoundNumber + 1;
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroId), StatusType, 1);
	}

	FGameXXKBattleRuntimeUnit MakeLegacyUnit(const FGameXXKCardCombatUnit& Unit)
	{
		FGameXXKBattleRuntimeUnit Legacy;
		Legacy.Id = Unit.UnitId;
		Legacy.DisplayName = FText::FromName(Unit.UnitId);
		Legacy.HP = Unit.HP;
		Legacy.MaxHP = Unit.MaxHP;
		Legacy.MP = Unit.Mana;
		Legacy.MaxMP = Unit.MaxMana;
		Legacy.Attack = Unit.Attack;
		Legacy.Defense = Unit.Defense;
		Legacy.Speed = Unit.Speed;
		Legacy.Shield = Unit.Armor;
		Legacy.bEnemy = Unit.Side == EGameXXKCardTargetSide::Enemy;
		Legacy.bDefeated = !Unit.bLiving;
		Legacy.BattleSlotNumber = Legacy.bEnemy ? 1 : INDEX_NONE;
		Legacy.CombatLevel = 1;
		return Legacy;
	}

	FGameXXKCardEnemyIntent MakeIntent(const int32 Damage, const int32 HitCount)
	{
		FGameXXKCardEnemyIntent Intent;
		Intent.CardId = TEXT("Integration.Enemy.MultiHit");
		Intent.CardDisplayName = TEXT("Integration Multi-Hit");
		Intent.SourceUnitId = EnemyId;
		Intent.SuggestedTargetUnitId = HeroId;
		Intent.Damage = Damage;
		Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		Intent.ResolutionOrder = 0;
		FGameXXKResolvedEnemyIntentEffect& Effect = Intent.Effects.AddDefaulted_GetRef();
		Effect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
		Effect.TargetUnitIds = {HeroId};
		Effect.Magnitude = Damage;
		Effect.BaseMagnitude = Damage;
		Effect.HitCount = HitCount;
		Effect.TargetRule = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
		return Intent;
	}

	FGameXXKRuntimeState MakeState(
		const FGameXXKCardBattleRuntime& Runtime,
		const TArray<FGameXXKCardEnemyIntent>& Intents = {})
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 1;
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = 1;
		State.CardRun.ActiveBattle = Runtime;
		State.CardRun.EnemyIntents = Intents;
		State.CardRun.NextEnemyIntentIndex = 0;
		State.ActiveBattleParty.Reset();
		State.ActiveBattleEnemies.Reset();
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			(Unit.Side == EGameXXKCardTargetSide::Party ? State.ActiveBattleParty : State.ActiveBattleEnemies)
				.Add(MakeLegacyUnit(Unit));
		}
		return State;
	}

	TArray<uint8> SerializeRuntime(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKCardBattleRuntime Copy = Runtime;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool DeserializeRuntime(const TArray<uint8>& Bytes, FGameXXKCardBattleRuntime& OutRuntime)
	{
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Archive, &OutRuntime, nullptr);
		return !Reader.IsError();
	}

	FGameXXKBattleRuntimeUnit MakeBattleUnit(
		const TCHAR* Id,
		const int32 HP,
		const int32 Mana,
		const int32 Attack,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(Id);
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.MP = Mana;
		Unit.MaxMP = Mana;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		Unit.BattleSlotNumber = bEnemy ? 1 : INDEX_NONE;
		Unit.EnemyDefinitionId = bEnemy ? FName(TEXT("Enemy.Ch1.Rooster")) : NAME_None;
		Unit.CombatLevel = 1;
		return Unit;
	}

	TArray<TArray<FName>> FocusedLoadouts()
	{
		const TArray<FName> Generic = {
			TEXT("Hero.Generic.QingFengYiShi"), TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"), TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"), TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"), TEXT("Hero.Generic.GuanXi")};
		const TArray<FName> Bridge = {
			TEXT("Hero.Generic.QingFengYiShi"), TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.HengJianShouShi"), TEXT("Hero.Generic.NingShenTuNa")};
		return {
			Generic,
			{TEXT("Hero.Blade.TongFengYinShi"), TEXT("Hero.Blade.XueLuXiangCheng"), TEXT("Hero.Blade.YingFengHuanBu"), TEXT("Hero.Blade.TongPaoJuShi"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]},
			{TEXT("Hero.Guard.TieBiTongShou"), TEXT("Hero.Guard.JieJiaHuanFeng"), TEXT("Hero.Guard.LieZhenChengFeng"), TEXT("Hero.Guard.XuanJiaZhenYue"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]},
			{TEXT("Hero.Healer.YiXueCuiFang"), TEXT("Hero.Healer.HuiChunNiMai"), TEXT("Hero.Healer.DuHuoTongLu"), TEXT("Hero.Healer.BaiCaoJiZhen"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]},
			{TEXT("Hero.Hunter.FengYanDingXian"), TEXT("Hero.Hunter.LieYuLianShi"), TEXT("Hero.Hunter.CuiDuChuanXin"), TEXT("Hero.Hunter.HuiFengGuanRi"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]},
			{TEXT("Hero.Mage.YanXuLiaoYuan"), TEXT("Hero.Mage.HanXuNingChuan"), TEXT("Hero.Mage.LeiXuYinTing"), TEXT("Hero.Mage.GuiXuTongXuan"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]},
			{TEXT("Hero.Formation.GuanShiLuoZi"), TEXT("Hero.Formation.YiZhenHuiXiang"), TEXT("Hero.Formation.LianYingBuShi"), TEXT("Hero.Formation.LiuHeGuiYi"), Bridge[0], Bridge[1], Bridge[2], Bridge[3]}};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladeReplayHunterIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.BladeReplayOfHunterDoesNotDoubleConsumeCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeReplayHunterIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("TongFeng"), TEXT("Hero.Blade.TongFengYinShi"), 0),
		MakeCard(TEXT("LieYu"), TEXT("Hero.Hunter.LieYuLianShi"), 1),
		MakeCard(TEXT("Filler"), TEXT("Hero.Generic.NingShenTuNa"), 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("TongFeng"), TEXT("LieYu")}, 61001)) return false;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroId), EGameXXKCardStatus::Charge, 2);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("TongFeng"), AllyId, Result, TEXT("Blade replay source"))) return false;
	if (!Resolve(*this, Runtime, TEXT("LieYu"), EnemyId, Result, TEXT("replayed Heavy Arrow"))) return false;
	TestEqual(TEXT("the real Heavy Arrow locks Charge2 once"), Result.HeavyArrowChargeConsumed, 2);
	TestEqual(TEXT("only the real Heavy Arrow appends two charge attacks"), Result.HeavyArrowExtraAttackCount, 2);
	TestEqual(TEXT("the active base emits one direct packet"), CountDamage(Result.DamageResults, EGameXXKCardResolutionOrigin::ActivePlay, EGameXXKCardDamageCause::DirectAttack), 1);
	TestEqual(TEXT("two and only two packets use Heavy Arrow origin"), CountDamage(Result.DamageResults, EGameXXKCardResolutionOrigin::HeavyArrow, EGameXXKCardDamageCause::DirectAttack), 2);
	TestEqual(TEXT("the replay contributes only one base direct packet"), CountDamage(Result.DamageResults, EGameXXKCardResolutionOrigin::AutomaticReplay, EGameXXKCardDamageCause::DirectAttack), 1);
	TestEqual(TEXT("the active transaction audits its one automatic replay"), Result.AutomaticResolutionCount, 1);
	TestEqual(TEXT("the active transaction audits a one-entry automatic queue"), Result.MaximumAutomaticQueueDepth, 1);
	TestEqual(TEXT("the replay never consumes Charge a second time"), Status(Runtime, HeroId, EGameXXKCardStatus::Charge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageFormationListenerIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.MageReplayOfFormationDoesNotConsumeTerrainListener",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageFormationListenerIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Listener"), TEXT("Hero.Formation.LianYingBuShi"), 0),
		MakeCard(TEXT("Filler"), TEXT("Hero.Generic.NingShenTuNa"), 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Listener")}, 61002)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Listener"), EnemyId, Result, TEXT("terrain listener source"))) return false;
	const FGameXXKCardBattleModifierRuntime* Listener = Runtime.Modifiers.FindByPredicate([](const FGameXXKCardBattleModifierRuntime& Modifier)
	{
		return Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::AfterEachActiveCard;
	});
	TestNotNull(TEXT("the Formation card registers its three-use active listener"), Listener);
	if (!Listener) return false;
	const FName ListenerId = Listener->ModifierId;
	const int32 TriggersBefore = Listener->Definition.RemainingTriggers;
	const TArray<FGameXXKResolvedCardSnapshot> Snapshots = MakeMageQueue(TEXT("Hero.Formation.GuanShiLuoZi"));
	SetMageQueue(Runtime, Snapshots, 7);
	FString Error;
	TestTrue(FString::Printf(TEXT("the completed Mage queue fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	TArray<FGameXXKCardPlayResult> Results;
	TestTrue(FString::Printf(TEXT("Mage replays the Formation card: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	const FGameXXKCardBattleModifierRuntime* ListenerAfter = Runtime.Modifiers.FindByPredicate([ListenerId](const FGameXXKCardBattleModifierRuntime& Modifier)
	{
		return Modifier.ModifierId == ListenerId;
	});
	TestNotNull(TEXT("the active-only terrain listener survives the Mage replay"), ListenerAfter);
	if (ListenerAfter)
	{
		TestEqual(TEXT("the Mage replay consumes no terrain-listener trigger"), ListenerAfter->Definition.RemainingTriggers, TriggersBefore);
	}
	int32 TerrainListenerPackets = 0;
	for (const FGameXXKCardPlayResult& QueueResult : Results)
	{
		TerrainListenerPackets += CountDamage(QueueResult.DamageResults, EGameXXKCardResolutionOrigin::TerrainListener);
	}
	TestEqual(TEXT("the automatic Formation replay never opens the active terrain listener"), TerrainListenerPackets, 0);
	TestEqual(TEXT("the Mage replay does not increment the active-card counter"), Runtime.ActiveCardsPlayedThisRound, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerMageQueueIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.HealerToxicExplosionCanFinishMageFireQueueWithoutPartialCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerMageQueueIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Hero.Generic.NingShenTuNa"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {}, 61003, 20, 100, 10)) return false;
	const TArray<FGameXXKResolvedCardSnapshot> Snapshots = MakeMageQueue(TEXT("Hero.Healer.DuHuoTongLu"));
	SetMageQueue(Runtime, Snapshots, 7);
	FString Error;
	TestTrue(FString::Printf(TEXT("the lethal Mage queue fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	TArray<FGameXXKCardPlayResult> Results;
	TestTrue(FString::Printf(TEXT("the lethal Toxic Explosion queue resolves atomically: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	int32 PoisonExplosions = 0;
	int32 BurnExplosions = 0;
	for (const FGameXXKCardPlayResult& QueueResult : Results)
	{
		PoisonExplosions += CountCause(QueueResult.DamageResults, EGameXXKCardDamageCause::ToxicExplosionPoison);
		BurnExplosions += CountCause(QueueResult.DamageResults, EGameXXKCardDamageCause::ToxicExplosionBurn);
	}
	TestEqual(TEXT("the queued Healer base commits its Poison explosion"), PoisonExplosions, 1);
	TestEqual(TEXT("the queued Healer base commits its Burn explosion even when it is terminal"), BurnExplosions, 1);
	TestEqual(TEXT("the Toxic Explosion defeats the final enemy"), FindUnit(Runtime, EnemyId)->HP, 0);
	TestFalse(TEXT("the completed Mage queue leaves no partial continuation"), Runtime.AutomaticResolutionQueue.bActive);
	TestFalse(TEXT("the completed Mage task resets after its reward boundary"), Runtime.HeroSpellTask.bActive);
	TestEqual(TEXT("the terminal queue gives the player victory"), Runtime.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDodgeMultiHitReactionIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.DodgedEnemyMultiHitStillProducesOneCounterAndOneBlockPerSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDodgeMultiHitReactionIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Hero.Generic.NingShenTuNa"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, 61004, 300)) return false;
	Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
	Runtime.CombatRandomState = 17;
	FindUnit(Runtime, HeroId)->Armor = 5;
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, HeroId), EGameXXKCardStatus::Agility, 6);
	AddReaction(Runtime, EGameXXKCardStatus::Counter, TEXT("Counter.A"));
	AddReaction(Runtime, EGameXXKCardStatus::Counter, TEXT("Counter.B"));
	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Block.A"));
	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Block.B"));
	FString Error;
	TestTrue(FString::Printf(TEXT("the reaction fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	FGameXXKRuntimeState State = MakeState(Runtime, {MakeIntent(20, 3)});
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> Results;
	bool bFinished = false;
	TestTrue(FString::Printf(TEXT("the saved multi-hit intent resolves: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bFinished, &Error));
	TestTrue(TEXT("the only intent reaches its enemy-card boundary"), bFinished);
	TestEqual(TEXT("all three direct segments are audited"), CountCause(Results, EGameXXKCardDamageCause::DirectAttack), 3);
	for (const FGameXXKCardDamageResult& Result : Results)
	{
		if (Result.Cause == EGameXXKCardDamageCause::DirectAttack)
		{
			TestTrue(TEXT("each multi-hit segment is fully dodged"), Result.bAvoidedByAgility);
			TestEqual(TEXT("a dodged segment causes no health damage"), Result.HealthDamage, 0);
		}
	}
	TestEqual(TEXT("each of two Counter sources triggers exactly once"), CountCause(Results, EGameXXKCardDamageCause::Counter), 2);
	TestEqual(TEXT("each of two Block sources triggers exactly once"), CountCause(Results, EGameXXKCardDamageCause::Block), 2);
	TestEqual(TEXT("the enemy-card boundary consumes all four independent sources"), State.CardRun.ActiveBattle.Reactions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDeckCompositionIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.EightHeroFivePartnerThreeNpcCompositionRemainsExact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDeckCompositionIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the card run initializes: %s"), *Error), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))) return false;
	FGameXXKCompanionRecruitResult Recruit;
	if (!TestTrue(FString::Printf(TEXT("a permanent partner recruits: %s"), *Error),
		FGameXXKCompanionRules::RecruitPermanentCompanion(State.CardRun.CompanionRoster, TEXT("Companion.Blade.01"), 61005, Recruit, &Error))) return false;
	if (Recruit.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
	{
		AddError(TEXT("the integration fixture requires a newly recruited permanent partner"));
		return false;
	}
	TestEqual(TEXT("the permanent partner owns exactly five selected cards"), Recruit.Companion.SelectedCardIds.Num(), 5);
	TestTrue(FString::Printf(TEXT("the permanent partner activates: %s"), *Error),
		FGameXXKCompanionRules::SetActivePermanentCompanion(State.CardRun.CompanionRoster, Recruit.Companion.InstanceId, &Error));
	State.CardRun.PartySelection.ActivePermanentCompanionInstanceId = Recruit.Companion.InstanceId;
	TestTrue(FString::Printf(TEXT("the temporary NPC configures three cards: %s"), *Error),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &Error));
	State.ActiveBattleParty = {MakeBattleUnit(TEXT("Player"), 100, 30, 15, false)};
	State.ActiveBattleEnemies = {MakeBattleUnit(TEXT("MoneyRat"), 80, 0, 8, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 42;
	TestTrue(FString::Printf(TEXT("the 8+5+3 party starts a card battle: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 61005, &Error));
	int32 HeroCards = 0;
	int32 PartnerCards = 0;
	int32 NpcCards = 0;
	int32 RouteCards = 0;
	const auto CountInstance = [&](const FGameXXKCardInstance& Instance)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Instance.CardId);
		if (!Definition)
		{
			AddError(FString::Printf(TEXT("materialized card %s has no catalog definition"), *Instance.CardId.ToString()));
			return;
		}
		switch (Definition->Owner)
		{
		case EGameXXKCardOwner::Hero: ++HeroCards; break;
		case EGameXXKCardOwner::Profession: ++PartnerCards; break;
		case EGameXXKCardOwner::QuestNpc: ++NpcCards; break;
		case EGameXXKCardOwner::Route: ++RouteCards; break;
		default: break;
		}
	};
	for (const FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.DrawPile) CountInstance(Instance);
	for (const FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.Hand) CountInstance(Instance);
	for (const FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.DiscardPile) CountInstance(Instance);
	for (const FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.ExhaustPile) CountInstance(Instance);
	TestEqual(TEXT("the protagonist contributes exactly eight cards"), HeroCards, 8);
	TestEqual(TEXT("the active permanent partner contributes exactly five cards"), PartnerCards, 5);
	TestEqual(TEXT("the temporary NPC contributes exactly three cards"), NpcCards, 3);
	TestEqual(TEXT("route cards remain a separately counted source"),
		HeroCards + PartnerCards + NpcCards + RouteCards,
		State.CardRun.ActiveBattle.Deck.ActiveInstanceIds.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLevelLoadoutResumeIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.LevelOneAndLevelTwentyLoadoutsStartAndResumeBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLevelLoadoutResumeIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	for (const int32 Level : {1, 20})
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.PlayerLevel = Level;
		State.PlayerXP = 0;
		UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("level %d card run initializes: %s"), Level, *Error),
			FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))) continue;
		if (Level == 20)
		{
			TArray<FName> Loadout = FGameXXKCardCatalog::GetHeroCardIdsUnlockedAtLevel(Level);
			Loadout.RemoveAt(0, Loadout.Num() - 8, EAllowShrinking::No);
			TestTrue(FString::Printf(TEXT("level %d accepts a late-game eight-card loadout: %s"), Level, *Error),
				FGameXXKCardBattleAdapter::SetHeroSelectedCards(State, Loadout, &Error));
		}
		if (!TestTrue(FString::Printf(TEXT("level %d enters a generated route"), Level),
			UGameXXKMVPRules::OpenWorldMap(State)
				&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
				&& UGameXXKMVPRules::AcceptTownQuest(State)
				&& UGameXXKMVPRules::EnterDungeon(State))) continue;
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.CurrentRouteNodeId = 0;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}},
			FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.5f, 0.5f), TArray<int32>{2}},
			FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{0, 1}, FGameXXKRouteMapEdge{1, 2}};
		State.VisitedRouteNodeIds = {0};
		State.ReachableRouteNodeIds = {1};
		if (!TestTrue(FString::Printf(TEXT("level %d starts its generated-route battle"), Level),
			UGameXXKMVPRules::SelectRouteNodeById(State, 1))) continue;
		const FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		FGameXXKRuntimeState Restored;
		FGameXXKSaveMigrationReport Report;
		if (!TestTrue(FString::Printf(TEXT("level %d active battle restores: %s"), Level, *Report.Error),
			FGameXXKSaveMigration::TryRestoreRuntimeState(Save, Restored, Report))) continue;
		TestTrue(FString::Printf(TEXT("level %d restored card runtime validates: %s"), Level, *Error),
			GameXXKCardRules::ValidateCardBattleRuntime(Restored.CardRun.ActiveBattle, &Error));
		TestEqual(FString::Printf(TEXT("level %d preserves its eight equipped Hero cards"), Level),
			Restored.CardRun.ActiveBattle.EquippedHeroCardIds.Num(), 8);
		if (!Restored.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			FGameXXKCardPlayPreview Preview;
			TestTrue(FString::Printf(TEXT("level %d restored battle accepts a card-check continuation: %s"), Level, *Error),
				FGameXXKCardBattleAdapter::BuildCardPlayPreview(Restored, Restored.CardRun.ActiveBattle.Deck.Hand[0].InstanceId, Preview, &Error));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKForcedDiscardReloadIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.SaveReloadDuringForcedDiscardContinuationIsByteStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKForcedDiscardReloadIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	TArray<FGameXXKCardInstance> Cards;
	TArray<FName> HandIds;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FString Instance = FString::Printf(TEXT("Forced.%d"), Index);
		Cards.Add(MakeCard(*Instance, TEXT("Hero.Generic.NingShenTuNa"), Index));
		if (Index < 5) HandIds.Add(FName(*Instance));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, HandIds, 61006)) return false;
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {
		MakeSnapshot(TEXT("Hero.Mage.GuiXuTongXuan")),
		MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), {EnemyId})};
	TArray<FGameXXKCardPlayResult> InitialResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("the automatic queue pauses on forced discard: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, InitialResults, &Error));
	TestEqual(TEXT("forced discard is the persisted continuation"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	const TArray<uint8> SavedBytes = SerializeRuntime(Runtime);
	FGameXXKCardBattleRuntime Reloaded;
	TestTrue(TEXT("the forced-discard runtime deserializes"), DeserializeRuntime(SavedBytes, Reloaded));
	TestEqual(TEXT("reload reproduces the exact paused bytes"), SerializeRuntime(Reloaded), SavedBytes);
	const FName DiscardId = Runtime.Deck.PendingChoice.Candidates[0].InstanceId;
	TArray<FGameXXKCardPlayResult> OriginalResults;
	TArray<FGameXXKCardPlayResult> ReloadedResults;
	TestTrue(FString::Printf(TEXT("the original continuation resumes: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(Runtime, {DiscardId}, &Error, &OriginalResults));
	TestTrue(FString::Printf(TEXT("the reloaded continuation resumes: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(Reloaded, {DiscardId}, &Error, &ReloadedResults));
	TestEqual(TEXT("forced-discard continuation is byte-stable after reload"), SerializeRuntime(Reloaded), SerializeRuntime(Runtime));
	TestEqual(TEXT("both continuations expose the same result count"), ReloadedResults.Num(), OriginalResults.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageSearchReloadIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.SaveReloadDuringMageSearchContinuationIsByteStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageSearchReloadIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FName> Equipped = {
		TEXT("Hero.Mage.YanXuLiaoYuan"), TEXT("Hero.Generic.QingFengYiShi"),
		TEXT("Hero.Generic.HeYuZhan"), TEXT("Hero.Generic.SuiYanJi"),
		TEXT("Hero.Generic.PoYunYiShan"), TEXT("Hero.Blade.XueLuXiangCheng"),
		TEXT("Hero.Hunter.LieYuLianShi"), TEXT("Hero.Healer.DuHuoTongLu")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		Cards.Add(MakeCard(Index == 0 ? TEXT("Starter") : *FString::Printf(TEXT("Candidate.%d"), Index), *Equipped[Index].ToString(), Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Starter")}, 61007)) return false;
	Runtime.EquippedHeroCardIds = Equipped;
	Runtime.HeroSpellTask.bActive = true;
	Runtime.HeroSpellTask.LockedHeroCardIds = Equipped;
	Runtime.HeroSpellTask.StarterReward = EGameXXKHeroSpellTaskReward::Fire;
	Runtime.HeroSpellTask.StarterOwnerUnitId = HeroId;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Starter"), NAME_None, Result, TEXT("Mage search starter"))) return false;
	TestEqual(TEXT("the Mage starter opens the serialized search choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	const TArray<uint8> SavedBytes = SerializeRuntime(Runtime);
	FGameXXKCardBattleRuntime Reloaded;
	TestTrue(TEXT("the Mage-search runtime deserializes"), DeserializeRuntime(SavedBytes, Reloaded));
	TestEqual(TEXT("reload reproduces the exact Mage-search bytes"), SerializeRuntime(Reloaded), SavedBytes);
	const FName PickedId = Runtime.Deck.PendingChoice.Candidates[0].InstanceId;
	TArray<FGameXXKCardPlayResult> OriginalResults;
	TArray<FGameXXKCardPlayResult> ReloadedResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("the original Mage search resumes: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, PickedId, OriginalResults, &Error));
	TestTrue(FString::Printf(TEXT("the reloaded Mage search resumes: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Reloaded, PickedId, ReloadedResults, &Error));
	TestEqual(TEXT("Mage-search continuation is byte-stable after reload"), SerializeRuntime(Reloaded), SerializeRuntime(Runtime));
	TestEqual(TEXT("both Mage-search continuations expose the same result count"), ReloadedResults.Num(), OriginalResults.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalReactionVictoryIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.BothSidesDieDuringFinalReactionAndPlayerWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalReactionVictoryIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Hero.Generic.NingShenTuNa"), 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, 61008, 10, 5, 10)) return false;
	Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
	AddReaction(Runtime, EGameXXKCardStatus::Counter, TEXT("Final.Counter"));
	FString Error;
	TestTrue(FString::Printf(TEXT("the final-reaction fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	FGameXXKRuntimeState State = MakeState(Runtime, {MakeIntent(5, 1)});
	FGameXXKCardEnemyIntent Intent;
	TArray<FGameXXKCardDamageResult> Results;
	bool bFinished = false;
	TestTrue(FString::Printf(TEXT("the lethal enemy card and final Counter resolve: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, Intent, Results, bFinished, &Error));
	TestEqual(TEXT("the enemy card defeats the final party member"), FindUnit(State.CardRun.ActiveBattle, HeroId)->HP, 0);
	TestEqual(TEXT("the already queued Counter defeats the final enemy"), FindUnit(State.CardRun.ActiveBattle, EnemyId)->HP, 0);
	TestEqual(TEXT("simultaneous final-reaction death is player victory"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Victory);
	TestEqual(TEXT("the final Counter is audited once"), CountCause(Results, EGameXXKCardDamageCause::Counter), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFocusedHeroSimulationIntegrationTest,
	"GameXXK.Data.HeroCards.Integration.FocusedSevenBuildSimulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFocusedHeroSimulationIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardIntegrationTest;
	const TArray<TArray<FName>> Loadouts = FocusedLoadouts();
	TestEqual(TEXT("the focused matrix contains seven distinct build families"), Loadouts.Num(), 7);
	TSet<int32> CoveredBuilds;
	int32 Victories = 0;
	int32 Defeats = 0;
	int64 TotalActiveCards = 0;
	int64 TotalAutomaticResolutions = 0;
	int64 TotalStrandedTargets = 0;
	int64 TotalRounds = 0;
	int64 TotalEnergySpent = 0;
	int64 TotalEnergyGained = 0;
	int64 TotalManaSpent = 0;
	int64 TotalManaGained = 0;
	int64 TotalHealingGenerated = 0;
	int64 TotalArmorGenerated = 0;
	int32 MaximumQueueDepth = 0;
	int32 MaximumHandSize = 0;
	TMap<FName, int64> DamageByOrigin;
	for (int32 Seed = 1101; Seed <= 1200; ++Seed)
	{
		const int32 BuildIndex = (Seed - 1101) % Loadouts.Num();
		CoveredBuilds.Add(BuildIndex);
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.PlayerLevel = 20;
		FString Error;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
			|| !FGameXXKCardBattleAdapter::SetHeroSelectedCards(State, Loadouts[BuildIndex], &Error))
		{
			AddError(FString::Printf(TEXT("seed %d build %d could not configure its legal eight-card loadout: %s"), Seed, BuildIndex, *Error));
			continue;
		}
		State.ActiveBattleEnemies = {MakeBattleUnit(TEXT("MoneyRat"), 80, 0, 7, true)};
		State.bHasActiveBattle = true;
		FGameXXKSimulationScenario Scenario;
		Scenario.Seed = Seed;
		Scenario.InitialRuntimeState = State;
		Scenario.NodeKind = EGameXXKNodeKind::Battle;
		Scenario.Terrain = EGameXXKCardTerrain::Plain;
		Scenario.Policy = EGameXXKSimulationPolicy::Skilled;
		Scenario.MaxRounds = 100;
		Scenario.MaxDecisions = 2000;
		FGameXXKSimulationMetrics Metrics;
		TArray<FGameXXKSimulationTraceEntry> Trace;
		if (!FGameXXKCombatSimulationRules::RunScenario(Scenario, Metrics, Trace, &Error))
		{
			AddError(FString::Printf(TEXT("seed %d build %d violated a simulation invariant: %s"), Seed, BuildIndex, *Error));
			continue;
		}
		Metrics.bVictory ? ++Victories : ++Defeats;
		TestTrue(FString::Printf(TEXT("seed %d ends within 100 rounds"), Seed), Metrics.Rounds > 0 && Metrics.Rounds <= 100);
		TestTrue(FString::Printf(TEXT("seed %d never exceeds the twenty-card hand cap"), Seed), Metrics.MaximumHandSize <= 20);
		TestTrue(FString::Printf(TEXT("seed %d never exceeds the automatic queue safety bound"), Seed), Metrics.MaximumAutomaticQueueDepth <= 64);
		TestEqual(FString::Printf(TEXT("seed %d leaves no card stranded by an otherwise legal target"), Seed), Metrics.StrandedTargetFailures, 0);
		TestTrue(FString::Printf(TEXT("seed %d records at least one active card"), Seed), Metrics.ActivelyPlayedCards > 0);
		TestTrue(FString::Printf(TEXT("seed %d keeps resource audit totals non-negative"), Seed),
			Metrics.EnergySpent >= 0 && Metrics.EnergyGained >= 0 && Metrics.ManaSpent >= 0 && Metrics.ManaGained >= 0);
		TotalActiveCards += Metrics.ActivelyPlayedCards;
		TotalAutomaticResolutions += Metrics.AutomaticResolutionCount;
		TotalStrandedTargets += Metrics.StrandedTargetFailures;
		TotalRounds += Metrics.Rounds;
		TotalEnergySpent += Metrics.EnergySpent;
		TotalEnergyGained += Metrics.EnergyGained;
		TotalManaSpent += Metrics.ManaSpent;
		TotalManaGained += Metrics.ManaGained;
		TotalHealingGenerated += Metrics.HealingGenerated;
		TotalArmorGenerated += Metrics.ArmorGenerated;
		MaximumQueueDepth = FMath::Max(MaximumQueueDepth, Metrics.MaximumAutomaticQueueDepth);
		MaximumHandSize = FMath::Max(MaximumHandSize, Metrics.MaximumHandSize);
		for (const TPair<FName, int64>& Pair : Metrics.DamageByOrigin)
		{
			DamageByOrigin.FindOrAdd(Pair.Key) += Pair.Value;
		}
	}
	TestEqual(TEXT("all seven build families are represented"), CoveredBuilds.Num(), 7);
	TestEqual(TEXT("all one hundred fixed seeds reach victory or normal defeat"), Victories + Defeats, 100);
	AddInfo(FString::Printf(
		TEXT("[HeroFocusedSimulation] cases=100 victories=%d defeats=%d rounds=%lld active_cards=%lld automatic_resolutions=%lld energy=%lld/%lld mana=%lld/%lld healing=%lld armor=%lld max_queue=%d max_hand=%d stranded_targets=%lld damage_origins=%s"),
		Victories,
		Defeats,
		TotalRounds,
		TotalActiveCards,
		TotalAutomaticResolutions,
		TotalEnergySpent,
		TotalEnergyGained,
		TotalManaSpent,
		TotalManaGained,
		TotalHealingGenerated,
		TotalArmorGenerated,
		MaximumQueueDepth,
		MaximumHandSize,
		TotalStrandedTargets,
		*SerializeMetricMap(DamageByOrigin)));
	return true;
}

#endif
