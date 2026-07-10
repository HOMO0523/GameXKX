// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadSimpleTool.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"

// localization namespace
#define LOCTEXT_NAMESPACE "Quick_RoadSimpleTool"

/*
 * ToolBuilder implementation
 */

UInteractiveTool* UQuick_RoadSimpleToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadSimpleTool* NewTool = NewObject<UQuick_RoadSimpleTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}



/*
 * ToolProperties implementation
 */

UQuick_RoadSimpleToolProperties::UQuick_RoadSimpleToolProperties()
{
	ShowExtendedInfo = true;
}


/*
 * Tool implementation
 */

UQuick_RoadSimpleTool::UQuick_RoadSimpleTool()
{
}


void UQuick_RoadSimpleTool::SetWorld(UWorld* World)
{
	this->TargetWorld = World;
}


void UQuick_RoadSimpleTool::Setup()
{
	USingleClickTool::Setup();

	Properties = NewObject<UQuick_RoadSimpleToolProperties>(this);
	AddToolPropertySource(Properties);
}


void UQuick_RoadSimpleTool::OnClicked(const FInputDeviceRay& ClickPos)
{
	// we will create actor at this position
	FVector NewActorPos = FVector::ZeroVector;

	// cast ray into world to find hit position
	FVector RayStart = ClickPos.WorldRay.Origin;
	FVector RayEnd = ClickPos.WorldRay.PointAt(99999999.f);
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	if (TargetWorld->LineTraceSingleByObjectType(Result, RayStart, RayEnd, QueryParams))
	{
		if (AActor* ClickedActor = Result.GetActor())
		{
			FText ActorInfoMsg;

			if (Properties->ShowExtendedInfo)
			{
				ActorInfoMsg = FText::Format(LOCTEXT("ExtendedActorInfo", "Name: {0}\nClass: {1}"), 
					FText::FromString(ClickedActor->GetName()), 
					FText::FromString(ClickedActor->GetClass()->GetName())
				);
			}
			else
			{
				ActorInfoMsg = FText::Format(LOCTEXT("BasicActorInfo", "Name: {0}"), FText::FromString(Result.GetActor()->GetName()));
			}

			FText Title = LOCTEXT("ActorInfoDialogTitle", "Actor Info");
			// JAH TODO: consider if we can highlight the actor prior to opening the dialog box or make it non-modal
			FMessageDialog::Open(EAppMsgType::Ok, ActorInfoMsg, Title);
		}
	}
}


#undef LOCTEXT_NAMESPACE