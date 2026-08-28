#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

class UWidget;

using FGameXXKGuideTargetRectResolver = TFunction<bool(FSlateRect&)>;

class GAMEXXK_API FGameXXKGuideTargetRegistry
{
public:
	static FGameXXKGuideTargetRegistry& Get();

	bool RegisterTarget(
		FName TargetId,
		UWidget* Widget,
		FGameXXKGuideTargetRectResolver RectResolver,
		FString* OutError = nullptr);

	void UnregisterTarget(FName TargetId, const UWidget* Widget);
	bool ResolveTargetRect(FName TargetId, FSlateRect& OutRect);
	bool IsTargetRegistered(FName TargetId) const;
	void PruneStaleTargets();
	void Reset();

	static bool IsKnownTargetId(FName TargetId);
	static bool IsKnownTriggerEventId(FName EventId);
	static bool IsKnownCompletionEventId(FName EventId);
	static bool IsKnownActionId(FName ActionId);
	static const TSet<FName>& KnownTargetIds();
	static const TSet<FName>& KnownTriggerEventIds();
	static const TSet<FName>& KnownCompletionEventIds();
	static const TSet<FName>& KnownActionIds();

private:
	struct FEntry
	{
		TWeakObjectPtr<UWidget> Widget;
		FGameXXKGuideTargetRectResolver RectResolver;
	};

	TMap<FName, FEntry> Entries;
};
