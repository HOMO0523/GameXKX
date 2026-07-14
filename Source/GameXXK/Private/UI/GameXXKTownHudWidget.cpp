#include "UI/GameXXKTownHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FString HudTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/HUD/"));
	const FString NavTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Nav/"));
	const FString CharacterTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Character/"));
	const FString CompanionTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Companion/"));
	const FString ProfileTexturePath(HudTextureRoot + TEXT("T_TownHUD_ProfilePortrait.T_TownHUD_ProfilePortrait"));
	const FString ResourceCoinTexturePath(HudTextureRoot + TEXT("T_TownHUD_ResourceCoin.T_TownHUD_ResourceCoin"));
	const FString ResourceGreenTexturePath(HudTextureRoot + TEXT("T_TownHUD_ResourceGreen.T_TownHUD_ResourceGreen"));
	const FString ResourceIngotTexturePath(HudTextureRoot + TEXT("T_TownHUD_ResourceIngot.T_TownHUD_ResourceIngot"));
	const FString ResourcePlusTexturePath(HudTextureRoot + TEXT("T_TownHUD_ResourcePlus.T_TownHUD_ResourcePlus"));
	const FString NavSidebarTexturePath(NavTextureRoot + TEXT("T_TownNav_Sidebar.T_TownNav_Sidebar"));
	const FString NavTaskTexturePath(NavTextureRoot + TEXT("T_TownNav_Task.T_TownNav_Task"));
	const FString NavInventoryTexturePath(NavTextureRoot + TEXT("T_TownNav_Inventory.T_TownNav_Inventory"));
	const FString NavRefineTexturePath(NavTextureRoot + TEXT("T_TownNav_Refine.T_TownNav_Refine"));
	const FString NavMapTexturePath(NavTextureRoot + TEXT("T_TownNav_Map.T_TownNav_Map"));
	const FString NavFriendsTexturePath(NavTextureRoot + TEXT("T_TownNav_Friends.T_TownNav_Friends"));
	const FString CharacterAttributeTexturePath(CharacterTextureRoot + TEXT("T_TownCharacter_AttributeSelected.T_TownCharacter_AttributeSelected"));
	const FString CompanionTabTexturePath(CompanionTextureRoot + TEXT("T_TownCompanion_AllSelected.T_TownCompanion_AllSelected"));

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

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Path, ImageSize));
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
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
	SetVisibility(bInTown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetIsEnabled(bInTown);
	if (!State)
	{
		return;
	}

	const int32 Power = State->PlayerAttack + State->PlayerDefense + State->PlayerSpeed;
	int32 MaterialCount = 0;
	for (const TPair<FName, int32>& Entry : State->Inventory)
	{
		MaterialCount += FMath::Max(0, Entry.Value);
	}
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("小侠客  Lv.%d"), State->PlayerLevel)));
	}
	if (ExperienceText)
	{
		ExperienceText->SetText(FText::FromString(FString::Printf(TEXT("经验 %d"), State->PlayerXP)));
	}
	if (PowerText)
	{
		PowerText->SetText(FText::FromString(FString::Printf(TEXT("战力 %d"), Power)));
	}
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::FromInt(State->PlayerGold)));
	}
	if (ExperienceResourceText)
	{
		ExperienceResourceText->SetText(FText::FromString(FString::FromInt(State->PlayerXP)));
	}
	if (MaterialText)
	{
		MaterialText->SetText(FText::FromString(FString::FromInt(MaterialCount)));
	}
	RefreshPanels();
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

	if (UImage* Portrait = MakeImage(WidgetTree, ProfileTexturePath, FVector2D(166.0f, 182.0f)))
	{
		AddCanvasChild(RootCanvas, Portrait, FVector2D(18.0f, 16.0f), FVector2D(166.0f, 182.0f));
	}
	LevelText = MakeText(WidgetTree, FText::GetEmpty(), 22);
	ExperienceText = MakeText(WidgetTree, FText::GetEmpty(), 17, FLinearColor(0.24f, 0.23f, 0.21f, 1.0f));
	PowerText = MakeText(WidgetTree, FText::GetEmpty(), 19, FLinearColor(0.52f, 0.12f, 0.06f, 1.0f));
	AddCanvasChild(RootCanvas, LevelText, FVector2D(185.0f, 38.0f), FVector2D(220.0f, 35.0f));
	AddCanvasChild(RootCanvas, ExperienceText, FVector2D(185.0f, 83.0f), FVector2D(180.0f, 28.0f));
	AddCanvasChild(RootCanvas, PowerText, FVector2D(185.0f, 120.0f), FVector2D(190.0f, 30.0f));

	if (UImage* Sidebar = MakeImage(WidgetTree, NavSidebarTexturePath, FVector2D(78.0f, 430.0f)))
	{
		AddCanvasChild(RootCanvas, Sidebar, FVector2D(22.0f, 210.0f), FVector2D(78.0f, 430.0f));
	}
	auto MakeNavButton = [this](const FName Name, const FString& TexturePath, float Y) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeTextureButtonStyle(TexturePath, FVector2D(66.0f, 68.0f)));
		AddCanvasChild(RootCanvas, Button, FVector2D(28.0f, Y), FVector2D(66.0f, 68.0f));
		return Button;
	};
	TaskButton = MakeNavButton(TEXT("TownHudTask"), NavTaskTexturePath, 225.0f);
	InventoryButton = MakeNavButton(TEXT("TownHudInventory"), NavInventoryTexturePath, 305.0f);
	CharacterButton = MakeNavButton(TEXT("TownHudCharacter"), NavRefineTexturePath, 385.0f);
	MapButton = MakeNavButton(TEXT("TownHudMap"), NavMapTexturePath, 465.0f);
	CompanionButton = MakeNavButton(TEXT("TownHudCompanion"), NavFriendsTexturePath, 545.0f);
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

	auto MakeResource = [this](const FString& TexturePath, float X, UTextBlock*& OutText)
	{
		if (UImage* Icon = MakeImage(WidgetTree, TexturePath, FVector2D(30.0f, 30.0f)))
		{
			AddCanvasChild(RootCanvas, Icon, FVector2D(X, 24.0f), FVector2D(30.0f, 30.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
		}
		OutText = MakeText(WidgetTree, FText::GetEmpty(), 18);
		AddCanvasChild(RootCanvas, OutText, FVector2D(X + 35.0f, 23.0f), FVector2D(72.0f, 31.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
	};
	MakeResource(ResourceCoinTexturePath, -380.0f, GoldText);
	MakeResource(ResourceGreenTexturePath, -250.0f, ExperienceResourceText);
	MakeResource(ResourceIngotTexturePath, -120.0f, MaterialText);
	ResourcePlusButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TownHudResourcePlus"));
	ResourcePlusButton->SetStyle(MakeTextureButtonStyle(ResourcePlusTexturePath, FVector2D(28.0f, 28.0f)));
	ResourcePlusButton->OnClicked.AddDynamic(this, &UGameXXKTownHudWidget::HandleResourcePlusClicked);
	AddCanvasChild(RootCanvas, ResourcePlusButton, FVector2D(-38.0f, 25.0f), FVector2D(28.0f, 28.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));

	CharacterPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCharacterPanel"));
	CharacterPanel->SetPadding(FMargin(24.0f));
	CharacterPanel->SetBrushColor(FLinearColor(0.95f, 0.91f, 0.83f, 0.97f));
	CharacterStatsText = MakeText(WidgetTree, FText::GetEmpty(), 20);
	CharacterPanel->SetContent(CharacterStatsText);
	CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(RootCanvas, CharacterPanel, FVector2D(-28.0f, 130.0f), FVector2D(410.0f, 300.0f), FVector2D(1.0f, 0.0f), FAnchors(1.0f, 0.0f));
	if (UImage* CharacterLabel = MakeImage(WidgetTree, CharacterAttributeTexturePath, FVector2D(105.0f, 40.0f)))
	{
		AddCanvasChild(RootCanvas, CharacterLabel, FVector2D(-408.0f, 140.0f), FVector2D(105.0f, 40.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
	}

	CompanionPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownHudCompanionPanel"));
	CompanionPanel->SetPadding(FMargin(24.0f));
	CompanionPanel->SetBrushColor(FLinearColor(0.95f, 0.91f, 0.83f, 0.97f));
	CompanionStatusText = MakeText(WidgetTree, FText::GetEmpty(), 20);
	CompanionPanel->SetContent(CompanionStatusText);
	CompanionPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(RootCanvas, CompanionPanel, FVector2D(-28.0f, 130.0f), FVector2D(410.0f, 240.0f), FVector2D(1.0f, 0.0f), FAnchors(1.0f, 0.0f));
	if (UImage* CompanionLabel = MakeImage(WidgetTree, CompanionTabTexturePath, FVector2D(105.0f, 40.0f)))
	{
		CompanionLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddCanvasChild(RootCanvas, CompanionLabel, FVector2D(-408.0f, 140.0f), FVector2D(105.0f, 40.0f), FVector2D::ZeroVector, FAnchors(1.0f, 0.0f));
	}

	NoticeText = MakeText(WidgetTree, FText::GetEmpty(), 19, FLinearColor(0.24f, 0.18f, 0.10f, 1.0f));
	NoticeText->SetJustification(ETextJustify::Center);
	AddCanvasChild(RootCanvas, NoticeText, FVector2D(0.0f, -92.0f), FVector2D(520.0f, 40.0f), FVector2D(0.5f, 1.0f), FAnchors(0.5f, 1.0f));
}

void UGameXXKTownHudWidget::RefreshPanels()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
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
	if (CompanionStatusText)
	{
		CompanionStatusText->SetText(State.bFollowerJoined
			? NSLOCTEXT("GameXXKTownHud", "FollowerJoined", "伙伴\n\n引路人\n已与你同行\n\n在北门出口继续主线任务。")
			: NSLOCTEXT("GameXXKTownHud", "FollowerNotJoined", "伙伴\n\n暂无同行伙伴\n\n前往镇中寻找引路人。"));
	}
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
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		if (CharacterPanel)
		{
			CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (CompanionPanel)
		{
			CompanionPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
		PlayerController->OpenTaskPanel();
	}
}

void UGameXXKTownHudWidget::HandleInventoryClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->CloseTaskPanel();
		PlayerController->OpenFreeInventoryWindow();
	}
}

void UGameXXKTownHudWidget::HandleCharacterClicked()
{
	if (CharacterPanel)
	{
		const bool bOpen = CharacterPanel->GetVisibility() != ESlateVisibility::Collapsed;
		CharacterPanel->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CompanionPanel)
	{
		CompanionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshPanels();
}

void UGameXXKTownHudWidget::HandleMapClicked()
{
	SetNotice(NSLOCTEXT("GameXXKTownHud", "MapLocked", "黄山世界地图暂未开放，当前请在青山镇推进主线。"));
}

void UGameXXKTownHudWidget::HandleCompanionClicked()
{
	if (CompanionPanel)
	{
		const bool bOpen = CompanionPanel->GetVisibility() != ESlateVisibility::Collapsed;
		CompanionPanel->SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CharacterPanel)
	{
		CharacterPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshPanels();
}

void UGameXXKTownHudWidget::HandleResourcePlusClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		if (PlayerController->OpenMerchantTradeWindow())
		{
			SetNotice(NSLOCTEXT("GameXXKTownHud", "MerchantOpened", "已打开商店。"));
		}
	}
}
