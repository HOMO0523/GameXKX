#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCardText.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCardResolvedTextTest
{
	const FName OwnerId(TEXT("Display.Owner"));
	const FName AllyId(TEXT("Display.Ally"));
	const FName EnemyId(TEXT("Display.Enemy"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 Defense,
		const int32 Sort)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 1000 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 50 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = 10;
		Unit.StableSortOrder = Sort;
		Unit.CombatLevel = 100;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName CardId, const EGameXXKCardQuality Quality, const int32 Index)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Display.Card.%d"), Index));
		Card.CardId = CardId;
		Card.CurrentQuality = Quality;
		Card.OwnerUnitId = OwnerId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Display.Source.%d"), Index));
		Card.AcquisitionOrdinal = Index;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		const EGameXXKCardQuality Quality,
		FGameXXKCardBattleRuntime& OutRuntime,
		const int32 OwnerAttack = 100,
		const int32 OwnerDefense = 100)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Cards.Add(MakeCard(CardId, Quality, Index));
		}
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, OwnerAttack, OwnerDefense, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 80, 80, 2),
			MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 20, 10)};
		Units.Last().Armor = 10;
		FString Error;
		const bool bBuilt = GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			74005,
			&Error);
		Test.TestTrue(FString::Printf(TEXT("resolved-text runtime initializes: %s"), *Error), bBuilt);
		if (bBuilt) OutRuntime.Deck.SharedEnergy = 10;
		return bBuilt;
	}

	const FGameXXKCardResolvedDisplayValue* FindValue(
		const FGameXXKCardPlayPreview& Preview,
		const EGameXXKCardDisplayValueKind Kind,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		return Preview.ResolvedDisplayValues.FindByPredicate([Kind, Status](const FGameXXKCardResolvedDisplayValue& Value)
		{
			return Value.Kind == Kind && (Status == EGameXXKCardStatus::None || Value.Status == Status);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardResolvedTextBoundaryTest,
	"GameXXK.Data.CardText.ResolvedValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardResolvedTextBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolvedTextTest;
	FString Error;
	FGameXXKCardTooltipContext Context;

	FGameXXKCardBattleRuntime AttackRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCardQuality::Rare, AttackRuntime)) return false;
	FGameXXKCardPlayPreview AttackPreview;
	TestTrue(TEXT("rare attack builds preview"), GameXXKCardRules::BuildCardPlayPreview(AttackRuntime, AttackRuntime.Deck.Hand[0].InstanceId, AttackPreview, &Error));
	const FGameXXKCardResolvedDisplayValue* AttackValue = FindValue(AttackPreview, EGameXXKCardDisplayValueKind::AttackDamage);
	TestNotNull(TEXT("attack preview owns a generated value"), AttackValue);
	if (AttackValue)
	{
		TestEqual(TEXT("Attack100 and 120 percent generate card value120"), AttackValue->ResolvedMagnitude, 120);
		TestEqual(TEXT("copied neutral target resolves to HP loss90"), AttackValue->ActualMagnitude, 90);
		TestEqual(TEXT("attack display records actual source"), AttackValue->SourceUnitId, OwnerId);
	}
	const FGameXXKCardDefinition* AttackDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
	const FString CompactAttack = GameXXKCardText::DescribeCompactTooltipBody(*AttackDefinition, EGameXXKCardQuality::Rare, &AttackPreview, Context);
	const FString DetailAttack = GameXXKCardText::DescribeExpandedTooltipBody(*AttackDefinition, EGameXXKCardQuality::Rare, &AttackPreview, Context);
	TestTrue(TEXT("compact attack shows generated integer"), CompactAttack.Contains(TEXT("造成120点伤害")));
	TestTrue(TEXT("detail attack shows only final attack percentage"), DetailAttack.Contains(TEXT("造成120%的攻击伤害")));
	TestFalse(TEXT("detail attack omits source Attack and arithmetic"), DetailAttack.Contains(TEXT("攻击100")) || DetailAttack.Contains(TEXT("100 ×")) || DetailAttack.Contains(TEXT("= 120")));
	TestFalse(TEXT("card tooltip omits target mitigation and HP loss"), DetailAttack.Contains(TEXT("防御20")) || DetailAttack.Contains(TEXT("吸收10")) || DetailAttack.Contains(TEXT("损失90")));
	FGameXXKCardPlayPreview DiscountedAttackPreview = AttackPreview;
	DiscountedAttackPreview.EffectiveEnergyCost = 0;
	DiscountedAttackPreview.EffectiveManaCost = 7;
	TestTrue(
		TEXT("compact tooltip shows preview-resolved costs"),
		GameXXKCardText::DescribeCompactTooltipBody(*AttackDefinition, EGameXXKCardQuality::Rare, &DiscountedAttackPreview, Context)
			.Contains(TEXT("费用：0 气 · 7 内")));
	TestTrue(
		TEXT("expanded tooltip shows preview-resolved costs"),
		GameXXKCardText::DescribeExpandedTooltipBody(*AttackDefinition, EGameXXKCardQuality::Rare, &DiscountedAttackPreview, Context)
			.Contains(TEXT("费用：0 气 / 7 内")));

	FGameXXKCardBattleRuntime DotRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Blade.XueLuXiangCheng"), EGameXXKCardQuality::Rare, DotRuntime)) return false;
	FGameXXKCardPlayPreview DotPreview;
	TestTrue(TEXT("DOT card builds preview"), GameXXKCardRules::BuildCardPlayPreview(DotRuntime, DotRuntime.Deck.Hand[0].InstanceId, DotPreview, &Error));
	const FGameXXKCardResolvedDisplayValue* DotValue = FindValue(DotPreview, EGameXXKCardDisplayValueKind::DamageOverTime, EGameXXKCardStatus::Bleed);
	TestNotNull(TEXT("DOT preview owns a generated value"), DotValue);
	if (DotValue)
	{
		TestEqual(TEXT("coefficient6 Rare level100 generates36 Bleed"), DotValue->ResolvedMagnitude, 36);
		TestEqual(TEXT("DOT preview folds quality and level into600 percent"), DotValue->AmplificationPercent, 600);
	}
	const FGameXXKCardDefinition* DotDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Blade.XueLuXiangCheng"));
	TestTrue(TEXT("compact DOT shows generated points"), GameXXKCardText::DescribeCompactTooltipBody(*DotDefinition, EGameXXKCardQuality::Rare, &DotPreview, Context).Contains(TEXT("36点流血")));
	TestTrue(TEXT("detail DOT shows base and combined multiplier"), GameXXKCardText::DescribeExpandedTooltipBody(*DotDefinition, EGameXXKCardQuality::Rare, &DotPreview, Context).Contains(TEXT("6点流血，600%增幅倍率")));

	FGameXXKCardBattleRuntime HealRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Healer.HuiChunNiMai"), EGameXXKCardQuality::Rare, HealRuntime)) return false;
	FGameXXKCardPlayPreview HealPreview;
	TestTrue(TEXT("healing card builds preview"), GameXXKCardRules::BuildCardPlayPreview(HealRuntime, HealRuntime.Deck.Hand[0].InstanceId, HealPreview, &Error));
	const FGameXXKCardResolvedDisplayValue* HealValue = FindValue(HealPreview, EGameXXKCardDisplayValueKind::Healing);
	TestNotNull(TEXT("healing preview owns a generated value"), HealValue);
	if (HealValue)
	{
		TestEqual(TEXT("coefficient25 Rare level100 generates150 healing"), HealValue->ResolvedMagnitude, 150);
		TestEqual(TEXT("healing preview folds quality and level into600 percent"), HealValue->AmplificationPercent, 600);
	}
	const FGameXXKCardDefinition* HealDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Healer.HuiChunNiMai"));
	TestTrue(TEXT("compact healing shows generated points"), GameXXKCardText::DescribeCompactTooltipBody(*HealDefinition, EGameXXKCardQuality::Rare, &HealPreview, Context).Contains(TEXT("150点治疗")));
	TestTrue(TEXT("detail healing shows base and combined multiplier"), GameXXKCardText::DescribeExpandedTooltipBody(*HealDefinition, EGameXXKCardQuality::Rare, &HealPreview, Context).Contains(TEXT("25点治疗，600%增幅倍率")));

	FGameXXKCardBattleRuntime ArmorRuntime;
	if (!BuildRuntime(*this, TEXT("Hero.Generic.HengJianShouShi"), EGameXXKCardQuality::Rare, ArmorRuntime)) return false;
	FGameXXKCardPlayPreview ArmorPreview;
	TestTrue(TEXT("Armor card builds preview"), GameXXKCardRules::BuildCardPlayPreview(ArmorRuntime, ArmorRuntime.Deck.Hand[0].InstanceId, ArmorPreview, &Error));
	const FGameXXKCardResolvedDisplayValue* ArmorValue = FindValue(ArmorPreview, EGameXXKCardDisplayValueKind::Armor);
	TestNotNull(TEXT("Armor preview owns a generated value"), ArmorValue);
	if (ArmorValue) TestEqual(TEXT("Defense100 80 percent Rare generates96 Armor"), ArmorValue->ResolvedMagnitude, 96);
	const FGameXXKCardDefinition* ArmorDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.HengJianShouShi"));
	TestTrue(TEXT("compact Armor shows generated points"), GameXXKCardText::DescribeCompactTooltipBody(*ArmorDefinition, EGameXXKCardQuality::Rare, &ArmorPreview, Context).Contains(TEXT("96点护甲")));
	TestTrue(TEXT("detail Armor keeps coefficient and quality multiplier"), GameXXKCardText::DescribeExpandedTooltipBody(*ArmorDefinition, EGameXXKCardQuality::Rare, &ArmorPreview, Context).Contains(TEXT("80%防御的护甲，120%增幅倍率")));

	FGameXXKCardCombatUnit ReferenceSource = MakeUnit(OwnerId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 173, 91, 1);
	FGameXXKCardPlayPreview ReferencePreview;
	TestTrue(TEXT("explicit panel snapshot builds a reference preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*AttackDefinition,
		EGameXXKCardQuality::Rare,
		ReferenceSource,
		100,
		{},
		ReferencePreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* ReferenceAttack = FindValue(ReferencePreview, EGameXXKCardDisplayValueKind::AttackDamage);
	TestEqual(TEXT("reference preview uses supplied panel Attack173 with combat-floor rounding"), ReferenceAttack ? ReferenceAttack->ResolvedMagnitude : INDEX_NONE, 207);

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("adapter reference fixture initializes card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.PlayerLevel = 100;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
	FGameXXKEquipmentLoadoutSnapshot HeroSnapshot;
	TestTrue(TEXT("adapter reference fixture builds authoritative hero panel"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
		State.EquipmentCollection,
		FGameXXKEquipmentRules::HeroCharacterId(),
		FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel),
		HeroSnapshot,
		&Error));
	FGameXXKCardPlayPreview AdapterPreview;
	TestTrue(TEXT("adapter projects the real hero panel into card text"), FGameXXKCardBattleAdapter::BuildReferenceCardPlayPreview(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		AttackDefinition->Id,
		EGameXXKCardQuality::Rare,
		AdapterPreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* AdapterAttack = FindValue(AdapterPreview, EGameXXKCardDisplayValueKind::AttackDamage);
	const int32 ExpectedAdapterAttack = HeroSnapshot.AttributesBeforeRoute.Attack * 120 / 100;
	TestEqual(TEXT("adapter compact value equals the authoritative equipped Attack"), AdapterAttack ? AdapterAttack->ResolvedMagnitude : INDEX_NONE, ExpectedAdapterAttack);
	return true;
}

#endif
