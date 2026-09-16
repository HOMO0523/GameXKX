#include "UI/GameXXKRewardPresentation.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKLocalization.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

namespace
{
    constexpr EGameXXKRewardIcon Kinds[]={EGameXXKRewardIcon::Gold,EGameXXKRewardIcon::Normal,EGameXXKRewardIcon::Advanced,EGameXXKRewardIcon::Hunt};
    bool Visible(const UWidget* Widget)
    {
        for(const UWidget* W=Widget;W;W=W->GetParent())
            if(W->GetVisibility()==ESlateVisibility::Collapsed||W->GetVisibility()==ESlateVisibility::Hidden)return false;
        return Widget!=nullptr;
    }
    UGameXXKDesktopTrainingWorkbenchWidget* FindHost(UUserWidget* Source)
    {
        for(UWidget* W=Source;W;W=W->GetParent())
        {
            if(auto* H=Cast<UGameXXKDesktopTrainingWorkbenchWidget>(W))return H;
            if(auto* H=W->GetTypedOuter<UGameXXKDesktopTrainingWorkbenchWidget>())return H;
        }
        auto* PC=Source?Cast<AGameXXKMVPPlayerController>(Source->GetOwningPlayer()):nullptr;
        if(!PC&&Source)PC=Source->GetTypedOuter<AGameXXKMVPPlayerController>();
        return PC?PC->GetDesktopTrainingWorkbenchWidgetForTest():nullptr;
    }
    UGameXXKRewardFlightWidget* LayerFor(UGameXXKDesktopTrainingWorkbenchWidget* H,bool Create)
    {
        if(!H||!H->WidgetTree)return nullptr;
        if(auto* Existing=Cast<UGameXXKRewardFlightWidget>(H->WidgetTree->FindWidget(TEXT("RewardFlightEffects"))))return Existing;
        auto* Root=Cast<UCanvasPanel>(H->WidgetTree->RootWidget);if(!Create||!Root)return nullptr;
        auto* Layer=H->WidgetTree->ConstructWidget<UGameXXKRewardFlightWidget>(UGameXXKRewardFlightWidget::StaticClass(),TEXT("RewardFlightEffects"));
        Layer->Configure(H);
        auto* Placement=Root->AddChildToCanvas(Layer);Placement->SetAnchors(FAnchors(0,0,1,1));Placement->SetOffsets(FMargin(0));Placement->SetZOrder(15);
        return Layer;
    }
    UWidget* Target(UGameXXKDesktopTrainingWorkbenchWidget* H,EGameXXKRewardIcon Kind)
    {
        if(!H||!H->WidgetTree)return nullptr;
        const TCHAR* Normal[]={TEXT("BackpackGoldIcon"),TEXT("TrainingNormalChestButton"),TEXT("TrainingAdvancedChestButton"),TEXT("TrainingHuntChestButton")};
        const TCHAR* Folded[]={TEXT("BackpackTabToggleButton"),TEXT("TrainingFoldedNormalChestButton"),TEXT("TrainingFoldedAdvancedChestButton"),TEXT("TrainingFoldedHuntChestButton")};
        for(const auto Name:{Normal[int32(Kind)],Folded[int32(Kind)]})
            if(auto* W=H->WidgetTree->FindWidget(FName(Name));Visible(W)&&!W->GetCachedGeometry().GetLocalSize().IsNearlyZero())return W;
        return nullptr;
    }
}

int32 FGameXXKRewardBundle::Count(EGameXXKRewardIcon Kind) const
{switch(Kind){case EGameXXKRewardIcon::Gold:return Gold;case EGameXXKRewardIcon::Normal:return Normal;case EGameXXKRewardIcon::Advanced:return Advanced;default:return Hunt;}}

namespace GameXXKRewardPresentation
{
    const TCHAR* Texture(EGameXXKRewardIcon Kind)
    {
        static const TCHAR* Paths[]={TEXT("/Game/GameXXK/UI/Items/T_Item_GoldCoin.T_Item_GoldCoin"),
            TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingNormalChest.T_Item_TrainingNormalChest"),
            TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingAdvancedChest.T_Item_TrainingAdvancedChest"),
            TEXT("/Game/GameXXK/UI/Items/T_Item_TrainingHuntChest.T_Item_TrainingHuntChest")};
        return Paths[int32(Kind)];
    }
    FName IconName(FName Prefix,EGameXXKRewardIcon Kind)
    {
        static const TCHAR* Suffix[]={TEXT("GoldIcon"),TEXT("NormalIcon"),TEXT("AdvancedIcon"),TEXT("HuntIcon")};
        return FName(*(Prefix.ToString()+Suffix[int32(Kind)]));
    }
    UHorizontalBox* BuildRow(UWidgetTree* Tree,const FGameXXKRewardBundle& Bundle,FName Prefix,float IconSize,bool Compact)
    {
        auto* Row=Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),FName(*(Prefix.ToString()+TEXT("Row"))));
        for(const auto Kind:Kinds)
        {
            const int32 Count=Bundle.Count(Kind);if(Count<=0)continue;
            auto* Badge=Tree->ConstructWidget<UHorizontalBox>();
            const FText Name=Kind==EGameXXKRewardIcon::Gold?GameXXKLocalization::Source(TEXT("金币")):
                GameXXKLocalization::Text(Kind==EGameXXKRewardIcon::Normal?TEXT("Chest.Normal.Name"):Kind==EGameXXKRewardIcon::Advanced?TEXT("Chest.Advanced.Name"):TEXT("Chest.Hunt.Name"));
            const FText Caption=Kind==EGameXXKRewardIcon::Gold||Bundle.BoxLevel<=0?Name:FText::FromString(FString::Printf(TEXT("%s · Lv.%d"),*Name.ToString(),Bundle.BoxLevel));
            Badge->SetToolTipText(Caption);
            auto* Icon=Tree->ConstructWidget<UImage>(UImage::StaticClass(),IconName(Prefix,Kind));
            FSlateBrush Brush;Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr,Texture(Kind)));Brush.ImageSize=FVector2D(IconSize,IconSize);
            Icon->SetBrush(Brush);Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* Size=Tree->ConstructWidget<USizeBox>();Size->SetWidthOverride(IconSize);Size->SetHeightOverride(IconSize);Size->SetContent(Icon);
            Badge->AddChildToHorizontalBox(Size)->SetVerticalAlignment(VAlign_Center);
            auto* Labels=Tree->ConstructWidget<UVerticalBox>();
            auto* Amount=Tree->ConstructWidget<UTextBlock>();Amount->SetFont(FGameXXKInRunUiStyle::BodyFont(Compact?24:32,true));
            Amount->SetColorAndOpacity(FGameXXKInRunUiStyle::Ink());Amount->SetText(FText::FromString((Kind==EGameXXKRewardIcon::Gold?FString(TEXT("+")):FString(TEXT("×")))+FText::AsNumber(Count).ToString()));
            Amount->SetVisibility(ESlateVisibility::HitTestInvisible);Labels->AddChildToVerticalBox(Amount);
            if(!Compact)
            {auto* Hint=Tree->ConstructWidget<UTextBlock>();Hint->SetFont(FGameXXKInRunUiStyle::BodyFont(14));Hint->SetColorAndOpacity(FGameXXKInRunUiStyle::MutedInk());Hint->SetText(Caption);Hint->SetVisibility(ESlateVisibility::HitTestInvisible);Labels->AddChildToVerticalBox(Hint);}
            auto* LabelSlot=Badge->AddChildToHorizontalBox(Labels);LabelSlot->SetPadding(FMargin(8,0,0,0));LabelSlot->SetVerticalAlignment(VAlign_Center);
            Row->AddChildToHorizontalBox(Badge)->SetPadding(FMargin(0,0,Compact?16:38,0));
        }
        return Row;
    }
    FGameXXKRewardOrigins Capture(UWidgetTree* Tree,FName Prefix)
    {
        FGameXXKRewardOrigins Result;if(!Tree)return Result;
        for(const auto Kind:Kinds)if(auto* W=Tree->FindWidget(IconName(Prefix,Kind)))
            Result.ScreenCenters.Add(Kind,W->GetCachedGeometry().LocalToAbsolute(W->GetCachedGeometry().GetLocalSize()*.5f));
        return Result;
    }
    FGameXXKRewardFlightSample Sample(FVector2D Start,FVector2D End,float Age,int32 Index,bool Chest)
    {
        FGameXXKRewardFlightSample S;const float T=(Age-Index*.055f)/1.15f;
        S.Position=T>=1?End:Start;if(T<0||T>=1)return S;
        const float Scatter=Chest?16.f:(Index-2)*12.f;
        const FVector2D Lift=Start+FVector2D(Scatter,-32);
        if(T<.23f){const float U=T/.23f;S.Position=FMath::Lerp(Start,Lift,U)-FVector2D(0,20*FMath::Sin(PI*U));S.Scale=FMath::Lerp(.75f,1.f,U);}
        else
        {
            const float U=(T-.23f)/.77f,V=U*U;
            const FVector2D Control=(Lift+End)*.5f+FVector2D(Scatter,-FMath::Min(100.f,float(FVector2D::Distance(Start,End))*.16f));
            S.Position=Lift*FMath::Square(1-V)+Control*(2*(1-V)*V)+End*(V*V);S.Scale=1-.85f*FMath::Pow(U,4);
        }
        S.Opacity=FMath::Min(1.f,T*16);S.Angle=Chest?FMath::Sin(T*10)*.1f:T*(Index%2?2.f:-2.f);return S;
    }
    void Play(UUserWidget* Source,const FGameXXKRewardBundle& Bundle,const FGameXXKRewardOrigins& Origins,FName Key)
    {if(auto* Layer=LayerFor(FindHost(Source),true))Layer->Queue(Bundle,Origins,Key);}
    void Reset(UGameXXKDesktopTrainingWorkbenchWidget* Host)
    {if(auto* Layer=LayerFor(Host,false))Layer->ResetFlights();}
}

void UGameXXKRewardFlightWidget::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InHost)
{
    Host=InHost;Brushes.Reset();
    for(const auto Kind:Kinds){FSlateBrush B;B.SetResourceObject(LoadObject<UTexture2D>(nullptr,GameXXKRewardPresentation::Texture(Kind)));Brushes.Add(B);}
    SetVisibility(ESlateVisibility::HitTestInvisible);SetClipping(EWidgetClipping::ClipToBoundsAlways);ForceVolatile(true);
}
TSharedRef<SWidget> UGameXXKRewardFlightWidget::RebuildWidget()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this);
    if(!WidgetTree->RootWidget)WidgetTree->RootWidget=WidgetTree->ConstructWidget<UCanvasPanel>();
    return Super::RebuildWidget();
}
bool UGameXXKRewardFlightWidget::Queue(const FGameXXKRewardBundle& Bundle,const FGameXXKRewardOrigins& Origins,FName Key)
{
    if(Key.IsNone()||SeenEvents.Contains(Key))return false;
    int32 Added=0;const double Now=FPlatformTime::Seconds();
    for(const auto Kind:Kinds)for(int32 I=0;I<(Kind==EGameXXKRewardIcon::Gold?(Bundle.Gold>0?5:0):FMath::Min(3,FMath::Max(0,Bundle.Count(Kind))));++I)
    {auto& F=Flights.AddDefaulted_GetRef();F.Kind=Kind;F.Index=I;F.SourceScreen=Origins.ScreenCenters.FindRef(Kind);F.QueuedAt=Now;++Added;}
    if(Added)SeenEvents.Add(Key);return Added>0;
}
void UGameXXKRewardFlightWidget::ResetFlights(){Flights.Reset();SeenEvents.Reset();}
void UGameXXKRewardFlightWidget::NativeTick(const FGeometry& G,float DeltaTime)
{
    Super::NativeTick(G,DeltaTime);auto* H=Host.Get();const double Now=FPlatformTime::Seconds();
    if(!H){ResetFlights();return;}
    const FVector2D Size=G.GetLocalSize();
    for(int32 I=Flights.Num()-1;I>=0;--I)
    {
        auto& F=Flights[I];
        if((F.bStarted&&Now-F.StartedAt>1.65)||(!F.bStarted&&Now-F.QueuedAt>3)){Flights.RemoveAt(I);continue;}
        auto* Destination=Visible(H)?Target(H,F.Kind):nullptr;if(!Destination||Size.X<80||Size.Y<80)continue;
        F.End=G.AbsoluteToLocal(Destination->GetCachedGeometry().LocalToAbsolute(Destination->GetCachedGeometry().GetLocalSize()*.5f));
        if(!F.bStarted)
        {
            F.Start=G.AbsoluteToLocal(F.SourceScreen);
            // A fullscreen receipt may close before its desktop receiver appears.
            F.Start.X=FMath::Clamp(F.Start.X,40.,FMath::Max(40.,Size.X-40));F.Start.Y=FMath::Clamp(F.Start.Y,40.,FMath::Max(40.,Size.Y-40));
            F.StartedAt=Now;F.bStarted=true;
        }
        F.Age=Now-F.StartedAt;
    }
}
int32 UGameXXKRewardFlightWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const
{
    Layer=Super::NativePaint(Args,G,Culling,Elements,Layer,Style,Enabled);
    for(const auto& F:Flights)
    {
        if(!F.bStarted)continue;const bool Chest=F.Kind!=EGameXXKRewardIcon::Gold;
        const auto S=GameXXKRewardPresentation::Sample(F.Start,F.End,F.Age,F.Index,Chest);
        const FLinearColor Tint=Style.GetColorAndOpacityTint();
        if(S.Opacity>0)
        {
            const float Size=(Chest?72:48)*S.Scale;
            FSlateDrawElement::MakeRotatedBox(Elements,++Layer,G.ToPaintGeometry(FVector2D(Size),FSlateLayoutTransform(S.Position-FVector2D(Size*.5f))),
                &Brushes[int32(F.Kind)],ESlateDrawEffect::None,S.Angle,TOptional<FVector2f>(),FSlateDrawElement::RelativeToElement,Tint*FLinearColor(1,1,1,S.Opacity));
        }
        const float Arrival=F.Age-(1.15f+F.Index*.055f);
        if(Arrival>=0&&Arrival<.24f)for(int32 J=0;J<6;++J)
        {
            const float Angle=J*PI/3,Radius=17*(1-Arrival/.24f);const FVector2D D(FMath::Cos(Angle),FMath::Sin(Angle));
            TArray<FVector2f> Points={FVector2f(F.End+D*Radius),FVector2f(F.End+D*(Radius+4))};
            FSlateDrawElement::MakeLines(Elements,++Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Tint*FLinearColor(1,.76f,.2f,1-Arrival/.24f),true,2.f);
        }
    }
    return Layer;
}
