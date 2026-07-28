#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentSetCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSetCatalogTest,
	"GameXXK.Equipment.SetCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	const TCHAR* SetSegment(const EGameXXKEquipmentSet Set)
	{
		switch (Set)
		{
		case EGameXXKEquipmentSet::PoJun: return TEXT("PoJun");
		case EGameXXKEquipmentSet::XuanJia: return TEXT("XuanJia");
		case EGameXXKEquipmentSet::QingNang: return TEXT("QingNang");
		case EGameXXKEquipmentSet::ZhuiFeng: return TEXT("ZhuiFeng");
		case EGameXXKEquipmentSet::ShiGu: return TEXT("ShiGu");
		case EGameXXKEquipmentSet::ShanHe: return TEXT("ShanHe");
		default: return TEXT("Invalid");
		}
	}

	EGameXXKEquipmentSetBonusHook ExpectedHook(const EGameXXKEquipmentSet Set, const int32 Pieces)
	{
		if (Pieces == 2)
		{
			return Set == EGameXXKEquipmentSet::ZhuiFeng ? EGameXXKEquipmentSetBonusHook::BattleStart : EGameXXKEquipmentSetBonusHook::Passive;
		}
		if (Pieces == 4)
		{
			switch (Set)
			{
			case EGameXXKEquipmentSet::PoJun: return EGameXXKEquipmentSetBonusHook::MultiHit;
			case EGameXXKEquipmentSet::XuanJia: return EGameXXKEquipmentSetBonusHook::RoundStart;
			case EGameXXKEquipmentSet::QingNang: return EGameXXKEquipmentSetBonusHook::CleanseOrOverheal;
			case EGameXXKEquipmentSet::ZhuiFeng: return EGameXXKEquipmentSetBonusHook::LowCostStreak;
			case EGameXXKEquipmentSet::ShiGu: return EGameXXKEquipmentSetBonusHook::MultipleDamageOverTime;
			case EGameXXKEquipmentSet::ShanHe: return EGameXXKEquipmentSetBonusHook::TerrainSynergyCard;
			default: return EGameXXKEquipmentSetBonusHook::Invalid;
			}
		}
		switch (Set)
		{
		case EGameXXKEquipmentSet::PoJun: return EGameXXKEquipmentSetBonusHook::FirstAttackPerRound;
		case EGameXXKEquipmentSet::XuanJia: return EGameXXKEquipmentSetBonusHook::FirstAllyHealthDamagePerRound;
		case EGameXXKEquipmentSet::QingNang: return EGameXXKEquipmentSetBonusHook::FirstHealPerRound;
		case EGameXXKEquipmentSet::ZhuiFeng: return EGameXXKEquipmentSetBonusHook::ComboThreshold;
		case EGameXXKEquipmentSet::ShiGu: return EGameXXKEquipmentSetBonusHook::RoundEnd;
		case EGameXXKEquipmentSet::ShanHe: return EGameXXKEquipmentSetBonusHook::Passive;
		default: return EGameXXKEquipmentSetBonusHook::Invalid;
		}
	}
}

bool FGameXXKEquipmentSetCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKEquipmentSetBonusDefinition>& Definitions = FGameXXKEquipmentSetCatalog::GetDefinitions();
	TestEqual(TEXT("six sets each expose 2/4/6-piece descriptors"), Definitions.Num(), 18);
	TSet<FName> Ids;
	TSet<uint8> BonusKinds;
	for (uint8 SetValue = static_cast<uint8>(EGameXXKEquipmentSet::PoJun); SetValue <= static_cast<uint8>(EGameXXKEquipmentSet::ShanHe); ++SetValue)
	{
		const EGameXXKEquipmentSet Set = static_cast<EGameXXKEquipmentSet>(SetValue);
		for (const int32 Pieces : {2, 4, 6})
		{
			const FName ExpectedId(*FString::Printf(TEXT("Set.%s.%d"), SetSegment(Set), Pieces));
			const FGameXXKEquipmentSetBonusDefinition* Definition = FGameXXKEquipmentSetCatalog::FindDefinition(ExpectedId);
			TestNotNull(FString::Printf(TEXT("stable descriptor %s resolves"), *ExpectedId.ToString()), Definition);
			if (!Definition)
			{
				continue;
			}
			TestEqual(TEXT("descriptor ID is exact"), Definition->Id, ExpectedId);
			TestEqual(TEXT("descriptor set is exact"), Definition->Set, Set);
			TestEqual(TEXT("descriptor piece threshold is exact"), Definition->RequiredPieces, Pieces);
			TestNotEqual(TEXT("descriptor effect kind is explicit"), Definition->BonusKind, EGameXXKEquipmentSetBonusKind::Invalid);
			TestFalse(TEXT("descriptor localized description is populated"), Definition->Description.IsEmpty());
			TestEqual(TEXT("descriptor hook matches the set skeleton"), Definition->Hook, ExpectedHook(Set, Pieces));
			const EGameXXKEquipmentSetBonusScope ExpectedScope = Pieces == 6 && (Set == EGameXXKEquipmentSet::XuanJia || Set == EGameXXKEquipmentSet::QingNang || Set == EGameXXKEquipmentSet::ShanHe)
				? EGameXXKEquipmentSetBonusScope::Team
				: EGameXXKEquipmentSetBonusScope::Owner;
			TestEqual(TEXT("descriptor scope matches owner/team semantics"), Definition->Scope, ExpectedScope);
			TestTrue(TEXT("descriptor values are non-zero"), Definition->Value > 0);
			if (Pieces == 2)
			{
				TestEqual(TEXT("two-piece passives use basis points"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
				TestEqual(TEXT("two-piece passives begin at 500 BP"), Definition->Value, 500);
			}
			else
			{
				const int32 PercentageValue = Pieces == 4 ? 800 : 1200;
				TestTrue(TEXT("four/six-piece values use the frozen percentage or flat seed"),
					(Definition->Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints && Definition->Value == PercentageValue)
					|| (Definition->Unit == EGameXXKEquipmentMagnitudeUnit::FlatCount && Definition->Value == 1));
			}
			const bool bTriggered = Definition->Hook != EGameXXKEquipmentSetBonusHook::Passive;
			TestEqual(TEXT("triggered effects begin at one use per round; passives have no trigger budget"), Definition->TriggersPerRound, bTriggered ? 1 : 0);
			Ids.Add(Definition->Id);
			BonusKinds.Add(static_cast<uint8>(Definition->BonusKind));
		}
	}
	TestEqual(TEXT("all descriptor IDs are unique"), Ids.Num(), 18);
	TestEqual(TEXT("all descriptor effect kinds are unique"), BonusKinds.Num(), 18);
	return true;
}

#endif
