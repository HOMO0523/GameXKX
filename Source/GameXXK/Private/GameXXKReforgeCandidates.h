#pragma once
#include "GameXXKAffixCatalog.h"
#include "GameXXKEquipmentCatalog.h"

namespace GameXXKReforgeCandidates
{
	inline TArray<const FGameXXKAffixDefinition*> Build(const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Equipment, const int32 SelectedIndex)
	{
		TArray<const FGameXXKAffixDefinition*> Result;
		if (!Instance.RolledAffixes.IsValidIndex(SelectedIndex)) return Result;
		TSet<EGameXXKEquipmentModifierKind> Used;
		for (const auto& Roll : Instance.RolledAffixes)
			if (const auto* Definition=FGameXXKAffixCatalog::FindDefinition(Roll.AffixId)) Used.Add(Definition->ModifierKind);
		const auto Append=[&](const auto& Definitions)
		{
			for (const auto& Definition : Definitions)
				if (!Used.Contains(Definition.ModifierKind)) Result.Add(&Definition);
		};
		Append(FGameXXKAffixCatalog::GetUniversalDefinitions());
		Append(FGameXXKAffixCatalog::GetSetDefinitions(Equipment.Set));
		Result.Sort([](const auto& A,const auto& B){return A.Id.LexicalLess(B.Id);});
		if (Result.IsEmpty())
		{
			const auto* Current=FGameXXKAffixCatalog::FindDefinition(Instance.RolledAffixes[SelectedIndex].AffixId);
			if (Current && !FGameXXKAffixCatalog::IsRetiredResourceAffix(Current->Id)
				&& Current->ModifierKind!=EGameXXKEquipmentModifierKind::ZhuiFengLateCardDamage) Result.Add(Current);
		}
		return Result;
	}
}
