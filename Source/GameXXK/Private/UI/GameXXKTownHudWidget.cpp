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
	const FString ApprovedTextureRoot(TEXT("/Game/GameXXK/UI/MasterV2/Approved/"));
	const FString IdentityPanelTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_IdentityPanel.T_MasterV2_IdentityPanel"));
	const FString HeroPortraitTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_HeroPortrait.T_MasterV2_HeroPortrait"));
	const FString CurrencyStripTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_CurrencyStripShort.T_MasterV2_CurrencyStripShort"));
	const FString IngotTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_Ingot.T_MasterV2_Ingot"));
	const FString NavDiscBackpackTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavDiscBackpack.T_MasterV2_NavDiscBackpack"));
	const FString NavDiscCompanionTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavDiscCompanion.T_MasterV2_NavDiscCompanion"));
	const FString NavDiscCodexTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavDiscCodex.T_MasterV2_NavDiscCodex"));
	const FString NavDiscTaskTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavDiscTask.T_MasterV2_NavDiscTask"));
	const FString NavDiscRouteTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavDiscRoute.T_MasterV2_NavDiscRoute"));
	const FString NavBackpackTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavBackpack.T_MasterV2_NavBackpack"));
	const FString NavCompanionTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavCompanion.T_MasterV2_NavCompanion"));
	const FString NavCodexTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavCodex.T_MasterV2_NavCodex"));
	const FString NavTaskTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavTask.T_MasterV2_NavTask"));
	const FString NavRouteTexturePath(ApprovedTextureRoot + TEXT("T_MasterV2_NavRoute.T_MasterV2_NavRoute"));
	const FString CompanionRosterActionTexturePath(PsdTextureRoot + TEXT("Controls/T_TownPsd_ButtonPrimary.T_TownPsd_ButtonPrimary"));
	const FString BackpackWindowFrameTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame"));
	const FString BackpackHeaderTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Header.T_TownBackpack_Header"));
	const FString BackpackSlotTexturePath(BackpackTextureRoot + TEXT("T_TownBackpack_Slot.T_TownBackpack_Slot"));
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

	if (HeroNameText)
	{
		HeroNameText->SetText(NSLOCTEXT("GameXXKTownHud", "HeroName", "小侠客"));
	}
	if (HeroLevelText)
	{
		HeroLevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), State->PlayerLevel)));
	}
	if (HeroTitleText)
	{
		HeroTitleText->SetText(NSLOCTEXT("GameXXKTownHud", "HeroTitle", "青山游侠"));
	}
	if (IngotValueText)
	{
		IngotValueText->SetText(FText::AsNumber(FMath::Max(0, State->PlayerGold)));
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

	// Master V1 page 02: minimalist town HUD — identity panel, compact ingot
	// currency strip, and five navigation discs. The 3D town world is the
	// backdrop; the runtime draws no full-screen paper over it.
	if (UImage* IdentityPanel = MakeImage(WidgetTree, IdentityPanelTexturePath, FVector2D(541.0f, 182.0f)))
	{
		AddCanvasChild(RootCanvas, IdentityPanel, FVector2D(24.0f, 14.0f), FVector2D(541.0f, 182.0f));
	}
	if (UImage* HeroPortrait = MakeImage(WidgetTree, HeroPortraitTexturePath, FVector2D(118.0f, 127.0f)))
	{
		AddCanvasChild(RootCanvas, HeroPortrait, FVector2D(52.0f, 51.0f), FVector2D(118.0f, 127.0f));
	}
	HeroNameText = MakeNamedText(TEXT("TownHudHeroName"), FText::GetEmpty(), 22, FLinearColor(0.18f, 0.12f, 0.07f, 1.0f));
	AddCanvasChild(RootCanvas, HeroNameText, FVector2D(198.0f, 54.0f), FVector2D(122.0f, 27.0f));
	HeroLevelText = MakeNamedText(TEXT("TownHudHeroLevel"), FText::GetEmpty(), 17, FLinearColor(0.38f, 0.30f, 0.22f, 1.0f));
	AddCanvasChild(RootCanvas, HeroLevelText, FVector2D(198.0f, 91.0f), FVector2D(57.0f, 18.0f));
	HeroTitleText = MakeNamedText(TEXT("TownHudHeroTitle"), FText::GetEmpty(), 17, FLinearColor(0.30f, 0.24f, 0.17f, 1.0f));
	AddCanvasChild(RootCanvas, HeroTitleText, FVector2D(198.0f, 126.0f), FVector2D(71.0f, 19.0f));

	if (UImage* CurrencyStrip = MakeImage(WidgetTree, CurrencyStripTexturePath, FVector2D(320.0f, 86.0f)))
	{
		AddCanvasChild(RootCanvas, CurrencyStrip, FVector2D(1570.0f, 28.0f), FVector2D(320.0f, 86.0f));
	}
	if (UImage* IngotIcon = MakeImage(WidgetTree, IngotTexturePath, FVector2D(40.0f, 40.0f)))
	{
		AddCanvasChild(RootCanvas, IngotIcon, FVector2D(1672.0f, 50.0f), FVector2D(40.0f, 40.0f));
	}
	IngotValueText = MakeNamedText(TEXT("TownHudIngotValue"), FText::GetEmpty(), 18, FLinearColor(0.30f, 0.18f, 0.06f, 1.0f));
	AddCanvasChild(RootCanvas, IngotValueText, FVector2D(1730.0f, 61.0f), FVector2D(46.0f, 22.0f));

	auto MakeNavDisc = [this](const FName Name, const FString& DiscTexture, const FVector2D& DiscPos, const FVector2D& DiscSize,
		const FString& IconTexture, const FVector2D& IconPos, const FVector2D& IconSize) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTextureButtonStyle(DiscTexture, DiscSize));
		Button->SetBackgroundColor(FLinearColor::White);
		AddCanvasChild(RootCanvas, Button, DiscPos, DiscSize);
		if (UImage* Icon = MakeImage(WidgetTree, IconTexture, IconSize))
		{
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			AddCanvasChild(RootCanvas, Icon, IconPos, IconSize);
		}
		return Button;
	};
	TaskButton = MakeNavDisc(TEXT("TownHudTask"), NavDiscTaskTexturePath, FVector2D(23.0f, 651.0f), FVector2D(160.0f, 161.0f),
		NavTaskTexturePath, FVector2D(73.0f, 697.0f), FVector2D(63.0f, 74.0f));
	InventoryButton = MakeNavDisc(TEXT("TownHudInventory"), NavDiscBackpackTexturePath, FVector2D(27.0f, 210.0f), FVector2D(152.0f, 154.0f),
		NavBackpackTexturePath, FVector2D(70.0f, 250.0f), FVector2D(70.0f, 74.0f));
	CodexDiscButton = MakeNavDisc(TEXT("TownHudCodex"), NavDiscCodexTexturePath, FVector2D(30.0f, 504.0f), FVector2D(147.0f, 159.0f),
		NavCodexTexturePath, FVector2D(68.0f, 556.0f), FVector2D(74.0f, 57.0f));
	MapButton = MakeNavDisc(TEXT("TownHudMap"), NavDiscRouteTexturePath, FVector2D(28.0f, 800.0f), FVector2D(155.0f, 154.0f),
		NavRouteTexturePath, FVector2D(68.0f, 850.0f), FVector2D(74.0f, 68.0f));
	CompanionButton = MakeNavDisc(TEXT("TownHudCompanion"), NavDiscCompanionTexturePath, FVector2D(29.0f, 359.0f), FVector2D(148.0f, 150.0f),
		NavCompanionTexturePath, FVector2D(68.0f, 412.0f), FVector2D(74.0f, 45.0f));
	if (TaskButton)
	{
		TaskButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleTaskClicked);
	}
	if (InventoryButton)
	{
		InventoryButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleInventoryClicked);
	}
	if (CodexDiscButton)
	{
		CodexDiscButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCodexDiscClicked);
	}
	if (MapButton)
	{
		MapButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleMapClicked);
	}
	if (CompanionButton)
	{
		CompanionButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleCompanionClicked);
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
	UTextBlock* FilterTitle = MakeNamedText(TEXT("TownHudCodexTitle"), FText::FromString(TEXT("角色图鉴")), 18, FLinearColor(0.18f, 0.10f, 0.05f, 1.0f));
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
		FText::FromString(TEXT("固定 NPC 6 名 · 固定拥有 · 可编入队伍")),
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
	CodexDetailText = MakeNamedText(TEXT("TownHudCodexDetailText"), FText::FromString(TEXT("选择固定 NPC 查看战斗定位与固定三张牌")), 17, FLinearColor(0.20f, 0.11f, 0.05f, 1.0f));
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
			// Selected task-NPC card matches the generic codex card: ink background, white name.
			const bool bTaskNpcSelected = SelectedTaskNpcCodexId == TaskNpcId;
			CardButton->SetBackgroundColor(bTaskNpcSelected
				? FLinearColor(0.12f, 0.09f, 0.06f, 1.0f)
				: FLinearColor::White);

			UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			UTextBlock* NameText = MakeText(WidgetTree, FText::FromString(FString(Presentation->DisplayName)), 11, bTaskNpcSelected
				? FLinearColor::White
				: FLinearColor(0.12f, 0.08f, 0.04f, 1.0f));
			NameText->SetJustification(ETextJustify::Center);
			AddCanvasChild(CardCanvas, NameText, FVector2D(12.0f, 5.0f), FVector2D(89.0f, 17.0f));
			if (UImage* Portrait = MakeImage(WidgetTree, Presentation->PortraitResourcePath, FVector2D(75.0f, 54.0f)))
			{
				AddCanvasChild(CardCanvas, Portrait, FVector2D(19.0f, 23.0f), FVector2D(75.0f, 54.0f));
			}
			UBorder* SupportStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			SupportStrip->SetBrushColor(FLinearColor(0.145f, 0.137f, 0.129f, 1.0f));
			UTextBlock* SupportRoleText = MakeText(WidgetTree, FText::FromString(FString::Printf(TEXT("%s · 固定 NPC"), Presentation->SupportRole)), 8, FLinearColor(0.722f, 0.706f, 0.671f, 1.0f));
			SupportRoleText->SetJustification(ETextJustify::Center);
			SupportStrip->SetContent(SupportRoleText);
			AddCanvasChild(CardCanvas, SupportStrip, FVector2D(12.0f, 81.0f), FVector2D(89.0f, 21.0f));
			UTextBlock* RouteSupportText = MakeText(WidgetTree, FText::FromString(TEXT("永久可用 · 可编入队伍")), 7, FLinearColor(0.28f, 0.19f, 0.12f, 1.0f));
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
				: (bSelected ? FLinearColor(0.12f, 0.09f, 0.06f, 1.0f) : FLinearColor::White));
			UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());

			UTextBlock* CardTitle = MakeText(WidgetTree, EntryView.bIsDiscovered ? EntryView.DisplayName : FText::FromString(TEXT("????")), 11, bSelected
				? FLinearColor::White
				: FLinearColor(0.12f, 0.08f, 0.04f, 1.0f));
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
				TEXT("%s  |  战斗定位：%s\n固定 NPC · 固定拥有 · 可编入队伍\n基础属性：生命 %d  攻击 %d\n防御 %d  真气 %d  被动：%s"),
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
				: FText::FromString(TEXT("选择固定 NPC 查看战斗定位与固定三张牌")));
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
	// Companion nav opens the partner backpack directly. The codex (图鉴) has
	// no dedicated entrance on the page-02 shell, so the local codex overlay
	// stays closed for ordinary navigation.
	CloseAuxiliaryPanels();
	SetNotice(FText::GetEmpty());
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->OpenCompanionRoster();
	}
}

void UGameXXKTownHudWidget::HandleCodexDiscClicked()
{
	CloseAuxiliaryPanels();
	SetNotice(NSLOCTEXT("GameXXKTownHud", "CodexNotAvailable", "图鉴尚未开放。"));
}

void UGameXXKTownHudWidget::HandleCodexCloseClicked()
{
	CloseCompanionCodex();
	RefreshPanels();
}

