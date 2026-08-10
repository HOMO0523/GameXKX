#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRunDeckRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#include <type_traits>

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKRouteCardEntriesSaveMigrationTest
{
	template <typename T, typename = void>
	struct TRouteCardEntriesVersionValue
	{
		static constexpr int32 Value = INDEX_NONE;
	};

	template <typename T>
	struct TRouteCardEntriesVersionValue<T, std::void_t<decltype(T::RouteCardEntriesIntroducedSaveVersion)>>
	{
		static constexpr int32 Value = T::RouteCardEntriesIntroducedSaveVersion;
	};

	const FName HeroUnitId(TEXT("Player"));
	const FName CommonMergeCardId(TEXT("Route.General.PoJiaTuCi"));
	const FName RareCardId(TEXT("Route.Rare.GuJuanCanZhang"));

	bool StartAcceptedRoute(FGameXXKRuntimeState& OutState, const int32 RootSeed = 0x24681357)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState))
		{
			return false;
		}
		OutState.RouteSeed = RootSeed;
		return UGameXXKMVPRules::EnterDungeon(OutState);
	}

	FGameXXKSaveState MakeVersionedSave(const FGameXXKRuntimeState& State, const int32 Version)
	{
		FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		Save.SaveVersion = Version;
		return Save;
	}

	FGameXXKRouteCardEntry MakePrereleaseEntry()
	{
		FGameXXKRouteCardEntry Entry;
		Entry.EntryId = TEXT("RouteEntry.Prerelease.MustDiscard");
		Entry.CardId = RareCardId;
		Entry.CurrentQuality = EGameXXKCardQuality::Rare;
		Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
		Entry.OwnerUnitId = HeroUnitId;
		Entry.bTemporaryRouteCard = true;
		Entry.bConsumesRouteCapacity = true;
		Entry.AcquisitionOrdinal = 777;
		return Entry;
	}

	void SetLegacyRouteCards(
		FGameXXKRuntimeState& InOutState,
		const TArray<FName>& LegacyIds,
		const bool bAddPrereleaseEntry = true)
	{
		InOutState.CardRun.RouteCardIds = LegacyIds;
		InOutState.CardRun.RouteCardEntries.Reset();
		if (bAddPrereleaseEntry)
		{
			InOutState.CardRun.RouteCardEntries.Add(MakePrereleaseEntry());
		}
		InOutState.CardRun.NextRouteCardEntryOrdinal = 778;
		InOutState.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	}

	bool EntriesMatch(
		const TArray<FGameXXKRouteCardEntry>& Left,
		const TArray<FGameXXKRouteCardEntry>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(
				&Left[Index],
				&Right[Index],
				PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	const FGameXXKRouteCardEntry* FindByOrdinal(
		const TArray<FGameXXKRouteCardEntry>& Entries,
		const int32 Ordinal)
	{
		return Entries.FindByPredicate([Ordinal](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.AcquisitionOrdinal == Ordinal;
		});
	}

	bool ContainsWarning(const FGameXXKSaveMigrationReport& Report, const FString& Fragment)
	{
		return Report.Warnings.ContainsByPredicate([&Fragment](const FString& Warning)
		{
			return Warning.Contains(Fragment);
		});
	}

	FGameXXKBattleRuntimeUnit MakeLegacyBattleUnit(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.MP = Mana;
		Unit.MaxMP = Mana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardEntriesV9VersionContractTest,
	"GameXXK.MVP.SaveGame.RouteCardEntriesV9.VersionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardEntriesV9VersionContractTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardEntriesSaveMigrationTest;
	TestEqual(
		TEXT("stable route-card entries are introduced by save version nine"),
		TRouteCardEntriesVersionValue<FGameXXKSaveMigration>::Value,
		9);
	TestEqual(TEXT("the protagonist card pool advances the current save version to twelve"), FGameXXKSaveMigration::CurrentSaveVersion, 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardEntriesV9MigrationTest,
	"GameXXK.MVP.SaveGame.RouteCardEntriesV9.Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardEntriesV9MigrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardEntriesSaveMigrationTest;
	const int32 LegacyVersion = FGameXXKSaveMigration::RouteCardEntriesIntroducedSaveVersion - 1;

	// Zero legacy cards still creates only the canonical eighteen, and a prerelease entries array is discarded.
	FGameXXKRuntimeState ZeroState;
	if (!TestTrue(TEXT("zero-card fixture enters a route"), StartAcceptedRoute(ZeroState)))
	{
		return false;
	}
	SetLegacyRouteCards(ZeroState, {});
	const FGameXXKSaveState ZeroSource = MakeVersionedSave(ZeroState, LegacyVersion);
	const FGameXXKSaveState ZeroSourceBefore = ZeroSource;
	FGameXXKSaveState ZeroMigrated;
	FGameXXKSaveMigrationReport ZeroReport;
	TestTrue(TEXT("a v8 route with zero legacy rewards migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(ZeroSource, ZeroMigrated, ZeroReport));
	TestEqual(TEXT("zero legacy cards leave the canonical eighteen"),
		ZeroMigrated.RuntimeState.CardRun.RouteCardEntries.Num(), FGameXXKRouteCardRecipe::BaseEntryCount);
	TestEqual(TEXT("zero legacy cards start the next ordinal at eighteen"),
		ZeroMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, FGameXXKRouteCardRecipe::BaseEntryCount);
	TestTrue(TEXT("legacy IDs are cleared"), ZeroMigrated.RuntimeState.CardRun.RouteCardIds.IsEmpty());
	TestFalse(TEXT("legacy replacement metadata is cleared"),
		ZeroMigrated.RuntimeState.CardRun.PendingReward.bRequiresRouteCardReplacement);
	TestNull(TEXT("the prerelease entry is never concatenated"),
		ZeroMigrated.RuntimeState.CardRun.RouteCardEntries.FindByPredicate([](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.EntryId == TEXT("RouteEntry.Prerelease.MustDiscard");
		}));
	TestTrue(TEXT("discarding a nonempty prerelease array is reported"), ContainsWarning(ZeroReport, TEXT("prerelease")));
	TestTrue(TEXT("successful migration leaves the source bit-for-bit unchanged"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&ZeroSource, &ZeroSourceBefore, PPF_None));
	FGameXXKSaveState ZeroLegacyFalseSource = ZeroSource;
	ZeroLegacyFalseSource.RuntimeState.CardRun.PendingReward.bRequiresRouteCardReplacement = false;
	FGameXXKSaveState ZeroLegacyFalseMigrated;
	FGameXXKSaveMigrationReport ZeroLegacyFalseReport;
	TestTrue(TEXT("the same v8 empty reward migrates when the stale legacy bool is false"),
		FGameXXKSaveMigration::MigrateToCurrent(
			ZeroLegacyFalseSource,
			ZeroLegacyFalseMigrated,
			ZeroLegacyFalseReport));
	TestTrue(TEXT("stale legacy bool true/false produce the same canonical migration output"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(
			&ZeroMigrated,
			&ZeroLegacyFalseMigrated,
			PPF_None));

	// One valid card receives stable ordinal 18 and its catalog quality/provenance.
	FGameXXKRuntimeState OneState;
	if (!TestTrue(TEXT("one-card fixture enters a route"), StartAcceptedRoute(OneState)))
	{
		return false;
	}
	// RootSeed is authoritative even when the route-map and adapter seeds disagree.
	OneState.RouteSeed = 0x11112222;
	OneState.CardRun.RouteRandomSeed = 0x33334444;
	SetLegacyRouteCards(OneState, {RareCardId});
	FGameXXKSaveState OneMigrated;
	FGameXXKSaveMigrationReport OneReport;
	TestTrue(TEXT("one valid legacy route card migrates"), FGameXXKSaveMigration::MigrateToCurrent(
		MakeVersionedSave(OneState, LegacyVersion), OneMigrated, OneReport));
	const FGameXXKRouteCardEntry* OneEntry = FindByOrdinal(OneMigrated.RuntimeState.CardRun.RouteCardEntries, 18);
	TestNotNull(TEXT("the first valid legacy card uses ordinal eighteen"), OneEntry);
	if (OneEntry)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RareCardId);
		TestNotNull(TEXT("the migrated route card exists in the catalog"), Definition);
		if (Definition)
		{
			TestEqual(TEXT("migration uses catalog base quality"), OneEntry->CurrentQuality, Definition->BaseQuality);
		}
		TestEqual(TEXT("migration uses route-reward provenance"), OneEntry->SourceKind, EGameXXKRouteCardSourceKind::RouteReward);
		TestEqual(TEXT("migration assigns the fixed player owner"), OneEntry->OwnerUnitId, HeroUnitId);
		TestTrue(TEXT("the migrated entry is temporary"), OneEntry->bTemporaryRouteCard);
		TestTrue(TEXT("the migrated entry consumes route capacity"), OneEntry->bConsumesRouteCapacity);
		FName ExpectedId;
		TestTrue(TEXT("expected stable id can be generated"), FGameXXKRouteCardRecipe::MakeStableEntryId(
			OneState.CardRun.RouteProgress.RootSeed, 18, ExpectedId));
		TestEqual(TEXT("the migrated entry receives the deterministic stable id"), OneEntry->EntryId, ExpectedId);
	}
	TestEqual(TEXT("one legacy slot advances next to nineteen"),
		OneMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 19);
	TestEqual(TEXT("migration does not increment actual acquisitions"),
		OneMigrated.RuntimeState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		OneState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount);

	const TArray<FName> TwelveDistinctCards = {
		TEXT("Route.General.HeJiLing"),
		TEXT("Route.Terrain.DuanYaLuoShi"),
		TEXT("Route.Terrain.LinYingFuXi"),
		TEXT("Route.Terrain.DuKouHuiLiu"),
		TEXT("Route.Terrain.ZhaiHuoYuanShou"),
		TEXT("Route.Terrain.DongHuoZhaoMing"),
		TEXT("Route.Terrain.JieShiTuXi"),
		TEXT("Route.Terrain.DiMaiHuiXiang"),
		TEXT("Route.Terrain.LinShiZhaYing"),
		TEXT("Route.Terrain.XianLuTuWei"),
		TEXT("Route.Rare.GuJuanCanZhang"),
		TEXT("Route.Rare.TieYiYiJue")};

	FGameXXKRuntimeState TwelveState;
	if (!TestTrue(TEXT("twelve-card fixture enters a route"), StartAcceptedRoute(TwelveState)))
	{
		return false;
	}
	SetLegacyRouteCards(TwelveState, TwelveDistinctCards, false);
	FGameXXKSaveState TwelveMigrated;
	FGameXXKSaveMigrationReport TwelveReport;
	TestTrue(TEXT("exactly twelve legacy cards migrate"), FGameXXKSaveMigration::MigrateToCurrent(
		MakeVersionedSave(TwelveState, LegacyVersion), TwelveMigrated, TwelveReport));
	int32 CapacityUsed = INDEX_NONE;
	FString CapacityError;
	TestTrue(TEXT("the twelve-card result is structurally valid"), FGameXXKRunDeckRules::GetCapacityUsed(
		TwelveMigrated.RuntimeState.CardRun.RouteCardEntries, CapacityUsed, &CapacityError));
	TestEqual(TEXT("all twelve distinct legacy cards consume capacity"), CapacityUsed, 12);
	TestEqual(TEXT("twelve source slots advance next to thirty"),
		TwelveMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 30);

	FGameXXKRuntimeState OverTwelveState;
	if (!TestTrue(TEXT("over-twelve fixture enters a route"), StartAcceptedRoute(OverTwelveState)))
	{
		return false;
	}
	TArray<FName> OverTwelveCards = TwelveDistinctCards;
	OverTwelveCards.Add(TEXT("Route.Rare.LingQuanYiYin"));
	SetLegacyRouteCards(OverTwelveState, OverTwelveCards, false);
	FGameXXKSaveState OverTwelveMigrated;
	FGameXXKSaveMigrationReport OverTwelveReport;
	TestTrue(TEXT("more than twelve legacy cards migrate with a deterministic cutoff"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(OverTwelveState, LegacyVersion), OverTwelveMigrated, OverTwelveReport));
	CapacityUsed = INDEX_NONE;
	TestTrue(TEXT("the truncated result is structurally valid"), FGameXXKRunDeckRules::GetCapacityUsed(
		OverTwelveMigrated.RuntimeState.CardRun.RouteCardEntries, CapacityUsed, &CapacityError));
	TestEqual(TEXT("only the first twelve source slots consume capacity"), CapacityUsed, 12);
	TestEqual(TEXT("ignored tail cards do not advance beyond ordinal thirty"),
		OverTwelveMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 30);
	TestFalse(TEXT("the thirteenth legacy card is ignored"),
		OverTwelveMigrated.RuntimeState.CardRun.RouteCardEntries.ContainsByPredicate([](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.CardId == TEXT("Route.Rare.LingQuanYiYin");
		}));
	TestTrue(TEXT("the ignored tail is reported"), ContainsWarning(OverTwelveReport, TEXT("12")));

	// Empty, unknown, and non-route cards leave ordinal holes; a later valid card retains its original index.
	FGameXXKRuntimeState HoleState;
	if (!TestTrue(TEXT("ordinal-hole fixture enters a route"), StartAcceptedRoute(HoleState)))
	{
		return false;
	}
	HoleState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 9;
	SetLegacyRouteCards(HoleState, {NAME_None, TEXT("Route.Unknown.Missing"), TEXT("Hero.Generic.QingFengYiShi"), RareCardId});
	const FGameXXKSaveState HoleSource = MakeVersionedSave(HoleState, LegacyVersion);
	FGameXXKSaveState HoleMigratedA;
	FGameXXKSaveState HoleMigratedB;
	FGameXXKSaveMigrationReport HoleReportA;
	FGameXXKSaveMigrationReport HoleReportB;
	TestTrue(TEXT("mixed invalid legacy cards migrate by warning and skip"),
		FGameXXKSaveMigration::MigrateToCurrent(HoleSource, HoleMigratedA, HoleReportA));
	TestTrue(TEXT("the same mixed source migrates a second time"),
		FGameXXKSaveMigration::MigrateToCurrent(HoleSource, HoleMigratedB, HoleReportB));
	TestNull(TEXT("empty source index leaves ordinal eighteen unused"), FindByOrdinal(HoleMigratedA.RuntimeState.CardRun.RouteCardEntries, 18));
	TestNull(TEXT("unknown source index leaves ordinal nineteen unused"), FindByOrdinal(HoleMigratedA.RuntimeState.CardRun.RouteCardEntries, 19));
	TestNull(TEXT("non-route source index leaves ordinal twenty unused"), FindByOrdinal(HoleMigratedA.RuntimeState.CardRun.RouteCardEntries, 20));
	TestNotNull(TEXT("the valid fourth source index retains ordinal twenty-one"), FindByOrdinal(HoleMigratedA.RuntimeState.CardRun.RouteCardEntries, 21));
	TestEqual(TEXT("actual acquisition history can advance next beyond source slots"),
		HoleMigratedA.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 27);
	TestTrue(TEXT("empty legacy IDs are warned"), ContainsWarning(HoleReportA, TEXT("empty")));
	TestTrue(TEXT("unknown legacy IDs are warned"), ContainsWarning(HoleReportA, TEXT("unknown")));
	TestTrue(TEXT("non-route legacy IDs are warned"), ContainsWarning(HoleReportA, TEXT("non-route")));
	TestTrue(TEXT("same input creates byte-identical stable entries"), EntriesMatch(
		HoleMigratedA.RuntimeState.CardRun.RouteCardEntries,
		HoleMigratedB.RuntimeState.CardRun.RouteCardEntries));
	TestEqual(TEXT("same input creates the same next ordinal"),
		HoleMigratedA.RuntimeState.CardRun.NextRouteCardEntryOrdinal,
		HoleMigratedB.RuntimeState.CardRun.NextRouteCardEntryOrdinal);

	// Three Common legacy duplicates merge through Rare to Epic into the stable base survivor; Epic no longer merges.
	FGameXXKRuntimeState MergeState;
	if (!TestTrue(TEXT("merge-chain fixture enters a route"), StartAcceptedRoute(MergeState)))
	{
		return false;
	}
	SetLegacyRouteCards(MergeState, {CommonMergeCardId, CommonMergeCardId, CommonMergeCardId, CommonMergeCardId}, false);
	FGameXXKSaveState MergeMigrated;
	FGameXXKSaveMigrationReport MergeReport;
	TestTrue(TEXT("legacy duplicates migrate through deterministic entry merging"), FGameXXKSaveMigration::MigrateToCurrent(
		MakeVersionedSave(MergeState, LegacyVersion), MergeMigrated, MergeReport));
	const FGameXXKRouteCardEntry* BaseSurvivor = FindByOrdinal(MergeMigrated.RuntimeState.CardRun.RouteCardEntries, 16);
	TestNotNull(TEXT("the earlier base recipe entry survives the merge chain"), BaseSurvivor);
	if (BaseSurvivor)
	{
		TestEqual(TEXT("Common to Rare to Epic chain upgrades the base survivor"),
			BaseSurvivor->CurrentQuality, EGameXXKCardQuality::Epic);
		TestFalse(TEXT("the base survivor remains non-capacity"), BaseSurvivor->bConsumesRouteCapacity);
	}
	const FGameXXKRouteCardEntry* PostEpicCommon = FindByOrdinal(MergeMigrated.RuntimeState.CardRun.RouteCardEntries, 21);
	TestNotNull(TEXT("a fourth Common remains after the Epic survivor stops merging"), PostEpicCommon);
	if (PostEpicCommon)
	{
		TestEqual(TEXT("Epic no longer consumes a later Common"), PostEpicCommon->CurrentQuality, EGameXXKCardQuality::Common);
	}
	TestEqual(TEXT("all four original source indexes advance the next ordinal"),
		MergeMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 22);

	// Inactive routes clear both representations and the dedicated sequence without disturbing unrelated save state.
	FGameXXKRuntimeState InactiveState = UGameXXKMVPRules::CreateNewGame();
	InactiveState.PlayerGold = 4321;
	InactiveState.CardRun.LastAppliedRouteSettlementId = FGuid(0x73010001, 0x73010002, 0x73010003, 0x73010004);
	SetLegacyRouteCards(InactiveState, {RareCardId});
	FGameXXKSaveState InactiveMigrated;
	FGameXXKSaveMigrationReport InactiveReport;
	TestTrue(TEXT("inactive v8 route-local cards are cleared"), FGameXXKSaveMigration::MigrateToCurrent(
		MakeVersionedSave(InactiveState, LegacyVersion), InactiveMigrated, InactiveReport));
	TestTrue(TEXT("inactive route entries are empty"), InactiveMigrated.RuntimeState.CardRun.RouteCardEntries.IsEmpty());
	TestTrue(TEXT("inactive legacy IDs are empty"), InactiveMigrated.RuntimeState.CardRun.RouteCardIds.IsEmpty());
	TestEqual(TEXT("inactive route next ordinal resets to zero"),
		InactiveMigrated.RuntimeState.CardRun.NextRouteCardEntryOrdinal, 0);
	TestFalse(TEXT("inactive replacement metadata resets"),
		InactiveMigrated.RuntimeState.CardRun.PendingReward.bRequiresRouteCardReplacement);
	TestEqual(TEXT("inactive migration preserves unrelated player gold"), InactiveMigrated.RuntimeState.PlayerGold, 4321);
	TestEqual(TEXT("inactive migration preserves settlement idempotency"),
		InactiveMigrated.RuntimeState.CardRun.LastAppliedRouteSettlementId,
		InactiveState.CardRun.LastAppliedRouteSettlementId);

	// The int64 next-ordinal calculation rejects values that cannot fit in the persisted int32, atomically.
	FGameXXKRuntimeState OverflowState;
	if (!TestTrue(TEXT("overflow fixture enters a route"), StartAcceptedRoute(OverflowState)))
	{
		return false;
	}
	OverflowState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = MAX_int32;
	SetLegacyRouteCards(OverflowState, {RareCardId});
	const FGameXXKSaveState OverflowSource = MakeVersionedSave(OverflowState, LegacyVersion);
	const FGameXXKSaveState OverflowSourceBefore = OverflowSource;
	FGameXXKSaveState OverflowRejected;
	FGameXXKSaveMigrationReport OverflowReport;
	TestFalse(TEXT("overflowing next entry ordinal rejects the migration"),
		FGameXXKSaveMigration::MigrateToCurrent(OverflowSource, OverflowRejected, OverflowReport));
	TestEqual(TEXT("overflow failure exposes no partial output"), OverflowRejected.SaveVersion, 0);
	TestTrue(TEXT("overflow failure leaves the source exact"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&OverflowSource, &OverflowSourceBefore, PPF_None));

	FGameXXKRuntimeState OrderingState;
	if (!TestTrue(TEXT("migration-order fixture enters a route"), StartAcceptedRoute(OrderingState)))
	{
		return false;
	}
	OrderingState.CardRun.RouteTravelMoney = -1;
	OrderingState.CardRun.HeroSelectedCardIds[0] = OrderingState.CardRun.HeroSelectedCardIds[1];
	SetLegacyRouteCards(OrderingState, {}, false);
	FGameXXKSaveState OrderingRejected;
	FGameXXKSaveMigrationReport OrderingReport;
	TestFalse(
		TEXT("invalid card-run configuration rejects before an invalid route economy"),
		FGameXXKSaveMigration::MigrateToCurrent(
			MakeVersionedSave(OrderingState, LegacyVersion),
			OrderingRejected,
			OrderingReport));
	TestTrue(TEXT("v12 repairs the hero loadout before reporting the invalid route economy"),
		OrderingReport.Error.Contains(TEXT("negative route travel-money")));
	TestEqual(TEXT("ordered migration failure exposes no partial output"), OrderingRejected.SaveVersion, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardEntriesV9RuntimeValidationTest,
	"GameXXK.MVP.SaveGame.RouteCardEntriesV9.RuntimeValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardEntriesV9RuntimeValidationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardEntriesSaveMigrationTest;
	FGameXXKRuntimeState Valid;
	if (!TestTrue(TEXT("runtime-validation fixture enters a route"), StartAcceptedRoute(Valid)))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("the canonical active route is structurally valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(Valid, Error));
	FGameXXKRuntimeState EmptyPendingLegacyTrue = Valid;
	EmptyPendingLegacyTrue.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	TestTrue(TEXT("validator ignores a stale true legacy bool on an empty pending reward"),
		FGameXXKSaveMigration::ValidateRuntimeState(EmptyPendingLegacyTrue, Error));

	FGameXXKRuntimeState Transitional = Valid;
	Transitional.CardRun.RouteCardEntries.Reset();
	Transitional.CardRun.NextRouteCardEntryOrdinal = 0;
	Transitional.CardRun.RouteCardIds = {RareCardId};
	TestTrue(TEXT("the current runtime may temporarily retain only legacy reward IDs"),
		FGameXXKSaveMigration::ValidateRuntimeState(Transitional, Error));
	FGameXXKRuntimeState MixedAuthority = Valid;
	MixedAuthority.CardRun.RouteCardIds = {RareCardId};
	TestTrue(TEXT("the current runtime may temporarily retain valid stable entries beside legacy reward IDs"),
		FGameXXKSaveMigration::ValidateRuntimeState(MixedAuthority, Error));

	auto ExpectRejected = [this, &Valid](const TCHAR* Label, const TFunction<void(FGameXXKRuntimeState&)>& Mutate)
	{
		FGameXXKRuntimeState State = Valid;
		Mutate(State);
		FString LocalError;
		TestFalse(Label, FGameXXKSaveMigration::ValidateRuntimeState(State, LocalError));
	};

	ExpectRejected(TEXT("unknown entry catalog card is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[0].CardId = TEXT("Route.Unknown.Validation");
	});
	ExpectRejected(TEXT("empty entry owner is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[0].OwnerUnitId = NAME_None;
	});
	ExpectRejected(TEXT("duplicate entry id is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[1].EntryId = State.CardRun.RouteCardEntries[0].EntryId;
	});
	ExpectRejected(TEXT("duplicate acquisition ordinal is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[1].AcquisitionOrdinal = State.CardRun.RouteCardEntries[0].AcquisitionOrdinal;
	});
	ExpectRejected(TEXT("invalid concrete quality is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[0].CurrentQuality = EGameXXKCardQuality::Invalid;
	});
	ExpectRejected(TEXT("invalid concrete source is rejected"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[0].SourceKind = EGameXXKRouteCardSourceKind::Invalid;
	});
	ExpectRejected(TEXT("a base source cannot consume route capacity"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries[0].bConsumesRouteCapacity = true;
	});
	ExpectRejected(TEXT("next ordinal must be greater than every persisted ordinal"), [](FGameXXKRuntimeState& State)
	{
		State.CardRun.NextRouteCardEntryOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount - 1;
	});
	ExpectRejected(TEXT("more than twelve capacity entries are rejected"), [](FGameXXKRuntimeState& State)
	{
		for (int32 Index = 0; Index < 13; ++Index)
		{
			FGameXXKRouteCardEntry Entry;
			Entry.EntryId = FName(*FString::Printf(TEXT("RouteEntry.Validation.%d"), Index));
			Entry.CardId = RareCardId;
			Entry.CurrentQuality = EGameXXKCardQuality::Rare;
			Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
			Entry.OwnerUnitId = HeroUnitId;
			Entry.bTemporaryRouteCard = true;
			Entry.bConsumesRouteCapacity = true;
			Entry.AcquisitionOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount + Index;
			State.CardRun.RouteCardEntries.Add(MoveTemp(Entry));
		}
		State.CardRun.NextRouteCardEntryOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount + 13;
	});

	FGameXXKRuntimeState InactiveWithEntries = UGameXXKMVPRules::CreateNewGame();
	InactiveWithEntries.CardRun.RouteCardEntries.Add(Valid.CardRun.RouteCardEntries[0]);
	InactiveWithEntries.CardRun.NextRouteCardEntryOrdinal = 1;
	TestFalse(TEXT("inactive route cannot retain stable entries"),
		FGameXXKSaveMigration::ValidateRuntimeState(InactiveWithEntries, Error));
	FGameXXKRuntimeState InactiveWithNext = UGameXXKMVPRules::CreateNewGame();
	InactiveWithNext.CardRun.NextRouteCardEntryOrdinal = 18;
	TestFalse(TEXT("inactive route cannot retain a next stable-entry ordinal"),
		FGameXXKSaveMigration::ValidateRuntimeState(InactiveWithNext, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardEntriesV9ActiveBattlePreservationTest,
	"GameXXK.MVP.SaveGame.RouteCardEntriesV9.ActiveBattlePreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardEntriesV9ActiveBattlePreservationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteCardEntriesSaveMigrationTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("active-battle fixture enters a route"), StartAcceptedRoute(State, 0x71234567)))
	{
		return false;
	}
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 30, 15, 8, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("MoneyRat"), TEXT("Money Rat"), 60, 0, 9, 2, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = State.CurrentRouteNodeId;
	FString Error;
	if (!TestTrue(TEXT("fixture creates a saved active card battle"), FGameXXKCardBattleAdapter::BeginCardBattle(
		State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 0x7654321, &Error)))
	{
		return false;
	}
	if (!State.CardRun.EnemyIntents.IsEmpty())
	{
		State.CardRun.NextEnemyIntentIndex = 1;
	}

	SetLegacyRouteCards(State, {RareCardId});
	FGameXXKCardBattleRuntime ExpectedBattle = State.CardRun.ActiveBattle;
	constexpr uint32 CombatRandomSalt = 0xA341316CU;
	uint32 ExpectedCombatSeed = static_cast<uint32>(ExpectedBattle.Deck.CurrentRandomState) ^ CombatRandomSalt;
	if (ExpectedCombatSeed == 0)
	{
		ExpectedCombatSeed = CombatRandomSalt;
	}
	ExpectedBattle.CombatRandomState = static_cast<int32>(ExpectedCombatSeed);
	const TArray<FGameXXKCardEnemyIntent> ExpectedIntents = State.CardRun.EnemyIntents;
	const int32 ExpectedNextIntent = State.CardRun.NextEnemyIntentIndex;
	const FGameXXKSaveState Source = MakeVersionedSave(
		State,
		FGameXXKSaveMigration::RouteCardEntriesIntroducedSaveVersion - 1);
	const FGameXXKSaveState SourceBefore = Source;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("a v8 save can migrate while a battle is in progress"),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	TestTrue(TEXT("active battle runtime, zones, RNG, and card instance ids are exact"),
		FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(
			&Migrated.RuntimeState.CardRun.ActiveBattle,
			&ExpectedBattle,
			PPF_None));
	TestEqual(TEXT("enemy intent count is exact"), Migrated.RuntimeState.CardRun.EnemyIntents.Num(), ExpectedIntents.Num());
	if (Migrated.RuntimeState.CardRun.EnemyIntents.Num() == ExpectedIntents.Num())
	{
		for (int32 Index = 0; Index < ExpectedIntents.Num(); ++Index)
		{
			TestTrue(
				FString::Printf(TEXT("enemy intent %d is exact"), Index),
				FGameXXKCardEnemyIntent::StaticStruct()->CompareScriptStruct(
					&Migrated.RuntimeState.CardRun.EnemyIntents[Index],
					&ExpectedIntents[Index],
					PPF_None));
		}
	}
	TestEqual(TEXT("next enemy intent index is exact"),
		Migrated.RuntimeState.CardRun.NextEnemyIntentIndex,
		ExpectedNextIntent);
	TestTrue(TEXT("active battle migration leaves the source exact"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Source, &SourceBefore, PPF_None));
	return true;
}

#endif
