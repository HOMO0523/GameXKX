#include "UI/GameXXKQuestDialogWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKQuestDialogWidget::AcceptQuest()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bAccepted = Subsystem && Subsystem->AcceptQuest();
	if (bAccepted)
	{
		OnQuestAccepted();
	}
	return bAccepted;
}
