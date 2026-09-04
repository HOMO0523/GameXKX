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

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		FGameXXKCardBattleRuntime& OutRuntime,
		const EGameXXKCharacterRole WearerRole = EGameXXKCharacterRole::FormationMaster)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 5; ++Index)
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

#endif
