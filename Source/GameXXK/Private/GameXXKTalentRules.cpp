#include "GameXXKTalentRules.h"

#include "GameXXKTalentCatalog.h"
#include "GameXXKMVPRules.h"

namespace
{
	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	bool FailPurchase(FGameXXKTalentPurchaseResult& OutResult, const FString& Error)
	{
		OutResult.bPurchased = false;
		OutResult.Message = FText::FromString(Error);
		return false;
	}

	int32 RankOf(const FGameXXKTalentProgress& Progress, const FName NodeId)
	{
		return FMath::Max(0, Progress.NodeRanks.FindRef(NodeId));
	}

	int32 ComputeRawBackpackCapacity(const FGameXXKTalentProgress& Progress)
	{
		int32 Capacity = 20;
		for (const FGameXXKTalentNodeDefinition& Node : FGameXXKTalentCatalog::GetDefinitions())
		{
			if (Node.Effect == EGameXXKTalentEffect::BackpackSlots)
			{
				Capacity += RankOf(Progress, Node.Id) * Node.EffectPerRank;
			}
		}
		return FMath::Clamp(FMath::Max(Capacity, Progress.MinimumBackpackCapacity), 20, 200);
	}

	FText LockReasonFor(const FGameXXKTalentProgress& Progress, const FGameXXKTalentNodeDefinition& Node)
	{
		if (Node.RequiredBackpackCapacity > 0
			&& ComputeRawBackpackCapacity(Progress) < Node.RequiredBackpackCapacity)
		{
			return FText::FromString(FString::Printf(
				TEXT("背包总容量达到 %d 格后解锁"),
				Node.RequiredBackpackCapacity));
		}
		for (const FName PrerequisiteId : Node.PrerequisiteIds)
		{
			if (RankOf(Progress, PrerequisiteId) <= 0)
			{
				const FGameXXKTalentNodeDefinition* Prerequisite = FGameXXKTalentCatalog::Find(PrerequisiteId);
				return FText::FromString(FString::Printf(
					TEXT("需要先点亮：%s"),
					Prerequisite ? *Prerequisite->DisplayName.ToString() : *PrerequisiteId.ToString()));
			}
		}
		return FText::GetEmpty();
	}
}

int64 FGameXXKTalentRules::GetPriceForCostTier(const int32 CostTier)
{
	const int32 SafeTier = FMath::Clamp(CostTier, 0, MaximumCostTier);
	const double Raw = 2500.0 * FMath::Pow(1.35, static_cast<double>(SafeTier));
	return static_cast<int64>(FMath::RoundToDouble(Raw / 100.0) * 100.0);
}

int64 FGameXXKTalentRules::GetRankPrice(const FGameXXKTalentNodeDefinition& Node, const int32 CurrentRank)
{
	return GetPriceForCostTier(Node.CostTier);
}

int64 FGameXXKTalentRules::GetFullCapacityPathPrice()
{
	int64 Total = 0;
	for (int32 CostTier = 1; CostTier <= MaximumCostTier; ++CostTier)
	{
		const int64 RankPrice = GetPriceForCostTier(CostTier);
		if (RankPrice > (MAX_int64 - Total) / 5)
		{
			return MAX_int64;
		}
		Total += RankPrice * 5;
	}
	return Total;
}

bool FGameXXKTalentRules::ValidateProgress(const FGameXXKTalentProgress& Progress, FString* OutError)
{
	if (Progress.MinimumBackpackCapacity < 20 || Progress.MinimumBackpackCapacity > 200
		|| Progress.MinimumWarehousePages < 1 || Progress.MinimumWarehousePages > 6)
	{
		SetError(OutError, TEXT("Talent compatibility capacity floors are invalid."));
		return false;
	}
	for (const TPair<FName, int32>& Pair : Progress.NodeRanks)
	{
		const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(Pair.Key);
		if (!Node || Pair.Value < 1 || Pair.Value > Node->MaxRank)
		{
			SetError(OutError, FString::Printf(TEXT("Talent rank is invalid: %s."), *Pair.Key.ToString()));
			return false;
		}
	}
	for (const TPair<FName, int32>& Pair : Progress.NodeRanks)
	{
		const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(Pair.Key);
		if (!Node || Node->bRoot)
		{
			continue;
		}
		for (const FName PrerequisiteId : Node->PrerequisiteIds)
		{
			if (RankOf(Progress, PrerequisiteId) <= 0)
			{
				SetError(OutError, FString::Printf(TEXT("Ranked talent %s lacks its prerequisite."), *Node->Id.ToString()));
				return false;
			}
		}
		if (Node->RequiredBackpackCapacity > 0
			&& ComputeRawBackpackCapacity(Progress) < Node->RequiredBackpackCapacity)
		{
			SetError(OutError, FString::Printf(TEXT("Ranked warehouse talent %s lacks capacity."), *Node->Id.ToString()));
			return false;
		}
	}
	SetError(OutError, FString());
	return true;
}

bool FGameXXKTalentRules::BuildProjection(const FGameXXKTalentProgress& Progress, FGameXXKTalentProjection& OutProjection, FString* OutError)
{
	OutProjection = FGameXXKTalentProjection();
	if (!ValidateProgress(Progress, OutError))
	{
		return false;
	}
	int32 AuthoredBackpackCapacity = 20;
	int32 AuthoredWarehousePages = 1;
	for (const FGameXXKTalentNodeDefinition& Node : FGameXXKTalentCatalog::GetDefinitions())
	{
		const int32 Rank = RankOf(Progress, Node.Id);
		if (Rank <= 0)
		{
			continue;
		}
		const int32 Value = Rank * Node.EffectPerRank;
		switch (Node.Effect)
		{
		case EGameXXKTalentEffect::UnlockWarehousePage: AuthoredWarehousePages += Value; break;
		case EGameXXKTalentEffect::UnlockOfflineRewards: OutProjection.bOfflineRewardsUnlocked = true; break;
		case EGameXXKTalentEffect::UnlockTools: OutProjection.bToolsUnlocked = true; break;
		case EGameXXKTalentEffect::CombatFoundation:
			OutProjection.FlatAttack += Value;
			OutProjection.FlatMaxHP += Value;
			OutProjection.FlatDefense += Value;
			break;
		case EGameXXKTalentEffect::FlatAttack: OutProjection.FlatAttack += Value; break;
		case EGameXXKTalentEffect::FlatMaxHP: OutProjection.FlatMaxHP += Value; break;
		case EGameXXKTalentEffect::FlatDefense: OutProjection.FlatDefense += Value; break;
		case EGameXXKTalentEffect::RouteAttackPercent: OutProjection.RouteAttackPercent += Value; break;
		case EGameXXKTalentEffect::RouteFinalDamagePercent: OutProjection.RouteFinalDamagePercent += Value; break;
		case EGameXXKTalentEffect::RouteDefensePercent: OutProjection.RouteDefensePercent += Value; break;
		case EGameXXKTalentEffect::RouteMaxHPPercent: OutProjection.RouteMaxHPPercent += Value; break;
		case EGameXXKTalentEffect::CriticalChancePercent: OutProjection.CriticalChancePercent += Value; break;
		case EGameXXKTalentEffect::CriticalDamagePercent: OutProjection.CriticalDamagePercent += Value; break;
		case EGameXXKTalentEffect::TravelMovementRank: OutProjection.TravelMovementRank += Value; break;
		case EGameXXKTalentEffect::BackpackSlots: AuthoredBackpackCapacity += Value; break;
		case EGameXXKTalentEffect::OnlineGoldPercent: OutProjection.OnlineGoldPercent += Value; break;
		case EGameXXKTalentEffect::OnlineExperiencePercent: OutProjection.OnlineExperiencePercent += Value; break;
		case EGameXXKTalentEffect::OfflineGoldPercent: OutProjection.OfflineGoldPercent += Value; break;
		case EGameXXKTalentEffect::OfflineExperiencePercent: OutProjection.OfflineExperiencePercent += Value; break;
		case EGameXXKTalentEffect::OfflineGoldTimePercent: OutProjection.OfflineGoldTimePercent += Value; break;
		case EGameXXKTalentEffect::OfflineExperienceTimePercent: OutProjection.OfflineExperienceTimePercent += Value; break;
		case EGameXXKTalentEffect::NormalChestDropPercent: OutProjection.NormalChestDropPercent += Value; break;
		case EGameXXKTalentEffect::AdvancedChestDropPercent: OutProjection.AdvancedChestDropPercent += Value; break;
		case EGameXXKTalentEffect::OfflineChestMinutes: OutProjection.OfflineChestMinutes += Value; break;
		case EGameXXKTalentEffect::ToolExperiencePercent: OutProjection.ToolExperiencePercent += Value; break;
		case EGameXXKTalentEffect::ToolGoldPercent: OutProjection.ToolGoldPercent += Value; break;
		default: break;
		}
	}

	OutProjection.FlatAttack = FMath::Clamp(OutProjection.FlatAttack, 0, 200);
	OutProjection.FlatMaxHP = FMath::Clamp(OutProjection.FlatMaxHP, 0, 200);
	OutProjection.FlatDefense = FMath::Clamp(OutProjection.FlatDefense, 0, 200);
	OutProjection.RouteAttackPercent = FMath::Clamp(OutProjection.RouteAttackPercent, 0, 100);
	OutProjection.RouteFinalDamagePercent = FMath::Clamp(OutProjection.RouteFinalDamagePercent, 0, 100);
	OutProjection.RouteDefensePercent = FMath::Clamp(OutProjection.RouteDefensePercent, 0, 100);
	OutProjection.RouteMaxHPPercent = FMath::Clamp(OutProjection.RouteMaxHPPercent, 0, 100);
	OutProjection.CriticalChancePercent = FMath::Clamp(OutProjection.CriticalChancePercent, 0, 20);
	OutProjection.CriticalDamagePercent = FMath::Clamp(OutProjection.CriticalDamagePercent, 0, 50);
	OutProjection.TravelMovementRank = FMath::Clamp(OutProjection.TravelMovementRank, 0, 5);
	OutProjection.BackpackCapacity = FMath::Clamp(
		FMath::Max(AuthoredBackpackCapacity, Progress.MinimumBackpackCapacity), 20, 200);
	OutProjection.WarehousePageCount = FMath::Clamp(
		FMath::Max(AuthoredWarehousePages, Progress.MinimumWarehousePages), 1, 6);
	OutProjection.OnlineGoldPercent = FMath::Clamp(OutProjection.OnlineGoldPercent, 0, 350);
	OutProjection.OnlineExperiencePercent = FMath::Clamp(OutProjection.OnlineExperiencePercent, 0, 350);
	OutProjection.OfflineGoldPercent = FMath::Clamp(OutProjection.OfflineGoldPercent, 0, 350);
	OutProjection.OfflineExperiencePercent = FMath::Clamp(OutProjection.OfflineExperiencePercent, 0, 350);
	OutProjection.OfflineGoldTimePercent = FMath::Clamp(OutProjection.OfflineGoldTimePercent, 0, 350);
	OutProjection.OfflineExperienceTimePercent = FMath::Clamp(OutProjection.OfflineExperienceTimePercent, 0, 350);
	OutProjection.NormalChestDropPercent = FMath::Clamp(OutProjection.NormalChestDropPercent, 0, 350);
	OutProjection.AdvancedChestDropPercent = FMath::Clamp(OutProjection.AdvancedChestDropPercent, 0, 350);
	OutProjection.OfflineChestMinutes = FMath::Clamp(OutProjection.OfflineChestMinutes, 0, 525);
	OutProjection.ToolExperiencePercent = FMath::Clamp(OutProjection.ToolExperiencePercent, 0, 250);
	OutProjection.ToolGoldPercent = FMath::Clamp(OutProjection.ToolGoldPercent, 0, 250);
	SetError(OutError, FString());
	return true;
}

bool FGameXXKTalentRules::IsRevealed(const FGameXXKTalentProgress& Progress, const FGameXXKTalentNodeDefinition& Node)
{
	if (Node.bRoot)
	{
		return true;
	}
	return ArePrerequisitesMet(Progress, Node, nullptr);
}

bool FGameXXKTalentRules::ArePrerequisitesMet(const FGameXXKTalentProgress& Progress, const FGameXXKTalentNodeDefinition& Node, FText* OutReason)
{
	const FText Reason = LockReasonFor(Progress, Node);
	if (OutReason)
	{
		*OutReason = Reason;
	}
	return Reason.IsEmpty();
}

TArray<FGameXXKTalentNodeView> FGameXXKTalentRules::BuildNodeViews(const FGameXXKRuntimeState& State)
{
	TArray<FGameXXKTalentNodeView> Views;
	Views.Reserve(FGameXXKTalentCatalog::GetDefinitions().Num());
	for (const FGameXXKTalentNodeDefinition& Node : FGameXXKTalentCatalog::GetDefinitions())
	{
		FGameXXKTalentNodeView& View = Views.AddDefaulted_GetRef();
		View.Definition = Node;
		View.Rank = RankOf(State.Talents, Node.Id);
		View.NextPrice = GetRankPrice(Node, View.Rank);
		if (!IsRevealed(State.Talents, Node))
		{
			View.State = EGameXXKTalentNodeState::Hidden;
			View.LockReason = LockReasonFor(State.Talents, Node);
		}
		else if (View.Rank >= Node.MaxRank)
		{
			View.State = EGameXXKTalentNodeState::Maxed;
		}
		else
		{
			FText Reason;
			if (!ArePrerequisitesMet(State.Talents, Node, &Reason))
			{
				View.State = EGameXXKTalentNodeState::Locked;
				View.LockReason = Reason;
			}
			else if (static_cast<int64>(State.PlayerGold) >= View.NextPrice)
			{
				View.State = View.Rank > 0
					? EGameXXKTalentNodeState::Purchased
					: EGameXXKTalentNodeState::Available;
			}
			else
			{
				View.State = EGameXXKTalentNodeState::Locked;
				View.LockReason = FText::FromString(TEXT("金币不足"));
			}
		}
	}
	return Views;
}

bool FGameXXKTalentRules::Purchase(FGameXXKRuntimeState& InOutState, const FName NodeId, FGameXXKTalentPurchaseResult& OutResult)
{
	OutResult = FGameXXKTalentPurchaseResult();
	OutResult.NodeId = NodeId;
	const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(NodeId);
	if (!Node)
	{
		return FailPurchase(OutResult, TEXT("未知天赋节点"));
	}
	const int32 RankBefore = RankOf(InOutState.Talents, NodeId);
	if (RankBefore >= Node->MaxRank)
	{
		return FailPurchase(OutResult, TEXT("该天赋已满级"));
	}
	if (!IsRevealed(InOutState.Talents, *Node))
	{
		return FailPurchase(OutResult, TEXT("该天赋尚未显现"));
	}
	FText PrerequisiteReason;
	if (!ArePrerequisitesMet(InOutState.Talents, *Node, &PrerequisiteReason))
	{
		return FailPurchase(OutResult, PrerequisiteReason.ToString());
	}
	const int64 Price = GetRankPrice(*Node, RankBefore);
	if (Price <= 0 || static_cast<int64>(InOutState.PlayerGold) < Price)
	{
		return FailPurchase(OutResult, TEXT("金币不足"));
	}

	FGameXXKRuntimeState Candidate = InOutState;
	Candidate.Talents.NodeRanks.Add(NodeId, RankBefore + 1);
	FGameXXKTalentProjection Projection;
	FString Error;
	if (!BuildProjection(Candidate.Talents, Projection, &Error))
	{
		return FailPurchase(OutResult, Error);
	}
	Candidate.PlayerGold -= static_cast<int32>(Price);
	InOutState = MoveTemp(Candidate);
	OutResult.bPurchased = true;
	OutResult.Price = Price;
	OutResult.RankAfter = RankBefore + 1;
	OutResult.Message = FText::FromString(TEXT("天赋已点亮"));
	return true;
}

int32 FGameXXKTalentRules::GetUnlockedBackpackCapacity(const FGameXXKRuntimeState& State)
{
	FGameXXKTalentProjection Projection;
	return BuildProjection(State.Talents, Projection) ? Projection.BackpackCapacity : 20;
}

int32 FGameXXKTalentRules::GetUnlockedWarehousePageCount(const FGameXXKRuntimeState& State)
{
	FGameXXKTalentProjection Projection;
	return BuildProjection(State.Talents, Projection) ? Projection.WarehousePageCount : 1;
}

int32 FGameXXKTalentRules::GetUnlockedWarehouseCapacity(const FGameXXKRuntimeState& State)
{
	return FMath::Min(PhysicalWarehouseCapacity, GetUnlockedWarehousePageCount(State) * 36);
}

FText FGameXXKTalentRules::DescribeEffect(const FGameXXKTalentNodeDefinition& Node, const int32 RankDelta)
{
	const int32 Value = FMath::Max(1, RankDelta) * Node.EffectPerRank;
	switch (Node.Effect)
	{
	case EGameXXKTalentEffect::UnlockWarehousePage: return FText::FromString(TEXT("仓库页数 +1"));
	case EGameXXKTalentEffect::UnlockOfflineRewards: return FText::FromString(TEXT("解锁离线奖励"));
	case EGameXXKTalentEffect::UnlockTools: return FText::FromString(TEXT("解锁全部工具功能"));
	case EGameXXKTalentEffect::CombatFoundation: return FText::FromString(FString::Printf(TEXT("全队攻击/生命/防御 +%d"), Value));
	case EGameXXKTalentEffect::FlatAttack: return FText::FromString(FString::Printf(TEXT("全队攻击 +%d"), Value));
	case EGameXXKTalentEffect::FlatMaxHP: return FText::FromString(FString::Printf(TEXT("全队最大生命 +%d"), Value));
	case EGameXXKTalentEffect::FlatDefense: return FText::FromString(FString::Printf(TEXT("全队防御 +%d"), Value));
	case EGameXXKTalentEffect::TravelMovementRank: return FText::FromString(TEXT("波次间走路时间 -0.5秒"));
	case EGameXXKTalentEffect::BackpackSlots: return FText::FromString(FString::Printf(TEXT("背包容量 +%d格"), Value));
	case EGameXXKTalentEffect::OfflineChestMinutes: return FText::FromString(FString::Printf(TEXT("离线宝箱累计时间 +%d分钟"), Value));
	case EGameXXKTalentEffect::RouteAttackPercent:
	case EGameXXKTalentEffect::RouteFinalDamagePercent:
	case EGameXXKTalentEffect::RouteDefensePercent:
	case EGameXXKTalentEffect::RouteMaxHPPercent:
	case EGameXXKTalentEffect::CriticalChancePercent:
	case EGameXXKTalentEffect::CriticalDamagePercent:
	case EGameXXKTalentEffect::OnlineGoldPercent:
	case EGameXXKTalentEffect::OnlineExperiencePercent:
	case EGameXXKTalentEffect::OfflineGoldPercent:
	case EGameXXKTalentEffect::OfflineExperiencePercent:
	case EGameXXKTalentEffect::OfflineGoldTimePercent:
	case EGameXXKTalentEffect::OfflineExperienceTimePercent:
	case EGameXXKTalentEffect::NormalChestDropPercent:
	case EGameXXKTalentEffect::AdvancedChestDropPercent:
	case EGameXXKTalentEffect::ToolExperiencePercent:
	case EGameXXKTalentEffect::ToolGoldPercent:
		return FText::FromString(FString::Printf(TEXT("效果 +%d%%"), Value));
	default: return Node.Description;
	}
}
