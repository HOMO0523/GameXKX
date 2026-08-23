#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCompanionRosterWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool PrepareFullRosterWithTwoUnownedSequenceCandidates(
		FAutomationTestBase& Test,
		FGameXXKCompanionRosterState& OutRoster,
		FGameXXKPermanentCompanion& OutFirstCandidate,
		FGameXXKPermanentCompanion& OutSecondCandidate)
	{
		OutRoster = FGameXXKCompanionRosterState();
		OutFirstCandidate = FGameXXKPermanentCompanion();
		OutSecondCandidate = FGameXXKPermanentCompanion();

		FGameXXKCompanionRosterState SequenceProbe;
		FGameXXKCompanionRecruitResult FirstProbe;
		FGameXXKCompanionRecruitResult SecondProbe;
		if (!Test.TestTrue(TEXT("the first deterministic sequence probe resolves"),
			FGameXXKCompanionRules::CreateAndResolveNextRecruitment(SequenceProbe, FirstProbe, nullptr))
			|| !Test.TestTrue(TEXT("the second deterministic sequence probe resolves"),
				FGameXXKCompanionRules::CreateAndResolveNextRecruitment(SequenceProbe, SecondProbe, nullptr)))
		{
			return false;
		}
		if (FirstProbe.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
			|| SecondProbe.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
			|| FirstProbe.Companion.RecruitTemplateId == SecondProbe.Companion.RecruitTemplateId)
		{
			Test.AddError(TEXT("the first two deterministic recruit tickets must produce distinct new templates."));
			return false;
		}

		const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
		for (const FGameXXKCompanionTemplateDefinition& Template : Templates)
		{
			if (Template.TemplateId == FirstProbe.Companion.RecruitTemplateId
				|| Template.TemplateId == SecondProbe.Companion.RecruitTemplateId
				|| OutRoster.PermanentCompanions.Num() >= FGameXXKCompanionRules::MaxPermanentCompanions)
			{
				continue;
			}

			FGameXXKCompanionRecruitResult FillResult;
			if (!Test.TestTrue(FString::Printf(TEXT("a unique template can fill full-roster fixture: %s"), *Template.TemplateId.ToString()),
				FGameXXKCompanionRules::RecruitPermanentCompanion(
					OutRoster,
					Template.TemplateId,
					100000 + OutRoster.PermanentCompanions.Num(),
					FillResult,
					nullptr)))
			{
				return false;
			}
		}

		if (!Test.TestEqual(TEXT("the fixture fills exactly twelve permanent roster entries"),
			OutRoster.PermanentCompanions.Num(), FGameXXKCompanionRules::MaxPermanentCompanions))
		{
			return false;
		}

		OutRoster.RecruitSequenceSeed = SequenceProbe.RecruitSequenceSeed;
		OutRoster.RecruitSequenceOrdinal = 0;
		OutFirstCandidate = FirstProbe.Companion;
		OutSecondCandidate = SecondProbe.Companion;
		return true;
	}

	UGameXXKCompanionRosterWidget* BuildRosterWidget(UGameXXKMVPSubsystem* Subsystem)
	{
		UGameXXKCompanionRosterWidget* Widget = NewObject<UGameXXKCompanionRosterWidget>();
		Widget->SetMVPSubsystem(Subsystem);
		Widget->TakeWidget();
		Widget->RefreshFromState();
		return Widget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRecruitmentSequenceTest,
	"GameXXK.Data.Companion.RecruitmentFlow.SequenceAndPendingReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRecruitmentSequenceTest::RunTest(const FString& Parameters)
{
	FGameXXKCompanionRosterState FirstRoster;
	FGameXXKCompanionRosterState SecondRoster;
	FGameXXKCompanionRecruitResult FirstResult;
	FGameXXKCompanionRecruitResult SecondResult;
	TestTrue(TEXT("the first saved deterministic recruitment resolves"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(FirstRoster, FirstResult, nullptr));
	TestTrue(TEXT("the matching saved deterministic recruitment resolves"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(SecondRoster, SecondResult, nullptr));
	TestEqual(TEXT("matching uninitialized saves choose the same first template"), FirstResult.Companion.RecruitTemplateId, SecondResult.Companion.RecruitTemplateId);
	TestEqual(TEXT("matching uninitialized saves choose the same first personal six-card birth pool"), FirstResult.Companion.PersonalCardIds, SecondResult.Companion.PersonalCardIds);
	TestEqual(TEXT("a new permanent recruit owns six birth cards"), FirstResult.Companion.PersonalCardIds.Num(), 6);
	TestTrue(TEXT("the old-save fallback initializes a persistent sequence seed"), FirstRoster.RecruitSequenceSeed != 0);
	TestEqual(TEXT("the first claim advances the persisted sequence ordinal"), FirstRoster.RecruitSequenceOrdinal, 1);

	FGameXXKCompanionRecruitResult FirstSecondResult;
	FGameXXKCompanionRecruitResult SecondSecondResult;
	TestTrue(TEXT("the next persistent sequence ticket resolves"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(FirstRoster, FirstSecondResult, nullptr));
	TestTrue(TEXT("the matching next persistent sequence ticket resolves"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(SecondRoster, SecondSecondResult, nullptr));
	TestEqual(TEXT("saved sequence order persists after a runtime copy"), FirstSecondResult.Companion.RecruitTemplateId, SecondSecondResult.Companion.RecruitTemplateId);
	TestEqual(TEXT("saved sequence card seed persists after a runtime copy"), FirstSecondResult.Companion.CardSeed, SecondSecondResult.Companion.CardSeed);
	TestNotEqual(TEXT("the first two sequence tickets are not duplicate templates"), FirstResult.Companion.RecruitTemplateId, FirstSecondResult.Companion.RecruitTemplateId);
	TestNotEqual(TEXT("the first two sequence tickets use different personal pools"), FirstResult.Companion.PersonalCardIds, FirstSecondResult.Companion.PersonalCardIds);

	FGameXXKCompanionRosterState FullRoster;
	FGameXXKPermanentCompanion ExpectedFirstCandidate;
	FGameXXKPermanentCompanion ExpectedSecondCandidate;
	if (!PrepareFullRosterWithTwoUnownedSequenceCandidates(*this, FullRoster, ExpectedFirstCandidate, ExpectedSecondCandidate))
	{
		return false;
	}

	FGameXXKCompanionRecruitResult PendingResult;
	TestTrue(TEXT("a full roster resolves its saved ticket into a pending replacement"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(FullRoster, PendingResult, nullptr));
	TestEqual(TEXT("the full-roster ticket reports pending replacement"), PendingResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	TestEqual(TEXT("the full-roster candidate preserves the first saved sequence identity"), PendingResult.Companion.RecruitTemplateId, ExpectedFirstCandidate.RecruitTemplateId);
	TestTrue(TEXT("the full roster persists the fixed candidate"), FullRoster.PendingRecruitment.bHasPendingRecruitment);
	const int32 OrdinalWhilePending = FullRoster.RecruitSequenceOrdinal;

	FGameXXKCompanionRecruitResult ReopenedPendingResult;
	TestTrue(TEXT("reopening a full-roster pending ticket does not reroll"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(FullRoster, ReopenedPendingResult, nullptr));
	TestEqual(TEXT("reopening preserves the candidate stable instance id"), ReopenedPendingResult.Companion.InstanceId, PendingResult.Companion.InstanceId);
	TestEqual(TEXT("reopening does not consume a further sequence ordinal"), FullRoster.RecruitSequenceOrdinal, OrdinalWhilePending);

	TArray<FName> RosterIdsBeforeDiscard;
	for (const FGameXXKPermanentCompanion& Existing : FullRoster.PermanentCompanions)
	{
		RosterIdsBeforeDiscard.Add(Existing.InstanceId);
	}
	TestTrue(TEXT("the player can explicitly discard a fixed full-roster candidate"),
		FGameXXKCompanionRules::DiscardPendingRecruitment(FullRoster, nullptr));
	TestFalse(TEXT("discard clears the pending full-roster candidate"), FullRoster.PendingRecruitment.bHasPendingRecruitment);
	TestFalse(TEXT("discard clears the associated no-reroll order"), FullRoster.PendingRecruitOrder.bHasPendingOrder);
	TestEqual(TEXT("discard never changes the permanent roster count"), FullRoster.PermanentCompanions.Num(), RosterIdsBeforeDiscard.Num());
	for (int32 Index = 0; Index < RosterIdsBeforeDiscard.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("discard never changes permanent companion %d"), Index), FullRoster.PermanentCompanions[Index].InstanceId, RosterIdsBeforeDiscard[Index]);
	}

	FGameXXKCompanionRecruitResult PostDiscardResult;
	TestTrue(TEXT("a later player-initiated request advances to the next ticket after discard"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(FullRoster, PostDiscardResult, nullptr));
	TestEqual(TEXT("the next full-roster ticket remains a replacement candidate"), PostDiscardResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	TestEqual(TEXT("discarded ticket does not reroll into a different first candidate"), PostDiscardResult.Companion.RecruitTemplateId, ExpectedSecondCandidate.RecruitTemplateId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRecruitmentFacadePersistenceTest,
	"GameXXK.MVP.Companion.RecruitmentFlow.TownFacadeAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRecruitmentFacadePersistenceTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("recruitment facade subsystem exists"), Subsystem);
	if (!Subsystem || !TestTrue(TEXT("the facade can establish a town runtime"), Subsystem->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}

	FGameXXKCompanionRecruitResult FirstTownRecruit;
	TestTrue(TEXT("the town facade starts a random permanent recruitment"), Subsystem->StartRandomPermanentCompanionRecruitment(FirstTownRecruit));
	TestEqual(TEXT("the town facade yields a permanent recruit while roster has space"), FirstTownRecruit.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
	TestEqual(TEXT("the facade recruit has its own six-card birth pool"), FirstTownRecruit.Companion.PersonalCardIds.Num(), 6);
	FGameXXKRuntimeState& FirstRecruitState = Subsystem->GetMutableRuntimeState();
	FString FirstRecruitFormationError;
	if (!TestTrue(TEXT("first recruitment save attaches the approved task NPC"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
			FirstRecruitState,
			TEXT("Npc.TusiChief"),
			{},
			&FirstRecruitFormationError))
		|| !TestTrue(TEXT("first recruitment save materializes v24 formation"),
			FGameXXKPartyFormationRules::Normalize(FirstRecruitState, &FirstRecruitFormationError)))
	{
		AddError(FirstRecruitFormationError);
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(FirstRecruitState);

	const FGameXXKSaveState SavedAfterFirstRecruit = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	UGameInstance* ReloadedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* ReloadedSubsystem = NewObject<UGameXXKMVPSubsystem>(ReloadedGameInstance);
	TestNotNull(TEXT("reloaded facade subsystem exists"), ReloadedSubsystem);
	if (!ReloadedSubsystem)
	{
		return false;
	}
	FGameXXKRuntimeState ReloadedRuntimeState;
	FGameXXKSaveMigrationReport ReloadReport;
	if (!TestTrue(
		TEXT("the first recruitment save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SavedAfterFirstRecruit, ReloadedRuntimeState, ReloadReport)))
	{
		return false;
	}
	ReloadedSubsystem->GetMutableRuntimeState() = MoveTemp(ReloadedRuntimeState);
	FGameXXKCompanionRecruitResult ContinuedRecruit;
	TestTrue(TEXT("the restored town facade continues the persisted recruit sequence"), ReloadedSubsystem->StartRandomPermanentCompanionRecruitment(ContinuedRecruit));
	TestNotEqual(TEXT("the restored next ticket is not the first ticket"), ContinuedRecruit.Companion.RecruitTemplateId, FirstTownRecruit.Companion.RecruitTemplateId);

	FGameXXKCompanionRosterState FullRoster;
	FGameXXKPermanentCompanion ExpectedFirstCandidate;
	FGameXXKPermanentCompanion ExpectedSecondCandidate;
	if (!PrepareFullRosterWithTwoUnownedSequenceCandidates(*this, FullRoster, ExpectedFirstCandidate, ExpectedSecondCandidate))
	{
		return false;
	}
	FGameXXKRuntimeState& FullRosterState = Subsystem->GetMutableRuntimeState();
	FullRosterState.CardRun.CompanionRoster = FullRoster;
	FullRosterState.CardRun.OrderedFormation = FGameXXKOrderedPartyFormation();
	FString FormationError;
	if (!TestTrue(TEXT("full-roster persistence fixture materializes v24 formation"),
		FGameXXKPartyFormationRules::Normalize(FullRosterState, &FormationError)))
	{
		AddError(FormationError);
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(FullRosterState);

	FGameXXKCompanionRecruitResult PendingResult;
	TestTrue(TEXT("the town facade converts a full roster request into a saved replacement candidate"),
		Subsystem->StartRandomPermanentCompanionRecruitment(PendingResult));
	TestEqual(TEXT("the facade returns pending-replacement status at capacity"), PendingResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	FGameXXKPermanentCompanion PendingView;
	TestTrue(TEXT("the facade exposes a copy-safe saved pending candidate"), Subsystem->TryGetPendingPermanentCompanionRecruitment(PendingView));
	TestEqual(TEXT("the pending facade view uses the candidate returned by recruitment"), PendingView.InstanceId, PendingResult.Companion.InstanceId);

	const FGameXXKSaveState PendingSave = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	UGameInstance* PendingReloadedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* PendingReloadedSubsystem = NewObject<UGameXXKMVPSubsystem>(PendingReloadedGameInstance);
	TestNotNull(TEXT("pending candidate reload facade subsystem exists"), PendingReloadedSubsystem);
	if (!PendingReloadedSubsystem)
	{
		return false;
	}
	FGameXXKRuntimeState PendingReloadedRuntimeState;
	FGameXXKSaveMigrationReport PendingReloadReport;
	if (!TestTrue(
		TEXT("the pending recruitment save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(PendingSave, PendingReloadedRuntimeState, PendingReloadReport)))
	{
		return false;
	}
	PendingReloadedSubsystem->GetMutableRuntimeState() = MoveTemp(PendingReloadedRuntimeState);
	FGameXXKPermanentCompanion ReloadedPendingView;
	TestTrue(TEXT("the saved pending candidate survives restore"), PendingReloadedSubsystem->TryGetPendingPermanentCompanionRecruitment(ReloadedPendingView));
	TestEqual(TEXT("the restored pending candidate keeps the stable identity"), ReloadedPendingView.InstanceId, PendingView.InstanceId);

	const FName DismissedInstanceId = FullRoster.PermanentCompanions[0].InstanceId;
	TestTrue(TEXT("the facade only replaces after the player chooses an existing roster entry"),
		PendingReloadedSubsystem->ResolvePendingPermanentCompanionReplacement(DismissedInstanceId, NAME_None));
	TestEqual(TEXT("explicit replacement keeps the twelve-slot maximum"),
		PendingReloadedSubsystem->GetPermanentCompanionViews().Num(), FGameXXKCompanionRules::MaxPermanentCompanions);
	TestFalse(TEXT("explicit replacement clears the saved candidate"), PendingReloadedSubsystem->TryGetPendingPermanentCompanionRecruitment(ReloadedPendingView));
	TestTrue(TEXT("explicit replacement installs the saved candidate, never a reroll"),
		PendingReloadedSubsystem->GetPermanentCompanionViews().ContainsByPredicate([PendingView](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId == PendingView.InstanceId;
		}));

	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::WorldMap;
	FGameXXKCompanionRecruitResult NotTownResult;
	TestFalse(TEXT("the facade rejects random recruitment outside town"), Subsystem->StartRandomPermanentCompanionRecruitment(NotTownResult));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionReplacementRefundSafetyTest,
	"GameXXK.MVP.Companion.RecruitmentFlow.ReplacementRefundSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionReplacementRefundSafetyTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("refund-safety facade subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->EnsureQingshanTownRuntimeForDirectMap())
	{
		return false;
	}

	FGameXXKCompanionRosterState FullRoster;
	FGameXXKPermanentCompanion IgnoredFirstCandidate;
	FGameXXKPermanentCompanion IgnoredSecondCandidate;
	if (!PrepareFullRosterWithTwoUnownedSequenceCandidates(*this, FullRoster, IgnoredFirstCandidate, IgnoredSecondCandidate))
	{
		return false;
	}
	FGameXXKPermanentCompanion& InvestedCompanion = FullRoster.PermanentCompanions[0];
	InvestedCompanion.Experience = 10;
	InvestedCompanion.EquippedItemIds = {TEXT("Item.Companion.RefundSafety")};
	const FName InvestedInstanceId = InvestedCompanion.InstanceId;
	Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster = FullRoster;

	FGameXXKCompanionRecruitResult PendingResult;
	TestTrue(TEXT("the full invested roster creates a fixed replacement candidate"),
		Subsystem->StartRandomPermanentCompanionRecruitment(PendingResult));
	TestEqual(TEXT("the invested full roster receives pending replacement state"),
		PendingResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	TestFalse(TEXT("replacement is blocked until refundable experience and equipment have an authoritative return path"),
		Subsystem->ResolvePendingPermanentCompanionReplacement(InvestedInstanceId, NAME_None));

	FGameXXKPermanentCompanion PreservedInvestedCompanion;
	TestTrue(TEXT("a blocked replacement preserves the invested companion"),
		Subsystem->TryGetPermanentCompanionView(InvestedInstanceId, PreservedInvestedCompanion));
	TestEqual(TEXT("a blocked replacement preserves spent experience"), PreservedInvestedCompanion.Experience, 10);
	TestEqual(TEXT("a blocked replacement preserves equipped item ownership"),
		PreservedInvestedCompanion.EquippedItemIds, TArray<FName>({TEXT("Item.Companion.RefundSafety")}));
	FGameXXKPermanentCompanion StillPendingCandidate;
	TestTrue(TEXT("a blocked replacement keeps the fixed candidate pending"),
		Subsystem->TryGetPendingPermanentCompanionRecruitment(StillPendingCandidate));
	TestEqual(TEXT("a blocked replacement never rerolls the candidate"), StillPendingCandidate.InstanceId, PendingResult.Companion.InstanceId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRecruitmentRosterInteractionTest,
	"GameXXK.UI.CompanionRoster.Recruitment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRecruitmentRosterInteractionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("roster interaction facade subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->EnsureQingshanTownRuntimeForDirectMap())
	{
		return false;
	}

	FGameXXKCompanionRosterState FullRoster;
	FGameXXKPermanentCompanion ExpectedFirstCandidate;
	FGameXXKPermanentCompanion ExpectedSecondCandidate;
	if (!PrepareFullRosterWithTwoUnownedSequenceCandidates(*this, FullRoster, ExpectedFirstCandidate, ExpectedSecondCandidate))
	{
		return false;
	}
	FGameXXKRuntimeState& RosterUiState = Subsystem->GetMutableRuntimeState();
	RosterUiState.CardRun.CompanionRoster = FullRoster;
	RosterUiState.CardRun.OrderedFormation = FGameXXKOrderedPartyFormation();
	FString RosterUiFormationError;
	if (!TestTrue(TEXT("roster UI fixture materializes v24 formation"),
		FGameXXKPartyFormationRules::Normalize(RosterUiState, &RosterUiFormationError)))
	{
		AddError(RosterUiFormationError);
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(RosterUiState);

	UGameXXKCompanionRosterWidget* Widget = BuildRosterWidget(Subsystem);
	TestNotNull(TEXT("the recruitment backpack builds"), Widget);
	if (!Widget)
	{
		return false;
	}
	// Page 18 removes the standalone 招贤 button (recruitment lives in the shop);
	// the canonical facade action remains the same.
	TestTrue(TEXT("the backpack action starts the saved full-roster candidate"), Widget->BeginRandomRecruitment());
	TestTrue(TEXT("the backpack renders a full-roster candidate after its action"), Widget->HasPendingRecruitmentForTest());
	TestEqual(TEXT("the backpack shows the saved candidate rather than constructing a local copy"), Widget->GetPendingRecruitmentCandidateIdForTest(), ExpectedFirstCandidate.InstanceId);

	Widget->SelectCompanion(FullRoster.PermanentCompanions[0].InstanceId);
	UButton* DismissButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("CompanionRosterReplacePendingAction")))
		: nullptr;
	TestNotNull(TEXT("a pending candidate exposes the real 遣散 action"), DismissButton);
	if (!DismissButton)
	{
		return false;
	}
	DismissButton->OnClicked.Broadcast();
	TestFalse(TEXT("the 遣散 action clears the fixed candidate"), Widget->HasPendingRecruitmentForTest());
	TestEqual(TEXT("the backpack remains capped at twelve permanent partners after replacement"), Subsystem->GetPermanentCompanionViews().Num(), FGameXXKCompanionRules::MaxPermanentCompanions);

	TestTrue(TEXT("the next action shows the next saved full-roster candidate"), Widget->BeginRandomRecruitment());
	TestTrue(TEXT("the next action leaves the next candidate pending"), Widget->HasPendingRecruitmentForTest());
	TestEqual(TEXT("the next candidate follows sequence order after the resolved replacement"), Widget->GetPendingRecruitmentCandidateIdForTest(), ExpectedSecondCandidate.InstanceId);
	TestTrue(TEXT("the retained discard capability clears only the candidate"), Widget->DiscardPendingRecruitment());
	TestFalse(TEXT("the discard action clears only the candidate"), Widget->HasPendingRecruitmentForTest());

	const FName PromotionTargetId = Subsystem->GetPermanentCompanionViews()[0].InstanceId;
	TestTrue(TEXT("a valid roster entry can be selected for real sigil promotion"), Widget->SelectCompanion(PromotionTargetId));
	FGameXXKPermanentCompanion BeforePromotion;
	TestTrue(TEXT("the promotion target is readable"), Subsystem->TryGetPermanentCompanionView(PromotionTargetId, BeforePromotion));
	Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.SigilCount = 1;
	Widget->RefreshFromState();
	TestTrue(TEXT("the retained promotion capability consumes the sigil"), Widget->PromoteSelectedCompanionStar());
	FGameXXKPermanentCompanion AfterPromotion;
	TestTrue(TEXT("the promoted roster entry remains readable"), Subsystem->TryGetPermanentCompanionView(PromotionTargetId, AfterPromotion));
	TestEqual(TEXT("the UI action uses the canonical sigil rule rather than granting experience"), AfterPromotion.Star, BeforePromotion.Star + 1);
	TestEqual(TEXT("real promotion consumes the existing sigil"), Subsystem->GetPermanentCompanionSigilCount(), 0);
	TestEqual(TEXT("the profile exposes persistent experience without a free experience action"), Widget->GetSelectedCompanionProfile().Experience, AfterPromotion.Experience);
	TestTrue(TEXT("the profile exposes the canonical next-level experience threshold"), Widget->GetSelectedCompanionProfile().ExperienceRequiredForNextLevel > 0);
	return true;
}

#endif
