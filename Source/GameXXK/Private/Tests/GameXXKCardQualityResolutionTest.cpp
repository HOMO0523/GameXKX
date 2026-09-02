#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKCardText.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakeQualityTestUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 MaxHP,
		const int32 Attack,
		const int32 Mana,
		const int32 MaxMana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeQualityTestInstances(
		const TCHAR* CardId,
		const int32 Count,
		const EGameXXKCardQuality Quality,
		const TCHAR* OwnerUnitId = TEXT("Hero"))
	{
		TArray<FGameXXKCardInstance> Instances;
		Instances.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Quality.Instance.%s.%d"), CardId, Index));
			Instance.CardId = FName(CardId);
			Instance.CurrentQuality = Quality;
			Instance.OwnerUnitId = FName(OwnerUnitId);
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Quality.Entry.%s.%d"), CardId, Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	FGameXXKCardCombatUnit* FindQualityTestUnit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	TArray<FGameXXKCardCombatUnit> MakeHeroAndEnemyQualityUnits(
		const int32 HeroAttack = 20,
		const int32 HeroMana = 0,
		const int32 HeroMaxMana = 50,
		const int32 EnemyHP = 500)
	{
		return {
			MakeQualityTestUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, HeroAttack, HeroMana, HeroMaxMana, 1),
			MakeQualityTestUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyHP, EnemyHP, 10, 0, 0, 10)
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardMagnitudePolicyResolutionTest,
	"GameXXK.Data.CardQuality.MagnitudePolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardMagnitudePolicyResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKCardEffect ContinuousDamage;
	ContinuousDamage.Type = EGameXXKCardEffectType::DamagePercentAttack;
	ContinuousDamage.Target = EGameXXKCardEffectTarget::SelectedTarget;
	ContinuousDamage.Magnitude = 101;
	ContinuousDamage.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
	TestEqual(TEXT("Rare continuously scales 101 upward to 122"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(ContinuousDamage, EGameXXKCardQuality::Rare), 122);
	TestEqual(TEXT("Epic continuously scales 101 upward to 142"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(ContinuousDamage, EGameXXKCardQuality::Epic), 142);

	FGameXXKCardEffect ExplicitDraw;
	ExplicitDraw.Type = EGameXXKCardEffectType::DrawCards;
	ExplicitDraw.Target = EGameXXKCardEffectTarget::CardOwner;
	ExplicitDraw.Magnitude = 2;
	ExplicitDraw.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ExplicitByQuality;
	ExplicitDraw.RareMagnitude = 3;
	ExplicitDraw.EpicMagnitude = 4;
	TestEqual(TEXT("Common explicit draw uses its base value"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(ExplicitDraw, EGameXXKCardQuality::Common), 2);
	TestEqual(TEXT("Rare explicit draw uses its authored value"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(ExplicitDraw, EGameXXKCardQuality::Rare), 3);
	TestEqual(TEXT("Epic explicit draw uses its authored value"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(ExplicitDraw, EGameXXKCardQuality::Epic), 4);

	FGameXXKCardEffect DotCoefficient;
	DotCoefficient.Type = EGameXXKCardEffectType::ApplyStatus;
	DotCoefficient.Target = EGameXXKCardEffectTarget::SelectedTarget;
	DotCoefficient.Status = EGameXXKCardStatus::Bleed;
	DotCoefficient.Magnitude = 6;
	DotCoefficient.MagnitudePolicy = EGameXXKCardMagnitudePolicy::DotCoefficient;
	TestEqual(TEXT("DOT keeps coefficient six for runtime level resolution"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(DotCoefficient, EGameXXKCardQuality::Epic), 6);

	FGameXXKCardEffect PrintedArmor;
	PrintedArmor.Type = EGameXXKCardEffectType::AddArmor;
	PrintedArmor.Target = EGameXXKCardEffectTarget::CardOwner;
	PrintedArmor.Magnitude = 80;
	PrintedArmor.MagnitudePolicy = EGameXXKCardMagnitudePolicy::PrintedCostArmor;
	TestEqual(TEXT("printed-cost Armor stays data-only during quality resolution"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(PrintedArmor, EGameXXKCardQuality::Epic), 80);

	FGameXXKCardEffect DefensePercent;
	DefensePercent.Type = EGameXXKCardEffectType::AddArmor;
	DefensePercent.Target = EGameXXKCardEffectTarget::CardOwner;
	DefensePercent.Magnitude = 150;
	DefensePercent.MagnitudePolicy = EGameXXKCardMagnitudePolicy::DefensePercent;
	TestEqual(TEXT("Defense percentage stays a runtime coefficient"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(DefensePercent, EGameXXKCardQuality::Rare), 150);

	FGameXXKCardEffect MedicineCoefficient;
	MedicineCoefficient.Type = EGameXXKCardEffectType::HealOrReverseWithMedicine;
	MedicineCoefficient.Target = EGameXXKCardEffectTarget::SelectedTarget;
	MedicineCoefficient.Magnitude = 25;
	MedicineCoefficient.MagnitudePolicy = EGameXXKCardMagnitudePolicy::MedicineCoefficient;
	TestEqual(TEXT("Medicine healing keeps coefficient twenty-five for runtime context"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(MedicineCoefficient, EGameXXKCardQuality::Epic), 25);

	FGameXXKCardEffect UnscaledDraw;
	UnscaledDraw.Type = EGameXXKCardEffectType::DrawCards;
	UnscaledDraw.Target = EGameXXKCardEffectTarget::CardOwner;
	UnscaledDraw.Magnitude = 2;
	UnscaledDraw.MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;
	TestEqual(TEXT("unscaled draw remains two"),
		FGameXXKCardQualityRules::ResolveEffectMagnitude(UnscaledDraw, EGameXXKCardQuality::Epic), 2);

	FGameXXKCardDefinition ModifierDefinition;
	ModifierDefinition.BaseQuality = EGameXXKCardQuality::Common;
	FGameXXKCardEffect ModifierEffect;
	ModifierEffect.Type = EGameXXKCardEffectType::ApplyBattleModifier;
	ModifierEffect.Modifier.EffectType = EGameXXKCardEffectType::DamageFlat;
	ModifierEffect.Modifier.Magnitude = 101;
	ModifierEffect.Modifier.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
	ModifierDefinition.Effects = {ModifierEffect};
	const FGameXXKCardDefinition EffectiveModifier = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		ModifierDefinition,
		EGameXXKCardQuality::Rare);
	TestEqual(TEXT("effective definition records the resolved Rare quality"), EffectiveModifier.BaseQuality, EGameXXKCardQuality::Rare);
	TestEqual(TEXT("nested modifier uses its own continuous policy"), EffectiveModifier.Effects[0].Modifier.Magnitude, 122);

	const FGameXXKCardDefinition* ValidBase = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
	if (!TestNotNull(TEXT("policy validation fixture finds a stable card"), ValidBase))
	{
		return false;
	}
	FString Error;
	FGameXXKCardDefinition InvalidDefinition = *ValidBase;
	ExplicitDraw.RareMagnitude = INDEX_NONE;
	InvalidDefinition.Effects = {ExplicitDraw};
	TestFalse(TEXT("explicit policy rejects a missing Rare value"), FGameXXKCardCatalog::ValidateCardDefinition(InvalidDefinition, Error));
	TestTrue(TEXT("missing explicit value reports the policy contract"), Error.Contains(TEXT("explicit quality")));

	InvalidDefinition = *ValidBase;
	DotCoefficient.Magnitude = -1;
	InvalidDefinition.Effects = {DotCoefficient};
	TestFalse(TEXT("DOT policy rejects a negative coefficient"), FGameXXKCardCatalog::ValidateCardDefinition(InvalidDefinition, Error));
	TestTrue(TEXT("negative DOT coefficient reports the policy contract"), Error.Contains(TEXT("negative policy coefficient")));

	InvalidDefinition = *ValidBase;
	DotCoefficient.Magnitude = 6;
	DotCoefficient.Status = EGameXXKCardStatus::Mark;
	InvalidDefinition.Effects = {DotCoefficient};
	TestFalse(TEXT("DOT policy rejects a non-DOT status"), FGameXXKCardCatalog::ValidateCardDefinition(InvalidDefinition, Error));
	TestTrue(TEXT("wrong DOT status reports the policy contract"), Error.Contains(TEXT("non-DOT")));

	InvalidDefinition = *ValidBase;
	PrintedArmor.Type = EGameXXKCardEffectType::DrawCards;
	InvalidDefinition.Effects = {PrintedArmor};
	TestFalse(TEXT("printed-cost Armor policy rejects a non-Armor effect"), FGameXXKCardCatalog::ValidateCardDefinition(InvalidDefinition, Error));
	TestTrue(TEXT("wrong printed Armor effect reports the policy contract"), Error.Contains(TEXT("non-Armor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardQualityResolveCardPlayTest,
	"GameXXK.Data.CardBattleRuntime.QualityResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardQualityResolveCardPlayTest::RunTest(const FString& Parameters)
{
	// Rare damage must use the instance quality, not the immutable catalog base quality.
	FGameXXKCardBattleRuntime RareDamageRuntime;
	if (!TestTrue(TEXT("Rare damage runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		RareDamageRuntime,
		MakeQualityTestInstances(TEXT("Hero.Generic.QingFengYiShi"), 6, EGameXXKCardQuality::Rare),
		MakeHeroAndEnemyQualityUnits(),
		EGameXXKCardTerrain::Plain,
		9101)))
	{
		return false;
	}
	const FName RareDamageInstanceId = RareDamageRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult RareDamageResult;
	TestTrue(TEXT("Rare damage card resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		RareDamageRuntime,
		RareDamageInstanceId,
		TEXT("Enemy"),
		RareDamageResult));
	const FGameXXKCardCombatUnit* RareDamageEnemy = FindQualityTestUnit(RareDamageRuntime.Units, TEXT("Enemy"));
	TestNotNull(TEXT("Rare damage keeps the enemy fixture addressable"), RareDamageEnemy);
	if (RareDamageEnemy)
	{
		TestEqual(TEXT("Rare scales the 140-percent attack packet to 168 percent"), RareDamageEnemy->HP, 467);
	}
	TestEqual(TEXT("Rare damage still spends the catalog's one energy"), RareDamageRuntime.Deck.SharedEnergy, 2);

	// Epic armor and its additive mana effect must both come from one effective definition.
	FGameXXKCardBattleRuntime EpicArmorRuntime;
	if (!TestTrue(TEXT("Epic armor runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		EpicArmorRuntime,
		MakeQualityTestInstances(TEXT("Profession.Guard.PiJiaXingJun"), 6, EGameXXKCardQuality::Epic),
		MakeHeroAndEnemyQualityUnits(),
		EGameXXKCardTerrain::Plain,
		9102)))
	{
		return false;
	}
	FGameXXKCardPlayResult EpicArmorResult;
	TestTrue(TEXT("Epic armor card resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		EpicArmorRuntime,
		EpicArmorRuntime.Deck.Hand[0].InstanceId,
		NAME_None,
		EpicArmorResult));
	const FGameXXKCardCombatUnit* EpicArmorHero = FindQualityTestUnit(EpicArmorRuntime.Units, TEXT("Hero"));
	TestNotNull(TEXT("Epic armor keeps the hero fixture addressable"), EpicArmorHero);
	if (EpicArmorHero)
	{
		TestEqual(TEXT("Epic scales six armor upward to nine"), EpicArmorHero->Armor, 9);
		TestEqual(TEXT("Epic leaves discrete mana at the base six"), EpicArmorHero->Mana, 6);
	}
	TestEqual(TEXT("Epic armor still spends the catalog's one energy"), EpicArmorRuntime.Deck.SharedEnergy, 2);

	// Healing also verifies preview, player-facing text, and the pure effective definition agree.
	TArray<FGameXXKCardCombatUnit> HealingUnits = MakeHeroAndEnemyQualityUnits(20, 20, 20);
	HealingUnits.Insert(
		MakeQualityTestUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 20, 200, 8, 0, 20, 2),
		1);
	TestEqual(TEXT("healing fixture starts with two removable bleed stacks"),
		GameXXKCardRules::AddCombatStatus(HealingUnits[1], EGameXXKCardStatus::Bleed, 2),
		2);
	FGameXXKCardBattleRuntime HealingRuntime;
	if (!TestTrue(TEXT("Rare healing runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		HealingRuntime,
		MakeQualityTestInstances(TEXT("Hero.Generic.GuiYuanShu"), 6, EGameXXKCardQuality::Rare),
		HealingUnits,
		EGameXXKCardTerrain::Plain,
		9103)))
	{
		return false;
	}
	const FGameXXKCardInstance HealingInstance = HealingRuntime.Deck.Hand[0];
	const FGameXXKCardDefinition* HealingBaseDefinition = FGameXXKCardCatalog::FindCardDefinition(HealingInstance.CardId);
	TestNotNull(TEXT("healing catalog definition exists"), HealingBaseDefinition);
	if (!HealingBaseDefinition)
	{
		return false;
	}
	const FGameXXKCardDefinition HealingEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		*HealingBaseDefinition,
		HealingInstance.CurrentQuality);
	if (!TestEqual(TEXT("healing effective definition preserves all three catalog effects"), HealingEffectiveDefinition.Effects.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Rare effective healing scales twelve upward to fifteen"), HealingEffectiveDefinition.Effects[0].Magnitude, 15);
	TestEqual(TEXT("Rare effective full cleanse keeps its unscaled marker"), HealingEffectiveDefinition.Effects[1].Magnitude, 0);
	TestEqual(TEXT("quality does not change effective energy cost"), HealingEffectiveDefinition.EnergyCost, HealingBaseDefinition->EnergyCost);
	TestEqual(TEXT("quality does not change effective mana cost"), HealingEffectiveDefinition.ManaCost, HealingBaseDefinition->ManaCost);

	FGameXXKCardPlayPreview HealingPreview;
	TestTrue(TEXT("Rare healing builds a real play preview"), GameXXKCardRules::BuildCardPlayPreview(
		HealingRuntime,
		HealingInstance.InstanceId,
		HealingPreview));
	TestEqual(TEXT("preview energy matches the quality-effective definition"), HealingPreview.EffectiveEnergyCost, HealingEffectiveDefinition.EnergyCost);
	TestEqual(TEXT("preview mana matches the quality-effective definition"), HealingPreview.EffectiveManaCost, HealingEffectiveDefinition.ManaCost);
	TestEqual(TEXT("preview targeting matches the quality-effective definition"), HealingPreview.TargetRequest.EffectiveMode, HealingEffectiveDefinition.TargetSpec.Mode);
	const FString HealingText = GameXXKCardText::DescribeDetail(
		*HealingBaseDefinition,
		HealingInstance.CurrentQuality,
		&HealingPreview);
	TestTrue(TEXT("quality-aware text shows the same fifteen healing"), HealingText.Contains(TEXT("15点生命")));
	TestTrue(TEXT("quality-aware text shows the full cleanse"), HealingText.Contains(TEXT("清除")) && HealingText.Contains(TEXT("流血")));
	TestTrue(TEXT("quality-aware text preserves the catalog costs"), HealingText.Contains(TEXT("费用：1 气 / 0 内")));

	FGameXXKCardPlayResult HealingResult;
	TestTrue(TEXT("Rare healing resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		HealingRuntime,
		HealingInstance.InstanceId,
		TEXT("Ally"),
		HealingResult));
	const FGameXXKCardCombatUnit* HealedAlly = FindQualityTestUnit(HealingRuntime.Units, TEXT("Ally"));
	const FGameXXKCardCombatUnit* HealingHero = FindQualityTestUnit(HealingRuntime.Units, TEXT("Hero"));
	TestNotNull(TEXT("Rare healing keeps the ally fixture addressable"), HealedAlly);
	TestNotNull(TEXT("Rare healing keeps the owner fixture addressable"), HealingHero);
	if (HealedAlly)
	{
		TestEqual(TEXT("real resolution heals the same fifteen shown in text"), HealedAlly->HP, 35);
		TestEqual(TEXT("real resolution removes all Bleed shown in text"),
			GameXXKCardRules::GetCombatStatusStacks(*HealedAlly, EGameXXKCardStatus::Bleed),
			0);
	}
	if (HealingHero)
	{
		TestEqual(TEXT("real resolution preserves mana for a zero-mana card"), HealingHero->Mana, 20);
	}
	TestEqual(TEXT("real resolution spends the unchanged one energy"), HealingRuntime.Deck.SharedEnergy, 2);

	// Rare quality leaves this discrete two-card draw unchanged. The five-card value is only the round-refill target;
	// card effects may grow the hand up to the twenty-card battle capacity without forcing discard.
	FGameXXKCardBattleRuntime RareDrawRuntime;
	if (!TestTrue(TEXT("Rare draw runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		RareDrawRuntime,
		MakeQualityTestInstances(TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), 8, EGameXXKCardQuality::Rare),
		MakeHeroAndEnemyQualityUnits(20, 3),
		EGameXXKCardTerrain::Plain,
		9104)))
	{
		return false;
	}
	const int32 RareDrawPileBeforePlay = RareDrawRuntime.Deck.DrawPile.Num();
	FGameXXKCardPlayResult RareDrawResult;
	TestTrue(TEXT("Rare draw card resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		RareDrawRuntime,
		RareDrawRuntime.Deck.Hand[0].InstanceId,
		TEXT("Enemy"),
		RareDrawResult));
	TestEqual(TEXT("Rare draw grows the hand from four to six"), RareDrawRuntime.Deck.Hand.Num(), 6);
	TestEqual(TEXT("Rare draws two concrete cards from the draw pile"),
		RareDrawRuntime.Deck.DrawPile.Num(),
		RareDrawPileBeforePlay - 2);
	TestEqual(TEXT("draw without a declared discard opens no pending choice below capacity"),
		RareDrawRuntime.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::None);

	// Epic quality leaves discrete mana generation unchanged without changing the zero cost.
	FGameXXKCardBattleRuntime EpicManaRuntime;
	if (!TestTrue(TEXT("Epic mana runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		EpicManaRuntime,
		MakeQualityTestInstances(TEXT("Hero.Generic.NingShenTuNa"), 6, EGameXXKCardQuality::Epic),
		MakeHeroAndEnemyQualityUnits(),
		EGameXXKCardTerrain::Plain,
		9105)))
	{
		return false;
	}
	FGameXXKCardPlayResult EpicManaResult;
	TestTrue(TEXT("Epic mana card resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		EpicManaRuntime,
		EpicManaRuntime.Deck.Hand[0].InstanceId,
		NAME_None,
		EpicManaResult));
	const FGameXXKCardCombatUnit* EpicManaHero = FindQualityTestUnit(EpicManaRuntime.Units, TEXT("Hero"));
	TestNotNull(TEXT("Epic mana keeps the hero fixture addressable"), EpicManaHero);
	if (EpicManaHero)
	{
		TestEqual(TEXT("Epic keeps the base ten mana"), EpicManaHero->Mana, 10);
		TestEqual(TEXT("Epic keeps the discrete two Momentum"),
			GameXXKCardRules::GetCombatStatusStacks(*EpicManaHero, EGameXXKCardStatus::Momentum), 2);
	}
	TestEqual(TEXT("Epic mana preserves the catalog's zero energy cost"), EpicManaRuntime.Deck.SharedEnergy, 3);

	// Epic quality leaves the redesigned card's three vulnerability stacks unchanged.
	FGameXXKCardBattleRuntime EpicStatusRuntime;
	if (!TestTrue(TEXT("Epic status runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		EpicStatusRuntime,
		MakeQualityTestInstances(TEXT("Npc.ZhouGuangZu.YanFenFengMai"), 6, EGameXXKCardQuality::Epic),
		MakeHeroAndEnemyQualityUnits(10, 20, 20, 1000),
		EGameXXKCardTerrain::Plain,
		9106)))
	{
		return false;
	}
	FGameXXKCardPlayResult EpicStatusResult;
	TestTrue(TEXT("Epic status card resolves through the real play transaction"), GameXXKCardRules::ResolveCardPlay(
		EpicStatusRuntime,
		EpicStatusRuntime.Deck.Hand[0].InstanceId,
		TEXT("Enemy"),
		EpicStatusResult));
	const FGameXXKCardCombatUnit* EpicStatusEnemy = FindQualityTestUnit(EpicStatusRuntime.Units, TEXT("Enemy"));
	TestNotNull(TEXT("Epic status keeps the enemy fixture addressable"), EpicStatusEnemy);
	if (EpicStatusEnemy)
	{
		TestEqual(TEXT("Epic keeps the base three vulnerability"),
			GameXXKCardRules::GetCombatStatusStacks(*EpicStatusEnemy, EGameXXKCardStatus::Vulnerability),
			3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardQualityTerrainCompositionTest,
	"GameXXK.Data.CardBattleRuntime.QualityTerrainComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardQualityTerrainCompositionTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> Units = MakeHeroAndEnemyQualityUnits(20, 0, 50);
	Units[0].HP = 50;
	Units.Insert(
		MakeQualityTestUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 50, 100, 8, 0, 50, 2),
		1);
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("quality and terrain composition runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		Runtime,
		MakeQualityTestInstances(TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), 6, EGameXXKCardQuality::Rare),
		Units,
		EGameXXKCardTerrain::WaterShore,
		9150)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Hero = FindQualityTestUnit(Runtime.Units, TEXT("Hero"));
	if (!TestNotNull(TEXT("quality and terrain composition keeps the hero fixture addressable"), Hero))
	{
		return false;
	}
	const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.CunZhaiYuanZhen"));
	if (!TestNotNull(TEXT("quality and terrain composition catalog definition exists"), BaseDefinition))
	{
		return false;
	}
	const FGameXXKCardDefinition QualityEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		*BaseDefinition,
		EGameXXKCardQuality::Rare);
	if (!TestEqual(TEXT("quality and terrain composition preserves all three catalog effects"), QualityEffectiveDefinition.Effects.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Rare scales the group heal from twelve to fifteen"), QualityEffectiveDefinition.Effects[0].Magnitude, 15);
	TestEqual(TEXT("Rare scales the group Armor from eight to ten"), QualityEffectiveDefinition.Effects[1].Magnitude, 10);
	TestEqual(TEXT("terrain trigger count remains one"), QualityEffectiveDefinition.Effects[2].Magnitude, 1);

	FGameXXKCardPlayResult Result;
	TestTrue(TEXT("Rare terrain card resolves through quality then terrain amplification"), GameXXKCardRules::ResolveCardPlay(
		Runtime,
		Runtime.Deck.Hand[0].InstanceId,
		NAME_None,
		Result));
	Hero = FindQualityTestUnit(Runtime.Units, TEXT("Hero"));
	const FGameXXKCardCombatUnit* Ally = FindQualityTestUnit(Runtime.Units, TEXT("Ally"));
	TestNotNull(TEXT("resolved quality and terrain composition keeps the hero addressable"), Hero);
	TestNotNull(TEXT("resolved quality and terrain composition keeps the ally addressable"), Ally);
	if (Hero)
	{
		TestEqual(TEXT("Rare group heal restores fifteen to the hero"), Hero->HP, 65);
		TestEqual(TEXT("Rare group Armor grants ten to the hero"), Hero->Armor, 10);
		TestEqual(TEXT("water-shore terrain benefit grants the hero three mana"), Hero->Mana, 3);
	}
	if (Ally)
	{
		TestEqual(TEXT("Rare group heal restores fifteen to the ally"), Ally->HP, 65);
		TestEqual(TEXT("Rare group Armor grants ten to the ally"), Ally->Armor, 10);
		TestEqual(TEXT("water-shore terrain benefit grants the ally three mana"), Ally->Mana, 3);
	}
	TestEqual(TEXT("quality and terrain composition preserves the catalog's two energy cost"), Runtime.Deck.SharedEnergy, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardInstanceQualityValidationTest,
	"GameXXK.Data.CardRules.CardInstanceQualityValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardInstanceQualityValidationTest::RunTest(const FString& Parameters)
{
	FGameXXKBattleDeckState PreservedDeck;
	PreservedDeck.SharedEnergy = 41;
	FString InvalidQualityError;
	TestFalse(TEXT("deck initialization rejects Invalid CurrentQuality"), GameXXKCardRules::InitializeBattleDeck(
		PreservedDeck,
		MakeQualityTestInstances(TEXT("Hero.Generic.QingFengYiShi"), 6, EGameXXKCardQuality::Invalid),
		9201,
		&InvalidQualityError));
	TestFalse(TEXT("invalid quality rejection explains the failure"), InvalidQualityError.IsEmpty());
	TestEqual(TEXT("invalid quality rejection preserves caller deck state"), PreservedDeck.SharedEnergy, 41);

	FGameXXKBattleDeckState UnsupportedQualityDeck;
	TestFalse(TEXT("deck initialization rejects unsupported serialized CurrentQuality"), GameXXKCardRules::InitializeBattleDeck(
		UnsupportedQualityDeck,
		MakeQualityTestInstances(TEXT("Hero.Generic.QingFengYiShi"), 6, static_cast<EGameXXKCardQuality>(255)),
		9202));

	FGameXXKBattleDeckState CandidateDeck;
	if (!TestTrue(TEXT("candidate-copy quality fixture initializes"), GameXXKCardRules::InitializeBattleDeck(
		CandidateDeck,
		MakeQualityTestInstances(TEXT("Hero.Generic.FengShenBu"), 8, EGameXXKCardQuality::Rare),
		9203)))
	{
		return false;
	}
	TestTrue(TEXT("candidate-copy quality fixture frees one hand slot"), GameXXKCardRules::MoveHandCardToDiscard(
		CandidateDeck,
		CandidateDeck.Hand.Last().InstanceId));
	TestTrue(TEXT("candidate-copy quality fixture opens its one declared forced discard"), GameXXKCardRules::DrawCards(CandidateDeck, 2, 1));
	if (!TestTrue(TEXT("forced discard exposes at least one candidate"), !CandidateDeck.PendingChoice.Candidates.IsEmpty()))
	{
		return false;
	}
	CandidateDeck.PendingChoice.Candidates[0].CurrentQuality = EGameXXKCardQuality::Epic;
	TestFalse(TEXT("serialized candidate copies with a different quality are not the same instance"),
		GameXXKCardRules::ValidateDeckState(CandidateDeck));

	FGameXXKCardBattleRuntime InvalidPlayRuntime;
	if (!TestTrue(TEXT("invalid-play atomicity fixture initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		InvalidPlayRuntime,
		MakeQualityTestInstances(TEXT("Hero.Generic.QingFengYiShi"), 6, EGameXXKCardQuality::Common),
		MakeHeroAndEnemyQualityUnits(),
		EGameXXKCardTerrain::Plain,
		9204)))
	{
		return false;
	}
	const FName InvalidPlayInstanceId = InvalidPlayRuntime.Deck.Hand[0].InstanceId;
	InvalidPlayRuntime.Deck.Hand[0].CurrentQuality = EGameXXKCardQuality::Invalid;
	const FGameXXKCardBattleRuntime RuntimeBeforeRejectedPlay = InvalidPlayRuntime;
	const int32 EnergyBeforeRejectedPlay = InvalidPlayRuntime.Deck.SharedEnergy;
	const int32 HandBeforeRejectedPlay = InvalidPlayRuntime.Deck.Hand.Num();
	const int32 EnemyHealthBeforeRejectedPlay = FindQualityTestUnit(InvalidPlayRuntime.Units, TEXT("Enemy"))->HP;
	FGameXXKCardPlayResult RejectedResult;
	RejectedResult.CardId = TEXT("Prior.Result");
	const FGameXXKCardPlayResult ResultBeforeRejectedPlay = RejectedResult;
	FString RejectedPlayError;
	TestFalse(TEXT("ResolveCardPlay rejects a non-concrete CurrentQuality"), GameXXKCardRules::ResolveCardPlay(
		InvalidPlayRuntime,
		InvalidPlayInstanceId,
		TEXT("Enemy"),
		RejectedResult,
		&RejectedPlayError));
	TestFalse(TEXT("rejected quality play explains the failure"), RejectedPlayError.IsEmpty());
	TestTrue(TEXT("rejected quality play preserves the complete runtime"),
		FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(
			&InvalidPlayRuntime,
			&RuntimeBeforeRejectedPlay,
			PPF_None));
	TestTrue(TEXT("rejected quality play preserves the complete caller output"),
		FGameXXKCardPlayResult::StaticStruct()->CompareScriptStruct(
			&RejectedResult,
			&ResultBeforeRejectedPlay,
			PPF_None));
	TestEqual(TEXT("rejected quality play preserves energy"), InvalidPlayRuntime.Deck.SharedEnergy, EnergyBeforeRejectedPlay);
	TestEqual(TEXT("rejected quality play preserves the hand"), InvalidPlayRuntime.Deck.Hand.Num(), HandBeforeRejectedPlay);
	TestEqual(TEXT("rejected quality play preserves target health"),
		FindQualityTestUnit(InvalidPlayRuntime.Units, TEXT("Enemy"))->HP,
		EnemyHealthBeforeRejectedPlay);
	TestEqual(TEXT("rejected quality play preserves caller output"), RejectedResult.CardId, FName(TEXT("Prior.Result")));

	return true;
}

#endif
