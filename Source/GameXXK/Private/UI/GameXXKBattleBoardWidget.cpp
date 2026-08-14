#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"

#include "Application/SlateApplicationBase.h"
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
#include "GameXXKRelicCatalog.h"
#include "Engine/Texture2D.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElementTypes.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Styling/SlateBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKCardOutcomePreviewWidget.h"
#include "UI/GameXXKBattleStatusIconWidget.h"
#include "UI/GameXXKBattleStatusIconStyle.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

int32 UGameXXKBattleBoardWidget::GAliveBattleBoardInstances = 0;

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
	// Page-18 card size (137x190) shared with the out-of-battle deck pages.
	static const FVector2D PlayerHandCardSize(206.0f, 285.0f);
	static const FVector2D PlayerHandRowSize(1170.0f, 287.0f);
	static const FVector2D PartyQiWidgetSize(104.0f, 104.0f);
	static const FVector2D FixedUnitHudWidgetSize(272.0f, 142.0f);
	static const FVector2D BattleHudSafeStageDesignSize(1920.0f, 1080.0f);
	static const FVector2D FormationVisualSize(410.0f, 410.0f);
	// Adjacent formation centers are 288 design pixels apart horizontally. Keep
	// the transparent interaction regions centered on their visuals, but narrow
	// enough that one unit can never intercept the hover/click meant for its
	// neighbour. The 410x410 rendered character layout remains unchanged.
	static const FVector2D FormationTargetProxySize(180.0f, 320.0f);
	static constexpr float FormationVisualVerticalOffsetPixels = -64.0f;
	static constexpr float FormationVisualVerticalOffsetNormalized = FormationVisualVerticalOffsetPixels / 1080.0f;
	static const FVector2D CinematicImpactVisualSize(360.0f, 360.0f);
	static const FVector2D CinematicEnemyAnchor(590.0f / 1920.0f, 0.5f);
	static const FVector2D CinematicPartyAnchor(1330.0f / 1920.0f, 0.5f);
	static const FVector2D CinematicImpactAnchor(0.5f, 0.5f);
	// Formation Master terrain adaptation: each battle terrain owns a generated
	// v2 backdrop; Invalid (and any unknown terrain) falls back to the approved riverside asset.
	static const TCHAR* RiversideBattleBackdropTexturePath =
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1.T_BattleArena_Riverside_GeneratedV1");
	// Indexed by EGameXXKCardTerrain (Invalid=0, Plain=1, Cliff=2, Forest=3, WaterShore=4, Ferry=5, Village=6, Cave=7).
	static const TCHAR* TerrainBattleBackdropTexturePaths[] = {
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1.T_BattleArena_Riverside_GeneratedV1"), // Invalid
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Plain_GeneratedV2.T_BattleArena_Plain_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Cliff_GeneratedV2.T_BattleArena_Cliff_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Forest_GeneratedV2.T_BattleArena_Forest_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_WaterShore_GeneratedV2.T_BattleArena_WaterShore_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Ferry_GeneratedV2.T_BattleArena_Ferry_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Village_GeneratedV2.T_BattleArena_Village_GeneratedV2"),
		TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Cave_GeneratedV2.T_BattleArena_Cave_GeneratedV2"),
	};
	// Legacy test-only resolver constants. Production HUD placement no longer reads them.
	static constexpr float ProjectedUnitHudFootGap = 8.0f;
	static constexpr float ProjectedUnitHudObstacleGap = 10.0f;
	static constexpr int32 ProjectedUnitHudLayerZOrder = -1;
	static constexpr int32 OutcomePreviewLayerZOrder = 1;
	static constexpr int32 BattleBackdropZOrder = 0;
	static constexpr int32 BattleSafeStageRootZOrder = 1;
	static constexpr int32 BattleCinematicViewportCoverZOrder = 2;
	static constexpr int32 BattleFormationZOrder = 10;
	static constexpr int32 BattleTargetProxyBaseZOrder = 10;
	static constexpr int32 BattleControlsZOrder = 20;
	static constexpr int32 BattleCinematicDimmerZOrder = 30;
	static constexpr int32 BattleCinematicParticipantZOrder = 40;
	static constexpr int32 BattleCinematicImpactZOrder = 50;
	static constexpr int32 BattleCinematicStatusIconZOrder = 55;
	static constexpr int32 BattleCinematicReadoutZOrder = 60;
	static constexpr int32 PartyQiWidgetZOrder = 35;
	static constexpr float PartyQiHandSafetyGap = 12.0f;
	static constexpr float PlayerHandSelectedScale = 1.20f;
	static constexpr float PlayerHandSelectedLift = -32.0f;
	static constexpr double PlayedCardCommitDurationSeconds = 0.18;
	static constexpr float PlayedCardCommitLift = -72.0f;
	static constexpr float PlayedCardCommitPeakScale = 1.26f;
	static const FVector2D EnemyIntentCardSize(150.0f, 171.0f);
	static const FVector2D EnemyIntentShowcaseCardSize(256.0f, 292.0f);
	static const FVector2D RewardCardSize(206.0f, 285.0f);
	static const FVector2D EnemyIntentRailSize(600.0f, 171.0f);
	static const FVector2D EnemyIntentTooltipSize(460.0f, 256.0f);
	static const FVector2D HandCardDetailPanelSize(420.0f, 252.0f);
	static const FLinearColor BattleStatusInkColor(0.12f, 0.09f, 0.06f, 1.0f);
	static constexpr float BattleStatusFrameMarginRatio = 5.0f / 368.0f;
	static constexpr float EnemyIntentRevealDuration = 0.55f;
	static constexpr float EnemyIntentResolveDuration = 0.18f;
	static const FVector2D GroupOutcomePreviewAnchor(0.245f, 0.34f);
	static constexpr float SingleOutcomePreviewTargetGap = 12.0f;
	static const float SingleOutcomePreviewTopOffset =
		-(FormationVisualSize.Y * 0.5f + SingleOutcomePreviewTargetGap);
	static const FMargin SingleOutcomePreviewOffsets(0.0f, SingleOutcomePreviewTopOffset, 272.0f, 56.0f);
	static const FMargin GroupOutcomePreviewOffsets(0.0f, 0.0f, 620.0f, 108.0f);

	TArray<FGameXXKCardDamageResult> FlattenResumedCardDamageResults(
		const TArray<FGameXXKCardPlayResult>& ResumedResults)
	{
		TArray<FGameXXKCardDamageResult> DamageResults;
		for (const FGameXXKCardPlayResult& Result : ResumedResults)
		{
			DamageResults.Append(Result.DamageResults);
		}
		return DamageResults;
	}
	static constexpr float EnemyIntentSettleDuration = 0.32f;
	static constexpr double BattleReadoutPeakHoldSeconds = 0.04;

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
	// Master V1 approved card frame, matching the out-of-battle deck pages.
	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame");
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
	static constexpr const TCHAR* TargetingArrowHeadTexturePath = TEXT("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead.T_BattleTargetArrowHead");

	FString ResolveEnemyPortraitPathByDefinitionId(const FName EnemyDefinitionId)
	{
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.Rooster")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_Rooster.T_CardPortrait_Enemy_Ch1_Rooster");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.Goat")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_Goat.T_CardPortrait_Enemy_Ch1_Goat");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.Weasel")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_Weasel.T_CardPortrait_Enemy_Ch1_Weasel");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.Civet")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_Civet.T_CardPortrait_Enemy_Ch1_Civet");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.IronfeatherRooster")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_IronfeatherRooster.T_CardPortrait_Enemy_Ch1_IronfeatherRooster");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.BluehornGoatKing")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_BluehornGoatKing.T_CardPortrait_Enemy_Ch1_BluehornGoatKing");
		if (EnemyDefinitionId == TEXT("Enemy.Ch1.MoneyRat")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch1_MoneyRatBoss.T_CardPortrait_Enemy_Ch1_MoneyRatBoss");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.GrayWolf")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_GrayWolf.T_CardPortrait_Enemy_Ch2_GrayWolf");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.Boar")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_Boar.T_CardPortrait_Enemy_Ch2_Boar");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.Macaque")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_Macaque.T_CardPortrait_Enemy_Ch2_Macaque");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.Porcupine")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_Porcupine.T_CardPortrait_Enemy_Ch2_Porcupine");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.GraymaneWolfKing")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_GraymaneWolfKing.T_CardPortrait_Enemy_Ch2_GraymaneWolfKing");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.RedtuskBoarKing")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_RedtuskBoarKing.T_CardPortrait_Enemy_Ch2_RedtuskBoarKing");
		if (EnemyDefinitionId == TEXT("Enemy.Ch2.BlackBear")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_BlackBearBoss.T_CardPortrait_Enemy_Ch2_BlackBearBoss");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.VenomSnake")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_VenomSnake.T_CardPortrait_Enemy_Ch3_VenomSnake");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.Wildcat")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_Wildcat.T_CardPortrait_Enemy_Ch3_Wildcat");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.Vulture")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_Vulture.T_CardPortrait_Enemy_Ch3_Vulture");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.GiantToad")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_GiantToad.T_CardPortrait_Enemy_Ch3_GiantToad");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.WhiteApe")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_WhiteApe.T_CardPortrait_Enemy_Ch3_WhiteApe");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.SpiralHornDeer")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_SpiralHornDeer.T_CardPortrait_Enemy_Ch3_SpiralHornDeer");
		if (EnemyDefinitionId == TEXT("Enemy.Ch3.Tiger")) return TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_TigerBoss.T_CardPortrait_Enemy_Ch3_TigerBoss");
		return FString();
	}

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
		return GameXXKCardText::DescribeStatusName(Status);
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
		else if (ChoiceKind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand)
		{
			Owner->SubmitPendingHeroTaskSearchChoice(CandidateInstanceId);
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

void UGameXXKBattleUnitTargetProxyButton::Configure(
	UGameXXKBattleBoardWidget* const InOwner,
	const FName InUnitId)
{
	Owner = InOwner;
	UnitId = InUnitId;
	OnClicked.Clear();
	OnHovered.Clear();
	OnUnhovered.Clear();
	OnClicked.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleClicked);
	OnHovered.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleHovered);
	OnUnhovered.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleUnhovered);
}

void UGameXXKBattleUnitTargetProxyButton::HandleHovered()
{
	if (Owner && !UnitId.IsNone())
	{
		Owner->HandleUnitTargetProxyHoverChanged(UnitId, true);
	}
}

void UGameXXKBattleUnitTargetProxyButton::HandleUnhovered()
{
	if (Owner && !UnitId.IsNone())
	{
		Owner->HandleUnitTargetProxyHoverChanged(UnitId, false);
	}
}

void UGameXXKBattleUnitTargetProxyButton::HandleClicked()
{
	if (Owner && !UnitId.IsNone())
	{
		Owner->HandleUnitTargetProxyClicked(UnitId);
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
	++GAliveBattleBoardInstances;
	UE_LOG(LogTemp, Verbose, TEXT("[Board] constructed name=%s alive=%d"), *GetName(), GAliveBattleBoardInstances);
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKBattleBoardWidget::NativeDestruct()
{
	ClearCardOutcomePreview();
	--GAliveBattleBoardInstances;
	UE_LOG(LogTemp, Verbose, TEXT("[Board] destructed name=%s alive=%d"), *GetName(), GAliveBattleBoardInstances);
	if (ActiveBattleVisualSessionToken != 0)
	{
		CancelBattleVisualSession(ActiveBattleVisualSessionToken);
	}
	ResetBattlePresentationFeedback();
	if (AtlasCache)
	{
		AtlasCache->Clear();
		AtlasCache.Reset();
	}
	Super::NativeDestruct();
}

void UGameXXKBattleBoardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshCinematicViewportCoverLayout(MyGeometry.GetLocalSize());
	if (ActiveBattleVisualSessionToken != 0 && FSlateApplicationBase::IsInitialized())
	{
		AdvanceVisualsAtRealTime(FSlateApplicationBase::Get().GetCurrentTime());
	}
	else if (IsBattlePresentationPending())
	{
		// A pending presentation can never advance without a visual session, so
		// the HP overrides/snapshot would stay frozen forever.  The mutation is
		// already committed to runtime; release the presentation and run the
		// deferred continuation so the HUD refreshes from live state.
		const EBattlePresentationContinuation StuckContinuation = DeferredBattlePresentationContinuation;
		ResetBattlePresentation();
		if (StuckContinuation != EBattlePresentationContinuation::None)
		{
			++ExecutedBattlePresentationContinuationCount;
			ExecuteBattlePresentationContinuation(StuckContinuation);
		}
	}
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

void UGameXXKBattleBoardWidget::QueuePresentation(const FGameXXKBattlePresentationEvent& Event)
{
	QueuePresentationInternal(Event, true);
}

void UGameXXKBattleBoardWidget::QueuePresentationInternal(
	const FGameXXKBattlePresentationEvent& Event,
	const bool bRefreshBaseline)
{
	if (Event.TargetUnitId.IsNone())
	{
		return;
	}
	ClearCardOutcomePreview();

	FBattlePresentationQueueEntry Entry;
	Entry.Event = Event;
	Entry.Kind = EBattlePresentationKind::AttackHit;
	Entry.Rhythm = FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(Event);
	Entry.QueueSerial = NextBattlePresentationQueueSerial++;
	if (NextBattlePresentationQueueSerial == 0)
	{
		NextBattlePresentationQueueSerial = 1;
	}
	if (!Event.AttackerUnitId.IsNone())
	{
		Entry.AttackerClip = ResolveUnitAnimationClip(
			Event.AttackerUnitId,
			Event.bAttackerEnemy,
			EGameXXKBattleAnimationAction::Attack);
		Entry.AttackerClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(
			Entry.AttackerClip,
			Entry.Rhythm.DurationSeconds);
	}
	Entry.TargetClip = ResolveUnitAnimationClip(
		Event.TargetUnitId,
		Event.bTargetEnemy,
		EGameXXKBattleAnimationAction::Hit);
	Entry.TargetClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(
		Entry.TargetClip,
		Entry.Rhythm.DurationSeconds);
	const uint64 QueueSerial = Entry.QueueSerial;
	BattlePresentationQueue.Add(MoveTemp(Entry));
	if (!DisplayedHealthOverrides.Contains(Event.TargetUnitId))
	{
		if (bRefreshBaseline)
		{
			SetDisplayedHealthOverlay(Event.TargetUnitId, Event.TargetHealthBefore);
		}
		else
		{
			const int32 BaselineHealth = FMath::Max(0, Event.TargetHealthBefore);
			DisplayedHealthOverrides.Add(Event.TargetUnitId, BaselineHealth);
			if (FGameXXKBattleUnitHudView* const View = DisplayedUnitHudOverrides.Find(Event.TargetUnitId))
			{
				View->CurrentHP = BaselineHealth;
				View->bLiving = true;
			}
		}
	}
	ApplyBattlePresentationInteractionLock();
	PrefetchPresentationEntry(QueueSerial);
}

void UGameXXKBattleBoardWidget::QueueStatusPresentation(
	const FGameXXKBattleStatusPresentationEvent& Event)
{
	if (Event.UnitId.IsNone()
		|| Event.StackDelta == 0
		|| (Event.AnimationAction != EGameXXKBattleAnimationAction::Buff
			&& Event.AnimationAction != EGameXXKBattleAnimationAction::Debuff))
	{
		return;
	}

	FBattlePresentationQueueEntry Entry;
	Entry.StatusEvent = Event;
	Entry.Kind = EBattlePresentationKind::Status;
	Entry.Rhythm = FGameXXKBattleAnimationPresentation::ResolveStatusRhythm();
	Entry.QueueSerial = NextBattlePresentationQueueSerial++;
	if (NextBattlePresentationQueueSerial == 0)
	{
		NextBattlePresentationQueueSerial = 1;
	}
	Entry.StatusClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(
		FGameXXKBattleAnimationPresentation::ResolveGenericClip(Event.AnimationAction),
		Entry.Rhythm.DurationSeconds);
	const uint64 QueueSerial = Entry.QueueSerial;
	BattlePresentationQueue.Add(MoveTemp(Entry));
	ApplyBattlePresentationInteractionLock();
	PrefetchPresentationEntry(QueueSerial);
}

void UGameXXKBattleBoardWidget::CapturePresentationHudSnapshot(
	const FGameXXKCardBattleRuntime& Runtime)
{
	DisplayedHealthOverrides.Reset();
	DisplayedUnitHudOverrides.Reset();
	DisplayedSharedEnergyOverride = Runtime.Deck.SharedEnergy;
	for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
	{
		if (Unit.UnitId.IsNone())
		{
			continue;
		}
		FGameXXKBattleUnitHudView View;
		if (FGameXXKBattlePresentation::BuildUnitHudView(
			Runtime,
			Unit.UnitId,
			ResolveProjectedUnitHudDisplayName(Unit.UnitId),
			View))
		{
			DisplayedUnitHudOverrides.Add(Unit.UnitId, MoveTemp(View));
		}
	}
}

void UGameXXKBattleBoardWidget::DiscardPresentationHudSnapshot()
{
	UE_LOG(LogTemp, Verbose, TEXT("[HPDiscard] Discarding presentation HUD snapshot (health overrides=%d, unit views=%d)"), DisplayedHealthOverrides.Num(), DisplayedUnitHudOverrides.Num());
	DisplayedHealthOverrides.Reset();
	DisplayedUnitHudOverrides.Reset();
	DisplayedSharedEnergyOverride.Reset();
}

bool UGameXXKBattleBoardWidget::QueueMutationPresentation(
	const FGameXXKCardBattleRuntime& Before,
	const TArray<FGameXXKCardDamageResult>& DamageResults,
	const EBattlePresentationContinuation Continuation,
	const FName PlayedCardInstanceId)
{
	UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		DiscardPresentationHudSnapshot();
		return ExecuteBattlePresentationContinuation(Continuation);
	}

	DeferredBattlePresentationContinuation = Continuation;
	const FGameXXKCardBattleRuntime& After = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
	const TArray<FGameXXKBattlePresentationEvent> DamageEvents =
		FGameXXKBattleAnimationPresentation::BuildPresentationEvents(After, NAME_None, DamageResults);
	const TArray<FGameXXKBattleStatusPresentationEvent> StatusEvents =
		FGameXXKBattleAnimationPresentation::BuildStatusPresentationEvents(Before, After);
	for (const FGameXXKBattlePresentationEvent& Event : DamageEvents)
	{
		QueuePresentationInternal(Event, false);
	}
	for (const FGameXXKBattleStatusPresentationEvent& Event : StatusEvents)
	{
		// Buff/debuff state belongs to the fixed actor HUD. It updates with the
		// mutation but never consumes a full-screen presentation entry.
		ApplyDisplayedStatusDelta(Event);
	}
	const bool bCommitStarted = BeginPlayedCardCommit(PlayedCardInstanceId);

	if (!BattlePresentationQueue.IsEmpty())
	{
		// Paint the captured pre-mutation baseline only after the complete immutable
		// damage-plus-status batch exists. No partial batch may leak through a HUD refresh.
		RefreshProjectedUnitHuds();
		ApplyBattlePresentationInteractionLock();
		return true;
	}
	if (bCommitStarted)
	{
		ApplyBattlePresentationInteractionLock();
		return true;
	}

	const EBattlePresentationContinuation ImmediateContinuation = DeferredBattlePresentationContinuation;
	DeferredBattlePresentationContinuation = EBattlePresentationContinuation::None;
	DiscardPresentationHudSnapshot();
	// Immediate refresh from live runtime so that healing-only / status-only /
	// relic-only HP mutations are visible before the deferred continuation runs.
	RefreshProjectedUnitHuds();
	if (ImmediateContinuation != EBattlePresentationContinuation::None)
	{
		++ExecutedBattlePresentationContinuationCount;
	}
	return ExecuteBattlePresentationContinuation(ImmediateContinuation);
}

bool UGameXXKBattleBoardWidget::BeginPlayedCardCommit(const FName PlayedCardInstanceId)
{
	if (PlayedCardInstanceId.IsNone()
		|| ActiveBattleVisualSessionToken == 0
		|| bPlayedCardCommitActive)
	{
		return false;
	}

	const int32 SlotIndex = HandCardInstanceIds.IndexOfByKey(PlayedCardInstanceId);
	if (!HandCardButtons.IsValidIndex(SlotIndex) || !HandCardButtons[SlotIndex])
	{
		return false;
	}

	UButton* const SourceButton = HandCardButtons[SlotIndex];
	bPlayedCardCommitActive = true;
	bPlayedCardCommitStarted = false;
	PlayedCardCommitInstanceId = PlayedCardInstanceId;
	PlayedCardCommitButton = SourceButton;
	PlayedCardCommitInitialTransform = SourceButton->GetRenderTransform();
	PlayedCardCommitInitialOpacity = SourceButton->GetRenderOpacity();
	PlayedCardCommitStartSeconds = 0.0;
	PlayedCardCommitElapsedSeconds = 0.0;
	return true;
}

TOptional<double> UGameXXKBattleBoardWidget::AdvancePlayedCardCommit(const double AbsoluteSeconds)
{
	if (!bPlayedCardCommitActive || !FMath::IsFinite(AbsoluteSeconds))
	{
		return TOptional<double>();
	}
	if (!bPlayedCardCommitStarted)
	{
		bPlayedCardCommitStarted = true;
		PlayedCardCommitStartSeconds = AbsoluteSeconds;
	}

	PlayedCardCommitElapsedSeconds = FMath::Max(0.0, AbsoluteSeconds - PlayedCardCommitStartSeconds);
	const float LinearProgress = static_cast<float>(FMath::Clamp(
		PlayedCardCommitElapsedSeconds / PlayedCardCommitDurationSeconds,
		0.0,
		1.0));
	const float EaseOutProgress = 1.0f - FMath::Square(1.0f - LinearProgress);
	if (UButton* const SourceButton = PlayedCardCommitButton.Get())
	{
		const FVector2D InitialTranslation = PlayedCardCommitInitialTransform.Translation;
		SourceButton->SetRenderTranslation(FVector2D(
			InitialTranslation.X,
			FMath::Lerp(InitialTranslation.Y, PlayedCardCommitLift, EaseOutProgress)));
		const float Scale = FMath::Lerp(
			PlayedCardCommitInitialTransform.Scale.X,
			PlayedCardCommitPeakScale,
			EaseOutProgress);
		SourceButton->SetRenderScale(FVector2D(Scale, Scale));
		const float FadeProgress = FMath::Clamp((LinearProgress - 0.5f) * 2.0f, 0.0f, 1.0f);
		SourceButton->SetRenderOpacity(FMath::Lerp(PlayedCardCommitInitialOpacity, 0.0f, FadeProgress));
	}

	if (PlayedCardCommitElapsedSeconds + static_cast<double>(KINDA_SMALL_NUMBER)
		< PlayedCardCommitDurationSeconds)
	{
		return TOptional<double>();
	}

	const double CompletionBoundarySeconds = PlayedCardCommitStartSeconds + PlayedCardCommitDurationSeconds;
	CompletePlayedCardCommit();
	return CompletionBoundarySeconds;
}

void UGameXXKBattleBoardWidget::CompletePlayedCardCommit()
{
	if (!bPlayedCardCommitActive)
	{
		return;
	}
	++PlayedCardCommitCompletionCount;
	ResetPlayedCardCommit(false);
	if (!BattlePresentationQueue.IsEmpty())
	{
		return;
	}

	const EBattlePresentationContinuation Continuation = DeferredBattlePresentationContinuation;
	DeferredBattlePresentationContinuation = EBattlePresentationContinuation::None;
	DiscardPresentationHudSnapshot();
	RefreshProjectedUnitHuds();
	if (Continuation != EBattlePresentationContinuation::None)
	{
		++ExecutedBattlePresentationContinuationCount;
	}
	ExecuteBattlePresentationContinuation(Continuation);
}

void UGameXXKBattleBoardWidget::ResetPlayedCardCommit(const bool bRestoreInitialVisual)
{
	if (UButton* const SourceButton = PlayedCardCommitButton.Get())
	{
		if (bRestoreInitialVisual)
		{
			SourceButton->SetRenderTransform(PlayedCardCommitInitialTransform);
			SourceButton->SetRenderOpacity(PlayedCardCommitInitialOpacity);
		}
		else
		{
			SourceButton->SetRenderTransform(FWidgetTransform());
			// The authoritative mutation has already consumed this card. Keep the
			// old slot hidden until the deferred continuation refreshes the hand;
			// restoring opacity here would flash the spent card over the damage queue.
			SourceButton->SetRenderOpacity(0.0f);
		}
	}
	bPlayedCardCommitActive = false;
	bPlayedCardCommitStarted = false;
	PlayedCardCommitInstanceId = NAME_None;
	PlayedCardCommitButton.Reset();
	PlayedCardCommitInitialTransform = FWidgetTransform();
	PlayedCardCommitInitialOpacity = 1.0f;
	PlayedCardCommitStartSeconds = 0.0;
	PlayedCardCommitElapsedSeconds = 0.0;
}

bool UGameXXKBattleBoardWidget::IsBattlePresentationPending() const
{
	return bPlayedCardCommitActive
		|| !BattlePresentationQueue.IsEmpty()
		|| DeferredBattlePresentationContinuation != EBattlePresentationContinuation::None;
}

bool UGameXXKBattleBoardWidget::RejectBattlePresentationMutation()
{
	if (!IsBattlePresentationPending())
	{
		return false;
	}
	LastCardInteractionError = TEXT("战斗演出正在结算，请等待演出完成。");
	ApplyBattlePresentationInteractionLock();
	return true;
}

void UGameXXKBattleBoardWidget::ApplyBattlePresentationInteractionLock()
{
	if (ActionBox)
	{
		ActionBox->SetIsEnabled(false);
	}
	for (UButton* Button : HandCardButtons)
	{
		if (Button) Button->SetIsEnabled(false);
	}
	for (UButton* Button : PendingChoiceCardButtons)
	{
		if (Button) Button->SetIsEnabled(false);
	}
	for (UButton* Button : RewardCardButtons)
	{
		if (Button) Button->SetIsEnabled(false);
	}
	for (UButton* Button : RouteRewardReplacementButtons)
	{
		if (Button) Button->SetIsEnabled(false);
	}
	if (EndTurnButton) EndTurnButton->SetIsEnabled(false);
	if (PendingChoiceCancelButton) PendingChoiceCancelButton->SetIsEnabled(false);
	if (EnemyIntentRecoveryButton) EnemyIntentRecoveryButton->SetIsEnabled(false);
	if (SkipRewardButton) SkipRewardButton->SetIsEnabled(false);
	SetTargetProxiesVisible(false);
}

bool UGameXXKBattleBoardWidget::ExecuteBattlePresentationContinuation(
	const EBattlePresentationContinuation Continuation)
{
	switch (Continuation)
	{
	case EBattlePresentationContinuation::None:
		return true;
	case EBattlePresentationContinuation::FinalizeCardMutation:
		return ResolveAndRefreshCardBattleAfterMutation();
	case EBattlePresentationContinuation::BeginEnemyIntentAfterPlayerPhase:
		BeginEnemyIntentPresentation();
		return ResolveAndRefreshCardBattleAfterMutation();
	case EBattlePresentationContinuation::ResumeEnemyIntentAfterMutation:
		EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Settle;
		EnemyIntentPresentationElapsed = 0.0f;
		return ResolveAndRefreshCardBattleAfterMutation();
	case EBattlePresentationContinuation::FinalizeEnemyPhase:
		ResetEnemyIntentPresentationState();
		return ResolveAndRefreshCardBattleAfterMutation();
	default:
		return false;
	}
}

void UGameXXKBattleBoardWidget::HandleBattlePresentationQueueDrained()
{
	if (!BattlePresentationQueue.IsEmpty())
	{
		return;
	}

	const EBattlePresentationContinuation Continuation = DeferredBattlePresentationContinuation;
	DeferredBattlePresentationContinuation = EBattlePresentationContinuation::None;
	for (const FName UnitId : DefeatedUnitVisualsPendingRemoval)
	{
		RemoveUnitVisual(UnitId);
	}
	DefeatedUnitVisualsPendingRemoval.Reset();
	DiscardPresentationHudSnapshot();
	ResetBattlePresentationFeedback();
	if (BattleCinematicImpact)
	{
		BattleCinematicImpact->HideForCinematic();
		BattleCinematicImpact->SetAtlas(nullptr);
	}
	if (BattleCinematicStatusIcon)
	{
		BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BattleCinematicReadout)
	{
		BattleCinematicReadout->SetText(FText::GetEmpty());
		BattleCinematicReadout->SetVisibility(ESlateVisibility::Hidden);
	}
	RestoreFormationAfterPresentation();

	if (Continuation != EBattlePresentationContinuation::None)
	{
		++ExecutedBattlePresentationContinuationCount;
		ExecuteBattlePresentationContinuation(Continuation);
		return;
	}
	RefreshProjectedUnitHuds();
	RefreshProgrammaticLayout();
}

UGameXXKBattleBoardWidget::FBattlePresentationQueueEntry*
UGameXXKBattleBoardWidget::FindPresentationEntry(const uint64 QueueSerial)
{
	return BattlePresentationQueue.FindByPredicate([QueueSerial](const FBattlePresentationQueueEntry& Entry)
	{
		return Entry.QueueSerial == QueueSerial;
	});
}

const UGameXXKBattleBoardWidget::FBattlePresentationQueueEntry*
UGameXXKBattleBoardWidget::GetActivePresentationEntry() const
{
	return !BattlePresentationQueue.IsEmpty() && BattlePresentationQueue[0].bStarted
		? &BattlePresentationQueue[0]
		: nullptr;
}

void UGameXXKBattleBoardWidget::PrefetchPresentationEntry(const uint64 QueueSerial)
{
	FBattlePresentationQueueEntry* const Entry = FindPresentationEntry(QueueSerial);
	if (!Entry || !AtlasCache || ActiveBattleVisualSessionToken == 0)
	{
		return;
	}

	PrefetchPresentationAtlas(QueueSerial, Entry->AttackerClip.TexturePath, EBattlePresentationAtlasRole::Attacker);
	PrefetchPresentationAtlas(QueueSerial, Entry->TargetClip.TexturePath, EBattlePresentationAtlasRole::Target);
}

void UGameXXKBattleBoardWidget::PrefetchPresentationAtlas(
	const uint64 QueueSerial,
	const FSoftObjectPath& TexturePath,
	const EBattlePresentationAtlasRole Role)
{
	FBattlePresentationQueueEntry* const Entry = FindPresentationEntry(QueueSerial);
	if (!Entry || !AtlasCache || ActiveBattleVisualSessionToken == 0 || !TexturePath.IsValid())
	{
		return;
	}

	if (!Entry->PinnedAtlasPaths.Contains(TexturePath))
	{
		AtlasCache->Pin(TexturePath);
		Entry->PinnedAtlasPaths.Add(TexturePath);
	}
	const uint64 RequestToken = ActiveBattleVisualSessionToken;
	const TWeakObjectPtr<UGameXXKBattleBoardWidget> WeakBoard(this);
	const TWeakObjectPtr<UGameXXKBattleUnitVisualWidget> RequestTargetVisual =
		Role == EBattlePresentationAtlasRole::Target
			? UnitVisuals.FindRef(Entry->Event.TargetUnitId)
			: Role == EBattlePresentationAtlasRole::Status
				? UnitVisuals.FindRef(Entry->StatusEvent.UnitId)
			: nullptr;
	AtlasCache->Acquire(
		TexturePath,
		RequestToken,
		[WeakBoard, RequestToken, QueueSerial, Role, RequestTargetVisual](
			UTexture2D* const Texture,
			const EGameXXKAtlasLoadResult Result)
		{
			UGameXXKBattleBoardWidget* const Board = WeakBoard.Get();
			if (!Board || Board->ActiveBattleVisualSessionToken != RequestToken)
			{
				return;
			}
			FBattlePresentationQueueEntry* const RequestEntry = Board->FindPresentationEntry(QueueSerial);
			if (!RequestEntry
				|| RequestEntry->bCompletionFired
				|| Result != EGameXXKAtlasLoadResult::Loaded
				|| !Texture)
			{
				return;
			}
			const auto UpgradeActiveAttackCinematic =
				[Board, RequestEntry, Texture](
					const FName UnitId,
					const bool bEnemy,
					const FGameXXKBattleAnimationClipDescriptor& Clip,
					const EBattlePresentationAtlasRole AtlasRole)
			{
				// The cinematic starts the moment the queue advances, before the
				// async atlas has necessarily finished streaming; if it began on
				// the idle fallback, swap the live visual to the real clip now.
				UGameXXKBattleUnitVisualWidget* const Visual = Board->UnitVisuals.FindRef(UnitId);
				if (!RequestEntry->bStarted
					|| RequestEntry->bCompletionFired
					|| !Clip.IsValid()
					|| !Visual)
				{
					return;
				}
				if (AtlasRole == EBattlePresentationAtlasRole::Attacker)
				{
					RequestEntry->AttackerAtlas = Texture;
					RequestEntry->PresentedAttackerClip = Clip;
				}
				else
				{
					RequestEntry->TargetAtlas = Texture;
					RequestEntry->PresentedTargetClip = Clip;
				}
				Visual->SetAtlas(Texture);
				Visual->ShowCinematic(
					Clip,
					bEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
				if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(Visual->Slot))
				{
					ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
				}
				Visual->AdvanceAtRealTime(RequestEntry->StartSeconds);
				Visual->AdvanceAtRealTime(Board->LastSlateSeconds);
			};

			switch (Role)
			{
			case EBattlePresentationAtlasRole::Attacker:
				RequestEntry->AttackerAtlas = Texture;
				if (Board->GetActivePresentationEntry() == RequestEntry
					&& RequestEntry->Kind == EBattlePresentationKind::AttackHit)
				{
					UpgradeActiveAttackCinematic(
						RequestEntry->Event.AttackerUnitId,
						RequestEntry->Event.bAttackerEnemy,
						RequestEntry->AttackerClip,
						Role);
				}
				break;
			case EBattlePresentationAtlasRole::Target:
				if (Board->GetActivePresentationEntry() == RequestEntry
					&& RequestEntry->Kind == EBattlePresentationKind::Death)
				{
					UGameXXKBattleUnitVisualWidget* const TargetVisual =
						Board->UnitVisuals.FindRef(RequestEntry->Event.TargetUnitId);
					if (RequestEntry->bStarted
						&& !RequestEntry->bCompletionFired
						&& RequestEntry->TargetClip.IsValid()
						&& TargetVisual
						&& TargetVisual == RequestTargetVisual.Get())
					{
						RequestEntry->TargetAtlas = Texture;
						RequestEntry->PresentedTargetClip = RequestEntry->TargetClip;
						TargetVisual->SetAtlas(Texture);
						TargetVisual->ShowCinematic(
							RequestEntry->TargetClip,
							RequestEntry->Event.bTargetEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
						if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(TargetVisual->Slot))
						{
							ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
						}
						TargetVisual->AdvanceAtRealTime(RequestEntry->StartSeconds);
						TargetVisual->AdvanceAtRealTime(Board->LastSlateSeconds);
					}
				}
				else
				{
					RequestEntry->TargetAtlas = Texture;
					if (Board->GetActivePresentationEntry() == RequestEntry
						&& RequestEntry->Kind == EBattlePresentationKind::AttackHit)
					{
						UpgradeActiveAttackCinematic(
							RequestEntry->Event.TargetUnitId,
							RequestEntry->Event.bTargetEnemy,
							RequestEntry->TargetClip,
							Role);
					}
				}
				break;
			case EBattlePresentationAtlasRole::Impact:
				RequestEntry->ImpactAtlas = Texture;
				if (Board->GetActivePresentationEntry() == RequestEntry
					&& RequestEntry->Kind == EBattlePresentationKind::AttackHit
					&& RequestEntry->bImpactFired
					&& !RequestEntry->bCompletionFired
					&& Board->BattleCinematicImpact)
				{
					Board->BattleCinematicImpact->SetAtlas(Texture);
					Board->BattleCinematicImpact->AdvanceAtRealTime(
						RequestEntry->StartSeconds + RequestEntry->Rhythm.ImpactSeconds);
					Board->BattleCinematicImpact->AdvanceAtRealTime(Board->LastSlateSeconds);
				}
				break;
			case EBattlePresentationAtlasRole::Status:
				RequestEntry->StatusAtlas = Texture;
				if (Board->GetActivePresentationEntry() == RequestEntry
					&& RequestEntry->Kind == EBattlePresentationKind::Status
					&& RequestEntry->bStarted
					&& !RequestEntry->bCompletionFired
					&& Board->BattleCinematicImpact)
				{
					Board->BattleCinematicImpact->SetAtlas(Texture);
					Board->BattleCinematicImpact->AdvanceAtRealTime(RequestEntry->StartSeconds);
					Board->BattleCinematicImpact->AdvanceAtRealTime(Board->LastSlateSeconds);
				}
				break;
			default: break;
			}
		});
}

void UGameXXKBattleBoardWidget::AdvanceBattlePresentation(const double AbsoluteSeconds)
{
	if (!FMath::IsFinite(AbsoluteSeconds))
	{
		return;
	}

	const TOptional<double> CommitCompletionBoundary = AdvancePlayedCardCommit(AbsoluteSeconds);
	if (bPlayedCardCommitActive)
	{
		UpdateBattlePresentationShake(AbsoluteSeconds);
		UpdateBattlePresentationReadout(AbsoluteSeconds);
		return;
	}

	double NextStartSeconds = CommitCompletionBoundary.IsSet()
		? CommitCompletionBoundary.GetValue()
		: AbsoluteSeconds;
	int32 CompletedEntryGuard = 0;
	bool bQueueDrained = false;
	while (!BattlePresentationQueue.IsEmpty() && CompletedEntryGuard++ < 256)
	{
		FBattlePresentationQueueEntry& Entry = BattlePresentationQueue[0];
		if (!Entry.bStarted)
		{
			StartPresentationEntry(Entry, NextStartSeconds);
		}

		const double ElapsedSeconds = FMath::Max(0.0, AbsoluteSeconds - Entry.StartSeconds);
		const double DurationSeconds = static_cast<double>(Entry.Rhythm.DurationSeconds);
		if (Entry.Kind == EBattlePresentationKind::AttackHit
			&& !Entry.bImpactFired
			&& ElapsedSeconds + static_cast<double>(KINDA_SMALL_NUMBER)
				>= static_cast<double>(Entry.Rhythm.ImpactSeconds))
		{
			FirePresentationImpact(Entry);
		}
		if (ElapsedSeconds < DurationSeconds)
		{
			break;
		}

		const double CompletionBoundarySeconds = Entry.StartSeconds + DurationSeconds;
		CompletePresentationEntry(Entry);
		BattlePresentationQueue.RemoveAt(0, 1, EAllowShrinking::No);
		if (BattlePresentationQueue.IsEmpty())
		{
			bQueueDrained = true;
			break;
		}
		NextStartSeconds = CompletionBoundarySeconds;
	}

	UpdateBattlePresentationShake(AbsoluteSeconds);
	UpdateBattlePresentationReadout(AbsoluteSeconds);
	if (bQueueDrained)
	{
		HandleBattlePresentationQueueDrained();
	}
}

void UGameXXKBattleBoardWidget::StartPresentationEntry(
	FBattlePresentationQueueEntry& Entry,
	const double StartSeconds)
{
	Entry.StartSeconds = StartSeconds;
	Entry.bStarted = true;
	Entry.bImpactFired = false;
	Entry.bCompletionFired = false;
	Entry.PresentedAttackerClip = FGameXXKBattleAnimationClipDescriptor();
	Entry.PresentedTargetClip = FGameXXKBattleAnimationClipDescriptor();

	HideFormationForPresentation();
	SetTargetProxiesVisible(false);
	if (BattleCinematicDimmer)
	{
		BattleCinematicDimmer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (BattleCinematicViewportCover)
	{
		BattleCinematicViewportCover->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (BattleCinematicImpact)
	{
		BattleCinematicImpact->HideForCinematic();
	}
	if (BattleCinematicStatusIcon)
	{
		BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BattleCinematicReadout)
	{
		BattleCinematicReadout->SetText(FText::GetEmpty());
		BattleCinematicReadout->SetVisibility(ESlateVisibility::Hidden);
	}

	if (Entry.Kind == EBattlePresentationKind::Status)
	{
		ApplyDisplayedStatusDelta(Entry.StatusEvent);
		UGameXXKBattleUnitVisualWidget* const UnitVisual = UnitVisuals.FindRef(Entry.StatusEvent.UnitId);
		if (UnitVisual)
		{
			const FGameXXKBattleAnimationClipDescriptor IdleClip =
				ResolveUnitAnimationClip(
					Entry.StatusEvent.UnitId,
					Entry.StatusEvent.bUnitEnemy,
					EGameXXKBattleAnimationAction::Idle);
			RestoreUnitIdleAtlas(Entry.StatusEvent.UnitId, UnitVisual);
			UnitVisual->ShowCinematic(
				IdleClip,
				Entry.StatusEvent.bUnitEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
			if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(UnitVisual->Slot))
			{
				ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
			}
			UnitVisual->AdvanceAtRealTime(Entry.StartSeconds);
		}
		if (BattleCinematicImpact)
		{
			if (Entry.StatusAtlas.IsValid())
			{
				BattleCinematicImpact->SetAtlas(Entry.StatusAtlas.Get());
			}
			BattleCinematicImpact->ShowCinematic(Entry.StatusClip, CinematicImpactAnchor);
			if (UCanvasPanelSlot* const ImpactSlot = Cast<UCanvasPanelSlot>(BattleCinematicImpact->Slot))
			{
				ImpactSlot->SetAnchors(FAnchors(CinematicImpactAnchor.X, CinematicImpactAnchor.Y));
				ImpactSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				ImpactSlot->SetPosition(FVector2D::ZeroVector);
				ImpactSlot->SetSize(CinematicImpactVisualSize);
				ImpactSlot->SetZOrder(BattleCinematicImpactZOrder);
			}
			BattleCinematicImpact->AdvanceAtRealTime(Entry.StartSeconds);
		}
		if (BattleCinematicStatusIcon)
		{
			FGameXXKBattleStatusBadgeModel Badge;
			Badge.Style = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(Entry.StatusEvent.Status);
			Badge.Stacks = FMath::Max(1, Entry.StatusEvent.StackAfter > 0
				? Entry.StatusEvent.StackAfter
				: Entry.StatusEvent.StackBefore);
			Badge.Tooltip = FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(Badge.Style, Badge.Stacks);
			BattleCinematicStatusIcon->SetBadgeModel(Badge);
			BattleCinematicStatusIcon->SetRenderScale(FVector2D(2.5f, 2.5f));
			BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (BattleCinematicReadout)
		{
			BattleCinematicReadout->SetText(FText::FromString(FString::Printf(
				TEXT("%+d"),
				Entry.StatusEvent.StackDelta)));
			BattleCinematicReadout->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	if (Entry.Kind == EBattlePresentationKind::Death)
	{
		SetDisplayedHealthOverlay(Entry.Event.TargetUnitId, Entry.Event.TargetHealthAfter);
		UGameXXKBattleUnitVisualWidget* const TargetVisual = UnitVisuals.FindRef(Entry.Event.TargetUnitId);
		if (TargetVisual)
		{
			const FGameXXKBattleAnimationClipDescriptor IdleClip =
				ResolveUnitAnimationClip(
					Entry.Event.TargetUnitId,
					Entry.Event.bTargetEnemy,
					EGameXXKBattleAnimationAction::Idle);
			Entry.PresentedTargetClip = Entry.TargetClip.IsValid() && Entry.TargetAtlas.IsValid()
				? Entry.TargetClip
				: IdleClip;
			if (Entry.TargetClip.IsValid() && Entry.TargetAtlas.IsValid())
			{
				TargetVisual->SetAtlas(Entry.TargetAtlas.Get());
			}
			else
			{
				RestoreUnitIdleAtlas(Entry.Event.TargetUnitId, TargetVisual);
			}
			TargetVisual->ShowCinematic(
				Entry.PresentedTargetClip,
				Entry.Event.bTargetEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
			if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(TargetVisual->Slot))
			{
				ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
			}
			TargetVisual->AdvanceAtRealTime(Entry.StartSeconds);
		}
		return;
	}

	SetDisplayedHealthOverlay(
		Entry.Event.TargetUnitId,
		Entry.Event.TargetHealthBefore,
		Entry.Event.TargetArmorBefore);
	UGameXXKBattleUnitVisualWidget* const AttackerVisual = UnitVisuals.FindRef(Entry.Event.AttackerUnitId);
	if (AttackerVisual)
	{
		const FGameXXKBattleAnimationClipDescriptor IdleClip =
			ResolveUnitAnimationClip(
				Entry.Event.AttackerUnitId,
				Entry.Event.bAttackerEnemy,
				EGameXXKBattleAnimationAction::Idle);
		Entry.PresentedAttackerClip = Entry.AttackerClip.IsValid() && Entry.AttackerAtlas.IsValid()
			? Entry.AttackerClip
			: IdleClip;
		if (Entry.AttackerClip.IsValid() && Entry.AttackerAtlas.IsValid())
		{
			AttackerVisual->SetAtlas(Entry.AttackerAtlas.Get());
		}
		else
		{
			RestoreUnitIdleAtlas(Entry.Event.AttackerUnitId, AttackerVisual);
		}
		AttackerVisual->ShowCinematic(
			Entry.PresentedAttackerClip,
			Entry.Event.bAttackerEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
		if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(AttackerVisual->Slot))
		{
			ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
		}
		AttackerVisual->AdvanceAtRealTime(Entry.StartSeconds);
	}

	UGameXXKBattleUnitVisualWidget* const TargetVisual = UnitVisuals.FindRef(Entry.Event.TargetUnitId);
	if (TargetVisual)
	{
		const FGameXXKBattleAnimationClipDescriptor IdleClip =
			ResolveUnitAnimationClip(
				Entry.Event.TargetUnitId,
				Entry.Event.bTargetEnemy,
				EGameXXKBattleAnimationAction::Idle);
		Entry.PresentedTargetClip = Entry.TargetClip.IsValid() && Entry.TargetAtlas.IsValid()
			? Entry.TargetClip
			: IdleClip;
		if (Entry.TargetClip.IsValid() && Entry.TargetAtlas.IsValid())
		{
			TargetVisual->SetAtlas(Entry.TargetAtlas.Get());
		}
		else
		{
			RestoreUnitIdleAtlas(Entry.Event.TargetUnitId, TargetVisual);
		}
		TargetVisual->ShowCinematic(
			Entry.PresentedTargetClip,
			Entry.Event.bTargetEnemy ? CinematicEnemyAnchor : CinematicPartyAnchor);
		if (UCanvasPanelSlot* const ParticipantSlot = Cast<UCanvasPanelSlot>(TargetVisual->Slot))
		{
			ParticipantSlot->SetZOrder(BattleCinematicParticipantZOrder);
		}
		TargetVisual->AdvanceAtRealTime(Entry.StartSeconds);
	}
}

void UGameXXKBattleBoardWidget::FirePresentationImpact(FBattlePresentationQueueEntry& Entry)
{
	if (Entry.bImpactFired || Entry.Kind != EBattlePresentationKind::AttackHit)
	{
		return;
	}
	Entry.bImpactFired = true;
	++BattlePresentationImpactCount;
	ApplyDisplayedDamagePacket(Entry.Event);

	if (BattleCinematicReadout)
	{
		FText Readout;
		if (Entry.Event.bAvoided)
		{
			Readout = NSLOCTEXT("GameXXK", "BattlePresentationAvoid", "闪避");
		}
		else if (Entry.Event.ArmorAbsorbed > 0 && Entry.Event.HealthDamage > 0)
		{
			Readout = FText::Format(
				NSLOCTEXT("GameXXK", "BattlePresentationArmorAndHealthDamage", "护甲 -{0} · 气血 -{1}"),
				FText::AsNumber(Entry.Event.ArmorAbsorbed),
				FText::AsNumber(Entry.Event.HealthDamage));
		}
		else if (Entry.Event.ArmorAbsorbed > 0)
		{
			Readout = FText::Format(
				NSLOCTEXT("GameXXK", "BattlePresentationArmorDamage", "护甲 -{0}"),
				FText::AsNumber(Entry.Event.ArmorAbsorbed));
		}
		else
		{
			Readout = FText::Format(
				NSLOCTEXT("GameXXK", "BattlePresentationDamage", "-{0}"),
				FText::AsNumber(Entry.Event.HealthDamage));
		}
		BattleCinematicReadout->SetText(Readout);
		BattleCinematicReadout->SetVisibility(ESlateVisibility::HitTestInvisible);
		BattleCinematicReadout->SetRenderScale(FVector2D(
			Entry.Rhythm.ReadoutPeakScale,
			Entry.Rhythm.ReadoutPeakScale));
		BattleCinematicReadout->SetRenderOpacity(1.0f);
	}

	const double ImpactAbsoluteSeconds = Entry.StartSeconds + Entry.Rhythm.ImpactSeconds;
	BattlePresentationReadoutStartSeconds = ImpactAbsoluteSeconds;
	BattlePresentationReadoutDurationSeconds = FMath::Max(
		static_cast<double>(Entry.Rhythm.DurationSeconds - Entry.Rhythm.ImpactSeconds),
		static_cast<double>(KINDA_SMALL_NUMBER));
	BattlePresentationReadoutPeakScale = Entry.Rhythm.ReadoutPeakScale;
	bBattlePresentationReadoutActive = true;

	BattlePresentationShakeStartSeconds = ImpactAbsoluteSeconds;
	BattlePresentationShakeAmplitude = FVector2D(
		Entry.Rhythm.ShakeAmplitude.X,
		Entry.Rhythm.ShakeAmplitude.Y);
	BattlePresentationShakeDurationSeconds = Entry.Rhythm.ShakeDurationSeconds;
	bBattlePresentationShakeActive = BattlePresentationShakeDurationSeconds > 0.0
		&& !BattlePresentationShakeAmplitude.IsNearlyZero();
	if (bBattlePresentationShakeActive)
	{
		++BattlePresentationHudShakeCount;
	}
	else if (ViewportRootCanvas)
	{
		ViewportRootCanvas->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UGameXXKBattleBoardWidget::CompletePresentationEntry(FBattlePresentationQueueEntry& Entry)
{
	if (Entry.bCompletionFired)
	{
		return;
	}
	Entry.bCompletionFired = true;
	++BattlePresentationCompletionCount;
	const FGameXXKBattlePresentationEvent CompletedEvent = Entry.Event;
	const EBattlePresentationKind CompletedKind = Entry.Kind;
	ReleasePresentationPins(Entry);

	if (BattleCinematicImpact)
	{
		BattleCinematicImpact->HideForCinematic();
		BattleCinematicImpact->SetAtlas(nullptr);
	}
	if (BattleCinematicReadout)
	{
		BattleCinematicReadout->SetText(FText::GetEmpty());
		BattleCinematicReadout->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BattleCinematicStatusIcon)
	{
		BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	ResetBattlePresentationFeedback();

	if (CompletedKind == EBattlePresentationKind::Death)
	{
		const bool bNeedsLaterStatusPresentation = BattlePresentationQueue.ContainsByPredicate(
			[&CompletedEvent](const FBattlePresentationQueueEntry& QueuedEntry)
			{
				return QueuedEntry.Kind == EBattlePresentationKind::Status
					&& QueuedEntry.StatusEvent.UnitId == CompletedEvent.TargetUnitId;
			});
		if (bNeedsLaterStatusPresentation)
		{
			// Death stays immediately after the lethal Hit, but a status delta from the
			// same rule packet still needs the affected unit as its central participant.
			DefeatedUnitVisualsPendingRemoval.Add(CompletedEvent.TargetUnitId);
			RestoreFormationAfterPresentation();
			return;
		}
		RemoveUnitVisual(CompletedEvent.TargetUnitId);
		RestoreFormationAfterPresentation(CompletedEvent.TargetUnitId);
		return;
	}

	RestoreFormationAfterPresentation();
	if (CompletedKind == EBattlePresentationKind::AttackHit && CompletedEvent.bTargetDefeated)
	{
		EnqueueDeathPresentationAfterActive(CompletedEvent);
	}
}

void UGameXXKBattleBoardWidget::EnqueueDeathPresentationAfterActive(
	const FGameXXKBattlePresentationEvent& Event)
{
	FBattlePresentationQueueEntry DeathEntry;
	DeathEntry.Event = Event;
	DeathEntry.Kind = EBattlePresentationKind::Death;
	DeathEntry.Rhythm = FGameXXKBattleAnimationPresentation::ResolveDeathRhythm();
	DeathEntry.QueueSerial = NextBattlePresentationQueueSerial++;
	if (NextBattlePresentationQueueSerial == 0)
	{
		NextBattlePresentationQueueSerial = 1;
	}
	DeathEntry.TargetClip = ResolveUnitAnimationClip(
		Event.TargetUnitId,
		Event.bTargetEnemy,
		EGameXXKBattleAnimationAction::Death);
	DeathEntry.TargetClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(
		DeathEntry.TargetClip,
		DeathEntry.Rhythm.DurationSeconds);
	const uint64 QueueSerial = DeathEntry.QueueSerial;
	BattlePresentationQueue.Insert(MoveTemp(DeathEntry), FMath::Min(1, BattlePresentationQueue.Num()));
	PrefetchPresentationEntry(QueueSerial);
}

void UGameXXKBattleBoardWidget::ReleasePresentationPins(FBattlePresentationQueueEntry& Entry)
{
	if (AtlasCache)
	{
		for (const FSoftObjectPath& Path : Entry.PinnedAtlasPaths)
		{
			AtlasCache->Unpin(Path);
		}
	}
	Entry.PinnedAtlasPaths.Reset();
}

void UGameXXKBattleBoardWidget::ResetBattlePresentation()
{
	ResetPlayedCardCommit(true);
	DeferredBattlePresentationContinuation = EBattlePresentationContinuation::None;
	for (FBattlePresentationQueueEntry& Entry : BattlePresentationQueue)
	{
		ReleasePresentationPins(Entry);
	}
	BattlePresentationQueue.Reset();
	for (const FName UnitId : DefeatedUnitVisualsPendingRemoval)
	{
		RemoveUnitVisual(UnitId);
	}
	DefeatedUnitVisualsPendingRemoval.Reset();
	DiscardPresentationHudSnapshot();
	BattlePresentationImpactCount = 0;
	BattlePresentationCompletionCount = 0;
	BattlePresentationHudShakeCount = 0;
	ExecutedBattlePresentationContinuationCount = 0;
	PlayedCardCommitCompletionCount = 0;
	ResetBattlePresentationFeedback();
	RestoreFormationAfterPresentation();
	if (BattleCinematicImpact)
	{
		BattleCinematicImpact->HideForCinematic();
		BattleCinematicImpact->SetAtlas(nullptr);
	}
	if (BattleCinematicReadout)
	{
		BattleCinematicReadout->SetText(FText::GetEmpty());
		BattleCinematicReadout->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BattleCinematicStatusIcon)
	{
		BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	if (ViewportRootCanvas)
	{
		ViewportRootCanvas->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

void UGameXXKBattleBoardWidget::HideFormationForPresentation()
{
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (Pair.Value)
		{
			Pair.Value->HideForCinematic();
		}
	}
}

void UGameXXKBattleBoardWidget::RestoreFormationAfterPresentation(const FName RemovedUnitId)
{
	if (IsBattlePresentationPending())
	{
		SetTargetProxiesVisible(false);
		return;
	}
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (!Pair.Value || Pair.Key == RemovedUnitId)
		{
			continue;
		}
		RestoreUnitIdleAtlas(Pair.Key, Pair.Value);
		Pair.Value->RestoreFormation();
	}
	SetTargetProxiesVisible(true);
	if (BattleCinematicDimmer)
	{
		BattleCinematicDimmer->SetVisibility(ESlateVisibility::Hidden);
	}
	if (BattleCinematicViewportCover)
	{
		BattleCinematicViewportCover->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UGameXXKBattleBoardWidget::RestoreUnitIdleAtlas(
	const FName UnitId,
	UGameXXKBattleUnitVisualWidget* const Visual)
{
	if (Visual)
	{
		Visual->SetAtlas(UnitIdleAtlasTextures.FindRef(UnitId));
	}
}

FGameXXKBattleAnimationClipDescriptor UGameXXKBattleBoardWidget::ResolveUnitAnimationClip(
	const FName UnitId,
	const bool bEnemy,
	const EGameXXKBattleAnimationAction Action) const
{
	FName EnemyDefinitionId = NAME_None;
	if (bEnemy)
	{
		const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
		if (State && State->CardRun.bHasActiveCardBattle)
		{
			if (const FGameXXKCardCombatUnit* const Unit = State->CardRun.ActiveBattle.Units.FindByPredicate(
				[UnitId](const FGameXXKCardCombatUnit& Candidate)
				{
					return Candidate.UnitId == UnitId;
				}))
			{
				EnemyDefinitionId = Unit->EnemyDefinitionId;
			}
		}
	}

	return FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
		UnitId,
		EnemyDefinitionId,
		bEnemy,
		Action);
}

void UGameXXKBattleBoardWidget::SetTargetProxiesVisible(const bool bVisible)
{
	const bool bActuallyVisible = bVisible && !IsBattlePresentationPending();
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitTargetProxyButton>>& Pair : UnitTargetProxies)
	{
		if (Pair.Value)
		{
			Pair.Value->SetIsEnabled(bActuallyVisible);
			Pair.Value->SetVisibility(bActuallyVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		}
	}
}

void UGameXXKBattleBoardWidget::SetDisplayedHealthOverlay(
	const FName UnitId,
	const int32 Health,
	const int32 Armor)
{
	if (!UnitId.IsNone())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[HPOverlay] unit=%s health=%d"), *UnitId.ToString(), Health);
		DisplayedHealthOverrides.Add(UnitId, FMath::Max(0, Health));
		if (FGameXXKBattleUnitHudView* const View = DisplayedUnitHudOverrides.Find(UnitId))
		{
			View->CurrentHP = FMath::Max(0, Health);
			if (Armor != INDEX_NONE)
			{
				View->Armor = FMath::Max(0, Armor);
			}
			View->bLiving = true;
		}
		RefreshProjectedUnitHuds();
	}
}

void UGameXXKBattleBoardWidget::ApplyDisplayedDamagePacket(
	const FGameXXKBattlePresentationEvent& Event)
{
	if (Event.TargetUnitId.IsNone())
	{
		return;
	}

	const int32 DisplayedHealth = FMath::Max(0, Event.TargetHealthAfter);
	DisplayedHealthOverrides.Add(Event.TargetUnitId, DisplayedHealth);
	FGameXXKBattleUnitHudView* View = DisplayedUnitHudOverrides.Find(Event.TargetUnitId);
	if (!View)
	{
		const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
		FGameXXKBattleUnitHudView Snapshot;
		if (State
			&& State->CardRun.bHasActiveCardBattle
			&& FGameXXKBattlePresentation::BuildUnitHudView(
				State->CardRun.ActiveBattle,
				Event.TargetUnitId,
				ResolveProjectedUnitHudDisplayName(Event.TargetUnitId),
				Snapshot))
		{
			DisplayedUnitHudOverrides.Add(Event.TargetUnitId, MoveTemp(Snapshot));
			View = DisplayedUnitHudOverrides.Find(Event.TargetUnitId);
		}
	}
	if (View)
	{
		View->CurrentHP = DisplayedHealth;
		View->Armor = FMath::Max(0, Event.TargetArmorAfter);
		View->bLiving = true;
	}
	RefreshProjectedUnitHuds();
}

void UGameXXKBattleBoardWidget::ClearDisplayedHealthOverlay(const FName UnitId)
{
	if (DisplayedHealthOverrides.Remove(UnitId) > 0)
	{
		RefreshProjectedUnitHuds();
	}
}

bool UGameXXKBattleBoardWidget::IsUnitRetainedByPresentation(const FName UnitId) const
{
	return DisplayedHealthOverrides.Contains(UnitId) || DisplayedUnitHudOverrides.Contains(UnitId);
}

void UGameXXKBattleBoardWidget::ApplyDisplayedStatusDelta(
	const FGameXXKBattleStatusPresentationEvent& Event)
{
	FGameXXKBattleUnitHudView* const View = DisplayedUnitHudOverrides.Find(Event.UnitId);
	if (!View)
	{
		return;
	}
	View->Statuses.RemoveAll([&Event](const FGameXXKCardStatusStack& Stack)
	{
		return Stack.Status == Event.Status;
	});
	if (Event.StackAfter > 0)
	{
		FGameXXKCardStatusStack& Stack = View->Statuses.AddDefaulted_GetRef();
		Stack.Status = Event.Status;
		Stack.Stacks = Event.StackAfter;
	}
	View->Statuses.Sort([](const FGameXXKCardStatusStack& Left, const FGameXXKCardStatusStack& Right)
	{
		return static_cast<uint8>(Left.Status) < static_cast<uint8>(Right.Status);
	});
	RefreshProjectedUnitHuds();
}

void UGameXXKBattleBoardWidget::UpdateBattlePresentationShake(const double AbsoluteSeconds)
{
	// Shake the full board root (backdrop + safe stage + cinematic cover) so the
	// impact reads as a whole-screen shake instead of only the letterboxed
	// design stage sliding against a static backdrop.
	if (!ViewportRootCanvas || !bBattlePresentationShakeActive)
	{
		return;
	}
	const double ElapsedSeconds = AbsoluteSeconds - BattlePresentationShakeStartSeconds;
	if (ElapsedSeconds < 0.0
		|| BattlePresentationShakeDurationSeconds <= 0.0
		|| ElapsedSeconds >= BattlePresentationShakeDurationSeconds)
	{
		bBattlePresentationShakeActive = false;
		ViewportRootCanvas->SetRenderTranslation(FVector2D::ZeroVector);
		return;
	}

	const double Decay = 1.0 - (ElapsedSeconds / BattlePresentationShakeDurationSeconds);
	const float X = static_cast<float>(
		FMath::Sin(ElapsedSeconds * 85.0) * BattlePresentationShakeAmplitude.X * Decay);
	const float Y = static_cast<float>(
		FMath::Cos(ElapsedSeconds * 110.0) * BattlePresentationShakeAmplitude.Y * Decay);
	ViewportRootCanvas->SetRenderTranslation(FVector2D(X, Y));
}

void UGameXXKBattleBoardWidget::UpdateBattlePresentationReadout(const double AbsoluteSeconds)
{
	if (!BattleCinematicReadout || !bBattlePresentationReadoutActive)
	{
		return;
	}

	const double ElapsedSeconds = AbsoluteSeconds - BattlePresentationReadoutStartSeconds;
	if (ElapsedSeconds < 0.0)
	{
		return;
	}
	const double FadeSeconds = FMath::Max(
		BattlePresentationReadoutDurationSeconds - BattleReadoutPeakHoldSeconds,
		static_cast<double>(KINDA_SMALL_NUMBER));
	const double FadeElapsedSeconds = FMath::Max(0.0, ElapsedSeconds - BattleReadoutPeakHoldSeconds);
	const float Progress = static_cast<float>(FMath::Clamp(FadeElapsedSeconds / FadeSeconds, 0.0, 1.0));
	const float SettleAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Progress, 2.0f);
	const float Scale = FMath::Lerp(BattlePresentationReadoutPeakScale, 1.0f, SettleAlpha);
	BattleCinematicReadout->SetRenderScale(FVector2D(Scale, Scale));
	BattleCinematicReadout->SetRenderOpacity(1.0f - Progress);
	if (Progress >= 1.0f)
	{
		bBattlePresentationReadoutActive = false;
	}
}

void UGameXXKBattleBoardWidget::ResetBattlePresentationFeedback()
{
	BattlePresentationShakeStartSeconds = 0.0;
	BattlePresentationShakeDurationSeconds = 0.0;
	BattlePresentationShakeAmplitude = FVector2D::ZeroVector;
	bBattlePresentationShakeActive = false;
	BattlePresentationReadoutStartSeconds = 0.0;
	BattlePresentationReadoutDurationSeconds = 0.0;
	BattlePresentationReadoutPeakScale = 1.0f;
	bBattlePresentationReadoutActive = false;
	if (ViewportRootCanvas)
	{
		ViewportRootCanvas->SetRenderTranslation(FVector2D::ZeroVector);
	}
	if (BattleCinematicReadout)
	{
		BattleCinematicReadout->SetRenderScale(FVector2D(1.0f, 1.0f));
		BattleCinematicReadout->SetRenderOpacity(1.0f);
	}
}

bool UGameXXKBattleBoardWidget::BeginBattleVisualSession(const uint64 SessionToken)
{
	if (SessionToken == 0)
	{
		return false;
	}
	if (ActiveBattleVisualSessionToken == SessionToken)
	{
		RefreshUnitVisuals();
		return true;
	}
	if (ActiveBattleVisualSessionToken != 0)
	{
		return false;
	}

	BuildProgrammaticLayout();
	if (!BattleDesignStage)
	{
		return false;
	}
	if (!AtlasCache)
	{
		AtlasCache = MakeUnique<FGameXXKBattleAtlasCache>();
	}

	ActiveBattleVisualSessionToken = SessionToken;
	RefreshUnitVisuals();
	for (const FBattlePresentationQueueEntry& Entry : BattlePresentationQueue)
	{
		PrefetchPresentationEntry(Entry.QueueSerial);
	}
	return ActiveBattleVisualSessionToken == SessionToken;
}

void UGameXXKBattleBoardWidget::CancelBattleVisualSession(const uint64 ClosingSessionToken)
{
	if (ClosingSessionToken == 0 || ClosingSessionToken != ActiveBattleVisualSessionToken)
	{
		return;
	}
	ClearCardOutcomePreview();
	ResetBattlePresentation();

	// Invalidate the Board first. CancelSession may synchronously deliver callbacks,
	// and those callbacks must observe a stale Board token before touching widgets.
	ActiveBattleVisualSessionToken = 0;
	if (AtlasCache)
	{
		AtlasCache->CancelSession(ClosingSessionToken);
	}

	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (Pair.Value)
		{
			Pair.Value->SetAtlas(nullptr);
		}
	}
	if (AtlasCache)
	{
		for (const TPair<FName, FSoftObjectPath>& Pair : PinnedUnitAtlasPaths)
		{
			AtlasCache->Unpin(Pair.Value);
		}
	}
	PinnedUnitAtlasPaths.Reset();
	RequestedUnitAtlasPaths.Reset();
	UnitIdleAtlasTextures.Reset();

	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitTargetProxyButton>>& Pair : UnitTargetProxies)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromParent();
		}
	}
	UnitVisuals.Reset();
	UnitTargetProxies.Reset();
	UnitTargetPlaceholders.Reset();
	LastSlateSeconds = 0.0;
}

void UGameXXKBattleBoardWidget::AdvanceVisualsAtRealTime(const double AbsoluteSeconds)
{
	if (!FMath::IsFinite(AbsoluteSeconds))
	{
		return;
	}
	LastSlateSeconds = AbsoluteSeconds;
	if (AtlasCache)
	{
		AtlasCache->AdvanceTimeouts(AbsoluteSeconds);
	}
	AdvanceBattlePresentation(AbsoluteSeconds);
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (Pair.Value)
		{
			Pair.Value->AdvanceAtRealTime(AbsoluteSeconds);
		}
	}
	if (BattleCinematicImpact)
	{
		BattleCinematicImpact->AdvanceAtRealTime(AbsoluteSeconds);
	}
}

void UGameXXKBattleBoardWidget::HandleUnitTargetProxyClicked(const FName UnitId)
{
	if (UnitId.IsNone())
	{
		return;
	}
	if (RejectBattlePresentationMutation())
	{
		return;
	}
	if (IsCardTargetingActive())
	{
		ConfirmTargetingUnit(UnitId);
		return;
	}
	if (!IsTargetingBattleActionForTest())
	{
		return;
	}

	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State)
	{
		return;
	}
	const int32 EnemyIndex = State->ActiveBattleEnemies.IndexOfByPredicate([UnitId](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == UnitId;
	});
	if (EnemyIndex != INDEX_NONE)
	{
		ConfirmTargetingEnemy(EnemyIndex);
	}
}

void UGameXXKBattleBoardWidget::HandleUnitTargetProxyHoverChanged(const FName UnitId, const bool bHovered)
{
	if (bHovered)
	{
		if (IsCardTargetingActive() && LegalCardTargetUnitIds.Contains(UnitId))
		{
			FVector2D TargetStageCenter;
			if (TryResolveUnitTargetStageCenter(UnitId, TargetStageCenter))
			{
				TargetingPointerPosition = TargetStageCenter;
			}
			BuildCardOutcomePreview(PendingCardPreview.CardInstanceId, UnitId);
		}
		else
		{
			ClearCardOutcomePreview();
		}
		return;
	}

	if (CachedOutcomeTargetUnitId == UnitId)
	{
		ClearCardOutcomePreview();
	}
}

int32 UGameXXKBattleBoardWidget::GetLayerZ(const EGameXXKBattleHudLayer Layer) const
{
	switch (Layer)
	{
	case EGameXXKBattleHudLayer::Backdrop: return BattleBackdropZOrder;
	case EGameXXKBattleHudLayer::Formation: return BattleFormationZOrder;
	case EGameXXKBattleHudLayer::TargetProxy: return BattleTargetProxyBaseZOrder + 1;
	case EGameXXKBattleHudLayer::Controls: return BattleControlsZOrder;
	default: return INDEX_NONE;
	}
}

UCanvasPanel* UGameXXKBattleBoardWidget::GetBattleViewportRootForTest() const
{
	return ViewportRootCanvas;
}

UCanvasPanel* UGameXXKBattleBoardWidget::GetBattleDesignStageForTest() const
{
	return BattleDesignStage;
}

UCanvasPanel* UGameXXKBattleBoardWidget::GetBattleControlsLayerForTest() const
{
	return RootCanvas;
}

UScaleBox* UGameXXKBattleBoardWidget::GetBattleBackdropScaleBoxForTest() const
{
	return BattleBackdropScaleBox;
}

UImage* UGameXXKBattleBoardWidget::GetBattleBackdropImageForTest() const
{
	return BattleBackdropImage;
}

FString UGameXXKBattleBoardWidget::GetBattleBackdropResourcePathForTest() const
{
	return BattleBackdropResourcePath;
}

FString UGameXXKBattleBoardWidget::ResolveBattleBackdropTexturePath(const EGameXXKCardTerrain Terrain)
{
	const int32 TerrainIndex = static_cast<int32>(Terrain);
	if (TerrainIndex >= static_cast<int32>(EGameXXKCardTerrain::Plain)
		&& TerrainIndex <= static_cast<int32>(EGameXXKCardTerrain::Cave))
	{
		return TerrainBattleBackdropTexturePaths[TerrainIndex];
	}
	return RiversideBattleBackdropTexturePath;
}

void UGameXXKBattleBoardWidget::ApplyBattleBackdropForTerrain(const EGameXXKCardTerrain Terrain)
{
	BattleBackdropResourcePath = ResolveBattleBackdropTexturePath(Terrain);
	BattleBackdropTerrain = Terrain;
	BattleBackdropTexture = LoadObject<UTexture2D>(nullptr, *BattleBackdropResourcePath);
	if (BattleBackdropTexture && BattleBackdropImage)
	{
		BattleBackdropImage->SetBrushFromTexture(BattleBackdropTexture, true);
	}
}

UGameXXKBattleUnitVisualWidget* UGameXXKBattleBoardWidget::GetUnitVisualForTest(const FName UnitId) const
{
	return UnitVisuals.FindRef(UnitId);
}

UButton* UGameXXKBattleBoardWidget::GetUnitTargetProxyForTest(const FName UnitId) const
{
	return UnitTargetProxies.FindRef(UnitId);
}

int32 UGameXXKBattleBoardWidget::GetUnitVisualCountForTest() const
{
	return UnitVisuals.Num();
}

bool UGameXXKBattleBoardWidget::IsUnitTargetPlaceholderVisibleForTest(const FName UnitId) const
{
	const UTextBlock* const Placeholder = UnitTargetPlaceholders.FindRef(UnitId);
	return Placeholder
		&& Placeholder->GetVisibility() != ESlateVisibility::Hidden
		&& Placeholder->GetVisibility() != ESlateVisibility::Collapsed;
}

uint64 UGameXXKBattleBoardWidget::GetActiveBattleVisualSessionTokenForTest() const
{
	return ActiveBattleVisualSessionToken;
}

int32 UGameXXKBattleBoardWidget::GetPinnedBattleAtlasCountForTest() const
{
	return PinnedUnitAtlasPaths.Num();
}

int32 UGameXXKBattleBoardWidget::GetDuplicateParticipantImageCountForTest() const
{
	if (!WidgetTree)
	{
		return 0;
	}
	int32 Count = 0;
	Count += WidgetTree->FindWidget(TEXT("BattleAnimationLeftUnit")) ? 1 : 0;
	Count += WidgetTree->FindWidget(TEXT("BattleAnimationRightUnit")) ? 1 : 0;
	return Count;
}

FGameXXKBattleAtlasCacheStats UGameXXKBattleBoardWidget::GetAtlasCacheStatsForTest() const
{
	return AtlasCache ? AtlasCache->GetStats() : FGameXXKBattleAtlasCacheStats();
}

bool UGameXXKBattleBoardWidget::IsBattlePresentationActiveForTest() const
{
	return GetActivePresentationEntry() != nullptr;
}

bool UGameXXKBattleBoardWidget::IsBattlePresentationLockedForTest() const
{
	return IsBattlePresentationPending();
}

bool UGameXXKBattleBoardWidget::IsBattleDeathPresentationActiveForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind == EBattlePresentationKind::Death;
}

bool UGameXXKBattleBoardWidget::IsBattleStatusPresentationActiveForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind == EBattlePresentationKind::Status;
}

int32 UGameXXKBattleBoardWidget::GetBattlePresentationQueueCountForTest() const
{
	return FMath::Max(0, BattlePresentationQueue.Num() - (GetActivePresentationEntry() ? 1 : 0));
}

uint64 UGameXXKBattleBoardWidget::GetActiveBattlePresentationEventIdForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry
		? Entry->Kind == EBattlePresentationKind::Status
			? Entry->StatusEvent.EventId
			: Entry->Event.EventId
		: 0;
}

FName UGameXXKBattleBoardWidget::GetActiveBattlePresentationAttackerUnitIdForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind != EBattlePresentationKind::Status
		? Entry->Event.AttackerUnitId
		: NAME_None;
}

FName UGameXXKBattleBoardWidget::GetActiveBattlePresentationTargetUnitIdForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry
		? Entry->Kind == EBattlePresentationKind::Status
			? Entry->StatusEvent.UnitId
			: Entry->Event.TargetUnitId
		: NAME_None;
}

double UGameXXKBattleBoardWidget::GetActiveBattlePresentationElapsedForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry ? FMath::Max(0.0, LastSlateSeconds - Entry->StartSeconds) : 0.0;
}

double UGameXXKBattleBoardWidget::GetActiveBattlePresentationDurationForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	if (!Entry)
	{
		return 0.0;
	}
	return static_cast<double>(Entry->Rhythm.DurationSeconds);
}

int32 UGameXXKBattleBoardWidget::GetBattlePresentationImpactCountForTest() const
{
	return BattlePresentationImpactCount;
}

int32 UGameXXKBattleBoardWidget::GetBattlePresentationCompletionCountForTest() const
{
	return BattlePresentationCompletionCount;
}

int32 UGameXXKBattleBoardWidget::GetBattlePresentationHudShakeCountForTest() const
{
	return BattlePresentationHudShakeCount;
}

FVector2D UGameXXKBattleBoardWidget::GetBattlePresentationShakeAmplitudeForTest() const
{
	return BattlePresentationShakeAmplitude;
}

double UGameXXKBattleBoardWidget::GetBattlePresentationShakeDurationForTest() const
{
	return BattlePresentationShakeDurationSeconds;
}

int32 UGameXXKBattleBoardWidget::GetExecutedBattlePresentationContinuationCountForTest() const
{
	return ExecutedBattlePresentationContinuationCount;
}

bool UGameXXKBattleBoardWidget::IsPlayedCardCommitActiveForTest() const
{
	return bPlayedCardCommitActive;
}

FName UGameXXKBattleBoardWidget::GetPlayedCardCommitInstanceIdForTest() const
{
	return PlayedCardCommitInstanceId;
}

double UGameXXKBattleBoardWidget::GetPlayedCardCommitElapsedForTest() const
{
	return PlayedCardCommitElapsedSeconds;
}

FVector2D UGameXXKBattleBoardWidget::GetPlayedCardCommitTranslationForTest() const
{
	return PlayedCardCommitButton.IsValid()
		? PlayedCardCommitButton->GetRenderTransform().Translation
		: FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetPlayedCardCommitScaleForTest() const
{
	return PlayedCardCommitButton.IsValid()
		? PlayedCardCommitButton->GetRenderTransform().Scale
		: FVector2D(1.0f, 1.0f);
}

float UGameXXKBattleBoardWidget::GetPlayedCardCommitOpacityForTest() const
{
	return PlayedCardCommitButton.IsValid()
		? PlayedCardCommitButton->GetRenderOpacity()
		: 1.0f;
}

int32 UGameXXKBattleBoardWidget::GetPlayedCardCommitCompletionCountForTest() const
{
	return PlayedCardCommitCompletionCount;
}

FString UGameXXKBattleBoardWidget::GetActiveBattleStatusAnimationAssetIdForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind == EBattlePresentationKind::Status
		? Entry->StatusClip.AssetId
		: FString();
}

int32 UGameXXKBattleBoardWidget::GetActiveBattleStatusDeltaForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind == EBattlePresentationKind::Status
		? Entry->StatusEvent.StackDelta
		: 0;
}

FName UGameXXKBattleBoardWidget::GetActiveBattleStatusIconIdForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->Kind == EBattlePresentationKind::Status
		? FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(Entry->StatusEvent.Status).IconId
		: NAME_None;
}

int32 UGameXXKBattleBoardWidget::GetDisplayedHealthForTest(const FName UnitId) const
{
	if (const int32* const Override = DisplayedHealthOverrides.Find(UnitId))
	{
		return *Override;
	}
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || !State->CardRun.bHasActiveCardBattle)
	{
		return 0;
	}
	const FGameXXKCardCombatUnit* const Unit = State->CardRun.ActiveBattle.Units.FindByPredicate(
		[UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	return Unit ? Unit->HP : 0;
}

float UGameXXKBattleBoardWidget::GetActiveAttackerPlaybackRateForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry ? Entry->PresentedAttackerClip.PlaybackRate : 0.0f;
}

float UGameXXKBattleBoardWidget::GetActiveTargetPlaybackRateForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry ? Entry->PresentedTargetClip.PlaybackRate : 0.0f;
}

float UGameXXKBattleBoardWidget::GetActiveImpactPlaybackRateForTest() const
{
	const FBattlePresentationQueueEntry* const Entry = GetActivePresentationEntry();
	return Entry && Entry->ImpactClip.IsValid() ? Entry->ImpactClip.PlaybackRate : 0.0f;
}

FString UGameXXKBattleBoardWidget::GetBattlePresentationReadoutForTest() const
{
	return BattleCinematicReadout ? BattleCinematicReadout->GetText().ToString() : FString();
}

FVector2D UGameXXKBattleBoardWidget::GetBattlePresentationReadoutScaleForTest() const
{
	return BattleCinematicReadout
		? BattleCinematicReadout->GetRenderTransform().Scale
		: FVector2D::ZeroVector;
}

float UGameXXKBattleBoardWidget::GetBattlePresentationReadoutOpacityForTest() const
{
	return BattleCinematicReadout ? BattleCinematicReadout->GetRenderOpacity() : 0.0f;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKBattleBoardWidget::SetAtlasCacheForTest(TUniquePtr<FGameXXKBattleAtlasCache> InAtlasCache)
{
	const uint64 ExistingSessionToken = ActiveBattleVisualSessionToken;
	if (ActiveBattleVisualSessionToken != 0)
	{
		CancelBattleVisualSession(ActiveBattleVisualSessionToken);
	}
	if (AtlasCache)
	{
		AtlasCache->Clear();
	}
	AtlasCache = MoveTemp(InAtlasCache);
	if (ExistingSessionToken != 0 && AtlasCache)
	{
		BeginBattleVisualSession(ExistingSessionToken);
	}
}
#endif

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

	const auto StageToBoardLocal = [this, &AllottedGeometry](const FVector2D StagePosition) -> FVector2D
	{
		if (BattleDesignStage)
		{
			const FGeometry StageGeometry = BattleDesignStage->GetCachedGeometry();
			if (StageGeometry.GetLocalSize().X > 1.0f && StageGeometry.GetLocalSize().Y > 1.0f)
			{
				return AllottedGeometry.AbsoluteToLocal(StageGeometry.LocalToAbsolute(StagePosition));
			}
		}
		return StagePosition;
	};
	const FVector2D Start = StageToBoardLocal(TargetingSourcePosition);
	const FVector2D End = StageToBoardLocal(TargetingPointerPosition);
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
	if (GAliveBattleBoardInstances > 1)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Board] refresh name=%s inViewport=%d"), *GetName(), IsInViewport());
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bInBattle = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle;
	if (bInBattle && IsBattlePresentationPending())
	{
		SetVisibility(ESlateVisibility::Visible);
		if (ActiveBattleVisualSessionToken == 0)
		{
			// The visual session is gone, so NativeTick can never advance the
			// presentation queue and the HP overrides would stay frozen forever.
			// Release the whole presentation, then run the deferred continuation
			// so terminal victory/defeat resolution and HUD refresh still happen.
			const EBattlePresentationContinuation StuckContinuation = DeferredBattlePresentationContinuation;
			ResetBattlePresentation();
			if (StuckContinuation != EBattlePresentationContinuation::None)
			{
				++ExecutedBattlePresentationContinuationCount;
				ExecuteBattlePresentationContinuation(StuckContinuation);
				return;
			}
			RefreshProjectedUnitHuds();
		}
		else
		{
			ApplyBattlePresentationInteractionLock();
			// Push current overrides (or live runtime if overrides were discarded) to
			// widgets so HP text never freezes even when the presentation queue is alive.
			// Non-participants still show live state thanks to the participant gating.
			RefreshProjectedUnitHuds();
		}
		return;
	}
	// A resolved terrain-switch formation card (e.g. 定阵) changes the active battle
	// terrain; swap the backdrop whenever it no longer matches what is displayed.
	const EGameXXKCardTerrain ActiveBattleTerrain = bInBattle
		&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
			? Subsystem->GetRuntimeState().CardRun.ActiveBattle.Terrain
			: EGameXXKCardTerrain::Invalid;
	if (ActiveBattleTerrain != BattleBackdropTerrain)
	{
		ApplyBattleBackdropForTerrain(ActiveBattleTerrain);
	}
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
		ResetBattlePresentation();
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
	RefreshUnitVisuals();

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
	if (RejectBattlePresentationMutation())
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
	FVector2D LockedTargetStageCenter;
	if (IsCardTargetingActive()
		&& !CachedOutcomeTargetUnitId.IsNone()
		&& LegalCardTargetUnitIds.Contains(CachedOutcomeTargetUnitId)
		&& TryResolveUnitTargetStageCenter(CachedOutcomeTargetUnitId, LockedTargetStageCenter))
	{
		ScreenPosition = LockedTargetStageCenter;
	}
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
	if (RejectBattlePresentationMutation())
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
	ClearCardOutcomePreview();
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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
	ClearCardOutcomePreview();
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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

	const FGameXXKCardBattleRuntime Before = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	FGameXXKCardPlayResult Result;
	FString Error;
	if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
		Subsystem->GetMutableRuntimeState(),
		PendingCardPreview.CardInstanceId,
		UnitId,
		Result,
		&Error))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshPendingCardTargetingPreview();
		RefreshProgrammaticLayout();
		return false;
	}

	ClearCardTargetingState();
	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		Result.DamageResults,
		EBattlePresentationContinuation::FinalizeCardMutation,
		Result.CardInstanceId);
}

bool UGameXXKBattleBoardWidget::EndCardPlayerPhase()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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
	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	if (!FGameXXKCardBattleAdapter::EndPlayerCardPhase(MutableState, DamageResults, &Error))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	LastCardInteractionError.Reset();
	const EBattlePresentationContinuation Continuation =
		MutableState.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy
			? EBattlePresentationContinuation::BeginEnemyIntentAfterPlayerPhase
			: EBattlePresentationContinuation::FinalizeCardMutation;
	return QueueMutationPresentation(Before, DamageResults, Continuation);
}

bool UGameXXKBattleBoardWidget::SubmitPendingInsightChoice(FName PickedInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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

	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitInsightChoice(
		MutableState,
		PickedInstanceId,
		RemainingTopOrder,
		&Error,
		&ResumedResults))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		FlattenResumedCardDamageResults(ResumedResults),
		EBattlePresentationContinuation::FinalizeCardMutation);
}

bool UGameXXKBattleBoardWidget::SubmitPendingHeroTaskSearchChoice(FName PickedInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("当前没有可检索的任务牌。");
		return false;
	}

	FGameXXKRuntimeState& MutableState = Subsystem->GetMutableRuntimeState();
	const FGameXXKPendingCardChoice& PendingChoice = MutableState.CardRun.ActiveBattle.Deck.PendingChoice;
	if (PendingChoice.Kind != EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand)
	{
		LastCardInteractionError = TEXT("当前没有可检索的任务牌。");
		RefreshProgrammaticLayout();
		return false;
	}
	const bool bIsCandidate = PendingChoice.Candidates.ContainsByPredicate([PickedInstanceId](const FGameXXKCardInstance& Candidate)
	{
		return Candidate.InstanceId == PickedInstanceId;
	});
	if (PickedInstanceId.IsNone() || !bIsCandidate)
	{
		LastCardInteractionError = TEXT("所选卡牌不在当前任务检索列表中。");
		RefreshProgrammaticLayout();
		return false;
	}

	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(
		MutableState,
		PickedInstanceId,
		ResumedResults,
		&Error))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		FlattenResumedCardDamageResults(ResumedResults),
		EBattlePresentationContinuation::FinalizeCardMutation);
}

bool UGameXXKBattleBoardWidget::SubmitPendingForcedDiscard(FName DiscardedInstanceId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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

	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::SubmitForcedDiscard(
		MutableState,
		{DiscardedInstanceId},
		&Error,
		&ResumedResults))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		FlattenResumedCardDamageResults(ResumedResults),
		EBattlePresentationContinuation::FinalizeCardMutation);
}

bool UGameXXKBattleBoardWidget::CancelPendingInsightChoice()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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

	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::CancelInsight(MutableState, &Error, &ResumedResults))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}

	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		FlattenResumedCardDamageResults(ResumedResults),
		EBattlePresentationContinuation::FinalizeCardMutation);
}

void UGameXXKBattleBoardWidget::RegisterBattleUnitScreenPosition(FName UnitId, FVector2D ScreenPosition)
{
	if (!UnitId.IsNone())
	{
		// Retained only for the dormant non-card compatibility bridge. CardRun
		// targeting is authored in the common stage and never consumes projection.
		RegisteredBattleUnitScreenPositions.Add(UnitId, ScreenPosition);
	}
}

void UGameXXKBattleBoardWidget::ClearBattleUnitScreenPositions()
{
	// Legacy non-card projections are transient compatibility data only. CardRun
	// targeting and fixed resource HUDs never consume this registry.
	RegisteredBattleUnitScreenPositions.Reset();
}

void UGameXXKBattleBoardWidget::RegisterBattleUnitHudScreenPosition(const FName UnitId, const FVector2D ScreenPosition)
{
	// Deliberately retained as a Blueprint-compatible no-op during the HUD migration.
	// The CardRun arrow and resource HUD plates both use fixed common-stage geometry.
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
			const bool bRetainedByPresentation = IsUnitRetainedByPresentation(Unit.UnitId);
			if (Unit.UnitId.IsNone() || (!Unit.bLiving && !bRetainedByPresentation))
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
			if (DisplayedHealthOverrides.Contains(Unit.UnitId))
			{
				// Only presentation participants (units with an animated HP override)
				// freeze to the pre-mutation snapshot and follow the HP override.
				// Non-participants (healed, armored, mana/status-changed units) must
				// always show live runtime state, otherwise healing and other side
				// effects are hidden behind the snapshot until the queue drains and
				// look permanently stuck.
				if (const FGameXXKBattleUnitHudView* const Snapshot = DisplayedUnitHudOverrides.Find(Unit.UnitId))
				{
					View = *Snapshot;
				}
				if (const int32* const DisplayedHealth = DisplayedHealthOverrides.Find(Unit.UnitId))
				{
					View.CurrentHP = *DisplayedHealth;
					// A lethal target remains a visible presentation participant until its
					// queued Death clip completes, even though authoritative state is already terminal.
					View.bLiving = true;
				}
			}
			UE_LOG(LogTemp, Verbose, TEXT("[HPSnap] unit=%s view=%d runtime=%d snap=%d snapHP=%d healthOverride=%d"),
				*Unit.UnitId.ToString(),
				View.CurrentHP,
				Unit.HP,
				DisplayedUnitHudOverrides.Contains(Unit.UnitId),
				DisplayedUnitHudOverrides.FindRef(Unit.UnitId).CurrentHP,
				DisplayedHealthOverrides.Contains(Unit.UnitId));
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
			UE_LOG(LogTemp, Verbose, TEXT("[HudSet] unit=%s hud=%s viewHP=%d layerChildren=%d"),
				*Unit.UnitId.ToString(),
				*Hud->GetPathName(),
				View.CurrentHP,
				BattleProjectedUnitHudLayer->GetChildrenCount());
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

void UGameXXKBattleBoardWidget::RefreshUnitVisuals()
{
	if (ActiveBattleVisualSessionToken == 0 || !BattleDesignStage || !WidgetTree || !AtlasCache)
	{
		return;
	}

	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bHasCardBattle = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle;
	TSet<FName> ActiveUnitIds;
	if (bHasCardBattle)
	{
		for (const FGameXXKCardCombatUnit& Unit : State->CardRun.ActiveBattle.Units)
		{
			if (Unit.UnitId.IsNone() || (!Unit.bLiving && !IsUnitRetainedByPresentation(Unit.UnitId)))
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

			ActiveUnitIds.Add(Unit.UnitId);
			const bool bEnemy = View.Side == EGameXXKCardTargetSide::Enemy;
			const FVector2D FormationAnchor(
				FixedLayout.Anchors.Minimum.X,
				FixedLayout.Anchors.Minimum.Y + FormationVisualVerticalOffsetNormalized);
			const FGameXXKBattleAnimationClipDescriptor IdleClip =
				ResolveUnitAnimationClip(
					Unit.UnitId,
					bEnemy,
					EGameXXKBattleAnimationAction::Idle);

			UGameXXKBattleUnitVisualWidget* Visual = UnitVisuals.FindRef(Unit.UnitId);
			bool bCreatedVisual = false;
			const bool bValidVisualOwnership = Visual
				&& Visual->GetParent() == BattleDesignStage
				&& Cast<UCanvasPanelSlot>(Visual->Slot);
			if (!bValidVisualOwnership)
			{
				if (Visual)
				{
					Visual->SetAtlas(nullptr);
					Visual->RemoveFromParent();
				}
				UnitVisuals.Remove(Unit.UnitId);
				Visual = WidgetTree->ConstructWidget<UGameXXKBattleUnitVisualWidget>(
					UGameXXKBattleUnitVisualWidget::StaticClass(),
					*FString::Printf(TEXT("BattleUnitVisual_%s"), *Unit.UnitId.ToString()));
				if (Visual)
				{
					if (UCanvasPanelSlot* const VisualSlot = BattleDesignStage->AddChildToCanvas(Visual))
					{
						VisualSlot->SetZOrder(BattleFormationZOrder);
						UnitVisuals.Add(Unit.UnitId, Visual);
						bCreatedVisual = true;
					}
					else
					{
						Visual->RemoveFromParent();
						Visual = nullptr;
					}
				}
			}
			if (!Visual)
			{
				continue;
			}

			if (bCreatedVisual)
			{
				Visual->ConfigureUnit(Unit.UnitId, bEnemy, FormationAnchor, IdleClip);
				Visual->ShowFormationIdle();
			}

			UGameXXKBattleUnitTargetProxyButton* Proxy = UnitTargetProxies.FindRef(Unit.UnitId);
			const bool bValidProxyOwnership = Proxy
				&& Proxy->GetParent() == BattleDesignStage
				&& Cast<UCanvasPanelSlot>(Proxy->Slot);
			if (!bValidProxyOwnership)
			{
				if (Proxy)
				{
					Proxy->RemoveFromParent();
				}
				UnitTargetProxies.Remove(Unit.UnitId);
				UnitTargetPlaceholders.Remove(Unit.UnitId);
				Proxy = WidgetTree->ConstructWidget<UGameXXKBattleUnitTargetProxyButton>(
					UGameXXKBattleUnitTargetProxyButton::StaticClass(),
					*FString::Printf(TEXT("BattleUnitTargetProxy_%s"), *Unit.UnitId.ToString()));
				if (Proxy)
				{
					FSlateBrush InvisibleBrush;
					InvisibleBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
					FButtonStyle InvisibleStyle;
					InvisibleStyle
						.SetNormal(InvisibleBrush)
						.SetHovered(InvisibleBrush)
						.SetPressed(InvisibleBrush)
						.SetDisabled(InvisibleBrush);
					Proxy->SetStyle(InvisibleStyle);
					Proxy->SetIsEnabled(!IsBattlePresentationPending());
					Proxy->SetVisibility(IsBattlePresentationPending()
						? ESlateVisibility::Hidden
						: ESlateVisibility::Visible);
					Proxy->Configure(this, Unit.UnitId);

					UTextBlock* const Placeholder = WidgetTree->ConstructWidget<UTextBlock>(
						UTextBlock::StaticClass(),
						*FString::Printf(TEXT("BattleUnitTargetPlaceholder_%s"), *Unit.UnitId.ToString()));
					if (Placeholder)
					{
						Placeholder->SetText(FText::Format(
							NSLOCTEXT("GameXXK", "BattleUnitIdleLoading", "{0}\n载入中"),
							View.DisplayName));
						Placeholder->SetJustification(ETextJustify::Center);
						Placeholder->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.79f, 0.48f, 0.82f)));
						FSlateFontInfo Font = Placeholder->GetFont();
						Font.Size = 18;
						Placeholder->SetFont(Font);
						Placeholder->SetVisibility(ESlateVisibility::HitTestInvisible);
						Proxy->AddChild(Placeholder);
						UnitTargetPlaceholders.Add(Unit.UnitId, Placeholder);
					}
					if (UCanvasPanelSlot* const ProxySlot = BattleDesignStage->AddChildToCanvas(Proxy))
					{
						UnitTargetProxies.Add(Unit.UnitId, Proxy);
					}
					else
					{
						Proxy->RemoveFromParent();
						UnitTargetPlaceholders.Remove(Unit.UnitId);
						Proxy = nullptr;
					}
				}
			}
			if (Proxy)
			{
				if (UCanvasPanelSlot* const ProxySlot = Cast<UCanvasPanelSlot>(Proxy->Slot))
				{
					ProxySlot->SetAnchors(FAnchors(FormationAnchor.X, FormationAnchor.Y));
					ProxySlot->SetAlignment(FVector2D(0.5f, 0.5f));
					ProxySlot->SetOffsets(FMargin(0.0f, 0.0f, FormationTargetProxySize.X, FormationTargetProxySize.Y));
					ProxySlot->SetZOrder(BattleTargetProxyBaseZOrder + FMath::Clamp(View.SlotNumber, 1, 5));
				}
			}

			if (RequestedUnitAtlasPaths.Contains(Unit.UnitId))
			{
				continue;
			}
			RequestedUnitAtlasPaths.Add(Unit.UnitId, IdleClip.TexturePath);
			SetUnitTargetPlaceholderVisible(Unit.UnitId, true);
			if (!IdleClip.IsValid())
			{
				continue;
			}

			// Pin before Acquire: the loader is permitted to invoke the completion
			// synchronously and the callback must see a complete Board registry.
			AtlasCache->Pin(IdleClip.TexturePath);
			PinnedUnitAtlasPaths.Add(Unit.UnitId, IdleClip.TexturePath);
			const TWeakObjectPtr<UGameXXKBattleBoardWidget> WeakBoard(this);
			const uint64 RequestToken = ActiveBattleVisualSessionToken;
			const FName RequestUnitId = Unit.UnitId;
			const FSoftObjectPath RequestPath = IdleClip.TexturePath;
			AtlasCache->Acquire(
				RequestPath,
				RequestToken,
				[WeakBoard, RequestToken, RequestUnitId, RequestPath](
					UTexture2D* const Texture,
					const EGameXXKAtlasLoadResult Result)
				{
					UGameXXKBattleBoardWidget* const Board = WeakBoard.Get();
					if (!Board
						|| Board->ActiveBattleVisualSessionToken != RequestToken
						|| Board->RequestedUnitAtlasPaths.FindRef(RequestUnitId) != RequestPath)
					{
						return;
					}

					UGameXXKBattleUnitVisualWidget* const RequestVisual = Board->UnitVisuals.FindRef(RequestUnitId);
					bool bApplyToCurrentVisual = true;
					if (const FBattlePresentationQueueEntry* const ActiveEntry = Board->GetActivePresentationEntry())
					{
						if (ActiveEntry->Kind == EBattlePresentationKind::Death
							&& ActiveEntry->Event.TargetUnitId == RequestUnitId)
						{
							bApplyToCurrentVisual = ActiveEntry->PresentedTargetClip.TexturePath == RequestPath;
						}
						else if (ActiveEntry->Kind == EBattlePresentationKind::AttackHit
							&& ActiveEntry->Event.AttackerUnitId == RequestUnitId)
						{
							bApplyToCurrentVisual = ActiveEntry->PresentedAttackerClip.TexturePath == RequestPath;
						}
						else if (ActiveEntry->Kind == EBattlePresentationKind::AttackHit
							&& ActiveEntry->Event.TargetUnitId == RequestUnitId)
						{
							bApplyToCurrentVisual = ActiveEntry->PresentedTargetClip.TexturePath == RequestPath;
						}
					}
					if (Result == EGameXXKAtlasLoadResult::Loaded && Texture && RequestVisual)
					{
						Board->UnitIdleAtlasTextures.Add(RequestUnitId, Texture);
						if (bApplyToCurrentVisual)
						{
							RequestVisual->SetAtlas(Texture);
						}
						Board->SetUnitTargetPlaceholderVisible(RequestUnitId, false);
						return;
					}

					if (RequestVisual && bApplyToCurrentVisual)
					{
						RequestVisual->SetAtlas(nullptr);
					}
					Board->UnitIdleAtlasTextures.Remove(RequestUnitId);
					Board->SetUnitTargetPlaceholderVisible(RequestUnitId, true);
					if (Board->PinnedUnitAtlasPaths.FindRef(RequestUnitId) == RequestPath)
					{
						Board->ReleasePinnedAtlasForUnit(RequestUnitId);
					}
				});
		}
	}

	TArray<FName> ExistingUnitIds;
	UnitVisuals.GetKeys(ExistingUnitIds);
	for (const FName UnitId : ExistingUnitIds)
	{
		if (!ActiveUnitIds.Contains(UnitId))
		{
			RemoveUnitVisual(UnitId);
		}
	}
	RefreshUnitTargetingPresentation();
}

void UGameXXKBattleBoardWidget::RefreshUnitTargetingPresentation()
{
	const bool bTargeting = IsCardTargetingActive();
	for (const TPair<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>>& Pair : UnitVisuals)
	{
		if (Pair.Value)
		{
			Pair.Value->SetCardTargetingAvailability(
				bTargeting,
				LegalCardTargetUnitIds.Contains(Pair.Key));
		}
	}
}

void UGameXXKBattleBoardWidget::ReleasePinnedAtlasForUnit(const FName UnitId)
{
	FSoftObjectPath Path;
	if (!PinnedUnitAtlasPaths.RemoveAndCopyValue(UnitId, Path))
	{
		return;
	}
	if (AtlasCache)
	{
		AtlasCache->Unpin(Path);
	}
}

void UGameXXKBattleBoardWidget::RemoveUnitVisual(const FName UnitId)
{
	ClearCardOutcomePreview();
	ReleasePinnedAtlasForUnit(UnitId);
	RequestedUnitAtlasPaths.Remove(UnitId);
	UnitIdleAtlasTextures.Remove(UnitId);
	if (UGameXXKBattleUnitVisualWidget* const Visual = UnitVisuals.FindRef(UnitId))
	{
		Visual->SetAtlas(nullptr);
		Visual->RemoveFromParent();
	}
	if (UGameXXKBattleUnitTargetProxyButton* const Proxy = UnitTargetProxies.FindRef(UnitId))
	{
		Proxy->RemoveFromParent();
	}
	UnitVisuals.Remove(UnitId);
	UnitTargetProxies.Remove(UnitId);
	UnitTargetPlaceholders.Remove(UnitId);
}

void UGameXXKBattleBoardWidget::SetUnitTargetPlaceholderVisible(const FName UnitId, const bool bVisible)
{
	if (UTextBlock* const Placeholder = UnitTargetPlaceholders.FindRef(UnitId))
	{
		Placeholder->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
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

bool UGameXXKBattleBoardWidget::ChoosePendingBattleRewardOption(int32 OptionIndex, FName ReplacementEntryId)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !HasPendingRouteReward())
	{
		LastCardInteractionError = TEXT("当前没有可选取的战后奖励。");
		return false;
	}
	const TArray<FGameXXKBattleRewardOption>& Options = Subsystem->GetRuntimeState().CardRun.PendingReward.Options;
	if (!Options.IsValidIndex(OptionIndex))
	{
		LastCardInteractionError = TEXT("所选奖励不在当前战后三选一内。");
		RefreshProgrammaticLayout();
		return false;
	}
	const FGameXXKBattleRewardOption Option = Options[OptionIndex];

	if (Option.Kind == EGameXXKBattleRewardKind::BossCard)
	{
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString Error;
		if (!FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			Subsystem->GetRuntimeState(),
			Option.CardId,
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
			if (RouteRewardCardIdAwaitingReplacement != Option.CardId)
			{
				RouteRewardCardIdAwaitingReplacement = Option.CardId;
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
	}

	FString Error;
	if (!Subsystem->ResolvePendingBattleRewardChoiceAndFinish(OptionIndex, ReplacementEntryId, &Error))
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
		&& (Subsystem->GetRuntimeState().CardRun.PendingReward.Options.Num() > 0
			|| Subsystem->GetRuntimeState().CardRun.PendingReward.CardIds.Num() > 0);
}

TArray<FName> UGameXXKBattleBoardWidget::GetPendingRouteRewardCardIds() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	TArray<FName> Result;
	if (!Subsystem)
	{
		return Result;
	}
	// One slot id per tiered option; non-card options report None so the
	// three-slot shape and replacement bookkeeping stay stable.
	const TArray<FGameXXKBattleRewardOption>& Options = Subsystem->GetRuntimeState().CardRun.PendingReward.Options;
	Result.Reserve(Options.Num());
	for (const FGameXXKBattleRewardOption& Option : Options)
	{
		Result.Add(Option.CardId);
	}
	return Result;
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

void UGameXXKBattleBoardWidget::RefreshCinematicViewportCoverLayout(const FVector2D ViewportSize)
{
	if (!BattleCinematicViewportCover || BattleCinematicViewportCoverStrips.Num() != 4
		|| ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		return;
	}

	const FGameXXKBattleHudSafeStageLayout SafeStage = ResolveBattleHudSafeStageLayoutForTest(ViewportSize);
	const float SafeRight = SafeStage.Offset.X + SafeStage.Size.X;
	const float SafeBottom = SafeStage.Offset.Y + SafeStage.Size.Y;
	const FVector2D Positions[] =
	{
		FVector2D::ZeroVector,
		FVector2D(0.0f, SafeBottom),
		FVector2D(0.0f, SafeStage.Offset.Y),
		FVector2D(SafeRight, SafeStage.Offset.Y)
	};
	const FVector2D Sizes[] =
	{
		FVector2D(ViewportSize.X, FMath::Max(0.0f, SafeStage.Offset.Y)),
		FVector2D(ViewportSize.X, FMath::Max(0.0f, ViewportSize.Y - SafeBottom)),
		FVector2D(FMath::Max(0.0f, SafeStage.Offset.X), SafeStage.Size.Y),
		FVector2D(FMath::Max(0.0f, ViewportSize.X - SafeRight), SafeStage.Size.Y)
	};

	for (int32 StripIndex = 0; StripIndex < BattleCinematicViewportCoverStrips.Num(); ++StripIndex)
	{
		UBorder* const Strip = BattleCinematicViewportCoverStrips[StripIndex];
		UCanvasPanelSlot* const StripSlot = Strip ? Cast<UCanvasPanelSlot>(Strip->Slot) : nullptr;
		if (!Strip || !StripSlot)
		{
			continue;
		}
		StripSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		StripSlot->SetAlignment(FVector2D::ZeroVector);
		StripSlot->SetPosition(Positions[StripIndex]);
		StripSlot->SetSize(Sizes[StripIndex]);
		Strip->SetVisibility(Sizes[StripIndex].X > KINDA_SMALL_NUMBER && Sizes[StripIndex].Y > KINDA_SMALL_NUMBER
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Hidden);
	}
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

FVector2D UGameXXKBattleBoardWidget::ResolveCardTargetingSourcePositionForTest(const FName OwnerUnitId) const
{
	return ResolveCardTargetingSourcePosition(OwnerUnitId);
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

FLinearColor UGameXXKBattleBoardWidget::ResolveCardFaceLabelColor()
{
	// Ink card text: every in-battle card face name band uses black text on the parchment frame.
	return FLinearColor(0.10f, 0.07f, 0.04f, 1.0f);
}

FLinearColor UGameXXKBattleBoardWidget::GetCardFaceLabelColorForTest() const
{
	return ResolveCardFaceLabelColor();
}

bool UGameXXKBattleBoardWidget::IsRewardPortraitVisibleForTest(const int32 SlotIndex) const
{
	const UImage* Portrait = RewardCardPortraits.IsValidIndex(SlotIndex)
		? RewardCardPortraits[SlotIndex].Get()
		: nullptr;
	return Portrait && Portrait->GetVisibility() != ESlateVisibility::Collapsed;
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

FString UGameXXKBattleBoardWidget::GetEnemyIntentPortraitResourcePathForTest(const int32 VisibleSlotIndex) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 PersistentIntentIndex = GetEnemyIntentPersistentIndexForVisibleSlot(VisibleSlotIndex);
	if (!Subsystem
		|| !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle
		|| !Subsystem->GetRuntimeState().CardRun.EnemyIntents.IsValidIndex(PersistentIntentIndex))
	{
		return FString();
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FGameXXKCardEnemyIntent& Intent = State.CardRun.EnemyIntents[PersistentIntentIndex];
	const FGameXXKBattleRuntimeUnit* SourceEnemy = State.ActiveBattleEnemies.FindByPredicate([&Intent](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == Intent.SourceUnitId;
	});
	return SourceEnemy ? ResolveEnemyIntentPortraitResourcePath(SourceEnemy->EnemyDefinitionId) : FString();
}

FString UGameXXKBattleBoardWidget::ResolveEnemyIntentPortraitResourcePath(const FName EnemyDefinitionId) const
{
	return ResolveEnemyPortraitPathByDefinitionId(EnemyDefinitionId);
}

UTexture2D* UGameXXKBattleBoardWidget::ResolveEnemyIntentPortraitTexture(const FName EnemyDefinitionId) const
{
	const FString ResourcePath = ResolveEnemyIntentPortraitResourcePath(EnemyDefinitionId);
	return ResourcePath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *ResourcePath);
}

FString UGameXXKBattleBoardWidget::GetCardTooltipTextForTest() const
{
	const FString Title = HandCardDetailTitle ? HandCardDetailTitle->GetText().ToString() : FString();
	const FString Body = HandCardDetailBody ? HandCardDetailBody->GetText().ToString() : FString();
	return Title.IsEmpty() ? Body : Title + TEXT("\n") + Body;
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

UButton* UGameXXKBattleBoardWidget::GetHandCardButtonForTest(const int32 SlotIndex) const
{
	return HandCardButtons.IsValidIndex(SlotIndex) ? HandCardButtons[SlotIndex] : nullptr;
}

bool UGameXXKBattleBoardWidget::IsCardOutcomePreviewVisibleForTest() const
{
	const auto IsVisible = [](const UWidget* Widget)
	{
		return Widget
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed
			&& Widget->GetVisibility() != ESlateVisibility::Hidden;
	};
	return IsVisible(SingleOutcomeWidget) || IsVisible(GroupOutcomeWidget);
}

FString UGameXXKBattleBoardWidget::GetCardOutcomePreviewClassForTest() const
{
	switch (CachedOutcomePreview.Classification)
	{
	case EGameXXKCardOutcomePreviewClass::ManualUnit: return TEXT("ManualUnit");
	case EGameXXKCardOutcomePreviewClass::PureEnemyGroup: return TEXT("PureEnemyGroup");
	default: return TEXT("None");
	}
}

FName UGameXXKBattleBoardWidget::GetCardOutcomePreviewCardInstanceIdForTest() const
{
	return CachedOutcomeCardInstanceId;
}

FName UGameXXKBattleBoardWidget::GetCardOutcomePreviewTargetUnitIdForTest() const
{
	return CachedOutcomeTargetUnitId;
}

TArray<FString> UGameXXKBattleBoardWidget::GetCardOutcomePreviewLinesForTest() const
{
	const UGameXXKCardOutcomePreviewWidget* VisibleWidget = nullptr;
	if (SingleOutcomeWidget && SingleOutcomeWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		VisibleWidget = SingleOutcomeWidget;
	}
	else if (GroupOutcomeWidget && GroupOutcomeWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		VisibleWidget = GroupOutcomeWidget;
	}

	TArray<FString> Result;
	if (!VisibleWidget)
	{
		return Result;
	}
	for (int32 LineIndex = 0; LineIndex < VisibleWidget->GetVisibleLineCountForTest(); ++LineIndex)
	{
		Result.Add(VisibleWidget->GetPlainLineForTest(LineIndex));
	}
	return Result;
}

int32 UGameXXKBattleBoardWidget::GetCardOutcomePreviewBuildCountForTest() const
{
	return OutcomePreviewBuildCountForTest;
}

FVector2D UGameXXKBattleBoardWidget::GetSingleOutcomePreviewAnchorForTest() const
{
	if (const UCanvasPanelSlot* const OutcomeSlot = SingleOutcomeWidget ? Cast<UCanvasPanelSlot>(SingleOutcomeWidget->Slot) : nullptr)
	{
		return OutcomeSlot->GetAnchors().Minimum;
	}
	return FVector2D::ZeroVector;
}

FVector2D UGameXXKBattleBoardWidget::GetGroupOutcomePreviewAnchorForTest() const
{
	if (const UCanvasPanelSlot* const OutcomeSlot = GroupOutcomeWidget ? Cast<UCanvasPanelSlot>(GroupOutcomeWidget->Slot) : nullptr)
	{
		return OutcomeSlot->GetAnchors().Minimum;
	}
	return FVector2D::ZeroVector;
}

FMargin UGameXXKBattleBoardWidget::GetSingleOutcomePreviewOffsetsForTest() const
{
	const UCanvasPanelSlot* const OutcomeSlot = SingleOutcomeWidget ? Cast<UCanvasPanelSlot>(SingleOutcomeWidget->Slot) : nullptr;
	return OutcomeSlot ? OutcomeSlot->GetOffsets() : FMargin();
}

FVector2D UGameXXKBattleBoardWidget::GetSingleOutcomePreviewAlignmentForTest() const
{
	const UCanvasPanelSlot* const OutcomeSlot = SingleOutcomeWidget ? Cast<UCanvasPanelSlot>(SingleOutcomeWidget->Slot) : nullptr;
	return OutcomeSlot ? OutcomeSlot->GetAlignment() : FVector2D::ZeroVector;
}

FString UGameXXKBattleBoardWidget::GetCardOutcomePreviewBackgroundResourceForTest() const
{
	return SingleOutcomeWidget ? SingleOutcomeWidget->GetBackgroundResourcePathForTest() : FString();
}

#if WITH_DEV_AUTOMATION_TESTS
UCanvasPanel* UGameXXKBattleBoardWidget::GetBattleOutcomePreviewLayerForTest() const
{
	return BattleOutcomePreviewLayer;
}

int32 UGameXXKBattleBoardWidget::GetBattleOutcomePreviewLayerZForTest() const
{
	const UCanvasPanelSlot* const OutcomeSlot = BattleOutcomePreviewLayer
		? Cast<UCanvasPanelSlot>(BattleOutcomePreviewLayer->Slot)
		: nullptr;
	return OutcomeSlot ? OutcomeSlot->GetZOrder() : INDEX_NONE;
}

FMargin UGameXXKBattleBoardWidget::GetGroupOutcomePreviewOffsetsForTest() const
{
	const UCanvasPanelSlot* const OutcomeSlot = GroupOutcomeWidget ? Cast<UCanvasPanelSlot>(GroupOutcomeWidget->Slot) : nullptr;
	return OutcomeSlot ? OutcomeSlot->GetOffsets() : FMargin();
}

FMargin UGameXXKBattleBoardWidget::GetRewardCardBoxOffsetsForTest() const
{
	return RewardCardBoxSlotOffsets;
}

FMargin UGameXXKBattleBoardWidget::GetHandCardDetailPanelOffsetsForTest() const
{
	const UCanvasPanelSlot* const DetailSlot = HandCardDetailPanel
		? Cast<UCanvasPanelSlot>(HandCardDetailPanel->Slot)
		: nullptr;
	return DetailSlot ? DetailSlot->GetOffsets() : FMargin();
}

FVector2D UGameXXKBattleBoardWidget::GetGroupOutcomePreviewAlignmentForTest() const
{
	const UCanvasPanelSlot* const OutcomeSlot = GroupOutcomeWidget ? Cast<UCanvasPanelSlot>(GroupOutcomeWidget->Slot) : nullptr;
	return OutcomeSlot ? OutcomeSlot->GetAlignment() : FVector2D::ZeroVector;
}

void UGameXXKBattleBoardWidget::RemoveUnitVisualForTest(const FName UnitId)
{
	RemoveUnitVisual(UnitId);
}

bool UGameXXKBattleBoardWidget::ResolveCardBattleTerminalStateForTest()
{
	return ResolveCardBattleTerminalState();
}
#endif

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
	if (!WidgetTree || ViewportRootCanvas || WidgetTree->RootWidget)
	{
		return;
	}

	ViewportRootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("GameXXKBattleViewportRoot"));
	WidgetTree->RootWidget = ViewportRootCanvas;
	UScaleBox* const BattleHudSafeStage = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("BattleHudSafeStage"));
	USizeBox* const BattleHudSafeStageSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("BattleHudSafeStageSize"));
	BattleDesignStage = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleDesignStage"));
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("GameXXKBattleControlsLayer"));
	BattleProjectedUnitHudLayer = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleProjectedUnitHudLayer"));
	BattleOutcomePreviewLayer = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleOutcomePreviewLayer"));
	SingleOutcomeWidget = WidgetTree->ConstructWidget<UGameXXKCardOutcomePreviewWidget>(
		UGameXXKCardOutcomePreviewWidget::StaticClass(),
		TEXT("BattleSingleOutcomePreview"));
	GroupOutcomeWidget = WidgetTree->ConstructWidget<UGameXXKCardOutcomePreviewWidget>(
		UGameXXKCardOutcomePreviewWidget::StaticClass(),
		TEXT("BattleGroupOutcomePreview"));
	if (BattleHudSafeStage
		&& BattleHudSafeStageSize
		&& BattleDesignStage
		&& RootCanvas
		&& BattleProjectedUnitHudLayer
		&& BattleOutcomePreviewLayer
		&& SingleOutcomeWidget
		&& GroupOutcomeWidget)
	{
		BattleHudSafeStage->SetStretch(EStretch::ScaleToFit);
		BattleHudSafeStage->SetStretchDirection(EStretchDirection::Both);
		BattleHudSafeStageSize->SetWidthOverride(BattleHudSafeStageDesignSize.X);
		BattleHudSafeStageSize->SetHeightOverride(BattleHudSafeStageDesignSize.Y);
		BattleDesignStage->SetClipping(EWidgetClipping::ClipToBounds);
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BattleProjectedUnitHudLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BattleOutcomePreviewLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
		BattleHudSafeStageSize->SetContent(BattleDesignStage);
		BattleHudSafeStage->SetContent(BattleHudSafeStageSize);
		if (UScaleBoxSlot* const SafeStageContentSlot = Cast<UScaleBoxSlot>(BattleHudSafeStageSize->Slot))
		{
			SafeStageContentSlot->SetHorizontalAlignment(HAlign_Center);
			SafeStageContentSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UCanvasPanelSlot* const SafeStageSlot = ViewportRootCanvas->AddChildToCanvas(BattleHudSafeStage))
		{
			SafeStageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			SafeStageSlot->SetOffsets(FMargin(0.0f));
			SafeStageSlot->SetAlignment(FVector2D::ZeroVector);
			SafeStageSlot->SetZOrder(BattleSafeStageRootZOrder);
		}

		BattleBackdropScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(),
			TEXT("BattleBackdropScaleToFill"));
		BattleBackdropImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			TEXT("BattleBackdropImage"));
		if (BattleBackdropScaleBox && BattleBackdropImage)
		{
			BattleBackdropScaleBox->SetStretch(EStretch::ScaleToFill);
			BattleBackdropScaleBox->SetStretchDirection(EStretchDirection::Both);
			BattleBackdropScaleBox->SetClipping(EWidgetClipping::ClipToBounds);
			const UGameXXKMVPSubsystem* const BattleSubsystem = ResolveMVPSubsystem();
			ApplyBattleBackdropForTerrain(
				BattleSubsystem
					? BattleSubsystem->GetRuntimeState().CardRun.ActiveBattle.Terrain
					: EGameXXKCardTerrain::Invalid);
			if (BattleBackdropTexture)
			{
				BattleBackdropImage->SetBrushFromTexture(BattleBackdropTexture, true);
			}
			BattleBackdropImage->SetColorAndOpacity(FLinearColor::White);
			BattleBackdropImage->SetOpacity(1.0f);
			BattleBackdropImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			BattleBackdropScaleBox->SetContent(BattleBackdropImage);
			if (UScaleBoxSlot* const BackdropContentSlot = Cast<UScaleBoxSlot>(BattleBackdropImage->Slot))
			{
				BackdropContentSlot->SetHorizontalAlignment(HAlign_Center);
				BackdropContentSlot->SetVerticalAlignment(VAlign_Center);
			}
			if (UCanvasPanelSlot* const BackdropSlot = ViewportRootCanvas->AddChildToCanvas(BattleBackdropScaleBox))
			{
				BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				BackdropSlot->SetOffsets(FMargin(0.0f));
				BackdropSlot->SetAlignment(FVector2D::ZeroVector);
				BackdropSlot->SetZOrder(BattleBackdropZOrder);
			}
		}

		if (UCanvasPanelSlot* const ControlsSlot = BattleDesignStage->AddChildToCanvas(RootCanvas))
		{
			ControlsSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			ControlsSlot->SetOffsets(FMargin(0.0f));
			ControlsSlot->SetAlignment(FVector2D::ZeroVector);
			ControlsSlot->SetZOrder(BattleControlsZOrder);
		}
		if (UCanvasPanelSlot* const UnitHudLayerSlot = RootCanvas->AddChildToCanvas(BattleProjectedUnitHudLayer))
		{
			UnitHudLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			UnitHudLayerSlot->SetOffsets(FMargin(0.0f));
			UnitHudLayerSlot->SetAlignment(FVector2D::ZeroVector);
			UnitHudLayerSlot->SetZOrder(0);
		}
		if (UCanvasPanelSlot* const OutcomeLayerSlot = RootCanvas->AddChildToCanvas(BattleOutcomePreviewLayer))
		{
			OutcomeLayerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			OutcomeLayerSlot->SetOffsets(FMargin(0.0f));
			OutcomeLayerSlot->SetAlignment(FVector2D::ZeroVector);
			OutcomeLayerSlot->SetZOrder(OutcomePreviewLayerZOrder);
		}
		if (UCanvasPanelSlot* const SingleSlot = BattleOutcomePreviewLayer->AddChildToCanvas(SingleOutcomeWidget))
		{
			SingleSlot->SetAnchors(FAnchors(0.0f));
			SingleSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			SingleSlot->SetOffsets(SingleOutcomePreviewOffsets);
			SingleSlot->SetZOrder(0);
		}
		if (UCanvasPanelSlot* const GroupSlot = BattleOutcomePreviewLayer->AddChildToCanvas(GroupOutcomeWidget))
		{
			GroupSlot->SetAnchors(FAnchors(GroupOutcomePreviewAnchor.X, GroupOutcomePreviewAnchor.Y));
			GroupSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			GroupSlot->SetOffsets(GroupOutcomePreviewOffsets);
			GroupSlot->SetZOrder(0);
		}
		ClearCardOutcomePreview();
	}

	BattleCinematicViewportCover = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleCinematicViewportCover"));
	if (BattleCinematicViewportCover && ViewportRootCanvas)
	{
		BattleCinematicViewportCover->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* const CoverSlot = ViewportRootCanvas->AddChildToCanvas(BattleCinematicViewportCover))
		{
			CoverSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CoverSlot->SetOffsets(FMargin(0.0f));
			CoverSlot->SetAlignment(FVector2D::ZeroVector);
			CoverSlot->SetZOrder(BattleCinematicViewportCoverZOrder);
		}

		static const TCHAR* const StripNames[] =
		{
			TEXT("BattleCinematicViewportCoverTop"),
			TEXT("BattleCinematicViewportCoverBottom"),
			TEXT("BattleCinematicViewportCoverLeft"),
			TEXT("BattleCinematicViewportCoverRight")
		};
		BattleCinematicViewportCoverStrips.Reset();
		for (const TCHAR* const StripName : StripNames)
		{
			UBorder* const Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), StripName);
			if (!Strip)
			{
				continue;
			}
			Strip->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
			Strip->SetVisibility(ESlateVisibility::Hidden);
			if (BattleCinematicViewportCover->AddChildToCanvas(Strip))
			{
				BattleCinematicViewportCoverStrips.Add(Strip);
			}
		}
		RefreshCinematicViewportCoverLayout(BattleHudSafeStageDesignSize);
	}

	BattleCinematicDimmer = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("BattleCinematicDimmer"));
	if (BattleCinematicDimmer && BattleDesignStage)
	{
		BattleCinematicDimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		BattleCinematicDimmer->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* const DimmerSlot = BattleDesignStage->AddChildToCanvas(BattleCinematicDimmer))
		{
			DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimmerSlot->SetOffsets(FMargin(0.0f));
			DimmerSlot->SetAlignment(FVector2D::ZeroVector);
			DimmerSlot->SetZOrder(BattleCinematicDimmerZOrder);
		}
	}

	BattleCinematicImpact = WidgetTree->ConstructWidget<UGameXXKBattleUnitVisualWidget>(
		UGameXXKBattleUnitVisualWidget::StaticClass(),
		TEXT("BattleCinematicImpact"));
	if (BattleCinematicImpact && BattleDesignStage)
	{
		if (UCanvasPanelSlot* const ImpactSlot = BattleDesignStage->AddChildToCanvas(BattleCinematicImpact))
		{
			ImpactSlot->SetAnchors(FAnchors(CinematicImpactAnchor.X, CinematicImpactAnchor.Y));
			ImpactSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ImpactSlot->SetPosition(FVector2D::ZeroVector);
			ImpactSlot->SetSize(CinematicImpactVisualSize);
			ImpactSlot->SetZOrder(BattleCinematicImpactZOrder);
		}
		const FGameXXKBattleAnimationClipDescriptor ImpactClip =
			FGameXXKBattleAnimationPresentation::ResolveGenericClip(EGameXXKBattleAnimationAction::Impact);
		BattleCinematicImpact->ConfigureUnit(TEXT("Battle.GenericImpact"), false, CinematicImpactAnchor, ImpactClip);
		if (UCanvasPanelSlot* const ImpactSlot = Cast<UCanvasPanelSlot>(BattleCinematicImpact->Slot))
		{
			ImpactSlot->SetAnchors(FAnchors(CinematicImpactAnchor.X, CinematicImpactAnchor.Y));
			ImpactSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ImpactSlot->SetPosition(FVector2D::ZeroVector);
			ImpactSlot->SetSize(CinematicImpactVisualSize);
			ImpactSlot->SetZOrder(BattleCinematicImpactZOrder);
		}
		BattleCinematicImpact->HideForCinematic();
	}

	BattleCinematicStatusIcon = WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>(
		UGameXXKBattleStatusIconWidget::StaticClass(),
		TEXT("BattleCinematicStatusIcon"));
	if (BattleCinematicStatusIcon && BattleDesignStage
		&& BattleCinematicStatusIcon->PrepareForScreenSpaceEmbedding())
	{
		BattleCinematicStatusIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		BattleCinematicStatusIcon->SetRenderScale(FVector2D(2.5f, 2.5f));
		BattleCinematicStatusIcon->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* const StatusIconSlot = BattleDesignStage->AddChildToCanvas(BattleCinematicStatusIcon))
		{
			StatusIconSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			StatusIconSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			StatusIconSlot->SetPosition(FVector2D(0.0f, -145.0f));
			StatusIconSlot->SetSize(FVector2D(44.0f, 44.0f));
			StatusIconSlot->SetZOrder(BattleCinematicStatusIconZOrder);
		}
	}

	BattleCinematicReadout = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BattleCinematicReadout"));
	if (BattleCinematicReadout && BattleDesignStage)
	{
		FSlateFontInfo ReadoutFont = BattleCinematicReadout->GetFont();
		ReadoutFont.Size = 64;
		BattleCinematicReadout->SetFont(ReadoutFont);
		BattleCinematicReadout->SetJustification(ETextJustify::Center);
		BattleCinematicReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.82f, 0.42f, 1.0f)));
		BattleCinematicReadout->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		BattleCinematicReadout->SetShadowOffset(FVector2D(3.0f, 3.0f));
		BattleCinematicReadout->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* const ReadoutSlot = BattleDesignStage->AddChildToCanvas(BattleCinematicReadout))
		{
			ReadoutSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ReadoutSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ReadoutSlot->SetPosition(FVector2D(0.0f, -260.0f));
			ReadoutSlot->SetSize(FVector2D(420.0f, 120.0f));
			ReadoutSlot->SetZOrder(BattleCinematicReadoutZOrder);
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
	EnemyIntentCardPortraits.Reserve(MaximumVisibleEnemyIntentCards);
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
		UImage* IntentCardPortrait = nullptr;
		BuildEnemyIntentCardFace(
			IntentCardButton,
			FString::Printf(TEXT("BattleEnemyIntentCard_%02d"), SlotIndex),
			IntentCardBody,
			IntentCardPortrait);
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
		EnemyIntentCardPortraits.Add(IntentCardPortrait);
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
		HandSlot->SetOffsets(FMargin(-PlayerHandRowSize.X * 0.5f, -305.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y));
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
		// UMG dynamic delegates cannot carry a slot index through AddDynamic,
		// so the per-slot UFUNCTIONs are bound by name.
		const FName ClickHandlerName(*FString::Printf(TEXT("HandleHandCardSlot%dClicked"), SlotIndex));
		const FName HoverHandlerName(*FString::Printf(TEXT("HandleHandCardSlot%dHovered"), SlotIndex));
		const FName UnhoverHandlerName(*FString::Printf(TEXT("HandleHandCardSlot%dUnhovered"), SlotIndex));
		FOnButtonClickedEvent::FDelegate ClickHandler;
		ClickHandler.BindUFunction(this, ClickHandlerName);
		CardButton->OnClicked.Add(ClickHandler);
		FOnButtonHoverEvent::FDelegate HoverHandler;
		HoverHandler.BindUFunction(this, HoverHandlerName);
		CardButton->OnHovered.Add(HoverHandler);
		FOnButtonHoverEvent::FDelegate UnhoverHandler;
		UnhoverHandler.BindUFunction(this, UnhoverHandlerName);
		CardButton->OnUnhovered.Add(UnhoverHandler);
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
	// Shared tooltip paper and nine-slice margin with the out-of-battle deck tooltips
	// (T_MasterV2_ItemSlot at the inventory tooltip's fixed 0.065 box margin).
	if (UTexture2D* TooltipPaper = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot")))
	{
		HandCardDetailPanel->SetBrush(BuildBoxTextureBrush(
			TooltipPaper,
			HandCardDetailPanelSize,
			FMargin(0.065f)));
	}
	HandCardDetailPanel->SetBrushColor(FLinearColor::White);
	HandCardDetailPanel->SetPadding(FMargin(16.0f, 12.0f));
	HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* TooltipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	HandCardDetailPanel->SetContent(TooltipBox);
	HandCardDetailTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleHandCardDetailTitle"));
	HandCardDetailTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.08f, 0.06f, 0.04f, 1.0f)));
	FSlateFontInfo TitleFont = HandCardDetailTitle->GetFont();
	TitleFont.Size = 18;
	HandCardDetailTitle->SetFont(TitleFont);
	TooltipBox->AddChildToVerticalBox(HandCardDetailTitle);
	HandCardDetailBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleHandCardDetailBody"));
	HandCardDetailBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.14f, 0.11f, 0.08f, 1.0f)));
	HandCardDetailBody->SetAutoWrapText(true);
	HandCardDetailBody->SetJustification(ETextJustify::Left);
	FSlateFontInfo DetailFont = HandCardDetailBody->GetFont();
	DetailFont.Size = 13;
	HandCardDetailBody->SetFont(DetailFont);
	if (UVerticalBoxSlot* TooltipBodySlot = TooltipBox->AddChildToVerticalBox(HandCardDetailBody))
	{
		TooltipBodySlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
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
		// Three vertical BuildCardFace reward cards (RewardCardSize each, 5px
		// slot padding per side) need a row sized for the full card faces.
		// The previous 370x136 strip was the legacy small-card container and
		// clipped the 206x285 faces down to horizontal slivers.
		const float RewardRowWidth = static_cast<float>(MaximumVisibleRewardCards) * (RewardCardSize.X + 10.0f);
		RewardCardBoxSlotOffsets = FMargin(
			-RewardRowWidth * 0.5f,
			-RewardCardSize.Y * 0.5f,
			RewardRowWidth,
			RewardCardSize.Y);
		RewardSlot->SetOffsets(RewardCardBoxSlotOffsets);
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
		// Below the full-height reward row, clear of the card faces.
		SkipRewardSlot->SetOffsets(FMargin(-95.0f, RewardCardSize.Y * 0.5f + 40.0f, 190.0f, 56.0f));
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
	const UGameXXKMVPSubsystem* const OutcomeSubsystem = ResolveMVPSubsystem();
	if (CachedOutcomeSourceState.IsSet()
		&& (!OutcomeSubsystem
			|| !FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
				&OutcomeSubsystem->GetRuntimeState(),
				&CachedOutcomeSourceState.GetValue(),
				PPF_None)))
	{
		ClearCardOutcomePreview();
	}
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
	const bool bCanUseMenu = !bFixtureReadOnly
		&& !IsBattlePresentationPending()
		&& InteractionMode == EGameXXKBattleInteractionMode::CommandMenuOpen;
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
		&& !IsBattlePresentationPending()
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
			&& !IsBattlePresentationPending()
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
	const FVector2D CanvasSize = RootCanvas
		? RootCanvas->GetCachedGeometry().GetLocalSize()
		: FVector2D::ZeroVector;
	RefreshPartyQiWidgetForCanvasSize(CanvasSize);
}

void UGameXXKBattleBoardWidget::RefreshPartyQiWidgetForCanvasSize(const FVector2D CanvasSize)
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
	LastPartyQiCanvasSize = CanvasSize;
	if (!bShouldShowPartyQi)
	{
		PartyQiWidget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// The card runtime remains the sole authority. While an immutable presentation batch is
	// pending, retain its captured pre-mutation value so responsive layout refreshes cannot
	// reveal post-state Qi before the batch's full-drain reconciliation.
	const int32 DisplayedSharedEnergy = DisplayedSharedEnergyOverride.IsSet()
		? DisplayedSharedEnergyOverride.GetValue()
		: State->CardRun.ActiveBattle.Deck.SharedEnergy;
	PartyQiWidget->SetSharedQi(DisplayedSharedEnergy);
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
		: FMargin(-PlayerHandRowSize.X * 0.5f, -305.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y);
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
		: FMargin(-PlayerHandRowSize.X * 0.5f, -305.0f, PlayerHandRowSize.X, PlayerHandRowSize.Y);
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

void UGameXXKBattleBoardWidget::RefreshPartyQiForCanvasSizeForTest(const FVector2D CanvasSize)
{
	RefreshPartyQiWidgetForCanvasSize(CanvasSize);
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
	FGameXXKCardTooltipContext Context;
	/** Non-card reward options write their tooltip text directly instead of a card definition. */
	TOptional<FText> DirectTooltipText;
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
			const bool bAddsToHand = HoveredPendingChoiceKind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
				|| HoveredPendingChoiceKind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
			Context.InteractionResult = bAddsToHand
				? TEXT("点击后加入手牌。")
				: TEXT("点击后弃置此牌。");
		}
		break;
	}
	case ECardTooltipSource::Reward:
	{
		if (!PendingRewardOptions.IsValidIndex(HoveredRewardCardSlot))
		{
			break;
		}
		const FGameXXKBattleRewardOption& RewardOption = PendingRewardOptions[HoveredRewardCardSlot];
		if (RewardOption.Kind == EGameXXKBattleRewardKind::Relic)
		{
			const FGameXXKRelicDefinition* RelicDefinition = FGameXXKRelicCatalog::FindDefinition(RewardOption.RelicId);
			if (RelicDefinition)
			{
				DirectTooltipText = FText::FromString(FString::Printf(
					TEXT("%s\n%s"),
					*RelicDefinition->DisplayName.ToString(),
					*RelicDefinition->Description.ToString()));
			}
		}
		else if (RewardOption.Kind == EGameXXKBattleRewardKind::EnergyCapBonus)
		{
			DirectTooltipText = FText::FromString(TEXT("[属性奖励]\n气力上限永久 +1"));
		}
		else if (RewardOption.Kind == EGameXXKBattleRewardKind::DrawBonus)
		{
			DirectTooltipText = FText::FromString(TEXT("[属性奖励]\n每回合抽牌数永久 +1"));
		}
		else if (!RewardOption.CardId.IsNone())
		{
			Definition = FGameXXKCardCatalog::FindCardDefinition(RewardOption.CardId);
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

	// Unified concise format shared with the out-of-battle deck tooltips:
	// 18pt ink title + 13pt concise description, no battle preview or context noise.
	FText TooltipTitle;
	FText TooltipBody;
	if (DirectTooltipText.IsSet())
	{
		FString TitlePart;
		FString BodyPart;
		const FString Raw = DirectTooltipText.GetValue().ToString();
		if (Raw.Split(TEXT("\n"), &TitlePart, &BodyPart))
		{
			TooltipTitle = FText::FromString(TitlePart);
			TooltipBody = FText::FromString(BodyPart);
		}
		else
		{
			TooltipBody = DirectTooltipText.GetValue();
		}
	}
	else if (Definition)
	{
		TooltipTitle = Definition->DisplayName;
		TooltipBody = FText::FromString(
			TooltipQuality == EGameXXKCardQuality::Invalid
				? GameXXKCardText::DescribeTooltip(*Definition, nullptr, FGameXXKCardTooltipContext())
				: GameXXKCardText::DescribeTooltip(*Definition, TooltipQuality, nullptr, FGameXXKCardTooltipContext()));
	}
	if (TooltipBody.IsEmpty() && TooltipTitle.IsEmpty())
	{
		// An empty tooltip must not touch layout: collapsing without rewriting
		// the slot keeps hover-invariant layout checks deterministic.
		HandCardDetailPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Follow the hovered card slot instead of the fixed default anchor.
	if (UCanvasPanelSlot* DetailSlot = Cast<UCanvasPanelSlot>(HandCardDetailPanel->Slot))
	{
		FVector2D PanelPosition(-HandCardDetailPanelSize.X * 0.5f, -588.0f);
		if (HoveredCardTooltipSource == ECardTooltipSource::Reward
			&& PendingRewardOptions.IsValidIndex(HoveredRewardCardSlot))
		{
			const float SlotCenterX = RewardCardBoxSlotOffsets.Left + 5.0f
				+ HoveredRewardCardSlot * (RewardCardSize.X + 10.0f)
				+ RewardCardSize.X * 0.5f;
			PanelPosition = FVector2D(
				SlotCenterX - HandCardDetailPanelSize.X * 0.5f,
				RewardCardBoxSlotOffsets.Top - HandCardDetailPanelSize.Y - 12.0f);
		}
		else if (HoveredCardTooltipSource == ECardTooltipSource::Hand
			&& HandCardInstanceIds.IsValidIndex(HoveredHandCardSlot))
		{
			const float SlotCenterX = -585.0f + 4.0f
				+ HoveredHandCardSlot * (PlayerHandCardSize.X + 8.0f)
				+ PlayerHandCardSize.X * 0.5f;
			PanelPosition = FVector2D(
				SlotCenterX - HandCardDetailPanelSize.X * 0.5f,
				-305.0f - HandCardDetailPanelSize.Y - 12.0f);
		}
		DetailSlot->SetOffsets(FMargin(
			PanelPosition.X,
			PanelPosition.Y,
			HandCardDetailPanelSize.X,
			HandCardDetailPanelSize.Y));
	}

	if (HandCardDetailTitle)
	{
		HandCardDetailTitle->SetText(TooltipTitle);
	}
	if (HandCardDetailBody)
	{
		HandCardDetailBody->SetText(TooltipBody);
	}
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
		UImage* CardPortrait = EnemyIntentCardPortraits.IsValidIndex(VisibleSlotIndex) ? EnemyIntentCardPortraits[VisibleSlotIndex] : nullptr;
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
			if (CardPortrait)
			{
				CardPortrait->SetVisibility(ESlateVisibility::Collapsed);
			}
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
		if (CardPortrait)
		{
			const FGameXXKBattleRuntimeUnit* SourceEnemy = State->ActiveBattleEnemies.FindByPredicate([&Intent](const FGameXXKBattleRuntimeUnit& Unit)
			{
				return Unit.Id == Intent.SourceUnitId;
			});
			UTexture2D* PortraitTexture = SourceEnemy
				? ResolveEnemyIntentPortraitTexture(SourceEnemy->EnemyDefinitionId)
				: nullptr;
			if (PortraitTexture)
			{
				CardPortrait->SetBrushFromTexture(PortraitTexture, true);
				CardPortrait->SetColorAndOpacity(FLinearColor::White);
				CardPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				CardPortrait->SetVisibility(ESlateVisibility::Collapsed);
			}
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
	const bool bCanRetry = !IsBattlePresentationPending()
		&& bEnemyIntentCompletionRecoveryPending
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
	if (RejectBattlePresentationMutation())
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
	const FGameXXKCardBattleRuntime Before = MutableState.CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
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
			return QueueMutationPresentation(
				Before,
				DamageResults,
				EBattlePresentationContinuation::ResumeEnemyIntentAfterMutation);
		}
		FString SkipError;
		if (!FGameXXKCardBattleAdapter::SkipCurrentEnemyIntent(MutableState, &SkipError))
		{
			DiscardPresentationHudSnapshot();
			LastCardInteractionError += FString::Printf(TEXT("\n敌方意图恢复失败：%s"), *SkipError);
			return false;
		}
		LastCardInteractionError += TEXT("\n已跳过该异常敌方意图，继续结算。");
		return QueueMutationPresentation(
			Before,
			DamageResults,
			EBattlePresentationContinuation::ResumeEnemyIntentAfterMutation);
	}
	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		DamageResults,
		EBattlePresentationContinuation::ResumeEnemyIntentAfterMutation);
}

bool UGameXXKBattleBoardWidget::CompleteEnemyIntentPresentation()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
	{
		LastCardInteractionError = TEXT("敌方意图完成时战斗状态已丢失。");
		return false;
	}

	const FGameXXKCardBattleRuntime Before = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	TArray<FGameXXKCardDamageResult> DamageResults;
	FString Error;
	if (!FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(Subsystem->GetMutableRuntimeState(), DamageResults, &Error))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		bEnemyIntentCompletionRecoveryPending = true;
		EnemyIntentPresentationState = EGameXXKEnemyIntentPresentationState::Settle;
		EnemyIntentPresentationElapsed = 0.0f;
		return false;
	}
	bEnemyIntentCompletionRecoveryPending = false;
	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		DamageResults,
		EBattlePresentationContinuation::FinalizeEnemyPhase);
}

bool UGameXXKBattleBoardWidget::RetryEnemyIntentCompletion()
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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
	if (IsBattlePresentationPending())
	{
		ApplyBattlePresentationInteractionLock();
		return;
	}
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
			if (IsBattlePresentationPending())
			{
				ApplyBattlePresentationInteractionLock();
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

	const FName HoveredInstanceId = HandCardInstanceIds.IsValidIndex(SlotIndex)
		? HandCardInstanceIds[SlotIndex]
		: NAME_None;
	if (!bHovered)
	{
		if (CachedOutcomeTargetUnitId.IsNone() && CachedOutcomeCardInstanceId == HoveredInstanceId)
		{
			ClearCardOutcomePreview();
		}
		return;
	}

	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	FGameXXKCardPlayPreview Playability;
	FString Error;
	if (Subsystem
		&& !HoveredInstanceId.IsNone()
		&& FGameXXKCardBattleAdapter::BuildCardPlayPreview(
			Subsystem->GetRuntimeState(), HoveredInstanceId, Playability, &Error)
		&& Playability.bCanPlay
		&& Playability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies)
	{
		BuildCardOutcomePreview(HoveredInstanceId, NAME_None);
	}
	else
	{
		ClearCardOutcomePreview();
	}
}

void UGameXXKBattleBoardWidget::ClearCardTooltipHoverState()
{
	ClearCardOutcomePreview();
	HoveredCardTooltipSource = ECardTooltipSource::None;
	HoveredHandCardSlot = INDEX_NONE;
	HoveredRewardCardSlot = INDEX_NONE;
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
	if (bHovered)
	{
		// Track the option slot instead of a card id: relic and attribute options
		// carry no CardId, but every option owns exactly one visible slot.
		HoveredCardTooltipSource = ECardTooltipSource::Reward;
		HoveredRewardCardSlot = SlotIndex;
		HoveredCardTooltipId = NAME_None;
		HoveredPendingChoiceKind = EGameXXKCardPendingChoiceKind::Invalid;
	}
	else if (HoveredCardTooltipSource == ECardTooltipSource::Reward && HoveredRewardCardSlot == SlotIndex)
	{
		HoveredCardTooltipSource = ECardTooltipSource::None;
		HoveredRewardCardSlot = INDEX_NONE;
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
		if (bPlayedCardCommitActive && CardButton == PlayedCardCommitButton.Get())
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
	const bool bShowHeroTaskSearch = PendingChoice && PendingChoice->Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
	const bool bShowForcedDiscard = PendingChoice && PendingChoice->Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard;
	const bool bShowPendingChoice = bShowInsight || bShowHeroTaskSearch || bShowForcedDiscard;
	if (PendingChoicePanel)
	{
		PendingChoicePanel->SetVisibility(bShowPendingChoice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (PendingChoiceCancelButton)
	{
		const bool bCanCancelInsight = bShowInsight && PendingChoice->bCanCancel;
		PendingChoiceCancelButton->SetVisibility(bCanCancelInsight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PendingChoiceCancelButton->SetIsEnabled(
			bCanCancelInsight && !bFixtureReadOnly && !IsBattlePresentationPending());
	}
	if (PendingChoicePromptText && bShowPendingChoice)
	{
		const int32 RequiredDiscardCount = PendingChoice->RequiredDiscardCount > 0
			? PendingChoice->RequiredDiscardCount
			: PendingChoice->RequiredCount;
		const FString Prompt = bShowHeroTaskSearch
			? TEXT("选择一张尚未完成任务的主角牌")
			: bShowInsight
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
			CardButton->SetIsEnabled(
				bHasCandidate && !bFixtureReadOnly && !IsBattlePresentationPending());
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
				(bShowInsight || bShowHeroTaskSearch) ? TEXT("加入手牌") : TEXT("点击弃置"))));
		}
	}
}

void UGameXXKBattleBoardWidget::RefreshPendingRewardChoices()
{
	EnsureBattleVisualResourcesLoaded();
	const bool bFixtureReadOnly = IsBattleHudFixtureReadOnly();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	PendingRewardOptions = Subsystem ? Subsystem->GetRuntimeState().CardRun.PendingReward.Options : TArray<FGameXXKBattleRewardOption>();
	PendingRewardCardIds = GetPendingRouteRewardCardIds();
	const bool bShowRewards = PendingRewardOptions.Num() > 0;
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
		SkipRewardButton->SetIsEnabled(
			bShowRewards && !bFixtureReadOnly && !IsBattlePresentationPending());
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
			RewardButton->SetIsEnabled(
				bHasReward && !bFixtureReadOnly && !IsBattlePresentationPending());
		}
		if (!bHasReward)
		{
			continue;
		}

		const FGameXXKBattleRewardOption& Option = PendingRewardOptions[SlotIndex];
		const bool bIsCardOption = Option.Kind == EGameXXKBattleRewardKind::DeckCardUpgrade
			|| Option.Kind == EGameXXKBattleRewardKind::BossCard;
		const FGameXXKCardDefinition* Definition = !Option.CardId.IsNone()
			? FGameXXKCardCatalog::FindCardDefinition(Option.CardId)
			: nullptr;
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString PreviewError;
		bool bPreviewValid = !bIsCardOption;
		if (Option.Kind == EGameXXKBattleRewardKind::BossCard)
		{
			bPreviewValid = Subsystem
				&& FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
					Subsystem->GetRuntimeState(),
					Option.CardId,
					NAME_None,
					Preview,
					&PreviewError)
				&& (Preview.Decision == EGameXXKRouteCardAcquisitionDecision::CanCommit
					|| Preview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
		}
		else if (bIsCardOption)
		{
			// Deck-card upgrades never touch route-deck capacity; always committable.
			bPreviewValid = true;
		}
		if (RewardButton)
		{
			RewardButton->SetIsEnabled(
				bPreviewValid && !bFixtureReadOnly && !IsBattlePresentationPending());
		}
		if (bIsCardOption)
		{
			ApplyCardPresentation(RewardButton, RewardLabel, RewardPortrait, RewardInfoStrip, Definition);
		}
		else
		{
			if (RewardInfoStrip)
			{
				RewardInfoStrip->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (RewardPortrait && Option.Kind != EGameXXKBattleRewardKind::Relic)
			{
				RewardPortrait->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (RewardLabel)
		{
			if (Option.Kind == EGameXXKBattleRewardKind::EnergyCapBonus)
			{
				RewardLabel->SetText(FText::FromString(TEXT("气力上限 +1\n[属性奖励]")));
			}
			else if (Option.Kind == EGameXXKBattleRewardKind::DrawBonus)
			{
				RewardLabel->SetText(FText::FromString(TEXT("每回合抽牌 +1\n[属性奖励]")));
			}
			else if (Option.Kind == EGameXXKBattleRewardKind::Relic)
			{
				FString RelicName = Option.RelicId.ToString();
				UTexture2D* RelicIcon = nullptr;
				for (const FGameXXKRelicDefinition& RelicDefinition : FGameXXKRelicCatalog::GetAllDefinitions())
				{
					if (RelicDefinition.Id == Option.RelicId)
					{
						RelicName = RelicDefinition.DisplayName.ToString();
						RelicIcon = RelicIconTextures.FindRef(RelicDefinition.Id);
						break;
					}
				}
				if (RewardPortrait)
				{
					if (RelicIcon)
					{
						// Centered relic icon inside the card's portrait area.
						RewardPortrait->SetBrushFromTexture(RelicIcon, true);
						RewardPortrait->SetDesiredSizeOverride(FVector2D(100.0f, 100.0f));
						RewardPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
					else
					{
						RewardPortrait->SetVisibility(ESlateVisibility::Collapsed);
					}
				}
				RewardLabel->SetText(FText::FromString(FString::Printf(TEXT("%s\n[遗物]"), *RelicName)));
			}
			else
			{
				const FString DisplayName = Definition ? Definition->DisplayName.ToString() : Option.CardId.ToString();
				const int32 Energy = Definition ? Definition->EnergyCost : 0;
				const int32 Mana = Definition ? Definition->ManaCost : 0;
				EGameXXKCardQuality ShownQuality = Definition ? Definition->BaseQuality : EGameXXKCardQuality::Common;
				if (Option.Kind == EGameXXKBattleRewardKind::DeckCardUpgrade)
				{
					ShownQuality = FGameXXKCardBattleAdapter::GetNextCardQuality(ShownQuality);
				}
				const FString Quality = FGameXXKCardQualityRules::GetDisplayName(ShownQuality).ToString();
				RewardLabel->SetText(FText::FromString(FString::Printf(
					TEXT("%s\n[%s] %d 气 / %d 内"),
					*DisplayName,
					*Quality,
					Energy,
					Mana)));
			}
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
		CardButton->SetIsEnabled(!bFixtureReadOnly && !IsBattlePresentationPending());
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
	if (RelicIconTextures.IsEmpty())
	{
		// Reward relic icons load once with the other board resources so the
		// reward refresh path performs no synchronous loads.
		for (const FGameXXKRelicDefinition& RelicDefinition : FGameXXKRelicCatalog::GetAllDefinitions())
		{
			if (!RelicDefinition.IconTexturePath.IsNull())
			{
				RelicIconTextures.Add(
					RelicDefinition.Id,
					LoadObject<UTexture2D>(nullptr, *RelicDefinition.IconTexturePath.ToString()));
			}
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

	// Page-18 card face layout shared with the out-of-battle deck pages:
	// bold name band on top, 127x152 portrait below it, no bottom strip.
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("%sLabel"), *NamePrefix));
	Label->SetJustification(ETextJustify::Center);
	Label->SetAutoWrapText(false);
	// Ink card text: all in-battle card faces use black text on the parchment frame.
	Label->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.18f));
	Label->SetShadowOffset(FVector2D(0.5f, 0.5f));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20));
	Label->SetColorAndOpacity(FSlateColor(ResolveCardFaceLabelColor()));
	if (UCanvasPanelSlot* LabelSlot = FaceCanvas->AddChildToCanvas(Label))
	{
		LabelSlot->SetOffsets(FMargin(0.0f, 12.0f, 206.0f, 33.0f));
		LabelSlot->SetAlignment(FVector2D::ZeroVector);
	}

	UImage* Portrait = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("%sPortrait"), *NamePrefix));
	Portrait->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* PortraitSlot = FaceCanvas->AddChildToCanvas(Portrait))
	{
		// Page-18 portrait cut: 127x152 upright, never squeezed.
		PortraitSlot->SetOffsets(FMargin(8.0f, 48.0f, 190.0f, 228.0f));
		PortraitSlot->SetAlignment(FVector2D::ZeroVector);
	}

	// No bottom color strip: the page-18 card face is frame + portrait + name only.
	OutLabel = Label;
	OutPortrait = Portrait;
	OutInfoStrip = nullptr;
}

void UGameXXKBattleBoardWidget::BuildEnemyIntentCardFace(
	UButton* CardButton,
	const FString& NamePrefix,
	UTextBlock*& OutBody,
	UImage*& OutPortrait)
{
	OutBody = nullptr;
	OutPortrait = nullptr;
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
		PortraitSlot->SetOffsets(FMargin(0.0f, 0.0f, EnemyIntentCardSize.X, EnemyIntentCardSize.Y));
		PortraitSlot->SetAlignment(FVector2D::ZeroVector);
	}

	UTextBlock* Body = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("%sBody"), *NamePrefix));
	Body->SetAutoWrapText(true);
	Body->SetJustification(ETextJustify::Center);
	Body->SetColorAndOpacity(FSlateColor(BattleStatusInkColor));
	Body->SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.34f));
	Body->SetShadowOffset(FVector2D(0.5f, 0.5f));
	Body->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11));
	if (UCanvasPanelSlot* BodySlot = FaceCanvas->AddChildToCanvas(Body))
	{
		BodySlot->SetOffsets(FMargin(16.0f, 16.0f, 118.0f, 138.0f));
		BodySlot->SetAlignment(FVector2D::ZeroVector);
	}
	OutBody = Body;
	OutPortrait = Portrait;
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
		if (AcquisitionKey == TEXT("Route.Boss.BlackBear")) return ResolveEnemyPortraitPathByDefinitionId(TEXT("Enemy.Ch2.BlackBear"));
		if (AcquisitionKey == TEXT("Route.Boss.Tiger")) return ResolveEnemyPortraitPathByDefinitionId(TEXT("Enemy.Ch3.Tiger"));
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
	const FGeometry Geometry = BattleDesignStage
		? BattleDesignStage->GetCachedGeometry()
		: GetCachedGeometry();
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

void UGameXXKBattleBoardWidget::ClearCardOutcomePreview()
{
	CachedOutcomeCardInstanceId = NAME_None;
	CachedOutcomeTargetUnitId = NAME_None;
	CachedOutcomeSourceState.Reset();
	CachedOutcomePreview = FGameXXKCardOutcomePreview();
	if (SingleOutcomeWidget)
	{
		SingleOutcomeWidget->Clear();
	}
	if (GroupOutcomeWidget)
	{
		GroupOutcomeWidget->Clear();
	}
}

bool UGameXXKBattleBoardWidget::BuildCardOutcomePreview(
	const FName CardInstanceId,
	const FName RequestedTargetUnitId)
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || CardInstanceId.IsNone())
	{
		ClearCardOutcomePreview();
		return false;
	}

	const FGameXXKRuntimeState& CurrentState = Subsystem->GetRuntimeState();
	const bool bCacheHit = CachedOutcomeSourceState.IsSet()
		&& CachedOutcomeCardInstanceId == CardInstanceId
		&& CachedOutcomeTargetUnitId == RequestedTargetUnitId
		&& FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&CurrentState,
			&CachedOutcomeSourceState.GetValue(),
			PPF_None);
	if (bCacheHit)
	{
		ApplyCardOutcomePreview(CachedOutcomePreview);
		return CachedOutcomePreview.bSuccess;
	}

	ClearCardOutcomePreview();
	++OutcomePreviewBuildCountForTest;
	FGameXXKCardOutcomePreview Preview;
	FString Error;
	const bool bBuilt = FGameXXKCardOutcomePreviewRules::Build(
		CurrentState,
		CardInstanceId,
		RequestedTargetUnitId,
		Preview,
		&Error);
	CachedOutcomeCardInstanceId = CardInstanceId;
	CachedOutcomeTargetUnitId = RequestedTargetUnitId;
	CachedOutcomeSourceState = CurrentState;
	CachedOutcomePreview = Preview;
	ApplyCardOutcomePreview(CachedOutcomePreview);
	return bBuilt;
}

void UGameXXKBattleBoardWidget::ApplyCardOutcomePreview(const FGameXXKCardOutcomePreview& Preview)
{
	if (SingleOutcomeWidget)
	{
		SingleOutcomeWidget->Clear();
	}
	if (GroupOutcomeWidget)
	{
		GroupOutcomeWidget->Clear();
	}

	if (!Preview.bSuccess)
	{
		FGameXXKCardOutcomeTextLine FailureLine;
		FGameXXKCardOutcomeTextSegment& Segment = FailureLine.Segments.AddDefaulted_GetRef();
		Segment.Text = FText::FromString(Preview.FailureText.IsEmpty() ? TEXT("无法预演") : Preview.FailureText);
		Segment.Tone = EGameXXKCardOutcomeTone::Neutral;
		if (Preview.HoveredTargetUnitId.IsNone())
		{
			if (GroupOutcomeWidget)
			{
				GroupOutcomeWidget->SetLines({FailureLine});
			}
		}
		else
		{
			RefreshSingleOutcomePreviewPlacement(Preview.HoveredTargetUnitId);
			if (SingleOutcomeWidget)
			{
				SingleOutcomeWidget->SetLines({FailureLine});
			}
		}
		return;
	}

	if (Preview.Classification == EGameXXKCardOutcomePreviewClass::ManualUnit)
	{
		TArray<FGameXXKCardOutcomeTextLine> Lines = Preview.FocusedLines;
		if (Lines.Num() > 2)
		{
			Lines.SetNum(2);
		}
		RefreshSingleOutcomePreviewPlacement(Preview.HoveredTargetUnitId);
		if (SingleOutcomeWidget)
		{
			SingleOutcomeWidget->SetLines(Lines);
		}
	}
	else if (Preview.Classification == EGameXXKCardOutcomePreviewClass::PureEnemyGroup && GroupOutcomeWidget)
	{
		GroupOutcomeWidget->SetLines(Preview.EnemyPositionLines);
	}
}

void UGameXXKBattleBoardWidget::RefreshSingleOutcomePreviewPlacement(const FName UnitId)
{
	UCanvasPanelSlot* const OutcomeSlot = SingleOutcomeWidget ? Cast<UCanvasPanelSlot>(SingleOutcomeWidget->Slot) : nullptr;
	FVector2D TargetStageCenter;
	if (!OutcomeSlot || !TryResolveUnitTargetStageCenter(UnitId, TargetStageCenter))
	{
		return;
	}

	const FVector2D TargetAnchor(
		TargetStageCenter.X / BattleHudSafeStageDesignSize.X,
		TargetStageCenter.Y / BattleHudSafeStageDesignSize.Y);
	OutcomeSlot->SetAnchors(FAnchors(TargetAnchor.X, TargetAnchor.Y));
	OutcomeSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	OutcomeSlot->SetOffsets(SingleOutcomePreviewOffsets);
}

bool UGameXXKBattleBoardWidget::TryResolveUnitTargetStageCenter(
	const FName UnitId,
	FVector2D& OutStageCenter) const
{
	OutStageCenter = FVector2D::ZeroVector;
	if (UnitId.IsNone())
	{
		return false;
	}

	if (const UGameXXKBattleUnitVisualWidget* const Visual = UnitVisuals.FindRef(UnitId))
	{
		OutStageCenter = Visual->GetStageCenter();
		return true;
	}

	const UCanvasPanelSlot* const ProxySlot = UnitTargetProxies.FindRef(UnitId)
		? Cast<UCanvasPanelSlot>(UnitTargetProxies.FindRef(UnitId)->Slot)
		: nullptr;
	if (!ProxySlot)
	{
		return false;
	}
	const FVector2D ProxyAnchor = ProxySlot->GetAnchors().Minimum;
	OutStageCenter = FVector2D(
		ProxyAnchor.X * BattleHudSafeStageDesignSize.X,
		ProxyAnchor.Y * BattleHudSafeStageDesignSize.Y);
	return true;
}

bool UGameXXKBattleBoardWidget::BeginCardTargeting(const FGameXXKCardPlayPreview& Preview)
{
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
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
	RefreshUnitTargetingPresentation();
	RefreshProgrammaticLayout();
	InvalidateLayoutAndVolatility();
	return true;
}

bool UGameXXKBattleBoardWidget::ResolveAutomaticCardPlay(FName CardInstanceId)
{
	ClearCardOutcomePreview();
	if (RejectBattleHudFixtureMutation())
	{
		return false;
	}
	if (RejectBattlePresentationMutation())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		LastCardInteractionError = TEXT("卡牌战斗状态不可用。");
		return false;
	}

	const FGameXXKCardBattleRuntime Before = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
	CapturePresentationHudSnapshot(Before);
	FGameXXKCardPlayResult Result;
	FString Error;
	if (!FGameXXKCardBattleAdapter::ResolveCardPlay(Subsystem->GetMutableRuntimeState(), CardInstanceId, NAME_None, Result, &Error))
	{
		DiscardPresentationHudSnapshot();
		LastCardInteractionError = Error;
		RefreshProgrammaticLayout();
		return false;
	}
	LastCardInteractionError.Reset();
	return QueueMutationPresentation(
		Before,
		Result.DamageResults,
		EBattlePresentationContinuation::FinalizeCardMutation,
		Result.CardInstanceId);
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
	if (CachedOutcomeSourceState.IsSet()
		&& !FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(),
			&CachedOutcomeSourceState.GetValue(),
			PPF_None))
	{
		ClearCardOutcomePreview();
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
	RefreshUnitTargetingPresentation();
	return true;
}

void UGameXXKBattleBoardWidget::ClearCardTargetingState()
{
	ClearCardOutcomePreview();
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
	RefreshUnitTargetingPresentation();
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
		ClearCardOutcomePreview();
		if (!Subsystem->ResolveBattleVictory(false))
		{
			LastCardInteractionError = TEXT("战斗胜利奖励未能生成。");
			return false;
		}
	}
	else if (Phase == EGameXXKCardBattlePhase::Defeat)
	{
		ClearCardOutcomePreview();
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
	if (const UGameXXKBattleUnitVisualWidget* const Visual = UnitVisuals.FindRef(OwnerUnitId))
	{
		return Visual->GetStageCenter();
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (State)
	{
		if (State->CardRun.bHasActiveCardBattle)
		{
			FGameXXKBattleUnitHudView View;
			FGameXXKFixedUnitHudLayout FixedLayout;
			if (FGameXXKBattlePresentation::BuildUnitHudView(
					State->CardRun.ActiveBattle,
					OwnerUnitId,
					ResolveProjectedUnitHudDisplayName(OwnerUnitId),
					View)
				&& TryResolveFixedUnitHudLayout(View, FixedLayout))
			{
				return FVector2D(
					FixedLayout.Anchors.Minimum.X * BattleHudSafeStageDesignSize.X,
					FixedLayout.Anchors.Minimum.Y * BattleHudSafeStageDesignSize.Y
						+ FormationVisualVerticalOffsetPixels);
			}

			// A malformed or stale CardRun owner still uses the lifted common-stage
			// fallback; it must never fall through to legacy actor projection.
			return BattleHudSafeStageDesignSize * 0.5f
				+ FVector2D(0.0f, FormationVisualVerticalOffsetPixels);
		}

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
		RefreshProjectedUnitHuds();
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
		ChoosePendingBattleRewardOption(SlotIndex, SelectedRouteRewardReplacementEntryId);
	}
}

void UGameXXKBattleBoardWidget::HandleRouteRewardReplacementClicked(FName EntryId)
{
	SelectRouteRewardReplacementEntry(EntryId);
}
