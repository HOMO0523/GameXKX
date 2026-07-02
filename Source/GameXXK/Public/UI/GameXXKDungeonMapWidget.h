#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKDungeonMapWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKDungeonMapWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Dungeon")
	bool OpenFromTownExit();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Dungeon")
	bool SelectNode(EGameXXKNodeKind ExpectedNode);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Dungeon")
	void OnDungeonOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Dungeon")
	void OnDungeonNodeSelected(EGameXXKNodeKind ExpectedNode);
};
