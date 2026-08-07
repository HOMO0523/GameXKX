#include "GameXXKCompanionRules.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionCatalog.h"

namespace
{
	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	bool IsPermanentCompanionRole(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade:
		case EGameXXKCharacterRole::Guard:
		case EGameXXKCharacterRole::Healer:
		case EGameXXKCharacterRole::Hunter:
		case EGameXXKCharacterRole::Sorcerer:
		case EGameXXKCharacterRole::FormationMaster:
			return true;
		default:
			return false;
		}
	}

	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	uint32 NextCardRandom(uint32& InOutState)
	{
		if (InOutState == 0)
		{
			InOutState = 0x6D2B79F5U;
		}
		InOutState ^= InOutState << 13;
		InOutState ^= InOutState >> 17;
		InOutState ^= InOutState << 5;
		return InOutState;
	}

	constexpr int32 DefaultRecruitSequenceSeed = 0x1F4A7C15;
	constexpr int32 RecruitSequenceStepOptions[] = {1, 5, 7, 11, 13, 17, 19, 23};

	int32 GetOrInitializeRecruitSequenceSeed(FGameXXKCompanionRosterState& InOutRoster)
	{
		if (InOutRoster.RecruitSequenceSeed == 0 || InOutRoster.RecruitSequenceSeed == MIN_int32)
		{
			InOutRoster.RecruitSequenceSeed = DefaultRecruitSequenceSeed;
		}
		if (InOutRoster.RecruitSequenceOrdinal < 0)
		{
			InOutRoster.RecruitSequenceOrdinal = 0;
		}
		return InOutRoster.RecruitSequenceSeed;
	}

	int32 MakeSequenceCardSeed(const uint32 SequenceSeed, const uint32 Ordinal)
	{
		uint32 RandomState = SequenceSeed ^ (Ordinal * 0x85EBCA6BU) ^ 0x9E3779B9U;
		const uint32 Result = NextCardRandom(RandomState);
		return static_cast<int32>(Result == 0 ? 0x6D2B79F5U : Result);
	}

	FName MakeCompanionInstanceId(const FName TemplateId, const int32 RecruitSeed)
	{
		FString SanitizedTemplateId = TemplateId.ToString();
		SanitizedTemplateId.ReplaceInline(TEXT("."), TEXT("_"));
		return FName(*FString::Printf(TEXT("CompanionInstance.%s.%08X"), *SanitizedTemplateId, static_cast<uint32>(RecruitSeed)));
	}

	bool HasUniqueNames(const TArray<FName>& Values)
	{
		TSet<FName> UniqueValues;
		for (const FName Value : Values)
		{
			if (Value.IsNone() || UniqueValues.Contains(Value))
			{
				return false;
			}
			UniqueValues.Add(Value);
		}
		return true;
	}

	int32 GetUnlockedPersonalCardCount(const FGameXXKPermanentCompanion& Companion)
	{
		int32 Count = 6;
		if (Companion.Level >= 4)
		{
			++Count;
		}
		if (Companion.Star >= 2)
		{
			++Count;
		}
		if (Companion.Level >= 8)
		{
			++Count;
		}
		if (Companion.Star >= 3)
		{
			++Count;
		}
		if (Companion.Level >= 12)
		{
			++Count;
		}
		if (Companion.Star >= 4)
		{
			++Count;
		}
		return Count;
	}

	bool BuildExpectedUnlockedPersonalCardIds(
		const FGameXXKPermanentCompanion& Companion,
		TArray<FName>& OutUnlockedCardIds,
		FString* OutError)
	{
		OutUnlockedCardIds.Reset();
		if (!IsPermanentCompanionRole(Companion.Role)
			|| Companion.Level < 1 || Companion.Level > FGameXXKCompanionRules::MaxCompanionLevel
			|| Companion.Star < 1 || Companion.Star > 5)
		{
			SetError(OutError, TEXT("A companion must use a permanent role with level 1-20 and star 1-5."));
			return false;
		}
		TArray<FName> ExpectedPersonalCardIds;
		if (!FGameXXKCompanionRules::BuildPersonalCardPool(Companion.Role, Companion.CardSeed, ExpectedPersonalCardIds, OutError))
		{
			return false;
		}
		if (Companion.PersonalCardIds != ExpectedPersonalCardIds)
		{
			SetError(OutError, TEXT("The persistent personal card pool does not match its role and immutable card seed."));
			return false;
		}
		OutUnlockedCardIds = ExpectedPersonalCardIds;
		OutUnlockedCardIds.SetNum(GetUnlockedPersonalCardCount(Companion), EAllowShrinking::No);
		return true;
	}

	bool ValidateCompanionImmutableState(const FGameXXKPermanentCompanion& Companion, FString* OutError)
	{
		if (Companion.InstanceId.IsNone())
		{
			SetError(OutError, TEXT("A permanent companion must have a stable non-empty instance id."));
			return false;
		}
		const FGameXXKCompanionTemplateDefinition* Template = FGameXXKCompanionCatalog::FindRecruitTemplate(Companion.RecruitTemplateId);
		if (!Template
			|| Template->Role != Companion.Role
			|| Template->PortraitVariantKey != Companion.PortraitVariantId)
		{
			SetError(OutError, TEXT("A permanent companion does not match an approved immutable recruit template."));
			return false;
		}
		if (Companion.Experience < 0
			|| (Companion.Level == FGameXXKCompanionRules::MaxCompanionLevel && Companion.Experience != 0)
			|| (Companion.Level < FGameXXKCompanionRules::MaxCompanionLevel
				&& Companion.Experience >= FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(Companion.Level)))
		{
			SetError(OutError, TEXT("A permanent companion has an invalid experience value for its current level."));
			return false;
		}
		if (!HasUniqueNames(Companion.EquippedItemIds))
		{
			SetError(OutError, TEXT("A permanent companion cannot carry empty or duplicate equipped-item ids."));
			return false;
		}
		TArray<FName> IgnoredUnlockedCards;
		return BuildExpectedUnlockedPersonalCardIds(Companion, IgnoredUnlockedCards, OutError);
	}

	int32 GetReturnedExperienceMaterialCount(const FGameXXKPermanentCompanion& Companion)
	{
		int64 TotalExperience = Companion.Experience;
		for (int32 Level = 1; Level < Companion.Level; ++Level)
		{
			TotalExperience += FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(Level);
			if (TotalExperience >= MAX_int32)
			{
				return MAX_int32;
			}
		}
		return static_cast<int32>(TotalExperience);
	}

	int32 ComputeNpcAttribute(const int32 BaseValue, const float GrowthValue, const int32 HeroLevel)
	{
		return FMath::FloorToInt(static_cast<float>(BaseValue) + GrowthValue * static_cast<float>(HeroLevel - 1));
	}
}

bool FGameXXKCompanionRules::BuildPersonalCardPool(
	const EGameXXKCharacterRole Role,
	const int32 CardSeed,
	TArray<FName>& OutCardIds,
	FString* OutError)
{
	OutCardIds.Reset();
	if (!IsPermanentCompanionRole(Role))
	{
		SetError(OutError, TEXT("A companion personal card pool requires one of the six permanent companion roles."));
		return false;
	}

	TArray<FName> CoreCardIds;
	TArray<FName> CandidateCardIds;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession || Definition.Role != Role)
		{
			continue;
		}

		if (Definition.bCoreProfessionCard)
		{
			CoreCardIds.Add(Definition.Id);
		}
		else
		{
			CandidateCardIds.Add(Definition.Id);
		}
	}

	CoreCardIds.Sort(NameLess);
	CandidateCardIds.Sort(NameLess);
	if (CoreCardIds.Num() != 4 || CandidateCardIds.Num() != 14)
	{
		SetError(OutError, FString::Printf(
			TEXT("The %d role catalog must contain four core cards and fourteen seeded candidates, but has %d and %d."),
			static_cast<int32>(Role),
			CoreCardIds.Num(),
			CandidateCardIds.Num()));
		return false;
	}

	OutCardIds = CoreCardIds;
	uint32 RandomState = static_cast<uint32>(CardSeed);
	for (int32 SelectionIndex = 0; SelectionIndex < 8; ++SelectionIndex)
	{
		const int32 CandidateIndex = static_cast<int32>(NextCardRandom(RandomState) % static_cast<uint32>(CandidateCardIds.Num()));
		OutCardIds.Add(CandidateCardIds[CandidateIndex]);
		CandidateCardIds.RemoveAt(CandidateIndex);
	}

	return true;
}

bool FGameXXKCompanionRules::ValidatePermanentCompanionProfile(
	const FGameXXKPermanentCompanion& Companion,
	FString* OutError)
{
	if (!ValidateCompanionImmutableState(Companion, OutError))
	{
		return false;
	}
	TArray<FName> ExpectedUnlockedCardIds;
	if (!BuildExpectedUnlockedPersonalCardIds(Companion, ExpectedUnlockedCardIds, OutError)
		|| Companion.UnlockedPersonalCardIds != ExpectedUnlockedCardIds)
	{
		if (OutError && OutError->IsEmpty())
		{
			SetError(OutError, TEXT("The companion unlock frontier does not match its immutable progression state."));
		}
		return false;
	}
	return ValidateSelectedPersonalCards(Companion, Companion.SelectedCardIds, OutError);
}

bool FGameXXKCompanionRules::CreateRecruitOrder(
	FGameXXKCompanionRosterState& InOutRoster,
	const int32 RecruitOrderSeed,
	FGameXXKCompanionRecruitOrder& OutOrder,
	FString* OutError)
{
	OutOrder = FGameXXKCompanionRecruitOrder();
	if (InOutRoster.PendingRecruitment.bHasPendingRecruitment || InOutRoster.PendingRecruitOrder.bHasPendingOrder)
	{
		SetError(OutError, TEXT("A recruit order is already awaiting a claim or full-roster replacement."));
		return false;
	}
	const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
	if (Templates.Num() != 24)
	{
		SetError(OutError, TEXT("The deterministic recruit catalog must expose exactly twenty-four templates."));
		return false;
	}
	uint32 RandomState = static_cast<uint32>(RecruitOrderSeed);
	const int32 TemplateIndex = static_cast<int32>(NextCardRandom(RandomState) % static_cast<uint32>(Templates.Num()));
	FGameXXKCompanionRecruitOrder NewOrder;
	NewOrder.bHasPendingOrder = true;
	NewOrder.RecruitOrderSeed = RecruitOrderSeed;
	NewOrder.ResolvedTemplateId = Templates[TemplateIndex].TemplateId;
	NewOrder.CardSeed = static_cast<int32>(NextCardRandom(RandomState));
	if (!FGameXXKCompanionCatalog::FindRecruitTemplate(NewOrder.ResolvedTemplateId))
	{
		SetError(OutError, TEXT("The deterministic recruit order selected an invalid template."));
		return false;
	}
	InOutRoster.PendingRecruitOrder = NewOrder;
	OutOrder = NewOrder;
	return true;
}

bool FGameXXKCompanionRules::CreateAndResolveNextRecruitment(
	FGameXXKCompanionRosterState& InOutRoster,
	FGameXXKCompanionRecruitResult& OutResult,
	FString* OutError)
{
	OutResult = FGameXXKCompanionRecruitResult();
	if (InOutRoster.PendingRecruitment.bHasPendingRecruitment || InOutRoster.PendingRecruitOrder.bHasPendingOrder)
	{
		// A ticket is save-authoritative once created. Reopening the backpack must reveal the same
		// candidate rather than consuming a new sequence ordinal or rerolling its personal cards.
		return ResolvePendingRecruitOrder(InOutRoster, OutResult, OutError);
	}

	const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
	if (Templates.Num() != 24)
	{
		SetError(OutError, TEXT("The deterministic recruit catalog must expose exactly twenty-four templates."));
		return false;
	}

	const uint32 SequenceSeed = static_cast<uint32>(GetOrInitializeRecruitSequenceSeed(InOutRoster));
	const uint32 Ordinal = static_cast<uint32>(InOutRoster.RecruitSequenceOrdinal);
	const int32 SequenceStep = RecruitSequenceStepOptions[(SequenceSeed >> 8U) % UE_ARRAY_COUNT(RecruitSequenceStepOptions)];
	const int32 TemplateIndex = static_cast<int32>((SequenceSeed % static_cast<uint32>(Templates.Num())
		+ (Ordinal % static_cast<uint32>(Templates.Num())) * static_cast<uint32>(SequenceStep))
		% static_cast<uint32>(Templates.Num()));
	if (!Templates.IsValidIndex(TemplateIndex) || Templates[TemplateIndex].TemplateId.IsNone())
	{
		SetError(OutError, TEXT("The saved recruit sequence selected an invalid approved template."));
		return false;
	}

	FGameXXKCompanionRecruitOrder NewOrder;
	NewOrder.bHasPendingOrder = true;
	NewOrder.RecruitOrderSeed = static_cast<int32>(SequenceSeed ^ (Ordinal * 0x27D4EB2DU));
	NewOrder.ResolvedTemplateId = Templates[TemplateIndex].TemplateId;
	NewOrder.CardSeed = MakeSequenceCardSeed(SequenceSeed, Ordinal);
	InOutRoster.PendingRecruitOrder = NewOrder;
	if (InOutRoster.RecruitSequenceOrdinal == MAX_int32)
	{
		SetError(OutError, TEXT("The persistent recruit sequence cannot advance beyond its supported ticket range."));
		InOutRoster.PendingRecruitOrder = FGameXXKCompanionRecruitOrder();
		return false;
	}
	++InOutRoster.RecruitSequenceOrdinal;
	return ResolvePendingRecruitOrder(InOutRoster, OutResult, OutError);
}

bool FGameXXKCompanionRules::ResolvePendingRecruitOrder(
	FGameXXKCompanionRosterState& InOutRoster,
	FGameXXKCompanionRecruitResult& OutResult,
	FString* OutError)
{
	OutResult = FGameXXKCompanionRecruitResult();
	const FGameXXKCompanionRecruitOrder& Order = InOutRoster.PendingRecruitOrder;
	if (!Order.bHasPendingOrder || Order.ResolvedTemplateId.IsNone())
	{
		SetError(OutError, TEXT("There is no persistent recruit order to claim."));
		return false;
	}
	if (InOutRoster.PendingRecruitment.bHasPendingRecruitment)
	{
		if (InOutRoster.PendingRecruitment.Candidate.RecruitTemplateId != Order.ResolvedTemplateId
			|| InOutRoster.PendingRecruitment.Candidate.CardSeed != Order.CardSeed)
		{
			SetError(OutError, TEXT("The pending full-roster candidate does not match the saved recruit order."));
			return false;
		}
		OutResult.Outcome = EGameXXKCompanionRecruitOutcome::PendingReplacement;
		OutResult.Companion = InOutRoster.PendingRecruitment.Candidate;
		return true;
	}
	if (!RecruitPermanentCompanion(InOutRoster, Order.ResolvedTemplateId, Order.CardSeed, OutResult, OutError))
	{
		return false;
	}
	if (OutResult.Outcome != EGameXXKCompanionRecruitOutcome::PendingReplacement)
	{
		InOutRoster.PendingRecruitOrder = FGameXXKCompanionRecruitOrder();
	}
	return true;
}

bool FGameXXKCompanionRules::RecruitPermanentCompanion(
	FGameXXKCompanionRosterState& InOutRoster,
	const FName RecruitTemplateId,
	const int32 RecruitSeed,
	FGameXXKCompanionRecruitResult& OutResult,
	FString* OutError)
{
	OutResult = FGameXXKCompanionRecruitResult();
	if (InOutRoster.PendingRecruitment.bHasPendingRecruitment)
	{
		SetError(OutError, TEXT("Resolve the existing full-roster recruitment before attempting another recruit."));
		return false;
	}
	if (InOutRoster.SigilCount < 0)
	{
		SetError(OutError, TEXT("The permanent companion roster has a negative sigil count."));
		return false;
	}
	for (const FGameXXKPermanentCompanion& Existing : InOutRoster.PermanentCompanions)
	{
		if (!ValidatePermanentCompanionProfile(Existing, OutError))
		{
			return false;
		}
	}

	const FGameXXKCompanionTemplateDefinition* Template = FGameXXKCompanionCatalog::FindRecruitTemplate(RecruitTemplateId);
	if (!Template)
	{
		SetError(OutError, FString::Printf(TEXT("Unknown companion recruit template: %s."), *RecruitTemplateId.ToString()));
		return false;
	}

	for (const FGameXXKPermanentCompanion& Existing : InOutRoster.PermanentCompanions)
	{
		if (Existing.RecruitTemplateId == RecruitTemplateId)
		{
			InOutRoster.SigilCount = FMath::Min(MAX_int32, InOutRoster.SigilCount + 1);
			OutResult.Outcome = EGameXXKCompanionRecruitOutcome::DuplicateSigil;
			OutResult.Companion = Existing;
			return true;
		}
	}

	FGameXXKPermanentCompanion Candidate;
	Candidate.InstanceId = MakeCompanionInstanceId(RecruitTemplateId, RecruitSeed);
	Candidate.RecruitTemplateId = Template->TemplateId;
	Candidate.PortraitVariantId = Template->PortraitVariantKey;
	Candidate.NameSeed = RecruitSeed;
	Candidate.Role = Template->Role;
	Candidate.Level = 1;
	Candidate.Experience = 0;
	Candidate.Star = 1;
	Candidate.CardSeed = RecruitSeed;
	Candidate.bIsActive = false;
	Candidate.bIsNew = true;
	if (!BuildPersonalCardPool(Candidate.Role, Candidate.CardSeed, Candidate.PersonalCardIds, OutError)
		|| !RefreshUnlockedPersonalCards(Candidate, OutError))
	{
		return false;
	}
	Candidate.SelectedCardIds = Candidate.UnlockedPersonalCardIds;
	Candidate.SelectedCardIds.SetNum(5, EAllowShrinking::No);

	if (InOutRoster.PermanentCompanions.Num() >= MaxPermanentCompanions)
	{
		InOutRoster.PendingRecruitment.bHasPendingRecruitment = true;
		InOutRoster.PendingRecruitment.Candidate = Candidate;
		OutResult.Outcome = EGameXXKCompanionRecruitOutcome::PendingReplacement;
		OutResult.Companion = Candidate;
		return true;
	}

	InOutRoster.PermanentCompanions.Add(Candidate);
	OutResult.Outcome = EGameXXKCompanionRecruitOutcome::Recruited;
	OutResult.Companion = Candidate;
	return true;
}

bool FGameXXKCompanionRules::RefreshUnlockedPersonalCards(FGameXXKPermanentCompanion& InOutCompanion, FString* OutError)
{
	TArray<FName> ExpectedUnlockedCardIds;
	if (!ValidateCompanionImmutableState(InOutCompanion, OutError)
		|| !BuildExpectedUnlockedPersonalCardIds(InOutCompanion, ExpectedUnlockedCardIds, OutError))
	{
		return false;
	}
	InOutCompanion.UnlockedPersonalCardIds = MoveTemp(ExpectedUnlockedCardIds);
	return true;
}

bool FGameXXKCompanionRules::ValidateSelectedPersonalCards(
	const FGameXXKPermanentCompanion& Companion,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	if (SelectedCardIds.Num() != 5 || !HasUniqueNames(SelectedCardIds))
	{
		SetError(OutError, TEXT("A permanent companion route configuration must contain exactly five distinct cards."));
		return false;
	}

	TArray<FName> ExpectedUnlockedCardIds;
	if (!ValidateCompanionImmutableState(Companion, OutError)
		|| !BuildExpectedUnlockedPersonalCardIds(Companion, ExpectedUnlockedCardIds, OutError)
		|| Companion.UnlockedPersonalCardIds != ExpectedUnlockedCardIds)
	{
		if (OutError && OutError->IsEmpty())
		{
			SetError(OutError, TEXT("The companion unlock frontier does not match its immutable progression state."));
		}
		return false;
	}

	for (const FName SelectedCardId : SelectedCardIds)
	{
		if (!Companion.UnlockedPersonalCardIds.Contains(SelectedCardId))
		{
			SetError(OutError, FString::Printf(TEXT("Selected companion card is not unlocked: %s."), *SelectedCardId.ToString()));
			return false;
		}
	}

	return true;
}

bool FGameXXKCompanionRules::SetSelectedPersonalCards(
	FGameXXKPermanentCompanion& InOutCompanion,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	if (!ValidateSelectedPersonalCards(InOutCompanion, SelectedCardIds, OutError))
	{
		return false;
	}
	InOutCompanion.SelectedCardIds = SelectedCardIds;
	return true;
}

bool FGameXXKCompanionRules::ValidateQuestNpcCardSelection(
	const FName QuestNpcId,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
	if (!Definition)
	{
		SetError(OutError, FString::Printf(TEXT("Unknown task NPC: %s."), *QuestNpcId.ToString()));
		return false;
	}

	if (Definition->FixedCardIds.Num() != 4 || SelectedCardIds.Num() != 3 || !HasUniqueNames(SelectedCardIds))
	{
		SetError(OutError, TEXT("A task NPC must retain four fixed cards and select exactly three distinct cards for the route."));
		return false;
	}

	for (const FName SelectedCardId : SelectedCardIds)
	{
		if (!Definition->FixedCardIds.Contains(SelectedCardId))
		{
			SetError(OutError, FString::Printf(TEXT("Selected task NPC card is not in the NPC fixed pool: %s."), *SelectedCardId.ToString()));
			return false;
		}
	}

	return true;
}

bool FGameXXKCompanionRules::SetQuestNpcCardSelection(
	FGameXXKQuestNpcCardSelection& InOutSelection,
	const FName QuestNpcId,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	if (!ValidateQuestNpcCardSelection(QuestNpcId, SelectedCardIds, OutError))
	{
		return false;
	}
	InOutSelection.NpcId = QuestNpcId;
	InOutSelection.SelectedCardIds = SelectedCardIds;
	return true;
}

bool FGameXXKCompanionRules::SetActivePermanentCompanion(
	FGameXXKCompanionRosterState& InOutRoster,
	const FName InstanceId,
	FString* OutError)
{
	if (InOutRoster.PermanentCompanions.Num() > MaxPermanentCompanions)
	{
		SetError(OutError, TEXT("The permanent companion roster exceeds its twelve-slot contract."));
		return false;
	}

	bool bFoundRequestedCompanion = InstanceId.IsNone();
	TSet<FName> SeenInstanceIds;
	for (const FGameXXKPermanentCompanion& Companion : InOutRoster.PermanentCompanions)
	{
		if (Companion.InstanceId.IsNone() || SeenInstanceIds.Contains(Companion.InstanceId))
		{
			SetError(OutError, TEXT("The permanent companion roster contains invalid or duplicate instance ids."));
			return false;
		}
		SeenInstanceIds.Add(Companion.InstanceId);
		if (!ValidatePermanentCompanionProfile(Companion, OutError))
		{
			return false;
		}
		bFoundRequestedCompanion |= Companion.InstanceId == InstanceId;
	}

	if (!bFoundRequestedCompanion)
	{
		SetError(OutError, FString::Printf(TEXT("Cannot activate a companion that is not in the permanent roster: %s."), *InstanceId.ToString()));
		return false;
	}
	for (FGameXXKPermanentCompanion& Companion : InOutRoster.PermanentCompanions)
	{
		Companion.bIsActive = !InstanceId.IsNone() && Companion.InstanceId == InstanceId;
	}
	return true;
}

bool FGameXXKCompanionRules::ResolvePendingRecruitment(
	FGameXXKCompanionRosterState& InOutRoster,
	const FName DismissedInstanceId,
	const FName ActivePermanentCompanionInstanceIdAfterReplacement,
	FGameXXKCompanionDismissalRefund& OutRefund,
	FString* OutError)
{
	OutRefund = FGameXXKCompanionDismissalRefund();
	if (!InOutRoster.PendingRecruitment.bHasPendingRecruitment
		|| InOutRoster.PermanentCompanions.Num() != MaxPermanentCompanions)
	{
		SetError(OutError, TEXT("A replacement can only resolve an existing pending recruit from a full twelve-slot roster."));
		return false;
	}
	const FGameXXKPermanentCompanion& PendingCandidate = InOutRoster.PendingRecruitment.Candidate;
	if (InOutRoster.PendingRecruitOrder.bHasPendingOrder
		&& (PendingCandidate.RecruitTemplateId != InOutRoster.PendingRecruitOrder.ResolvedTemplateId
			|| PendingCandidate.CardSeed != InOutRoster.PendingRecruitOrder.CardSeed))
	{
		SetError(OutError, TEXT("The pending replacement candidate does not match its saved recruit order."));
		return false;
	}
	if (!ValidatePermanentCompanionProfile(PendingCandidate, OutError))
	{
		return false;
	}

	const int32 DismissedIndex = InOutRoster.PermanentCompanions.IndexOfByPredicate([DismissedInstanceId](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == DismissedInstanceId;
	});
	if (DismissedIndex == INDEX_NONE)
	{
		SetError(OutError, FString::Printf(TEXT("The selected companion is not available to dismiss: %s."), *DismissedInstanceId.ToString()));
		return false;
	}

	int32 ActiveCompanionCount = 0;
	FName ExistingActiveInstanceId = NAME_None;
	TSet<FName> SeenInstanceIds;
	TSet<FName> RemainingInstanceIds;
	for (int32 Index = 0; Index < InOutRoster.PermanentCompanions.Num(); ++Index)
	{
		const FGameXXKPermanentCompanion& Existing = InOutRoster.PermanentCompanions[Index];
		if (!ValidatePermanentCompanionProfile(Existing, OutError)
			|| SeenInstanceIds.Contains(Existing.InstanceId))
		{
			if (OutError && OutError->IsEmpty())
			{
				SetError(OutError, TEXT("The permanent companion roster contains invalid or duplicate profiles."));
			}
			return false;
		}
		SeenInstanceIds.Add(Existing.InstanceId);
		if (Existing.bIsActive)
		{
			++ActiveCompanionCount;
			ExistingActiveInstanceId = Existing.InstanceId;
		}
		if (Index != DismissedIndex)
		{
			RemainingInstanceIds.Add(Existing.InstanceId);
		}
	}
	if (ActiveCompanionCount > 1 || RemainingInstanceIds.Contains(PendingCandidate.InstanceId))
	{
		SetError(OutError, TEXT("A replacement requires a roster with unique companion identities and at most one active companion."));
		return false;
	}
	RemainingInstanceIds.Add(PendingCandidate.InstanceId);

	const FGameXXKPermanentCompanion& Dismissed = InOutRoster.PermanentCompanions[DismissedIndex];
	FName DesiredActiveInstanceId = ActivePermanentCompanionInstanceIdAfterReplacement;
	if (!Dismissed.bIsActive && DesiredActiveInstanceId.IsNone())
	{
		DesiredActiveInstanceId = ExistingActiveInstanceId;
	}
	if (!DesiredActiveInstanceId.IsNone() && !RemainingInstanceIds.Contains(DesiredActiveInstanceId))
	{
		SetError(OutError, TEXT("The requested active companion is not available after the replacement."));
		return false;
	}

	OutRefund.DismissedInstanceId = Dismissed.InstanceId;
	OutRefund.ReturnedExperienceMaterials = GetReturnedExperienceMaterialCount(Dismissed);
	// Legacy compatibility payload only. The v7+ authoritative equipment collection is neither read
	// nor mutated by this roster-only rule.
	OutRefund.ReturnedEquippedItemIds = Dismissed.EquippedItemIds;
	OutRefund.bDismissedWasActive = Dismissed.bIsActive;

	FGameXXKPermanentCompanion Candidate = PendingCandidate;
	Candidate.bIsActive = false;
	InOutRoster.PermanentCompanions.RemoveAt(DismissedIndex);
	InOutRoster.PermanentCompanions.Add(MoveTemp(Candidate));
	for (FGameXXKPermanentCompanion& Companion : InOutRoster.PermanentCompanions)
	{
		Companion.bIsActive = !DesiredActiveInstanceId.IsNone() && Companion.InstanceId == DesiredActiveInstanceId;
	}
	InOutRoster.PendingRecruitment = FGameXXKPendingCompanionRecruitment();
	InOutRoster.PendingRecruitOrder = FGameXXKCompanionRecruitOrder();
	return true;
}

bool FGameXXKCompanionRules::DiscardPendingRecruitment(FGameXXKCompanionRosterState& InOutRoster, FString* OutError)
{
	if (!InOutRoster.PendingRecruitment.bHasPendingRecruitment)
	{
		SetError(OutError, TEXT("There is no saved full-roster recruit candidate to discard."));
		return false;
	}

	const FGameXXKPermanentCompanion& Candidate = InOutRoster.PendingRecruitment.Candidate;
	if (!ValidatePermanentCompanionProfile(Candidate, OutError))
	{
		return false;
	}
	if (InOutRoster.PendingRecruitOrder.bHasPendingOrder
		&& (Candidate.RecruitTemplateId != InOutRoster.PendingRecruitOrder.ResolvedTemplateId
			|| Candidate.CardSeed != InOutRoster.PendingRecruitOrder.CardSeed))
	{
		SetError(OutError, TEXT("The saved full-roster candidate does not match its no-reroll recruit ticket."));
		return false;
	}

	InOutRoster.PendingRecruitment = FGameXXKPendingCompanionRecruitment();
	InOutRoster.PendingRecruitOrder = FGameXXKCompanionRecruitOrder();
	return true;
}

int32 FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(const int32 CurrentLevel)
{
	return CurrentLevel >= 1 && CurrentLevel < MaxCompanionLevel
		? 40 + 20 * (CurrentLevel - 1)
		: 0;
}

bool FGameXXKCompanionRules::AwardExperience(
	FGameXXKPermanentCompanion& InOutCompanion,
	const int32 ExperienceAmount,
	FString* OutError)
{
	if (ExperienceAmount < 0 || !RefreshUnlockedPersonalCards(InOutCompanion, OutError))
	{
		if (ExperienceAmount < 0)
		{
			SetError(OutError, TEXT("Companion experience cannot be negative."));
		}
		return false;
	}

	const int64 NewExperience = static_cast<int64>(InOutCompanion.Experience) + static_cast<int64>(ExperienceAmount);
	InOutCompanion.Experience = static_cast<int32>(FMath::Min<int64>(MAX_int32, NewExperience));
	while (InOutCompanion.Level < MaxCompanionLevel)
	{
		const int32 RequiredExperience = GetExperienceRequiredForNextLevel(InOutCompanion.Level);
		if (InOutCompanion.Experience < RequiredExperience)
		{
			break;
		}
		InOutCompanion.Experience -= RequiredExperience;
		++InOutCompanion.Level;
	}
	if (InOutCompanion.Level == MaxCompanionLevel)
	{
		InOutCompanion.Experience = 0;
	}
	return RefreshUnlockedPersonalCards(InOutCompanion, OutError);
}

bool FGameXXKCompanionRules::PromoteCompanionStar(
	FGameXXKPermanentCompanion& InOutCompanion,
	int32& InOutSigilCount,
	FString* OutError)
{
	if (InOutSigilCount < 0 || !RefreshUnlockedPersonalCards(InOutCompanion, OutError))
	{
		if (InOutSigilCount < 0)
		{
			SetError(OutError, TEXT("Companion sigils cannot be negative."));
		}
		return false;
	}
	if (InOutCompanion.Star >= 5)
	{
		SetError(OutError, TEXT("A companion is already at the five-star cap."));
		return false;
	}

	const int32 RequiredSigils = InOutCompanion.Star;
	if (InOutSigilCount < RequiredSigils)
	{
		SetError(OutError, FString::Printf(TEXT("Promoting this companion requires %d sigils."), RequiredSigils));
		return false;
	}

	InOutSigilCount -= RequiredSigils;
	++InOutCompanion.Star;
	return RefreshUnlockedPersonalCards(InOutCompanion, OutError);
}

bool FGameXXKCompanionRules::GetCompanionAttributes(
	const EGameXXKCharacterRole Role,
	const int32 Level,
	const int32 Star,
	const FGameXXKCompanionAttributes& EquipmentBonus,
	FGameXXKCompanionAttributes& OutAttributes,
	FString* OutError)
{
	OutAttributes = FGameXXKCompanionAttributes();
	FGameXXKCharacterStats BareStats;
	if (!FGameXXKCharacterStatRules::GetBareCompanionStats(Role, Level, Star, BareStats, OutError))
	{
		return false;
	}

	OutAttributes.Health = BareStats.MaxHealth + EquipmentBonus.Health;
	OutAttributes.Attack = BareStats.Attack + EquipmentBonus.Attack;
	OutAttributes.Defense = BareStats.Defense + EquipmentBonus.Defense;
	OutAttributes.Mana = BareStats.MaxMana + EquipmentBonus.Mana;
	OutAttributes.Speed = BareStats.Speed + EquipmentBonus.Speed;
	return true;
}

bool FGameXXKCompanionRules::GetQuestNpcAttributes(
	const FName QuestNpcId,
	const int32 HeroLevel,
	FGameXXKCompanionAttributes& OutAttributes,
	FString* OutError)
{
	OutAttributes = FGameXXKCompanionAttributes();
	const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
	if (!Definition || HeroLevel < 1)
	{
		SetError(OutError, TEXT("Task NPC attributes require an approved NPC and a positive hero level."));
		return false;
	}

	OutAttributes.Health = ComputeNpcAttribute(Definition->BaseAttributes.Health, Definition->GrowthPerLevel.Health, HeroLevel);
	OutAttributes.Attack = ComputeNpcAttribute(Definition->BaseAttributes.Attack, Definition->GrowthPerLevel.Attack, HeroLevel);
	OutAttributes.Defense = ComputeNpcAttribute(Definition->BaseAttributes.Defense, Definition->GrowthPerLevel.Defense, HeroLevel);
	OutAttributes.Mana = ComputeNpcAttribute(Definition->BaseAttributes.Mana, Definition->GrowthPerLevel.Mana, HeroLevel);
	OutAttributes.Speed = Definition->BaseAttributes.Speed;
	return true;
}

bool FGameXXKCompanionRules::ValidatePartySelection(
	const FGameXXKCompanionRosterState& Roster,
	const FGameXXKCompanionPartySelection& Selection,
	FString* OutError)
{
	if (Roster.PermanentCompanions.Num() > MaxPermanentCompanions)
	{
		SetError(OutError, TEXT("The permanent companion roster exceeds its twelve-slot contract."));
		return false;
	}
	int32 ActiveCompanionCount = 0;
	FName RosterActiveInstanceId = NAME_None;
	TSet<FName> SeenInstanceIds;
	for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
	{
		if (Companion.InstanceId.IsNone() || SeenInstanceIds.Contains(Companion.InstanceId))
		{
			SetError(OutError, TEXT("The permanent companion roster contains invalid or duplicate instance ids."));
			return false;
		}
		SeenInstanceIds.Add(Companion.InstanceId);
		if (!ValidatePermanentCompanionProfile(Companion, OutError))
		{
			return false;
		}
		if (Companion.bIsActive)
		{
			++ActiveCompanionCount;
			RosterActiveInstanceId = Companion.InstanceId;
		}
	}
	if (ActiveCompanionCount > 1)
	{
		SetError(OutError, TEXT("A roster cannot expose more than one active permanent companion."));
		return false;
	}

	if (Selection.ActivePermanentCompanionInstanceId != RosterActiveInstanceId)
	{
		SetError(OutError, TEXT("The route party selection must exactly match the roster's one active permanent companion, if any."));
		return false;
	}

	if (!Selection.ActivePermanentCompanionInstanceId.IsNone())
	{
		const FGameXXKPermanentCompanion* Companion = Roster.PermanentCompanions.FindByPredicate([&Selection](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == Selection.ActivePermanentCompanionInstanceId;
		});
		if (ActiveCompanionCount != 1 || !Companion || !Companion->bIsActive)
		{
			if (OutError && OutError->IsEmpty())
			{
				SetError(OutError, TEXT("The selected permanent partner must be the one active companion with a valid five-card configuration."));
			}
			return false;
		}
	}

	if (Selection.QuestNpc.NpcId.IsNone())
	{
		if (!Selection.QuestNpc.SelectedCardIds.IsEmpty())
		{
			SetError(OutError, TEXT("A route without a temporary task NPC cannot retain task-NPC cards."));
			return false;
		}
		return true;
	}

	return ValidateQuestNpcCardSelection(Selection.QuestNpc.NpcId, Selection.QuestNpc.SelectedCardIds, OutError);
}

FString FGameXXKCompanionRules::GetCompanionDisplayName(const EGameXXKCharacterRole Role, const int32 NameSeed)
{
	static const TCHAR* Surnames[] = {
		TEXT("白"), TEXT("柳"), TEXT("沈"), TEXT("顾"), TEXT("秦"), TEXT("苏"), TEXT("叶"), TEXT("陆"),
		TEXT("温"), TEXT("韩"), TEXT("姜"), TEXT("甄"), TEXT("萧"), TEXT("裴"), TEXT("霍"), TEXT("燕")};
	static const TCHAR* GivenNames[] = {
		TEXT("一刀"), TEXT("无咎"), TEXT("惊鸿"), TEXT("流云"), TEXT("破军"), TEXT("听风"), TEXT("望舒"), TEXT("斩星"),
		TEXT("沐雪"), TEXT("临渊"), TEXT("承影"), TEXT("逍遥"), TEXT("孤鸿"), TEXT("长歌"), TEXT("归雁"), TEXT("啸月"),
		TEXT("观澜"), TEXT("拂尘"), TEXT("踏雪"), TEXT("问剑"), TEXT("扶摇"), TEXT("停云"), TEXT("衔月"), TEXT("疏影"),
		TEXT("孤舟"), TEXT("明夷"), TEXT("太阿"), TEXT("赤霄"), TEXT("龙渊"), TEXT("湛卢"), TEXT("鱼肠"), TEXT("纯钧")};
	constexpr int32 SurnameCount = UE_ARRAY_COUNT(Surnames);
	constexpr int32 GivenCount = UE_ARRAY_COUNT(GivenNames);

	const int32 RoleIndex = static_cast<int32>(Role);
	if (RoleIndex <= static_cast<int32>(EGameXXKCharacterRole::Invalid))
	{
		return FString();
	}

	// Deterministic mixing: role + seed must both move the final name so that a
	// same-seed different-role pair can never collide.
	uint32 Mixed = static_cast<uint32>(NameSeed) * 2654435761u;
	Mixed ^= static_cast<uint32>(RoleIndex) * 40503u;
	Mixed ^= Mixed >> 16;
	const int32 SurnameIndex = Mixed % SurnameCount;
	const int32 GivenIndex = (Mixed / SurnameCount) % GivenCount;
	return FString::Printf(TEXT("%s%s"), Surnames[SurnameIndex], GivenNames[GivenIndex]);
}
