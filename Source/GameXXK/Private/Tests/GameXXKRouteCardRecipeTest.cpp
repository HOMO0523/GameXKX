#include "GameXXKRouteCardRecipe.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 RecipeRouteSeed = 0x2468ACE;
	const FName HeroUnitId(TEXT("Player"));
	const FName CompanionInstanceId(TEXT("Companion.Test.RouteRecipe"));
	const FName QuestNpcId(TEXT("Npc.TusiChief"));

	const TArray<FName> MissingPartyFillCards = {
		TEXT("Route.General.QingShenQuShi"),
		TEXT("Route.General.TuNaJue"),
		TEXT("Route.General.ZhiXueSan"),
		TEXT("Route.General.FeiZhen"),
		TEXT("Route.General.YanDun"),
		TEXT("Route.General.TieJiLi"),
		TEXT("Route.General.LinZhenMoRen"),
		TEXT("Route.Terrain.XingJunBuZhen")};

	const TArray<FName> FixedRouteCards = {
		TEXT("Route.General.PoJiaTuCi"),
		TEXT("Route.General.ShouShiHuiYuan")};

	bool AreEntriesIdentical(
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

	FGameXXKCardRunState MakeRecipeRun(const bool bWithCompanion, const bool bWithQuestNpc)
	{
		FGameXXKCardRunState Run;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				Run.HeroUnlockedCardIds.Add(Definition.Id);
				if (Run.HeroSelectedCardIds.Num() < 8)
				{
					Run.HeroSelectedCardIds.Add(Definition.Id);
				}
			}
		}

		if (bWithCompanion)
		{
			FGameXXKPermanentCompanion& Companion = Run.CompanionRoster.PermanentCompanions.AddDefaulted_GetRef();
			Companion.InstanceId = CompanionInstanceId;
			Companion.Role = EGameXXKCharacterRole::Blade;
			Companion.bIsActive = true;
			for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
			{
				if (Definition.Owner == EGameXXKCardOwner::Profession
					&& Definition.Role == Companion.Role
					&& Companion.SelectedCardIds.Num() < 5)
				{
					Companion.SelectedCardIds.Add(Definition.Id);
				}
			}
			Companion.PersonalCardIds = Companion.SelectedCardIds;
			Companion.UnlockedPersonalCardIds = Companion.SelectedCardIds;
			Run.PartySelection.ActivePermanentCompanionInstanceId = Companion.InstanceId;
		}

		if (bWithQuestNpc)
		{
			const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
			if (Definition)
			{
				Run.ActiveTemporaryQuestNpcId = QuestNpcId;
				Run.PartySelection.QuestNpc.NpcId = QuestNpcId;
				Run.PartySelection.QuestNpc.SelectedCardIds = Definition->DefaultRouteCardIds;
			}
		}

		return Run;
	}

	TArray<FName> BuildExpectedCardIds(const FGameXXKCardRunState& Run, const bool bWithCompanion, const bool bWithQuestNpc)
	{
		TArray<FName> Expected = Run.HeroSelectedCardIds;
		int32 MissingFillIndex = 0;
		if (bWithCompanion)
		{
			Expected.Append(Run.CompanionRoster.PermanentCompanions[0].SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < 5; ++Index)
			{
				Expected.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}

		if (bWithQuestNpc)
		{
			Expected.Append(Run.PartySelection.QuestNpc.SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Expected.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}
		Expected.Append(FixedRouteCards);
		return Expected;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeCombinationsTest,
	"GameXXK.Route.CardRecipe.Combinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeCombinationsTest::RunTest(const FString& Parameters)
{
	for (const bool bWithCompanion : {false, true})
	{
		for (const bool bWithQuestNpc : {false, true})
		{
			const FString CaseLabel = FString::Printf(
				TEXT("companion=%s npc=%s"),
				bWithCompanion ? TEXT("yes") : TEXT("no"),
				bWithQuestNpc ? TEXT("yes") : TEXT("no"));
			const FGameXXKCardRunState Run = MakeRecipeRun(bWithCompanion, bWithQuestNpc);
			const FGameXXKCardRunState Before = Run;
			TArray<FGameXXKRouteCardEntry> Entries;
			FString Error;
			if (!TestTrue(
				FString::Printf(TEXT("the base recipe builds (%s): %s"), *CaseLabel, *Error),
				FGameXXKRouteCardRecipe::BuildBaseEntries(Run, RecipeRouteSeed, Entries, &Error)))
			{
				continue;
			}

			TestEqual(FString::Printf(TEXT("the recipe has exactly 18 entries (%s)"), *CaseLabel), Entries.Num(), 18);
			TestTrue(
				FString::Printf(TEXT("building never changes the source run (%s)"), *CaseLabel),
				FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Run, &Before, PPF_None));
			if (Entries.Num() != 18)
			{
				continue;
			}

			const TArray<FName> ExpectedCardIds = BuildExpectedCardIds(Run, bWithCompanion, bWithQuestNpc);
			TSet<FName> EntryIds;
			for (int32 Index = 0; Index < Entries.Num(); ++Index)
			{
				const FGameXXKRouteCardEntry& Entry = Entries[Index];
				const FGameXXKCardDefinition* CardDefinition = FGameXXKCardCatalog::FindCardDefinition(Entry.CardId);
				FName ExpectedEntryId = NAME_None;
				TestTrue(
					FString::Printf(TEXT("the stable helper accepts base ordinal %d (%s)"), Index, *CaseLabel),
					FGameXXKRouteCardRecipe::MakeStableEntryId(RecipeRouteSeed, Index, ExpectedEntryId, &Error));
				TestEqual(FString::Printf(TEXT("card order %d (%s)"), Index, *CaseLabel), Entry.CardId, ExpectedCardIds[Index]);
				TestEqual(FString::Printf(TEXT("entry id %d (%s)"), Index, *CaseLabel), Entry.EntryId, ExpectedEntryId);
				TestFalse(FString::Printf(TEXT("entry id is unique %d (%s)"), Index, *CaseLabel), EntryIds.Contains(Entry.EntryId));
				EntryIds.Add(Entry.EntryId);
				TestEqual(FString::Printf(TEXT("ordinal %d (%s)"), Index, *CaseLabel), Entry.AcquisitionOrdinal, Index);
				TestFalse(FString::Printf(TEXT("base entry consumes no capacity %d (%s)"), Index, *CaseLabel), Entry.bConsumesRouteCapacity);
				TestNotNull(FString::Printf(TEXT("card exists %d (%s)"), Index, *CaseLabel), CardDefinition);
				if (CardDefinition)
				{
					TestEqual(
						FString::Printf(TEXT("quality comes from catalog %d (%s)"), Index, *CaseLabel),
						Entry.CurrentQuality,
						CardDefinition->BaseQuality);
				}

				if (Index < 8)
				{
					TestEqual(FString::Printf(TEXT("hero owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, HeroUnitId);
					TestEqual(FString::Printf(TEXT("hero source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::HeroBase);
					TestFalse(FString::Printf(TEXT("hero base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else if (Index < 13 && bWithCompanion)
				{
					TestEqual(FString::Printf(TEXT("companion owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, CompanionInstanceId);
					TestEqual(FString::Printf(TEXT("companion source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::CompanionBase);
					TestFalse(FString::Printf(TEXT("companion base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else if (Index >= 13 && Index < 16 && bWithQuestNpc)
				{
					TestEqual(FString::Printf(TEXT("npc owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, QuestNpcId);
					TestEqual(FString::Printf(TEXT("npc source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::QuestNpcBase);
					TestFalse(FString::Printf(TEXT("npc base is durable %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
				else
				{
					TestEqual(FString::Printf(TEXT("filler/fixed owner %d (%s)"), Index, *CaseLabel), Entry.OwnerUnitId, HeroUnitId);
					TestEqual(FString::Printf(TEXT("filler/fixed source %d (%s)"), Index, *CaseLabel), Entry.SourceKind, EGameXXKRouteCardSourceKind::RouteBase);
					TestTrue(FString::Printf(TEXT("filler/fixed is temporary %d (%s)"), Index, *CaseLabel), Entry.bTemporaryRouteCard);
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeStableIdentityTest,
	"GameXXK.Route.CardRecipe.StableIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeStableIdentityTest::RunTest(const FString& Parameters)
{
	const FGameXXKCardRunState Run = MakeRecipeRun(true, true);
	const FGameXXKCardRunState RunBefore = Run;
	TArray<FGameXXKRouteCardEntry> First;
	TArray<FGameXXKRouteCardEntry> Replay;
	TArray<FGameXXKRouteCardEntry> DifferentSeed;
	FString Error;
	TestTrue(TEXT("the first deterministic recipe builds"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 42, First, &Error));
	TestTrue(TEXT("the deterministic recipe replays"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 42, Replay, &Error));
	TestTrue(TEXT("a different-seed recipe builds"), FGameXXKRouteCardRecipe::BuildBaseEntries(Run, 43, DifferentSeed, &Error));
	TestTrue(TEXT("same input and seed reproduce every entry"), AreEntriesIdentical(First, Replay));
	TestTrue(TEXT("run overload remains pure"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Run, &RunBefore, PPF_None));
	if (First.Num() == 18 && DifferentSeed.Num() == 18)
	{
		for (int32 Index = 0; Index < First.Num(); ++Index)
		{
			TestNotEqual(FString::Printf(TEXT("a different route seed changes id %d"), Index), First[Index].EntryId, DifferentSeed[Index].EntryId);
			TestEqual(FString::Printf(TEXT("a different route seed preserves card order %d"), Index), First[Index].CardId, DifferentSeed[Index].CardId);
		}
	}

	FGameXXKRuntimeState Runtime;
	Runtime.CardRun = Run;
	Runtime.RouteSeed = 777;
	Runtime.CardRun.RouteRandomSeed = 888;
	Runtime.ActiveBattleNodeId = 9001;
	const FGameXXKRuntimeState RuntimeBefore = Runtime;
	TArray<FGameXXKRouteCardEntry> RuntimeEntries;
	TestTrue(TEXT("runtime overload builds from its card-run state and explicit seed"), FGameXXKRouteCardRecipe::BuildBaseEntries(Runtime, 42, RuntimeEntries, &Error));
	TestTrue(TEXT("runtime and card-run overloads agree"), AreEntriesIdentical(First, RuntimeEntries));
	TestTrue(TEXT("runtime overload remains pure"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Runtime, &RuntimeBefore, PPF_None));

	FName AcquiredEntryId = NAME_None;
	TestTrue(TEXT("the stable helper accepts acquired ordinal 18"), FGameXXKRouteCardRecipe::MakeStableEntryId(42, 18, AcquiredEntryId, &Error));
	TestEqual(TEXT("the stable id uses fixed-width normalized seed and ordinal"), AcquiredEntryId, FName(TEXT("RouteEntry.0000002A.00000012")));
	FName ZeroSeedEntryId = NAME_None;
	FName ZeroSeedReplayId = NAME_None;
	TestTrue(TEXT("zero seed uses a deterministic fallback"), FGameXXKRouteCardRecipe::MakeStableEntryId(0, 18, ZeroSeedEntryId, &Error));
	TestTrue(TEXT("zero seed fallback replays"), FGameXXKRouteCardRecipe::MakeStableEntryId(0, 18, ZeroSeedReplayId, &Error));
	TestEqual(TEXT("zero seed fallback is stable"), ZeroSeedEntryId, ZeroSeedReplayId);
	TestEqual(TEXT("zero seed fallback is explicitly normalized"), ZeroSeedEntryId, FName(TEXT("RouteEntry.13579BDF.00000012")));

	FName RejectedId(TEXT("Sentinel.Entry"));
	TestFalse(TEXT("negative ordinals are rejected"), FGameXXKRouteCardRecipe::MakeStableEntryId(42, -1, RejectedId, &Error));
	TestEqual(TEXT("negative-ordinal rejection preserves the caller output"), RejectedId, FName(TEXT("Sentinel.Entry")));
	TestTrue(TEXT("negative-ordinal rejection reports an error"), !Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeAtomicFailureTest,
	"GameXXK.Route.CardRecipe.AtomicFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeAtomicFailureTest::RunTest(const FString& Parameters)
{
	const auto MakeSentinelEntries = []()
	{
		TArray<FGameXXKRouteCardEntry> Entries;
		FGameXXKRouteCardEntry& Sentinel = Entries.AddDefaulted_GetRef();
		Sentinel.EntryId = TEXT("Sentinel.Entry");
		Sentinel.CardId = TEXT("Route.General.PoJiaTuCi");
		Sentinel.AcquisitionOrdinal = 77;
		return Entries;
	};

	FString Error;
	FGameXXKCardRunState BrokenHero = MakeRecipeRun(true, true);
	BrokenHero.HeroSelectedCardIds.Pop();
	const FGameXXKCardRunState BrokenHeroBefore = BrokenHero;
	TArray<FGameXXKRouteCardEntry> HeroOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> HeroOutputBefore = HeroOutput;
	TestFalse(TEXT("a broken hero loadout fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenHero, RecipeRouteSeed, HeroOutput, &Error));
	TestTrue(TEXT("broken hero failure preserves output"), AreEntriesIdentical(HeroOutput, HeroOutputBefore));
	TestTrue(TEXT("broken hero failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenHero, &BrokenHeroBefore, PPF_None));
	TestTrue(TEXT("broken hero failure reports an error"), !Error.IsEmpty());

	FGameXXKCardRunState BrokenCompanion = MakeRecipeRun(true, true);
	BrokenCompanion.CompanionRoster.PermanentCompanions[0].SelectedCardIds.Pop();
	const FGameXXKCardRunState BrokenCompanionBefore = BrokenCompanion;
	TArray<FGameXXKRouteCardEntry> CompanionOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> CompanionOutputBefore = CompanionOutput;
	TestFalse(TEXT("a broken companion loadout fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenCompanion, RecipeRouteSeed, CompanionOutput, &Error));
	TestTrue(TEXT("broken companion failure preserves output"), AreEntriesIdentical(CompanionOutput, CompanionOutputBefore));
	TestTrue(TEXT("broken companion failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenCompanion, &BrokenCompanionBefore, PPF_None));

	FGameXXKCardRunState LockedCompanionCard = MakeRecipeRun(true, true);
	FGameXXKPermanentCompanion& LockedCardCompanion = LockedCompanionCard.CompanionRoster.PermanentCompanions[0];
	FName LockedSameRoleCardId = NAME_None;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == LockedCardCompanion.Role
			&& !LockedCardCompanion.UnlockedPersonalCardIds.Contains(Definition.Id))
		{
			LockedSameRoleCardId = Definition.Id;
			break;
		}
	}
	TestFalse(TEXT("the corrupt-loadout fixture finds a locked same-role card"), LockedSameRoleCardId.IsNone());
	if (!LockedSameRoleCardId.IsNone())
	{
		LockedCardCompanion.SelectedCardIds.Last() = LockedSameRoleCardId;
		const FGameXXKCardRunState LockedCompanionCardBefore = LockedCompanionCard;
		TArray<FGameXXKRouteCardEntry> LockedCardOutput = MakeSentinelEntries();
		const TArray<FGameXXKRouteCardEntry> LockedCardOutputBefore = LockedCardOutput;
		TestFalse(
			TEXT("a same-role card outside the companion's unlocked personal pool fails"),
			FGameXXKRouteCardRecipe::BuildBaseEntries(LockedCompanionCard, RecipeRouteSeed, LockedCardOutput, &Error));
		TestTrue(TEXT("locked companion-card failure preserves output"), AreEntriesIdentical(LockedCardOutput, LockedCardOutputBefore));
		TestTrue(
			TEXT("locked companion-card failure preserves input"),
			FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&LockedCompanionCard, &LockedCompanionCardBefore, PPF_None));
	}

	FGameXXKCardRunState BrokenNpc = MakeRecipeRun(true, true);
	BrokenNpc.PartySelection.QuestNpc.SelectedCardIds[2] = TEXT("Card.Does.Not.Exist");
	const FGameXXKCardRunState BrokenNpcBefore = BrokenNpc;
	TArray<FGameXXKRouteCardEntry> NpcOutput = MakeSentinelEntries();
	const TArray<FGameXXKRouteCardEntry> NpcOutputBefore = NpcOutput;
	TestFalse(TEXT("a late broken NPC card fails"), FGameXXKRouteCardRecipe::BuildBaseEntries(BrokenNpc, RecipeRouteSeed, NpcOutput, &Error));
	TestTrue(TEXT("late NPC failure atomically preserves output"), AreEntriesIdentical(NpcOutput, NpcOutputBefore));
	TestTrue(TEXT("late NPC failure preserves input"), FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&BrokenNpc, &BrokenNpcBefore, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteCardRecipeStateContractTest,
	"GameXXK.Route.CardRecipe.StateContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteCardRecipeStateContractTest::RunTest(const FString& Parameters)
{
	const FGameXXKCardRunState DefaultRun;
	TestEqual(TEXT("next route-card entry ordinal defaults to zero"), DefaultRun.NextRouteCardEntryOrdinal, 0);
	TestEqual(TEXT("legacy reward ordinal still defaults independently"), DefaultRun.NextRewardOrdinal, 0);

	FGameXXKCardRunState IndependentRun;
	IndependentRun.NextRewardOrdinal = 37;
	TestEqual(TEXT("changing reward ordinal does not change route-card ordinal"), IndependentRun.NextRouteCardEntryOrdinal, 0);
	IndependentRun.NextRouteCardEntryOrdinal = 18;
	TestEqual(TEXT("changing route-card ordinal does not change reward ordinal"), IndependentRun.NextRewardOrdinal, 37);

	const FProperty* Property = FindFProperty<FProperty>(
		FGameXXKCardRunState::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FGameXXKCardRunState, NextRouteCardEntryOrdinal));
	TestNotNull(TEXT("next route-card entry ordinal is reflected"), Property);
	if (Property)
	{
		TestTrue(TEXT("next route-card entry ordinal is SaveGame state"), Property->HasAnyPropertyFlags(CPF_SaveGame));
		TestTrue(TEXT("next route-card entry ordinal is an int32"), Property->IsA<FIntProperty>());
	}
	return true;
}

#endif
