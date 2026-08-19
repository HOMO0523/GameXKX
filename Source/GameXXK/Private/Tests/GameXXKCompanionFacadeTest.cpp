#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"

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

#endif
