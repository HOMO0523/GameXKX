#include "UI/GameXXKWorldMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Styling/SlateTypes.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	const FVector2D RegionMarkerSize(86.0f, 86.0f);
	const FVector2D PlayerMarkerSize(54.0f, 54.0f);
	const FVector2D RegionLabelPlateSize(176.0f, 66.0f);
	const FString WorldMapTextureRoot(TEXT("/Game/GameXXK/UI/Maps/Textures/WorldMap/"));
	const FString WorldMapTerrainTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_Terrain.T_WorldMap_Terrain"));
	const FString WorldMapRegionPathsTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_RegionPaths.T_WorldMap_RegionPaths"));
	const FString WorldMapQingshanMarkerTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_QingshanMarker.T_WorldMap_QingshanMarker"));
	const FString WorldMapLockedMarkerTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_LockedMarker.T_WorldMap_LockedMarker"));
	const FString WorldMapPlayerMarkerTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_PlayerMarker.T_WorldMap_PlayerMarker"));
	const FString WorldMapLabelPlateTexturePath(WorldMapTextureRoot + TEXT("T_WorldMap_RegionLabelPlate.T_WorldMap_RegionLabelPlate"));

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const FVector2D& ImageSize)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Color);
		return Brush;
	}

	FSlateBrush MakeTextureOrSolidBrush(const FString& TexturePath, const FLinearColor& FallbackColor, const FVector2D& ImageSize)
	{
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = ImageSize;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			return Brush;
		}

		return MakeSolidBrush(FallbackColor, ImageSize);
	}

	FButtonStyle MakeRegionButtonStyle(const FString& TexturePath, const FLinearColor& NormalColor)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureOrSolidBrush(TexturePath, NormalColor, RegionMarkerSize));
		Style.SetHovered(MakeTextureOrSolidBrush(TexturePath, NormalColor * FLinearColor(1.10f, 1.10f, 1.10f, 1.0f), RegionMarkerSize));
		Style.SetPressed(MakeTextureOrSolidBrush(TexturePath, NormalColor * FLinearColor(0.78f, 0.78f, 0.78f, 1.0f), RegionMarkerSize));
		Style.SetDisabled(MakeTextureOrSolidBrush(TexturePath, NormalColor * FLinearColor(0.60f, 0.60f, 0.60f, 0.78f), RegionMarkerSize));
		return Style;
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

	UTextBlock* MakeWorldMapText(
		UWidgetTree* WidgetTree,
		FName Name,
		const FText& Text,
		int32 FontSize,
		const FLinearColor& Color)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}
}

TSharedRef<SWidget> UGameXXKWorldMapWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKWorldMapWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bWorldMapActive = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::WorldMap;
	SetVisibility(bWorldMapActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetIsEnabled(bWorldMapActive);
	RefreshProgrammaticLayout();
}

bool UGameXXKWorldMapWidget::TrySelectRegion(FName RegionId)
{
	LastSelectedRegion = RegionId;
	bLastSelectionUnlocked = false;

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::WorldMap)
	{
		SetSelectionNotice(NSLOCTEXT("GameXXKWorldMap", "WrongScreen", "请先进入世界地图。"));
	}
	else if (!IsRegionEnabledForTest(RegionId))
	{
		SetSelectionNotice(NSLOCTEXT("GameXXKWorldMap", "UnavailableTown", "该城镇暂未开放。"));
	}
	else
	{
		bLastSelectionUnlocked = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, TEXT("SelectQingshan"));
		if (!bLastSelectionUnlocked)
		{
			SetSelectionNotice(NSLOCTEXT("GameXXKWorldMap", "LockedTown", "该城镇尚未解锁。"));
		}
		else
		{
			SetSelectionNotice(FText::GetEmpty());
		}
	}

	if (bLastSelectionUnlocked)
	{
		OnUnlockedRegionSelected(RegionId);
	}
	else
	{
		OnLockedRegionSelected(RegionId);
	}

	RefreshFromState();
	if (!NotifyPlayerFlowStateChanged())
	{
		RefreshFromState();
	}
	return bLastSelectionUnlocked;
}

FName UGameXXKWorldMapWidget::GetLastSelectedRegion() const
{
	return LastSelectedRegion;
}

bool UGameXXKWorldMapWidget::WasLastSelectionUnlocked() const
{
	return bLastSelectionUnlocked;
}

bool UGameXXKWorldMapWidget::IsWorldMapVisibleForTest() const
{
	return GetVisibility() == ESlateVisibility::Visible;
}

bool UGameXXKWorldMapWidget::IsRegionEnabledForTest(FName RegionId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::WorldMap || !HasPlayableTarget(RegionId))
	{
		return false;
	}

	return Subsystem->GetRuntimeState().UnlockedRegions.Contains(RegionId);
}

FText UGameXXKWorldMapWidget::GetSelectionNoticeForTest() const
{
	return SelectionNotice;
}

FString UGameXXKWorldMapWidget::GetTerrainResourcePathForTest() const
{
	return WorldMapTerrainTexturePath;
}

void UGameXXKWorldMapWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WorldMapWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldMapRoot"));
	WidgetTree->RootWidget = RootCanvas;

	TerrainImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldMapTerrain"));
	TerrainImage->SetBrush(MakeTextureOrSolidBrush(WorldMapTerrainTexturePath, FLinearColor(0.12f, 0.18f, 0.15f, 1.0f), FVector2D(1920.0f, 1080.0f)));
	TerrainImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, TerrainImage, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	RegionPathsImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldMapRegionPaths"));
	RegionPathsImage->SetBrush(MakeTextureOrSolidBrush(WorldMapRegionPathsTexturePath, FLinearColor(0.73f, 0.64f, 0.42f, 0.58f), FVector2D(1920.0f, 1080.0f)));
	RegionPathsImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, RegionPathsImage, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	PlayerMarkerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldMapPlayerMarker"));
	PlayerMarkerImage->SetBrush(MakeTextureOrSolidBrush(WorldMapPlayerMarkerTexturePath, FLinearColor(0.85f, 0.29f, 0.18f, 1.0f), PlayerMarkerSize));
	PlayerMarkerImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, PlayerMarkerImage, FVector2D(590.0f, 303.0f), PlayerMarkerSize);

	QingshanButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WorldMapRegionQingshan"));
	QingshanButton->SetStyle(MakeRegionButtonStyle(WorldMapQingshanMarkerTexturePath, FLinearColor(0.14f, 0.43f, 0.31f, 1.0f)));
	QingshanButton->SetBackgroundColor(FLinearColor::White);
	QingshanButton->OnClicked.AddDynamic(this, &UGameXXKWorldMapWidget::HandleQingshanClicked);
	AddCanvasChild(RootCanvas, QingshanButton, FVector2D(574.0f, 345.0f), RegionMarkerSize);

	TanjiangButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WorldMapRegionTanjiang"));
	TanjiangButton->SetStyle(MakeRegionButtonStyle(WorldMapLockedMarkerTexturePath, FLinearColor(0.42f, 0.38f, 0.31f, 1.0f)));
	TanjiangButton->SetBackgroundColor(FLinearColor::White);
	TanjiangButton->OnClicked.AddDynamic(this, &UGameXXKWorldMapWidget::HandleTanjiangClicked);
	AddCanvasChild(RootCanvas, TanjiangButton, FVector2D(808.0f, 410.0f), RegionMarkerSize);

	UImage* QingshanLabelPlate = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldMapQingshanLabelPlate"));
	QingshanLabelPlate->SetBrush(MakeTextureOrSolidBrush(WorldMapLabelPlateTexturePath, FLinearColor(0.91f, 0.83f, 0.66f, 0.92f), RegionLabelPlateSize));
	QingshanLabelPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, QingshanLabelPlate, FVector2D(529.0f, 435.0f), RegionLabelPlateSize);
	UTextBlock* QingshanLabel = MakeWorldMapText(WidgetTree, TEXT("WorldMapLabelQingshan"), NSLOCTEXT("GameXXKWorldMap", "QingshanLabel", "青山镇"), 24, FLinearColor(0.10f, 0.18f, 0.16f, 1.0f));
	AddCanvasChild(RootCanvas, QingshanLabel, FVector2D(544.0f, 451.0f), FVector2D(146.0f, 34.0f));

	UImage* TanjiangLabelPlate = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldMapTanjiangLabelPlate"));
	TanjiangLabelPlate->SetBrush(MakeTextureOrSolidBrush(WorldMapLabelPlateTexturePath, FLinearColor(0.74f, 0.70f, 0.61f, 0.84f), RegionLabelPlateSize));
	TanjiangLabelPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, TanjiangLabelPlate, FVector2D(764.0f, 500.0f), RegionLabelPlateSize);
	UTextBlock* TanjiangLabel = MakeWorldMapText(WidgetTree, TEXT("WorldMapLabelTanjiang"), NSLOCTEXT("GameXXKWorldMap", "TanjiangLabel", "潭江渡 · 未开放"), 20, FLinearColor(0.22f, 0.18f, 0.13f, 1.0f));
	AddCanvasChild(RootCanvas, TanjiangLabel, FVector2D(776.0f, 517.0f), FVector2D(152.0f, 32.0f));

	SelectionNoticeText = MakeWorldMapText(WidgetTree, TEXT("WorldMapSelectionNotice"), FText::GetEmpty(), 20, FLinearColor(0.29f, 0.14f, 0.08f, 1.0f));
	SelectionNoticeText->SetAutoWrapText(true);
	AddCanvasChild(RootCanvas, SelectionNoticeText, FVector2D::ZeroVector, FVector2D(520.0f, 48.0f), FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f));
}

void UGameXXKWorldMapWidget::RefreshProgrammaticLayout()
{
	const bool bQingshanEnabled = IsRegionEnabledForTest(UGameXXKMVPRules::RegionQingshan());
	const bool bTanjiangEnabled = IsRegionEnabledForTest(UGameXXKMVPRules::RegionTanjiang());
	if (QingshanButton)
	{
		QingshanButton->SetIsEnabled(bQingshanEnabled);
	}
	if (TanjiangButton)
	{
		TanjiangButton->SetIsEnabled(bTanjiangEnabled);
	}
	if (SelectionNoticeText)
	{
		SelectionNoticeText->SetText(SelectionNotice);
	}
}

bool UGameXXKWorldMapWidget::HasPlayableTarget(FName RegionId) const
{
	return RegionId == UGameXXKMVPRules::RegionQingshan();
}

void UGameXXKWorldMapWidget::SetSelectionNotice(const FText& Notice)
{
	SelectionNotice = Notice;
	if (SelectionNoticeText)
	{
		SelectionNoticeText->SetText(SelectionNotice);
	}
}

void UGameXXKWorldMapWidget::HandleQingshanClicked()
{
	TrySelectRegion(UGameXXKMVPRules::RegionQingshan());
}

void UGameXXKWorldMapWidget::HandleTanjiangClicked()
{
	TrySelectRegion(UGameXXKMVPRules::RegionTanjiang());
}
