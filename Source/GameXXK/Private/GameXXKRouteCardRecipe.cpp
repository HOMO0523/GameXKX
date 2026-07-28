#include "GameXXKRouteCardRecipe.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"

namespace
{
	constexpr int32 HeroSelectedCardCount = 8;
	constexpr int32 PermanentCompanionSelectedCardCount = 5;
	constexpr int32 QuestNpcSelectedCardCount = 3;
	constexpr uint32 ZeroRouteSeedFallback = 0x13579BDFU;

	const FName HeroUnitId(TEXT("Player"));
	const TArray<FName> BaseRouteCards = {
		TEXT("Route.General.PoJiaTuCi"),
		TEXT("Route.General.ShouShiHuiYuan")};
	const TArray<FName> MissingPartyFillCards = {
		TEXT("Route.General.QingShenQuShi"),
		TEXT("Route.General.TuNaJue"),
		TEXT("Route.General.ZhiXueSan"),
		TEXT("Route.General.FeiZhen"),
		TEXT("Route.General.YanDun"),
		TEXT("Route.General.TieJiLi"),
		TEXT("Route.General.LinZhenMoRen"),
		TEXT("Route.Terrain.XingJunBuZhen")};

	bool SetFailure(FString* OutError, const TCHAR* Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool SetFailure(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool HasUniqueNonEmptyNames(const TArray<FName>& Names)
	{
		TSet<FName> Seen;
		for (const FName Name : Names)
		{
			if (Name.IsNone() || Seen.Contains(Name))
			{
				return false;
			}
			Seen.Add(Name);
		}
		return true;
	}

	bool ValidateHeroLoadout(const FGameXXKCardRunState& Run, FString* OutError)
	{
		if (Run.HeroUnlockedCardIds.Num() < HeroSelectedCardCount
			|| Run.HeroSelectedCardIds.Num() != HeroSelectedCardCount
			|| !HasUniqueNonEmptyNames(Run.HeroUnlockedCardIds)
			|| !HasUniqueNonEmptyNames(Run.HeroSelectedCardIds))
		{
			return SetFailure(OutError, TEXT("The hero card collection must have at least eight unlocked cards and exactly eight unique selections."));
		}
		for (const FName CardId : Run.HeroUnlockedCardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (!Definition || Definition->Owner != EGameXXKCardOwner::Hero)
			{
				return SetFailure(OutError, TEXT("The hero card collection contains an unknown or non-hero card."));
			}
		}
		for (const FName CardId : Run.HeroSelectedCardIds)
		{
			if (!Run.HeroUnlockedCardIds.Contains(CardId))
			{
				return SetFailure(OutError, TEXT("The hero card selection contains a locked card."));
			}
		}
		return true;
	}

	bool ResolveActiveCompanion(
		const FGameXXKCardRunState& Run,
		const FGameXXKPermanentCompanion*& OutCompanion,
		FString* OutError)
	{
		OutCompanion = nullptr;
		for (const FGameXXKPermanentCompanion& Candidate : Run.CompanionRoster.PermanentCompanions)
		{
			if (!Candidate.bIsActive)
			{
				continue;
			}
			if (OutCompanion)
			{
				return SetFailure(OutError, TEXT("The route card recipe cannot resolve more than one active permanent companion."));
			}
			OutCompanion = &Candidate;
		}

		if (!OutCompanion)
		{
			if (!Run.PartySelection.ActivePermanentCompanionInstanceId.IsNone())
			{
				return SetFailure(OutError, TEXT("The selected permanent companion is not active in the saved roster."));
			}
			return true;
		}

		if (OutCompanion->InstanceId.IsNone()
			|| Run.PartySelection.ActivePermanentCompanionInstanceId != OutCompanion->InstanceId
			|| OutCompanion->SelectedCardIds.Num() != PermanentCompanionSelectedCardCount
			|| !HasUniqueNonEmptyNames(OutCompanion->SelectedCardIds))
		{
			return SetFailure(OutError, TEXT("The active permanent companion must have a matching stable id and five unique selected cards."));
		}
		for (const FName CardId : OutCompanion->SelectedCardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (!Definition
				|| Definition->Owner != EGameXXKCardOwner::Profession
				|| Definition->Role != OutCompanion->Role
				|| !OutCompanion->PersonalCardIds.Contains(CardId)
				|| !OutCompanion->UnlockedPersonalCardIds.Contains(CardId))
			{
				return SetFailure(OutError, TEXT("The active permanent companion selection contains an unknown, wrong-role, non-personal, or locked card."));
			}
		}
		return true;
	}

	bool ValidateQuestNpcLoadout(const FGameXXKCardRunState& Run, FString* OutError)
	{
		if (Run.ActiveTemporaryQuestNpcId.IsNone())
		{
			if (!Run.PartySelection.QuestNpc.NpcId.IsNone()
				|| !Run.PartySelection.QuestNpc.SelectedCardIds.IsEmpty())
			{
				return SetFailure(OutError, TEXT("The route has a task-NPC selection without active task-NPC provenance."));
			}
			return true;
		}

		const FGameXXKQuestNpcDefinition* NpcDefinition =
			FGameXXKCompanionCatalog::FindQuestNpcDefinition(Run.ActiveTemporaryQuestNpcId);
		if (!NpcDefinition
			|| Run.PartySelection.QuestNpc.NpcId != Run.ActiveTemporaryQuestNpcId
			|| Run.PartySelection.QuestNpc.SelectedCardIds.Num() != QuestNpcSelectedCardCount
			|| !HasUniqueNonEmptyNames(Run.PartySelection.QuestNpc.SelectedCardIds))
		{
			return SetFailure(OutError, TEXT("The active task NPC must have matching provenance and three unique selected cards."));
		}
		for (const FName CardId : Run.PartySelection.QuestNpc.SelectedCardIds)
		{
			const FGameXXKCardDefinition* CardDefinition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (!CardDefinition
				|| CardDefinition->Owner != EGameXXKCardOwner::QuestNpc
				|| CardDefinition->NpcId != Run.ActiveTemporaryQuestNpcId
				|| !NpcDefinition->FixedCardIds.Contains(CardId))
			{
				return SetFailure(OutError, TEXT("The active task-NPC selection contains an unknown or foreign card."));
			}
		}
		return true;
	}

	bool AddEntry(
		TArray<FGameXXKRouteCardEntry>& InOutEntries,
		const int32 RouteSeed,
		const FName CardId,
		const FName OwnerUnitId,
		const EGameXXKRouteCardSourceKind SourceKind,
		const bool bTemporaryRouteCard,
		FString* OutError)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!Definition || Definition->BaseQuality == EGameXXKCardQuality::Invalid || OwnerUnitId.IsNone())
		{
			return SetFailure(OutError, TEXT("The base route-card recipe contains an unknown card, invalid quality, or empty owner."));
		}

		FGameXXKRouteCardEntry Entry;
		Entry.CardId = Definition->Id;
		Entry.CurrentQuality = Definition->BaseQuality;
		Entry.SourceKind = SourceKind;
		Entry.OwnerUnitId = OwnerUnitId;
		Entry.bTemporaryRouteCard = bTemporaryRouteCard;
		Entry.bConsumesRouteCapacity = false;
		Entry.AcquisitionOrdinal = InOutEntries.Num();
		if (!FGameXXKRouteCardRecipe::MakeStableEntryId(
			RouteSeed,
			Entry.AcquisitionOrdinal,
			Entry.EntryId,
			OutError))
		{
			return false;
		}
		InOutEntries.Add(MoveTemp(Entry));
		return true;
	}
}

bool FGameXXKRouteCardRecipe::MakeStableEntryId(
	const int32 RouteSeed,
	const int32 AcquisitionOrdinal,
	FName& OutEntryId,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (AcquisitionOrdinal < 0)
	{
		return SetFailure(OutError, TEXT("A route-card entry ordinal cannot be negative."));
	}

	const uint32 NormalizedRouteSeed = RouteSeed == 0
		? ZeroRouteSeedFallback
		: static_cast<uint32>(RouteSeed);
	const FName Candidate(*FString::Printf(
		TEXT("RouteEntry.%08X.%08X"),
		NormalizedRouteSeed,
		static_cast<uint32>(AcquisitionOrdinal)));
	OutEntryId = Candidate;
	return true;
}

bool FGameXXKRouteCardRecipe::BuildBaseEntries(
	const FGameXXKCardRunState& Run,
	const int32 RouteSeed,
	TArray<FGameXXKRouteCardEntry>& OutEntries,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	const FGameXXKPermanentCompanion* ActiveCompanion = nullptr;
	if (!ValidateHeroLoadout(Run, OutError)
		|| !ResolveActiveCompanion(Run, ActiveCompanion, OutError)
		|| !ValidateQuestNpcLoadout(Run, OutError))
	{
		return false;
	}

	TArray<FGameXXKRouteCardEntry> CandidateEntries;
	CandidateEntries.Reserve(BaseEntryCount);
	for (const FName CardId : Run.HeroSelectedCardIds)
	{
		if (!AddEntry(CandidateEntries, RouteSeed, CardId, HeroUnitId, EGameXXKRouteCardSourceKind::HeroBase, false, OutError))
		{
			return false;
		}
	}

	int32 MissingFillIndex = 0;
	if (ActiveCompanion)
	{
		for (const FName CardId : ActiveCompanion->SelectedCardIds)
		{
			if (!AddEntry(
				CandidateEntries,
				RouteSeed,
				CardId,
				ActiveCompanion->InstanceId,
				EGameXXKRouteCardSourceKind::CompanionBase,
				false,
				OutError))
			{
				return false;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < PermanentCompanionSelectedCardCount; ++Index)
		{
			if (!MissingPartyFillCards.IsValidIndex(MissingFillIndex)
				|| !AddEntry(
					CandidateEntries,
					RouteSeed,
					MissingPartyFillCards[MissingFillIndex++],
					HeroUnitId,
					EGameXXKRouteCardSourceKind::RouteBase,
					true,
					OutError))
			{
				return SetFailure(OutError, TEXT("The deterministic missing-companion fill sequence is incomplete."));
			}
		}
	}

	if (!Run.ActiveTemporaryQuestNpcId.IsNone())
	{
		for (const FName CardId : Run.PartySelection.QuestNpc.SelectedCardIds)
		{
			if (!AddEntry(
				CandidateEntries,
				RouteSeed,
				CardId,
				Run.ActiveTemporaryQuestNpcId,
				EGameXXKRouteCardSourceKind::QuestNpcBase,
				false,
				OutError))
			{
				return false;
			}
		}
	}
	else
	{
		for (int32 Index = 0; Index < QuestNpcSelectedCardCount; ++Index)
		{
			if (!MissingPartyFillCards.IsValidIndex(MissingFillIndex)
				|| !AddEntry(
					CandidateEntries,
					RouteSeed,
					MissingPartyFillCards[MissingFillIndex++],
					HeroUnitId,
					EGameXXKRouteCardSourceKind::RouteBase,
					true,
					OutError))
			{
				return SetFailure(OutError, TEXT("The deterministic missing-NPC fill sequence is incomplete."));
			}
		}
	}

	for (const FName CardId : BaseRouteCards)
	{
		if (!AddEntry(
			CandidateEntries,
			RouteSeed,
			CardId,
			HeroUnitId,
			EGameXXKRouteCardSourceKind::RouteBase,
			true,
			OutError))
		{
			return false;
		}
	}
	if (CandidateEntries.Num() != BaseEntryCount)
	{
		return SetFailure(OutError, TEXT("The deterministic base route-card recipe must contain exactly eighteen entries."));
	}

	OutEntries = MoveTemp(CandidateEntries);
	return true;
}

bool FGameXXKRouteCardRecipe::BuildBaseEntries(
	const FGameXXKRuntimeState& State,
	const int32 RouteSeed,
	TArray<FGameXXKRouteCardEntry>& OutEntries,
	FString* OutError)
{
	return BuildBaseEntries(State.CardRun, RouteSeed, OutEntries, OutError);
}
