#include "Misc/AutomationTest.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EGameXXKEquipmentSlot AllSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory,
	};

	FGameXXKCharacterStats Stats(
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const int32 Speed)
	{
		FGameXXKCharacterStats Result;
		Result.MaxHealth = Health;
		Result.MaxMana = Mana;
		Result.Attack = Attack;
		Result.Defense = Defense;
		Result.Speed = Speed;
		return Result;
	}

	void TestStats(
		FAutomationTestBase& Test,
		const FString& Label,
		const FGameXXKCharacterStats& Actual,
		const FGameXXKCharacterStats& Expected)
	{
		Test.TestEqual(Label + TEXT(" health"), Actual.MaxHealth, Expected.MaxHealth);
		Test.TestEqual(Label + TEXT(" mana"), Actual.MaxMana, Expected.MaxMana);
		Test.TestEqual(Label + TEXT(" attack"), Actual.Attack, Expected.Attack);
		Test.TestEqual(Label + TEXT(" defense"), Actual.Defense, Expected.Defense);
		Test.TestEqual(Label + TEXT(" speed"), Actual.Speed, Expected.Speed);
	}

	TArray<uint8> SerializeCollection(const FGameXXKEquipmentCollectionState& Collection)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FGameXXKEquipmentCollectionState Copy = Collection;
		FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	FName& SlotRef(FGameXXKEquipmentLoadout& Loadout, const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return Loadout.WeaponInstanceId;
		case EGameXXKEquipmentSlot::Head: return Loadout.HeadInstanceId;
		case EGameXXKEquipmentSlot::Armor: return Loadout.ArmorInstanceId;
		case EGameXXKEquipmentSlot::Belt: return Loadout.BeltInstanceId;
		case EGameXXKEquipmentSlot::Shoes: return Loadout.ShoesInstanceId;
		default: return Loadout.AccessoryInstanceId;
		}
	}

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

	const TCHAR* SlotSegment(const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return TEXT("Weapon");
		case EGameXXKEquipmentSlot::Head: return TEXT("Head");
		case EGameXXKEquipmentSlot::Armor: return TEXT("Armor");
		case EGameXXKEquipmentSlot::Belt: return TEXT("Belt");
		case EGameXXKEquipmentSlot::Shoes: return TEXT("Shoes");
		default: return TEXT("Accessory");
		}
	}

	FGameXXKEquipmentAffixRoll Affix(
		const TCHAR* Id,
		const int32 Magnitude,
		const EGameXXKEquipmentMagnitudeUnit Unit,
		const EGameXXKAffixTier Tier = EGameXXKAffixTier::Common)
	{
		FGameXXKEquipmentAffixRoll Roll;
		Roll.AffixId = FName(Id);
		Roll.Tier = Tier;
		Roll.Magnitude = Magnitude;
		Roll.Unit = Unit;
		return Roll;
	}

	void AddQualityFillers(
		FGameXXKEquipmentInstance& Instance,
		const EGameXXKEquipmentQuality Quality,
		const bool bPrimaryIsMaxHealth,
		const bool bPrimaryIsMaxMana,
		const bool bPrimaryIsDefense)
	{
		const int32 Required = static_cast<int32>(static_cast<uint8>(Quality));
		if (Instance.RolledAffixes.Num() < Required && !bPrimaryIsMaxHealth)
		{
			Instance.RolledAffixes.Add(Affix(TEXT("Affix.Universal.MaxHealth"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints));
		}
		if (Instance.RolledAffixes.Num() < Required && !bPrimaryIsMaxMana)
		{
			Instance.RolledAffixes.Add(Affix(TEXT("Affix.Universal.MaxMana"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints));
		}
		if (Instance.RolledAffixes.Num() < Required && !bPrimaryIsDefense)
		{
			Instance.RolledAffixes.Add(Affix(TEXT("Affix.Universal.Defense"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints));
		}
	}

	FName AddModernItem(
		FGameXXKEquipmentCollectionState& Collection,
		const FName CharacterId,
		const EGameXXKEquipmentSet Set,
		const EGameXXKEquipmentSlot Slot,
		const EGameXXKEquipmentQuality Quality,
		const int32 ItemLevel,
		const int32 Enhancement,
		const FGameXXKEquipmentAffixRoll& PrimaryAffix,
		const bool bWarehouse = false,
		const TCHAR* Suffix = TEXT(""))
	{
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(
			TEXT("EquipmentInstance.Stats.%s.%s.%s.%s"),
			*CharacterId.ToString(),
			SetSegment(Set),
			SlotSegment(Slot),
			Suffix));
		Instance.BaseEquipmentId = FName(*FString::Printf(TEXT("Equipment.%s.%s"), SetSegment(Set), SlotSegment(Slot)));
		Instance.ItemLevel = ItemLevel;
		Instance.Quality = Quality;
		Instance.EnhancementLevel = Enhancement;
		Instance.ScalingRule = EGameXXKEquipmentScalingRule::ModernPercentBase;
		Instance.RolledAffixes.Add(PrimaryAffix);
		AddQualityFillers(
			Instance,
			Quality,
			PrimaryAffix.AffixId == FName(TEXT("Affix.Universal.MaxHealth")),
			PrimaryAffix.AffixId == FName(TEXT("Affix.Universal.MaxMana")),
			PrimaryAffix.AffixId == FName(TEXT("Affix.Universal.Defense")));
		if (bWarehouse)
		{
			Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
			Collection.WarehouseInstanceIds.Add(Instance.InstanceId);
		}
		else
		{
			Instance.OwnerKind = CharacterId == FGameXXKEquipmentRules::HeroCharacterId()
				? EGameXXKEquipmentOwnerKind::Hero
				: EGameXXKEquipmentOwnerKind::PermanentCompanion;
			Instance.OwnerCharacterId = CharacterId;
			SlotRef(Collection.CharacterLoadouts.FindOrAdd(CharacterId), Slot) = Instance.InstanceId;
		}
		const FName Id = Instance.InstanceId;
		Collection.EquipmentInstances.Add(MoveTemp(Instance));
		return Id;
	}

	FName AddLegacyItem(
		FGameXXKEquipmentCollectionState& Collection,
		const FName CharacterId,
		const TCHAR* BaseId,
		const EGameXXKEquipmentSlot Slot,
		const int32 Enhancement)
	{
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(FName(BaseId));
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(TEXT("EquipmentInstance.Stats.Legacy.%s"), SlotSegment(Slot)));
		Instance.BaseEquipmentId = Definition->Id;
		Instance.Quality = EGameXXKEquipmentQuality::Common;
		Instance.EnhancementLevel = Enhancement;
		Instance.ScalingRule = Definition->ScalingRule;
		Instance.LegacyBaseStatSnapshot = Definition->LegacyBaseStatSnapshot;
		Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
		Instance.OwnerCharacterId = CharacterId;
		SlotRef(Collection.CharacterLoadouts.FindOrAdd(CharacterId), Slot) = Instance.InstanceId;
		const FName Id = Instance.InstanceId;
		Collection.EquipmentInstances.Add(MoveTemp(Instance));
		return Id;
	}

	FGameXXKEquipmentCollectionState MakeSixPieceCollection(
		const FName CharacterId,
		const EGameXXKEquipmentSet Set,
		const EGameXXKEquipmentQuality Quality,
		const int32 Enhancement,
		const bool bAttackAffixes)
	{
		FGameXXKEquipmentCollectionState Collection;
		for (const EGameXXKEquipmentSlot Slot : AllSlots)
		{
			const FGameXXKEquipmentAffixRoll Primary = bAttackAffixes
				? Affix(TEXT("Affix.Universal.Attack"), 1000, EGameXXKEquipmentMagnitudeUnit::BasisPoints, EGameXXKAffixTier::Epic)
				: Affix(TEXT("Affix.Universal.MaxHealth"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
			AddModernItem(Collection, CharacterId, Set, Slot, Quality, 1, Enhancement, Primary);
		}
		return Collection;
	}

	void AppendCollection(FGameXXKEquipmentCollectionState& Target, FGameXXKEquipmentCollectionState&& Source)
	{
		Target.EquipmentInstances.Append(MoveTemp(Source.EquipmentInstances));
		Target.WarehouseInstanceIds.Append(MoveTemp(Source.WarehouseInstanceIds));
		for (TPair<FName, FGameXXKEquipmentLoadout>& Pair : Source.CharacterLoadouts)
		{
			Target.CharacterLoadouts.Add(Pair.Key, MoveTemp(Pair.Value));
		}
	}

	const FGameXXKEquipmentActiveEffect* FindEffect(
		const TArray<FGameXXKEquipmentActiveEffect>& Effects,
		const FName EffectId)
	{
		return Effects.FindByPredicate([EffectId](const FGameXXKEquipmentActiveEffect& Effect)
		{
			return Effect.EffectId == EffectId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentStatOrderTest,
	"GameXXK.Equipment.Stats.OrderLegacyAndPostBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentStatOrderTest::RunTest(const FString& Parameters)
{
	const FName Hero = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKEquipmentCollectionState Modern = MakeSixPieceCollection(
		Hero,
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentQuality::Epic,
		0,
		true);
	const int32 Enhancements[] = {3, 1, 5, 1, 10, 10};
	for (int32 Index = 0; Index < Modern.EquipmentInstances.Num(); ++Index)
	{
		Modern.EquipmentInstances[Index].EnhancementLevel = Enhancements[Index];
		Modern.EquipmentInstances[Index].RolledAffixes[1] = Affix(
			TEXT("Affix.PoJun.DirectDamage"),
			1000,
			EGameXXKEquipmentMagnitudeUnit::BasisPoints,
			EGameXXKAffixTier::Epic);
		Modern.EquipmentInstances[Index].RolledAffixes[2] = Affix(
			TEXT("Affix.PoJun.ArmorBreakStacks"),
			2,
			EGameXXKEquipmentMagnitudeUnit::FlatCount,
			EGameXXKAffixTier::Epic);
	}
	TestTrue(TEXT("precise modern fixture validates"), FGameXXKEquipmentRules::ValidateCollectionState(Modern));

	const FGameXXKCharacterStats Bare = Stats(101, 51, 11, 7, 3);
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	FString Error;
	TestTrue(TEXT("modern six-piece projection builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(Modern, Hero, Bare, Snapshot, &Error));
	TestStats(*this, TEXT("percent and flat enhancement resolve per item before summing"), Snapshot.EnhancedEquipmentBaseStats, Stats(80, 0, 34, 31, 32));
	TestStats(*this, TEXT("six attack percentages add to sixty percent of the shared attack subtotal once"), Snapshot.AttributesBeforeRoute, Stats(181, 51, 72, 38, 35));
	TestEqual(TEXT("six attack affixes aggregate to 6000 BP"), Snapshot.UniversalModifiers.FindRef(EGameXXKEquipmentModifierKind::Attack), 6000);
	TestEqual(TEXT("set percentage affixes remain additive descriptors"), Snapshot.SetModifiers.FindRef(EGameXXKEquipmentModifierKind::DirectDamage), 6000);
	TestEqual(TEXT("flat-count set affixes remain exact integers"), Snapshot.SetModifiers.FindRef(EGameXXKEquipmentModifierKind::ArmorBreakStacks), 12);
	TestEqual(TEXT("set-affix aggregates plus personal 2/4/6 effects are declarative"), Snapshot.ActivePersonalEffects.Num(), 5);
	TestNotNull(TEXT("four-piece trigger is declarative"), FindEffect(Snapshot.ActivePersonalEffects, TEXT("Set.PoJun.4")));

	FGameXXKEquipmentCollectionState Legacy;
	AddLegacyItem(Legacy, Hero, TEXT("Item.WoodenSword"), EGameXXKEquipmentSlot::Weapon, 2);
	AddLegacyItem(Legacy, Hero, TEXT("Item.StarterClothArmor"), EGameXXKEquipmentSlot::Armor, 3);
	AddLegacyItem(Legacy, Hero, TEXT("Item.ClothTalisman"), EGameXXKEquipmentSlot::Accessory, 4);
	TestTrue(TEXT("legacy loadout fixture validates"), FGameXXKEquipmentRules::ValidateCollectionState(Legacy));
	FGameXXKEquipmentLoadoutSnapshot LegacySnapshot;
	TestTrue(TEXT("legacy projection builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(Legacy, Hero, Bare, LegacySnapshot, &Error));
	TestStats(*this, TEXT("legacy equipment retains slot-specific flat enhancement"), LegacySnapshot.EnhancedEquipmentBaseStats, Stats(10, 0, 5, 6, 4));
	TestStats(*this, TEXT("legacy final attributes preserve old behavior"), LegacySnapshot.AttributesBeforeRoute, Stats(111, 51, 16, 13, 7));

	TMap<EGameXXKEquipmentModifierKind, int32> PostBasisPoints;
	PostBasisPoints.Add(EGameXXKEquipmentModifierKind::Attack, 1000);
	TMap<EGameXXKEquipmentModifierKind, int32> PostFlatCounts;
	PostFlatCounts.Add(EGameXXKEquipmentModifierKind::MaxHealth, 5);
	const FGameXXKCharacterStats Post = FGameXXKEquipmentRules::ApplyPostEquipmentModifiers(
		Snapshot.AttributesBeforeRoute,
		PostBasisPoints,
		PostFlatCounts);
	TestStats(*this, TEXT("route/relic/terrain/status modifiers apply only through explicit post call"), Post, Stats(186, 51, 79, 38, 35));
	TestStats(*this, TEXT("explicit post call never mutates the pre-route snapshot"), Snapshot.AttributesBeforeRoute, Stats(181, 51, 72, 38, 35));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentCrossLayerAdditiveTest,
	"GameXXK.Equipment.Stats.CrossLayerAdditiveAndDescriptorSplit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentCrossLayerAdditiveTest::RunTest(const FString& Parameters)
{
	const FName Hero = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKEquipmentCollectionState Collection;
	for (const EGameXXKEquipmentSlot Slot : {EGameXXKEquipmentSlot::Weapon, EGameXXKEquipmentSlot::Head})
	{
		AddModernItem(
			Collection,
			Hero,
			EGameXXKEquipmentSet::ZhuiFeng,
			Slot,
			EGameXXKEquipmentQuality::Common,
			1,
			0,
			Affix(TEXT("Affix.Universal.Speed"), 500, EGameXXKEquipmentMagnitudeUnit::BasisPoints));
	}
	TestTrue(TEXT("two-piece cross-layer fixture validates"), FGameXXKEquipmentRules::ValidateCollectionState(Collection));

	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	TestTrue(TEXT("two-piece cross-layer projection builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
		Collection,
		Hero,
		Stats(100, 40, 10, 10, 200),
		Snapshot));
	TestEqual(TEXT("legacy universal Speed remains readable without any retired ZhuiFeng speed passive"), Snapshot.AttributesBeforeRoute.Speed, 220);
	TestEqual(TEXT("universal speed basis points remain separately inspectable"), Snapshot.UniversalModifiers.FindRef(EGameXXKEquipmentModifierKind::Speed), 1000);
	const FGameXXKEquipmentActiveEffect* PairDraw = FindEffect(Snapshot.CandidateTeamEffects, TEXT("Set.ZhuiFeng.2"));
	TestNotNull(TEXT("ZhuiFeng two-piece emits the approved team card-count descriptor"), PairDraw);
	if (PairDraw)
	{
		TestEqual(TEXT("pair-draw uses the combined ZhuiFeng cycle modifier"), PairDraw->ModifierKind, EGameXXKEquipmentModifierKind::ZhuiFengCycle);
		TestEqual(TEXT("pair-draw is a flat event count"), PairDraw->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
		TestEqual(TEXT("pair-draw grants one card"), PairDraw->Magnitude, 1);
		TestEqual(TEXT("pair-draw listens to active card count"), PairDraw->Hook, EGameXXKEquipmentSetBonusHook::ZhuiFengActiveCardCount);
	}
	TestFalse(TEXT("no active descriptor can apply projected speed a second time"), Snapshot.ActivePersonalEffects.ContainsByPredicate(
		[](const FGameXXKEquipmentActiveEffect& Effect)
		{
			return Effect.ModifierKind == EGameXXKEquipmentModifierKind::Speed;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSetAndTeamProjectionTest,
	"GameXXK.Equipment.Stats.MixedSetsAndTeamSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSetAndTeamProjectionTest::RunTest(const FString& Parameters)
{
	FGameXXKEquipmentCollectionState Collection;
	const FName Hero = FGameXXKEquipmentRules::HeroCharacterId();
	const FName CompanionA(TEXT("Companion.A"));
	const FName CompanionB(TEXT("Companion.B"));
	const FName CompanionC(TEXT("Companion.C"));

	FGameXXKEquipmentCollectionState Mixed;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const EGameXXKEquipmentQuality Quality = static_cast<EGameXXKEquipmentQuality>(1 + Index % 3);
		AddModernItem(
			Mixed,
			Hero,
			EGameXXKEquipmentSet::XuanJia,
			AllSlots[Index],
			Quality,
			1,
			Index % 2,
			Affix(TEXT("Affix.Universal.MaxHealth"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints));
	}
	AppendCollection(Collection, MoveTemp(Mixed));
	AppendCollection(Collection, MakeSixPieceCollection(CompanionA, EGameXXKEquipmentSet::XuanJia, EGameXXKEquipmentQuality::Rare, 2, false));
	AppendCollection(Collection, MakeSixPieceCollection(CompanionB, EGameXXKEquipmentSet::XuanJia, EGameXXKEquipmentQuality::Rare, 2, false));
	AppendCollection(Collection, MakeSixPieceCollection(CompanionC, EGameXXKEquipmentSet::ShanHe, EGameXXKEquipmentQuality::Common, 0, false));
	TestTrue(TEXT("multi-character team fixture validates"), FGameXXKEquipmentRules::ValidateCollectionState(Collection));

	TArray<FGameXXKEquipmentLoadoutSnapshot> Snapshots;
	for (const FName CharacterId : {Hero, CompanionA, CompanionB, CompanionC})
	{
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		TestTrue(TEXT("each team member snapshot builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(Collection, CharacterId, Stats(100, 20, 10, 10, 10), Snapshot));
		Snapshots.Add(MoveTemp(Snapshot));
	}
	TestEqual(TEXT("mixed qualities all count toward the same six-piece set"), Snapshots[0].SetPieceCounts.FindRef(EGameXXKEquipmentSet::XuanJia), 6);
	TestEqual(TEXT("mixed qualities expose personal two/four effects together"), Snapshots[0].ActivePersonalEffects.Num(), 2);
	TestEqual(TEXT("mixed score sums quality*10 plus enhancement"), Snapshots[0].TeamEffectSourceScore, 123);
	TestEqual(TEXT("equal rare +2 loadouts have the same combined score"), Snapshots[1].TeamEffectSourceScore, 132);
	TestEqual(TEXT("equal rare +2 loadouts have the same combined score again"), Snapshots[2].TeamEffectSourceScore, 132);

	const TArray<FGameXXKEquipmentActiveEffect> TeamEffects = FGameXXKEquipmentRules::ResolveTeamEffects(Snapshots);
	TestEqual(TEXT("same-name team six-piece resolves once while another set coexists"), TeamEffects.Num(), 2);
	const FGameXXKEquipmentActiveEffect* XuanJia = FindEffect(TeamEffects, TEXT("Set.XuanJia.6"));
	const FGameXXKEquipmentActiveEffect* ShanHe = FindEffect(TeamEffects, TEXT("Set.ShanHe.6"));
	TestNotNull(TEXT("XuanJia team effect survives deduplication"), XuanJia);
	TestNotNull(TEXT("different ShanHe team effect coexists"), ShanHe);
	const FGameXXKEquipmentSetBonusDefinition* Xuan2 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.2"));
	const FGameXXKEquipmentSetBonusDefinition* Xuan4 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.4"));
	const FGameXXKEquipmentSetBonusDefinition* Xuan6 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.6"));
	const FGameXXKEquipmentSetBonusDefinition* Shan2 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.2"));
	const FGameXXKEquipmentSetBonusDefinition* Shan4 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.4"));
	const FGameXXKEquipmentSetBonusDefinition* Shan6 = FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.6"));
	TestNotNull(TEXT("Xuanjia two-piece definition exists"), Xuan2);
	TestNotNull(TEXT("Xuanjia four-piece definition exists"), Xuan4);
	TestNotNull(TEXT("Xuanjia six-piece definition exists"), Xuan6);
	TestNotNull(TEXT("Shanhe two-piece definition exists"), Shan2);
	TestNotNull(TEXT("Shanhe four-piece definition exists"), Shan4);
	TestNotNull(TEXT("Shanhe six-piece definition exists"), Shan6);
	if (Xuan2 && Xuan4 && Xuan6 && Shan2 && Shan4 && Shan6)
	{
		TestEqual(TEXT("Xuanjia two-piece uses ten-percent Armor generation"), Xuan2->Value, 1000);
		TestEqual(TEXT("Xuanjia four-piece retains fifty percent"), Xuan4->Value, 5000);
		TestEqual(TEXT("Xuanjia four-piece adds eighty-percent attack"), Xuan4->SecondaryValue, 80);
		TestEqual(TEXT("Xuanjia six-piece grants forty-percent Defense Armor"), Xuan6->Value, 4000);
		TestEqual(TEXT("Xuanjia six-piece grants one Guard link"), Xuan6->SecondaryValue, 1);
		TestEqual(TEXT("Shanhe two-piece draws one"), Shan2->Value, 1);
		TestEqual(TEXT("Shanhe four-piece discounts one"), Shan4->Value, 1);
		TestEqual(TEXT("Shanhe four-piece restores two Mana"), Shan4->SecondaryValue, 2);
		TestEqual(TEXT("Shanhe six-piece triggers once"), Shan6->Value, 1);
		TestEqual(TEXT("Xuanjia two-piece player text"), Xuan2->Description.ToString(), FString(TEXT("穿戴者产生的护甲提高10%。")));
		TestEqual(TEXT("Xuanjia four-piece player text"), Xuan4->Description.ToString(), FString(TEXT("我方回合开始时保留50%护甲；每个敌方回合首次格挡后，追加80%攻击伤害。")));
		TestEqual(TEXT("Xuanjia six-piece player text"), Xuan6->Description.ToString(), FString(TEXT("全队唯一。敌方回合首次有友方因攻击损失气血后，全体获得穿戴者40%防御的护甲；穿戴者援护其他友方各1次。")));
		TestEqual(TEXT("Shanhe two-piece player text"), Shan2->Description.ToString(), FString(TEXT("每回合首次打出地势牌后，抽1张。")));
		TestEqual(TEXT("Shanhe four-piece player text"), Shan4->Description.ToString(), FString(TEXT("每回合首张地势牌少耗1气；结算后，其他友方各回复2内力。")));
		TestEqual(TEXT("Shanhe six-piece player text"), Shan6->Description.ToString(), FString(TEXT("全队唯一。每个我方回合开始时，由穿戴者触发1次当前地势。")));
		const FString ApprovedText = Xuan2->Description.ToString()
			+ Xuan4->Description.ToString()
			+ Xuan6->Description.ToString()
			+ Shan2->Description.ToString()
			+ Shan4->Description.ToString()
			+ Shan6->Description.ToString();
		TestFalse(TEXT("approved set text contains no retired five-percent payload"), ApprovedText.Contains(TEXT("5%")));
		TestFalse(TEXT("approved set text contains no retired twelve-percent payload"), ApprovedText.Contains(TEXT("12%")));
		TestFalse(TEXT("approved set text contains no retired adjacent-ally wording"), ApprovedText.Contains(TEXT("相邻队友")));
	}
	if (XuanJia)
	{
		TestEqual(TEXT("score tie uses stable lexical CharacterId"), XuanJia->SourceCharacterId, CompanionA);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentTooltipProjectionTest,
	"GameXXK.Equipment.Stats.FullLoadoutTooltip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentTooltipProjectionTest::RunTest(const FString& Parameters)
{
	const FName Hero = FGameXXKEquipmentRules::HeroCharacterId();
	FGameXXKEquipmentCollectionState Collection;
	FName CurrentWeapon;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const EGameXXKEquipmentSet Set = Index == 1 ? EGameXXKEquipmentSet::ZhuiFeng : EGameXXKEquipmentSet::PoJun;
		const FGameXXKEquipmentAffixRoll Roll = Index == 0
			? Affix(TEXT("Affix.Universal.MaxHealth"), 500, EGameXXKEquipmentMagnitudeUnit::BasisPoints)
			: Affix(TEXT("Affix.Universal.MaxMana"), 300, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
		const FName Id = AddModernItem(Collection, Hero, Set, AllSlots[Index], EGameXXKEquipmentQuality::Common, 1, 0, Roll);
		if (Index == 0)
		{
			CurrentWeapon = Id;
		}
	}
	const FName CandidateWeapon = AddModernItem(
		Collection,
		Hero,
		EGameXXKEquipmentSet::ZhuiFeng,
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentQuality::Common,
		1,
		2,
		Affix(TEXT("Affix.Universal.Attack"), 500, EGameXXKEquipmentMagnitudeUnit::BasisPoints),
		true,
		TEXT("Candidate"));
	TestTrue(TEXT("complete loadout plus warehouse candidate validates"), FGameXXKEquipmentRules::ValidateCollectionState(Collection));
	const TArray<uint8> Before = SerializeCollection(Collection);

	const FGameXXKCharacterStats BareA = Stats(100, 40, 20, 10, 20);
	FGameXXKEquipmentTooltipSnapshot TooltipA;
	FString Error;
	TestTrue(TEXT("full-loadout candidate tooltip builds"), FGameXXKEquipmentRules::BuildTooltipSnapshot(Collection, CandidateWeapon, Hero, BareA, TooltipA, &Error));
	TestEqual(TEXT("tooltip retains candidate identity"), TooltipA.InstanceId, CandidateWeapon);
	TestEqual(TEXT("tooltip resolves candidate slot"), TooltipA.Slot, EGameXXKEquipmentSlot::Weapon);
	TestEqual(TEXT("tooltip retains candidate quality"), TooltipA.Quality, EGameXXKEquipmentQuality::Common);
	TestEqual(TEXT("tooltip retains item level"), TooltipA.ItemLevel, 1);
	TestEqual(TEXT("tooltip retains enhancement"), TooltipA.EnhancementLevel, 2);
	TestEqual(TEXT("tooltip retains affix units"), TooltipA.Affixes[0].Unit, EGameXXKEquipmentMagnitudeUnit::BasisPoints);
	TestEqual(TEXT("current complete loadout has five PoJun pieces"), TooltipA.CurrentSetPieceCounts.FindRef(EGameXXKEquipmentSet::PoJun), 5);
	TestEqual(TEXT("candidate complete loadout crosses ZhuiFeng two-piece threshold"), TooltipA.CandidateSetPieceCounts.FindRef(EGameXXKEquipmentSet::ZhuiFeng), 2);
	TestEqual(TEXT("legal warehouse swap reports no transaction error"), TooltipA.EquipError, EGameXXKEquipmentTransactionError::None);
	TestStats(*this, TEXT("tooltip delta is candidate complete stats minus current complete stats"), TooltipA.CharacterStatDeltas, Stats(-1, 0, 3, 2, 2));

	const FGameXXKCharacterStats BareB = Stats(100, 40, 220, 10, 220);
	FGameXXKEquipmentTooltipSnapshot TooltipB;
	TestTrue(TEXT("same candidate supports a second authoritative naked-stat input"), FGameXXKEquipmentRules::BuildTooltipSnapshot(Collection, CandidateWeapon, Hero, BareB, TooltipB, &Error));
	TestTrue(TEXT("percentage-derived attack delta changes with CompareBareStats"), TooltipB.CharacterStatDeltas.Attack > TooltipA.CharacterStatDeltas.Attack);
	TestEqual(TEXT("retired passive set speed no longer scales the tooltip delta with CompareBareStats"), TooltipB.CharacterStatDeltas.Speed, TooltipA.CharacterStatDeltas.Speed);
	TestEqual(TEXT("tooltip projection never mutates the source collection"), SerializeCollection(Collection), Before);

	FGameXXKEquipmentTooltipSnapshot AlreadyEquipped;
	TestTrue(TEXT("already equipped same-slot tooltip builds"), FGameXXKEquipmentRules::BuildTooltipSnapshot(Collection, CurrentWeapon, Hero, BareA, AlreadyEquipped, &Error));
	TestEqual(TEXT("already-equipped same-slot preview reports the shared Task3 transaction blocker"), AlreadyEquipped.EquipError, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
	TestStats(*this, TEXT("already-equipped item has zero five-stat delta"), AlreadyEquipped.CharacterStatDeltas, Stats(0, 0, 0, 0, 0));
	TestStats(*this, TEXT("already-equipped item keeps current and candidate final stats identical"), AlreadyEquipped.CandidateCharacterStats, AlreadyEquipped.CurrentCharacterStats);

	FGameXXKEquipmentTooltipSnapshot InvalidBare;
	TestFalse(TEXT("zero CompareBareStats is rejected rather than fabricated"), FGameXXKEquipmentRules::BuildTooltipSnapshot(Collection, CandidateWeapon, Hero, FGameXXKCharacterStats(), InvalidBare, &Error));
	return true;
}

#endif
