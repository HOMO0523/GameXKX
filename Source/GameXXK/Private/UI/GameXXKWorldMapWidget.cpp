#include "UI/GameXXKWorldMapWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKWorldMapWidget::TrySelectRegion(FName RegionId)
{
	LastSelectedRegion = RegionId;
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bLastSelectionUnlocked = Subsystem && Subsystem->SelectWorldRegion(RegionId);
	if (bLastSelectionUnlocked)
	{
		OnUnlockedRegionSelected(RegionId);
	}
	else
	{
		OnLockedRegionSelected(RegionId);
	}
	return bLastSelectionUnlocked;
}

FName UGameXXKWorldMapWidget::GetLastSelectedRegion() const
{
	return LastSelectedRegion;
}

bool UGameXXKWorldMapWidget::WasLastSelectionUnlocked() const
{
	return bLastSelectionUnlocked;
}
