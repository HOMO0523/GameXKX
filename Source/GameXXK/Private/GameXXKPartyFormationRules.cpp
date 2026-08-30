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
			return FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId) != nullptr;
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
		for (const FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
		{
			if (Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
				&& StableOwnedCompanionIds.Contains(Ref.MemberId))
			{
				return Ref.MemberId;
			}
		}
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
		for (const FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
		{
			if (Ref.Kind == EGameXXKPartyMemberKind::QuestNpc
				&& FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId))
			{
				return Ref.MemberId;
			}
		}
		const FName SelectedNpcId = State.CardRun.PartySelection.QuestNpc.NpcId;
		return FGameXXKCompanionCatalog::FindQuestNpcDefinition(SelectedNpcId)
			? SelectedNpcId
			: FName(TEXT("Npc.TusiChief"));
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
	AddUniqueMember(
		Candidate,
		MakeMember(EGameXXKPartyMemberKind::QuestNpc, LegacyQuestNpcId));

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

bool FGameXXKPartyFormationRules::ResolveQuestNpcId(
	const FGameXXKRuntimeState& State,
	FName& OutNpcId,
	FString* OutError)
{
	ResetError(OutError);
	OutNpcId = NAME_None;
	if (!Validate(State, State.CardRun.OrderedFormation, OutError))
	{
		return false;
	}
	for (const FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
	{
		if (Ref.Kind == EGameXXKPartyMemberKind::QuestNpc)
		{
			OutNpcId = Ref.MemberId;
			return true;
		}
	}
	SetError(OutError, TEXT("Ordered formation has no approved NPC."));
	return false;
}

bool FGameXXKPartyFormationRules::SetQuestNpc(
	FGameXXKRuntimeState& InOutState,
	const FName QuestNpcId,
	FString* OutError)
{
	ResetError(OutError);
	if (InOutState.CardRun.bLoadoutLockedForRoute
		|| InOutState.CardRun.bHasActiveCardBattle
		|| InOutState.bHasActiveBattle
		|| InOutState.bDungeonActive
		|| InOutState.Training.bChallengeActive
		|| InOutState.Screen == EGameXXKScreen::Battle)
	{
		SetError(OutError, TEXT("NPC formation cannot change during a route or battle."));
		return false;
	}
	if (!FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId))
	{
		SetError(OutError, TEXT("Selected NPC is not one of the six owned definitions."));
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	if (!Normalize(Candidate, OutError))
	{
		return false;
	}
	FGameXXKPartyMemberRef* NpcSlot = Candidate.CardRun.OrderedFormation.Members.FindByPredicate(
		[](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
		});
	if (!NpcSlot)
	{
		SetError(OutError, TEXT("Normalized formation has no NPC slot."));
		return false;
	}
	NpcSlot->MemberId = QuestNpcId;
	ProjectCompatibility(Candidate);
	if (!Validate(Candidate, Candidate.CardRun.OrderedFormation, OutError)
		|| !ValidateCompatibilityProjection(Candidate, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
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
	int32 HeroCount = 0;
	int32 CompanionCount = 0;
	int32 QuestNpcCount = 0;
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
		switch (Ref.Kind)
		{
		case EGameXXKPartyMemberKind::Hero:
			++HeroCount;
			break;
		case EGameXXKPartyMemberKind::PermanentCompanion:
			++CompanionCount;
			break;
		case EGameXXKPartyMemberKind::QuestNpc:
			++QuestNpcCount;
			break;
		default:
			SetError(OutError, TEXT("Party formation contains an invalid member kind."));
			return false;
		}
	}

	if (HeroCount != 1 || CompanionCount != 1 || QuestNpcCount != 1)
	{
		SetError(
			OutError,
			TEXT("Party formation requires exactly one hero, one permanent companion, and one NPC."));
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
	if (!State.CardRun.ActiveTemporaryQuestNpcId.IsNone()
		|| State.CardRun.PartySelection.QuestNpc.NpcId != FirstQuestNpcId)
	{
		SetError(OutError, TEXT("Saved NPC compatibility projection does not match ordered formation."));
		return false;
	}
	return true;
}

bool FGameXXKPartyFormationRules::Normalize(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	ResetError(OutError);
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Validate(Candidate, Candidate.CardRun.OrderedFormation))
	{
		FGameXXKOrderedPartyFormation LegacyProjection;
		if (!BuildLegacyProjection(Candidate, LegacyProjection))
		{
			SetError(
				OutError,
				TEXT("Unable to normalize party formation: legacy state does not provide three legal members."));
			return false;
		}
		Candidate.CardRun.OrderedFormation = MoveTemp(LegacyProjection);
	}
	ProjectCompatibility(Candidate);
	if (!Validate(Candidate, Candidate.CardRun.OrderedFormation, OutError))
	{
		return false;
	}
	if (!ValidateCompatibilityProjection(Candidate, OutError))
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
	if (const FGameXXKQuestNpcOwnedCardLoadout* SavedLoadout =
		InOutState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(FirstQuestNpcId))
	{
		ProjectedQuestNpcCards = SavedLoadout->SelectedCardIds;
	}
	InOutState.CardRun.PartySelection.QuestNpc.NpcId = FirstQuestNpcId;
	InOutState.CardRun.PartySelection.QuestNpc.SelectedCardIds = MoveTemp(ProjectedQuestNpcCards);
}
