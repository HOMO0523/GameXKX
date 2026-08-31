#include "UI/GameXXKInventoryWindowWidget.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKCardText.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKGemRules.h"

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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKTalentRules.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryItemPresentation.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"

namespace
{
	class SGameXXKInventorySlotButton final : public SButton
	{
	public:
		using FArguments = SButton::FArguments;

		void Construct(const FArguments& InArgs, UGameXXKInventorySlotButton* InOwner)
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
		TWeakObjectPtr<UGameXXKInventorySlotButton> Owner;
	};

	// Master V1 page 03/18 WindowControls: 74x74 ink close glyph at (1652,201).
	const FVector2D CloseButtonSize(74.0f, 74.0f);
	const FVector2D BackpackSlotSize(110.0f, 116.0f);
	const FVector2D BackpackIconSize(64.0f, 64.0f);
	const FVector2D EquipmentSlotSize(118.0f, 124.0f);
	const FVector2D ActionButtonSize(206.0f, 64.0f);
	const FVector2D CharacterTabSize(105.0f, 62.0f);
	// Page 18 hero deck cards are 137x190 in a 3-column grid.
	const FVector2D HeroDeckCardSize(137.0f, 190.0f);
	// The battle-card portrait cut is 190x228 inside a 206x285 card.  The
	// inventory card is exactly two-thirds scale, so preserve that inset here
	// instead of stretching the portrait over the shared parchment frame.
	const FVector2D HeroDeckPortraitSize(127.0f, 152.0f);
	const int32 BackpackViewportSlotCount = 20;
	const int32 BackpackStorageCapacity = FGameXXKEquipmentRules::WarehouseCapacity;
	const int32 BackpackColumns = 4;

	// Master V1 page 03 absolute geometry (origin 6120,0). The protagonist
	// backpack paper window sits over the town shell; the 20 visible cells
	// are a 4x5 window into the 200-slot warehouse.
	// The parchment alone grows five percent around its original center.  Every
	// interactive control remains on the fixed 1920x1080 authored coordinates.
	const FVector2D InventoryPaperPos(274.75f, 151.775f);
	const FVector2D InventoryPaperSize(1522.5f, 891.45f);
	const FVector2D BackpackViewportPos(1135.0f, 300.0f);
	const FVector2D BackpackViewportSize(488.0f, 650.0f);      // 4 cols x 5 rows
	const FVector2D BackpackSlotPitch(122.0f, 130.0f);          // page 03 grid pitch
	const FMargin BackpackSlotPadding(6.0f, 7.0f);
	const FVector2D BackpackContentOffset(-6.0f, -7.0f);        // keeps slot 1 at (1135,300)
	const FVector2D BackpackGridSize(488.0f, 6500.0f);          // 50 rows x pitch 130
	const FVector2D InventoryScrollbarPos(1642.0f, 303.0f);
	const FVector2D InventoryScrollbarSize(30.0f, 633.0f);
	const FVector2D InventoryScrollbarTrackPos(1647.0f, 303.0f);
	const FVector2D InventoryScrollbarTrackSize(19.0f, 633.0f);
	const FVector2D InventoryScrollbarThumbTop(1642.0f, 323.0f);
	const FVector2D InventoryScrollbarThumbSize(30.0f, 126.0f);
	const FVector2D BackpackSelectionInkPos(1128.0f, 284.0f);
	const FVector2D BackpackSelectionInkSize(126.0f, 42.0f);
	const FVector2D BackpackFilterRowPos(1142.0f, 240.0f);
	const float BackpackFilterRowPitch = 80.0f;
	const FVector2D BackpackFilterRowSize(80.0f, 26.0f);
	const FVector2D DecomposeButtonSize(105.0f, 62.0f);
	const FVector2D EquipmentFramePositions[6] = {
		FVector2D(420.0f, 340.0f), FVector2D(420.0f, 515.0f), FVector2D(420.0f, 690.0f),
		FVector2D(930.0f, 340.0f), FVector2D(930.0f, 515.0f), FVector2D(930.0f, 690.0f)};
	const FName WeaponSlotId(TEXT("Weapon"));
	const FName HeadSlotId(TEXT("Head"));
	const FName ArmorSlotId(TEXT("Armor"));
	const FName BeltSlotId(TEXT("Belt"));
	const FName ShoesSlotId(TEXT("Shoes"));
	const FName AccessorySlotId(TEXT("Accessory"));

	const FString TextureRoot(TEXT("/Game/GameXXK/UI/Inventory/Textures/"));
	const FString ApprovedTextureRoot(TEXT("/Game/GameXXK/UI/MasterV2/Approved/"));
	const FString SelectionInkTexturePath;
	const FString SquareSelectedTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_SquareSelected.T_MasterV2_SquareSelected"));
	const FString RectangularSelectionTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString WindowFrameTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_PanelLarge.T_MasterV2_PanelLarge"));
	const FString PanelFrameTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_PanelLarge.T_MasterV2_PanelLarge"));
	const FString ConfirmationDialogTexturePath(PanelFrameTexturePath);
	const FString CloseButtonTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CloseInk.T_MasterV2_CloseInk"));
	const FString BackpackScrollbarTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_BackpackScrollbarRight.T_MasterV2_BackpackScrollbarRight"));
	// Backpack slot paper for tooltips per user request.
	const FString TooltipPaperTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ItemSlot.T_MasterV2_ItemSlot"));
	const FString BackpackSlotTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ItemSlot.T_MasterV2_ItemSlot"));
	const FString EquipmentSlotTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_EquipmentSlot.T_MasterV2_EquipmentSlot"));
	const FString HeroFullBodyTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_HeroFullBody.T_MasterV2_HeroFullBody"));
	const FString ScrollbarThumbTexturePath(ApprovedTextureRoot + TEXT("inventory_scrollbar_Button.inventory_scrollbar_Button"));
	const FString ActionButtonTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackTabAllTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackTabEquipmentTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackTabPropTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackTabMaterialTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackTabTaskTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	const FString BackpackSortTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase"));
	// Master V1 page 03 approved decompose glyph (user-exported 01_DecomposeButton).
	const FString BackpackDisassembleTexturePath(ApprovedTextureRoot + TEXT("01_DecomposeButton.01_DecomposeButton"));
	const FString CharacterTabNormalTexturePath(ApprovedTextureRoot + TEXT("003_tab_1.003_tab_1"));
	const FString CharacterTabSelectedTexturePath(ApprovedTextureRoot + TEXT("004_tab_2.004_tab_2"));
	const FString HeroCardFrameTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CardFrame.T_MasterV2_CardFrame"));
	const FString HeroLockedCardIconTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CardLockedIcon.T_MasterV2_CardLockedIcon"));
	// Same portrait catalog used by the in-battle card face.
	const FString HeroCardPortraitTexturePath(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero"));

	FString ResolveDeckCardPortraitPath(const FGameXXKCardDefinition& Definition)
	{
		static const FString CardArtRoot(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/"));
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc && !Definition.NpcId.IsNone())
		{
			FString Token = Definition.NpcId.ToString().Replace(TEXT("."), TEXT("_"));
			return CardArtRoot
				+ FString::Printf(TEXT("T_CardPortrait_%s.T_CardPortrait_%s"), *Token, *Token);
		}
		if (Definition.Owner == EGameXXKCardOwner::Profession)
		{
			const TCHAR* RoleToken = nullptr;
			switch (Definition.Role)
			{
			case EGameXXKCharacterRole::Blade: RoleToken = TEXT("Blade"); break;
			case EGameXXKCharacterRole::Guard: RoleToken = TEXT("Guard"); break;
			case EGameXXKCharacterRole::Healer: RoleToken = TEXT("Healer"); break;
			case EGameXXKCharacterRole::Hunter: RoleToken = TEXT("Hunter"); break;
			case EGameXXKCharacterRole::Sorcerer: RoleToken = TEXT("Sorcerer"); break;
			case EGameXXKCharacterRole::FormationMaster: RoleToken = TEXT("FormationMaster"); break;
			default: break;
			}
			if (RoleToken)
			{
				return CardArtRoot
					+ FString::Printf(TEXT("T_CardPortrait_Role_%s.T_CardPortrait_Role_%s"), RoleToken, RoleToken);
			}
		}
		return HeroCardPortraitTexturePath;
	}
	const FString ApprovedPanelTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_PanelLarge.T_MasterV2_PanelLarge"));
	const FMargin SlotFrameMargin(5.0f / 61.0f, 5.0f / 56.0f, 5.0f / 61.0f, 5.0f / 56.0f);
	const FMargin ActionFrameMargin(0.05f, 0.11f, 0.05f, 0.11f);

	struct FBackpackRuntimeEntry
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

	EGameXXKEquipmentSlot EquipmentSlotFromId(const FName SlotId)
	{
		if (SlotId == WeaponSlotId)
		{
			return EGameXXKEquipmentSlot::Weapon;
		}
		if (SlotId == HeadSlotId)
		{
			return EGameXXKEquipmentSlot::Head;
		}
		if (SlotId == ArmorSlotId)
		{
			return EGameXXKEquipmentSlot::Armor;
		}
		if (SlotId == BeltSlotId)
		{
			return EGameXXKEquipmentSlot::Belt;
		}
		if (SlotId == ShoesSlotId)
		{
			return EGameXXKEquipmentSlot::Shoes;
		}
		if (SlotId == AccessorySlotId)
		{
			return EGameXXKEquipmentSlot::Accessory;
		}
		return EGameXXKEquipmentSlot::Invalid;
	}

	UTexture2D* LoadTexture(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *Path);
	}

	FSlateBrush MakeTextureBrush(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FSlateBrush MakeBoxTextureBrush(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize, Tint);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(0.065f);
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		const FSlateBrush NormalBrush = MakeTextureBrush(Path, ImageSize, Tint);
		FButtonStyle Style;
		Style.SetNormal(NormalBrush);
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.45f, 0.45f, 0.45f, 0.75f)));
		return Style;
	}

	FButtonStyle MakeBoxTextureButtonStyle(const FString& Path, const FVector2D& ImageSize, const FMargin& Margin, const FLinearColor& Tint = FLinearColor::White)
	{
		FButtonStyle Style = MakeTextureButtonStyle(Path, ImageSize, Tint);
		FSlateBrush NormalBrush = Style.Normal;
		NormalBrush.DrawAs = ESlateBrushDrawType::Box;
		NormalBrush.Margin = Margin;
		Style.SetNormal(NormalBrush);
		FSlateBrush HoveredBrush = Style.Hovered;
		HoveredBrush.DrawAs = ESlateBrushDrawType::Box;
		HoveredBrush.Margin = Margin;
		Style.SetHovered(HoveredBrush);
		FSlateBrush PressedBrush = Style.Pressed;
		PressedBrush.DrawAs = ESlateBrushDrawType::Box;
		PressedBrush.Margin = Margin;
		Style.SetPressed(PressedBrush);
		FSlateBrush DisabledBrush = Style.Disabled;
		DisabledBrush.DrawAs = ESlateBrushDrawType::Box;
		DisabledBrush.Margin = Margin;
		Style.SetDisabled(DisabledBrush);
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
		UWidgetTree* WidgetTree,
		const FText& Text,
		int32 FontSize = 16,
		const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		const FName Name = NAME_None)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		// Button labels (分解/页签等) must never wrap into vertical stacked glyphs.
		TextBlock->SetAutoWrapText(false);
		TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FontSize));
		return TextBlock;
	}

	UButton* MakeActionButton(UWidgetTree* WidgetTree, const FText& Text, UTextBlock*& OutText)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(MakeBoxTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize, ActionFrameMargin));
		Button->SetBackgroundColor(FLinearColor::White);
		OutText = MakeText(WidgetTree, Text, 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
		if (OutText)
		{
			OutText->SetJustification(ETextJustify::Center);
			Button->AddChild(OutText);
		}
		return Button;
	}

	void AddCanvasChild(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FAnchors& Anchors = FAnchors(0.0f, 0.0f), const FVector2D& Alignment = FVector2D::ZeroVector)
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

	FString ResolveItemIconTexturePath(FName ItemId)
	{
		const FString InspectableIcon =
			FGameXXKInventoryItemPresentation::ResolveIconPath(ItemId);
		if (!InspectableIcon.IsEmpty())
		{
			return InspectableIcon;
		}
		const FSoftObjectPath GemIconPath = FGameXXKGemRules::GetIconTexturePathForItemId(ItemId);
		if (GemIconPath.IsValid())
		{
			return GemIconPath.ToString();
		}
		if (ItemId == UGameXXKMVPRules::ItemHealingPowder())
		{
			return TextureRoot + TEXT("T_ItemHealingPowder.T_ItemHealingPowder");
		}
		if (ItemId == UGameXXKMVPRules::ItemEnhancementStone())
		{
			// UI V2 approved core-item icon.
			return TEXT("/Game/GameXXK/UI/Items/strengthening_stone.strengthening_stone");
		}
		if (ItemId == UGameXXKMVPRules::ItemRefinementSand())
		{
			// UI V2 approved core-item icon.
			return TEXT("/Game/GameXXK/UI/Items/refinement_sand.refinement_sand");
		}
		if (ItemId == UGameXXKMVPRules::ItemQingshanRouteSeal())
		{
			// UI V2 approved core-item icon.
			return TEXT("/Game/GameXXK/UI/Items/qingshan_suppression_token.qingshan_suppression_token");
		}
		if (ItemId == FName(TEXT("Item.LingzhiPowder")))
		{
			return TextureRoot + TEXT("T_ItemLingzhiPowder.T_ItemLingzhiPowder");
		}
		if (ItemId == FName(TEXT("Item.QingxinTea")))
		{
			return TextureRoot + TEXT("T_ItemQingxinTea.T_ItemQingxinTea");
		}
		if (ItemId == FName(TEXT("Item.CraneSachet")))
		{
			return TextureRoot + TEXT("T_ItemCraneSachet.T_ItemCraneSachet");
		}
		if (ItemId == UGameXXKMVPRules::ItemIronSword())
		{
			return TextureRoot + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword");
		}
		if (ItemId == UGameXXKMVPRules::ItemClothArmor())
		{
			return TextureRoot + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor");
		}
		if (ItemId == FName(TEXT("Item.CranePatternTalisman")))
		{
			return TextureRoot + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman");
		}
		if (ItemId == FName(TEXT("Item.InkstonePendant")))
		{
			return TextureRoot + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant");
		}
		if (ItemId == UGameXXKMVPRules::ItemWoodenSword())
		{
			return TextureRoot + TEXT("T_ItemWoodenSword.T_ItemWoodenSword");
		}
		if (ItemId == UGameXXKMVPRules::ItemStarterClothArmor())
		{
			return TextureRoot + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor");
		}
		if (ItemId == UGameXXKMVPRules::ItemClothTalisman())
		{
			return TextureRoot + TEXT("T_ItemClothTalisman.T_ItemClothTalisman");
		}
		return FString();
	}

	FString ResolveInventoryFilterTexturePath(EGameXXKInventoryFilter Filter)
	{
		switch (Filter)
		{
		case EGameXXKInventoryFilter::All:
			return BackpackTabAllTexturePath;
		case EGameXXKInventoryFilter::Equipment:
			return BackpackTabEquipmentTexturePath;
		case EGameXXKInventoryFilter::Props:
			return BackpackTabPropTexturePath;
		case EGameXXKInventoryFilter::Materials:
			return BackpackTabMaterialTexturePath;
		case EGameXXKInventoryFilter::Tasks:
			return BackpackTabTaskTexturePath;
		default:
			return FString();
		}
	}

	bool MatchesInventoryFilter(EGameXXKItemKind Kind, EGameXXKInventoryFilter Filter)
	{
		switch (Filter)
		{
		case EGameXXKInventoryFilter::All:
			return true;
		case EGameXXKInventoryFilter::Equipment:
			return Kind == EGameXXKItemKind::Weapon || Kind == EGameXXKItemKind::Armor || Kind == EGameXXKItemKind::Accessory;
		case EGameXXKInventoryFilter::Props:
			return Kind == EGameXXKItemKind::Consumable;
		case EGameXXKInventoryFilter::Materials:
			return Kind == EGameXXKItemKind::Material;
		case EGameXXKInventoryFilter::Tasks:
			return Kind == EGameXXKItemKind::Task;
		default:
			return false;
		}
	}

	int32 InventorySortRank(EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Weapon:
		case EGameXXKItemKind::Armor:
		case EGameXXKItemKind::Accessory:
			return 0;
		case EGameXXKItemKind::Consumable:
			return 1;
		case EGameXXKItemKind::Material:
			return 2;
		case EGameXXKItemKind::Task:
			return 3;
		default:
			return 4;
		}
	}

	FText ItemKindText(EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Consumable:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindConsumable", "消耗");
		case EGameXXKItemKind::Weapon:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindWeapon", "武器");
		case EGameXXKItemKind::Armor:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindArmor", "防具");
		case EGameXXKItemKind::Accessory:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindAccessory", "饰品");
		case EGameXXKItemKind::Material:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindMaterial", "材料");
		case EGameXXKItemKind::Task:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindTask", "任务");
		default:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindUnknown", "物品");
		}
	}

	FString ItemStatsText(const FGameXXKItemDef& Def, int32 EnhancementLevel)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("类型：%s"), *ItemKindText(Def.Kind).ToString()));
		if (Def.HealAmount > 0)
		{
			Lines.Add(FString::Printf(TEXT("恢复 HP +%d"), Def.HealAmount));
		}
		if (Def.MPHealAmount > 0)
		{
			Lines.Add(FString::Printf(TEXT("恢复 MP +%d"), Def.MPHealAmount));
		}
		if (Def.AttackBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("攻击 +%d"), Def.AttackBonus));
		}
		if (Def.DefenseBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("防御 +%d"), Def.DefenseBonus));
		}
		if (Def.MaxHPBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("生命上限 +%d"), Def.MaxHPBonus));
		}
		if (Def.MaxMPBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("真气上限 +%d"), Def.MaxMPBonus));
		}
		if (Def.Kind == EGameXXKItemKind::Weapon || Def.Kind == EGameXXKItemKind::Armor || Def.Kind == EGameXXKItemKind::Accessory)
		{
			Lines.Add(FString::Printf(TEXT("强化 +%d / +%d"), EnhancementLevel, UGameXXKMVPRules::GetMaxItemEnhancementLevel()));
		}
		if (Def.Kind != EGameXXKItemKind::Task)
		{
			Lines.Add(FString::Printf(TEXT("买入 %d金 / 卖出 %d金"), Def.BuyPrice, Def.SellPrice));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FText EquipmentQualityText(const EGameXXKEquipmentQuality Quality)
	{
		return FGameXXKEquipmentQualityRules::GetDisplayName(Quality);
	}

	FText EquipmentSlotText(const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotWeapon", "武器");
		case EGameXXKEquipmentSlot::Head: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotHead", "头部");
		case EGameXXKEquipmentSlot::Armor: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotArmor", "衣甲");
		case EGameXXKEquipmentSlot::Belt: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotBelt", "腰带");
		case EGameXXKEquipmentSlot::Shoes: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotShoes", "鞋");
		case EGameXXKEquipmentSlot::Accessory: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotAccessory", "饰品");
		default: return NSLOCTEXT("GameXXKInventoryWindow", "EquipmentSlotUnknown", "未知部位");
		}
	}

	FText BuildEquipmentInstanceDetail(
		const UGameXXKMVPSubsystem* Subsystem,
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Definition,
		const FName CompareCharacterId)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("部位：%s"), *EquipmentSlotText(Definition.Slot).ToString()));
		Lines.Add(FString::Printf(TEXT("装备等级 %d"), Instance.ItemLevel));
		Lines.Add(FString::Printf(TEXT("品质：%s"), *EquipmentQualityText(Instance.Quality).ToString()));
		Lines.Add(FString::Printf(TEXT("强化 +%d"), Instance.EnhancementLevel));
		for (const FGameXXKEquipmentAffixRoll& Roll : Instance.RolledAffixes)
		{
			const FGameXXKAffixDefinition* Affix = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
			if (Affix)
			{
				if (Roll.Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints)
				{
					// 万分比词缀显示为百分比（312 → +3.12%）
					Lines.Add(FString::Printf(TEXT("%s +%.2f%%"), *Affix->DisplayName.ToString(), Roll.Magnitude / 100.0));
				}
				else
				{
					Lines.Add(FString::Printf(TEXT("%s +%d"), *Affix->DisplayName.ToString(), Roll.Magnitude));
				}
			}
		}
		for (int32 SocketIndex = 0; SocketIndex < Instance.SocketedGems.Num(); ++SocketIndex)
		{
			const FGameXXKSocketedGem& Gem = Instance.SocketedGems[SocketIndex];
			if (Gem.IsEmpty())
			{
				Lines.Add(FString::Printf(TEXT("孔位 %d：空"), SocketIndex + 1));
				continue;
			}
			Lines.Add(FString::Printf(
				TEXT("孔位 %d：%s +%d"),
				SocketIndex + 1,
				*FGameXXKGemRules::GetDisplayName(Gem.Type, Gem.Quality).ToString(),
				FGameXXKGemRules::GetStatBonus(Gem.Type, Gem.Quality)));
		}

		FGameXXKEquipmentTooltipSnapshot Snapshot;
		const bool bHasSnapshot = Subsystem
			&& Subsystem->GetEquipmentTooltipSnapshot(
				Instance.InstanceId,
				CompareCharacterId,
				Snapshot);

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

	FName SlotForItemKind(EGameXXKItemKind Kind)
	{
		if (Kind == EGameXXKItemKind::Weapon)
		{
			return WeaponSlotId;
		}
		if (Kind == EGameXXKItemKind::Armor)
		{
			return ArmorSlotId;
		}
		if (Kind == EGameXXKItemKind::Accessory)
		{
			return AccessorySlotId;
		}
		return NAME_None;
	}
}

void UGameXXKInventorySlotButton::Configure(UGameXXKInventoryWindowWidget* InOwner, EGameXXKInventorySlotSource InSource, int32 InSlotIndex, FName InEquipmentSlotId)
{
	Owner = InOwner;
	Source = InSource;
	SlotIndex = InSlotIndex;
	EquipmentSlotId = InEquipmentSlotId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKInventorySlotButton::HandleClicked);
}

void UGameXXKInventorySlotButton::HandleClicked()
{
	if (Owner)
	{
		const bool bLockableCell = Source == EGameXXKInventorySlotSource::PlayerBackpack
			|| Source == EGameXXKInventorySlotSource::Equipment;
		if (bLockableCell
			&& FSlateApplication::IsInitialized()
			&& FSlateApplication::Get().GetModifierKeys().IsAltDown())
		{
			Owner->HandleConfiguredSlotAltClicked(Source, SlotIndex, EquipmentSlotId);
			return;
		}
		Owner->HandleConfiguredSlotClicked(Source, SlotIndex, EquipmentSlotId);
	}
}

bool UGameXXKInventorySlotButton::HandleRightMouseButtonDown()
{
	return Owner && Owner->HandleConfiguredSlotRightClicked(Source, SlotIndex, EquipmentSlotId);
}

TSharedRef<SWidget> UGameXXKInventorySlotButton::RebuildWidget()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyButton = SNew(SGameXXKInventorySlotButton, this)
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

void UGameXXKInventoryFilterButton::Configure(UGameXXKInventoryWindowWidget* InOwner, EGameXXKInventoryFilter InFilter)
{
	Owner = InOwner;
	Filter = InFilter;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKInventoryFilterButton::HandleClicked);
}

void UGameXXKInventoryFilterButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleInventoryFilterClicked(Filter);
	}
}

void UGameXXKCharacterBackpackTabButton::Configure(
	UGameXXKInventoryWindowWidget* InOwner,
	const EGameXXKCharacterBackpackTab InTab)
{
	Owner = InOwner;
	Tab = InTab;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKCharacterBackpackTabButton::HandleClicked);
}

void UGameXXKCharacterBackpackTabButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleCharacterBackpackTabClicked(Tab);
	}
}

void UGameXXKHeroDeckCardButton::Configure(UGameXXKInventoryWindowWidget* InOwner, const FName InCardId)
{
	Owner = InOwner;
	CardId = InCardId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKHeroDeckCardButton::HandleClicked);
}

void UGameXXKHeroDeckCardButton::HandleClicked()
{
	if (Owner && !CardId.IsNone())
	{
		Owner->HandleHeroDeckCardClicked(CardId);
	}
}

void UGameXXKInventoryWindowWidget::ConfigureDesktopTrainingEmbeddedMode(const bool bEnabled)
{
	bDesktopTrainingEmbeddedMode = bEnabled;
	if (RootCanvas)
	{
		RefreshProgrammaticLayout();
	}
}

void UGameXXKInventoryWindowWidget::ConfigureDesktopTrainingCharacter(const FName CharacterId)
{
	if (ConfiguredDesktopTrainingCharacterId != CharacterId)
	{
		PendingHeroDeckIds.Reset();
	}
	ConfiguredDesktopTrainingCharacterId = CharacterId;
	if (RootCanvas)
	{
		RefreshProgrammaticLayout();
	}
}

FName UGameXXKInventoryWindowWidget::GetConfiguredCharacterIdForTest() const
{
	return ResolveInventoryCharacterId();
}

FName UGameXXKInventoryWindowWidget::ResolveInventoryCharacterId() const
{
	return bDesktopTrainingEmbeddedMode && !ConfiguredDesktopTrainingCharacterId.IsNone()
		? ConfiguredDesktopTrainingCharacterId
		: FGameXXKEquipmentRules::HeroCharacterId();
}

FGameXXKBattleAnimationClipDescriptor
UGameXXKInventoryWindowWidget::ResolveCentralCharacterIdleClip() const
{
	const FName CharacterId = ResolveInventoryCharacterId();
	if (CharacterId.IsNone()
		|| CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
	{
		return {};
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	if (!Subsystem
		|| !Subsystem->GetEquipmentLoadoutSnapshot(CharacterId, Snapshot)
		|| Snapshot.CharacterId != CharacterId)
	{
		return {};
	}

	return FGameXXKBattleAnimationPresentation::ResolveClip(
		CharacterId,
		false,
		EGameXXKBattleAnimationAction::Idle);
}

void UGameXXKInventoryWindowWidget::RefreshCentralCharacterPresentation()
{
	if (!CentralHeroIdleImage)
	{
		return;
	}

	CentralHeroIdleImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
	const FName CharacterId = ResolveInventoryCharacterId();
	if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
	{
		UTexture2D* HeroTexture = LoadTexture(HeroFullBodyTexturePath);
		if (!HeroTexture)
		{
			ClearCentralCharacterPresentation();
			return;
		}

		FSlateBrush HeroBrush;
		HeroBrush.DrawAs = ESlateBrushDrawType::Image;
		HeroBrush.ImageSize = FVector2D(518.0f, 518.0f);
		HeroBrush.SetResourceObject(HeroTexture);
		CentralHeroIdleImage->SetBrush(HeroBrush);
		CentralHeroIdleImage->SetColorAndOpacity(FLinearColor::White);
		CentralHeroIdleImage->SetRenderOpacity(1.0f);
		return;
	}

	ApplyCentralCharacterIdleClip(ResolveCentralCharacterIdleClip());
}

void UGameXXKInventoryWindowWidget::ClearCentralCharacterPresentation()
{
	if (!CentralHeroIdleImage)
	{
		return;
	}
	FSlateBrush EmptyBrush;
	EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	EmptyBrush.ImageSize = FVector2D(518.0f, 518.0f);
	CentralHeroIdleImage->SetBrush(EmptyBrush);
	CentralHeroIdleImage->SetRenderOpacity(0.0f);
}

void UGameXXKInventoryWindowWidget::ApplyCentralCharacterIdleClip(
	const FGameXXKBattleAnimationClipDescriptor& Clip)
{
	if (!CentralHeroIdleImage)
	{
		return;
	}
	CentralHeroIdleImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
	UTexture2D* AtlasTexture = Clip.IsValid()
		? LoadTexture(Clip.TexturePath.ToString())
		: nullptr;
	if (!AtlasTexture)
	{
		ClearCentralCharacterPresentation();
		return;
	}

	FSlateBrush AtlasBrush;
	AtlasBrush.DrawAs = ESlateBrushDrawType::Image;
	AtlasBrush.ImageSize = FVector2D(518.0f, 518.0f);
	AtlasBrush.SetResourceObject(AtlasTexture);
	AtlasBrush.SetUVRegion(
		FGameXXKBattleAnimationPresentation::CalculateUvRegion(Clip, 0));
	CentralHeroIdleImage->SetBrush(AtlasBrush);
	CentralHeroIdleImage->SetColorAndOpacity(FLinearColor::White);
	CentralHeroIdleImage->SetRenderOpacity(1.0f);
}

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKInventoryWindowWidget::RefreshCentralCharacterPresentationFromClipForTest(
	const FGameXXKBattleAnimationClipDescriptor& Clip)
{
	ApplyCentralCharacterIdleClip(Clip);
}
#endif

int32 UGameXXKInventoryWindowWidget::GetConfiguredDeckRequiredCount() const
{
	const FName CharacterId = ResolveInventoryCharacterId();
	if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
	{
		return 8;
	}
	return FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId) ? 3 : 5;
}

void UGameXXKInventoryWindowWidget::ConfigureDesktopTrainingHost(UGameXXKDesktopTrainingWorkbenchWidget* InHost)
{
	DesktopTrainingHost = InHost;
}

void UGameXXKInventoryWindowWidget::RefreshTalentCapacityPresentation()
{
	RefreshBackpackSlots();
}

bool UGameXXKInventoryWindowWidget::IsDesktopTrainingEmbeddedModeForTest() const
{
	return bDesktopTrainingEmbeddedMode;
}

FGameXXKEmbeddedInventorySessionState UGameXXKInventoryWindowWidget::CaptureEmbeddedSessionState() const
{
	FGameXXKEmbeddedInventorySessionState State;
	State.CharacterId = ResolveInventoryCharacterId();
	State.ActiveInventoryFilter = ActiveInventoryFilter;
	State.ActiveCharacterTab = ActiveCharacterTab;
	State.bBackpackSorted = bBackpackSorted;
	State.BackpackScrollOffset = DeferredBackpackScrollOffset;
	State.PendingDeckIds = PendingHeroDeckIds;
	return State;
}

void UGameXXKInventoryWindowWidget::RestoreEmbeddedSessionState(const FGameXXKEmbeddedInventorySessionState& State)
{
	ConfiguredDesktopTrainingCharacterId = State.CharacterId;
	ActiveInventoryFilter = State.ActiveInventoryFilter;
	ActiveCharacterTab = State.ActiveCharacterTab;
	bBackpackSorted = State.bBackpackSorted;
	DeferredBackpackScrollOffset = FMath::Max(0.0f, State.BackpackScrollOffset);
	PendingHeroDeckIds = State.PendingDeckIds;
	RefreshProgrammaticLayout();
	if (BackpackScrollBox)
	{
		BackpackScrollBox->SetScrollOffset(DeferredBackpackScrollOffset);
	}
}

bool UGameXXKInventoryWindowWidget::IsBackpackSortedForTest() const
{
	return bBackpackSorted;
}

void UGameXXKInventoryWindowWidget::SetBackpackScrollOffsetForTest(const float ScrollOffset)
{
	DeferredBackpackScrollOffset = FMath::Max(0.0f, ScrollOffset);
	if (BackpackScrollBox)
	{
		BackpackScrollBox->SetScrollOffset(DeferredBackpackScrollOffset);
	}
}

float UGameXXKInventoryWindowWidget::GetBackpackScrollOffsetForTest() const
{
	return DeferredBackpackScrollOffset;
}

FName UGameXXKInventoryWindowWidget::GetBackpackItemIdAtSlotForDesktopTraining(const int32 SlotIndex) const
{
	return CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex)
		? CurrentBackpackSlotItemIds[SlotIndex]
		: NAME_None;
}

FName UGameXXKInventoryWindowWidget::GetBackpackEquipmentInstanceIdAtSlotForDesktopTraining(const int32 SlotIndex) const
{
	return CurrentBackpackSlotEquipmentInstanceIds.IsValidIndex(SlotIndex)
		? CurrentBackpackSlotEquipmentInstanceIds[SlotIndex]
		: NAME_None;
}

int32 UGameXXKInventoryWindowWidget::GetBackpackQuantityAtSlotForDesktopTraining(const int32 SlotIndex) const
{
	return CurrentBackpackSlotQuantities.IsValidIndex(SlotIndex)
		? CurrentBackpackSlotQuantities[SlotIndex]
		: 0;
}

FString UGameXXKInventoryWindowWidget::GetBackpackIconPathAtSlotForDesktopTraining(const int32 SlotIndex) const
{
	return CurrentBackpackSlotIconPaths.IsValidIndex(SlotIndex)
		? CurrentBackpackSlotIconPaths[SlotIndex]
		: FString();
}

bool UGameXXKInventoryWindowWidget::OpenFreeInventory()
{
	return OpenInventoryWindow(EGameXXKInventoryWindowMode::FreeInventory);
}

bool UGameXXKInventoryWindowWidget::OpenMerchantTrade()
{
	return OpenInventoryWindow(EGameXXKInventoryWindowMode::MerchantTrade);
}

bool UGameXXKInventoryWindowWidget::CloseInventoryWindow()
{
	CancelDialog();
	WindowMode = EGameXXKInventoryWindowMode::None;
	SelectedSlotSource = EGameXXKInventorySlotSource::None;
	SelectedItemId = NAME_None;
	SelectedEquipmentInstanceId = NAME_None;
	SelectedSlotIndex = INDEX_NONE;
	SelectedEquipmentSlotId = NAME_None;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::OpenFreeInventoryForTest()
{
	return OpenFreeInventory();
}

bool UGameXXKInventoryWindowWidget::OpenMerchantTradeForTest()
{
	return OpenMerchantTrade();
}

EGameXXKInventoryWindowMode UGameXXKInventoryWindowWidget::GetWindowModeForTest() const
{
	return WindowMode;
}

bool UGameXXKInventoryWindowWidget::HasWindowFrameForTest() const
{
	return WindowFrame != nullptr;
}

bool UGameXXKInventoryWindowWidget::HasCloseButtonForTest() const
{
	return CloseButton != nullptr;
}

bool UGameXXKInventoryWindowWidget::IsWindowVisibleForTest() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UGameXXKInventoryWindowWidget::IsModalInputLockActiveForTest() const
{
	return WindowMode == EGameXXKInventoryWindowMode::MerchantTrade && IsWindowVisibleForTest();
}

FString UGameXXKInventoryWindowWidget::GetWindowFrameResourcePathForTest() const
{
	return WindowFrameTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetCloseButtonResourcePathForTest() const
{
	return CloseButtonTexturePath;
}

int32 UGameXXKInventoryWindowWidget::GetBackpackSlotCountForTest() const
{
	return BackpackViewportSlotCount;
}

FString UGameXXKInventoryWindowWidget::GetBackpackSlotResourcePathForTest() const
{
	return BackpackSlotTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetBackpackSlotIconResourcePathForTest(int32 SlotIndex) const
{
	return CurrentBackpackSlotIconPaths.IsValidIndex(SlotIndex) ? CurrentBackpackSlotIconPaths[SlotIndex] : FString();
}

int32 UGameXXKInventoryWindowWidget::GetEquipmentSlotCountForTest() const
{
	return EquipmentSlotButtons.Num();
}

int32 UGameXXKInventoryWindowWidget::GetBackpackColumnCountForTest() const
{
	return BackpackColumns;
}

int32 UGameXXKInventoryWindowWidget::GetBackpackStorageCapacityForTest() const
{
	return BackpackStorageCapacity;
}

bool UGameXXKInventoryWindowWidget::HasBackpackScrollBoxForTest() const
{
	return BackpackScrollBox != nullptr;
}

FString UGameXXKInventoryWindowWidget::GetScrollbarResourcePathForTest() const
{
	return BackpackScrollbarTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetSelectionInkResourcePathForTest() const
{
	return SelectionInkTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetTooltipResourcePathForTest() const
{
	return TooltipPaperTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetEquipmentSlotResourcePathForTest() const
{
	return EquipmentSlotTexturePath;
}

int32 UGameXXKInventoryWindowWidget::GetMerchantStockSlotCountForTest() const
{
	return MerchantStockSlotButtons.Num();
}

FString UGameXXKInventoryWindowWidget::GetMerchantStockSlotResourcePathForTest() const
{
	return BackpackSlotTexturePath;
}

FText UGameXXKInventoryWindowWidget::GetSelectedPrimaryActionTextForTest() const
{
	return CurrentPrimaryActionText;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackItemForTest(FName ItemId)
{
	return SelectPlayerBackpackItem(ItemId);
}

bool UGameXXKInventoryWindowWidget::ExecuteSelectedPrimaryActionForTest()
{
	if (!SelectedEquipmentInstanceId.IsNone())
	{
		if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
		{
			return QuickEquipBackpackInstanceForTest(SelectedEquipmentInstanceId);
		}
		if (SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
		{
			const EGameXXKEquipmentSlot EquipmentSlot = EquipmentSlotFromId(SelectedEquipmentSlotId);
			return EquipmentSlot != EGameXXKEquipmentSlot::Invalid && QuickUnequipSlotForTest(EquipmentSlot);
		}
		return false;
	}
	if (SelectedItemId.IsNone())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	bool bExecuted = false;
	if (SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
	{
		bExecuted = Subsystem->UnequipItem(SelectedItemId);
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
		if (!bFound)
		{
			return false;
		}
		if (Def.Kind == EGameXXKItemKind::Consumable)
		{
			bExecuted = Subsystem->UseItem(SelectedItemId);
		}
		else
		{
			bExecuted = Subsystem->EquipItem(SelectedItemId);
		}
	}

	if (bExecuted)
	{
		RefreshProgrammaticLayout();
	}
	return bExecuted;
}

FName UGameXXKInventoryWindowWidget::GetEquippedItemForSlotForTest(FName SlotId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return NAME_None;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (SlotId == WeaponSlotId)
	{
		return State.EquippedWeapon;
	}
	if (SlotId == ArmorSlotId)
	{
		return State.EquippedArmor;
	}
	if (SlotId == AccessorySlotId)
	{
		return State.EquippedAccessory;
	}
	return NAME_None;
}

bool UGameXXKInventoryWindowWidget::QuickEquipBackpackInstanceForTest(const FName InstanceId)
{
	CharacterBackpackModel.Bind(ResolveMVPSubsystem(), ResolveInventoryCharacterId());
	FGameXXKEquipmentTransactionResult Result;
	const bool bSucceeded = CharacterBackpackModel.QuickEquip(InstanceId, Result);
	if (bSucceeded)
	{
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
		{
			Subsystem->NormalizeDesktopInventoryState();
		}
		RefreshProgrammaticLayout();
	}
	else if (SelectedDetailTextBlock && !Result.Message.IsEmpty())
	{
		SelectedDetailTextBlock->SetText(Result.Message);
	}
	return bSucceeded;
}

bool UGameXXKInventoryWindowWidget::QuickUnequipSlotForTest(const EGameXXKEquipmentSlot EquipmentSlot)
{
	CharacterBackpackModel.Bind(ResolveMVPSubsystem(), ResolveInventoryCharacterId());
	FGameXXKEquipmentTransactionResult Result;
	const bool bSucceeded = CharacterBackpackModel.QuickUnequip(EquipmentSlot, Result);
	if (bSucceeded)
	{
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
		{
			Subsystem->NormalizeDesktopInventoryState();
		}
		RefreshProgrammaticLayout();
	}
	else if (SelectedDetailTextBlock && !Result.Message.IsEmpty())
	{
		SelectedDetailTextBlock->SetText(Result.Message);
	}
	return bSucceeded;
}

FName UGameXXKInventoryWindowWidget::GetEquippedInstanceForSlotForTest(const EGameXXKEquipmentSlot EquipmentSlot) const
{
	FGameXXKCharacterBackpackModel Model;
	Model.Bind(const_cast<UGameXXKMVPSubsystem*>(ResolveMVPSubsystem()), ResolveInventoryCharacterId());
	const TArray<FGameXXKCharacterBackpackSlotView> Slots = Model.GetSixSlotSnapshot();
	const FGameXXKCharacterBackpackSlotView* View = Slots.FindByPredicate([EquipmentSlot](const FGameXXKCharacterBackpackSlotView& Candidate)
	{
		return Candidate.Slot == EquipmentSlot;
	});
	return View ? View->EquippedInstanceId : NAME_None;
}

bool UGameXXKInventoryWindowWidget::SelectMerchantStockSlotForTest(int32 SlotIndex)
{
	return SelectMerchantStockSlot(SlotIndex);
}

bool UGameXXKInventoryWindowWidget::RequestSelectedBuyForTest()
{
	return RequestBuyForSelectedItem();
}

bool UGameXXKInventoryWindowWidget::ConfirmDialogForTest()
{
	return ConfirmDialog();
}

bool UGameXXKInventoryWindowWidget::CancelDialogForTest()
{
	return CancelDialog();
}

bool UGameXXKInventoryWindowWidget::IsConfirmationDialogVisibleForTest() const
{
	return ConfirmationDialogFrame && ConfirmationDialogFrame->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKInventoryWindowWidget::HasConfirmationConfirmButtonForTest() const
{
	return ConfirmationConfirmButton != nullptr;
}

bool UGameXXKInventoryWindowWidget::HasConfirmationCancelButtonForTest() const
{
	return ConfirmationCancelButton != nullptr;
}

EGameXXKInventoryFilter UGameXXKInventoryWindowWidget::GetActiveInventoryFilterForTest() const
{
	return ActiveInventoryFilter;
}

bool UGameXXKInventoryWindowWidget::SelectInventoryFilterForTest(EGameXXKInventoryFilter Filter)
{
	return SelectInventoryFilter(Filter);
}

TArray<FName> UGameXXKInventoryWindowWidget::GetVisibleBackpackItemIdsForTest() const
{
	TArray<FName> VisibleItemIds;
	for (const FName ItemId : CurrentBackpackSlotItemIds)
	{
		if (!ItemId.IsNone())
		{
			VisibleItemIds.Add(ItemId);
		}
	}
	return VisibleItemIds;
}

TArray<FName> UGameXXKInventoryWindowWidget::GetVisibleBackpackEquipmentInstanceIdsForTest() const
{
	TArray<FName> VisibleInstanceIds;
	for (const FName InstanceId : CurrentBackpackSlotEquipmentInstanceIds)
	{
		if (!InstanceId.IsNone())
		{
			VisibleInstanceIds.Add(InstanceId);
		}
	}
	return VisibleInstanceIds;
}

int32 UGameXXKInventoryWindowWidget::FindBackpackEquipmentInstanceSlotForTest(const FName InstanceId) const
{
	return CurrentBackpackSlotEquipmentInstanceIds.IndexOfByKey(InstanceId);
}

int32 UGameXXKInventoryWindowWidget::FindBackpackItemSlotForTest(const FName ItemId) const
{
	return CurrentBackpackSlotItemIds.IndexOfByKey(ItemId);
}

int32 UGameXXKInventoryWindowWidget::GetSelectedBackpackSlotIndexForTest() const
{
	if (SelectedSlotSource != EGameXXKInventorySlotSource::PlayerBackpack)
	{
		return INDEX_NONE;
	}
	if (!SelectedEquipmentInstanceId.IsNone())
	{
		return CurrentBackpackSlotEquipmentInstanceIds.IndexOfByKey(SelectedEquipmentInstanceId);
	}
	return GetVisibleBackpackItemIdsForTest().IndexOfByKey(SelectedItemId);
}

bool UGameXXKInventoryWindowWidget::SortInventoryForTest()
{
	return SortInventory();
}

bool UGameXXKInventoryWindowWidget::RequestSelectedDecomposeForTest()
{
	return RequestDecomposeForSelectedItem();
}

bool UGameXXKInventoryWindowWidget::RequestSelectedEnhanceForTest()
{
	return RequestEnhanceForSelectedItem();
}

bool UGameXXKInventoryWindowWidget::EnhanceSelectedEquipmentInstanceForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || SelectedEquipmentInstanceId.IsNone())
	{
		return false;
	}
	const FGameXXKEquipmentInstance* Before = FGameXXKEquipmentRules::FindInstance(
		Subsystem->GetRuntimeState().EquipmentCollection,
		SelectedEquipmentInstanceId);
	if (!Before)
	{
		return false;
	}
	const int32 EnhancementLevelBefore = Before->EnhancementLevel;
	HandleEnhanceMainClicked();
	const FGameXXKEquipmentInstance* After = FGameXXKEquipmentRules::FindInstance(
		Subsystem->GetRuntimeState().EquipmentCollection,
		SelectedEquipmentInstanceId);
	return After && After->EnhancementLevel == EnhancementLevelBefore + 1;
}

FText UGameXXKInventoryWindowWidget::GetSelectedDetailTextForTest() const
{
	return SelectedDetailTextBlock ? SelectedDetailTextBlock->GetText() : FText::GetEmpty();
}

FString UGameXXKInventoryWindowWidget::GetInventoryFilterTexturePathForTest(EGameXXKInventoryFilter Filter) const
{
	return ResolveInventoryFilterTexturePath(Filter);
}

int32 UGameXXKInventoryWindowWidget::GetCharacterTabButtonCountForTest() const
{
	return CharacterTabButtons.Num();
}

EGameXXKCharacterBackpackTab UGameXXKInventoryWindowWidget::GetActiveCharacterBackpackTabForTest() const
{
	return ActiveCharacterTab;
}

bool UGameXXKInventoryWindowWidget::OpenCharacterBackpackTabForTest(const EGameXXKCharacterBackpackTab Tab)
{
	if (WindowMode != EGameXXKInventoryWindowMode::FreeInventory)
	{
		return false;
	}
	const EGameXXKCharacterBackpackTab PreviousTab = ActiveCharacterTab;
	ActiveCharacterTab = Tab;
	if (Tab == EGameXXKCharacterBackpackTab::Deck)
	{
		if (PreviousTab != EGameXXKCharacterBackpackTab::Deck)
		{
			PendingHeroDeckIds.Reset();
		}
		RefreshHeroDeckCards();
	}
	RefreshCharacterTabs();
	// Full refresh so backpack-only controls (filters, decompose/enhance/reforge)
	// hide on non-equipment tabs.
	RefreshProgrammaticLayout();
	return true;
}

FText UGameXXKInventoryWindowWidget::GetCharacterTabBodyTextForTest() const
{
	return CharacterTabBodyText ? CharacterTabBodyText->GetText() : FText::GetEmpty();
}

TArray<FName> UGameXXKInventoryWindowWidget::GetHeroCardBackpackIdsForTest() const
{
	return HeroCardBackpackIds;
}

TArray<FName> UGameXXKInventoryWindowWidget::GetPendingHeroDeckIdsForTest() const
{
	return PendingHeroDeckIds;
}

bool UGameXXKInventoryWindowWidget::ToggleHeroDeckCardForTest(const FName CardId)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 RequiredCount = GetConfiguredDeckRequiredCount();
	if (!Subsystem
		|| Subsystem->IsCompanionLoadoutMutationLocked()
		|| CardId.IsNone()
		|| !UnlockedHeroCardIds.Contains(CardId))
	{
		return false;
	}
	if (PendingHeroDeckIds.Contains(CardId))
	{
		PendingHeroDeckIds.RemoveSingle(CardId);
	}
	else
	{
		if (PendingHeroDeckIds.Num() >= RequiredCount)
		{
			return false;
		}
		PendingHeroDeckIds.Add(CardId);
	}
	RefreshHeroDeckCards();
	return true;
}

bool UGameXXKInventoryWindowWidget::ApplyHeroDeckForTest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FName CharacterId = ResolveInventoryCharacterId();
	const int32 RequiredCount = GetConfiguredDeckRequiredCount();
	if (!Subsystem || PendingHeroDeckIds.Num() != RequiredCount)
	{
		return false;
	}
	bool bApplied = false;
	if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
	{
		bApplied = Subsystem->SetHeroCardLoadout(PendingHeroDeckIds);
	}
	else if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId))
	{
		bApplied = Subsystem->SetTemporaryQuestNpcCardLoadout(CharacterId, PendingHeroDeckIds);
	}
	else
	{
		bApplied = Subsystem->SetPermanentCompanionCardLoadout(CharacterId, PendingHeroDeckIds);
	}
	if (!bApplied)
	{
		return false;
	}
	RefreshHeroDeckCards();
	return true;
}

FString UGameXXKInventoryWindowWidget::GetHeroCardFrameResourcePathForTest() const
{
	return HeroCardFrameTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetHeroLockedCardIconResourcePathForTest() const
{
	return HeroLockedCardIconTexturePath;
}

void UGameXXKInventoryWindowWidget::HandleCharacterBackpackTabClicked(const EGameXXKCharacterBackpackTab Tab)
{
	const bool bOpened = OpenCharacterBackpackTabForTest(Tab);
	if (bOpened && bDesktopTrainingEmbeddedMode && DesktopTrainingHost)
	{
		DesktopTrainingHost->HandleDesktopCharacterSubpageClicked(Tab);
	}
}

void UGameXXKInventoryWindowWidget::HandleHeroDeckCardClicked(const FName CardId)
{
	ToggleHeroDeckCardForTest(CardId);
}

void UGameXXKInventoryWindowWidget::HandleConfiguredSlotClicked(EGameXXKInventorySlotSource Source, int32 SlotIndex, FName EquipmentSlotId)
{
	if (PendingConfirmationAction != EConfirmationAction::None)
	{
		return;
	}
	if (bDesktopTrainingEmbeddedMode
		&& Source == EGameXXKInventorySlotSource::PlayerBackpack
		&& DesktopTrainingHost)
	{
		DesktopTrainingHost->HandleDesktopBackpackSlotLeftClicked(SlotIndex);
		return;
	}
	if (bDesktopTrainingEmbeddedMode
		&& Source == EGameXXKInventorySlotSource::Equipment
		&& DesktopTrainingHost
		&& DesktopTrainingHost->HasDesktopCarriedEntry())
	{
		DesktopTrainingHost->HandleDesktopEquipmentSlotLeftClicked(
			EquipmentSlotFromId(EquipmentSlotId));
		return;
	}

	bool bSelected = false;
	switch (Source)
	{
	case EGameXXKInventorySlotSource::PlayerBackpack:
		bSelected = SelectPlayerBackpackSlot(SlotIndex);
		break;
	case EGameXXKInventorySlotSource::MerchantStock:
		bSelected = SelectMerchantStockSlot(SlotIndex);
		break;
	case EGameXXKInventorySlotSource::Equipment:
		bSelected = SelectEquipmentSlot(EquipmentSlotId);
		break;
	default:
		break;
	}

	if (bSelected)
	{
		RefreshProgrammaticLayout();
	}
}

bool UGameXXKInventoryWindowWidget::HandleConfiguredSlotAltClicked(
	const EGameXXKInventorySlotSource Source,
	const int32 SlotIndex,
	const FName EquipmentSlotId)
{
	if (PendingConfirmationAction != EConfirmationAction::None)
	{
		return false;
	}
	if (bDesktopTrainingEmbeddedMode && DesktopTrainingHost)
	{
		if (Source == EGameXXKInventorySlotSource::PlayerBackpack)
		{
			return DesktopTrainingHost->HandleDesktopSlotAltClicked(
				EGameXXKDesktopItemContainer::Backpack,
				SlotIndex);
		}
		if (Source == EGameXXKInventorySlotSource::Equipment)
		{
			return DesktopTrainingHost->HandleDesktopEquipmentSlotAltClicked(
				EquipmentSlotFromId(EquipmentSlotId));
		}
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	FGameXXKDesktopInventoryEntryKey Entry;
	if (Source == EGameXXKInventorySlotSource::PlayerBackpack
		&& CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex)
		&& CurrentBackpackSlotEquipmentInstanceIds.IsValidIndex(SlotIndex))
	{
		const FName InstanceId = CurrentBackpackSlotEquipmentInstanceIds[SlotIndex];
		Entry = !InstanceId.IsNone()
			? FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId)
			: FGameXXKDesktopInventoryRules::MakeItemEntry(
				CurrentBackpackSlotItemIds[SlotIndex]);
	}
	else if (Source == EGameXXKInventorySlotSource::Equipment)
	{
		const EGameXXKEquipmentSlot ResolvedEquipmentSlot = EquipmentSlotFromId(EquipmentSlotId);
		Entry = FGameXXKDesktopInventoryRules::MakeEquipmentEntry(
			ResolvedEquipmentSlot == EGameXXKEquipmentSlot::Invalid
				? NAME_None
				: GetEquippedInstanceForSlotForTest(ResolvedEquipmentSlot));
	}
	if (!Entry.IsValid())
	{
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
		return false;
	}
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::HandleConfiguredSlotRightClicked(
	const EGameXXKInventorySlotSource Source,
	const int32 SlotIndex,
	const FName EquipmentSlotId)
{
	if (PendingConfirmationAction != EConfirmationAction::None)
	{
		return false;
	}
	if (bDesktopTrainingEmbeddedMode
		&& DesktopTrainingHost
		&& DesktopTrainingHost->HasDesktopCarriedEntry())
	{
		return DesktopTrainingHost->HandleDesktopCarryRightClicked();
	}
	if (bDesktopTrainingEmbeddedMode
		&& Source == EGameXXKInventorySlotSource::PlayerBackpack
		&& DesktopTrainingHost)
	{
		return DesktopTrainingHost->HandleDesktopBackpackSlotRightClicked(SlotIndex);
	}
	if (Source == EGameXXKInventorySlotSource::PlayerBackpack
		&& CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex)
		&& FGameXXKInventoryItemPresentation::IsInspectable(
			CurrentBackpackSlotItemIds[SlotIndex]))
	{
		AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController();
		return PlayerController && PlayerController->OpenTutorialMapInspection();
	}

	if (Source == EGameXXKInventorySlotSource::Equipment)
	{
		const EGameXXKEquipmentSlot ResolvedEquipmentSlot = EquipmentSlotFromId(EquipmentSlotId);
		if (ResolvedEquipmentSlot != EGameXXKEquipmentSlot::Invalid && !GetEquippedInstanceForSlotForTest(ResolvedEquipmentSlot).IsNone())
		{
			return QuickUnequipSlotForTest(ResolvedEquipmentSlot);
		}

		// A pre-migration save may still be represented by the legacy item mirror.
		return SelectEquipmentSlot(EquipmentSlotId) && ExecuteSelectedPrimaryActionForTest();
	}

	if (Source == EGameXXKInventorySlotSource::PlayerBackpack
		&& CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex)
		&& CurrentBackpackSlotEquipmentInstanceIds.IsValidIndex(SlotIndex))
	{
		const FName EquipmentInstanceId = CurrentBackpackSlotEquipmentInstanceIds[SlotIndex];
		if (!EquipmentInstanceId.IsNone())
		{
			return QuickEquipBackpackInstanceForTest(EquipmentInstanceId);
		}
		const FName ItemId = CurrentBackpackSlotItemIds[SlotIndex];
		bool bFound = false;
		const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		const bool bLegacyEquipment = bFound
			&& (Definition.Kind == EGameXXKItemKind::Weapon
				|| Definition.Kind == EGameXXKItemKind::Armor
				|| Definition.Kind == EGameXXKItemKind::Accessory);
		return bLegacyEquipment && SelectPlayerBackpackSlot(SlotIndex) && ExecuteSelectedPrimaryActionForTest();
	}
	return false;
}

void UGameXXKInventoryWindowWidget::HandleInventoryFilterClicked(EGameXXKInventoryFilter Filter)
{
	if (PendingConfirmationAction == EConfirmationAction::None)
	{
		SelectInventoryFilter(Filter);
	}
}

void UGameXXKInventoryWindowWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("InventoryWindowWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryWindowRoot"));
	WidgetTree->RootWidget = RootCanvas;

	ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryModalBackdrop"));
	ModalBackdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.30f));
	AddCanvasChild(RootCanvas, ModalBackdrop, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryWindowFrame"));
	WindowFrame->SetBrush(MakeBoxTextureBrush(WindowFrameTexturePath, InventoryPaperSize));
	WindowFrame->SetBrushColor(FLinearColor::White);
	WindowFrame->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
	AddCanvasChild(RootCanvas, WindowFrame, InventoryPaperPos, InventoryPaperSize);

	// Content is placed at Master V1 screen coordinates, so the content canvas
	// must live on the root at (0,0) — a sibling of the paper window, not a child.
	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryWindowFrameCanvas"));
	AddCanvasChild(RootCanvas, FrameCanvas, FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f));

	TitleTextBlock = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		28,
		FLinearColor(0.08f, 0.06f, 0.04f, 1.0f),
		TEXT("InventoryWindowTitleText"));
	AddCanvasChild(FrameCanvas, TitleTextBlock, FVector2D(383.0f, 230.0f), FVector2D(84.0f, 42.0f));

	// The town HUD already renders the ingot currency strip on this screen;
	// drawing it here would duplicate the display over the town shell.

	const EGameXXKCharacterBackpackTab CharacterTabs[] = {
		EGameXXKCharacterBackpackTab::Attributes,
		EGameXXKCharacterBackpackTab::Equipment,
		EGameXXKCharacterBackpackTab::Deck,
		EGameXXKCharacterBackpackTab::Talents,
		EGameXXKCharacterBackpackTab::Titles};
	const FText CharacterTabLabels[] = {
		NSLOCTEXT("GameXXKInventoryWindow", "CharacterTabAttributes", "属性"),
		NSLOCTEXT("GameXXKInventoryWindow", "CharacterTabEquipment", "装备"),
		NSLOCTEXT("GameXXKInventoryWindow", "CharacterTabDeck", "卡组"),
		NSLOCTEXT("GameXXKInventoryWindow", "CharacterTabTalents", "天赋"),
		NSLOCTEXT("GameXXKInventoryWindow", "CharacterTabTitles", "称号")};
	const FVector2D CharacterTabPositions[UE_ARRAY_COUNT(CharacterTabs)] = {
		FVector2D(514.0f, 220.0f), FVector2D(639.0f, 219.0f), FVector2D(764.0f, 220.0f), FVector2D(889.0f, 220.0f), FVector2D(1019.0f, 221.0f)};
	for (int32 TabIndex = 0; TabIndex < UE_ARRAY_COUNT(CharacterTabs); ++TabIndex)
	{
		UGameXXKCharacterBackpackTabButton* TabButton = WidgetTree->ConstructWidget<UGameXXKCharacterBackpackTabButton>(
			UGameXXKCharacterBackpackTabButton::StaticClass(),
			*FString::Printf(TEXT("InventoryCharacterTab_%d"), TabIndex));
		TabButton->Configure(this, CharacterTabs[TabIndex]);
		TabButton->SetStyle(MakeBoxTextureButtonStyle(CharacterTabNormalTexturePath, CharacterTabSize, FMargin(0.08f)));
		UTextBlock* TabText = MakeText(WidgetTree, CharacterTabLabels[TabIndex], 14);
		TabText->SetJustification(ETextJustify::Center);
		TabButton->AddChild(TabText);
		AddCanvasChild(FrameCanvas, TabButton, CharacterTabPositions[TabIndex], CharacterTabSize);
		CharacterTabButtons.Add(TabButton);
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryCloseButton"));
	CloseButton->SetStyle(MakeTextureButtonStyle(CloseButtonTexturePath, CloseButtonSize));
	CloseButton->SetBackgroundColor(FLinearColor::White);
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCloseClicked);
	AddCanvasChild(FrameCanvas, CloseButton, FVector2D(1652.0f, 201.0f), CloseButtonSize);

	LeftRailFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryLeftRailFrame"));
	LeftRailFrame->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, FVector2D(220.0f, 470.0f)));
	LeftRailFrame->SetBrushColor(FLinearColor::White);
	LeftRailFrame->SetPadding(FMargin(12.0f));
	AddCanvasChild(FrameCanvas, LeftRailFrame, FVector2D(420.0f, 340.0f), FVector2D(220.0f, 470.0f));

	UOverlay* LeftRailOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventoryLeftRailOverlay"));
	LeftRailFrame->AddChild(LeftRailOverlay);

	EquipmentPanelBox = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryEquipmentPanel"));
	LeftRailOverlay->AddChildToOverlay(EquipmentPanelBox);

	MerchantStockGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryMerchantStockGrid"));
	LeftRailOverlay->AddChildToOverlay(MerchantStockGrid);

	// Page 03: the viewed owner's presentation is wrapped by 3 equipment slots per side.
	CentralHeroIdleImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryCentralHeroIdle"));
	CentralHeroIdleImage->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
	AddCanvasChild(FrameCanvas, CentralHeroIdleImage, FVector2D(478.0f, 304.0f), FVector2D(518.0f, 518.0f));

	// Page 03 backpack: a 4x5 viewport into the scrollable warehouse, no panel
	// behind the cells — they sit directly on the paper window.
	BackpackScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryBackpackScrollBox"));
	BackpackScrollBox->SetOrientation(EOrientation::Orient_Vertical);
	BackpackScrollBox->SetAlwaysShowScrollbar(false);
	BackpackScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	BackpackScrollBox->OnUserScrolled.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleBackpackScrolled);
	BackpackScrollBox->SetScrollOffset(DeferredBackpackScrollOffset);
	AddCanvasChild(FrameCanvas, BackpackScrollBox, BackpackViewportPos, BackpackViewportSize);

	UCanvasPanel* BackpackContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryBackpackContentCanvas"));
	BackpackScrollBox->AddChild(BackpackContentCanvas);

	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryBackpackGrid"));
	BackpackGrid->SetSlotPadding(BackpackSlotPadding);
	AddCanvasChild(BackpackContentCanvas, BackpackGrid, BackpackContentOffset, BackpackGridSize);

	// Page 03 filter row: text labels above the backpack grid.
	const EGameXXKInventoryFilter InventoryFilters[] = {
		EGameXXKInventoryFilter::All,
		EGameXXKInventoryFilter::Equipment,
		EGameXXKInventoryFilter::Props,
		EGameXXKInventoryFilter::Materials,
		EGameXXKInventoryFilter::Tasks,
	};
	const FText InventoryFilterLabels[] = {
		NSLOCTEXT("GameXXKInventoryWindow", "FilterAll", "全部"),
		NSLOCTEXT("GameXXKInventoryWindow", "FilterEquipment", "装备"),
		NSLOCTEXT("GameXXKInventoryWindow", "FilterProps", "道具"),
		NSLOCTEXT("GameXXKInventoryWindow", "FilterMaterials", "材料"),
		NSLOCTEXT("GameXXKInventoryWindow", "FilterTasks", "任务")};
	for (int32 FilterIndex = 0; FilterIndex < UE_ARRAY_COUNT(InventoryFilters); ++FilterIndex)
	{
		const EGameXXKInventoryFilter Filter = InventoryFilters[FilterIndex];
		UGameXXKInventoryFilterButton* FilterButton = WidgetTree->ConstructWidget<UGameXXKInventoryFilterButton>(UGameXXKInventoryFilterButton::StaticClass(), *FString::Printf(TEXT("InventoryFilter_%d"), FilterIndex));
		FilterButton->Configure(this, Filter);
		FilterButton->SetStyle(MakeInvisibleButtonStyle());
		FilterButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UTextBlock* FilterText = MakeText(WidgetTree, InventoryFilterLabels[FilterIndex], 18, FLinearColor(0.20f, 0.14f, 0.09f, 1.0f));
		FilterText->SetJustification(ETextJustify::Center);
		FilterButton->AddChild(FilterText);
		AddCanvasChild(FrameCanvas, FilterButton, BackpackFilterRowPos + FVector2D(FilterIndex * BackpackFilterRowPitch, 0.0f), BackpackFilterRowSize);
		InventoryFilterButtons.Add(FilterButton);
		InventoryFilterTextBlocks.Add(FilterText);
	}

	// Page 03 right-side scrollbar: PSD track + thumb; the thumb follows scroll.
	if (UImage* ScrollbarTrack = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryScrollbarTrack")))
	{
		ScrollbarTrack->SetBrush(MakeTextureBrush(BackpackScrollbarTexturePath, InventoryScrollbarTrackSize));
		AddCanvasChild(FrameCanvas, ScrollbarTrack, InventoryScrollbarTrackPos, InventoryScrollbarTrackSize);
	}
	InventoryScrollbarThumb = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryScrollbarThumb"));
	InventoryScrollbarThumb->SetBrush(MakeTextureBrush(ScrollbarThumbTexturePath, InventoryScrollbarThumbSize));
	AddCanvasChild(FrameCanvas, InventoryScrollbarThumb, InventoryScrollbarThumbTop, InventoryScrollbarThumbSize);

	// Page 03 selection ink: one bracket above the selected backpack column.
	BackpackSelectionInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryBackpackSelectionInk"));
	BackpackSelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, BackpackSelectionInkSize));
	BackpackSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(FrameCanvas, BackpackSelectionInk, BackpackSelectionInkPos, BackpackSelectionInkSize);

	DecomposeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryDecomposeButton"));
	DecomposeButton->SetStyle(MakeBoxTextureButtonStyle(BackpackDisassembleTexturePath, DecomposeButtonSize, FMargin(0.08f)));
	DecomposeButton->SetBackgroundColor(FLinearColor::White);
	UTextBlock* DecomposeText = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Decompose", "分解"), 16, FLinearColor(0.95f, 0.90f, 0.80f, 1.0f));
	DecomposeText->SetJustification(ETextJustify::Center);
	DecomposeButton->AddChild(DecomposeText);
	DecomposeButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleDecomposeClicked);
	AddCanvasChild(FrameCanvas, DecomposeButton, FVector2D(935.0f, 878.0f), DecomposeButtonSize);

	// Enhancement / Reforge action buttons (tab styling) left of Decompose:
	// 强化 (725,878), 洗炼 (830,878), 分解 (935,878).
	EnhanceMainButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryEnhanceMainButton"));
	EnhanceMainButton->SetStyle(MakeBoxTextureButtonStyle(CharacterTabNormalTexturePath, DecomposeButtonSize, FMargin(0.08f)));
	EnhanceMainButton->SetBackgroundColor(FLinearColor::White);
	UTextBlock* EnhanceText = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "EnhanceMain", "强化"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	EnhanceText->SetJustification(ETextJustify::Center);
	EnhanceMainButton->AddChild(EnhanceText);
	EnhanceMainButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleEnhanceMainClicked);
	AddCanvasChild(FrameCanvas, EnhanceMainButton, FVector2D(725.0f, 878.0f), DecomposeButtonSize);

	ReforgeMainButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryReforgeMainButton"));
	ReforgeMainButton->SetStyle(MakeBoxTextureButtonStyle(CharacterTabNormalTexturePath, DecomposeButtonSize, FMargin(0.08f)));
	ReforgeMainButton->SetBackgroundColor(FLinearColor::White);
	UTextBlock* ReforgeText = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "ReforgeMain", "洗炼"), 16, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
	ReforgeText->SetJustification(ETextJustify::Center);
	ReforgeMainButton->AddChild(ReforgeText);
	ReforgeMainButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleReforgeMainClicked);
	AddCanvasChild(FrameCanvas, ReforgeMainButton, FVector2D(830.0f, 878.0f), DecomposeButtonSize);

	DetailPanelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryDetailPanel"));
	DetailPanelFrame->SetBrush(MakeBoxTextureBrush(PanelFrameTexturePath, FVector2D(246.0f, 470.0f)));
	DetailPanelFrame->SetBrushColor(FLinearColor::White);
	DetailPanelFrame->SetPadding(FMargin(14.0f));
	AddCanvasChild(FrameCanvas, DetailPanelFrame, FVector2D(1305.0f, 340.0f), FVector2D(246.0f, 470.0f));

	UVerticalBox* DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryDetailBox"));
	DetailPanelFrame->AddChild(DetailBox);
	SelectedNameTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionTitle", "选择物品"), 20, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
	DetailBox->AddChildToVerticalBox(SelectedNameTextBlock);
	SelectedDetailTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionDetail", "从背包、商店或装备槽中选择。"), 15);
	SelectedDetailTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* DetailTextSlot = DetailBox->AddChildToVerticalBox(SelectedDetailTextBlock))
	{
		DetailTextSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 12.0f));
	}
	UTextBlock* RawPrimaryActionText = nullptr;
	PrimaryActionButton = MakeActionButton(WidgetTree, FText::GetEmpty(), RawPrimaryActionText);
	PrimaryActionTextBlock = RawPrimaryActionText;
	PrimaryActionButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandlePrimaryActionClicked);
	if (UVerticalBoxSlot* PrimarySlot = DetailBox->AddChildToVerticalBox(PrimaryActionButton))
	{
		PrimarySlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
	UTextBlock* RawSecondaryActionText = nullptr;
	SecondaryActionButton = MakeActionButton(WidgetTree, FText::GetEmpty(), RawSecondaryActionText);
	SecondaryActionTextBlock = RawSecondaryActionText;
	SecondaryActionButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleSecondaryActionClicked);
	if (UVerticalBoxSlot* SecondarySlot = DetailBox->AddChildToVerticalBox(SecondaryActionButton))
	{
		SecondarySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
	UTextBlock* RawEnhanceActionText = nullptr;
	EnhanceButton = MakeActionButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "EnhanceAction", "强化"), RawEnhanceActionText);
	EnhanceActionTextBlock = RawEnhanceActionText;
	EnhanceButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleEnhanceClicked);
	if (UVerticalBoxSlot* EnhanceSlot = DetailBox->AddChildToVerticalBox(EnhanceButton))
	{
		EnhanceSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}

	// Attribute/Talent/Title body occupies the backpack grid area without a paper back.
	CharacterTabBodyPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryCharacterTabBodyPanel"));
	{
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		CharacterTabBodyPanel->SetBrush(Transparent);
	}
	CharacterTabBodyPanel->SetPadding(FMargin(24.0f, 20.0f));
	UVerticalBox* CharacterBodyStack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("InventoryCharacterTabBodyStack"));
	CharacterTabBodyText = MakeText(WidgetTree, FText::GetEmpty(), 20, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
	if (UVerticalBoxSlot* BodySlot = CharacterBodyStack->AddChildToVerticalBox(CharacterTabBodyText))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	CharacterExperienceText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("InventoryCharacterExperienceText"));
	CharacterExperienceText->SetText(FText::GetEmpty());
	CharacterExperienceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.10f, 0.07f, 0.04f, 1.0f)));
	CharacterExperienceText->SetAutoWrapText(false);
	CharacterExperienceText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17));
	if (UVerticalBoxSlot* ExperienceTextSlot = CharacterBodyStack->AddChildToVerticalBox(CharacterExperienceText))
	{
		ExperienceTextSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 5.0f));
	}
	CharacterExperienceBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("InventoryCharacterExperienceBar"));
	CharacterExperienceBar->SetPercent(0.0f);
	CharacterExperienceBar->SetFillColorAndOpacity(FLinearColor(0.78f, 0.46f, 0.12f, 1.0f));
	CharacterBodyStack->AddChildToVerticalBox(CharacterExperienceBar);
	CharacterTabBodyPanel->SetContent(CharacterBodyStack);
	AddCanvasChild(FrameCanvas, CharacterTabBodyPanel, FVector2D(1135.0f, 300.0f), FVector2D(488.0f, 650.0f));

	// Deck body occupies the backpack grid area without a paper back.
	HeroDeckPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryHeroDeckPanel"));
	{
		FSlateBrush Transparent;
		Transparent.DrawAs = ESlateBrushDrawType::NoDrawType;
		HeroDeckPanel->SetBrush(Transparent);
	}
	HeroDeckPanel->SetPadding(FMargin(24.0f, 20.0f));
	AddCanvasChild(FrameCanvas, HeroDeckPanel, FVector2D(1135.0f, 300.0f), FVector2D(488.0f, 650.0f));
	UCanvasPanel* HeroDeckCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryHeroDeckCanvas"));
	HeroDeckPanel->SetContent(HeroDeckCanvas);
	HeroDeckCaptionText = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "HeroDeckCaption", "卡组背包 36 张 · 角色卡组 8 张"), 17);
	AddCanvasChild(HeroDeckCanvas, HeroDeckCaptionText.Get(), FVector2D::ZeroVector, FVector2D(470.0f, 28.0f));
	HeroDeckGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryHeroDeckGrid"));
	HeroDeckGrid->SetSlotPadding(FMargin(5.0f));
	// Keep the approved three-column viewport; all thirty-six cards are reached
	// through the existing vertical scroll box without moving surrounding UI.
	HeroDeckScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("InventoryHeroDeckScrollBox"));
	HeroDeckScrollBox->SetOrientation(EOrientation::Orient_Vertical);
	HeroDeckScrollBox->SetAlwaysShowScrollbar(false);
	HeroDeckScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	HeroDeckScrollBox->AddChild(HeroDeckGrid);
	AddCanvasChild(HeroDeckCanvas, HeroDeckScrollBox, FVector2D(0.0f, 34.0f), FVector2D(470.0f, 500.0f));
	// Apply button centered below the deck grid, with a (x/8) pick counter.
	ApplyHeroDeckButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryApplyHeroDeckButton"));
	ApplyHeroDeckButton->SetStyle(MakeTextureButtonStyle(BackpackDisassembleTexturePath, FVector2D(120.0f, 42.0f)));
	ApplyHeroDeckButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleApplyHeroDeckClicked);
	UTextBlock* ApplyHeroDeckText = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "ApplyHeroDeck", "应用卡组"), 14);
	ApplyHeroDeckText->SetJustification(ETextJustify::Center);
	ApplyHeroDeckButton->AddChild(ApplyHeroDeckText);
	AddCanvasChild(HeroDeckCanvas, ApplyHeroDeckButton, FVector2D(175.0f, 550.0f), FVector2D(120.0f, 42.0f));
	HeroDeckCountText = MakeText(WidgetTree, FText::GetEmpty(), 14, FLinearColor(0.10f, 0.07f, 0.04f, 1.0f));
	HeroDeckCountText->SetJustification(ETextJustify::Center);
	AddCanvasChild(HeroDeckCanvas, HeroDeckCountText, FVector2D(175.0f, 596.0f), FVector2D(120.0f, 22.0f));
	for (int32 CardIndex = 0; CardIndex < 36; ++CardIndex)
	{
		UGameXXKHeroDeckCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKHeroDeckCardButton>(
			UGameXXKHeroDeckCardButton::StaticClass(),
			*FString::Printf(TEXT("InventoryHeroDeckCard_%02d"), CardIndex));
		// Card portraits are shared with the in-battle card face and contain only
		// character art.  Draw the approved shared parchment as the actual button
		// background, then inset the portrait inside it.
		FButtonStyle CardFrameStyle = MakeTextureButtonStyle(HeroCardFrameTexturePath, HeroDeckCardSize);
		CardFrameStyle.SetNormalPadding(FMargin(0.0f));
		CardFrameStyle.SetPressedPadding(FMargin(0.0f));
		CardButton->SetStyle(CardFrameStyle);
		CardButton->SetBackgroundColor(FLinearColor::White);
		UOverlay* CardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* CardPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("InventoryHeroDeckPortrait_%02d"), CardIndex));
		CardPortrait->SetBrush(MakeTextureBrush(HeroCardPortraitTexturePath, HeroDeckPortraitSize));
		if (UOverlaySlot* PortraitSlot = CardOverlay->AddChildToOverlay(CardPortrait))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Fill);
			PortraitSlot->SetVerticalAlignment(VAlign_Fill);
			PortraitSlot->SetPadding(FMargin(5.0f, 32.0f, 5.0f, 6.0f));
		}
		// Selection ink sits under the name so the selected card name stays visible.
		UImage* SelectedInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("InventoryHeroDeckSelectedInk_%02d"), CardIndex));
		SelectedInk->SetBrush(MakeBoxTextureBrush(RectangularSelectionTexturePath, BackpackSelectionInkSize));
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
		// Cost summary: second line "x气", third line "x内", left-aligned.
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
		// Hover tooltip: card name + effect description.
		UBorder* CardTooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("InventoryHeroDeckTooltip_%02d"), CardIndex));
		CardTooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, FVector2D(260.0f, 120.0f)));
		CardTooltipFrame->SetBrushColor(FLinearColor::White);
		CardTooltipFrame->SetPadding(FMargin(16.0f, 12.0f));
		UVerticalBox* CardTooltipBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		CardTooltipFrame->AddChild(CardTooltipBox);
		UTextBlock* TooltipName = MakeText(WidgetTree, FText::GetEmpty(), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
		CardTooltipBox->AddChildToVerticalBox(TooltipName);
		UTextBlock* TooltipDetail = MakeText(WidgetTree, FText::GetEmpty(), 13, FLinearColor(0.14f, 0.11f, 0.08f, 1.0f));
		TooltipDetail->SetAutoWrapText(true);
		if (UVerticalBoxSlot* TooltipDetailSlot = CardTooltipBox->AddChildToVerticalBox(TooltipDetail))
		{
			TooltipDetailSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		}
		CardButton->SetToolTip(CardTooltipFrame);
		HeroDeckTooltipNameBlocks.Add(TooltipName);
		HeroDeckTooltipDetailBlocks.Add(TooltipDetail);
		UImage* LockedIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("InventoryHeroDeckLockedIcon_%02d"), CardIndex));
		LockedIcon->SetBrush(MakeTextureBrush(HeroLockedCardIconTexturePath, FVector2D(34.0f, 34.0f)));
		LockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* LockSlot = CardOverlay->AddChildToOverlay(LockedIcon))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Center);
			LockSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* UnlockLabel = MakeText(
			WidgetTree,
			FText::GetEmpty(),
			10,
			FLinearColor(0.42f, 0.08f, 0.04f, 1.0f),
			*FString::Printf(TEXT("InventoryHeroDeckUnlockText_%02d"), CardIndex));
		UnlockLabel->SetJustification(ETextJustify::Center);
		UnlockLabel->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* UnlockSlot = CardOverlay->AddChildToOverlay(UnlockLabel))
		{
			UnlockSlot->SetHorizontalAlignment(HAlign_Fill);
			UnlockSlot->SetVerticalAlignment(VAlign_Bottom);
			UnlockSlot->SetPadding(FMargin(3.0f, 0.0f, 3.0f, 5.0f));
		}
		CardButton->AddChild(CardOverlay);
		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSize->SetWidthOverride(HeroDeckCardSize.X);
		CardSize->SetHeightOverride(HeroDeckCardSize.Y);
		CardSize->AddChild(CardButton);
		if (UUniformGridSlot* CardSlot = HeroDeckGrid->AddChildToUniformGrid(CardSize, CardIndex / 3, CardIndex % 3))
		{
			CardSlot->SetHorizontalAlignment(HAlign_Center);
			CardSlot->SetVerticalAlignment(VAlign_Center);
		}
		HeroDeckCardButtons.Add(CardButton);
		HeroDeckCardPortraits.Add(CardPortrait);
		HeroDeckCardLabels.Add(CardLabel);
		HeroDeckCostLabels.Add(CostQiLabel);
		HeroDeckManaCostLabels.Add(CostManaLabel);
		HeroDeckLockedIcons.Add(LockedIcon);
		HeroDeckSelectedInks.Add(SelectedInk);
	}

	for (int32 SlotIndex = 0; SlotIndex < BackpackStorageCapacity; ++SlotIndex)
	{
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryBackpackSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::PlayerBackpack, SlotIndex);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(BackpackSlotTexturePath, BackpackSlotSize, SlotFrameMargin));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SlotIcon);

		UTextBlock* SlotLabel = MakeText(
			WidgetTree,
			FText::GetEmpty(),
			14,
			FLinearColor::White,
			*FString::Printf(TEXT("InventoryBackpackStackCount_%02d"), SlotIndex));
		SlotLabel->SetJustification(ETextJustify::Right);
		FSlateFontInfo StackCountFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
		StackCountFont.OutlineSettings.OutlineSize = 2;
		StackCountFont.OutlineSettings.OutlineColor = FLinearColor::Black;
		SlotLabel->SetFont(StackCountFont);
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Right);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 2.0f));
		}
		UImage* LockedIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("InventoryBackpackLockedIcon_%03d"), SlotIndex));
		LockedIcon->SetBrush(MakeTextureBrush(
			HeroLockedCardIconTexturePath,
			FVector2D(34.0f, 34.0f)));
		LockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* LockSlot = SlotOverlay->AddChildToOverlay(LockedIcon))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Right);
			LockSlot->SetVerticalAlignment(VAlign_Top);
			LockSlot->SetPadding(FMargin(4.0f));
		}

		UBorder* TooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("InventoryBackpackTooltip_%02d"), SlotIndex));
		TooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, FVector2D(260.0f, 120.0f)));
		TooltipFrame->SetBrushColor(FLinearColor::White);
		TooltipFrame->SetPadding(FMargin(16.0f, 12.0f));
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
		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSizeBox->SetWidthOverride(BackpackSlotSize.X);
		SlotSizeBox->SetHeightOverride(BackpackSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = BackpackGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / BackpackColumns, SlotIndex % BackpackColumns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		BackpackSlotButtons.Add(SlotButton);
		BackpackSlotIcons.Add(SlotIcon);
		BackpackLockedIcons.Add(LockedIcon);
		BackpackSlotLabels.Add(SlotLabel);
		BackpackTooltipFrames.Add(TooltipFrame);
		BackpackTooltipNameTextBlocks.Add(TooltipName);
		BackpackTooltipDetailTextBlocks.Add(TooltipDetail);
	}

	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	for (int32 SlotIndex = 0; SlotIndex < ShopItems.Num(); ++SlotIndex)
	{
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryMerchantStockSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::MerchantStock, SlotIndex);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(BackpackSlotTexturePath, BackpackSlotSize, SlotFrameMargin));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotOverlay->AddChildToOverlay(SlotIcon);
		UTextBlock* SlotLabel = MakeText(WidgetTree, FText::GetEmpty(), 11, FLinearColor::White);
		SlotLabel->SetJustification(ETextJustify::Right);
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Right);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		}
		UImage* SelectedOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SelectedOverlay->SetBrush(MakeTextureBrush(SelectionInkTexturePath, BackpackSlotSize, FLinearColor::White));
		SelectedOverlay->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SelectedOverlay);
		SlotButton->AddChild(SlotOverlay);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSizeBox->SetWidthOverride(BackpackSlotSize.X);
		SlotSizeBox->SetHeightOverride(BackpackSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = MerchantStockGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / 2, SlotIndex % 2))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		MerchantStockSlotButtons.Add(SlotButton);
		MerchantStockSlotIcons.Add(SlotIcon);
		MerchantStockSlotLabels.Add(SlotLabel);
		MerchantStockSelectedOverlays.Add(SelectedOverlay);
	}

	const TArray<TPair<FName, FText>> EquipmentSlots = {
		TPair<FName, FText>(WeaponSlotId, NSLOCTEXT("GameXXKInventoryWindow", "WeaponSlot", "武器")),
		TPair<FName, FText>(HeadSlotId, NSLOCTEXT("GameXXKInventoryWindow", "HeadSlot", "头部")),
		TPair<FName, FText>(ArmorSlotId, NSLOCTEXT("GameXXKInventoryWindow", "ArmorSlot", "衣甲")),
		TPair<FName, FText>(BeltSlotId, NSLOCTEXT("GameXXKInventoryWindow", "BeltSlot", "腰带")),
		TPair<FName, FText>(ShoesSlotId, NSLOCTEXT("GameXXKInventoryWindow", "ShoesSlot", "鞋")),
		TPair<FName, FText>(AccessorySlotId, NSLOCTEXT("GameXXKInventoryWindow", "AccessorySlot", "饰品")),
	};
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlots.Num(); ++SlotIndex)
	{
		const TPair<FName, FText>& SlotDef = EquipmentSlots[SlotIndex];
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryEquipmentSlot_%s"), *SlotDef.Key.ToString()));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::Equipment, SlotIndex, SlotDef.Key);
		SlotButton->SetStyle(MakeBoxTextureButtonStyle(EquipmentSlotTexturePath, EquipmentSlotSize, SlotFrameMargin));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = SlotOverlay->AddChildToOverlay(SlotIcon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
		UTextBlock* SlotLabel = MakeText(WidgetTree, SlotDef.Value, 12, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 4.0f));
		}
		UImage* LockedIcon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(
				TEXT("InventoryEquipmentLockedIcon_%s"),
				*SlotDef.Key.ToString()));
		LockedIcon->SetBrush(MakeTextureBrush(
			HeroLockedCardIconTexturePath,
			FVector2D(34.0f, 34.0f)));
		LockedIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* LockSlot = SlotOverlay->AddChildToOverlay(LockedIcon))
		{
			LockSlot->SetHorizontalAlignment(HAlign_Right);
			LockSlot->SetVerticalAlignment(VAlign_Top);
			LockSlot->SetPadding(FMargin(4.0f));
		}
		SlotButton->AddChild(SlotOverlay);

		// Hover tooltip paper for the equipped item (hidden while the slot is empty).
		UBorder* TooltipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("InventoryEquipmentTooltip_%s"), *SlotDef.Key.ToString()));
		TooltipFrame->SetBrush(MakeBoxTextureBrush(TooltipPaperTexturePath, FVector2D(260.0f, 120.0f)));
		TooltipFrame->SetBrushColor(FLinearColor::White);
		TooltipFrame->SetPadding(FMargin(16.0f, 12.0f));
		TooltipFrame->SetVisibility(ESlateVisibility::Collapsed);
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

		// Page 03 wraps the central hero with 3 equipment frames per side at fixed coordinates.
		AddCanvasChild(FrameCanvas, SlotButton, EquipmentFramePositions[SlotIndex], EquipmentSlotSize);
		EquipmentSlotButtons.Add(SlotButton);
		EquipmentSlotIcons.Add(SlotIcon);
		EquipmentLockedIcons.Add(LockedIcon);
		EquipmentSlotLabels.Add(SlotLabel);
		EquipmentTooltipFrames.Add(TooltipFrame);
		EquipmentTooltipNameTextBlocks.Add(TooltipName);
		EquipmentTooltipDetailTextBlocks.Add(TooltipDetail);
	}

	// Selection ink hovering above the selected equipment slot (like the
	// backpack column ink): a small bracket asset, never a full-slot fill.
	EquipmentSelectionInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryEquipmentSelectionInk"));
	EquipmentSelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, BackpackSelectionInkSize));
	EquipmentSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(FrameCanvas, EquipmentSelectionInk, BackpackSelectionInkPos, BackpackSelectionInkSize);

	ConfirmationDialogFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryConfirmationDialogFrame"));
	ConfirmationDialogFrame->SetBrush(MakeBoxTextureBrush(ConfirmationDialogTexturePath, FVector2D(520.0f, 260.0f)));
	ConfirmationDialogFrame->SetBrushColor(FLinearColor::White);
	ConfirmationDialogFrame->SetPadding(FMargin(34.0f, 30.0f));
	AddCanvasChild(RootCanvas, ConfirmationDialogFrame, FVector2D::ZeroVector, FVector2D(520.0f, 260.0f), FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	UVerticalBox* DialogBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryConfirmationDialogBox"));
	ConfirmationDialogFrame->AddChild(DialogBox);
	ConfirmationPromptTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "ConfirmPrompt", "确认操作？"), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
	DialogBox->AddChildToVerticalBox(ConfirmationPromptTextBlock);
	ConfirmationSummaryTextBlock = MakeText(WidgetTree, FText::GetEmpty(), 15);
	if (UVerticalBoxSlot* SummarySlot = DialogBox->AddChildToVerticalBox(ConfirmationSummaryTextBlock))
	{
		SummarySlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 14.0f));
	}
	UHorizontalBox* DialogButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DialogBox->AddChildToVerticalBox(DialogButtons);
	UTextBlock* ConfirmText = nullptr;
	ConfirmationConfirmButton = MakeActionButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Confirm", "确认"), ConfirmText);
	ConfirmationConfirmButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleConfirmClicked);
	if (UHorizontalBoxSlot* ConfirmSlot = DialogButtons->AddChildToHorizontalBox(ConfirmationConfirmButton))
	{
		ConfirmSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UTextBlock* CancelText = nullptr;
	ConfirmationCancelButton = MakeActionButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Cancel", "取消"), CancelText);
	ConfirmationCancelButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCancelClicked);
	if (UHorizontalBoxSlot* CancelSlot = DialogButtons->AddChildToHorizontalBox(ConfirmationCancelButton))
	{
		CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RefreshProgrammaticLayout();
}

void UGameXXKInventoryWindowWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();
	const bool bWindowVisible = WindowMode != EGameXXKInventoryWindowMode::None;
	const bool bEquipmentBackpackVisible = WindowMode == EGameXXKInventoryWindowMode::MerchantTrade
		|| (WindowMode == EGameXXKInventoryWindowMode::FreeInventory
			&& ActiveCharacterTab == EGameXXKCharacterBackpackTab::Equipment);
	SetVisibility(bWindowVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ModalBackdrop)
	{
		ModalBackdrop->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (WindowFrame)
	{
		WindowFrame->SetVisibility(bWindowVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(bWindowVisible && !bDesktopTrainingEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TitleTextBlock)
	{
		const bool bMerchantTitle = WindowMode == EGameXXKInventoryWindowMode::MerchantTrade;
		TitleTextBlock->SetText(bMerchantTitle
			? NSLOCTEXT("GameXXKInventoryWindow", "TitleTrade", "商铺")
			: NSLOCTEXT("GameXXKInventoryWindow", "TitleBackpack", "背包"));
		TitleTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (CloseButton)
		{
			CloseButton->SetStyle(MakeTextureButtonStyle(CloseButtonTexturePath, CloseButtonSize));
		}
	}
	if (EquipmentPanelBox)
	{
		EquipmentPanelBox->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CentralHeroIdleImage)
	{
		CentralHeroIdleImage->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	RefreshCentralCharacterPresentation();
	if (MerchantStockGrid)
	{
		MerchantStockGrid->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (int32 FilterIndex = 0; FilterIndex < InventoryFilterButtons.Num(); ++FilterIndex)
	{
		if (UGameXXKInventoryFilterButton* FilterButton = InventoryFilterButtons[FilterIndex])
		{
			const bool bSelected = static_cast<int32>(ActiveInventoryFilter) == FilterIndex;
			FilterButton->SetVisibility(bEquipmentBackpackVisible && !bDesktopTrainingEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* FilterText = InventoryFilterTextBlocks.IsValidIndex(FilterIndex) ? InventoryFilterTextBlocks[FilterIndex].Get() : nullptr)
		{
			const bool bSelected = static_cast<int32>(ActiveInventoryFilter) == FilterIndex;
			FilterText->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor(0.80f, 0.45f, 0.12f, 1.0f)
				: FLinearColor(0.20f, 0.14f, 0.09f, 1.0f)));
		}
	}
	if (DecomposeButton)
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const bool bCanDecompose = Subsystem
			&& (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack || SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
			&& (!SelectedItemId.IsNone() || !SelectedEquipmentInstanceId.IsNone());
		DecomposeButton->SetVisibility(bEquipmentBackpackVisible && !bDesktopTrainingEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		DecomposeButton->SetIsEnabled(bCanDecompose);
	}
	// Enhance/Reforge actions only exist on the equipment backpack tab.
	if (EnhanceMainButton)
	{
		EnhanceMainButton->SetVisibility(bEquipmentBackpackVisible && !bDesktopTrainingEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ReforgeMainButton)
	{
		ReforgeMainButton->SetVisibility(bEquipmentBackpackVisible && !bDesktopTrainingEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	RefreshBackpackSlots();
	RefreshMerchantStockSlots();
	RefreshEquipmentSlots();
	RefreshDetailPanel();
	RefreshConfirmationDialog();
	if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Deck)
	{
		RefreshHeroDeckCards();
	}
	RefreshCharacterTabs();
	UpdateBackpackScrollbarThumb();
}

void UGameXXKInventoryWindowWidget::RefreshVisibleRuntimeValues()
{
	if (WindowMode == EGameXXKInventoryWindowMode::None
		|| GetVisibility() == ESlateVisibility::Collapsed
		|| GetVisibility() == ESlateVisibility::Hidden)
	{
		return;
	}

	RefreshCharacterTabs();
	if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Equipment)
	{
		RefreshBackpackSlots();
		RefreshEquipmentSlots();
		RefreshDetailPanel();
		UpdateBackpackScrollbarThumb();
	}
	else if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Deck)
	{
		RefreshHeroDeckCards();
	}
}

void UGameXXKInventoryWindowWidget::RefreshCharacterTabs()
{
	const bool bFreeInventory = WindowMode == EGameXXKInventoryWindowMode::FreeInventory;
	const bool bMerchant = WindowMode == EGameXXKInventoryWindowMode::MerchantTrade;
	const bool bEquipmentTab = bFreeInventory && ActiveCharacterTab == EGameXXKCharacterBackpackTab::Equipment;
	const bool bShowEquipmentBackpack = bMerchant || bEquipmentTab;

	const EGameXXKCharacterBackpackTab Tabs[] = {
		EGameXXKCharacterBackpackTab::Attributes,
		EGameXXKCharacterBackpackTab::Equipment,
		EGameXXKCharacterBackpackTab::Deck,
		EGameXXKCharacterBackpackTab::Talents,
		EGameXXKCharacterBackpackTab::Titles};
	for (int32 Index = 0; Index < CharacterTabButtons.Num(); ++Index)
	{
		if (UGameXXKCharacterBackpackTabButton* Button = CharacterTabButtons[Index])
		{
			const bool bSelected = bFreeInventory && UE_ARRAY_COUNT(Tabs) > Index && Tabs[Index] == ActiveCharacterTab;
			const bool bAllowedInEmbeddedMode = !bDesktopTrainingEmbeddedMode || Index < 3;
			Button->SetVisibility(bFreeInventory && bAllowedInEmbeddedMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Button->SetStyle(MakeBoxTextureButtonStyle(
				bSelected ? CharacterTabSelectedTexturePath : CharacterTabNormalTexturePath,
				CharacterTabSize,
				FMargin(0.08f)));
		}
	}

	if (LeftRailFrame)
	{
		LeftRailFrame->SetVisibility(bMerchant ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (EquipmentPanelBox)
	{
		EquipmentPanelBox->SetVisibility(bFreeInventory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CentralHeroIdleImage)
	{
		CentralHeroIdleImage->SetVisibility(bFreeInventory ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (MerchantStockGrid)
	{
		MerchantStockGrid->SetVisibility(bMerchant ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BackpackScrollBox)
	{
		BackpackScrollBox->SetVisibility(bShowEquipmentBackpack ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (DetailPanelFrame)
	{
		// Page 03 has no separate detail column; item info shows on selection.
		DetailPanelFrame->SetVisibility(ESlateVisibility::Collapsed);
	}

	const bool bShowDeck = bFreeInventory && ActiveCharacterTab == EGameXXKCharacterBackpackTab::Deck;
	const bool bShowBody = bFreeInventory
		&& ActiveCharacterTab != EGameXXKCharacterBackpackTab::Equipment
		&& ActiveCharacterTab != EGameXXKCharacterBackpackTab::Deck;
	if (HeroDeckPanel)
	{
		HeroDeckPanel->SetVisibility(bShowDeck ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CharacterTabBodyPanel)
	{
		CharacterTabBodyPanel->SetVisibility(bShowBody ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const bool bShowExperience = bShowBody
		&& ActiveCharacterTab == EGameXXKCharacterBackpackTab::Attributes;
	if (CharacterExperienceText)
	{
		CharacterExperienceText->SetVisibility(bShowExperience
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (CharacterExperienceBar)
	{
		CharacterExperienceBar->SetVisibility(bShowExperience
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (CharacterTabBodyText && bShowBody)
	{
		if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Attributes)
		{
			if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
			{
				const auto& State = Subsystem->GetRuntimeState();
				const FName CharacterId = ResolveInventoryCharacterId();
				FGameXXKEquipmentLoadoutSnapshot Snapshot;
				if (Subsystem->GetEquipmentLoadoutSnapshot(CharacterId, Snapshot))
				{
					const FGameXXKCharacterStats& Stats = Snapshot.AttributesBeforeRoute;
					const bool bHero = CharacterId == FGameXXKEquipmentRules::HeroCharacterId();
					const bool bQuestNpc =
						FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId) != nullptr;
					const FString CharacterLabel = bHero ? TEXT("主角") : CharacterId.ToString();
					int32 CharacterLevel = State.PlayerLevel;
					int32 CharacterExperience = State.PlayerXP;
					int32 RequiredExperience =
						UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(CharacterLevel);
					if (bQuestNpc)
					{
						if (const FGameXXKQuestNpcProgression* Progression =
							State.CardRun.PartySelection.QuestNpcProgressions.Find(CharacterId))
						{
							CharacterLevel = Progression->Level;
							CharacterExperience = Progression->Experience;
							RequiredExperience =
								UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(CharacterLevel);
						}
					}
					else if (!bHero)
					{
						FGameXXKPermanentCompanion Companion;
						if (Subsystem->TryGetPermanentCompanionView(CharacterId, Companion))
						{
							CharacterLevel = Companion.Level;
							CharacterExperience = Companion.Experience;
							RequiredExperience =
								FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(CharacterLevel);
						}
					}
					CharacterTabBodyText->SetText(FText::FromString(FString::Printf(
						TEXT("%s属性\n\n等级  %d\n生命  %d / %d\n内力  %d / %d\n攻击  %d\n防御  %d\n速度  %d"),
						*CharacterLabel,
						CharacterLevel,
						bHero ? State.PlayerHP : Stats.MaxHealth,
						Stats.MaxHealth,
						bHero ? State.PlayerMP : Stats.MaxMana,
						Stats.MaxMana,
						Stats.Attack,
						Stats.Defense,
						Stats.Speed)));
					if (CharacterExperienceText)
					{
						CharacterExperienceText->SetText(RequiredExperience > 0
							? FText::FromString(FString::Printf(
								TEXT("经验  %d / %d"),
								CharacterExperience,
								RequiredExperience))
							: FText::FromString(TEXT("经验  已满级")));
					}
					if (CharacterExperienceBar)
					{
						CharacterExperienceBar->SetPercent(RequiredExperience > 0
							? FMath::Clamp(
								static_cast<float>(CharacterExperience) / static_cast<float>(RequiredExperience),
								0.0f,
								1.0f)
							: 1.0f);
					}
				}
				else
				{
					CharacterTabBodyText->SetText(NSLOCTEXT("GameXXKInventoryWindow", "AttributesUnavailable", "角色属性\n\n暂无运行时数据"));
				}
			}
			else
			{
				CharacterTabBodyText->SetText(NSLOCTEXT("GameXXKInventoryWindow", "AttributesUnavailable", "主角属性\n\n暂无运行时数据"));
			}
		}
		else if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Talents)
		{
			CharacterTabBodyText->SetText(NSLOCTEXT("GameXXKInventoryWindow", "TalentsUnavailable", "天赋\n\n尚未开放"));
		}
		else
		{
			CharacterTabBodyText->SetText(NSLOCTEXT("GameXXKInventoryWindow", "TitlesUnavailable", "称号\n\n尚未开放"));
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshHeroDeckCards()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FName CharacterId = ResolveInventoryCharacterId();
	const int32 RequiredCount = GetConfiguredDeckRequiredCount();
	int32 CompanionLevel = 0;
	HeroCardBackpackIds.Reset();
	UnlockedHeroCardIds.Reset();
	if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
	{
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				HeroCardBackpackIds.Add(Definition.Id);
			}
		}
		if (Subsystem)
		{
			UnlockedHeroCardIds = Subsystem->GetRuntimeState().CardRun.HeroUnlockedCardIds;
			if (PendingHeroDeckIds.IsEmpty())
			{
				PendingHeroDeckIds = Subsystem->GetHeroCardLoadout();
			}
		}
	}
	else if (const FGameXXKQuestNpcDefinition* NpcDefinition =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId))
	{
		HeroCardBackpackIds = NpcDefinition->FixedCardIds;
		UnlockedHeroCardIds = HeroCardBackpackIds;
		if (Subsystem && PendingHeroDeckIds.IsEmpty())
		{
			if (const FGameXXKQuestNpcOwnedCardLoadout* Loadout =
				Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpcCardLoadouts.Find(CharacterId))
			{
				PendingHeroDeckIds = Loadout->SelectedCardIds;
			}
		}
	}
	else if (Subsystem)
	{
		FGameXXKPermanentCompanion Companion;
		if (Subsystem->TryGetPermanentCompanionView(CharacterId, Companion))
		{
			CompanionLevel = Companion.Level;
			HeroCardBackpackIds = Companion.PersonalCardIds;
			UnlockedHeroCardIds = Companion.UnlockedPersonalCardIds;
			if (PendingHeroDeckIds.IsEmpty())
			{
				PendingHeroDeckIds = Companion.SelectedCardIds;
			}
		}
	}
	if (HeroDeckCaptionText)
	{
		HeroDeckCaptionText->SetText(FText::FromString(FString::Printf(
			TEXT("卡组背包 %d 张 · 角色卡组 %d 张"),
			HeroCardBackpackIds.Num(),
			RequiredCount)));
	}

	const bool bMutationLocked = !Subsystem || Subsystem->IsCompanionLoadoutMutationLocked();
	for (int32 Index = 0; Index < HeroDeckCardButtons.Num(); ++Index)
	{
		const FName CardId = HeroCardBackpackIds.IsValidIndex(Index) ? HeroCardBackpackIds[Index] : NAME_None;
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		const bool bUnlocked = !CardId.IsNone() && UnlockedHeroCardIds.Contains(CardId);
		const bool bSelected = PendingHeroDeckIds.Contains(CardId);
		if (UGameXXKHeroDeckCardButton* Button = HeroDeckCardButtons[Index])
		{
			Button->Configure(this, CardId);
			Button->SetVisibility(CardId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
			Button->SetIsEnabled(bUnlocked && !bMutationLocked);
			// The button owns the shared parchment frame; selection is a separate
			// ink overlay and therefore never replaces the card base.
			Button->SetBackgroundColor(FLinearColor::White);
		}
		if (UImage* SelectedInk = HeroDeckSelectedInks.IsValidIndex(Index) ? HeroDeckSelectedInks[Index].Get() : nullptr)
		{
			SelectedInk->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* Label = HeroDeckCardLabels.IsValidIndex(Index) ? HeroDeckCardLabels[Index].Get() : nullptr)
		{
			const FText CardName = Definition ? Definition->DisplayName : FText::FromName(CardId);
			Label->SetText(CardName);
			// Selected card name turns white and renders above the selection ink.
			Label->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor::White
				: FLinearColor(0.10f, 0.07f, 0.04f, 1.0f)));
		}
		if (UTextBlock* CostLabel = HeroDeckCostLabels.IsValidIndex(Index) ? HeroDeckCostLabels[Index].Get() : nullptr)
		{
			CostLabel->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: FText::FromString(FString::Printf(TEXT("%d气"), Definition->EnergyCost)));
		}
		if (UTextBlock* ManaCostLabel = HeroDeckManaCostLabels.IsValidIndex(Index) ? HeroDeckManaCostLabels[Index].Get() : nullptr)
		{
			ManaCostLabel->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: FText::FromString(FString::Printf(TEXT("%d内"), Definition->ManaCost)));
		}
		if (UTextBlock* TooltipName = HeroDeckTooltipNameBlocks.IsValidIndex(Index) ? HeroDeckTooltipNameBlocks[Index].Get() : nullptr)
		{
			TooltipName->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: Definition->DisplayName);
		}
		if (UTextBlock* TooltipDetail = HeroDeckTooltipDetailBlocks.IsValidIndex(Index) ? HeroDeckTooltipDetailBlocks[Index].Get() : nullptr)
		{
			TooltipDetail->SetText(CardId.IsNone() || !Definition
				? FText::GetEmpty()
				: FText::FromString(GameXXKCardText::DescribeTooltip(
					*Definition,
					Definition->BaseQuality,
					nullptr,
					FGameXXKCardTooltipContext())));
		}
		if (UImage* Portrait = HeroDeckCardPortraits.IsValidIndex(Index) ? HeroDeckCardPortraits[Index].Get() : nullptr)
		{
			if (Definition)
			{
				const FString PortraitPath = ResolveDeckCardPortraitPath(*Definition);
				if (UTexture2D* Texture = LoadTexture(PortraitPath))
				{
					Portrait->SetBrushFromTexture(Texture, true);
					FSlateBrush Brush = Portrait->GetBrush();
					Brush.ImageSize = HeroDeckPortraitSize;
					Portrait->SetBrush(Brush);
				}
			}
			Portrait->SetVisibility(CardId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			Portrait->SetColorAndOpacity(bUnlocked
				? FLinearColor::White
				: FLinearColor(0.30f, 0.29f, 0.27f, 0.62f));
		}
		if (UImage* LockedIcon = HeroDeckLockedIcons.IsValidIndex(Index) ? HeroDeckLockedIcons[Index].Get() : nullptr)
		{
			LockedIcon->SetVisibility(!bUnlocked && !CardId.IsNone() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* UnlockLabel = WidgetTree
			? Cast<UTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("InventoryHeroDeckUnlockText_%02d"), Index)))
			: nullptr)
		{
			const bool bShowCompanionUnlockLevel = CompanionLevel > 0
				&& !bUnlocked
				&& !CardId.IsNone();
			if (bShowCompanionUnlockLevel)
			{
				const int32 UnlockLevel = Index < 10 ? 5 : Index < 14 ? 10 : 15;
				UnlockLabel->SetText(FText::FromString(FString::Printf(TEXT("%d级解锁"), UnlockLevel)));
				UnlockLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				UnlockLabel->SetText(FText::GetEmpty());
				UnlockLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	if (ApplyHeroDeckButton)
	{
		ApplyHeroDeckButton->SetIsEnabled(Subsystem && !bMutationLocked && PendingHeroDeckIds.Num() == RequiredCount);
	}
	if (HeroDeckCountText)
	{
		HeroDeckCountText->SetText(FText::FromString(FString::Printf(TEXT("(%d / %d)"), PendingHeroDeckIds.Num(), RequiredCount)));
	}
}

void UGameXXKInventoryWindowWidget::RefreshBackpackSlots()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const int32 UnlockedBackpackCapacity = Subsystem
		? FGameXXKTalentRules::GetUnlockedBackpackCapacity(Subsystem->GetRuntimeState())
		: 20;
	// Six-slot snapshot drives the replaced-slot comparison rows inside the
	// warehouse tooltips, so bind it before the backpack window fills.
	CharacterBackpackModel.Bind(const_cast<UGameXXKMVPSubsystem*>(Subsystem), ResolveInventoryCharacterId());
	const TArray<FGameXXKCharacterBackpackSlotView> EquippedSlots = CharacterBackpackModel.GetSixSlotSnapshot();
	TArray<FBackpackRuntimeEntry> BackpackEntries;

	if (Subsystem && (ActiveInventoryFilter == EGameXXKInventoryFilter::All || ActiveInventoryFilter == EGameXXKInventoryFilter::Equipment))
	{
		TArray<FName> WarehouseInstanceIds;
		Subsystem->GetEquipmentWarehouseSnapshot(WarehouseInstanceIds);
		for (const FName InstanceId : WarehouseInstanceIds)
		{
			if (bDesktopTrainingEmbeddedMode
				&& Subsystem->GetRuntimeState().DesktopInventory.WarehouseEquipmentInstanceIds.Contains(InstanceId))
			{
				continue;
			}
			const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
				Subsystem->GetRuntimeState().EquipmentCollection,
				InstanceId);
			const FGameXXKEquipmentDefinition* Definition = Instance
				? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
				: nullptr;
			if (!Instance || !Definition)
			{
				continue;
			}

			FBackpackRuntimeEntry Entry;
			Entry.EquipmentInstanceId = InstanceId;
			Entry.IconPath = Definition->IconSoftPath.ToString();
			Entry.DisplayName = Definition->DisplayName;
			Entry.DetailText = BuildEquipmentInstanceDetail(Subsystem, *Instance, *Definition, ResolveInventoryCharacterId());
			BackpackEntries.Add(MoveTemp(Entry));
		}
	}

	TArray<TPair<FName, int32>> LegacyInventoryEntries;
	if (Subsystem)
	{
		for (const TPair<FName, int32>& Entry : Subsystem->GetRuntimeState().Inventory)
		{
			bool bFound = false;
			const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(Entry.Key, bFound);
			if (Entry.Value > 0 && bFound && MatchesInventoryFilter(Def.Kind, ActiveInventoryFilter))
			{
				LegacyInventoryEntries.Add(Entry);
			}
		}
	}
	if (!bBackpackSorted)
	{
		const TArray<FName> KnownItemIds = UGameXXKMVPRules::GetKnownItemIds();
		LegacyInventoryEntries.Sort([&KnownItemIds](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
		{
			const int32 AIndex = KnownItemIds.IndexOfByKey(A.Key);
			const int32 BIndex = KnownItemIds.IndexOfByKey(B.Key);
			if (AIndex != INDEX_NONE || BIndex != INDEX_NONE)
			{
				if (AIndex == INDEX_NONE) { return false; }
				if (BIndex == INDEX_NONE) { return true; }
				return AIndex < BIndex;
			}
			return A.Key.ToString() < B.Key.ToString();
		});
	}

	for (const TPair<FName, int32>& LegacyEntry : LegacyInventoryEntries)
	{
		bool bFound = false;
		const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(LegacyEntry.Key, bFound);
		if (!bFound)
		{
			continue;
		}
		FBackpackRuntimeEntry Entry;
		Entry.ItemId = LegacyEntry.Key;
		Entry.Quantity = LegacyEntry.Value;
		Entry.IconPath = ResolveItemIconTexturePath(LegacyEntry.Key);
		Entry.DisplayName = Definition.DisplayName;
		Entry.DetailText = FText::FromString(ItemStatsText(Definition, Subsystem ? Subsystem->GetItemEnhancementLevel(LegacyEntry.Key) : 0));
		BackpackEntries.Add(MoveTemp(Entry));
	}

	if (bBackpackSorted)
	{
		BackpackEntries.StableSort([](const FBackpackRuntimeEntry& A, const FBackpackRuntimeEntry& B)
		{
			const int32 ARank = A.IsEquipmentInstance() ? 0 : [&A]()
			{
				bool bFound = false;
				const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(A.ItemId, bFound);
				return bFound ? InventorySortRank(Definition.Kind) : 4;
			}();
			const int32 BRank = B.IsEquipmentInstance() ? 0 : [&B]()
			{
				bool bFound = false;
				const FGameXXKItemDef Definition = UGameXXKMVPRules::GetItemDef(B.ItemId, bFound);
				return bFound ? InventorySortRank(Definition.Kind) : 4;
			}();
			if (ARank != BRank) { return ARank < BRank; }
			const FString AName = A.DisplayName.ToString();
			const FString BName = B.DisplayName.ToString();
			if (AName != BName) { return AName < BName; }
			const FName AId = A.IsEquipmentInstance() ? A.EquipmentInstanceId : A.ItemId;
			const FName BId = B.IsEquipmentInstance() ? B.EquipmentInstanceId : B.ItemId;
			return AId.LexicalLess(BId);
		});
	}

	if (bDesktopTrainingEmbeddedMode && Subsystem)
	{
		TMap<FGameXXKDesktopInventoryEntryKey, FBackpackRuntimeEntry> EntriesByKey;
		for (const FBackpackRuntimeEntry& Entry : BackpackEntries)
		{
			const FGameXXKDesktopInventoryEntryKey Key = Entry.IsEquipmentInstance()
				? FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Entry.EquipmentInstanceId)
				: FGameXXKDesktopInventoryRules::MakeItemEntry(Entry.ItemId);
			if (Key.IsValid())
			{
				EntriesByKey.Add(Key, Entry);
			}
		}
		TArray<FBackpackRuntimeEntry> OrderedEntries;
		OrderedEntries.SetNum(BackpackSlotButtons.Num());
		const FGameXXKRuntimeState& RuntimeState = Subsystem->GetRuntimeState();
		for (int32 SlotIndex = 0; SlotIndex < OrderedEntries.Num(); ++SlotIndex)
		{
			const FGameXXKDesktopInventoryEntryKey Key = FGameXXKDesktopInventoryRules::GetEntryAt(
				RuntimeState,
				EGameXXKDesktopItemContainer::Backpack,
				SlotIndex);
			if (!Key.IsValid()
				|| (DesktopTrainingHost
					&& DesktopTrainingHost->ShouldHideDesktopInventoryEntry(
						EGameXXKDesktopItemContainer::Backpack,
						Key)))
			{
				continue;
			}
			if (const FBackpackRuntimeEntry* Existing = EntriesByKey.Find(Key))
			{
				OrderedEntries[SlotIndex] = *Existing;
			}
		}
		BackpackEntries = MoveTemp(OrderedEntries);
	}

	CurrentBackpackSlotItemIds.Reset();
	CurrentBackpackSlotEquipmentInstanceIds.Reset();
	CurrentBackpackSlotQuantities.Reset();
	CurrentBackpackSlotIconPaths.Reset();
	for (int32 SlotIndex = 0; SlotIndex < BackpackSlotButtons.Num(); ++SlotIndex)
	{
		const FBackpackRuntimeEntry* Entry = BackpackEntries.IsValidIndex(SlotIndex) ? &BackpackEntries[SlotIndex] : nullptr;
		CurrentBackpackSlotItemIds.Add(Entry ? Entry->ItemId : NAME_None);
		CurrentBackpackSlotEquipmentInstanceIds.Add(Entry ? Entry->EquipmentInstanceId : NAME_None);
		CurrentBackpackSlotQuantities.Add(Entry ? Entry->Quantity : 0);
		CurrentBackpackSlotIconPaths.Add(Entry ? Entry->IconPath : FString());
	}

	if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		const int32 ReboundSlotIndex = !SelectedEquipmentInstanceId.IsNone()
			? CurrentBackpackSlotEquipmentInstanceIds.IndexOfByKey(SelectedEquipmentInstanceId)
			: CurrentBackpackSlotItemIds.IndexOfByKey(SelectedItemId);
		if (ReboundSlotIndex == INDEX_NONE)
		{
			SelectedSlotSource = EGameXXKInventorySlotSource::None;
			SelectedItemId = NAME_None;
			SelectedEquipmentInstanceId = NAME_None;
			SelectedSlotIndex = INDEX_NONE;
			SelectedEquipmentSlotId = NAME_None;
		}
		else
		{
			SelectedSlotIndex = ReboundSlotIndex;
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < BackpackSlotButtons.Num(); ++SlotIndex)
	{
		const FName ItemId = CurrentBackpackSlotItemIds[SlotIndex];
		const FName EquipmentInstanceId = CurrentBackpackSlotEquipmentInstanceIds[SlotIndex];
		const bool bHasItem = !ItemId.IsNone() || !EquipmentInstanceId.IsNone();
		const int32 Quantity = CurrentBackpackSlotQuantities[SlotIndex];
		const FString& IconPath = CurrentBackpackSlotIconPaths[SlotIndex];
		const FBackpackRuntimeEntry* Entry = BackpackEntries.IsValidIndex(SlotIndex) ? &BackpackEntries[SlotIndex] : nullptr;
		const bool bSlotUnlocked = SlotIndex < UnlockedBackpackCapacity;

		if (UGameXXKInventorySlotButton* SlotButton = BackpackSlotButtons[SlotIndex])
		{
			const bool bSelected = SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack
				&& SelectedSlotIndex == SlotIndex;
			SlotButton->SetStyle(MakeBoxTextureButtonStyle(
				bSelected ? SquareSelectedTexturePath : BackpackSlotTexturePath,
				BackpackSlotSize,
				SlotFrameMargin));
			// Empty physical cells remain valid left-click drop targets while the
			// desktop carry transaction is active.
			SlotButton->SetIsEnabled(bSlotUnlocked && (bHasItem || bDesktopTrainingEmbeddedMode));
			SlotButton->SetBackgroundColor(
				bSlotUnlocked ? FLinearColor::White : FLinearColor(0.28f, 0.28f, 0.26f, 0.72f));
			if (!bSlotUnlocked)
			{
				SlotButton->SetToolTipText(FText::FromString(TEXT("该背包格尚未由永久天赋解锁")));
			}
		}
		if (UBorder* Tooltip = BackpackTooltipFrames.IsValidIndex(SlotIndex) ? BackpackTooltipFrames[SlotIndex].Get() : nullptr)
		{
			// Empty slots must not show a tiny blank tooltip paper on hover.
			Tooltip->SetVisibility(bHasItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* Label = BackpackSlotLabels.IsValidIndex(SlotIndex) ? BackpackSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(bHasItem && Quantity > 1
				? FText::FromString(FString::FromInt(Quantity))
				: FText::GetEmpty());
		}
		if (UImage* Icon = BackpackSlotIcons.IsValidIndex(SlotIndex) ? BackpackSlotIcons[SlotIndex].Get() : nullptr)
		{
			if (UTexture2D* Texture = LoadTexture(IconPath))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = BackpackIconSize;
				Icon->SetBrush(Brush);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* LockedIcon = BackpackLockedIcons.IsValidIndex(SlotIndex)
			? BackpackLockedIcons[SlotIndex].Get()
			: nullptr)
		{
			const FGameXXKDesktopInventoryEntryKey EntryKey = !EquipmentInstanceId.IsNone()
				? FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentInstanceId)
				: FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId);
			const bool bLocked = !bSlotUnlocked || (Subsystem
				&& FGameXXKDesktopInventoryRules::IsEntryLocked(
					Subsystem->GetRuntimeState(),
					EntryKey));
			LockedIcon->SetVisibility(
				bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UTextBlock* TooltipName = BackpackTooltipNameTextBlocks.IsValidIndex(SlotIndex) ? BackpackTooltipNameTextBlocks[SlotIndex].Get() : nullptr)
		{
			TooltipName->SetText(Entry ? Entry->DisplayName : FText::GetEmpty());
		}
		if (UTextBlock* TooltipDetail = BackpackTooltipDetailTextBlocks.IsValidIndex(SlotIndex) ? BackpackTooltipDetailTextBlocks[SlotIndex].Get() : nullptr)
		{
			TooltipDetail->SetText(Entry ? Entry->DetailText : FText::GetEmpty());
		}
		// Comparison rows: only when this warehouse item would replace an
		// occupied slot do we show the red-gain / green-loss stat deltas.
		if (TArray<TObjectPtr<UTextBlock>>* CompareRows = BackpackCompareTextBlocks.IsValidIndex(SlotIndex) ? &BackpackCompareTextBlocks[SlotIndex] : nullptr)
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
				if (Subsystem->GetEquipmentTooltipSnapshot(Entry->EquipmentInstanceId, ResolveInventoryCharacterId(), CompareSnapshot))
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

	// The selected square is the slot's base style, so no opaque overlay may
	// cover the item icon or quantity label.
	if (BackpackSelectionInk)
	{
		BackpackSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameXXKInventoryWindowWidget::HandleBackpackScrolled(float CurrentOffset)
{
	DeferredBackpackScrollOffset = FMath::Max(0.0f, CurrentOffset);
	UpdateBackpackScrollbarThumb();
}

void UGameXXKInventoryWindowWidget::UpdateBackpackScrollbarThumb()
{
	if (!InventoryScrollbarThumb || !BackpackScrollBox)
	{
		return;
	}
	const float MaxOffset = BackpackScrollBox->GetScrollOffsetOfEnd();
	const float Offset = BackpackScrollBox->GetScrollOffset();
	const float ThumbTravel = InventoryScrollbarSize.Y - InventoryScrollbarThumbSize.Y;
	const float Ratio = MaxOffset > 0.0f ? FMath::Clamp(Offset / MaxOffset, 0.0f, 1.0f) : 0.0f;
	const FVector2D ThumbPosition = InventoryScrollbarThumbTop + FVector2D(0.0f, Ratio * ThumbTravel);
	if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(InventoryScrollbarThumb->Slot))
	{
		ThumbSlot->SetPosition(ThumbPosition);
	}
}

void UGameXXKInventoryWindowWidget::RefreshMerchantStockSlots()
{
	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	CurrentMerchantStockSlotItemIds = ShopItems;
	for (int32 SlotIndex = 0; SlotIndex < MerchantStockSlotButtons.Num(); ++SlotIndex)
	{
		const bool bHasItem = ShopItems.IsValidIndex(SlotIndex);
		const FName ItemId = bHasItem ? ShopItems[SlotIndex] : NAME_None;
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		if (UGameXXKInventorySlotButton* SlotButton = MerchantStockSlotButtons[SlotIndex])
		{
			const bool bSelected = SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock
				&& SelectedSlotIndex == SlotIndex;
			SlotButton->SetStyle(MakeBoxTextureButtonStyle(
				bSelected ? SquareSelectedTexturePath : BackpackSlotTexturePath,
				BackpackSlotSize,
				SlotFrameMargin));
			SlotButton->SetIsEnabled(bHasItem && bFound);
		}
		if (UTextBlock* Label = MerchantStockSlotLabels.IsValidIndex(SlotIndex) ? MerchantStockSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(bFound ? FText::FromString(FString::Printf(TEXT("%d金"), Def.BuyPrice)) : FText::GetEmpty());
		}
		if (UImage* Icon = MerchantStockSlotIcons.IsValidIndex(SlotIndex) ? MerchantStockSlotIcons[SlotIndex].Get() : nullptr)
		{
			if (UTexture2D* Texture = LoadTexture(ResolveItemIconTexturePath(ItemId)))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = BackpackIconSize;
				Icon->SetBrush(Brush);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* SelectedOverlay = MerchantStockSelectedOverlays.IsValidIndex(SlotIndex) ? MerchantStockSelectedOverlays[SlotIndex].Get() : nullptr)
		{
			SelectedOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshEquipmentSlots()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	CharacterBackpackModel.Bind(Subsystem, ResolveInventoryCharacterId());
	CurrentEquipmentSlotItemIds.Reset();
	const TArray<FGameXXKCharacterBackpackSlotView> SlotViews = CharacterBackpackModel.GetSixSlotSnapshot();
	const bool bAllowLegacyHeroFallback =
		ResolveInventoryCharacterId() == FGameXXKEquipmentRules::HeroCharacterId();
	const FName SlotIds[] = {WeaponSlotId, HeadSlotId, ArmorSlotId, BeltSlotId, ShoesSlotId, AccessorySlotId};
	const FText EmptyLabels[] = {
		NSLOCTEXT("GameXXKInventoryWindow", "WeaponEmpty", "武器"),
		NSLOCTEXT("GameXXKInventoryWindow", "HeadEmpty", "头部"),
		NSLOCTEXT("GameXXKInventoryWindow", "ArmorEmpty", "衣甲"),
		NSLOCTEXT("GameXXKInventoryWindow", "BeltEmpty", "腰带"),
		NSLOCTEXT("GameXXKInventoryWindow", "ShoesEmpty", "鞋"),
		NSLOCTEXT("GameXXKInventoryWindow", "AccessoryEmpty", "饰品"),
	};
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlotButtons.Num(); ++SlotIndex)
	{
		const FName InstanceId = SlotViews.IsValidIndex(SlotIndex) ? SlotViews[SlotIndex].EquippedInstanceId : NAME_None;
		const FGameXXKEquipmentInstance* Instance = Subsystem && !InstanceId.IsNone()
			? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, InstanceId)
			: nullptr;
		const FGameXXKEquipmentDefinition* EquipmentDefinition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;

		// Keep pre-instance save projections usable while the final UI migrates to the six-slot model.
		const FName LegacyItemId = bAllowLegacyHeroFallback
			&& InstanceId.IsNone()
			&& UE_ARRAY_COUNT(SlotIds) > SlotIndex
			? GetEquippedItemForSlotForTest(SlotIds[SlotIndex])
			: NAME_None;
		bool bLegacyFound = false;
		const FGameXXKItemDef LegacyDefinition = UGameXXKMVPRules::GetItemDef(LegacyItemId, bLegacyFound);
		CurrentEquipmentSlotItemIds.Add(!InstanceId.IsNone() ? InstanceId : LegacyItemId);
		if (UGameXXKInventorySlotButton* SlotButton = EquipmentSlotButtons[SlotIndex])
		{
			const bool bSelected = SelectedSlotSource == EGameXXKInventorySlotSource::Equipment
				&& UE_ARRAY_COUNT(SlotIds) > SlotIndex
				&& SelectedEquipmentSlotId == SlotIds[SlotIndex];
			SlotButton->SetStyle(MakeBoxTextureButtonStyle(
				bSelected ? SquareSelectedTexturePath : EquipmentSlotTexturePath,
				EquipmentSlotSize,
				SlotFrameMargin));
		}
		// No name label once an item is equipped; empty slots show the part name.
		if (UTextBlock* Label = EquipmentSlotLabels.IsValidIndex(SlotIndex) ? EquipmentSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(!InstanceId.IsNone()
				? FText::GetEmpty()
				: bLegacyFound ? LegacyDefinition.DisplayName : EmptyLabels[SlotIndex]);
		}
		if (UImage* Icon = EquipmentSlotIcons.IsValidIndex(SlotIndex) ? EquipmentSlotIcons[SlotIndex].Get() : nullptr)
		{
			const FString IconPath = EquipmentDefinition
				? EquipmentDefinition->IconSoftPath.ToString()
				: ResolveItemIconTexturePath(LegacyItemId);
			if (UTexture2D* Texture = LoadTexture(IconPath))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = FVector2D(72.0f, 72.0f);
				Icon->SetBrush(Brush);
				Icon->SetRenderOpacity(1.0f);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetBrush(FSlateBrush());
				Icon->SetRenderOpacity(0.0f);
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* LockedIcon = EquipmentLockedIcons.IsValidIndex(SlotIndex)
			? EquipmentLockedIcons[SlotIndex].Get()
			: nullptr)
		{
			const bool bLocked = Subsystem
				&& FGameXXKDesktopInventoryRules::IsEntryLocked(
					Subsystem->GetRuntimeState(),
					FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
			LockedIcon->SetVisibility(
				bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UBorder* TooltipFrame = EquipmentTooltipFrames.IsValidIndex(SlotIndex) ? EquipmentTooltipFrames[SlotIndex].Get() : nullptr)
		{
			const bool bHasItem = Instance != nullptr;
			TooltipFrame->SetVisibility(bHasItem ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			if (bHasItem && EquipmentDefinition)
			{
				if (UTextBlock* TooltipName = EquipmentTooltipNameTextBlocks.IsValidIndex(SlotIndex) ? EquipmentTooltipNameTextBlocks[SlotIndex].Get() : nullptr)
				{
					TooltipName->SetText(EquipmentDefinition->DisplayName);
				}
				if (UTextBlock* TooltipDetail = EquipmentTooltipDetailTextBlocks.IsValidIndex(SlotIndex) ? EquipmentTooltipDetailTextBlocks[SlotIndex].Get() : nullptr)
				{
					TooltipDetail->SetText(BuildEquipmentInstanceDetail(Subsystem, *Instance, *EquipmentDefinition, ResolveInventoryCharacterId()));
				}
			}
		}
	}

	// The selected square is the equipment slot's base style.
	if (EquipmentSelectionInk)
	{
		EquipmentSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameXXKInventoryWindowWidget::RefreshDetailPanel()
{
	CurrentPrimaryActionText = FText::GetEmpty();
	CurrentSecondaryActionText = FText::GetEmpty();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();

	if (!SelectedEquipmentInstanceId.IsNone() && Subsystem)
	{
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
			Subsystem->GetRuntimeState().EquipmentCollection,
			SelectedEquipmentInstanceId);
		const FGameXXKEquipmentDefinition* Definition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;
		if (Instance && Definition)
		{
			if (SelectedNameTextBlock)
			{
				SelectedNameTextBlock->SetText(Definition->DisplayName);
			}
			if (SelectedDetailTextBlock)
			{
				SelectedDetailTextBlock->SetText(BuildEquipmentInstanceDetail(Subsystem, *Instance, *Definition, ResolveInventoryCharacterId()));
			}
			CurrentPrimaryActionText = SelectedSlotSource == EGameXXKInventorySlotSource::Equipment
				? NSLOCTEXT("GameXXKInventoryWindow", "UnequipInstanceAction", "卸下")
				: NSLOCTEXT("GameXXKInventoryWindow", "EquipInstanceAction", "装备");
			if (PrimaryActionButton && PrimaryActionTextBlock)
			{
				PrimaryActionTextBlock->SetText(CurrentPrimaryActionText);
				PrimaryActionButton->SetVisibility(ESlateVisibility::Visible);
			}
			if (SecondaryActionButton)
			{
				SecondaryActionButton->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (EnhanceButton)
			{
				EnhanceButton->SetVisibility(ESlateVisibility::Collapsed);
			}
			return;
		}
	}

	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		if (SelectedNameTextBlock)
		{
			SelectedNameTextBlock->SetText(NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionTitle", "选择物品"));
		}
		if (SelectedDetailTextBlock)
		{
			SelectedDetailTextBlock->SetText(NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionDetail", "从背包、商店或装备槽中选择。"));
		}
		if (PrimaryActionButton)
		{
			PrimaryActionButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (SecondaryActionButton)
		{
			SecondaryActionButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (EnhanceButton)
		{
			EnhanceButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bIsEquipment = Def.Kind == EGameXXKItemKind::Weapon
		|| Def.Kind == EGameXXKItemKind::Armor
		|| Def.Kind == EGameXXKItemKind::Accessory;
	const int32 EnhancementLevel = Subsystem ? Subsystem->GetItemEnhancementLevel(SelectedItemId) : 0;

	if (SelectedNameTextBlock)
	{
		SelectedNameTextBlock->SetText(Def.DisplayName);
	}
	if (SelectedDetailTextBlock)
	{
		SelectedDetailTextBlock->SetText(FText::FromString(ItemStatsText(Def, EnhancementLevel)));
	}

	if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		if (Def.Kind == EGameXXKItemKind::Consumable)
		{
			CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "UseAction", "使用");
		}
		else if (bIsEquipment)
		{
			CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "EquipAction", "装备");
		}
		if (WindowMode == EGameXXKInventoryWindowMode::MerchantTrade && Subsystem && Subsystem->CanSellItem(SelectedItemId))
		{
			CurrentSecondaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "SellAction", "出售");
		}
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock)
	{
		CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "BuyAction", "购买");
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
	{
		CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "UnequipAction", "卸下");
	}

	if (PrimaryActionButton && PrimaryActionTextBlock)
	{
		PrimaryActionTextBlock->SetText(CurrentPrimaryActionText);
		PrimaryActionButton->SetVisibility(CurrentPrimaryActionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (SecondaryActionButton && SecondaryActionTextBlock)
	{
		SecondaryActionTextBlock->SetText(CurrentSecondaryActionText);
		SecondaryActionButton->SetVisibility(CurrentSecondaryActionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (EnhanceButton)
	{
		const bool bCanEnhanceSelection = Subsystem
			&& bIsEquipment
			&& (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack || SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
			&& Subsystem->GetItemCount(SelectedItemId) > 0;
		EnhanceButton->SetVisibility(bCanEnhanceSelection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EnhanceButton->SetIsEnabled(bCanEnhanceSelection && Subsystem->CanEnhanceItem(SelectedItemId));
		if (EnhanceActionTextBlock)
		{
			EnhanceActionTextBlock->SetText(NSLOCTEXT("GameXXKInventoryWindow", "EnhanceAction", "强化"));
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshConfirmationDialog()
{
	if (!ConfirmationDialogFrame)
	{
		return;
	}
	const bool bHasPendingItem = !PendingConfirmationItemId.IsNone()
		|| !PendingConfirmationEquipmentInstanceId.IsNone();
	const bool bVisible = PendingConfirmationAction != EConfirmationAction::None && bHasPendingItem;
	ConfirmationDialogFrame->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}

	FText ItemName;
	if (!PendingConfirmationEquipmentInstanceId.IsNone())
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		const FGameXXKEquipmentInstance* Instance = Subsystem
			? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, PendingConfirmationEquipmentInstanceId)
			: nullptr;
		const FGameXXKEquipmentDefinition* EquipmentDefinition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;
		ItemName = EquipmentDefinition
			? EquipmentDefinition->DisplayName
			: FText::FromName(PendingConfirmationEquipmentInstanceId);
	}
	else
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(PendingConfirmationItemId, bFound);
		ItemName = bFound ? Def.DisplayName : FText::FromName(PendingConfirmationItemId);
	}
	if (ConfirmationPromptTextBlock)
	{
		switch (PendingConfirmationAction)
		{
		case EConfirmationAction::Buy:
			ConfirmationPromptTextBlock->SetText(FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "BuyConfirmPrompt", "购买 {0}？"), ItemName));
			break;
		case EConfirmationAction::Sell:
			ConfirmationPromptTextBlock->SetText(FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "SellConfirmPrompt", "卖出 {0}？"), ItemName));
			break;
		case EConfirmationAction::Decompose:
			ConfirmationPromptTextBlock->SetText(FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "DecomposeConfirmPrompt", "分解 {0}？"), ItemName));
			break;
		case EConfirmationAction::Enhance:
			ConfirmationPromptTextBlock->SetText(FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "EnhanceConfirmPrompt", "强化 {0}？"), ItemName));
			break;
		default:
			ConfirmationPromptTextBlock->SetText(FText::GetEmpty());
			break;
		}
	}
	if (ConfirmationSummaryTextBlock)
	{
		if (PendingConfirmationAction == EConfirmationAction::Decompose)
		{
			ConfirmationSummaryTextBlock->SetText(
				NSLOCTEXT("GameXXKInventoryWindow", "DecomposeRewardSummary", "分解奖励：金币 10、强化石 x1、洗炼砂 x1"));
		}
		else if (PendingConfirmationAction == EConfirmationAction::Enhance)
		{
			const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
			const int32 CurrentLevel = Subsystem ? Subsystem->GetItemEnhancementLevel(PendingConfirmationItemId) : 0;
			ConfirmationSummaryTextBlock->SetText(FText::FromString(FString::Printf(TEXT("消耗 强化材料 1    当前 +%d / +%d"), CurrentLevel, UGameXXKMVPRules::GetMaxItemEnhancementLevel())));
		}
		else
		{
			ConfirmationSummaryTextBlock->SetText(FText::FromString(FString::Printf(TEXT("数量 x%d    金 %d"), FMath::Max(1, PendingConfirmationQuantity), PendingConfirmationPrice)));
		}
	}
}

bool UGameXXKInventoryWindowWidget::OpenInventoryWindow(EGameXXKInventoryWindowMode InMode)
{
	if (InMode == EGameXXKInventoryWindowMode::None)
	{
		return false;
	}
	WindowMode = InMode;
	PendingConfirmationAction = EConfirmationAction::None;
	PendingConfirmationItemId = NAME_None;
	PendingConfirmationQuantity = 0;
	PendingConfirmationPrice = 0;
	if (InMode == EGameXXKInventoryWindowMode::FreeInventory)
	{
		ActiveCharacterTab = EGameXXKCharacterBackpackTab::Equipment;
		PendingHeroDeckIds.Reset();
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
		{
			Subsystem->PrepareCompanionRosterForTown();
		}
	}
	BuildProgrammaticLayout();
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackItem(FName ItemId)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ItemId.IsNone() || UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), ItemId) <= 0)
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::PlayerBackpack;
	SelectedItemId = ItemId;
	SelectedEquipmentInstanceId = NAME_None;
	SelectedSlotIndex = CurrentBackpackSlotItemIds.IndexOfByKey(ItemId);
	SelectedEquipmentSlotId = NAME_None;
	RefreshDetailPanel();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackSlot(int32 SlotIndex)
{
	if (!CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex)
		|| !CurrentBackpackSlotEquipmentInstanceIds.IsValidIndex(SlotIndex)
		|| (CurrentBackpackSlotItemIds[SlotIndex].IsNone() && CurrentBackpackSlotEquipmentInstanceIds[SlotIndex].IsNone()))
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::PlayerBackpack;
	SelectedItemId = CurrentBackpackSlotItemIds[SlotIndex];
	SelectedEquipmentInstanceId = CurrentBackpackSlotEquipmentInstanceIds[SlotIndex];
	SelectedSlotIndex = SlotIndex;
	SelectedEquipmentSlotId = NAME_None;
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectMerchantStockSlot(int32 SlotIndex)
{
	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	if (!ShopItems.IsValidIndex(SlotIndex))
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::MerchantStock;
	SelectedItemId = ShopItems[SlotIndex];
	SelectedEquipmentInstanceId = NAME_None;
	SelectedSlotIndex = SlotIndex;
	SelectedEquipmentSlotId = NAME_None;
	RefreshDetailPanel();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectEquipmentSlot(FName SlotId)
{
	const EGameXXKEquipmentSlot EquipmentSlot = EquipmentSlotFromId(SlotId);
	const FName EquipmentInstanceId = EquipmentSlot != EGameXXKEquipmentSlot::Invalid
		? GetEquippedInstanceForSlotForTest(EquipmentSlot)
		: NAME_None;
	const FName ItemId = GetEquippedItemForSlotForTest(SlotId);
	if (EquipmentInstanceId.IsNone() && ItemId.IsNone())
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::Equipment;
	SelectedItemId = ItemId;
	SelectedEquipmentInstanceId = EquipmentInstanceId;
	SelectedSlotIndex = INDEX_NONE;
	SelectedEquipmentSlotId = SlotId;
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectInventoryFilter(EGameXXKInventoryFilter Filter)
{
	ActiveInventoryFilter = Filter;
	if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		bool bSelectionMatches = false;
		if (!SelectedEquipmentInstanceId.IsNone())
		{
			bSelectionMatches = ActiveInventoryFilter == EGameXXKInventoryFilter::All
				|| ActiveInventoryFilter == EGameXXKInventoryFilter::Equipment;
		}
		else
		{
			bool bFound = false;
			const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
			bSelectionMatches = bFound && MatchesInventoryFilter(Def.Kind, ActiveInventoryFilter);
		}
		if (!bSelectionMatches)
		{
			SelectedSlotSource = EGameXXKInventorySlotSource::None;
			SelectedItemId = NAME_None;
			SelectedEquipmentInstanceId = NAME_None;
			SelectedSlotIndex = INDEX_NONE;
			SelectedEquipmentSlotId = NAME_None;
		}
	}
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::SortInventory()
{
	if (WindowMode == EGameXXKInventoryWindowMode::None)
	{
		return false;
	}
	bBackpackSorted = true;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestBuyForSelectedItem()
{
	if (WindowMode != EGameXXKInventoryWindowMode::MerchantTrade || SelectedSlotSource != EGameXXKInventorySlotSource::MerchantStock || SelectedItemId.IsNone())
	{
		return false;
	}
	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		return false;
	}
	PendingConfirmationAction = EConfirmationAction::Buy;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = Def.BuyPrice;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestSellForSelectedItem()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (WindowMode != EGameXXKInventoryWindowMode::MerchantTrade
		|| SelectedSlotSource != EGameXXKInventorySlotSource::PlayerBackpack
		|| SelectedItemId.IsNone()
		|| !Subsystem
		|| !Subsystem->CanSellItem(SelectedItemId))
	{
		return false;
	}
	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		return false;
	}
	PendingConfirmationAction = EConfirmationAction::Sell;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = Def.SellPrice;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestDecomposeForSelectedItem()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (WindowMode == EGameXXKInventoryWindowMode::None
		|| (SelectedSlotSource != EGameXXKInventorySlotSource::PlayerBackpack && SelectedSlotSource != EGameXXKInventorySlotSource::Equipment)
		|| !Subsystem)
	{
		return false;
	}

	// Equipment instance (modern set gear) uses the equipment dismantle path.
	if (!SelectedEquipmentInstanceId.IsNone())
	{
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
			Subsystem->GetRuntimeState().EquipmentCollection, SelectedEquipmentInstanceId);
		if (!Instance)
		{
			return false;
		}
		PendingConfirmationAction = EConfirmationAction::Decompose;
		PendingConfirmationEquipmentInstanceId = SelectedEquipmentInstanceId;
		PendingConfirmationQuantity = 1;
		PendingConfirmationPrice = 1;
		RefreshProgrammaticLayout();
		return true;
	}

	if (SelectedItemId.IsNone() || !Subsystem->CanDecomposeItem(SelectedItemId))
	{
		return false;
	}

	PendingConfirmationAction = EConfirmationAction::Decompose;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = 1;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestEnhanceForSelectedItem()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (WindowMode == EGameXXKInventoryWindowMode::None
		|| !Subsystem
		|| SelectedItemId.IsNone()
		|| (SelectedSlotSource != EGameXXKInventorySlotSource::PlayerBackpack && SelectedSlotSource != EGameXXKInventorySlotSource::Equipment)
		|| !Subsystem->CanEnhanceItem(SelectedItemId))
	{
		return false;
	}

	PendingConfirmationAction = EConfirmationAction::Enhance;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = 1;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::ConfirmDialog()
{
	if (PendingConfirmationAction == EConfirmationAction::None
		|| (PendingConfirmationItemId.IsNone() && PendingConfirmationEquipmentInstanceId.IsNone()))
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	bool bExecuted = false;
	if (PendingConfirmationAction == EConfirmationAction::Buy)
	{
		bExecuted = Subsystem->BuyItem(PendingConfirmationItemId, FMath::Max(1, PendingConfirmationQuantity));
	}
	else if (PendingConfirmationAction == EConfirmationAction::Sell)
	{
		bExecuted = Subsystem->SellItem(PendingConfirmationItemId, FMath::Max(1, PendingConfirmationQuantity));
	}
	else if (PendingConfirmationAction == EConfirmationAction::Decompose)
	{
		if (!PendingConfirmationEquipmentInstanceId.IsNone())
		{
			FGameXXKEquipmentTransactionResult DismantleResult;
			bExecuted = Subsystem->DismantleEquipmentInstances(
				{PendingConfirmationEquipmentInstanceId}, true, DismantleResult);
		}
		else
		{
			bExecuted = Subsystem->DecomposeItem(PendingConfirmationItemId);
		}
	}
	else if (PendingConfirmationAction == EConfirmationAction::Enhance)
	{
		bExecuted = Subsystem->EnhanceItem(PendingConfirmationItemId);
	}

	if (bExecuted)
	{
		if ((PendingConfirmationAction == EConfirmationAction::Sell || PendingConfirmationAction == EConfirmationAction::Decompose)
			&& Subsystem->GetItemCount(PendingConfirmationItemId) <= 0)
		{
			SelectedSlotSource = EGameXXKInventorySlotSource::None;
			SelectedItemId = NAME_None;
			SelectedEquipmentInstanceId = NAME_None;
			SelectedSlotIndex = INDEX_NONE;
			SelectedEquipmentSlotId = NAME_None;
		}
		CancelDialog();
		RefreshProgrammaticLayout();
		// Refresh the town HUD gold strip after money-changing actions.
		NotifyPlayerFlowStateChanged();
	}
	return bExecuted;
}

bool UGameXXKInventoryWindowWidget::CancelDialog()
{
	PendingConfirmationAction = EConfirmationAction::None;
	PendingConfirmationItemId = NAME_None;
	PendingConfirmationEquipmentInstanceId = NAME_None;
	PendingConfirmationQuantity = 0;
	PendingConfirmationPrice = 0;
	if (ConfirmationDialogFrame)
	{
		ConfirmationDialogFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	return true;
}

void UGameXXKInventoryWindowWidget::HandleCloseClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->CloseInventoryWindow();
		return;
	}
	CloseInventoryWindow();
}

void UGameXXKInventoryWindowWidget::HandlePrimaryActionClicked()
{
	if (SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock)
	{
		RequestBuyForSelectedItem();
		return;
	}
	ExecuteSelectedPrimaryActionForTest();
}

void UGameXXKInventoryWindowWidget::HandleSecondaryActionClicked()
{
	RequestSellForSelectedItem();
}

void UGameXXKInventoryWindowWidget::HandleSortClicked()
{
	SortInventory();
}

void UGameXXKInventoryWindowWidget::HandleDecomposeClicked()
{
	RequestDecomposeForSelectedItem();
}

void UGameXXKInventoryWindowWidget::HandleEnhanceClicked()
{
	RequestEnhanceForSelectedItem();
}

void UGameXXKInventoryWindowWidget::HandleEnhanceMainClicked()
{
	// Directly enhance the currently selected equipment instance.
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || SelectedEquipmentInstanceId.IsNone())
	{
		ShowActionErrorText(FText::FromString(TEXT("请先选中一件装备。")));
		return;
	}
	FGameXXKEquipmentTransactionResult Result;
	if (Subsystem->EnhanceEquipmentInstance(SelectedEquipmentInstanceId, Result))
	{
		ShowActionErrorText(FText::GetEmpty());
		RefreshProgrammaticLayout();
	}
	else
	{
		ShowActionErrorText(Result.Message.IsEmpty() ? NSLOCTEXT("GameXXKInventoryWindow", "EnhanceFailed", "强化失败。") : Result.Message);
	}
}

void UGameXXKInventoryWindowWidget::HandleReforgeMainClicked()
{
	// Directly reforge (wash) the first affix of the selected equipment instance.
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || SelectedEquipmentInstanceId.IsNone())
	{
		ShowActionErrorText(FText::FromString(TEXT("请先选中一件装备。")));
		return;
	}
	FGameXXKEquipmentTransactionResult Result;
	if (Subsystem->BeginEquipmentReforge(SelectedEquipmentInstanceId, 0, Result))
	{
		// Auto-accept the generated candidate for the direct-action flow.
		FGameXXKEquipmentTransactionResult ResolveResult;
		Subsystem->ResolveEquipmentReforge(true, ResolveResult);
		ShowActionErrorText(FText::GetEmpty());
		RefreshProgrammaticLayout();
	}
	else
	{
		ShowActionErrorText(Result.Message.IsEmpty() ? NSLOCTEXT("GameXXKInventoryWindow", "ReforgeFailed", "洗炼失败。") : Result.Message);
	}
}

void UGameXXKInventoryWindowWidget::ShowActionErrorText(const FText& Message)
{
	if (SelectedDetailTextBlock)
	{
		SelectedDetailTextBlock->SetText(Message);
	}
}

void UGameXXKInventoryWindowWidget::HandleConfirmClicked()
{
	ConfirmDialog();
}

void UGameXXKInventoryWindowWidget::HandleCancelClicked()
{
	CancelDialog();
}

void UGameXXKInventoryWindowWidget::HandleApplyHeroDeckClicked()
{
	ApplyHeroDeckForTest();
}
