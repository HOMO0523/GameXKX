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
	FGameXXKCardQualityResolveCardPlayTest,
	"GameXXK.Data.CardBattleRuntime.QualityResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardQualityResolveCardPlayTest::RunTest(const FString& Parameters)
{
	// Rare damage must use the instance quality, not the immutable catalog base quality.
	FGameXXKCardBattleRuntime RareDamageRuntime;
	if (!TestTrue(TEXT("Rare damage runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		RareDamageRuntime,
		MakeQualityTestInstances(TEXT("Route.General.PoJiaTuCi"), 6, EGameXXKCardQuality::Rare),
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
		TestEqual(TEXT("Rare doubles the 100-percent attack packet to 200 percent"), RareDamageEnemy->HP, 460);
	}
	TestEqual(TEXT("Rare damage still spends the catalog's one energy"), RareDamageRuntime.Deck.SharedEnergy, 2);

	// Epic armor and its additive mana effect must both come from one effective definition.
	FGameXXKCardBattleRuntime EpicArmorRuntime;
	if (!TestTrue(TEXT("Epic armor runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		EpicArmorRuntime,
		MakeQualityTestInstances(TEXT("Route.General.ShouShiHuiYuan"), 6, EGameXXKCardQuality::Epic),
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
		TestEqual(TEXT("Epic quadruples eight armor to thirty-two"), EpicArmorHero->Armor, 32);
		TestEqual(TEXT("Epic adds four quality mana to the base three"), EpicArmorHero->Mana, 7);
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
		MakeQualityTestInstances(TEXT("Route.General.ZhiXueSan"), 6, EGameXXKCardQuality::Rare),
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
	if (!TestEqual(TEXT("healing effective definition preserves both catalog effects"), HealingEffectiveDefinition.Effects.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Rare effective healing doubles twelve to twenty-four"), HealingEffectiveDefinition.Effects[0].Magnitude, 24);
	TestEqual(TEXT("Rare effective cleansing increases one stack to two"), HealingEffectiveDefinition.Effects[1].Magnitude, 2);
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
	TestTrue(TEXT("quality-aware text shows the same twenty-four healing"), HealingText.Contains(TEXT("24点生命")));
	TestTrue(TEXT("quality-aware text shows the same two-stack bleed removal"), HealingText.Contains(TEXT("2层流血")));
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
		TestEqual(TEXT("real resolution heals the same twenty-four shown in text"), HealedAlly->HP, 44);
		TestEqual(TEXT("real resolution removes the same two stacks shown in text"),
			GameXXKCardRules::GetCombatStatusStacks(*HealedAlly, EGameXXKCardStatus::Bleed),
			0);
	}
	if (HealingHero)
	{
		TestEqual(TEXT("real resolution preserves mana for a zero-mana card"), HealingHero->Mana, 20);
	}
	TestEqual(TEXT("real resolution spends the unchanged one energy"), HealingRuntime.Deck.SharedEnergy, 2);

	// Rare draw increases the redesigned card's two draws to three. The five-card value is only the round-refill target;
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
	TestEqual(TEXT("Rare draw grows the hand from four to seven"), RareDrawRuntime.Deck.Hand.Num(), 7);
	TestEqual(TEXT("Rare draws three concrete cards from the draw pile"),
		RareDrawRuntime.Deck.DrawPile.Num(),
		RareDrawPileBeforePlay - 3);
	TestEqual(TEXT("draw without a declared discard opens no pending choice below capacity"),
		RareDrawRuntime.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::None);

	// Epic mana uses +4 over the base two without changing the zero cost.
	FGameXXKCardBattleRuntime EpicManaRuntime;
	if (!TestTrue(TEXT("Epic mana runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		EpicManaRuntime,
		MakeQualityTestInstances(TEXT("Route.General.TuNaJue"), 6, EGameXXKCardQuality::Epic),
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
		TestEqual(TEXT("Epic adds four mana to the base five"), EpicManaHero->Mana, 9);
	}
	TestEqual(TEXT("Epic mana preserves the catalog's zero energy cost"), EpicManaRuntime.Deck.SharedEnergy, 3);

	// Epic status increases the redesigned card's three vulnerability stacks to five.
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
		TestEqual(TEXT("Epic adds two stacks to the base three vulnerability"),
			GameXXKCardRules::GetCombatStatusStacks(*EpicStatusEnemy, EGameXXKCardStatus::Vulnerability),
			5);
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
	Units.Insert(
		MakeQualityTestUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 100, 100, 8, 0, 50, 2),
		1);
	FGameXXKCardBattleRuntime Runtime;
	if (!TestTrue(TEXT("quality and terrain composition runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		Runtime,
		MakeQualityTestInstances(TEXT("Route.Terrain.DuKouHuiLiu"), 6, EGameXXKCardQuality::Rare),
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
	TestEqual(TEXT("quality and terrain composition starts with one doubling window"),
		GameXXKCardRules::AddCombatStatus(*Hero, EGameXXKCardStatus::TerrainBonusDouble, 1),
		1);

	const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Route.Terrain.DuKouHuiLiu"));
	if (!TestNotNull(TEXT("quality and terrain composition catalog definition exists"), BaseDefinition))
	{
		return false;
	}
	const FGameXXKCardDefinition QualityEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		*BaseDefinition,
		EGameXXKCardQuality::Rare);
	if (!TestEqual(TEXT("quality and terrain composition preserves both catalog effects"), QualityEffectiveDefinition.Effects.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Rare scales the unconditional mana segment before terrain amplification"), QualityEffectiveDefinition.Effects[0].Magnitude, 5);
	TestEqual(TEXT("Rare scales the water-shore mana segment before terrain amplification"), QualityEffectiveDefinition.Effects[1].Magnitude, 5);

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
		TestEqual(TEXT("Rare base five plus two amplified water-shore fives gives the hero fifteen mana"), Hero->Mana, 15);
		TestEqual(TEXT("quality and terrain composition consumes exactly one doubling window"),
			GameXXKCardRules::GetCombatStatusStacks(*Hero, EGameXXKCardStatus::TerrainBonusDouble),
			0);
	}
	if (Ally)
	{
		TestEqual(TEXT("Rare base five plus two amplified water-shore fives gives the ally fifteen mana"), Ally->Mana, 15);
	}
	TestEqual(TEXT("quality and terrain composition preserves the catalog's one energy cost"), Runtime.Deck.SharedEnergy, 2);

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
