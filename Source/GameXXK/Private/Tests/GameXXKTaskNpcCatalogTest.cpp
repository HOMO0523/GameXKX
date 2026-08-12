#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcCatalogTest
{
	struct FExpectedCard
	{
		const TCHAR* Id;
		const TCHAR* DisplayName;
		const TCHAR* OwnerId;
		int32 EnergyCost;
		int32 ManaCost;
		EGameXXKCardTargetMode TargetMode;
	};

	constexpr FExpectedCard ExpectedCards[] = {
		{TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("寨主号令"), TEXT("Npc.TusiChief"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.TusiChief.ShiMenShouShi"), TEXT("石门守势"), TEXT("Npc.TusiChief"), 1, 0, EGameXXKCardTargetMode::SingleAlly},
		{TEXT("Npc.TusiChief.TuSiJunLing"), TEXT("土司军令"), TEXT("Npc.TusiChief"), 1, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.TusiChief.MengZhaiShiYue"), TEXT("盟寨誓约"), TEXT("Npc.TusiChief"), 2, 6, EGameXXKCardTargetMode::SingleEnemy},

		{TEXT("Npc.JinGui.ShiJingErMu"), TEXT("市井耳目"), TEXT("Npc.JinGui"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.JinGui.ZaYiChouBei"), TEXT("杂役筹备"), TEXT("Npc.JinGui"), 1, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.JinGui.QiaoYanZhouXuan"), TEXT("巧言周旋"), TEXT("Npc.JinGui"), 1, 0, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.JinGui.HouXiangTuoShen"), TEXT("后巷脱身"), TEXT("Npc.JinGui"), 2, 6, EGameXXKCardTargetMode::None},

		{TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), TEXT("藤桥飞渡"), TEXT("Npc.QiongMeiEr"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.QiongMeiEr.GuWuMiZong"), TEXT("蛊雾迷踪"), TEXT("Npc.QiongMeiEr"), 1, 0, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.QiongMeiEr.YinLingZhenXin"), TEXT("银铃镇心"), TEXT("Npc.QiongMeiEr"), 1, 3, EGameXXKCardTargetMode::SingleAlly},
		{TEXT("Npc.QiongMeiEr.ShanGeHuanLing"), TEXT("山歌唤灵"), TEXT("Npc.QiongMeiEr"), 2, 6, EGameXXKCardTargetMode::AllAllies},

		{TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), TEXT("异草辨识"), TEXT("Npc.ZhouGuangZu"), 0, 0, EGameXXKCardTargetMode::AnyLivingUnit},
		{TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), TEXT("黄山敷治"), TEXT("Npc.ZhouGuangZu"), 1, 3, EGameXXKCardTargetMode::AllAllies},
		{TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), TEXT("地志摹图"), TEXT("Npc.ZhouGuangZu"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.ZhouGuangZu.YanFenFengMai"), TEXT("岩粉封脉"), TEXT("Npc.ZhouGuangZu"), 1, 3, EGameXXKCardTargetMode::SingleEnemy},

		{TEXT("Npc.YueBai.QingYanDianDeng"), TEXT("青焰点灯"), TEXT("Npc.YueBai"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.YueBai.YueBaiZhaoYe"), TEXT("月白照夜"), TEXT("Npc.YueBai"), 1, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.YueBai.CanJuanPiZhu"), TEXT("残卷批注"), TEXT("Npc.YueBai"), 0, 0, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.YueBai.ShanHeCanTu"), TEXT("山河残图"), TEXT("Npc.YueBai"), 0, 6, EGameXXKCardTargetMode::SingleEnemy},

		{TEXT("Npc.SongJinBao.ShangQianGuWu"), TEXT("赏钱鼓舞"), TEXT("Npc.SongJinBao"), 0, 0, EGameXXKCardTargetMode::SingleAlly},
		{TEXT("Npc.SongJinBao.ErMuMiBao"), TEXT("耳目密报"), TEXT("Npc.SongJinBao"), 0, 3, EGameXXKCardTargetMode::SingleEnemy},
		{TEXT("Npc.SongJinBao.GuiKeLing"), TEXT("贵客令"), TEXT("Npc.SongJinBao"), 1, 0, EGameXXKCardTargetMode::Self},
		{TEXT("Npc.SongJinBao.YiNuoQianJin"), TEXT("一诺千金"), TEXT("Npc.SongJinBao"), 1, 6, EGameXXKCardTargetMode::None},
	};

	bool HasEffect(
		const TArray<FGameXXKCardEffect>& Effects,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const EGameXXKCardEffectSource Source = EGameXXKCardEffectSource::CardOwner,
		const int32 SecondaryMagnitude = 0)
	{
		return Effects.ContainsByPredicate([=](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == Type
				&& Effect.Target == Target
				&& Effect.Magnitude == Magnitude
				&& Effect.Status == Status
				&& Effect.Source == Source
				&& Effect.SecondaryMagnitude == SecondaryMagnitude;
		});
	}

	void ExpectEffect(
		FAutomationTestBase& Test,
		const FName CardId,
		const TArray<FGameXXKCardEffect>& Effects,
		const TCHAR* Label,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const EGameXXKCardEffectSource Source = EGameXXKCardEffectSource::CardOwner,
		const int32 SecondaryMagnitude = 0)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s contains %s"), *CardId.ToString(), Label),
			HasEffect(Effects, Type, Target, Magnitude, Status, Source, SecondaryMagnitude));
	}

	void ExpectModifierEffect(
		FAutomationTestBase& Test,
		const FName CardId,
		const TArray<FGameXXKCardEffect>& Effects,
		const TCHAR* Label,
		const EGameXXKCardBattleModifierTrigger Trigger,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const int32 RemainingTriggers,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const EGameXXKCardModifierExpiry Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s contains timed %s"), *CardId.ToString(), Label),
			Effects.ContainsByPredicate([=](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
					&& Effect.Modifier.Trigger == Trigger
					&& Effect.Modifier.EffectType == Type
					&& Effect.Modifier.Target == Target
					&& Effect.Modifier.Magnitude == Magnitude
					&& Effect.Modifier.RemainingTriggers == RemainingTriggers
					&& Effect.Modifier.Status == Status
					&& Effect.Modifier.Expiry == Expiry;
			}));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcCatalogTest,
	"GameXXK.Data.TaskNpcCards.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcCatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcCatalogTest;
	int32 ActualTaskNpcCardCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		ActualTaskNpcCardCount += Definition.Owner == EGameXXKCardOwner::QuestNpc ? 1 : 0;
	}
	TestEqual(TEXT("six task NPCs own exactly twenty-four candidate cards"), ActualTaskNpcCardCount, 24);
	TestEqual(TEXT("the locked task-NPC table contains exactly twenty-four rows"), static_cast<int32>(UE_ARRAY_COUNT(ExpectedCards)), 24);

	TSet<FName> UniqueIds;
	for (const FExpectedCard& Expected : ExpectedCards)
	{
		const FName CardId(Expected.Id);
		TestFalse(FString::Printf(TEXT("task-NPC CardId is unique (%s)"), Expected.Id), UniqueIds.Contains(CardId));
		UniqueIds.Add(CardId);
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		TestNotNull(FString::Printf(TEXT("task-NPC card exists (%s)"), Expected.Id), Definition);
		if (!Definition)
		{
			continue;
		}
		const FString Prefix = FString::Printf(TEXT("task-NPC card %s"), Expected.Id);
		TestEqual(*FString::Printf(TEXT("%s keeps its approved name"), *Prefix), Definition->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(*FString::Printf(TEXT("%s remains a task-NPC card"), *Prefix), Definition->Owner, EGameXXKCardOwner::QuestNpc);
		TestEqual(*FString::Printf(TEXT("%s keeps the quest-NPC role"), *Prefix), Definition->Role, EGameXXKCharacterRole::QuestNpc);
		TestEqual(*FString::Printf(TEXT("%s keeps its named owner"), *Prefix), Definition->OwnerId, FName(Expected.OwnerId));
		TestEqual(*FString::Printf(TEXT("%s keeps its named NPC identity"), *Prefix), Definition->NpcId, FName(Expected.OwnerId));
		TestEqual(*FString::Printf(TEXT("%s uses the approved Energy cost"), *Prefix), Definition->EnergyCost, Expected.EnergyCost);
		TestEqual(*FString::Printf(TEXT("%s uses the approved Mana cost"), *Prefix), Definition->ManaCost, Expected.ManaCost);
		TestEqual(*FString::Printf(TEXT("%s uses one fixed target mode"), *Prefix), Definition->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(*FString::Printf(TEXT("%s has no terrain/status target override"), *Prefix), Definition->TargetSpec.ModeOverrides.Num(), 0);
		FString ValidationError;
		const bool bValidDefinition = FGameXXKCardCatalog::ValidateCardDefinition(*Definition, ValidationError);
		TestTrue(
			*FString::Printf(TEXT("%s satisfies the production catalog contract: %s"), *Prefix, *ValidationError),
			bValidDefinition);
	}

	const auto Card = [this](const TCHAR* CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FName(CardId));
		TestNotNull(FString::Printf(TEXT("exact task-NPC payload card exists (%s)"), CardId), Definition);
		return Definition;
	};

	const FGameXXKCardDefinition* ZhaiZhu = Card(TEXT("Npc.TusiChief.ZhaiZhuHaoLing"));
	if (ZhaiZhu)
	{
		TestEqual(TEXT("寨主号令 has four base clauses"), ZhaiZhu->Effects.Num(), 4);
		ExpectEffect(*this, ZhaiZhu->Id, ZhaiZhu->Effects, TEXT("协战者气势1"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 1, EGameXXKCardStatus::Momentum);
		ExpectEffect(*this, ZhaiZhu->Id, ZhaiZhu->Effects, TEXT("协战者护甲8"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestAttackAlly, 8);
		ExpectEffect(*this, ZhaiZhu->Id, ZhaiZhu->Effects, TEXT("协战者格挡1"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::HighestAttackAlly, 1, EGameXXKCardStatus::Block);
		ExpectEffect(*this, ZhaiZhu->Id, ZhaiZhu->Effects, TEXT("协战100%"), EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 100, EGameXXKCardStatus::None, EGameXXKCardEffectSource::HighestAttackAlly);
		TestEqual(TEXT("寨主号令 has one Charge payload"), ZhaiZhu->ChargeEffects.Num(), 1);
		ExpectModifierEffect(*this, ZhaiZhu->Id, ZhaiZhu->ChargeEffects, TEXT("冲锋重放下一主动牌基础"), EGameXXKCardBattleModifierTrigger::AfterNextActiveCard, EGameXXKCardEffectType::ReplayTriggeredCardBase, EGameXXKCardEffectTarget::PlayedCard, 1, 1);
		TestEqual(TEXT("寨主号令 has one Finish payload"), ZhaiZhu->FinishEffects.Num(), 1);
		ExpectEffect(*this, ZhaiZhu->Id, ZhaiZhu->FinishEffects, TEXT("收招全队护甲6"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6);
	}

	const FGameXXKCardDefinition* ShiMen = Card(TEXT("Npc.TusiChief.ShiMenShouShi"));
	if (ShiMen)
	{
		TestEqual(TEXT("石门守势 has four base clauses"), ShiMen->Effects.Num(), 4);
		ExpectEffect(*this, ShiMen->Id, ShiMen->Effects, TEXT("目标标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, ShiMen->Id, ShiMen->Effects, TEXT("目标灵动2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility);
		ExpectEffect(*this, ShiMen->Id, ShiMen->Effects, TEXT("目标护甲16"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16);
		ExpectEffect(*this, ShiMen->Id, ShiMen->Effects, TEXT("目标格挡2"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Block);
		TestEqual(TEXT("石门守势 has two Charge payload clauses"), ShiMen->ChargeEffects.Num(), 2);
		ExpectModifierEffect(*this, ShiMen->Id, ShiMen->ChargeEffects, TEXT("下一使用者护甲12"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::PlayedCard, 12, 1);
		ExpectModifierEffect(*this, ShiMen->Id, ShiMen->ChargeEffects, TEXT("下一使用者格挡1"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::PlayedCard, 1, 1, EGameXXKCardStatus::Block);
		TestEqual(TEXT("石门守势 has one Finish payload"), ShiMen->FinishEffects.Num(), 1);
		ExpectEffect(*this, ShiMen->Id, ShiMen->FinishEffects, TEXT("收招重定向下一敌方单攻"), EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks, EGameXXKCardEffectTarget::SelectedTarget, 1);
	}

	const FGameXXKCardDefinition* TuSi = Card(TEXT("Npc.TusiChief.TuSiJunLing"));
	if (TuSi)
	{
		TestEqual(TEXT("土司军令 has five base clauses"), TuSi->Effects.Num(), 5);
		ExpectEffect(*this, TuSi->Id, TuSi->Effects, TEXT("目标标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, TuSi->Id, TuSi->Effects, TEXT("目标破绽3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability);
		ExpectEffect(*this, TuSi->Id, TuSi->Effects, TEXT("协战者护甲8"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestAttackAlly, 8);
		ExpectEffect(*this, TuSi->Id, TuSi->Effects, TEXT("协战者格挡1"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::HighestAttackAlly, 1, EGameXXKCardStatus::Block);
		ExpectEffect(*this, TuSi->Id, TuSi->Effects, TEXT("协战150%"), EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 150, EGameXXKCardStatus::None, EGameXXKCardEffectSource::HighestAttackAlly);
		ExpectModifierEffect(*this, TuSi->Id, TuSi->ChargeEffects, TEXT("冲锋改单体为同阵营群体"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::WidenNextActiveSingleTarget, EGameXXKCardEffectTarget::PlayedCard, 1, 1);
		ExpectEffect(*this, TuSi->Id, TuSi->FinishEffects, TEXT("收招保护下一次格挡"), EGameXXKCardEffectType::PreserveNextReactionUse, EGameXXKCardEffectTarget::AllAllies, 1);
	}

	const FGameXXKCardDefinition* MengZhai = Card(TEXT("Npc.TusiChief.MengZhaiShiYue"));
	if (MengZhai)
	{
		TestEqual(TEXT("盟寨誓约 has four base clauses"), MengZhai->Effects.Num(), 4);
		ExpectEffect(*this, MengZhai->Id, MengZhai->Effects, TEXT("全队气势1"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Momentum);
		ExpectEffect(*this, MengZhai->Id, MengZhai->Effects, TEXT("全队护甲8"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8);
		ExpectEffect(*this, MengZhai->Id, MengZhai->Effects, TEXT("全队格挡1"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Block);
		ExpectEffect(*this, MengZhai->Id, MengZhai->Effects, TEXT("全队各攻击60%"), EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 60);
		ExpectModifierEffect(*this, MengZhai->Id, MengZhai->ChargeEffects, TEXT("下一牌气力免费"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ModifyEnergyCost, EGameXXKCardEffectTarget::PlayedCard, -99, 1);
		ExpectModifierEffect(*this, MengZhai->Id, MengZhai->ChargeEffects, TEXT("下一牌内力免费"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ModifyManaCost, EGameXXKCardEffectTarget::PlayedCard, -99, 1);
		ExpectEffect(*this, MengZhai->Id, MengZhai->FinishEffects, TEXT("下回合护甲不衰减"), EGameXXKCardEffectType::RetainArmorNextRound, EGameXXKCardEffectTarget::AllAllies, 1);
	}

	const FGameXXKCardDefinition* ShiJing = Card(TEXT("Npc.JinGui.ShiJingErMu"));
	if (ShiJing)
	{
		TestEqual(TEXT("市井耳目 has three base clauses"), ShiJing->Effects.Num(), 3);
		ExpectEffect(*this, ShiJing->Id, ShiJing->Effects, TEXT("敌群标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, ShiJing->Id, ShiJing->Effects, TEXT("抽2"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2);
		ExpectEffect(*this, ShiJing->Id, ShiJing->Effects, TEXT("协战者蓄力2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Charge);
		TestEqual(TEXT("市井耳目 Heavy Arrow attacks at 50% per Charge"), ShiJing->HeavyArrow.MagnitudePerCharge, 50);
		TestEqual(TEXT("市井耳目 Heavy Arrow consumes the highest-Attack ally Charge"), ShiJing->HeavyArrow.ChargeSource, EGameXXKHeavyArrowChargeSource::HighestAttackAlly);
	}

	const FGameXXKCardDefinition* ZaYi = Card(TEXT("Npc.JinGui.ZaYiChouBei"));
	if (ZaYi)
	{
		TestEqual(TEXT("杂役筹备 has four base clauses"), ZaYi->Effects.Num(), 4);
		ExpectEffect(*this, ZaYi->Id, ZaYi->Effects, TEXT("抽3"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3);
		ExpectEffect(*this, ZaYi->Id, ZaYi->Effects, TEXT("弃1"), EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, ZaYi->Id, ZaYi->Effects, TEXT("回1气"), EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, ZaYi->Id, ZaYi->Effects, TEXT("协战者蓄力3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 3, EGameXXKCardStatus::Charge);
		TestEqual(TEXT("杂役筹备 Heavy Arrow attacks at 40% per Charge"), ZaYi->HeavyArrow.MagnitudePerCharge, 40);
		TestEqual(TEXT("杂役筹备 Heavy Arrow restores one Mana per Charge"), ZaYi->HeavyArrow.ManaPerCharge, 1);
		TestEqual(TEXT("杂役筹备 Heavy Arrow consumes the highest-Attack ally Charge"), ZaYi->HeavyArrow.ChargeSource, EGameXXKHeavyArrowChargeSource::HighestAttackAlly);
	}

	const FGameXXKCardDefinition* QiaoYan = Card(TEXT("Npc.JinGui.QiaoYanZhouXuan"));
	if (QiaoYan)
	{
		TestEqual(TEXT("巧言周旋 has three base clauses"), QiaoYan->Effects.Num(), 3);
		ExpectEffect(*this, QiaoYan->Id, QiaoYan->Effects, TEXT("目标破绽3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability);
		ExpectEffect(*this, QiaoYan->Id, QiaoYan->Effects, TEXT("最高护甲友方护甲12"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestArmorAlly, 12);
		ExpectEffect(*this, QiaoYan->Id, QiaoYan->Effects, TEXT("最高护甲友方格挡2"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::HighestArmorAlly, 2, EGameXXKCardStatus::Block);
	}

	const FGameXXKCardDefinition* HouXiang = Card(TEXT("Npc.JinGui.HouXiangTuoShen"));
	if (HouXiang)
	{
		TestEqual(TEXT("后巷脱身 has four base clauses"), HouXiang->Effects.Num(), 4);
		ExpectEffect(*this, HouXiang->Id, HouXiang->Effects, TEXT("全队灵动2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 2, EGameXXKCardStatus::Agility);
		ExpectEffect(*this, HouXiang->Id, HouXiang->Effects, TEXT("最低血比友方标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::LowestHealthAlly, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, HouXiang->Id, HouXiang->Effects, TEXT("最低血比友方护甲16"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 16);
		ExpectEffect(*this, HouXiang->Id, HouXiang->Effects, TEXT("最低血比友方格挡2"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::LowestHealthAlly, 2, EGameXXKCardStatus::Block);
	}

	const FGameXXKCardDefinition* TengQiao = Card(TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"));
	if (TengQiao)
	{
		TestEqual(TEXT("藤桥飞渡 has three base clauses"), TengQiao->Effects.Num(), 3);
		ExpectEffect(*this, TengQiao->Id, TengQiao->Effects, TEXT("协战者灵动2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Agility);
		ExpectEffect(*this, TengQiao->Id, TengQiao->Effects, TEXT("协战者蓄力2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Charge);
		ExpectEffect(*this, TengQiao->Id, TengQiao->Effects, TEXT("抽1"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1);
		TestEqual(TEXT("藤桥飞渡 Heavy Arrow attacks at 50% per Charge"), TengQiao->HeavyArrow.MagnitudePerCharge, 50);
		TestEqual(TEXT("藤桥飞渡 Heavy Arrow consumes the highest-Attack ally Charge"), TengQiao->HeavyArrow.ChargeSource, EGameXXKHeavyArrowChargeSource::HighestAttackAlly);
	}

	const FGameXXKCardDefinition* GuWu = Card(TEXT("Npc.QiongMeiEr.GuWuMiZong"));
	if (GuWu)
	{
		TestEqual(TEXT("蛊雾迷踪 has exactly Bleed, Poison, and Toxic Explosion"), GuWu->Effects.Num(), 3);
		ExpectEffect(*this, GuWu->Id, GuWu->Effects, TEXT("流血4"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Bleed);
		ExpectEffect(*this, GuWu->Id, GuWu->Effects, TEXT("中毒6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison);
		ExpectEffect(*this, GuWu->Id, GuWu->Effects, TEXT("毒爆1"), EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1);
	}

	const FGameXXKCardDefinition* YinLing = Card(TEXT("Npc.QiongMeiEr.YinLingZhenXin"));
	if (YinLing)
	{
		TestEqual(TEXT("银铃镇心 has Medicine, ally cleanse, and heal"), YinLing->Effects.Num(), 3);
		ExpectEffect(*this, YinLing->Id, YinLing->Effects, TEXT("药效6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine);
		ExpectEffect(*this, YinLing->Id, YinLing->Effects, TEXT("友方三DoT全清"), EGameXXKCardEffectType::CleanseFriendlyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1);
		ExpectEffect(*this, YinLing->Id, YinLing->Effects, TEXT("单体治疗12加药效"), EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 12);
	}

	const FGameXXKCardDefinition* ShanGe = Card(TEXT("Npc.QiongMeiEr.ShanGeHuanLing"));
	if (ShanGe)
	{
		TestEqual(TEXT("山歌唤灵 has Medicine and group heal"), ShanGe->Effects.Num(), 2);
		ExpectEffect(*this, ShanGe->Id, ShanGe->Effects, TEXT("药效6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine);
		ExpectEffect(*this, ShanGe->Id, ShanGe->Effects, TEXT("群体治疗6加药效"), EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 6);
	}

	const FGameXXKCardDefinition* YiCao = Card(TEXT("Npc.ZhouGuangZu.YiCaoBianShi"));
	if (YiCao)
	{
		TestEqual(TEXT("异草辨识 has Medicine, bidirectional resolution, and ally-only cleanse"), YiCao->Effects.Num(), 3);
		ExpectEffect(*this, YiCao->Id, YiCao->Effects, TEXT("药效6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine);
		ExpectEffect(*this, YiCao->Id, YiCao->Effects, TEXT("友方治疗或敌方扣血6加药效"), EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 6);
		ExpectEffect(*this, YiCao->Id, YiCao->Effects, TEXT("仅友方清三DoT"), EGameXXKCardEffectType::CleanseFriendlyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1);
	}

	const FGameXXKCardDefinition* HuangShan = Card(TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"));
	if (HuangShan)
	{
		TestEqual(TEXT("黄山敷治 has nonlethal loss, Medicine, and group heal"), HuangShan->Effects.Num(), 3);
		ExpectEffect(*this, HuangShan->Id, HuangShan->Effects, TEXT("全队非致死失血1"), EGameXXKCardEffectType::LoseHealthNonlethal, EGameXXKCardEffectTarget::AllAllies, 1);
		ExpectEffect(*this, HuangShan->Id, HuangShan->Effects, TEXT("药效6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine);
		ExpectEffect(*this, HuangShan->Id, HuangShan->Effects, TEXT("群体治疗6加药效"), EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 6);
	}

	const FGameXXKCardDefinition* DiZhi = Card(TEXT("Npc.ZhouGuangZu.DiZhiMoTu"));
	if (DiZhi)
	{
		TestEqual(TEXT("地志摹图 has draw and two terrain benefits"), DiZhi->Effects.Num(), 2);
		ExpectEffect(*this, DiZhi->Id, DiZhi->Effects, TEXT("抽2"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2);
		ExpectEffect(*this, DiZhi->Id, DiZhi->Effects, TEXT("地势收益2次"), EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 2);
	}

	const FGameXXKCardDefinition* YanFen = Card(TEXT("Npc.ZhouGuangZu.YanFenFengMai"));
	if (YanFen)
	{
		TestEqual(TEXT("岩粉封脉 has terrain, Vulnerability, Poison, and Toxic Explosion"), YanFen->Effects.Num(), 4);
		ExpectEffect(*this, YanFen->Id, YanFen->Effects, TEXT("地势收益1次"), EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1);
		ExpectEffect(*this, YanFen->Id, YanFen->Effects, TEXT("破绽3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability);
		ExpectEffect(*this, YanFen->Id, YanFen->Effects, TEXT("中毒11"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 11, EGameXXKCardStatus::Poison);
		ExpectEffect(*this, YanFen->Id, YanFen->Effects, TEXT("毒爆1"), EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1);
	}

	const FGameXXKCardDefinition* QingYan = Card(TEXT("Npc.YueBai.QingYanDianDeng"));
	if (QingYan)
	{
		TestEqual(TEXT("青焰点灯 base has Burn6, one trigger, and task search"), QingYan->Effects.Num(), 3);
		ExpectEffect(*this, QingYan->Id, QingYan->Effects, TEXT("灼烧6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Burn);
		ExpectEffect(*this, QingYan->Id, QingYan->Effects, TEXT("触发灼烧1次"), EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn);
		ExpectEffect(*this, QingYan->Id, QingYan->Effects, TEXT("检索未完成月白牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		TestEqual(TEXT("青焰点灯 reward has group Burn and trigger"), QingYan->TaskNpcRewardEffects.Num(), 2);
		ExpectEffect(*this, QingYan->Id, QingYan->TaskNpcRewardEffects, TEXT("奖励敌群灼烧6"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 6, EGameXXKCardStatus::Burn);
		ExpectEffect(*this, QingYan->Id, QingYan->TaskNpcRewardEffects, TEXT("奖励敌群触发灼烧"), EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn);
	}

	const FGameXXKCardDefinition* YueZhao = Card(TEXT("Npc.YueBai.YueBaiZhaoYe"));
	if (YueZhao)
	{
		TestEqual(TEXT("月白照夜 base has Mark, Burn, attack, trigger, and task search"), YueZhao->Effects.Num(), 5);
		ExpectEffect(*this, YueZhao->Id, YueZhao->Effects, TEXT("标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, YueZhao->Id, YueZhao->Effects, TEXT("灼烧4"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Burn);
		ExpectEffect(*this, YueZhao->Id, YueZhao->Effects, TEXT("攻击100%"), EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 100);
		ExpectEffect(*this, YueZhao->Id, YueZhao->Effects, TEXT("触发灼烧1次"), EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn);
		ExpectEffect(*this, YueZhao->Id, YueZhao->Effects, TEXT("检索未完成月白牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, YueZhao->Id, YueZhao->TaskNpcRewardEffects, TEXT("奖励敌群标记3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, YueZhao->Id, YueZhao->TaskNpcRewardEffects, TEXT("奖励每层标记落雷50%"), EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 50, EGameXXKCardStatus::Mark);
	}

	const FGameXXKCardDefinition* CanJuan = Card(TEXT("Npc.YueBai.CanJuanPiZhu"));
	if (CanJuan)
	{
		TestEqual(TEXT("残卷批注 base has draw2, terrain, and task search"), CanJuan->Effects.Num(), 3);
		ExpectEffect(*this, CanJuan->Id, CanJuan->Effects, TEXT("抽2"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2);
		ExpectEffect(*this, CanJuan->Id, CanJuan->Effects, TEXT("地势收益1次"), EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1);
		ExpectEffect(*this, CanJuan->Id, CanJuan->Effects, TEXT("检索未完成月白牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, CanJuan->Id, CanJuan->TaskNpcRewardEffects, TEXT("奖励地势收益3次"), EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 3);
	}

	const FGameXXKCardDefinition* ShanHe = Card(TEXT("Npc.YueBai.ShanHeCanTu"));
	if (ShanHe)
	{
		TestEqual(TEXT("山河残图 base has group armor, group Mana, terrain, and task search"), ShanHe->Effects.Num(), 4);
		ExpectEffect(*this, ShanHe->Id, ShanHe->Effects, TEXT("全队护甲9"), EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 9);
		ExpectEffect(*this, ShanHe->Id, ShanHe->Effects, TEXT("全队内力3"), EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3);
		ExpectEffect(*this, ShanHe->Id, ShanHe->Effects, TEXT("地势收益1次"), EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1);
		ExpectEffect(*this, ShanHe->Id, ShanHe->Effects, TEXT("检索未完成月白牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, ShanHe->Id, ShanHe->TaskNpcRewardEffects, TEXT("奖励每甲20%敌群"), EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 0, EGameXXKCardStatus::None, EGameXXKCardEffectSource::CardOwner, 20);
	}

	const FGameXXKCardDefinition* ShangQian = Card(TEXT("Npc.SongJinBao.ShangQianGuWu"));
	if (ShangQian)
	{
		TestEqual(TEXT("赏钱鼓舞 base has Momentum, Mana, and selected-ally assist"), ShangQian->Effects.Num(), 3);
		ExpectEffect(*this, ShangQian->Id, ShangQian->Effects, TEXT("友方气势2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Momentum);
		ExpectEffect(*this, ShangQian->Id, ShangQian->Effects, TEXT("友方内力6"), EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6);
		ExpectEffect(*this, ShangQian->Id, ShangQian->Effects, TEXT("所选友方协战100%"), EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::PriorityEnemy, 100, EGameXXKCardStatus::None, EGameXXKCardEffectSource::SelectedTarget);
		ExpectModifierEffect(*this, ShangQian->Id, ShangQian->ChargeEffects, TEXT("冲锋重放下一主动牌基础"), EGameXXKCardBattleModifierTrigger::AfterNextActiveCard, EGameXXKCardEffectType::ReplayTriggeredCardBase, EGameXXKCardEffectTarget::PlayedCard, 1, 1);
		ExpectModifierEffect(*this, ShangQian->Id, ShangQian->FinishEffects, TEXT("收招下回合首牌后重放本牌"), EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound, EGameXXKCardEffectType::ReplaySourceCardBase, EGameXXKCardEffectTarget::CardOwner, 1, 1);
		ExpectEffect(*this, ShangQian->Id, ShangQian->TaskNpcRewardEffects, TEXT("奖励全队气势2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 2, EGameXXKCardStatus::Momentum);
		ExpectEffect(*this, ShangQian->Id, ShangQian->TaskNpcRewardEffects, TEXT("奖励抽2"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2);
		ExpectEffect(*this, ShangQian->Id, ShangQian->TaskNpcRewardEffects, TEXT("奖励回2气"), EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 2);
	}

	const FGameXXKCardDefinition* ErMu = Card(TEXT("Npc.SongJinBao.ErMuMiBao"));
	if (ErMu)
	{
		TestEqual(TEXT("耳目密报 base has reveal, Mark, and task search"), ErMu->Effects.Num(), 3);
		ExpectEffect(*this, ErMu->Id, ErMu->Effects, TEXT("显示全部意图"), EGameXXKCardEffectType::RevealEnemyIntent, EGameXXKCardEffectTarget::CardOwner, 99);
		ExpectEffect(*this, ErMu->Id, ErMu->Effects, TEXT("目标标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, ErMu->Id, ErMu->Effects, TEXT("检索未完成宋金宝牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectEffect(*this, ErMu->Id, ErMu->TaskNpcRewardEffects, TEXT("奖励目标标记3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, ErMu->Id, ErMu->TaskNpcRewardEffects, TEXT("奖励全队各攻击100%"), EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 100);
	}

	const FGameXXKCardDefinition* GuiKe = Card(TEXT("Npc.SongJinBao.GuiKeLing"));
	if (GuiKe)
	{
		TestEqual(TEXT("贵客令 base has Mark, Agility, Counter, and draw"), GuiKe->Effects.Num(), 4);
		ExpectEffect(*this, GuiKe->Id, GuiKe->Effects, TEXT("自身标记2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, GuiKe->Id, GuiKe->Effects, TEXT("自身灵动2"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility);
		ExpectEffect(*this, GuiKe->Id, GuiKe->Effects, TEXT("自身反击1"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Counter);
		ExpectEffect(*this, GuiKe->Id, GuiKe->Effects, TEXT("抽1"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectModifierEffect(*this, GuiKe->Id, GuiKe->ChargeEffects, TEXT("下一使用者灵动2"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::PlayedCard, 2, 1, EGameXXKCardStatus::Agility);
		ExpectModifierEffect(*this, GuiKe->Id, GuiKe->ChargeEffects, TEXT("下一使用者反击1"), EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::PlayedCard, 1, 1, EGameXXKCardStatus::Counter);
		ExpectModifierEffect(*this, GuiKe->Id, GuiKe->FinishEffects, TEXT("下回合自身标记2"), EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, 1, EGameXXKCardStatus::Mark);
		ExpectModifierEffect(*this, GuiKe->Id, GuiKe->FinishEffects, TEXT("下回合自身反击2"), EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::CardOwner, 2, 1, EGameXXKCardStatus::Counter);
		ExpectEffect(*this, GuiKe->Id, GuiKe->TaskNpcRewardEffects, TEXT("奖励自身标记3"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Mark);
		ExpectEffect(*this, GuiKe->Id, GuiKe->TaskNpcRewardEffects, TEXT("奖励自身灵动4"), EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::Agility);
		ExpectEffect(*this, GuiKe->Id, GuiKe->TaskNpcRewardEffects, TEXT("奖励自身反击3"), EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Counter);
	}

	const FGameXXKCardDefinition* YiNuo = Card(TEXT("Npc.SongJinBao.YiNuoQianJin"));
	if (YiNuo)
	{
		ExpectEffect(*this, YiNuo->Id, YiNuo->Effects, TEXT("检索未完成宋金宝牌"), EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1);
		ExpectModifierEffect(*this, YiNuo->Id, YiNuo->Effects, TEXT("后两张气力免费"), EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyEnergyCost, EGameXXKCardEffectTarget::PlayedCard, -99, 2);
		ExpectModifierEffect(*this, YiNuo->Id, YiNuo->Effects, TEXT("后两张内力免费"), EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyManaCost, EGameXXKCardEffectTarget::PlayedCard, -99, 2);
		ExpectEffect(*this, YiNuo->Id, YiNuo->TaskNpcRewardEffects, TEXT("奖励抽3"), EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3);
		ExpectEffect(*this, YiNuo->Id, YiNuo->TaskNpcRewardEffects, TEXT("奖励回2气"), EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 2);
		ExpectModifierEffect(*this, YiNuo->Id, YiNuo->TaskNpcRewardEffects, TEXT("奖励本回合气力免费"), EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyEnergyCost, EGameXXKCardEffectTarget::PlayedCard, -99, 0, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::EndOfCurrentRound);
		ExpectModifierEffect(*this, YiNuo->Id, YiNuo->TaskNpcRewardEffects, TEXT("奖励本回合内力免费"), EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyManaCost, EGameXXKCardEffectTarget::PlayedCard, -99, 0, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::EndOfCurrentRound);
	}
	return true;
}

#endif
