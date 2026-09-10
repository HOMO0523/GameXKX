#include "GameXXKRelicSynergyRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKCombatScalingRules.h"
#include "GameXXKRelicCatalog.h"

namespace
{
    using S = EGameXXKCardStatus;
    using T = EGameXXKRelicTrigger;
    FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Battle, FName Id)
    { return Battle.Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id && U.bLiving;}); }
    int32 Percent(int32 Value,int32 Rate)
    { return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Value)*Rate/100,0,MAX_int32)); }
    bool Waiting(const FGameXXKCardBattleRuntime& B)
    {
        return B.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard
            || B.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
            || B.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand
            || B.AutomaticResolutionQueue.bActive;
    }
    void AppendEvidence(FGameXXKCardPlayResult& A,const FGameXXKCardPlayResult& B)
    {
        A.DamageResults.Append(B.DamageResults);A.HealingResults.Append(B.HealingResults);
        A.ArmorResults.Append(B.ArmorResults);A.StatusChanges.Append(B.StatusChanges);
        A.ToxicExplosionDistinctDotTypeCounts.Append(B.ToxicExplosionDistinctDotTypeCounts);
        A.HeavyArrowChargeConsumed += B.HeavyArrowChargeConsumed;
    }
    bool Flush(FGameXXKRuntimeState& State,FGameXXKCardPlayResult& Output,FString* Error)
    {
        auto& Pending=State.CardRun.PendingRelicAction;
        if(!Pending.bPending || Waiting(State.CardRun.ActiveBattle))return true;
        const FGameXXKRelicActionEvidence Evidence=Pending;
        Pending=FGameXXKRelicActionEvidence();
        for(auto& I:State.CardRun.Relics)
        {
            const auto* D=FGameXXKRelicCatalog::FindDefinition(I.RelicId);
            if(D && D->EffectKind==EGameXXKRelicEffectKind::Synergy && D->Trigger==T::CardPlayed
                && !GameXXKRelicSynergyRules::Apply(State,*D,I,T::CardPlayed,&Evidence,&Evidence.Primary.DamageResults,&Output,Error))return false;
        }
        GameXXKCardRules::RefreshCombatTerminalPhase(State.CardRun.ActiveBattle);
        return true;
    }
}

bool GameXXKRelicSynergyRules::BeginAction(FGameXXKRuntimeState& State,const FGameXXKCardBattleRuntime& Before,
    const FGameXXKCardPlayResult& Primary,FGameXXKCardPlayResult& Output,FString* Error)
{
    if(State.CardRun.PendingRelicAction.bPending)
    { if(Error)*Error=TEXT("An earlier relic action is still awaiting its card choice.");return false; }
    if(Primary.ResolutionOrigin!=EGameXXKCardResolutionOrigin::ActivePlay)return true;
    auto& E=State.CardRun.PendingRelicAction;
    E.bPending=true;E.BeforeUnits=Before.Units;E.BeforeTerrain=Before.Terrain;
    E.AfterTerrain=State.CardRun.ActiveBattle.Terrain;E.PreviousOwner=Before.LastActiveCard.OwnerUnitId;
    E.CountBefore=Before.ActiveCardsPlayedThisRound;E.CountAfter=State.CardRun.ActiveBattle.ActiveCardsPlayedThisRound;
    E.Primary=Primary;
    return Flush(State,Output,Error);
}

bool GameXXKRelicSynergyRules::ResumeAction(FGameXXKRuntimeState& State,TArray<FGameXXKCardPlayResult>& Results,FString* Error)
{
    auto& E=State.CardRun.PendingRelicAction;
    if(!E.bPending)return true;
    for(const auto& Result:Results)AppendEvidence(E.Primary,Result);
    E.AfterTerrain=State.CardRun.ActiveBattle.Terrain;
    if(Waiting(State.CardRun.ActiveBattle))return true;
    FGameXXKCardPlayResult Extra;
    Extra.OwnerUnitId=E.Primary.OwnerUnitId;Extra.CardInstanceId=E.Primary.CardInstanceId;
    Extra.CardId=E.Primary.CardId;Extra.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;
    if(!Flush(State,Extra,Error))return false;
    // A separate receipt keeps the resumed card's original evidence intact.
    if(!Extra.DamageResults.IsEmpty()||!Extra.ArmorResults.IsEmpty()||!Extra.HealingResults.IsEmpty()||!Extra.StatusChanges.IsEmpty())
        Results.Add(MoveTemp(Extra));
    return true;
}

bool GameXXKRelicSynergyRules::Apply(FGameXXKRuntimeState& State,const FGameXXKRelicDefinition& D,
    FGameXXKRelicInstance& I,T Trigger,const FGameXXKRelicActionEvidence* Evidence,
    const TArray<FGameXXKCardDamageResult>* PrimaryDamage,FGameXXKCardPlayResult* Output,FString* Error)
{
    auto& B=State.CardRun.ActiveBattle;
    if(I.SynergyRound!=B.RoundNumber)
    { I.SynergyRound=B.RoundNumber;I.SynergyUses=0;I.LastSynergyActiveCardOrdinal=INDEX_NONE;I.SynergyOwners.Reset(); }
    if(Trigger==T::CardPlayed)
    {
        if(!Evidence || Evidence->Primary.ResolutionOrigin!=EGameXXKCardResolutionOrigin::ActivePlay)return true;
        if(I.LastSynergyActiveCardOrdinal==Evidence->CountAfter)return true;
        I.LastSynergyActiveCardOrdinal=Evidence->CountAfter;
    }
    const FGameXXKCardPlayResult* Primary=Evidence?&Evidence->Primary:nullptr;
    const FName OwnerId=Primary?Primary->OwnerUnitId:NAME_None;
    const TArray<FGameXXKCardCombatUnit>& Snapshot=Evidence?Evidence->BeforeUnits:B.Units;
    TArray<FName> Party,Enemies;
    int32 HighestAttack=0;
    FName StrongestId=NAME_None;
    int32 BestOrder=MAX_int32;
    for(const auto& U:Snapshot)
    {
        if(!U.bLiving)continue;
        if(U.Side==EGameXXKCardTargetSide::Party)
        {
            if(!Unit(B,U.UnitId))continue;
            Party.Add(U.UnitId);
            if(StrongestId.IsNone()||U.Attack>HighestAttack||(U.Attack==HighestAttack&&U.StableSortOrder<BestOrder))
            {HighestAttack=U.Attack;StrongestId=U.UnitId;BestOrder=U.StableSortOrder;}
        }
    }
    for(const auto& U:B.Units)if(U.bLiving&&U.Side==EGameXXKCardTargetSide::Enemy)Enemies.Add(U.UnitId);
    const auto* BeforeOwner=Snapshot.FindByPredicate([OwnerId](const auto& U){return U.UnitId==OwnerId;});
    if(Party.IsEmpty())return true;
    const FName SourceId=Unit(B,OwnerId)?OwnerId:StrongestId;
    auto Status=[&](FName Id,S Kind,int32 Amount)->bool
    {
        auto* U=Unit(B,Id);if(!U||Amount<=0)return true;
        if(Kind==S::Block||Kind==S::Counter)return GameXXKCardRules::GrantRelicReaction(B,D.Id,Id,Kind,Amount,Error);
        if(Kind==S::Poison||Kind==S::Burn||Kind==S::Bleed)
        {
            const int32 Cap=FGameXXKCombatScalingRules::ResolveDotCap(B.TeamMaxLevelSnapshot);
            Amount=FMath::Min(Amount,FMath::Max(0,Cap-GameXXKCardRules::GetCombatStatusStacks(*U,Kind)));
        }
        const int32 Added=GameXXKCardRules::AddCombatStatus(*U,Kind,Amount,SourceId);
        if(Added>0&&U->Side==EGameXXKCardTargetSide::Enemy)
            return GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(B,*U,Error);
        return true;
    };
    auto PartyStatus=[&](S Kind,int32 Amount)->bool
    {for(FName Id:Party)if(!Status(Id,Kind,Amount))return false;return true;};
    auto Mana=[&](FName Id,int32 Amount){if(auto* U=Unit(B,Id))U->Mana=static_cast<int32>(FMath::Min<int64>(U->MaxMana,static_cast<int64>(U->Mana)+Amount));};
    auto PartyMana=[&](int32 Amount){for(FName Id:Party)Mana(Id,Amount);};
    auto Armor=[&](FName Id,int32 Amount)
    {
        if(auto* U=Unit(B,Id))
        {
            const int32 Actual=GameXXKCardRules::AddCombatArmor(*U,Amount);
            if(Output){auto& R=Output->ArmorResults.AddDefaulted_GetRef();R.SourceUnitId=SourceId;R.TargetUnitId=Id;R.RequestedArmor=Amount;R.EffectiveArmor=Actual;}
        }
    };
    auto Heal=[&](FName Id,int32 Amount)
    {
        if(auto* U=Unit(B,Id))
        {
            const int32 Actual=GameXXKCardRules::HealCombatUnit(*U,Amount);
            if(Output){auto& R=Output->HealingResults.AddDefaulted_GetRef();R.SourceUnitId=SourceId;R.TargetUnitId=Id;R.RequestedHealing=Amount;R.EffectiveHealing=Actual;}
        }
    };
    auto PartyArmor=[&](int32 Rate){for(FName Id:Party)if(const auto* U=Unit(B,Id))Armor(Id,Percent(U->Defense,Rate));};
    auto Energy=[&](int32 Amount){B.Deck.SharedEnergy=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(B.Deck.SharedEnergy)+Amount));};
    bool bSucceeded=true;
    auto Draw=[&](int32 Count){if(!GameXXKCardRules::DrawCards(B.Deck,Count,0,Error))bSucceeded=false;};
    auto Damage=[&](FName Id,int32 Amount)->bool
    {
        if(!Unit(B,Id)||Amount<=0||SourceId.IsNone())return true;
        FGameXXKCardDamageContext Context;Context.SourceUnitId=SourceId;
        Context.Kind=EGameXXKCardDamageKind::FixedDamage;Context.ResolutionOrigin=EGameXXKCardResolutionOrigin::ActivePlay;
        FGameXXKCardDamageResult Packet;
        if(!GameXXKCardRules::ApplyPlayerCardDirectDamage(B,Context,Id,Amount,Packet,Error))return false;
        Packet.Cause=EGameXXKCardDamageCause::Relic;
        if(Output)Output->DamageResults.Add(MoveTemp(Packet));
        return true;
    };
    const auto EffectiveHeal=[&](){return Primary&&Primary->HealingResults.ContainsByPredicate([](const auto& R){return R.EffectiveHealing>0;});};
    const auto Hit=[&](bool Bleeding)->FName
    {
        if(!PrimaryDamage)return NAME_None;
        for(const auto& R:*PrimaryDamage)
            if(!R.bAvoidedByAgility && (R.Kind==EGameXXKCardDamageKind::SingleTargetAttack||R.Kind==EGameXXKCardDamageKind::GroupAttack)
                && R.Cause!=EGameXXKCardDamageCause::Relic
                && (Bleeding?R.BleedStacksBeforeHit:R.MarkStacksBeforeHit)>0
                && R.HealthDamage+R.ArmorAbsorbed>0)
                if(const auto* U=B.Units.FindByPredicate([&](const auto& Candidate){return Candidate.UnitId==R.ResolvedTargetUnitId;});
                    U&&U->Side==EGameXXKCardTargetSide::Enemy)return U->UnitId;
        return NAME_None;
    };
    const FName K=D.SynergyKey;const int32 A=D.Magnitude,C=D.SecondaryMagnitude;
    const auto Is=[&](const TCHAR* Key){return K==FName(Key);};
    const bool First=Evidence&&Evidence->CountBefore==0;
    const bool Third=Evidence&&Evidence->CountBefore<3&&Evidence->CountAfter>=3;
    const bool TerrainChanged=Evidence&&Evidence->BeforeTerrain!=Evidence->AfterTerrain;
    FName PreviousOwner=NAME_None;
    if(Is(TEXT("alternate_owner_charge"))&&Primary)
    {
        for(FName Id:I.SynergyOwners){PreviousOwner=Id;break;}
        I.SynergyOwners.Reset();I.SynergyOwners.Add(OwnerId);
    }
    bool Used=false;
    if(Is(TEXT("party_status"))&&I.SynergyUses==0)
    {if(!PartyStatus(S::Momentum,A)||!PartyStatus(S::Charge,C))return false;Used=true;}
    else if(Is(TEXT("poison_all_attack_percent"))&&I.SynergyUses==0)
    {for(FName Id:Enemies)if(!Status(Id,S::Poison,Percent(HighestAttack,A)))return false;Used=true;}
    else if(Is(TEXT("party_armor_defense_percent"))&&I.SynergyUses==0){PartyArmor(A);Used=true;}
    else if(Is(TEXT("strongest_armor_block_momentum"))&&I.SynergyUses==0)
    {
        FName Best;int32 ArmorValue=-1,Order=MAX_int32;
        for(const auto& U:Snapshot)if(U.bLiving&&U.Side==EGameXXKCardTargetSide::Party&&Unit(B,U.UnitId)
            &&(U.Armor>ArmorValue||(U.Armor==ArmorValue&&U.StableSortOrder<Order)))
        {Best=U.UnitId;ArmorValue=U.Armor;Order=U.StableSortOrder;}
        if(!Best.IsNone()){if(!Status(Best,S::Block,A)||!Status(Best,S::Momentum,C))return false;Used=true;}
    }
    else if(Is(TEXT("enemy_mark_party_charge"))&&I.SynergyUses==0)
    {for(FName Id:Enemies)if(!Status(Id,S::Mark,A))return false;if(!PartyStatus(S::Charge,C))return false;Used=true;}
    else if(Is(TEXT("low_health_party_heal"))&&I.SynergyUses==0)
    {for(const auto& U:Snapshot)if(U.Side==EGameXXKCardTargetSide::Party&&U.bLiving&&static_cast<int64>(U.HP)*100<=static_cast<int64>(U.MaxHP)*C)Heal(U.UnitId,Percent(U.MaxHP,A));Used=true;}
    else if(Is(TEXT("party_armor_counter"))&&I.SynergyUses==0)
    {PartyArmor(A);if(!PartyStatus(S::Counter,C))return false;Used=true;}
    else if(Is(TEXT("trigger_bleed_burn"))&&I.SynergyUses==0)
    {
        for(FName Id:Enemies)for(S Kind:{S::Bleed,S::Burn})
        {
            if(!Unit(B,Id))break;
            TArray<FGameXXKCardDamageResult> Packets;
            if(!GameXXKCardRules::TriggerCombatDamageOverTime(B,SourceId,Id,Kind,1,Packets,Error))return false;
            for(auto& Packet:Packets)Packet.Cause=EGameXXKCardDamageCause::Relic;
            if(Output)Output->DamageResults.Append(MoveTemp(Packets));
        }
        Used=true;
    }
    else if(Is(TEXT("first_hit_defense_armor"))&&I.SynergyUses==0&&PrimaryDamage)
    {
        for(const auto& R:*PrimaryDamage)if(R.HealthDamage>0)
            if(const auto* U=Unit(B,R.ResolvedTargetUnitId);U&&U->Side==EGameXXKCardTargetSide::Party)
            {Armor(U->UnitId,Percent(U->Defense,A));Used=true;break;}
    }
    else if(!Primary)return true;
    else if(Is(TEXT("armor_restore_owner_mana"))&&I.SynergyUses<C&&Primary->ArmorResults.ContainsByPredicate([](const auto& R){return R.EffectiveArmor>0;}))
    {Mana(OwnerId,A);Used=true;}
    else if(Is(TEXT("terrain_change_mana_draw"))&&TerrainChanged&&I.SynergyUses==0){PartyMana(A);Draw(C);Used=true;}
    else if(Is(TEXT("effective_healing_to_armor"))&&EffectiveHeal()&&I.SynergyUses<C)
    {for(const auto& R:Primary->HealingResults)if(R.EffectiveHealing>0)Armor(R.TargetUnitId,Percent(R.EffectiveHealing,A));Used=true;}
    else if(Is(TEXT("effective_healing_party_medicine"))&&EffectiveHeal()&&I.SynergyUses<C)
    {if(!PartyStatus(S::Medicine,A))return false;Used=true;}
    else if(Is(TEXT("third_card_energy_mana"))&&Third&&I.SynergyUses==0){Energy(A);PartyMana(C);Used=true;}
    else if(Is(TEXT("first_owner_charge_armor"))&&First&&I.SynergyUses==0)
    {if(!Status(OwnerId,S::Charge,A))return false;if(BeforeOwner)Armor(OwnerId,Percent(BeforeOwner->Defense,C));Used=true;}
    else if(Is(TEXT("bleeding_hit_bonus"))&&I.SynergyUses<C)
    {const FName Target=Hit(true);if(!Target.IsNone()&&BeforeOwner){if(!Damage(Target,Percent(BeforeOwner->Attack,A)))return false;Used=true;}}
    else if(Is(TEXT("first_three_burn_all"))&&I.SynergyUses<C)
    {for(FName Id:Enemies)if(!Status(Id,S::Burn,Percent(HighestAttack,A)))return false;Used=true;}
    else if(Is(TEXT("alternate_owner_charge"))&&I.SynergyUses<C&&!PreviousOwner.IsNone()&&PreviousOwner!=OwnerId)
    {if(!Status(OwnerId,S::Charge,A))return false;Used=true;}
    else if(Is(TEXT("first_marked_hit_energy_momentum"))&&I.SynergyUses==0&&!Hit(false).IsNone())
    {Energy(A);if(!Status(OwnerId,S::Momentum,C))return false;Used=true;}
    else if(Is(TEXT("toxic_explosion_mana_draw"))&&I.SynergyUses<C&&Primary->ToxicExplosionDistinctDotTypeCounts.ContainsByPredicate([](int32 N){return N>0;}))
    {Mana(OwnerId,A);Draw(1);Used=true;}
    else if(Is(TEXT("heavy_shot_refund"))&&I.SynergyUses==0&&Primary->HeavyArrowChargeConsumed>0)
    {if(!Status(OwnerId,S::Charge,A))return false;Energy(C);Used=true;}
    else if(Is(TEXT("first_card_draw"))&&First&&I.SynergyUses==0){Draw(A);Used=true;}
    else if(Is(TEXT("third_card_energy_draw"))&&Third&&I.SynergyUses==0){Energy(A);Draw(C);Used=true;}
    else if(Is(TEXT("first_per_owner_draw_mana"))&&!I.SynergyOwners.Contains(OwnerId))
    {I.SynergyOwners.Add(OwnerId);Draw(A);Mana(OwnerId,C);Used=true;}
    else if(Is(TEXT("three_owners_block_mana"))&&I.SynergyUses==0)
    {I.SynergyOwners.Add(OwnerId);if(I.SynergyOwners.Num()>=3){if(!PartyStatus(S::Block,A))return false;PartyMana(C);Used=true;}}
    else if(Is(TEXT("fifth_card_party_attack_aoe"))&&I.SynergyUses==0&&Evidence->CountBefore<C&&Evidence->CountAfter>=C)
    {for(FName Id:Enemies)if(!Damage(Id,Percent(HighestAttack,A)))return false;Used=true;}
    else if(Is(TEXT("terrain_change_energy_party_armor"))&&TerrainChanged&&I.SynergyUses==0){Energy(A);PartyArmor(C);Used=true;}
    else if(Is(TEXT("first_heal_party_percent_medicine"))&&EffectiveHeal()&&I.SynergyUses==0)
    {for(FName Id:Party)if(const auto* U=Unit(B,Id))Heal(Id,Percent(U->MaxHP,A));if(!PartyStatus(S::Medicine,C))return false;Used=true;}
    else if(Is(TEXT("spend_last_mana_refill_draw"))&&Primary->bActiveSpentLastMana&&Primary->ActiveManaSpent>0&&I.SynergyUses==0)
    {if(auto* U=Unit(B,OwnerId))Mana(OwnerId,U->MaxMana);Draw(A);Used=true;}
    else if(Is(TEXT("marked_hit_party_attack_bonus"))&&I.SynergyUses<C)
    {const FName Target=Hit(false);if(!Target.IsNone()){if(!Damage(Target,Percent(HighestAttack,A)))return false;Used=true;}}
    if(Used)++I.SynergyUses;
    return bSucceeded;
}
