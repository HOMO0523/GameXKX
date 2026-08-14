#!/usr/bin/env python3
"""One-shot patch: Phase-2 tiered reward builder, boss-card commit, quality helpers, resolver."""

from __future__ import annotations

import sys
from pathlib import Path

ADAPTER = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\GameXXKCardBattleAdapter.cpp")
RULES = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\GameXXKMVPRules.cpp")

ADAPTER_NEW_FUNCS = '''EGameXXKCardQuality FGameXXKCardBattleAdapter::GetConfiguredCardQuality(
	const FGameXXKCardRunState& CardRun,
	const FName CardId)
{
	if (const EGameXXKCardQuality* Upgraded = CardRun.UpgradedCardQualities.Find(CardId))
	{
		return *Upgraded;
	}
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Id == CardId)
		{
			return Definition.BaseQuality;
		}
	}
	return EGameXXKCardQuality::Common;
}

EGameXXKCardQuality FGameXXKCardBattleAdapter::GetNextCardQuality(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common:
		return EGameXXKCardQuality::Rare;
	case EGameXXKCardQuality::Rare:
		return EGameXXKCardQuality::Epic;
	default:
		return Quality;
	}
}

bool FGameXXKCardBattleAdapter::CommitBossCardReward(
	FGameXXKRuntimeState& InOutState,
	const FName RewardCardId,
	const FName ReplacementEntryId,
	FString* OutError)
{
	FGameXXKRuntimeState CandidateState = InOutState;
	FGameXXKRouteCardEntry CandidateEntry;
	if (!BuildPendingRouteRewardCandidate(CandidateState, RewardCardId, CandidateEntry, OutError))
	{
		return false;
	}
	FGameXXKRouteCardAcquisitionPreview AppliedPreview;
	if (!FGameXXKRunDeckRules::CommitAcquisition(
		CandidateState.CardRun,
		CandidateEntry,
		ReplacementEntryId,
		AppliedPreview,
		OutError))
	{
		return false;
	}
	++CandidateState.CardRun.NextRouteCardEntryOrdinal;
	InOutState = MoveTemp(CandidateState);
	return true;
}

bool FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKNodeKind NodeKind,
	const int32 SourceNodeId,
	const int32 ChoiceSeed,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (SourceNodeId < 0)
	{
		return SetFailure(OutError, TEXT("Tiered battle rewards require a non-negative source node."));
	}

	FGameXXKRuntimeState CandidateState = InOutState;
	if (!EnsureCardRunInitialized(CandidateState, OutError))
	{
		return false;
	}
	int32 CapacityUsed = 0;
	if (!ValidateRouteRewardEntryAuthority(CandidateState, false, CapacityUsed, OutError)
		|| !ValidatePendingRouteRewardGate(CandidateState, false, OutError))
	{
		return false;
	}

	FGameXXKCardRunState& Run = CandidateState.CardRun;
	if (!Run.PendingReward.Options.IsEmpty() || !Run.PendingReward.CardIds.IsEmpty())
	{
		if (Run.PendingReward.SourceNodeId != SourceNodeId)
		{
			return SetFailure(OutError, TEXT("A different route reward source cannot replace the saved pending offer."));
		}
		return true;
	}
	if (!IsRewardNodeKind(NodeKind) || ChoiceSeed == 0)
	{
		return SetFailure(OutError, TEXT("A new tiered battle reward requires a battle, elite, or boss node and a non-zero choice seed."));
	}
	if (SourceNodeId != GetActiveRewardSourceNodeId(CandidateState))
	{
		return SetFailure(OutError, TEXT("The route reward source does not match the active card-battle victory."));
	}
	if (Run.NextRewardOrdinal < 0 || Run.NextRewardOrdinal == MAX_int32)
	{
		return SetFailure(OutError, TEXT("The next route reward ordinal must be non-negative and safely incrementable."));
	}

	uint32 RandomState = static_cast<uint32>(ChoiceSeed);
	TArray<FGameXXKBattleRewardOption> Options;
	Options.Reserve(3);
	auto AddOption = [&Options](const EGameXXKBattleRewardKind Kind, const FName CardId, const FName RelicId)
	{
		FGameXXKBattleRewardOption Option;
		Option.Kind = Kind;
		Option.CardId = CardId;
		Option.RelicId = RelicId;
		Options.Add(MoveTemp(Option));
	};

	// Deck-card upgrade candidates: hero + active-companion configured cards below Epic.
	TArray<FName> DeckCandidates;
	TSet<FName> SeenDeckCards;
	auto AppendDeckCards = [&](const TArray<FName>& CardIds)
	{
		for (const FName CardId : CardIds)
		{
			if (!CardId.IsNone() && !SeenDeckCards.Contains(CardId)
				&& GetConfiguredCardQuality(Run, CardId) < EGameXXKCardQuality::Epic)
			{
				SeenDeckCards.Add(CardId);
				DeckCandidates.Add(CardId);
			}
		}
	};
	AppendDeckCards(Run.HeroSelectedCardIds);
	for (const FGameXXKPermanentCompanion& Companion : Run.CompanionRoster.PermanentCompanions)
	{
		if (Companion.bIsActive)
		{
			AppendDeckCards(Companion.SelectedCardIds);
		}
	}

	TArray<FName> RelicPool;
	for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
	{
		const bool bOwned = Run.Relics.ContainsByPredicate(
			[&Definition](const FGameXXKRelicInstance& Instance) { return Instance.RelicId == Definition.Id; });
		if (!bOwned || Definition.bStackable)
		{
			RelicPool.Add(Definition.Id);
		}
	}

	auto PickDeckCard = [&]() -> bool
	{
		if (DeckCandidates.IsEmpty())
		{
			return false;
		}
		const int32 Index = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(DeckCandidates.Num()));
		AddOption(EGameXXKBattleRewardKind::DeckCardUpgrade, DeckCandidates[Index], NAME_None);
		DeckCandidates.RemoveAt(Index);
		return true;
	};
	auto PickRelic = [&]() -> bool
	{
		if (RelicPool.IsEmpty())
		{
			return false;
		}
		const int32 Index = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(RelicPool.Num()));
		AddOption(EGameXXKBattleRewardKind::Relic, NAME_None, RelicPool[Index]);
		RelicPool.RemoveAt(Index);
		return true;
	};

	if (NodeKind == EGameXXKNodeKind::Boss)
	{
		const bool bTiger = CandidateState.ActiveBattleEnemies.ContainsByPredicate(
			[](const FGameXXKBattleRuntimeUnit& Enemy) { return Enemy.Id == TEXT("Tiger"); });
		const FName BossAcquisitionKey = bTiger ? FName(TEXT("Route.Boss.Tiger")) : FName(TEXT("Route.Boss.BlackBear"));
		TArray<FName> BossCards;
		AppendEligibleRouteCards(Run, [BossAcquisitionKey](const FGameXXKCardDefinition& Definition)
		{
			return Definition.AcquisitionKey == BossAcquisitionKey;
		}, BossCards);
		if (BossCards.IsEmpty())
		{
			return SetFailure(OutError, TEXT("The boss reward pool is empty."));
		}
		const int32 BossPick = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(BossCards.Num()));
		AddOption(EGameXXKBattleRewardKind::BossCard, BossCards[BossPick], NAME_None);
		if (!PickDeckCard())
		{
			PickRelic();
		}
		PickRelic();
	}
	else if (NodeKind == EGameXXKNodeKind::Elite)
	{
		const EGameXXKBattleRewardKind AttributeKind = (NextRandom(RandomState) % 2U) == 0U
			? EGameXXKBattleRewardKind::EnergyCapBonus
			: EGameXXKBattleRewardKind::DrawBonus;
		AddOption(AttributeKind, NAME_None, NAME_None);
		if (!PickDeckCard())
		{
			PickRelic();
		}
		PickRelic();
	}
	else
	{
		PickRelic();
		PickRelic();
		if (!PickDeckCard())
		{
			PickRelic();
		}
	}
	while (Options.Num() < 3 && PickRelic())
	{
	}

	if (Options.Num() != 3)
	{
		return SetFailure(OutError, TEXT("The tiered battle reward pools cannot supply three distinct legal options."));
	}
	Run.PendingReward.SourceNodeId = SourceNodeId;
	Run.PendingReward.ChoiceSeed = ChoiceSeed;
	Run.PendingReward.Options = MoveTemp(Options);
	Run.PendingReward.bRequiresRouteCardReplacement = false;
	++Run.NextRewardOrdinal;
	InOutState = MoveTemp(CandidateState);
	return true;
}

'''

RULES_NEW_RESOLVER = '''bool UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
	FGameXXKRuntimeState& State,
	const int32 OptionIndex,
	const FName ReplacementEntryId,
	FString* OutError)
{
	FGameXXKRuntimeState Candidate = State;

	bool bBossBattle = false;
	if (Candidate.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(Candidate);
		if (!PendingNode
			|| (PendingNode->NodeKind != EGameXXKNodeKind::Battle
				&& PendingNode->NodeKind != EGameXXKNodeKind::Elite
				&& PendingNode->NodeKind != EGameXXKNodeKind::Boss))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending battle reward has no resolvable battle node.");
			}
			return false;
		}
		bBossBattle = PendingNode->NodeKind == EGameXXKNodeKind::Boss;
	}
	else
	{
		bBossBattle = GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Boss);
		if (!bBossBattle && !GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Battle))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending battle reward has no resolvable fixed battle node.");
			}
			return false;
		}
	}

	if (!Candidate.CardRun.PendingReward.Options.IsValidIndex(OptionIndex))
	{
		if (OutError)
		{
			*OutError = TEXT("The chosen battle reward option is not part of the saved offer.");
		}
		return false;
	}

	const FGameXXKBattleRewardOption Option = Candidate.CardRun.PendingReward.Options[OptionIndex];
	switch (Option.Kind)
	{
	case EGameXXKBattleRewardKind::DeckCardUpgrade:
		{
			const EGameXXKCardQuality CurrentQuality =
				FGameXXKCardBattleAdapter::GetConfiguredCardQuality(Candidate.CardRun, Option.CardId);
			if (CurrentQuality >= EGameXXKCardQuality::Epic)
			{
				if (OutError)
				{
					*OutError = TEXT("The chosen deck card is already at maximum quality.");
				}
				return false;
			}
			Candidate.CardRun.UpgradedCardQualities.Add(
				Option.CardId,
				FGameXXKCardBattleAdapter::GetNextCardQuality(CurrentQuality));
			break;
		}
	case EGameXXKBattleRewardKind::BossCard:
		if (!FGameXXKCardBattleAdapter::CommitBossCardReward(
			Candidate,
			Option.CardId,
			ReplacementEntryId,
			OutError))
		{
			return false;
		}
		break;
	case EGameXXKBattleRewardKind::Relic:
		if (!FGameXXKRelicRules::AcquireRelic(Candidate, Option.RelicId, OutError))
		{
			return false;
		}
		break;
	case EGameXXKBattleRewardKind::EnergyCapBonus:
		++Candidate.CardRun.BonusSharedEnergyCap;
		break;
	case EGameXXKBattleRewardKind::DrawBonus:
		++Candidate.CardRun.BonusRoundDrawCount;
		break;
	default:
		if (OutError)
		{
			*OutError = TEXT("The chosen battle reward option has an unknown kind.");
		}
		return false;
	}

	Candidate.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
	Candidate.CardRun.bActiveBattleRewardResolved = true;

	if (!ResolveBattleVictory(Candidate, bBossBattle))
	{
		if (OutError)
		{
			*OutError = TEXT("Reward choice succeeded but battle victory settlement failed.");
		}
		return false;
	}

	State = MoveTemp(Candidate);
	return true;
}

'''

PATCHES = [
    (
        ADAPTER,
        (
            "\tInOutState = MoveTemp(CandidateState);\n"
            "\tOutCardIds = MoveTemp(Picks);\n"
            "\treturn true;\n"
            "}\n"
        ),
        (
            "\tInOutState = MoveTemp(CandidateState);\n"
            "\tOutCardIds = MoveTemp(Picks);\n"
            "\treturn true;\n"
            "}\n"
            "\n"
            + ADAPTER_NEW_FUNCS
        ),
    ),
    (
        RULES,
        (
            "\t\tint32 ChoiceSeed = Candidate.CardRun.PendingReward.ChoiceSeed;\n"
            "\t\tif (Candidate.CardRun.PendingReward.CardIds.IsEmpty()\n"
            "\t\t\t&& !GameXXKMVP::TryBuildRouteRewardChoiceSeed(\n"
            "\t\t\t\tCandidate.CardRun.RouteRandomSeed,\n"
            "\t\t\t\tStableRewardSourceNodeId,\n"
            "\t\t\t\tCandidate.CardRun.NextRewardOrdinal,\n"
            "\t\t\t\tChoiceSeed))\n"
            "\t\t{\n"
            "\t\t\treturn false;\n"
            "\t\t}\n"
            "\t\tTArray<FName> IgnoredRewardCardIds;\n"
            "\t\tif (!FGameXXKCardBattleAdapter::CreateRouteRewardOffer(\n"
            "\t\t\tCandidate,\n"
            "\t\t\tRewardNodeKind,\n"
            "\t\t\tStableRewardSourceNodeId,\n"
            "\t\t\tChoiceSeed,\n"
            "\t\t\tIgnoredRewardCardIds))\n"
            "\t\t{\n"
            "\t\t\treturn false;\n"
            "\t\t}\n"
        ),
        (
            "\t\tint32 ChoiceSeed = Candidate.CardRun.PendingReward.ChoiceSeed;\n"
            "\t\tif (Candidate.CardRun.PendingReward.Options.IsEmpty()\n"
            "\t\t\t&& Candidate.CardRun.PendingReward.CardIds.IsEmpty()\n"
            "\t\t\t&& !GameXXKMVP::TryBuildRouteRewardChoiceSeed(\n"
            "\t\t\t\tCandidate.CardRun.RouteRandomSeed,\n"
            "\t\t\t\tStableRewardSourceNodeId,\n"
            "\t\t\t\tCandidate.CardRun.NextRewardOrdinal,\n"
            "\t\t\t\tChoiceSeed))\n"
            "\t\t{\n"
            "\t\t\treturn false;\n"
            "\t\t}\n"
            "\t\tif (!FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(\n"
            "\t\t\tCandidate,\n"
            "\t\t\tRewardNodeKind,\n"
            "\t\t\tStableRewardSourceNodeId,\n"
            "\t\t\tChoiceSeed))\n"
            "\t\t{\n"
            "\t\t\treturn false;\n"
            "\t\t}\n"
        ),
    ),
    (
        RULES,
        "bool UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(",
        RULES_NEW_RESOLVER + "\nbool UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(",
    ),
]


def main() -> int:
    for path, old, new in PATCHES:
        raw = path.read_bytes()
        crlf = b"\r\n" in raw
        eol = "\r\n" if crlf else "\n"
        old_b = old.replace("\n", eol).encode("utf-8")
        new_b = new.replace("\n", eol).encode("utf-8")
        count = raw.count(old_b)
        if count != 1:
            print(f"ABORT {path.name}: expected 1 occurrence, found {count}")
            return 1
        path.write_bytes(raw.replace(old_b, new_b, 1))
        print(f"patched {path.name} (crlf={crlf})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
