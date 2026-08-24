#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

	namespace
{
	struct FScopedActionCallbackGuard
	{
		bool& bFlag;
		bool bPrevious;

		explicit FScopedActionCallbackGuard(bool& InFlag)
			: bFlag(InFlag)
			, bPrevious(InFlag)
		{
			bFlag = true;
		}

		~FScopedActionCallbackGuard()
		{
			bFlag = bPrevious;
		}
	};

	class SGameXXKDesktopTrainingActionButton final : public SButton
	{
	public:
		using FArguments = SButton::FArguments;

		void Construct(const FArguments& InArgs, UGameXXKDesktopTrainingActionButton* InOwner)
		{
			Owner = InOwner;
			SButton::Construct(InArgs);
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				if (Owner.IsValid())
				{
					Owner->HandleRightMouseButtonDown();
				}
				// Always consume right-click so it never falls through to the
				// normal SButton click path and triggers the left-click action.
				return FReply::Handled();
			}
			return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		}

	private:
		TWeakObjectPtr<UGameXXKDesktopTrainingActionButton> Owner;
	};

	constexpr int32 WarehouseColumns = 4;
	constexpr int32 WarehouseRows = 9;
	constexpr int32 WarehousePageSize = WarehouseColumns * WarehouseRows;
	constexpr int32 ToolSlotCount = 9;
	constexpr int32 ToolModeCount = 5;
	constexpr int32 TopToolbarButtonCount = 5;
	constexpr int32 ActionCloseWarehouse = 62;
	constexpr int32 ActionCloseCentralPage = 63;
	constexpr int32 ActionCloseRightPanel = 64;
	const FVector2D TravelVisualSize(953.0f, 202.0f);
	const FVector2D TravelBackgroundImageSize(FGameXXKTrainingTravelVisualRuntime::LaneTileWidth, 300.0f);
	const FVector2D TravelCombatVisualSize(150.0f, 150.0f);
	const FVector2D TravelHeroWalkVisualSize(112.0f, 112.0f);
	const FVector2D TravelHealthBarSize(124.0f, 9.0f);

	FName MakeTravelOneKUnitId(const FName UnitId)
	{
		if (UnitId.IsNone())
		{
			return NAME_None;
		}
		FString Value = UnitId.ToString();
		if (!Value.EndsWith(TEXT(".1K"), ESearchCase::IgnoreCase))
		{
			Value += TEXT(".1K");
		}
		return FName(*Value);
	}
	// Deterministic alpha-bounds audit of the approved source atlases.  The walk
	// atlas occupies 90.6% of its cell height; the battle clips occupy 81.2%
	// (Idle), 59.8% (Attack), 69.5% (Hit), and 80.9% (Death).  Normalize each
	// action around a bottom-center pivot so action changes preserve the hero's
	// apparent size and ground contact.
	float ResolveTravelHeroContentScale(const EGameXXKBattleAnimationAction Action)
	{
		switch (Action)
		{
		case EGameXXKBattleAnimationAction::Attack: return 1.516f;
		case EGameXXKBattleAnimationAction::Hit: return 1.303f;
		case EGameXXKBattleAnimationAction::Death: return 1.121f;
		case EGameXXKBattleAnimationAction::Idle:
		default: return 1.117f;
		}
	}

	constexpr float TravelTargetVisibleHeightFraction = 0.906f;

	struct FTravelEnemyAlphaHeights
	{
		const TCHAR* AssetToken;
		float Idle;
		float Attack;
		float Hit;
		float Death;
	};

	// Median occupied-cell heights from the approved 2K atlas sources.  The
	// atlases have materially different transparent padding per enemy/action,
	// so a single widget scale causes the visible character to pulse in size.
	constexpr FTravelEnemyAlphaHeights TravelEnemyAlphaHeights[] = {
		{TEXT("enemy_01_rooster"), 0.8047f, 0.5869f, 0.6816f, 0.7363f},
		{TEXT("enemy_02_goat"), 0.8320f, 0.6191f, 0.5352f, 0.7646f},
		{TEXT("enemy_03_weasel"), 0.5840f, 0.4307f, 0.5127f, 0.4824f},
		{TEXT("enemy_04_civet"), 0.5703f, 0.4688f, 0.5547f, 0.6133f},
		{TEXT("enemy_05_ironfeather"), 0.8066f, 0.5713f, 0.5723f, 0.6602f},
		{TEXT("enemy_06_bluehorn"), 0.7910f, 0.6094f, 0.5859f, 0.7090f},
		{TEXT("enemy_19_moneyrat_boss"), 0.6562f, 0.5713f, 0.5684f, 0.5781f},
	};

	float ResolveTravelEnemyContentScale(
		const FName EnemyDefinitionId,
		const EGameXXKBattleAnimationAction Action)
	{
		const FString AssetId = FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(EnemyDefinitionId, true);
		for (const FTravelEnemyAlphaHeights& Entry : TravelEnemyAlphaHeights)
		{
			if (!AssetId.Contains(Entry.AssetToken))
			{
				continue;
			}

			float OccupiedHeight = Entry.Idle;
			switch (Action)
			{
			case EGameXXKBattleAnimationAction::Attack: OccupiedHeight = Entry.Attack; break;
			case EGameXXKBattleAnimationAction::Hit: OccupiedHeight = Entry.Hit; break;
			case EGameXXKBattleAnimationAction::Death: OccupiedHeight = Entry.Death; break;
			case EGameXXKBattleAnimationAction::Idle:
			default: break;
			}
			return FMath::Clamp(TravelTargetVisibleHeightFraction / OccupiedHeight, 1.0f, 2.2f);
		}
		return 1.13f;
	}

	struct FTravelPartyAlphaHeights
	{
		const TCHAR* AssetToken;
		float Idle;
		float Attack;
		float Hit;
		float Death;
	};

	// Median occupied-cell heights measured from the approved action atlases.
	// Using one scale per identity/action keeps Idle/Attack/Hit/Death at the
	// same apparent height and ground anchor instead of visibly pulsing.
	constexpr FTravelPartyAlphaHeights TravelPartyAlphaHeights[] = {
		{TEXT("character_00_hero"), 0.8164f, 0.6016f, 0.6992f, 0.8125f},
		{TEXT("character_01_blade"), 0.7832f, 0.6484f, 0.5957f, 0.7109f},
		{TEXT("character_02_guard"), 0.7969f, 0.6367f, 0.7363f, 0.7617f},
		{TEXT("character_03_healer"), 0.7969f, 0.5879f, 0.8242f, 0.7930f},
		{TEXT("character_04_hunter"), 0.6582f, 0.5781f, 0.7129f, 0.6699f},
		{TEXT("character_05_sorcerer"), 0.7773f, 0.5566f, 0.6738f, 0.7461f},
		{TEXT("character_06_formation_master"), 0.7305f, 0.6094f, 0.5879f, 0.6699f},
		{TEXT("character_07_tusi_chief"), 0.6934f, 0.6074f, 0.5703f, 0.5664f},
		{TEXT("character_08_song_jin_bao"), 0.8340f, 0.8125f, 0.6367f, 0.7949f},
		{TEXT("character_09_yue_bai"), 0.7344f, 0.6250f, 0.6797f, 0.8281f},
		{TEXT("character_10_zhou_guang_zu"), 0.8262f, 0.7148f, 0.6680f, 0.7969f},
		{TEXT("character_11_jin_gui"), 0.6992f, 0.7207f, 0.5859f, 0.8242f},
		{TEXT("character_12_qiong_mei_er"), 0.8047f, 0.7383f, 0.7793f, 0.7715f},
	};

	float ResolveTravelPartyContentScale(
		const FName UnitId,
		const EGameXXKBattleAnimationAction Action)
	{
		const FString AssetId = FGameXXKBattleAnimationPresentation::ResolveUnitAssetId(UnitId, false);
		for (const FTravelPartyAlphaHeights& Entry : TravelPartyAlphaHeights)
		{
			if (AssetId.Contains(Entry.AssetToken))
			{
				float OccupiedHeight = Entry.Idle;
				switch (Action)
				{
				case EGameXXKBattleAnimationAction::Attack: OccupiedHeight = Entry.Attack; break;
				case EGameXXKBattleAnimationAction::Hit: OccupiedHeight = Entry.Hit; break;
				case EGameXXKBattleAnimationAction::Death: OccupiedHeight = Entry.Death; break;
				case EGameXXKBattleAnimationAction::Idle:
				default: break;
				}
				return FMath::Clamp(TravelTargetVisibleHeightFraction / OccupiedHeight, 1.0f, 2.2f);
			}
		}
		return 1.12f;
	}
	const FLinearColor Ink(0.06f, 0.045f, 0.035f, 0.98f);
	const FLinearColor Panel(0.13f, 0.09f, 0.055f, 0.97f);
	const FLinearColor PanelAlt(0.20f, 0.13f, 0.07f, 0.98f);
	const FLinearColor Accent(0.82f, 0.43f, 0.08f, 1.0f);
	const FLinearColor Gold(1.0f, 0.78f, 0.25f, 1.0f);
	static constexpr const TCHAR* PanelLargeTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	static constexpr const TCHAR* ItemSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	static constexpr const TCHAR* EquipmentSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_EquipmentSlot.T_MasterV2_EquipmentSlot");
	static constexpr const TCHAR* LockedIconTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardLockedIcon.T_MasterV2_CardLockedIcon");
	static constexpr const TCHAR* HeroFullBodyTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody");
	static constexpr const TCHAR* CloseInkTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk");
	static constexpr const TCHAR* IngotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_Ingot.T_MasterV2_Ingot");
	static constexpr const TCHAR* CharacterTabNormalTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1.003_tab_1");
	static constexpr const TCHAR* CharacterTabSelectedTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/004_tab_2.004_tab_2");
	static constexpr const TCHAR* SettingsTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/HUD/T_TownPsd_HudSettings.T_TownPsd_HudSettings");
	static constexpr const TCHAR* InventoryTextureRoot = TEXT("/Game/GameXXK/UI/Inventory/Textures/");
	static constexpr const TCHAR* RouteNodeTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavRoute.T_MasterV2_NavRoute");
	static constexpr const TCHAR* TruthNavWarehouseTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse");
	static constexpr const TCHAR* TruthNavFormationTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavFormation.T_TrainingNavFormation");
	static constexpr const TCHAR* TruthNavTalentsTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
	static constexpr const TCHAR* TruthNavToolsTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools");
	static constexpr const TCHAR* TruthNavTrainingTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining.T_TrainingNavTraining");
	static constexpr const TCHAR* TruthTopToolbarAlwaysOnTopTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTop.T_TrainingTopToolbarAlwaysOnTop");
	static constexpr const TCHAR* TruthTopToolbarAlwaysOnTopOffTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTopOffGray.T_TrainingTopToolbarAlwaysOnTopOffGray");
	static constexpr const TCHAR* TravelHeroAtlasTexturePath = TEXT("/Game/GameXXK/UI/Training/Generated/walkloop_pilot_v1/character_00_hero_walk_left/atlas_1K/T_TrainingHeroWalkLeft_1K.T_TrainingHeroWalkLeft_1K");
	static constexpr const TCHAR* TravelBackgroundTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background.T_TrainingIdleStrip_Background");
	static constexpr const TCHAR* TravelBackgroundFallbackTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Backgrounds/T_TownPsd_Background_Map.T_TownPsd_Background_Map");
	static constexpr const TCHAR* TravelHeroFallbackTexturePaths[] = {
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"),
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody")};

	FString TravelVisualPhaseName(const EGameXXKTrainingTravelVisualPhase Phase)
	{
		switch (Phase)
		{
		case EGameXXKTrainingTravelVisualPhase::Walking: return TEXT("Walking");
		case EGameXXKTrainingTravelVisualPhase::EncounterIdle: return TEXT("EncounterIdle");
		case EGameXXKTrainingTravelVisualPhase::HeroAttack: return TEXT("HeroAttack");
		case EGameXXKTrainingTravelVisualPhase::EnemyHit: return TEXT("EnemyHit");
		case EGameXXKTrainingTravelVisualPhase::EnemyAttack: return TEXT("EnemyAttack");
		case EGameXXKTrainingTravelVisualPhase::HeroHit: return TEXT("HeroHit");
		case EGameXXKTrainingTravelVisualPhase::EnemyDeath: return TEXT("EnemyDeath");
		case EGameXXKTrainingTravelVisualPhase::HeroDeath: return TEXT("HeroDeath");
		case EGameXXKTrainingTravelVisualPhase::Paused: return TEXT("Paused");
		default: return TEXT("Unknown");
		}
	}

	FString TrainingTravelPhaseName(const EGameXXKTrainingTravelPhase Phase)
	{
		switch (Phase)
		{
		case EGameXXKTrainingTravelPhase::Idle: return TEXT("Idle");
		case EGameXXKTrainingTravelPhase::Walking: return TEXT("Walking");
		case EGameXXKTrainingTravelPhase::Combat: return TEXT("Combat");
		case EGameXXKTrainingTravelPhase::Defeated: return TEXT("Defeated");
		default: return TEXT("Unknown");
		}
	}

	FString BattleAnimationActionName(const EGameXXKBattleAnimationAction Action)
	{
		switch (Action)
		{
		case EGameXXKBattleAnimationAction::Idle: return TEXT("Idle");
		case EGameXXKBattleAnimationAction::Attack: return TEXT("Attack");
		case EGameXXKBattleAnimationAction::Hit: return TEXT("Hit");
		case EGameXXKBattleAnimationAction::Buff: return TEXT("Buff");
		case EGameXXKBattleAnimationAction::Debuff: return TEXT("Debuff");
		case EGameXXKBattleAnimationAction::Death: return TEXT("Death");
		case EGameXXKBattleAnimationAction::Impact: return TEXT("Impact");
		default: return TEXT("Unknown");
		}
	}

	TMap<FString, TStrongObjectPtr<UTexture2D>>& GetTextureCache()
	{
		static TMap<FString, TStrongObjectPtr<UTexture2D>> Cache;
		return Cache;
	}

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		if (!Path)
		{
			return nullptr;
		}
		TMap<FString, TStrongObjectPtr<UTexture2D>>& Cache = GetTextureCache();
		const FString Key(Path);
		if (const TStrongObjectPtr<UTexture2D>* Cached = Cache.Find(Key))
		{
			return Cached->Get();
		}
		UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, Path);
		Cache.FindOrAdd(Key) = TStrongObjectPtr<UTexture2D>(Loaded);
		return Loaded;
	}

	FSlateBrush MakeTextureBrush(const TCHAR* Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FSlateBrush MakeBoxTextureBrush(
		const TCHAR* Path,
		const FVector2D& ImageSize,
		const FMargin& Margin = FMargin(0.065f),
		const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize, Tint);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(
		const TCHAR* Path,
		const FVector2D& ImageSize,
		const FMargin& Margin = FMargin(0.065f),
		const FLinearColor& Tint = FLinearColor::White)
	{
		FButtonStyle Style;
		const FSlateBrush Normal = MakeBoxTextureBrush(Path, ImageSize, Margin, Tint);
		Style.SetNormal(Normal);
		Style.SetHovered(MakeBoxTextureBrush(Path, ImageSize, Margin, Tint * FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeBoxTextureBrush(Path, ImageSize, Margin, Tint * FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeBoxTextureBrush(Path, ImageSize, Margin, FLinearColor(0.45f, 0.45f, 0.45f, 0.75f)));
		return Style;
	}

	FButtonStyle MakeImageButtonStyle(
		const TCHAR* Path,
		const FVector2D& ImageSize,
		const FLinearColor& Tint = FLinearColor::White)
	{
		FButtonStyle Style;
		const FSlateBrush Normal = MakeTextureBrush(Path, ImageSize, Tint);
		Style.SetNormal(Normal);
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(1.06f, 1.06f, 1.06f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(0.86f, 0.86f, 0.86f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.55f, 0.55f, 0.55f, 0.72f)));
		return Style;
	}

	FButtonStyle MakeInvisibleButtonStyle()
	{
		FSlateBrush EmptyBrush;
		EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style;
		Style.SetNormal(EmptyBrush);
		Style.SetHovered(EmptyBrush);
		Style.SetPressed(EmptyBrush);
		Style.SetDisabled(EmptyBrush);
		return Style;
	}

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FText& Text,
		int32 Size,
		const FLinearColor& Color = FLinearColor(0.06f, 0.045f, 0.035f, 0.98f),
		const FName Name = NAME_None)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Result->SetAutoWrapText(true);
		return Result;
	}

	UTextBlock* MakeButtonText(
		UWidgetTree* Tree,
		const FText& Text,
		const int32 Size,
		const FLinearColor& Color = FLinearColor(0.06f, 0.045f, 0.035f, 0.98f))
	{
		UTextBlock* Result = MakeText(Tree, Text, Size, Color);
		Result->SetAutoWrapText(false);
		Result->SetJustification(ETextJustify::Center);
		return Result;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FName Name = NAME_None)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		const FSlateBrush Brush = MakeBoxTextureBrush(PanelLargeTexturePath, FVector2D(320.0f, 180.0f));
		if (Brush.GetResourceObject())
		{
			Result->SetBrush(Brush);
			Result->SetBrushColor(FLinearColor::White);
		}
		else
		{
			Result->SetBrushColor(Color);
		}
		return Result;
	}

	UBorder* MakeTransparentPanel(UWidgetTree* Tree, const FName Name)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		FSlateBrush TransparentBrush;
		TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentBrush.TintColor = FSlateColor(FLinearColor::Transparent);
		Result->SetBrush(TransparentBrush);
		Result->SetBrushColor(FLinearColor::Transparent);
		return Result;
	}

	UBorder* MakeSlotPanel(
		UWidgetTree* Tree,
		const TCHAR* TexturePath,
		const FLinearColor& Color,
		const FVector2D& ImageSize,
		const FName Name = NAME_None)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		const FSlateBrush Brush = MakeBoxTextureBrush(TexturePath, ImageSize, FMargin(0.08f));
		if (Brush.GetResourceObject())
		{
			Result->SetBrush(Brush);
			Result->SetBrushColor(FLinearColor::White);
		}
		else
		{
			Result->SetBrushColor(Color);
		}
		return Result;
	}

	UBorder* MakeImagePanel(
		UWidgetTree* Tree,
		const TCHAR* TexturePath,
		const FLinearColor& FallbackColor,
		const FVector2D& ImageSize,
		const FName Name = NAME_None)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		const FSlateBrush Brush = MakeTextureBrush(TexturePath, ImageSize);
		if (Brush.GetResourceObject())
		{
			Result->SetBrush(Brush);
			Result->SetBrushColor(FLinearColor::White);
		}
		else
		{
			Result->SetBrushColor(FallbackColor);
		}
		return Result;
	}

	template <typename T>
	void AddCanvas(UCanvasPanel* Canvas, T* Child, const FVector2D& Position, const FVector2D& Size)
	{
		if (!Canvas || !Child)
		{
			return;
		}
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Canvas->AddChild(Child));
		if (Slot)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	void AddCanvasRect(UCanvasPanel* Canvas, UWidget* Child, const FVector4& Rect)
	{
		AddCanvas(Canvas, Child, FVector2D(Rect.X, Rect.Y), FVector2D(Rect.Z, Rect.W));
	}

	FText NavText(const EGameXXKDesktopTrainingNav Nav)
	{
		switch (Nav)
		{
		case EGameXXKDesktopTrainingNav::Warehouse: return FText::FromString(TEXT("仓库"));
		case EGameXXKDesktopTrainingNav::Formation: return FText::FromString(TEXT("编队"));
		case EGameXXKDesktopTrainingNav::Talents: return FText::FromString(TEXT("天赋"));
		case EGameXXKDesktopTrainingNav::Tools: return FText::FromString(TEXT("工具"));
		case EGameXXKDesktopTrainingNav::Training: return FText::FromString(TEXT("历练"));
		default: return FText::GetEmpty();
		}
	}

	const TCHAR* BottomNavigationIconTexturePath(const EGameXXKDesktopTrainingNav Nav)
	{
		switch (Nav)
		{
		case EGameXXKDesktopTrainingNav::Warehouse: return TruthNavWarehouseTexturePath;
		case EGameXXKDesktopTrainingNav::Formation: return TruthNavFormationTexturePath;
		case EGameXXKDesktopTrainingNav::Talents: return TruthNavTalentsTexturePath;
		case EGameXXKDesktopTrainingNav::Tools: return TruthNavToolsTexturePath;
		case EGameXXKDesktopTrainingNav::Training: return TruthNavTrainingTexturePath;
		default: return nullptr;
		}
	}

	UWidget* MakeNavigationContent(
		UWidgetTree* Tree,
		const EGameXXKDesktopTrainingNav Nav,
		const bool bSelected)
	{
		UVerticalBox* Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(66.0f);
		IconBox->SetHeightOverride(66.0f);
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		const FLinearColor IconTint = bSelected ? FLinearColor::White : FLinearColor(0.86f, 0.86f, 0.86f, 1.0f);
		const FSlateBrush IconBrush = MakeTextureBrush(BottomNavigationIconTexturePath(Nav), FVector2D(66.0f, 66.0f), IconTint);
		if (IconBrush.GetResourceObject())
		{
			Icon->SetBrush(IconBrush);
		}
		UScaleBox* IconScale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		IconScale->SetStretch(EStretch::ScaleToFit);
		IconScale->SetStretchDirection(EStretchDirection::Both);
		IconScale->SetContent(Icon);
		IconBox->AddChild(IconScale);
		UVerticalBoxSlot* IconSlot = Content->AddChildToVerticalBox(IconBox);
		IconSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Label = MakeButtonText(Tree, NavText(Nav), 20, bSelected ? FLinearColor(0.48f, 0.12f, 0.07f, 1.0f) : Ink);
		Label->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* LabelSlot = Content->AddChildToVerticalBox(Label);
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		LabelSlot->SetHorizontalAlignment(HAlign_Fill);
		return Content;
	}

	TArray<FName> SortedVisibleInventoryItemIds(const FGameXXKRuntimeState& State)
	{
		TArray<FName> Result;
		for (const TPair<FName, int32>& Pair : State.Inventory)
		{
			if (!Pair.Key.IsNone() && Pair.Value > 0)
			{
				Result.Add(Pair.Key);
			}
		}
		Result.Sort([](const FName& Left, const FName& Right)
		{
			return Left.ToString() < Right.ToString();
		});
		return Result;
	}

	FString ItemDisplayName(const FName ItemId)
	{
		bool bFound = false;
		const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		return bFound && !Definition.DisplayName.IsEmpty()
			? Definition.DisplayName.ToString()
			: ItemId.ToString();
	}

	FString EquipmentDisplayName(const FGameXXKEquipmentCollectionState& Collection, const FName InstanceId)
	{
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(Collection, InstanceId);
		if (!Instance)
		{
			return InstanceId.ToString();
		}
		if (const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId))
		{
			return FString::Printf(TEXT("%s\nLv.%d"), *Definition->DisplayName.ToString(), Instance->ItemLevel);
		}
		return FString::Printf(TEXT("%s\nLv.%d"), *Instance->BaseEquipmentId.ToString(), Instance->ItemLevel);
	}

	FString EquipmentIconTexturePath(const FGameXXKEquipmentCollectionState& Collection, const FName InstanceId)
	{
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(Collection, InstanceId);
		if (!Instance)
		{
			return FString();
		}
		if (const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId))
		{
			return Definition->IconSoftPath.ToString();
		}
		return FString();
	}

	FString InventoryItemIconTexturePath(const FName ItemId)
	{
		const FSoftObjectPath GemIconPath = FGameXXKGemRules::GetIconTexturePathForItemId(ItemId);
		if (GemIconPath.IsValid()) return GemIconPath.ToString();
		const FString Root(InventoryTextureRoot);
		if (ItemId == UGameXXKMVPRules::ItemHealingPowder()) return Root + TEXT("T_ItemHealingPowder.T_ItemHealingPowder");
		if (ItemId == UGameXXKMVPRules::ItemEnhancementStone()) return TEXT("/Game/GameXXK/UI/Items/strengthening_stone.strengthening_stone");
		if (ItemId == UGameXXKMVPRules::ItemRefinementSand()) return TEXT("/Game/GameXXK/UI/Items/refinement_sand.refinement_sand");
		if (ItemId == UGameXXKMVPRules::ItemQingshanRouteSeal()) return TEXT("/Game/GameXXK/UI/Items/qingshan_suppression_token.qingshan_suppression_token");
		if (ItemId == FName(TEXT("Item.LingzhiPowder"))) return Root + TEXT("T_ItemLingzhiPowder.T_ItemLingzhiPowder");
		if (ItemId == FName(TEXT("Item.QingxinTea"))) return Root + TEXT("T_ItemQingxinTea.T_ItemQingxinTea");
		if (ItemId == FName(TEXT("Item.CraneSachet"))) return Root + TEXT("T_ItemCraneSachet.T_ItemCraneSachet");
		if (ItemId == UGameXXKMVPRules::ItemIronSword()) return Root + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword");
		if (ItemId == UGameXXKMVPRules::ItemClothArmor()) return Root + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor");
		if (ItemId == FName(TEXT("Item.CranePatternTalisman"))) return Root + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman");
		if (ItemId == FName(TEXT("Item.InkstonePendant"))) return Root + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant");
		if (ItemId == UGameXXKMVPRules::ItemWoodenSword()) return Root + TEXT("T_ItemWoodenSword.T_ItemWoodenSword");
		if (ItemId == UGameXXKMVPRules::ItemStarterClothArmor()) return Root + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor");
		if (ItemId == UGameXXKMVPRules::ItemClothTalisman()) return Root + TEXT("T_ItemClothTalisman.T_ItemClothTalisman");
		return FString();
	}

	UOverlay* MakeIconLabelContent(
		UWidgetTree* Tree,
		const FString& IconTexturePath,
		const FVector2D& IconSize,
		const FText& Label,
		const int32 LabelSize = 11)
	{
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		if (!IconTexturePath.IsEmpty())
		{
			if (UTexture2D* Texture = LoadTexture(*IconTexturePath))
			{
				USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				IconBox->SetWidthOverride(IconSize.X);
				IconBox->SetHeightOverride(IconSize.Y);
				UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass());
				Icon->SetBrush(MakeTextureBrush(*IconTexturePath, IconSize));
				IconBox->AddChild(Icon);
				if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(IconBox))
				{
					IconSlot->SetHorizontalAlignment(HAlign_Center);
					IconSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
		}
		if (!Label.IsEmpty())
		{
			UTextBlock* LabelText = MakeText(Tree, Label, LabelSize, Ink);
			LabelText->SetJustification(ETextJustify::Center);
			if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(LabelText))
			{
				LabelSlot->SetHorizontalAlignment(HAlign_Fill);
				LabelSlot->SetVerticalAlignment(VAlign_Bottom);
				LabelSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 3.0f));
			}
		}
		return Overlay;
	}

	void AddLockedCellIcon(
		UWidgetTree* Tree,
		UButton* Button,
		const FName IconName)
	{
		if (!Tree || !Button)
		{
			return;
		}
		UOverlay* Overlay = Cast<UOverlay>(Button->GetContent());
		if (!Overlay)
		{
			Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
			Button->SetContent(Overlay);
		}
		UImage* LockedIcon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
		LockedIcon->SetBrush(MakeTextureBrush(LockedIconTexturePath, FVector2D(28.0f, 28.0f)));
		LockedIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* LockSlot = Overlay->AddChildToOverlay(LockedIcon))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Right);
			LockSlot->SetVerticalAlignment(VAlign_Top);
			LockSlot->SetPadding(FMargin(3.0f));
		}
	}

	EGameXXKEquipmentSlot BackpackSlotFromIndex(const int32 SlotIndex)
	{
		switch (SlotIndex)
		{
		case 0: return EGameXXKEquipmentSlot::Weapon;
		case 1: return EGameXXKEquipmentSlot::Head;
		case 2: return EGameXXKEquipmentSlot::Armor;
		case 3: return EGameXXKEquipmentSlot::Belt;
		case 4: return EGameXXKEquipmentSlot::Shoes;
		case 5: return EGameXXKEquipmentSlot::Accessory;
		default: return EGameXXKEquipmentSlot::Invalid;
		}
	}

	FString BackpackCharacterDisplayName(
		const UGameXXKMVPSubsystem* Subsystem,
		const FName CharacterId)
	{
		if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
		{
			return TEXT("主角");
		}
		if (Subsystem)
		{
			FGameXXKPermanentCompanion Companion;
			if (Subsystem->TryGetPermanentCompanionView(CharacterId, Companion))
			{
				return FString::Printf(
					TEXT("%s Lv.%d"),
					*FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed),
					Companion.Level);
			}
		}
		return CharacterId.ToString();
	}

	FString QuestNpcDisplayName(const FName NpcId)
	{
		if (NpcId == TEXT("Npc.TusiChief")) return TEXT("土司首领");
		if (NpcId == TEXT("Npc.SongJinBao")) return TEXT("宋金宝");
		if (NpcId == TEXT("Npc.YueBai")) return TEXT("月白");
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return TEXT("周光祖");
		if (NpcId == TEXT("Npc.JinGui")) return TEXT("金贵");
		if (NpcId == TEXT("Npc.QiongMeiEr")) return TEXT("琼梅儿");
		return NpcId.ToString();
	}

	FString CharacterRosterPortraitPath(
		const UGameXXKMVPSubsystem* Subsystem,
		const FName CharacterId)
	{
		if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
		{
			return TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero");
		}
		if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId))
		{
			FString Token = CharacterId.ToString().Replace(TEXT("."), TEXT("_"));
			return FString::Printf(
				TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_%s.T_CardPortrait_%s"),
				*Token,
				*Token);
		}
		FGameXXKPermanentCompanion Companion;
		if (!Subsystem || !Subsystem->TryGetPermanentCompanionView(CharacterId, Companion))
		{
			return FString();
		}
		const TCHAR* RoleToken = nullptr;
		switch (Companion.Role)
		{
		case EGameXXKCharacterRole::Blade: RoleToken = TEXT("Blade"); break;
		case EGameXXKCharacterRole::Guard: RoleToken = TEXT("Guard"); break;
		case EGameXXKCharacterRole::Healer: RoleToken = TEXT("Healer"); break;
		case EGameXXKCharacterRole::Hunter: RoleToken = TEXT("Hunter"); break;
		case EGameXXKCharacterRole::Sorcerer: RoleToken = TEXT("Sorcerer"); break;
		case EGameXXKCharacterRole::FormationMaster: RoleToken = TEXT("FormationMaster"); break;
		default: break;
		}
		return RoleToken
			? FString::Printf(
				TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_%s.T_CardPortrait_Role_%s"),
				RoleToken,
				RoleToken)
			: FString();
	}

	UOverlay* MakeCharacterPortraitContent(
		UWidgetTree* Tree,
		const FString& PortraitPath,
		const FText& Label,
		const FVector2D& PortraitSize,
		const FName PortraitName = NAME_None,
		const FLinearColor& LabelColor = Ink)
	{
		UOverlay* Content = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		USizeBox* PortraitBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PortraitBox->SetWidthOverride(PortraitSize.X);
		PortraitBox->SetHeightOverride(PortraitSize.Y);
		UScaleBox* PortraitScale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		PortraitScale->SetStretch(EStretch::ScaleToFit);
		PortraitScale->SetStretchDirection(EStretchDirection::Both);
		UImage* Portrait = Tree->ConstructWidget<UImage>(UImage::StaticClass(), PortraitName);
		if (UTexture2D* Texture = PortraitPath.IsEmpty() ? nullptr : LoadTexture(*PortraitPath))
		{
			Portrait->SetBrushFromTexture(Texture, true);
			Portrait->SetColorAndOpacity(FLinearColor::White);
		}
		PortraitScale->SetContent(Portrait);
		PortraitBox->SetContent(PortraitScale);
		if (UOverlaySlot* PortraitSlot = Content->AddChildToOverlay(PortraitBox))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Center);
			PortraitSlot->SetVerticalAlignment(VAlign_Top);
			PortraitSlot->SetPadding(FMargin(4.0f, 3.0f, 4.0f, 19.0f));
		}
		UTextBlock* LabelText = MakeText(Tree, Label, 12, LabelColor);
		LabelText->SetAutoWrapText(false);
		LabelText->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* LabelSlot = Content->AddChildToOverlay(LabelText))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 2.0f));
		}
		return Content;
	}
}

void UGameXXKDesktopTrainingStageButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const FName InStageId)
{
	Owner = InOwner;
	StageId = InStageId;
	SetStyle(MakeImageButtonStyle(RouteNodeTexturePath, FVector2D(58.0f, 58.0f)));
	SetBackgroundColor(FLinearColor::White);
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingStageButton::HandleClicked);
}

void UGameXXKDesktopTrainingStageButton::HandleClicked()
{
	if (Owner)
	{
		FScopedActionCallbackGuard CallbackGuard(Owner->bInActionCallback);
		Owner->HandleStageClicked(StageId);
	}
}

void UGameXXKDesktopTrainingActionButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const int32 InActionId)
{
	Owner = InOwner;
	ActionId = InActionId;
	SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(150.0f, 54.0f), FMargin(0.08f)));
	SetBackgroundColor(FLinearColor::White);
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleClicked);
}

void UGameXXKDesktopTrainingActionButton::HandleClicked()
{
	if (Owner)
	{
		FScopedActionCallbackGuard CallbackGuard(Owner->bInActionCallback);
		const bool bLockableCell =
			(ActionId >= 100 && ActionId < 100 + WarehousePageSize)
			|| (ActionId >= 300 && ActionId < 300 + ToolSlotCount);
		if (bLockableCell
			&& FSlateApplication::IsInitialized()
			&& FSlateApplication::Get().GetModifierKeys().IsAltDown())
		{
			Owner->HandleActionAltClicked(ActionId);
			return;
		}
		Owner->HandleActionClicked(ActionId);
	}
}

bool UGameXXKDesktopTrainingActionButton::HandleRightMouseButtonDown()
{
	if (!Owner)
	{
		return false;
	}
	FScopedActionCallbackGuard CallbackGuard(Owner->bInActionCallback);
	return Owner->HandleActionRightClicked(ActionId);
}

TSharedRef<SWidget> UGameXXKDesktopTrainingActionButton::RebuildWidget()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyButton = SNew(SGameXXKDesktopTrainingActionButton, this)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
		.OnHovered_UObject(this, &ThisClass::SlateHandleHovered)
		.OnUnhovered_UObject(this, &ThisClass::SlateHandleUnhovered)
		.OnReceivedFocus_UObject(this, &ThisClass::SlateHandleOnReceivedFocus)
		.OnLostFocus_UObject(this, &ThisClass::SlateHandleOnLostFocus)
		.OnSlateButtonDragDetected(BIND_UOBJECT_DELEGATE(FOnDragDetected, SlateHandleDragDetected))
		.OnSlateButtonDragEnter(BIND_UOBJECT_DELEGATE(FOnDragEnter, SlateHandleDragEnter))
		.OnSlateButtonDragLeave(BIND_UOBJECT_DELEGATE(FOnDragLeave, SlateHandleDragLeave))
		.OnSlateButtonDragOver(BIND_UOBJECT_DELEGATE(FOnDragOver, SlateHandleDragOver))
		.OnSlateButtonDrop(BIND_UOBJECT_DELEGATE(FOnDrop, SlateHandleDrop))
		.ButtonStyle(&WidgetStyle)
		.ClickMethod(ClickMethod)
		.TouchMethod(TouchMethod)
		.PressMethod(PressMethod)
		.IsFocusable(IsFocusable)
		.AllowDragDrop(bAllowDragDrop);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (GetChildrenCount() > 0)
	{
		Cast<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
	}
	return MyButton.ToSharedRef();
}

TSharedRef<SWidget> UGameXXKDesktopTrainingWorkbenchWidget::RebuildWidget()
{
	if (!bInternalLayoutRebuild)
	{
		AbortTransientInventoryInteraction(true, false);
	}
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TravelVisualRuntime.Reset();
	if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		TravelVisualRuntime.Synchronize(Subsystem->GetTrainingTravelRuntimeCopy());
		if (!PersistenceBoundaryHandle.IsValid())
		{
			PersistenceBoundaryHandle = Subsystem->OnPersistenceBoundary().AddUObject(
				this,
				&UGameXXKDesktopTrainingWorkbenchWidget::HandlePersistenceBoundary);
		}
	}
	if (FSlateApplication::IsInitialized() && !ApplicationActivationHandle.IsValid())
	{
		ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
			this,
			&UGameXXKDesktopTrainingWorkbenchWidget::HandleApplicationActivationChanged);
	}
	if (SelectedStageId.IsNone())
	{
		SelectedStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bLayoutRefreshPending && !bLayoutRebuildScheduled && !bInActionCallback)
	{
		RebuildLayoutNow();
	}
	++TravelVisualNativeTickCount;
	UpdateCarriedItemVisualPosition();
	TGuardValue<bool> NativeTickGuard(bNativeTickActive, true);
	TickCollapsedResourceUnload(InDeltaTime);
	const UGameXXKMVPSubsystem* TravelSubsystem = ResolveMVPSubsystem();
	if (TravelSubsystem)
	{
		TravelVisualRuntime.Synchronize(TravelSubsystem->GetTrainingTravelRuntimeCopy());
	}
	TravelVisualRuntime.Tick(InDeltaTime);
	if (TravelAtlasCache)
	{
		TravelAtlasCache->AdvanceTimeouts(FPlatformTime::Seconds());
	}
	UpdateTravelVisuals();
	if (!TravelSubsystem || !TravelSubsystem->GetRuntimeState().Training.bTravelActive)
	{
		return;
	}

	// Logical travel advances on a one-second cadence, but the mutation is
	// captured at the end of this frame. Never retroactively consume the
	// frame's DeltaTime from the newly-created attack/hit/death presentation.
	TravelAccumulator += InDeltaTime;
	if (TravelAccumulator >= 1.0f)
	{
		const int32 ElapsedSeconds = FMath::Max(1, FMath::FloorToInt(TravelAccumulator));
		TravelAccumulator -= static_cast<float>(ElapsedSeconds);
		AdvanceTravelForTest(ElapsedSeconds);
	}
	UpdateTravelVisuals();
}

FReply UGameXXKDesktopTrainingWorkbenchWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& HandleWorkbenchRightMouseCancel())
	{
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	AbortTransientInventoryInteraction(false, true);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeDestruct()
{
	if (!bInternalLayoutRebuild)
	{
		AbortTransientInventoryInteraction(true, false);
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
			Subsystem && PersistenceBoundaryHandle.IsValid())
		{
			Subsystem->OnPersistenceBoundary().Remove(PersistenceBoundaryHandle);
		}
		PersistenceBoundaryHandle.Reset();
		if (FSlateApplication::IsInitialized() && ApplicationActivationHandle.IsValid())
		{
			FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
		}
		ApplicationActivationHandle.Reset();
		ReleaseTravelAtlasSession();
	}
	Super::NativeDestruct();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenWorkbench()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	if (!Subsystem->NormalizeDesktopInventoryState())
	{
		return false;
	}
	CancelCollapsedResourceUnload();
	bCollapsedResourcesReleased = false;
	bHasSavedEmbeddedInventorySession = false;
	AbortTransientInventoryInteraction(true, false);
	ToolSlots.SetNum(ToolSlotCount);
	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	SelectedStageId = Progress.SelectedStageId.IsNone() ? Progress.CurrentTravelStageId : Progress.SelectedStageId;
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	if (ActiveBackpackCharacterId.IsNone() || !CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		ActiveBackpackCharacterId = CharacterIds.Num() > 0
			? CharacterIds[0]
			: FGameXXKEquipmentRules::HeroCharacterId();
	}
	bSettingsPanelOpen = false;
	bBackpackExpanded = false;
	bWarehousePanelOpen = false;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	bExitConfirmationOpen = false;
	TravelAccumulator = 0.0f;
	TravelVisualRuntime.Reset();
	RefreshLayout();
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CloseWorkbench()
{
	const bool bWasVisible = IsInViewport() && GetVisibility() != ESlateVisibility::Collapsed;
	CancelCollapsedResourceUnload();
	bHasSavedEmbeddedInventorySession = false;
	AbortTransientInventoryInteraction(true, false);
	bSettingsPanelOpen = false;
	bBackpackExpanded = false;
	bWarehousePanelOpen = false;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	bExitConfirmationOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	return bWasVisible;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenBackpack()
{
	CancelCollapsedResourceUnload();
	CancelCarryForStructuralChange();
	ToolSlots.SetNum(ToolSlotCount);
	ActiveNav = EGameXXKDesktopTrainingNav::None;
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	bSettingsPanelOpen = false;
	bBackpackExpanded = true;
	bExitConfirmationOpen = false;
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	if (ActiveBackpackCharacterId.IsNone() || !CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		ActiveBackpackCharacterId = CharacterIds.Num() > 0
			? CharacterIds[0]
			: FGameXXKEquipmentRules::HeroCharacterId();
	}
	RefreshLayout();
	bCollapsedResourcesReleased = false;
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsBackpackExpandedForTest() const
{
	return bBackpackExpanded;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsWarehousePanelOpenForTest() const
{
	return bBackpackExpanded && bWarehousePanelOpen;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsRightPanelOpenForTest() const
{
	return bBackpackExpanded && RightPanel != EGameXXKDesktopTrainingRightPanel::None;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTopToolbarButtonCountForTest() const
{
	return bBackpackExpanded ? TopToolbarButtonCount : 0;
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTopToolbarAlwaysOnTopResourcePathForTest() const
{
	return FString(TruthTopToolbarAlwaysOnTopTexturePath);
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTopToolbarAlwaysOnTopOffResourcePathForTest() const
{
	return FString(TruthTopToolbarAlwaysOnTopOffTexturePath);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsAlwaysOnTopForTest() const
{
	return bAlwaysOnTop;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsMutedForTest() const
{
	return bMuted;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsExitConfirmationOpenForTest() const
{
	return bExitConfirmationOpen;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CancelExitForTest()
{
	if (!bExitConfirmationOpen)
	{
		return false;
	}
	bExitConfirmationOpen = false;
	RefreshLayout();
	return true;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetToolSlotCountForTest() const
{
	return ToolSlotCount;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetToolModeCountForTest() const
{
	return ToolModeCount;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetOccupiedToolSlotCountForTest() const
{
	int32 Count = 0;
	for (const FDesktopToolEntry& Entry : ToolSlots)
	{
		Count += Entry.IsValid() ? 1 : 0;
	}
	return Count;
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetToolSlotItemIdForTest(const int32 SlotIndex) const
{
	return ToolSlots.IsValidIndex(SlotIndex) && ToolSlots[SlotIndex].IsValid()
		? ToolSlots[SlotIndex].Entry.EntryId
		: NAME_None;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SetToolModeForTest(const EGameXXKDesktopToolMode Mode)
{
	if (static_cast<uint8>(Mode) >= ToolModeCount)
	{
		return false;
	}
	CancelCarryForStructuralChange();
	ActiveToolMode = Mode;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsCarryingItemForTest() const
{
	return CarriedEntry.IsValid();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HasDesktopCarriedEntry() const
{
	return CarriedEntry.IsValid();
}

FText UGameXXKDesktopTrainingWorkbenchWidget::GetLastDesktopInventoryNoticeForTest() const
{
	return LastNotice;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PickUpBackpackSlotForTest(const int32 SlotIndex)
{
	return PickUpDesktopEntry(EGameXXKDesktopItemContainer::Backpack, SlotIndex);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PickUpToolSlotForTest(const int32 SlotIndex)
{
	return PickUpToolEntry(SlotIndex);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CancelCarriedItemForTest()
{
	const bool bCancelled = CancelCarriedItem();
	if (bCancelled)
	{
		RefreshLayout();
	}
	return bCancelled;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::DropCarriedOnBackpackSlotForTest(const int32 SlotIndex)
{
	return DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer::Backpack, SlotIndex);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NotifyApplicationDeactivatedForTest()
{
	HandleApplicationActivationChanged(false);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CancelCarriedFromWorkbenchRightMouseForTest()
{
	return HandleWorkbenchRightMouseCancel();
}

void UGameXXKDesktopTrainingWorkbenchWidget::ForceExternalSlateRebuildForTest()
{
	RebuildWidget();
}

void UGameXXKDesktopTrainingWorkbenchWidget::DestructForTest()
{
	NativeDestruct();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RightClickBackpackSlotForTest(const int32 SlotIndex)
{
	return RouteBackpackRightClick(SlotIndex);
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::FindBackpackItemSlotForTest(const FName ItemId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		? FGameXXKDesktopInventoryRules::FindEntrySlot(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Backpack,
			FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId))
		: INDEX_NONE;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::FindFirstBackpackEquipmentSlotForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return INDEX_NONE;
	}
	for (int32 SlotIndex = 0; SlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity; ++SlotIndex)
	{
		if (FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Backpack,
			SlotIndex).bEquipmentInstance)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetActiveBackpackCharacterIdForTest() const
{
	return ActiveBackpackCharacterId.IsNone()
		? FGameXXKEquipmentRules::HeroCharacterId()
		: ActiveBackpackCharacterId;
}

EGameXXKDesktopTrainingNav UGameXXKDesktopTrainingWorkbenchWidget::GetActiveNavForTest() const
{
	return ActiveNav;
}

EGameXXKDesktopTrainingCenterPage UGameXXKDesktopTrainingWorkbenchWidget::GetActiveCenterPageForTest() const
{
	return ActiveCenterPage;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsToolsPanelActiveForTest() const
{
	return bBackpackExpanded
		&& RightPanel == EGameXXKDesktopTrainingRightPanel::Tools;
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetBackpackCharacterIdsForTest() const
{
	TArray<FName> CharacterIds;
	CharacterIds.Add(FGameXXKEquipmentRules::HeroCharacterId());
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return CharacterIds;
	}
	for (const FGameXXKPermanentCompanion& Companion : Subsystem->GetPermanentCompanionViews())
	{
		if (!Companion.InstanceId.IsNone() && !CharacterIds.Contains(Companion.InstanceId))
		{
			CharacterIds.Add(Companion.InstanceId);
		}
	}
	return CharacterIds;
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetCompanionCharacterIdsForTest() const
{
	TArray<FName> CharacterIds;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return CharacterIds;
	}
	for (const FGameXXKPermanentCompanion& Companion : Subsystem->GetPermanentCompanionViews())
	{
		if (!Companion.InstanceId.IsNone())
		{
			CharacterIds.AddUnique(Companion.InstanceId);
		}
	}
	return CharacterIds;
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetNpcCharacterIdsForTest() const
{
	TArray<FName> CharacterIds;
	for (const FGameXXKQuestNpcDefinition& Definition : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		if (!Definition.NpcId.IsNone())
		{
			CharacterIds.AddUnique(Definition.NpcId);
		}
	}
	return CharacterIds;
}

FName UGameXXKDesktopTrainingWorkbenchWidget::ResolveRosterRepresentativeCharacterId(
	const EGameXXKDesktopTrainingCharacterRoster Roster) const
{
	if (Roster == EGameXXKDesktopTrainingCharacterRoster::Hero)
	{
		return FGameXXKEquipmentRules::HeroCharacterId();
	}
	const TArray<FName> CharacterIds = Roster == EGameXXKDesktopTrainingCharacterRoster::Companions
		? GetCompanionCharacterIdsForTest()
		: GetNpcCharacterIdsForTest();
	if (CharacterIds.IsEmpty())
	{
		return NAME_None;
	}
	if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		const FGameXXKCardRunState& CardRun = Subsystem->GetRuntimeState().CardRun;
		const FName ActivePartyId = Roster == EGameXXKDesktopTrainingCharacterRoster::Companions
			? CardRun.PartySelection.ActivePermanentCompanionInstanceId
			: CardRun.ActiveTemporaryQuestNpcId;
		if (CharacterIds.Contains(ActivePartyId))
		{
			return ActivePartyId;
		}
	}
	if (CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		return ActiveBackpackCharacterId;
	}
	return CharacterIds[0];
}

void UGameXXKDesktopTrainingWorkbenchWidget::EnsureFormationCandidate()
{
	if (ActiveFormationRoster != EGameXXKDesktopTrainingCharacterRoster::Companions
		&& ActiveFormationRoster != EGameXXKDesktopTrainingCharacterRoster::Npcs)
	{
		ActiveFormationRoster = EGameXXKDesktopTrainingCharacterRoster::Companions;
	}
	const TArray<FName> CharacterIds = ActiveFormationRoster == EGameXXKDesktopTrainingCharacterRoster::Companions
		? GetCompanionCharacterIdsForTest()
		: GetNpcCharacterIdsForTest();
	if (CharacterIds.Contains(FormationCandidateCharacterId))
	{
		return;
	}
	FormationCandidateCharacterId = ResolveRosterRepresentativeCharacterId(ActiveFormationRoster);
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetEmbeddedBackpackCharacterIdForTest() const
{
	return EmbeddedInventoryWidget
		? EmbeddedInventoryWidget->GetConfiguredCharacterIdForTest()
		: GetActiveBackpackCharacterIdForTest();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectBackpackCharacterForTest(const FName CharacterId)
{
	CancelCarryForStructuralChange();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bHero = CharacterId == FGameXXKEquipmentRules::HeroCharacterId();
	const bool bCompanion = GetCompanionCharacterIdsForTest().Contains(CharacterId);
	const bool bNpc = GetNpcCharacterIdsForTest().Contains(CharacterId);
	if (!Subsystem || CharacterId.IsNone() || (!bHero && !bCompanion && !bNpc))
	{
		return false;
	}
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	if (!Subsystem->GetEquipmentLoadoutSnapshot(CharacterId, Snapshot))
	{
		return false;
	}
	ActiveBackpackCharacterId = CharacterId;
	ActiveCharacterRoster = bHero
		? EGameXXKDesktopTrainingCharacterRoster::Hero
		: bCompanion
			? EGameXXKDesktopTrainingCharacterRoster::Companions
			: EGameXXKDesktopTrainingCharacterRoster::Npcs;
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	ActiveNav = EGameXXKDesktopTrainingNav::None;
	bSettingsPanelOpen = false;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectFormationCandidateForTest(const FName CharacterId)
{
	CancelCarryForStructuralChange();
	const bool bCompanion = GetCompanionCharacterIdsForTest().Contains(CharacterId);
	const bool bNpc = GetNpcCharacterIdsForTest().Contains(CharacterId);
	if (CharacterId.IsNone() || (!bCompanion && !bNpc))
	{
		return false;
	}
	ActiveFormationRoster = bCompanion
		? EGameXXKDesktopTrainingCharacterRoster::Companions
		: EGameXXKDesktopTrainingCharacterRoster::Npcs;
	FormationCandidateCharacterId = CharacterId;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ApplyFormationCandidateForTest()
{
	CancelCarryForStructuralChange();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || FormationCandidateCharacterId.IsNone())
	{
		return false;
	}
	const bool bCompanion = GetCompanionCharacterIdsForTest().Contains(FormationCandidateCharacterId);
	const bool bNpc = GetNpcCharacterIdsForTest().Contains(FormationCandidateCharacterId);
	const bool bApplied = bCompanion
		? Subsystem->SetActivePermanentCompanion(FormationCandidateCharacterId)
		: bNpc && Subsystem->SelectTownQuestNpcForParty(FormationCandidateCharacterId);
	if (!bApplied)
	{
		SetNotice(FText::FromString(TEXT("当前状态不能修改编队；请回到可编辑的城镇状态")));
		RefreshLayout();
		return false;
	}
	const FString Name = bNpc
		? QuestNpcDisplayName(FormationCandidateCharacterId)
		: BackpackCharacterDisplayName(Subsystem, FormationCandidateCharacterId);
	SetNotice(FText::FromString(FString::Printf(TEXT("%s 已编入队伍"), *Name)));
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsWorkbenchVisibleForTest() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsSettingsPanelOpenForTest() const
{
	return bSettingsPanelOpen
		&& ActiveCenterPage != EGameXXKDesktopTrainingCenterPage::Talents;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehouseColumnCountForTest() const
{
	return WarehouseColumns;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehouseRowCountForTest() const
{
	return WarehouseRows;
}

TArray<FString> UGameXXKDesktopTrainingWorkbenchWidget::GetMasterV2ResourcePathsForTest() const
{
	const TCHAR* RequiredPaths[] = {
		PanelLargeTexturePath,
		ItemSlotTexturePath,
		EquipmentSlotTexturePath,
		HeroFullBodyTexturePath,
		CloseInkTexturePath,
		IngotTexturePath,
		CharacterTabNormalTexturePath,
		CharacterTabSelectedTexturePath,
		RouteNodeTexturePath};
	TArray<FString> LoadedPaths;
	for (const TCHAR* Path : RequiredPaths)
	{
		if (const UTexture2D* Texture = LoadTexture(Path))
		{
			LoadedPaths.Add(Texture->GetPathName());
		}
	}
	return LoadedPaths;
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetBottomNavigationIconResourcePathForTest(
	const EGameXXKDesktopTrainingNav Nav) const
{
	return FString(BottomNavigationIconTexturePath(Nav));
}

TArray<FString> UGameXXKDesktopTrainingWorkbenchWidget::GetBottomNavigationIconResourcePathsForTest() const
{
	const EGameXXKDesktopTrainingNav Navs[] = {
		EGameXXKDesktopTrainingNav::Warehouse,
		EGameXXKDesktopTrainingNav::Formation,
		EGameXXKDesktopTrainingNav::Talents,
		EGameXXKDesktopTrainingNav::Tools,
		EGameXXKDesktopTrainingNav::Training};
	TArray<FString> Paths;
	Paths.Reserve(UE_ARRAY_COUNT(Navs));
	for (const EGameXXKDesktopTrainingNav Nav : Navs)
	{
		Paths.Add(GetBottomNavigationIconResourcePathForTest(Nav));
	}
	return Paths;
}

FVector2D UGameXXKDesktopTrainingWorkbenchWidget::GetBackpackAspectRatioForTest() const
{
	return BackpackAspectRatio;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetRuntimeGoldForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetRuntimeState().PlayerGold : 0;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetPendingTravelGoldForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetPendingTrainingTravelRewardCopy().Gold : 0;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetPendingTravelNormalChestCountForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetPendingTrainingTravelRewardCopy().NormalChestCount : 0;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetPendingTravelAdvancedChestCountForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetPendingTrainingTravelRewardCopy().AdvancedChestCount : 0;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CollectTravelRewardsForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	FGameXXKTrainingOfflineReward CollectedReward;
	if (!Subsystem->CollectTrainingTravelRewards(CollectedReward))
	{
		return false;
	}
	SetNotice(FText::FromString(FString::Printf(
		TEXT("收菜完成：+%d 金币 / +%d 经验 · 普通箱 %d · 精英箱 %d"),
		CollectedReward.Gold,
		CollectedReward.Experience,
		CollectedReward.NormalChestCount,
		CollectedReward.AdvancedChestCount)));
	RefreshLayout();
	return true;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehouseOccupancyForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		? FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse)
		: 0;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehousePageCountForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 LastOccupiedSlot = Subsystem
		? FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse)
		: INDEX_NONE;
	return FMath::Max(1, FMath::DivideAndRoundUp(LastOccupiedSlot + 1, WarehousePageSize));
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehousePageIndexForTest() const
{
	return FMath::Clamp(WarehousePageIndex, 0, GetWarehousePageCountForTest() - 1);
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetVisibleWarehouseInstanceIdsForTest() const
{
	TArray<FName> Visible;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Visible;
	}
	const int32 PageStart = GetWarehousePageIndexForTest() * WarehousePageSize;
	for (int32 Index = PageStart; Index < PageStart + WarehousePageSize; ++Index)
	{
		const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			Index);
		if (Entry.bEquipmentInstance && !ShouldHideDesktopInventoryEntry(EGameXXKDesktopItemContainer::Warehouse, Entry))
		{
			Visible.Add(Entry.EntryId);
		}
	}
	return Visible;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::NextWarehousePageForTest()
{
	const int32 CurrentPage = GetWarehousePageIndexForTest();
	const int32 LastPage = GetWarehousePageCountForTest() - 1;
	if (CurrentPage >= LastPage)
	{
		return false;
	}
	WarehousePageIndex = CurrentPage + 1;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PreviousWarehousePageForTest()
{
	const int32 CurrentPage = GetWarehousePageIndexForTest();
	if (CurrentPage <= 0)
	{
		return false;
	}
	WarehousePageIndex = CurrentPage - 1;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::QuickEquipVisibleWarehouseSlotForTest(const int32 VisibleSlotIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + VisibleSlotIndex;
	const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Warehouse,
		PhysicalSlotIndex);
	if (!Entry.bEquipmentInstance)
	{
		return false;
	}
	const int32 BackpackSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack);
	FString MoveError;
	if (BackpackSlot == INDEX_NONE
		|| !Subsystem->MoveDesktopInventoryEntry(
			EGameXXKDesktopItemContainer::Warehouse,
			PhysicalSlotIndex,
			EGameXXKDesktopItemContainer::Backpack,
			BackpackSlot,
			&MoveError))
	{
		return false;
	}
	FGameXXKCharacterBackpackModel BackpackModel;
	BackpackModel.Bind(Subsystem, GetActiveBackpackCharacterIdForTest());
	FGameXXKEquipmentTransactionResult Result;
	const bool bEquipped = BackpackModel.QuickEquip(Entry.EntryId, Result);
	if (bEquipped)
	{
		Subsystem->NormalizeDesktopInventoryState();
		SetNotice(Result.Message.IsEmpty() ? FText::FromString(TEXT("装备已转入当前角色")) : Result.Message);
		RefreshLayout();
	}
	return bEquipped;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SortWarehouseForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->NormalizeDesktopInventoryState())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	TArray<FGameXXKDesktopInventoryEntryKey> Entries;
	for (const FGameXXKDesktopInventoryEntryKey& Entry : State.DesktopInventory.WarehouseSlots)
	{
		if (Entry.IsValid())
		{
			Entries.Add(Entry);
		}
	}
	Entries.Sort([](const FGameXXKDesktopInventoryEntryKey& Left, const FGameXXKDesktopInventoryEntryKey& Right)
	{
		if (Left.bEquipmentInstance != Right.bEquipmentInstance)
		{
			return Left.bEquipmentInstance;
		}
		return Left.EntryId.LexicalLess(Right.EntryId);
	});
	State.DesktopInventory.WarehouseSlots.Init(FGameXXKDesktopInventoryEntryKey(), FGameXXKDesktopInventoryRules::WarehouseCapacity);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		State.DesktopInventory.WarehouseSlots[Index] = Entries[Index];
	}
	WarehousePageIndex = 0;
	SetNotice(FText::FromString(TEXT("仓库已排序：槽位 → 品质 → 等级")));
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::QuickUnequipActiveBackpackSlotForTest(const int32 SlotIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const EGameXXKEquipmentSlot EquipmentSlot = BackpackSlotFromIndex(SlotIndex);
	if (EquipmentSlot == EGameXXKEquipmentSlot::Invalid)
	{
		return false;
	}
	FGameXXKCharacterBackpackModel BackpackModel;
	BackpackModel.Bind(Subsystem, GetActiveBackpackCharacterIdForTest());
	FGameXXKEquipmentTransactionResult Result;
	const bool bUnequipped = BackpackModel.QuickUnequip(EquipmentSlot, Result);
	if (bUnequipped)
	{
		Subsystem->NormalizeDesktopInventoryState();
		SetNotice(Result.Message.IsEmpty() ? FText::FromString(TEXT("装备已卸下并返回背包")) : Result.Message);
		RefreshLayout();
	}
	return bUnequipped;
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetVisibleBackpackItemIdsForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? SortedVisibleInventoryItemIds(Subsystem->GetRuntimeState()) : TArray<FName>();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTrainingStageButtonCountForTest() const
{
	return StageButtons.Num();
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetSelectedStageIdForTest() const
{
	return SelectedStageId;
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetCurrentTravelStageIdForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetTrainingProgressCopy().CurrentTravelStageId : NAME_None;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HasTravelVisualStripForTest() const
{
	return TravelVisualViewport != nullptr
		&& TravelBackgroundImages.Num() == 3
		&& TravelEnemyImages.Num() == 3
		&& TravelHeroImage != nullptr
		&& TravelEnemyHealthBars.Num() == 3
		&& TravelCompanionHealthBars.Num() == 2
		&& TravelHeroHealth != nullptr;
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualScrollOffsetForTest() const
{
	return TravelVisualRuntime.GetScrollOffset();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualWalkFrameForTest() const
{
	return TravelVisualRuntime.GetWalkFrameIndex();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualCompletedLoopCountForTest() const
{
	return TravelVisualRuntime.GetCompletedLoopCount();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualNativeTickCountForTest() const
{
	return TravelVisualNativeTickCount;
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualAtlasResourcePathForTest() const
{
	return TravelHeroAtlasTexturePath;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AreTravelCombatAtlasesOneKForTest() const
{
	if (TravelRequestedAtlasPaths.IsEmpty())
	{
		return false;
	}
	for (const FSoftObjectPath& Path : TravelRequestedAtlasPaths)
	{
		if (!Path.ToString().Contains(TEXT("_1k_"), ESearchCase::IgnoreCase))
		{
			return false;
		}
	}
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKDesktopTrainingWorkbenchWidget::SetTravelAtlasCacheForTest(
	TUniquePtr<FGameXXKBattleAtlasCache> InAtlasCache)
{
	ReleaseTravelAtlasSession();
	if (TravelAtlasCache)
	{
		TravelAtlasCache->Clear();
	}
	TravelAtlasCache = MoveTemp(InAtlasCache);
}

FSoftObjectPath UGameXXKDesktopTrainingWorkbenchWidget::GetTravelAppliedCompanionAtlasPathForTest(
	const int32 CompanionIndex) const
{
	return TravelAppliedCompanionAtlasPaths.IsValidIndex(CompanionIndex)
		? TravelAppliedCompanionAtlasPaths[CompanionIndex]
		: FSoftObjectPath();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelAppliedCompanionFrameForTest(
	const int32 CompanionIndex) const
{
	return TravelAppliedCompanionFrames.IsValidIndex(CompanionIndex)
		? TravelAppliedCompanionFrames[CompanionIndex]
		: INDEX_NONE;
}
#endif

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualBackgroundResourcePathForTest() const
{
	return TravelBackgroundTexturePath;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelBackgroundTileCountForTest() const
{
	return TravelBackgroundImages.Num();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVerboseTextBlockCountForTest() const
{
	// Stage/reward detail is exposed through the map and compact button tooltips;
	// the continuously animated strip owns no diagnostic prose.
	return 0;
}

EGameXXKTrainingTravelVisualPhase UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualPhaseForTest() const
{
	return TravelVisualRuntime.GetVisualPhase();
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualPhaseNameForTest() const
{
	return TravelVisualPhaseName(TravelVisualRuntime.GetVisualPhase());
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelLogicalPhaseNameForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return TrainingTravelPhaseName(Subsystem
		? Subsystem->GetTrainingTravelRuntimeCopy().Phase
		: EGameXXKTrainingTravelPhase::Idle);
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualHeroActionNameForTest() const
{
	return BattleAnimationActionName(TravelVisualRuntime.GetHeroAction());
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualPartyActionNameForTest(const int32 PartyIndex) const
{
	return BattleAnimationActionName(TravelVisualRuntime.GetPartyAction(PartyIndex));
}

FString UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualEnemyActionNameForTest() const
{
	return BattleAnimationActionName(TravelVisualRuntime.GetEnemyAction());
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsTravelVisualEnemyVisibleForTest() const
{
	return TravelVisualRuntime.IsEnemyVisible();
}

FName UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualEnemyDefinitionIdForTest() const
{
	return TravelVisualRuntime.GetEnemyDefinitionId();
}

EGameXXKBattleAnimationAction UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualHeroActionForTest() const
{
	return TravelVisualRuntime.GetHeroAction();
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualEnemyHealthFractionForTest() const
{
	return TravelVisualRuntime.GetEnemyHealthFraction();
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualHeroHealthFractionForTest() const
{
	return TravelVisualRuntime.GetHeroHealthFraction();
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualPartyHealthFractionForTest(const int32 PartyIndex) const
{
	return TravelVisualRuntime.GetPartyHealthFraction(PartyIndex);
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualScrollVelocityForTest() const
{
	return TravelVisualRuntime.GetScrollVelocity();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualHeroRenderedFrameForTest() const
{
	return TravelAppliedHeroFrame;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelVisualEnemyRenderedFrameForTest() const
{
	const int32 PresentedSlotIndex = TravelVisualRuntime.GetPresentedEnemySlotIndex();
	return TravelAppliedEnemyFrames.IsValidIndex(PresentedSlotIndex)
		? TravelAppliedEnemyFrames[PresentedSlotIndex]
		: INDEX_NONE;
}

FText UGameXXKDesktopTrainingWorkbenchWidget::GetStageTooltipForTest(const FName StageId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->BuildTrainingStageTooltip(StageId) : FText::GetEmpty();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectStageForTest(const FName StageId)
{
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FGameXXKTrainingStageDefinition Definition;
	if (!Subsystem || !FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
	{
		return false;
	}
	SelectedStageId = StageId;
	return Subsystem->SelectTrainingStage(StageId);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickChallengeForTest()
{
	ApplyAction(6);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->IsTrainingChallengeBattleActive();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickTravelForTest()
{
	ApplyAction(7);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->GetRuntimeState().Training.bTravelActive;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AdvanceTravelForTest(const int32 ElapsedSeconds)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const FGameXXKTrainingTravelRuntime Before = Subsystem->GetTrainingTravelRuntimeCopy();
	bool bEncounterCompleted = false;
	bool bCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	if (!Subsystem->AdvanceTrainingTravelStep(bEncounterCompleted, bCompleted, bDefeated, Reward, FMath::Max(1, ElapsedSeconds)))
	{
		return false;
	}
	const FGameXXKTrainingTravelRuntime AfterStep = Subsystem->GetTrainingTravelRuntimeCopy();
	TravelVisualRuntime.NotifyTravelStep(Before, AfterStep, bEncounterCompleted, bCompleted, bDefeated);
	if (bDefeated)
	{
		Subsystem->ResolveTrainingTravelFailure();
	}
	TravelVisualRuntime.Synchronize(Subsystem->GetTrainingTravelRuntimeCopy());
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SetRetryOnFailureForTest(const bool bEnabled)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->SetTrainingRetryOnFailure(bEnabled);
}

void UGameXXKDesktopTrainingWorkbenchWidget::TickForTest(const float InDeltaTime)
{
	NativeTick(FGeometry(), FMath::Max(0.0f, InDeltaTime));
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HasPendingLayoutRefreshForTest() const
{
	return bLayoutRefreshPending;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ConstructForTest()
{
	NativeConstruct();
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetProgrammaticLayoutBuildCountForTest() const
{
	return ProgrammaticLayoutBuildCount;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsCollapsedResourceUnloadPendingForTest() const
{
	return bCollapsedResourceUnloadPending;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AreCollapsedResourcesReleasedForTest() const
{
	return bCollapsedResourcesReleased;
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetCollapsedResourceUnloadRemainingSecondsForTest() const
{
	return CollapsedResourceUnloadRemainingSeconds;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetCollapsedGcRequestCountForTest() const
{
	return CollapsedGcRequestCount;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetEmbeddedInventoryWidgetCountForTest() const
{
	return EmbeddedInventoryWidget ? 1 : 0;
}

EGameXXKCharacterBackpackTab UGameXXKDesktopTrainingWorkbenchWidget::GetEmbeddedBackpackTabForTest() const
{
	return EmbeddedInventoryWidget
		? EmbeddedInventoryWidget->GetActiveCharacterBackpackTabForTest()
		: EGameXXKCharacterBackpackTab::Equipment;
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetEmbeddedPendingDeckIdsForTest() const
{
	return EmbeddedInventoryWidget
		? EmbeddedInventoryWidget->GetPendingHeroDeckIdsForTest()
		: TArray<FName>();
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleStageClicked(const FName StageId)
{
	SelectStageForTest(StageId);
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleActionClicked(const int32 ActionId)
{
	ApplyAction(ActionId);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleActionAltClicked(const int32 ActionId)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (ActionId >= 100 && ActionId < 100 + WarehousePageSize)
	{
		const int32 PhysicalSlotIndex =
			GetWarehousePageIndexForTest() * WarehousePageSize + (ActionId - 100);
		return HandleDesktopSlotAltClicked(
			EGameXXKDesktopItemContainer::Warehouse,
			PhysicalSlotIndex);
	}
	if (ActionId >= 300 && ActionId < 300 + ToolSlotCount)
	{
		return HandleDesktopToolSlotAltClicked(ActionId - 300);
	}
	return false;
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DesktopTrainingWorkbenchWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}
	if (!RootScaleBox)
	{
		RootScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("DesktopTrainingScaleRoot"));
	}
	if (!ReferenceCanvasBox)
	{
		ReferenceCanvasBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesktopTrainingReferenceBox"));
	}
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesktopTrainingReferenceCanvas"));
	}
	if (!RootScaleBox || !ReferenceCanvasBox || !RootCanvas)
	{
		return;
	}
	RootScaleBox->SetStretch(EStretch::ScaleToFit);
	RootScaleBox->SetStretchDirection(EStretchDirection::Both);
	const FVector2D ReferenceCanvasSize = GameXXKDesktopTrainingLayout::GetReferenceCanvasSize();
	ReferenceCanvasBox->SetWidthOverride(ReferenceCanvasSize.X);
	ReferenceCanvasBox->SetHeightOverride(ReferenceCanvasSize.Y);
	if (ReferenceCanvasBox->GetContent() != RootCanvas)
	{
		ReferenceCanvasBox->SetContent(RootCanvas);
	}
	if (RootScaleBox->GetContent() != ReferenceCanvasBox)
	{
		RootScaleBox->SetContent(ReferenceCanvasBox);
	}
	++ProgrammaticLayoutBuildCount;
	WidgetTree->RootWidget = RootScaleBox;
	TravelVisualViewport = nullptr;
	TravelBackgroundImageA = nullptr;
	TravelBackgroundImageB = nullptr;
	TravelBackgroundImages.Reset();
	TravelEnemyImages.Reset();
	TravelHeroImage = nullptr;
	TravelCompanionImages.Reset();
	TravelEnemyHealthBars.Reset();
	TravelHeroHealth = nullptr;
	TravelCompanionHealthBars.Reset();
	EmbeddedInventoryWidget = nullptr;
	CarriedItemImage = nullptr;
	TravelHeroAtlasTexture = nullptr;
	TravelBackgroundTexture = nullptr;
	TravelHeroFallbackTextures.Reset();
	TravelAppliedHeroAtlasPath.Reset();
	TravelAppliedEnemyAtlasPaths.Reset();
	TravelAppliedCompanionAtlasPaths.Reset();
	TravelAppliedHeroFrame = INDEX_NONE;
	TravelAppliedEnemyFrames.Reset();
	TravelAppliedCompanionFrames.Reset();
	TravelAppliedHeroHealth = -1.0f;
	TravelAppliedEnemyHealth.Reset();
	TravelAppliedCompanionHealth.Reset();
	RootCanvas->ClearChildren();
	StageButtons.Reset();
	ActionButtons.Reset();
	BuildWorkbenchShell();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWorkbenchShell()
{
	AddCanvas(RootCanvas, MakeTransparentPanel(WidgetTree, TEXT("WorkbenchBackground")), FVector2D::ZeroVector, GameXXKDesktopTrainingLayout::GetReferenceCanvasSize());
	AddCanvasRect(
		RootCanvas,
		MakeTransparentPanel(WidgetTree, TEXT("CenterWorkbenchFrame")),
		GameXXKDesktopTrainingLayout::GetCenterShellRect());
	BuildTopIdleStrip();
	BuildBackpackTabToggle();
	if (bBackpackExpanded)
	{
		if (bWarehousePanelOpen)
		{
			BuildWarehousePanel();
		}
		if (ActiveCenterPage == EGameXXKDesktopTrainingCenterPage::Talents)
		{
			BuildTalentsPanel();
		}
		else if (ActiveCenterPage == EGameXXKDesktopTrainingCenterPage::Formation)
		{
			BuildFormationPanel();
		}
		else
		{
			BuildBackpackPanel();
		}
		if (RightPanel == EGameXXKDesktopTrainingRightPanel::Tools)
		{
			BuildToolsPanel();
		}
		else if (RightPanel == EGameXXKDesktopTrainingRightPanel::TrainingMap)
		{
			BuildTrainingMapPanel();
		}
		BuildTopToolbar();
		BuildBottomNavigation();
	}
	if (bExitConfirmationOpen)
	{
		BuildExitConfirmation();
	}
	if (CarriedEntry.IsValid())
	{
		BuildCarriedItemVisual();
	}
	NoticePanel = MakePanel(
		WidgetTree,
		FLinearColor(0.08f, 0.05f, 0.03f, 0.96f),
		TEXT("DesktopInventoryNoticePanel"));
	NoticeText = MakeText(
		WidgetTree,
		LastNotice,
		16,
		Gold,
		TEXT("DesktopInventoryNoticeText"));
	NoticePanel->SetContent(NoticeText);
	NoticePanel->SetVisibility(
		LastNotice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	AddCanvas(RootCanvas, NoticePanel.Get(), FVector2D(397.0f, 226.0f), FVector2D(420.0f, 28.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBackpackTabToggle()
{
	UGameXXKDesktopTrainingActionButton* Toggle = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("BackpackTabToggleButton"));
	Toggle->Configure(this, 60);
	Toggle->SetStyle(MakeTextureButtonStyle(
		bBackpackExpanded ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
		FVector2D(68.0f, 44.0f),
		FMargin(0.08f)));
	Toggle->SetBackgroundColor(FLinearColor::White);
	Toggle->SetContent(MakeButtonText(
		WidgetTree,
		FText::FromString(bBackpackExpanded ? TEXT("▲") : TEXT("▼")),
		24,
		Ink));
	Toggle->SetToolTipText(FText::FromString(
		bBackpackExpanded
			? TEXT("关闭背包与全部子界面；历练挂机继续运行")
			: TEXT("菜单 [Tab]：展开角色背包")));
	AddCanvas(RootCanvas, Toggle, FVector2D(1225.0f, 30.0f), FVector2D(68.0f, 44.0f));
	ActionButtons.Add(Toggle);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildPanelCloseButton(
	const FName WidgetName,
	const int32 ActionId,
	const FVector2D Position)
{
	UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		WidgetName);
	Button->Configure(this, ActionId);
	Button->SetStyle(MakeImageButtonStyle(CloseInkTexturePath, FVector2D(44.0f, 44.0f)));
	Button->SetBackgroundColor(FLinearColor::White);
	AddCanvas(RootCanvas, Button, Position, FVector2D(44.0f, 44.0f));
	ActionButtons.Add(Button);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTopToolbar()
{
	struct FToolbarSpec
	{
		int32 ActionId;
		const TCHAR* Name;
		const TCHAR* Label;
		const TCHAR* Tooltip;
	};
	const FToolbarSpec Specs[] = {
		{14, TEXT("TopToolbarAlwaysOnTop"), bAlwaysOnTop ? TEXT("置") : TEXT("顶"), TEXT("窗口始终置于桌面最顶层")},
		{17, TEXT("TopToolbarMute"), bMuted ? TEXT("静") : TEXT("声"), TEXT("静音 / 恢复声音")},
		{18, TEXT("TopToolbarMail"), TEXT("信"), TEXT("邮件")},
		{19, TEXT("TopToolbarShop"), TEXT("店"), TEXT("商店")},
		{15, TEXT("TopToolbarExit"), TEXT("退"), TEXT("退出游戏（需要再次确认）")}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Specs); ++Index)
	{
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			Specs[Index].Name);
		Button->Configure(this, Specs[Index].ActionId);
		Button->SetStyle(MakeTextureButtonStyle(
			Specs[Index].ActionId == 15 ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(42.0f, 36.0f),
			FMargin(0.08f)));
		Button->SetBackgroundColor(FLinearColor::White);
		if (Index == 0)
		{
			UImage* PinIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TopToolbarAlwaysOnTopIcon"));
			const TCHAR* PinTexturePath = bAlwaysOnTop
				? TruthTopToolbarAlwaysOnTopTexturePath
				: TruthTopToolbarAlwaysOnTopOffTexturePath;
			PinIcon->SetBrush(MakeTextureBrush(PinTexturePath, FVector2D(52.0f, 52.0f)));
			UScaleBox* PinScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TopToolbarAlwaysOnTopIconScale"));
			PinScale->SetStretch(EStretch::ScaleToFit);
			PinScale->SetStretchDirection(EStretchDirection::Both);
			PinScale->SetContent(PinIcon);
			Button->SetContent(PinScale);
		}
		else
		{
			Button->SetContent(MakeButtonText(WidgetTree, FText::FromString(Specs[Index].Label), 15, Ink));
		}
		Button->SetToolTipText(FText::FromString(Specs[Index].Tooltip));
		AddCanvas(RootCanvas, Button, FVector2D(1092.0f + Index * 47.0f, 226.0f), FVector2D(42.0f, 36.0f));
		ActionButtons.Add(Button);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildExitConfirmation()
{
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ExitGameBackdrop"));
	FSlateBrush BackdropBrush;
	BackdropBrush.DrawAs = ESlateBrushDrawType::Box;
	BackdropBrush.TintColor = FSlateColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.72f));
	Backdrop->SetBrush(BackdropBrush);
	Backdrop->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.01f, 0.72f));
	AddCanvas(RootCanvas, Backdrop, FVector2D::ZeroVector, GameXXKDesktopTrainingLayout::GetReferenceCanvasSize());

	UBorder* Frame = MakePanel(WidgetTree, PanelAlt, TEXT("ExitGameConfirmation"));
	AddCanvas(RootCanvas, Frame, FVector2D(690.0f, 360.0f), FVector2D(360.0f, 210.0f));
	UTextBlock* Prompt = MakeText(
		WidgetTree,
		FText::FromString(TEXT("退出游戏？\n历练挂机会在关闭游戏后按离线规则结算。")),
		21,
		Ink);
	Prompt->SetJustification(ETextJustify::Center);
	AddCanvas(RootCanvas, Prompt, FVector2D(725.0f, 395.0f), FVector2D(290.0f, 82.0f));
	UGameXXKDesktopTrainingActionButton* Cancel = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("ExitGameCancelButton"));
	Cancel->Configure(this, 53);
	Cancel->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(116.0f, 48.0f), FMargin(0.08f)));
	Cancel->SetBackgroundColor(FLinearColor::White);
	Cancel->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("取消")), 18, Ink));
	AddCanvas(RootCanvas, Cancel, FVector2D(725.0f, 495.0f), FVector2D(116.0f, 48.0f));
	ActionButtons.Add(Cancel);
	UGameXXKDesktopTrainingActionButton* Confirm = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("ExitGameConfirmButton"));
	Confirm->Configure(this, 54);
	Confirm->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(116.0f, 48.0f), FMargin(0.08f)));
	Confirm->SetBackgroundColor(FLinearColor::White);
	Confirm->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("退出")), 18, Ink));
	AddCanvas(RootCanvas, Confirm, FVector2D(899.0f, 495.0f), FVector2D(116.0f, 48.0f));
	ActionButtons.Add(Confirm);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildCarriedItemVisual()
{
	if (!CarriedEntry.IsValid())
	{
		return;
	}
	CarriedItemImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DesktopCarriedItemImage"));
	if (!CarriedEntry.Payload.IconPath.IsEmpty())
	{
		CarriedItemImage->SetBrush(MakeTextureBrush(*CarriedEntry.Payload.IconPath, FVector2D(56.0f, 56.0f)));
	}
	else
	{
		CarriedItemImage->SetBrush(MakeTextureBrush(ItemSlotTexturePath, FVector2D(56.0f, 56.0f)));
	}
	CarriedItemImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.92f));
	CarriedItemImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	CarriedItemImage->SetIsEnabled(false);
	AddCanvas(RootCanvas, CarriedItemImage.Get(), FVector2D(800.0f, 470.0f), FVector2D(56.0f, 56.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTopIdleStrip()
{
	UBorder* Strip = MakeTransparentPanel(WidgetTree, TEXT("TrainingTravelStrip"));
	TravelVisualViewport = Strip;
	AddCanvasRect(RootCanvas, Strip, GameXXKDesktopTrainingLayout::GetIdleStripRect());
	UCanvasPanel* TravelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TravelVisualCanvas"));
	if (TravelCanvas)
	{
		TravelCanvas->SetClipping(EWidgetClipping::ClipToBounds);
		Strip->SetContent(TravelCanvas);
		TravelBackgroundTexture = LoadTexture(TravelBackgroundTexturePath);
		if (!TravelBackgroundTexture)
		{
			TravelBackgroundTexture = LoadTexture(TravelBackgroundFallbackTexturePath);
		}
		FSlateBrush BackgroundBrush;
		if (TravelBackgroundTexture)
		{
			BackgroundBrush.SetResourceObject(TravelBackgroundTexture);
			BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
			BackgroundBrush.ImageSize = TravelBackgroundImageSize;
		}
		for (int32 TileIndex = 0; TileIndex < 3; ++TileIndex)
		{
			UImage* Tile = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				*FString::Printf(TEXT("TravelBackgroundTile_%d"), TileIndex));
			if (!Tile)
			{
				continue;
			}
			if (TravelBackgroundTexture)
			{
				Tile->SetBrush(BackgroundBrush);
			}
			else
			{
				Tile->SetColorAndOpacity(FLinearColor::Transparent);
			}
			UCanvasPanelSlot* TileSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(Tile));
			if (TileSlot)
			{
				TileSlot->SetPosition(FVector2D((TileIndex - 1) * TravelBackgroundImageSize.X, 0.0f));
				TileSlot->SetSize(TravelBackgroundImageSize);
				TileSlot->SetZOrder(0);
			}
			TravelBackgroundImages.Add(Tile);
			if (TileIndex == 0)
			{
				TravelBackgroundImageA = Tile;
			}
			else if (TileIndex == 1)
			{
				TravelBackgroundImageB = Tile;
			}
		}

		TravelHeroAtlasTexture = LoadTexture(TravelHeroAtlasTexturePath);
		TravelHeroFallbackTextures.Reset();
		for (const TCHAR* FallbackPath : TravelHeroFallbackTexturePaths)
		{
			TravelHeroFallbackTextures.Add(LoadTexture(FallbackPath));
		}
		for (int32 EnemySlotIndex = 0; EnemySlotIndex < 3; ++EnemySlotIndex)
		{
			UImage* EnemyImage = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				*FString::Printf(TEXT("TravelEnemyAnimatedUnit_%d"), EnemySlotIndex));
			if (!EnemyImage)
			{
				continue;
			}
			EnemyImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			EnemyImage->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* EnemySlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(EnemyImage));
			if (EnemySlot)
			{
				EnemySlot->SetPosition(FVector2D(20.0f + EnemySlotIndex * 125.0f, 23.0f));
				EnemySlot->SetSize(TravelCombatVisualSize);
				EnemySlot->SetZOrder(2);
			}
			TravelEnemyImages.Add(EnemyImage);
			TravelAppliedEnemyAtlasPaths.Add(FSoftObjectPath());
			TravelAppliedEnemyFrames.Add(INDEX_NONE);
			TravelAppliedEnemyHealth.Add(-1.0f);
		}

		TravelHeroImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TravelHeroAnimatedUnit"));
		if (TravelHeroImage)
		{
			TravelHeroImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			UCanvasPanelSlot* HeroSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(TravelHeroImage));
			if (HeroSlot)
			{
				HeroSlot->SetPosition(FVector2D(520.0f, 23.0f));
				HeroSlot->SetSize(TravelCombatVisualSize);
				HeroSlot->SetZOrder(2);
			}
		}

		for (int32 CompanionIndex = 0; CompanionIndex < 2; ++CompanionIndex)
		{
			UImage* CompanionImage = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				*FString::Printf(TEXT("TravelCompanionAnimatedUnit_%d"), CompanionIndex));
			if (!CompanionImage)
			{
				continue;
			}
			CompanionImage->SetBrush(FSlateBrush());
			CompanionImage->SetRenderOpacity(0.0f);
			CompanionImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			CompanionImage->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* CompanionSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(CompanionImage));
			if (CompanionSlot)
			{
				CompanionSlot->SetPosition(FVector2D(645.0f + CompanionIndex * 125.0f, 23.0f));
				CompanionSlot->SetSize(TravelCombatVisualSize);
				CompanionSlot->SetZOrder(2);
			}
			TravelCompanionImages.Add(CompanionImage);
			TravelAppliedCompanionAtlasPaths.Add(FSoftObjectPath());
			TravelAppliedCompanionFrames.Add(INDEX_NONE);
			TravelAppliedCompanionHealth.Add(-1.0f);
		}

		for (int32 EnemySlotIndex = 0; EnemySlotIndex < 3; ++EnemySlotIndex)
		{
			UProgressBar* EnemyHealth = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(),
				*FString::Printf(TEXT("TravelEnemyHealth_%d"), EnemySlotIndex));
			if (!EnemyHealth)
			{
				continue;
			}
			EnemyHealth->SetFillColorAndOpacity(FLinearColor(0.78f, 0.08f, 0.045f, 1.0f));
			EnemyHealth->SetPercent(1.0f);
			EnemyHealth->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(EnemyHealth));
			if (HealthSlot)
			{
				HealthSlot->SetPosition(FVector2D(33.0f + EnemySlotIndex * 125.0f, 174.0f));
				HealthSlot->SetSize(TravelHealthBarSize);
				HealthSlot->SetZOrder(3);
			}
			TravelEnemyHealthBars.Add(EnemyHealth);
		}

		TravelHeroHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TravelHeroHealth"));
		if (TravelHeroHealth)
		{
			TravelHeroHealth->SetFillColorAndOpacity(FLinearColor(0.12f, 0.72f, 0.22f, 1.0f));
			TravelHeroHealth->SetPercent(1.0f);
			TravelHeroHealth->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(TravelHeroHealth));
			if (HealthSlot)
			{
				HealthSlot->SetPosition(FVector2D(533.0f, 174.0f));
				HealthSlot->SetSize(TravelHealthBarSize);
				HealthSlot->SetZOrder(3);
			}
		}

		for (int32 CompanionIndex = 0; CompanionIndex < 2; ++CompanionIndex)
		{
			UProgressBar* CompanionHealth = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(),
				*FString::Printf(TEXT("TravelCompanionHealth_%d"), CompanionIndex));
			if (!CompanionHealth)
			{
				continue;
			}
			CompanionHealth->SetFillColorAndOpacity(FLinearColor(0.12f, 0.72f, 0.22f, 1.0f));
			CompanionHealth->SetPercent(1.0f);
			CompanionHealth->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* HealthSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(CompanionHealth));
			if (HealthSlot)
			{
				HealthSlot->SetPosition(FVector2D(658.0f + CompanionIndex * 125.0f, 174.0f));
				HealthSlot->SetSize(TravelHealthBarSize);
				HealthSlot->SetZOrder(3);
			}
			TravelCompanionHealthBars.Add(CompanionHealth);
		}
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKTrainingTravelRuntime TravelRuntime = Subsystem
		? Subsystem->GetTrainingTravelRuntimeCopy()
		: FGameXXKTrainingTravelRuntime();
	TravelVisualRuntime.Synchronize(TravelRuntime);
	if (TravelRuntime.Enemies.IsEmpty())
	{
		RequestTravelCombatAtlases(TravelRuntime.EnemyDefinitionId);
	}
	else
	{
		for (const FGameXXKTrainingTravelEnemyRuntime& Enemy : TravelRuntime.Enemies)
		{
			RequestTravelCombatAtlases(Enemy.EnemyDefinitionId);
		}
	}
	const FGameXXKTrainingOfflineReward PendingReward = Subsystem
		? Subsystem->GetPendingTrainingTravelRewardCopy()
		: FGameXXKTrainingOfflineReward();
	const FGameXXKTrainingProgress Progress = Subsystem
		? Subsystem->GetTrainingProgressCopy()
		: FGameXXKTrainingProgress();
	const auto FormatCooldown = [](const int32 RemainingSeconds) -> FString
	{
		const int32 SafeSeconds = FMath::Max(0, RemainingSeconds);
		return FString::Printf(TEXT("%02d:%02d"), SafeSeconds / 60, SafeSeconds % 60);
	};
	const FString RewardTooltip = FString::Printf(
		TEXT("待领取：%d 金币 / %d 经验 / 普通箱 %d / 精英箱 %d\n普通箱冷却 %s · 精英箱冷却 %s"),
		PendingReward.Gold,
		PendingReward.Experience,
		PendingReward.NormalChestCount,
		PendingReward.AdvancedChestCount,
		*FormatCooldown(Progress.TravelNormalChestCooldownRemainingSeconds),
		*FormatCooldown(Progress.TravelAdvancedChestCooldownRemainingSeconds));
	Strip->SetToolTipText(FText::FromString(RewardTooltip));
	UGameXXKDesktopTrainingActionButton* RetryButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TravelRetryButton"));
	RetryButton->Configure(this, 10);
	RetryButton->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(104.0f, 45.0f), FMargin(0.08f)));
	RetryButton->SetBackgroundColor(FLinearColor::White);
	RetryButton->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("失败重试")), 18));
	RetryButton->SetToolTipText(FText::FromString(TEXT("关闭时阵亡会回退到前一关；1-1 失败仍重试 1-1。")));
	AddCanvas(RootCanvas, RetryButton, FVector2D(1223.0f, 150.0f), FVector2D(104.0f, 45.0f));
	ActionButtons.Add(RetryButton);
	UpdateTravelVisuals();
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateTravelVisuals()
{
	if (!HasTravelVisualStripForTest())
	{
		return;
	}

	const float ScrollOffset = TravelVisualRuntime.GetScrollOffset();
	for (UImage* BackgroundImage : TravelBackgroundImages)
	{
		if (BackgroundImage)
		{
			// The authored hero walks left, so scenery must travel right to convey
			// forward movement while the hero remains anchored in the HUD strip.
			BackgroundImage->SetRenderTranslation(FVector2D(ScrollOffset, 0.0f));
		}
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKTrainingTravelRuntime AuthoritativeRuntime = Subsystem
		? Subsystem->GetTrainingTravelRuntimeCopy()
		: FGameXXKTrainingTravelRuntime();
	const FName PresentedEnemyId = TravelVisualRuntime.GetEnemyDefinitionId();
	RequestTravelCombatAtlases(PresentedEnemyId.IsNone() ? AuthoritativeRuntime.EnemyDefinitionId : PresentedEnemyId);
	for (int32 EnemySlotIndex = 0; EnemySlotIndex < TravelVisualRuntime.GetEnemyFormationSlotCount(); ++EnemySlotIndex)
	{
		RequestTravelCombatAtlases(TravelVisualRuntime.GetEnemyDefinitionIdForSlot(EnemySlotIndex));
	}

	if (TravelVisualRuntime.IsWalking())
	{
		const int32 WalkFrame = TravelVisualRuntime.GetWalkFrameIndex();
		const FSoftObjectPath WalkAtlasPath(TravelHeroAtlasTexturePath);
		if (TravelHeroAtlasTexture
			&& (TravelAppliedHeroAtlasPath != WalkAtlasPath || TravelAppliedHeroFrame != WalkFrame))
		{
			FSlateBrush HeroBrush;
			HeroBrush.DrawAs = ESlateBrushDrawType::Image;
			HeroBrush.ImageSize = TravelHeroWalkVisualSize;
			HeroBrush.SetResourceObject(TravelHeroAtlasTexture);
			const int32 SafeFrame = FMath::Clamp(WalkFrame, 0, FGameXXKTrainingTravelVisualRuntime::WalkFrameCount - 1);
			const int32 Column = SafeFrame % 8;
			const int32 Row = SafeFrame / 8;
			HeroBrush.SetUVRegion(FBox2f(
				FVector2f(static_cast<float>(Column) / 8.0f, static_cast<float>(Row) / 8.0f),
				FVector2f(static_cast<float>(Column + 1) / 8.0f, static_cast<float>(Row + 1) / 8.0f)));
			TravelHeroImage->SetBrush(HeroBrush);
			TravelAppliedHeroAtlasPath = WalkAtlasPath;
			TravelAppliedHeroFrame = WalkFrame;
		}
		TravelHeroImage->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
	else
	{
		const EGameXXKBattleAnimationAction HeroAction = TravelVisualRuntime.GetHeroAction();
		FGameXXKBattleAnimationClipDescriptor HeroClip = FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			MakeTravelOneKUnitId(FGameXXKEquipmentRules::HeroCharacterId()),
			NAME_None,
			false,
			HeroAction);
		float HeroPhaseDuration = 0.0f;
		switch (TravelVisualRuntime.GetVisualPhase())
		{
		case EGameXXKTrainingTravelVisualPhase::HeroAttack: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds; break;
		case EGameXXKTrainingTravelVisualPhase::HeroHit: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds; break;
		case EGameXXKTrainingTravelVisualPhase::HeroDeath: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroDeathSeconds; break;
		default: break;
		}
		if (HeroPhaseDuration > 0.0f)
		{
			HeroClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(HeroClip, HeroPhaseDuration);
		}
		if (!ApplyTravelAnimationFrame(
			TravelHeroImage,
			HeroClip,
			HeroAction == EGameXXKBattleAnimationAction::Idle,
			TravelAppliedHeroAtlasPath,
			TravelAppliedHeroFrame)
			&& TravelHeroFallbackTextures.Num() > 0
			&& TravelHeroFallbackTextures[0]
			&& TravelAppliedHeroAtlasPath == FSoftObjectPath(TravelHeroAtlasTexturePath))
		{
			FSlateBrush FallbackBrush;
			FallbackBrush.DrawAs = ESlateBrushDrawType::Image;
			FallbackBrush.ImageSize = TravelCombatVisualSize;
			FallbackBrush.SetResourceObject(TravelHeroFallbackTextures[0]);
			TravelHeroImage->SetBrush(FallbackBrush);
			TravelAppliedHeroAtlasPath.Reset();
			TravelAppliedHeroFrame = INDEX_NONE;
		}
		// The compact strip uses the same enemy-left / party-right formation as
		// the battle board.  Preserve the authored left-facing hero action and
		// compensate for each action atlas's authored alpha-bounds difference.
		const float HeroContentScale = ResolveTravelHeroContentScale(HeroAction);
		TravelHeroImage->SetRenderScale(FVector2D(
			HeroContentScale,
			HeroContentScale));
	}

	const int32 PresentedEnemySlotIndex = TravelVisualRuntime.GetPresentedEnemySlotIndex();
	bool bEnemyVisible = false;
	for (int32 EnemySlotIndex = 0; EnemySlotIndex < TravelEnemyImages.Num(); ++EnemySlotIndex)
	{
		UImage* EnemyImage = TravelEnemyImages[EnemySlotIndex];
		UProgressBar* EnemyHealthBar = TravelEnemyHealthBars.IsValidIndex(EnemySlotIndex)
			? TravelEnemyHealthBars[EnemySlotIndex]
			: nullptr;
		const bool bShowEnemy = TravelVisualRuntime.IsEnemySlotVisible(EnemySlotIndex);
		bEnemyVisible |= bShowEnemy;
		if (EnemyImage)
		{
			EnemyImage->SetVisibility(bShowEnemy ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (EnemyHealthBar)
		{
			EnemyHealthBar->SetVisibility(bShowEnemy ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (!bShowEnemy
			|| !EnemyImage
			|| !TravelAppliedEnemyAtlasPaths.IsValidIndex(EnemySlotIndex)
			|| !TravelAppliedEnemyFrames.IsValidIndex(EnemySlotIndex)
			|| !TravelAppliedEnemyHealth.IsValidIndex(EnemySlotIndex))
		{
			continue;
		}

		const FName EnemyId = TravelVisualRuntime.GetEnemyDefinitionIdForSlot(EnemySlotIndex);
		const bool bPresentedTarget = EnemySlotIndex == PresentedEnemySlotIndex;
		const EGameXXKBattleAnimationAction EnemyAction = bPresentedTarget
			? TravelVisualRuntime.GetEnemyAction()
			: EGameXXKBattleAnimationAction::Idle;
		FGameXXKBattleAnimationClipDescriptor EnemyClip = FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			MakeTravelOneKUnitId(EnemyId),
			MakeTravelOneKUnitId(EnemyId),
			true,
			EnemyAction);
		float EnemyPhaseDuration = 0.0f;
		if (bPresentedTarget)
		{
			switch (TravelVisualRuntime.GetVisualPhase())
			{
			case EGameXXKTrainingTravelVisualPhase::EnemyHit: EnemyPhaseDuration = FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds; break;
			case EGameXXKTrainingTravelVisualPhase::EnemyAttack: EnemyPhaseDuration = FGameXXKTrainingTravelVisualRuntime::EnemyAttackSeconds; break;
			case EGameXXKTrainingTravelVisualPhase::EnemyDeath: EnemyPhaseDuration = FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds; break;
			default: break;
			}
		}
		if (EnemyPhaseDuration > 0.0f)
		{
			EnemyClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(EnemyClip, EnemyPhaseDuration);
		}
		ApplyTravelAnimationFrame(
			EnemyImage,
			EnemyClip,
			EnemyAction == EGameXXKBattleAnimationAction::Idle,
			TravelAppliedEnemyAtlasPaths[EnemySlotIndex],
			TravelAppliedEnemyFrames[EnemySlotIndex]);
		const float EnemyContentScale = ResolveTravelEnemyContentScale(EnemyId, EnemyAction);
		EnemyImage->SetRenderScale(FVector2D(EnemyContentScale, EnemyContentScale));

		const float EnemyHealth = TravelVisualRuntime.GetEnemyHealthFractionForSlot(EnemySlotIndex);
		if (EnemyHealthBar && !FMath::IsNearlyEqual(EnemyHealth, TravelAppliedEnemyHealth[EnemySlotIndex]))
		{
			EnemyHealthBar->SetPercent(EnemyHealth);
			TravelAppliedEnemyHealth[EnemySlotIndex] = EnemyHealth;
		}
	}
	TravelHeroHealth->SetVisibility(bEnemyVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const TArray<FName> CompanionUnitIds = GetTravelCompanionUnitIds();
	for (int32 CompanionIndex = 0; CompanionIndex < TravelCompanionImages.Num(); ++CompanionIndex)
	{
		UImage* CompanionImage = TravelCompanionImages[CompanionIndex];
		UProgressBar* CompanionHealthBar = TravelCompanionHealthBars.IsValidIndex(CompanionIndex)
			? TravelCompanionHealthBars[CompanionIndex]
			: nullptr;
		const bool bShowCompanion = bEnemyVisible && CompanionUnitIds.IsValidIndex(CompanionIndex);
		if (!CompanionImage)
		{
			continue;
		}
		CompanionImage->SetVisibility(
			bShowCompanion ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (CompanionHealthBar)
		{
			CompanionHealthBar->SetVisibility(
				bShowCompanion ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			const float CompanionHealth = TravelVisualRuntime.GetPartyHealthFraction(CompanionIndex + 1);
			if (bShowCompanion
				&& TravelAppliedCompanionHealth.IsValidIndex(CompanionIndex)
				&& !FMath::IsNearlyEqual(TravelAppliedCompanionHealth[CompanionIndex], CompanionHealth))
			{
				CompanionHealthBar->SetPercent(CompanionHealth);
				TravelAppliedCompanionHealth[CompanionIndex] = CompanionHealth;
			}
		}
		if (!TravelAppliedCompanionAtlasPaths.IsValidIndex(CompanionIndex)
			|| !TravelAppliedCompanionFrames.IsValidIndex(CompanionIndex))
		{
			continue;
		}
		if (!CompanionUnitIds.IsValidIndex(CompanionIndex))
		{
			CompanionImage->SetBrush(FSlateBrush());
			CompanionImage->SetRenderOpacity(0.0f);
			TravelAppliedCompanionAtlasPaths[CompanionIndex].Reset();
			TravelAppliedCompanionFrames[CompanionIndex] = INDEX_NONE;
			continue;
		}

		const FName CompanionUnitId = CompanionUnitIds[CompanionIndex];
		const EGameXXKBattleAnimationAction CompanionAction =
			TravelVisualRuntime.GetPartyAction(CompanionIndex + 1);
		FGameXXKBattleAnimationClipPair CompanionClips =
			FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
				CompanionUnitId,
				false,
				CompanionAction);
		float CompanionPhaseDuration = 0.0f;
		switch (TravelVisualRuntime.GetVisualPhase())
		{
		case EGameXXKTrainingTravelVisualPhase::HeroAttack:
			if (CompanionAction == EGameXXKBattleAnimationAction::Attack)
			{
				CompanionPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds;
			}
			break;
		case EGameXXKTrainingTravelVisualPhase::HeroHit:
			if (CompanionAction == EGameXXKBattleAnimationAction::Hit)
			{
				CompanionPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds;
			}
			break;
		case EGameXXKTrainingTravelVisualPhase::HeroDeath:
			if (CompanionAction == EGameXXKBattleAnimationAction::Death)
			{
				CompanionPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroDeathSeconds;
			}
			break;
		default: break;
		}
		if (CompanionPhaseDuration > 0.0f)
		{
			CompanionClips.Preferred = FGameXXKBattleAnimationPresentation::FitClipToDuration(
				CompanionClips.Preferred,
				CompanionPhaseDuration);
			CompanionClips.Fallback = FGameXXKBattleAnimationPresentation::FitClipToDuration(
				CompanionClips.Fallback,
				CompanionPhaseDuration);
		}
		RequestTravelAtlas(CompanionClips);
		ApplyTravelAnimationFrame(
			CompanionImage,
			CompanionClips,
			CompanionAction == EGameXXKBattleAnimationAction::Idle,
			TravelAppliedCompanionAtlasPaths[CompanionIndex],
			TravelAppliedCompanionFrames[CompanionIndex]);
		const float CompanionContentScale = ResolveTravelPartyContentScale(
			CompanionUnitId,
			CompanionAction);
		CompanionImage->SetRenderScale(FVector2D(CompanionContentScale, CompanionContentScale));
	}
	const float HeroHealth = TravelVisualRuntime.GetHeroHealthFraction();
	if (!FMath::IsNearlyEqual(HeroHealth, TravelAppliedHeroHealth))
	{
		TravelHeroHealth->SetPercent(HeroHealth);
		TravelAppliedHeroHealth = HeroHealth;
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::EnsureTravelAtlasSession()
{
	if (!TravelAtlasCache)
	{
		TravelAtlasCache = MakeUnique<FGameXXKBattleAtlasCache>();
	}
	if (TravelAtlasSessionToken == 0)
	{
		static uint64 NextSessionToken = 1;
		TravelAtlasSessionToken = NextSessionToken++;
		if (TravelAtlasSessionToken == 0)
		{
			TravelAtlasSessionToken = NextSessionToken++;
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RequestTravelCombatAtlases(const FName EnemyDefinitionId)
{
	EnsureTravelAtlasSession();
	const EGameXXKBattleAnimationAction Actions[] = {
		EGameXXKBattleAnimationAction::Idle,
		EGameXXKBattleAnimationAction::Attack,
		EGameXXKBattleAnimationAction::Hit,
		EGameXXKBattleAnimationAction::Death};
	for (const EGameXXKBattleAnimationAction Action : Actions)
	{
		RequestTravelAtlas(FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			MakeTravelOneKUnitId(FGameXXKEquipmentRules::HeroCharacterId()), NAME_None, false, Action));
		if (!EnemyDefinitionId.IsNone())
		{
			RequestTravelAtlas(FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
				MakeTravelOneKUnitId(EnemyDefinitionId), MakeTravelOneKUnitId(EnemyDefinitionId), true, Action));
		}
	}
	for (const FName CompanionUnitId : GetTravelCompanionUnitIds())
	{
		RequestTravelAtlas(FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
			CompanionUnitId,
			false,
			EGameXXKBattleAnimationAction::Idle));
	}
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetTravelCompanionUnitIds() const
{
	TArray<FName> Result;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Result;
	}
	const FGameXXKTrainingTravelRuntime TravelRuntime = Subsystem->GetTrainingTravelRuntimeCopy();
	if (TravelRuntime.PartyUnits.Num() > 1)
	{
		for (int32 PartyIndex = 1; PartyIndex < TravelRuntime.PartyUnits.Num() && PartyIndex < 3; ++PartyIndex)
		{
			if (!TravelRuntime.PartyUnits[PartyIndex].UnitId.IsNone())
			{
				Result.Add(TravelRuntime.PartyUnits[PartyIndex].UnitId);
			}
		}
		if (!Result.IsEmpty())
		{
			return Result;
		}
	}

	const FGameXXKCardRunState& CardRun = Subsystem->GetRuntimeState().CardRun;
	const FName PermanentInstanceId = CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	if (!PermanentInstanceId.IsNone())
	{
		const FGameXXKPermanentCompanion* PermanentCompanion =
			CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
				[PermanentInstanceId](const FGameXXKPermanentCompanion& Candidate)
				{
					return Candidate.InstanceId == PermanentInstanceId && Candidate.bIsActive;
				});
		if (PermanentCompanion)
		{
			Result.Add(PermanentCompanion->InstanceId);
		}
	}

	const FName ActiveQuestNpcId = CardRun.ActiveTemporaryQuestNpcId;
	if (!ActiveQuestNpcId.IsNone()
		&& CardRun.PartySelection.QuestNpc.NpcId == ActiveQuestNpcId)
	{
		Result.AddUnique(ActiveQuestNpcId);
	}
	return Result;
}

void UGameXXKDesktopTrainingWorkbenchWidget::RequestTravelAtlas(
	const FGameXXKBattleAnimationClipDescriptor& Clip)
{
	FGameXXKBattleAnimationClipPair ClipPair;
	ClipPair.Preferred = Clip;
	RequestTravelAtlas(ClipPair);
}

void UGameXXKDesktopTrainingWorkbenchWidget::RequestTravelAtlas(
	const FGameXXKBattleAnimationClipPair& ClipPair)
{
	const FGameXXKBattleAnimationClipDescriptor& Clip = ClipPair.Preferred;
	if (!Clip.IsValid())
	{
		if (ClipPair.Fallback.IsValid())
		{
			RequestTravelAtlas(ClipPair.Fallback);
		}
		return;
	}
	if (!TravelAtlasCache
		|| TravelAtlasSessionToken == 0
		|| TravelRequestedAtlasPaths.Contains(Clip.TexturePath))
	{
		return;
	}

	TravelRequestedAtlasPaths.Add(Clip.TexturePath);
	if (!TravelPinnedAtlasPaths.Contains(Clip.TexturePath))
	{
		TravelAtlasCache->Pin(Clip.TexturePath);
		TravelPinnedAtlasPaths.Add(Clip.TexturePath);
	}
	const uint64 RequestToken = TravelAtlasSessionToken;
	const FSoftObjectPath RequestPath = Clip.TexturePath;
	const FGameXXKBattleAnimationClipDescriptor FallbackClip = ClipPair.Fallback;
	const TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> WeakWidget(this);
	TravelAtlasCache->Acquire(
		RequestPath,
		RequestToken,
		[WeakWidget, RequestToken, RequestPath, FallbackClip](
			UTexture2D* Texture,
			const EGameXXKAtlasLoadResult Result)
		{
			UGameXXKDesktopTrainingWorkbenchWidget* Widget = WeakWidget.Get();
			if (!Widget
				|| Widget->TravelAtlasSessionToken != RequestToken)
			{
				return;
			}
			if (Result == EGameXXKAtlasLoadResult::Loaded && Texture)
			{
				Widget->TravelLoadedAtlasTextures.Add(RequestPath, Texture);
				Widget->UpdateTravelVisuals();
				return;
			}

			Widget->TravelLoadedAtlasTextures.Remove(RequestPath);
			if (Widget->TravelPinnedAtlasPaths.Remove(RequestPath) > 0 && Widget->TravelAtlasCache)
			{
				Widget->TravelAtlasCache->Unpin(RequestPath);
			}
			if ((Result == EGameXXKAtlasLoadResult::Missing
					|| Result == EGameXXKAtlasLoadResult::TimedOut)
				&& FallbackClip.IsValid()
				&& FallbackClip.TexturePath != RequestPath)
			{
				Widget->RequestTravelAtlas(FallbackClip);
			}
			Widget->UpdateTravelVisuals();
		});
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ApplyTravelAnimationFrame(
	UImage* Image,
	const FGameXXKBattleAnimationClipDescriptor& Clip,
	const bool bLooping,
	FSoftObjectPath& InOutAppliedPath,
	int32& InOutAppliedFrame)
{
	if (!Image || !Clip.IsValid())
	{
		return false;
	}
	const TWeakObjectPtr<UTexture2D>* LoadedTexture = TravelLoadedAtlasTextures.Find(Clip.TexturePath);
	if (!LoadedTexture || !LoadedTexture->IsValid())
	{
		return false;
	}
	const int32 FrameIndex = FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
		Clip,
		TravelVisualRuntime.GetVisualPhaseElapsedSeconds(),
		bLooping);
	if (InOutAppliedPath == Clip.TexturePath && InOutAppliedFrame == FrameIndex)
	{
		return true;
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = TravelCombatVisualSize;
	Brush.SetResourceObject(LoadedTexture->Get());
	Brush.SetUVRegion(FGameXXKBattleAnimationPresentation::CalculateUvRegion(Clip, FrameIndex));
	Image->SetBrush(Brush);
	InOutAppliedPath = Clip.TexturePath;
	InOutAppliedFrame = FrameIndex;
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ApplyTravelAnimationFrame(
	UImage* Image,
	const FGameXXKBattleAnimationClipPair& ClipPair,
	const bool bLooping,
	FSoftObjectPath& InOutAppliedPath,
	int32& InOutAppliedFrame)
{
	if (!Image)
	{
		return false;
	}

	const FGameXXKBattleAnimationClipDescriptor* SelectedClip = nullptr;
	const TWeakObjectPtr<UTexture2D>* LoadedTexture = nullptr;
	const auto TrySelectLoadedClip = [this, &SelectedClip, &LoadedTexture](
		const FGameXXKBattleAnimationClipDescriptor& Candidate)
	{
		if (!Candidate.IsValid())
		{
			return;
		}
		const TWeakObjectPtr<UTexture2D>* CandidateTexture =
			TravelLoadedAtlasTextures.Find(Candidate.TexturePath);
		if (CandidateTexture && CandidateTexture->IsValid())
		{
			SelectedClip = &Candidate;
			LoadedTexture = CandidateTexture;
		}
	};
	TrySelectLoadedClip(ClipPair.Preferred);
	if (!SelectedClip)
	{
		TrySelectLoadedClip(ClipPair.Fallback);
	}

	if (!SelectedClip || !LoadedTexture)
	{
		Image->SetBrush(FSlateBrush());
		Image->SetRenderOpacity(0.0f);
		InOutAppliedPath.Reset();
		InOutAppliedFrame = INDEX_NONE;
		return false;
	}

	const int32 FrameIndex = FGameXXKBattleAnimationPresentation::CalculateFrameIndex(
		*SelectedClip,
		TravelVisualRuntime.GetVisualPhaseElapsedSeconds(),
		bLooping);
	if (InOutAppliedPath == SelectedClip->TexturePath
		&& InOutAppliedFrame == FrameIndex
		&& Image->GetBrush().GetResourceObject() == LoadedTexture->Get())
	{
		Image->SetRenderOpacity(1.0f);
		return true;
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = TravelCombatVisualSize;
	Brush.SetResourceObject(LoadedTexture->Get());
	Brush.SetUVRegion(FGameXXKBattleAnimationPresentation::CalculateUvRegion(*SelectedClip, FrameIndex));
	Image->SetBrush(Brush);
	Image->SetRenderOpacity(1.0f);
	InOutAppliedPath = SelectedClip->TexturePath;
	InOutAppliedFrame = FrameIndex;
	return true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReleaseTravelAtlasSession()
{
	if (TravelAtlasCache && TravelAtlasSessionToken != 0)
	{
		const uint64 ClosingToken = TravelAtlasSessionToken;
		TravelAtlasSessionToken = 0;
		TravelAtlasCache->CancelSession(ClosingToken);
		for (const FSoftObjectPath& Path : TravelPinnedAtlasPaths)
		{
			TravelAtlasCache->Unpin(Path);
		}
		TravelAtlasCache->Clear();
	}
	TravelRequestedAtlasPaths.Reset();
	TravelPinnedAtlasPaths.Reset();
	TravelLoadedAtlasTextures.Reset();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWarehousePanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel, TEXT("WarehousePanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetWarehouseRect());
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("仓库")), 28, Ink);
	AddCanvas(RootCanvas, Title, FVector2D(30.0f, 34.0f), FVector2D(323.0f, 38.0f));
	BuildPanelCloseButton(TEXT("WarehouseCloseButton"), ActionCloseWarehouse, FVector2D(314.0f, 30.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	for (int32 PageTabIndex = 0; PageTabIndex < 4; ++PageTabIndex)
	{
		UGameXXKDesktopTrainingActionButton* PageTab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("WarehousePageTab_%d"), PageTabIndex));
		PageTab->Configure(this, 70 + PageTabIndex);
		const bool bSelectedPage = PageTabIndex < 3 && PageTabIndex == GetWarehousePageIndexForTest();
		PageTab->SetStyle(MakeTextureButtonStyle(
			bSelectedPage ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(62.0f, 38.0f),
			FMargin(0.08f)));
		PageTab->SetBackgroundColor(FLinearColor::White);
		PageTab->SetContent(MakeButtonText(
			WidgetTree,
			FText::FromString(PageTabIndex < 3 ? FString::FromInt(PageTabIndex + 1) : TEXT("+")),
			17,
			Ink));
		PageTab->SetIsEnabled(PageTabIndex == 3 || PageTabIndex < GetWarehousePageCountForTest());
		AddCanvas(RootCanvas, PageTab, FVector2D(30.0f + PageTabIndex * 78.0f, 84.0f), FVector2D(62.0f, 38.0f));
		ActionButtons.Add(PageTab);
	}
	for (int32 SlotIndex = 0; SlotIndex < WarehousePageSize; ++SlotIndex)
	{
		const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + SlotIndex;
		FGameXXKDesktopInventoryEntryKey Entry = RuntimeState
			? FGameXXKDesktopInventoryRules::GetEntryAt(
				*RuntimeState,
				EGameXXKDesktopItemContainer::Warehouse,
				PhysicalSlotIndex)
			: FGameXXKDesktopInventoryEntryKey();
		if (ShouldHideDesktopInventoryEntry(EGameXXKDesktopItemContainer::Warehouse, Entry))
		{
			Entry = FGameXXKDesktopInventoryEntryKey();
		}
		const int32 Column = SlotIndex % WarehouseColumns;
		const int32 Row = SlotIndex / WarehouseColumns;
		const FVector2D CellPosition(30.0f + Column * 78.0f, 142.0f + Row * 72.0f);
		const FVector2D CellSize(68.0f, 68.0f);
		const FName CellName(*FString::Printf(TEXT("WarehouseSlot_%d"), SlotIndex));
		UGameXXKDesktopTrainingActionButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			CellName);
		SlotButton->Configure(this, 100 + SlotIndex);
		SlotButton->SetBackgroundColor(FLinearColor::White);
		if (Entry.IsValid())
		{
			const FString EntryDisplayLabel = Entry.bEquipmentInstance && RuntimeState
				? EquipmentDisplayName(RuntimeState->EquipmentCollection, Entry.EntryId)
				: ItemDisplayName(Entry.EntryId);
			const FString IconPath = Entry.bEquipmentInstance && RuntimeState
				? EquipmentIconTexturePath(RuntimeState->EquipmentCollection, Entry.EntryId)
				: InventoryItemIconTexturePath(Entry.EntryId);
			const int32 Quantity = !Entry.bEquipmentInstance && RuntimeState
				? RuntimeState->DesktopInventory.WarehouseItems.FindRef(Entry.EntryId)
				: 1;
			SlotButton->SetStyle(MakeImageButtonStyle(
				Entry.bEquipmentInstance ? EquipmentSlotTexturePath : ItemSlotTexturePath,
				CellSize));
			SlotButton->SetContent(MakeIconLabelContent(
				WidgetTree,
				IconPath,
				FVector2D(46.0f, 46.0f),
				Quantity > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), Quantity)) : FText::GetEmpty()));
			if (RuntimeState && FGameXXKDesktopInventoryRules::IsEntryLocked(*RuntimeState, Entry))
			{
				AddLockedCellIcon(
					WidgetTree,
					SlotButton,
					*FString::Printf(TEXT("WarehouseLockedIcon_%d"), SlotIndex));
			}
			SlotButton->SetToolTipText(FText::FromString(FString::Printf(
				TEXT("%s\n%s\n左键拿起；右键返回背包"),
				*Entry.EntryId.ToString(),
				*EntryDisplayLabel)));
			SlotButton->SetIsEnabled(true);
		}
		else
		{
			SlotButton->SetStyle(MakeImageButtonStyle(ItemSlotTexturePath, CellSize));
			SlotButton->SetContent(nullptr);
			SlotButton->SetIsEnabled(CarriedEntry.IsValid());
		}
		AddCanvas(RootCanvas, SlotButton, CellPosition, CellSize);
		ActionButtons.Add(SlotButton);
	}
	const int32 WarehouseCount = GetWarehouseOccupancyForTest();
	UTextBlock* PageText = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("第 %d / %d 页 · 每页 %d 格"),
		GetWarehousePageIndexForTest() + 1,
		GetWarehousePageCountForTest(),
		WarehousePageSize)), 15, Ink);
	AddCanvas(RootCanvas, PageText, FVector2D(30.0f, 806.0f), FVector2D(300.0f, 24.0f));
	UGameXXKDesktopTrainingActionButton* Previous = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Previous->Configure(this, 40);
	Previous->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(94.0f, 40.0f), FMargin(0.08f)));
	Previous->SetBackgroundColor(FLinearColor::White);
	Previous->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("上一页")), 15));
	Previous->SetIsEnabled(GetWarehousePageIndexForTest() > 0);
	AddCanvas(RootCanvas, Previous, FVector2D(30.0f, 842.0f), FVector2D(94.0f, 40.0f));
	ActionButtons.Add(Previous);
	UGameXXKDesktopTrainingActionButton* Next = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Next->Configure(this, 41);
	Next->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(94.0f, 40.0f), FMargin(0.08f)));
	Next->SetBackgroundColor(FLinearColor::White);
	Next->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("下一页")), 15));
	Next->SetIsEnabled(GetWarehousePageIndexForTest() + 1 < GetWarehousePageCountForTest());
	AddCanvas(RootCanvas, Next, FVector2D(132.0f, 842.0f), FVector2D(94.0f, 40.0f));
	ActionButtons.Add(Next);
	UGameXXKDesktopTrainingActionButton* Sort = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Sort->Configure(this, 5);
	Sort->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(104.0f, 40.0f), FMargin(0.08f)));
	Sort->SetBackgroundColor(FLinearColor::White);
	Sort->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("排序")), 15));
	Sort->SetToolTipText(FText::FromString(TEXT("按槽位、品质和等级排序仓库")));
	AddCanvas(RootCanvas, Sort, FVector2D(236.0f, 842.0f), FVector2D(104.0f, 40.0f));
	ActionButtons.Add(Sort);
	UTextBlock* Footer = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("仓库物品 %d / %d\n不显示角色身份卡"),
		WarehouseCount,
		FGameXXKEquipmentRules::WarehouseCapacity)), 16, Ink);
	AddCanvas(RootCanvas, Footer, FVector2D(30.0f, 888.0f), FVector2D(310.0f, 32.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBackpackPanel()
{
	UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackpackPanel"));
	FSlateBrush TransparentBrush;
	TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	PanelBorder->SetBrush(TransparentBrush);
	PanelBorder->SetPadding(FMargin(0.0f));
	PanelBorder->SetClipping(EWidgetClipping::ClipToBounds);
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetContentRect());

	UScaleBox* EmbeddedScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("EmbeddedBackpackScale"));
	EmbeddedScale->SetStretch(EStretch::ScaleToFit);
	EmbeddedScale->SetStretchDirection(EStretchDirection::Both);
	USizeBox* ApprovedPaperReference = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EmbeddedBackpackPaperReference"));
	ApprovedPaperReference->SetWidthOverride(1450.0f);
	ApprovedPaperReference->SetHeightOverride(849.0f);
	UCanvasPanel* CropCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EmbeddedBackpackCropCanvas"));
	CropCanvas->SetClipping(EWidgetClipping::ClipToBounds);

	EmbeddedInventoryWidget = WidgetTree->ConstructWidget<UGameXXKInventoryWindowWidget>(
		UGameXXKInventoryWindowWidget::StaticClass(),
		TEXT("EmbeddedApprovedBackpack"));
	if (EmbeddedInventoryWidget)
	{
		EmbeddedInventoryWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		EmbeddedInventoryWidget->ConfigureDesktopTrainingEmbeddedMode(true);
		EmbeddedInventoryWidget->ConfigureDesktopTrainingCharacter(GetActiveBackpackCharacterIdForTest());
		EmbeddedInventoryWidget->ConfigureDesktopTrainingHost(this);
		EmbeddedInventoryWidget->OpenFreeInventoryForTest();
		if (bHasSavedEmbeddedInventorySession)
		{
			EmbeddedInventoryWidget->RestoreEmbeddedSessionState(SavedEmbeddedInventorySession);
			bHasSavedEmbeddedInventorySession = false;
		}
		AddCanvas(CropCanvas, EmbeddedInventoryWidget.Get(), FVector2D(-311.0f, -173.0f), FVector2D(1920.0f, 1080.0f));
	}
	ApprovedPaperReference->SetContent(CropCanvas);
	EmbeddedScale->SetContent(ApprovedPaperReference);
	PanelBorder->SetContent(EmbeddedScale);

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	UImage* GoldIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackpackGoldIcon"));
	GoldIcon->SetBrush(MakeTextureBrush(IngotTexturePath, FVector2D(30.0f, 30.0f)));
	AddCanvas(RootCanvas, GoldIcon, FVector2D(1098.0f, 263.0f), FVector2D(30.0f, 30.0f));
	const FString GoldLabel = RuntimeState ? FString::Printf(TEXT("%d"), RuntimeState->PlayerGold) : TEXT("--");
	UTextBlock* GoldText = MakeText(WidgetTree, FText::FromString(GoldLabel), 18, Ink);
	AddCanvas(RootCanvas, GoldText, FVector2D(1132.0f, 264.0f), FVector2D(100.0f, 30.0f));

	UGameXXKDesktopTrainingActionButton* Sort = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("BackpackSortButton"));
	Sort->Configure(this, 61);
	Sort->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(100.0f, 44.0f), FMargin(0.08f)));
	Sort->SetBackgroundColor(FLinearColor::White);
	Sort->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("排序")), 17, Ink));
	AddCanvas(RootCanvas, Sort, FVector2D(1212.0f, 710.0f), FVector2D(100.0f, 44.0f));
	ActionButtons.Add(Sort);
	BuildCharacterRosterTabs();
	BuildPanelCloseButton(TEXT("BackpackPanelCloseButton"), 60, FVector2D(1290.0f, 270.0f));
	if (UButton* CloseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton"))))
	{
		const FText CloseDescription = FText::FromString(TEXT("关闭背包与全部子界面"));
		CloseButton->SetToolTipText(CloseDescription);
		UTextBlock* AccessibleLabel = MakeButtonText(WidgetTree, CloseDescription, 1, FLinearColor::Transparent);
		AccessibleLabel->SetRenderOpacity(0.0f);
		CloseButton->SetContent(AccessibleLabel);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildCharacterRosterTabs()
{
	struct FRosterTabSpec
	{
		EGameXXKDesktopTrainingCharacterRoster Roster;
		int32 ActionId;
		const TCHAR* Name;
		const TCHAR* Label;
	};
	const FRosterTabSpec Tabs[] = {
		{EGameXXKDesktopTrainingCharacterRoster::Hero, 80, TEXT("CharacterRosterHeroButton"), TEXT("主角")},
		{EGameXXKDesktopTrainingCharacterRoster::Companions, 81, TEXT("CharacterRosterCompanionButton"), TEXT("伙伴")},
		{EGameXXKDesktopTrainingCharacterRoster::Npcs, 82, TEXT("CharacterRosterNpcButton"), TEXT("NPC")}};
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Tabs); ++Index)
	{
		const bool bSelected = ActiveCharacterRoster == Tabs[Index].Roster;
		const FName RepresentativeId = ResolveRosterRepresentativeCharacterId(Tabs[Index].Roster);
		const FString PortraitPath = CharacterRosterPortraitPath(Subsystem, RepresentativeId);
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			Tabs[Index].Name);
		Button->Configure(this, Tabs[Index].ActionId);
		Button->SetStyle(MakeTextureButtonStyle(
			bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(105.0f, 62.0f),
			FMargin(0.08f)));
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(MakeCharacterPortraitContent(
			WidgetTree,
			PortraitPath,
			FText::FromString(Tabs[Index].Label),
			FVector2D(38.0f, 38.0f),
			*FString::Printf(TEXT("CharacterRosterRepresentativePortrait_%d"), Index),
			bSelected ? Accent : Ink));
		Button->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("查看%s；这里只切换查看对象，不会改变编队"),
			Tabs[Index].Label)));
		AddCanvas(RootCanvas, Button, FVector2D(414.0f + Index * 113.0f, 706.0f), FVector2D(105.0f, 62.0f));
		ActionButtons.Add(Button);
	}

	TArray<FName> VisibleCharacters;
	int32 FirstActionId = INDEX_NONE;
	if (ActiveCharacterRoster == EGameXXKDesktopTrainingCharacterRoster::Companions)
	{
		VisibleCharacters = GetCompanionCharacterIdsForTest();
		FirstActionId = 400;
	}
	else if (ActiveCharacterRoster == EGameXXKDesktopTrainingCharacterRoster::Npcs)
	{
		VisibleCharacters = GetNpcCharacterIdsForTest();
		FirstActionId = 420;
	}
	if (FirstActionId == INDEX_NONE)
	{
		return;
	}

	for (int32 Index = 0; Index < VisibleCharacters.Num() && Index < 6; ++Index)
	{
		const FName CharacterId = VisibleCharacters[Index];
		const bool bSelected = CharacterId == ActiveBackpackCharacterId;
		UGameXXKDesktopTrainingActionButton* PortraitButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("CharacterRosterPortraitButton_%d_%d"), static_cast<int32>(ActiveCharacterRoster), Index));
		PortraitButton->Configure(this, FirstActionId + Index);
		PortraitButton->SetStyle(MakeTextureButtonStyle(
			bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(105.0f, 62.0f),
			FMargin(0.08f)));
		PortraitButton->SetBackgroundColor(FLinearColor::White);
		const FString PortraitPath = CharacterRosterPortraitPath(Subsystem, CharacterId);
		const FString Label = ActiveCharacterRoster == EGameXXKDesktopTrainingCharacterRoster::Npcs
			? QuestNpcDisplayName(CharacterId)
			: BackpackCharacterDisplayName(Subsystem, CharacterId);
		PortraitButton->SetContent(MakeCharacterPortraitContent(
			WidgetTree,
			PortraitPath,
			FText::FromString(Label),
			FVector2D(38.0f, 38.0f),
			*FString::Printf(TEXT("CharacterRosterMemberPortrait_%d_%d"), static_cast<int32>(ActiveCharacterRoster), Index),
			bSelected ? Accent : Ink));
		PortraitButton->SetToolTipText(FText::FromString(TEXT("查看该角色的属性、装备与卡组；不会改变编队")));
		AddCanvas(
			RootCanvas,
			PortraitButton,
			FVector2D(414.0f + (Index % 3) * 113.0f, 566.0f + (Index / 3) * 66.0f),
			FVector2D(105.0f, 62.0f));
		ActionButtons.Add(PortraitButton);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildFormationPanel()
{
	EnsureFormationCandidate();
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt, TEXT("FormationPanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetContentRect());
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("编队")), 30, Ink);
	AddCanvas(RootCanvas, Title, FVector2D(421.0f, 258.0f), FVector2D(180.0f, 40.0f));
	BuildPanelCloseButton(TEXT("FormationCloseButton"), ActionCloseCentralPage, FVector2D(1284.0f, 258.0f));
	UTextBlock* Hint = MakeText(
		WidgetTree,
		FText::FromString(TEXT("查看角色不会换队；只有右侧“编入队伍”会写入当前伙伴或任务 NPC。")),
		15,
		Ink);
	AddCanvas(RootCanvas, Hint, FVector2D(520.0f, 264.0f), FVector2D(770.0f, 30.0f));

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKCardRunState* CardRun = Subsystem ? &Subsystem->GetRuntimeState().CardRun : nullptr;
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const FName CompanionId = CardRun
		? CardRun->PartySelection.ActivePermanentCompanionInstanceId
		: NAME_None;
	const FName NpcId = CardRun ? CardRun->ActiveTemporaryQuestNpcId : NAME_None;
	const FName PartyIds[] = {HeroId, CompanionId, NpcId};
	const TCHAR* SlotNames[] = {TEXT("FormationHeroSlot"), TEXT("FormationCompanionSlot"), TEXT("FormationNpcSlot")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PartyIds); ++Index)
	{
		const FName CharacterId = PartyIds[Index];
		FString Label;
		if (Index == 0)
		{
			Label = TEXT("主角 · 固定出战");
		}
		else if (Index == 1)
		{
			Label = CharacterId.IsNone()
				? TEXT("伙伴 · 未编入")
				: BackpackCharacterDisplayName(Subsystem, CharacterId);
		}
		else
		{
			Label = CharacterId.IsNone()
				? TEXT("NPC · 未编入")
				: QuestNpcDisplayName(CharacterId);
		}
		UBorder* PartySlot = MakeSlotPanel(
			WidgetTree,
			Index == 0 ? EquipmentSlotTexturePath : ItemSlotTexturePath,
			Panel,
			FVector2D(150.0f, 205.0f),
			SlotNames[Index]);
		PartySlot->SetPadding(FMargin(6.0f));
		PartySlot->SetContent(MakeCharacterPortraitContent(
			WidgetTree,
			CharacterRosterPortraitPath(Subsystem, CharacterId),
			FText::FromString(Label),
			FVector2D(126.0f, 126.0f),
			*FString::Printf(TEXT("FormationCurrentPortrait_%d"), Index),
			Ink));
		AddCanvas(RootCanvas, PartySlot, FVector2D(421.0f + Index * 165.0f, 315.0f), FVector2D(150.0f, 205.0f));
	}

	UTextBlock* CandidateTitle = MakeText(WidgetTree, FText::FromString(TEXT("选择候选")), 20, Ink);
	AddCanvas(RootCanvas, CandidateTitle, FVector2D(930.0f, 306.0f), FVector2D(190.0f, 30.0f));
	struct FFormationTabSpec
	{
		EGameXXKDesktopTrainingCharacterRoster Roster;
		int32 ActionId;
		const TCHAR* Name;
		const TCHAR* Label;
	};
	const FFormationTabSpec FormationTabs[] = {
		{EGameXXKDesktopTrainingCharacterRoster::Companions, 83, TEXT("FormationCompanionRosterButton"), TEXT("伙伴")},
		{EGameXXKDesktopTrainingCharacterRoster::Npcs, 84, TEXT("FormationNpcRosterButton"), TEXT("NPC")}};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FormationTabs); ++Index)
	{
		const bool bSelected = ActiveFormationRoster == FormationTabs[Index].Roster;
		UGameXXKDesktopTrainingActionButton* Tab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			FormationTabs[Index].Name);
		Tab->Configure(this, FormationTabs[Index].ActionId);
		Tab->SetStyle(MakeTextureButtonStyle(
			bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(105.0f, 62.0f),
			FMargin(0.08f)));
		Tab->SetBackgroundColor(FLinearColor::White);
		Tab->SetContent(MakeButtonText(WidgetTree, FText::FromString(FormationTabs[Index].Label), 16, bSelected ? Accent : Ink));
		AddCanvas(RootCanvas, Tab, FVector2D(930.0f + Index * 113.0f, 338.0f), FVector2D(105.0f, 62.0f));
		ActionButtons.Add(Tab);
	}

	const bool bCompanionRoster = ActiveFormationRoster == EGameXXKDesktopTrainingCharacterRoster::Companions;
	const TArray<FName> Candidates = bCompanionRoster
		? GetCompanionCharacterIdsForTest()
		: GetNpcCharacterIdsForTest();
	const int32 FirstActionId = bCompanionRoster ? 440 : 460;
	for (int32 Index = 0; Index < Candidates.Num() && Index < 6; ++Index)
	{
		const FName CharacterId = Candidates[Index];
		const bool bSelected = CharacterId == FormationCandidateCharacterId;
		UGameXXKDesktopTrainingActionButton* Candidate = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("FormationCandidateButton_%d"), Index));
		Candidate->Configure(this, FirstActionId + Index);
		Candidate->SetStyle(MakeTextureButtonStyle(
			bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(105.0f, 62.0f),
			FMargin(0.08f)));
		Candidate->SetBackgroundColor(FLinearColor::White);
		const FString Label = bCompanionRoster
			? BackpackCharacterDisplayName(Subsystem, CharacterId)
			: QuestNpcDisplayName(CharacterId);
		Candidate->SetContent(MakeCharacterPortraitContent(
			WidgetTree,
			CharacterRosterPortraitPath(Subsystem, CharacterId),
			FText::FromString(Label),
			FVector2D(38.0f, 38.0f),
			*FString::Printf(TEXT("FormationCandidatePortrait_%d"), Index),
			bSelected ? Accent : Ink));
		Candidate->SetToolTipText(FText::FromString(TEXT("只选择候选；点击下方“编入队伍”后才会生效")));
		AddCanvas(
			RootCanvas,
			Candidate,
			FVector2D(930.0f + (Index % 3) * 113.0f, 410.0f + (Index / 3) * 66.0f),
			FVector2D(105.0f, 62.0f));
		ActionButtons.Add(Candidate);
	}

	UGameXXKDesktopTrainingActionButton* Apply = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("FormationApplyButton"));
	Apply->Configure(this, 85);
	Apply->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(160.0f, 62.0f), FMargin(0.08f)));
	Apply->SetBackgroundColor(FLinearColor::White);
	Apply->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("编入队伍")), 18, Ink));
	Apply->SetIsEnabled(!FormationCandidateCharacterId.IsNone());
	AddCanvas(RootCanvas, Apply, FVector2D(1017.0f, 552.0f), FVector2D(160.0f, 62.0f));
	ActionButtons.Add(Apply);

	UTextBlock* Footer = MakeText(
		WidgetTree,
		FText::FromString(TEXT("主角固定；伙伴与任务 NPC 各最多一名。候选选择不会改动背包当前查看对象。")),
		15,
		Ink);
	AddCanvas(RootCanvas, Footer, FVector2D(421.0f, 548.0f), FVector2D(470.0f, 64.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTalentsPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt, TEXT("TalentsPanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetContentRect());
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("天赋  ·  天赋树 / 称号")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(417.0f, 260.0f), FVector2D(700.0f, 42.0f));
	BuildPanelCloseButton(TEXT("TalentsCloseButton"), ActionCloseCentralPage, FVector2D(1284.0f, 258.0f));
	UTextBlock* Notice = MakeText(
		WidgetTree,
		FText::FromString(TEXT("天赋和称号集中在此页；真实节点数据与宝箱掉率加成尚未接入。")),
		18,
		FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
	AddCanvas(RootCanvas, Notice, FVector2D(420.0f, 312.0f), FVector2D(860.0f, 42.0f));
	for (int32 NodeIndex = 0; NodeIndex < 12; ++NodeIndex)
	{
		UBorder* Node = MakePanel(WidgetTree, NodeIndex == 0 ? Accent : Panel);
		AddCanvas(
			RootCanvas,
			Node,
			FVector2D(420.0f + (NodeIndex % 4) * 225.0f, 370.0f + (NodeIndex / 4) * 118.0f),
			FVector2D(190.0f, 88.0f));
		UTextBlock* NodeText = MakeText(
			WidgetTree,
			FText::FromString(NodeIndex == 0 ? TEXT("基础天赋\n待配置") : FString::Printf(TEXT("节点 %02d\n锁定"), NodeIndex + 1)),
			16,
			FLinearColor::White);
		AddCanvas(
			RootCanvas,
			NodeText,
			FVector2D(436.0f + (NodeIndex % 4) * 225.0f, 388.0f + (NodeIndex / 4) * 118.0f),
			FVector2D(158.0f, 50.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildToolsPanel()
{
	ToolSlots.SetNum(ToolSlotCount);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel, TEXT("ToolsPanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetRightShellRect());
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("工具")), 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1387.0f, 34.0f), FVector2D(255.0f, 38.0f));
	BuildPanelCloseButton(TEXT("ToolsCloseButton"), ActionCloseRightPanel, FVector2D(1602.0f, 30.0f));
	const TArray<FText> ToolLabels = {
		FText::FromString(TEXT("分解")),
		FText::FromString(TEXT("合成")),
		FText::FromString(TEXT("强化")),
		FText::FromString(TEXT("洗炼")),
		FText::FromString(TEXT("镶嵌"))};
	for (int32 ToolIndex = 0; ToolIndex < ToolLabels.Num(); ++ToolIndex)
	{
		UGameXXKDesktopTrainingActionButton* ToolButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("ToolButton_%d"), ToolIndex));
		ToolButton->Configure(this, 30 + ToolIndex);
		ToolButton->SetStyle(MakeTextureButtonStyle(
			ToolIndex == static_cast<int32>(ActiveToolMode) ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(47.0f, 40.0f),
			FMargin(0.08f)));
		ToolButton->SetBackgroundColor(FLinearColor::White);
		ToolButton->SetContent(MakeButtonText(WidgetTree, ToolLabels[ToolIndex], 13, Ink));
		AddCanvas(RootCanvas, ToolButton, FVector2D(1385.0f + ToolIndex * 52.0f, 84.0f), FVector2D(47.0f, 40.0f));
		ActionButtons.Add(ToolButton);
	}
	if (Subsystem)
	{
		const FGameXXKToolProgress& Progress = Subsystem->GetToolProgress();
		const int64 Next = FGameXXKEquipmentToolRules::GetExperienceForNextLevel(Progress.Level);
		const FString ProgressText = Progress.Level >= FGameXXKEquipmentToolRules::MaximumLevel
			? FString::Printf(TEXT("工具 Lv.%d  MAX"), Progress.Level)
			: FString::Printf(TEXT("工具 Lv.%d  %lld/%lld"), Progress.Level, Progress.Experience, Next);
		UTextBlock* ProgressLabel = MakeText(WidgetTree, FText::FromString(ProgressText), 15, Ink);
		AddCanvas(RootCanvas, ProgressLabel, FVector2D(1390.0f, 126.0f), FVector2D(245.0f, 22.0f));
	}
	UBorder* GridFrame = MakePanel(WidgetTree, PanelAlt, TEXT("ToolInputGridFrame"));
	AddCanvas(RootCanvas, GridFrame, FVector2D(1385.0f, 148.0f), FVector2D(260.0f, 360.0f));
	for (int32 SlotIndex = 0; SlotIndex < ToolSlotCount; ++SlotIndex)
	{
		const int32 Column = SlotIndex % 3;
		const int32 Row = SlotIndex / 3;
		UGameXXKDesktopTrainingActionButton* ToolSlotButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("ToolInputSlot_%d"), SlotIndex));
		ToolSlotButton->Configure(this, 300 + SlotIndex);
		ToolSlotButton->SetStyle(MakeImageButtonStyle(ItemSlotTexturePath, FVector2D(64.0f, 64.0f)));
		ToolSlotButton->SetBackgroundColor(FLinearColor::White);
		if (ToolSlots[SlotIndex].IsValid())
		{
			ToolSlotButton->SetContent(MakeIconLabelContent(
				WidgetTree,
				ToolSlots[SlotIndex].IconPath,
				FVector2D(46.0f, 46.0f),
				ToolSlots[SlotIndex].Quantity > 1
					? FText::FromString(FString::Printf(TEXT("x%d"), ToolSlots[SlotIndex].Quantity))
					: FText::GetEmpty()));
			if (Subsystem
				&& FGameXXKDesktopInventoryRules::IsEntryLocked(
					Subsystem->GetRuntimeState(),
					ToolSlots[SlotIndex].Entry))
			{
				AddLockedCellIcon(
					WidgetTree,
					ToolSlotButton,
					*FString::Printf(TEXT("ToolLockedIcon_%d"), SlotIndex));
			}
			ToolSlotButton->SetToolTipText(FText::FromString(TEXT("左键拿起；右键返回原容器")));
		}
		else
		{
			ToolSlotButton->SetContent(nullptr);
		}
		AddCanvas(
			RootCanvas,
			ToolSlotButton,
			FVector2D(1408.0f + Column * 72.0f, 184.0f + Row * 76.0f),
			FVector2D(64.0f, 64.0f));
		ActionButtons.Add(ToolSlotButton);
	}
	UGameXXKDesktopTrainingActionButton* Confirm = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("ToolConfirmButton"));
	Confirm->Configure(this, 309);
	Confirm->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(170.0f, 54.0f), FMargin(0.08f)));
	Confirm->SetBackgroundColor(FLinearColor::White);
	Confirm->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("确定")), 20, Ink));
	Confirm->SetIsEnabled(GetOccupiedToolSlotCountForTest() > 0);
	AddCanvas(RootCanvas, Confirm, FVector2D(1430.0f, 450.0f), FVector2D(170.0f, 54.0f));
	ActionButtons.Add(Confirm);

	auto AddToolControl = [&](const TCHAR* Name, const int32 ActionId, const FString& Label, const float X, const float Y, const float Width)
	{
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(), Name);
		Button->Configure(this, ActionId);
		Button->SetStyle(MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(Width, 34.0f), FMargin(0.08f)));
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(MakeButtonText(WidgetTree, FText::FromString(Label), 13, Ink));
		AddCanvas(RootCanvas, Button, FVector2D(X, Y), FVector2D(Width, 34.0f));
		ActionButtons.Add(Button);
	};
	if (ActiveToolMode == EGameXXKDesktopToolMode::Combine)
	{
		AddToolControl(TEXT("ToolCombineKind"), 310,
			ActiveToolCombineKind == EGameXXKToolCombineKind::Equipment ? TEXT("装备") : TEXT("宝石"), 1388.0f, 510.0f, 72.0f);
		AddToolControl(TEXT("ToolAutoFill"), 311, TEXT("自动放置"), 1464.0f, 510.0f, 86.0f);
		const bool bIncludeWarehouse = Subsystem && Subsystem->GetRuntimeState().DesktopInventory.bToolAutoFillIncludesWarehouse;
		AddToolControl(TEXT("ToolIncludeWarehouse"), 312, bIncludeWarehouse ? TEXT("仓库✓") : TEXT("仓库×"), 1554.0f, 510.0f, 82.0f);
	}
	if (Subsystem)
	{
		const FGameXXKToolProgress& Progress = Subsystem->GetToolProgress();
		AddToolControl(TEXT("ToolCraftLevelDown"), 313, TEXT("-"), 1400.0f, 550.0f, 36.0f);
		UTextBlock* CraftLevel = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("合成等级 %d"), Progress.SelectedCraftingLevel)), 14, Ink);
		AddCanvas(RootCanvas, CraftLevel, FVector2D(1442.0f, 556.0f), FVector2D(130.0f, 24.0f));
		AddToolControl(TEXT("ToolCraftLevelUp"), 314, TEXT("+"), 1590.0f, 550.0f, 36.0f);
		if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge && Subsystem->GetRuntimeState().EquipmentCollection.PendingReforge.bActive)
		{
			AddToolControl(TEXT("ToolReforgeAccept"), 315, TEXT("采用新词缀"), 1400.0f, 590.0f, 108.0f);
			AddToolControl(TEXT("ToolReforgeKeep"), 316, TEXT("保留原词缀"), 1516.0f, 590.0f, 108.0f);
		}
		if (ActiveToolMode == EGameXXKDesktopToolMode::Socket)
		{
			AddToolControl(TEXT("ToolSocketPrevious"), 317, TEXT("孔位-"), 1400.0f, 590.0f, 72.0f);
			AddToolControl(TEXT("ToolSocketNext"), 318, FString::Printf(TEXT("孔位%d +"), SelectedToolSocketIndex + 1), 1480.0f, 590.0f, 96.0f);
		}
	}

	FString Description;
	switch (ActiveToolMode)
	{
	case EGameXXKDesktopToolMode::Dismantle: Description = TEXT("分解：放入 1~9 件装备，确认后获得金币与材料。" ); break;
	case EGameXXKDesktopToolMode::Enhance: Description = TEXT("强化：仅放入 1 件装备，消耗背包强化石。" ); break;
	case EGameXXKDesktopToolMode::Reforge: Description = TEXT("洗炼：生成付费预览后，明确选择采用或保留。" ); break;
	case EGameXXKDesktopToolMode::Combine: Description = TEXT("合成：9件同品质装备，或9个同类同品质宝石。" ); break;
	case EGameXXKDesktopToolMode::Socket: Description = TEXT("镶嵌：格0放装备、格1放宝石；替换会返还原宝石。" ); break;
	default: break;
	}
	UTextBlock* Hint = MakeText(WidgetTree, FText::FromString(Description), 16, Ink);
	AddCanvas(RootCanvas, Hint, FVector2D(1398.0f, 635.0f), FVector2D(232.0f, 110.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTrainingMapPanel()
{
	UBorder* Map = MakePanel(WidgetTree, Panel, TEXT("TrainingMapPanel"));
	AddCanvasRect(RootCanvas, Map, GameXXKDesktopTrainingLayout::GetRightShellRect());
	const FText MapTitle = FText::FromString(TEXT("历练地图"));
	UTextBlock* Title = MakeText(WidgetTree, MapTitle, 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1387.0f, 34.0f), FVector2D(255.0f, 38.0f));
	BuildPanelCloseButton(TEXT("TrainingCloseButton"), ActionCloseRightPanel, FVector2D(1602.0f, 30.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKTrainingStageDefinition> Definitions = Subsystem ? Subsystem->GetTrainingStageDefinitions() : TArray<FGameXXKTrainingStageDefinition>();
	const EGameXXKTrainingDifficulty ActiveDifficulty = FGameXXKTrainingRules::DifficultyFromStageId(SelectedStageId);
	const TArray<EGameXXKTrainingDifficulty> Difficulties = {
		EGameXXKTrainingDifficulty::Normal,
		EGameXXKTrainingDifficulty::Hard,
		EGameXXKTrainingDifficulty::Hell};
	for (int32 DifficultyIndex = 0; DifficultyIndex < Difficulties.Num(); ++DifficultyIndex)
	{
		const TCHAR* Label = DifficultyIndex == 0 ? TEXT("普通") : DifficultyIndex == 1 ? TEXT("困难") : TEXT("地狱");
		UGameXXKDesktopTrainingActionButton* DifficultyTab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("TrainingDifficultyTab_%d"), DifficultyIndex));
		DifficultyTab->Configure(this, 11 + DifficultyIndex);
		DifficultyTab->SetStyle(MakeTextureButtonStyle(
			Difficulties[DifficultyIndex] == ActiveDifficulty ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(78.0f, 36.0f),
			FMargin(0.08f)));
		DifficultyTab->SetBackgroundColor(FLinearColor::White);
		DifficultyTab->SetContent(MakeButtonText(WidgetTree, FText::FromString(Label), 18));
		AddCanvas(RootCanvas, DifficultyTab, FVector2D(1388.0f + DifficultyIndex * 84.0f, 84.0f), FVector2D(78.0f, 36.0f));
		ActionButtons.Add(DifficultyTab);
	}
	for (const FGameXXKTrainingStageDefinition& Definition : Definitions)
	{
		if (Definition.Difficulty != ActiveDifficulty)
		{
			continue;
		}
		const int32 LocalIndex = Definition.StageNumber - 1;
		const FVector2D NodePosition(1390.0f + (LocalIndex % 3) * 84.0f, 158.0f + (LocalIndex / 3) * 104.0f);
		const FVector2D NodeSize(58.0f, 58.0f);
		const FString NodeNameString = FString::Printf(TEXT("TrainingNode_%d"), Definition.StageNumber);
		const FName NodeName(*NodeNameString);
		const FText NodeLabel = FText::FromString(FString::Printf(TEXT("%d-%d"), Definition.Chapter, ((Definition.StageNumber - 1) % 3) + 1));
		const FText NodeTooltip = Subsystem ? Subsystem->BuildTrainingStageTooltip(Definition.StageId) : FText::GetEmpty();
		UGameXXKDesktopTrainingStageButton* Node = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingStageButton>(
			UGameXXKDesktopTrainingStageButton::StaticClass(),
			NodeName);
		Node->Configure(this, Definition.StageId);
		Node->SetStyle(MakeImageButtonStyle(RouteNodeTexturePath, NodeSize));
		Node->SetBackgroundColor(FLinearColor::White);
		Node->SetToolTipText(NodeTooltip);
		Node->SetContent(MakeButtonText(
			WidgetTree,
			NodeLabel,
			18,
			Definition.StageId == SelectedStageId ? FLinearColor(0.48f, 0.12f, 0.07f, 1.0f) : Ink));
		AddCanvas(RootCanvas, Node, NodePosition, NodeSize);
		StageButtons.Add(Node);
	}
	TravelStageText = MakeText(WidgetTree, FText::FromString(TEXT("当前游历关卡：未选择")), 20, FLinearColor::White);
	if (Subsystem)
	{
		const FName Current = Subsystem->GetTrainingProgressCopy().CurrentTravelStageId;
		TravelStageText->SetText(FText::FromString(FString::Printf(TEXT("当前游历关卡：%s"), *Current.ToString())));
	}
	AddCanvas(RootCanvas, TravelStageText.Get(), FVector2D(1388.0f, 640.0f), FVector2D(252.0f, 72.0f));
	UGameXXKDesktopTrainingActionButton* Challenge = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TrainingChallengeButton"));
	Challenge->Configure(this, 6);
	Challenge->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(116.0f, 58.0f), FMargin(0.08f)));
	Challenge->SetBackgroundColor(FLinearColor::White);
	Challenge->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("挑战")), 22));
	if (Subsystem)
	{
		const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
		const bool bCanChallenge = FGameXXKTrainingRules::CanChallenge(Progress, SelectedStageId);
		Challenge->SetIsEnabled(bCanChallenge);
		if (!bCanChallenge)
		{
			Challenge->SetToolTipText(FText::FromString(TEXT("需要先完成前置关卡或解锁当前难度")));
		}
	}
	AddCanvas(RootCanvas, Challenge, FVector2D(1388.0f, 828.0f), FVector2D(116.0f, 58.0f));
	ActionButtons.Add(Challenge);
	UGameXXKDesktopTrainingActionButton* Travel = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TrainingTravelButton"));
	Travel->Configure(this, 7);
	Travel->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(116.0f, 58.0f), FMargin(0.08f)));
	Travel->SetBackgroundColor(FLinearColor::White);
	Travel->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("游历")), 22));
	const bool bCanTravel = Subsystem
		&& !SelectedStageId.IsNone()
		&& FGameXXKTrainingRules::CanTravel(Subsystem->GetTrainingProgressCopy(), SelectedStageId);
	Travel->SetIsEnabled(bCanTravel);
	if (!bCanTravel)
	{
		Travel->SetToolTipText(FText::FromString(TEXT("需要先通关前置关卡或选择可游历关卡")));
	}
	AddCanvas(RootCanvas, Travel, FVector2D(1517.0f, 828.0f), FVector2D(116.0f, 58.0f));
	ActionButtons.Add(Travel);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBottomNavigation()
{
	UBorder* NavigationPanel = MakeTransparentPanel(WidgetTree, TEXT("BottomNavigationPanel"));
	AddCanvasRect(RootCanvas, NavigationPanel, GameXXKDesktopTrainingLayout::GetNavigationRect());
	const TArray<EGameXXKDesktopTrainingNav> Navs = {
		EGameXXKDesktopTrainingNav::Warehouse,
		EGameXXKDesktopTrainingNav::Formation,
		EGameXXKDesktopTrainingNav::Talents,
		EGameXXKDesktopTrainingNav::Tools,
		EGameXXKDesktopTrainingNav::Training};
	for (int32 Index = 0; Index < Navs.Num(); ++Index)
	{
		const bool bSelected = Navs[Index] == ActiveNav;
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("BottomNavigationButton_%d"), Index));
		Button->Configure(this, Index);
		Button->SetStyle(MakeInvisibleButtonStyle());
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(MakeNavigationContent(WidgetTree, Navs[Index], bSelected));
		AddCanvas(RootCanvas, Button, FVector2D(421.0f + Index * 181.0f, 800.0f), FVector2D(151.0f, 112.0f));
		ActionButtons.Add(Button);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshLayout()
{
	if (bNativeTickActive || bInActionCallback)
	{
		bLayoutRefreshPending = true;
		if (bLayoutRebuildScheduled)
		{
			return;
		}
		bLayoutRebuildScheduled = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([WeakThis = TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget>(this)]()
			{
				UGameXXKDesktopTrainingWorkbenchWidget* Widget = WeakThis.Get();
				if (!Widget)
				{
					return;
				}
				Widget->bLayoutRebuildScheduled = false;
				if (Widget->bLayoutRefreshPending)
				{
					Widget->RebuildLayoutNow();
				}
			});
		}
		else
		{
			bLayoutRebuildScheduled = false;
		}
		return;
	}
	RebuildLayoutNow();
}

void UGameXXKDesktopTrainingWorkbenchWidget::RebuildLayoutNow()
{
	bLayoutRefreshPending = false;
	TGuardValue<bool> InternalLayoutRebuildGuard(bInternalLayoutRebuild, true);
	const bool bWasInViewport = IsInViewport();
	const ESlateVisibility PreviousVisibility = GetVisibility();
	if (bWasInViewport)
	{
		// WidgetTree children are rebuilt for workbench navigation changes. A live
		// UUserWidget otherwise keeps the old Slate tree, so detach and release the
		// cached Slate resource before attaching the new tree.
		RemoveFromParent();
		ReleaseSlateResources(true);
	}
	BuildProgrammaticLayout();
	if (bWasInViewport)
	{
		AddToViewport(200);
		// Reattaching can restore the Slate tree with its default collapsed
		// visibility. Preserve the caller's visible/collapsed state.
		SetVisibility(PreviousVisibility);
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::CaptureExpandedSessionState()
{
	if (!EmbeddedInventoryWidget)
	{
		return;
	}
	SavedEmbeddedInventorySession = EmbeddedInventoryWidget->CaptureEmbeddedSessionState();
	bHasSavedEmbeddedInventorySession = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::PreserveEmbeddedSessionForLocalClose()
{
	if (bBackpackExpanded
		&& ActiveCenterPage == EGameXXKDesktopTrainingCenterPage::Backpack)
	{
		CaptureExpandedSessionState();
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::ScheduleCollapsedResourceUnload()
{
	CollapsedResourceUnloadRemainingSeconds = CollapsedResourceUnloadDelaySeconds;
	bCollapsedResourceUnloadPending = true;
	bCollapsedResourcesReleased = false;
	++CollapsedResourceGeneration;
}

void UGameXXKDesktopTrainingWorkbenchWidget::CancelCollapsedResourceUnload()
{
	bCollapsedResourceUnloadPending = false;
	CollapsedResourceUnloadRemainingSeconds = 0.0f;
	++CollapsedResourceGeneration;
}

void UGameXXKDesktopTrainingWorkbenchWidget::TickCollapsedResourceUnload(const float InDeltaTime)
{
	if (!bCollapsedResourceUnloadPending
		|| bBackpackExpanded)
	{
		return;
	}
	CollapsedResourceUnloadRemainingSeconds = FMath::Max(
		0.0f,
		CollapsedResourceUnloadRemainingSeconds - FMath::Max(0.0f, InDeltaTime));
	if (CollapsedResourceUnloadRemainingSeconds <= KINDA_SMALL_NUMBER)
	{
		ReleaseCollapsedResources();
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReleaseCollapsedResources()
{
	if (!bCollapsedResourceUnloadPending
		|| bBackpackExpanded
		|| bCollapsedResourcesReleased)
	{
		return;
	}
	bCollapsedResourceUnloadPending = false;
	CollapsedResourceUnloadRemainingSeconds = 0.0f;
	bCollapsedResourcesReleased = true;
	++CollapsedGcRequestCount;
	++CollapsedResourceGeneration;

	// The collapsed rebuild has already detached the backpack/warehouse/map/tool
	// tree. The travel cache/session deliberately stays pinned so the always-on
	// strip never stalls or restarts.
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(false);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopBackpackSlotLeftClicked(const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	return CarriedEntry.IsValid()
		? DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer::Backpack, SlotIndex)
		: PickUpDesktopEntry(EGameXXKDesktopItemContainer::Backpack, SlotIndex);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopBackpackSlotRightClicked(const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (CarriedEntry.IsValid())
	{
		const bool bCancelled = CancelCarriedItem();
		if (bCancelled)
		{
			RefreshLayout();
		}
		return bCancelled;
	}
	return RouteBackpackRightClick(SlotIndex);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopEquipmentSlotLeftClicked(
	const EGameXXKEquipmentSlot EquipmentSlot)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (!CarriedEntry.IsValid())
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem
		|| EquipmentSlot == EGameXXKEquipmentSlot::Invalid
		|| !CarriedEntry.Payload.Entry.bEquipmentInstance)
	{
		SetNotice(FText::FromString(TEXT("仅装备实例可放入角色装备格")));
		return false;
	}

	FGameXXKEquipmentTransactionResult Result;
	if (!Subsystem->EquipEquipmentFromDesktopCell(
		GetActiveBackpackCharacterIdForTest(),
		EquipmentSlot,
		CarriedEntry.Payload.AuthoritativeContainer,
		CarriedEntry.Payload.AuthoritativeSlotIndex,
		CarriedEntry.Payload.Entry.EntryId,
		Result))
	{
		SetNotice(Result.Message.IsEmpty()
			? FText::FromString(TEXT("该装备无法放入目标装备格"))
			: Result.Message);
		return false;
	}

	CarriedEntry.Reset();
	SetNotice(Result.Message);
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleDesktopEntryLock(
	const FGameXXKDesktopInventoryEntryKey& Entry)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Entry.IsValid())
	{
		SetNotice(FText::FromString(TEXT("空格没有可锁定的物品")));
		return false;
	}
	const bool bLock = !FGameXXKDesktopInventoryRules::IsEntryLocked(
		Subsystem->GetRuntimeState(),
		Entry);
	FString Error;
	if (!FGameXXKDesktopInventoryRules::SetEntryLocked(
		Subsystem->GetMutableRuntimeState(),
		Entry,
		bLock,
		&Error))
	{
		SetNotice(FText::FromString(
			Error.IsEmpty() ? TEXT("无法切换物品锁定状态") : Error));
		return false;
	}
	SetNotice(FText::FromString(bLock ? TEXT("已锁定物品") : TEXT("已解除物品锁定")));
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopSlotAltClicked(
	const EGameXXKDesktopItemContainer Container,
	const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	return ToggleDesktopEntryLock(FGameXXKDesktopInventoryRules::GetEntryAt(
		Subsystem->GetRuntimeState(),
		Container,
		SlotIndex));
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopEquipmentSlotAltClicked(
	const EGameXXKEquipmentSlot EquipmentSlot)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || EquipmentSlot == EGameXXKEquipmentSlot::Invalid)
	{
		return false;
	}
	const FGameXXKEquipmentLoadout* Loadout =
		Subsystem->GetRuntimeState().EquipmentCollection.CharacterLoadouts.Find(
			GetActiveBackpackCharacterIdForTest());
	const FName InstanceId = Loadout
		? FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*Loadout, EquipmentSlot)
		: NAME_None;
	return ToggleDesktopEntryLock(
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopToolSlotAltClicked(
	const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	return ToolSlots.IsValidIndex(SlotIndex)
		? ToggleDesktopEntryLock(ToolSlots[SlotIndex].Entry)
		: false;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopCarryRightClicked()
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (!CarriedEntry.IsValid())
	{
		return false;
	}
	const bool bCancelled = CancelCarriedItem();
	if (bCancelled)
	{
		RefreshLayout();
	}
	return bCancelled;
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopCharacterSubpageClicked(
	const EGameXXKCharacterBackpackTab Tab)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (EmbeddedInventoryWidget)
	{
		SavedEmbeddedInventorySession = EmbeddedInventoryWidget->CaptureEmbeddedSessionState();
	}
	else
	{
		SavedEmbeddedInventorySession = FGameXXKEmbeddedInventorySessionState();
		SavedEmbeddedInventorySession.CharacterId = GetActiveBackpackCharacterIdForTest();
	}
	SavedEmbeddedInventorySession.ActiveCharacterTab = Tab;
	bHasSavedEmbeddedInventorySession = true;
	CancelCarryForStructuralChange();
	RefreshLayout();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ShouldHideDesktopInventoryEntry(
	const EGameXXKDesktopItemContainer Container,
	const FGameXXKDesktopInventoryEntryKey& Entry) const
{
	if (!Entry.IsValid())
	{
		return false;
	}
	if (CarriedEntry.IsValid()
		&& CarriedEntry.Payload.AuthoritativeContainer == Container
		&& CarriedEntry.Payload.Entry == Entry)
	{
		return true;
	}
	for (const FDesktopToolEntry& ToolEntry : ToolSlots)
	{
		if (ToolEntry.IsValid()
			&& ToolEntry.AuthoritativeContainer == Container
			&& ToolEntry.Entry == Entry)
		{
			return true;
		}
	}
	return false;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PickUpDesktopEntry(
	const EGameXXKDesktopItemContainer Container,
	const int32 SlotIndex)
{
	if (CarriedEntry.IsValid() || !bBackpackExpanded)
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->NormalizeDesktopInventoryState())
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(State, Container, SlotIndex);
	if (!Entry.IsValid() || ShouldHideDesktopInventoryEntry(Container, Entry))
	{
		return false;
	}

	CarriedEntry.Reset();
	CarriedEntry.Payload.Entry = Entry;
	CarriedEntry.Payload.AuthoritativeContainer = Container;
	CarriedEntry.Payload.AuthoritativeSlotIndex = SlotIndex;
	if (Entry.bEquipmentInstance)
	{
		CarriedEntry.Payload.Quantity = 1;
		CarriedEntry.Payload.IconPath = EquipmentIconTexturePath(State.EquipmentCollection, Entry.EntryId);
	}
	else
	{
		CarriedEntry.Payload.Quantity = Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems.FindRef(Entry.EntryId)
			: State.Inventory.FindRef(Entry.EntryId);
		CarriedEntry.Payload.IconPath = InventoryItemIconTexturePath(Entry.EntryId);
	}
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PickUpToolEntry(const int32 SlotIndex)
{
	if (CarriedEntry.IsValid() || !ToolSlots.IsValidIndex(SlotIndex) || !ToolSlots[SlotIndex].IsValid())
	{
		return false;
	}
	CarriedEntry.Reset();
	CarriedEntry.Payload = ToolSlots[SlotIndex];
	CarriedEntry.bOriginIsTool = true;
	CarriedEntry.OriginToolSlotIndex = SlotIndex;
	ToolSlots[SlotIndex] = FDesktopToolEntry();
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::DropCarriedOnDesktopSlot(
	const EGameXXKDesktopItemContainer Container,
	const int32 SlotIndex)
{
	if (!CarriedEntry.IsValid())
	{
		return PickUpDesktopEntry(Container, SlotIndex);
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || SlotIndex < 0 || SlotIndex >= FGameXXKDesktopInventoryRules::BackpackCapacity)
	{
		return false;
	}
	for (int32 ToolSlotIndex = 0; ToolSlotIndex < ToolSlots.Num(); ++ToolSlotIndex)
	{
		const FDesktopToolEntry& ReservedEntry = ToolSlots[ToolSlotIndex];
		if (ReservedEntry.IsValid()
			&& ReservedEntry.AuthoritativeContainer == Container
			&& ReservedEntry.AuthoritativeSlotIndex == SlotIndex)
		{
			SetNotice(FText::FromString(TEXT("目标格已被另一工具输入占用；当前道具继续吸附在鼠标上")));
			return false;
		}
	}
	const bool bSameOrigin = Container == CarriedEntry.Payload.AuthoritativeContainer
		&& SlotIndex == CarriedEntry.Payload.AuthoritativeSlotIndex;
	if (bSameOrigin && !CarriedEntry.bOriginIsTool)
	{
		CarriedEntry.Reset();
		RefreshLayout();
		return true;
	}

	FGameXXKDesktopInventoryMoveRequest Request;
	Request.FromContainer = CarriedEntry.Payload.AuthoritativeContainer;
	Request.FromSlotIndex = CarriedEntry.Payload.AuthoritativeSlotIndex;
	Request.ToContainer = Container;
	Request.ToSlotIndex = SlotIndex;
	Request.bAllowSwap = true;
	Request.ExpectedEntry = CarriedEntry.Payload.Entry;
	FString Error;
	if (!FGameXXKDesktopInventoryRules::MoveOrSwap(
		Subsystem->GetMutableRuntimeState(),
		Request,
		&Error))
	{
		SetNotice(FText::FromString(Error.IsEmpty() ? TEXT("该格无法放置道具") : Error));
		return false;
	}
	CarriedEntry.Reset();
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::DropCarriedOnToolSlot(const int32 SlotIndex)
{
	ToolSlots.SetNum(ToolSlotCount);
	if (!CarriedEntry.IsValid() || !ToolSlots.IsValidIndex(SlotIndex))
	{
		return false;
	}
	if (CarriedEntry.bOriginIsTool
		&& SlotIndex == CarriedEntry.OriginToolSlotIndex)
	{
		const bool bCancelled = CancelCarriedItem();
		if (bCancelled)
		{
			RefreshLayout();
		}
		return bCancelled;
	}
	const bool bEquipment = CarriedEntry.Payload.Entry.bEquipmentInstance;
	bool bShapeValid = true;
	if (ActiveToolMode == EGameXXKDesktopToolMode::Dismantle)
	{
		bShapeValid = bEquipment;
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Combine)
	{
		bShapeValid = ActiveToolCombineKind == EGameXXKToolCombineKind::Equipment
			? bEquipment : (!bEquipment && SlotIndex == 0);
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Enhance || ActiveToolMode == EGameXXKDesktopToolMode::Reforge)
	{
		bShapeValid = bEquipment && SlotIndex == 0;
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Socket)
	{
		bShapeValid = (SlotIndex == 0 && bEquipment) || (SlotIndex == 1 && !bEquipment);
	}
	if (!bShapeValid)
	{
		SetNotice(FText::FromString(TEXT("该道具不符合当前工具格要求；仍吸附在鼠标上")));
		return false;
	}
	if ((ActiveToolMode == EGameXXKDesktopToolMode::Dismantle || ActiveToolMode == EGameXXKDesktopToolMode::Combine)
		&& ResolveMVPSubsystem()
		&& FGameXXKDesktopInventoryRules::IsEntryLocked(ResolveMVPSubsystem()->GetRuntimeState(), CarriedEntry.Payload.Entry))
	{
		SetNotice(FText::FromString(TEXT("锁定道具不能用于分解或合成")));
		return false;
	}

	const FDesktopToolEntry DisplacedEntry = ToolSlots[SlotIndex];
	if (CarriedEntry.bOriginIsTool)
	{
		if (!ToolSlots.IsValidIndex(CarriedEntry.OriginToolSlotIndex)
			|| ToolSlots[CarriedEntry.OriginToolSlotIndex].IsValid())
		{
			SetNotice(FText::FromString(TEXT("原工具格已变化；当前道具继续吸附在鼠标上")));
			return false;
		}
		ToolSlots[CarriedEntry.OriginToolSlotIndex] = DisplacedEntry;
	}
	ToolSlots[SlotIndex] = CarriedEntry.Payload;
	CarriedEntry.Reset();
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RouteBackpackRightClick(const int32 SlotIndex)
{
	if (CarriedEntry.IsValid())
	{
		const bool bCancelled = CancelCarriedItem();
		if (bCancelled)
		{
			RefreshLayout();
		}
		return bCancelled;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	if (bWarehousePanelOpen)
	{
		const int32 DestinationSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse);
		FString Error;
		if (DestinationSlot != INDEX_NONE
			&& Subsystem->MoveDesktopInventoryEntry(
				EGameXXKDesktopItemContainer::Backpack,
				SlotIndex,
				EGameXXKDesktopItemContainer::Warehouse,
				DestinationSlot,
				&Error))
		{
			RefreshLayout();
			return true;
		}
		SetNotice(FText::FromString(Error.IsEmpty() ? TEXT("仓库已满") : Error));
		return false;
	}
	if (RightPanel == EGameXXKDesktopTrainingRightPanel::Tools)
	{
		ToolSlots.SetNum(ToolSlotCount);
		const int32 ToolSlot = ToolSlots.IndexOfByPredicate([](const FDesktopToolEntry& Entry)
		{
			return !Entry.IsValid();
		});
		return ToolSlot != INDEX_NONE
			&& PickUpDesktopEntry(EGameXXKDesktopItemContainer::Backpack, SlotIndex)
			&& DropCarriedOnToolSlot(ToolSlot);
	}

	const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		SlotIndex);
	if (Entry.bEquipmentInstance && EmbeddedInventoryWidget)
	{
		const bool bEquipped = EmbeddedInventoryWidget->QuickEquipBackpackInstanceForTest(Entry.EntryId);
		if (bEquipped)
		{
			Subsystem->NormalizeDesktopInventoryState();
			RefreshLayout();
		}
		return bEquipped;
	}
	return false;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CancelCarriedItem()
{
	if (!CarriedEntry.IsValid())
	{
		return false;
	}
	if (CarriedEntry.bOriginIsTool)
	{
		ToolSlots.SetNum(ToolSlotCount);
		if (ToolSlots.IsValidIndex(CarriedEntry.OriginToolSlotIndex)
			&& !ToolSlots[CarriedEntry.OriginToolSlotIndex].IsValid())
		{
			ToolSlots[CarriedEntry.OriginToolSlotIndex] = CarriedEntry.Payload;
		}
	}
	CarriedEntry.Reset();
	return true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::CancelCarryForStructuralChange()
{
	AbortTransientInventoryInteraction(false, false);
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReturnAllToolEntries()
{
	ToolSlots.Empty();
	ToolSlots.SetNum(ToolSlotCount);
}

void UGameXXKDesktopTrainingWorkbenchWidget::CloseWarehousePanelToParent()
{
	PreserveEmbeddedSessionForLocalClose();
	CancelCarryForStructuralChange();
	bWarehousePanelOpen = false;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Warehouse)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::CloseCentralPageToBackpack()
{
	PreserveEmbeddedSessionForLocalClose();
	CancelCarryForStructuralChange();
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Formation
		|| ActiveNav == EGameXXKDesktopTrainingNav::Talents)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::CloseRightPanelToParent()
{
	PreserveEmbeddedSessionForLocalClose();
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Tools
		|| ActiveNav == EGameXXKDesktopTrainingNav::Training)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::ResetWorkbenchChildrenForGlobalClose()
{
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	bWarehousePanelOpen = false;
	ActiveBackpackCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	ActiveCharacterRoster = EGameXXKDesktopTrainingCharacterRoster::Hero;
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	ActiveNav = EGameXXKDesktopTrainingNav::None;
	bHasSavedEmbeddedInventorySession = false;
}

void UGameXXKDesktopTrainingWorkbenchWidget::AbortTransientInventoryInteraction(
	const bool bReturnToolEntries,
	const bool bRefreshLayout)
{
	const bool bHadCarriedEntry = CancelCarriedItem();
	bool bHadToolEntries = false;
	if (bReturnToolEntries)
	{
		bHadToolEntries = GetOccupiedToolSlotCountForTest() > 0;
		ReturnAllToolEntries();
	}
	if (bRefreshLayout && (bHadCarriedEntry || bHadToolEntries))
	{
		RefreshLayout();
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleApplicationActivationChanged(const bool bIsActive)
{
	if (!bIsActive)
	{
		AbortTransientInventoryInteraction(false, true);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleWorkbenchRightMouseCancel()
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (!CarriedEntry.IsValid())
	{
		return false;
	}
	AbortTransientInventoryInteraction(false, true);
	return true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandlePersistenceBoundary()
{
	AbortTransientInventoryInteraction(true, true);
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateCarriedItemVisualPosition()
{
	if (!CarriedEntry.IsValid() || !CarriedItemImage || !GEngine || !GEngine->GameViewport)
	{
		return;
	}
	APlayerController* PlayerController = GetOwningPlayer();
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	if (!PlayerController || !PlayerController->GetMousePosition(MouseX, MouseY) || ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		return;
	}
	const GameXXKDesktopTrainingLayout::FFitTransform Fit = GameXXKDesktopTrainingLayout::MakeFitTransform(ViewportSize);
	if (Fit.Scale <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector2D ReferencePosition = (FVector2D(MouseX, MouseY) - Fit.Offset) / Fit.Scale - FVector2D(28.0f, 28.0f);
	if (UCanvasPanelSlot* CarriedCanvasSlot = Cast<UCanvasPanelSlot>(CarriedItemImage->Slot))
	{
		CarriedCanvasSlot->SetPosition(ReferencePosition);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleAlwaysOnTop()
{
	bAlwaysOnTop = !bAlwaysOnTop;
#if PLATFORM_WINDOWS
	if (!GIsEditor && FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(TakeWidget());
		const TSharedPtr<FGenericWindow> NativeWindow = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
		HWND WindowHandle = NativeWindow.IsValid()
			? static_cast<HWND>(NativeWindow->GetOSWindowHandle())
			: nullptr;
		if (WindowHandle)
		{
			::SetWindowPos(
				WindowHandle,
				bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
				0,
				0,
				0,
				0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}
	}
#endif
	SetNotice(FText::FromString(bAlwaysOnTop ? TEXT("窗口已保持最顶层") : TEXT("窗口已取消最顶层")));
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleMuted()
{
	if (!bMuted)
	{
		UnmutedVolumeMultiplier = FMath::Max(0.01f, FApp::GetVolumeMultiplier());
		FApp::SetVolumeMultiplier(0.0f);
		bMuted = true;
	}
	else
	{
		FApp::SetVolumeMultiplier(UnmutedVolumeMultiplier);
		bMuted = false;
	}
	SetNotice(FText::FromString(bMuted ? TEXT("已静音") : TEXT("声音已恢复")));
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RequestExit()
{
	AbortTransientInventoryInteraction(true, false);
	bExitConfirmationOpen = true;
	RefreshLayout();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ConfirmExit(const bool bExecutePlatformQuit)
{
	if (!bExitConfirmationOpen)
	{
		return false;
	}
	bExitConfirmationOpen = false;
	if (bExecutePlatformQuit)
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
	}
	else
	{
		RefreshLayout();
	}
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ConfirmToolForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || CarriedEntry.IsValid())
	{
		return false;
	}
	const FGameXXKRuntimeState& RuntimeState = Subsystem->GetRuntimeState();
	for (const FDesktopToolEntry& ReservedEntry : ToolSlots)
	{
		if (!ReservedEntry.IsValid())
		{
			continue;
		}
		const bool bValidContainer =
			ReservedEntry.AuthoritativeContainer == EGameXXKDesktopItemContainer::Backpack
			|| ReservedEntry.AuthoritativeContainer == EGameXXKDesktopItemContainer::Warehouse;
		const bool bValidSlot = ReservedEntry.AuthoritativeSlotIndex >= 0
			&& ReservedEntry.AuthoritativeSlotIndex < FGameXXKDesktopInventoryRules::BackpackCapacity;
		const FGameXXKDesktopInventoryEntryKey PhysicalEntry = bValidContainer && bValidSlot
			? FGameXXKDesktopInventoryRules::GetEntryAt(
				RuntimeState,
				ReservedEntry.AuthoritativeContainer,
				ReservedEntry.AuthoritativeSlotIndex)
			: FGameXXKDesktopInventoryEntryKey();
		bool bExactAuthority = PhysicalEntry == ReservedEntry.Entry;
		if (bExactAuthority && ReservedEntry.Entry.bEquipmentInstance)
		{
			bExactAuthority = ReservedEntry.Quantity == 1
				&& RuntimeState.EquipmentCollection.WarehouseInstanceIds.Contains(
					ReservedEntry.Entry.EntryId)
				&& FGameXXKEquipmentRules::FindInstance(
					RuntimeState.EquipmentCollection,
					ReservedEntry.Entry.EntryId) != nullptr;
		}
		else if (bExactAuthority)
		{
			const int32 AuthoritativeQuantity =
				ReservedEntry.AuthoritativeContainer == EGameXXKDesktopItemContainer::Warehouse
					? RuntimeState.DesktopInventory.WarehouseItems.FindRef(ReservedEntry.Entry.EntryId)
					: RuntimeState.Inventory.FindRef(ReservedEntry.Entry.EntryId);
			bExactAuthority = AuthoritativeQuantity > 0
				&& AuthoritativeQuantity == ReservedEntry.Quantity;
		}
		if (!bExactAuthority)
		{
			SetNotice(FText::FromString(TEXT("工具输入来源已变化；未执行操作且未消耗任何道具")));
			return false;
		}
	}
	TArray<FGameXXKToolInputRef> Inputs;
	for (const FDesktopToolEntry& Entry : ToolSlots)
	{
		if (!Entry.IsValid())
		{
			continue;
		}
		FGameXXKToolInputRef& Input = Inputs.AddDefaulted_GetRef();
		Input.Container = Entry.AuthoritativeContainer;
		Input.SlotIndex = Entry.AuthoritativeSlotIndex;
		Input.ExpectedEntry = Entry.Entry;
	}
	if (Inputs.IsEmpty())
	{
		SetNotice(FText::FromString(TEXT("请先放入道具")));
		return false;
	}

	FGameXXKEquipmentTransactionResult Result;
	bool bSucceeded = false;
	bool bKeepReservations = false;
	if (ActiveToolMode == EGameXXKDesktopToolMode::Dismantle)
	{
		bSucceeded = Subsystem->ExecuteToolDismantle(Inputs, true, Result);
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Combine)
	{
		bSucceeded = Subsystem->ExecuteToolCombine(ActiveToolCombineKind, Inputs, Result);
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Socket)
	{
		if (!ToolSlots.IsValidIndex(0) || !ToolSlots.IsValidIndex(1)
			|| !ToolSlots[0].IsValid() || !ToolSlots[1].IsValid()
			|| !ToolSlots[0].Entry.bEquipmentInstance || ToolSlots[1].Entry.bEquipmentInstance)
		{
			SetNotice(FText::FromString(TEXT("镶嵌要求格0为装备、格1为宝石")));
			return false;
		}
		FGameXXKSocketGemRequest Request;
		Request.EquipmentInput = {ToolSlots[0].AuthoritativeContainer, ToolSlots[0].AuthoritativeSlotIndex, ToolSlots[0].Entry};
		Request.GemInput = {ToolSlots[1].AuthoritativeContainer, ToolSlots[1].AuthoritativeSlotIndex, ToolSlots[1].Entry};
		Request.SocketIndex = SelectedToolSocketIndex;
		bSucceeded = Subsystem->ExecuteToolSocket(Request, Result);
	}
	else if (Inputs.Num() != 1 || !Inputs[0].ExpectedEntry.bEquipmentInstance)
	{
		SetNotice(FText::FromString(TEXT("强化和洗炼一次只能放入一件装备")));
		return false;
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Enhance)
	{
		bSucceeded = Subsystem->ExecuteToolEnhance(Inputs[0], Result);
	}
	else if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge)
	{
		if (Subsystem->GetRuntimeState().EquipmentCollection.PendingReforge.bActive)
		{
			SetNotice(FText::FromString(TEXT("请选择采用新词缀或保留原词缀")));
			return false;
		}
		bSucceeded = Subsystem->ExecuteToolBeginReforge(Inputs[0], 0, Result);
		bKeepReservations = bSucceeded;
	}
	if (!bSucceeded)
	{
		SetNotice(Result.Message.IsEmpty() ? FText::FromString(TEXT("工具执行失败；未改变输入道具")) : Result.Message);
		return false;
	}
	if (!bKeepReservations)
	{
		ReturnAllToolEntries();
	}
	Subsystem->NormalizeDesktopInventoryState();
	SetNotice(bKeepReservations
		? FText::FromString(TEXT("洗炼预览已生成，请选择采用或保留"))
		: (Result.Message.IsEmpty() ? FText::FromString(TEXT("工具执行完成")) : Result.Message));
	RefreshLayout();
	return true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyAction(const int32 ActionId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (bExitConfirmationOpen && ActionId != 53 && ActionId != 54)
	{
		return;
	}
	if (ActionId == ActionCloseWarehouse)
	{
		CloseWarehousePanelToParent();
		return;
	}
	if (ActionId == ActionCloseCentralPage)
	{
		CloseCentralPageToBackpack();
		return;
	}
	if (ActionId == ActionCloseRightPanel)
	{
		CloseRightPanelToParent();
		return;
	}
	if (ActionId == 0 && bWarehousePanelOpen)
	{
		CloseWarehousePanelToParent();
		return;
	}
	if (ActionId >= 0 && ActionId <= 4)
	{
		CancelCarryForStructuralChange();
		bBackpackExpanded = true;
		bSettingsPanelOpen = false;
		if (ActionId == 0)
		{
			bWarehousePanelOpen = !bWarehousePanelOpen;
			ActiveNav = bWarehousePanelOpen
				? EGameXXKDesktopTrainingNav::Warehouse
				: EGameXXKDesktopTrainingNav::None;
		}
		else if (ActionId == 1)
		{
			ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Formation;
			ActiveNav = EGameXXKDesktopTrainingNav::Formation;
			EnsureFormationCandidate();
		}
		else if (ActionId == 2)
		{
			ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Talents;
			ActiveNav = EGameXXKDesktopTrainingNav::Talents;
		}
		else if (ActionId == 3)
		{
			ActiveNav = EGameXXKDesktopTrainingNav::Tools;
			RightPanel = EGameXXKDesktopTrainingRightPanel::Tools;
		}
		else
		{
			if (RightPanel == EGameXXKDesktopTrainingRightPanel::Tools)
			{
				ReturnAllToolEntries();
			}
			ActiveNav = EGameXXKDesktopTrainingNav::Training;
			RightPanel = EGameXXKDesktopTrainingRightPanel::TrainingMap;
		}
		RefreshLayout();
		return;
	}
	if (ActionId >= 20 && ActionId < 20 + GetBackpackCharacterIdsForTest().Num())
	{
		SelectBackpackCharacterForTest(GetBackpackCharacterIdsForTest()[ActionId - 20]);
		return;
	}
	if (ActionId >= 80 && ActionId <= 82)
	{
		const EGameXXKDesktopTrainingCharacterRoster RequestedRoster =
			static_cast<EGameXXKDesktopTrainingCharacterRoster>(ActionId - 80);
		SelectBackpackCharacterForTest(
			ResolveRosterRepresentativeCharacterId(RequestedRoster));
		return;
	}
	if (ActionId == 83 || ActionId == 84)
	{
		CancelCarryForStructuralChange();
		ActiveFormationRoster = ActionId == 83
			? EGameXXKDesktopTrainingCharacterRoster::Companions
			: EGameXXKDesktopTrainingCharacterRoster::Npcs;
		FormationCandidateCharacterId = NAME_None;
		EnsureFormationCandidate();
		RefreshLayout();
		return;
	}
	if (ActionId == 85)
	{
		ApplyFormationCandidateForTest();
		return;
	}
	if (ActionId >= 400 && ActionId < 406)
	{
		const TArray<FName> CompanionIds = GetCompanionCharacterIdsForTest();
		const int32 Index = ActionId - 400;
		if (CompanionIds.IsValidIndex(Index))
		{
			SelectBackpackCharacterForTest(CompanionIds[Index]);
		}
		return;
	}
	if (ActionId >= 420 && ActionId < 426)
	{
		const TArray<FName> NpcIds = GetNpcCharacterIdsForTest();
		const int32 Index = ActionId - 420;
		if (NpcIds.IsValidIndex(Index))
		{
			SelectBackpackCharacterForTest(NpcIds[Index]);
		}
		return;
	}
	if (ActionId >= 440 && ActionId < 446)
	{
		const TArray<FName> CompanionIds = GetCompanionCharacterIdsForTest();
		const int32 Index = ActionId - 440;
		if (CompanionIds.IsValidIndex(Index))
		{
			SelectFormationCandidateForTest(CompanionIds[Index]);
		}
		return;
	}
	if (ActionId >= 460 && ActionId < 466)
	{
		const TArray<FName> NpcIds = GetNpcCharacterIdsForTest();
		const int32 Index = ActionId - 460;
		if (NpcIds.IsValidIndex(Index))
		{
			SelectFormationCandidateForTest(NpcIds[Index]);
		}
		return;
	}
	if (ActionId == 40)
	{
		CancelCarryForStructuralChange();
		PreviousWarehousePageForTest();
		return;
	}
	if (ActionId == 41)
	{
		CancelCarryForStructuralChange();
		NextWarehousePageForTest();
		return;
	}
	if (ActionId >= 70 && ActionId <= 72)
	{
		CancelCarryForStructuralChange();
		const int32 RequestedPage = ActionId - 70;
		if (RequestedPage < GetWarehousePageCountForTest())
		{
			WarehousePageIndex = RequestedPage;
			RefreshLayout();
		}
		return;
	}
	if (ActionId == 73)
	{
		SetNotice(FText::FromString(TEXT("仓库扩展页将在容量扩展后启用")));
		return;
	}
	if (ActionId >= 100 && ActionId < 100 + WarehousePageSize)
	{
		const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + (ActionId - 100);
		if (CarriedEntry.IsValid())
		{
			DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer::Warehouse, PhysicalSlotIndex);
		}
		else
		{
			PickUpDesktopEntry(EGameXXKDesktopItemContainer::Warehouse, PhysicalSlotIndex);
		}
		return;
	}
	if (ActionId >= 300 && ActionId < 300 + ToolSlotCount)
	{
		const int32 ToolSlotIndex = ActionId - 300;
		if (CarriedEntry.IsValid())
		{
			DropCarriedOnToolSlot(ToolSlotIndex);
		}
		else
		{
			PickUpToolEntry(ToolSlotIndex);
		}
		return;
	}
	if (ActionId >= 200 && ActionId < 206)
	{
		QuickUnequipActiveBackpackSlotForTest(ActionId - 200);
		return;
	}
	if (ActionId >= 11 && ActionId <= 13)
	{
		CancelCarryForStructuralChange();
		ReturnAllToolEntries();
		const EGameXXKTrainingDifficulty Difficulty = static_cast<EGameXXKTrainingDifficulty>(ActionId - 11);
		SelectedStageId = FGameXXKTrainingRules::MakeStageId(Difficulty, 1);
		Subsystem->SelectTrainingStage(SelectedStageId);
		RefreshLayout();
		return;
	}
	switch (ActionId)
	{
	case 5:
		CancelCarryForStructuralChange();
		SortWarehouseForTest();
		break;
	case 6:
		CancelCarryForStructuralChange();
		ReturnAllToolEntries();
		if (Subsystem->StartTrainingChallenge(SelectedStageId))
		{
			bSettingsPanelOpen = false;
			bExitConfirmationOpen = false;
			CloseWorkbench();
			NotifyPlayerFlowStateChanged();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("当前关卡尚未解锁，或已有挑战正在进行")));
		}
		break;
	case 7:
		CancelCarryForStructuralChange();
		if (Subsystem->StartTrainingTravel(SelectedStageId))
		{
			SetNotice(FText::FromString(TEXT("开始游历：走动、遭遇、自动战斗、结算后循环")));
			RefreshLayout();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("未通关关卡不能游历")));
		}
		break;
	case 10:
		Subsystem->SetTrainingRetryOnFailure(!Subsystem->GetTrainingProgressCopy().bRetryOnFailure);
		SetNotice(FText::FromString(TEXT("已切换游历失败重试策略")));
		break;
	case 16:
		CollectTravelRewardsForTest();
		break;
	case 61:
		CancelCarryForStructuralChange();
		ReturnAllToolEntries();
		if (Subsystem->NormalizeDesktopInventoryState())
		{
			FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
			TArray<FGameXXKDesktopInventoryEntryKey> Entries;
			for (const FGameXXKDesktopInventoryEntryKey& Entry : State.DesktopInventory.BackpackSlots)
			{
				if (Entry.IsValid())
				{
					Entries.Add(Entry);
				}
			}
			Entries.Sort([](const FGameXXKDesktopInventoryEntryKey& Left, const FGameXXKDesktopInventoryEntryKey& Right)
			{
				if (Left.bEquipmentInstance != Right.bEquipmentInstance)
				{
					return Left.bEquipmentInstance;
				}
				return Left.EntryId.LexicalLess(Right.EntryId);
			});
			State.DesktopInventory.BackpackSlots.Init(FGameXXKDesktopInventoryEntryKey(), FGameXXKDesktopInventoryRules::BackpackCapacity);
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				State.DesktopInventory.BackpackSlots[Index] = Entries[Index];
			}
			SetNotice(FText::FromString(TEXT("背包已按已确认规则排序")));
			RefreshLayout();
		}
		break;
	case 14:
		ToggleAlwaysOnTop();
		break;
	case 15:
		RequestExit();
		break;
	case 30:
	case 31:
	case 32:
	case 33:
	case 34:
		SetToolModeForTest(static_cast<EGameXXKDesktopToolMode>(ActionId - 30));
		break;
	case 17:
		ToggleMuted();
		break;
	case 18:
		SetNotice(FText::FromString(TEXT("邮件功能尚未开放")));
		break;
	case 19:
		SetNotice(FText::FromString(TEXT("商店功能尚未开放")));
		break;
	case 53:
		CancelExitForTest();
		break;
	case 54:
		ConfirmExit(true);
		break;
	case 60:
		if (bBackpackExpanded)
		{
			ResetWorkbenchChildrenForGlobalClose();
			bBackpackExpanded = false;
			bExitConfirmationOpen = false;
			RefreshLayout();
			// Do not unload collapsed resources: reopening must stay safe and
			// fast. Only hide the expanded UI; keep textures/atlases alive.
		}
		else
		{
			OpenBackpack();
		}
		break;
	case 310:
		ActiveToolCombineKind = ActiveToolCombineKind == EGameXXKToolCombineKind::Equipment
			? EGameXXKToolCombineKind::Gem : EGameXXKToolCombineKind::Equipment;
		ReturnAllToolEntries();
		RefreshLayout();
		break;
	case 311:
		if (Subsystem)
		{
			TArray<FGameXXKToolInputRef> Inputs;
			FString Error;
			if (!Subsystem->BuildToolCombineAutoFill(
				ActiveToolCombineKind,
				Subsystem->GetRuntimeState().DesktopInventory.bToolAutoFillIncludesWarehouse,
				Inputs,
				&Error))
			{
				SetNotice(FText::FromString(Error));
				break;
			}
			ReturnAllToolEntries();
			for (int32 Index = 0; Index < Inputs.Num() && ToolSlots.IsValidIndex(Index); ++Index)
			{
				const FGameXXKToolInputRef& Input = Inputs[Index];
				FDesktopToolEntry& Entry = ToolSlots[Index];
				Entry.Entry = Input.ExpectedEntry;
				Entry.AuthoritativeContainer = Input.Container;
				Entry.AuthoritativeSlotIndex = Input.SlotIndex;
				Entry.Quantity = Input.ExpectedEntry.bEquipmentInstance ? 1
					: (Input.Container == EGameXXKDesktopItemContainer::Warehouse
						? Subsystem->GetRuntimeState().DesktopInventory.WarehouseItems.FindRef(Input.ExpectedEntry.EntryId)
						: Subsystem->GetRuntimeState().Inventory.FindRef(Input.ExpectedEntry.EntryId));
				Entry.IconPath = Input.ExpectedEntry.bEquipmentInstance
					? EquipmentIconTexturePath(Subsystem->GetRuntimeState().EquipmentCollection, Input.ExpectedEntry.EntryId)
					: InventoryItemIconTexturePath(Input.ExpectedEntry.EntryId);
			}
			SetNotice(FText::FromString(TEXT("已按最低可合成品质自动放置")));
			RefreshLayout();
		}
		break;
	case 312:
		if (Subsystem)
		{
			Subsystem->SetToolAutoFillIncludesWarehouse(!Subsystem->GetRuntimeState().DesktopInventory.bToolAutoFillIncludesWarehouse);
			RefreshLayout();
		}
		break;
	case 313:
	case 314:
		if (Subsystem)
		{
			const int32 Delta = ActionId == 313 ? -1 : 1;
			Subsystem->SetToolSelectedCraftingLevel(Subsystem->GetToolProgress().SelectedCraftingLevel + Delta);
			RefreshLayout();
		}
		break;
	case 315:
	case 316:
		if (Subsystem)
		{
			FGameXXKEquipmentTransactionResult Result;
			if (Subsystem->ExecuteToolResolveReforge(ActionId == 315, Result))
			{
				ReturnAllToolEntries();
				SetNotice(FText::FromString(ActionId == 315 ? TEXT("已采用新词缀") : TEXT("已保留原词缀")));
				RefreshLayout();
			}
			else SetNotice(Result.Message);
		}
		break;
	case 317:
		SelectedToolSocketIndex = FMath::Max(0, SelectedToolSocketIndex - 1);
		RefreshLayout();
		break;
	case 318:
		if (Subsystem && ToolSlots.IsValidIndex(0) && ToolSlots[0].Entry.bEquipmentInstance)
		{
			const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
				Subsystem->GetRuntimeState().EquipmentCollection, ToolSlots[0].Entry.EntryId);
			const int32 Capacity = Instance ? Instance->SocketedGems.Num() : 1;
			SelectedToolSocketIndex = FMath::Min(FMath::Max(0, Capacity - 1), SelectedToolSocketIndex + 1);
		}
		RefreshLayout();
		break;
	case 309:
		ConfirmToolForTest();
		break;
	default:
		break;
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleActionRightClicked(const int32 ActionId)
{
	if (CarriedEntry.IsValid())
	{
		const bool bCancelled = CancelCarriedItem();
		if (bCancelled)
		{
			RefreshLayout();
		}
		return bCancelled;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	if (ActionId >= 100 && ActionId < 100 + WarehousePageSize)
	{
		const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + (ActionId - 100);
		const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			PhysicalSlotIndex);
		const int32 BackpackSlot = FGameXXKDesktopInventoryRules::FindFirstEmptySlot(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Backpack);
		FString Error;
		if (Entry.IsValid() && BackpackSlot != INDEX_NONE
			&& Subsystem->MoveDesktopInventoryEntry(
				EGameXXKDesktopItemContainer::Warehouse,
				PhysicalSlotIndex,
				EGameXXKDesktopItemContainer::Backpack,
				BackpackSlot,
				&Error))
		{
			RefreshLayout();
			return true;
		}
		return false;
	}
	if (ActionId >= 300 && ActionId < 300 + ToolSlotCount)
	{
		const int32 ToolSlotIndex = ActionId - 300;
		if (ToolSlots.IsValidIndex(ToolSlotIndex) && ToolSlots[ToolSlotIndex].IsValid())
		{
			ToolSlots[ToolSlotIndex] = FDesktopToolEntry();
			RefreshLayout();
			return true;
		}
	}
	return false;
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetNotice(const FText& Notice)
{
	LastNotice = Notice;
	if (NoticeText)
	{
		NoticeText->SetText(Notice);
	}
	if (NoticePanel)
	{
		NoticePanel->SetVisibility(
			Notice.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
