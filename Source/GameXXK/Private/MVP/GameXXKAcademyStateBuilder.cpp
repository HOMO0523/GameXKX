#include "GameXXKAcademyStateBuilder.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKEquipmentEconomyRules.h"

bool GameXXKAcademyStateBuilder::BuildLoadout(const FGameXXKAcademyCourse& Course,int32 LessonIndex,FGameXXKRuntimeState& State,FName& Focus,FString& Error)
{
	if(!Course.Lessons.IsValidIndex(LessonIndex))return false;
	State=UGameXXKMVPRules::CreateNewGame();State.PlayerLevel=100;State.PlayerXP=0;State.PlayerGold=0;
	const auto Role=Course.Role==EGameXXKCharacterRole::Hero ? EGameXXKCharacterRole::Guard : Course.Role;
	const auto* Recruit=FGameXXKCompanionCatalog::GetRecruitTemplates().FindByPredicate([Role](const auto& T){return T.Role==Role && T.TemplateId.ToString().EndsWith(TEXT(".01"));});
	if(!Recruit){Error=TEXT("教学伙伴配置缺失。");return false;}
	FGameXXKCompanionRecruitResult Result;
	if(!FGameXXKCompanionRules::RecruitPermanentCompanion(State.CardRun.CompanionRoster,Recruit->TemplateId,20260908,Result,&Error))return false;
	if(!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State,&Error))return false;
	auto* Partner=State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([Role](const auto& C){return C.Role==Role;});
	if(!Partner)return false;
	Partner->Level=100;Partner->Experience=0;
	if(!FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*Partner,&Error))return false;
	TArray<FName> PartnerCards;
	if(Course.NpcId.IsNone() && Course.Role!=EGameXXKCharacterRole::Hero)PartnerCards=Course.Lessons[LessonIndex].Cards;
	else for(FName Id:Partner->UnlockedPersonalCardIds)if(PartnerCards.Num()<5)PartnerCards.Add(Id);
	if(!FGameXXKCompanionRules::SetSelectedPersonalCards(*Partner,PartnerCards,&Error))return false;
	const FName PartnerId=Partner->InstanceId;
	if(!FGameXXKCompanionRules::SetActivePermanentCompanion(State.CardRun.CompanionRoster,PartnerId,&Error))return false;
	const FName Npc=Course.NpcId.IsNone()?FName(TEXT("Npc.TusiChief")):Course.NpcId;
	State.CardRun.PartySelection.QuestNpcProgressions.FindOrAdd(Npc).Level=100;
	State.CardRun.OrderedFormation.Members.Reset();
	const FName Ids[]={FName(TEXT("Player")),PartnerId,Npc};
	const EGameXXKPartyMemberKind Kinds[]={EGameXXKPartyMemberKind::Hero,EGameXXKPartyMemberKind::PermanentCompanion,EGameXXKPartyMemberKind::QuestNpc};
	for(int32 I=0;I<3;++I){FGameXXKPartyMemberRef R;R.Kind=Kinds[I];R.MemberId=Ids[I];State.CardRun.OrderedFormation.Members.Add(R);}
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	TArray<FName> NpcCards;
	if(!Course.NpcId.IsNone())NpcCards=Course.Lessons[LessonIndex].Cards;
	if(!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State,Npc,NpcCards,&Error))return false;
	State.CardRun.HeroUnlockedCardIds=FGameXXKCardCatalog::GetHeroCardIdsUnlockedAtLevel(100);
	TArray<FName> HeroCards={TEXT("Hero.Generic.QingFengYiShi"),TEXT("Hero.Generic.HeYuZhan"),TEXT("Hero.Generic.SuiYanJi"),TEXT("Hero.Generic.PoYunYiShan"),TEXT("Hero.Generic.GuiYuanShu"),TEXT("Hero.Generic.NingShenTuNa"),TEXT("Hero.Generic.GuanXi"),TEXT("Hero.Generic.HengJianShouShi")};
	if(Course.Role==EGameXXKCharacterRole::Hero)HeroCards=Course.Lessons[LessonIndex].Cards;
	if(!FGameXXKCardBattleAdapter::SetHeroSelectedCards(State,HeroCards,&Error) || !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))return false;
	Focus=!Course.NpcId.IsNone()?Npc:Course.Role==EGameXXKCharacterRole::Hero?Ids[0]:PartnerId;
	State.PlayerHP=State.PlayerMaxHP;State.PlayerMP=State.PlayerMaxMP;
	return true;
}

bool GameXXKAcademyStateBuilder::ConfigureBattle(const FGameXXKAcademyCourse& Course,int32 LessonIndex,FGameXXKRuntimeState& State,FName Focus,FString& Error)
{
	auto& Battle=State.CardRun.ActiveBattle;
	const auto* FocusUnit=Battle.Units.FindByPredicate([Focus](const auto& U){return U.UnitId==Focus;});
	if(!FocusUnit){Error=TEXT("教学角色未能进入战斗。");return false;}
	const int32 EnemyHealth=FMath::Max(700,FocusUnit->Attack*12);
	for(auto& Unit:Battle.Units)
	{
		if(Unit.Side==EGameXXKCardTargetSide::Enemy){Unit.MaxHP=EnemyHealth;Unit.HP=EnemyHealth;Unit.Attack=35;Unit.Defense=0;Unit.CombatLevel=100;}
		else Unit.HP=FMath::Max(1,Unit.MaxHP*7/10);
	}
	TArray<FGameXXKCardInstance> Cards;auto& Deck=Battle.Deck;
	for(auto* Zone:{&Deck.Hand,&Deck.DrawPile,&Deck.DiscardPile,&Deck.ExhaustPile,&Deck.PendingAutomaticHandCards}){Cards.Append(*Zone);Zone->Reset();}
	for(FName Id:Course.Lessons[LessonIndex].Cards)
	{
		const int32 Index=Cards.IndexOfByPredicate([&](const auto& C){return C.CardId==Id && C.OwnerUnitId==Focus;});
		if(Index==INDEX_NONE){Error=TEXT("教学牌组不完整。");return false;}
		if(Deck.Hand.Num()<Deck.HandLimit){Deck.Hand.Add(Cards[Index]);Cards.RemoveAt(Index);}
	}
	while(Deck.Hand.Num()<Deck.HandLimit && !Cards.IsEmpty()){Deck.Hand.Add(Cards[0]);Cards.RemoveAt(0);}
	Deck.DrawPile=MoveTemp(Cards);
	return FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State,&Error) && FGameXXKCardBattleAdapter::RefreshEnemyIntentForecast(State,&Error);
}
