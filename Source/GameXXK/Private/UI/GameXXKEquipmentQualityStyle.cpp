#include "UI/GameXXKEquipmentQualityStyle.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

namespace
{
	int32 Rank(EGameXXKEquipmentQuality Quality){return FMath::Clamp(static_cast<int32>(Quality),0,10);}
	UMaterialInstanceDynamic* Instance(UObject* Owner,EGameXXKEquipmentQuality Quality,const TCHAR* Layer)
	{
		const FName Name(*FString::Printf(TEXT("EquipmentQuality%d%s"),Rank(Quality),Layer));
		if(auto* Existing=FindObject<UMaterialInstanceDynamic>(Owner,*Name.ToString()))return Existing;
		auto* Parent=LoadObject<UMaterialInterface>(nullptr,*GameXXKEquipmentQualityStyle::MaterialPath(Quality,Layer));
		return Parent?UMaterialInstanceDynamic::Create(Parent,Owner,Name):nullptr;
	}
	UImage* Image(UWidgetTree* Tree,UButton* Button,const TCHAR* Layer)
	{
		const FName Name(*(Button->GetName()+TEXT("Equipment")+Layer));
		if(auto* Existing=Cast<UImage>(Tree->FindWidget(Name)))return Existing;
		auto* Result=Tree->ConstructWidget<UImage>(UImage::StaticClass(),Name);
		Result->SetVisibility(ESlateVisibility::HitTestInvisible);Result->SetDesiredSizeOverride(FVector2D::ZeroVector);return Result;
	}
	void SetMaterial(UImage* Image,EGameXXKEquipmentQuality Quality,const TCHAR* Layer,FVector2D Size,bool Enabled,bool Animated)
	{
		Image->SetVisibility(Enabled?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
		Image->ForceVolatile(Enabled && Animated);if(!Enabled)return;
		if(auto* Mat=Instance(Image,Quality,Layer))
		{
			Mat->SetScalarParameterValue(TEXT("CardWidth"),Size.X);Mat->SetScalarParameterValue(TEXT("CardHeight"),Size.Y);
			Mat->SetScalarParameterValue(TEXT("TextAspect"),Size.X/FMath::Max(1.0,Size.Y));
			FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.ImageSize=Size;Brush.SetResourceObject(Mat);Image->SetBrush(Brush);
		}
	}
}

FString GameXXKEquipmentQualityStyle::MaterialPath(EGameXXKEquipmentQuality Quality,const TCHAR* Layer)
{
	return FString::Printf(TEXT("/Game/GameXXK/UI/Materials/EquipmentQuality/MI_Equipment_R%02d_%s"),FMath::Max(1,Rank(Quality)),Layer);
}

void GameXXKEquipmentQualityStyle::ApplyName(UTextBlock* Text,EGameXXKEquipmentQuality Quality)
{
	if(!Text)return;auto Font=Text->GetFont();
	Font.FontMaterial=nullptr;Font.OutlineSettings.OutlineMaterial=nullptr;
	Font.OutlineSettings.OutlineSize=0;Font.OutlineSettings.bSeparateFillAlpha=false;
	Text->SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("56534D"))));
	const int32 Tier=Rank(Quality);
	if(Tier>1)
	{
		float Aspect=FMath::Max(1,Text->GetText().ToString().Len());
		if(FSlateApplication::IsInitialized())
		{
			const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			Aspect=Measure->Measure(Text->GetText(),Font).X/FMath::Max(1.0f,static_cast<float>(Measure->GetMaxCharacterHeight(Font)));
		}
		auto* Fill=Instance(Text,Quality,TEXT("Fill"));auto* Outline=Instance(Text,Quality,TEXT("Outline"));
		if(Fill && Outline)
		{
			Fill->SetScalarParameterValue(TEXT("TextAspect"),Aspect);Outline->SetScalarParameterValue(TEXT("TextAspect"),Aspect);
			Font.FontMaterial=Fill;Font.OutlineSettings.OutlineMaterial=Outline;
			Font.OutlineSettings.OutlineSize=Tier>=6?3:2;Font.OutlineSettings.OutlineColor=FLinearColor::White;
			Font.OutlineSettings.bSeparateFillAlpha=true;Text->SetColorAndOpacity(FLinearColor::White);
		}
	}
	Text->SetFont(Font);Text->ForceVolatile(Tier>1);
}

void GameXXKEquipmentQualityStyle::ApplySlot(UWidgetTree* Tree,UButton* Button,UImage* Art,EGameXXKEquipmentQuality Quality,FVector2D ReferenceSize)
{
	if(!Tree || !Button)return;
	auto* Canvas=Cast<UCanvasPanel>(Button->GetContent());auto* Overlay=Cast<UOverlay>(Button->GetContent());
	if(!Canvas && !Overlay)return;
	auto* Surface=Image(Tree,Button,TEXT("Surface"));auto* Aura=Image(Tree,Button,TEXT("Aura"));auto* Frame=Image(Tree,Button,TEXT("Frame"));
	if(!Surface->GetParent())
	{
		if(Canvas)
		{
			for(auto* Layer:{Surface,Aura,Frame})
			{
				auto* Slot=Canvas->AddChildToCanvas(Layer);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(Layer==Surface?-3:Layer==Aura?-2:4);
			}
			if(Art)if(auto* ArtSlot=Cast<UCanvasPanelSlot>(Art->Slot))
			{
				auto* Slot=Cast<UCanvasPanelSlot>(Aura->Slot);const auto Size=ArtSlot->GetSize();const auto Pad=Size*.16;
				Slot->SetAnchors(FAnchors(0,0));Slot->SetPosition(ArtSlot->GetPosition()-Pad);Slot->SetSize(Size+Pad*2);
			}
		}
		else
		{
			struct FChild {UWidget* Widget;FMargin Padding;EHorizontalAlignment H;EVerticalAlignment V;};TArray<FChild> Children;
			for(auto* Child:Overlay->GetAllChildren())if(auto* Slot=Cast<UOverlaySlot>(Child->Slot))Children.Add({Child,Slot->GetPadding(),Slot->GetHorizontalAlignment(),Slot->GetVerticalAlignment()});
			Overlay->ClearChildren();
			for(auto* Layer:{Surface,Aura}){auto* Slot=Overlay->AddChildToOverlay(Layer);Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);}
			for(const auto& Child:Children){auto* Slot=Overlay->AddChildToOverlay(Child.Widget);Slot->SetPadding(Child.Padding);Slot->SetHorizontalAlignment(Child.H);Slot->SetVerticalAlignment(Child.V);}
			auto* Slot=Overlay->AddChildToOverlay(Frame);Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	const int32 Tier=Rank(Quality);
	SetMaterial(Surface,Quality,TEXT("Surface"),ReferenceSize,Tier>0,Tier>=9);
	SetMaterial(Aura,Quality,TEXT("Aura"),ReferenceSize,Tier>=6,Tier>=6);
	SetMaterial(Frame,Quality,TEXT("Frame"),ReferenceSize,Tier>0,Tier>1);
}
