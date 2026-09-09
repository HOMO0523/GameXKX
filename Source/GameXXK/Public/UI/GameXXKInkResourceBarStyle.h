#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
class UMaterialInstanceDynamic;
class UProgressBar;

namespace GameXXKInkResourceBarStyle
{
	inline constexpr const TCHAR* MaterialPath=TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/M_InkResourceBar.M_InkResourceBar");
	inline constexpr const TCHAR* TexturePath=TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/T_InkResourceBarMaster.T_InkResourceBarMaster");
	GAMEXXK_API UMaterialInstanceDynamic* Create(UObject* Outer,const FVector2D& Size,bool bMana=false);
	GAMEXXK_API FSlateBrush Brush(UMaterialInstanceDynamic* Material,const FVector2D& Size);
	GAMEXXK_API void Update(UMaterialInstanceDynamic* Material,float Percent);
	GAMEXXK_API void Apply(UProgressBar* Bar,const FVector2D& Size,bool bMana=false);
	GAMEXXK_API void Update(UProgressBar* Bar,float Percent);
}
