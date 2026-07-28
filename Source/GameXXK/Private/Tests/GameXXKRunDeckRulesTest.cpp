#include "GameXXKCardRunTypes.h"
#include "GameXXKRunDeckRules.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRouteCardEntry MakeRouteCardEntry(
		const FName EntryId,
		const FName CardId,
		const EGameXXKCardQuality Quality,
		const EGameXXKRouteCardSourceKind SourceKind,
		const bool bTemporary,
		const int32 AcquisitionOrdinal,
		const FName OwnerUnitId = NAME_None,
		const bool bConsumesRouteCapacity = false)
	{
		FGameXXKRouteCardEntry Entry;
		Entry.EntryId = EntryId;
		Entry.CardId = CardId;
		Entry.CurrentQuality = Quality;
		Entry.SourceKind = SourceKind;
		Entry.OwnerUnitId = OwnerUnitId;
		Entry.bTemporaryRouteCard = bTemporary;
		Entry.bConsumesRouteCapacity = bConsumesRouteCapacity;
		Entry.AcquisitionOrdinal = AcquisitionOrdinal;
		return Entry;
	}

	bool AreEntriesIdentical(
		const FGameXXKRouteCardEntry& Left,
		const FGameXXKRouteCardEntry& Right)
	{
		return Left.EntryId == Right.EntryId
			&& Left.CardId == Right.CardId
			&& Left.CurrentQuality == Right.CurrentQuality
			&& Left.SourceKind == Right.SourceKind
			&& Left.OwnerUnitId == Right.OwnerUnitId
			&& Left.bTemporaryRouteCard == Right.bTemporaryRouteCard
			&& Left.bConsumesRouteCapacity == Right.bConsumesRouteCapacity
			&& Left.AcquisitionOrdinal == Right.AcquisitionOrdinal;
	}

	bool AreEntryArraysIdentical(
		const TArray<FGameXXKRouteCardEntry>& Left,
		const TArray<FGameXXKRouteCardEntry>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!AreEntriesIdentical(Left[Index], Right[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool ArePreviewsIdentical(
		const FGameXXKCardMergePreview& Left,
		const FGameXXKCardMergePreview& Right)
	{
		return Left.bWillMerge == Right.bWillMerge
			&& Left.SurvivorEntryId == Right.SurvivorEntryId
			&& Left.ConsumedEntryIds == Right.ConsumedEntryIds
			&& Left.FinalQuality == Right.FinalQuality
			&& Left.TemporaryCountDelta == Right.TemporaryCountDelta
			&& Left.CapacityDelta == Right.CapacityDelta;
	}

	bool AreAcquisitionPreviewsIdentical(
		const FGameXXKRouteCardAcquisitionPreview& Left,
		const FGameXXKRouteCardAcquisitionPreview& Right)
	{
		return Left.Decision == Right.Decision
			&& ArePreviewsIdentical(Left.Merge, Right.Merge)
			&& Left.CapacityBefore == Right.CapacityBefore
			&& Left.CapacityAfter == Right.CapacityAfter
			&& Left.ReplacementEntryId == Right.ReplacementEntryId
			&& Left.EligibleReplacementEntryIds == Right.EligibleReplacementEntryIds;
	}

	bool AreCardRunsIdentical(
		const FGameXXKCardRunState& Left,
		const FGameXXKCardRunState& Right)
	{
		return FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	FGameXXKCardRunState MakeCapacityRun(const int32 CapacityCount, const int32 AcquisitionCount = 0)
	{
		FGameXXKCardRunState Run;
		Run.RouteProgress.ActualRouteCardAcquisitionCount = AcquisitionCount;
		for (int32 Index = 0; Index < CapacityCount; ++Index)
		{
			Run.RouteCardEntries.Add(MakeRouteCardEntry(
				FName(*FString::Printf(TEXT("Entry.Capacity.%02d"), Index)),
				FName(*FString::Printf(TEXT("Route.Test.Capacity.%02d"), Index)),
				EGameXXKCardQuality::Common,
				EGameXXKRouteCardSourceKind::RouteReward,
				false,
				Index,
				NAME_None,
				true));
		}
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRunDeckMergeTest,
	"GameXXK.Route.RunDeck.Merge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRunDeckMergeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("source kind Invalid remains zero"), static_cast<uint8>(EGameXXKRouteCardSourceKind::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("source kind HeroBase remains one"), static_cast<uint8>(EGameXXKRouteCardSourceKind::HeroBase), static_cast<uint8>(1));
	TestEqual(TEXT("source kind CompanionBase remains two"), static_cast<uint8>(EGameXXKRouteCardSourceKind::CompanionBase), static_cast<uint8>(2));
	TestEqual(TEXT("source kind QuestNpcBase remains three"), static_cast<uint8>(EGameXXKRouteCardSourceKind::QuestNpcBase), static_cast<uint8>(3));
	TestEqual(TEXT("source kind RouteReward remains four"), static_cast<uint8>(EGameXXKRouteCardSourceKind::RouteReward), static_cast<uint8>(4));
	TestEqual(TEXT("source kind Merchant remains five"), static_cast<uint8>(EGameXXKRouteCardSourceKind::Merchant), static_cast<uint8>(5));
	TestEqual(TEXT("source kind RouteBase is append-only six"), static_cast<uint8>(EGameXXKRouteCardSourceKind::RouteBase), static_cast<uint8>(6));

	const FGameXXKRouteCardEntry DefaultEntry;
	TestTrue(TEXT("route entry defaults to an empty entry ID"), DefaultEntry.EntryId.IsNone());
	TestTrue(TEXT("route entry defaults to an empty card ID"), DefaultEntry.CardId.IsNone());
	TestEqual(TEXT("route entry defaults to Common quality"), DefaultEntry.CurrentQuality, EGameXXKCardQuality::Common);
	TestEqual(TEXT("route entry defaults to Invalid source"), DefaultEntry.SourceKind, EGameXXKRouteCardSourceKind::Invalid);
	TestTrue(TEXT("route entry defaults to an empty owner"), DefaultEntry.OwnerUnitId.IsNone());
	TestFalse(TEXT("route entry defaults to a base card"), DefaultEntry.bTemporaryRouteCard);
	TestFalse(TEXT("route entry defaults to not consuming route capacity"), DefaultEntry.bConsumesRouteCapacity);
	TestEqual(TEXT("route entry defaults to no acquisition ordinal"), DefaultEntry.AcquisitionOrdinal, INDEX_NONE);
	const FBoolProperty* CapacityProperty = FindFProperty<FBoolProperty>(
		FGameXXKRouteCardEntry::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FGameXXKRouteCardEntry, bConsumesRouteCapacity));
	TestNotNull(TEXT("route capacity authority is reflected as a bool property"), CapacityProperty);
	if (CapacityProperty)
	{
		TestTrue(TEXT("route capacity authority is persisted by SaveGame serialization"), CapacityProperty->HasAnyPropertyFlags(CPF_SaveGame));
	}

	const FGameXXKCardRunState DefaultRunState;
	TestTrue(TEXT("new route-card entry state defaults empty"), DefaultRunState.RouteCardEntries.IsEmpty());
	TestTrue(TEXT("legacy route-card ID state remains present and defaults empty"), DefaultRunState.RouteCardIds.IsEmpty());

	const FName TestCardId(TEXT("Route.General.PoJiaTuCi"));
	const FName OtherCardId(TEXT("Route.General.ShouShiHuiYuan"));

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Common.Old"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 10)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Common.New"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::Merchant, true, 11);
		FGameXXKCardMergePreview Applied;
		FString Error;
		TestTrue(TEXT("two Common entries can be added and merged"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied, &Error));
		TestTrue(TEXT("valid Common merge leaves no error"), Error.IsEmpty());
		TestEqual(TEXT("two Common entries produce one entry"), Entries.Num(), 1);
		TestEqual(TEXT("two Common entries produce Rare"), Entries[0].CurrentQuality, EGameXXKCardQuality::Rare);
		TestEqual(TEXT("earlier Common entry survives"), Entries[0].EntryId, FName(TEXT("Entry.Common.Old")));
		TestTrue(TEXT("Common pair reports a merge"), Applied.bWillMerge);
		TestEqual(TEXT("Common pair reports its survivor"), Applied.SurvivorEntryId, Entries[0].EntryId);
		TestEqual(TEXT("Common pair reports one consumed entry"), Applied.ConsumedEntryIds.Num(), 1);
		if (Applied.ConsumedEntryIds.Num() == 1)
		{
			TestEqual(TEXT("Common pair reports the later entry as consumed"), Applied.ConsumedEntryIds[0], Candidate.EntryId);
		}
		TestEqual(TEXT("Common pair reports Rare final quality"), Applied.FinalQuality, EGameXXKCardQuality::Rare);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Rare.Old"), TestCardId, EGameXXKCardQuality::Rare, EGameXXKRouteCardSourceKind::HeroBase, false, 20, TEXT("Hero.Player"))
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Rare.New"), TestCardId, EGameXXKCardQuality::Rare, EGameXXKRouteCardSourceKind::Merchant, true, 21, TEXT("Npc.Merchant"));
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("two Rare entries can be added and merged"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("two Rare entries produce one entry"), Entries.Num(), 1);
		TestEqual(TEXT("two Rare entries produce Epic"), Entries[0].CurrentQuality, EGameXXKCardQuality::Epic);
		TestEqual(TEXT("Rare pair reports Epic final quality"), Applied.FinalQuality, EGameXXKCardQuality::Epic);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Chain.1"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 30),
			MakeRouteCardEntry(TEXT("Entry.Chain.2"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 31),
			MakeRouteCardEntry(TEXT("Entry.Chain.3"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 32)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Chain.4"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::Merchant, true, 33);
		FGameXXKCardMergePreview Preview;
		FString Error;
		TestTrue(TEXT("four Common entries can be previewed"), FGameXXKRunDeckRules::PreviewAdd(Entries, Candidate, Preview, &Error));
		TestEqual(TEXT("four Common entries preview an Epic"), Preview.FinalQuality, EGameXXKCardQuality::Epic);
		TestEqual(TEXT("four Common entries preview three consumed IDs"), Preview.ConsumedEntryIds.Num(), 3);
		TSet<FName> UniqueConsumedIds(Preview.ConsumedEntryIds);
		TestEqual(TEXT("chained preview reports each consumed ID once"), UniqueConsumedIds.Num(), Preview.ConsumedEntryIds.Num());
		if (Preview.ConsumedEntryIds.Num() == 3)
		{
			TestEqual(TEXT("first Common pair records its consumed entry first"), Preview.ConsumedEntryIds[0], FName(TEXT("Entry.Chain.2")));
			TestEqual(TEXT("second Common pair records its consumed entry second"), Preview.ConsumedEntryIds[1], FName(TEXT("Entry.Chain.4")));
			TestEqual(TEXT("Rare chain pair records its consumed entry last"), Preview.ConsumedEntryIds[2], FName(TEXT("Entry.Chain.3")));
		}
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("four Common entries can be committed"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied, &Error));
		TestEqual(TEXT("four Common entries chain into one"), Entries.Num(), 1);
		TestEqual(TEXT("four Common entries chain to Epic"), Entries[0].CurrentQuality, EGameXXKCardQuality::Epic);
		TestTrue(TEXT("chain preview and commit are identical"), ArePreviewsIdentical(Preview, Applied));
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Epic.Old"), TestCardId, EGameXXKCardQuality::Epic, EGameXXKRouteCardSourceKind::HeroBase, false, 40)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Epic.New"), TestCardId, EGameXXKCardQuality::Epic, EGameXXKRouteCardSourceKind::Merchant, true, 41);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("an Epic candidate can coexist with an Epic entry"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("Epic plus Epic keeps both entries"), Entries.Num(), 2);
		TestEqual(TEXT("existing Epic stays Epic"), Entries[0].CurrentQuality, EGameXXKCardQuality::Epic);
		TestEqual(TEXT("candidate Epic stays Epic"), Entries[1].CurrentQuality, EGameXXKCardQuality::Epic);
		TestFalse(TEXT("Epic plus Epic reports no merge"), Applied.bWillMerge);
		TestTrue(TEXT("Epic plus Epic consumes nothing"), Applied.ConsumedEntryIds.IsEmpty());
		TestEqual(TEXT("no-merge preview identifies the appended candidate"), Applied.SurvivorEntryId, Candidate.EntryId);
		TestEqual(TEXT("no-merge Epic preview keeps Epic quality"), Applied.FinalQuality, EGameXXKCardQuality::Epic);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Different.A"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 50)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Different.B"), OtherCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 51);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("a different card ID can be appended"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("different card IDs remain separate"), Entries.Num(), 2);
		TestEqual(TEXT("different first card keeps its ID"), Entries[0].CardId, TestCardId);
		TestEqual(TEXT("different candidate keeps its ID"), Entries[1].CardId, OtherCardId);
		TestFalse(TEXT("different card IDs report no merge"), Applied.bWillMerge);
		TestEqual(TEXT("temporary no-merge append reports plus one"), Applied.TemporaryCountDelta, 1);
		TestEqual(TEXT("non-capacity no-merge append reports zero capacity delta"), Applied.CapacityDelta, 0);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(
				TEXT("Entry.Priority.Capacity"),
				TestCardId,
				EGameXXKCardQuality::Common,
				EGameXXKRouteCardSourceKind::RouteReward,
				false,
				1,
				NAME_None,
				true)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Priority.NonCapacityTemporary"),
			TestCardId,
			EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteBase,
			true,
			99);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("a non-capacity temporary candidate can merge with a stable-capacity entry"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("non-capacity wins survivor priority before temporary and ordinal"), Entries[0].EntryId, Candidate.EntryId);
		TestFalse(TEXT("non-capacity survivor remains outside route capacity"), Entries[0].bConsumesRouteCapacity);
		TestEqual(TEXT("merge removes one pre-existing capacity entry"), Applied.CapacityDelta, -1);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.DifferentQuality.Common"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 52)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.DifferentQuality.Rare"), TestCardId, EGameXXKCardQuality::Rare, EGameXXKRouteCardSourceKind::Merchant, true, 53);
		const TArray<FGameXXKRouteCardEntry> BeforePreview = Entries;
		FGameXXKCardMergePreview Preview;
		FString Error;
		TestTrue(TEXT("same card with different quality can be previewed"), FGameXXKRunDeckRules::PreviewAdd(Entries, Candidate, Preview, &Error));
		TestTrue(TEXT("different-quality preview leaves input exact"), AreEntryArraysIdentical(Entries, BeforePreview));
		TestFalse(TEXT("same card with Common and Rare does not merge"), Preview.bWillMerge);
		TestEqual(TEXT("different-quality preview identifies appended candidate"), Preview.SurvivorEntryId, Candidate.EntryId);
		TestTrue(TEXT("different-quality preview consumes nothing"), Preview.ConsumedEntryIds.IsEmpty());
		TestEqual(TEXT("different-quality preview keeps candidate quality"), Preview.FinalQuality, EGameXXKCardQuality::Rare);
		TestEqual(TEXT("different-quality temporary candidate increases count by one"), Preview.TemporaryCountDelta, 1);

		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("same card with different quality can be committed"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied, &Error));
		TestTrue(TEXT("different-quality preview and commit are identical"), ArePreviewsIdentical(Preview, Applied));
		TestEqual(TEXT("different-quality add keeps both entries"), Entries.Num(), 2);
		if (Entries.Num() == 2)
		{
			TestEqual(TEXT("different-quality existing entry stays first"), Entries[0].EntryId, FName(TEXT("Entry.DifferentQuality.Common")));
			TestEqual(TEXT("different-quality existing entry stays Common"), Entries[0].CurrentQuality, EGameXXKCardQuality::Common);
			TestEqual(TEXT("different-quality candidate appends last"), Entries[1].EntryId, Candidate.EntryId);
			TestEqual(TEXT("different-quality candidate stays Rare"), Entries[1].CurrentQuality, EGameXXKCardQuality::Rare);
		}
	}

	{
		const FName BaseOwner(TEXT("Hero.Player"));
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Priority.Temporary"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 1, TEXT("Route.Owner"))
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Priority.Base"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 99, BaseOwner);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("a base candidate can merge with an earlier temporary entry"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("base entry wins over an earlier temporary entry"), Entries[0].EntryId, Candidate.EntryId);
		TestEqual(TEXT("survivor retains base owner"), Entries[0].OwnerUnitId, BaseOwner);
		TestEqual(TEXT("survivor retains base source"), Entries[0].SourceKind, EGameXXKRouteCardSourceKind::HeroBase);
		TestFalse(TEXT("survivor retains base temporary flag"), Entries[0].bTemporaryRouteCard);
		TestEqual(TEXT("survivor retains base ordinal"), Entries[0].AcquisitionOrdinal, Candidate.AcquisitionOrdinal);
		TestEqual(TEXT("base survivor alone changes quality"), Entries[0].CurrentQuality, EGameXXKCardQuality::Rare);
		TestEqual(TEXT("consuming one pre-existing temporary entry reports minus one"), Applied.TemporaryCountDelta, -1);
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Ordinal.Other.Before"), OtherCardId, EGameXXKCardQuality::Rare, EGameXXKRouteCardSourceKind::CompanionBase, false, 19),
			MakeRouteCardEntry(TEXT("Entry.Ordinal.Existing.21"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 21, TEXT("Owner.Existing")),
			MakeRouteCardEntry(TEXT("Entry.Ordinal.Other.After"), OtherCardId, EGameXXKCardQuality::Epic, EGameXXKRouteCardSourceKind::QuestNpcBase, false, 22)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Ordinal.Candidate.0"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::Merchant, true, 0, TEXT("Owner.Candidate"));
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("ordinal zero candidate is valid and can merge"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("ordinal merge removes only the existing matching entry"), Entries.Num(), 3);
		TestEqual(TEXT("unrelated entry before consumed entry stays first"), Entries[0].EntryId, FName(TEXT("Entry.Ordinal.Other.Before")));
		TestEqual(TEXT("unrelated entry after consumed entry keeps relative order"), Entries[1].EntryId, FName(TEXT("Entry.Ordinal.Other.After")));
		TestEqual(TEXT("lower-ordinal appended candidate survives at the end"), Entries[2].EntryId, Candidate.EntryId);
		TestEqual(TEXT("ordinal-zero survivor retains owner"), Entries[2].OwnerUnitId, FName(TEXT("Owner.Candidate")));
		TestEqual(TEXT("ordinal-zero survivor upgrades to Rare"), Entries[2].CurrentQuality, EGameXXKCardQuality::Rare);
		TestEqual(TEXT("ordinal-zero candidate is the reported survivor"), Applied.SurvivorEntryId, Candidate.EntryId);
		TestEqual(TEXT("temporary plus temporary has zero temporary-count delta"), Applied.TemporaryCountDelta, 0);
		TestEqual(TEXT("ordinal merge reports one consumed entry"), Applied.ConsumedEntryIds.Num(), 1);
		if (Applied.ConsumedEntryIds.Num() == 1)
		{
			TestEqual(TEXT("earlier-array matching entry is consumed"), Applied.ConsumedEntryIds[0], FName(TEXT("Entry.Ordinal.Existing.21")));
		}
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.Order.Other.Before"), OtherCardId, EGameXXKCardQuality::Rare, EGameXXKRouteCardSourceKind::CompanionBase, false, 60),
			MakeRouteCardEntry(TEXT("Entry.Order.Merge"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 61),
			MakeRouteCardEntry(TEXT("Entry.Order.Other.After"), OtherCardId, EGameXXKCardQuality::Epic, EGameXXKRouteCardSourceKind::QuestNpcBase, false, 62)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Order.ConsumedCandidate"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 63);
		const TArray<FGameXXKRouteCardEntry> BeforePreview = Entries;
		FGameXXKCardMergePreview Preview;
		FString Error;
		TestTrue(TEXT("ordered merge can be previewed"), FGameXXKRunDeckRules::PreviewAdd(Entries, Candidate, Preview, &Error));
		TestTrue(TEXT("preview is pure"), AreEntryArraysIdentical(Entries, BeforePreview));
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("ordered merge can be committed"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied, &Error));
		TestTrue(TEXT("ordered preview matches commit"), ArePreviewsIdentical(Preview, Applied));
		TestEqual(TEXT("ordered merge removes only the consumed entry"), Entries.Num(), 3);
		TestEqual(TEXT("entry before survivor retains position"), Entries[0].EntryId, FName(TEXT("Entry.Order.Other.Before")));
		TestEqual(TEXT("survivor retains its original position"), Entries[1].EntryId, FName(TEXT("Entry.Order.Merge")));
		TestEqual(TEXT("entry after survivor retains position"), Entries[2].EntryId, FName(TEXT("Entry.Order.Other.After")));
	}

	{
		TArray<FGameXXKRouteCardEntry> Entries = {
			MakeRouteCardEntry(TEXT("Entry.TempChain.1"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 70),
			MakeRouteCardEntry(TEXT("Entry.TempChain.2"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 71),
			MakeRouteCardEntry(TEXT("Entry.TempChain.3"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 72)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.TempChain.Base"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 73);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("base candidate can trigger a full temporary chain"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied));
		TestEqual(TEXT("base candidate chains four Common into one Epic"), Entries[0].CurrentQuality, EGameXXKCardQuality::Epic);
		TestEqual(TEXT("base candidate consumes all three prior temporary entries"), Applied.TemporaryCountDelta, -3);
		TestEqual(TEXT("negative temporary delta chain consumes three entries"), Applied.ConsumedEntryIds.Num(), 3);
	}

	{
		FGameXXKCardRunState RunState;
		RunState.RouteCardIds = { TEXT("Legacy.Route.Card") };
		RunState.RouteCardEntries = {
			MakeRouteCardEntry(TEXT("Entry.Legacy.Isolation"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 80)
		};
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Legacy.Isolation.Candidate"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 81);
		FGameXXKCardMergePreview Applied;
		TestTrue(TEXT("entry rules can operate beside legacy input"), FGameXXKRunDeckRules::AddAndMerge(RunState.RouteCardEntries, Candidate, Applied));
		TestEqual(TEXT("entry rules never rewrite legacy route-card IDs"), RunState.RouteCardIds.Num(), 1);
		TestEqual(TEXT("legacy route-card ID remains exact"), RunState.RouteCardIds[0], FName(TEXT("Legacy.Route.Card")));
	}

	auto TestRejectedWithoutMutation = [this](
		const FString& Label,
		TArray<FGameXXKRouteCardEntry> Entries,
		const FGameXXKRouteCardEntry& Candidate)
	{
		const TArray<FGameXXKRouteCardEntry> Before = Entries;
		FGameXXKCardMergePreview Preview;
		FString PreviewError;
		TestFalse(Label + TEXT(" preview is rejected"), FGameXXKRunDeckRules::PreviewAdd(Entries, Candidate, Preview, &PreviewError));
		TestFalse(Label + TEXT(" preview explains rejection"), PreviewError.IsEmpty());
		TestTrue(Label + TEXT(" preview leaves input exact"), AreEntryArraysIdentical(Entries, Before));

		FGameXXKCardMergePreview Applied;
		FString CommitError;
		TestFalse(Label + TEXT(" commit is rejected"), FGameXXKRunDeckRules::AddAndMerge(Entries, Candidate, Applied, &CommitError));
		TestFalse(Label + TEXT(" commit explains rejection"), CommitError.IsEmpty());
		TestTrue(Label + TEXT(" failed commit leaves input exact"), AreEntryArraysIdentical(Entries, Before));
	};

	const FGameXXKRouteCardEntry ValidExisting = MakeRouteCardEntry(
		TEXT("Entry.Valid.Existing"), TestCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::HeroBase, false, 100);
	const FGameXXKRouteCardEntry ValidCandidate = MakeRouteCardEntry(
		TEXT("Entry.Valid.Candidate"), OtherCardId, EGameXXKCardQuality::Common, EGameXXKRouteCardSourceKind::RouteReward, true, 101);

	{
		FGameXXKRouteCardEntry Invalid = ValidCandidate;
		Invalid.EntryId = NAME_None;
		TestRejectedWithoutMutation(TEXT("empty candidate entry ID"), { ValidExisting }, Invalid);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidExisting;
		Invalid.EntryId = NAME_None;
		TestRejectedWithoutMutation(TEXT("empty existing entry ID"), { Invalid }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidCandidate;
		Invalid.CardId = NAME_None;
		TestRejectedWithoutMutation(TEXT("empty candidate card ID"), { ValidExisting }, Invalid);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidExisting;
		Invalid.CardId = NAME_None;
		TestRejectedWithoutMutation(TEXT("empty existing card ID"), { Invalid }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry DuplicateId = ValidCandidate;
		DuplicateId.EntryId = ValidExisting.EntryId;
		TestRejectedWithoutMutation(TEXT("candidate duplicate entry ID"), { ValidExisting }, DuplicateId);
	}
	{
		FGameXXKRouteCardEntry DuplicateExisting = ValidExisting;
		DuplicateExisting.CardId = OtherCardId;
		DuplicateExisting.AcquisitionOrdinal = 102;
		TestRejectedWithoutMutation(TEXT("existing duplicate entry ID"), { ValidExisting, DuplicateExisting }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry DuplicateOrdinal = ValidCandidate;
		DuplicateOrdinal.AcquisitionOrdinal = ValidExisting.AcquisitionOrdinal;
		TestRejectedWithoutMutation(TEXT("candidate duplicate ordinal"), { ValidExisting }, DuplicateOrdinal);
	}
	{
		FGameXXKRouteCardEntry DuplicateExisting = ValidExisting;
		DuplicateExisting.EntryId = TEXT("Entry.Valid.Existing.Second");
		TestRejectedWithoutMutation(TEXT("existing duplicate ordinal"), { ValidExisting, DuplicateExisting }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidCandidate;
		Invalid.CurrentQuality = EGameXXKCardQuality::Invalid;
		TestRejectedWithoutMutation(TEXT("Invalid candidate quality"), { ValidExisting }, Invalid);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidExisting;
		Invalid.CurrentQuality = static_cast<EGameXXKCardQuality>(255);
		TestRejectedWithoutMutation(TEXT("out-of-range existing quality"), { Invalid }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidCandidate;
		Invalid.SourceKind = EGameXXKRouteCardSourceKind::Invalid;
		TestRejectedWithoutMutation(TEXT("Invalid candidate source"), { ValidExisting }, Invalid);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidExisting;
		Invalid.SourceKind = static_cast<EGameXXKRouteCardSourceKind>(255);
		TestRejectedWithoutMutation(TEXT("out-of-range existing source"), { Invalid }, ValidCandidate);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidCandidate;
		Invalid.AcquisitionOrdinal = INDEX_NONE;
		TestRejectedWithoutMutation(TEXT("negative candidate ordinal"), { ValidExisting }, Invalid);
	}
	{
		FGameXXKRouteCardEntry Invalid = ValidExisting;
		Invalid.AcquisitionOrdinal = -2;
		TestRejectedWithoutMutation(TEXT("negative existing ordinal"), { Invalid }, ValidCandidate);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRunDeckAcquisitionTest,
	"GameXXK.Route.RunDeck.Acquisition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRunDeckAcquisitionTest::RunTest(const FString& Parameters)
{
	const FName CandidateCardId(TEXT("Route.Test.Acquisition.Candidate"));

	{
		FGameXXKCardRunState Run;
		for (int32 Index = 0; Index < 18; ++Index)
		{
			const EGameXXKRouteCardSourceKind SourceKind = Index < 6
				? EGameXXKRouteCardSourceKind::HeroBase
				: (Index < 12 ? EGameXXKRouteCardSourceKind::CompanionBase : EGameXXKRouteCardSourceKind::QuestNpcBase);
			Run.RouteCardEntries.Add(MakeRouteCardEntry(
				FName(*FString::Printf(TEXT("Entry.Base.%02d"), Index)),
				FName(*FString::Printf(TEXT("Route.Test.Base.%02d"), Index)),
				EGameXXKCardQuality::Common,
				SourceKind,
				false,
				Index));
		}
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.RouteBase.Temporary"),
			CandidateCardId,
			EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteBase,
			true,
			18);
		const TArray<FGameXXKRouteCardEntry> Before = Run.RouteCardEntries;
		int32 CapacityUsed = INDEX_NONE;
		FString Error;
		TestTrue(TEXT("validated capacity query accepts eighteen base entries"),
			FGameXXKRunDeckRules::GetCapacityUsed(Run.RouteCardEntries, CapacityUsed, &Error));
		TestEqual(TEXT("eighteen base entries consume zero capacity"), CapacityUsed, 0);
		FGameXXKCardMergePreview Preview;
		TestTrue(TEXT("temporary RouteBase refill can be previewed by pure merge rules"),
			FGameXXKRunDeckRules::PreviewAdd(Run.RouteCardEntries, Candidate, Preview, &Error));
		TestTrue(TEXT("base refill merge preview leaves entries exact"), AreEntryArraysIdentical(Run.RouteCardEntries, Before));
		TestEqual(TEXT("temporary RouteBase refill has zero capacity delta"), Preview.CapacityDelta, 0);

		TArray<FGameXXKRouteCardEntry> InvalidEntries = Run.RouteCardEntries;
		InvalidEntries[1].EntryId = InvalidEntries[0].EntryId;
		const TArray<FGameXXKRouteCardEntry> InvalidEntriesBefore = InvalidEntries;
		CapacityUsed = 123;
		Error.Reset();
		TestFalse(TEXT("validated capacity query rejects duplicate stable EntryIds"),
			FGameXXKRunDeckRules::GetCapacityUsed(InvalidEntries, CapacityUsed, &Error));
		TestFalse(TEXT("validated capacity query explains invalid input"), Error.IsEmpty());
		TestEqual(TEXT("failed capacity query clears stale output"), CapacityUsed, 0);
		TestTrue(TEXT("failed capacity query leaves input entries exact"), AreEntryArraysIdentical(InvalidEntries, InvalidEntriesBefore));
	}

	{
		FGameXXKCardRunState Run = MakeCapacityRun(0, 4);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Capacity.First"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, false, 0, NAME_None, true);
		const FGameXXKCardRunState Before = Run;
		FGameXXKRouteCardAcquisitionPreview Preview;
		TestTrue(TEXT("zero to one acquisition can be previewed"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, NAME_None, Preview));
		TestTrue(TEXT("zero to one preview is pure"), AreCardRunsIdentical(Run, Before));
		TestEqual(TEXT("zero to one capacity before"), Preview.CapacityBefore, 0);
		TestEqual(TEXT("zero to one capacity after"), Preview.CapacityAfter, 1);
		FGameXXKRouteCardAcquisitionPreview Applied;
		TestTrue(TEXT("zero to one acquisition commits"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, NAME_None, Applied));
		TestTrue(TEXT("zero to one preview and commit summaries match"), AreAcquisitionPreviewsIdentical(Preview, Applied));
		TestEqual(TEXT("zero to one commit increments cumulative count once"), Run.RouteProgress.ActualRouteCardAcquisitionCount, 5);
	}

	{
		FGameXXKCardRunState Run = MakeCapacityRun(11, 7);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Capacity.11"), CandidateCardId, EGameXXKCardQuality::Rare,
			EGameXXKRouteCardSourceKind::Merchant, true, 11, NAME_None, true);
		const FGameXXKCardRunState BeforePreview = Run;
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString PreviewError;
		TestTrue(TEXT("eleven to twelve acquisition can be previewed"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, NAME_None, Preview, &PreviewError));
		TestTrue(TEXT("eleven to twelve preview is pure"), AreCardRunsIdentical(Run, BeforePreview));
		TestEqual(TEXT("eleven to twelve capacity before"), Preview.CapacityBefore, 11);
		TestEqual(TEXT("eleven to twelve capacity after"), Preview.CapacityAfter, 12);
		TestEqual(TEXT("eleven to twelve needs no replacement"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
		TestEqual(TEXT("capacity append reports plus one"), Preview.Merge.CapacityDelta, 1);

		FGameXXKRouteCardAcquisitionPreview Applied;
		FString CommitError;
		TestTrue(TEXT("eleven to twelve acquisition commits"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, NAME_None, Applied, &CommitError));
		TestTrue(TEXT("eleven to twelve preview and commit summaries match"), AreAcquisitionPreviewsIdentical(Preview, Applied));
		TestEqual(TEXT("eleven to twelve commit stores twelve capacity entries"), Run.RouteCardEntries.Num(), 12);
		TestEqual(TEXT("successful normal acquisition increments cumulative count once"), Run.RouteProgress.ActualRouteCardAcquisitionCount, 8);
	}

	FGameXXKCardRunState FullDifferentQualityRun = MakeCapacityRun(12, 20);
	FullDifferentQualityRun.RouteCardEntries[0].CardId = CandidateCardId;
	const FGameXXKRouteCardEntry DifferentQualityCandidate = MakeRouteCardEntry(
		TEXT("Entry.Full.DifferentQuality.Candidate"), CandidateCardId, EGameXXKCardQuality::Rare,
		EGameXXKRouteCardSourceKind::Merchant, true, 12, NAME_None, true);

	{
		FGameXXKCardRunState Run = MakeCapacityRun(12, 30);
		Run.RouteCardEntries[0].CardId = CandidateCardId;
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Full.Merge.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, true, 12, NAME_None, true);
		FGameXXKRouteCardAcquisitionPreview Preview;
		TestTrue(TEXT("full same-card same-quality acquisition previews without replacement"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, NAME_None, Preview));
		TestEqual(TEXT("full merge starts at twelve"), Preview.CapacityBefore, 12);
		TestEqual(TEXT("full merge remains at twelve"), Preview.CapacityAfter, 12);
		TestEqual(TEXT("full merge can commit directly"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
		TestTrue(TEXT("full merge reports merge"), Preview.Merge.bWillMerge);
		TestEqual(TEXT("full merge has zero capacity delta"), Preview.Merge.CapacityDelta, 0);
		TestTrue(TEXT("full merge has no replacement candidates"), Preview.EligibleReplacementEntryIds.IsEmpty());
		FGameXXKRouteCardAcquisitionPreview Applied;
		TestTrue(TEXT("full merge commits"), FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, NAME_None, Applied));
		TestTrue(TEXT("full merge preview and commit summaries match"), AreAcquisitionPreviewsIdentical(Preview, Applied));
		TestEqual(TEXT("successful merge acquisition increments cumulative count once"), Run.RouteProgress.ActualRouteCardAcquisitionCount, 31);
	}

	FGameXXKRouteCardAcquisitionPreview RequiresPreview;
	{
		FGameXXKCardRunState Run = FullDifferentQualityRun;
		const FGameXXKCardRunState Before = Run;
		FString Error;
		TestTrue(TEXT("full different-quality acquisition previews as a replacement decision"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, DifferentQualityCandidate, NAME_None, RequiresPreview, &Error));
		TestTrue(TEXT("replacement decision is not an error"), Error.IsEmpty());
		TestEqual(TEXT("replacement decision starts at twelve"), RequiresPreview.CapacityBefore, 12);
		TestEqual(TEXT("replacement decision exposes simulated thirteen"), RequiresPreview.CapacityAfter, 13);
		TestEqual(TEXT("different-quality full acquisition requires replacement"), RequiresPreview.Decision, EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);
		TestEqual(TEXT("all twelve stable capacity entries are eligible"), RequiresPreview.EligibleReplacementEntryIds.Num(), 12);
		if (RequiresPreview.EligibleReplacementEntryIds.Num() == 12)
		{
			TestEqual(TEXT("eligible replacement order begins with original first entry"), RequiresPreview.EligibleReplacementEntryIds[0], Run.RouteCardEntries[0].EntryId);
			TestEqual(TEXT("eligible replacement order ends with original last entry"), RequiresPreview.EligibleReplacementEntryIds.Last(), Run.RouteCardEntries.Last().EntryId);
		}
		TestTrue(TEXT("requires-replacement preview leaves full state exact"), AreCardRunsIdentical(Run, Before));

		FGameXXKRouteCardAcquisitionPreview Applied;
		TestFalse(TEXT("commit without required replacement does not commit"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, DifferentQualityCandidate, NAME_None, Applied, &Error));
		TestTrue(TEXT("requires-replacement commit summary matches preview"), AreAcquisitionPreviewsIdentical(RequiresPreview, Applied));
		TestTrue(TEXT("requires-replacement commit leaves full state exact"), AreCardRunsIdentical(Run, Before));
	}

	{
		FGameXXKCardRunState Run = FullDifferentQualityRun;
		Run.RouteCardEntries.Add(MakeRouteCardEntry(
			TEXT("Entry.Base.NotEligible"), TEXT("Route.Test.Base.NotEligible"), EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteBase, true, 13));
		const FGameXXKCardRunState Before = Run;
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString Error;
		TestFalse(TEXT("non-capacity stable entry cannot be selected for replacement"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, DifferentQualityCandidate, TEXT("Entry.Base.NotEligible"), Preview, &Error));
		TestFalse(TEXT("non-capacity replacement rejection explains error"), Error.IsEmpty());
		TestTrue(TEXT("invalid non-capacity replacement leaves state exact"), AreCardRunsIdentical(Run, Before));
		TestFalse(TEXT("candidate itself cannot be selected for replacement"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, DifferentQualityCandidate, DifferentQualityCandidate.EntryId, Preview, &Error));
		TestFalse(TEXT("unknown entry cannot be selected for replacement"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, DifferentQualityCandidate, TEXT("Entry.Unknown"), Preview, &Error));
	}

	{
		FGameXXKCardRunState Run = MakeCapacityRun(12, 40);
		const FName DuplicateCardId(TEXT("Route.Test.DuplicateReplacement"));
		Run.RouteCardEntries[0].CardId = DuplicateCardId;
		Run.RouteCardEntries[0].CurrentQuality = EGameXXKCardQuality::Common;
		Run.RouteCardEntries[1].CardId = DuplicateCardId;
		Run.RouteCardEntries[1].CurrentQuality = EGameXXKCardQuality::Rare;
		const FName KeptEntryId = Run.RouteCardEntries[0].EntryId;
		const FName ReplacedEntryId = Run.RouteCardEntries[1].EntryId;
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.Replacement.Candidate"), CandidateCardId, EGameXXKCardQuality::Epic,
			EGameXXKRouteCardSourceKind::Merchant, true, 12, NAME_None, true);
		FGameXXKRouteCardAcquisitionPreview Preview;
		TestTrue(TEXT("exact duplicate-card EntryId replacement can be previewed"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, ReplacedEntryId, Preview));
		TestEqual(TEXT("replacement preview records exact selected EntryId"), Preview.ReplacementEntryId, ReplacedEntryId);
		TestEqual(TEXT("replacement preview returns to twelve"), Preview.CapacityAfter, 12);
		FGameXXKRouteCardAcquisitionPreview Applied;
		TestTrue(TEXT("exact duplicate-card EntryId replacement commits"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, ReplacedEntryId, Applied));
		TestTrue(TEXT("replacement preview and commit summaries match"), AreAcquisitionPreviewsIdentical(Preview, Applied));
		TestNotNull(TEXT("same-CardId unselected entry remains"), Run.RouteCardEntries.FindByPredicate([KeptEntryId](const FGameXXKRouteCardEntry& Entry) { return Entry.EntryId == KeptEntryId; }));
		TestNull(TEXT("exact selected EntryId is removed"), Run.RouteCardEntries.FindByPredicate([ReplacedEntryId](const FGameXXKRouteCardEntry& Entry) { return Entry.EntryId == ReplacedEntryId; }));
		TestNotNull(TEXT("replacement candidate is stored"), Run.RouteCardEntries.FindByPredicate([Candidate](const FGameXXKRouteCardEntry& Entry) { return Entry.EntryId == Candidate.EntryId; }));
		TestEqual(TEXT("successful replacement acquisition increments cumulative count once"), Run.RouteProgress.ActualRouteCardAcquisitionCount, 41);
	}

	{
		FGameXXKCardRunState Run = MakeCapacityRun(11, 50);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.UnneededReplacement.Candidate"), CandidateCardId, EGameXXKCardQuality::Epic,
			EGameXXKRouteCardSourceKind::RouteReward, true, 11, NAME_None, true);
		const FGameXXKCardRunState Before = Run;
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString Error;
		TestFalse(TEXT("replacement argument is rejected when capacity does not require it"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, Run.RouteCardEntries[0].EntryId, Preview, &Error));
		TestFalse(TEXT("unneeded replacement rejection explains error"), Error.IsEmpty());
		TestTrue(TEXT("unneeded replacement preview leaves state exact"), AreCardRunsIdentical(Run, Before));
		TestFalse(TEXT("unneeded replacement commit is rejected"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, Run.RouteCardEntries[0].EntryId, Preview, &Error));
		TestTrue(TEXT("unneeded replacement commit leaves state exact"), AreCardRunsIdentical(Run, Before));
	}

	auto TestRejectedAtomically = [this](
		const FString& Label,
		FGameXXKCardRunState Run,
		const FGameXXKRouteCardEntry& Candidate)
	{
		const FGameXXKCardRunState Before = Run;
		FGameXXKRouteCardAcquisitionPreview Preview;
		FString Error;
		TestFalse(Label + TEXT(" preview is rejected"),
			FGameXXKRunDeckRules::PreviewAcquisition(Run, Candidate, NAME_None, Preview, &Error));
		TestFalse(Label + TEXT(" preview explains rejection"), Error.IsEmpty());
		TestTrue(Label + TEXT(" preview leaves full state exact"), AreCardRunsIdentical(Run, Before));
		FGameXXKRouteCardAcquisitionPreview Applied;
		TestFalse(Label + TEXT(" commit is rejected"),
			FGameXXKRunDeckRules::CommitAcquisition(Run, Candidate, NAME_None, Applied, &Error));
		TestTrue(Label + TEXT(" commit leaves full state exact"), AreCardRunsIdentical(Run, Before));
	};

	{
		FGameXXKCardRunState Run = MakeCapacityRun(0, MAX_int32);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.MaxCount.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, true, 0, NAME_None, true);
		TestRejectedAtomically(TEXT("MAX_int32 cumulative acquisition count"), Run, Candidate);
	}
	{
		FGameXXKCardRunState Run = MakeCapacityRun(0, -1);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.NegativeCount.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, true, 0, NAME_None, true);
		TestRejectedAtomically(TEXT("negative cumulative acquisition count"), Run, Candidate);
	}
	{
		FGameXXKCardRunState Run = MakeCapacityRun(13, 60);
		const FGameXXKRouteCardEntry Candidate = MakeRouteCardEntry(
			TEXT("Entry.OverflowingDeck.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, true, 13, NAME_None, true);
		TestRejectedAtomically(TEXT("pre-existing capacity above twelve"), Run, Candidate);
	}
	{
		FGameXXKCardRunState Run = MakeCapacityRun(0, 60);
		const FGameXXKRouteCardEntry InvalidCandidate = MakeRouteCardEntry(
			TEXT("Entry.NonCapacityAcquisition.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteReward, true, 0);
		TestRejectedAtomically(TEXT("acquisition candidate that does not consume capacity"), Run, InvalidCandidate);
	}
	{
		FGameXXKCardRunState Run = MakeCapacityRun(0, 60);
		const FGameXXKRouteCardEntry InvalidCandidate = MakeRouteCardEntry(
			TEXT("Entry.InvalidBaseCapacity.Candidate"), CandidateCardId, EGameXXKCardQuality::Common,
			EGameXXKRouteCardSourceKind::RouteBase, false, 0, NAME_None, true);
		TestRejectedAtomically(TEXT("base source consuming route capacity"), Run, InvalidCandidate);
	}

	return true;
}

#endif
