#include "GameXXKRunDeckRules.h"

namespace
{
	bool FailWithError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	bool IsConcreteSourceKind(const EGameXXKRouteCardSourceKind SourceKind)
	{
		switch (SourceKind)
		{
		case EGameXXKRouteCardSourceKind::HeroBase:
		case EGameXXKRouteCardSourceKind::CompanionBase:
		case EGameXXKRouteCardSourceKind::QuestNpcBase:
		case EGameXXKRouteCardSourceKind::RouteReward:
		case EGameXXKRouteCardSourceKind::Merchant:
		case EGameXXKRouteCardSourceKind::RouteBase:
			return true;
		default:
			return false;
		}
	}

	bool IsCapacitySourceKind(const EGameXXKRouteCardSourceKind SourceKind)
	{
		return SourceKind == EGameXXKRouteCardSourceKind::RouteReward
			|| SourceKind == EGameXXKRouteCardSourceKind::Merchant;
	}

	bool ConsumesRouteCapacity(const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.bConsumesRouteCapacity;
	}

	bool ValidateEntries(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		FString* OutError)
	{
		TSet<FName> EntryIds;
		TSet<int32> AcquisitionOrdinals;
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FGameXXKRouteCardEntry& Entry = Entries[Index];
			if (Entry.EntryId.IsNone())
			{
				return FailWithError(OutError, FString::Printf(TEXT("Route-card entry %d has an empty EntryId."), Index));
			}
			if (Entry.CardId.IsNone())
			{
				return FailWithError(OutError, FString::Printf(TEXT("Route-card entry %d has an empty CardId."), Index));
			}
			if (!IsConcreteQuality(Entry.CurrentQuality))
			{
				return FailWithError(OutError, FString::Printf(TEXT("Route-card entry %s has an invalid CurrentQuality."), *Entry.EntryId.ToString()));
			}
			if (!IsConcreteSourceKind(Entry.SourceKind))
			{
				return FailWithError(OutError, FString::Printf(TEXT("Route-card entry %s has an invalid SourceKind."), *Entry.EntryId.ToString()));
			}
			if (Entry.AcquisitionOrdinal < 0)
			{
				return FailWithError(OutError, FString::Printf(TEXT("Route-card entry %s has a negative AcquisitionOrdinal."), *Entry.EntryId.ToString()));
			}
			if (ConsumesRouteCapacity(Entry) && !IsCapacitySourceKind(Entry.SourceKind))
			{
				return FailWithError(OutError, FString::Printf(
					TEXT("Route-card entry %s uses a base source that cannot consume route capacity."),
					*Entry.EntryId.ToString()));
			}
			if (EntryIds.Contains(Entry.EntryId))
			{
				return FailWithError(OutError, FString::Printf(TEXT("Duplicate route-card EntryId: %s."), *Entry.EntryId.ToString()));
			}
			if (AcquisitionOrdinals.Contains(Entry.AcquisitionOrdinal))
			{
				return FailWithError(OutError, FString::Printf(TEXT("Duplicate route-card AcquisitionOrdinal: %d."), Entry.AcquisitionOrdinal));
			}

			EntryIds.Add(Entry.EntryId);
			AcquisitionOrdinals.Add(Entry.AcquisitionOrdinal);
		}
		return true;
	}

	int32 CountTemporaryEntries(const TArray<FGameXXKRouteCardEntry>& Entries)
	{
		int32 Count = 0;
		for (const FGameXXKRouteCardEntry& Entry : Entries)
		{
			if (Entry.bTemporaryRouteCard)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 CountCapacityEntries(const TArray<FGameXXKRouteCardEntry>& Entries)
	{
		int32 Count = 0;
		for (const FGameXXKRouteCardEntry& Entry : Entries)
		{
			if (ConsumesRouteCapacity(Entry))
			{
				++Count;
			}
		}
		return Count;
	}

	EGameXXKCardQuality GetMergedQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			? EGameXXKCardQuality::Rare
			: EGameXXKCardQuality::Epic;
	}

	bool SimulateAddAndMerge(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		const FGameXXKRouteCardEntry& Candidate,
		TArray<FGameXXKRouteCardEntry>& OutEntries,
		FGameXXKCardMergePreview& OutPreview,
		FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		OutPreview = FGameXXKCardMergePreview();
		OutEntries = Entries;
		OutEntries.Add(Candidate);

		if (!ValidateEntries(OutEntries, OutError))
		{
			return false;
		}

		const int32 TemporaryCountBefore = CountTemporaryEntries(Entries);
		const int32 CapacityBefore = CountCapacityEntries(Entries);
		FName CandidateLineageEntryId = Candidate.EntryId;
		OutPreview.SurvivorEntryId = Candidate.EntryId;
		OutPreview.FinalQuality = Candidate.CurrentQuality;

		const EGameXXKCardQuality MergeableQualities[] = {
			EGameXXKCardQuality::Common,
			EGameXXKCardQuality::Rare
		};

		bool bMergedThisPass = false;
		do
		{
			bMergedThisPass = false;
			for (const EGameXXKCardQuality Quality : MergeableQualities)
			{
				TArray<int32> MatchingIndices;
				for (int32 Index = 0; Index < OutEntries.Num(); ++Index)
				{
					const FGameXXKRouteCardEntry& Entry = OutEntries[Index];
					if (Entry.CardId == Candidate.CardId && Entry.CurrentQuality == Quality)
					{
						MatchingIndices.Add(Index);
					}
				}

				if (MatchingIndices.Num() < 2)
				{
					continue;
				}

				MatchingIndices.Sort([&OutEntries](const int32 LeftIndex, const int32 RightIndex)
				{
					const FGameXXKRouteCardEntry& Left = OutEntries[LeftIndex];
					const FGameXXKRouteCardEntry& Right = OutEntries[RightIndex];
					if (Left.bConsumesRouteCapacity != Right.bConsumesRouteCapacity)
					{
						return !Left.bConsumesRouteCapacity;
					}
					if (Left.bTemporaryRouteCard != Right.bTemporaryRouteCard)
					{
						return !Left.bTemporaryRouteCard;
					}
					if (Left.AcquisitionOrdinal != Right.AcquisitionOrdinal)
					{
						return Left.AcquisitionOrdinal < Right.AcquisitionOrdinal;
					}
					return LeftIndex < RightIndex;
				});

				const int32 SurvivorIndex = MatchingIndices[0];
				const int32 ConsumedIndex = MatchingIndices[1];
				const FName SurvivorEntryId = OutEntries[SurvivorIndex].EntryId;
				const FName ConsumedEntryId = OutEntries[ConsumedIndex].EntryId;
				OutEntries[SurvivorIndex].CurrentQuality = GetMergedQuality(Quality);
				OutPreview.ConsumedEntryIds.Add(ConsumedEntryId);
				OutPreview.bWillMerge = true;

				if (CandidateLineageEntryId == SurvivorEntryId || CandidateLineageEntryId == ConsumedEntryId)
				{
					CandidateLineageEntryId = SurvivorEntryId;
				}

				OutEntries.RemoveAt(ConsumedIndex);
				bMergedThisPass = true;
				break;
			}
		}
		while (bMergedThisPass);

		const FGameXXKRouteCardEntry* FinalSurvivor = OutEntries.FindByPredicate(
			[CandidateLineageEntryId](const FGameXXKRouteCardEntry& Entry)
			{
				return Entry.EntryId == CandidateLineageEntryId;
			});
		check(FinalSurvivor);
		OutPreview.SurvivorEntryId = FinalSurvivor->EntryId;
		OutPreview.FinalQuality = FinalSurvivor->CurrentQuality;
		OutPreview.TemporaryCountDelta = CountTemporaryEntries(OutEntries) - TemporaryCountBefore;
		OutPreview.CapacityDelta = CountCapacityEntries(OutEntries) - CapacityBefore;
		return true;
	}

	bool SimulateAcquisition(
		const FGameXXKCardRunState& CardRun,
		const FGameXXKRouteCardEntry& Candidate,
		const FName ReplacementEntryId,
		TArray<FGameXXKRouteCardEntry>& OutEntries,
		FGameXXKRouteCardAcquisitionPreview& OutPreview,
		FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		OutPreview = FGameXXKRouteCardAcquisitionPreview();
		OutEntries.Reset();

		if (!ValidateEntries(CardRun.RouteCardEntries, OutError))
		{
			return false;
		}

		TArray<FGameXXKRouteCardEntry> EntriesWithCandidate = CardRun.RouteCardEntries;
		EntriesWithCandidate.Add(Candidate);
		if (!ValidateEntries(EntriesWithCandidate, OutError))
		{
			return false;
		}
		if (!ConsumesRouteCapacity(Candidate))
		{
			return FailWithError(OutError, TEXT("A route-card acquisition candidate must consume route capacity."));
		}
		if (!IsCapacitySourceKind(Candidate.SourceKind))
		{
			return FailWithError(OutError, TEXT("A route-card acquisition candidate must come from a route reward or merchant."));
		}

		const int32 CapacityBefore = CountCapacityEntries(CardRun.RouteCardEntries);
		if (CapacityBefore > FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			return FailWithError(OutError, FString::Printf(
				TEXT("Route-card capacity is already %d; maximum is %d."),
				CapacityBefore,
				FGameXXKRunDeckRules::MaxRouteCardCapacity));
		}
		if (CardRun.RouteProgress.ActualRouteCardAcquisitionCount < 0
			|| CardRun.RouteProgress.ActualRouteCardAcquisitionCount == MAX_int32)
		{
			return FailWithError(OutError, TEXT("Route-card acquisition count must be non-negative and safely incrementable."));
		}

		TArray<FGameXXKRouteCardEntry> FirstPassEntries;
		FGameXXKCardMergePreview FirstPassMerge;
		if (!SimulateAddAndMerge(CardRun.RouteCardEntries, Candidate, FirstPassEntries, FirstPassMerge, OutError))
		{
			return false;
		}

		const int32 FirstPassCapacity = CountCapacityEntries(FirstPassEntries);
		if (FirstPassCapacity > FGameXXKRunDeckRules::MaxRouteCardCapacity + 1)
		{
			return FailWithError(OutError, TEXT("A single route-card acquisition cannot increase capacity above 13."));
		}

		OutPreview.Merge = FirstPassMerge;
		OutPreview.CapacityBefore = CapacityBefore;
		OutPreview.CapacityAfter = FirstPassCapacity;
		OutPreview.ReplacementEntryId = ReplacementEntryId;

		if (FirstPassCapacity <= FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			if (!ReplacementEntryId.IsNone())
			{
				OutPreview = FGameXXKRouteCardAcquisitionPreview();
				return FailWithError(OutError, TEXT("A replacement EntryId was supplied when the acquisition does not require replacement."));
			}

			OutEntries = MoveTemp(FirstPassEntries);
			return true;
		}

		for (const FGameXXKRouteCardEntry& Entry : CardRun.RouteCardEntries)
		{
			if (ConsumesRouteCapacity(Entry))
			{
				OutPreview.EligibleReplacementEntryIds.Add(Entry.EntryId);
			}
		}

		if (ReplacementEntryId.IsNone())
		{
			OutPreview.Decision = EGameXXKRouteCardAcquisitionDecision::RequiresReplacement;
			return true;
		}

		const int32 ReplacementIndex = CardRun.RouteCardEntries.IndexOfByPredicate(
			[ReplacementEntryId](const FGameXXKRouteCardEntry& Entry)
			{
				return Entry.EntryId == ReplacementEntryId;
			});
		if (ReplacementIndex == INDEX_NONE)
		{
			OutPreview = FGameXXKRouteCardAcquisitionPreview();
			return FailWithError(OutError, TEXT("Replacement EntryId does not identify an original stable route-card entry."));
		}
		if (!ConsumesRouteCapacity(CardRun.RouteCardEntries[ReplacementIndex]))
		{
			OutPreview = FGameXXKRouteCardAcquisitionPreview();
			return FailWithError(OutError, TEXT("Replacement EntryId does not consume route capacity."));
		}

		TArray<FGameXXKRouteCardEntry> ReducedEntries = CardRun.RouteCardEntries;
		ReducedEntries.RemoveAt(ReplacementIndex);
		FGameXXKCardMergePreview ReplacementMerge;
		if (!SimulateAddAndMerge(ReducedEntries, Candidate, OutEntries, ReplacementMerge, OutError))
		{
			OutPreview = FGameXXKRouteCardAcquisitionPreview();
			return false;
		}

		const int32 FinalCapacity = CountCapacityEntries(OutEntries);
		if (FinalCapacity > FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			OutPreview = FGameXXKRouteCardAcquisitionPreview();
			return FailWithError(OutError, FString::Printf(
				TEXT("Replacement acquisition would leave capacity at %d; maximum is %d."),
				FinalCapacity,
				FGameXXKRunDeckRules::MaxRouteCardCapacity));
		}

		OutPreview.Merge = MoveTemp(ReplacementMerge);
		OutPreview.CapacityAfter = FinalCapacity;
		return true;
	}
}

bool FGameXXKRunDeckRules::GetCapacityUsed(
	const TArray<FGameXXKRouteCardEntry>& Entries,
	int32& OutCapacityUsed,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	OutCapacityUsed = 0;
	if (!ValidateEntries(Entries, OutError))
	{
		return false;
	}

	OutCapacityUsed = CountCapacityEntries(Entries);
	return true;
}

bool FGameXXKRunDeckRules::PreviewAdd(
	const TArray<FGameXXKRouteCardEntry>& Entries,
	const FGameXXKRouteCardEntry& Candidate,
	FGameXXKCardMergePreview& OutPreview,
	FString* OutError)
{
	TArray<FGameXXKRouteCardEntry> SimulatedEntries;
	return SimulateAddAndMerge(Entries, Candidate, SimulatedEntries, OutPreview, OutError);
}

bool FGameXXKRunDeckRules::AddAndMerge(
	TArray<FGameXXKRouteCardEntry>& InOutEntries,
	const FGameXXKRouteCardEntry& Candidate,
	FGameXXKCardMergePreview& OutApplied,
	FString* OutError)
{
	TArray<FGameXXKRouteCardEntry> CandidateEntries;
	FGameXXKCardMergePreview CandidatePreview;
	if (!SimulateAddAndMerge(InOutEntries, Candidate, CandidateEntries, CandidatePreview, OutError))
	{
		OutApplied = FGameXXKCardMergePreview();
		return false;
	}

	InOutEntries = MoveTemp(CandidateEntries);
	OutApplied = MoveTemp(CandidatePreview);
	return true;
}

bool FGameXXKRunDeckRules::PreviewAcquisition(
	const FGameXXKCardRunState& CardRun,
	const FGameXXKRouteCardEntry& Candidate,
	const FName ReplacementEntryId,
	FGameXXKRouteCardAcquisitionPreview& OutPreview,
	FString* OutError)
{
	TArray<FGameXXKRouteCardEntry> SimulatedEntries;
	return SimulateAcquisition(CardRun, Candidate, ReplacementEntryId, SimulatedEntries, OutPreview, OutError);
}

bool FGameXXKRunDeckRules::CommitAcquisition(
	FGameXXKCardRunState& InOutCardRun,
	const FGameXXKRouteCardEntry& Candidate,
	const FName ReplacementEntryId,
	FGameXXKRouteCardAcquisitionPreview& OutApplied,
	FString* OutError)
{
	TArray<FGameXXKRouteCardEntry> CandidateEntries;
	FGameXXKRouteCardAcquisitionPreview CandidatePreview;
	if (!SimulateAcquisition(InOutCardRun, Candidate, ReplacementEntryId, CandidateEntries, CandidatePreview, OutError))
	{
		OutApplied = FGameXXKRouteCardAcquisitionPreview();
		return false;
	}

	OutApplied = CandidatePreview;
	if (CandidatePreview.Decision == EGameXXKRouteCardAcquisitionDecision::RequiresReplacement)
	{
		return false;
	}

	InOutCardRun.RouteCardEntries = MoveTemp(CandidateEntries);
	++InOutCardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	return true;
}
