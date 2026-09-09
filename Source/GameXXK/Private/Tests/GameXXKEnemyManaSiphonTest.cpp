#include "Misc/AutomationTest.h"
#include "GameXXKCardRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEnemyText.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "GameXXKResistanceRules.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FName EnemyId(TEXT("SiphonEnemy"));
	FGameXXKCardCombatUnit* Find(FGameXXKRuntimeState& State,FName Id)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id;});
	}
	bool Prepare(FAutomationTestBase& Test,FName DefinitionId,EGameXXKEnemyDifficulty Difficulty,FName IntentId,FGameXXKRuntimeState& State,TArray<FName>& Party)
	{
		State=GameXXKPermanentPartyTestFixtures::MakeStartedState();State.PlayerLevel=100;
		const auto* Sorcerer=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([](const auto& C){return C.Role==EGameXXKCharacterRole::Sorcerer;});
		const auto* Healer=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([](const auto& C){return C.Role==EGameXXKCharacterRole::Healer;});
		if(!Sorcerer || !Healer){Test.AddError(TEXT("Siphon fixture requires the two named role templates"));return false;}
		Party={TEXT("Hero"),Sorcerer->InstanceId,Healer->InstanceId};
		State.CardRun.OrderedFormation.Members={{EGameXXKPartyMemberKind::Hero,Party[0]},{EGameXXKPartyMemberKind::PermanentCompanion,Party[1]},{EGameXXKPartyMemberKind::PermanentCompanion,Party[2]}};
		FGameXXKPartyFormationRules::ProjectCompatibility(State);
		const auto* Definition=FGameXXKEnemyCatalog::Find(DefinitionId);if(!Definition)return false;
		FGameXXKBattleRuntimeUnit Enemy;Enemy.Id=EnemyId;Enemy.EnemyDefinitionId=DefinitionId;Enemy.DisplayName=Definition->DisplayName;
		Enemy.HP=Enemy.MaxHP=10000;Enemy.Attack=100;Enemy.Defense=0;Enemy.Speed=8;Enemy.bEnemy=true;Enemy.BattleSlotNumber=1;Enemy.CombatLevel=100;
		State.ActiveBattleEnemies={Enemy};State.bHasActiveBattle=true;State.ActiveBattleNodeId=INDEX_NONE;State.Screen=EGameXXKScreen::Battle;
		FString Error;const int32 Percent=Difficulty==EGameXXKEnemyDifficulty::Hell?150:Difficulty==EGameXXKEnemyDifficulty::Hard?125:100;
		if(!FGameXXKCardBattleAdapter::BeginCardBattle(State,Definition->Tier==EGameXXKEnemyTier::Boss?EGameXXKNodeKind::Boss:EGameXXKNodeKind::Battle,EGameXXKCardTerrain::Plain,71831,&Error,Percent)){Test.AddError(Error);return false;}
		auto& Runtime=State.CardRun.ActiveBattle;
		for(auto& U:Runtime.Units)if(U.Side==EGameXXKCardTargetSide::Party){U.HP=U.MaxHP=10000;U.Defense=0;U.Armor=0;U.Attack=1;U.Mana=U.MaxMana=20;U.Statuses.Reset();}
		TArray<FGameXXKCardDamageResult> Dots;
		if(!GameXXKCardRules::EndPlayerCardPhase(Runtime,Dots,&Error)){Test.AddError(Error);return false;}
		Runtime.LockedEnemyIntents={{EnemyId,IntentId,1,Runtime.RoundNumber}};
		State.CardRun.EnemyIntents.Reset();State.CardRun.NextEnemyIntentIndex=0;return true;
	}
	bool ForecastAndResolve(FAutomationTestBase& Test,FGameXXKRuntimeState& State,FGameXXKCardEnemyIntent& Intent,TArray<FGameXXKCardDamageResult>& Results)
	{
		FString Error;
		if(!FGameXXKCardBattleAdapter::RefreshEnemyIntentForecast(State,&Error)){Test.AddError(Error);return false;}
		bool Finished=false;
		if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State,Intent,Results,Finished,&Error)){Test.AddError(Error);return false;}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHighestManaSiphonTest,"GameXXK.Battle.EnemyManaSiphon.HighestManaAndFixedEight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHighestManaSiphonTest::RunTest(const FString& Parameters)
{
	for(auto Difficulty:{EGameXXKEnemyDifficulty::Normal,EGameXXKEnemyDifficulty::Hard,EGameXXKEnemyDifficulty::Hell})
	{
		FGameXXKRuntimeState State;TArray<FName> Party;if(!Prepare(*this,TEXT("Enemy.Ch1.MoneyRat"),Difficulty,TEXT("GreedyMark"),State,Party))return false;
		Find(State,Party[0])->Mana=3;Find(State,Party[1])->Mana=12;Find(State,Party[2])->Mana=12;
		FString Error;if(!FGameXXKCardBattleAdapter::RefreshEnemyIntentForecast(State,&Error)){AddError(Error);return false;}
		if(!TestEqual(TEXT("one enemy has one predicted intent"),State.CardRun.EnemyIntents.Num(),1))return false;
		const auto& Forecast=State.CardRun.EnemyIntents[0];const auto* Drain=Forecast.Effects.FindByPredicate([](const auto& E){return E.Type==EGameXXKEnemyIntentEffectType::DrainMana;});
		if(!TestNotNull(TEXT("forecast contains the resource effect"),Drain))return false;
		TestEqual(TEXT("all difficulties use the approved eight"),Drain->Magnitude,8);
		if(!TestEqual(TEXT("only one highest-mana target"),Drain->TargetUnitIds.Num(),1))return false;
		TestEqual(TEXT("highest mana ties follow stable party order"),Drain->TargetUnitIds[0],Party[1]);
		TestEqual(TEXT("forecast is read-only"),Find(State,Party[1])->Mana,12);
		TestTrue(TEXT("tooltip describes mana siphon"),FGameXXKEnemyText::FormatIntentTooltip(State,Forecast).Contains(TEXT("吸取8点内力")));
		FGameXXKCardEnemyIntent Resolved;TArray<FGameXXKCardDamageResult> Results;bool Finished=false;
		if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State,Resolved,Results,Finished,&Error)){AddError(Error);return false;}
		TestEqual(TEXT("actual target loses eight mana"),Find(State,Party[1])->Mana,4);
		TestEqual(TEXT("the other tied character is untouched"),Find(State,Party[2])->Mana,12);
		TestEqual(TEXT("the receipt records eight, not a health packet"),State.CardRun.ActiveBattle.LastManaDrainedByTarget.FindRef(Party[1]),8);
		TestTrue(TEXT("a pure siphon has no fake damage events"),Results.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKManaSiphonHitTest,"GameXXK.Battle.EnemyManaSiphon.HitRedirectDodgeAndInsufficientMana",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKManaSiphonHitTest::RunTest(const FString& Parameters)
{
	for(int32 Scenario=0;Scenario<3;++Scenario)
	{
		FGameXXKRuntimeState State;TArray<FName> Party;if(!Prepare(*this,TEXT("Enemy.Ch3.GiantToad"),EGameXXKEnemyDifficulty::Hell,TEXT("Tongue"),State,Party))return false;
		GameXXKCardRules::AddCombatStatus(*Find(State,Party[0]),EGameXXKCardStatus::Mark,1);
		FGameXXKCardGuardLinkRuntime Guard;Guard.GuardianUnitId=Party[2];Guard.ProtectedUnitId=Party[0];Guard.Stacks=1;Guard.RedirectPolicy=EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
		State.CardRun.ActiveBattle.GuardLinks={Guard};
		if(Scenario==1)GameXXKCardRules::AddCombatStatus(*Find(State,Party[2]),EGameXXKCardStatus::Agility,2);
		if(Scenario==2)Find(State,Party[2])->Mana=3;
		const int32 Before=Find(State,Party[2])->Mana;
		FGameXXKCardEnemyIntent Intent;TArray<FGameXXKCardDamageResult> Results;if(!ForecastAndResolve(*this,State,Intent,Results))return false;
		const int32 Taken=Scenario==1?0:Scenario==2?3:6;
		TestEqual(TEXT("siphon follows the actual guard receiver and clamps to available mana"),Find(State,Party[2])->Mana,Before-Taken);
		TestEqual(TEXT("the originally protected target keeps its mana"),Find(State,Party[0])->Mana,20);
		int32 Attached=0;for(const auto& Hit:Results)Attached+=Hit.ManaDrained;
		TestEqual(TEXT("one follow-up is attached to the real hit, dodge has none"),Attached,Taken);
		TestEqual(TEXT("receipt agrees with actual loss"),State.CardRun.ActiveBattle.LastManaDrainedByTarget.FindRef(Party[2]),Taken);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKManaSiphonSaveTest,"GameXXK.Battle.EnemyManaSiphon.SaveResumeAndCardLegality",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKManaSiphonSaveTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;TArray<FName> Party;if(!Prepare(*this,TEXT("Enemy.Ch1.MoneyRat"),EGameXXKEnemyDifficulty::Normal,TEXT("GreedyMark"),State,Party))return false;
	Find(State,Party[0])->Mana=1;Find(State,Party[1])->Mana=3;Find(State,Party[2])->Mana=1;
	FString Error;if(!FGameXXKCardBattleAdapter::RefreshEnemyIntentForecast(State,&Error)){AddError(Error);return false;}
	TArray<uint8> Bytes;FMemoryWriter Writer(Bytes,true);FObjectAndNameAsStringProxyArchive WriteArchive(Writer,false);WriteArchive.ArIsSaveGame=true;
	FGameXXKCardRunState::StaticStruct()->SerializeItem(WriteArchive,&State.CardRun,nullptr);
	FGameXXKRuntimeState Resumed=State;FMemoryReader Reader(Bytes,true);FObjectAndNameAsStringProxyArchive ReadArchive(Reader,false);ReadArchive.ArIsSaveGame=true;
	FGameXXKCardRunState::StaticStruct()->SerializeItem(ReadArchive,&Resumed.CardRun,nullptr);
	FGameXXKCardEnemyIntent Intent;TArray<FGameXXKCardDamageResult> Results;bool Finished=false;
	if(!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(Resumed,Intent,Results,Finished,&Error)){AddError(Error);return false;}
	TestEqual(TEXT("saved intent drains only the remaining three"),Find(Resumed,Party[1])->Mana,0);
	TestEqual(TEXT("source profile survives the save"),Find(Resumed,EnemyId)->InnateResistanceBasisPoints.FindRef(EGameXXKCardDamageElement::Fire),2000);
	auto Check=Resumed.CardRun.ActiveBattle;Check.Phase=EGameXXKCardBattlePhase::Player;Check.Deck.SharedEnergy=20;
	const int32 Index=Check.Deck.DiscardPile.IndexOfByPredicate([&Party](const auto& Card){const auto* D=FGameXXKCardCatalog::FindCardDefinition(Card.CardId);return Card.OwnerUnitId==Party[1] && D && D->ManaCost>0;});
	if(!TestTrue(TEXT("the victim has a carried mana-cost card"),Index!=INDEX_NONE))return false;
	const auto Card=Check.Deck.DiscardPile[Index];Check.Deck.DiscardPile.RemoveAt(Index);Check.Deck.Hand.Add(Card);
	FGameXXKCardPlayPreview Preview;
	TestTrue(TEXT("existing legality check evaluates the depleted mana"),GameXXKCardRules::BuildCardPlayPreview(Check,Card.InstanceId,Preview,&Error));
	TestFalse(TEXT("a mana-cost card cannot be played after its source was drained to zero"),Preview.bCanPlay);
	TestTrue(TEXT("the failure is specifically mana rather than a different rule"),Preview.FailureReason.Contains(TEXT("内力")) || Preview.FailureReason.Contains(TEXT("Mana")));
	return true;
}
#endif
