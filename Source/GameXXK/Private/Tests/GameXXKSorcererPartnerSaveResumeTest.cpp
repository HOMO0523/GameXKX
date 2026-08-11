#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerSaveResumeTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;

	TArray<uint8> SerializeRuntime(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		Writer.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(
			Writer,
			const_cast<FGameXXKCardBattleRuntime*>(&Runtime),
			nullptr);
		Writer.Close();
		return Bytes;
	}

	bool RoundTripStable(
		FAutomationTestBase& Test,
		const FGameXXKCardBattleRuntime& Source,
		FGameXXKCardBattleRuntime& OutLoaded,
		const TCHAR* Label)
	{
		const TArray<uint8> Before = SerializeRuntime(Source);
		if (!Test.TestTrue(FString::Printf(TEXT("%s produces save bytes"), Label), !Before.IsEmpty()))
		{
			return false;
		}
		FMemoryReader Reader(Before, true);
		Reader.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Reader, &OutLoaded, nullptr);
		Reader.Close();
		FString Error;
		if (!Test.TestTrue(
			FString::Printf(TEXT("%s validates after load: %s"), Label, *Error),
			GameXXKCardRules::ValidateCardBattleRuntime(OutLoaded, &Error)))
		{
			return false;
		}
		Test.TestEqual(
			FString::Printf(TEXT("%s is byte-stable after load/save normalization"), Label),
			SerializeRuntime(OutLoaded),
			Before);
		return true;
	}

	bool InstallZones(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FGameXXKCardInstance>& Hand,
		const TArray<FGameXXKCardInstance>& Draw,
		const TArray<FGameXXKCardInstance>& Discard,
		const TArray<FGameXXKCardInstance>& PendingAutomaticHand = {})
	{
		Runtime.Deck.Hand = Hand;
		Runtime.Deck.DrawPile = Draw;
		Runtime.Deck.DiscardPile = Discard;
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.PendingAutomaticHandCards = PendingAutomaticHand;
		FString Error;
		const bool bValid = GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error);
		Test.TestTrue(FString::Printf(TEXT("installed save/resume zones validate: %s"), *Error), bValid);
		return bValid;
	}

	const FGameXXKSorcererPartnerTaskRuntime* FindTask(const FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.SorcererPartnerTasks.FindByPredicate([](const FGameXXKSorcererPartnerTaskRuntime& Task)
		{
			return Task.OwnerUnitId == SorcererId;
		});
	}

	bool CompareResults(
		FAutomationTestBase& Test,
		const TArray<FGameXXKCardPlayResult>& Left,
		const TArray<FGameXXKCardPlayResult>& Right,
		const TCHAR* Label)
	{
		if (!Test.TestEqual(FString::Printf(TEXT("%s result count"), Label), Left.Num(), Right.Num()))
		{
			return false;
		}
		bool bSame = true;
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			const bool bEntrySame = FGameXXKCardPlayResult::StaticStruct()->CompareScriptStruct(
				&Left[Index],
				&Right[Index],
				PPF_None);
			Test.TestTrue(FString::Printf(TEXT("%s result %d is identical"), Label, Index), bEntrySame);
			bSame &= bEntrySame;
		}
		return bSame;
	}

	bool ResolveBoth(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Left,
		FGameXXKCardBattleRuntime& Right,
		const FName InstanceId,
		const TCHAR* Label,
		TArray<FGameXXKCardPlayResult>& OutLeft,
		TArray<FGameXXKCardPlayResult>& OutRight)
	{
		FGameXXKCardPlayResult LeftResult;
		FGameXXKCardPlayResult RightResult;
		if (!ResolveActive(Test, Left, InstanceId, LeftResult, Label)
			|| !ResolveActive(Test, Right, InstanceId, RightResult, Label))
		{
			return false;
		}
		OutLeft.Add(MoveTemp(LeftResult));
		OutRight.Add(MoveTemp(RightResult));
		return true;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		const int32 Position,
		const EGameXXKSorcererCardFamily PreviousFamily,
		const EGameXXKSorcererTaskBranch Branch,
		const int32 PaidMana)
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = SorcererId;
		Snapshot.PaidManaCost = PaidMana;
		Snapshot.SorcererSequencePosition = Position;
		Snapshot.PreviousSorcererFamily = PreviousFamily;
		Snapshot.SorcererTaskBranch = Branch;
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (Definition)
		{
			switch (Definition->TargetSpec.Mode)
			{
			case EGameXXKCardTargetMode::AllEnemies:
				Snapshot.OriginalTargetUnitIds = {EnemyAId, EnemyBId};
				break;
			case EGameXXKCardTargetMode::Self:
				Snapshot.OriginalTargetUnitIds = {SorcererId};
				break;
			case EGameXXKCardTargetMode::AllAllies:
				Snapshot.OriginalTargetUnitIds = {SorcererId, AllyId};
				break;
			default:
				break;
			}
		}
		return Snapshot;
	}

	TArray<FGameXXKCardCombatUnit> StandardUnits()
	{
		return {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 2),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererMidTaskSaveResumeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.SaveResume.TwoOfFiveContinuesIdentically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererMidTaskSaveResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerSaveResumeTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing"),
		TEXT("Profession.Sorcerer.NingYanChengRen")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, StandardUnits(), 59711, Runtime)
		|| !InstallAllCardsInHand(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult Ignored;
	if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, Ignored, TEXT("first pre-save card"))
		|| !ResolveActive(*this, Runtime, Cards[1].InstanceId, Ignored, TEXT("second pre-save card")))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("two-of-five task exists at save point"), Task))
	{
		return true;
	}
	TestEqual(TEXT("exactly two cards are complete at save point"), Task->CompletedCardIds.Num(), 2);
	TestEqual(TEXT("exactly two snapshots exist at save point"), Task->FirstPlayOrder.Num(), 2);

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTripStable(*this, Runtime, Loaded, TEXT("two-of-five task")))
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> OriginalResults;
	TArray<FGameXXKCardPlayResult> LoadedResults;
	for (int32 Index = 2; Index < Cards.Num(); ++Index)
	{
		if (!ResolveBoth(
			*this,
			Runtime,
			Loaded,
			Cards[Index].InstanceId,
			TEXT("post-load distinct card"),
			OriginalResults,
			LoadedResults))
		{
			return true;
		}
	}
	CompareResults(*this, OriginalResults, LoadedResults, TEXT("two-of-five continuation"));
	TestEqual(TEXT("two-of-five continuation reaches byte-identical final runtime"), SerializeRuntime(Runtime), SerializeRuntime(Loaded));
	TestEqual(TEXT("two-of-five continuation preserves terminal phase"), Runtime.Phase, Loaded.Phase);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererUniversalBranchSaveResumeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.SaveResume.UniversalBranchLocksAfterSecond",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererUniversalBranchSaveResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerSaveResumeTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, StandardUnits(), 59712, Runtime)
		|| !InstallAllCardsInHand(*this, Runtime, Cards))
	{
		return false;
	}
	FGameXXKCardPlayResult FirstResult;
	if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, FirstResult, TEXT("Universal starter")))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("Universal starter creates a task"), Task))
	{
		return true;
	}
	TestEqual(TEXT("Universal starter leaves branch undecided"), Task->LockedBranch, EGameXXKSorcererTaskBranch::None);

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTripStable(*this, Runtime, Loaded, TEXT("Universal undecided branch")))
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> OriginalResults;
	TArray<FGameXXKCardPlayResult> LoadedResults;
	if (!ResolveBoth(
		*this,
		Runtime,
		Loaded,
		Cards[1].InstanceId,
		TEXT("Fire second card"),
		OriginalResults,
		LoadedResults))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* OriginalTask = FindTask(Runtime);
	const FGameXXKSorcererPartnerTaskRuntime* LoadedTask = FindTask(Loaded);
	if (!TestNotNull(TEXT("original branch task remains"), OriginalTask)
		|| !TestNotNull(TEXT("loaded branch task remains"), LoadedTask))
	{
		return true;
	}
	TestEqual(TEXT("second Fire card locks original to Fire"), OriginalTask->LockedBranch, EGameXXKSorcererTaskBranch::Fire);
	TestEqual(TEXT("second Fire card locks loaded to Fire"), LoadedTask->LockedBranch, EGameXXKSorcererTaskBranch::Fire);
	FGameXXKCardBattleRuntime LoadedAfterBranch;
	RoundTripStable(*this, Loaded, LoadedAfterBranch, TEXT("Universal selected Fire branch"));
	TestEqual(TEXT("selected branch normalization is byte-identical"), SerializeRuntime(Runtime), SerializeRuntime(LoadedAfterBranch));

	for (int32 Index = 2; Index < Cards.Num(); ++Index)
	{
		if (!ResolveBoth(
			*this,
			Runtime,
			LoadedAfterBranch,
			Cards[Index].InstanceId,
			TEXT("Universal task continuation"),
			OriginalResults,
			LoadedResults))
		{
			return true;
		}
	}
	CompareResults(*this, OriginalResults, LoadedResults, TEXT("Universal branch continuation"));
	TestEqual(TEXT("Universal branch continuation reaches byte-identical final runtime"), SerializeRuntime(Runtime), SerializeRuntime(LoadedAfterBranch));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererReplaySearchPauseSaveResumeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.SaveResume.ReplaySearchChoiceResumesIdentically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererReplaySearchPauseSaveResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerSaveResumeTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.LingHuoFu"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, StandardUnits(), 59713, Runtime)
		|| !InstallZones(*this, Runtime, {Cards[0], Cards[1]}, {Cards[2], Cards[3], Cards[4]}, {}))
	{
		return false;
	}
	FGameXXKCardPlayResult StarterResult;
	if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, StarterResult, TEXT("Fire starter before replay")))
	{
		return true;
	}
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {
		MakeSnapshot(CardIds[1], 2, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, 2)};
	Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("replay search fixture validates: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error)))
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> InitialReplayResults;
	if (!TestTrue(FString::Printf(TEXT("automatic search replay reaches choice: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, InitialReplayResults, &Error)))
	{
		return true;
	}
	TestEqual(TEXT("search replay emits one paused result"), InitialReplayResults.Num(), 1);
	TestEqual(TEXT("search replay opens task choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	TestTrue(TEXT("automatic queue remains saved while choice is open"), Runtime.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("automatic queue cursor advances past the paused replay"), Runtime.AutomaticResolutionQueue.NextCardIndex, 1);
	if (!TestEqual(TEXT("three unfinished candidates are offered"), Runtime.Deck.PendingChoice.Candidates.Num(), 3))
	{
		return true;
	}
	const FName PickedInstanceId = Runtime.Deck.PendingChoice.Candidates[0].InstanceId;

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTripStable(*this, Runtime, Loaded, TEXT("replay search pending choice")))
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> OriginalResumed;
	TArray<FGameXXKCardPlayResult> LoadedResumed;
	TestTrue(FString::Printf(TEXT("original search choice resumes: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, PickedInstanceId, OriginalResumed, &Error));
	TestTrue(FString::Printf(TEXT("loaded search choice resumes: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Loaded, PickedInstanceId, LoadedResumed, &Error));
	CompareResults(*this, OriginalResumed, LoadedResumed, TEXT("replay search resumed tail"));
	TestEqual(TEXT("search choice is cleared after resume"), Loaded.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestFalse(TEXT("automatic queue is cleared after resume"), Loaded.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("replay-search continuation reaches byte-identical runtime"), SerializeRuntime(Runtime), SerializeRuntime(Loaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererOverflowQueueSaveResumeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.SaveResume.HandTwentyPlusTwoQueueContinuesIdentically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererOverflowQueueSaveResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerSaveResumeTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> SorcererCards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		SorcererCards.Add(MakeCard(CardIds[Index], Index));
	}
	TArray<FGameXXKCardInstance> Fillers;
	for (int32 Index = 0; Index < 18; ++Index)
	{
		Fillers.Add(MakeCard(TEXT("Route.General.PoJiaTuCi"), 100 + Index, AllyId));
	}
	TArray<FGameXXKCardInstance> AllCards = SorcererCards;
	AllCards.Append(Fillers);
	TArray<FGameXXKCardInstance> FullHand = {SorcererCards[0], SorcererCards[1]};
	FullHand.Append(Fillers);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, AllCards, StandardUnits(), 59714, Runtime)
		|| !InstallZones(*this, Runtime, FullHand, {SorcererCards[3], SorcererCards[4]}, {SorcererCards[2]}))
	{
		return false;
	}
	FGameXXKCardPlayResult StarterResult;
	if (!ResolveActive(*this, Runtime, SorcererCards[0].InstanceId, StarterResult, TEXT("full-hand Universal starter")))
	{
		return true;
	}
	TestEqual(TEXT("hand stays at hard capacity"), Runtime.Deck.Hand.Num(), 20);
	TestEqual(TEXT("exactly two automatic additions queue"), Runtime.Deck.PendingAutomaticHandCards.Num(), 2);

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTripStable(*this, Runtime, Loaded, TEXT("20-card hand plus two queued")))
	{
		return true;
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		for (FGameXXKCardBattleRuntime* Candidate : {&Runtime, &Loaded})
		{
			FString Error;
			TestTrue(FString::Printf(TEXT("freeing overflow slot %d succeeds: %s"), Index, *Error),
				GameXXKCardRules::MoveHandCardToDiscard(Candidate->Deck, Fillers[Index].InstanceId, &Error));
		}
	}
	TestTrue(TEXT("original overflow queue drains"), Runtime.Deck.PendingAutomaticHandCards.IsEmpty());
	TestTrue(TEXT("loaded overflow queue drains"), Loaded.Deck.PendingAutomaticHandCards.IsEmpty());
	TestEqual(TEXT("overflow continuation reaches byte-identical runtime"), SerializeRuntime(Runtime), SerializeRuntime(Loaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererRewardSearchFallbackSaveResumeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.SaveResume.RewardReplaySearchUsesCompletedTaskFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererRewardSearchFallbackSaveResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerSaveResumeTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.ChiYanFengJie"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.FenMaiFu"),
		TEXT("Profession.Sorcerer.LingHuoFu")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], Index));
	}
	const FGameXXKCardInstance FillerA = MakeCard(TEXT("Route.General.PoJiaTuCi"), 100, AllyId);
	const FGameXXKCardInstance FillerB = MakeCard(TEXT("Route.General.PoJiaTuCi"), 101, AllyId);
	TArray<FGameXXKCardInstance> AllCards = Cards;
	AllCards.Add(FillerA);
	AllCards.Add(FillerB);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, AllCards, StandardUnits(), 59715, Runtime)
		|| !InstallZones(*this, Runtime, {}, {FillerA, FillerB}, Cards))
	{
		return false;
	}

	FGameXXKSorcererPartnerTaskRuntime& Task = Runtime.SorcererPartnerTasks.AddDefaulted_GetRef();
	Task.bActive = true;
	Task.OwnerUnitId = SorcererId;
	Task.LockedCardIds = CardIds;
	Task.CompletedCardIds = CardIds;
	Task.StarterReward = EGameXXKSorcererRewardRule::UniversalSearch;
	Task.LockedBranch = EGameXXKSorcererTaskBranch::Normal;
	Task.FirstPlayOrder = {
		MakeSnapshot(CardIds[0], 1, EGameXXKSorcererCardFamily::None, EGameXXKSorcererTaskBranch::Normal, 2),
		MakeSnapshot(CardIds[1], 2, EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererTaskBranch::Normal, 0),
		MakeSnapshot(CardIds[2], 3, EGameXXKSorcererCardFamily::Core, EGameXXKSorcererTaskBranch::Normal, 1),
		MakeSnapshot(CardIds[3], 4, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Normal, 0),
		MakeSnapshot(CardIds[4], 5, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Normal, 2)};
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = Task.FirstPlayOrder;
	Runtime.AutomaticResolutionQueue.NextCardIndex = Task.FirstPlayOrder.Num();
	Runtime.AutomaticResolutionQueue.PendingSorcererReward = Task.StarterReward;
	Runtime.AutomaticResolutionQueue.RewardOwnerUnitId = SorcererId;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("completed reward-search fixture validates: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error)))
	{
		return true;
	}

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTripStable(*this, Runtime, Loaded, TEXT("pending Universal search reward")))
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> OriginalResults;
	TArray<FGameXXKCardPlayResult> LoadedResults;
	TestTrue(FString::Printf(TEXT("original Universal search reward resolves: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, OriginalResults, &Error));
	TestTrue(FString::Printf(TEXT("loaded Universal search reward resolves: %s"), *Error),
		GameXXKCardRules::ResumeAutomaticResolutionQueue(Loaded, LoadedResults, &Error));
	CompareResults(*this, OriginalResults, LoadedResults, TEXT("Universal search reward continuation"));
	TestEqual(TEXT("one aggregated reward result is emitted"), LoadedResults.Num(), 1);
	if (LoadedResults.Num() == 1)
	{
		TestEqual(TEXT("completed-task search replay uses attack plus fallback against both enemies"), LoadedResults[0].DamageResults.Num(), 4);
	}
	// At reward time all five CardIds are complete, so a search can have no legal unfinished candidate.
	// The serializable contract is therefore the specified fallback, not an impossible pending choice.
	TestEqual(TEXT("reward replay search opens no impossible choice"), Loaded.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestFalse(TEXT("reward replay queue completes"), Loaded.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("Universal search reward reaches byte-identical runtime"), SerializeRuntime(Runtime), SerializeRuntime(Loaded));
	return true;
}

#endif
