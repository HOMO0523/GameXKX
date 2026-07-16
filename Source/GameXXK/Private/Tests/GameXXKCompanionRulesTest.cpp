#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRulesTest,
	"GameXXK.Data.CompanionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRulesTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
	TestEqual(TEXT("the immutable recruitment catalog exposes twenty-four templates"), Templates.Num(), 24);
	const TArray<EGameXXKCharacterRole> PermanentRoles = {
		EGameXXKCharacterRole::Blade, EGameXXKCharacterRole::Guard, EGameXXKCharacterRole::Healer,
		EGameXXKCharacterRole::Hunter, EGameXXKCharacterRole::Sorcerer, EGameXXKCharacterRole::FormationMaster};
	for (const EGameXXKCharacterRole Role : PermanentRoles)
	{
		int32 TemplateCountForRole = 0;
		TArray<FName> RolePool;
		TestTrue(FString::Printf(TEXT("each permanent role can build its deterministic twelve-card pool (%d)"), static_cast<int32>(Role)), FGameXXKCompanionRules::BuildPersonalCardPool(Role, 7331, RolePool, nullptr));
		TestEqual(FString::Printf(TEXT("each permanent role pool has twelve cards (%d)"), static_cast<int32>(Role)), RolePool.Num(), 12);
		for (const FGameXXKCompanionTemplateDefinition& Template : Templates)
		{
			TemplateCountForRole += Template.Role == Role ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("each permanent role exposes exactly four recruit templates (%d)"), static_cast<int32>(Role)), TemplateCountForRole, 4);
	}

	TArray<FName> FirstPool;
	TArray<FName> SecondPool;
	FString FirstPoolError;
	FString SecondPoolError;
	TestTrue(TEXT("a blade companion can build its seeded personal card pool"), FGameXXKCompanionRules::BuildPersonalCardPool(EGameXXKCharacterRole::Blade, 7331, FirstPool, &FirstPoolError));
	TestTrue(TEXT("the same role and seed rebuild the same personal card pool"), FGameXXKCompanionRules::BuildPersonalCardPool(EGameXXKCharacterRole::Blade, 7331, SecondPool, &SecondPoolError));
	TestEqual(TEXT("a personal pool has the approved twelve cards"), FirstPool.Num(), 12);
	TestEqual(TEXT("same seed preserves pool order"), FirstPool, SecondPool);
	TSet<FName> UniquePoolCards(FirstPool);
	TestEqual(TEXT("a personal pool has no duplicate card ids"), UniquePoolCards.Num(), FirstPool.Num());
	int32 CoreCardCount = 0;
	for (const FName CardId : FirstPool)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		CoreCardCount += Definition && Definition->bCoreProfessionCard ? 1 : 0;
	}
	TestEqual(TEXT("a personal pool always contains exactly four profession core cards"), CoreCardCount, 4);

	FGameXXKCompanionRosterState Roster;
	FGameXXKCompanionRecruitResult RecruitResult;
	FString RecruitError;
	TestTrue(TEXT("a first-time template recruit becomes a permanent companion"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, TEXT("Companion.Blade.01"), 7331, RecruitResult, &RecruitError));
	TestEqual(TEXT("a first-time template recruit has the recruited outcome"), RecruitResult.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
	TestEqual(TEXT("a first-time template recruit occupies one permanent roster slot"), Roster.PermanentCompanions.Num(), 1);
	if (Roster.PermanentCompanions.Num() == 1)
	{
		const FGameXXKPermanentCompanion& Companion = Roster.PermanentCompanions[0];
		TestEqual(TEXT("a recruited companion begins at level one"), Companion.Level, 1);
		TestEqual(TEXT("a recruited companion begins at one star"), Companion.Star, 1);
		TestEqual(TEXT("a recruited companion persists all twelve generated personal cards"), Companion.PersonalCardIds.Num(), 12);
		TestEqual(TEXT("a recruited companion begins with six unlocked cards"), Companion.UnlockedPersonalCardIds.Num(), 6);
		TestEqual(TEXT("a recruited companion begins with a valid five-card route selection"), Companion.SelectedCardIds.Num(), 5);
		TestTrue(TEXT("the default five-card selection is valid against its unlocked personal pool"), FGameXXKCompanionRules::ValidateSelectedPersonalCards(Companion, Companion.SelectedCardIds, nullptr));
		TestTrue(TEXT("a freshly recruited companion passes complete immutable-profile validation"), FGameXXKCompanionRules::ValidatePermanentCompanionProfile(Companion, nullptr));
		FGameXXKPermanentCompanion TamperedCompanion = Companion;
		TamperedCompanion.PersonalCardIds[0] = TEXT("Hero.QingFengYiShi");
		TestFalse(TEXT("a tampered companion pool cannot enter a partner slot"), FGameXXKCompanionRules::ValidatePermanentCompanionProfile(TamperedCompanion, nullptr));
	}

	const TArray<FGameXXKQuestNpcDefinition>& QuestNpcs = FGameXXKCompanionCatalog::GetQuestNpcDefinitions();
	TestEqual(TEXT("the immutable task NPC catalog exposes the approved six task NPCs"), QuestNpcs.Num(), 6);
	const TArray<FName> ExpectedQuestNpcIds = {
		TEXT("Npc.TusiChief"), TEXT("Npc.SongJinBao"), TEXT("Npc.YueBai"),
		TEXT("Npc.ZhouGuangZu"), TEXT("Npc.JinGui"), TEXT("Npc.QiongMeiEr")};
	for (const FName ExpectedNpcId : ExpectedQuestNpcIds)
	{
		const FGameXXKQuestNpcDefinition* QuestNpc = FGameXXKCompanionCatalog::FindQuestNpcDefinition(ExpectedNpcId);
		TestNotNull(FString::Printf(TEXT("the task NPC catalog contains %s"), *ExpectedNpcId.ToString()), QuestNpc);
		if (QuestNpc)
		{
			TestEqual(FString::Printf(TEXT("each named task NPC has exactly four fixed cards (%s)"), *ExpectedNpcId.ToString()), QuestNpc->FixedCardIds.Num(), 4);
			TestFalse(FString::Printf(TEXT("each named task NPC has a passive (%s)"), *ExpectedNpcId.ToString()), QuestNpc->PassiveId.IsNone());
			for (const FName CardId : QuestNpc->FixedCardIds)
			{
				TestNotNull(FString::Printf(TEXT("each named task NPC card exists in the shared catalog (%s/%s)"), *ExpectedNpcId.ToString(), *CardId.ToString()), FGameXXKCardCatalog::FindCardDefinition(CardId));
			}
		}
	}
	const FGameXXKQuestNpcDefinition* TusiChief = FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("the task NPC catalog contains the Tusi chief"), TusiChief);
	if (TusiChief)
	{
		TestEqual(TEXT("each task NPC retains exactly four fixed cards"), TusiChief->FixedCardIds.Num(), 4);
		TestFalse(TEXT("each task NPC has an explicit passive key"), TusiChief->PassiveId.IsNone());
		if (TusiChief->FixedCardIds.Num() == 4)
		{
			TArray<FName> ValidNpcSelection;
			ValidNpcSelection.Append(TusiChief->FixedCardIds.GetData(), 3);
			TestTrue(TEXT("a task NPC accepts exactly three distinct cards from its fixed pool"), FGameXXKCompanionRules::ValidateQuestNpcCardSelection(TusiChief->NpcId, ValidNpcSelection, nullptr));
			ValidNpcSelection.Add(TusiChief->FixedCardIds[3]);
			TestFalse(TEXT("a task NPC rejects four selected cards"), FGameXXKCompanionRules::ValidateQuestNpcCardSelection(TusiChief->NpcId, ValidNpcSelection, nullptr));
		}
	}

	if (Roster.PermanentCompanions.Num() == 1)
	{
		FGameXXKPermanentCompanion& ProgressingCompanion = Roster.PermanentCompanions[0];
		TestTrue(TEXT("companion experience advances through the published level thresholds"), FGameXXKCompanionRules::AwardExperience(ProgressingCompanion, 180, nullptr));
		TestEqual(TEXT("one hundred eighty experience advances the companion to level four"), ProgressingCompanion.Level, 4);
		TestEqual(TEXT("level four unlocks the seventh personal card"), ProgressingCompanion.UnlockedPersonalCardIds.Num(), 7);
		int32 Sigils = 1;
		TestTrue(TEXT("one sigil promotes a one-star companion to two stars"), FGameXXKCompanionRules::PromoteCompanionStar(ProgressingCompanion, Sigils, nullptr));
		TestEqual(TEXT("promotion consumes the required one sigil"), Sigils, 0);
		TestEqual(TEXT("two stars unlock the eighth personal card"), ProgressingCompanion.UnlockedPersonalCardIds.Num(), 8);
		TArray<FName> LockedCardSelection = ProgressingCompanion.SelectedCardIds;
		LockedCardSelection[4] = ProgressingCompanion.PersonalCardIds[8];
		TestFalse(TEXT("a five-card selection cannot include a still-locked personal card"), FGameXXKCompanionRules::ValidateSelectedPersonalCards(ProgressingCompanion, LockedCardSelection, nullptr));
		TArray<FName> UpdatedPersonalSelection;
		UpdatedPersonalSelection.Append(ProgressingCompanion.UnlockedPersonalCardIds.GetData() + 1, 5);
		TestTrue(TEXT("the player can persist a different valid five-card personal selection"), FGameXXKCompanionRules::SetSelectedPersonalCards(ProgressingCompanion, UpdatedPersonalSelection, nullptr));
		TestEqual(TEXT("the persisted personal selection preserves player order"), ProgressingCompanion.SelectedCardIds, UpdatedPersonalSelection);

		FGameXXKCompanionAttributes BladeAttributes;
		TestTrue(TEXT("progressed permanent companion attributes use role growth and star scaling"), FGameXXKCompanionRules::GetCompanionAttributes(EGameXXKCharacterRole::Blade, 4, 2, FGameXXKCompanionAttributes(), BladeAttributes, nullptr));
		TestEqual(TEXT("blade level-four two-star health is floored after star scaling"), BladeAttributes.Health, 128);
		TestEqual(TEXT("blade level-four two-star attack is floored after star scaling"), BladeAttributes.Attack, 24);

		FGameXXKCompanionAttributes TusiAttributes;
		TestTrue(TEXT("task NPC attributes follow the hero level without permanent star progression"), FGameXXKCompanionRules::GetQuestNpcAttributes(TEXT("Npc.TusiChief"), 3, TusiAttributes, nullptr));
		TestEqual(TEXT("Tusi chief level-three attack follows its fractional growth"), TusiAttributes.Attack, 16);

		TestTrue(TEXT("exactly one permanent companion can be marked active"), FGameXXKCompanionRules::SetActivePermanentCompanion(Roster, ProgressingCompanion.InstanceId, nullptr));
		FGameXXKCompanionPartySelection PartySelection;
		PartySelection.ActivePermanentCompanionInstanceId = ProgressingCompanion.InstanceId;
		if (TusiChief && TusiChief->FixedCardIds.Num() == 4)
		{
			TArray<FName> TaskNpcSelection;
			TaskNpcSelection.Append(TusiChief->FixedCardIds.GetData(), 3);
			TestTrue(TEXT("the player can persist a valid temporary task-NPC three-card selection"), FGameXXKCompanionRules::SetQuestNpcCardSelection(PartySelection.QuestNpc, TEXT("Npc.TusiChief"), TaskNpcSelection, nullptr));
		TestTrue(TEXT("party validation permits only the fixed hero, one permanent partner, and one task NPC"), FGameXXKCompanionRules::ValidatePartySelection(Roster, PartySelection, nullptr));
		FGameXXKCompanionPartySelection MismatchedPartySelection = PartySelection;
		MismatchedPartySelection.ActivePermanentCompanionInstanceId = NAME_None;
		TestFalse(TEXT("route setup rejects a party selection that disagrees with the roster's active companion"), FGameXXKCompanionRules::ValidatePartySelection(Roster, MismatchedPartySelection, nullptr));
		}

		FGameXXKCompanionRecruitResult DuplicateResult;
		TestTrue(TEXT("recruiting an owned template becomes a sigil instead of another permanent companion"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, TEXT("Companion.Blade.01"), 9999, DuplicateResult, nullptr));
		TestEqual(TEXT("an owned template reports the duplicate-sigil outcome"), DuplicateResult.Outcome, EGameXXKCompanionRecruitOutcome::DuplicateSigil);
		TestEqual(TEXT("a duplicate recruit awards one sigil"), Roster.SigilCount, 1);
	}

	FGameXXKCompanionRosterState FullRoster;
	for (int32 TemplateIndex = 0; TemplateIndex < FGameXXKCompanionRules::MaxPermanentCompanions; ++TemplateIndex)
	{
		FGameXXKCompanionRecruitResult FillResult;
		TestTrue(FString::Printf(TEXT("unique template %d can fill a permanent roster slot"), TemplateIndex), FGameXXKCompanionRules::RecruitPermanentCompanion(FullRoster, Templates[TemplateIndex].TemplateId, TemplateIndex + 100, FillResult, nullptr));
	}
	TestEqual(TEXT("the permanent roster holds exactly twelve companions"), FullRoster.PermanentCompanions.Num(), FGameXXKCompanionRules::MaxPermanentCompanions);
	if (FullRoster.PermanentCompanions.Num() == FGameXXKCompanionRules::MaxPermanentCompanions)
	{
		FullRoster.PermanentCompanions[0].EquippedItemIds = {TEXT("Item.Companion.TestBlade")};
		TestTrue(TEXT("a full-roster companion can retain spent progression before replacement"), FGameXXKCompanionRules::AwardExperience(FullRoster.PermanentCompanions[0], 40, nullptr));
		TestTrue(TEXT("the planned dismissal can be explicitly marked as the active companion"), FGameXXKCompanionRules::SetActivePermanentCompanion(FullRoster, FullRoster.PermanentCompanions[0].InstanceId, nullptr));
	}

	FGameXXKCompanionRecruitOrder FullRosterOrder;
	bool bFoundUnownedOrder = false;
	for (int32 OrderSeed = 1; OrderSeed <= 256 && !bFoundUnownedOrder; ++OrderSeed)
	{
		FGameXXKCompanionRosterState OrderProbe = FullRoster;
		FGameXXKCompanionRecruitOrder ProbeOrder;
		if (!FGameXXKCompanionRules::CreateRecruitOrder(OrderProbe, OrderSeed, ProbeOrder, nullptr))
		{
			continue;
		}
		const bool bAlreadyOwned = FullRoster.PermanentCompanions.ContainsByPredicate([&ProbeOrder](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.RecruitTemplateId == ProbeOrder.ResolvedTemplateId;
		});
		if (!bAlreadyOwned)
		{
			FullRoster.PendingRecruitOrder = ProbeOrder;
			FullRosterOrder = ProbeOrder;
			bFoundUnownedOrder = true;
		}
	}
	TestTrue(TEXT("a full roster can persist a deterministic order for an unowned template"), bFoundUnownedOrder);
	FGameXXKCompanionRecruitResult PendingResult;
	TestTrue(TEXT("a saved recruit order resolves to a pending replacement rather than rerolling or discarding the result"), FGameXXKCompanionRules::ResolvePendingRecruitOrder(FullRoster, PendingResult, nullptr));
	TestEqual(TEXT("a full roster reports its pending-replacement recruit outcome"), PendingResult.Outcome, EGameXXKCompanionRecruitOutcome::PendingReplacement);
	TestTrue(TEXT("the full-roster candidate remains fixed in persistent pending state"), FullRoster.PendingRecruitment.bHasPendingRecruitment);
	FGameXXKCompanionRecruitResult ReopenedPendingResult;
	TestTrue(TEXT("reopening a saved full-roster recruit order returns the exact same fixed candidate"), FGameXXKCompanionRules::ResolvePendingRecruitOrder(FullRoster, ReopenedPendingResult, nullptr));
	TestEqual(TEXT("reopened full-roster order retains the same candidate instance id"), ReopenedPendingResult.Companion.InstanceId, PendingResult.Companion.InstanceId);
	TestEqual(TEXT("reopened full-roster order retains the same immutable card seed"), ReopenedPendingResult.Companion.CardSeed, FullRosterOrder.CardSeed);
	const FName DismissedInstanceId = FullRoster.PermanentCompanions[0].InstanceId;
	FGameXXKCompanionDismissalRefund Refund;
	TestTrue(TEXT("explicitly dismissing one companion resolves the fixed pending recruit and chooses a replacement partner"), FGameXXKCompanionRules::ResolvePendingRecruitment(FullRoster, DismissedInstanceId, PendingResult.Companion.InstanceId, Refund, nullptr));
	TestEqual(TEXT("replacing one companion retains the twelve-slot maximum"), FullRoster.PermanentCompanions.Num(), FGameXXKCompanionRules::MaxPermanentCompanions);
	TestFalse(TEXT("resolving replacement clears pending state"), FullRoster.PendingRecruitment.bHasPendingRecruitment);
	TestFalse(TEXT("resolving replacement clears the saved no-reroll recruit order"), FullRoster.PendingRecruitOrder.bHasPendingOrder);
	TestEqual(TEXT("dismissal returns all invested experience materials"), Refund.ReturnedExperienceMaterials, 40);
	TestEqual(TEXT("dismissal returns every equipped companion item"), Refund.ReturnedEquippedItemIds.Num(), 1);
	TestTrue(TEXT("the explicitly chosen replacement becomes the only active permanent partner"), FullRoster.PermanentCompanions.ContainsByPredicate([&PendingResult](const FGameXXKPermanentCompanion& Companion)
	{
		return Companion.InstanceId == PendingResult.Companion.InstanceId && Companion.bIsActive;
	}));

	FGameXXKCompanionAttributes HighLevelNpcAttributes;
	TestTrue(TEXT("task NPC attributes continue to follow a hero beyond the permanent-companion level cap"), FGameXXKCompanionRules::GetQuestNpcAttributes(TEXT("Npc.TusiChief"), 21, HighLevelNpcAttributes, nullptr));
	return true;
}

#endif
