#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKXuanJiaSetRuntimeTest
{
	const FName WearerId(TEXT("Xuan.Wearer"));
	const FName AllyId(TEXT("Xuan.Ally"));
	const FName EnemyId(TEXT("Xuan.Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Defense,
		const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 500 : 5000;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 50;
		Unit.Defense = Defense;
		Unit.Speed = 10;
		Unit.StableSortOrder = Sort;
		Unit.CombatLevel = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName CardId,
		const FName OwnerId,
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Xuan.Card.%d"), Ordinal));
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Xuan.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	FGameXXKEquipmentActiveEffect MakeXuanTwoEffect()
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = TEXT("Set.XuanJia.2");
		Effect.SourceCharacterId = WearerId;
		Effect.Set = EGameXXKEquipmentSet::XuanJia;
		Effect.RequiredPieces = 2;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Owner;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::Passive;
		Effect.ModifierKind = EGameXXKEquipmentModifierKind::ArmorGain;
		Effect.Magnitude = 1000;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
		Effect.MaxTriggersPerRound = 0;
		return Effect;
	}

	FGameXXKEquipmentActiveEffect MakeXuanFourEffect()
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = TEXT("Set.XuanJia.4");
		Effect.SourceCharacterId = WearerId;
		Effect.Set = EGameXXKEquipmentSet::XuanJia;
		Effect.RequiredPieces = 4;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Owner;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::RoundStart;
		Effect.ModifierKind = EGameXXKEquipmentModifierKind::CounterDamage;
		Effect.Magnitude = 5000;
		Effect.SecondaryMagnitude = 80;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
		Effect.MaxTriggersPerRound = 1;
		return Effect;
	}

	bool AddXuanFourEffect(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		const FGameXXKEquipmentActiveEffect Effect = MakeXuanFourEffect();
		if (!Test.TestTrue(TEXT("Xuanjia four-piece descriptor is authoritative"), FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)))
		{
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = Runtime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = Effect;
		RuntimeEffect.SourceCharacterId = WearerId;
		return true;
	}

	FGameXXKEquipmentActiveEffect MakeXuanSixEffect()
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = TEXT("Set.XuanJia.6");
		Effect.SourceCharacterId = WearerId;
		Effect.Set = EGameXXKEquipmentSet::XuanJia;
		Effect.RequiredPieces = 6;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Team;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::FirstAllyHealthDamagePerRound;
		Effect.ModifierKind = EGameXXKEquipmentModifierKind::GuardReduction;
		Effect.Magnitude = 4000;
		Effect.SecondaryMagnitude = 1;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
		Effect.MaxTriggersPerRound = 1;
		return Effect;
	}

	bool AddXuanSixEffect(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		const FGameXXKEquipmentActiveEffect Effect = MakeXuanSixEffect();
		if (!Test.TestTrue(TEXT("Xuanjia six-piece descriptor is authoritative"), FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)))
		{
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = Runtime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = Effect;
		RuntimeEffect.SourceCharacterId = WearerId;
		return true;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		const FName CardOwnerId,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardInstance> Cards;
		const bool bSorcererCard = CardId.ToString().StartsWith(TEXT("Profession.Sorcerer."));
		const TArray<FName> SorcererCardIds = {
			CardId,
			TEXT("Profession.Sorcerer.YanMuHuTi"),
			TEXT("Profession.Sorcerer.LieFu"),
			TEXT("Profession.Sorcerer.ChiYanFengJie"),
			TEXT("Profession.Sorcerer.SheLingHuo")};
		const int32 CardCount = bSorcererCard ? SorcererCardIds.Num() : 8;
		for (int32 Index = 0; Index < CardCount; ++Index)
		{
			Cards.Add(MakeCard(bSorcererCard ? SorcererCardIds[Index] : CardId, CardOwnerId, Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(WearerId, EGameXXKCardTargetSide::Party, bSorcererCard && CardOwnerId == WearerId ? EGameXXKCharacterRole::Sorcerer : EGameXXKCharacterRole::Guard, 358, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, bSorcererCard && CardOwnerId == AllyId ? EGameXXKCharacterRole::Sorcerer : EGameXXKCharacterRole::Guard, 100, 2),
			MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 10)};
		FString Error;
		if (!Test.TestTrue(
			TEXT("Xuanjia runtime initializes"),
			GameXXKCardRules::InitializeCardBattleRuntime(
				OutRuntime,
				Cards,
				Units,
				EGameXXKCardTerrain::Plain,
				72005,
				&Error)))
		{
			Test.AddError(Error);
			return false;
		}
		OutRuntime.Deck.SharedEnergy = 10;
		const int32 PrimaryIndex = OutRuntime.Deck.Hand.IndexOfByPredicate([CardId](const FGameXXKCardInstance& Card)
		{
			return Card.CardId == CardId;
		});
		if (!Test.TestTrue(TEXT("primary Xuanjia test card is in hand"), PrimaryIndex != INDEX_NONE))
		{
			return false;
		}
		OutRuntime.Deck.Hand.Swap(0, PrimaryIndex);
		const FGameXXKEquipmentActiveEffect Effect = MakeXuanTwoEffect();
		if (!Test.TestTrue(TEXT("Xuanjia two-piece descriptor is authoritative"), FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)))
		{
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = OutRuntime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = Effect;
		RuntimeEffect.SourceCharacterId = WearerId;
		if (!Test.TestTrue(TEXT("Xuanjia fixture validates"), GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error)))
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

	const FGameXXKCardArmorResult* FindArmorPacket(
		const FGameXXKCardPlayResult& Result,
		const FName TargetUnitId)
	{
		return Result.ArmorResults.FindByPredicate([TargetUnitId](const FGameXXKCardArmorResult& Armor)
		{
			return Armor.TargetUnitId == TargetUnitId;
		});
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Label)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(
			Runtime,
			Runtime.Deck.Hand[0].InstanceId,
			TargetId,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Label, *Error), bResolved);
		return bResolved;
	}

	bool AdvanceToNextPlayerRound(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		FString Error;
		if (!Test.TestTrue(TEXT("Xuanjia fixture ends the player phase"),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, DamageResults, &Error)))
		{
			Test.AddError(Error);
			return false;
		}
		DamageResults.Reset();
		if (!Test.TestTrue(TEXT("Xuanjia fixture begins the next player round"),
			GameXXKCardRules::BeginNextPlayerCardRound(Runtime, DamageResults, &Error)))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}

	void AddReaction(
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardStatus Status,
		const TCHAR* SourceCardInstanceId)
	{
		const int32 Ordinal = Runtime.NextReactionOrdinal++;
		FGameXXKReactionRuntime& Reaction = Runtime.Reactions.AddDefaulted_GetRef();
		Reaction.ReactionId = FName(*FString::Printf(TEXT("Reaction.%d"), Ordinal));
		Reaction.Status = Status;
		Reaction.RecipientUnitId = WearerId;
		Reaction.GrantedByUnitId = WearerId;
		Reaction.SourceCardInstanceId = FName(SourceCardInstanceId);
		Reaction.RemainingTriggers = 1;
		Reaction.ExpireBeforePlayerRound = Runtime.RoundNumber + 1;
		GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, WearerId), Status, 1);
	}

	bool ResolveReactionBoundary(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardDamageKind Kind,
		TArray<FGameXXKCardDamageResult>& OutResults,
		const bool bPartyHealthLost = false)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
			Runtime,
			EnemyId,
			Kind,
			Kind == EGameXXKCardDamageKind::SingleTargetAttack ? WearerId : NAME_None,
			OutResults,
			&Error,
			bPartyHealthLost);
		Test.TestTrue(FString::Printf(TEXT("Xuanjia reaction boundary resolves: %s"), *Error), bResolved);
		return bResolved;
	}

	int32 CountEquipmentPackets(const TArray<FGameXXKCardDamageResult>& Results)
	{
		return Results.FilterByPredicate([](const FGameXXKCardDamageResult& Result)
		{
			return Result.ResolutionOrigin == EGameXXKCardResolutionOrigin::Equipment;
		}).Num();
	}

	FGameXXKBattleRuntimeUnit MakeLegacyUnit(const FGameXXKCardCombatUnit& Unit)
	{
		FGameXXKBattleRuntimeUnit Legacy;
		Legacy.Id = Unit.UnitId;
		Legacy.HP = Unit.HP;
		Legacy.MaxHP = Unit.MaxHP;
		Legacy.MP = Unit.Mana;
		Legacy.MaxMP = Unit.MaxMana;
		Legacy.Attack = Unit.Attack;
		Legacy.Defense = Unit.Defense;
		Legacy.Speed = Unit.Speed;
		Legacy.Shield = Unit.Armor;
		Legacy.bEnemy = Unit.Side == EGameXXKCardTargetSide::Enemy;
		return Legacy;
	}

	FGameXXKRuntimeState MakeIntentState(
		FGameXXKCardBattleRuntime Runtime,
		const int32 Damage,
		const int32 HitCount)
	{
		FGameXXKRuntimeState State;
		State.bHasActiveBattle = true;
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = 1;
		State.CardRun.ActiveBattle = MoveTemp(Runtime);
		FGameXXKCardEnemyIntent Intent;
		Intent.CardId = TEXT("Test.XuanJia.MultiHit");
		Intent.CardDisplayName = TEXT("玄甲多段测试");
		Intent.SourceUnitId = EnemyId;
		Intent.SuggestedTargetUnitId = AllyId;
		Intent.Damage = Damage;
		Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		Intent.ResolutionOrder = 0;
		FGameXXKResolvedEnemyIntentEffect& Effect = Intent.Effects.AddDefaulted_GetRef();
		Effect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
		Effect.TargetUnitIds = {AllyId};
		Effect.Magnitude = Damage;
		Effect.BaseMagnitude = Damage;
		Effect.HitCount = HitCount;
		Effect.TargetRule = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
		State.CardRun.EnemyIntents = {MoveTemp(Intent)};
		for (const FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			(Unit.Side == EGameXXKCardTargetSide::Party ? State.ActiveBattleParty : State.ActiveBattleEnemies)
				.Add(MakeLegacyUnit(Unit));
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXuanJiaArmorGenerationTest,
	"GameXXK.Equipment.XuanJia.ArmorGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXuanJiaArmorGenerationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKXuanJiaSetRuntimeTest;

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, WearerId, Result, TEXT("wearer single Armor")))
		{
			const FGameXXKCardArmorResult* Armor = FindArmorPacket(Result, WearerId);
			TestNotNull(TEXT("single Armor result exists"), Armor);
			if (Armor) TestEqual(TEXT("80 percent source Armor becomes 316"), Armor->RequestedArmor, 316);
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Hero.Guard.LieZhenChengFeng"), WearerId, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("wearer group Armor")))
		{
			const FGameXXKCardArmorResult* Armor = FindArmorPacket(Result, WearerId);
			TestNotNull(TEXT("group Armor result exists"), Armor);
			if (Armor) TestEqual(TEXT("140 percent source Armor becomes 553"), Armor->RequestedArmor, 553);
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), AllyId, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, AllyId, Result, TEXT("foreign caster Armor")))
		{
			const FGameXXKCardArmorResult* Armor = FindArmorPacket(Result, AllyId);
			TestNotNull(TEXT("foreign Armor result exists"), Armor);
			if (Armor) TestEqual(TEXT("another caster's Armor is not amplified"), Armor->RequestedArmor, 80);
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Sorcerer.XingHuoHuiShou"), WearerId, Runtime)) return false;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("resolved Armor copy")))
		{
			const FGameXXKCardArmorResult* Original = FindArmorPacket(Result, WearerId);
			const FGameXXKCardArmorResult* Copy = FindArmorPacket(Result, AllyId);
			TestNotNull(TEXT("original overflow Armor exists"), Original);
			TestNotNull(TEXT("copied overflow Armor exists"), Copy);
			if (Original && Copy)
			{
				TestEqual(TEXT("overflow Armor is amplified once"), Original->RequestedArmor, 22);
				TestEqual(TEXT("PriorEffectResult copy is not amplified twice"), Copy->RequestedArmor, Original->RequestedArmor);
			}
		}
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Profession.Sorcerer.LingYanLianDan"), WearerId, Runtime)) return false;
		FGameXXKCardCombatUnit* Wearer = FindUnit(Runtime, WearerId);
		if (!TestNotNull(TEXT("current-Armor copy fixture retains wearer"), Wearer)) return false;
		Wearer->Armor = 100;
		FGameXXKCardPlayResult Result;
		if (Resolve(*this, Runtime, NAME_None, Result, TEXT("current Armor copy")))
		{
			const FGameXXKCardArmorResult* Copy = FindArmorPacket(Result, WearerId);
			TestNotNull(TEXT("current Armor copy packet exists"), Copy);
			if (Copy) TestEqual(TEXT("current-Armor copy is not amplified"), Copy->RequestedArmor, 100);
			const FGameXXKCardCombatUnit* ResolvedWearer = FindUnit(Runtime, WearerId);
			TestEqual(TEXT("current Armor doubles exactly"), ResolvedWearer ? ResolvedWearer->Armor : INDEX_NONE, 200);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXuanJiaArmorRetentionTest,
	"GameXXK.Equipment.XuanJia.ArmorRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXuanJiaArmorRetentionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKXuanJiaSetRuntimeTest;

	for (const TPair<int32, int32>& ArmorCase : {
		TPair<int32, int32>(300, 150),
		TPair<int32, int32>(301, 150)})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, Runtime)
			|| !AddXuanFourEffect(*this, Runtime))
		{
			return false;
		}
		FindUnit(Runtime, WearerId)->Armor = ArmorCase.Key;
		FindUnit(Runtime, AllyId)->Armor = 99;
		if (!AdvanceToNextPlayerRound(*this, Runtime))
		{
			return false;
		}
		TestEqual(TEXT("Xuanjia four-piece retains exactly half with floor rounding"), FindUnit(Runtime, WearerId)->Armor, ArmorCase.Value);
		TestEqual(TEXT("non-wearer clears Armor"), FindUnit(Runtime, AllyId)->Armor, 0);
	}

	FGameXXKCardBattleRuntime FullRetentionRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, FullRetentionRuntime)
		|| !AddXuanFourEffect(*this, FullRetentionRuntime))
	{
		return false;
	}
	FindUnit(FullRetentionRuntime, WearerId)->Armor = 301;
	FullRetentionRuntime.RetainArmorAtNextPartyPhaseUnitIds.Add(WearerId);
	if (!AdvanceToNextPlayerRound(*this, FullRetentionRuntime))
	{
		return false;
	}
	TestEqual(TEXT("explicit full retention wins"), FindUnit(FullRetentionRuntime, WearerId)->Armor, 301);
	TestTrue(TEXT("explicit retention token is consumed"), FullRetentionRuntime.RetainArmorAtNextPartyPhaseUnitIds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXuanJiaBlockFollowUpTest,
	"GameXXK.Equipment.XuanJia.BlockFollowUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXuanJiaBlockFollowUpTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKXuanJiaSetRuntimeTest;

	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, Runtime)
		|| !AddXuanFourEffect(*this, Runtime))
	{
		return false;
	}
	Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Xuan.Block.FirstA"));
	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Xuan.Block.FirstB"));
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveReactionBoundary(*this, Runtime, EGameXXKCardDamageKind::SingleTargetAttack, Results))
	{
		return false;
	}
	TestEqual(TEXT("multiple Block sources still add one Xuanjia packet"), CountEquipmentPackets(Results), 1);
	const FGameXXKCardDamageResult* EquipmentPacket = Results.FindByPredicate([](const FGameXXKCardDamageResult& Result)
	{
		return Result.ResolutionOrigin == EGameXXKCardResolutionOrigin::Equipment;
	});
	TestNotNull(TEXT("first blocked card records the Xuanjia packet"), EquipmentPacket);
	if (EquipmentPacket)
	{
		TestEqual(TEXT("Xuanjia packet uses the wearer as source"), EquipmentPacket->SourceUnitId, WearerId);
		TestEqual(TEXT("Xuanjia packet requests eighty percent of Attack"), EquipmentPacket->BaseRequestedDamage, 40);
	}

	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Xuan.Block.Second"));
	Results.Reset();
	if (!ResolveReactionBoundary(*this, Runtime, EGameXXKCardDamageKind::SingleTargetAttack, Results))
	{
		return false;
	}
	TestEqual(TEXT("second enemy card adds no second Xuanjia packet"), CountEquipmentPackets(Results), 0);

	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Xuan.Block.Group"));
	Results.Reset();
	if (!ResolveReactionBoundary(*this, Runtime, EGameXXKCardDamageKind::GroupAttack, Results))
	{
		return false;
	}
	TestEqual(TEXT("group enemy card never opens the Xuanjia Block follow-up"), CountEquipmentPackets(Results), 0);

	TArray<FGameXXKCardDamageResult> BoundaryDamage;
	FString Error;
	if (!TestTrue(TEXT("Xuanjia Block fixture reaches the next player round"),
		GameXXKCardRules::BeginNextPlayerCardRound(Runtime, BoundaryDamage, &Error))
		|| !TestTrue(TEXT("Xuanjia Block fixture enters the next enemy phase"),
			GameXXKCardRules::EndPlayerCardPhase(Runtime, BoundaryDamage, &Error)))
	{
		AddError(Error);
		return false;
	}
	AddReaction(Runtime, EGameXXKCardStatus::Block, TEXT("Xuan.Block.NextPhase"));
	Results.Reset();
	if (!ResolveReactionBoundary(*this, Runtime, EGameXXKCardDamageKind::SingleTargetAttack, Results))
	{
		return false;
	}
	TestEqual(TEXT("next enemy phase rearms Xuanjia"), CountEquipmentPackets(Results), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXuanJiaTeamRescueTest,
	"GameXXK.Equipment.XuanJia.TeamRescue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXuanJiaTeamRescueTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKXuanJiaSetRuntimeTest;

	for (const EGameXXKCardDamageKind Kind : {
		EGameXXKCardDamageKind::SingleTargetAttack,
		EGameXXKCardDamageKind::GroupAttack})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, Runtime)
			|| !AddXuanSixEffect(*this, Runtime))
		{
			return false;
		}
		Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
		TArray<FGameXXKCardDamageResult> Results;
		if (!ResolveReactionBoundary(*this, Runtime, Kind, Results, true))
		{
			return false;
		}
		TestEqual(TEXT("Xuanjia rescue grants amplified forty-percent Defense Armor to wearer"), FindUnit(Runtime, WearerId)->Armor, 159);
		TestEqual(TEXT("Xuanjia rescue grants the same Armor to every ally"), FindUnit(Runtime, AllyId)->Armor, 159);
		TestEqual(TEXT("Xuanjia rescue creates one wearer-to-ally Guard link"), Runtime.GuardLinks.Num(), 1);
		if (Runtime.GuardLinks.Num() == 1)
		{
			TestEqual(TEXT("rescue Guard link uses the wearer"), Runtime.GuardLinks[0].GuardianUnitId, WearerId);
			TestEqual(TEXT("rescue Guard link protects the other ally"), Runtime.GuardLinks[0].ProtectedUnitId, AllyId);
			TestEqual(TEXT("rescue Guard link has one use"), Runtime.GuardLinks[0].Stacks, 1);
		}

		Results.Reset();
		if (!ResolveReactionBoundary(*this, Runtime, Kind, Results, true))
		{
			return false;
		}
		TestEqual(TEXT("same enemy phase cannot grant rescue Armor twice"), FindUnit(Runtime, WearerId)->Armor, 159);
		TestEqual(TEXT("same enemy phase cannot add another Guard use"), Runtime.GuardLinks.Num() == 1 ? Runtime.GuardLinks[0].Stacks : INDEX_NONE, 1);
	}

	FGameXXKCardBattleRuntime NoHealthLossRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, NoHealthLossRuntime)
		|| !AddXuanSixEffect(*this, NoHealthLossRuntime))
	{
		return false;
	}
	NoHealthLossRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveReactionBoundary(*this, NoHealthLossRuntime, EGameXXKCardDamageKind::SingleTargetAttack, Results, false))
	{
		return false;
	}
	TestEqual(TEXT("full Armor absorption, Poison, and self-loss do not trigger rescue"), FindUnit(NoHealthLossRuntime, WearerId)->Armor, 0);
	TestTrue(TEXT("ineligible health loss creates no Guard link"), NoHealthLossRuntime.GuardLinks.IsEmpty());

	FGameXXKCardBattleRuntime ExistingLinkRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, ExistingLinkRuntime)
		|| !AddXuanSixEffect(*this, ExistingLinkRuntime))
	{
		return false;
	}
	ExistingLinkRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	FGameXXKCardGuardLinkRuntime& ExistingLink = ExistingLinkRuntime.GuardLinks.AddDefaulted_GetRef();
	ExistingLink.GuardianUnitId = WearerId;
	ExistingLink.ProtectedUnitId = AllyId;
	ExistingLink.Stacks = 2;
	ExistingLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	if (!ResolveReactionBoundary(*this, ExistingLinkRuntime, EGameXXKCardDamageKind::GroupAttack, Results, true))
	{
		return false;
	}
	TestEqual(TEXT("rescue increments an existing matching Guard link"), ExistingLinkRuntime.GuardLinks.Num(), 1);
	TestEqual(TEXT("existing Guard link gains exactly one use"), ExistingLinkRuntime.GuardLinks[0].Stacks, 3);

	FGameXXKCardBattleRuntime DeadSourceRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, DeadSourceRuntime)
		|| !AddXuanSixEffect(*this, DeadSourceRuntime))
	{
		return false;
	}
	DeadSourceRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	FindUnit(DeadSourceRuntime, WearerId)->HP = 0;
	FindUnit(DeadSourceRuntime, WearerId)->bLiving = false;
	Results.Reset();
	if (!ResolveReactionBoundary(*this, DeadSourceRuntime, EGameXXKCardDamageKind::GroupAttack, Results, true))
	{
		return false;
	}
	TestEqual(TEXT("defeated six-piece source cannot grant Armor"), FindUnit(DeadSourceRuntime, AllyId)->Armor, 0);
	TestTrue(TEXT("defeated six-piece source cannot create Guard links"), DeadSourceRuntime.GuardLinks.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKXuanJiaIntentIntegrationTest,
	"GameXXK.Equipment.XuanJia.IntentIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKXuanJiaIntentIntegrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKXuanJiaSetRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), WearerId, Runtime)
		|| !AddXuanSixEffect(*this, Runtime))
	{
		return false;
	}
	Runtime.Phase = EGameXXKCardBattlePhase::Enemy;
	FindUnit(Runtime, AllyId)->Defense = 0;
	FGameXXKRuntimeState State = MakeIntentState(MoveTemp(Runtime), 20, 3);
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> Results;
	bool bFinished = false;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("multi-hit Xuanjia intent resolves: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			ResolvedIntent,
			Results,
			bFinished,
			&Error)))
	{
		return false;
	}
	int32 DirectHitCount = 0;
	for (const FGameXXKCardDamageResult& Result : Results)
	{
		if (Result.Cause == EGameXXKCardDamageCause::DirectAttack && Result.SourceUnitId == EnemyId)
		{
			++DirectHitCount;
			TestEqual(TEXT("rescue does not redirect any hit inside the completed volley"), Result.ResolvedTargetUnitId, AllyId);
			TestTrue(TEXT("each multi-hit packet really loses health"), Result.HealthDamage > 0);
		}
	}
	TestEqual(TEXT("adapter reports all three direct hits before rescue"), DirectHitCount, 3);
	TestEqual(TEXT("adapter triggers rescue once after the complete multi-hit card"), State.CardRun.ActiveBattle.GuardLinks.Num(), 1);
	TestEqual(TEXT("post-volley rescue grants amplified Armor"), FindUnit(State.CardRun.ActiveBattle, AllyId)->Armor, 159);
	return true;
}

#endif
