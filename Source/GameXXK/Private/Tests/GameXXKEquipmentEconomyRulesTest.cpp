#include "Misc/AutomationTest.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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

	FGameXXKRuntimeState MakeState(const int32 EnhancementStones = 0, const int32 RefinementSand = 0)
	{
		FGameXXKRuntimeState State;
		State.PlayerLevel = 1;
		State.PlayerHP = 100;
		State.PlayerMaxHP = 100;
		State.PlayerMP = 30;
		State.PlayerMaxMP = 30;
		State.PlayerAttack = 15;
		State.PlayerDefense = 8;
		State.PlayerSpeed = 10;
		State.Inventory.Add(UGameXXKMVPRules::ItemEnhancementStone(), EnhancementStones);
		State.Inventory.Add(UGameXXKMVPRules::ItemRefinementSand(), RefinementSand);
		State.EnhancementMaterial = EnhancementStones;
		State.EquipmentCollection.CollectionSeed = 0x2157;
		State.EquipmentCollection.RefinementSand = RefinementSand;
		return State;
	}

	FName CreateModern(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentQuality Quality,
		const EGameXXKEquipmentSlot Slot,
		const EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::PoJun,
		const int32 ItemLevel = 1)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = Set;
		Request.Quality = Quality;
		Request.ItemLevel = ItemLevel;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;
		FName InstanceId;
		FString Error;
		if (!FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error))
		{
			Test.AddError(FString::Printf(TEXT("modern fixture creation failed: %s"), *Error));
		}
		return InstanceId;
	}

	FName AddLegacyWarehouse(
		FGameXXKRuntimeState& State,
		const FName BaseEquipmentId,
		const int32 EnhancementLevel,
		const FString& Suffix)
	{
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseEquipmentId);
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(TEXT("EquipmentInstance.Economy.Legacy.%s"), *Suffix));
		Instance.BaseEquipmentId = BaseEquipmentId;
		Instance.ItemLevel = 1;
		Instance.Quality = EGameXXKEquipmentQuality::Common;
		Instance.EnhancementLevel = EnhancementLevel;
		Instance.ScalingRule = Definition ? Definition->ScalingRule : EGameXXKEquipmentScalingRule::Invalid;
		Instance.LegacyBaseStatSnapshot = Definition ? Definition->LegacyBaseStatSnapshot : FGameXXKCharacterStats();
		Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
		const FName InstanceId = Instance.InstanceId;
		State.EquipmentCollection.EquipmentInstances.Add(MoveTemp(Instance));
		State.EquipmentCollection.WarehouseInstanceIds.Add(InstanceId);
		return InstanceId;
	}

	FGameXXKEquipmentInstance* FindMutableInstance(FGameXXKRuntimeState& State, const FName InstanceId)
	{
		return State.EquipmentCollection.EquipmentInstances.FindByPredicate(
			[InstanceId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
	}

	TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool DeserializeRuntimeState(const TArray<uint8>& Bytes, FGameXXKRuntimeState& OutState)
	{
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &OutState, nullptr);
		return !Archive.IsError();
	}

	void TestRuntimeUnchanged(
		FAutomationTestBase& Test,
		const FString& Label,
		const TArray<uint8>& Before,
		const FGameXXKRuntimeState& After)
	{
		Test.TestEqual(Label, SerializeRuntimeState(After), Before);
	}

	bool AffixRollsEqual(const FGameXXKEquipmentAffixRoll& A, const FGameXXKEquipmentAffixRoll& B)
	{
		return A.AffixId == B.AffixId
			&& A.Tier == B.Tier
			&& A.Magnitude == B.Magnitude
			&& A.Unit == B.Unit;
	}

	void TestHeroMatchesProjection(FAutomationTestBase& Test, const FString& Label, const FGameXXKRuntimeState& State)
	{
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		FString Error;
		const FGameXXKCharacterStats Bare = FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel);
		Test.TestTrue(Label + TEXT(" projection builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			State.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			Bare,
			Snapshot,
			&Error));
		Test.TestEqual(Label + TEXT(" max health mirror"), State.PlayerMaxHP, Snapshot.AttributesBeforeRoute.MaxHealth);
		Test.TestEqual(Label + TEXT(" max mana mirror"), State.PlayerMaxMP, Snapshot.AttributesBeforeRoute.MaxMana);
		Test.TestEqual(Label + TEXT(" attack mirror"), State.PlayerAttack, Snapshot.AttributesBeforeRoute.Attack);
		Test.TestEqual(Label + TEXT(" defense mirror"), State.PlayerDefense, Snapshot.AttributesBeforeRoute.Defense);
		Test.TestEqual(Label + TEXT(" speed mirror"), State.PlayerSpeed, Snapshot.AttributesBeforeRoute.Speed);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyTenQualityCompatibilityTest,
	"GameXXK.Equipment.Economy.TenQualityCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyTenQualityCompatibilityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("invalid quality has no reforge cost"),
		FGameXXKEquipmentCatalog::GetReforgeSandCost(EGameXXKEquipmentQuality::Invalid), 0);
	TestEqual(TEXT("unknown quality has no reforge cost"),
		FGameXXKEquipmentCatalog::GetReforgeSandCost(static_cast<EGameXXKEquipmentQuality>(11)), 0);
	const EGameXXKEquipmentQuality Qualities[] = {
		EGameXXKEquipmentQuality::Common,
		EGameXXKEquipmentQuality::Rare,
		EGameXXKEquipmentQuality::Epic,
		EGameXXKEquipmentQuality::Legendary,
		EGameXXKEquipmentQuality::Immortal,
		EGameXXKEquipmentQuality::Treasure,
		EGameXXKEquipmentQuality::Transcendent,
		EGameXXKEquipmentQuality::Celestial,
		EGameXXKEquipmentQuality::Ascendant,
		EGameXXKEquipmentQuality::Cosmic,
	};
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Qualities)); ++Index)
	{
		const int32 Rank = Index + 1;
		const EGameXXKEquipmentQuality Quality = Qualities[Index];
		TestEqual(FString::Printf(TEXT("quality %d reforge keeps the one-sand compatibility cost"), Rank), FGameXXKEquipmentCatalog::GetReforgeSandCost(Quality), 1);
		TestEqual(FString::Printf(TEXT("quality %d dismantle keeps the one-sand compatibility yield"), Rank), FGameXXKEquipmentCatalog::GetDismantleSandYield(Quality), 1);

		FGameXXKRuntimeState State = MakeState(0, 2);
		State.EquipmentCollection.CollectionSeed += Rank;
		const FName InstanceId = CreateModern(
			*this,
			State,
			Quality,
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKEquipmentSet::QingNang,
			Rank);
		FGameXXKEquipmentTransactionResult Result;
		TestTrue(FString::Printf(TEXT("quality %d can begin a paid reforge"), Rank), FGameXXKEquipmentEconomyRules::BeginReforge(State, InstanceId, 0, Result));
		const FGameXXKEquipmentAffixRoll& Candidate = State.EquipmentCollection.PendingReforge.CandidateAffix;
		const FGameXXKAffixTierWeights Weights = FGameXXKAffixCatalog::GetTierWeights(Quality);
		TestTrue(FString::Printf(TEXT("quality %d reforge chooses a weighted tier"), Rank), Weights.GetWeight(Candidate.Tier) > 0);
		TestTrue(FString::Printf(TEXT("quality %d reforge tier does not exceed quality"), Rank), FGameXXKEquipmentQualityRules::GetRank(Candidate.Tier) <= Rank);
		const FGameXXKAffixMagnitudeRange Range = FGameXXKAffixCatalog::GetMagnitudeRange(Candidate.Unit, Candidate.Tier);
		TestTrue(FString::Printf(TEXT("quality %d reforge magnitude stays in the exact tier range"), Rank), Candidate.Magnitude >= Range.Minimum && Candidate.Magnitude <= Range.Maximum);
		TestTrue(FString::Printf(TEXT("quality %d paid preview remains collection-valid"), Rank), FGameXXKEquipmentRules::ValidateCollectionState(State.EquipmentCollection));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyEnhancementTest,
	"GameXXK.Equipment.Economy.EnhancementAndRuntimeWrappers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyEnhancementTest::RunTest(const FString& Parameters)
{
	for (int32 Level = 0; Level < FGameXXKEquipmentRules::MaxEnhancementLevel; ++Level)
	{
		TestEqual(FString::Printf(TEXT("enhancement +%d to +%d costs one stone"), Level, Level + 1),
			FGameXXKEquipmentCatalog::GetEnhancementStoneCost(Level), 1);
	}
	TestEqual(TEXT("max enhancement has no next-step cost"),
		FGameXXKEquipmentCatalog::GetEnhancementStoneCost(FGameXXKEquipmentRules::MaxEnhancementLevel), 0);

	FGameXXKRuntimeState State = MakeState(10);
	const FName ModernWeapon = CreateModern(*this, State, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Weapon);
	const FGameXXKEquipmentInstance* InitialInstance = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, ModernWeapon);
	const FName ModernWeaponBaseId = InitialInstance ? InitialInstance->BaseEquipmentId : NAME_None;
	const TArray<FGameXXKEquipmentAffixRoll> InitialAffixes = InitialInstance ? InitialInstance->RolledAffixes : TArray<FGameXXKEquipmentAffixRoll>();
	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("runtime wrapper equips a modern hero weapon"), FGameXXKEquipmentEconomyRules::Equip(
		State, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, ModernWeapon, Result));
	TestTrue(TEXT("equip result succeeds"), Result.bSucceeded);
	TestEqual(TEXT("modern hero weapon synchronizes the compatibility slot"), State.EquippedWeapon, ModernWeaponBaseId);
	TestHeroMatchesProjection(*this, TEXT("equipped modern hero"), State);

	for (int32 TargetLevel = 1; TargetLevel <= FGameXXKEquipmentRules::MaxEnhancementLevel; ++TargetLevel)
	{
		TestTrue(FString::Printf(TEXT("enhancement reaches +%d"), TargetLevel),
			FGameXXKEquipmentEconomyRules::EnhanceInstance(State, ModernWeapon, Result));
		const FGameXXKEquipmentInstance* Enhanced = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, ModernWeapon);
		TestEqual(FString::Printf(TEXT("instance stores +%d"), TargetLevel), Enhanced ? Enhanced->EnhancementLevel : INDEX_NONE, TargetLevel);
		TestEqual(FString::Printf(TEXT("+%d spends exactly one authoritative stone"), TargetLevel), Result.EnhancementStoneDelta, -1);
		TestEqual(FString::Printf(TEXT("+%d inventory stone balance"), TargetLevel),
			State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()), 10 - TargetLevel);
		TestEqual(FString::Printf(TEXT("+%d legacy material mirror"), TargetLevel), State.EnhancementMaterial, 10 - TargetLevel);
	}
	const FGameXXKEquipmentInstance* PlusTen = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, ModernWeapon);
	TestEqual(TEXT("modern enhancement never changes affix count"), PlusTen ? PlusTen->RolledAffixes.Num() : INDEX_NONE, InitialAffixes.Num());
	if (PlusTen && PlusTen->RolledAffixes.Num() == InitialAffixes.Num())
	{
		for (int32 Index = 0; Index < InitialAffixes.Num(); ++Index)
		{
			TestTrue(FString::Printf(TEXT("modern enhancement preserves affix %d"), Index), AffixRollsEqual(PlusTen->RolledAffixes[Index], InitialAffixes[Index]));
		}
	}
	FGameXXKEquipmentLoadoutSnapshot PlusTenSnapshot;
	TestTrue(TEXT("+10 modern snapshot builds"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
		State.EquipmentCollection,
		FGameXXKEquipmentRules::HeroCharacterId(),
		FGameXXKCharacterStatRules::GetBareHeroStats(1),
		PlusTenSnapshot));
	TestEqual(TEXT("+10 modern weapon doubles its level-one base and gains ten flat enhancement attack"), PlusTenSnapshot.EnhancedEquipmentBaseStats.Attack, 14);
	TestHeroMatchesProjection(*this, TEXT("+10 modern hero"), State);

	const TArray<uint8> AtMaximum = SerializeRuntimeState(State);
	TestFalse(TEXT("enhancement cannot exceed +10"), FGameXXKEquipmentEconomyRules::EnhanceInstance(State, ModernWeapon, Result));
	TestEqual(TEXT("max enhancement returns the typed error"), Result.Error, EGameXXKEquipmentTransactionError::MaxEnhancementReached);
	TestRuntimeUnchanged(*this, TEXT("max enhancement rolls back the complete runtime"), AtMaximum, State);

	FGameXXKRuntimeState NoStone = MakeState(0);
	NoStone.EnhancementMaterial = 77;
	const FName NoStoneItem = CreateModern(*this, NoStone, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Head);
	const TArray<uint8> BeforeNoStone = SerializeRuntimeState(NoStone);
	TestFalse(TEXT("authoritative inventory blocks enhancement without stones"),
		FGameXXKEquipmentEconomyRules::EnhanceInstance(NoStone, NoStoneItem, Result));
	TestEqual(TEXT("missing stones return the typed error"), Result.Error, EGameXXKEquipmentTransactionError::InsufficientEnhancementStones);
	TestRuntimeUnchanged(*this, TEXT("failed enhancement preserves even a stale compatibility mirror"), BeforeNoStone, NoStone);

	FGameXXKRuntimeState Legacy = MakeState(6);
	const FName LegacyWeapon = AddLegacyWarehouse(Legacy, TEXT("Item.WoodenSword"), 0, TEXT("Weapon"));
	const FName LegacyArmor = AddLegacyWarehouse(Legacy, TEXT("Item.StarterClothArmor"), 0, TEXT("Armor"));
	const FName LegacyAccessory = AddLegacyWarehouse(Legacy, TEXT("Item.ClothTalisman"), 0, TEXT("Accessory"));
	TestTrue(TEXT("legacy weapon equips through the full-state wrapper"), FGameXXKEquipmentEconomyRules::Equip(
		Legacy, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, LegacyWeapon, Result));
	TestTrue(TEXT("legacy armor equips through the full-state wrapper"), FGameXXKEquipmentEconomyRules::Equip(
		Legacy, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Armor, LegacyArmor, Result));
	TestTrue(TEXT("legacy accessory equips through the full-state wrapper"), FGameXXKEquipmentEconomyRules::Equip(
		Legacy, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Accessory, LegacyAccessory, Result));
	for (const FName InstanceId : {LegacyWeapon, LegacyArmor, LegacyAccessory})
	{
		TestTrue(TEXT("legacy equipment enhances to +1"), FGameXXKEquipmentEconomyRules::EnhanceInstance(Legacy, InstanceId, Result));
		TestTrue(TEXT("legacy equipment enhances to +2"), FGameXXKEquipmentEconomyRules::EnhanceInstance(Legacy, InstanceId, Result));
	}
	const FGameXXKCharacterStats Bare = FGameXXKCharacterStatRules::GetBareHeroStats(1);
	TestEqual(TEXT("legacy weapon keeps +1 attack per enhancement"), Legacy.PlayerAttack, Bare.Attack + 3 + 2);
	TestEqual(TEXT("legacy armor keeps +1 defense per enhancement"), Legacy.PlayerDefense, Bare.Defense + 3 + 2);
	TestEqual(TEXT("legacy accessory keeps +1 speed per enhancement"), Legacy.PlayerSpeed, Bare.Speed + 2);
	TestEqual(TEXT("legacy accessory preserves its flat base health"), Legacy.PlayerMaxHP, Bare.MaxHealth + 10);
	TestEqual(TEXT("legacy weapon compatibility enhancement mirror"), Legacy.ItemEnhancementLevels.FindRef(TEXT("Item.WoodenSword")), 2);
	TestEqual(TEXT("legacy armor compatibility enhancement mirror"), Legacy.ItemEnhancementLevels.FindRef(TEXT("Item.StarterClothArmor")), 2);
	TestEqual(TEXT("legacy accessory compatibility enhancement mirror"), Legacy.ItemEnhancementLevels.FindRef(TEXT("Item.ClothTalisman")), 2);
	TestHeroMatchesProjection(*this, TEXT("enhanced legacy hero"), Legacy);

	TestTrue(TEXT("runtime wrapper unequips the hero accessory"), FGameXXKEquipmentEconomyRules::Unequip(
		Legacy, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Accessory, Result));
	TestTrue(TEXT("unequip result succeeds"), Result.bSucceeded);
	TestTrue(TEXT("legacy accessory compatibility slot clears"), Legacy.EquippedAccessory.IsNone());
	TestEqual(TEXT("unequipped legacy accessory leaves the compatibility enhancement map"), Legacy.ItemEnhancementLevels.FindRef(TEXT("Item.ClothTalisman")), 0);
	TestHeroMatchesProjection(*this, TEXT("legacy hero after unequip"), Legacy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyReforgeTest,
	"GameXXK.Equipment.Economy.PaidReforgeTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyReforgeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("common reforge costs one sand"), FGameXXKEquipmentCatalog::GetReforgeSandCost(EGameXXKEquipmentQuality::Common), 1);
	TestEqual(TEXT("rare reforge costs one sand"), FGameXXKEquipmentCatalog::GetReforgeSandCost(EGameXXKEquipmentQuality::Rare), 1);
	TestEqual(TEXT("epic reforge costs one sand"), FGameXXKEquipmentCatalog::GetReforgeSandCost(EGameXXKEquipmentQuality::Epic), 1);

	FGameXXKRuntimeState SeedState = MakeState(0, 200);
	const FName EpicItem = CreateModern(*this, SeedState, EGameXXKEquipmentQuality::Epic, EGameXXKEquipmentSlot::Weapon, EGameXXKEquipmentSet::QingNang, 8);
	FGameXXKRuntimeState LegacySpeedState = SeedState;
	FGameXXKEquipmentInstance* LegacySpeedInstance = FindMutableInstance(LegacySpeedState, EpicItem);
	TestNotNull(TEXT("legacy Speed reforge fixture resolves"), LegacySpeedInstance);
	if (LegacySpeedInstance)
	{
		FGameXXKEquipmentAffixRoll& LegacySpeed = LegacySpeedInstance->RolledAffixes[0];
		LegacySpeed.AffixId = TEXT("Affix.Universal.Speed");
		LegacySpeed.Tier = EGameXXKAffixTier::Common;
		LegacySpeed.Magnitude = 300;
		LegacySpeed.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
		FGameXXKEquipmentTransactionResult LegacySpeedResult;
		TestTrue(TEXT("an existing legacy Speed affix remains eligible for paid replacement"),
			FGameXXKEquipmentEconomyRules::BeginReforge(LegacySpeedState, EpicItem, 0, LegacySpeedResult));
		TestFalse(TEXT("paid replacement never proposes the retired Speed affix again"),
			LegacySpeedState.EquipmentCollection.PendingReforge.CandidateAffix.AffixId == FName(TEXT("Affix.Universal.Speed")));
	}
	FGameXXKRuntimeState A = SeedState;
	FGameXXKRuntimeState B = SeedState;
	FGameXXKEquipmentTransactionResult ResultA;
	FGameXXKEquipmentTransactionResult ResultB;
	TestTrue(TEXT("paid reforge preview begins"), FGameXXKEquipmentEconomyRules::BeginReforge(A, EpicItem, 0, ResultA));
	TestTrue(TEXT("identical seed reforge preview begins"), FGameXXKEquipmentEconomyRules::BeginReforge(B, EpicItem, 0, ResultB));
	const FGameXXKPendingEquipmentReforge& PendingA = A.EquipmentCollection.PendingReforge;
	const FGameXXKPendingEquipmentReforge& PendingB = B.EquipmentCollection.PendingReforge;
	TestTrue(TEXT("pending reforge stores a complete active preview"), PendingA.bActive);
	TestEqual(TEXT("pending reforge stores the selected instance"), PendingA.InstanceId, EpicItem);
	TestEqual(TEXT("pending reforge stores the selected affix index"), PendingA.AffixIndex, 0);
	TestEqual(TEXT("pending reforge stores its paid cost"), PendingA.PaidRefinementSand, 1);
	TestEqual(TEXT("pending reforge stores its consumed ordinal"), PendingA.ConsumedReforgeOrdinal, 0);
	TestEqual(TEXT("paid preview deducts sand immediately"), A.EquipmentCollection.RefinementSand, 199);
	TestEqual(TEXT("paid preview reports negative sand delta"), ResultA.RefinementSandDelta, -1);
	TestEqual(TEXT("paid preview advances the sequence once"), A.EquipmentCollection.NextReforgeOrdinal, 1);
	TestTrue(TEXT("fixed seed produces the same complete candidate"), AffixRollsEqual(PendingA.CandidateAffix, PendingB.CandidateAffix));
	TestFalse(TEXT("reforge replaces the selected affix with a new type"), PendingA.CandidateAffix.AffixId == PendingA.OriginalAffix.AffixId);
	TestTrue(TEXT("the stored preview keeps the complete collection valid"), FGameXXKEquipmentRules::ValidateCollectionState(A.EquipmentCollection));

	FGameXXKRuntimeState StaleSandMirror = SeedState;
	StaleSandMirror.EquipmentCollection.RefinementSand = 0;
	FGameXXKEquipmentTransactionResult StaleSandResult;
	TestTrue(TEXT("reforge accepts the authoritative backpack sand when the legacy mirror is stale"),
		FGameXXKEquipmentEconomyRules::BeginReforge(StaleSandMirror, EpicItem, 0, StaleSandResult));
	TestEqual(TEXT("reforge deducts one authoritative backpack sand"),
		StaleSandMirror.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()), 199);
	TestEqual(TEXT("reforge repairs the legacy sand mirror from the authoritative backpack balance"),
		StaleSandMirror.EquipmentCollection.RefinementSand, 199);

	const TArray<uint8> BeforeSecondPreview = SerializeRuntimeState(A);
	TestFalse(TEXT("a second preview is rejected"), FGameXXKEquipmentEconomyRules::BeginReforge(A, EpicItem, 1, ResultA));
	TestEqual(TEXT("second preview returns the typed pending error"), ResultA.Error, EGameXXKEquipmentTransactionError::PendingReforgeExists);
	TestRuntimeUnchanged(*this, TEXT("second preview advances neither sand nor ordinal"), BeforeSecondPreview, A);

	const TArray<uint8> ReloadBytes = SerializeRuntimeState(A);
	FGameXXKRuntimeState Reloaded;
	TestTrue(TEXT("paid preview survives a runtime-state save reload"), DeserializeRuntimeState(ReloadBytes, Reloaded));
	const FGameXXKEquipmentAffixRoll SavedCandidate = Reloaded.EquipmentCollection.PendingReforge.CandidateAffix;
	TestTrue(TEXT("reloaded preview can be accepted"), FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Reloaded, true, ResultA));
	const FGameXXKEquipmentInstance* Accepted = FGameXXKEquipmentRules::FindInstance(Reloaded.EquipmentCollection, EpicItem);
	TestTrue(TEXT("accept applies the saved candidate exactly"), Accepted && AffixRollsEqual(Accepted->RolledAffixes[0], SavedCandidate));
	TestFalse(TEXT("accept clears pending data"), Reloaded.EquipmentCollection.PendingReforge.bActive);
	TestEqual(TEXT("accept does not advance the sequence again"), Reloaded.EquipmentCollection.NextReforgeOrdinal, 1);
	TestEqual(TEXT("accept does not refund paid sand"), Reloaded.EquipmentCollection.RefinementSand, 199);

	TestTrue(TEXT("another paid preview can begin after accept"), FGameXXKEquipmentEconomyRules::BeginReforge(Reloaded, EpicItem, 0, ResultA));
	const FGameXXKEquipmentAffixRoll BeforeCancel = Reloaded.EquipmentCollection.PendingReforge.OriginalAffix;
	const int32 SandBeforeCancel = Reloaded.EquipmentCollection.RefinementSand;
	TestTrue(TEXT("paid preview can be cancelled"), FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Reloaded, false, ResultA));
	const FGameXXKEquipmentInstance* Cancelled = FGameXXKEquipmentRules::FindInstance(Reloaded.EquipmentCollection, EpicItem);
	TestTrue(TEXT("cancel retains the original affix"), Cancelled && AffixRollsEqual(Cancelled->RolledAffixes[0], BeforeCancel));
	TestEqual(TEXT("cancel never refunds sand"), Reloaded.EquipmentCollection.RefinementSand, SandBeforeCancel);
	TestFalse(TEXT("cancel clears pending data"), Reloaded.EquipmentCollection.PendingReforge.bActive);

	FGameXXKRuntimeState Insufficient = SeedState;
	Insufficient.EquipmentCollection.RefinementSand = 0;
	Insufficient.Inventory.FindOrAdd(UGameXXKMVPRules::ItemRefinementSand()) = 0;
	const TArray<uint8> BeforeInsufficient = SerializeRuntimeState(Insufficient);
	TestFalse(TEXT("insufficient sand blocks preview"), FGameXXKEquipmentEconomyRules::BeginReforge(Insufficient, EpicItem, 0, ResultA));
	TestEqual(TEXT("insufficient sand returns the typed error"), ResultA.Error, EGameXXKEquipmentTransactionError::InsufficientRefinementSand);
	TestRuntimeUnchanged(*this, TEXT("failed paid preview preserves state and sequence"), BeforeInsufficient, Insufficient);

	FGameXXKRuntimeState InvalidIndex = SeedState;
	const TArray<uint8> BeforeInvalidIndex = SerializeRuntimeState(InvalidIndex);
	TestFalse(TEXT("invalid affix index blocks preview"), FGameXXKEquipmentEconomyRules::BeginReforge(InvalidIndex, EpicItem, 99, ResultA));
	TestEqual(TEXT("invalid affix index returns invalid request"), ResultA.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(*this, TEXT("invalid affix index advances nothing"), BeforeInvalidIndex, InvalidIndex);

	FGameXXKRuntimeState ExhaustedOrdinal = SeedState;
	ExhaustedOrdinal.EquipmentCollection.NextReforgeOrdinal = MAX_int32;
	const TArray<uint8> BeforeExhaustedOrdinal = SerializeRuntimeState(ExhaustedOrdinal);
	TestFalse(TEXT("reforge rejects an ordinal that cannot advance safely"), FGameXXKEquipmentEconomyRules::BeginReforge(
		ExhaustedOrdinal, EpicItem, 0, ResultA));
	TestEqual(TEXT("exhausted reforge ordinal returns invalid request"), ResultA.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(*this, TEXT("exhausted reforge ordinal preserves the complete runtime"), BeforeExhaustedOrdinal, ExhaustedOrdinal);

	FGameXXKRuntimeState Stale = SeedState;
	TestTrue(TEXT("stale fixture starts with a paid preview"), FGameXXKEquipmentEconomyRules::BeginReforge(Stale, EpicItem, 0, ResultA));
	FGameXXKEquipmentInstance* StaleInstance = FindMutableInstance(Stale, EpicItem);
	if (StaleInstance)
	{
		StaleInstance->RolledAffixes[0].Magnitude = Stale.EquipmentCollection.PendingReforge.OriginalAffix.Magnitude == 300 ? 301 : 300;
	}
	const TArray<uint8> BeforeStaleAccept = SerializeRuntimeState(Stale);
	TestFalse(TEXT("accept rejects a stale original affix"), FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Stale, true, ResultA));
	TestEqual(TEXT("stale accept returns the typed error"), ResultA.Error, EGameXXKEquipmentTransactionError::PendingReforgeStale);
	TestRuntimeUnchanged(*this, TEXT("stale accept preserves the complete runtime"), BeforeStaleAccept, Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyDismantleTest,
	"GameXXK.Equipment.Economy.ProtectedBatchDismantle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyDismantleTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("common dismantle yields one sand"), FGameXXKEquipmentCatalog::GetDismantleSandYield(EGameXXKEquipmentQuality::Common), 1);
	TestEqual(TEXT("rare dismantle yields one sand"), FGameXXKEquipmentCatalog::GetDismantleSandYield(EGameXXKEquipmentQuality::Rare), 1);
	TestEqual(TEXT("epic dismantle yields one sand"), FGameXXKEquipmentCatalog::GetDismantleSandYield(EGameXXKEquipmentQuality::Epic), 1);

	FGameXXKRuntimeState State = MakeState(0, 1);
	State.PlayerGold = 777;
	const FName Common = CreateModern(*this, State, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Weapon);
	const FName Rare = CreateModern(*this, State, EGameXXKEquipmentQuality::Rare, EGameXXKEquipmentSlot::Armor);
	const FName Epic = CreateModern(*this, State, EGameXXKEquipmentQuality::Epic, EGameXXKEquipmentSlot::Accessory);
	FindMutableInstance(State, Common)->EnhancementLevel = 1;
	FindMutableInstance(State, Rare)->EnhancementLevel = 2;
	FindMutableInstance(State, Epic)->EnhancementLevel = 3;
	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("rare fixture equips before protected dismantle"), FGameXXKEquipmentEconomyRules::Equip(
		State, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Armor, Rare, Result));
	const TArray<FName> Selected = {Common, Rare, Epic};
	const TArray<uint8> BeforeConfirmation = SerializeRuntimeState(State);
	TestFalse(TEXT("protected batch requires confirmation"), FGameXXKEquipmentEconomyRules::DismantleBatch(State, Selected, false, Result));
	TestEqual(TEXT("protected batch returns confirmation error"), Result.Error, EGameXXKEquipmentTransactionError::ConfirmationRequired);
	TestTrue(TEXT("protected batch exposes confirmation flag"), Result.bConfirmationRequired);
	TestEqual(TEXT("confirmation preview reports one sand per item"), Result.RefinementSandDelta, 3);
	TestEqual(TEXT("confirmation preview reports one stone per item"), Result.EnhancementStoneDelta, 3);
	TestRuntimeUnchanged(*this, TEXT("confirmation preview mutates nothing"), BeforeConfirmation, State);

	TestTrue(TEXT("confirmed protected batch dismantles"), FGameXXKEquipmentEconomyRules::DismantleBatch(State, Selected, true, Result));
	TestTrue(TEXT("confirmed dismantle result succeeds"), Result.bSucceeded);
	TestEqual(TEXT("confirmed dismantle reports all instances"), Result.AffectedInstanceIds.Num(), 3);
	TestEqual(TEXT("confirmed dismantle adds sand"), State.EquipmentCollection.RefinementSand, 4);
	TestEqual(TEXT("confirmed dismantle adds backpack sand"), State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()), 4);
	TestEqual(TEXT("confirmed dismantle adds one stone per item"), State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()), 3);
	TestEqual(TEXT("confirmed dismantle synchronizes the stone mirror"), State.EnhancementMaterial, 3);
	TestEqual(TEXT("dismantle awards ten gold per item"), State.PlayerGold, 807);
	TestNull(TEXT("common instance is deleted"), FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Common));
	TestNull(TEXT("rare instance is deleted"), FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Rare));
	TestNull(TEXT("epic instance is deleted"), FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Epic));
	const FGameXXKEquipmentLoadout* HeroLoadout = State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	TestTrue(TEXT("equipped dismantle clears its slot directly"), !HeroLoadout || HeroLoadout->ArmorInstanceId.IsNone());
	TestHeroMatchesProjection(*this, TEXT("hero after equipped dismantle"), State);

	FGameXXKRuntimeState DuplicateState = MakeState();
	const FName DuplicateItem = CreateModern(*this, DuplicateState, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Head);
	const TArray<uint8> BeforeDuplicate = SerializeRuntimeState(DuplicateState);
	TestFalse(TEXT("duplicate IDs are rejected as one atomic batch"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		DuplicateState, {DuplicateItem, DuplicateItem}, true, Result));
	TestEqual(TEXT("duplicate batch returns invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(*this, TEXT("duplicate batch preserves complete state"), BeforeDuplicate, DuplicateState);

	FGameXXKRuntimeState PerItemFloor = MakeState();
	const FName FloorA = CreateModern(*this, PerItemFloor, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Head);
	const FName FloorB = CreateModern(*this, PerItemFloor, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Shoes);
	FindMutableInstance(PerItemFloor, FloorA)->EnhancementLevel = 1;
	FindMutableInstance(PerItemFloor, FloorB)->EnhancementLevel = 1;
	TestFalse(TEXT("two enhanced common items still require confirmation"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		PerItemFloor, {FloorA, FloorB}, false, Result));
	TestEqual(TEXT("two items always preview two fixed stone rewards"), Result.EnhancementStoneDelta, 2);

	FGameXXKRuntimeState SandOverflow = MakeState(0, MAX_int32);
	const FName SandOverflowItem = CreateModern(*this, SandOverflow, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Weapon);
	FindMutableInstance(SandOverflow, SandOverflowItem)->EnhancementLevel = 1;
	const TArray<uint8> BeforeSandOverflow = SerializeRuntimeState(SandOverflow);
	TestFalse(TEXT("dismantle preview rejects a sand reward that cannot be represented"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		SandOverflow, {SandOverflowItem}, false, Result));
	TestEqual(TEXT("sand reward overflow returns invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestFalse(TEXT("sand reward overflow is not presented as a confirmation"), Result.bConfirmationRequired);
	TestEqual(TEXT("sand reward overflow reports no unapplied sand delta"), Result.RefinementSandDelta, 0);
	TestRuntimeUnchanged(*this, TEXT("sand reward overflow preserves the complete runtime"), BeforeSandOverflow, SandOverflow);
	TestFalse(TEXT("confirmed dismantle rejects the same overflowing sand reward"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		SandOverflow, {SandOverflowItem}, true, Result));
	TestEqual(TEXT("confirmed sand overflow returns the same invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestEqual(TEXT("confirmed sand overflow reports no unapplied sand delta"), Result.RefinementSandDelta, 0);
	TestRuntimeUnchanged(*this, TEXT("confirmed sand overflow preserves the complete runtime"), BeforeSandOverflow, SandOverflow);

	FGameXXKRuntimeState BackpackSandOverflow = MakeState(0, 0);
	BackpackSandOverflow.Inventory.FindOrAdd(UGameXXKMVPRules::ItemRefinementSand()) = MAX_int32;
	const FName BackpackSandOverflowItem = CreateModern(
		*this,
		BackpackSandOverflow,
		EGameXXKEquipmentQuality::Common,
		EGameXXKEquipmentSlot::Weapon);
	const TArray<uint8> BeforeBackpackSandOverflow = SerializeRuntimeState(BackpackSandOverflow);
	TestFalse(TEXT("dismantle rejects overflow of the authoritative backpack sand even when the legacy mirror is stale"),
		FGameXXKEquipmentEconomyRules::DismantleBatch(
			BackpackSandOverflow,
			{BackpackSandOverflowItem},
			true,
			Result));
	TestEqual(TEXT("authoritative backpack sand overflow returns invalid request"),
		Result.Error,
		EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(
		*this,
		TEXT("authoritative backpack sand overflow preserves the complete runtime"),
		BeforeBackpackSandOverflow,
		BackpackSandOverflow);

	FGameXXKRuntimeState StoneOverflow = MakeState(MAX_int32, 0);
	const FName StoneOverflowItem = CreateModern(*this, StoneOverflow, EGameXXKEquipmentQuality::Common, EGameXXKEquipmentSlot::Armor);
	FindMutableInstance(StoneOverflow, StoneOverflowItem)->EnhancementLevel = 10;
	const TArray<uint8> BeforeStoneOverflow = SerializeRuntimeState(StoneOverflow);
	TestFalse(TEXT("dismantle preview rejects a stone reward that cannot be represented"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		StoneOverflow, {StoneOverflowItem}, false, Result));
	TestEqual(TEXT("stone reward overflow preview returns invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestFalse(TEXT("stone reward overflow is not presented as a confirmation"), Result.bConfirmationRequired);
	TestEqual(TEXT("stone reward overflow preview reports no unapplied stone delta"), Result.EnhancementStoneDelta, 0);
	TestRuntimeUnchanged(*this, TEXT("stone reward overflow preview preserves the complete runtime"), BeforeStoneOverflow, StoneOverflow);
	TestFalse(TEXT("confirmed dismantle rejects a stone reward that cannot be represented"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		StoneOverflow, {StoneOverflowItem}, true, Result));
	TestEqual(TEXT("stone reward overflow returns invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestEqual(TEXT("stone reward overflow reports no unapplied stone delta"), Result.EnhancementStoneDelta, 0);
	TestRuntimeUnchanged(*this, TEXT("stone reward overflow preserves the complete runtime"), BeforeStoneOverflow, StoneOverflow);

	FGameXXKRuntimeState PendingReference = MakeState(0, 30);
	const FName PendingItem = CreateModern(*this, PendingReference, EGameXXKEquipmentQuality::Rare, EGameXXKEquipmentSlot::Belt);
	TestTrue(TEXT("pending-reference fixture begins reforge"), FGameXXKEquipmentEconomyRules::BeginReforge(PendingReference, PendingItem, 0, Result));
	const TArray<uint8> BeforePendingDismantle = SerializeRuntimeState(PendingReference);
	TestFalse(TEXT("pending reforge target cannot be dismantled"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		PendingReference, {PendingItem}, true, Result));
	TestEqual(TEXT("pending reference returns the pending error"), Result.Error, EGameXXKEquipmentTransactionError::PendingReforgeExists);
	TestRuntimeUnchanged(*this, TEXT("pending-reference rejection preserves paid preview"), BeforePendingDismantle, PendingReference);

	FGameXXKRuntimeState Overflow = MakeState();
	Overflow.EquipmentCollection.CollectionSeed = 0x4119;
	Overflow.EquipmentCollection.bLegacyWarehouseOverflow = true;
	for (int32 Index = 0; Index < 201; ++Index)
	{
		AddLegacyWarehouse(Overflow, TEXT("Item.WoodenSword"), 0, FString::Printf(TEXT("Overflow.%03d"), Index));
	}
	TestTrue(TEXT("legacy overflow fixture is valid"), FGameXXKEquipmentRules::ValidateCollectionState(Overflow.EquipmentCollection));
	const FName OverflowRemoval = Overflow.EquipmentCollection.WarehouseInstanceIds[0];
	TestTrue(TEXT("dismantling can reduce a migrated legacy overflow"), FGameXXKEquipmentEconomyRules::DismantleBatch(
		Overflow, {OverflowRemoval}, false, Result));
	TestEqual(TEXT("overflow reduces to normal capacity"), Overflow.EquipmentCollection.WarehouseInstanceIds.Num(), 200);
	TestFalse(TEXT("overflow flag clears at normal capacity"), Overflow.EquipmentCollection.bLegacyWarehouseOverflow);
	TestEqual(TEXT("legacy compatibility inventory rebuilds remaining copies"), Overflow.Inventory.FindRef(TEXT("Item.WoodenSword")), 200);
	TestEqual(TEXT("overflow dismantle yields one fixed sand"), Overflow.EquipmentCollection.RefinementSand, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyLegacyPurchaseTest,
	"GameXXK.Equipment.Economy.LegacyCompatibilityPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyLegacyPurchaseTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeState();
	State.PlayerGold = 50;
	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("legacy compatibility purchase creates an instance"),
		FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(State, TEXT("Item.WoodenSword"), Result));
	TestTrue(TEXT("legacy purchase result succeeds"), Result.bSucceeded);
	TestEqual(TEXT("legacy purchase spends the existing wooden-sword price"), State.PlayerGold, 32);
	TestEqual(TEXT("legacy purchase adds one warehouse instance"), State.EquipmentCollection.WarehouseInstanceIds.Num(), 1);
	TestEqual(TEXT("legacy purchase synchronizes inventory count"), State.Inventory.FindRef(TEXT("Item.WoodenSword")), 1);
	const FGameXXKEquipmentInstance* Purchased = Result.AffectedInstanceIds.Num() == 1
		? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Result.AffectedInstanceIds[0])
		: nullptr;
	TestNotNull(TEXT("legacy purchase returns the created instance ID"), Purchased);
	if (Purchased)
	{
		TestEqual(TEXT("legacy purchase stores common quality"), Purchased->Quality, EGameXXKEquipmentQuality::Common);
		TestEqual(TEXT("legacy purchase stores legacy scaling"), Purchased->ScalingRule, EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement);
		TestEqual(TEXT("legacy purchase stores the exact base snapshot"), Purchased->LegacyBaseStatSnapshot.Attack, 3);
	}
	State.PlayerGold = 17;
	const TArray<uint8> InsufficientSnapshot = SerializeRuntimeState(State);
	TestFalse(TEXT("legacy compatibility purchase rejects insufficient gold"),
		FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(State, TEXT("Item.WoodenSword"), Result));
	TestRuntimeUnchanged(*this, TEXT("failed legacy purchase preserves complete runtime"), InsufficientSnapshot, State);
	State.PlayerGold = 32;
	TestFalse(TEXT("modern package definitions cannot use the legacy compatibility purchase"),
		FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(State, TEXT("Equipment.PoJun.Weapon"), Result));
	TestEqual(TEXT("modern compatibility purchase is an invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);

	FGameXXKRuntimeState ExhaustedOrdinal = MakeState();
	ExhaustedOrdinal.PlayerGold = 50;
	ExhaustedOrdinal.EquipmentCollection.NextInstanceOrdinal = MAX_int32;
	const TArray<uint8> BeforeExhaustedOrdinal = SerializeRuntimeState(ExhaustedOrdinal);
	TestFalse(TEXT("legacy compatibility purchase rejects an exhausted instance ordinal"),
		FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(ExhaustedOrdinal, TEXT("Item.WoodenSword"), Result));
	TestEqual(TEXT("exhausted legacy instance ordinal returns invalid request"), Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(*this, TEXT("exhausted legacy instance ordinal preserves the complete runtime"), BeforeExhaustedOrdinal, ExhaustedOrdinal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentEconomyLegacyEquipIdempotenceTest,
	"GameXXK.Equipment.Economy.LegacyFacadeEquipIdempotence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentEconomyLegacyEquipIdempotenceTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeState();
	State.PlayerGold = 500;
	const FName IronSword = UGameXXKMVPRules::ItemIronSword();
	TestTrue(TEXT("legacy facade purchase creates the iron-sword warehouse instance"),
		UGameXXKMVPRules::BuyItem(State, IronSword, 1));
	TestTrue(TEXT("legacy facade equips the iron sword once"), UGameXXKMVPRules::EquipItem(State, IronSword));

	const FGameXXKEquipmentLoadout* HeroLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	const FName FirstEquippedInstanceId = HeroLoadout ? HeroLoadout->WeaponInstanceId : NAME_None;
	TestFalse(TEXT("first legacy facade equip stores a hero weapon instance"), FirstEquippedInstanceId.IsNone());
	const int32 AttackAfterFirstEquip = State.PlayerAttack;
	const FName EquippedWeaponMirrorAfterFirstEquip = State.EquippedWeapon;
	const TArray<uint8> StateAfterFirstEquip = SerializeRuntimeState(State);
	FGameXXKRuntimeState CorruptMatchingEquip = State;
	CorruptMatchingEquip.EquipmentCollection.WarehouseInstanceIds.Add(FirstEquippedInstanceId);
	const TArray<uint8> CorruptMatchingEquipSnapshot = SerializeRuntimeState(CorruptMatchingEquip);
	TestFalse(TEXT("legacy facade does not accept a corrupt matching equipped instance"),
		UGameXXKMVPRules::EquipItem(CorruptMatchingEquip, IronSword));
	TestRuntimeUnchanged(
		*this,
		TEXT("rejected corrupt matching equip preserves the complete runtime"),
		CorruptMatchingEquipSnapshot,
		CorruptMatchingEquip);

	TestTrue(TEXT("legacy facade treats the same base weapon as an idempotent equip"),
		UGameXXKMVPRules::EquipItem(State, IronSword));
	HeroLoadout = State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("idempotent legacy facade equip preserves the equipped instance"),
		HeroLoadout ? HeroLoadout->WeaponInstanceId : NAME_None,
		FirstEquippedInstanceId);
	TestEqual(TEXT("idempotent legacy facade equip does not stack attack"), State.PlayerAttack, AttackAfterFirstEquip);
	TestEqual(TEXT("idempotent legacy facade equip preserves the compatibility mirror"),
		State.EquippedWeapon,
		EquippedWeaponMirrorAfterFirstEquip);
	TestRuntimeUnchanged(*this, TEXT("idempotent legacy facade equip preserves the complete runtime"), StateAfterFirstEquip, State);

	const FName WoodenSword = UGameXXKMVPRules::ItemWoodenSword();
	FGameXXKEquipmentTransactionResult WoodenSwordPurchaseResult;
	TestTrue(TEXT("legacy facade purchase creates a different warehouse weapon"),
		FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(
			State,
			WoodenSword,
			WoodenSwordPurchaseResult));
	const FName WoodenSwordInstanceId =
		FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, WoodenSword, false);
	TestFalse(TEXT("different warehouse weapon has a stable instance ID"), WoodenSwordInstanceId.IsNone());
	TestTrue(TEXT("legacy facade switches the occupied slot to a different warehouse base"),
		UGameXXKMVPRules::EquipItem(State, WoodenSword));
	HeroLoadout = State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("different-base legacy equip installs the deterministic warehouse instance"),
		HeroLoadout ? HeroLoadout->WeaponInstanceId : NAME_None,
		WoodenSwordInstanceId);
	TestEqual(TEXT("different-base legacy equip updates the compatibility mirror"), State.EquippedWeapon, WoodenSword);
	return true;
}

#endif
