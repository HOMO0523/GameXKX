#include "UI/GameXXKTalentTotals.h"
#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"

bool GameXXKTalentTotals::Build(const FGameXXKTalentProgress& Progress,TArray<FGameXXKTalentTotalGroup>& OutGroups,FString* Error)
{
	OutGroups.Reset();
	FGameXXKTalentProjection P;
	if(!FGameXXKTalentRules::BuildProjection(Progress,P,Error))return false;
	// Capacity compatibility floors protect old inventories but are not learned bonuses.
	int32 Backpack=0,Warehouse=0;
	for(const auto& Node:FGameXXKTalentCatalog::GetDefinitions())
	{
		const int32 Value=Progress.NodeRanks.FindRef(Node.Id)*Node.EffectPerRank;
		if(Node.Effect==EGameXXKTalentEffect::BackpackSlots)Backpack+=Value;
		if(Node.Effect==EGameXXKTalentEffect::UnlockWarehousePage)Warehouse+=Value;
	}
	FGameXXKTalentTotalGroup Group;
	const auto Begin=[&](const TCHAR* Title)
	{
		if(!Group.Entries.IsEmpty())OutGroups.Add(MoveTemp(Group));
		Group=FGameXXKTalentTotalGroup();Group.Title=FText::FromString(Title);
	};
	const auto Add=[&](const TCHAR* Id,const TCHAR* Label,int32 Amount,const TCHAR* Unit=TEXT(""))
	{
		if(Amount<=0)return;
		Group.Entries.Add({FName(Id),FText::FromString(Label),FText::FromString(FString::Printf(TEXT("+%d%s"),Amount,Unit)),Amount});
	};
	const auto Unlock=[&](const TCHAR* Id,const TCHAR* Label,bool Enabled)
	{
		if(Enabled)Group.Entries.Add({FName(Id),FText::FromString(Label),FText::FromString(TEXT("已开启")),1});
	};
	Begin(TEXT("基础战力"));
	Add(TEXT("FlatAttack"),TEXT("攻击"),P.FlatAttack);
	Add(TEXT("FlatMaxHP"),TEXT("生命"),P.FlatMaxHP);
	Add(TEXT("FlatDefense"),TEXT("防御"),P.FlatDefense);
	Begin(TEXT("游历战力"));
	Add(TEXT("RouteAttackPercent"),TEXT("攻击增幅"),P.RouteAttackPercent,TEXT("%"));
	Add(TEXT("RouteFinalDamagePercent"),TEXT("最终伤害"),P.RouteFinalDamagePercent,TEXT("%"));
	Add(TEXT("RouteDefensePercent"),TEXT("防御增幅"),P.RouteDefensePercent,TEXT("%"));
	Add(TEXT("RouteMaxHPPercent"),TEXT("生命增幅"),P.RouteMaxHPPercent,TEXT("%"));
	Add(TEXT("CriticalChancePercent"),TEXT("暴击率"),P.CriticalChancePercent,TEXT("%"));
	Add(TEXT("CriticalDamagePercent"),TEXT("暴击伤害"),P.CriticalDamagePercent,TEXT("%"));
	Begin(TEXT("行囊容量"));
	Add(TEXT("BackpackSlots"),TEXT("背包容量"),FMath::Clamp(Backpack,0,180),TEXT("格"));
	Add(TEXT("WarehousePages"),TEXT("仓库页数"),FMath::Clamp(Warehouse,0,5),TEXT("页"));
	Begin(TEXT("游历收益"));
	if(P.TravelMovementRank>0)Group.Entries.Add({TEXT("TravelMovement"),FText::FromString(TEXT("行进耗时")),FText::FromString(FString::Printf(TEXT("-%.1f秒"),5.f-P.GetTravelWalkSeconds())),P.TravelMovementRank});
	Add(TEXT("OnlineGoldPercent"),TEXT("挂机金币"),P.OnlineGoldPercent,TEXT("%"));
	Add(TEXT("OnlineExperiencePercent"),TEXT("挂机经验"),P.OnlineExperiencePercent,TEXT("%"));
	Begin(TEXT("离线收益"));
	Unlock(TEXT("OfflineUnlocked"),TEXT("离线挂机"),P.bOfflineRewardsUnlocked);
	Add(TEXT("OfflineGoldPercent"),TEXT("离线金币"),P.OfflineGoldPercent,TEXT("%"));
	Add(TEXT("OfflineExperiencePercent"),TEXT("离线经验"),P.OfflineExperiencePercent,TEXT("%"));
	Add(TEXT("OfflineGoldTimePercent"),TEXT("金币时长"),P.OfflineGoldTimePercent,TEXT("%"));
	Add(TEXT("OfflineExperienceTimePercent"),TEXT("经验时长"),P.OfflineExperienceTimePercent,TEXT("%"));
	Begin(TEXT("宝箱收获"));
	Add(TEXT("NormalChestDropPercent"),TEXT("普通掉落"),P.NormalChestDropPercent,TEXT("%"));
	Add(TEXT("AdvancedChestDropPercent"),TEXT("高级掉落"),P.AdvancedChestDropPercent,TEXT("%"));
	Add(TEXT("OfflineChestMinutes"),TEXT("离线时长"),P.OfflineChestMinutes,TEXT("分"));
	Begin(TEXT("百工收益"));
	Unlock(TEXT("ToolsUnlocked"),TEXT("工具收益分支"),Progress.NodeRanks.FindRef(TEXT("Talent.Entry.Tools"))>0);
	Add(TEXT("ToolExperiencePercent"),TEXT("经验加成"),P.ToolExperiencePercent,TEXT("%"));
	Add(TEXT("ToolGoldPercent"),TEXT("金币加成"),P.ToolGoldPercent,TEXT("%"));
	if(!Group.Entries.IsEmpty())OutGroups.Add(MoveTemp(Group));
	return true;
}
