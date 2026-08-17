#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

	namespace
{
	constexpr int32 WarehouseColumns = 4;
	constexpr int32 WarehousePageSize = 20;
	const FVector2D ShellSize(1920.0f, 1080.0f);
	const FVector4 ChallengeViewportRect(365.0f, 22.0f, 960.0f, 968.0f);
	const FVector4 ChallengeCombatStripRect(405.0f, 145.0f, 880.0f, 86.0f);
	const FVector4 ChallengeBattleBoardRect(395.0f, 240.0f, 710.0f, 535.0f);
	constexpr int32 ChallengeCombatSlotCount = 6;
	const FLinearColor Ink(0.06f, 0.045f, 0.035f, 0.98f);
	const FLinearColor Panel(0.13f, 0.09f, 0.055f, 0.97f);
	const FLinearColor PanelAlt(0.20f, 0.13f, 0.07f, 0.98f);
	const FLinearColor Accent(0.82f, 0.43f, 0.08f, 1.0f);
	const FLinearColor Gold(1.0f, 0.78f, 0.25f, 1.0f);
	static constexpr const TCHAR* PanelLargeTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	static constexpr const TCHAR* ButtonNeutralTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonNeutral.T_MasterV2_ButtonNeutral");
	static constexpr const TCHAR* ItemSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	static constexpr const TCHAR* EquipmentSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_EquipmentSlot.T_MasterV2_EquipmentSlot");
	static constexpr const TCHAR* TabNormalTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TabNormal.T_MasterV2_TabNormal");
	static constexpr const TCHAR* TabSelectedTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_TabSelected.T_MasterV2_TabSelected");
	static constexpr const TCHAR* RouteNodeTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavRoute.T_MasterV2_NavRoute");
	static constexpr const TCHAR* NavDiscBackpackTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavDiscBackpack.T_MasterV2_NavDiscBackpack");
	static constexpr const TCHAR* NavDiscCompanionTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavDiscCompanion.T_MasterV2_NavDiscCompanion");
	static constexpr const TCHAR* NavDiscCodexTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavDiscCodex.T_MasterV2_NavDiscCodex");
	static constexpr const TCHAR* NavDiscTaskTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavDiscTask.T_MasterV2_NavDiscTask");
	static constexpr const TCHAR* NavDiscRouteTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_NavDiscRoute.T_MasterV2_NavDiscRoute");

	TMap<FString, TWeakObjectPtr<UTexture2D>>& GetTextureCache()
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
		return Cache;
	}

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		if (!Path)
		{
			return nullptr;
		}
		TMap<FString, TWeakObjectPtr<UTexture2D>>& Cache = GetTextureCache();
		const FString Key(Path);
		if (const TWeakObjectPtr<UTexture2D>* Cached = Cache.Find(Key))
		{
			if (Cached->IsValid())
			{
				return Cached->Get();
			}
		}
		UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, Path);
		Cache.FindOrAdd(Key) = Loaded;
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

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
		Result->SetAutoWrapText(true);
		return Result;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		const FSlateBrush Brush = MakeBoxTextureBrush(PanelLargeTexturePath, FVector2D(320.0f, 180.0f));
		if (Brush.GetResourceObject())
		{
			Result->SetBrush(Brush);
		}
		else
		{
			Result->SetBrushColor(Color);
		}
		return Result;
	}

	UBorder* MakeSlotPanel(UWidgetTree* Tree, const TCHAR* TexturePath, const FLinearColor& Color, const FVector2D& ImageSize)
	{
		UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		const FSlateBrush Brush = MakeBoxTextureBrush(TexturePath, ImageSize, FMargin(0.08f));
		if (Brush.GetResourceObject())
		{
			Result->SetBrush(Brush);
		}
		else
		{
			Result->SetBrushColor(Color);
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

	FText NavText(const EGameXXKDesktopTrainingNav Nav)
	{
		switch (Nav)
		{
		case EGameXXKDesktopTrainingNav::Warehouse: return FText::FromString(TEXT("仓库"));
		case EGameXXKDesktopTrainingNav::Formation: return FText::FromString(TEXT("编队"));
		case EGameXXKDesktopTrainingNav::Talents: return FText::FromString(TEXT("天赋"));
		case EGameXXKDesktopTrainingNav::Tools: return FText::FromString(TEXT("工具"));
		default: return FText::FromString(TEXT("历练"));
		}
	}

	const TCHAR* NavDiscTexturePath(const EGameXXKDesktopTrainingNav Nav)
	{
		switch (Nav)
		{
		case EGameXXKDesktopTrainingNav::Warehouse: return NavDiscBackpackTexturePath;
		case EGameXXKDesktopTrainingNav::Formation: return NavDiscCompanionTexturePath;
		case EGameXXKDesktopTrainingNav::Talents: return NavDiscCodexTexturePath;
		case EGameXXKDesktopTrainingNav::Tools: return NavDiscTaskTexturePath;
		default: return NavDiscRouteTexturePath;
		}
	}

	UWidget* MakeNavigationContent(
		UWidgetTree* Tree,
		const EGameXXKDesktopTrainingNav Nav,
		const bool bSelected)
	{
		UHorizontalBox* Content = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* IconBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(46.0f);
		IconBox->SetHeightOverride(46.0f);
		UImage* Icon = Tree->ConstructWidget<UImage>(UImage::StaticClass());
		const FLinearColor IconTint = bSelected ? FLinearColor::White : FLinearColor(0.78f, 0.72f, 0.62f, 1.0f);
		const FSlateBrush IconBrush = MakeTextureBrush(NavDiscTexturePath(Nav), FVector2D(46.0f, 46.0f), IconTint);
		if (IconBrush.GetResourceObject())
		{
			Icon->SetBrush(IconBrush);
		}
		IconBox->AddChild(Icon);
		UHorizontalBoxSlot* IconSlot = Content->AddChildToHorizontalBox(IconBox);
		IconSlot->SetPadding(FMargin(5.0f, 6.0f, 4.0f, 6.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Label = MakeText(Tree, NavText(Nav), 22, bSelected ? FLinearColor::White : FLinearColor(0.86f, 0.76f, 0.62f, 1.0f));
		UHorizontalBoxSlot* LabelSlot = Content->AddChildToHorizontalBox(Label);
		LabelSlot->SetPadding(FMargin(2.0f, 0.0f, 7.0f, 0.0f));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
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
}

void UGameXXKDesktopTrainingStageButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const FName InStageId)
{
	Owner = InOwner;
	StageId = InStageId;
	SetStyle(MakeTextureButtonStyle(RouteNodeTexturePath, FVector2D(76.0f, 76.0f), FMargin(0.08f)));
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingStageButton::HandleClicked);
}

void UGameXXKDesktopTrainingStageButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleStageClicked(StageId);
	}
}

void UGameXXKDesktopTrainingActionButton::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const int32 InActionId)
{
	Owner = InOwner;
	ActionId = InActionId;
	SetStyle(MakeTextureButtonStyle(ButtonNeutralTexturePath, FVector2D(150.0f, 54.0f), FMargin(0.08f)));
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKDesktopTrainingActionButton::HandleClicked);
}

void UGameXXKDesktopTrainingActionButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleActionClicked(ActionId);
	}
}

TSharedRef<SWidget> UGameXXKDesktopTrainingWorkbenchWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SelectedStageId.IsNone())
	{
		SelectedStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	}
	BuildProgrammaticLayout();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKDesktopTrainingWorkbenchWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ViewMode != EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		const UGameXXKMVPSubsystem* TravelSubsystem = ResolveMVPSubsystem();
		if (!TravelSubsystem || !TravelSubsystem->GetRuntimeState().Training.bTravelActive)
		{
			UpdateTravelCooldownText();
			return;
		}
		UpdateTravelCooldownText();
		TravelAccumulator += InDeltaTime;
		if (TravelAccumulator >= 1.0f)
		{
			const int32 ElapsedSeconds = FMath::Max(1, FMath::FloorToInt(TravelAccumulator));
			TravelAccumulator -= static_cast<float>(ElapsedSeconds);
			AdvanceTravelForTest(ElapsedSeconds);
		}
		return;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetRuntimeState().Training.bChallengeActive || !Subsystem->GetRuntimeState().Training.bChallengeAutoBattle)
	{
		return;
	}
	AutoBattleAccumulator += InDeltaTime;
	if (AutoBattleAccumulator < 0.75f)
	{
		return;
	}
	AutoBattleAccumulator = 0.0f;
	AdvanceChallengeForTest();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenWorkbench()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
	SelectedStageId = Progress.SelectedStageId.IsNone() ? Progress.CurrentTravelStageId : Progress.SelectedStageId;
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	if (ActiveBackpackCharacterId.IsNone() || !CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		ActiveBackpackCharacterId = CharacterIds.Num() > 0
			? CharacterIds[0]
			: FGameXXKEquipmentRules::HeroCharacterId();
	}
	ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	bSettingsPanelOpen = false;
	bChallengeSidePanelsReadOnly = false;
	TravelAccumulator = 0.0f;
	RefreshLayout();
	SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CloseWorkbench()
{
	const bool bWasVisible = IsInViewport() && GetVisibility() != ESlateVisibility::Collapsed;
	bSettingsPanelOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
	return bWasVisible;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::OpenBackpack()
{
	ActiveNav = EGameXXKDesktopTrainingNav::Formation;
	ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	bSettingsPanelOpen = false;
	bChallengeSidePanelsReadOnly = false;
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	if (ActiveBackpackCharacterId.IsNone() || !CharacterIds.Contains(ActiveBackpackCharacterId))
	{
		ActiveBackpackCharacterId = CharacterIds.Num() > 0
			? CharacterIds[0]
			: FGameXXKEquipmentRules::HeroCharacterId();
	}
	RefreshLayout();
	return true;
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

bool UGameXXKDesktopTrainingWorkbenchWidget::IsToolsPanelActiveForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::Workbench
		&& ActiveNav == EGameXXKDesktopTrainingNav::Tools;
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

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectBackpackCharacterForTest(const FName CharacterId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || CharacterId.IsNone() || !GetBackpackCharacterIdsForTest().Contains(CharacterId))
	{
		return false;
	}
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	if (!Subsystem->GetEquipmentLoadoutSnapshot(CharacterId, Snapshot))
	{
		return false;
	}
	ActiveBackpackCharacterId = CharacterId;
	ActiveNav = EGameXXKDesktopTrainingNav::Formation;
	ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	bSettingsPanelOpen = false;
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
		&& ViewMode == EGameXXKDesktopTrainingViewMode::Workbench
		&& ActiveNav != EGameXXKDesktopTrainingNav::Talents;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehouseColumnCountForTest() const
{
	return WarehouseColumns;
}

TArray<FString> UGameXXKDesktopTrainingWorkbenchWidget::GetMasterV2ResourcePathsForTest() const
{
	const TCHAR* RequiredPaths[] = {
		PanelLargeTexturePath,
		ButtonNeutralTexturePath,
		ItemSlotTexturePath,
		EquipmentSlotTexturePath,
		TabNormalTexturePath,
		TabSelectedTexturePath,
		RouteNodeTexturePath,
		NavDiscBackpackTexturePath,
		NavDiscCompanionTexturePath,
		NavDiscCodexTexturePath,
		NavDiscTaskTexturePath,
		NavDiscRouteTexturePath};
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
	if (!Subsystem || ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
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
	return Subsystem ? Subsystem->GetRuntimeState().EquipmentCollection.WarehouseInstanceIds.Num() : 0;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehousePageCountForTest() const
{
	return FMath::Max(1, FMath::DivideAndRoundUp(GetWarehouseOccupancyForTest(), WarehousePageSize));
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetWarehousePageIndexForTest() const
{
	return FMath::Clamp(WarehousePageIndex, 0, GetWarehousePageCountForTest() - 1);
}

TArray<FName> UGameXXKDesktopTrainingWorkbenchWidget::GetVisibleWarehouseInstanceIdsForTest() const
{
	TArray<FName> Warehouse;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->GetEquipmentWarehouseSnapshot(Warehouse))
	{
		return Warehouse;
	}
	const int32 PageStart = GetWarehousePageIndexForTest() * WarehousePageSize;
	const int32 PageEnd = FMath::Min(PageStart + WarehousePageSize, Warehouse.Num());
	TArray<FName> Visible;
	for (int32 Index = PageStart; Index < PageEnd; ++Index)
	{
		Visible.Add(Warehouse[Index]);
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
	const TArray<FName> VisibleWarehouse = GetVisibleWarehouseInstanceIdsForTest();
	if (!VisibleWarehouse.IsValidIndex(VisibleSlotIndex))
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	FGameXXKCharacterBackpackModel BackpackModel;
	BackpackModel.Bind(Subsystem, GetActiveBackpackCharacterIdForTest());
	FGameXXKEquipmentTransactionResult Result;
	const bool bEquipped = BackpackModel.QuickEquip(VisibleWarehouse[VisibleSlotIndex], Result);
	if (bEquipped)
	{
		SetNotice(Result.Message.IsEmpty() ? FText::FromString(TEXT("装备已转入当前角色")) : Result.Message);
		RefreshLayout();
	}
	return bEquipped;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SortWarehouseForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->SortEquipmentWarehouse())
	{
		return false;
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
		SetNotice(Result.Message.IsEmpty() ? FText::FromString(TEXT("装备已卸下并返回仓库")) : Result.Message);
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

bool UGameXXKDesktopTrainingWorkbenchWidget::IsChallengeViewportActiveForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AreChallengeSidePanelsReadOnlyForTest() const
{
	return bChallengeSidePanelsReadOnly
		&& ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsAutoBattleVisibleForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsRetryVisibleForTest() const
{
	return ViewMode == EGameXXKDesktopTrainingViewMode::Workbench;
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetChallengeViewportRectForTest() const
{
	return ChallengeViewportRect;
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetChallengeCombatStripRectForTest() const
{
	return ChallengeCombatStripRect;
}

FVector4 UGameXXKDesktopTrainingWorkbenchWidget::GetChallengeBattleBoardRectForTest() const
{
	return ChallengeBattleBoardRect;
}

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetChallengeCombatSlotCountForTest() const
{
	return ChallengeCombatSlotCount;
}

FText UGameXXKDesktopTrainingWorkbenchWidget::GetStageTooltipForTest(const FName StageId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->BuildTrainingStageTooltip(StageId) : FText::GetEmpty();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SelectStageForTest(const FName StageId)
{
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
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ClickTravelForTest()
{
	ApplyAction(7);
	return ViewMode == EGameXXKDesktopTrainingViewMode::Workbench;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::ToggleAutoBattleForTest(const bool bEnabled)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport
		&& Subsystem && Subsystem->SetTrainingChallengeAutoBattle(bEnabled);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AdvanceChallengeForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ViewMode != EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		return false;
	}
	bool bCompleted = false;
	FGameXXKTrainingReward Reward;
	if (!Subsystem->AdvanceTrainingChallengeEncounter(bCompleted, Reward))
	{
		return false;
	}
	if (bCompleted)
	{
		ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
		SetNotice(FText::FromString(TEXT("挑战完成：已结算金币、经验与宝箱")));
		RefreshLayout();
	}
	else
	{
		SetNotice(FText::FromString(TEXT("自动战斗：击败当前遭遇，继续路线")));
	}
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::AdvanceTravelForTest(const int32 ElapsedSeconds)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ViewMode != EGameXXKDesktopTrainingViewMode::Workbench)
	{
		return false;
	}
	bool bEncounterCompleted = false;
	bool bCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	if (!Subsystem->AdvanceTrainingTravelStep(bEncounterCompleted, bCompleted, bDefeated, Reward, FMath::Max(1, ElapsedSeconds)))
	{
		return false;
	}
	if (bDefeated)
	{
		const bool bRetry = Subsystem->GetTrainingProgressCopy().bRetryOnFailure;
		if (Subsystem->ResolveTrainingTravelFailure())
		{
			SetNotice(bRetry
				? FText::FromString(TEXT("游历阵亡：重试当前关卡"))
				: FText::FromString(TEXT("游历阵亡：已暂停并回退前一关")));
			RefreshLayout();
		}
	}
	else if (bCompleted)
	{
		SetNotice(FText::FromString(FString::Printf(TEXT("游历结算：+%d 金币 / +%d 经验，继续循环"), Reward.Gold, Reward.Experience)));
		RefreshLayout();
	}
	else if (bEncounterCompleted)
	{
		SetNotice(FText::FromString(TEXT("游历中：击杀当前怪物，继续走动")));
		RefreshLayout();
	}
	else
	{
		SetNotice(FText::FromString(TEXT("游历中：走动、遭遇、自动战斗")));
	}
	return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::SetRetryOnFailureForTest(const bool bEnabled)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->SetTrainingRetryOnFailure(bEnabled);
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
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesktopTrainingWorkbenchRoot"));
	}
	if (!RootCanvas)
	{
		return;
	}
	WidgetTree->RootWidget = RootCanvas;
	if (ChallengeBattleBoard && ChallengeBattleVisualSessionToken != 0)
	{
		ChallengeBattleBoard->CancelBattleVisualSession(ChallengeBattleVisualSessionToken);
		ChallengeBattleVisualSessionToken = 0;
	}
	TravelCooldownText = nullptr;
	RootCanvas->ClearChildren();
	StageButtons.Reset();
	ActionButtons.Reset();
	BuildWorkbenchShell();
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWorkbenchShell()
{
	bChallengeSidePanelsReadOnly = ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport;
	AddCanvas(RootCanvas, MakePanel(WidgetTree, Ink), FVector2D::ZeroVector, ShellSize);
	if (ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
	{
		BuildWarehousePanel(true);
		BuildChallengeViewport();
		BuildTrainingMapPanel(true);
	}
	else
	{
		BuildTopIdleStrip();
		BuildWarehousePanel();
		if (ActiveNav == EGameXXKDesktopTrainingNav::Talents)
		{
			BuildTalentsPanel();
		}
		else
		{
			BuildBackpackPanel();
		}
		if (ActiveNav == EGameXXKDesktopTrainingNav::Tools)
		{
			BuildToolsPanel();
		}
		else
		{
			BuildTrainingMapPanel();
		}
	}
	BuildBottomNavigation();
	NoticePanel = MakePanel(WidgetTree, FLinearColor(0.08f, 0.05f, 0.03f, 0.96f));
	NoticeText = MakeText(WidgetTree, LastNotice, 22, Gold);
	NoticePanel->SetContent(NoticeText);
	AddCanvas(RootCanvas, NoticePanel.Get(), FVector2D(700.0f, 18.0f), FVector2D(520.0f, 48.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTopIdleStrip()
{
	UBorder* Strip = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Strip, FVector2D(360.0f, 22.0f), FVector2D(1200.0f, 108.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKTrainingTravelRuntime TravelRuntime = Subsystem
		? Subsystem->GetTrainingTravelRuntimeCopy()
		: FGameXXKTrainingTravelRuntime();
	FString PhaseLabel = TEXT("待机");
	switch (TravelRuntime.Phase)
	{
	case EGameXXKTrainingTravelPhase::Walking: PhaseLabel = TEXT("走动"); break;
	case EGameXXKTrainingTravelPhase::Combat: PhaseLabel = TEXT("自动战斗"); break;
	case EGameXXKTrainingTravelPhase::Defeated: PhaseLabel = TEXT("阵亡"); break;
	default: break;
	}
	FString CurrentEnemyDisplayName = TravelRuntime.EnemyDefinitionId.ToString();
	FString EnemyLabel = TEXT("当前遭遇：等待");
	if (Subsystem && !TravelRuntime.EnemyDefinitionId.IsNone())
	{
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = Subsystem->GetTrainingEncounterSequence(TravelRuntime.StageId, true);
		if (Encounters.IsValidIndex(TravelRuntime.EncounterIndex))
		{
			CurrentEnemyDisplayName = Encounters[TravelRuntime.EncounterIndex].DisplayName.ToString();
			EnemyLabel = FString::Printf(
				TEXT("当前遭遇：%s · %s · HP %d/%d"),
				*Encounters[TravelRuntime.EncounterIndex].DisplayName.ToString(),
				*PhaseLabel,
				TravelRuntime.EnemyHP,
				TravelRuntime.EnemyMaxHP);
		}
	}
	UTextBlock* Label = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("游历挂机 · 3 敌方 / 3 我方 · %s"), *EnemyLabel)), 18, Gold);
	AddCanvas(RootCanvas, Label, FVector2D(385.0f, 38.0f), FVector2D(650.0f, 40.0f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bCurrentEnemy = Index == 0 && !TravelRuntime.EnemyDefinitionId.IsNone();
		const FString EnemyText = bCurrentEnemy
			? FString::Printf(TEXT("敌 %d\n%s\n%d/%d"), Index + 1, *CurrentEnemyDisplayName, TravelRuntime.EnemyHP, TravelRuntime.EnemyMaxHP)
			: FString::Printf(TEXT("敌 %d\n待机"), Index + 1);
		UTextBlock* Enemy = MakeText(WidgetTree, FText::FromString(EnemyText), 17, FLinearColor(1.0f, 0.65f, 0.55f, 1.0f));
		AddCanvas(RootCanvas, Enemy, FVector2D(725.0f + Index * 115.0f, 35.0f), FVector2D(95.0f, 58.0f));
		const FString PartyText = Index == 0
			? FString::Printf(TEXT("角 %d\n%s\n%d/%d"), Index + 1, *PhaseLabel, TravelRuntime.PlayerHP, TravelRuntime.PlayerMaxHP)
			: FString::Printf(TEXT("角 %d\n待机"), Index + 1);
		UTextBlock* Party = MakeText(WidgetTree, FText::FromString(PartyText), 17, FLinearColor(0.55f, 0.85f, 1.0f, 1.0f));
		AddCanvas(RootCanvas, Party, FVector2D(1080.0f + Index * 115.0f, 35.0f), FVector2D(95.0f, 58.0f));
	}
	const FGameXXKTrainingOfflineReward PendingReward = Subsystem
		? Subsystem->GetPendingTrainingTravelRewardCopy()
		: FGameXXKTrainingOfflineReward();
	UTextBlock* PendingLabel = MakeText(
		WidgetTree,
		FText::FromString(FString::Printf(
			TEXT("待收菜：%d 金 · 普通箱 %d · 精英箱 %d"),
			PendingReward.Gold,
			PendingReward.NormalChestCount,
			PendingReward.AdvancedChestCount)),
		15,
		FLinearColor(0.90f, 0.82f, 0.56f, 1.0f));
	AddCanvas(RootCanvas, PendingLabel, FVector2D(1335.0f, 28.0f), FVector2D(145.0f, 44.0f));
	TravelCooldownText = MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.95f, 0.82f, 0.46f, 1.0f));
	TravelCooldownText->SetToolTipText(FText::FromString(TEXT("游历宝箱概率与局内一致；普通箱掉落后 4 分钟冷却，精英箱掉落后 6 分钟冷却。")));
	AddCanvas(RootCanvas, TravelCooldownText.Get(), FVector2D(385.0f, 88.0f), FVector2D(335.0f, 30.0f));
	UpdateTravelCooldownText();
	UGameXXKDesktopTrainingActionButton* CollectButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	CollectButton->Configure(this, 16);
	CollectButton->SetBackgroundColor(PendingReward.Gold > 0 || PendingReward.NormalChestCount > 0 || PendingReward.AdvancedChestCount > 0 ? Accent : Panel);
	CollectButton->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("收菜")), 18));
	CollectButton->SetToolTipText(FText::FromString(TEXT("领取离线游历的金币、经验和宝箱；宝箱概率与局内一致")));
	CollectButton->SetIsEnabled(PendingReward.Gold > 0 || PendingReward.Experience > 0 || PendingReward.NormalChestCount > 0 || PendingReward.AdvancedChestCount > 0);
	AddCanvas(RootCanvas, CollectButton, FVector2D(1325.0f, 72.0f), FVector2D(145.0f, 42.0f));
	ActionButtons.Add(CollectButton);
	UGameXXKDesktopTrainingActionButton* RetryButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	RetryButton->Configure(this, 10);
	RetryButton->SetBackgroundColor(Accent);
	RetryButton->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("失败重试")), 18));
	AddCanvas(RootCanvas, RetryButton, FVector2D(1485.0f, 42.0f), FVector2D(135.0f, 50.0f));
	ActionButtons.Add(RetryButton);
}

void UGameXXKDesktopTrainingWorkbenchWidget::UpdateTravelCooldownText()
{
	if (!TravelCooldownText)
	{
		return;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKTrainingProgress Progress = Subsystem
		? Subsystem->GetTrainingProgressCopy()
		: FGameXXKTrainingProgress();
	const auto FormatCooldown = [](const int32 RemainingSeconds) -> FString
	{
		const int32 SafeSeconds = FMath::Max(0, RemainingSeconds);
		return FString::Printf(TEXT("%02d:%02d"), SafeSeconds / 60, SafeSeconds % 60);
	};
	const FString NormalText = Progress.TravelNormalChestCooldownRemainingSeconds > 0
		? FString::Printf(TEXT("普通箱 CD %s"), *FormatCooldown(Progress.TravelNormalChestCooldownRemainingSeconds))
		: TEXT("普通箱 可掉落");
	const FString AdvancedText = Progress.TravelAdvancedChestCooldownRemainingSeconds > 0
		? FString::Printf(TEXT("精英箱 CD %s"), *FormatCooldown(Progress.TravelAdvancedChestCooldownRemainingSeconds))
		: TEXT("精英箱 可掉落");
	TravelCooldownText->SetText(FText::FromString(FString::Printf(TEXT("掉箱冷却 · %s · %s"), *NormalText, *AdvancedText)));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildWarehousePanel(const bool bReadOnly)
{
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(24.0f, 150.0f), FVector2D(320.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(bReadOnly ? TEXT("仓库  ·  4 列  ·  只读") : TEXT("仓库  ·  4 列")), 28, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(48.0f, 174.0f), FVector2D(260.0f, 42.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	TArray<FName> Warehouse;
	if (Subsystem)
	{
		Subsystem->GetEquipmentWarehouseSnapshot(Warehouse);
	}
	const TArray<FName> VisibleWarehouse = GetVisibleWarehouseInstanceIdsForTest();
	for (int32 SlotIndex = 0; SlotIndex < 20; ++SlotIndex)
	{
		const int32 Column = SlotIndex % WarehouseColumns;
		const int32 Row = SlotIndex / WarehouseColumns;
		const FVector2D CellPosition(46.0f + Column * 68.0f, 235.0f + Row * 68.0f);
		if (VisibleWarehouse.IsValidIndex(SlotIndex))
		{
			const FName InstanceId = VisibleWarehouse[SlotIndex];
			const FString EquipmentLabel = RuntimeState
				? EquipmentDisplayName(RuntimeState->EquipmentCollection, InstanceId)
				: InstanceId.ToString();
			if (bReadOnly)
			{
				UBorder* Cell = MakeSlotPanel(WidgetTree, EquipmentSlotTexturePath, FLinearColor(0.07f, 0.06f, 0.05f, 1.0f), FVector2D(58.0f, 58.0f));
				Cell->SetContent(MakeText(WidgetTree, FText::FromString(EquipmentLabel), 10, FLinearColor::White));
				Cell->SetToolTipText(FText::FromString(FString::Printf(TEXT("装备实例：%s\n%s\n挑战中只读"), *InstanceId.ToString(), *EquipmentLabel)));
				AddCanvas(RootCanvas, Cell, CellPosition, FVector2D(58.0f, 58.0f));
			}
			else
			{
				UGameXXKDesktopTrainingActionButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
				SlotButton->Configure(this, 100 + SlotIndex);
				SlotButton->SetStyle(MakeTextureButtonStyle(EquipmentSlotTexturePath, FVector2D(58.0f, 58.0f), FMargin(0.08f)));
				SlotButton->SetBackgroundColor(FLinearColor(0.07f, 0.06f, 0.05f, 1.0f));
				SlotButton->SetContent(MakeText(WidgetTree, FText::FromString(EquipmentLabel), 10, FLinearColor::White));
				SlotButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("装备实例：%s\n%s\n点击装备到当前角色"), *InstanceId.ToString(), *EquipmentLabel)));
				AddCanvas(RootCanvas, SlotButton, CellPosition, FVector2D(58.0f, 58.0f));
				ActionButtons.Add(SlotButton);
			}
		}
		else
		{
			UBorder* Cell = MakeSlotPanel(WidgetTree, ItemSlotTexturePath, FLinearColor(0.07f, 0.06f, 0.05f, 1.0f), FVector2D(58.0f, 58.0f));
			AddCanvas(RootCanvas, Cell, CellPosition, FVector2D(58.0f, 58.0f));
		}
	}
	const int32 WarehouseCount = Warehouse.Num();
	UTextBlock* PageText = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("第 %d / %d 页 · 每页 %d 格"),
		GetWarehousePageIndexForTest() + 1,
		GetWarehousePageCountForTest(),
		WarehousePageSize)), 15, FLinearColor(0.78f, 0.70f, 0.60f, 1.0f));
	AddCanvas(RootCanvas, PageText, FVector2D(48.0f, 805.0f), FVector2D(230.0f, 28.0f));
	if (bReadOnly)
	{
		UTextBlock* ReadOnlyText = MakeText(WidgetTree, FText::FromString(TEXT("挑战进行中\n仓库只读 · 不可翻页、排序或装备")), 15, FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
		AddCanvas(RootCanvas, ReadOnlyText, FVector2D(48.0f, 850.0f), FVector2D(250.0f, 52.0f));
	}
	else
	{
		UGameXXKDesktopTrainingActionButton* Previous = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Previous->Configure(this, 40);
		Previous->SetBackgroundColor(PanelAlt);
		Previous->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("上一页")), 15));
		Previous->SetIsEnabled(GetWarehousePageIndexForTest() > 0);
		AddCanvas(RootCanvas, Previous, FVector2D(48.0f, 850.0f), FVector2D(90.0f, 40.0f));
		ActionButtons.Add(Previous);
		UGameXXKDesktopTrainingActionButton* Next = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Next->Configure(this, 41);
		Next->SetBackgroundColor(PanelAlt);
		Next->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("下一页")), 15));
		Next->SetIsEnabled(GetWarehousePageIndexForTest() + 1 < GetWarehousePageCountForTest());
		AddCanvas(RootCanvas, Next, FVector2D(150.0f, 850.0f), FVector2D(90.0f, 40.0f));
		ActionButtons.Add(Next);
		UGameXXKDesktopTrainingActionButton* Sort = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Sort->Configure(this, 5);
		Sort->SetBackgroundColor(Accent);
		Sort->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("排序")), 15));
		Sort->SetToolTipText(FText::FromString(TEXT("按槽位、品质和等级排序仓库")));
		AddCanvas(RootCanvas, Sort, FVector2D(248.0f, 850.0f), FVector2D(76.0f, 40.0f));
		ActionButtons.Add(Sort);
	}
	UTextBlock* Footer = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("装备实例 %d / %d\n%s"),
		WarehouseCount,
		FGameXXKEquipmentRules::WarehouseCapacity,
		bReadOnly ? TEXT("挑战中保持只读") : TEXT("不显示角色身份卡"))), 16, FLinearColor(0.75f, 0.68f, 0.55f, 1.0f));
	AddCanvas(RootCanvas, Footer, FVector2D(48.0f, 900.0f), FVector2D(240.0f, 54.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBackpackPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(365.0f, 150.0f), FVector2D(960.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree,
		ActiveNav == EGameXXKDesktopTrainingNav::Formation
			? FText::FromString(TEXT("编队  ·  角色 / 伙伴"))
			: FText::FromString(TEXT("背包  ·  角色装备")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(400.0f, 175.0f), FVector2D(700.0f, 46.0f));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* RuntimeState = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FString GoldLabel = RuntimeState
		? FString::Printf(TEXT("金币  %d  ·  数据来自存档"), RuntimeState->PlayerGold)
		: TEXT("金币  --  ·  等待存档");
	UTextBlock* GoldText = MakeText(WidgetTree, FText::FromString(GoldLabel), 20, Gold);
	AddCanvas(RootCanvas, GoldText, FVector2D(810.0f, 180.0f), FVector2D(190.0f, 40.0f));
	UGameXXKDesktopTrainingActionButton* Settings = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Settings->Configure(this, 14);
	Settings->SetBackgroundColor(bSettingsPanelOpen ? Accent : Panel);
	Settings->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("设置")), 17));
	Settings->SetToolTipText(FText::FromString(TEXT("打开独立设置面板；不会关闭历练工作台")));
	AddCanvas(RootCanvas, Settings, FVector2D(1015.0f, 180.0f), FVector2D(90.0f, 40.0f));
	ActionButtons.Add(Settings);
	UGameXXKDesktopTrainingActionButton* Close = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Close->Configure(this, 15);
	Close->SetBackgroundColor(FLinearColor(0.30f, 0.10f, 0.08f, 1.0f));
	Close->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("关闭")), 17));
	Close->SetToolTipText(FText::FromString(TEXT("关闭桌面历练工作台；与设置按钮独立")));
	AddCanvas(RootCanvas, Close, FVector2D(1115.0f, 180.0f), FVector2D(90.0f, 40.0f));
	ActionButtons.Add(Close);
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	for (int32 CharacterIndex = 0; CharacterIndex < CharacterIds.Num(); ++CharacterIndex)
	{
		UGameXXKDesktopTrainingActionButton* CharacterButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		CharacterButton->Configure(this, 20 + CharacterIndex);
		CharacterButton->SetBackgroundColor(CharacterIds[CharacterIndex] == GetActiveBackpackCharacterIdForTest() ? Accent : Panel);
		CharacterButton->SetContent(MakeText(
			WidgetTree,
			FText::FromString(BackpackCharacterDisplayName(Subsystem, CharacterIds[CharacterIndex])),
			16));
		AddCanvas(RootCanvas, CharacterButton, FVector2D(405.0f + CharacterIndex * 170.0f, 220.0f), FVector2D(155.0f, 42.0f));
		ActionButtons.Add(CharacterButton);
	}
	const FName ActiveCharacterId = GetActiveBackpackCharacterIdForTest();
	FGameXXKCharacterBackpackModel BackpackModel;
	if (Subsystem)
	{
		BackpackModel.Bind(const_cast<UGameXXKMVPSubsystem*>(Subsystem), ActiveCharacterId);
	}
	const TArray<FGameXXKCharacterBackpackSlotView> SlotViews = BackpackModel.GetSixSlotSnapshot();
	TArray<FName> EquippedInstanceIds;
	for (const FGameXXKCharacterBackpackSlotView& SlotView : SlotViews)
	{
		EquippedInstanceIds.Add(SlotView.EquippedInstanceId);
	}
	for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
	{
		const FVector2D SlotPosition(405.0f + (SlotIndex % 3) * 90.0f, 275.0f + (SlotIndex / 3) * 90.0f);
		const bool bHasEquippedInstance = EquippedInstanceIds.IsValidIndex(SlotIndex)
			&& !EquippedInstanceIds[SlotIndex].IsNone()
			&& RuntimeState;
		if (bHasEquippedInstance)
		{
			const FString EquipmentLabel = EquipmentDisplayName(RuntimeState->EquipmentCollection, EquippedInstanceIds[SlotIndex]);
			UGameXXKDesktopTrainingActionButton* EquipButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
			EquipButton->Configure(this, 200 + SlotIndex);
			EquipButton->SetStyle(MakeTextureButtonStyle(EquipmentSlotTexturePath, FVector2D(78.0f, 78.0f), FMargin(0.08f)));
			EquipButton->SetBackgroundColor(FLinearColor(0.10f, 0.07f, 0.05f, 1.0f));
			EquipButton->SetContent(MakeText(WidgetTree, FText::FromString(EquipmentLabel), 11, FLinearColor::White));
			EquipButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("已装备实例：%s\n%s\n点击卸下并返回仓库"), *EquippedInstanceIds[SlotIndex].ToString(), *EquipmentLabel)));
			AddCanvas(RootCanvas, EquipButton, SlotPosition, FVector2D(78.0f, 78.0f));
			ActionButtons.Add(EquipButton);
		}
		else
		{
			UBorder* EmptyEquip = MakeSlotPanel(WidgetTree, EquipmentSlotTexturePath, FLinearColor(0.10f, 0.07f, 0.05f, 1.0f), FVector2D(78.0f, 78.0f));
			AddCanvas(RootCanvas, EmptyEquip, SlotPosition, FVector2D(78.0f, 78.0f));
		}
	}
	FGameXXKEquipmentLoadoutSnapshot ActiveLoadoutSnapshot;
	const bool bHasActiveLoadout = Subsystem && Subsystem->GetEquipmentLoadoutSnapshot(ActiveCharacterId, ActiveLoadoutSnapshot);
	int32 ActiveLevel = RuntimeState ? RuntimeState->PlayerLevel : 0;
	int32 ActiveHP = RuntimeState ? RuntimeState->PlayerHP : 0;
	int32 ActiveMaxHP = RuntimeState ? RuntimeState->PlayerMaxHP : 0;
	int32 ActiveMP = RuntimeState ? RuntimeState->PlayerMP : 0;
	int32 ActiveMaxMP = RuntimeState ? RuntimeState->PlayerMaxMP : 0;
	if (RuntimeState && ActiveCharacterId != FGameXXKEquipmentRules::HeroCharacterId())
	{
		FGameXXKPermanentCompanion Companion;
		if (Subsystem && Subsystem->TryGetPermanentCompanionView(ActiveCharacterId, Companion))
		{
			ActiveLevel = Companion.Level;
		}
	}
	if (bHasActiveLoadout && ActiveCharacterId != FGameXXKEquipmentRules::HeroCharacterId())
	{
		ActiveHP = ActiveLoadoutSnapshot.AttributesBeforeRoute.MaxHealth;
		ActiveMaxHP = ActiveLoadoutSnapshot.AttributesBeforeRoute.MaxHealth;
		ActiveMP = ActiveLoadoutSnapshot.AttributesBeforeRoute.MaxMana;
		ActiveMaxMP = ActiveLoadoutSnapshot.AttributesBeforeRoute.MaxMana;
	}
	const FString IdentityLabel = RuntimeState && bHasActiveLoadout
		? FString::Printf(
			TEXT("角色 / 伙伴 · %s\nLv.%d  HP %d/%d  MP %d/%d\n攻击 %d  防御 %d\n六装备槽 · 角色与伙伴在背包内部切换"),
			*BackpackCharacterDisplayName(Subsystem, ActiveCharacterId),
			ActiveLevel,
			ActiveHP,
			ActiveMaxHP,
			ActiveMP,
			ActiveMaxMP,
			ActiveLoadoutSnapshot.AttributesBeforeRoute.Attack,
			ActiveLoadoutSnapshot.AttributesBeforeRoute.Defense)
		: TEXT("角色 / 伙伴\n等待存档\n六装备槽 · 角色与伙伴在背包内部切换");
	UTextBlock* Identity = MakeText(WidgetTree, FText::FromString(IdentityLabel), 16, FLinearColor::White);
	AddCanvas(RootCanvas, Identity, FVector2D(720.0f, 250.0f), FVector2D(300.0f, 100.0f));
	const TArray<FName> VisibleInventoryItems = RuntimeState ? SortedVisibleInventoryItemIds(*RuntimeState) : TArray<FName>();
	for (int32 SlotIndex = 0; SlotIndex < 20; ++SlotIndex)
	{
		const int32 Column = SlotIndex % 4;
		const int32 Row = SlotIndex / 4;
		UBorder* Cell = MakeSlotPanel(WidgetTree, ItemSlotTexturePath, FLinearColor(0.06f, 0.05f, 0.04f, 1.0f), FVector2D(105.0f, 56.0f));
		AddCanvas(RootCanvas, Cell, FVector2D(700.0f + Column * 118.0f, 450.0f + Row * 66.0f), FVector2D(105.0f, 56.0f));
		if (VisibleInventoryItems.IsValidIndex(SlotIndex) && RuntimeState)
		{
			const FName ItemId = VisibleInventoryItems[SlotIndex];
			const FString ItemLabel = FString::Printf(
				TEXT("%s\nx%d"),
				*ItemDisplayName(ItemId),
				RuntimeState->Inventory.FindRef(ItemId));
			UTextBlock* ItemText = MakeText(WidgetTree, FText::FromString(ItemLabel), 12, FLinearColor::White);
			AddCanvas(RootCanvas, ItemText, FVector2D(704.0f + Column * 118.0f, 456.0f + Row * 66.0f), FVector2D(98.0f, 44.0f));
			Cell->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s\n数量：%d\n物品 ID：%s"),
				*ItemDisplayName(ItemId),
				RuntimeState->Inventory.FindRef(ItemId),
				*ItemId.ToString())));
		}
	}
	UGameXXKDesktopTrainingActionButton* Sort = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Sort->Configure(this, 5);
	Sort->SetBackgroundColor(Accent);
	Sort->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("排序")), 18));
	AddCanvas(RootCanvas, Sort, FVector2D(1120.0f, 840.0f), FVector2D(150.0f, 54.0f));
	ActionButtons.Add(Sort);
	UTextBlock* Ratio = MakeText(WidgetTree, FText::FromString(FString::Printf(
		TEXT("背包比例锁定：1.76 : 1  ·  4 × 5 可视格  ·  %d 类物品"),
		VisibleInventoryItems.Num())), 16, FLinearColor(0.78f, 0.70f, 0.60f, 1.0f));
	AddCanvas(RootCanvas, Ratio, FVector2D(400.0f, 925.0f), FVector2D(460.0f, 30.0f));
	if (bSettingsPanelOpen)
	{
		UBorder* SettingsPanel = MakePanel(WidgetTree, FLinearColor(0.08f, 0.06f, 0.05f, 0.98f));
		AddCanvas(RootCanvas, SettingsPanel, FVector2D(705.0f, 320.0f), FVector2D(560.0f, 360.0f));
		UTextBlock* SettingsTitle = MakeText(WidgetTree, FText::FromString(TEXT("设置")), 26, Gold);
		AddCanvas(RootCanvas, SettingsTitle, FVector2D(745.0f, 350.0f), FVector2D(460.0f, 42.0f));
		UTextBlock* SettingsText = MakeText(
			WidgetTree,
			FText::FromString(TEXT("工作台设置入口已独立于关闭按钮\n\n默认 3D 城镇回退：保持开启\n静置帧率与窗口尺寸：沿用项目配置\n\n设置数据接入 RuntimeState 后在此处扩展。")),
			18,
			FLinearColor::White);
		AddCanvas(RootCanvas, SettingsText, FVector2D(745.0f, 410.0f), FVector2D(470.0f, 210.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTalentsPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(365.0f, 150.0f), FVector2D(960.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("天赋  ·  天赋树 / 称号")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(400.0f, 175.0f), FVector2D(700.0f, 46.0f));
	UTextBlock* Notice = MakeText(
		WidgetTree,
		FText::FromString(TEXT("天赋和称号集中在此页；真实节点数据与宝箱掉率加成尚未接入。")),
		18,
		FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
	AddCanvas(RootCanvas, Notice, FVector2D(405.0f, 235.0f), FVector2D(760.0f, 42.0f));
	for (int32 NodeIndex = 0; NodeIndex < 12; ++NodeIndex)
	{
		UBorder* Node = MakePanel(WidgetTree, NodeIndex == 0 ? Accent : Panel);
		AddCanvas(
			RootCanvas,
			Node,
			FVector2D(430.0f + (NodeIndex % 4) * 190.0f, 320.0f + (NodeIndex / 4) * 130.0f),
			FVector2D(150.0f, 92.0f));
		UTextBlock* NodeText = MakeText(
			WidgetTree,
			FText::FromString(NodeIndex == 0 ? TEXT("基础天赋\n待配置") : FString::Printf(TEXT("节点 %02d\n锁定"), NodeIndex + 1)),
			16,
			FLinearColor::White);
		AddCanvas(
			RootCanvas,
			NodeText,
			FVector2D(442.0f + (NodeIndex % 4) * 190.0f, 342.0f + (NodeIndex / 4) * 130.0f),
			FVector2D(126.0f, 54.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildToolsPanel()
{
	UBorder* PanelBorder = MakePanel(WidgetTree, Panel);
	AddCanvas(RootCanvas, PanelBorder, FVector2D(1340.0f, 150.0f), FVector2D(556.0f, 840.0f));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("工具  ·  魔方 / 合成 / 制作")), 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1370.0f, 175.0f), FVector2D(480.0f, 48.0f));
	const TArray<FText> ToolLabels = {
		FText::FromString(TEXT("魔方")),
		FText::FromString(TEXT("合成")),
		FText::FromString(TEXT("制作"))};
	for (int32 ToolIndex = 0; ToolIndex < ToolLabels.Num(); ++ToolIndex)
	{
		UGameXXKDesktopTrainingActionButton* ToolButton = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		ToolButton->Configure(this, 30 + ToolIndex);
		ToolButton->SetBackgroundColor(ToolIndex == 0 ? Accent : PanelAlt);
		ToolButton->SetContent(MakeText(WidgetTree, ToolLabels[ToolIndex], 22));
		AddCanvas(RootCanvas, ToolButton, FVector2D(1375.0f + ToolIndex * 175.0f, 245.0f), FVector2D(155.0f, 58.0f));
		ActionButtons.Add(ToolButton);
	}
	UTextBlock* Hint = MakeText(
		WidgetTree,
		FText::FromString(TEXT("工具容器替换右侧历练地图；强化、洗炼、分解后续只从这里进入。")),
		18,
		FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
	AddCanvas(RootCanvas, Hint, FVector2D(1375.0f, 345.0f), FVector2D(480.0f, 72.0f));
	UBorder* Queue = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Queue, FVector2D(1375.0f, 455.0f), FVector2D(480.0f, 270.0f));
	UTextBlock* QueueText = MakeText(
		WidgetTree,
		FText::FromString(TEXT("制作队列\n\n当前没有进行中的制作\n\n材料与配方将从 RuntimeState 读取")),
		18,
		FLinearColor::White);
	AddCanvas(RootCanvas, QueueText, FVector2D(1400.0f, 485.0f), FVector2D(420.0f, 190.0f));
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildTrainingMapPanel(const bool bReadOnly)
{
	UBorder* Map = MakePanel(WidgetTree, Panel);
	AddCanvas(RootCanvas, Map, FVector2D(1340.0f, 150.0f), FVector2D(556.0f, 840.0f));
	const FText MapTitle = bReadOnly
		? FText::FromString(TEXT("历练地图  ·  挑战只读"))
		: (ActiveNav == EGameXXKDesktopTrainingNav::Tools ? FText::FromString(TEXT("工具替换右侧地图")) : FText::FromString(TEXT("历练地图")));
	UTextBlock* Title = MakeText(WidgetTree, MapTitle, 30, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(1370.0f, 175.0f), FVector2D(450.0f, 48.0f));
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
		if (bReadOnly)
		{
			UBorder* DifficultyTab = MakeSlotPanel(
				WidgetTree,
				Difficulties[DifficultyIndex] == ActiveDifficulty ? TabSelectedTexturePath : TabNormalTexturePath,
				Difficulties[DifficultyIndex] == ActiveDifficulty ? Accent : PanelAlt,
				FVector2D(135.0f, 42.0f));
			DifficultyTab->SetContent(MakeText(WidgetTree, FText::FromString(Label), 18));
			AddCanvas(RootCanvas, DifficultyTab, FVector2D(1380.0f + DifficultyIndex * 155.0f, 225.0f), FVector2D(135.0f, 42.0f));
		}
		else
		{
			UGameXXKDesktopTrainingActionButton* DifficultyTab = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
			DifficultyTab->Configure(this, 11 + DifficultyIndex);
			DifficultyTab->SetStyle(MakeTextureButtonStyle(
				Difficulties[DifficultyIndex] == ActiveDifficulty ? TabSelectedTexturePath : TabNormalTexturePath,
				FVector2D(135.0f, 42.0f),
				FMargin(0.08f)));
			DifficultyTab->SetBackgroundColor(Difficulties[DifficultyIndex] == ActiveDifficulty ? Accent : PanelAlt);
			DifficultyTab->SetContent(MakeText(WidgetTree, FText::FromString(Label), 18));
			AddCanvas(RootCanvas, DifficultyTab, FVector2D(1380.0f + DifficultyIndex * 155.0f, 225.0f), FVector2D(135.0f, 42.0f));
			ActionButtons.Add(DifficultyTab);
		}
	}
	for (const FGameXXKTrainingStageDefinition& Definition : Definitions)
	{
		if (Definition.Difficulty != ActiveDifficulty)
		{
			continue;
		}
		const int32 LocalIndex = Definition.StageNumber - 1;
		const FVector2D NodePosition(1380.0f + (LocalIndex % 3) * 160.0f, 300.0f + (LocalIndex / 3) * 78.0f);
		const FText NodeLabel = FText::FromString(FString::Printf(TEXT("%d-%d"), Definition.Chapter, ((Definition.StageNumber - 1) % 3) + 1));
		const FText NodeTooltip = Subsystem ? Subsystem->BuildTrainingStageTooltip(Definition.StageId) : FText::GetEmpty();
		if (bReadOnly)
		{
			UBorder* Node = MakeSlotPanel(
				WidgetTree,
				RouteNodeTexturePath,
				Definition.StageId == SelectedStageId ? Gold : FLinearColor(0.35f, 0.25f, 0.13f, 1.0f),
				FVector2D(76.0f, 76.0f));
			Node->SetContent(MakeText(WidgetTree, NodeLabel, 18, Ink));
			Node->SetToolTipText(FText::FromString(NodeTooltip.ToString() + TEXT("\n挑战中只读")));
			AddCanvas(RootCanvas, Node, NodePosition, FVector2D(76.0f, 76.0f));
		}
		else
		{
			UGameXXKDesktopTrainingStageButton* Node = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingStageButton>(UGameXXKDesktopTrainingStageButton::StaticClass());
			Node->Configure(this, Definition.StageId);
			Node->SetStyle(MakeTextureButtonStyle(
				RouteNodeTexturePath,
				FVector2D(76.0f, 76.0f),
				FMargin(0.08f),
				Definition.StageId == SelectedStageId ? Gold : FLinearColor::White));
			Node->SetBackgroundColor(Definition.StageId == SelectedStageId ? Gold : FLinearColor(0.35f, 0.25f, 0.13f, 1.0f));
			Node->SetToolTipText(NodeTooltip);
			Node->SetContent(MakeText(WidgetTree, NodeLabel, 18, Ink));
			AddCanvas(RootCanvas, Node, NodePosition, FVector2D(76.0f, 76.0f));
			StageButtons.Add(Node);
		}
	}
	TravelStageText = MakeText(WidgetTree, FText::FromString(TEXT("当前游历关卡：未选择")), 20, FLinearColor::White);
	if (Subsystem)
	{
		const FName Current = Subsystem->GetTrainingProgressCopy().CurrentTravelStageId;
		TravelStageText->SetText(FText::FromString(FString::Printf(TEXT("当前游历关卡：%s"), *Current.ToString())));
	}
	AddCanvas(RootCanvas, TravelStageText.Get(), FVector2D(1375.0f, 735.0f), FVector2D(480.0f, 46.0f));
	if (bReadOnly)
	{
		UTextBlock* ReadOnlyNotice = MakeText(WidgetTree, FText::FromString(TEXT("挑战进行中\n右侧历练地图只读")), 18, FLinearColor(0.82f, 0.74f, 0.62f, 1.0f));
		AddCanvas(RootCanvas, ReadOnlyNotice, FVector2D(1375.0f, 820.0f), FVector2D(300.0f, 58.0f));
	}
	else
	{
		UGameXXKDesktopTrainingActionButton* Challenge = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Challenge->Configure(this, 6);
		Challenge->SetBackgroundColor(Accent);
		Challenge->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("挑战")), 24));
		if (Subsystem)
		{
			const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
			const bool bCanChallenge = FGameXXKTrainingRules::CanChallenge(Progress, SelectedStageId);
			Challenge->SetIsEnabled(bCanChallenge);
			if (!bCanChallenge && FGameXXKTrainingRules::AreAllStagesCleared(Progress))
			{
				Challenge->SetToolTipText(FText::FromString(TEXT("挑战按钮已完成；期待新内容")));
			}
			else if (!bCanChallenge && FGameXXKTrainingRules::IsStageCleared(Progress, SelectedStageId))
			{
				Challenge->SetToolTipText(FText::FromString(TEXT("本关已通关，请选择其他未通关关卡")));
			}
			else if (!bCanChallenge)
			{
				Challenge->SetToolTipText(FText::FromString(TEXT("需要先完成前置关卡或解锁当前难度")));
			}
		}
		AddCanvas(RootCanvas, Challenge, FVector2D(1380.0f, 820.0f), FVector2D(175.0f, 64.0f));
		ActionButtons.Add(Challenge);
		UGameXXKDesktopTrainingActionButton* Travel = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Travel->Configure(this, 7);
		Travel->SetBackgroundColor(FLinearColor(0.28f, 0.20f, 0.12f, 1.0f));
		Travel->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("游历")), 24));
		AddCanvas(RootCanvas, Travel, FVector2D(1580.0f, 820.0f), FVector2D(175.0f, 64.0f));
		ActionButtons.Add(Travel);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildChallengeViewport()
{
	UBorder* Viewport = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Viewport, FVector2D(ChallengeViewportRect.X, ChallengeViewportRect.Y), FVector2D(ChallengeViewportRect.Z, ChallengeViewportRect.W));
	UTextBlock* Title = MakeText(WidgetTree, FText::FromString(TEXT("挑战路线 / 局内战斗")), 32, Gold);
	AddCanvas(RootCanvas, Title, FVector2D(405.0f, 55.0f), FVector2D(600.0f, 48.0f));
	ChallengeStatusText = MakeText(WidgetTree, FText::FromString(TEXT("路线加载中：普通怪 → 次级精英 → 首领")), 22, FLinearColor::White);
	AddCanvas(RootCanvas, ChallengeStatusText.Get(), FVector2D(405.0f, 115.0f), FVector2D(800.0f, 44.0f));
	BuildChallengeCombatStrip();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem)
	{
		const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = Subsystem->GetTrainingEncounterSequence(Progress.ActiveChallengeStageId);
		if (Subsystem->IsTrainingChallengeBattleActive())
		{
			if (!ChallengeBattleBoard)
			{
				ChallengeBattleBoard = WidgetTree->ConstructWidget<UGameXXKBattleBoardWidget>(UGameXXKBattleBoardWidget::StaticClass());
			}
			if (ChallengeBattleBoard)
			{
				ChallengeBattleBoard->SetMVPSubsystem(const_cast<UGameXXKMVPSubsystem*>(Subsystem));
				AddCanvas(RootCanvas, ChallengeBattleBoard.Get(), FVector2D(ChallengeBattleBoardRect.X, ChallengeBattleBoardRect.Y), FVector2D(ChallengeBattleBoardRect.Z, ChallengeBattleBoardRect.W));
				ChallengeBattleBoard->SetVisibility(ESlateVisibility::Visible);
				ChallengeBattleVisualSessionToken = 1;
				ChallengeBattleBoard->BeginBattleVisualSession(ChallengeBattleVisualSessionToken);
				ChallengeBattleBoard->RefreshFromState();
			}
		}
		for (int32 Index = 0; Index < Encounters.Num(); ++Index)
		{
			const bool bActive = Index == Progress.ActiveChallengeEncounterIndex;
			UTextBlock* Encounter = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%s %s"), bActive ? TEXT("▶") : TEXT("○"), *Encounters[Index].DisplayName.ToString())), 20, bActive ? Gold : FLinearColor(0.75f, 0.70f, 0.62f, 1.0f));
			AddCanvas(RootCanvas, Encounter, FVector2D(1115.0f + (Index % 2) * 145.0f, 230.0f + (Index / 2) * 56.0f), FVector2D(135.0f, 40.0f));
		}
	}
	UGameXXKDesktopTrainingActionButton* Auto = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Auto->Configure(this, 8);
	Auto->SetBackgroundColor(Accent);
	Auto->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("自动战斗")), 22));
	AddCanvas(RootCanvas, Auto, FVector2D(450.0f, 780.0f), FVector2D(190.0f, 62.0f));
	ActionButtons.Add(Auto);
	UGameXXKDesktopTrainingActionButton* Advance = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
	Advance->Configure(this, 9);
	Advance->SetBackgroundColor(FLinearColor(0.25f, 0.18f, 0.11f, 1.0f));
	Advance->SetContent(MakeText(WidgetTree, FText::FromString(TEXT("击败当前遭遇")), 22));
	AddCanvas(RootCanvas, Advance, FVector2D(675.0f, 780.0f), FVector2D(260.0f, 62.0f));
	ActionButtons.Add(Advance);
	UTextBlock* Hint = MakeText(WidgetTree, FText::FromString(TEXT("自动战斗只在挑战局内显示；游历条仅提供失败重试。")), 18, FLinearColor(0.78f, 0.70f, 0.60f, 1.0f));
	AddCanvas(RootCanvas, Hint, FVector2D(450.0f, 880.0f), FVector2D(700.0f, 36.0f));
	}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildChallengeCombatStrip()
{
	UBorder* Strip = MakePanel(WidgetTree, PanelAlt);
	AddCanvas(RootCanvas, Strip, FVector2D(ChallengeCombatStripRect.X, ChallengeCombatStripRect.Y), FVector2D(ChallengeCombatStripRect.Z, ChallengeCombatStripRect.W));
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FString EnemyName = TEXT("等待");
	FString BattleState = TEXT("战斗准备");
	if (Subsystem)
	{
		const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
		const FName StageId = Progress.ActiveChallengeStageId.IsNone() ? SelectedStageId : Progress.ActiveChallengeStageId;
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = Subsystem->GetTrainingEncounterSequence(StageId);
		if (Encounters.IsValidIndex(Progress.ActiveChallengeEncounterIndex))
		{
			EnemyName = Encounters[Progress.ActiveChallengeEncounterIndex].DisplayName.ToString();
		}
		BattleState = Subsystem->IsTrainingChallengeBattleActive() ? TEXT("自动战斗") : TEXT("路线结算");
	}
	UTextBlock* EnemySide = MakeText(WidgetTree, FText::FromString(TEXT("敌方")), 15, FLinearColor(1.0f, 0.68f, 0.55f, 1.0f));
	AddCanvas(RootCanvas, EnemySide, FVector2D(420.0f, 148.0f), FVector2D(70.0f, 24.0f));
	UTextBlock* StateText = MakeText(
		WidgetTree,
		FText::FromString(FString::Printf(TEXT("挑战画布 · 3 敌 / 3 我 · %s · %s"), *EnemyName, *BattleState)),
		15,
		Gold);
	AddCanvas(RootCanvas, StateText, FVector2D(620.0f, 148.0f), FVector2D(480.0f, 24.0f));
	UTextBlock* PartySide = MakeText(WidgetTree, FText::FromString(TEXT("我方")), 15, FLinearColor(0.58f, 0.86f, 1.0f, 1.0f));
	AddCanvas(RootCanvas, PartySide, FVector2D(1190.0f, 148.0f), FVector2D(70.0f, 24.0f));

	const FVector2D SlotSize(64.0f, 64.0f);
	const FVector2D SlotPositions[ChallengeCombatSlotCount] = {
		FVector2D(435.0f, 162.0f), FVector2D(510.0f, 162.0f), FVector2D(585.0f, 162.0f),
		FVector2D(1030.0f, 162.0f), FVector2D(1105.0f, 162.0f), FVector2D(1180.0f, 162.0f)};
	const TArray<FName> CharacterIds = GetBackpackCharacterIdsForTest();
	for (int32 Index = 0; Index < ChallengeCombatSlotCount; ++Index)
	{
		UBorder* CellBorder = MakeSlotPanel(WidgetTree, ItemSlotTexturePath, PanelAlt, SlotSize);
		AddCanvas(RootCanvas, CellBorder, SlotPositions[Index], SlotSize);
		FString SlotLabel;
		if (Index < 3)
		{
			SlotLabel = Index == 0 ? EnemyName : TEXT("待机");
		}
		else if (CharacterIds.IsValidIndex(Index - 3))
		{
			SlotLabel = BackpackCharacterDisplayName(Subsystem, CharacterIds[Index - 3]);
		}
		else
		{
			SlotLabel = TEXT("待机");
		}
		UTextBlock* SlotText = MakeText(WidgetTree, FText::FromString(SlotLabel), 12, Ink);
		AddCanvas(RootCanvas, SlotText, SlotPositions[Index] + FVector2D(4.0f, 22.0f), FVector2D(56.0f, 32.0f));
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::BuildBottomNavigation()
{
	const TArray<EGameXXKDesktopTrainingNav> Navs = {
		EGameXXKDesktopTrainingNav::Warehouse,
		EGameXXKDesktopTrainingNav::Formation,
		EGameXXKDesktopTrainingNav::Talents,
		EGameXXKDesktopTrainingNav::Tools,
		EGameXXKDesktopTrainingNav::Training};
	for (int32 Index = 0; Index < Navs.Num(); ++Index)
	{
		UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(UGameXXKDesktopTrainingActionButton::StaticClass());
		Button->Configure(this, Index);
		Button->SetBackgroundColor(Navs[Index] == ActiveNav ? Accent : PanelAlt);
		Button->SetContent(MakeNavigationContent(WidgetTree, Navs[Index], Navs[Index] == ActiveNav));
		AddCanvas(RootCanvas, Button, FVector2D(365.0f + Index * 190.0f, 1000.0f), FVector2D(175.0f, 58.0f));
		ActionButtons.Add(Button);
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshLayout()
{
	const bool bWasInViewport = IsInViewport();
	const ESlateVisibility PreviousVisibility = GetVisibility();
	if (bWasInViewport)
	{
		// WidgetTree children are rebuilt for the workbench/challenge switch. A
		// live UUserWidget otherwise keeps the old Slate tree (for example the
		// idle travel strip remains painted over the challenge canvas). Detach
		// and release the cached Slate resource before attaching the new tree.
		RemoveFromParent();
		ReleaseSlateResources(true);
	}
	BuildProgrammaticLayout();
	if (bWasInViewport)
	{
		AddToViewport(200);
		// Reattaching a live UUserWidget can restore the Slate tree with its
		// default collapsed visibility. Preserve the caller's visible/collapsed
		// state so switching from the workbench to the challenge viewport does
		// not silently remove the whole shell.
		SetVisibility(PreviousVisibility);
		if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::ApplyAction(const int32 ActionId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}
	if (ActionId >= 0 && ActionId <= 4)
	{
		if (ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
		{
			SetNotice(FText::FromString(TEXT("挑战进行中：左仓库与右侧导航暂时只读")));
			return;
		}
		ActiveNav = static_cast<EGameXXKDesktopTrainingNav>(ActionId);
		ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
		bSettingsPanelOpen = false;
		RefreshLayout();
		return;
	}
	if (ActionId >= 20 && ActionId < 20 + GetBackpackCharacterIdsForTest().Num())
	{
		SelectBackpackCharacterForTest(GetBackpackCharacterIdsForTest()[ActionId - 20]);
		return;
	}
	if (ActionId == 40)
	{
		PreviousWarehousePageForTest();
		return;
	}
	if (ActionId == 41)
	{
		NextWarehousePageForTest();
		return;
	}
	if (ActionId >= 100 && ActionId < 100 + WarehousePageSize)
	{
		QuickEquipVisibleWarehouseSlotForTest(ActionId - 100);
		return;
	}
	if (ActionId >= 200 && ActionId < 206)
	{
		QuickUnequipActiveBackpackSlotForTest(ActionId - 200);
		return;
	}
	if (ActionId >= 11 && ActionId <= 13)
	{
		const EGameXXKTrainingDifficulty Difficulty = static_cast<EGameXXKTrainingDifficulty>(ActionId - 11);
		SelectedStageId = FGameXXKTrainingRules::MakeStageId(Difficulty, 1);
		Subsystem->SelectTrainingStage(SelectedStageId);
		RefreshLayout();
		return;
	}
	switch (ActionId)
	{
	case 5:
		SortWarehouseForTest();
		break;
	case 6:
		if (Subsystem->StartTrainingChallenge(SelectedStageId))
		{
			ViewMode = EGameXXKDesktopTrainingViewMode::ChallengeViewport;
			bChallengeSidePanelsReadOnly = true;
			bSettingsPanelOpen = false;
			AutoBattleAccumulator = 0.0f;
			RefreshLayout();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("该关卡尚未解锁或已经通关")));
		}
		break;
	case 7:
		if (Subsystem->StartTrainingTravel(SelectedStageId))
		{
			ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
			bChallengeSidePanelsReadOnly = false;
			SetNotice(FText::FromString(TEXT("开始游历：走动、遭遇、自动战斗、结算后循环")));
			RefreshLayout();
		}
		else
		{
			SetNotice(FText::FromString(TEXT("未通关关卡不能游历")));
		}
		break;
	case 8:
		if (ViewMode == EGameXXKDesktopTrainingViewMode::ChallengeViewport)
		{
			const bool bAuto = !Subsystem->GetTrainingProgressCopy().bChallengeAutoBattle;
			Subsystem->SetTrainingChallengeAutoBattle(bAuto);
			SetNotice(bAuto ? FText::FromString(TEXT("自动战斗已开启")) : FText::FromString(TEXT("自动战斗已暂停")));
			RefreshLayout();
		}
		break;
	case 9:
		AdvanceChallengeForTest();
		break;
	case 10:
		Subsystem->SetTrainingRetryOnFailure(!Subsystem->GetTrainingProgressCopy().bRetryOnFailure);
		SetNotice(FText::FromString(TEXT("已切换游历失败重试策略")));
		break;
	case 16:
		CollectTravelRewardsForTest();
		break;
	case 14:
		bSettingsPanelOpen = !bSettingsPanelOpen;
		SetNotice(bSettingsPanelOpen
			? FText::FromString(TEXT("设置面板已打开；关闭按钮保持独立"))
			: FText::FromString(TEXT("设置面板已收起")));
		RefreshLayout();
		break;
	case 15:
		CloseWorkbench();
		break;
	case 30:
	case 31:
	case 32:
		SetNotice(FText::FromString(TEXT("工具容器已打开：具体配方/材料读取待接入")));
		break;
	default:
		break;
	}
}

void UGameXXKDesktopTrainingWorkbenchWidget::SetNotice(const FText& Notice)
{
	LastNotice = Notice;
	if (NoticeText)
	{
		NoticeText->SetText(Notice);
	}
}
