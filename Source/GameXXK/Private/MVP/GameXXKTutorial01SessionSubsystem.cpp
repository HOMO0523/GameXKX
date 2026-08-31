#include "MVP/GameXXKTutorial01SessionSubsystem.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKPartyFormationRules.h"

namespace GameXXKTutorial01SessionPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}

	void ClearBattleProjection(FGameXXKRuntimeState& InOutState)
	{
		InOutState.bHasActiveBattle = false;
		InOutState.ActiveBattleNodeId = INDEX_NONE;
		InOutState.ActiveBattleEnemies.Reset();
		InOutState.ActiveBattleParty.Reset();
		InOutState.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint();
		InOutState.CardRun.bHasActiveCardBattle = false;
		InOutState.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;
		InOutState.CardRun.ActiveBattle = FGameXXKCardBattleRuntime();
		InOutState.CardRun.EnemyIntents.Reset();
		InOutState.CardRun.NextEnemyIntentIndex = 0;
		InOutState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
		InOutState.CardRun.bActiveBattleRewardResolved = false;
	}

	bool RemoveFirstCardById(
		TArray<FGameXXKCardInstance>& Zone,
		const FName CardId,
		FGameXXKCardInstance& OutInstance)
	{
		const int32 CardIndex = Zone.IndexOfByPredicate(
			[CardId](const FGameXXKCardInstance& Candidate)
			{
				return Candidate.CardId == CardId;
			});
		if (CardIndex == INDEX_NONE)
		{
			return false;
		}
		OutInstance = MoveTemp(Zone[CardIndex]);
		Zone.RemoveAt(CardIndex, 1, EAllowShrinking::No);
		return true;
	}

	int32 CountDeckZones(const FGameXXKBattleDeckState& Deck)
	{
		return Deck.Hand.Num()
			+ Deck.DrawPile.Num()
			+ Deck.DiscardPile.Num()
			+ Deck.ExhaustPile.Num()
			+ Deck.PendingAutomaticHandCards.Num();
	}
}

bool UGameXXKTutorial01SessionSubsystem::BeginFromTown(
	const FGameXXKRuntimeState& RuntimeBeforeTutorial,
	const FTransform& StatueReturnTransform,
	const EGameXXKGuidePreference GuidePreference)
{
	if (Context.bActive || GuidePreference == EGameXXKGuidePreference::Unset)
	{
		return false;
	}

	Context.RuntimeBeforeTutorial = RuntimeBeforeTutorial;
	Context.StatueReturnTransform = StatueReturnTransform;
	Context.GuidePreference = GuidePreference;
	Context.ReturnReason = EGameXXKTutorial01ReturnReason::None;
	Context.bActive = true;
	FGameXXKTutorial01RouteRules::Initialize(Context.RouteState);
	ResetGuideForRetry();
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::BuildRouteRuntime(
	FGameXXKRuntimeState& OutRuntimeState) const
{
	if (!Context.bActive)
	{
		return false;
	}
	OutRuntimeState = Context.RuntimeBeforeTutorial;
	GameXXKTutorial01SessionPrivate::ClearBattleProjection(OutRuntimeState);
	OutRuntimeState.Screen = EGameXXKScreen::DungeonMap;
	OutRuntimeState.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::BuildBattleSeedRuntime(
	FGameXXKRuntimeState& OutRuntimeState,
	FString* OutError)
{
	using namespace GameXXKTutorial01SessionPrivate;
	ClearError(OutError);
	if (!Context.bActive || !Context.RouteState.bBattleInProgress)
	{
		return SetError(
			OutError,
			TEXT("Tutorial battle seed requires an active selected battle node."));
	}
	FGameXXKRuntimeState Candidate = Context.RuntimeBeforeTutorial;
	ClearBattleProjection(Candidate);
	Candidate.Screen = EGameXXKScreen::Town;
	Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, OutError))
	{
		return false;
	}
	const TArray<FName> RequiredTutorialCards = {
		TEXT("Hero.Generic.HengJianShouShi"),
		TEXT("Hero.Generic.SuiYanJi"),
		TEXT("Hero.Generic.FengShenBu"),
	};
	TArray<FName> TutorialHeroLoadout = Candidate.CardRun.HeroSelectedCardIds;
	for (const FName RequiredCardId : RequiredTutorialCards)
	{
		if (TutorialHeroLoadout.Contains(RequiredCardId))
		{
			continue;
		}
		int32 ReplacementIndex = INDEX_NONE;
		for (int32 CardIndex = TutorialHeroLoadout.Num() - 1;
			CardIndex >= 0;
			--CardIndex)
		{
			if (!RequiredTutorialCards.Contains(TutorialHeroLoadout[CardIndex]))
			{
				ReplacementIndex = CardIndex;
				break;
			}
		}
		if (ReplacementIndex == INDEX_NONE)
		{
			return SetError(
				OutError,
				TEXT("Tutorial hero loadout has no replaceable card slot."));
		}
		TutorialHeroLoadout[ReplacementIndex] = RequiredCardId;
	}
	if (!FGameXXKCardBattleAdapter::SetHeroSelectedCards(
			Candidate,
			TutorialHeroLoadout,
			OutError))
	{
		return false;
	}
	if (!FGameXXKPartyFormationRules::SetQuestNpc(
			Candidate,
			TEXT("Npc.YueBai"),
			OutError))
	{
		return false;
	}
	OutRuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::ArrangeDeterministicOpeningHand(
	FGameXXKRuntimeState& InOutRuntimeState,
	FString* OutError) const
{
	using namespace GameXXKTutorial01SessionPrivate;
	ClearError(OutError);
	if (!Context.bActive
		|| !InOutRuntimeState.CardRun.bHasActiveCardBattle
		|| InOutRuntimeState.CardRun.ActiveBattle.Phase
			!= EGameXXKCardBattlePhase::Player)
	{
		return SetError(
			OutError,
			TEXT("Tutorial opening hand requires an active player-phase card battle."));
	}

	FGameXXKRuntimeState Candidate = InOutRuntimeState;
	FGameXXKBattleDeckState& Deck = Candidate.CardRun.ActiveBattle.Deck;
	if (Deck.HandLimit < 3)
	{
		return SetError(OutError, TEXT("Tutorial opening hand requires at least three slots."));
	}
	const TArray<FName> RequiredCardIds = {
		TEXT("Hero.Generic.HengJianShouShi"),
		TEXT("Hero.Generic.SuiYanJi"),
		TEXT("Hero.Generic.FengShenBu"),
	};
	const TArray<FName> ActiveInstanceIdsBefore = Deck.ActiveInstanceIds;
	const int32 ZoneCountBefore = CountDeckZones(Deck);
	TArray<FGameXXKCardInstance> RequiredInstances;
	RequiredInstances.Reserve(RequiredCardIds.Num());
	for (const FName CardId : RequiredCardIds)
	{
		FGameXXKCardInstance Found;
		if ((!RemoveFirstCardById(Deck.Hand, CardId, Found)
				&& !RemoveFirstCardById(Deck.DrawPile, CardId, Found))
			|| Found.OwnerUnitId != FGameXXKEquipmentRules::HeroCharacterId())
		{
			return SetError(
				OutError,
				FString::Printf(
					TEXT("Tutorial opening card is missing or not hero-owned: %s"),
					*CardId.ToString()));
		}
		RequiredInstances.Add(MoveTemp(Found));
	}

	while (Deck.Hand.Num() > Deck.HandLimit - RequiredInstances.Num())
	{
		Deck.DrawPile.Insert(Deck.Hand.Pop(EAllowShrinking::No), 0);
	}
	for (int32 RequiredIndex = RequiredInstances.Num() - 1;
		RequiredIndex >= 0;
		--RequiredIndex)
	{
		Deck.Hand.Insert(MoveTemp(RequiredInstances[RequiredIndex]), 0);
	}
	if (Deck.ActiveInstanceIds != ActiveInstanceIdsBefore
		|| CountDeckZones(Deck) != ZoneCountBefore
		|| !GameXXKCardRules::ValidateCardBattleRuntime(
			Candidate.CardRun.ActiveBattle,
			OutError))
	{
		return SetError(
			OutError,
			OutError && !OutError->IsEmpty()
				? *OutError
				: TEXT("Tutorial opening hand violated the battle deck ledger."));
	}
	InOutRuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::MarkBattleVictory(
	FGameXXKRuntimeState& OutRouteRuntime)
{
	if (!Context.bActive
		|| !FGameXXKTutorial01RouteRules::MarkVictory(Context.RouteState))
	{
		return false;
	}
	return BuildRouteRuntime(OutRouteRuntime);
}

bool UGameXXKTutorial01SessionSubsystem::MarkBattleDefeat()
{
	if (!Context.bActive || !Context.RouteState.bBattleInProgress)
	{
		return false;
	}
	FGameXXKTutorial01RouteRules::MarkBattleAborted(Context.RouteState);
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::RequestRouteNode(
	const int32 NodeId,
	EGameXXKTutorial01RouteAction& OutAction)
{
	return Context.bActive
		&& FGameXXKTutorial01RouteRules::RequestNode(
			Context.RouteState,
			NodeId,
			OutAction);
}

TArray<FGameXXKRouteMapNode>
UGameXXKTutorial01SessionSubsystem::BuildRouteNodes() const
{
	return Context.bActive
		? FGameXXKTutorial01RouteRules::BuildNodes(Context.RouteState)
		: TArray<FGameXXKRouteMapNode>();
}

TArray<FGameXXKRouteMapEdge>
UGameXXKTutorial01SessionSubsystem::BuildRouteEdges() const
{
	return Context.bActive
		? FGameXXKTutorial01RouteRules::BuildEdges()
		: TArray<FGameXXKRouteMapEdge>();
}

TMap<int32, FText>
UGameXXKTutorial01SessionSubsystem::BuildRouteLabels() const
{
	return Context.bActive
		? FGameXXKTutorial01RouteRules::BuildLabels()
		: TMap<int32, FText>();
}

FText UGameXXKTutorial01SessionSubsystem::BuildRouteCompletionNotice() const
{
	return Context.bActive
		? FGameXXKTutorial01RouteRules::BuildCompletionNotice(Context.RouteState)
		: FText::GetEmpty();
}

FGameXXKGuideProgress&
UGameXXKTutorial01SessionSubsystem::GetMutableGuideProgress()
{
	return Context.TutorialGuideProgress;
}

void UGameXXKTutorial01SessionSubsystem::ResetGuideForRetry()
{
	Context.TutorialGuideProgress = FGameXXKGuideProgress();
	Context.TutorialGuideProgress.Preference = Context.GuidePreference;
}

bool UGameXXKTutorial01SessionSubsystem::PrepareRetry(
	FGameXXKRuntimeState& OutRuntimeState)
{
	if (!Context.bActive)
	{
		return false;
	}
	OutRuntimeState = Context.RuntimeBeforeTutorial;
	FGameXXKTutorial01RouteRules::MarkBattleAborted(Context.RouteState);
	ResetGuideForRetry();
	Context.ReturnReason = EGameXXKTutorial01ReturnReason::None;
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::RestoreForTownReturn(
	const EGameXXKTutorial01ReturnReason ReturnReason,
	FGameXXKRuntimeState& OutRuntimeState)
{
	if (!Context.bActive || ReturnReason == EGameXXKTutorial01ReturnReason::None)
	{
		return false;
	}
	OutRuntimeState = Context.RuntimeBeforeTutorial;
	Context.ReturnReason = ReturnReason;
	return true;
}

bool UGameXXKTutorial01SessionSubsystem::ConsumeTownReturn(
	FGameXXKTutorial01ReturnContext& OutContext)
{
	if (!Context.bActive
		|| Context.ReturnReason == EGameXXKTutorial01ReturnReason::None)
	{
		return false;
	}
	OutContext = MoveTemp(Context);
	Context = FGameXXKTutorial01ReturnContext();
	return true;
}

void UGameXXKTutorial01SessionSubsystem::CancelSession()
{
	Context = FGameXXKTutorial01ReturnContext();
}

EGameXXKGuidePreference
UGameXXKTutorial01SessionSubsystem::GetGuidePreference() const
{
	return Context.bActive
		? Context.GuidePreference
		: EGameXXKGuidePreference::Unset;
}
