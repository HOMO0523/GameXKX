#pragma once
#include "Guide/GameXXKAcademyRules.h"
#include "GameXXKMVPRules.h"
namespace GameXXKAcademyStateBuilder
{
	bool BuildLoadout(const FGameXXKAcademyCourse& Course,int32 LessonIndex,FGameXXKRuntimeState& State,FName& Focus,FString& Error);
	bool ConfigureBattle(const FGameXXKAcademyCourse& Course,int32 LessonIndex,FGameXXKRuntimeState& State,FName Focus,FString& Error);
}
