#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPCommandDescriptor.h"

class UGameXXKMVPSubsystem;

struct GAMEXXK_API FGameXXKMVPRouteNodeDescriptor
{
	FGameXXKMVPRouteNodeDescriptor() = default;

	FGameXXKMVPRouteNodeDescriptor(FName InCommandName, const FText& InLabel, EGameXXKNodeKind InNodeKind, int32 InNodeIndex, bool bInEnabled, FVector2D InNormalizedPosition, TArray<int32> InOutgoingNodeIds = {})
		: CommandName(InCommandName)
		, Label(InLabel)
		, NodeKind(InNodeKind)
		, NodeIndex(InNodeIndex)
		, bEnabled(bInEnabled)
		, NormalizedPosition(InNormalizedPosition)
		, OutgoingNodeIds(MoveTemp(InOutgoingNodeIds))
	{
	}

	FName CommandName;
	FText Label;
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Start;
	int32 NodeIndex = INDEX_NONE;
	bool bEnabled = false;
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
	TArray<int32> OutgoingNodeIds;
};

namespace GameXXKMVPCommandRouter
{
	GAMEXXK_API TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands(const UGameXXKMVPSubsystem* Subsystem, const FString& SlotName = TEXT(""), int32 UserIndex = 0);
	GAMEXXK_API TArray<FGameXXKMVPRouteNodeDescriptor> BuildRouteMapNodes(const UGameXXKMVPSubsystem* Subsystem);
	GAMEXXK_API bool ExecuteVisibleCommand(UGameXXKMVPSubsystem* Subsystem, FName CommandName, const FString& SlotName = TEXT(""), int32 UserIndex = 0);
	GAMEXXK_API FText BuildStatusText(const UGameXXKMVPSubsystem* Subsystem);
}
