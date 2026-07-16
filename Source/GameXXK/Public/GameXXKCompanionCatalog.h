#pragma once

#include "CoreMinimal.h"
#include "GameXXKCompanionTypes.h"

/** Read-only identity catalog for deterministic permanent-companion recruitment. */
class GAMEXXK_API FGameXXKCompanionCatalog final
{
public:
	static const TArray<FGameXXKCompanionTemplateDefinition>& GetRecruitTemplates();
	static const FGameXXKCompanionTemplateDefinition* FindRecruitTemplate(FName TemplateId);
	static const TArray<FGameXXKQuestNpcDefinition>& GetQuestNpcDefinitions();
	static const FGameXXKQuestNpcDefinition* FindQuestNpcDefinition(FName NpcId);
};
