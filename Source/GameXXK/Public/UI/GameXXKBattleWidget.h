#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKBattleWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKBattleWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ResolveVictory(bool bBossBattle);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool FailToTown();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	TArray<FName> BuildTurnOrder(bool bBossBattle) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Battle")
	void OnBattleResolved(bool bBossBattle);
};
