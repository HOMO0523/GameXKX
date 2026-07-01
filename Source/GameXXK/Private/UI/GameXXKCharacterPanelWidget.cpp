#include "UI/GameXXKCharacterPanelWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

void UGameXXKCharacterPanelWidget::SetCharacterSummary(const FGameXXKCharacterSummary& InSummary)
{
	Summary = InSummary;
	OnCharacterSummaryChanged();
}

bool UGameXXKCharacterPanelWidget::RefreshPlayerSummary()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	Summary.DisplayName = TEXT("Player");
	Summary.Level = State.PlayerLevel;
	Summary.HP = State.PlayerHP;
	Summary.Attack = State.PlayerAttack;
	Summary.Defense = State.PlayerDefense;
	Summary.Speed = State.PlayerSpeed;
	OnCharacterSummaryChanged();
	return true;
}

const FGameXXKCharacterSummary& UGameXXKCharacterPanelWidget::GetCharacterSummary() const
{
	return Summary;
}
