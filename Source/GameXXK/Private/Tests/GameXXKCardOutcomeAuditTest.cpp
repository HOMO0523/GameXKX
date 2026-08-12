#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentSetCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardRulesTestBridge
{
	bool IsMedicineReverseDamage(const FGameXXKCardDamageResult& Result, FName OwnerUnitId);
}

namespace GameXXKCardOutcomeAuditTest
{
	const FName OwnerUnitId(TEXT("Outcome.Owner"));
	const FName AllyUnitId(TEXT("Outcome.Ally"));
	const FName EnemyAUnitId(TEXT("Outcome.EnemyA"));
	const FName EnemyBUnitId(TEXT("Outcome.EnemyB"));
	const FName MedicineCardInstanceId(TEXT("Outcome.HuiChun"));
	const FName MedicineCardId(TEXT("Hero.Healer.HuiChunNiMai"));
	const FName FlatReverseCardId(TEXT("Profession.Healer.LingZhiXuMing"));
	const FName BelowThresholdCardId(TEXT("Profession.Healer.CaoMuFuZhi"));
	const FName FirstHealingFormulaCardId(TEXT("Profession.Healer.CaoMuFuZhi"));
	const FName LargeHealingFormulaCardId(TEXT("Profession.Healer.WenYangGao"));
	const FName WhiteApeDefinitionId(TEXT("Enemy.Ch3.WhiteApe"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName CardId)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = MedicineCardInstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = TEXT("Outcome.Source.HuiChun");
		Card.AcquisitionOrdinal = 0;
		return Card;
	}

	FGameXXKCardInstance MakeNamedCard(
		const FName InstanceId,
		const FName CardId,
		const FName OwnerId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Outcome.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const int32 Seed,
		const FName CardId = MedicineCardId,
		const EGameXXKCharacterRole OwnerRole = EGameXXKCharacterRole::Hero,
		const EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Plain)
	{
		const FGameXXKCardInstance TriggerCard = MakeCard(CardId);
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, OwnerRole, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			{TriggerCard},
			Units,
			Terrain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand = {TriggerCard};
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool BuildExactRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const EGameXXKCharacterRole OwnerRole,
		const int32 Seed)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, OwnerRole, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
			MakeUnit(EnemyAUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBUnitId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		Units[2].HP = 1000;
		Units[2].MaxHP = 1000;
		Units[3].HP = 1000;
		Units[3].MaxHP = 1000;
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome exact runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome exact fixture is invalid: %s"), *Error));
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

	int32 Status(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId,
		const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool InstallFormula(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName FormulaCardId)
	{
		const FGameXXKCardDefinition* FormulaDefinition = FGameXXKCardCatalog::FindCardDefinition(FormulaCardId);
		if (!FormulaDefinition || FormulaDefinition->HealerRule.FormulaKind == EGameXXKHealerFormulaKind::None)
		{
			Test.AddError(FString::Printf(TEXT("outcome audit formula source is missing: %s"), *FormulaCardId.ToString()));
			return false;
		}
		FGameXXKHealerFormulaRuntime& Formula = Runtime.HealerFormulas.AddDefaulted_GetRef();
		Formula.OwnerUnitId = OwnerUnitId;
		Formula.SourceCardId = FormulaDefinition->Id;
		Formula.Kind = FormulaDefinition->HealerRule.FormulaKind;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome audit formula fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool InstallQingNangFourPiece(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime)
	{
		const FGameXXKEquipmentSetBonusDefinition* Definition =
			FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.QingNang.4"));
		if (!Definition)
		{
			Test.AddError(TEXT("outcome audit requires the catalog QingNang four-piece definition"));
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = Runtime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.SourceCharacterId = OwnerUnitId;
		RuntimeEffect.ActiveEffect.EffectId = Definition->Id;
		RuntimeEffect.ActiveEffect.SourceCharacterId = OwnerUnitId;
		RuntimeEffect.ActiveEffect.Set = Definition->Set;
		RuntimeEffect.ActiveEffect.RequiredPieces = Definition->RequiredPieces;
		RuntimeEffect.ActiveEffect.Scope = Definition->Scope;
		RuntimeEffect.ActiveEffect.Hook = Definition->Hook;
		RuntimeEffect.ActiveEffect.ModifierKind = static_cast<EGameXXKEquipmentModifierKind>(39);
		RuntimeEffect.ActiveEffect.Magnitude = Definition->Value;
		RuntimeEffect.ActiveEffect.Unit = Definition->Unit;
		RuntimeEffect.ActiveEffect.MaxTriggersPerRound = Definition->TriggersPerRound;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome QingNang fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool BuildIceArmorRewardRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		const TArray<FName> CardIds = {
			TEXT("Profession.Sorcerer.LingYanLianDan"),
			TEXT("Profession.Sorcerer.JuLing"),
			TEXT("Profession.Sorcerer.LiHuoYin"),
			TEXT("Profession.Sorcerer.FenMaiFu"),
			TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			Cards.Add(MakeNamedCard(
				FName(*FString::Printf(TEXT("Outcome.IceReward.%d"), Index)),
				CardIds[Index],
				OwnerUnitId,
				Index));
		}
		if (!BuildExactRuntime(Test, OutRuntime, Cards, EGameXXKCharacterRole::Sorcerer, 61050))
		{
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DiscardPile = Cards;

		FGameXXKSorcererPartnerTaskRuntime& Task = OutRuntime.SorcererPartnerTasks.AddDefaulted_GetRef();
		Task.bActive = true;
		Task.OwnerUnitId = OwnerUnitId;
		Task.LockedCardIds = CardIds;
		Task.CompletedCardIds = CardIds;
		Task.StarterReward = EGameXXKSorcererRewardRule::IceArmorDouble;
		Task.LockedBranch = EGameXXKSorcererTaskBranch::Ice;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardIds[Index]);
			if (!Definition)
			{
				Test.AddError(FString::Printf(TEXT("outcome Ice reward card is missing: %s"), *CardIds[Index].ToString()));
				return false;
			}
			FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder.AddDefaulted_GetRef();
			Snapshot.CardId = CardIds[Index];
			Snapshot.Quality = EGameXXKCardQuality::Common;
			Snapshot.OwnerUnitId = OwnerUnitId;
			Snapshot.SorcererSequencePosition = Index + 1;
			Snapshot.PreviousSorcererFamily = Index == 0
				? EGameXXKSorcererCardFamily::None
				: FGameXXKCardCatalog::FindCardDefinition(CardIds[Index - 1])->SorcererRule.Family;
			Snapshot.SorcererTaskBranch = EGameXXKSorcererTaskBranch::Ice;
		}
		OutRuntime.AutomaticResolutionQueue.bActive = true;
		OutRuntime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
		OutRuntime.AutomaticResolutionQueue.PendingCards = Task.FirstPlayOrder;
		OutRuntime.AutomaticResolutionQueue.NextCardIndex = Task.FirstPlayOrder.Num();
		OutRuntime.AutomaticResolutionQueue.PendingSorcererReward = Task.StarterReward;
		OutRuntime.AutomaticResolutionQueue.RewardOwnerUnitId = OwnerUnitId;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("outcome Ice reward fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool TestHealingPacket(
		FAutomationTestBase& Test,
		const FGameXXKCardHealingResult& Result,
		const FName SourceUnitId,
		const FName TargetUnitId,
		const int32 RequestedHealing,
		const int32 EffectiveHealing,
		const TCHAR* Label)
	{
		const FString Prefix(Label);
		bool bMatches = true;
		bMatches &= Test.TestEqual(Prefix + TEXT(" source"), Result.SourceUnitId, SourceUnitId);
		bMatches &= Test.TestEqual(Prefix + TEXT(" target"), Result.TargetUnitId, TargetUnitId);
		bMatches &= Test.TestEqual(Prefix + TEXT(" requested"), Result.RequestedHealing, RequestedHealing);
		bMatches &= Test.TestEqual(Prefix + TEXT(" effective"), Result.EffectiveHealing, EffectiveHealing);
		return bMatches;
	}

	bool TestArmorPacket(
		FAutomationTestBase& Test,
		const FGameXXKCardArmorResult& Result,
		const FName SourceUnitId,
		const FName TargetUnitId,
		const int32 RequestedArmor,
		const int32 EffectiveArmor,
		const TCHAR* Label)
	{
		const FString Prefix(Label);
		bool bMatches = true;
		bMatches &= Test.TestEqual(Prefix + TEXT(" source"), Result.SourceUnitId, SourceUnitId);
		bMatches &= Test.TestEqual(Prefix + TEXT(" target"), Result.TargetUnitId, TargetUnitId);
		bMatches &= Test.TestEqual(Prefix + TEXT(" requested"), Result.RequestedArmor, RequestedArmor);
		bMatches &= Test.TestEqual(Prefix + TEXT(" effective"), Result.EffectiveArmor, EffectiveArmor);
		return bMatches;
	}

	const FGameXXKHealerFormulaRuntime* FindFormula(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKHealerFormulaKind Kind)
	{
		return Runtime.HealerFormulas.FindByPredicate([Kind](const FGameXXKHealerFormulaRuntime& Formula)
		{
			return Formula.OwnerUnitId == OwnerUnitId && Formula.Kind == Kind;
		});
	}

	bool ResolveCard(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName TargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* ContextText)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(
			Runtime,
			MedicineCardInstanceId,
			TargetUnitId,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves through real card rules: %s"), ContextText, *Error), bResolved);
		return bResolved;
	}

	bool ApplySharedDirectDamage(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardDamageKind Kind,
		const FName TargetUnitId,
		FGameXXKCardDamageResult& OutResult,
		const TCHAR* ContextText)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = OwnerUnitId;
		Context.Kind = Kind;
		Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
		FString Error;
		const bool bApplied = GameXXKCardRules::ApplyPlayerCardDirectDamage(
			Runtime,
			Context,
			TargetUnitId,
			3,
			OutResult,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s uses the shared damage resolver: %s"), ContextText, *Error), bApplied);
		return bApplied;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomePositiveAuditTest,
	"GameXXK.Data.CardOutcomePreview.Audit.HealingAndArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomePositiveAuditTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomeAuditTest;

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61020, TEXT("Route.General.ZhiXueSan"))) return false;
		FindUnit(Runtime, AllyUnitId)->HP = 80;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("ordinary Heal"))) return false;
		if (!TestEqual(TEXT("ordinary Heal emits one packet"), Result.HealingResults.Num(), 1)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, AllyUnitId, 12, 12, TEXT("ordinary Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61021, TEXT("Route.General.ZhiXueSan"))) return false;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("full-health positive Heal"))) return false;
		if (!TestEqual(TEXT("a full-health positive attempt still emits exactly one healing packet"), Result.HealingResults.Num(), 1)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, AllyUnitId, 12, 0, TEXT("full-health positive Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61022)) return false;
		FindUnit(Runtime, AllyUnitId)->HP = 80;
		TestEqual(TEXT("Medicine ally fixture adds five Medicine"),
			GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, OwnerUnitId), EGameXXKCardStatus::Medicine, 5), 5);
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("Medicine ally Heal"))) return false;
		if (!TestEqual(TEXT("Medicine ally Heal emits one packet"), Result.HealingResults.Num(), 1)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, AllyUnitId, 15, 15, TEXT("Medicine ally Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61023, TEXT("Hero.Healer.BaiCaoJiZhen"), EGameXXKCharacterRole::Healer)) return false;
		FindUnit(Runtime, OwnerUnitId)->HP = 80;
		FindUnit(Runtime, AllyUnitId)->HP = 90;
		TestEqual(TEXT("Medicine party fixture adds four Medicine"),
			GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, OwnerUnitId), EGameXXKCardStatus::Medicine, 4), 4);
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, NAME_None, Result, TEXT("Medicine party Heal"))) return false;
		if (!TestEqual(TEXT("Medicine party Heal emits one packet per living ally"), Result.HealingResults.Num(), 2)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, OwnerUnitId, 10, 10, TEXT("Medicine party owner Heal"));
		TestHealingPacket(*this, Result.HealingResults[1], OwnerUnitId, AllyUnitId, 10, 10, TEXT("Medicine party ally Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61024, FlatReverseCardId, EGameXXKCharacterRole::Healer)) return false;
		FindUnit(Runtime, AllyUnitId)->HP = 20;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("flat Medicine ally Heal"))) return false;
		if (!TestEqual(TEXT("flat Medicine ally path emits its base and conditional packets"), Result.HealingResults.Num(), 2)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, AllyUnitId, 10, 10, TEXT("flat Medicine base Heal"));
		TestHealingPacket(*this, Result.HealingResults[1], OwnerUnitId, AllyUnitId, 2, 2, TEXT("flat Medicine conditional Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61025, TEXT("Hero.Generic.JianYiGuanHong"))
			|| !InstallQingNangFourPiece(*this, Runtime)) return false;
		FindUnit(Runtime, OwnerUnitId)->HP = 100;
		FindUnit(Runtime, AllyUnitId)->HP = 50;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, EnemyAUnitId, Result, TEXT("QingNang party health cycle"))) return false;
		if (!TestEqual(TEXT("QingNang records both positive two-point attempts"), Result.HealingResults.Num(), 2)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, OwnerUnitId, 2, 1, TEXT("QingNang owner Heal"));
		TestHealingPacket(*this, Result.HealingResults[1], OwnerUnitId, AllyUnitId, 2, 2, TEXT("QingNang ally Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61026, TEXT("Hero.Generic.JianYiGuanHong"), EGameXXKCharacterRole::Healer)
			|| !InstallFormula(*this, Runtime, TEXT("Profession.Healer.XingQiZhen"))) return false;
		FindUnit(Runtime, OwnerUnitId)->HP = 50;
		FindUnit(Runtime, AllyUnitId)->HP = 50;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, EnemyAUnitId, Result, TEXT("formula party Heal"))) return false;
		if (!TestEqual(TEXT("formula party Heal records one packet per living ally"), Result.HealingResults.Num(), 2)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, OwnerUnitId, 2, 2, TEXT("formula party owner Heal"));
		TestHealingPacket(*this, Result.HealingResults[1], OwnerUnitId, AllyUnitId, 2, 2, TEXT("formula party ally Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61027, TEXT("Profession.Blade.YinXueDao"), EGameXXKCharacterRole::Blade)) return false;
		FGameXXKCardCombatUnit* Owner = FindUnit(Runtime, OwnerUnitId);
		Owner->HP = 50;
		Owner->MaxHP = 100;
		TestEqual(TEXT("Blade healing fixture adds four Bleed"),
			GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyAUnitId), EGameXXKCardStatus::Bleed, 4), 4);
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, EnemyAUnitId, Result, TEXT("Blade triggered-Bleed Heal"))) return false;
		if (!TestEqual(TEXT("Blade triggered-Bleed Heal emits one packet"), Result.HealingResults.Num(), 1)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, OwnerUnitId, 4, 4, TEXT("Blade triggered-Bleed Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61028, TEXT("Profession.FormationMaster.LinYingMiZong"), EGameXXKCharacterRole::FormationMaster)) return false;
		FindUnit(Runtime, OwnerUnitId)->HP = 80;
		FindUnit(Runtime, AllyUnitId)->HP = 80;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, NAME_None, Result, TEXT("Forest terrain Heal"))) return false;
		if (!TestEqual(TEXT("Forest terrain records one Heal per living ally"), Result.HealingResults.Num(), 2)) return false;
		TestHealingPacket(*this, Result.HealingResults[0], OwnerUnitId, OwnerUnitId, 4, 4, TEXT("Forest owner Heal"));
		TestHealingPacket(*this, Result.HealingResults[1], OwnerUnitId, AllyUnitId, 4, 4, TEXT("Forest ally Heal"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61029, TEXT("Hero.Generic.HengJianShouShi"))) return false;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("ordinary AddArmor"))) return false;
		if (!TestEqual(TEXT("ordinary AddArmor emits one packet"), Result.ArmorResults.Num(), 1)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, AllyUnitId, 16, 16, TEXT("ordinary AddArmor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61030, TEXT("Hero.Generic.HengJianShouShi"))) return false;
		FindUnit(Runtime, AllyUnitId)->Armor = 99;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("armor-cap positive AddArmor"))) return false;
		if (!TestEqual(TEXT("an armor-cap positive attempt still emits exactly one packet"), Result.ArmorResults.Num(), 1)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, AllyUnitId, 16, 0, TEXT("armor-cap positive AddArmor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61031, TEXT("Hero.Mage.HanXuNingChuan"), EGameXXKCharacterRole::Sorcerer)) return false;
		FGameXXKCardCombatUnit* Owner = FindUnit(Runtime, OwnerUnitId);
		Owner->Mana = 20;
		Owner->MaxMana = 20;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, NAME_None, Result, TEXT("current Mana and overflow Armor"))) return false;
		if (!TestEqual(TEXT("current Mana and real overflow emit two armor packets"), Result.ArmorResults.Num(), 2)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, OwnerUnitId, 5, 5, TEXT("current Mana Armor"));
		TestArmorPacket(*this, Result.ArmorResults[1], OwnerUnitId, OwnerUnitId, 6, 6, TEXT("Mana overflow Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61032, TEXT("Hero.Mage.HanXuNingChuan"), EGameXXKCharacterRole::Sorcerer)) return false;
		FGameXXKCardCombatUnit* Owner = FindUnit(Runtime, OwnerUnitId);
		Owner->Mana = 10;
		Owner->MaxMana = 20;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, NAME_None, Result, TEXT("unmet Mana overflow"))) return false;
		if (!TestEqual(TEXT("zero overflow creates no fake armor packet"), Result.ArmorResults.Num(), 1)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, OwnerUnitId, 2, 2, TEXT("nonzero current Mana Armor"));
	}

	for (const TPair<FName, int32>& TerrainCase : {
		TPair<FName, int32>(TEXT("Profession.FormationMaster.DingZhen"), 4),
		TPair<FName, int32>(TEXT("Profession.FormationMaster.KunZhen"), 8)})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61033 + TerrainCase.Value, TerrainCase.Key, EGameXXKCharacterRole::FormationMaster)) return false;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, NAME_None, Result, TEXT("terrain Armor"))) return false;
		if (!TestEqual(TEXT("terrain Armor records one packet per living ally"), Result.ArmorResults.Num(), 2)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, OwnerUnitId, TerrainCase.Value, TerrainCase.Value, TEXT("terrain owner Armor"));
		TestArmorPacket(*this, Result.ArmorResults[1], OwnerUnitId, AllyUnitId, TerrainCase.Value, TerrainCase.Value, TEXT("terrain ally Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61042, TEXT("Route.General.ZhiXueSan"), EGameXXKCharacterRole::Healer)
			|| !InstallFormula(*this, Runtime, TEXT("Profession.Healer.ZhiXueCao"))) return false;
		FindUnit(Runtime, AllyUnitId)->HP = 80;
		TestEqual(TEXT("formula party-Armor fixture adds one Bleed"),
			GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, AllyUnitId), EGameXXKCardStatus::Bleed, 1), 1);
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("formula party Armor"))) return false;
		if (!TestEqual(TEXT("formula party Armor records one packet per living ally"), Result.ArmorResults.Num(), 2)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, OwnerUnitId, 2, 2, TEXT("formula party owner Armor"));
		TestArmorPacket(*this, Result.ArmorResults[1], OwnerUnitId, AllyUnitId, 2, 2, TEXT("formula party ally Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61043, TEXT("Route.General.ZhiXueSan"), EGameXXKCharacterRole::Healer)
			|| !InstallFormula(*this, Runtime, LargeHealingFormulaCardId)) return false;
		FindUnit(Runtime, AllyUnitId)->HP = 80;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, AllyUnitId, Result, TEXT("formula target Armor"))) return false;
		if (!TestEqual(TEXT("formula target Armor emits one packet"), Result.ArmorResults.Num(), 1)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, AllyUnitId, 4, 4, TEXT("formula target Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildIceArmorRewardRuntime(*this, Runtime)) return false;
		FindUnit(Runtime, OwnerUnitId)->Armor = 4;
		TArray<FGameXXKCardPlayResult> Results;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("Ice armor reward resolves: %s"), *Error),
			GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error))) return false;
		if (!TestEqual(TEXT("Ice armor reward emits one aggregated result"), Results.Num(), 1)) return false;
		if (!TestEqual(TEXT("Ice armor reward records both party armor attempts"), Results[0].ArmorResults.Num(), 2)) return false;
		TestArmorPacket(*this, Results[0].ArmorResults[0], OwnerUnitId, OwnerUnitId, 6, 6, TEXT("Ice reward owner Armor"));
		TestArmorPacket(*this, Results[0].ArmorResults[1], OwnerUnitId, AllyUnitId, 6, 6, TEXT("Ice reward ally Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61044, TEXT("Profession.Healer.YaoYin"), EGameXXKCharacterRole::Healer)) return false;
		FGameXXKCardCombatUnit* WhiteApe = FindUnit(Runtime, EnemyAUnitId);
		WhiteApe->EnemyDefinitionId = WhiteApeDefinitionId;
		WhiteApe->CombatLevel = 1;
		FGameXXKEnemyBattleState& EnemyState = Runtime.EnemyStates.FindOrAdd(EnemyAUnitId);
		EnemyState.DefinitionId = WhiteApeDefinitionId;
		EnemyState.bFirstStatusPassiveAvailable = true;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("White Ape audit fixture validates: %s"), *Error),
			GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))) return false;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, EnemyAUnitId, Result, TEXT("White Ape first negative status"))) return false;
		if (!TestEqual(TEXT("White Ape first negative status emits one enemy armor packet"), Result.ArmorResults.Num(), 1)) return false;
		TestArmorPacket(*this, Result.ArmorResults[0], OwnerUnitId, EnemyAUnitId, 8, 8, TEXT("White Ape status guard Armor"));
	}

	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Runtime, 61045, FlatReverseCardId, EGameXXKCharacterRole::Healer)) return false;
		FGameXXKCardPlayResult Result;
		if (!ResolveCard(*this, Runtime, EnemyAUnitId, Result, TEXT("unmet positive-effect branches"))) return false;
		TestTrue(TEXT("enemy reversal creates no fake healing packets"), Result.HealingResults.IsEmpty());
		TestTrue(TEXT("unmet ally-only armor creates no fake armor packets"), Result.ArmorResults.IsEmpty());
	}

	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeNamedCard(TEXT("Outcome.Rollback.YinXue"), TEXT("Profession.Blade.YinXueDao"), OwnerUnitId, 0),
			MakeNamedCard(TEXT("Outcome.Rollback.Consumer"), TEXT("Hero.Guard.XuanJiaZhenYue"), OwnerUnitId, 1)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildExactRuntime(*this, Runtime, Cards, EGameXXKCharacterRole::Blade, 61046)) return false;
		FindUnit(Runtime, OwnerUnitId)->Armor = 10;
		FGameXXKCardPlayResult Result;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("rollback setup Yin Xue resolves: %s"), *Error),
			GameXXKCardRules::ResolveCardPlay(Runtime, Cards[0].InstanceId, EnemyAUnitId, Result, &Error))) return false;
		Result = FGameXXKCardPlayResult();
		Error.Reset();
		if (!TestTrue(FString::Printf(TEXT("rollback armor consumer resolves: %s"), *Error),
			GameXXKCardRules::ResolveCardPlay(Runtime, Cards[1].InstanceId, OwnerUnitId, Result, &Error))) return false;
		TestEqual(TEXT("internal rollback restores all consumed armor"), FindUnit(Runtime, OwnerUnitId)->Armor, 10);
		TestTrue(TEXT("RestoreConsumedStatusesAndArmor emits no visible armor packet"), Result.ArmorResults.IsEmpty());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardOutcomeDamageAuditTest,
	"GameXXK.Data.CardOutcomePreview.Audit.DamageKindAndCause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardOutcomeDamageAuditTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardOutcomeAuditTest;

	FGameXXKCardBattleRuntime DirectRuntime;
	if (!BuildRuntime(*this, DirectRuntime, 61001))
	{
		return false;
	}
	FGameXXKCardDamageResult Single;
	if (!ApplySharedDirectDamage(
		*this,
		DirectRuntime,
		EGameXXKCardDamageKind::SingleTargetAttack,
		EnemyAUnitId,
		Single,
		TEXT("single-target packet")))
	{
		return false;
	}
	FGameXXKCardDamageResult Group;
	if (!ApplySharedDirectDamage(
		*this,
		DirectRuntime,
		EGameXXKCardDamageKind::GroupAttack,
		EnemyBUnitId,
		Group,
		TEXT("group packet")))
	{
		return false;
	}

	FGameXXKCardBattleRuntime PoisonRuntime;
	if (!BuildRuntime(*this, PoisonRuntime, 61002))
	{
		return false;
	}
	FGameXXKCardCombatUnit* PoisonTarget = FindUnit(PoisonRuntime, AllyUnitId);
	if (!TestNotNull(TEXT("poison fixture has its ally target"), PoisonTarget))
	{
		return false;
	}
	TestEqual(
		TEXT("poison fixture applies three real stacks"),
		GameXXKCardRules::AddCombatStatus(*PoisonTarget, EGameXXKCardStatus::Poison, 3),
		3);
	TArray<FGameXXKCardDamageResult> EndPhaseResults;
	FString EndPhaseError;
	if (!TestTrue(
		TEXT("player end phase resolves ordinary poison"),
		GameXXKCardRules::EndPlayerCardPhase(PoisonRuntime, EndPhaseResults, &EndPhaseError)))
	{
		AddError(EndPhaseError);
		return false;
	}
	const FGameXXKCardDamageResult* PoisonPacket = EndPhaseResults.FindByPredicate([](const FGameXXKCardDamageResult& Result)
	{
		return Result.Cause == EGameXXKCardDamageCause::Poison;
	});
	if (!TestNotNull(TEXT("ordinary poison emits a real damage packet"), PoisonPacket))
	{
		return false;
	}
	const FGameXXKCardDamageResult Poison = *PoisonPacket;

	FGameXXKCardBattleRuntime MedicineRuntime;
	if (!BuildRuntime(*this, MedicineRuntime, 61003))
	{
		return false;
	}
	FGameXXKCardCombatUnit* MedicineOwner = FindUnit(MedicineRuntime, OwnerUnitId);
	if (!TestNotNull(TEXT("medicine fixture has its card owner"), MedicineOwner))
	{
		return false;
	}
	TestEqual(
		TEXT("medicine fixture applies five real stacks"),
		GameXXKCardRules::AddCombatStatus(*MedicineOwner, EGameXXKCardStatus::Medicine, 5),
		5);
	FGameXXKCardPlayResult ReverseResult;
	FString ReverseError;
	if (!TestTrue(
		TEXT("real medicine card reverses healing against an enemy"),
		GameXXKCardRules::ResolveCardPlay(
			MedicineRuntime,
			MedicineCardInstanceId,
			EnemyAUnitId,
			ReverseResult,
			&ReverseError)))
	{
		AddError(ReverseError);
		return false;
	}
	if (!TestEqual(TEXT("medicine reverse emits one damage packet"), ReverseResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& Reverse = ReverseResult.DamageResults[0];

	TestEqual(TEXT("direct single keeps kind"), Single.Kind, EGameXXKCardDamageKind::SingleTargetAttack);
	TestEqual(TEXT("group packet keeps kind"), Group.Kind, EGameXXKCardDamageKind::GroupAttack);
	TestEqual(TEXT("ordinary poison keeps dot kind"), Poison.Kind, EGameXXKCardDamageKind::DamageOverTime);
	TestEqual(TEXT("medicine reverse has medicine cause"), Reverse.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("medicine source remains the card owner"), Reverse.SourceUnitId, OwnerUnitId);
	TestTrue(
		TEXT("real Medicine reverse is accepted by the Medicine classifier"),
		GameXXKCardRulesTestBridge::IsMedicineReverseDamage(Reverse, OwnerUnitId));

	FGameXXKCardBattleRuntime FlatRuntime;
	if (!BuildRuntime(*this, FlatRuntime, 61004, FlatReverseCardId))
	{
		return false;
	}
	FGameXXKCardCombatUnit* FlatOwner = FindUnit(FlatRuntime, OwnerUnitId);
	FGameXXKCardCombatUnit* FlatTarget = FindUnit(FlatRuntime, EnemyAUnitId);
	if (!TestNotNull(TEXT("flat reverse fixture has its owner"), FlatOwner)
		|| !TestNotNull(TEXT("flat reverse fixture has its enemy"), FlatTarget))
	{
		return false;
	}
	FlatTarget->HP = 30;
	FlatTarget->Defense = 40;
	FlatTarget->Armor = 50;
	TestEqual(
		TEXT("flat reverse fixture applies five Medicine"),
		GameXXKCardRules::AddCombatStatus(*FlatOwner, EGameXXKCardStatus::Medicine, 5),
		5);
	FGameXXKCardPlayResult FlatResult;
	if (!ResolveCard(*this, FlatRuntime, EnemyAUnitId, FlatResult, TEXT("flat reverse card")))
	{
		return false;
	}
	TestEqual(TEXT("flat reverse card emits primary and flat packets"), FlatResult.DamageResults.Num(), 2);
	const FGameXXKCardDamageResult* FlatReverse = FlatResult.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Result)
	{
		return Result.RequestedDamage == 2;
	});
	if (!TestNotNull(TEXT("flat reverse emits its real two-point packet"), FlatReverse))
	{
		return false;
	}
	TestEqual(TEXT("flat reverse has Medicine cause"), FlatReverse->Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("flat reverse source remains the card owner"), FlatReverse->SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("flat reverse loses exactly two health"), FlatReverse->HealthDamage, 2);
	TestEqual(TEXT("flat reverse absorbs no Armor"), FlatReverse->ArmorAbsorbed, 0);
	TestEqual(TEXT("flat reverse keeps enemy Armor"), FindUnit(FlatRuntime, EnemyAUnitId)->Armor, 50);
	TestEqual(TEXT("flat reverse card loses seventeen total enemy health"), FindUnit(FlatRuntime, EnemyAUnitId)->HP, 13);
	TestEqual(TEXT("flat reverse card consumes the Medicine snapshot once"), Status(FlatRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 0);

	FGameXXKCardBattleRuntime FirstFormulaRuntime;
	if (!BuildRuntime(*this, FirstFormulaRuntime, 61005)
		|| !InstallFormula(*this, FirstFormulaRuntime, FirstHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult FirstFormulaResult;
	if (!ResolveCard(*this, FirstFormulaRuntime, EnemyAUnitId, FirstFormulaResult, TEXT("first-healing formula enemy reverse")))
	{
		return false;
	}
	TestEqual(TEXT("enemy Medicine reverse triggers first-healing Medicine2"), Status(FirstFormulaRuntime, OwnerUnitId, EGameXXKCardStatus::Medicine), 2);
	const FGameXXKHealerFormulaRuntime* TriggeredFirstFormula = FindFormula(
		FirstFormulaRuntime,
		EGameXXKHealerFormulaKind::FirstHealingMedicine);
	if (!TestNotNull(TEXT("first-healing formula remains installed"), TriggeredFirstFormula))
	{
		return false;
	}
	TestEqual(TEXT("first-healing formula spends this round's budget"), TriggeredFirstFormula->LastTriggeredRound, FirstFormulaRuntime.RoundNumber);

	FGameXXKCardBattleRuntime EnvironmentalControlRuntime;
	if (!BuildRuntime(*this, EnvironmentalControlRuntime, 61006))
	{
		return false;
	}
	FGameXXKCardDamageContext EnvironmentalContext;
	EnvironmentalContext.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
	EnvironmentalContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult EnvironmentalResult;
	FString EnvironmentalError;
	if (!TestTrue(
		TEXT("non-Medicine environmental damage resolves through shared rules"),
		GameXXKCardRules::ApplyCombatDirectDamage(
			EnvironmentalControlRuntime.Units,
			EnvironmentalControlRuntime.GuardLinks,
			EnvironmentalContext,
			EnemyAUnitId,
			3,
			EnvironmentalResult,
			&EnvironmentalError)))
	{
		AddError(EnvironmentalError);
		return false;
	}
	TestEqual(TEXT("environmental control remains Environment"), EnvironmentalResult.Cause, EGameXXKCardDamageCause::Environment);
	TestFalse(
		TEXT("real environmental packet is rejected by the Medicine classifier"),
		GameXXKCardRulesTestBridge::IsMedicineReverseDamage(EnvironmentalResult, EnvironmentalResult.SourceUnitId));

	FGameXXKCardBattleRuntime BelowThresholdRuntime;
	if (!BuildRuntime(*this, BelowThresholdRuntime, 61007, BelowThresholdCardId)
		|| !InstallFormula(*this, BelowThresholdRuntime, LargeHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult BelowThresholdResult;
	if (!ResolveCard(*this, BelowThresholdRuntime, EnemyAUnitId, BelowThresholdResult, TEXT("below-threshold enemy reverse")))
	{
		return false;
	}
	if (!TestEqual(TEXT("below-threshold reverse emits one packet"), BelowThresholdResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& BelowThresholdDamage = BelowThresholdResult.DamageResults[0];
	TestEqual(TEXT("below-threshold reverse requests eight"), BelowThresholdDamage.RequestedDamage, 8);
	TestEqual(TEXT("below-threshold reverse keeps Medicine cause"), BelowThresholdDamage.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("below-threshold reverse keeps owner source"), BelowThresholdDamage.SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("below-threshold reverse grants no Vulnerability"), Status(BelowThresholdRuntime, EnemyAUnitId, EGameXXKCardStatus::Vulnerability), 0);
	const FGameXXKHealerFormulaRuntime* BelowThresholdFormula = FindFormula(
		BelowThresholdRuntime,
		EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability);
	if (!TestNotNull(TEXT("below-threshold formula remains installed"), BelowThresholdFormula))
	{
		return false;
	}
	TestEqual(TEXT("below-threshold reverse spends no formula budget"), BelowThresholdFormula->LastTriggeredRound, 0);

	FGameXXKCardBattleRuntime BoundaryRuntime;
	if (!BuildRuntime(*this, BoundaryRuntime, 61008)
		|| !InstallFormula(*this, BoundaryRuntime, LargeHealingFormulaCardId))
	{
		return false;
	}
	FGameXXKCardPlayResult BoundaryResult;
	if (!ResolveCard(*this, BoundaryRuntime, EnemyAUnitId, BoundaryResult, TEXT("ten-point boundary enemy reverse")))
	{
		return false;
	}
	if (!TestEqual(TEXT("ten-point boundary reverse emits one packet"), BoundaryResult.DamageResults.Num(), 1))
	{
		return false;
	}
	const FGameXXKCardDamageResult& BoundaryDamage = BoundaryResult.DamageResults[0];
	TestEqual(TEXT("ten-point boundary reverse requests ten"), BoundaryDamage.RequestedDamage, 10);
	TestEqual(TEXT("ten-point boundary reverse keeps Medicine cause"), BoundaryDamage.Cause, EGameXXKCardDamageCause::Medicine);
	TestEqual(TEXT("ten-point boundary reverse keeps owner source"), BoundaryDamage.SourceUnitId, OwnerUnitId);
	TestEqual(TEXT("ten-point boundary reverse grants Vulnerability1"), Status(BoundaryRuntime, EnemyAUnitId, EGameXXKCardStatus::Vulnerability), 1);
	const FGameXXKHealerFormulaRuntime* BoundaryFormula = FindFormula(
		BoundaryRuntime,
		EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability);
	if (!TestNotNull(TEXT("ten-point boundary formula remains installed"), BoundaryFormula))
	{
		return false;
	}
	TestEqual(TEXT("ten-point boundary reverse spends this round's formula budget"), BoundaryFormula->LastTriggeredRound, BoundaryRuntime.RoundNumber);
	return true;
}

#endif
