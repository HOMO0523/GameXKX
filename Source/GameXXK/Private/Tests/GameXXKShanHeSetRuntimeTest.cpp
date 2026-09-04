#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKShanHeSetRuntimeTest
{
	const FName WearerId(TEXT("Shan.Wearer"));
	const FName AllyId(TEXT("Shan.Ally"));
	const FName EnemyId(TEXT("Shan.Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 500 : 5000;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 50 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 50;
		Unit.Defense = UnitId == WearerId ? 301 : 20;
		Unit.Speed = 10;
		Unit.StableSortOrder = Sort;
		Unit.CombatLevel = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName CardId,
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Shan.Card.%d"), Ordinal));
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = WearerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Shan.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	FGameXXKEquipmentActiveEffect MakeShanFourEffect()
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = TEXT("Set.ShanHe.4");
		Effect.SourceCharacterId = WearerId;
		Effect.Set = EGameXXKEquipmentSet::ShanHe;
		Effect.RequiredPieces = 4;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Owner;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::TerrainSynergyCard;
		Effect.ModifierKind = EGameXXKEquipmentModifierKind::TerrainCostReduction;
		Effect.Magnitude = 1;
		Effect.SecondaryMagnitude = 2;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::FlatCount;
		Effect.MaxTriggersPerRound = 1;
		return Effect;
	}

	FGameXXKEquipmentActiveEffect MakeShanTwoEffect()
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = TEXT("Set.ShanHe.2");
		Effect.SourceCharacterId = WearerId;
		Effect.Set = EGameXXKEquipmentSet::ShanHe;
		Effect.RequiredPieces = 2;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Owner;
		Effect.Hook = EGameXXKEquipmentSetBonusHook::TerrainSynergyCard;
		Effect.ModifierKind = EGameXXKEquipmentModifierKind::TerrainPower;
		Effect.Magnitude = 1;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::FlatCount;
		Effect.MaxTriggersPerRound = 1;
		return Effect;
	}

	bool AddShanTwoEffect(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime)
	{
		const FGameXXKEquipmentActiveEffect Effect = MakeShanTwoEffect();
		if (!Test.TestTrue(TEXT("Shanhe two-piece descriptor is authoritative"), FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)))
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
		FGameXXKCardBattleRuntime& OutRuntime,
		const EGameXXKCharacterRole WearerRole = EGameXXKCharacterRole::FormationMaster,
		const int32 CardCount = 5)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardCount; ++Index)
		{
			Cards.Add(MakeCard(CardId, Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(WearerId, EGameXXKCardTargetSide::Party, WearerRole, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 2),
			MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		FString Error;
		if (!Test.TestTrue(TEXT("Shanhe runtime initializes"),
			GameXXKCardRules::InitializeCardBattleRuntime(
				OutRuntime,
				Cards,
				Units,
				EGameXXKCardTerrain::Cave,
				73005,
				&Error)))
		{
			Test.AddError(Error);
			return false;
		}
		OutRuntime.Deck.SharedEnergy = 10;
		const FGameXXKEquipmentActiveEffect Effect = MakeShanFourEffect();
		if (!Test.TestTrue(TEXT("Shanhe four-piece descriptor is authoritative"), FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)))
		{
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = OutRuntime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = Effect;
		RuntimeEffect.SourceCharacterId = WearerId;
		if (!Test.TestTrue(TEXT("Shanhe fixture validates"), GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error)))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}

	FGameXXKEquipmentBattleEffectRuntime* FindShanFour(FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.EquipmentEffects.FindByPredicate([](const FGameXXKEquipmentBattleEffectRuntime& Effect)
		{
			return Effect.ActiveEffect.EffectId == FName(TEXT("Set.ShanHe.4"));
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShanHeTerrainCostTest,
	"GameXXK.Equipment.ShanHe.TerrainCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShanHeTerrainCostTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShanHeSetRuntimeTest;
	for (const FName CardId : {
		FName(TEXT("Hero.Formation.GuanShiLuoZi")),
		FName(TEXT("Profession.FormationMaster.GuanShi")),
		FName(TEXT("Npc.YueBai.ShanHeCanTu"))})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, CardId, Runtime)) return false;
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(TEXT("terrain classification card exists"), Definition)) continue;
		FGameXXKCardPlayPreview Preview;
		FString Error;
		TestTrue(FString::Printf(TEXT("%s builds preview: %s"), *CardId.ToString(), *Error),
			GameXXKCardRules::BuildCardPlayPreview(Runtime, Runtime.Deck.Hand[0].InstanceId, Preview, &Error));
		TestEqual(TEXT("first wearer terrain card previews one less Energy"), Preview.EffectiveEnergyCost, FMath::Max(0, Definition->EnergyCost - 1));
		TestEqual(TEXT("terrain preview exposes the exact Shanhe four-piece"), Preview.AppliedShanHeFourPieceEffectId, FName(TEXT("Set.ShanHe.4")));
	}

	FGameXXKCardBattleRuntime AttackRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.JianYiGuanHong"), AttackRuntime)) return false;
	FGameXXKCardPlayPreview AttackPreview;
	FString Error;
	TestTrue(TEXT("ordinary attack builds preview"), GameXXKCardRules::BuildCardPlayPreview(AttackRuntime, AttackRuntime.Deck.Hand[0].InstanceId, AttackPreview, &Error));
	TestEqual(TEXT("ordinary attack keeps its printed Energy"), AttackPreview.EffectiveEnergyCost, 2);
	TestTrue(TEXT("ordinary attack does not select Shanhe"), AttackPreview.AppliedShanHeFourPieceEffectId.IsNone());

	FGameXXKCardBattleRuntime TemporaryRuntime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.GuanShi"), TemporaryRuntime)) return false;
	FGameXXKCardInstance& Temporary = TemporaryRuntime.Deck.Hand[0];
	Temporary.bTemporary = true;
	Temporary.EnergyCostOverride = 2;
	Temporary.ManaCostOverride = 0;
	Temporary.ExpireAfterPlayerRound = TemporaryRuntime.RoundNumber;
	FGameXXKCardPlayPreview TemporaryPreview;
	TestTrue(TEXT("temporary terrain copy builds preview"), GameXXKCardRules::BuildCardPlayPreview(TemporaryRuntime, Temporary.InstanceId, TemporaryPreview, &Error));
	TestEqual(TEXT("temporary actively played terrain copy receives Shanhe discount"), TemporaryPreview.EffectiveEnergyCost, 1);
	TestEqual(TEXT("temporary terrain copy identifies Shanhe"), TemporaryPreview.AppliedShanHeFourPieceEffectId, FName(TEXT("Set.ShanHe.4")));

	FGameXXKCardBattleRuntime CommitRuntime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.GuanShi"), CommitRuntime)) return false;
	FGameXXKCardPlayPreview FirstPreview;
	TestTrue(TEXT("first terrain play previews"), GameXXKCardRules::BuildCardPlayPreview(CommitRuntime, CommitRuntime.Deck.Hand[0].InstanceId, FirstPreview, &Error));
	FGameXXKCardPlayResult FirstResult;
	TestTrue(FString::Printf(TEXT("first terrain play resolves: %s"), *Error), GameXXKCardRules::ResolveCardPlay(CommitRuntime, CommitRuntime.Deck.Hand[0].InstanceId, NAME_None, FirstResult, &Error));
	FGameXXKEquipmentBattleEffectRuntime* CommittedEffect = FindShanFour(CommitRuntime);
	TestEqual(TEXT("successful first terrain play consumes Shanhe in this round"), CommittedEffect ? CommittedEffect->CurrentRoundTriggerCount : INDEX_NONE, 1);
	FGameXXKCardPlayPreview SecondPreview;
	TestTrue(TEXT("second terrain play previews"), GameXXKCardRules::BuildCardPlayPreview(CommitRuntime, CommitRuntime.Deck.Hand[0].InstanceId, SecondPreview, &Error));
	TestEqual(TEXT("second terrain card returns to printed cost"), SecondPreview.EffectiveEnergyCost, 1);
	TestTrue(TEXT("second terrain card has no Shanhe discount ID"), SecondPreview.AppliedShanHeFourPieceEffectId.IsNone());

	FGameXXKCardBattleRuntime FailedRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Formation.GuanShiLuoZi"), FailedRuntime)) return false;
	FGameXXKCardPlayResult FailedResult;
	TestFalse(TEXT("illegal target rejects terrain play"), GameXXKCardRules::ResolveCardPlay(FailedRuntime, FailedRuntime.Deck.Hand[0].InstanceId, TEXT("Missing.Target"), FailedResult, &Error));
	TestEqual(TEXT("failed terrain play leaves Shanhe unused"), FindShanFour(FailedRuntime)->CurrentRoundTriggerCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKShanHePostTerrainRewardTest,
	"GameXXK.Equipment.ShanHe.PostTerrainReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKShanHePostTerrainRewardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKShanHeSetRuntimeTest;
	FString Error;

	FGameXXKCardBattleRuntime ImmediateRuntime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.GuanShi"), ImmediateRuntime, EGameXXKCharacterRole::FormationMaster, 8)
		|| !AddShanTwoEffect(*this, ImmediateRuntime))
	{
		return false;
	}
	ImmediateRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana = 10;
	const int32 ImmediateHandBefore = ImmediateRuntime.Deck.Hand.Num();
	FGameXXKCardPlayResult ImmediateResult;
	TestTrue(FString::Printf(TEXT("immediate terrain reward card resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(
			ImmediateRuntime,
			ImmediateRuntime.Deck.Hand[0].InstanceId,
			NAME_None,
			ImmediateResult,
			&Error));
	TestEqual(TEXT("Shanhe two-piece replaces the played card with one draw"), ImmediateRuntime.Deck.Hand.Num(), ImmediateHandBefore);
	const FGameXXKCardCombatUnit* ImmediateOwner = ImmediateRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == WearerId; });
	const FGameXXKCardCombatUnit* ImmediateAlly = ImmediateRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; });
	TestEqual(TEXT("Shanhe four-piece restores two Mana only to other allies"), ImmediateAlly ? ImmediateAlly->Mana : INDEX_NONE, 12);
	TestEqual(TEXT("Shanhe four-piece does not restore its owner"), ImmediateOwner ? ImmediateOwner->Mana : INDEX_NONE, 50);
	TestEqual(TEXT("immediate Shanhe two-piece marks one use"), ImmediateRuntime.EquipmentEffects[1].CurrentRoundTriggerCount, 1);

	FGameXXKCardBattleRuntime ChoiceRuntime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.BaMenLunZhuan"), ChoiceRuntime, EGameXXKCharacterRole::FormationMaster, 10)
		|| !AddShanTwoEffect(*this, ChoiceRuntime))
	{
		return false;
	}
	ChoiceRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana = 10;
	FGameXXKCardPlayResult ChoiceResult;
	TestTrue(FString::Printf(TEXT("choice-opening terrain card resolves: %s"), *Error),
		GameXXKCardRules::ResolveCardPlay(
			ChoiceRuntime,
			ChoiceRuntime.Deck.Hand[0].InstanceId,
			NAME_None,
			ChoiceResult,
			&Error));
	TestEqual(TEXT("base forced-discard choice opens before Shanhe reward"), ChoiceRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	const int32 HandBeforeChoice = ChoiceRuntime.Deck.Hand.Num();
	TestEqual(TEXT("other ally receives no Mana before base choice resolves"), ChoiceRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana, 10);
	const FName DiscardId = ChoiceRuntime.Deck.PendingChoice.Candidates[0].InstanceId;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(FString::Printf(TEXT("base forced discard resolves: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(ChoiceRuntime, {DiscardId}, &Error, &ResumedResults));
	TestEqual(TEXT("Shanhe draw happens after discard and restores the prior hand count"), ChoiceRuntime.Deck.Hand.Num(), HandBeforeChoice);
	TestEqual(TEXT("Shanhe Mana happens after the base choice"), ChoiceRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana, 12);
	TestEqual(TEXT("owner still receives no Shanhe Mana after choice"), ChoiceRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == WearerId; })->Mana, 50);

	FGameXXKCardBattleRuntime AutomaticRuntime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.GuanShi"), AutomaticRuntime, EGameXXKCharacterRole::FormationMaster, 8)
		|| !AddShanTwoEffect(*this, AutomaticRuntime))
	{
		return false;
	}
	AutomaticRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana = 10;
	FGameXXKCardPlayResult AutomaticTerrainResult;
	TestTrue(TEXT("automatic round-start terrain resolves"), GameXXKCardRules::ResolveRoundStartTerrainBenefits(AutomaticRuntime, AutomaticTerrainResult, &Error));
	TestEqual(TEXT("automatic terrain does not consume Shanhe two-piece"), AutomaticRuntime.EquipmentEffects[1].CurrentRoundTriggerCount, 0);
	TestEqual(TEXT("automatic terrain does not grant Shanhe four-piece Mana"), AutomaticRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == AllyId; })->Mana, 10);
	return true;
}

#endif
