#include "UI/GameXXKCompanionRosterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardText.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKMVPRules.h"
#include "InputCoreTypes.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKPartyDeckUiStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"

namespace
{
	class SGameXXKCompanionEquipmentSlotButton final : public SButton
	{
	public:
		using FArguments = SButton::FArguments;

		void Construct(const FArguments& InArgs, UGameXXKCompanionEquipmentSlotButton* InOwner)
		{
			Owner = InOwner;
			SButton::Construct(InArgs);
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton
				&& Owner.IsValid()
				&& Owner->HandleRightMouseButtonDown())
			{
				return FReply::Handled();
			}
			return SButton::OnMouseButtonDown(MyGeometry, MouseEvent);
		}

	private:
		TWeakObjectPtr<UGameXXKCompanionEquipmentSlotButton> Owner;
	};

	constexpr int32 RosterSlotCount = 12;
	constexpr int32 RosterColumnCount = 3;
	constexpr int32 RosterPageSize = 3;
	constexpr int32 RosterPageCount = 4;
	constexpr int32 PersonalCardColumnCount = 3;
	constexpr int32 EquipmentBackpackColumnCount = 4;
	constexpr int32 EquipmentBackpackViewportSlotCount = 20;
	constexpr int32 EquipmentBackpackStorageCapacity = FGameXXKEquipmentRules::WarehouseCapacity;

	// Master V1 page 18 (companion backpack) absolute geometry. The shell matches
	// page 03; the right side hosts the attributes text, the 20-cell warehouse
	// window, or the 3x3 personal-card grid depending on the active tab.
	const FVector2D CompanionPaperPos(311.0f, 173.0f);
	const FVector2D CompanionPaperSize(1450.0f, 849.0f);
	const FVector2D CompanionTabSize(105.0f, 62.0f);
	const FVector2D CompanionTabPositions[5] = {
		FVector2D(514.0f, 220.0f), FVector2D(639.0f, 219.0f), FVector2D(764.0f, 220.0f),
		FVector2D(889.0f, 220.0f), FVector2D(1019.0f, 221.0f)};
	const FVector2D CompanionCloseButtonSize(74.0f, 74.0f);
	const FVector2D CompanionEquipmentFramePositions[6] = {
		FVector2D(420.0f, 340.0f), FVector2D(420.0f, 515.0f), FVector2D(420.0f, 690.0f),
		FVector2D(930.0f, 340.0f), FVector2D(930.0f, 515.0f), FVector2D(930.0f, 690.0f)};
	const FVector2D CompanionEquipmentSlotSize(118.0f, 124.0f);
	const FVector2D CompanionCenterPortraitPos(478.0f, 304.0f);
	const FVector2D CompanionCenterPortraitSize(518.0f, 518.0f);
	const FVector2D CompanionCenterNamePos(637.0f, 290.0f);
	const FVector2D CompanionCenterNameSize(200.0f, 32.0f);
	const FVector2D CompanionBackpackViewportPos(1135.0f, 300.0f);
	const FVector2D CompanionBackpackViewportSize(488.0f, 650.0f);
	const FVector2D CompanionBackpackSlotPitch(122.0f, 130.0f);
	const FMargin CompanionBackpackSlotPadding(6.0f, 7.0f);
	const FVector2D CompanionBackpackContentOffset(-6.0f, -7.0f);
	// Exactly five rows (20 cells) tall: the warehouse window shows the full
	// 4x5 viewport with no scroll gap below the cells.
	const FVector2D CompanionBackpackGridSize(488.0f, 650.0f);
	const FVector2D CompanionWarehouseSlotSize(110.0f, 116.0f);
	const FVector2D CompanionFilterRowPos(1142.0f, 240.0f);
	const FVector2D CompanionFilterRowSize(80.0f, 26.0f);
	const float CompanionFilterRowPitch = 80.0f;
	const FVector2D CompanionScrollbarTrackPos(1647.0f, 303.0f);
	const FVector2D CompanionScrollbarTrackSize(19.0f, 633.0f);
	const FVector2D CompanionScrollbarThumbTop(1642.0f, 323.0f);
	const FVector2D CompanionScrollbarThumbSize(30.0f, 126.0f);
	const FVector2D CompanionSelectionInkPos(1128.0f, 284.0f);
	const FVector2D CompanionSelectionInkSize(126.0f, 42.0f);
	// Hero deck-tab geometry mirrored for the partner card grid (panel padding 24,20).
	const FVector2D CompanionDeckScrollPos(0.0f, 34.0f);
	const FVector2D CompanionDeckScrollSize(470.0f, 500.0f);
	const FVector2D CompanionDeckApplyPos(175.0f, 550.0f);
	const FVector2D CompanionDeckApplySize(120.0f, 42.0f);
	const FVector2D CompanionDeckCountPos(175.0f, 596.0f);
	const FVector2D CompanionDeckCountSize(120.0f, 22.0f);
	const FVector2D PersonalCardSize(137.0f, 190.0f);
	const FVector2D CompanionCardPortraitPos(5.0f, 6.0f);
	const FVector2D CompanionCardPortraitSize(127.0f, 152.0f);
	const FVector2D CompanionCardGridSize(441.0f, 800.0f);
	const FMargin CompanionCardSlotPadding(5.0f, 5.0f);
	const FVector2D CompanionBackpackIconSize(64.0f, 64.0f);
	const FVector2D CompanionTooltipPaperSize(260.0f, 120.0f);
	const FMargin CompanionTooltipPadding(16.0f, 12.0f, 16.0f, 12.0f);
	const FVector2D CompanionRosterSlotSize(105.0f, 62.0f);
	const FVector2D CompanionRosterSlotPositions[RosterPageSize] = {
		FVector2D(575.0f, 793.0f), FVector2D(677.0f, 793.0f), FVector2D(779.0f, 793.0f)};
	const FVector2D CompanionRosterPageArrowSize(36.0f, 62.0f);
	const FVector2D CompanionRosterPageLeftPos(540.0f, 793.0f);
	const FVector2D CompanionRosterPageRightPos(883.0f, 793.0f);
	const FVector2D CompanionRosterCountPos(640.0f, 858.0f);
	const FVector2D CompanionRosterCountSize(290.0f, 24.0f);
	const FVector2D CompanionDismissButtonPos(935.0f, 878.0f);
	const FVector2D CompanionDismissButtonSize(105.0f, 62.0f);
	const FVector2D CompanionDeckCaptionPos(1142.0f, 240.0f);
	const FVector2D CompanionDeckCaptionSize(300.0f, 26.0f);
	const FVector2D CardTooltipSize(402.0f, 244.0f);
	const float CardFaceScale = PersonalCardSize.X / 113.0f;
	const FMargin SlotFrameMargin(5.0f / 61.0f, 5.0f / 56.0f, 5.0f / 61.0f, 5.0f / 56.0f);
	const FMargin ActionFrameMargin(5.0f / 73.0f, 5.0f / 31.0f, 5.0f / 73.0f, 5.0f / 31.0f);

	static constexpr const TCHAR* ApprovedTextureRoot = TEXT("/Game/GameXXK/UI/MasterV2/Approved/");
	static constexpr const TCHAR* WindowFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	static constexpr const TCHAR* PanelFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	// Page 18 avatar-slot base uses the approved tab paper (PSD 005_tab_3; only 003_tab_1 is exported).
	static constexpr const TCHAR* RosterSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1.003_tab_1");
	static constexpr const TCHAR* RosterPageLeftTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CompanionPageLeft.T_MasterV2_CompanionPageLeft");
	static constexpr const TCHAR* RosterPageRightTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CompanionPageRight.T_MasterV2_CompanionPageRight");
	static constexpr const TCHAR* ActionButtonTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonPrimary.T_MasterV2_ButtonPrimary");
	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame");
	static constexpr const TCHAR* LockedCardIconTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardLockedIcon.T_MasterV2_CardLockedIcon");
	static constexpr const TCHAR* ItemSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	static constexpr const TCHAR* EquipmentSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_EquipmentSlot.T_MasterV2_EquipmentSlot");
	// User-exported original tab filenames (page 03 convention).
	static constexpr const TCHAR* TabNormalTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/003_tab_1.003_tab_1");
	static constexpr const TCHAR* TabSelectedTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/004_tab_2.004_tab_2");
	// Same paper as the hero backpack tooltips (approved item-slot paper).
	static constexpr const TCHAR* TooltipPaperTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	static constexpr const TCHAR* CloseButtonTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk");
	// Page 03/18 scrollbar and selection ink.
	static constexpr const TCHAR* ScrollbarTrackTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.T_MasterV2_BackpackScrollbarRight");
	static constexpr const TCHAR* ScrollbarThumbTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/inventory_scrollbar_Button.inventory_scrollbar_Button");
	static constexpr const TCHAR* SelectionInkTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SelectionInk.T_MasterV2_SelectionInk");
	// Page 18 dismiss action reuses the approved decompose paper (01_DecomposeButton).
	static constexpr const TCHAR* DismissButtonTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/01_DecomposeButton.01_DecomposeButton");
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

	const TArray<EGameXXKEquipmentSlot>& GetCompanionEquipmentSlotOrder()
	{
		static const TArray<EGameXXKEquipmentSlot> Slots = {
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKEquipmentSlot::Head,
			EGameXXKEquipmentSlot::Armor,
			EGameXXKEquipmentSlot::Belt,
			EGameXXKEquipmentSlot::Shoes,
			EGameXXKEquipmentSlot::Accessory};
		return Slots;
	}

	FText GetCompanionEquipmentSlotText(const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentWeapon", "武器");
		case EGameXXKEquipmentSlot::Head: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentHead", "头部");
		case EGameXXKEquipmentSlot::Armor: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentArmor", "衣甲");
		case EGameXXKEquipmentSlot::Belt: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentBelt", "腰带");
		case EGameXXKEquipmentSlot::Shoes: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentShoes", "鞋");
		case EGameXXKEquipmentSlot::Accessory: return NSLOCTEXT("GameXXKCompanionRoster", "EquipmentAccessory", "饰品");
		default: return FText::GetEmpty();
		}
	}

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return Path ? LoadObject<UTexture2D>(nullptr, Path) : nullptr;
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

	FSlateBrush MakeBoxTextureBrush(const TCHAR* Path, const FVector2D& ImageSize, const FMargin& Margin = FMargin(0.065f))
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	FButtonStyle MakeBoxTextureButtonStyle(const TCHAR* Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FButtonStyle Style;
		FSlateBrush Brush = MakeBoxTextureBrush(Path, ImageSize, Margin);
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	FButtonStyle MakeTextureButtonStyle(const TCHAR* Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		const FSlateBrush Normal = MakeTextureBrush(Path, ImageSize);
		Style.SetNormal(Normal);
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, FLinearColor(1.10f, 1.10f, 1.10f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, FLinearColor(0.78f, 0.78f, 0.78f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.42f, 0.42f, 0.42f, 0.55f)));
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

	FButtonStyle MakeCardButtonStyle()
	{
		FButtonStyle Style;
		const FSlateBrush FrameBrush = MakeTextureBrush(CardFrameTexturePath, PersonalCardSize);
		Style.SetNormal(FrameBrush);
		Style.SetHovered(FrameBrush);
		Style.SetPressed(FrameBrush);
		Style.SetDisabled(FrameBrush);
		return Style;
	}

	UTextBlock* MakeText(
		UWidgetTree* WidgetTree,
		const FText& Text,
		const int32 FontSize = 16,
		const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		const FName WidgetName = NAME_None)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), WidgetName);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		// Same convention as the hero backpack: no auto wrap by default so narrow
		// boxes never stack Chinese characters into a vertical column.
		TextBlock->SetAutoWrapText(false);
		TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FontSize));
		return TextBlock;
	}

	void AddCanvasChild(
		UCanvasPanel* Canvas,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f),
		const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (!Canvas || !Child)
		{
			return;
		}

		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	FText GetRoleDisplayName(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: return NSLOCTEXT("GameXXKCompanionRoster", "RoleBlade", "刀客");
		case EGameXXKCharacterRole::Guard: return NSLOCTEXT("GameXXKCompanionRoster", "RoleGuard", "护卫");
		case EGameXXKCharacterRole::Healer: return NSLOCTEXT("GameXXKCompanionRoster", "RoleHealer", "医师");
		case EGameXXKCharacterRole::Hunter: return NSLOCTEXT("GameXXKCompanionRoster", "RoleHunter", "猎手");
		case EGameXXKCharacterRole::Sorcerer: return NSLOCTEXT("GameXXKCompanionRoster", "RoleSorcerer", "术士");
		case EGameXXKCharacterRole::FormationMaster: return NSLOCTEXT("GameXXKCompanionRoster", "RoleFormationMaster", "阵师");
		default: return NSLOCTEXT("GameXXKCompanionRoster", "RoleUnknown", "未知职业");
		}
	}

	FString ResolveCompanionPortraitResourcePath(const EGameXXKCharacterRole Role, const bool bActive)
	{
		const TCHAR* Suffix = bActive ? TEXT("") : TEXT("Inactive");
		const TCHAR* RoleName = nullptr;
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: RoleName = TEXT("Blade"); break;
		case EGameXXKCharacterRole::Guard: RoleName = TEXT("Guard"); break;
		case EGameXXKCharacterRole::Healer: RoleName = TEXT("Healer"); break;
		case EGameXXKCharacterRole::Hunter: RoleName = TEXT("Hunter"); break;
		case EGameXXKCharacterRole::Sorcerer: RoleName = TEXT("Sorcerer"); break;
		case EGameXXKCharacterRole::FormationMaster: RoleName = TEXT("FormationMaster"); break;
		default: return FString();
		}
		const FString AssetName = FString::Printf(TEXT("T_MasterV2_Companion%s%s"), RoleName, Suffix);
		return FString::Printf(TEXT("%s%s.%s"), ApprovedTextureRoot, *AssetName, *AssetName);
	}

	FString ResolveCompanionFullBodyResourcePath(const EGameXXKCharacterRole Role)
	{
		const TCHAR* RoleName = nullptr;
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: RoleName = TEXT("Blade"); break;
		case EGameXXKCharacterRole::Guard: RoleName = TEXT("Guard"); break;
		case EGameXXKCharacterRole::Healer: RoleName = TEXT("Healer"); break;
		case EGameXXKCharacterRole::Hunter: RoleName = TEXT("Hunter"); break;
		case EGameXXKCharacterRole::Sorcerer: RoleName = TEXT("Sorcerer"); break;
		case EGameXXKCharacterRole::FormationMaster: RoleName = TEXT("FormationMaster"); break;
		default: return FString();
		}
		const FString AssetName = FString::Printf(TEXT("T_MasterV2_CompanionFullBody_%s"), RoleName);
		return FString::Printf(TEXT("%s%s.%s"), ApprovedTextureRoot, *AssetName, *AssetName);
	}

	FText ResolveCompanionDisplayName(const EGameXXKCharacterRole Role, const int32 NameSeed)
	{
		return FText::FromString(FGameXXKCompanionRules::GetCompanionDisplayName(Role, NameSeed));
	}

	// The partner warehouse window mirrors the hero backpack's content source:
	// shared warehouse equipment plus the player inventory, filtered by category.
	bool MatchesCompanionFilter(const EGameXXKItemKind Kind, const int32 FilterIndex)
	{
		switch (FilterIndex)
		{
		case 0: // 全部
			return true;
		case 1: // 装备
			return Kind == EGameXXKItemKind::Weapon || Kind == EGameXXKItemKind::Armor || Kind == EGameXXKItemKind::Accessory;
		case 2: // 道具
			return Kind == EGameXXKItemKind::Consumable;
		case 3: // 材料
			return Kind == EGameXXKItemKind::Material;
		case 4: // 任务
			return Kind == EGameXXKItemKind::Task;
		default:
			return false;
		}
	}

	FText CompanionItemKindText(const EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Consumable: return NSLOCTEXT("GameXXKCompanionRoster", "KindConsumable", "消耗");
		case EGameXXKItemKind::Weapon: return NSLOCTEXT("GameXXKCompanionRoster", "KindWeapon", "武器");
		case EGameXXKItemKind::Armor: return NSLOCTEXT("GameXXKCompanionRoster", "KindArmor", "防具");
		case EGameXXKItemKind::Accessory: return NSLOCTEXT("GameXXKCompanionRoster", "KindAccessory", "饰品");
		case EGameXXKItemKind::Material: return NSLOCTEXT("GameXXKCompanionRoster", "KindMaterial", "材料");
		case EGameXXKItemKind::Task: return NSLOCTEXT("GameXXKCompanionRoster", "KindTask", "任务");
		default: return NSLOCTEXT("GameXXKCompanionRoster", "KindUnknown", "物品");
		}
	}

	FString CompanionItemStatsText(const FGameXXKItemDef& Def, const int32 EnhancementLevel)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("类型：%s"), *CompanionItemKindText(Def.Kind).ToString()));
		if (Def.HealAmount > 0) { Lines.Add(FString::Printf(TEXT("恢复 HP +%d"), Def.HealAmount)); }
		if (Def.MPHealAmount > 0) { Lines.Add(FString::Printf(TEXT("恢复 MP +%d"), Def.MPHealAmount)); }
		if (Def.AttackBonus > 0) { Lines.Add(FString::Printf(TEXT("攻击 +%d"), Def.AttackBonus)); }
		if (Def.DefenseBonus > 0) { Lines.Add(FString::Printf(TEXT("防御 +%d"), Def.DefenseBonus)); }
		if (Def.MaxHPBonus > 0) { Lines.Add(FString::Printf(TEXT("生命上限 +%d"), Def.MaxHPBonus)); }
		if (Def.MaxMPBonus > 0) { Lines.Add(FString::Printf(TEXT("真气上限 +%d"), Def.MaxMPBonus)); }
		if (EnhancementLevel > 0)
		{
			Lines.Add(FString::Printf(TEXT("强化 +%d"), EnhancementLevel));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString CompanionResolveItemIconTexturePath(const FName ItemId)
	{
		const FString TextureRoot(TEXT("/Game/GameXXK/UI/Inventory/Textures/"));
		if (ItemId == UGameXXKMVPRules::ItemHealingPowder()) { return TextureRoot + TEXT("T_ItemHealingPowder.T_ItemHealingPowder"); }
		if (ItemId == UGameXXKMVPRules::ItemEnhancementStone()) { return TEXT("/Game/GameXXK/UI/Items/strengthening_stone.strengthening_stone"); }
		if (ItemId == UGameXXKMVPRules::ItemRefinementSand()) { return TEXT("/Game/GameXXK/UI/Items/refinement_sand.refinement_sand"); }
		if (ItemId == UGameXXKMVPRules::ItemQingshanRouteSeal()) { return TEXT("/Game/GameXXK/UI/Items/qingshan_suppression_token.qingshan_suppression_token"); }
		if (ItemId == FName(TEXT("Item.LingzhiPowder"))) { return TextureRoot + TEXT("T_ItemLingzhiPowder.T_ItemLingzhiPowder"); }
		if (ItemId == FName(TEXT("Item.QingxinTea"))) { return TextureRoot + TEXT("T_ItemQingxinTea.T_ItemQingxinTea"); }
		if (ItemId == FName(TEXT("Item.CraneSachet"))) { return TextureRoot + TEXT("T_ItemCraneSachet.T_ItemCraneSachet"); }
		if (ItemId == UGameXXKMVPRules::ItemIronSword()) { return TextureRoot + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword"); }
		if (ItemId == UGameXXKMVPRules::ItemClothArmor()) { return TextureRoot + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor"); }
		if (ItemId == FName(TEXT("Item.CranePatternTalisman"))) { return TextureRoot + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman"); }
		if (ItemId == FName(TEXT("Item.InkstonePendant"))) { return TextureRoot + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant"); }
		if (ItemId == UGameXXKMVPRules::ItemWoodenSword()) { return TextureRoot + TEXT("T_ItemWoodenSword.T_ItemWoodenSword"); }
		if (ItemId == UGameXXKMVPRules::ItemStarterClothArmor()) { return TextureRoot + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor"); }
		if (ItemId == UGameXXKMVPRules::ItemClothTalisman()) { return TextureRoot + TEXT("T_ItemClothTalisman.T_ItemClothTalisman"); }
		return FString();
	}

	FText CompanionEquipmentQualityText(const EGameXXKEquipmentQuality Quality)
	{
		switch (Quality)
		{
		case EGameXXKEquipmentQuality::Common: return NSLOCTEXT("GameXXKCompanionRoster", "QualityCommon", "普通");
		case EGameXXKEquipmentQuality::Rare: return NSLOCTEXT("GameXXKCompanionRoster", "QualityRare", "稀有");
		case EGameXXKEquipmentQuality::Epic: return NSLOCTEXT("GameXXKCompanionRoster", "QualityEpic", "珍稀");
		default: return NSLOCTEXT("GameXXKCompanionRoster", "QualityUnknown", "未知");
		}
	}

	FText BuildCompanionEquipmentInstanceDetail(
		const UGameXXKMVPSubsystem* Subsystem,
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Definition,
		const FName CharacterId)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("部位：%s"), *GetCompanionEquipmentSlotText(Definition.Slot).ToString()));
		Lines.Add(FString::Printf(TEXT("装备等级 %d"), Instance.ItemLevel));
		Lines.Add(FString::Printf(TEXT("品质：%s"), *CompanionEquipmentQualityText(Instance.Quality).ToString()));
		Lines.Add(FString::Printf(TEXT("强化 +%d"), Instance.EnhancementLevel));
		for (const FGameXXKEquipmentAffixRoll& Roll : Instance.RolledAffixes)
		{
			const FGameXXKAffixDefinition* Affix = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
			if (Affix)
			{
				if (Roll.Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints)
				{
					Lines.Add(FString::Printf(TEXT("%s +%.2f%%"), *Affix->DisplayName.ToString(), Roll.Magnitude / 100.0));
				}
				else
				{
					Lines.Add(FString::Printf(TEXT("%s +%d"), *Affix->DisplayName.ToString(), Roll.Magnitude));
				}
			}
		}

		FGameXXKEquipmentTooltipSnapshot Snapshot;
		const bool bHasSnapshot = Subsystem
			&& Subsystem->GetEquipmentTooltipSnapshot(Instance.InstanceId, CharacterId, Snapshot);

		// The 2/4/6-piece set bonus block marks each tier the character has reached.
		if (Definition.Set != EGameXXKEquipmentSet::Invalid && Definition.Set != EGameXXKEquipmentSet::Legacy)
		{
			const FText SetName = FGameXXKEquipmentSetCatalog::GetSetDisplayName(Definition.Set);
			if (!SetName.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("套装：%s"), *SetName.ToString()));
				const int32 CurrentPieceCount = bHasSnapshot ? Snapshot.CurrentSetPieceCounts.FindRef(Definition.Set) : 0;
				for (const FGameXXKEquipmentSetBonusDefinition& Bonus : FGameXXKEquipmentSetCatalog::GetDefinitions())
				{
					if (Bonus.Set != Definition.Set)
					{
						continue;
					}
					FString BonusLine = FString::Printf(TEXT("%d件：%s"), Bonus.RequiredPieces, *Bonus.Description.ToString());
					if (CurrentPieceCount >= Bonus.RequiredPieces)
					{
						BonusLine += TEXT("（已激活）");
					}
					Lines.Add(MoveTemp(BonusLine));
				}
			}
		}
		if (bHasSnapshot)
		{
			if (Snapshot.ItemCurrentStats.Attack != 0) { Lines.Add(FString::Printf(TEXT("攻击 %+d"), Snapshot.ItemCurrentStats.Attack)); }
			if (Snapshot.ItemCurrentStats.Defense != 0) { Lines.Add(FString::Printf(TEXT("防御 %+d"), Snapshot.ItemCurrentStats.Defense)); }
			if (Snapshot.ItemCurrentStats.MaxHealth != 0) { Lines.Add(FString::Printf(TEXT("气血 %+d"), Snapshot.ItemCurrentStats.MaxHealth)); }
			if (Snapshot.ItemCurrentStats.MaxMana != 0) { Lines.Add(FString::Printf(TEXT("真气 %+d"), Snapshot.ItemCurrentStats.MaxMana)); }
			if (Snapshot.ItemCurrentStats.Speed != 0) { Lines.Add(FString::Printf(TEXT("身法 %+d"), Snapshot.ItemCurrentStats.Speed)); }
		}
		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	FString ResolveCardPortraitResourcePath(const FGameXXKCardDefinition& Definition)
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

		return FString();
	}

	FString BuildCardSummary(const TArray<FName>& CardIds)
	{
		TArray<FString> DisplayNames;
		DisplayNames.Reserve(CardIds.Num());
		for (const FName CardId : CardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			DisplayNames.Add(Definition ? Definition->DisplayName.ToString() : CardId.ToString());
		}
		return FString::Join(DisplayNames, TEXT("、"));
	}

}

void UGameXXKCompanionFilterButton::Configure(UGameXXKCompanionRosterWidget* InOwner, const int32 InFilterIndex)
{
	Owner = InOwner;
	FilterIndex = InFilterIndex;
	OnClicked.AddDynamic(this, &UGameXXKCompanionFilterButton::HandleClicked);
}

void UGameXXKCompanionFilterButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleBackpackFilterClicked(FilterIndex);
	}
}

void UGameXXKCompanionRosterSlotButton::Configure(UGameXXKCompanionRosterWidget* InOwner, const int32 InSlotIndex)
{
	Owner = InOwner;
	SlotIndex = InSlotIndex;
	OnClicked.AddDynamic(this, &UGameXXKCompanionRosterSlotButton::HandleClicked);
}

void UGameXXKCompanionRosterSlotButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredRosterSlotClicked(SlotIndex);
	}
}

void UGameXXKCompanionRosterCardButton::Configure(
	UGameXXKCompanionRosterWidget* InOwner,
	const FName InCardId,
	const bool bInHeroDeck)
{
	Owner = InOwner;
	CardId = InCardId;
	bHeroDeckCard = bInHeroDeck;
	OnClicked.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleClicked);
	OnHovered.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleHovered);
	OnHovered.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleHovered);
	OnUnhovered.RemoveDynamic(this, &UGameXXKCompanionRosterCardButton::HandleUnhovered);
	OnUnhovered.AddDynamic(this, &UGameXXKCompanionRosterCardButton::HandleUnhovered);
}

void UGameXXKCompanionRosterCardButton::HandleClicked()
{
	if (Owner)
	{
		if (bHeroDeckCard)
		{
			Owner->HandleConfiguredHeroCardClicked(CardId);
		}
		else
		{
			Owner->HandleConfiguredPersonalCardClicked(CardId);
		}
	}
}

void UGameXXKCompanionRosterCardButton::HandleHovered()
{
	if (Owner && !CardId.IsNone())
	{
		Owner->HandleConfiguredCardHoverChanged(CardId, bHeroDeckCard, true);
	}
}

void UGameXXKCompanionRosterCardButton::HandleUnhovered()
{
	if (Owner && !CardId.IsNone())
	{
		Owner->HandleConfiguredCardHoverChanged(CardId, bHeroDeckCard, false);
	}
}

void UGameXXKCompanionEquipmentSlotButton::Configure(
	UGameXXKCompanionRosterWidget* InOwner,
	const EGameXXKCompanionEquipmentSlotSource InSource,
	const int32 InWarehouseIndex,
	const EGameXXKEquipmentSlot InEquipmentSlot)
{
	Owner = InOwner;
	Source = InSource;
	WarehouseIndex = InWarehouseIndex;
	EquipmentSlot = InEquipmentSlot;
	OnClicked.RemoveDynamic(this, &UGameXXKCompanionEquipmentSlotButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKCompanionEquipmentSlotButton::HandleClicked);
}

void UGameXXKCompanionEquipmentSlotButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredEquipmentSlotClicked(Source, WarehouseIndex, EquipmentSlot);
	}
}

bool UGameXXKCompanionEquipmentSlotButton::HandleRightMouseButtonDown()
{
	return Owner && Owner->HandleConfiguredEquipmentSlotRightClicked(Source, WarehouseIndex, EquipmentSlot);
}

TSharedRef<SWidget> UGameXXKCompanionEquipmentSlotButton::RebuildWidget()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyButton = SNew(SGameXXKCompanionEquipmentSlotButton, this)
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

TSharedRef<SWidget> UGameXXKCompanionRosterWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKCompanionRosterWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKCompanionRosterWidget::RefreshFromState()
{
	ClearCardTooltipHoverState();
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem)
	{
		// A new save reaches town before any battle setup has touched CardRun.  Prepare the fixed
		// twelve-card hero pool before this presentation layer performs its read-only snapshots.
		Subsystem->PrepareCompanionRosterForTown();
	}
	CachedRoster = Subsystem ? Subsystem->GetPermanentCompanionViews() : TArray<FGameXXKPermanentCompanion>();
	HeroCardSummary = Subsystem ? Subsystem->GetHeroCardLoadout() : TArray<FName>();
	TaskNpcCardSummary = Subsystem ? Subsystem->GetQuestNpcCardLoadout() : FGameXXKQuestNpcCardSelection();
	RefreshHeroCardData();
	bLoadoutReadOnly = !Subsystem || Subsystem->IsCompanionLoadoutMutationLocked();
	bRecruitmentActionsReadOnly = !Subsystem
		|| bLoadoutReadOnly
		|| Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town;
	RosterCapacity = Subsystem ? FMath::Clamp(Subsystem->GetPermanentCompanionRosterCapacity(), 0, RosterSlotCount) : RosterSlotCount;
	PendingRecruitmentCandidate = FGameXXKPermanentCompanion();
	SigilCount = Subsystem ? Subsystem->GetPermanentCompanionSigilCount() : 0;
	if (Subsystem)
	{
		Subsystem->TryGetPendingPermanentCompanionRecruitment(PendingRecruitmentCandidate);
	}

	if (!IsCurrentSelectedCompanion(SelectedCompanionId))
	{
		SelectedCompanionId = CachedRoster.IsEmpty() ? NAME_None : CachedRoster[0].InstanceId;
	}

	if (!bRosterPageInitialized)
	{
		const int32 SelectedRosterIndex = CachedRoster.IndexOfByPredicate([this](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId == SelectedCompanionId;
		});
		CurrentRosterPage = SelectedRosterIndex == INDEX_NONE ? 0 : SelectedRosterIndex / RosterPageSize;
		bRosterPageInitialized = true;
	}
	CurrentRosterPage = FMath::Clamp(CurrentRosterPage, 0, RosterPageCount - 1);
	RefreshVisibleRosterPage();
	RefreshSelectedCompanionData();
	RefreshProgrammaticLayout();
}

bool UGameXXKCompanionRosterWidget::SelectCompanion(const FName InstanceId)
{
	if (!IsCurrentSelectedCompanion(InstanceId))
	{
		return false;
	}

	ClearCardTooltipHoverState();
	SelectedCompanionId = InstanceId;
	const int32 SelectedRosterIndex = CachedRoster.IndexOfByPredicate([InstanceId](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == InstanceId;
	});
	if (SelectedRosterIndex != INDEX_NONE)
	{
		CurrentRosterPage = SelectedRosterIndex / RosterPageSize;
		RefreshVisibleRosterPage();
	}
	bEditingHeroDeck = false;
	RefreshSelectedCompanionData();
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ToggleSelectedCompanionCard(const FName CardId)
{
	if (bLoadoutReadOnly || SelectedCompanionId.IsNone() || CardId.IsNone() || !UnlockedPersonalCardIds.Contains(CardId))
	{
		return false;
	}

	if (PendingPersonalCardIds.Contains(CardId))
	{
		PendingPersonalCardIds.RemoveSingle(CardId);
	}
	else
	{
		if (PendingPersonalCardIds.Num() >= 5)
		{
			return false;
		}
		PendingPersonalCardIds.Add(CardId);
	}

	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ApplySelectedCompanionCardLoadout()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !Subsystem || SelectedCompanionId.IsNone() || PendingPersonalCardIds.Num() != 5)
	{
		return false;
	}

	if (!Subsystem->SetPermanentCompanionCardLoadout(SelectedCompanionId, PendingPersonalCardIds))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::SetSelectedCompanionAsActive()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !Subsystem || SelectedCompanionId.IsNone())
	{
		return false;
	}

	if (!Subsystem->SetActivePermanentCompanion(SelectedCompanionId))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::ClearActivePermanentCompanion()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem)
	{
		return false;
	}

	if (!Subsystem->ClearActivePermanentCompanion())
	{
		RecruitmentFeedback = TEXT("暂不编入未完成：该操作仅可在城镇的可编辑状态执行。");
		RefreshProgrammaticLayout();
		return false;
	}

	RecruitmentFeedback = TEXT("已暂不编入永久伙伴；本次路线可仅由主角与任务 NPC 出战。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::OpenHeroDeckEditor()
{
	if (VisibleHeroCardIds.IsEmpty())
	{
		return false;
	}

	bEditingHeroDeck = true;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ToggleHeroCard(const FName CardId)
{
	if (bLoadoutReadOnly || !bEditingHeroDeck || CardId.IsNone()
		|| !VisibleHeroCardIds.Contains(CardId)
		|| !UnlockedHeroCardIds.Contains(CardId))
	{
		return false;
	}

	if (PendingHeroCardIds.Contains(CardId))
	{
		PendingHeroCardIds.RemoveSingle(CardId);
	}
	else
	{
		if (PendingHeroCardIds.Num() >= 8)
		{
			return false;
		}
		PendingHeroCardIds.Add(CardId);
	}

	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKCompanionRosterWidget::ApplyHeroCardLoadout()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bLoadoutReadOnly || !bEditingHeroDeck || !Subsystem || PendingHeroCardIds.Num() != 8)
	{
		return false;
	}

	if (!Subsystem->SetHeroCardLoadout(PendingHeroCardIds))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::BeginRandomRecruitment()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem)
	{
		return false;
	}

	FGameXXKCompanionRecruitResult Result;
	if (!Subsystem->StartRandomPermanentCompanionRecruitment(Result))
	{
		return false;
	}

	switch (Result.Outcome)
	{
	case EGameXXKCompanionRecruitOutcome::Recruited:
		SelectedCompanionId = Result.Companion.InstanceId;
		bEditingHeroDeck = false;
		RecruitmentFeedback = FString::Printf(TEXT("已招募：%s · 已自动选中，可配置个人牌组。"), *GetRoleDisplayName(Result.Companion.Role).ToString());
		break;
	case EGameXXKCompanionRecruitOutcome::DuplicateSigil:
		RecruitmentFeedback = FString::Printf(TEXT("招募重复：%s 转化为 1 枚升星印（现有 %d 枚）。"),
			*GetRoleDisplayName(Result.Companion.Role).ToString(),
			Subsystem->GetPermanentCompanionSigilCount());
		break;
	case EGameXXKCompanionRecruitOutcome::PendingReplacement:
		RecruitmentFeedback = TEXT("名册已满：新伙伴已固定保存，请选择替换对象或放弃候选。");
		break;
	default:
		RecruitmentFeedback = TEXT("招贤未能完成，请稍后重试。");
		break;
	}

	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::ResolvePendingRecruitmentWithSelectedCompanion()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || SelectedCompanionId.IsNone())
	{
		return false;
	}

	// The dismissal transaction requires the companion's six slots to be empty;
	// the 遣散 action returns every equipped piece to the shared warehouse first.
	CharacterBackpackModel.Bind(Subsystem, SelectedCompanionId);
	for (const FGameXXKCharacterBackpackSlotView& SlotView : CharacterBackpackModel.GetSixSlotSnapshot())
	{
		if (!SlotView.EquippedInstanceId.IsNone())
		{
			FGameXXKEquipmentTransactionResult UnequipResult;
			CharacterBackpackModel.QuickUnequip(SlotView.Slot, UnequipResult);
		}
	}

	// Without a saved candidate the 遣散 action dismisses the partner outright
	// (equipment returns to the warehouse; the active party slot clears).
	if (PendingRecruitmentCandidate.InstanceId.IsNone())
	{
		if (!Subsystem->DismissPermanentCompanion(SelectedCompanionId))
		{
			RecruitmentFeedback = TEXT("遣散未完成：至少保留一名伙伴。");
			RefreshProgrammaticLayout();
			return false;
		}
		SelectedCompanionId = NAME_None;
		RecruitmentFeedback = TEXT("已遣散该伙伴，装备已返还到背包。");
		RefreshFromState();
		return true;
	}

	// The player has explicitly selected the replaced slot. If that slot is currently deployed, the
	// candidate visibly inherits that one optional party position instead of leaving an invalid party.
	const FName ActiveAfterReplacement = SelectedCompanionProfile.bIsActive
		? PendingRecruitmentCandidate.InstanceId
		: NAME_None;
	if (!Subsystem->ResolvePendingPermanentCompanionReplacement(SelectedCompanionId, ActiveAfterReplacement))
	{
		RecruitmentFeedback = TEXT("替换未完成：请先卸下装备并清空已投入经验，再替换该伙伴。");
		RefreshProgrammaticLayout();
		return false;
	}

	SelectedCompanionId = PendingRecruitmentCandidate.InstanceId;
	bEditingHeroDeck = false;
	RecruitmentFeedback = TEXT("已完成替换，并自动选中新伙伴。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::DiscardPendingRecruitment()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || PendingRecruitmentCandidate.InstanceId.IsNone())
	{
		return false;
	}

	if (!Subsystem->DiscardPendingPermanentCompanionRecruitment())
	{
		RecruitmentFeedback = TEXT("放弃候选未完成，请稍后重试。");
		RefreshProgrammaticLayout();
		return false;
	}

	RecruitmentFeedback = TEXT("已放弃本次候选；下次招贤将继续后续卡池顺序。");
	RefreshFromState();
	return true;
}

bool UGameXXKCompanionRosterWidget::PromoteSelectedCompanionStar()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (bRecruitmentActionsReadOnly || !Subsystem || SelectedCompanionId.IsNone() || SigilCount <= 0)
	{
		return false;
	}

	if (!Subsystem->PromotePermanentCompanionStar(SelectedCompanionId))
	{
		return false;
	}

	RefreshFromState();
	return true;
}

FGameXXKCompanionRosterProfileView UGameXXKCompanionRosterWidget::GetSelectedCompanionProfile() const
{
	return SelectedCompanionProfile;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisiblePersonalCardIds() const
{
	return VisiblePersonalCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetPendingPersonalCardIds() const
{
	return PendingPersonalCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetHeroCardSummary() const
{
	return HeroCardSummary;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisibleHeroCardIds() const
{
	return VisibleHeroCardIds;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetPendingHeroCardIds() const
{
	return PendingHeroCardIds;
}

FGameXXKQuestNpcCardSelection UGameXXKCompanionRosterWidget::GetTaskNpcCardSummary() const
{
	return TaskNpcCardSummary;
}

void UGameXXKCompanionRosterWidget::HandleConfiguredRosterSlotClicked(const int32 SlotIndex)
{
	if (VisibleRosterSlotInstanceIds.IsValidIndex(SlotIndex))
	{
		const FName InstanceId = VisibleRosterSlotInstanceIds[SlotIndex];
		if (!InstanceId.IsNone() && SelectCompanion(InstanceId))
		{
			SetSelectedCompanionAsActive();
		}
	}
}

void UGameXXKCompanionRosterWidget::HandleConfiguredPersonalCardClicked(const FName CardId)
{
	ToggleSelectedCompanionCard(CardId);
}

void UGameXXKCompanionRosterWidget::HandleConfiguredHeroCardClicked(const FName CardId)
{
	ToggleHeroCard(CardId);
}

void UGameXXKCompanionRosterWidget::HandleConfiguredCardHoverChanged(
	const FName CardId,
	const bool bInHeroDeck,
	const bool bHovered)
{
	if (bHovered)
	{
		HoveredCardTooltipId = CardId;
		bHoveredCardTooltipIsHeroDeck = bInHeroDeck;
	}
	else if (HoveredCardTooltipId == CardId && bHoveredCardTooltipIsHeroDeck == bInHeroDeck)
	{
		HoveredCardTooltipId = NAME_None;
		bHoveredCardTooltipIsHeroDeck = false;
	}
	RefreshCardTooltip();
}

int32 UGameXXKCompanionRosterWidget::GetRosterSlotCountForTest() const
{
	return RosterSlotCount;
}

int32 UGameXXKCompanionRosterWidget::GetRosterColumnCountForTest() const
{
	return RosterColumnCount;
}

int32 UGameXXKCompanionRosterWidget::GetRosterPageSizeForTest() const
{
	return RosterPageSize;
}

int32 UGameXXKCompanionRosterWidget::GetRosterPageCountForTest() const
{
	return RosterPageCount;
}

int32 UGameXXKCompanionRosterWidget::GetCurrentRosterPageForTest() const
{
	return CurrentRosterPage;
}

int32 UGameXXKCompanionRosterWidget::GetVisibleRosterButtonCountForTest() const
{
	return RosterSlotButtons.Num();
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisibleRosterSlotInstanceIdsForTest() const
{
	TArray<FName> Result;
	for (const FName InstanceId : VisibleRosterSlotInstanceIds)
	{
		if (!InstanceId.IsNone())
		{
			Result.Add(InstanceId);
		}
	}
	return Result;
}

bool UGameXXKCompanionRosterWidget::GoToNextRosterPageForTest()
{
	return ChangeRosterPage(1);
}

bool UGameXXKCompanionRosterWidget::GoToPreviousRosterPageForTest()
{
	return ChangeRosterPage(-1);
}

FString UGameXXKCompanionRosterWidget::GetRosterPageLeftResourcePathForTest() const
{
	return RosterPageLeftTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetRosterPageRightResourcePathForTest() const
{
	return RosterPageRightTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetRosterPortraitResourcePathForTest(const int32 VisibleSlotIndex) const
{
	return CurrentRosterPortraitResourcePaths.IsValidIndex(VisibleSlotIndex)
		? CurrentRosterPortraitResourcePaths[VisibleSlotIndex]
		: FString();
}

bool UGameXXKCompanionRosterWidget::HasSeparateSetActiveButtonForTest() const
{
	return SetActiveButton != nullptr;
}

FString UGameXXKCompanionRosterWidget::GetWindowFrameResourcePathForTest() const
{
	return WindowFrameTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetRosterSlotResourcePathForTest() const
{
	return RosterSlotTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardFrameResourcePathForTest() const
{
	return CardFrameTexturePath;
}

FName UGameXXKCompanionRosterWidget::GetSelectedCompanionIdForTest() const
{
	return SelectedCompanionId;
}

bool UGameXXKCompanionRosterWidget::IsLoadoutReadOnlyForTest() const
{
	return bLoadoutReadOnly;
}

bool UGameXXKCompanionRosterWidget::HasPersonalCardScrollBoxForTest() const
{
	return PersonalCardScroll != nullptr;
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardScrollTrackResourcePathForTest() const
{
	// Page 18 deck grid shares the page 03 PSD scrollbar track with the warehouse window.
	return ScrollbarTrackTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetPersonalCardScrollThumbResourcePathForTest() const
{
	return ScrollbarThumbTexturePath;
}

bool UGameXXKCompanionRosterWidget::HasPendingRecruitmentForTest() const
{
	return !PendingRecruitmentCandidate.InstanceId.IsNone();
}

FName UGameXXKCompanionRosterWidget::GetPendingRecruitmentCandidateIdForTest() const
{
	return PendingRecruitmentCandidate.InstanceId;
}

FString UGameXXKCompanionRosterWidget::GetRecruitmentStatusForTest() const
{
	// Page 18 has no legacy recruitment status panel; the dismiss action's
	// tooltip carries the transient recruitment feedback and dismissal reason.
	if (RecruitmentStatusText)
	{
		return RecruitmentStatusText->GetText().ToString();
	}
	return ReplacePendingButton ? ReplacePendingButton->GetToolTipText().ToString() : FString();
}

bool UGameXXKCompanionRosterWidget::IsHeroDeckEditorOpenForTest() const
{
	return bEditingHeroDeck;
}

FString UGameXXKCompanionRosterWidget::GetCardTooltipTextForTest() const
{
	// The hovered card's paper tooltip carries the full effect description.
	if (HoveredCardTooltipId.IsNone())
	{
		return FString();
	}
	const TArray<FName>& VisibleCardIds = bEditingHeroDeck ? VisibleHeroCardIds : VisiblePersonalCardIds;
	const int32 CardIndex = VisibleCardIds.IndexOfByKey(HoveredCardTooltipId);
	return PersonalCardTooltipTexts.IsValidIndex(CardIndex) ? PersonalCardTooltipTexts[CardIndex] : FString();
}

bool UGameXXKCompanionRosterWidget::IsCardTooltipVisibleForTest() const
{
	return !HoveredCardTooltipId.IsNone();
}

bool UGameXXKCompanionRosterWidget::IsCardTooltipHitTestInvisibleForTest() const
{
	return !HoveredCardTooltipId.IsNone();
}

FText UGameXXKCompanionRosterWidget::GetTitleTextForTest() const
{
	return TitleText ? TitleText->GetText() : FText::GetEmpty();
}

int32 UGameXXKCompanionRosterWidget::GetEquipmentSlotCountForTest() const
{
	return CompanionEquipmentSlotButtons.Num();
}

int32 UGameXXKCompanionRosterWidget::GetEquipmentBackpackViewportSlotCountForTest() const
{
	return EquipmentBackpackViewportSlotCount;
}

int32 UGameXXKCompanionRosterWidget::GetEquipmentBackpackStorageCapacityForTest() const
{
	return EquipmentBackpackStorageCapacity;
}

bool UGameXXKCompanionRosterWidget::HasEquipmentBackpackScrollBoxForTest() const
{
	return EquipmentBackpackScrollBox != nullptr;
}

bool UGameXXKCompanionRosterWidget::IsEquipmentBackpackTabOpenForTest() const
{
	return ActiveBackpackTab == EGameXXKCompanionBackpackTab::Equipment;
}

bool UGameXXKCompanionRosterWidget::IsCardBackpackTabOpenForTest() const
{
	return ActiveBackpackTab == EGameXXKCompanionBackpackTab::Cards;
}

bool UGameXXKCompanionRosterWidget::OpenEquipmentBackpackTabForTest()
{
	SetActiveBackpackTab(EGameXXKCompanionBackpackTab::Equipment);
	return true;
}

bool UGameXXKCompanionRosterWidget::OpenCardBackpackTabForTest()
{
	RefreshPersonalCards();
	RefreshDeckEditorControls();
	SetActiveBackpackTab(EGameXXKCompanionBackpackTab::Cards);
	return true;
}

TArray<FName> UGameXXKCompanionRosterWidget::GetVisibleEquipmentInstanceIdsForTest() const
{
	return VisibleEquipmentWarehouseInstanceIds;
}

bool UGameXXKCompanionRosterWidget::QuickEquipSelectedCompanionInstanceForTest(const FName InstanceId)
{
	CharacterBackpackModel.Bind(ResolveMVPSubsystem(), SelectedCompanionId);
	FGameXXKEquipmentTransactionResult Result;
	const bool bSucceeded = CharacterBackpackModel.QuickEquip(InstanceId, Result);
	if (bSucceeded)
	{
		RefreshFromState();
	}
	return bSucceeded;
}

bool UGameXXKCompanionRosterWidget::QuickUnequipSelectedCompanionSlotForTest(const EGameXXKEquipmentSlot EquipmentSlot)
{
	CharacterBackpackModel.Bind(ResolveMVPSubsystem(), SelectedCompanionId);
	FGameXXKEquipmentTransactionResult Result;
	const bool bSucceeded = CharacterBackpackModel.QuickUnequip(EquipmentSlot, Result);
	if (bSucceeded)
	{
		RefreshFromState();
	}
	return bSucceeded;
}

FName UGameXXKCompanionRosterWidget::GetSelectedCompanionEquippedInstanceForTest(const EGameXXKEquipmentSlot EquipmentSlot) const
{
	FGameXXKCharacterBackpackModel Model;
	Model.Bind(ResolveMVPSubsystem(), SelectedCompanionId);
	const TArray<FGameXXKCharacterBackpackSlotView> Snapshot = Model.GetSixSlotSnapshot();
	const FGameXXKCharacterBackpackSlotView* MatchedSlot = Snapshot.FindByPredicate([EquipmentSlot](const FGameXXKCharacterBackpackSlotView& Candidate)
	{
		return Candidate.Slot == EquipmentSlot;
	});
	return MatchedSlot ? MatchedSlot->EquippedInstanceId : NAME_None;
}

FString UGameXXKCompanionRosterWidget::GetLockedCardIconResourcePathForTest() const
{
	return LockedCardIconTexturePath;
}

FString UGameXXKCompanionRosterWidget::GetCloseButtonResourcePathForTest() const
{
	return CloseButtonTexturePath;
}

void UGameXXKCompanionRosterWidget::HandleConfiguredEquipmentSlotClicked(
	const EGameXXKCompanionEquipmentSlotSource Source,
	const int32 WarehouseIndex,
	const EGameXXKEquipmentSlot EquipmentSlot)
{
	if (Source == EGameXXKCompanionEquipmentSlotSource::Warehouse)
	{
		SelectedWarehouseSlotIndex = WarehouseIndex;
		SelectedEquippedSlotIndex = INDEX_NONE;
	}
	else
	{
		SelectedWarehouseSlotIndex = INDEX_NONE;
		SelectedEquippedSlotIndex = EquipmentSlot != EGameXXKEquipmentSlot::Invalid
			? GetCompanionEquipmentSlotOrder().IndexOfByKey(EquipmentSlot)
			: INDEX_NONE;
	}
	RefreshEquipmentBackpack();
}

bool UGameXXKCompanionRosterWidget::HandleConfiguredEquipmentSlotRightClicked(
	const EGameXXKCompanionEquipmentSlotSource Source,
	const int32 WarehouseIndex,
	const EGameXXKEquipmentSlot EquipmentSlot)
{
	if (Source == EGameXXKCompanionEquipmentSlotSource::Warehouse)
	{
		return VisibleEquipmentWarehouseInstanceIds.IsValidIndex(WarehouseIndex)
			&& !VisibleEquipmentWarehouseInstanceIds[WarehouseIndex].IsNone()
			&& QuickEquipSelectedCompanionInstanceForTest(VisibleEquipmentWarehouseInstanceIds[WarehouseIndex]);
	}
	return EquipmentSlot != EGameXXKEquipmentSlot::Invalid
		&& !GetSelectedCompanionEquippedInstanceForTest(EquipmentSlot).IsNone()
		&& QuickUnequipSelectedCompanionSlotForTest(EquipmentSlot);
}

void UGameXXKCompanionRosterWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("CompanionRosterWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Page 18 paper window at absolute Master V1 screen coordinates.
	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterWindowFrame"));
	WindowFrame->SetBrush(MakeBoxTextureBrush(WindowFrameTexturePath, CompanionPaperSize));
	WindowFrame->SetBrushColor(FLinearColor::White);
	WindowFrame->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
	AddCanvasChild(RootCanvas, WindowFrame, CompanionPaperPos, CompanionPaperSize);

	// Content is placed at Master V1 screen coordinates, so the content canvas
	// must live on the root at (0,0) — a sibling of the paper window, not a child.
	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterFrameCanvas"));
	AddCanvasChild(RootCanvas, FrameCanvas, FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f));

	TitleText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "Title", "伙伴"), 28, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f), TEXT("CompanionRosterTitle"));
	AddCanvasChild(FrameCanvas, TitleText, FVector2D(383.0f, 230.0f), FVector2D(84.0f, 42.0f));

	// Five tabs at page 03/18 positions: 属性/装备/卡组 are functional;
	// 天赋/称号 render the approved "尚未开放" body like the hero backpack.
	const EGameXXKCompanionBackpackTab CompanionTabs[] = {
		EGameXXKCompanionBackpackTab::Attributes,
		EGameXXKCompanionBackpackTab::Equipment,
		EGameXXKCompanionBackpackTab::Cards,
		EGameXXKCompanionBackpackTab::Talents,
		EGameXXKCompanionBackpackTab::Titles};
	const FText CompanionTabLabels[] = {
		NSLOCTEXT("GameXXKCompanionRoster", "TabAttributes", "属性"),
		NSLOCTEXT("GameXXKCompanionRoster", "TabEquipment", "装备"),
		NSLOCTEXT("GameXXKCompanionRoster", "TabCards", "卡组"),
		NSLOCTEXT("GameXXKCompanionRoster", "TabTalents", "天赋"),
		NSLOCTEXT("GameXXKCompanionRoster", "TabTitles", "称号")};
	TObjectPtr<UButton>* TabTargets[] = {
		&AttributesTabButton, &EquipmentBackpackTabButton, &CardBackpackTabButton, &TalentsTabButton, &TitlesTabButton};
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(CompanionTabs); ++TabIndex)
	{
		UButton* TabButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("CompanionRosterTab_%d"), TabIndex));
		TabButton->SetStyle(MakeBoxTextureButtonStyle(TabNormalTexturePath, CompanionTabSize, FMargin(0.08f)));
		TabButton->SetBackgroundColor(FLinearColor::White);
		UTextBlock* TabText = MakeText(WidgetTree, CompanionTabLabels[TabIndex], 14);
		TabText->SetJustification(ETextJustify::Center);
		TabButton->AddChild(TabText);
		switch (CompanionTabs[TabIndex])
		{
		case EGameXXKCompanionBackpackTab::Attributes:
			TabButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleAttributesTabClicked);
			break;
		case EGameXXKCompanionBackpackTab::Equipment:
			TabButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleEquipmentBackpackTabClicked);
			break;
		case EGameXXKCompanionBackpackTab::Cards:
			TabButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleCardBackpackTabClicked);
			break;
		case EGameXXKCompanionBackpackTab::Talents:
			TabButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleTalentsTabClicked);
			break;
		case EGameXXKCompanionBackpackTab::Titles:
			TabButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleTitlesTabClicked);
			break;
		default:
			break;
		}
		AddCanvasChild(FrameCanvas, TabButton, CompanionTabPositions[TabIndex], CompanionTabSize);
		*TabTargets[TabIndex] = TabButton;
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterCloseButton"));
	CloseButton->SetStyle(MakeTextureButtonStyle(CloseButtonTexturePath, CompanionCloseButtonSize));
	CloseButton->SetBackgroundColor(FLinearColor::White);
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleCloseClicked);
	AddCanvasChild(FrameCanvas, CloseButton, FVector2D(1652.0f, 201.0f), CompanionCloseButtonSize);

	// Page 18 center: the selected companion's idle full body plus its display
	// name above the head; the brush is swapped in RefreshCenterCompanionPresentation.
	CenterCompanionPortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompanionRosterCenterPortrait"));
	CenterCompanionPortraitImage->SetBrush(MakeTextureBrush(*ResolveCompanionFullBodyResourcePath(EGameXXKCharacterRole::Blade), CompanionCenterPortraitSize));
	AddCanvasChild(FrameCanvas, CenterCompanionPortraitImage, CompanionCenterPortraitPos, CompanionCenterPortraitSize);
	CenterCompanionNameText = MakeText(WidgetTree, FText::GetEmpty(), 20, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f), TEXT("CompanionRosterCenterName"));
	CenterCompanionNameText->SetJustification(ETextJustify::Center);
	AddCanvasChild(FrameCanvas, CenterCompanionNameText, CompanionCenterNamePos, CompanionCenterNameSize);

	// Six equipment slots around the center portrait (page 18 left/right columns).
	const TArray<EGameXXKEquipmentSlot>& EquipmentSlots = GetCompanionEquipmentSlotOrder();
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlots.Num(); ++SlotIndex)
	{
		const EGameXXKEquipmentSlot EquipmentSlot = EquipmentSlots[SlotIndex];
		UGameXXKCompanionEquipmentSlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKCompanionEquipmentSlotButton>(
			UGameXXKCompanionEquipmentSlotButton::StaticClass(),
			*FString::Printf(TEXT("CompanionEquipmentSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, EGameXXKCompanionEquipmentSlotSource::Equipped, INDEX_NONE, EquipmentSlot);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(EquipmentSlotTexturePath, CompanionEquipmentSlotSize, FMargin(0.08f)));
		SlotButton->SetBackgroundColor(FLinearColor::White);
		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = SlotOverlay->AddChildToOverlay(SlotIcon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		SlotButton->AddChild(SlotOverlay);

		// Page 03 paper tooltip identical to the hero equipped-slot tooltip.
		UBorder* TooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("CompanionEquipmentTooltip_%02d"), SlotIndex));
		TooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, CompanionTooltipPaperSize));
		TooltipFrame->SetBrushColor(FLinearColor::White);
		TooltipFrame->SetPadding(CompanionTooltipPadding);
		UVerticalBox* TooltipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TooltipFrame->AddChild(TooltipBox);
		UTextBlock* TooltipName = MakeText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
		TooltipBox->AddChildToVerticalBox(TooltipName);
		UTextBlock* TooltipDetail = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.14f, 0.11f, 0.08f, 1.0f));
		if (UVerticalBoxSlot* TooltipDetailSlot = TooltipBox->AddChildToVerticalBox(TooltipDetail))
		{
			TooltipDetailSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
		SlotButton->SetToolTip(TooltipFrame);

		AddCanvasChild(FrameCanvas, SlotButton, CompanionEquipmentFramePositions[SlotIndex], CompanionEquipmentSlotSize);
		CompanionEquipmentSlotButtons.Add(SlotButton);
		CompanionEquipmentSlotIcons.Add(SlotIcon);
		CompanionEquipmentTooltipFrames.Add(TooltipFrame);
		CompanionEquipmentTooltipNameBlocks.Add(TooltipName);
		CompanionEquipmentTooltipDetailBlocks.Add(TooltipDetail);
	}

	// Selection ink hovering above the selected equipped slot (like the hero backpack).
	EquipmentSelectionInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompanionRosterEquipmentSelectionInk"));
	EquipmentSelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, CompanionSelectionInkSize));
	EquipmentSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(FrameCanvas, EquipmentSelectionInk, CompanionSelectionInkPos, CompanionSelectionInkSize);

	// Right body area (page 18): the attribute / talent / title body occupies the
	// backpack grid area without a paper back.
	AttributesBodyPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterAttributesBodyPanel"));
	{
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		AttributesBodyPanel->SetBrush(Transparent);
	}
	AttributesBodyPanel->SetPadding(FMargin(24.0f, 20.0f));
	UVerticalBox* AttributesBodyBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CompanionRosterAttributesBodyBox"));
	AttributesBodyPanel->SetContent(AttributesBodyBox);
	ProfileTitleText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "NoCompanion", "尚未招募伙伴"), 21, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f), TEXT("CompanionRosterProfileTitle"));
	AttributesBodyBox->AddChildToVerticalBox(ProfileTitleText);
	ProfileDetailText = MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.12f, 0.09f, 0.06f, 1.0f), TEXT("CompanionRosterProfileDetail"));
	if (UVerticalBoxSlot* DetailSlot = AttributesBodyBox->AddChildToVerticalBox(ProfileDetailText))
	{
		DetailSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 10.0f));
	}
	AddCanvasChild(FrameCanvas, AttributesBodyPanel, CompanionBackpackViewportPos, CompanionBackpackViewportSize);

	// Equipment tab: page 03 filter row above the warehouse window.
	const FText CompanionFilterLabels[] = {
		NSLOCTEXT("GameXXKCompanionRoster", "FilterAll", "全部"),
		NSLOCTEXT("GameXXKCompanionRoster", "FilterEquipment", "装备"),
		NSLOCTEXT("GameXXKCompanionRoster", "FilterProps", "道具"),
		NSLOCTEXT("GameXXKCompanionRoster", "FilterMaterials", "材料"),
		NSLOCTEXT("GameXXKCompanionRoster", "FilterTasks", "任务")};
	for (int32 FilterIndex = 0; FilterIndex < UE_ARRAY_COUNT(CompanionFilterLabels); ++FilterIndex)
	{
		UGameXXKCompanionFilterButton* FilterButton = WidgetTree->ConstructWidget<UGameXXKCompanionFilterButton>(UGameXXKCompanionFilterButton::StaticClass(), *FString::Printf(TEXT("CompanionRosterFilter_%d"), FilterIndex));
		FilterButton->Configure(this, FilterIndex);
		FilterButton->SetStyle(MakeInvisibleButtonStyle());
		FilterButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UTextBlock* FilterText = MakeText(WidgetTree, CompanionFilterLabels[FilterIndex], 18, FLinearColor(0.20f, 0.14f, 0.09f, 1.0f));
		FilterText->SetJustification(ETextJustify::Center);
		FilterButton->AddChild(FilterText);
		// The partner warehouse holds equipment only; props/materials/tasks are inert.
		if (FilterIndex >= 2)
		{
			FilterButton->SetIsEnabled(false);
			FilterButton->SetToolTipText(NSLOCTEXT("GameXXKCompanionRoster", "FilterUnavailable", "伙伴仓库仅装备，此分类暂无内容"));
		}
		AddCanvasChild(FrameCanvas, FilterButton, CompanionFilterRowPos + FVector2D(FilterIndex * CompanionFilterRowPitch, 0.0f), CompanionFilterRowSize);
		BackpackFilterButtons.Add(FilterButton);
		BackpackFilterTextBlocks.Add(FilterText);
	}

	// Equipment tab: a 4x5 viewport into the scrollable warehouse, no panel behind
	// the cells — they sit directly on the paper window (page 03 style).
	// Page 03 structure identical to the hero backpack: the warehouse window is a
	// plain scroll box on the content canvas (no intermediate border/canvas).
	EquipmentBackpackPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterEquipmentBackpackPanel"));
	{
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		EquipmentBackpackPanel->SetBrush(Transparent);
	}
	EquipmentBackpackPanel->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
	AddCanvasChild(FrameCanvas, EquipmentBackpackPanel, CompanionBackpackViewportPos, CompanionBackpackViewportSize);

	EquipmentBackpackScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CompanionRosterEquipmentScrollBox"));
	EquipmentBackpackScrollBox->SetOrientation(Orient_Vertical);
	EquipmentBackpackScrollBox->SetAlwaysShowScrollbar(false);
	EquipmentBackpackScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	EquipmentBackpackScrollBox->OnUserScrolled.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleEquipmentBackpackScrolled);
	AddCanvasChild(FrameCanvas, EquipmentBackpackScrollBox, CompanionBackpackViewportPos, CompanionBackpackViewportSize);
	UE_LOG(LogTemp, Warning, TEXT("[WarehouseGrid] scrollbox pos=%s size=%s grid size=%s slot count=%d"),
		*CompanionBackpackViewportPos.ToString(),
		*CompanionBackpackViewportSize.ToString(),
		*CompanionBackpackGridSize.ToString(),
		EquipmentBackpackViewportSlotCount);

	UCanvasPanel* BackpackContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterBackpackContentCanvas"));
	EquipmentBackpackScrollBox->AddChild(BackpackContentCanvas);

	EquipmentBackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CompanionRosterEquipmentGrid"));
	EquipmentBackpackGrid->SetSlotPadding(CompanionBackpackSlotPadding);
	AddCanvasChild(BackpackContentCanvas, EquipmentBackpackGrid, CompanionBackpackContentOffset, CompanionBackpackGridSize);

	for (int32 WarehouseIndex = 0; WarehouseIndex < EquipmentBackpackViewportSlotCount; ++WarehouseIndex)
	{
		UGameXXKCompanionEquipmentSlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKCompanionEquipmentSlotButton>(
			UGameXXKCompanionEquipmentSlotButton::StaticClass(),
			*FString::Printf(TEXT("CompanionEquipmentWarehouseSlot_%03d"), WarehouseIndex));
		SlotButton->Configure(this, EGameXXKCompanionEquipmentSlotSource::Warehouse, WarehouseIndex);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(ItemSlotTexturePath, CompanionWarehouseSlotSize, FMargin(0.08f)));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SlotIcon);

		UTextBlock* SlotLabel = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor::White);
		SlotLabel->SetJustification(ETextJustify::Right);
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Right);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 4.0f));
		}

		// Page 03 paper tooltip identical to the hero backpack (name + detail).
		UBorder* TooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("CompanionWarehouseTooltip_%03d"), WarehouseIndex));
		TooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, CompanionTooltipPaperSize));
		TooltipFrame->SetBrushColor(FLinearColor::White);
		TooltipFrame->SetPadding(CompanionTooltipPadding);
		UVerticalBox* TooltipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TooltipFrame->AddChild(TooltipBox);
		UTextBlock* TooltipName = MakeText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
		TooltipBox->AddChildToVerticalBox(TooltipName);
		UTextBlock* TooltipDetail = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.14f, 0.11f, 0.08f, 1.0f));
		if (UVerticalBoxSlot* TooltipDetailSlot = TooltipBox->AddChildToVerticalBox(TooltipDetail))
		{
			TooltipDetailSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
		// Replaced-slot comparison rows (red gain / green loss) appear on hover
		// when the warehouse item would replace an occupied equipment slot.
		TArray<TObjectPtr<UTextBlock>> CompareRows;
		for (int32 CompareIndex = 0; CompareIndex < 5; ++CompareIndex)
		{
			UTextBlock* CompareRow = MakeText(WidgetTree, FText::GetEmpty(), 11, FLinearColor::White);
			CompareRow->SetVisibility(ESlateVisibility::Collapsed);
			TooltipBox->AddChildToVerticalBox(CompareRow);
			CompareRows.Add(CompareRow);
		}
		BackpackCompareTextBlocks.Add(MoveTemp(CompareRows));
		SlotButton->SetToolTip(TooltipFrame);

		SlotButton->AddChild(SlotOverlay);
		USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSize->SetWidthOverride(CompanionWarehouseSlotSize.X);
		SlotSize->SetHeightOverride(CompanionWarehouseSlotSize.Y);
		SlotSize->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = EquipmentBackpackGrid->AddChildToUniformGrid(
			SlotSize,
			WarehouseIndex / EquipmentBackpackColumnCount,
			WarehouseIndex % EquipmentBackpackColumnCount))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		EquipmentWarehouseSlotButtons.Add(SlotButton);
		EquipmentWarehouseSlotIcons.Add(SlotIcon);
		BackpackSlotLabels.Add(SlotLabel);
		BackpackTooltipFrames.Add(TooltipFrame);
		BackpackTooltipNameTextBlocks.Add(TooltipName);
		BackpackTooltipDetailTextBlocks.Add(TooltipDetail);
	}

	// Page 03/18 right-side scrollbar: PSD track + thumb; shared by the warehouse
	// window and the card grid.
	ScrollbarTrackImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompanionRosterScrollbarTrack"));
	ScrollbarTrackImage->SetBrush(MakeTextureBrush(ScrollbarTrackTexturePath, CompanionScrollbarTrackSize));
	AddCanvasChild(FrameCanvas, ScrollbarTrackImage, CompanionScrollbarTrackPos, CompanionScrollbarTrackSize);
	InventoryScrollbarThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompanionRosterScrollbarThumb"));
	InventoryScrollbarThumb->SetBrush(MakeTextureBrush(ScrollbarThumbTexturePath, CompanionScrollbarThumbSize));
	AddCanvasChild(FrameCanvas, InventoryScrollbarThumb, CompanionScrollbarThumbTop, CompanionScrollbarThumbSize);

	// Selection ink above the selected warehouse column (page 03 style).
	BackpackSelectionInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CompanionRosterBackpackSelectionInk"));
	BackpackSelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, CompanionSelectionInkSize));
	BackpackSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(FrameCanvas, BackpackSelectionInk, CompanionSelectionInkPos, CompanionSelectionInkSize);

	// Deck tab mirrors the hero deck tab geometry: caption, a 3-column card grid
	// inside a scroll viewport, the apply button and pick counter in the same spots.
	PersonalDeckPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterPersonalDeckPanel"));
	{
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		PersonalDeckPanel->SetBrush(Transparent);
	}
	PersonalDeckPanel->SetPadding(FMargin(24.0f, 20.0f));
	AddCanvasChild(FrameCanvas, PersonalDeckPanel, CompanionBackpackViewportPos, CompanionBackpackViewportSize);

	UCanvasPanel* PersonalDeckCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CompanionRosterPersonalDeckCanvas"));
	PersonalDeckPanel->AddChild(PersonalDeckCanvas);

	DeckCaptionText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "PersonalDeckCaption", "个人牌组（12 张，编入 5 张）"), 17, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f), TEXT("CompanionRosterDeckCaption"));
	AddCanvasChild(PersonalDeckCanvas, DeckCaptionText, FVector2D::ZeroVector, FVector2D(470.0f, 28.0f));

	PersonalCardScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CompanionRosterPersonalCardScroll"));
	PersonalCardScroll->SetOrientation(Orient_Vertical);
	PersonalCardScroll->SetAlwaysShowScrollbar(false);
	PersonalCardScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PersonalCardScroll->OnUserScrolled.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandlePersonalCardScrolled);
	AddCanvasChild(PersonalDeckCanvas, PersonalCardScroll, CompanionDeckScrollPos, CompanionDeckScrollSize);

	PersonalCardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("CompanionRosterPersonalCardGrid"));
	PersonalCardGrid->SetSlotPadding(CompanionCardSlotPadding);
	PersonalCardScroll->AddChild(PersonalCardGrid);

	// Twelve fixed hero-style cards: frame paper + portrait + name band + costs
	// + top selection ink + centered lock; refreshed in place per companion.
	for (int32 CardIndex = 0; CardIndex < 12; ++CardIndex)
	{
		UGameXXKCompanionRosterCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKCompanionRosterCardButton>(
			UGameXXKCompanionRosterCardButton::StaticClass(),
			*FString::Printf(TEXT("CompanionRosterPersonalCard_%02d"), CardIndex));
		CardButton->Configure(this, NAME_None, false);
		CardButton->SetStyle(MakeCardButtonStyle());
		CardButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* CardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* CardPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("CompanionRosterPersonalCardPortrait_%02d"), CardIndex));
		CardPortrait->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* PortraitSlot = CardOverlay->AddChildToOverlay(CardPortrait))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Center);
			PortraitSlot->SetVerticalAlignment(VAlign_Center);
			PortraitSlot->SetPadding(FMargin(0.0f));
		}

		// Selection ink sits at the card top so the selected card name stays visible.
		UImage* SelectedInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("CompanionRosterPersonalCardInk_%02d"), CardIndex));
		SelectedInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, CompanionSelectionInkSize));
		SelectedInk->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* InkSlot = CardOverlay->AddChildToOverlay(SelectedInk))
		{
			InkSlot->SetHorizontalAlignment(HAlign_Center);
			InkSlot->SetVerticalAlignment(VAlign_Top);
		}

		UTextBlock* CardLabel = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
		CardLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13));
		CardLabel->SetJustification(ETextJustify::Center);
		if (UOverlaySlot* LabelSlot = CardOverlay->AddChildToOverlay(CardLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Top);
			LabelSlot->SetPadding(FMargin(5.0f, 15.0f, 5.0f, 0.0f));
		}

		UTextBlock* CostQiLabel = MakeText(WidgetTree, FText::GetEmpty(), 10, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
		CostQiLabel->SetJustification(ETextJustify::Left);
		if (UOverlaySlot* CostSlot = CardOverlay->AddChildToOverlay(CostQiLabel))
		{
			CostSlot->SetHorizontalAlignment(HAlign_Left);
			CostSlot->SetVerticalAlignment(VAlign_Top);
			CostSlot->SetPadding(FMargin(5.0f, 50.0f, 0.0f, 0.0f));
		}

		UTextBlock* CostManaLabel = MakeText(WidgetTree, FText::GetEmpty(), 10, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
		CostManaLabel->SetJustification(ETextJustify::Left);
		if (UOverlaySlot* CostSlot = CardOverlay->AddChildToOverlay(CostManaLabel))
		{
			CostSlot->SetHorizontalAlignment(HAlign_Left);
			CostSlot->SetVerticalAlignment(VAlign_Top);
			CostSlot->SetPadding(FMargin(5.0f, 68.0f, 0.0f, 0.0f));
		}

		UImage* LockedIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("CompanionRosterPersonalCardLock_%02d"), CardIndex));
		LockedIcon->SetBrush(MakeTextureBrush(LockedCardIconTexturePath, FVector2D(34.0f, 34.0f)));
		LockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* LockSlot = CardOverlay->AddChildToOverlay(LockedIcon))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Center);
			LockSlot->SetVerticalAlignment(VAlign_Center);
		}

		// Hero-style paper tooltip (name + full effect description) follows the cursor.
		UBorder* CardTooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("CompanionRosterPersonalCardTooltip_%02d"), CardIndex));
		CardTooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, CompanionTooltipPaperSize));
		CardTooltipFrame->SetBrushColor(FLinearColor::White);
		CardTooltipFrame->SetPadding(CompanionTooltipPadding);
		UVerticalBox* CardTooltipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CardTooltipFrame->AddChild(CardTooltipBox);
		UTextBlock* CardTooltipName = MakeText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
		CardTooltipBox->AddChildToVerticalBox(CardTooltipName);
		UTextBlock* CardTooltipDetail = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.14f, 0.11f, 0.08f, 1.0f));
		CardTooltipDetail->SetAutoWrapText(true);
		if (UVerticalBoxSlot* TooltipDetailSlot = CardTooltipBox->AddChildToVerticalBox(CardTooltipDetail))
		{
			TooltipDetailSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
		CardButton->SetToolTip(CardTooltipFrame);

		CardButton->AddChild(CardOverlay);
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(PersonalCardSize.X);
		CardSize->SetHeightOverride(PersonalCardSize.Y);
		CardSize->AddChild(CardButton);
		if (UUniformGridSlot* CardSlot = PersonalCardGrid->AddChildToUniformGrid(CardSize, CardIndex / PersonalCardColumnCount, CardIndex % PersonalCardColumnCount))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
		PersonalCardButtons.Add(CardButton);
		PersonalCardPortraits.Add(CardPortrait);
		PersonalCardSelectedInks.Add(SelectedInk);
		PersonalCardNameLabels.Add(CardLabel);
		PersonalCardCostLabels.Add(CostQiLabel);
		PersonalCardManaCostLabels.Add(CostManaLabel);
		PersonalCardLockedIcons.Add(LockedIcon);
		PersonalCardTooltipFrames.Add(CardTooltipFrame);
		PersonalCardTooltipNameBlocks.Add(CardTooltipName);
		PersonalCardTooltipDetailBlocks.Add(CardTooltipDetail);
		PersonalCardTooltipTexts.Add(FString());
	}

	LoadoutStatusText = MakeText(WidgetTree, FText::GetEmpty(), 14, FLinearColor(0.30f, 0.20f, 0.10f, 1.0f), TEXT("CompanionRosterLoadoutStatus"));
	LoadoutStatusText->SetJustification(ETextJustify::Center);
	AddCanvasChild(PersonalDeckCanvas, LoadoutStatusText, CompanionDeckCountPos, CompanionDeckCountSize);

	ApplyLoadoutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterApplyLoadout"));
	ApplyLoadoutButton->SetStyle(MakeTextureButtonStyle(DismissButtonTexturePath, CompanionDeckApplySize));
	ApplyLoadoutButton->SetBackgroundColor(FLinearColor::White);
	ApplyLoadoutButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleApplyLoadoutClicked);
	ApplyLoadoutButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ApplyLoadout", "确认编入 5 张"), 14, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ApplyLoadoutButtonText->SetJustification(ETextJustify::Center);
	ApplyLoadoutButton->AddChild(ApplyLoadoutButtonText);
	AddCanvasChild(PersonalDeckCanvas, ApplyLoadoutButton, CompanionDeckApplyPos, CompanionDeckApplySize);

	// Bottom avatar strip (page 18): page arrows flanking three avatar slots.
	RosterPageLeftButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterPageLeft"));
	RosterPageLeftButton->SetStyle(MakeTextureButtonStyle(RosterPageLeftTexturePath, CompanionRosterPageArrowSize));
	RosterPageLeftButton->SetBackgroundColor(FLinearColor::White);
	RosterPageLeftButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleRosterPageLeftClicked);
	AddCanvasChild(FrameCanvas, RosterPageLeftButton, CompanionRosterPageLeftPos, CompanionRosterPageArrowSize);

	for (int32 SlotIndex = 0; SlotIndex < RosterPageSize; ++SlotIndex)
	{
		UGameXXKCompanionRosterSlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKCompanionRosterSlotButton>(UGameXXKCompanionRosterSlotButton::StaticClass(), *FString::Printf(TEXT("CompanionRosterSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, SlotIndex);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(RosterSlotTexturePath, CompanionRosterSlotSize, SlotFrameMargin));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotPortrait->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* PortraitSlot = SlotOverlay->AddChildToOverlay(SlotPortrait))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Center);
			PortraitSlot->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* SelectionBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SelectionBorder->SetBrushColor(FLinearColor(0.94f, 0.75f, 0.31f, 0.35f));
		SelectionBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* SelectionSlot = SlotOverlay->AddChildToOverlay(SelectionBorder))
		{
			SelectionSlot->SetHorizontalAlignment(HAlign_Fill);
			SelectionSlot->SetVerticalAlignment(VAlign_Fill);
		}
		SlotButton->AddChild(SlotOverlay);
		AddCanvasChild(FrameCanvas, SlotButton, CompanionRosterSlotPositions[SlotIndex], CompanionRosterSlotSize);
		RosterSlotButtons.Add(SlotButton);
		RosterSlotPortraits.Add(SlotPortrait);
		RosterSlotSelectionBorders.Add(SelectionBorder);
	}

	RosterPageRightButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterPageRight"));
	RosterPageRightButton->SetStyle(MakeTextureButtonStyle(RosterPageRightTexturePath, CompanionRosterPageArrowSize));
	RosterPageRightButton->SetBackgroundColor(FLinearColor::White);
	RosterPageRightButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleRosterPageRightClicked);
	AddCanvasChild(FrameCanvas, RosterPageRightButton, CompanionRosterPageRightPos, CompanionRosterPageArrowSize);

	RosterCountText = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.24f, 0.17f, 0.10f, 1.0f), TEXT("CompanionRosterCount"));
	RosterCountText->SetJustification(ETextJustify::Center);
	AddCanvasChild(FrameCanvas, RosterCountText, CompanionRosterCountPos, CompanionRosterCountSize);

	// Page 18 bottom-right action: 遣散 (dismiss/replacement) on the approved decompose paper.
	ReplacePendingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CompanionRosterReplacePendingAction"));
	ReplacePendingButton->SetStyle(MakeBoxTextureButtonStyle(DismissButtonTexturePath, CompanionDismissButtonSize, FMargin(0.08f)));
	ReplacePendingButton->SetBackgroundColor(FLinearColor::White);
	ReplacePendingButton->OnClicked.AddDynamic(this, &UGameXXKCompanionRosterWidget::HandleReplacePendingClicked);
	UTextBlock* DismissButtonText = MakeText(WidgetTree, NSLOCTEXT("GameXXKCompanionRoster", "ReplacePendingAction", "遣散"), 16, FLinearColor(0.95f, 0.90f, 0.80f, 1.0f));
	DismissButtonText->SetJustification(ETextJustify::Center);
	ReplacePendingButton->AddChild(DismissButtonText);
	AddCanvasChild(FrameCanvas, ReplacePendingButton, CompanionDismissButtonPos, CompanionDismissButtonSize);

	// Shared PSD paper tooltip for cards and avatar hover names; it never intercepts input.
	CardTooltipPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CompanionRosterCardTooltipPanel"));
	CardTooltipPanel->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, CardTooltipSize));
	CardTooltipPanel->SetBrushColor(FLinearColor::White);
	CardTooltipPanel->SetPadding(FMargin(20.0f, 16.0f, 20.0f, 14.0f));
	CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
	CardTooltipText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		14,
		FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		TEXT("CompanionRosterCardTooltipText"));
	CardTooltipText->SetJustification(ETextJustify::Left);
	CardTooltipText->SetAutoWrapText(true);
	CardTooltipPanel->SetContent(CardTooltipText);
	if (UCanvasPanelSlot* TooltipSlot = RootCanvas->AddChildToCanvas(CardTooltipPanel))
	{
		TooltipSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		TooltipSlot->SetAlignment(FVector2D::ZeroVector);
		TooltipSlot->SetPosition(FVector2D(940.0f, 470.0f));
		TooltipSlot->SetSize(CardTooltipSize);
		TooltipSlot->SetZOrder(50);
	}
}


void UGameXXKCompanionRosterWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();
	RefreshRosterSlots();
	RefreshProfilePanel();
	RefreshRecruitmentPanel();
	RefreshDeckSummaries();
	RefreshPersonalCards();
	RefreshDeckEditorControls();
	RefreshEquipmentBackpack();
	RefreshCenterCompanionPresentation();
	RefreshBackpackTabVisibility();
	RefreshCardTooltip();
}

void UGameXXKCompanionRosterWidget::RefreshSelectedCompanionData()
{
	SelectedCompanionProfile = FGameXXKCompanionRosterProfileView();
	VisiblePersonalCardIds.Reset();
	UnlockedPersonalCardIds.Reset();
	PendingPersonalCardIds.Reset();

	const FGameXXKPermanentCompanion* SelectedCompanion = CachedRoster.FindByPredicate([this](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == SelectedCompanionId;
	});
	if (!SelectedCompanion)
	{
		return;
	}

	SelectedCompanionProfile.InstanceId = SelectedCompanion->InstanceId;
	SelectedCompanionProfile.Role = SelectedCompanion->Role;
	SelectedCompanionProfile.Level = SelectedCompanion->Level;
	SelectedCompanionProfile.Star = SelectedCompanion->Star;
	SelectedCompanionProfile.Experience = SelectedCompanion->Experience;
	SelectedCompanionProfile.ExperienceRequiredForNextLevel = FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(SelectedCompanion->Level);
	SelectedCompanionProfile.bIsActive = SelectedCompanion->bIsActive;
	FGameXXKCompanionRules::GetCompanionAttributes(
		SelectedCompanion->Role,
		SelectedCompanion->Level,
		SelectedCompanion->Star,
		FGameXXKCompanionAttributes(),
		SelectedCompanionProfile.Attributes,
		nullptr);
	VisiblePersonalCardIds = SelectedCompanion->PersonalCardIds;
	UnlockedPersonalCardIds = SelectedCompanion->UnlockedPersonalCardIds;
	PendingPersonalCardIds = SelectedCompanion->SelectedCardIds;
}

void UGameXXKCompanionRosterWidget::RefreshHeroCardData()
{
	VisibleHeroCardIds.Reset();
	UnlockedHeroCardIds.Reset();
	PendingHeroCardIds.Reset();

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}

	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetCardDefinitionsForOwner(FName(TEXT("Hero"))))
	{
		VisibleHeroCardIds.Add(Definition.Id);
	}
	UnlockedHeroCardIds = Subsystem->GetRuntimeState().CardRun.HeroUnlockedCardIds;
	PendingHeroCardIds = HeroCardSummary;
}

void UGameXXKCompanionRosterWidget::RefreshVisibleRosterPage()
{
	VisibleRosterSlotInstanceIds.Init(NAME_None, RosterPageSize);
	const int32 PageStart = CurrentRosterPage * RosterPageSize;
	for (int32 VisibleIndex = 0; VisibleIndex < RosterPageSize; ++VisibleIndex)
	{
		const int32 RosterIndex = PageStart + VisibleIndex;
		if (CachedRoster.IsValidIndex(RosterIndex))
		{
			VisibleRosterSlotInstanceIds[VisibleIndex] = CachedRoster[RosterIndex].InstanceId;
		}
	}
}

void UGameXXKCompanionRosterWidget::RefreshRosterSlots()
{
	CurrentRosterPortraitResourcePaths.Init(FString(), RosterPageSize);
	for (int32 SlotIndex = 0; SlotIndex < RosterSlotButtons.Num(); ++SlotIndex)
	{
		const FName InstanceId = VisibleRosterSlotInstanceIds.IsValidIndex(SlotIndex) ? VisibleRosterSlotInstanceIds[SlotIndex] : NAME_None;
		const FGameXXKPermanentCompanion* Companion = CachedRoster.FindByPredicate([InstanceId](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == InstanceId;
		});
		if (UGameXXKCompanionRosterSlotButton* SlotButton = RosterSlotButtons[SlotIndex])
		{
			SlotButton->SetIsEnabled(Companion != nullptr);
			// Page 18 avatar tooltip carries the partner's random display name.
			SlotButton->SetToolTipText(Companion ? ResolveCompanionDisplayName(Companion->Role, Companion->NameSeed) : FText::GetEmpty());
		}
		if (UTextBlock* Label = RosterSlotLabels.IsValidIndex(SlotIndex) ? RosterSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(FText::GetEmpty());
		}
		if (UImage* Portrait = RosterSlotPortraits.IsValidIndex(SlotIndex) ? RosterSlotPortraits[SlotIndex].Get() : nullptr)
		{
			const FString PortraitPath = Companion
				? ResolveCompanionPortraitResourcePath(Companion->Role, Companion->bIsActive)
				: FString();
			CurrentRosterPortraitResourcePaths[SlotIndex] = PortraitPath;
			if (UTexture2D* PortraitTexture = PortraitPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *PortraitPath))
			{
				Portrait->SetBrushFromTexture(PortraitTexture, true);
				Portrait->SetColorAndOpacity(FLinearColor::White);
				Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Portrait->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UBorder* SelectionBorder = RosterSlotSelectionBorders.IsValidIndex(SlotIndex) ? RosterSlotSelectionBorders[SlotIndex].Get() : nullptr)
		{
			SelectionBorder->SetVisibility(Companion && Companion->InstanceId == SelectedCompanionId ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (RosterCountText)
	{
		RosterCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d · %d / %d 页"), CachedRoster.Num(), RosterCapacity, CurrentRosterPage + 1, RosterPageCount)));
	}
	if (RosterPageLeftButton)
	{
		RosterPageLeftButton->SetIsEnabled(CurrentRosterPage > 0);
	}
	if (RosterPageRightButton)
	{
		RosterPageRightButton->SetIsEnabled(CurrentRosterPage + 1 < RosterPageCount);
	}
}

void UGameXXKCompanionRosterWidget::RefreshProfilePanel()
{
	const bool bHasSelectedCompanion = !SelectedCompanionProfile.InstanceId.IsNone();
	const FString ExperienceText = SelectedCompanionProfile.ExperienceRequiredForNextLevel > 0
		? FString::Printf(TEXT("经验  %d / %d"), SelectedCompanionProfile.Experience, SelectedCompanionProfile.ExperienceRequiredForNextLevel)
		: TEXT("经验  已满级");
	if (ProfileTitleText)
	{
		if (bHasSelectedCompanion)
		{
			const FGameXXKPermanentCompanion* Companion = CachedRoster.FindByPredicate([this](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == SelectedCompanionProfile.InstanceId;
			});
			const FText CompanionName = Companion
				? ResolveCompanionDisplayName(Companion->Role, Companion->NameSeed)
				: GetRoleDisplayName(SelectedCompanionProfile.Role);
			ProfileTitleText->SetText(FText::FromString(FString::Printf(TEXT("%s%s"),
				*CompanionName.ToString(),
				SelectedCompanionProfile.bIsActive ? TEXT(" · 已出战") : TEXT(""))));
		}
		else
		{
			ProfileTitleText->SetText(NSLOCTEXT("GameXXKCompanionRoster", "NoCompanion", "尚未招募伙伴"));
		}
	}
	if (ProfileDetailText)
	{
		ProfileDetailText->SetText(bHasSelectedCompanion
			? FText::FromString(FString::Printf(
				TEXT("职业  %s\n等级  Lv.%d\n%s\n星级  ★%d\n\n气血  %d\n攻击  %d\n防御  %d\n内力  %d"),
				*GetRoleDisplayName(SelectedCompanionProfile.Role).ToString(),
				SelectedCompanionProfile.Level,
				*ExperienceText,
				SelectedCompanionProfile.Star,
				SelectedCompanionProfile.Attributes.Health,
				SelectedCompanionProfile.Attributes.Attack,
				SelectedCompanionProfile.Attributes.Defense,
				SelectedCompanionProfile.Attributes.Mana))
			: NSLOCTEXT("GameXXKCompanionRoster", "NoCompanionDetail", "招募后可在此查看属性与个人牌组。"));
	}
	if (LoadoutStatusText)
	{
		LoadoutStatusText->SetText(bLoadoutReadOnly
			? NSLOCTEXT("GameXXKCompanionRoster", "LockedStatus", "本次路线已锁定，牌组只读")
			: FText::FromString(FString::Printf(TEXT("已选 %d / 5 张 · 升星印 %d"), PendingPersonalCardIds.Num(), SigilCount)));
	}
	if (ApplyLoadoutButton)
	{
		ApplyLoadoutButton->SetIsEnabled(!bLoadoutReadOnly && bHasSelectedCompanion && PendingPersonalCardIds.Num() == 5);
	}
	if (SetActiveButton)
	{
		SetActiveButton->SetIsEnabled(!bLoadoutReadOnly && bHasSelectedCompanion);
	}
	const bool bHasActivePermanentCompanion = CachedRoster.ContainsByPredicate([](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.bIsActive;
	});
	if (ClearActiveButton)
	{
		ClearActiveButton->SetIsEnabled(!bRecruitmentActionsReadOnly && bHasActivePermanentCompanion);
	}
	const int32 RequiredSigils = bHasSelectedCompanion ? SelectedCompanionProfile.Star : 0;
	if (PromoteStarButton)
	{
		PromoteStarButton->SetIsEnabled(!bRecruitmentActionsReadOnly
			&& bHasSelectedCompanion
			&& SelectedCompanionProfile.Star < 5
			&& SigilCount >= RequiredSigils);
	}
	if (PromoteStarButtonText)
	{
		PromoteStarButtonText->SetText(FText::FromString(bHasSelectedCompanion
			? FString::Printf(TEXT("升星 · 消耗 %d 枚升星印"), RequiredSigils)
			: TEXT("升星 · 请选择伙伴")));
	}
}

void UGameXXKCompanionRosterWidget::RefreshRecruitmentPanel()
{
	const bool bHasPendingCandidate = !PendingRecruitmentCandidate.InstanceId.IsNone();
	if (RecruitmentStatusText)
	{
		const FText DefaultStatus = bHasPendingCandidate
			? FText::FromString(FString::Printf(
				TEXT("待决定：%s · 个人牌组 %d 张\n选择左侧伙伴替换，或放弃候选。"),
				*GetRoleDisplayName(PendingRecruitmentCandidate.Role).ToString(),
				PendingRecruitmentCandidate.PersonalCardIds.Num()))
			: NSLOCTEXT("GameXXKCompanionRoster", "RecruitmentReady", "招贤会固定保存候选；满员时不会重掷。");
		RecruitmentStatusText->SetText(RecruitmentFeedback.IsEmpty() ? DefaultStatus : FText::FromString(RecruitmentFeedback));
	}
	if (RecruitButton)
	{
		RecruitButton->SetIsEnabled(!bRecruitmentActionsReadOnly && !bHasPendingCandidate);
		RecruitButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ReplacePendingButton)
	{
		// 遣散 has no full-roster requirement: any companion can be dismissed
		// freely, but the roster must keep at least one partner.
		const bool bLastCompanion = CachedRoster.Num() <= 1;
		const bool bDismissEnabled = !bRecruitmentActionsReadOnly
			&& !bLastCompanion
			&& !SelectedCompanionId.IsNone();
		ReplacePendingButton->SetIsEnabled(bDismissEnabled);
		// The approved button carries its disabled reason as a tooltip.
		FString DismissReason;
		if (bRecruitmentActionsReadOnly)
		{
			DismissReason = TEXT("本次路线已锁定，无法遣散。");
		}
		else if (bLastCompanion)
		{
			DismissReason = TEXT("至少保留一名伙伴，无法遣散。");
		}
		else if (SelectedCompanionId.IsNone())
		{
			DismissReason = TEXT("请先选择一名伙伴。");
		}
		else
		{
			DismissReason = bHasPendingCandidate
				? TEXT("遣散当前伙伴并让候选入队；装备与经验材料将返还。")
				: TEXT("遣散当前伙伴；装备将返还到背包。");
		}
		// The transient recruit feedback (recruited / duplicate-to-sigil) leads
		// the dismissal reason in the button tooltip.
		const FString TooltipText = RecruitmentFeedback.IsEmpty()
			? DismissReason
			: FString::Printf(TEXT("%s\n%s"), *RecruitmentFeedback, *DismissReason);
		ReplacePendingButton->SetToolTipText(FText::FromString(TooltipText));
	}
	if (DiscardPendingButton)
	{
		DiscardPendingButton->SetIsEnabled(!bRecruitmentActionsReadOnly && bHasPendingCandidate);
		DiscardPendingButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameXXKCompanionRosterWidget::RefreshPersonalCards()
{
	if (!PersonalCardGrid || !WidgetTree)
	{
		return;
	}

	ClearCardTooltipHoverState();
	const TArray<FName>& VisibleCardIds = bEditingHeroDeck ? VisibleHeroCardIds : VisiblePersonalCardIds;
	const TArray<FName>& UnlockedCardIds = bEditingHeroDeck ? UnlockedHeroCardIds : UnlockedPersonalCardIds;
	const TArray<FName>& PendingCardIds = bEditingHeroDeck ? PendingHeroCardIds : PendingPersonalCardIds;
	const bool bMutationLocked = bLoadoutReadOnly;
	for (int32 CardIndex = 0; CardIndex < PersonalCardButtons.Num(); ++CardIndex)
	{
		const FName CardId = VisibleCardIds.IsValidIndex(CardIndex) ? VisibleCardIds[CardIndex] : NAME_None;
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		const bool bUnlocked = !CardId.IsNone() && UnlockedCardIds.Contains(CardId);
		const bool bSelected = !CardId.IsNone() && PendingCardIds.Contains(CardId);

		if (UGameXXKCompanionRosterCardButton* Button = PersonalCardButtons[CardIndex])
		{
			Button->Configure(this, CardId, bEditingHeroDeck);
			Button->SetVisibility(CardId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
			Button->SetIsEnabled(bUnlocked && !bMutationLocked);
			Button->SetRenderOpacity(!bMutationLocked && bUnlocked ? 1.0f : 0.62f);
		}
		if (UImage* Portrait = PersonalCardPortraits.IsValidIndex(CardIndex) ? PersonalCardPortraits[CardIndex].Get() : nullptr)
		{
			const FString PortraitPath = Definition ? ResolveCardPortraitResourcePath(*Definition) : FString();
			if (UTexture2D* Texture = PortraitPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *PortraitPath))
			{
				Portrait->SetBrushFromTexture(Texture, true);
				Portrait->SetColorAndOpacity(FLinearColor::White);
				Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Portrait->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* SelectedInk = PersonalCardSelectedInks.IsValidIndex(CardIndex) ? PersonalCardSelectedInks[CardIndex].Get() : nullptr)
		{
			SelectedInk->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* CardLabel = PersonalCardNameLabels.IsValidIndex(CardIndex) ? PersonalCardNameLabels[CardIndex].Get() : nullptr)
		{
			CardLabel->SetText(Definition ? Definition->DisplayName : FText::FromName(CardId));
			// Selected card name turns white and renders above the selection ink.
			CardLabel->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor::White
				: FLinearColor(0.10f, 0.07f, 0.04f, 1.0f)));
		}
		if (UTextBlock* CostLabel = PersonalCardCostLabels.IsValidIndex(CardIndex) ? PersonalCardCostLabels[CardIndex].Get() : nullptr)
		{
			CostLabel->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: FText::FromString(FString::Printf(TEXT("%d气"), Definition->EnergyCost)));
		}
		if (UTextBlock* ManaCostLabel = PersonalCardManaCostLabels.IsValidIndex(CardIndex) ? PersonalCardManaCostLabels[CardIndex].Get() : nullptr)
		{
			ManaCostLabel->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: FText::FromString(FString::Printf(TEXT("%d内"), Definition->ManaCost)));
		}
		if (UImage* LockedIcon = PersonalCardLockedIcons.IsValidIndex(CardIndex) ? PersonalCardLockedIcons[CardIndex].Get() : nullptr)
		{
			LockedIcon->SetVisibility(!bUnlocked && !CardId.IsNone()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		}
		// Hero-style paper tooltip: name + full effect description (with the
		// unavailable reason / interaction hint when applicable).
		if (UBorder* TooltipFrame = PersonalCardTooltipFrames.IsValidIndex(CardIndex) ? PersonalCardTooltipFrames[CardIndex].Get() : nullptr)
		{
			TooltipFrame->SetVisibility(CardId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		}
		if (UTextBlock* TooltipName = PersonalCardTooltipNameBlocks.IsValidIndex(CardIndex) ? PersonalCardTooltipNameBlocks[CardIndex].Get() : nullptr)
		{
			TooltipName->SetText(Definition ? Definition->DisplayName : FText::FromName(CardId));
		}
		if (UTextBlock* TooltipDetail = PersonalCardTooltipDetailBlocks.IsValidIndex(CardIndex) ? PersonalCardTooltipDetailBlocks[CardIndex].Get() : nullptr)
		{
			FString TooltipText;
			if (Definition)
			{
				FGameXXKCardTooltipContext Context;
				const int32 RequiredDeckSize = bEditingHeroDeck ? 8 : 5;
				if (bLoadoutReadOnly)
				{
					Context.UnavailableReason = TEXT("本次路线已锁定，牌组只读。");
				}
				else if (!bUnlocked)
				{
					Context.UnavailableReason = TEXT("此牌尚未解锁。");
				}
				else if (!bSelected && PendingCardIds.Num() >= RequiredDeckSize)
				{
					Context.UnavailableReason = bEditingHeroDeck
						? TEXT("主角牌组已满（8 张），无法编入此牌。")
						: TEXT("该伙伴个人牌组已满（5 张），无法编入此牌。");
				}
				else
				{
					Context.InteractionResult = bEditingHeroDeck
						? TEXT("点击后编入/移出主角牌组；需保持 8 张。")
						: TEXT("点击后编入/移出该伙伴个人牌组；需保持 5 张。");
				}
				TooltipText = GameXXKCardText::DescribeTooltip(*Definition, Definition->BaseQuality, nullptr, Context);
			}
			TooltipDetail->SetText(FText::FromString(TooltipText));
			if (PersonalCardTooltipTexts.IsValidIndex(CardIndex))
			{
				PersonalCardTooltipTexts[CardIndex] = TooltipText;
			}
		}
	}
}

void UGameXXKCompanionRosterWidget::RefreshEquipmentBackpack()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();

	// Six-slot snapshot drives the replaced-slot comparison rows inside the
	// warehouse tooltips, so bind it before the warehouse window fills.
	CharacterBackpackModel.Bind(Subsystem, SelectedCompanionId);
	const TArray<FGameXXKCharacterBackpackSlotView> EquippedSlots = CharacterBackpackModel.GetSixSlotSnapshot();

	// Page 03 content source identical to the hero backpack: shared warehouse
	// equipment (under 全部/装备) plus the player inventory (per category).
	struct FCompanionBackpackRuntimeEntry
	{
		FName ItemId = NAME_None;
		FName EquipmentInstanceId = NAME_None;
		int32 Quantity = 0;
		FString IconPath;
		FText DisplayName;
		FText DetailText;

		bool IsEquipmentInstance() const
		{
			return !EquipmentInstanceId.IsNone();
		}
	};
	TArray<FCompanionBackpackRuntimeEntry> BackpackEntries;
	if (Subsystem && (ActiveBackpackFilter == 0 || ActiveBackpackFilter == 1))
	{
		TArray<FName> WarehouseInstanceIds;
		Subsystem->GetEquipmentWarehouseSnapshot(WarehouseInstanceIds);
		for (const FName InstanceId : WarehouseInstanceIds)
		{
			const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, InstanceId);
			const FGameXXKEquipmentDefinition* Definition = Instance
				? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
				: nullptr;
			if (!Instance || !Definition)
			{
				continue;
			}
			FCompanionBackpackRuntimeEntry Entry;
			Entry.EquipmentInstanceId = InstanceId;
			Entry.IconPath = Definition->IconSoftPath.ToString();
			Entry.DisplayName = Definition->DisplayName;
			Entry.DetailText = BuildCompanionEquipmentInstanceDetail(Subsystem, *Instance, *Definition, SelectedCompanionId);
			BackpackEntries.Add(MoveTemp(Entry));
		}
	}
	if (Subsystem)
	{
		struct FCompanionInventoryEntry
		{
			FName ItemId = NAME_None;
			int32 Quantity = 0;
			int32 KnownIndex = INDEX_NONE;
			FGameXXKItemDef Definition;
		};
		TArray<FCompanionInventoryEntry> InventoryEntries;
		const TArray<FName> KnownItemIds = UGameXXKMVPRules::GetKnownItemIds();
		for (const TPair<FName, int32>& ItemEntry : Subsystem->GetRuntimeState().Inventory)
		{
			bool bFound = false;
			const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(ItemEntry.Key, bFound);
			if (ItemEntry.Value > 0 && bFound && MatchesCompanionFilter(Definition.Kind, ActiveBackpackFilter))
			{
				FCompanionInventoryEntry InventoryEntry;
				InventoryEntry.ItemId = ItemEntry.Key;
				InventoryEntry.Quantity = ItemEntry.Value;
				InventoryEntry.KnownIndex = KnownItemIds.IndexOfByKey(ItemEntry.Key);
				InventoryEntry.Definition = Definition;
				InventoryEntries.Add(MoveTemp(InventoryEntry));
			}
		}
		InventoryEntries.Sort([](const FCompanionInventoryEntry& A, const FCompanionInventoryEntry& B)
		{
			if (A.KnownIndex != INDEX_NONE || B.KnownIndex != INDEX_NONE)
			{
				if (A.KnownIndex == INDEX_NONE) { return false; }
				if (B.KnownIndex == INDEX_NONE) { return true; }
				return A.KnownIndex < B.KnownIndex;
			}
			return A.ItemId.LexicalLess(B.ItemId);
		});
		for (const FCompanionInventoryEntry& InventoryEntry : InventoryEntries)
		{
			FCompanionBackpackRuntimeEntry Entry;
			Entry.ItemId = InventoryEntry.ItemId;
			Entry.Quantity = InventoryEntry.Quantity;
			Entry.IconPath = CompanionResolveItemIconTexturePath(InventoryEntry.ItemId);
			Entry.DisplayName = InventoryEntry.Definition.DisplayName;
			Entry.DetailText = FText::FromString(CompanionItemStatsText(InventoryEntry.Definition, Subsystem->GetItemEnhancementLevel(InventoryEntry.ItemId)));
			BackpackEntries.Add(MoveTemp(Entry));
		}
	}

	CurrentBackpackSlotItemIds.Reset();
	CurrentBackpackSlotEquipmentInstanceIds.Reset();
	CurrentBackpackSlotQuantities.Reset();
	CurrentBackpackSlotIconPaths.Reset();
	VisibleEquipmentWarehouseInstanceIds.Reset();
	for (int32 WarehouseIndex = 0; WarehouseIndex < EquipmentWarehouseSlotButtons.Num(); ++WarehouseIndex)
	{
		const FCompanionBackpackRuntimeEntry* Entry = BackpackEntries.IsValidIndex(WarehouseIndex) ? &BackpackEntries[WarehouseIndex] : nullptr;
		const FName ItemId = Entry ? Entry->ItemId : NAME_None;
		const FName EquipmentInstanceId = Entry ? Entry->EquipmentInstanceId : NAME_None;
		const bool bHasItem = !ItemId.IsNone() || !EquipmentInstanceId.IsNone();
		CurrentBackpackSlotItemIds.Add(ItemId);
		CurrentBackpackSlotEquipmentInstanceIds.Add(EquipmentInstanceId);
		CurrentBackpackSlotQuantities.Add(Entry ? Entry->Quantity : 0);
		CurrentBackpackSlotIconPaths.Add(Entry ? Entry->IconPath : FString());
		VisibleEquipmentWarehouseInstanceIds.Add(EquipmentInstanceId);

		if (UGameXXKCompanionEquipmentSlotButton* SlotButton = EquipmentWarehouseSlotButtons[WarehouseIndex])
		{
			SlotButton->SetIsEnabled(bHasItem && !SelectedCompanionId.IsNone() && !bLoadoutReadOnly);
		}
		if (UBorder* Tooltip = BackpackTooltipFrames.IsValidIndex(WarehouseIndex) ? BackpackTooltipFrames[WarehouseIndex].Get() : nullptr)
		{
			// Empty slots must not show a tiny blank tooltip paper on hover.
			Tooltip->SetVisibility(bHasItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* Label = BackpackSlotLabels.IsValidIndex(WarehouseIndex) ? BackpackSlotLabels[WarehouseIndex].Get() : nullptr)
		{
			Label->SetText(bHasItem && Entry->Quantity > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), Entry->Quantity)) : FText::GetEmpty());
		}
		if (UImage* SlotIcon = EquipmentWarehouseSlotIcons.IsValidIndex(WarehouseIndex) ? EquipmentWarehouseSlotIcons[WarehouseIndex].Get() : nullptr)
		{
			if (!Entry || Entry->IconPath.IsEmpty())
			{
				SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
			else if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Entry->IconPath))
			{
				SlotIcon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = SlotIcon->GetBrush();
				Brush.ImageSize = CompanionBackpackIconSize;
				SlotIcon->SetBrush(Brush);
				SlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UTextBlock* TooltipName = BackpackTooltipNameTextBlocks.IsValidIndex(WarehouseIndex) ? BackpackTooltipNameTextBlocks[WarehouseIndex].Get() : nullptr)
		{
			TooltipName->SetText(Entry ? Entry->DisplayName : FText::GetEmpty());
		}
		if (UTextBlock* TooltipDetail = BackpackTooltipDetailTextBlocks.IsValidIndex(WarehouseIndex) ? BackpackTooltipDetailTextBlocks[WarehouseIndex].Get() : nullptr)
		{
			TooltipDetail->SetText(Entry ? Entry->DetailText : FText::GetEmpty());
		}
		// Comparison rows: only when this warehouse item would replace an
		// occupied slot do we show the red-gain / green-loss stat deltas.
		if (TArray<TObjectPtr<UTextBlock>>* CompareRows = BackpackCompareTextBlocks.IsValidIndex(WarehouseIndex) ? &BackpackCompareTextBlocks[WarehouseIndex] : nullptr)
		{
			int32 CompareRowIndex = 0;
			auto ShowCompareRow = [&CompareRows, &CompareRowIndex](const FString& Label, const int32 Delta)
			{
				if (CompareRowIndex >= CompareRows->Num() || Delta == 0)
				{
					return;
				}
				if (UTextBlock* Row = (*CompareRows)[CompareRowIndex])
				{
					Row->SetText(FText::FromString(FString::Printf(TEXT("%s %+d"), *Label, Delta)));
					Row->SetColorAndOpacity(FSlateColor(Delta > 0
						? FLinearColor(0.85f, 0.15f, 0.15f, 1.0f)
						: FLinearColor(0.10f, 0.65f, 0.25f, 1.0f)));
					Row->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				++CompareRowIndex;
			};
			const FGameXXKEquipmentInstance* Instance = Subsystem && Entry && Entry->IsEquipmentInstance()
				? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, Entry->EquipmentInstanceId)
				: nullptr;
			const FGameXXKEquipmentDefinition* Definition = Instance
				? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
				: nullptr;
			const bool bSlotOccupied = Definition
				&& EquippedSlots.ContainsByPredicate([Definition](const FGameXXKCharacterBackpackSlotView& SlotView)
				{
					return SlotView.Slot == Definition->Slot && !SlotView.EquippedInstanceId.IsNone();
				});
			if (bSlotOccupied && Subsystem)
			{
				FGameXXKEquipmentTooltipSnapshot CompareSnapshot;
				if (Subsystem->GetEquipmentTooltipSnapshot(Entry->EquipmentInstanceId, SelectedCompanionId, CompareSnapshot))
				{
					ShowCompareRow(TEXT("攻击"), CompareSnapshot.CharacterStatDeltas.Attack);
					ShowCompareRow(TEXT("防御"), CompareSnapshot.CharacterStatDeltas.Defense);
					ShowCompareRow(TEXT("气血"), CompareSnapshot.CharacterStatDeltas.MaxHealth);
					ShowCompareRow(TEXT("真气"), CompareSnapshot.CharacterStatDeltas.MaxMana);
					ShowCompareRow(TEXT("身法"), CompareSnapshot.CharacterStatDeltas.Speed);
				}
			}
			for (; CompareRowIndex < CompareRows->Num(); ++CompareRowIndex)
			{
				if (UTextBlock* Row = (*CompareRows)[CompareRowIndex])
				{
					Row->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < CompanionEquipmentSlotButtons.Num(); ++SlotIndex)
	{
		UGameXXKCompanionEquipmentSlotButton* SlotButton = CompanionEquipmentSlotButtons[SlotIndex];
		UImage* SlotIcon = CompanionEquipmentSlotIcons.IsValidIndex(SlotIndex) ? CompanionEquipmentSlotIcons[SlotIndex].Get() : nullptr;
		const FGameXXKCharacterBackpackSlotView* SlotView = EquippedSlots.IsValidIndex(SlotIndex) ? &EquippedSlots[SlotIndex] : nullptr;
		const FGameXXKEquipmentInstance* Instance = Subsystem && SlotView && !SlotView->EquippedInstanceId.IsNone()
			? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, SlotView->EquippedInstanceId)
			: nullptr;
		const FGameXXKEquipmentDefinition* Definition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;
		if (SlotButton)
		{
			SlotButton->SetIsEnabled(SlotView != nullptr && !SelectedCompanionId.IsNone());
		}
		if (UBorder* Tooltip = CompanionEquipmentTooltipFrames.IsValidIndex(SlotIndex) ? CompanionEquipmentTooltipFrames[SlotIndex].Get() : nullptr)
		{
			Tooltip->SetVisibility(Definition ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* TooltipName = CompanionEquipmentTooltipNameBlocks.IsValidIndex(SlotIndex) ? CompanionEquipmentTooltipNameBlocks[SlotIndex].Get() : nullptr)
		{
			TooltipName->SetText(Definition ? Definition->DisplayName : (SlotView ? GetCompanionEquipmentSlotText(SlotView->Slot) : FText::GetEmpty()));
		}
		if (UTextBlock* TooltipDetail = CompanionEquipmentTooltipDetailBlocks.IsValidIndex(SlotIndex) ? CompanionEquipmentTooltipDetailBlocks[SlotIndex].Get() : nullptr)
		{
			TooltipDetail->SetText(Definition && Instance
				? BuildCompanionEquipmentInstanceDetail(Subsystem, *Instance, *Definition, SelectedCompanionId)
				: FText::GetEmpty());
		}
		if (SlotIcon)
		{
			if (Definition && !Definition->IconSoftPath.IsNull())
			{
				if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Definition->IconSoftPath.ToString()))
				{
					SlotIcon->SetBrushFromTexture(Texture, true);
					SlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
					continue;
				}
			}
			SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Selection inks follow the last clicked slot (page 03 convention).
	if (BackpackSelectionInk)
	{
		const bool bBackpackSelection = SelectedWarehouseSlotIndex >= 0 && SelectedWarehouseSlotIndex < EquipmentBackpackViewportSlotCount;
		BackpackSelectionInk->SetVisibility(bBackpackSelection ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bBackpackSelection)
		{
			const int32 Column = SelectedWarehouseSlotIndex % EquipmentBackpackColumnCount;
			const int32 Row = SelectedWarehouseSlotIndex / EquipmentBackpackColumnCount;
			const FVector2D InkPosition = CompanionSelectionInkPos
				+ FVector2D(Column * CompanionBackpackSlotPitch.X, Row * CompanionBackpackSlotPitch.Y);
			if (UCanvasPanelSlot* InkSlot = Cast<UCanvasPanelSlot>(BackpackSelectionInk->Slot))
			{
				InkSlot->SetPosition(InkPosition);
			}
		}
	}
	if (EquipmentSelectionInk)
	{
		const bool bEquipmentSelection = SelectedEquippedSlotIndex >= 0 && SelectedEquippedSlotIndex < CompanionEquipmentSlotButtons.Num();
		EquipmentSelectionInk->SetVisibility(bEquipmentSelection ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bEquipmentSelection)
		{
			if (UCanvasPanelSlot* InkSlot = Cast<UCanvasPanelSlot>(EquipmentSelectionInk->Slot))
			{
				// Same small bracket offset as the hero backpack's equipped-slot ink.
				InkSlot->SetPosition(CompanionEquipmentFramePositions[SelectedEquippedSlotIndex] + FVector2D(-7.0f, -16.0f));
			}
		}
	}
	UpdateEquipmentScrollbarThumb();
}

void UGameXXKCompanionRosterWidget::RefreshBackpackTabVisibility()
{
	const bool bAttributesTab = ActiveBackpackTab == EGameXXKCompanionBackpackTab::Attributes;
	const bool bEquipmentTab = ActiveBackpackTab == EGameXXKCompanionBackpackTab::Equipment;
	const bool bCardsTab = ActiveBackpackTab == EGameXXKCompanionBackpackTab::Cards;
	const bool bTalentsTab = ActiveBackpackTab == EGameXXKCompanionBackpackTab::Talents;
	const bool bTitlesTab = ActiveBackpackTab == EGameXXKCompanionBackpackTab::Titles;
	const bool bNotOpenTab = bTalentsTab || bTitlesTab;
	const bool bShowBody = bAttributesTab || bNotOpenTab;

	if (AttributesBodyPanel)
	{
		AttributesBodyPanel->SetVisibility(bShowBody ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EquipmentBackpackPanel)
	{
		EquipmentBackpackPanel->SetVisibility(bEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	// The warehouse window lives directly on the content canvas (page 03
	// structure), so it must hide with the equipment tab as well.
	if (EquipmentBackpackScrollBox)
	{
		EquipmentBackpackScrollBox->SetVisibility(bEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (PersonalDeckPanel)
	{
		PersonalDeckPanel->SetVisibility(bCardsTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (DeckCaptionText)
	{
		DeckCaptionText->SetVisibility(bCardsTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (UButton* FilterButton : BackpackFilterButtons)
	{
		if (FilterButton)
		{
			FilterButton->SetVisibility(bEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	const bool bShowScrollbar = bEquipmentTab || bCardsTab;
	if (ScrollbarTrackImage)
	{
		ScrollbarTrackImage->SetVisibility(bShowScrollbar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (InventoryScrollbarThumb)
	{
		InventoryScrollbarThumb->SetVisibility(bShowScrollbar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	// Selection inks belong to the equipment tab only; RefreshEquipmentBackpack
	// restores their per-selection visibility when the tab becomes active again.
	if (BackpackSelectionInk)
	{
		BackpackSelectionInk->SetVisibility(bEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EquipmentSelectionInk)
	{
		EquipmentSelectionInk->SetVisibility(bEquipmentTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 天赋/称号 render the approved "尚未开放" body text like the hero backpack.
	if (ProfileTitleText && bNotOpenTab)
	{
		ProfileTitleText->SetText(bTalentsTab
			? NSLOCTEXT("GameXXKCompanionRoster", "TalentsTitle", "天赋")
			: NSLOCTEXT("GameXXKCompanionRoster", "TitlesTitle", "称号"));
	}
	if (ProfileDetailText && bNotOpenTab)
	{
		ProfileDetailText->SetText(NSLOCTEXT("GameXXKCompanionRoster", "NotOpenYet", "尚未开放"));
	}

	// Tab selected texture (004_tab_2) vs normal (003_tab_1).
	auto ApplyTabStyle = [this](UButton* TabButton, const bool bSelected)
	{
		if (TabButton)
		{
			TabButton->SetStyle(MakeBoxTextureButtonStyle(
				bSelected ? TabSelectedTexturePath : TabNormalTexturePath,
				CompanionTabSize,
				FMargin(0.08f)));
		}
	};
	ApplyTabStyle(AttributesTabButton, bAttributesTab);
	ApplyTabStyle(EquipmentBackpackTabButton, bEquipmentTab);
	ApplyTabStyle(CardBackpackTabButton, bCardsTab);
	ApplyTabStyle(TalentsTabButton, bTalentsTab);
	ApplyTabStyle(TitlesTabButton, bTitlesTab);

	// Selected filter text turns gold (page 03 convention).
	for (int32 FilterIndex = 0; FilterIndex < BackpackFilterTextBlocks.Num(); ++FilterIndex)
	{
		if (UTextBlock* FilterText = BackpackFilterTextBlocks[FilterIndex])
		{
			FilterText->SetColorAndOpacity(FSlateColor(FilterIndex == ActiveBackpackFilter
				? FLinearColor(0.85f, 0.62f, 0.18f, 1.0f)
				: FLinearColor(0.20f, 0.14f, 0.09f, 1.0f)));
		}
	}

	// Keep the shared PSD thumb parked at the active tab's scroll origin.
	if (bEquipmentTab)
	{
		UpdateEquipmentScrollbarThumb();
	}
	else if (bCardsTab)
	{
		UpdatePersonalCardScrollbarThumb();
	}
}

void UGameXXKCompanionRosterWidget::RefreshCenterCompanionPresentation()
{
	const FGameXXKPermanentCompanion* Companion = CachedRoster.FindByPredicate([this](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == SelectedCompanionId;
	});
	if (CenterCompanionNameText)
	{
		CenterCompanionNameText->SetText(Companion
			? ResolveCompanionDisplayName(Companion->Role, Companion->NameSeed)
			: FText::GetEmpty());
	}
	if (CenterCompanionPortraitImage)
	{
		const FString FullBodyPath = Companion ? ResolveCompanionFullBodyResourcePath(Companion->Role) : FString();
		if (UTexture2D* Texture = FullBodyPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *FullBodyPath))
		{
			CenterCompanionPortraitImage->SetBrushFromTexture(Texture, true);
			CenterCompanionPortraitImage->SetColorAndOpacity(FLinearColor::White);
			CenterCompanionPortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			CenterCompanionPortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKCompanionRosterWidget::UpdateEquipmentScrollbarThumb()
{
	if (!InventoryScrollbarThumb || !EquipmentBackpackScrollBox)
	{
		return;
	}
	const float MaxOffset = EquipmentBackpackScrollBox->GetScrollOffsetOfEnd();
	UE_LOG(LogTemp, Warning, TEXT("[WarehouseGrid] scrollOffsetOfEnd=%.1f viewport=%s"),
		MaxOffset,
		*EquipmentBackpackScrollBox->GetCachedGeometry().GetLocalSize().ToString());
	const float Offset = EquipmentBackpackScrollBox->GetScrollOffset();
	const float ThumbTravel = CompanionScrollbarTrackSize.Y - CompanionScrollbarThumbSize.Y;
	const float Ratio = MaxOffset > 0.0f ? FMath::Clamp(Offset / MaxOffset, 0.0f, 1.0f) : 0.0f;
	const FVector2D ThumbPosition = CompanionScrollbarThumbTop + FVector2D(0.0f, Ratio * ThumbTravel);
	if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(InventoryScrollbarThumb->Slot))
	{
		ThumbSlot->SetPosition(ThumbPosition);
	}
}

void UGameXXKCompanionRosterWidget::UpdatePersonalCardScrollbarThumb()
{
	if (!InventoryScrollbarThumb || !PersonalCardScroll)
	{
		return;
	}
	const float MaxOffset = PersonalCardScroll->GetScrollOffsetOfEnd();
	const float Offset = PersonalCardScroll->GetScrollOffset();
	const float ThumbTravel = CompanionScrollbarTrackSize.Y - CompanionScrollbarThumbSize.Y;
	const float Ratio = MaxOffset > 0.0f ? FMath::Clamp(Offset / MaxOffset, 0.0f, 1.0f) : 0.0f;
	const FVector2D ThumbPosition = CompanionScrollbarThumbTop + FVector2D(0.0f, Ratio * ThumbTravel);
	if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(InventoryScrollbarThumb->Slot))
	{
		ThumbSlot->SetPosition(ThumbPosition);
	}
}

void UGameXXKCompanionRosterWidget::HandleEquipmentBackpackScrolled(float CurrentOffset)
{
	(void)CurrentOffset;
	UpdateEquipmentScrollbarThumb();
}

void UGameXXKCompanionRosterWidget::HandlePersonalCardScrolled(float CurrentOffset)
{
	(void)CurrentOffset;
	UpdatePersonalCardScrollbarThumb();
}

void UGameXXKCompanionRosterWidget::SetActiveBackpackTab(const EGameXXKCompanionBackpackTab Tab)
{
	ActiveBackpackTab = Tab;
	bEditingHeroDeck = false;
	ClearCardTooltipHoverState();
	RefreshProfilePanel();
	RefreshBackpackTabVisibility();
	// Returning to the equipment tab restores the ink/selection state that the
	// visibility pass collapsed while another tab was active.
	if (Tab == EGameXXKCompanionBackpackTab::Equipment)
	{
		RefreshEquipmentBackpack();
	}
}

void UGameXXKCompanionRosterWidget::HandleAttributesTabClicked()
{
	SetActiveBackpackTab(EGameXXKCompanionBackpackTab::Attributes);
}

void UGameXXKCompanionRosterWidget::HandleTalentsTabClicked()
{
	SetActiveBackpackTab(EGameXXKCompanionBackpackTab::Talents);
}

void UGameXXKCompanionRosterWidget::HandleTitlesTabClicked()
{
	SetActiveBackpackTab(EGameXXKCompanionBackpackTab::Titles);
}

void UGameXXKCompanionRosterWidget::HandleBackpackFilterClicked(const int32 FilterIndex)
{
	if (FilterIndex < 0 || FilterIndex >= BackpackFilterButtons.Num())
	{
		return;
	}
	ActiveBackpackFilter = FilterIndex;
	for (int32 Index = 0; Index < BackpackFilterTextBlocks.Num(); ++Index)
	{
		if (UTextBlock* FilterText = BackpackFilterTextBlocks[Index])
		{
			FilterText->SetColorAndOpacity(FSlateColor(Index == ActiveBackpackFilter
				? FLinearColor(0.85f, 0.62f, 0.18f, 1.0f)
				: FLinearColor(0.20f, 0.14f, 0.09f, 1.0f)));
		}
	}
}

void UGameXXKCompanionRosterWidget::RefreshDeckSummaries()
{
	if (HeroDeckSummaryText)
	{
		HeroDeckSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("主角牌组  %d / 8\n%s"),
			HeroCardSummary.Num(),
			*BuildCardSummary(HeroCardSummary))));
	}
	if (TaskNpcDeckSummaryText)
	{
		const FString NpcTitle = TaskNpcCardSummary.NpcId.IsNone() ? TEXT("任务 NPC 未加入") : TaskNpcCardSummary.NpcId.ToString();
		TaskNpcDeckSummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("任务 NPC · %s\n固定支援牌组  %d / 3 · 只读\n%s"),
			*NpcTitle,
			TaskNpcCardSummary.SelectedCardIds.Num(),
			*BuildCardSummary(TaskNpcCardSummary.SelectedCardIds))));
	}
}

void UGameXXKCompanionRosterWidget::RefreshDeckEditorControls()
{
	if (DeckCaptionText)
	{
		DeckCaptionText->SetText(bEditingHeroDeck
			? NSLOCTEXT("GameXXKCompanionRoster", "HeroDeckCaption", "主角牌组（12 张，编入 8 张）")
			: NSLOCTEXT("GameXXKCompanionRoster", "PersonalDeckCaption", "个人牌组（12 张，编入 5 张）"));
	}
	if (HeroDeckToggleButtonText)
	{
		HeroDeckToggleButtonText->SetText(bEditingHeroDeck
			? NSLOCTEXT("GameXXKCompanionRoster", "ReturnPersonalDeck", "查看伙伴牌组")
			: NSLOCTEXT("GameXXKCompanionRoster", "OpenHeroDeck", "编辑主角牌组"));
	}
	if (ApplyHeroLoadoutButton)
	{
		ApplyHeroLoadoutButton->SetVisibility(bEditingHeroDeck ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ApplyHeroLoadoutButton->SetIsEnabled(bEditingHeroDeck && !bLoadoutReadOnly && PendingHeroCardIds.Num() == 8);
	}
	if (HeroDeckStatusText)
	{
		HeroDeckStatusText->SetVisibility(bEditingHeroDeck ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		HeroDeckStatusText->SetText(bEditingHeroDeck
			? (bLoadoutReadOnly
				? NSLOCTEXT("GameXXKCompanionRoster", "HeroDeckLocked", "路线已锁定 · 只读")
				: FText::FromString(FString::Printf(TEXT("已选 %d / 8 张"), PendingHeroCardIds.Num())))
			: FText::GetEmpty());
	}
}

void UGameXXKCompanionRosterWidget::RefreshCardTooltip()
{
	// Card tooltips are hero-style per-card paper tips (SetToolTip) filled during
	// RefreshPersonalCards; the old floating panel is retired.
}


void UGameXXKCompanionRosterWidget::ClearCardTooltipHoverState()
{
	HoveredCardTooltipId = NAME_None;
	bHoveredCardTooltipIsHeroDeck = false;
	if (CardTooltipPanel)
	{
		CardTooltipPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CardTooltipText)
	{
		CardTooltipText->SetText(FText::GetEmpty());
	}
}

bool UGameXXKCompanionRosterWidget::IsCurrentSelectedCompanion(const FName InstanceId) const
{
	return !InstanceId.IsNone() && CachedRoster.ContainsByPredicate([InstanceId](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == InstanceId;
	});
}

bool UGameXXKCompanionRosterWidget::ChangeRosterPage(const int32 Direction)
{
	if (Direction == 0)
	{
		return false;
	}
	const int32 NewPage = FMath::Clamp(CurrentRosterPage + FMath::Sign(Direction), 0, RosterPageCount - 1);
	if (NewPage == CurrentRosterPage)
	{
		return false;
	}
	CurrentRosterPage = NewPage;
	bRosterPageInitialized = true;
	RefreshVisibleRosterPage();
	RefreshRosterSlots();
	return true;
}

void UGameXXKCompanionRosterWidget::HandleRosterPageLeftClicked()
{
	ChangeRosterPage(-1);
}

void UGameXXKCompanionRosterWidget::HandleRosterPageRightClicked()
{
	ChangeRosterPage(1);
}

void UGameXXKCompanionRosterWidget::HandleApplyLoadoutClicked()
{
	ApplySelectedCompanionCardLoadout();
}

void UGameXXKCompanionRosterWidget::HandleSetActiveClicked()
{
	SetSelectedCompanionAsActive();
}

void UGameXXKCompanionRosterWidget::HandleClearActiveClicked()
{
	ClearActivePermanentCompanion();
}

void UGameXXKCompanionRosterWidget::HandleHeroDeckToggleClicked()
{
	if (bEditingHeroDeck)
	{
		bEditingHeroDeck = false;
		RefreshProgrammaticLayout();
		return;
	}

	OpenHeroDeckEditor();
}

void UGameXXKCompanionRosterWidget::HandleApplyHeroLoadoutClicked()
{
	ApplyHeroCardLoadout();
}

void UGameXXKCompanionRosterWidget::HandleRecruitClicked()
{
	BeginRandomRecruitment();
}

void UGameXXKCompanionRosterWidget::HandleReplacePendingClicked()
{
	ResolvePendingRecruitmentWithSelectedCompanion();
}

void UGameXXKCompanionRosterWidget::HandleDiscardPendingClicked()
{
	DiscardPendingRecruitment();
}

void UGameXXKCompanionRosterWidget::HandlePromoteStarClicked()
{
	PromoteSelectedCompanionStar();
}

void UGameXXKCompanionRosterWidget::HandleEquipmentBackpackTabClicked()
{
	OpenEquipmentBackpackTabForTest();
}

void UGameXXKCompanionRosterWidget::HandleCardBackpackTabClicked()
{
	OpenCardBackpackTabForTest();
}

void UGameXXKCompanionRosterWidget::HandleCloseClicked()
{
	if (AGameXXKMVPPlayerController* Controller = Cast<AGameXXKMVPPlayerController>(GetOwningPlayer()))
	{
		Controller->CloseCompanionRoster();
		return;
	}
	SetVisibility(ESlateVisibility::Collapsed);
}
