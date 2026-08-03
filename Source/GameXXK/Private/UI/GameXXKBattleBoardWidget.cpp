#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleAnimationLayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardText.h"
#include "GameXXKEnemyText.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRunDeckRules.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElementTypes.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

namespace
{
	static const FName ResolveBattleVictoryCommand(TEXT("ResolveBattleVictory"));
	static const FName BasicAttackAction(TEXT("BattleBasicAttack"));
	static const FName CraneWingSlashAction(TEXT("BattleCraneWingSlash"));
	static const FName GuiyuanArtAction(TEXT("BattleGuiyuanArt"));
	static const FName DefendAction(TEXT("BattleDefend"));
	static const FName HealingPowderAction(TEXT("BattleHealingPowder"));
	static const FName CardTargetingAction(TEXT("BattleCardTargeting"));
	static constexpr int32 TargetingInkDabCount = 12;
	static constexpr int32 MaximumVisibleHandCards = 5;
	static constexpr int32 MaximumVisibleEnemyIntentCards = 3;
	static constexpr int32 MaximumVisibleRewardCards = 3;
	static constexpr int32 MaximumVisiblePendingCardChoices = 6;
	static constexpr float CommandMenuWidth = 260.0f;
	static constexpr float CommandMenuHeight = 300.0f;
	static constexpr float CommandMenuGap = 18.0f;
	static const FVector2D CommandMenuDefaultOffset(-500.0f, 0.0f);
	static constexpr float PartyCommandLaneMinXRatio = 0.72f;
	static constexpr float PartyCommandLanePreferredXRatio = 0.75f;
	// The viewport's UMG scale is about 0.64 at the supported 1288x770 PIE size.
	// Keep player hand cards readable after that scale without inflating compact
	// enemy-intent and reward cards that share the same PSD frame asset.
	static const FVector2D PlayerHandCardSize(225.0f, 257.0f);
	static const FVector2D PlayerHandRowSize(1170.0f, 259.0f);
	static const FVector2D PartyQiWidgetSize(104.0f, 104.0f);
	static const FVector2D FixedUnitHudWidgetSize(272.0f, 142.0f);
	static const FVector2D BattleHudSafeStageDesignSize(1920.0f, 1080.0f);
	// Legacy test-only resolver constants. Production HUD placement no longer reads them.
	static constexpr float ProjectedUnitHudFootGap = 8.0f;
	static constexpr float ProjectedUnitHudObstacleGap = 10.0f;
	static constexpr int32 ProjectedUnitHudLayerZOrder = -1;
	static constexpr int32 PartyQiWidgetZOrder = 35;
	static constexpr float PartyQiHandSafetyGap = 12.0f;
	static constexpr float PlayerHandSelectedScale = 1.20f;
	static constexpr float PlayerHandSelectedLift = -32.0f;
	static const FVector2D EnemyIntentCardSize(150.0f, 171.0f);
	static const FVector2D EnemyIntentShowcaseCardSize(256.0f, 292.0f);
	static const FVector2D RewardCardSize(113.0f, 129.0f);
	static const FVector2D EnemyIntentRailSize(600.0f, 171.0f);
	static const FVector2D EnemyIntentTooltipSize(460.0f, 256.0f);
	static const FVector2D HandCardDetailPanelSize(420.0f, 252.0f);
	static const FLinearColor BattleStatusInkColor(0.12f, 0.09f, 0.06f, 1.0f);
	static constexpr float BattleStatusFrameMarginRatio = 5.0f / 368.0f;
	static constexpr float EnemyIntentRevealDuration = 0.55f;
	static constexpr float EnemyIntentResolveDuration = 0.18f;
	static constexpr float EnemyIntentSettleDuration = 0.32f;

	struct FGameXXKFixedUnitHudLayout
	{
		FAnchors Anchors;
		FVector2D Alignment = FVector2D(0.5f, 0.0f);
		FVector2D Size = FixedUnitHudWidgetSize;
	};

	bool TryResolveFixedUnitHudLayout(
		const FGameXXKBattleUnitHudView& View,
		FGameXXKFixedUnitHudLayout& OutLayout)
	{
		if (View.SlotNumber < 1 || View.SlotNumber > 3)
		{
			return false;
		}

		FVector2D Anchor = FVector2D::ZeroVector;
		if (View.Side == EGameXXKCardTargetSide::Party)
		{
			switch (View.SlotNumber)
			{
			case 1: Anchor = FVector2D(0.905f, 0.60f); break; // Permanent companion, outer lane.
			case 2: Anchor = FVector2D(0.755f, 0.52f); break; // Hero, middle lane.
			case 3: Anchor = FVector2D(0.605f, 0.44f); break; // Temporary task NPC, inner lane.
			default: return false;
			}
		}
		else if (View.Side == EGameXXKCardTargetSide::Enemy)
		{
			switch (View.SlotNumber)
			{
			case 1: Anchor = FVector2D(0.095f, 0.60f); break;
			case 2: Anchor = FVector2D(0.245f, 0.52f); break;
			case 3: Anchor = FVector2D(0.395f, 0.44f); break;
			default: return false;
			}
		}
		else
		{
			return false;
		}

		OutLayout.Anchors = FAnchors(Anchor.X, Anchor.Y, Anchor.X, Anchor.Y);
		OutLayout.Alignment = FVector2D(0.5f, 0.0f);
		OutLayout.Size = FixedUnitHudWidgetSize;
		return true;
	}

	FVector2D ClampToLocalSize(FVector2D LocalPosition, FVector2D LocalSize)
	{
		if (LocalSize.X > 1.0f)
		{
			LocalPosition.X = FMath::Clamp(LocalPosition.X, 0.0f, LocalSize.X);
		}
		if (LocalSize.Y > 1.0f)
		{
			LocalPosition.Y = FMath::Clamp(LocalPosition.Y, 0.0f, LocalSize.Y);
		}
		return LocalPosition;
	}

	FBox2D ResolveCanvasSlotRect(
		const FAnchors& Anchors,
		const FMargin& Offsets,
		const FVector2D& Alignment,
		const FVector2D& CanvasSize)
	{
		if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
		{
			return FBox2D(EForceInit::ForceInit);
		}

		const FVector2D AnchorMinimum(Anchors.Minimum.X * CanvasSize.X, Anchors.Minimum.Y * CanvasSize.Y);
		const FVector2D AnchorMaximum(Anchors.Maximum.X * CanvasSize.X, Anchors.Maximum.Y * CanvasSize.Y);
		const bool bStretchesHorizontally = !FMath::IsNearlyEqual(Anchors.Minimum.X, Anchors.Maximum.X);
		const bool bStretchesVertically = !FMath::IsNearlyEqual(Anchors.Minimum.Y, Anchors.Maximum.Y);
		FVector2D Position(AnchorMinimum.X + Offsets.Left, AnchorMinimum.Y + Offsets.Top);
		FVector2D Size(
			bStretchesHorizontally ? AnchorMaximum.X - Offsets.Right - Position.X : Offsets.Right,
			bStretchesVertically ? AnchorMaximum.Y - Offsets.Bottom - Position.Y : Offsets.Bottom);
		if (Size.X < 0.0f || Size.Y < 0.0f)
		{
			return FBox2D(EForceInit::ForceInit);
		}

		Position -= Alignment * Size;
		return FBox2D(Position, Position + Size);
	}

	bool DoRectsOverlap(const FBox2D& First, const FBox2D& Second)
	{
		return First.bIsValid
			&& Second.bIsValid
			&& First.Min.X < Second.Max.X
			&& First.Max.X > Second.Min.X
			&& First.Min.Y < Second.Max.Y
			&& First.Max.Y > Second.Min.Y;
	}

	bool IsVisibleObstacleWidget(const UWidget* const Widget)
	{
		return Widget
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	}
	static constexpr const TCHAR* InkButtonTexturePath = TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase");
	static constexpr const TCHAR* BattleStatusWindowFrameTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame");
	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057.T_CardFrame_PSD057");
	static constexpr const TCHAR* HeroCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero");
	static constexpr const TCHAR* TusiChiefCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief");
	static constexpr const TCHAR* SongJinBaoCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_SongJinBao.T_CardPortrait_Npc_SongJinBao");
	static constexpr const TCHAR* YueBaiCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_YueBai.T_CardPortrait_Npc_YueBai");
	static constexpr const TCHAR* ZhouGuangZuCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_ZhouGuangZu.T_CardPortrait_Npc_ZhouGuangZu");
	static constexpr const TCHAR* JinGuiCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_JinGui.T_CardPortrait_Npc_JinGui");
	static constexpr const TCHAR* QiongMeiErCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_QiongMeiEr.T_CardPortrait_Npc_QiongMeiEr");
	static constexpr const TCHAR* BladeCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade.T_CardPortrait_Role_Blade");
	static constexpr const TCHAR* GuardCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Guard.T_CardPortrait_Role_Guard");
	static constexpr const TCHAR* HealerCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Healer.T_CardPortrait_Role_Healer");
	static constexpr const TCHAR* HunterCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Hunter.T_CardPortrait_Role_Hunter");
	static constexpr const TCHAR* SorcererCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Sorcerer.T_CardPortrait_Role_Sorcerer");
	static constexpr const TCHAR* FormationMasterCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_FormationMaster.T_CardPortrait_Role_FormationMaster");
	static constexpr const TCHAR* RouteGeneralCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_General.T_CardPortrait_Route_General");
	static constexpr const TCHAR* RouteTerrainCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Terrain.T_CardPortrait_Route_Terrain");
	static constexpr const TCHAR* RouteRareCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Rare.T_CardPortrait_Route_Rare");
	static constexpr const TCHAR* RouteBossCardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Boss.T_CardPortrait_Route_Boss");
	static constexpr const TCHAR* TargetingArrowHeadTexturePath = TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead.T_BattleTargetArrowHead");

	static FString BuildCardTargetHint(const FGameXXKCardPlayPreview& Preview)
	{
		if (!Preview.bCanPlay)
		{
			return Preview.FailureReason.IsEmpty() ? TEXT("当前不可用") : Preview.FailureReason;
		}
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			return TEXT("选择目标");
		}
		switch (Preview.TargetRequest.Presentation)
		{
		case EGameXXKCardTargetPresentation::Self:
			return TEXT("自身施放");
		case EGameXXKCardTargetPresentation::Group:
			return TEXT("群体施放");
		case EGameXXKCardTargetPresentation::AutomaticUnit:
			return TEXT("自动目标");
		case EGameXXKCardTargetPresentation::NoSelection:
			return TEXT("直接施放");
		default:
			return TEXT("自动结算");
		}
	}

	static FLinearColor ResolveCardInfoStripTint(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero)
		{
			return FLinearColor(0.945f, 0.894f, 0.800f, 1.0f);
		}
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			return FLinearColor(0.145f, 0.137f, 0.129f, 1.0f);
		}
		switch (Definition.Role)
		{
		case EGameXXKCharacterRole::Blade:
			return FLinearColor(0.714f, 0.282f, 0.247f, 1.0f);
		case EGameXXKCharacterRole::Guard:
			return FLinearColor(0.145f, 0.302f, 0.302f, 1.0f);
		case EGameXXKCharacterRole::Healer:
			return FLinearColor(0.353f, 0.576f, 0.427f, 1.0f);
		case EGameXXKCharacterRole::Hunter:
			return FLinearColor(0.604f, 0.408f, 0.200f, 1.0f);
		case EGameXXKCharacterRole::Sorcerer:
			return FLinearColor(0.251f, 0.318f, 0.553f, 1.0f);
		case EGameXXKCharacterRole::FormationMaster:
			return FLinearColor(0.502f, 0.384f, 0.475f, 1.0f);
		default:
			return FLinearColor(0.882f, 0.827f, 0.722f, 1.0f);
		}
	}

	static FLinearColor ResolveCardInfoInkTint(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::Route)
		{
			return FLinearColor(0.137f, 0.118f, 0.098f, 1.0f);
		}
		// Task NPC cards use the locked black + light-gray pair rather than a
		// profession accent: black information strip with a pale stone nameplate.
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			return FLinearColor(0.722f, 0.706f, 0.671f, 1.0f);
		}
		return FLinearColor(0.953f, 0.941f, 0.914f, 1.0f);
	}

	static FSlateBrush BuildTextureBrush(UTexture2D* Texture, const FVector2D& ImageSize, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = ImageSize;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	static FSlateBrush BuildBoxTextureBrush(UTexture2D* Texture, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FSlateBrush Brush = BuildTextureBrush(Texture, ImageSize, FLinearColor::White);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	static FString BuildTargetingInkDabTexturePath(int32 DabIndex)
	{
		return FString::Printf(
			TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetInkDab_%02d.T_BattleTargetInkDab_%02d"),
			DabIndex,
			DabIndex);
	}

	static FVector2D QuadraticBezierPoint(const FVector2D& Start, const FVector2D& Control, const FVector2D& End, float T)
	{
		const float OneMinusT = 1.0f - T;
		return OneMinusT * OneMinusT * Start + 2.0f * OneMinusT * T * Control + T * T * End;
	}

	FString DescribeEnemyIntentStatus(const EGameXXKCardStatus Status)
	{
		switch (Status)
		{
		case EGameXXKCardStatus::Momentum: return TEXT("势");
		case EGameXXKCardStatus::Agility: return TEXT("灵动");
		case EGameXXKCardStatus::Vulnerability: return TEXT("破绽");
		case EGameXXKCardStatus::Bleed: return TEXT("流血");
		case EGameXXKCardStatus::Poison: return TEXT("中毒");
		case EGameXXKCardStatus::Burn: return TEXT("灼烧");
		case EGameXXKCardStatus::Mark: return TEXT("标记");
		case EGameXXKCardStatus::Guard: return TEXT("守护");
		case EGameXXKCardStatus::DamageOverTime: return TEXT("持续伤害");
		case EGameXXKCardStatus::CannotReceiveVulnerability: return TEXT("免疫破绽");
		case EGameXXKCardStatus::NextAttackBonus: return TEXT("下次攻击强化");
		case EGameXXKCardStatus::NextAttackAppliesVulnerability: return TEXT("下次攻击附加破绽");
		case EGameXXKCardStatus::NextHealingBonus: return TEXT("下次治疗强化");
		case EGameXXKCardStatus::TerrainBonusDouble: return TEXT("地形加成翻倍");
		case EGameXXKCardStatus::NextTerrainCardFree: return TEXT("下一张地形牌免费");
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction: return TEXT("下一张地形牌减气");
		case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack: return TEXT("转移单体敌袭");
		case EGameXXKCardStatus::TerrainBonusDoubleThisRound: return TEXT("本回合地形加成翻倍");
		default: return TEXT("无效状态");
		}
	}

	FString BuildEnemyIntentStatusSummary(const FGameXXKCardEnemyIntent& Intent)
	{
		if (Intent.OnHitStatuses.IsEmpty())
		{
			return TEXT("无附加状态");
		}

		TArray<FString> StatusLines;
		for (const FGameXXKCardStatusStack& Status : Intent.OnHitStatuses)
		{
			StatusLines.Add(FString::Printf(TEXT("%s %d层"), *DescribeEnemyIntentStatus(Status.Status), Status.Stacks));
		}
		return FString::Join(StatusLines, TEXT("、"));
	}

	FString ResolveEnemyIntentSourceName(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
	{
		const FGameXXKBattleRuntimeUnit* Source = State.ActiveBattleEnemies.FindByPredicate([&Intent](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == Intent.SourceUnitId;
		});
		return Source && !Source->DisplayName.IsEmpty() ? Source->DisplayName.ToString() : Intent.SourceUnitId.ToString();
	}

	FString ResolveEnemyIntentSourceSlotLabel(const FGameXXKCardEnemyIntent& Intent)
	{
		return FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Enemy, Intent.SourceSlotNumber);
	}

	FString ResolveEnemyIntentTargetSlotLabel(const FGameXXKCardEnemyIntent& Intent)
	{
		return FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Party, Intent.TargetSlotNumber);
	}

	FString ResolveEnemyIntentSkillName(const FGameXXKCardEnemyIntent& Intent)
	{
		return Intent.CardDisplayName.IsEmpty() ? TEXT("攻击") : Intent.CardDisplayName;
	}

	FString BuildEnemyIntentCardBody(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
	{
		return FGameXXKEnemyText::FormatIntentCard(State, Intent);
	}

	FString BuildEnemyIntentTooltip(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent)
	{
		return FGameXXKEnemyText::FormatIntentTooltip(State, Intent);
	}

	FGameXXKCardTooltipContext BuildHandTooltipContext(const FGameXXKCardPlayPreview& Preview)
	{
		FGameXXKCardTooltipContext Context;
		if (!Preview.bCanPlay)
		{
			Context.UnavailableReason = Preview.FailureReason.IsEmpty()
				? TEXT("不满足出牌条件")
				: Preview.FailureReason;
			return Context;
		}
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			Context.InteractionResult = TEXT("点击后选择高亮合法目标。");
		}
		else if (Preview.TargetRequest.bRequiresRandomResolution)
		{
			Context.InteractionResult = TEXT("点击后自动随机结算。");
		}
		else
		{
			Context.InteractionResult = TEXT("点击后立即施放。");
		}
		return Context;
	}
}

void UGameXXKRouteRewardReplacementButton::Configure(UGameXXKBattleBoardWidget* InOwner, FName InEntryId)
{
	Owner = InOwner;
	EntryId = InEntryId;
	OnClicked.RemoveDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleClicked);
	OnHovered.RemoveDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleHovered);
	OnHovered.AddDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleHovered);
	OnUnhovered.RemoveDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleUnhovered);
	OnUnhovered.AddDynamic(this, &UGameXXKRouteRewardReplacementButton::HandleUnhovered);
}

void UGameXXKRouteRewardReplacementButton::HandleClicked()
{
	if (Owner && !EntryId.IsNone())
	{
		Owner->SelectRouteRewardReplacementEntry(EntryId);
	}
}

void UGameXXKRouteRewardReplacementButton::HandleHovered()
{
	if (Owner && !EntryId.IsNone())
	{
		Owner->HandleRouteRewardReplacementEntryHoverChanged(EntryId, true);
	}
}

void UGameXXKRouteRewardReplacementButton::HandleUnhovered()
{
	if (Owner && !EntryId.IsNone())
	{
		Owner->HandleRouteRewardReplacementEntryHoverChanged(EntryId, false);
	}
}

void UGameXXKPendingChoiceCardButton::Configure(
	UGameXXKBattleBoardWidget* InOwner,
	FName InCandidateInstanceId,
	EGameXXKCardPendingChoiceKind InChoiceKind)
{
	Owner = InOwner;
	CandidateInstanceId = InCandidateInstanceId;
	ChoiceKind = InChoiceKind;
	OnClicked.RemoveDynamic(this, &UGameXXKPendingChoiceCardButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKPendingChoiceCardButton::HandleClicked);
	OnHovered.RemoveDynamic(this, &UGameXXKPendingChoiceCardButton::HandleHovered);
	OnHovered.AddDynamic(this, &UGameXXKPendingChoiceCardButton::HandleHovered);
	OnUnhovered.RemoveDynamic(this, &UGameXXKPendingChoiceCardButton::HandleUnhovered);
	OnUnhovered.AddDynamic(this, &UGameXXKPendingChoiceCardButton::HandleUnhovered);
}

void UGameXXKPendingChoiceCardButton::HandleClicked()
{
	if (Owner && !CandidateInstanceId.IsNone())
	{
		if (ChoiceKind == EGameXXKCardPendingChoiceKind::InsightChooseToHand)
		{
			Owner->SubmitPendingInsightChoice(CandidateInstanceId);
		}
		else if (ChoiceKind == EGameXXKCardPendingChoiceKind::ForcedDiscard)
		{
			Owner->SubmitPendingForcedDiscard(CandidateInstanceId);
		}
	}
}

void UGameXXKPendingChoiceCardButton::HandleHovered()
{
	if (Owner && !CandidateInstanceId.IsNone())
	{
		Owner->HandlePendingChoiceCardHoverChanged(CandidateInstanceId, ChoiceKind, true);
	}
}

void UGameXXKPendingChoiceCardButton::HandleUnhovered()
{
	if (Owner && !CandidateInstanceId.IsNone())
	{
		Owner->HandlePendingChoiceCardHoverChanged(CandidateInstanceId, ChoiceKind, false);
	}
}

TSharedRef<SWidget> UGameXXKBattleBoardWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKBattleBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvanceHandCardHoverMotion(InDeltaTime);
	AdvanceEnemyIntentPresentation(InDeltaTime);
	if (PartyQiWidget && RootCanvas)
	{
		const FVector2D CurrentCanvasSize = RootCanvas->GetCachedGeometry().GetLocalSize();
		if (!CurrentCanvasSize.Equals(LastPartyQiCanvasSize, 0.5f))
		{
			RefreshPartyQiWidget();
		}
	}
}

void UGameXXKBattleBoardWidget::QueueCombatAnimation(
	const FName AttackerUnitId,
	const bool bAttackerEnemy,
	const FName TargetUnitId,
	const bool bTargetEnemy,
	const bool bTargetDefeated)
{
	if (BattleAnimationLayer)
	{
		BattleAnimationLayer->QueueCombatSequence(
			AttackerUnitId,
			bAttackerEnemy,
			TargetUnitId,
			bTargetEnemy,
			bTargetDefeated);
	}
}

UGameXXKBattleAnimationLayerWidget* UGameXXKBattleBoardWidget::GetBattleAnimationLayerForTest() const
{
	return BattleAnimationLayer;
}

FReply UGameXXKBattleBoardWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && CancelBattleTargeting())
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UGameXXKBattleBoardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && CancelBattleTargeting())
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

int32 UGameXXKBattleBoardWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 MaxLayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!IsTargetingBattleActionForTest() || !TargetingArrowHeadTexture || TargetingInkDabTextures.IsEmpty())
	{
		return MaxLayerId;
	}

	const FVector2D Start = TargetingSourcePosition;
	const FVector2D End = TargetingPointerPosition;
	const FVector2D Delta = End - Start;
	const float Distance = Delta.Size();
	if (Distance < 8.0f)
	{
		return MaxLayerId;
	}

	const FVector2D Direction = Delta / Distance;
	const FVector2D Normal(-Direction.Y, Direction.X);
	const float BowAmount = FMath::Clamp(Distance * 0.16f, 22.0f, 96.0f);
	const FVector2D Control = (Start + End) * 0.5f + Normal * BowAmount;
	const int32 SegmentCount = FMath::Clamp(FMath::RoundToInt(Distance / 58.0f), 5, 24);
	const int32 DabLayer = MaxLayerId + 1;

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		UTexture2D* DabTexture = TargetingInkDabTextures[SegmentIndex % TargetingInkDabTextures.Num()].Get();
		if (!DabTexture)
		{
			continue;
		}

		const float T = (static_cast<float>(SegmentIndex) + 0.5f) / static_cast<float>(SegmentCount);
		const FVector2D Point = QuadraticBezierPoint(Start, Control, End, T);
		const float Size = FMath::Lerp(18.0f, 30.0f, FMath::Sin(T * PI));
		FSlateBrush DabBrush = BuildTextureBrush(DabTexture, FVector2D(Size, Size), FLinearColor(1.0f, 1.0f, 1.0f, 0.88f));
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			DabLayer,
			AllottedGeometry.ToPaintGeometry(FVector2D(Size, Size), FSlateLayoutTransform(Point - FVector2D(Size * 0.5f, Size * 0.5f))),
			&DabBrush,
			ESlateDrawEffect::None,
			FLinearColor::White);
	}

	const FVector2D ArrowSize(74.0f, 56.0f);
	const FVector2D ArrowPosition = End - ArrowSize * 0.5f;
	FSlateBrush ArrowBrush = BuildTextureBrush(TargetingArrowHeadTexture.Get(), ArrowSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.96f));
	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		DabLayer + 1,
		AllottedGeometry.ToPaintGeometry(ArrowSize, FSlateLayoutTransform(ArrowPosition)),
		&ArrowBrush,
		ESlateDrawEffect::None,
		FMath::Atan2(Direction.Y, Direction.X),
		TOptional<FVector2f>(),
		FSlateDrawElement::RelativeToElement,
		FLinearColor::White);

	return DabLayer + 1;
}

void UGameXXKBattleBoardWidget::RefreshFromState()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bInBattle = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle;
	const bool bFixtureReadOnly = Subsystem && Subsystem->IsBattleHudFixtureActiveForTest();
	const bool bHasActiveCardControls = bInBattle
		&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle;
	if (!bHasActiveCardControls)
	{
		ClearCardTooltipHoverState();
	}
	if (!bInBattle)
	{
		ClearCardTargetingState();
		if (BattleAnimationLayer)
		{
			BattleAnimationLayer->ResetPresentation();
		}
		EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;
		EnemyIntentPresentationElapsed = 0.0f;
		ActiveEnemyIntentPresentationIndex = INDEX_NONE;
		HoveredEnemyIntentSlot = INDEX_NONE;
		bEnemyIntentCompletionRecoveryPending = false;
		InteractionMode = EGameXXKBattleInteractionMode::Hidden;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
		TargetingSourcePosition = FVector2D::ZeroVector;
		PendingRewardCardIds.Reset();
	}
	else if (bFixtureReadOnly)
	{
		// A fixture can be applied between ticks while this board still carries a
		// target arrow or an enemy intent showcase from the raw battle.  Those are
		// local presentation states only and must never become a mutation path into
		// the raw runtime hidden behind the copied fixture view.
		ClearCardTargetingState();
		ResetEnemyIntentPresentationState();
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
		TargetingSourcePosition = FVector2D::ZeroVector;
		TargetingPointerPosition = FVector2D::ZeroVector;
	}
	else if (InteractionMode == EGameXXKBattleInteractionMode::Hidden)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
	}
	else if (Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		&& InteractionMode != EGameXXKBattleInteractionMode::TargetingCard)
	{
		// Once the shared card runtime owns the battle, old fixed-action buttons
		// must not reappear through a party-actor click.  End Turn and card input
		// remain the only player operations.
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
	}
	else if (IsCardTargetingActive() && !RefreshPendingCardTargetingPreview())
	{
		ClearCardTargetingState();
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
	}
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bEnemyCardPhase = bInBattle
		&& RuntimeState
		&& RuntimeState->CardRun.bHasActiveCardBattle
		&& RuntimeState->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy;
	if (!bEnemyCardPhase)
	{
		EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;
		EnemyIntentPresentationElapsed = 0.0f;
		ActiveEnemyIntentPresentationIndex = INDEX_NONE;
		HoveredEnemyIntentSlot = INDEX_NONE;
		bEnemyIntentCompletionRecoveryPending = false;
	}
	else if (EnemyIntentPresentationState == EGameXXKEnemyIntentPresentationState::None)
	{
		const FGameXXKCardRunState& Run = RuntimeState->CardRun;
		if (Run.EnemyIntents.IsValidIndex(Run.NextEnemyIntentIndex))
		{
			// A freshly reconstructed board has no local presentation phase, but the
			// runtime still owns a pending intent. Resume from that persisted index.
			BeginEnemyIntentPresentation();
		}
		else if (Run.NextEnemyIntentIndex >= Run.EnemyIntents.Num())
		{
			// All saved intents were consumed before this board was reconstructed.
			// Keep the enemy phase visibly recoverable until its completion sync succeeds.
			EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Settle;
			EnemyIntentPresentationElapsed = 0.0f;
			ActiveEnemyIntentPresentationIndex = INDEX_NONE;
			bEnemyIntentCompletionRecoveryPending = true;
		}
	}

	BuildProgrammaticLayout();
	RefreshProgrammaticLayout();
	RefreshProjectedUnitHuds();

	SetVisibility(bInBattle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UGameXXKBattleBoardWidget::IsBattleHudFixtureReadOnly() const
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->IsBattleHudFixtureActiveForTest();
}

bool UGameXXKBattleBoardWidget::RejectBattleHudFixtureMutation()
{
	if (!IsBattleHudFixtureReadOnly())
	{
		return false;
	}

	ClearCardTargetingState();
	ResetEnemyIntentPresentationState();
	InteractionMode = EGameXXKBattleInteractionMode::Idle;
	SelectedPartyIndex = INDEX_NONE;
	TargetingActionName = NAME_None;
	TargetingSourcePosition = FVector2D::ZeroVector;
	TargetingPointerPosition = FVector2D::ZeroVector;
	LastCardInteractionError = TEXT("开发用战斗 HUD 预览为只读，不能执行战斗操作。");
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKBattleBoardWidget::ExecutePrimaryEnemyAction()
{
	return false;
}

bool UGameXXKBattleBoardWidget::ExecuteBasicAttackAction()
{
	return BeginTargetingBattleAction(BasicAttackAction);
}

bool UGameXXKBattleBoardWidget::ExecuteCraneWingSlashAction()
{
	return BeginTargetingBattleAction(CraneWingSlashAction);
}

bool UGameXXKBattleBoardWidget::ExecuteGuiyuanArtAction()
{
	return ExecuteBattleAction(GuiyuanArtAction);
}

bool UGameXXKBattleBoardWidget::ExecuteDefendAction()
{
	return ExecuteBattleAction(DefendAction);
}

bool UGameXXKBattleBoardWidget::ExecuteHealingPowderAction()
{
	return ExecuteBattleAction(HealingPowderAction);
}

bool UGameXXKBattleBoardWidget::OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || State->Screen != EGameXXKScreen::Battle || !State->ActiveBattleParty.IsValidIndex(PartyIndex))
	{
		return false;
	}
	if (State->CardRun.bHasActiveCardBattle)
	{
		// Card clicks are the battle action source in all card-runtime encounters.
		return false;
	}

	const FGameXXKBattleRuntimeUnit& Unit = State->ActiveBattleParty[PartyIndex];
	if (Unit.bEnemy || Unit.bDefeated || Unit.HP <= 0)
	{
		return false;
	}

	SelectedPartyIndex = PartyIndex;
	const FVector2D CommandSourcePosition = ResolveCommandSourcePosition(
		PartyIndex,
		MenuScreenPosition,
		UnitScreenPosition,
		GetCachedGeometry().GetLocalSize());
	CommandMenuAnchor = ResolveCommandMenuAnchor(CommandSourcePosition);
	SelectedPartyScreenPosition = CommandSourcePosition;
	TargetingSourcePosition = CommandSourcePosition;
	RegisterBattleUnitScreenPosition(Unit.Id, CommandSourcePosition);
	TargetingPointerPosition = CommandSourcePosition;
	TargetingActionName = NAME_None;
	InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKBattleBoardWidget::ToggleCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	if (InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen && SelectedPartyIndex == PartyIndex)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	return OpenCommandMenuForPartyUnit(PartyIndex, MenuScreenPosition, UnitScreenPosition);
}

void UGameXXKBattleBoardWidget::UpdateTargetingPointer(FVector2D ScreenPosition)
{
	if (IsTargetingBattleActionForTest() && !TargetingPointerPosition.Equals(ScreenPosition, 0.5f))
	{
		TargetingPointerPosition = ScreenPosition;
		InvalidateLayoutAndVolatility();
	}
}

void UGameXXKBattleBoardWidget::UpdateTargetingPointerFromSlateAbsolutePosition(FVector2D ScreenPosition)
{
	UpdateTargetingPointer(ResolveSlateAbsolutePositionToLocal(ScreenPosition));
}

bool UGameXXKBattleBoardWidget::ConfirmTargetingEnemy(int32 EnemyIndex)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	if (IsCardTargetingActive())
	{
		const UGameXXKMVPSubsystem* CardSubsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* CardState = CardSubsystem ? &CardSubsystem->GetRuntimeState() : nullptr;
		if (!CardState || !CardState->ActiveBattleEnemies.IsValidIndex(EnemyIndex))
		{
			return false;
		}
		return ConfirmTargetingUnit(CardState->ActiveBattleEnemies[EnemyIndex].Id);
	}

	if (!IsTargetingBattleActionForTest() || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (!State.ActiveBattleEnemies.IsValidIndex(EnemyIndex))
	{
		return false;
	}
	const FGameXXKBattleRuntimeUnit& Enemy = State.ActiveBattleEnemies[EnemyIndex];
	if (!Enemy.bEnemy || Enemy.bDefeated || Enemy.HP <= 0)
	{
		return false;
	}

	const FName ActionToExecute = TargetingActionName;
	const int32 PartyIndex = SelectedPartyIndex;
	bool bExecuted = false;
	if (ActionToExecute == BasicAttackAction)
	{
		bExecuted = Subsystem->ExecuteBattleBasicAttack(PartyIndex, EnemyIndex);
	}
	else if (ActionToExecute == CraneWingSlashAction)
	{
		bExecuted = Subsystem->ExecuteBattleCraneWingSlash(PartyIndex, EnemyIndex);
	}

	if (bExecuted)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		TargetingActionName = NAME_None;
		SelectedPartyIndex = INDEX_NONE;
		if (Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::ClickCardInHand(FName CardInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有可操作的卡牌战斗。");
		return false;
	}
	if (IsCardTargetingActive())
	{
		LastCardInteractionError = TEXT("请先取消当前的目标选择。");
		return false;
	}

	const FGameXXKCardBattleRuntime& Runtime = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
	if (Runtime.Phase != EGameXXKCardBattlePhase::Player)
	{
		LastCardInteractionError = TEXT("敌方意图正在结算，请等待玩家回合。");
		RefreshProgrammaticLayout();
		return false;
	}
	if (!Runtime.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == CardInstanceId;
	}))
	{
		LastCardInteractionError = TEXT("该卡已不在当前手牌中。");
		RefreshProgrammaticLayout();
		return false;
	}

	FGameXXKCardPlayPreview Preview;
	FString Error;
	if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(Subsystem->GetRuntimeState(), CardInstanceId, Preview, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	if (!Preview.bCanPlay)
	{
		LastCardInteractionError = Preview.FailureReason;
		RefreshProgrammaticLayout();
		return false;
	}
	if (Preview.TargetRequest.bRequiresManualSelection)
	{
		return BeginCardTargeting(Preview);
	}
	return ResolveAutomaticCardPlay(CardInstanceId);
}

bool UGameXXKBattleBoardWidget::ConfirmTargetingUnit(FName UnitId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	if (!IsCardTargetingActive() || UnitId.IsNone() || !LegalCardTargetUnitIds.Contains(UnitId))
	{
		LastCardInteractionError = TEXT("该单位不是此牌当前可选的目标。");
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		LastCardInteractionError = TEXT("战斗状态已发生变化，请重新选择卡牌。");
		ClearCardTargetingState();
		RefreshProgrammaticLayout();
		return false;
	}

	FGameXXKCardPlayResult Result;
	FString Error;
	if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
		Subsystem->GetMutableRuntimeState(),
		PendingCardPreview.CardInstanceId,
		UnitId,
		Result,
		&Error))
	{
		LastCardInteractionError = Error;
		RefreshPendingCardTargetingPreview();
		RefreshProgrammaticLayout();
		return false;
	}

	ClearCardTargetingState();
	const bool bRefreshed = ResolveAndRefreshCardBattleAfterMutation();
	if (bRefreshed)
	{
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->RefreshBattleSceneAfterCardMutation(Result.OwnerUnitId, Result.DamageResults);
		}
	}
	return bRefreshed;
}

bool UGameXXKBattleBoardWidget::EndCardPlayerPhase()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有可结束的卡牌回合。");
		return false;
	}
	if (IsCardTargetingActive())
	{
		LastCardInteractionError = TEXT("请先取消或完成当前的目标选择。");
		return false;
	}

	TArray<FGameXXKCardDamageResult> DamageResults;
	FString Error;
	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	if (!FGameXXKCardBattleAdapter::EndPlayerCardPhase(MutableState, DamageResults, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	LastCardInteractionError.Reset();
	if (MutableState.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		BeginEnemyIntentPresentation();
	}
	const bool bRefreshed = ResolveAndRefreshCardBattleAfterMutation();
	if (bRefreshed && MutableState.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		// The enemy intent Reveal/Resolve/Settle presentation starts immediately
		// after ending the player turn.  Lock the already-visible controls here as
		// well as in RefreshHandCards so a deferred player-flow refresh can never
		// leave a one-frame window where the old hand remains clickable.
		for (UButton* CardButton : HandCardButtons)
		{
			if (CardButton)
			{
				CardButton->SetIsEnabled(false);
			}
		}
		if (EndTurnButton)
		{
			EndTurnButton->SetIsEnabled(false);
		}
	}
	return bRefreshed;
}

bool UGameXXKBattleBoardWidget::SubmitPendingInsightChoice(FName PickedInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有可选择的洞察牌。");
		return false;
	}

	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPendingCardChoice& PendingChoice = MutableState.CardRun.ActiveBattle.Deck.PendingChoice;
	if (PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand)
	{
		LastCardInteractionError = TEXT("当前没有可选择的洞察牌。");
		RefreshProgrammaticLayout();
		return false;
	}
	if (PickedInstanceId.IsNone() || !PendingChoice.InsightTopOrder.Contains(PickedInstanceId))
	{
		LastCardInteractionError = TEXT("所选卡牌不在当前洞察列表中。");
		RefreshProgrammaticLayout();
		return false;
	}

	TArray<FName> RemainingTopOrder;
	RemainingTopOrder.Reserve(PendingChoice.InsightTopOrder.Num() - 1);
	for (const FName CandidateInstanceId : PendingChoice.InsightTopOrder)
	{
		if (CandidateInstanceId != PickedInstanceId)
		{
			RemainingTopOrder.Add(CandidateInstanceId);
		}
	}

	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitInsightChoice(MutableState, PickedInstanceId, RemainingTopOrder, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return ResolveAndRefreshCardBattleAfterMutation();
}

bool UGameXXKBattleBoardWidget::SubmitPendingForcedDiscard(FName DiscardedInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有需要弃置的手牌。");
		return false;
	}

	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPendingCardChoice& PendingChoice = MutableState.CardRun.ActiveBattle.Deck.PendingChoice;
	if (PendingChoice.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard)
	{
		LastCardInteractionError = TEXT("当前没有需要弃置的手牌。");
		RefreshProgrammaticLayout();
		return false;
	}
	const bool bIsCandidate = PendingChoice.Candidates.ContainsByPredicate([DiscardedInstanceId](const FGameXXKCardInstance& Candidate)
	{
		return Candidate.InstanceId == DiscardedInstanceId;
	});
	if (DiscardedInstanceId.IsNone() || !bIsCandidate)
	{
		LastCardInteractionError = TEXT("所选卡牌不在当前弃牌列表中。");
		RefreshProgrammaticLayout();
		return false;
	}

	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitForcedDiscard(MutableState, {DiscardedInstanceId}, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return ResolveAndRefreshCardBattleAfterMutation();
}

bool UGameXXKBattleBoardWidget::CancelPendingInsightChoice()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有可取消的洞察。");
		return false;
	}

	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPendingCardChoice& PendingChoice = MutableState.CardRun.ActiveBattle.Deck.PendingChoice;
	if (PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand || !PendingChoice.bCanCancel)
	{
		LastCardInteractionError = TEXT("当前洞察不可取消。");
		RefreshProgrammaticLayout();
		return false;
	}

	FString Error;
	if (!FGameXXKCardBattleAdapter::CancelInsight(MutableState, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return ResolveAndRefreshCardBattleAfterMutation();
}

void UGameXXKBattleBoardWidget::RegisterBattleUnitScreenPosition(FName UnitId, FVector2D ScreenPosition)
{
	if (!UnitId.IsNone())
	{
		RegisteredBattleUnitScreenPositions.Add(UnitId, ScreenPosition);
		if (IsCardTargetingActive()
			&& PendingCardPreview.OwnerUnitId == UnitId
			&& !TargetingSourcePosition.Equals(ScreenPosition, 0.5f))
		{
			TargetingSourcePosition = ScreenPosition;
			InvalidateLayoutAndVolatility();
		}
	}
}

void UGameXXKBattleBoardWidget::ClearBattleUnitScreenPositions()
{
	// Card-targeting centers are transient actor projections for the arrow bridge.
	// Resource HUDs use their fixed P-slot layout and are never stored here.
	RegisteredBattleUnitScreenPositions.Reset();
}

void UGameXXKBattleBoardWidget::RegisterBattleUnitHudScreenPosition(const FName UnitId, const FVector2D ScreenPosition)
{
	// Deliberately retained as a Blueprint-compatible no-op during the HUD migration.
	// Actor projections still drive the targeting arrow, but never resource HUD plates.
	(void)UnitId;
	(void)ScreenPosition;
}

void UGameXXKBattleBoardWidget::RefreshProjectedUnitHuds()
{
	if (!BattleProjectedUnitHudLayer || !WidgetTree)
	{
		return;
	}

	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bHasCardBattle = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle;
	TSet<FName> LivingUnitIds;
	if (bHasCardBattle)
	{
		for (const FGameXXKCardCombatUnit& Unit : State->CardRun.ActiveBattle.Units)
		{
			if (Unit.UnitId.IsNone() || !Unit.bLiving)
			{
				continue;
			}

			FGameXXKBattleUnitHudView View;
			if (!FGameXXKBattlePresentation::BuildUnitHudView(
				State->CardRun.ActiveBattle,
				Unit.UnitId,
				ResolveProjectedUnitHudDisplayName(Unit.UnitId),
				View))
			{
				continue;
			}
			FGameXXKFixedUnitHudLayout FixedLayout;
			if (!TryResolveFixedUnitHudLayout(View, FixedLayout))
			{
				continue;
			}

			LivingUnitIds.Add(Unit.UnitId);
			UGameXXKBattleUnitHudWidget* Hud = ProjectedUnitHuds.FindRef(Unit.UnitId);
			const bool bHasValidProjectedOwnership = Hud
				&& Hud->GetParent() == BattleProjectedUnitHudLayer
				&& Cast<UCanvasPanelSlot>(Hud->Slot);
			if (!bHasValidProjectedOwnership)
			{
				if (Hud)
				{
					Hud->RemoveFromParent();
				}
				ProjectedUnitHuds.Remove(Unit.UnitId);
				Hud = nullptr;
			}
			if (!Hud)
			{
				Hud = WidgetTree->ConstructWidget<UGameXXKBattleUnitHudWidget>(
					UGameXXKBattleUnitHudWidget::StaticClass(),
					*FString::Printf(TEXT("BattleProjectedUnitHud_%s"), *Unit.UnitId.ToString()));
				if (!Hud || !Hud->PrepareForBoardEmbedding())
				{
					continue;
				}
				UCanvasPanelSlot* const HudSlot = BattleProjectedUnitHudLayer->AddChildToCanvas(Hud);
				if (!HudSlot)
				{
					Hud->RemoveFromParent();
					continue;
				}
				HudSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
				HudSlot->SetAlignment(FixedLayout.Alignment);
				HudSlot->SetOffsets(FMargin(0.0f, 0.0f, FixedLayout.Size.X, FixedLayout.Size.Y));
				HudSlot->SetZOrder(0);
				ProjectedUnitHuds.Add(Unit.UnitId, Hud);
			}
			Hud->SetUnitView(View);
			if (UCanvasPanelSlot* const HudSlot = Cast<UCanvasPanelSlot>(Hud->Slot))
			{
				HudSlot->SetAnchors(FixedLayout.Anchors);
				HudSlot->SetAlignment(FixedLayout.Alignment);
				HudSlot->SetOffsets(FMargin(0.0f, 0.0f, FixedLayout.Size.X, FixedLayout.Size.Y));
			}
			Hud->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}

	for (auto It = ProjectedUnitHuds.CreateIterator(); It; ++It)
	{
		if (LivingUnitIds.Contains(It.Key()))
		{
			continue;
		}
		if (UGameXXKBattleUnitHudWidget* const Hud = It.Value())
		{
			Hud->RemoveFromParent();
		}
		It.RemoveCurrent();
	}
}

void UGameXXKBattleBoardWidget::RefreshProjectedUnitHudPositions()
{
	// Fixed HUD layout is applied synchronously by RefreshProjectedUnitHuds(). This
	// compatibility hook intentionally performs no world-to-screen projection.
}

bool UGameXXKBattleBoardWidget::IsTargetUnitHighlighted(FName UnitId) const
{
	return IsCardTargetingActive() && !UnitId.IsNone() && LegalCardTargetUnitIds.Contains(UnitId);
}

bool UGameXXKBattleBoardWidget::IsCardTargetingActive() const
{
	return InteractionMode == EGameXXKBattleInteractionMode::TargetingCard;
}

bool UGameXXKBattleBoardWidget::ChoosePendingRouteReward(FName RewardCardId, FName ReplacementEntryId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward())
	{
		LastCardInteractionError = TEXT("当前没有可选取的战后卡牌。");
		return false;
	}

	FGameXXKRouteCardAcquisitionPreview Preview;
	FString Error;
	if (!FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
		Subsystem->GetRuntimeState(),
		RewardCardId,
		NAME_None,
		Preview,
		&Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	if (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement)
	{
		if (RouteRewardCardIdAwaitingReplacement != RewardCardId)
		{
			RouteRewardCardIdAwaitingReplacement = RewardCardId;
			SelectedRouteRewardReplacementEntryId = NAME_None;
			LastCardInteractionError.Reset();
			RefreshProgrammaticLayout();
			return false;
		}

		if (ReplacementEntryId.IsNone()
			|| ReplacementEntryId != SelectedRouteRewardReplacementEntryId
			|| !Preview.EligibleReplacementEntryIds.Contains(ReplacementEntryId))
		{
			LastCardInteractionError = TEXT("路线临时牌已满，请先选择一张可替换的路线牌实例。");
			RefreshProgrammaticLayout();
			return false;
		}
	}
	else if (Preview.Decision != EGameXXKRouteCardAcquisitionDecision::CanCommit)
	{
		LastCardInteractionError = Error.IsEmpty() ? TEXT("当前奖励候选不可提交。") : Error;
		RefreshProgrammaticLayout();
		return false;
	}
	else
	{
		ReplacementEntryId = NAME_None;
		SelectedRouteRewardReplacementEntryId = NAME_None;
		RouteRewardCardIdAwaitingReplacement = NAME_None;
	}

	if (!Subsystem->ResolvePendingRouteRewardChoiceAndFinish(RewardCardId, ReplacementEntryId, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	SelectedRouteRewardReplacementEntryId = NAME_None;
	RouteRewardCardIdAwaitingReplacement = NAME_None;
	return ResolveAndRefreshCardBattleAfterMutation();
}

bool UGameXXKBattleBoardWidget::SkipPendingRouteReward()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward())
	{
		LastCardInteractionError = TEXT("当前没有可跳过的战后卡牌。");
		return false;
	}

	FString Error;
	if (!Subsystem->SkipPendingRouteRewardAndFinish(&Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	SelectedRouteRewardReplacementEntryId = NAME_None;
	RouteRewardCardIdAwaitingReplacement = NAME_None;
	return ResolveAndRefreshCardBattleAfterMutation();
}

bool UGameXXKBattleBoardWidget::HasPendingRouteReward() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle
		&& Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds.Num() > 0;
}

TArray<FName> UGameXXKBattleBoardWidget::GetPendingRouteRewardCardIds() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds : TArray<FName>();
}

bool UGameXXKBattleBoardWidget::SelectRouteRewardReplacementEntry(FName EntryId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward() || RouteRewardCardIdAwaitingReplacement.IsNone())
	{
		return false;
	}
	if (EntryId.IsNone() || !GetRouteRewardReplacementEntryIds().Contains(EntryId))
	{
		LastCardInteractionError = TEXT("只能替换当前候选允许的临时路线牌实例。");
		RefreshProgrammaticLayout();
		return false;
	}
	SelectedRouteRewardReplacementEntryId = EntryId;
	LastCardInteractionError.Reset();
	RefreshProgrammaticLayout();
	return true;
}

TArray<FName> UGameXXKBattleBoardWidget::GetRouteRewardReplacementEntryIds() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward() || RouteRewardCardIdAwaitingReplacement.IsNone())
	{
		return TArray<FName>();
	}

	FGameXXKRouteCardAcquisitionPreview Preview;
	FString Error;
	if (!FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
		Subsystem->GetRuntimeState(),
		RouteRewardCardIdAwaitingReplacement,
		NAME_None,
		Preview,
		&Error)
		|| Preview.Decision != EGameXXKRouteCardAcquisitionDecision::RequiresReplacement)
	{
		return TArray<FName>();
	}
	return Preview.EligibleReplacementEntryIds;
}

bool UGameXXKBattleBoardWidget::CancelRouteRewardReplacement()
{
	if (RouteRewardCardIdAwaitingReplacement.IsNone()
		&& SelectedRouteRewardReplacementEntryId.IsNone())
	{
		return false;
	}

	RouteRewardCardIdAwaitingReplacement = NAME_None;
	SelectedRouteRewardReplacementEntryId = NAME_None;
	LastCardInteractionError.Reset();
	if (HoveredCardTooltipSource == ECardTooltipSource::RouteReplacement)
	{
		HoveredCardTooltipSource = ECardTooltipSource::None;
		HoveredCardTooltipId = NAME_None;
	}
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKBattleBoardWidget::CancelBattleTargeting()
{
	if (!RouteRewardCardIdAwaitingReplacement.IsNone()
		|| !SelectedRouteRewardReplacementEntryId.IsNone())
	{
		return CancelRouteRewardReplacement();
	}
	if (IsCardTargetingActive())
	{
		ClearCardTargetingState();
		LastCardInteractionError.Reset();
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		RefreshProgrammaticLayout();
		return true;
	}
	if (IsTargetingBattleActionForTest())
	{
		InteractionMode = EGameXXKBattleInteractionMode::CommandMenuOpen;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	if (InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
		SelectedPartyIndex = INDEX_NONE;
		TargetingActionName = NAME_None;
		RefreshProgrammaticLayout();
		return true;
	}
	return false;
}

bool UGameXXKBattleBoardWidget::ExecuteBattleAction(FName ActionName)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}
	if (InteractionMode != EGameXXKBattleInteractionMode::CommandMenuOpen || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 PartyIndex = SelectedPartyIndex;
	bool bExecuted = false;
	if (ActionName == BasicAttackAction)
	{
		bExecuted = BeginTargetingBattleAction(BasicAttackAction);
	}
	else if (ActionName == CraneWingSlashAction)
	{
		bExecuted = BeginTargetingBattleAction(CraneWingSlashAction);
	}
	else if (ActionName == GuiyuanArtAction)
	{
		bExecuted = Subsystem->ExecuteBattleGuiyuanArt(PartyIndex);
	}
	else if (ActionName == DefendAction)
	{
		bExecuted = Subsystem->ExecuteBattleDefend(PartyIndex);
	}
	else if (ActionName == HealingPowderAction)
	{
		bExecuted = Subsystem->ExecuteBattleHealingPowder(PartyIndex);
	}
	if (!bExecuted && !Subsystem->GetRuntimeState().bHasActiveBattle)
	{
		bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, ResolveBattleVictoryCommand);
	}
	if (bExecuted)
	{
		if (ActionName != BasicAttackAction && ActionName != CraneWingSlashAction)
		{
			InteractionMode = EGameXXKBattleInteractionMode::Idle;
			SelectedPartyIndex = INDEX_NONE;
			TargetingActionName = NAME_None;
		}
		if (Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
		{
			GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
		}
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

bool UGameXXKBattleBoardWidget::IsBattleBoardVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

int32 UGameXXKBattleBoardWidget::GetEnemySlotCount() const
{
	return 0;
}

int32 UGameXXKBattleBoardWidget::GetPartySlotCount() const
{
	return 0;
}

FString UGameXXKBattleBoardWidget::GetEnemySlotSide() const
{
	return TEXT("Left");
}

FString UGameXXKBattleBoardWidget::GetPartySlotSide() const
{
	return TEXT("Right");
}

UGameXXKBattlePartyQiWidget* UGameXXKBattleBoardWidget::GetPartyQiWidgetForTest() const
{
	return PartyQiWidget;
}

UHorizontalBox* UGameXXKBattleBoardWidget::GetHandCardBoxForTest() const
{
	return HandCardBox;
}

UButton* UGameXXKBattleBoardWidget::GetEndTurnButtonForTest() const
{
	return EndTurnButton;
}

UCanvasPanel* UGameXXKBattleBoardWidget::GetBattleProjectedUnitHudLayerForTest() const
{
	return BattleProjectedUnitHudLayer;
}

UGameXXKBattleUnitHudWidget* UGameXXKBattleBoardWidget::GetProjectedUnitHudForTest(const FName UnitId) const
{
	return ProjectedUnitHuds.FindRef(UnitId);
}

int32 UGameXXKBattleBoardWidget::GetProjectedUnitHudCountForTest() const
{
	return ProjectedUnitHuds.Num();
}

FVector2D UGameXXKBattleBoardWidget::GetProjectedUnitHudAnchorPositionForTest(const FName UnitId) const
{
	const UGameXXKBattleUnitHudWidget* const Hud = GetProjectedUnitHudForTest(UnitId);
	const UCanvasPanelSlot* const CanvasSlot = Hud ? Cast<UCanvasPanelSlot>(Hud->Slot) : nullptr;
	return CanvasSlot ? CanvasSlot->GetAnchors().Minimum : FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetBattleUnitScreenPositionForTest(const FName UnitId) const
{
	return RegisteredBattleUnitScreenPositions.FindRef(UnitId);
}

bool UGameXXKBattleBoardWidget::HasBattleUnitScreenPositionForTest(const FName UnitId) const
{
	return RegisteredBattleUnitScreenPositions.Contains(UnitId);
}

bool UGameXXKBattleBoardWidget::HasProjectedUnitHudScreenPositionForTest(const FName UnitId) const
{
	(void)UnitId;
	return false;
}

FGameXXKBattleHudSafeStageLayout UGameXXKBattleBoardWidget::ResolveBattleHudSafeStageLayoutForTest(const FVector2D ViewportSize) const
{
	FGameXXKBattleHudSafeStageLayout Result;
	if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		Result.Scale = 0.0f;
		return Result;
	}

	Result.Scale = FMath::Min(
		ViewportSize.X / BattleHudSafeStageDesignSize.X,
		ViewportSize.Y / BattleHudSafeStageDesignSize.Y);
	Result.Size = BattleHudSafeStageDesignSize * Result.Scale;
	Result.Offset = (ViewportSize - Result.Size) * 0.5f;
	return Result;
}

FGameXXKBattleProjectedUnitHudLayout UGameXXKBattleBoardWidget::ResolveProjectedUnitHudLayoutForTest(
	const FVector2D Anchor,
	const FVector2D WidgetSize,
	const FVector2D CanvasSize,
	const FBox2D& HandRect,
	const FBox2D& QiRect,
	const FBox2D& EndTurnRect,
	const FBox2D& ShowcaseRect) const
{
	FGameXXKBattleProjectedUnitHudLayout Result;
	if (WidgetSize.X <= 0.0f || WidgetSize.Y <= 0.0f || CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		return Result;
	}

	FVector2D SlotPosition(Anchor.X - WidgetSize.X * 0.5f, Anchor.Y + ProjectedUnitHudFootGap);
	SlotPosition.X = FMath::Clamp(SlotPosition.X, 0.0f, FMath::Max(0.0f, CanvasSize.X - WidgetSize.X));
	SlotPosition.Y = FMath::Clamp(SlotPosition.Y, 0.0f, FMath::Max(0.0f, CanvasSize.Y - WidgetSize.Y));
	FBox2D Rect(SlotPosition, SlotPosition + WidgetSize);
	const FBox2D Obstacles[] = {HandRect, QiRect, EndTurnRect, ShowcaseRect};
	for (const FBox2D& Obstacle : Obstacles)
	{
		if (!DoRectsOverlap(Rect, Obstacle))
		{
			continue;
		}

		SlotPosition.Y = FMath::Clamp(
			Obstacle.Min.Y - ProjectedUnitHudObstacleGap - WidgetSize.Y,
			0.0f,
			FMath::Max(0.0f, CanvasSize.Y - WidgetSize.Y));
		Rect = FBox2D(SlotPosition, SlotPosition + WidgetSize);
		Result.bLiftedForObstacle = true;
	}
	Result.SlotPosition = SlotPosition;
	Result.Rect = Rect;
	return Result;
}

FVector2D UGameXXKBattleBoardWidget::GetEnemySlotPositionForTest(int32 SlotIndex) const
{
	return FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetPartySlotPositionForTest(int32 SlotIndex) const
{
	return FVector2D::ZeroVector;
}

bool UGameXXKBattleBoardWidget::HasBattleActionForTest(FName ActionName, bool bRequireEnabled) const
{
	const UButton* Button = nullptr;
	if (ActionName == BasicAttackAction)
	{
		Button = BasicAttackButton;
	}
	else if (ActionName == CraneWingSlashAction)
	{
		Button = CraneWingSlashButton;
	}
	else if (ActionName == GuiyuanArtAction)
	{
		Button = GuiyuanArtButton;
	}
	else if (ActionName == DefendAction)
	{
		Button = DefendButton;
	}
	else if (ActionName == HealingPowderAction)
	{
		Button = HealingPowderButton;
	}
	return Button && (!bRequireEnabled || (IsCommandMenuVisibleForTest() && Button->GetIsEnabled()));
}

bool UGameXXKBattleBoardWidget::IsCommandMenuVisibleForTest() const
{
	return InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingBasicAttack
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
}

bool UGameXXKBattleBoardWidget::IsTargetingBattleActionForTest() const
{
	return InteractionMode == EGameXXKBattleInteractionMode::TargetingBasicAttack
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCraneWingSlash
		|| InteractionMode == EGameXXKBattleInteractionMode::TargetingCard;
}

bool UGameXXKBattleBoardWidget::IsCardTargetingForTest() const
{
	return IsCardTargetingActive();
}

bool UGameXXKBattleBoardWidget::KeepTargetingAfterEmptyClickForTest() const
{
	return IsTargetingBattleActionForTest();
}

int32 UGameXXKBattleBoardWidget::GetSelectedPartyIndexForTest() const
{
	return SelectedPartyIndex;
}

FName UGameXXKBattleBoardWidget::GetTargetingActionNameForTest() const
{
	return TargetingActionName;
}

FVector2D UGameXXKBattleBoardWidget::GetTargetingPointerPositionForTest() const
{
	return TargetingPointerPosition;
}

FVector2D UGameXXKBattleBoardWidget::GetTargetingSourcePositionForTest() const
{
	return TargetingSourcePosition;
}

FName UGameXXKBattleBoardWidget::GetPendingCardInstanceIdForTest() const
{
	return PendingCardPreview.CardInstanceId;
}

int32 UGameXXKBattleBoardWidget::GetVisibleHandCardCountForTest() const
{
	return HandCardInstanceIds.Num();
}

FVector2D UGameXXKBattleBoardWidget::GetCommandMenuAnchorForTest() const
{
	return CommandMenuAnchor;
}

FVector2D UGameXXKBattleBoardWidget::ResolveCommandSourcePositionForTest(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition, FVector2D LocalSize) const
{
	return ResolveCommandSourcePosition(PartyIndex, MenuScreenPosition, UnitScreenPosition, LocalSize);
}

FVector2D UGameXXKBattleBoardWidget::ResolveSlateAbsolutePositionToLocalForTest(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D LocalSize) const
{
	return ResolveSlateAbsolutePositionToLocal(ScreenPosition, WidgetAbsolutePosition, LocalSize);
}

FVector2D UGameXXKBattleBoardWidget::ResolveSlateAbsolutePositionToLocalForTest(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D WidgetAbsoluteSize, FVector2D LocalSize) const
{
	return ResolveSlateAbsolutePositionToLocal(ScreenPosition, WidgetAbsolutePosition, WidgetAbsoluteSize, LocalSize);
}

FString UGameXXKBattleBoardWidget::GetBattleActionButtonResourcePathForTest(FName ActionName)
{
	EnsureBattleVisualResourcesLoaded();
	if (!BattleActionInkButtonTexture)
	{
		return TEXT("");
	}
	if (ActionName != BasicAttackAction
		&& ActionName != CraneWingSlashAction
		&& ActionName != GuiyuanArtAction
		&& ActionName != DefendAction
		&& ActionName != HealingPowderAction)
	{
		return TEXT("");
	}
	return BattleActionInkButtonTexture->GetPathName();
}

FLinearColor UGameXXKBattleBoardWidget::GetBattleActionButtonTintForTest(FName ActionName) const
{
	return ResolveBattleActionButtonTint(ActionName);
}

FString UGameXXKBattleBoardWidget::GetTargetingArrowHeadResourcePathForTest()
{
	EnsureBattleVisualResourcesLoaded();
	return TargetingArrowHeadTexture ? TargetingArrowHeadTexture->GetPathName() : FString();
}

int32 UGameXXKBattleBoardWidget::GetTargetingInkDabTextureCountForTest()
{
	EnsureBattleVisualResourcesLoaded();
	int32 LoadedCount = 0;
	for (const TObjectPtr<UTexture2D>& DabTexture : TargetingInkDabTextures)
	{
		if (DabTexture)
		{
			++LoadedCount;
		}
	}
	return LoadedCount;
}

FString UGameXXKBattleBoardWidget::GetCardFrameResourcePathForTest()
{
	EnsureBattleVisualResourcesLoaded();
	return CardFrameTexture ? CardFrameTexture->GetPathName() : FString();
}

FVector2D UGameXXKBattleBoardWidget::GetCardFrameRuntimeSizeForTest() const
{
	return PlayerHandCardSize;
}

FLinearColor UGameXXKBattleBoardWidget::GetCardFrameTintForTest() const
{
	return FLinearColor::White;
}

FString UGameXXKBattleBoardWidget::GetCardPortraitResourcePathForTest(FName CardId) const
{
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
	return Definition ? ResolveCardPortraitResourcePath(*Definition) : FString();
}

FLinearColor UGameXXKBattleBoardWidget::GetCardInfoStripTintForTest(FName CardId) const
{
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
	return Definition ? ResolveCardInfoStripTint(*Definition) : FLinearColor::Transparent;
}

FName UGameXXKBattleBoardWidget::GetSelectedRouteRewardReplacementEntryIdForTest() const
{
	return SelectedRouteRewardReplacementEntryId;
}

FName UGameXXKBattleBoardWidget::GetRouteRewardCardIdAwaitingReplacementForTest() const
{
	return RouteRewardCardIdAwaitingReplacement;
}

int32 UGameXXKBattleBoardWidget::GetVisibleEnemyIntentCardCountForTest() const
{
	return VisibleEnemyIntentIndices.Num();
}

bool UGameXXKBattleBoardWidget::IsHandCardSlotEnabledForTest(const int32 SlotIndex) const
{
	return HandCardButtons.IsValidIndex(SlotIndex)
		&& HandCardButtons[SlotIndex]
		&& HandCardButtons[SlotIndex]->GetIsEnabled();
}

int32 UGameXXKBattleBoardWidget::GetActiveEnemyIntentPresentationIndexForTest() const
{
	return ActiveEnemyIntentPresentationIndex;
}

int32 UGameXXKBattleBoardWidget::GetEnemyIntentPersistentIndexForVisibleSlot(const int32 VisibleSlotIndex) const
{
	return VisibleEnemyIntentIndices.IsValidIndex(VisibleSlotIndex)
		? VisibleEnemyIntentIndices[VisibleSlotIndex]
		: INDEX_NONE;
}

FString UGameXXKBattleBoardWidget::GetEnemyIntentSlotLabelForTest(const int32 VisibleSlotIndex) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 PersistentIntentIndex = GetEnemyIntentPersistentIndexForVisibleSlot(VisibleSlotIndex);
	if (!Subsystem
		|| !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		|| !Subsystem->GetRuntimeState().CardRun.EnemyIntents.IsValidIndex(PersistentIntentIndex))
	{
		return FString();
	}
	return ResolveEnemyIntentSourceSlotLabel(Subsystem->GetRuntimeState().CardRun.EnemyIntents[PersistentIntentIndex]);
}

FString UGameXXKBattleBoardWidget::GetEnemyIntentTooltipForTest(const int32 VisibleSlotIndex) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 PersistentIntentIndex = GetEnemyIntentPersistentIndexForVisibleSlot(VisibleSlotIndex);
	if (!Subsystem
		|| !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		|| !Subsystem->GetRuntimeState().CardRun.EnemyIntents.IsValidIndex(PersistentIntentIndex))
	{
		return FString();
	}
	const FGameXXKCardRunState& Run = Subsystem->GetRuntimeState().CardRun;
	return BuildEnemyIntentTooltip(Subsystem->GetRuntimeState(), Run.EnemyIntents[PersistentIntentIndex]);
}

FString UGameXXKBattleBoardWidget::GetCardTooltipTextForTest() const
{
	return HandCardDetailBody ? HandCardDetailBody->GetText().ToString() : FString();
}

bool UGameXXKBattleBoardWidget::IsCardTooltipVisibleForTest() const
{
	return HandCardDetailPanel
		&& HandCardDetailPanel->GetVisibility() != ESlateVisibility::Collapsed
		&& HandCardDetailPanel->GetVisibility() != ESlateVisibility::Hidden;
}

bool UGameXXKBattleBoardWidget::IsCardTooltipHitTestInvisibleForTest() const
{
	return HandCardDetailPanel && HandCardDetailPanel->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

void UGameXXKBattleBoardWidget::HandlePendingChoiceCardHoverChanged(
	const FName CandidateInstanceId,
	const EGameXXKCardPendingChoiceKind ChoiceKind,
	const bool bHovered)
{
	if (bHovered)
	{
		HoveredCardTooltipSource = ECardTooltipSource::PendingChoice;
		HoveredCardTooltipId = CandidateInstanceId;
		HoveredPendingChoiceKind = ChoiceKind;
	}
	else if (HoveredCardTooltipSource == ECardTooltipSource::PendingChoice
		&& HoveredCardTooltipId == CandidateInstanceId
		&& HoveredPendingChoiceKind == ChoiceKind)
	{
		HoveredCardTooltipSource = ECardTooltipSource::None;
		HoveredCardTooltipId = NAME_None;
		HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
	}
	RefreshCardTooltip();
}

void UGameXXKBattleBoardWidget::HandleRouteRewardReplacementEntryHoverChanged(const FName EntryId, const bool bHovered)
{
	if (bHovered)
	{
		HoveredCardTooltipSource = ECardTooltipSource::RouteReplacement;
		HoveredCardTooltipId = EntryId;
	}
	else if (HoveredCardTooltipSource == ECardTooltipSource::RouteReplacement && HoveredCardTooltipId == EntryId)
	{
		HoveredCardTooltipSource = ECardTooltipSource::None;
		HoveredCardTooltipId = NAME_None;
	}
	RefreshCardTooltip();
}

void UGameXXKBattleBoardWidget::BuildProgrammaticLayout()
{
	EnsureBattleVisualResourcesLoaded();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("BattleBoardWidgetTree"));
	}
	if (!WidgetTree || RootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKBattleBoardRoot"));
	WidgetTree->RootWidget = RootCanvas;
	UScaleBox* const BattleHudSafeStage = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("BattleHudSafeStage"));
	USizeBox* const BattleHudSafeStageSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("BattleHudSafeStageSize"));
	BattleProjectedUnitHudLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BattleProjectedUnitHudLayer"));
	if (BattleHudSafeStage && BattleHudSafeStageSize && BattleProjectedUnitHudLayer)
	{
		BattleHudSafeStage->SetStretch(EStretch::ScaleToFit);
		BattleHudSafeStage->SetStretchDirection(EStretchDirection::Both);
		BattleHudSafeStageSize->SetWidthOverride(BattleHudSafeStageDesignSize.X);
		BattleHudSafeStageSize->SetHeightOverride(BattleHudSafeStageDesignSize.Y);
		BattleProjectedUnitHudLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BattleHudSafeStageSize->SetContent(BattleProjectedUnitHudLayer);
		BattleHudSafeStage->SetContent(BattleHudSafeStageSize);
		if (UScaleBoxSlot* const SafeStageContentSlot = Cast<UScaleBoxSlot>(BattleHudSafeStageSize->Slot))
		{
			SafeStageContentSlot->SetHorizontalAlignment(HAlign_Center);
			SafeStageContentSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UCanvasPanelSlot* const SafeStageSlot = RootCanvas->AddChildToCanvas(BattleHudSafeStage))
		{
			SafeStageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			SafeStageSlot->SetOffsets(FMargin(0.0f));
			SafeStageSlot->SetAlignment(FVector2D::ZeroVector);
			SafeStageSlot->SetZOrder(ProjectedUnitHudLayerZOrder);
		}
	}

	BattleAnimationLayer = WidgetTree->ConstructWidget<UGameXXKBattleAnimationLayerWidget>(
		UGameXXKBattleAnimationLayerWidget::StaticClass(),
		TEXT("BattleAnimationLayer"));
	if (BattleAnimationLayer)
	{
		BattleAnimationLayer->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* AnimationLayerSlot = RootCanvas->AddChildToCanvas(BattleAnimationLayer))
		{
			AnimationLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			AnimationLayerSlot->SetOffsets(FMargin(0.0f));
			AnimationLayerSlot->SetAlignment(FVector2D::ZeroVector);
			AnimationLayerSlot->SetZOrder(1000);
		}
	}

	EnemyIntentCardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattleEnemyIntentCardBox"));
	EnemyIntentCardBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* IntentRailSlot = RootCanvas->AddChildToCanvas(EnemyIntentCardBox))
	{
		IntentRailSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		IntentRailSlot->SetOffsets(FMargin(-EnemyIntentRailSize.X * 0.5f, 24.0f, EnemyIntentRailSize.X, EnemyIntentRailSize.Y));
		IntentRailSlot->SetAlignment(FVector2D::ZeroVector);
	}
	EnemyIntentCardButtons.Reserve(MaximumVisibleEnemyIntentCards);
	EnemyIntentSlotLabels.Reserve(MaximumVisibleEnemyIntentCards);
	EnemyIntentCardBodies.Reserve(MaximumVisibleEnemyIntentCards);
	for (int32 SlotIndex = 0; SlotIndex < MaximumVisibleEnemyIntentCards; ++SlotIndex)
	{
		UHorizontalBox* IntentSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("BattleEnemyIntentSlot_%02d"), SlotIndex));
		USizeBox* SideLabelSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("BattleEnemyIntentSlotLabelSize_%02d"), SlotIndex));
		SideLabelSizeBox->SetWidthOverride(42.0f);
		SideLabelSizeBox->SetHeightOverride(EnemyIntentCardSize.Y);
		UTextBlock* SideLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("BattleEnemyIntentSlotLabel_%02d"), SlotIndex));
		SideLabel->SetJustification(ETextJustify::Center);
		SideLabel->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
		FSlateFontInfo SideLabelFont = SideLabel->GetFont();
		SideLabelFont.Size = 16;
		SideLabelFont.TypefaceFontName = TEXT("Bold");
		SideLabel->SetFont(SideLabelFont);
		SideLabelSizeBox->AddChild(SideLabel);
		if (UHorizontalBoxSlot* SideLabelSlot = IntentSlotBox->AddChildToHorizontalBox(SideLabelSizeBox))
		{
			SideLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f));
			SideLabelSlot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* IntentCardSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("BattleEnemyIntentCardSize_%02d"), SlotIndex));
		IntentCardSizeBox->SetWidthOverride(EnemyIntentCardSize.X);
		IntentCardSizeBox->SetHeightOverride(EnemyIntentCardSize.Y);
		UButton* IntentCardButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			*FString::Printf(TEXT("BattleEnemyIntentCard_%02d"), SlotIndex));
		StyleCardButton(IntentCardButton, EnemyIntentCardSize);
		IntentCardButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		UTextBlock* IntentCardBody = nullptr;
		BuildEnemyIntentCardFace(
			IntentCardButton,
			FString::Printf(TEXT("BattleEnemyIntentCard_%02d"), SlotIndex),
			IntentCardBody);
		IntentCardSizeBox->AddChild(IntentCardButton);
		if (UHorizontalBoxSlot* IntentCardSlot = IntentSlotBox->AddChildToHorizontalBox(IntentCardSizeBox))
		{
			IntentCardSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
			IntentCardSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* IntentSlot = EnemyIntentCardBox->AddChildToHorizontalBox(IntentSlotBox))
		{
			IntentSlot->SetPadding(FMargin(0.0f));
			IntentSlot->SetVerticalAlignment(VAlign_Center);
		}
		EnemyIntentCardButtons.Add(IntentCardButton);
		EnemyIntentSlotLabels.Add(SideLabel);
		EnemyIntentCardBodies.Add(IntentCardBody);
		switch (SlotIndex)
		{
		case 0:
			IntentCardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot0Hovered);
			IntentCardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot0Unhovered);
			break;
		case 1:
			IntentCardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot1Hovered);
			IntentCardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot1Unhovered);
			break;
		case 2:
			IntentCardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot2Hovered);
			IntentCardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentSlot2Unhovered);
			break;
		default: break;
		}
	}

	EnemyIntentShowcaseCard = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattleEnemyIntentShowcaseCard"));
	StyleCardButton(EnemyIntentShowcaseCard, EnemyIntentShowcaseCardSize);
	EnemyIntentShowcaseCard->SetVisibility(ESlateVisibility::Collapsed);
	EnemyIntentShowcaseBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleEnemyIntentShowcaseBody"));
	EnemyIntentShowcaseBody->SetAutoWrapText(true);
	EnemyIntentShowcaseBody->SetJustification(ETextJustify::Center);
	EnemyIntentShowcaseBody->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	FSlateFontInfo ShowcaseFont = EnemyIntentShowcaseBody->GetFont();
	ShowcaseFont.Size = 15;
	ShowcaseFont.TypefaceFontName = TEXT("Bold");
	EnemyIntentShowcaseBody->SetFont(ShowcaseFont);
	EnemyIntentShowcaseCard->AddChild(EnemyIntentShowcaseBody);
	if (UCanvasPanelSlot* ShowcaseSlot = RootCanvas->AddChildToCanvas(EnemyIntentShowcaseCard))
	{
		ShowcaseSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		ShowcaseSlot->SetOffsets(FMargin(-128.0f, -140.0f, 256.0f, 292.0f));
		ShowcaseSlot->SetAlignment(FVector2D::ZeroVector);
	}

	EnemyIntentRecoveryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattleEnemyIntentRecoveryButton"));
	StyleBattleActionButton(EnemyIntentRecoveryButton, FName(TEXT("BattleEnemyIntentRecovery")));
	EnemyIntentRecoveryButton->SetVisibility(ESlateVisibility::Collapsed);
	UTextBlock* EnemyIntentRecoveryLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleEnemyIntentRecoveryLabel"));
	EnemyIntentRecoveryLabel->SetText(NSLOCTEXT("GameXXKBattle", "RetryEnemyIntentCompletion", "重试敌方结算"));
	EnemyIntentRecoveryLabel->SetJustification(ETextJustify::Center);
	EnemyIntentRecoveryLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	EnemyIntentRecoveryButton->AddChild(EnemyIntentRecoveryLabel);
	EnemyIntentRecoveryButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEnemyIntentRecoveryClicked);
	if (UCanvasPanelSlot* RecoverySlot = RootCanvas->AddChildToCanvas(EnemyIntentRecoveryButton))
	{
		RecoverySlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		RecoverySlot->SetOffsets(FMargin(-115.0f, 168.0f, 230.0f, 58.0f));
		RecoverySlot->SetAlignment(FVector2D::ZeroVector);
	}

	EnemyIntentDetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleEnemyIntentDetailPanel"));
	EnemyIntentDetailPanel->SetBrush(BuildBoxTextureBrush(
		BattleStatusWindowFrameTexture.Get(),
		EnemyIntentTooltipSize,
		FMargin(BattleStatusFrameMarginRatio)));
	EnemyIntentDetailPanel->SetBrushColor(FLinearColor::White);
	EnemyIntentDetailPanel->SetPadding(FMargin(22.0f, 18.0f, 22.0f, 16.0f));
	EnemyIntentDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	EnemyIntentDetailBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleEnemyIntentDetailBody"));
	EnemyIntentDetailBody->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	EnemyIntentDetailBody->SetAutoWrapText(true);
	EnemyIntentDetailBody->SetJustification(ETextJustify::Left);
	FSlateFontInfo EnemyIntentDetailFont = EnemyIntentDetailBody->GetFont();
	EnemyIntentDetailFont.Size = 14;
	EnemyIntentDetailBody->SetFont(EnemyIntentDetailFont);
	EnemyIntentDetailPanel->SetContent(EnemyIntentDetailBody);
	if (UCanvasPanelSlot* IntentDetailSlot = RootCanvas->AddChildToCanvas(EnemyIntentDetailPanel))
	{
		IntentDetailSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		IntentDetailSlot->SetOffsets(FMargin(-EnemyIntentTooltipSize.X * 0.5f, -118.0f, EnemyIntentTooltipSize.X, EnemyIntentTooltipSize.Y));
		IntentDetailSlot->SetAlignment(FVector2D::ZeroVector);
	}

	ActionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleActionBox"));
	if (UCanvasPanelSlot* ActionSlot = RootCanvas->AddChildToCanvas(ActionBox))
	{
		ActionSlot->SetAnchors(FAnchors(0.68f, 0.52f, 0.98f, 0.96f));
		ActionSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		ActionSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}

	HandCardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattleHandCardBox"));
	if (UCanvasPanelSlot* HandSlot = RootCanvas->AddChildToCanvas(HandCardBox))
	{
		HandSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		HandSlot->SetOffsets(FMargin(-PlayerHandRowSize.X * 0.5f, -277.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y));
		HandSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}
	HandCardButtons.Reserve(MaximumVisibleHandCards);
	HandCardLabels.Reserve(MaximumVisibleHandCards);
	HandCardPortraits.Reserve(MaximumVisibleHandCards);
	HandCardInfoStrips.Reserve(MaximumVisibleHandCards);
	for (int32 SlotIndex = 0; SlotIndex < MaximumVisibleHandCards; ++SlotIndex)
	{
		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("BattleHandCardSize_%02d"), SlotIndex));
		CardSizeBox->SetWidthOverride(PlayerHandCardSize.X);
		CardSizeBox->SetHeightOverride(PlayerHandCardSize.Y);
		UButton* CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("BattleHandCard_%02d"), SlotIndex));
		StyleCardButton(CardButton, PlayerHandCardSize);
		CardButton->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		UTextBlock* CardLabel = nullptr;
		UImage* CardPortrait = nullptr;
		UBorder* CardInfoStrip = nullptr;
		BuildCardFace(
			CardButton,
			FString::Printf(TEXT("BattleHandCard_%02d"), SlotIndex),
			CardLabel,
			CardPortrait,
			CardInfoStrip,
			true);
		CardSizeBox->AddChild(CardButton);
		if (UHorizontalBoxSlot* CardSlot = HandCardBox->AddChildToHorizontalBox(CardSizeBox))
		{
			CardSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 0.0f));
			CardSlot->SetVerticalAlignment(VAlign_Bottom);
		}
		HandCardButtons.Add(CardButton);
		HandCardLabels.Add(CardLabel);
		HandCardPortraits.Add(CardPortrait);
		HandCardInfoStrips.Add(CardInfoStrip);
		switch (SlotIndex)
		{
		case 0:
			CardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot0Clicked);
			CardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot0Hovered);
			CardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot0Unhovered);
			break;
		case 1:
			CardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot1Clicked);
			CardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot1Hovered);
			CardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot1Unhovered);
			break;
		case 2:
			CardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot2Clicked);
			CardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot2Hovered);
			CardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot2Unhovered);
			break;
		case 3:
			CardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot3Clicked);
			CardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot3Hovered);
			CardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot3Unhovered);
			break;
		case 4:
			CardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot4Clicked);
			CardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot4Hovered);
			CardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHandCardSlot4Unhovered);
			break;
		default: break;
		}
	}

	PartyQiWidget = WidgetTree->ConstructWidget<UGameXXKBattlePartyQiWidget>(UGameXXKBattlePartyQiWidget::StaticClass(), TEXT("BattlePartyQiWidget"));
	PartyQiWidget->PrepareForBoardEmbedding();
	if (UCanvasPanelSlot* PartyQiSlot = RootCanvas->AddChildToCanvas(PartyQiWidget))
	{
		const FGameXXKBattlePartyQiLayout PartyQiLayout = ResolvePartyQiLayout(FVector2D::ZeroVector);
		PartyQiSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
		PartyQiSlot->SetOffsets(PartyQiLayout.SlotOffsets);
		PartyQiSlot->SetAlignment(FVector2D::ZeroVector);
		PartyQiSlot->SetZOrder(PartyQiWidgetZOrder);
	}

	EndTurnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattleEndTurnButton"));
	StyleBattleActionButton(EndTurnButton, FName(TEXT("BattleEndTurn")));
	UTextBlock* EndTurnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleEndTurnLabel"));
	EndTurnLabel->SetText(NSLOCTEXT("GameXXKBattle", "EndTurn", "结束回合"));
	EndTurnLabel->SetJustification(ETextJustify::Center);
	EndTurnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	EndTurnButton->AddChild(EndTurnLabel);
	EndTurnButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleEndTurnClicked);
	if (UCanvasPanelSlot* EndTurnSlot = RootCanvas->AddChildToCanvas(EndTurnButton))
	{
		EndTurnSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
		EndTurnSlot->SetOffsets(FMargin(-230.0f, -138.0f, 190.0f, 62.0f));
		EndTurnSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}

	HandCardDetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleHandCardDetailPanel"));
	HandCardDetailPanel->SetBrush(BuildBoxTextureBrush(
		BattleStatusWindowFrameTexture.Get(),
		HandCardDetailPanelSize,
		FMargin(BattleStatusFrameMarginRatio)));
	HandCardDetailPanel->SetBrushColor(FLinearColor::White);
	HandCardDetailPanel->SetPadding(FMargin(20.0f, 16.0f, 20.0f, 14.0f));
	HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	HandCardDetailBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleHandCardDetailBody"));
	HandCardDetailBody->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	HandCardDetailBody->SetAutoWrapText(true);
	HandCardDetailBody->SetJustification(ETextJustify::Left);
	HandCardDetailBody->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f));
	HandCardDetailBody->SetShadowOffset(FVector2D(0.5f, 0.5f));
	FSlateFontInfo DetailFont = HandCardDetailBody->GetFont();
	DetailFont.Size = 14;
	HandCardDetailBody->SetFont(DetailFont);
	HandCardDetailPanel->SetContent(HandCardDetailBody);
	if (UCanvasPanelSlot* DetailSlot = RootCanvas->AddChildToCanvas(HandCardDetailPanel))
	{
		DetailSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		DetailSlot->SetOffsets(FMargin(-HandCardDetailPanelSize.X * 0.5f, -588.0f, HandCardDetailPanelSize.X, HandCardDetailPanelSize.Y));
		DetailSlot->SetAlignment(FVector2D::ZeroVector);
		DetailSlot->SetZOrder(50);
	}

	RewardCardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattleRewardCardBox"));
	if (UCanvasPanelSlot* RewardSlot = RootCanvas->AddChildToCanvas(RewardCardBox))
	{
		RewardSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		RewardSlot->SetOffsets(FMargin(-185.0f, -116.0f, 370.0f, 136.0f));
		RewardSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}
	RewardCardButtons.Reserve(MaximumVisibleRewardCards);
	RewardCardLabels.Reserve(MaximumVisibleRewardCards);
	RewardCardPortraits.Reserve(MaximumVisibleRewardCards);
	RewardCardInfoStrips.Reserve(MaximumVisibleRewardCards);
	for (int32 SlotIndex = 0; SlotIndex < MaximumVisibleRewardCards; ++SlotIndex)
	{
		USizeBox* RewardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("BattleRewardCardSize_%02d"), SlotIndex));
		RewardSizeBox->SetWidthOverride(RewardCardSize.X);
		RewardSizeBox->SetHeightOverride(RewardCardSize.Y);
		UButton* RewardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("BattleRewardCard_%02d"), SlotIndex));
		StyleCardButton(RewardButton, RewardCardSize);
		UTextBlock* RewardLabel = nullptr;
		UImage* RewardPortrait = nullptr;
		UBorder* RewardInfoStrip = nullptr;
		BuildCardFace(
			RewardButton,
			FString::Printf(TEXT("BattleRewardCard_%02d"), SlotIndex),
			RewardLabel,
			RewardPortrait,
			RewardInfoStrip);
		RewardSizeBox->AddChild(RewardButton);
		if (UHorizontalBoxSlot* RewardCardSlot = RewardCardBox->AddChildToHorizontalBox(RewardSizeBox))
		{
			RewardCardSlot->SetPadding(FMargin(5.0f, 0.0f, 5.0f, 0.0f));
		}
		RewardCardButtons.Add(RewardButton);
		RewardCardLabels.Add(RewardLabel);
		RewardCardPortraits.Add(RewardPortrait);
		RewardCardInfoStrips.Add(RewardInfoStrip);
		switch (SlotIndex)
		{
		case 0:
			RewardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot0Clicked);
			RewardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot0Hovered);
			RewardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot0Unhovered);
			break;
		case 1:
			RewardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot1Clicked);
			RewardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot1Hovered);
			RewardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot1Unhovered);
			break;
		case 2:
			RewardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot2Clicked);
			RewardButton->OnHovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot2Hovered);
			RewardButton->OnUnhovered.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleRewardCardSlot2Unhovered);
			break;
		default: break;
		}
	}

	RouteRewardReplacementScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BattleRouteRewardReplacementScroll"));
	RouteRewardReplacementScrollBox->SetOrientation(Orient_Vertical);
	RouteRewardReplacementScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	FGameXXKPartyDeckUiStyle::ApplyPaperInkScrollBar(RouteRewardReplacementScrollBox);
	RouteRewardReplacementScrollBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* ReplacementSlot = RootCanvas->AddChildToCanvas(RouteRewardReplacementScrollBox))
	{
		ReplacementSlot->SetAnchors(FAnchors(0.03f, 0.19f, 0.18f, 0.77f));
		ReplacementSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
		ReplacementSlot->SetAlignment(FVector2D::ZeroVector);
	}

	SkipRewardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattleSkipRewardButton"));
	StyleBattleActionButton(SkipRewardButton, FName(TEXT("BattleSkipReward")));
	UTextBlock* SkipRewardLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleSkipRewardLabel"));
	SkipRewardLabel->SetText(NSLOCTEXT("GameXXKBattle", "SkipReward", "跳过奖励"));
	SkipRewardLabel->SetJustification(ETextJustify::Center);
	SkipRewardLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SkipRewardButton->AddChild(SkipRewardLabel);
	SkipRewardButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleSkipRewardClicked);
	if (UCanvasPanelSlot* SkipRewardSlot = RootCanvas->AddChildToCanvas(SkipRewardButton))
	{
		SkipRewardSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		SkipRewardSlot->SetOffsets(FMargin(-95.0f, 26.0f, 190.0f, 56.0f));
		SkipRewardSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}

	// A card effect can deliberately pause the rules runtime for a choice.  This
	// parchment overlay keeps that blocking state visible and actionable instead
	// of leaving an apparently enabled End Turn button that the rules reject.
	PendingChoicePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattlePendingChoicePanel"));
	PendingChoicePanel->SetBrush(BuildBoxTextureBrush(
		BattleStatusWindowFrameTexture.Get(),
		FVector2D(760.0f, 250.0f),
		FMargin(BattleStatusFrameMarginRatio)));
	PendingChoicePanel->SetBrushColor(FLinearColor::White);
	PendingChoicePanel->SetPadding(FMargin(18.0f, 14.0f, 18.0f, 14.0f));
	PendingChoicePanel->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanel* PendingChoiceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BattlePendingChoiceCanvas"));
	PendingChoicePanel->SetContent(PendingChoiceCanvas);
	PendingChoicePromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattlePendingChoicePrompt"));
	PendingChoicePromptText->SetJustification(ETextJustify::Center);
	PendingChoicePromptText->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	FSlateFontInfo PendingChoiceFont = PendingChoicePromptText->GetFont();
	PendingChoiceFont.Size = 17;
	PendingChoiceFont.TypefaceFontName = TEXT("Bold");
	PendingChoicePromptText->SetFont(PendingChoiceFont);
	if (UCanvasPanelSlot* PromptSlot = PendingChoiceCanvas->AddChildToCanvas(PendingChoicePromptText))
	{
		PromptSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 0.0f));
		PromptSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 31.0f));
		PromptSlot->SetAlignment(FVector2D::ZeroVector);
	}
	PendingChoiceCardBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BattlePendingChoiceCardBox"));
	if (UCanvasPanelSlot* PendingCardsSlot = PendingChoiceCanvas->AddChildToCanvas(PendingChoiceCardBox))
	{
		PendingCardsSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		PendingCardsSlot->SetOffsets(FMargin(-357.0f, 39.0f, 714.0f, 129.0f));
		PendingCardsSlot->SetAlignment(FVector2D::ZeroVector);
	}
	PendingChoiceCardButtons.Reserve(MaximumVisiblePendingCardChoices);
	PendingChoiceCardLabels.Reserve(MaximumVisiblePendingCardChoices);
	PendingChoiceCardPortraits.Reserve(MaximumVisiblePendingCardChoices);
	PendingChoiceCardInfoStrips.Reserve(MaximumVisiblePendingCardChoices);
	for (int32 SlotIndex = 0; SlotIndex < MaximumVisiblePendingCardChoices; ++SlotIndex)
	{
		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("BattlePendingChoiceCardSize_%02d"), SlotIndex));
		CardSizeBox->SetWidthOverride(RewardCardSize.X);
		CardSizeBox->SetHeightOverride(RewardCardSize.Y);
		UGameXXKPendingChoiceCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKPendingChoiceCardButton>(
			UGameXXKPendingChoiceCardButton::StaticClass(),
			*FString::Printf(TEXT("BattlePendingChoiceCard_%02d"), SlotIndex));
		CardButton->Configure(this, NAME_None, EGameXXKCardPendingChoiceKind::Invalid);
		StyleCardButton(CardButton, RewardCardSize);
		UTextBlock* CardLabel = nullptr;
		UImage* CardPortrait = nullptr;
		UBorder* CardInfoStrip = nullptr;
		BuildCardFace(
			CardButton,
			FString::Printf(TEXT("BattlePendingChoiceCard_%02d"), SlotIndex),
			CardLabel,
			CardPortrait,
			CardInfoStrip);
		CardSizeBox->AddChild(CardButton);
		if (UHorizontalBoxSlot* CardSlot = PendingChoiceCardBox->AddChildToHorizontalBox(CardSizeBox))
		{
			CardSlot->SetPadding(FMargin(3.0f, 0.0f, 3.0f, 0.0f));
		}
		CardButton->SetVisibility(ESlateVisibility::Collapsed);
		PendingChoiceCardButtons.Add(CardButton);
		PendingChoiceCardLabels.Add(CardLabel);
		PendingChoiceCardPortraits.Add(CardPortrait);
		PendingChoiceCardInfoStrips.Add(CardInfoStrip);
	}
	PendingChoiceCancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BattlePendingChoiceCancelButton"));
	StyleBattleActionButton(PendingChoiceCancelButton, FName(TEXT("BattleInsightCancel")));
	UTextBlock* PendingChoiceCancelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattlePendingChoiceCancelLabel"));
	PendingChoiceCancelLabel->SetText(NSLOCTEXT("GameXXKBattle", "CancelInsight", "取消洞察"));
	PendingChoiceCancelLabel->SetJustification(ETextJustify::Center);
	PendingChoiceCancelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PendingChoiceCancelButton->AddChild(PendingChoiceCancelLabel);
	PendingChoiceCancelButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandlePendingInsightCancelClicked);
	PendingChoiceCancelButton->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PendingCancelSlot = PendingChoiceCanvas->AddChildToCanvas(PendingChoiceCancelButton))
	{
		PendingCancelSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		PendingCancelSlot->SetOffsets(FMargin(-90.0f, 178.0f, 180.0f, 32.0f));
		PendingCancelSlot->SetAlignment(FVector2D::ZeroVector);
	}
	if (UCanvasPanelSlot* PendingChoiceSlot = RootCanvas->AddChildToCanvas(PendingChoicePanel))
	{
		PendingChoiceSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PendingChoiceSlot->SetOffsets(FMargin(-380.0f, -125.0f, 760.0f, 250.0f));
		PendingChoiceSlot->SetAlignment(FVector2D::ZeroVector);
	}

	BasicAttackButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "BasicAttack", "普攻"), FName(TEXT("BattleBasicAttackButton")), BasicAttackAction);
	CraneWingSlashButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "CraneWingSlash", "鹤羽斩"), FName(TEXT("BattleCraneWingSlashButton")), CraneWingSlashAction);
	GuiyuanArtButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "GuiyuanArt", "归元术"), FName(TEXT("BattleGuiyuanArtButton")), GuiyuanArtAction);
	DefendButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "Defend", "防御"), FName(TEXT("BattleDefendButton")), DefendAction);
	HealingPowderButton = AddBattleActionButton(NSLOCTEXT("GameXXKBattle", "HealingPowder", "金疮药"), FName(TEXT("BattleHealingPowderButton")), HealingPowderAction);

	if (BasicAttackButton)
	{
		BasicAttackButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleBasicAttackClicked);
	}
	if (CraneWingSlashButton)
	{
		CraneWingSlashButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleCraneWingSlashClicked);
	}
	if (GuiyuanArtButton)
	{
		GuiyuanArtButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleGuiyuanArtClicked);
	}
	if (DefendButton)
	{
		DefendButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleDefendClicked);
	}
	if (HealingPowderButton)
	{
		HealingPowderButton->OnClicked.AddDynamic(this, &UGameXXKBattleBoardWidget::HandleHealingPowderClicked);
	}
}

UButton* UGameXXKBattleBoardWidget::AddBattleActionButton(const FText& Label, FName ButtonName, FName ActionName)
{
	if (!ActionBox || !WidgetTree)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
	StyleBattleActionButton(Button, ActionName);
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LabelText->SetJustification(ETextJustify::Center);
	LabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	LabelText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo LabelFont = LabelText->GetFont();
	LabelFont.Size = 22;
	LabelFont.TypefaceFontName = TEXT("Bold");
	LabelText->SetFont(LabelFont);
	Button->AddChild(LabelText);
	if (UVerticalBoxSlot* ButtonSlot = ActionBox->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	return Button;
}

void UGameXXKBattleBoardWidget::RefreshProgrammaticLayout()
{
	if (ActionBox)
	{
		if (UCanvasPanelSlot* ActionSlot = Cast<UCanvasPanelSlot>(ActionBox->Slot))
		{
			ActionSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			ActionSlot->SetOffsets(FMargin(CommandMenuAnchor.X, CommandMenuAnchor.Y, CommandMenuWidth, CommandMenuHeight));
			ActionSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
	}
	RefreshActionButtons();
	RefreshHandCards();
	RefreshPartyQiWidget();
	RefreshEnemyIntentCards();
	RefreshEnemyIntentShowcase();
	RefreshEnemyIntentDetail();
	RefreshEnemyIntentRecoveryControl();
	RefreshPendingCardChoices();
	RefreshPendingRewardChoices();
	RefreshCardTooltip();
}

void UGameXXKBattleBoardWidget::RefreshActionButtons()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bFixtureReadOnly = Subsystem && Subsystem->IsBattleHudFixtureActiveForTest();
	const bool bShowMenu = IsCommandMenuVisibleForTest();
	const bool bCanUseMenu = !bFixtureReadOnly && InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen;
	if (ActionBox)
	{
		ActionBox->SetVisibility(bShowMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ActionBox->SetIsEnabled(bCanUseMenu);
		ActionBox->SetRenderOpacity(IsTargetingBattleActionForTest() ? 0.55f : 1.0f);
	}

	const bool bInBattle = State && State->Screen == EGameXXKScreen::Battle && State->bHasActiveBattle && State->ActiveBattleParty.IsValidIndex(SelectedPartyIndex);
	const bool bHeroReady = bInBattle && !State->ActiveBattleParty[SelectedPartyIndex].bDefeated && State->ActiveBattleParty[SelectedPartyIndex].HP > 0;
	const bool bHasTarget = bInBattle && FindFirstLivingEnemyIndex() != INDEX_NONE;
	const bool bCanHealHero = bHeroReady && State->ActiveBattleParty[SelectedPartyIndex].HP < State->ActiveBattleParty[SelectedPartyIndex].MaxHP;

	if (BasicAttackButton)
	{
		BasicAttackButton->SetIsEnabled(bCanUseMenu && bHeroReady && bHasTarget);
	}
	if (CraneWingSlashButton)
	{
		CraneWingSlashButton->SetIsEnabled(bCanUseMenu && bHeroReady && bHasTarget && State->ActiveBattleParty[SelectedPartyIndex].MP >= 8);
	}
	if (GuiyuanArtButton)
	{
		GuiyuanArtButton->SetIsEnabled(bCanUseMenu && bCanHealHero && State->ActiveBattleParty[SelectedPartyIndex].MP >= 10);
	}
	if (DefendButton)
	{
		DefendButton->SetIsEnabled(bCanUseMenu && bHeroReady);
	}
	if (HealingPowderButton)
	{
		HealingPowderButton->SetIsEnabled(bCanUseMenu && bCanHealHero && UGameXXKMVPRules::GetItemCount(*State, UGameXXKMVPRules::ItemHealingPowder()) > 0);
	}
}

void UGameXXKBattleBoardWidget::RefreshHandCards()
{
	HandCardInstanceIds.Reset();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bFixtureReadOnly = Subsystem && Subsystem->IsBattleHudFixtureActiveForTest();
	const bool bCardBattleVisible = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		&& !HasPendingRouteReward();
	if (bCardBattleVisible)
	{
		for (const FGameXXKCardInstance& Card : State->CardRun.ActiveBattle.Deck.Hand)
		{
			if (HandCardInstanceIds.Num() >= MaximumVisibleHandCards)
			{
				break;
			}
			HandCardInstanceIds.Add(Card.InstanceId);
		}
	}

	if (HandCardBox)
	{
		HandCardBox->SetVisibility(bCardBattleVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const EGameXXKCardPendingChoiceKind PendingChoiceKind = bCardBattleVisible
		? State->CardRun.ActiveBattle.Deck.PendingChoice.Kind
		: EGameXXKCardPendingChoiceKind::None;
	const bool bHasBlockingCardChoice = PendingChoiceKind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
		|| PendingChoiceKind == EGameXXKCardPendingChoiceKind::ForcedDiscard;
	// Enemy intent Reveal/Resolve/Settle owns the board. The cards stay visible
	// for spatial continuity, but their buttons must not remain interactive while
	// the runtime phase is Enemy.
	const bool bCanEndTurn = bCardBattleVisible
		&& !bFixtureReadOnly
		&& !IsCardTargetingActive()
		&& !bHasBlockingCardChoice
		&& State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player;
	if (EndTurnButton)
	{
		EndTurnButton->SetVisibility(bCardBattleVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EndTurnButton->SetIsEnabled(bCanEndTurn);
	}

	for (int32 SlotIndex = 0; SlotIndex < HandCardButtons.Num(); ++SlotIndex)
	{
		UButton* CardButton = HandCardButtons[SlotIndex];
		UTextBlock* CardLabel = HandCardLabels.IsValidIndex(SlotIndex) ? HandCardLabels[SlotIndex] : nullptr;
		UImage* CardPortrait = HandCardPortraits.IsValidIndex(SlotIndex) ? HandCardPortraits[SlotIndex] : nullptr;
		UBorder* CardInfoStrip = HandCardInfoStrips.IsValidIndex(SlotIndex) ? HandCardInfoStrips[SlotIndex] : nullptr;
		const bool bHasCard = bCardBattleVisible && HandCardInstanceIds.IsValidIndex(SlotIndex);
		const bool bCanReceiveHandInput = bHasCard
			&& !bFixtureReadOnly
			&& State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player;
		if (CardButton)
		{
			CardButton->SetVisibility(bHasCard ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			// Player-phase cards remain hover-inspectable even when their individual
			// play preview fails. Enemy intent presentation disables every hand slot,
			// including an empty collapsed slot restored by a reconstructed board.
			CardButton->SetIsEnabled(bCanReceiveHandInput);
		}
		if (!bHasCard)
		{
			continue;
		}

		const FName CardInstanceId = HandCardInstanceIds[SlotIndex];
		const FGameXXKCardInstance* CardInstance = State->CardRun.ActiveBattle.Deck.Hand.FindByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == CardInstanceId;
		});
		const FGameXXKCardDefinition* Definition = CardInstance ? FGameXXKCardCatalog::FindCardDefinition(CardInstance->CardId) : nullptr;
		FGameXXKCardPlayPreview Preview;
		FString Error;
		const bool bPreviewBuilt = CardInstance
			&& FGameXXKCardBattleAdapter::BuildCardPlayPreview(*State, CardInstanceId, Preview, &Error);
		const bool bCanPlay = bPreviewBuilt && Preview.bCanPlay && !IsCardTargetingActive();
		const bool bSelectedForTargeting = IsCardTargetingActive() && PendingCardPreview.CardInstanceId == CardInstanceId;
		ApplyCardPresentation(CardButton, CardLabel, CardPortrait, CardInfoStrip, Definition);
		if (CardButton)
		{
			CardButton->SetRenderOpacity((bCanPlay || bSelectedForTargeting) ? 1.0f : 0.58f);
		}
		if (CardLabel)
		{
			const FString DisplayName = Definition ? Definition->DisplayName.ToString() : CardInstanceId.ToString();
			const int32 Energy = bPreviewBuilt ? Preview.EffectiveEnergyCost : (Definition ? Definition->EnergyCost : 0);
			const int32 Mana = bPreviewBuilt ? Preview.EffectiveManaCost : (Definition ? Definition->ManaCost : 0);
			CardLabel->SetText(FText::FromString(FString::Printf(TEXT("%s\n%d 气 / %d 内"), *DisplayName, Energy, Mana)));
		}
	}
}

void UGameXXKBattleBoardWidget::RefreshPartyQiWidget()
{
	if (!PartyQiWidget)
	{
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bShouldShowPartyQi = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		&& !HasPendingRouteReward();
	const FVector2D CanvasSize = RootCanvas ? RootCanvas->GetCachedGeometry().GetLocalSize() : FVector2D::ZeroVector;
	LastPartyQiCanvasSize = CanvasSize;
	if (!bShouldShowPartyQi)
	{
		PartyQiWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// CardRun.ActiveBattle.Deck.SharedEnergy is the sole authority for the player team's
	// current-turn Qi.  Do not project actor MP, a rules cache, or a save-game surrogate here.
	PartyQiWidget->SetSharedQi(State->CardRun.ActiveBattle.Deck.SharedEnergy);
	PartyQiWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* PartyQiSlot = Cast<UCanvasPanelSlot>(PartyQiWidget->Slot))
	{
		const FGameXXKBattlePartyQiLayout Layout = ResolvePartyQiLayout(CanvasSize);
		PartyQiSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
		PartyQiSlot->SetOffsets(Layout.SlotOffsets);
		PartyQiSlot->SetAlignment(FVector2D::ZeroVector);
		PartyQiSlot->SetZOrder(PartyQiWidgetZOrder);
	}
}

FText UGameXXKBattleBoardWidget::ResolveProjectedUnitHudDisplayName(const FName UnitId) const
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || UnitId.IsNone())
	{
		return FText::FromName(UnitId);
	}

	const auto ResolveLegacyName = [UnitId](const TArray<FGameXXKBattleRuntimeUnit>& Units) -> const FText*
	{
		const FGameXXKBattleRuntimeUnit* const LegacyUnit = Units.FindByPredicate([UnitId](const FGameXXKBattleRuntimeUnit& Candidate)
		{
			return Candidate.Id == UnitId;
		});
		return LegacyUnit ? &LegacyUnit->DisplayName : nullptr;
	};
	if (const FText* const PartyName = ResolveLegacyName(State->ActiveBattleParty))
	{
		return *PartyName;
	}
	if (const FText* const EnemyName = ResolveLegacyName(State->ActiveBattleEnemies))
	{
		return *EnemyName;
	}
	return FText::FromName(UnitId);
}

FBox2D UGameXXKBattleBoardWidget::ResolveExpandedHandRect(const FVector2D CanvasSize) const
{
	const UCanvasPanelSlot* const HandSlot = HandCardBox ? Cast<UCanvasPanelSlot>(HandCardBox->Slot) : nullptr;
	const FAnchors HandAnchors = HandSlot ? HandSlot->GetAnchors() : FAnchors(0.5f, 1.0f, 0.5f, 1.0f);
	const FMargin HandOffsets = HandSlot
		? HandSlot->GetOffsets()
		: FMargin(-PlayerHandRowSize.X * 0.5f, -277.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y);
	const FVector2D HandAlignment = HandSlot ? HandSlot->GetAlignment() : FVector2D::ZeroVector;
	FBox2D ExpandedHandRect = ResolveCanvasSlotRect(HandAnchors, HandOffsets, HandAlignment, CanvasSize);
	if (ExpandedHandRect.bIsValid)
	{
		const float HorizontalExpansion = PlayerHandCardSize.X * (PlayerHandSelectedScale - 1.0f) * 0.5f;
		const float VerticalExpansion = PlayerHandCardSize.Y * (PlayerHandSelectedScale - 1.0f) + FMath::Abs(PlayerHandSelectedLift);
		ExpandedHandRect.Min.X -= HorizontalExpansion;
		ExpandedHandRect.Max.X += HorizontalExpansion;
		ExpandedHandRect.Min.Y -= VerticalExpansion;
	}
	return ExpandedHandRect;
}

FGameXXKBattlePartyQiLayout UGameXXKBattleBoardWidget::ResolvePartyQiLayout(const FVector2D CanvasSize) const
{
	const UCanvasPanelSlot* HandSlot = HandCardBox ? Cast<UCanvasPanelSlot>(HandCardBox->Slot) : nullptr;
	const UCanvasPanelSlot* EndTurnSlot = EndTurnButton ? Cast<UCanvasPanelSlot>(EndTurnButton->Slot) : nullptr;
	const FMargin HandOffsets = HandSlot
		? HandSlot->GetOffsets()
		: FMargin(-PlayerHandRowSize.X * 0.5f, -277.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y);
	const FAnchors EndTurnAnchors = EndTurnSlot ? EndTurnSlot->GetAnchors() : FAnchors(1.0f, 1.0f, 1.0f, 1.0f);
	const FMargin EndTurnOffsets = EndTurnSlot
		? EndTurnSlot->GetOffsets()
		: FMargin(-230.0f, -138.0f, 190.0f, 62.0f);
	const FVector2D EndTurnAlignment = EndTurnSlot ? EndTurnSlot->GetAlignment() : FVector2D::ZeroVector;

	FGameXXKBattlePartyQiLayout Layout;
	const float CenteredQiLeft = EndTurnOffsets.Left + FMath::Max(0.0f, (EndTurnOffsets.Right - PartyQiWidgetSize.X) * 0.5f);
	const float QiTopAboveEndTurn = EndTurnOffsets.Top - PartyQiHandSafetyGap - PartyQiWidgetSize.Y;
	Layout.SlotOffsets = FMargin(CenteredQiLeft, QiTopAboveEndTurn, PartyQiWidgetSize.X, PartyQiWidgetSize.Y);
	Layout.EndTurnRect = ResolveCanvasSlotRect(EndTurnAnchors, EndTurnOffsets, EndTurnAlignment, CanvasSize);
	Layout.ExpandedHandRect = ResolveExpandedHandRect(CanvasSize);

	if (CanvasSize.X <= 0.0f || CanvasSize.Y <= 0.0f)
	{
		// Cached geometry is unavailable during initial native construction.  Keep the
		// right rail in the conservative hand-safe position until the next layout pass.
		const float VerticalExpansion = PlayerHandCardSize.Y * (PlayerHandSelectedScale - 1.0f) + FMath::Abs(PlayerHandSelectedLift);
		Layout.SlotOffsets.Top = HandOffsets.Top - VerticalExpansion - PartyQiHandSafetyGap - PartyQiWidgetSize.Y;
		Layout.bUsesHandSafeFallback = true;
		return Layout;
	}

	Layout.QiRect = ResolveCanvasSlotRect(
		FAnchors(1.0f, 1.0f, 1.0f, 1.0f),
		Layout.SlotOffsets,
		FVector2D::ZeroVector,
		CanvasSize);
	if (DoRectsOverlap(Layout.QiRect, Layout.ExpandedHandRect) || DoRectsOverlap(Layout.QiRect, Layout.EndTurnRect))
	{
		float SafeTop = CanvasSize.Y + Layout.SlotOffsets.Top;
		if (Layout.ExpandedHandRect.bIsValid)
		{
			SafeTop = Layout.ExpandedHandRect.Min.Y;
		}
		if (Layout.EndTurnRect.bIsValid)
		{
			SafeTop = FMath::Min(SafeTop, Layout.EndTurnRect.Min.Y);
		}
		Layout.SlotOffsets.Top = SafeTop - PartyQiHandSafetyGap - PartyQiWidgetSize.Y - CanvasSize.Y;
		Layout.bUsesHandSafeFallback = true;
		Layout.QiRect = ResolveCanvasSlotRect(
			FAnchors(1.0f, 1.0f, 1.0f, 1.0f),
			Layout.SlotOffsets,
			FVector2D::ZeroVector,
			CanvasSize);
	}
	return Layout;
}

#if WITH_DEV_AUTOMATION_TESTS
FGameXXKBattlePartyQiLayout UGameXXKBattleBoardWidget::ResolvePartyQiLayoutForTest(const FVector2D CanvasSize) const
{
	return ResolvePartyQiLayout(CanvasSize);
}
#endif

void UGameXXKBattleBoardWidget::RefreshCardTooltip()
{
	if (!HandCardDetailPanel || !HandCardDetailBody)
	{
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FGameXXKCardDefinition* Definition = nullptr;
	EGameXXKCardQuality TooltipQuality = EGameXXKCardQuality::Invalid;
	const FGameXXKCardPlayPreview* PreviewForText = nullptr;
	FGameXXKCardPlayPreview Preview;
	FGameXXKCardTooltipContext Context;
	if (!State)
	{
		HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	switch (HoveredCardTooltipSource)
	{
	case ECardTooltipSource::Hand:
	{
		if (!HandCardInstanceIds.IsValidIndex(HoveredHandCardSlot))
		{
			break;
		}
		const FName CardInstanceId = HandCardInstanceIds[HoveredHandCardSlot];
		const FGameXXKCardInstance* CardInstance = State->CardRun.ActiveBattle.Deck.Hand.FindByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == CardInstanceId;
		});
		Definition = CardInstance ? FGameXXKCardCatalog::FindCardDefinition(CardInstance->CardId) : nullptr;
		TooltipQuality = CardInstance ? CardInstance->CurrentQuality : EGameXXKCardQuality::Invalid;
		FString Error;
		if (Definition && !FGameXXKCardBattleAdapter::BuildCardPlayPreview(*State, CardInstanceId, Preview, &Error) && !Error.IsEmpty())
		{
			Preview.FailureReason = Error;
		}
		if (Definition)
		{
			PreviewForText = &Preview;
			Context = BuildHandTooltipContext(Preview);
		}
		break;
	}
	case ECardTooltipSource::PendingChoice:
	{
		const FGameXXKPendingCardChoice& PendingChoice = State->CardRun.ActiveBattle.Deck.PendingChoice;
		const FGameXXKCardInstance* Candidate = PendingChoice.Kind == HoveredPendingChoiceKind
			? PendingChoice.Candidates.FindByPredicate([this](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == HoveredCardTooltipId;
			})
			: nullptr;
		Definition = Candidate ? FGameXXKCardCatalog::FindCardDefinition(Candidate->CardId) : nullptr;
		TooltipQuality = Candidate ? Candidate->CurrentQuality : EGameXXKCardQuality::Invalid;
		if (Definition)
		{
			Context.InteractionResult = HoveredPendingChoiceKind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
				? TEXT("点击后加入手牌。")
				: TEXT("点击后弃置此牌。");
		}
		break;
	}
	case ECardTooltipSource::Reward:
	{
		if (GetPendingRouteRewardCardIds().Contains(HoveredCardTooltipId))
		{
			Definition = FGameXXKCardCatalog::FindCardDefinition(HoveredCardTooltipId);
			TooltipQuality = Definition ? Definition->BaseQuality : EGameXXKCardQuality::Invalid;
			Context.InteractionResult = TEXT("点击后加入临时路线卡组；满位时选择要替换的路线牌。");
		}
		break;
	}
	case ECardTooltipSource::RouteReplacement:
	{
		if (GetRouteRewardReplacementEntryIds().Contains(HoveredCardTooltipId))
		{
			const FGameXXKRouteCardEntry* Entry = State->CardRun.RouteCardEntries.FindByPredicate([this](const FGameXXKRouteCardEntry& Candidate)
			{
				return Candidate.EntryId == HoveredCardTooltipId;
			});
			Definition = Entry ? FGameXXKCardCatalog::FindCardDefinition(Entry->CardId) : nullptr;
			TooltipQuality = Entry ? Entry->CurrentQuality : EGameXXKCardQuality::Invalid;
			if (Definition)
			{
				Context.InteractionResult = TEXT("点击后作为被替换的临时路线牌。");
			}
		}
		break;
	}
	default:
		break;
	}

	if (!Definition)
	{
		HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	HandCardDetailBody->SetText(FText::FromString(
		TooltipQuality == EGameXXKCardQuality::Invalid
			? GameXXKCardText::DescribeTooltip(*Definition, PreviewForText, Context)
			: GameXXKCardText::DescribeTooltip(*Definition, TooltipQuality, PreviewForText, Context)));
	// The panel must never swallow the button's leave/click events while it overlaps the hand.
	HandCardDetailPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGameXXKBattleBoardWidget::RefreshEnemyIntentCards()
{
	VisibleEnemyIntentIndices.Reset();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FGameXXKCardRunState* Run = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		&& (State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player
			|| State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
		? &State->CardRun
		: nullptr;
	if (Run)
	{
		TArray<int32> OrderedIntentIndices;
		for (int32 IntentIndex = 0; IntentIndex < Run->EnemyIntents.Num(); ++IntentIndex)
		{
			const FGameXXKCardEnemyIntent& Intent = Run->EnemyIntents[IntentIndex];
			const bool bHasValidSource = Run->ActiveBattle.Units.ContainsByPredicate([&Intent](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == Intent.SourceUnitId
					&& Unit.bLiving
					&& Unit.Side == EGameXXKCardTargetSide::Enemy;
			});
			if (!Intent.SourceUnitId.IsNone()
				&& bHasValidSource
				&& !ResolveEnemyIntentSourceSlotLabel(Intent).IsEmpty()
				&& !ResolveEnemyIntentTargetSlotLabel(Intent).IsEmpty())
			{
				OrderedIntentIndices.Add(IntentIndex);
			}
		}
		OrderedIntentIndices.Sort([Run](const int32 LeftIndex, const int32 RightIndex)
		{
			const FGameXXKCardEnemyIntent& Left = Run->EnemyIntents[LeftIndex];
			const FGameXXKCardEnemyIntent& Right = Run->EnemyIntents[RightIndex];
			return Left.SourceSlotNumber != Right.SourceSlotNumber
				? Left.SourceSlotNumber < Right.SourceSlotNumber
				: LeftIndex < RightIndex;
		});
		for (const int32 IntentIndex : OrderedIntentIndices)
		{
			if (VisibleEnemyIntentIndices.Num() >= MaximumVisibleEnemyIntentCards)
			{
				break;
			}
			VisibleEnemyIntentIndices.Add(IntentIndex);
		}
	}

	if (EnemyIntentCardBox)
	{
		EnemyIntentCardBox->SetVisibility(VisibleEnemyIntentIndices.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (!VisibleEnemyIntentIndices.IsValidIndex(HoveredEnemyIntentSlot))
	{
		HoveredEnemyIntentSlot = INDEX_NONE;
	}

	for (int32 VisibleSlotIndex = 0; VisibleSlotIndex < EnemyIntentCardButtons.Num(); ++VisibleSlotIndex)
	{
		UButton* IntentCardButton = EnemyIntentCardButtons[VisibleSlotIndex];
		UTextBlock* SideLabel = EnemyIntentSlotLabels.IsValidIndex(VisibleSlotIndex) ? EnemyIntentSlotLabels[VisibleSlotIndex] : nullptr;
		UTextBlock* CardBody = EnemyIntentCardBodies.IsValidIndex(VisibleSlotIndex) ? EnemyIntentCardBodies[VisibleSlotIndex] : nullptr;
		const int32 PersistentIntentIndex = GetEnemyIntentPersistentIndexForVisibleSlot(VisibleSlotIndex);
		const bool bHasIntent = Run && Run->EnemyIntents.IsValidIndex(PersistentIntentIndex);
		if (IntentCardButton)
		{
			IntentCardButton->SetVisibility(bHasIntent ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			// These cards expose only their hover explanation; they never dispatch a player action.
			IntentCardButton->SetIsEnabled(bHasIntent);
		}
		if (!bHasIntent)
		{
			continue;
		}

		const FGameXXKCardEnemyIntent& Intent = Run->EnemyIntents[PersistentIntentIndex];
		const bool bCurrentIntent = IsEnemyIntentPresentationActive()
			&& PersistentIntentIndex == ActiveEnemyIntentPresentationIndex;
		if (SideLabel)
		{
			SideLabel->SetText(FText::FromString(ResolveEnemyIntentSourceSlotLabel(Intent)));
			SideLabel->SetRenderOpacity(bCurrentIntent ? 1.0f : 0.68f);
		}
		if (CardBody)
		{
			CardBody->SetText(FText::FromString(BuildEnemyIntentCardBody(*State, Intent)));
		}
		if (IntentCardButton)
		{
			IntentCardButton->SetToolTipText(FText::GetEmpty());
			IntentCardButton->SetRenderOpacity(bCurrentIntent ? 1.0f : 0.70f);
			IntentCardButton->SetRenderScale(bCurrentIntent ? FVector2D(1.06f, 1.06f) : FVector2D(1.0f, 1.0f));
		}
	}
}

void UGameXXKBattleBoardWidget::RefreshEnemyIntentShowcase()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FGameXXKCardRunState* Run = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		&& State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy
		? &State->CardRun
		: nullptr;
	const bool bShowShowcase = IsEnemyIntentPresentationActive()
		&& Run
		&& Run->EnemyIntents.IsValidIndex(ActiveEnemyIntentPresentationIndex);
	if (EnemyIntentShowcaseCard)
	{
		EnemyIntentShowcaseCard->SetVisibility(bShowShowcase ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		EnemyIntentShowcaseCard->SetRenderOpacity(EnemyIntentPresentationState == EGameXXKEnemyIntentPresentationState::Reveal ? 0.88f : 1.0f);
	}
	if (bShowShowcase && EnemyIntentShowcaseBody)
	{
		EnemyIntentShowcaseBody->SetText(FText::FromString(BuildEnemyIntentCardBody(
			*State,
			Run->EnemyIntents[ActiveEnemyIntentPresentationIndex])));
	}
}

void UGameXXKBattleBoardWidget::RefreshEnemyIntentRecoveryControl()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bCanRetry = bEnemyIntentCompletionRecoveryPending
		&& Subsystem
		&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle
		&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		&& Subsystem->GetRuntimeState().CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy;
	if (EnemyIntentRecoveryButton)
	{
		EnemyIntentRecoveryButton->SetVisibility(bCanRetry ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EnemyIntentRecoveryButton->SetIsEnabled(bCanRetry);
	}
}

void UGameXXKBattleBoardWidget::RefreshEnemyIntentDetail()
{
	if (!EnemyIntentDetailPanel || !EnemyIntentDetailBody)
	{
		return;
	}
	const FString Tooltip = GetEnemyIntentTooltipForTest(HoveredEnemyIntentSlot);
	if (Tooltip.IsEmpty())
	{
		EnemyIntentDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	EnemyIntentDetailBody->SetText(FText::FromString(Tooltip));
	// It overlaps presentation space but must not intercept card-hover transitions.
	EnemyIntentDetailPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGameXXKBattleBoardWidget::SetEnemyIntentHoverState(const int32 VisibleSlotIndex, const bool bHovered)
{
	if (!VisibleEnemyIntentIndices.IsValidIndex(VisibleSlotIndex))
	{
		return;
	}
	if (bHovered)
	{
		HoveredEnemyIntentSlot = VisibleSlotIndex;
	}
	else if (HoveredEnemyIntentSlot == VisibleSlotIndex)
	{
		HoveredEnemyIntentSlot = INDEX_NONE;
	}
	RefreshEnemyIntentDetail();
}

bool UGameXXKBattleBoardWidget::IsEnemyIntentPresentationActive() const
{
	return EnemyIntentPresentationState != EGameXXKEnemyIntentPresentationState::None;
}

void UGameXXKBattleBoardWidget::ResetEnemyIntentPresentationState()
{
	EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;
	EnemyIntentPresentationElapsed = 0.0f;
	ActiveEnemyIntentPresentationIndex = INDEX_NONE;
	HoveredEnemyIntentSlot = INDEX_NONE;
	bEnemyIntentCompletionRecoveryPending = false;
}

void UGameXXKBattleBoardWidget::BeginEnemyIntentPresentation()
{
	if (IsBattleHudFixtureReadOnly())
	{
		ResetEnemyIntentPresentationState();
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State
		|| !State->CardRun.bHasActiveCardBattle
		|| State->CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		ResetEnemyIntentPresentationState();
		return;
	}

	ActiveEnemyIntentPresentationIndex = State->CardRun.NextEnemyIntentIndex;
	EnemyIntentPresentationElapsed = 0.0f;
	HoveredEnemyIntentSlot = INDEX_NONE;
	bEnemyIntentCompletionRecoveryPending = false;
	EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Reveal;
}

bool UGameXXKBattleBoardWidget::ResolveCurrentEnemyIntentPresentation()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("敌方意图结算时战斗状态已丢失。");
		return false;
	}

	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const int32 IntentIndexBeforeResolve = MutableState.CardRun.NextEnemyIntentIndex;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> DamageResults;
	bool bIntentsFinished = false;
	FString Error;
	if (!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
		MutableState,
		ResolvedIntent,
		DamageResults,
		bIntentsFinished,
		&Error))
	{
		LastCardInteractionError = Error;
		const int32 IntentIndexAfterResolve = MutableState.CardRun.NextEnemyIntentIndex;
		if (IntentIndexAfterResolve != IntentIndexBeforeResolve)
		{
			if (IntentIndexAfterResolve > IntentIndexBeforeResolve)
			{
				LastCardInteractionError += TEXT("\n敌方意图已结算，投影同步失败；将继续完成该敌方回合。");
			}
			else
			{
				LastCardInteractionError += TEXT("\n敌方意图索引在失败后发生变化；将继续完成该敌方回合。");
			}
			// ResolveNextEnemyIntent can apply the direct attack and consume the saved
			// intent before legacy projection sync reports failure. Never skip again.
			return true;
		}
		FString SkipError;
		if (!FGameXXKCardBattleAdapter::SkipCurrentEnemyIntent(MutableState, &SkipError))
		{
			LastCardInteractionError += FString::Printf(TEXT("\n敌方意图恢复失败：%s"), *SkipError);
			return false;
		}
		LastCardInteractionError += TEXT("\n已跳过该异常敌方意图，继续结算。");
		return true;
	}
	LastCardInteractionError.Reset();
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->RefreshBattleSceneAfterCardMutation(ResolvedIntent.SourceUnitId, DamageResults);
	}
	return true;
}

bool UGameXXKBattleBoardWidget::CompleteEnemyIntentPresentation()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("敌方意图完成时战斗状态已丢失。");
		return false;
	}

	TArray<FGameXXKCardDamageResult> DamageResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(Subsystem->GetMutableRuntimeState(), DamageResults, &Error))
	{
		LastCardInteractionError = Error;
		bEnemyIntentCompletionRecoveryPending = true;
		EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Settle;
		EnemyIntentPresentationElapsed = 0.0f;
		return false;
	}
	EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;
	EnemyIntentPresentationElapsed = 0.0f;
	ActiveEnemyIntentPresentationIndex = INDEX_NONE;
	HoveredEnemyIntentSlot = INDEX_NONE;
	bEnemyIntentCompletionRecoveryPending = false;
	LastCardInteractionError.Reset();
	return ResolveAndRefreshCardBattleAfterMutation();
}

bool UGameXXKBattleBoardWidget::RetryEnemyIntentCompletion()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	if (!bEnemyIntentCompletionRecoveryPending)
	{
		return false;
	}
	bEnemyIntentCompletionRecoveryPending = false;
	if (!CompleteEnemyIntentPresentation())
	{
		RefreshProgrammaticLayout();
		return false;
	}
	return true;
}

void UGameXXKBattleBoardWidget::AdvanceEnemyIntentPresentation(float InDeltaTime)
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bCanAdvanceRawEnemyPresentation = Subsystem
		&& !Subsystem->IsBattleHudFixtureActiveForTest()
		&& State
		&& State->CardRun.bHasActiveCardBattle
		&& State->CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy;
	if (!bCanAdvanceRawEnemyPresentation)
	{
		const bool bHadLocalPresentation = IsEnemyIntentPresentationActive()
			|| ActiveEnemyIntentPresentationIndex != INDEX_NONE
			|| bEnemyIntentCompletionRecoveryPending;
		ResetEnemyIntentPresentationState();
		if (bHadLocalPresentation)
		{
			RefreshProgrammaticLayout();
		}
		return;
	}

	if (!IsEnemyIntentPresentationActive())
	{
		return;
	}

	float RemainingDelta = FMath::Max(0.0f, InDeltaTime);
	while (IsEnemyIntentPresentationActive())
	{
		float StateDuration = 0.0f;
		switch (EnemyIntentPresentationState)
		{
		case EGameXXKEnemyIntentPresentationState::Reveal: StateDuration = EnemyIntentRevealDuration; break;
		case EGameXXKEnemyIntentPresentationState::Resolve: StateDuration = EnemyIntentResolveDuration; break;
		case EGameXXKEnemyIntentPresentationState::Settle: StateDuration = EnemyIntentSettleDuration; break;
		default: return;
		}
		const float TimeToBoundary = FMath::Max(0.0f, StateDuration - EnemyIntentPresentationElapsed);
		if (RemainingDelta + KINDA_SMALL_NUMBER < TimeToBoundary)
		{
			EnemyIntentPresentationElapsed += RemainingDelta;
			break;
		}

		RemainingDelta = FMath::Max(0.0f, RemainingDelta - TimeToBoundary);
		EnemyIntentPresentationElapsed = 0.0f;
		if (EnemyIntentPresentationState == EGameXXKEnemyIntentPresentationState::Reveal)
		{
			EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Resolve;
		}
		else if (EnemyIntentPresentationState == EGameXXKEnemyIntentPresentationState::Resolve)
		{
			if (!ResolveCurrentEnemyIntentPresentation())
			{
				// Keep the saved intent and its presentation visible for another explicit adapter attempt.
				EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Reveal;
				EnemyIntentPresentationElapsed = 0.0f;
				RefreshProgrammaticLayout();
				return;
			}
			EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Settle;
		}
		else if (EnemyIntentPresentationState == EGameXXKEnemyIntentPresentationState::Settle)
		{
			const UGameXXKMVPSubsystem* const CompletionSubsystem = ResolveMVPSubsystem();
			const FGameXXKRuntimeState* const CompletionState = CompletionSubsystem
				? &CompletionSubsystem->GetRuntimeState()
				: nullptr;
			const bool bMustComplete = CompletionState
				&& CompletionState->CardRun.bHasActiveCardBattle
				&& (CompletionState->CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy
					|| CompletionState->CardRun.NextEnemyIntentIndex >= CompletionState->CardRun.EnemyIntents.Num());
			if (bMustComplete)
			{
				if (bEnemyIntentCompletionRecoveryPending)
				{
					RefreshProgrammaticLayout();
					return;
				}
				if (!CompleteEnemyIntentPresentation())
				{
					RefreshProgrammaticLayout();
				}
				return;
			}
			if (!CompletionState || !CompletionState->CardRun.bHasActiveCardBattle)
			{
				EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::None;
				ActiveEnemyIntentPresentationIndex = INDEX_NONE;
				RefreshProgrammaticLayout();
				return;
			}
			ActiveEnemyIntentPresentationIndex = CompletionState->CardRun.NextEnemyIntentIndex;
			EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Reveal;
		}

		if (RemainingDelta <= KINDA_SMALL_NUMBER)
		{
			break;
		}
	}
	RefreshProgrammaticLayout();
}

void UGameXXKBattleBoardWidget::SetHandCardHoverState(int32 SlotIndex, bool bHovered)
{
	if (!HandCardButtons.IsValidIndex(SlotIndex))
	{
		return;
	}
	if (bHovered)
	{
		HoveredHandCardSlot = SlotIndex;
		HoveredCardTooltipSource = ECardTooltipSource::Hand;
		HoveredCardTooltipId = NAME_None;
		HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
	}
	else if (HoveredCardTooltipSource == ECardTooltipSource::Hand && HoveredHandCardSlot == SlotIndex)
	{
		HoveredHandCardSlot = INDEX_NONE;
		HoveredCardTooltipSource = ECardTooltipSource::None;
	}
	RefreshCardTooltip();
}

void UGameXXKBattleBoardWidget::ClearCardTooltipHoverState()
{
	HoveredCardTooltipSource = ECardTooltipSource::None;
	HoveredHandCardSlot = INDEX_NONE;
	HoveredCardTooltipId = NAME_None;
	HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
	for (UButton* CardButton : HandCardButtons)
	{
		if (CardButton)
		{
			CardButton->SetRenderTransform(FWidgetTransform());
		}
	}
	if (HandCardDetailPanel)
	{
		HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HandCardDetailBody)
	{
		HandCardDetailBody->SetText(FText::GetEmpty());
	}
}

void UGameXXKBattleBoardWidget::SetRewardCardHoverState(const int32 SlotIndex, const bool bHovered)
{
	if (!PendingRewardCardIds.IsValidIndex(SlotIndex))
	{
		return;
	}
	const FName RewardCardId = PendingRewardCardIds[SlotIndex];
	if (bHovered)
	{
		HoveredCardTooltipSource = ECardTooltipSource::Reward;
		HoveredCardTooltipId = RewardCardId;
		HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
	}
	else if (HoveredCardTooltipSource == ECardTooltipSource::Reward && HoveredCardTooltipId == RewardCardId)
	{
		HoveredCardTooltipSource = ECardTooltipSource::None;
		HoveredCardTooltipId = NAME_None;
	}
	RefreshCardTooltip();
}

void UGameXXKBattleBoardWidget::AdvanceHandCardHoverMotion(float InDeltaTime)
{
	const int32 SelectedCardSlot = IsCardTargetingActive()
		? HandCardInstanceIds.IndexOfByKey(PendingCardPreview.CardInstanceId)
		: INDEX_NONE;
	const bool bCardTargeting = SelectedCardSlot != INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < HandCardButtons.Num(); ++SlotIndex)
	{
		UButton* CardButton = HandCardButtons[SlotIndex];
		if (!CardButton)
		{
			continue;
		}

		const bool bSelected = SlotIndex == SelectedCardSlot;
		const bool bHovered = !bCardTargeting && SlotIndex == HoveredHandCardSlot;
		const float TargetScale = bSelected ? PlayerHandSelectedScale : (bHovered ? 1.16f : 1.0f);
		const float TargetLift = bSelected ? PlayerHandSelectedLift : (bHovered ? -26.0f : 0.0f);
		const FWidgetTransform CurrentTransform = CardButton->GetRenderTransform();
		const float NextScale = FMath::FInterpTo(CurrentTransform.Scale.X, TargetScale, InDeltaTime, 16.0f);
		const float NextLift = FMath::FInterpTo(CurrentTransform.Translation.Y, TargetLift, InDeltaTime, 18.0f);
		CardButton->SetRenderScale(FVector2D(NextScale, NextScale));
		CardButton->SetRenderTranslation(FVector2D(0.0f, NextLift));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKBattleBoardWidget::AdvanceHandCardHoverMotionForTest(float InDeltaTime)
{
	AdvanceHandCardHoverMotion(InDeltaTime);
}

void UGameXXKBattleBoardWidget::AdvanceEnemyIntentPresentationForTest(float InDeltaTime)
{
	AdvanceEnemyIntentPresentation(InDeltaTime);
}
#endif

void UGameXXKBattleBoardWidget::RefreshPendingCardChoices()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bFixtureReadOnly = Subsystem && Subsystem->IsBattleHudFixtureActiveForTest();
	const FGameXXKPendingCardChoice* PendingChoice = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		? &State->CardRun.ActiveBattle.Deck.PendingChoice
		: nullptr;
	const bool bShowInsight = PendingChoice && PendingChoice->Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand;
	const bool bShowForcedDiscard = PendingChoice && PendingChoice->Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard;
	const bool bShowPendingChoice = bShowInsight || bShowForcedDiscard;
	if (PendingChoicePanel)
	{
		PendingChoicePanel->SetVisibility(bShowPendingChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (PendingChoiceCancelButton)
	{
		const bool bCanCancelInsight = bShowInsight && PendingChoice->bCanCancel;
		PendingChoiceCancelButton->SetVisibility(bCanCancelInsight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PendingChoiceCancelButton->SetIsEnabled(bCanCancelInsight && !bFixtureReadOnly);
	}
	if (PendingChoicePromptText && bShowPendingChoice)
	{
		const int32 RequiredDiscardCount = PendingChoice->RequiredDiscardCount > 0
			? PendingChoice->RequiredDiscardCount
			: PendingChoice->RequiredCount;
		const FString Prompt = bShowInsight
			? TEXT("洞察：选择一张加入手牌")
			: FString::Printf(TEXT("此牌要求弃置 %d 张手牌"), FMath::Max(1, RequiredDiscardCount));
		PendingChoicePromptText->SetText(FText::FromString(Prompt));
	}
	if (PendingChoiceCardBox)
	{
		const int32 VisibleCandidateCount = bShowPendingChoice
			? FMath::Min(PendingChoice->Candidates.Num(), MaximumVisiblePendingCardChoices)
			: 0;
		const float RowWidth = static_cast<float>(VisibleCandidateCount) * 119.0f;
		if (UCanvasPanelSlot* PendingCardsSlot = Cast<UCanvasPanelSlot>(PendingChoiceCardBox->Slot))
		{
			PendingCardsSlot->SetOffsets(FMargin(-RowWidth * 0.5f, 39.0f, RowWidth, 129.0f));
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < PendingChoiceCardButtons.Num(); ++SlotIndex)
	{
		UButton* CardButton = PendingChoiceCardButtons[SlotIndex];
		UTextBlock* CardLabel = PendingChoiceCardLabels.IsValidIndex(SlotIndex) ? PendingChoiceCardLabels[SlotIndex] : nullptr;
		UImage* CardPortrait = PendingChoiceCardPortraits.IsValidIndex(SlotIndex) ? PendingChoiceCardPortraits[SlotIndex] : nullptr;
		UBorder* CardInfoStrip = PendingChoiceCardInfoStrips.IsValidIndex(SlotIndex) ? PendingChoiceCardInfoStrips[SlotIndex] : nullptr;
		const bool bHasCandidate = bShowPendingChoice && PendingChoice->Candidates.IsValidIndex(SlotIndex);
		if (CardButton)
		{
			CardButton->SetVisibility(bHasCandidate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			CardButton->SetIsEnabled(bHasCandidate && !bFixtureReadOnly);
		}
		if (!bHasCandidate)
		{
			continue;
		}

		const FGameXXKCardInstance& Candidate = PendingChoice->Candidates[SlotIndex];
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Candidate.CardId);
		if (UGameXXKPendingChoiceCardButton* PendingChoiceButton = Cast<UGameXXKPendingChoiceCardButton>(CardButton))
		{
			PendingChoiceButton->Configure(this, Candidate.InstanceId, PendingChoice->Kind);
		}
		ApplyCardPresentation(CardButton, CardLabel, CardPortrait, CardInfoStrip, Definition);
		if (CardLabel)
		{
			const FString DisplayName = Definition ? Definition->DisplayName.ToString() : Candidate.CardId.ToString();
			CardLabel->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n%s"),
				*DisplayName,
				bShowInsight ? TEXT("加入手牌") : TEXT("点击弃置"))));
		}
	}
}

void UGameXXKBattleBoardWidget::RefreshPendingRewardChoices()
{
	const bool bFixtureReadOnly = IsBattleHudFixtureReadOnly();
	PendingRewardCardIds = GetPendingRouteRewardCardIds();
	const bool bShowRewards = PendingRewardCardIds.Num() > 0;
	if (!bShowRewards || !PendingRewardCardIds.Contains(RouteRewardCardIdAwaitingReplacement))
	{
		RouteRewardCardIdAwaitingReplacement = NAME_None;
		SelectedRouteRewardReplacementEntryId = NAME_None;
	}
	if (RewardCardBox)
	{
		RewardCardBox->SetVisibility(bShowRewards ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (SkipRewardButton)
	{
		SkipRewardButton->SetVisibility(bShowRewards ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		SkipRewardButton->SetIsEnabled(bShowRewards && !bFixtureReadOnly);
	}

	for (int32 SlotIndex = 0; SlotIndex < RewardCardButtons.Num(); ++SlotIndex)
	{
		UButton* RewardButton = RewardCardButtons[SlotIndex];
		UTextBlock* RewardLabel = RewardCardLabels.IsValidIndex(SlotIndex) ? RewardCardLabels[SlotIndex] : nullptr;
		UImage* RewardPortrait = RewardCardPortraits.IsValidIndex(SlotIndex) ? RewardCardPortraits[SlotIndex] : nullptr;
		UBorder* RewardInfoStrip = RewardCardInfoStrips.IsValidIndex(SlotIndex) ? RewardCardInfoStrips[SlotIndex] : nullptr;
		const bool bHasReward = PendingRewardCardIds.IsValidIndex(SlotIndex);
		if (RewardButton)
		{
			RewardButton->SetVisibility(bHasReward ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			RewardButton->SetIsEnabled(bHasReward && !bFixtureReadOnly);
		}
		if (!bHasReward)
		{
			continue;
		}

		const FName RewardCardId = PendingRewardCardIds[SlotIndex];
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RewardCardId);
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString PreviewError;
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const bool bPreviewValid = Subsystem
			&& FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
				Subsystem->GetRuntimeState(),
				RewardCardId,
				NAME_None,
				Preview,
				&PreviewError)
			&& (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::CanCommit
				|| Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
		if (RewardButton)
		{
			RewardButton->SetIsEnabled(bPreviewValid && !bFixtureReadOnly);
		}
		ApplyCardPresentation(RewardButton, RewardLabel, RewardPortrait, RewardInfoStrip, Definition);
		if (RewardLabel)
		{
			const FString DisplayName = Definition ? Definition->DisplayName.ToString() : RewardCardId.ToString();
			const int32 Energy = Definition ? Definition->EnergyCost : 0;
			const int32 Mana = Definition ? Definition->ManaCost : 0;
			const FString Quality = Definition
				? FGameXXKCardQualityRules::GetDisplayName(Definition->BaseQuality).ToString()
				: FString();
			RewardLabel->SetText(FText::FromString(FString::Printf(
				TEXT("%s\n[%s] %d 气 / %d 内"),
				*DisplayName,
				*Quality,
				Energy,
				Mana)));
		}
	}
	RefreshRouteRewardReplacementChoices();
}

void UGameXXKBattleBoardWidget::RefreshRouteRewardReplacementChoices()
{
	if (!RouteRewardReplacementScrollBox || !WidgetTree)
	{
		return;
	}

	RouteRewardReplacementScrollBox->ClearChildren();
	RouteRewardReplacementButtons.Reset();
	const TArray<FName> ReplaceableEntryIds = GetRouteRewardReplacementEntryIds();
	if (ReplaceableEntryIds.IsEmpty())
	{
		SelectedRouteRewardReplacementEntryId = NAME_None;
		RouteRewardReplacementScrollBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (!ReplaceableEntryIds.Contains(SelectedRouteRewardReplacementEntryId))
	{
		SelectedRouteRewardReplacementEntryId = NAME_None;
	}

	const bool bFixtureReadOnly = IsBattleHudFixtureReadOnly();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKCardRunState* Run = Subsystem ? &Subsystem->GetRuntimeState().CardRun : nullptr;
	RouteRewardReplacementScrollBox->SetVisibility(ESlateVisibility::Visible);
	for (int32 CardIndex = 0; CardIndex < ReplaceableEntryIds.Num(); ++CardIndex)
	{
		const FName EntryId = ReplaceableEntryIds[CardIndex];
		const FGameXXKRouteCardEntry* Entry = Run
			? Run->RouteCardEntries.FindByPredicate([EntryId](const FGameXXKRouteCardEntry& Candidate)
			{
				return Candidate.EntryId == EntryId;
			})
			: nullptr;
		const FGameXXKCardDefinition* Definition = Entry
			? FGameXXKCardCatalog::FindCardDefinition(Entry->CardId)
			: nullptr;
		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			*FString::Printf(TEXT("BattleRouteReplaceCardSize_%02d"), CardIndex));
		CardSizeBox->SetWidthOverride(RewardCardSize.X);
		CardSizeBox->SetHeightOverride(RewardCardSize.Y);
		UGameXXKRouteRewardReplacementButton* CardButton = WidgetTree->ConstructWidget<UGameXXKRouteRewardReplacementButton>(
			UGameXXKRouteRewardReplacementButton::StaticClass(),
			*FString::Printf(TEXT("BattleRouteReplaceCard_%02d"), CardIndex));
		CardButton->Configure(this, EntryId);
		StyleCardButton(CardButton, RewardCardSize);
		CardButton->SetIsEnabled(!bFixtureReadOnly);
		UTextBlock* Label = nullptr;
		UImage* Portrait = nullptr;
		UBorder* InfoStrip = nullptr;
		BuildCardFace(
			CardButton,
			FString::Printf(TEXT("BattleRouteReplaceCard_%02d"), CardIndex),
			Label,
			Portrait,
			InfoStrip);
		ApplyCardPresentation(CardButton, Label, Portrait, InfoStrip, Definition);
		if (Label)
		{
			const FString DisplayName = Definition ? Definition->DisplayName.ToString() : EntryId.ToString();
			const FString Prefix = EntryId == SelectedRouteRewardReplacementEntryId ? TEXT("替换\n") : TEXT("");
			const FString Quality = Entry
				? FGameXXKCardQualityRules::GetDisplayName(Entry->CurrentQuality).ToString()
				: FString();
			Label->SetText(FText::FromString(FString::Printf(TEXT("%s%s\n[%s]"), *Prefix, *DisplayName, *Quality)));
		}
		const bool bSelected = EntryId == SelectedRouteRewardReplacementEntryId;
		CardButton->SetRenderOpacity(bSelected ? 1.0f : 0.74f);
		CardSizeBox->AddChild(CardButton);
		RouteRewardReplacementScrollBox->AddChild(CardSizeBox);
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(CardSizeBox->Slot))
		{
			CardSlot->SetPadding(FMargin(3.0f, 3.0f, 9.0f, 3.0f));
			CardSlot->SetHorizontalAlignment(HAlign_Center);
		}
		RouteRewardReplacementButtons.Add(CardButton);
	}
}

void UGameXXKBattleBoardWidget::EnsureBattleVisualResourcesLoaded()
{
	if (!BattleActionInkButtonTexture)
	{
		BattleActionInkButtonTexture = LoadObject<UTexture2D>(nullptr, InkButtonTexturePath);
	}
	if (!BattleStatusWindowFrameTexture)
	{
		BattleStatusWindowFrameTexture = LoadObject<UTexture2D>(nullptr, BattleStatusWindowFrameTexturePath);
	}
	if (!CardFrameTexture)
	{
		CardFrameTexture = LoadObject<UTexture2D>(nullptr, CardFrameTexturePath);
	}
	if (!TargetingArrowHeadTexture)
	{
		TargetingArrowHeadTexture = LoadObject<UTexture2D>(nullptr, TargetingArrowHeadTexturePath);
	}
	if (TargetingInkDabTextures.Num() != TargetingInkDabCount)
	{
		TargetingInkDabTextures.Reset(TargetingInkDabCount);
		for (int32 DabIndex = 0; DabIndex < TargetingInkDabCount; ++DabIndex)
		{
			TargetingInkDabTextures.Add(LoadObject<UTexture2D>(nullptr, *BuildTargetingInkDabTexturePath(DabIndex)));
		}
	}
}

void UGameXXKBattleBoardWidget::StyleBattleActionButton(UButton* Button, FName ActionName)
{
	if (!Button)
	{
		return;
	}

	EnsureBattleVisualResourcesLoaded();
	const FLinearColor ActionTint = ResolveBattleActionButtonTint(ActionName);
	Button->SetBackgroundColor(FLinearColor::White);
	if (!BattleActionInkButtonTexture)
	{
		Button->SetBackgroundColor(ActionTint);
		return;
	}

	const FVector2D ButtonImageSize(360.0f, 74.0f);
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(BuildTextureBrush(BattleActionInkButtonTexture.Get(), ButtonImageSize, ActionTint));
	ButtonStyle.SetHovered(BuildTextureBrush(BattleActionInkButtonTexture.Get(), ButtonImageSize, FLinearColor(ActionTint.R, ActionTint.G, ActionTint.B, 1.0f)));
	ButtonStyle.SetPressed(BuildTextureBrush(BattleActionInkButtonTexture.Get(), ButtonImageSize, FLinearColor(ActionTint.R * 0.82f, ActionTint.G * 0.86f, ActionTint.B * 0.90f, 0.98f)));
	ButtonStyle.SetDisabled(BuildTextureBrush(BattleActionInkButtonTexture.Get(), ButtonImageSize, FLinearColor(0.42f, 0.46f, 0.44f, 0.52f)));
	ButtonStyle.SetNormalPadding(FMargin(42.0f, 10.0f, 42.0f, 10.0f));
	ButtonStyle.SetPressedPadding(FMargin(42.0f, 12.0f, 42.0f, 8.0f));
	Button->SetStyle(ButtonStyle);
}

void UGameXXKBattleBoardWidget::StyleCardButton(UButton* Button, const FVector2D& CardImageSize)
{
	if (!Button)
	{
		return;
	}

	EnsureBattleVisualResourcesLoaded();
	// 057 remains the locked parchment/ink frame. Ownership color belongs solely
	// to the lower information strip; readable hand-card motion is transform-only.
	Button->SetBackgroundColor(FLinearColor::White);
	if (!CardFrameTexture)
	{
		return;
	}

	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(BuildTextureBrush(CardFrameTexture.Get(), CardImageSize, FLinearColor::White));
	ButtonStyle.SetHovered(BuildTextureBrush(CardFrameTexture.Get(), CardImageSize, FLinearColor(1.0f, 0.962f, 0.874f, 1.0f)));
	ButtonStyle.SetPressed(BuildTextureBrush(CardFrameTexture.Get(), CardImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.88f)));
	ButtonStyle.SetDisabled(BuildTextureBrush(CardFrameTexture.Get(), CardImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.52f)));
	const bool bUsesFullFrameContent = CardImageSize.X >= EnemyIntentCardSize.X;
	const FMargin CardContentPadding = bUsesFullFrameContent ? FMargin(0.0f) : FMargin(10.0f, 12.0f, 10.0f, 12.0f);
	ButtonStyle.SetNormalPadding(CardContentPadding);
	ButtonStyle.SetPressedPadding(CardContentPadding);
	Button->SetStyle(ButtonStyle);
}

void UGameXXKBattleBoardWidget::BuildCardFace(
	UButton* CardButton,
	const FString& NamePrefix,
	UTextBlock*& OutLabel,
	UImage*& OutPortrait,
	UBorder*& OutInfoStrip,
	bool bUsePlayerHandSize)
{
	OutLabel = nullptr;
	OutPortrait = nullptr;
	OutInfoStrip = nullptr;
	if (!CardButton || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* FaceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		*FString::Printf(TEXT("%sFace"), *NamePrefix));
	CardButton->AddChild(FaceCanvas);

	UImage* Portrait = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("%sPortrait"), *NamePrefix));
	Portrait->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PortraitSlot = FaceCanvas->AddChildToCanvas(Portrait))
	{
		PortraitSlot->SetOffsets(bUsePlayerHandSize
			? FMargin(30.0f, 27.0f, 162.0f, 135.0f)
			: FMargin(16.0f, 14.0f, 81.0f, 68.0f));
		PortraitSlot->SetAlignment(FVector2D::ZeroVector);
	}

	UBorder* InfoStrip = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		*FString::Printf(TEXT("%sInfoStrip"), *NamePrefix));
	InfoStrip->SetPadding(FMargin(2.0f, 1.0f, 2.0f, 1.0f));
	InfoStrip->SetHorizontalAlignment(HAlign_Center);
	InfoStrip->SetVerticalAlignment(VAlign_Center);
	if (UCanvasPanelSlot* InfoStripSlot = FaceCanvas->AddChildToCanvas(InfoStrip))
	{
		InfoStripSlot->SetOffsets(bUsePlayerHandSize
			? FMargin(24.0f, 174.0f, 177.0f, 54.0f)
			: FMargin(12.0f, 87.0f, 89.0f, 27.0f));
		InfoStripSlot->SetAlignment(FVector2D::ZeroVector);
	}

	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("%sLabel"), *NamePrefix));
	Label->SetJustification(ETextJustify::Center);
	Label->SetAutoWrapText(false);
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.36f));
	Label->SetShadowOffset(FVector2D(0.5f, 0.5f));
	FSlateFontInfo CardFont = Label->GetFont();
	CardFont.Size = bUsePlayerHandSize ? 18 : 10;
	Label->SetFont(CardFont);
	InfoStrip->SetContent(Label);

	OutLabel = Label;
	OutPortrait = Portrait;
	OutInfoStrip = InfoStrip;
}

void UGameXXKBattleBoardWidget::BuildEnemyIntentCardFace(
	UButton* CardButton,
	const FString& NamePrefix,
	UTextBlock*& OutBody)
{
	OutBody = nullptr;
	if (!CardButton || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* FaceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		*FString::Printf(TEXT("%sFace"), *NamePrefix));
	CardButton->AddChild(FaceCanvas);

	UTextBlock* Body = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("%sBody"), *NamePrefix));
	Body->SetAutoWrapText(true);
	Body->SetJustification(ETextJustify::Center);
	Body->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	Body->SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.34f));
	Body->SetShadowOffset(FVector2D(0.5f, 0.5f));
	FSlateFontInfo BodyFont = Body->GetFont();
	BodyFont.Size = 11;
	BodyFont.TypefaceFontName = TEXT("Bold");
	Body->SetFont(BodyFont);
	if (UCanvasPanelSlot* BodySlot = FaceCanvas->AddChildToCanvas(Body))
	{
		BodySlot->SetOffsets(FMargin(16.0f, 16.0f, 118.0f, 138.0f));
		BodySlot->SetAlignment(FVector2D::ZeroVector);
	}
	OutBody = Body;
}

void UGameXXKBattleBoardWidget::ApplyCardPresentation(
	UButton* CardButton,
	UTextBlock* CardLabel,
	UImage* PortraitImage,
	UBorder* InfoStrip,
	const FGameXXKCardDefinition* Definition)
{
	const FLinearColor StripTint = Definition
		? ResolveCardInfoStripTint(*Definition)
		: FLinearColor(0.882f, 0.827f, 0.722f, 1.0f);
	if (InfoStrip)
	{
		InfoStrip->SetBrushColor(StripTint);
	}
	if (CardLabel)
	{
		CardLabel->SetColorAndOpacity(FSlateColor(Definition
			? ResolveCardInfoInkTint(*Definition)
			: FLinearColor(0.137f, 0.118f, 0.098f, 1.0f)));
	}
	if (!PortraitImage)
	{
		return;
	}
	UTexture2D* PortraitTexture = Definition ? ResolveCardPortraitTexture(*Definition) : nullptr;
	if (PortraitTexture)
	{
		PortraitImage->SetBrushFromTexture(PortraitTexture, true);
		PortraitImage->SetColorAndOpacity(FLinearColor::White);
		PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FString UGameXXKBattleBoardWidget::ResolveCardPortraitResourcePath(const FGameXXKCardDefinition& Definition) const
{
	if (Definition.Owner == EGameXXKCardOwner::Hero)
	{
		return HeroCardPortraitTexturePath;
	}
	if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
	{
		const FName NpcId = Definition.NpcId.IsNone() ? Definition.OwnerId : Definition.NpcId;
		if (NpcId == TEXT("Npc.TusiChief")) return TusiChiefCardPortraitTexturePath;
		if (NpcId == TEXT("Npc.SongJinBao")) return SongJinBaoCardPortraitTexturePath;
		if (NpcId == TEXT("Npc.YueBai")) return YueBaiCardPortraitTexturePath;
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return ZhouGuangZuCardPortraitTexturePath;
		if (NpcId == TEXT("Npc.JinGui")) return JinGuiCardPortraitTexturePath;
		if (NpcId == TEXT("Npc.QiongMeiEr")) return QiongMeiErCardPortraitTexturePath;
		return FString();
	}
	if (Definition.Owner == EGameXXKCardOwner::Profession)
	{
		switch (Definition.Role)
		{
		case EGameXXKCharacterRole::Blade: return BladeCardPortraitTexturePath;
		case EGameXXKCharacterRole::Guard: return GuardCardPortraitTexturePath;
		case EGameXXKCharacterRole::Healer: return HealerCardPortraitTexturePath;
		case EGameXXKCharacterRole::Hunter: return HunterCardPortraitTexturePath;
		case EGameXXKCharacterRole::Sorcerer: return SorcererCardPortraitTexturePath;
		case EGameXXKCharacterRole::FormationMaster: return FormationMasterCardPortraitTexturePath;
		default: return FString();
		}
	}
	if (Definition.Owner == EGameXXKCardOwner::Route)
	{
		const FString AcquisitionKey = Definition.AcquisitionKey.ToString();
		if (AcquisitionKey == TEXT("Route.General")) return RouteGeneralCardPortraitTexturePath;
		if (AcquisitionKey == TEXT("Route.Terrain")) return RouteTerrainCardPortraitTexturePath;
		if (AcquisitionKey == TEXT("Route.Rare")) return RouteRareCardPortraitTexturePath;
		if (AcquisitionKey.StartsWith(TEXT("Route.Boss."))) return RouteBossCardPortraitTexturePath;
	}
	return FString();
}

UTexture2D* UGameXXKBattleBoardWidget::ResolveCardPortraitTexture(const FGameXXKCardDefinition& Definition)
{
	const FString ResourcePath = ResolveCardPortraitResourcePath(Definition);
	if (ResourcePath.IsEmpty())
	{
		return nullptr;
	}
	const FName CacheKey(*ResourcePath);
	if (const TObjectPtr<UTexture2D>* Existing = CardPortraitTextures.Find(CacheKey))
	{
		return Existing->Get();
	}
	UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, *ResourcePath);
	CardPortraitTextures.Add(CacheKey, Loaded);
	return Loaded;
}

FLinearColor UGameXXKBattleBoardWidget::ResolveBattleActionButtonTint(FName ActionName) const
{
	if (ActionName == BasicAttackAction || ActionName == CraneWingSlashAction)
	{
		return FLinearColor(0.92f, 0.42f, 0.34f, 0.88f);
	}
	if (ActionName == DefendAction)
	{
		return FLinearColor(0.34f, 0.54f, 0.92f, 0.88f);
	}
	if (ActionName == GuiyuanArtAction)
	{
		return FLinearColor(0.38f, 0.70f, 0.54f, 0.88f);
	}
	if (ActionName == HealingPowderAction)
	{
		return FLinearColor(0.84f, 0.66f, 0.34f, 0.88f);
	}
	return FLinearColor(0.70f, 0.78f, 0.74f, 0.88f);
}

FVector2D UGameXXKBattleBoardWidget::ResolveCommandSourcePosition(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition, FVector2D LocalSize) const
{
	FVector2D SourcePosition = UnitScreenPosition.IsNearlyZero() ? MenuScreenPosition : UnitScreenPosition;
	if (LocalSize.X > 1.0f)
	{
		const float PartyLaneMinX = LocalSize.X * PartyCommandLaneMinXRatio;
		if (SourcePosition.X < PartyLaneMinX)
		{
			SourcePosition.X = FMath::Max3(SourcePosition.X, MenuScreenPosition.X, LocalSize.X * PartyCommandLanePreferredXRatio);
		}
		SourcePosition.X = FMath::Clamp(SourcePosition.X, 12.0f, FMath::Max(12.0f, LocalSize.X - 12.0f));
	}
	if (LocalSize.Y > 1.0f)
	{
		if (SourcePosition.Y < 12.0f || SourcePosition.Y > LocalSize.Y - 12.0f)
		{
			const float FallbackYRatio = PartyIndex <= 0 ? 0.42f : 0.60f;
			SourcePosition.Y = LocalSize.Y * FallbackYRatio;
		}
		SourcePosition.Y = FMath::Clamp(SourcePosition.Y, 12.0f, FMath::Max(12.0f, LocalSize.Y - 12.0f));
	}
	return SourcePosition;
}

FVector2D UGameXXKBattleBoardWidget::ResolveCommandMenuAnchor(FVector2D UnitScreenPosition) const
{
	FVector2D Anchor = UnitScreenPosition + CommandMenuDefaultOffset;
	const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
	if (LocalSize.X > 1.0f && LocalSize.Y > 1.0f)
	{
		if (Anchor.X < 12.0f)
		{
			Anchor.X = UnitScreenPosition.X + CommandMenuGap;
		}
		Anchor.X = FMath::Clamp(Anchor.X, 12.0f, FMath::Max(12.0f, LocalSize.X - (CommandMenuWidth + 12.0f)));
		Anchor.Y = FMath::Clamp(Anchor.Y, 12.0f, FMath::Max(12.0f, LocalSize.Y - (CommandMenuHeight + 12.0f)));
	}
	return Anchor;
}

FVector2D UGameXXKBattleBoardWidget::ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition) const
{
	const FGeometry Geometry = GetCachedGeometry();
	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (LocalSize.X <= 1.0f || LocalSize.Y <= 1.0f)
	{
		return ScreenPosition;
	}
	return ClampToLocalSize(Geometry.AbsoluteToLocal(ScreenPosition), LocalSize);
}

FVector2D UGameXXKBattleBoardWidget::ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D LocalSize) const
{
	return ResolveSlateAbsolutePositionToLocal(ScreenPosition, WidgetAbsolutePosition, LocalSize, LocalSize);
}

FVector2D UGameXXKBattleBoardWidget::ResolveSlateAbsolutePositionToLocal(FVector2D ScreenPosition, FVector2D WidgetAbsolutePosition, FVector2D WidgetAbsoluteSize, FVector2D LocalSize) const
{
	if (LocalSize.X <= 1.0f || LocalSize.Y <= 1.0f)
	{
		return ScreenPosition;
	}

	if (WidgetAbsoluteSize.X <= 1.0f || WidgetAbsoluteSize.Y <= 1.0f)
	{
		return ClampToLocalSize(ScreenPosition - WidgetAbsolutePosition, LocalSize);
	}

	const FVector2D NormalizedPosition(
		(ScreenPosition.X - WidgetAbsolutePosition.X) / WidgetAbsoluteSize.X,
		(ScreenPosition.Y - WidgetAbsolutePosition.Y) / WidgetAbsoluteSize.Y);
	return ClampToLocalSize(FVector2D(NormalizedPosition.X * LocalSize.X, NormalizedPosition.Y * LocalSize.Y), LocalSize);
}

bool UGameXXKBattleBoardWidget::BeginCardTargeting(const FGameXXKCardPlayPreview& Preview)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	if (!Preview.bCanPlay || !Preview.TargetRequest.bRequiresManualSelection || Preview.CardInstanceId.IsNone() || Preview.OwnerUnitId.IsNone())
	{
		LastCardInteractionError = TEXT("该卡无法进入手动目标选择。");
		return false;
	}

	TSet<FName> NewLegalTargets;
	for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
	{
		if (Candidate.bCanSelect && !Candidate.UnitId.IsNone())
		{
			NewLegalTargets.Add(Candidate.UnitId);
		}
	}
	if (NewLegalTargets.IsEmpty())
	{
		LastCardInteractionError = TEXT("该卡当前没有合法目标。");
		return false;
	}

	PendingCardPreview = Preview;
	LegalCardTargetUnitIds = MoveTemp(NewLegalTargets);
	SelectedPartyIndex = INDEX_NONE;
	TargetingActionName = CardTargetingAction;
	TargetingSourcePosition = ResolveCardTargetingSourcePosition(Preview.OwnerUnitId);
	TargetingPointerPosition = TargetingSourcePosition;
	InteractionMode = EGameXXKBattleInteractionMode::TargetingCard;
	LastCardInteractionError.Reset();
	RefreshProgrammaticLayout();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UGameXXKBattleBoardWidget::ResolveAutomaticCardPlay(FName CardInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		LastCardInteractionError = TEXT("卡牌战斗状态不可用。");
		return false;
	}

	FGameXXKCardPlayResult Result;
	FString Error;
	if (!FGameXXKCardBattleAdapter::ResolveCardPlay(Subsystem->GetMutableRuntimeState(), CardInstanceId, NAME_None, Result, &Error))
	{
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	LastCardInteractionError.Reset();
	const bool bRefreshed = ResolveAndRefreshCardBattleAfterMutation();
	if (bRefreshed)
	{
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->RefreshBattleSceneAfterCardMutation(Result.OwnerUnitId, Result.DamageResults);
		}
	}
	return bRefreshed;
}

bool UGameXXKBattleBoardWidget::RefreshPendingCardTargetingPreview()
{
	if (!IsCardTargetingActive() || PendingCardPreview.CardInstanceId.IsNone())
	{
		return false;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return false;
	}

	FGameXXKCardPlayPreview NewPreview;
	FString Error;
	if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(Subsystem->GetRuntimeState(), PendingCardPreview.CardInstanceId, NewPreview, &Error)
		|| !NewPreview.bCanPlay
		|| !NewPreview.TargetRequest.bRequiresManualSelection)
	{
		LastCardInteractionError = Error.IsEmpty() ? NewPreview.FailureReason : Error;
		return false;
	}

	TSet<FName> NewLegalTargets;
	for (const FGameXXKCardTargetCandidateView& Candidate : NewPreview.TargetRequest.CandidateViews)
	{
		if (Candidate.bCanSelect && !Candidate.UnitId.IsNone())
		{
			NewLegalTargets.Add(Candidate.UnitId);
		}
	}
	if (NewLegalTargets.IsEmpty())
	{
		LastCardInteractionError = TEXT("该卡当前没有合法目标。");
		return false;
	}

	PendingCardPreview = NewPreview;
	LegalCardTargetUnitIds = MoveTemp(NewLegalTargets);
	TargetingSourcePosition = ResolveCardTargetingSourcePosition(NewPreview.OwnerUnitId);
	return true;
}

void UGameXXKBattleBoardWidget::ClearCardTargetingState()
{
	PendingCardPreview = FGameXXKCardPlayPreview();
	LegalCardTargetUnitIds.Reset();
	if (InteractionMode == EGameXXKBattleInteractionMode::TargetingCard)
	{
		InteractionMode = EGameXXKBattleInteractionMode::Idle;
	}
	if (TargetingActionName == CardTargetingAction)
	{
		TargetingActionName = NAME_None;
	}
	TargetingSourcePosition = FVector2D::ZeroVector;
}

bool UGameXXKBattleBoardWidget::ResolveCardBattleTerminalState()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		return true;
	}

	const EGameXXKCardBattlePhase Phase = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Phase;
	if (Phase == EGameXXKCardBattlePhase::Victory)
	{
		if (!Subsystem->ResolveBattleVictory(false))
		{
			LastCardInteractionError = TEXT("战斗胜利奖励未能生成。");
			return false;
		}
	}
	else if (Phase == EGameXXKCardBattlePhase::Defeat)
	{
		if (!Subsystem->FailDungeonToTown())
		{
			LastCardInteractionError = TEXT("战斗失败后未能返回城镇。");
			return false;
		}
	}
	return true;
}

FVector2D UGameXXKBattleBoardWidget::ResolveCardTargetingSourcePosition(FName OwnerUnitId) const
{
	if (const FVector2D* RegisteredPosition = RegisteredBattleUnitScreenPositions.Find(OwnerUnitId))
	{
		return *RegisteredPosition;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (State)
	{
		const int32 OwnerPartyIndex = State->ActiveBattleParty.IndexOfByPredicate([OwnerUnitId](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == OwnerUnitId;
		});
		if (OwnerPartyIndex != INDEX_NONE)
		{
			const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
			if (LocalSize.X > 1.0f && LocalSize.Y > 1.0f)
			{
				const float VerticalStep = 0.16f;
				return FVector2D(LocalSize.X * 0.80f, LocalSize.Y * (0.42f + VerticalStep * OwnerPartyIndex));
			}
		}
	}

	return !SelectedPartyScreenPosition.IsNearlyZero() ? SelectedPartyScreenPosition : FVector2D(960.0f, 420.0f);
}

bool UGameXXKBattleBoardWidget::ResolveAndRefreshCardBattleAfterMutation()
{
	if (!ResolveCardBattleTerminalState())
	{
		RefreshProgrammaticLayout();
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem && Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
	}
	if (!NotifyPlayerFlowStateChanged())
	{
		RefreshFromState();
	}
	return true;
}

bool UGameXXKBattleBoardWidget::BeginTargetingBattleAction(FName ActionName)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	if (InteractionMode != EGameXXKBattleInteractionMode::CommandMenuOpen || SelectedPartyIndex == INDEX_NONE)
	{
		return false;
	}

	if (ActionName == BasicAttackAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingBasicAttack;
	}
	else if (ActionName == CraneWingSlashAction)
	{
		InteractionMode = EGameXXKBattleInteractionMode::TargetingCraneWingSlash;
	}
	else
	{
		return false;
	}

	TargetingActionName = ActionName;
	TargetingSourcePosition = SelectedPartyScreenPosition;
	TargetingPointerPosition = SelectedPartyScreenPosition;
	RefreshProgrammaticLayout();
	return true;
}

int32 UGameXXKBattleBoardWidget::FindFirstLivingEnemyIndex() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return INDEX_NONE;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	for (int32 EnemyIndex = 0; EnemyIndex < State.ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		const FGameXXKBattleRuntimeUnit& Enemy = State.ActiveBattleEnemies[EnemyIndex];
		if (Enemy.bEnemy && !Enemy.bDefeated && Enemy.HP > 0)
		{
			return EnemyIndex;
		}
	}
	return INDEX_NONE;
}

void UGameXXKBattleBoardWidget::HandleBasicAttackClicked()
{
	ExecuteBasicAttackAction();
}

void UGameXXKBattleBoardWidget::HandleCraneWingSlashClicked()
{
	ExecuteCraneWingSlashAction();
}

void UGameXXKBattleBoardWidget::HandleGuiyuanArtClicked()
{
	ExecuteGuiyuanArtAction();
}

void UGameXXKBattleBoardWidget::HandleDefendClicked()
{
	ExecuteDefendAction();
}

void UGameXXKBattleBoardWidget::HandleHealingPowderClicked()
{
	ExecuteHealingPowderAction();
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot0Clicked()
{
	HandleHandCardSlotClicked(0);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot1Clicked()
{
	HandleHandCardSlotClicked(1);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot2Clicked()
{
	HandleHandCardSlotClicked(2);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot3Clicked()
{
	HandleHandCardSlotClicked(3);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot4Clicked()
{
	HandleHandCardSlotClicked(4);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot0Hovered()
{
	SetHandCardHoverState(0, true);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot1Hovered()
{
	SetHandCardHoverState(1, true);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot2Hovered()
{
	SetHandCardHoverState(2, true);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot3Hovered()
{
	SetHandCardHoverState(3, true);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot4Hovered()
{
	SetHandCardHoverState(4, true);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot0Unhovered()
{
	SetHandCardHoverState(0, false);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot1Unhovered()
{
	SetHandCardHoverState(1, false);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot2Unhovered()
{
	SetHandCardHoverState(2, false);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot3Unhovered()
{
	SetHandCardHoverState(3, false);
}

void UGameXXKBattleBoardWidget::HandleHandCardSlot4Unhovered()
{
	SetHandCardHoverState(4, false);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot0Hovered()
{
	SetEnemyIntentHoverState(0, true);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot1Hovered()
{
	SetEnemyIntentHoverState(1, true);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot2Hovered()
{
	SetEnemyIntentHoverState(2, true);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot0Unhovered()
{
	SetEnemyIntentHoverState(0, false);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot1Unhovered()
{
	SetEnemyIntentHoverState(1, false);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentSlot2Unhovered()
{
	SetEnemyIntentHoverState(2, false);
}

void UGameXXKBattleBoardWidget::HandleEnemyIntentRecoveryClicked()
{
	RetryEnemyIntentCompletion();
}

void UGameXXKBattleBoardWidget::HandlePendingInsightCancelClicked()
{
	CancelPendingInsightChoice();
}

void UGameXXKBattleBoardWidget::HandleEndTurnClicked()
{
	EndCardPlayerPhase();
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot0Clicked()
{
	HandleRewardCardSlotClicked(0);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot1Clicked()
{
	HandleRewardCardSlotClicked(1);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot2Clicked()
{
	HandleRewardCardSlotClicked(2);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot0Hovered()
{
	SetRewardCardHoverState(0, true);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot1Hovered()
{
	SetRewardCardHoverState(1, true);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot2Hovered()
{
	SetRewardCardHoverState(2, true);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot0Unhovered()
{
	SetRewardCardHoverState(0, false);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot1Unhovered()
{
	SetRewardCardHoverState(1, false);
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlot2Unhovered()
{
	SetRewardCardHoverState(2, false);
}

void UGameXXKBattleBoardWidget::HandleSkipRewardClicked()
{
	SkipPendingRouteReward();
}

void UGameXXKBattleBoardWidget::HandleHandCardSlotClicked(int32 SlotIndex)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const EGameXXKCardPendingChoiceKind PendingChoiceKind = Subsystem
		&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		? Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.PendingChoice.Kind
		: EGameXXKCardPendingChoiceKind::None;
	if (PendingChoiceKind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
		|| PendingChoiceKind == EGameXXKCardPendingChoiceKind::ForcedDiscard)
	{
		// The button stays enabled solely so disabled-looking hand cards retain pure-hover inspection.
		// The existing blocking choice remains the only legal mutation path.
		return;
	}
	if (HandCardInstanceIds.IsValidIndex(SlotIndex))
	{
		ClickCardInHand(HandCardInstanceIds[SlotIndex]);
	}
}

void UGameXXKBattleBoardWidget::HandleRewardCardSlotClicked(int32 SlotIndex)
{
	if (PendingRewardCardIds.IsValidIndex(SlotIndex))
	{
		ChoosePendingRouteReward(PendingRewardCardIds[SlotIndex], SelectedRouteRewardReplacementEntryId);
	}
}

void UGameXXKBattleBoardWidget::HandleRouteRewardReplacementClicked(FName EntryId)
{
	SelectRouteRewardReplacementEntry(EntryId);
}
