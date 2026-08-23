#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::Invalid) == 0, "Invalid formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::Hero) == 1, "Hero formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::PermanentCompanion) == 2, "Companion formation kind ordinal is serialized.");
static_assert(static_cast<uint8>(EGameXXKPartyMemberKind::QuestNpc) == 3, "Quest NPC formation kind ordinal is serialized.");

namespace
{
	const FName BladeId(TEXT("Companion.Instance.01.Blade"));
	const FName GuardId(TEXT("Companion.Instance.02.Guard"));
	const FName HealerId(TEXT("Companion.Instance.03.Healer"));
	const FName HunterId(TEXT("Companion.Instance.04.Hunter"));
	const FName SorcererId(TEXT("Companion.Instance.05.Sorcerer"));
	const FName FormationMasterId(TEXT("Companion.Instance.06.FormationMaster"));
	const FName TusiChiefId(TEXT("Npc.TusiChief"));
	const FName YueBaiId(TEXT("Npc.YueBai"));

	FGameXXKPartyMemberRef MakeMember(const EGameXXKPartyMemberKind Kind, const FName MemberId)
	{
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = Kind;
		Ref.MemberId = MemberId;
		return Ref;
	}

	FGameXXKPermanentCompanion MakeCompanion(
		const FName InstanceId,
		const EGameXXKCharacterRole Role,
		const bool bActive)
	{
		FGameXXKPermanentCompanion Companion;
		Companion.InstanceId = InstanceId;
		Companion.RecruitTemplateId = FName(*FString::Printf(TEXT("Template.%s"), *InstanceId.ToString()));
		Companion.Role = Role;
		Companion.PersonalCardIds = {
			FName(*FString::Printf(TEXT("%s.Card.1"), *InstanceId.ToString())),
			FName(*FString::Printf(TEXT("%s.Card.2"), *InstanceId.ToString()))};
		Companion.SelectedCardIds = Companion.PersonalCardIds;
		Companion.bIsActive = bActive;
		return Companion;
	}

	TArray<FName> TusiCards()
	{
		return {
			TEXT("Npc.TusiChief.ZhaiZhuHaoLing"),
			TEXT("Npc.TusiChief.ShiMenShouShi"),
			TEXT("Npc.TusiChief.TuSiJunLing")};
	}

	TArray<FName> YueBaiCards()
	{
		return {
			TEXT("Npc.YueBai.QingYanDianDeng"),
			TEXT("Npc.YueBai.CanJuanPiZhu"),
			TEXT("Npc.YueBai.YueBaiZhaoYe")};
	}

	FGameXXKRuntimeState MakeFullFixture()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		// Deliberately keep roster order different from stable ID order.
		State.CardRun.CompanionRoster.PermanentCompanions = {
			MakeCompanion(GuardId, EGameXXKCharacterRole::Guard, false),
			MakeCompanion(BladeId, EGameXXKCharacterRole::Blade, true),
			MakeCompanion(HunterId, EGameXXKCharacterRole::Hunter, false),
			MakeCompanion(HealerId, EGameXXKCharacterRole::Healer, false),
			MakeCompanion(FormationMasterId, EGameXXKCharacterRole::FormationMaster, false),
			MakeCompanion(SorcererId, EGameXXKCharacterRole::Sorcerer, false)};
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId = BladeId;
		State.CardRun.ActiveTemporaryQuestNpcId = TusiChiefId;
		State.CardRun.PartySelection.QuestNpc.NpcId = TusiChiefId;
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds = TusiCards();
		State.CardRun.PartySelection.QuestNpcCardLoadouts.FindOrAdd(TusiChiefId).SelectedCardIds = TusiCards();
		State.CardRun.PartySelection.QuestNpcCardLoadouts.FindOrAdd(YueBaiId).SelectedCardIds = YueBaiCards();
		return State;
	}

	bool FormationsMatch(
		const FGameXXKOrderedPartyFormation& Left,
		const FGameXXKOrderedPartyFormation& Right)
	{
		return FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool RuntimeStatesMatch(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	FGameXXKOrderedPartyFormation ExpectedLegacyFormation()
	{
		FGameXXKOrderedPartyFormation Formation;
		Formation.Members = {
			MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()),
			MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, BladeId),
			MakeMember(EGameXXKPartyMemberKind::QuestNpc, TusiChiefId)};
		return Formation;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationOrderValidationTest,
	"GameXXK.PartyFormation.Rules.OrderValidationAndLegacyProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationOrderValidationTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState State = MakeFullFixture();
	const FGameXXKRuntimeState StateBefore = State;
	FGameXXKOrderedPartyFormation Legacy;
	TestTrue(TEXT("complete new-game fixture projects its legacy party"),
		FGameXXKPartyFormationRules::BuildLegacyProjection(State, Legacy));
	if (!TestEqual(TEXT("legacy projection has exactly three slots"), Legacy.Members.Num(), 3))
	{
		return false;
	}
	TestTrue(TEXT("hero projects into 1P"), Legacy.Members[0] ==
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()));
	TestTrue(TEXT("active permanent companion projects into 2P"), Legacy.Members[1] ==
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, BladeId));
	TestTrue(TEXT("current task NPC projects into 3P"), Legacy.Members[2] ==
		MakeMember(EGameXXKPartyMemberKind::QuestNpc, TusiChiefId));

	FString Error;
	TestTrue(TEXT("legacy projection validates"), FGameXXKPartyFormationRules::Validate(State, Legacy, &Error));
	FGameXXKOrderedPartyFormation HeroAt2P = Legacy;
	Swap(HeroAt2P.Members[0], HeroAt2P.Members[1]);
	TestTrue(TEXT("hero may occupy 2P"), FGameXXKPartyFormationRules::Validate(State, HeroAt2P, &Error));
	FGameXXKOrderedPartyFormation HeroAt3P = Legacy;
	Swap(HeroAt3P.Members[0], HeroAt3P.Members[2]);
	TestTrue(TEXT("hero may occupy 3P"), FGameXXKPartyFormationRules::Validate(State, HeroAt3P, &Error));

	FGameXXKOrderedPartyFormation InactiveOwned;
	InactiveOwned.Members = {
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()),
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, GuardId),
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, HealerId)};
	TestTrue(TEXT("owned inactive permanent companions are legal candidates"),
		FGameXXKPartyFormationRules::Validate(State, InactiveOwned, &Error));

	auto TestRejected = [this, &State](
		const TCHAR* Label,
		const FGameXXKOrderedPartyFormation& Candidate,
		const TCHAR* ExpectedErrorFragment)
	{
		FString Rejection;
		TestFalse(Label, FGameXXKPartyFormationRules::Validate(State, Candidate, &Rejection));
		TestFalse(*FString::Printf(TEXT("%s reports an explicit error"), Label), Rejection.IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s reports the expected reason"), Label),
			Rejection.Contains(ExpectedErrorFragment, ESearchCase::IgnoreCase));
	};

	FGameXXKOrderedPartyFormation WrongCount = Legacy;
	WrongCount.Members.Pop(EAllowShrinking::No);
	TestRejected(TEXT("wrong member count is rejected"), WrongCount, TEXT("exactly three"));

	FGameXXKOrderedPartyFormation Duplicate = Legacy;
	Duplicate.Members[2] = Duplicate.Members[1];
	TestRejected(TEXT("duplicate entity is rejected"), Duplicate, TEXT("duplicate"));

	FGameXXKOrderedPartyFormation NoHero;
	NoHero.Members = {
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, BladeId),
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, GuardId),
		MakeMember(EGameXXKPartyMemberKind::QuestNpc, TusiChiefId)};
	TestRejected(TEXT("formation without a hero is rejected"), NoHero, TEXT("hero"));

	FGameXXKOrderedPartyFormation UnknownCompanion = Legacy;
	UnknownCompanion.Members[1].MemberId = TEXT("Companion.Instance.Unknown");
	TestRejected(TEXT("unknown companion is rejected"), UnknownCompanion, TEXT("unknown or unavailable"));

	FGameXXKOrderedPartyFormation UnavailableNpc = Legacy;
	UnavailableNpc.Members[2].MemberId = YueBaiId;
	TestRejected(TEXT("catalog NPC that is not current is rejected"), UnavailableNpc, TEXT("unknown or unavailable"));

	FGameXXKOrderedPartyFormation WrongKind = Legacy;
	WrongKind.Members[1].Kind = EGameXXKPartyMemberKind::QuestNpc;
	TestRejected(TEXT("member ID under the wrong kind is rejected"), WrongKind, TEXT("unknown or unavailable"));

	FGameXXKOrderedPartyFormation WrongHeroId = Legacy;
	WrongHeroId.Members[0].MemberId = TEXT("Hero.Future.Unknown");
	TestRejected(TEXT("unknown hero ID is rejected"), WrongHeroId, TEXT("unknown or unavailable"));

	FGameXXKOrderedPartyFormation InvalidRef = Legacy;
	InvalidRef.Members[1] = FGameXXKPartyMemberRef{};
	TestRejected(TEXT("invalid reference is rejected"), InvalidRef, TEXT("invalid"));
	TestTrue(TEXT("validation never mutates authoritative state"), RuntimeStatesMatch(State, StateBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationFallbackRulesTest,
	"GameXXK.PartyFormation.Rules.FallbackResolutionAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationFallbackRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState MissingOptional = MakeFullFixture();
	MissingOptional.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
	for (FGameXXKPermanentCompanion& Companion : MissingOptional.CardRun.CompanionRoster.PermanentCompanions)
	{
		Companion.bIsActive = false;
	}
	MissingOptional.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
	MissingOptional.CardRun.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection{};

	FGameXXKOrderedPartyFormation StableFallback;
	TestTrue(TEXT("missing optional legacy slots use owned legal replacements"),
		FGameXXKPartyFormationRules::BuildLegacyProjection(MissingOptional, StableFallback));
	if (!TestEqual(TEXT("replacement projection remains exactly three"), StableFallback.Members.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("stable fallback keeps the hero first"), StableFallback.Members[0].MemberId,
		FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("stable fallback chooses lowest companion ID first"), StableFallback.Members[1].MemberId, BladeId);
	TestEqual(TEXT("stable fallback chooses next companion ID without duplication"), StableFallback.Members[2].MemberId, GuardId);
	FGameXXKOrderedPartyFormation RepeatedFallback;
	TestTrue(TEXT("stable fallback can be repeated"),
		FGameXXKPartyFormationRules::BuildLegacyProjection(MissingOptional, RepeatedFallback));
	TestTrue(TEXT("stable fallback is deterministic"), FormationsMatch(StableFallback, RepeatedFallback));

	FGameXXKRuntimeState SavedValid = MakeFullFixture();
	FGameXXKOrderedPartyFormation SavedOrder = ExpectedLegacyFormation();
	Swap(SavedOrder.Members[0], SavedOrder.Members[2]);
	SavedValid.CardRun.OrderedFormation = SavedOrder;
	FGameXXKOrderedPartyFormation Effective;
	FString Error;
	TestTrue(TEXT("valid saved formation resolves"),
		FGameXXKPartyFormationRules::ResolveEffective(SavedValid, Effective, &Error));
	TestTrue(TEXT("valid saved formation order is returned unchanged"), FormationsMatch(Effective, SavedOrder));

	FGameXXKRuntimeState SavedInvalid = MakeFullFixture();
	SavedInvalid.CardRun.OrderedFormation = ExpectedLegacyFormation();
	SavedInvalid.CardRun.OrderedFormation.Members[2] = SavedInvalid.CardRun.OrderedFormation.Members[1];
	TestTrue(TEXT("invalid saved formation resolves through approved legacy projection"),
		FGameXXKPartyFormationRules::ResolveEffective(SavedInvalid, Effective, &Error));
	TestTrue(TEXT("invalid saved formation resolves deterministically"),
		FormationsMatch(Effective, ExpectedLegacyFormation()));

	FGameXXKRuntimeState SavedEmpty = MakeFullFixture();
	TestTrue(TEXT("empty saved formation resolves through approved legacy projection"),
		FGameXXKPartyFormationRules::ResolveEffective(SavedEmpty, Effective, &Error));
	TestTrue(TEXT("empty saved formation receives the legacy order"),
		FormationsMatch(Effective, ExpectedLegacyFormation()));

	const FGameXXKRuntimeState ValidBeforeNormalize = SavedValid;
	TestTrue(TEXT("valid saved formation normalizes successfully"),
		FGameXXKPartyFormationRules::Normalize(SavedValid, &Error));
	TestTrue(TEXT("normalizing valid saved formation preserves every byte and order"),
		RuntimeStatesMatch(SavedValid, ValidBeforeNormalize));

	TestTrue(TEXT("invalid saved formation normalizes successfully"),
		FGameXXKPartyFormationRules::Normalize(SavedInvalid, &Error));
	TestTrue(TEXT("normalization replaces invalid order with legacy projection"),
		FormationsMatch(SavedInvalid.CardRun.OrderedFormation, ExpectedLegacyFormation()));
	TestTrue(TEXT("empty saved formation normalizes successfully"),
		FGameXXKPartyFormationRules::Normalize(SavedEmpty, &Error));
	TestTrue(TEXT("normalization fills an empty saved formation"),
		FormationsMatch(SavedEmpty.CardRun.OrderedFormation, ExpectedLegacyFormation()));

	FGameXXKRuntimeState Insufficient = UGameXXKMVPRules::CreateNewGame();
	Insufficient.CardRun.CompanionRoster.PermanentCompanions.Add(
		MakeCompanion(BladeId, EGameXXKCharacterRole::Blade, true));
	Insufficient.CardRun.PartySelection.ActivePermanentCompanionInstanceId = BladeId;
	FGameXXKOrderedPartyFormation Sentinel;
	Sentinel.Members = {MakeMember(EGameXXKPartyMemberKind::Hero, TEXT("Sentinel.Hero"))};
	const FGameXXKOrderedPartyFormation SentinelBefore = Sentinel;
	TestFalse(TEXT("legacy projection fails rather than fabricating a third member"),
		FGameXXKPartyFormationRules::BuildLegacyProjection(Insufficient, Sentinel));
	TestTrue(TEXT("failed legacy projection leaves output untouched"), FormationsMatch(Sentinel, SentinelBefore));
	Error.Reset();
	TestFalse(TEXT("effective resolution explicitly fails with fewer than three legal entities"),
		FGameXXKPartyFormationRules::ResolveEffective(Insufficient, Sentinel, &Error));
	TestFalse(TEXT("failed effective resolution reports an error"), Error.IsEmpty());
	TestTrue(TEXT("failed effective resolution leaves output untouched"), FormationsMatch(Sentinel, SentinelBefore));

	Insufficient.CardRun.OrderedFormation = SentinelBefore;
	const FGameXXKRuntimeState InsufficientBefore = Insufficient;
	Error.Reset();
	TestFalse(TEXT("normalization fails when no legal three-member projection exists"),
		FGameXXKPartyFormationRules::Normalize(Insufficient, &Error));
	TestFalse(TEXT("failed normalization reports an error"), Error.IsEmpty());
	TestTrue(TEXT("failed normalization is candidate-copy atomic"),
		RuntimeStatesMatch(Insufficient, InsufficientBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationCompatibilityProjectionTest,
	"GameXXK.PartyFormation.Rules.CompatibilityProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationCompatibilityProjectionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeFullFixture();
	State.CardRun.ActiveTemporaryQuestNpcId = YueBaiId;
	const TArray<FName> OriginalTusiCards = State.CardRun.PartySelection.QuestNpc.SelectedCardIds;
	const TArray<FName> ExpectedYueBaiCards = YueBaiCards();
	const int32 OriginalRosterCount = State.CardRun.CompanionRoster.PermanentCompanions.Num();
	TArray<FName> OriginalRosterIds;
	TMap<FName, TArray<FName>> OriginalCompanionCards;
	for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
	{
		OriginalRosterIds.Add(Companion.InstanceId);
		OriginalCompanionCards.Add(Companion.InstanceId, Companion.SelectedCardIds);
	}
	const FGameXXKCompanionPartySelection OriginalSelection = State.CardRun.PartySelection;

	State.CardRun.OrderedFormation.Members = {
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, GuardId),
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()),
		MakeMember(EGameXXKPartyMemberKind::QuestNpc, YueBaiId)};
	const FGameXXKOrderedPartyFormation OriginalOrder = State.CardRun.OrderedFormation;
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	TestTrue(TEXT("compatibility projection never reorders ordered formation"),
		FormationsMatch(State.CardRun.OrderedFormation, OriginalOrder));
	TestEqual(TEXT("first permanent companion in formation becomes legacy active companion"),
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId, GuardId);
	TestEqual(TEXT("first NPC in formation becomes legacy active temporary NPC"),
		State.CardRun.ActiveTemporaryQuestNpcId, YueBaiId);
	TestEqual(TEXT("first NPC in formation becomes legacy party selection"),
		State.CardRun.PartySelection.QuestNpc.NpcId, YueBaiId);
	TestEqual(TEXT("compatibility projection restores the selected cards belonging to that NPC"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds, ExpectedYueBaiCards);
	TestEqual(TEXT("projection retains old NPC owned-card data"),
		State.CardRun.PartySelection.QuestNpcCardLoadouts.FindChecked(TusiChiefId).SelectedCardIds,
		OriginalTusiCards);
	TestEqual(TEXT("projection does not add or remove owned companions"),
		State.CardRun.CompanionRoster.PermanentCompanions.Num(), OriginalRosterCount);

	int32 ActiveCount = 0;
	for (int32 CompanionIndex = 0; CompanionIndex < State.CardRun.CompanionRoster.PermanentCompanions.Num(); ++CompanionIndex)
	{
		const FGameXXKPermanentCompanion& Companion = State.CardRun.CompanionRoster.PermanentCompanions[CompanionIndex];
		TestEqual(TEXT("projection preserves companion roster order and identity"),
			Companion.InstanceId, OriginalRosterIds[CompanionIndex]);
		TestEqual(TEXT("projection preserves companion selected-card ownership"),
			Companion.SelectedCardIds, OriginalCompanionCards.FindChecked(Companion.InstanceId));
		if (Companion.bIsActive)
		{
			++ActiveCount;
			TestEqual(TEXT("only the first deployed companion is legacy active"), Companion.InstanceId, GuardId);
		}
	}
	TestEqual(TEXT("legacy projection exposes only one active permanent companion"), ActiveCount, 1);
	TestEqual(TEXT("owned NPC loadout count is not polluted"),
		State.CardRun.PartySelection.QuestNpcCardLoadouts.Num(),
		OriginalSelection.QuestNpcCardLoadouts.Num());

	State.CardRun.OrderedFormation.Members = {
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()),
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, GuardId),
		MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, BladeId)};
	const FGameXXKOrderedPartyFormation TwoCompanionOrder = State.CardRun.OrderedFormation;
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	TestTrue(TEXT("two-companion compatibility projection keeps authoritative order"),
		FormationsMatch(State.CardRun.OrderedFormation, TwoCompanionOrder));
	TestEqual(TEXT("two-companion formation projects only its first companion"),
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId, GuardId);
	TestTrue(TEXT("formation without NPC clears old active temporary NPC projection"),
		State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestTrue(TEXT("formation without NPC clears old active NPC selection ID"),
		State.CardRun.PartySelection.QuestNpc.NpcId.IsNone());
	TestTrue(TEXT("formation without NPC clears transient selected NPC cards"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.IsEmpty());
	TestEqual(TEXT("clearing active NPC keeps owned NPC card loadouts"),
		State.CardRun.PartySelection.QuestNpcCardLoadouts.Num(),
		OriginalSelection.QuestNpcCardLoadouts.Num());

	State.CardRun.OrderedFormation.Members = {
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId())};
	const FGameXXKOrderedPartyFormation HeroOnlyOrder = State.CardRun.OrderedFormation;
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	TestTrue(TEXT("missing-kind cleanup does not rewrite even an invalid authoritative order"),
		FormationsMatch(State.CardRun.OrderedFormation, HeroOnlyOrder));
	TestTrue(TEXT("formation without companion kind clears legacy companion pointer"),
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId.IsNone());
	for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
	{
		TestFalse(TEXT("formation without companion kind clears every legacy active flag"), Companion.bIsActive);
	}
	TestEqual(TEXT("missing-kind cleanup still does not remove companions"),
		State.CardRun.CompanionRoster.PermanentCompanions.Num(), OriginalRosterCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationSaveContractTest,
	"GameXXK.PartyFormation.Rules.SaveContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationSaveContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Invalid enum ordinal remains zero"), static_cast<uint8>(EGameXXKPartyMemberKind::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("Hero enum ordinal remains one"), static_cast<uint8>(EGameXXKPartyMemberKind::Hero), static_cast<uint8>(1));
	TestEqual(TEXT("PermanentCompanion enum ordinal remains two"), static_cast<uint8>(EGameXXKPartyMemberKind::PermanentCompanion), static_cast<uint8>(2));
	TestEqual(TEXT("QuestNpc enum ordinal remains three"), static_cast<uint8>(EGameXXKPartyMemberKind::QuestNpc), static_cast<uint8>(3));

	const FProperty* KindProperty = FGameXXKPartyMemberRef::StaticStruct()->FindPropertyByName(TEXT("Kind"));
	const FProperty* MemberIdProperty = FGameXXKPartyMemberRef::StaticStruct()->FindPropertyByName(TEXT("MemberId"));
	const FProperty* MembersProperty = FGameXXKOrderedPartyFormation::StaticStruct()->FindPropertyByName(TEXT("Members"));
	const FProperty* OrderedFormationProperty = FGameXXKCardRunState::StaticStruct()->FindPropertyByName(TEXT("OrderedFormation"));
	TestTrue(TEXT("member Kind is a SaveGame field"), KindProperty && KindProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("member ID is a SaveGame field"), MemberIdProperty && MemberIdProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("ordered Members are a SaveGame field"), MembersProperty && MembersProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("CardRun ordered formation is a SaveGame field"),
		OrderedFormationProperty && OrderedFormationProperty->HasAnyPropertyFlags(CPF_SaveGame));

	const FGameXXKPartyMemberRef Invalid;
	const FGameXXKPartyMemberRef Hero =
		MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId());
	TestFalse(TEXT("default member reference is invalid"), Invalid.IsValid());
	TestTrue(TEXT("typed non-empty member reference is structurally valid"), Hero.IsValid());
	TestTrue(TEXT("member reference equality compares stable kind and ID"), Hero == Hero);
	TestFalse(TEXT("member reference equality distinguishes IDs"),
		Hero == MakeMember(EGameXXKPartyMemberKind::Hero, TEXT("Hero.Other")));
	return true;
}

#endif
