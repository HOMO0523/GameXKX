#include "GameXXKRouteSettlementRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRouteEconomyRules.h"

namespace
{
	bool SetFailure(FString* OutError, const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	bool MatchesReceipt(const FGameXXKRouteSettlementReceipt& Left, const FGameXXKRouteSettlementReceipt& Right)
	{
		return Left.SettlementId == Right.SettlementId
			&& Left.Outcome == Right.Outcome
			&& Left.SourceTravelMoney == Right.SourceTravelMoney
			&& Left.SourceCardAcquisitionCount == Right.SourceCardAcquisitionCount
			&& Left.PermanentGoldAward == Right.PermanentGoldAward
			&& Left.EnhancementStoneAward == Right.EnhancementStoneAward;
	}

	bool HasValidAwardFormula(const FGameXXKRouteSettlementReceipt& Receipt)
	{
		if (!Receipt.SettlementId.IsValid()
			|| Receipt.SourceTravelMoney < 0
			|| Receipt.SourceCardAcquisitionCount < 0
			|| Receipt.PermanentGoldAward < 0
			|| Receipt.EnhancementStoneAward < 0)
		{
			return false;
		}
		const int32 MoneyDivisor = Receipt.Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 10 : 20;
		const int32 CardDivisor = Receipt.Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 5 : 10;
		return Receipt.PermanentGoldAward == Receipt.SourceTravelMoney / MoneyDivisor
			&& Receipt.EnhancementStoneAward == Receipt.SourceCardAcquisitionCount / CardDivisor;
	}

	bool HasValidInitializedRouteEconomy(const FGameXXKRuntimeState& State)
	{
		if (!State.CardRun.bRouteEconomyInitialized)
		{
			return false;
		}
		FGameXXKCardRunState ValidationCandidate = State.CardRun;
		return FGameXXKRouteEconomyRules::InitializeRoute(
			ValidationCandidate,
			State.CardRun.RouteTravelMoney);
	}

	void ClearSettledRouteLocalState(FGameXXKRuntimeState& InOutState)
	{
		FGameXXKCardBattleAdapter::ClearRouteLocalCardState(InOutState);
		InOutState.CardRun.RouteProgress = FGameXXKRouteProgress();
		FGameXXKRouteEconomyRules::ClearRouteEconomy(InOutState.CardRun);
		InOutState.CardRun.PendingSettlement = FGameXXKRouteSettlementReceipt();
	}
}

bool FGameXXKRouteSettlementRules::Preview(
	const FGameXXKRuntimeState& State,
	const EGameXXKRouteTerminalOutcome Outcome,
	FGameXXKRouteSettlementReceipt& OutReceipt,
	FString* OutError)
{
	OutReceipt = FGameXXKRouteSettlementReceipt();
	if (!State.bDungeonActive
		|| !HasValidInitializedRouteEconomy(State)
		|| State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount < 0)
	{
		return SetFailure(OutError, TEXT("A terminal route receipt requires non-negative resources from an active route."));
	}

	const int32 MoneyDivisor = Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 10 : 20;
	const int32 CardDivisor = Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 5 : 10;
	OutReceipt.SettlementId = FGuid::NewGuid();
	OutReceipt.Outcome = Outcome;
	OutReceipt.SourceTravelMoney = State.CardRun.RouteTravelMoney;
	OutReceipt.SourceCardAcquisitionCount = State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	OutReceipt.PermanentGoldAward = OutReceipt.SourceTravelMoney / MoneyDivisor;
	OutReceipt.EnhancementStoneAward = OutReceipt.SourceCardAcquisitionCount / CardDivisor;
	return true;
}

bool FGameXXKRouteSettlementRules::Apply(
	FGameXXKRuntimeState& State,
	const FGameXXKRouteSettlementReceipt& Receipt,
	FString* OutError)
{
	if (!HasValidAwardFormula(Receipt))
	{
		return SetFailure(OutError, TEXT("The route settlement receipt is malformed."));
	}

	if (State.CardRun.LastAppliedRouteSettlementId == Receipt.SettlementId)
	{
		// A replay from an already-clean state (including a later route that
		// retained this idempotency key) is a pure no-op. Only finish cleanup for
		// the narrow crash-recovery snapshot whose pending receipt and sources
		// still exactly match the already-applied receipt.
		if (MatchesReceipt(State.CardRun.PendingSettlement, Receipt)
			&& State.CardRun.RouteTravelMoney == Receipt.SourceTravelMoney
			&& State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount == Receipt.SourceCardAcquisitionCount)
		{
			FGameXXKRuntimeState Candidate = State;
			ClearSettledRouteLocalState(Candidate);
			State = MoveTemp(Candidate);
		}
		return true;
	}

	if (!State.bDungeonActive
		|| !HasValidInitializedRouteEconomy(State)
		|| !MatchesReceipt(State.CardRun.PendingSettlement, Receipt)
		|| State.CardRun.RouteTravelMoney != Receipt.SourceTravelMoney
		|| State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount != Receipt.SourceCardAcquisitionCount)
	{
		return SetFailure(OutError, TEXT("The settlement receipt does not match the active route snapshot."));
	}

	const int64 NewGold = static_cast<int64>(State.PlayerGold) + Receipt.PermanentGoldAward;
	if (NewGold < 0 || NewGold > MAX_int32)
	{
		return SetFailure(OutError, TEXT("The permanent currency award would overflow."));
	}

	FGameXXKRuntimeState Candidate = State;
	Candidate.PlayerGold = static_cast<int32>(NewGold);
	if (Receipt.EnhancementStoneAward > 0
		&& !UGameXXKMVPRules::AddItem(Candidate, UGameXXKMVPRules::ItemEnhancementStone(), Receipt.EnhancementStoneAward))
	{
		return SetFailure(OutError, TEXT("The enhancement-stone award cannot be applied."));
	}
	Candidate.CardRun.LastAppliedRouteSettlementId = Receipt.SettlementId;
	ClearSettledRouteLocalState(Candidate);
	State = MoveTemp(Candidate);
	return true;
}
