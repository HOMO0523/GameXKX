#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKPartyFormationRules.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<FName> FirstCards(const TArray<FName>& SourceCards, const int32 Count)
	{
		TArray<FName> Result;
		if (SourceCards.Num() >= Count)
		{
			Result.Append(SourceCards.GetData(), Count);
		}
		return Result;
	}

	bool RecruitOneCompanion(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* Subsystem,
		const int32 Seed,
		FGameXXKPermanentCompanion& OutCompanion)
	{
		OutCompanion = FGameXXKPermanentCompanion();
		if (!Test.TestTrue(TEXT("a seeded recruitment fixture enters town before claiming its ticket"),
			Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap()))
		{
			return false;
		}
		FGameXXKCompanionRecruitResult Result;
		if (!Test.TestTrue(TEXT("a seeded facade recruit succeeds"), Subsystem && Subsystem->RecruitPermanentCompanionFromSeed(Seed, Result)))
		{
			return false;
		}
		if (!Test.TestEqual(TEXT("an empty facade roster receives a permanent recruit"), Result.Outcome, EGameXXKCompanionRecruitOutcome::Recruited))
		{
			return false;
		}
		FString FormationError;
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		if (!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &FormationError)
			|| !FGameXXKPartyFormationRules::Normalize(State, &FormationError))
		{
			Test.AddError(FormationError);
			return false;
		}
		FGameXXKPartyFormationRules::ProjectCompatibility(State);
		OutCompanion = Result.Companion;
		return !OutCompanion.InstanceId.IsNone();
	}

	bool AssertConfigurationRejectedOutsideTown(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* Subsystem,
		const EGameXXKScreen Screen,
		const TCHAR* ScreenName,
		const FName CompanionInstanceId,
		const TArray<FName>& CompanionEditAttempt,
		const TArray<FName>& ExpectedCompanionCards,
		const TArray<FName>& HeroEditAttempt,
		const TArray<FName>& ExpectedHeroCards,
		const FName QuestNpcId,
		const TArray<FName>& QuestNpcEditAttempt,
		const FGameXXKQuestNpcCardSelection& ExpectedQuestNpcSelection,
		const FName ExpectedActiveCompanionId)
	{
		if (!Subsystem)
		{
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State.Screen = Screen;
		State.CardRun.bLoadoutLockedForRoute = false;
		State.CardRun.bHasActiveCardBattle = false;
		State.CardRun.PartySelection.ActivePermanentCompanionInstanceId = ExpectedActiveCompanionId;
		State.CardRun.HeroSelectedCardIds = ExpectedHeroCards;
		State.CardRun.PartySelection.QuestNpc = ExpectedQuestNpcSelection;
		State.CardRun.ActiveTemporaryQuestNpcId = ExpectedQuestNpcSelection.NpcId;
		if (FGameXXKPermanentCompanion* StoredCompanion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[CompanionInstanceId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == CompanionInstanceId;
			}))
		{
			StoredCompanion->SelectedCardIds = ExpectedCompanionCards;
		}

		bool bPassed = true;
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s rejects clearing the active permanent companion"), ScreenName),
			Subsystem->SetActivePermanentCompanion(NAME_None));
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s rejects permanent companion card edits"), ScreenName),
			Subsystem->SetPermanentCompanionCardLoadout(CompanionInstanceId, CompanionEditAttempt));
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s rejects hero card edits"), ScreenName),
			Subsystem->SetHeroCardLoadout(HeroEditAttempt));
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s rejects task NPC card edits"), ScreenName),
			Subsystem->SetTemporaryQuestNpcCardLoadout(QuestNpcId, QuestNpcEditAttempt));

		FGameXXKPermanentCompanion StoredCompanion;
		bPassed &= Test.TestTrue(
			FString::Printf(TEXT("%s retains the configured permanent companion"), ScreenName),
			Subsystem->TryGetPermanentCompanionView(CompanionInstanceId, StoredCompanion));
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the permanent companion card loadout unchanged"), ScreenName),
			StoredCompanion.SelectedCardIds,
			ExpectedCompanionCards);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the hero card loadout unchanged"), ScreenName),
			Subsystem->GetHeroCardLoadout(),
			ExpectedHeroCards);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the active permanent companion unchanged"), ScreenName),
			Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,
			ExpectedActiveCompanionId);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the task NPC identity unchanged"), ScreenName),
			Subsystem->GetQuestNpcCardLoadout().NpcId,
			ExpectedQuestNpcSelection.NpcId);
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the task NPC fixed cards unchanged"), ScreenName),
			Subsystem->GetQuestNpcCardLoadout().SelectedCardIds,
			ExpectedQuestNpcSelection.SelectedCardIds);
		return bPassed;
	}

	TArray<uint8> SerializeFacadeRuntimeState(const FGameXXKRuntimeState& Source)
	{
		FGameXXKRuntimeState Copy = Source;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		return Bytes;
	}

	bool AssertFormationRejectedWithoutMutation(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* Subsystem,
		const FGameXXKOrderedPartyFormation& Candidate,
		const FString& Label)
	{
		if (!Subsystem)
		{
			return false;
		}
		const TArray<uint8> Before = SerializeFacadeRuntimeState(Subsystem->GetRuntimeState());
		FString Error;
		bool bPassed = true;
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s is rejected"), *Label),
			Subsystem->SetOrderedPartyFormation(Candidate, Error));
		bPassed &= Test.TestFalse(
			FString::Printf(TEXT("%s exposes a player-visible error"), *Label),
			Error.IsEmpty());
		bPassed &= Test.TestEqual(
			FString::Printf(TEXT("%s leaves the entire runtime state bit-identical"), *Label),
			SerializeFacadeRuntimeState(Subsystem->GetRuntimeState()),
			Before);
		return bPassed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionFacadeRecruitReadTest,
	"GameXXK.MVP.Companion.Facade.RecruitAndRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionFacadeRecruitReadTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* FirstSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKMVPSubsystem* SecondSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("first facade subsystem exists"), FirstSubsystem);
	TestNotNull(TEXT("second facade subsystem exists"), SecondSubsystem);
	if (!FirstSubsystem || !SecondSubsystem)
	{
		return false;
	}

	TestEqual(TEXT("the facade exposes the twelve-slot roster capacity"),
		FirstSubsystem->GetPermanentCompanionRosterCapacity(), FGameXXKCompanionRules::MaxPermanentCompanions);
	TestEqual(TEXT("a new facade roster is empty"), FirstSubsystem->GetPermanentCompanionViews().Num(), 0);
	FGameXXKCompanionRecruitResult OutsideTownResult;
	TestFalse(TEXT("the public seeded recruitment seam cannot be claimed outside town"),
		FirstSubsystem->RecruitPermanentCompanionFromSeed(47291, OutsideTownResult));
	TestEqual(TEXT("a rejected outside-town seed does not mutate the roster"), FirstSubsystem->GetPermanentCompanionViews().Num(), 0);

	FGameXXKPermanentCompanion FirstRecruit;
	FGameXXKPermanentCompanion SecondRecruit;
	if (!RecruitOneCompanion(*this, FirstSubsystem, 47291, FirstRecruit)
		|| !RecruitOneCompanion(*this, SecondSubsystem, 47291, SecondRecruit))
	{
		return false;
	}

	TestEqual(TEXT("the same recruit seed resolves the same template"), FirstRecruit.RecruitTemplateId, SecondRecruit.RecruitTemplateId);
	TestEqual(TEXT("the same recruit seed resolves the same personal card seed"), FirstRecruit.CardSeed, SecondRecruit.CardSeed);
	TestEqual(TEXT("the same recruit seed resolves the same twelve-card personal pool"), FirstRecruit.PersonalCardIds, SecondRecruit.PersonalCardIds);
	TestEqual(TEXT("the facade roster lists the recruited companion"), FirstSubsystem->GetPermanentCompanionViews().Num(), 1);

	FGameXXKPermanentCompanion ReadView;
	TestTrue(TEXT("the facade exposes a copy-safe read view by stable instance id"), FirstSubsystem->TryGetPermanentCompanionView(FirstRecruit.InstanceId, ReadView));
	TestEqual(TEXT("the read view preserves the stable recruit identity"), ReadView.InstanceId, FirstRecruit.InstanceId);
	ReadView.Level = 19;
	FGameXXKPermanentCompanion ReReadView;
	TestTrue(TEXT("the facade can reread the persisted companion"), FirstSubsystem->TryGetPermanentCompanionView(FirstRecruit.InstanceId, ReReadView));
	TestEqual(TEXT("mutating a facade read view cannot mutate the saved roster"), ReReadView.Level, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionFacadeLoadoutProgressionTest,
	"GameXXK.MVP.Companion.Facade.LoadoutsAndProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionFacadeLoadoutProgressionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("facade subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	FGameXXKPermanentCompanion Recruit;
	if (!RecruitOneCompanion(*this, Subsystem, 8831, Recruit))
	{
		return false;
	}
	FGameXXKPermanentCompanion Companion;
	TestTrue(TEXT("the recruited companion is readable before configuration"), Subsystem->TryGetPermanentCompanionView(Recruit.InstanceId, Companion));
	if (Companion.UnlockedPersonalCardIds.Num() < 6)
	{
		AddError(TEXT("the recruited companion needs at least six unlocked cards for loadout coverage"));
		return false;
	}

	TArray<FName> CompanionLoadout = FirstCards(Companion.UnlockedPersonalCardIds, 5);
	Swap(CompanionLoadout[0], CompanionLoadout[4]);
	TestTrue(TEXT("the facade persists exactly five unlocked companion cards"), Subsystem->SetPermanentCompanionCardLoadout(Recruit.InstanceId, CompanionLoadout));
	TestFalse(TEXT("the facade rejects a companion loadout with fewer than five cards"), Subsystem->SetPermanentCompanionCardLoadout(Recruit.InstanceId, FirstCards(Companion.UnlockedPersonalCardIds, 4)));

	TestTrue(TEXT("the facade selects the one active permanent partner"), Subsystem->SetActivePermanentCompanion(Recruit.InstanceId));
	TestEqual(TEXT("the route party projection follows the selected stable companion id"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId, Recruit.InstanceId);

	TArray<FName> HeroLoadout = Subsystem->GetHeroCardLoadout();
	TestEqual(TEXT("the facade initializes an eight-card hero loadout"), HeroLoadout.Num(), 8);
	if (HeroLoadout.Num() != 8)
	{
		return false;
	}
	Swap(HeroLoadout[0], HeroLoadout[7]);
	TestTrue(TEXT("the facade persists exactly eight hero cards"), Subsystem->SetHeroCardLoadout(HeroLoadout));
	TestFalse(TEXT("the facade rejects a hero loadout with seven cards"), Subsystem->SetHeroCardLoadout(FirstCards(HeroLoadout, 7)));

	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the fixed task NPC catalog exposes the Tusi chief"), TusiChief);
	if (!TusiChief)
	{
		return false;
	}
	TestTrue(TEXT("the route-owned adapter attaches the NPC's canonical fixed three-card loadout"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Subsystem->GetMutableRuntimeState(), TusiChief->NpcId, {}));
	const FGameXXKQuestNpcCardSelection NpcLoadout = Subsystem->GetQuestNpcCardLoadout();
	TestEqual(TEXT("the NPC read view exposes the route-selected three fixed cards"), NpcLoadout.SelectedCardIds.Num(), 3);
	const TArray<FName> EditedNpcLoadout = FirstCards(TusiChief->FixedCardIds, 3);
	TestTrue(TEXT("the facade persists exactly three cards for an owned NPC"),
		Subsystem->SetTemporaryQuestNpcCardLoadout(TusiChief->NpcId, EditedNpcLoadout));
	TestEqual(TEXT("the active NPC read view reflects the persisted edit"),
		Subsystem->GetQuestNpcCardLoadout().SelectedCardIds, EditedNpcLoadout);

	Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.SigilCount = 1;
	TestTrue(TEXT("the facade awards persistent companion experience through the canonical progression rule"), Subsystem->AwardPermanentCompanionExperience(Recruit.InstanceId, 40));
	TestTrue(TEXT("the facade promotes a companion star through the canonical sigil rule"), Subsystem->PromotePermanentCompanionStar(Recruit.InstanceId));
	FGameXXKPermanentCompanion Progressed;
	TestTrue(TEXT("the progressed companion remains readable"), Subsystem->TryGetPermanentCompanionView(Recruit.InstanceId, Progressed));
	TestEqual(TEXT("forty experience advances the companion one level"), Progressed.Level, 2);
	TestEqual(TEXT("one sigil advances the companion one star"), Progressed.Star, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionFacadeTownOnlyConfigurationTest,
	"GameXXK.MVP.Companion.Facade.TownOnlyConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionFacadeTownOnlyConfigurationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("town-only configuration facade subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	FGameXXKPermanentCompanion Recruit;
	if (!RecruitOneCompanion(*this, Subsystem, 94731, Recruit))
	{
		return false;
	}
	FGameXXKPermanentCompanion Companion;
	if (!this->TestTrue(TEXT("town-only configuration fixture can read its recruit"),
		Subsystem->TryGetPermanentCompanionView(Recruit.InstanceId, Companion))
		|| Companion.UnlockedPersonalCardIds.Num() < 6)
	{
		return false;
	}

	const TArray<FName> CompanionEditAttempt = FirstCards(Companion.UnlockedPersonalCardIds, 5);
	TArray<FName> ConfiguredCompanionCards = CompanionEditAttempt;
	Swap(ConfiguredCompanionCards[0], ConfiguredCompanionCards[4]);
	TestTrue(TEXT("town permits one permanent companion loadout edit"),
		Subsystem->SetPermanentCompanionCardLoadout(Recruit.InstanceId, ConfiguredCompanionCards));
	TestTrue(TEXT("town permits selecting one active permanent companion"),
		Subsystem->SetActivePermanentCompanion(Recruit.InstanceId));

	const TArray<FName> HeroEditAttempt = Subsystem->GetHeroCardLoadout();
	if (!this->TestEqual(TEXT("the town fixture begins with eight editable hero cards"), HeroEditAttempt.Num(), 8))
	{
		return false;
	}
	TArray<FName> ConfiguredHeroCards = HeroEditAttempt;
	Swap(ConfiguredHeroCards[0], ConfiguredHeroCards[7]);
	TestTrue(TEXT("town permits one hero card loadout edit"), Subsystem->SetHeroCardLoadout(ConfiguredHeroCards));

	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the town-only fixture finds the task NPC definition"), TusiChief);
	if (!TusiChief || TusiChief->FixedCardIds.Num() != 4)
	{
		return false;
	}
	TestTrue(TEXT("the route-owned adapter creates a fixed task NPC card selection"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Subsystem->GetMutableRuntimeState(), TusiChief->NpcId, {}));
	TArray<FName> QuestNpcEditAttempt = FirstCards(TusiChief->FixedCardIds, 3);
	QuestNpcEditAttempt[2] = TusiChief->FixedCardIds[3];
	TestTrue(TEXT("town permits an owned NPC three-card persistence edit"),
		Subsystem->SetTemporaryQuestNpcCardLoadout(TusiChief->NpcId, QuestNpcEditAttempt));
	TestEqual(TEXT("town applies the edited NPC selection"),
		Subsystem->GetQuestNpcCardLoadout().SelectedCardIds, QuestNpcEditAttempt);
	const FGameXXKQuestNpcCardSelection EditedQuestNpcSelection = Subsystem->GetQuestNpcCardLoadout();

	const FName ExpectedActiveCompanionId = Recruit.InstanceId;
	const bool bMainMenuRejected = AssertConfigurationRejectedOutsideTown(
		*this, Subsystem, EGameXXKScreen::MainMenu, TEXT("main menu"), Recruit.InstanceId,
		CompanionEditAttempt, ConfiguredCompanionCards, HeroEditAttempt, ConfiguredHeroCards,
		TusiChief->NpcId, QuestNpcEditAttempt, EditedQuestNpcSelection, ExpectedActiveCompanionId);
	const bool bWorldMapRejected = AssertConfigurationRejectedOutsideTown(
		*this, Subsystem, EGameXXKScreen::WorldMap, TEXT("world map"), Recruit.InstanceId,
		CompanionEditAttempt, ConfiguredCompanionCards, HeroEditAttempt, ConfiguredHeroCards,
		TusiChief->NpcId, QuestNpcEditAttempt, EditedQuestNpcSelection, ExpectedActiveCompanionId);
	const bool bRouteRejected = AssertConfigurationRejectedOutsideTown(
		*this, Subsystem, EGameXXKScreen::DungeonMap, TEXT("route map"), Recruit.InstanceId,
		CompanionEditAttempt, ConfiguredCompanionCards, HeroEditAttempt, ConfiguredHeroCards,
		TusiChief->NpcId, QuestNpcEditAttempt, EditedQuestNpcSelection, ExpectedActiveCompanionId);
	const bool bBattleRejected = AssertConfigurationRejectedOutsideTown(
		*this, Subsystem, EGameXXKScreen::Battle, TEXT("battle"), Recruit.InstanceId,
		CompanionEditAttempt, ConfiguredCompanionCards, HeroEditAttempt, ConfiguredHeroCards,
		TusiChief->NpcId, QuestNpcEditAttempt, EditedQuestNpcSelection, ExpectedActiveCompanionId);
	return bMainMenuRejected && bWorldMapRejected && bRouteRejected && bBattleRejected;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionFacadeRouteLockTest,
	"GameXXK.MVP.Companion.Facade.RouteLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionFacadeRouteLockTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("facade subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}

	FGameXXKPermanentCompanion Recruit;
	if (!RecruitOneCompanion(*this, Subsystem, 19031, Recruit))
	{
		return false;
	}
	FGameXXKPermanentCompanion Companion;
	TestTrue(TEXT("the lock fixture can read its recruited companion"), Subsystem->TryGetPermanentCompanionView(Recruit.InstanceId, Companion));
	const TArray<FName> CompanionLoadout = FirstCards(Companion.UnlockedPersonalCardIds, 5);
	const TArray<FName> HeroLoadout = Subsystem->GetHeroCardLoadout();
	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the lock fixture finds the task NPC definition"), TusiChief);
	if (!TusiChief || HeroLoadout.Num() != 8)
	{
		return false;
	}

	Subsystem->GetMutableRuntimeState().CardRun.bLoadoutLockedForRoute = true;
	TestTrue(TEXT("the facade reports the active route loadout lock"), Subsystem->IsCompanionLoadoutMutationLocked());
	FGameXXKCompanionRecruitResult LockedRecruitResult;
	TestFalse(TEXT("the facade blocks new persistent recruitment after the route lock"),
		Subsystem->RecruitPermanentCompanionFromSeed(19032, LockedRecruitResult));
	TestFalse(TEXT("the facade blocks active-companion selection after the route lock"), Subsystem->SetActivePermanentCompanion(Recruit.InstanceId));
	TestFalse(TEXT("the facade blocks companion card edits after the route lock"), Subsystem->SetPermanentCompanionCardLoadout(Recruit.InstanceId, CompanionLoadout));
	TestFalse(TEXT("the facade blocks hero card edits after the route lock"), Subsystem->SetHeroCardLoadout(HeroLoadout));
	TestFalse(TEXT("the facade blocks NPC card edits after the route lock"),
		Subsystem->SetTemporaryQuestNpcCardLoadout(TusiChief->NpcId, FirstCards(TusiChief->FixedCardIds, 3)));
	TestFalse(TEXT("the facade blocks permanent progression after the route lock"), Subsystem->AwardPermanentCompanionExperience(Recruit.InstanceId, 40));
	TestFalse(TEXT("the facade blocks star promotion after the route lock"), Subsystem->PromotePermanentCompanionStar(Recruit.InstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOrderedFormationFacadeTransactionTest,
	"GameXXK.MVP.Companion.Facade.OrderedFormationTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOrderedFormationFacadeTransactionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("ordered-formation facade subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("ordered-formation facade starts a new game"), Subsystem->StartGame()))
	{
		return false;
	}

	const FGameXXKRuntimeState InitialState = Subsystem->GetRuntimeStateCopy();
	TestEqual(TEXT("StartGame materializes exactly three raw ordered members before save"),
		InitialState.CardRun.OrderedFormation.Members.Num(), FGameXXKPartyFormationRules::PartySize);
	const FGameXXKOrderedPartyFormation InitialEffective = Subsystem->GetOrderedPartyFormation();
	TestEqual(TEXT("formation getter returns exactly three effective members"),
		InitialEffective.Members.Num(), FGameXXKPartyFormationRules::PartySize);
	TestEqual(TEXT("materialized raw and effective formations agree"),
		InitialState.CardRun.OrderedFormation.Members, InitialEffective.Members);
	if (InitialEffective.Members.Num() != FGameXXKPartyFormationRules::PartySize)
	{
		return false;
	}

	const TArray<FName> HeroUnlockedBefore = InitialState.CardRun.HeroUnlockedCardIds;
	const TArray<FName> HeroSelectedBefore = InitialState.CardRun.HeroSelectedCardIds;
	const FGameXXKCompanionRosterState RosterBefore = InitialState.CardRun.CompanionRoster;
	const TMap<FName, FGameXXKQuestNpcOwnedCardLoadout> NpcLoadoutsBefore =
		InitialState.CardRun.PartySelection.QuestNpcCardLoadouts;
	const TArray<FName> ActiveNpcCardsBefore = InitialState.CardRun.PartySelection.QuestNpc.SelectedCardIds;

	FGameXXKOrderedPartyFormation Reordered = InitialEffective;
	Swap(Reordered.Members[0], Reordered.Members[2]);
	FString Error;
	if (!TestTrue(TEXT("legal 1P/3P reorder commits atomically"),
		Subsystem->SetOrderedPartyFormation(Reordered, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("successful formation commit clears the error"), Error.IsEmpty());
	const FGameXXKRuntimeState CommittedState = Subsystem->GetRuntimeStateCopy();
	TestEqual(TEXT("raw ordered formation updates to the exact requested order"),
		CommittedState.CardRun.OrderedFormation.Members, Reordered.Members);
	TestEqual(TEXT("effective formation updates to the exact requested order"),
		Subsystem->GetOrderedPartyFormation().Members, Reordered.Members);

	FName FirstCompanionId = NAME_None;
	FName FirstNpcId = NAME_None;
	for (const FGameXXKPartyMemberRef& Ref : Reordered.Members)
	{
		if (FirstCompanionId.IsNone() && Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion)
		{
			FirstCompanionId = Ref.MemberId;
		}
		else if (FirstNpcId.IsNone() && Ref.Kind == EGameXXKPartyMemberKind::QuestNpc)
		{
			FirstNpcId = Ref.MemberId;
		}
	}
	TestEqual(TEXT("compatibility projection follows the first ordered companion"),
		CommittedState.CardRun.PartySelection.ActivePermanentCompanionInstanceId, FirstCompanionId);
	TestEqual(TEXT("compatibility projection follows the first ordered task NPC"),
		CommittedState.CardRun.ActiveTemporaryQuestNpcId, FirstNpcId);
	FGameXXKRuntimeState ProjectionProbe = CommittedState;
	FGameXXKPartyFormationRules::ProjectCompatibility(ProjectionProbe);
	TestEqual(TEXT("compatibility projection never reorders authoritative members"),
		ProjectionProbe.CardRun.OrderedFormation.Members, Reordered.Members);

	TestEqual(TEXT("formation commit leaves hero unlock deck unchanged"),
		CommittedState.CardRun.HeroUnlockedCardIds, HeroUnlockedBefore);
	TestEqual(TEXT("formation commit leaves hero selected deck unchanged"),
		CommittedState.CardRun.HeroSelectedCardIds, HeroSelectedBefore);
	TestTrue(TEXT("formation reorder leaves the owned roster unchanged"),
		FGameXXKCompanionRosterState::StaticStruct()->CompareScriptStruct(
			&RosterBefore,
			&CommittedState.CardRun.CompanionRoster,
			PPF_None));
	TestEqual(TEXT("formation commit leaves the active NPC deck unchanged"),
		CommittedState.CardRun.PartySelection.QuestNpc.SelectedCardIds, ActiveNpcCardsBefore);
	TestEqual(TEXT("formation commit leaves owned NPC loadout count unchanged"),
		CommittedState.CardRun.PartySelection.QuestNpcCardLoadouts.Num(), NpcLoadoutsBefore.Num());
	for (const TPair<FName, FGameXXKQuestNpcOwnedCardLoadout>& Pair : NpcLoadoutsBefore)
	{
		const FGameXXKQuestNpcOwnedCardLoadout* After =
			CommittedState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(Pair.Key);
		TestNotNull(FString::Printf(TEXT("owned NPC %s loadout remains present"), *Pair.Key.ToString()), After);
		if (After)
		{
			TestEqual(FString::Printf(TEXT("owned NPC %s deck remains exact"), *Pair.Key.ToString()),
				After->SelectedCardIds, Pair.Value.SelectedCardIds);
		}
	}

	const TArray<uint8> BeforeIdempotent = SerializeFacadeRuntimeState(Subsystem->GetRuntimeState());
	TestTrue(TEXT("setting the already-committed formation is idempotently safe"),
		Subsystem->SetOrderedPartyFormation(Reordered, Error));
	TestEqual(TEXT("idempotent formation set leaves runtime state bit-identical"),
		SerializeFacadeRuntimeState(Subsystem->GetRuntimeState()), BeforeIdempotent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOrderedFormationFacadeRejectionTest,
	"GameXXK.MVP.Companion.Facade.OrderedFormationRejections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOrderedFormationFacadeRejectionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("formation rejection subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("formation rejection subsystem starts"), Subsystem->StartGame()))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation Valid = Subsystem->GetOrderedPartyFormation();
	if (!TestEqual(TEXT("rejection fixture starts with three effective members"), Valid.Members.Num(), 3))
	{
		return false;
	}

	FGameXXKOrderedPartyFormation WrongSize = Valid;
	WrongSize.Members.Pop();
	AssertFormationRejectedWithoutMutation(*this, Subsystem, WrongSize, TEXT("wrong-size formation"));

	FGameXXKOrderedPartyFormation Duplicate = Valid;
	Duplicate.Members[1] = Duplicate.Members[0];
	AssertFormationRejectedWithoutMutation(*this, Subsystem, Duplicate, TEXT("duplicate formation"));

	FGameXXKOrderedPartyFormation NoHero;
	for (const FGameXXKPermanentCompanion& Companion : Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions)
	{
		if (NoHero.Members.Num() >= 3)
		{
			break;
		}
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
		Ref.MemberId = Companion.InstanceId;
		NoHero.Members.Add(Ref);
	}
	AssertFormationRejectedWithoutMutation(*this, Subsystem, NoHero, TEXT("no-hero formation"));

	FGameXXKOrderedPartyFormation Unknown = Valid;
	Unknown.Members[1].Kind = EGameXXKPartyMemberKind::PermanentCompanion;
	Unknown.Members[1].MemberId = TEXT("Companion.Unknown.Facade");
	AssertFormationRejectedWithoutMutation(*this, Subsystem, Unknown, TEXT("unknown-member formation"));

	FGameXXKOrderedPartyFormation StaleNpc = Valid;
	const int32 NpcIndex = StaleNpc.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
	});
	if (TestTrue(TEXT("rejection fixture contains a task NPC"), NpcIndex != INDEX_NONE))
	{
		StaleNpc.Members[NpcIndex].MemberId = TEXT("Npc.YueBai");
		AssertFormationRejectedWithoutMutation(*this, Subsystem, StaleNpc, TEXT("stale task-NPC formation"));
	}

	FGameXXKOrderedPartyFormation LegalSwap = Valid;
	Swap(LegalSwap.Members[0], LegalSwap.Members[1]);
	FGameXXKRuntimeState& Mutable = Subsystem->GetMutableRuntimeState();
	Mutable.CardRun.bLoadoutLockedForRoute = true;
	AssertFormationRejectedWithoutMutation(*this, Subsystem, LegalSwap, TEXT("route-loadout-locked formation"));
	Mutable.CardRun.bLoadoutLockedForRoute = false;
	Mutable.bHasActiveBattle = true;
	AssertFormationRejectedWithoutMutation(*this, Subsystem, LegalSwap, TEXT("legacy-battle-active formation"));
	Mutable.bHasActiveBattle = false;
	Mutable.CardRun.bHasActiveCardBattle = true;
	AssertFormationRejectedWithoutMutation(*this, Subsystem, LegalSwap, TEXT("card-battle-active formation"));
	Mutable.CardRun.bHasActiveCardBattle = false;
	const EGameXXKScreen OriginalScreen = Mutable.Screen;
	Mutable.Screen = EGameXXKScreen::Battle;
	AssertFormationRejectedWithoutMutation(*this, Subsystem, LegalSwap, TEXT("battle-screen formation"));
	Mutable.Screen = OriginalScreen;

	Mutable.PlayerGold = -1;
	AssertFormationRejectedWithoutMutation(*this, Subsystem, LegalSwap, TEXT("authoritatively-invalid candidate state"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOrderedFormationFacadePersistenceTest,
	"GameXXK.MVP.Companion.Facade.OrderedFormationPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOrderedFormationFacadePersistenceTest::RunTest(const FString& Parameters)
{
	const FString Slot = TEXT("GameXXK_Automation_OrderedFormationFacadePersistence");
	constexpr int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(Slot, UserIndex);

	UGameXXKMVPSubsystem* Source = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("formation persistence source exists"), Source)
		|| !TestTrue(TEXT("formation persistence source starts"), Source->StartGame()))
	{
		return false;
	}
	FGameXXKOrderedPartyFormation Swapped = Source->GetOrderedPartyFormation();
	if (!TestEqual(TEXT("formation persistence source has three members"), Swapped.Members.Num(), 3))
	{
		return false;
	}
	Swap(Swapped.Members[0], Swapped.Members[2]);
	FString Error;
	TestTrue(TEXT("formation persistence source commits swap"), Source->SetOrderedPartyFormation(Swapped, Error));
	TestTrue(TEXT("formation persistence source saves"), Source->SaveCurrentGame(Slot, UserIndex));

	UGameXXKMVPSubsystem* Loaded = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestNotNull(TEXT("formation persistence target exists"), Loaded);
	if (!Loaded)
	{
		return false;
	}
	TestTrue(TEXT("formation persistence target loads"), Loaded->LoadGameFromSlot(Slot, UserIndex));
	TestEqual(TEXT("facade load preserves exact swapped order"),
		Loaded->GetOrderedPartyFormation().Members, Swapped.Members);
	TestEqual(TEXT("raw loaded save preserves exact swapped order"),
		Loaded->GetRuntimeState().CardRun.OrderedFormation.Members, Swapped.Members);
	UGameplayStatics::DeleteGameInSlot(Slot, UserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLegacyFormationFacadeAuthorityTest,
	"GameXXK.MVP.Companion.Facade.LegacyFormationAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLegacyFormationFacadeAuthorityTest::RunTest(const FString& Parameters)
{
	auto MakeStartedSubsystem = [this](const TCHAR* Label)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!TestNotNull(Label, Subsystem) || !TestTrue(FString::Printf(TEXT("%s starts"), Label), Subsystem->StartGame()))
		{
			return static_cast<UGameXXKMVPSubsystem*>(nullptr);
		}
		return Subsystem;
	};
	auto FindUndeployedCompanionId = [](const FGameXXKRuntimeState& State)
	{
		for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (!State.CardRun.OrderedFormation.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.MemberId == Companion.InstanceId;
			}))
			{
				return Companion.InstanceId;
			}
		}
		return FName(NAME_None);
	};
	auto AssertCurrentRoundTrip = [this](const TCHAR* Label, const FGameXXKRuntimeState& State)
	{
		FString ValidationError;
		bool bPassed = TestTrue(
			FString::Printf(TEXT("%s passes authoritative validation"), Label),
			FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError));
		FGameXXKSaveState RoundTrip;
		FGameXXKSaveMigrationReport Report;
		bPassed &= TestTrue(
			FString::Printf(TEXT("%s roundtrips as v24"), Label),
			FGameXXKSaveMigration::MigrateToCurrent(UGameXXKMVPRules::MakeSaveState(State), RoundTrip, Report));
		return bPassed;
	};

	UGameXXKMVPSubsystem* ReplaceSubsystem = MakeStartedSubsystem(TEXT("legacy companion replacement subsystem"));
	if (!ReplaceSubsystem)
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation ReplaceBefore = ReplaceSubsystem->GetOrderedPartyFormation();
	const FName UndeployedCompanionId = FindUndeployedCompanionId(ReplaceSubsystem->GetRuntimeState());
	TestFalse(TEXT("legacy companion replacement fixture finds an undeployed companion"), UndeployedCompanionId.IsNone());
	const int32 FirstCompanionSlot = ReplaceBefore.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
	});
	if (!TestTrue(TEXT("legacy companion replacement fixture has a companion slot"), FirstCompanionSlot != INDEX_NONE)
		|| UndeployedCompanionId.IsNone())
	{
		return false;
	}
	TestTrue(TEXT("legacy SetActive replaces the first companion slot"),
		ReplaceSubsystem->SetActivePermanentCompanion(UndeployedCompanionId));
	const FGameXXKOrderedPartyFormation ReplaceAfter = ReplaceSubsystem->GetOrderedPartyFormation();
	TestEqual(TEXT("selected undeployed companion occupies the same exact slot"),
		ReplaceAfter.Members[FirstCompanionSlot].MemberId, UndeployedCompanionId);
	for (int32 SlotIndex = 0; SlotIndex < ReplaceBefore.Members.Num(); ++SlotIndex)
	{
		if (SlotIndex != FirstCompanionSlot)
		{
			TestTrue(TEXT("legacy companion replacement preserves every other slot"),
				ReplaceAfter.Members[SlotIndex] == ReplaceBefore.Members[SlotIndex]);
		}
	}
	AssertCurrentRoundTrip(TEXT("legacy companion replacement"), ReplaceSubsystem->GetRuntimeState());

	UGameXXKMVPSubsystem* SwapSubsystem = MakeStartedSubsystem(TEXT("legacy companion swap subsystem"));
	if (!SwapSubsystem)
	{
		return false;
	}
	FGameXXKOrderedPartyFormation TwoCompanions = SwapSubsystem->GetOrderedPartyFormation();
	const FName SecondCompanionId = FindUndeployedCompanionId(SwapSubsystem->GetRuntimeState());
	const int32 NpcSlot = TwoCompanions.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
	});
	if (!TestTrue(TEXT("legacy companion swap fixture has an NPC slot"), NpcSlot != INDEX_NONE)
		|| SecondCompanionId.IsNone())
	{
		return false;
	}
	TwoCompanions.Members[NpcSlot].Kind = EGameXXKPartyMemberKind::PermanentCompanion;
	TwoCompanions.Members[NpcSlot].MemberId = SecondCompanionId;
	FString FormationError;
	TestTrue(TEXT("legacy companion swap fixture commits two companions"),
		SwapSubsystem->SetOrderedPartyFormation(TwoCompanions, FormationError));
	const int32 TwoCompanionFirstSlot = TwoCompanions.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
	});
	int32 TwoCompanionLaterSlot = INDEX_NONE;
	for (int32 SlotIndex = TwoCompanionFirstSlot + 1; SlotIndex < TwoCompanions.Members.Num(); ++SlotIndex)
	{
		if (TwoCompanions.Members[SlotIndex].Kind == EGameXXKPartyMemberKind::PermanentCompanion)
		{
			TwoCompanionLaterSlot = SlotIndex;
			break;
		}
	}
	if (!TestTrue(TEXT("legacy companion swap fixture has two companion slots"),
		TwoCompanionFirstSlot != INDEX_NONE && TwoCompanionLaterSlot != INDEX_NONE))
	{
		return false;
	}
	const FGameXXKPartyMemberRef FirstCompanionBeforeSwap = TwoCompanions.Members[TwoCompanionFirstSlot];
	const FGameXXKPartyMemberRef LaterCompanionBeforeSwap = TwoCompanions.Members[TwoCompanionLaterSlot];
	TestTrue(TEXT("legacy SetActive swaps an already-deployed later companion forward"),
		SwapSubsystem->SetActivePermanentCompanion(LaterCompanionBeforeSwap.MemberId));
	const FGameXXKOrderedPartyFormation SwapAfter = SwapSubsystem->GetOrderedPartyFormation();
	TestTrue(TEXT("selected later companion becomes first companion"),
		SwapAfter.Members[TwoCompanionFirstSlot] == LaterCompanionBeforeSwap);
	TestTrue(TEXT("old first companion moves into the selected companion's old slot"),
		SwapAfter.Members[TwoCompanionLaterSlot] == FirstCompanionBeforeSwap);
	AssertCurrentRoundTrip(TEXT("legacy companion swap"), SwapSubsystem->GetRuntimeState());

	UGameXXKMVPSubsystem* NpcSubsystem = MakeStartedSubsystem(TEXT("legacy NPC replacement subsystem"));
	if (!NpcSubsystem)
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation NpcBefore = NpcSubsystem->GetOrderedPartyFormation();
	const int32 ExistingNpcSlot = NpcBefore.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
	});
	if (!TestTrue(TEXT("legacy NPC replacement fixture has an NPC slot"), ExistingNpcSlot != INDEX_NONE))
	{
		return false;
	}
	const FName YueBaiId(TEXT("Npc.YueBai"));
	TestTrue(TEXT("legacy town NPC facade selects Yue Bai"), NpcSubsystem->SelectTownQuestNpcForParty(YueBaiId));
	const FGameXXKRuntimeState& NpcAfterState = NpcSubsystem->GetRuntimeState();
	TestEqual(TEXT("selected NPC occupies the exact old NPC slot"),
		NpcAfterState.CardRun.OrderedFormation.Members[ExistingNpcSlot].MemberId, YueBaiId);
	for (int32 SlotIndex = 0; SlotIndex < NpcBefore.Members.Num(); ++SlotIndex)
	{
		if (SlotIndex != ExistingNpcSlot)
		{
			TestTrue(TEXT("legacy NPC replacement preserves every other slot"),
				NpcAfterState.CardRun.OrderedFormation.Members[SlotIndex] == NpcBefore.Members[SlotIndex]);
		}
	}
	const FGameXXKQuestNpcOwnedCardLoadout* YueBaiLoadout =
		NpcAfterState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(YueBaiId);
	TestNotNull(TEXT("Yue Bai keeps an owned loadout"), YueBaiLoadout);
	if (YueBaiLoadout)
	{
		TestEqual(TEXT("selected NPC cards come from the owned loadout"),
			NpcAfterState.CardRun.PartySelection.QuestNpc.SelectedCardIds,
			YueBaiLoadout->SelectedCardIds);
	}
	AssertCurrentRoundTrip(TEXT("legacy NPC replacement"), NpcAfterState);

	UGameXXKMVPSubsystem* ClearSubsystem = MakeStartedSubsystem(TEXT("legacy clear subsystem"));
	if (!ClearSubsystem)
	{
		return false;
	}
	const TArray<uint8> BeforeClear = SerializeFacadeRuntimeState(ClearSubsystem->GetRuntimeState());
	TestFalse(TEXT("ClearActive rejects while exact formation still contains a companion"),
		ClearSubsystem->ClearActivePermanentCompanion());
	TestEqual(TEXT("rejected ClearActive leaves runtime bit-identical"),
		SerializeFacadeRuntimeState(ClearSubsystem->GetRuntimeState()), BeforeClear);
	TestFalse(TEXT("SetActive NAME_None also rejects exact formation authority"),
		ClearSubsystem->SetActivePermanentCompanion(NAME_None));
	TestEqual(TEXT("rejected SetActive NAME_None leaves runtime bit-identical"),
		SerializeFacadeRuntimeState(ClearSubsystem->GetRuntimeState()), BeforeClear);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDismissFormationSlotRepairTest,
	"GameXXK.MVP.Companion.Facade.DismissFormationSlotRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDismissFormationSlotRepairTest::RunTest(const FString& Parameters)
{
	auto MakeCompanionRef = [](const FName InstanceId)
	{
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = EGameXXKPartyMemberKind::PermanentCompanion;
		Ref.MemberId = InstanceId;
		return Ref;
	};
	auto MakeHeroRef = []()
	{
		FGameXXKPartyMemberRef Ref;
		Ref.Kind = EGameXXKPartyMemberKind::Hero;
		Ref.MemberId = FGameXXKEquipmentRules::HeroCharacterId();
		return Ref;
	};

	UGameXXKMVPSubsystem* Deployed = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("deployed dismissal subsystem exists"), Deployed)
		|| !TestTrue(TEXT("deployed dismissal subsystem starts"), Deployed->StartGame()))
	{
		return false;
	}
	const TArray<FGameXXKPermanentCompanion>& Roster =
		Deployed->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions;
	if (!TestTrue(TEXT("deployed dismissal fixture has three companion candidates"), Roster.Num() >= 3))
	{
		return false;
	}
	const FName SurvivorId = Roster[1].InstanceId;
	const FName DismissedId = Roster[0].InstanceId;
	FGameXXKOrderedPartyFormation Before;
	Before.Members = {MakeCompanionRef(SurvivorId), MakeHeroRef(), MakeCompanionRef(DismissedId)};
	FString FormationError;
	if (!TestTrue(TEXT("deployed dismissal fixture commits survivor/hero/dismissed order"),
		Deployed->SetOrderedPartyFormation(Before, FormationError)))
	{
		AddError(FormationError);
		return false;
	}
	TestTrue(TEXT("ordinary dismissal succeeds for deployed companion"),
		Deployed->DismissPermanentCompanion(DismissedId));
	const FGameXXKRuntimeState& DeployedAfter = Deployed->GetRuntimeState();
	const TArray<FGameXXKPartyMemberRef>& AfterMembers = DeployedAfter.CardRun.OrderedFormation.Members;
	if (!TestEqual(TEXT("ordinary dismissal keeps exact party size"), AfterMembers.Num(), 3))
	{
		return false;
	}
	TestTrue(TEXT("ordinary dismissal keeps 1P bit-identical"), AfterMembers[0] == Before.Members[0]);
	TestTrue(TEXT("ordinary dismissal keeps non-companion 2P bit-identical"), AfterMembers[1] == Before.Members[1]);
	TestEqual(TEXT("ordinary dismissal fills only the removed 3P slot kind"),
		AfterMembers[2].Kind, EGameXXKPartyMemberKind::PermanentCompanion);
	TestNotEqual(TEXT("ordinary dismissal replaces removed 3P identity"), AfterMembers[2].MemberId, DismissedId);
	TestFalse(TEXT("ordinary dismissal removes old roster identity"),
		DeployedAfter.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
			[DismissedId](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == DismissedId;
			}));
	FString ValidationError;
	TestTrue(TEXT("ordinary dismissal result passes full v24 validation"),
		FGameXXKSaveMigration::ValidateRuntimeState(DeployedAfter, ValidationError));

	UGameXXKMVPSubsystem* OffFormation = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("off-formation dismissal subsystem exists"), OffFormation)
		|| !TestTrue(TEXT("off-formation dismissal subsystem starts"), OffFormation->StartGame()))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation OffFormationBefore = OffFormation->GetOrderedPartyFormation();
	FName OffFormationId = NAME_None;
	for (const FGameXXKPermanentCompanion& Companion : OffFormation->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions)
	{
		if (!OffFormationBefore.Members.ContainsByPredicate([&Companion](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.MemberId == Companion.InstanceId;
		}))
		{
			OffFormationId = Companion.InstanceId;
			break;
		}
	}
	if (!TestFalse(TEXT("off-formation dismissal fixture finds undeployed companion"), OffFormationId.IsNone()))
	{
		return false;
	}
	TestTrue(TEXT("ordinary dismissal succeeds for off-formation companion"),
		OffFormation->DismissPermanentCompanion(OffFormationId));
	TestEqual(TEXT("off-formation dismissal preserves ordered array byte-semantically"),
		OffFormation->GetRuntimeState().CardRun.OrderedFormation.Members,
		OffFormationBefore.Members);

	UGameXXKMVPSubsystem* NoReplacement = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("no-replacement dismissal subsystem exists"), NoReplacement)
		|| !TestTrue(TEXT("no-replacement dismissal subsystem starts"), NoReplacement->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& SparseState = NoReplacement->GetMutableRuntimeState();
	const FName SparseDismissedId = SparseState.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId;
	const FName SparseSurvivorId = SparseState.CardRun.CompanionRoster.PermanentCompanions[1].InstanceId;
	SparseState.CardRun.CompanionRoster.PermanentCompanions.RemoveAll(
		[SparseDismissedId, SparseSurvivorId](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId != SparseDismissedId && Companion.InstanceId != SparseSurvivorId;
		});
	FGameXXKOrderedPartyFormation SparseFormation;
	SparseFormation.Members = {
		MakeHeroRef(),
		MakeCompanionRef(SparseDismissedId),
		MakeCompanionRef(SparseSurvivorId)};
	SparseState.CardRun.OrderedFormation = SparseFormation;
	FGameXXKPartyFormationRules::ProjectCompatibility(SparseState);
	const TArray<uint8> SparseBefore = SerializeFacadeRuntimeState(SparseState);
	TestFalse(TEXT("ordinary dismissal rejects when no unique slot replacement exists"),
		NoReplacement->DismissPermanentCompanion(SparseDismissedId));
	TestEqual(TEXT("no-replacement dismissal rolls back every runtime byte"),
		SerializeFacadeRuntimeState(NoReplacement->GetRuntimeState()), SparseBefore);
	return true;
}

#endif
