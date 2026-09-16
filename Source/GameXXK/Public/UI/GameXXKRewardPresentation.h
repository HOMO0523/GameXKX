#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "GameXXKRewardPresentation.generated.h"

class UHorizontalBox;
class UWidgetTree;
class UGameXXKDesktopTrainingWorkbenchWidget;

enum class EGameXXKRewardIcon : uint8 { Gold, Normal, Advanced, Hunt };
struct FGameXXKRewardBundle
{
    int32 Gold=0, Normal=0, Advanced=0, Hunt=0, BoxLevel=0;
    int32 Count(EGameXXKRewardIcon Kind) const;
};
struct FGameXXKRewardOrigins { TMap<EGameXXKRewardIcon,FVector2D> ScreenCenters; };
struct FGameXXKRewardFlightSample
{
    FVector2D Position=FVector2D::ZeroVector;
    float Scale=0, Opacity=0, Angle=0;
};

namespace GameXXKRewardPresentation
{
    GAMEXXK_API const TCHAR* Texture(EGameXXKRewardIcon Kind);
    GAMEXXK_API FName IconName(FName Prefix,EGameXXKRewardIcon Kind);
    GAMEXXK_API UHorizontalBox* BuildRow(UWidgetTree* Tree,const FGameXXKRewardBundle& Bundle,FName Prefix,float IconSize=72,bool Compact=false);
    GAMEXXK_API FGameXXKRewardOrigins Capture(UWidgetTree* Tree,FName Prefix);
    GAMEXXK_API FGameXXKRewardFlightSample Sample(FVector2D Start,FVector2D End,float Age,int32 Index,bool Chest);
    GAMEXXK_API void Play(UUserWidget* Source,const FGameXXKRewardBundle& Bundle,const FGameXXKRewardOrigins& Origins,FName EventKey);
    GAMEXXK_API void Reset(UGameXXKDesktopTrainingWorkbenchWidget* Host);
}

/** Transient feedback for already-committed rewards; never owns currency or inventory. */
UCLASS()
class GAMEXXK_API UGameXXKRewardFlightWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(UGameXXKDesktopTrainingWorkbenchWidget* Host);
    bool Queue(const FGameXXKRewardBundle& Bundle,const FGameXXKRewardOrigins& Origins,FName EventKey);
    void ResetFlights();
    UFUNCTION(BlueprintPure,Category="GameXXK|Presentation|Test") int32 GetRewardEventCountForTest() const { return SeenEvents.Num(); }
    UFUNCTION(BlueprintPure,Category="GameXXK|Presentation|Test") int32 GetFlightCountForTest() const { return Flights.Num(); }
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
        FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override;
private:
    struct FFlight
    {
        EGameXXKRewardIcon Kind=EGameXXKRewardIcon::Gold;
        FVector2D SourceScreen=FVector2D::ZeroVector, Start=FVector2D::ZeroVector, End=FVector2D::ZeroVector;
        int32 Index=0;
        double QueuedAt=0, StartedAt=0;
        float Age=0;
        bool bStarted=false;
    };
    TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Host;
    TArray<FFlight> Flights;
    TSet<FName> SeenEvents;
    UPROPERTY(Transient) TArray<FSlateBrush> Brushes;
};
