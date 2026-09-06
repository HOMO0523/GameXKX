#include "GameXXKDevFixtures.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "MVP/GameXXKSaveMigration.h"

namespace GameXXKDevFixtures
{
bool SetLevel(FGameXXKRuntimeState& S,FName Character,int32 Level,FString& Error)
{
 if (Character==TEXT("Player"))
 {
  S.PlayerLevel=Level; S.PlayerXP=0; S.CardRun.HeroUnlockedCardIds=FGameXXKCardCatalog::GetHeroCardIdsUnlockedAtLevel(Level);
  S.CardRun.HeroSelectedCardIds.RemoveAll([&S](FName Id){return !S.CardRun.HeroUnlockedCardIds.Contains(Id);});
  for(FName Id:S.CardRun.HeroUnlockedCardIds) if(S.CardRun.HeroSelectedCardIds.Num()<8) S.CardRun.HeroSelectedCardIds.AddUnique(Id);
 }
 else if(FGameXXKCompanionCatalog::FindQuestNpcDefinition(Character))
 { auto& P=S.CardRun.PartySelection.QuestNpcProgressions.FindOrAdd(Character); P.Level=Level; P.Experience=0; }
 else
 {
  auto* C=S.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([Character](const auto& V){return V.InstanceId==Character;});
  if(!C){Error=TEXT("角色不存在。");return false;} C->Level=Level; C->Experience=0;
  if(!FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*C,&Error))return false;
  C->SelectedCardIds.RemoveAll([C](FName Id){return !C->UnlockedPersonalCardIds.Contains(Id);});
  for(FName Id:C->UnlockedPersonalCardIds)if(C->SelectedCardIds.Num()<5)C->SelectedCardIds.AddUnique(Id);
 }
 return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(S,&Error);
}
EGameXXKEquipmentSet RecommendedSet(EGameXXKCharacterRole Role)
{
 switch(Role)
 {
 case EGameXXKCharacterRole::Guard:return EGameXXKEquipmentSet::XuanJia;
 case EGameXXKCharacterRole::Healer:return EGameXXKEquipmentSet::QingNang;
 case EGameXXKCharacterRole::Hunter:case EGameXXKCharacterRole::Sorcerer:return EGameXXKEquipmentSet::ZhuiFeng;
 case EGameXXKCharacterRole::FormationMaster:return EGameXXKEquipmentSet::ShanHe;
 default:return EGameXXKEquipmentSet::PoJun;
 }
}
EGameXXKEquipmentSet NpcRecommendedSet(FName Npc)
{
 // Primary linked professions in the authored NPC card-pool design.
 if(Npc==TEXT("Npc.JinGui"))return EGameXXKEquipmentSet::XuanJia;
 if(Npc==TEXT("Npc.QiongMeiEr")||Npc==TEXT("Npc.SongJinBao"))return EGameXXKEquipmentSet::ZhuiFeng;
 if(Npc==TEXT("Npc.ZhouGuangZu"))return EGameXXKEquipmentSet::QingNang;
 if(Npc==TEXT("Npc.YueBai"))return EGameXXKEquipmentSet::ShanHe;
 return EGameXXKEquipmentSet::PoJun;
}
static bool ConfigureSet(FGameXXKRuntimeState& S,FName Owner,EGameXXKEquipmentSet Set,int32 Level,TArray<FName>& Created,TArray<FName>& Items,FString& Error)
{
 int32 GemIndex=0;
 for(int32 I=1;I<=6;++I)
 {
  const auto Slot=static_cast<EGameXXKEquipmentSlot>(I);
  auto* Item=S.EquipmentCollection.EquipmentInstances.FindByPredicate([&](const auto& E)
  {
   const auto* D=FGameXXKEquipmentCatalog::FindDefinition(E.BaseEquipmentId);
   return D&&D->Set==Set&&D->Slot==Slot&&E.Quality==EGameXXKEquipmentQuality::Treasure
    &&(Owner.IsNone()?E.OwnerKind==EGameXXKEquipmentOwnerKind::Warehouse:E.OwnerCharacterId==Owner);
  });
  FName Id;
  if(Item)Id=Item->InstanceId;
  else
  {
   FGameXXKEquipmentCreateRequest R;R.Set=Set;R.Quality=EGameXXKEquipmentQuality::Treasure;R.ItemLevel=Level;R.bForceSlot=true;R.ForcedSlot=Slot;
   if(!FGameXXKEquipmentRules::CreateRolledInstance(S.EquipmentCollection,R,Id,&Error))return false;
   Created.Add(Id);Item=S.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& E){return E.InstanceId==Id;});
  }
  check(Item);Item->ItemLevel=Level;Item->EnhancementLevel=10;
  for(auto& A:Item->RolledAffixes){const auto Range=FGameXXKAffixCatalog::GetMagnitudeRange(A.Unit,A.Tier);A.Magnitude=Range.Minimum+(Range.Maximum-Range.Minimum)/2;}
  for(auto& G:Item->SocketedGems){G.Type=static_cast<EGameXXKGemType>(1+GemIndex++%3);G.Quality=EGameXXKGemQuality::Treasure;}
  if(!Owner.IsNone()&&Item->OwnerCharacterId!=Owner)
  {FGameXXKEquipmentTransactionResult Result;if(!FGameXXKEquipmentEconomyRules::Equip(S,Owner,Slot,Id,Result)){Error=Result.Message.ToString();return false;}}
  Items.Add(Id);
 }
 return true;
}
bool RecommendAll(FGameXXKRuntimeState& State,int32 Level,EGameXXKEquipmentSet HeroSet,TArray<FName>& Created,FString& Error)
{
 if(State.CardRun.bLoadoutLockedForRoute){Error=TEXT("请先返回战前整备，再一键配装。");return false;}
 if(Level<1||Level>100||HeroSet<EGameXXKEquipmentSet::PoJun||HeroSet>EGameXXKEquipmentSet::ShanHe){Error=TEXT("等级或主角套装无效。");return false;}
 auto S=State; TArray<FName> NewIds;
 TArray<TPair<FName,EGameXXKEquipmentSet>> Owners;Owners.Add({TEXT("Player"),HeroSet});
 for(const auto& C:S.CardRun.CompanionRoster.PermanentCompanions)Owners.Add({C.InstanceId,RecommendedSet(C.Role)});
 for(const auto& N:FGameXXKCompanionCatalog::GetQuestNpcDefinitions())Owners.Add({N.NpcId,NpcRecommendedSet(N.NpcId)});
 for(const auto& O:Owners)
 {
  TArray<FName> Items;
  if(!SetLevel(S,O.Key,Level,Error)||!ConfigureSet(S,O.Key,O.Value,Level,NewIds,Items,Error))return false;
 }
 TArray<FName> Spare;
 if(!ConfigureSet(S,NAME_None,EGameXXKEquipmentSet::ShiGu,Level,NewIds,Spare,Error)||!FGameXXKDesktopInventoryRules::Normalize(S,&Error))return false;
 for(FName Id:Spare)
 {
  const auto Entry=FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id);
  if(FGameXXKDesktopInventoryRules::FindEntrySlot(S,EGameXXKDesktopItemContainer::Backpack,Entry)!=INDEX_NONE)continue;
  const int32 From=FGameXXKDesktopInventoryRules::FindEntrySlot(S,EGameXXKDesktopItemContainer::Warehouse,Entry);
  const int32 To=FGameXXKDesktopInventoryRules::FindFirstEmptySlot(S,EGameXXKDesktopItemContainer::Backpack);
  if(From==INDEX_NONE||To==INDEX_NONE||!FGameXXKDesktopInventoryRules::MoveEntry(S,EGameXXKDesktopItemContainer::Warehouse,From,EGameXXKDesktopItemContainer::Backpack,To,&Error))
  {if(Error.IsEmpty())Error=TEXT("背包空间不足，请为蚀骨套预留六格。");return false;}
 }
 if(!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(S)){Error=TEXT("配装属性同步失败。");return false;}
 S.PlayerHP=S.PlayerMaxHP;S.PlayerMP=S.PlayerMaxMP;
 if(!FGameXXKSaveMigration::ValidateRuntimeState(S,Error))return false;
 State=MoveTemp(S);Created=MoveTemp(NewIds);return true;
}
bool BuildBenchmark(EGameXXKCharacterRole Role,FName Npc,const FString& HeroDirection,int32 NpcOmit,FGameXXKRuntimeState& Out,FString& Error)
{
 FGameXXKRuntimeState S=UGameXXKMVPRules::CreateNewGame();
 for(const auto& Template:FGameXXKCompanionCatalog::GetRecruitTemplates())
 {
  if(!Template.TemplateId.ToString().EndsWith(TEXT(".01")))continue;
  FGameXXKCompanionRecruitResult R;
  if(!FGameXXKCompanionRules::RecruitPermanentCompanion(S.CardRun.CompanionRoster,Template.TemplateId,20260906+static_cast<int32>(Template.Role),R,&Error))return false;
 }
 if(!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(S,&Error))return false;
 TArray<FName> Created;
 if(!RecommendAll(S,100,EGameXXKEquipmentSet::PoJun,Created,Error))return false;
 auto* C=S.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([Role](const auto& V){return V.Role==Role;});
 if(!C){Error=TEXT("标准阵容缺少该职业伙伴。");return false;}
 // A declared legal baseline, not a deck optimizer: the first five unlocked cards.
 TArray<FName> PartnerCards;for(FName Id:C->UnlockedPersonalCardIds)if(PartnerCards.Num()<5)PartnerCards.Add(Id);
 if(!FGameXXKCompanionRules::SetSelectedPersonalCards(*C,PartnerCards,&Error))return false;
 const FName CompanionId=C->InstanceId;
 if(!FGameXXKCompanionRules::SetActivePermanentCompanion(S.CardRun.CompanionRoster,CompanionId,&Error))return false;
 S.CardRun.PartySelection.ActivePermanentCompanionInstanceId=CompanionId;
 for(auto& Member:S.CardRun.OrderedFormation.Members)if(Member.Kind==EGameXXKPartyMemberKind::PermanentCompanion)Member.MemberId=CompanionId;
 FGameXXKPartyFormationRules::ProjectCompatibility(S);
 const auto* N=FGameXXKCompanionCatalog::FindQuestNpcDefinition(Npc);
 if(!N||NpcOmit<0||NpcOmit>=N->FixedCardIds.Num()){Error=TEXT("NPC或省略卡序号无效。");return false;}
 auto NpcCards=N->FixedCardIds;NpcCards.RemoveAt(NpcOmit);
 if(!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(S,Npc,NpcCards,&Error))return false;
 TArray<FName> HeroCards;
 const FString Prefix=TEXT("Hero.")+HeroDirection+TEXT(".");
 for(FName Id:S.CardRun.HeroUnlockedCardIds)if(Id.ToString().StartsWith(Prefix))HeroCards.Add(Id);
 if(HeroCards.Num()!=4){Error=TEXT("主角方向需为Blade/Guard/Healer/Hunter/Mage/Formation。");return false;}
 for(const TCHAR* Id:{TEXT("Hero.Generic.QingFengYiShi"),TEXT("Hero.Generic.GuiYuanShu"),TEXT("Hero.Generic.NingShenTuNa"),TEXT("Hero.Generic.GuanXi")})HeroCards.Add(FName(Id));
 if(!FGameXXKCardBattleAdapter::SetHeroSelectedCards(S,HeroCards,&Error)||!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(S))return false;
 S.PlayerHP=S.PlayerMaxHP;S.PlayerMP=S.PlayerMaxMP;
 if(!FGameXXKSaveMigration::ValidateRuntimeState(S,Error))return false;
 Out=MoveTemp(S);return true;
}
}
