#include "GameXXKCardCatalog.h"
#include "GameXXKCardText.h"

#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerTextTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;

	struct FExpectedCardText
	{
		FName CardId = NAME_None;
		const TCHAR* Base = TEXT("");
		const TCHAR* Sequence = TEXT("");
		const TCHAR* Reward = TEXT("");
	};

	bool TestOrderedFragments(
		FAutomationTestBase& Test,
		const FString& Context,
		const FString& Text,
		const TArray<FString>& Fragments)
	{
		int32 SearchFrom = 0;
		for (const FString& Fragment : Fragments)
		{
			const int32 FoundAt = Text.Find(Fragment, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (!Test.TestTrue(
				FString::Printf(TEXT("%s contains ordered clause '%s'"), *Context, *Fragment),
				FoundAt != INDEX_NONE))
			{
				Test.AddError(FString::Printf(TEXT("Actual text for %s:\n%s"), *Context, *Text));
				return false;
			}
			SearchFrom = FoundAt + Fragment.Len();
		}
		return true;
	}

	TArray<FName> ExpectedAutomaticTargets(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::AllEnemies:
			return {EnemyAId, EnemyBId};
		case EGameXXKCardTargetMode::Self:
			return {SorcererId};
		case EGameXXKCardTargetMode::AllAllies:
			return {SorcererId, AllyId};
		default:
			return {};
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerAllCardTextTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Text.All18DescribeFinalRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerAllCardTextTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTextTest;
	const TArray<FExpectedCardText> Expected = {
		{TEXT("Profession.Sorcerer.LingHuoFu"), TEXT("基础：敌方全体造成70%攻击伤害"), TEXT("编序：第1～2位时，检索牌本回合内力消耗-3"), TEXT("首牌奖励：回复1点气力、8点内力，抽2张牌")},
		{TEXT("Profession.Sorcerer.JuLing"), TEXT("基础：自身回复3点内力"), TEXT("编序：再回复此前记录牌实际支付内力总和的50%"), TEXT("首牌奖励：我方全体回复8点内力，抽2张牌")},
		{TEXT("Profession.Sorcerer.LiHuoYin"), TEXT("基础：敌方全体造成60%攻击伤害并获得2层灼烧"), TEXT("编序：第1～2位时，灼烧改为4层"), TEXT("首牌奖励：敌方全体当前灼烧翻倍")},
		{TEXT("Profession.Sorcerer.YanQiang"), TEXT("基础：敌方全体获得1层灼烧"), TEXT("编序：前一张记录牌为炎牌时，灼烧改为3层"), TEXT("首牌奖励：按场上最高灼烧补齐敌方全体，再各获得3层灼烧")},
		{TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("基础：敌方全体造成80%攻击伤害"), TEXT("编序：第3～5位时，每层灼烧使倍率+10个百分点，不消耗灼烧"), TEXT("首牌奖励：敌方全体结算2次当前灼烧伤害，均不减层")},
		{TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), TEXT("基础：敌方全体造成40%攻击伤害"), TEXT("编序：第4～5位时，每段改为70%攻击伤害"), TEXT("首牌奖励：敌方全体获得6层灼烧；回复1点气力，抽2张牌")},
		{TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("基础：自身回复当前内力25%的内力，向下取整；溢出内力100%转为护甲"), TEXT(""), TEXT("首牌奖励：执行标准寒冰伤害；回复1点气力，抽1张牌")},
		{TEXT("Profession.Sorcerer.FenMaiFu"), TEXT("基础：自身内力上限+4并获得4点护甲，当前内力不变"), TEXT(""), TEXT("首牌奖励：执行标准寒冰伤害；自身内力上限再+8并补满内力")},
		{TEXT("Profession.Sorcerer.LingYanLianDan"), TEXT("基础：自身护甲为0时获得4点护甲，否则当前护甲翻倍，最高99"), TEXT(""), TEXT("首牌奖励：执行标准寒冰伤害；我方全体获得6点护甲")},
		{TEXT("Profession.Sorcerer.HuLingMu"), TEXT("基础：自身获得当前内力25%的护甲"), TEXT(""), TEXT("首牌奖励：执行标准寒冰伤害；敌方全体获得2层虚弱")},
		{TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("基础：敌方全体造成50%攻击伤害，伤害后获得2层标记"), TEXT("编序：第1～2位时，标记改为3层"), TEXT("首牌奖励：敌方全体获得5层标记；回复1点气力，抽2张牌")},
		{TEXT("Profession.Sorcerer.FenTianJue"), TEXT("基础：敌方全体造成70%攻击伤害"), TEXT("编序：第1～2位时，标记改为3层"), TEXT("首牌奖励：敌方全体获得3层标记；回复1点气力，抽2张牌")},
		{TEXT("Profession.Sorcerer.NingYanChengRen"), TEXT("基础：按敌方各自标记快照逐层落雷，每次造成50%攻击伤害"), TEXT("编序：第4～5位时，每次改为65%攻击伤害"), TEXT("首牌奖励：敌方全体先获得5层标记，再各触发5次70%落雷")},
		{TEXT("Profession.Sorcerer.RanLingHuanYuan"), TEXT("基础：按敌方各自标记快照逐层落雷，每次造成30%攻击伤害"), TEXT("编序：第4～5位时，每次改为45%攻击伤害"), TEXT("首牌奖励：敌方全体先获得3层标记，再各触发3次60%落雷")},
		{TEXT("Profession.Sorcerer.YanMuHuTi"), TEXT("基础：敌方全体造成60%攻击伤害"), TEXT("编序：此前每记录1张牌，倍率+25个百分点"), TEXT("首牌奖励·普通：敌方全体造成300%攻击伤害")},
		{TEXT("Profession.Sorcerer.LieFu"), TEXT("基础：自身抽1张牌"), TEXT("编序：第3～5位时，额外回复5点内力"), TEXT("首牌奖励·普通：回复2点气力，抽3张牌；我方全体回复6点内力")},
		{TEXT("Profession.Sorcerer.XingHuoHuiShou"), TEXT("基础：我方全体获得3点护甲"), TEXT("编序：前一张记录牌不含直接伤害时，改为6点护甲"), TEXT("首牌奖励·普通：我方全体获得12点护甲；敌方全体获得2层虚弱")},
		{TEXT("Profession.Sorcerer.ChiYanFengJie"), TEXT("基础：敌方全体造成65%攻击伤害"), TEXT("编序：第4～5位时，每段改为90%攻击伤害"), TEXT("首牌奖励·普通：额外重放第5张记录牌，抽1张牌")}};

	TestEqual(TEXT("Sorcerer text matrix covers exactly 18 cards"), Expected.Num(), 18);
	const TArray<FString> RetiredNames = {
		TEXT("灵火符"), TEXT("聚灵"), TEXT("离火印"), TEXT("炎墙"), TEXT("爆炎术"), TEXT("星火燎原"),
		TEXT("摄灵火"), TEXT("焚脉符"), TEXT("灵焰连弹"), TEXT("护灵幕"), TEXT("赤霄焚星"), TEXT("焚天诀"),
		TEXT("凝焰成刃"), TEXT("燃灵换元"), TEXT("焰幕护体"), TEXT("裂符"), TEXT("星火回收"), TEXT("赤焰封界")};

	for (const FExpectedCardText& Row : Expected)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Row.CardId);
		if (!TestNotNull(FString::Printf(TEXT("%s exists for text acceptance"), *Row.CardId.ToString()), Definition))
		{
			continue;
		}
		const FString Detail = GameXXKCardText::DescribeDetail(*Definition, EGameXXKCardQuality::Common, nullptr);
		TArray<FString> Ordered = {GameXXKCardText::DescribeTarget(Definition->TargetSpec), Row.Base};
		if (FCString::Strlen(Row.Sequence) > 0)
		{
			Ordered.Add(Row.Sequence);
		}
		Ordered.Add(Row.Reward);
		TestOrderedFragments(*this, Row.CardId.ToString(), Detail, Ordered);
		TestFalse(FString::Printf(TEXT("%s never calls Burn 燃烧"), *Row.CardId.ToString()), Detail.Contains(TEXT("燃烧")));
		TestFalse(FString::Printf(TEXT("%s exposes no enum or UI term"), *Row.CardId.ToString()),
			Detail.Contains(TEXT("EGameXXK")) || Detail.Contains(TEXT("按钮")) || Detail.Contains(TEXT("控件")));
		for (const FString& RetiredName : RetiredNames)
		{
			TestFalse(FString::Printf(TEXT("%s does not expose retired name %s"), *Row.CardId.ToString(), *RetiredName), Detail.Contains(RetiredName));
		}
	}

	const TArray<FName> UniversalCards = {
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.LieFu"),
		TEXT("Profession.Sorcerer.XingHuoHuiShou"),
		TEXT("Profession.Sorcerer.ChiYanFengJie")};
	for (const FName CardId : UniversalCards)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!Definition)
		{
			continue;
		}
		const FString Detail = GameXXKCardText::DescribeDetail(*Definition, EGameXXKCardQuality::Common, nullptr);
		TestOrderedFragments(*this, CardId.ToString(), Detail, {
			TEXT("首牌奖励·普通："),
			TEXT("首牌奖励·炎法："),
			TEXT("首牌奖励·寒冰："),
			TEXT("首牌奖励·雷法：")});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerPreviewTargetingTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Text.PreviewAndTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerPreviewTargetingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTextTest;
	TArray<FGameXXKCardInstance> Cards;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == EGameXXKCharacterRole::Sorcerer
			&& Definition.OwnerId == FName(TEXT("Profession.Sorcerer")))
		{
			Cards.Add(MakeCard(Definition.Id, Cards.Num()));
		}
	}
	TestEqual(TEXT("preview fixture finds all 18 permanent Sorcerer cards"), Cards.Num(), 18);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(
		*this,
		Cards,
		{
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 20),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 2),
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)
		},
		59701,
		Runtime)
		|| !InstallAllCardsInHand(*this, Runtime, Cards))
	{
		return false;
	}

	for (const FGameXXKCardInstance& Card : Cards)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Card.CardId);
		if (!Definition)
		{
			AddError(FString::Printf(TEXT("preview fixture lost %s"), *Card.CardId.ToString()));
			continue;
		}
		FGameXXKCardPlayPreview Preview;
		FString Error;
		const FString Context = Card.CardId.ToString();
		if (!TestTrue(FString::Printf(TEXT("%s previews without a selected target: %s"), *Context, *Error),
			GameXXKCardRules::BuildCardPlayPreview(Runtime, Card.InstanceId, Preview, &Error)))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s is currently playable"), *Context), Preview.bCanPlay);
		TestFalse(FString::Printf(TEXT("%s never opens manual targeting"), *Context), Preview.TargetRequest.bRequiresManualSelection);
		TestFalse(FString::Printf(TEXT("%s never opens random targeting"), *Context), Preview.TargetRequest.bRequiresRandomResolution);
		TestEqual(FString::Printf(TEXT("%s retains its automatic target mode"), *Context), Preview.TargetRequest.EffectiveMode, Definition->TargetSpec.Mode);
		TestEqual(
			FString::Printf(TEXT("%s auto-locks stable targets"), *Context),
			Preview.TargetRequest.AutomaticTargetUnitIds,
			ExpectedAutomaticTargets(Definition->TargetSpec.Mode));
	}
	return true;
}

#endif
