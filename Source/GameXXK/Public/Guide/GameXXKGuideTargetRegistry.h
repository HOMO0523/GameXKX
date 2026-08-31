#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

class UWidget;

using FGameXXKGuideTargetRectResolver =
	TFunction<bool(const UWidget& HostWidget, FSlateRect& OutLocalRect)>;
using FGameXXKLegacyGuideTargetRectResolver = TFunction<bool(FSlateRect&)>;
DECLARE_MULTICAST_DELEGATE_OneParam(FGameXXKGuideEventDelegate, FName);

class GAMEXXK_API FGameXXKGuideTargetRegistry
{
public:
	static FGameXXKGuideTargetRegistry& Get();

	bool RegisterTarget(
		FName TargetId,
		UWidget* Widget,
		FGameXXKGuideTargetRectResolver RectResolver,
		FString* OutError = nullptr);
	bool RegisterTarget(
		FName TargetId,
		UWidget* Widget,
		FGameXXKLegacyGuideTargetRectResolver RectResolver,
		FString* OutError = nullptr);
	bool RegisterWidgetTarget(FName TargetId, UWidget* Widget, FString* OutError = nullptr);

	void UnregisterTarget(FName TargetId, const UWidget* Widget);
	bool ResolveTargetRect(FName TargetId, const UWidget& HostWidget, FSlateRect& OutLocalRect);
	bool ResolveTargetRect(FName TargetId, FSlateRect& OutRect);
	bool IsTargetRegistered(FName TargetId) const;
	void PruneStaleTargets();
	void Reset();
	bool EmitEvent(FName EventId, FString* OutError = nullptr);
	FGameXXKGuideEventDelegate& OnGuideEvent();
	void SetActionGate(UObject* Owner, TFunction<bool(FName)> InGate);
	void ClearActionGate(const UObject* Owner);
	bool IsActionAllowed(FName ActionId) const;
	bool HasActionGate() const;

	static bool IsKnownTargetId(FName TargetId);
	static bool IsKnownGuideId(FName GuideId);
	static bool IsKnownTriggerEventId(FName EventId);
	static bool IsKnownCompletionEventId(FName EventId);
	static bool IsKnownActionId(FName ActionId);
	static const TSet<FName>& KnownTargetIds();
	static const TSet<FName>& KnownGuideIds();
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
	FGameXXKGuideEventDelegate GuideEventDelegate;
	TWeakObjectPtr<UObject> ActionGateOwner;
	TFunction<bool(FName)> ActionGate;
};
