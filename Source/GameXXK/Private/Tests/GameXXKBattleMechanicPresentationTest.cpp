#include "Misc/AutomationTest.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "UI/GameXXKBattleMechanicPresentation.h"
#include "UI/GameXXKBattleUnitMechanicsWidget.h"
#include "UObject/Class.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	FGameXXKCardBattleRuntime Fixture(const TArray<FName>& Ids, const FName Owner)
	{
		FGameXXKCardBattleRuntime Runtime;Runtime.RoundNumber=3;Runtime.TeamMaxLevelSnapshot=100;
		FGameXXKCardCombatUnit Unit;Unit.UnitId=Owner;Unit.Side=EGameXXKCardTargetSide::Party;
		Unit.bLiving=true;Unit.HP=100;Unit.MaxHP=100;Unit.Mana=30;Unit.MaxMana=30;Runtime.Units.Add(Unit);
		for (int32 Index=0;Index<Ids.Num();++Index)
		{
			FGameXXKCardInstance Card;Card.CardId=Ids[Index];Card.OwnerUnitId=Owner;Card.AcquisitionOrdinal=Index;
			Card.InstanceId=*FString::Printf(TEXT("Mechanic.Card.%d"),Index);Card.SourceEntryId=Card.InstanceId;
			Card.CurrentQuality=FGameXXKCardQualityRules::GetCardBaseQuality(Card.CardId);Runtime.Deck.Hand.Add(Card);
		}
		return Runtime;
	}
	TArray<FName> PartnerCards()
	{
		TArray<FName> Result;
		for (const auto& D:FGameXXKCardCatalog::GetAllCardDefinitions())
			if (D.Owner==EGameXXKCardOwner::Profession && D.Role==EGameXXKCharacterRole::Sorcerer && Result.Num()<5) Result.Add(D.Id);
		return Result;
	}
	FGameXXKResolvedCardSnapshot Played(FName Card,FName Owner)
	{
		FGameXXKResolvedCardSnapshot Result;Result.CardId=Card;Result.OwnerUnitId=Owner;
		Result.Quality=FGameXXKCardQualityRules::GetCardBaseQuality(Card);return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMechanicFixedSlotsTest,"GameXXK.UI.Battle.Mechanics.FixedSlotsAndOwnerIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMechanicFixedSlotsTest::RunTest(const FString& Parameters)
{
	const FName Owner(TEXT("Mechanic.Mage"));const auto Ids=PartnerCards();
	if (!TestEqual(TEXT("five real mage cards are available"),Ids.Num(),5)) return false;
	auto Runtime=Fixture(Ids,Owner);
	FGameXXKSorcererPartnerTaskRuntime Task;Task.OwnerUnitId=Owner;Task.bActive=true;Task.LockedCardIds=Ids;
	Task.CompletedCardIds={Ids[3],Ids[1]};Task.FirstPlayOrder={Played(Ids[3],Owner),Played(Ids[1],Owner)};
	Task.LockedBranch=EGameXXKSorcererTaskBranch::Ice;Runtime.SorcererPartnerTasks.Add(Task);
	const auto Before=Runtime;
	const auto View=GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	TestEqual(TEXT("active task uses the locked ice branch"),View.Task.Element,EGameXXKMechanicElement::Ice);
	TestEqual(TEXT("five fixed positions remain"),View.Task.Cards.Num(),5);
	const int32 Expected[]={0,2,0,1,0};
	for (int32 Index=0;Index<5;++Index)
	{
		TestEqual(TEXT("slot keeps its carried identity"),View.Task.Cards[Index].CardId,Ids[Index]);
		TestEqual(TEXT("mark shows actual first-play order"),View.Task.Cards[Index].PlayOrder,Expected[Index]);
	}
	Runtime.Deck.DiscardPile.Add(Runtime.Deck.Hand[0]);Runtime.Deck.Hand.RemoveAt(0);
	const auto Discarded=GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	TestEqual(TEXT("discard does not complete a missing task card"),Discarded.Task.Cards[0].PlayOrder,0);
	TestTrue(TEXT("missing task card tooltip names its real zone"),Discarded.Task.Cards[0].Tooltip.Contains(TEXT("弃牌堆")));
	TestEqual(TEXT("other owner never receives this task"),GameXXKBattleMechanicPresentation::Build(Runtime,TEXT("OtherOwner")).Task.Cards.Num(),0);
	Runtime=Before;GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	TestTrue(TEXT("presentation cannot mutate the task runtime"),FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(&Before,&Runtime,0));
	Runtime.SorcererPartnerTasks[0].LockedBranch=EGameXXKSorcererTaskBranch::None;
	TestEqual(TEXT("unlocked universal starter has its own neutral icon"),GameXXKBattleMechanicPresentation::Build(Runtime,Owner).Task.Element,EGameXXKMechanicElement::Universal);
	Runtime.Units[0].bLiving=false;
	TestFalse(TEXT("a defeated owner has no active mechanic row"),GameXXKBattleMechanicPresentation::Build(Runtime,Owner).bReserveSpace);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMechanicTaskWidthsTest,"GameXXK.UI.Battle.Mechanics.HeroAndNpcTaskWidths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMechanicTaskWidthsTest::RunTest(const FString& Parameters)
{
	TArray<FName> Hero,Npc;
	for (const auto& D:FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (D.Owner==EGameXXKCardOwner::Hero && D.SpellTaskReward!=EGameXXKHeroSpellTaskReward::None) Hero.Add(D.Id);
		if (D.Owner==EGameXXKCardOwner::QuestNpc && D.NpcId==TEXT("Npc.YueBai") && !D.TaskNpcRewardEffects.IsEmpty() && Npc.Num()<3) Npc.Add(D.Id);
	}
	TestEqual(TEXT("hero mage loadout owns four task cards"),Hero.Num(),4);
	TestEqual(TEXT("NPC fixture carries three cards"),Npc.Num(),3);
	TestEqual(TEXT("hero HUD reserves four stable marks"),GameXXKBattleMechanicPresentation::Build(Fixture(Hero,TEXT("Player")),TEXT("Player")).Task.Cards.Num(),4);
	TestEqual(TEXT("NPC HUD reserves three stable marks"),GameXXKBattleMechanicPresentation::Build(Fixture(Npc,TEXT("Npc.YueBai")),TEXT("Npc.YueBai")).Task.Cards.Num(),3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMechanicFormulaIdentityTest,"GameXXK.UI.Battle.Mechanics.FormulaNumberQualityAndCooldown",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMechanicFormulaIdentityTest::RunTest(const FString& Parameters)
{
	using K=EGameXXKHealerFormulaKind;
	const K Kinds[]={K::AnyHealthChangeMedicine,K::FirstHealingMedicine,K::ThreeCleansedDotMedicine,K::ThreeEffectiveHealsDraw,K::BleedRemovedPartyArmor};
	TArray<FName> Ids;
	for (K Kind:Kinds) for (const auto& D:FGameXXKCardCatalog::GetAllCardDefinitions())
		if (D.Owner==EGameXXKCardOwner::Profession && D.Role==EGameXXKCharacterRole::Healer && D.HealerRule.FormulaKind==Kind) {Ids.Add(D.Id);break;}
	if (!TestEqual(TEXT("five real prescriptions are available"),Ids.Num(),5)) return false;
	const FName Owner(TEXT("Mechanic.Healer"));auto Runtime=Fixture(Ids,Owner);
	for (int32 Index:{4,1})
	{
		FGameXXKHealerFormulaRuntime Formula;Formula.OwnerUnitId=Owner;Formula.SourceCardId=Ids[Index];Formula.Kind=Kinds[Index];Formula.SourceQuality=EGameXXKCardQuality::Rare;Formula.LastTriggeredRound=3;Runtime.HealerFormulas.Add(Formula);
	}
	const FGameXXKHealerFormulaRuntime ReopenedFormula = Runtime.HealerFormulas[0];
	Runtime.HealerFormulas.Add(ReopenedFormula);
	for (auto& Card:Runtime.Deck.Hand) Card.CurrentQuality=EGameXXKCardQuality::Epic;
	const auto View=GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	TestEqual(TEXT("repeated opening does not create another badge"),View.Formulas.Num(),2);
	TestEqual(TEXT("formula number follows its carried position, not opening order"),View.Formulas[0].Number,2);
	TestEqual(TEXT("fifth carried prescription retains number five"),View.Formulas[1].Number,5);
	TestEqual(TEXT("opened quality is retained after later card upgrade"),View.Formulas[0].Quality,EGameXXKCardQuality::Rare);
	TestTrue(TEXT("once-per-round reaction dims only after it has triggered"),View.Formulas[0].bSpentThisRound);
	Runtime.RoundNumber=4;
	TestFalse(TEXT("the next round restores the same formula badge"),GameXXKBattleMechanicPresentation::Build(Runtime,Owner).Formulas[0].bSpentThisRound);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMechanicCompletionPresentationTest,"GameXXK.UI.Battle.Mechanics.CompletionSurvivesReplayPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMechanicCompletionPresentationTest::RunTest(const FString& Parameters)
{
	const FName Owner(TEXT("Mechanic.Mage"));const auto Ids=PartnerCards();if (Ids.Num()!=5) return false;
	auto Runtime=Fixture(Ids,Owner);Runtime.ActiveCardsPlayedThisRound=4;
	FGameXXKSorcererPartnerTaskRuntime Task;Task.bActive=true;Task.OwnerUnitId=Owner;Task.LockedCardIds=Ids;
	for (int32 Index=0;Index<4;++Index) {Task.CompletedCardIds.Add(Ids[Index]);Task.FirstPlayOrder.Add(Played(Ids[Index],Owner));}
	Runtime.SorcererPartnerTasks.Add(Task);
	const auto Before=GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	Runtime.SorcererPartnerTasks.Reset();Runtime.ActiveCardsPlayedThisRound=5;Runtime.LastActiveCard=Played(Ids[4],Owner);
	const auto After=GameXXKBattleMechanicPresentation::Build(Runtime,Owner);
	auto* Widget=NewObject<UGameXXKBattleUnitMechanicsWidget>();Widget->PrepareForBoardEmbedding();Widget->SetMechanicView(Before);Widget->SetMechanicView(After);
	TestEqual(TEXT("completed fifth card stays displayed after runtime reset"),Widget->GetTaskCardOrderForTest(4),5);
	Widget->AdvancePresentation(4.0f,true);
	TestEqual(TEXT("replay presentation keeps the completed record visible"),Widget->GetTaskCardOrderForTest(4),5);
	Widget->AdvancePresentation(1.2f,false);
	TestEqual(TEXT("after completion feedback the HUD reflects the reset task"),Widget->GetTaskCardOrderForTest(4),0);
	Widget->ResetPresentation();Widget->SetMechanicView(Before);
	TestEqual(TEXT("loading a task restores progress without a fake completion"),Widget->GetTaskCardOrderForTest(4),0);
	return true;
}
#endif
