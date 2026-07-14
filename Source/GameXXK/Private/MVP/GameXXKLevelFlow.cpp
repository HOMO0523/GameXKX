#include "MVP/GameXXKLevelFlow.h"

#include "GameXXKMVPRules.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FName MainMap(TEXT("/Game/GameXXK/Maps/L_Main"));
	const FName QingshanTownMap(TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"));
	const FName LegacyQingshanTownMap(TEXT("/Game/GameXXK/Maps/L_QingshanInn"));
	const FName RouteMap(TEXT("/Game/GameXXK/Maps/L_RouteMap"));
	const FName RouteEventMap(TEXT("/Game/GameXXK/Maps/L_RouteEvent"));
	const FName RouteCampMap(TEXT("/Game/GameXXK/Maps/L_RouteCamp"));
	const FName RouteMerchantMap(TEXT("/Game/GameXXK/Maps/L_RouteMerchant"));
	const FName BattleSceneMap(TEXT("/Game/GameXXK/Maps/L_BattleScene"));

	FString StripPIEPrefix(FString ShortMapName)
	{
		static const FString PIEPrefix(TEXT("UEDPIE_"));
		if (!ShortMapName.StartsWith(PIEPrefix))
		{
			return ShortMapName;
		}

		const int32 SecondUnderscoreIndex = ShortMapName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, PIEPrefix.Len());
		if (SecondUnderscoreIndex == INDEX_NONE)
		{
			return ShortMapName;
		}
		return ShortMapName.RightChop(SecondUnderscoreIndex + 1);
	}

	FString ShortMapNameForPackage(const FString& PackageName)
	{
		return StripPIEPrefix(FPackageName::GetShortName(PackageName));
	}

	bool IsCurrentMap(const UWorld* World, FName TargetMap)
	{
		if (!World || TargetMap.IsNone())
		{
			return false;
		}

		const FString CurrentPackageName = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		return GameXXKLevelFlow::MapPackageMatches(CurrentPackageName, TargetMap);
	}
}

FName GameXXKLevelFlow::MapForScreen(EGameXXKScreen Screen)
{
	switch (Screen)
	{
	case EGameXXKScreen::Town:
		return QingshanTownMap;
	case EGameXXKScreen::DungeonMap:
		return RouteMap;
	case EGameXXKScreen::RouteEvent:
		return RouteEventMap;
	case EGameXXKScreen::RouteCamp:
		return RouteCampMap;
	case EGameXXKScreen::RouteMerchant:
		return RouteMerchantMap;
	case EGameXXKScreen::Battle:
		return BattleSceneMap;
	case EGameXXKScreen::MainMenu:
	case EGameXXKScreen::WorldMap:
	default:
		return MainMap;
	}
}

FName GameXXKLevelFlow::MapForRuntimeState(const FGameXXKRuntimeState& State)
{
	return MapForScreen(State.Screen);
}

bool GameXXKLevelFlow::MapPackageMatches(const FString& CurrentPackageName, FName TargetMap)
{
	if (CurrentPackageName.IsEmpty() || TargetMap.IsNone())
	{
		return false;
	}

	const FString TargetPackageName = TargetMap.ToString();
	return CurrentPackageName == TargetPackageName
		|| ShortMapNameForPackage(CurrentPackageName) == ShortMapNameForPackage(TargetPackageName);
}

bool GameXXKLevelFlow::IsTownGameplayMapPackage(const FString& CurrentPackageName)
{
	return MapPackageMatches(CurrentPackageName, QingshanTownMap)
		|| MapPackageMatches(CurrentPackageName, LegacyQingshanTownMap);
}

bool GameXXKLevelFlow::OpenMapForRuntimeState(UGameXXKMVPSubsystem* Subsystem)
{
	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	const FName TargetMap = MapForRuntimeState(Subsystem->GetRuntimeState());
	if (TargetMap.IsNone() || IsCurrentMap(World, TargetMap))
	{
		return false;
	}

	UGameplayStatics::OpenLevel(World, TargetMap);
	return true;
}
