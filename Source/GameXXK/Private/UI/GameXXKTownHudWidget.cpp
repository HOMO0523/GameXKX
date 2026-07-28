#include "UI/GameXXKTownHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

namespace
{
	const FString HudTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/HUD/"));
	const FString NavTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Nav/"));
	const FString PsdTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/"));
	const FString BackpackTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/"));
	const FString CharacterTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Character/"));
	const FString CompanionTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Companion/"));
	const FString HudBackgroundTexturePath(PsdTextureRoot + TEXT("Backgrounds/T_TownPsd_Background_Hud.T_TownPsd_Background_Hud"));
	const FString HudHeroTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudProfile.T_TownPsd_HudProfile"));
	const FString ProfileTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudAvatar.T_TownPsd_HudAvatar"));
	const FString HealthBarFrameTexturePath(HudTextureRoot + TEXT("T_TownHUD_HealthBarFrame.T_TownHUD_HealthBarFrame"));
	const FString HealthBarFillTexturePath(HudTextureRoot + TEXT("T_TownHUD_HealthBarFill.T_TownHUD_HealthBarFill"));
	const FString ExperienceBarFrameTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudExperienceFrame.T_TownPsd_HudExperienceFrame"));
	const FString ExperienceBarFillTexturePath(HudTextureRoot + TEXT("T_TownHUD_ExperienceBarFill.T_TownHUD_ExperienceBarFill"));
	const FString ResourceCoinTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudCoin.T_TownPsd_HudCoin"));
	const FString ResourceGreenTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudJade.T_TownPsd_HudJade"));
	const FString ResourceIngotTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudIngot.T_TownPsd_HudIngot"));
	const FString ResourcePlusTexturePath(PsdTextureRoot + TEXT("HUD/T_TownPsd_HudPlus.T_TownPsd_HudPlus"));
	const FString NavSidebarTexturePath;
	const FString NavTaskTexturePath(PsdTextureRoot + TEXT("Nav/T_TownPsd_NavTask.T_TownPsd_NavTask"));
	const FString NavInventoryTexturePath(PsdTextureRoot + TEXT("Nav/T_TownPsd_NavBackpack.T_TownPsd_NavBackpack"));
	const FString NavRefineTexturePath(PsdTextureRoot + TEXT("Nav/T_TownPsd_NavRefine.T_TownPsd_NavRefine"));
	const FString NavMapTexturePath(PsdTextureRoot + TEXT("Nav/T_TownPsd_NavMap.T_TownPsd_NavMap"));
	const FString NavFriendsTexturePath(PsdTextureRoot + TEXT("Nav/T_TownPsd_NavCompanion.T_TownPsd_NavCompanion"));
	const FString CompanionRosterActionTexturePath(PsdTextureRoot + TEXT("Controls/T_TownPsd_ButtonPrimary.T_TownPsd_ButtonPrimary"));
	const FString BackpackWindowFrameTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
	const FString BackpackHeaderTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Header.T_TownBackpack_Header"));
	const FString BackpackSlotTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Slot.T_TownBackpack_Slot"));
	const FString CharacterBackgroundTexturePath(PsdTextureRoot + TEXT("Backgrounds/T_TownPsd_Background_Character.T_TownPsd_Background_Character"));
	const FString CharacterAttributeTexturePath(PsdTextureRoot + TEXT("Character/T_TownPsd_CharacterTabOne.T_TownPsd_CharacterTabOne"));
	const FString CharacterHeroDetailTexturePath(PsdTextureRoot + TEXT("Character/T_TownPsd_CharacterHeroDetail.T_TownPsd_CharacterHeroDetail"));
	const FString CompanionAllTexturePath(PsdTextureRoot + TEXT("Companion/T_TownPsd_CompanionTabOne.T_TownPsd_CompanionTabOne"));
	const FString CompanionHeroTexturePath(PsdTextureRoot + TEXT("Companion/T_TownPsd_CompanionTabTwo.T_TownPsd_CompanionTabTwo"));
	const FString CompanionSpiritTexturePath(PsdTextureRoot + TEXT("Companion/T_TownPsd_CompanionTabThree.T_TownPsd_CompanionTabThree"));
	const FString CompanionMonsterTexturePath(PsdTextureRoot + TEXT("Companion/T_TownPsd_CompanionTabFour.T_TownPsd_CompanionTabFour"));
	const FString CompanionBeastTexturePath(PsdTextureRoot + TEXT("Companion/T_TownPsd_CompanionTabFive.T_TownPsd_CompanionTabFive"));
	const FString PartyDeckCardFrameTexturePath(TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057.T_CardFrame_PSD057"));
	const FMargin BackpackWindowFrameMargin(12.0f / 368.0f, 12.0f / 304.0f, 12.0f / 368.0f, 12.0f / 304.0f);
	const FMargin BackpackSlotFrameMargin(5.0f / 61.0f, 5.0f / 56.0f, 5.0f / 61.0f, 5.0f / 56.0f);
	const FMargin BackpackActionFrameMargin(6.0f / 73.0f, 5.0f / 31.0f, 6.0f / 73.0f, 5.0f / 31.0f);

	struct FTaskNpcCodexPresentation
	{
		FName NpcId;
		const TCHAR* DisplayName;
		const TCHAR* SupportRole;
		const TCHAR* PassiveLabel;
		const TCHAR* PortraitResourcePath;
	};

	const FTaskNpcCodexPresentation TaskNpcCodexPresentations[] = {
		{TEXT("Npc.TusiChief"), TEXT("土司首领"), TEXT("统御·护阵"), TEXT("寨卫"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief")},
		{TEXT("Npc.SongJinBao"), TEXT("宋金宝"), TEXT("谋略·情报"), TEXT("人情面"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_SongJinBao.T_CardPortrait_Npc_SongJinBao")},
		{TEXT("Npc.YueBai"), TEXT("月白"), TEXT("术法·灼印"), TEXT("残卷先知"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_YueBai.T_CardPortrait_Npc_YueBai")},
		{TEXT("Npc.ZhouGuangZu"), TEXT("周光祖"), TEXT("医术·地志"), TEXT("草木札记"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_ZhouGuangZu.T_CardPortrait_Npc_ZhouGuangZu")},
		{TEXT("Npc.JinGui"), TEXT("金贵"), TEXT("市井·策应"), TEXT("市井门路"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_JinGui.T_CardPortrait_Npc_JinGui")},
		{TEXT("Npc.QiongMeiEr"), TEXT("琼么儿"), TEXT("山野·灵引"), TEXT("苗岭引路"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_QiongMeiEr.T_CardPortrait_Npc_QiongMeiEr")}
	};

	const FTaskNpcCodexPresentation* FindTaskNpcCodexPresentation(const FName NpcId)
	{
		for (const FTaskNpcCodexPresentation& Presentation : TaskNpcCodexPresentations)
		{
			if (Presentation.NpcId == NpcId)
			{
				return &Presentation;
			}
		}
		return nullptr;
	}

	TArray<FName> GetTaskNpcDefaultRouteLoadout(const FName NpcId)
	{
		TArray<FName> Loadout;
		const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcId);
		if (!Definition || Definition->FixedCardIds.Num() < 3)
		{
			return Loadout;
		}
		Loadout.Append(Definition->FixedCardIds.GetData(), 3);
		return Loadout;
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

	FSlateBrush MakeBoxTextureBrush(const FString& Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FSlateBrush Brush = MakeTextureBrush(Path, ImageSize);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Path, ImageSize));
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		return Style;
	}

	FButtonStyle MakeBoxTextureButtonStyle(const FString& Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FSlateBrush Brush = MakeBoxTextureBrush(Path, ImageSize, Margin);
		FButtonStyle Style;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	FProgressBarStyle MakeProgressStyle(const FString& FramePath, const FString& FillPath, const FVector2D& ImageSize)
	{
		FProgressBarStyle Style;
		Style.SetBackgroundImage(MakeTextureBrush(FramePath, ImageSize));
		Style.SetFillImage(MakeTextureBrush(FillPath, ImageSize));
		return Style;
	}

	void AddCanvasChild(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment = FVector2D::ZeroVector, const FAnchors& Anchors = FAnchors(0.0f, 0.0f))
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

	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize, const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f))
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UImage* MakeImage(UWidgetTree* WidgetTree, const FString& TexturePath, const FVector2D& Size)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(MakeTextureBrush(TexturePath, Size));
		return Image;
	}

	FText GetCodexCategoryLabel(EGameXXKCodexCategory Category)
	{
		switch (Category)
		{
		case EGameXXKCodexCategory::All:
			return FText::FromString(TEXT("全部"));
		case EGameXXKCodexCategory::Hero:
			return FText::FromString(TEXT("侠客"));
		case EGameXXKCodexCategory::Spirit:
			return FText::FromString(TEXT("仙灵"));
		case EGameXXKCodexCategory::Monster:
			return FText::FromString(TEXT("妖怪"));
		case EGameXXKCodexCategory::Beast:
			return FText::FromString(TEXT("珍兽"));
		default:
			return FText::GetEmpty();
		}
	}

	const FString& GetCodexFilterTexturePath(EGameXXKCodexCategory Category)
	{
		switch (Category)
		{
		case EGameXXKCodexCategory::All:
			return CompanionAllTexturePath;
		case EGameXXKCodexCategory::Hero:
			return CompanionHeroTexturePath;
		case EGameXXKCodexCategory::Spirit:
			return CompanionSpiritTexturePath;
		case EGameXXKCodexCategory::Monster:
			return CompanionMonsterTexturePath;
		case EGameXXKCodexCategory::Beast:
			return CompanionBeastTexturePath;
		default:
			return CompanionAllTexturePath;
		}
	}
}

void UGameXXKCompanionCodexFilterButton::Configure(UGameXXKTownHudWidget* InOwner, EGameXXKCodexCategory InCategory)
{
	Owner = InOwner;
	Category = InCategory;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKCompanionCodexFilterButton::HandleClicked);
}

void UGameXXKCompanionCodexFilterButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredCodexFilterClicked(Category);
	}
}

void UGameXXKCompanionCodexCardButton::Configure(UGameXXKTownHudWidget* InOwner, FName InEntryId)
{
	Owner = InOwner;
	EntryId = InEntryId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKCompanionCodexCardButton::HandleClicked);
}

void UGameXXKCompanionCodexCardButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredCodexCardClicked(EntryId);
	}
}

TSharedRef<SWidget> UGameXXKTownHudWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKTownHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKTownHudWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bInTown = State && State->Screen == EGameXXKScreen::Town;
	if (!bInTown)
	{
		CloseCompanionCodex();
	}
	SetVisibility(bInTown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetIsEnabled(bInTown);
	if (!State)
	{
		return;
	}

	const int32 Power = State->PlayerAttack + State->PlayerDefense + State->PlayerSpeed;
	int32 InventoryStackCount = 0;
	for (const TPair<FName, int32>& Entry : State->Inventory)
	{
		InventoryStackCount += FMath::Max(0, Entry.Value);
	}
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("小侠客  Lv.%d"), State->PlayerLevel)));
	}
	if (ExperienceText)
	{
		ExperienceText->SetText(FText::FromString(FString::Printf(TEXT("经验 %d / %d"), State->PlayerXP, FMath::Max(1, State->PlayerLevel * 100))));
	}
	if (HealthBar)
	{
		HealthBar->SetPercent(FMath::Clamp(
			static_cast<float>(State->PlayerHP) / static_cast<float>(FMath::Max(1, State->PlayerMaxHP)),
			0.0f,
			1.0f));
	}
	if (ExperienceBar)
	{
		ExperienceBar->SetPercent(FMath::Clamp(
			static_cast<float>(State->PlayerXP) / static_cast<float>(FMath::Max(1, State->PlayerLevel * 100)),
			0.0f,
			1.0f));
	}
	if (PowerText)
	{
		PowerText->SetText(FText::FromString(FString::Printf(TEXT("战力 %d"), Power)));
	}
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::FromInt(State->PlayerGold)));
	}
	if (EnhancementMaterialText)
	{
		EnhancementMaterialText->SetText(FText::AsNumber(FMath::Max(0, State->EnhancementMaterial)));
	}
	if (MaterialText)
	{
		MaterialText->SetText(FText::AsNumber(InventoryStackCount));
	}
	RefreshPanels();
	RefreshCompanionCodex();
}

void UGameXXKTownHudWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("TownHudWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownHudRoot"));
	WidgetTree->RootWidget = RootCanvas;

	if (UImage* HudBackground = MakeImage(WidgetTree, HudBackgroundTexturePath, FVector2D(1024.0f, 430.0f)))
	{
		AddCanvasChild(RootCanvas, HudBackground, FVector2D::ZeroVector, FVector2D(1024.0f, 430.0f));
	}
	if (UImage* HudHero = MakeImage(WidgetTree, HudHeroTexturePath, FVector2D(176.0f, 389.0f)))
	{
		AddCanvasChild(RootCanvas, HudHero, FVector2D(304.0f, 39.0f), FVector2D(176.0f, 389.0f));
	}
	if (UImage* Portrait = MakeImage(WidgetTree, ProfileTexturePath, FVector2D(75.0f, 75.0f)))
	{
		AddCanvasChild(RootCanvas, Portrait, FVector2D(17.0f, 11.0f), FVector2D(75.0f, 75.0f));
	}
	LevelText = MakeText(WidgetTree, FText::GetEmpty(), 22);
	ExperienceText = MakeText(WidgetTree, FText::GetEmpty(), 17, FLinearColor(0.24f, 0.23f, 0.21f, 1.0f));
	PowerText = MakeText(WidgetTree, FText::GetEmpty(), 19, FLinearColor(0.52f, 0.12f, 0.06f, 1.0f));
	AddCanvasChild(RootCanvas, LevelText, FVector2D(185.0f, 38.0f), FVector2D(220.0f, 35.0f));
	AddCanvasChild(RootCanvas, ExperienceText, FVector2D(185.0f, 76.0f), FVector2D(180.0f, 24.0f));
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TownHudHealthBar"));
	HealthBar->SetWidgetStyle(MakeProgressStyle(HealthBarFrameTexturePath, HealthBarFillTexturePath, FVector2D(110.0f, 10.0f)));
	HealthBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	AddCanvasChild(RootCanvas, HealthBar, FVector2D(185.0f, 105.0f), FVector2D(110.0f, 10.0f));
	ExperienceBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TownHudExperienceBar"));
	ExperienceBar->SetWidgetStyle(MakeProgressStyle(ExperienceBarFrameTexturePath, ExperienceBarFillTexturePath, FVector2D(110.0f, 10.0f)));
	ExperienceBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	AddCanvasChild(RootCanvas, ExperienceBar, FVector2D(185.0f, 120.0f), FVector2D(110.0f, 10.0f));
	AddCanvasChild(RootCanvas, PowerText, FVector2D(185.0f, 140.0f), FVector2D(190.0f, 30.0f));

	if (!NavSidebarTexturePath.IsEmpty())
	{
		if (UImage* Sidebar = MakeImage(WidgetTree, NavSidebarTexturePath, FVector2D(78.0f, 324.0f)))
		{
			AddCanvasChild(RootCanvas, Sidebar, FVector2D(22.0f, 104.0f), FVector2D(78.0f, 324.0f));
		}
	}
	auto MakeNavButton = [this](const FName Name, const FString& TexturePath, float Y) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTextureButtonStyle(TexturePath, FVector2D(56.0f, 59.0f)));
		AddCanvasChild(RootCanvas, Button, FVector2D(17.0f, Y), FVector2D(56.0f, 59.0f));
		return Button;
	};
	TaskButton = MakeNavButton(TEXT("TownHudTask"), NavTaskTexturePath, 111.0f);
	InventoryButton = MakeNavButton(TEXT("TownHudInventory"), NavInventoryTexturePath, 178.0f);
	CharacterButton = MakeNavButton(TEXT("TownHudCharacter"), NavRefineTexturePath, 245.0f);
	MapButton = MakeNavButton(TEXT("TownHudMap"), NavMapTexturePath, 313.0f);
	CompanionButton = MakeNavButton(TEXT("TownHudCompanion"), NavFriendsTexturePath, 381.0f);
	if (TaskButton)
	{
		TaskButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleTaskClicked);
	}
	if (InventoryButton)
	{
		InventoryButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleInventoryClicked);
	}
	if (CharacterButton)
	{
		CharacterButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCharacterClicked);
	}
	if (MapButton)
	{
		MapButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleMapClicked);
	}
	if (CompanionButton)
	{
		CompanionButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCompanionClicked);
	}
	CompanionRosterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TownHudCompanionRoster"));
	CompanionRosterButton->SetStyle(MakeTextureButtonStyle(CompanionRosterActionTexturePath, FVector2D(126.0f, 42.0f)));
	if (UTextBlock* RosterLabel = MakeText(WidgetTree, NSLOCTEXT("GameXXKTownHud", "CompanionRoster", "伙伴背包"), 15))
	{
		RosterLabel->SetJustification(ETextJustify::Center);
		CompanionRosterButton->AddChild(RosterLabel);
	}
	CompanionRosterButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCompanionRosterClicked);
	AddCanvasChild(RootCanvas, CompanionRosterButton, FVector2D(108.0f, 481.0f), FVector2D(126.0f, 42.0f));

	auto MakeResource = [this](const FString& TexturePath, const FVector2D& IconSize, float X) -> UTextBlock*
	{
		if (UImage* Icon = MakeImage(WidgetTree, TexturePath, IconSize))
		{
			AddCanvasChild(RootCanvas, Icon, FVector2D(X, 22.0f), IconSize, FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
		}
		UTextBlock* OutText = MakeText(WidgetTree, FText::GetEmpty(), 17);
		AddCanvasChild(RootCanvas, OutText, FVector2D(X + IconSize.X + 5.0f, 22.0f), FVector2D(60.0f, 31.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
		return OutText;
	};
	GoldText = MakeResource(ResourceCoinTexturePath, FVector2D(30.6f, 30.0f), -650.0f);
	EnhancementMaterialText = MakeResource(ResourceGreenTexturePath, FVector2D(31.9f, 30.0f), -470.0f);
	MaterialText = MakeResource(ResourceIngotTexturePath, FVector2D(31.5f, 30.0f), -290.0f);
	auto MakeResourcePlusButton = [this](const FName Name, float X) -> UButton*
	{
		const FVector2D PlusSize(21.6f, 24.0f);
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTextureButtonStyle(ResourcePlusTexturePath, PlusSize));
		Button->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleResourcePlusClicked);
		AddCanvasChild(RootCanvas, Button, FVector2D(X, 25.0f), PlusSize, FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
		return Button;
	};
	CoinResourcePlusButton = MakeResourcePlusButton(TEXT("TownHudCoinResourcePlus"), -550.0f);
	EnhancementMaterialPlusButton = MakeResourcePlusButton(TEXT("TownHudEnhancementMaterialPlus"), -370.0f);
	InventoryStackPlusButton = MakeResourcePlusButton(TEXT("TownHudInventoryStackPlus"), -190.0f);

	CharacterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCharacterPanel"));
	CharacterPanel->SetPadding(FMargin(12.0f));
	CharacterPanel->SetBrush(MakeBoxTextureBrush(CharacterBackgroundTexturePath, FVector2D(560.0f, 320.0f), BackpackWindowFrameMargin));
	CharacterPanel->SetBrushColor(FLinearColor::White);
	UCanvasPanel* CharacterCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownHudCharacterCanvas"));
	if (UImage* CharacterHeader = MakeImage(WidgetTree, BackpackHeaderTexturePath, FVector2D(120.0f, 36.0f)))
	{
		AddCanvasChild(CharacterCanvas, CharacterHeader, FVector2D(246.0f, 14.0f), FVector2D(120.0f, 36.0f));
	}
	CharacterHeroDetailPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TownHudCharacterHeroDetailPortrait"));
	CharacterHeroDetailPortrait->SetBrush(MakeTextureBrush(CharacterHeroDetailTexturePath, FVector2D(128.0f, 256.0f)));
	AddCanvasChild(CharacterCanvas, CharacterHeroDetailPortrait, FVector2D(24.0f, 58.0f), FVector2D(128.0f, 256.0f));
	CharacterStatsText = MakeText(WidgetTree, FText::GetEmpty(), 18);
	AddCanvasChild(CharacterCanvas, CharacterStatsText, FVector2D(164.0f, 58.0f), FVector2D(210.0f, 272.0f));
	CharacterPanel->SetContent(CharacterCanvas);
	CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(RootCanvas, CharacterPanel, FVector2D(-28.0f, 130.0f), FVector2D(560.0f, 320.0f), FVector2D(1.0f, 0.0f), FAnchors(1.0f, 0.0f));
	CharacterLabel = MakeImage(WidgetTree, CharacterAttributeTexturePath, FVector2D(105.0f, 55.0f));
	if (CharacterLabel)
	{
		CharacterLabel->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvasChild(RootCanvas, CharacterLabel, FVector2D(-408.0f, 140.0f), FVector2D(105.0f, 55.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
	}

	BuildCompanionCodexOverlay();

	NoticeText = MakeText(WidgetTree, FText::GetEmpty(), 19, FLinearColor(0.24f, 0.18f, 0.10f, 1.0f));
	NoticeText->SetJustification(ETextJustify::Center);
	AddCanvasChild(RootCanvas, NoticeText, FVector2D(0.0f, -92.0f), FVector2D(520.0f, 40.0f), FVector2D(0.5f, 1.0f), FAnchors(0.5f, 1.0f));
}

void UGameXXKTownHudWidget::BuildCompanionCodexOverlay()
{
	if (!WidgetTree || !RootCanvas || CodexOverlay)
	{
		return;
	}

	auto MakeNamedText = [this](const FName Name, const FText& Text, int32 FontSize, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	};

	CodexOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCodexOverlay"));
	CodexOverlay->SetPadding(FMargin(8.0f));
	CodexOverlay->SetBrushColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.82f));
	CodexOverlay->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(RootCanvas, CodexOverlay, FVector2D::ZeroVector, FVector2D(1060.0f, 680.0f), FVector2D(0.5f, 0.5f), FAnchors(0.5f, 0.5f));

	CodexFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCodexFrame"));
	CodexFrame->SetPadding(FMargin(12.0f));
	CodexFrame->SetBrush(MakeBoxTextureBrush(BackpackWindowFrameTexturePath, FVector2D(1044.0f, 664.0f), BackpackWindowFrameMargin));
	CodexFrame->SetBrushColor(FLinearColor::White);
	CodexOverlay->SetContent(CodexFrame);

	UCanvasPanel* CodexCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownHudCodexCanvas"));
	CodexFrame->SetContent(CodexCanvas);

	UBorder* FilterRail = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCodexFilterRail"));
	FilterRail->SetBrush(MakeBoxTextureBrush(BackpackWindowFrameTexturePath, FVector2D(150.0f, 636.0f), BackpackWindowFrameMargin));
	FilterRail->SetBrushColor(FLinearColor::White);
	AddCanvasChild(CodexCanvas, FilterRail, FVector2D(14.0f, 14.0f), FVector2D(150.0f, 636.0f));
	UCanvasPanel* FilterCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownHudCodexFilterCanvas"));
	FilterRail->SetContent(FilterCanvas);
	if (UImage* FilterHeader = MakeImage(WidgetTree, BackpackHeaderTexturePath, FVector2D(120.0f, 36.0f)))
	{
		AddCanvasChild(FilterCanvas, FilterHeader, FVector2D(15.0f, 14.0f), FVector2D(120.0f, 36.0f));
	}
	UTextBlock* FilterTitle = MakeNamedText(TEXT("TownHudCodexTitle"), FText::FromString(TEXT("任务支援图鉴")), 18, FLinearColor(0.18f, 0.10f, 0.05f, 1.0f));
	FilterTitle->SetJustification(ETextJustify::Center);
	AddCanvasChild(FilterCanvas, FilterTitle, FVector2D(15.0f, 19.0f), FVector2D(120.0f, 28.0f));

	const EGameXXKCodexCategory Categories[] = {
		EGameXXKCodexCategory::All,
		EGameXXKCodexCategory::Hero,
		EGameXXKCodexCategory::Spirit,
		EGameXXKCodexCategory::Monster,
		EGameXXKCodexCategory::Beast
	};
	for (int32 CategoryIndex = 0; CategoryIndex < UE_ARRAY_COUNT(Categories); ++CategoryIndex)
	{
		const EGameXXKCodexCategory Category = Categories[CategoryIndex];
		UGameXXKCompanionCodexFilterButton* FilterButton = WidgetTree->ConstructWidget<UGameXXKCompanionCodexFilterButton>(
			UGameXXKCompanionCodexFilterButton::StaticClass(),
			FName(*FString::Printf(TEXT("TownHudCodexFilter_%d"), CategoryIndex)));
		FilterButton->Configure(this, Category);
		FilterButton->SetStyle(MakeTextureButtonStyle(GetCodexFilterTexturePath(Category), FVector2D(122.0f, 54.0f)));
		FilterButton->SetBackgroundColor(FLinearColor::White);
		UTextBlock* FilterLabel = MakeText(WidgetTree, GetCodexCategoryLabel(Category), 17, FLinearColor(0.18f, 0.10f, 0.05f, 1.0f));
		FilterLabel->SetJustification(ETextJustify::Center);
		FilterButton->AddChild(FilterLabel);
		AddCanvasChild(FilterCanvas, FilterButton, FVector2D(14.0f, 78.0f + CategoryIndex * 96.0f), FVector2D(122.0f, 54.0f));
		CodexFilterButtons.Add(FilterButton);
	}

	CodexCollectionText = MakeNamedText(TEXT("TownHudCodexCollection"), FText::GetEmpty(), 20, FLinearColor(0.25f, 0.13f, 0.06f, 1.0f));
	AddCanvasChild(CodexCanvas, CodexCollectionText, FVector2D(186.0f, 24.0f), FVector2D(310.0f, 34.0f));
	UTextBlock* TaskNpcCaption = MakeNamedText(
		TEXT("TownHudTaskNpcCodexCaption"),
		FText::FromString(TEXT("任务支援 6 名 · 本次路线临时加入 · 不可招募")),
		16,
		FLinearColor(0.31f, 0.20f, 0.12f, 1.0f));
	TaskNpcCaption->SetJustification(ETextJustify::Right);
	AddCanvasChild(CodexCanvas, TaskNpcCaption, FVector2D(504.0f, 28.0f), FVector2D(450.0f, 28.0f));

	CodexScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TownHudCodexScroll"));
	CodexScroll->SetOrientation(Orient_Vertical);
	CodexScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	CodexScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	FGameXXKPartyDeckUiStyle::ApplyPaperInkScrollBar(CodexScroll);
	CodexGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TownHudCodexGrid"));
	CodexGrid->SetSlotPadding(FMargin(8.0f, 6.0f));
	CodexScroll->AddChild(CodexGrid);
	AddCanvasChild(CodexCanvas, CodexScroll, FVector2D(182.0f, 72.0f), FVector2D(840.0f, 420.0f));

	CodexEmptyText = MakeNamedText(TEXT("TownHudCodexEmpty"), FText::FromString(TEXT("尚未收录")), 26, FLinearColor(0.37f, 0.26f, 0.15f, 1.0f));
	CodexEmptyText->SetJustification(ETextJustify::Center);
	CodexEmptyText->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(CodexCanvas, CodexEmptyText, FVector2D(182.0f, 244.0f), FVector2D(810.0f, 52.0f));

	CodexDetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCodexDetail"));
	CodexDetailPanel->SetPadding(FMargin(3.0f));
	CodexDetailPanel->SetBrush(MakeBoxTextureBrush(BackpackWindowFrameTexturePath, FVector2D(840.0f, 136.0f), BackpackWindowFrameMargin));
	CodexDetailPanel->SetBrushColor(FLinearColor::White);
	UCanvasPanel* DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TownHudCodexDetailCanvas"));
	CodexDetailPanel->SetContent(DetailCanvas);
	TaskNpcCodexDetailPortraitSlot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudTaskNpcCodexDetailPortraitSlot"));
	TaskNpcCodexDetailPortraitSlot->SetPadding(FMargin(4.0f, 5.0f));
	TaskNpcCodexDetailPortraitSlot->SetBrush(MakeBoxTextureBrush(BackpackSlotTexturePath, FVector2D(88.0f, 118.0f), BackpackSlotFrameMargin));
	TaskNpcCodexDetailPortraitSlot->SetVisibility(ESlateVisibility::Collapsed);
	TaskNpcCodexDetailPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TownHudTaskNpcCodexDetailPortrait"));
	TaskNpcCodexDetailPortrait->SetVisibility(ESlateVisibility::Collapsed);
	TaskNpcCodexDetailPortraitSlot->SetContent(TaskNpcCodexDetailPortrait);
	AddCanvasChild(DetailCanvas, TaskNpcCodexDetailPortraitSlot, FVector2D(4.0f, 6.0f), FVector2D(88.0f, 118.0f));
	CodexDetailText = MakeNamedText(TEXT("TownHudCodexDetailText"), FText::FromString(TEXT("选择任务 NPC 查看本次路线的临时支援与固定三张牌")), 17, FLinearColor(0.20f, 0.11f, 0.05f, 1.0f));
	AddCanvasChild(DetailCanvas, CodexDetailText, FVector2D(94.0f, 6.0f), FVector2D(352.0f, 116.0f));
	for (int32 LoadoutIndex = 0; LoadoutIndex < 3; ++LoadoutIndex)
	{
		UCanvasPanel* LoadoutCardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("TownHudTaskNpcCodexLoadoutCard_%d"), LoadoutIndex));
		LoadoutCardCanvas->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvasChild(DetailCanvas, LoadoutCardCanvas, FVector2D(456.0f + LoadoutIndex * 118.0f, 0.0f), FVector2D(113.0f, 129.0f));
		if (UImage* CardFrame = MakeImage(WidgetTree, PartyDeckCardFrameTexturePath, FVector2D(113.0f, 129.0f)))
		{
			AddCanvasChild(LoadoutCardCanvas, CardFrame, FVector2D::ZeroVector, FVector2D(113.0f, 129.0f));
		}
		UImage* LoadoutPortrait = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("TownHudTaskNpcCodexLoadoutPortrait_%d"), LoadoutIndex));
		LoadoutPortrait->SetVisibility(ESlateVisibility::Collapsed);
		AddCanvasChild(LoadoutCardCanvas, LoadoutPortrait, FVector2D(16.0f, 14.0f), FVector2D(81.0f, 68.0f));
		UBorder* LoadoutInfoStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		LoadoutInfoStrip->SetBrushColor(FLinearColor(0.145f, 0.137f, 0.129f, 1.0f));
		LoadoutInfoStrip->SetPadding(FMargin(2.0f, 1.0f));
		UTextBlock* LoadoutLabel = MakeText(WidgetTree, FText::GetEmpty(), 9, FLinearColor(0.722f, 0.706f, 0.671f, 1.0f));
		LoadoutLabel->SetJustification(ETextJustify::Center);
		LoadoutLabel->SetAutoWrapText(false);
		LoadoutInfoStrip->SetContent(LoadoutLabel);
		AddCanvasChild(LoadoutCardCanvas, LoadoutInfoStrip, FVector2D(12.0f, 87.0f), FVector2D(89.0f, 27.0f));
		TaskNpcCodexLoadoutCards.Add(LoadoutCardCanvas);
		TaskNpcCodexLoadoutPortraits.Add(LoadoutPortrait);
		TaskNpcCodexLoadoutLabels.Add(LoadoutLabel);
	}
	AddCanvasChild(CodexCanvas, CodexDetailPanel, FVector2D(182.0f, 514.0f), FVector2D(840.0f, 136.0f));

	CodexCloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TownHudCodexClose"));
	CodexCloseButton->SetStyle(MakeBoxTextureButtonStyle(CompanionRosterActionTexturePath, FVector2D(64.0f, 32.0f), BackpackActionFrameMargin));
	CodexCloseButton->SetBackgroundColor(FLinearColor::White);
	CodexCloseButton->AddChild(MakeText(WidgetTree, FText::FromString(TEXT("×")), 25, FLinearColor(0.22f, 0.12f, 0.06f, 1.0f)));
	CodexCloseButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCodexCloseClicked);
	AddCanvasChild(CodexCanvas, CodexCloseButton, FVector2D(958.0f, 18.0f), FVector2D(64.0f, 32.0f));

	CompanionUnreadBadge = MakeNamedText(TEXT("CompanionUnreadBadge"), FText::FromString(TEXT("●")), 23, FLinearColor(0.90f, 0.06f, 0.04f, 1.0f));
	CompanionUnreadBadge->SetJustification(ETextJustify::Center);
	CompanionUnreadBadge->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(RootCanvas, CompanionUnreadBadge, FVector2D(78.0f, 466.0f), FVector2D(24.0f, 30.0f));
}

void UGameXXKTownHudWidget::RefreshPanels()
{
	const bool bCharacterOpen = CharacterPanel && CharacterPanel->GetVisibility() != ESlateVisibility::Collapsed;
	if (CharacterLabel)
	{
		CharacterLabel->SetVisibility(bCharacterOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		RefreshCompanionUnreadBadge();
		return;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (CharacterStatsText)
	{
		CharacterStatsText->SetText(FText::FromString(FString::Printf(
			TEXT("角色\n\nLv.%d\nHP %d / %d\nMP %d / %d\n\n攻击 %d   防御 %d   速度 %d\n\n已装备\n%s\n%s\n%s"),
			State.PlayerLevel,
			State.PlayerHP,
			State.PlayerMaxHP,
			State.PlayerMP,
			State.PlayerMaxMP,
			State.PlayerAttack,
			State.PlayerDefense,
			State.PlayerSpeed,
			*State.EquippedWeapon.ToString(),
			*State.EquippedArmor.ToString(),
			*State.EquippedAccessory.ToString())));
	}
	RefreshCompanionUnreadBadge();
}

void UGameXXKTownHudWidget::RefreshCompanionCodex()
{
	if (!CodexOverlay)
	{
		return;
	}
	if (!bCompanionCodexOpen)
	{
		CodexOverlay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		CloseCompanionCodex();
		return;
	}

	CodexOverlay->SetVisibility(ESlateVisibility::Visible);
	const TArray<FGameXXKCodexEntryView> EntryViews = Subsystem->GetCodexEntryViews(ActiveCodexCategory);
	VisibleCodexEntryIds.Reset();
	for (const FGameXXKCodexEntryView& EntryView : EntryViews)
	{
		VisibleCodexEntryIds.Add(EntryView.Id);
	}
	VisibleTaskNpcCodexEntryIds.Reset();
	if (ActiveCodexCategory == EGameXXKCodexCategory::All)
	{
		for (const FTaskNpcCodexPresentation& Presentation : TaskNpcCodexPresentations)
		{
			if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(Presentation.NpcId))
			{
				VisibleTaskNpcCodexEntryIds.Add(Presentation.NpcId);
			}
		}
	}

	if (CodexCollectionText)
	{
		if (EntryViews.IsEmpty())
		{
			CodexCollectionText->SetText(FText::FromString(TEXT("尚未收录")));
		}
		else
		{
			CodexCollectionText->SetText(FText::FromString(FString::Printf(
				TEXT("已收录 %d / %d"),
				Subsystem->GetDiscoveredCodexEntryCount(ActiveCodexCategory),
				Subsystem->GetCodexEntryCount(ActiveCodexCategory))));
		}
	}
	if (CodexEmptyText)
	{
		CodexEmptyText->SetVisibility(EntryViews.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (CodexGrid)
	{
		CodexGrid->ClearChildren();
		int32 GridEntryIndex = 0;
		for (const FName TaskNpcId : VisibleTaskNpcCodexEntryIds)
		{
			const FTaskNpcCodexPresentation* Presentation = FindTaskNpcCodexPresentation(TaskNpcId);
			const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TaskNpcId);
			if (!Presentation || !Definition)
			{
				continue;
			}

			USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
			CardSizeBox->SetWidthOverride(CodexCardSize.X);
			CardSizeBox->SetHeightOverride(CodexCardSize.Y);
			const FString SafeNpcName = TaskNpcId.ToString().Replace(TEXT("."), TEXT("_"));
			UGameXXKCompanionCodexCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKCompanionCodexCardButton>(
				UGameXXKCompanionCodexCardButton::StaticClass(),
				*FString::Printf(TEXT("TownHudTaskNpcCodexCard_%s"), *SafeNpcName));
			CardButton->Configure(this, TaskNpcId);
			CardButton->SetStyle(MakeTextureButtonStyle(PartyDeckCardFrameTexturePath, CodexCardSize));
			CardButton->SetBackgroundColor(FLinearColor::White);

			UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			UTextBlock* NameText = MakeText(WidgetTree, FText::FromString(FString(Presentation->DisplayName)), 11, FLinearColor(0.14f, 0.10f, 0.06f, 1.0f));
			NameText->SetJustification(ETextJustify::Center);
			AddCanvasChild(CardCanvas, NameText, FVector2D(12.0f, 5.0f), FVector2D(89.0f, 17.0f));
			if (UImage* Portrait = MakeImage(WidgetTree, Presentation->PortraitResourcePath, FVector2D(75.0f, 54.0f)))
			{
				AddCanvasChild(CardCanvas, Portrait, FVector2D(19.0f, 23.0f), FVector2D(75.0f, 54.0f));
			}
			UBorder* SupportStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			SupportStrip->SetBrushColor(FLinearColor(0.145f, 0.137f, 0.129f, 1.0f));
			UTextBlock* SupportRoleText = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%s · 临时支援"), Presentation->SupportRole)), 8, FLinearColor(0.722f, 0.706f, 0.671f, 1.0f));
			SupportRoleText->SetJustification(ETextJustify::Center);
			SupportStrip->SetContent(SupportRoleText);
			AddCanvasChild(CardCanvas, SupportStrip, FVector2D(12.0f, 81.0f), FVector2D(89.0f, 21.0f));
			UTextBlock* RouteSupportText = MakeText(WidgetTree, FText::FromString(TEXT("本次路线有效 · 不可招募")), 7, FLinearColor(0.28f, 0.19f, 0.12f, 1.0f));
			RouteSupportText->SetJustification(ETextJustify::Center);
			AddCanvasChild(CardCanvas, RouteSupportText, FVector2D(12.0f, 105.0f), FVector2D(89.0f, 15.0f));

			CardButton->AddChild(CardCanvas);
			CardSizeBox->AddChild(CardButton);
			if (UUniformGridSlot* GridSlot = CodexGrid->AddChildToUniformGrid(CardSizeBox, GridEntryIndex / CodexColumnCount, GridEntryIndex % CodexColumnCount))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Center);
				GridSlot->SetVerticalAlignment(VAlign_Center);
			}
			++GridEntryIndex;
		}
		for (int32 EntryIndex = 0; EntryIndex < EntryViews.Num(); ++EntryIndex)
		{
			const FGameXXKCodexEntryView& EntryView = EntryViews[EntryIndex];
			USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				NAME_None);
			CardSizeBox->SetWidthOverride(CodexCardSize.X);
			CardSizeBox->SetHeightOverride(CodexCardSize.Y);

			const FString SafeEntryName = EntryView.Id.ToString().Replace(TEXT("."), TEXT("_"));
			UGameXXKCompanionCodexCardButton* CardButton = WidgetTree->ConstructWidget<UGameXXKCompanionCodexCardButton>(
				UGameXXKCompanionCodexCardButton::StaticClass(),
				*FString::Printf(TEXT("TownHudGenericCodexCard_%s"), *SafeEntryName));
			CardButton->Configure(this, EntryView.Id);
			CardButton->SetStyle(MakeTextureButtonStyle(PartyDeckCardFrameTexturePath, CodexCardSize));
			const bool bSelected = SelectedCodexEntryId == EntryView.Id;
			CardButton->SetBackgroundColor(!EntryView.bIsDiscovered
				? FLinearColor(0.72f, 0.72f, 0.69f, 1.0f)
				: (bSelected ? FLinearColor(1.0f, 0.88f, 0.62f, 1.0f) : FLinearColor::White));
			UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());

			UTextBlock* CardTitle = MakeText(WidgetTree, EntryView.bIsDiscovered ? EntryView.DisplayName : FText::FromString(TEXT("????")), 11, FLinearColor(0.12f, 0.08f, 0.04f, 1.0f));
			CardTitle->SetJustification(ETextJustify::Center);
			AddCanvasChild(CardCanvas, CardTitle, FVector2D(12.0f, 7.0f), FVector2D(89.0f, 17.0f));
			if (EntryView.bIsDiscovered)
			{
				UTextBlock* CategoryText = MakeText(WidgetTree, GetCodexCategoryLabel(EntryView.Category), 8, FLinearColor(0.29f, 0.17f, 0.08f, 1.0f));
				CategoryText->SetJustification(ETextJustify::Center);
				UTextBlock* DescriptionText = MakeText(WidgetTree, EntryView.Description, 8, FLinearColor(0.17f, 0.11f, 0.06f, 1.0f));
				AddCanvasChild(CardCanvas, CategoryText, FVector2D(12.0f, 27.0f), FVector2D(89.0f, 13.0f));
				AddCanvasChild(CardCanvas, DescriptionText, FVector2D(12.0f, 43.0f), FVector2D(89.0f, 54.0f));
				if (!EntryView.bIsRead)
				{
					UTextBlock* UnreadDot = MakeText(WidgetTree, FText::FromString(TEXT("●")), 11, FLinearColor(0.90f, 0.05f, 0.04f, 1.0f));
					UnreadDot->SetJustification(ETextJustify::Center);
					AddCanvasChild(CardCanvas, UnreadDot, FVector2D(87.0f, 4.0f), FVector2D(14.0f, 14.0f));
				}
			}
			else
			{
				UTextBlock* UndiscoveredText = MakeText(WidgetTree, FText::FromString(TEXT("未遇见")), 11, FLinearColor(0.18f, 0.18f, 0.17f, 1.0f));
				UndiscoveredText->SetJustification(ETextJustify::Center);
				AddCanvasChild(CardCanvas, UndiscoveredText, FVector2D(12.0f, 55.0f), FVector2D(89.0f, 20.0f));
			}

			CardButton->AddChild(CardCanvas);
			CardSizeBox->AddChild(CardButton);
			if (UUniformGridSlot* GridSlot = CodexGrid->AddChildToUniformGrid(CardSizeBox, GridEntryIndex / CodexColumnCount, GridEntryIndex % CodexColumnCount))
			{
				GridSlot->SetHorizontalAlignment(HAlign_Center);
				GridSlot->SetVerticalAlignment(VAlign_Center);
			}
			++GridEntryIndex;
		}
	}

	const FTaskNpcCodexPresentation* SelectedTaskNpcPresentation = VisibleTaskNpcCodexEntryIds.Contains(SelectedTaskNpcCodexId)
		? FindTaskNpcCodexPresentation(SelectedTaskNpcCodexId)
		: nullptr;
	const FGameXXKQuestNpcDefinition* SelectedTaskNpcDefinition = SelectedTaskNpcPresentation
		? FGameXXKCompanionCatalog::FindQuestNpcDefinition(SelectedTaskNpcCodexId)
		: nullptr;
	const bool bHasSelectedTaskNpc = SelectedTaskNpcPresentation && SelectedTaskNpcDefinition;
	if (TaskNpcCodexDetailPortraitSlot)
	{
		TaskNpcCodexDetailPortraitSlot->SetVisibility(bHasSelectedTaskNpc ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TaskNpcCodexDetailPortrait)
	{
		TaskNpcCodexDetailPortrait->SetVisibility(bHasSelectedTaskNpc ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasSelectedTaskNpc)
		{
			TaskNpcCodexDetailPortrait->SetBrush(MakeTextureBrush(SelectedTaskNpcPresentation->PortraitResourcePath, FVector2D(80.0f, 108.0f)));
		}
	}
	if (CodexDetailText)
	{
		if (UCanvasPanelSlot* DetailTextSlot = Cast<UCanvasPanelSlot>(CodexDetailText->Slot))
		{
			DetailTextSlot->SetPosition(bHasSelectedTaskNpc ? FVector2D(94.0f, 6.0f) : FVector2D(8.0f, 6.0f));
			DetailTextSlot->SetSize(bHasSelectedTaskNpc ? FVector2D(352.0f, 116.0f) : FVector2D(816.0f, 116.0f));
		}
		if (bHasSelectedTaskNpc)
		{
			CodexDetailText->SetText(FText::FromString(FString::Printf(
				TEXT("%s  |  支援定位：%s\n临时路线支援 · 不可招募\n基础属性：生命 %d  攻击 %d\n防御 %d  真气 %d  被动：%s"),
				SelectedTaskNpcPresentation->DisplayName,
				SelectedTaskNpcPresentation->SupportRole,
				SelectedTaskNpcDefinition->BaseAttributes.Health,
				SelectedTaskNpcDefinition->BaseAttributes.Attack,
				SelectedTaskNpcDefinition->BaseAttributes.Defense,
				SelectedTaskNpcDefinition->BaseAttributes.Mana,
				SelectedTaskNpcPresentation->PassiveLabel)));
		}
		else
		{
			const FGameXXKCodexEntryView* SelectedEntry = EntryViews.FindByPredicate([this](const FGameXXKCodexEntryView& EntryView)
			{
				return EntryView.Id == SelectedCodexEntryId && EntryView.bIsDiscovered;
			});
			CodexDetailText->SetText(SelectedEntry
				? FText::FromString(FString::Printf(TEXT("%s\n%s\n%s"), *GetCodexCategoryLabel(SelectedEntry->Category).ToString(), *SelectedEntry->DisplayName.ToString(), *SelectedEntry->Description.ToString()))
				: FText::FromString(TEXT("选择任务 NPC 查看本次路线的临时支援与固定三张牌")));
		}
	}
	const TArray<FName> SelectedTaskNpcLoadout = bHasSelectedTaskNpc
		? GetTaskNpcDefaultRouteLoadout(SelectedTaskNpcCodexId)
		: TArray<FName>();
	for (int32 LoadoutIndex = 0; LoadoutIndex < TaskNpcCodexLoadoutCards.Num(); ++LoadoutIndex)
	{
		const bool bShowLoadoutCard = bHasSelectedTaskNpc && SelectedTaskNpcLoadout.IsValidIndex(LoadoutIndex);
		if (TaskNpcCodexLoadoutCards[LoadoutIndex])
		{
			TaskNpcCodexLoadoutCards[LoadoutIndex]->SetVisibility(bShowLoadoutCard ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (!bShowLoadoutCard)
		{
			continue;
		}
		if (TaskNpcCodexLoadoutPortraits.IsValidIndex(LoadoutIndex) && TaskNpcCodexLoadoutPortraits[LoadoutIndex])
		{
			TaskNpcCodexLoadoutPortraits[LoadoutIndex]->SetBrush(MakeTextureBrush(SelectedTaskNpcPresentation->PortraitResourcePath, FVector2D(81.0f, 68.0f)));
			TaskNpcCodexLoadoutPortraits[LoadoutIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (TaskNpcCodexLoadoutLabels.IsValidIndex(LoadoutIndex) && TaskNpcCodexLoadoutLabels[LoadoutIndex])
		{
			const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(SelectedTaskNpcLoadout[LoadoutIndex]);
			TaskNpcCodexLoadoutLabels[LoadoutIndex]->SetText(Card ? Card->DisplayName : FText::FromString(SelectedTaskNpcLoadout[LoadoutIndex].ToString()));
		}
	}
	for (int32 FilterIndex = 0; FilterIndex < CodexFilterButtons.Num(); ++FilterIndex)
	{
		if (CodexFilterButtons[FilterIndex])
		{
			CodexFilterButtons[FilterIndex]->SetBackgroundColor(FilterIndex == static_cast<int32>(ActiveCodexCategory)
				? FLinearColor(1.0f, 0.86f, 0.46f, 1.0f)
				: FLinearColor(0.78f, 0.74f, 0.64f, 1.0f));
		}
	}
	if (CodexScroll)
	{
		CodexScroll->SetScrollOffset(LastCodexScrollOffset);
	}
}

void UGameXXKTownHudWidget::RefreshCompanionUnreadBadge()
{
	if (!CompanionUnreadBadge)
	{
		return;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bShowUnreadBadge = Subsystem
		&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town
		&& Subsystem->HasUnreadCodexEntries();
	CompanionUnreadBadge->SetVisibility(bShowUnreadBadge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

bool UGameXXKTownHudWidget::OpenCompanionCodex()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		CloseCompanionCodex();
		return false;
	}
	if (CharacterPanel)
	{
		CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	bCompanionCodexOpen = true;
	ActiveCodexCategory = EGameXXKCodexCategory::All;
	SelectedCodexEntryId = NAME_None;
	SelectedTaskNpcCodexId = NAME_None;
	RefreshCompanionCodex();
	RefreshPanels();
	return true;
}

bool UGameXXKTownHudWidget::CloseCompanionCodex()
{
	const bool bWasOpen = bCompanionCodexOpen;
	bCompanionCodexOpen = false;
	SelectedCodexEntryId = NAME_None;
	SelectedTaskNpcCodexId = NAME_None;
	VisibleCodexEntryIds.Reset();
	VisibleTaskNpcCodexEntryIds.Reset();
	LastCodexScrollOffset = 0.0f;
	if (CodexScroll)
	{
		CodexScroll->SetScrollOffset(0.0f);
	}
	if (CodexOverlay)
	{
		CodexOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	return bWasOpen;
}

bool UGameXXKTownHudWidget::IsValidCodexCategory(EGameXXKCodexCategory Category) const
{
	switch (Category)
	{
	case EGameXXKCodexCategory::All:
	case EGameXXKCodexCategory::Hero:
	case EGameXXKCodexCategory::Spirit:
	case EGameXXKCodexCategory::Monster:
	case EGameXXKCodexCategory::Beast:
		return true;
	default:
		return false;
	}
}

bool UGameXXKTownHudWidget::IsCompanionCodexOpenForTest() const
{
	return bCompanionCodexOpen && CodexOverlay && CodexOverlay->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKTownHudWidget::OpenCompanionCodexForTest()
{
	return OpenCompanionCodex();
}

bool UGameXXKTownHudWidget::SelectCodexCategoryForTest(EGameXXKCodexCategory Category)
{
	if (!IsValidCodexCategory(Category) || !IsCompanionCodexOpenForTest())
	{
		return false;
	}
	ActiveCodexCategory = Category;
	SelectedCodexEntryId = NAME_None;
	SelectedTaskNpcCodexId = NAME_None;
	if (CodexScroll)
	{
		CodexScroll->SetScrollOffset(0.0f);
	}
	LastCodexScrollOffset = 0.0f;
	RefreshCompanionCodex();
	return true;
}

bool UGameXXKTownHudWidget::SelectCodexEntryForTest(FName EntryId)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsCompanionCodexOpenForTest() || EntryId.IsNone() || !VisibleCodexEntryIds.Contains(EntryId))
	{
		return false;
	}
	const TArray<FGameXXKCodexEntryView> EntryViews = Subsystem->GetCodexEntryViews(ActiveCodexCategory);
	const FGameXXKCodexEntryView* EntryView = EntryViews.FindByPredicate([EntryId](const FGameXXKCodexEntryView& Candidate)
	{
		return Candidate.Id == EntryId;
	});
	if (!EntryView || !EntryView->bIsDiscovered)
	{
		return false;
	}
	SelectedCodexEntryId = EntryId;
	SelectedTaskNpcCodexId = NAME_None;
	Subsystem->MarkCodexEntryRead(EntryId);
	RefreshCompanionCodex();
	RefreshCompanionUnreadBadge();
	return true;
}

bool UGameXXKTownHudWidget::SelectTaskNpcCodexEntryForTest(FName NpcId)
{
	if (!IsCompanionCodexOpenForTest() || NpcId.IsNone() || !VisibleTaskNpcCodexEntryIds.Contains(NpcId))
	{
		return false;
	}
	if (!FindTaskNpcCodexPresentation(NpcId) || !FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcId))
	{
		return false;
	}
	SelectedCodexEntryId = NAME_None;
	SelectedTaskNpcCodexId = NpcId;
	RefreshCompanionCodex();
	return true;
}

EGameXXKCodexCategory UGameXXKTownHudWidget::GetActiveCodexCategoryForTest() const
{
	return ActiveCodexCategory;
}

TArray<FName> UGameXXKTownHudWidget::GetVisibleCodexEntryIdsForTest() const
{
	return VisibleCodexEntryIds;
}

TArray<FName> UGameXXKTownHudWidget::GetTaskNpcCodexEntryIdsForTest() const
{
	return VisibleTaskNpcCodexEntryIds;
}

TArray<FName> UGameXXKTownHudWidget::GetTaskNpcFixedRouteLoadoutForTest(FName NpcId) const
{
	return FindTaskNpcCodexPresentation(NpcId) ? GetTaskNpcDefaultRouteLoadout(NpcId) : TArray<FName>();
}

FString UGameXXKTownHudWidget::GetTaskNpcPortraitResourcePathForTest(FName NpcId) const
{
	const FTaskNpcCodexPresentation* Presentation = FindTaskNpcCodexPresentation(NpcId);
	return Presentation ? FString(Presentation->PortraitResourcePath) : FString();
}

FString UGameXXKTownHudWidget::GetHeroDetailPortraitResourcePathForTest() const
{
	return CharacterHeroDetailTexturePath;
}

FText UGameXXKTownHudWidget::GetTaskNpcCodexDetailForTest() const
{
	return CodexDetailText ? CodexDetailText->GetText() : FText::GetEmpty();
}

int32 UGameXXKTownHudWidget::GetCodexColumnCountForTest() const
{
	return CodexColumnCount;
}

FVector2D UGameXXKTownHudWidget::GetCodexCardSizeForTest() const
{
	return CodexCardSize;
}

bool UGameXXKTownHudWidget::IsCodexEmptyStateVisibleForTest() const
{
	return CodexEmptyText && CodexEmptyText->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKTownHudWidget::HasCompanionUnreadNoticeForTest() const
{
	return CompanionUnreadBadge && CompanionUnreadBadge->GetVisibility() != ESlateVisibility::Collapsed;
}

FText UGameXXKTownHudWidget::GetCodexCollectionSummaryForTest() const
{
	return CodexCollectionText ? CodexCollectionText->GetText() : FText::GetEmpty();
}

float UGameXXKTownHudWidget::GetCodexScrollOffsetForTest() const
{
	return LastCodexScrollOffset;
}

bool UGameXXKTownHudWidget::SetCodexScrollOffsetForTest(float Offset)
{
	if (!IsCompanionCodexOpenForTest() || !CodexScroll)
	{
		return false;
	}
	LastCodexScrollOffset = FMath::Max(0.0f, Offset);
	CodexScroll->SetScrollOffset(LastCodexScrollOffset);
	return true;
}

void UGameXXKTownHudWidget::HandleConfiguredCodexFilterClicked(EGameXXKCodexCategory Category)
{
	SelectCodexCategoryForTest(Category);
}

void UGameXXKTownHudWidget::HandleConfiguredCodexCardClicked(FName EntryId)
{
	if (!SelectTaskNpcCodexEntryForTest(EntryId))
	{
		SelectCodexEntryForTest(EntryId);
	}
}

void UGameXXKTownHudWidget::CloseAuxiliaryPanels()
{
	if (CharacterPanel)
	{
		CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	CloseCompanionCodex();
	RefreshPanels();
}

void UGameXXKTownHudWidget::SetNotice(const FText& Notice)
{
	if (NoticeText)
	{
		NoticeText->SetText(Notice);
	}
}

void UGameXXKTownHudWidget::HandleTaskClicked()
{
	CloseAuxiliaryPanels();
	SetNotice(FText::GetEmpty());
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->OpenTaskPanel();
	}
}

void UGameXXKTownHudWidget::HandleInventoryClicked()
{
	CloseAuxiliaryPanels();
	SetNotice(FText::GetEmpty());
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->OpenFreeInventoryWindow();
	}
}

void UGameXXKTownHudWidget::HandleCharacterClicked()
{
	SetNotice(FText::GetEmpty());
	CloseCompanionCodex();
	if (CharacterPanel)
	{
		const bool bOpen = CharacterPanel->GetVisibility() != ESlateVisibility::Collapsed;
		CharacterPanel->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	RefreshPanels();
}

void UGameXXKTownHudWidget::HandleMapClicked()
{
	CloseAuxiliaryPanels();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem && GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, TEXT("OpenWorldMap")))
	{
		SetNotice(FText::GetEmpty());
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
		return;
	}

	SetNotice(NSLOCTEXT("GameXXKTownHud", "OpenWorldMapFailed", "暂时无法返回世界地图。"));
}

void UGameXXKTownHudWidget::HandleCompanionClicked()
{
	SetNotice(FText::GetEmpty());
	if (IsCompanionCodexOpenForTest())
	{
		CloseCompanionCodex();
		RefreshPanels();
		return;
	}
	OpenCompanionCodex();
}

void UGameXXKTownHudWidget::HandleCompanionRosterClicked()
{
	CloseAuxiliaryPanels();
	SetNotice(FText::GetEmpty());
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->OpenCompanionRoster();
	}
}

void UGameXXKTownHudWidget::HandleCodexCloseClicked()
{
	CloseCompanionCodex();
	RefreshPanels();
}

void UGameXXKTownHudWidget::HandleResourcePlusClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		CloseAuxiliaryPanels();
		SetNotice(FText::GetEmpty());
		PlayerController->OpenFreeInventoryWindow();
	}
}
