#include "UI/GameXXKBattleWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKBattleWidget::ResolveVictory(bool bBossBattle)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bResolved = Subsystem && Subsystem->ResolveBattleVictory(bBossBattle);
	if (bResolved)
	{
		OnBattleResolved(bBossBattle);
	}
	return bResolved;
}

bool UGameXXKBattleWidget::FailToTown()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->FailDungeonToTown();
}

TArray<FName> UGameXXKBattleWidget::BuildTurnOrder(bool bBossBattle) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->BuildTurnOrder(bBossBattle) : TArray<FName>();
}
