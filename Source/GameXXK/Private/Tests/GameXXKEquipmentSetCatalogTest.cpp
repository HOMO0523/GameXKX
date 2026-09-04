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
		if (Set == EGameXXKEquipmentSet::PoJun)
		{
			// Serialized values reserved by the approved PoJun redesign:
			// ChargeConsumed, BladeFinish, and FirstActiveCardNextRound.
			return static_cast<EGameXXKEquipmentSetBonusHook>(Pieces == 2 ? 14 : Pieces == 4 ? 15 : 16);
		}
		if (Set == EGameXXKEquipmentSet::QingNang)
		{
			return static_cast<EGameXXKEquipmentSetBonusHook>(17);
		}
		if (Set == EGameXXKEquipmentSet::ShiGu)
		{
			return static_cast<EGameXXKEquipmentSetBonusHook>(Pieces == 2 ? 18 : Pieces == 4 ? 19 : 20);
		}
		if (Set == EGameXXKEquipmentSet::ZhuiFeng)
		{
			return static_cast<EGameXXKEquipmentSetBonusHook>(21);
		}
		if (Set == EGameXXKEquipmentSet::XuanJia)
		{
			return Pieces == 2
				? EGameXXKEquipmentSetBonusHook::Passive
				: Pieces == 4
					? EGameXXKEquipmentSetBonusHook::RoundStart
					: EGameXXKEquipmentSetBonusHook::FirstAllyHealthDamagePerRound;
		}
		if (Set == EGameXXKEquipmentSet::ShanHe)
		{
			return Pieces == 6
				? EGameXXKEquipmentSetBonusHook::RoundStart
				: EGameXXKEquipmentSetBonusHook::TerrainSynergyCard;
		}
		return EGameXXKEquipmentSetBonusHook::Invalid;
	}

	EGameXXKEquipmentSetBonusKind ExpectedPoJunKind(const int32 Pieces)
	{
		// Keep legacy values 1..3 readable by old saves; the redesign owns 19..21.
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 19 : Pieces == 4 ? 20 : 21);
	}

	EGameXXKEquipmentSetBonusKind ExpectedQingNangKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 22 : Pieces == 4 ? 23 : 24);
	}

	EGameXXKEquipmentSetBonusKind ExpectedShiGuKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 25 : Pieces == 4 ? 26 : 27);
	}

	EGameXXKEquipmentSetBonusKind ExpectedZhuiFengKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 28 : Pieces == 4 ? 29 : 30);
	}

	const TCHAR* ExpectedPoJunDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("每回合首次由穿戴者产生的冲锋被消费后，抽1张牌。");
		case 4: return TEXT("穿戴者收招后，将该牌的冲锋保存为下回合藏式。");
		case 6: return TEXT("同回合消费冲锋并触发收招：下回合首张主动牌重放基础效果。");
		default: return TEXT("");
		}
	}

	const TCHAR* ExpectedShiGuDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("每张牌首次对一个目标施加流血、中毒或灼烧时，施加1层蚀伤。");
		case 4: return TEXT("每回合首次使目标同时具有至少2种流血、中毒或灼烧时，自动毒爆1次。");
		case 6: return TEXT("每回合首次毒爆不减少流血、中毒和灼烧层数。");
		default: return TEXT("");
		}
	}

	const TCHAR* ExpectedZhuiFengDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("全队每主动打出2张牌，抽1张牌。");
		case 4: return TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回复1点气力。");
		case 6: return TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回1气，第4张再回1气、全队蓄力1并抽1张。");
		default: return TEXT("");
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
			const EGameXXKEquipmentSetBonusScope ExpectedScope = (Set == EGameXXKEquipmentSet::QingNang || Set == EGameXXKEquipmentSet::ZhuiFeng)
				|| (Pieces == 6 && (Set == EGameXXKEquipmentSet::XuanJia || Set == EGameXXKEquipmentSet::ShanHe))
				? EGameXXKEquipmentSetBonusScope::Team
				: EGameXXKEquipmentSetBonusScope::Owner;
			TestEqual(TEXT("descriptor scope matches owner/team semantics"), Definition->Scope, ExpectedScope);
			TestTrue(TEXT("descriptor values are non-zero"), Definition->Value > 0);
			if (Set == EGameXXKEquipmentSet::PoJun)
			{
				TestEqual(TEXT("PoJun uses its redesigned serialized effect kind"), Definition->BonusKind, ExpectedPoJunKind(Pieces));
				TestEqual(TEXT("PoJun uses a one-event flat payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
				TestEqual(TEXT("PoJun uses one event per successful trigger"), Definition->Value, 1);
				TestEqual(TEXT("PoJun exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedPoJunDescription(Pieces)));
			}
			else if (Set == EGameXXKEquipmentSet::QingNang)
			{
				TestEqual(TEXT("QingNang uses its redesigned serialized effect kind"), Definition->BonusKind, ExpectedQingNangKind(Pieces));
				TestEqual(TEXT("QingNang uses a one-event flat payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
				TestEqual(TEXT("QingNang uses one event per successful trigger"), Definition->Value, 1);
			}
			else if (Set == EGameXXKEquipmentSet::ShiGu)
			{
				TestEqual(TEXT("ShiGu uses its redesigned serialized effect kind"), Definition->BonusKind, ExpectedShiGuKind(Pieces));
				TestEqual(TEXT("ShiGu uses a one-event flat payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
				TestEqual(TEXT("ShiGu uses one event per successful trigger"), Definition->Value, 1);
				TestEqual(TEXT("ShiGu exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedShiGuDescription(Pieces)));
			}
			else if (Set == EGameXXKEquipmentSet::ZhuiFeng)
			{
				TestEqual(TEXT("ZhuiFeng uses its redesigned serialized effect kind"), Definition->BonusKind, ExpectedZhuiFengKind(Pieces));
				TestEqual(TEXT("ZhuiFeng uses a one-event flat payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
				TestEqual(TEXT("ZhuiFeng uses one event per successful trigger"), Definition->Value, 1);
				TestEqual(TEXT("ZhuiFeng exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedZhuiFengDescription(Pieces)));
			}
			else if (Set == EGameXXKEquipmentSet::XuanJia)
			{
				TestEqual(TEXT("Xuanjia uses basis-point primary values"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
				TestEqual(TEXT("Xuanjia primary value matches approved tier"), Definition->Value, Pieces == 2 ? 1000 : Pieces == 4 ? 5000 : 4000);
				TestEqual(TEXT("Xuanjia secondary value matches approved tier"), Definition->SecondaryValue, Pieces == 4 ? 80 : Pieces == 6 ? 1 : 0);
			}
			else if (Set == EGameXXKEquipmentSet::ShanHe)
			{
				TestEqual(TEXT("Shanhe uses one flat operation per tier"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
				TestEqual(TEXT("Shanhe primary operation count is one"), Definition->Value, 1);
				TestEqual(TEXT("only Shanhe four-piece carries two Mana"), Definition->SecondaryValue, Pieces == 4 ? 2 : 0);
			}
			const int32 ExpectedTriggerBudget = (Set == EGameXXKEquipmentSet::ShiGu && Pieces == 2)
				|| Set == EGameXXKEquipmentSet::ZhuiFeng
				? 0
				: Definition->Hook != EGameXXKEquipmentSetBonusHook::Passive ? 1 : 0;
			TestEqual(TEXT("triggered effects use their exact per-round budget"), Definition->TriggersPerRound, ExpectedTriggerBudget);
			Ids.Add(Definition->Id);
			BonusKinds.Add(static_cast<uint8>(Definition->BonusKind));
		}
	}
	TestEqual(TEXT("all descriptor IDs are unique"), Ids.Num(), 18);
	TestEqual(TEXT("all descriptor effect kinds are unique"), BonusKinds.Num(), 18);
	return true;
}

#endif
