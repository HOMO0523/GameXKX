#include "UI/GameXXKDungeonMapWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKDungeonMapWidget::OpenFromTownExit()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bOpened = Subsystem && Subsystem->OpenDungeonFromTownExit();
	if (bOpened)
	{
		OnDungeonOpened();
	}
	return bOpened;
}

bool UGameXXKDungeonMapWidget::SelectNode(EGameXXKNodeKind ExpectedNode)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bSelected = Subsystem && Subsystem->SelectDungeonNode(ExpectedNode);
	if (bSelected)
	{
		OnDungeonNodeSelected(ExpectedNode);
	}
	return bSelected;
}
