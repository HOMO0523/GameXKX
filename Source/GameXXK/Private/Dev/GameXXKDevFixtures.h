#pragma once
#include "GameXXKMVPRules.h"

/** Dev-only configurations. Every generated item/deck passes the normal rules. */
namespace GameXXKDevFixtures
{
 bool SetLevel(FGameXXKRuntimeState& State, FName Character, int32 Level, FString& Error);
 EGameXXKEquipmentSet RecommendedSet(EGameXXKCharacterRole Role);
 EGameXXKEquipmentSet NpcRecommendedSet(FName Npc);
 bool RecommendAll(FGameXXKRuntimeState& State, int32 Level, EGameXXKEquipmentSet HeroSet, TArray<FName>& Created, FString& Error);
 bool BuildBenchmark(EGameXXKCharacterRole Role, FName Npc, const FString& HeroDirection, int32 NpcOmit, FGameXXKRuntimeState& Out, FString& Error);
}
