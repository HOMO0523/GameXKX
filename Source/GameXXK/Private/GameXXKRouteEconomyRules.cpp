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

	void ResetError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}

	uint64 MakeNodeKey(const int32 Chapter, const int32 NodeId)
	{
		return (static_cast<uint64>(static_cast<uint32>(Chapter)) << 32)
			| static_cast<uint32>(NodeId);
	}

	bool IsValidInitializedEconomy(const FGameXXKCardRunState& CardRun)
	{
		if (!CardRun.bRouteEconomyInitialized || CardRun.RouteTravelMoney < 0)
		{
			return false;
		}

		TSet<uint64> SeenNodeKeys;
		SeenNodeKeys.Reserve(CardRun.RewardedTravelMoneyNodes.Num());
		for (const FGameXXKRouteTravelMoneyReceipt& Receipt : CardRun.RewardedTravelMoneyNodes)
		{
			if (Receipt.Chapter < 1
				|| Receipt.Chapter > 3
				|| Receipt.NodeId < 0
				|| Receipt.Amount < 0)
			{
				return false;
			}

			const uint64 Key = MakeNodeKey(Receipt.Chapter, Receipt.NodeId);
			if (SeenNodeKeys.Contains(Key))
			{
				return false;
			}
			SeenNodeKeys.Add(Key);
		}
		return true;
	}
}

bool FGameXXKRouteEconomyRules::InitializeRoute(
	FGameXXKCardRunState& CardRun,
	const int32 StartingBalance,
	FString* OutError)
{
	ResetError(OutError);
	if (StartingBalance < 0)
	{
		return SetFailure(OutError, TEXT("The route starting balance must be non-negative."));
	}
	if (CardRun.bRouteEconomyInitialized)
	{
		return IsValidInitializedEconomy(CardRun)
			? true
			: SetFailure(OutError, TEXT("The initialized route economy state is invalid."));
	}

	FGameXXKCardRunState Candidate = CardRun;
	Candidate.RouteTravelMoney = StartingBalance;
	Candidate.bRouteEconomyInitialized = true;
	Candidate.RewardedTravelMoneyNodes.Reset();
	if (!IsValidInitializedEconomy(Candidate))
	{
		return SetFailure(OutError, TEXT("The route economy could not be initialized."));
	}
	CardRun = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteEconomyRules::AwardNodeOnce(
	FGameXXKCardRunState& CardRun,
	const int32 Chapter,
	const int32 NodeId,
	const int32 Amount,
	bool& OutAwarded,
	FString* OutError)
{
	OutAwarded = false;
	ResetError(OutError);
	if (!CardRun.bRouteEconomyInitialized)
	{
		return SetFailure(OutError, TEXT("The route economy has not been initialized."));
	}
	if (Chapter < 1 || Chapter > 3)
	{
		return SetFailure(OutError, TEXT("The route chapter must be between one and three."));
	}
	if (NodeId < 0)
	{
		return SetFailure(OutError, TEXT("The route node ID must be non-negative."));
	}
	if (Amount < 0)
	{
		return SetFailure(OutError, TEXT("The travel-money award must be non-negative."));
	}
	if (!IsValidInitializedEconomy(CardRun))
	{
		return SetFailure(OutError, TEXT("The initialized route economy state is invalid."));
	}

	for (const FGameXXKRouteTravelMoneyReceipt& Receipt : CardRun.RewardedTravelMoneyNodes)
	{
		if (Receipt.Chapter == Chapter && Receipt.NodeId == NodeId)
		{
			return true;
		}
	}

	const int64 NewBalance = static_cast<int64>(CardRun.RouteTravelMoney) + Amount;
	if (NewBalance > MAX_int32)
	{
		return SetFailure(OutError, TEXT("The travel-money award would overflow the route balance."));
	}

	FGameXXKCardRunState Candidate = CardRun;
	Candidate.RouteTravelMoney = static_cast<int32>(NewBalance);
	FGameXXKRouteTravelMoneyReceipt& Receipt = Candidate.RewardedTravelMoneyNodes.AddDefaulted_GetRef();
	Receipt.Chapter = Chapter;
	Receipt.NodeId = NodeId;
	Receipt.Amount = Amount;
	if (!IsValidInitializedEconomy(Candidate))
	{
		return SetFailure(OutError, TEXT("The travel-money receipt could not be recorded."));
	}

	CardRun = MoveTemp(Candidate);
	OutAwarded = true;
	return true;
}

bool FGameXXKRouteEconomyRules::CanAfford(const FGameXXKCardRunState& CardRun, const int32 Amount)
{
	return Amount >= 0
		&& CardRun.RouteTravelMoney >= 0
		&& Amount <= CardRun.RouteTravelMoney;
}

bool FGameXXKRouteEconomyRules::Spend(
	FGameXXKCardRunState& CardRun,
	const int32 Amount,
	FString* OutError)
{
	ResetError(OutError);
	if (!CanAfford(CardRun, Amount))
	{
		return SetFailure(OutError, TEXT("The travel-money spend is invalid or exceeds the route balance."));
	}

	FGameXXKCardRunState Candidate = CardRun;
	Candidate.RouteTravelMoney -= Amount;
	CardRun = MoveTemp(Candidate);
	return true;
}

void FGameXXKRouteEconomyRules::ClearRouteEconomy(FGameXXKCardRunState& CardRun)
{
	FGameXXKCardRunState Candidate = CardRun;
	Candidate.RouteTravelMoney = 0;
	Candidate.bRouteEconomyInitialized = false;
	Candidate.RewardedTravelMoneyNodes.Reset();
	CardRun = MoveTemp(Candidate);
}
