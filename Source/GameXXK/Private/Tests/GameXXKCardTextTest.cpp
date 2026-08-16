#include "GameXXKCardCatalog.h"
#include "GameXXKCardRunTypes.h"
#include "GameXXKCardText.h"
#include "GameXXKEnemyText.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardTextTest,
	"GameXXK.Integration.CardText",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardTextTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("the card catalogue remains complete while its player text is generated"), Definitions.Num(), 198);
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const FString Detail = GameXXKCardText::DescribeDetail(Definition, nullptr);
		TestFalse(FString::Printf(TEXT("%s has non-empty player-facing detail text"), *Definition.Id.ToString()), Detail.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has no unresolved player-facing formatter fallback"), *Definition.Id.ToString()), Detail.Contains(TEXT("未知")));
		TestFalse(FString::Printf(TEXT("%s has no invalid player-facing formatter fallback"), *Definition.Id.ToString()), Detail.Contains(TEXT("无效")));
		TestFalse(FString::Printf(TEXT("%s never exposes the retired medicine name"), *Definition.Id.ToString()), Detail.Contains(TEXT("药材")));
	}

	struct FVisibleCardTextCase
	{
		FName CardId;
		const TCHAR* ExpectedText;
		const TCHAR* ForbiddenToken;
	};
	const TArray<FVisibleCardTextCase> VisibleCardTextCases = {
		{TEXT("Hero.Formation.LianYingBuShi"), TEXT("触发当前地势收益1次"), TEXT("TriggerTerrainBenefit")},
		{TEXT("Profession.Guard.TieBiRuShan"), TEXT("破绽免疫"), TEXT("CannotReceiveVulnerability")},
		{TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("地形免耗"), TEXT("NextTerrainCardFree")},
		{TEXT("Route.Terrain.XingJunBuZhen"), TEXT("地形减耗"), TEXT("NextTerrainCardEnergyReduction")}};
	for (const FVisibleCardTextCase& TextCase : VisibleCardTextCases)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(TextCase.CardId);
		TestNotNull(*FString::Printf(TEXT("%s text fixture exists"), *TextCase.CardId.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		const FString Detail = GameXXKCardText::DescribeDetail(*Definition, nullptr);
		TestTrue(
			*FString::Printf(TEXT("%s uses concise Chinese player text"), *TextCase.CardId.ToString()),
			Detail.Contains(TextCase.ExpectedText));
		TestFalse(
			*FString::Printf(TEXT("%s does not expose an internal enum token"), *TextCase.CardId.ToString()),
			Detail.Contains(TextCase.ForbiddenToken));
	}

	const FGameXXKCardDefinition* QingFeng = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
	const FGameXXKCardDefinition* GuiYuan = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.GuiYuanShu"));
	const FGameXXKCardDefinition* TerrainSwitch = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.YinShuiHuiYuan"));
	const FGameXXKCardDefinition* Consumption = FGameXXKCardCatalog::FindCardDefinition(TEXT("Route.Boss.FuHuDuanJiang"));
	const FGameXXKCardDefinition* DelayedModifier = FGameXXKCardCatalog::FindCardDefinition(TEXT("Route.Rare.TieYiYiJue"));
	const FGameXXKCardDefinition* Momentum = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.NingShenTuNa"));
	const FGameXXKCardDefinition* Blade = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Blade.TongFengYinShi"));
	const FGameXXKCardDefinition* Medicine = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Healer.HuiChunNiMai"));
	const FGameXXKCardDefinition* HeavyArrow = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Hunter.HuiFengGuanRi"));
	const FGameXXKCardDefinition* SpellTask = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Mage.YanXuLiaoYuan"));
	const FGameXXKCardDefinition* TerrainBenefit = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Formation.GuanShiLuoZi"));
	TestNotNull(TEXT("the manual enemy target fixture exists"), QingFeng);
	TestNotNull(TEXT("the manual ally target fixture exists"), GuiYuan);
	TestNotNull(TEXT("the fixed-target terrain-switch fixture exists"), TerrainSwitch);
	TestNotNull(TEXT("the consumed-status fixture exists"), Consumption);
	TestNotNull(TEXT("the delayed modifier fixture exists"), DelayedModifier);
	TestNotNull(TEXT("the momentum naming fixture exists"), Momentum);
	TestNotNull(TEXT("the Blade keyword fixture exists"), Blade);
	TestNotNull(TEXT("the Medicine keyword fixture exists"), Medicine);
	TestNotNull(TEXT("the Heavy Arrow keyword fixture exists"), HeavyArrow);
	TestNotNull(TEXT("the spell-task keyword fixture exists"), SpellTask);
	TestNotNull(TEXT("the terrain-benefit keyword fixture exists"), TerrainBenefit);
	if (!QingFeng || !GuiYuan || !TerrainSwitch || !Consumption || !DelayedModifier
		|| !Momentum || !Blade || !Medicine || !HeavyArrow || !SpellTask || !TerrainBenefit)
	{
		return false;
	}

	const FString QingFengText = GameXXKCardText::DescribeDetail(*QingFeng, nullptr);
	const FString GuiYuanText = GameXXKCardText::DescribeDetail(*GuiYuan, nullptr);
	const FString TerrainSwitchText = GameXXKCardText::DescribeDetail(*TerrainSwitch, nullptr);
	const FString ConsumptionText = GameXXKCardText::DescribeDetail(*Consumption, nullptr);
	const FString DelayedModifierText = GameXXKCardText::DescribeDetail(*DelayedModifier, nullptr);
	TestTrue(TEXT("manual enemy target text explains the player selection"), QingFengText.Contains(TEXT("单体敌方")) && QingFengText.Contains(TEXT("选择目标")));
	TestTrue(TEXT("manual enemy target text explains attack damage"), QingFengText.Contains(TEXT("攻击伤害")));
	TestTrue(TEXT("manual ally target text explains the player selection"), GuiYuanText.Contains(TEXT("单体友方")) && GuiYuanText.Contains(TEXT("选择目标")));
	TestTrue(TEXT("terrain-switch text names its fixed destination and immediate benefit"),
		TerrainSwitchText.Contains(TEXT("切换至水岸")) && TerrainSwitchText.Contains(TEXT("触发当前地势收益1次")));
	TestTrue(TEXT("consumed-status text explains the consumption"), ConsumptionText.Contains(TEXT("消耗")));
	TestTrue(TEXT("delayed modifier text explains the deferred trigger"), DelayedModifierText.Contains(TEXT("触发")));

	const FString MomentumText = GameXXKCardText::DescribeDetail(*Momentum, nullptr);
	const FString BladeText = GameXXKCardText::DescribeDetail(*Blade, nullptr);
	const FString MedicineText = GameXXKCardText::DescribeDetail(*Medicine, nullptr);
	const FString HeavyArrowText = GameXXKCardText::DescribeDetail(*HeavyArrow, nullptr);
	const FString SpellTaskText = GameXXKCardText::DescribeDetail(*SpellTask, nullptr);
	const FString TerrainBenefitText = GameXXKCardText::DescribeDetail(*TerrainBenefit, nullptr);
	TestTrue(TEXT("card text uses the approved Momentum name"), MomentumText.Contains(TEXT("气势")));
	TestFalse(TEXT("card text never falls back to the retired one-character Momentum name"), MomentumText.Contains(TEXT("获得2层势")));
	TestTrue(TEXT("Blade cards expose the exact concise Charge keyword rule"), BladeText.Contains(TEXT("冲锋：此牌是本回合第一张主动牌打出时触发。")));
	TestTrue(TEXT("Blade cards expose the exact concise Finish keyword rule"), BladeText.Contains(TEXT("收招：此牌是本回合最后一张主动牌时触发。")));
	TestTrue(TEXT("Blade cards describe their actual Charge payload"), BladeText.Contains(TEXT("冲锋效果：")) && BladeText.Contains(TEXT("重放")));
	TestTrue(TEXT("Blade cards describe their actual Finish payload"), BladeText.Contains(TEXT("收招效果：")) && BladeText.Contains(TEXT("重放")));
	TestTrue(TEXT("Medicine cards expose the exact concise Medicine keyword rule"), MedicineText.Contains(TEXT("药效：下一次治疗或治疗反转每层＋1；结算时全部消耗。")));
	TestTrue(TEXT("Heavy Arrow cards expose the exact concise Heavy Arrow keyword rule"), HeavyArrowText.Contains(TEXT("重箭：消耗全部蓄力，逐层触发本牌重箭效果。")));
	TestTrue(TEXT("Heavy Arrow cards retain their data-defined per-layer payload"), HeavyArrowText.Contains(TEXT("每消耗1层")) && HeavyArrowText.Contains(TEXT("抽1张牌")));
	TestTrue(TEXT("spell-task cards expose the exact concise task rule"), SpellTaskText.Contains(TEXT("法术任务：主角8张装备牌各主动打出一次后，依序重放基础效果并触发阵赏。")));
	TestTrue(TEXT("spell-task cards retain the data-defined reward"), SpellTaskText.Contains(TEXT("阵赏·炎")) && SpellTaskText.Contains(TEXT("8层灼烧")));
	TestTrue(TEXT("terrain cards expose the exact concise current-terrain rule"), TerrainBenefitText.Contains(TEXT("地势：按当前地势触发对应效果。")));
	TestTrue(TEXT("terrain cards retain the data-defined terrain payload"), TerrainBenefitText.Contains(TEXT("触发当前地势收益1次")));

	const FGameXXKCardDefinition* BladeLieFeng = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.LieFengZhan"));
	const FGameXXKCardDefinition* BladeFengHou = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.FengHou"));
	const FGameXXKCardDefinition* BladeDuanYue = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.DuanYue"));
	const FGameXXKCardDefinition* BladeJingHong = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.JingHongChuQiao"));
	const FGameXXKCardDefinition* BladeHengYun = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.HengYunKaiFeng"));
	TestNotNull(TEXT("the Blade charge/finish fixture exists"), BladeLieFeng);
	TestNotNull(TEXT("the Blood Edge keyword fixture exists"), BladeFengHou);
	TestNotNull(TEXT("the Momentum Break keyword fixture exists"), BladeDuanYue);
	TestNotNull(TEXT("the Sheathed open-blade fixture exists"), BladeJingHong);
	TestNotNull(TEXT("the Sheathed residual fixture exists"), BladeHengYun);
	if (BladeLieFeng && BladeFengHou && BladeDuanYue && BladeJingHong && BladeHengYun)
	{
		const FString LieFengText = GameXXKCardText::DescribeDetail(*BladeLieFeng, nullptr);
		const FString FengHouText = GameXXKCardText::DescribeDetail(*BladeFengHou, nullptr);
		const FString DuanYueText = GameXXKCardText::DescribeDetail(*BladeDuanYue, nullptr);
		const FString JingHongText = GameXXKCardText::DescribeDetail(*BladeJingHong, nullptr);
		const FString HengYunText = GameXXKCardText::DescribeDetail(*BladeHengYun, nullptr);
		TestTrue(TEXT("Blade charge text names its trigger and payload"),
			LieFengText.Contains(TEXT("冲锋：此牌是本回合第一张主动牌打出时，下一张主动牌基础效果再结算一次。")));
		TestTrue(TEXT("Blade finish text names its trigger and payload"),
			LieFengText.Contains(TEXT("收招：此牌是本回合最后一张主动牌时，返还本回合首张主动牌至手牌（原费用）。")));
		TestTrue(TEXT("Blood Edge cards expose the blood-edge keyword"),
			FengHouText.Contains(TEXT("血势：每层目标流血使本段攻击倍率+10%。")));
		TestTrue(TEXT("Momentum Break cards expose the momentum keyword"),
			DuanYueText.Contains(TEXT("乘势：每层自身气势使本段攻击倍率+10%。")));
		TestTrue(TEXT("Sheathed cards explain storing the charge as a native style"),
			JingHongText.Contains(TEXT("收招：此牌是本回合最后一张主动牌时，将冲锋效果保存为「藏式」。")));
		TestTrue(TEXT("Sheathed cards explain the style expiry"),
			JingHongText.Contains(TEXT("藏式：下回合第一张主动牌消耗；未消耗则回合末失效。")));
		TestTrue(TEXT("Sheathed opener explains its blade-draw bonus"),
			JingHongText.Contains(TEXT("开锋：本牌消耗藏式时，追加1段90%攻击。")));
		TestTrue(TEXT("Sheathed residual explains its one-shot continuation"),
			HengYunText.Contains(TEXT("开锋：本牌消耗藏式时，将该藏式复制为「余式」，继续作用于本回合下一张主动牌；余式不再产生余式。")));
	}

	FGameXXKRuntimeState EnemyTextState;
	FGameXXKCardEnemyIntent EnemyTextIntent;
	EnemyTextIntent.SourceUnitId = TEXT("Enemy.StatusText");
	EnemyTextIntent.CardDisplayName = TEXT("状态文案测试");
	const TArray<TPair<EGameXXKCardStatus, FString>> EnemyStatusNames = {
		{EGameXXKCardStatus::Momentum, TEXT("气势")},
		{EGameXXKCardStatus::Agility, TEXT("灵动")},
		{EGameXXKCardStatus::Vulnerability, TEXT("破绽")},
		{EGameXXKCardStatus::DamageOverTime, TEXT("蚀伤")},
		{EGameXXKCardStatus::CannotReceiveVulnerability, TEXT("破绽免疫")},
		{EGameXXKCardStatus::Medicine, TEXT("药效")},
		{EGameXXKCardStatus::Counter, TEXT("反击")},
		{EGameXXKCardStatus::Block, TEXT("格挡")}};
	for (const TPair<EGameXXKCardStatus, FString>& Pair : EnemyStatusNames)
	{
		EnemyTextIntent.OnHitStatuses = {FGameXXKCardStatusStack{Pair.Key, 2}};
		const FString IntentText = FGameXXKEnemyText::FormatIntentCard(EnemyTextState, EnemyTextIntent);
		TestTrue(*FString::Printf(TEXT("enemy intent uses approved status name %s"), *Pair.Value), IntentText.Contains(Pair.Value));
		TestFalse(*FString::Printf(TEXT("enemy intent has no unresolved status for %s"), *Pair.Value), IntentText.Contains(TEXT("未知状态")));
		TestFalse(*FString::Printf(TEXT("enemy intent never exposes the retired medicine name for %s"), *Pair.Value), IntentText.Contains(TEXT("药材")));
	}

	FGameXXKCardTooltipContext TooltipContext;
	TooltipContext.InteractionResult = TEXT("点击后加入手牌。");
	TooltipContext.UnavailableReason = TEXT("手牌已满。");
	const FString TooltipText = GameXXKCardText::DescribeTooltip(*QingFeng, nullptr, TooltipContext);
	TestTrue(TEXT("card tooltips retain the formatted card effect"), TooltipText.Contains(TEXT("攻击伤害")));
	TestTrue(TEXT("card tooltips append the actual interaction result"), TooltipText.Contains(TEXT("点击后加入手牌。")));
	TestTrue(TEXT("card tooltips append the actual unavailable reason"), TooltipText.Contains(TEXT("手牌已满。")));

	FGameXXKCardDefinition FormatterCoverage;
	FormatterCoverage.Id = TEXT("Test.CardText.CurrentEnums");
	FormatterCoverage.DisplayName = FText::FromString(TEXT("现行枚举文案覆盖"));
	FormatterCoverage.Owner = EGameXXKCardOwner::Route;
	FormatterCoverage.Rarity = EGameXXKCardRarity::Common;
	FormatterCoverage.BaseQuality = EGameXXKCardQuality::Common;
	FormatterCoverage.TargetSpec.Mode = EGameXXKCardTargetMode::AnyLivingUnit;
	const auto AddCoverageEffect = [&FormatterCoverage](
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude = 1)
	{
		FGameXXKCardEffect& Effect = FormatterCoverage.Effects.AddDefaulted_GetRef();
		Effect.Type = Type;
		Effect.Target = Target;
		Effect.Source = EGameXXKCardEffectSource::CardOwner;
		Effect.Magnitude = Magnitude;
		return &Effect;
	};
	AddCoverageEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestAttackAlly, 6);
	AddCoverageEffect(EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::PriorityEnemy, 100);
	AddCoverageEffect(EGameXXKCardEffectType::HealOrReverseFlat, EGameXXKCardEffectTarget::SelectedTargetSide, 2);
	FGameXXKCardEffect* HighestAttackSource = AddCoverageEffect(
		EGameXXKCardEffectType::DamagePercentAttackPlusArmor,
		EGameXXKCardEffectTarget::SelectedTarget,
		100);
	HighestAttackSource->Source = EGameXXKCardEffectSource::HighestAttackAlly;
	AddCoverageEffect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
	AddCoverageEffect(EGameXXKCardEffectType::ModifyManaCost, EGameXXKCardEffectTarget::PlayedCard, -1);
	AddCoverageEffect(EGameXXKCardEffectType::WidenNextActiveSingleTarget, EGameXXKCardEffectTarget::PlayedCard, 1);
	AddCoverageEffect(EGameXXKCardEffectType::PreserveNextReactionUse, EGameXXKCardEffectTarget::AllAllies, 1);
	AddCoverageEffect(EGameXXKCardEffectType::RetainArmorNextRound, EGameXXKCardEffectTarget::AllAllies, 1);
	AddCoverageEffect(EGameXXKCardEffectType::CleanseFriendlyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1);
	FGameXXKCardEffect* AllyCondition = AddCoverageEffect(
		EGameXXKCardEffectType::AddArmor,
		EGameXXKCardEffectTarget::SelectedTarget,
		4);
	AllyCondition->Condition.Type = EGameXXKCardEffectConditionType::TargetIsAlly;
	FGameXXKCardEffect* EnemyCondition = AddCoverageEffect(
		EGameXXKCardEffectType::ApplyStatus,
		EGameXXKCardEffectTarget::SelectedTarget,
		2);
	EnemyCondition->Status = EGameXXKCardStatus::Poison;
	EnemyCondition->Condition.Type = EGameXXKCardEffectConditionType::TargetIsEnemy;

	const FString FormatterCoverageText = GameXXKCardText::DescribeEffects(FormatterCoverage);
	TestTrue(TEXT("current formatter names the highest-Attack ally target and source"), FormatterCoverageText.Contains(TEXT("攻击最高友方")));
	TestTrue(TEXT("current formatter explains deterministic priority-enemy selection"), FormatterCoverageText.Contains(TEXT("标记最高")) && FormatterCoverageText.Contains(TEXT("生命比例最低")));
	TestTrue(TEXT("current formatter names the selected unit's whole side"), FormatterCoverageText.Contains(TEXT("所选目标同阵营全体")));
	TestTrue(TEXT("current formatter explains named task-NPC search"), FormatterCoverageText.Contains(TEXT("任务 NPC")) && FormatterCoverageText.Contains(TEXT("检索")));
	TestTrue(TEXT("current formatter explains Mana-cost changes"), FormatterCoverageText.Contains(TEXT("内力消耗-1")));
	TestTrue(TEXT("current formatter explains single-target widening"), FormatterCoverageText.Contains(TEXT("扩展为目标所在阵营全体")));
	TestTrue(TEXT("current formatter explains preserved reaction use"), FormatterCoverageText.Contains(TEXT("反击或格挡")) && FormatterCoverageText.Contains(TEXT("不消耗次数")));
	TestTrue(TEXT("current formatter explains next-round Armor retention"), FormatterCoverageText.Contains(TEXT("下回合保留当前护甲")));
	TestTrue(TEXT("current formatter explains ally-only damage-over-time cleansing"), FormatterCoverageText.Contains(TEXT("若所选目标为友方")) && FormatterCoverageText.Contains(TEXT("清除其全部流血、中毒和灼烧")));
	TestTrue(TEXT("current formatter explains flat healing reversal"), FormatterCoverageText.Contains(TEXT("恢复2点生命")) && FormatterCoverageText.Contains(TEXT("失去2点生命")));
	TestTrue(TEXT("current formatter explains ally and enemy gates"), FormatterCoverageText.Contains(TEXT("当所选目标是友方")) && FormatterCoverageText.Contains(TEXT("当所选目标是敌方")));
	TestFalse(TEXT("current enum coverage has no unresolved fallback"), FormatterCoverageText.Contains(TEXT("未知")) || FormatterCoverageText.Contains(TEXT("无效")));

	FGameXXKCardDefinition QualityFixture;
	QualityFixture.Id = TEXT("Test.CardText.Quality");
	QualityFixture.DisplayName = FText::FromString(TEXT("品质文案测试牌"));
	QualityFixture.Owner = EGameXXKCardOwner::Route;
	QualityFixture.Rarity = EGameXXKCardRarity::Boss;
	QualityFixture.BaseQuality = EGameXXKCardQuality::Rare;
	QualityFixture.EnergyCost = 1;
	QualityFixture.ManaCost = 2;
	QualityFixture.TargetSpec.Mode = EGameXXKCardTargetMode::SingleEnemy;
	FGameXXKCardEffect QualityEffect;
	QualityEffect.Type = EGameXXKCardEffectType::DamagePercentAttack;
	QualityEffect.Target = EGameXXKCardEffectTarget::SelectedTarget;
	QualityEffect.Magnitude = 7;
	QualityEffect.HitCount = 2;
	QualityEffect.Condition.Type = EGameXXKCardEffectConditionType::TargetHealthBelowPercent;
	QualityEffect.Condition.HealthPercentThreshold = 50.0f;
	QualityFixture.Effects.Add(QualityEffect);

	const FString EpicEffectsText = GameXXKCardText::DescribeEffects(QualityFixture, EGameXXKCardQuality::Epic);
	const FString EpicDetailText = GameXXKCardText::DescribeDetail(QualityFixture, EGameXXKCardQuality::Epic, nullptr);
	const FString EpicTooltipText = GameXXKCardText::DescribeTooltip(
		QualityFixture,
		EGameXXKCardQuality::Epic,
		nullptr,
		TooltipContext);
	TestTrue(TEXT("explicit-quality effects show the effective Epic value"), EpicEffectsText.Contains(TEXT("28%攻击伤害")));
	TestTrue(TEXT("explicit-quality detail shows the same effective Epic value"), EpicDetailText.Contains(TEXT("28%攻击伤害")));
	TestTrue(TEXT("explicit-quality tooltip shows the same effective Epic value"), EpicTooltipText.Contains(TEXT("28%攻击伤害")));
	TestFalse(TEXT("detail does not scale an already-effective value twice"), EpicDetailText.Contains(TEXT("112%攻击伤害")));
	TestFalse(TEXT("tooltip does not scale an already-effective value twice"), EpicTooltipText.Contains(TEXT("112%攻击伤害")));
	TestTrue(TEXT("detail has an independent Epic quality line"), EpicDetailText.Contains(TEXT("品质：珍稀")));
	TestTrue(TEXT("tooltip has an independent Epic quality line"), EpicTooltipText.Contains(TEXT("品质：珍稀")));
	TestTrue(TEXT("legacy rarity remains a separately-labelled acquisition-source semantic"), EpicDetailText.Contains(TEXT("来源：路线临时卡 · 首领")));
	TestTrue(TEXT("condition percentage remains unchanged in quality-aware detail"), EpicDetailText.Contains(TEXT("50%")));
	TestTrue(TEXT("target interaction language remains unchanged in quality-aware detail"), EpicDetailText.Contains(TEXT("单体敌方")) && EpicDetailText.Contains(TEXT("选择目标")));
	TestTrue(TEXT("quality-aware tooltip retains interaction result"), EpicTooltipText.Contains(TEXT("点击后加入手牌。")));
	TestTrue(TEXT("quality-aware tooltip retains unavailable reason"), EpicTooltipText.Contains(TEXT("手牌已满。")));

	const FString DefaultQualityText = GameXXKCardText::DescribeDetail(QualityFixture, nullptr);
	TestTrue(TEXT("legacy detail signature defaults to definition BaseQuality"),
		DefaultQualityText.Contains(TEXT("14%攻击伤害")) && DefaultQualityText.Contains(TEXT("品质：稀有")));
	const FString InvalidRequestedQualityText = GameXXKCardText::DescribeDetail(QualityFixture, EGameXXKCardQuality::Invalid, nullptr);
	TestTrue(TEXT("explicit Invalid quality safely falls back to definition BaseQuality"),
		InvalidRequestedQualityText.Contains(TEXT("14%攻击伤害")) && InvalidRequestedQualityText.Contains(TEXT("品质：稀有")));
	QualityFixture.BaseQuality = EGameXXKCardQuality::Invalid;
	const FString DoubleInvalidQualityText = GameXXKCardText::DescribeDetail(QualityFixture, nullptr);
	TestTrue(TEXT("legacy detail safely falls back to Common when BaseQuality is Invalid"),
		DoubleInvalidQualityText.Contains(TEXT("7%攻击伤害")) && DoubleInvalidQualityText.Contains(TEXT("品质：普通")));
	return true;
}

#endif
