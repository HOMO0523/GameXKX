#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCardTypes.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroCardCatalogTest
{
	struct FExpectedHeroCard
	{
		const TCHAR* Id;
		EGameXXKCharacterRole LinkedRole;
		int32 UnlockLevel;
		int32 EnergyCost;
		int32 ManaCost;
		EGameXXKCardTargetMode TargetMode;
		int32 BaseEffectCount;
		int32 ChargeEffectCount;
		int32 FinishEffectCount;
		bool bExhaust;
		EGameXXKHeavyArrowKind HeavyArrowKind;
		EGameXXKHeroSpellTaskReward SpellTaskReward;
	};

	const TArray<FExpectedHeroCard>& ExpectedHeroCards()
	{
		static const TArray<FExpectedHeroCard> Expected = {
			{TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCharacterRole::Invalid, 1, 1, 0, EGameXXKCardTargetMode::SingleEnemy, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.HeYuZhan"), EGameXXKCharacterRole::Invalid, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.FengShenBu"), EGameXXKCharacterRole::Invalid, 1, 0, 0, EGameXXKCardTargetMode::SingleAlly, 3, 0, 0, true, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.SuiYanJi"), EGameXXKCharacterRole::Invalid, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.GuiYuanShu"), EGameXXKCharacterRole::Invalid, 1, 1, 0, EGameXXKCardTargetMode::SingleAlly, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.HengJianShouShi"), EGameXXKCharacterRole::Invalid, 1, 1, 0, EGameXXKCardTargetMode::SingleAlly, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.NingShenTuNa"), EGameXXKCharacterRole::Invalid, 1, 0, 0, EGameXXKCardTargetMode::Self, 2, 0, 0, true, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.GuanXi"), EGameXXKCharacterRole::Invalid, 1, 0, 0, EGameXXKCardTargetMode::None, 2, 0, 0, true, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.PoYunYiShan"), EGameXXKCharacterRole::Invalid, 5, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.XingQiHuiHuan"), EGameXXKCharacterRole::Invalid, 10, 0, 0, EGameXXKCardTargetMode::None, 2, 0, 0, true, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.JianYiGuanHong"), EGameXXKCharacterRole::Invalid, 15, 2, 6, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Generic.GuiYuanFanZhao"), EGameXXKCharacterRole::Invalid, 20, 2, 6, EGameXXKCardTargetMode::AllAllies, 4, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Blade.TongFengYinShi"), EGameXXKCharacterRole::Blade, 1, 0, 0, EGameXXKCardTargetMode::SingleAlly, 2, 1, 1, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Blade.XueLuXiangCheng"), EGameXXKCharacterRole::Blade, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 2, 1, 2, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Blade.YingFengHuanBu"), EGameXXKCharacterRole::Blade, 1, 1, 0, EGameXXKCardTargetMode::Self, 3, 2, 2, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Blade.TongPaoJuShi"), EGameXXKCharacterRole::Blade, 1, 1, 0, EGameXXKCardTargetMode::SingleAlly, 2, 1, 1, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Guard.TieBiTongShou"), EGameXXKCharacterRole::Guard, 1, 1, 0, EGameXXKCardTargetMode::SingleAlly, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Guard.JieJiaHuanFeng"), EGameXXKCharacterRole::Guard, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Guard.LieZhenChengFeng"), EGameXXKCharacterRole::Guard, 1, 2, 0, EGameXXKCardTargetMode::AllAllies, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Guard.XuanJiaZhenYue"), EGameXXKCharacterRole::Guard, 1, 2, 6, EGameXXKCardTargetMode::SingleAlly, 1, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Healer.YiXueCuiFang"), EGameXXKCharacterRole::Healer, 1, 0, 0, EGameXXKCardTargetMode::None, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Healer.HuiChunNiMai"), EGameXXKCharacterRole::Healer, 1, 1, 3, EGameXXKCardTargetMode::AnyLivingUnit, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Healer.DuHuoTongLu"), EGameXXKCharacterRole::Healer, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 5, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Healer.BaiCaoJiZhen"), EGameXXKCharacterRole::Healer, 1, 2, 6, EGameXXKCardTargetMode::None, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Hunter.FengYanDingXian"), EGameXXKCharacterRole::Hunter, 1, 0, 3, EGameXXKCardTargetMode::None, 4, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Hunter.LieYuLianShi"), EGameXXKCharacterRole::Hunter, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 2, 0, 0, false, EGameXXKHeavyArrowKind::ExtraAttackPerCharge, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Hunter.CuiDuChuanXin"), EGameXXKCharacterRole::Hunter, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::ToxicExplosionPerCharge, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Hunter.HuiFengGuanRi"), EGameXXKCharacterRole::Hunter, 1, 1, 6, EGameXXKCardTargetMode::SingleEnemy, 1, 0, 0, false, EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Mage.YanXuLiaoYuan"), EGameXXKCharacterRole::Sorcerer, 1, 1, 3, EGameXXKCardTargetMode::AllEnemies, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::Fire},
			{TEXT("Hero.Mage.HanXuNingChuan"), EGameXXKCharacterRole::Sorcerer, 1, 0, 0, EGameXXKCardTargetMode::Self, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::Ice},
			{TEXT("Hero.Mage.LeiXuYinTing"), EGameXXKCharacterRole::Sorcerer, 1, 1, 3, EGameXXKCardTargetMode::AllEnemies, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::Lightning},
			{TEXT("Hero.Mage.GuiXuTongXuan"), EGameXXKCharacterRole::Sorcerer, 1, 0, 0, EGameXXKCardTargetMode::None, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::Universal},
			{TEXT("Hero.Formation.GuanShiLuoZi"), EGameXXKCharacterRole::FormationMaster, 1, 0, 3, EGameXXKCardTargetMode::SingleEnemy, 3, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Formation.YiZhenHuiXiang"), EGameXXKCharacterRole::FormationMaster, 1, 1, 3, EGameXXKCardTargetMode::SingleEnemy, 2, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Formation.LianYingBuShi"), EGameXXKCharacterRole::FormationMaster, 1, 1, 0, EGameXXKCardTargetMode::SingleEnemy, 1, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None},
			{TEXT("Hero.Formation.LiuHeGuiYi"), EGameXXKCharacterRole::FormationMaster, 1, 2, 6, EGameXXKCardTargetMode::SingleEnemy, 7, 0, 0, false, EGameXXKHeavyArrowKind::None, EGameXXKHeroSpellTaskReward::None}
		};
		return Expected;
	}
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHeroCardCatalogDataTest,
	"GameXXK.Data.HeroCards.Catalog.Data",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHeroCardCatalogDataTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroCardCatalogTest;

	const TArray<FGameXXKCardDefinition>& AllDefinitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TArray<const FGameXXKCardDefinition*> HeroDefinitions;
	int32 IdentityLockedCount = 0;
	for (const FGameXXKCardDefinition& Definition : AllDefinitions)
	{
		if (Definition.bIdentityLocked)
		{
			++IdentityLockedCount;
		}
		if (Definition.Owner == EGameXXKCardOwner::Hero)
		{
			HeroDefinitions.Add(&Definition);
		}
	}

	const TArray<FExpectedHeroCard>& Expected = ExpectedHeroCards();
	TestEqual(TEXT("the protagonist pool has thirty-six cards"), HeroDefinitions.Num(), Expected.Num());
	TestEqual(TEXT("the full catalog grows to one hundred ninety-eight cards"), AllDefinitions.Num(), 198);
	TestEqual(TEXT("hero and named NPC identity locks total sixty"), IdentityLockedCount, 60);

	TSet<FName> SeenIds;
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		const FExpectedHeroCard& Row = Expected[Index];
		const FName ExpectedId(Row.Id);
		TestFalse(FString::Printf(TEXT("expected row %d is unique"), Index), SeenIds.Contains(ExpectedId));
		SeenIds.Add(ExpectedId);

		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(ExpectedId);
		TestNotNull(FString::Printf(TEXT("catalog contains %s"), Row.Id), Definition);
		if (!Definition)
		{
			continue;
		}
		if (HeroDefinitions.IsValidIndex(Index))
		{
			TestEqual(FString::Printf(TEXT("hero insertion order %d"), Index), HeroDefinitions[Index]->Id, ExpectedId);
		}
		TestEqual(FString::Printf(TEXT("%s remains a hero card"), Row.Id), Definition->Owner, EGameXXKCardOwner::Hero);
		TestEqual(FString::Printf(TEXT("%s runtime role is Hero"), Row.Id), Definition->Role, EGameXXKCharacterRole::Hero);
		TestEqual(FString::Printf(TEXT("%s linked role"), Row.Id), Definition->LinkedRole, Row.LinkedRole);
		TestEqual(FString::Printf(TEXT("%s unlock level"), Row.Id), Definition->HeroUnlockLevel, Row.UnlockLevel);
		TestEqual(FString::Printf(TEXT("%s energy cost"), Row.Id), Definition->EnergyCost, Row.EnergyCost);
		TestEqual(FString::Printf(TEXT("%s mana cost"), Row.Id), Definition->ManaCost, Row.ManaCost);
		TestEqual(FString::Printf(TEXT("%s target mode"), Row.Id), Definition->TargetSpec.Mode, Row.TargetMode);
		TestEqual(FString::Printf(TEXT("%s base effect count"), Row.Id), Definition->Effects.Num(), Row.BaseEffectCount);
		TestEqual(FString::Printf(TEXT("%s charge effect count"), Row.Id), Definition->ChargeEffects.Num(), Row.ChargeEffectCount);
		TestEqual(FString::Printf(TEXT("%s finish effect count"), Row.Id), Definition->FinishEffects.Num(), Row.FinishEffectCount);
		TestEqual(FString::Printf(TEXT("%s exhaust flag"), Row.Id), Definition->bExhaustOnPlay, Row.bExhaust);
		TestEqual(FString::Printf(TEXT("%s Heavy Arrow kind"), Row.Id), Definition->HeavyArrow.Kind, Row.HeavyArrowKind);
		TestEqual(FString::Printf(TEXT("%s spell reward"), Row.Id), Definition->SpellTaskReward, Row.SpellTaskReward);
		TestEqual(FString::Printf(TEXT("%s uses approved default quality"), Row.Id), Definition->BaseQuality, EGameXXKCardQuality::Common);
		TestTrue(FString::Printf(TEXT("%s remains identity locked"), Row.Id), Definition->bIdentityLocked);
	}

	const TArray<TPair<int32, int32>> UnlockCounts = {
		{1, 32}, {5, 33}, {10, 34}, {15, 35}, {20, 36}
	};
	for (const TPair<int32, int32>& ExpectedCount : UnlockCounts)
	{
		int32 ActualCount = 0;
		for (const FExpectedHeroCard& Row : Expected)
		{
			ActualCount += Row.UnlockLevel <= ExpectedCount.Key ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("expected table unlock count at level %d"), ExpectedCount.Key), ActualCount, ExpectedCount.Value);
	}

	const FGameXXKCardDefinition* QingFengDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
	TestNotNull(TEXT("Qing Feng keeps its catalog definition"), QingFengDefinition);
	if (QingFengDefinition && QingFengDefinition->Effects.IsValidIndex(1))
	{
		const FGameXXKCardBattleModifier& Discount = QingFengDefinition->Effects[1].Modifier;
		TestEqual(TEXT("Qing Feng discount belongs to the shared deck"), Discount.RecipientScope, EGameXXKCardModifierRecipientScope::SharedDeck);
		TestEqual(TEXT("Qing Feng shared-deck discount explicitly targets the played card"), Discount.RecipientTarget, EGameXXKCardEffectTarget::PlayedCard);
	}

	FGameXXKCardInstance FormationInstance;
	FormationInstance.InstanceId = TEXT("Formation.Targeting.Contract");
	FormationInstance.OwnerUnitId = TEXT("Player");
	FormationInstance.SourceEntryId = TEXT("Formation.Targeting.Entry");
	FormationInstance.AcquisitionOrdinal = 0;
	FGameXXKCardTargetUnit Player;
	Player.UnitId = TEXT("Player");
	Player.Side = EGameXXKCardTargetSide::Party;
	Player.bLiving = true;
	Player.HP = Player.MaxHP = 100;
	Player.StableSortOrder = 0;
	FGameXXKCardTargetUnit Enemy;
	Enemy.UnitId = TEXT("Enemy");
	Enemy.Side = EGameXXKCardTargetSide::Enemy;
	Enemy.bLiving = true;
	Enemy.HP = Enemy.MaxHP = 100;
	Enemy.StableSortOrder = 1;
	const TArray<FGameXXKCardTargetUnit> Units = {Player, Enemy};
	const EGameXXKCardTerrain Terrains[] = {
		EGameXXKCardTerrain::Plain,
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest,
		EGameXXKCardTerrain::WaterShore,
		EGameXXKCardTerrain::Village,
		EGameXXKCardTerrain::Cave
	};
	for (int32 Index = 32; Index < 36; ++Index)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FName(Expected[Index].Id));
		if (!Definition)
		{
			continue;
		}
		FormationInstance.CardId = Definition->Id;
		for (const EGameXXKCardTerrain Terrain : Terrains)
		{
			FGameXXKCardTargetRequest Request;
			FString Error;
			TestTrue(
				FString::Printf(TEXT("%s builds a target request on terrain %d"), Expected[Index].Id, static_cast<int32>(Terrain)),
				GameXXKCardRules::BuildTargetRequest(*Definition, FormationInstance, Terrain, Units, Request, &Error));
			TestEqual(
				FString::Printf(TEXT("%s stays manually enemy-targeted on terrain %d"), Expected[Index].Id, static_cast<int32>(Terrain)),
				Request.EffectiveMode,
				EGameXXKCardTargetMode::SingleEnemy);
		}
	}

	FString ValidationError;
	TestTrue(TEXT("the complete protagonist catalog validates"), FGameXXKCardCatalog::ValidateCardDefinitions(ValidationError));
	return true;
}

#endif
