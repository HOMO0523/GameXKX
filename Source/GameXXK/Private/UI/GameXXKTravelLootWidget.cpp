#include "UI/GameXXKTravelLootWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace GameXXKTravelLoot
{
	FVector2D Source(int32 Slot) { return FVector2D(858.0f - FMath::Clamp(Slot, 0, 2)*125.0f, 116.0f); }
	FVector2D Target(EGameXXKTrainingRewardTier Tier)
	{
		switch (Tier)
		{
		case EGameXXKTrainingRewardTier::NormalChest: return FVector2D(989, 82);
		case EGameXXKTrainingRewardTier::AdvancedChest: return FVector2D(989, 158);
		case EGameXXKTrainingRewardTier::HuntChest: return FVector2D(1069, 120);
		// Existing backpack Tab in the strip's bottom summary row.
		default: return FVector2D(989, 214);
		}
	}
	float Arrival(bool Chest, int32 Index) { return Chest ? 1.08f : 0.78f + FMath::Clamp(Index,0,4)*0.035f; }
	float Pulse(float Age)
	{
		return Age < 0 || Age > 0.30f ? 0 : FMath::Sin(PI*Age/0.30f)*(1-Age/0.30f);
	}
	FGameXXKTravelLootSample Sample(const FGameXXKTravelLootBurst& Burst, int32 Index, bool Chest)
	{
		FGameXXKTravelLootSample S;
		const float Delay = Chest ? 0.06f : Index*0.035f;
		const float Age = Burst.Age-Delay;
		const float Duration = Chest ? 1.02f : 0.78f;
		const FVector2D Start = Source(Burst.EnemySlotIndex);
		const FVector2D End = Target(Chest ? Burst.Reward.ChestTier : EGameXXKTrainingRewardTier::None);
		S.Position = Age >= Duration ? End : Start;
		if (Age < 0 || Age >= Duration) return S;
		// Private visual seed: never consumes the loot/equipment random stream.
		FRandomStream Random(static_cast<int32>(Burst.Ordinal*97 + Index*31 + (Chest ? 613 : 17)));
		const float Spread = Chest ? Random.FRandRange(-18,18) : (Index-2)*18.0f + Random.FRandRange(-7,7);
		const float Height = Chest ? 56 : Random.FRandRange(30,52);
		const FVector2D Scatter = Start + FVector2D(Spread, -Height*0.55f);
		const float ScatterTime = Chest ? 0.44f : 0.28f;
		if (Age < ScatterTime)
		{
			const float T = Age/ScatterTime;
			S.Position = FMath::Lerp(Start,Scatter,T) - FVector2D(0,Height*0.6f*FMath::Sin(PI*T));
			S.Scale = FMath::Lerp(0.65f,1.0f,FMath::Min(1.0f,T*3));
		}
		else
		{
			const float T = (Age-ScatterTime)/(Duration-ScatterTime);
			const float U = T*T;
			const FVector2D Control = (Scatter+End)*0.5f + FVector2D(Chest ? 18 : -20,-28);
			S.Position = Scatter*FMath::Square(1-U) + Control*(2*(1-U)*U) + End*(U*U);
			S.Scale = 1.0f - 0.82f*FMath::Pow(T,4);
		}
		S.Angle = Chest ? FMath::Sin(Age*9)*0.12f : Spread*0.014f + Age*(Index%2 ? 4.0f : -4.0f);
		S.Opacity = FMath::Min(1.0f,Age*30);
		return S;
	}
}

namespace
{
	class SGameXXKTravelLoot final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGameXXKTravelLoot) {} SLATE_END_ARGS()
		void Construct(const FArguments&, UGameXXKTravelLootWidget* InOwner) { Owner=InOwner; }
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1118,226); }
		virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& E,int32 Layer,const FWidgetStyle& Style,bool) const override
		{
			if (!Owner.IsValid() || !Owner->GetHost()) return Layer;
			const auto* Host=Owner->GetHost();
			const auto& Bursts=Host->GetTravelPresentation().GetLootBursts();
			if (Bursts.IsEmpty()) return Layer;
			const FLinearColor Tint=Style.GetColorAndOpacityTint();
			auto Icon=[&](int32 Brush,FVector2D Pos,float Size,float Angle,float Alpha)
			{
				FSlateDrawElement::MakeRotatedBox(E,++Layer,G.ToPaintGeometry(FVector2D(Size,Size),FSlateLayoutTransform(Pos-FVector2D(Size,Size)*0.5f)),
					&Owner->GetLootBrush(Brush),ESlateDrawEffect::None,Angle,TOptional<FVector2f>(),FSlateDrawElement::RelativeToElement,Tint*FLinearColor(1,1,1,Alpha));
			};
			auto Line=[&](const TArray<FVector2f>& Points,float Alpha,float Width)
			{
				FSlateDrawElement::MakeLines(E,++Layer,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Tint*FLinearColor(1,0.76f,0.22f,Alpha),true,Width);
			};
			for (const auto& B:Bursts)
			{
				const int32 Count=B.Reward.Gold>0 ? 5 : 0;
				for(int32 I=0;I<Count+(B.Reward.bChestRolled ? 1 : 0);++I)
				{
					const bool Chest=I==Count;
					const int32 Index=Chest ? 0 : I;
					const auto S=GameXXKTravelLoot::Sample(B,Index,Chest);
					const int32 Brush=Chest ? (B.Reward.ChestTier==EGameXXKTrainingRewardTier::HuntChest ? 3 : B.Reward.ChestTier==EGameXXKTrainingRewardTier::AdvancedChest ? 2 : 1) : 0;
					if(S.Opacity>0)
					{
						if(B.Age>0.46f)
						{
							auto Previous=B;Previous.Age-=0.025f;
							Line({FVector2f(GameXXKTravelLoot::Sample(Previous,Index,Chest).Position),FVector2f(S.Position)},0.3f*S.Opacity,Chest ? 3 : 1.5f);
						}
						Icon(Brush,S.Position,(Chest ? 80 : 44)*S.Scale,S.Angle,S.Opacity);
					}
					// A few sparks converge on collection, kept inside the strip.
					const float A=B.Age-GameXXKTravelLoot::Arrival(Chest,Index);
					if(Chest && A>=0 && A<0.22f)
					{
						const auto Target=GameXXKTravelLoot::Target(Chest ? B.Reward.ChestTier : EGameXXKTrainingRewardTier::None);
						for(int32 J=0;J<6;++J)
						{
							const float Angle=PI*2*J/6;
							const FVector2D Dir(FMath::Cos(Angle),FMath::Sin(Angle));
							const float Radius=12*(1-A/0.22f);
							Line({FVector2f(Target+Dir*Radius),FVector2f(Target+Dir*(Radius+3))},1-A/0.22f,1.8f);
						}
					}
				}
			}
			return Layer;
		}
	private:
		TWeakObjectPtr<UGameXXKTravelLootWidget> Owner;
	};
}

void UGameXXKTravelLootWidget::Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner,const TArray<FSlateBrush>& InBrushes)
{
	Owner=InOwner;Brushes=InBrushes;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	ForceVolatile(true);
}
TSharedRef<SWidget> UGameXXKTravelLootWidget::RebuildWidget() { return SNew(SGameXXKTravelLoot,this); }
