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

	FGameXXKCardCombatUnit SorcererSource = MakeUnit(
		TEXT("Display.Sorcerer"),
		EGameXXKCardTargetSide::Party,
		EGameXXKCharacterRole::Sorcerer,
		137,
		100,
		1);
	SorcererSource.MaxMana = 34;
	SorcererSource.Mana = 34;
	const FGameXXKCardDefinition* SorcererAttackDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.LingHuoFu"));
	FGameXXKCardPlayPreview SorcererAttackPreview;
	TestTrue(TEXT("special Sorcerer attack builds a panel-aware preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*SorcererAttackDefinition,
		EGameXXKCardQuality::Common,
		SorcererSource,
		100,
		{},
		SorcererAttackPreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* SorcererAttack = FindValue(
		SorcererAttackPreview,
		EGameXXKCardDisplayValueKind::AttackDamage);
	TestEqual(TEXT("special Sorcerer attack uses Attack137"), SorcererAttack ? SorcererAttack->ResolvedMagnitude : INDEX_NONE, 95);
	const FString SorcererAttackCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*SorcererAttackDefinition,
		EGameXXKCardQuality::Common,
		&SorcererAttackPreview,
		Context);
	const FString SorcererAttackDetail = GameXXKCardText::DescribeExpandedTooltipBody(
		*SorcererAttackDefinition,
		EGameXXKCardQuality::Common,
		&SorcererAttackPreview,
		Context);
	TestTrue(TEXT("special Sorcerer compact text shows generated damage"), SorcererAttackCompact.Contains(TEXT("造成95点伤害")));
	TestTrue(TEXT("special Sorcerer detail keeps only the attack multiplier"), SorcererAttackDetail.Contains(TEXT("造成70%的攻击伤害")));
	TestFalse(TEXT("special Sorcerer attack sentence omits an inline target"), SorcererAttackDetail.Contains(TEXT("敌方全体造成70%的攻击伤害")));

	const FGameXXKCardDefinition* FireDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.LiHuoYin"));
	FGameXXKCardPlayPreview FirePreview;
	TestTrue(TEXT("special Sorcerer fire card builds a panel-aware preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*FireDefinition,
		EGameXXKCardQuality::Common,
		SorcererSource,
		100,
		{},
		FirePreview,
		&Error));
	const FString FireCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*FireDefinition,
		EGameXXKCardQuality::Common,
		&FirePreview,
		Context);
	const FString FireDetail = GameXXKCardText::DescribeExpandedTooltipBody(
		*FireDefinition,
		EGameXXKCardQuality::Common,
		&FirePreview,
		Context);
	TestTrue(TEXT("special Sorcerer fire compact uses actual Attack"), FireCompact.Contains(TEXT("造成82点伤害")));
	TestTrue(TEXT("special Sorcerer fire compact uses generated DOT"), FireCompact.Contains(TEXT("10点灼烧")));
	TestTrue(TEXT("special Sorcerer fire detail combines level and quality"), FireDetail.Contains(TEXT("2点灼烧，500%增幅倍率")));

	FGameXXKCardCombatUnit BladeSource = SorcererSource;
	BladeSource.UnitId = TEXT("Display.Blade");
	BladeSource.Role = EGameXXKCharacterRole::Blade;
	const FGameXXKCardDefinition* BladeDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.LieFengZhan"));
	FGameXXKCardPlayPreview BladePreview;
	TestTrue(TEXT("special Blade card builds a panel-aware preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*BladeDefinition,
		EGameXXKCardQuality::Common,
		BladeSource,
		100,
		{},
		BladePreview,
		&Error));
	const FString BladeCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*BladeDefinition,
		EGameXXKCardQuality::Common,
		&BladePreview,
		Context);
	const FString BladeDetail = GameXXKCardText::DescribeExpandedTooltipBody(
		*BladeDefinition,
		EGameXXKCardQuality::Common,
		&BladePreview,
		Context);
	TestTrue(TEXT("special Blade compact uses actual Attack"), BladeCompact.Contains(TEXT("造成137点伤害")));
	TestTrue(TEXT("special Blade compact uses generated DOT"), BladeCompact.Contains(TEXT("5点流血")));
	TestTrue(TEXT("special Blade detail combines DOT level and quality"), BladeDetail.Contains(TEXT("1点流血，500%增幅倍率")));
	const FGameXXKCardDefinition* MultiHitBladeDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.JiYuLianZhan"));
	FGameXXKCardPlayPreview MultiHitBladePreview;
	TestTrue(TEXT("multi-hit Blade card builds a panel-aware preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*MultiHitBladeDefinition,
		EGameXXKCardQuality::Common,
		BladeSource,
		100,
		{},
		MultiHitBladePreview,
		&Error));
	const FString MultiHitBladeCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*MultiHitBladeDefinition,
		EGameXXKCardQuality::Common,
		&MultiHitBladePreview,
		Context);
	TestTrue(
		TEXT("multi-hit compact text shows the generated per-hit value"),
		MultiHitBladeCompact.Contains(TEXT("攻击3次，每次造成116点伤害")));

	const FGameXXKCardDefinition* IceDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.SheLingHuo"));
	FGameXXKCardPlayPreview IcePreview;
	TestTrue(TEXT("Ice overflow card builds a panel-aware preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*IceDefinition,
		EGameXXKCardQuality::Rare,
		SorcererSource,
		100,
		{},
		IcePreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* ManaRecovery = FindValue(
		IcePreview,
		EGameXXKCardDisplayValueKind::ManaRecovery);
	TestNotNull(TEXT("Ice preview exposes Mana recovery separately"), ManaRecovery);
	if (ManaRecovery)
	{
		TestEqual(TEXT("Ice preview requests ten percent current Mana rounded up"), ManaRecovery->ResolvedMagnitude, 4);
		TestEqual(TEXT("full Mana accepts none of the recovery"), ManaRecovery->ActualMagnitude, 0);
		TestEqual(TEXT("full Mana overflows all four points"), ManaRecovery->OverflowMagnitude, 4);
		TestEqual(TEXT("Rare level100 overflow uses a combined600 percent amplification"), ManaRecovery->AmplificationPercent, 600);
	}
	const FGameXXKCardResolvedDisplayValue* OverflowArmor = FindValue(
		IcePreview,
		EGameXXKCardDisplayValueKind::Armor);
	TestEqual(TEXT("four overflow Mana converts to24 Armor"), OverflowArmor ? OverflowArmor->ResolvedMagnitude : INDEX_NONE, 24);
	const FString IceCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*IceDefinition,
		EGameXXKCardQuality::Rare,
		&IcePreview,
		Context);
	const FString IceDetail = GameXXKCardText::DescribeExpandedTooltipBody(
		*IceDefinition,
		EGameXXKCardQuality::Rare,
		&IcePreview,
		Context);
	TestTrue(TEXT("Ice compact separates recovery and converted Armor"), IceCompact.Contains(TEXT("回复4点内力")) && IceCompact.Contains(TEXT("24点护甲")));
	TestTrue(TEXT("Ice detail keeps current-Mana coefficient and combined amplification"), IceDetail.Contains(TEXT("当前内力的10%")) && IceDetail.Contains(TEXT("600%增幅倍率")));

	const FGameXXKCardDefinition* MaxManaIceDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.FenMaiFu"));
	FGameXXKCardPlayPreview MaxManaIcePreview;
	TestTrue(TEXT("Max-Mana Ice card builds a staged preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*MaxManaIceDefinition,
		EGameXXKCardQuality::Common,
		SorcererSource,
		100,
		{},
		MaxManaIcePreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* MaxManaRecovery = FindValue(
		MaxManaIcePreview,
		EGameXXKCardDisplayValueKind::ManaRecovery);
	TestEqual(TEXT("Max-Mana increase happens before ten-percent recovery"), MaxManaRecovery ? MaxManaRecovery->ResolvedMagnitude : INDEX_NONE, 4);
	TestEqual(TEXT("expanded cap accepts the complete recovery"), MaxManaRecovery ? MaxManaRecovery->ActualMagnitude : INDEX_NONE, 4);
	TestEqual(TEXT("expanded cap leaves no overflow"), MaxManaRecovery ? MaxManaRecovery->OverflowMagnitude : INDEX_NONE, 0);

	const FGameXXKCardDefinition* PartyOverflowDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.XingHuoHuiShou"));
	FGameXXKCardPlayPreview PartyOverflowPreview;
	TestTrue(TEXT("party overflow card builds a post-cost preview"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*PartyOverflowDefinition,
		EGameXXKCardQuality::Rare,
		SorcererSource,
		100,
		{},
		PartyOverflowPreview,
		&Error));
	const FGameXXKCardResolvedDisplayValue* PartyManaRecovery = FindValue(
		PartyOverflowPreview,
		EGameXXKCardDisplayValueKind::ManaRecovery);
	TestEqual(TEXT("party overflow generates eight Mana after paying four"), PartyManaRecovery ? PartyManaRecovery->ResolvedMagnitude : INDEX_NONE, 8);
	TestEqual(TEXT("party overflow restores four Mana to the caster"), PartyManaRecovery ? PartyManaRecovery->ActualMagnitude : INDEX_NONE, 4);
	TestEqual(TEXT("party overflow converts the remaining four Mana"), PartyManaRecovery ? PartyManaRecovery->OverflowMagnitude : INDEX_NONE, 4);
	const FString PartyOverflowCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*PartyOverflowDefinition,
		EGameXXKCardQuality::Rare,
		&PartyOverflowPreview,
		Context);
	TestTrue(
		TEXT("party overflow compact text reports one shared Armor grant"),
		PartyOverflowCompact.Contains(TEXT("仅自身回复8点内力"))
			&& PartyOverflowCompact.Contains(TEXT("全体友方各获得24点护甲")));

	FGameXXKCardCombatUnit HealerSource = SorcererSource;
	HealerSource.UnitId = TEXT("Display.Healer");
	HealerSource.Role = EGameXXKCharacterRole::Healer;
	const FGameXXKCardDefinition* BranchHealingDefinition =
		FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Healer.YaoYin"));
	FGameXXKCardPlayPreview BranchHealingPreview;
	TestTrue(TEXT("ally/enemy healing card builds every target branch"), GameXXKCardRules::BuildReferenceCardPlayPreview(
		*BranchHealingDefinition,
		EGameXXKCardQuality::Common,
		HealerSource,
		100,
		{},
		BranchHealingPreview,
		&Error));
	const FString BranchHealingCompact = GameXXKCardText::DescribeCompactTooltipBody(
		*BranchHealingDefinition,
		EGameXXKCardQuality::Common,
		&BranchHealingPreview,
		Context);
	const FString BranchHealingDetail = GameXXKCardText::DescribeExpandedTooltipBody(
		*BranchHealingDefinition,
		EGameXXKCardQuality::Common,
		&BranchHealingPreview,
		Context);
	TestTrue(TEXT("ally branch compact uses generated30-point coefficient healing"), BranchHealingCompact.Contains(TEXT("150点治疗")));
	TestTrue(TEXT("enemy branch compact uses generated15-point coefficient party healing"), BranchHealingCompact.Contains(TEXT("全体友方各获得75点治疗")));
	TestFalse(TEXT("compact branch text preserves late effects without hard ellipsis"), BranchHealingCompact.Contains(TEXT("…")));
	TestTrue(TEXT("ally branch detail combines level and quality"), BranchHealingDetail.Contains(TEXT("30点治疗，500%增幅倍率")));
	TestTrue(TEXT("enemy branch detail combines level and quality"), BranchHealingDetail.Contains(TEXT("15点治疗，500%增幅倍率")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAllCardReferenceTextTest,
	"GameXXK.Data.CardText.All173ReferencePreviews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAllCardReferenceTextTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardResolvedTextTest;
	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("reference-text audit covers the active card catalog"), Definitions.Num(), 173);
	int32 QualityVersionCount = 0;
	for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
	{
		const FGameXXKCardDefinition& Definition = Definitions[DefinitionIndex];
		const EGameXXKCharacterRole Role = Definition.Owner == EGameXXKCardOwner::Profession
			? Definition.Role
			: Definition.Owner == EGameXXKCardOwner::QuestNpc
				? EGameXXKCharacterRole::QuestNpc
				: EGameXXKCharacterRole::Hero;
		FGameXXKCardCombatUnit Source = MakeUnit(
			FName(*FString::Printf(TEXT("Display.Catalog.%d"), DefinitionIndex)),
			EGameXXKCardTargetSide::Party,
			Role,
			137,
			113,
			1);
		Source.MaxMana = 34;
		Source.Mana = 34;
		for (int32 QualityValue = static_cast<int32>(Definition.BaseQuality);
			QualityValue <= static_cast<int32>(EGameXXKCardQuality::Epic);
			++QualityValue)
		{
			const EGameXXKCardQuality Quality = static_cast<EGameXXKCardQuality>(QualityValue);
			++QualityVersionCount;
			FGameXXKCardPlayPreview Preview;
			FString Error;
			const FString ContextLabel = FString::Printf(
				TEXT("%s quality=%d"),
				*Definition.Id.ToString(),
				QualityValue);
			if (!GameXXKCardRules::BuildReferenceCardPlayPreview(
					Definition,
					Quality,
					Source,
					100,
					{},
					Preview,
					&Error))
			{
				AddError(FString::Printf(TEXT("%s reference preview failed: %s"), *ContextLabel, *Error));
				continue;
			}
			const FString Compact = GameXXKCardText::DescribeCompactTooltipBody(
				Definition,
				Quality,
				&Preview,
				{});
			const FString Detail = GameXXKCardText::DescribeExpandedTooltipBody(
				Definition,
				Quality,
				&Preview,
				{});
			TestFalse(*FString::Printf(TEXT("%s compact text is populated"), *ContextLabel), Compact.IsEmpty());
			TestFalse(*FString::Printf(TEXT("%s detail text is populated"), *ContextLabel), Detail.IsEmpty());
			TestFalse(
				*FString::Printf(TEXT("%s has no unresolved formatter token"), *ContextLabel),
				Compact.Contains(TEXT("未知")) || Detail.Contains(TEXT("未知"))
					|| Compact.Contains(TEXT("无效")) || Detail.Contains(TEXT("无效")));
			TestFalse(
				*FString::Printf(TEXT("%s compact text has no hard semantic truncation"), *ContextLabel),
				Compact.Contains(TEXT("…")));
		}
	}
	TestEqual(TEXT("reference-text audit covers all legal quality versions"), QualityVersionCount, 419);
	return true;
}

#endif
