#include "UI/GameXXKCardNameStyle.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void GameXXKCardNameStyle::AttachFrame(UWidgetTree* Tree, UPanelWidget* Face, UTextBlock* Title, const FVector2D& ReferenceSize)
{
	if (!Tree || !Face || !Title) return;
	const FName Name(*(Title->GetName()+TEXT("QualityFrame")));
	if (Tree->FindWidget(Name)) return;
	UImage* Frame = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = ReferenceSize;
	Frame->SetBrush(Brush);
	// The overlay must never enlarge the card's desired layout size.
	Frame->SetDesiredSizeOverride(FVector2D::ZeroVector);
	Frame->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Face))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Frame);
		Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0)); Slot->SetZOrder(4);
	}
	else if (UOverlay* Overlay = Cast<UOverlay>(Face))
	{
		UOverlaySlot* Slot = Overlay->AddChildToOverlay(Frame);
		Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
	}
}

void GameXXKCardNameStyle::Apply(UTextBlock* Text, const EGameXXKCardQuality Quality, const int32 NormalOutlineSize)
{
	if (!Text) return;
	FSlateFontInfo Font = Text->GetFont();
	Font.FontMaterial = nullptr;
	Font.OutlineSettings.OutlineMaterial = nullptr;
	Font.OutlineSettings.OutlineSize = NormalOutlineSize;
	Font.OutlineSettings.OutlineColor = FLinearColor(0.08f, 0.06f, 0.04f, 1.0f);
	Font.OutlineSettings.bSeparateFillAlpha = false;
	const bool bAnimated = Quality == EGameXXKCardQuality::Rare || Quality == EGameXXKCardQuality::Epic;
	const TCHAR* Tier = Quality == EGameXXKCardQuality::Epic ? TEXT("Epic") : TEXT("Rare");
	if (bAnimated)
	{
		const FString Root(TEXT("/Game/GameXXK/UI/Materials/CardNameFlow/MI_CardName"));
		float Aspect = FMath::Max(1, Text->GetText().ToString().Len());
		if (FSlateApplication::IsInitialized())
		{
			const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			Aspect = Measure->Measure(Text->GetText(), Font).X / FMath::Max(1.0f, static_cast<float>(Measure->GetMaxCharacterHeight(Font)));
		}
		const auto Instance = [&](const TCHAR* Layer) -> UMaterialInstanceDynamic*
		{
			const FName Name(*(FString(TEXT("CardName")) + Tier + Layer));
			UMaterialInstanceDynamic* Material = FindObject<UMaterialInstanceDynamic>(Text, *Name.ToString());
			if (!Material)
			{
				UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *(Root + Tier + Layer));
				Material = Parent ? UMaterialInstanceDynamic::Create(Parent, Text, Name) : nullptr;
			}
			if (Material) Material->SetScalarParameterValue(TEXT("TextAspect"), FMath::Max(1.0f, Aspect));
			return Material;
		};
		Font.FontMaterial = Instance(TEXT("Fill"));
		Font.OutlineSettings.OutlineMaterial = Instance(TEXT("Outline"));
		Font.OutlineSettings.OutlineSize = Quality == EGameXXKCardQuality::Epic ? 3 : 2;
		Font.OutlineSettings.OutlineColor = FLinearColor::White;
		Font.OutlineSettings.bSeparateFillAlpha = true;
		Text->SetColorAndOpacity(FLinearColor::White);
	}
	Text->SetFont(Font);
	Text->ForceVolatile(bAnimated);
	UWidgetTree* Tree = Text->GetTypedOuter<UWidgetTree>();
	UImage* Frame = Tree ? Cast<UImage>(Tree->FindWidget(*(Text->GetName()+TEXT("QualityFrame")))) : nullptr;
	if (Frame)
	{
		Frame->SetVisibility(bAnimated ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Frame->ForceVolatile(bAnimated);
		if (bAnimated)
		{
			const FName Name(*(FString(TEXT("QualityFrame")) + Tier));
			UMaterialInstanceDynamic* Material = FindObject<UMaterialInstanceDynamic>(Frame, *Name.ToString());
			if (!Material)
			{
				UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
					*(FString(TEXT("/Game/GameXXK/UI/Materials/CardNameFlow/MI_CardFrame")) + Tier));
				Material = Parent ? UMaterialInstanceDynamic::Create(Parent, Frame, Name) : nullptr;
			}
			if (Material)
			{
				FSlateBrush Brush = Frame->GetBrush();
				const FVector2D Size(Brush.ImageSize);
				Material->SetScalarParameterValue(TEXT("CardWidth"), Size.X);
				Material->SetScalarParameterValue(TEXT("CardHeight"), Size.Y);
				Material->SetScalarParameterValue(TEXT("TextAspect"), Size.X / FMath::Max(1.0, Size.Y));
				Brush.SetResourceObject(Material); Brush.TintColor = FSlateColor(FLinearColor::White);
				Frame->SetBrush(Brush);
			}
		}
	}
}
