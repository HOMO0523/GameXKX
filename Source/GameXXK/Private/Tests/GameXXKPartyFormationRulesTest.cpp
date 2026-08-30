#include "GameXXKCompanionCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::Invalid) == 0, "Invalid formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::Hero) == 1, "Hero formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::PermanentCompanion) == 2, "Companion formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::QuestNpc) == 3, "NPC formation kind ordinal is serialized.");

namespace
{
	FGameXXKPartyMemberRef MakeMember(
		const EGameXXKPartyMemberKind Kind,
		const FName MemberId)
	{
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = Kind;
		Ref.MemberId = MemberId;
		return Ref;
	}

	bool BuildStartedState(FGameXXKRuntimeState& OutState)
	{
		UGameXXKMVPSubsystem* Subsystem =
			NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeStateCopy();
		return true;
	}

	bool FormationsMatch(
		const FGameXXKOrderedPartyFormation& Left,
		const FGameXXKOrderedPartyFormation& Right)
	{
		return FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
			&Left,
			&Right,
			PPF_None);
	}

	bool RuntimeStatesMatch(
		const FGameXXKRuntimeState& Left,
		const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Left,
			&Right,
			PPF_None);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationOrderValidationTest,
	"GameXXK.PartyFormation.Rules.OrderValidationAndLegacyProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationOrderValidationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("formation fixture starts"), BuildStartedState(State)))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("started formation validates"),
		FGameXXKPartyFormationRules::Validate(
			State,
			State.CardRun.OrderedFormation,
			&Error));
	TestEqual(TEXT("started formation has exactly three members"),
		State.CardRun.OrderedFormation.Members.Num(),
		FGameXXKPartyFormationRules::PartySize);

	FGameXXKOrderedPartyFormation Reordered = State.CardRun.OrderedFormation;
	Swap(Reordered.Members[0], Reordered.Members[2]);
	TestTrue(TEXT("formation order remains player-authored"),
		FGameXXKPartyFormationRules::Validate(State, Reordered, &Error));

	FGameXXKOrderedPartyFormation AnotherOwnedNpc = State.CardRun.OrderedFormation;
	FGameXXKPartyMemberRef* NpcRef = AnotherOwnedNpc.Members.FindByPredicate(
		[](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
		});
	if (!TestNotNull(TEXT("formation owns an NPC slot"), NpcRef))
	{
		return false;
	}
	NpcRef->MemberId = TEXT("Npc.YueBai");
	TestTrue(TEXT("every catalog NPC is an available formation member"),
		FGameXXKPartyFormationRules::Validate(State, AnotherOwnedNpc, &Error));

	auto TestRejected = [this, &State](
		const TCHAR* Label,
		const FGameXXKOrderedPartyFormation& Candidate)
	{
		FString Rejection;
		TestFalse(Label,
			FGameXXKPartyFormationRules::Validate(State, Candidate, &Rejection));
		TestFalse(*FString::Printf(TEXT("%s reports an error"), Label),
			Rejection.IsEmpty());
	};

	FGameXXKOrderedPartyFormation WrongCount = State.CardRun.OrderedFormation;
	WrongCount.Members.Pop(EAllowShrinking::No);
	TestRejected(TEXT("wrong member count is rejected"), WrongCount);
	FGameXXKOrderedPartyFormation Duplicate = State.CardRun.OrderedFormation;
	Duplicate.Members[2] = Duplicate.Members[1];
	TestRejected(TEXT("duplicate entity is rejected"), Duplicate);
	FGameXXKOrderedPartyFormation TwoCompanions = State.CardRun.OrderedFormation;
	const FName SecondCompanionId = State.CardRun.CompanionRoster.PermanentCompanions[1].InstanceId;
	TwoCompanions.Members[2] = MakeMember(
		EGameXXKPartyMemberKind::PermanentCompanion,
		SecondCompanionId);
	TestRejected(TEXT("formation must retain one fixed NPC"), TwoCompanions);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationFallbackRulesTest,
	"GameXXK.PartyFormation.Rules.FallbackResolutionAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationFallbackRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("normalization fixture starts"), BuildStartedState(State)))
	{
		return false;
	}
	FString Error;
	FGameXXKOrderedPartyFormation Reordered = State.CardRun.OrderedFormation;
	Swap(Reordered.Members[0], Reordered.Members[2]);
	State.CardRun.OrderedFormation = Reordered;
	const FGameXXKRuntimeState BeforeValidNormalize = State;
	TestTrue(TEXT("valid reordered formation normalizes"),
		FGameXXKPartyFormationRules::Normalize(State, &Error));
	TestTrue(TEXT("valid normalization preserves authored order and state"),
		RuntimeStatesMatch(State, BeforeValidNormalize));

	FGameXXKRuntimeState Invalid = State;
	Invalid.CardRun.OrderedFormation.Members[2] =
		Invalid.CardRun.OrderedFormation.Members[1];
	TestTrue(TEXT("invalid formation normalizes from compatibility selection"),
		FGameXXKPartyFormationRules::Normalize(Invalid, &Error));
	TestTrue(TEXT("normalized fallback contains one approved NPC"),
		!GameXXKPermanentPartyTestFixtures::ResolveNpc(Invalid).IsNone());
	TestTrue(TEXT("normalized fallback validates"),
		FGameXXKPartyFormationRules::Validate(
			Invalid,
			Invalid.CardRun.OrderedFormation,
			&Error));

	FGameXXKRuntimeState Empty = State;
	Empty.CardRun.OrderedFormation = FGameXXKOrderedPartyFormation();
	TestTrue(TEXT("empty legacy formation receives the current three-role projection"),
		FGameXXKPartyFormationRules::Normalize(Empty, &Error));
	TestEqual(TEXT("empty legacy projection restores the selected NPC"),
		GameXXKPermanentPartyTestFixtures::ResolveNpc(Empty),
		State.CardRun.PartySelection.QuestNpc.NpcId);

	FGameXXKRuntimeState Insufficient = UGameXXKMVPRules::CreateNewGame();
	const FGameXXKRuntimeState InsufficientBefore = Insufficient;
	TestFalse(TEXT("normalization fails without an owned permanent companion"),
		FGameXXKPartyFormationRules::Normalize(Insufficient, &Error));
	TestFalse(TEXT("failed sparse normalization reports an error"), Error.IsEmpty());
	TestTrue(TEXT("failed sparse normalization is atomic"),
		RuntimeStatesMatch(Insufficient, InsufficientBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationQuestNpcProvenanceTest,
	"GameXXK.PartyFormation.Rules.QuestNpcProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationQuestNpcProvenanceTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState Started;
	if (!TestTrue(TEXT("NPC authority fixture starts"), BuildStartedState(Started)))
	{
		return false;
	}
	for (const FGameXXKQuestNpcDefinition& Definition :
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		FGameXXKRuntimeState Candidate = Started;
		FString Error;
		TestTrue(*FString::Printf(TEXT("%s selects through ordered formation"),
			*Definition.NpcId.ToString()),
			GameXXKPermanentPartyTestFixtures::SelectNpc(
				Candidate,
				Definition.NpcId,
				&Error));
		TestEqual(TEXT("selected NPC resolves from ordered formation"),
			GameXXKPermanentPartyTestFixtures::ResolveNpc(Candidate),
			Definition.NpcId);
		TestTrue(TEXT("current fixture keeps the tombstone empty"),
			Candidate.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	}

	FGameXXKRuntimeState MismatchedSelection = Started;
	const FName OrderedNpcId =
		GameXXKPermanentPartyTestFixtures::ResolveNpc(MismatchedSelection);
	MismatchedSelection.CardRun.PartySelection.QuestNpc.NpcId = TEXT("Npc.YueBai");
	FString ProjectionError;
	TestFalse(TEXT("selection mirror cannot disagree with ordered NPC"),
		FGameXXKPartyFormationRules::ValidateCompatibilityProjection(
			MismatchedSelection,
			&ProjectionError));
	FGameXXKPartyFormationRules::ProjectCompatibility(MismatchedSelection);
	TestEqual(TEXT("projection restores ordered NPC identity"),
		MismatchedSelection.CardRun.PartySelection.QuestNpc.NpcId,
		OrderedNpcId);
	TestTrue(TEXT("projection never revives temporary provenance"),
		MismatchedSelection.CardRun.ActiveTemporaryQuestNpcId.IsNone());

	FGameXXKRuntimeState Unknown = Started;
	FString Error;
	TestFalse(TEXT("unknown NPC is rejected"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			Unknown,
			TEXT("Npc.Unknown"),
			&Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationCompatibilityProjectionTest,
	"GameXXK.PartyFormation.Rules.CompatibilityProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationCompatibilityProjectionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("projection fixture starts"), BuildStartedState(State)))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("projection fixture selects Yue Bai"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			State,
			TEXT("Npc.YueBai"),
			&Error));
	Swap(State.CardRun.OrderedFormation.Members[0],
		State.CardRun.OrderedFormation.Members[2]);
	const FGameXXKOrderedPartyFormation AuthoredOrder = State.CardRun.OrderedFormation;
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	TestTrue(TEXT("projection never reorders formation"),
		FormationsMatch(State.CardRun.OrderedFormation, AuthoredOrder));
	TestEqual(TEXT("projection selects Yue Bai from ordered formation"),
		State.CardRun.PartySelection.QuestNpc.NpcId,
		FName(TEXT("Npc.YueBai")));
	TestTrue(TEXT("projection keeps temporary provenance empty"),
		State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestTrue(TEXT("projected compatibility validates"),
		FGameXXKPartyFormationRules::ValidateCompatibilityProjection(
			State,
			&Error));

	// Intentional corruption fixture: a non-empty v29 tombstone is never a
	// valid current compatibility projection.
	FGameXXKRuntimeState CorruptLegacyTombstone = State;
	CorruptLegacyTombstone.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.TusiChief");
	TestFalse(TEXT("current validator rejects a revived tombstone"),
		FGameXXKPartyFormationRules::ValidateCompatibilityProjection(
			CorruptLegacyTombstone,
			&Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationSaveContractTest,
	"GameXXK.PartyFormation.Rules.SaveContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationSaveContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Invalid enum ordinal remains zero"),
		static_cast<uint8>(EGameXXKPartyMemberKind::Invalid),
		static_cast<uint8>(0));
	TestEqual(TEXT("Hero enum ordinal remains one"),
		static_cast<uint8>(EGameXXKPartyMemberKind::Hero),
		static_cast<uint8>(1));
	TestEqual(TEXT("PermanentCompanion enum ordinal remains two"),
		static_cast<uint8>(EGameXXKPartyMemberKind::PermanentCompanion),
		static_cast<uint8>(2));
	TestEqual(TEXT("QuestNpc enum ordinal remains three"),
		static_cast<uint8>(EGameXXKPartyMemberKind::QuestNpc),
		static_cast<uint8>(3));

	const FProperty* KindProperty =
		FGameXXKPartyMemberRef::StaticStruct()->FindPropertyByName(TEXT("Kind"));
	const FProperty* MemberIdProperty =
		FGameXXKPartyMemberRef::StaticStruct()->FindPropertyByName(TEXT("MemberId"));
	const FProperty* MembersProperty =
		FGameXXKOrderedPartyFormation::StaticStruct()->FindPropertyByName(TEXT("Members"));
	const FProperty* OrderedFormationProperty =
		FGameXXKCardRunState::StaticStruct()->FindPropertyByName(TEXT("OrderedFormation"));
	TestTrue(TEXT("member Kind is a SaveGame field"),
		KindProperty && KindProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("member ID is a SaveGame field"),
		MemberIdProperty && MemberIdProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("ordered Members are a SaveGame field"),
		MembersProperty && MembersProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("CardRun ordered formation is a SaveGame field"),
		OrderedFormationProperty && OrderedFormationProperty->HasAnyPropertyFlags(CPF_SaveGame));
	return true;
}

#endif
