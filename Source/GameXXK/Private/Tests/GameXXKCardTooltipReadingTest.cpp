#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardText.h"
#include "GameXXKMVPRules.h"
#include "GameXXKEquipmentRules.h"
#include "UI/GameXXKCardTooltipInteraction.h"
#include "UI/GameXXKCardTooltipWidget.h"
#include "UI/GameXXKInRunUiStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipPlayerLanguageTest,
	"GameXXK.UI.CardTooltip.PlayerFacingLanguage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardTooltipPlayerLanguageTest::RunTest(const FString&)
{
	int32 Checked = 0;
	for (const auto& Card : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		for (const auto Quality : {EGameXXKCardQuality::Common, EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
		{
			if (static_cast<int32>(Quality) < static_cast<int32>(Card.BaseQuality)) continue;
			const FString Text = GameXXKCardText::DescribeCompactTooltipBody(Card, Quality, nullptr, {})
				+ GameXXKCardText::DescribeExpandedTooltipBody(Card, Quality, nullptr, {});
			for (const TCHAR* Internal : {TEXT("标准冰爆"), TEXT("标准寒冰伤害"), TEXT("并各减少1层"),
				TEXT("任务 NPC"), TEXT("本次打出的牌获得"), TEXT("本次打出的牌登记"), TEXT("标记快照")})
				TestFalse(FString::Printf(TEXT("%s excludes obsolete/internal phrase %s"), *Card.Id.ToString(), Internal), Text.Contains(Internal));
			++Checked;
		}
	}
	TestEqual(TEXT("language audit covers every current card quality"), Checked, 419);
	const auto* Ice = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.SheLingHuo"));
	const auto* Medicine = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Healer.YaoYin"));
	const auto* Npc = FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.SongJinBao.GuiKeLing"));
	if (!Ice || !Medicine || !Npc) return false;
	const FString IceText = GameXXKCardText::DescribeCompactTooltipBody(*Ice, EGameXXKCardQuality::Common, nullptr, {});
	TestTrue(TEXT("ice explosion stays inside the task reward and explains armor consumption"),
		IceText.Contains(TEXT("阵赏：冰爆，消耗全部护甲")) && IceText.Contains(TEXT("100%攻击"))
		&& !IceText.Contains(TEXT("\n冰爆：")));
	const FString MedicineText = GameXXKCardText::DescribeCompactTooltipBody(*Medicine, nullptr, {});
	TestTrue(TEXT("health-change formula counts events rather than damage magnitude"),
		MedicineText.Contains(TEXT("每笔伤害")) && MedicineText.Contains(TEXT("1点药效")));
	const FString NpcText = GameXXKCardText::DescribeCompactTooltipBody(*Npc, nullptr, {});
	TestTrue(TEXT("reactive status goes to the next card's owner"), NpcText.Contains(TEXT("该牌出牌者")));
	TestTrue(TEXT("one-shot trigger and next-round timing remain explicit"),
		NpcText.Contains(TEXT("下一张主动牌结算前")) && NpcText.Contains(TEXT("下个玩家回合开始时")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipOwnerPreviewTest,
	"GameXXK.UI.CardTooltip.CorrectPreviewOwner", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardTooltipOwnerPreviewTest::RunTest(const FString&)
{
	FGameXXKRuntimeState State;
	auto& Active = State.CardRun.CompanionRoster.PermanentCompanions.AddDefaulted_GetRef();
	Active.InstanceId = TEXT("Tooltip.ActiveBlade"); Active.Role = EGameXXKCharacterRole::Blade; Active.bIsActive = true;
	const auto* Blade = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.HuiFengJiaShi"));
	const auto* Npc = FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.TusiChief.TuSiJunLing"));
	if (!Blade || !Npc) return false;
	TestEqual(TEXT("shop and reward previews use the active companion rather than hero stats"),
		UGameXXKCardTooltipWidget::ResolveCardOwnerCharacterId(State, *Blade), Active.InstanceId);
	TestEqual(TEXT("NPC cards resolve the real NPC equipment owner"),
		UGameXXKCardTooltipWidget::ResolveCardOwnerCharacterId(State, *Npc), FName(TEXT("Npc.TusiChief")));
	Active.bIsActive = false;
	TestTrue(TEXT("an unavailable profession never silently borrows hero stats"),
		UGameXXKCardTooltipWidget::ResolveCardOwnerCharacterId(State, *Blade).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipModifierRestrictionTest,
	"GameXXK.UI.CardTooltip.ModifierRestrictions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardTooltipModifierRestrictionTest::RunTest(const FString&)
{
	const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
	if (!TestNotNull(TEXT("real next-card discount exists"), Card)) return false;
	const FString Compact = GameXXKCardText::DescribeCompactTooltipBody(*Card, nullptr, {});
	TestTrue(TEXT("compact retains active-play gating"), Compact.Contains(TEXT("主动牌")));
	TestTrue(TEXT("compact retains the excluded source unit"), Compact.Contains(TEXT("其他角色")));
	TestTrue(TEXT("compact retains its one-use limit"), Compact.Contains(TEXT("下一张")));
	TestTrue(TEXT("the benefit still reduces the next card's energy"), Compact.Contains(TEXT("气力-1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTooltipOriginalArtTest,
	"GameXXK.UI.CardTooltip.OriginalPillsAndItemSlot", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTooltipOriginalArtTest::RunTest(const FString&)
{
	FGameXXKCardDefinition Card; Card.Id = TEXT("Test.Tooltip.OriginalArt");
	Card.DisplayName = FText::FromString(TEXT("原样检查")); Card.BaseQuality = EGameXXKCardQuality::Common;
	Card.TargetSpec.Mode = EGameXXKCardTargetMode::SingleEnemy;
	auto& Effect = Card.Effects.AddDefaulted_GetRef(); Effect.Type = EGameXXKCardEffectType::ApplyStatus;
	Effect.Target = EGameXXKCardEffectTarget::SelectedTarget; Effect.Status = EGameXXKCardStatus::Poison; Effect.Magnitude = 2;
	UGameXXKCardTooltipWidget* Tooltip = NewObject<UGameXXKCardTooltipWidget>();
	Tooltip->Initialize(); Tooltip->SetExpandedForTest(false); Tooltip->ConfigureCard(Card, Card.BaseQuality, nullptr, {});
	TSharedRef<SWidget> SlateTooltip = Tooltip->TakeWidget(); SlateTooltip->SlatePrepass();
	UBorder* Paper = Cast<UBorder>(Tooltip->WidgetTree->FindWidget(TEXT("CardTooltipPaper")));
	TestTrue(TEXT("the original ItemSlot paper is retained"), Paper && Paper->Background.GetResourceObject()
		&& Paper->Background.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_ItemSlot")));
	TestTrue(TEXT("the original paper slice is retained"), Paper && Paper->Background.Margin == FMargin(0.065f));
	bool FoundOriginalPill = false;
	Tooltip->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		UBorder* Pill = Cast<UBorder>(Widget); UTextBlock* Label = Pill ? Cast<UTextBlock>(Pill->GetContent()) : nullptr;
		if (Label && Label->GetText().ToString() == TEXT("中毒"))
			FoundOriginalPill = Pill->Background.TintColor.GetSpecifiedColor().Equals(FLinearColor(0.18f,0.13f,0.09f,1))
				&& Label->GetFont().Size == 16;
	});
	TestTrue(TEXT("status Pill keeps its original dark fill and type size"), FoundOriginalPill);
	TestEqual(TEXT("short explanations keep a fixed minimum width"),
		GameXXKCardTooltipPresentation::PreferredWidth(TEXT("造成100点伤害。")), 520.0f);
	TestTrue(TEXT("long explanations grow without exceeding the declared maximum"),
		GameXXKCardTooltipPresentation::PreferredWidth(FString::ChrN(500, TEXT('字'))) > 520.0f
		&& GameXXKCardTooltipPresentation::PreferredWidth(FString::ChrN(500, TEXT('字'))) <= 800.0f);
	return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKCardTooltipUnresolvedBranchTest,
	"GameXXK.UI.CardTooltip.UnresolvedBranchAndLongLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKCardTooltipUnresolvedBranchTest::RunTest(const FString&)
{
	const auto* Card = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Sorcerer.ChiYanFengJie"));
	if (!TestNotNull(TEXT("real universal reward card exists"), Card)) return false;
	const FString Compact = GameXXKCardText::DescribeCompactTooltipBody(*Card, EGameXXKCardQuality::Epic, nullptr, {});
	const FString Detail = GameXXKCardText::DescribeExpandedTooltipBody(*Card, EGameXXKCardQuality::Epic, nullptr, {});
	TestTrue(TEXT("compact explains that the reward branch is not yet fixed"), Compact.Contains(TEXT("任务分支尚未确定")));
	TestFalse(TEXT("compact does not stack four alternative reward paragraphs"), Compact.Contains(TEXT("阵赏·炎法：")));
	for (const TCHAR* Branch : {TEXT("阵赏·普通："), TEXT("阵赏·炎法："), TEXT("阵赏·寒冰："), TEXT("阵赏·雷法：")})
		TestTrue(TEXT("detail preserves every unresolved branch"), Detail.Contains(Branch));
	FGameXXKCardTooltipContext Context; Context.LockedSpellBranch = EGameXXKSorcererTaskBranch::Fire;
	const FString Locked = GameXXKCardText::DescribeCompactTooltipBody(*Card, EGameXXKCardQuality::Epic, nullptr, Context);
	TestTrue(TEXT("a locked branch shows its actual reward"), Locked.Contains(TEXT("阵赏·炎法：")));
	TestFalse(TEXT("a locked branch omits unrelated ice rewards"), Locked.Contains(TEXT("阵赏·寒冰：")));
	UGameXXKCardTooltipWidget* Tooltip = NewObject<UGameXXKCardTooltipWidget>();
	Tooltip->Initialize(); Tooltip->SetExpandedForTest(true); Tooltip->ConfigureCard(*Card, EGameXXKCardQuality::Epic, nullptr, {});
	TSharedRef<SWidget> SlateTooltip = Tooltip->TakeWidget();
	SlateTooltip->SlatePrepass();
	const FVector2D Measured = SlateTooltip->GetDesiredSize();
	AddInfo(FString::Printf(TEXT("expanded tooltip measured %.1f x %.1f"), Measured.X, Measured.Y));
	if (UVerticalBox* Body = Cast<UVerticalBox>(Tooltip->WidgetTree->FindWidget(TEXT("CardTooltipBody"))))
	{
		float Widest = 0, Tallest = 0;
		for (UWidget* Row : Body->GetAllChildren()) { Widest = FMath::Max(Widest, Row->GetDesiredSize().X); Tallest = FMath::Max(Tallest, Row->GetDesiredSize().Y); }
		AddInfo(FString::Printf(TEXT("rows %d, widest %.1f, tallest %.1f; text %s"), Body->GetChildrenCount(), Widest, Tallest, *Tooltip->GetRenderedTextForTest()));
	}
	TestTrue(TEXT("long detailed text fits a bounded panel instead of growing past the screen"),
		Measured.Y > 200 && Measured.Y < 850 && Measured.X >= 520 && Measured.X <= 800);
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
	TestTrue(TEXT("owner input opens the real widget's separate help body"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌术语")));
	Tooltip->UpdateInspectionFromOwner(true, false, false, false);
	TestTrue(TEXT("the real widget retains help on Ctrl release"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌术语")));
	const TArray<FString> RenderedPills = Tooltip->GetPillTextsForTest();
	TestEqual(TEXT("the help render has one poison pill"), RenderedPills.FilterByPredicate([](const FString& Name) { return Name == TEXT("中毒"); }).Num(), 1);
	TestTrue(TEXT("the help render uses the combined charge label"), RenderedPills.Contains(TEXT("蓄力／重箭")));
	Tooltip->UpdateInspectionFromOwner(true, true, false, false);
	const FString Detail = Tooltip->GetDisplayedTextForTest();
	TestFalse(TEXT("Shift has no generic poison explanation"), Detail.Contains(TEXT("任意一方回合结束时")));
	TestFalse(TEXT("Shift has no appended status glossary"), Detail.Contains(TEXT("状态说明：")));
	TestFalse(TEXT("Shift does not repeat the generic heavy-arrow definition"), Detail.Contains(TEXT("逐层触发本牌重箭效果")));
	Tooltip->UpdateInspectionFromOwner(true, false, false, false);
	TestTrue(TEXT("Shift release restores this widget's help"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌术语")));
	Tooltip->UpdateInspectionFromOwner(false, false, false, false);
	TestFalse(TEXT("leaving resets the real widget"), Tooltip->GetDisplayedTextForTest().Contains(TEXT("本牌术语")));
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
				bBoldTarget |= Label->GetText().ToString() == Row.Value
					&& Label->GetFont().TypefaceFontName == TEXT("Default")
					&& Label->GetFont().OutlineSettings.OutlineSize>=1
					&& Label->GetFont().FontObject&&Label->GetFont().FontObject->GetPathName().Contains(TEXT("JiangHuGuFeng"));
			}
		});
		TestTrue(TEXT("recipient text is emphasized without leaving the Jianghu face"), bBoldTarget);
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
