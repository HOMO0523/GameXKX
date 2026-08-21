#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleStatusIconStyle.h"
#include "UI/GameXXKBattleStatusIconWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardStatusStack MakeStatusStack(const EGameXXKCardStatus Status, const int32 Stacks)
	{
		FGameXXKCardStatusStack Result;
		Result.Status = Status;
		Result.Stacks = Stacks;
		return Result;
	}

	UGameXXKBattleStatusIconWidget* GetStatusIconAt(
		UGameXXKBattleUnitStatusEffectsWidget* const EffectsWidget,
		const int32 Index)
	{
		UHorizontalBox* const StatusIconRow = EffectsWidget
			? Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")))
			: nullptr;
		return StatusIconRow ? Cast<UGameXXKBattleStatusIconWidget>(StatusIconRow->GetChildAt(Index)) : nullptr;
	}

	FString GetStatusIconTooltipText(UGameXXKBattleStatusIconWidget* const IconWidget)
	{
		UBorder* const HitTarget = IconWidget
			? Cast<UBorder>(IconWidget->GetWidgetFromName(TEXT("BattleStatusIconHitTarget")))
			: nullptr;
		UBorder* const TooltipPaper = HitTarget ? Cast<UBorder>(HitTarget->GetToolTip()) : nullptr;
		UTextBlock* const TooltipText = TooltipPaper ? Cast<UTextBlock>(TooltipPaper->GetContent()) : nullptr;
		return TooltipText ? TooltipText->GetText().ToString() : FString();
	}

	FGameXXKCardCombatUnit MakeStatusPresentationUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Mana = 20;
		Unit.MaxMana = 20;
		Unit.Attack = 20;
		Unit.Speed = 8;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit MakeStatusPresentationLegacyUnit(
		const TCHAR* UnitId,
		const TCHAR* DisplayName,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(UnitId);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.MP = 20;
		Unit.MaxMP = 20;
		Unit.Attack = 20;
		Unit.Speed = 8;
		Unit.Shield = 0;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	bool BuildPositiveStatusPresentationFixture(
		UGameXXKMVPSubsystem* const Subsystem,
		FName& OutCardInstanceId,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		if (!Subsystem)
		{
			OutError = TEXT("The status presentation subsystem is missing.");
			return false;
		}
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 83;
		State.ActiveBattleParty = {
			MakeStatusPresentationLegacyUnit(TEXT("Npc.JinGui"), TEXT("金贵"), false)};
		State.ActiveBattleEnemies = {
			MakeStatusPresentationLegacyUnit(TEXT("Enemy.Status"), TEXT("状态敌人"), true)};

		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Status.Presentation.Card.%d"), Index));
			Card.CardId = TEXT("Npc.JinGui.QiaoYanZhouXuan");
			Card.OwnerUnitId = TEXT("Npc.JinGui");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Status.Presentation.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		FGameXXKCardBattleRuntime Runtime;
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeStatusPresentationUnit(TEXT("Npc.JinGui"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 0),
			MakeStatusPresentationUnit(TEXT("Enemy.Status"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0)};
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			8802,
			&OutError))
		{
			return false;
		}
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = State.ActiveBattleNodeId;
		State.CardRun.ActiveBattle = MoveTemp(Runtime);
		if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &OutError)
			|| State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			return false;
		}
		OutCardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		return true;
	}

	template <typename TPresentation, typename = void>
	struct TStatusPresentationDeltaApi
	{
		static constexpr bool bAvailable = false;
		static int32 Count(const FGameXXKCardBattleRuntime&, const FGameXXKCardBattleRuntime&) { return INDEX_NONE; }
		static FName UnitId(const FGameXXKCardBattleRuntime&, const FGameXXKCardBattleRuntime&, int32) { return NAME_None; }
		static EGameXXKCardStatus Status(const FGameXXKCardBattleRuntime&, const FGameXXKCardBattleRuntime&, int32) { return EGameXXKCardStatus::Invalid; }
		static int32 Delta(const FGameXXKCardBattleRuntime&, const FGameXXKCardBattleRuntime&, int32) { return 0; }
		static EGameXXKBattleAnimationAction Action(const FGameXXKCardBattleRuntime&, const FGameXXKCardBattleRuntime&, int32) { return EGameXXKBattleAnimationAction::Idle; }
	};

	template <typename TPresentation>
	struct TStatusPresentationDeltaApi<TPresentation, std::void_t<decltype(TPresentation::BuildStatusPresentationEvents(
		std::declval<const FGameXXKCardBattleRuntime&>(),
		std::declval<const FGameXXKCardBattleRuntime&>()))>>
	{
		static constexpr bool bAvailable = true;
		static auto Events(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After)
		{
			return TPresentation::BuildStatusPresentationEvents(Before, After);
		}
		static int32 Count(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After)
		{
			return Events(Before, After).Num();
		}
		static FName UnitId(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After, const int32 Index)
		{
			const auto Built = Events(Before, After);
			return Built.IsValidIndex(Index) ? Built[Index].UnitId : NAME_None;
		}
		static EGameXXKCardStatus Status(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After, const int32 Index)
		{
			const auto Built = Events(Before, After);
			return Built.IsValidIndex(Index) ? Built[Index].Status : EGameXXKCardStatus::Invalid;
		}
		static int32 Delta(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After, const int32 Index)
		{
			const auto Built = Events(Before, After);
			return Built.IsValidIndex(Index) ? Built[Index].StackDelta : 0;
		}
		static EGameXXKBattleAnimationAction Action(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After, const int32 Index)
		{
			const auto Built = Events(Before, After);
			return Built.IsValidIndex(Index) ? Built[Index].AnimationAction : EGameXXKBattleAnimationAction::Idle;
		}
	};

	template <typename TBoard, typename = void>
	struct TBoardStatusPresentationApi
	{
		static constexpr bool bAvailable = false;
		static bool IsStatus(const TBoard*) { return false; }
		static FString AssetId(const TBoard*) { return FString(); }
		static int32 Delta(const TBoard*) { return 0; }
		static FName IconId(const TBoard*) { return NAME_None; }
		static double Duration(const TBoard*) { return 0.0; }
	};

	template <typename TBoard>
	struct TBoardStatusPresentationApi<TBoard, std::void_t<
		decltype(std::declval<const TBoard&>().IsBattleStatusPresentationActiveForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattleStatusAnimationAssetIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattleStatusDeltaForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattleStatusIconIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationDurationForTest())>>
	{
		static constexpr bool bAvailable = true;
		static bool IsStatus(const TBoard* Board) { return Board->IsBattleStatusPresentationActiveForTest(); }
		static FString AssetId(const TBoard* Board) { return Board->GetActiveBattleStatusAnimationAssetIdForTest(); }
		static int32 Delta(const TBoard* Board) { return Board->GetActiveBattleStatusDeltaForTest(); }
		static FName IconId(const TBoard* Board) { return Board->GetActiveBattleStatusIconIdForTest(); }
		static double Duration(const TBoard* Board) { return Board->GetActiveBattlePresentationDurationForTest(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleStatusTooltipsTest,
	"GameXXK.UI.Battle.StatusTooltips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleStatusTooltipsTest::RunTest(const FString& Parameters)
{
	struct FExpectedStatusTooltip
	{
		EGameXXKCardStatus Status;
		const TCHAR* DisplayName;
		const TCHAR* Rule;
		const TCHAR* IconId;
	};

	const TArray<FExpectedStatusTooltip> ExpectedStatuses = {
		{EGameXXKCardStatus::Momentum, TEXT("气势"), TEXT("每层使每段攻击伤害+1；仅指定牌与驱散会消耗。"), TEXT("MomentumSeal")},
		{EGameXXKCardStatus::Agility, TEXT("灵动"), TEXT("25%概率消耗1层完美闪避；失败时可消耗2层闪避。"), TEXT("AgilityWing")},
		{EGameXXKCardStatus::Vulnerability, TEXT("破绽"), TEXT("每层使下一段直接攻击伤害提高10%；结算后清空。"), TEXT("VulnerabilityMask")},
		{EGameXXKCardStatus::Bleed, TEXT("流血"), TEXT("受到直接攻击后，失去等同层数的生命并减少1层；回合结束不衰减。"), TEXT("BleedDrop")},
		{EGameXXKCardStatus::Poison, TEXT("中毒"), TEXT("回合结束时，失去等同层数的生命并减少1层。"), TEXT("PoisonVial")},
		{EGameXXKCardStatus::Burn, TEXT("灼烧"), TEXT("打出牌或执行意图后，失去等同层数的生命并减少1层；回合结束再减少1层。"), TEXT("BurnFlame")},
		{EGameXXKCardStatus::Mark, TEXT("标记"), TEXT("直接攻击伤害提高15%；每段有效命中后减少1层。"), TEXT("MarkTarget")},
		{EGameXXKCardStatus::Guard, TEXT("守护"), TEXT("下一次针对本单位的单体攻击由守护者承受；触发后减少1层。"), TEXT("GuardShield")},
		{EGameXXKCardStatus::DamageOverTime, TEXT("蚀伤"), TEXT("流血、中毒或灼烧造成伤害时，额外失去等同层数的生命；回合结束减少1层。"), TEXT("RotSpiral")},
		{EGameXXKCardStatus::CannotReceiveVulnerability, TEXT("破绽免疫"), TEXT("无法获得新的破绽；不会自行消耗。"), TEXT("ImmunityTalisman")},
		{EGameXXKCardStatus::NextAttackBonus, TEXT("追击标记"), TEXT("下一次攻击的首段命中施加1层标记；出手后减少1层。"), TEXT("TacticSeal")},
		{EGameXXKCardStatus::NextAttackAppliesVulnerability, TEXT("破绽追击"), TEXT("下一次攻击的首段命中施加1层破绽；出手后减少1层。"), TEXT("TacticSeal")},
		{EGameXXKCardStatus::NextHealingBonus, TEXT("疗愈增幅"), TEXT("下一次治疗中，每个目标的治疗量增加等同层数的数值；结算后清空。"), TEXT("TacticSeal")},
		{EGameXXKCardStatus::TerrainBonusDouble, TEXT("地形双效"), TEXT("队伍下一张地形牌的地形条件效果额外结算1次；使用后减少1层。"), TEXT("TerrainAndRedirect")},
		{EGameXXKCardStatus::NextTerrainCardFree, TEXT("地形免耗"), TEXT("队伍下一张地形牌的气力消耗变为0；使用后减少1层。"), TEXT("TerrainAndRedirect")},
		{EGameXXKCardStatus::NextTerrainCardEnergyReduction, TEXT("地形减耗"), TEXT("队伍下一张地形牌的气力消耗-1；使用后减少1层。"), TEXT("TerrainAndRedirect")},
		{EGameXXKCardStatus::RedirectSingleTargetEnemyAttack, TEXT("代挡"), TEXT("替队友承受下一次敌方单体攻击；触发后减少1层。"), TEXT("TerrainAndRedirect")},
		{EGameXXKCardStatus::TerrainBonusDoubleThisRound, TEXT("本回合地形双效"), TEXT("本回合队伍下一张地形牌的地形条件效果额外结算1次；使用或回合结束时清除。"), TEXT("TerrainAndRedirect")},
		{EGameXXKCardStatus::Medicine, TEXT("药效"), TEXT("下一次治疗或治疗反转每层+1；结算时全部消耗。"), TEXT("MedicineHerbs")},
		{EGameXXKCardStatus::Weak, TEXT("虚弱"), TEXT("直接攻击伤害降低50%；回合结束减少1层。"), TEXT("WeakBrokenBlade")},
		{EGameXXKCardStatus::Wealth, TEXT("财富"), TEXT("钱潮冲击每层伤害+15；散财疗伤最多消耗3层，每层回复6%最大生命。"), TEXT("WealthCoin")},
		{EGameXXKCardStatus::Rage, TEXT("狂怒"), TEXT("受到玩家牌的生命伤害时增加1层；怒獠每层伤害+20。"), TEXT("RageFlame")},
		{EGameXXKCardStatus::Prey, TEXT("猎物"), TEXT("老虎锁定的目标；虎扑将攻击该单位。"), TEXT("PreyTargetEye")},
		{EGameXXKCardStatus::Charge, TEXT("蓄力"), TEXT("层数表示剩余蓄力回合；归零后执行已准备的意图。"), TEXT("ChargeSpiralHorn")},
		{EGameXXKCardStatus::Counter, TEXT("反击"), TEXT("敌方单体攻击牌结算后，造成100%攻击并消耗1次。"), TEXT("CounterHookBlade")},
		{EGameXXKCardStatus::Block, TEXT("格挡"), TEXT("敌方单体攻击牌结算后，造成100%攻击＋当前护甲并消耗1次。"), TEXT("BlockShield")},
	};

	const FGameXXKBattleStatusIconStyle ArmorStyle = FGameXXKBattleStatusIconStyle::ResolveArmorIconStyle();
	TestEqual(TEXT("armor uses the approved display name"), ArmorStyle.DisplayName, FString(TEXT("护甲")));
	TestEqual(TEXT("armor uses one concise rule"), ArmorStyle.Tooltip, FString(TEXT("优先抵挡直接攻击伤害；所属阵营回合开始时清空。")));
	TestEqual(
		TEXT("armor tooltip keeps the full real value and exactly one rule line"),
		FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(ArmorStyle, 128),
		FString(TEXT("护甲\n层数：128\n优先抵挡直接攻击伤害；所属阵营回合开始时清空。")));

	for (const FExpectedStatusTooltip& Expected : ExpectedStatuses)
	{
		const FString Label = FString::Printf(TEXT("%s (%d)"), Expected.DisplayName, static_cast<int32>(Expected.Status));
		const FGameXXKBattleStatusIconStyle Style = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(Expected.Status);
		TestEqual(*FString::Printf(TEXT("%s has the approved display name"), *Label), Style.DisplayName, FString(Expected.DisplayName));
		TestEqual(*FString::Printf(TEXT("%s has the approved concise rule"), *Label), Style.Tooltip, FString(Expected.Rule));
		TestEqual(*FString::Printf(TEXT("%s has an explicit icon mapping"), *Label), Style.IconId, FName(Expected.IconId));
		TestFalse(*FString::Printf(TEXT("%s does not use the unknown fallback"), *Label), Style.bFallback);

		const FString Tooltip = FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(Style, 128);
		TArray<FString> Lines;
		Tooltip.ParseIntoArrayLines(Lines, false);
		TestEqual(*FString::Printf(TEXT("%s tooltip is title, real layer, and one rule"), *Label), Lines.Num(), 3);
		TestTrue(*FString::Printf(TEXT("%s tooltip retains the uncapped real layer"), *Label), Tooltip.Contains(TEXT("层数：128")));
		TestFalse(*FString::Printf(TEXT("%s tooltip has no retired effect prefix"), *Label), Tooltip.Contains(TEXT("效果：")));
		TestFalse(*FString::Printf(TEXT("%s tooltip has no retired timing prefix"), *Label), Tooltip.Contains(TEXT("时机：")));
	}

	const FGameXXKBattleStatusIconStyle BlockStyle =
		FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(EGameXXKCardStatus::Block);
	TestEqual(
		TEXT("block resolves the approved imported texture path"),
		BlockStyle.TexturePath.ToString(),
		FString(TEXT("/Game/GameXXK/UI/Battle/StatusIcons/T_BattleStatus_BlockShield.T_BattleStatus_BlockShield")));
	TestNotNull(
		TEXT("block imported texture is loadable instead of falling back to a glyph"),
		BlockStyle.TexturePath.TryLoad());

	TestEqual(TEXT("only the icon caps values above ninety-nine"), UGameXXKBattleStatusIconWidget::FormatStackForTest(128), FString(TEXT("99+")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleStatusEffectsWidgetTest,
	"GameXXK.UI.Battle.StatusEffectsWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleStatusEffectsWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleUnitStatusEffectsWidget* const EffectsWidget = NewObject<UGameXXKBattleUnitStatusEffectsWidget>();
	TestNotNull(TEXT("status effects widget is created"), EffectsWidget);
	if (!EffectsWidget)
	{
		return false;
	}

	const auto VerifyReflectedRenderedBadgeGetter = [this, EffectsWidget](const FName FunctionName)
	{
		const UFunction* const Function = EffectsWidget->FindFunction(FunctionName);
		TestNotNull(*FString::Printf(TEXT("%s is exposed for real-PIE badge inspection"), *FunctionName.ToString()), Function);
		if (Function)
		{
			TestTrue(*FString::Printf(TEXT("%s is BlueprintPure"), *FunctionName.ToString()), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
#if WITH_METADATA
			TestTrue(*FString::Printf(TEXT("%s is development-only"), *FunctionName.ToString()), Function->HasMetaData(TEXT("DevelopmentOnly")));
#else
			TestTrue(*FString::Printf(TEXT("%s development-only metadata is unavailable in this target"), *FunctionName.ToString()), true);
#endif
		}
	};
	VerifyReflectedRenderedBadgeGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitStatusEffectsWidget, GetIconCountForTest));
	VerifyReflectedRenderedBadgeGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitStatusEffectsWidget, GetIconIdForTest));
	VerifyReflectedRenderedBadgeGetter(GET_FUNCTION_NAME_CHECKED(UGameXXKBattleUnitStatusEffectsWidget, GetIconDisplayedStackForTest));

	TestTrue(TEXT("status effects widget prepares a native runtime tree for screen-space embedding"), EffectsWidget->PrepareForScreenSpaceEmbedding());
	TestTrue(TEXT("status effects widget retains its native runtime tree"), EffectsWidget->HasRuntimeWidgetTreeForTest());
	TestEqual(TEXT("effects root is input-transparent"), EffectsWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(
		TEXT("effects root exposes the required input transparency contract"),
		UGameXXKBattleUnitStatusEffectsWidget::GetRootHitTestVisibilityForTest(),
		ESlateVisibility::SelfHitTestInvisible);

	TArray<FGameXXKCardStatusStack> InitialStatuses;
	InitialStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Poison, 2));
	InitialStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 3));
	EffectsWidget->SetStatusEffects(7, InitialStatuses);
	TestEqual(TEXT("armor, poison, and bleed expose exactly three visible badges"), EffectsWidget->GetIconCountForTest(), 3);
	TestEqual(TEXT("armor is the first sorted icon"), EffectsWidget->GetIconIdForTest(0), FName(TEXT("ArmorShield")));
	TestEqual(TEXT("armor badge renders the active armor stack"), EffectsWidget->GetIconDisplayedStackForTest(0), FString(TEXT("7")));
	TestEqual(TEXT("bleed is the second sorted icon"), EffectsWidget->GetIconIdForTest(1), FName(TEXT("BleedDrop")));
	TestEqual(TEXT("bleed badge renders the active bleed stack"), EffectsWidget->GetIconDisplayedStackForTest(1), FString(TEXT("3")));
	TestEqual(TEXT("poison is the third sorted icon"), EffectsWidget->GetIconIdForTest(2), FName(TEXT("PoisonVial")));
	TestEqual(TEXT("poison stack is displayed as its resolved stack count"), EffectsWidget->GetIconDisplayedStackForTest(2), FString(TEXT("2")));
	TestEqual(TEXT("normal icon stacks cap at a readable value"), UGameXXKBattleStatusIconWidget::FormatStackForTest(150), FString(TEXT("99+")));
	UGameXXKBattleStatusIconWidget* const BleedIcon = GetStatusIconAt(EffectsWidget, 1);
	UGameXXKBattleStatusIconWidget* const PoisonIcon = GetStatusIconAt(EffectsWidget, 2);
	TestNotNull(TEXT("bleed has a live badge for tooltip inspection"), BleedIcon);
	TestNotNull(TEXT("poison has a live badge for tooltip inspection"), PoisonIcon);
	if (BleedIcon)
	{
		const FString BleedTooltip = GetStatusIconTooltipText(BleedIcon);
		TestEqual(TEXT("bleed tooltip uses the approved on-hit rule"),
			BleedTooltip,
			FString(TEXT("流血\n层数：3\n受到直接攻击后，失去等同层数的生命并减少1层；回合结束不衰减。")));
	}
	if (PoisonIcon)
	{
		const FString PoisonTooltip = GetStatusIconTooltipText(PoisonIcon);
		TestEqual(TEXT("poison tooltip uses the approved owner-end rule"),
			PoisonTooltip,
			FString(TEXT("中毒\n层数：2\n回合结束时，失去等同层数的生命并减少1层。")));
	}
	UHorizontalBox* StatusIconRow = Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")));
	TestNotNull(TEXT("effects widget exposes its live status icon row"), StatusIconRow);
	if (StatusIconRow)
	{
		TestEqual(TEXT("the live nonempty effects row is input-transparent"), StatusIconRow->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	EffectsWidget->SetVisibility(ESlateVisibility::Visible);
	TestTrue(TEXT("effects widget re-prepares after an outer visibility reset"), EffectsWidget->PrepareForScreenSpaceEmbedding());
	TestEqual(TEXT("repreparing restores effects outer input transparency"), EffectsWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	StatusIconRow = Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")));
	TestNotNull(TEXT("effects widget retains its live status icon row after reprepare"), StatusIconRow);
	if (StatusIconRow)
	{
		TestEqual(TEXT("the live nonempty effects row remains input-transparent after reprepare"), StatusIconRow->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}

	UGameXXKBattleStatusIconWidget* const FirstIcon = GetStatusIconAt(EffectsWidget, 0);
	TestNotNull(TEXT("effects row embeds actual native icon widgets"), FirstIcon);
	if (FirstIcon)
	{
		TestTrue(TEXT("embedded icon prepares its own native runtime tree"), FirstIcon->HasRuntimeWidgetTreeForTest());
		FirstIcon->SetVisibility(ESlateVisibility::Visible);
		TestTrue(TEXT("embedded icon re-prepares after an outer visibility reset"), FirstIcon->PrepareForScreenSpaceEmbedding());
		TestEqual(TEXT("repreparing restores embedded icon outer input transparency"), FirstIcon->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		FirstIcon->SetVisibility(ESlateVisibility::Hidden);
		TestTrue(TEXT("a deliberately hidden prepared icon can re-enter construction"), FirstIcon->PrepareForScreenSpaceEmbedding());
		TestEqual(
			TEXT("re-entering construction never resurrects a hidden empty cinematic badge"),
			FirstIcon->GetVisibility(),
			ESlateVisibility::Hidden);
		FirstIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		USizeBox* const IconRoot = Cast<USizeBox>(FirstIcon->GetWidgetFromName(TEXT("BattleStatusIconRoot")));
		TestNotNull(TEXT("embedded icon exposes its native size-box root"), IconRoot);
		if (IconRoot)
		{
			TestEqual(TEXT("embedded icon root is input-transparent"), IconRoot->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
			TestTrue(TEXT("embedded icon uses a readable forty-four-pixel logical width"), IconRoot->GetWidthOverride() >= 44.0f);
		TestTrue(TEXT("embedded icon uses a readable forty-four-pixel logical height"), IconRoot->GetHeightOverride() >= 44.0f);
		TestTrue(TEXT("status artwork overscans the paper inset so the glyph fills the badge"),
			UGameXXKBattleStatusIconWidget::GetIconOverscanPaddingForTest() <= -2.0f);
		}
		UBorder* const HitTarget = Cast<UBorder>(FirstIcon->GetWidgetFromName(TEXT("BattleStatusIconHitTarget")));
		TestNotNull(TEXT("embedded icon exposes its pointer-active hit target"), HitTarget);
		if (HitTarget)
		{
			TestEqual(TEXT("the icon hit target is pointer-active"), HitTarget->GetVisibility(), ESlateVisibility::Visible);
			const FReply MouseDownReply = FirstIcon->GetMouseButtonDownReplyForTest();
			TestFalse(TEXT("the production icon mouse-down reply is unhandled for board targeting"), MouseDownReply.IsEventHandled());
			TestTrue(TEXT("the pointer-active icon explicitly passes mouse-down through to board targeting"),
				FirstIcon->DoesMouseDownPassThroughForTest());
			TestEqual(TEXT("the boolean pass-through seam derives from the production mouse-down reply"),
				FirstIcon->DoesMouseDownPassThroughForTest(),
				!MouseDownReply.IsEventHandled());
			UWidget* const TooltipWidget = HitTarget->GetToolTip();
			TestNotNull(TEXT("the icon hit target owns a tooltip widget"), TooltipWidget);
			if (TooltipWidget)
			{
				TestEqual(TEXT("the tooltip itself does not block hit testing"), TooltipWidget->GetVisibility(), ESlateVisibility::HitTestInvisible);
			}
		}
	}
	TestEqual(
		TEXT("icon hit target exposes the required pointer visibility contract"),
		UGameXXKBattleStatusIconWidget::GetHitTargetVisibilityForTest(),
		ESlateVisibility::Visible);
	TestEqual(
		TEXT("icon tooltip exposes the required input transparency contract"),
		UGameXXKBattleStatusIconWidget::GetTooltipVisibilityForTest(),
		ESlateVisibility::HitTestInvisible);

	const int32 InitialGeneration = EffectsWidget->GetIconRebuildGenerationForTest();
	EffectsWidget->SetStatusEffects(7, InitialStatuses);
	TestEqual(TEXT("an unchanged status snapshot does not rebuild icon children"), EffectsWidget->GetIconRebuildGenerationForTest(), InitialGeneration);
	EffectsWidget->SetStatusEffects(0, TArray<FGameXXKCardStatusStack>());
	TestEqual(TEXT("clearing the snapshot removes all icon children"), EffectsWidget->GetIconCountForTest(), 0);
	TestEqual(TEXT("clearing a changed snapshot rebuilds the icon row"), EffectsWidget->GetIconRebuildGenerationForTest(), InitialGeneration + 1);
	StatusIconRow = Cast<UHorizontalBox>(EffectsWidget->GetWidgetFromName(TEXT("BattleUnitStatusEffectsRow")));
	TestNotNull(TEXT("empty status effects retains its row for future refreshes"), StatusIconRow);
	if (StatusIconRow)
	{
		TestEqual(TEXT("only an empty status row is collapsed"), StatusIconRow->GetVisibility(), ESlateVisibility::Collapsed);
	}

	TArray<FGameXXKCardStatusStack> OverflowStatuses;
	// Armor and five bleed entries fit; the sixth bleed entry plus mark must be listed by overflow.
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Bleed, 1));
	OverflowStatuses.Add(MakeStatusStack(EGameXXKCardStatus::Mark, 1));
	EffectsWidget->SetStatusEffects(4, OverflowStatuses);
	TestEqual(TEXT("armor plus seven statuses uses six icons and one overflow icon"), EffectsWidget->GetIconCountForTest(), 7);
	TestEqual(TEXT("the final icon is the overflow indicator"), EffectsWidget->GetIconIdForTest(6), FName(TEXT("MoreStatuses")));
	TestEqual(TEXT("the overflow indicator reports the omitted count"), EffectsWidget->GetIconDisplayedStackForTest(6), FString(TEXT("+2")));
	UGameXXKBattleStatusIconWidget* const OverflowIcon = GetStatusIconAt(EffectsWidget, 6);
	TestNotNull(TEXT("the overflow entry is an embedded native icon widget"), OverflowIcon);
	if (OverflowIcon)
	{
		UBorder* const OverflowHitTarget = Cast<UBorder>(OverflowIcon->GetWidgetFromName(TEXT("BattleStatusIconHitTarget")));
		TestNotNull(TEXT("the overflow entry exposes its tooltip hit target"), OverflowHitTarget);
		UBorder* const OverflowTooltipPaper = OverflowHitTarget ? Cast<UBorder>(OverflowHitTarget->GetToolTip()) : nullptr;
		TestNotNull(TEXT("the overflow entry owns a tooltip paper"), OverflowTooltipPaper);
		UTextBlock* const OverflowTooltipText = OverflowTooltipPaper ? Cast<UTextBlock>(OverflowTooltipPaper->GetContent()) : nullptr;
		TestNotNull(TEXT("the overflow entry owns readable tooltip text"), OverflowTooltipText);
		if (OverflowTooltipText)
		{
			const FString OverflowTooltip = OverflowTooltipText->GetText().ToString();
			TestTrue(TEXT("overflow tooltip lists the omitted bleed status"), OverflowTooltip.Contains(TEXT("流血 × 1")));
			TestTrue(TEXT("overflow tooltip lists the omitted mark status"), OverflowTooltip.Contains(TEXT("标记 × 1")));
		}
	}

	float RequiredStatusRowWidth = 0.0f;
	bool bMeasuredAllStatusBadges = true;
	for (int32 IconIndex = 0; IconIndex < EffectsWidget->GetIconCountForTest(); ++IconIndex)
	{
		UGameXXKBattleStatusIconWidget* const IconWidget = GetStatusIconAt(EffectsWidget, IconIndex);
		USizeBox* const IconRoot = IconWidget ? Cast<USizeBox>(IconWidget->GetWidgetFromName(TEXT("BattleStatusIconRoot"))) : nullptr;
		UHorizontalBoxSlot* const IconSlot = IconWidget ? Cast<UHorizontalBoxSlot>(IconWidget->Slot) : nullptr;
		if (!IconRoot || !IconSlot)
		{
			bMeasuredAllStatusBadges = false;
			continue;
		}

		const FMargin Padding = IconSlot->GetPadding();
		RequiredStatusRowWidth += IconRoot->GetWidthOverride() + Padding.Left + Padding.Right;
	}
	TestTrue(TEXT("the visible status badges expose measurable native layout roots"), bMeasuredAllStatusBadges);
	TestTrue(TEXT("six badges plus overflow fit inside the actor's 320-pixel status HUD width"), RequiredStatusRowWidth <= 320.0f);

	using FDeltaApi = TStatusPresentationDeltaApi<FGameXXKBattleAnimationPresentation>;
	using FBoardApi = TBoardStatusPresentationApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("status presentation exposes deterministic Board-owned snapshot deltas"), FDeltaApi::bAvailable);
	TestTrue(TEXT("Board exposes central status-presentation diagnostics"), FBoardApi::bAvailable);
	if (!FDeltaApi::bAvailable || !FBoardApi::bAvailable)
	{
		return false;
	}

	FGameXXKCardBattleRuntime StatusBefore;
	FGameXXKCardCombatUnit BeforeUnit = MakeStatusPresentationUnit(
		TEXT("Enemy.Status"),
		EGameXXKCardTargetSide::Enemy,
		EGameXXKCharacterRole::Invalid,
		0);
	BeforeUnit.Statuses = {
		MakeStatusStack(EGameXXKCardStatus::Burn, 2),
		MakeStatusStack(EGameXXKCardStatus::Mark, 1)};
	StatusBefore.Units = {BeforeUnit};
	FGameXXKCardBattleRuntime StatusAfter = StatusBefore;
	StatusAfter.Units[0].Statuses = {
		MakeStatusStack(EGameXXKCardStatus::Poison, 3),
		MakeStatusStack(EGameXXKCardStatus::Mark, 1)};
	TestEqual(TEXT("only nonzero net status deltas create presentation entries"), FDeltaApi::Count(StatusBefore, StatusAfter), 2);
	TestEqual(TEXT("deterministic status order begins with the lower concrete status enum"),
		FDeltaApi::Status(StatusBefore, StatusAfter, 0),
		EGameXXKCardStatus::Poison);
	TestEqual(TEXT("positive net stacks retain the affected unit"),
		FDeltaApi::UnitId(StatusBefore, StatusAfter, 0),
		FName(TEXT("Enemy.Status")));
	TestEqual(TEXT("positive net stacks retain their signed delta"), FDeltaApi::Delta(StatusBefore, StatusAfter, 0), 3);
	TestEqual(TEXT("positive net stacks request only the generic Buff action"),
		FDeltaApi::Action(StatusBefore, StatusAfter, 0),
		EGameXXKBattleAnimationAction::Buff);
	TestEqual(TEXT("negative net stacks retain their signed delta"), FDeltaApi::Delta(StatusBefore, StatusAfter, 1), -2);
	TestEqual(TEXT("negative net stacks request only the generic Debuff action"),
		FDeltaApi::Action(StatusBefore, StatusAfter, 1),
		EGameXXKBattleAnimationAction::Debuff);
	TestEqual(TEXT("generic Buff uses the one approved 2K sibling atlas"),
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Buff).AssetId,
		FString(TEXT("status_buff_2k_generic")));
	TestEqual(TEXT("generic Debuff uses the one approved 2K sibling atlas"),
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Debuff).AssetId,
		FString(TEXT("status_debuff_2k_generic")));

	UGameInstance* const StatusGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const StatusSubsystem = NewObject<UGameXXKMVPSubsystem>(StatusGameInstance);
	FName StatusCardInstanceId;
	FString StatusError;
	if (!TestTrue(TEXT("positive-status fixture builds a real Board card transaction"),
		BuildPositiveStatusPresentationFixture(StatusSubsystem, StatusCardInstanceId, StatusError)))
	{
		AddError(StatusError);
		return false;
	}
	UGameXXKBattleBoardWidget* const StatusBoard = NewObject<UGameXXKBattleBoardWidget>();
	StatusBoard->SetMVPSubsystem(StatusSubsystem);
	TestTrue(TEXT("positive-status Board initializes"), StatusBoard->Initialize());
	StatusBoard->NativeConstruct();
	TestTrue(TEXT("positive-status Board begins a visual session"), StatusBoard->BeginBattleVisualSession(8201));
	TestTrue(TEXT("positive-status card enters manual targeting"), StatusBoard->ClickCardInHand(StatusCardInstanceId));
	const int32 ContinuationCountBeforeCommit = StatusBoard->GetExecutedBattlePresentationContinuationCountForTest();
	TestTrue(TEXT("positive-status card commits through the Board"), StatusBoard->ConfirmTargetingUnit(TEXT("Enemy.Status")));
	TestEqual(TEXT("buff and debuff deltas never enter the cinematic queue"),
		StatusBoard->GetBattlePresentationQueueCountForTest(),
		0);
	TestTrue(TEXT("status-only hand play starts the shared played-card commit"),
		StatusBoard->IsPlayedCardCommitActiveForTest());
	TestTrue(TEXT("status-only hand play remains presentation locked during commit"),
		StatusBoard->IsBattlePresentationLockedForTest());
	StatusBoard->AdvanceVisualsAtRealTime(0.0);
	TestFalse(TEXT("status-only card resolution never starts a central close-up"), FBoardApi::IsStatus(StatusBoard));
	TestTrue(TEXT("status-only commit remains active at its initial absolute sample"),
		StatusBoard->IsPlayedCardCommitActiveForTest());
	TestTrue(TEXT("status-only continuation stays locked at its initial absolute sample"),
		StatusBoard->IsBattlePresentationLockedForTest());
	StatusBoard->AdvanceVisualsAtRealTime(0.18);
	TestFalse(TEXT("status-only commit completes exactly at the shared 0.18-second boundary"),
		StatusBoard->IsPlayedCardCommitActiveForTest());
	TestFalse(TEXT("status-only continuation unlocks after commit completion"),
		StatusBoard->IsBattlePresentationLockedForTest());
	TestEqual(TEXT("status-only commit executes its continuation exactly once"),
		StatusBoard->GetExecutedBattlePresentationContinuationCountForTest(),
		ContinuationCountBeforeCommit + 1);
	UGameXXKBattleStatusIconWidget* const CentralStatusIcon = StatusBoard->WidgetTree
		? Cast<UGameXXKBattleStatusIconWidget>(StatusBoard->WidgetTree->FindWidget(TEXT("BattleCinematicStatusIcon")))
		: nullptr;
	TestNotNull(TEXT("central status presentation reuses the existing status icon widget"), CentralStatusIcon);
	TestEqual(TEXT("the retired central status badge stays hidden"),
		CentralStatusIcon ? CentralStatusIcon->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	UGameXXKBattleUnitHudWidget* const UpdatedStatusHud = StatusBoard->GetProjectedUnitHudForTest(TEXT("Enemy.Status"));
	UGameXXKBattleUnitStatusEffectsWidget* const UpdatedStatusEffects = UpdatedStatusHud
		? UpdatedStatusHud->GetStatusEffectsWidgetForTest()
		: nullptr;
	TestTrue(TEXT("status changes still update the ordinary unit HUD without a close-up"),
		UpdatedStatusEffects && UpdatedStatusEffects->GetIconCountForTest() > 0);

	return true;
}

#endif
