#include "GameXXKEquipmentRules.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKEquipmentCatalog.h"

namespace
{
	using K = EGameXXKEquipmentModifierKind;

	bool ConvertRoll(FGameXXKEquipmentAffixRoll& Roll, EGameXXKEquipmentSet Set,
		TSet<K>& UsedKinds, FString* OutError)
	{
		if (!FGameXXKAffixCatalog::IsRetiredResourceAffix(Roll.AffixId)) return true;
		TArray<FGameXXKAffixDefinition> Candidates = FGameXXKAffixCatalog::GetSetDefinitions(Set);
		Candidates.Append(FGameXXKAffixCatalog::GetUniversalDefinitions());
		Candidates.StableSort([](const auto& A, const auto& B)
		{
			const bool a = A.ModifierKind == K::ZhuiFengLateCardDamage;
			const bool b = B.ModifierKind == K::ZhuiFengLateCardDamage;
			return a != b ? a : A.Id.LexicalLess(B.Id);
		});
		const auto* Replacement = Candidates.FindByPredicate([&](const auto& Definition)
		{
			return !UsedKinds.Contains(Definition.ModifierKind);
		});
		if (!Replacement)
		{
			if (OutError) *OutError = TEXT("No unique numeric replacement exists for a retired resource affix.");
			return false;
		}
		const auto OldRange = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.AffixId, Roll.Tier);
		const auto NewRange = FGameXXKAffixCatalog::GetMagnitudeRange(Replacement->Id, Roll.Tier);
		if (OldRange.Minimum <= 0 || NewRange.Minimum <= 0
			|| Roll.Magnitude < OldRange.Minimum || Roll.Magnitude > OldRange.Maximum)
		{
			if (OutError) *OutError = TEXT("Cannot convert an invalid retired resource roll.");
			return false;
		}
		const int64 OldSpan = OldRange.Maximum - OldRange.Minimum;
		const int64 Relative = Roll.Magnitude - OldRange.Minimum;
		Roll.Magnitude = NewRange.Minimum + (OldSpan > 0
			? static_cast<int32>(Relative * (NewRange.Maximum - NewRange.Minimum) / OldSpan) : 0);
		Roll.AffixId = Replacement->Id;
		Roll.Unit = Replacement->Unit;
		UsedKinds.Add(Replacement->ModifierKind);
		return true;
	}
}

bool FGameXXKEquipmentRules::NormalizeRetiredResourceAffixes(
	FGameXXKEquipmentCollectionState& InOutCollection, FString* OutError)
{
	// Validate first: conversion must not conceal corruption or stale paid previews.
	if (!ValidateCollectionState(InOutCollection, OutError)) return false;
	FGameXXKEquipmentCollectionState Candidate = InOutCollection;
	for (auto& Instance : Candidate.EquipmentInstances)
	{
		const auto* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
		if (!Definition) return false; // Already diagnosed by collection validation.
		TSet<K> Used;
		for (const auto& Roll : Instance.RolledAffixes)
		{
			if (!FGameXXKAffixCatalog::IsRetiredResourceAffix(Roll.AffixId))
				Used.Add(FGameXXKAffixCatalog::FindDefinition(Roll.AffixId)->ModifierKind);
		}
		// Prefer replacing the user's specifically retired "省力" with Momentum,
		// regardless of its random position among other retired rolls.
		for (auto& Roll : Instance.RolledAffixes)
		{
			if (Roll.AffixId == FName(TEXT("Affix.ZhuiFeng.TemporaryCostReduction"))
				&& !ConvertRoll(Roll, Definition->Set, Used, OutError)) return false;
		}
		for (auto& Roll : Instance.RolledAffixes)
		{
			if (!ConvertRoll(Roll, Definition->Set, Used, OutError)) return false;
		}
	}
	auto& Pending = Candidate.PendingReforge;
	if (Pending.bActive)
	{
		const auto* Instance = FindInstance(Candidate, Pending.InstanceId);
		const auto* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
		Pending.OriginalAffix = Instance->RolledAffixes[Pending.AffixIndex];
		TSet<K> Used;
		for (int32 Index = 0; Index < Instance->RolledAffixes.Num(); ++Index)
		{
			if (Index != Pending.AffixIndex)
				Used.Add(FGameXXKAffixCatalog::FindDefinition(Instance->RolledAffixes[Index].AffixId)->ModifierKind);
		}
		const auto* PreviewDefinition = FGameXXKAffixCatalog::FindDefinition(Pending.CandidateAffix.AffixId);
		if (!FGameXXKAffixCatalog::IsRetiredResourceAffix(Pending.CandidateAffix.AffixId)
			&& Used.Contains(PreviewDefinition->ModifierKind))
		{
			// An untouched candidate may collide with a newly converted equipped roll.
			// Reuse its paid preview's percentile in the now available selected family.
			const auto OldRange = FGameXXKAffixCatalog::GetMagnitudeRange(Pending.CandidateAffix.AffixId, Pending.CandidateAffix.Tier);
			const auto NewRange = FGameXXKAffixCatalog::GetMagnitudeRange(Pending.OriginalAffix.AffixId, Pending.CandidateAffix.Tier);
			const int64 OldSpan = OldRange.Maximum - OldRange.Minimum;
			const int64 Position = Pending.CandidateAffix.Magnitude - OldRange.Minimum;
			Pending.CandidateAffix.AffixId = Pending.OriginalAffix.AffixId;
			Pending.CandidateAffix.Unit = Pending.OriginalAffix.Unit;
			Pending.CandidateAffix.Magnitude = NewRange.Minimum + (OldSpan > 0
				? static_cast<int32>(Position * (NewRange.Maximum - NewRange.Minimum) / OldSpan) : 0);
		}
		if (!ConvertRoll(Pending.CandidateAffix, Definition->Set, Used, OutError)) return false;
	}
	if (!ValidateCollectionState(Candidate, OutError)) return false;
	InOutCollection = MoveTemp(Candidate);
	return true;
}
