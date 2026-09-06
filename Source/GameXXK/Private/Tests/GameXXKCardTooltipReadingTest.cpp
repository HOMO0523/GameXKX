#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardText.h"
#include "UI/GameXXKCardTooltipInteraction.h"
#include "UI/GameXXKCardTooltipWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipControlTest,
	"GameXXK.UI.CardTooltip.ControlReading", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardTooltipControlTest::RunTest(const FString& Parameters)
{
	FGameXXKCardTooltipInteraction State;
	using EMode = EGameXXKCardTooltipMode;
	State.Update(true, false, false, false);
	State.Update(true, false, true, false);
	TestEqual(TEXT("a fresh Ctrl press opens pill help"), State.GetMode(), EMode::Pills);
	State.Update(true, false, true, false);
	TestEqual(TEXT("holding Ctrl does not toggle repeatedly"), State.GetMode(), EMode::Pills);
	State.Update(true, false, false, false);
	TestEqual(TEXT("Ctrl release preserves the open help"), State.GetMode(), EMode::Pills);
	State.Update(true, true, false, false);
	TestEqual(TEXT("Shift temporarily overrides pill help"), State.GetMode(), EMode::Detail);
	State.Update(true, false, false, false);
	TestEqual(TEXT("Shift release restores pill help"), State.GetMode(), EMode::Pills);
	State.Update(true, false, true, false);
	TestEqual(TEXT("the second Ctrl press closes help"), State.GetMode(), EMode::Compact);
	State.Update(true, true, false, false);
	State.Update(true, true, true, false);
	TestEqual(TEXT("Ctrl during Shift does not replace detail"), State.GetMode(), EMode::Detail);
	State.Update(true, false, false, false);
	TestEqual(TEXT("the selected mode returns after Shift"), State.GetMode(), EMode::Pills);
	State.Update(true, false, false, true);
	TestEqual(TEXT("Escape closes help"), State.GetMode(), EMode::Compact);
	State.Update(true, false, true, false);
	State.Update(false, false, false, false);
	TestEqual(TEXT("leaving or losing window focus closes help"), State.GetMode(), EMode::Compact);
	State.Update(false, false, true, false);
	State.Update(true, false, true, false);
	TestEqual(TEXT("entering with Ctrl already held does not open help"), State.GetMode(), EMode::Compact);
	State.Update(true, false, false, false);
	State.Update(true, false, true, false);
	TestEqual(TEXT("a new press after entering works"), State.GetMode(), EMode::Pills);
	State.Reset();
	TestEqual(TEXT("changing the card resets its reading state"), State.GetMode(), EMode::Compact);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipPillCopyTest,
	"GameXXK.UI.CardTooltip.PillCopy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardTooltipPillCopyTest::RunTest(const FString& Parameters)
{
	FGameXXKCardDefinition Card;
	Card.Id = TEXT("Test.Tooltip.PoisonArrow");
	Card.DisplayName = FText::FromString(TEXT("测试毒箭"));
	Card.BaseQuality = EGameXXKCardQuality::Rare;
	Card.TargetSpec.Mode = EGameXXKCardTargetMode::SingleEnemy;
	FGameXXKCardEffect Poison;
	Poison.Type = EGameXXKCardEffectType::ApplyStatus;
	Poison.Target = EGameXXKCardEffectTarget::SelectedTarget;
	Poison.Status = EGameXXKCardStatus::Poison;
	Poison.Magnitude = 2;
	Card.Effects = {Poison, Poison};
	Card.HeavyArrow.Kind = EGameXXKHeavyArrowKind::ExtraAttackPerCharge;
	Card.HeavyArrow.MagnitudePerCharge = 40;
	const FGameXXKCardTooltipContext Context;
	const FString Help = GameXXKCardText::DescribePillTooltipBody(Card, Card.BaseQuality, Context);
	TestTrue(TEXT("poison uses the approved both-turns sentence"), Help.Contains(TEXT("中毒：任意一方回合结束时，失去等同中毒值的生命。")));
	TestTrue(TEXT("charge and heavy-arrow explanations are merged"), Help.Contains(TEXT("蓄力／重箭：消耗全部蓄力，按消耗量强化本牌。")));
	TestTrue(TEXT("DOT common behavior appears once"), Help.Contains(TEXT("持续伤害直接损失生命，触发不消耗数值。")));
	TestEqual(TEXT("duplicate poison effects produce one explanation"), Help.Find(TEXT("中毒：")), Help.Find(TEXT("中毒："), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
	TestFalse(TEXT("unrelated burn is absent"), Help.Contains(TEXT("灼烧：")));
	TestFalse(TEXT("unrelated armor is absent"), Help.Contains(TEXT("护甲：")));
	TestFalse(TEXT("ordinary resources do not become pill explanations"), Help.Contains(TEXT("内力：")) || Help.Contains(TEXT("气力：")));

	UGameXXKCardTooltipWidget* Tooltip = NewObject<UGameXXKCardTooltipWidget>();
	TestTrue(TEXT("tooltip initializes"), Tooltip->Initialize());
	Tooltip->ConfigureCard(Card, Card.BaseQuality, nullptr, Context);
	Tooltip->TakeWidget();
	Tooltip->UpdateInspectionFromOwner(true, false, false, false);
	Tooltip->UpdateInspectionFromOwner(true, false, true, false);
	TestTrue(TEXT("owner input opens the real widget's separate help body"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌Pill说明")));
	Tooltip->UpdateInspectionFromOwner(true, false, false, false);
	TestTrue(TEXT("the real widget retains help on Ctrl release"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌Pill说明")));
	const TArray<FString> RenderedPills = Tooltip->GetPillTextsForTest();
	TestEqual(TEXT("the help render has one poison pill"), RenderedPills.FilterByPredicate([](const FString& Name) { return Name == TEXT("中毒"); }).Num(), 1);
	TestTrue(TEXT("the help render uses the combined charge label"), RenderedPills.Contains(TEXT("蓄力／重箭")));
	Tooltip->UpdateInspectionFromOwner(true, true, false, false);
	const FString Detail = Tooltip->GetDisplayedTextForTest();
	TestFalse(TEXT("Shift has no generic poison explanation"), Detail.Contains(TEXT("任意一方回合结束时")));
	TestFalse(TEXT("Shift has no appended status glossary"), Detail.Contains(TEXT("状态说明：")));
	TestFalse(TEXT("Shift does not repeat the generic heavy-arrow definition"), Detail.Contains(TEXT("逐层触发本牌重箭效果")));
	Tooltip->UpdateInspectionFromOwner(true, false, false, false);
	TestTrue(TEXT("Shift release restores this widget's help"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌Pill说明")));
	Tooltip->UpdateInspectionFromOwner(false, false, false, false);
	TestFalse(TEXT("leaving resets the real widget"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌Pill说明")));
	TestEqual(TEXT("reading never mutates the card's authored effects"), Card.Effects.Num(), 2);

	Card.HeavyArrow.Kind = EGameXXKHeavyArrowKind::None;
	Card.Effects.SetNum(1);
	Card.Effects[0].Status = EGameXXKCardStatus::CannotReceiveVulnerability;
	const FString ImmunityHelp = GameXXKCardText::DescribePillTooltipBody(Card, Card.BaseQuality, Context);
	TestTrue(TEXT("compound status has its own explanation"), ImmunityHelp.Contains(TEXT("破绽免疫：")));
	TestFalse(TEXT("compound status does not introduce a second partial-name pill"), ImmunityHelp.Contains(TEXT("\n破绽：")));
	Card.Effects[0].Status = EGameXXKCardStatus::Counter;
	TestTrue(TEXT("Rare counter help includes its effective attack multiplier"),
		GameXXKCardText::DescribePillTooltipBody(Card, EGameXXKCardQuality::Rare, Context).Contains(TEXT("120%攻击伤害")));
	TestTrue(TEXT("Epic counter help follows the currently displayed quality"),
		GameXXKCardText::DescribePillTooltipBody(Card, EGameXXKCardQuality::Epic, Context).Contains(TEXT("140%攻击伤害")));
	Card.SpellTaskReward = EGameXXKHeroSpellTaskReward::Fire;
	TestTrue(TEXT("Hero task help uses four distinct cards"),
		GameXXKCardText::DescribePillTooltipBody(Card, Card.BaseQuality, Context).Contains(TEXT("本组4种牌")));

	const FGameXXKCardDefinition* Universal = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.YanMuHuTi"));
	TestNotNull(TEXT("universal card fixture exists"), Universal);
	if (Universal)
	{
		FGameXXKCardTooltipContext FireContext;
		FireContext.LockedSpellBranch = EGameXXKSorcererTaskBranch::Fire;
		const FString FireHelp = GameXXKCardText::DescribePillTooltipBody(*Universal, Universal->BaseQuality, FireContext);
		TestTrue(TEXT("the locked Fire reward explains burn"), FireHelp.Contains(TEXT("灼烧：")));
		TestFalse(TEXT("the locked Fire reward excludes Lightning-only mark"), FireHelp.Contains(TEXT("标记：")));
		TestFalse(TEXT("the locked Fire reward excludes Ice-only armor"), FireHelp.Contains(TEXT("护甲：")));
		TestTrue(TEXT("partner task help uses five distinct cards"), FireHelp.Contains(TEXT("本组5种牌")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipTargetHeadingTest,
	"GameXXK.UI.CardTooltip.TargetHeading", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardTooltipTargetHeadingTest::RunTest(const FString& Parameters)
{
	for (const auto& Row : TArray<TPair<EGameXXKCardTargetMode, FString>>{
		{EGameXXKCardTargetMode::Self, TEXT("单体友方")},
		{EGameXXKCardTargetMode::SingleAlly, TEXT("单体友方")},
		{EGameXXKCardTargetMode::SingleEnemy, TEXT("单体敌方")},
		{EGameXXKCardTargetMode::AnyLivingUnit, TEXT("单体友方/敌方")},
		{EGameXXKCardTargetMode::AllEnemies, TEXT("全体敌方")},
		{EGameXXKCardTargetMode::AllAllies, TEXT("全体友方")}})
	{
		FGameXXKCardDefinition Card;
		Card.Id = TEXT("Test.Tooltip.Target");
		Card.DisplayName = FText::FromString(TEXT("目标测试"));
		Card.TargetSpec.Mode = Row.Key;
		TestEqual(TEXT("recipient label is separate from source and targeting instructions"), GameXXKCardText::DescribeTargetHeading(Card), Row.Value);
		UGameXXKCardTooltipWidget* Tooltip = NewObject<UGameXXKCardTooltipWidget>();
		Tooltip->Initialize();
		Tooltip->SetExpandedForTest(false);
		Tooltip->ConfigureCard(Card, EGameXXKCardQuality::Common, nullptr, {});
		Tooltip->TakeWidget();
		const FString Text = Tooltip->GetRenderedTextForTest();
		TestTrue(TEXT("the target occupies its own line"), Text.Contains(TEXT("\n") + Row.Value + TEXT("\n")));
		TestFalse(TEXT("target label has no prefix"), Text.Contains(TEXT("目标：")) || Text.Contains(TEXT("对象：")));
		bool bBoldTarget = false;
		Tooltip->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (const UTextBlock* Label = Cast<UTextBlock>(Widget))
			{
				bBoldTarget |= Label->GetText().ToString() == Row.Value && Label->GetFont().TypefaceFontName == TEXT("Bold");
			}
		});
		TestTrue(TEXT("recipient text is bold"), bBoldTarget);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipTargetConstraintsTest,
	"GameXXK.UI.CardTooltip.TargetConstraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardTooltipTargetConstraintsTest::RunTest(const FString& Parameters)
{
	FGameXXKCardDefinition Card;
	Card.TargetSpec.Mode = EGameXXKCardTargetMode::OtherAlly;
	Card.TargetSpec.bRequireDifferentFromOwner = true;
	Card.TargetSpec.RequiredStatus = EGameXXKCardStatus::Bleed;
	Card.TargetSpec.RequiredStatusMinimumStacks = 2;
	Card.TargetSpec.MaximumHealthPercent = 35;
	const FString Detail = GameXXKCardText::DescribeExpandedTooltipBody(Card, EGameXXKCardQuality::Common, nullptr, {});
	TestTrue(TEXT("moving the target label preserves self-exclusion in prose"), Detail.Contains(TEXT("不能选择出牌者自身")));
	TestTrue(TEXT("required status remains a readable restriction in DOT points"), Detail.Contains(TEXT("至少2点流血")));
	TestTrue(TEXT("health restriction is not lost with the former target block"), Detail.Contains(TEXT("35%")));
	TestFalse(TEXT("restriction prose does not restore the target-prefix heading"), Detail.Contains(TEXT("目标：")));
	Card = FGameXXKCardDefinition();
	Card.TargetSpec.Mode = EGameXXKCardTargetMode::SingleAlly;
	FGameXXKCardEffect Blast;
	Blast.Type = EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor;
	Blast.Target = EGameXXKCardEffectTarget::SelectedTarget;
	Card.Effects.Add(Blast);
	TestEqual(TEXT("consuming an ally's armor also identifies the group of enemy recipients"),
		GameXXKCardText::DescribeTargetHeading(Card), FString(TEXT("单体友方 · 全体敌方")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipSemanticSummaryTest,
	"GameXXK.UI.CardTooltip.SemanticSummary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardTooltipSemanticSummaryTest::RunTest(const FString&)
{
	const auto* Order=FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.TusiChief.TuSiJunLing"));
	FGameXXKCardTooltipContext Context;Context.UnavailableReason=TEXT("测试内力不足");
	const FString Text=GameXXKCardText::DescribeCompactTooltipBody(*Order,Order->BaseQuality,nullptr,Context);
	TestTrue(TEXT("compact preserves charge"),Text.Contains(TEXT("冲锋：")));TestTrue(TEXT("compact preserves finish after three base rows"),Text.Contains(TEXT("收招：")));
	TestTrue(TEXT("unavailable reason cannot be truncated"),Text.Contains(Context.UnavailableReason));TestFalse(TEXT("joined clauses have no double punctuation"),Text.Contains(TEXT("。；")));
	FGameXXKCardPlayPreview Preview;Preview.bCanPlay=true;Preview.TargetRequest.bRequiresManualSelection=true;
	for(int32 I=0;I<3;++I){auto& V=Preview.TargetRequest.CandidateViews.AddDefaulted_GetRef();V.bCanSelect=true;}
	const FString Selection=GameXXKCardText::DescribeCompactTooltipBody(*Order,Order->BaseQuality,&Preview,{});
	TestTrue(TEXT("multiple candidates still require exactly one choice"),Selection.Contains(TEXT("选择一名高亮目标")));
	TestFalse(TEXT("candidate count is not selection count"),Selection.Contains(TEXT("请选择 3 个")));
	return true;
}
#endif
