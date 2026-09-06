#include "UI/GameXXKInkScrollBar.h"

#include "Components/ScrollBox.h"
#include "Widgets/Layout/SScrollBar.h"

void UGameXXKInkScrollBar::Configure(UScrollBox* InTarget, const float InFixedThumbLength)
{
	Target = InTarget;
	FixedThumbLength = InFixedThumbLength;
	if (!Target) return;
	InitOrientation(Target->GetOrientation());
	SetWidgetStyle(Target->GetWidgetBarStyle());
	SetThickness(Target->GetScrollbarThickness());
	SetPadding(FMargin(0));
	SetAlwaysShowScrollbar(true);
}

TSharedRef<SWidget> UGameXXKInkScrollBar::RebuildWidget()
{
	TSharedRef<SWidget> Widget = Super::RebuildWidget();
	MyScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateUObject(this, &UGameXXKInkScrollBar::ScrollToFraction));
	return Widget;
}

float UGameXXKInkScrollBar::GetContentExtent() const
{
	if (!Target) return 0.0f;
	const FVector2D Size = Target->GetCachedGeometry().GetLocalSize();
	return Target->GetScrollOffsetOfEnd() + (GetOrientation() == Orient_Horizontal ? Size.X : Size.Y);
}

void UGameXXKInkScrollBar::RefreshFromTarget()
{
	const float Extent = GetContentExtent();
	if (!Target || Extent <= 0.0f) return;
	const float Maximum = Target->GetScrollOffsetOfEnd();
	const float Fraction = GetThumbFraction();
	SetState(Maximum > 0.0f ? Target->GetScrollOffset()/Maximum*(1.0f-Fraction) : 0.0f,Fraction);
}

float UGameXXKInkScrollBar::GetThumbFraction() const
{
	if (!Target || Target->GetScrollOffsetOfEnd() <= 0.0f) return 1.0f;
	if (FixedThumbLength > 0.0f)
	{
		const FVector2D Size = GetCachedGeometry().GetLocalSize();
		const float Length = GetOrientation() == Orient_Horizontal ? Size.X : Size.Y;
		return Length > 0.0f ? FMath::Clamp(FixedThumbLength/Length,0.01f,0.95f) : 1.0f;
	}
	return FMath::Clamp(1.0f-Target->GetScrollOffsetOfEnd()/FMath::Max(1.0f,GetContentExtent()),0.0f,1.0f);
}

void UGameXXKInkScrollBar::ScrollToFraction(const float Fraction)
{
	if (!Target) return;
	const float Maximum = Target->GetScrollOffsetOfEnd();
	const float Offset = FMath::Clamp(Fraction/FMath::Max(0.001f,1.0f-GetThumbFraction())*Maximum,0.0f,Maximum);
	Target->SetScrollOffset(Offset);
	Target->OnUserScrolled.Broadcast(Offset);
	RefreshFromTarget();
}
