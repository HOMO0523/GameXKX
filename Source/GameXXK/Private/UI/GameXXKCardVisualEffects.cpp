#include "UI/GameXXKCardVisualEffects.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace GameXXKCardVisualEffects
{
	FGameXXKCardMotionSample Deal(float Age,int32 Index,int32 Order)
	{
		FGameXXKCardMotionSample S;
		const float T=FMath::Clamp((Age-FMath::Max(0,Order==INDEX_NONE ? Index : Order)*0.045f)/0.34f,0.0f,1.0f);
		if(T>=1.0f)return S;
		const float Ease=1-FMath::Pow(1-T,3.0f);
		S.Offset=FVector2D(-(160+FMath::Max(0,Index)*214.0f)*(1-Ease),115*(1-Ease)-26*FMath::Sin(PI*T));
		const float Scale=FMath::Lerp(0.70f,1.0f,Ease);S.Scale=FVector2D(Scale,Scale);
		S.Angle=-9*(1-Ease);S.Opacity=FMath::Clamp(T*4,0.0f,1.0f);
		return S;
	}
	FGameXXKCardMotionSample Flip(float Age,int32 Index)
	{
		FGameXXKCardMotionSample S;
		const float T=FMath::Clamp((Age-FMath::Max(0,Index)*0.065f)/0.44f,0.0f,1.0f);
		if(T>=1.0f)return S;
		S.Scale=FVector2D(FMath::Max(0.035f,FMath::Abs(FMath::Cos(PI*T))),1+0.025f*FMath::Sin(PI*T));
		S.Offset=FVector2D(0,18*(1-T));S.Angle=2*FMath::Sin(2*PI*T);
		S.bFrontFace=T>=0.5f;S.Opacity=FMath::Clamp((Age-Index*0.065f)*9+0.18f,0.0f,1.0f);
		return S;
	}
	float Breath(float Seconds) { return 0.46f+0.54f*(0.5f+0.5f*FMath::Sin(Seconds*2*PI/1.8f)); }
	FLinearColor StatusColor(EGameXXKCardStatus Status)
	{
		if (Status == EGameXXKCardStatus::Burn) return FLinearColor(1.0f,0.40f,0.055f,1);
		if (Status == EGameXXKCardStatus::Poison) return FLinearColor(0.24f,0.84f,0.28f,1);
		if (Status == EGameXXKCardStatus::Bleed) return FLinearColor(0.98f,0.13f,0.10f,1);
		return FLinearColor(1.0f,0.95f,0.82f,1);
	}
	FLinearColor DamageColor(EGameXXKCardDamageCause Cause)
	{
		switch (Cause)
		{
		case EGameXXKCardDamageCause::Burn: case EGameXXKCardDamageCause::ToxicExplosionBurn: return StatusColor(EGameXXKCardStatus::Burn);
		case EGameXXKCardDamageCause::Poison: case EGameXXKCardDamageCause::ToxicExplosionPoison: return StatusColor(EGameXXKCardStatus::Poison);
		case EGameXXKCardDamageCause::Bleed: case EGameXXKCardDamageCause::ToxicExplosionBleed: return StatusColor(EGameXXKCardStatus::Bleed);
		default: return FLinearColor(0.95f,0.82f,0.42f,1);
		}
	}
}

namespace
{
	class SGameXXKCardAura final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGameXXKCardAura) {} SLATE_END_ARGS()
		void Construct(const FArguments&,UGameXXKCardAuraWidget* InOwner) { Owner=InOwner; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
		virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool ParentEnabled) const override
		{
			if(!Owner.IsValid() || Owner->GetCues().IsEmpty())return Layer;
			const auto& Cues=Owner->GetCues();const auto& Main=Cues[0];
			const float Time=static_cast<float>(FMath::Fmod(FSlateApplication::Get().GetCurrentTime(),180.0));
			const float Power=0.70f*Main.Strength*GameXXKCardVisualEffects::Breath(Time)*(ParentEnabled ? 1.0f : 0.3f);
			const FVector2f Size(Geometry.GetLocalSize());
			if(Size.X<10 || Size.Y<10)return Layer;
			constexpr int32 ArcSteps = 16;
			TArray<FVector2f> Outline;Outline.Reserve(4*(ArcSteps+1)+1);
			const float Radius=8;
			// Keep the light under the ink edge; only its soft outer falloff escapes
			// the paper. Symmetric insets avoid a detached strip on the right side.
			const float Inset=2.5f;
			const FVector2f Centers[]={FVector2f(Size.X-Radius-Inset,Radius+Inset),FVector2f(Size.X-Radius-Inset,Size.Y-Radius-Inset),FVector2f(Radius+Inset,Size.Y-Radius-Inset),FVector2f(Radius+Inset,Radius+Inset)};
			for(int32 Corner=0;Corner<4;++Corner)for(int32 P=0;P<=ArcSteps;++P)
			{
				const float A=(-90+Corner*90+P*90.0f/ArcSteps)*PI/180;
				Outline.Add(Centers[Corner]+FVector2f(FMath::Cos(A),FMath::Sin(A))*Radius);
			}
			const FVector2f ClosingPoint=Outline[0];
			Outline.Add(ClosingPoint);
			auto Ink=[&](FLinearColor Color,float Alpha,float Width,const TArray<FVector2f>& Points)
			{
				Color*=Style.GetColorAndOpacityTint();Color.A*=Alpha*Power;
				FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Width);
			};
			Ink(Main.Color,0.36f,12,Outline);Ink(Main.Color,0.76f,8,Outline);Ink(Main.Color,1.0f,4.0f,Outline);
			// A short moving edge catches the eye without lighting the card artwork.
			TArray<FVector2f> Highlight;
			const int32 Segments = Outline.Num()-1;
			const int32 Start=static_cast<int32>(Time*Segments/4.5f)%Segments;
			for(int32 I=0;I<10;++I)Highlight.Add(Outline[(Start+I)%Segments]);
			// Keep the highlight in the same saturated hue. Mixing pale paper into
			// blue-purple made the entire halo read as white on the cream card edge.
			Ink(Main.Color,0.96f,5.0f,Highlight);
			for(int32 I=1;I<Cues.Num();++I)
			{
				if(Cues[I].Color.Equals(Main.Color,0.05f))continue;
				TArray<FVector2f> Corner={FVector2f(Size.X-28,Size.Y-Inset)};
				for (int32 P=ArcSteps;P>=0;--P)
				{
					const float A=P*0.5f*PI/ArcSteps;
					Corner.Add(Centers[1]+FVector2f(FMath::Cos(A),FMath::Sin(A))*Radius);
				}
				Corner.Add(FVector2f(Size.X-Inset,Size.Y-28));
				Ink(Cues[I].Color,0.75f,3.4f,Corner);break;
			}
			return Layer;
		}
	private:
		TWeakObjectPtr<UGameXXKCardAuraWidget> Owner;
	};
}

void UGameXXKCardAuraWidget::SetCues(const TArray<FGameXXKCardSynergyCue>& InCues)
{
	Cues=InCues;SetRenderOpacity(1);ForceVolatile(!Cues.IsEmpty());
	SetVisibility(Cues.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
}
FString UGameXXKCardAuraWidget::GetCueDebugText() const { return GameXXKCardSynergyPresentation::Describe(Cues); }
TSharedRef<SWidget> UGameXXKCardAuraWidget::RebuildWidget() { return SNew(SGameXXKCardAura,this); }
