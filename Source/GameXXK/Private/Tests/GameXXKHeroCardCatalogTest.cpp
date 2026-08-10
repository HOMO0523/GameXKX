#include "GameXXKCardTypes.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroCardCatalogSchemaTest,
	"GameXXK.Data.HeroCards.Catalog.Schema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroCardCatalogSchemaTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Block appends after Counter"), static_cast<uint8>(EGameXXKCardStatus::Block), static_cast<uint8>(27));
	TestEqual(TEXT("highest-armor ally appends after PlayedCard"), static_cast<uint8>(EGameXXKCardEffectTarget::HighestArmorAlly), static_cast<uint8>(11));
	TestEqual(TEXT("Exhaust appends after Discard"), static_cast<uint8>(EGameXXKCardZone::ExhaustPile), static_cast<uint8>(4));
	TestEqual(TEXT("hero search appends after Insight"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand), static_cast<uint8>(4));
	TestEqual(TEXT("Block damage cause appends after Environment"), static_cast<uint8>(EGameXXKCardDamageCause::Block), static_cast<uint8>(12));

	const TMap<EGameXXKCardEffectType, uint8> ExpectedEffectValues = {
		{EGameXXKCardEffectType::RegisterReaction, 29},
		{EGameXXKCardEffectType::LoseHealthNonlethal, 30},
		{EGameXXKCardEffectType::Cleanse, 31},
		{EGameXXKCardEffectType::TriggerHighestDamageOverTime, 32},
		{EGameXXKCardEffectType::ResolveToxicExplosion, 33},
		{EGameXXKCardEffectType::HealOrReverseWithMedicine, 34},
		{EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, 35},
		{EGameXXKCardEffectType::DamagePercentAttackPlusArmor, 36},
		{EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, 37},
		{EGameXXKCardEffectType::TriggerTerrainBenefit, 38},
		{EGameXXKCardEffectType::GainArmorFromCurrentManaPercent, 39},
		{EGameXXKCardEffectType::GainManaOverflowToArmor, 40},
		{EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, 41},
		{EGameXXKCardEffectType::TriggerStatus, 42},
		{EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, 43},
		{EGameXXKCardEffectType::ReplayTriggeredCardBase, 44},
		{EGameXXKCardEffectType::ReplaySourceCardBase, 45}
	};
	for (const TPair<EGameXXKCardEffectType, uint8>& Expected : ExpectedEffectValues)
	{
		TestEqual(
			FString::Printf(TEXT("effect type %d retains its append-only value"), static_cast<int32>(Expected.Key)),
			static_cast<uint8>(Expected.Key),
			Expected.Value);
	}

	TestEqual(TEXT("before-next-active trigger appends at seven"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard), static_cast<uint8>(7));
	TestEqual(TEXT("after-next-active trigger appends at eight"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterNextActiveCard), static_cast<uint8>(8));
	TestEqual(TEXT("next-round-start trigger appends at nine"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart), static_cast<uint8>(9));
	TestEqual(TEXT("before-first-next-round trigger appends at ten"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound), static_cast<uint8>(10));
	TestEqual(TEXT("after-first-next-round trigger appends at eleven"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound), static_cast<uint8>(11));
	TestEqual(TEXT("first attack against status trigger appends at twelve"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound), static_cast<uint8>(12));
	TestEqual(TEXT("after-each-active trigger appends at thirteen"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterEachActiveCard), static_cast<uint8>(13));

	TestEqual(TEXT("active play origin is one"), static_cast<uint8>(EGameXXKCardResolutionOrigin::ActivePlay), static_cast<uint8>(1));
	TestEqual(TEXT("automatic replay origin is two"), static_cast<uint8>(EGameXXKCardResolutionOrigin::AutomaticReplay), static_cast<uint8>(2));
	TestEqual(TEXT("Mage replay origin is three"), static_cast<uint8>(EGameXXKCardResolutionOrigin::MageTaskReplay), static_cast<uint8>(3));
	TestEqual(TEXT("Heavy Arrow origin is four"), static_cast<uint8>(EGameXXKCardResolutionOrigin::HeavyArrow), static_cast<uint8>(4));
	TestEqual(TEXT("reaction origin is five"), static_cast<uint8>(EGameXXKCardResolutionOrigin::Reaction), static_cast<uint8>(5));
	TestEqual(TEXT("terrain-listener origin is six"), static_cast<uint8>(EGameXXKCardResolutionOrigin::TerrainListener), static_cast<uint8>(6));
	TestEqual(TEXT("task-reward origin is seven"), static_cast<uint8>(EGameXXKCardResolutionOrigin::TaskReward), static_cast<uint8>(7));

	FGameXXKCardDefinition Definition;
	TestEqual(TEXT("generic cards have no linked role by default"), Definition.LinkedRole, EGameXXKCharacterRole::Invalid);
	TestEqual(TEXT("non-hero cards have no hero unlock level"), Definition.HeroUnlockLevel, 0);
	TestFalse(TEXT("cards do not exhaust by default"), Definition.bExhaustOnPlay);
	TestTrue(TEXT("charge effects default empty"), Definition.ChargeEffects.IsEmpty());
	TestTrue(TEXT("finish effects default empty"), Definition.FinishEffects.IsEmpty());
	TestEqual(TEXT("Heavy Arrow defaults off"), Definition.HeavyArrow.Kind, EGameXXKHeavyArrowKind::None);
	TestEqual(TEXT("Mage reward defaults off"), Definition.SpellTaskReward, EGameXXKHeroSpellTaskReward::None);

	FGameXXKCardEffect Effect;
	TestEqual(TEXT("effects source from their card owner by default"), Effect.Source, EGameXXKCardEffectSource::CardOwner);
	TestEqual(TEXT("terrain override defaults off"), Effect.TerrainOverride, EGameXXKCardTerrain::Invalid);
	TestTrue(TEXT("effect result group defaults empty"), Effect.ResultGroupId.IsNone());
	TestTrue(TEXT("effect result reference defaults empty"), Effect.ResultRef.IsNone());

	FGameXXKCardBattleModifier Modifier;
	TestFalse(TEXT("modifiers are not active-play-only by default"), Modifier.bActivePlayOnly);
	TestFalse(TEXT("modifiers include their source unit by default"), Modifier.bExcludeSourceUnit);
	TestFalse(TEXT("triggered statuses decay by default"), Modifier.bPreserveTriggeredStatus);

	FGameXXKCardDamageContext DamageContext;
	FGameXXKCardDamageResult DamageResult;
	FGameXXKCardPlayResult PlayResult;
	TestEqual(TEXT("damage context origin defaults invalid"), DamageContext.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
	TestEqual(TEXT("damage result origin defaults invalid"), DamageResult.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
	TestEqual(TEXT("play result origin defaults invalid"), PlayResult.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
	TestEqual(TEXT("low-level agility roll defaults deterministically"), DamageContext.AgilityRollPercent, 0);
	TestEqual(TEXT("damage audit starts without a roll"), DamageResult.AgilityRollPercent, INDEX_NONE);
	TestEqual(TEXT("damage audit starts with no agility consumption"), DamageResult.AgilityStacksConsumed, 0);
	TestFalse(TEXT("damage audit starts without a perfect dodge"), DamageResult.bPerfectAgilityDodge);

	FGameXXKBattleDeckState Deck;
	TestTrue(TEXT("exhaust pile defaults empty"), Deck.ExhaustPile.IsEmpty());

	FGameXXKCardBattleModifierRuntime RuntimeModifier;
	TestTrue(TEXT("modifier source snapshot defaults empty"), RuntimeModifier.SourceCardSnapshot.CardId.IsNone());

	FGameXXKCardBattleRuntime Runtime;
	TestTrue(TEXT("equipped hero IDs default empty"), Runtime.EquippedHeroCardIds.IsEmpty());
	TestEqual(TEXT("active-card count defaults zero"), Runtime.ActiveCardsPlayedThisRound, 0);
	TestTrue(TEXT("last active card defaults empty"), Runtime.LastActiveCard.CardId.IsNone());
	TestFalse(TEXT("terrain-change flag defaults false"), Runtime.bTerrainChangedThisRound);
	TestEqual(TEXT("combat random state defaults zero"), Runtime.CombatRandomState, 0);
	TestTrue(TEXT("reaction records default empty"), Runtime.Reactions.IsEmpty());
	TestEqual(TEXT("reaction ordinal defaults zero"), Runtime.NextReactionOrdinal, 0);
	TestFalse(TEXT("automatic queue defaults inactive"), Runtime.AutomaticResolutionQueue.bActive);
	TestFalse(TEXT("Mage task defaults inactive"), Runtime.HeroSpellTask.bActive);

	return true;
}

#endif
