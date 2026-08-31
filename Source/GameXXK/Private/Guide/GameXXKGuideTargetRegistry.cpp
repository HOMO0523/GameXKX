#include "Guide/GameXXKGuideTargetRegistry.h"

#include "Components/Widget.h"
#include "Math/TransformCalculus2D.h"

namespace GameXXKGuideTargetRegistryPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	const TSet<FName>& Targets()
	{
		static const TSet<FName> Values = {
			TEXT("Route.Tutorial.NextNode"),
			TEXT("Route.Settlement.Confirm"),
			TEXT("Battle.Hud.PartyQi"),
			TEXT("Battle.Unit.Hero.Health"),
			TEXT("Battle.Unit.Hero.Mana"),
			TEXT("Battle.Enemy.Intent"),
			TEXT("Battle.Hand.HengJianShouShi"),
			TEXT("Battle.Hand.SuiYanJi"),
			TEXT("Battle.Hand.FengShenBu"),
			TEXT("Battle.Unit.Hero.Target"),
			TEXT("Battle.Unit.Enemy.Target"),
			TEXT("Battle.Unit.YueBai.Visual"),
			TEXT("Battle.Pending.ForcedDiscard"),
			TEXT("Battle.AutoBattle"),
			TEXT("Battle.Hand.FirstPlayableTargetedCard"),
			TEXT("Battle.Enemy.FirstLegalTarget"),
			TEXT("Battle.EndTurn"),
			TEXT("Route.Merchant.CardRow"),
			TEXT("Route.Merchant.RelicRow"),
			TEXT("Route.Merchant.Leave"),
			TEXT("Route.Event.ValidChoiceGroup"),
			TEXT("Route.Camp.Heal"),
			TEXT("Route.Camp.Gold"),
			TEXT("Route.Chest.Open"),
			TEXT("Desktop.Settings.ResetCombatGuide")};
		return Values;
	}

	const TSet<FName>& Guides()
	{
		static const TSet<FName> Values = {
			TEXT("Guide.RouteMap.Basic"),
			TEXT("Guide.Battle.Basic"),
			TEXT("Guide.Battle.Tutorial01.NewPlayer"),
			TEXT("Guide.Merchant.Basic"),
			TEXT("Guide.Event.Basic"),
			TEXT("Guide.Camp.Basic"),
			TEXT("Guide.Chest.Basic"),
			TEXT("Guide.Boss.Basic"),
			TEXT("Guide.Settlement.Basic")};
		return Values;
	}

	const TSet<FName>& Triggers()
	{
		static const TSet<FName> Values = {
			TEXT("Event.Route.Opened"),
			TEXT("Event.RouteMap.Opened"),
			TEXT("Event.Battle.Opened"),
			TEXT("Event.Merchant.Opened"),
			TEXT("Event.Route.EventOpened"),
			TEXT("Event.Route.CampOpened"),
			TEXT("Event.Route.ChestOpened"),
			TEXT("Event.Boss.Opened"),
			TEXT("Event.Settlement.Opened")};
		return Values;
	}

	const TSet<FName>& Completions()
	{
		static const TSet<FName> Values = {
			TEXT("Event.Route.NextNodeSelected"),
			TEXT("Event.Battle.TargetedCardSelected"),
			TEXT("Event.Battle.LegalTargetSelected"),
			TEXT("Event.Battle.CardResolved"),
			TEXT("Event.Battle.EndTurnResolved"),
			TEXT("Event.Tutorial01.Continue"),
			TEXT("Event.Tutorial01.HengJianResolved"),
			TEXT("Event.Tutorial01.SuiYanResolved"),
			TEXT("Event.Tutorial01.FengShenForcedDiscardOpened"),
			TEXT("Event.Tutorial01.ForcedDiscardResolved"),
			TEXT("Event.Tutorial01.PlayerTurnReady"),
			TEXT("Event.Tutorial01.AutoBattleEnabled"),
			TEXT("Event.Merchant.CardPurchased"),
			TEXT("Event.Merchant.RelicPurchased"),
			TEXT("Event.Merchant.Left"),
			TEXT("Event.Route.EventChoiceResolved"),
			TEXT("Event.Route.CampHealResolved"),
			TEXT("Event.Route.CampGoldResolved"),
			TEXT("Event.Route.CampResolved"),
			TEXT("Event.Route.ChestRewardResolved"),
			TEXT("Event.Boss.Completed"),
			TEXT("Event.Settlement.Confirmed"),
			TEXT("Event.Guide.Done")};
		return Values;
	}

	const TSet<FName>& Actions()
	{
		static const TSet<FName> Values = {
			TEXT("Action.Route.SelectNext"),
			TEXT("Action.Battle.SelectTargetedCard"),
			TEXT("Action.Guide.Continue"),
			TEXT("Action.Battle.SelectCard.HengJianShouShi"),
			TEXT("Action.Battle.SelectCard.SuiYanJi"),
			TEXT("Action.Battle.SelectCard.FengShenBu"),
			TEXT("Action.Battle.SelectTarget.Hero"),
			TEXT("Action.Battle.SelectTarget.Enemy"),
			TEXT("Action.Battle.SubmitForcedDiscard"),
			TEXT("Action.Battle.EnableAuto"),
			TEXT("Action.Battle.SelectLegalTarget"),
			TEXT("Action.Battle.CommitCard"),
			TEXT("Action.Battle.EndTurn"),
			TEXT("Action.Merchant.PurchaseCard"),
			TEXT("Action.Merchant.PurchaseRelic"),
			TEXT("Action.Merchant.Leave"),
			TEXT("Action.Route.EventChoose"),
			TEXT("Action.Route.CampHeal"),
			TEXT("Action.Route.CampGold"),
			TEXT("Action.Route.ChestOpen"),
			TEXT("Action.Route.SettlementConfirm"),
			TEXT("Action.Desktop.ResetCombatGuide")};
		return Values;
	}
}

FGameXXKGuideTargetRegistry& FGameXXKGuideTargetRegistry::Get()
{
	static FGameXXKGuideTargetRegistry Registry;
	return Registry;
}

bool FGameXXKGuideTargetRegistry::RegisterTarget(
	const FName TargetId,
	UWidget* Widget,
	FGameXXKGuideTargetRectResolver RectResolver,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (!IsKnownTargetId(TargetId) || !IsValid(Widget) || !RectResolver)
	{
		return GameXXKGuideTargetRegistryPrivate::SetError(
			OutError,
			TEXT("Guide target registration requires a known ID, live Widget and resolver."));
	}
	PruneStaleTargets();
	if (FEntry* Existing = Entries.Find(TargetId))
	{
		if (Existing->Widget.Get() != Widget)
		{
			return GameXXKGuideTargetRegistryPrivate::SetError(
				OutError,
				FString::Printf(TEXT("Duplicate live guide target ID: %s"), *TargetId.ToString()));
		}
		Existing->RectResolver = MoveTemp(RectResolver);
		return true;
	}

	FEntry Entry;
	Entry.Widget = Widget;
	Entry.RectResolver = MoveTemp(RectResolver);
	Entries.Add(TargetId, MoveTemp(Entry));
	return true;
}

bool FGameXXKGuideTargetRegistry::RegisterWidgetTarget(
	const FName TargetId,
	UWidget* Widget,
	FString* OutError)
{
	const TWeakObjectPtr<UWidget> WeakWidget(Widget);
	return RegisterTarget(
		TargetId,
		Widget,
		[WeakWidget](const UWidget& HostWidget, FSlateRect& OutRect)
		{
			const UWidget* LiveWidget = WeakWidget.Get();
			if (!LiveWidget
				|| LiveWidget->GetVisibility() == ESlateVisibility::Collapsed
				|| LiveWidget->GetVisibility() == ESlateVisibility::Hidden)
			{
				return false;
			}
			const FGeometry& Geometry = LiveWidget->GetCachedGeometry();
			const FGeometry& HostGeometry = HostWidget.GetCachedGeometry();
			const FVector2D LocalSize = Geometry.GetLocalSize();
			if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f
				|| HostGeometry.GetLocalSize().X <= 0.0f
				|| HostGeometry.GetLocalSize().Y <= 0.0f)
			{
				return false;
			}
			const FSlateRenderTransform TargetToHost = Concatenate(
				Geometry.GetAccumulatedRenderTransform(),
				Inverse(HostGeometry.GetAccumulatedRenderTransform()));
			const FVector2D Minimum = TransformPoint(
				TargetToHost,
				FVector2D::ZeroVector);
			const FVector2D Maximum = TransformPoint(TargetToHost, LocalSize);
			OutRect = FSlateRect(Minimum.X, Minimum.Y, Maximum.X, Maximum.Y);
			return true;
		},
		OutError);
}

bool FGameXXKGuideTargetRegistry::RegisterTarget(
	const FName TargetId,
	UWidget* Widget,
	FGameXXKLegacyGuideTargetRectResolver RectResolver,
	FString* OutError)
{
	return RegisterTarget(
		TargetId,
		Widget,
		[LegacyResolver = MoveTemp(RectResolver)](
			const UWidget& HostWidget,
			FSlateRect& OutRect)
		{
			(void)HostWidget;
			return LegacyResolver && LegacyResolver(OutRect);
		},
		OutError);
}

void FGameXXKGuideTargetRegistry::UnregisterTarget(const FName TargetId, const UWidget* Widget)
{
	if (const FEntry* Existing = Entries.Find(TargetId))
	{
		if (Existing->Widget.Get() == Widget)
		{
			Entries.Remove(TargetId);
		}
	}
}

bool FGameXXKGuideTargetRegistry::ResolveTargetRect(
	const FName TargetId,
	const UWidget& HostWidget,
	FSlateRect& OutRect)
{
	PruneStaleTargets();
	FEntry* Entry = Entries.Find(TargetId);
	if (!Entry || !Entry->RectResolver)
	{
		return false;
	}
	FSlateRect Candidate;
	if (!Entry->RectResolver(HostWidget, Candidate)
		|| !FMath::IsFinite(Candidate.Left)
		|| !FMath::IsFinite(Candidate.Top)
		|| !FMath::IsFinite(Candidate.Right)
		|| !FMath::IsFinite(Candidate.Bottom)
		|| Candidate.Right <= Candidate.Left
		|| Candidate.Bottom <= Candidate.Top)
	{
		return false;
	}
	OutRect = Candidate;
	return true;
}

bool FGameXXKGuideTargetRegistry::ResolveTargetRect(
	const FName TargetId,
	FSlateRect& OutRect)
{
	PruneStaleTargets();
	const FEntry* Entry = Entries.Find(TargetId);
	const UWidget* TargetWidget = Entry ? Entry->Widget.Get() : nullptr;
	return TargetWidget
		&& ResolveTargetRect(TargetId, *TargetWidget, OutRect);
}

bool FGameXXKGuideTargetRegistry::IsTargetRegistered(const FName TargetId) const
{
	const FEntry* Entry = Entries.Find(TargetId);
	return Entry && Entry->Widget.IsValid() && static_cast<bool>(Entry->RectResolver);
}

void FGameXXKGuideTargetRegistry::PruneStaleTargets()
{
	for (auto Iterator = Entries.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Value().Widget.IsValid())
		{
			Iterator.RemoveCurrent();
		}
	}
}

void FGameXXKGuideTargetRegistry::Reset()
{
	Entries.Reset();
}

bool FGameXXKGuideTargetRegistry::EmitEvent(const FName EventId, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (!IsKnownTriggerEventId(EventId) && !IsKnownCompletionEventId(EventId))
	{
		return GameXXKGuideTargetRegistryPrivate::SetError(
			OutError,
			FString::Printf(TEXT("Unknown guide event ID: %s"), *EventId.ToString()));
	}
	GuideEventDelegate.Broadcast(EventId);
	return true;
}

FGameXXKGuideEventDelegate& FGameXXKGuideTargetRegistry::OnGuideEvent()
{
	return GuideEventDelegate;
}

void FGameXXKGuideTargetRegistry::SetActionGate(
	UObject* Owner,
	TFunction<bool(FName)> InGate)
{
	if (IsValid(Owner) && InGate)
	{
		ActionGateOwner = Owner;
		ActionGate = MoveTemp(InGate);
	}
}

void FGameXXKGuideTargetRegistry::ClearActionGate(const UObject* Owner)
{
	if (ActionGateOwner.Get() == Owner)
	{
		ActionGateOwner.Reset();
		ActionGate = nullptr;
	}
}

bool FGameXXKGuideTargetRegistry::IsActionAllowed(const FName ActionId) const
{
	return !ActionGateOwner.IsValid() || !ActionGate || ActionGate(ActionId);
}

bool FGameXXKGuideTargetRegistry::HasActionGate() const
{
	return ActionGateOwner.IsValid() && static_cast<bool>(ActionGate);
}

bool FGameXXKGuideTargetRegistry::IsKnownTargetId(const FName TargetId)
{
	return KnownTargetIds().Contains(TargetId);
}

bool FGameXXKGuideTargetRegistry::IsKnownGuideId(const FName GuideId)
{
	return KnownGuideIds().Contains(GuideId);
}

bool FGameXXKGuideTargetRegistry::IsKnownTriggerEventId(const FName EventId)
{
	return KnownTriggerEventIds().Contains(EventId);
}

bool FGameXXKGuideTargetRegistry::IsKnownCompletionEventId(const FName EventId)
{
	return KnownCompletionEventIds().Contains(EventId);
}

bool FGameXXKGuideTargetRegistry::IsKnownActionId(const FName ActionId)
{
	return KnownActionIds().Contains(ActionId);
}

const TSet<FName>& FGameXXKGuideTargetRegistry::KnownTargetIds()
{
	return GameXXKGuideTargetRegistryPrivate::Targets();
}

const TSet<FName>& FGameXXKGuideTargetRegistry::KnownGuideIds()
{
	return GameXXKGuideTargetRegistryPrivate::Guides();
}

const TSet<FName>& FGameXXKGuideTargetRegistry::KnownTriggerEventIds()
{
	return GameXXKGuideTargetRegistryPrivate::Triggers();
}

const TSet<FName>& FGameXXKGuideTargetRegistry::KnownCompletionEventIds()
{
	return GameXXKGuideTargetRegistryPrivate::Completions();
}

const TSet<FName>& FGameXXKGuideTargetRegistry::KnownActionIds()
{
	return GameXXKGuideTargetRegistryPrivate::Actions();
}
