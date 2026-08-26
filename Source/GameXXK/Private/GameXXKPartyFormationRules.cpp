#include "GameXXKPartyFormationRules.h"

#include "GameXXKCompanionCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"

namespace
{
	void ResetError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	const TCHAR* KindLabel(const EGameXXKPartyMemberKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKPartyMemberKind::Hero:
			return TEXT("hero");
		case EGameXXKPartyMemberKind::PermanentCompanion:
			return TEXT("permanent companion");
		case EGameXXKPartyMemberKind::QuestNpc:
			return TEXT("quest NPC");
		default:
			return TEXT("party member");
		}
	}

	bool ResolveMember(const FGameXXKRuntimeState& State, const FGameXXKPartyMemberRef& Ref)
	{
		if (!Ref.IsValid())
		{
			return false;
		}

		switch (Ref.Kind)
		{
		case EGameXXKPartyMemberKind::Hero:
			return Ref.MemberId == FGameXXKEquipmentRules::HeroCharacterId();
		case EGameXXKPartyMemberKind::PermanentCompanion:
			return State.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
				[&Ref](const FGameXXKPermanentCompanion& Companion)
				{
					return Companion.InstanceId == Ref.MemberId;
				});
		case EGameXXKPartyMemberKind::QuestNpc:
			return FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId) != nullptr
				&& Ref.MemberId == State.CardRun.ActiveTemporaryQuestNpcId
				&& Ref.MemberId == State.CardRun.PartySelection.QuestNpc.NpcId;
		default:
			return false;
		}
	}

	FGameXXKPartyMemberRef MakeMember(const EGameXXKPartyMemberKind Kind, const FName MemberId)
	{
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = Kind;
		Ref.MemberId = MemberId;
		return Ref;
	}

	bool AddUniqueMember(
		FGameXXKOrderedPartyFormation& InOutFormation,
		const FGameXXKPartyMemberRef& Candidate)
	{
		if (!Candidate.IsValid()
			|| InOutFormation.Members.ContainsByPredicate(
				[&Candidate](const FGameXXKPartyMemberRef& Existing)
				{
					return Existing.MemberId == Candidate.MemberId;
				}))
		{
			return false;
		}
		InOutFormation.Members.Add(Candidate);
		return true;
	}

	TArray<FName> GetStableOwnedCompanionIds(const FGameXXKRuntimeState& State)
	{
		TArray<FName> Result;
		for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (!Companion.InstanceId.IsNone())
			{
				Result.AddUnique(Companion.InstanceId);
			}
		}
		Result.Sort(FNameLexicalLess());
		return Result;
	}

	FName FindLegacyCompanionId(
		const FGameXXKRuntimeState& State,
		const TArray<FName>& StableOwnedCompanionIds)
	{
		const FName SelectedId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		if (StableOwnedCompanionIds.Contains(SelectedId))
		{
			return SelectedId;
		}

		for (const FName CandidateId : StableOwnedCompanionIds)
		{
			const FGameXXKPermanentCompanion* Companion =
				State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
					[CandidateId](const FGameXXKPermanentCompanion& Candidate)
					{
						return Candidate.InstanceId == CandidateId;
					});
			if (Companion && Companion->bIsActive)
			{
				return CandidateId;
			}
		}

		return StableOwnedCompanionIds.IsEmpty() ? NAME_None : StableOwnedCompanionIds[0];
	}

	FName FindLegacyQuestNpcId(const FGameXXKRuntimeState& State)
	{
		const FName ActiveNpcId = State.CardRun.ActiveTemporaryQuestNpcId;
		const FGameXXKPartyMemberRef Ref =
			MakeMember(EGameXXKPartyMemberKind::QuestNpc, ActiveNpcId);
		return ResolveMember(State, Ref) ? ActiveNpcId : NAME_None;
	}

	bool ValidatePermanentCompanionCompatibilityProjection(
		const FGameXXKRuntimeState& State,
		const FGameXXKOrderedPartyFormation& Formation,
		FString* OutError)
	{
		FName FirstCompanionId = NAME_None;
		for (const FGameXXKPartyMemberRef& Ref : Formation.Members)
		{
			if (Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion)
			{
				FirstCompanionId = Ref.MemberId;
				break;
			}
		}

		int32 ActiveCompanionCount = 0;
		FName ActiveRosterCompanionId = NAME_None;
		for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (Companion.bIsActive)
			{
				++ActiveCompanionCount;
				ActiveRosterCompanionId = Companion.InstanceId;
			}
		}
		const int32 ExpectedActiveCompanionCount = FirstCompanionId.IsNone() ? 0 : 1;
		if (State.CardRun.PartySelection.ActivePermanentCompanionInstanceId != FirstCompanionId
			|| ActiveRosterCompanionId != FirstCompanionId
			|| ActiveCompanionCount != ExpectedActiveCompanionCount)
		{
			SetError(OutError, TEXT("Saved active-companion compatibility fields do not match the first ordered companion."));
			return false;
		}
		return true;
	}
}

bool FGameXXKPartyFormationRules::BuildLegacyProjection(
	const FGameXXKRuntimeState& State,
	FGameXXKOrderedPartyFormation& OutFormation)
{
	FGameXXKOrderedPartyFormation Candidate;
	AddUniqueMember(
		Candidate,
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()));

	const TArray<FName> StableOwnedCompanionIds = GetStableOwnedCompanionIds(State);
	const FName LegacyCompanionId = FindLegacyCompanionId(State, StableOwnedCompanionIds);
	if (!LegacyCompanionId.IsNone())
	{
		AddUniqueMember(
			Candidate,
			MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, LegacyCompanionId));
	}

	const FName LegacyQuestNpcId = FindLegacyQuestNpcId(State);
	if (!LegacyQuestNpcId.IsNone())
	{
		AddUniqueMember(
			Candidate,
			MakeMember(EGameXXKPartyMemberKind::QuestNpc, LegacyQuestNpcId));
	}

	for (const FName CompanionId : StableOwnedCompanionIds)
	{
		if (Candidate.Members.Num() >= PartySize)
		{
			break;
		}
		AddUniqueMember(
			Candidate,
			MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, CompanionId));
	}

	if (Candidate.Members.Num() != PartySize || !Validate(State, Candidate))
	{
		return false;
	}

	OutFormation = MoveTemp(Candidate);
	return true;
}

bool FGameXXKPartyFormationRules::ResolveEffective(
	const FGameXXKRuntimeState& State,
	FGameXXKOrderedPartyFormation& OutFormation,
	FString* OutError)
{
	ResetError(OutError);
	FGameXXKOrderedPartyFormation Candidate = State.CardRun.OrderedFormation;
	if (!Validate(State, Candidate))
	{
		if (!BuildLegacyProjection(State, Candidate))
		{
			SetError(
				OutError,
				TEXT("Unable to resolve effective party formation: legacy state does not provide three legal members."));
			return false;
		}
	}

	OutFormation = MoveTemp(Candidate);
	return true;
}

bool FGameXXKPartyFormationRules::Validate(
	const FGameXXKRuntimeState& State,
	const FGameXXKOrderedPartyFormation& Formation,
	FString* OutError)
{
	ResetError(OutError);
	if (Formation.Members.Num() != PartySize)
	{
		SetError(OutError, TEXT("Party formation must contain exactly three members."));
		return false;
	}

	TArray<FName> SeenMemberIds;
	SeenMemberIds.Reserve(PartySize);
	bool bHasHero = false;
	for (int32 MemberIndex = 0; MemberIndex < Formation.Members.Num(); ++MemberIndex)
	{
		const FGameXXKPartyMemberRef& Ref = Formation.Members[MemberIndex];
		if (!Ref.IsValid())
		{
			SetError(
				OutError,
				FString::Printf(TEXT("Party member slot %d contains an invalid reference."), MemberIndex + 1));
			return false;
		}
		if (SeenMemberIds.Contains(Ref.MemberId))
		{
			SetError(
				OutError,
				FString::Printf(TEXT("Party formation contains duplicate entity '%s'."), *Ref.MemberId.ToString()));
			return false;
		}
		if (!ResolveMember(State, Ref))
		{
			SetError(
				OutError,
				FString::Printf(
					TEXT("Party member slot %d references an unknown or unavailable %s '%s'."),
					MemberIndex + 1,
					KindLabel(Ref.Kind),
					*Ref.MemberId.ToString()));
			return false;
		}

		SeenMemberIds.Add(Ref.MemberId);
		bHasHero |= Ref.Kind == EGameXXKPartyMemberKind::Hero;
	}

	if (!bHasHero)
	{
		SetError(OutError, TEXT("Party formation must contain at least one hero."));
		return false;
	}
	return true;
}

bool FGameXXKPartyFormationRules::ValidateCompatibilityProjection(
	const FGameXXKRuntimeState& State,
	FString* OutError)
{
	ResetError(OutError);
	FName FirstQuestNpcId = NAME_None;
	for (const FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
	{
		if (FirstQuestNpcId.IsNone() && Ref.Kind == EGameXXKPartyMemberKind::QuestNpc)
		{
			FirstQuestNpcId = Ref.MemberId;
		}
	}

	if (!ValidatePermanentCompanionCompatibilityProjection(
			State,
			State.CardRun.OrderedFormation,
			OutError))
	{
		return false;
	}
	if (State.CardRun.ActiveTemporaryQuestNpcId != FirstQuestNpcId
		|| State.CardRun.PartySelection.QuestNpc.NpcId != FirstQuestNpcId)
	{
		SetError(OutError, TEXT("Saved task-NPC compatibility fields do not match the first ordered quest NPC."));
		return false;
	}
	return true;
}

bool FGameXXKPartyFormationRules::RepairUnavailableQuestNpcSlotsPreservingOrder(
	const FGameXXKRuntimeState& State,
	FGameXXKOrderedPartyFormation& OutFormation,
	FString* OutError)
{
	ResetError(OutError);
	FGameXXKOrderedPartyFormation Candidate = State.CardRun.OrderedFormation;
	if (!ValidatePermanentCompanionCompatibilityProjection(State, Candidate, OutError))
	{
		return false;
	}
	TArray<int32> UnavailableQuestNpcSlots;
	TSet<FName> ReservedMemberIds;
	const bool bQuestNpcAvailabilityCleared = State.CardRun.ActiveTemporaryQuestNpcId.IsNone()
		&& State.CardRun.PartySelection.QuestNpc.NpcId.IsNone();
	for (int32 SlotIndex = 0; SlotIndex < Candidate.Members.Num(); ++SlotIndex)
	{
		const FGameXXKPartyMemberRef& Ref = Candidate.Members[SlotIndex];
		if (Ref.Kind != EGameXXKPartyMemberKind::QuestNpc || ResolveMember(State, Ref))
		{
			if (!Ref.MemberId.IsNone())
			{
				ReservedMemberIds.Add(Ref.MemberId);
			}
			continue;
		}

		if (!bQuestNpcAvailabilityCleared
			|| !FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId))
		{
			SetError(
				OutError,
				TEXT("Only a known task NPC retired by cleared availability mirrors can be replaced."));
			return false;
		}
		UnavailableQuestNpcSlots.Add(SlotIndex);
	}

	const TArray<FName> StableOwnedCompanionIds = GetStableOwnedCompanionIds(State);
	for (const int32 SlotIndex : UnavailableQuestNpcSlots)
	{
		const FName* ReplacementId = StableOwnedCompanionIds.FindByPredicate(
			[&ReservedMemberIds](const FName CandidateId)
			{
				return !CandidateId.IsNone() && !ReservedMemberIds.Contains(CandidateId);
			});
		if (!ReplacementId)
		{
			SetError(
				OutError,
				TEXT("No undeployed owned companion can replace the retired task-NPC slot."));
			return false;
		}
		Candidate.Members[SlotIndex] = MakeMember(
			EGameXXKPartyMemberKind::PermanentCompanion,
			*ReplacementId);
		ReservedMemberIds.Add(*ReplacementId);
	}

	if (!Validate(State, Candidate, OutError))
	{
		return false;
	}
	OutFormation = MoveTemp(Candidate);
	return true;
}

bool FGameXXKPartyFormationRules::InsertOrReplaceCurrentQuestNpcPreservingOrder(
	const FGameXXKRuntimeState& State,
	FGameXXKOrderedPartyFormation& OutFormation,
	FString* OutError)
{
	ResetError(OutError);
	const FName QuestNpcId = State.CardRun.ActiveTemporaryQuestNpcId;
	if (QuestNpcId.IsNone()
		|| QuestNpcId != State.CardRun.PartySelection.QuestNpc.NpcId
		|| !FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId))
	{
		SetError(OutError, TEXT("Current task-NPC availability mirrors are not synchronized to an approved NPC."));
		return false;
	}

	FGameXXKOrderedPartyFormation Candidate = State.CardRun.OrderedFormation;
	if (!ValidatePermanentCompanionCompatibilityProjection(State, Candidate, OutError))
	{
		return false;
	}
	int32 QuestNpcSlot = INDEX_NONE;
	for (int32 SlotIndex = 0; SlotIndex < Candidate.Members.Num(); ++SlotIndex)
	{
		if (Candidate.Members[SlotIndex].Kind == EGameXXKPartyMemberKind::QuestNpc)
		{
			if (QuestNpcSlot != INDEX_NONE)
			{
				SetError(OutError, TEXT("Ordered formation contains more than one task-NPC slot."));
				return false;
			}
			QuestNpcSlot = SlotIndex;
		}
	}
	if (QuestNpcSlot != INDEX_NONE)
	{
		const FGameXXKPartyMemberRef& ExistingQuestNpc = Candidate.Members[QuestNpcSlot];
		if (ExistingQuestNpc.MemberId != QuestNpcId || !ResolveMember(State, ExistingQuestNpc))
		{
			SetError(OutError, TEXT("Existing task-NPC slot is stale or unrelated to the synchronized current NPC."));
			return false;
		}
	}
	else
	{
		if (!Validate(State, Candidate, OutError))
		{
			return false;
		}
		for (int32 SlotIndex = Candidate.Members.Num() - 1; SlotIndex >= 0; --SlotIndex)
		{
			if (Candidate.Members[SlotIndex].Kind == EGameXXKPartyMemberKind::PermanentCompanion)
			{
				QuestNpcSlot = SlotIndex;
				break;
			}
		}
	}
	if (QuestNpcSlot == INDEX_NONE)
	{
		SetError(OutError, TEXT("Ordered formation has no task-NPC or permanent-companion slot available for support."));
		return false;
	}

	Candidate.Members[QuestNpcSlot] = MakeMember(EGameXXKPartyMemberKind::QuestNpc, QuestNpcId);
	if (!Validate(State, Candidate, OutError))
	{
		return false;
	}
	OutFormation = MoveTemp(Candidate);
	return true;
}

bool FGameXXKPartyFormationRules::Normalize(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	ResetError(OutError);
	if (Validate(InOutState, InOutState.CardRun.OrderedFormation))
	{
		return true;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKOrderedPartyFormation LegacyProjection;
	if (!BuildLegacyProjection(Candidate, LegacyProjection))
	{
		SetError(
			OutError,
			TEXT("Unable to normalize party formation: legacy state does not provide three legal members."));
		return false;
	}
	Candidate.CardRun.OrderedFormation = MoveTemp(LegacyProjection);
	if (!Validate(Candidate, Candidate.CardRun.OrderedFormation, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(Candidate);
	return true;
}

void FGameXXKPartyFormationRules::ProjectCompatibility(FGameXXKRuntimeState& InOutState)
{
	FName FirstCompanionId = NAME_None;
	FName FirstQuestNpcId = NAME_None;
	for (const FGameXXKPartyMemberRef& Ref : InOutState.CardRun.OrderedFormation.Members)
	{
		if (!ResolveMember(InOutState, Ref))
		{
			continue;
		}
		if (FirstCompanionId.IsNone() && Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion)
		{
			FirstCompanionId = Ref.MemberId;
		}
		else if (FirstQuestNpcId.IsNone() && Ref.Kind == EGameXXKPartyMemberKind::QuestNpc)
		{
			FirstQuestNpcId = Ref.MemberId;
		}
	}

	InOutState.CardRun.PartySelection.ActivePermanentCompanionInstanceId = FirstCompanionId;
	bool bAssignedActiveCompanion = false;
	for (FGameXXKPermanentCompanion& Companion : InOutState.CardRun.CompanionRoster.PermanentCompanions)
	{
		const bool bShouldBeActive = !bAssignedActiveCompanion
			&& !FirstCompanionId.IsNone()
			&& Companion.InstanceId == FirstCompanionId;
		Companion.bIsActive = bShouldBeActive;
		bAssignedActiveCompanion |= bShouldBeActive;
	}

	TArray<FName> ProjectedQuestNpcCards;
	if (!FirstQuestNpcId.IsNone())
	{
		if (InOutState.CardRun.PartySelection.QuestNpc.NpcId == FirstQuestNpcId)
		{
			ProjectedQuestNpcCards = InOutState.CardRun.PartySelection.QuestNpc.SelectedCardIds;
		}
		else if (const FGameXXKQuestNpcOwnedCardLoadout* SavedLoadout =
			InOutState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(FirstQuestNpcId))
		{
			ProjectedQuestNpcCards = SavedLoadout->SelectedCardIds;
		}
	}

	InOutState.CardRun.ActiveTemporaryQuestNpcId = FirstQuestNpcId;
	InOutState.CardRun.PartySelection.QuestNpc.NpcId = FirstQuestNpcId;
	InOutState.CardRun.PartySelection.QuestNpc.SelectedCardIds = MoveTemp(ProjectedQuestNpcCards);
}
