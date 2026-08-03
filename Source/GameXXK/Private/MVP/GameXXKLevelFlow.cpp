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
	const FName RouteCampMap(TEXT("/Game/GameXXK/Maps/L_RouteCamp"));

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
		// Events and chests are modal choices over the route map.  Travelling to
		// L_RouteEvent left the player outside the route HUD and made a pending
		// choice appear to be stuck.
		return RouteMap;
	case EGameXXKScreen::RouteCamp:
		return RouteCampMap;
	case EGameXXKScreen::RouteMerchant:
		// The merchant is a modal HUD over the live route map, just like events
		// and chests.
		return RouteMap;
	case EGameXXKScreen::Battle:
		return RouteMap;
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

bool GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
	const FString& CurrentPackageName,
	const FGameXXKRuntimeState& State)
{
	const FName TargetMap = MapForRuntimeState(State);
	return !TargetMap.IsNone() && !MapPackageMatches(CurrentPackageName, TargetMap);
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

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FString CurrentPackageName = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
	if (!RequiresMapLoadForRuntimeState(CurrentPackageName, State))
	{
		return false;
	}

	const FName TargetMap = MapForRuntimeState(State);
	UGameplayStatics::OpenLevel(World, TargetMap);
	return true;
}
