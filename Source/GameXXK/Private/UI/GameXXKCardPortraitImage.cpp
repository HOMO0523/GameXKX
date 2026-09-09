#include "UI/GameXXKCardPortraitImage.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"

namespace
{
	class SGameXXKMaskedCardImage final : public SImage
	{
	public:
		SLATE_BEGIN_ARGS(SGameXXKMaskedCardImage) {} SLATE_END_ARGS()
		void Construct(const FArguments&, UGameXXKCardPortraitImage* InOwner)
		{
			Owner = InOwner;
			SImage::Construct(SImage::FArguments().Image(&InOwner->GetBrush())
				.FlipForRightToLeftFlowDirection(InOwner->ShouldFlipForRightToLeftFlowDirection()));
		}
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
			FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool ParentEnabled) const override
		{
			const FSlateBrush* Source = GetImageAttribute().Get();
			if (!Owner.IsValid() || !Source || Source->DrawAs == ESlateBrushDrawType::NoDrawType)
				return SImage::OnPaint(Args,Geometry,Culling,Elements,Layer,Style,ParentEnabled);
			const FGeometry PaintGeometry = bFlipForRightToLeftFlowDirection && GSlateFlowDirection == EFlowDirection::RightToLeft
				? Geometry.MakeChild(FSlateRenderTransform(FScale2D(-1,1))) : Geometry;
			const FSlateBrush* Brush = Owner->PrepareMaskedBrush(PaintGeometry,*Source);
			const FLinearColor Tint = Style.GetColorAndOpacityTint()
				* GetColorAndOpacityAttribute().Get().GetColor(Style) * Brush->GetTint(Style);
			FSlateDrawElement::MakeBox(Elements,Layer,PaintGeometry.ToPaintGeometry(),Brush,
				ShouldBeEnabled(ParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,Tint);
			return Layer;
		}
	private:
		TWeakObjectPtr<UGameXXKCardPortraitImage> Owner;
	};
}

TSharedRef<SWidget> UGameXXKCardPortraitImage::RebuildWidget()
{
	MyImage = SNew(SGameXXKMaskedCardImage,this);
	return MyImage.ToSharedRef();
}

const FSlateBrush* UGameXXKCardPortraitImage::PrepareMaskedBrush(const FGeometry& Geometry, const FSlateBrush& SourceBrush)
{
	UTexture2D* Texture = Cast<UTexture2D>(SourceBrush.GetResourceObject());
	if (!Texture || !CardFace.IsValid()) return &SourceBrush;
	if (!MaskMaterial)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/GameXXK/UI/Materials/CardPortrait/M_CardPortraitRoundedMask"));
		if (!Parent) return &SourceBrush;
		MaskMaterial = UMaterialInstanceDynamic::Create(Parent,this);
	}
	const FVector2f ImageSize(Geometry.GetLocalSize());
	// Match the child's current paint space. Tick geometry can include the
	// floating window's desktop offset and must not be mixed into this mask.
	const FGeometry& CardGeometry = CardFace->GetPaintSpaceGeometry();
	FVector2f CardSize(CardGeometry.GetLocalSize());
	bUsesCardGeometry = CardSize.X > 1 && CardSize.Y > 1;
	FVector2f Origin = FVector2f::ZeroVector;
	FVector2f AxisX(ImageSize.X,0), AxisY(0,ImageSize.Y);
	if (bUsesCardGeometry)
	{
		// One relative render transform cancels shared DPI, window origin,
		// scrolling and whole-card hover/flip transforms. No layout is changed.
		const FSlateRenderTransform ImageToCard = Concatenate(Geometry.GetAccumulatedRenderTransform(),
			Inverse(CardGeometry.GetAccumulatedRenderTransform()));
		Origin = TransformPoint(ImageToCard,FVector2f::ZeroVector);
		AxisX = TransformVector(ImageToCard,FVector2f(ImageSize.X,0));
		AxisY = TransformVector(ImageToCard,FVector2f(0,ImageSize.Y));
	}
	else CardSize = ImageSize;
	if (CardSize.X <= 1 || CardSize.Y <= 1) return &SourceBrush;
	if (CachedTexture != Texture)
	{
		MaskMaterial->SetTextureParameterValue(TEXT("ArtTexture"),Texture);
		CachedTexture = Texture;
	}
	if (!CachedCardSize.Equals(CardSize,0.01f) || !CachedOrigin.Equals(Origin,0.01f)
		|| !CachedAxisX.Equals(AxisX,0.01f) || !CachedAxisY.Equals(AxisY,0.01f))
	{
		MaskMaterial->SetVectorParameterValue(TEXT("CardSize"),FLinearColor(CardSize.X,CardSize.Y,0,0));
		MaskMaterial->SetVectorParameterValue(TEXT("ArtOrigin"),FLinearColor(Origin.X,Origin.Y,0,0));
		MaskMaterial->SetVectorParameterValue(TEXT("ArtAxisX"),FLinearColor(AxisX.X,AxisX.Y,0,0));
		MaskMaterial->SetVectorParameterValue(TEXT("ArtAxisY"),FLinearColor(AxisY.X,AxisY.Y,0,0));
		CachedCardSize=CardSize; CachedOrigin=Origin; CachedAxisX=AxisX; CachedAxisY=AxisY;
	}
	MaskBrush = SourceBrush;
	MaskBrush.SetResourceObject(MaskMaterial);
	return &MaskBrush;
}
