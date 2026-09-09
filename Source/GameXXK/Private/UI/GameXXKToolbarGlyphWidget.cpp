#include "UI/GameXXKToolbarGlyphWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

class SGameXXKToolbarGlyph : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGameXXKToolbarGlyph) {} SLATE_END_ARGS()
	void Construct(const FArguments&) {}
	void Configure(int32 InSymbol, bool bInActive) { Symbol=InSymbol; bActive=bInActive; Invalidate(EInvalidateWidgetReason::Paint); }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(32,32); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
		int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size=Geometry.GetLocalSize();
		const float Scale=FMath::Min(Size.X,Size.Y)/32.0f;
		FLinearColor Color(.16f,.12f,.075f,1);
		if (Symbol==0 && bActive) Color=FLinearColor(.055f,.24f,.19f,1);
		if (Symbol==1 && bActive) Color=FLinearColor(.40f,.075f,.045f,1);
		Color*=Style.GetColorAndOpacityTint();
		if (!bParentEnabled) Color.A*=.45f;
		auto Transform=[&](FVector2f Point)
		{
			Point-=FVector2f(16,16);
			if (Symbol==0 && !bActive)
			{
				const float X=Point.X;Point.X=X*.9063f+Point.Y*.4226f;Point.Y=-X*.4226f+Point.Y*.9063f;
			}
			return Point*Scale+FVector2f(Size.X*.5f,Size.Y*.5f);
		};
		auto Line=[&](TArray<FVector2f> Points, float Weight=3.0f)
		{
			for (FVector2f& Point:Points)
			{
				Point-=FVector2f(16,16);
				if (Symbol==0 && !bActive)
				{
					const float X=Point.X; Point.X=X*.9063f+Point.Y*.4226f; Point.Y=-X*.4226f+Point.Y*.9063f;
				}
				Point=Point*Scale+FVector2f(Size.X*.5f,Size.Y*.5f);
			}
			FSlateDrawElement::MakeLines(Elements,Layer+1,Geometry.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color,true,Weight*Scale);
		};
		auto Fill=[&](const TArray<FVector2f>& Points)
		{
			TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
			for(const FVector2f& Point:Points) Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),Transform(Point),FVector2f(.5f,.5f),Color.ToFColor(true)));
			for(int32 I=1;I+1<Points.Num();++I) {Indices.Add(0);Indices.Add(static_cast<SlateIndex>(I));Indices.Add(static_cast<SlateIndex>(I+1));}
			const FSlateResourceHandle Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
			FSlateDrawElement::MakeCustomVerts(Elements,Layer+1,Resource,Vertices,Indices,nullptr,0,0);
		};
		switch(Symbol)
		{
		case 0:
			Fill({{7,3},{25,3},{25,9},{7,9}});Fill({{12,8},{20,8},{20,16},{12,16}});
			Fill({{12,14},{20,14},{27,22},{5,22}});Fill({{14,21},{18,21},{16,31}});break;
		case 1:
			Fill({{3,11},{11,11},{11,21},{3,21}});Fill({{10,11},{19,4},{19,28},{10,21}});
			if(bActive) {Line({{23,12},{30,20}},4);Line({{30,12},{23,20}},4);}
			else {Line({{23,11},{25,14},{25,18},{23,21}},3.5f);Line({{27,6},{30,11},{31,16},{30,21},{27,26}},3.5f);}break;
		case 2:
			Fill({{4,6},{28,6},{16,16.5f}});Fill({{3,9},{14,18.5f},{3,27}});
			Fill({{29,9},{29,27},{18,18.5f}});Fill({{4,28},{15,19},{17,19},{28,28}});break;
		case 3:
		{
			for(int32 I=0;I<24;++I)
			{
				const float A=I*2*PI/24,B=(I+1)*2*PI/24;
				const float R=(I%4<2)?13.0f:10.0f,R2=((I+1)%4<2)?13.0f:10.0f;
				Fill({{16+R*FMath::Cos(A),16+R*FMath::Sin(A)},{16+R2*FMath::Cos(B),16+R2*FMath::Sin(B)},
					{16+4.2f*FMath::Cos(B),16+4.2f*FMath::Sin(B)},{16+4.2f*FMath::Cos(A),16+4.2f*FMath::Sin(A)}});
			}
			break;
		}
		case 4:
		{
			TArray<FVector2f> Arc;
			for(int32 I=0;I<=32;++I) {const float A=(-50+280.0f*I/32)*PI/180;Arc.Add({16+10.5f*FMath::Cos(A),17+10.5f*FMath::Sin(A)});}
			Line(Arc,5.0f);Line({{16,2},{16,15}},5.0f);break;
		}
		case 5:
			Line({{2,4},{7,4},{11,23},{27,23}},3.5f);
			Fill({{8,8},{31,8},{27,19},{11,19}});
			Fill({{11,26},{15,26},{15,30},{11,30}});
			Fill({{23,26},{27,26},{27,30},{23,30}});break;
		default:break;
		}
		return Layer+1;
	}
private:
	int32 Symbol=0;
	bool bActive=false;
};

void UGameXXKToolbarGlyphWidget::Configure(int32 InSymbol,bool bInActive)
{
	Symbol=InSymbol;bActive=bInActive;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if(Glyph) Glyph->Configure(Symbol,bActive);
}
TSharedRef<SWidget> UGameXXKToolbarGlyphWidget::RebuildWidget()
{
	Glyph=SNew(SGameXXKToolbarGlyph);Glyph->Configure(Symbol,bActive);return Glyph.ToSharedRef();
}
void UGameXXKToolbarGlyphWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);Glyph.Reset();
}
