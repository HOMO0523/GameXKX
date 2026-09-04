#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
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
#include "Components/ScrollBox.h"
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
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKTalentRules.h"
#include "Guide/GameXXKGuideCoordinator.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "UI/GameXXKGuidePreferenceWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKInventoryItemPresentation.h"
#include "UI/GameXXKTalentTreeWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
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

		virtual FReply OnMouseWheel(
			const FGeometry& MyGeometry,
			const FPointerEvent& MouseEvent) override
		{
			if (Owner.IsValid() && Owner->HandleMouseWheel(MouseEvent.GetWheelDelta()))
			{
				return FReply::Handled();
			}
			return SButton::OnMouseWheel(MyGeometry, MouseEvent);
		}

	private:
		TWeakObjectPtr<UGameXXKDesktopTrainingActionButton> Owner;
	};

#if PLATFORM_WINDOWS
	struct FGameXXKDesktopWindowHook
	{
		WNDPROC PreviousWindowProc = nullptr;
		TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Owner;
	};

	TMap<HWND, FGameXXKDesktopWindowHook>& GetDesktopWindowHooks()
	{
		static TMap<HWND, FGameXXKDesktopWindowHook> Hooks;
		return Hooks;
	}

	float GetDesktopMonitorDpiScale(HWND WindowHandle)
	{
		return WindowHandle
			? GameXXKDesktopTrainingLayout::ResolveWindowDpiScale(
				::GetDpiForWindow(WindowHandle))
			: 1.0f;
	}

	LRESULT CALLBACK GameXXKDesktopWindowProc(
		HWND WindowHandle,
		UINT Message,
		WPARAM WParam,
		LPARAM LParam)
	{
		FGameXXKDesktopWindowHook* Hook = GetDesktopWindowHooks().Find(WindowHandle);
		if (Hook && Hook->Owner.IsValid())
		{
			if (Message == WM_KEYDOWN
				&& WParam == VK_TAB
				&& (static_cast<uint64>(LParam) & (1ull << 30)) == 0)
			{
				Hook->Owner->HandleActionClicked(60);
				return 0;
			}
			if (Message == WM_EXITSIZEMOVE)
			{
				Hook->Owner->NotifyDesktopNativeMoveCompleted();
			}
			else if (Message == WM_DPICHANGED || Message == WM_DISPLAYCHANGE)
			{
				Hook->Owner->NotifyDesktopNativeDisplayMetricsChanged();
			}
		}
		return Hook && Hook->PreviousWindowProc
			? ::CallWindowProc(Hook->PreviousWindowProc, WindowHandle, Message, WParam, LParam)
			: ::DefWindowProc(WindowHandle, Message, WParam, LParam);
	}
#endif

	constexpr int32 WarehouseColumns = 4;
	constexpr int32 WarehouseRows = 9;
	constexpr int32 WarehousePageSize = WarehouseColumns * WarehouseRows;
	constexpr int32 ToolSlotCount = 9;
	constexpr int32 ToolModeCount = 5;
	constexpr int32 TopToolbarButtonCount = 5;
	constexpr int32 ActionCloseWarehouse = 62;
	constexpr int32 ActionCloseCentralPage = 63;
	constexpr int32 ActionCloseRightPanel = 64;
	constexpr int32 ActionTrainingDifficultyDropdown = 620;
	constexpr int32 ActionTrainingDifficultyFirst = 621;
	constexpr int32 ActionTrainingChapterFirst = 624;
	constexpr int32 ActionNoticeSurface = 680;
	constexpr int32 ActionNoticeCollapse = 681;
	constexpr int32 ActionNoticeExpand = 682;
	constexpr int32 ActionNoticeSettings = 683;
	constexpr int32 ActionNoticeCategoryFirst = 684;
	constexpr int32 ActionHudScale100 = 650;
	constexpr int32 ActionHudScale50 = 651;
	constexpr int32 ActionHudScale75 = 656;
	constexpr int32 ActionToggleTown = 652;
	constexpr int32 ActionIdleStripFold = 653;
	constexpr int32 ActionStoryQuest = 654;
	constexpr int32 ActionResetCombatGuide = 655;
	constexpr int32 NoticeHistoryCapacity = 200;
	constexpr float NoticeLineHeight = 24.0f;
	constexpr float NoticeRecordsBarHeight = 28.0f;
	constexpr float IdleSummaryReportWidth = 420.0f;
	constexpr float IdleSummaryFoldSlotWidth = 113.0f;
	constexpr float IdleSummaryFoldButtonWidth = 72.0f;
	constexpr float IdleSummaryFoldButtonX =
		IdleSummaryReportWidth + (IdleSummaryFoldSlotWidth - IdleSummaryFoldButtonWidth) * 0.5f;
	constexpr float IdleSummaryProgressX = IdleSummaryReportWidth + IdleSummaryFoldSlotWidth;
	constexpr float IdleSummaryProgressWidth = 420.0f;
	constexpr float IdleSummaryTabX = IdleSummaryProgressX + IdleSummaryProgressWidth;
	constexpr float IdleSummaryTabWidth = 72.0f;
	constexpr float FoldedChestTextWidth = 96.0f;
	constexpr float FoldedSummaryWidth =
		IdleSummaryTabX + IdleSummaryTabWidth + FoldedChestTextWidth * 2.0f;
	constexpr float WaveTrackX = 84.0f;
	constexpr float WaveProgressFixedContentWidth = 138.0f;

	float ResolveIdleSummaryProgressWidth(const bool bExpanded)
	{
		return IdleSummaryProgressWidth;
	}

	float ResolveIdleSummaryTabX(const bool bExpanded)
	{
		return IdleSummaryProgressX + ResolveIdleSummaryProgressWidth(bExpanded);
	}

	float ResolveWaveTrackWidth(const bool bExpanded)
	{
		return ResolveIdleSummaryProgressWidth(bExpanded) - WaveProgressFixedContentWidth;
	}
	static constexpr const TCHAR* NoticeSettingsSection =
		TEXT("/Script/GameXXK.DesktopTrainingNoticeSettings");
	static constexpr const TCHAR* HudSettingsSection =
		TEXT("/Script/GameXXK.DesktopHudSettings");
	static constexpr const TCHAR* HudScaleConfigKey = TEXT("HudScalePercent");
	static constexpr const TCHAR* DesktopWindowPositionXKey = TEXT("WindowPositionX");
	static constexpr const TCHAR* DesktopWindowPositionYKey = TEXT("WindowPositionY");

	FString GetDesktopHudStableSettingsIni()
	{
#if WITH_EDITOR
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Saved/Config/GameXXKDesktopHudSettings.ini")));
#else
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Config/GameXXKDesktopHudSettings.ini")));
#endif
	}

	FString GetDesktopHudCanonicalLegacySettingsIni()
	{
#if WITH_EDITOR
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Saved/Config/WindowsEditor/GameUserSettings.ini")));
#else
		return GGameUserSettingsIni;
#endif
	}

	bool ReadDesktopHudStableScale(int32& OutScale)
	{
		FConfigFile StableConfig;
		StableConfig.Read(GetDesktopHudStableSettingsIni());
		FString RawScale;
		if (!StableConfig.GetString(
				HudSettingsSection,
				HudScaleConfigKey,
				RawScale))
		{
			return false;
		}
		OutScale = FCString::Atoi(*RawScale);
		return true;
	}

	void WriteDesktopHudStableScale(const int32 Scale)
	{
		const FString StableSettingsIni = GetDesktopHudStableSettingsIni();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(StableSettingsIni), true);
		FConfigFile StableConfig;
		StableConfig.Read(StableSettingsIni);
		StableConfig.SetString(
			HudSettingsSection,
			HudScaleConfigKey,
			*FString::FromInt(Scale));
		StableConfig.Dirty = true;
		StableConfig.Write(StableSettingsIni);
	}

	int32 NormalizeHudScalePercent(const int32 Percent)
	{
		if (Percent <= 50)
		{
			return 50;
		}
		return Percent <= 75 ? 75 : 100;
	}
	constexpr EGameXXKDesktopNoticeCategory NoticeCategories[] = {
		EGameXXKDesktopNoticeCategory::ChestAcquired,
		EGameXXKDesktopNoticeCategory::ChestOpenResult,
		EGameXXKDesktopNoticeCategory::StageCleared,
		EGameXXKDesktopNoticeCategory::StageFailed,
		EGameXXKDesktopNoticeCategory::CharacterLevelUp,
		EGameXXKDesktopNoticeCategory::CharacterDeath,
		EGameXXKDesktopNoticeCategory::CharacterRevive,
		EGameXXKDesktopNoticeCategory::EquipmentCombine,
		EGameXXKDesktopNoticeCategory::EnhanceReforge,
		EGameXXKDesktopNoticeCategory::Socket,
		EGameXXKDesktopNoticeCategory::System};

	FString NoticeCategoryLabel(const EGameXXKDesktopNoticeCategory Category)
	{
		switch (Category)
		{
		case EGameXXKDesktopNoticeCategory::ChestAcquired: return TEXT("获得宝箱");
		case EGameXXKDesktopNoticeCategory::ChestOpenResult: return TEXT("宝箱开启结果");
		case EGameXXKDesktopNoticeCategory::StageCleared: return TEXT("关卡通关");
		case EGameXXKDesktopNoticeCategory::StageFailed: return TEXT("关卡失败");
		case EGameXXKDesktopNoticeCategory::CharacterLevelUp: return TEXT("角色升级");
		case EGameXXKDesktopNoticeCategory::CharacterDeath: return TEXT("角色死亡");
		case EGameXXKDesktopNoticeCategory::CharacterRevive: return TEXT("角色复活");
		case EGameXXKDesktopNoticeCategory::EquipmentCombine: return TEXT("装备合成结果");
		case EGameXXKDesktopNoticeCategory::EnhanceReforge: return TEXT("强化与洗炼结果");
		case EGameXXKDesktopNoticeCategory::Socket: return TEXT("镶嵌结果");
		case EGameXXKDesktopNoticeCategory::System:
		default: return TEXT("系统提示");
		}
	}

	const TCHAR* NoticeCategoryConfigKey(const EGameXXKDesktopNoticeCategory Category)
	{
		switch (Category)
		{
		case EGameXXKDesktopNoticeCategory::ChestAcquired: return TEXT("ChestAcquired");
		case EGameXXKDesktopNoticeCategory::ChestOpenResult: return TEXT("ChestOpenResult");
		case EGameXXKDesktopNoticeCategory::StageCleared: return TEXT("StageCleared");
		case EGameXXKDesktopNoticeCategory::StageFailed: return TEXT("StageFailed");
		case EGameXXKDesktopNoticeCategory::CharacterLevelUp: return TEXT("CharacterLevelUp");
		case EGameXXKDesktopNoticeCategory::CharacterDeath: return TEXT("CharacterDeath");
		case EGameXXKDesktopNoticeCategory::CharacterRevive: return TEXT("CharacterRevive");
		case EGameXXKDesktopNoticeCategory::EquipmentCombine: return TEXT("EquipmentCombine");
		case EGameXXKDesktopNoticeCategory::EnhanceReforge: return TEXT("EnhanceReforge");
		case EGameXXKDesktopNoticeCategory::Socket: return TEXT("Socket");
		case EGameXXKDesktopNoticeCategory::System:
		default: return TEXT("System");
		}
	}

	EGameXXKDesktopNoticeCategory NoticeCategoryForToolMode(
		const EGameXXKDesktopToolMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKDesktopToolMode::Combine:
			return EGameXXKDesktopNoticeCategory::EquipmentCombine;
		case EGameXXKDesktopToolMode::Enhance:
		case EGameXXKDesktopToolMode::Reforge:
			return EGameXXKDesktopNoticeCategory::EnhanceReforge;
		case EGameXXKDesktopToolMode::Socket:
			return EGameXXKDesktopNoticeCategory::Socket;
		case EGameXXKDesktopToolMode::Dismantle:
		default:
			return EGameXXKDesktopNoticeCategory::System;
		}
	}

	FLinearColor NoticeCategoryColor(const EGameXXKDesktopNoticeCategory Category)
	{
		switch (Category)
		{
		case EGameXXKDesktopNoticeCategory::ChestAcquired: return FLinearColor(0.38f, 0.86f, 0.96f, 1.0f);
		case EGameXXKDesktopNoticeCategory::ChestOpenResult: return FLinearColor(0.98f, 0.76f, 0.20f, 1.0f);
		case EGameXXKDesktopNoticeCategory::StageCleared: return FLinearColor(0.96f, 0.72f, 0.18f, 1.0f);
		case EGameXXKDesktopNoticeCategory::StageFailed: return FLinearColor(0.88f, 0.34f, 0.26f, 1.0f);
		case EGameXXKDesktopNoticeCategory::CharacterLevelUp: return FLinearColor(0.38f, 0.96f, 0.48f, 1.0f);
		case EGameXXKDesktopNoticeCategory::CharacterDeath: return FLinearColor(0.78f, 0.32f, 0.30f, 1.0f);
		case EGameXXKDesktopNoticeCategory::CharacterRevive: return FLinearColor(0.42f, 0.94f, 0.62f, 1.0f);
		case EGameXXKDesktopNoticeCategory::EquipmentCombine: return FLinearColor(0.72f, 0.62f, 0.98f, 1.0f);
		case EGameXXKDesktopNoticeCategory::EnhanceReforge: return FLinearColor(0.94f, 0.58f, 0.22f, 1.0f);
		case EGameXXKDesktopNoticeCategory::Socket: return FLinearColor(0.36f, 0.82f, 0.98f, 1.0f);
		case EGameXXKDesktopNoticeCategory::System:
		default: return FLinearColor(0.94f, 0.90f, 0.78f, 1.0f);
		}
	}
	EGameXXKTrainingDifficulty TrainingDifficultyFromIndex(const int32 Index)
	{
		switch (Index)
		{
		case 1: return EGameXXKTrainingDifficulty::Hard;
		case 2: return EGameXXKTrainingDifficulty::Hell;
		case 0:
		default: return EGameXXKTrainingDifficulty::Normal;
		}
	}

	FString TrainingStageShortLabel(const FName StageId)
	{
		FGameXXKTrainingStageDefinition Definition;
		if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
		{
			return TEXT("未游历");
		}
		const TCHAR* DifficultyLabel = TEXT("普通");
		switch (Definition.Difficulty)
		{
		case EGameXXKTrainingDifficulty::Hard: DifficultyLabel = TEXT("困难"); break;
		case EGameXXKTrainingDifficulty::Hell: DifficultyLabel = TEXT("地狱"); break;
		case EGameXXKTrainingDifficulty::Normal:
		default: break;
		}
		return FString::Printf(
			TEXT("%s %d-%d"),
			DifficultyLabel,
			Definition.Chapter,
			((Definition.StageNumber - 1) % 3) + 1);
	}
	const FVector2D TravelVisualSize(953.0f, 202.0f);
	const FVector2D IdleGroupLogicalSize(1038.0f, 202.0f);
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
	// Deterministic alpha-bounds audit of the corrected 2026-08-27 atlases. Hit
	// and death are procedural effects on the Idle pose, so they intentionally
	// retain the same scale instead of referencing retired action atlases.
	float ResolveTravelHeroContentScale(const EGameXXKBattleAnimationAction Action)
	{
		switch (Action)
		{
		case EGameXXKBattleAnimationAction::Attack: return 1.160f;
		case EGameXXKBattleAnimationAction::Hit: return 1.094f;
		case EGameXXKBattleAnimationAction::Death: return 1.094f;
		case EGameXXKBattleAnimationAction::Idle:
		default: return 1.094f;
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
		{TEXT("enemy_01_rooster"), 0.824219f, 0.789062f, 0.824219f, 0.824219f},
		{TEXT("enemy_02_goat"), 0.8320f, 0.6191f, 0.5352f, 0.7646f},
		{TEXT("enemy_03_weasel"), 0.632812f, 0.406250f, 0.632812f, 0.632812f},
		{TEXT("enemy_04_civet"), 0.5703f, 0.4688f, 0.5547f, 0.6133f},
		{TEXT("enemy_05_ironfeather"), 0.783203f, 0.567383f, 0.783203f, 0.783203f},
		{TEXT("enemy_06_bluehorn"), 0.7910f, 0.6094f, 0.5859f, 0.7090f},
		{TEXT("enemy_11_graymane"), 0.614258f, 0.578125f, 0.614258f, 0.614258f},
		{TEXT("enemy_16_toad"), 0.417969f, 0.321289f, 0.417969f, 0.417969f},
		{TEXT("enemy_18_deer"), 0.812500f, 0.789062f, 0.812500f, 0.812500f},
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
			case EGameXXKBattleAnimationAction::Hit:
			case EGameXXKBattleAnimationAction::Death:
				OccupiedHeight = Entry.Idle;
				break;
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
		{TEXT("character_09_yue_bai"), 0.808594f, 0.615234f, 0.808594f, 0.808594f},
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
				case EGameXXKBattleAnimationAction::Hit:
				case EGameXXKBattleAnimationAction::Death:
					OccupiedHeight = Entry.Idle;
					break;
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
	static constexpr const TCHAR* TrainingNodePassedTexturePath = TEXT("/Game/GameXXK/UI/Training/MapNodes/T_TrainingNode_Passed.T_TrainingNode_Passed");
	static constexpr const TCHAR* TrainingNodeChallengeTexturePath = TEXT("/Game/GameXXK/UI/Training/MapNodes/T_TrainingNode_Challenge.T_TrainingNode_Challenge");
	static constexpr const TCHAR* TrainingNodeLockedTexturePath = TEXT("/Game/GameXXK/UI/Training/MapNodes/T_TrainingNode_Locked.T_TrainingNode_Locked");
	static constexpr const TCHAR* TruthNavWarehouseTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse");
	static constexpr const TCHAR* TruthNavFormationTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavFormation.T_TrainingNavFormation");
	static constexpr const TCHAR* TruthNavTalentsTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
	static constexpr const TCHAR* TruthNavToolsTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools");
	static constexpr const TCHAR* TruthNavTrainingTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining.T_TrainingNavTraining");
	static constexpr const TCHAR* TruthTopToolbarAlwaysOnTopTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTop.T_TrainingTopToolbarAlwaysOnTop");
	static constexpr const TCHAR* TruthTopToolbarAlwaysOnTopOffTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingTopToolbarAlwaysOnTopOffGray.T_TrainingTopToolbarAlwaysOnTopOffGray");
	static constexpr const TCHAR* TravelHeroAtlasTexturePath = TEXT("/Game/GameXXK/UI/Training/Generated/walkloop_pilot_v1/character_00_hero_walk_left/atlas_1K/T_TrainingHeroWalkLeft_1K.T_TrainingHeroWalkLeft_1K");
	static constexpr const TCHAR* TravelBackgroundTexturePath = TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background.T_TrainingIdleStrip_Background");
	static constexpr const TCHAR* TrainingNormalChestTexturePath = TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingNormalChest.T_Item_TrainingNormalChest");
	static constexpr const TCHAR* TrainingAdvancedChestTexturePath = TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingAdvancedChest.T_Item_TrainingAdvancedChest");
	static constexpr const TCHAR* TrainingRetryButtonBaseTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryButtonBase.T_TrainingRetryButtonBase");
	static constexpr const TCHAR* TrainingRetryIconEnabledTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryIconEnabled.T_TrainingRetryIconEnabled");
	static constexpr const TCHAR* TrainingRetryIconDisabledTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingRetryIconDisabled.T_TrainingRetryIconDisabled");
	static constexpr const TCHAR* TrainingWaveMarkerNormalTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingWaveMarkerNormal.T_TrainingWaveMarkerNormal");
	static constexpr const TCHAR* TrainingWaveMarkerEliteTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingWaveMarkerElite.T_TrainingWaveMarkerElite");
	static constexpr const TCHAR* TrainingWaveMarkerBossTexturePath = TEXT("/Game/GameXXK/UI/Training/IdleStrip/T_TrainingWaveMarkerBoss.T_TrainingWaveMarkerBoss");
	static constexpr const TCHAR* DesktopTownEnterButtonTexturePath = TEXT("/Game/GameXXK/UI/DesktopOverlay/T_DesktopTownEnterButton.T_DesktopTownEnterButton");
	static constexpr const TCHAR* DesktopTownExitButtonTexturePath = TEXT("/Game/GameXXK/UI/DesktopOverlay/T_DesktopTownExitButton.T_DesktopTownExitButton");
	static constexpr const TCHAR* DesktopStoryQuestButtonTexturePath =
		TEXT("/Game/GameXXK/UI/DesktopOverlay/"
			"T_DesktopStoryQuestButton.T_DesktopStoryQuestButton");
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

	FSlateBrush MakeCircularBrush(
		const FVector2D& ImageSize,
		const FLinearColor& Fill,
		const FLinearColor& Outline,
		const float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			FMath::Min(ImageSize.X, ImageSize.Y) * 0.5f,
			FSlateColor(Outline),
			OutlineWidth);
		return Brush;
	}

	FButtonStyle MakeCircularButtonStyle(
		const FVector2D& ImageSize,
		const FLinearColor& Fill,
		const FLinearColor& Outline)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeCircularBrush(ImageSize, Fill, Outline, 3.0f));
		Style.SetHovered(MakeCircularBrush(
			ImageSize,
			Fill * FLinearColor(1.05f, 1.05f, 1.05f, 1.0f),
			Outline,
			4.0f));
		Style.SetPressed(MakeCircularBrush(
			ImageSize,
			Fill * FLinearColor(0.84f, 0.84f, 0.84f, 1.0f),
			Outline,
			4.0f));
		Style.SetDisabled(MakeCircularBrush(
			ImageSize,
			FLinearColor(0.50f, 0.48f, 0.44f, 0.70f),
			FLinearColor(0.25f, 0.24f, 0.22f, 0.75f),
			3.0f));
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
		const FString InspectableIcon =
			FGameXXKInventoryItemPresentation::ResolveIconPath(ItemId);
		if (!InspectableIcon.IsEmpty()) return InspectableIcon;
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
		const int32 LabelSize = 11,
		const FName LabelName = NAME_None,
		UTextBlock** OutLabelText = nullptr,
		const FName IconName = NAME_None,
		const bool bQuantityLabel = false)
	{
		if (OutLabelText)
		{
			*OutLabelText = nullptr;
		}
		UOverlay* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		if (!IconTexturePath.IsEmpty())
		{
			if (UTexture2D* Texture = LoadTexture(*IconTexturePath))
			{
				USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				IconBox->SetWidthOverride(IconSize.X);
				IconBox->SetHeightOverride(IconSize.Y);
				UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass(), IconName);
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
			UTextBlock* LabelText = MakeText(
				Tree,
				Label,
				LabelSize,
				bQuantityLabel ? FLinearColor::White : Ink,
				LabelName);
			LabelText->SetJustification(bQuantityLabel ? ETextJustify::Right : ETextJustify::Center);
			if (bQuantityLabel)
			{
				FSlateFontInfo QuantityFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
				QuantityFont.OutlineSettings.OutlineSize = 2;
				QuantityFont.OutlineSettings.OutlineColor = FLinearColor::Black;
				LabelText->SetFont(QuantityFont);
			}
			if (UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(LabelText))
			{
				LabelSlot->SetHorizontalAlignment(bQuantityLabel ? HAlign_Right : HAlign_Fill);
				LabelSlot->SetVerticalAlignment(VAlign_Bottom);
				LabelSlot->SetPadding(bQuantityLabel
					? FMargin(0.0f, 0.0f, 2.0f, 1.0f)
					: FMargin(2.0f, 0.0f, 2.0f, 3.0f));
			}
			if (OutLabelText)
			{
				*OutLabelText = LabelText;
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

	FName ResolveWorkbenchNpcId(const UGameXXKMVPSubsystem* Subsystem)
	{
		FName NpcId;
		FString Error;
		return Subsystem
			&& FGameXXKPartyFormationRules::ResolveQuestNpcId(
				Subsystem->GetRuntimeState(),
				NpcId,
				&Error)
			? NpcId
			: NAME_None;
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
			Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Portrait->SetBrush(FSlateBrush());
			Portrait->SetVisibility(ESlateVisibility::Collapsed);
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
	SetStyle(MakeCircularButtonStyle(
		FVector2D(82.0f, 82.0f),
		FLinearColor(0.94f, 0.87f, 0.70f, 0.96f),
		Ink));
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
	OnHovered.Clear();
	OnHovered.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleHovered);
	OnUnhovered.Clear();
	OnUnhovered.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleUnhovered);
	OnPressed.Clear();
	OnPressed.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandlePressed);
	OnReleased.Clear();
	OnReleased.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleReleased);
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
		BuildProgrammaticLayout();
	}
	return Super::RebuildWidget();
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadHudScaleSetting();
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
	if (GuideCoordinator)
	{
		GuideCoordinator->RefreshTarget();
	}
	if (bLayoutRefreshPending && !bLayoutRebuildScheduled && !bInActionCallback)
	{
		RebuildLayoutNow();
	}
	++TravelVisualNativeTickCount;
	UpdateCarriedItemVisualPosition();
	TGuardValue<bool> NativeTickGuard(bNativeTickActive, true);
	TickCollapsedResourceUnload(InDeltaTime);
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		TickDesktopNativeWindow();
	}
	else
	{
		const FVector2D ViewportHostSize = MyGeometry.GetLocalSize();
		if (ViewportHostSize.X > 1.0f
			&& ViewportHostSize.Y > 1.0f
			&& (!bDesktopResolvedMetricsValid
				|| !DesktopResolvedMetrics.PhysicalWorkAreaSize.Equals(
					ViewportHostSize,
					0.5f)))
		{
			DesktopWindowPositionNormalized =
				GameXXKDesktopTrainingLayout::ResolvePresentationAnchor(
					false,
					DesktopWindowPositionNormalized);
			InitializeDesktopPresentationHostSize(ViewportHostSize);
		}
	}
	if (NoticeHoveredWidgetCount <= 0 && NoticeHoverHideRemainingSeconds > 0.0f)
	{
		NoticeHoverHideRemainingSeconds = FMath::Max(
			0.0f,
			NoticeHoverHideRemainingSeconds - FMath::Max(0.0f, InDeltaTime));
		if (NoticeHoverHideRemainingSeconds <= 0.0f)
		{
			RefreshNoticeControlVisibility();
		}
	}
	LivePresentationAccumulator += FMath::Max(0.0f, InDeltaTime);
	int32 RealElapsedSeconds = 0;
	if (LivePresentationAccumulator >= 1.0f)
	{
		RealElapsedSeconds = FMath::FloorToInt(LivePresentationAccumulator);
		LivePresentationAccumulator = FMath::Fmod(LivePresentationAccumulator, 1.0f);
		RefreshLivePresentation(false);
	}
	const UGameXXKMVPSubsystem* TravelSubsystem = ResolveMVPSubsystem();
	FGameXXKTrainingTravelRuntime AuthoritativeTravelRuntime;
	if (TravelSubsystem)
	{
		AuthoritativeTravelRuntime = TravelSubsystem->GetTrainingTravelRuntimeCopy();
		TravelVisualRuntime.Synchronize(AuthoritativeTravelRuntime);
	}
	const bool bVisualWasWalkingAtFrameStart = TravelVisualRuntime.IsWalking();
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
	if (AuthoritativeTravelRuntime.Phase == EGameXXKTrainingTravelPhase::Walking
		&& !bVisualWasWalkingAtFrameStart)
	{
		// Finish every queued hit/death presentation before starting the next
		// encounter's five-second walkloop. Discarding the partial accumulator
		// here prevents old combat presentation time from consuming spawn delay.
		TravelAccumulator = 0.0f;
		UpdateTravelVisuals();
		return;
	}

	// Combat keeps its one-second logical cadence. Walking divides the authored
	// five logical steps across the movement talent's real-time duration, while
	// chest cooldowns continue consuming actual wall-clock seconds.
	TravelAccumulator += InDeltaTime;
	if (AuthoritativeTravelRuntime.Phase == EGameXXKTrainingTravelPhase::Walking)
	{
		FGameXXKTalentProjection TalentProjection;
		const float WalkSeconds = TravelSubsystem
			&& FGameXXKTalentRules::BuildProjection(
				TravelSubsystem->GetRuntimeState().Talents,
				TalentProjection)
			? TalentProjection.GetTravelWalkSeconds()
			: static_cast<float>(FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds);
		const float LogicalCadence = FMath::Max(
			0.05f,
			WalkSeconds / static_cast<float>(FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds));
		const int32 LogicalSteps = FMath::Clamp(
			FMath::FloorToInt(TravelAccumulator / LogicalCadence),
			0,
			FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds);
		if (LogicalSteps > 0)
		{
			TravelAccumulator -= static_cast<float>(LogicalSteps) * LogicalCadence;
			for (int32 StepIndex = 0; StepIndex < LogicalSteps; ++StepIndex)
			{
				const int32 CooldownSeconds = StepIndex == 0 ? RealElapsedSeconds : 0;
				if (!AdvanceTravelForTest(CooldownSeconds)
					|| TravelSubsystem->GetTrainingTravelRuntimeCopy().Phase
						!= EGameXXKTrainingTravelPhase::Walking)
				{
					TravelAccumulator = 0.0f;
					break;
				}
			}
		}
	}
	else if (TravelAccumulator >= 1.0f)
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
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D HostPointInWindow =
			GameXXKDesktopTrainingLayout::SlateHostUnitsToPhysicalPixels(
				InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()),
				DesktopInputDpiScale);
		const FVector2D HostPoint = HostPointInWindow
			- (bDesktopFixedHostEnabled
				? DesktopFixedContentOffset
				: FVector2D::ZeroVector);
		const FVector2D StripLocalTopLeft =
			DesktopOverlayPlacement.StripTopLeft - DesktopOverlayPlacement.HudTopLeft;
		const FVector2D StripPoint = HostPoint - StripLocalTopLeft;
		const bool bInsideStrip = StripPoint.X >= 0.0f
			&& StripPoint.Y >= 0.0f
			&& StripPoint.X <= DesktopOverlayPlacement.StripSize.X
			&& StripPoint.Y <= DesktopOverlayPlacement.StripSize.Y;
		if (bInsideStrip)
		{
			const FVector2D IdlePoint = bIdleStripFolded
				? StripPoint / FMath::Max(0.01f, DesktopOverlayPlacement.Scale)
				: FVector2D(
					StripPoint.X * GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize().X
						/ FMath::Max(1.0f, DesktopOverlayPlacement.StripSize.X),
					StripPoint.Y * GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize().Y
						/ FMath::Max(1.0f, DesktopOverlayPlacement.StripSize.Y));
			const float ChestControlLocalX = bBackpackExpanded
				? GameXXKDesktopTrainingLayout::GetIdleStripChestControlX()
				: 953.0f;
			const FVector4 ExpandedControls[] = {
				FVector4(ChestControlLocalX - 60.0f, 18.0f, 52.0f, 52.0f),
				FVector4(ChestControlLocalX, 8.0f, 72.0f, 72.0f),
				FVector4(ChestControlLocalX, 84.0f, 72.0f, 72.0f)};
			const FVector4 FoldedControls[] = {
				FVector4(IdleSummaryFoldButtonX, 0.0f, IdleSummaryFoldButtonWidth, NoticeLineHeight),
				FVector4(
					ResolveIdleSummaryTabX(bBackpackExpanded),
					0.0f,
					IdleSummaryTabWidth,
					NoticeLineHeight)};
			bool bIdleControl = false;
			const FVector4* Controls = bIdleStripFolded ? FoldedControls : ExpandedControls;
			const int32 ControlCount = bIdleStripFolded
				? UE_ARRAY_COUNT(FoldedControls)
				: UE_ARRAY_COUNT(ExpandedControls);
			for (int32 ControlIndex = 0; ControlIndex < ControlCount; ++ControlIndex)
			{
				const FVector4& Control = Controls[ControlIndex];
				bIdleControl = bIdleControl
					|| (IdlePoint.X >= Control.X
						&& IdlePoint.X <= Control.X + Control.Z
						&& IdlePoint.Y >= Control.Y
						&& IdlePoint.Y <= Control.Y + Control.W);
			}
			if (!bIdleControl)
			{
				FVector2D PointerScreen;
				if (TryGetDesktopHudPointerScreenPosition(PointerScreen))
				{
					bDesktopHudDragging = true;
					DesktopHudDragStartPointerScreen = PointerScreen;
					DesktopHudDragStartNormalizedAnchor = DesktopWindowPositionNormalized;
					return FReply::Handled().CaptureMouse(TakeWidget());
				}
			}
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UGameXXKDesktopTrainingWorkbenchWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bDesktopHudDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UpdateDesktopOverlayAnchorFromPointer();
		const FVector2D PreviousBodyOffset = DesktopOverlayPlacement.BodyOffset;
		bDesktopHudDragging = false;
		const bool bPreviousExpandUpward = bExpandUpward;
		if (bBackpackExpanded)
		{
			UpdateExpansionDirectionFromNativeWindow();
		}
		SaveDesktopNativeWindowPosition();
		if (bBackpackExpanded
			&& (bExpandUpward != bPreviousExpandUpward
				|| !DesktopOverlayPlacement.BodyOffset.Equals(PreviousBodyOffset, 0.01f)))
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(
					[WeakThis = TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget>(this)]()
					{
						if (UGameXXKDesktopTrainingWorkbenchWidget* Widget = WeakThis.Get())
						{
							Widget->RefreshLayout();
						}
					});
			}
			else
			{
				RefreshLayout();
			}
		}
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGameXXKDesktopTrainingWorkbenchWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bDesktopHudDragging)
	{
		UpdateDesktopOverlayAnchorFromPointer();
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeOnMouseCaptureLost(
	const FCaptureLostEvent& CaptureLostEvent)
{
	if (bDesktopHudDragging)
	{
		bDesktopHudDragging = false;
		SaveDesktopNativeWindowPosition();
	}
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (bDesktopHudDragging)
	{
		bDesktopHudDragging = false;
		SaveDesktopNativeWindowPosition();
	}
	AbortTransientInventoryInteraction(false, true);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeDestruct()
{
	if (!bInternalLayoutRebuild)
	{
		if (GuideCoordinator)
		{
			GuideCoordinator->NotifyOverlayDestroyed();
		}
		FGameXXKGuideTargetRegistry& GuideRegistry = FGameXXKGuideTargetRegistry::Get();
		if (GuideEventHandle.IsValid())
		{
			GuideRegistry.OnGuideEvent().Remove(GuideEventHandle);
			GuideEventHandle.Reset();
		}
		GuideRegistry.ClearActionGate(this);
		if (ResetCombatGuideButton)
		{
			GuideRegistry.UnregisterTarget(TEXT("Desktop.Settings.ResetCombatGuide"), ResetCombatGuideButton);
		}
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
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->SetDesktopWorkbenchTownPanelInputLock(false);
		}
		ReleaseDesktopNativeWindow();
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
	SynchronizeTrainingPageFromStage(SelectedStageId);
	const FGuid CurrentSettlementReceipt =
		Subsystem->GetRuntimeState().CardRun.LastAppliedRouteSettlementId;
	const bool bCompletedSettlementSinceChallenge = bRestoreTrainingPanelAfterChallenge
		&& CurrentSettlementReceipt.IsValid()
		&& CurrentSettlementReceipt != RouteSettlementReceiptAtChallengeStart;
	const bool bRestoreTrainingPanel = bRestoreTrainingPanelAfterChallenge
		&& !bCompletedSettlementSinceChallenge;
	bRestoreTrainingPanelAfterChallenge = false;
	RouteSettlementReceiptAtChallengeStart = CurrentSettlementReceipt;
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	if (ActiveBackpackCharacterId.IsNone() || !CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		ActiveBackpackCharacterId = CharacterIds.Num() > 0
			? CharacterIds[0]
			: FGameXXKEquipmentRules::HeroCharacterId();
	}
	bSettingsPanelOpen = false;
	bBackpackExpanded = bRestoreTrainingPanel;
	bIdleStripFolded = false;
	bWarehousePanelOpen = false;
	RightPanel = bRestoreTrainingPanel
		? EGameXXKDesktopTrainingRightPanel::TrainingMap
		: EGameXXKDesktopTrainingRightPanel::None;
	ActiveNav = bRestoreTrainingPanel
		? EGameXXKDesktopTrainingNav::Training
		: EGameXXKDesktopTrainingNav::None;
	bTrainingDifficultyDropdownOpen = false;
	bExitConfirmationOpen = false;
	TravelAccumulator = 0.0f;
	LivePresentationAccumulator = 0.0f;
	TravelVisualRuntime.Reset();
	bDesktopNativeLayoutDirty = true;
	RefreshLayout();
	SetVisibility(ESlateVisibility::Visible);
	UpdateTownPresentationInputLock();
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CloseWorkbench()
{
	const bool bWasVisible = GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
	CancelCollapsedResourceUnload();
	bHasSavedEmbeddedInventorySession = false;
	AbortTransientInventoryInteraction(true, false);
	bSettingsPanelOpen = false;
	bBackpackExpanded = false;
	bIdleStripFolded = false;
	bWarehousePanelOpen = false;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	bTrainingDifficultyDropdownOpen = false;
	bExitConfirmationOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	UpdateTownPresentationInputLock();
	bDesktopNativeLayoutDirty = true;
	ReleaseDesktopNativeWindow();
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
	UpdateExpansionDirectionFromNativeWindow();
	bBackpackExpanded = true;
	bExitConfirmationOpen = false;
	UpdateTownPresentationInputLock();
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

EGameXXKDesktopNoticeCategory
UGameXXKDesktopTrainingWorkbenchWidget::GetLastNoticeCategoryForTest() const
{
	return NoticeHistory.IsEmpty()
		? EGameXXKDesktopNoticeCategory::System
		: NoticeHistory.Last().Category;
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

bool UGameXXKDesktopTrainingWorkbenchWidget::RightClickWarehouseSlotForTest(
	const int32 PhysicalSlotIndex)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		&& RequestTutorialMapInspection(FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			PhysicalSlotIndex));
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
			: ResolveWorkbenchNpcId(Subsystem);
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

FName UGameXXKDesktopTrainingWorkbenchWidget::ResolveRememberedBackpackCharacterId(
	const EGameXXKDesktopTrainingCharacterRoster Roster) const
{
	if (Roster == EGameXXKDesktopTrainingCharacterRoster::Hero)
	{
		return FGameXXKEquipmentRules::HeroCharacterId();
	}
	const TArray<FName> CharacterIds = Roster == EGameXXKDesktopTrainingCharacterRoster::Companions
		? GetCompanionCharacterIdsForTest()
		: GetNpcCharacterIdsForTest();
	const FName RememberedId = Roster == EGameXXKDesktopTrainingCharacterRoster::Companions
		? LastCompanionBackpackCharacterId
		: LastNpcBackpackCharacterId;
	return CharacterIds.Contains(RememberedId)
		? RememberedId
		: ResolveRosterRepresentativeCharacterId(Roster);
}

void UGameXXKDesktopTrainingWorkbenchWidget::PreserveEmbeddedSessionForCharacter(
	const FName CharacterId)
{
	const FName PreviousCharacterId = EmbeddedInventoryWidget
		? EmbeddedInventoryWidget->GetConfiguredCharacterIdForTest()
		: SavedEmbeddedInventorySession.CharacterId;
	if (EmbeddedInventoryWidget)
	{
		SavedEmbeddedInventorySession = EmbeddedInventoryWidget->CaptureEmbeddedSessionState();
	}
	else if (!bHasSavedEmbeddedInventorySession)
	{
		SavedEmbeddedInventorySession = FGameXXKEmbeddedInventorySessionState();
	}
	SavedEmbeddedInventorySession.CharacterId = CharacterId;
	if (PreviousCharacterId != CharacterId)
	{
		SavedEmbeddedInventorySession.PendingDeckIds.Reset();
	}
	bHasSavedEmbeddedInventorySession = true;
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
	PreserveEmbeddedSessionForCharacter(CharacterId);
	ActiveBackpackCharacterId = CharacterId;
	ActiveCharacterRoster = bHero
		? EGameXXKDesktopTrainingCharacterRoster::Hero
		: bCompanion
			? EGameXXKDesktopTrainingCharacterRoster::Companions
			: EGameXXKDesktopTrainingCharacterRoster::Npcs;
	if (bCompanion)
	{
		LastCompanionBackpackCharacterId = CharacterId;
	}
	else if (bNpc)
	{
		LastNpcBackpackCharacterId = CharacterId;
	}
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
		TrainingNodePassedTexturePath,
		TrainingNodeChallengeTexturePath,
		TrainingNodeLockedTexturePath};
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
		TEXT("收菜完成：+%d金币 / +%d经验 · 普通箱%d · 高级箱%d"),
		CollectedReward.Gold,
		CollectedReward.Experience,
		CollectedReward.NormalChestCount,
		CollectedReward.AdvancedChestCount)),
		EGameXXKDesktopNoticeCategory::ChestAcquired);
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
	return Subsystem
		? FGameXXKTalentRules::GetUnlockedWarehousePageCount(Subsystem->GetRuntimeState())
		: 1;
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
		&& TravelEnemyHealthTracks.Num() == 3
		&& TravelEnemyHealthFills.Num() == 3
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

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelHeroHealthBarPercentForTest() const
{
	return TravelHeroHealth ? TravelHeroHealth->GetPercent() : -1.0f;
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetTravelHeroHealthBarPercentForTest(const float InPercent)
{
	if (TravelHeroHealth)
	{
		TravelHeroHealth->SetPercent(FMath::Clamp(InPercent, 0.0f, 1.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetTravelHeroHealthBarFillColorForTest(const FLinearColor InColor)
{
	if (TravelHeroHealth)
	{
		TravelHeroHealth->SetFillColorAndOpacity(InColor);
	}
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetProgrammaticLayoutBuildCountForTestBlueprint() const
{
	return ProgrammaticLayoutBuildCount;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HasPendingLayoutRefreshForTestBlueprint() const
{
	return bLayoutRefreshPending;
}

FVector2D UGameXXKDesktopTrainingWorkbenchWidget::GetTravelHeroHealthBarSlateSizeForTest() const
{
	if (!TravelHeroHealth)
	{
		return FVector2D::ZeroVector;
	}
	const FGeometry& Geometry = TravelHeroHealth->GetCachedGeometry();
	return Geometry.GetLocalSize();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsTravelHeroHealthBarSlateValidForTest() const
{
	return TravelHeroHealth && TravelHeroHealth->GetCachedWidget().IsValid();
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelCompanionHealthBarPercentForTest(const int32 CompanionIndex) const
{
	return TravelCompanionHealthBars.IsValidIndex(CompanionIndex)
		&& TravelCompanionHealthBars[CompanionIndex]
		? TravelCompanionHealthBars[CompanionIndex]->GetPercent()
		: -1.0f;
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetTravelEnemyHealthBarPercentForTest(const int32 EnemySlotIndex) const
{
	return TravelEnemyHealthFills.IsValidIndex(EnemySlotIndex)
		&& TravelEnemyHealthFills[EnemySlotIndex]
		? FMath::Clamp(TravelEnemyHealthFills[EnemySlotIndex]->GetRenderTransform().Scale.X, 0.0f, 1.0f)
		: -1.0f;
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelHeroHealthBarRectForTest() const
{
	if (!TravelHeroHealth)
	{
		return FVector4::Zero();
	}
	const FGeometry& Geometry = TravelHeroHealth->GetCachedGeometry();
	const FVector2D Position = Geometry.GetAbsolutePosition();
	const FVector2D Size = Geometry.GetAbsoluteSize();
	return FVector4(Position.X, Position.Y, Size.X, Size.Y);
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelCompanionHealthBarRectForTest(const int32 CompanionIndex) const
{
	if (!TravelCompanionHealthBars.IsValidIndex(CompanionIndex)
		|| !TravelCompanionHealthBars[CompanionIndex])
	{
		return FVector4::Zero();
	}
	const FGeometry& Geometry = TravelCompanionHealthBars[CompanionIndex]->GetCachedGeometry();
	const FVector2D Position = Geometry.GetAbsolutePosition();
	const FVector2D Size = Geometry.GetAbsoluteSize();
	return FVector4(Position.X, Position.Y, Size.X, Size.Y);
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetTravelEnemyHealthBarRectForTest(const int32 EnemySlotIndex) const
{
	if (!TravelEnemyHealthTracks.IsValidIndex(EnemySlotIndex)
		|| !TravelEnemyHealthTracks[EnemySlotIndex])
	{
		return FVector4::Zero();
	}
	const FGeometry& Geometry = TravelEnemyHealthTracks[EnemySlotIndex]->GetCachedGeometry();
	const FVector2D Position = Geometry.GetAbsolutePosition();
	const FVector2D Size = Geometry.GetAbsoluteSize();
	return FVector4(Position.X, Position.Y, Size.X, Size.Y);
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

bool UGameXXKDesktopTrainingWorkbenchWidget::HasResetCombatGuideButtonForTest() const
{
	return ResetCombatGuideButton != nullptr;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsGuidePreferencePromptVisibleForTest() const
{
	return GuidePreferenceWidget && GuidePreferenceWidget->IsPromptVisibleForTest();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ResetCombatGuideForTest()
{
	return HandleResetCombatGuide();
}

bool UGameXXKDesktopTrainingActionButton::HandleMouseWheel(const float WheelDelta)
{
	return Owner && Owner->HandleActionMouseWheel(ActionId, WheelDelta);
}

void UGameXXKDesktopTrainingActionButton::HandleHovered()
{
	if (Owner)
	{
		Owner->HandleActionHoverChanged(ActionId, true);
	}
}

void UGameXXKDesktopTrainingActionButton::HandleUnhovered()
{
	if (Owner)
	{
		Owner->HandleActionHoverChanged(ActionId, false);
	}
}

void UGameXXKDesktopTrainingActionButton::HandlePressed()
{
	if (bScaleOnPress)
	{
		SetRenderScale(FVector2D(0.96f, 0.96f));
	}
}

void UGameXXKDesktopTrainingActionButton::HandleReleased()
{
	if (bScaleOnPress)
	{
		SetRenderScale(FVector2D(1.0f, 1.0f));
	}
}

FName UGameXXKDesktopTrainingWorkbenchWidget::ResolvePreferredTrainingStageForPage(
	const EGameXXKTrainingDifficulty Difficulty,
	const int32 Chapter) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 SafeChapter = FMath::Clamp(Chapter, 1, 3);
	const int32 FirstStageNumber = (SafeChapter - 1) * 3 + 1;
	if (!Subsystem)
	{
		return FGameXXKTrainingRules::MakeStageId(Difficulty, FirstStageNumber);
	}

	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	FGameXXKTrainingStageDefinition CurrentTravelDefinition;
	if (FGameXXKTrainingRules::TryGetStageDefinition(
			Progress.CurrentTravelStageId,
			CurrentTravelDefinition)
		&& CurrentTravelDefinition.Difficulty == Difficulty
		&& CurrentTravelDefinition.Chapter == SafeChapter)
	{
		return Progress.CurrentTravelStageId;
	}

	for (int32 LocalStage = 2; LocalStage >= 0; --LocalStage)
	{
		const FName Candidate = FGameXXKTrainingRules::MakeStageId(
			Difficulty,
			FirstStageNumber + LocalStage);
		if (FGameXXKTrainingRules::CanTravel(Progress, Candidate)
			|| FGameXXKTrainingRules::CanChallenge(Progress, Candidate))
		{
			return Candidate;
		}
	}
	return FGameXXKTrainingRules::MakeStageId(Difficulty, FirstStageNumber);
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::ResolvePreferredTrainingChapter(
	const EGameXXKTrainingDifficulty Difficulty) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return 1;
	}
	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	FGameXXKTrainingStageDefinition CurrentTravelDefinition;
	if (FGameXXKTrainingRules::TryGetStageDefinition(
			Progress.CurrentTravelStageId,
			CurrentTravelDefinition)
		&& CurrentTravelDefinition.Difficulty == Difficulty)
	{
		return FMath::Clamp(CurrentTravelDefinition.Chapter, 1, 3);
	}
	for (int32 StageNumber = 9; StageNumber >= 1; --StageNumber)
	{
		const FName Candidate = FGameXXKTrainingRules::MakeStageId(Difficulty, StageNumber);
		if (FGameXXKTrainingRules::CanTravel(Progress, Candidate)
			|| FGameXXKTrainingRules::CanChallenge(Progress, Candidate))
		{
			return ((StageNumber - 1) / 3) + 1;
		}
	}
	return 1;
}

void UGameXXKDesktopTrainingWorkbenchWidget::SynchronizeTrainingPageFromStage(const FName StageId)
{
	FGameXXKTrainingStageDefinition Definition;
	if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
	{
		return;
	}
	ActiveTrainingDifficultyIndex = FMath::Clamp(static_cast<int32>(Definition.Difficulty), 0, 2);
	ActiveTrainingChapter = FMath::Clamp(Definition.Chapter, 1, 3);
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
	SynchronizeTrainingPageFromStage(StageId);
	bTrainingDifficultyDropdownOpen = false;
	return Subsystem->SelectTrainingStage(StageId);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickChallengeForTest()
{
	ApplyAction(6);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		&& (Subsystem->IsTrainingChallengeRouteMapActive()
			|| Subsystem->IsTrainingChallengeBattleActive());
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
	if (!Subsystem->AdvanceTrainingTravelStep(bEncounterCompleted, bCompleted, bDefeated, Reward, FMath::Max(0, ElapsedSeconds)))
	{
		return false;
	}
	const FGameXXKTrainingTravelRuntime AfterStep = Subsystem->GetTrainingTravelRuntimeCopy();
	TravelVisualRuntime.NotifyTravelStep(Before, AfterStep, bEncounterCompleted, bCompleted, bDefeated);
	if (bEncounterCompleted && Reward.bChestRolled)
	{
		SetNotice(
			FText::FromString(FString::Printf(
				TEXT("获得了%s"),
				Reward.ChestTier == EGameXXKTrainingRewardTier::AdvancedChest
					? TEXT("高级宝箱")
					: TEXT("普通宝箱"))),
			EGameXXKDesktopNoticeCategory::ChestAcquired);
	}
	if (bCompleted)
	{
		SetNotice(
			FText::FromString(FString::Printf(
				TEXT("通关了关卡 %s"),
				*Before.StageId.ToString())),
			EGameXXKDesktopNoticeCategory::StageCleared);
	}
	if (bDefeated)
	{
		SetNotice(
			FText::FromString(FString::Printf(
				TEXT("关卡 %s 游历失败"),
				*Before.StageId.ToString())),
			EGameXXKDesktopNoticeCategory::StageFailed);
		Subsystem->ResolveTrainingTravelFailure();
	}
	TravelVisualRuntime.Synchronize(Subsystem->GetTrainingTravelRuntimeCopy());
	RefreshLivePresentation(false);
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

void UGameXXKDesktopTrainingWorkbenchWidget::SimulateViewportReattachForTest()
{
	bLayoutRefreshPending = false;
	TGuardValue<bool> InternalLayoutRebuildGuard(bInternalLayoutRebuild, true);
	ReleaseSlateResources(true);
	BuildProgrammaticLayout();
	TakeWidget();
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

bool UGameXXKDesktopTrainingWorkbenchWidget::IsNoticeAction(const int32 ActionId) const
{
	return ActionId >= ActionNoticeSurface
		&& ActionId < ActionNoticeCategoryFirst + UE_ARRAY_COUNT(NoticeCategories);
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleActionHoverChanged(
	const int32 ActionId,
	const bool bHovered)
{
	if (!IsNoticeAction(ActionId) || bBackpackExpanded)
	{
		return;
	}
	if (bHovered)
	{
		++NoticeHoveredWidgetCount;
		NoticeHoverHideRemainingSeconds = 0.0f;
	}
	else
	{
		NoticeHoveredWidgetCount = FMath::Max(0, NoticeHoveredWidgetCount - 1);
		if (NoticeHoveredWidgetCount == 0)
		{
			NoticeHoverHideRemainingSeconds = 0.2f;
		}
	}
	RefreshNoticeControlVisibility();
}

float UGameXXKDesktopTrainingWorkbenchWidget::GetNoticePanelLogicalHeight() const
{
	const EDesktopNoticeDisplayMode EffectiveMode = bBackpackExpanded || bIdleStripFolded
		? EDesktopNoticeDisplayMode::Single
		: NoticeDisplayMode;
	const int32 LineCount = EffectiveMode == EDesktopNoticeDisplayMode::Long
		? 18
		: (EffectiveMode == EDesktopNoticeDisplayMode::Medium ? 5 : 1);
	const bool bShowSettings = !bBackpackExpanded && !bIdleStripFolded && bNoticeSettingsOpen;
	const float ContentHeight = bShowSettings
		? 34.0f + UE_ARRAY_COUNT(NoticeCategories) * 29.0f
		: LineCount * NoticeLineHeight;
	return ContentHeight + NoticeRecordsBarHeight;
}

FVector2D UGameXXKDesktopTrainingWorkbenchWidget::GetCurrentDesignCanvasSize() const
{
	if (bBackpackExpanded)
	{
		return GameXXKDesktopTrainingLayout::GetExpandedReferenceCanvasSize(
				bExpandUpward,
				GetNoticePanelLogicalHeight())
			+ FVector2D(
				GameXXKDesktopTrainingLayout::GetExpandedLeftExtension(
					bWarehousePanelOpen),
				0.0f);
	}
	if (bIdleStripFolded)
	{
		return FVector2D(FoldedSummaryWidth, GetNoticePanelLogicalHeight());
	}
	const FVector2D StripSize = GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize();
	return FVector2D(StripSize.X, StripSize.Y + GetNoticePanelLogicalHeight());
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyUpwardExpansionTransforms()
{
	if (!RootCanvas)
	{
		return;
	}
	const FVector2D BodyOffset = DesktopOverlayPlacement.BodyOffset;
	if (BodyOffset.IsNearlyZero())
	{
		return;
	}
	for (int32 ChildIndex = 0; ChildIndex < RootCanvas->GetChildrenCount(); ++ChildIndex)
	{
		UWidget* Child = RootCanvas->GetChildAt(ChildIndex);
		if (!Child
			|| Child->GetFName() == TEXT("TrainingTravelStrip")
			|| Child->GetFName() == TEXT("DesktopInventoryNoticePanel")
			|| Child->GetFName() == TEXT("IdleStripFoldButton")
			|| Child->GetFName() == TEXT("TrainingWaveProgressPanel")
			|| Child->GetFName() == TEXT("BackpackTabToggleButton")
			|| Child->GetFName() == TEXT("TrainingFoldedNormalChestText")
			|| Child->GetFName() == TEXT("TrainingFoldedAdvancedChestText")
			|| Child->GetFName() == TEXT("TrainingNormalChestButton")
			|| Child->GetFName() == TEXT("TrainingAdvancedChestButton")
			|| Child->GetFName() == TEXT("TravelRetryButton"))
		{
			continue;
		}
		Child->SetRenderTranslation(BodyOffset);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleActionMouseWheel(
	const int32 ActionId,
	const float WheelDelta)
{
	if (!IsNoticeAction(ActionId)
		|| bBackpackExpanded
		|| bNoticeSettingsOpen
		|| NoticeDisplayMode == EDesktopNoticeDisplayMode::Single
		|| FMath::IsNearlyZero(WheelDelta))
	{
		return false;
	}

	int32 EnabledEntryCount = 0;
	for (const FDesktopNoticeEntry& Entry : NoticeHistory)
	{
		EnabledEntryCount += NoticeCategoryEnabled.FindRef(Entry.Category) ? 1 : 0;
	}
	const int32 VisibleCapacity = NoticeDisplayMode == EDesktopNoticeDisplayMode::Long ? 18 : 5;
	const int32 MaximumOffset = FMath::Max(0, EnabledEntryCount - VisibleCapacity);
	NoticeScrollOffset = FMath::Clamp(
		NoticeScrollOffset + (WheelDelta > 0.0f ? 1 : -1),
		0,
		MaximumOffset);
	RefreshNoticePresentation();
	return true;
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
	LoadHudScaleSetting();
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DesktopTrainingWorkbenchWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}
	if (!DesktopOverlayRootCanvas)
	{
		DesktopOverlayRootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("DesktopTrainingOverlayRoot"));
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
	if (!HudDesignCanvas)
	{
		HudDesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("DesktopTrainingDesignCanvas"));
	}
	if (!DesktopCursorCanvas)
	{
		DesktopCursorCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("DesktopCursorCanvas"));
	}
	if (!DesktopOverlayRootCanvas || !RootScaleBox || !ReferenceCanvasBox
		|| !HudDesignCanvas || !RootCanvas || !DesktopCursorCanvas)
	{
		return;
	}
	RootScaleBox->SetStretch(EStretch::ScaleToFit);
	RootScaleBox->SetStretchDirection(EStretchDirection::Both);
	HudDesignCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DesktopCursorCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FVector2D DesignCanvasSize = GetCurrentDesignCanvasSize();
	ReferenceCanvasBox->SetWidthOverride(DesignCanvasSize.X);
	ReferenceCanvasBox->SetHeightOverride(DesignCanvasSize.Y);
	if (ReferenceCanvasBox->GetContent() != HudDesignCanvas)
	{
		ReferenceCanvasBox->SetContent(HudDesignCanvas);
	}
	if (RootScaleBox->GetContent() != ReferenceCanvasBox)
	{
		RootScaleBox->SetContent(ReferenceCanvasBox);
	}
	if (RootScaleBox->GetParent() != DesktopOverlayRootCanvas)
	{
		RootScaleBox->RemoveFromParent();
		DesktopHudCanvasSlot = DesktopOverlayRootCanvas->AddChildToCanvas(RootScaleBox);
	}
	else
	{
		DesktopHudCanvasSlot = Cast<UCanvasPanelSlot>(RootScaleBox->Slot);
	}
	if (DesktopHudCanvasSlot)
	{
		DesktopHudCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		DesktopHudCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		DesktopHudCanvasSlot->SetAutoSize(false);
		DesktopHudCanvasSlot->SetZOrder(1);
	}
	if (DesktopCursorCanvas->GetParent() != DesktopOverlayRootCanvas)
	{
		DesktopCursorCanvas->RemoveFromParent();
		DesktopCursorCanvasSlot = DesktopOverlayRootCanvas->AddChildToCanvas(DesktopCursorCanvas);
	}
	else
	{
		DesktopCursorCanvasSlot = Cast<UCanvasPanelSlot>(DesktopCursorCanvas->Slot);
	}
	if (DesktopCursorCanvasSlot)
	{
		DesktopCursorCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		DesktopCursorCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		DesktopCursorCanvasSlot->SetAutoSize(false);
		DesktopCursorCanvasSlot->SetPosition(FVector2D::ZeroVector);
		DesktopCursorCanvasSlot->SetZOrder(20);
	}
	++ProgrammaticLayoutBuildCount;
	WidgetTree->RootWidget = DesktopOverlayRootCanvas;
	UpdateDesktopOverlayPlacement(DesktopOverlayHostSize);
	HudDesignCanvas->ClearChildren();
	DesktopCursorCanvas->ClearChildren();
	RootCanvas->RemoveFromParent();
	RootCanvasDesignSlot = HudDesignCanvas->AddChildToCanvas(RootCanvas);
	if (RootCanvasDesignSlot)
	{
		RootCanvasDesignSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		RootCanvasDesignSlot->SetAlignment(FVector2D::ZeroVector);
		RootCanvasDesignSlot->SetAutoSize(false);
		RootCanvasDesignSlot->SetZOrder(1);
		RootCanvasDesignSlot->SetPosition(DesktopOverlayPlacement.ContentOffset);
		RootCanvasDesignSlot->SetSize(
			bBackpackExpanded
				? FVector2D(
					GameXXKDesktopTrainingLayout::GetReferenceCanvasSize().X,
					DesignCanvasSize.Y)
				: DesignCanvasSize);
	}
	TravelVisualViewport = nullptr;
	IdleGroupCanvas = nullptr;
	TravelBackgroundImageA = nullptr;
	TravelBackgroundImageB = nullptr;
	TravelBackgroundImages.Reset();
	TravelEnemyImages.Reset();
	TravelHeroImage = nullptr;
	TravelCompanionImages.Reset();
	TravelEnemyHealthTracks.Reset();
	TravelEnemyHealthFills.Reset();
	TravelHeroHealth = nullptr;
	TravelCompanionHealthBars.Reset();
	EmbeddedInventoryWidget = nullptr;
	TownToggleButton = nullptr;
	StoryQuestButton = nullptr;
	if (ResetCombatGuideButton)
	{
		FGameXXKGuideTargetRegistry::Get().UnregisterTarget(
			TEXT("Desktop.Settings.ResetCombatGuide"),
			ResetCombatGuideButton);
	}
	ResetCombatGuideButton = nullptr;
	BackpackGoldText = nullptr;
	TrainingNormalChestButton = nullptr;
	TrainingAdvancedChestButton = nullptr;
	TrainingNormalChestCountText = nullptr;
	TrainingAdvancedChestCountText = nullptr;
	TrainingWaveProgressFill = nullptr;
	TrainingWaveStageText = nullptr;
	TrainingWaveIndexText = nullptr;
	TrainingWaveMarkerImages.Reset();
	TrainingFoldedNormalChestText = nullptr;
	TrainingFoldedAdvancedChestText = nullptr;
	NoticePanel = nullptr;
	NoticeText = nullptr;
	NoticeSurfaceButton = nullptr;
	NoticeRecordsBar = nullptr;
	NoticeSettingsPanel = nullptr;
	NoticeLineTexts.Reset();
	NoticeHoveredWidgetCount = 0;
	NoticeHoverHideRemainingSeconds = 0.0f;
	WarehousePageText = nullptr;
	WarehouseFooterText = nullptr;
	WarehouseBatchToBackpackButton = nullptr;
	BackpackBatchToWarehouseButton = nullptr;
	WarehousePageButtons.Reset();
	ToolProgressText = nullptr;
	ToolCraftLevelText = nullptr;
	ToolConfirmButton = nullptr;
	bHasLivePresentationSnapshot = false;
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
	bDesktopNativeInputRegionDirty = true;
	UpdateTownPresentationInputLock();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWorkbenchShell()
{
	if (bBackpackExpanded)
	{
		AddCanvasRect(
			RootCanvas,
			MakeTransparentPanel(WidgetTree, TEXT("CenterWorkbenchFrame")),
			GameXXKDesktopTrainingLayout::GetCenterShellRect());
	}
	BuildTopIdleStrip();
	BuildBackpackTabToggle();
	if (bBackpackExpanded)
	{
		BuildTownToggleButton();
		BuildStoryQuestButton();
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
		BuildSharedGoldIndicator();
		BuildTopToolbar();
		if (bSettingsPanelOpen)
		{
			BuildHudSettingsPanel();
		}
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
	BuildNoticeRail();
	if (bBackpackExpanded && !DesktopOverlayPlacement.BodyOffset.IsNearlyZero())
	{
		ApplyUpwardExpansionTransforms();
	}
	EnsureGuideSurfaces();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTownToggleButton()
{
	if (!RootCanvas || !bBackpackExpanded)
	{
		return;
	}
	TownToggleButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TownToggleButton"));
	TownToggleButton->Configure(this, ActionToggleTown);
	const TCHAR* TexturePath =
		PresentationMode == EGameXXKDesktopHudPresentationMode::TownViewport
			? DesktopTownExitButtonTexturePath
			: DesktopTownEnterButtonTexturePath;
	TownToggleButton->SetStyle(MakeImageButtonStyle(
		TexturePath,
		GameXXKDesktopTrainingLayout::GetTownToggleButtonSize()));
	TownToggleButton->SetBackgroundColor(FLinearColor::White);
	TownToggleButton->SetContent(nullptr);
	TownToggleButton->SetScaleOnPress(true);
	TownToggleButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	TownToggleButton->SetIsEnabled(!bTownMapTravelPending);
	AddCanvasRect(
		RootCanvas,
		TownToggleButton.Get(),
		DesktopOverlayPlacement.TownToggleRect);
	if (UCanvasPanelSlot* TownCanvasSlot =
		Cast<UCanvasPanelSlot>(TownToggleButton->Slot))
	{
		TownCanvasSlot->SetZOrder(0);
	}
	ActionButtons.Add(TownToggleButton);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildStoryQuestButton()
{
	if (!RootCanvas || !bBackpackExpanded)
	{
		return;
	}
	StoryQuestButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("StoryQuestButton"));
	StoryQuestButton->Configure(this, ActionStoryQuest);
	StoryQuestButton->SetStyle(MakeImageButtonStyle(
		DesktopStoryQuestButtonTexturePath,
		GameXXKDesktopTrainingLayout::GetStoryQuestButtonSize()));
	StoryQuestButton->SetBackgroundColor(FLinearColor::White);
	StoryQuestButton->SetContent(nullptr);
	StoryQuestButton->SetScaleOnPress(true);
	StoryQuestButton->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	StoryQuestButton->SetIsEnabled(!bTownMapTravelPending);
	AddCanvasRect(
		RootCanvas,
		StoryQuestButton.Get(),
		DesktopOverlayPlacement.StoryQuestRect);
	if (UCanvasPanelSlot* StoryCanvasSlot =
		Cast<UCanvasPanelSlot>(StoryQuestButton->Slot))
	{
		StoryCanvasSlot->SetZOrder(0);
	}
	ActionButtons.Add(StoryQuestButton);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBackpackTabToggle()
{
	UGameXXKDesktopTrainingActionButton* Toggle = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("BackpackTabToggleButton"));
	Toggle->Configure(this, 60);
	Toggle->SetStyle(MakeTextureButtonStyle(
		bBackpackExpanded ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
		FVector2D(IdleSummaryTabWidth, NoticeLineHeight),
		FMargin(0.08f)));
	Toggle->SetBackgroundColor(FLinearColor::White);
	Toggle->SetContent(MakeButtonText(
		WidgetTree,
		FText::FromString(bBackpackExpanded ? TEXT("▲") : TEXT("▼")),
		16,
		Ink));
	Toggle->SetToolTipText(FText::FromString(
		bBackpackExpanded
			? TEXT("关闭背包与全部子界面；历练挂机继续运行")
			: TEXT("菜单 [Tab]：展开角色背包")));
	AddCanvas(
		RootCanvas.Get(),
		Toggle,
		GetNoticeRailLogicalPosition() + FVector2D(
			ResolveIdleSummaryTabX(bBackpackExpanded),
			0.0f),
		FVector2D(IdleSummaryTabWidth, NoticeLineHeight));
	ActionButtons.Add(Toggle);
}

FVector2D UGameXXKDesktopTrainingWorkbenchWidget::GetNoticeRailLogicalPosition() const
{
	if (!bBackpackExpanded)
	{
		return FVector2D(
			0.0f,
			bIdleStripFolded
				? 0.0f
				: GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize().Y);
	}
	if (bIdleStripFolded)
	{
		const FVector4 StripRect =
			GameXXKDesktopTrainingLayout::GetExpandedIdleStripRect(bExpandUpward);
		return FVector2D(StripRect.X, StripRect.Y);
	}
	return GameXXKDesktopTrainingLayout::GetExpandedNoticeRailPosition(bExpandUpward);
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildIdleSummaryControls(
	const FVector2D& RowOrigin)
{
	const float ProgressWidth = ResolveIdleSummaryProgressWidth(bBackpackExpanded);
	const float TrackWidth = ResolveWaveTrackWidth(bBackpackExpanded);
	const float TabX = ResolveIdleSummaryTabX(bBackpackExpanded);
	UGameXXKDesktopTrainingActionButton* FoldButton =
		WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			TEXT("IdleStripFoldButton"));
	FoldButton->Configure(this, ActionIdleStripFold);
	FoldButton->SetStyle(MakeTextureButtonStyle(
		bIdleStripFolded ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
		FVector2D(IdleSummaryFoldButtonWidth, NoticeLineHeight),
		FMargin(0.08f)));
	FoldButton->SetBackgroundColor(FLinearColor::White);
	FoldButton->SetContent(MakeButtonText(
		WidgetTree,
		FText::FromString(bIdleStripFolded ? TEXT("▼") : TEXT("▲")),
		16,
		Ink));
	FoldButton->SetToolTipText(FText::FromString(
		bIdleStripFolded ? TEXT("向下展开挂机栏") : TEXT("向上折叠挂机栏")));
	FoldButton->SetVisibility(ESlateVisibility::Visible);
	AddCanvas(
		RootCanvas.Get(),
		FoldButton,
		RowOrigin + FVector2D(IdleSummaryFoldButtonX, 0.0f),
		FVector2D(IdleSummaryFoldButtonWidth, NoticeLineHeight));
	ActionButtons.Add(FoldButton);

	UBorder* ProgressPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TrainingWaveProgressPanel"));
	ProgressPanel->SetBrush(MakeBoxTextureBrush(
		CharacterTabNormalTexturePath,
		FVector2D(ProgressWidth, NoticeLineHeight),
		FMargin(0.08f),
		FLinearColor(0.28f, 0.25f, 0.21f, 0.96f)));
	ProgressPanel->SetBrushColor(FLinearColor::White);
	UCanvasPanel* ProgressCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("TrainingWaveProgressCanvas"));
	ProgressPanel->SetContent(ProgressCanvas);
	AddCanvas(
		RootCanvas.Get(),
		ProgressPanel,
		RowOrigin + FVector2D(IdleSummaryProgressX, 0.0f),
		FVector2D(ProgressWidth, NoticeLineHeight));

	TrainingWaveStageText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		11,
		FLinearColor(0.92f, 0.82f, 0.61f, 1.0f),
		TEXT("TrainingWaveStageText"));
	TrainingWaveStageText->SetAutoWrapText(false);
	AddCanvas(
		ProgressCanvas,
		TrainingWaveStageText.Get(),
		FVector2D(5.0f, 3.0f),
		FVector2D(75.0f, 19.0f));

	FSlateBrush SolidBrush;
	SolidBrush.DrawAs = ESlateBrushDrawType::Box;
	SolidBrush.ImageSize = FVector2D(TrackWidth, 4.0f);
	UBorder* Track = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TrainingWaveProgressTrack"));
	Track->SetBrush(SolidBrush);
	Track->SetBrushColor(FLinearColor(0.10f, 0.085f, 0.07f, 0.92f));
	Track->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvas(
		ProgressCanvas,
		Track,
		FVector2D(WaveTrackX, 10.0f),
		FVector2D(TrackWidth, 4.0f));

	TrainingWaveProgressFill = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TrainingWaveProgressFill"));
	TrainingWaveProgressFill->SetBrush(SolidBrush);
	TrainingWaveProgressFill->SetBrushColor(FLinearColor(0.50f, 0.70f, 0.47f, 0.96f));
	TrainingWaveProgressFill->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvas(
		ProgressCanvas,
		TrainingWaveProgressFill.Get(),
		FVector2D(WaveTrackX + TrackWidth - 2.0f, 10.0f),
		FVector2D(2.0f, 4.0f));

	static constexpr const TCHAR* MarkerPaths[] = {
		TrainingWaveMarkerBossTexturePath,
		TrainingWaveMarkerNormalTexturePath,
		TrainingWaveMarkerEliteTexturePath,
		TrainingWaveMarkerNormalTexturePath,
		TrainingWaveMarkerEliteTexturePath,
		TrainingWaveMarkerNormalTexturePath,
		TrainingWaveMarkerNormalTexturePath};
	TrainingWaveMarkerImages.Reset();
	for (int32 MarkerIndex = 0; MarkerIndex < UE_ARRAY_COUNT(MarkerPaths); ++MarkerIndex)
	{
		UImage* Marker = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("TrainingWaveMarker_%d"), MarkerIndex));
		Marker->SetBrush(MakeTextureBrush(MarkerPaths[MarkerIndex], FVector2D(18.0f, 18.0f)));
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		Marker->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		const float CenterX = WaveTrackX
			+ TrackWidth * static_cast<float>(MarkerIndex) / 6.0f;
		AddCanvas(
			ProgressCanvas,
			Marker,
			FVector2D(CenterX - 9.0f, 3.0f),
			FVector2D(18.0f, 18.0f));
		TrainingWaveMarkerImages.Add(Marker);
	}

	TrainingWaveIndexText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		11,
		FLinearColor(0.94f, 0.90f, 0.78f, 1.0f),
		TEXT("TrainingWaveIndexText"));
	TrainingWaveIndexText->SetAutoWrapText(false);
	TrainingWaveIndexText->SetJustification(ETextJustify::Center);
	AddCanvas(
		ProgressCanvas,
		TrainingWaveIndexText.Get(),
		FVector2D(ProgressWidth - 48.0f, 3.0f),
		FVector2D(44.0f, 19.0f));

	if (bIdleStripFolded)
	{
		TrainingFoldedNormalChestText = MakeText(
			WidgetTree,
			FText::GetEmpty(),
			12,
			FLinearColor(0.50f, 0.82f, 0.90f, 1.0f),
			TEXT("TrainingFoldedNormalChestText"));
		TrainingFoldedAdvancedChestText = MakeText(
			WidgetTree,
			FText::GetEmpty(),
			12,
			FLinearColor(0.93f, 0.72f, 0.27f, 1.0f),
			TEXT("TrainingFoldedAdvancedChestText"));
		AddCanvas(
			RootCanvas.Get(),
			TrainingFoldedNormalChestText.Get(),
			RowOrigin + FVector2D(TabX + IdleSummaryTabWidth, 0.0f),
			FVector2D(FoldedChestTextWidth, NoticeLineHeight));
		AddCanvas(
			RootCanvas.Get(),
			TrainingFoldedAdvancedChestText.Get(),
			RowOrigin + FVector2D(
				TabX + IdleSummaryTabWidth + FoldedChestTextWidth,
				0.0f),
			FVector2D(FoldedChestTextWidth, NoticeLineHeight));
	}

	UpdateWaveProgressPresentation(CaptureLivePresentationSnapshot());
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildNoticeRail()
{
	LoadNoticeCategorySettings();

	const EDesktopNoticeDisplayMode EffectiveMode = bBackpackExpanded || bIdleStripFolded
		? EDesktopNoticeDisplayMode::Single
		: NoticeDisplayMode;
	const int32 LineCount = EffectiveMode == EDesktopNoticeDisplayMode::Long
		? 18
		: (EffectiveMode == EDesktopNoticeDisplayMode::Medium ? 5 : 1);
	const bool bShowSettings = !bBackpackExpanded && !bIdleStripFolded && bNoticeSettingsOpen;
	const float SettingsHeight = 34.0f + UE_ARRAY_COUNT(NoticeCategories) * 29.0f;
	const float ContentHeight = bShowSettings
		? SettingsHeight
		: LineCount * NoticeLineHeight;
	const FVector2D NoticeSize(420.0f, ContentHeight + NoticeRecordsBarHeight);

	NoticePanel = MakeTransparentPanel(WidgetTree, TEXT("DesktopInventoryNoticePanel"));
	NoticePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanel* NoticeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("DesktopNoticeCanvas"));
	NoticePanel->SetContent(NoticeCanvas);
	const FVector2D NoticePosition = GetNoticeRailLogicalPosition();
	AddCanvas(RootCanvas, NoticePanel.Get(), NoticePosition, NoticeSize);
	if (UCanvasPanelSlot* NoticeSlot = Cast<UCanvasPanelSlot>(NoticePanel->Slot))
	{
		NoticeSlot->SetZOrder(90);
	}

	NoticeSurfaceButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("DesktopNoticeSurfaceButton"));
	NoticeSurfaceButton->Configure(this, ActionNoticeSurface);
	NoticeSurfaceButton->SetStyle(MakeInvisibleButtonStyle());
	NoticeSurfaceButton->SetBackgroundColor(FLinearColor::Transparent);
	NoticeSurfaceButton->SetVisibility(bShowSettings
		? ESlateVisibility::Collapsed
		: (bBackpackExpanded ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible));

	UCanvasPanel* LinesCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("DesktopNoticeLinesCanvas"));
	LinesCanvas->SetClipping(EWidgetClipping::ClipToBounds);
	for (int32 LineIndex = 0; LineIndex < LineCount; ++LineIndex)
	{
		UBorder* Row = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("DesktopNoticeLine_%d"), LineIndex));
		Row->SetBrush(MakeBoxTextureBrush(
			CharacterTabNormalTexturePath,
			FVector2D(420.0f, NoticeLineHeight - 1.0f),
			FMargin(0.08f),
			FLinearColor(0.33f, 0.29f, 0.24f, 0.97f)));
		Row->SetBrushColor(FLinearColor::White);
		Row->SetPadding(FMargin(8.0f, 2.0f, 8.0f, 1.0f));
		UTextBlock* LineText = MakeText(
			WidgetTree,
			FText::GetEmpty(),
			13,
			FLinearColor(0.94f, 0.90f, 0.78f, 1.0f),
			LineIndex == LineCount - 1
				? FName(TEXT("DesktopInventoryNoticeText"))
				: NAME_None);
		LineText->SetAutoWrapText(false);
		Row->SetContent(LineText);
		AddCanvas(
			LinesCanvas,
			Row,
			FVector2D(0.0f, LineIndex * NoticeLineHeight),
			FVector2D(420.0f, NoticeLineHeight - 1.0f));
		NoticeLineTexts.Add(LineText);
	}
	NoticeText = NoticeLineTexts.IsEmpty() ? nullptr : NoticeLineTexts.Last();
	NoticeSurfaceButton->SetContent(LinesCanvas);
	AddCanvas(
		NoticeCanvas,
		NoticeSurfaceButton.Get(),
		FVector2D::ZeroVector,
		FVector2D(420.0f, LineCount * NoticeLineHeight));
	ActionButtons.Add(NoticeSurfaceButton);

	if (bShowSettings)
	{
		NoticeSettingsPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("DesktopNoticeSettingsPanel"));
		NoticeSettingsPanel->SetBrush(MakeBoxTextureBrush(
			PanelLargeTexturePath,
			FVector2D(420.0f, SettingsHeight),
			FMargin(0.08f),
			FLinearColor(0.20f, 0.18f, 0.16f, 0.98f)));
		NoticeSettingsPanel->SetBrushColor(FLinearColor::White);
		UCanvasPanel* SettingsCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("DesktopNoticeSettingsCanvas"));
		NoticeSettingsPanel->SetContent(SettingsCanvas);
		UTextBlock* SettingsTitle = MakeButtonText(
			WidgetTree,
			FText::FromString(TEXT("消息设置")),
			15,
			Gold);
		AddCanvas(SettingsCanvas, SettingsTitle, FVector2D(12.0f, 5.0f), FVector2D(396.0f, 25.0f));
		for (int32 CategoryIndex = 0; CategoryIndex < UE_ARRAY_COUNT(NoticeCategories); ++CategoryIndex)
		{
			const EGameXXKDesktopNoticeCategory Category = NoticeCategories[CategoryIndex];
			const bool bEnabled = NoticeCategoryEnabled.FindRef(Category);
			UGameXXKDesktopTrainingActionButton* CategoryButton =
				WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
					UGameXXKDesktopTrainingActionButton::StaticClass(),
					*FString::Printf(TEXT("DesktopNoticeCategoryButton_%d"), CategoryIndex));
			CategoryButton->Configure(this, ActionNoticeCategoryFirst + CategoryIndex);
			CategoryButton->SetStyle(MakeTextureButtonStyle(
				CharacterTabNormalTexturePath,
				FVector2D(396.0f, 27.0f),
				FMargin(0.08f),
				FLinearColor(0.31f, 0.28f, 0.24f, 0.98f)));
			CategoryButton->SetBackgroundColor(FLinearColor::White);
			CategoryButton->SetContent(MakeButtonText(
				WidgetTree,
				FText::FromString(FString::Printf(
					TEXT("%s  %s"),
					bEnabled ? TEXT("✓") : TEXT("□"),
					*NoticeCategoryLabel(Category))),
				13,
				bEnabled ? NoticeCategoryColor(Category) : FLinearColor(0.52f, 0.50f, 0.47f, 0.86f)));
			AddCanvas(
				SettingsCanvas,
				CategoryButton,
				FVector2D(12.0f, 33.0f + CategoryIndex * 29.0f),
				FVector2D(396.0f, 27.0f));
			ActionButtons.Add(CategoryButton);
		}
		AddCanvas(
			NoticeCanvas,
			NoticeSettingsPanel.Get(),
			FVector2D::ZeroVector,
			FVector2D(420.0f, SettingsHeight));
	}

	NoticeRecordsBar = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("DesktopNoticeRecordsBar"));
	UGameXXKDesktopTrainingActionButton* RecordsHoverSurface =
		WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			TEXT("DesktopNoticeRecordsHoverSurface"));
	RecordsHoverSurface->Configure(this, ActionNoticeSurface);
	RecordsHoverSurface->SetStyle(MakeTextureButtonStyle(
		CharacterTabSelectedTexturePath,
		FVector2D(420.0f, NoticeRecordsBarHeight),
		FMargin(0.08f),
		FLinearColor(0.34f, 0.11f, 0.08f, 0.98f)));
	RecordsHoverSurface->SetBackgroundColor(FLinearColor::White);
	AddCanvas(NoticeRecordsBar.Get(), RecordsHoverSurface, FVector2D::ZeroVector, FVector2D(420.0f, NoticeRecordsBarHeight));
	ActionButtons.Add(RecordsHoverSurface);
	UTextBlock* RecordsLabel = MakeButtonText(
		WidgetTree,
		FText::FromString(TEXT("RECORDS")),
		12,
		Gold);
	RecordsLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvas(NoticeRecordsBar.Get(), RecordsLabel, FVector2D(100.0f, 3.0f), FVector2D(220.0f, 22.0f));

	const auto AddRecordsButton = [this](
		const FName Name,
		const int32 ActionId,
		const TCHAR* Label,
		const float X)
	{
		UGameXXKDesktopTrainingActionButton* Button =
			WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
				UGameXXKDesktopTrainingActionButton::StaticClass(),
				Name);
		Button->Configure(this, ActionId);
		Button->SetStyle(MakeTextureButtonStyle(
			CharacterTabNormalTexturePath,
			FVector2D(36.0f, 24.0f),
			FMargin(0.08f),
			FLinearColor(0.42f, 0.35f, 0.28f, 1.0f)));
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(MakeButtonText(WidgetTree, FText::FromString(Label), 12, Gold));
		AddCanvas(NoticeRecordsBar.Get(), Button, FVector2D(X, 2.0f), FVector2D(36.0f, 24.0f));
		ActionButtons.Add(Button);
	};
	if (EffectiveMode != EDesktopNoticeDisplayMode::Single)
	{
		AddRecordsButton(TEXT("DesktopNoticeCollapseButton"), ActionNoticeCollapse, TEXT("折"), 4.0f);
	}
	if (EffectiveMode != EDesktopNoticeDisplayMode::Long)
	{
		AddRecordsButton(
			TEXT("DesktopNoticeExpandButton"),
			ActionNoticeExpand,
			TEXT("展"),
			EffectiveMode == EDesktopNoticeDisplayMode::Medium ? 43.0f : 4.0f);
	}
	AddRecordsButton(
		TEXT("DesktopNoticeSettingsButton"),
		ActionNoticeSettings,
		bShowSettings ? TEXT("×") : TEXT("设"),
		380.0f);
	AddCanvas(
		NoticeCanvas,
		NoticeRecordsBar.Get(),
		FVector2D(0.0f, ContentHeight),
		FVector2D(420.0f, NoticeRecordsBarHeight));
	BuildIdleSummaryControls(NoticePosition);

	RefreshNoticePresentation();
	RefreshNoticeControlVisibility();
}

void UGameXXKDesktopTrainingWorkbenchWidget::LoadNoticeCategorySettings()
{
	if (bNoticeSettingsLoaded)
	{
		return;
	}
	NoticeCategoryEnabled.Reset();
	for (const EGameXXKDesktopNoticeCategory Category : NoticeCategories)
	{
		bool bEnabled = true;
		if (GConfig)
		{
			GConfig->GetBool(
				NoticeSettingsSection,
				NoticeCategoryConfigKey(Category),
				bEnabled,
				GGameUserSettingsIni);
		}
		NoticeCategoryEnabled.Add(Category, bEnabled);
	}
	bNoticeSettingsLoaded = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::SaveNoticeCategorySetting(
	const EGameXXKDesktopNoticeCategory Category) const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetBool(
		NoticeSettingsSection,
		NoticeCategoryConfigKey(Category),
		NoticeCategoryEnabled.FindRef(Category),
		GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
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
		{19, TEXT("TopToolbarSettings"), TEXT("设"), TEXT("HUD设置")},
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
		else if (Specs[Index].ActionId == 19)
		{
			UImage* SettingsIcon = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				TEXT("TopToolbarSettingsIcon"));
			SettingsIcon->SetBrush(MakeTextureBrush(
				SettingsTexturePath,
				FVector2D(30.0f, 30.0f)));
			SettingsIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			Button->SetContent(SettingsIcon);
		}
		else
		{
			Button->SetContent(MakeButtonText(WidgetTree, FText::FromString(Specs[Index].Label), 15, Ink));
		}
		Button->SetToolTipText(FText::FromString(Specs[Index].Tooltip));
		AddCanvas(RootCanvas, Button, FVector2D(1028.0f + Index * 47.0f, 252.0f), FVector2D(42.0f, 36.0f));
		ActionButtons.Add(Button);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildHudSettingsPanel()
{
	UBorder* SettingsPanel = MakePanel(
		WidgetTree,
		PanelAlt,
		TEXT("DesktopHudSettingsPanel"));
	SettingsPanel->SetPadding(FMargin(12.0f));
	AddCanvas(RootCanvas, SettingsPanel, FVector2D(1115.0f, 270.0f), FVector2D(220.0f, 184.0f));
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(SettingsPanel->Slot))
	{
		PanelSlot->SetZOrder(85);
	}

	UCanvasPanel* SettingsCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("DesktopHudSettingsCanvas"));
	SettingsPanel->SetContent(SettingsCanvas);
	UTextBlock* Title = MakeButtonText(
		WidgetTree,
		FText::FromString(TEXT("设置")),
		17,
		Gold);
	AddCanvas(SettingsCanvas, Title, FVector2D(0.0f, 0.0f), FVector2D(196.0f, 26.0f));
	UTextBlock* ScaleLabel = MakeText(
		WidgetTree,
		FText::FromString(TEXT("HUD缩放")),
		14,
		Ink,
		TEXT("HudScaleSettingLabel"));
	AddCanvas(SettingsCanvas, ScaleLabel, FVector2D(4.0f, 38.0f), FVector2D(58.0f, 28.0f));

	const struct FScaleOption
	{
		int32 Percent;
		int32 ActionId;
		const TCHAR* Name;
		float X;
	} Options[] = {
		{50, ActionHudScale50, TEXT("HudScale50Button"), 64.0f},
		{75, ActionHudScale75, TEXT("HudScale75Button"), 108.0f},
		{100, ActionHudScale100, TEXT("HudScale100Button"), 152.0f}};
	for (const FScaleOption& Option : Options)
	{
		const bool bSelected = HudScalePercent == Option.Percent;
		UGameXXKDesktopTrainingActionButton* Button =
			WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
				UGameXXKDesktopTrainingActionButton::StaticClass(),
				Option.Name);
		Button->Configure(this, Option.ActionId);
		Button->SetStyle(MakeTextureButtonStyle(
			bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(42.0f, 34.0f),
			FMargin(0.08f)));
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetContent(MakeButtonText(
			WidgetTree,
			FText::FromString(FString::Printf(TEXT("%d%%"), Option.Percent)),
			12,
			bSelected ? Gold : Ink));
		AddCanvas(SettingsCanvas, Button, FVector2D(Option.X, 34.0f), FVector2D(42.0f, 34.0f));
		ActionButtons.Add(Button);
	}

	ResetCombatGuideButton =
		WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			TEXT("ResetCombatGuideButton"));
	ResetCombatGuideButton->Configure(this, ActionResetCombatGuide);
	ResetCombatGuideButton->SetStyle(MakeTextureButtonStyle(
		CharacterTabNormalTexturePath,
		FVector2D(192.0f, 38.0f),
		FMargin(0.08f)));
	ResetCombatGuideButton->SetBackgroundColor(FLinearColor::White);
	ResetCombatGuideButton->SetContent(MakeButtonText(
		WidgetTree,
		FText::FromString(TEXT("重置战斗引导")),
		14,
		Ink));
	AddCanvas(
		SettingsCanvas,
		ResetCombatGuideButton.Get(),
		FVector2D(2.0f, 116.0f),
		FVector2D(192.0f, 38.0f));
	ActionButtons.Add(ResetCombatGuideButton);
	FGameXXKGuideTargetRegistry::Get().RegisterWidgetTarget(
		TEXT("Desktop.Settings.ResetCombatGuide"),
		ResetCombatGuideButton);
}

void UGameXXKDesktopTrainingWorkbenchWidget::EnsureGuideSurfaces()
{
	if (!WidgetTree || !DesktopOverlayRootCanvas)
	{
		return;
	}
	const auto AttachFullscreen = [this](UWidget* Widget, const int32 ZOrder)
	{
		if (!Widget || Widget->GetParent() == DesktopOverlayRootCanvas)
		{
			return;
		}
		Widget->RemoveFromParent();
		if (UCanvasPanelSlot* CanvasSlot = DesktopOverlayRootCanvas->AddChildToCanvas(Widget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(ZOrder);
		}
	};
	if (!GuideOverlayWidget)
	{
		GuideOverlayWidget = WidgetTree->ConstructWidget<UGameXXKGuideOverlayWidget>(
			UGameXXKGuideOverlayWidget::StaticClass(),
			TEXT("DesktopGuideOverlay"));
	}
	if (!GuidePreferenceWidget)
	{
		GuidePreferenceWidget = WidgetTree->ConstructWidget<UGameXXKGuidePreferenceWidget>(
			UGameXXKGuidePreferenceWidget::StaticClass(),
			TEXT("DesktopGuidePreference"));
		GuidePreferenceWidget->SetPreferenceChosenDelegate(
			FGameXXKGuidePreferenceChosen::CreateUObject(
				this,
				&UGameXXKDesktopTrainingWorkbenchWidget::HandleGuidePreferenceChosen));
	}
	AttachFullscreen(GuideOverlayWidget, 900);
	AttachFullscreen(GuidePreferenceWidget, 910);
	if (!GuideCoordinator)
	{
		GuideCoordinator = NewObject<UGameXXKGuideCoordinator>(this);
	}
	FGameXXKGuideTargetRegistry& GuideRegistry = FGameXXKGuideTargetRegistry::Get();
	if (!GuideEventHandle.IsValid())
	{
		GuideEventHandle = GuideRegistry.OnGuideEvent().AddUObject(
			this,
			&UGameXXKDesktopTrainingWorkbenchWidget::HandleGuideEvent);
	}
	const TWeakObjectPtr<UGameXXKGuideCoordinator> WeakCoordinator(GuideCoordinator);
	GuideRegistry.SetActionGate(this, [WeakCoordinator](const FName ActionId)
	{
		const UGameXXKGuideCoordinator* Coordinator = WeakCoordinator.Get();
		return !Coordinator || Coordinator->CanExecuteAction(ActionId);
	});
	RefreshGuideSurfaces();
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshGuideSurfaces()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !GuideCoordinator || !GuideOverlayWidget || !GuidePreferenceWidget)
	{
		return;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	GuideCoordinator->Bind(
		State.GuideProgress,
		FGameXXKGuideTargetRegistry::Get(),
		GuideOverlayWidget);
	GuideCoordinator->SetPersistenceDelegate(
		FGameXXKGuidePersistenceDelegate::CreateUObject(
			this,
			&UGameXXKDesktopTrainingWorkbenchWidget::PersistGuideProgressCandidate));

	GuidePreferenceWidget->RefreshFromProgress(State.GuideProgress);

	if (!State.GuideProgress.ActiveGuideId.IsNone())
	{
		FString AssetName = State.GuideProgress.ActiveGuideId.ToString();
		AssetName.ReplaceInline(TEXT("."), TEXT("_"));
		AssetName = TEXT("DA_") + AssetName;
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/GameXXK/Narrative/Guides/%s.%s"),
			*AssetName,
			*AssetName);
		if (UGameXXKGuideAsset* Asset = LoadObject<UGameXXKGuideAsset>(nullptr, *ObjectPath))
		{
			const FGameXXKGuideStepDefinition* ActiveStep =
				Asset->FindStep(State.GuideProgress.ActiveGuideStepId);
			if (ActiveStep
				&& FGameXXKGuideTargetRegistry::Get().IsTargetRegistered(ActiveStep->TargetId))
			{
				GuideCoordinator->ResumeGuide(*Asset);
			}
			else
			{
				GuideOverlayWidget->DismissGuide();
			}
		}
	}
	else
	{
		GuideOverlayWidget->DismissGuide();
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleGuidePreferenceChosen(
	const EGameXXKGuidePreference Preference)
{
	if (!GuideCoordinator)
	{
		return;
	}
	FString Error;
	if (GuideCoordinator->ApplyPreference(Preference, &Error))
	{
		SetNotice(FText::FromString(
			Preference == EGameXXKGuidePreference::NewPlayer
				? TEXT("已开启战斗引导")
				: TEXT("已跳过战斗引导")));
		RefreshGuideSurfaces();
	}
	else
	{
		SetNotice(FText::FromString(Error.IsEmpty() ? TEXT("战斗引导设置未保存") : Error));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::HandleGuideEvent(const FName EventId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !GuideCoordinator)
	{
		return;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	FString Error;
	if (!State.GuideProgress.ActiveGuideId.IsNone())
	{
		FString AssetName = State.GuideProgress.ActiveGuideId.ToString();
		AssetName.ReplaceInline(TEXT("."), TEXT("_"));
		AssetName = TEXT("DA_") + AssetName;
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/GameXXK/Narrative/Guides/%s.%s"),
			*AssetName,
			*AssetName);
		if (UGameXXKGuideAsset* Asset = LoadObject<UGameXXKGuideAsset>(nullptr, *ObjectPath))
		{
			GuideCoordinator->ResumeGuide(*Asset, &Error);
			GuideCoordinator->HandleEvent(EventId, &Error);
		}
		return;
	}
	if (State.GuideProgress.Preference != EGameXXKGuidePreference::NewPlayer)
	{
		return;
	}

	FName GuideId;
	if (EventId == TEXT("Event.RouteMap.Opened")) GuideId = TEXT("Guide.RouteMap.Basic");
	else if (EventId == TEXT("Event.Battle.Opened")) GuideId = TEXT("Guide.Battle.Basic");
	else if (EventId == TEXT("Event.Merchant.Opened")) GuideId = TEXT("Guide.Merchant.Basic");
	else if (EventId == TEXT("Event.Route.EventOpened")) GuideId = TEXT("Guide.Event.Basic");
	else if (EventId == TEXT("Event.Route.CampOpened")) GuideId = TEXT("Guide.Camp.Basic");
	else if (EventId == TEXT("Event.Route.ChestOpened")) GuideId = TEXT("Guide.Chest.Basic");
	else if (EventId == TEXT("Event.Boss.Opened")) GuideId = TEXT("Guide.Boss.Basic");
	else if (EventId == TEXT("Event.Settlement.Opened")) GuideId = TEXT("Guide.Settlement.Basic");
	if (GuideId.IsNone())
	{
		return;
	}

	const FGameXXKTaskProgress* TrackedTask =
		State.NarrativeProgress.TaskProgressById.Find(State.NarrativeProgress.TrackedTaskId);
	if (!TrackedTask
		|| TrackedTask->State != EGameXXKTaskState::Active
		|| TrackedTask->CurrentStepId != TEXT("Step.Main.XuXiake.CombatTutorial"))
	{
		return;
	}
	FString AssetName = GuideId.ToString();
	AssetName.ReplaceInline(TEXT("."), TEXT("_"));
	AssetName = TEXT("DA_") + AssetName;
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/GameXXK/Narrative/Guides/%s.%s"),
		*AssetName,
		*AssetName);
	if (UGameXXKGuideAsset* Asset = LoadObject<UGameXXKGuideAsset>(nullptr, *ObjectPath))
	{
		GuideCoordinator->StartGuide(*Asset, EventId, &Error);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::PersistGuideProgressCandidate(
	const FGameXXKGuideProgress& Candidate)
{
	if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		return Subsystem->CommitGuideProgress(Candidate);
	}
	return false;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleResetCombatGuide()
{
	if (!FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Desktop.ResetCombatGuide")))
	{
		return false;
	}
	if (!GuideCoordinator)
	{
		EnsureGuideSurfaces();
	}
	FString Error;
	if (!GuideCoordinator || !GuideCoordinator->ResetCombatGuide(&Error))
	{
		SetNotice(FText::FromString(Error.IsEmpty() ? TEXT("战斗引导重置失败") : Error));
		return false;
	}
	SetNotice(FText::FromString(TEXT("战斗引导已重置，下次进入引导时会重新询问")));
	RefreshGuideSurfaces();
	return true;
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
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow
		&& DesktopCursorCanvas)
	{
		AddCanvas(
			DesktopCursorCanvas,
			CarriedItemImage.Get(),
			FVector2D::ZeroVector,
			FVector2D(56.0f, 56.0f));
	}
	else
	{
		AddCanvas(
			RootCanvas,
			CarriedItemImage.Get(),
			FVector2D(800.0f, 470.0f),
			FVector2D(56.0f, 56.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTopIdleStrip()
{
	UBorder* Strip = MakeTransparentPanel(WidgetTree, TEXT("TrainingTravelStrip"));
	TravelVisualViewport = Strip;
	FVector4 StripRect = GameXXKDesktopTrainingLayout::GetIdleStripRect();
	if (!bBackpackExpanded)
	{
		const FVector2D CollapsedSize = GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize();
		StripRect = FVector4(0.0f, 0.0f, CollapsedSize.X, CollapsedSize.Y);
	}
	else if (bExpandUpward)
	{
		StripRect = GameXXKDesktopTrainingLayout::GetExpandedIdleStripRect(true);
	}
	AddCanvasRect(RootCanvas, Strip, StripRect);
	if (bIdleStripFolded)
	{
		Strip->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	UScaleBox* IdleGroupScale = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("TrainingIdleGroupScale"));
	IdleGroupScale->SetStretch(EStretch::ScaleToFit);
	IdleGroupScale->SetStretchDirection(EStretchDirection::Both);
	USizeBox* IdleGroupReference = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("TrainingIdleGroupReference"));
	IdleGroupReference->SetWidthOverride(IdleGroupLogicalSize.X);
	IdleGroupReference->SetHeightOverride(IdleGroupLogicalSize.Y);
	IdleGroupCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("TrainingIdleGroupCanvas"));
	IdleGroupReference->SetContent(IdleGroupCanvas.Get());
	IdleGroupScale->SetContent(IdleGroupReference);
	Strip->SetContent(IdleGroupScale);
	UCanvasPanel* TravelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TravelVisualCanvas"));
	if (TravelCanvas)
	{
		TravelCanvas->SetClipping(EWidgetClipping::ClipToBounds);
		AddCanvas(IdleGroupCanvas.Get(), TravelCanvas, FVector2D::ZeroVector, TravelVisualSize);
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
			const FVector2D HealthPosition(33.0f + EnemySlotIndex * 125.0f, 174.0f);
			FSlateBrush SolidBarBrush;
			SolidBarBrush.DrawAs = ESlateBrushDrawType::Box;
			SolidBarBrush.ImageSize = TravelHealthBarSize;

			UBorder* EnemyHealthTrack = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("TravelEnemyHealthTrack_%d"), EnemySlotIndex));
			UBorder* EnemyHealthFill = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("TravelEnemyHealth_%d"), EnemySlotIndex));
			if (!EnemyHealthTrack || !EnemyHealthFill)
			{
				continue;
			}

			EnemyHealthTrack->SetBrush(SolidBarBrush);
			EnemyHealthTrack->SetBrushColor(FLinearColor(0.48f, 0.48f, 0.48f, 1.0f));
			EnemyHealthTrack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			EnemyHealthTrack->SetRenderOpacity(0.0f);
			if (UCanvasPanelSlot* TrackSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(EnemyHealthTrack)))
			{
				TrackSlot->SetPosition(HealthPosition);
				TrackSlot->SetSize(TravelHealthBarSize);
				TrackSlot->SetZOrder(3);
			}

			EnemyHealthFill->SetBrush(SolidBarBrush);
			EnemyHealthFill->SetBrushColor(FLinearColor(0.46f, 0.047f, 0.026f, 1.0f));
			EnemyHealthFill->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
			EnemyHealthFill->SetRenderScale(FVector2D(0.0f, 1.0f));
			EnemyHealthFill->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			EnemyHealthFill->SetRenderOpacity(0.0f);
			EnemyHealthFill->ForceVolatile(true);
			if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(TravelCanvas->AddChild(EnemyHealthFill)))
			{
				FillSlot->SetPosition(HealthPosition);
				FillSlot->SetSize(TravelHealthBarSize);
				FillSlot->SetZOrder(4);
			}
			TravelEnemyHealthTracks.Add(EnemyHealthTrack);
			TravelEnemyHealthFills.Add(EnemyHealthFill);
		}

		TravelHeroHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TravelHeroHealth"));
		if (TravelHeroHealth)
		{
			TravelHeroHealth->ForceVolatile(true);
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
			CompanionHealth->ForceVolatile(true);
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
		TEXT("待领取：%d金币 / %d经验 / 普通箱%d / 高级箱%d\n普通箱冷却 %s · 高级箱冷却 %s"),
		PendingReward.Gold,
		PendingReward.Experience,
		PendingReward.NormalChestCount,
		PendingReward.AdvancedChestCount,
		*FormatCooldown(Progress.TravelNormalChestCooldownRemainingSeconds),
		*FormatCooldown(Progress.TravelAdvancedChestCooldownRemainingSeconds));
	Strip->SetToolTipText(FText::FromString(RewardTooltip));
	const int32 NormalChestCount = Subsystem ? Subsystem->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest) : 0;
	const int32 AdvancedChestCount = Subsystem ? Subsystem->GetTrainingChestCount(EGameXXKTrainingRewardTier::AdvancedChest) : 0;
	const float ChestControlX = bBackpackExpanded
		? StripRect.X + GameXXKDesktopTrainingLayout::GetIdleStripChestControlX()
		: 953.0f;
	const float ChestControlY = bBackpackExpanded ? StripRect.Y : 0.0f;
	const struct FChestButtonSpec
	{
		const TCHAR* Name;
		const TCHAR* TexturePath;
		int32 ActionId;
		int32 Count;
		const TCHAR* Label;
		float Y;
	} ChestButtons[] = {
		{TEXT("TrainingNormalChestButton"), TrainingNormalChestTexturePath, 600, NormalChestCount, TEXT("普通历练宝箱"), 8.0f},
		{TEXT("TrainingAdvancedChestButton"), TrainingAdvancedChestTexturePath, 601, AdvancedChestCount, TEXT("高级历练宝箱"), 84.0f},
	};
	for (const FChestButtonSpec& Spec : ChestButtons)
	{
		UGameXXKDesktopTrainingActionButton* ChestButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(), Spec.Name);
		ChestButton->Configure(this, Spec.ActionId);
		ChestButton->SetStyle(MakeImageButtonStyle(ItemSlotTexturePath, FVector2D(72.0f, 72.0f)));
		ChestButton->SetBackgroundColor(FLinearColor::White);
		UTextBlock* ChestCountText = nullptr;
		const bool bAdvancedChest = Spec.ActionId == 601;
		ChestButton->SetContent(MakeIconLabelContent(
			WidgetTree,
			Spec.TexturePath,
			FVector2D(66.0f, 66.0f),
			FText::FromString(FString::FromInt(Spec.Count)),
			13,
			bAdvancedChest
				? FName(TEXT("TrainingAdvancedChestCountText"))
				: FName(TEXT("TrainingNormalChestCountText")),
			&ChestCountText,
			bAdvancedChest
				? FName(TEXT("TrainingAdvancedChestIcon"))
				: FName(TEXT("TrainingNormalChestIcon")),
			true));
		ChestButton->SetIsEnabled(Spec.Count > 0);
		ChestButton->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("%s ×%d\n左键开启1个；右键开启全部"), Spec.Label, Spec.Count)));
		AddCanvas(
			RootCanvas.Get(),
			ChestButton,
			FVector2D(ChestControlX, ChestControlY + Spec.Y),
			FVector2D(72.0f, 72.0f));
		ActionButtons.Add(ChestButton);
		if (bAdvancedChest)
		{
			TrainingAdvancedChestButton = ChestButton;
			TrainingAdvancedChestCountText = ChestCountText;
		}
		else
		{
			TrainingNormalChestButton = ChestButton;
			TrainingNormalChestCountText = ChestCountText;
		}
	}
	UGameXXKDesktopTrainingActionButton* RetryButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TravelRetryButton"));
	RetryButton->Configure(this, 10);
	const bool bRetryEnabled = Progress.bRetryOnFailure;
	RetryButton->SetStyle(MakeImageButtonStyle(
		TrainingRetryButtonBaseTexturePath,
		FVector2D(52.0f, 52.0f)));
	RetryButton->SetBackgroundColor(FLinearColor::White);
	UImage* RetryIcon = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("TravelRetryIcon"));
	RetryIcon->SetBrush(MakeTextureBrush(
		bRetryEnabled
			? TrainingRetryIconEnabledTexturePath
			: TrainingRetryIconDisabledTexturePath,
		FVector2D(36.0f, 36.0f)));
	RetryIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	RetryButton->SetContent(RetryIcon);
	RetryButton->SetToolTipText(FText::FromString(
		bRetryEnabled
			? TEXT("失败自动重试：已开启\n点击关闭；关闭后阵亡会回退到前一关。")
			: TEXT("失败自动重试：已关闭\n点击开启；1-1失败仍重试1-1。")));
	AddCanvas(
		RootCanvas.Get(),
		RetryButton,
		FVector2D(ChestControlX - 60.0f, ChestControlY + 18.0f),
		FVector2D(52.0f, 52.0f));
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
		TravelHeroImage->SetRenderTranslation(FVector2D::ZeroVector);
		TravelHeroImage->SetRenderOpacity(1.0f);
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
		const EGameXXKBattleAnimationAction HeroDisplayAction =
			HeroAction == EGameXXKBattleAnimationAction::Hit
				|| HeroAction == EGameXXKBattleAnimationAction::Death
			? EGameXXKBattleAnimationAction::Idle
			: HeroAction;
		FGameXXKBattleAnimationClipDescriptor HeroClip = FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			MakeTravelOneKUnitId(FGameXXKEquipmentRules::HeroCharacterId()),
			NAME_None,
			false,
			HeroDisplayAction);
		float HeroPhaseDuration = 0.0f;
		switch (TravelVisualRuntime.GetVisualPhase())
		{
		case EGameXXKTrainingTravelVisualPhase::HeroAttack: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds; break;
		case EGameXXKTrainingTravelVisualPhase::HeroHit: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds; break;
		case EGameXXKTrainingTravelVisualPhase::HeroDeath: HeroPhaseDuration = FGameXXKTrainingTravelVisualRuntime::HeroDeathSeconds; break;
		default: break;
		}
		if (HeroPhaseDuration > 0.0f && HeroDisplayAction != EGameXXKBattleAnimationAction::Idle)
		{
			HeroClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(HeroClip, HeroPhaseDuration);
		}
		if (!ApplyTravelAnimationFrame(
			TravelHeroImage,
			HeroClip,
			HeroDisplayAction == EGameXXKBattleAnimationAction::Idle,
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
		const float HeroContentScale = ResolveTravelHeroContentScale(HeroDisplayAction);
		TravelHeroImage->SetRenderScale(FVector2D(
			HeroContentScale,
			HeroContentScale));
		TravelHeroImage->SetRenderTranslation(FVector2D::ZeroVector);
		if (HeroAction == EGameXXKBattleAnimationAction::Hit)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds,
				0.0f,
				1.0f);
			TravelHeroImage->SetRenderTranslation(
				FGameXXKBattleAnimationPresentation::CalculateProceduralHitOffset(false, Progress)
				* (TravelCombatVisualSize.X / 410.0f));
		}
		else if (HeroAction == EGameXXKBattleAnimationAction::Death
			&& TravelHeroImage->GetRenderOpacity() > 0.0f)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::HeroDeathSeconds,
				0.0f,
				1.0f);
			TravelHeroImage->SetRenderOpacity(
				FGameXXKBattleAnimationPresentation::CalculateProceduralDeathOpacity(Progress));
		}
	}

	const int32 PresentedEnemySlotIndex = TravelVisualRuntime.GetPresentedEnemySlotIndex();
	bool bEnemyVisible = false;
	for (int32 EnemySlotIndex = 0; EnemySlotIndex < TravelEnemyImages.Num(); ++EnemySlotIndex)
	{
		UImage* EnemyImage = TravelEnemyImages[EnemySlotIndex];
		UBorder* EnemyHealthTrack = TravelEnemyHealthTracks.IsValidIndex(EnemySlotIndex)
			? TravelEnemyHealthTracks[EnemySlotIndex]
			: nullptr;
		UBorder* EnemyHealthFill = TravelEnemyHealthFills.IsValidIndex(EnemySlotIndex)
			? TravelEnemyHealthFills[EnemySlotIndex]
			: nullptr;
		const bool bShowEnemy = TravelVisualRuntime.IsEnemySlotVisible(EnemySlotIndex);
		bEnemyVisible |= bShowEnemy;
		if (EnemyImage)
		{
			EnemyImage->SetVisibility(bShowEnemy ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (EnemyHealthTrack)
		{
			EnemyHealthTrack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			EnemyHealthTrack->SetRenderOpacity(bShowEnemy ? 1.0f : 0.0f);
		}
		if (EnemyHealthFill)
		{
			EnemyHealthFill->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			EnemyHealthFill->SetRenderOpacity(bShowEnemy ? 1.0f : 0.0f);
			EnemyHealthFill->ForceVolatile(true);
			if (!bShowEnemy)
			{
				EnemyHealthFill->SetRenderScale(FVector2D(0.0f, 1.0f));
				if (TravelAppliedEnemyHealth.IsValidIndex(EnemySlotIndex))
				{
					TravelAppliedEnemyHealth[EnemySlotIndex] = -1.0f;
				}
			}
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
		const EGameXXKBattleAnimationAction EnemyDisplayAction =
			EnemyAction == EGameXXKBattleAnimationAction::Hit
				|| EnemyAction == EGameXXKBattleAnimationAction::Death
			? EGameXXKBattleAnimationAction::Idle
			: EnemyAction;
		FGameXXKBattleAnimationClipDescriptor EnemyClip = FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(
			MakeTravelOneKUnitId(EnemyId),
			MakeTravelOneKUnitId(EnemyId),
			true,
			EnemyDisplayAction);
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
		if (EnemyPhaseDuration > 0.0f && EnemyDisplayAction != EGameXXKBattleAnimationAction::Idle)
		{
			EnemyClip = FGameXXKBattleAnimationPresentation::FitClipToDuration(EnemyClip, EnemyPhaseDuration);
		}
		ApplyTravelAnimationFrame(
			EnemyImage,
			EnemyClip,
			EnemyDisplayAction == EGameXXKBattleAnimationAction::Idle,
			TravelAppliedEnemyAtlasPaths[EnemySlotIndex],
			TravelAppliedEnemyFrames[EnemySlotIndex]);
		const float EnemyContentScale = ResolveTravelEnemyContentScale(EnemyId, EnemyDisplayAction);
		EnemyImage->SetRenderScale(FVector2D(EnemyContentScale, EnemyContentScale));
		EnemyImage->SetRenderTranslation(FVector2D::ZeroVector);
		if (bPresentedTarget && EnemyAction == EGameXXKBattleAnimationAction::Hit)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds,
				0.0f,
				1.0f);
			EnemyImage->SetRenderTranslation(
				FGameXXKBattleAnimationPresentation::CalculateProceduralHitOffset(true, Progress)
				* (TravelCombatVisualSize.X / 410.0f));
		}
		else if (bPresentedTarget
			&& EnemyAction == EGameXXKBattleAnimationAction::Death
			&& EnemyImage->GetRenderOpacity() > 0.0f)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds,
				0.0f,
				1.0f);
			EnemyImage->SetRenderOpacity(
				FGameXXKBattleAnimationPresentation::CalculateProceduralDeathOpacity(Progress));
		}

		const float EnemyHealth = TravelVisualRuntime.GetEnemyHealthFractionForSlot(EnemySlotIndex);
		if (EnemyHealthFill && !FMath::IsNearlyEqual(EnemyHealth, TravelAppliedEnemyHealth[EnemySlotIndex]))
		{
			EnemyHealthFill->SetRenderScale(FVector2D(EnemyHealth, 1.0f));
			EnemyHealthFill->InvalidateLayoutAndVolatility();
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
				CompanionHealthBar->InvalidateLayoutAndVolatility();
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
		const EGameXXKBattleAnimationAction CompanionDisplayAction =
			CompanionAction == EGameXXKBattleAnimationAction::Hit
				|| CompanionAction == EGameXXKBattleAnimationAction::Death
			? EGameXXKBattleAnimationAction::Idle
			: CompanionAction;
		FGameXXKBattleAnimationClipPair CompanionClips =
			FGameXXKBattleAnimationPresentation::ResolveCompactTravelClipPair(
				CompanionUnitId,
				false,
				CompanionDisplayAction);
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
		if (CompanionPhaseDuration > 0.0f && CompanionDisplayAction != EGameXXKBattleAnimationAction::Idle)
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
			CompanionDisplayAction == EGameXXKBattleAnimationAction::Idle,
			TravelAppliedCompanionAtlasPaths[CompanionIndex],
			TravelAppliedCompanionFrames[CompanionIndex]);
		const float CompanionContentScale = ResolveTravelPartyContentScale(
			CompanionUnitId,
			CompanionDisplayAction);
		CompanionImage->SetRenderScale(FVector2D(CompanionContentScale, CompanionContentScale));
		CompanionImage->SetRenderTranslation(FVector2D::ZeroVector);
		if (CompanionAction == EGameXXKBattleAnimationAction::Hit)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::HeroHitSeconds,
				0.0f,
				1.0f);
			CompanionImage->SetRenderTranslation(
				FGameXXKBattleAnimationPresentation::CalculateProceduralHitOffset(false, Progress)
				* (TravelCombatVisualSize.X / 410.0f));
		}
		else if (CompanionAction == EGameXXKBattleAnimationAction::Death
			&& CompanionImage->GetRenderOpacity() > 0.0f)
		{
			const float Progress = FMath::Clamp(
				TravelVisualRuntime.GetVisualPhaseElapsedSeconds()
					/ FGameXXKTrainingTravelVisualRuntime::HeroDeathSeconds,
				0.0f,
				1.0f);
			CompanionImage->SetRenderOpacity(
				FGameXXKBattleAnimationPresentation::CalculateProceduralDeathOpacity(Progress));
		}
	}
	const float HeroHealth = TravelVisualRuntime.GetHeroHealthFraction();
	if (!FMath::IsNearlyEqual(HeroHealth, TravelAppliedHeroHealth))
	{
		TravelHeroHealth->SetPercent(HeroHealth);
		TravelHeroHealth->InvalidateLayoutAndVolatility();
		TravelAppliedHeroHealth = HeroHealth;
	}
}

UGameXXKDesktopTrainingWorkbenchWidget::FLivePresentationSnapshot
UGameXXKDesktopTrainingWorkbenchWidget::CaptureLivePresentationSnapshot() const
{
	FLivePresentationSnapshot Snapshot;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Snapshot;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	const FGameXXKTrainingTravelRuntime TravelRuntime = Subsystem->GetTrainingTravelRuntimeCopy();
	const FGameXXKTrainingOfflineReward Pending = Subsystem->GetPendingTrainingTravelRewardCopy();
	const FGameXXKToolProgress& ToolProgress = Subsystem->GetToolProgress();
	Snapshot.TravelStageId = TravelRuntime.StageId.IsNone()
		? Progress.CurrentTravelStageId
		: TravelRuntime.StageId;
	Snapshot.TravelEncounterIndex = TravelRuntime.EncounterIndex == INDEX_NONE
		? FMath::Max(0, Progress.ActiveTravelEncounterIndex)
		: TravelRuntime.EncounterIndex;
	Snapshot.PlayerGold = State.PlayerGold;
	Snapshot.PlayerLevel = State.PlayerLevel;
	Snapshot.PlayerExperience = State.PlayerXP;
	Snapshot.PlayerHealth = State.PlayerHP;
	Snapshot.PlayerMana = State.PlayerMP;
	Snapshot.PendingGold = Pending.Gold;
	Snapshot.PendingExperience = Pending.Experience;
	Snapshot.PendingNormalChests = Pending.NormalChestCount;
	Snapshot.PendingAdvancedChests = Pending.AdvancedChestCount;
	Snapshot.HeldNormalChests = Subsystem->GetTrainingChestCount(EGameXXKTrainingRewardTier::NormalChest);
	Snapshot.HeldAdvancedChests = Subsystem->GetTrainingChestCount(EGameXXKTrainingRewardTier::AdvancedChest);
	Snapshot.NormalChestCooldown = Progress.TravelNormalChestCooldownRemainingSeconds;
	Snapshot.AdvancedChestCooldown = Progress.TravelAdvancedChestCooldownRemainingSeconds;
	Snapshot.WarehouseOccupancy = GetWarehouseOccupancyForTest();
	Snapshot.WarehousePageCount = GetWarehousePageCountForTest();
	Snapshot.ToolLevel = ToolProgress.Level;
	Snapshot.ToolExperience = ToolProgress.Experience;
	Snapshot.ToolCraftingLevel = ToolProgress.SelectedCraftingLevel;
	Snapshot.OccupiedToolSlots = GetOccupiedToolSlotCountForTest();
	return Snapshot;
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateWaveProgressPresentation(
	const FLivePresentationSnapshot& Snapshot)
{
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters =
		FGameXXKTrainingRules::BuildEncounterSequence(Snapshot.TravelStageId, true);
	const int32 EncounterCount = FMath::Max(1, Encounters.Num());
	const int32 CurrentEncounter = FMath::Clamp(
		Snapshot.TravelEncounterIndex,
		0,
		EncounterCount - 1);
	if (TrainingWaveStageText)
	{
		TrainingWaveStageText->SetText(FText::FromString(
			TrainingStageShortLabel(Snapshot.TravelStageId)));
	}
	if (TrainingWaveIndexText)
	{
		TrainingWaveIndexText->SetText(FText::FromString(FString::Printf(
			TEXT("%d/%d"),
			CurrentEncounter + 1,
			EncounterCount)));
	}
	if (TrainingWaveProgressFill)
	{
		const float TrackWidth = ResolveWaveTrackWidth(bBackpackExpanded);
		const float Fraction = EncounterCount > 1
			? static_cast<float>(CurrentEncounter) / static_cast<float>(EncounterCount - 1)
			: 1.0f;
		const float FillWidth = FMath::Max(2.0f, TrackWidth * Fraction);
		if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(TrainingWaveProgressFill->Slot))
		{
			FillSlot->SetPosition(FVector2D(
				WaveTrackX + TrackWidth - FillWidth,
				10.0f));
			FillSlot->SetSize(FVector2D(FillWidth, 4.0f));
		}
	}
	for (int32 VisualIndex = 0; VisualIndex < TrainingWaveMarkerImages.Num(); ++VisualIndex)
	{
		UImage* Marker = TrainingWaveMarkerImages[VisualIndex];
		if (!Marker)
		{
			continue;
		}
		const int32 EncounterIndex = TrainingWaveMarkerImages.Num() - 1 - VisualIndex;
		const bool bCurrent = EncounterIndex == CurrentEncounter;
		const bool bCompleted = EncounterIndex < CurrentEncounter;
		Marker->SetColorAndOpacity(bCurrent
			? FLinearColor::White
			: (bCompleted
				? FLinearColor(0.78f, 0.78f, 0.78f, 0.78f)
				: FLinearColor(0.44f, 0.44f, 0.44f, 0.32f)));
		Marker->SetRenderScale(bCurrent
			? FVector2D(1.16f, 1.16f)
			: FVector2D(1.0f, 1.0f));
	}
	if (TrainingFoldedNormalChestText)
	{
		TrainingFoldedNormalChestText->SetText(FText::FromString(FString::Printf(
			TEXT("[普通]：%d"),
			FMath::Max(0, Snapshot.HeldNormalChests))));
	}
	if (TrainingFoldedAdvancedChestText)
	{
		TrainingFoldedAdvancedChestText->SetText(FText::FromString(FString::Printf(
			TEXT("[高级]：%d"),
			FMath::Max(0, Snapshot.HeldAdvancedChests))));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateTrainingChestPresentation(
	const bool bAdvanced,
	const int32 Count)
{
	const int32 SafeCount = FMath::Max(0, Count);
	UTextBlock* CountText = bAdvanced
		? TrainingAdvancedChestCountText.Get()
		: TrainingNormalChestCountText.Get();
	UGameXXKDesktopTrainingActionButton* Button = bAdvanced
		? TrainingAdvancedChestButton.Get()
		: TrainingNormalChestButton.Get();
	if (CountText)
	{
		CountText->SetText(FText::FromString(FString::FromInt(SafeCount)));
	}
	if (Button)
	{
		Button->SetIsEnabled(SafeCount > 0);
		Button->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("%s ×%d\n左键开启1个；右键开启全部"),
			bAdvanced ? TEXT("高级历练宝箱") : TEXT("普通历练宝箱"),
			SafeCount)));
	}
	UTextBlock* FoldedText = bAdvanced
		? TrainingFoldedAdvancedChestText.Get()
		: TrainingFoldedNormalChestText.Get();
	if (FoldedText)
	{
		FoldedText->SetText(FText::FromString(bAdvanced
			? FString::Printf(TEXT("[高级]：%d"), SafeCount)
			: FString::Printf(TEXT("[普通]：%d"), SafeCount)));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateWarehouseNumericPresentation(
	const FLivePresentationSnapshot& Snapshot)
{
	const int32 PageCount = FMath::Max(1, Snapshot.WarehousePageCount);
	const int32 PageIndex = FMath::Clamp(WarehousePageIndex, 0, PageCount - 1);
	if (WarehousePageText)
	{
		WarehousePageText->SetText(FText::FromString(FString::Printf(
			TEXT("第 %d / %d 页 · 每页 %d 格"),
			PageIndex + 1,
			PageCount,
			WarehousePageSize)));
	}
	if (WarehouseFooterText)
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const int32 Capacity = Subsystem
			? FGameXXKTalentRules::GetUnlockedWarehouseCapacity(Subsystem->GetRuntimeState())
			: WarehousePageSize;
		WarehouseFooterText->SetText(FText::FromString(FString::Printf(
			TEXT("仓库物品 %d / %d\n不显示角色身份卡"),
			Snapshot.WarehouseOccupancy,
			Capacity)));
	}
	if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		const TSet<FGameXXKDesktopInventoryEntryKey> Exclusions = BuildBatchTransferExclusions();
		FGameXXKDesktopInventoryBatchTransferRequest ToBackpack;
		ToBackpack.FromContainer = EGameXXKDesktopItemContainer::Warehouse;
		ToBackpack.ToContainer = EGameXXKDesktopItemContainer::Backpack;
		ToBackpack.WarehousePageIndex = PageIndex;
		ToBackpack.WarehousePageSize = WarehousePageSize;
		ToBackpack.ExcludedEntries = Exclusions;
		FGameXXKDesktopInventoryBatchTransferRequest ToWarehouse;
		ToWarehouse.FromContainer = EGameXXKDesktopItemContainer::Backpack;
		ToWarehouse.ToContainer = EGameXXKDesktopItemContainer::Warehouse;
		ToWarehouse.WarehousePageIndex = PageIndex;
		ToWarehouse.WarehousePageSize = WarehousePageSize;
		ToWarehouse.ExcludedEntries = Exclusions;
		if (WarehouseBatchToBackpackButton)
		{
			WarehouseBatchToBackpackButton->SetIsEnabled(
				FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(
					Subsystem->GetRuntimeState(), ToBackpack));
		}
		if (BackpackBatchToWarehouseButton)
		{
			BackpackBatchToWarehouseButton->SetIsEnabled(
				FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(
					Subsystem->GetRuntimeState(), ToWarehouse));
		}
	}
	for (int32 PageButtonIndex = 0; PageButtonIndex < WarehousePageButtons.Num(); ++PageButtonIndex)
	{
		if (UGameXXKDesktopTrainingActionButton* PageButton = WarehousePageButtons[PageButtonIndex])
		{
			PageButton->SetIsEnabled(PageButtonIndex < PageCount);
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateToolNumericPresentation(
	const FLivePresentationSnapshot& Snapshot)
{
	if (ToolProgressText)
	{
		const int64 NextExperience = FGameXXKEquipmentToolRules::GetExperienceForNextLevel(Snapshot.ToolLevel);
		ToolProgressText->SetText(FText::FromString(
			Snapshot.ToolLevel >= FGameXXKEquipmentToolRules::MaximumLevel
				? FString::Printf(TEXT("工具 Lv.%d  MAX"), Snapshot.ToolLevel)
				: FString::Printf(
					TEXT("工具 Lv.%d  %lld/%lld"),
					Snapshot.ToolLevel,
					Snapshot.ToolExperience,
					NextExperience)));
	}
	if (ToolCraftLevelText)
	{
		ToolCraftLevelText->SetText(FText::FromString(FString::Printf(
			TEXT("合成等级 %d"),
			Snapshot.ToolCraftingLevel)));
	}
	if (ToolConfirmButton)
	{
		ToolConfirmButton->SetIsEnabled(Snapshot.OccupiedToolSlots > 0);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshLivePresentation(const bool bForce)
{
	if (GetVisibility() == ESlateVisibility::Collapsed
		|| GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}

	const FLivePresentationSnapshot Snapshot = CaptureLivePresentationSnapshot();
	const bool bOuterValuesChanged = bForce
		|| !bHasLivePresentationSnapshot
		|| !LastLivePresentationSnapshot.Equals(Snapshot);
	if (bOuterValuesChanged)
	{
		if (bHasLivePresentationSnapshot
			&& Snapshot.PlayerLevel > LastLivePresentationSnapshot.PlayerLevel)
		{
			SetNotice(
				FText::FromString(FString::Printf(
					TEXT("主角升级至 Lv.%d"),
					Snapshot.PlayerLevel)),
				EGameXXKDesktopNoticeCategory::CharacterLevelUp);
		}
		if (BackpackGoldText)
		{
			BackpackGoldText->SetText(FText::FromString(FString::FromInt(Snapshot.PlayerGold)));
		}
		if (TravelVisualViewport)
		{
			const auto FormatCooldown = [](const int32 RemainingSeconds)
			{
				const int32 SafeSeconds = FMath::Max(0, RemainingSeconds);
				return FString::Printf(TEXT("%02d:%02d"), SafeSeconds / 60, SafeSeconds % 60);
			};
			TravelVisualViewport->SetToolTipText(FText::FromString(FString::Printf(
				TEXT("待领取：%d金币 / %d经验 / 普通箱%d / 高级箱%d\n普通箱冷却 %s · 高级箱冷却 %s"),
				Snapshot.PendingGold,
				Snapshot.PendingExperience,
				Snapshot.PendingNormalChests,
				Snapshot.PendingAdvancedChests,
				*FormatCooldown(Snapshot.NormalChestCooldown),
				*FormatCooldown(Snapshot.AdvancedChestCooldown))));
		}
		UpdateTrainingChestPresentation(false, Snapshot.HeldNormalChests);
		UpdateTrainingChestPresentation(true, Snapshot.HeldAdvancedChests);
		UpdateWaveProgressPresentation(Snapshot);
		UpdateWarehouseNumericPresentation(Snapshot);
		UpdateToolNumericPresentation(Snapshot);
	}
	if (EmbeddedInventoryWidget)
	{
		EmbeddedInventoryWidget->RefreshVisibleRuntimeValues();
	}
	LastLivePresentationSnapshot = Snapshot;
	bHasLivePresentationSnapshot = true;
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
		EGameXXKBattleAnimationAction::Attack};
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

	const FName ActiveQuestNpcId = ResolveWorkbenchNpcId(Subsystem);
	if (!ActiveQuestNpcId.IsNone())
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
	// A defeated unit fades this same UImage to zero. A later spawn can reuse
	// the same atlas path and frame, so restore opacity before the cache-hit
	// early return instead of leaving the new unit born invisible.
	Image->SetRenderOpacity(1.0f);
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
	UBorder* ShelfDivider = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WarehouseShelfDivider"));
	FSlateBrush ShelfDividerBrush;
	ShelfDividerBrush.DrawAs = ESlateBrushDrawType::Box;
	ShelfDividerBrush.TintColor = FSlateColor(FLinearColor(0.18f, 0.14f, 0.09f, 0.70f));
	ShelfDivider->SetBrush(ShelfDividerBrush);
	AddCanvas(RootCanvas, ShelfDivider, FVector2D(20.0f, 786.0f), FVector2D(343.0f, 2.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("仓库")), 28, Ink);
	AddCanvas(RootCanvas, Title, FVector2D(30.0f, 258.0f), FVector2D(323.0f, 38.0f));
	BuildPanelCloseButton(TEXT("WarehouseCloseButton"), ActionCloseWarehouse, FVector2D(314.0f, 254.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const int32 WarehousePageCount = GetWarehousePageCountForTest();
	const int32 WarehouseCapacity = RuntimeState
		? FGameXXKTalentRules::GetUnlockedWarehouseCapacity(*RuntimeState)
		: WarehousePageSize;
	for (int32 PageTabIndex = 0; PageTabIndex < 6; ++PageTabIndex)
	{
		UGameXXKDesktopTrainingActionButton* PageTab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			*FString::Printf(TEXT("WarehousePageTab_%d"), PageTabIndex));
		PageTab->Configure(this, 70 + PageTabIndex);
		const bool bSelectedPage = PageTabIndex == GetWarehousePageIndexForTest();
		PageTab->SetStyle(MakeTextureButtonStyle(
			bSelectedPage ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(44.0f, 38.0f),
			FMargin(0.08f)));
		PageTab->SetBackgroundColor(FLinearColor::White);
		PageTab->SetContent(MakeButtonText(
			WidgetTree,
			FText::FromString(FString::FromInt(PageTabIndex + 1)),
			17,
			Ink));
		PageTab->SetIsEnabled(PageTabIndex < WarehousePageCount);
		PageTab->SetToolTipText(FText::FromString(
			PageTabIndex < WarehousePageCount
				? FString::Printf(TEXT("仓库第 %d 页"), PageTabIndex + 1)
				: TEXT("需要容量分支的仓库页天赋")));
		AddCanvas(RootCanvas, PageTab, FVector2D(30.0f + PageTabIndex * 50.0f, 300.0f), FVector2D(44.0f, 38.0f));
		ActionButtons.Add(PageTab);
		WarehousePageButtons.Add(PageTab);
	}
	UScrollBox* WarehouseScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("WarehouseSlotScrollBox"));
	WarehouseScroll->SetOrientation(EOrientation::Orient_Vertical);
	WarehouseScroll->SetAnimateWheelScrolling(true);
	WarehouseScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	USizeBox* WarehouseGridReference = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("WarehouseSlotGridReference"));
	WarehouseGridReference->SetWidthOverride(323.0f);
	WarehouseGridReference->SetHeightOverride(648.0f);
	UCanvasPanel* WarehouseGridCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("WarehouseSlotGridCanvas"));
	WarehouseGridReference->SetContent(WarehouseGridCanvas);
	WarehouseScroll->AddChild(WarehouseGridReference);
	AddCanvas(
		RootCanvas,
		WarehouseScroll,
		FVector2D(20.0f, 342.0f),
		FVector2D(343.0f, 430.0f));
	for (int32 SlotIndex = 0; SlotIndex < WarehousePageSize; ++SlotIndex)
	{
		const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + SlotIndex;
		const bool bSlotUnlocked = PhysicalSlotIndex < WarehouseCapacity;
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
		const FVector2D CellPosition(6.0f + Column * 78.0f, Row * 72.0f);
		const FVector2D CellSize(68.0f, 68.0f);
		const FName CellName(*FString::Printf(TEXT("WarehouseSlot_%d"), SlotIndex));
		UGameXXKDesktopTrainingActionButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			CellName);
		SlotButton->Configure(this, 100 + SlotIndex);
		SlotButton->SetBackgroundColor(FLinearColor::White);
		if (bSlotUnlocked && Entry.IsValid())
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
			SlotButton->SetIsEnabled(bSlotUnlocked && CarriedEntry.IsValid());
			if (!bSlotUnlocked)
			{
				SlotButton->SetBackgroundColor(FLinearColor(0.30f, 0.30f, 0.28f, 0.72f));
				SlotButton->SetToolTipText(FText::FromString(TEXT("该仓库格尚未由永久天赋解锁")));
			}
		}
		AddCanvas(WarehouseGridCanvas, SlotButton, CellPosition, CellSize);
		ActionButtons.Add(SlotButton);
	}
	const int32 WarehouseCount = GetWarehouseOccupancyForTest();
	WarehousePageText = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("第 %d / %d 页 · 每页 %d 格"),
		GetWarehousePageIndexForTest() + 1,
		GetWarehousePageCountForTest(),
		WarehousePageSize)), 15, Ink, TEXT("WarehousePageSummaryText"));
	AddCanvas(RootCanvas, WarehousePageText.Get(), FVector2D(30.0f, 792.0f), FVector2D(300.0f, 24.0f));
	const TSet<FGameXXKDesktopInventoryEntryKey> BatchExclusions = BuildBatchTransferExclusions();
	FGameXXKDesktopInventoryBatchTransferRequest ToBackpackRequest;
	ToBackpackRequest.FromContainer = EGameXXKDesktopItemContainer::Warehouse;
	ToBackpackRequest.ToContainer = EGameXXKDesktopItemContainer::Backpack;
	ToBackpackRequest.WarehousePageIndex = GetWarehousePageIndexForTest();
	ToBackpackRequest.WarehousePageSize = WarehousePageSize;
	ToBackpackRequest.ExcludedEntries = BatchExclusions;
	FGameXXKDesktopInventoryBatchTransferRequest ToWarehouseRequest;
	ToWarehouseRequest.FromContainer = EGameXXKDesktopItemContainer::Backpack;
	ToWarehouseRequest.ToContainer = EGameXXKDesktopItemContainer::Warehouse;
	ToWarehouseRequest.WarehousePageIndex = GetWarehousePageIndexForTest();
	ToWarehouseRequest.WarehousePageSize = WarehousePageSize;
	ToWarehouseRequest.ExcludedEntries = BatchExclusions;

	WarehouseBatchToBackpackButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(), TEXT("WarehouseBatchToBackpackButton"));
	WarehouseBatchToBackpackButton->Configure(this, 40);
	WarehouseBatchToBackpackButton->SetStyle(MakeTextureButtonStyle(
		CharacterTabNormalTexturePath, FVector2D(145.0f, 42.0f), FMargin(0.08f)));
	WarehouseBatchToBackpackButton->SetBackgroundColor(FLinearColor::White);
	WarehouseBatchToBackpackButton->SetContent(MakeButtonText(
		WidgetTree, FText::FromString(TEXT("仓库 → 背包")), 14, Ink));
	WarehouseBatchToBackpackButton->SetToolTipText(FText::FromString(
		TEXT("将当前仓库页的全部装备和道具转入背包；锁定状态保留")));
	WarehouseBatchToBackpackButton->SetIsEnabled(RuntimeState
		&& FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(
			*RuntimeState, ToBackpackRequest));
	AddCanvas(RootCanvas, WarehouseBatchToBackpackButton.Get(), FVector2D(30.0f, 824.0f), FVector2D(145.0f, 42.0f));
	ActionButtons.Add(WarehouseBatchToBackpackButton);

	BackpackBatchToWarehouseButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(), TEXT("BackpackBatchToWarehouseButton"));
	BackpackBatchToWarehouseButton->Configure(this, 41);
	BackpackBatchToWarehouseButton->SetStyle(MakeTextureButtonStyle(
		CharacterTabNormalTexturePath, FVector2D(145.0f, 42.0f), FMargin(0.08f)));
	BackpackBatchToWarehouseButton->SetBackgroundColor(FLinearColor::White);
	BackpackBatchToWarehouseButton->SetContent(MakeButtonText(
		WidgetTree, FText::FromString(TEXT("背包 → 仓库")), 14, Ink));
	BackpackBatchToWarehouseButton->SetToolTipText(FText::FromString(
		TEXT("将背包内全部装备和道具转入当前仓库页；锁定状态保留")));
	BackpackBatchToWarehouseButton->SetIsEnabled(RuntimeState
		&& FGameXXKDesktopInventoryRules::CanBatchTransferCurrentWarehousePage(
			*RuntimeState, ToWarehouseRequest));
	AddCanvas(RootCanvas, BackpackBatchToWarehouseButton.Get(), FVector2D(195.0f, 824.0f), FVector2D(145.0f, 42.0f));
	ActionButtons.Add(BackpackBatchToWarehouseButton);
	WarehouseFooterText = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("仓库物品 %d / %d\n不显示角色身份卡"),
		WarehouseCount,
		WarehouseCapacity)), 16, Ink, TEXT("WarehouseFooterText"));
	AddCanvas(RootCanvas, WarehouseFooterText.Get(), FVector2D(30.0f, 874.0f), FVector2D(310.0f, 32.0f));
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
	BuildPanelCloseButton(TEXT("BackpackPanelCloseButton"), 60, FVector2D(1272.0f, 252.0f));
	if (UButton* CloseButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BackpackPanelCloseButton"))))
	{
		const FText CloseDescription = FText::FromString(TEXT("关闭背包与全部子界面"));
		CloseButton->SetToolTipText(CloseDescription);
		UTextBlock* AccessibleLabel = MakeButtonText(WidgetTree, CloseDescription, 1, FLinearColor::Transparent);
		AccessibleLabel->SetRenderOpacity(0.0f);
		CloseButton->SetContent(AccessibleLabel);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildSharedGoldIndicator()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	UImage* GoldIcon = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("BackpackGoldIcon"));
	GoldIcon->SetBrush(MakeTextureBrush(IngotTexturePath, FVector2D(30.0f, 30.0f)));
	AddCanvas(RootCanvas, GoldIcon, FVector2D(1034.0f, 291.0f), FVector2D(30.0f, 30.0f));
	const FString GoldLabel = RuntimeState
		? FString::FromInt(RuntimeState->PlayerGold)
		: TEXT("--");
	BackpackGoldText = MakeText(
		WidgetTree,
		FText::FromString(GoldLabel),
		18,
		Ink,
		TEXT("BackpackGoldText"));
	AddCanvas(
		RootCanvas,
		BackpackGoldText.Get(),
		FVector2D(1068.0f, 292.0f),
		FVector2D(100.0f, 30.0f));
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
		const bool bSelected = bCharacterRosterMembersExpanded
			&& ActiveCharacterRoster == Tabs[Index].Roster;
		const FName RepresentativeId = ResolveRememberedBackpackCharacterId(Tabs[Index].Roster);
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
		const FString RosterTooltip = bSelected
			? FString::Printf(TEXT("收起%s角色页签；当前查看对象保持不变"), Tabs[Index].Label)
			: FString::Printf(TEXT("展开%s角色页签；这里只切换查看对象，不会改变编队"), Tabs[Index].Label);
		Button->SetToolTipText(FText::FromString(RosterTooltip));
		AddCanvas(RootCanvas, Button, FVector2D(414.0f + Index * 113.0f, 706.0f), FVector2D(105.0f, 62.0f));
		ActionButtons.Add(Button);
	}

	if (!bCharacterRosterMembersExpanded)
	{
		return;
	}

	TArray<FName> VisibleCharacters;
	int32 FirstActionId = INDEX_NONE;
	if (ActiveCharacterRoster == EGameXXKDesktopTrainingCharacterRoster::Hero)
	{
		VisibleCharacters.Add(FGameXXKEquipmentRules::HeroCharacterId());
		FirstActionId = 20;
	}
	else if (ActiveCharacterRoster == EGameXXKDesktopTrainingCharacterRoster::Companions)
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
		FText::FromString(TEXT("查看角色不会换队；只有右侧“编入队伍”会写入当前伙伴或 NPC。")),
		15,
		Ink);
	AddCanvas(RootCanvas, Hint, FVector2D(520.0f, 264.0f), FVector2D(770.0f, 30.0f));

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKCardRunState* CardRun = Subsystem ? &Subsystem->GetRuntimeState().CardRun : nullptr;
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const FName CompanionId = CardRun
		? CardRun->PartySelection.ActivePermanentCompanionInstanceId
		: NAME_None;
	const FName NpcId = ResolveWorkbenchNpcId(Subsystem);
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
			Label = QuestNpcDisplayName(CharacterId);
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
		FText::FromString(TEXT("主角固定；伙伴与 NPC 各一名。候选选择不会改动背包当前查看对象。")),
		15,
		Ink);
	AddCanvas(RootCanvas, Footer, FVector2D(421.0f, 548.0f), FVector2D(470.0f, 64.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTalentsPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt, TEXT("TalentsPanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetContentRect());
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("永久天赋  ·  全队共享")), 25, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(417.0f, 254.0f), FVector2D(700.0f, 36.0f));
	BuildPanelCloseButton(TEXT("TalentsCloseButton"), ActionCloseCentralPage, FVector2D(1284.0f, 258.0f));
	UTextBlock* Notice = MakeText(
		WidgetTree,
		FText::FromString(TEXT("从中心向四个 45° 分区展开 · 滚轮纵向浏览，Shift+滚轮横向浏览 · 每个普通节点可点 5 次")),
		12,
		FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
	AddCanvas(RootCanvas, Notice, FVector2D(420.0f, 292.0f), FVector2D(850.0f, 24.0f));
	UGameXXKTalentTreeWidget* TalentTree =
		WidgetTree->ConstructWidget<UGameXXKTalentTreeWidget>(
			UGameXXKTalentTreeWidget::StaticClass(),
			TEXT("PermanentTalentTreeWidget"));
	TalentTree->SetMVPSubsystem(ResolveMVPSubsystem());
	TalentTree->OnPurchaseCommitted().AddWeakLambda(this, [this]()
	{
		HandleTalentPurchaseCommitted();
	});
	AddCanvas(RootCanvas, TalentTree, FVector2D(409.0f, 318.0f), FVector2D(920.0f, 444.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildToolsPanel()
{
	ToolSlots.SetNum(ToolSlotCount);
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel, TEXT("ToolsPanel"));
	AddCanvasRect(RootCanvas, PanelBorder, GameXXKDesktopTrainingLayout::GetRightShellRect());
	UBorder* ShelfDivider = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ToolsShelfDivider"));
	FSlateBrush ShelfDividerBrush;
	ShelfDividerBrush.DrawAs = ESlateBrushDrawType::Box;
	ShelfDividerBrush.TintColor = FSlateColor(FLinearColor(0.18f, 0.14f, 0.09f, 0.70f));
	ShelfDivider->SetBrush(ShelfDividerBrush);
	AddCanvas(RootCanvas, ShelfDivider, FVector2D(1379.0f, 786.0f), FVector2D(271.0f, 2.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("工具")), 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1387.0f, 258.0f), FVector2D(255.0f, 38.0f));
	BuildPanelCloseButton(TEXT("ToolsCloseButton"), ActionCloseRightPanel, FVector2D(1602.0f, 254.0f));
	FGameXXKTalentProjection ToolTalentProjection;
	const bool bToolsUnlocked = Subsystem
		&& FGameXXKTalentRules::BuildProjection(
			Subsystem->GetRuntimeState().Talents,
			ToolTalentProjection)
		&& ToolTalentProjection.bToolsUnlocked;
	if (!bToolsUnlocked)
	{
		UBorder* LockedPanel = MakePanel(WidgetTree, PanelAlt, TEXT("ToolsTalentLockedPanel"));
		const FSlateColorBrush LockedPaperBrush(FLinearColor(0.91f, 0.84f, 0.69f, 1.0f));
		LockedPanel->SetBrush(LockedPaperBrush);
		LockedPanel->SetClipping(EWidgetClipping::ClipToBounds);
		LockedPanel->SetPadding(FMargin(16.0f, 58.0f, 16.0f, 20.0f));
		UTextBlock* LockedText = MakeText(
			WidgetTree,
			FText::FromString(TEXT("工具功能尚未解锁\n\n在永久天赋中点亮：\n行旅根基 → 百工开物\n\n一次开放：\n分解 / 合成 / 强化\n洗炼 / 镶嵌\n\n工具经验与金币加成也在该分支继续提升。")),
			15,
			Ink);
		LockedText->SetJustification(ETextJustify::Center);
		LockedText->SetWrapTextAt(210.0f);
		LockedPanel->SetContent(LockedText);
		AddCanvas(
			RootCanvas,
			LockedPanel,
			FVector2D(1386.0f, 300.0f),
			FVector2D(258.0f, 460.0f));
		if (UCanvasPanelSlot* LockedSlot = Cast<UCanvasPanelSlot>(LockedPanel->Slot))
		{
			LockedSlot->SetZOrder(100);
		}
	}
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
		AddCanvas(RootCanvas, ToolButton, FVector2D(1385.0f + ToolIndex * 52.0f, 300.0f), FVector2D(47.0f, 40.0f));
		ActionButtons.Add(ToolButton);
	}
	const FGameXXKToolProgress Progress = Subsystem
		? Subsystem->GetToolProgress()
		: FGameXXKToolProgress();
	const int64 NextToolExperience = FGameXXKEquipmentToolRules::GetExperienceForNextLevel(Progress.Level);
	const FString ProgressLabelText = Progress.Level >= FGameXXKEquipmentToolRules::MaximumLevel
		? FString::Printf(TEXT("工具 Lv.%d  MAX"), Progress.Level)
		: FString::Printf(TEXT("工具 Lv.%d  %lld/%lld"), Progress.Level, Progress.Experience, NextToolExperience);
	ToolProgressText = MakeText(
		WidgetTree,
		FText::FromString(ProgressLabelText),
		15,
		Ink,
		TEXT("ToolProgressText"));
	AddCanvas(RootCanvas, ToolProgressText.Get(), FVector2D(1390.0f, 342.0f), FVector2D(245.0f, 22.0f));
	UBorder* GridFrame = MakePanel(WidgetTree, PanelAlt, TEXT("ToolInputGridFrame"));
	AddCanvas(RootCanvas, GridFrame, FVector2D(1385.0f, 366.0f), FVector2D(260.0f, 238.0f));
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
			FVector2D(1408.0f + Column * 72.0f, 382.0f + Row * 72.0f),
			FVector2D(64.0f, 64.0f));
		ActionButtons.Add(ToolSlotButton);
	}
	ToolConfirmButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("ToolConfirmButton"));
	ToolConfirmButton->Configure(this, 309);
	ToolConfirmButton->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(170.0f, 54.0f), FMargin(0.08f)));
	ToolConfirmButton->SetBackgroundColor(FLinearColor::White);
	ToolConfirmButton->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("确定")), 20, Ink));
	ToolConfirmButton->SetIsEnabled(GetOccupiedToolSlotCountForTest() > 0);
	AddCanvas(RootCanvas, ToolConfirmButton.Get(), FVector2D(1430.0f, 824.0f), FVector2D(170.0f, 54.0f));
	ActionButtons.Add(ToolConfirmButton);

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
			ActiveToolCombineKind == EGameXXKToolCombineKind::Equipment ? TEXT("装备") : TEXT("宝石"), 1388.0f, 700.0f, 72.0f);
		AddToolControl(TEXT("ToolAutoFill"), 311, TEXT("自动放置"), 1464.0f, 700.0f, 86.0f);
		const bool bIncludeWarehouse = Subsystem && Subsystem->GetRuntimeState().DesktopInventory.bToolAutoFillIncludesWarehouse;
		AddToolControl(TEXT("ToolIncludeWarehouse"), 312, bIncludeWarehouse ? TEXT("仓库✓") : TEXT("仓库×"), 1554.0f, 700.0f, 82.0f);
	}
	if (Subsystem)
	{
		AddToolControl(TEXT("ToolCraftLevelDown"), 313, TEXT("-"), 1400.0f, 738.0f, 36.0f);
		ToolCraftLevelText = MakeText(
			WidgetTree,
			FText::FromString(FString::Printf(TEXT("合成等级 %d"), Progress.SelectedCraftingLevel)),
			14,
			Ink,
			TEXT("ToolCraftLevelText"));
		AddCanvas(RootCanvas, ToolCraftLevelText.Get(), FVector2D(1442.0f, 744.0f), FVector2D(130.0f, 24.0f));
		AddToolControl(TEXT("ToolCraftLevelUp"), 314, TEXT("+"), 1590.0f, 738.0f, 36.0f);
		if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge && Subsystem->GetRuntimeState().EquipmentCollection.PendingReforge.bActive)
		{
			AddToolControl(TEXT("ToolReforgeAccept"), 315, TEXT("采用新词缀"), 1400.0f, 700.0f, 108.0f);
			AddToolControl(TEXT("ToolReforgeKeep"), 316, TEXT("保留原词缀"), 1516.0f, 700.0f, 108.0f);
		}
		if (ActiveToolMode == EGameXXKDesktopToolMode::Socket)
		{
			AddToolControl(TEXT("ToolSocketPrevious"), 317, TEXT("孔位-"), 1400.0f, 700.0f, 72.0f);
			AddToolControl(TEXT("ToolSocketNext"), 318, FString::Printf(TEXT("孔位%d +"), SelectedToolSocketIndex + 1), 1480.0f, 700.0f, 96.0f);
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
	UTextBlock* Hint = MakeText(WidgetTree, FText::FromString(Description), 14, Ink);
	AddCanvas(RootCanvas, Hint, FVector2D(1398.0f, 664.0f), FVector2D(232.0f, 32.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTrainingMapPanel()
{
	UBorder* Map = MakePanel(WidgetTree, Panel, TEXT("TrainingMapPanel"));
	AddCanvasRect(RootCanvas, Map, GameXXKDesktopTrainingLayout::GetRightShellRect());
	UBorder* ShelfDivider = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TrainingShelfDivider"));
	FSlateBrush ShelfDividerBrush;
	ShelfDividerBrush.DrawAs = ESlateBrushDrawType::Box;
	ShelfDividerBrush.TintColor = FSlateColor(FLinearColor(0.18f, 0.14f, 0.09f, 0.70f));
	ShelfDivider->SetBrush(ShelfDividerBrush);
	AddCanvas(RootCanvas, ShelfDivider, FVector2D(1379.0f, 786.0f), FVector2D(271.0f, 2.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("历练地图")), 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1387.0f, 258.0f), FVector2D(255.0f, 38.0f));
	BuildPanelCloseButton(TEXT("TrainingCloseButton"), ActionCloseRightPanel, FVector2D(1602.0f, 254.0f));
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKTrainingProgress Progress = Subsystem
		? Subsystem->GetTrainingProgressCopy()
		: FGameXXKTrainingProgress();
	ActiveTrainingDifficultyIndex = FMath::Clamp(ActiveTrainingDifficultyIndex, 0, 2);
	ActiveTrainingChapter = FMath::Clamp(ActiveTrainingChapter, 1, 3);
	EGameXXKTrainingDifficulty ActiveDifficulty =
		TrainingDifficultyFromIndex(ActiveTrainingDifficultyIndex);

	auto DifficultyLabel = [](const EGameXXKTrainingDifficulty Difficulty) -> FString
	{
		switch (Difficulty)
		{
		case EGameXXKTrainingDifficulty::Hard: return TEXT("困难");
		case EGameXXKTrainingDifficulty::Hell: return TEXT("地狱");
		case EGameXXKTrainingDifficulty::Normal:
		default: return TEXT("普通");
		}
	};
	auto StageShortLabel = [&DifficultyLabel](const FName StageId) -> FString
	{
		FGameXXKTrainingStageDefinition Definition;
		if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
		{
			return TEXT("未选择");
		}
		return FString::Printf(
			TEXT("%s %d-%d"),
			*DifficultyLabel(Definition.Difficulty),
			Definition.Chapter,
			((Definition.StageNumber - 1) % 3) + 1);
	};

	FGameXXKTrainingStageDefinition SelectedDefinition;
	if (!FGameXXKTrainingRules::TryGetStageDefinition(SelectedStageId, SelectedDefinition)
		|| SelectedDefinition.Difficulty != ActiveDifficulty
		|| SelectedDefinition.Chapter != ActiveTrainingChapter)
	{
		SelectedStageId = ResolvePreferredTrainingStageForPage(
			ActiveDifficulty,
			ActiveTrainingChapter);
		if (Subsystem)
		{
			Subsystem->SelectTrainingStage(SelectedStageId);
		}
	}

	// One compact difficulty selector replaces the old three-button row.
	UGameXXKDesktopTrainingActionButton* DifficultyDropdown =
		WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
			UGameXXKDesktopTrainingActionButton::StaticClass(),
			TEXT("TrainingDifficultyDropdownButton"));
	DifficultyDropdown->Configure(this, ActionTrainingDifficultyDropdown);
	DifficultyDropdown->SetStyle(MakeTextureButtonStyle(
		CharacterTabNormalTexturePath,
		FVector2D(238.0f, 42.0f),
		FMargin(0.08f)));
	DifficultyDropdown->SetBackgroundColor(FLinearColor::White);
	DifficultyDropdown->SetContent(MakeButtonText(
		WidgetTree,
		FText::FromString(FString::Printf(TEXT("难度：%s  ▼"), *DifficultyLabel(ActiveDifficulty))),
		18,
		Ink));
	AddCanvas(RootCanvas, DifficultyDropdown, FVector2D(1388.0f, 300.0f), FVector2D(238.0f, 42.0f));
	ActionButtons.Add(DifficultyDropdown);

	// Chapters are viewing filters.  They remain visible even when every node in
	// a future chapter is still locked.
	for (int32 ChapterIndex = 0; ChapterIndex < 3; ++ChapterIndex)
	{
		const int32 Chapter = ChapterIndex + 1;
		const bool bSelectedChapter = Chapter == ActiveTrainingChapter;
		UGameXXKDesktopTrainingActionButton* ChapterTab =
			WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
				UGameXXKDesktopTrainingActionButton::StaticClass(),
				*FString::Printf(TEXT("TrainingChapterTab_%d"), ChapterIndex));
		ChapterTab->Configure(this, ActionTrainingChapterFirst + ChapterIndex);
		ChapterTab->SetStyle(MakeTextureButtonStyle(
			bSelectedChapter ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
			FVector2D(76.0f, 36.0f),
			FMargin(0.08f)));
		ChapterTab->SetBackgroundColor(FLinearColor::White);
		ChapterTab->SetContent(MakeButtonText(
			WidgetTree,
			FText::FromString(FString::Printf(TEXT("第%d章"), Chapter)),
			15,
			bSelectedChapter ? Accent : Ink));
		AddCanvas(
			RootCanvas,
			ChapterTab,
			FVector2D(1388.0f + ChapterIndex * 81.0f, 350.0f),
			FVector2D(76.0f, 36.0f));
		ActionButtons.Add(ChapterTab);
	}

	// Preserve the authored vertical route while fitting it above the fixed action shelf.
	const FVector2D NodeSize(64.0f, 64.0f);
	const float NodeX = 1482.0f;
	const float NodeY[3] = {400.0f, 500.0f, 600.0f};
	for (int32 ConnectorIndex = 0; ConnectorIndex < 2; ++ConnectorIndex)
	{
		const float StartY = NodeY[ConnectorIndex] + NodeSize.Y + 6.0f;
		for (int32 DashIndex = 0; DashIndex < 4; ++DashIndex)
		{
			UBorder* Dash = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("TrainingRouteConnector_%d_%d"), ConnectorIndex, DashIndex));
			FSlateBrush DashBrush;
			DashBrush.DrawAs = ESlateBrushDrawType::Box;
			DashBrush.TintColor = FSlateColor(FLinearColor(0.18f, 0.14f, 0.09f, 0.78f));
			Dash->SetBrush(DashBrush);
			Dash->SetBrushColor(FLinearColor(0.18f, 0.14f, 0.09f, 0.78f));
			Dash->SetVisibility(ESlateVisibility::HitTestInvisible);
			AddCanvas(
				RootCanvas,
				Dash,
				FVector2D(NodeX + NodeSize.X * 0.5f - 2.0f, StartY + DashIndex * 7.0f),
				FVector2D(4.0f, 4.0f));
		}
	}

	const int32 FirstStageNumber = (ActiveTrainingChapter - 1) * 3 + 1;
	for (int32 LocalIndex = 0; LocalIndex < 3; ++LocalIndex)
	{
		const int32 StageNumber = FirstStageNumber + LocalIndex;
		const FName StageId = FGameXXKTrainingRules::MakeStageId(ActiveDifficulty, StageNumber);
		FGameXXKTrainingStageDefinition Definition;
		if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
		{
			continue;
		}
		const bool bCurrentTravel = Progress.CurrentTravelStageId == StageId;
		const bool bCleared = FGameXXKTrainingRules::IsStageCleared(Progress, StageId);
		const bool bCanChallenge = FGameXXKTrainingRules::CanChallenge(Progress, StageId);
		const bool bSelected = SelectedStageId == StageId;
		const bool bLocked = !bCleared && !bCanChallenge;
		const TCHAR* StatusIconPath = bLocked
			? TrainingNodeLockedTexturePath
			: bCleared
				? TrainingNodePassedTexturePath
				: TrainingNodeChallengeTexturePath;
		const FLinearColor NodeTint = bCurrentTravel
			? FLinearColor(0.75f, 1.0f, 0.72f, 1.0f)
			: bCleared
				? FLinearColor(0.88f, 1.0f, 0.84f, 1.0f)
				: bCanChallenge
					? FLinearColor(1.0f, 0.88f, 0.78f, 1.0f)
					: FLinearColor(0.58f, 0.58f, 0.56f, 0.78f);
		const FLinearColor StateColor = bLocked
			? FLinearColor(0.28f, 0.28f, 0.28f, 1.0f)
			: bCleared
				? FLinearColor(0.10f, 0.38f, 0.13f, 1.0f)
				: FLinearColor(0.55f, 0.08f, 0.04f, 1.0f);
		const FString StateText = bCurrentTravel
			? TEXT("游历中")
			: bCleared
				? TEXT("通关")
				: bCanChallenge ? TEXT("挑战") : TEXT("锁定");

		if (bSelected)
		{
			UImage* SelectionHalo = WidgetTree->ConstructWidget<UImage>(
				UImage::StaticClass(),
				*FString::Printf(TEXT("TrainingNodeSelection_%d"), StageNumber));
			SelectionHalo->SetBrush(MakeCircularBrush(
				FVector2D(74.0f, 74.0f),
				FLinearColor::Transparent,
				Accent,
				4.0f));
			SelectionHalo->SetColorAndOpacity(FLinearColor::White);
			SelectionHalo->SetVisibility(ESlateVisibility::HitTestInvisible);
			AddCanvas(
				RootCanvas,
				SelectionHalo,
				FVector2D(NodeX - 5.0f, NodeY[LocalIndex] - 5.0f),
				FVector2D(74.0f, 74.0f));
		}

		UGameXXKDesktopTrainingStageButton* Node =
			WidgetTree->ConstructWidget<UGameXXKDesktopTrainingStageButton>(
				UGameXXKDesktopTrainingStageButton::StaticClass(),
				*FString::Printf(TEXT("TrainingNode_%d"), StageNumber));
		Node->Configure(this, StageId);
		Node->SetStyle(MakeCircularButtonStyle(
			NodeSize,
			FLinearColor(0.94f, 0.87f, 0.70f, 0.96f),
			NodeTint));
		Node->SetBackgroundColor(FLinearColor::White);
		Node->SetToolTipText(Subsystem ? Subsystem->BuildTrainingStageTooltip(StageId) : FText::GetEmpty());

		UOverlay* NodeFace = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		USizeBox* StatusIconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		StatusIconSize->SetWidthOverride(46.0f);
		StatusIconSize->SetHeightOverride(46.0f);
		UImage* StatusIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("TrainingNodeStatusIcon_%d"), StageNumber));
		StatusIcon->SetBrush(MakeTextureBrush(StatusIconPath, FVector2D(46.0f, 46.0f)));
		StatusIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		StatusIconSize->AddChild(StatusIcon);
		if (UOverlaySlot* IconSlot = NodeFace->AddChildToOverlay(StatusIconSize))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* StageLabel = MakeButtonText(
			WidgetTree,
			FText::FromString(FString::Printf(
				TEXT("%d-%d"),
				Definition.Chapter,
				((Definition.StageNumber - 1) % 3) + 1)),
			15,
			Ink);
		StageLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
		if (UOverlaySlot* LabelSlot = NodeFace->AddChildToOverlay(StageLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Top);
			LabelSlot->SetPadding(FMargin(4.0f, 3.0f, 4.0f, 0.0f));
		}
		UTextBlock* StatusLabel = MakeButtonText(
			WidgetTree,
			FText::FromString(StateText),
			12,
			StateColor);
		if (UOverlaySlot* StatusSlot = NodeFace->AddChildToOverlay(StatusLabel))
		{
			StatusSlot->SetHorizontalAlignment(HAlign_Fill);
			StatusSlot->SetVerticalAlignment(VAlign_Bottom);
			StatusSlot->SetPadding(FMargin(3.0f, 0.0f, 3.0f, 4.0f));
		}
		Node->SetContent(NodeFace);
		AddCanvas(RootCanvas, Node, FVector2D(NodeX, NodeY[LocalIndex]), NodeSize);
		StageButtons.Add(Node);
	}

	UTextBlock* SelectedStageText = MakeText(
		WidgetTree,
		FText::FromString(FString::Printf(TEXT("已选择：%s"), *StageShortLabel(SelectedStageId))),
		16,
		Ink,
		TEXT("TrainingSelectedStageText"));
	AddCanvas(RootCanvas, SelectedStageText, FVector2D(1388.0f, 674.0f), FVector2D(242.0f, 26.0f));
	TravelStageText = MakeText(
		WidgetTree,
		FText::FromString(FString::Printf(
			TEXT("当前游历：%s"),
			*StageShortLabel(Progress.CurrentTravelStageId))),
		17,
		FLinearColor(0.10f, 0.07f, 0.04f, 1.0f),
		TEXT("TrainingCurrentTravelStageText"));
	AddCanvas(RootCanvas, TravelStageText.Get(), FVector2D(1388.0f, 704.0f), FVector2D(242.0f, 56.0f));

	UGameXXKDesktopTrainingActionButton* Challenge = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(),
		TEXT("TrainingChallengeButton"));
	Challenge->Configure(this, 6);
	Challenge->SetStyle(MakeTextureButtonStyle(CharacterTabSelectedTexturePath, FVector2D(116.0f, 58.0f), FMargin(0.08f)));
	Challenge->SetBackgroundColor(FLinearColor::White);
	Challenge->SetContent(MakeButtonText(WidgetTree, FText::FromString(TEXT("挑战")), 22));
	if (Subsystem)
	{
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

	// Dropdown options are appended last so the expanded list overlays chapter
	// tabs and route nodes without shifting the fixed right-panel layout.
	if (bTrainingDifficultyDropdownOpen)
	{
		UBorder* DropdownPaper = MakeSlotPanel(
			WidgetTree,
			ItemSlotTexturePath,
			PanelAlt,
			FVector2D(246.0f, 132.0f),
			TEXT("TrainingDifficultyDropdownPaper"));
		AddCanvas(RootCanvas, DropdownPaper, FVector2D(1384.0f, 342.0f), FVector2D(246.0f, 132.0f));
		if (UCanvasPanelSlot* PaperSlot = Cast<UCanvasPanelSlot>(DropdownPaper->Slot))
		{
			PaperSlot->SetZOrder(49);
		}
		for (int32 DifficultyIndex = 0; DifficultyIndex < 3; ++DifficultyIndex)
		{
			const EGameXXKTrainingDifficulty Difficulty =
				TrainingDifficultyFromIndex(DifficultyIndex);
			const bool bUnlocked = !Subsystem
				|| FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, Difficulty);
			const bool bSelectedDifficulty = DifficultyIndex == ActiveTrainingDifficultyIndex;
			UGameXXKDesktopTrainingActionButton* Option =
				WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
					UGameXXKDesktopTrainingActionButton::StaticClass(),
					*FString::Printf(TEXT("TrainingDifficultyOption_%d"), DifficultyIndex));
			Option->Configure(this, ActionTrainingDifficultyFirst + DifficultyIndex);
			Option->SetStyle(MakeTextureButtonStyle(
				bSelectedDifficulty ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
				FVector2D(230.0f, 38.0f),
				FMargin(0.08f)));
			Option->SetBackgroundColor(FLinearColor::White);
			Option->SetContent(MakeButtonText(
				WidgetTree,
				FText::FromString(bUnlocked
					? DifficultyLabel(Difficulty)
					: FString::Printf(TEXT("%s · 未解锁"), *DifficultyLabel(Difficulty))),
				16,
				bUnlocked ? (bSelectedDifficulty ? Accent : Ink) : FLinearColor(0.35f, 0.35f, 0.35f, 0.72f)));
			// Locked difficulties remain viewable.  Their node/action permissions
			// are still driven by CanChallenge/CanTravel below.
			Option->SetIsEnabled(true);
			AddCanvas(
				RootCanvas,
				Option,
				FVector2D(1392.0f, 346.0f + DifficultyIndex * 41.0f),
				FVector2D(230.0f, 38.0f));
			if (UCanvasPanelSlot* OptionSlot = Cast<UCanvasPanelSlot>(Option->Slot))
			{
				OptionSlot->SetZOrder(50);
			}
			ActionButtons.Add(Option);
		}
	}
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
	const TSharedPtr<SBox> ExternalContentHost = DesktopOverlayContentHost.Pin();
	TSharedPtr<SWindow> ExternalHostWindow;
	if (!bWasInViewport && FSlateApplication::IsInitialized())
	{
		if (ExternalContentHost.IsValid())
		{
			ExternalHostWindow = FSlateApplication::Get().FindWidgetWindow(
				ExternalContentHost.ToSharedRef());
		}
		else
		{
			const TSharedPtr<SWidget> CachedSlateWidget = GetCachedWidget();
			ExternalHostWindow = CachedSlateWidget.IsValid()
				? FSlateApplication::Get().FindWidgetWindow(CachedSlateWidget.ToSharedRef())
				: nullptr;
		}
	}
	if (bWasInViewport || ExternalHostWindow.IsValid())
	{
		// WidgetTree children are rebuilt for workbench navigation changes. A live
		// UUserWidget otherwise keeps the old Slate tree, so detach and release the
		// cached Slate resource before attaching the new tree.
		if (bWasInViewport)
		{
			RemoveFromParent();
		}
		else
		{
			if (ExternalContentHost.IsValid())
			{
				ExternalContentHost->SetContent(SNullWidget::NullWidget);
			}
			else
			{
				ExternalHostWindow->SetContent(SNullWidget::NullWidget);
			}
		}
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
	else if (ExternalHostWindow.IsValid())
	{
		if (ExternalContentHost.IsValid())
		{
			ExternalContentHost->SetContent(TakeWidget());
		}
		else
		{
			ExternalHostWindow->SetContent(TakeWidget());
		}
		SetVisibility(PreviousVisibility);
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
	if (!CarriedEntry.Payload.Entry.bEquipmentInstance
		&& FGameXXKInventoryItemPresentation::IsInspectable(
			CarriedEntry.Payload.Entry.EntryId))
	{
		SetNotice(FText::FromString(TEXT("任务地图不能放入工具格；仍吸附在鼠标上")));
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
	bDesktopNativeLayoutDirty = true;
	return true;
}

TSet<FGameXXKDesktopInventoryEntryKey>
UGameXXKDesktopTrainingWorkbenchWidget::BuildBatchTransferExclusions() const
{
	TSet<FGameXXKDesktopInventoryEntryKey> Result;
	if (CarriedEntry.IsValid())
	{
		Result.Add(CarriedEntry.Payload.Entry);
	}
	for (const FDesktopToolEntry& ToolEntry : ToolSlots)
	{
		if (ToolEntry.IsValid())
		{
			Result.Add(ToolEntry.Entry);
		}
	}
	return Result;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RequestTutorialMapInspection(
	const FGameXXKDesktopInventoryEntryKey& Entry)
{
	if (!Entry.IsValid()
		|| Entry.bEquipmentInstance
		|| !FGameXXKInventoryItemPresentation::IsInspectable(Entry.EntryId))
	{
		return false;
	}
	if (TutorialMapInspectionRequested.IsBound())
	{
		return TutorialMapInspectionRequested.Execute();
	}
	AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController();
	return PlayerController && PlayerController->OpenTutorialMapInspection();
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
	const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
		Subsystem->GetRuntimeState(),
		EGameXXKDesktopItemContainer::Backpack,
		SlotIndex);
	if (RequestTutorialMapInspection(Entry))
	{
		return true;
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
	bTrainingDifficultyDropdownOpen = false;
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
	bCharacterRosterMembersExpanded = false;
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	ActiveNav = EGameXXKDesktopTrainingNav::None;
	bTrainingDifficultyDropdownOpen = false;
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

void UGameXXKDesktopTrainingWorkbenchWidget::HandleTalentPurchaseCommitted()
{
	RefreshLivePresentation(true);
	if (EmbeddedInventoryWidget)
	{
		EmbeddedInventoryWidget->RefreshTalentCapacityPresentation();
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FGameXXKTalentProjection Projection;
	const bool bToolsUnlocked = Subsystem
		&& FGameXXKTalentRules::BuildProjection(
			Subsystem->GetRuntimeState().Talents,
			Projection)
		&& Projection.bToolsUnlocked;
	if (bToolsUnlocked && WidgetTree)
	{
		if (UWidget* LockedPanel = WidgetTree->FindWidget(TEXT("ToolsTalentLockedPanel")))
		{
			LockedPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateCarriedItemVisualPosition()
{
	if (!CarriedEntry.IsValid() || !CarriedItemImage)
	{
		return;
	}
	FVector2D ReferencePosition;
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
	#if PLATFORM_WINDOWS
		HWND WindowHandle = static_cast<HWND>(DesktopNativeWindowHandle);
		POINT CursorPoint = {};
		if (!WindowHandle
			|| !::IsWindow(WindowHandle)
			|| !::GetCursorPos(&CursorPoint)
			|| !::ScreenToClient(WindowHandle, &CursorPoint))
		{
			return;
		}
		const FVector4 CursorRect =
			GameXXKDesktopTrainingLayout::ResolveDesktopCursorSlateRect(
				FVector2D(
					static_cast<double>(CursorPoint.x),
					static_cast<double>(CursorPoint.y)),
				DesktopOverlayPlacement.Scale,
				DesktopInputDpiScale,
				FVector2D(56.0f, 56.0f));
		if (UCanvasPanelSlot* CarriedCanvasSlot = Cast<UCanvasPanelSlot>(CarriedItemImage->Slot))
		{
			CarriedCanvasSlot->SetPosition(FVector2D(CursorRect.X, CursorRect.Y));
			CarriedCanvasSlot->SetSize(FVector2D(CursorRect.Z, CursorRect.W));
		}
		return;
	#else
		return;
	#endif
	}
	else
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return;
		}
		APlayerController* PlayerController = GetOwningPlayer();
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		if (!PlayerController
			|| !PlayerController->GetMousePosition(MouseX, MouseY)
			|| ViewportSize.X <= 0.0f
			|| ViewportSize.Y <= 0.0f)
		{
			return;
		}
		const GameXXKDesktopTrainingLayout::FFitTransform Fit =
			GameXXKDesktopTrainingLayout::MakeFitTransform(ViewportSize);
		if (Fit.Scale <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		ReferencePosition = (FVector2D(MouseX, MouseY) - Fit.Offset) / Fit.Scale
			- FVector2D(28.0f, 28.0f);
	}
	if (UCanvasPanelSlot* CarriedCanvasSlot = Cast<UCanvasPanelSlot>(CarriedItemImage->Slot))
	{
		CarriedCanvasSlot->SetPosition(ReferencePosition);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::NotifyDesktopNativeMoveCompleted()
{
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	bDesktopNativeMoveSavePending = true;
	bDesktopNativeLayoutDirty = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::NotifyDesktopNativeDisplayMetricsChanged()
{
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	bDesktopResolvedMetricsValid = false;
	bDesktopNativeMoveSavePending = true;
	bDesktopNativeLayoutDirty = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::InitializeDesktopPresentationHostSize(
	const FVector2D& PhysicalWorkAreaSize)
{
	if (PhysicalWorkAreaSize.X <= 1.0f || PhysicalWorkAreaSize.Y <= 1.0f)
	{
		return;
	}
	LoadHudScaleSetting();
	bDesktopFixedHostEnabled =
		PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow;
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::TownViewport)
	{
		DesktopInputDpiScale = 1.0f;
		DesktopWindowPositionNormalized =
			GameXXKDesktopTrainingLayout::ResolvePresentationAnchor(
				false,
				DesktopWindowPositionNormalized);
	}
	DesktopResolvedMetrics = GameXXKDesktopTrainingLayout::ResolveDesktopHudMetrics(
		PhysicalWorkAreaSize,
		HudScalePercent);
	bDesktopResolvedMetricsValid = true;
	DesktopOverlayHostSize = DesktopResolvedMetrics.PhysicalWorkAreaSize;
	UpdateDesktopOverlayPlacement(DesktopOverlayHostSize);
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetPresentationMode(
	const EGameXXKDesktopHudPresentationMode InMode)
{
	if (PresentationMode == InMode)
	{
		UpdateTownPresentationInputLock();
		return;
	}
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		ReleaseDesktopNativeWindow();
	}
	PresentationMode = InMode;
	bDesktopHudDragging = false;
	bDesktopResolvedMetricsValid = false;
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::TownViewport)
	{
		DesktopInputDpiScale = 1.0f;
		DesktopWindowPositionNormalized =
			GameXXKDesktopTrainingLayout::ResolvePresentationAnchor(
				false,
				DesktopWindowPositionNormalized);
	}
	else
	{
		bDesktopWindowPositionLoaded = false;
		LoadDesktopNativeWindowPosition();
	}
	UpdateDesktopOverlayPlacement(DesktopOverlayHostSize);
	UpdateTownPresentationInputLock();
}

FGameXXKDesktopWorkbenchSessionState
UGameXXKDesktopTrainingWorkbenchWidget::CaptureSessionStateForMapTravel()
{
	FGameXXKDesktopWorkbenchSessionState Result;
	if (EmbeddedInventoryWidget)
	{
		Result.EmbeddedInventory = EmbeddedInventoryWidget->CaptureEmbeddedSessionState();
	}
	else if (bHasSavedEmbeddedInventorySession)
	{
		Result.EmbeddedInventory = SavedEmbeddedInventorySession;
	}

	AbortTransientInventoryInteraction(true, false);
	bTrainingDifficultyDropdownOpen = false;
	bNoticeSettingsOpen = false;
	bExitConfirmationOpen = false;
	bDesktopHudDragging = false;

	Result.bValid = ResolveMVPSubsystem() != nullptr;
	Result.bBackpackExpanded = bBackpackExpanded;
	Result.bWarehousePanelOpen = bWarehousePanelOpen;
	Result.WarehousePageIndex = GetWarehousePageIndexForTest();
	Result.ActiveNav = ActiveNav;
	Result.ActiveCenterPage = ActiveCenterPage;
	Result.RightPanel = RightPanel;
	Result.ActiveToolMode = ActiveToolMode;
	Result.ActiveToolCombineKind = ActiveToolCombineKind;
	Result.SelectedToolSocketIndex = SelectedToolSocketIndex;
	Result.ActiveCharacterRoster = ActiveCharacterRoster;
	Result.ActiveFormationRoster = ActiveFormationRoster;
	Result.SelectedStageId = SelectedStageId;
	Result.ActiveTrainingDifficultyIndex = ActiveTrainingDifficultyIndex;
	Result.ActiveTrainingChapter = ActiveTrainingChapter;
	Result.ActiveBackpackCharacterId = ActiveBackpackCharacterId;
	Result.LastCompanionBackpackCharacterId = LastCompanionBackpackCharacterId;
	Result.LastNpcBackpackCharacterId = LastNpcBackpackCharacterId;
	Result.FormationCandidateCharacterId = FormationCandidateCharacterId;
	Result.bCharacterRosterMembersExpanded = bCharacterRosterMembersExpanded;
	Result.bSettingsPanelOpen = bSettingsPanelOpen;
	return Result;
}

void UGameXXKDesktopTrainingWorkbenchWidget::RestoreSessionStateAfterMapTravel(
	const FGameXXKDesktopWorkbenchSessionState& State)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!State.bValid || !Subsystem)
	{
		return;
	}
	AbortTransientInventoryInteraction(true, false);
	bTownMapTravelPending = false;
	bBackpackExpanded = State.bBackpackExpanded;
	bWarehousePanelOpen = bBackpackExpanded && State.bWarehousePanelOpen;
	WarehousePageIndex = FMath::Clamp(
		State.WarehousePageIndex,
		0,
		FMath::Max(0, GetWarehousePageCountForTest() - 1));
	ActiveNav = bBackpackExpanded ? State.ActiveNav : EGameXXKDesktopTrainingNav::None;
	ActiveCenterPage = State.ActiveCenterPage;
	RightPanel = bBackpackExpanded
		? State.RightPanel
		: EGameXXKDesktopTrainingRightPanel::None;
	ActiveToolMode = State.ActiveToolMode;
	ActiveToolCombineKind = State.ActiveToolCombineKind;
	SelectedToolSocketIndex = FMath::Clamp(State.SelectedToolSocketIndex, 0, 5);
	ActiveCharacterRoster = State.ActiveCharacterRoster;
	ActiveFormationRoster = State.ActiveFormationRoster;
	bCharacterRosterMembersExpanded = State.bCharacterRosterMembersExpanded;
	bSettingsPanelOpen = bBackpackExpanded && State.bSettingsPanelOpen;

	const TArray<FName> BackpackCharacters = GetBackpackCharacterIdsForTest();
	ActiveBackpackCharacterId = BackpackCharacters.Contains(State.ActiveBackpackCharacterId)
		? State.ActiveBackpackCharacterId
		: (BackpackCharacters.IsEmpty()
			? FGameXXKEquipmentRules::HeroCharacterId()
			: BackpackCharacters[0]);
	const TArray<FName> CompanionCharacters = GetCompanionCharacterIdsForTest();
	LastCompanionBackpackCharacterId =
		CompanionCharacters.Contains(State.LastCompanionBackpackCharacterId)
			? State.LastCompanionBackpackCharacterId
			: (CompanionCharacters.IsEmpty() ? NAME_None : CompanionCharacters[0]);
	const TArray<FName> NpcCharacters = GetNpcCharacterIdsForTest();
	LastNpcBackpackCharacterId = NpcCharacters.Contains(State.LastNpcBackpackCharacterId)
		? State.LastNpcBackpackCharacterId
		: (NpcCharacters.IsEmpty() ? NAME_None : NpcCharacters[0]);
	FormationCandidateCharacterId =
		(CompanionCharacters.Contains(State.FormationCandidateCharacterId)
			|| NpcCharacters.Contains(State.FormationCandidateCharacterId))
			? State.FormationCandidateCharacterId
			: NAME_None;
	EnsureFormationCandidate();

	FGameXXKTrainingStageDefinition StageDefinition;
	if (FGameXXKTrainingRules::TryGetStageDefinition(
			State.SelectedStageId,
			StageDefinition))
	{
		SelectedStageId = State.SelectedStageId;
		Subsystem->SelectTrainingStage(SelectedStageId);
		SynchronizeTrainingPageFromStage(SelectedStageId);
	}
	else
	{
		ActiveTrainingDifficultyIndex = FMath::Clamp(
			State.ActiveTrainingDifficultyIndex,
			0,
			2);
		ActiveTrainingChapter = FMath::Clamp(State.ActiveTrainingChapter, 1, 3);
		SelectedStageId = ResolvePreferredTrainingStageForPage(
			TrainingDifficultyFromIndex(ActiveTrainingDifficultyIndex),
			ActiveTrainingChapter);
		Subsystem->SelectTrainingStage(SelectedStageId);
	}

	SavedEmbeddedInventorySession = State.EmbeddedInventory;
	SavedEmbeddedInventorySession.CharacterId = ActiveBackpackCharacterId;
	bHasSavedEmbeddedInventorySession = true;
	bTrainingDifficultyDropdownOpen = false;
	bNoticeSettingsOpen = false;
	bExitConfirmationOpen = false;
	bDesktopHudDragging = false;
	bDesktopResolvedMetricsValid = false;
	bDesktopNativeLayoutDirty = true;
	RefreshLayout();
	UpdateTownPresentationInputLock();
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetTownMapTravelPending(
	const bool bPending)
{
	bTownMapTravelPending = bPending;
	if (TownToggleButton)
	{
		TownToggleButton->SetIsEnabled(!bPending);
	}
	if (StoryQuestButton)
	{
		StoryQuestButton->SetIsEnabled(!bPending);
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RequestTownToggle()
{
	if (bTownMapTravelPending)
	{
		return false;
	}
	AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController();
	return PlayerController
		&& PlayerController->RequestDesktopTownToggleFromWorkbench();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RequestStoryCarriage()
{
	if (bTownMapTravelPending)
	{
		return false;
	}
	if (StoryCarriageRequested.IsBound())
	{
		return StoryCarriageRequested.Execute();
	}
	AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController();
	return PlayerController
		&& PlayerController->RequestDesktopStoryCarriageFromWorkbench();
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateTownPresentationInputLock()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->SetDesktopWorkbenchTownPanelInputLock(
			PresentationMode == EGameXXKDesktopHudPresentationMode::TownViewport
			&& bBackpackExpanded
			&& GetVisibility() != ESlateVisibility::Collapsed
			&& GetVisibility() != ESlateVisibility::Hidden);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::LoadDesktopNativeWindowPosition()
{
	if (bDesktopWindowPositionLoaded)
	{
		return;
	}
	float PositionX = 0.5f;
	float PositionY = 0.08f;
	if (GConfig)
	{
		GConfig->GetFloat(
			HudSettingsSection,
			DesktopWindowPositionXKey,
			PositionX,
			GGameUserSettingsIni);
		GConfig->GetFloat(
			HudSettingsSection,
			DesktopWindowPositionYKey,
			PositionY,
			GGameUserSettingsIni);
	}
	DesktopWindowPositionNormalized = FVector2D(
		FMath::Clamp(PositionX, 0.0f, 1.0f),
		FMath::Clamp(PositionY, 0.0f, 1.0f));
	bDesktopWindowPositionLoaded = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::SaveDesktopNativeWindowPosition()
{
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	if (GConfig)
	{
		GConfig->SetFloat(
			HudSettingsSection,
			DesktopWindowPositionXKey,
			DesktopWindowPositionNormalized.X,
			GGameUserSettingsIni);
		GConfig->SetFloat(
			HudSettingsSection,
			DesktopWindowPositionYKey,
			DesktopWindowPositionNormalized.Y,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateDesktopOverlayPlacement(
	const FVector2D& HostSize)
{
	if (HostSize.X <= 1.0f || HostSize.Y <= 1.0f)
	{
		return;
	}
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		LoadDesktopNativeWindowPosition();
	}
	else
	{
		DesktopWindowPositionNormalized =
			GameXXKDesktopTrainingLayout::ResolvePresentationAnchor(
				false,
				DesktopWindowPositionNormalized);
	}
	const FVector2D PreviousHudSize = DesktopOverlayPlacement.HudSize;
	const FVector2D PreviousFixedHostSize = DesktopFixedHostPlacement.HudSize;
	const FVector2D PreviousFixedHostTopLeft = DesktopFixedHostPlacement.HudTopLeft;
	const FVector2D PreviousFixedContentOffset = DesktopFixedContentOffset;
	const FVector2D PreviousBodyOffset = DesktopOverlayPlacement.BodyOffset;
	DesktopOverlayHostSize = HostSize;
	const GameXXKDesktopTrainingLayout::FDesktopHudResolvedMetrics Metrics =
		bDesktopResolvedMetricsValid
			? DesktopResolvedMetrics
			: GameXXKDesktopTrainingLayout::ResolveDesktopHudMetrics(
				DesktopOverlayHostSize,
				HudScalePercent);
	DesktopOverlayPlacement = GameXXKDesktopTrainingLayout::ComputeDesktopOverlayPlacementAtScale(
		Metrics,
		DesktopWindowPositionNormalized,
		bBackpackExpanded,
		bExpandUpward,
		GetNoticePanelLogicalHeight(),
		bWarehousePanelOpen);
	if (bBackpackExpanded)
	{
		DesktopOverlayPlacement.BodyOffset = bDesktopHudDragging
			? PreviousBodyOffset
			: GameXXKDesktopTrainingLayout::ResolveExpandedBodyFitOffset(
				Metrics.PhysicalWorkAreaSize,
				DesktopOverlayPlacement.HudTopLeft,
				DesktopOverlayPlacement.ContentOffset,
				DesktopOverlayPlacement.Scale,
				bWarehousePanelOpen,
				RightPanel != EGameXXKDesktopTrainingRightPanel::None,
				bExpandUpward);
	}
	if (!bBackpackExpanded && bIdleStripFolded)
	{
		const FVector2D FoldedLogicalSize = GetCurrentDesignCanvasSize();
		DesktopOverlayPlacement.HudSize = FoldedLogicalSize * DesktopOverlayPlacement.Scale;
		const FVector2D MaximumHudTopLeft(
			FMath::Max(0.0f, DesktopOverlayPlacement.HostSize.X - DesktopOverlayPlacement.HudSize.X),
			FMath::Max(0.0f, DesktopOverlayPlacement.HostSize.Y - DesktopOverlayPlacement.HudSize.Y));
		DesktopOverlayPlacement.HudTopLeft = FVector2D(
			FMath::Clamp(DesktopOverlayPlacement.HudTopLeft.X, 0.0f, MaximumHudTopLeft.X),
			FMath::Clamp(DesktopOverlayPlacement.HudTopLeft.Y, 0.0f, MaximumHudTopLeft.Y));
		DesktopOverlayPlacement.StripTopLeft = DesktopOverlayPlacement.HudTopLeft;
		DesktopOverlayPlacement.StripSize = FVector2D(
			IdleSummaryTabX + IdleSummaryTabWidth,
			NoticeLineHeight) * DesktopOverlayPlacement.Scale;
	}
	if (bDesktopFixedHostEnabled
		&& PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		DesktopFixedHostPlacement =
			GameXXKDesktopTrainingLayout::ResolveDesktopWorkAreaHostPlacement(Metrics);
		DesktopFixedContentOffset = DesktopOverlayPlacement.HudTopLeft;
	}
	else
	{
		DesktopFixedHostPlacement = DesktopOverlayPlacement;
		DesktopFixedContentOffset = FVector2D::ZeroVector;
	}
	if (!DesktopOverlayPlacement.HudSize.Equals(PreviousHudSize, 0.5f))
	{
		bDesktopNativeLayoutDirty = true;
		bDesktopNativeInputRegionDirty = true;
	}
	if (!DesktopFixedHostPlacement.HudSize.Equals(PreviousFixedHostSize, 0.5f)
		|| !DesktopFixedHostPlacement.HudTopLeft.Equals(PreviousFixedHostTopLeft, 0.5f))
	{
		bDesktopNativeLayoutDirty = true;
		bDesktopNativeInputRegionDirty = true;
	}
	if (!DesktopFixedContentOffset.Equals(PreviousFixedContentOffset, 0.5f))
	{
		bDesktopNativeInputRegionDirty = true;
	}
	if (!DesktopOverlayPlacement.BodyOffset.Equals(PreviousBodyOffset, 0.01f))
	{
		bDesktopNativeInputRegionDirty = true;
	}
	if (DesktopHudCanvasSlot)
	{
		const bool bDesktopWindow =
			PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow;
		const GameXXKDesktopTrainingLayout::FDesktopSlateHostGeometry HostGeometry =
			GameXXKDesktopTrainingLayout::ResolveDesktopSlateHostGeometry(
				DesktopOverlayPlacement,
				DesktopFixedContentOffset,
				bDesktopWindow,
				DesktopInputDpiScale);
		DesktopHudCanvasSlot->SetPosition(HostGeometry.Position);
		DesktopHudCanvasSlot->SetSize(HostGeometry.Size);
	}
	if (DesktopCursorCanvasSlot)
	{
		DesktopCursorCanvasSlot->SetPosition(FVector2D::ZeroVector);
		DesktopCursorCanvasSlot->SetSize(
			GameXXKDesktopTrainingLayout::PhysicalPixelsToSlateHost(
				DesktopFixedHostPlacement.HudSize,
				PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow
					? DesktopInputDpiScale
					: 1.0f));
	}
}

bool UGameXXKDesktopTrainingWorkbenchWidget::TryGetDesktopHudPointerScreenPosition(
	FVector2D& OutPointerScreen) const
{
#if PLATFORM_WINDOWS
	POINT CursorPoint = {};
	if (::GetCursorPos(&CursorPoint))
	{
		OutPointerScreen = FVector2D(
			static_cast<float>(CursorPoint.x),
			static_cast<float>(CursorPoint.y));
		return true;
	}
#endif
	OutPointerScreen = FVector2D::ZeroVector;
	return false;
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateDesktopOverlayAnchorFromPointer()
{
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	const FVector2D HostSize = DesktopOverlayHostSize;
	if (HostSize.X <= 1.0f || HostSize.Y <= 1.0f)
	{
		return;
	}
	FVector2D CurrentPointerScreen;
	if (!TryGetDesktopHudPointerScreenPosition(CurrentPointerScreen))
	{
		return;
	}
	const float Scale = bDesktopResolvedMetricsValid
		? DesktopResolvedMetrics.Scale
		: GameXXKDesktopTrainingLayout::ResolveManualHudScale(HudScalePercent);
	const FVector2D CollapsedStripSize =
		GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize() * Scale;
	DesktopWindowPositionNormalized =
		GameXXKDesktopTrainingLayout::ResolveDesktopHudDragAnchor(
			DesktopHudDragStartNormalizedAnchor,
			DesktopHudDragStartPointerScreen,
			CurrentPointerScreen,
			HostSize,
			CollapsedStripSize);
	UpdateDesktopOverlayPlacement(HostSize);
	bDesktopNativeLayoutDirty = true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateExpansionDirectionFromNativeWindow()
{
	if (PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		LoadDesktopNativeWindowPosition();
	}
	else
	{
		DesktopWindowPositionNormalized =
			GameXXKDesktopTrainingLayout::ResolvePresentationAnchor(
				false,
				DesktopWindowPositionNormalized);
	}
	const FVector2D HostSize(
		FMath::Max(1.0f, DesktopOverlayHostSize.X),
		FMath::Max(1.0f, DesktopOverlayHostSize.Y));
	const GameXXKDesktopTrainingLayout::FDesktopHudResolvedMetrics Metrics =
		bDesktopResolvedMetricsValid
			? DesktopResolvedMetrics
			: GameXXKDesktopTrainingLayout::ResolveDesktopHudMetrics(
				HostSize,
				HudScalePercent);
	const GameXXKDesktopTrainingLayout::FDesktopOverlayPlacement CollapsedPlacement =
		GameXXKDesktopTrainingLayout::ComputeDesktopOverlayPlacementAtScale(
			Metrics,
			DesktopWindowPositionNormalized,
			false,
			false,
			GetNoticePanelLogicalHeight());
	const FVector4 WorkArea(0.0f, 0.0f, HostSize.X, HostSize.Y);
	const FVector4 StripRect(
		CollapsedPlacement.StripTopLeft.X,
		CollapsedPlacement.StripTopLeft.Y,
		CollapsedPlacement.StripSize.X,
		CollapsedPlacement.StripSize.Y);
	bExpandUpward = GameXXKDesktopTrainingLayout::ChooseVerticalExpansionDirection(
		WorkArea,
		StripRect,
		GameXXKDesktopTrainingLayout::GetReferenceCanvasSize().Y * CollapsedPlacement.Scale)
		== GameXXKDesktopTrainingLayout::EGameXXKDesktopVerticalExpansionDirection::Up;
	UpdateDesktopOverlayPlacement(HostSize);
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetDesktopOverlayContentHost(
	const TSharedPtr<SBox>& InContentHost)
{
	DesktopOverlayContentHost = InContentHost;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AttachDesktopNativeWindowForPresentation(
	void* NativeWindowHandle)
{
#if PLATFORM_WINDOWS
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return false;
	}
	HWND WindowHandle = static_cast<HWND>(NativeWindowHandle);
	if (!WindowHandle || !::IsWindow(WindowHandle))
	{
		return false;
	}
	if (bDesktopNativeHookInstalled)
	{
		const bool bSameWindow = DesktopNativeWindowHandle == NativeWindowHandle;
		if (bSameWindow && FSlateApplication::IsInitialized())
		{
			const TSharedPtr<SBox> ContentHost = DesktopOverlayContentHost.Pin();
			if (ContentHost.IsValid())
			{
				DesktopNativeSlateWindow = FSlateApplication::Get().FindWidgetWindow(
					ContentHost.ToSharedRef());
			}
		}
		return bSameWindow;
	}
	WNDPROC PreviousWindowProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
		WindowHandle,
		GWLP_WNDPROC,
		reinterpret_cast<LONG_PTR>(&GameXXKDesktopWindowProc)));
	if (!PreviousWindowProc)
	{
		return false;
	}
	FGameXXKDesktopWindowHook Hook;
	Hook.PreviousWindowProc = PreviousWindowProc;
	Hook.Owner = this;
	GetDesktopWindowHooks().Add(WindowHandle, Hook);
	DesktopNativeWindowHandle = WindowHandle;
	DesktopPreviousWindowProc = reinterpret_cast<void*>(PreviousWindowProc);
	bDesktopNativeHookInstalled = true;
	bDesktopNativeInputRegionDirty = true;
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SBox> ContentHost = DesktopOverlayContentHost.Pin();
		if (ContentHost.IsValid())
		{
			DesktopNativeSlateWindow = FSlateApplication::Get().FindWidgetWindow(
				ContentHost.ToSharedRef());
		}
	}
	bDesktopNativeLayoutDirty = true;
	bDesktopNativeLastExpanded = bBackpackExpanded;
	DesktopNativeLastHudScalePercent = INDEX_NONE;
	ApplyDesktopNativeWindowLayout(true);
	RefreshDesktopNativeMousePassthrough();
	return true;
#else
	(void)NativeWindowHandle;
	return false;
#endif
}

void UGameXXKDesktopTrainingWorkbenchWidget::DetachDesktopNativeWindowForPresentation()
{
	ReleaseDesktopNativeWindow();
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetDesktopNativeMousePassthrough(
	const bool bEnabled)
{
	bDesktopNativeMousePassthrough = bEnabled;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ShouldDesktopNativeClientPointPassThrough(
	const FVector2D ClientPoint) const
{
	GameXXKDesktopTrainingLayout::FDesktopOverlayMouseState MouseState;
	MouseState.bCarryingItem = CarriedEntry.IsValid();
	MouseState.bHudDragging = bDesktopHudDragging;
	if (FSlateApplication::IsInitialized())
	{
		MouseState.bMouseCaptured = FSlateApplication::Get().HasAnyMouseCaptor();
		MouseState.bDragDropActive = FSlateApplication::Get().IsDragDropping();
	}

	GameXXKDesktopTrainingLayout::FDesktopNativeRegionState SurfaceState;
	SurfaceState.bExpanded = bBackpackExpanded;
	SurfaceState.bIdleStripFolded = bIdleStripFolded;
	SurfaceState.bExpandUpward = bExpandUpward;
	SurfaceState.bWarehouseOpen = bWarehousePanelOpen;
	SurfaceState.bRightPanelOpen =
		RightPanel != EGameXXKDesktopTrainingRightPanel::None;
	SurfaceState.bExitConfirmationOpen = bExitConfirmationOpen;
	SurfaceState.NoticeHeight = GetNoticePanelLogicalHeight();
	SurfaceState.Scale = DesktopOverlayPlacement.Scale;
	SurfaceState.ContentOffset = DesktopOverlayPlacement.ContentOffset;
	SurfaceState.BodyOffset = DesktopOverlayPlacement.BodyOffset;
	SurfaceState.bTownToggleVisible =
		bBackpackExpanded && DesktopOverlayPlacement.TownToggleRect.Z > 0.0f;
	SurfaceState.TownToggleRect = DesktopOverlayPlacement.TownToggleRect;
	SurfaceState.bStoryQuestVisible =
		bBackpackExpanded && DesktopOverlayPlacement.StoryQuestRect.Z > 0.0f;
	SurfaceState.StoryQuestRect = DesktopOverlayPlacement.StoryQuestRect;
	const TArray<GameXXKDesktopTrainingLayout::FDesktopNativeRegionShape> Surfaces =
		GameXXKDesktopTrainingLayout::BuildDesktopNativeRegionShapes(SurfaceState);
	MouseState.bPointerOverInteractiveSurface =
		GameXXKDesktopTrainingLayout::IsPointInsideDesktopNativeRegionShapes(
			Surfaces,
			ClientPoint - (bDesktopFixedHostEnabled
				? DesktopFixedContentOffset
				: FVector2D::ZeroVector));
	return GameXXKDesktopTrainingLayout::ShouldDesktopOverlayPassMouseThrough(MouseState);
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyDesktopNativeInputRegion()
{
#if PLATFORM_WINDOWS
	HWND WindowHandle = static_cast<HWND>(DesktopNativeWindowHandle);
	if (!WindowHandle || !::IsWindow(WindowHandle))
	{
		return;
	}
	bool bRequireFullRegion = CarriedEntry.IsValid()
		|| bDesktopHudDragging
		|| bExitConfirmationOpen;
	if (FSlateApplication::IsInitialized())
	{
		bRequireFullRegion |= FSlateApplication::Get().HasAnyMouseCaptor()
			|| FSlateApplication::Get().IsDragDropping();
	}
	if (bRequireFullRegion != bDesktopNativeInputRegionWasFull)
	{
		bDesktopNativeInputRegionDirty = true;
	}
	if (!bDesktopNativeInputRegionDirty)
	{
		return;
	}

	RECT ClientRect = {};
	if (!::GetClientRect(WindowHandle, &ClientRect))
	{
		return;
	}
	HRGN CombinedRegion = ::CreateRectRgn(0, 0, 0, 0);
	if (!CombinedRegion)
	{
		return;
	}
	if (bRequireFullRegion)
	{
		::SetRectRgn(
			CombinedRegion,
			0,
			0,
			ClientRect.right - ClientRect.left,
			ClientRect.bottom - ClientRect.top);
	}
	else
	{
		GameXXKDesktopTrainingLayout::FDesktopNativeRegionState SurfaceState;
		SurfaceState.bExpanded = bBackpackExpanded;
		SurfaceState.bIdleStripFolded = bIdleStripFolded;
		SurfaceState.bExpandUpward = bExpandUpward;
		SurfaceState.bWarehouseOpen = bWarehousePanelOpen;
		SurfaceState.bRightPanelOpen =
			RightPanel != EGameXXKDesktopTrainingRightPanel::None;
		SurfaceState.bExitConfirmationOpen = bExitConfirmationOpen;
		SurfaceState.NoticeHeight = GetNoticePanelLogicalHeight();
		SurfaceState.Scale = DesktopOverlayPlacement.Scale;
		SurfaceState.ContentOffset = DesktopOverlayPlacement.ContentOffset;
		SurfaceState.BodyOffset = DesktopOverlayPlacement.BodyOffset;
		SurfaceState.bTownToggleVisible =
			bBackpackExpanded && DesktopOverlayPlacement.TownToggleRect.Z > 0.0f;
		SurfaceState.TownToggleRect = DesktopOverlayPlacement.TownToggleRect;
		SurfaceState.bStoryQuestVisible =
			bBackpackExpanded && DesktopOverlayPlacement.StoryQuestRect.Z > 0.0f;
		SurfaceState.StoryQuestRect = DesktopOverlayPlacement.StoryQuestRect;
		const TArray<GameXXKDesktopTrainingLayout::FDesktopNativeRegionShape> Surfaces =
			GameXXKDesktopTrainingLayout::BuildDesktopNativeRegionShapes(SurfaceState);
		const FVector2D HostOffset = bDesktopFixedHostEnabled
			? DesktopFixedContentOffset
			: FVector2D::ZeroVector;
		constexpr float RegionPadding = 3.0f;
		for (const GameXXKDesktopTrainingLayout::FDesktopNativeRegionShape& Surface : Surfaces)
		{
			const FVector4 Rect =
				GameXXKDesktopTrainingLayout::ResolveDesktopNativeRegionRect(
					Surface,
					HostOffset,
					RegionPadding);
			const int32 Left = FMath::FloorToInt(Rect.X);
			const int32 Top = FMath::FloorToInt(Rect.Y);
			const int32 Right = FMath::CeilToInt(Rect.X + Rect.Z);
			const int32 Bottom = FMath::CeilToInt(Rect.Y + Rect.W);
			HRGN ShapeRegion = Surface.Type
				== GameXXKDesktopTrainingLayout::EDesktopNativeRegionShapeType::Ellipse
				? ::CreateEllipticRgn(Left, Top, Right, Bottom)
				: ::CreateRectRgn(Left, Top, Right, Bottom);
			if (ShapeRegion)
			{
				::CombineRgn(CombinedRegion, CombinedRegion, ShapeRegion, RGN_OR);
				::DeleteObject(ShapeRegion);
			}
		}
	}

	const LONG_PTR CurrentStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
	if ((CurrentStyle & static_cast<LONG_PTR>(WS_EX_TRANSPARENT)) != 0)
	{
		::SetWindowLongPtrW(
			WindowHandle,
			GWL_EXSTYLE,
			CurrentStyle & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT));
	}
	if (::SetWindowRgn(WindowHandle, CombinedRegion, false) == 0)
	{
		::DeleteObject(CombinedRegion);
		return;
	}
	bDesktopNativeInputRegionWasFull = bRequireFullRegion;
	bDesktopNativeInputRegionDirty = false;
#endif
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshDesktopNativeMousePassthrough()
{
#if PLATFORM_WINDOWS
	if (!bDesktopNativeHookInstalled
		|| PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	HWND WindowHandle = static_cast<HWND>(DesktopNativeWindowHandle);
	POINT CursorPoint = {};
	RECT ClientRect = {};
	bool bShouldPassMouseThrough = true;
	if (WindowHandle
		&& ::IsWindow(WindowHandle)
		&& ::GetCursorPos(&CursorPoint)
		&& ::ScreenToClient(WindowHandle, &CursorPoint)
		&& ::GetClientRect(WindowHandle, &ClientRect)
		&& CursorPoint.x >= ClientRect.left
		&& CursorPoint.y >= ClientRect.top
		&& CursorPoint.x < ClientRect.right
		&& CursorPoint.y < ClientRect.bottom)
	{
		bShouldPassMouseThrough = ShouldDesktopNativeClientPointPassThrough(
			FVector2D(
				static_cast<double>(CursorPoint.x),
				static_cast<double>(CursorPoint.y)));
	}
	SetDesktopNativeMousePassthrough(bShouldPassMouseThrough);
#endif
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyDesktopNativeWindowLayout(const bool bForce)
{
#if PLATFORM_WINDOWS
	HWND WindowHandle = static_cast<HWND>(DesktopNativeWindowHandle);
	if (!WindowHandle || !::IsWindow(WindowHandle))
	{
		return;
	}
	if (!bForce && !bDesktopNativeLayoutDirty)
	{
		return;
	}
	MONITORINFO MonitorInfo = {};
	MonitorInfo.cbSize = sizeof(MONITORINFO);
	if (!::GetMonitorInfo(
			::MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST),
			&MonitorInfo))
	{
		return;
	}
	const int32 DesiredWidth = FMath::Max(1, MonitorInfo.rcWork.right - MonitorInfo.rcWork.left);
	const int32 DesiredHeight = FMath::Max(1, MonitorInfo.rcWork.bottom - MonitorInfo.rcWork.top);
	DesktopWorkAreaOrigin = FIntPoint(MonitorInfo.rcWork.left, MonitorInfo.rcWork.top);
	DesktopInputDpiScale = FMath::Max(1.0f, GetDesktopMonitorDpiScale(WindowHandle));
	const FVector2D PhysicalWorkAreaSize(
		static_cast<float>(DesiredWidth),
		static_cast<float>(DesiredHeight));
	if (!bDesktopResolvedMetricsValid
		|| !DesktopResolvedMetrics.PhysicalWorkAreaSize.Equals(PhysicalWorkAreaSize, 0.5f)
		|| DesktopNativeLastHudScalePercent != HudScalePercent)
	{
		InitializeDesktopPresentationHostSize(PhysicalWorkAreaSize);
	}
	else
	{
		UpdateDesktopOverlayPlacement(PhysicalWorkAreaSize);
	}
	const GameXXKDesktopTrainingLayout::FDesktopOverlayPlacement& WindowPlacement =
		bDesktopFixedHostEnabled
			? DesktopFixedHostPlacement
			: DesktopOverlayPlacement;
	const int32 WindowLeft =
		MonitorInfo.rcWork.left + FMath::RoundToInt(WindowPlacement.HudTopLeft.X);
	const int32 WindowTop =
		MonitorInfo.rcWork.top + FMath::RoundToInt(WindowPlacement.HudTopLeft.Y);
	const int32 WindowWidth =
		FMath::Max(1, FMath::RoundToInt(WindowPlacement.HudSize.X));
	const int32 WindowHeight =
		FMath::Max(1, FMath::RoundToInt(WindowPlacement.HudSize.Y));

	TSharedPtr<SWindow> SlateWindow = DesktopNativeSlateWindow.Pin();
	if (FSlateApplication::IsInitialized())
	{
		if (!SlateWindow.IsValid())
		{
			const TSharedPtr<SBox> ContentHost = DesktopOverlayContentHost.Pin();
			if (ContentHost.IsValid())
			{
				SlateWindow = FSlateApplication::Get().FindWidgetWindow(
					ContentHost.ToSharedRef());
				DesktopNativeSlateWindow = SlateWindow;
			}
		}
	}
	const TSharedPtr<FGenericWindow> SlateNativeWindow =
		SlateWindow.IsValid() ? SlateWindow->GetNativeWindow() : nullptr;
	const bool bSlateWindowMatchesNativeHandle = SlateNativeWindow.IsValid()
		&& SlateNativeWindow->GetOSWindowHandle() == DesktopNativeWindowHandle;
	if (bSlateWindowMatchesNativeHandle)
	{
		// Keep Slate's cached geometry and the RHI viewport in the same resize
		// transaction as the HWND. Direct SetWindowPos sizing leaves the old
		// swap-chain extent active and exposes opaque system pixels after Tab.
		SlateWindow->ReshapeWindow(
			FVector2D(WindowLeft, WindowTop),
			FVector2D(WindowWidth, WindowHeight));
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->RequestResize(
				SlateWindow,
				static_cast<uint32>(WindowWidth),
				static_cast<uint32>(WindowHeight));
		}
	}
	else
	{
		// Defensive fallback for a detached/rebuilding Slate tree. Normal desktop
		// presentation always resolves the owning SWindow above.
		::SetWindowPos(
			WindowHandle,
			nullptr,
			WindowLeft,
			WindowTop,
			WindowWidth,
			WindowHeight,
			SWP_NOACTIVATE | SWP_NOZORDER);
	}
	::SetWindowPos(
		WindowHandle,
		bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
		0,
		0,
		0,
		0,
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	bDesktopNativeLastExpanded = bBackpackExpanded;
	DesktopNativeLastHudScalePercent = HudScalePercent;
	bDesktopNativeLayoutDirty = false;
#endif
}

void UGameXXKDesktopTrainingWorkbenchWidget::TickDesktopNativeWindow()
{
#if PLATFORM_WINDOWS
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow
		|| GetVisibility() == ESlateVisibility::Collapsed
		|| GetVisibility() == ESlateVisibility::Hidden
		|| !FSlateApplication::IsInitialized())
	{
		return;
	}
	if (!bDesktopNativeHookInstalled)
	{
		return;
	}
	ApplyDesktopNativeInputRegion();
	if (bDesktopNativeMoveSavePending)
	{
		bDesktopNativeMoveSavePending = false;
		SaveDesktopNativeWindowPosition();
	}
	ApplyDesktopNativeWindowLayout(false);
	RefreshDesktopNativeMousePassthrough();
#endif
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReleaseDesktopNativeWindow()
{
#if PLATFORM_WINDOWS
	HWND WindowHandle = static_cast<HWND>(DesktopNativeWindowHandle);
	if (WindowHandle && ::IsWindow(WindowHandle))
	{
		::SetWindowRgn(WindowHandle, nullptr, false);
		SetDesktopNativeMousePassthrough(false);
		if (FGameXXKDesktopWindowHook* Hook = GetDesktopWindowHooks().Find(WindowHandle))
		{
			if (Hook->PreviousWindowProc)
			{
				::SetWindowLongPtr(
					WindowHandle,
					GWLP_WNDPROC,
					reinterpret_cast<LONG_PTR>(Hook->PreviousWindowProc));
			}
			GetDesktopWindowHooks().Remove(WindowHandle);
		}
	}
	DesktopNativeWindowHandle = nullptr;
	DesktopPreviousWindowProc = nullptr;
	bDesktopNativeHookInstalled = false;
	DesktopNativeSlateWindow.Reset();
	bDesktopNativeMousePassthrough = false;
	bDesktopNativeInputRegionDirty = true;
	bDesktopNativeInputRegionWasFull = false;
	DesktopInputDpiScale = 1.0f;
	bDesktopNativeLayoutDirty = true;
	DesktopNativeLastHudScalePercent = INDEX_NONE;
#endif
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleAlwaysOnTop()
{
	bAlwaysOnTop = !bAlwaysOnTop;
	bDesktopNativeLayoutDirty = true;
#if PLATFORM_WINDOWS
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> CachedSlateWidget = GetCachedWidget();
		const TSharedPtr<SWindow> Window = CachedSlateWidget.IsValid()
			? FSlateApplication::Get().FindWidgetWindow(CachedSlateWidget.ToSharedRef())
			: nullptr;
		const TSharedPtr<FGenericWindow> NativeWindow =
			Window.IsValid() && Window->GetTitle().ToString() == TEXT("GameXXKDesktopOverlay")
				? Window->GetNativeWindow()
				: nullptr;
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

void UGameXXKDesktopTrainingWorkbenchWidget::LoadHudScaleSetting()
{
	if (bHudScaleSettingLoaded)
	{
		return;
	}
	int32 SavedPercent = 100;
	bool bLoadedScale = ReadDesktopHudStableScale(SavedPercent);
	if (GConfig)
	{
		if (!bLoadedScale)
		{
			bLoadedScale = GConfig->GetInt(
				HudSettingsSection,
				HudScaleConfigKey,
				SavedPercent,
				GGameUserSettingsIni);
		}
		const FString CanonicalLegacySettingsIni =
			GetDesktopHudCanonicalLegacySettingsIni();
		if (!bLoadedScale && CanonicalLegacySettingsIni != GGameUserSettingsIni)
		{
			bLoadedScale = GConfig->GetInt(
				HudSettingsSection,
				HudScaleConfigKey,
				SavedPercent,
				CanonicalLegacySettingsIni);
		}
	}
	HudScalePercent = NormalizeHudScalePercent(SavedPercent);
	if (bLoadedScale)
	{
		WriteDesktopHudStableScale(HudScalePercent);
	}
	bHudScaleSettingLoaded = true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SetHudScalePercent(const int32 InPercent)
{
	const int32 NewPercent = NormalizeHudScalePercent(InPercent);
	if (HudScalePercent == NewPercent)
	{
		return false;
	}
	HudScalePercent = NewPercent;
	bDesktopResolvedMetricsValid = false;
	bDesktopNativeLayoutDirty = true;
	if (GConfig)
	{
		WriteDesktopHudStableScale(HudScalePercent);
		GConfig->SetInt(
			HudSettingsSection,
			HudScaleConfigKey,
			HudScalePercent,
			GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	SetNotice(FText::FromString(FString::Printf(
		TEXT("HUD缩放已切换为 %d%%"),
		HudScalePercent)));
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
	const EGameXXKDesktopNoticeCategory ToolNoticeCategory =
		NoticeCategoryForToolMode(ActiveToolMode);
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
			SetNotice(
				FText::FromString(TEXT("工具输入来源已变化；未执行操作且未消耗任何道具")),
				ToolNoticeCategory);
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
		SetNotice(FText::FromString(TEXT("请先放入道具")), ToolNoticeCategory);
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
			SetNotice(
				FText::FromString(TEXT("镶嵌要求格0为装备、格1为宝石")),
				ToolNoticeCategory);
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
		SetNotice(
			FText::FromString(TEXT("强化和洗炼一次只能放入一件装备")),
			ToolNoticeCategory);
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
			SetNotice(
				FText::FromString(TEXT("请选择采用新词缀或保留原词缀")),
				ToolNoticeCategory);
			return false;
		}
		bSucceeded = Subsystem->ExecuteToolBeginReforge(Inputs[0], 0, Result);
		bKeepReservations = bSucceeded;
	}
	if (!bSucceeded)
	{
		SetNotice(
			Result.Message.IsEmpty()
				? FText::FromString(TEXT("工具执行失败；未改变输入道具"))
				: Result.Message,
			ToolNoticeCategory);
		return false;
	}
	if (!bKeepReservations)
	{
		ReturnAllToolEntries();
	}
	Subsystem->NormalizeDesktopInventoryState();
	SetNotice(bKeepReservations
		? FText::FromString(TEXT("洗炼预览已生成，请选择采用或保留"))
		: (Result.Message.IsEmpty() ? FText::FromString(TEXT("工具执行完成")) : Result.Message),
		ToolNoticeCategory);
	RefreshLayout();
	return true;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyAction(const int32 ActionId)
{
	if (ActionId == ActionStoryQuest)
	{
		RequestStoryCarriage();
		return;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (bExitConfirmationOpen && ActionId != 53 && ActionId != 54)
	{
		return;
	}
	if (ActionId == ActionToggleTown)
	{
		RequestTownToggle();
		return;
	}
	if (ActionId == ActionResetCombatGuide)
	{
		HandleResetCombatGuide();
		return;
	}
	if (ActionId == ActionIdleStripFold)
	{
		bIdleStripFolded = !bIdleStripFolded;
		bNoticeSettingsOpen = false;
		NoticeDisplayMode = EDesktopNoticeDisplayMode::Single;
		NoticeScrollOffset = 0;
		bDesktopNativeLayoutDirty = true;
		RefreshLayout();
		return;
	}
	if (ActionId == ActionNoticeSurface)
	{
		return;
	}
	if (ActionId == ActionNoticeCollapse)
	{
		bNoticeSettingsOpen = false;
		NoticeDisplayMode = NoticeDisplayMode == EDesktopNoticeDisplayMode::Long
			? EDesktopNoticeDisplayMode::Medium
			: EDesktopNoticeDisplayMode::Single;
		NoticeScrollOffset = 0;
		bDesktopNativeLayoutDirty = true;
		RefreshLayout();
		return;
	}
	if (ActionId == ActionNoticeExpand)
	{
		bNoticeSettingsOpen = false;
		NoticeDisplayMode = NoticeDisplayMode == EDesktopNoticeDisplayMode::Single
			? EDesktopNoticeDisplayMode::Medium
			: EDesktopNoticeDisplayMode::Long;
		NoticeScrollOffset = 0;
		bDesktopNativeLayoutDirty = true;
		RefreshLayout();
		return;
	}
	if (ActionId == ActionNoticeSettings)
	{
		bNoticeSettingsOpen = !bNoticeSettingsOpen;
		bDesktopNativeLayoutDirty = true;
		RefreshLayout();
		return;
	}
	if (ActionId >= ActionNoticeCategoryFirst
		&& ActionId < ActionNoticeCategoryFirst + UE_ARRAY_COUNT(NoticeCategories))
	{
		const EGameXXKDesktopNoticeCategory Category =
			NoticeCategories[ActionId - ActionNoticeCategoryFirst];
		NoticeCategoryEnabled.FindOrAdd(Category, true) = !NoticeCategoryEnabled.FindRef(Category);
		SaveNoticeCategorySetting(Category);
		NoticeScrollOffset = 0;
		RefreshLayout();
		return;
	}
	if (ActionId == ActionHudScale100
		|| ActionId == ActionHudScale75
		|| ActionId == ActionHudScale50)
	{
		const int32 SelectedPercent = ActionId == ActionHudScale50
			? 50
			: (ActionId == ActionHudScale75 ? 75 : 100);
		SetHudScalePercent(SelectedPercent);
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
			bTrainingDifficultyDropdownOpen = false;
		}
		RefreshLayout();
		return;
	}
	if (ActionId == ActionTrainingDifficultyDropdown)
	{
		CancelCarryForStructuralChange();
		bTrainingDifficultyDropdownOpen = !bTrainingDifficultyDropdownOpen;
		RefreshLayout();
		return;
	}
	if (ActionId >= ActionTrainingDifficultyFirst
		&& ActionId < ActionTrainingDifficultyFirst + 3)
	{
		CancelCarryForStructuralChange();
		const int32 DifficultyIndex = ActionId - ActionTrainingDifficultyFirst;
		const EGameXXKTrainingDifficulty Difficulty =
			TrainingDifficultyFromIndex(DifficultyIndex);
		ActiveTrainingDifficultyIndex = DifficultyIndex;
		ActiveTrainingChapter = ResolvePreferredTrainingChapter(Difficulty);
		SelectedStageId = ResolvePreferredTrainingStageForPage(Difficulty, ActiveTrainingChapter);
		Subsystem->SelectTrainingStage(SelectedStageId);
		bTrainingDifficultyDropdownOpen = false;
		RefreshLayout();
		return;
	}
	if (ActionId >= ActionTrainingChapterFirst
		&& ActionId < ActionTrainingChapterFirst + 3)
	{
		CancelCarryForStructuralChange();
		ActiveTrainingChapter = ActionId - ActionTrainingChapterFirst + 1;
		const EGameXXKTrainingDifficulty Difficulty =
			TrainingDifficultyFromIndex(ActiveTrainingDifficultyIndex);
		SelectedStageId = ResolvePreferredTrainingStageForPage(Difficulty, ActiveTrainingChapter);
		Subsystem->SelectTrainingStage(SelectedStageId);
		bTrainingDifficultyDropdownOpen = false;
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
		if (RequestedRoster == ActiveCharacterRoster)
		{
			PreserveEmbeddedSessionForCharacter(ActiveBackpackCharacterId);
			CancelCarryForStructuralChange();
			bCharacterRosterMembersExpanded = !bCharacterRosterMembersExpanded;
			RefreshLayout();
		}
		else
		{
			bCharacterRosterMembersExpanded = true;
			SelectBackpackCharacterForTest(
				ResolveRememberedBackpackCharacterId(RequestedRoster));
		}
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
	if (ActionId == 40 || ActionId == 41)
	{
		FGameXXKDesktopInventoryBatchTransferRequest Request;
		Request.FromContainer = ActionId == 40
			? EGameXXKDesktopItemContainer::Warehouse
			: EGameXXKDesktopItemContainer::Backpack;
		Request.ToContainer = ActionId == 40
			? EGameXXKDesktopItemContainer::Backpack
			: EGameXXKDesktopItemContainer::Warehouse;
		Request.WarehousePageIndex = GetWarehousePageIndexForTest();
		Request.WarehousePageSize = WarehousePageSize;
		Request.ExcludedEntries = BuildBatchTransferExclusions();
		FGameXXKDesktopInventoryBatchTransferResult Result;
		if (!FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(
			Subsystem->GetMutableRuntimeState(), Request, Result))
		{
			SetNotice(FText::FromString(
				Result.Error.IsEmpty() ? TEXT("当前物品无法批量转移") : Result.Error));
			RefreshLayout();
			return;
		}
		SetNotice(FText::FromString(Result.bDestinationFull
			? FString::Printf(TEXT("已移动 %d 件，目标空间不足"), Result.MovedEntryCount)
			: FString::Printf(TEXT("已移动 %d 件"), Result.MovedEntryCount)));
		RefreshLayout();
		return;
	}
	if (ActionId >= 70 && ActionId <= 75)
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
			RouteSettlementReceiptAtChallengeStart =
				Subsystem->GetRuntimeState().CardRun.LastAppliedRouteSettlementId;
			bRestoreTrainingPanelAfterChallenge = true;
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
		RefreshLayout();
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
		bSettingsPanelOpen = !bSettingsPanelOpen;
		RefreshLayout();
		break;
	case 53:
		CancelExitForTest();
		break;
	case 54:
		ConfirmExit(true);
		break;
	case 60:
		bNoticeSettingsOpen = false;
		if (bBackpackExpanded)
		{
			ResetWorkbenchChildrenForGlobalClose();
			bBackpackExpanded = false;
			bExitConfirmationOpen = false;
			bDesktopNativeLayoutDirty = true;
			UpdateTownPresentationInputLock();
			RefreshLayout();
			// Do not unload collapsed resources: reopening must stay safe and
			// fast. Only hide the expanded UI; keep textures/atlases alive.
		}
		else
		{
			OpenBackpack();
		}
		break;
	case 600:
	case 601:
		if (Subsystem)
		{
			FGameXXKTrainingChestOpenResult Result;
			const EGameXXKTrainingRewardTier Tier = ActionId == 600
				? EGameXXKTrainingRewardTier::NormalChest : EGameXXKTrainingRewardTier::AdvancedChest;
			if (Subsystem->OpenOneTrainingChest(Tier, Result))
			{
				SetNotice(
					FText::FromString(FString::Printf(
						TEXT("已开启 1 个宝箱：装备 %d 件，道具 %d 类"),
						Result.EquipmentInstanceIds.Num(),
						Result.ItemDeltas.Num())),
					EGameXXKDesktopNoticeCategory::ChestOpenResult);
				RefreshLayout();
			}
			else SetNotice(Result.Message, EGameXXKDesktopNoticeCategory::ChestOpenResult);
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
				SetNotice(
					FText::FromString(ActionId == 315 ? TEXT("已采用新词缀") : TEXT("已保留原词缀")),
					EGameXXKDesktopNoticeCategory::EnhanceReforge);
				RefreshLayout();
			}
			else SetNotice(
				Result.Message.IsEmpty()
					? FText::FromString(TEXT("当前没有待处理的洗炼结果"))
					: Result.Message,
				EGameXXKDesktopNoticeCategory::EnhanceReforge);
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
	if (ActionId == 600 || ActionId == 601)
	{
		FGameXXKTrainingChestOpenResult Result;
		const EGameXXKTrainingRewardTier Tier = ActionId == 600
			? EGameXXKTrainingRewardTier::NormalChest : EGameXXKTrainingRewardTier::AdvancedChest;
		const bool bSucceeded = Subsystem->OpenAllTrainingChests(Tier, Result);
		SetNotice(bSucceeded
			? FText::FromString(FString::Printf(TEXT("已开启 %d 个宝箱：装备 %d 件，道具 %d 类%s"),
				Result.OpenedCount,
				Result.EquipmentInstanceIds.Num(),
				Result.ItemDeltas.Num(),
				Result.Error == EGameXXKTrainingChestOpenError::BackpackFull ? TEXT("；背包已满，剩余宝箱保留") : TEXT("")))
			: Result.Message,
			EGameXXKDesktopNoticeCategory::ChestOpenResult);
		if (bSucceeded) RefreshLayout();
		return bSucceeded;
	}
	if (ActionId >= 100 && ActionId < 100 + WarehousePageSize)
	{
		const int32 PhysicalSlotIndex = GetWarehousePageIndexForTest() * WarehousePageSize + (ActionId - 100);
		const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::GetEntryAt(
			Subsystem->GetRuntimeState(),
			EGameXXKDesktopItemContainer::Warehouse,
			PhysicalSlotIndex);
		if (RequestTutorialMapInspection(Entry))
		{
			return true;
		}
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

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshNoticePresentation()
{
	if (NoticeLineTexts.IsEmpty())
	{
		return;
	}

	TArray<const FDesktopNoticeEntry*> EnabledEntries;
	EnabledEntries.Reserve(NoticeHistory.Num());
	for (const FDesktopNoticeEntry& Entry : NoticeHistory)
	{
		if (NoticeCategoryEnabled.FindRef(Entry.Category))
		{
			EnabledEntries.Add(&Entry);
		}
	}

	const int32 VisibleCapacity = NoticeLineTexts.Num();
	const int32 MaximumOffset = FMath::Max(0, EnabledEntries.Num() - VisibleCapacity);
	NoticeScrollOffset = FMath::Clamp(NoticeScrollOffset, 0, MaximumOffset);
	const int32 StartIndex = FMath::Max(
		0,
		EnabledEntries.Num() - VisibleCapacity - NoticeScrollOffset);
	const int32 EndIndex = FMath::Min(
		EnabledEntries.Num(),
		StartIndex + VisibleCapacity);
	const int32 DisplayedCount = EndIndex - StartIndex;

	for (int32 LineIndex = 0; LineIndex < NoticeLineTexts.Num(); ++LineIndex)
	{
		UTextBlock* LineText = NoticeLineTexts[LineIndex];
		if (!LineText)
		{
			continue;
		}
		if (LineIndex < DisplayedCount)
		{
			const FDesktopNoticeEntry& Entry = *EnabledEntries[StartIndex + LineIndex];
			LineText->SetText(Entry.Message);
			LineText->SetColorAndOpacity(NoticeCategoryColor(Entry.Category));
			if (UPanelWidget* Parent = LineText->GetParent())
			{
				Parent->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else if (EnabledEntries.IsEmpty() && LineIndex == 0)
		{
			LineText->SetText(FText::FromString(TEXT("暂无记录")));
			LineText->SetColorAndOpacity(FLinearColor(0.68f, 0.65f, 0.60f, 0.92f));
			if (UPanelWidget* Parent = LineText->GetParent())
			{
				Parent->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else if (UPanelWidget* Parent = LineText->GetParent())
		{
			Parent->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshNoticeControlVisibility()
{
	if (!NoticeRecordsBar)
	{
		return;
	}
	const bool bShowControls = !bBackpackExpanded
		&& (bNoticeSettingsOpen
			|| NoticeHoveredWidgetCount > 0
			|| NoticeHoverHideRemainingSeconds > 0.0f);
	NoticeRecordsBar->SetVisibility(
		bShowControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetNotice(
	const FText& Notice,
	const EGameXXKDesktopNoticeCategory Category)
{
	const bool bHistoryWasEmpty = NoticeHistory.IsEmpty();
	LastNotice = Notice;
	if (!Notice.IsEmpty())
	{
		FDesktopNoticeEntry& Entry = NoticeHistory.AddDefaulted_GetRef();
		Entry.Category = Category;
		Entry.Message = Notice;
		Entry.Ordinal = NextNoticeOrdinal++;
		if (NoticeHistory.Num() > NoticeHistoryCapacity)
		{
			NoticeHistory.RemoveAt(
				0,
				NoticeHistory.Num() - NoticeHistoryCapacity,
				EAllowShrinking::No);
		}
		NoticeScrollOffset = 0;
	}
	RefreshNoticePresentation();
	if (bHistoryWasEmpty && !Notice.IsEmpty() && NoticePanel)
	{
		NoticePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}
