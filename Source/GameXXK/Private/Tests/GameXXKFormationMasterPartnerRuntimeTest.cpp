#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKFormationMasterPartnerRuntimeTest
{
	struct FExpectedEffect
	{
		EGameXXKCardEffectType Type = EGameXXKCardEffectType::Invalid;
		EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;
		int32 Magnitude = 0;
		EGameXXKCardStatus Status = EGameXXKCardStatus::None;
		EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;
	};

	struct FExpectedCard
	{
		const TCHAR* Id = nullptr;
		const TCHAR* DisplayName = nullptr;
		int32 EnergyCost = 0;
		int32 ManaCost = 0;
		EGameXXKCardTargetMode TargetMode = EGameXXKCardTargetMode::Invalid;
		TArray<FExpectedEffect> Effects;
		const TCHAR* ArchetypeId = nullptr;
	};

	const FGameXXKCardDefinition* RequireCard(FAutomationTestBase& Test, const TCHAR* CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FName(CardId));
		Test.TestNotNull(FString::Printf(TEXT("formation card exists: %s"), CardId), Definition);
		return Definition;
	}

	void TestCard(FAutomationTestBase& Test, const FExpectedCard& Expected, const bool bSwitch)
	{
		const FGameXXKCardDefinition* Definition = RequireCard(Test, Expected.Id);
		if (!Definition)
		{
			return;
		}
		const FString Prefix = FString::Printf(TEXT("%s"), Expected.Id);
		Test.TestEqual(Prefix + TEXT(" display name"), Definition->DisplayName.ToString(), FString(Expected.DisplayName));
		Test.TestEqual(Prefix + TEXT(" energy"), Definition->EnergyCost, Expected.EnergyCost);
		Test.TestEqual(Prefix + TEXT(" mana"), Definition->ManaCost, Expected.ManaCost);
		Test.TestEqual(Prefix + TEXT(" fixed target mode"), Definition->TargetSpec.Mode, Expected.TargetMode);
		Test.TestTrue(Prefix + TEXT(" has no terrain-driven target override"), Definition->TargetSpec.ModeOverrides.IsEmpty());
		Test.TestFalse(Prefix + TEXT(" is not a normal-role core card"), Definition->bCoreProfessionCard);
		Test.TestEqual(Prefix + TEXT(" effect count"), Definition->Effects.Num(), Expected.Effects.Num());
		for (int32 Index = 0; Index < FMath::Min(Definition->Effects.Num(), Expected.Effects.Num()); ++Index)
		{
			const FGameXXKCardEffect& Actual = Definition->Effects[Index];
			const FExpectedEffect& Wanted = Expected.Effects[Index];
			const FString EffectPrefix = FString::Printf(TEXT("%s effect %d"), Expected.Id, Index);
			Test.TestEqual(EffectPrefix + TEXT(" type"), Actual.Type, Wanted.Type);
			Test.TestEqual(EffectPrefix + TEXT(" target"), Actual.Target, Wanted.Target);
			Test.TestEqual(EffectPrefix + TEXT(" magnitude"), Actual.Magnitude, Wanted.Magnitude);
			Test.TestEqual(EffectPrefix + TEXT(" status"), Actual.Status, Wanted.Status);
			Test.TestEqual(EffectPrefix + TEXT(" terrain"), Actual.TerrainOverride, Wanted.Terrain);
		}
		if (bSwitch)
		{
			Test.TestTrue(Prefix + TEXT(" switch carries no benefit-archetype metadata"), Definition->ProfessionArchetypeIds.IsEmpty());
		}
		else
		{
			Test.TestEqual(Prefix + TEXT(" carries exactly one benefit archetype"), Definition->ProfessionArchetypeIds.Num(), 1);
			if (Definition->ProfessionArchetypeIds.Num() == 1)
			{
				Test.TestEqual(Prefix + TEXT(" benefit archetype"), Definition->ProfessionArchetypeIds[0], FName(Expected.ArchetypeId));
			}
		}
	}

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 Mana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = HP;
		Unit.MaxHP = Side == EGameXXKCardTargetSide::Enemy ? 1000 : 100;
		Unit.Attack = 20;
		Unit.Mana = Mana;
		Unit.MaxMana = 100;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeCards(const TCHAR* CardId)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Formation.Runtime.%s.%d"), CardId, Index));
			Card.CardId = FName(CardId);
			Card.CurrentQuality = EGameXXKCardQuality::Common;
			Card.OwnerUnitId = TEXT("Formation");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Formation.Entry.%s.%d"), CardId, Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TCHAR* CardId,
		const EGameXXKCardTerrain Terrain,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Formation"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 80, 100, 1),
			MakeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 80, 0, 2),
			MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1000, 0, 10)};
		FString Error;
		const bool bInitialized = GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeCards(CardId),
			Units,
			Terrain,
			Seed,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("%s runtime initializes: %s"), CardId, *Error), bInitialized);
		return bInitialized;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterCatalogTest,
	"GameXXK.Data.PartnerCards.Formation.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterCatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterPartnerRuntimeTest;
	const TArray<FGameXXKCardDefinition> Definitions =
		FGameXXKCardCatalog::GetCardDefinitionsForOwner(TEXT("Profession.FormationMaster"));
	TestEqual(TEXT("formation partner catalog contains six switches plus twelve benefits"), Definitions.Num(), 18);

	const TArray<FExpectedCard> Switches = {
		{TEXT("Profession.FormationMaster.GuanShi"), TEXT("平野观势"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::Plain},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1}}},
		{TEXT("Profession.FormationMaster.JieShanWeiZhang"), TEXT("借山为障"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::Cliff},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1}}},
		{TEXT("Profession.FormationMaster.LinYingMiZong"), TEXT("林影迷踪"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::Forest},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}},
		{TEXT("Profession.FormationMaster.YinShuiHuiYuan"), TEXT("引水回元"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::WaterShore},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}},
		{TEXT("Profession.FormationMaster.DingZhen"), TEXT("定阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::Village},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}},
		{TEXT("Profession.FormationMaster.KunZhen"), TEXT("困阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardTerrain::Cave},
			 {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}}
	};
	for (const FExpectedCard& Expected : Switches)
	{
		TestCard(*this, Expected, true);
	}

	const TArray<FExpectedCard> Benefits = {
		{TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("回声震杀"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 240}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1}}, TEXT("Archetype.Formation.Assault")},
		{TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("镇煞阵"), 3, 10, EGameXXKCardTargetMode::AllEnemies,
			{{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::AllEnemies, 320}, {EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Vulnerability}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Assault")},
		{TEXT("Profession.FormationMaster.ShanMenFengSuo"), TEXT("山门封锁"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1}}, TEXT("Archetype.Formation.Assault")},

		{TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("村寨援阵"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 12}, {EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Support")},
		{TEXT("Profession.FormationMaster.ShuiJingZheGuang"), TEXT("水镜折光"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16}, {EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 2}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Support")},
		{TEXT("Profession.FormationMaster.LinFengFuZhen"), TEXT("林风拂阵"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Support")},

		{TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("易位阵"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility}, {EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Cycle")},
		{TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("八门轮转"), 2, 0, EGameXXKCardTargetMode::Self,
			{{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3}, {EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}, {EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Cycle")},
		{TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("阵旗鼓舞"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::ApplyBattleModifier, EGameXXKCardEffectTarget::AllAllies, 0}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Cycle")},

		{TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("万象归阵"), 3, 14, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 40}, {EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3}, {EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextTerrainCardFree}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1}}, TEXT("Archetype.Formation.Convergence")},
		{TEXT("Profession.FormationMaster.DiMaiJieLi"), TEXT("地脉借力"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 200}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 2}}, TEXT("Archetype.Formation.Convergence")},
		{TEXT("Profession.FormationMaster.SiXiangLianHuan"), TEXT("四象连环"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 24}, {EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3}, {EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 2}}, TEXT("Archetype.Formation.Convergence")}
	};
	for (const FExpectedCard& Expected : Benefits)
	{
		TestCard(*this, Expected, false);
	}

	if (const FGameXXKCardDefinition* ZhenQi = RequireCard(*this, TEXT("Profession.FormationMaster.ZhenQiGuWu")))
	{
		TestEqual(TEXT("阵旗鼓舞 grants each ally's next attack twenty percent"), ZhenQi->Effects[0].Modifier.Magnitude, 20);
		TestEqual(TEXT("阵旗鼓舞 applies on next attack"), ZhenQi->Effects[0].Modifier.Trigger, EGameXXKCardBattleModifierTrigger::OnNextAttack);
		TestEqual(TEXT("阵旗鼓舞 applies once per ally"), ZhenQi->Effects[0].Modifier.RemainingTriggers, 1);
		TestEqual(TEXT("阵旗鼓舞 covers all allies"), ZhenQi->Effects[0].Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::AllAllies);
	}

	FString ValidationError;
	TestTrue(FString::Printf(TEXT("the complete 198-card catalog remains valid: %s"), *ValidationError), FGameXXKCardCatalog::ValidateCardDefinitions(ValidationError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterSwitchRuntimeTest,
	"GameXXK.Data.PartnerCards.Formation.SwitchRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterSwitchRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterPartnerRuntimeTest;
	struct FSwitchCase
	{
		const TCHAR* CardId;
		EGameXXKCardTerrain Destination;
		EGameXXKCardTargetMode TargetMode;
	};
	const TArray<FSwitchCase> Cases = {
		{TEXT("Profession.FormationMaster.GuanShi"), EGameXXKCardTerrain::Plain, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Profession.FormationMaster.JieShanWeiZhang"), EGameXXKCardTerrain::Cliff, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Profession.FormationMaster.LinYingMiZong"), EGameXXKCardTerrain::Forest, EGameXXKCardTargetMode::AllAllies},
		{TEXT("Profession.FormationMaster.YinShuiHuiYuan"), EGameXXKCardTerrain::WaterShore, EGameXXKCardTargetMode::AllAllies},
		{TEXT("Profession.FormationMaster.DingZhen"), EGameXXKCardTerrain::Village, EGameXXKCardTargetMode::AllAllies},
		{TEXT("Profession.FormationMaster.KunZhen"), EGameXXKCardTerrain::Cave, EGameXXKCardTargetMode::AllAllies}};

	int32 Seed = 64000;
	for (const FSwitchCase& Case : Cases)
	{
		for (const bool bStartsOnDestination : {false, true})
		{
			const EGameXXKCardTerrain StartTerrain = bStartsOnDestination
				? Case.Destination
				: (Case.Destination == EGameXXKCardTerrain::Plain ? EGameXXKCardTerrain::Cave : EGameXXKCardTerrain::Plain);
			FGameXXKCardBattleRuntime Runtime;
			if (!BuildRuntime(*this, Case.CardId, StartTerrain, Seed++, Runtime))
			{
				continue;
			}
			Unit(Runtime, TEXT("Formation"))->Mana = 0;
			const FName PlayedInstanceId = Runtime.Deck.Hand[0].InstanceId;
			FGameXXKCardPlayPreview Preview;
			FString Error;
			TestTrue(FString::Printf(TEXT("%s builds a preview: %s"), Case.CardId, *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, PlayedInstanceId, Preview, &Error));
			TestEqual(FString::Printf(TEXT("%s keeps its fixed target mode"), Case.CardId), Preview.TargetRequest.EffectiveMode, Case.TargetMode);
			const FName TargetId = Case.TargetMode == EGameXXKCardTargetMode::SingleEnemy ? FName(TEXT("Enemy")) : NAME_None;
			FGameXXKCardPlayResult Result;
			Error.Reset();
			const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, TargetId, Result, &Error);
			TestTrue(FString::Printf(TEXT("%s resolves from terrain %d: %s"), Case.CardId, static_cast<int32>(StartTerrain), *Error), bResolved);
			if (!bResolved)
			{
				continue;
			}
			const FString Context = FString::Printf(TEXT("%s start=%d"), Case.CardId, static_cast<int32>(StartTerrain));
			TestEqual(Context + TEXT(" lands on its declared terrain"), Runtime.Terrain, Case.Destination);
			TestEqual(Context + TEXT(" reports a real change only when the destination differed"), Runtime.bTerrainChangedThisRound, !bStartsOnDestination);
			TestEqual(Context + TEXT(" pays exactly one shared Energy"), Runtime.Deck.SharedEnergy, 2);
			TestFalse(Context + TEXT(" moves the played instance out of hand"), Runtime.Deck.Hand.ContainsByPredicate([PlayedInstanceId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == PlayedInstanceId;
			}));

			FGameXXKCardCombatUnit* Formation = Unit(Runtime, TEXT("Formation"));
			FGameXXKCardCombatUnit* Ally = Unit(Runtime, TEXT("Ally"));
			FGameXXKCardCombatUnit* Enemy = Unit(Runtime, TEXT("Enemy"));
			if (Case.Destination == EGameXXKCardTerrain::Plain)
			{
				TestEqual(Context + TEXT(" resolves one Plain Burn benefit"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn), 2);
			}
			else if (Case.Destination == EGameXXKCardTerrain::Cliff)
			{
				TestEqual(Context + TEXT(" resolves one Cliff Vulnerability benefit"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Vulnerability), 2);
				TestEqual(Context + TEXT(" resolves one Cliff Mark benefit"), GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Mark), 1);
			}
			else if (Case.Destination == EGameXXKCardTerrain::Forest)
			{
				TestEqual(Context + TEXT(" heals the formation owner by four"), Formation->HP, 84);
				TestEqual(Context + TEXT(" heals every ally by four"), Ally->HP, 84);
			}
			else if (Case.Destination == EGameXXKCardTerrain::WaterShore)
			{
				TestEqual(Context + TEXT(" grants the formation owner three Mana"), Formation->Mana, 3);
				TestEqual(Context + TEXT(" grants every ally three Mana"), Ally->Mana, 3);
			}
			else if (Case.Destination == EGameXXKCardTerrain::Village)
			{
				TestEqual(Context + TEXT(" grants the formation owner four Armor"), Formation->Armor, 4);
				TestEqual(Context + TEXT(" grants every ally four Armor"), Ally->Armor, 4);
				TestEqual(Context + TEXT(" replaces the played card with one drawn card"), Runtime.Deck.Hand.Num(), 5);
			}
			else if (Case.Destination == EGameXXKCardTerrain::Cave)
			{
				TestEqual(Context + TEXT(" grants the formation owner eight Armor"), Formation->Armor, 8);
				TestEqual(Context + TEXT(" grants every ally eight Armor"), Ally->Armor, 8);
				TestEqual(Context + TEXT(" grants the formation owner one Block"), GameXXKCardRules::GetCombatStatusStacks(*Formation, EGameXXKCardStatus::Block), 1);
				TestEqual(Context + TEXT(" grants every ally one Block"), GameXXKCardRules::GetCombatStatusStacks(*Ally, EGameXXKCardStatus::Block), 1);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterBenefitRuntimeTest,
	"GameXXK.Data.PartnerCards.Formation.BenefitRuntime.PlainMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterBenefitRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterPartnerRuntimeTest;
	const TArray<FName> BenefitCardIds = {
		TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("Profession.FormationMaster.ShanMenFengSuo"),
		TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("Profession.FormationMaster.ShuiJingZheGuang"), TEXT("Profession.FormationMaster.LinFengFuZhen"),
		TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("Profession.FormationMaster.ZhenQiGuWu"),
		TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("Profession.FormationMaster.DiMaiJieLi"), TEXT("Profession.FormationMaster.SiXiangLianHuan")};
	int32 Seed = 65000;
	for (const FName CardId : BenefitCardIds)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, *CardId.ToString(), EGameXXKCardTerrain::Plain, Seed++, Runtime))
		{
			continue;
		}
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("%s definition exists"), *CardId.ToString()), Definition))
		{
			continue;
		}
		const FName PlayedInstanceId = Runtime.Deck.Hand[0].InstanceId;
		FName TargetId = NAME_None;
		if (Definition->TargetSpec.Mode == EGameXXKCardTargetMode::SingleEnemy)
		{
			TargetId = TEXT("Enemy");
		}
		else if (Definition->TargetSpec.Mode == EGameXXKCardTargetMode::SingleAlly)
		{
			TargetId = TEXT("Ally");
		}
		FGameXXKCardPlayResult Result;
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, TargetId, Result, &Error);
		TestTrue(FString::Printf(TEXT("%s resolves its base and terrain benefit: %s"), *CardId.ToString(), *Error), bResolved);
		if (!bResolved)
		{
			continue;
		}
		const int32 ExpectedBurn = (CardId == TEXT("Profession.FormationMaster.DiMaiJieLi")
			|| CardId == TEXT("Profession.FormationMaster.SiXiangLianHuan")) ? 4 : 2;
		TestEqual(FString::Printf(TEXT("%s resolves its declared Plain benefit count"), *CardId.ToString()),
			GameXXKCardRules::GetCombatStatusStacks(*Unit(Runtime, TEXT("Enemy")), EGameXXKCardStatus::Burn),
			ExpectedBurn);
		if (CardId == TEXT("Profession.FormationMaster.BaMenLunZhuan"))
		{
			TestEqual(TEXT("八门轮转 leaves its next-benefit doubling ready after its own single benefit"),
				GameXXKCardRules::GetCombatStatusStacks(*Unit(Runtime, TEXT("Formation")), EGameXXKCardStatus::TerrainBonusDouble),
				1);
		}
		if (CardId == TEXT("Profession.FormationMaster.WanXiangGuiZhen"))
		{
			TestEqual(TEXT("万象归阵 leaves the next terrain card free"),
				GameXXKCardRules::GetCombatStatusStacks(*Unit(Runtime, TEXT("Formation")), EGameXXKCardStatus::NextTerrainCardFree),
				1);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFormationMasterVillageDiscardSequenceTest,
	"GameXXK.Data.PartnerCards.Formation.BenefitRuntime.VillageDiscardSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterVillageDiscardSequenceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterPartnerRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(
			*this,
			TEXT("Profession.FormationMaster.BaMenLunZhuan"),
			EGameXXKCardTerrain::Village,
			66001,
			Runtime))
	{
		return false;
	}

	const FName PlayedInstanceId = Runtime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult Result;
	FString Error;
	const bool bResolved = GameXXKCardRules::ResolveCardPlay(
		Runtime,
		PlayedInstanceId,
		NAME_None,
		Result,
		&Error);
	TestTrue(FString::Printf(TEXT("八门轮转 resolves its Village draw before requesting one discard: %s"), *Error), bResolved);
	if (!bResolved)
	{
		return false;
	}

	TestEqual(TEXT("八门轮转 draws three plus the Village benefit card before the choice"), Runtime.Deck.Hand.Num(), 8);
	TestEqual(TEXT("八门轮转 opens exactly one forced-discard choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("all eight current hand cards are legal discard candidates"), Runtime.Deck.PendingChoice.Candidates.Num(), 8);
	TestEqual(TEXT("八门轮转 keeps its next terrain benefit doubled"),
		GameXXKCardRules::GetCombatStatusStacks(*Unit(Runtime, TEXT("Formation")), EGameXXKCardStatus::TerrainBonusDouble),
		1);
	TestEqual(TEXT("Village grants the formation owner four Armor"), Unit(Runtime, TEXT("Formation"))->Armor, 4);
	TestEqual(TEXT("Village grants the ally four Armor"), Unit(Runtime, TEXT("Ally"))->Armor, 4);
	return true;
}

#endif
